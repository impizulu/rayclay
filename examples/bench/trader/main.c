/*
    main.c - RayClay `trader`: a markets terminal.

    Shows: a custom titlebar, a three-pane desktop layout that folds to one pane
    on a narrow window, a candlestick chart drawn from plain rcBox rects, a depth
    ladder, a text input, a combo, a modal confirm dialog and a live theme switch.

    Zero-asset: the bundled font plus procedural chrome, nothing to ship. Build
    and run the rayclay_bench_trader target, or `cmake --preset web` for the
    browser. RAYCLAY_MAX_FRAMES=N draws N frames headless and exits.
*/
#include "trader_app.h"

static void demo_update(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    if (!st->seeded)                        /* seed once the renderer is up */
        trader_seed(st, 0x517A7Eu);         /* a fixed demo seed */
    AppCtx ctx = app_demo_ctx(app);
    trader_update(st, &ctx);
}

static void demo_layout(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    AppCtx ctx = app_demo_ctx(app);
    trader_layout(st, &ctx);
    trader_demo_chrome(st, &ctx);

    /* THE SECOND LINE A RUNTIME THEME SWITCH NEEDS. The window's clear colour is a
       snapshot taken at create, not a live link to the style, so without this the
       old ground shows wherever the layout does not cover the window. Call it
       unconditionally: it is change-gated inside the library, and guarding it on
       your own "did the theme change" flag is wrong here, because the toggle is a
       widget INSIDE the layout and flips after this frame's style is installed. */
    rcWindowSetClearColor(rcAppMainWindow(app), rcGetStyle().background);
}

int main(void) {
    static AppState state;   /* zero-init; trader_seed fills it on frame 1 */

    static const float font_sizes[F_COUNT] = {
        [F_SMALL] = 12.0f,
        [F_BODY]  = 14.0f,
        [F_MD]    = 16.0f,
        [F_HEAD]  = 20.0f,
        [F_TITLE] = 26.0f,
        [F_HERO]  = 52.0f,
    };

    /* the same palette trader_layout installs, so the window's clear colour is
       already the ground the first painted frame will use */
    rcSetStyle(trader_style(true));

    RC_AppOptions opts = {
        .width               = 1280,
        .height              = 720,
        .title               = "RayClay Markets",
        .clearColor          = rcGetStyle().background,
        .fontSizes           = font_sizes,
        .fontCount           = F_COUNT,
        .scratchArenaBytes   = 4096,      /* backs rcFormat in the demo readout only */
        .startLayoutElements = 8192,      /* watchlist + book + candles, all at once  */
        .nativeFrame         = true,
        .titlebarHeight      = TR_TOPBAR_H,
        .updateCallback      = demo_update,
        .layoutCallback      = demo_layout,
        .userData            = &state,
        .titlebar            = { .custom = true },   /* the topbar IS the titlebar */
        /* A live feed advances on its own clock, so this demo must draw every
           frame. Most apps should NOT copy this: RC_RENDER_ON_DEMAND is the
           default, and calling rcWindowRequestFrame() when your state changes is
           what keeps an idle window at about zero CPU. */
        .renderMode          = RC_RENDER_CONTINUOUS,
    };

    return rcRunApp(&opts);
}
