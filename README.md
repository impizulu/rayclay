# RayClay

**Make the world a better place and build your JavaScript application in C.**

RayClay aims to be the most developer-friendly framework for creating native desktop and mobile applications. It handles all the DIY aspects of C for you while retaining all the performance benefits of using a low-level programming language.

**Why not use React Native or Electron?** Because your users can feel when an app is 10x more responsive, or when their device is almost out of memory and your app is the only process not lagging, or when the install size is 1 MB instead of 100 MB. The user experience is straight up better. The lure of popular JavaScript frameworks is the developer experience; it's so much easier to ship something fast. And it is undeniable that the unholy trinity (HTML, CSS, JS) produces some beautiful UIs that are tricky to replicate in lower-level languages. Enter RayClay.

> **RayClay builds for desktop, mobile, and the web.** It is rigorously tested on physical desktop machines and mobile devices (we test real pixels, not just remote logs): macOS, Windows 11, Linux (various distributions), iOS, Android, and WebAssembly.

RayClay was developed because it is simply too difficult to build full-stack apps in existing low-level frameworks, while JavaScript frameworks make it easy but are bloated beyond belief. It felt like there was no middle ground. Dear ImGui did not have the out-of-the-box ease of development that I was looking for (RayClay has flexbox-style layouts and networking) and Qt, the most capable lower-level framework, has a dual-licensing model that can become somewhat restrictive or even expensive, depending on your project's goals, distribution methods, and budget. And so, RayClay was born; ready to build complete apps out of the box, open source (no Qt strings attached), and free from the resource burden of JavaScript frameworks like React Native or Electron.

And now, because your app is in C, you unlock all sorts of potential lower-level capabilities that JavaScript simply does not have access to, and your app is straight up more performant and less resource intensive. The point is, RayClay is great for any kind of native app, whether performance is a necessity (real-time overlays, machine telemetry, low-latency trading, etc.) or simply a nice-to-have.

**C is measurably more performant than JavaScript by a wide margin, so if it's no extra effort to vendor a C framework over a JavaScript one... well, the choice should be pretty straightforward.**

Happy coding!

Check out RayClay on [GitHub](https://github.com/impizulu/rayclay) and [GitLab](https://gitlab.com/impizulu/rayclay).

Useful Resources:
- **[RayClay cheatsheet](https://www.rayclay.dev/docs/cheatsheet)** - every public function in the API (or find at `docs/api.md`).
- **RayClay template** ([GitHub](https://github.com/impizulu/rayclay-template), [GitLab](https://gitlab.com/impizulu/rayclay-template)) - a starter app with a custom titlebar, a collapsible sidebar and browser-style zoom. The app follows RayClay best practices and is structured to be scalable.

---

## API

### Naming Conventions

| Category | Convention | Example |
|---|---|---|
| Public function or function-like macro | `rc` + PascalCase | `rcAppCreate`, `rcBox` |
| Public type | `RC_` + PascalCase | `RC_AppOptions` |
| Public constant, enum value, or value-like macro | `RC_` + UPPER_SNAKE | `RC_GROW`, `RC_PX` |
| Public callback type | `RC_` + PascalCase + `Callback` | `RC_LogCallback` |

Anything you call, pass or set is spelled `rc` or `RC_`; the exceptions are `RAYCLAY_IMPLEMENTATION`, `RAYCLAY_NO_GUI` and the `RAYCLAY_` environment variables. Names beginning `rci_` or `RCI_` are internal; they can change in any release, so don't depend on them.

### Public Functions

All public functions and element macros are in the single `rayclay.h` you include. Full one-line reference: [`docs/api.md`](docs/api.md).

```c
rcRunApp(NULL)                              // zero-config: a window + the welcome canvas
rcRunApp(&opts)                             // your layout and full RC_AppOptions control
rcAppCreate / rcRunFrame / rcAppDestroy     // you own the frame loop
```

| Module | Key symbols |
|---|---|
| Layout | `rcBox` `rcRow` `rcColumn` `rcSpacer` `rcVirtualList` *(macros)* |
| Text | `rcTextL` `rcText` `rcTextC` *(macros)* · `rcLoadFont` `rcRegisterFont` `rcFont` |
| Widgets | `rcButton` `rcCheckbox` `rcToggle` `rcSlider` `rcProgress` `rcRadio` `rcCombo` |
| Text entry | `rcTextInput` `rcTextArea` *(macros)* |
| Text selection | `rcSelectable` *(macro)* · `rcHasSelection` `rcCopySelection` `rcSelectAll` · `RC_SelectMode` |
| Menus & modals | `rcBeginMenu` `rcMenuItem` `rcBeginContextMenu` `rcBeginModal` |
| Interaction reads | `rcIsHovered` `rcClicked` `rcIsFocused` `rcSetFocus` |
| Keyboard | `rcKeyDown` `rcKeyPressed` `rcKeyReleased` `rcModDown` |
| Scrolling | `rcScrollBy` `rcScrollToTop` `rcScrollToBottom` `rcScrollbar` `rcWheelDelta` |
| Zoom | `rcWindowZoom` `rcWindowSetZoom` `rcWindowZoomMode` `rcWindowSetZoomMode` · `rcWindowZoomLadder` |
| Titlebar *(desktop-only)* | `rcWindowControls` `rcWindowControlButton` |
| Theme | `rcStyleDark` `rcStyleLight` `rcGetStyle` `rcSetStyle` |
| Images & icons | `rcLoadImage` `rcLoadImageFromMemory` `rcUnloadImage` · `RC_IconCallback` |
| Clipboard | `rcClipboardRequest` `rcClipboardPoll` `rcClipboardSet` |
| Cursor | `rcSetCursor` |
| Color | `rcRgb` `rcHex` `rcWithAlpha` |
| Utilities | `rcFormat` `rcStrCopy` · `rcArena*` |

### Element Macros

Containers, text and the text-entry fields are **macros**, not functions: designated initializers with a brace body. Widgets, menus and modals are macros too (they take designated options); interaction reads are plain functions.

```c
rcColumn(.id = "sidebar", .bg = RC_SLATE_800, .gap = 8, .p = 12) {
    rcTextL("Inbox", .color = RC_WHITE, .size = 18);
    if (rcButton("send", "Send", .variant = RC_BTN_PRIMARY))
        send_message();
}
```

Sizing tokens: `RC_FIT`, `RC_GROW`, `RC_PX(n)`, `RC_PCT(n)`.

### C++

RayClay builds for C++ projects too: `rayclay.h` is wrapped in `extern "C"`, so a C++20 file can include it and use the same element macros, with each macro's designated fields written in declaration order (C++20 requires it). Link the `rayclay` CMake target, which compiles the implementation as C for you.

### Desktop-only surface

`rcWindowControls`, `rcWindowControlButton` and the `RC_ID_WINDOW_*` ids (`MINIMIZE` / `MAXIMIZE` / `CLOSE` / `DRAG`) compile on every target but emit nothing on the web and on phones, where the browser tab or the OS shell is the window chrome. Same source, no `#ifdef`.

---

## Developer Principles

To quote an extract from [learncpp.com](https://www.learncpp.com/cpp-tutorial/introduction-to-cpp-development/), typically, good solutions have the following characteristics:
- They are straightforward (not overly complicated or confusing).
- They are well documented (especially around any assumptions being made or limitations).
- They are built modularly, so parts can be reused or changed later without impacting other parts of the program.
- They can recover gracefully or give useful error messages when something unexpected happens.

To add a few of my own...

...non-negotiables:
- open source (free for the world to use and redistribute).
- os agnostic (can compile on any os).
- lightweight (or produces lightweight outputs).
- good 'big business' adopters (improves longevity).

...preferences:
- simple to use. not a headache to learn and setup.
- large community. lots of tutorials.
- capable of flexible, modern builds. does not feel/look outdated.

Why C? To quote the [sokol README](https://github.com/floooh/sokol#why-c):

- easier integration with other languages
- easier integration into other projects
- adds only minimal size overhead to executables
