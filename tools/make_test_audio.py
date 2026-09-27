"""Writes the test sounds the Mac plays into the recorder's microphone.

    ~/.platformio/penv/bin/python tools/make_test_audio.py <out_dir>

tone_1k.wav     1 kHz, 30 s: dropouts, timing, level
sweep.wav       200 Hz -> 3.5 kHz logarithmic, 20 s: frequency response
alias_5k.wav    5 kHz, 5 s: must be filtered out, not folded back to 3 kHz
bursts.wav      0.5 s of 1 kHz / 0.5 s of silence, 60 s: long-run timing

All at 44.1 kHz, 16-bit mono, -6 dBFS, with 20 ms fades so the edges do
not click.
"""
import math
import os
import struct
import sys
import wave

RATE = 44100
AMP = 0.5 * 32767


def write(path, samples):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-32767, min(32767, s)))) for s in samples))
    print("wrote " + path)


def envelope(i, n, fade=int(0.02 * RATE)):
    return min(1.0, i / fade, (n - 1 - i) / fade)


def tone(freq, seconds):
    n = int(seconds * RATE)
    return [AMP * envelope(i, n) * math.sin(2 * math.pi * freq * i / RATE) for i in range(n)]


def sweep(f0, f1, seconds):
    n = int(seconds * RATE)
    k = math.log(f1 / f0)
    out = []
    for i in range(n):
        t = i / RATE
        phase = 2 * math.pi * f0 * seconds / k * (math.exp(t / seconds * k) - 1)
        out.append(AMP * envelope(i, n) * math.sin(phase))
    return out


def bursts(seconds):
    out = []
    for _ in range(int(seconds)):
        out += tone(1000, 0.5) + [0.0] * int(0.5 * RATE)
    return out


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(out_dir, exist_ok=True)
    write(os.path.join(out_dir, "tone_1k.wav"), tone(1000, 30))
    write(os.path.join(out_dir, "sweep.wav"), sweep(200, 3500, 20))
    write(os.path.join(out_dir, "alias_5k.wav"), tone(5000, 5))
    write(os.path.join(out_dir, "bursts.wav"), bursts(60))


if __name__ == "__main__":
    main()
