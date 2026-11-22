# Getting started

RayClay is a C framework for building complete apps: flexbox-style layouts, Tailwind-style
classes, widgets and networking built in. Include `rayclay.h`; one C source builds for macOS,
Windows, Linux, iOS, Android, and the web. Ship RayClay's `third_parties/LICENSES.txt` with your
app: it holds every bundled library's notice.

## Your first app

Build it as [desktop.md](desktop.md), [mobile.md](mobile.md) or [web.md](web.md) shows.

<!-- compile: c -->
```c
#include "rayclay.h"

typedef struct { int clicks; } AppState;

static void layout(RC_App *app, void *user) {
    AppState *st = (AppState *)user;
    rcColumn(.className = "flex-1 gap-3 p-6 bg-background") {
        rcTextL("Hello, RayClay", .className = "text-2xl");
        rcText(rcFormat(rcAppArena(app), "Clicked %d times", st->clicks));
        if (rcButton("inc", "Click me", .variant = RC_BTN_PRIMARY)) st->clicks++;
    }
}

int main(void) {
    static AppState state;
    RC_AppOptions opts = {
        .width = 480, .height = 320, .title = "Hello",
        .layoutCallback = layout,
        .userData = &state,
    };
    return rcRunApp(&opts);
}
```

- `rcRunApp` opens the window and returns when it closes: 0, or non-zero on failure. On the web it
  never returns: put nothing after it.
- Your state is your struct, handed to every callback through `.userData`. A control's value (a
  checkbox's `bool`, a field's buffer) lives there too, by pointer.
- `.layoutCallback` declares the UI; the optional `.updateCallback` runs before it each frame.
- Load or parse slow data in `.updateCallback` on the second frame, as ex27 does (the layout
  marks the first drawn and asks for another), never before `rcRunApp`.
- `rcRunApp(NULL)` opens a welcome canvas: if none appears, suspect your toolchain or graphics
  driver.
- `rcAppCreate`, `rcRunFrame` and `rcAppDestroy` are `rcRunApp` taken apart, for a host owning its
  loop and pacing; only `rcRunApp` runs unchanged on the web.
- RayClay warns once per mistake in the platform's log (stderr, the browser console, logcat, the
  iOS unified log); `rcSetLogSink` sends it elsewhere.

## How a frame works

- **The whole UI is declared every frame**, top to bottom, like a React render.
- **An id names an element across frames**, as a React `key` does: hover, focus, a caret and a
  scroll position stay with it. Build it from the data
  (`rcFormat(rcAppArena(app), "row%d", item->key).chars`), never from a value that shifts. Two
  elements with one id share one entry; only one gets clicks.
- **Never jump out of an element body.** The `{ }` after `rcRow`, `rcColumn` or `rcBox` is a loop
  the macro writes: `break` and `continue` end only that body (`break` warns), and `return` or
  `goto` leaves the element unclosed. To stop your own loop early, test a flag in its condition.
- **RayClay draws only when something happens**: input, a resize, its timers, an answer to its
  request; otherwise the app sleeps. When state RayClay cannot see changes (a countdown, a worker's
  result), ask for a frame, as `requestAnimationFrame` does:
  - `rcWindowRequestFrame(rcAppMainWindow(app))`: one more frame; calls coalesce.
  - `rcWindowRequestFrameAfter(rcAppMainWindow(app), 1.0)`: one frame in a second. Only the
    earliest pending request is kept: re-arm a later one when it fires.
  - `rcAppWake()`: from another thread, after handing its result over (an atomic, or a queue under
    your lock). Make every other call on the main thread unless it says otherwise.
  - `.renderMode = RC_RENDER_CONTINUOUS`: draw every display refresh, for a game or simulation.

## Strings and memory

- **Text and ids you pass are borrowed until the frame is drawn**, after your layout callback
  returns: give `rcText`, `rcTextC` and `.id` a literal, storage you own or `rcFormat` text, never
  a callback's local array. A `.tooltip` is copied when declared.
- `rcFormat(rcAppArena(app), ...)` writes into the frame arena, valid until the next frame: 16 KiB,
  or the size `.scratchArenaBytes` sets. A full arena returns `""`.
- Strings RayClay hands you stay RayClay's: a response body until `rcFetchFree`, dialog paths until
  the next `rcFileDialogOpen`. Copy what you keep.

## Doing more: start, then poll

Networking, media, file dialogs and updates return a token at once, read every frame: no
callbacks, no threads.

<!-- compile: c -->
```c
static RC_Fetch quote;                            /* 0 = not asked yet */

static void quote_view(void) {
    if (!quote) quote = rcFetch("https://api.example.com/quote");
    RC_FetchResult r = rcFetchResult(quote);
    if (r.state == RC_FETCH_PENDING)      rcTextL("Loading...");
    else if (r.state == RC_FETCH_FAILED)  rcTextC(r.message);
    else                                  rcTextC(r.ok ? r.text : "The server sent an error.");
    if (rcButton("reload", "Reload")) { rcFetchFree(quote); quote = 0; }
}
```

| Work | Start | Read every frame | Let go |
|---|---|---|---|
| HTTP | `rcFetch` | `rcFetchResult` | `rcFetchFree` |
| WebSocket | `rcWebSocket` | `rcWebSocketNext`, `rcWebSocketState` | `rcWebSocketFree` |
| Audio, video | `rcMediaOpen` | `rcMediaRead` | `rcMediaClose` |
| File dialog | `rcFileDialogOpen`, `rcFileDialogSave` | `rcFileDialogPoll` | `rcReleaseFile` |
| Update check | `rcUpdateCheck` | `rcUpdateResult` | `rcUpdateFree` |

- Start behind `if (!token)` or on an event, never unconditionally in a layout. `rcFileDialogOpen`
  needs a user action: call it the frame a button is clicked.
- A token is never 0; work RayClay refuses still gets one, already answered with the reason.
- A response of any status reads `DONE`; `ok` means 200-299. `FAILED` means no usable one.
- `rcFileDialogPoll` hands each answer over once (later polls read `NONE`): keep what it returns.
  `RC_FILE_DIALOG_OPENED_BY_OS` polls deep links and Open With ([mobile.md](mobile.md)).
- The app holds at most 64 requests and 16 sockets (one it freed stops counting at the free) and
  16 media at once; past that a token reads `UNAVAILABLE`, or `INVALID` for a request
  `rcFetchCheck` refuses. Six buffered requests run at once, beside up to 16 streams; the rest
  wait in line, their time limits running.
- No request answers from an HTTP cache. A native one sends `User-Agent: RayClay/<version>` unless
  you set one, no `Cookie`, and hides `Set-Cookie` (the web's uses the page's), so a cookie-only
  server needs an adapter. Carry a session in a
  header such as `Authorization`, which every host
  sends: `.headerPairs = (RC_FetchHeaderPair[]){{"Authorization", key}, {0}}`. `{0}` ends that
  list; `NULL` ends `.headers` and `.protocols`.
- Unlike `fetch()`, a buffered request fails `TIMEOUT` after 30 s and `TOO_LARGE` past 8 MiB unless
  `.timeoutSeconds` or `.maxBytes` says otherwise, and a native request whose method is not GET or
  HEAD is never resent when its kept-alive connection ends before any reply: it fails `NETWORK`.
- A download to a file: a `.stream = true` request with `.idleSeconds` (a stream has no total
  timeout); each frame, before reading its state, `rcFetchRead` into `fwrite` until
  `length` is 0. The file is whole only on `DONE` and `ok`. Time suspended counts toward
  `.idleSeconds`: resume a failed download with a `Range` header.
- Linux, Android and Windows speak HTTP/1.1 only, a connection per request in flight; macOS, iOS
  and the web speak what the system or the browser negotiates, HTTP/2 included.

## Not in v1.0

- Accessibility, system text size, ruby text, `rotate`, `scale`, `position: sticky` and scroll
  snapping: none.
- Secure storage (Keychain, Keystore), notifications, purchases and platform sign-in: the
  system's API.

## Where to go next

- [api.md](api.md): every call.
- [ui.md](ui.md): layout, classes, colors, fonts, images and controls.
- [desktop.md](desktop.md), [mobile.md](mobile.md), [web.md](web.md): building and platform duties.
- [examples.md](examples.md): complete programs to copy.
