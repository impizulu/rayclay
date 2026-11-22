#!/usr/bin/env python3
"""
serve.py - the update server RayClay Preferences (ex13_user_preferences) checks on the
desktop, in Python 3's standard library alone (3.9 or later, no pip).

    python3 examples/ex13_user_preferences/serve.py [--build PATH] [--port 8788] [--offer 1.1.0]
                                                 [--notes TEXT] [--publisher PATH]
    python3 examples/ex13_user_preferences/serve.py --selftest [--publisher PATH]

It signs with rc-update-publish.py, the signer that ships beside it, run as the
command line a developer types (keygen, sign). At start it makes a THROWAWAY
Ed25519 key pair in a new temporary directory outside this tree and prints the
public key: paste it into the Public key field of the app's Developer options.
EXAMPLE ONLY: the pair is deleted when serve.py stops, and the next start makes
another, so a key pasted before a restart no longer verifies. A shipped app
compiles its own public key in (.publicKey = APP_UPDATE_KEY, src/app/update.h)
and keeps the secret key outside every repository.

    GET  /manifest.json          the envelope: {"manifest", "signature", "keyId"}
    GET  /download/<name>        what an entry names: the new build, or a page
                                 standing in for a release's download page
    POST /publish?version=<V>    publish V (SemVer) as the newest release, signed
                                 with the same key: the app's next check sees it
                                 (&notes=<text> sets its notes)

INSTALL. With --build naming the example as built (rayclay_ex13_user_preferences,
.exe on Windows), its own platform key gets an "exe" entry: a copy of that
program with the version tag in it (APP_VERSION_TAG, src/app/update.h)
rewritten to the offered version, its size and sha256 filled by the signer.
With the key pasted, the app's Install downloads it and puts it in its own
place, and Restart now starts it, as that version. EXAMPLE ONLY: a real
release's new build is built with its new version. Every other desktop key,
and every key without --build, gets a "page" entry, which the app opens in the
browser: a signed "exe" would install whatever file it names. Knobs
on the manifest's query show each state on purpose (the Simulate list of the app's Developer options
sends them): ?offer=<V> offers V instead (the app's own version reads CURRENT,
a lower one is a downgrade), ?required=1 sets minimumVersion to the offer,
?tamper=1 changes the document after signing (SIGNATURE with a key, an offer
without one), ?app=<id> signs another app's manifest (MANIFEST: the app names
itself with .app),
?delay=<ms> holds the answer (at most 30 s) and ?fail=<status> answers that
status. The server listens on 127.0.0.1 only: the app accepts plain http to
loopback alone. A refused offer or publish answers 422 with {"error": ...}.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit

HERE = os.path.dirname(os.path.abspath(__file__))
PUBLISHER = os.path.join(HERE, "rc-update-publish.py")

APP = "org.rayclay.demo.ex13_user_preferences"   # the app's APP_ID (src/app/update.h)
APP_VERSION = "1.0.0"                          # the app's APP_VERSION
CHANNEL = "stable"
DESKTOP = ("macos-arm64", "macos-x86_64", "windows-x86_64", "windows-arm64", "linux-x86_64",
           "linux-arm64")
LIFETIME = 30 * 86400          # seconds a signed manifest stays good ("expires")
NOTES = "Faster start-up, and a clearer message when the network is down."
DOWNLOAD = re.compile(r"/download/((?:rayclay-preferences-[0-9A-Za-z.+-]{1,64}\.html)|"
                      r"(?:RayClay-Preferences-[0-9A-Za-z.+-]{1,64}(?:\.exe)?))")
VERSION_TAG = b"ex13-version:"   # APP_VERSION_TAG's head, src/app/update.h
TAG_SLOT = 13 + 32               # the whole array: the head, then the version and its zeros
PUBLIC_KEY = re.compile(r"^publicKey:\s*([A-Za-z0-9+/]{43}=)\s*$", re.M)
KEY_ID = re.compile(r"^keyId:\s*([0-9a-f]{8})\s*$", re.M)


class Refused(Exception):
    """What the signer refused, in its own words."""


class Publisher:
    """rc-update-publish.py, run as a developer runs it."""

    def __init__(self, path):
        self.path = path

    def run(self, *args):
        done = subprocess.run([sys.executable, self.path] + list(args), capture_output=True,
                              text=True, encoding="utf-8")
        if done.returncode != 0:
            raise Refused(done.stderr.strip() or "rc-update-publish.py %s exited %d"
                          % (args[0], done.returncode))
        return done.stdout

    def keygen(self, key_path):
        """(public key, keyId) of a new secret key file at key_path."""
        out = self.run("keygen", key_path)
        pub, kid = PUBLIC_KEY.search(out), KEY_ID.search(out)
        if not pub or not kid:
            raise Refused("rc-update-publish.py keygen printed no publicKey and keyId:\n" + out)
        return pub.group(1), kid.group(1)

    def sign(self, draft_path, key_path, artifacts):
        """The envelope's text: sign <draft> --key <file> --artifact <platform>=<file>..."""
        args = ["sign", draft_path, "--key", key_path]
        for platform, path in artifacts:
            args += ["--artifact", "%s=%s" % (platform, path)]
        return self.run(*args)

    def verify(self, envelope_path, public_key):
        """True when the envelope verifies under public_key, as the app checks it."""
        try:
            self.run("verify", envelope_path, "--public-key", public_key)
            return True
        except Refused:
            return False


def rfc3339(seconds):
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(seconds))


def platforms(data):
    """The platform keys a program's own header names (ELF, PE, Mach-O or a
    universal Mach-O), which are the keys its app reads; Refused otherwise."""
    def u16(at, order="little"):
        return int.from_bytes(data[at:at + 2], order)

    def u32(at, order="little"):
        return int.from_bytes(data[at:at + 4], order)

    elf = {62: "linux-x86_64", 183: "linux-arm64"}
    pe = {0x8664: "windows-x86_64", 0xAA64: "windows-arm64"}
    mach = {0x01000007: "macos-x86_64", 0x0100000C: "macos-arm64"}
    keys = []
    if data[:4] == b"\x7fELF" and len(data) >= 20:
        keys = [elf.get(u16(18, "little" if data[5] == 1 else "big"))]
    elif data[:2] == b"MZ" and len(data) >= 64:
        at = u32(0x3C)
        keys = [pe.get(u16(at + 4))] if data[at:at + 4] == b"PE\0\0" else []
    elif data[:4] == b"\xcf\xfa\xed\xfe":
        keys = [mach.get(u32(4))]
    elif data[:4] == b"\xca\xfe\xba\xbe":
        keys = [mach.get(u32(8 + 20 * i, "big")) for i in range(min(u32(4, "big"), 8))]
    keys = sorted(set(k for k in keys if k))
    if not keys:
        raise Refused("--build names no x86-64 or arm64 program for Linux, Windows or macOS")
    return keys


def relabel(data, version):
    """A copy of the program whose version tag reads version: every slot of
    TAG_SLOT bytes that holds the tag, APP_VERSION and then zeros (one, or one
    per slice of a universal Mach-O) is rewritten. Refused when there is none."""
    want = VERSION_TAG + APP_VERSION.encode("ascii")
    want += bytes(TAG_SLOT - len(want))
    new = VERSION_TAG + version.encode("ascii")
    if len(new) >= TAG_SLOT:
        raise Refused("--build writes the offered version into a %d-byte tag: %s is too long"
                      % (TAG_SLOT - len(VERSION_TAG) - 1, version))
    new += bytes(TAG_SLOT - len(new))
    out, at, found = bytearray(data), 0, 0
    while True:
        at = out.find(want, at)
        if at < 0:
            break
        out[at:at + TAG_SLOT] = new
        at += TAG_SLOT
        found += 1
    if not found:
        raise Refused("--build names a program without RayClay Preferences' version tag for %s "
                      "(APP_VERSION_TAG): build this example, then pass its program" % APP_VERSION)
    return bytes(out)


class Releases:
    """The throwaway key, the newest release and every envelope signed so far,
    behind one lock. Each knob's envelope is signed when first asked for, and again
    before it expires."""

    def __init__(self, publisher, base, version, notes, build=None):
        self.publisher, self.base = publisher, base
        self.lock = threading.Lock()
        self.signed = {}
        self.version, self.notes = version, notes
        # The program --build names, read once: its keys install, every other
        # key's entry is a page.
        self.program, self.installs = None, []
        if build:
            try:
                with open(build, "rb") as f:
                    self.program = f.read()
            except OSError as e:
                raise Refused("--build cannot be read: %s" % e)
            self.installs = platforms(self.program)
            relabel(self.program, version)       # a program without the tag is refused now
            self.exe = ".exe" if self.program[:2] == b"MZ" else ""
        self.dir = tempfile.mkdtemp(prefix="rayclay-ex13-")
        self.key = os.path.join(self.dir, "example-only.key")
        try:
            if os.path.realpath(self.dir).startswith(os.path.realpath(HERE) + os.sep):
                raise Refused("the temporary directory %s is inside this example" % self.dir)
            self.public_key, self.key_id = publisher.keygen(self.key)
            self.envelope()      # the newest release, signed now: a refusal shows at start
        except Refused:
            self.close()
            raise

    def close(self):
        """The key pair and everything signed with it go."""
        shutil.rmtree(self.dir, ignore_errors=True)

    def page(self, version):
        """(name, path) of the page that stands in for version's download page."""
        name = "rayclay-preferences-%s.html" % version
        path = os.path.join(self.dir, name)
        if not os.path.exists(path):
            with open(path, "w", encoding="utf-8", newline="\n") as f:
                f.write("<!doctype html><meta charset=\"utf-8\"><title>RayClay Preferences %s</title>\n"
                        "<h1>RayClay Preferences %s</h1>\n<p>This page stands in for a release's download "
                        "page: a real one links the build for each platform.</p>\n" % (version, version))
        return name, path

    def build(self, version):
        """(name, path) of the program --build named, relabeled as version. A
        Mach-O copy is signed again (ad hoc): a changed byte voids the linker's
        signature, and macOS would kill the new build at its start."""
        name = "RayClay-Preferences-%s%s" % (version, self.exe)
        path = os.path.join(self.dir, name)
        if not os.path.exists(path):
            with open(path, "wb") as f:
                f.write(relabel(self.program, version))
            os.chmod(path, 0o755)
            if self.installs[0].startswith("macos-"):
                done = subprocess.run(["codesign", "--force", "--sign", "-", path],
                                      capture_output=True, text=True)
                if done.returncode != 0:
                    os.remove(path)
                    raise Refused("codesign could not sign the relabeled copy: %s"
                                  % done.stderr.strip())
        return name, path

    def envelope(self, offer=None, required=False, app=APP):
        """The signed envelope's text for one combination of knobs."""
        with self.lock:
            version = offer or self.version
            cache = (version, self.notes, required, app)
            held = self.signed.get(cache)
            # Its "expires" is fixed when it is signed, so one held past half of LIFETIME is
            # signed again: a server left running for weeks never answers with an expired one.
            if held is None or time.time() - held[0] > LIFETIME / 2:
                page = "%s/download/%s" % (self.base, self.page(version)[0])
                entries = {k: {"kind": "page", "url": page} for k in DESKTOP}
                signs = []
                if self.installs:
                    name, path = self.build(version)
                    for k in self.installs:
                        entries[k] = {"kind": "exe", "url": "%s/download/%s" % (self.base, name)}
                        signs.append((k, path))
                doc = {"schema": 1, "app": app, "channel": CHANNEL, "version": version,
                       "expires": rfc3339(time.time() + LIFETIME), "notes": self.notes,
                       "artifacts": entries}
                if required:
                    doc["minimumVersion"] = version
                draft = os.path.join(self.dir, "draft.json")
                with open(draft, "w", encoding="utf-8") as f:
                    json.dump(doc, f)
                self.signed[cache] = (time.time(), self.publisher.sign(draft, self.key, signs))
            return self.signed[cache][1]

    def publish(self, version, notes):
        """Make version the newest release, signed now; Refused names why not."""
        with self.lock:
            old = (self.version, self.notes)
            self.version, self.notes = version, notes or self.notes
        try:
            self.envelope()
        except Refused:
            with self.lock:
                self.version, self.notes = old
            raise

    def file(self, name):
        """The bytes of a file page() or build() wrote, or None."""
        path = os.path.join(self.dir, name)
        if not os.path.isfile(path):
            return None
        with open(path, "rb") as f:
            return f.read()


def tampered(text):
    """The envelope with its document changed after signing: the signature and
    keyId stay, so a check with the key fails SIGNATURE."""
    env = json.loads(text)
    doc = json.loads(env["manifest"])
    doc["notes"] = "Changed after signing. " + doc.get("notes", "")
    env["manifest"] = json.dumps(doc)
    return json.dumps(env) + "\n"


def knob(query, name, default=""):
    return query.get(name, [default])[0]


def number(query, name):
    try:
        return int(knob(query, name, "0"))
    except ValueError:
        return 0


class Handler(BaseHTTPRequestHandler):
    server_version = "RayClayPreferences/1.0"

    def do_GET(self):
        url = urlsplit(self.path)
        query = parse_qs(url.query)
        delay, fail = number(query, "delay"), number(query, "fail")
        if delay > 0:
            time.sleep(min(delay, 30000) / 1000.0)
        if 400 <= fail <= 599:
            return self.reply(fail, {"error": "forced %d" % fail})
        if url.path == "/manifest.json":
            return self.manifest(query)
        one = DOWNLOAD.fullmatch(url.path)
        data = self.server.releases.file(one.group(1)) if one else None
        if data is not None and one.group(1).endswith(".html"):
            return self.reply(200, data, "text/html; charset=utf-8")
        if data is not None:
            return self.reply(200, data, "application/octet-stream", one.group(1))
        return self.reply(404, {"error": "no route for GET %s" % url.path})

    def do_POST(self):
        url = urlsplit(self.path)
        query = parse_qs(url.query)
        if url.path != "/publish":
            return self.reply(404, {"error": "no route for POST %s" % url.path})
        try:
            self.server.releases.publish(knob(query, "version"), knob(query, "notes"))
        except Refused as e:
            return self.reply(422, {"error": str(e)})
        print("RayClay Preferences: published %s" % self.server.releases.version, flush=True)
        return self.reply(200, {"version": self.server.releases.version})

    def manifest(self, query):
        releases = self.server.releases
        try:
            text = releases.envelope(knob(query, "offer") or None, knob(query, "required") == "1",
                                     knob(query, "app") or APP)
        except Refused as e:
            return self.reply(422, {"error": str(e)})
        if knob(query, "tamper") == "1":
            text = tampered(text)
        return self.reply(200, text.encode("utf-8"), "application/json; charset=utf-8")

    def reply(self, status, value, kind="application/json; charset=utf-8", attachment=None):
        """Send a whole response and return None. A value that is not bytes is
        sent as JSON. No answer is cached: each check reads the newest."""
        data = value if isinstance(value, bytes) else json.dumps(value).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        if attachment:
            self.send_header("Content-Disposition", 'attachment; filename="%s"' % attachment)
        self.end_headers()
        self.wfile.write(data)
        return None


def missing(path):
    sys.stderr.write(
        "serve.py: the signer rc-update-publish.py is not at %s.\n"
        "  It ships beside this example, in examples/ex13_user_preferences/.\n"
        "  Pass its path with --publisher.\n" % path)


def selftest(publisher_path):
    """A key, signing and the knobs through the real signer, verified as the app
    verifies. Binds no port. 0 pass, 1 fail, 77 skip."""
    failures = []

    def check(ok, what):
        print("  %s %s" % ("ok  " if ok else "FAIL", what))
        if not ok:
            failures.append(what)

    print("serve.py --selftest")
    if not os.path.isfile(publisher_path):
        missing(publisher_path)
        print("SKIP: the signing checks need rc-update-publish.py")
        return 1 if failures else 77

    try:
        releases = Releases(Publisher(publisher_path), "http://127.0.0.1:8788", "1.1.0", NOTES)
    except Refused as e:
        print("FAIL: the signer refused the newest release: %s" % e)
        return 1
    try:
        publisher = releases.publisher
        check(not os.path.realpath(releases.dir).startswith(os.path.realpath(HERE) + os.sep),
              "the throwaway key lives outside this example")
        path = os.path.join(releases.dir, "check.json")

        def verifies(text):
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)
            return publisher.verify(path, releases.public_key)

        good = releases.envelope()
        signed = json.loads(json.loads(good)["manifest"])
        check(verifies(good), "the newest release verifies under the printed key")
        check(json.loads(good)["keyId"] == releases.key_id, "its keyId is the one keygen printed")
        name = releases.page("1.1.0")[0]
        entries = signed["artifacts"]
        check(set(entries) == set(DESKTOP) and all(
            e == {"kind": "page", "url": "http://127.0.0.1:8788/download/" + name}
            for e in entries.values()), "without --build every desktop entry is a page: nothing installs")
        check(not verifies(tampered(good)), "CONTROL: a document changed after signing fails verify")
        doc = json.loads(json.loads(releases.envelope(offer=APP_VERSION))["manifest"])
        check(doc["version"] == APP_VERSION, "?offer= offers the app's own version (CURRENT)")
        doc = json.loads(json.loads(releases.envelope(required=True))["manifest"])
        check(doc.get("minimumVersion") == doc["version"], "?required=1 sets minimumVersion to the offer")
        doc = json.loads(json.loads(releases.envelope(app="org.example.other"))["manifest"])
        check(doc["app"] == "org.example.other", "?app= signs another app's manifest")
        try:
            releases.publish("not-a-version", "")
            refused = False
        except Refused:
            refused = True
        check(refused and releases.version == "1.1.0", "publish refuses a version that is not SemVer")
        releases.publish("1.2.0", "")
        doc = json.loads(json.loads(releases.envelope())["manifest"])
        check(doc["version"] == "1.2.0", "publish makes 1.2.0 the newest release")
    finally:
        releases.close()
    check(not os.path.exists(releases.dir), "closing deletes the key pair")
    selftest_build(publisher_path, check)
    print("PASS" if not failures else "FAIL: %d check(s)" % len(failures))
    return 1 if failures else 0


def selftest_build(publisher_path, check):
    """--build on a stand-in program: an ELF header and the version tag, which
    is all serve.py reads of a build."""
    def program(head, tag=True):
        body = bytearray(head) + bytes(64 - len(head)) + b"code"
        if tag:
            body += VERSION_TAG + APP_VERSION.encode("ascii")
            body += bytes(TAG_SLOT - len(VERSION_TAG) - len(APP_VERSION))
        return bytes(body + b"more code")

    elf = b"\x7fELF\x02\x01" + bytes(12) + (62).to_bytes(2, "little")
    pe = bytearray(b"MZ" + bytes(62))
    pe[0x3C:0x40] = (64).to_bytes(4, "little")
    pe = bytes(pe) + b"PE\0\0" + (0xAA64).to_bytes(2, "little")
    fat = b"\xca\xfe\xba\xbe" + (2).to_bytes(4, "big") + (0x01000007).to_bytes(4, "big") + bytes(16) \
        + (0x0100000C).to_bytes(4, "big")
    check(platforms(program(elf)) == ["linux-x86_64"], "an x86-64 ELF program is linux-x86_64")
    check(platforms(pe + bytes(8)) == ["windows-arm64"], "an arm64 PE program is windows-arm64")
    check(platforms(fat + bytes(32)) == ["macos-arm64", "macos-x86_64"],
          "a universal Mach-O program is both macOS keys")
    new = relabel(program(elf), "1.1.0")
    check(len(new) == len(program(elf)) and (VERSION_TAG + b"1.1.0\0") in new and
          (VERSION_TAG + APP_VERSION.encode("ascii")) not in new,
          "relabel writes the offered version over the tag, at the same length")
    tmp = tempfile.mkdtemp(prefix="rayclay-ex13-build-")
    try:
        built, bare = os.path.join(tmp, "app"), os.path.join(tmp, "untagged")
        with open(built, "wb") as f:
            f.write(program(elf))
        with open(bare, "wb") as f:
            f.write(program(elf, tag=False))
        try:
            Releases(Publisher(publisher_path), "http://127.0.0.1:8788", "1.1.0", NOTES, bare).close()
            refused = False
        except Refused:
            refused = True
        check(refused, "CONTROL: --build refuses a program without the version tag")
        releases = Releases(Publisher(publisher_path), "http://127.0.0.1:8788", "1.1.0", NOTES, built)
        try:
            entries = json.loads(json.loads(releases.envelope())["manifest"])["artifacts"]
            name, path = releases.build("1.1.0")
            with open(path, "rb") as f:
                body = f.read()
            mine = entries.get("linux-x86_64", {})
            check(body == new and mine == {"kind": "exe", "url": "http://127.0.0.1:8788/download/" + name,
                                           "size": len(body), "sha256": hashlib.sha256(body).hexdigest()},
                  "--build: linux-x86_64 installs the relabeled copy, its size and sha256 signed")
            check(all(e["kind"] == "page" for k, e in entries.items() if k != "linux-x86_64"),
                  "--build: every other desktop key is a page")
            check(releases.file(name) == body, "GET /download/ serves the relabeled copy")
        finally:
            releases.close()
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


class Server(ThreadingHTTPServer):
    daemon_threads = True


def main():
    parser = argparse.ArgumentParser(description="The update server of RayClay Preferences "
                                                 "(ex13_user_preferences).")
    parser.add_argument("--port", type=int, default=8788, help="the port to listen on")
    parser.add_argument("--offer", default="1.1.0", help="the version the manifest offers")
    parser.add_argument("--build", metavar="PATH",
                        help="this example as built: its platform's entry installs a copy of it")
    parser.add_argument("--notes", default=NOTES, help="the offer's notes")
    parser.add_argument("--publisher", default=PUBLISHER, metavar="PATH",
                        help="rc-update-publish.py (default: the one beside this file)")
    parser.add_argument("--selftest", action="store_true", help="run the checks and exit")
    args = parser.parse_args()

    if args.selftest:
        return selftest(args.publisher)
    if not os.path.isfile(args.publisher):
        missing(args.publisher)
        return 2
    server = Server(("127.0.0.1", args.port), Handler)
    base = "http://127.0.0.1:%d" % server.server_address[1]
    try:
        server.releases = Releases(Publisher(args.publisher), base, args.offer, args.notes, args.build)
    except Refused as e:
        server.server_close()
        sys.stderr.write("serve.py: %s\n" % e)
        return 1
    releases = server.releases
    print("RayClay Preferences: the manifest is at %s/manifest.json, offering %s" % (base, args.offer),
          flush=True)
    if releases.installs:
        print("RayClay Preferences: %s installs a copy of %s, relabeled as each offer (EXAMPLE ONLY)"
              % (", ".join(releases.installs), args.build), flush=True)
    else:
        print("RayClay Preferences: every entry is a page, which the app opens in the browser; pass\n"
              "  --build <the example as built> to install a copy of it instead", flush=True)
    print("RayClay Preferences: the EXAMPLE-ONLY public key, for the Public key field of the app's Developer options (keyId %s):\n\n"
          "    %s\n\n"
          "  It was made for this run in %s and goes when serve.py stops.\n"
          "  Publish a newer release while the app runs, then press Check for updates:\n"
          "    curl -X POST '%s/publish?version=1.2.0'"
          % (releases.key_id, releases.public_key, releases.dir, base), flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        releases.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
