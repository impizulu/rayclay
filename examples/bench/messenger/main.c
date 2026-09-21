/*
    main.c - RayClay Messenger: the demo runner.

    The only translation unit that touches RC_App. It opens a window with a real
    clock and real input, seeds the app once the renderer is up, and drives the
    same core (messenger_update / messenger_layout) a scripted run drives.

    One source builds the native window and the web canvas (cmake --preset web).
    Zero-asset: the bundled font and procedural avatars, no files to ship.
    Headless smoke: RAYCLAY_MAX_FRAMES=N draws N frames and exits 0.

    Build: cmake --build build --target rayclay_bench_messenger
*/
#include "messenger_app.h"

static void demo_update(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    if (!st->seeded)                        /* seed once the renderer/GL is up */
        messenger_seed(st, 0xC0FFEEu);
    AppCtx ctx = app_demo_ctx(app);
    messenger_update(st, &ctx);
}


static void demo_layout(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    AppCtx ctx = app_demo_ctx(app);
    messenger_layout(st, &ctx);
    messenger_demo_chrome(st, &ctx);

    /* The window's clear colour is a snapshot taken at create, not a live link
       to the style, so push it every frame or the old ground shows wherever
       the layout does not cover the window - the edge during a live resize.
       The call is change-gated inside the library, so this costs nothing. */
    rcWindowSetClearColor(rcAppMainWindow(app), rcGetStyle().background);
}

int main(void) {
    static AppState state;   /* zero-init; messenger_seed fills it on frame 1 */

    static const float font_sizes[F_COUNT] = {
        [F_SMALL] = 13.0f,
        [F_BODY]  = 15.0f,
        [F_HEAD]  = 17.0f,
        [F_TITLE] = 21.0f,
    };

    rcSetStyle(messenger_theme(true));   /* the app seeds itself dark */

    RC_AppOptions opts = {
        .width               = 1280,
        .height              = 720,
        .title               = "RayClay Messenger",
        .clearColor          = rcGetStyle().background,
        .fontSizes           = font_sizes,
        .fontCount           = F_COUNT,
        .scratchArenaBytes   = 4096,      /* backs rcFormat in the demo HUD only */
        .startLayoutElements = 8192,      /* a dense multi-pane app: many elements/frame */
        .nativeFrame         = true,
        .titlebarHeight      = 52,
        .updateCallback      = demo_update,
        .layoutCallback      = demo_layout,
        .userData            = &state,
        .titlebar            = { .custom = true },   /* the topbar IS the titlebar */
        /* Incoming messages arrive on a simulated clock and the HUD reports fps,
           so this runner draws every frame. A normal app should not copy that:
           stay on demand (the default) and call rcWindowRequestFrame() when your
           state changes, which holds an idle window at ~0 CPU. */
        .renderMode          = RC_RENDER_CONTINUOUS,
    };

    return rcRunApp(&opts);
}
