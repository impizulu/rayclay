/*
    RayClay Fleet Operations - a service-operations dashboard.

    Demonstrates: a dark panel theme, a custom titlebar carrying the scope
    filters, a card grid that reflows with the window, a scroll container with
    rcScrollbar, and a live telemetry rail that animates every frame.

    Build: cmake --build build --target rayclay_bench_opsdash
    Web:   cmake --preset web
    RAYCLAY_MAX_FRAMES=N draws N frames and exits - a headless smoke test.
*/
#include "opsdash_app.h"

static void demo_update(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    if (!st->seeded)
        opsdash_seed(st, 0x0D5DA5EEu);   /* a fixed demo seed */
    AppCtx ctx = app_demo_ctx(app);
    opsdash_update(st, &ctx);
}

static void demo_layout(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    AppCtx ctx = app_demo_ctx(app);
    opsdash_layout(st, &ctx);
    opsdash_demo_chrome(st, &ctx);
}

int main(void) {
    static AppState state;   /* zero-init; opsdash_seed fills it on frame 1 */

    /* Indexed by OpsdashFont: F_SMALL, F_BODY, F_MONO, F_MD, F_TITLE. Positional
       rather than designated, because a designated ARRAY initialiser is C99-only
       and a hard error in C++. */
    static const float font_sizes[F_COUNT] = { 12.0f, 14.0f, 14.0f, 20.0f, 26.0f };

    rcSetStyle(ops_style());   /* the clear colour below reads it; the core installs it again each frame */

    RC_AppOptions opts = {
        .width               = 1280,
        .height              = 720,
        .title               = "RayClay Fleet Operations",
        .clearColor          = rcGetStyle().background,
        .fontSizes           = font_sizes,
        .fontCount           = F_COUNT,
        .scratchArenaBytes   = 4096,      /* backs rcFormat in the demo HUD only */
        .startLayoutElements = 4096,      /* 48 cards x ~10 elements + nav + rail */
        .nativeFrame         = true,
        .titlebarHeight      = 52,
        .updateCallback      = demo_update,
        .layoutCallback      = demo_layout,
        .userData            = &state,
        .titlebar            = { .custom = true },   /* the topbar IS the titlebar */
        /* The telemetry rail advances a pulse and rolls a counter every frame, so
           the picture never settles and there is nothing for the on-demand runner to
           park on. A live readout is an animation and you pay for it, so put one on
           screen only when you mean to; an ops dashboard means to. */
        .renderMode          = RC_RENDER_CONTINUOUS,
    };

    return rcRunApp(&opts);
}
