/*
    opsdash_app.h - state and hooks for the RayClay Fleet Operations dashboard.

    The app: a 48-service inventory grid with a left-nav group filter and a live
    telemetry rail (latency sparkline, rolling counters, a pulsing incident
    banner). Below OPS_STACK_W the three panes become one scrolling page.

    Demonstrates: a dark panel theme over rcStyleDark, a custom titlebar carrying
    the scope filters, a scroll container with rcScrollbar, a card grid that
    reflows on viewport width, and per-frame animation driven from a dt.
*/
#ifndef OPSDASH_APP_H
#define OPSDASH_APP_H

#include "bench_app.h"          /* the shared AppCtx / AppInputSink / AppMode contract */
#include "opsdash_backend.h"    /* OpsStore: the fleet + the live telemetry band       */

/* The frozen bench scenario's version. Bump ONLY when the scripted path's rendered
   output changes - never for demo-only chrome. */
#define OPSDASH_BENCH_VERSION 5

/* Font ladder, baked from the bundled face. F_MONO is the telemetry readout and is
   a SIZE, not a face: the "monospace numbers" look comes from the zero-padded
   fixed-width strings the backend formats, so nothing here ships a second font. */
typedef enum { F_SMALL = 0, F_BODY, F_MONO, F_MD, F_TITLE, F_COUNT } OpsdashFont;

/* App state: a flat memset-able POD, so opsdash_seed can memset then set fields.
   The four filters compose - a service is drawn when it passes all of them. */
typedef struct {
    OpsStore store;         /* the backend model, BY VALUE (seed zeroes it)      */
    int      selected;      /* focused service, 0..OPS_SVC_COUNT-1               */
    int      group;         /* left-nav filter; -1 = all groups                  */
    int      regionFilter;  /* index into OPS_REGION_FILTER; 0 = every region    */
    int      tierFilter;    /* index into OPS_TIER_FILTER; 0 = every tier        */
    bool     onlyUnhealthy; /* titlebar toggle: hide the services reporting OK   */
    bool     seeded;        /* demo lazy-init guard                              */
} AppState;

/* THE PALETTE IS THE ONE AN OPERATIONS DASHBOARD IS READ IN: dark canvas, flat
   panels, one blue for action and saturated green/amber/red for thresholds. It is
   installed by opsdash_layout rather than by the runner, so every caller of that
   function draws the same picture.
   TWO BLUES: `.primary` is a FILL under white text and is dark enough to carry it;
   OPS_DATA_BLUE is a BAR, where the lighter shade reads. */
#define OPS_DATA_BLUE rcRgb(87, 148, 242)

static inline RC_Style ops_style(void) {
    RC_Style s = rcStyleDark();

    s.background   = rcRgb( 17,  18,  23);   /* canvas                       */
    s.surface      = rcRgb( 24,  27,  31);   /* panel                        */
    s.surfaceAlt   = rcRgb( 34,  37,  43);   /* inset: chips, selected card  */
    s.chrome       = rcRgb( 24,  27,  31);
    s.text         = rcRgb(244, 245, 245);
    s.textMuted    = rcRgb(156, 163, 175);
    s.primary      = rcRgb( 61, 113, 217);
    s.primaryHover = rcRgb( 55,  97, 206);
    s.danger       = rcRgb(242,  73,  92);
    s.dangerHover  = rcRgb(224,  56,  76);
    s.success      = rcRgb( 86, 166,  75);
    s.successHover = rcRgb(115, 191, 105);
    s.warning      = rcRgb(224, 180,   0);
    s.warningHover = rcRgb(242, 204,  12);
    s.border       = rcRgb( 42,  47,  55);
    /* Flat panel grammar: a dashboard is a grid of panels, and 8 px corners on
       fifty of them is the ornament that makes it read as a card wall. */
    s.radius       = 3.0f;
    return s;
}

/* The five-function app contract. No RC_App or window handle reaches any of them. */
void opsdash_seed  (AppState *st, unsigned seed);
void opsdash_update(AppState *st, const AppCtx *ctx);        /* advances ONLY the live band */
void opsdash_layout(AppState *st, const AppCtx *ctx);        /* the whole UI */
void opsdash_demo_chrome(AppState *st, const AppCtx *ctx);   /* demo-only overlay */
void opsdash_bench_step(AppState *st, const AppInputSink *in, int frame);

#endif /* OPSDASH_APP_H */
