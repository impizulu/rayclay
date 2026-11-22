/*
    app_ctx.h - the seam between this app and the clock, window and input it runs on.

    The app is four functions over its own state: <app>_seed, <app>_update (advance
    by ctx->dt), <app>_layout (the whole UI) and <app>_bench_step (a scripted run).
    main.c fills an AppCtx from the live window every frame; a headless benchmark
    fills the same struct itself (an injected dt, a fixed seed and synthetic input
    through AppInputSink), so the shipped source is the measured source.

    ex21, ex22, ex23, ex24 and ex29 each carry this same file, so one benchmark
    driver runs any of them and each example still builds from its own directory.
*/
#ifndef APP_CTX_H
#define APP_CTX_H

#include "rayclay.h"

typedef enum { APP_DEMO = 0, APP_BENCH } AppMode;

/* Everything the UI needs from the frame it is drawing into. */
typedef struct {
    float     dt;       /* injected seconds/frame; 0 == the freeze */
    int       fbWidth;  /* logical viewport width  */
    int       fbHeight; /* logical viewport height */
    float     zoom;     /* display zoom factor (bench = 1.0) */
    AppMode   mode;     /* a real window, or a scripted run at a fixed size */
    /* The window's unsafe margins in logical px, from rcGetSafeAreaInsets(): a
       phone status bar, a camera cutout, a home indicator. Zero on desktop unless
       RAYCLAY_SAFE_INSETS stands a phone's bands in. */
    RC_Insets safe;
    /* The live viewport from rcViewport(): layout units, layout-unit safe insets,
       a breakpoint name and the pointer class, all in one space. Zero width means
       "not supplied", and the helpers below then fall back to fbWidth/fbHeight. */
    RC_Viewport view;
} AppCtx;

/* The responsive seam. An ordinary app calls rcViewport() where it lays out and
   reads .width / .safe / .breakpoint directly - ex20 and ex25-ex28 show that plain
   form. These three take the viewport as an argument, because these apps have a
   second caller with no window to measure. Never re-derive the layout width from
   the window size and the zoom: it inverts every breakpoint under optical zoom. */
static inline float app_view_w(const AppCtx *ctx) {
    return ctx->view.width > 0.0f ? ctx->view.width : (float)ctx->fbWidth;
}

static inline float app_view_h(const AppCtx *ctx) {
    return ctx->view.height > 0.0f ? ctx->view.height : (float)ctx->fbHeight;
}

/* Already in layout units when the library supplied them, and returned unchanged:
   there is no second conversion to do here, so do not divide them again. */
static inline RC_Insets app_safe(const AppCtx *ctx) {
    return ctx->view.width > 0.0f ? ctx->view.safe : ctx->safe;
}

/* Read the live clock, size and zoom off a running app - every runner needs
   exactly this. A headless caller fills the same struct itself. */
static inline AppCtx app_demo_ctx(RC_App *app) {
    RC_Dimensions d = rcWindowSize(rcAppCurrentWindow(app));
    AppCtx ctx = {
        .dt       = rcWindowFrameTime(rcAppMainWindow(app)),
        .fbWidth  = (int)d.width,
        .fbHeight = (int)d.height,
        .zoom     = rcWindowZoom(rcAppMainWindow(app)),
        .mode     = APP_DEMO,
        .safe     = rcGetSafeAreaInsets(),
        .view     = rcViewport(),
    };
    return ctx;
}

/* The input seam. A scripted scenario drives every interaction through this sink,
   so the real hit-test, focus and caret paths run rather than a direct state
   write. Members stage this frame's input only. */
typedef enum { APP_MBTN_LEFT = 0, APP_MBTN_RIGHT } AppMouseButton;

typedef struct AppInputSink {
    void *ctx;                                            /* the caller's own handle */
    void (*move)  (void *ctx, float x, float y);          /* pointer position (level) */
    void (*button)(void *ctx, AppMouseButton b, bool down); /* held; edges are the caller's */
    void (*wheel) (void *ctx, float dx, float dy);       /* scroll notches this frame */
    void (*text)  (void *ctx, unsigned int codepoint);  /* one typed char this frame */
    void (*key)   (void *ctx, int semanticKey, bool down); /* editing key (minimal) */
} AppInputSink;

#endif /* APP_CTX_H */
