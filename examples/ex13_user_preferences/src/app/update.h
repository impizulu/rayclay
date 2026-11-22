/*
    update.h - the update check and its banner: the part to copy into your app

    A web app fetches its version.json at load and, when the server has a
    newer build, shows a banner; the next load is the new build. This file is
    that, in C, as the About page of a settings dialog:

      rcUpdateCheck(url, .version, .publicKey, .app)   ask, from Check for updates
      rcUpdateResult(upd)          every frame: CHECKING, then CURRENT, AVAILABLE or FAILED;
                                   after Install, DOWNLOADING, then READY
      rcUpdateApply(upd)           from the click: INSTALL downloads the new build and puts it
                                   in this program's place, OPEN hands the offer's url to the
                                   browser (a phone's store page, a package manager's folder)
      rcUpdateRelaunch(upd)        Restart now, on READY: arms the new build's start at this exit
      rcUpdateFree(upd)            Later: the banner goes until the next check
      rcUpdateCancel(upd)          Stop: a check or a download still running reads CANCELED

    update_banner() is the banner, the one function to lift whole. The
    Updater's url and key fields and the Simulate list are this example's
    developer options, so serve.py can show every state on purpose.

    THE RULES THIS FILE KEEPS, each one a contract of rcUpdate*:
      - A check starts only from an event: Check for updates, Enter in a
        developer field, its hotkey, or the app's start once the user has
        turned on Automatic updates. ONE check per app: while one is CHECKING or
        DOWNLOADING a new ask starts nothing; in any other state a new check
        replaces the old one, whose token then reads NONE.
      - rcUpdateResult is read once, at the top of the frame, into up->u, and
        the screen draws from that copy. A click that starts, stops or frees
        the check reads it again, so what the frame draws after the click
        agrees with it.
      - The result's strings are RayClay's until rcUpdateFree or the next
        check (under the runner, to the end of that frame): what the app
        keeps, it copies (up->later).
      - rcUpdateApply is called from the click and nowhere else: a browser
        opens a page only from a user action. INSTALL needs the key: a check
        without one is only offered OPEN, which hands the browser the manifest's
        url, whatever it names: a store page in a shipped app, the new build's
        file under serve.py --build, which the browser then downloads. Where
        this program cannot replace itself the offer is OPEN too, and .message
        says why.
*/
#ifndef APP_UPDATE_H
#define APP_UPDATE_H

#include "rayclay.h"
#include "app/theme.h"

#include "icons/rc_icons_download.h"   /* a newer build, to fetch */

/* The running app, which the manifest's offer is compared with, and its
   name, which a signed manifest must carry. A release flow also passes
   .build, a whole number that rises with every build: where the app and the
   manifest both have one, it is the only number compared. */
#define APP_VERSION "1.0.0"
#define APP_ID      "org.rayclay.demo.ex13_user_preferences"

/* THE RUNNING VERSION. A shipped app passes APP_VERSION, and its next build
   is built with the next one. EXAMPLE ONLY: serve.py --build offers a copy
   of this very program as the new build, with the version it offers written
   over this array in the copy, so the build Install puts in place runs as
   that version and its own check reads CURRENT. The array is the tag, 13
   bytes, then 32 for the version and its zeros (serve.py's TAG_SLOT). */
static const char APP_VERSION_TAG[13 + 32] = "ex13-version:" APP_VERSION;
#define RUNNING_VERSION (APP_VERSION_TAG + 13)

/* THE KEY. A shipped app compiles its public key in and passes it with its
   app id, so only a manifest its developer signed is believed:

       #define APP_UPDATE_KEY "<the publicKey that rc-update-publish.py keygen printed>"
       upd = rcUpdateCheck(url, .version = APP_VERSION, .publicKey = APP_UPDATE_KEY, .app = APP_ID);

   This example compiles no key in. serve.py makes a throwaway pair each time
   it starts, outside this tree, and prints the public half to paste into the
   developer options' Public key field. With the field empty the check is
   prompt-only (no signature is checked), and the section says so. A key
   compiled into an example is trusted by every app copied from it. */

/* URL_CAP holds a manifest address and a knob's query; KEY_CAP holds the
   eight keys .publicKey takes at most, each 44 bytes of base64, with the
   ';' between them. */
enum { URL_CAP = 512, KEY_CAP = 384, NOTE_CAP = 200, VERSION_CAP = 64 };

/* serve.py's address on this machine: plain http is accepted to loopback
   alone. A shipped app names its own https address, such as
   https://github.com/<owner>/<repo>/releases/latest/download/manifest.json */
#define DEV_MANIFEST "http://127.0.0.1:8788/manifest.json"

/* The Simulate list, each a state shown on purpose: serve.py reads them from
   the manifest's query. */
typedef enum {
    KNOB_NONE = 0, KNOB_CURRENT, KNOB_REQUIRED, KNOB_TAMPER, KNOB_OTHER_APP, KNOB_FAIL, KNOB_SLOW,
    KNOB_COUNT
} Knob;
/* KNOB_CURRENT's offer is the running version. */
static const char *const KNOB_QUERY[KNOB_COUNT] = {
    "", "offer=", "required=1", "tamper=1", "app=org.example.other", "fail=500", "delay=3000"
};
static const char *const KNOB_LABEL[KNOB_COUNT] = {
    "A newer release", "This version again", "A required update",
    "A changed manifest", "Another app's manifest", "A server error (500)", "A slow answer (3 s)"
};

typedef struct {
    RC_Update       upd;                 /* the one check; 0 = none                    */
    RC_UpdateResult u;                   /* this frame's read of it                    */
    bool            keyed;               /* the live check was given a key             */
    int             knob;                /* the Simulate list's choice (a Knob)        */
    char            url[URL_CAP];        /* developer options: the manifest's address  */
    char            key[KEY_CAP];        /* developer options: EXAMPLE ONLY (THE KEY)  */
    char            later[VERSION_CAP];  /* the offer Later put off; "" = none         */
    char            note[NOTE_CAP];      /* what the last Open or Restart did          */
} Updater;

/* What Check for updates asks, under its own name: Developer options read it back. */
static const char *const UPDATE_STATE_NAME[] = {
    "NONE", "CHECKING", "CURRENT", "AVAILABLE", "DOWNLOADING", "READY", "FAILED", "CANCELED"
};
static const char *const UPDATE_ACTION_NAME[] = { "NONE", "INSTALL", "OPEN", "RELOAD" };
static const char *const UPDATE_ERROR_NAME[] = {
    "NONE", "NETWORK", "MANIFEST", "SIGNATURE", "EXPIRED", "HASH", "UNSUPPORTED", "IO",
    "UNAVAILABLE", "INVALID"
};

static inline bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

static inline bool has_char(const char *s, char c) {
    for (; *s; s++)
        if (*s == c) return true;
    return false;
}

/* @p src without the white space at either end, copied into @p dst: a pasted
   key often brings a newline with it. */
static inline void text_trim(char *dst, const char *src, size_t cap) {
    size_t n;

    while (is_space(*src)) src++;
    rcStrCopy(dst, src, cap);
    for (n = (size_t)rcStringFromCStr(dst).length; n > 0 && is_space(dst[n - 1]); n--)
        dst[n - 1] = '\0';
}

/* At the top of every frame, before anything is drawn. */
static inline void updater_read(Updater *up) {
    up->u = rcUpdateResult(up->upd);
}

static inline bool updater_busy(const Updater *up) {
    return up->u.state == RC_UPDATE_CHECKING || up->u.state == RC_UPDATE_DOWNLOADING;
}

/* Ask, from an event: the address and key as they stand, trimmed (an empty
   address is serve.py's), and the Simulate choice on the query. The copies
   are static: rcUpdateCheck copies every string before it returns, and an
   option set to a local array's address is MSVC's warning C4221 in C. */
static inline void updater_check(RC_App *app, Updater *up) {
    static char url[URL_CAP], key[KEY_CAP];
    const char *ask;

    if (updater_busy(up)) return;
    text_trim(url, up->url, sizeof url);
    text_trim(key, up->key, sizeof key);
    if (!url[0]) rcStrCopy(url, DEV_MANIFEST, sizeof url);
    ask = up->knob ? rcFormat(rcAppArena(app), "%s%s%s%s", url, has_char(url, '?') ? "&" : "?",
                              KNOB_QUERY[up->knob],
                              up->knob == KNOB_CURRENT ? RUNNING_VERSION : "").chars
                   : url;
    up->upd      = rcUpdateCheck(ask, .version = RUNNING_VERSION, .publicKey = key[0] ? key : NULL,
                                 .app = APP_ID);
    up->keyed    = key[0] != '\0';
    up->later[0] = up->note[0] = '\0';
    updater_read(up);
}

/* Stop: a check or a download still running reads CANCELED, and the
   download's partial file goes. */
static inline void updater_stop(Updater *up) {
    rcUpdateCancel(up->upd);
    updater_read(up);
}

/* Install or Open, from the click. INSTALL starts the download: the check
   reads DOWNLOADING, or FAILED when the file beside this program cannot be
   made. OPEN returns true when the url was handed to the system's browser,
   which says nothing of whether a page loaded; the check stays AVAILABLE. */
static inline void updater_apply(Updater *up) {
    RC_UpdateAction act = up->u.action;
    bool            ok  = rcUpdateApply(up->upd);

    if (act == RC_UPDATE_ACTION_INSTALL)
        up->note[0] = '\0';
    else
        rcStrCopy(up->note, ok ? "The offer went to your browser."
                               : "No browser took the link on this system.", sizeof up->note);
    updater_read(up);
}

/* Restart now, on READY: the new build's start is armed for this process's
   exit, and the app quits as it does for any reason. False when this
   process's arguments cannot be read: the new build runs at the next start. */
static inline void updater_restart(RC_App *app, Updater *up) {
    if (rcUpdateRelaunch(up->upd))
        rcAppRequestClose(app);
    else
        rcStrCopy(up->note, "The new build could not be started from here: it runs at the "
                            "next start.", sizeof up->note);
}

/* Later: the offer's version is copied, as its string goes with the check,
   and the check is freed, which hides the banner until the next check. */
static inline void updater_later(Updater *up) {
    rcStrCopy(up->later, up->u.version, sizeof up->later);
    rcUpdateFree(up->upd);
    up->upd = 0;
    updater_read(up);
}

/* The section's one-line answer under the version: what the last check said. */
static inline RC_String update_status(RC_App *app, const Updater *up) {
    RC_Arena *a = rcAppArena(app);

    switch (up->u.state) {
    case RC_UPDATE_CHECKING:    return rcStringFromCStr("Checking for updates...");
    case RC_UPDATE_CURRENT:     return rcStringFromCStr("You are up to date.");
    case RC_UPDATE_AVAILABLE:   return rcFormat(a, "Version %s is available.", up->u.version);
    case RC_UPDATE_DOWNLOADING: return rcFormat(a, "Downloading version %s...", up->u.version);
    case RC_UPDATE_READY:       return rcFormat(a, "Version %s is installed.", up->u.version);
    case RC_UPDATE_FAILED:
        /* No server, a 500, a download cut short: nothing is wrong with this copy, and this
           is not an alarm. Developer options show which one it was. */
        if (up->u.error == RC_UPDATE_ERROR_NETWORK)
            return rcStringFromCStr("The update server gave no usable answer. "
                                    "This version keeps working.");
        return rcFormat(a, "Could not check: %s",
                        up->u.message ? up->u.message : "no reason given");
    case RC_UPDATE_CANCELED:    return rcStringFromCStr("The check was stopped.");
    default:
        return up->later[0] ? rcFormat(a, "Version %s can wait until the next check.", up->later)
                            : rcStringFromCStr("Not checked yet.");
    }
}

/* The status line's color: an answer that says something is wrong with the update is the
   only one that takes the danger color. */
static inline RC_Color update_status_color(const Updater *up) {
    RC_Style s = rcGetStyle();
    bool     quiet = up->u.error == RC_UPDATE_ERROR_NETWORK ||
                     up->u.error == RC_UPDATE_ERROR_UNAVAILABLE;

    return up->u.state == RC_UPDATE_FAILED && !quiet ? s.danger : s.textMuted;
}

/* THE BANNER: nothing unless a newer build is offered or on its way, so the
   section is quiet when the app is current, while it checks and when the
   check fails. It is the offer as an update screen shows one: the version,
   what is new in it, and the buttons under them, the primary one last. A
   required update (the running version is below the manifest's
   minimumVersion) offers no Later. While the new build downloads the banner
   shows how far it has come and offers Stop; once it is in place, Restart. */
static inline void update_banner(RC_App *app, Updater *up) {
    RC_Style        s     = rcGetStyle();
    Palette         pal   = app_palette();
    RC_UpdateResult u     = up->u;           /* this frame's one read */
    RC_Arena       *a     = rcAppArena(app);
    bool            down  = u.state == RC_UPDATE_DOWNLOADING, ready = u.state == RC_UPDATE_READY;
    const char     *plain = control_class(), *primary = primary_class();

    if (u.state != RC_UPDATE_AVAILABLE && !down && !ready) return;
    rcColumn(.id = "update_banner", .gap = 12, .p = 16, .w = "grow",
             .className = "card bg-primary/10 border border-primary/45") {
        rcRow(.gap = 12, .align = "cl", .w = "grow") {
            rcBox(.bg = s.primary, .align = "cc", .borderRadius = "all-full", .wType = RC_PX(32),
                  .hType = RC_PX(32)) {
                rcIconDownload(16.0f, pal.accentInk);
            }
            rcColumn(.gap = 2, .w = "grow") {
                rcRow(.gap = 8, .align = "cl") {
                    rcTextC(ready ? "Update installed"
                                  : down ? "Downloading the update" : "Update available",
                            .font = rcFontSemibold((float)T_HEAD), .color = s.text);
                    if (u.required)
                        rcBox(.px = 8, .py = 2, .className = "rounded-full bg-warning/25") {
                            rcTextL("Required", .color = pal.warnInk, .size = T_CHIP);
                        }
                }
                rcText(rcFormat(a, "Version %s", u.version), .color = s.textMuted, .size = T_HINT);
            }
        }
        if (down) {
            /* u.size is above 0: a signed entry that installs carries it. */
            rcProgress("update_progress", (float)((double)u.received / (double)u.size));
            rcText(rcFormat(a, "%d of %d KB", (int)(u.received / 1024u), (int)(u.size / 1024u)),
                   .color = s.textMuted, .size = T_HINT);
        } else if (ready) {
            rcTextL("It is in this program's place, and runs when the app next starts.",
                    .color = s.text, .size = T_HINT);
        } else {
            if (u.notes[0]) rcTextC(u.notes, .color = s.text, .size = T_HINT);
            /* Why not Install, where this program cannot replace itself. */
            if (u.message)  rcTextC(u.message, .color = s.textMuted, .size = T_HINT);
        }
        rcRow(.gap = 8, .align = "cr", .w = "grow") {
            if (down) {
                if (rcButton("update_stop", "Stop", .className = plain)) updater_stop(up);
            } else if (ready) {
                if (rcButton("update_restart", "Restart now", .className = primary,
                             .variant = RC_BTN_PRIMARY))
                    updater_restart(app, up);
            } else {
                if (!u.required && rcButton("update_later", "Later", .className = plain))
                    updater_later(up);
                if (rcButton("update_go", u.action == RC_UPDATE_ACTION_INSTALL ? "Install" : "Open",
                             .className = primary, .variant = RC_BTN_PRIMARY))
                    updater_apply(up);
            }
        }
    }
}

#endif /* APP_UPDATE_H */
