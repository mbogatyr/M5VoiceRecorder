"""Serves the recorder's web page on the Mac, without the board.

    ~/.platformio/penv/bin/python tools/mock_portal.py <dir_with_wavs> [port] [--full]

Takes the page straight from src/WebPage.h and answers the same routes as
src/WebPortal.cpp, with the same JSON, using REC_nnnn.wav files from a
directory (for example recordings pulled off the board with
tools/serial_cmd.py). Record/Stop fake a recording: the timer ticks, the
waveform moves, and Stop adds a silent recording that lives in memory.
Nothing on disk is changed: deletions only hide files for the session.
--full pretends the memory is full (Record disabled).

Handy for working on the page: joining the recorder's own network cuts the
Mac off the internet.
"""
import http.server
import json
import math
import os
import random
import re
import struct
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAGE = re.search(r'R"HTML\((.*)\)HTML"', open(os.path.join(ROOT, "src/WebPage.h")).read(),
                 re.S).group(1).encode()
NAME = re.compile(r"^REC_(?!0000)\d{4}\.wav$")
BLOCK, SPB, RATE, HEADER = 256, 505, 8000, 60
TOTAL = 6160384  # the LittleFS partition
RESERVE = 64 * 1024


def silent_wav(seconds):
    blocks = max(1, int(seconds * RATE / SPB))
    data = b"\0" * (blocks * BLOCK)
    fmt = struct.pack("<HHIIHHHH", 0x11, 1, RATE, RATE * BLOCK // SPB, BLOCK, 4, 2, SPB)
    return (b"RIFF" + struct.pack("<I", 52 + len(data)) + b"WAVE" + b"fmt " + struct.pack("<I", 20)
            + fmt + b"fact" + struct.pack("<II", 4, blocks * SPB) + b"data"
            + struct.pack("<I", len(data)) + data)


class Mock:
    wav_dir = "."
    full = False
    hidden = set()
    made = {}          # name -> bytes, recordings "made" by Record/Stop
    recording = None   # (name, start time)
    saved = ("", 0, 0)
    levels = [0] * 54
    last_number = 0

    @classmethod
    def files(cls):
        found = {n: os.path.getsize(os.path.join(cls.wav_dir, n))
                 for n in os.listdir(cls.wav_dir) if NAME.match(n)}
        found.update({n: len(b) for n, b in cls.made.items()})
        return {n: s for n, s in found.items() if n not in cls.hidden}

    @classmethod
    def free(cls):
        return TOTAL - sum(cls.files().values()) - 8192

    @classmethod
    def seconds_for(cls, size):
        return max(0, size - HEADER) // BLOCK * SPB // RATE

    @classmethod
    def status(cls):
        left = max(0, cls.free() - RESERVE) // BLOCK * SPB // RATE
        s = {"state": "ready", "secondsLeft": left, "stoppedFull": False,
             "canRecord": cls.recording is None and not cls.full}
        if cls.recording:
            name, start = cls.recording
            secs = time.time() - start
            t = secs * 7
            cls.levels = cls.levels[1:] + [int(255 * (0.3 + 0.6 * abs(math.sin(t / 2.3)) * random.random()))
                                           if math.sin(t / 7) > -0.3 else random.randint(0, 40)]
            s.update(state="recording", name=name, seconds=int(secs),
                     bytes=HEADER + int(secs * RATE / SPB) * BLOCK,
                     levels="".join("%02x" % v for v in cls.levels))
        else:
            s.update(name=cls.saved[0], seconds=cls.saved[1], bytes=cls.saved[2])
        return s


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def send(self, code, body=b"", kind="text/plain", extra=()):
        if isinstance(body, str):
            body = body.encode()
        self.send_response(code)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(body)))
        for k, v in extra:
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def json(self, obj):
        self.send(200, json.dumps(obj), "application/json")

    def busy(self):
        if Mock.recording:
            self.send(409, "Recording in progress")
            return True
        return False

    def do_GET(self):
        if self.path == "/":
            return self.send(200, PAGE, "text/html; charset=utf-8")
        if self.path == "/api/status":
            return self.json(Mock.status())
        if self.path == "/api/recordings":
            files = Mock.files()
            recs = [{"name": n, "bytes": s, "seconds": Mock.seconds_for(s)}
                    for n, s in sorted(files.items(), reverse=True)]
            free = Mock.free()
            left = max(0, free - RESERVE) // BLOCK * SPB // RATE
            return self.json({"total": TOTAL, "free": free, "secondsLeft": left, "recordings": recs})
        m = re.match(r"^/rec/(.+)$", self.path)
        if m and m.group(1) in Mock.files():
            if self.busy():
                return
            n = m.group(1)
            data = Mock.made.get(n) or open(os.path.join(Mock.wav_dir, n), "rb").read()
            return self.send(200, data, "audio/wav",
                             [("Content-Disposition", 'attachment; filename="%s"' % n)])
        self.send(404, "Not found")

    def do_POST(self):
        if self.path == "/api/record":
            if Mock.recording:
                return self.send(409, "Already recording")
            if Mock.full:
                return self.send(409, "Memory full")
            numbers = [int(n[4:8]) for n in Mock.files()] + [Mock.last_number]
            Mock.last_number = max(numbers) + 1
            Mock.recording = ("REC_%04d.wav" % Mock.last_number, time.time())
            Mock.levels = [0] * 54
            return self.json(Mock.status())
        if self.path == "/api/stop":
            if not Mock.recording:
                return self.send(409, "Not recording")
            name, start = Mock.recording
            data = silent_wav(time.time() - start)
            Mock.made[name] = data
            Mock.recording = None
            Mock.saved = (name, Mock.seconds_for(len(data)), len(data))
            status = Mock.status()
            status["state"] = "saved"
            return self.json(status)
        self.send(404, "Not found")

    def do_DELETE(self):
        if self.path == "/api/recordings":
            if self.busy():
                return
            names = list(Mock.files())
            Mock.hidden.update(names)
            return self.json({"deleted": len(names)})
        m = re.match(r"^/rec/(.+)$", self.path)
        if m and m.group(1) in Mock.files():
            if self.busy():
                return
            Mock.hidden.add(m.group(1))
            return self.send(204)
        self.send(404, "Not found")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    Mock.full = "--full" in sys.argv
    Mock.wav_dir = args[0] if args else "."
    port = int(args[1]) if len(args) > 1 else 8080
    print("http://localhost:%d  (recordings from %s%s)"
          % (port, Mock.wav_dir, ", memory full" if Mock.full else ""))
    http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()


if __name__ == "__main__":
    main()
