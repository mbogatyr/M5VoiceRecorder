"""Checks a recording pulled off the recorder.

    PY=~/.platformio/penv/bin/python
    $PY tools/analyze_recording.py rec.wav                    # basics
    $PY tools/analyze_recording.py rec.wav --tone 1000        # dropouts in a tone
    $PY tools/analyze_recording.py rec.wav --sweep 200:3500:20
    $PY tools/analyze_recording.py rec.wav --afconvert        # cross-check decoder

Decodes the IMA ADPCM itself (the same algorithm as lib/ImaAdpcm and the
web page) and reports duration, level, noise floor, clipping and the
strongest frequency. With --tone it looks for dropouts: moments where the
tone's level dips, and sample-level jumps that a gap in the audio leaves
behind. With --sweep it prints the level per band. --afconvert decodes the
file with macOS as well and compares the samples, which also proves that
macOS can play the file.

Pure Python: the PlatformIO venv has no numpy.
"""
import argparse
import math
import os
import struct
import subprocess
import sys
import tempfile
import wave

STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66,
        73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408,
        449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
        2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630,
        9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
        32767]
INDEX = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]


def read_adpcm(path):
    data = open(path, "rb").read()
    if data[0:4] != b"RIFF" or data[8:12] != b"WAVE":
        sys.exit("Not a WAV file")
    pos, fmt, fact, body = 12, None, None, None
    while pos + 8 <= len(data):
        cid, size = data[pos:pos + 4], struct.unpack("<I", data[pos + 4:pos + 8])[0]
        chunk = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            fmt = struct.unpack("<HHIIHHHH", chunk[:20])
        elif cid == b"fact":
            fact = struct.unpack("<I", chunk[:4])[0]
        elif cid == b"data":
            body = chunk
            break
        pos += 8 + size + (size & 1)
    tag, channels, rate, byte_rate, align, bits, _, spb = fmt
    if tag != 0x11 or channels != 1:
        sys.exit("Expected mono IMA ADPCM, got tag %#x, %d channels" % (tag, channels))
    samples = []
    for b in range(len(body) // align):
        blk = body[b * align:(b + 1) * align]
        pred = struct.unpack("<h", blk[0:2])[0]
        idx = blk[2]
        samples.append(pred)
        for byte in blk[4:]:
            for code in (byte & 15, byte >> 4):
                step = STEP[idx]
                d = step >> 3
                if code & 4:
                    d += step
                if code & 2:
                    d += step >> 1
                if code & 1:
                    d += step >> 2
                pred = max(-32768, min(32767, pred - d if code & 8 else pred + d))
                idx = max(0, min(88, idx + INDEX[code]))
                samples.append(pred)
    return rate, samples, dict(align=align, spb=spb, fact=fact, data=len(body), bytes=len(data))


def db(x):
    return 20 * math.log10(x) if x > 0 else -120.0


def rms(xs):
    return math.sqrt(sum(v * v for v in xs) / len(xs)) if xs else 0.0


def goertzel(xs, freq, rate):
    w = 2 * math.pi * freq / rate
    c = 2 * math.cos(w)
    s1 = s2 = 0.0
    for v in xs:
        s1, s2 = v + c * s1 - s2, s1
    power = s1 * s1 + s2 * s2 - c * s1 * s2
    return math.sqrt(max(power, 0.0)) * 2 / len(xs)  # amplitude of that sine


def windows(samples, rate, ms):
    n = int(rate * ms / 1000)
    return [samples[i:i + n] for i in range(0, len(samples) - n + 1, n)], n


def basics(rate, samples, info):
    dur = len(samples) / rate
    print("duration      %.2f s  (%d samples, %d blocks, fact %s)"
          % (dur, len(samples), info["data"] // info["align"], info["fact"]))
    if info["fact"] is not None and info["fact"] != len(samples):
        print("  WARNING: fact chunk says %d samples" % info["fact"])
    peak = max((abs(v) for v in samples), default=0)
    clipped = sum(1 for v in samples if abs(v) >= 32000)
    print("peak          %.1f dBFS   clipped samples %d" % (db(peak / 32768), clipped))
    wins, _ = windows(samples, rate, 50)
    levels = sorted(db(rms(w) / 32768) for w in wins)
    if levels:
        floor = sum(levels[:max(1, len(levels) // 10)]) / max(1, len(levels) // 10)
        loud = sum(levels[-max(1, len(levels) // 10):]) / max(1, len(levels) // 10)
        print("noise floor   %.1f dBFS (quietest 10%% of 50 ms windows)" % floor)
        print("loudest       %.1f dBFS (loudest 10%%)" % loud)
        return floor
    return -120.0


def active_span(samples, rate, floor, margin_db=12):
    wins, n = windows(samples, rate, 50)
    on = [i for i, w in enumerate(wins) if db(rms(w) / 32768) > floor + margin_db]
    if not on:
        return None
    return on[0] * n, (on[-1] + 1) * n


def dominant(samples, rate, span):
    a, b = span
    mid = (a + b) // 2
    seg = samples[max(a, mid - rate // 4):min(b, mid + rate // 4)]
    best = max(range(50, rate // 2 - 40, 10), key=lambda f: goertzel(seg, f, rate))
    fine = max([best + d for d in range(-10, 11)], key=lambda f: goertzel(seg, f, rate))
    print("strongest     %d Hz (middle of the sound)" % fine)
    return fine


def tone_check(samples, rate, freq, span):
    a, b = span
    # Skip 100 ms at each edge: the fades of the test sound.
    a += rate // 10
    b -= rate // 10
    seg = samples[a:b]
    wins, n = windows(seg, rate, 20)
    amps = [goertzel(w, freq, rate) for w in wins]
    med = sorted(amps)[len(amps) // 2]
    # Only dips with the tone on both sides count; the ends are the fades.
    on = [i for i, v in enumerate(amps) if v >= med / 2]
    first, last = (on[0], on[-1]) if on else (0, -1)
    dips = [i for i in range(first, last + 1) if amps[i] < med / 2]  # > 6 dB down
    tone_power = sum(v * v / 2 for v in amps) / len(amps)
    total_power = sum(rms(w) ** 2 for w in wins) / len(wins)
    print("tone          %d Hz at %.1f dBFS, %.1f dB of the signal is the tone"
          % (freq, db(med / 32768), 10 * math.log10(tone_power / total_power) if total_power else 0))
    print("level dips    %d of %d 20 ms windows more than 6 dB below the median" % (len(dips), len(amps)))
    for i in dips[:10]:
        print("  dip at %.3f s: %.1f dB" % ((a + i * n) / rate, db(amps[i] / med)))
    # A pure tone obeys x[n] = 2cos(w) x[n-1] - x[n-2]; a gap breaks that.
    c = 2 * math.cos(2 * math.pi * freq / rate)
    resid = [abs(seg[i] - c * seg[i - 1] + seg[i - 2]) for i in range(2, len(seg))]
    typical = sorted(resid)[len(resid) // 2]
    spikes = [i for i, r in enumerate(resid) if r > max(8 * typical, med)]
    # A gap or a bad sample leaves one isolated jump. Sounds in the room
    # (speech, a knock) leave clusters of them; those are reported apart.
    near = rate // 50  # 20 ms
    isolated = [i for k, i in enumerate(spikes)
                if (k == 0 or i - spikes[k - 1] > near)
                and (k == len(spikes) - 1 or spikes[k + 1] - i > near)]
    clusters = 0
    for k, i in enumerate(spikes):
        if i not in isolated and (k == 0 or i - spikes[k - 1] > near):
            clusters += 1
    print("jumps         %d isolated (a gap or bad sample each), %d clusters (room sounds)"
          % (len(isolated), clusters))
    for i in isolated[:10]:
        pos = (a + i + 2) % 505
        print("  isolated jump at %.4f s, position %d in its ADPCM block" % ((a + i + 2) / rate, pos))
    return len(dips), len(isolated)


def sweep_check(samples, rate, spec, span):
    """Level of the sweep per band.

    The onset is only known to within a window or two, so instead of
    trusting the expected frequency, each 100 ms window takes the strongest
    frequency within 25 % of it.
    """
    f0, f1, seconds = (float(x) for x in spec.split(":"))
    a, _ = span
    k = math.log(f1 / f0)
    wins, n = windows(samples[a:a + int(seconds * rate)], rate, 100)
    rows = []
    for i, w in enumerate(wins):
        t = (i + 0.5) * n / rate
        expect = f0 * math.exp(t / seconds * k)
        grid = [expect * (0.75 + 0.5 * j / 20) for j in range(21)]
        f = max(grid, key=lambda x: goertzel(w, x, rate))
        rows.append((f, db(goertzel(w, f, rate) / 32768)))
    bands = [(200, 300), (300, 500), (500, 800), (800, 1300), (1300, 2000), (2000, 3000), (3000, 3500)]
    means = []
    for lo, hi in bands:
        vals = [v for f, v in rows if lo <= f < hi]
        means.append(sum(vals) / len(vals) if vals else None)
    ref = sorted(m for m in means if m is not None)[len([m for m in means if m is not None]) // 2]
    print("sweep level per band, dBFS (and relative to the median band):")
    for (lo, hi), m in zip(bands, means):
        if m is not None:
            print("  %5d-%-5d Hz  %6.1f dBFS  %+5.1f dB" % (lo, hi, m, m - ref))


def afconvert_check(path, samples):
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "pcm.wav")
        res = subprocess.run(["afconvert", "-f", "WAVE", "-d", "LEI16", path, out],
                             capture_output=True, text=True)
        if res.returncode != 0:
            print("afconvert     FAILED: " + (res.stderr.strip() or res.stdout.strip()))
            return
        with wave.open(out) as w:
            raw = w.readframes(w.getnframes())
        theirs = list(struct.unpack("<%dh" % (len(raw) // 2), raw))
    n = min(len(theirs), len(samples))
    diff = max((abs(theirs[i] - samples[i]) for i in range(n)), default=0)
    print("afconvert     macOS decodes %d samples (ours %d), max difference %d"
          % (len(theirs), len(samples), diff))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    ap.add_argument("--tone", type=float)
    ap.add_argument("--sweep")
    ap.add_argument("--afconvert", action="store_true")
    args = ap.parse_args()

    rate, samples, info = read_adpcm(args.path)
    print("file          %s, %d bytes, %d Hz" % (args.path, info["bytes"], rate))
    floor = basics(rate, samples, info)
    span = active_span(samples, rate, floor)
    if span is None:
        print("sound         nothing clearly above the noise floor")
    else:
        print("sound         %.2f s .. %.2f s" % (span[0] / rate, span[1] / rate))
        dominant(samples, rate, span)
        if args.tone:
            tone_check(samples, rate, args.tone, span)
        if args.sweep:
            sweep_check(samples, rate, args.sweep, span)
    if args.afconvert:
        afconvert_check(args.path, samples)


if __name__ == "__main__":
    main()
