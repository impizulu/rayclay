/*
    main.c - RayClay Photos: the runner

    The only translation unit that touches RC_App. It opens the window, seeds the library once
    the renderer is up, and drives the same core (gallery_update, gallery_layout) a scripted
    run drives. One source builds the desktops, the web canvas and the phones. Run from the
    repository root or the build folder, so the film finds examples/assets/media.

    The app is src/app/gallery_app.c. Its media teaching, a captioned video story through
    rcMediaOpen, is in src/app/ui_player.h.

    Target: rayclay_ex21_media_library
*/
#include "app/gallery_app.h"

/* The user's appearance, as a page reads prefers-color-scheme: the library opens in it and
   follows it, as Photos does. The app's own Appearance buttons choose until the system next
   changes. No stated preference keeps the white room. */
static void follow_scheme(AppState *st) {
    st->darkMode = rcPreferredColorScheme() == RC_COLOR_SCHEME_DARK;
}

static void on_app_event(RC_AppEvent event, void *userData) {
    if (event == RC_APP_EVENT_COLOR_SCHEME_CHANGED)
        follow_scheme((AppState *)userData);
}

static void demo_update(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    if (!st->seeded) {
        gallery_seed(st, 0x6A11E70u);       /* the same seed the benchmark runs */
        follow_scheme(st);                  /* the seed zeroes the state, appearance included */
    }
    st->density = rcWindowContentScale(rcAppCurrentWindow(app));

    /* ctx.dt is this frame's true delta, a park included: holding it to a frame's worth
       keeps one long gap from skipping a whole transition. */
    AppCtx ctx = app_demo_ctx(app);
    if (ctx.dt > 0.05f)
        ctx.dt = 0.05f;
    gallery_update(st, &ctx);
}

static void demo_layout(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    AppCtx ctx = app_demo_ctx(app);
    gallery_layout(st, &ctx);

    /* The clear color shows at the edge of a live resize, and the theme can change it. */
    rcWindowSetClearColor(rcAppMainWindow(app), rcGetStyle().background);
    if (gallery_wants_frame(st))
        rcWindowRequestFrame(rcAppMainWindow(app));
}

int main(void) {
    static AppState state;

    /* Sizes are written where they are used. This one entry sets font slot 0's own size,
       which is what a .tooltip and a text field draw at. */
    static const float base_size[1] = { 13.0f };

    /* The GLFW host and Linux answer before the window opens, so its first clear is already
       the user's scheme; where the answer comes later, the first layout sets it. */
    rcSetStyle(gallery_style(rcPreferredColorScheme() == RC_COLOR_SCHEME_DARK));
    rcSetAppEventHandler(on_app_event, &state);
    RC_AppOptions opts = {
        .width               = 1280,
        .height              = 720,
        .title               = "Photos",
        .clearColor          = rcGetStyle().background,
        .fontSizes           = base_size,
        .fontCount           = 1,
        .startLayoutElements = 4096,
        .frame               = RC_WINDOW_FRAME_NONE,
        .updateCallback      = demo_update,
        .layoutCallback      = demo_layout,
        .userData            = &state,
        .titlebar            = { .height = GALLERY_BAND_H }, /* the toolbar is the titlebar */
    };
    return rcRunApp(&opts);
}
