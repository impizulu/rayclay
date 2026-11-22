# Examples

Each example in `examples/` builds as CMake target `rayclay_<directory name>`
([desktop.md](desktop.md)); run it from the repository root. All but ex10, ex13 and ex16 build
for the web ([web.md](web.md)), where `index.html` links them. On iOS ([mobile.md](mobile.md)),
`-DRC_BUILD_EXAMPLES=ON` makes each example, unchanged, an app with the RayClay icon and, ex10
aside, the `examples/assets` files it opens. On Android, build one as mobile.md's app, its `main` in the `SDL_main.h` form,
with `examples/assets` and its `src` folder on the include path and its files at the same paths.

## Simple GUI

- [ex00_hello](https://github.com/impizulu/rayclay/blob/main/examples/ex00_hello/hello.c):
  `rcRunApp(NULL)`: the built-in welcome canvas.
- [ex01_1980s_gui](https://github.com/impizulu/rayclay/blob/main/examples/ex01_1980s_gui/main.c):
  A 1-bit 1984 Macintosh: menus, windows dragged by an outline, a Calculator and Note Pad.
- [ex02_1990s_gui](https://github.com/impizulu/rayclay/blob/main/examples/ex02_1990s_gui/main.c):
  A Windows 95 desktop: Minesweeper and Display Properties, whose Apply repaints it all.
- [ex03_2000s_gui](https://github.com/impizulu/rayclay/blob/main/examples/ex03_2000s_gui/main.c):
  An Aqua-era jukebox: gel gradients, Cover Flow drawn through `rcImageUpdate`, a song table
  dropping columns.
- [ex04_2010s_gui](https://github.com/impizulu/rayclay/blob/main/examples/ex04_2010s_gui/main.c):
  A Material Design tasks app; ripples and an Undo snackbar animate from `rcAppTime`.
- [ex05_2020s_gui](https://github.com/impizulu/rayclay/blob/main/examples/ex05_2020s_gui/main.c):
  A Linear-style project console: metric tabs drive a chart; Cmd/Ctrl+K opens a command palette.
- [ex06_widgets_gallery](https://github.com/impizulu/rayclay/blob/main/examples/ex06_widgets_gallery/main.c):
  Every widget, the system file dialogs included, live beside the code that makes it.

## Utilities and components

- [ex10_util_rc_icon_converter](https://github.com/impizulu/rayclay/blob/main/examples/ex10_util_rc_icon_converter/main.c):
  A Figma-style desktop tool turning SVG files into icon headers, each previewed before export.
- [ex11_util_rc_inspector](https://github.com/impizulu/rayclay/blob/main/examples/ex11_util_rc_inspector/main.c):
  A DevTools-style inspector of a live page: element tree, flame chart, a console fed by
  `rcSetLogSink`.
- [ex12_signin_form](https://github.com/impizulu/rayclay/blob/main/examples/ex12_signin_form/src/main.c):
  One call to copy: Google and Apple buttons, email, reset, a one-time code, a bearer-token session.
- [ex13_user_preferences](https://github.com/impizulu/rayclay/blob/main/examples/ex13_user_preferences/src/main.c):
  A phone-first settings dialog: rows applying live and saving to JSON, and `rcUpdateCheck` against
  its `serve.py`.
- [ex14_file_selector](https://github.com/impizulu/rayclay/blob/main/examples/ex14_file_selector/src/main.c):
  An in-app open panel to copy, over `rcListDirectory`; Browse opens the system's dialog.
- [ex15_file_explorer](https://github.com/impizulu/rayclay/blob/main/examples/ex15_file_explorer/src/main.c):
  Finder's icon, list and column views, `rcFileDialogOpen`, dropped folders, a right-click menu.
- [ex16_tab_navigator](https://github.com/impizulu/rayclay/blob/main/examples/ex16_tab_navigator/src/main.c):
  A phone-first reader on `navigator.h`, to copy: badges, a stack per tab, swipe back.

## Full apps

- [ex20_system_monitor](https://github.com/impizulu/rayclay/blob/main/examples/ex20_system_monitor/src/main.c):
  A Grafana-style dashboard of simulated servers under this device's real readings: an
  app-defined element macro, `rcIconEmit` gauges, a heatmap painted with `rcImageUpdate`,
  crosshairs shared across charts.
- [ex21_media_library](https://github.com/impizulu/rayclay/blob/main/examples/ex21_media_library/src/app/gallery_app.c):
  A photo library: a viewer, an editor, thumbnails under `rcAppCachePath`, a film through
  `rcMediaOpen`.
- [ex22_markdown_notes](https://github.com/impizulu/rayclay/blob/main/examples/ex22_markdown_notes/src/app/notes_app.c):
  A Markdown notes app with wiki links and a reading mode, laid out by walking the text with
  `rcTextWidth`.
- [ex23_stocks_trader](https://github.com/impizulu/rayclay/blob/main/examples/ex23_stocks_trader/src/app/trader_app.c):
  A TradingView-style terminal on a simulated market: a candlestick chart in one custom-draw
  element, ECB rates through `rcFetch` and `rcJsonParse`.
- [ex24_messenger](https://github.com/impizulu/rayclay/blob/main/examples/ex24_messenger/src/app/messenger_app.c):
  Simulated chats, an optional `rcWebSocket` room on its `serve.py`, an export thread calling
  `rcAppWake`.
- [ex25_cpp_kanban](https://github.com/impizulu/rayclay/blob/main/examples/ex25_cpp_kanban/main.cpp):
  A C++20 Trello-style drag-and-drop board, optionally synced to its `serve.py` by
  `rcFetch`; its opening comment lists what differs from C.
- [ex26_data_explorer](https://github.com/impizulu/rayclay/blob/main/examples/ex26_data_explorer/src/main.c):
  12,000 orders behind a query bar, facets, a brushable trend and an `rcBeginTable` grid dropping
  columns to fit.
- [ex27_issue_inbox](https://github.com/impizulu/rayclay/blob/main/examples/ex27_issue_inbox/src/main.c):
  GitHub Issues over 100,000 issues in one file: `rcVirtualList` and an index-array filter keep
  frame cost flat.
- [ex28_ide](https://github.com/impizulu/rayclay/blob/main/examples/ex28_ide/src/main.c):
  A VS Code-shaped editor: tree, tabs, highlighting, find, palette, a live SVG preview through
  `rcSvg` and `rcSvgHandle`.
- [ex29_dash_game](https://github.com/impizulu/rayclay/blob/main/examples/ex29_dash_game/src/app/platformer_app.c):
  A rhythm runner whose titlebar plays the course's waveform: `rcIconEmit` sprites,
  `rcWindowSetRenderMode` only while something moves.
