/*
    main.c - RayClay `platformer`: a side-scrolling auto-runner.

    Shows: a custom titlebar, a floating-positioned scene built entirely from
    rcBox rects, a continuous render mode for a game that steps its own physics,
    and one input union - SPACE, a click anywhere, or an on-screen JUMP button -
    so the same source plays on desktop, web and mobile with no platform #ifdef.

    Zero-asset: the bundled font plus a procedural scene, nothing to ship.
    Build and run the rayclay_bench_platformer target, or `cmake --preset web`
    for the browser. RAYCLAY_MAX_FRAMES=N draws N frames headless and exits.
*/
#include "platformer_app.h"

static void demo_update(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    if (!st->seeded)                        /* seed once the renderer is up */
        platformer_seed(st, 0x1A2B3C4Du);   /* a fixed demo seed */
    AppCtx ctx = app_demo_ctx(app);
    platformer_update(st, &ctx);
}

static void demo_layout(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    AppCtx ctx = app_demo_ctx(app);
    platformer_layout(st, &ctx);
    platformer_demo_chrome(st, &ctx);
}

int main(void) {
    static AppState state;   /* zero-init; platformer_seed fills it on frame 1 */

    static const float font_sizes[F_COUNT] = {
        [F_SMALL] = 12.0f,
        [F_BODY]  = 14.0f,
        [F_MD]    = 18.0f,
        [F_TITLE] = 28.0f,
        [F_HERO]  = 48.0f,
    };

    rcSetStyle(rcStyleDark());

    RC_AppOptions opts = {
        .width               = 1280,
        .height              = 720,
        .title               = "RayClay Runner",
        .clearColor          = rcGetStyle().background,
        .fontSizes           = font_sizes,
        .fontCount           = F_COUNT,
        .scratchArenaBytes   = 4096,      /* backs rcFormat in the demo readout only */
        .startLayoutElements = 2048,      /* the scene peaks near 150 floating roots  */
        .nativeFrame         = true,
        .titlebarHeight      = PL_TOPBAR_H,
        .updateCallback      = demo_update,
        .layoutCallback      = demo_layout,
        .userData            = &state,
        .titlebar            = { .custom = true },   /* the topbar IS the titlebar */
        /* A game steps its own physics, so it must draw every frame. Most apps
           should NOT copy this: RC_RENDER_ON_DEMAND is the default, and calling
           rcWindowRequestFrame() when your state changes is what keeps an idle
           window at about zero CPU. */
        .renderMode          = RC_RENDER_CONTINUOUS,
    };

    return rcRunApp(&opts);
}
