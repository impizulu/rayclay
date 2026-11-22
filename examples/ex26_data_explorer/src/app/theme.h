/*
    theme.h - this app's design tokens

    Every value the UI reads that is a choice rather than a mechanism: the
    colors, the bands' heights and the widths the layout changes shape at. The
    file to edit to restyle the app without reading the layout.

    The library's widgets read their colors from RC_Style, which app_style()
    fills. The colors RC_Style has no role for live in the Palette, which
    app_palette() fills, or, when only a class reads one, in app_classes().
    main.c installs all three, once, before the first frame.
*/
#ifndef APP_THEME_H
#define APP_THEME_H

#include "rayclay.h"

enum { GROUP_HUES = 8 };

typedef struct {
    RC_Color rail;        /* the facet rail and the table's header              */
    RC_Color rule;        /* between table rows: quieter than a border          */
    RC_Color context;     /* every order, behind the filtered bars              */
    /* The band is its own small dark theme: its combo and menus take these roles. */
    RC_Color bandRaised, bandBorder, bandText, bandMuted;
    RC_Color up;          /* a change for the better                            */
    RC_Color down;        /* a change for the worse                             */
    /* One hue per group value on the chart lines and the top list, far enough apart to name
       a line, and none of them the accent, which on the chart means the metric's total. A
       dimension with more values than hues colors by rank instead (group_hue in app.h). */
    RC_Color hue[GROUP_HUES];
    /* Order status, as a pill: dark ink on its own light tint. */
    RC_Color statusInk[4], statusFill[4];
} Palette;

/* The workbench: cool white panels flush against each other under a deep navy
   band, one blue accent, hairlines where panels meet. */
static inline RC_Style app_style(void) {
    RC_Style s = rcStyleLight();

    s.background   = rcHex(0xF4F6FA);
    s.surface      = rcHex(0xFFFFFF);
    s.surfaceAlt   = rcHex(0xEEF2F8);   /* row hover, track, pressed fill */
    s.chrome       = rcHex(0x0E2240);   /* the top band                   */
    s.text         = rcHex(0x17212F);
    s.textMuted    = rcHex(0x5B687A);
    s.primary      = rcHex(0x1F6FEB);
    s.primaryHover = rcHex(0x1659C4);
    s.border       = rcHex(0xDCE2EC);
    s.radius       = 8.0f;
    return s;
}

static inline Palette app_palette(void) {
    Palette p = {
        .rail       = rcHex(0xF8F9FC),
        .rule       = rcHex(0xECEFF4),
        .context    = rcHex(0xCCD6E4),
        .bandRaised = rcHex(0x1B3457),
        .bandBorder = rcHex(0x2D4A72),
        .bandText   = rcHex(0xF2F6FC),
        .bandMuted  = rcHex(0xA6B6CC),
        .up         = rcHex(0x12805C),
        .down       = rcHex(0xC93A3A),
        .hue        = { rcHex(0x0F9D8A), rcHex(0xE8890C), rcHex(0x7C4DDB), rcHex(0xD6408B),
                        rcHex(0x8A9A2B), rcHex(0x4F88C6), rcHex(0x8C5A3C), rcHex(0x6B7686) },
        .statusInk  = { rcHex(0x0F6B4D), rcHex(0x8A5A00), rcHex(0xB42318), rcHex(0x1659C4) },
        .statusFill = { rcHex(0xDFF5EC), rcHex(0xFDF0D0), rcHex(0xFCE5E2), rcHex(0xE3EEFD) },
    };

    return p;
}

/* The app's own looks as classes. A class string is resolved once and cached, so it
   cannot read a Palette field: a look built on a color only a class reads is named here,
   once, and its hover rides with it. A color RC_Style has a role for needs no name,
   because a role token (bg-surface, hover:bg-surface-alt) follows the installed style. */
static inline void app_classes(void) {
    /* The selection's tint, a color only a class reads: a selected table row, a filter
       chip, the metric's tile, a checked top-list row and the sort in use. */
    rcDefineClass("tint", "bg-[#EAF2FE]");
    /* A filter chip, a shade deeper under the pointer. */
    rcDefineClass("chip", "tint hover:bg-[#D8E7FD]");
    /* A KPI tile, and the one whose metric the charts show: a 2 px bottom edge in the
       theme's border or primary. */
    rcDefineClass("kpi", "bg-surface hover:bg-surface-alt border-b-2 border-border");
    rcDefineClass("kpi-on", "tint border-b-2 border-primary");
}

enum {
    BAND_H     = 56,    /* the top band, which is also the titlebar           */
    QUERY_H    = 40,    /* the query bar under a mouse, border included       */
    QUERY_H_TOUCH = 46, /* the same bar under a finger                        */
    ROW_H      = 34,    /* a table row under a mouse                          */
    ROW_H_TOUCH = 44,   /* the same row under a finger                        */
    ROW_H_LIST = 76,    /* a row that has folded its columns into three lines */
    LIST_SHOWS = 7,     /* the columns those three lines carry                */
    QUERY_CAP  = 48,    /* the query field's buffer, NUL included             */
    HIST_BINS  = 24,    /* bars over the order-value facet                    */
    VALUE_TOP  = 600,   /* dollars; the slider's top stop means "no limit"    */
    BUCKET_MAX = 64,    /* bars in the trend chart: 58 weeks is the most     */
    SPLIT_MAX  = 6      /* lines the trend chart splits into                  */
};

/* The widths and heights this app changes shape at, always compared against the
   space actually being laid out, so a desktop window dragged narrow takes the same
   branch a phone does. */
#define EXP_DESK_MIN_W   860.0f   /* the rail beside the charts and the table      */
#define EXP_DESK_MIN_H   560.0f   /* the chart over the table, with rows to read   */
#define EXP_LOW_H        500.0f   /* below it a KPI tile is its name and number    */
#define EXP_RAIL_W       256.0f
#define EXP_RAIL_NARROW  224.0f   /* below EXP_RAIL_ROOMY_W                        */
#define EXP_RAIL_ROOMY_W 1100.0f
#define EXP_DRAWER_W     344.0f
#define EXP_DRAWER_MIN   600.0f   /* the main column the drawer must leave         */
#define EXP_LIST_W       560.0f   /* the table folds its columns into its rows     */
#define EXP_KPI_ROW_W    640.0f   /* five KPIs in one row rather than two across   */
#define EXP_TABS_INLINE  600.0f   /* the pane tabs join the query bar's row        */
#define EXP_MENU_LABEL_W 960.0f   /* the band's menus keep their words             */
#define EXP_CHARTS_SIDE  620.0f   /* the trend and the top list side by side       */

#endif /* APP_THEME_H */
