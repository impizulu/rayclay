/*
    trader_app.h - the app's contract: types and prototypes only, so it includes
    cleanly from C and from C++.

    Two drivers can share it: main.c, which opens a real window, and a headless
    one that feeds the same core a fixed dt, a fixed seed and synthetic
    input. The core runs identically either way - only the SOURCE of the clock,
    the seed and the input differs.
*/
#ifndef TRADER_APP_H
#define TRADER_APP_H

#include "app_ctx.h"          /* AppCtx / AppInputSink / AppMode: the seam */
#include "trader_backend.h"   /* TrStore, embedded by value in AppState    */

/* Bump ONLY when the scripted path's rendered output changes. */
#define TRADER_BENCH_VERSION 22

/* The titlebar's height. main.c pins the window's drag strip to it and the layout
   measures the body against it, so the two must be one number. */
#define TR_TOPBAR_H 44

/* The body text size, which main.c loads as font slot 0: controls, menus and tooltips
   draw in that slot. Every other run states its own size where it is drawn. */
#define TR_BODY_SIZE 14

#define TR_MAX_DRAWINGS 32
#define TR_MAX_YTICKS   14
#define TR_MAX_XTICKS   24

typedef enum { TR_CHART_CANDLES = 0, TR_CHART_LINE, TR_CHART_AREA, TR_CHART_COUNT } TrChartType;

/* The left rail's drawing tools. Crosshair is the resting tool. */
typedef enum { TR_TOOL_CROSSHAIR = 0, TR_TOOL_TREND, TR_TOOL_HLINE, TR_TOOL_MEASURE } TrTool;

/* What the chart shows, one bit each. */
enum {
    TR_SHOW_VOLUME = 1 << 0,
    TR_SHOW_SMA    = 1 << 1,
    TR_SHOW_EMA    = 1 << 2,
    TR_SHOW_BB     = 1 << 3,
    TR_SHOW_RSI    = 1 << 4,
};

/* The trading panel's tabs. DEPTH exists only where the book has no column of its own. */
typedef enum {
    TR_TAB_POSITIONS = 0, TR_TAB_ORDERS, TR_TAB_HISTORY, TR_TAB_ACCOUNT, TR_TAB_DEPTH
} TrTab;

/* A phone shows one page at a time. */
typedef enum { TR_PAGE_WATCHLIST = 0, TR_PAGE_CHART, TR_PAGE_PORTFOLIO } TrPage;

/* A line a user drew: a price and a moment at each end, so it stays on its candles
   through any zoom or interval. */
typedef struct {
    int32_t tool;
    int32_t inst;
    int64_t t0, t1;
    int32_t p0, p1;
} TrDrawing;

/* The price plot's geometry, worked out in layout and read back by its draw callback.
   Everything is in layout pixels from the plot's top-left corner. */
typedef struct {
    float   w, h;               /* the plot                                            */
    float   mainH;              /* the price pane                                      */
    float   priceTop, priceBot; /* the band prices map into                            */
    float   volH;               /* the volume bars' tallest                            */
    float   rsiY, rsiH;         /* the RSI pane; rsiH 0 when it is off                 */
    float   barSpan;            /* pixels per bar this frame                           */
    float   first;              /* the bar at the plot's left edge, fractional         */
    float   bars;               /* bars across the plot, fractional                    */
    int     from, to;           /* the bars on screen, inclusive; to < from when none  */
    int32_t lo, hi;             /* the prices at priceBot and priceTop                 */
    int64_t volMax;

    int     yTicks;
    float   yTickY[TR_MAX_YTICKS];
    char    yTickStr[TR_MAX_YTICKS][12];
    int     xTicks;
    float   xTickX[TR_MAX_XTICKS];
    bool    xTickDay[TR_MAX_XTICKS];
    char    xTickStr[TR_MAX_XTICKS][12];

    bool    cross;              /* the pointer is over the plot                        */
    float   crossX, crossY;     /* snapped to the bar, and to a price under the magnet */
    int     crossBar;           /* the bar under the crosshair                         */
    int32_t crossPrice;
    bool    crossInRsi;
} TrPlot;

/* The account's value in another currency, at the European Central Bank's daily reference
   rates. The core asks (.want) once the user picks a currency; main.c fetches the rates and
   writes the rest. Nothing reaches the network before that pick, and a bench run never asks. */
#define TR_FX_COUNT 6
static const char *const TR_FX_CODES[TR_FX_COUNT] = { "USD", "EUR", "GBP", "CHF", "CAD", "AUD" };
typedef enum { TR_FX_IDLE = 0, TR_FX_LOADING, TR_FX_READY, TR_FX_FAILED } TrFxState;
typedef struct {
    int    pick;                /* the currency shown, an index in TR_FX_CODES; 0 = USD  */
    bool   want;                /* the core asks for rates; main.c clears it             */
    int    state;               /* TrFxState                                             */
    double rate[TR_FX_COUNT];   /* units of each currency to one US dollar               */
    char   date[12];            /* the ECB's observation date, YYYY-MM-DD                */
    char   amount[24];          /* the account in that currency, as drawn: text a layout
                                   draws must outlive the frame, so never a local        */
} TrFx;

/* App state - a flat, memset-able blob, so trader_seed can zero it and rebuild.
   The selected instrument lives in the store, not here. */
typedef struct {
    TrStore store;
    RC_Arena frame;             /* this frame's labels and row ids, reset as its layout starts */
    unsigned char frameBytes[16 * 1024];

    /* The chart's view. */
    int     chartType;
    unsigned show;              /* TR_SHOW_*                                           */
    float   barPx;              /* pixels per bar                                      */
    float   viewEnd;            /* bars scrolled back from the newest                  */
    int     range;              /* the range button in force, or -1 after a manual pan */
    bool    percentScale;       /* the price axis reads percent from the first bar     */
    TrPlot  plot;
    /* A symbol or a range a click picked, applied as the next frame starts, or -1. The plot
       is drawn after the layout, from the store, so a pick made once the chart is laid out
       would draw the new bars against the old scale for a frame. */
    int     pickSymbol;
    int     pickRange;

    /* Drawing. */
    int       tool;
    bool      magnet;
    bool      hideDrawings;
    TrDrawing drawings[TR_MAX_DRAWINGS];
    int       drawCount;        /* the drawings in force                               */
    int       drawTop;          /* drawCount..drawTop are the ones redo can restore    */
    int       clearedCount;     /* the drawings a Remove-all took, back in one undo    */
    bool      placing;          /* the first point of a line is down                   */
    int64_t   placeT;
    int32_t   placeP;
    bool      measuring;        /* a finished measurement stays until the next click   */
    int64_t   measureT1;
    int32_t   measureP1;

    /* The drag that pans the plot. */
    bool    pressOnPlot;
    bool    pressMoved;
    float   pressX, pressEnd;
    int32_t menuPrice;          /* the price the plot's context menu acts at           */
    bool    menuShown;          /* open last frame: menuPrice is taken as it opens     */

    /* The panels. */
    int     tab;                /* TrTab                                               */
    int     page;               /* TrPage                                              */
    int     equitySpan;         /* the phone's curve: its span, an index in TR_SPANS   */
    bool    pageAsked;          /* the user asked for the list, so no window overrides */
    int     watchFilter;        /* 0 All, 1 Gainers, 2 Losers, 3 Holdings              */
    int     sortColumn;         /* -1 as listed, else a watchlist column               */
    bool    sortDown;

    /* The ticket. */
    int     orderSide;          /* TrSide                                              */
    int     orderType;          /* TrOrderType                                         */
    char    qty[8];
    char    priceText[12];
    int     ticketQty;          /* parsed each update                                  */
    int32_t ticketPrice;
    int64_t ticketEstimate;
    const char *ticketProblem;  /* why it cannot go, NULL when it can                  */
    char    estimateStr[20];
    char    avgStr[12];

    /* The dialogs. */
    bool    modalConfirm;
    bool    modalSettings;
    bool    modalSearch;
    bool    modalTicket;        /* the phone's order sheet                             */
    bool    modalReset;         /* the question before the account is reset            */
    bool    confirmShown;       /* the review was up last frame                        */
    int     closeInst;          /* the position a pending Close sells, or -1           */
    bool    searchFocus;        /* the search field takes focus as it opens            */
    char    search[24];
    int     searchRow;          /* the row Enter would open                            */

    bool    focusMode;          /* the chart takes the window                          */
    bool    darkMode;
    bool    confirmOrders;
    int     speedIndex;

    /* The toast announcing what the account just did. */
    uint32_t toastSeq;
    float    toastLeft;
    bool     dialogUp;          /* a dialog was open, so one opening is seen as it opens */
    TrFx     fx;
    /* The device's own date and time, which main.c reads every frame for the range bar's
       clock. A zero year, as in a run with no real clock (the bench's), shows the market's. */
    RC_LocalTime localNow;

    bool    seeded;
} AppState;

/* The app's style, dark and light. trader_layout installs it every frame; the
   demo runner reads it for the window's clear color, painted before any layout. */
RC_Style trader_style(bool dark);

void trader_seed(AppState *st, unsigned seed);               /* memset then build  */
void trader_update(AppState *st, const AppCtx *ctx);         /* advance the market */
void trader_layout(AppState *st, const AppCtx *ctx);         /* the UI, both modes */
void trader_bench_step(AppState *st, const AppInputSink *in, int frame);

#endif /* TRADER_APP_H */
