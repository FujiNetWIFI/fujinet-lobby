#!/usr/bin/env python3
"""lobbyproxy.py -- the rig's lobby: a proxy in front of a private lobby
server, which can change the server's rooms between two GETs.

    lobbyproxy.py PORT UPSTREAM [AFTER FILE [METHOD]]

Forwards every request to UPSTREAM (http://127.0.0.1:port) and logs it.
Before the AFTERth GET /view it sends FILE (one room, JSON) to UPSTREAM's
/server with METHOD (POST: the room changed, e.g. its players -- the list's
order moves; DELETE: the room gone), so a join's second look finds the room
moved or gone.
"""
import http.server
import sys
import urllib.request

PORT, UPSTREAM = int(sys.argv[1]), sys.argv[2]
AFTER = int(sys.argv[3]) if len(sys.argv) > 3 else 0
FILE = sys.argv[4] if len(sys.argv) > 4 else None
METHOD = sys.argv[5] if len(sys.argv) > 5 else "POST"
views = 0


def change():
    body = open(FILE, "rb").read()
    req = urllib.request.Request(UPSTREAM + "/server", data=body, method=METHOD,
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req) as r:
        print("proxy: %s /server -> %d" % (METHOD, r.status), flush=True)


class Proxy(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        global views
        if self.path.startswith("/view"):
            views += 1
            if FILE and views == AFTER:
                change()
        try:
            with urllib.request.urlopen(UPSTREAM + self.path) as r:
                code, body, ctype = r.status, r.read(), r.headers.get("Content-Type")
        except urllib.error.HTTPError as e:
            code, body, ctype = e.code, e.read(), e.headers.get("Content-Type")
        print("proxy: GET %s -> %d, %d bytes" % (self.path, code, len(body)), flush=True)
        self.send_response(code)
        self.send_header("Content-Type", ctype or "application/octet-stream")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *a):
        pass


http.server.ThreadingHTTPServer(("127.0.0.1", PORT), Proxy).serve_forever()
