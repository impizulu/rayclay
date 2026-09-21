# Getting started with RayClay

Build a GUI in C and ship it on every desktop OS **and** the web, from one source.

> **Coming from React, Vue, or Tailwind?** Read [Coming from the web](for-web-developers.md); it maps
> components / props / state / utility classes onto RayClay and flags the one C rule (string lifetimes)
> that trips up JS developers.

> **What you have in front of you, so a cross-reference never sends you looking for a file you were
> never given.** The RayClay drop is `rayclay.h` - the whole library, one header - plus these nine
> pages and the two vendored window hosts. **It does not include `examples/`.** These pages name
> example programs often, because they are where a pattern is worked end to end, and a name like
> `examples/ex20_system_monitor` is a pointer into a separate checkout rather than a path beside
> this file. Every page is written to answer its own question without one; where an example is the
> only place a full pattern lives, that is a gap in the page and worth telling us about.

## 1. The whole app, in two lines

```c
#include "rayclay.h"

int main(void) { return rcRunApp(NULL); }
```

That opens a window with the welcome canvas on the RayClay-dark background: a small "RC" mark, a
centred **"Welcome to RayClay"** with a muted version chip beside it, **"This is your blank canvas."**,
and the one line that tells you what to edit next (`Set RC_AppOptions.layoutCallback to draw your own
UI.`). No renderer wiring, no asset files (the font is bundled), no `#ifdef`. This program is checked in as
[`examples/ex00_hello/hello.c`](https://github.com/impizulu/rayclay/blob/examples/examples/ex00_hello/hello.c).

It also writes one line to stderr, and that line is not a mistake on your part:

```
RAYCLAY[WARNING]: rcRunApp: no layout callback supplied - rendering the built-in welcome canvas.
Set RC_AppOptions.layoutCallback to draw your own UI.
```

The canvas is a **stand-in for the UI you have not written yet**, and RayClay says so rather than
quietly producing content you did not author. It goes away the moment you set `.layoutCallback`.

> **`rcRunApp` is the entry point this guide uses throughout, and you have already met it.**
> Everything that follows configures it by passing options instead of `NULL`, and nothing above
> needs rewriting when you outgrow it. **One lower-level way in exists and this page needs it not
> at all: `rcAppCreate` / `rcRunFrame` / `rcAppDestroy`, for a host that must own the frame loop.**
> `rcInitWindow` is not a second one - it mints an ownerless L1 window and an `rcRunApp`
> application must never call it.
> Because this program contains none of *your* code, it also separates a broken toolchain from a
> broken layout: if this window does not appear, the problem is your compiler, linker, GPU driver
> or emsdk.

## 2. Add RayClay to your build

> **You need CMake 3.21 or newer and a C99 compiler.** The floor is RayClay's own
> `CMakeLists.txt`; an older CMake stops at `add_subdirectory` and names that line. Declaring a
> lower `cmake_minimum_required` in your project does not lower it; it only understates what
> your project needs, so match it at 3.21.

```cmake
add_subdirectory(rayclay)                      # vendored (git submodule or copy)
add_executable(myapp main.c)
target_link_libraries(myapp PRIVATE rayclay)   # window host, sokol, Clay, stb come with it
```

One link line pulls in everything. **`FetchContent` works today**: `FetchContent_MakeAvailable()` resolves
to exactly this `add_subdirectory`, so you can pull RayClay straight from a git tag without vendoring it.

> **If you took the single-header drop instead, know what it is: declarations, and nothing else.**
> The distributed `rayclay.h` is *generated* - around 94,000 lines and 3.8 MB of amalgamated source -
> and the amalgamator strips prose by design. **The load-bearing figures are the zeros, and they are
> exact:** the artifact a consumer receives holds **0 `/**` blocks, 0 `@param` and 0 `@brief`**,
> where the source header it was built from carries three hundred doc blocks. The line and block
> counts drift every time the library grows, so re-derive rather than quote them -
> `grep -c '/\*\*' rayclay.h` on either file answers it in one command, and that command is the one
> to trust over any number written in prose, including this one.
> So opening the shipped file and finding no comment beside a function is not a gap in the
> documentation; it is the file doing what it is meant to. **`docs/` is the reference, and the header
> is not a substitute for it.** Two practical consequences:
>
> - A rule that has to survive into the artifact is written as a `#error` or `static_assert` MESSAGE,
>   or into an IDENTIFIER. Those are string literals and names, and they survive; a comment does not.
> - **A name is your only in-file signal.** `RC_`/`rc` is the public surface; `rci_`/`RCI_` is
>   internal, and a handful of those carry `RC_API` only so the public macros have something your
>   translation unit can link - see [api-notes](api-notes.md) under `rcComponent`. Being linkable is
>   not an invitation, and the comment that says so is one of the ones that got stripped.

> **Setting a build knob? It goes on the RayClay target, not on yours, and putting it on yours is
> silent.** With `add_subdirectory`/`FetchContent`, RayClay compiles its implementation in its own
> generated translation unit; your `main.c` gets declarations only, so a define on *your* target is
> read by nothing and you get no warning:
>
> ```cmake
> target_compile_definitions(rayclay PRIVATE RC_NO_BUNDLED_FONT=1)   # reaches the implementation
> target_compile_definitions(myapp   PRIVATE RC_NO_BUNDLED_FONT=1)   # silent no-op
> ```
>
> Verified by building both ways: the wrong placement produced a **byte-for-byte identical** binary,
> the right one a smaller one. Three knobs invert this rule: `RC_NO_UI_HELPERS`, `RC_NO_STYLE`,
> `RC_NO_COLOR_PALETTE` trim *your* view and must **not** reach the implementation.
> → **[api-notes.md ▸ Build knobs: where to define them](api-notes.md#build-knobs-where-to-define-them)**

> **The inverse also holds, and this direction is not inert: put compiler settings on a TARGET, never
> on the directory.** A bare `set(CMAKE_C_...)` before `add_subdirectory`/`FetchContent_MakeAvailable`
> is inherited by RayClay's own subdirectory and lands on its target. RayClay pins its own
> `C_STANDARD`, but a variable it does not pin is yours by default, so your line silently changes how
> the *library* is compiled:
>
> ```cmake
> set(CMAKE_C_EXTENSIONS OFF)                                  # reaches RayClay too
> set_target_properties(myapp PROPERTIES C_STANDARD 99)        # yours alone
> ```
>
### Choosing the window host: `RC_WINDOW_BACKEND`

RayClay talks to the operating system through one seam, and there are two implementations of it.
The knob is a cache variable, so it goes on the configure line rather than in your source:

```bash
cmake -B build-desktop                            # GLFW, the desktop default
cmake -B build-sdl -DRC_WINDOW_BACKEND=SDL3       # the SDL3 host instead
```

| Target | Host | Why |
|---|---|---|
| Linux, macOS, Windows | **GLFW** by default, SDL3 on request | GLFW is the smaller of the two: SDL3 adds roughly 1.8 MB of code to the binary |
| Android, iOS, the web | **SDL3**, and only SDL3 | GLFW has no port to these; the knob is forced and a different value is a configure error |

**Your code does not change either way.** The seam is below the public API: the same source, the
same `rcRunApp`, the same behaviour. Pick SDL3 on the desktop when you want one host across desktop
and mobile in a single build system, or when you need something SDL3 carries and GLFW does not;
otherwise the default is the smaller binary. The one thing to know before mixing your own SDL code
with RayClay on that host is that `rcRunApp` owns the SDL event queue:
**[`api-notes.md` > Several windows, one app](api-notes.md#several-windows-one-app)**.

> Measured, because this one is easy to dismiss as harmless hygiene: with `CMAKE_C_EXTENSIONS OFF` the
> implementation compiles `-std=c99` instead of `-std=gnu99`, `CLOCK_MONOTONIC` is not declared, and
> **`rcProcessCpuPercent()` returns `-1.0f` on every call**: 99.66 became -1.00 on the same source
> with only `-std` changed. Nothing warns: it still compiles, links and runs.

> **Including `rayclay.h` from C++ needs C++20; from C, C99 is enough.** The vendored Clay refuses
> anything older with `#error "Clay requires C99, C++20, or MSVC"`, and it does so for a
> *declarations-only* include, so a C++17 project fails at the `#include`, not at the link, and the
> message names a dependency you had no reason to know about. Measured on g++ and clang++ at
> `-std=c++17` (refused) and `-std=c++20` (clean). MSVC is exempt by that same condition.

There is deliberately **no `install()` / `find_package()` path**, and that is a decision rather than a gap:
every dependency (Clay, sokol, stb, GLFW, SDL3, nobar) is vendored, so a correct `install(EXPORT)`
would have to re-export that whole closure. Embedding the source in your build is the supported model.

## 3. Build & run on the desktop

```bash
cmake -B build-desktop
cmake --build build-desktop
./build-desktop/myapp           # Windows (multi-config): .\build-desktop\Debug\myapp.exe
```

> **`myapp` and RayClay's own bundled examples are two different builds.** The two lines above build
> `myapp`. To build the examples that ship with a RayClay checkout, run the same shape *inside that
> checkout*: `cmake -B build && cmake --build build`, or `cmake --preset desktop`, which a RayClay
> checkout defines for exactly that. Your own project gets the two portable lines above unless you
> write presets of your own.

> **Linux system packages.** The window host needs X11/Wayland development headers, and this is
> true of **either** host - the default GLFW and the optional SDL3 both build against them. On
> Debian/Ubuntu: `sudo apt install libwayland-dev libxkbcommon-dev xorg-dev`. On other distros
> install the equivalent Wayland + X11 dev packages. macOS and Windows need only the compiler
> toolchain.
>
> Vendoring the source removes the *fetch* step, not the platform's dev headers: RayClay ships the
> host's code, and that code still compiles against your distribution's Wayland and X11 headers.

## 4. Build for the web: the *same* source

Your `main.c` is byte-for-byte the same on desktop and web; the loop is inverted internally for the
browser, with no `#ifdef` in your code.

> **On web `rcRunApp` does NOT return.** It hands the frame to `emscripten_set_main_loop`, which
> unwinds the C stack and lets the browser drive from there. ⇒ **anything you write after `rcRunApp`
> in `main()` runs on desktop and never on web.** There is no teardown hook to move it into:
> `frameEndCallback` runs after every *presented frame*, not once at the end, and the run ends
> inside the browser loop where RayClay destroys the app itself. So release what must be released as
> you go, and let process exit be your teardown. This is the one place where
> "byte-for-byte the same source" and "the same control flow" are not the same claim. What changes is the **link**: a browser target needs
emscripten's WebGL2 flags, and it has to emit a *page* rather than an
executable. Add that to the `myapp` target from step 2; it is the whole web-specific diff:

```cmake
if(EMSCRIPTEN)
    set_target_properties(myapp PROPERTIES SUFFIX ".html")   # emit myapp.html, not a binary
    target_link_options(myapp PRIVATE
        -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3
        -sALLOW_MEMORY_GROWTH=1 -sGROWABLE_ARRAYBUFFERS=0
        -sSTACK_SIZE=8MB -sENVIRONMENT=web -sMALLOC=emmalloc)
endif()
```

> **`-sGROWABLE_ARRAYBUFFERS=0` is load-bearing, not tuning.** emcc 6.0.2 turned it *on* by default
> alongside `ALLOW_MEMORY_GROWTH`, which backs the heap views with a resizable `ArrayBuffer`, and
> Chrome's WebGL rejects those (`texSubImage2D: ... must not be resizable`). The glyph-atlas upload
> then faults and **the page renders blank while the build stays green.** Pin it off.

Then configure with emscripten's wrapper and build:

```bash
emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-web --parallel 4      # -> build-web/myapp.{html,js,wasm}
```

Host the output on any static server (RayClay is single-threaded, so there are **no COOP/COEP
headers** to configure):

```bash
python3 -m http.server 8080 --directory build-web   # open http://localhost:8080/myapp.html
```

### Assets on the web: the browser has no filesystem

Every RayClay call that takes a path (`rcLoadFont`, `rcLoadImage`, `rcLoadSvg`, `rcSvg`) opens that
path through emscripten's **virtual** filesystem. Nothing from your disk is there unless you packaged
it at link time, so a font or icon that loads on desktop resolves to "no such file" in the browser.

Package one entry per file, and give each its virtual path explicitly:

```cmake
if(EMSCRIPTEN)
    target_link_options(myapp PRIVATE
        "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets/Inter.ttf@/assets/Inter.ttf"
        "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets/logo.svg@/assets/logo.svg")
endif()
```

Then open `/assets/Inter.ttf` in your code. Choosing the same virtual path on both platforms is what
lets one string serve desktop and web with no `#ifdef`; it is how `ex22` loads its three faces.

> **The `SHELL:` prefix and the single-string form are both load-bearing.** CMake de-duplicates
> identical `target_link_options` entries, so several bare `--preload-file` tokens collapse into one.
> Measured on the generated `link.txt` while adding RayClay's second preloaded asset: **4 requested,
> 1 flag emitted, 4 paths emitted.** The three orphaned paths then reach `emcc` as bare arguments and
> it reports them as *missing input files*, which reads like an absent asset and is not:
>
> ```text
> emcc: error: <abs>/Lato-Bold.ttf@/assets/fonts/Lato-Bold.ttf:
>       No such file or directory (... was expected to be an input file)
> ```
>
> The file is present; only the flag that claimed it was dropped. `SHELL:` is CMake's documented
> escape from that de-duplication and keeps each flag next to its value.

### The page around the canvas

The link block above produces a working page using emcc's default shell. RayClay addresses the canvas
by a **literal CSS selector**, `#canvas`, fixed inside its window backend, so whatever page hosts it must give
that element the box you want the UI to fill:

```css
html, body { margin: 0; padding: 0; height: 100%; overflow: hidden; }
#canvas    { display: block; width: 100vw; height: 100vh; }
```

Supply your own page with `--shell-file <your-shell.html>`. **The CSS block above is the whole of
what a shell must do to get your app RUNNING**, and you can paste it into a page of your own. One
browser gesture needs four more lines, and their absence is silent - nothing looks broken, the
gesture simply does nothing:

```html
<!-- after the <canvas> element -->
<script>
  /* Ctrl+wheel and trackpad pinch are the BROWSER'S zoom on web - RayClay's own
     wheel zoom is compiled out there. The window host accepts every wheel over
     the canvas and calls preventDefault() on it, so a page without this
     exemption zooms NOTHING at all. Capture phase, registered at parse time, so
     it runs before the host's listener whatever order the two register in. */
  var glCanvas = document.getElementById('canvas');
  var passZoom = function (e) { if (e.ctrlKey) e.stopImmediatePropagation(); };
  glCanvas.addEventListener('wheel',      passZoom, { passive: true, capture: true });
  glCanvas.addEventListener('mousewheel', passZoom, { passive: true, capture: true });
</script>
```

That is transcribed from RayClay's own shell, which is a longer worked example and is **not in the
distribution**; it is on the public examples branch as `examples/web/shell.html`. That shell carries
the CSS, the exemption above, a resize handler that re-stretches the canvas, a WebGL2 probe that
shows a readable card instead of a black rectangle, and a "reload to continue" card for
`webglcontextlost`.

> **In DevTools a RayClay diagnostic arrives at its own severity**: errors as `console.error`,
> warnings as `console.warn`, information as `console.log`, so you can filter the console by level
> and get the answer you expect. On desktop every level goes to
> stderr carrying that same tag. Either way `rcSetLogSink` takes precedence and hands you the level
> as a value, which is what you want if you are routing diagnostics anywhere but a console.

> **Building RayClay's own bundled examples into web pages is a separate job from building `myapp`.**
> A RayClay checkout wires each of its web-registered example targets through an `rc_web_example()`
> helper that supplies the page shell and the demo hub, and `cmake --preset web` drives the lot. None
> of that is needed for `myapp`; the link block above, plus the asset and shell notes beside it,
> is the whole web-specific diff.
> See [web-build.md](web-build.md) for the full web story (testing, deploy, limitations).

## 5. Make it yours

Swap the `NULL` for an options struct. That is the whole difference between the two-line program above
and a real app: same call, same shape, you only ever **add lines**. The layout uses a Tailwind-like,
flexbox DSL:

```c
#include "rayclay.h"

static void layout(RC_App *app, void *user) {
    (void)app; (void)user;
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.background, .gap = 16, .p = 24, .w = "grow", .h = "grow") {
        rcTextL("My app", .color = s.text);
        if (rcButton("go", "Click me", RC_BTN_PRIMARY)) {
            /* handle the click */
        }
    }
}

int main(void) {
    RC_AppOptions opts = {
        .width = 900, .height = 600, .title = "My app",
        .scratchArenaBytes = 4096,   /* see the rcFormat note below */
        .layoutCallback    = layout,
    };
    return rcRunApp(&opts);
}
```

**`.layoutCallback` is the only field with no default**, and leaving it unset is not an error: you get
the welcome canvas, exactly as `rcRunApp(NULL)` does, plus one `RAYCLAY[WARNING]` at startup saying so.
A fallback you did not intend is never silent. Everything else zero-initialises to something
sensible, including `.clearColor`, which falls back to the active style's background rather than to
transparent black (a SNAPSHOT of the theme installed at that moment, not a live link), so change it
later with `rcWindowSetClearColor`. Grow the struct as you need it:

```c
    .fontSizes  = sizes, .fontCount = 3,     /* your type ladder         */
    .nativeFrame = true,                     /* borderless + RayClay's titlebar */
    .renderMode = RC_RENDER_ON_DEMAND,       /* park at ~0% CPU when idle */
```

### Utility classes, if you think in Tailwind

Every element also takes `.className`, a space-separated Tailwind v4 utility string. It is the same
element and the same call - a second way to say the same things:

```c
    rcColumn(.className = "w-full h-full p-6 gap-4 bg-slate-900") {
        rcTextL("My app", .color = s.text);
    }
```

**The class string is the stylesheet and the typed fields are the inline style, so a typed field
wins** where both name the same property. Name a group of utilities once with
`rcDefineClass("card", "p-4 gap-2 rounded-lg bg-slate-800")` and use it as `"card"` anywhere.

One rule has no browser equivalent: **a numeric C field left at `0` means UNSET, not zero**, because
the two are the same bytes in a C struct. If you want an explicit zero, spell it in the class string
(`p-0`, `gap-0`). The full vocabulary, the grammar boundary and the caps are in
[Coming from the web](for-web-developers.md#utility-classes-classname).

### Looping over data: never jump out of an element body

You will write this the moment you have a list. The container's `{ ... }` is a macro-generated loop
rather than a plain block, so C's jump keywords do not mean what they look like:

```c
for (int i = 0; i < n; i++) {
    rcBox(.id = ids[i]) {
        if (rows[i].hidden) continue;   /* fine: ends this element's body    */
        if (rows[i].last)   break;      /* compiles, runs, does NOT do what  */
                                        /*    you think (see below)          */
    }
}
```

- **`continue` is safe.** It ends the element body cleanly and reports nothing.
- **`break` is safe but almost never what you mean.** It ends the *element body*, not your `for` loop,
  and the loop keeps going. Measured: `break` at `i == 2` of 5 still ran `i = 3` and `i = 4`. RayClay
  closes the element for you and logs one warning naming the mistake.
- **`goto` or `return` out of the body is unsupported.** Do not write them. Both leave the whole `for`
  statement without running its increment, so the close is skipped and the element stack is left
  unbalanced. The cost, measured on gcc and clang: the layout engine drains what you left open, so the frame is
  *completed*, but it is the **wrong frame**. **Every element you would have declared after the escape
  is simply missing, every frame** (a 5-iteration loop declared 3), and a sibling declared after the
  escape point can vanish entirely. "Unsupported" is the contract, *not* a promise about what happens
  next, so do not rely on whatever you happen to observe, and treat it as a bug, not a warning.
- **TWO DIFFERENT DIAGNOSTICS SIT HERE AND THEY LATCH DIFFERENTLY. Neither floods your log, and the
  scope matters when you are trying to reproduce one.** The *escape* warning - the one this section
  is about, fired when a body is left early - latches **once per PROCESS**: it is a block-scope
  `static` and nothing resets it, so a second window and a second `rcAppCreate` both stay silent.
  The layout-engine errors quoted in the next bullet (`Innermost element left open`, and its family)
  latch **per WINDOW**: each surface carries its own bit per error type, so a second window really
  does report the same fault again. **If you opened a second window to make a warning reappear and
  got nothing, you were looking at the escape warning, and it was not going to.** It scrolls away
  while the broken UI stays on screen, so do not go looking for a repeating line to confirm the problem;
  there isn't one. That is what makes this **harder to notice** than a crash: a crash gets investigated,
  half a missing UI gets shipped.
- **It *usually* names the culprit** (*"Innermost element left open: `<your-element-id>`"*), but **do not
  treat the absence of that clause as a different bug.** It needs two things: the innermost open element must
  have a **string `.id`** (an auto-generated id has no name to print), and the frame's diagnostic buffer must
  have room for the whole composed message. When either is missing you get the bare sentence, by design. So
  **give the containers you loop over real string ids**: that is what buys you the name when you need it.
- **No compiler warns about any of this**, at any level. That is why it is worth knowing up front.

**The fix for all three is the same:** put the loop *outside* the element, or set a flag inside the body
and test it after the braces.

```c
int stop = 0;
for (int i = 0; i < n && !stop; i++) {
    rcBox(.id = ids[i]) { if (rows[i].last) stop = 1; }   /* leaves cleanly */
}
```

**Watch your log for these.** A crash announces itself; a partial frame does not. That one warning is
the *only* signal you get, which makes an eye on the log the difference between catching this in
development and shipping it.

On the web those warnings still reach you; they land in the browser console. See
[web-build.md](web-build.md#seeing-rayclays-diagnostics-in-the-browser).

**Your own font, and your own sizes.** A font is three fields on `RC_AppOptions`; the runner bakes
the whole ladder before the first frame:

```c
enum { F_SMALL, F_BODY, F_H1, F_COUNT };            /* name the slots */
static const float sizes[F_COUNT] = { 13, 16, 30 };

int main(void) {
    RC_AppOptions opts = {
        .width      = 900, .height = 600,
        .title      = "My app",
        .fontPath   = "assets/Inter.ttf",  /* NULL = the bundled Roboto subset */
        .fontSizes  = sizes,
        .fontCount  = F_COUNT,
        .layoutCallback   = layout,
    };
    return rcRunApp(&opts);
}
```

Then select a size by **slot**: `rcTextL("Title", .font = F_H1)`.

- **The i-th size becomes `fontId` i: the load-order index, not the size value.** `.font = 30` does
  not mean 30px; it means slot 30, which does not exist. Name the slots and you cannot get this wrong.
- **Leave `.fontPath` NULL and you still get the ladder**: RayClay bakes the *bundled* face at each of
  your `.fontSizes`, which is why only one bundled example ships a font at all.
- **`.fontPath` without `.fontSizes`/`.fontCount` is ignored** (it warns once) and you silently get the
  bundled face; if your font "didn't load", this is almost always why.
- **Font problems are reported to the LOG, never through a return value.** A failed bake degrades to the
  bundled face and keeps drawing, so nothing crashes and nothing looks obviously wrong; the text is just
  not in your font. `rcLoadFont` returns `0` for the default slot on *every* failure, and `0` is also a
  perfectly normal id, so `if (!id)` cannot tell the two apart. Install `rcSetLogSink` while you are
  bringing a font up and read what it says.
  **Branch on the LEVEL, not the wording.** During an `rcLoadFont` call an **ERROR** means the load was
  refused and you hold the default slot; a **WARNING** means the face loaded and something about it is
  worth knowing. The set of sentences is open-ended, so a sink that matches specific text will one day
  score a real failure as success; the level will not.
  Then read the sentence for the *cause*. `rc_font: bad font data` is the damaged-file one;
  `rc_font: unsupported font …` means the opposite: the face is fine and this build's rasteriser cannot
  open it. Two more are *warnings* on a face that **did** load (some glyphs skipped; a symbol-encoded
  face). **And the refusals you are most likely to hit are not about the font at all:** a path that did
  not resolve (`rc_font_load: cannot read …`; note the different prefix), and running out of slots
  (`rc_font: table full (16) …`, because 16 is the *total* including the bundled face and `rcLoadFont`
  does not de-duplicate paths). [api-notes.md ▸ rcUnloadFont](api-notes.md) has the full table: every
  sentence the font path can print, which are refusals and which are warnings on a face that loaded.
- Set neither and you get the bundled face at one default size as `fontId` 0, so text always renders.
- Need a *named* ladder instead of indices (`rcFont("Inter", RC_WEIGHT_BOLD, 22)`): register faces with
  `rcRegisterFont(family, weight, path, size)` at startup. Up to 16 baked faces total.
  **GIVE EVERY WEIGHT ITS OWN FILE.** `weight` is the *label* you file the face under, not a dial that
  reaches inside it. Register one file under two weights and both bake the same outlines: two ids, and
  your bold renders as Regular. **It is not a variable-font problem:** a *static* Regular file handed to
  `RC_WEIGHT_BOLD` does the same. **A Google Fonts download is laid out to lead you straight into it:**
  `Roboto-VariableFont_wght.ttf` sits at the top level and the per-weight files are tucked inside
  `static/`. Use `static/`.
  **RayClay warns** when a second weight points at an already-registered path, naming both
  weights (once per weight, not per call). The face still loads; it is a warning, not a refusal.
  *(Measured 2026-08-09 across Roboto, Inter, Open Sans and Nunito: registering the variable file as
  REGULAR and as BOLD gives renders identical to the ink pixel, while the static pair separates cleanly.)*
  **The worked example is `examples/ex22_docs_reader`**, which registers a `"Lato"` ladder across
  `RC_WEIGHT_REGULAR` and `RC_WEIGHT_BOLD` at several sizes and resolves it with `rcFont` (see
  `src/app/app.h`). It is the one example that ships its own faces; every other example takes the
  bundled Roboto-Regular, which has no synthetic bold, so a second weight there would mean shipping
  a second TTF.

**Formatting dynamic text?** The default options include **no scratch arena**: `scratchArenaBytes` is the
one field where `0` means *off* rather than *a sensible default*, so `rcFormat` (printf into a per-frame
arena) renders the visible placeholder `<set scratchArenaBytes>` until you set it. Set it (e.g. `4096`); then
`rcFormat(rcAppArena(app), "%d fps", n)` works. Or format into your own buffer with `snprintf` and pass it
to `rcTextC`; RayClay retains the pointer until the frame is drawn, so keep the buffer alive that long.

Colours are CSS-familiar too. Alongside the semantic theme (`s.background`, `s.text`) and the
built-in Tailwind-style palette (`RC_SLATE_800`, `RC_INDIGO_600`; or define your own brand tokens: see
the cheatsheet), any colour slot takes a CSS string via
`rcColor`: `.bg = rcColor("#1e293b")`, `rcColor("rgb(30,41,59)")`, `rcColor("rgba(0,0,0,.45)")`, or a
name like `rcColor("red")` / `rcColor("transparent")`.

The `examples/` folder grows this further, each a single source that runs on both targets. The
`ex01`–`ex05` set is a walk through GUI history, one app per decade, each a little richer than the
last: [`ex01_1980s_gui`](https://github.com/impizulu/rayclay/blob/examples/examples/ex01_1980s_gui/main.c) is a 1-bit calculator,
[`ex02_1990s_gui`](https://github.com/impizulu/rayclay/blob/examples/examples/ex02_1990s_gui/main.c) a Windows 95 settings dialog,
[`ex03_2000s_gui`](https://github.com/impizulu/rayclay/blob/examples/examples/ex03_2000s_gui/main.c) an Aqua/Winamp media player,
[`ex04_2010s_gui`](https://github.com/impizulu/rayclay/blob/examples/examples/ex04_2010s_gui/main.c) a flat Material tasks app, and
[`ex05_2020s_gui`](https://github.com/impizulu/rayclay/blob/examples/examples/ex05_2020s_gui/main.c) a modern dark SaaS dashboard. For a copy-paste
widget reference, [`ex10_rayclay_widgets_gallery`](https://github.com/impizulu/rayclay/blob/examples/examples/ex10_rayclay_widgets_gallery/main.c)
exercises every widget in one file. See [examples.md](examples.md) for the full tour.

## Troubleshooting: first run

Two walls a first build can hit:

**`undefined reference to 'rcRunApp'` (or `rcInitWindow`, …) at *link* time.** RayClay is
**not header-only.** The header gives you the declarations; the implementation lives in RayClay's
library, together with all of its bundled dependencies (§2), so a bare `gcc main.c` /
`clang main.c` compiles your file but has nothing to link the symbols against. Build through CMake:
`target_link_libraries(app PRIVATE rayclay)` (§2) pulls in the library, the vendored windowing and
render deps, and the OpenGL/system libraries in one line. There is no supported raw-compiler build:
hand-linking the sources would mean compiling the whole window host for your platform yourself.

**Consuming RayClay from a project *outside* its clone.** `add_subdirectory(rayclay)` in §2 assumes
RayClay is vendored *inside* your project (a submodule or a copied folder). If you instead cloned
RayClay alongside your app, point `add_subdirectory` at the RayClay **repo root** (the folder that
holds the top-level `CMakeLists.txt`), and give it an output
dir, since it lives outside your source tree:

```cmake
cmake_minimum_required(VERSION 3.21)
project(myapp C)

# ../rayclay is the repo ROOT - the folder that holds the top-level CMakeLists.txt.
add_subdirectory(../rayclay rayclay-build)

add_executable(myapp main.c)
target_link_libraries(myapp PRIVATE rayclay)   # header include dir + every dep come with it
```

Once the target is linked you include the header by name (`#include "rayclay.h"`, no relative
path), because linking `rayclay` puts its header dir on your include path. Then
`cmake -B build && cmake --build build` builds the library and your app together (Windows run path
as in §3). Pulled in this way, RayClay builds **only** the library: its own examples and tests
stay off.

### Consuming RayClay from C++

The library is pure C99 and needs no C++ toolchain to build. Your *app* may be C++ (the header is wrapped
in `extern "C"`), under the rules below. The worked example is `examples/ex06_cpp_kanban/main.cpp`: a task
board held in `std::vector` and `std::string`, with every rule below shown at the line it bites.

**WHAT IS SUPPORTED, EXACTLY: a C++ project can consume RayClay's public C API.** That is the whole
claim, and it is gated - a check builds a separate CMake project that sets no dialect of its own, checks that our target hands it C++20, checks that the default dialect is
REFUSED by RayClay's own `#error`, and compiles the header at `-std=c++20 -Wall -Wextra -Werror`
with a planted offender as the red control.

**WHAT IS NOT CLAIMED IS THAT RAYCLAY'S C SOURCES, OR THE SHIPPED C EXAMPLES, ARE VALID ISO C++** -
and they are not. They use C compound literals, `(T){...}`, which ISO C++ has never had; g++ and
clang++ accept them as an EXTENSION, so "it compiled as C++" is not the same result. Under
`-pedantic` g++ says it in those words: *"ISO C++ forbids compound-literals"*. On 2026-09-20 the
compound-literal ratchet read **60 distinct sites in 11 of the 29 graded consumer translation
units** (clang++ 21.1.8: 65); that is a DATED READING, and the
current one is whatever the ratchet prints today. This bites exactly one person: someone who
renames a shipped `.c` example to `.cpp` and expects a strict-ISO compiler to take it. MSVC is that
compiler - it rejects the bare form outright (`error C4576`) - which is why the public header and
the DSL route through `RC_LIT(T)` and compile on every compiler the project ships to.

**Build at C++20 or later.** The vendored layout engine `#error`s otherwise ("requires C99, C++20, or
MSVC"), so this is a hard stop at the first `#include`, not a subtle incompatibility.

**Designated initialisers must follow declaration order in C++.** C lets you write them in any
order; C++20 does not, and g++ and clang++ both hard-error rather than warn. It bites soonest on
`.className`, which sits near the END of `RC_ComponentOptions` - after every layout and style
designator, and before `.scrollOffset`, which is the last member (an ABI addition appended there).
Order by the HEADER, never by habit: `rayclay.h` is the authority and a designator added to the
struct can move what "last" means:

```cpp
rcBox(.p = 12, .className = "gap-2");   /* ok   */
rcBox(.className = "gap-2", .p = 12);   /* C ok, C++ ERROR: designators out of order */
```

The field is a `const char *`, so a `std::string` needs `.c_str()` - and it must outlive the frame,
which a temporary does not. Prefer a string literal.

**`-Wextra -Werror` needs `-Wno-missing-field-initializers`, and the CMake target passes it for you.**
`rcBox(.p = 12)` names one member of `RC_ComponentOptions` and value-initialises the rest - that is the
DSL's contract, and the zeros are well-defined - but in C++ g++ reports every unnamed member under
`-Wextra`, and clang 18 and later does the same (`-Wmissing-designated-field-initializers`, a subgroup of
the same flag). Neither warns in C. Linking `rayclay` adds `-Wno-missing-field-initializers` to your
C++ translation units under GCC and Clang, and to nothing else (measured on g++ 15 and clang++ 21: a
`-Wextra -Werror` consumer is red without it and green with it). Compiling against the header on your
own line, without our CMake, add that one flag yourself. *(Re-measured 2026-09-20 on the same
translation unit: g++ 15.3.1 reds on `RC_ComponentOptions::bg` and clang++ 21.1.8 on
`-Wmissing-designated-field-initializers`, but **AppleClang 21.0.0 does not warn at all** - so the
flag is a no-op on a mac and a requirement everywhere else. Do not "correct" this paragraph from a
macOS build: it is the platform where the gotcha is invisible.)*

**Single-header users: `RAYCLAY_IMPLEMENTATION` goes in a `.c` file, never a `.cpp`.** This is the one
place the usual single-header habit does not transfer: the reflex is to drop the `#define` into whatever
translation unit is handy, and in a C++ project every unit is handy and none of them is C. RayClay stops
you rather than letting it through:

```
error: #error "RAYCLAY_IMPLEMENTATION must be compiled as C: put it in one .c TU;
               C++ TUs get declarations only"
```

Add one `rayclay_impl.c` containing exactly `#define RAYCLAY_IMPLEMENTATION` and `#include "rayclay.h"`,
compile that file as C11, and link it. **Every other translation unit (all of them C++) just
`#include "rayclay.h"` and gets declarations**, which is the supported and compile-checked path. *(This
rule is about the single-header drop. Building from the source tree, RayClay is already its own C target,
so there is nothing to arrange.)*

**AND IT MUST BE THE FIRST TIME THAT TU SEES `rayclay.h`.** The shipped header carries a second
refusal for the case where the `#define` arrives too late, most often because one of your own headers
already pulled `rayclay.h` in above it:

```
error: #error "RAYCLAY_IMPLEMENTATION must be defined before the FIRST include of rayclay.h
               in a translation unit"
```

Keep `rayclay_impl.c` to exactly those two lines and include nothing else in it, and the situation
cannot arise. *(Both refusals are minted by the amalgamator, so they exist in the single-header drop
only; building from the source tree, neither can fire.)*

**Write options structs in brace form, and list fields in declaration order.** C's designated-initialiser
compound literal `(RC_TableOptions){ .cellPadding = RC_VAL(4) }` is a C-only extension; the C++ spelling
drops the parentheses:

```cpp
// C                                         // C++
(RC_TableOptions){ .cellPadding = RC_VAL(4) }   RC_TableOptions{ .cellPadding = RC_VAL(4) }
```

**The element DSL itself needs no change and no shim**: `rcBox`/`rcRow`/`rcColumn` and the sizing macros
(`RC_PX`, `RC_GROW`, `RC_VAL`, …) route through a portability shim internally, so this compiles as written
in a C++20 translation unit:

```cpp
rcColumn(.id = "root", .gap = 8, .p = 12) {
    rcBox(.id = "card", .w = "grow", .h = "64px") {}
}
```

*(Both snippets above are compile-checked under `g++ -std=c++20 -pedantic-errors`, which rejects the C
compound-literal extension exactly as MSVC does, so "it happens to work on my compiler" is not what is
being claimed.)*

### Windows: MSVC *and* MinGW / MSYS2 / clang

Both dominant Windows toolchains work, and the CMake path above needs nothing extra on either; RayClay
names the OS libraries it needs, so `target_link_libraries(myapp PRIVATE rayclay)` resolves them for you.

**You only need the list below if you are NOT using our CMake**: hand-rolling a compile line, or
vendoring the sources into your own build system. The custom titlebar calls into five Win32 libraries:

```
comctl32   dwmapi   shell32   user32   gdi32
```

**MSVC auto-links these** (the headers request them with `#pragma comment(lib, ...)`). **Every GCC-family
toolchain (MinGW-w64, MSYS2, clang in gnu mode) silently ignores that directive**, so if you omit them
the compile succeeds and the *link* fails on `SetWindowSubclass` / `DefSubclassProc`. `gdi32` is the one
people forget (`CreateRectRgnIndirect` / `SetWindowRgn`).

`comctl32`, `dwmapi` and `gdi32` are the true minimum; `user32` and `shell32` merely happen to sit in
MinGW's default link set today, which is a convenience rather than a promise: name all five.

> **Your Windows app will open a console window beside it, and that is the linker's default, not
> RayClay.** It is where your `RAYCLAY[WARNING]` lines go, so it is useful while you are building, but it
> is not what you ship. **One flag fixes it, and it costs you the console, stdout and stderr both:** see
> the cheatsheet ▸ *"shipping on Windows: stop the console window appearing behind your app"* for the
> recipe on both toolchains and what to do with your log once the console is gone.
> **The bundled examples deliberately stay console builds**: their diagnostics *are* the
> demonstration, so you can see what RayClay is telling you. Flip the subsystem in the app you ship,
> not in an example you are reading.

#### Where Windows actually stands, measured

Rather than a support badge, here is what was run. Host **Windows 11 26200 x64, MSVC 19.44 (VS 2022),
Release**.

- **The library compiles clean:** MSVC at **C99, 0 errors and 0 warnings**.
- **Every bundled example links, and every one of them runs.** They are launched, not merely built,
  on all three platforms, on a real compositor with a real GPU driver rather than a software
  rasteriser: window, native resizing and the taskbar icon all correct.
- **The behavioural test suite runs on MSVC, not just the compile**: tables, charts, titlebars,
  zoom, interaction, text areas and the bench suite all execute there.
- **A handful of checks are Linux-only, and deliberately so**: X11, `-ffast-math`, GNU-ld `--wrap`
  and Linux profiling tools, where running them on MSVC would prove nothing or pass vacuously.
- **The one caveat worth carrying away: there is no pixel-level verification on Windows or macOS
  at all.** Only the Linux leg reads back actual pixels; everywhere else what is proven is which
  *draw calls* were issued, not what landed on screen. That distinction is not academic: a driver can accept
  **every draw call** and still put nothing correct on screen, with every other check staying green,
  which is exactly why the pixel readback exists.
- **The layout itself is compared across platforms, and it agrees exactly.** 44 text measurements
  (`rcMeasureText`) plus the element and draw-command counts (`rcWindowFrameCounts`) are **bit-identical**
  on x86_64/gcc, arm64/Apple clang and x86_64/MSVC: `max |Δ| = 0`, not "close". So the *inputs* your
  layout is computed from, and the *shape* it produces, do not drift between platforms.

**What that means for you:** building, testing and shipping a RayClay app with MSVC is an exercised path,
not an aspiration, but it is **not** "Windows is green", and we would rather you heard the difference
from us.

**Neither of those is a pixel check, and the caveat above still stands.** Equal metrics and equal
counts leave anti-aliasing, tessellation and rasterisation untested, and those are exactly where a
driver bug lives. **If you ship on Windows or macOS, run your own UI on it and look at it before you
believe it.**

## Troubleshooting: a warning in your console

RayClay writes diagnostics to **stderr**, prefixed `RAYCLAY[…]`, so `./myapp > log.txt` captures none of
them and `2>` or `2>&1` is what you want. The layout arena is **noisy while it grows**,
and nearly all of that noise is benign, so the thing to read is not the *text* of a line but its
**level**. There is exactly one `RAYCLAY[ERROR]` in this family, and it is the only one that means a frame
was lost.

| what you see | what it means |
|---|---|
| nothing | the arena was big enough from the start |
| `[WARNING]` *ran out of element capacity* **+** `[WARNING]` *still open layout elements* | **benign warm-up growth.** Self-resolving, but that frame draws NO UI: it shows the clear colour alone |
| both of those **plus** `[ERROR]` *capacity hit the ceiling (N)* (usually with a hashmap warning) | **real.** The arena refuses to grow; the frame IS presented, TRUNCATED: the overflow is what is dropped |
| one `[WARNING]` naming *internal text measurement cache*, **with no companion warning** | **the other capacity.** Same warm-up growth, different pool: measured words, not elements. See below |

**The two warnings on their own are normal warm-up.** The layout arena starts at
`RC_AppOptions.startLayoutElements` and *grows on demand*. On the frames where it is growing, that frame's
declaration is cut short, so the layout engine reports both that it ran out of element capacity and that
elements were left open. RayClay then grows the arena, re-lays out at the larger size, and carries on.
**It is self-resolving, you see it once, and any app whose first screen is bigger than its start
count shows it** - including any app that lowers `startLayoutElements` to keep its initial arena
small.
**But a growing frame draws nothing of your UI.** RayClay skips `rcRender` on exactly that
frame; the clear, the end-frame and the swap all still run, so what the user sees is the **clear
colour alone**. That is deliberate (a half-declared layout is worse than a blank one), and it is why
`RC_AppOptions.clearColor` / `rcWindowSetClearColor` matter more than they look: on a warm-up frame the
clear colour IS your app.
Measured: 600 elements from a 2,048 start is silent; the same 600 from a 128 start prints
those two warnings. **That measurement pinned frames PRESENTED, not frames whose UI was drawn**: do
not read it as "the UI rendered every frame".

**There is a second capacity, and its message looks nothing like the first two.** Text is measured
into its own cache keyed by word, so a text-heavy screen can exhaust that pool while the element
arena is comfortable. It grows and warns the same way, with two differences worth knowing before you
go looking for a bug: it arrives **alone**, with no *still open layout elements* beside it, so it
matches neither row above; and the ceiling it reports is **twice** `RC_AppOptions.maxLayoutElements`,
because the word pool is sized at 2x the element pool. A reader who has set `maxLayoutElements` to
65,536 and reads a ceiling of 131,072 has not misread anything.
**Raise `RC_AppOptions.maxLayoutElements`.** The message is Clay's own and names Clay's setter; that
is the layer underneath, and RayClay sizes both pools from the one option you already have.

> **The warning tells you this itself.** On the growth path RayClay appends *"capacity is
> growing and this layout will be re-run at the larger size: no lasting data loss. Raise
> `RC_AppOptions.startLayoutElements` to skip these warm-up frames"*. **If the warm-up frames bother you,
> that is the knob**; raising `maxLayoutElements` does nothing here. The layout engine's own sentence before
> it still says *"every element past this point was dropped"*, which is true of the pass that was cut short
> but not of the frame you finally see.

**`RAYCLAY[ERROR]`, "…capacity hit the ceiling (N); not growing further", is the real problem.** The
arena has refused to grow. **The frame is still PRESENTED, truncated rather than dropped**: at the ceiling
the runner deliberately does NOT take the growth path, precisely so you get a degraded frame instead of
the background held forever. What is dropped is the OVERFLOW, every element past the ceiling. The knob is
**`RC_AppOptions.maxLayoutElements`** (default 65,536); that line names it for you, and names the ceiling
it actually stopped at. Before you raise it, check whether you are *declaring* far more elements than you
draw: a long list wants `rcVirtualList` ([cheatsheet.md](cheatsheet.md)), which declares only the visible
window, and raising the ceiling instead permanently costs memory that the real fix costs nothing. The
arena never shrinks back.

> **There is a second reason to keep the declared count down, and it is not about memory.** Element ids are
> **32-bit hashes**, so two ids you wrote as different strings can land on the same value, and the loser
> silently takes the winner's box, with no error at the call site.
>
> **You cannot predict this from your element count.** Pure chance would put it near 1% at 10,000 declared
> elements, but that is a *floor*, not an estimate, because the real rate depends on **the id strings you
> chose**. Measured over sequential ids of the form `<prefix><index>`: `"Row 0…"` and `"e0…"` stay
> clean all the way to 65,536, `"cell-0…"` is clean until 50,000, and **`"item0…"` is clean at 15,000 and
> collides at 20,000**, far above chance, and with nothing to warn you that your prefix is one of the bad
> ones. **A prefix that is safe in your app is not safe in the next one.**
>
> **The reliable defence is to declare fewer elements, not to pick a luckier name.** A virtualized list
> (`rcVirtualList`) declares only the visible window, typically tens of rows, where no prefix has been
> observed to collide. So "declare fewer elements" fixes two independent problems at once, and "raise the
> ceiling" makes the second one worse.

> **A capacity warning also carries the vendored layout engine's own prose, which names ITS entry
> points, none of which are in `rayclay.h`.** RayClay passes that text through unchanged (it carries the
> detail) and then appends the remedy you can act on, in brackets, so **the callable name is always the
> last thing you read**. If a diagnostic names something you cannot find in the header, that is why: read
> the bracketed `[RayClay: …]` clause and ignore the entry point named before it.
>
> **That clause is case-aware, and it names `startLayoutElements` on the growth path and
> `maxLayoutElements` at the ceiling.** One wrinkle worth knowing: a run that ends *at* the ceiling usually
> shows **both** clauses, because the earlier grows really did succeed before the last one refused. **The
> level still settles it**: take the `[ERROR]`'s knob when there is an `[ERROR]`.

**"…is ignored" on an SVG: your artwork drew, but not the way you drew it.** RayClay names
nine attributes it does not implement, in five messages, because each of them leaves the shape **on
screen and wrong** rather than missing: `transform` (art lands at its raw viewBox coordinates), `display`/
`visibility` (**a hidden layer still draws**), `opacity` (fully opaque), `fill-rule` (an evenodd hole
fills solid) and `stroke-dasharray` (dashes draw solid). Each warns **once per document**, and each
message names the way out.

> **This is the one warning class where the fix is in the ASSET, not your code.** An icon that appears
> off-centre or oversized is almost always a `<g transform>`; flatten it in your editor and reframe the
> `viewBox` around the art as drawn. **RayClay maps your `viewBox` onto the element box; it does not fit
> the drawing's bounding box**, so no amount of `.align` will re-centre art that sits high inside its own
> viewBox. Full detail, and why `stroke-linecap` is ignored *without* warning, in
> [api-notes.md ▸ Attributes the parser ignores](api-notes.md).

**"An element was declared with a duplicate ID during this layout: `<id>"` names the offending id.** It
means what it says, and it is unrelated to every line above: two elements in one frame were given the same
`.id`. It is worth stating plainly because neither growth nor the ceiling produces it: re-measured at
across four ceiling configurations (256 / 2,048 / 8,192 / 20,000), none reported a duplicate id,
while a control declaring 600 siblings under one id reported it immediately and named that id. **So a
duplicate-id report is always a real duplicate, and always worth chasing.**

## Does it really look the same everywhere?

That is RayClay's central promise, so here is the evidence rather than the claim. The same source was
built and run on **Linux/x86-64 (GL 3.3, NVIDIA)** and **macOS 15 on Apple Silicon (GL 4.1 via Metal)**,
and a pixel readback compared the two:

> Every solid-fill colour region matched at a ratio of **4.0000**, exactly the 2×2 Retina factor, with
> no drift. In other words the layout is **pixel-identical in logical space**; the Mac simply has four
> device pixels where Linux has one.

So the boxes, spacing, colours and geometry you lay out are the same picture on both. Three things do
differ, all of them by design:

| | Win32 / X11 | macOS (Retina), Wayland, web |
|---|---|---|
| Device pixel ratio | **1.0 at every DPI**: the framebuffer equals the window | **2.0** on a Retina Mac: window stays logical, framebuffer doubles |
| Text | crisp at 1× | **re-baked at the true density**: genuinely sharper, not an upscale, so glyph pixels do *not* simply quadruple |
| Max MSAA the GPU offers | often 32 | **4**: the default is 4, so nothing is lost; just don't expect more |

**Text is the one thing that is deliberately not byte-identical.** RayClay re-bakes the glyph atlas at
the true render density, so on a Retina display you get thinner, sharper strokes rather than a blurry
2× blow-up. That is the correct behaviour and the same thing a browser does, but it means a
pixel-exact screenshot diff across platforms will always differ in the text, and should be compared on
layout and colour instead.

Nothing in the build needed a platform `#ifdef`, and the whole library plus examples compiled on macOS
with **zero warnings** under `-Wall -Wextra -Werror`.

**Outside the window, one thing genuinely is not uniform: the application icon.** What you lay out is the
same everywhere; where the *desktop shell* shows your app is not, and no library can make it be.
`RC_AppOptions.iconBytes`/`iconPath` sets the taskbar and title-bar icon on Windows, the window icon on
X11, and the **dock** icon on macOS (a macOS window has no icon of its own). On **Wayland** and on **the
web** it does nothing at all: Wayland has no runtime protocol for it (the compositor matches your `app_id`
against an installed `.desktop` file, so there it is a packaging job) and a browser page owns its favicon
through HTML. Setting the field is still correct (it is simply inert on those two), so **ship a
`.desktop` file if Wayland users matter to you.** Full table, and the `RC_NO_DEFAULT_APP_ICON` build knob,
in the [cheatsheet](cheatsheet.md).

## One source, every screen size

The section above is about the *picture* being the same everywhere. This one is about the *space* not
being: a desktop window is wide and a phone is 411 logical pixels across, and no amount of pixel
fidelity saves a three-pane layout squeezed into a third of its design width.

RayClay's answer is deliberately small, because the alternative is two applications.

### The rule: branch on the space you have, never on the operating system

There is no `RC_MOBILE`, and nothing else keyed on the operating system. The class grammar carries
Tailwind's five responsive prefixes (`sm:` `md:` `lg:` `xl:` `2xl:`, at Tailwind's own values; see
[for-web-developers.md > Breakpoints](for-web-developers.md#breakpoints-measure-the-layout-width-not-the-window-width)),
and for the branches a class string cannot express - which pane to declare at all, how many columns
to build - a responsive layout here is an ordinary C `if`, on `rcViewport()`:

```c
static void layout(RC_App *app, void *user)
{
    RC_Viewport v = rcViewport();

    if (v.width < 768.0f) compact_layout(user);
    else                  wide_layout(user);
}
```

`rcViewport().width` is **the space Clay was actually given**, in layout units, not window pixels.
That distinction is the single most common way a breakpoint ends up inverted, and reading it from
the library is how you stop having to think about it: `RC_ZOOM_LAYOUT`, the default, reflows into
`window / zoom`, while `RC_ZOOM_OPTICAL` lays out at `max(window, window / zoom)` and magnifies the
raster instead, so dividing the window size by the zoom yourself is right in one mode and wrong in
the other. `rcViewport()` has already resolved that.

It also carries `.breakpoint`, Tailwind's six bands at Tailwind's own values, if you would rather
name the rung than repeat the number:

```c
if (rcViewport().breakpoint < RC_BP_MD) compact_layout(user);   /* RC_BP_MD is >= 768 */
else                                    wide_layout(user);
```

That is strictly more expressive than a media query, because the condition is C and can read anything:
the size, the app's own state, whether a pointer is present. It is also strictly less familiar, so it is
worth saying plainly what the rule buys you.

**Choose the layout from the space, never from the platform.** An `#ifdef __ANDROID__` gets this wrong
in both directions: a tablet in landscape gets the phone layout it does not need, and a desktop window
dragged to a third of the screen keeps a wide layout it has no room to draw. Every layout you can reach
on a phone you can also reach on a desktop by making the window narrow, which is the only reason this is
testable at your desk at all.

`768` above is Tailwind's `md`; RayClay has no opinion about the number. Derive yours from your own fixed
widths: if a sidebar is 280 and a detail pane stops being useful below 420, your breakpoint is 700, not a
number from a table.

The one thing not to do is measure it yourself from `rcGetWindowDimensions()`: the naive
`width / zoom` inverts every breakpoint under optical zoom. `rcViewport()` exists so that this is
someone else's problem. The full rule is in
[for-web-developers.md ▸ Breakpoints](for-web-developers.md#breakpoints-measure-the-layout-width-not-the-window-width).

### The safe area: a phone draws your window under the status bar

Ask for it, and subtract it. The OS will not do this for you and the runner does not do it either.

```c
RC_Viewport v = rcViewport();     /* .safe is ALREADY in layout units - no arithmetic here */

rcColumn(.id = "root",
         .pt = (uint16_t)v.safe.top, .pb = (uint16_t)v.safe.bottom,
         .pl = (uint16_t)v.safe.left, .pr = (uint16_t)v.safe.right,
         .w = "grow", .h = "grow") {
    /* ... */
}
```

Apply it **once, at your root**, not at every child: it is a property of the window, not of a widget.

**One desktop window is not zero, and it is probably the one on your desk.** A FULLSCREEN window on a
notched Mac reports the notch: SDL sets the insets from `NSScreen.safeAreaInsets` on macOS 12 and
later, and zeroes them again the moment you leave fullscreen. A windowed app on any desktop still
gets `{0,0,0,0}`, which is why the rest of this section reads the way it does. Derived from the
backend's source rather than measured on the machine, so treat it as a reason to spend the inset at
your root on every target, not as a number to quote.

**Do not reach for `rcGetSafeAreaInsets()` and divide it by `rcWindowZoom()` here.** That is the same
mistake the paragraph above describes, wearing different clothes: the raw insets arrive in window
pixels, and dividing by zoom converts them correctly under `RC_ZOOM_LAYOUT` and **incorrectly under
`RC_ZOOM_OPTICAL`**, which lays out at `max(window, window/zoom)`. It is also a single factor
applied to both axes, and the two axes floor independently. `rcViewport().safe` does the conversion
from the ratio the layout engine actually got, per axis, so it is right in both modes and stays
right if a third is ever added. The `(uint16_t)` casts remain **required** - the padding fields are
`uint16_t`, `RC_Insets` members are `float`, and `-Wfloat-conversion` is on.

The insets are `{0,0,0,0}` on desktop, so this is the same code on every target and
there is nothing to `#ifdef`. Measured on a Pixel 8a (1080x2400 at 420 dpi, so a **411 x 914 logical-px
viewport**), the insets are **46 px at the top** (status bar and camera cutout) and **24 px at the
bottom** (the gesture pill): about 8% of the window, and precisely the 8% your header and your bottom bar
want to occupy.

### The architecture: share everything except the layout

**The file layout that goes with all of this is its own page: [How to structure a RayClay
app](app-structure.md).** It is four files, one of which is empty on purpose.

The shape that works is the one most cross-platform GUI stacks converge on, and RayClay does nothing to
stop you:

| layer | what lives there | platform-aware? |
|---|---|---|
| domain / services | your state, your rules, your I/O | **no.** Never fork this. A bug fixed on desktop must be fixed on the phone by the same edit |
| theme + components | colours, spacing, your own widgets | **no.** One `rcSetStyle`, one set of components, both layouts draw them |
| layout | which panes exist, and how they are reached | **yes** - and by *available space*, per the rule above |
| platform adapter | only what genuinely has no portable spelling | **yes**, and it should be almost empty |

The layout layer is the only one that branches, and it branches on a number rather than on a target. If
you find yourself forking a service, that is the signal that a *capability* is missing from the layer
below, not that you need a second application.

### What "the compact layout" usually is

Three panes become one pane at a time plus a way back. A fixed-width sidebar becomes a full-width list
that the detail view replaces. A wide row of controls becomes a column. None of this is a RayClay
feature; it is all ordinary elements, which is the point.

**What it is never: a smaller app.** Portrait is where a phone spends almost all of its time, so the
compact layout must carry everything the wide one does, reachable by scrolling; landscape may
rearrange the same things and may never be the only place a control or a number appears. The
patterns that keep an app whole at 393 units wide (chip rows for hidden table headers, a scrolling
root, no fixed width wider than the phone, no text cut mid-word) are in
[app-structure.md ▸ Portrait is the contract](app-structure.md#portrait-is-the-contract-landscape-is-a-presentation).

**Scrolling works by finger, and the rules are worth knowing before you lean on it.** A press-drag on
your content pans the deepest scroll container under the contact, per axis, after an 8 px threshold.
Two things follow that a reader usually assumes the other way round: **a pan does not fire a click**,
so a draggable list of buttons behaves; and **an axis only pans if you authored it scrollable and the
content actually exceeds the view**, so `overflow: hidden` without `.scroll` never pans, whatever the
content size. **A flick flings**: the release keeps moving and damps to a stop, and because the
velocity is smoothed over the last moment rather than read off the final pair of points, a slow drag
that ends in a flick throws at the flick's speed. Park the finger for a moment before lifting and
nothing is thrown. There is no scroll chaining for a finger: a pan that
reaches the end of its container stops there rather than moving the parent. The mouse wheel does chain,
so a notch that a nested or sideways scroller cannot take reaches the page behind it.

`rcScrollbar(id)` is a nicety rather than the only way in, and **you call it unconditionally** -
there is no `if (!rcPointerIsCoarse())` to write, because the library asks that question itself. On a
desktop you get the bar you expect, and it is something a mouse can actually grab. On a phone you get
what a mobile browser gives you: no track, a thin translucent thumb while the content is moving, gone
about half a second after it stops. It claims no input while you tap or pan, so it can never swallow
a tap meant for the content underneath - but press and hold the thumb for half a second and it widens
and takes the drag, so a long list can be driven by the bar as well as swiped.

One thing that is still not a solution: **do not answer a too-wide element by making it scroll
sideways.** It will pan now, but a `.scroll` container is a clip container on **both** axes, so
`.scroll = "h"` cuts a drop shadow off the top and bottom of its own row on every platform. Make it
narrower, make its text smaller, or drop something.

### Where mobile actually stands today, stated plainly

RayClay builds and runs on Android, and the examples in this repository have been qualified on a physical
device. Two boundaries matter to you before you plan around it:

- **The Android path is an in-repo debug APK producer, not a release-packaging interface.** It builds the
  applications registered by RayClay's own build. It does not accept an external source tree, an
  arbitrary consumer CMake project, or loose external assets, and its generated package identity and
  debug signing are not an end-user distribution path. If you are shipping to a store, that work is
  yours today.
- **Desktop is unaffected by any of it.** It keeps its window host; the Android work is additive.
- **The web is unaffected except on one point, and it is a sharp one: the soft keyboard.**
  `rcSetSoftKeyboardVisible()` is inert in a browser, so a RayClay page cannot raise a phone's
  on-screen keyboard, and your code cannot work around it: the page is a single `<canvas>` with no
  focusable text element for the browser to attach one to. **Typing works wherever a real keyboard
  does** - a desktop browser, or a phone with one attached; what a touch-only phone cannot reach is
  the on-screen keyboard. Plan a web build for reading and pointing on a phone, not for data entry.

Three things are absent rather than automatic, and an app cannot paper over any of them from its own
code:

1. **Ordinary editor focus does not raise the soft keyboard**, and there is **no automatic
   keyboard-avoidance**, so a field low on the screen can sit behind the IME.
   `rcSetSoftKeyboardVisible()` and `rcSetImeCaretRect()` work when you call them explicitly **on
   Android and iOS**; on the web `rcSetSoftKeyboardVisible()` does nothing at all and says so once
   in the log rather than failing silently (see the web point above). IME preedit composition is
   not implemented.
2. **There is no scroll chaining for a finger.** A pan that reaches the end of its container stops
   there rather than moving the parent, so a nested list already at its end holds the page still
   under a finger. (A mouse wheel does chain to the enclosing scroller; a finger does not.)
3. **A long-press does not open a context menu.** `rcBeginContextMenu` opens on a right-click and a
   finger has no right button, so a control whose only affordance is a right-click has none on a
   phone.

**iOS: every example runs on a physical phone, and this is what is established.** Every example
builds for the device, and every one of them except the desktop icon converter (`ex11`) is
installed on an iPhone 15 (iOS 17.5.1) on each sync: every `ex*` example and all six `bench/` apps,
each built, signed, installed and launched from the shell with nobody at the Mac, each rendering its
full budget of frames and screenshotted on the device, each carrying the RayClay icon on the home
screen. The set grows with the example roster rather than with this paragraph. Two things a phone
user sees were checked by eye on that device: an app launches with **no soft keyboard** until an editor is focused, and **rotation
keeps the frame at 1:1** rather than stretching it during the turn.

What it is not is a shipping target. Release packaging and signing are unresolved, the renderer on
the phone is OpenGL ES rather than Metal, the three gaps above have not been established either
way on iOS, the soft-keyboard behaviour a phone app needs is unverified there beyond "it stays
down", and nothing in this section's Android qualification carries across. Build and run your app
on the phone freely; do not plan a store release around it yet.

## Known limitations

**Holding a resize edge motionless pauses frame-driven animation (Windows + macOS desktop).** While you
*drag* a window edge the content repaints live, but pressing an edge and holding it motionless freezes
per-frame animation (and any frame counter) until you move again or release. This is deliberate: to
preserve the native OS resize (Aero Snap, native edge-resize feel) the pointer is handed to an
OS-owned modal loop that doesn't return control to the app until mouse-up. The window never goes
blank and static UI is unaffected. Linux/X11 doesn't exhibit it: there the resize is delegated to the
window manager, and there is nothing to change in your app code.

**Linux under Wayland has the opposite problem, and it is an open defect.** The compositor sends a
configure at pointer-motion rate and RayClay draws a full frame for each one, synchronously, with no
coalescing: if a draw is slower than the configures arrive the app is unresponsive for the length of
the drag and then walks the backlog afterwards. Building with `-DRC_NO_LIVE_RESIZE` avoids it, at the
cost of holding the last frame until the drag ends.

### Seven fixed ceilings, and what happens when you reach one

These are compile-time array bounds, not tunables: there is no `-D` knob and no runtime setting.
**None of them crashes, corrupts, or silently renders wrong: every one of them warns.** Six warn once
per process; the font registry warns on every refused call, because a refused registration is a
question about that one face. Most counts are per frame, so a screen showing eight inputs at a time
is nowhere near the input ceiling however many fields your form declares in total.

| Ceiling | Value | What it counts | Past the limit |
|---|---:|---|---|
| Text inputs | 16 | `rcTextInput` / `rcTextArea` registered in one frame | the extras still LAY OUT but draw no text and no caret |
| Tooltips | 32 | tooltips shown in one frame | the extra tooltip is not shown |
| Modal nesting | 8 | how deep modals may nest, not how many you declare | the extra modal is not opened |
| Overlay nesting | 16 | overlay push depth | the tints past the limit are not drawn; the elements still are |
| Registered font families | 16 | `rcRegisterFont` entries for the life of the process | the face may still load, but no registry row is taken, so `rcFont` never resolves it |
| Table columns | 16 | columns in one `rcBeginTable` | the extra columns are dropped, the table still draws |
| Table nesting | 4 | how deep tables may nest | `rcBeginTable` returns **false** and the whole table is skipped |

Two of these deserve planning rather than remembering. **The font registry is the only per-process
entry**: each family plus weight plus size consumes a slot permanently, so registering faces in a
loop exhausts it where a form with 40 fields never exhausts the input ceiling. Recover a slot with
`rcUnloadFont`, which frees the registry row as well as the face. And **the two nesting limits refuse
the whole element** rather than trimming it: `rcBeginModal` and `rcBeginTable` both return `false`,
so branch on the return if you nest either.

### Not in v1.0 (so you can plan around it)

RayClay is deliberately scoped for its first tag. These are **out** in v1.0, named here so nothing
surprises you:

- **Keyboard traversal, focus rings, OS dark-mode detection, and accessibility.**
  Widgets are pointer/touch-first: `Tab` / `Shift-Tab` do not move focus between them and there is no
  focus ring. Static text IS selectable and selection is **on by default**, the way a browser has it,
  and `.select = rcSelectable(false)` turns it off per widget. A drag wraps onto the next visual line
  and carries on across components, primary modifier + A selects every run in the window
  (`rcSelectAll()` where there is no keyboard), and copy (primary modifier + C) takes up to 4,095
  bytes; double-click takes a word and triple click the whole element, as a browser does. Dark and light themes ship (`rcStyleDark` / `rcStyleLight`); install one with `rcSetStyle(rcStyleLight())` and
  read the active theme anywhere with `rcGetStyle()` (it returns the dark preset until you install
  another). **A theme switch AT RUNTIME needs a second line**, because `rcSetStyle` cannot reach the
  window behind your layout: `rcWindowSetClearColor(rcAppMainWindow(app), rcGetStyle().background)`.
  Without it the old background stays wherever your layout does not cover the window - the edge
  during a live resize, and the one frame RayClay holds back while it grows its layout arena, where
  the clear colour is the whole screen.
  **Call it unconditionally, next to wherever you install the style.** `rcWindowSetClearColor` is
  change-gated inside the library - it requests a frame only when the colour actually moves - so the
  plain immediate-mode idiom is also the cheapest correct one, and there is no shadow copy to keep in
  sync:

```c
rcSetStyle(my_theme(st->dark));                  /* wherever you already do this */
rcWindowSetClearColor(rcAppMainWindow(app), rcGetStyle().background); /* the second line */
```

  **Do not "optimise" that into `if (st->dark != last)`, and this is worth knowing before you write
  it.** A theme toggle is a widget inside your layout, so it flips your bool *after* the frame's
  `rcSetStyle` has run. A guard keyed to the bool therefore fires on a frame where `rcGetStyle()` is
  still the OLD style, pushes the wrong colour once, and then never fires again - because your bool
  and its shadow now agree, and the window keeps the wrong ground for the rest of the run. Reading
  the style itself has no such ordering to get wrong. `examples/bench/trader`, `messenger`, `notes`,
  `gallery` and `ex22_docs_reader` all do it the short way.
  RayClay does not read the OS preference; you choose it. There is no assistive-technology / screen-reader surface.
- **One primary window per process, and as many extra desktop windows as you like.** `rcAppCreate`
  returns `NULL` when a primary window is already open, and says so in the log; a bare `rcInitWindow`
  trips the same guard. Sequential lifetimes are legal, each with its own clear:
  `create`, `rcAppDestroy`, `create` for an app, and `rcCloseWindow` for a window you opened yourself
  with `rcInitWindow`.
  **For a second native window, call `rcAppOpenWindow(app, &options)` on the app you already have.**
  One `RC_App` and one thread drive every surface, each with its own layout callback, scratch arena,
  zoom, clear colour and render mode. It is desktop only: on mobile and the web it returns `NULL`
  without attempting a host window. A non-`NULL` return is a record, not a ready window, so observe
  `rcWindowState(w) == RC_WINDOW_READY` before moving content into it.
  **A torn-off panel almost always wants `RC_WindowOptions.owner` set to the window it came from.**
  That is what makes it a child of your app rather than a second application: the desktop keeps it
  above its owner and minimises it alongside, so it cannot be lost behind the document it belongs to.
  Set it at open, not afterwards, along with `.placement` and `.topmost` - state applied once the
  window is already showing is visible to the user as a jump or a flash. Each one is a request a
  window manager may decline, so each has a query to ask first (`rcWindowOwnerSupported`,
  `rcWindowPositionSupported`, `rcWindowTopmostSupported`) and a getter to read back afterwards.
  **Two qualifiers that a reader of the paragraph above will otherwise assume wrongly.** `.owner`
  does NOT by itself keep the panel out of the taskbar - that is `.taskbar`, independent on purpose,
  because a desktop decides listing by window TYPE and an owned window is listed like any
  application; a tool panel asks for both. And `rcWindowOwnerSupported` is false on a Wayland
  desktop built against GLFW, which has no route to parent a toplevel there, so on that session the
  panel is a plain window and your app owes the user its own way back to it. See
  [cheatsheet.md](cheatsheet.md) for the full window API and
  [api-notes.md](api-notes.md#several-windows-one-app) for the lifecycle rules.
  `examples/ex20_system_monitor` is the worked case for a panel the user tears off and expects to
  get, and `examples/ex12_rayclay_inspector` for the opposite one - a watch window that opens
  without taking the keyboard (`.focusPolicy`) and stays out of the taskbar (`.taskbar`).
  **If a second view belongs in the same window instead**, `rcBeginSplitPane` gives a resizable
  sidebar and detail split and a non-modal `rcBeginModal` gives a floating inspector that leaves the
  app live behind it. Both are demoed in `ex10` and `ex12`; see
  [widgets.md](widgets.md#modal-dialogs-and-non-modal-panels).
- **Scrollbars are opt-in and vertical-only.** A scroll container clips and scrolls, but draws no bar
  until you call `rcScrollbar(id)`, **from inside your layout callback**, since the bar is a floating
  element declared at the call site. Horizontal overflow scrolls without a bar. Layering is automatic:
  the bar sits above its content and *below* a modal scrim, so an open dialog covers and dims it like
  everything else behind it. Declare it in the same scope as the container it names.
- **An app that animates on its own must ask for frames.** The runner is event-driven, so an
  idle window parks at ~0 CPU, but state RayClay cannot see changing (a timer, a socket, a physics step)
  needs a `rcWindowRequestFrame` or `RC_RENDER_CONTINUOUS`, or it stops when the window goes idle. See
  [Idle CPU](#idle-cpu-rayclay-draws-only-when-something-happens). RayClay still repaints the *whole*
  window when it does draw: there is no partial-damage / dirty-region path, so a caret blink costs a full
  frame rather than a caret-sized one.
- **Text is ASCII + Latin-1 by default.** Glyph coverage is an *engine* codepoint window: printable ASCII
  (32–126) plus the Latin-1 supplement up to `RC_FONT_LAST_CODEPOINT` (default 255). A printable codepoint
  outside it (smart quotes, €, CJK) draws `'?'` **and says so in the log, once for each distinct
  codepoint, up to eight of them**, so a second offending character is not hidden behind the first
  and a document pasted from the web cannot flood your log. The window is the
  engine's *and* the font's, so **both** must cover a character: a custom font containing € still shows
  `'?'` until you raise the cap, and raising the cap alone changes nothing while the bundled Latin-1 face is
  in use. `-DRC_FONT_LAST_CODEPOINT=N` widens it, at a cost **linear in the cap**: the glyph table is
  direct-indexed, so every codepoint from 32 to N reserves a slot in *every* loaded face. Suitable for
  contiguous Western ranges; **not** a CJK switch. *Typed* text input is ASCII in v1, but a
  `.placeholder` or a value you pre-fill into the buffer renders the whole Latin-1 window.
  **`RC_AppOptions.title` is drawn by two different renderers, and only one of them is subject to
  this.** On an OS-decorated window (the default) the string goes straight to the OS caption, which has
  the system font and the whole of Unicode. Under `.nativeFrame = true` the bundled bar draws that same
  string with **your** `fontId` 0, so a title that was fine becomes `'?'` where it leaves the glyph
  window. The usual casualty is a typographic dash in a title: an em dash (U+2014)
  and an en dash (U+2013) both sit outside Latin-1, while the middle dot `·` (U+00B7) and a plain
  ASCII hyphen are inside it. `"My App - Editor"` is safe; the same title set with U+2014 is not. Nothing warns; the OS-decorated build looks correct right up until you switch frame modes.
- **The clipboard works out of the box on desktop *and* on the web**: the web backend is the browser's own
  `navigator.clipboard`. Two browser rules apply there and are not RayClay's to relax: the Clipboard API
  exists only in a **secure context** (https or `localhost`), and a **read** additionally needs a user
  gesture, so issue one from a click handler, not at startup. A refused or unavailable read resolves as
  "no text" rather than crashing. Read with `rcClipboardRequest` + `rcClipboardPoll`; **`rcClipboardGet`
  returns `NULL` on the web**, because it can only answer under a synchronous backend, so never
  feature-detect the clipboard with it. See [web-build.md](web-build.md) §7.

## Idle CPU: RayClay draws only when something happens

This is the default, and there is nothing to set up. RayClay apps do not redraw at the display
refresh rate forever; they draw when something actually happened (input, a resize, a DPI change, a
tooltip or caret deadline, a resource publish, or a frame you asked for) and otherwise sleep in the
OS event loop.

Continuous rendering costs real CPU for as long as the window is open, and it costs the same whether
the scene is a hello-world label or the full widgets gallery, because the cost is the redraw rate
rather than the content. On demand, an idle window reaches **zero admitted frames** and stays there.
Measure both on your own box with `RAYCLAY_IDLE_STATS=1`, which prints
admitted/waits/deadline/spurious at exit and then names the source of every admission - that is the
comparison to trust, because it is your machine, your compositor and your app.

You still author the same way: the whole UI is still declared top-to-bottom every frame; on-demand
scheduling only changes *how often* that happens. Both are modes you can select (`RC_RenderMode`, or
`RAYCLAY_RENDER_MODE`), so continuous is what you pay if you opt back into drawing every refresh.

### The one rule: if RayClay can't see it change, ask for a frame

RayClay tracks its own redraw reasons, so buttons, text fields, scrolling, hovers, tooltips and zoom
all just work. What it cannot see is state that changes on its own: a countdown, a socket message, a
worker result, a physics step. That state needs one line:

```c
static void update(RC_App *app, void *user) {
    AppState *st = (AppState *)user;
    if (st->playing) {
        st->pos += 0.25f;
        rcWindowRequestFrame(rcAppMainWindow(app));      /* "I changed something. Draw once more" */
    }
}
```

This is the browser's `requestAnimationFrame` bargain: nothing animates for free there either. Three
ways to ask, cheapest first:

```c
RC_Window *win = rcAppMainWindow(app);   /* the surface an RC_App callback is drawing  */

rcWindowRequestFrame(win);               /* draw one more frame, now                   */
rcWindowRequestFrameAfter(win, 1.0);     /* wake in 1s and draw; stay parked until then */
opts.renderMode = RC_RENDER_CONTINUOUS;  /* draw every vsync: a game, a simulation     */
```

Prefer `rcWindowRequestFrameAfter` for anything on a clock (a 1 Hz feed, a poll): it keeps the app
asleep between steps instead of burning ~60 frames to show one update. Reach for
`RC_RENDER_CONTINUOUS` only when the picture really does change every frame; you can also flip it at
runtime with `rcWindowSetContinuousRendering(rcAppMainWindow(app), true)` and turn it off when the
animation ends.

**One timer, earliest-wins.** There is a single outstanding one-shot deadline, so two *independent*
timers coalesce to the sooner one: arm a 5 s poll and a 1 s countdown and you get one wake at 1 s, not
two. Re-arm the later one when you are woken:

```c
rcWindowRequestFrameAfter(rcAppMainWindow(app), next_deadline_in_seconds(st));   /* recompute each wake */
```

Calling it every frame with a shrinking delay is the documented idiom and is unaffected: it is cheap and
always correct. The case to watch is two unrelated schedules assuming they each own a timer.

### The other trap: a hand-rolled loop opts you out entirely

The park lives in `rcRunApp`'s **loop**, not in the frame call. While a window can draw, `rcRunFrame` polls,
renders one frame per eligible window and returns immediately, so this gets none of the above and spins at
full speed (it waits, up to 250 ms per call, only when no window is eligible to draw, such as a minimized
primary):

```c
while (rcRunFrame(app)) { }      /* ← no sleep anywhere: on-demand buys you nothing here */
```

Measured on one still, empty window over the same 8-second idle window: **`rcRunApp` 0.20 CPU-s vs a hand-rolled
`rcRunFrame` loop 115.45, 577×.** Use `rcRunApp` unless you genuinely must own the frame loop (a browser host,
an engine that already has one); if you do, you are responsible for blocking or pacing it yourself.
**Pacing is yours; the two BOUNDS are not.** `maxFrames`/`RAYCLAY_MAX_FRAMES` and
`maxSeconds`/`RAYCLAY_MAX_SECONDS` are both honoured in a split loop - `rcRunFrame` tests the deadline
on entry and returns false, the same edge `rcRunApp` and the web tick test it on.

### The trap: a live fps counter is an animation

If your UI prints `"%.0f fps"` or a frame number, that text changes every frame, so the picture never
settles and the window can never park: one label silently costs you the entire idle win. Drop it, or
accept `RC_RENDER_CONTINUOUS` knowingly. (`ex04` prints the task count rather than a frame counter,
so it parks; copy that shape.)

### If your app freezes

An app that animates without ever asking for a frame stops the moment its window goes idle. Rebuild
the **library** with the digest on and RayClay warns you once, naming the fix; it detects a picture
that changed on a frame nobody requested:

```cmake
target_compile_definitions(rayclay PRIVATE RC_GFX_DIGEST=1)   # ← the LIBRARY target, as for RC_PERF_COUNTERS
```

A plain `cmake -DRC_GFX_DIGEST=1` does **not** turn it on: the knob has to reach the translation
unit that compiles RayClay itself, so a cache variable on your own target sets nothing. Two run-time knobs help too: `RAYCLAY_IDLE_STATS=1` prints
`admitted/waits/deadline/spurious` counters at exit, and `RAYCLAY_MAX_SECONDS=n` bounds a run by wall
clock (a *frame* budget can't bound an event-driven run).

**`RAYCLAY_IDLE_STATS=1` also prints one line per *admission reason***: `initial`, `input`,
`window`, `expose`, `resource`, `app`, `deadline`, `internal`. That breakdown answers the
opposite question, the one that is otherwise pure guesswork: **when a window refuses to park, it names
what keeps waking it.** Your own `rcWindowRequestFrame` shows up as `app`; `rcWindowRequestFrameAfter` and
RayClay's own bounded internal retries show up as `deadline`. A healthy idle app is a short list:
`ex00` parked for four seconds on-demand reports `2 admitted (1 initial, 1 input)`.

### Two behaviours worth knowing

- **A focused text field's caret stops blinking** after 10 seconds without interaction and stays
  solid, resuming on the next keystroke or click. GTK has done this for years (`gtk-cursor-blink-timeout`,
  same 10 s default). It settles **on**, so it never stops marking the insertion point, and it is what
  lets an app with a focused field reach a true zero instead of paying two frames a second forever.
- **A minimized window** skips layout and render entirely. Set `RC_AppOptions.renderWhileMinimized`
  only if a simulation must keep running while hidden.

**On the web this applies too.** The browser owns the frame loop, so RayClay cannot sleep in it;
the animation frame stays registered, but a tick with nothing to draw returns without laying out or
rendering, which is where the cost actually was. A hidden tab is throttled by the browser either
way.

## The first number to look at: `rcWindowFrameCounts()`

The section above is about *how often* you draw. This one is about *how much each draw does*.

Start here, because it needs no build knob and it answers the question that is wrong most often: **how
much layout am I asking for, and how much of it actually reaches the screen?**

```c
static void on_frame_end(RC_App *app, void *userData)
{
    RC_FrameCounts c = rcWindowFrameCounts(rcAppMainWindow(app));
    printf("%u declared -> %u draw commands\n", c.declared, c.drawCommands);
}

opts.frameEndCallback = on_frame_end;
return rcRunApp(&opts);
```

- **`.declared`**: elements your `layoutCallback` declared, *before* culling.
- **`.drawCommands`**: how many of them survived culling and reached the renderer.

The two together are the **over-declaration ratio**, and a large one is the single most common reason a
RayClay frame feels expensive. Declaring an element is never free even when it is culled: it is still
built, measured and laid out first. A measured scene in this project declared **8,002 elements to draw
16**: that frame is not slow in the renderer, and no amount of renderer tuning would have found it.

If the ratio is large, the fix is to declare less, not to draw faster:

| what you see | what to reach for |
|---|---|
| thousands declared, tens drawn, in a scrolling list | **`rcVirtualList`**: it declares only the visible window, so cost stops depending on the row count (measured 254× at 5,000 rows) |
| a large ratio with no list involved | a subtree that is built every frame and hidden by layout; declare it only when it is on screen |
| a ratio near 1 and the frame is *still* expensive | the cost is in the drawing, not the declaring; that is what `rcAppPerfFrame()` below is for |

**Read it between frames: `frameEndCallback` is the right place.** The pair is snapshotted where the
render walk begins, so a read from *inside* `layoutCallback` or `updateCallback` describes the **previous** drawn
frame. Those are correct numbers for an earlier frame, not torn numbers for this one: a half-declared
frame has no meaningful count. Both fields are `0` before the first frame is drawn.

Under the default on-demand scheduling, *"most recent drawn frame"* can be seconds old; it is still the
last frame the user actually saw.

## Measuring what a frame COSTS: `rcAppPerfFrame()`

When the ratio above is not the answer, this is the next instrument: it tells you *what the drawing did*:
vertices, rectangles, text runs, clip pushes. RayClay can hand you the frame's work counters directly, so
you are reading your own app rather than guessing from a profiler.

It is **opt-in and off by default**, because it is an instrument rather than a feature: with the knob off
the library's objects are byte-for-byte identical to a tree without it (RayClay gates that, so it is
checked rather than claimed). Turn it on:

```cmake
target_compile_definitions(rayclay PRIVATE RC_PERF_COUNTERS=1)   # ← the LIBRARY target
```

**The knob must reach the translation unit that compiles RayClay, not just your executable.** This is
the one mistake worth calling out, and it is the reason the line above names `rayclay` and not your app.

**The good news, which is worth knowing before you go hunting: getting it wrong is loud, not silent.**
Both halves are compiler-enforced, so you cannot end up staring at a table of plausible-looking zeroes:

| what you did | what you get |
|---|---|
| knob on your **app only** | a **link error** naming the symbol: `undefined reference to 'rcAppPerfFrame'` |
| knob on the **library only** | a **compile error** in your file: `unknown type name 'RC_PerfFrame'` |
| knob on **both** | counters |

*(Both verified by compiling. The default library genuinely does not define the symbol, and an app TU
with the knob on emits an undefined reference to it.)*

### Where to read them: `frameEndCallback`

Counters are zeroed at the **top** of each frame and filled **during** the render walk. So every field
reads `0` anywhere before the walk, **including inside your `layoutCallback` and inside `updateCallback`**, which both
run after the reset and before anything is drawn. Reading there is the documented way to measure nothing
at all.

**`RC_AppOptions.frameEndCallback` exists for exactly this.** It runs last in the frame, after the overlay
pass, so everything an instrument would want to read is final. Its type is **`RC_FrameEndCallback`**,
the same `(RC_App *app, void *userData)` shape as `updateCallback` and `layoutCallback`:

```c
static void on_frame_end(RC_App *app, void *userData)
{
    const RC_PerfFrame *p = rcAppPerfFrame(app);   /* final, not yet reset */
    printf("%u declared -> %u commands, %u verts\n",
           p->declaredElements, p->renderCommands, p->vertices);
}

opts.frameEndCallback = on_frame_end;
return rcRunApp(&opts);
```

**Use this rather than a hand-rolled `rcAppCreate`/`rcRunFrame` loop.** You *can* read the counters from
a split lifecycle, but you should not reach for one just to measure: the idle win lives in `rcRunApp`'s
loop, and a hand-rolled loop measured **577× the idle CPU** over the same window. **An instrument you can
only reach by building your app the wrong way measures the wrong app.**

**It is called only for frames that actually drew.** Under the default on-demand scheduling an idle frame
does not redraw and the callback does not fire, so *"per frame"* here means **per drawn frame**. That is
usually what you want, and it means your average is not diluted by frames that did no work.

**But "drawn frames" is a LARGER set than the frame budget, so do not use this callback to build a
denominator.** A frame-budgeted run (`RC_AppOptions.maxFrames` / `RAYCLAY_MAX_FRAMES=N`) reports
`rendered N of N budgeted frames` at teardown, and that figure counts **main-loop** frames. The callback
additionally fires for repaints driven outside the loop, chiefly the live-resize path, which redraws
*during* the OS modal resize so a dragged window keeps painting. **Measured on Linux/Xvfb: a budget of 30
produced 31 calls and a budget of 90 produced 91**, the single extra being the refresh delivered when the
window is first mapped; during an actual resize drag it is many more, and a build with
`-DRC_NO_LIVE_RESIZE` sees none. ⇒ **Accumulate totals and averages from your own counter, and if you need
"how many frames did the run render", take the runner's teardown line, not the number of times this
callback fired.**

The pointer is **borrowed** and valid until the next frame overwrites it. Do not free it, and do not keep
it across frames; copy the struct if you need to.

**It returns `NULL` only if you pass a `NULL` app**, so a non-`NULL` return is not a signal that the
numbers are meaningful. Inside `frameEndCallback` the app is always live and the values are always final,
which is the other reason to read them there rather than to invent your own read point and then wonder
whether an all-zero row means "idle" or "not ready yet".

### The fields

| field | what it counts |
|---|---|
| `declaredElements` | elements your layout asked for |
| `renderCommands` | commands left after culling |
| `rects`, `borders`, `shadows`, `texts`, `images` | draws, by primitive |
| `vertices` | **triangle** vertices |
| `clipPushes` | scissor pushes |

All `uint32_t`. **`declaredElements` against `renderCommands` is the pair worth looking at first**: a wide
gap means your layout is declaring work the renderer then throws away, and no amount of render-side tuning
will find that for you.

**`vertices` counts triangle vertices, so a rectangle costs 6, not 4**: it is written as four corners
and expanded into two triangles before it reaches the GPU. Divide by 6 for rectangles, not by 4.

## A titlebar out of the box

Set `.nativeFrame = true` in `RC_AppOptions` and the window goes borderless while the runner
draws a complete titlebar for you, with centred title, drag-to-move, and minimize/maximize/close as
bundled **Flat Slabs**: a square-cornered, full-band-height rectangle behind a stroked glyph,
invisible at rest and filling with its accent on hover; close reaches a near-solid red, minimize
and maximize a faint wash. No radius, no border, no glow, no press bounce. Tune it through
`RC_AppOptions.titlebar`
(`RC_TitlebarOptions`): move the control cluster left (`controlsLeft`), trim buttons, change
the height or colours, or swap any button's per-state icons (`RC_TitlebarButtonIcons`). Want
full control? Set `.titlebar.custom = true` and draw your own band; tag it with the
`RC_ID_WINDOW_*` ids (or place `rcTitlebar(NULL)` wherever you like: it takes an options pointer,
and `NULL` means all defaults) and the runner still performs
the OS actions. **If you put an interactive widget inside your drag band** (a theme toggle, a
search field), **wrap it in an element tagged `RC_ID_WINDOW_NODRAG`**, or the press starts an OS
window-move instead of reaching the widget. This bites on desktop only (the web has no window drag,
so the same code looks perfect there); see
[`widgets.md`](widgets.md#titlebars-keeping-a-widget-clickable-inside-the-drag-band).
Titlebars are **desktop-only**: on the web the browser tab is the window chrome
and on mobile the app is full-screen, so there `rcTitlebar` / `rcWindowControls` /
`rcWindowControlButton` draw nothing and the `RC_ID_WINDOW_*` ids are inert (no OS action,
close included); the same source still compiles and runs everywhere, no `#ifdef` needed.

The bundled band is **fixed chrome**: content zoom never resizes or moves it. Like a browser's own
titlebar it keeps a constant on-screen size, pinned to the window's top edge, while your content
magnifies, reflows and pans beneath it, in *both* zoom modes. The heights and sizes you set in
`RC_TitlebarOptions` are therefore a constant *physical* size. Set `.titlebar.zoomWithContent` to opt
the band back in and let it zoom like any other element.

**A `.titlebar.custom` band is yours to draw, so it zooms with your layout; wrap it in
[`rcUnzoomed()`](api-notes.md#rcunzoomed).** This is not cosmetic: `RC_AppOptions.titlebarHeight`
freezes the strip the OS lets you drag in *physical* pixels, so an unwrapped band draws 92 px over a
46 px drag strip at 2× zoom, and the bar you see stops matching the bar you can grab. The two knobs
are inverses: the bundled band is out of the zoom by default and `zoomWithContent` opts it **in**;
a custom band is in by default and `rcUnzoomed()` opts it **out**.

**The window controls are always on screen.** Two guards hold this, against two different ways of
losing them:

- **Content can't push them off.** The bundled band is sized by the *visible window*, never by what you
  lay out inside it, so no amount of content, however wide, at whatever zoom, moves
  minimize/maximize/close past the edge.
- **Nor can the window shrink under them, and you get that for free.** Leave `RC_AppOptions.minWidth` /
  `.minHeight` at 0 and the resize floor is **derived from the title bar itself**: a RayClay window
  shrinks to its own chrome and no further, which is the width of the control cluster by the height of the caption
  band. That is what a modern desktop app does out of the box. It **tracks what the bar actually draws**:
  `150x38` with the default three slabs, `58x38` once minimize and maximize are hidden, and the height
  follows `titlebar.height`. Read those as *worked examples, not constants*: change the band and the floor
  moves with it, so never hard-code them.
- **The derived floor applies whether or not `nativeFrame` is set.** Presentation is not a size contract: the
  same app toggling its frame style should not gain or lose one, and an OS-decorated RayClay app can still
  lay out `rcTitlebar` itself.
- **Setting a value replaces the default on that axis**, so `minWidth = 900` with `.minHeight` left 0 still
  gets the derived height floor. **One asymmetry worth knowing:** if what you asked for is small enough to
  clip the controls off the band, it is **raised, with one warning naming the override**, but only when
  **RayClay is the one drawing those controls**, which means `nativeFrame` with the bundled bar. An
  OS-decorated window honours a tiny `minWidth` exactly as written, silently, since the chrome there is the
  OS's business; and so does `titlebar.custom`, because a band you draw owns its own controls.
- Both fields are **ignored on the web**, where the browser owns the window size.

If you genuinely want a window that can be sized smaller than its own controls, say so deliberately with
`RC_AppOptions.titlebar.allowControlsClip = true`; then your `minWidth` is honoured exactly as written
and nothing is raised. It exists so the guard is an opt-*out*, never a surprise.

## Zoom, on by default

Every RayClay desktop app zooms like a browser out of the box: `Ctrl` `+`/`-` walk a **ladder of zoom
stops**, **`Ctrl` `0` puts it back to 100%** (keypad `0` too), and `Ctrl` + scroll zooms smoothly; on
macOS the accelerator is `Cmd` (physical
`Ctrl` there keeps the text-navigation variants).

**The keys land on round numbers, because that is what a browser does.** The bundled ladder is Chrome's:

```
25  33  50  67  75  80  90  100  110  125  150  175  200  250  300  400  500   (%)
```

so `Ctrl` `+` from 100% gives 110, 125, 150, 175, 200; never 121% or 146%. **The wheel is different on
purpose: it stays continuous**, so a notch lands wherever you stop, and the next keypress moves to the
first stop beyond it. Two knobs, and you rarely want either:

- `.zoom.ladder` / `.zoom.ladderCount` supply your own stops: ascending, positive, at least two.
  A drawing app might want `{ 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f }`. The array is **not copied**, so
  it has to outlive the app; a `static` or a literal is the intended shape. An invalid table warns once
  and falls back to the bundled one rather than zooming unpredictably.
- `.zoom.step` is the **opt-out**: set it (say `1.05f`) and the keyboard goes continuous at that
  factor, turning the ladder off for your app.

**If you draw your own zoom UI, read the stops back rather than copying them:**

```c
uint16_t n = 0;
const float *stops = rcAppZoomLadder(app, &n);   /* the stops the KEYS actually walk */
for (uint16_t i = 0; i < n; i++)                 /* stops[i] is a factor: 1.25f == 125% */
    ...
```

It hands back your table if you supplied a valid one and RayClay's if you did not, so a preset row can
never disagree with the keyboard. **It returns `NULL` with `n == 0` when there is no ladder to show**,
i.e. you set `.step`, and the honest response to that is to draw no preset chips at all, not to fall
back to a hard-coded list the keys will never visit.

By default this is **layout zoom**
(`RC_ZOOM_LAYOUT`): the whole UI **reflows** (layout
re-runs and text re-wraps into a rescaled logical viewport, exactly like a browser's `Ctrl +`)
rather than magnifying a snapshot. Geometry and the built-in icons stay resolution-independent at
any factor, and **settled text is native-resolution too** (within the atlas's capacity): once a zoom
gesture settles (a few frames) the runner re-bakes the glyph atlas at the density the renderer is
actually drawing at, so even a fractional zoom lands on real pixels rather than a rounded guess. On a
`nativeFrame` window that density is the *larger* of your content's demand and the fixed titlebar's:
the band counter-scales to hold its physical size, so below 100% it is drawing **bigger** glyphs than
your content, and baking for the larger of the two is what keeps the bar's title crisp when you zoom
out. Only the brief mid-gesture moment shows the previous bake magnified. At an extreme zoom × display
scale a very large face can overflow the fixed glyph atlas: the re-bake then searches down for the
highest density the sheet can *actually* hold, so text goes slightly soft (a bounded soft fallback),
never missing; the log says so once per episode, not once per zoom step. **Where you land is a property
of the atlas, not of your gesture**: the ceiling is the sheet's, and no way of arriving at a factor gets
you past it.

**It is a ceiling, not a fixed number.** The search that finds it is a bisect that stops within 1%,
so the first hold slightly *under*-resolves the sheet; a later, larger request re-enters with a tighter
bracket and can land a shade higher. Measured on the shipped 1024×1024 sheet, the held density creeps
from **3.64 to 3.675** across an episode: under 1%, invisible on screen, and the search converging on
the true ceiling rather than drifting off it. **So do not build a test that asserts an exact held
density**, and do not expect two routes to the same zoom factor to agree to the last decimal. The
actionable symptom is the one the log names: **text has stopped tracking the zoom.**

**Text softens in *two* separate places, and the first one arrives much earlier than the second.** They
have different causes and different fixes, so it is worth keeping them apart:

### 1. The oversampling cliff (early, per-face)

Small text is baked with **2× horizontal oversampling**, which is what lets a glyph land on a fractional
pixel without smearing. That is switched off once a face **bakes** past 36 px, on the reasoning that
large text does not need it.

But the size a face *bakes* at is `authored size × display scale × zoom`, so a face that is **small to
the user** can still bake large, purely because the screen is HiDPI. **Each face therefore drops its
oversampling at its own zoom level, when `authored × scale × zoom` reaches 36 px:**

| authored size | drops oversampling at (1× display) | **at (2× Retina)** |
|---|---|---|
| 12px | 300% zoom | 150% |
| 14px | 257% | 129% |
| 16px | 225% | 112% |
| **18px** | 200% | **100%, i.e. at rest, before you zoom at all** |
| 24px | 150% | 75% |

**On a Retina panel, any face you author at 18px or larger never gets oversampling.** Measured against a
supersampled reference, crossing that boundary costs about **a third more error** (18.5 → 25.3 on a
0–255 scale), from a *0.1 px* change in bake size. Text does not break; it gets softer, and it gets
softer as you zoom *in*, which is the opposite of what anyone expects.

**The knob is `RC_AppOptions.fontOversample`.** Set it (1–8) and it *replaces* the policy, on **both**
axes, at every size, so the 36 px cliff never fires. `= 2` roughly **halves** the error (25.3 → 12.2).

**But it is not free, and for some apps it is a bad trade; and what it costs depends on where you
look.** At ordinary text sizes it roughly **doubles** the sheet each glyph takes, because the automatic
policy was *already* oversampling 2× horizontally and you are only adding the vertical axis. But your
zoom ceiling is set by the far end, where every face is baking past 36 px; and *there* the automatic
policy has already dropped to no oversampling at all, so the same `= 2` is paying the full **4×**. That
is why it **halves your zoom ceiling** rather than merely denting it (below): the widget gallery's own
`14/18/30/56` falls from an effective scale of **3.04 to 1.51**, which on a Retina panel means it could
not bake cleanly even at **100%** zoom. A lean `14/18/24` ladder halves too, but from 6.36 to 3.13; it
can afford it. **Measure your own ladder before reaching for it.**

### 2. The atlas ceiling (later, whole-ladder)

**This one is on the *product*, not on zoom alone, and where it lands is decided by *your font
ladder*.** What the atlas must bake is `display scale × zoom`, so a HiDPI panel spends part of your
headroom before the user zooms at all: a 2× Retina display hits the soft-fallback point at roughly
*half* the zoom factor a 1× display does. And because every size in `.fontSizes[]` is re-baked into the
same fixed sheet, **the more (and larger) sizes you declare, the sooner that point arrives.** Measured
against the bundled face on the default 1024² atlas:

| your `.fontSizes[]` ladder | crisp up to `scale × zoom` | on a 1× display | on a 2× Retina |
|---|---|---|---|
| one 14px size | ≈ 14.3 | past 500% | past 500% |
| 14 / 18 / 24 | ≈ 6.4 | past 500% | ≈ 320% |
| 14 / 18 / 30 / 56 *(ex10's)* | ≈ 3.0 | ≈ 300% | **≈ 152%** |
| eight sizes, 12–64 | ≈ 2.1 | ≈ 215% | ≈ 105% |

These are the **first** density at which text softens, which is the one that matters: you zoom
*continuously*, so you meet it on the way up. (Oddly, a few *denser* bakes above it fit again: past 36 px a
face drops its horizontal oversampling and each glyph suddenly takes half the width, so the sheet can
briefly get *roomier* as you zoom in. Don't plan around those islands: you have already passed through the
soft band to reach them.)

The lever hiding in that table is **the ladder itself, and it is free.** A glyph's atlas footprint grows
with the *square* of its baked size, so a single 56px display face costs about as much sheet as sixteen
14px ones: trimming one oversized heading you never really needed can more than double your zoom
headroom, at no cost in RAM. (Drawing accented Latin-1 characters lowers it too: they are only baked once
actually used, so an app that stays in ASCII keeps roughly a third more headroom than one that does not.)

Past the ceiling nothing clamps and nothing disappears: the range is still the full 25–500%, and text
goes *softly* rather than missing. If you need more headroom than the ladder can buy, `RC_FONT_ATLAS_W/_H`
raises the sheet, but the cost is quadratic in both CPU and GPU memory (see the cheatsheet), and that
trades directly against RayClay's small-footprint promise.

A few things to know as an app author:

- **`Ctrl` + wheel goes to zoom**, so `rcScrollDeltaX/Y()` return 0 on those frames.
- `rcGetWindowDimensions()` stays in real logical pixels; under zoom, layout and the pointer use a
  zoom-divided space, so don't mix the two.
- Raster images (`RC_Image`) are bitmaps: zoom magnifies them, so they soften above 100%, exactly
  as in a browser.

**Prefer a canvas-style magnify?** Set `.zoom.mode = RC_ZOOM_OPTICAL` and zoom leaves the layout
untouched and scales the whole rendered surface instead, like Figma, a map, or a PDF viewer. The
layout does not reflow: it is a fixed logical surface that the renderer scales. Zoomed **in**, that
surface is the window, so content past the viewport edge overflows; the zoom magnifies
**about the cursor** (not the top-left corner) and clamps the pan so the surface never overscrolls
past its own edges. Zoomed **out**, the shrink means the window now shows *more* logical space than it
is wide, and RayClay lays out into that larger visible rectangle, so a `grow` root keeps filling the
whole window instead of shrinking away into a corner, and nothing that is plainly on screen gets
culled for sitting past the window's logical width.

Crispness works the same way as in layout mode: the surface is *re-rendered*
through a scaled projection rather than blitted, so geometry and icons stay vector-crisp and glyphs
re-bake on settle; only raster `RC_Image` bitmaps magnify and soften. Layout zoom is the default
because it's the web-app behaviour most desktop apps want; the *starting* mode is set per app
(`RC_AppOptions.zoom.mode`), and you can flip between the two at runtime with `rcWindowSetZoomMode()`,
e.g. behind a "View → Zoom mode" menu item or a toolbar button:

**Magnifying is only half of it: turn on `.zoom.pan` too.** Optical zoom keeps the point under the
cursor still across a factor change, but that is *anchoring*, not travel: on its own, a surface
magnified to 400% gives you no way to reach its own edges. `.zoom.pan = true` adds **drag-to-pan**:
hold `.bindPan` (Space by default) and drag with the left button, exactly as Figma does.

```c
.zoom = { .mode = RC_ZOOM_OPTICAL, .pan = true },   /* magnify AND travel */
```

It is **off by default, and that default is right**: a browser has no pan, and out of the box a
RayClay app is a browser. Turn it on for canvas-shaped apps (maps, diagrams, image work) and leave
it alone for ordinary UI. The cursor hints `grab` / `grabbing` while it is armed, and unlike the zoom
*keys* it **works on the web build too**: the browser owns `Ctrl` `+`/`-`/wheel, but it owns no
space-drag, so the gesture is one of the few that is genuinely identical on both targets.

Three things worth knowing before you enable it:

- It does nothing under `RC_ZOOM_LAYOUT`: reflow puts everything inside the window, so there is
  nothing outside to reach.
- **Space is a content key**, so panning is suppressed while an `rcTextInput` holds focus: a space
  typed into a field can never drag the view out from under your caret. The suppression is tied to the
  *gesture*, not to the Space key; rebind `.bindPan` and it still applies.
- **A drag already in progress is never stolen.** It arms only on the button's *down* edge with the key
  already held, so pressing Space midway through a slider drag or a text selection does nothing.

```c
if (rcClicked("zoom_mode_toggle")) {                       /* in your layout / update */
    RC_Window *win = rcAppMainWindow(app);
    RC_ZoomMode m  = rcWindowZoomMode(win);
    rcWindowSetZoomMode(win, m == RC_ZOOM_LAYOUT ? RC_ZOOM_OPTICAL : RC_ZOOM_LAYOUT);
}
```

The switch takes effect next frame and preserves the current zoom factor. (Want a *keyboard* shortcut
for the toggle? Poll `rcKeyPressed` yourself; see [widgets.md](widgets.md) → "Keyboard".)

`RC_AppOptions.fontOversample` (rounded, clamped 1–8) sharpens the **mid-gesture** transient, and, more
importantly, *replaces* the automatic oversampling policy, so the 36 px cliff above never fires. Read the
trade in "Zoom, on by default" before setting it: it costs atlas capacity, and therefore zoom ceiling.
Out of the box the glyph atlas is oversampled horizontally but only **1× vertically**, so *while a
zoom gesture is in flight* text softens vertically first; set it to **`2`** and that transient stays
crisp on *both* axes through roughly 2× zoom. (Settled text is re-baked to native resolution either
way; this only smooths the few frames before the re-bake lands.) Higher factors sharpen further but cost atlas RAM fast: the
oversample applies to both axes, so a glyph's footprint grows with its **square**, and the atlas is a
single fixed 1024² sheet shared by every face and size. Ask for more than fits, e.g. `8` at a large
face, and RayClay quietly **retries at a halved oversample** (down to 1×1, one `WARN` per step) so
the face still loads, just softer than you asked; only a genuinely full atlas falls back to the
default font. A high value is therefore never *invisible*, but it may be silently reduced at larger
sizes (watch the log: on the web those `WARN` lines land in the browser console, or route them into
your own UI with `rcSetLogSink`). Pick the
smallest value that meets your crispness need: `2` is the zoom sweet spot; reserve the high end for a
single small face.

Want Chrome-style zoom visibility? The change trigger is a one-liner; the runner applies zoom
gestures *before* your callbacks, so `rcWindowZoom()` already reflects a gesture from this frame:

```c
RC_Window *win = rcAppMainWindow(app);
float z = rcWindowZoom(win);                      /* in updateCallback */
if (z != st->prevZoom) {
    if (st->prevZoom > 0) st->badgeSecs = 1.5f;   /* first tick only seeds it */
    st->prevZoom = z;
}
if (st->badgeSecs > 0) st->badgeSecs -= rcWindowFrameTime(win);
```

then, while `badgeSecs > 0`, float a small "125%" pill over the UI (a `.floating` box with
`RC_ATTACH_ROOT`); the gallery (`ex10`) carries the full ~15-line pattern to copy.

Configure or disable it via `RC_AppOptions.zoom` (an `RC_ZoomOptions`: `.disabled`, `.minZoom` /
`.maxZoom`, `.step`, `.mode`, `.pan` / `.bindPan`, rebinds (`.bindZoomIn` / `.bindZoomOut` /
`.bindZoomReset`: `RC_Key`
values such as `RC_KEY_I`; leave them `RC_KEY_NONE` (`0`) for the default bindings), and
per-gesture switches; zero-init means on, 25–500%, layout mode, pan off). Read or drive it at runtime with
`rcWindowZoom()` / `rcWindowSetZoom()` (e.g. a "Reset zoom" menu item), and read the display's
device-pixel-ratio with `rcGetContentScale()`. On the web the zoom *gestures* are inert (the
browser owns zoom in a tab), while `rcWindowSetZoom()` still applies (a reflow in the default layout
mode) and `rcGetContentScale()` returns the display's true device-pixel-ratio there (e.g. `2.0` on a
2× display), because the web backing store renders at device pixels.

**To see your app at another zoom without touching its code, set `RAYCLAY_ZOOM=<float>`.** It replaces
the starting zoom for that run, so `RAYCLAY_ZOOM=1.5 ./build-desktop/myapp` opens at 150%: one binary
sampled at several zooms, no rebuild. It is applied *before the first layout*, so frame 1 is already at
the requested zoom and a first-frame screenshot is honest.

```bash
RAYCLAY_ZOOM=1.5 ./build-desktop/myapp        # opens at 150%
RAYCLAY_ZOOM=0.5 ./build-desktop/myapp        # opens at 50%; check your layout still holds together
```

It is clamped to the app's own `.minZoom` / `.maxZoom` and says so, naming both the value you asked for
and the one it used. A value that does not parse, or that is not positive, is **ignored rather than
guessed at**, again with one warning, so a typo (`2O` for `2.0`) shows up as a sentence in the log
instead of a degenerate layout that reads like a RayClay bug. An app that sets `.disabled` is never
forced into zoom by the environment.

> **`rcGetContentScale()` reads framebuffer ÷ window size.** Where a platform spends the HiDPI factor
> on the FRAMEBUFFER, that ratio is the true DPR. Where it spends the same factor on the WINDOW SIZE
> instead - multiplying the size you asked for by the monitor's content scale - the ratio reads `1.0`
> at every DPI, and that is not a missing feature, it is where the desktop scale went. **The ratio is
> only informative where the factor landed on the framebuffer**, so do not treat it as a DPR you can
> rely on cross-platform.
> **So never multiply either size by a display scale to get device pixels.** Whichever of the two the
> platform spent the factor on, it has already been spent. **The device-pixel count is the framebuffer
> size on every platform, full stop**: multiplying applies the factor twice.

## Headless / CI

Set `RAYCLAY_MAX_FRAMES=N` to bound any app to N frames and exit 0, without changing its code (an explicit
`RC_AppOptions.maxFrames` always wins). Related knobs: `RAYCLAY_MAX_SECONDS=n` bounds by wall clock,
`RAYCLAY_IDLE_STATS=1` prints scheduler counters at exit, `RAYCLAY_RENDER_MODE={continuous|ondemand}`
overrides the mode, `RAYCLAY_FIXED_DT=s` pins a deterministic frame delta for benchmarking, and
`RAYCLAY_ZOOM=f` starts at a given zoom factor (see *Zoom* above).

Two more are for measuring rather than running: **`RAYCLAY_MSAA=1|2|4`** overrides the compiled
antialiasing count for one process (so you can compare arms without rebuilding, and `1` is how you
spell *off*), and **`RAYCLAY_GFX_STATS=1`** prints one line per window naming the sample count you
asked for, the one the framebuffer actually gave you, and the backing-store size in device pixels.
Both are silent unless you ask. See [api-notes.md ▸ RC_GFX_MSAA_SAMPLES](api-notes.md).

One is for laying out a phone screen on a desk: **`RAYCLAY_SAFE_INSETS=top,right,bottom,left`** (window
pixels, like the real hook) makes `rcGetSafeAreaInsets()` and `rcViewport().safe` report those bands on a
desktop, where the host reports none, so a 393x852 window shows the status-bar and home-indicator bands a
device would spend. A phone keeps its own numbers; a malformed value is ignored.

```bash
RAYCLAY_MAX_FRAMES=3 ./build-desktop/myapp   # runs 3 frames, exits 0
```

**A FRAME BUDGET SILENTLY SWITCHES YOU TO CONTINUOUS RENDERING, so this cannot smoke-test the default.**
Setting `maxFrames` or `RAYCLAY_MAX_FRAMES` (or `RAYCLAY_FIXED_DT`) forces `RC_RENDER_CONTINUOUS`, because a
budget of N frames is meaningless if the runner is allowed to park instead of drawing them. The substitution is
deliberate and correct, but it means **a `RAYCLAY_MAX_FRAMES` run exercises a different render mode than your
users get**, and nothing in the output says so. Anything that only happens on the on-demand path (idle CPU
near zero, parking, a frame that is never requested) is invisible to it, and a bug that lives there will pass.

**To bound a run and stay on the default path, bound by TIME**. There is an API field for it, so
this needs no environment variable at all:

```c
RC_AppOptions opts = { .layoutCallback = layout, .maxSeconds = 3.0 };  /* on-demand survives this */
```

```bash
RAYCLAY_MAX_SECONDS=3 ./build-desktop/myapp   # overrides the field: sample several durations, no rebuild
```

Neither `RC_AppOptions.maxSeconds` nor `RAYCLAY_MAX_SECONDS` is a forcing knob, so both bound the run without
substituting the mode. **`maxSeconds` is the only bound that leaves on-demand intact**, which makes it the
only way to test RayClay's headline on-demand behaviour.
Expect **very few frames**: an idle on-demand app that draws once and parks is working correctly, and a
frame count near 1 is the success signal, not a failure. Add `RAYCLAY_IDLE_STATS=1` to see the scheduler's
own counters rather than inferring from frame totals.

**A frame budget wins over `RAYCLAY_RENDER_MODE=ondemand`, and tells you so.** A budget only means
anything if the frames actually arrive, so `maxFrames` / `RAYCLAY_MAX_FRAMES` and `RAYCLAY_FIXED_DT`
each force continuous. Asking for `ondemand` on top is **ignored**, with a warning naming which knob
won; the run still terminates. **To bound an on-demand run without leaving on-demand, bound it by
TIME** (`RAYCLAY_MAX_SECONDS`): wall clock needs no frames, so it is the only bound that leaves the
mode you are trying to test intact.

**Know exactly what this proves, because it is less than it looks.** It is a **crash and assertion** smoke
test: it proves your app starts, completes N main-loop ITERATIONS, and exits without dying. **It does not
prove anything was drawn**: a frame that lays out correctly and renders nothing visible still exits `0`.
**Nor is an iteration always a layout:** a minimized desktop window without `renderWhileMinimized` parks and
skips layout and render entirely while the counter still advances, so a run bounded
by frames can finish its budget having drawn fewer times than it counted.

**What it *does* catch, reliably: no display at all.** With the window system genuinely unavailable, RayClay
fails loudly and exits non-zero; it does not pretend to have rendered:

```
RAYCLAY[ERROR]: rc_window (host): initialize video/events: No available video device
RAYCLAY[ERROR]: rcInitWindow: window-system init failed
RAYCLAY[ERROR]: rcAppCreate: window creation failed        # exit 1
```

**The text after `initialize video/events:` is the windowing system's own, not RayClay's**, so it
varies by platform and by cause. Match on the RayClay half of the line and treat the tail as detail.

**Your host may also print untagged lines around these.** The same run on a Linux desktop emits
`error: XDG_RUNTIME_DIR is invalid or not set in the environment.` twice, with no `RAYCLAY[...]`
prefix, because it comes from the system libraries rather than from RayClay. A CI step that fails on
`RAYCLAY[ERROR]` will not see those; do not be surprised by them in the log.

**If you want to reproduce that on a Linux desktop, unset all three of `DISPLAY`, `WAYLAND_DISPLAY` **and**
`XDG_RUNTIME_DIR`.** A Wayland client does not need `WAYLAND_DISPLAY`: it falls back to the default
socket `wayland-0` inside `$XDG_RUNTIME_DIR`, so dropping only the first two leaves your app quietly running
against your real desktop and "proves" nothing. That is a property of the Wayland client library
underneath, so it holds whichever windowing host RayClay was built with.

Use the smoke test for what it is good at (catching a crash, an assert, a bad font path, an unbalanced
element tree, and add a real oracle on top if you need to prove pixels:

- **Give CI a display** (`xvfb-run -a -s "-screen 0 1280x800x24" ./myapp`) and capture the window to compare.
  On a Linux box with a live Wayland session, `xvfb-run` **alone does not isolate you**: the window
  host prefers Wayland and will use the real compositor. Use
  `env -u WAYLAND_DISPLAY -u XDG_RUNTIME_DIR xvfb-run -a ./myapp` to actually land on X11.
- **Watch stderr, not just the exit code.** RayClay reports layout and asset problems there and still exits 0
  An unbalanced element tree logs *once* and the run stays "green" for every frame after it. Because it is
  a single line and not a repeating one, grepping a tail of the log will miss it: fail your CI step on any
  `RAYCLAY[WARNING]` (and `RAYCLAY[ERROR]`) across the *whole* run if you want those caught.
  **One warning is expected, and it is the one you are most likely to hit first:** an app with no
  `.layoutCallback` logs `rcRunApp: no layout callback supplied` and draws the welcome canvas. So if your
  smoke test *is* `rcRunApp(NULL)`, that absolute rule fails on the very first run. Give the smoke app a
  one-line layout callback and keep the rule absolute; that is the better trade, because a carve-out you
  add today is a carve-out that hides a real warning later.
**ONE CATEGORY OF DIAGNOSTIC IS NOT ABOUT YOUR APP, AND THE ABSOLUTE RULE HAS TO KNOW IT.** RayClay
  FORWARDS every error the windowing system raises, and some of those are recoverable conditions a
  correct app still hits. They are all tagged **`rc_window (host):`**, a single, greppable category:

```
RAYCLAY[WARNING]: rc_window (host): X11: Standard cursor shape unavailable (0x1000B)
```

  **The one you will actually meet is a bare X server with no cursor theme**, so the standard
  cursors cannot be created. RayClay classifies the recoverable platform gaps as `WARNING` and
  reports each one once per run; a genuine window-system failure stays `ERROR`. Either way the tag
  is the same, so the filter below does not need to know the difference.

  **SO EXCLUDE THAT ONE CATEGORY, KEEP EVERYTHING ELSE ABSOLUTE - AND KEEP THE EXIT CODE.** The tag
  is emitted from exactly one function, so the filter stays a single line rather than the growing
  carve-out list the paragraph above warns against. What it costs you is not nothing, and the honest
  version is worth two sentences:

  **A startup failure is still caught, because the layer above re-reports it without the tag.** The
  block earlier in this section is the whole log of a run with no display: the `(host)` line is
  followed by `rcInitWindow: window-system init failed` and `rcAppCreate: window creation failed`,
  neither tagged, and the run exits non-zero. Filtering the tag loses the detail, not the failure.

  **The tag is reserved for diagnostics a CORRECT app still hits**, and a genuinely fatal window
  failure deliberately uses the UNTAGGED `rc_window:` form so this filter cannot swallow it. That
  holds for the ones with no other symptom too: a failed present or a failed event wait log untagged
  and make `rcRunApp` return non-zero rather than ending like a clean quit.
  **Fail your CI step on a non-zero exit as well as on the log** anyway. It costs one line and it is
  the half that does not depend on anyone getting a log tag right.

  **THE TAG NAMES THE ROLE, NOT THE WINDOWING LIBRARY, AND THAT IS LOAD BEARING FOR YOU.** It reads
  `(host)` on every backend RayClay ships, so this recipe does not change when the library's internal
  window host does - and RayClay's host is not part of its public surface, so you should never have to
  care which one you got. Match on `(host)` in a CI script and it will not need updating again.

```bash
if ./myapp 2>&1 | grep -v 'rc_window (host):' | grep -q 'RAYCLAY\[\(WARNING\|ERROR\)\]'; then
    echo "RayClay reported a problem"; exit 1
fi
```

  **Exclude it from FAILING, not from your LOG.** Print those lines; they are how you find out your
  CI image has no cursor theme. What they must not do is red a green run.

**AND FOR THE CURSOR CASE SPECIFICALLY, YOU CAN JUST FIX IT**. Point X at a theme and the errors
  stop, which beats filtering:

```bash
env -u WAYLAND_DISPLAY -u XDG_RUNTIME_DIR \
    XCURSOR_PATH=/usr/share/icons XCURSOR_THEME=Adwaita \
    xvfb-run -a -s "-screen 0 1280x800x24" ./myapp        # measured: 1 warning -> 0, exit 0
```

  **Measured, and the obvious diagnosis is wrong.** Keeping `XDG_RUNTIME_DIR` set also makes the
  errors vanish, but only because the app then reaches your REAL Wayland compositor, which is the trap
  warned about above rather than a fix. Point it at an EMPTY directory instead and you land on Xvfb
  with the variable present and the errors return, which is what isolates the cursor theme as the
  cause. The count depends on your X server's theme, not on your app, so do not pin it.

- **On the web the equivalent check is cheap and real**: assert the canvas has a live WebGL2 context and the
  console is quiet. See [web-build.md](web-build.md) §5, layer 3.

---

**That's the whole story: two lines, one command per target, host it, grow it.** Same source,
every screen.
