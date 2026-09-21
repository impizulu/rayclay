/*
    main.c - RayClay Gallery: the demo runner.

    The only translation unit that touches RC_App. It opens a window with a real
    clock and real input, seeds the app once the renderer is up (the textures
    upload there), and drives the same core (gallery_update / gallery_layout) a
    scripted run drives.

    One source builds the native window and the web canvas (cmake --preset web).
    Zero-asset: the twelve photos are generated in memory, no files to ship.
    Headless smoke: RAYCLAY_MAX_FRAMES=N draws N frames and exits 0.

    Build: cmake --build build --target rayclay_bench_gallery
*/
#include "gallery_app.h"

static void demo_update(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    if (!st->seeded)                        /* seed once the renderer/GL is up (textures upload here) */
        gallery_seed(st, 0x6A11E70u);       /* a fixed demo seed */
    AppCtx ctx = app_demo_ctx(app);
    gallery_update(st, &ctx);
}


static void demo_layout(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    AppCtx ctx = app_demo_ctx(app);
    gallery_layout(st, &ctx);
    gallery_demo_chrome(st, &ctx);

    /* The window's clear colour is a snapshot taken at create, not a live link
       to the style, so push it every frame or the old ground shows wherever
       the layout does not cover the window - the edge during a live resize.
       The call is change-gated inside the library, so this costs nothing. */
    rcWindowSetClearColor(rcAppMainWindow(app), rcGetStyle().background);
}

int main(void) {
    static AppState state;   /* zero-init; gallery_seed fills it + loads images on frame 1 */

    static const float font_sizes[F_COUNT] = {
        [F_SMALL] = 12.0f,
        [F_BODY]  = 14.0f,
        [F_MD]    = 18.0f,
        [F_TITLE] = 30.0f,
        [F_HERO]  = 56.0f,
    };

    /* The app's own neutral theme, installed before the window opens so the
       clear colour behind the first frame is the wall the photos hang on. */
    rcSetStyle(gallery_style(true));

    RC_AppOptions opts = {
        .width               = 1280,
        .height              = 720,
        .title               = "RayClay Gallery",
        .clearColor          = rcGetStyle().background,
        .fontSizes           = font_sizes,
        .fontCount           = F_COUNT,
        .scratchArenaBytes   = 4096,      /* backs rcFormat in the demo HUD only */
        .startLayoutElements = 4096,      /* the thumbnail grid + detail pane + modal */
        .nativeFrame         = true,
        .titlebarHeight      = 52,
        .updateCallback      = demo_update,
        .layoutCallback      = demo_layout,
        .userData            = &state,
        .titlebar            = { .custom = true },   /* the topbar IS the titlebar */
        /* The live fps readout in the demo HUD is an animation - the text moves
           every frame - so this runner cannot park. Do not copy that into a
           real app: stay on demand and it idles at ~0 CPU (ex04). */
        .renderMode          = RC_RENDER_CONTINUOUS,
    };

    return rcRunApp(&opts);
}
