"""Drives the recorder over USB Serial for self-testing, without Wi-Fi.

The firmware takes line commands: r (= KEY1), w (= KEY2), l (list),
g <name> (send a file), s (screenshot). This script sends them, saves what
comes back and, for `record`, plays a sound on the Mac while the recorder
records it.

    PY=~/.platformio/penv/bin/python
    $PY tools/serial_cmd.py list
    $PY tools/serial_cmd.py get REC_0001.wav out.wav
    $PY tools/serial_cmd.py snap screen.png
    $PY tools/serial_cmd.py key1            # or key2
    $PY tools/serial_cmd.py monitor 10      # print what the board says
    $PY tools/serial_cmd.py record 30 --play tone.wav --volume 0.5 --out rec.wav

The port is opened so that the DTR/RTS lines never pass through the state
that resets the chip (see open_port()): CLAUDE.md records that toggling
them can reset the board or drop it into its bootloader.

Needs only pyserial, which ships with PlatformIO.
"""
import argparse
import glob
import struct
import subprocess
import sys
import time
import zlib

import serial


def find_port():
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        sys.exit("No /dev/cu.usbmodem* port found; is the board connected?")
    return ports[0]


def open_port(port=None):
    """Opens the port without resetting the board.

    The USB-Serial-JTAG resets the chip while RTS is high and DTR is low.
    macOS raises both lines when the port opens; pyserial then applies DTR
    before RTS, so clearing both passes through exactly that reset state.
    Keeping DTR high until RTS is down avoids it: (1,1) -> (1,0) -> (0,0).
    """
    link = serial.Serial()
    link.port = port or find_port()
    link.baudrate = 115200
    link.timeout = 0.2
    link.dtr = True
    link.rts = False
    link.open()
    link.dtr = False
    return link


def send(link, line):
    link.write((line + "\n").encode())
    link.flush()


_partial = b""  # a line cut off by a deadline, continued by the next call


def read_line(link, deadline, echo=True, want=None):
    """Next text line; with `want`, skips lines until one starts with it."""
    global _partial
    while time.time() < deadline:
        c = link.read(1)
        if not c:
            continue
        if c == b"\n":
            line = _partial.decode(errors="replace").rstrip("\r")
            _partial = b""
            if want is None or line.startswith(want):
                return line
            if echo:
                print("  | " + line)
            continue
        _partial += c
    return None


def read_exact(link, size, deadline):
    data = b""
    while len(data) < size:
        if time.time() > deadline:
            sys.exit("Transfer cut short: %d of %d bytes." % (len(data), size))
        data += link.read(size - len(data))
    return data


def list_files(link):
    send(link, "l")
    deadline = time.time() + 5
    head = read_line(link, deadline, want="LIST ")
    if head is None:
        sys.exit("No LIST answer from the board.")
    files = []
    while True:
        line = read_line(link, deadline, echo=False)
        if line is None or line == "END":
            break
        name, size = line.split()
        files.append((name, int(size)))
    return head, files


def get_file(link, name, out):
    send(link, "g " + name)
    deadline = time.time() + 120
    head = read_line(link, deadline, want="FILE ")
    if head is None:
        sys.exit("No FILE answer from the board.")
    _, got, size = head.split()
    data = read_exact(link, int(size), deadline)
    with open(out, "wb") as f:
        f.write(data)
    print("saved %s (%d bytes) -> %s" % (got, len(data), out))


def write_png(path, width, height, pixels, scale=3):
    rows = []
    for y in range(height):
        row = bytearray()
        for x in range(width):
            i = 2 * (y * width + x)
            v = (pixels[i] << 8) | pixels[i + 1]
            r, g, b = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
            row += bytes(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))) * scale
        rows += [bytes(row)] * scale

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    raw = b"".join(b"\x00" + row for row in rows)
    header = struct.pack(">IIBBBBB", width * scale, height * scale, 8, 2, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def snap(link, out):
    send(link, "s")
    deadline = time.time() + 10
    head = read_line(link, deadline, want="SNAP ")
    if head is None:
        sys.exit("No SNAP answer from the board.")
    _, w, h = head.split()
    w, h = int(w), int(h)
    write_png(out, w, h, read_exact(link, w * h * 2, deadline))
    print("screenshot -> " + out)


def monitor(link, seconds):
    deadline = time.time() + seconds
    while time.time() < deadline:
        line = read_line(link, deadline, echo=False)
        if line is not None:
            print("  | " + line)


def record(link, seconds, play, volume, out, lead):
    """KEY1, play a sound (optional), wait, KEY1, fetch the new file."""
    _, before = list_files(link)
    known = {name for name, _ in before}
    send(link, "r")
    started = time.time()
    time.sleep(lead)
    player = None
    if play:
        player = subprocess.Popen(["afplay", "-v", str(volume), play])
    while time.time() - started < seconds:
        line = read_line(link, min(started + seconds, time.time() + 1), echo=False)
        if line:
            print("  | " + line)
    send(link, "r")
    print("host time between the two KEY1 commands: %.2f s" % (time.time() - started))
    if player:
        player.wait()
    monitor(link, 1.0)
    _, after = list_files(link)
    new = [name for name, _ in after if name not in known]
    if not new:
        sys.exit("No new recording appeared.")
    get_file(link, new[0], out)


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("key1")
    sub.add_parser("key2")
    sub.add_parser("list")
    g = sub.add_parser("get")
    g.add_argument("name")
    g.add_argument("out")
    s = sub.add_parser("snap")
    s.add_argument("out")
    m = sub.add_parser("monitor")
    m.add_argument("seconds", type=float)
    r = sub.add_parser("record")
    r.add_argument("seconds", type=float)
    r.add_argument("--play")
    r.add_argument("--volume", type=float, default=0.5)
    r.add_argument("--out", required=True)
    r.add_argument("--lead", type=float, default=1.0,
                   help="seconds of silence before the sound starts")
    args = ap.parse_args()

    link = open_port()
    try:
        link.reset_input_buffer()
        if args.cmd == "key1":
            send(link, "r")
            monitor(link, 0.5)
        elif args.cmd == "key2":
            send(link, "w")
            monitor(link, 0.5)
        elif args.cmd == "list":
            head, files = list_files(link)
            print(head)
            for name, size in files:
                print("%s %d" % (name, size))
        elif args.cmd == "get":
            get_file(link, args.name, args.out)
        elif args.cmd == "snap":
            snap(link, args.out)
        elif args.cmd == "monitor":
            monitor(link, args.seconds)
        elif args.cmd == "record":
            record(link, args.seconds, args.play, args.volume, args.out, args.lead)
    finally:
        link.close()


if __name__ == "__main__":
    main()
