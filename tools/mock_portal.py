"""Serves the recorder's web page on the Mac, without the board.

    ~/.platformio/penv/bin/python tools/mock_portal.py <dir_with_wavs> [port]

Takes the page straight from src/WebPage.h and answers the same routes as
src/WebPortal.cpp, with the same JSON, using REC_nnnn.wav files from a
directory (for example recordings pulled off the board with
tools/serial_cmd.py). DELETE only hides a file for the session; nothing on
disk is removed. Handy for working on the page: joining the recorder's own
network cuts the Mac off the internet.
"""
import http.server
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAGE = re.search(r'R"HTML\((.*)\)HTML"', open(os.path.join(ROOT, "src/WebPage.h")).read(),
                 re.S).group(1).encode()
NAME = re.compile(r"^REC_(?!0000)\d{4}\.wav$")
BLOCK, SPB, RATE, HEADER = 256, 505, 8000, 60
TOTAL = 6160384  # the LittleFS partition
RESERVE = 64 * 1024


class Handler(http.server.BaseHTTPRequestHandler):
    wav_dir = "."
    hidden = set()

    def names(self):
        found = [n for n in os.listdir(self.wav_dir) if NAME.match(n) and n not in self.hidden]
        return sorted(found, reverse=True)

    def send(self, code, body=b"", kind="text/plain", extra=()):
        self.send_response(code)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(body)))
        for k, v in extra:
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/":
            return self.send(200, PAGE, "text/html; charset=utf-8")
        if self.path == "/api/recordings":
            recs, used = [], 0
            for n in self.names():
                size = os.path.getsize(os.path.join(self.wav_dir, n))
                used += size
                blocks = max(0, size - HEADER) // BLOCK
                recs.append({"name": n, "bytes": size, "seconds": blocks * SPB // RATE})
            free = TOTAL - used - 8192
            left = max(0, free - RESERVE) // BLOCK * SPB // RATE
            body = json.dumps({"total": TOTAL, "free": free, "secondsLeft": left,
                               "recordings": recs}).encode()
            return self.send(200, body, "application/json")
        m = re.match(r"^/rec/(.+)$", self.path)
        if m and m.group(1) in self.names():
            data = open(os.path.join(self.wav_dir, m.group(1)), "rb").read()
            return self.send(200, data, "audio/wav",
                             [("Content-Disposition", 'attachment; filename="%s"' % m.group(1))])
        self.send(404, b"Not found")

    def do_DELETE(self):
        m = re.match(r"^/rec/(.+)$", self.path)
        if m and m.group(1) in self.names():
            Handler.hidden.add(m.group(1))
            return self.send(204)
        self.send(404, b"Not found")


def main():
    Handler.wav_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 8080
    print("http://localhost:%d  (recordings from %s)" % (port, Handler.wav_dir))
    http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()


if __name__ == "__main__":
    main()
