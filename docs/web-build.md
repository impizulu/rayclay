# RayClay on the web (WebAssembly)

> **Scope: this page is about building RayClay's own bundled `examples/` into web pages.** The
> `examples` branch registers each one with `rc_web_example()` and supplies the page shell they link
> against. **Building *your own* app for the browser needs none of it**: that is a dozen link flags,
> given in full in
> [getting-started.md](getting-started.md#4-build-for-the-web-the-same-source).

**Status:** working. `cmake --preset web` builds the `examples/` programs into runnable web pages you can
host and open in any browser: the six bench/showcase apps, the decade walk, the widgets gallery, the
live inspector, the system monitor, the data explorer, the documentation reader, the issue inbox, the
SVG showcase, the C++ kanban board and the welcome canvas. **The target-to-page mapping is one
table in [examples.md](examples.md#build--run-any-example)**, which is where it is maintained; a second
copy here would drift the first time a page is added. Same source as desktop; the platform is selected
at build time, never forked.

Every one sits behind a hub landing page, `examples/web/hub.html`, which the build copies to
`index.html`, so a local build and the deployed site (GitHub Pages) have the same front door. See §2.

This is the practical "how do I build, host, and test RayClay as a web app" guide. For the *why*, see
§6 "How it works" below.

---

## 1. Prerequisites: the Emscripten SDK

The web build needs [emscripten](https://emscripten.org) (the C/C++ → WebAssembly toolchain). One-time install:

```sh
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install 6.0.2 && ./emsdk activate 6.0.2
source ./emsdk_env.sh            # sets $EMSDK + puts emcc on PATH (re-run per shell)
emcc --version                   # expect 6.0.2
```

`source emsdk_env.sh` exports `$EMSDK`, which the `web` CMake preset uses to find the toolchain file: no
hard-coded paths.

> **Pin the toolchain to a `6.0.x`.** `6.0.2` is the documented choice and `6.0.6` is what built
> every web target here. Nothing in the build enforces a version, so a drifting emsdk fails
> **silently** - it still compiles.
>
> **The one flag that matters: `-sGROWABLE_ARRAYBUFFERS=0`.** From 6.0.2 onward emcc turns growable
> array buffers on by default, which Chrome's WebGL rejects, so a page renders blank without it. It
> is already in every RayClay web target; `getting-started.md` §4 gives it for your own app.

## 2. Build the `examples/` into web apps

*(Needs a checkout that carries `examples/`, which the `examples` branch does.)*

```sh
cmake --preset web               # configures build-web/ with the emscripten toolchain
cmake --build --preset web       # compiles the library + every web-registered example to WebAssembly
```

> **Build via the preset, never `cmake --build build-web -j`.** The presets carry an explicit job count.
> A bare `-j` (or a bare `--parallel`) is an *unbounded* `make -j`: GNU make then spawns every job the
> dependency graph allows (not one per core), which is enough concurrent compilers to exhaust RAM and
> take the machine down with it. Pass a number if you drive the directory directly: `--parallel 4`.

Output lands in `build-web/`: one `<name>.{html,js,wasm}` set per example (and a `.data` only where an
example preloads assets), plus `index.html`, the hub. **One page per web-eligible target: every
`examples/ex*/` app except three** - `ex11`, the SVG→icon converter, because it does batch filesystem
I/O the Emscripten VFS does not model, and `ex30` and `ex32`, because the web is not a phone -
**plus the `examples/bench/` apps.**

**Do not trust a page COUNT written here; count the build:** `ls build-web/*.html | wc -l`. The set
grows whenever an example is added, and a number in prose cannot follow it. The bijection between
`examples/ex*/` and the web targets is enforced upstream, where a check also
refuses an exclusion that gives no reason.

| File | What it is |
|---|---|
| `dashboard.html` | the ex05 2020s dashboard, the showcase app (hosted in RayClay's shell page) |
| `hello.html`   | the ex00 welcome canvas (no assets) |
| `gui1980s.html` … `gui2010s.html` | the ex01–ex04 decade GUIs (no assets) |
| `kanban.html` | the ex06 kanban board, the worked C++ consumer: the same DSL from a C++20 translation unit (no assets) |
| `gallery.html` | the ex10 widgets gallery (`gallery.data` holds its one preloaded PNG at `/assets`) |
| `inspector.html` | the ex12 live inspector / test harness: split pane, live fps + frame-time charts, a table log (no assets) |
| `sysmon.html` | the ex20 system monitor: sortable virtualized process table, live charts, and a custom titlebar. **The band is drawn but inert here:** `rcWindowControlButton` and the `RC_ID_WINDOW_*` ids emit nothing on web, because the browser tab is the chrome |
| `explorer.html` | the ex21 data explorer: a 12,000-row catalogue behind filters, three linked charts and a sortable virtualized table (no assets) |
| `reader.html` | the ex22 documentation reader, the typography example: `reader.data` carries its three font faces at `/assets/fonts` |
| `inbox.html` | the ex23 issue inbox: 100,000 rows behind a live filter, with a rail and a detail pane (no assets). Its titlebar band is drawn but inert here, for the same reason as `sysmon.html` |
| `svg.html` | the ex24 SVG showcase: the vector routes side by side; `svg.data` carries the twelve `.svg` sources it reads at run time at `/assets/icons` |
| `messenger.html` `notes.html` `trader.html` `platformer.html` `photos.html` `opsdash.html` | the six bench/showcase apps, the same sources that serve as the perf benchmark. The bench image viewer ships as **`photos.html`** so it does not collide with ex10's `gallery.html` |
| `index.html`   | the hub, staged from `examples/web/hub.html` - a copy, never a rename. Hub links are checked against the built pages upstream, not by your build |
| `<name>.js`    | the runtime/loader glue for that page (instantiates the wasm) |
| `<name>.wasm`  | the compiled code (the library + that example) |

A wasm is **not** standalone: it needs its `<name>.js` (and `<name>.data` when present) beside it, so
always ship/serve the whole set. **Three pages ship a `.data`:** `gallery.html` (ex10's one preloaded
PNG), `reader.html` (ex22's three font faces) and `svg.html` (ex24's twelve
`.svg` sources). Everything else is fully procedural:
bundled font plus procedural icons, and the bench image viewer generates its images at runtime rather
than shipping any.

**Wasm size is a property of the BUILD TYPE, so measure yours rather than quoting a range:**
`ls -l build-web/*.wasm`. A `Release` build and a `MinSizeRel` build of the same page differ enough that
a number copied from the wrong one will mislead you about your own download.

> Driving the directory yourself needs the preset's cache variables spelled out; they are not cosmetic:
>
> ```sh
> emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=MinSizeRel -DRC_BUILD_TESTS=OFF \
>              -DRC_BUILD_EXAMPLES=ON
> cmake --build build-web --parallel 8
> ```
>
> **`RC_BUILD_TESTS=OFF` is what the `web` preset sets.** RayClay's CTest suite is a desktop suite a
> browser target has nothing to run, so turning it off keeps an emscripten build to the library and the
> examples. The flag is a choice, not a repair: an emscripten configure with tests ON succeeds, because
> the test targets that need the vendored GLFW are guarded on that target existing.

### The hub (the deployed landing page)

`examples/web/hub.html` is a plain, self-contained page (no external fonts, scripts or stylesheets) that
links every built demo. **The build copies it to `build-web/index.html`**, so the hub is the landing page both
locally and on the deployed site; local and deployed are the same tree, and a broken hub link fails where
you can see it rather than after a deploy.

**Page names are set at BUILD time** (`rc_web_example()` in `CMakeLists.txt`), never by renaming a file
afterwards. emcc bakes the wasm filename into the generated `.js`, so a copy-time rename leaves e.g.
`dashboard.js` still fetching `index.wasm` and the page dies with a blank canvas.

## 3. Host it locally (you can do this right now)

A wasm app **must be served over HTTP**: browsers block `wasm`/`fetch` on `file://`. RayClay is
single-threaded, so **no special COOP/COEP headers are needed**; any static server works:

```sh
# Option A: Python (the universal fallback, no extra deps)
cd build-web && python3 -m http.server 8080
#   then open http://localhost:8080/            (index.html is the hub; every app is linked from it)

# Option B: emrun (ships with emsdk; also used for headless CI smoke, §5)
emrun --no_browser --port 8080 build-web/dashboard.html
```

Open the URL and the dashboard renders in the canvas, redrawing **on demand**: input, resize and
device-pixel-ratio changes wake it, and an idle page draws nothing (see `RC_RenderMode`; the animation
frame stays registered, but a tick nothing asked for returns without laying out or rendering). Don't
benchmark the page or write a test wait expecting a steady 60 fps. Otherwise: the same widgets, fonts, gradients,
shadows and icons as the desktop build. **A local build emits `index.html` too**: the demo hub is copied
into `build-web/` at build time, so serving the directory and opening `/` lands on the same hub page the
public site does. Local and deployed are the same tree; there is no deploy-only step to get wrong.

## 4. Responsive layout: desktop / tablet / phone

RayClay draws the whole UI into **one WebGL canvas**, not a DOM tree, so the canvas fills the viewport and
the C side re-lays-out each frame. On any window or device resize the SDL3 host, which is the only
window backend the web build permits, reports the new canvas size and RayClay pushes it into the
layout, so the **layout flexbox re-flows** to the new viewport. (The logical window size stays CSS px;
the layout engine lays out and hit-tests there, while the WebGL backing store is sized to
`cssPx × devicePixelRatio`, so vector content rasterises at native device resolution; see §7.)

To test the responsive story:
- **In a browser:** open DevTools → device toolbar (Ctrl+Shift+M) → pick iPhone / iPad / a desktop size, or
  just drag the window; the layout adapts live.
- **Headless (Playwright):** `page.setViewportSize({width, height})` (or `devices['iPhone 13']`) +
  `toHaveScreenshot()` per viewport. Verified manually at 1366×768 (desktop two-column) and 390×844 (phone
  single-column): the top-level layout re-flows correctly.

> **Note, what does NOT apply:** because the UI is a single canvas (no DOM reflow), the web-vitals **CLS**
> (Cumulative Layout Shift) metric is N/A here. Don't run a CLS audit, it measures the wrong thing.

## 5. Testing the WASM build

The standard pyramid for a C/wasm canvas app, cheapest → richest. RayClay already owns the first three:

| Layer | Tool | Asserts | Cost to you |
|---|---|---|---|
| 1. Artifact validation | `wasm-validate` (WABT) | the `.wasm` is a valid module | one command against the file you just built - below |
| 2. Headless logic smoke | Node runs a GL-free build of your logic; exit code = pass/fail | your layout and widget code runs correctly on wasm32 | a second link target, no browser |
| 3. Browser smoke | a real browser drives the page | the *rendered* page lives: canvas sized, WebGL2 ok, no `abort()` | a headless browser in CI |
| 4. Pixel/visual diff | Playwright `toHaveScreenshot` per viewport | screenshot vs baseline | advisory (GPU/AA variance) |

**Layer 3 is the one that catches what the others cannot.** An exit code proves a page did not `abort()`; it
does **not** prove the page is not blank, and it does not prove the page is quiet. A build that compiles, exits
0, and paints an empty canvas passes layers 1 and 2, so the render check runs *before* the deploy, not after.
Two assertions are enough:

```js
canvas.width > 0 && canvas.getContext('webgl2') !== null   // it actually has a live surface
consoleErrors.length === 0                                 // and it is not screaming
```

Layer 1 is a single command against the artifact your build just produced, and it is the cheapest
thing in this table to wire into CI:

```sh
wasm-validate build-web/myapp.wasm    # silent, exit 0 when the module is valid
                                      # names the byte offset and exits 1 when it is not
```

Layers 1–2 prove the **logic** is byte-correct on wasm32 (no GPU). Layer 3 is the first tier that proves the
**rendered** app, and it is the only layer that can tell a live page from a blank one - which is why it
is the one worth building for your own app too.

### Seeing RayClay's diagnostics in the browser

RayClay reports mistakes it can detect at runtime (a bad sizing unit, an unparsable `.align`, a `break`
out of an element body) straight to the browser console, and **routes them by level**:
`console.error` for errors, `console.warn` for warnings, `console.log` for info. Each line carries a
`RAYCLAY[LEVEL]: ` prefix. **Open devtools and they are there** - no shell wiring required.

**You do not need a `printErr` hook for these.** RayClay calls the console directly, so a shell that
omits `printErr` still shows them and a shell that defines it does not capture them. Routing by level
is why the zero-config `return rcRunApp(NULL)` program, which warns by design that no layout callback
was supplied, shows a yellow warning rather than a red error.

To capture diagnostics yourself - to a panel, a buffer or your own telemetry - install a sink with
`rcSetLogSink(sink, user)`. A sink takes precedence over the console on every platform, and it
receives the structured `RC_LogLevel` plus the bare message, so you own both the prefix and the
destination.

The practical failure mode is not invisibility; it is simply
**not having devtools open**, which is worth ruling out before you conclude a warning "doesn't fire on
web".

`rcSetLogSink(fn, user)` is for sending diagnostics *somewhere else* (an in-page log panel, a toast,
telemetry), not for making them visible in the first place. While a sink is installed, `stderr` is not
written, so a sink **replaces** the console output rather than adding to it; call `rcSetLogSink(NULL, NULL)`
to restore it.

## 6. How it works (the three things that make web possible)

1. **Loop inversion.** A browser tab can't run a blocking `while` loop. `rcRunApp` is built from
   `rcAppCreate` / `rcRunFrame` / `rcAppDestroy`; on `__EMSCRIPTEN__` it hands `rcRunFrame` to
   `emscripten_set_main_loop` (requestAnimationFrame) instead of looping. The same `main.c` is unchanged.
   **`rcRunApp` therefore does NOT return on web**: `emscripten_set_main_loop` unwinds the C
   stack and the browser drives the frames. Code you write after `rcRunApp` in `main()` runs on
   desktop and never in a browser, and there is no end-of-run hook to move it into: the run ends
   inside the browser loop and RayClay destroys the app there. `frameEndCallback` is not that hook -
   it fires after every presented frame - so release as you go and let process exit be the teardown.
2. **Build-time platform select.** The root CMake skips desktop OpenGL / X11+nobar under
   `if(EMSCRIPTEN)`; sokol uses its **GLES3 → WebGL2** backend, and the example links with `-sFULL_ES3`,
   `-sSTACK_SIZE=8MB` (the layout recurses), asset preload, and our own shell page. **No windowing flag
   is passed**: SDL3 arrives through the `rayclay` target's PUBLIC link like any other dependency.
3. **One window backend, two targets.** The SDL3 backend doubles as the web backend through SDL3's
   first-party emscripten port; the few desktop-only calls (window centring, resize cursors) are gated
   out under `__EMSCRIPTEN__`.

## 7. Known limitations (web)

- **No native window chrome.** Titlebars are desktop-only: nobar compiles out, the bundled titlebar is
  not drawn on web, and `rcTitlebar`/`rcWindowControls`/`rcWindowControlButton` emit nothing there.
  An example's hand-rolled bar still renders as ordinary layout, and every `RC_ID_WINDOW_*` verb,
  close included, is inert (the page chrome is the browser's). This is correct: `nativeFrame` has no
  meaning in a tab.
- **The clipboard works on the web out of the box: copy and paste both, with no setup.** RayClay's web
  backend is the browser's own clipboard: a copy calls `navigator.clipboard.writeText`, and a read calls
  `readText()` and resolves that promise back through the token protocol. It deliberately does NOT
  go through the window host: SDL has no emscripten clipboard driver, so that seam does not exist to
  be used. RayClay ships that path; you do not wire it up. You read with `rcClipboardRequest` +
  `rcClipboardPoll`, or let the bundled text field do it.
  **`rcClipboardGet` returns `NULL` on the web, and that is not a failure signal.** It can only answer
  under a *synchronous* backend, and a browser read is a promise. **Do not feature-detect the clipboard with
  it**: you will report "no clipboard" on a platform where the clipboard works perfectly.
  **Two browser rules travel with this, and neither is RayClay's to relax.** The Clipboard API exists only
  in a **secure context** (https, or `localhost`), so a page served over plain http to a LAN address has
  none at all. And a **read** additionally needs a user gesture or a granted permission, so issue it from
  inside a click handler rather than at startup. When the API is absent or the read is refused, RayClay
  **delivers a denial** instead of throwing: the paste resolves as "no text" and a UI waiting on it stops
  waiting. Through the public API an empty clipboard and a refused read are indistinguishable - **but
not in the log**: an absent Clipboard API and a refused read or write each warn once, naming the
cause. When copy or paste "does nothing", read the diagnostics before suspecting your own code.
  You can still install your own `RC_ClipboardImpl` via `rcSetClipboardImpl()` to **override** the default:
  the struct is copied **by value**, so a stack local may go out of scope; only `user` stays borrowed. Its
  `request(user, token)` starts your read and calls `rcClipboardDeliver(token, text)` when it resolves,
  echoing the same token back.
  **You do not have to wake the app yourself, with the default backend or your own.** Under
  on-demand rendering an app can be parked when the promise resolves, which would leave a correct delivery
  invisible until the user happened to move the mouse. `rcClipboardDeliver` therefore requests a frame as
  part of delivering, including on the denial path, so a UI waiting on the answer is woken to observe a
  refusal too. A custom `request` callback only has to call `rcClipboardDeliver(token, text)`; echo the
  token back and RayClay does the rest. (The hook lives from `rcAppCreate` to `rcAppDestroy`, so it covers a hand-rolled `rcRunFrame`
  loop as well as `rcRunApp`.)
- **A lost WebGL context needs a page reload.** If the browser drops the GL context (a GPU
  reset, or the tab backgrounded/starved too long), RayClay cannot repaint: the textures it uploaded
  are gone from the GPU and the CPU-side image bytes were freed after upload. The page shell shows an
  honest "reload to continue" card; RayClay does not restore the context automatically, so the reload is the recovery.
  Desktop has no equivalent: the GL context lives with the window.
- **Antialiasing is a request on desktop and a *hint* on web, and the browser may simply decline.**
  Desktop GL negotiates a sample **count**, so RayClay asks for `RC_GFX_MSAA_SAMPLES` and compares what
  it got. On the web there is no count to negotiate: it reaches WebGL as the **boolean** `antialias`
  context attribute, and the spec explicitly lets an implementation ignore it. **An outright refusal is reported**: one warning, in the console, alongside every other
  RayClay diagnostic (see *Seeing RayClay's diagnostics in the browser* above).
  **Silence on web means "some antialiasing", not "four samples".** A browser that gives you 2 where
  you asked for 4 stays quiet on purpose: the count there is the browser's choice, so treating it as a
  disagreement would fire for your users and never for you.
  **What it means for your UI:** the fallback is legible, not broken; rounded corners, chart lines
  and icon strokes are harder-edged. **Text is unaffected on every platform** (it is sampled from an
  already-antialiased atlas). Design so 4× edges are a polish layer rather than the thing that makes
  the screen readable, and the web fallback costs you nothing you cannot afford.
- **Zoom is the browser's, and your page has to let it through.** The desktop zoom *gestures*
  (Ctrl +/-/0/wheel) are inert on web: the browser owns page zoom in a tab. **Ctrl+wheel and trackpad
  pinch reach the browser only because the shell page exempts them** - the window host accepts every
  wheel over the canvas and calls `preventDefault()` on it, so a custom `--shell-file` without that
  exemption zooms nothing at all, neither the page nor the app. The four lines that fix it are in
  [getting-started.md ▸ The page around the canvas](getting-started.md). `rcWindowSetZoom()` still
  applies programmatically throughout, clamped to the app's configured `RC_AppOptions.zoom`
  `[minZoom, maxZoom]` range and honouring `.mode` (a reflow in the default layout mode), and
  `rcGetContentScale()` returns the display's true device-pixel-ratio there (e.g. `2.0` on a 2× display).
- **But `.zoom.pan` *is* live on web, and the asymmetry is deliberate.** Drag-to-pan (hold Space, drag
  with the left button) behaves exactly as it does on desktop. The zoom *keys* are excluded here because
  the browser already owns `Ctrl` `+`/`-`/wheel and intercepting them would double-zoom; **no browser owns
  a space-drag**, and `rcWindowSetZoom()` + `RC_ZOOM_OPTICAL` do work on web, so excluding the pan would
  hand a web build a magnified view with no way to reach its own edges, which is the one-source promise
  breaking on the platform least able to work around it.
- **HiDPI renders at native device resolution.** The WebGL backing store is sized to `cssPx ×
  devicePixelRatio` (SDL's logical window size stays CSS px, where the layout engine lays out and hit-tests), so all vector
  content (borders, icons, rects, components) rasterises at the display's true device resolution and
  **re-crisps on a browser page-zoom** instead of being bilinearly upscaled from one 1× bitmap. Glyph text
  follows it: `contentScale` carries the browser's device-pixel-ratio *and* its page zoom, so once that
  settles the runner re-bakes the glyph atlas at the new density and settled text is native-resolution here
  exactly as on the desktop, bounded by the atlas's capacity, where the re-bake settles on the highest
  density the sheet can hold (the same one however fast you drove the zoom) and text goes slightly soft
  rather than missing. Only the few frames before the re-bake lands show the
  previous bake magnified; `RC_AppOptions.fontOversample` smooths that transient, and also replaces the
  automatic oversampling policy, at a real cost in zoom ceiling (see
  [getting-started](getting-started.md)). **The window host's own HiDPI path is what drives this on the
  web**: RayClay asks for high pixel density on every window, so the backing store is
  CSS px x `devicePixelRatio` while the logical window size stays CSS px. Measured on the web build: a 1280 CSS-px canvas at DPR 2 gives a
  2560 px backing store.
- **The same code serves desktop and web**, which is the point of the seam rather than a coincidence:
  one backend, one HiDPI flag, and the browser reports its device-pixel ratio through the same call the
  desktop uses. There is no web-specific DPI path left to reason about.
