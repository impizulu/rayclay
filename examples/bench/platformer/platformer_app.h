/*
    platformer_app.h - the app's contract: types and prototypes only, so it
    includes cleanly from C and from C++.

    Two drivers share it: main.c, which opens a real window, and a headless
    harness that feeds the same core a fixed dt, a fixed seed and synthetic
    input. The core runs identically either way - only the SOURCE of the clock,
    the seed and the input differs.
*/
#ifndef PLATFORMER_APP_H
#define PLATFORMER_APP_H

#include "bench_app.h"            /* the shared AppCtx / AppInputSink / AppMode contract */
#include "platformer_backend.h"   /* PlWorld, embedded by value in AppState              */

/* Bump ONLY when the scripted path's rendered output changes. */
#define PLATFORMER_BENCH_VERSION 4

/* The titlebar's height. main.c pins the window's drag strip to it and the
   layout measures the play area against it, so the two must be one number. */
#define PL_TOPBAR_H 52

/* Font ladder, baked from the bundled face. F_HERO = 48 is the big score
   readout: a large heading has to stay crisp at 2x HiDPI or zoom. */
typedef enum { F_SMALL = 0, F_BODY, F_MD, F_TITLE, F_HERO, F_COUNT } PlatFont;

/* App state - a flat, memset-able blob, so platformer_seed can zero it and
   rebuild. All simulation lives in `world`. */
typedef struct {
    PlWorld world;        /* the deterministic auto-runner sim, BY VALUE            */
    bool    jumpQueued;   /* OR-accumulated by layout from three inputs; consumed on a tick */
    bool    seeded;       /* demo lazy-init guard                                   */
} AppState;

void platformer_seed  (AppState *st, unsigned seed);            /* memset then build */
void platformer_update(AppState *st, const AppCtx *ctx);        /* dt <= 0 is a no-op */
void platformer_layout(AppState *st, const AppCtx *ctx);        /* the UI, both modes */
void platformer_demo_chrome(AppState *st, const AppCtx *ctx);   /* demo-only overlay  */
void platformer_bench_step(AppState *st, const AppInputSink *in, int frame);

#endif /* PLATFORMER_APP_H */
