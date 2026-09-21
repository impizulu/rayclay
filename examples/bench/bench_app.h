/*
    bench_app.h - the contract the six showcase apps share.

    Each app declares its own AppState plus four functions, so one driver runs any
    of them: <app>_seed, <app>_update (advance by ctx->dt), <app>_layout (the whole
    UI) and <app>_bench_step (a scripted run), plus a demo-only <app>_demo_chrome.

    AppCtx and AppInputSink are identical across every app, so the same source runs
    as the shipped app (real clock and input) and as a scripted scenario (an
    injected dt, a fixed seed and synthetic input) with no window at all.
*/
#ifndef BENCH_APP_H
#define BENCH_APP_H

#include "rayclay.h"   /* RC_Arena, referenced by AppCtx */

typedef enum { APP_DEMO = 0, APP_BENCH } AppMode;

/* Everything the UI needs from the frame it is drawing into. */
typedef struct {
    RC_Arena *arena;    /* scratch for rcFormat (demo chrome only); bench = NULL */
    float     dt;       /* injected seconds/frame; 0 == the freeze */
    int       fbWidth;  /* logical viewport width  */
    int       fbHeight; /* logical viewport height */
    float     zoom;     /* display zoom factor (bench = 1.0) */
    AppMode   mode;     /* gates the demo-only chrome overlay */
    /* The window's unsafe margins in logical px, from rcGetSafeAreaInsets(): a
       phone status bar, a camera cutout, a home indicator. Zero on desktop. */
    RC_Insets safe;
    /* The live viewport from rcViewport(): layout units, layout-unit safe insets,
       a breakpoint name and the pointer class, all in one space. Zero width means
       "not supplied", and the helpers below then fall back to fbWidth/fbHeight. */
    RC_Viewport view;
    /* Frames per second, from rcAppFPS(); zero where there is no running app.
       New members go at the END, so a caller that fills this struct positionally
       needs no edit - C zero-fills what it left out. */
    float fps;
} AppCtx;

/* The responsive seam. An ordinary app calls rcViewport() where it lays out and
   reads .width / .safe / .breakpoint directly - ex20 and after show that plain
   form. These four take the viewport as an argument, because these apps have a
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

/* One pane or several. RC_BP_MD is 768, the same one/many-pane line Tailwind's
   `md:` draws; an app with unusual fixed columns tests app_view_w() against its
   own number. Never branch a layout on the operating system - the condition is
   the SPACE, which is why a window dragged narrow gets the compact arm. */
static inline bool app_compact(const AppCtx *ctx) {
    return ctx->view.width > 0.0f ? ctx->view.breakpoint < RC_BP_MD
                                  : app_view_w(ctx) < 768.0f;
}

/* Read the live clock, size and zoom off a running app - every runner needs
   exactly this. A headless caller fills the same struct itself. */
static inline AppCtx app_demo_ctx(RC_App *app) {
    RC_Dimensions d = rcGetWindowDimensions();
    AppCtx ctx = {
        .arena    = rcAppArena(app),
        .dt       = rcWindowFrameTime(rcAppMainWindow(app)),
        .fbWidth  = (int)d.width,
        .fbHeight = (int)d.height,
        .zoom     = rcWindowZoom(rcAppMainWindow(app)),
        .mode     = APP_DEMO,
        .safe     = rcGetSafeAreaInsets(),
        .view     = rcViewport(),
        .fps      = rcAppFPS(app),
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

#endif /* BENCH_APP_H */
