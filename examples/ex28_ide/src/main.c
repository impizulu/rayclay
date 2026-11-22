/*
    main.c - RayClay IDE: a code editor in the shape of VS Code, in pure C

    It opens the repo's examples/ folder and this app's own source, and everything on screen
    works: the explorer tree (rcListDirectory), tabs, a real editor with syntax highlighting for
    C, Markdown, JSON and SVG, find and replace, undo, the Ctrl/Cmd+P palette, Problems, Output,
    and a Zen mode that folds the title bar.

    What a developer takes from it:

      - A large immediate-mode app stays simple: the state is one struct (state.h), and layout()
        in app.h is the frame in reading order.
      - Long documents are cheap: one rcVirtualList row per line, one text run per token.
      - Files are safe: rcLoadFile reads, and rcSaveFile, reached only from an explicit Save,
        writes atomically, once the app has checked that the file has not moved.
      - A code editor's keyboard is a real text field at the caret (editor.h says why).

    Vector art, three ways, all pixel-identical. Open any .svg and its live preview sits beside
    the code, with size and color controls:

      rcSvg(path, size, color)       point at the file in the layout, like <img src>. Nothing to
                                     load or free. The title bar's logo is drawn this way.
      rcSvgHandle(svg, size, color)  you called rcLoadSvg or rcLoadSvgFromMemory, so the lifetime
                                     is yours. The preview parses the unsaved buffer this way,
                                     once per edit, so a number you type moves the drawing.
      rcIcon<Name>(size, color)      converted to C at build time: nothing to ship and no parser
                                     to run. Every icon in the chrome is one.

    Shapes become icon ops, not pixels: paths, line, polyline, polygon, rect, circle and ellipse
    work. Gradients, <text>, filters and embedded images are skipped with a warning.

    Build target: rayclay_ex28_ide
*/
#include "app/app.h"

/* The window's X, Cmd+Q, a Dock Quit and a log-off ask first (RC_APP_EVENT_CLOSE_REQUESTED):
   with a file unsaved the app keeps its window and asks, as File > Quit does. On the web the
   browser asks the user in its own words, so the page is only kept there. */
static void on_app_event(RC_AppEvent event, void *user) {
    AppState *st = (AppState *)user;

    if (event != RC_APP_EVENT_CLOSE_REQUESTED || !st->app || !dirty_count(st))
        return;
    rcWindowCancelClose(rcAppMainWindow(st->app));
#ifndef __EMSCRIPTEN__
    post_command(st, CMD_QUIT);
#endif
}

int main(void) {
    static AppState state;
    static const float BODY_SIZE[] = { (float)SZ_UI };   /* slot 0: what bundled widgets draw */

    apply_theme(true);
    state.dark      = true;
    state.sideOpen  = true;
    state.minimapOn = true;
    state.previewOn = true;
    state.outlineOpen = true;
    state.panelOpen = true;
    state.rootOpen  = true;
    state.sideFrac  = 0.18f;
    state.panelFrac = 0.74f;
    state.barH      = (float)BAR_H;
    state.svgSize   = 128.0f;
    state.find.seenDoc = -1;
    state.revealNode = -1;
    state.active = -1;      /* no tab until update opens the files */

    RC_AppOptions opts = {
        .width = 1280, .height = 820, .title = "RayClay IDE",
        .clearColor = theme()->editor,
        .fontSizes = BODY_SIZE, .fontCount = 1,
        .scratchArenaBytes = 1 << 20,       /* backs every rcFormat in a frame */
        .startLayoutElements = 1 << 14,     /* a minimap of 270 lines: no warm-up growth */
        .frame = RC_WINDOW_FRAME_NONE,
        .updateCallback = update, .layoutCallback = layout, .userData = &state,
        .titlebar = { .height = BAR_H },
        .renderMode = RC_RENDER_ON_DEMAND,
    };
    rcSetAppEventHandler(on_app_event, &state);
    return rcRunApp(&opts);
}
