/*
================================================================================
    app.h - the whole UI: state, and the layout that renders it
================================================================================
    ONE header, no .c beside it, every function `static inline`: main.c is the
    only translation unit, so there is nothing to link, no forward declarations
    and no filename typed twice. It is the JS module model, in C, and it is what
    RayClay itself does - the library ships as one amalgamated header.

    WHERE THINGS GO: state and everything that draws here, the arithmetic behind
    it in view.h, visual choices in theme.h, the rows in catalog.h (the dataset
    seam) and anything OS-specific in platform/platform.h, which is empty.
================================================================================
*/
#ifndef APP_APP_H
#define APP_APP_H

#include "rayclay.h"

#include "icons/rc_icons_chart_column.h"

#include "app/theme.h"
#include "app/catalog.h"
#include "app/view.h"
#include "platform/platform.h"

/* The widths, and the one height, this app changes shape at. Each is compared
   against the space ACTUALLY being laid out, so a desktop window dragged narrow
   takes the same branch a phone does - and the safe-area bands come off before
   any comparison, so a phone's gutters are never handed to a pane that does not
   get them. NOT a scrolling pane: a chart inside a scrolling parent has no
   bounded height to size its plot against. Reflow them; do not scroll them. */
#define EXP_RAIL_W        276.0f  /* the rail's own fixed width                */
#define EXP_GAP            12.0f  /* the one gap this app uses, everywhere     */
#define EXP_ROOT_CHROME    24.0f  /* .p = 12 on the root, both sides           */
#define EXP_CHART_PAIR_W  612.0f  /* 300 growing chart + 12 + 300 fixed        */
#define EXP_TABLE_PAIR_W  966.0f  /* 634 table + 12 + 320 scatter              */
#define EXP_TABLE_FULL_W  634.0f  /* 460 fixed columns + 160 legible name + 14 */
/* Where the query band can carry the selection's three statistics beside the
   count and the sort line. Below it they go back into the rail's composition
   card, so the numbers are always somewhere - never nowhere, never twice. */
#define EXP_QUERY_STATS_W 800.0f
#define EXP_CHARTS_STACK_H 814.0f /* 3 x 160 charts + 2 x 12 gaps + 310 shell   */
#define EXP_CHART_TRIO_W  702.0f  /* 3 x 226 charts side by side + 2 x 12 gaps  */
/* Where the rail fits beside the content: rail, gap, narrowest chart pair,
   root padding. Derived, so moving any one of them moves the threshold. */
#define EXP_RAIL_BESIDE_W (EXP_RAIL_W + EXP_GAP + EXP_CHART_PAIR_W \
                           + EXP_ROOT_CHROME)
/* The content column's fixed vertical furniture above and below the table. */
#define EXP_CONTENT_SHELL_H 418.0f
#define EXP_SCATTER_H     260.0f  /* the stacked scatter's roomy slot           */
#define EXP_SCATTER_MIN_H 170.0f  /* the shortest scatter whose axes still read */
#define EXP_SCATTER_SHARE   0.4f  /* of the stacked row; the table keeps the rest */

typedef struct Column {
    const char *label;
    const char *width;
    const char *sortId;    /* a literal: an id must outlive the frame. It
                              names the key's ONE sort control this frame -
                              the heading in the wide column set, the chip
                              in the narrow one, never both.                */
    bool        numeric;   /* right-align, as every table of numbers does    */
    bool        compact;   /* keeps its own column in the narrow column set  */
} Column;

/* ONE table drives the header row, the sort keys and the cells, so a column
   cannot drift out of alignment with its own heading. Five of the six are FIXED
   at 460 px before the planet name has a single pixel - wider than a phone, so
   the narrow arm FOLDS three of them into a second line of the name cell.
   Their headings fold with them, which is why that arm draws six sort chips:
   a heading that is not on screen cannot be the only way to sort by it. */
static const Column COLUMN[K_COUNT] = {
    { "Planet",        "grow",  "h_name",   false, true  },
    { "Method",        "88px",  "h_method", false, false },
    { "Year",          "64px",  "h_year",   true,  true  },
    { "Radius",        "80px",  "h_radius", true,  true  },
    { "Period (d)",    "100px", "h_period", true,  false },
    { "Distance (pc)", "128px", "h_dist",   true,  false }
};

/* Sort chips per row in the narrow arm. Fixed rows of equal "grow" cells
   rather than a wrap, because this is a GRID by intent. */
enum { SORT_CHIP_COLS = 3 };

/* One marker slot per chart: the same planet is shown on all three at once,
   so the linked selection needs one point each, not one point. */
enum { PICK_YEAR = 0, PICK_RADIUS, PICK_SCATTER, PICK_COUNT };

typedef struct AppState {
    Catalog cat;
    View    view;
    Filters filter;
    SortKey sortKey;
    bool    sortDesc;
    int     selected;         /* catalogue index, or -1                      */
    bool    refilter;         /* a filter control moved this frame           */
    bool    resort;           /* a header was clicked this frame             */
    bool    revealRow;        /* a chart picked a planet; scroll the table to it */
    int     pane;             /* narrow layouts only: 0 filters 1 charts 2 table */

    /* The one-point series behind each chart's marker. In the state, never in
       the function that draws them: rcChart BORROWS the arrays it is handed
       until the frame is drawn, so a local draws as nothing at all. */
    float   pickX[PICK_COUNT];
    float   pickY[PICK_COUNT];
} AppState;

enum { PANE_FILTERS = 0, PANE_CHARTS = 1, PANE_TABLE = 2, PANE_COUNT = 3 };
static const char *const PANE_LABEL[PANE_COUNT] = { "Filters", "Charts", "Table" };
static const char *const PANE_ID[PANE_COUNT]    = { "pane0", "pane1", "pane2" };

/** A quantity at a sensible number of significant figures. Distance runs from
    1.3 pc to nine thousand and period from half a day to three centuries, so no
    single printf specifier serves either column: a column of numbers needs a
    rule, not a format string. */
static inline RC_String fmt_sig(RC_Arena *mem, float v)
{
    if (v < 10.0f)
        return rcFormat(mem, "%.2f", (double)v);
    if (v < 100.0f)
        return rcFormat(mem, "%.1f", (double)v);
    return rcFormat(mem, "%.0f", (double)v);
}

/** The one-word size class a radius puts a planet in, for a reader who does
    not think in Earth radii. */
static inline const char *size_class(float radius)
{
    if (radius < 1.25f)
        return "Earth-sized";
    if (radius < 2.0f)
        return "Super-Earth";
    if (radius < 4.0f)
        return "Sub-Neptune";
    if (radius < 8.0f)
        return "Neptune-like";
    return "Gas giant";
}

/** The app's palette, installed once from main.c.
 *
 *  BUILT FROM rcStyleDark() RATHER THAN FROM {0}: a zero RC_Style is a black
 *  app with square corners, because every widget derives its corners from
 *  .radius. Start from a preset and override what the app has an opinion about.
 *  Four plotting hues, because the catalogue has FOUR detection methods.
 */
static inline RC_Style explorer_style(void)
{
    RC_Style s = rcStyleDark();

    s.background   = rcHex(0x0B1320);   /* midnight, the canvas              */
    s.surface      = rcHex(0x121C2E);   /* cards on it                       */
    s.surfaceAlt   = rcHex(0x1B2740);   /* inset: header row, tab strip      */
    s.chrome       = rcHex(0x0E1626);
    s.text         = rcHex(0xE8EEF7);
    s.textMuted    = rcHex(0x8FA3BF);
    s.primary      = rcHex(0x2DD4BF);   /* teal                              */
    s.primaryHover = rcHex(0x5EEAD4);
    s.success      = rcHex(0xA78BFA);   /* violet                            */
    s.successHover = rcHex(0xC4B5FD);
    s.warning      = rcHex(0xFBBF24);   /* amber, the fourth method          */
    s.warningHover = rcHex(0xFCD34D);
    s.danger       = rcHex(0xFB7185);   /* coral                             */
    s.dangerHover  = rcHex(0xFDA4AF);
    s.border       = rcHex(0x24324D);
    return s;
}

/* The linked selection's own accent. NOT a style field: "you picked this" is
   not a role the library draws anything with, and it has to stay clear of all
   four method hues at once, or the marker reads as a fifth method. A #define,
   never a file-scope static const: see theme.h. */
#define EXP_PICK rcRgb(0xF1, 0xF5, 0xF9)

/** A distinct colour per detection method, so the same method reads the same
    everywhere. Derived from the theme, so it follows a theme swap. */
static inline RC_Color method_color(const RC_Style *s, int method)
{
    switch (method) {
    case CAT_TRANSIT:   return s->primary;
    case CAT_RADIAL:    return s->success;
    case CAT_MICROLENS: return s->warning;
    default:            return s->danger;
    }
}

static inline void panel_heading(const char *text)
{
    rcTextC(text, .font = F_MICRO, .color = rcGetStyle().textMuted);
}

/** The rebuild point: a control is polled ABOVE the panels that read what it
 *  changes, and the view is rebuilt in between. Clearing the flag is what stops
 *  the second call on a frame redoing the first one's work. */
static inline bool apply_filters(AppState *st)
{
    if (!st->refilter)
        return false;
    view_rebuild(&st->view, &st->cat, &st->filter, st->sortKey, st->sortDesc);
    /* A different set of rows need not contain the planet picked out of the
       old one. */
    st->selected = -1;
    st->refilter = false;
    return true;
}

/* One applied-filter chip. A chip exists only for a control that is AWAY from
   its default, so "no chips" and "the whole catalogue" say the same thing. */
enum { CHIP_METHOD = 0, CHIP_SINCE, CHIP_WITHIN, CHIP_RADIUS, CHIP_KINDS };

typedef struct Chip {
    const char *id;    /* a literal, one per kind: an id must outlive the frame */
    RC_String   text;
    RC_Color    dot;   /* alpha 0 = no swatch                                   */
    int         kind;
} Chip;

/** Build the chip list. Data only: it has to exist before anything is drawn,
    because the same chips are dealt into one row on a desktop and two on a
    phone. */
static inline int chips_build(RC_Arena *mem, const AppState *st, Chip *out)
{
    const Filters *f = &st->filter;
    RC_Style       s = rcGetStyle();
    int n = 0, i, on = 0, first = 0;

    for (i = 0; i < CAT_METHODS; i++) {
        if (f->method[i]) {
            if (!on)
                first = i;
            on++;
        }
    }
    /* The methods are ONE chip, not one each: all four are the default, so a
       chip per method would put three on screen to say one was switched off. */
    if (on != CAT_METHODS) {
        out[n].id   = "q_method";
        out[n].dot  = on == 1 ? method_color(&s, first) : rcAlpha(s.text, 0);
        out[n].text = on == 0 ? rcStringFromCStr("No method")
                    : on == 1 ? rcStringFromCStr(CAT_METHOD_NAME[first])
                    : rcFormat(mem, "%s +%d", CAT_METHOD_NAME[first], on - 1);
        out[n].kind = CHIP_METHOD;
        n++;
    }
    if (f->sinceYear > (float)CAT_YEAR_FIRST) {
        out[n].id   = "q_since";
        out[n].dot  = rcAlpha(s.text, 0);
        out[n].text = rcFormat(mem, "Since %.0f", (double)f->sinceYear);
        out[n].kind = CHIP_SINCE;
        n++;
    }
    /* The slider's top stop IS the farthest row in the catalogue, so this
       compares against the data rather than against a constant. */
    if (f->maxDistance < st->view.distanceMax) {
        out[n].id   = "q_within";
        out[n].dot  = rcAlpha(s.text, 0);
        out[n].text = rcFormat(mem, "Within %.0f pc", (double)f->maxDistance);
        out[n].kind = CHIP_WITHIN;
        n++;
    }
    if (f->minRadius > 0.0f || f->maxRadius < (float)HIST_MAX_R) {
        out[n].id   = "q_radius";
        out[n].dot  = rcAlpha(s.text, 0);
        out[n].text = rcFormat(mem, "Radius %.1f-%.1f Earth",
                                (double)f->minRadius, (double)f->maxRadius);
        out[n].kind = CHIP_RADIUS;
        n++;
    }
    return n;
}

static inline void chip_clear(AppState *st, int kind)
{
    int i;

    switch (kind) {
    case CHIP_METHOD:
        for (i = 0; i < CAT_METHODS; i++)
            st->filter.method[i] = true;
        break;
    case CHIP_SINCE:
        st->filter.sinceYear = (float)CAT_YEAR_FIRST;
        break;
    case CHIP_WITHIN:
        st->filter.maxDistance = st->view.distanceMax;
        break;
    default:
        st->filter.minRadius = 0.0f;
        st->filter.maxRadius = (float)HIST_MAX_R;
        break;
    }
    st->refilter = true;
}

/** One small statistic in the query band: caption over value. */
static inline void stat_cell(const char *caption, RC_String value, RC_Color tone)
{
    RC_Style s = rcGetStyle();

    rcColumn(.gap = 1) {
        rcTextC(caption, .font = F_MICRO, .color = s.textMuted);
        rcText(value, .font = F_HEAD, .color = tone);
    }
}

/** The query, and under it its result.
 *
 *  THE ORDER OF THE TWO ROWS IS WHAT MAKES THE NUMBER HONEST: chips polled
 *  first, view rebuilt between the rows, count printed last, so a chip taken
 *  off on this frame is out of the total on this frame. @p wide is the
 *  side-by-side arm, which rides the sort line on the result row where a phone
 *  gives it one of its own; @p stats is whether that row is also wide enough to
 *  carry the selection's three summary statistics.
 */
static inline void query_card(RC_App *app, AppState *st, bool wide, bool stats)
{
    RC_Arena *mem  = rcAppArena(app);
    RC_Style  s    = rcGetStyle();
    Chip      chip[CHIP_KINDS];
    int       n    = chips_build(mem, st, chip);
    RC_String sorted = rcFormat(mem, "sorted by %s, %s",
                                 COLUMN[st->sortKey].label,
                                 st->sortDesc ? "descending" : "ascending");
    int       i;

    rcColumn(.id = "p_query", .bg = s.surface, .gap = 8, .p = 12,
             .borderRadius = "all-lg",
             .border = { .color = s.border, .width = "1px" }, .w = "grow") {
        /* ONE WRAPPING ROW, the CSS way: heading, chips and the Clear action
           all in the flow, breaking onto further lines on a narrow card. A
           wrapping container has ONE gap - the 6 between chips is the 6
           between lines - so `gap-x-*` / `gap-y-*` are refused here. */
        rcRow(.gap = 6, .align = "cl", .w = "grow", .className = "flex-wrap") {
            panel_heading("QUERY");
            /* Height is all a chip the width of its own label can spend on
               being hittable: 19 px under a mouse, 43 under a finger. */
            uint16_t removePad = rcPointerIsCoarse() ? 16 : 4;

            for (i = 0; i < n; i++) {
                /* Polling rcClicked below is what makes this a button, and
                   what gives it the pointer cursor: no rcSetCursor needed. */
                rcRow(.id = chip[i].id,
                      .bg = rcAlpha(s.border,
                                     rcIsHovered(chip[i].id) ? 200 : 110),
                      .gap = 5, .px = 8, .py = removePad, .align = "cl",
                      .borderRadius = "all-full",
                      .tooltip = "Remove this filter") {
                    if (chip[i].dot.a > 0.0f) {
                        rcBox(.bg = chip[i].dot, .borderRadius = "all-full",
                              .wType = RC_PX(7), .hType = RC_PX(7)) {}
                    }
                    rcText(chip[i].text, .font = F_MICRO, .color = s.text);
                    /* An ASCII x, not a multiplication sign or a glyph
                       from an icon font: the bundled face is Latin-1. */
                    rcTextL("x", .font = F_MICRO, .color = s.textMuted);
                }
                if (rcClicked(chip[i].id))
                    chip_clear(st, chip[i].kind);
            }
            if (!n) {
                rcTextL("No filters \xc2\xb7 the whole catalogue",
                         .font = F_MICRO, .color = s.textMuted);
            }
            if (n && rcButton("q_clear", "Clear all", RC_BTN_GHOST)) {
                filters_reset(&st->filter, st->view.distanceMax);
                st->refilter = true;
            }
        }

        apply_filters(st);

        rcRow(.gap = 8, .align = "bl", .w = "grow") {
            /* The empty result is a STATE, not an absence: a red zero says the
               query ran and excluded everything. */
            rcText(rcFormat(mem, "%d", st->view.count), .font = F_STAT,
                    .color = st->view.count ? s.primary : s.danger);
            rcText(rcFormat(mem, "of %d planets match", CAT_COUNT),
                    .font = F_SMALL, .color = s.textMuted);
            if (wide) {
                /* The widest strip on the screen carries what a reader of a
                   filtered catalogue asks next. Where it is not wide enough,
                   these three are in the rail's composition card instead. */
                rcBox(.w = "grow") {}
                if (stats && st->view.count) {
                    int top = 0;

                    for (i = 1; i < CAT_METHODS; i++) {
                        if (st->view.byMethod[i] > st->view.byMethod[top])
                            top = i;
                    }
                    rcRow(.gap = 28, .align = "bl") {
                        stat_cell("MEAN RADIUS",
                                   rcFormat(mem, "%.2f Earth",
                                             (double)st->view.meanRadius),
                                   s.text);
                        stat_cell("MEAN DISTANCE",
                                   rcFormat(mem, "%.0f pc",
                                             (double)st->view.meanDistance),
                                   s.text);
                        stat_cell("MOSTLY FOUND BY",
                                   rcFormat(mem, "%s %d%%",
                                             CAT_METHOD_ABBR[top],
                                             (int)(100.0f *
                                               (float)st->view.byMethod[top] /
                                               (float)st->view.count + 0.5f)),
                                   method_color(&s, top));
                    }
                }
                rcBox(.w = "grow") {}
                rcText(sorted, .font = F_MICRO, .color = s.textMuted);
            }
        }
        if (!wide)
            rcText(sorted, .font = F_MICRO, .color = s.textMuted);
    }
}

/** A labelled slider: caption and live value on one line, the track under it.
    Returns true on the frames the value changes. */
static inline bool slider_row(RC_App *app, const char *id, const char *label,
                       const char *fmt, float *value, float min, float max)
{
    RC_Arena *mem = rcAppArena(app);
    RC_Style  s   = rcGetStyle();
    bool      moved;

    rcRow(.align = "cl", .w = "grow") {
        rcTextC(label, .font = F_SMALL, .color = s.textMuted);
        rcBox(.w = "grow") {}
        rcText(rcFormat(mem, fmt, (double)*value), .font = F_SMALL,
                .color = s.text);
    }
    moved = rcSlider(id, value, min, max);
    return moved;
}

static inline void filter_card(RC_App *app, AppState *st)
{
    RC_Arena *mem = rcAppArena(app);
    RC_Style  s   = rcGetStyle();
    int       i;

    rcColumn(.id = "p_filters", .bg = s.surface, .gap = 10, .p = 14,
             .borderRadius = "all-lg", .border = { .color = s.border,
                                                    .width = "1px" },
             .w = "grow") {
        /* No Reset here: clearing every filter at once is "Clear all" in the
           query card, beside the chips that say what there is to clear. */
        panel_heading("FILTERS");

        rcColumn(.gap = 2, .w = "grow") {
            for (i = 0; i < CAT_METHODS; i++) {
                rcRow(.gap = 8, .align = "cl", .w = "grow") {
                    rcBox(.bg = method_color(&s, i), .borderRadius = "all-sm",
                          .wType = RC_PX(8), .hType = RC_PX(8)) {}
                    if (rcCheckbox(rcFormat(mem, "f_m%d", i).chars,
                                    CAT_METHOD_NAME[i], &st->filter.method[i]))
                        st->refilter = true;
                    rcBox(.w = "grow") {}
                    rcText(rcFormat(mem, "%d", st->view.byMethod[i]),
                            .font = F_MICRO, .color = s.textMuted);
                }
            }
        }

        if (slider_row(app, "f_year", "Discovered since", "%.0f",
                        &st->filter.sinceYear, (float)CAT_YEAR_FIRST,
                        (float)CAT_YEAR_LAST))
            st->refilter = true;

        /* The top stop IS the farthest row in the catalogue, so the slider has
           a genuine "no limit" end that excludes nothing. */
        if (slider_row(app, "f_dist", "Within (pc)", "%.0f",
                        &st->filter.maxDistance, 10.0f, st->view.distanceMax))
            st->refilter = true;

        if (slider_row(app, "f_rmin", "Radius at least", "%.1f",
                        &st->filter.minRadius, 0.0f, (float)HIST_MAX_R))
            st->refilter = true;
        if (slider_row(app, "f_rmax", "Radius at most", "%.1f",
                        &st->filter.maxRadius, 0.0f, (float)HIST_MAX_R))
            st->refilter = true;
    }
}

/** What the selection is MADE OF. @p statsInBand says the query band at the top
    of the screen is carrying the means, so this card does not repeat them. */
static inline void summary_card(RC_App *app, AppState *st, bool statsInBand)
{
    RC_Arena *mem = rcAppArena(app);
    RC_Style  s   = rcGetStyle();
    const View *v = &st->view;
    int       i;

    rcColumn(.id = "p_summary", .bg = s.surface, .gap = 8, .p = 14,
             .borderRadius = "all-lg", .border = { .color = s.border,
                                                    .width = "1px" },
             .w = "grow") {
        panel_heading("COMPOSITION");

        /* ONE bar in four segments, not four bars: the shares are parts of a
           single whole. .overflow keeps the last segment inside the rounded
           ends when the shares round up past 100. */
        rcRow(.bg = s.surfaceAlt, .gap = 0, .overflow = "hidden",
              .borderRadius = "all-sm", .w = "grow", .hType = RC_PX(8)) {
            for (i = 0; i < CAT_METHODS; i++) {
                float pct = v->count ? 100.0f * (float)v->byMethod[i] /
                                       (float)v->count
                                     : 0.0f;

                rcBox(.bg = method_color(&s, i), .h = "grow",
                      .wType = RC_PCT(pct)) {}
            }
        }
        /* The stack's key: a bar whose parts cannot be named is decoration. */
        rcRow(.gap = 10, .align = "cl", .w = "grow") {
            for (i = 0; i < CAT_METHODS; i++) {
                int pct = v->count ? (int)(100.0f * (float)v->byMethod[i] /
                                            (float)v->count + 0.5f)
                                   : 0;

                rcRow(.gap = 4, .align = "cl") {
                    rcBox(.bg = method_color(&s, i), .borderRadius = "all-full",
                          .wType = RC_PX(6), .hType = RC_PX(6)) {}
                    rcText(rcFormat(mem, "%d%%", pct), .font = F_MICRO,
                            .color = s.textMuted);
                }
            }
        }

        if (!statsInBand) {
            rcRow(.align = "cl", .w = "grow") {
                rcTextL("Mean radius", .font = F_SMALL, .color = s.textMuted);
                rcBox(.w = "grow") {}
                rcText(rcFormat(mem, "%.2f Earth", (double)v->meanRadius),
                        .font = F_SMALL, .color = s.text);
            }
            rcRow(.align = "cl", .w = "grow") {
                rcTextL("Mean distance", .font = F_SMALL, .color = s.textMuted);
                rcBox(.w = "grow") {}
                rcText(rcFormat(mem, "%.0f pc", (double)v->meanDistance),
                        .font = F_SMALL, .color = s.text);
            }
        }
    }
}

/** One label/value line in the detail card. The unit is its own muted run, so
    the numbers stay scannable down the column and fmt_sig can own the precision
    without knowing the unit. */
static inline void detail_row(const char *label, RC_String value, const char *unit)
{
    RC_Style s = rcGetStyle();

    rcRow(.align = "cl", .w = "grow") {
        rcTextC(label, .font = F_SMALL, .color = s.textMuted);
        rcBox(.w = "grow") {}
        rcRow(.gap = 3, .align = "bl") {
            rcText(value, .font = F_SMALL, .color = s.text);
            if (unit)
                rcTextC(unit, .font = F_MICRO, .color = s.textMuted);
        }
    }
}

static inline void detail_card(RC_App *app, AppState *st)
{
    RC_Arena *mem = rcAppArena(app);
    RC_Style  s   = rcGetStyle();
    /* The nearest planet in the selection, so the card is never empty. */
    int       idx = st->selected >= 0 ? st->selected : st->view.nearest;

    rcColumn(.id = "p_detail", .bg = s.surface, .gap = 8, .p = 14,
             .borderRadius = "all-lg", .border = { .color = s.border, .width = "1px" },
             .w = "grow", .h = "grow") {
        panel_heading(st->selected >= 0 ? "SELECTED" : "CLOSEST IN SELECTION");

        if (idx < 0) {
            rcTextL("No planet matches these filters.", .font = F_SMALL,
                     .color = s.textMuted);
        } else {
            const CatPlanet *p = &st->cat.row[idx];

            rcText(rcFormat(mem, "%s-%d %s", CAT_SURVEY[p->survey],
                              (int)p->host, catalog_letter(p)),
                    .font = F_HEAD, .color = s.text);
            /* The subtitle carries the method as well as the size class, so
               every row below it is a number. */
            rcRow(.gap = 6, .align = "cl", .w = "grow") {
                rcBox(.bg = method_color(&s, p->method), .borderRadius = "all-sm",
                      .wType = RC_PX(8), .hType = RC_PX(8)) {}
                rcTextC(size_class(p->radius), .font = F_SMALL,
                         .color = s.textMuted);
                rcTextL("\xc2\xb7", .font = F_SMALL, .color = s.textMuted);
                rcTextC(CAT_METHOD_NAME[p->method], .font = F_SMALL,
                         .color = method_color(&s, p->method));
            }
            rcBox(.bg = s.border, .w = "grow", .hType = RC_PX(1)) {}

            detail_row("Discovered", rcFormat(mem, "%d", p->year), NULL);
            detail_row("Radius / mass",
                        rcFormat(mem, "%s / %s", fmt_sig(mem, p->radius).chars,
                                  fmt_sig(mem, p->mass).chars), "Earth");
            detail_row("Orbital period", fmt_sig(mem, p->period), "days");
            detail_row("Distance", fmt_sig(mem, p->distance), "pc");
            detail_row("Equilibrium temp", rcFormat(mem, "%d", p->teq), "K");
        }
    }
}

/** The linked-selection marker: ONE point, in the accent no method uses, on
    whichever chart is asking for it. Half of the link between table and plots;
    scatter_pick is the other half. Returns the series count added. */
static inline int pick_series(AppState *st, RC_Series *dst, int slot,
                               float x, float y)
{
    st->pickX[slot] = x;
    st->pickY[slot] = y;

    dst->y         = &st->pickY[slot];
    dst->x         = &st->pickX[slot];
    dst->count     = 1;
    dst->kind      = RC_SERIES_SCATTER;
    dst->color     = EXP_PICK;
    dst->label     = "Selected planet";
    dst->thickness = 5.0f;   /* marker RADIUS: bigger than the plot's own dots */
    return 1;
}

static inline void chart_years(AppState *st)
{
    RC_Style s = rcGetStyle();
    View    *v = &st->view;
    int      n = 2;

    /* Drawn first, so it sits behind: the whole catalogue as a band, the
       selection as bars in front. Without that context a selection that removed
       90% of the data still looks like the whole story - so the band is
       composited well clear of the panel, not whispered onto it. */
    RC_Series series[3] = {
        { .y = v->yearAll, .x = v->yearX, .count = CAT_YEARS,
          .kind = RC_SERIES_AREA, .color = rcAlpha(s.textMuted, 150),
          .label = "All discoveries" },
        { .y = v->yearSel, .x = v->yearX, .count = CAT_YEARS,
          .kind = RC_SERIES_BAR, .color = s.primary,
          .label = "Current selection" }
    };

    /* On TOP of its own year's bar: the y axis counts discoveries and a planet
       has no count, so which bar it is in is all the chart can say. */
    if (st->selected >= 0) {
        const CatPlanet *p = &st->cat.row[st->selected];

        n += pick_series(st, &series[n], PICK_YEAR, (float)p->year,
                          v->yearSel[p->year - CAT_YEAR_FIRST]);
    }

    rcChart("ch_years", series, n, RC_LIT(RC_ChartOptions){
        .x = { .min = (float)CAT_YEAR_FIRST, .max = (float)CAT_YEAR_LAST,
               .ticks = 8 },
        .y = { .ticks = 4, .grid = true },
        .legend      = true,
        .fontSize    = 11,
        .tooltip     = RC_CHART_TOOLTIP_NEAREST,
        .tooltipPlace = RC_TOOLTIP_PLACE_CORNER,
        .hoverGuide  = true,
        .hoverMarkers = true
    });
}

static inline void chart_radius(AppState *st)
{
    RC_Style  s = rcGetStyle();
    View     *v = &st->view;
    int       n = 1;
    float     tallest = 0.0f;
    int       bin;
    RC_Series series[2] = {
        { .y = st->view.histSel, .x = st->view.histX, .count = HIST_BINS,
          .kind = RC_SERIES_BAR, .color = s.success, .label = "Planets" }
    };

    /* An empty histogram has no range to auto-fit, so the axis opens out
       symmetrically and a count axis goes negative. The EMPTY case names its
       own 0..4 instead; populated ones auto-fit, which is both fields zero. */
    for (bin = 0; bin < HIST_BINS; bin++) {
        if (v->histSel[bin] > tallest)
            tallest = v->histSel[bin];
    }

    /* The histogram's own binning decides which bar a radius is in, so the
       marker repeats it or it lands beside the bar it belongs to. */
    if (st->selected >= 0) {
        float radius = st->cat.row[st->selected].radius;

        bin = (int)(radius * (float)HIST_BINS / (float)HIST_MAX_R);
        if (bin >= HIST_BINS)
            bin = HIST_BINS - 1;
        n += pick_series(st, &series[n], PICK_RADIUS, v->histX[bin],
                          v->histSel[bin]);
    }

    rcChart("ch_radius", series, n, RC_LIT(RC_ChartOptions){
        .x = { .min = 0.0f, .max = (float)HIST_MAX_R, .ticks = 5 },
        .y = { .min = 0.0f, .max = tallest > 0.0f ? 0.0f : 4.0f,
               .ticks = 4, .grid = true },
        .fontSize = 11,
        .tooltip  = RC_CHART_TOOLTIP_NEAREST,
        .tooltipPlace = RC_TOOLTIP_PLACE_CORNER
    });
}

/** Clicking a dot selects its planet - the other half of the link.
 *
 *  A CHART IS NOT A WIDGET WITH A CALLBACK, and does not need to be: three
 *  public reads do it. rcChartPlotRect says where the DATA is, not where the
 *  chart is - the axis gutters and legend row are the chart's. rcPointer says
 *  where the cursor is in the same space, and rcClicked says a click landed
 *  (and gives the plot its pointer cursor, so no rcSetCursor here).
 *
 *  MAP THE POINTS FORWARD INTO PIXELS, never the pointer back into data:
 *  searching in data space measures distance in a mixture of days and Earth
 *  radii, so a click snaps to whichever axis is numerically smaller.
 */
static inline void scatter_pick(AppState *st)
{
    RC_Box   plot = rcChartPlotRect("ch_scatter");
    RC_Vec2  p    = rcPointer();
    int      best = -1;
    float    bestD = 0.0f;
    int      i;

    if (!rcClicked("ch_scatter"))
        return;
    /* BOTH halves: .found only means the id exists, and an element's first
       frame is found with an all-zero rect. */
    if (!plot.found || plot.width <= 0.0f || plot.height <= 0.0f)
        return;

    for (i = 0; i < st->view.scatterCount; i++) {
        float dx = plot.x + st->view.scatterX[i] / (float)SCATTER_P * plot.width
                 - p.x;
        float dy = plot.y + plot.height
                 - st->view.scatterY[i] / (float)SCATTER_R * plot.height - p.y;
        float d  = dx * dx + dy * dy;

        if (best < 0 || d < bestD) {
            best  = i;
            bestD = d;
        }
    }
    /* A grab radius, so a click on empty sky is a miss rather than whichever
       dot was least far away. Squared, to spare the loop a square root. */
    if (best >= 0 && bestD <= 18.0f * 18.0f) {
        st->selected  = st->view.scatterIdx[best];
        st->revealRow = true;
    }
}

static inline void chart_scatter(AppState *st)
{
    RC_Style  s = rcGetStyle();
    int       n = 1;
    RC_Series series[2] = {
        { .y = st->view.scatterY, .x = st->view.scatterX,
          .count = st->view.scatterCount, .kind = RC_SERIES_SCATTER,
          .color = rcAlpha(s.danger, 170),
          .thickness = 2.0f } /* SCATTER reads .thickness as the marker radius */
    };

    scatter_pick(st);

    /* Only when the planet is inside the pinned window: the plot does not clamp
       its points, so a marker for a 300-day orbit would be a lie on the axis. */
    if (st->selected >= 0) {
        const CatPlanet *p = &st->cat.row[st->selected];

        if (p->period <= (float)SCATTER_P && p->radius <= (float)SCATTER_R)
            n += pick_series(st, &series[n], PICK_SCATTER, p->period, p->radius);
    }

    rcChart("ch_scatter", series, n, RC_LIT(RC_ChartOptions){
        .x = { .min = 0.0f, .max = (float)SCATTER_P, .ticks = 5 },
        .y = { .min = 0.0f, .max = (float)SCATTER_R, .ticks = 5, .grid = true },
        .fontSize = 11,
        .tooltip  = RC_CHART_TOOLTIP_NEAREST,
        .tooltipPlace = RC_TOOLTIP_PLACE_CORNER
    });
}

/** A titled frame around a chart. CHARTS GROW BOTH WAYS and have no size of
    their own, so something must bound them - a chart in a fit-sized container
    draws nothing at all. */
static inline void chart_card(const char *id, const char *title, const char *note,
                       const char *width, const char *height,
                       void (*body)(AppState *), AppState *st)
{
    RC_Style s = rcGetStyle();

    rcColumn(.id = id, .bg = s.surface, .gap = 6, .p = 12, .borderRadius = "all-lg",
             .border = { .color = s.border, .width = "1px" }, .w = width, .h = height) {
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            rcTextC(title, .font = F_MICRO, .color = s.textMuted);
            rcBox(.w = "grow") {}
            rcTextC(note, .font = F_MICRO, .color = s.textMuted);
        }
        rcBox(.w = "grow", .h = "grow") {
            body(st);
        }
    }
}

/** One tap on a sort control. Clicking the active key flips it; a new key
    starts ascending, except where "most" is the interesting end. */
static inline void sort_pick(AppState *st, int key)
{
    if (st->sortKey == (SortKey)key) {
        st->sortDesc = !st->sortDesc;
    } else {
        st->sortKey  = (SortKey)key;
        st->sortDesc = (key == K_YEAR || key == K_RADIUS);
    }
    st->resort = true;
}

/** The sortable header row. Hand-rolled rather than rcBeginTable's header,
    because table header cells are TEXT, not elements you can give an id to, so
    there is nothing to hit-test.

    Direction is an accent colour plus a word, not an arrow glyph: the bundled
    face is Latin-1, so a triangle draws as a missing-glyph box. Check your text
    against the font, not against your editor. */
static inline void table_header(AppState *st, bool wideCols)
{
    RC_Style s = rcGetStyle();
    int      i;

    /* The right inset matches the body's, so the columns stay registered with
       their headings once the scrollbar takes the last 12 px of the rows. */
    rcRow(.bg = s.surfaceAlt, .gap = 0, .pr = 12, .borderRadius = "t-lg", .w = "grow") {
        for (i = 0; i < K_COUNT; i++) {
            bool active = (st->sortKey == (SortKey)i);

            if (!wideCols && !COLUMN[i].compact)
                continue;

            /* The heading is the sort control only while every heading is
               drawn: in the narrow column set the chips above own the ids. */
            rcColumn(.id = wideCols ? COLUMN[i].sortId : NULL, .gap = 4,
                     .px = 8, .py = 7, .w = COLUMN[i].width) {
                rcRow(.gap = 5, .align = "cl", .w = "grow") {
                    /* `.align` is a char[3], not a const char *, so a ternary
                       cannot pick it the way it picks `.w`; a growing spacer in
                       front right-aligns the numeric columns instead. */
                    if (COLUMN[i].numeric)
                        rcBox(.w = "grow") {}
                    rcTextC(COLUMN[i].label, .font = F_MICRO,
                             .color = active ? s.primary : s.textMuted);
                    if (active) {
                        rcTextC(st->sortDesc ? "DESC" : "ASC", .font = F_MICRO,
                                 .color = rcAlpha(s.primary, 200));
                    }
                }
                rcBox(.bg = active ? s.primary : rcAlpha(s.border, 0), .w = "grow",
                      .hType = RC_PX(2)) {}
            }
            if (wideCols && rcClicked(COLUMN[i].sortId))
                sort_pick(st, i);
        }
    }
}

/** "+N columns", at the table's top right, whenever the table draws fewer
    columns than it has.

    THE RULE IT SERVES: a data table must FIT the width it is given, because a
    user who has to scroll one sideways has lost the column they were reading.
    So the narrow arm drops columns - and a drop nobody is told about is
    indistinguishable from a table that never had them. This chip is the
    telling. A label, not a control: nothing polls a click here. */
static inline void columns_chip(RC_App *app)
{
    RC_Arena *mem = rcAppArena(app);
    RC_Style  s   = rcGetStyle();
    int       i, folded = 0;

    for (i = 0; i < K_COUNT; i++) {
        if (!COLUMN[i].compact)
            folded++;
    }

    rcRow(.id = "t_folded", .bg = rcAlpha(s.border, 120), .px = 8, .py = 3,
          .align = "cl", .borderRadius = "all-full",
          .tooltip = "Method, Period and Distance are on each row's second line") {
        rcText(rcFormat(mem, "+%d columns", folded), .font = F_MICRO,
                .color = s.textMuted);
    }
}

/** The narrow arm's sort control: six chips, one per key, so every order stays
    one tap away where only three of the six headings are drawn. */
static inline void sort_chips(RC_App *app, AppState *st)
{
    RC_Style s = rcGetStyle();
    int      row, i;
    /* rcPointerIsCoarse latches on the first real touch and starts true on
       native mobile, so a phone's first frame is already the tall one. */
    uint16_t chipPad = rcPointerIsCoarse() ? 16 : 7;

    rcColumn(.gap = 4, .p = 6, .w = "grow") {
        rcRow(.gap = 6, .align = "cl", .w = "grow") {
            rcTextL("SORT BY", .font = F_MICRO, .color = s.textMuted);
            rcBox(.w = "grow") {}
            columns_chip(app);
        }
        for (row = 0; row < K_COUNT; row += SORT_CHIP_COLS) {
            rcRow(.gap = 4, .w = "grow") {
                for (i = row; i < row + SORT_CHIP_COLS && i < K_COUNT; i++) {
                    bool active = (st->sortKey == (SortKey)i);

                    /* SAME STATES AS THE PANE TABS, so the two strips read as
                       one family: the chosen key is an accent TINT with an
                       accent outline and accent text, never white on a solid
                       accent - white on this teal is 1.9:1, and these chips
                       are the only way to sort anything at this width. */
                    rcBox(.id = COLUMN[i].sortId,
                          .bg = active ? rcAlpha(s.primary, 38)
                                       : rcAlpha(s.border,
                                                  rcIsHovered(COLUMN[i].sortId) ? 90 : 40),
                          .py = chipPad, .align = "cc", .borderRadius = "all-md",
                          .border = { .color = active ? s.primary : RC_TRANSPARENT,
                                      .width = "all-1" },
                          .w = "grow") {
                        rcRow(.gap = 5, .align = "cl") {
                            rcTextC(COLUMN[i].label, .font = F_MICRO,
                                     .color = active ? s.primary : s.textMuted);
                            if (active) {
                                rcTextC(st->sortDesc ? "DESC" : "ASC",
                                         .font = F_MICRO,
                                         .color = rcAlpha(s.primary, 200));
                            }
                        }
                    }
                    if (rcClicked(COLUMN[i].sortId))
                        sort_pick(st, i);
                }
            }
        }
    }
    rcBox(.bg = s.border, .w = "grow", .hType = RC_PX(1)) {}
}

/** The virtualized, selectable table body.
 *
 *  12,000 rows, about twenty declared: rcVirtualList emits the visible window
 *  plus a row of overscan and pads the rest with spacers, so scroll travel,
 *  content height and the wheel all behave as if every row were there. Without
 *  it all 12,000 are laid out each frame, on screen or not.
 *
 *  Rows are plain rcRow elements because the narrow arm changes the row's
 *  SHAPE, not just its widths. For a selectable list that keeps one shape,
 *  rcBeginTable with rcTableRowId is fewer moving parts; prefer it.
 *
 *  Either way, BUILD A ROW ID FROM THE DATA INDEX, NEVER THE SCREEN POSITION.
 *  Under rcVirtualList the screen position IS the scroll window, so an id
 *  derived from it renames every row on every scroll - the hover you were
 *  tracking jumps to a different record, and nothing warns.
 */
static inline void table_body(RC_App *app, AppState *st, bool wideCols)
{
    RC_Arena *mem   = rcAppArena(app);
    RC_Style  s     = rcGetStyle();
    /* ONE number, handed to the virtual list AND to every row: the list paces
       its travel by the height it is told. */
    float     rowH  = (float)(wideCols ? ROW_H : ROW_H_TWO);

    /* THE EMPTY STATE IS A SENTENCE and it names the way back out. It REPLACES
       the scroll container: an empty virtual list has no rows to pace by. */
    if (!st->view.count) {
        rcColumn(.id = "tbl_empty", .gap = 6, .p = 24, .align = "cc",
                 .w = "grow", .h = "grow") {
            rcTextL("No planets match this query.", .font = F_SMALL,
                     .color = s.text);
            rcTextL("Take a chip off the query bar, or Clear all, to widen it.",
                     .font = F_MICRO, .color = s.textMuted);
        }
        return;
    }

    /* An element id must outlive the frame: RayClay keeps the pointer you hand
       it, not a copy. The frame arena is exactly that lifetime. */
    rcColumn(.id = "tbl", .pr = 12, .scroll = "v", .w = "grow", .h = "grow") {
        rcVirtualList(row, "tbl", st->view.count, rowH) {
            const CatPlanet *p = &st->cat.row[st->view.sel[row.index]];
            bool  chosen = (st->view.sel[row.index] == st->selected);
            const char *id = rcFormat(mem, "r%d", row.index).chars;
            RC_Color bg;
            int   i;

            /* The selected row carries the SAME accent as the marker on the
               three charts: one statement about one planet. ONE CHANNEL IS NOT
               ENOUGH - a tint is what a colour-blind reader, a dimmed screen
               and a photographed frame all lose first - so the outline is the
               second. A border draws inside the element's own box, so unlike a
               child it cannot push the cells out of register with their
               headings. Declared in BOTH states, transparent when not
               chosen. */
            bg = chosen        ? rcAlpha(EXP_PICK, 52)
               : rcIsHovered(id) ? rcAlpha(s.border, 90)
               : (row.index & 1) ? rcAlpha(s.border, 26)
                                 : rcAlpha(s.border, 0);

            rcRow(.id = id, .bg = bg,
                  .border = { .color = chosen ? EXP_PICK : RC_TRANSPARENT,
                              .width = "all-1" },
                  .w = "grow", .hType = RC_PX(rowH)) {
                for (i = 0; i < K_COUNT; i++) {
                    RC_Color fg = i == K_NAME ? s.text : s.textMuted;

                    if (!wideCols && !COLUMN[i].compact)
                        continue;

                    rcRow(.px = 8, .align = "cl", .w = COLUMN[i].width, .h = "grow") {
                        if (COLUMN[i].numeric)
                            rcBox(.w = "grow") {}
                        switch (i) {
                        case K_NAME:
                            /* FRAME ARENA, not a local buffer: a stack string
                               is dangling by the time this is drawn - silently,
                               as an empty column. */
                            /* The class runs on from the name rather than
                               being pushed to the far edge of the cell: right
                               aligned it butts against the next heading and
                               reads as a seventh, unlabelled column. */
                            if (wideCols) {
                                rcRow(.gap = 6, .align = "cl") {
                                    rcText(rcFormat(mem, "%s-%d %s",
                                                      CAT_SURVEY[p->survey],
                                                      (int)p->host,
                                                      catalog_letter(p)),
                                            .font = F_SMALL, .color = fg);
                                    rcTextC(size_class(p->radius),
                                             .font = F_MICRO,
                                             .color = rcAlpha(s.textMuted, 150));
                                }
                                break;
                            }
                            /* Narrow: the name line, and under it the three
                               folded values. Nothing is withheld. */
                            rcColumn(.gap = 1, .w = "grow") {
                                rcRow(.gap = 6, .align = "cl", .w = "grow") {
                                    rcText(rcFormat(mem, "%s-%d %s",
                                                      CAT_SURVEY[p->survey],
                                                      (int)p->host,
                                                      catalog_letter(p)),
                                            .font = F_SMALL, .color = fg);
                                    rcTextC(size_class(p->radius),
                                             .font = F_MICRO,
                                             .color = rcAlpha(s.textMuted, 150));
                                }
                                rcRow(.gap = 5, .align = "cl", .w = "grow") {
                                    rcBox(.bg = method_color(&s, p->method),
                                          .borderRadius = "all-sm",
                                          .wType = RC_PX(6), .hType = RC_PX(6)) {}
                                    rcTextC(CAT_METHOD_ABBR[p->method],
                                             .font = F_MICRO,
                                             .color = method_color(&s, p->method));
                                    rcText(rcFormat(mem, "\xc2\xb7 %s d \xc2\xb7 %s pc",
                                                      fmt_sig(mem, p->period).chars,
                                                      fmt_sig(mem, p->distance).chars),
                                            .font = F_MICRO, .color = s.textMuted);
                                }
                            }
                            break;
                        case K_METHOD:
                            rcTextC(CAT_METHOD_ABBR[p->method], .font = F_SMALL,
                                     .color = method_color(&s, p->method));
                            break;
                        case K_YEAR:
                            rcText(rcFormat(mem, "%d", p->year),
                                    .font = F_SMALL, .color = fg);
                            break;
                        case K_RADIUS:
                            rcText(rcFormat(mem, "%.2f", (double)p->radius),
                                    .font = F_SMALL, .color = fg);
                            break;
                        case K_PERIOD:
                            rcText(fmt_sig(mem, p->period), .font = F_SMALL,
                                    .color = fg);
                            break;
                        default:
                            rcText(fmt_sig(mem, p->distance), .font = F_SMALL,
                                    .color = fg);
                            break;
                        }
                    }
                }
            }

            if (rcClicked(id))
                st->selected = st->view.sel[row.index];
        }
    }
    rcScrollbar("tbl");
}

/** THE window's one status line, outside every pane so the app has a single
    footer baseline at every width. It WRAPS rather than switching arrangement:
    a fixed-height band cannot grow, so text that wrapped inside one would draw
    below its own edge. @p plotting is whether the scatter is on screen - its
    counts are not a fact about a window that is not showing it. */
static inline void status_bar(RC_App *app, AppState *st, bool plotting)
{
    RC_Arena *mem = rcAppArena(app);
    RC_Style  s   = rcGetStyle();
    const View *v = &st->view;
    /* The cache, made visible: this moves when a control moves and at no other
       time. Resize, scroll or hover and it stays put. */
    RC_String scanned = rcFormat(mem, "%d rows scanned per rebuild \xc2\xb7 rebuilds "
                                      "so far: %d", CAT_COUNT, v->rebuilds);

    rcRow(.bg = s.surface, .gap = 16, .px = 12, .py = 5, .align = "cl",
          .borderRadius = "all-md", .w = "grow", .className = "flex-wrap") {
        rcTextL("Synthetic catalogue, shaped after the real record \xc2\xb7 "
                 "see catalog.h", .font = F_MICRO, .color = s.textMuted);
        rcText(scanned, .font = F_MICRO, .color = s.textMuted);
        if (plotting) {
            /* A zero-basis spacer grows on the FIRST line, as in CSS: it
               right-aligns these counts while they fit on it. */
            rcBox(.w = "grow") {}
            rcText(rcFormat(mem, "scatter: %d of %d plotted (every %d), %d "
                                  "outside the window",
                             v->scatterCount, v->count, v->scatterStride,
                             v->scatterOutside),
                    .font = F_MICRO, .color = s.textMuted);
        }
    }
}

/* The app's name, one row. ABOVE the pane tabs when the panes take turns -
   inside the Filters pane it would scroll away with the cards. */
static inline void app_title(void)
{
    RC_Style s = rcGetStyle();

    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        rcIconChartColumn(17.0f, s.primary);
        rcTextL("Exoplanet Explorer", .font = F_HEAD, .color = s.text);
    }
}

/* The rail's contents, shared by both arrangements; only the SHELL differs.
   @p withTitle: the side-by-side arm puts the app's name at the top of the
   rail, where the narrow arm has already drawn it over the tabs. */
static inline void rail_cards(RC_App *app, AppState *st, bool withTitle,
                               bool statsInBand)
{
    if (withTitle)
        app_title();
    filter_card(app, st);
    summary_card(app, st, statsInBand);
    detail_card(app, st);
}

static inline void table_pane(RC_App *app, AppState *st, bool wideCols)
{
    RC_Style s = rcGetStyle();

    rcColumn(.id = "p_table", .bg = s.surface, .gap = 0, .borderRadius = "all-lg",
             .border = { .color = s.border, .width = "1px" },
             .w = "grow", .h = "grow") {
        /* Chips only where headings are missing: the wide header row IS the
           sort control. */
        if (!wideCols)
            sort_chips(app, st);
        table_header(st, wideCols);
        /* Re-sorting reorders the SAME selection, so no full rebuild - but it
           has to land before the rows below are declared. */
        if (st->resort)
            view_sort(&st->view, &st->cat, st->sortKey, st->sortDesc);
        table_body(app, st, wideCols);

        /* A link the user cannot see is not a link, so a planet picked in the
           scatter brings its row into view. rcScrollBy acts on the LAST
           laid-out frame, so the scroll lands on the next one. */
        if (st->revealRow && st->selected >= 0 && st->view.count) {
            RC_ScrollInfo info = rcGetScrollInfo("tbl");
            /* THE VIEWPORT HEIGHT COMES FROM THE CONTAINER'S OWN RECT. Both
               reads describe the LAST laid-out frame, and the frame a pane
               opens on reports found with an all-zero rect - so a height of
               zero is what "not settled yet" looks like, and the request waits
               rather than being spent on a clamp that does nothing. */
            RC_Box box  = rcGetElementBox("tbl");
            float  rowH = (float)(wideCols ? ROW_H : ROW_H_TWO);
            int    rank = -1;
            int    i;

            for (i = 0; i < st->view.count; i++) {
                if (st->view.sel[i] == st->selected) {
                    rank = i;
                    break;
                }
            }
            if (info.found && box.found && box.height > 0.0f) {
                /* Centred: the point of landing there is to see what the row
                   sits among. */
                if (rank >= 0) {
                    float want = (float)rank * rowH
                                - (box.height - rowH) * 0.5f;

                    rcScrollBy("tbl", 0.0f, want - info.offsetY);
                }
                st->revealRow = false;
            }
            rcWindowRequestFrame(rcAppMainWindow(app));
        }
    }
}

/* The pane picker a narrow window gets. Stacking three panes needs a scrolling
   root, and a virtualized table cannot live in one without being given an
   explicit height. So the panes take turns, each with the whole window. */
static inline void pane_tabs(AppState *st)
{
    RC_Style s = rcGetStyle();
    int      i;

    /* The only navigation a phone has, sized for the pointer that uses it: a
       30 px strip under a mouse, 44 under a finger. */
    uint16_t tabPad = rcPointerIsCoarse() ? 15 : 8;

    rcRow(.bg = s.surfaceAlt, .gap = 4, .p = 4, .borderRadius = "all-lg",
          .w = "grow") {
        for (i = 0; i < PANE_COUNT; i++) {
            bool on = (st->pane == i);

            /* THE CHOSEN TAB IS A TINT PLUS AN OUTLINE PLUS ACCENT TEXT, not
               white on a solid accent slab: white on this teal measures 1.9:1,
               and the only navigation a narrow window has is the last thing on
               the page that should be hard to read. */
            rcBox(.id = PANE_ID[i],
                  .bg = on ? rcAlpha(s.primary, 38)
                           : rcAlpha(s.border, rcIsHovered(PANE_ID[i]) ? 90 : 0),
                  .py = tabPad, .align = "cc", .borderRadius = "all-md",
                  .border = { .color = on ? s.primary : RC_TRANSPARENT,
                              .width = "all-1" },
                  .w = "grow") {
                rcTextC(PANE_LABEL[i], .font = F_SMALL,
                         .color = on ? s.primary : s.textMuted);
            }
            if (rcClicked(PANE_ID[i]))
                st->pane = i;
        }
    }
}

static inline void layout(RC_App *app, void *userData)
{
    AppState *st = (AppState *)userData;
    RC_Style  s  = rcGetStyle();
    int       before = st->selected;

    st->refilter = false;
    st->resort   = false;

    /* SAFE AREA. A phone draws the window edge to edge, UNDER the status bar and
       UNDER the home indicator, and nothing moves content out of the way for you.
       Ask for the margins and spend them ONCE, at the root, already in layout
       units. They are {0,0,0,0} on a desktop unless RAYCLAY_SAFE_INSETS stands a
       phone's bands in, so this is one code path everywhere.
       ADDED to the existing 12, not replacing it: specificity runs
       pt/pb/pl/pr > px/py > p, so naming a side would drop that side's 12 px. */
    RC_Insets safe = rcViewport().safe;

    /* THE ARRANGEMENT, derived once and read below. One bool per IDEA, never
       one per widget: each asks whether a specific PAIR of panes still fits
       side by side. Both are measured against the space a pane ACTUALLY gets -
       `content` is what is left after the rail, `vw` the viewport less the safe
       bands - or two branches disagree in the band between them. */
    float vw         = rcViewport().width - safe.left - safe.right;
    bool  railBeside = vw >= EXP_RAIL_BESIDE_W;
    float content    = vw - EXP_ROOT_CHROME
                     - (railBeside ? EXP_RAIL_W + EXP_GAP : 0.0f);
    bool  tableRow   = content >= EXP_TABLE_PAIR_W;
    bool  wideCols   = content >= EXP_TABLE_FULL_W;
    bool  bandStats  = railBeside && content >= EXP_QUERY_STATS_W;
    /* Two ideas, two bools: can the stack give each chart a legible height,
       and if not, is there room to lay the three across instead. */
    bool  chartsFit  = rcViewport().height - safe.top - safe.bottom
                     >= EXP_CHARTS_STACK_H;
    bool  chartsRow  = !chartsFit && content >= EXP_CHART_TRIO_W;
    /* THE STACKED PAIR NEEDS HEIGHT AS WELL AS WIDTH. A FIXED-height scatter
       would take its slot out of the row first and hand the table a header with
       no rows under it, so the row is SHARED: the scatter asks for
       EXP_SCATTER_SHARE, the table keeps the rest. Below the shortest scatter
       that can still carry two labelled axes it stands down rather than shrink
       into a sliver. */
    float scatterH   = (rcViewport().height - safe.top - safe.bottom
                        - EXP_CONTENT_SHELL_H) * EXP_SCATTER_SHARE;
    bool  scatterUnder = tableRow || scatterH >= EXP_SCATTER_MIN_H;

    if (scatterH > EXP_SCATTER_H)
        scatterH = EXP_SCATTER_H;

    rcBox(.id = "root", .bg = s.background, .gap = 12, .p = 12,
          .pt = (uint16_t)(12.0f + safe.top),
          .pb = (uint16_t)(12.0f + safe.bottom),
          .pl = (uint16_t)(12.0f + safe.left),
          .pr = (uint16_t)(12.0f + safe.right), .w = "grow", .h = "grow") {

        if (railBeside) {
            rcRow(.gap = 12, .w = "grow", .h = "grow") {

                /* The rail scrolls: its cards are sized by their content, so
                   past about 175% zoom it is taller than the window - the same
                   thing a sidebar does in a browser. A SIBLING of the table's
                   scroll container, never an ancestor: a virtualized list needs
                   a viewport that is the window's, not its parent's content. */
                rcColumn(.id = "rail", .gap = 12, .scroll = "v", .h = "grow",
                         .wType = RC_PX(EXP_RAIL_W)) {
                    rail_cards(app, st, true, bandStats);
                }
                /* Auto-hides while the rail fits. Declared beside the rail in
                   each arm: a bar for an id that was not declared this frame
                   warns on every such frame. */
                rcScrollbar("rail");

                /* The rebuild happens HERE, between the two columns: the rail's
                   controls have just been polled and everything that reads the
                   view is still ahead of it. */
                apply_filters(st);

                rcColumn(.gap = 12, .w = "grow", .h = "grow") {
                    /* THE QUERY COMES FIRST AND THE RESULT COMES WITH IT: a
                       rail of controls does not say what the screen is showing,
                       the chips and the count do. */
                    query_card(app, st, true, bandStats);

                    /* The chart pair never has to stack in this arm, by
                       arithmetic rather than luck: EXP_RAIL_BESIDE_W is DEFINED
                       as the width that leaves the content column
                       EXP_CHART_PAIR_W. A bool here could not be false. */
                    rcRow(.gap = 12, .w = "grow", .hType = RC_PX(228)) {
                        chart_card("c_years", "DISCOVERIES BY YEAR",
                                    "selection over the full catalogue", "grow",
                                    "grow", chart_years, st);
                        chart_card("c_radius", "RADIUS DISTRIBUTION",
                                    "Earth radii", "300px", "grow",
                                    chart_radius, st);
                    }

                    rcRow(.gap = 12, .w = "grow", .h = "grow",
                          .className = tableRow ? "flex-row" : "flex-col") {
                        table_pane(app, st, wideCols);
                        /* Stacked, the scatter takes the slot worked out above
                           and the table keeps the rest - two panes both saying
                           "grow" would split the window. */
                        if (scatterUnder) {
                            chart_card("c_scatter", "PERIOD vs RADIUS",
                                        "days vs Earth radii \xc2\xb7 click a dot",
                                        tableRow ? "320px" : "grow",
                                        tableRow ? "grow"
                                                 : rcFormat(rcAppArena(app),
                                                             "%.0fpx",
                                                             (double)scatterH).chars,
                                        chart_scatter, st);
                        }
                    }
                }
            }
        } else {
            /* NARROW: one pane at a time, each with the whole window. This arm
               exists because the panes stopped fitting, not because this is a
               phone - a desktop window dragged narrow gets it too. */
            rcColumn(.gap = 12, .w = "grow", .h = "grow") {
                app_title();
                pane_tabs(st);
                /* ABOVE the panes, not inside one: what was asked and how many
                   rows came back is true of all three. */
                query_card(app, st, false, false);

                if (st->pane == PANE_FILTERS) {
                    /* .pr = 14 is a gutter for this pane's own scrollbar,
                       which floats 8 px wide and 3 px in from the right edge. */
                    rcColumn(.id = "rail", .gap = 12, .pr = 14, .scroll = "v",
                             .w = "grow", .h = "grow") {
                        rail_cards(app, st, false, false);
                    }
                    rcScrollbar("rail");
                }

                /* Same reason as the wide arm, and OUTSIDE the pane branches
                   because a filter moved in the Filters pane must reach the
                   charts the user switches to. The frame is requested because
                   the one reader NOT below this line is the query card's count,
                   which this arm puts above the sliders. */
                if (apply_filters(st))
                    rcWindowRequestFrame(rcAppMainWindow(app));

                if (st->pane == PANE_CHARTS) {
                    /* Stacked while that gives each chart a legible height,
                       three across when it does not - a phone in landscape.
                       One element, its direction picked by class. */
                    rcRow(.gap = 12, .w = "grow", .h = "grow",
                          .className = chartsRow ? "flex-row" : "flex-col") {
                        chart_card("c_years", "DISCOVERIES BY YEAR",
                                    "selection over the full catalogue", "grow",
                                    "grow", chart_years, st);
                        chart_card("c_radius", "RADIUS DISTRIBUTION",
                                    "Earth radii", "grow", "grow",
                                    chart_radius, st);
                        chart_card("c_scatter", "PERIOD vs RADIUS",
                                    "days vs Earth radii \xc2\xb7 click a dot", "grow",
                                    "grow", chart_scatter, st);
                    }
                }

                if (st->pane == PANE_TABLE)
                    table_pane(app, st, wideCols);
            }
        }

        /* ONE footer, outside both arrangements and the full width of the
           window. */
        status_bar(app, st, railBeside ? scatterUnder
                                       : st->pane == PANE_CHARTS);
    }

    /* The detail card is drawn BEFORE the table that changes the selection, so
       a click would otherwise show the previous planet for a frame. On demand,
       nothing else is coming to fix that: ask for the frame. */
    if (st->selected != before)
        rcWindowRequestFrame(rcAppMainWindow(app));

    /* Same rule, second reader: the query card prints the sort order above the
       controls that change it. */
    if (st->resort)
        rcWindowRequestFrame(rcAppMainWindow(app));
}

#endif /* APP_APP_H */
