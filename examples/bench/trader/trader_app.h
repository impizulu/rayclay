/*
    trader_app.h - the app's contract: types and prototypes only, so it includes
    cleanly from C and from C++.

    Two drivers share it: main.c, which opens a real window, and a headless
    harness that feeds the same core a fixed dt, a fixed seed and synthetic
    input. The core runs identically either way - only the SOURCE of the clock,
    the seed and the input differs.
*/
#ifndef TRADER_APP_H
#define TRADER_APP_H

#include "bench_app.h"        /* the shared AppCtx / AppInputSink / AppMode contract */
#include "trader_backend.h"   /* TrStore, embedded by value in AppState              */

/* Bump ONLY when the scripted path's rendered output changes. */
#define TRADER_BENCH_VERSION 5

/* The titlebar's height. main.c pins the window's drag strip to it and the layout
   measures the body against it, so the two must be one number. */
#define TR_TOPBAR_H 52

/* Font ladder, baked from the bundled face. F_HERO = 52 is the big price ticker:
   a large heading has to stay crisp at 2x HiDPI or zoom. */
typedef enum { F_SMALL = 0, F_BODY, F_MD, F_HEAD, F_TITLE, F_HERO, F_COUNT } TradeFont;

/* App state - a flat, memset-able blob, so trader_seed can zero it and rebuild.
   The selected instrument lives in the store, not here. */
typedef struct {
    TrStore store;           /* the backend, BY VALUE (the seed's memset zeroes it)   */
    int   navTab;            /* nav rail: 0 markets / 1 portfolio (switches the body)  */
    int   watchFilter;       /* watchlist pills: 0 All / 1 Gainers / 2 Losers         */
    int   tf;                /* chart timeframe tab 0..3 (reslices the candle window) */
    int   orderSide;         /* TR_BUY / TR_SELL (segmented toggle)                   */
    int   orderType;         /* 0 Market / 1 Limit (combo)                            */
    char  search[24];        /* watchlist search (case-insensitive symbol/name filter)*/
    char  qty[8];            /* order qty input -> pushed to the backend for est cost */
    char  limitPx[12];       /* limit-price input (parsed for est cost + Limit fill)  */
    bool  modalConfirm;      /* the order-confirm dialog                              */
    bool  modalSettings;     /* the settings dialog (mutually exclusive)              */
    bool  darkMode;          /* theme toggle                                          */
    bool  confirmDialogs;    /* settings toggle: gate the confirm dialog on Place order */
    /* Whether a narrow window is showing the detail PAGE or the list. It is not the
       selection - the store always holds one, so the wide arm's panes are never
       empty - and the wide arm ignores it. One bool per idea, so the app cannot
       open on the wrong screen. */
    bool  detailOpen;
    bool  seeded;            /* demo lazy-init guard                                  */
} AppState;

/* The app's palette, dark and light. trader_layout installs it every frame; the
   demo runner reads it for the window's clear colour, painted before any layout. */
RC_Style trader_style(bool dark);

void trader_seed  (AppState *st, unsigned seed);            /* memset then build      */
void trader_update(AppState *st, const AppCtx *ctx);        /* advance the market      */
void trader_layout(AppState *st, const AppCtx *ctx);        /* the UI, both modes      */
void trader_demo_chrome(AppState *st, const AppCtx *ctx);   /* demo-only overlay       */
void trader_bench_step(AppState *st, const AppInputSink *in, int frame);

#endif /* TRADER_APP_H */
