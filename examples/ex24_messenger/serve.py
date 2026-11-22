#!/usr/bin/env python3
"""
serve.py - the chat server RayClay Messenger (ex24_messenger) talks to, in Python 3's
standard library alone (no pip). RFC 6455 is written out below, the handshake and
the framing both, so every byte the app's rcWebSocket meets is in this file. Run
one, connect two windows (or a window and the web build), and each sees the other.

    python3 examples/ex24_messenger/serve.py [--port 8789] [--web <dir>]
                                             [--allow-origin <origin>] ...
                                             [--then drop|close|restart --after <seconds>]
    python3 examples/ex24_messenger/serve.py --selftest

    ws://127.0.0.1:8789/room/<general|design|random>   one socket per room
    GET /dump                                          every room's messages, as JSON
    GET /<file>                                        a file of the web build --web names

It listens on 127.0.0.1 only. A socket offering the subprotocol rayclay.chat.v1
gets it. Every message is one JSON object in one text frame:

    app -> server   {"type": "join", "name": "Ada", "client": "3f9a01c2"}
                                                         first, once open ("" = a Guest name)
                    {"type": "say", "text": "...", "nonce": 7}
                    {"type": "name", "name": "Ada"}
                    {"type": "simulate", "what": "drop" | "close" | "restart" | "stall"}
                    a binary frame: bulk bytes, counted and dropped (the app's burst)
    server -> app   {"type": "welcome", "room", "you": person, "people": [person],
                     "history": [message], "rooms": [{"name", "people"}], "limits": {"text"}}
                    {"type": "message", "id", "from", "name", "text", "at", "client", "nonce"}
                    {"type": "joined" | "left", "person": person, "at"}
                    {"type": "renamed", "person": person, "was", "at"}
                    {"type": "rooms", "rooms": [...]}      on every join and leave, anywhere
                    {"type": "error", "text"}  {"type": "note", "text"}  {"type": "sink", "bytes"}

A person is {"id", "name"}; ids and message ids are positive and never reused while
the server runs. "client" is the app's own tag for this run, and with its "nonce"
it names a line in the history too, so an app whose socket ended before the echo
came can tell its line arrived. Text and names that UTF-8 cannot carry (a lone
surrogate, sent as a JSON escape) are refused before anything keeps them.

The Simulate commands show each state on purpose: drop ends the TCP connection
with no Close (the app reads 1006 and reconnects with a back-off), close sends
Close 4001 with a reason (the app stops and says so), restart sends Close 1012 and
refuses upgrades with 503 for three seconds (the app's back-off meets a
refusal), stall stops reading that socket for ten seconds (what the app sends
meanwhile fills the operating system's socket buffers first; only what they
cannot hold waits in its bufferedAmount). --then does one of the first three to
every socket --after seconds from its join, so each state shows with no click.

Every request must name this server in its Host (127.0.0.1, localhost or [::1],
at its port), so a page on another name cannot read /dump through DNS
rebinding; an upgrade must say Connection: upgrade and Upgrade: websocket, and a
page's Origin must be this server's own (the page --web serves) or one named
with --allow-origin; an app sends none. No lock is ever held across the network:
each socket has a writer thread, and one whose peer stops reading is shut down
once BACKLOG bytes wait for it. A session that meets an error of the server's
own ends with Close 1011 (a bad HTTP request: 400). Ctrl+C closes every socket
with 1001.
"""
import argparse
import base64
import hashlib
import json
import os
import select
import socket
import struct
import sys
import tempfile
import threading
import time
from collections import deque
from urllib.parse import unquote, urlsplit

GUID = b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"   # RFC 6455 section 1.3
PROTOCOL = "rayclay.chat.v1"
ROOMS = ("general", "design", "random")
TEXT_MAX = 500          # characters in one message, as Python counts them
NAME_MAX = 24
ROOM_MAX = 32           # people in one room
HISTORY = 50            # messages a room keeps and a join is sent
MESSAGE_MAX = 1 << 20   # bytes in one received message; past it, Close 1009
PING_EVERY = 25.0       # seconds of silence before the server pings
STALL = 10.0
REFUSE_FOR = 3.0
BACKLOG = 4 << 20       # bytes queued for one socket; past it, its peer is not reading: shut it
CLIENT_MAX = 16         # characters of an app's own tag, hex
TYPES = {".html": "text/html; charset=utf-8", ".js": "text/javascript", ".wasm": "application/wasm",
         ".json": "application/json", ".css": "text/css", ".png": "image/png",
         ".data": "application/octet-stream"}

# The few lines each room starts with, so a first window has a conversation to
# join. (name, text, minutes ago). The app's demo room (src/app/messenger_backend.h) opens on
# the same lines.
SEED = {
    "general": (("Maya", "Morning, everyone.", 14),
                ("Theo", "Morning! The projector in the big room works again.", 12),
                ("Priya", "Nice. Was it the cable after all?", 11),
                ("Theo", "That one. It was loose the whole time.", 9)),
    "design": (("Jonas", "I pushed the dark palette for the settings page.", 30),
               ("Maya", "The muted text reads much better on it.", 26)),
    "random": (("Theo", "Lunch at the usual place?", 40),
               ("Lena", "Only if they still have the soup.", 38)),
}

OP_CONT, OP_TEXT, OP_BINARY, OP_CLOSE, OP_PING, OP_PONG = 0x0, 0x1, 0x2, 0x8, 0x9, 0xA


# --------------------------------------------------------------------- RFC 6455: the handshake

def accept_of(key):
    """Sec-WebSocket-Accept for a Sec-WebSocket-Key (section 4.2.2)."""
    return base64.b64encode(hashlib.sha1(key.encode("ascii") + GUID).digest()).decode("ascii")


def parse_head(data):
    """(method, target, headers) of an HTTP request head; header names lowercased,
    repeats joined with ", " as HTTP allows."""
    lines = data.decode("latin-1").split("\r\n")
    method, target, _ = (lines[0].split(" ", 2) + ["", ""])[:3]
    headers = {}
    for line in lines[1:]:
        name, _, value = line.partition(":")
        key = name.strip().lower()
        if key:
            headers[key] = (headers[key] + ", " if key in headers else "") + value.strip()
    return method, target, headers


def tokens(value):
    """A header's comma-separated tokens, lowercased (HTTP tokens compare without case)."""
    return [t.strip().lower() for t in (value or "").split(",") if t.strip()]


def host_ok(host, port):
    """Whether a Host header names this server: a loopback name at its port, and nothing
    else, so a page served under another name (DNS rebinding) is refused."""
    return (host or "").strip().lower() in ("127.0.0.1:%d" % port, "localhost:%d" % port,
                                            "[::1]:%d" % port)


def handshake(method, headers, origin_ok):
    """The upgrade's answer: (status, extra header lines, protocol). 101 takes it. The
    Connection header must hold the token upgrade, and Upgrade must be websocket."""
    key = headers.get("sec-websocket-key", "")
    if (method != "GET" or "upgrade" not in tokens(headers.get("connection"))
            or tokens(headers.get("upgrade")) != ["websocket"] or not key):
        return 400, [], None
    try:
        if len(base64.b64decode(key, validate=True)) != 16:
            return 400, [], None
    except ValueError:
        return 400, [], None
    if headers.get("sec-websocket-version") != "13":
        return 426, ["Sec-WebSocket-Version: 13"], None
    if not origin_ok(headers.get("origin")):
        return 403, [], None
    offered = [p.strip() for p in headers.get("sec-websocket-protocol", "").split(",") if p.strip()]
    lines = ["Upgrade: websocket", "Connection: Upgrade", "Sec-WebSocket-Accept: " + accept_of(key)]
    proto = PROTOCOL if PROTOCOL in offered else None
    if proto:
        lines.append("Sec-WebSocket-Protocol: " + proto)
    return 101, lines, proto


# --------------------------------------------------------------------- RFC 6455: the framing

class ProtocolError(Exception):
    """The peer broke the protocol: the Close code to answer with, and why."""

    def __init__(self, code, why):
        super().__init__(why)
        self.code = code


def encode(opcode, payload=b"", fin=True, mask=None):
    """One frame. A server sends unmasked (mask None); a client masks with a
    four-byte key, as the selftest's client does (section 5.3)."""
    head = bytes([(0x80 if fin else 0) | opcode])
    bit = 0x80 if mask is not None else 0
    n = len(payload)
    if n < 126:
        head += bytes([bit | n])
    elif n < 65536:
        head += bytes([bit | 126]) + struct.pack("!H", n)
    else:
        head += bytes([bit | 127]) + struct.pack("!Q", n)
    if mask is not None:
        head += mask
        payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
    return head + payload


def close_payload(code, reason=""):
    """A Close's body: the code, then at most 123 bytes of reason, cut where a character
    starts, never inside one, so the reason stays UTF-8."""
    return struct.pack("!H", code) + reason.encode("utf-8")[:123].decode("utf-8",
                                                                         "ignore").encode("utf-8")


def valid_close_code(code):
    """A code a peer may put on the wire (section 7.4): never 1004-1006 or 1015."""
    return code in (1000, 1001, 1002, 1003) or 1007 <= code <= 1014 or 3000 <= code <= 4999


class Reader:
    """Frames out of a byte stream, then whole messages out of the frames: a
    client's frames must be masked; control frames are whole and at most 125
    bytes, and may come between the fragments of a message; text must be UTF-8."""

    def __init__(self, limit=MESSAGE_MAX):
        self.buf, self.limit = b"", limit
        self.parts, self.kind, self.size = [], None, 0

    def feed(self, data):
        """Every event the bytes complete: ("text", str), ("binary", bytes),
        ("ping", bytes), ("pong", bytes), ("close", code or None, reason)."""
        self.buf += data
        events = []
        while True:
            frame = self._frame()
            if frame is None:
                return events
            events.extend(self._message(*frame))

    def _frame(self):
        b = self.buf
        if len(b) < 2:
            return None
        fin, rsv, opcode = bool(b[0] & 0x80), b[0] & 0x70, b[0] & 0x0F
        masked, n, at = bool(b[1] & 0x80), b[1] & 0x7F, 2
        if rsv:
            raise ProtocolError(1002, "a reserved bit is set and no extension was agreed")
        if opcode not in (OP_CONT, OP_TEXT, OP_BINARY, OP_CLOSE, OP_PING, OP_PONG):
            raise ProtocolError(1002, "opcode %d is reserved" % opcode)
        if not masked:
            raise ProtocolError(1002, "a client frame must be masked")
        if n == 126:
            if len(b) < 4:
                return None
            n, at = struct.unpack("!H", b[2:4])[0], 4
        elif n == 127:
            if len(b) < 10:
                return None
            n, at = struct.unpack("!Q", b[2:10])[0], 10
            if n >> 63:
                raise ProtocolError(1002, "a 64-bit length has its top bit set")
        if opcode >= OP_CLOSE and (not fin or n > 125):
            raise ProtocolError(1002, "a control frame must be whole and at most 125 bytes")
        if n > self.limit:
            raise ProtocolError(1009, "a frame of %d bytes is over the limit" % n)
        if len(b) < at + 4 + n:
            return None
        key, payload = b[at:at + 4], b[at + 4:at + 4 + n]
        self.buf = b[at + 4 + n:]
        return fin, opcode, bytes(c ^ key[i % 4] for i, c in enumerate(payload))

    def _message(self, fin, opcode, payload):
        if opcode == OP_PING:
            return [("ping", payload)]
        if opcode == OP_PONG:
            return [("pong", payload)]
        if opcode == OP_CLOSE:
            if len(payload) == 1:
                raise ProtocolError(1002, "a Close body of one byte")
            if not payload:
                return [("close", None, "")]
            code = struct.unpack("!H", payload[:2])[0]
            if not valid_close_code(code):
                raise ProtocolError(1002, "close code %d may not be sent" % code)
            try:
                return [("close", code, payload[2:].decode("utf-8"))]
            except UnicodeDecodeError:
                raise ProtocolError(1007, "a Close reason that is not UTF-8")
        if opcode == OP_CONT and self.kind is None:
            raise ProtocolError(1002, "a continuation with no message to continue")
        if opcode != OP_CONT and self.kind is not None:
            raise ProtocolError(1002, "a new message inside a fragmented one")
        if opcode != OP_CONT:
            self.kind, self.parts, self.size = opcode, [], 0
        self.parts.append(payload)
        self.size += len(payload)
        if self.size > self.limit:
            raise ProtocolError(1009, "a message over %d bytes" % self.limit)
        if not fin:
            return []
        data, kind = b"".join(self.parts), self.kind
        self.kind, self.parts = None, []
        if kind == OP_BINARY:
            return [("binary", data)]
        try:
            return [("text", data.decode("utf-8"))]
        except UnicodeDecodeError:
            raise ProtocolError(1007, "a text message that is not UTF-8")


# --------------------------------------------------------------------- the rooms

class Member:
    """One open socket in one room. `frame` is the only way out, under a lock, so the
    room's broadcasts and this socket's own answers never interleave. The transport
    only queues (Transport below), so the lock is never held across the network."""

    def __init__(self, hub, room, transport):
        self.hub, self.room, self.transport = hub, room, transport
        self.id, self.name, self.client = 0, "", ""
        self.joined_at = None
        self.lock = threading.Lock()
        self.closing = False
        self.closed_at = 0.0     # time.monotonic() when this end sent its Close
        self.stall_until = 0.0
        self.sunk = 0
        self.acted = False

    def person(self):
        return {"id": self.id, "name": self.name}

    def send(self, value):
        self.frame(OP_TEXT, json.dumps(value, ensure_ascii=False,
                                       separators=(",", ":")).encode("utf-8"))

    def frame(self, opcode, payload=b""):
        """Write one frame, unless this end has sent its Close: a Close is the last frame
        (RFC 6455 section 5.5.1), so `closing` is set here, under the lock, before the
        lock lets a broadcast in."""
        with self.lock:
            if self.closing:
                return
            if opcode == OP_CLOSE:
                self.closing, self.closed_at = True, time.monotonic()
            try:
                self.transport.write(encode(opcode, payload))
            except OSError:
                self.closing = True

    def close(self, code, reason):
        """Start the closing handshake: no frame but the peer's Close follows."""
        self.frame(OP_CLOSE, close_payload(code, reason))


class Hub:
    """Every room, behind one lock: who is in it and what it said."""

    def __init__(self, now=time.time):
        self.now = now
        self.lock = threading.Lock()
        self.members = {r: [] for r in ROOMS}
        self.history = {r: deque(maxlen=HISTORY) for r in ROOMS}
        self.next_person, self.next_message = 1, 1
        self.refuse_until = 0.0
        start = int(now())
        for room, lines in SEED.items():
            for name, text, ago in lines:
                self._keep(room, {"type": "message", "id": self._mid(), "from": 0, "name": name,
                                  "text": text,
                                  "at": start - ago * 60})

    def _mid(self):
        self.next_message += 1
        return self.next_message - 1

    def _keep(self, room, message):
        self.history[room].append(message)

    def rooms(self):
        return [{"name": r, "people": len(self.members[r])} for r in ROOMS]

    def _everyone(self, value, room=None):
        """Send to a room, or to every socket. The lock is held; Member.send takes
        only its own."""
        for r in ROOMS if room is None else (room,):
            for m in self.members[r]:
                m.send(value)

    def full(self, room):
        with self.lock:
            return len(self.members[room]) >= ROOM_MAX

    def join(self, m, name, client=""):
        with self.lock:
            m.id, self.next_person = self.next_person, self.next_person + 1
            m.name = clean_name(name) or "Guest %d" % m.id
            m.client = clean_client(client)
            m.joined_at = time.monotonic()
            self.members[m.room].append(m)
            m.send({"type": "welcome", "room": m.room, "you": m.person(),
                    "people": [x.person() for x in self.members[m.room]],
                    "history": list(self.history[m.room]), "rooms": self.rooms(),
                    "limits": {"text": TEXT_MAX}})
            for x in self.members[m.room]:
                if x is not m:
                    x.send({"type": "joined", "person": m.person(), "at": int(self.now())})
            self._everyone({"type": "rooms", "rooms": self.rooms()})

    def leave(self, m):
        with self.lock:
            if m not in self.members[m.room]:
                return
            self.members[m.room].remove(m)
            self._everyone({"type": "left", "person": m.person(), "at": int(self.now())}, m.room)
            self._everyone({"type": "rooms", "rooms": self.rooms()})

    def say(self, m, text, nonce):
        if not isinstance(text, str) or not text.strip():
            return m.send({"type": "error", "text": "A message needs some text."})
        if len(text) > TEXT_MAX:
            return m.send({"type": "error", "text": "A message is at most %d characters."
                           % TEXT_MAX})
        if not utf8_ok(text):           # refused before the history keeps it: every join sends that
            return m.send({"type": "error", "text": "A message must be text UTF-8 can carry."})
        with self.lock:
            msg = {"type": "message", "id": self._mid(), "from": m.id, "name": m.name, "text": text,
                   "at": int(self.now()), "client": m.client, "nonce": nonce}
            self._keep(m.room, msg)
            for x in self.members[m.room]:
                x.send(msg)

    def rename(self, m, name):
        name = clean_name(name)
        if not name:
            return m.send({"type": "error", "text": "A name needs a letter or two."})
        with self.lock:
            was, m.name = m.name, name
            if was != name:
                self._everyone({"type": "renamed", "person": m.person(), "was": was,
                                "at": int(self.now())}, m.room)

    def take(self, m, text):
        """One text message from the app."""
        try:
            value = json.loads(text)
        except ValueError:
            return m.send({"type": "error", "text": "That was not JSON."})
        kind = value.get("type") if isinstance(value, dict) else None
        if kind == "join" and m.joined_at is None:
            return self.join(m, value.get("name", ""), value.get("client", ""))
        if m.joined_at is None:
            return m.send({"type": "error", "text": "Join first."})
        if kind == "say":
            nonce = value.get("nonce", 0)
            return self.say(m, value.get("text"), nonce if type(nonce) is int else 0)
        if kind == "name":
            return self.rename(m, value.get("name", ""))
        if kind == "simulate":
            return self.simulate(m, value.get("what"))
        return m.send({"type": "error", "text": "No message type %r." % kind})

    def simulate(self, m, what):
        if what == "drop":
            m.closing = True
            m.transport.drop()
        elif what == "close":
            m.close(4001, "The server closed this room (Simulate)")
        elif what == "restart":
            with self.lock:
                self.refuse_until = time.monotonic() + REFUSE_FOR
            m.close(1012, "The server is restarting")
        elif what == "stall":
            m.send({"type": "note", "text": "The server stops reading this socket for %d s."
                    % STALL})
            m.stall_until = time.monotonic() + STALL
        else:
            m.send({"type": "error", "text": "Nothing to simulate called %r." % what})

    def refusing(self):
        return time.monotonic() < self.refuse_until

    def dump(self):
        with self.lock:
            return {"rooms": {r: list(self.history[r]) for r in ROOMS}}

    def close_all(self, code, reason):
        with self.lock:
            everyone = [m for r in ROOMS for m in self.members[r]]
        for m in everyone:
            m.close(code, reason)


def utf8_ok(text):
    """Whether UTF-8 can carry the text: a lone surrogate, which JSON can escape, cannot."""
    try:
        text.encode("utf-8")
        return True
    except UnicodeEncodeError:
        return False


def clean_name(name):
    """A name as shown: no control characters and no lone surrogate (neither is
    printable), no space at the ends, NAME_MAX at most."""
    if not isinstance(name, str):
        return ""
    name = "".join(c for c in name if c.isprintable() and utf8_ok(c)).strip()[:NAME_MAX]
    return name if utf8_ok(name) else ""


def clean_client(client):
    """An app's tag as kept: up to CLIENT_MAX hex digits, else none."""
    ok = isinstance(client, str) and 0 < len(client) <= CLIENT_MAX
    return client if ok and all(c in "0123456789abcdef" for c in client) else ""


def serve_frames(hub, m, events):
    """What one socket's events do. False once the connection is done."""
    for ev in events:
        if ev[0] == "text":
            hub.take(m, ev[1])
        elif ev[0] == "binary":
            m.sunk += len(ev[1])
            m.send({"type": "sink", "bytes": m.sunk})
        elif ev[0] == "ping":
            m.frame(OP_PONG, ev[1])
        elif ev[0] == "close":                     # the peer's Close: answer with its code,
            m.frame(OP_CLOSE, close_payload(ev[1]) if ev[1] else b"")    # unless ours went first
            return False
    return True


def serve_bytes(hub, m, reader, data):
    """Bytes that came on one socket, read and acted on. False once the connection is
    done: the peer closed, it broke the protocol (Close with its code), or the server
    met an error of its own, which ends the session with Close 1011 rather than a
    thread that dies with no Close (a JSON nested past Python's recursion limit is
    one such)."""
    try:
        return serve_frames(hub, m, reader.feed(data))
    except ProtocolError as e:
        log("%s: %s, Close %d" % (m.name or "a socket", e, e.code))
        m.close(e.code, str(e))
    except OSError:
        pass
    except Exception as e:
        log("%s: the server failed (%s), Close 1011" % (m.name or "a socket", e.__class__.__name__))
        m.close(1011, "The server met an error")
    return False


# --------------------------------------------------------------------- the sockets

class Transport:
    """A connected socket, as Member writes to it: write() only queues, and a thread of
    the socket's own sends, so a room's broadcast never waits on one slow reader. A
    send that fails or times out (the socket's 10 s), or a queue past `backlog` (a
    peer that has stopped reading), shuts the socket down, so its reader ends and the
    member leaves its room."""

    def __init__(self, s, backlog=BACKLOG):
        self.s, self.backlog = s, backlog
        self.queue, self.waiting = deque(), 0
        self.cv = threading.Condition()
        self.dead = self.done = False
        self.thread = threading.Thread(target=self._send, daemon=True)
        self.thread.start()

    def write(self, data):
        with self.cv:
            if self.dead or self.done:
                return
            if self.waiting + len(data) > self.backlog:
                self._kill()
                return
            self.queue.append(data)
            self.waiting += len(data)
            self.cv.notify()

    def _send(self):
        while True:
            with self.cv:
                while not self.queue and not (self.done or self.dead):
                    self.cv.wait()
                if self.dead or not self.queue:
                    return
                data = self.queue.popleft()
                self.waiting -= len(data)
            try:
                self.s.sendall(data)
            except OSError:
                with self.cv:
                    self._kill()
                return

    def _kill(self):
        """The lock is held."""
        self.dead = True
        self.cv.notify()
        self.drop()

    def drop(self):
        try:
            self.s.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass

    def finish(self, wait=2.0):
        """No more writes: what is queued goes out, for at most `wait` seconds."""
        with self.cv:
            self.done = True
            self.cv.notify()
        self.thread.join(wait)


def web_file(root, path):
    """(status, file) for a GET of `path` from the web build at `root` (a real path):
    200 and the file, 404, or 400 for a path the file system cannot name (a NUL)."""
    try:
        name = unquote(path).lstrip("/") or ("chat.html"
                                             if os.path.isfile(os.path.join(root, "chat.html"))
                                             else "index.html")
        full = os.path.realpath(os.path.join(root, name))
        found = full.startswith(root + os.sep) and os.path.isfile(full)
    except ValueError:
        return 400, None
    return (200, full) if found else (404, None)


class Server:
    def __init__(self, port, web, origins, then, after):
        self.hub = Hub()
        self.web = os.path.realpath(web) if web else None
        self.origins, self.then, self.after = set(origins), then, after
        self.srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.srv.bind(("127.0.0.1", port))
        self.srv.listen(64)
        self.port = self.srv.getsockname()[1]
        self.own = {"http://127.0.0.1:%d" % self.port, "http://localhost:%d" % self.port}

    def origin_ok(self, origin):
        """An app sends no Origin; a page must be this server's, or one --allow-origin names."""
        return origin is None or origin in self.own or origin in self.origins or "*" in self.origins

    def run(self):
        while True:
            s, _ = self.srv.accept()
            threading.Thread(target=self.session, args=(s,), daemon=True).start()

    def session(self, s):
        try:
            self._session(s)
        finally:
            try:
                s.close()
            except OSError:
                pass

    def _session(self, s):
        try:
            s.settimeout(10)
            data = b""
            while b"\r\n\r\n" not in data:
                chunk = s.recv(4096)
                if not chunk or len(data) > 16384:
                    return None
                data += chunk
            head, rest = data.split(b"\r\n\r\n", 1)
            method, target, headers = parse_head(head)
            path = urlsplit(target).path
        except OSError:
            return None
        except Exception:                           # a head that is not HTTP
            return self.bye(s, 400)
        try:
            if not host_ok(headers.get("host"), self.port):
                log("refused a request for Host %r" % headers.get("host", ""))
                return self.reply(s, 421,
                                  b'{"error":"this server is 127.0.0.1, localhost or [::1]"}')
            if "upgrade" in headers:
                return self.upgrade(s, method, path, headers, rest)
            return self.plain(s, method, path)
        except OSError:
            return None
        except Exception as e:                      # the server's own error, before any upgrade
            log("a request failed (%s)" % e.__class__.__name__)
            return self.bye(s, 400)

    def bye(self, s, status):
        try:
            self.reply(s, status, b'{"error":"bad request"}')
        except OSError:
            pass

    def reply(self, s, status, body=b"", kind="application/json", extra=()):
        words = {101: "Switching Protocols", 200: "OK", 400: "Bad Request", 403: "Forbidden",
                 404: "Not Found",
                 421: "Misdirected Request", 426: "Upgrade Required", 503: "Service Unavailable"}
        lines = ["HTTP/1.1 %d %s" % (status, words.get(status, ""))] + list(extra)
        if status != 101:
            lines += ["Content-Type: " + kind, "Content-Length: %d" % len(body),
                      "Cache-Control: no-store",
                      "Connection: close"]
        s.sendall(("\r\n".join(lines) + "\r\n\r\n").encode("latin-1") + body)

    def plain(self, s, method, path):
        if method != "GET":
            return self.reply(s, 404, b'{"error":"not found"}')
        if path == "/dump":
            return self.reply(s, 200, json.dumps(self.hub.dump(),
                                                 ensure_ascii=False).encode("utf-8"))
        status, full = web_file(self.web, path) if self.web else (404, None)
        if status == 200:
            with open(full, "rb") as f:
                return self.reply(s, 200, f.read(),
                                  TYPES.get(os.path.splitext(full)[1], "application/octet-stream"))
        return self.reply(s, status, b'{"error":"%s"}'
                          % (b"not found" if status == 404 else b"bad path"))

    def upgrade(self, s, method, path, headers, rest):
        room = path[len("/room/"):] if path.startswith("/room/") else ""
        status, lines, _ = handshake(method, headers, self.origin_ok)
        if status == 101 and room not in ROOMS:
            status, lines = 404, []
        if status == 101 and (self.hub.refusing() or self.hub.full(room)):
            status, lines = 503, ["Retry-After: %d" % int(REFUSE_FOR)]
        if status != 101:
            log("refused an upgrade to %s: HTTP %d" % (path, status))
            return self.reply(s, status, b"", "text/plain", lines)
        self.reply(s, 101, extra=lines)
        m = Member(self.hub, room, Transport(s))
        reader = Reader()
        last = time.monotonic()
        pinged = told = False
        try:
            alive = serve_bytes(self.hub, m, reader, rest) if rest else True
            while alive:
                now = time.monotonic()
                if now < m.stall_until:
                    time.sleep(0.2)
                    continue
                if m.closed_at and now - m.closed_at > 5.0:
                    break                                    # our Close was never answered
                if m.joined_at is not None and not told:
                    told = True
                    log("%s joined #%s" % (m.name, room))
                if (self.then and m.joined_at is not None and not m.acted
                        and now - m.joined_at >= self.after):
                    m.acted = True
                    self.hub.simulate(m, self.then)
                if not select.select([s], [], [], 1.0)[0]:
                    if now - last > PING_EVERY * 2:
                        break                                # no answer to the ping: gone
                    if now - last > PING_EVERY and not pinged:
                        m.frame(OP_PING, b"are-you-there")
                        pinged = True
                    continue
                data = s.recv(65536)
                if not data:
                    break
                last, pinged = time.monotonic(), False
                alive = serve_bytes(self.hub, m, reader, data)
        except OSError:
            pass
        except Exception as e:                      # the loop's own: Close 1011, not a dead thread
            log("%s: the session failed (%s), Close 1011"
                % (m.name or "a socket", e.__class__.__name__))
            m.close(1011, "The server met an error")
        finally:
            if m.joined_at is not None:
                self.hub.leave(m)
                log("%s left #%s" % (m.name, room))
            m.transport.finish()


QUIET = []            # the selftest's: its planted failures need no log line


def log(line):
    if not QUIET:
        print("RayClay Messenger: " + line, flush=True)


# --------------------------------------------------------------------- the selftest

def selftest():
    """The framing and the rooms, with no port bound: each check beside a control
    that must come out the other way, so a check that cannot fail is caught."""
    checks = [0]
    QUIET.append(True)

    def want(cond, what):
        if not cond:
            raise SystemExit("SELFTEST FAIL: " + what)
        checks[0] += 1

    def refused(data, code, what):
        try:
            Reader().feed(data)
        except ProtocolError as e:
            return want(e.code == code, "%s: Close %d, not %d" % (what, code, e.code))
        want(False, what + ": accepted")

    key = b"\x01\x02\x03\x04"
    # The handshake: RFC 6455's own example key, and a control key that must differ.
    want(accept_of("dGhlIHNhbXBsZSBub25jZQ==") == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=",
         "the RFC's accept")
    want(accept_of("AQIDBAUGBwgJCgsMDQ4PEA==") != "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=",
         "control: another key")
    good = {"upgrade": "websocket", "connection": "keep-alive, Upgrade",
            "sec-websocket-key": "dGhlIHNhbXBsZSBub25jZQ==", "sec-websocket-version": "13",
            "sec-websocket-protocol": "x.other, " + PROTOCOL}
    st, lines, proto = handshake("GET", good, lambda o: True)
    want(st == 101 and proto == PROTOCOL
         and "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=" in lines,
         "an upgrade selects rayclay.chat.v1")
    want(handshake("GET", dict(good, **{"sec-websocket-protocol": "x.other"}),
                   lambda o: True)[2] is None,
         "control: no protocol of ours, none selected")
    want(handshake("GET", dict(good, **{"sec-websocket-version": "8"}), lambda o: True)[0] == 426,
         "version 8")
    want(handshake("GET", {k: v for k, v in good.items() if k != "sec-websocket-key"},
                   lambda o: True)[0] == 400,
         "no key")
    want(handshake("GET", dict(good, origin="https://elsewhere.example"),
                   lambda o: o is None)[0] == 403,
         "a foreign Origin")
    want(handshake("GET", good, lambda o: o is None)[0] == 101,
         "control: an app's request has no Origin")
    # Connection must hold the token upgrade, Upgrade must BE websocket, and Host must
    # name this server (a rebound name is refused), each beside the control that passes.
    want(handshake("GET", dict(good, connection="keep-alive"), lambda o: True)[0] == 400,
         "a Connection header with no upgrade token")
    want(handshake("GET", dict(good, upgrade="websocketx"), lambda o: True)[0] == 400,
         "an Upgrade that only contains websocket")
    want(handshake("GET", dict(good, upgrade="WebSocket", connection="Upgrade"),
                   lambda o: True)[0] == 101,
         "control: the tokens in any case")
    want(host_ok("127.0.0.1:8789", 8789) and host_ok("LOCALHOST:8789", 8789)
         and host_ok("[::1]:8789", 8789),
         "control: the three loopback names at the port")
    want(not host_ok("rebound.example:8789", 8789) and not host_ok("127.0.0.1:8790", 8789)
         and not host_ok(None, 8789), "a Host that is not this server's, or none")

    # Masking: the wire carries the payload XORed with the key, and the reader undoes it.
    wire = encode(OP_TEXT, b"hello", mask=key)
    want(b"hello" not in wire, "a masked frame hides its payload")
    want(Reader().feed(wire) == [("text", "hello")], "a masked frame reads back")
    want(Reader().feed(encode(OP_TEXT, b"hello", mask=b"\x00\x00\x00\x00")) == [("text", "hello")],
         "control: a zero key leaves the bytes as they are")
    refused(encode(OP_TEXT, b"hello"), 1002, "an unmasked client frame")

    # Lengths: the 7-bit, 16-bit and 64-bit forms, and a frame fed one byte at a time.
    for n in (125, 126, 65535, 65536):
        want(Reader().feed(encode(OP_BINARY, bytes(n), mask=key)) == [("binary", bytes(n))],
             "length %d" % n)
    r, got = Reader(), []
    for b in encode(OP_TEXT, "na\u00efve".encode("utf-8"), mask=key):
        got += r.feed(bytes([b]))
    want(got == [("text", "na\u00efve")], "a frame fed a byte at a time")
    try:
        Reader(limit=100).feed(encode(OP_BINARY, bytes(101), mask=key))
        want(False, "a frame over the limit was accepted")
    except ProtocolError as e:
        want(e.code == 1009, "a frame over the limit is 1009")

    # Fragmentation: three pieces with a ping between them make one message and a pong.
    data = (encode(OP_TEXT, b"frag", fin=False, mask=key) + encode(OP_PING, b"mid", mask=key)
            + encode(OP_CONT, b"ment", fin=False, mask=key) + encode(OP_CONT, b"ed", mask=key))
    want(Reader().feed(data) == [("ping", b"mid"), ("text", "fragmented")],
         "fragments and a ping between")
    refused(encode(OP_CONT, b"x", mask=key), 1002, "a continuation with nothing to continue")
    refused(encode(OP_TEXT, b"a", fin=False, mask=key) + encode(OP_TEXT, b"b", mask=key), 1002,
            "a new message inside a fragmented one")
    refused(encode(OP_PING, b"x", fin=False, mask=key), 1002, "a fragmented ping")
    refused(encode(OP_PING, bytes(126), mask=key), 1002, "a ping over 125 bytes")
    want(Reader().feed(encode(OP_PING, bytes(125), mask=key)) == [("ping", bytes(125))],
         "control: 125 is allowed")
    refused(encode(0x3, b"", mask=key), 1002, "a reserved opcode")
    refused(bytes([0xC1]) + encode(OP_TEXT, b"x", mask=key)[1:], 1002, "RSV1 with no extension")

    # UTF-8: text must be, binary need not.
    refused(encode(OP_TEXT, b"ok\xff\xfe", mask=key), 1007, "text that is not UTF-8")
    want(Reader().feed(encode(OP_BINARY, b"ok\xff\xfe", mask=key)) == [("binary", b"ok\xff\xfe")],
         "control: the same bytes as binary")

    # Close codes: what a peer may send, the ones it may not, an empty Close.
    want(Reader().feed(encode(OP_CLOSE, close_payload(1000, "bye"), mask=key)) == [("close", 1000,
                                                                                    "bye")],
         "Close 1000 with a reason")
    want(Reader().feed(encode(OP_CLOSE, close_payload(4001, "x"), mask=key)) == [("close", 4001,
                                                                                  "x")],
         "an application's 4001")
    want(Reader().feed(encode(OP_CLOSE, b"", mask=key)) == [("close", None, "")], "an empty Close")
    for code in (1005, 1006, 1015, 999, 2999, 5000):
        refused(encode(OP_CLOSE, close_payload(code), mask=key), 1002, "close code %d on the wire"
                % code)
    refused(encode(OP_CLOSE, b"\x03", mask=key), 1002, "a one-byte Close")
    refused(encode(OP_CLOSE, b"\x03\xe8\xff", mask=key), 1007, "a Close reason that is not UTF-8")
    want(len(close_payload(4001, "x" * 200)) == 125, "a reason is cut to 123 bytes")
    # A cut at 123 bytes can land inside a character; the cut steps back to its start.
    cut = close_payload(4001, "\u00e9" * 100)
    want(len(cut) <= 125 and cut[2:].decode("utf-8") == "\u00e9" * 61,
         "a long reason is cut where a character starts")
    try:
        ("\u00e9" * 100).encode("utf-8")[:123].decode("utf-8")
        naive = True
    except UnicodeDecodeError:
        naive = False
    want(not naive, "control: a plain cut at 123 bytes is not UTF-8")

    # The rooms, over two fake sockets: what each one is sent.
    class Fake:
        def __init__(self):
            self.out, self.dropped = b"", False

        def write(self, data):
            self.out += data

        def drop(self):
            self.dropped = True

        def sent(self):
            """Every frame written so far, as (opcode, payload); a server never masks."""
            frames, b = [], self.out
            while b:
                n, at = b[1] & 0x7F, 2
                if b[1] & 0x80:             # checked on every frame, counted as no check
                    raise SystemExit("SELFTEST FAIL: a server frame is masked")
                if n == 126:
                    n, at = struct.unpack("!H", b[2:4])[0], 4
                elif n == 127:
                    n, at = struct.unpack("!Q", b[2:10])[0], 10
                frames.append((b[0] & 0x0F, b[at:at + n]))
                b = b[at + n:]
            self.out = b""
            return [(op, json.loads(p) if op == OP_TEXT else p) for op, p in frames]

    hub = Hub(now=lambda: 1000000)
    a, b = Member(hub, "general", Fake()), Member(hub, "general", Fake())
    hub.take(a, '{"type":"join","name":"  Ada  ","client":"c0ffee"}')
    welcome = a.transport.sent()[0][1]
    want(welcome["type"] == "welcome" and welcome["you"]["name"] == "Ada"
         and len(welcome["history"]) == 4,
         "a join's welcome: the name, and the four seeded lines")
    hub.take(b, '{"type":"join","name":"","client":"NOT-HEX"}')
    want(b.name == "Guest %d" % b.id and a.client == "c0ffee" and b.client == "",
         "an empty name is a Guest's; a tag is hex or none")
    sent = [v for _, v in a.transport.sent()]
    want([v["type"] for v in sent] == ["joined", "rooms"] and sent[1]["rooms"][0]["people"] == 2,
         "the first is told of the second, and the count")
    b.transport.sent()
    hub.take(a, '{"type":"say","text":"hi","nonce":7}')
    mine, theirs = a.transport.sent()[0][1], b.transport.sent()[0][1]
    want(mine == theirs and mine["text"] == "hi" and mine["client"] == "c0ffee"
         and mine["nonce"] == 7,
         "a message carries its sender's tag and nonce")
    want(hub.history["general"][-1]["client"] == "c0ffee"
         and hub.history["general"][-1]["nonce"] == 7,
         "the history keeps them, so a line can be known after its socket ended")
    hub.take(a, json.dumps({"type": "say", "text": "x" * (TEXT_MAX + 1)}))
    want(a.transport.sent()[0][1]["type"] == "error" and not b.transport.sent(),
         "a message over the limit")
    hub.take(a, json.dumps({"type": "say", "text": "x" * TEXT_MAX}))
    want(b.transport.sent()[0][1]["type"] == "message", "control: a message at the limit")
    a.transport.sent()
    hub.take(b, '{"type":"name","name":"Bo"}')
    want(a.transport.sent()[0][1] == {"type": "renamed", "person": {"id": b.id, "name": "Bo"},
                                      "was": "Guest 2",
                                      "at": 1000000}, "a rename")
    b.transport.sent()
    want(serve_frames(hub, a, [("ping", b"p")]) and a.transport.sent()[-1] == (OP_PONG, b"p"),
         "a ping's pong")
    want(not serve_frames(hub, a, [("close", 1000, "bye")])
         and a.transport.sent()[-1] == (OP_CLOSE, close_payload(1000)),
         "a Close is answered with its code")
    a.send({"type": "note", "text": "late"})
    want(not a.transport.sent(), "control: nothing follows a Close")
    hub.leave(a)
    want([v["type"] for _, v in b.transport.sent()] == ["left", "rooms"], "a leave")
    hub.take(b, '{"type":"simulate","what":"close"}')
    want(b.transport.sent() == [(OP_CLOSE,
                                 close_payload(4001, "The server closed this room (Simulate)"))],
         "Simulate close: Close 4001 and its reason")
    c = Member(hub, "design", Fake())
    hub.take(c, '{"type":"join","name":"Cy"}')
    hub.take(c, '{"type":"simulate","what":"drop"}')
    want(c.transport.dropped and c.closing, "Simulate drop: the TCP connection ends, no Close")
    d = Member(hub, "random", Fake())
    hub.take(d, '{"type":"say","text":"x"}')
    want(d.transport.sent()[0][1]["text"] == "Join first.", "a message before the join")

    # Text UTF-8 cannot carry (a lone surrogate, as a JSON escape) is refused before the
    # history keeps it, so every later join and /dump still encode.
    h1 = Hub(now=lambda: 1000000)
    a1, b1 = Member(h1, "general", Fake()), Member(h1, "general", Fake())
    h1.take(a1, '{"type":"join","name":"Ada"}')
    h1.take(b1, '{"type":"join","name":"Bo"}')
    a1.transport.sent()
    b1.transport.sent()
    kept = len(h1.history["general"])
    h1.take(a1, '{"type":"say","text":"\\ud800","nonce":1}')
    want(a1.transport.sent()[0][1]["type"] == "error" and len(h1.history["general"]) == kept
         and not b1.transport.sent(), "a lone surrogate is refused, and kept nowhere")
    try:
        json.dumps(h1.dump(), ensure_ascii=False).encode("utf-8")
        dumped = True
    except UnicodeEncodeError:
        dumped = False
    c1 = Member(h1, "general", Fake())
    h1.take(c1, '{"type":"join","name":"\\ud800Cy"}')
    want(dumped and c1.name == "Cy" and c1.transport.sent()[0][1]["type"] == "welcome",
         "/dump and a later join still encode; a lone surrogate leaves a name")
    b1.transport.sent()
    h1.take(a1, '{"type":"say","text":"\\ud83d\\ude00","nonce":2}')
    want(b1.transport.sent()[-1][1]["text"] == "\U0001F600",
         "control: a surrogate PAIR is one character, kept")

    # A Close is the last frame. A thread broadcasting while the Close goes out
    # must not get a text frame in after it.
    h4 = Hub(now=lambda: 1000000)
    m4 = Member(h4, "general", Fake())
    stop = threading.Event()

    def chatter():
        while not stop.is_set():
            m4.send({"type": "note", "text": "x"})

    th = threading.Thread(target=chatter)
    th.start()
    time.sleep(0.05)
    m4.close(4001, "bye")
    time.sleep(0.05)
    stop.set()
    th.join()
    ops = [op for op, _ in m4.transport.sent()]
    want(ops.count(OP_CLOSE) == 1 and ops[-1] == OP_CLOSE, "nothing follows the Close, sent once")
    want(OP_TEXT in ops[:-1], "control: the broadcast's frames went out before it")

    # An error of the server's own ends the session with Close 1011, not a dead thread.
    h15 = Hub(now=lambda: 1000000)
    m15 = Member(h15, "general", Fake())
    deep = encode(OP_TEXT, b"[" * 100000 + b"]" * 100000, mask=key)
    want(not serve_bytes(h15, m15, Reader(), deep)
         and m15.transport.sent()[-1] == (OP_CLOSE, close_payload(1011, "The server met an error")),
         "JSON nested past the recursion limit: Close 1011")
    n15 = Member(h15, "general", Fake())
    want(serve_bytes(h15, n15, Reader(), encode(OP_TEXT, b'{"type":"join","name":"N"}', mask=key))
         and n15.transport.sent()[0][1]["type"] == "welcome", "control: a join goes on")
    want(not serve_bytes(h15, n15, Reader(), encode(OP_TEXT, b"x"))
         and n15.transport.sent()[-1][0] == OP_CLOSE,
         "control: the protocol's own error is still its Close (1002)")
    with tempfile.TemporaryDirectory() as d:
        root = os.path.realpath(d)
        with open(os.path.join(root, "chat.html"), "w") as f:
            f.write("<p>chat</p>")
        want(web_file(root, "/") == (200, os.path.join(root, "chat.html")),
             "control: / is the page")
        want(web_file(root, "/%00") == (400, None), "a path with a NUL is a 400, not a dead thread")
        want(web_file(root, "/../etc/passwd")[0] == 404, "a path out of the build is not served")

    # A socket's writes only queue. A peer that never reads blocks no caller, and is
    # shut down once its backlog is passed; a peer that reads gets every byte.
    left, right = socket.socketpair()
    left.settimeout(10)
    t5 = Transport(left, backlog=256 * 1024)
    began = time.monotonic()
    for _ in range(400):
        t5.write(b"x" * 4096)
    want(time.monotonic() - began < 1.0 and t5.dead,
         "a peer that does not read: no wait, then shut down")
    right.settimeout(2)
    try:
        while right.recv(65536):
            pass
        ended = True
    except OSError:
        ended = False
    want(ended, "its peer reads the end of the stream")
    left.close()
    right.close()
    left, right = socket.socketpair()
    t5 = Transport(left)
    t5.write(b"hello")
    t5.finish()
    right.settimeout(2)
    want(right.recv(16) == b"hello" and not t5.dead, "control: a peer that reads gets the bytes")
    left.close()
    right.close()
    print("SELFTEST PASS (%d checks)" % checks[0])
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="The chat server of RayClay Messenger (ex24_messenger).")
    parser.add_argument("--port", type=int, default=8789,
                        help="the port to listen on, on 127.0.0.1")
    parser.add_argument("--web", metavar="DIR", help="also serve a web build's files from DIR")
    parser.add_argument("--allow-origin", action="append", default=[], metavar="ORIGIN",
                        help="a page origin allowed to open a socket besides this server's own "
                             "('*' for any)")
    parser.add_argument("--then", choices=("drop", "close", "restart"),
                        help="do this to every socket --after seconds from its join")
    parser.add_argument("--after", type=float, default=5.0, metavar="SECONDS")
    parser.add_argument("--selftest", action="store_true",
                        help="check the framing and the rooms, bind nothing")
    args = parser.parse_args()
    if args.selftest:
        return selftest()

    server = Server(args.port, args.web, args.allow_origin, args.then, args.after)
    log("rooms at ws://127.0.0.1:%d/room/<%s>" % (server.port, "|".join(ROOMS)))
    if server.web:
        log("the web build is at http://127.0.0.1:%d/ (from %s)" % (server.port, server.web))
    threading.Thread(target=server.run, daemon=True).start()
    try:
        while True:
            time.sleep(3600)
    except KeyboardInterrupt:
        pass
    server.hub.close_all(1001, "The server is shutting down")
    time.sleep(0.5)                 # time for each Close's answer
    return 0


if __name__ == "__main__":
    sys.exit(main())
