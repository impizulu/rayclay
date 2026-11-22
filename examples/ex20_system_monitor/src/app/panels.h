/*
    panels.h - the panel types a Grafana dashboard is built from: stat, gauge, status
    tile, bar gauge, time series with a legend table, and table. Each is an ordinary
    function over rcBox, rcRow and rcColumn, so a panel is copied by copying its
    function. A panel reads AppState and writes only what its own controls change.
*/
#ifndef APP_PANELS_H
#define APP_PANELS_H

#include "app/state.h"

/* Every glyph is a header generated from an SVG in examples/assets/icons/;
   ex10_util_rc_icon_converter makes the same kind of header from your own
   SVGs. */
#include "icons/rc_icons_arrow_down.h"
#include "icons/rc_icons_arrow_up.h"
#include "icons/rc_icons_chevron_down.h"
#include "icons/rc_icons_chevron_left.h"
#include "icons/rc_icons_chevron_right.h"
#include "icons/rc_icons_chevron_up.h"
#include "icons/rc_icons_circle_alert.h"
#include "icons/rc_icons_circle_check.h"
#include "icons/rc_icons_expand.h"
#include "icons/rc_icons_info.h"
#include "icons/rc_icons_search.h"
#include "icons/rc_icons_x.h"

/* An app-defined element. rcComponent is public so an app can write a fourth element
   beside rcBox, rcRow and rcColumn, which are rcComponent with the library's defaults:
   dash_panel is Grafana's panel frame, under the same designated-initializer options
   every element takes. The defaults go in rcComponent's first argument, not into the
   options literal, because sharing one list warns at every call that overrides a
   default. min-w-0: a panel takes its width from its row, never from its title, as a
   web dashboard's card. */
static inline RC_ElementDeclaration panel_defaults(void) {
    Palette pal = app_palette();
    RC_ElementDeclaration zero = { 0 };

    return rcParseComponentOptions(
        RC_LIT(RC_ComponentOptions){ ._reserved = 0, .bg = pal.glass,
                                     .borderRadius = "all-xs",
                                     .border = { .color = pal.glassEdge, .width = 1 },
                                     .className = "flex-col min-w-0" }, zero);
}

/* The body runs once and rcComponent closes the panel after it, as it closes an rcBox.
   Leave the body by its end. A `continue` ends the panel only, never an enclosing loop's
   pass; so does a `break`, which the library also reports once. `return` and `goto` skip
   the close and are unsupported (rayclay.h, rcComponent). */
#define dash_panel(...) rcComponent(panel_defaults(), __VA_ARGS__)

/* The threshold colors: the band a value is in picks green, yellow or red. */
static inline RC_Color load_color(float percent) {
    Palette pal = app_palette();

    if (percent >= LOAD_CRIT) return pal.red;
    if (percent >= LOAD_WARN) return pal.yellow;
    return pal.green;
}

static inline RC_Color latency_color(float ms) {
    Palette pal = app_palette();

    if (ms >= LAT_CRIT) return pal.red;
    if (ms >= LAT_WARN) return pal.yellow;
    return pal.green;
}

static inline RC_Color health_color(SvcHealth h) {
    Palette pal = app_palette();

    if (h == SVC_DOWN)     return pal.red;
    if (h == SVC_DEGRADED) return pal.yellow;
    return pal.green;
}

static inline const char *state_label(SysProcState st) {
    switch (st) {
    case SYS_RUNNING: return "running";
    case SYS_STOPPED: return "stopped";
    default:          return "sleeping";
    }
}

static inline bool state_matches(int filter, SysProcState st) {
    switch (filter) {
    case 1:  return st == SYS_RUNNING;
    case 2:  return st == SYS_SLEEPING;
    case 3:  return st == SYS_STOPPED;
    default: return true;
    }
}

/* The slot at or just before time t: a series is oldest first, so a binary search. */
static inline int slot_at(const float *x, int count, float t) {
    int lo = 0, hi = count - 1;

    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;

        if (x[mid] <= t) lo = mid;
        else             hi = mid - 1;
    }
    return lo;
}

/* The slot whose time is closest to t. */
static inline int nearest_slot(const float *x, int count, float t) {
    int k = slot_at(x, count, t);

    return k + 1 < count && x[k + 1] - t < t - x[k] ? k + 1 : k;
}

/* The span of time the Resources charts show, in axis minutes: the picked range up to
   the newest sample, or what was dragged out on a chart. */
static inline void view_window(const AppState *st, float *from, float *to) {
    if (st->zoomed) {
        *from = st->zoomFrom;
        *to   = st->zoomTo;
    } else {
        *to   = st->at[HISTORY - 1];
        *from = *to - (float)RANGE_SEC[st->range] / 60.0f;
    }
}

/* The first and last slots of that span. The first is the slot at or just before its
   start, so a line runs into the plot's left edge rather than starting short of it: the
   plot clips its own geometry. */
static inline int range_first(const AppState *st) {
    float from, to;

    view_window(st, &from, &to);
    return slot_at(st->at, HISTORY, from);
}

static inline int range_last(const AppState *st) {
    return st->zoomed ? slot_at(st->at, HISTORY, st->zoomTo) : HISTORY - 1;
}

/* Average a long series down to at most @p most points: Grafana's max data points. Half
   an hour at one sample a second is 1800 points on a plot 600 px wide: three to a pixel,
   which can show only one. @p phase is how many samples the series has ever taken, so
   the buckets stay put as new samples arrive and the line does not shimmer. @p tx may
   be NULL for a sparkline, which has no x. */
static inline int thin_series(const float *x, const float *y, int count, int most, int phase,
                              float *tx, float *ty) {
    int k    = count / (most - 2) + 1;
    int left = (count - phase % k) % k;
    int n    = 0, i = 0;

    if (left == 0)
        left = k;
    while (i < count) {
        float sumX = 0.0f, sumY = 0.0f;
        int   j;

        for (j = 0; j < left && i < count; j++, i++) {
            if (tx) sumX += x[i];
            sumY += y[i];
        }
        if (tx) tx[n] = sumX / (float)j;
        ty[n++] = sumY / (float)j;
        left = k;
    }
    return n;
}

/* The first slot of a series that holds only its newest count readings. */
static inline int series_first(const AppState *st, int count) {
    int first = range_first(st);

    return first > HISTORY - count ? first : HISTORY - count;
}

static inline float series_peak(const float *y, int n) {
    float peak = 0.0f;
    int   i;

    for (i = 0; i < n; i++) {
        float v = y[i] < 0.0f ? -y[i] : y[i];

        if (v > peak) peak = v;
    }
    return peak;
}

/* The top a plot is drawn against: the next round number above the peak, so it does
   not jitter from sample to sample. A chart keeps its zero, or it lies about proportion. */
static inline float percent_ceiling(float peak) {
    int bands = (int)(peak * 1.15f) / 20 + 1;

    return (float)(bands > 5 ? 100 : bands * 20);
}

/* A rate's top: the first multiple of its tick step at or above the peak, the step 1, 2
   or 5 times a power of ten for @p ticks steps, as rcChart's own auto-fit picks it. So
   890 KB/s tops out at 1 MB/s, and the top moves only when the peak crosses a tick. It
   is pinned rather than left to auto-fit because the crosshair spans the whole axis. */
static inline float nice_ceiling(float peak, int ticks) {
    float raw = peak / (float)ticks, step = 1.0f, norm, top;

    if (!(raw > 0.0f))
        return 1.0f;
    while (step * 10.0f <= raw)
        step *= 10.0f;
    while (step > raw)
        step /= 10.0f;
    norm  = raw / step;
    step *= norm < 1.5f ? 1.0f : norm < 3.5f ? 2.0f : norm < 7.5f ? 5.0f : 10.0f;
    top   = (float)(int)(peak / step) * step;
    return top < peak ? top + step : top;
}

/* How a chart's numbers print, in the legend, in its readout and on its panels. */
typedef enum Unit { UNIT_PERCENT = 0, UNIT_GIB, UNIT_RATE, UNIT_MS } Unit;

static inline const char *value_text(RC_Arena *mem, double v, Unit unit) {
    double d = v < 0.0 ? -v : v;

    switch (unit) {
    case UNIT_GIB: return rcFormat(mem, "%.1f GiB", d).chars;
    case UNIT_MS:  return rcFormat(mem, "%.0f ms", d).chars;
    case UNIT_RATE:
        if (d >= 1000.0)
            return rcFormat(mem, "%.1f MB/s", d / 1000.0).chars;
        return rcFormat(mem, "%.0f KB/s", d).chars;
    default:       return rcFormat(mem, "%.1f%%", d).chars;
    }
}

/* The chart callbacks. Each writes as snprintf does. The time axis prints the clock in
   the picked zone, with seconds once a tick step is under a minute; the readout's
   header always has them. */
static inline int write_text(const char *text, char *out, int cap) {
    int n = 0;

    rcStrCopy(out, text, (size_t)cap);
    while (out[n])
        n++;
    return n;
}

static inline int clock_text(const AppState *st, double minutes, bool seconds, char *out, int cap) {
    RC_Arena *mem  = rcAppArena(st->app);
    int       secs = clock_seconds(minutes, zone_minutes(st));
    int       h    = secs / 3600, m = secs / 60 % 60;

    return write_text(seconds ? rcFormat(mem, "%02d:%02d:%02d", h, m, secs % 60).chars
                              : rcFormat(mem, "%02d:%02d", h, m).chars, out, cap);
}

static inline int clock_tick(double v, char *out, int cap, void *user) {
    const AppState *st = (const AppState *)user;

    return clock_text(st, v, st->tickSeconds, out, cap);
}

static inline int clock_readout(double v, char *out, int cap, void *user) {
    return clock_text((const AppState *)user, v, true, out, cap);
}

/* A rate axis: its ticks are round, so "10 MB/s" needs no decimal, and transmit, drawn
   below the zero line, still prints as a positive rate. */
static inline int rate_tick(double v, char *out, int cap, void *user) {
    RC_Arena *mem = (RC_Arena *)user;
    double    d   = v < 0.0 ? -v : v;

    if (d == 0.0)
        return write_text("0", out, cap);
    if (d >= 1000.0)
        return write_text(rcFormat(mem, "%g MB/s", d / 1000.0).chars, out, cap);
    return write_text(rcFormat(mem, "%g KB/s", d).chars, out, cap);
}

/* The hover readout of the same axis, with the decimals a tick does not need. */
static inline int rate_readout(double v, char *out, int cap, void *user) {
    return write_text(value_text((RC_Arena *)user, v, UNIT_RATE), out, cap);
}

/* The hover readout of a value axis: its .unit follows. A tick can read "60" where the
   readout reads "36.4". */
static inline int tenths_readout(double v, char *out, int cap, void *user) {
    return write_text(rcFormat((RC_Arena *)user, "%.1f", v).chars, out, cap);
}

static inline int whole_readout(double v, char *out, int cap, void *user) {
    return write_text(rcFormat((RC_Arena *)user, "%.0f", v).chars, out, cap);
}

/* One small icon control: a plain rcBox with an id is a button, .focusable makes it a Tab
   stop, and its class shades it under the pointer and while it is held. .disabled is the
   library's: it fades the box, takes no hover or click, and Tab passes it. Sized by the
   pointer, never by the width: 14 px around a 16 px glyph is the 44 px a finger needs. */
static inline bool icon_button(const char *id, RC_IconCallback icon, RC_Color tint,
                               const char *tip, bool enabled) {
    float pad = rcPointerIsCoarse() ? 14.0f : 7.0f;

    rcBox(.id = id, .focusable = true, .p = pad, .align = "cc", .borderRadius = "all-xs",
          .disabled = !enabled, .tooltip = tip,
          .className = "hover:bg-surface-alt active:bg-border") {
        icon(16.0f, tint);
    }
    return rcClicked(id);
}

/* A panel's title with Grafana's (i): the description is a hover away rather than a
   caption, which is what keeps a panel's chrome one line tall. A finger cannot hover,
   so it gets no (i) and the title has the room. */
static inline void panel_title(const char *title, const char *infoId, const char *tip) {
    RC_Style s = rcGetStyle();

    /* A truncate box with min-w-0 shrinks, as on the web, and ends its text in an ellipsis: no
       title can wrap over its panel or push the row past the window. */
    rcBox(.className = "truncate min-w-0") {
        rcTextC(title, .font = rcFontSemibold(14), .color = s.text);
    }
    if (tip && !rcPointerIsCoarse()) {
        rcBox(.id = infoId, .align = "cc", .tooltip = tip) {
            rcIconInfo(14.0f, s.textMuted);
        }
    }
}

/* A quiet tag: a card's region and tier, or whether a row's figures are real. */
static inline void tag(const char *text) {
    RC_Style s = rcGetStyle();

    rcBox(.bg = s.surfaceAlt, .px = 8, .py = 2, .align = "cc", .borderRadius = "all-full") {
        rcTextC(text, .color = s.textMuted, .size = 12, .wrap = "n");
    }
}

/* Defined in windows.h, which needs the panels below. */
static inline bool detach_offered(const AppState *st, Panel p);
static inline void detach_button(AppState *st, Panel p);

/* ---- STAT -------------------------------------------------------------------- */

typedef struct StatDef {
    const char *id, *sparkId, *infoId, *title;
    int         slot;   /* its row in stat_panel's scratch: rcSparkline borrows the data */
} StatDef;

/* One slot per stat: 0-2 the overview's, 3-8 This device's, 9-10 the live band's. */
enum { STAT_SLOTS = 11 };

typedef struct StatValue {
    const char   *value;
    const char   *unit;
    const char   *tip;
    const char   *caption;   /* a line under the number, as Grafana's description; NULL none */
    RC_Color      color;
    const float  *spark;     /* the visible tail, oldest first; NULL for none   */
    int           count;
    /* 0 leaves the sparkline's axis auto-fitting. Pin it when the series barely
       moves, or auto-fit magnifies the noise into a solid block. */
    float         max;
    RC_SeriesKind kind;
} StatValue;

/* Grafana's big number over its own trend. The reading takes its line and the trend
   fills the rest of the panel edge to edge, so the two never collide at any size. With
   no trend the reading keeps the same line, so a row of stats reads level. A stat with
   no number yet (value NULL) draws neither number nor unit, and keeps the line and the
   trend's box a number would have, so nothing moves when the first reading lands. */
static inline void stat_panel(AppState *st, const StatDef *def, const StatValue *v,
                              uint16_t big) {
    static float thinned[STAT_SLOTS][SPARK_POINTS];
    const float *spark = v->spark;
    /* One point is not a line: under two the box stays, empty. */
    int          count = v->count >= 2 ? v->count : 0;
    /* About the bundled face's normal line, ascent plus descent at 2400/2048 of the size
       (the library rounds each to device px), so a number landing moves nothing visible. */
    float        lineH = (float)big * 2400.0f / 2048.0f;

    if (count > SPARK_POINTS) {
        spark = thinned[def->slot];
        count = thin_series(NULL, v->spark, v->count, SPARK_POINTS, st->samples, NULL,
                            thinned[def->slot]);
    }

    dash_panel(.id = def->id, .w = "grow", .h = "grow") {
        rcRow(.gap = 6, .px = 8, .align = "cl", .w = "grow", .hType = RC_PX(PANEL_HEAD)) {
            panel_title(def->title, def->infoId, v->tip);
        }
        rcColumn(.align = "tc", .w = "grow", .h = "grow") {
            /* A percent sign sits against its number, as the gauges print it; a word
               unit keeps a space. */
            rcRow(.gap = v->unit && v->unit[0] == '%' ? 0.0f : 4.0f,
                  .hMin = v->value ? RC_UNSET_VAL : RC_VAL(lineH),
                  .className = "items-baseline") {
                if (v->value) {
                    rcTextC(v->value, .color = v->color, .size = big);
                    if (v->unit)
                        rcTextC(v->unit, .color = v->color,
                                .size = (uint16_t)(big >= 32 ? 20 : 14));
                }
            }
            if (v->caption) rcTextC(v->caption, .color = rcGetStyle().textMuted, .size = 12);
            if (spark) {
                rcBox(.w = "grow", .h = "grow") {
                    rcSparkline(def->sparkId, spark, count, .kind = v->kind, .color = v->color,
                                .min = RC_VAL(0.0f),
                                .max = v->max > 0.0f ? RC_VAL(v->max) : RC_UNSET_VAL);
                }
            }
        }
    }
}

/* ---- GAUGE ------------------------------------------------------------------- */

/* The gauge is drawn by the same helpers RayClay's icons use, in a square 100 units
   across, because no control draws an arc. Its 240 degree sweep is 48 steps of 5
   degrees, computed once by rotating a vector, so the app needs no libm. */
enum { GAUGE_STEPS = 48 };
static RC_IconPoint GAUGE_UNIT[GAUGE_STEPS + 1];

static inline void gauge_init(void) {
    float x = -0.8660254f, y = 0.5f;   /* lower left: 150 degrees, clockwise on screen */
    int   i;

    for (i = 0; i <= GAUGE_STEPS; i++) {
        float nx = x * 0.9961947f - y * 0.0871557f;

        GAUGE_UNIT[i].x = x;
        GAUGE_UNIT[i].y = y;
        y = x * 0.0871557f + y * 0.9961947f;
        x = nx;
    }
}

/* One band of the gauge between two fractions of the sweep. */
static inline void gauge_arc(RC_BoundingBox b, float radius, float width, float from, float to,
                             RC_Color c) {
    RC_IconPoint pts[GAUGE_STEPS + 3];
    float a = from * (float)GAUGE_STEPS, z = to * (float)GAUGE_STEPS;
    int   k = (int)a, n = 0;

    if (z - a < 0.05f)
        return;
    while ((float)k <= z && k <= GAUGE_STEPS) {
        /* Both ends fall between two steps; 5 degrees is close enough to a chord. */
        float at = (float)k < a ? a : (float)k;
        int   i  = (int)at < GAUGE_STEPS ? (int)at : GAUGE_STEPS - 1;
        float t  = at - (float)i;

        pts[n].x = 50.0f + radius * (GAUGE_UNIT[i].x + (GAUGE_UNIT[i + 1].x - GAUGE_UNIT[i].x) * t);
        pts[n].y = 52.0f + radius * (GAUGE_UNIT[i].y + (GAUGE_UNIT[i + 1].y - GAUGE_UNIT[i].y) * t);
        n++;
        k++;
    }
    {
        int   i = (int)z < GAUGE_STEPS ? (int)z : GAUGE_STEPS - 1;
        float t = z - (float)i;

        pts[n].x = 50.0f + radius * (GAUGE_UNIT[i].x + (GAUGE_UNIT[i + 1].x - GAUGE_UNIT[i].x) * t);
        pts[n].y = 52.0f + radius * (GAUGE_UNIT[i].y + (GAUGE_UNIT[i + 1].y - GAUGE_UNIT[i].y) * t);
        n++;
    }
    rcIconDrawPolyline(b, pts, n, 100.0f, width, false, c);
}

/* Runs when the frame is drawn: the thin threshold band outside, then the value over
   its track. @p c is the value's own threshold color. */
static inline void draw_gauge(RC_BoundingBox b, RC_Color c, const void *user) {
    const GaugeDraw *g   = (const GaugeDraw *)user;
    Palette          pal = app_palette();

    gauge_arc(b, 47.0f, 2.0f, 0.0f, g->warn - 0.005f, pal.green);
    gauge_arc(b, 47.0f, 2.0f, g->warn + 0.005f, g->crit - 0.005f, pal.yellow);
    gauge_arc(b, 47.0f, 2.0f, g->crit + 0.005f, 1.0f, pal.red);
    gauge_arc(b, 38.0f, 12.0f, 0.0f, 1.0f, g->track);
    gauge_arc(b, 38.0f, 12.0f, 0.0f, g->fraction, c);
}

/* @p size is the gauge's square; only its upper 78% holds ink, so the box beside it
   stops there and the panel keeps no dead strip under the arc. */
static inline void gauge_panel(const char *id, const char *infoId, const char *title,
                               const char *tip, GaugeDraw *g, float percent, RC_Color c,
                               const char *value, float size) {
    uint16_t big = (uint16_t)(size >= 120.0f ? 28 : size >= 90.0f ? 20 : 16);

    g->fraction = percent >= 100.0f ? 1.0f : percent <= 0.0f ? 0.0f : percent / 100.0f;
    g->warn     = LOAD_WARN / 100.0f;
    g->crit     = LOAD_CRIT / 100.0f;
    g->track    = rcGetStyle().surfaceAlt;

    dash_panel(.id = id, .w = "grow", .h = "grow") {
        rcRow(.gap = 6, .px = 8, .align = "cl", .w = "grow", .hType = RC_PX(PANEL_HEAD)) {
            panel_title(title, infoId, tip);
        }
        rcBox(.align = "tc", .w = "grow", .h = "grow") {
            rcBox(.wType = RC_PX(size), .hType = RC_PX(size * 0.78f)) {
                rcIconEmit(size, c, draw_gauge, g);
                rcBox(.floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_CENTER,
                                    .element = RC_ANCHOR_CENTER,
                                    .offset = { 0.0f, size * 0.15f },
                                    .capture = RC_CAPTURE_PASSTHROUGH }) {
                    rcTextC(value, .color = c, .size = big);
                }
            }
        }
    }
}

/* ---- STATUS TILE ------------------------------------------------------------- */

/* Grafana's status tile: a name over a block of the state's own color, which is what
   an operator's eye finds first. Dark text, because white on this green is 2.2:1. */
static inline void status_tile(const SysProc *p) {
    RC_Style s   = rcGetStyle();
    Palette  pal = app_palette();
    bool     up  = p->state != SYS_STOPPED;
    RC_Color c   = up ? pal.green : pal.red;

    dash_panel(.w = "grow", .h = "grow") {
        rcRow(.gap = 6, .px = 8, .align = "cl", .w = "grow", .hType = RC_PX(PANEL_HEAD)) {
            panel_title(p->name, NULL, NULL);
        }
        rcBox(.align = "cc", .w = "grow", .h = "grow",
              .gradient = { .from = c, .to = rcWithAlpha(c, 0.784f), .dir = "h" }) {
            if (up)
                rcTextL("Up", .font = rcFontSemibold(16), .color = s.background);
            else
                rcTextL("Down", .font = rcFontSemibold(16), .color = s.background);
        }
    }
}

/* The host's four daemons, in a block of two by two or a row of four. */
static inline void status_tiles(const SysHost *h, bool inRow) {
    rcColumn(.gap = GUTTER, .w = "grow", .h = "grow") {
        if (inRow) {
            rcRow(.gap = GUTTER, .w = "grow", .h = "grow") {
                for (int i = 0; i < SYS_UNITS; i++)
                    status_tile(&h->proc[i]);
            }
        } else {
            for (int r = 0; r < 2; r++) {
                rcRow(.gap = GUTTER, .w = "grow", .h = "grow") {
                    status_tile(&h->proc[r * 2]);
                    status_tile(&h->proc[r * 2 + 1]);
                }
            }
        }
    }
}

/* ---- BAR GAUGE --------------------------------------------------------------- */

/* Grafana's per-core bars: each core's bar is a row of segments, every segment in the
   color of the threshold band it stands for. */
enum { CORE_SEGMENTS = 30 };

static inline void bar_gauge(AppState *st, RC_Arena *mem, const char *w, const char *h) {
    RC_Style     s    = rcGetStyle();
    const float *core = st->view[st->host].host.core;
    int          i, k;

    dash_panel(.id = "p_cores", .w = w, .h = h) {
        rcRow(.gap = 6, .px = 8, .align = "cl", .w = "grow", .hType = RC_PX(PANEL_HEAD)) {
            panel_title("CPU cores", "i_cores",
                        "Load per logical core: green below 60%, yellow to 80%, red above");
            rcSpacer() {}
            if (rcShowsHover("p_cores"))
                detach_button(st, PANEL_CORES);
        }
        rcColumn(.gap = 4, .pb = 8, .px = 8, .w = "grow", .h = h) {
            for (i = 0; i < SYS_CORES; i++) {
                float    v = core[i];
                RC_Color c = load_color(v);

                if (st->cores != 0 && st->cores != i + 1)
                    continue;
                /* The bars share the panel's height, each at least 24 px and at most 56: in
                   a panel as tall as its content they are 24. One core picked by the Cores
                   variable fills the panel, Grafana's single bar gauge. */
                rcRow(.gap = 8, .align = "cl", .w = "grow", .h = "grow", .hMin = RC_VAL(24.0f),
                      .hMax = st->cores != 0 ? RC_UNSET_VAL : RC_VAL(56)) {
                    rcBox(.w = "36px") {
                        rcTextC(CORE_LABEL[i + 1], .color = s.textMuted, .size = 12);
                    }
                    /* Segments lit to the reading, the rest faintly in their own band, so the
                       bar shows where each threshold lies before the load gets there. */
                    rcRow(.gap = 2, .w = "grow", .h = "grow", .className = "min-w-0") {
                        for (k = 0; k < CORE_SEGMENTS; k++) {
                            float    at   = (float)k * 100.0f / (float)CORE_SEGMENTS;
                            RC_Color band = load_color(at);

                            rcBox(.bg = at < v ? band : rcWithAlpha(band, 0.118f), .w = "grow",
                                  .h = "grow") {}
                        }
                    }
                    rcBox(.align = "cr", .w = "48px") {
                        rcText(rcFormat(mem, "%.0f%%", (double)v), .color = c, .size = 16);
                    }
                }
            }
        }
    }
}

/* ---- TIME SERIES ------------------------------------------------------------- */

/* One id for the chart and one for the box that sizes it. rcChart grows on both axes,
   so the box around it is what gives it a size. */
static const struct { const char *chart, *box; } CHART_DEF[CHART_COUNT] = {
    { "sys_chart",  "box_chart" }, { "mem_chart", "box_mem" }, { "net_chart", "box_net" },
    { "disk_chart", "box_disk"  }, { "lat_chart", "box_lat" },
};

/* A legend entry is a series: what it draws, where its legend row is, and its ids. */
enum { LEGEND_MAX = 4 };

typedef struct LegendEntry {
    const char   *id, *label;
    RC_Color      color;
    const float  *y, *x;   /* the visible tail, oldest first */
    int           count;
    RC_SeriesKind kind;
} LegendEntry;

static inline bool series_shown(const AppState *st, ChartId chart, int entry) {
    return st->isolate[chart] == 0 || st->isolate[chart] == entry + 1;
}

/* A click shows that series alone, as Grafana's legend does; a second shows them all. */
static inline void legend_click(AppState *st, ChartId chart, int entry) {
    st->isolate[chart] = st->isolate[chart] == entry + 1 ? 0 : entry + 1;
}

/* The legend table under a plot: Mean, Max and Last over the time range. Each stat
   column clears the widest value ("12.3 MB/s"). */
static inline void legend_table(AppState *st, RC_Arena *mem, ChartId chart, const LegendEntry *e,
                                int n, Unit unit) {
    static const char *const HEAD[3] = { "Mean", "Max", "Last" };
    RC_Style    s    = rcGetStyle();
    Palette     pal  = app_palette();
    float       rowH = rcPointerIsCoarse() ? 44.0f : 22.0f;
    const char *colW = unit == UNIT_RATE ? "64px" : "52px";

    rcColumn(.pb = 4, .px = 8, .w = "grow") {
        rcRow(.gap = 4, .align = "cl", .w = "grow", .hType = RC_PX(22)) {
            rcTextL("Name", .color = pal.link, .size = 12);
            rcSpacer() {}
            for (int k = 0; k < 3; k++) {
                rcBox(.align = "cr", .w = colW) {
                    rcTextC(HEAD[k], .color = pal.link, .size = 12);
                }
            }
        }
        for (int i = 0; i < n; i++) {
            bool     on  = series_shown(st, chart, i);
            float    sum = 0.0f, peak = series_peak(e[i].y, e[i].count), stat[3];
            RC_Color ink = on ? s.text : s.textMuted;

            for (int j = 0; j < e[i].count; j++)
                sum += e[i].y[j];
            stat[0] = e[i].count > 0 ? sum / (float)e[i].count : 0.0f;
            stat[1] = peak;
            stat[2] = e[i].count > 0 ? e[i].y[e[i].count - 1] : 0.0f;

            rcRow(.id = e[i].id, .focusable = true, .gap = 4, .align = "cl", .w = "grow",
                  .hType = RC_PX(rowH),
                  .className = "border-t border-border hover:bg-surface-alt") {
                rcRow(.gap = 8, .align = "cl") {
                    rcBox(.bg = on ? e[i].color : rcWithAlpha(e[i].color, 0.275f),
                          .borderRadius = "all-xs", .wType = RC_PX(14), .hType = RC_PX(4)) {}
                    rcTextC(e[i].label, .color = ink, .size = 12);
                }
                rcSpacer() {}
                for (int k = 0; k < 3; k++) {
                    rcBox(.align = "cr", .w = colW) {
                        rcTextC(value_text(mem, stat[k], unit), .color = ink, .size = 12);
                    }
                }
            }
            if (rcClicked(e[i].id))
                legend_click(st, chart, i);
        }
    }
}

/* The time axis between two moments, its ticks on round clock minutes. */
static inline RC_Axis time_axis(AppState *st, float from, float to) {
    RC_Axis x = { 0 };

    /* A step is 1, 2 or 5 x 10^k of the axis's unit, the minute: with six ticks it drops
       under a minute below about 4.5, where HH:MM would print one time twice. */
    st->tickSeconds = to - from < 4.6f;
    x.min    = RC_VAL(from);
    x.max    = RC_VAL(to);
    x.ticks  = 6;
    x.grid   = true;
    x.format = clock_tick;
    x.formatUserData = st;
    x.readoutFormat = clock_readout;
    return x;
}

static inline RC_Axis view_axis(AppState *st) {
    float from, to;

    view_window(st, &from, &to);
    return time_axis(st, from, to);
}

/* The moment the pointer is over in another panel, read off this panel's own series: a
   small readout hung on the crosshair, drawn in the chart's box and cut by the page. */
static inline void cross_readout(const AppState *st, RC_Arena *mem, ChartId chart,
                                 const LegendEntry *e, int n, Unit unit, RC_Axis x) {
    RC_Style s    = rcGetStyle();
    RC_Box   plot = rcChartPlotRect(CHART_DEF[chart].chart);
    RC_Box   box  = rcGetElementBox(CHART_DEF[chart].box);
    float    frac = (st->crossAt - x.min.value) / (x.max.value - x.min.value);
    bool     flip = frac > 0.6f;
    int      secs = clock_seconds(st->crossAt, zone_minutes(st));
    float    dx, dy;

    if (!plot.found || !box.found || plot.width <= 0.0f || frac < 0.0f || frac > 1.0f)
        return;
    dx = plot.x - box.x + frac * plot.width + (flip ? -8.0f : 8.0f);
    dy = plot.y - box.y + 4.0f;
    rcColumn(.bg = s.surfaceAlt, .gap = 2, .px = 8, .py = 5, .borderRadius = "all-xs",
             .border = { .color = s.border, .width = 1 },
             .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                           .element = flip ? RC_ANCHOR_TOP_RIGHT : RC_ANCHOR_TOP_LEFT,
                           .offset = { dx, dy }, .zIndex = 2,
                           .capture = RC_CAPTURE_PASSTHROUGH }) {
        rcText(rcFormat(mem, "%02d:%02d:%02d", secs / 3600, secs / 60 % 60, secs % 60),
               .color = s.textMuted, .size = 12);
        for (int i = 0; i < n; i++) {
            if (!series_shown(st, chart, i) || e[i].count < 1)
                continue;
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                rcBox(.bg = e[i].color, .borderRadius = "all-xs", .wType = RC_PX(10),
                      .hType = RC_PX(3)) {}
                rcTextC(e[i].label, .color = s.text, .size = 12);
                rcSpacer() {}
                rcTextC(value_text(mem, e[i].y[nearest_slot(e[i].x, e[i].count, st->crossAt)],
                                   unit), .color = s.text, .size = 12);
            }
        }
    }
}

/* The span being dragged out, over the plot it is dragged on. */
static inline void brush_box(const AppState *st, ChartId chart, RC_Axis x) {
    RC_Box  plot = rcChartPlotRect(CHART_DEF[chart].chart);
    RC_Box  box  = rcGetElementBox(CHART_DEF[chart].box);
    float   lo   = st->brushFrom < st->brushTo ? st->brushFrom : st->brushTo;
    float   hi   = st->brushFrom < st->brushTo ? st->brushTo : st->brushFrom;
    float   k    = plot.width / (x.max.value - x.min.value);
    Palette pal  = app_palette();

    if (!plot.found || !box.found)
        return;
    rcBox(.bg = rcWithAlpha(pal.link, 0.196f), .border = { .color = pal.link, .width = 1 },
          .wType = RC_PX((hi - lo) * k), .hType = RC_PX(plot.height),
          .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                        .element = RC_ANCHOR_TOP_LEFT,
                        .offset = { plot.x - box.x + (lo - x.min.value) * k, plot.y - box.y },
                        .zIndex = 2, .capture = RC_CAPTURE_PASSTHROUGH }) {}
}

/* One plot for every time series: the entries the legend has not hidden, then the
   caller's extra marks, then the crosshair when the pointer is over another panel.
   rcChart reads a chart of bars only with the pointer on a bar, so the disk chart takes
   the crosshair under the pointer too, in place of a tooltip that would blink off in
   every gap. */
static inline void plot(AppState *st, RC_Arena *mem, ChartId chart, const LegendEntry *e, int n,
                        Unit unit, RC_Axis x, RC_Axis y, const RC_Series *extra, int extraCount) {
    static float crossX[CHART_COUNT][2], crossY[CHART_COUNT][2];
    static float thinX[CHART_COUNT][LEGEND_MAX][PLOT_POINTS];
    static float thinY[CHART_COUNT][LEGEND_MAX][PLOT_POINTS];
    RC_Series ser[RC_CHART_MAX_SERIES];
    bool      bars  = chart == CHART_DISK;
    bool      cross = st->crossChart >= 0 && (st->crossChart != (int)chart || bars);
    int       phase = chart == CHART_LAT ? (int)st->fleet.tick : st->samples;
    int       k = 0, i;

    for (i = 0; i < n; i++) {
        RC_Series one = { 0 };

        if (!series_shown(st, chart, i) || e[i].count < 2)
            continue;
        one.y         = e[i].y;
        one.x         = e[i].x;
        one.count     = e[i].count;
        one.kind      = e[i].kind;
        one.color     = e[i].color;
        one.label     = e[i].label;
        one.thickness = 1.5f;
        if (one.count > PLOT_POINTS) {
            one.x     = thinX[chart][i];
            one.y     = thinY[chart][i];
            one.count = thin_series(e[i].x, e[i].y, e[i].count, PLOT_POINTS, phase,
                                    thinX[chart][i], thinY[chart][i]);
        }
        ser[k++] = one;
    }
    for (i = 0; i < extraCount && k > 0; i++)
        ser[k++] = extra[i];
    if (cross && k > 0) {
        RC_Series guide = { 0 };

        crossX[chart][0] = crossX[chart][1] = st->crossAt;
        crossY[chart][0] = y.min.value;
        crossY[chart][1] = y.max.value;
        guide.y         = crossY[chart];
        guide.x         = crossX[chart];
        guide.count     = 2;
        guide.color     = rcWithAlpha(rcGetStyle().text, 0.59f);
        guide.thickness = 1.0f;
        ser[k++] = guide;
    }
    rcBox(.id = CHART_DEF[chart].box, .px = 4, .w = "grow", .h = "grow") {
        if (k == 0) {
            /* One reading is not a line, and a grid holding a point reads as broken. */
            rcBox(.align = "cc", .w = "grow", .h = "grow") {
                rcTextL("No data", .color = rcGetStyle().textMuted, .size = 20);
            }
        } else {
            rcChart(CHART_DEF[chart].chart, ser, k, .className = "text-[12px]", .x = x, .y = y,
                    .tooltip = !bars);
            if (cross)
                cross_readout(st, mem, chart, e, n, unit, x);
            if (st->brushChart == (int)chart)
                brush_box(st, chart, x);
        }
    }
}

static inline RC_Axis value_axis(RC_Arena *mem, float yMin, float yMax, const char *unit) {
    RC_Axis y = { 0 };

    y.min   = RC_VAL(yMin);
    y.max   = RC_VAL(yMax);
    y.ticks = 4;
    y.grid  = true;
    y.unit  = unit;
    y.formatUserData = mem;
    y.readoutFormat = tenths_readout;
    return y;
}

/* The load this panel draws: the core the Cores variable picks, or the mean of them all. */
static inline const float *cpu_load(const AppState *st) {
    const HostView *v = &st->view[st->host];

    return st->cores > 0 ? v->core[st->cores - 1] : v->cpu;
}

/* The series a panel's legend shows, with the x positions each needs. */
static inline void entry_set(LegendEntry *e, const char *id, const char *label, RC_Color color,
                             const float *y, const float *x, int first, int count,
                             RC_SeriesKind kind) {
    e->id    = id;
    e->label = label;
    e->color = color;
    e->y     = y + first;
    e->x     = x + first;
    e->count = count > 0 ? count : 0;
    e->kind  = kind;
}

/* CPU usage: the load, and how it splits between user, system and iowait, each a band
   the legend table prints. Scratch arrays because rcChart borrows them until the frame
   is drawn; a panel is docked or floating, never both, so one set is enough. */
static inline void cpu_panel(AppState *st, RC_Arena *mem, const char *w, const char *h) {
    static float user[HISTORY], sys[HISTORY], wait[HISTORY];
    const HostView *v = &st->view[st->host];
    LegendEntry e[4];
    int         first = range_first(st), n = range_last(st) - first + 1, i;
    const float *load = cpu_load(st);
    float       peak;
    Palette     pal = app_palette();

    for (i = first; i < HISTORY; i++) {
        user[i] = load[i] * v->userShare[i];
        sys[i]  = load[i] * v->sysShare[i];
        wait[i] = load[i] - user[i] - sys[i];
    }
    entry_set(&e[0], "lg_cpu_total", "total", rcRgb(236, 240, 255), load, st->at, first, n,
              RC_SERIES_LINE);
    entry_set(&e[1], "lg_cpu_user", "user", pal.blue, user, st->at, first, n, RC_SERIES_AREA);
    entry_set(&e[2], "lg_cpu_sys", "system", pal.purple, sys, st->at, first, n, RC_SERIES_AREA);
    entry_set(&e[3], "lg_cpu_wait", "iowait", pal.orange, wait, st->at, first, n, RC_SERIES_AREA);
    peak = series_peak(load + first, n > 0 ? n : 0);

    dash_panel(.id = "p_chart", .w = w, .h = h) {
        rcRow(.gap = 6, .px = 8, .align = "cl", .w = "grow", .hType = RC_PX(PANEL_HEAD)) {
            panel_title("CPU usage", "i_chart",
                        "All cores or the one the Cores variable picks. Hover to read every "
                        "panel at that moment; drag across a chart to zoom them all; click a "
                        "legend row to isolate it.");
            rcSpacer() {}
            if (rcShowsHover("p_chart"))
                detach_button(st, PANEL_CHART);
        }
        plot(st, mem, CHART_CPU, e, 4, UNIT_PERCENT, view_axis(st),
             value_axis(mem, 0.0f, percent_ceiling(peak), "%"), NULL, 0);
        legend_table(st, mem, CHART_CPU, e, 4, UNIT_PERCENT);
    }
}

/* Memory: what is in use and what the kernel holds as cache, against the machine's
   total, with swap as a line. */
static inline void memory_panel(AppState *st, RC_Arena *mem) {
    const HostView *v = &st->view[st->host];
    int   first = range_first(st), n = range_last(st) - first + 1;
    LegendEntry e[3];
    Palette     pal = app_palette();

    entry_set(&e[0], "lg_mem_used", "used", pal.purple, v->memUsed, st->at, first, n,
              RC_SERIES_AREA);
    entry_set(&e[1], "lg_mem_cache", "cache", pal.teal, v->memCache, st->at, first, n,
              RC_SERIES_AREA);
    entry_set(&e[2], "lg_mem_swap", "swap", pal.orange, v->swap, st->at, first, n,
              RC_SERIES_LINE);

    dash_panel(.id = "p_mem", .w = "grow", .h = "grow") {
        rcRow(.gap = 6, .px = 8, .align = "cl", .w = "grow", .hType = RC_PX(PANEL_HEAD)) {
            panel_title("Memory", "i_mem_chart",
                        "Against the machine's total: what is in use, what the kernel "
                        "holds as cache, and swap");
        }
        plot(st, mem, CHART_MEM, e, 3, UNIT_GIB, view_axis(st),
             value_axis(mem, 0.0f, v->host.profile->memTotalGiB, " GiB"), NULL, 0);
        legend_table(st, mem, CHART_MEM, e, 3, UNIT_GIB);
    }
}

/* Network traffic: receive above the zero line and transmit mirrored below it, the
   form node-exporter's dashboards use, so the two never hide each other. */
static inline void network_panel(AppState *st, RC_Arena *mem) {
    static float txDown[HISTORY];
    const HostView *v = &st->view[st->host];
    int   first = range_first(st), n = range_last(st) - first + 1, i;
    float top;
    LegendEntry e[2];
    RC_Axis y;
    Palette pal = app_palette();

    for (i = first; i < HISTORY; i++)
        txDown[i] = -v->tx[i];
    entry_set(&e[0], "lg_net_rx", "receive", pal.teal, v->rx, st->at, first, n, RC_SERIES_AREA);
    entry_set(&e[1], "lg_net_tx", "transmit", pal.orange, txDown, st->at, first, n,
              RC_SERIES_AREA);
    top = series_peak(e[0].y, e[0].count);
    if (series_peak(e[1].y, e[1].count) > top)
        top = series_peak(e[1].y, e[1].count);
    /* Each half of the mirrored axis holds two of its four ticks. */
    top = nice_ceiling(top, 2);
    y = value_axis(mem, -top, top, NULL);
    y.format        = rate_tick;
    y.readoutFormat = rate_readout;

    dash_panel(.id = "p_net", .w = "grow", .h = "grow") {
        rcRow(.gap = 6, .px = 8, .align = "cl", .w = "grow", .hType = RC_PX(PANEL_HEAD)) {
            panel_title("Network traffic", "i_net_chart",
                        "Receive above the line, transmit below");
        }
        plot(st, mem, CHART_NET, e, 2, UNIT_RATE, view_axis(st), y, NULL, 0);
        legend_table(st, mem, CHART_NET, e, 2, UNIT_RATE);
    }
}

/* Disk throughput as bars: one bucket per thirtieth of the range, each on a whole
   multiple of its own width, so the bars step along instead of shimmering. */
enum { DISK_BUCKETS = 30 };

static inline void disk_panel(AppState *st, RC_Arena *mem) {
    static float bx[DISK_BUCKETS], brd[DISK_BUCKETS], bwr[DISK_BUCKETS];
    const HostView *v = &st->view[st->host];
    int   samples[DISK_BUCKETS] = { 0 };
    int   first = range_first(st), last = range_last(st), newest, i;
    float from, to, width, top;
    LegendEntry e[2];
    RC_Axis y;
    Palette pal = app_palette();

    view_window(st, &from, &to);
    width  = (to - from) / (float)DISK_BUCKETS;
    newest = (int)(to / width);
    for (i = 0; i < DISK_BUCKETS; i++) {
        bx[i]  = ((float)(newest - DISK_BUCKETS + 1 + i) + 0.5f) * width;
        brd[i] = bwr[i] = 0.0f;
    }
    for (i = first; i <= last; i++) {
        int b = (int)(st->at[i] / width) - (newest - DISK_BUCKETS + 1);

        if (b < 0 || b >= DISK_BUCKETS)
            continue;
        brd[b] += v->rd[i];
        bwr[b] += v->wr[i];
        samples[b]++;
    }
    for (i = 0; i < DISK_BUCKETS; i++) {
        if (samples[i] > 0) {
            brd[i] /= (float)samples[i];
            bwr[i] /= (float)samples[i];
        }
    }
    entry_set(&e[0], "lg_disk_rd", "read", pal.blue, brd, bx, 0, DISK_BUCKETS, RC_SERIES_BAR);
    entry_set(&e[1], "lg_disk_wr", "write", pal.orange, bwr, bx, 0, DISK_BUCKETS, RC_SERIES_BAR);
    top = series_peak(brd, DISK_BUCKETS);
    if (series_peak(bwr, DISK_BUCKETS) > top)
        top = series_peak(bwr, DISK_BUCKETS);
    y = value_axis(mem, 0.0f, nice_ceiling(top, 4), NULL);
    y.format        = rate_tick;
    y.readoutFormat = rate_readout;

    dash_panel(.id = "p_disk", .w = "grow", .h = "grow") {
        rcRow(.gap = 6, .px = 8, .align = "cl", .w = "grow", .hType = RC_PX(PANEL_HEAD)) {
            panel_title("Disk throughput", "i_disk_chart",
                        "Mean read and write rate in each bucket of the time range");
        }
        plot(st, mem, CHART_DISK, e, 2, UNIT_RATE, view_axis(st), y, NULL, 0);
        legend_table(st, mem, CHART_DISK, e, 2, UNIT_RATE);
    }
}

/* ---- TABLE ------------------------------------------------------------------- */

/* One table drives both the clickable header row and the table's column widths, so the
   two cannot drift apart. minW is what the column needs before it earns its place. */
static const struct {
    const char *label, *w, *id;
    float       minW;
    bool        right;
} COLUMN[SORT_COUNT] = {
    { "Process", "grow",  "th_name",  120.0f, false },
    { "PID",     "72px",  "th_pid",    72.0f, true  },
    { "User",    "96px",  "th_user",   96.0f, false },
    { "CPU %",   "148px", "th_cpu",   148.0f, true  },
    { "Memory",  "96px",  "th_mem",    96.0f, true  },
    { "State",   "104px", "th_state", 104.0f, false },
};

/* A table never scrolls sideways: the columns that fit are drawn, most important first
   and the name always, and what does not fit moves to a second line in each row, so a
   phone can still read every column and a chip can still sort by it. */
static inline int visible_columns(float width, SortKey *out) {
    static const SortKey RANK[SORT_COUNT] = {
        SORT_NAME, SORT_CPU, SORT_MEM, SORT_STATE, SORT_PID, SORT_USER
    };
    bool  keep[SORT_COUNT] = { false };
    float used = 0.0f;
    int   n = 0, i;

    for (i = 0; i < SORT_COUNT; i++) {
        SortKey c = RANK[i];

        if (c == SORT_NAME || used + COLUMN[c].minW <= width) {
            keep[c] = true;
            used += COLUMN[c].minW;
        }
    }
    for (i = 0; i < SORT_COUNT; i++)
        if (keep[i])
            out[n++] = (SortKey)i;
    return n;
}

static inline bool column_drawn(const SortKey *vis, int visCount, int c) {
    for (int v = 0; v < visCount; v++)
        if ((int)vis[v] == c)
            return true;
    return false;
}

/* Below zero when p sorts before q in ascending order. */
static inline int proc_compare(const SysProc *p, const SysProc *q, SortKey key) {
    const char *x = p->name, *y = q->name;

    switch (key) {
    case SORT_PID:   return (p->pid > q->pid) - (p->pid < q->pid);
    case SORT_CPU:   return (p->cpu > q->cpu) - (p->cpu < q->cpu);
    case SORT_MEM:   return (p->memMiB > q->memMiB) - (p->memMiB < q->memMiB);
    case SORT_STATE: return (p->state > q->state) - (p->state < q->state);
    case SORT_USER:  x = p->user; y = q->user; break;
    default:         break;
    }
    while (*x && *x == *y) {
        x++;
        y++;
    }
    return (int)(unsigned char)*x - (int)(unsigned char)*y;
}

/* Insertion sort rather than qsort: C's qsort has no context parameter, so the key
   would become a file-static the comparator reads. Equal keys fall back to the pid, in
   both directions, so a live table re-sorted every second moves only the rows whose
   value really passed another's. Only the host on screen is kept in order; picking
   another host sorts it. */
static inline void resort(AppState *st) {
    HostView *v = &st->view[st->host];
    int i, j;

    for (i = 1; i < v->host.procCount; i++) {
        int            key = v->order[i];
        const SysProc *p   = &v->host.proc[key];

        for (j = i - 1; j >= 0; j--) {
            const SysProc *q = &v->host.proc[v->order[j]];
            int            c = proc_compare(p, q, st->sortKey);

            if (st->sortDesc) c = -c;
            if (c > 0 || (c == 0 && p->pid > q->pid)) break;
            v->order[j + 1] = v->order[j];
        }
        v->order[j + 1] = key;
    }
}

static inline void sort_by(AppState *st, SortKey key) {
    if (st->sortKey == key) {
        st->sortDesc = !st->sortDesc;
    } else {
        st->sortKey = key;
        /* Text reads best A-Z, numbers biggest-first. */
        st->sortDesc = key != SORT_NAME && key != SORT_USER;
    }
    st->page = 0;
    resort(st);
}

static inline void status_dot(RC_Color c) {
    rcBox(.bg = c, .borderRadius = "all-full", .w = "8px", .h = "8px") {}
}

/* One cell's content. CPU is Grafana's gauge cell: a bar in the band's color and the
   number beside it. @p mem is the arena of the window being drawn. */
static inline void proc_cell(RC_Arena *mem, RC_Style s, const SysProc *p, SortKey col,
                             uint16_t size) {
    Palette pal = app_palette();

    switch (col) {
    case SORT_NAME:
        rcTextC(p->name, .color = s.text, .size = size);
        break;
    case SORT_PID:
        rcText(rcFormat(mem, "%d", p->pid), .color = s.textMuted, .size = size);
        break;
    case SORT_USER:
        rcTextC(p->user, .color = s.textMuted, .size = size);
        break;
    case SORT_CPU: {
        float bar = p->cpu > 100.0f ? 100.0f : p->cpu;

        rcBox(.bg = s.surfaceAlt, .w = "grow", .h = "6px") {
            rcBox(.bg = load_color(p->cpu), .h = "grow", .wType = RC_PCT(bar)) {}
        }
        rcBox(.align = "cr", .w = "44px") {
            rcText(rcFormat(mem, "%.1f", (double)p->cpu), .color = s.text, .size = size);
        }
        break;
    }
    case SORT_MEM:
        if (p->memMiB >= 1024.0f)
            rcText(rcFormat(mem, "%.1f GiB", (double)p->memMiB / 1024.0), .color = s.text,
                   .size = size);
        else
            rcText(rcFormat(mem, "%.0f MiB", (double)p->memMiB), .color = s.text, .size = size);
        break;
    default:
        status_dot(p->state == SYS_RUNNING ? pal.green
                   : p->state == SYS_STOPPED ? pal.red : s.textMuted);
        rcTextC(state_label(p->state), .color = p->state == SYS_SLEEPING ? s.textMuted : s.text,
                .size = size);
        break;
    }
}

static inline void sort_arrow(const AppState *st) {
    Palette pal = app_palette();

    if (st->sortDesc) rcIconArrowDown(12.0f, pal.link);
    else              rcIconArrowUp(12.0f, pal.link);
}

/* A dropped column cannot be clicked, so it keeps its sort as a chip, under the id its
   header has when it is drawn. */
static inline void sort_chips(AppState *st, const SortKey *vis, int visCount) {
    RC_Style s   = rcGetStyle();
    Palette  pal = app_palette();
    float    pad = rcPointerIsCoarse() ? 15.0f : 3.0f;

    rcRow(.id = "sort_chips", .gap = 6, .pb = 6, .px = 8, .align = "cl", .w = "grow",
          .className = "flex-wrap") {
        rcTextL("Sort by", .color = s.textMuted, .size = 12);
        for (int c = 0; c < SORT_COUNT; c++) {
            const char *id = COLUMN[c].id;

            if (column_drawn(vis, visCount, c))
                continue;
            rcRow(.id = id, .focusable = true, .gap = 4, .px = 8, .py = pad, .align = "cc",
                  .borderRadius = "all-xs", .border = { .color = s.border, .width = 1 },
                  .className = "hover:bg-surface-alt") {
                rcTextC(COLUMN[c].label, .color = pal.link, .size = 12);
                if ((int)st->sortKey == c)
                    sort_arrow(st);
            }
            if (rcClicked(id))
                sort_by(st, (SortKey)c);
        }
    }
}

/* The header row is drawn here rather than by the table because a table's header cell holds
   its title alone, and a header class's hover: lights the whole row; this one carries the sort
   arrow and fills one cell on hover. It stays out of the rows, so it never scrolls away. */
static inline void table_header(AppState *st, const SortKey *vis, int visCount) {
    RC_Style s   = rcGetStyle();
    Palette  pal = app_palette();

    rcRow(.border = { .color = s.border }, .w = "grow",
          .hType = RC_PX(rcPointerIsCoarse() ? 44 : 30), .className = "border-b") {
        for (int v = 0; v < visCount; v++) {
            SortKey     c  = vis[v];
            const char *id = COLUMN[c].id;

            rcRow(.id = id, .focusable = true, .gap = 4, .px = 8, .w = COLUMN[c].w, .h = "grow",
                  .className = COLUMN[c].right ? "items-center justify-end hover:bg-surface-alt"
                                               : "items-center hover:bg-surface-alt") {
                rcTextC(COLUMN[c].label, .font = rcFontSemibold(13), .color = pal.link);
                if (st->sortKey == c)
                    sort_arrow(st);
            }
            if (rcClicked(id))
                sort_by(st, c);
        }
    }
}

/* One row, from the same visible list the header drew. The id comes from the data
   index, so the row's hover: follows the process rather than the screen slot. */
static inline void proc_row(RC_Arena *mem, RC_Style s, const SysProc *p, int index,
                            const SortKey *vis, int visCount) {
    const char *rowId = rcFormat(mem, "proc_%d", index).chars;

    rcTableRow(.id = rowId, .className = "bg-surface hover:bg-surface-alt");
    if (visCount < SORT_COUNT) {
        /* One table cell holding both lines, so the second can run the row's full width. */
        rcColumn(.gap = 2, .w = "grow", .hType = RC_PX(ROW_H_STACK),
                 .className = "justify-center border-b border-border") {
            rcRow(.w = "grow") {
                for (int v = 0; v < visCount; v++) {
                    SortKey c = vis[v];

                    rcRow(.gap = 8, .px = 8, .w = COLUMN[c].w,
                          .className = COLUMN[c].right ? "items-center justify-end"
                                                       : "items-center truncate") {
                        proc_cell(mem, s, p, c, 13);
                    }
                }
            }
            rcRow(.gap = 12, .px = 8, .align = "cl", .w = "grow") {
                for (int c = 0; c < SORT_COUNT; c++) {
                    if (column_drawn(vis, visCount, c))
                        continue;
                    rcRow(.gap = 6, .align = "cl") {
                        proc_cell(mem, s, p, (SortKey)c, 12);
                    }
                }
            }
        }
        return;
    }
    for (int v = 0; v < visCount; v++) {
        SortKey c = vis[v];

        if (v)
            rcTableNext();
        rcRow(.gap = c == SORT_CPU ? 8.0f : 6.0f, .px = 8, .w = "grow", .hType = RC_PX(ROW_H),
              .className = COLUMN[c].right ? "items-center justify-end border-b border-border"
                                           : "items-center truncate border-b border-border") {
            proc_cell(mem, s, p, c, 13);
        }
    }
}

/* The process filter, resetting the page because the rows just changed: typed, or emptied
   by its x. */
static inline void proc_filter(AppState *st) {
    if (rcTextInput("proc_q", st->procQuery, sizeof st->procQuery,
                    .className = rcPointerIsCoarse() ? NULL : "h-6.5",
                    .placeholder = "Filter processes", .leadingIcon = rcIconSearch,
                    .clearButton = true))
        st->page = 0;
}

/* Grafana's table panel: dense rows on hairlines, blue sortable headers. It pages in the
   main window, where a table that scrolled itself would trap a finger's swipe (a pan
   goes to the innermost scroller); detached, its own window bounds it and it scrolls. */
static inline void process_table(AppState *st, RC_Arena *mem, const char *w, const char *h) {
    RC_Style        s = rcGetStyle();
    const HostView *v = &st->view[st->host];
    RC_TableColumn  cols[SORT_COUNT];
    SortKey         vis[SORT_COUNT];
    float           width = content_width() - 48.0f;
    int             visCount = visible_columns(width, vis);
    int             dropped = SORT_COUNT - visCount;
    bool            paged = !st->app || rcAppCurrentWindow(st->app) == rcAppMainWindow(st->app);
    bool            fieldRow = width < 440.0f;   /* the head cannot hold the field too */
    float           rowH = dropped ? (float)ROW_H_STACK : (float)ROW_H;
    int             shown[SYS_PROCS];
    int             n = 0, running = 0, pages, from, to, i;

    for (i = 0; i < v->host.procCount; i++) {
        const SysProc *p = &v->host.proc[v->order[i]];

        running += p->state == SYS_RUNNING;
        if (state_matches(st->stateFilter, p->state) && text_has(p->name, st->procQuery))
            shown[n++] = v->order[i];
    }
    pages = n > 0 ? (n + PAGE_ROWS - 1) / PAGE_ROWS : 1;
    if (st->page >= pages) st->page = pages - 1;
    from = st->page * PAGE_ROWS;
    to   = from + PAGE_ROWS < n ? from + PAGE_ROWS : n;

    for (i = 0; i < visCount; i++) {
        RC_TableColumn c = { 0 };

        c.w = COLUMN[vis[i]].w;
        c.align = COLUMN[vis[i]].right ? "cr" : "cl";
        cols[i] = c;
    }
    if (dropped) {
        cols[0].w = "grow";
        cols[0].align = "cl";
    }

    dash_panel(.id = "p_procs", .w = w, .h = h) {
        rcRow(.gap = 8, .pl = 8, .pr = 4, .align = "cl", .w = "grow", .hType = RC_PX(36)) {
            panel_title("Processes", "i_procs",
                        rcFormat(mem, "%d processes on %s, %d running. Click a header to sort.",
                                 v->host.procCount, HOST_LABEL[st->host], running).chars);
            rcSpacer() {}
            /* The table always fits: narrow it and columns go rather than a sideways
               scrollbar appearing, and this says how many went. */
            if (dropped) {
                rcBox(.id = "proc_hidden",
                      .tooltip = "These read on each row's second line, and sort by the chips") {
                    tag(rcFormat(mem, "+%d %s", dropped,
                                 dropped == 1 ? "column" : "columns").chars);
                }
            }
            if (!fieldRow) {
                rcBox(.w = "176px") {
                    proc_filter(st);
                }
            }
            /* The slot stays while the button hides, so the field beside it holds still
               as the pointer reaches for it. */
            if (detach_offered(st, PANEL_TABLE)) {
                rcBox(.wType = RC_PX(30), .hType = RC_PX(30)) {
                    if (rcShowsHover("p_procs"))
                        detach_button(st, PANEL_TABLE);
                }
            }
        }
        if (fieldRow) {
            rcBox(.pb = 8, .px = 8, .w = "grow") {
                proc_filter(st);
            }
        }
        if (dropped)
            sort_chips(st, vis, visCount);
        rcColumn(.px = 8, .w = "grow", .h = paged ? "fit" : "grow") {
            table_header(st, vis, visCount);
            if (n == 0) {
                rcColumn(.gap = 8, .align = "cc", .w = "grow", .hType = RC_PX(ROW_H * 3)) {
                    rcTextL("No processes match", .color = s.textMuted, .size = 14);
                    if (rcButton("proc_clear", "Clear filters", .variant = RC_BTN_GHOST)) {
                        st->procQuery[0] = '\0';
                        st->stateFilter  = 0;
                    }
                }
            } else if (paged) {
                if (rcBeginTable("proc_table", cols, dropped ? 1 : visCount,
                                 .cellPadding = RC_VAL(0))) {
                    for (i = from; i < to; i++)
                        proc_row(mem, s, &v->host.proc[shown[i]], shown[i], vis, visCount);
                    rcEndTable();
                }
            } else {
                rcBox(.w = "grow", .h = "grow") {
                    if (rcBeginTable("proc_table", cols, dropped ? 1 : visCount,
                                     .cellPadding = RC_VAL(0))) {
                        rcVirtualList(row, "proc_table", n, rowH) {
                            proc_row(mem, s, &v->host.proc[shown[row.index]], shown[row.index],
                                     vis, visCount);
                        }
                        rcEndTable();
                    }
                    rcScrollbar("proc_table");
                }
            }
        }
        rcRow(.gap = 6, .px = 8, .align = "cl", .w = "grow", .hType = RC_PX(34)) {
            if (paged) {
                rcText(rcFormat(mem, "%d - %d of %d %s", n ? from + 1 : 0, to, n,
                                n == 1 ? "row" : "rows"), .color = s.textMuted, .size = 12);
                rcSpacer() {}
                if (icon_button("pg_prev", rcIconChevronLeft, s.text, "Previous page",
                                st->page > 0))
                    st->page--;
                rcText(rcFormat(mem, "Page %d of %d", st->page + 1, pages), .color = s.text,
                       .size = 12);
                if (icon_button("pg_next", rcIconChevronRight, s.text, "Next page",
                                st->page + 1 < pages))
                    st->page++;
            } else {
                rcText(rcFormat(mem, "%d %s", n, n == 1 ? "row" : "rows"), .color = s.textMuted,
                       .size = 12);
            }
        }
    }
}

/* The three detachable panels behind one name, so two surfaces cannot drift. */
static inline void panel_body(AppState *st, Panel p, RC_Arena *mem, const char *w, const char *h) {
    switch (p) {
    case PANEL_CHART: cpu_panel(st, mem, w, h);     break;
    case PANEL_CORES: bar_gauge(st, mem, w, h);     break;
    case PANEL_TABLE: process_table(st, mem, w, h); break;
    case PANEL_COUNT:                               break;
    }
}

#endif /* APP_PANELS_H */
