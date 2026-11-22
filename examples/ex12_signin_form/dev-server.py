#!/usr/bin/env python3
"""dev-server.py - a sign-in server for ex12 to talk to, on this machine only.

    python3 examples/ex12_signin_form/dev-server.py [port]        (8765 by default)
    ./build/rayclay_ex12_signin_form                              (started from the same folder)

While it runs it names itself in signin-server.txt in the folder it was started from, where
ex12 looks (src/app/server.h), and removes the file when it stops.

POST /session {"email", "password"} answers 200 {"token", "expiresIn"} when the password is
"rayclay", else 401. GET /me answers 200 {"email"} for a live bearer token, else 401. DELETE
/session forgets the token (204). Tokens live in memory for SIGNIN_SESSION_SECONDS (120 by
default). A teaching aid, not a server to deploy: no TLS, no stored accounts, loopback only.
"""
import json
import os
import secrets
import signal
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

SESSION_SECONDS = int(os.environ.get("SIGNIN_SESSION_SECONDS", "120"))
PASSWORD = "rayclay"
NAME_FILE = "signin-server.txt"
sessions = {}  # token -> (email, expires at)


class Handler(BaseHTTPRequestHandler):
    def answer(self, status, body=None):
        data = json.dumps(body).encode() if body is not None else b""
        self.send_response(status)
        if body is not None:
            self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def bearer(self):
        auth = self.headers.get("Authorization", "")
        token = auth[7:] if auth.startswith("Bearer ") else ""
        email, until = sessions.get(token, (None, 0.0))
        if email is None or time.monotonic() >= until:
            sessions.pop(token, None)
            return None, None
        return token, email

    def do_POST(self):
        if self.path != "/session":
            return self.answer(404, {"error": "not found"})
        try:
            asked = json.loads(self.rfile.read(int(self.headers.get("Content-Length", "0"))))
        except ValueError:
            return self.answer(400, {"error": "not JSON"})
        if asked.get("password") != PASSWORD or not asked.get("email"):
            return self.answer(401, {"error": "wrong email or password"})
        token = secrets.token_urlsafe(32)
        sessions[token] = (asked["email"], time.monotonic() + SESSION_SECONDS)
        self.answer(200, {"token": token, "expiresIn": SESSION_SECONDS})

    def do_GET(self):
        if self.path != "/me":
            return self.answer(404, {"error": "not found"})
        token, email = self.bearer()
        if token is None:
            return self.answer(401, {"error": "no live session"})
        self.answer(200, {"email": email})

    def do_DELETE(self):
        token, _ = self.bearer()
        sessions.pop(token, None)
        self.answer(204)


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    url = f"http://127.0.0.1:{port}"
    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))
    with open(NAME_FILE, "w", encoding="utf-8") as f:
        f.write(url + "\n")
    print(f"ex12 dev server on {url}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        os.remove(NAME_FILE)
