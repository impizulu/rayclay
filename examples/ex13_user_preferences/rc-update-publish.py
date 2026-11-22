#!/usr/bin/env python3
"""rc-update-publish.py - make, sign and check the update manifest rcUpdateCheck reads.

RayClay's update check (rcUpdateCheck) fetches ONE JSON file the developer hosts
beside the build, an ENVELOPE:

    {"manifest": "<the document as one JSON string>",
     "signature": "<base64 Ed25519 over that string's UTF-8 bytes>",
     "keyId": "<8 hex: the first 4 bytes of SHA-256 over the public key>"}

and verifies it with the public key(s) compiled into the app (.publicKey). This
tool is the developer's half: it makes the key pair, fills each artifact's size
and sha256, signs, checks an envelope before it is uploaded, and prints the order
of a GitHub Releases upload. It runs nothing on the network, signs no code
(codesign, notarytool, signtool and gh stay the developer's), and never pushes.

    rc-update-publish.py keygen <secret-key-file>
    rc-update-publish.py sign <draft.json> --key <secret-key-file>
                         [--artifact <platform>=<file>]... [--previous <envelope.json>]
                         [-o <envelope.json>]
    rc-update-publish.py verify <envelope.json> (--public-key <base64> | --key <file>)...
    rc-update-publish.py --plan [--repo OWNER/REPO] [--tag TAG] [--artifact <platform>=<file>]...
    rc-update-publish.py --selftest [<envelope.json>]

THE DRAFT is the document, as JSON: schema (1), app, channel, version (SemVer),
build (optional, a whole number: when the running app and the manifest both have
one it is the only number compared), expires (RFC 3339: a replayed manifest stops
working then), minimumVersion and notes (optional), and artifacts, one entry per
platform key (macos-arm64, macos-x86_64, windows-x86_64, windows-arm64,
linux-x86_64, linux-arm64, android, ios, web) with its kind (page, exe,
installer, app-zip, reload) and url: https, or http to loopback for a local
test, written as rcOpenUrl takes it (printable ASCII, no credentials, and
none of " < > \\ ^ ` { } |). sign refuses these mistakes, as the app does,
so they show here, not in the field: a malformed field, NaN or a lone
surrogate the app's JSON reader rejects, an installer entry whose url's path
(before any ? or #) ends in .msi (the kind takes an .exe: Windows Installer
reads a .msi after the app's hold on it has ended, so ship it inside a
bootstrapper .exe), and an envelope over the 64 KiB the app fetches. It is
stricter in three places, on purpose. A kind this release does not know is
refused under every platform key, where the app refuses it only under its
own, so a typo shows here; a kind a later release adds is signed with that
release's tool. An exe, installer or app-zip entry needs its size (above 0)
and sha256, which --artifact fills, under every platform key, where the app
refuses one without them only on a host that installs that kind; page and
reload need neither. And an installer naming a .msi is refused under every
key, where the app refuses it under the key it reads on Windows, whatever
that is: an app's .platform can name any key (windows-x86_64-beta,
win-beta), and only Windows installs the kind, so the tool cannot tell a
Windows key by its name.

THE OLD KEY. Apps built with an older RayClay read the artifact table under the
older spelling "artefacts" alone, a key no check here reads. sign writes "artifacts"
alone and refuses a draft that still carries "artefacts"; verify refuses an
envelope whose "artefacts" is not the same table as its "artifacts". Either
way, those apps would act on a table nothing checked. Envelopes signed by
earlier releases of this tool carry the two keys equal and still verify.

THE SECRET KEY FILE is JSON, written by keygen with mode 600 and never over an
existing file: {"kind": "rc-update-secret-key", "seed": "<base64 of the 32-byte
Ed25519 seed>", "publicKey": ..., "keyId": ...}. Only "seed" is read. Keep it
outside every repository and never ship it: keygen refuses a path inside a git
work tree, found by a .git above the directory with its symlinks resolved (a
bare repository whose work tree is elsewhere, as a --git-dir dotfiles setup,
leaves no .git to find: keep keys out of such a tree yourself). The public key
goes into the app's .publicKey. To rotate, ship a build that trusts the old and
the new key (';'-separated), sign with the new one from the next release, and
drop the old one a release later.

THE BYTES. The document is serialized as json.dumps(document) with its defaults
(", " and ": " separators, ASCII, the draft's key order) and the envelope as
json.dumps(envelope) and one newline: Ed25519 is deterministic, so the same
draft and key give the same file, byte for byte.

ED25519 is RFC 8032 section 5.1 written here from the RFC's equations, in
Python's integers (standard library only: hashlib, base64, json, os, argparse;
Python 3.9 or later), checked by --selftest against RFC 8032 section 7.1's
TEST 1, 2 and 3 (given an envelope signed with TEST 1's key, it also checks
that sign writes that file's bytes from its document). verify refuses what
RayClay's verifier refuses: s >= L, a public key of small order, an R or a
key that does not decode. It is not constant-time: run it on the developer's
own machine, never as a service.

Exit: 0 done; 1 refused or a check failed (the reason on stderr); 2 usage.
"""

import argparse
import base64
import contextlib
import hashlib
import json
import os
import re
import sys
import tempfile

# ---------------------------------------------------------------------------
# Ed25519 (RFC 8032 section 5.1): the curve -x^2 + y^2 = 1 + d x^2 y^2 over
# GF(p), points in extended coordinates (X, Y, Z, T) with x = X/Z, y = Y/Z,
# x*y = T/Z (section 5.1.4).
# ---------------------------------------------------------------------------

P = 2 ** 255 - 19
L = 2 ** 252 + 27742317777372353535851937790883648493   # the group order
D = (-121665 * pow(121666, P - 2, P)) % P
SQRT_M1 = pow(2, (P - 1) // 4, P)
IDENTITY = (0, 1, 1, 0)


def _inv(x):
    return pow(x, P - 2, P)


def _recover_x(y, sign):
    """x for y and x's sign bit (section 5.1.3), or None when there is none."""
    if y >= P:
        return None
    x2 = (y * y - 1) * _inv(D * y * y + 1) % P
    if x2 == 0:
        return None if sign else 0
    x = pow(x2, (P + 3) // 8, P)
    if (x * x - x2) % P != 0:
        x = x * SQRT_M1 % P
    if (x * x - x2) % P != 0:
        return None
    if (x & 1) != sign:
        x = P - x
    return x


def _add(p1, p2):
    """Section 5.1.4's addition, which also doubles."""
    x1, y1, z1, t1 = p1
    x2, y2, z2, t2 = p2
    a = (y1 - x1) * (y2 - x2) % P
    b = (y1 + x1) * (y2 + x2) % P
    c = 2 * t1 * t2 * D % P
    dd = 2 * z1 * z2 % P
    e, f, g, h = b - a, dd - c, dd + c, b + a
    return (e * f % P, g * h % P, f * g % P, e * h % P)


def _mul(s, pt):
    q = IDENTITY
    while s > 0:
        if s & 1:
            q = _add(q, pt)
        pt = _add(pt, pt)
        s >>= 1
    return q


def _equal(p1, p2):
    x1, y1, z1, _ = p1
    x2, y2, z2, _ = p2
    return (x1 * z2 - x2 * z1) % P == 0 and (y1 * z2 - y2 * z1) % P == 0


_BY = 4 * _inv(5) % P
_BX = _recover_x(_BY, 0)
BASE = (_BX, _BY, 1, _BX * _BY % P)


def _encode(pt):
    x, y, z, _ = pt
    zi = _inv(z)
    x, y = x * zi % P, y * zi % P
    return (y | ((x & 1) << 255)).to_bytes(32, "little")


def _decode(b):
    if len(b) != 32:
        return None
    y = int.from_bytes(b, "little")
    sign = y >> 255
    y &= (1 << 255) - 1
    x = _recover_x(y, sign)
    return None if x is None else (x, y, 1, x * y % P)


def _h(data):
    return int.from_bytes(hashlib.sha512(data).digest(), "little")


def _expand(seed):
    h = hashlib.sha512(seed).digest()
    a = int.from_bytes(h[:32], "little")
    a &= (1 << 254) - 8
    a |= 1 << 254
    return a, h[32:]


def public_key(seed):
    """The 32-byte public key of a 32-byte seed (section 5.1.5)."""
    a, _ = _expand(seed)
    return _encode(_mul(a, BASE))


def sign(seed, msg):
    """The 64-byte signature of msg (section 5.1.6)."""
    a, prefix = _expand(seed)
    pub = _encode(_mul(a, BASE))
    r = _h(prefix + msg) % L
    rb = _encode(_mul(r, BASE))
    k = _h(rb + pub + msg) % L
    return rb + ((r + k * a) % L).to_bytes(32, "little")


def verify(pub, msg, sig):
    """Section 5.1.7, cofactorless, plus RayClay's refusals: s >= L and a public
    key of small order ([8]A the identity), which TweetNaCl alone accepts."""
    if len(pub) != 32 or len(sig) != 64:
        return False
    a = _decode(pub)
    r = _decode(sig[:32])
    s = int.from_bytes(sig[32:], "little")
    if a is None or r is None or s >= L or _equal(_mul(8, a), IDENTITY):
        return False
    k = _h(sig[:32] + pub + msg) % L
    return _equal(_mul(s, BASE), _add(r, _mul(k, a)))


def key_id(pub):
    return hashlib.sha256(pub).digest()[:4].hex()


def b64(data):
    return base64.b64encode(data).decode("ascii")


def unb64(text, size):
    """Strict base64 of exactly `size` bytes, or None."""
    if not isinstance(text, str) or not re.fullmatch(r"[A-Za-z0-9+/]*={0,2}", text) or \
            len(text) % 4:
        return None
    try:
        data = base64.b64decode(text, validate=True)
    except (ValueError, TypeError):
        return None
    return data if len(data) == size and b64(data) == text else None


# ---------------------------------------------------------------------------
# The document's rules: what rcUpdateCheck refuses, refused here first.
# ---------------------------------------------------------------------------

KINDS = ("page", "exe", "installer", "app-zip", "reload")
# The kinds a desktop host downloads and installs (exe; installer on Windows;
# app-zip on macOS): the app refuses a signed entry of one that does not name
# what it installs, its size above 0 and its sha256.
INSTALLS = ("exe", "installer", "app-zip")
# re.ASCII throughout: a str pattern's \d also matches other scripts' digits,
# which the app's parsers refuse.
SEMVER = re.compile(r"(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)"
                    r"(?:-((?:0|[1-9]\d*|\d*[A-Za-z-][0-9A-Za-z-]*)"
                    r"(?:\.(?:0|[1-9]\d*|\d*[A-Za-z-][0-9A-Za-z-]*))*))?"
                    r"(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?", re.A)
RFC3339 = re.compile(r"(\d{4})-(\d{2})-(\d{2})[Tt](\d{2}):(\d{2}):(\d{2})(?:\.\d+)?"
                     r"(?:[Zz]|([+-])(\d{2}):(\d{2}))", re.A)
LOOPBACK = re.compile(r"(?:localhost|[^/?#@:\[\]]+\.localhost"
                      r"|127(?:\.(?:25[0-5]|2[0-4]\d|1?\d?\d)){3}|\[::1\])(?::\d*)?", re.I | re.A)
# The authority rcFetchCheck reads: an IPv6 literal or a host with none of
# % [ ] :, then an optional port of digits (0-65535, checked after).
AUTHORITY = re.compile(r"(\[[0-9A-Fa-f:.]+\]|[^:%\[\]]+)(?::([0-9]*))?", re.A)
URL_BANNED = "\"<>\\^`{}|"           # rcOpenUrl's: a shell or a URL parser reads these two ways
ENVELOPE_MAX = 65536                  # rcUpdateCheck fetches at most this many bytes


class Refused(Exception):
    pass


def semver_key(v):
    """A sort key in SemVer 2.0.0 precedence; Refused when v is not SemVer."""
    m = SEMVER.fullmatch(v) if isinstance(v, str) else None
    if not m:
        raise Refused("%r is not SemVer-shaped, such as \"1.4.2\"" % (v,))
    pre = m.group(4)
    ids = []
    for part in (pre.split(".") if pre else []):
        ids.append((0, int(part), "") if part.isdigit() else (1, 0, part))
    # A release ranks above every pre-release of it: (1,) beats (0, ...).
    return (int(m.group(1)), int(m.group(2)), int(m.group(3)), (1,) if pre is None else (0, ids))


def _days(y, m, d):
    """Days from 1970-01-01 to y-m-d (Howard Hinnant's days_from_civil, as the
    app counts them): any four-digit year, 0000 included, where
    calendar.timegm stops at 1."""
    y -= m <= 2
    era = (y if y >= 0 else y - 399) // 400
    yoe = y - era * 400
    doe = yoe * 365 + yoe // 4 - yoe // 100 + (153 * (m - 3 if m > 2 else m + 9) + 2) // 5 + d - 1
    return era * 146097 + doe - 719468


def rfc3339_seconds(text):
    m = RFC3339.fullmatch(text) if isinstance(text, str) else None
    if not m:
        raise Refused("%r is not an RFC 3339 time such as \"2027-01-18T18:00:00Z\"" % (text,))
    y, mo, d, h, mi, s = (int(m.group(i)) for i in range(1, 7))
    leap = (y % 4 == 0 and y % 100 != 0) or y % 400 == 0
    days_in = [31, 29 if leap else 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
    if not (1 <= mo <= 12 and 1 <= d <= days_in[mo - 1] and h < 24 and mi < 60 and s <= 60):
        raise Refused("%r is not a real time" % (text,))
    off = 0
    if m.group(7):
        oh, om = int(m.group(8)), int(m.group(9))
        if oh > 23 or om > 59:
            raise Refused("%r has an offset past +/-23:59" % (text,))
        off = (oh * 60 + om) * 60 * (1 if m.group(7) == "+" else -1)
    return _days(y, mo, d) * 86400 + h * 3600 + mi * 60 + s - off


def url_ok(url):
    """The app's rule for an artifact url: rcFetchCheck's, https or http to
    loopback, and rcOpenUrl's bytes, all as written (nothing normalized)."""
    if not isinstance(url, str) or any(ord(c) < 0x21 or ord(c) > 0x7e or c in URL_BANNED
                                       for c in url):
        return False
    m = re.match(r"(?i)(https?)://([^/?#]*)", url)
    if not m or not m.group(2) or "@" in m.group(2):
        return False
    a = AUTHORITY.fullmatch(m.group(2))
    if not a or (a.group(2) and int(a.group(2)) > 65535):
        return False
    return m.group(1).lower() == "https" or bool(LOOPBACK.fullmatch(m.group(2)))


def names_msi(url):
    """The app's rule for an installer's url: its path, up to a '?' or '#', ends
    in .msi, in any case."""
    return re.split(r"[?#]", url, maxsplit=1)[0].lower().endswith(".msi")


def is_int(v):
    return isinstance(v, int) and not isinstance(v, bool) and 0 <= v < 2 ** 64


def check_document(doc, now, signed=True):
    """Refused, with the reason, for a document rcUpdateCheck would refuse."""
    if not isinstance(doc, dict):
        raise Refused("the document is not a JSON object")
    need = ["schema", "version"] + (["app", "channel", "expires"] if signed else [])
    for name in need:
        if name not in doc:
            raise Refused("the document has no \"%s\"%s" % (
                name, ", which a signed manifest must carry" if signed else ""))
    if doc["schema"] != 1 or not is_int(doc["schema"]):
        raise Refused("\"schema\" must be 1")
    for name in ("app", "channel", "version", "expires", "minimumVersion", "notes"):
        if name in doc and (not isinstance(doc[name], str) or "\0" in doc[name]):
            raise Refused("\"%s\" must be a string" % name)
    semver_key(doc["version"])
    if "minimumVersion" in doc:
        semver_key(doc["minimumVersion"])
    if "build" in doc and not is_int(doc["build"]):
        raise Refused("\"build\" must be a whole number")
    if "expires" in doc and rfc3339_seconds(doc["expires"]) <= now:
        raise Refused("\"expires\" (%s) has already passed" % doc["expires"])
    arts = doc.get("artifacts")
    if not isinstance(arts, dict) or not arts:
        # "artefacts" is the spelling an older RayClay read; the app reads
        # "artifacts" alone, so a draft still spelled the old way is named here.
        raise Refused("the document has no \"artifacts\" object%s" % (
            " (\"artefacts\" is the spelling older apps read)" if "artefacts" in doc else ""))
    for key, entry in arts.items():
        if not isinstance(entry, dict) or not isinstance(entry.get("kind"), str):
            raise Refused("artifact %r is not an object with a \"kind\"" % key)
        kind = entry["kind"]
        if kind not in KINDS:
            raise Refused("artifact %r: kind %r is none of %s" % (key, kind, ", ".join(KINDS)))
        if kind != "reload" and "url" not in entry:
            raise Refused("artifact %r has no \"url\"" % key)
        if "url" in entry and not url_ok(entry["url"]):
            raise Refused("artifact %r: its url is not https (plain http only to loopback), "
                          "printable ASCII with none of \" < > \\ ^ ` { } |, with a valid host "
                          "and port and no credentials" % key)
        if kind == "installer" and names_msi(entry["url"]):
            raise Refused("artifact %r: kind installer takes an .exe, and its url names a .msi, "
                          "which Windows Installer reads after the app's hold on it has ended: "
                          "ship the .msi inside a bootstrapper .exe" % key)
        if "size" in entry and not is_int(entry["size"]):
            raise Refused("artifact %r: \"size\" must be a whole number of bytes" % key)
        if "sha256" in entry and not (isinstance(entry["sha256"], str) and
                                      re.fullmatch(r"[0-9a-f]{64}", entry["sha256"])):
            raise Refused("artifact %r: \"sha256\" must be 64 lower-case hex digits" % key)
        if signed and kind in INSTALLS and not (is_int(entry.get("size")) and entry["size"] > 0
                                                and "sha256" in entry):
            if entry.get("size") == 0:
                raise Refused("artifact %r is kind %s, which the app installs: its \"size\" is 0, "
                              "an empty file, which the app refuses (is the file --artifact %s= "
                              "read the one you built?)" % (key, kind, key))
            raise Refused("artifact %r is kind %s, which the app installs: it needs its \"size\" "
                          "(above 0) and \"sha256\" (--artifact %s=<file> fills both)"
                          % (key, kind, key))


def newer(doc, prev):
    """Refused unless doc is newer than prev: the build when both have one,
    else the version, as the app compares."""
    if prev.get("app") != doc.get("app"):
        raise Refused("--previous is another app's manifest (%r, not %r)" % (prev.get("app"),
                                                                           doc.get("app")))
    if prev.get("channel") != doc.get("channel"):
        raise Refused("--previous is another channel's manifest (%r, not %r)" % (
            prev.get("channel"), doc.get("channel")))
    if is_int(doc.get("build")) and is_int(prev.get("build")) and doc["build"] and prev["build"]:
        if doc["build"] <= prev["build"]:
            raise Refused("build %d is not above the previous manifest's %d: the app would read "
                          "CURRENT or refuse a downgrade" % (doc["build"], prev["build"]))
    elif semver_key(doc["version"]) <= semver_key(prev["version"]):
        raise Refused("version %s is not above the previous manifest's %s" % (doc["version"],
                                                                             prev["version"]))


# ---------------------------------------------------------------------------
# Files
# ---------------------------------------------------------------------------

def inside_git_work_tree(path):
    """The work tree above where `path` would land, or None: symlinks resolved
    first (realpath), so a linked directory does not hide the repository."""
    d = os.path.realpath(os.path.dirname(os.path.abspath(path)))
    while True:
        if os.path.exists(os.path.join(d, ".git")):
            return d
        parent = os.path.dirname(d)
        if parent == d:
            return None
        d = parent


def keygen(path, out=sys.stdout):
    repo = inside_git_work_tree(path)
    if repo:
        raise Refused("%s is inside the git work tree %s: keep the secret key outside every "
                      "repository" % (path, repo))
    seed = os.urandom(32)
    pub = public_key(seed)
    text = json.dumps({"kind": "rc-update-secret-key", "seed": b64(seed), "publicKey": b64(pub),
                       "keyId": key_id(pub)}, indent=1) + "\n"
    try:
        fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError:
        raise Refused("%s exists: keygen never replaces a key" % path)
    with os.fdopen(fd, "w") as f:
        f.write(text)
    out.write("secret key: %s (mode 600; keep it outside every repository)\n" % path)
    out.write("publicKey:  %s\nkeyId:      %s\n" % (b64(pub), key_id(pub)))
    return pub


def read_seed(path):
    try:
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, ValueError) as e:
        raise Refused("cannot read the secret key file %s: %s" % (path, e))
    seed = unb64(data.get("seed") if isinstance(data, dict) else None, 32)
    if seed is None:
        raise Refused("%s holds no \"seed\" of 32 bytes of base64" % path)
    return seed


def read_json(path):
    """The file's JSON, refused when what sign would write from it is JSON the
    app's reader refuses: NaN or Infinity (Python reads 1e400 as Infinity and
    would write that), or a lone surrogate."""
    try:
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, ValueError) as e:
        raise Refused("cannot read %s as JSON: %s" % (path, e))
    try:
        json.dumps(data, allow_nan=False, ensure_ascii=False).encode("utf-8")
    except UnicodeEncodeError:
        raise Refused("%s holds a lone surrogate (\\ud800-\\udfff), which the app's JSON "
                      "reader refuses" % path)
    except ValueError as e:   # allow_nan=False: NaN, Infinity, and 1e400 read as Infinity
        raise Refused("%s holds a number the app's JSON reader refuses: %s" % (path, e))
    return data


def inner_document(data):
    """An envelope's document, or the object itself when it is a bare document."""
    if isinstance(data, dict) and isinstance(data.get("manifest"), str):
        try:
            return json.loads(data["manifest"])
        except ValueError as e:
            raise Refused("the envelope's manifest is not JSON: %s" % e)
    return data


def envelope_text(doc, seed):
    try:
        inner = json.dumps(doc, allow_nan=False)
    except ValueError as e:
        raise Refused("the document is not JSON the app reads: %s" % e)
    sig = sign(seed, inner.encode("utf-8"))
    text = json.dumps({"manifest": inner, "signature": b64(sig),
                       "keyId": key_id(public_key(seed))}) + "\n"
    if len(text.encode("utf-8")) > ENVELOPE_MAX:
        raise Refused("the envelope is %d bytes: the app reads at most %d (shorten the notes)"
                      % (len(text.encode("utf-8")), ENVELOPE_MAX))
    return text


def fill_artifacts(doc, specs):
    arts = doc.get("artifacts") if isinstance(doc, dict) else None
    for spec in specs or []:
        platform, sep, path = spec.partition("=")
        if not sep or not platform or not path:
            raise Refused("--artifact takes <platform>=<file>, not %r" % spec)
        if not isinstance(arts, dict) or platform not in arts or not isinstance(arts[platform],
                                                                              dict):
            raise Refused("--artifact %s: the draft has no artifacts entry %r" % (spec, platform))
        h = hashlib.sha256()
        size = 0
        try:
            with open(path, "rb") as f:
                for block in iter(lambda: f.read(1 << 20), b""):
                    h.update(block)
                    size += len(block)
        except OSError as e:
            raise Refused("--artifact %s: %s" % (spec, e))
        arts[platform]["size"] = size
        arts[platform]["sha256"] = h.hexdigest()


def cmd_sign(a, now):
    doc = read_json(a.draft)
    # check_document grades "artifacts" alone, and an app built with an older RayClay reads
    # "artefacts" alone: signed, that table would reach those apps unchecked.
    if isinstance(doc, dict) and "artefacts" in doc:
        raise Refused("the draft carries \"artefacts\", the older spelling, which nothing here "
                      "checks: delete it (sign writes \"artifacts\" alone)")
    fill_artifacts(doc, a.artifact)
    check_document(doc, now)
    if a.previous:
        prev = inner_document(read_json(a.previous))
        if not isinstance(prev, dict) or "version" not in prev:
            raise Refused("--previous %s holds no manifest with a version" % a.previous)
        newer(doc, prev)
    text = envelope_text(doc, read_seed(a.key))
    if a.output:
        with open(a.output, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        sys.stderr.write("signed: %s (keyId %s)\n" % (a.output, json.loads(text)["keyId"]))
    else:
        sys.stdout.write(text)


def check_envelope(data, keys, now):
    """The document of an envelope that verifies under one of `keys` (the one its
    keyId names), checked as the app checks it; Refused otherwise."""
    if not isinstance(data, dict) or not all(isinstance(data.get(k), str)
                                             for k in ("manifest", "signature", "keyId")):
        raise Refused("not an envelope: {\"manifest\", \"signature\", \"keyId\"}, each a string")
    by_id = {key_id(k): k for k in keys}
    pub = by_id.get(data["keyId"])
    if pub is None:
        raise Refused("no given key has the envelope's keyId %r (given: %s)" % (
            data["keyId"], ", ".join(sorted(by_id)) or "none"))
    sig = unb64(data["signature"], 64)
    if sig is None or not verify(pub, data["manifest"].encode("utf-8"), sig):
        raise Refused("the signature does not verify under key %s" % data["keyId"])
    doc = inner_document(data)
    check_document(doc, now)
    # Apps built with an older RayClay read "artefacts" alone. Earlier releases of sign wrote it
    # equal to "artifacts", which is checked above; a table that differs is not.
    if "artefacts" in doc and doc["artefacts"] != doc["artifacts"]:
        raise Refused("the envelope's \"artefacts\" (the older spelling, which apps built "
                      "with an older RayClay read alone) differs from its checked \"artifacts\"")
    return doc


def cmd_verify(a, now):
    keys = []
    for text in a.public_key or []:
        k = unb64(text, 32)
        if k is None:
            raise Refused("--public-key %r is not 32 bytes of base64" % text)
        keys.append(k)
    for path in a.key or []:
        keys.append(public_key(read_seed(path)))
    if not keys:
        raise Refused("verify needs --public-key or --key")
    doc = check_envelope(read_json(a.envelope), keys, now)
    print("OK: %s verifies under keyId %s: %s %s%s, channel %s, expires %s" % (
        a.envelope, read_json(a.envelope)["keyId"], doc["app"], doc["version"],
        " build %d" % doc["build"] if is_int(doc.get("build")) else "", doc["channel"],
        doc["expires"]))
    for key, entry in doc["artifacts"].items():
        print("  %-16s %-9s %s" % (key, entry["kind"], entry.get("url", "")))


def plan(repo, tag, specs, out=sys.stdout):
    """Print the GitHub Releases order. Runs nothing."""
    repo = repo or "OWNER/REPO"
    tag = tag or "vX.Y.Z"
    files = []
    for spec in specs or []:
        platform, _, path = spec.partition("=")
        files.append((platform, path or platform))
    w = out.write
    w("== rc-update-publish --plan: a release on GitHub Releases "
      "(prints only: nothing runs) ==\n\n")
    w("1. Build, sign and notarize each artifact with your own tools (codesign, notarytool,\n"
      "   signtool). This tool signs only the manifest.\n")
    w("2. Make the release as a DRAFT, so nothing is public yet:\n"
      "       gh release create %s --repo %s --draft --title %s --notes-file NOTES.md\n"
      % (tag, repo, tag))
    w("3. Upload the artifacts BEFORE the manifest, so no manifest ever names a missing file:\n")
    for _, path in files or [("", "<artifact>")]:
        w("       gh release upload %s --repo %s %s\n" % (tag, repo, path))
    w("4. In the draft manifest, each artifact's url is its download URL:\n")
    for platform, path in files or [("<platform>", "<artifact>")]:
        w("       %-15s https://github.com/%s/releases/download/%s/%s\n"
          % (platform, repo, tag, os.path.basename(path)))
    w("5. Sign it (fills size and sha256; --previous refuses a build that is not newer):\n"
      "       rc-update-publish.py sign draft.json --key <secret-key-file> %s"
      "--previous <last manifest.json> -o manifest.json\n"
      % "".join("--artifact %s=%s " % f for f in files))
    w("6. Check it against the key the app ships, then upload it:\n"
      "       rc-update-publish.py verify manifest.json --public-key <the app's .publicKey>\n"
      "       gh release upload %s --repo %s manifest.json\n" % (tag, repo))
    w("7. Review the draft (every asset present, the notes), then publish it as the latest:\n"
      "       gh release edit %s --repo %s --draft=false --latest\n" % (tag, repo))
    w("   The app's manifest URL is then\n"
      "       https://github.com/%s/releases/latest/download/manifest.json\n"
      "   (a beta channel's manifest lives under its own tag's URL, never latest).\n\n" % repo)
    w("NOTHING WAS UPLOADED OR RELEASED: these are the commands, in order.\n")


# ---------------------------------------------------------------------------
# --selftest
# ---------------------------------------------------------------------------

RFC8032 = (   # section 7.1: TEST 1, 2 and 3 (secret key, public key, message, signature)
    ("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
     "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", "",
     "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555"
     "fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b"),
    ("4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb",
     "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", "72",
     "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da"
     "085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00"),
    ("c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7",
     "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025", "af82",
     "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac"
     "18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a"),
)


def selftest(envelope=None):
    now = rfc3339_seconds("2026-10-27T12:00:00Z")
    fails = []

    def check(ok, what):
        print("selftest: %-66s %s" % (what, "ok" if ok else "FAILED"))
        if not ok:
            fails.append(what)

    def refused(fn, what, *args):
        check(_raises(fn, *args), what)

    for i, (sk, pk, msg, sig) in enumerate(RFC8032, 1):
        seed, m = bytes.fromhex(sk), bytes.fromhex(msg)
        check(public_key(seed).hex() == pk, "RFC 8032 7.1 TEST %d: the public key" % i)
        check(sign(seed, m).hex() == sig, "RFC 8032 7.1 TEST %d: the signature" % i)
        check(verify(bytes.fromhex(pk), m, bytes.fromhex(sig)),
              "RFC 8032 7.1 TEST %d: verifies" % i)
    seed2, pub2 = bytes.fromhex(RFC8032[1][0]), bytes.fromhex(RFC8032[1][1])
    sig2 = bytes.fromhex(RFC8032[1][3])
    # The keyIds these two RFC 8032 keys must derive: a sign under another derivation
    # would write envelopes every app refuses.
    check(key_id(bytes.fromhex(RFC8032[0][1])) == "21fe31df" and key_id(pub2) == "39f713d0",
          "keyId: TEST 1's key is 21fe31df, TEST 2's 39f713d0")
    twin = sig2[:32] + (int.from_bytes(sig2[32:], "little") + L).to_bytes(32, "little")
    check(not verify(pub2, b"\x72", twin), "TEST 2's signature with s + L is refused (s >= L)")
    # Under the identity key (y = 1, of small order) R = B, s = 1 satisfies the
    # equation for any message, [1]B = B + [k]A, so only the small-order rule refuses it.
    ident, forged = bytes([1]) + bytes(31), _encode(BASE) + (1).to_bytes(32, "little")
    k = _h(forged[:32] + ident + b"any") % L
    check(_equal(_mul(1, BASE), _add(_decode(forged[:32]), _mul(k, _decode(ident)))),
          "control: R = B, s = 1 meets the equation under the identity key")
    check(not verify(ident, b"any", forged), "that forgery under a small-order key is refused")

    with tempfile.TemporaryDirectory() as tmp, open(os.devnull, "w") as quiet:
        key = os.path.join(tmp, "update.key")
        pub = keygen(key, quiet)
        mode = os.stat(key).st_mode & 0o777
        check(os.name != "posix" or mode == 0o600, "keygen writes the secret key mode 600")
        check(public_key(read_seed(key)) == pub, "keygen's file holds the seed of the printed key")
        refused(keygen, "keygen never replaces an existing key", key, quiet)
        repo = os.path.join(tmp, "repo")
        os.makedirs(os.path.join(repo, ".git"))
        refused(keygen, "keygen refuses a path inside a git work tree",
                os.path.join(repo, "sub", "k.key"), quiet)
        check(not os.path.exists(os.path.join(repo, "sub", "k.key")), "  and writes nothing there")
        link = os.path.join(tmp, "link")
        try:
            os.makedirs(os.path.join(repo, "keys"))
            os.symlink(os.path.join(repo, "keys"), link)
        except (OSError, NotImplementedError):
            link = None   # Windows without the symlink privilege: nothing to check
        if link:
            refused(keygen, "keygen refuses a work tree reached through a symlink",
                    os.path.join(link, "k.key"), quiet)

        art = os.path.join(tmp, "Notes.AppImage")
        with open(art, "wb") as f:
            f.write(b"abc")
        draft = {"schema": 1, "app": "org.example.notes", "channel": "stable", "version": "1.4.2",
                 "build": 20261020, "expires": "2099-12-31T00:00:00Z", "notes": "Fixes.",
                 "artifacts": {"linux-x86_64": {"kind": "exe",
                                                "url": "https://example.com/Notes.AppImage"}}}
        fill_artifacts(draft, ["linux-x86_64=" + art])
        e = draft["artifacts"]["linux-x86_64"]
        check(e["size"] == 3 and e["sha256"] == hashlib.sha256(b"abc").hexdigest(),
              "--artifact fills size and sha256")
        text = envelope_text(draft, read_seed(key))
        env = json.loads(text)
        check(text.endswith("}\n") and env["keyId"] == key_id(pub), "sign writes the envelope")
        old = {k: v for k, v in draft.items() if k != "artifacts"}
        old["artefacts"] = draft["artifacts"]
        refused(check_document, "a draft with the older key \"artefacts\" alone is refused, "
                "as the app does", old, now)
        try:
            check(check_envelope(env, [pub], now)["build"] == 20261020,
                  "sign then verify: the round trip verifies")
        except Refused as err:
            check(False, "sign then verify: the round trip verifies (%s)" % err)
        flipped = dict(env, manifest=env["manifest"].replace("20261020", "20261029"))
        refused(check_envelope, "one byte of the manifest changed: verify refuses", flipped,
                [pub], now)
        refused(check_envelope, "a key the envelope does not name: verify refuses", env,
                [pub2], now)
        # b64decode(validate=True) takes pad bits that are not 0, which the app refuses:
        # the last data character before "==" with its low pad bit set.
        alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
        s = env["signature"]
        loose = s[:85] + alphabet[alphabet.index(s[85]) | 1] + s[86:]
        refused(check_envelope, "a signature whose base64 sets a pad bit: verify refuses",
                dict(env, signature=loose), [pub], now)
        # An envelope given after --selftest, signed with TEST 1's key: its document signed
        # again gives the file byte for byte. A file that is missing reads FAILED.
        if envelope is not None:
            same = False
            if os.path.isfile(envelope):
                with open(envelope, "rb") as f:
                    raw = f.read()
                doc1 = json.loads(json.loads(raw.decode("utf-8"))["manifest"])
                same = envelope_text(doc1, bytes.fromhex(RFC8032[0][0])).encode("utf-8") == raw
            check(same, "the envelope given is sign's bytes for its document")

        # The commands themselves, through main(): what wires the rules into sign and verify.
        def write(name, data):
            path = os.path.join(tmp, name)
            with open(path, "w", encoding="utf-8") as f:
                f.write(data if isinstance(data, str) else json.dumps(data))
            return path

        def run(*argv):
            with contextlib.redirect_stdout(quiet), contextlib.redirect_stderr(quiet):
                return main(list(argv))
        good, prev = write("draft.json", draft), write("prev.json", text)
        bad = write("bad.json", dict(draft, artifacts={"web": {"kind": "page",
                                                                "url": "http://example.com/"}}))
        outs = [os.path.join(tmp, "out%d.json" % i) for i in range(4)]
        check(run("sign", good, "--key", key, "-o", outs[0]) == 0 and os.path.exists(outs[0]),
              "control: sign exits 0 and writes the envelope")
        check(os.path.exists(outs[0]) and json.loads(read_json(outs[0])["manifest"]) == draft,
              "sign signs the draft's document unchanged: \"artifacts\" alone")
        check(run("sign", bad, "--key", key, "-o", outs[1]) == 1 and not os.path.exists(outs[1]),
              "sign refuses a draft the app would refuse: exit 1, no file")
        check(run("sign", good, "--key", key, "--previous", prev, "-o", outs[2]) == 1 and
              not os.path.exists(outs[2]), "sign --previous of the same build: exit 1, no file")
        # "artefacts" beside a good "artifacts" is a table an app built with an older RayClay reads
        # alone and no check reads: sign refuses it, a copy equal to "artifacts" too.
        for what, arts in (("an http table", {"linux-x86_64": {
                               "kind": "exe", "url": "http://example.com/old.AppImage"}}),
                           ("a copy equal to it", draft["artifacts"])):
            stale = write("stale.json", dict(draft, artefacts=arts))
            stale_out = os.path.join(tmp, "stale-out.json")
            check(run("sign", stale, "--key", key, "-o", stale_out) == 1 and
                  not os.path.exists(stale_out),
                  "\"artefacts\" beside \"artifacts\", %s: sign exits 1, no file" % what)
        # Earlier releases of sign wrote both keys equal: such an envelope still verifies,
        # and one whose "artefacts" differs does not.
        both = dict(draft, artefacts=draft["artifacts"])
        check(run("verify", write("both-env.json", envelope_text(both, read_seed(key))), "--key",
                  key) == 0, "an envelope whose \"artefacts\" equals \"artifacts\": verify exits 0")
        both["artefacts"] = {"linux-x86_64": {"kind": "exe", "url": "http://example.com/old"}}
        check(run("verify", write("diff-env.json", envelope_text(both, read_seed(key))), "--key",
                  key) == 1, "an envelope whose \"artefacts\" differs: verify exits 1")
        # An installed kind names what is installed. The control is the signed exe
        # above (good), its size and sha256 filled by --artifact from a real file.
        bare = dict(draft, artifacts={"linux-x86_64": {
            "kind": "exe", "url": "https://example.com/Notes.AppImage"}})
        check(run("sign", write("bare.json", bare), "--key", key, "-o", outs[3]) == 1 and
              not os.path.exists(outs[3]), "an exe with no size or sha256: sign exits 1, no file")
        check(run("verify", write("bare-env.json", envelope_text(bare, read_seed(key))), "--key",
                  key) == 1, "a signed exe with no size or sha256: verify exits 1")
        abc = hashlib.sha256(b"abc").hexdigest()
        for kind, entry, what in (("exe", {"size": 0, "sha256": abc}, "size 0"),
                                  ("exe", {"sha256": abc}, "no size"),
                                  ("exe", {"size": 3}, "no sha256"),
                                  ("installer", {}, "no size and no sha256"),
                                  ("app-zip", {}, "no size and no sha256")):
            refused(check_document, "a signed %s with %s is refused" % (kind, what),
                    dict(draft, artifacts={"windows-x86_64": dict(
                        entry, kind=kind, url="https://example.com/Notes.bin")}), now)
        # An empty artifact file fills size 0: the refusal names the empty file, not the
        # missing fields --artifact would have filled.
        empty, why = write("empty.bin", ""), ""
        ed = dict(draft, artifacts={"linux-x86_64": {"kind": "exe",
                                                     "url": "https://example.com/Notes.AppImage"}})
        fill_artifacts(ed, ["linux-x86_64=" + empty])
        try:
            check_document(ed, now)
        except Refused as err:
            why = str(err)
        check("is 0, an empty file" in why, "an exe --artifact filled from an empty file: refused, "
              "named as empty")
        check(run("verify", prev, "--key", key) == 0, "verify of the envelope exits 0")
        check(run("verify", write("flipped.json", flipped), "--key", key) == 1,
              "verify of a changed manifest exits 1")
        same = dict(draft, version="1.4.3")
        refused(newer, "--previous: a build not above the previous one is refused", same, draft)
        refused(newer, "--previous: another channel's manifest is refused",
                dict(draft, build=20261021, channel="beta"), draft)
        check(not _raises(newer, dict(draft, build=20261021), draft),
              "--previous: a higher build passes")
        refused(newer, "--previous: another app's manifest is refused",
                dict(draft, build=20261021, app="org.example.other"), draft)
        # With no build on a side the version is compared, in SemVer precedence:
        # a release ranks above its own pre-releases.
        nob = {k: v for k, v in draft.items() if k != "build"}
        refused(newer, "--previous, no build: the same version is refused", nob, nob)
        refused(newer, "--previous, no build: 1.4.2-rc.1, below the release 1.4.2, is refused",
                dict(nob, version="1.4.2-rc.1"), nob)
        check(not _raises(newer, dict(nob, version="1.4.3-rc.1"), nob),
              "--previous, no build: 1.4.3-rc.1, above the release 1.4.2, passes")
        refused(check_document, "a version that is not SemVer (1.4) is refused",
                dict(draft, version="1.4"), now)
        refused(check_document, "a document with http:// to a public host is refused",
                dict(draft, artifacts={"web": {"kind": "page", "url": "http://example.com/"}}),
                now)
        refused(check_document, "a signed document with no channel is refused",
                {k: v for k, v in draft.items() if k != "channel"}, now)
        refused(check_document, "an expired document is refused",
                dict(draft, expires="2026-10-26T12:00:00Z"), now)
        for bad, what in (("2099-12-31T00:00:00+99:99", "an offset past 23:59"),
                          ("\u0662\u0660\u0669\u0669-12-31T00:00:00Z", "non-ASCII digits"),
                          ("0000-01-01T00:00:00Z", "the year 0000 (passed)")):
            refused(check_document, "expires with %s is refused" % what, dict(draft, expires=bad),
                    now)
        for url in ("https://example.com/a|b", "https://example.com:99999/x"):
            refused(check_document, "an artifact url %s is refused" % url,
                    dict(draft, artifacts={"web": {"kind": "page", "url": url}}), now)
        for url, what in (("https://user@example.com/x", "credentials"),
                          ("https://example.com/a b", "a space"),
                          ("https://example.com/\u00e9", "a character above 0x7e")):
            refused(check_document, "an artifact url with %s is refused" % what,
                    dict(draft, artifacts={"web": {"kind": "page", "url": url}}), now)
        refused(check_document, "an artifact of a kind this release does not know is refused",
                dict(draft, artifacts={"web": {"kind": "zip", "url": "https://example.com/x"}}),
                now)
        refused(check_document, "a page with no url is refused",
                dict(draft, artifacts={"web": {"kind": "page"}}), now)
        # The types and bounds the app's reader holds a field to, one field each.
        page = {"kind": "page", "url": "https://example.com/"}
        upper = dict(e, sha256=e["sha256"].upper())
        for bad, what in ((dict(draft, build=True), "a build of true"),
                          (dict(draft, notes="a\0b"), "a NUL in the notes"),
                          (dict(draft, minimumVersion="1.4"), "a minimumVersion of 1.4"),
                          (dict(draft, artifacts={"linux-x86_64": upper}), "an upper-case sha256"),
                          (dict(draft, artifacts={"web": dict(page, size="3")}), "a size of \"3\""),
                          (dict(draft, artifacts={}), "an empty artifacts object"),
                          (dict(draft, expires="2099-12-31T24:00:00Z"), "expires at 24:00")):
            refused(check_document, "%s is refused" % what, bad, now)
        for url in ("https://example.com/Notes-1.4.2.msi", "https://example.com/n.MSI?dl=1#x"):
            for plat in ("windows-x86_64", "win-beta", "Windows-x86_64", "macos-arm64"):
                refused(check_document, "an installer whose url names a .msi is refused (%s)" % plat,
                        dict(draft, artifacts={plat: {"kind": "installer", "url": url,
                                                      "size": 3, "sha256": abc}}), now)
        check(not _raises(check_document, dict(draft, artifacts={
            "windows-x86_64": {"kind": "installer", "url": "https://example.com/s.exe?n=a.msi",
                               "size": 3, "sha256": abc},
            "web": {"kind": "page", "url": "https://example.com/notes.msi"}}), now),
            "an installer .exe (a .msi in its query only) and a page naming a .msi pass")
        for text, what in (('{"x": NaN}', "NaN"), ('{"x": 1e400}', "1e400"),
                           ('{"x": "\\ud800"}', "a lone surrogate")):
            path = os.path.join(tmp, "bad.json")
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)
            refused(read_json, "a draft holding %s is refused" % what, path)
        refused(envelope_text, "an envelope over 64 KiB is refused",
                dict(draft, notes="x" * ENVELOPE_MAX), read_seed(key))
        check(not _raises(check_document, dict(draft, artifacts={
            "web": {"kind": "reload"},
            "linux-x86_64": {"kind": "page", "url": "http://127.0.0.1:8080/x"}}), now),
            "reload with no url, and http to 127.0.0.1, pass")
        lines = []

        class Sink:
            write = lines.append
        plan("acme/notes", "v1.4.2", ["linux-x86_64=" + art], Sink())
        out = "".join(lines)
        check("NOTHING WAS UPLOADED OR RELEASED" in out and out.index("upload v1.4.2") <
              out.index("manifest.json\n"), "--plan prints the artifacts before the manifest")
    if fails:
        print("selftest: FAIL (%d of the checks above)" % len(fails))
        return 1
    print("selftest: PASS")
    return 0


def _raises(fn, *args):
    try:
        fn(*args)
    except Refused:
        return True
    return False


def main(argv):
    if argv[:1] == ["--selftest"]:
        if len(argv) > 2:
            sys.stderr.write("usage: rc-update-publish.py --selftest [<envelope.json>]\n")
            return 2
        return selftest(argv[1] if len(argv) == 2 else None)
    if argv[:1] == ["--plan"]:
        p = argparse.ArgumentParser(prog="rc-update-publish.py --plan")
        p.add_argument("--repo")
        p.add_argument("--tag")
        p.add_argument("--artifact", "--artefact", dest="artifact", action="append")
        a = p.parse_args(argv[1:])
        plan(a.repo, a.tag, a.artifact)
        return 0
    p = argparse.ArgumentParser(prog="rc-update-publish.py",
                                description="Make, sign and check RayClay's update manifest "
                                            "(also: --plan, --selftest).")
    sub = p.add_subparsers(dest="cmd")
    k = sub.add_parser("keygen", help="write a new secret key file (mode 600) and print its "
                                      "public key")
    k.add_argument("path")
    s = sub.add_parser("sign", help="sign a draft document into an envelope")
    s.add_argument("draft")
    s.add_argument("--key", required=True)
    s.add_argument("--artifact", "--artefact", dest="artifact", action="append",
                   metavar="PLATFORM=FILE")
    s.add_argument("--previous", metavar="ENVELOPE")
    s.add_argument("-o", "--output")
    v = sub.add_parser("verify", help="check an envelope against public key(s)")
    v.add_argument("envelope")
    v.add_argument("--public-key", action="append")
    v.add_argument("--key", action="append", help="a secret key file, for its public key")
    a = p.parse_args(argv)
    if not a.cmd:
        p.print_help(sys.stderr)
        return 2
    import time
    now = int(time.time())
    try:
        if a.cmd == "keygen":
            keygen(a.path)
        elif a.cmd == "sign":
            cmd_sign(a, now)
        else:
            cmd_verify(a, now)
    except Refused as e:
        sys.stderr.write("rc-update-publish: REFUSED: %s\n" % e)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
