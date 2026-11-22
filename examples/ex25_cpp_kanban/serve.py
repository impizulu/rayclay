#!/usr/bin/env python3
"""
serve.py - the backend the kanban board (ex25_cpp_kanban) syncs with, in Python 3's
standard library alone (no pip). Run one, press Connect in the Sync dialog of two
windows (or a window and the web build), and each shows the other's moves as they
happen.

    python3 examples/ex25_cpp_kanban/serve.py [--port 8787] [--host 127.0.0.1] [--web <dir>]
    python3 examples/ex25_cpp_kanban/serve.py --selftest

    GET    /api/board        200 {"version": N, "lists": [list, ...], "cards": [card, ...]}
    POST   /api/cards        201 card    from {"title", "list", ...}
    PATCH  /api/cards/<id>   200 card    from any of a card's fields
    DELETE /api/cards/<id>   204
    POST   /api/lists        201 list    from {"title", "pos"}
    PATCH  /api/lists/<id>   200 list    from {"title"} or {"pos"}
    DELETE /api/lists/<id>   204         its cards go with it
    GET    /api/events       text/event-stream: one event per change

A list is {"id", "title", "pos", "version"}. A card is {"id", "list", "pos", "title",
"description", "labels", "members", "due", "dueDone", "priority", "cover", "checklist",
"comments", "meta", "version"}: labels and members are bit masks, due an RFC 3339 time or
null, meta an object a client sends back untouched, and version the store's version at
its last change, so a client tells the echo of its own change from a newer one. Bad JSON
answers 400, a bad field 422, an unknown id 404, each with {"error": "..."}. Two knobs on
any request show each state on purpose: ?delay=<ms> holds the reply, ?fail=<status>
answers {"error": "the server answered <status> on purpose"}. Every response allows any
origin (CORS), the errors http.server writes itself included; --web <dir> also serves a
web build's files, so the page and the API share an origin.
"""
import argparse
import datetime
import http.client
import json
import mimetypes
import os
import queue
import re
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, unquote, urlsplit

TITLE_MAX = 250       # characters, as Python counts them
TEXT_MAX = 2000
RFC3339 = re.compile(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?(Z|[+-]\d{2}:\d{2})")
KEEP_ALIVE = 15.0     # seconds between comments on a quiet event stream
ONE_CARD = re.compile(r"/api/cards/([0-9]{1,9})")
ONE_LIST = re.compile(r"/api/lists/([0-9]{1,9})")

# The board the store starts with. The app's sample (board.h, atlas_board) is the same
# board, so the two read alike: keep them in step. A date is the app's day number here, a
# count from Dec 31 2025 written as if today were day 310 (Nov 6 2026), and an RFC 3339 time
# on the wire, moved as the app moves its own: day 310 is the day the server starts.
GREEN, YELLOW, ORANGE, RED, PURPLE, BLUE = 1, 2, 4, 8, 16, 32
MAYA, DIEGO, PRIYA, TOM, ANA, KENJI = 1, 2, 4, 8, 16, 32
LISTS = ("Backlog", "In progress", "In review", "Shipped")
# (list, title, labels, members, due, priority, cover, description, dueDone, checklist, comments)
SEED = (
    (1, "Single sign-on for enterprise workspaces", GREEN | PURPLE, PRIYA, 330, 2, -1,
     "SAML first, then OIDC. Acme and Northwind are waiting on this before they renew.", False,
     (("Pick the SAML library", True), ("Admin settings page", False), ("Test with Okta and Entra", False)), ()),
    (1, "Rate limit the public API", PURPLE, TOM, 328, 2, -1,
     "100 requests a minute per token, with a Retry-After header.", False, (), ()),
    (1, "Translate onboarding into Spanish and German", BLUE, ANA, 0, 1, -1, "", False, (),
     ((4, "The Spanish draft is back from the agency.", 306),)),
    (1, "Migration guide for v1 customers", BLUE, MAYA, 323, 3, -1,
     "Cover the API key change and the new webhook format.", False, (), ()),
    (1, "Dashboard is slow on accounts with 10k projects", RED, KENJI, 0, 3, -1,
     "p95 is 4.2 s. The project count query is doing a full scan.", False, (), ()),
    (2, "Redesign the pricing page", YELLOW | ORANGE, MAYA | ANA, 319, 3, 4,
     "Three plans, annual toggle, comparison table below the fold. Legal has signed off on the wording.", False,
     (("Wireframes", True), ("Copy review", True), ("Comparison table", True), ("Annual toggle", False),
      ("Launch checklist", False)),
     ((4, "Do we still show the Starter plan on mobile?", 307), (0, "Yes, but collapsed. Mock is in the design file.", 307),
      (3, "Stripe price ids are ready when you are.", 308))),
    (2, "Stop duplicate invoice emails", RED, DIEGO, 309, 4, -1,
     "Webhook retries send a second email. Make the send idempotent on the invoice id.", False, (),
     ((1, "Reproduced on staging.", 308), (2, "Support has 14 tickets on this.", 309))),
    (2, "Usage-based billing: metering service", GREEN | PURPLE, PRIYA | TOM, 322, 3, -1,
     "Events into Kafka, hourly rollups, one line on the invoice.", False,
     (("Event schema", True), ("Kafka topic", True), ("Hourly rollup job", False), ("Invoice line", False),
      ("Backfill", False), ("Alerting", False)), ()),
    (3, "Mobile nav drawer for small screens", YELLOW, KENJI, 311, 2, -1, "Waiting on one last review pass.", False, (), ()),
    (3, "Export the audit log to CSV", GREEN, TOM, 314, 1, -1,
     "Admins asked for this in the last three customer calls.", False, (), ()),
    (3, "Landing page copy, second pass", BLUE | ORANGE, ANA | MAYA, 0, 1, 0,
     "Shorter hero line. Keep the customer quotes.", False, (),
     ((0, "Much better. Two small notes in the doc.", 305),)),
    (4, "Dark mode for the dashboard", GREEN | ORANGE, KENJI, 306, 2, -1,
     "Follows the system setting, with an override in the profile.", True, (), ()),
    (4, "Retry failed Stripe webhooks", PURPLE, DIEGO, 304, 3, -1,
     "Five tries over an hour, then a page to the on-call.", True, (), ()),
    (4, "Press kit and launch blog post", BLUE, ANA, 303, 1, 2, "", True,
     (("Logos and screenshots", True), ("Founder quote", True), ("Post goes live", True)), ()),
)


SAMPLE_DAY = 310
STARTED_ON = datetime.date.today()


def time_of_day(day):
    """A sample day number as toISOString writes it, or None for no date."""
    if not day:
        return None
    return (STARTED_ON + datetime.timedelta(days=day - SAMPLE_DAY)).isoformat() + "T00:00:00.000Z"


# Some cards came from other tools. The app never reads this field: it sends it back.
META = {1: {"importedFrom": "jira", "key": "ATL-311"}, 5: {"importedFrom": "zendesk", "ticket": 48213}}


def event(version, name, data):
    """One server-sent event, the version as its id."""
    return "id: %d\nevent: %s\ndata: %s\n\n" % (version, name, json.dumps(data, ensure_ascii=False))


class Store:
    """The lists, the cards, the version and one queue per open event stream, behind
    one lock. The version rises by one on every create, update and delete."""

    def __init__(self):
        self.lock = threading.Lock()
        self.lists = {}        # id -> list
        self.cards = {}        # id -> card
        self.streams = []
        self.version = 0
        self.next_card = 1     # an id is never used twice, even after a delete
        self.next_list = 1
        for i, title in enumerate(LISTS):
            self.add_list({"title": title, "pos": 1024.0 * (i + 1)})
        for row in SEED:
            count = sum(1 for c in self.cards.values() if c["list"] == row[0])
            self.add_card({
                "list": row[0], "title": row[1], "labels": row[2], "members": row[3],
                "due": time_of_day(row[4]),
                "priority": row[5], "cover": row[6], "description": row[7], "dueDone": row[8],
                "pos": 1024.0 * (count + 1),
                "checklist": [{"text": t, "done": d} for t, d in row[9]],
                "comments": [{"author": a, "text": t, "at": time_of_day(d)} for a, t, d in row[10]],
                "meta": META.get(self.next_card),
            })

    def add_list(self, fields):
        self.version += 1
        lid, self.next_list = self.next_list, self.next_list + 1
        self.lists[lid] = dict(id=lid, title=fields["title"], pos=fields.get("pos", 1024.0 * lid),
                               version=self.version)
        return self.lists[lid]

    def add_card(self, fields):
        self.version += 1
        cid, self.next_card = self.next_card, self.next_card + 1
        card = dict(id=cid, list=fields["list"], pos=fields.get("pos", 1024.0 * cid),
                    title=fields["title"], description=fields.get("description", ""),
                    labels=fields.get("labels", 0), members=fields.get("members", 0),
                    due=fields.get("due"), dueDone=fields.get("dueDone", False),
                    priority=fields.get("priority", 0), cover=fields.get("cover", -1),
                    checklist=fields.get("checklist", []), comments=fields.get("comments", []),
                    meta=fields.get("meta"), version=self.version)
        self.cards[cid] = card
        return card

    def publish(self, name, op, data):
        """Tell every stream. The lock is held."""
        text = event(self.version, name, dict(op=op, version=self.version, **data))
        for q in self.streams:
            q.put(text)

    def board(self):
        with self.lock:
            return {"version": self.version,
                    "lists": [dict(v) for v in self.lists.values()],
                    "cards": [dict(c) for c in self.cards.values()]}

    def create_card(self, fields):
        with self.lock:
            if fields["list"] not in self.lists:
                return None
            card = self.add_card(fields)
            self.publish("card", "create", {"card": card})
            return dict(card)

    def update_card(self, cid, fields):
        with self.lock:
            card = self.cards.get(cid)
            if card is None or ("list" in fields and fields["list"] not in self.lists):
                return None
            self.version += 1
            card.update(fields, version=self.version)
            self.publish("card", "update", {"card": card})
            return dict(card)

    def delete_card(self, cid):
        with self.lock:
            if self.cards.pop(cid, None) is None:
                return False
            self.version += 1
            self.publish("card", "delete", {"id": cid})
            return True

    def create_list(self, fields):
        with self.lock:
            entry = self.add_list(fields)
            self.publish("list", "create", {"list": entry})
            return dict(entry)

    def update_list(self, lid, fields):
        with self.lock:
            entry = self.lists.get(lid)
            if entry is None:
                return None
            self.version += 1
            entry.update(fields, version=self.version)
            self.publish("list", "update", {"list": entry})
            return dict(entry)

    def delete_list(self, lid):
        with self.lock:
            if self.lists.pop(lid, None) is None:
                return False
            self.cards = {cid: c for cid, c in self.cards.items() if c["list"] != lid}
            self.version += 1
            self.publish("list", "delete", {"id": lid})
            return True

    def subscribe(self, last_id):
        """A queue for one stream. A client that names a Last-Event-ID other than the
        newest version has missed changes, or knew a server that has since restarted,
        so its first event is a resync: fetch the board again."""
        q = queue.Queue()
        with self.lock:
            if last_id is not None and last_id.strip() != str(self.version):
                q.put(event(self.version, "resync", {"version": self.version}))
            self.streams.append(q)
        return q

    def unsubscribe(self, q):
        with self.lock:
            self.streams.remove(q)


def check_text(body, key, limit, out):
    """A string field copied to out. The reason it is refused, or None."""
    value = body[key]
    if not isinstance(value, str) or len(value) > limit:
        return "%s is text of at most %d characters" % (key, limit)
    try:                # JSON can escape a lone surrogate, which UTF-8 cannot carry
        value.encode("utf-8")
    except UnicodeEncodeError:
        return "%s is not valid text" % key
    out[key] = value
    return None


def check_int(body, key, low, high, out):
    value = body[key]
    if type(value) is not int or not low <= value <= high:
        return "%s is a whole number from %d to %d" % (key, low, high)
    out[key] = value
    return None


def check_card(body, creating):
    """A create's or an update's fields: (fields, None), or (None, why not)."""
    if not isinstance(body, dict):
        return None, "the body is not a JSON object"
    if creating and ("title" not in body or "list" not in body):
        return None, "a card needs a title and a list"
    fields, why = {}, None
    for key, step in (("title", lambda k: check_text(body, k, TITLE_MAX, fields)),
                      ("description", lambda k: check_text(body, k, TEXT_MAX, fields)),
                      ("list", lambda k: check_int(body, k, 1, 2 ** 31 - 1, fields)),
                      ("labels", lambda k: check_int(body, k, 0, 63, fields)),
                      ("members", lambda k: check_int(body, k, 0, 63, fields)),
                      ("priority", lambda k: check_int(body, k, 0, 4, fields)),
                      ("cover", lambda k: check_int(body, k, -1, 5, fields))):
        if key in body:
            why = why or step(key)
    if "title" in fields and not fields["title"].strip():
        why = "a card needs a title"
    if "pos" in body:
        if type(body["pos"]) not in (int, float):
            why = why or "pos is a number"
        fields["pos"] = body["pos"]
    if "due" in body:
        if body["due"] is not None and not is_time(body["due"]):
            why = why or "due is an RFC 3339 time or null"
        fields["due"] = body["due"]
    if "meta" in body:
        if body["meta"] is not None and not isinstance(body["meta"], dict):
            why = why or "meta is an object or null"
        fields["meta"] = body["meta"]
    if "dueDone" in body:
        if type(body["dueDone"]) is not bool:
            why = why or "dueDone is true or false"
        fields["dueDone"] = body["dueDone"]
    for key, shape in (("checklist", check_item), ("comments", check_comment)):
        if key in body:
            items = body[key]
            if not isinstance(items, list) or len(items) > 200 or not all(shape(i) for i in items):
                why = why or "%s is a list of its items" % key
            fields[key] = items
    if why:
        return None, why
    return (fields, None) if fields else (None, "nothing to change")


def check_item(item):
    return isinstance(item, dict) and isinstance(item.get("text"), str) and type(item.get("done")) is bool


def check_comment(item):
    return (isinstance(item, dict) and isinstance(item.get("text"), str)
            and type(item.get("author")) is int and is_time(item.get("at")))


def is_time(value):
    return isinstance(value, str) and RFC3339.fullmatch(value) is not None


def check_list(body, creating):
    """A list's fields: (fields, None), or (None, why not)."""
    if not isinstance(body, dict):
        return None, "the body is not a JSON object"
    if creating and "title" not in body:
        return None, "a list needs a title"
    fields = {}
    if "title" in body:
        why = check_text(body, "title", TITLE_MAX, fields)
        if why or not fields["title"].strip():
            return None, why or "a list needs a title"
    if "pos" in body:
        if type(body["pos"]) not in (int, float):
            return None, "pos is a number"
        fields["pos"] = body["pos"]
    return (fields, None) if fields else (None, "nothing to change")


def knob(query, name):
    """An integer knob from the query string, or 0."""
    try:
        return int(query.get(name, ["0"])[0])
    except ValueError:
        return 0


class Handler(BaseHTTPRequestHandler):
    server_version = "RayClayKanban/1.0"

    def log_message(self, fmt, *args):
        if not self.server.quiet:
            super().log_message(fmt, *args)

    def end_headers(self):
        """Every response allows any origin, send_error's included."""
        self.send_header("Access-Control-Allow-Origin", "*")
        super().end_headers()

    def do_OPTIONS(self):
        """The preflight a browser sends before a PATCH, a DELETE, a JSON body or a
        Last-Event-ID header goes to another origin."""
        self.send_response(204)
        self.send_header("Access-Control-Allow-Methods", "GET, POST, PATCH, DELETE, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, Last-Event-ID")
        self.send_header("Access-Control-Max-Age", "600")
        # A page on a public origin may call this server on localhost: Chrome asks first.
        self.send_header("Access-Control-Allow-Private-Network", "true")
        self.end_headers()

    def route(self):
        """Every GET, POST, PATCH and DELETE: the knobs first, then the API, then the
        web build's files."""
        url, method, store = urlsplit(self.path), self.command, self.server.store
        query = parse_qs(url.query)
        delay, fail = knob(query, "delay"), knob(query, "fail")
        if delay > 0:
            time.sleep(min(delay, 30000) / 1000.0)
        if 400 <= fail <= 599:
            return self.reply(fail, {"error": "the server answered %d on purpose" % fail})
        path = url.path.rstrip("/")
        card, entry = ONE_CARD.fullmatch(path), ONE_LIST.fullmatch(path)
        if path == "/api/events" and method == "GET":
            return self.events(store)
        if path == "/api/board" and method == "GET":
            return self.reply(200, store.board())
        if path == "/api/cards" and method == "POST":
            return self.create(check_card, store.create_card, "no such list")
        if path == "/api/lists" and method == "POST":
            return self.create(check_list, store.create_list, "")
        if card and method == "PATCH":
            return self.update(check_card, store.update_card, int(card.group(1)), "card")
        if entry and method == "PATCH":
            return self.update(check_list, store.update_list, int(entry.group(1)), "list")
        if card and method == "DELETE":
            return self.remove(store.delete_card, int(card.group(1)), "card")
        if entry and method == "DELETE":
            return self.remove(store.delete_list, int(entry.group(1)), "list")
        if self.server.web and method == "GET":
            return self.static(unquote(url.path))
        return self.reply(404, {"error": "no route for %s %s" % (method, url.path)})

    do_GET = do_POST = do_PATCH = do_DELETE = route

    def create(self, check, make, missing):
        fields = self.fields(check, True)
        if fields:
            made = make(fields)
            if made is None:
                return self.reply(422, {"error": missing})
            self.reply(201, made)
        return None

    def update(self, check, change, rid, name):
        fields = self.fields(check, False)
        if not fields:
            return None
        changed = change(rid, fields)
        if changed is None:
            return self.reply(404, {"error": "no %s %d" % (name, rid)})
        return self.reply(200, changed)

    def remove(self, drop, rid, name):
        if drop(rid):
            return self.reply(204)
        return self.reply(404, {"error": "no %s %d" % (name, rid)})

    def fields(self, check, creating):
        """The body read as JSON and checked: its fields, or None once a 400 or a 422
        has been sent."""
        try:
            length = int(self.headers.get("Content-Length") or 0)
            body = json.loads(self.rfile.read(min(max(length, 0), 1 << 20)) or b"null")
        except ValueError:   # a bad length, bad JSON or bad UTF-8
            return self.reply(400, {"error": "the body is not JSON"})
        fields, why = check(body, creating)
        if why:
            return self.reply(422, {"error": why})
        return fields

    def reply(self, status, value=None, kind="application/json; charset=utf-8"):
        """Send a whole response and return None. A value that is not bytes is sent as
        JSON; a 204 has no body."""
        data = value if isinstance(value, bytes) else b"" if value is None else \
            json.dumps(value, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        if status != 204:
            self.send_header("Content-Type", kind)
            self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)
        return None

    def events(self, store):
        """The event stream: the reconnection time first, then one event per change and
        a comment every KEEP_ALIVE seconds, until the socket breaks."""
        q = store.subscribe(self.headers.get("Last-Event-ID"))
        try:
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(b"retry: 2000\n\n")
            self.wfile.flush()
            while True:
                try:
                    text = q.get(timeout=KEEP_ALIVE)
                except queue.Empty:
                    text = ": keep-alive\n\n"
                self.wfile.write(text.encode("utf-8"))
                self.wfile.flush()
        except OSError:      # the client went away: a broken pipe or a reset
            pass
        finally:
            store.unsubscribe(q)

    def static(self, path):
        """A file of the web build --web names, or 404."""
        root = self.server.web
        name = os.path.realpath(os.path.join(root, path.lstrip("/") or "index.html"))
        if not name.startswith(root + os.sep) or not os.path.isfile(name):
            return self.reply(404, {"error": "no file %s" % path})
        kind = "application/wasm" if name.endswith(".wasm") else mimetypes.guess_type(name)[0]
        with open(name, "rb") as f:
            return self.reply(200, f.read(), kind or "application/octet-stream")


class Server(ThreadingHTTPServer):
    daemon_threads = True       # an open event stream does not hold up Ctrl+C
    request_queue_size = 128    # the listen backlog
    quiet = False


def selftest():
    """The store, the checks and every route over a real socket on a port the system
    picks on loopback, each check beside a control that must come out the other way, so
    a check that cannot fail is caught. 0 pass, 1 fail."""
    failures = []

    def want(ok, what):
        print("  %s %s" % ("ok  " if ok else "FAIL", what))
        if not ok:
            failures.append(what)

    print("serve.py --selftest")

    # The store's seed reads as the app's sample does.
    store = Store()
    board = store.board()
    want([v["title"] for v in board["lists"]] == list(LISTS), "the four lists, in order")
    want(len(board["cards"]) == len(SEED) == 14, "the fourteen cards")
    want(board["version"] == len(LISTS) + len(SEED), "one version per seeded item")
    want(all(c["list"] in store.lists for c in board["cards"]), "every card sits in a list")

    # The checks.
    want(check_card({"title": "a", "list": 1}, True)[1] is None, "a create with a title and a list")
    want(check_card({"list": 1}, True)[1] is not None, "control: a create with no title")
    want(check_card({"title": "   ", "list": 1}, True)[1] is not None, "a blank title")
    want(check_card({"title": "x" * (TITLE_MAX + 1)}, False)[1] is not None, "a title past the limit")
    want(check_card({"labels": 63}, False)[1] is None, "the largest label mask")
    want(check_card({"labels": 64}, False)[1] is not None, "control: one past it")
    want(check_card({"due": "2026-11-12T00:00:00.000Z"}, False)[1] is None, "a due date")
    want(check_card({"due": None}, False)[1] is None, "control: no due date")
    want(check_card({"due": "Nov 12"}, False)[1] is not None, "a due that is not RFC 3339")
    want(time_of_day(SAMPLE_DAY) == STARTED_ON.isoformat() + "T00:00:00.000Z", "day 310 is today")
    want(time_of_day(SAMPLE_DAY + 1) != time_of_day(SAMPLE_DAY), "control: day 311 is another")
    want(check_card({"meta": {"key": "ATL-311"}}, False)[1] is None, "a meta object")
    want(check_card({"meta": 7}, False)[1] is not None, "control: a meta that is no object")
    want(check_card({"checklist": [{"text": "a", "done": False}]}, False)[1] is None, "a checklist")
    want(check_card({"checklist": [{"text": "a"}]}, False)[1] is not None, "control: an item with no done")
    want(check_card({}, False)[1] is not None, "nothing to change")
    want(check_list({"title": "Later"}, True)[1] is None, "a list with a title")
    want(check_list({}, True)[1] is not None, "control: a list with no title")

    # The routes, over a socket.
    server = Server(("127.0.0.1", 0), Handler)
    server.store, server.web, server.quiet = Store(), None, True
    port = server.server_address[1]
    threading.Thread(target=server.serve_forever, daemon=True).start()

    def call(method, path, body=None):
        conn = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
        data = None if body is None else json.dumps(body)
        conn.request(method, path, data, {"Content-Type": "application/json"})
        answer = conn.getresponse()
        text = answer.read()
        conn.close()
        return answer.status, json.loads(text) if text else None

    # The stream opens first, so it sees every change below, in order.
    stream = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
    stream.request("GET", "/api/events", headers={"Last-Event-ID": str(call("GET", "/api/board")[1]["version"])})
    answer = stream.getresponse()
    want(answer.status == 200 and answer.getheader("Content-Type").startswith("text/event-stream"),
         "the event stream opens")

    def next_event():
        lines = {}
        while True:
            line = answer.fp.readline().decode("utf-8").rstrip("\r\n")
            if line.startswith("retry:") or line.startswith(":"):
                continue
            if not line:
                if lines:
                    return lines
                continue
            key, _, value = line.partition(": ")
            lines[key] = value

    status, board = call("GET", "/api/board")
    want(status == 200 and len(board["cards"]) == 14, "GET the board")
    version = board["version"]

    status, card = call("POST", "/api/cards", {"title": "Plan the launch party", "list": 1, "pos": 9000.0})
    want(status == 201 and card["version"] == version + 1 and card["description"] == "", "POST a card")
    got = next_event()
    want(got["event"] == "card" and json.loads(got["data"])["op"] == "create" and got["id"] == str(card["version"]),
         "its create event, the version as its id")

    status, moved = call("PATCH", "/api/cards/%d" % card["id"], {"list": 2, "pos": 512.0, "labels": 5})
    want(status == 200 and moved["list"] == 2 and moved["labels"] == 5 and moved["title"] == card["title"],
         "PATCH moves a card and keeps its other fields")
    want(json.loads(next_event()["data"])["card"]["list"] == 2, "its update event")
    want(call("GET", "/api/board")[1]["cards"][0]["meta"] == {"importedFrom": "jira", "key": "ATL-311"},
         "the first card keeps its meta")
    want(call("PATCH", "/api/cards/%d" % card["id"], {"list": 9999})[0] == 404, "a move to a list that is not there")
    want(call("PATCH", "/api/cards/999999", {"title": "x"})[0] == 404, "control: a card that is not there")
    want(call("POST", "/api/cards", {"title": "", "list": 1})[0] == 422, "a blank title answers 422")
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
    conn.request("POST", "/api/cards", "{not json", {"Content-Type": "application/json"})
    want(conn.getresponse().status == 400, "bad JSON answers 400")
    conn.close()

    want(call("DELETE", "/api/cards/%d" % card["id"])[0] == 204, "DELETE a card")
    want(json.loads(next_event()["data"])["op"] == "delete", "its delete event")
    want(call("DELETE", "/api/cards/%d" % card["id"])[0] == 404, "control: it is gone")

    status, entry = call("POST", "/api/lists", {"title": "On hold", "pos": 9000.0})
    want(status == 201 and entry["title"] == "On hold", "POST a list")
    want(next_event()["event"] == "list", "its event")
    status, renamed = call("PATCH", "/api/lists/%d" % entry["id"], {"title": "Parked"})
    want(status == 200 and renamed["title"] == "Parked", "PATCH renames a list")
    next_event()
    status, card = call("POST", "/api/cards", {"title": "Goes with its list", "list": entry["id"]})
    next_event()
    want(call("DELETE", "/api/lists/%d" % entry["id"])[0] == 204, "DELETE a list")
    next_event()
    want(call("PATCH", "/api/cards/%d" % card["id"], {"title": "x"})[0] == 404, "its cards went with it")

    want(call("GET", "/api/board?fail=503")[0] == 503, "?fail= forces a status")
    began = time.monotonic()
    want(call("GET", "/api/board?delay=200")[0] == 200 and time.monotonic() - began >= 0.2, "?delay= holds the reply")
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
    conn.request("OPTIONS", "/api/board")
    preflight = conn.getresponse()
    want(preflight.status == 204 and preflight.getheader("Access-Control-Allow-Origin") == "*", "the CORS preflight")
    conn.close()

    # A stream that names an old version is told to fetch again.
    late = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
    late.request("GET", "/api/events", headers={"Last-Event-ID": "1"})
    answer = late.getresponse()
    want(next_event()["event"] == "resync", "a stale Last-Event-ID gets a resync")

    stream.close()
    late.close()
    server.shutdown()
    server.server_close()
    print("PASS" if not failures else "FAIL: %d check(s)" % len(failures))
    return 1 if failures else 0


def main():
    parser = argparse.ArgumentParser(description="The backend of the kanban board (ex25_cpp_kanban).")
    parser.add_argument("--host", default="127.0.0.1", help="the address to listen on")
    parser.add_argument("--port", type=int, default=8787, help="the port to listen on")
    parser.add_argument("--web", metavar="DIR", help="also serve a web build's files from DIR")
    parser.add_argument("--selftest", action="store_true", help="check the store and the routes, then exit")
    args = parser.parse_args()
    if args.selftest:
        return selftest()

    server = Server((args.host, args.port), Handler)
    server.store = Store()
    server.web = os.path.realpath(args.web) if args.web else None
    base = "http://%s:%d" % (args.host, server.server_address[1])
    print("Kanban: the API is at %s/api/board. Put %s in the app's Sync dialog." % (base, base), flush=True)
    if server.web:
        page = "kanban.html" if os.path.isfile(os.path.join(server.web, "kanban.html")) else ""
        print("Kanban: the web build is at %s/%s (from %s)" % (base, page, server.web), flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
