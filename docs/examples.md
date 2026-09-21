# RayClay examples

Every example is **one translation unit** - one `.c`, or `main.cpp` for the C++ example `ex06` (`ex10`, `ex11` and
`ex12` each add one local header; each `ex20`+ app adds three to five under `src/app/` and
`src/platform/`) and carries **no asset files of its own** (the font is bundled and icons are
procedural). **Every one builds unchanged on all five native targets** - Linux, macOS, Windows,
Android and iOS. Some carry an INTENT, and an intent changes what happens around the build, never
whether it happens: `ex11`, the SVG converter, is designed for a desktop because it reads and writes
a folder you point it at, and the `ex30`+ band is designed for a phone. A configure building
something outside its intent says so in one warning. Those three are also the three with no web
page.

**Structure:** `ex24_svg_live` is the reference for the project layout every app should use -
`src/main.c` plus headers under `src/app/` plus an empty `src/platform/`. See
[How to structure a RayClay app](app-structure.md); the whole `ex20`+ band is on it - all seven of
`ex20`..`ex24`, `ex30` and `ex32`, one `.c` each, no stray sources at the example root. **On a phone, read [Getting started ▸ One source, every screen size](getting-started.md) first.**
Every example spends `rcViewport().safe` once at its root, so nothing is drawn under the status bar
or the home indicator, and the bench apps share one responsive seam (`examples/bench/bench_app.h`)
that picks a compact or a wide layout from the viewport width and never from the operating system.
That is the pattern to copy, and it comes with one rule every example keeps: **portrait is the
contract.** Everything an example shows or lets you do on a desktop is present in a 393-unit
portrait phone, reachable by scrolling; a wider window only rearranges the same things
([app-structure.md ▸ Portrait is the contract](app-structure.md#portrait-is-the-contract-landscape-is-a-presentation)).
Every mobile-built example has a narrow arm, and they are worth reading together because each
answers a different question, and the answer is never "the same layout, smaller":

- **`ex20` is the DENSITY case.** Nothing to hide, simply too wide: the cards rewrap, the cores
  pane stacks under the chart, the columns the compact table cannot show ride as a second line in
  each row with a chip per sort order, and the page scrolls.
- **`ex23` and `ex21` are the PANE case.** Panes that cannot share a phone screen take turns.
  `ex21` adds the two things a data TABLE needs - a narrow COLUMN SET rather than squeezed
  columns, because five fixed columns are a floor no shrinking reaches, with the folded columns as
  a muted second line and their sort orders as chips, and a pane switch that keeps the virtualized
  list out of a second scroll container.
- **`ex22` sizes the COLUMN itself**, from the font's average character width toward a comfortable
  measure, and moves its contents list from the rail into the reading column when the measure
  would suffer.
- **`ex24` STACKS rather than switching**, deliberately: its whole subject is three drawing routes
  producing the same picture, and a tab strip would hide the comparison.
- **`ex10` and `ex12` are the SINGLE-COLUMN case.** A two-column gallery and a split-pane
  inspector become one scrolling column below the width their own panes add up to; fixed-width
  rows wrap, dialogs size to the viewport, sliders grow.
- **`ex05` is the CHROME case.** A sidebar becomes a labelled segment strip; the readout and the
  pill the narrow band cannot hold move to the foot of the page rather than disappearing.
- **The bench apps** (`messenger`, `notes`, `trader`, `gallery`, `opsdash`) each derive one
  breakpoint from their own pane widths and go one pane at a time or one scrolling page below it;
  `platformer` fills any screen.

The decade demos (`ex00`-`ex04`) are single-screen programs that already fit a phone; `ex03`
scrolls its page in a short landscape window rather than squeezing the playlist.

The suite shares one `examples/assets/` folder for its icon headers, SVG sources, font faces and logo
art. Three demos read real files from it at run time, because reading real files is the thing they
demonstrate: `ex10` a PNG (`RC_Image`), `ex22` font faces (`rcRegisterFont`) and `ex24` SVG sources
(`rcLoadSvg`), and **each keeps working when the file is absent**, saying so in the UI rather than
failing. `ex11`, a dev tool intended for a desktop, reads a folder of SVGs *you* point it at.

They fall into three numbered ranges plus the benchmark suite:

- **`ex00`–`ex09`: learn RayClay.** `ex00` is the smallest possible app; `ex01`–`ex05` walk one
  GUI per decade, each a little richer than the last, so you can see both the *style* and the
  *complexity* of GUIs grow over time, and how RayClay expresses each; `ex06` is the same library
  from a C++ translation unit, the file to copy if your application is C++.
- **`ex10`–`ex19`: RayClay dev tools.** `ex10` is a copy-paste gallery of every widget; `ex11` is
  a desktop app that converts SVG files into RayClay icon headers (a **tool demo, not a step you
  are expected to take**), and it swaps two of its own window-control glyphs, which is where the
  per-button `RC_TitlebarButtonIcons` override is shown; `ex12` is a live inspector / test harness
  that reads the runner's own metrics and can tear its diagnostics off into a focus-free watch
  window.
- **`ex20`–`ex29`: full demo apps.** Complete, realistic applications, single-source like the
  learning set rather than split like the bench suite. `ex20` is a system monitor; `ex21` is a data
  explorer; `ex22` is a documentation reader; `ex23` is an issue inbox; `ex24` puts the vector
  routes side by side.
- **`ex30`-`ex39`: examples intended for a phone.** Apps that exist for what a phone needs and a desktop
  does not: `ex30` is a sign-up form under the soft keyboard (the focused field stays above it,
  read from `rcViewport().keyboard`); `ex32` is a portrait-locked tab navigator (a bottom tab bar,
  list-to-detail push stacks with a back control and the Android back button, per-tab scroll
  kept across tab switches). They are installed on iOS and Android, and they build everywhere the
  others do: a desktop configure gives each one a window at the phone's size (393x852). There is
  no web page: the web is not a phone.
- **`examples/bench/`: six full applications.** Realistic apps (a messenger, a notes app, a
  trading terminal, a photo gallery, a platformer, an operations dashboard) that double as the
  project's performance benchmarks. See [The benchmark suite](#the-benchmark-suite) below.

## The lineup

> **A NAME THAT IS A LINK IS ONE YOU CAN READ ONLINE; A NAME THAT IS NOT, YOU CANNOT.** A row whose
> name carries no link is browsable only in the source drop, never on the web - and it is in the drop,
> and it builds with the rest. The row carries its description alone rather than linking you to a 404.

| Example | Era | What it is | What it teaches |
|---------|-----|-----------|-----------------|
| [`ex00_hello`](https://github.com/impizulu/rayclay/blob/examples/examples/ex00_hello/hello.c) | - | Two lines → a welcome window | `rcRunApp(NULL)`; the one-source promise, and the one startup warning that tells you the canvas is a fallback, not your app |
| [`ex01_1980s_gui`](https://github.com/impizulu/rayclay/blob/examples/examples/ex01_1980s_gui/main.c) | 1980s | A 1-bit desktop calculator | Styling raw boxes into buttons (`rcClicked`/`rcIsHovered`), a menu bar, a custom monochrome theme |
| [`ex02_1990s_gui`](https://github.com/impizulu/rayclay/blob/examples/examples/ex02_1990s_gui/main.c) | 1990s | A Windows 95 settings dialog | Tabs, checkboxes, radios, combos, sliders; building a two-tone 3D bevel by nesting boxes, and composing a **live monitor preview** out of those same bevels so the page previews what its controls do |
| [`ex03_2000s_gui`](https://github.com/impizulu/rayclay/blob/examples/examples/ex03_2000s_gui/main.c) | 2000s | An Aqua / Winamp media player | Gradients (`RC_Gradient`), drop shadows (`RC_Shadow`), sliders, a progress bar, a scrolling page with a playlist. Also the **clock lesson**: the transport advances by the delta between two `rcAppTime` readings, not by a fixed step per frame, so the track and its `3:49 / 4:12` readout keep real seconds on a 240 Hz panel as well as a 60 Hz one |
| [`ex04_2010s_gui`](https://github.com/impizulu/rayclay/blob/examples/examples/ex04_2010s_gui/main.c) | 2010s | A flat Material tasks app | Cards, a floating action button (`RC_Float`), a live list with add + deferred delete. Its app bar is the **two-scope band**: a plain surface column carries the safe-area insets (layout units) and the drag row sits `rcUnzoomed()` inside it, because an inset spent inside that scope is counter-scaled by the zoom |
| [`ex05_2020s_gui`](https://github.com/impizulu/rayclay/blob/examples/examples/ex05_2020s_gui/main.c) | 2020s | A modern dark SaaS dashboard | Theming, a sidebar + tabbed content, stat cards, soft shadows, a gradient accent |
| `ex06_cpp_kanban` | - | A three-lane kanban board, written in C++ | The **C++ consumer**: the same DSL from a C++20 translation unit, with the board held in `std::array` / `std::vector` / `std::string` and a capture-less lambda as the layout callback. The four things that differ from C are numbered in its header and shown in the code: designators in **declaration order** (g++ hard-errors otherwise), **RayClay borrows your strings until the frame is drawn** (so a card move is recorded and applied at the top of the next layout, never mid-layout), a lambda converts to the C callback pointer, array designators are C-only, and nothing else changes. Below the `md` breakpoint the three lanes become one lane behind a segmented switcher, read from `rcViewport()` |
| [`ex10_rayclay_widgets_gallery`](https://github.com/impizulu/rayclay/blob/examples/examples/ex10_rayclay_widgets_gallery/main.c) | - | Every widget in one screen | A reference: buttons, inputs, menus, a context menu, a modal dialog **and a non-modal inspector panel**, an image, scrolling, charts (including **16 series in one plot**, the cap), and **a 5,000-row virtualized table** (`rcVirtualList`) showing how a large dataset stays cheap; runs under the bundled default titlebar |
| [`ex11_rayclay_icon_converter`](https://github.com/impizulu/rayclay/blob/examples/examples/ex11_rayclay_icon_converter/main.c) | - | Convert SVGs → RayClay icon headers (intended for a desktop) | A real app built *with* RayClay: folder scan, live preview, batch export. **A tool demo, not a step in a normal workflow**: `rcSvg("art.svg", size, color)` draws the `.svg` directly, with no conversion step |
| [`ex12_rayclay_inspector`](https://github.com/impizulu/rayclay/blob/examples/examples/ex12_rayclay_inspector/main.c) | - | A live inspector / test harness | `rcBeginSplitPane`, `rcChart` + `rcSparkline` (live fps/frame-time), an `rcBeginTable` grid with a follow-the-tail log, `rcSetLogSink`, and buttons that provoke real library diagnostics. **Filter the log by level and source**, **export it, or both metric histories as CSV, to the clipboard** (`rcClipboardSet`, with a byte count and a preview), and open a **non-modal live panel** reading window size, zoom and mode, frame cost, arena occupancy and log depth straight from the public getters. The worked example of a tool where **every control changes what the app shows or produces**; widget-for-its-own-sake belongs in ex10. Its titlebar **folds away on the primary modifier + T**. It also carries a **SCHEDULER** panel reading `rcWindowSchedStats`: `spurious / waits` (the wake-loop metric), the per-reason breakdown via `rcFrameReasonName`, and a **render-mode toggle**, because the counters are all a structural zero while an app draws every frame: the contrast is the lesson. The panel detects the mode from a **delta**, not a total, since the counters only ever climb. Its PERFORMANCE panel carries the worked `rcClipSlotCounts` census - a `clips` row plus a caption that is drawn in **every** state, healthy or not, so the panel never reflows at the moment a fault appears. **Pop out** puts the whole diagnostics pane in a second native window opened with `.focusPolicy = RC_WINDOW_FOCUS_NONE` and `.taskbar = RC_WINDOW_TASKBAR_SKIP` - the worked case for a watch window that must not take the keyboard from the app you are inspecting, and the only place in the suite where either field is the right answer. The main window shows a window list in its place, because skipping the taskbar means the desktop will not offer that window back to you. |
| `ex20_system_monitor` | - | A live system monitor | The **branded-window** example: a custom animated titlebar (`rcWindowControlButton`, `RC_ID_WINDOW_DRAG` / `RC_ID_WINDOW_NODRAG`) that folds to a drag rail on the primary modifier + T. Also the worked **floating panels**: the chart, the core strip and the process table each detach into a native window of their own (`rcAppOpenWindow`), pin above other applications (`rcWindowSetTopmost`, read back with `rcWindowIsTopmost`) and dock again, with the lifecycle rules written out beside the code. It is the worked case for **the state a window is born with** too: each panel is an OWNED child of the main window (`RC_WindowOptions.owner`) so the desktop keeps it with the app it came from, and it opens at the geometry the user last left it at (`.placement = RC_WINDOW_PLACE_AT` with `.x`/`.y`, read back every frame with `rcWindowPosition`) rather than appearing elsewhere and jumping. Every one of those is a request, so every one is asked about first - `rcWindowOwnerSupported`, `rcWindowPositionSupported`, `rcWindowTopmostSupported` - and a control whose session cannot honour it is not drawn. The panel list in the main window is where the read-only half earns its place: it reports a minimised panel (`rcWindowIsMinimized`) and offers hide and show (`rcWindowMinimize`, `rcWindowRestore`) beside dock, and the floating window can snap itself beside its parent (`rcWindowSetPosition`, `rcWindowSetSize`), which it withholds while it is maximised (`rcWindowIsMaximized`). An unfocused, unpinned panel is woken every OTHER sample (`rcWindowIsFocused`) and a minimised one not at all, which is what a monitor left open all day should cost; on a phone or the web `rcChildWindowsSupported` is false, so the detach control is never drawn and the status bar says why from the first frame, same source. The worked **custom element macro** (`rcBeginComponent` / `rcParseComponentOptions` / `rcEndComponent`), a **sortable** virtualized table whose columns drop with the width behind a `+N columns` chip rather than scrolling sideways, and the polled-deadline **on-demand sampling** pattern. It is also the worked **app-owned palette**: one `rcSetStyle` in `main.c` installs Grafana's canvas, panel and threshold colours, and colour on a number means a threshold rather than decoration. Real readings for its own process and for the whole machine (`rcMachineMemoryBytes`, `rcMachineCpuPercent`, `rcUnixTimeSeconds`), each card naming its source; a simulated host behind one swappable file for the rest |
| `ex21_data_explorer` | - | An analytics workbench over a 12,000-row catalogue | The **one dataset, many views** shape every data app has. Filters and sorts permute an **index array** (never the records), and a table, three charts and a summary all read that one selection. Also: **caching derived data on a change rather than per frame** (the app prints its own rebuild count, so you can watch it not move), **sampling a scatter** instead of asking the renderer for 12,000 discs, and **selectable virtualized rows** built from `rcRow` - not because a table row cannot be named (`rcTableRowId` makes one a hit-test target, and when selection is all you want, prefer it) but because header and body share ONE width table here, and the narrow arm swaps it for a shorter set that folds three values into a second line of the name cell, so the row's *shape* changes with the window. Build a row id from the **data index, never the screen position**: under `rcVirtualList` the screen position is the scroll window, so an id derived from it renames every row on every scroll |
| `ex22_docs_reader` | - | An offline documentation reader | The **typography** example. Registers a real family ladder with `rcRegisterFont` (**one file per weight**, the mistake that costs an afternoon) and resolves it with `rcFont`. Derives its reading column from `rcMeasureText` × a target character count rather than a hardcoded width, and folds the table of contents away when the window cannot hold it. **Degrades to the bundled face and still exits 0 when its fonts are absent**, so the exit code cannot tell you they loaded; the on-screen status line can. It is also the worked case for the **responsive class prefixes**: the article column spells its vertical reading rhythm `pt-5 sm:pt-7 lg:pt-10 pb-12 sm:pb-16` and nothing else in the app uses a prefix, because nothing else in it is a judgement about the size of the screen rather than about the type or the content |
| `ex23_issue_inbox` | - | A triage inbox over 100,000 issues | The **rail / list / detail** shape every desktop app is built on, at a size where the naive version stops working. A **100,000-row** `rcVirtualList` declares ~30 elements, so drawing costs what the *window* costs, not what the dataset does; filtering permutes an **index array** and never touches a record; the scan runs on a **change, not a frame** (the rail prints its own rebuild counter, so you can watch it not move while you drag the window); and a row carries **no strings at all**: 20 bytes of indices into shared word tables, with the title composed where it is drawn. Also the worked **arrow-key list**: `rcGetScrollInfo` + `rcScrollBy` keep the keyboard selection on screen, and the selection survives a refilter **by identity** rather than by row. The detail pane carries the **activity thread**, which is the worked case for content that is DERIVED rather than stored: a reply is a pure function of the issue number and its index, so a hundred thousand threads cost no memory and are stable across frames, sorts and runs |
| `ex24_svg_live` | - | The three vector routes, side by side | The choice a developer actually has to make, made visible: the **same artwork** drawn three ways at once, **by path** (`rcSvg("…/settings.svg", size, color)`, the default: no app state, no load, no unload), **from a handle you own** (`rcLoadSvg` / `rcLoadSvgFromMemory` return an `RC_Svg *`; `rcSvgHandle` draws it and `rcUnloadSvg` frees it), and from a **generated header** (compiled in, nothing to ship, cannot fail at run time). All three land in the same draw path, so the three panels are indistinguishable, which is the point: the choice is about your *build*, not about how it looks. The **Unload** button is the lesson in one click: it frees the handle you own while the path panel keeps drawing, because the library owns that one. Also live re-tint (`colour is a per-call argument`), a size slider, and one rail entry whose markup lives **only in the C source**: nothing for `rcSvg` to point at, which is exactly when you reach for a handle |
| `ex30_keyboard_form` | - | A sign-up form under the soft keyboard, intended for a phone | The **keyboard story** end to end: eight `rcTextInput` / `rcTextArea` fields, a country `rcCombo`, a toggle and a Create button in a portrait-locked shell. The form column ends at the keyboard's top edge by construction (its footer is `max(safe.bottom, keyboard.bottom)` tall from `rcViewport()`), the focused field is scrolled into view two frames after any change to the form's height, its box goes to `rcSetImeCaretRect` every frame, and a Done bar carries Previous / Next / Done (`rcSetSoftKeyboardVisible`). Validation is live and never blocking. Measured on a Pixel 8a with Gboard in portrait: `rcViewport().keyboard.bottom` reads **365 dp** with the keyboard up and 0 when it is dismissed, and the form shrinks above it. That number is the device's, not the example's - Gboard's default height on a 914 dp screen - so it is not a constant you will find in the source |
| `ex32_tab_navigator` | - | A tab navigator intended for a phone ("RayClay Pocket") | The **portrait-first navigation model**: `RC_AppOptions.orientation = RC_ORIENTATION_PORTRAIT`, a bottom tab bar of finger-sized items (sized by `rcPointerIsCoarse`), each tab a **list -> detail push stack** with a back arrow and the Android back button (`rcKeyPressed(RC_KEY_ESCAPE)`), depth kept across tab switches, the active tab popping to its root. The safe area is spent by the band that sits on it, so the header and the bar reach the screen edge under a notch. A 300-post `rcVirtualList`, an `rcTextInput` filter, unread dots, a settings page. At 900 units or more the same source shows a rail beside list and detail. Also the worked **scroll-offset keeper**: a scroll container not declared for two frames loses its offset, so a tab shell restores it itself |

## The decade walk

The point of `ex01`–`ex05` is that a good-looking GUI is a moving target. As hardware and taste
changed, so did what "modern" meant, and the feature set of a typical app grew with it. The set
reads as one story:

1. **1980s: 1-bit.** Black on white, hard rectangular buttons, no rounding, no gradients, no
   shadows. A menu bar and a number pad. This is the whole visual vocabulary of the era, and it maps
   to a handful of `rcBox`es with 1px borders.
2. **1990s: beveled gray.** The battleship-silver 3D look: raised buttons, sunken wells, a solid
   blue caption. More widgets appear (tabs, radio groups, combos). RayClay has a single-colour
   border, so the two-tone bevel is built by nesting a light edge over a dark one: a good lesson in
   composing effects the primitives don't give you directly.
3. **2000s: glossy.** Skeuomorphism arrives: vertical gradients on every button, subtle rounding,
   soft drop shadows, saturated aqua blues. `RC_Gradient` and `RC_Shadow` (each needs an `.id`) do
   the heavy lifting; sliders and a progress bar drive a working media player.
4. **2010s: flat.** The reaction: bold flat colour, generous whitespace, cards, and Material's one
   concession to depth (a soft shadow and a circular floating action button). `RC_Float` pins the
   FAB to a corner; a card list adds and deletes tasks.
5. **2020s: modern.** Dark mode, muted palette with an accent, rounded-xl cards, soft large
   shadows, a sidebar-and-content shell with live tabs. This is the current house style, and the
   richest example.

## The icon converter (ex11)

> **You almost certainly do not need this.** Using your own artwork is `rcSvg("art.svg", 24.0f,
> s.text)` in your layout: one line, no handle, no conversion step in a normal workflow.
> ex11 is here as a **real app built with RayClay**, and for the advanced case where you want the
> parser out of your binary entirely (see `ex24_svg_live` for the comparison). Read it as a tool
> demo, not a rite of passage.

`ex11_rayclay_icon_converter` is a dev tool built *with* RayClay (dogfooding): it opens already
scanned on the examples' own icon folder, and you point it at any other with the in-app Input
directory field. Preview any icon live (rendered through the exact same path a generated icon uses,
so it is WYSIWYG) and export RayClay icon headers, one at a time or the whole folder at once; each
lands in the Output directory as `<name>.h`, named after the SVG it came from. It bundles `rc_svg2icon.h`, a self-contained C SVG parser (paths, arcs, béziers,
rects/circles/ellipses/polylines, stroke *and* multi-colour), so the whole conversion workflow is a
GUI. **It vendors that parser because it needs the EMITTER** (writing a
`.h`), which the public API does not expose. If you only want to *draw* an SVG, you want `rcSvg`
and `ex24_svg_live`, not this. It is **intended for a desktop** (it reads and writes the filesystem
and batch-converts) and, by design, the one example that steps outside the pure-`RC_` rule: a
deliberate, documented exception for a real-world dev tool.

```bash
cmake --build build-desktop --target rayclay_ex11_rayclay_icon_converter
./build-desktop/rayclay_ex11_rayclay_icon_converter   # then set the directories in the app
```

## The benchmark suite

`examples/bench/` holds six applications that are deliberately bigger than the teaching examples.
Each is a plausible real app rather than a widget demo, and most double as a **performance
benchmark**: the same source is both the public showcase and the frame the test suite measures.

| App | Target / page | What it exercises |
|-----|---------------|-------------------|
| `messenger` | `rayclay_bench_messenger` · `messenger.html` | A chat client: conversation list with live search, a collapsible sidebar, content-sized message bubbles, a profile drawer |
| `notes` | `rayclay_bench_notes` · `notes.html` | A notes app: a note list, an editable body, tag chips, and **document tabs you can drag out of the window**. The tear-out is the worked case for a second window the USER asks for: the layout records a request (`st->tearNote` plus the drop point) and the runner turns it into `rcAppOpenWindow` at `RC_WINDOW_PLACE_AT`, because the layout half of a bench app holds no `RC_App`. The window is created on RELEASE with a drag ghost meanwhile, which is what Obsidian, VS Code and Slack do; only a native tab strip glues a live window to the cursor. Closing the last tab is allowed, so the editor also shows a real **empty state** |
| `trader` | `rayclay_bench_trader` · `trader.html` | A trading terminal: a live chart, an order book, a portfolio tab, symbol search (the densest layout in the suite) |
| `gallery` | `rayclay_bench_gallery` · `photos.html` | A photo gallery: an aspect-fit thumbnail grid and a detail view, across 12 procedurally-generated bitmaps decoded once through the real `rcLoadImageFromMemory` path, so it exercises decode + GPU upload while staying zero-asset |
| `platformer` | `rayclay_bench_platformer` · `platformer.html` | A game loop: per-frame animation and input, rather than a document-shaped UI. Four parallax bands, a contact shadow and a finish flag, all seeded once and read-only per frame |
| `opsdash` | `rayclay_bench_opsdash` · `opsdash.html` | An operations dashboard: a 48-service inventory that never changes under a telemetry band that changes every frame (the mostly-static screen most real tools are). Its draw order is worst-health-first, computed once at seed from health the tick never writes, so failures lead without the island ceasing to be static |

Two consequences worth knowing before you edit one:

- **They are structured differently from the single-source examples.** Each is a small set of files
  * `main.c` (the entry point), `<app>_app.c` (the GUI), `<app>_app.h` (the contract),
  `<app>_backend.h` (the data), so the UI can be driven by a deterministic fake backend under test
  and by live data in the demo.
- **Their frames are frozen.** The benchmark compares against a recorded reference frame, so a
  change that moves the layout requires re-recording the baseline. Each app carries an
  `<APP>_BENCH_VERSION` that is bumped when that happens.

If you are looking for a starting point to copy, prefer `ex05` or `ex10`: same API, none of the
benchmark machinery.

## Build & run any example

Each example is a CMake target named `rayclay_<dir>` (e.g. `rayclay_ex03_2000s_gui`).

```bash
# Desktop: in any checkout that carries examples/
cmake -B build-desktop
cmake --build build-desktop --target rayclay_ex03_2000s_gui
./build-desktop/rayclay_ex03_2000s_gui
```

The `ex30`+ band is intended for a phone and installed by the iOS and Android producers, and a
desktop configure builds it like anything else, with no option to turn on. Each target opens a
window at the phone's size (393x852). To check the
layout without a device, run it under `RAYCLAY_SAFE_INSETS=59,0,34,0` (an iPhone's bands; see
[Getting started ▸ Headless / CI](getting-started.md#headless--ci)).

> **The public distribution splits the library from the demos, so check which one you cloned.**
> `main` carries the single header alone; the example sources live on the **`examples` branch**:
>
> ```bash
> git clone --branch examples <repo>
> ```
>
> A configure on a branch without them prints `RayClay: no examples on this branch - building the
> library only`, so the build tells you rather than leaving you to wonder why `cmake --build`
> succeeded and produced nothing to run.

**Web pages for the bundled examples build from the `examples` branch**, which
registers each one with its `rc_web_example()` helper and ships the page shell they link against:

```bash
cmake --preset web                                   # the emscripten toolchain
cmake --build build-web --target rayclay_ex03_2000s_gui
python3 -m http.server 8080 --directory build-web    # open the page below
```

> **This builds RayClay's *bundled demos* into pages.** Building *your own* app for the browser is a
> different and much shorter recipe: a dozen link flags, given in full in
> [getting-started.md](getting-started.md#4-build-for-the-web-the-same-source).

Web pages are named for what they show, not for their target, so **`index.html` is the demo hub**:
a landing page linking to every demo. The full mapping:

| Target | Page | | Target | Page |
|--------|------|-|--------|------|
| `ex00_hello` | `hello.html` | | `ex12_rayclay_inspector` | `inspector.html` |
| `ex01_1980s_gui` | `gui1980s.html` | | `bench_messenger` | `messenger.html` |
| `ex02_1990s_gui` | `gui1990s.html` | | `bench_notes` | `notes.html` |
| `ex03_2000s_gui` | `gui2000s.html` | | `bench_trader` | `trader.html` |
| `ex04_2010s_gui` | `gui2010s.html` | | `bench_gallery` | `photos.html` |
| `ex05_2020s_gui` | `dashboard.html` | | `bench_platformer` | `platformer.html` |
| `ex06_cpp_kanban` | `kanban.html` | | | |
| `ex10_rayclay_widgets_gallery` | `gallery.html` | | `ex20_system_monitor` | `sysmon.html` |
| `ex21_data_explorer` | `explorer.html` | | `bench_opsdash` | `opsdash.html` |
| `ex22_docs_reader` | `reader.html` | | `ex23_issue_inbox` | `inbox.html` |
| `ex24_svg_live` | `svg.html` | | | |
| *(the hub)* | `index.html` | | | |

`ex11` has no web page: a browser has no folder for it to convert. It builds on every native
target and is installed on neither phone.

## Headless / CI

Any example bounds itself to N frames and exits 0 when `RAYCLAY_MAX_FRAMES` is set, a one-line
smoke test with no code change:

```bash
RAYCLAY_MAX_FRAMES=3 ./build-desktop/rayclay_ex01_1980s_gui   # opens, draws 3 frames, exits 0
```

**A frame budget forces `RC_RENDER_CONTINUOUS`**, so this smoke-tests a different render mode than the
example ships with. That is correct (a budget only means something if the frames actually happen), but it
means on-demand behaviour (idle CPU, parking) cannot be observed this way. To bound a run and stay on the
default path, bound by time instead: `RAYCLAY_MAX_SECONDS=3 RAYCLAY_RENDER_MODE=ondemand ./…`.
Full explanation → [getting-started.md](getting-started.md) ▸ Headless / CI.

## House rules (if you copy an example as a starting point)

- **Pure `RC_` API.** Examples call only the public `RC_` API and name only `RC_` types
  (`RC_Color`, `RC_String`, `RC_Dimensions`, `RC_Vec2`, `RC_BoundingBox`); no layout-engine calls, no
  libc includes needed. That includes a custom element's draw callback: `RC_CustomDrawCallback` takes an
  `RC_BoundingBox`. This is gated rather than aspirational: a check in RayClay's own CI fails the
  build if an example calls a layout-engine function or reaches for a system include. (The type
  names are a convention the examples keep, not something that check reads; `ex11`, the desktop
  converter, is exempt by name because it genuinely needs libc and OS I/O.)
- **Zero-asset.** The font is baked from the bundled face via `RC_AppOptions.fontSizes[]`; icons are
  procedural, compiled in from the shared `examples/assets/icons/` headers, so for most of the suite
  there is nothing to ship alongside the binary. The three that deliberately read real files
  (`ex10` a PNG, `ex22` font faces, `ex24` SVG sources) each degrade to working without them.
- **One source, both targets.** No `#ifdef` in your code: `.nativeFrame` (borderless + the bundled
  titlebar, drawn by the runner) is simply ignored on the web, where the browser owns the frame.
  The decade GUIs set `.titlebar.custom = true` and hand-roll their period bars instead; `ex10`
  and `ex11` run under the default bar. On the web the period bars still render, deliberately, as
  inert period art (titlebars are desktop-only, so every `RC_ID_WINDOW_*` verb is a no-op there).
- **A clickable thing shows the hand.** You get this for nothing: polling `rcClicked` or
  `rcPressed` on an element IS what marks it clickable, and the frame's cursor becomes
  `RC_CURSOR_POINTER` while the pointer is over it. The only way to lose it is to build a button
  out of raw reads (`rcIsHovered` plus `rcPointerPressed`), which also fires on the press edge and
  so cannot be cancelled by sliding off. Use the verb. A surface that is DRAGGED rather than
  clicked polls neither, so it names its own cursor: `rcSetCursor(RC_CURSOR_GRAB)` while hovered
  and `RC_CURSOR_GRABBING` while held.
- **A chart tooltip that fits is never cropped by the view, in any mode; the mode decides where
  it sits on the plot.** Two different things, and the difference is worth knowing before you
  choose a mode. The library measures every tooltip panel before it emits it - the rows are
  formatted first, so the size is known in the same frame - and clamps it to the admissible view:
  the visible extent, minus the safe area and the soft keyboard, taken per edge. A panel that
  fits cannot be cut off by a window edge, a notch or an IME, whatever mode you choose and however
  the view is zoomed or panned; `CORNER` and `FIXED` keep their anchor as a preference and yield
  to the view the same way. A panel larger than the view pins to its leading edge, header and
  first rows readable, rather than escaping, and a fully occluded view withholds it. That is the
  library's job, not each app's: you write `.tooltip = RC_CHART_TOOLTIP_NEAREST` and nothing
  else, at any window size, on any platform. The clamp is what makes that true: without it, half
  of the placements a tooltip can take near an edge leave the view.
  What the mode decides is where the panel sits relative to the PLOT. Every chart in `examples/`
  that shows a readout *opens* in `RC_TOOLTIP_PLACE_CORNER`: eight set it outright, and `ex10`'s
  gallery chart starts there behind a combo, so you can feel the difference rather than read about
  it. **Verified by capture rather than by grep** - a hover held on the plot's right edge, its
  middle and its left at 1280x820, and one at 420x900 phone width, puts the panel in the opposite
  top corner every time. The default, `RC_TOOLTIP_PLACE_CURSOR`, follows the pointer and picks its
  direction by quadrant, which is what a web chart does and is right for most charts; the direction
  needs no measurement, and the clamp that follows uses one. Neither mode confines the panel to the
  plot: `CORNER` parks it in a plot corner, and a panel taller or wider than the plot spills over
  the card around it, still inside the view. Use CURSOR when your panel is small next to your plot
  and you want the web's feel; use CORNER when the readout must stay off the data.
- **A table fits the width it is given, and says what the width cost.** Never make the reader
  scroll a table sideways: drop the columns that matter least and keep the ones the table is read
  for. `ex20` does this at `SYS_TABLE_FULL_W` and puts a `+N columns` chip at the card's top right
  when it has, with the dropped sort keys still reachable as chips below. A table that quietly
  shows fewer columns than it has is one a reader can misread.
