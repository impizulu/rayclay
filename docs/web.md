# Web

The same source builds a page (`myapp.html`, `.js`, `.wasm`, `.data`) that draws into one WebGL 2
canvas.

## Build and serve

Install the Emscripten SDK (`emsdk install 6.0.2`, `emsdk activate 6.0.2`,
`source emsdk_env.sh`), then add to your target ([desktop.md](desktop.md)).

```cmake
if(EMSCRIPTEN)
    set_target_properties(myapp PROPERTIES SUFFIX ".html")
    target_link_options(myapp PRIVATE
        -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=8MB -sENVIRONMENT=web
        "SHELL:--preload-file \"${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets\""
        "SHELL:--shell-file \"${CMAKE_CURRENT_SOURCE_DIR}/web/page.html\"")
endif()
```

```sh
emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-web --parallel 4
python3 -m http.server 8080 --directory build-web   # open localhost:8080/myapp.html
```

- Linking `rayclay` (or `rayclay_impl_deps`) adds the WebGL 2 flags; never override its
  `-sGROWABLE_ARRAYBUFFERS=0`.
- Serve the files over HTTP (never `file://`) with `Cache-Control: no-cache`, so a reload never
  mixes two builds.

## Assets and the page

Assets reach the page's file system only when packaged at link time. With the `--preload-file` line,
`"assets/logo.svg"` opens on the web as on the desktop; relink when an
asset or `page.html` changes. Serve large media and sounds beside the page, by relative URL.

RayClay draws into `#canvas`, sized by its CSS box. A minimal page (`examples/web/shell.html` is
fuller):

```html
<!doctype html>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  html, body { margin: 0; height: 100%; overflow: hidden; }
  #canvas { display: block; width: 100vw; height: 100vh; height: 100dvh; }
</style>
<canvas id="canvas" tabindex="0" oncontextmenu="event.preventDefault()"></canvas>
<script>
  document.getElementById('canvas').addEventListener('webglcontextlost', function () {
    document.body.textContent = 'Reload the page to continue.'; });
</script>
{{{ SCRIPT }}}
```

## What the browser decides

- **Fetch** is the browser's `fetch()`: a relative URL (web only) resolves against the page, and
  another origin must allow yours (CORS). Offline, a host that does not resolve, TLS, CORS and mixed
  content all read `RC_FETCH_ERROR_NETWORK`: the browser names no cause. It
  sends its own User-Agent, not yours. Chrome opens six connections to one HTTP/1.1 server,
  shared with your other tabs, and an open `.stream` holds one: keep streams to such a server under
  six, or a further request waits and times out (HTTP/2 servers are not bound so). A request whose
  kept-alive connection ends before any reply is sent again by the browser, whatever its method:
  give a request the server must not apply twice a key the server checks.
- **WebSocket** sends no header of yours (one fails with `RC_WEBSOCKET_ERROR_INVALID`): carry a
  token in the query, or in `.protocols` as an HTTP token the server echoes. Unread messages past
  `.maxUnreadBytes` fail the socket.
- **Media** plays what the browser plays: give `.alt` a second encoding. Another origin needs
  CORS. A sound or audible media played outside a click may read `RC_MEDIA_PAUSED` with
  `RC_MEDIA_ERROR_BLOCKED` and never start alone: play from a click, or open the media `.muted`.
- **Storage:** `rcAppDataPath` lives in IndexedDB, shared by every RayClay app on the origin, and
  is `NULL` without it. A file is stored once closed, created, removed, renamed or truncated, as
  `rcSavePoll` reports; between tabs, the last save wins. `rcAppCachePath` is memory only, empty
  at every load.
- **Files** opened or dropped are copies in memory: free each with `rcReleaseFile`.
  `rcFileDialogSave` is a download.
- **Clipboard** needs https or localhost, and a read a user gesture. A read answers a frame or
  two later: keep `rcClipboardRequest`'s token, polling while `rcClipboardPoll` reads PENDING.
- **A hidden page** takes one turn at the hide, then draws a frame only when networking or media has
  news or a time limit comes, at most once a second: time limits fire, up to about two seconds late,
  and a socket the layout reads stays open while it receives under about a sixth of
  `.maxUnreadBytes` a second. A page opened in a background tab runs nothing until it is first
  shown. Save on `RC_APP_EVENT_SUSPENDED`. Closing or reloading sends `RC_APP_EVENT_TERMINATING`; a
  save there past 5 MiB may be lost. Before it, the page's `beforeunload` sends
  `RC_APP_EVENT_CLOSE_REQUESTED`: `rcWindowCancelClose` there has the browser ask the user, in its
  own words and only once the page was used; start no quit flow. A lost GPU context sends
  `RC_APP_EVENT_CONTEXT_LOST`, then the app ends: save there.
- **The tab is the window:** `rcAppOpenWindow` returns `NULL`; titlebar calls draw nothing.
- **Tab** past the last control (outside a dialog) leaves the canvas, one stop (`tabindex="0"`) in
  the page's order; Tab back in focuses the first control, Shift+Tab the last.
- **Updates:** check `manifest.json` by a relative URL; its `web` entry is kind `reload`.
