/*
    theme.h - this app's design tokens: Grafana's data colors and the bands they stand
    for, on RayClay's own night sky, and the dashboard's measurements. Nothing here holds
    state, so this is the file to edit to restyle the app without reading the layout.

    The type scale is written where it is drawn, as true CSS numbers: 12 for axes,
    legends and tags, 13 for table data, 14 for panel titles, 16 for row headers,
    then 20, 28, 30 and 44 for gauges and big numbers. rcFontSemibold takes the titles
    a weight above the body, as Grafana does.
*/
#ifndef APP_THEME_H
#define APP_THEME_H

/* The colors RC_Style has no field for. Green, yellow and red always mean a state, a
   band crossed or a process up or down, so no series is drawn in them. */
typedef struct {
    RC_Color green, yellow, red;          /* the threshold bands                        */
    RC_Color blue, purple, orange, teal;  /* series, for readings with no band to cross */
    RC_Color link;                        /* template-variable labels, table headers
                                             and links                                  */
    RC_Color tabFrom, tabTo;              /* the two ends of the bar under the active
                                             tab                                        */
    RC_Color glass, glassEdge;            /* a panel's fill and rim, over the aurora    */
} Palette;

static inline Palette app_palette(void) {
    Palette p;

    p.green   = rcRgb(115, 191, 105);
    p.yellow  = rcRgb(250, 222,  42);
    p.red     = rcRgb(242,  73,  92);
    p.blue    = rcRgb( 87, 148, 242);
    p.purple  = rcRgb(184, 119, 217);
    p.orange  = rcRgb(255, 152,  48);
    p.teal    = rcRgb(110, 208, 224);
    p.link    = rcRgb(110, 159, 255);
    p.tabFrom = rcRgb(139,  92, 246);
    p.tabTo   = rcRgb( 34, 211, 238);
    p.glass     = rcRgb(19, 23, 38, 205 / 255.0f);
    p.glassEdge = rcRgb(150, 165, 215, 44 / 255.0f);
    return p;
}

/* The load bands, in percent: green below LOAD_WARN, yellow to LOAD_CRIT, red above. */
#define LOAD_WARN 60.0f
#define LOAD_CRIT 80.0f

/* The same bands for the fleet's p99 latency, in ms. LAT_WARN is also the alert's
   line across the latency chart, and LAT_TOP the chart's pinned ceiling: five 25 ms
   gridlines, so the line sits on one. */
#define LAT_WARN  75.0f
#define LAT_CRIT 100.0f
#define LAT_TOP  125.0f

/* The style, installed once in main(); every color it holds is read back from
   rcGetStyle(). The chrome is the panel color, so the top bars and the panels read
   as one surface above the darker canvas. */
static inline RC_Style app_style(void) {
    RC_Style s = rcStyleDark();

    s.background   = rcRgb( 11,  13,  22);   /* canvas, under the aurora     */
    s.surface      = rcRgb( 22,  25,  38);   /* menus and popovers           */
    s.surfaceAlt   = rcRgb( 36,  40,  58);   /* inset: hover, chips, tracks  */
    s.chrome       = rcRgb( 16,  18,  30);   /* titlebar and toolbar         */
    s.text         = rcRgb(204, 204, 220);
    s.textMuted    = rcRgb(150, 155, 172);   /* 6:1 on the panel             */
    s.primary      = rcRgb( 61, 113, 217);   /* white on it is 4.6:1         */
    s.primaryHover = rcRgb( 83, 131, 227);
    s.danger       = rcRgb(209,  14,  92);
    s.dangerHover  = rcRgb(229,  34, 112);
    s.success      = rcRgb( 26, 127,  75);
    s.successHover = rcRgb( 36, 150,  92);
    s.warning      = rcRgb(255, 152,  48);
    s.warningHover = rcRgb(255, 172,  84);
    s.border       = rcRgb( 66,  71,  92);   /* the charts' grid reads this  */
    s.radius       = 2.0f;
    return s;
}

/* The widths the pages change shape at, each compared against the width being laid
   out (content_width), so a desktop window dragged narrow takes the same branch a
   phone does. */
#define SYS_WIDE_W 1100.0f   /* everything across; three panels to a row */
#define SYS_MID_W   700.0f   /* below this the panels stack              */

/* The shortest span a drag can zoom to, in axis minutes: 15 one-second samples, enough
   to draw a line through, on ticks three seconds apart. */
#define ZOOM_MIN_SPAN (15.0f / 60.0f)

enum {
    HISTORY     = 1800, /* samples kept: the longest range at the fastest refresh */
    /* The most points a chart and a sparkline draw per series: a plot gains nothing from
       more points than it has pixels, and fewer points cost less to draw. */
    PLOT_POINTS = 320,
    SPARK_POINTS = 120,
    BAR_OPEN_H  = 40,   /* titlebar height, open                                  */
    BAR_RAIL_H  = 4,    /* ... and folded: the strip the window still drags by    */
    GUTTER      = 8,    /* Grafana's gap between panels                           */
    PAGE_PAD    = 8,    /* the page's inset from the window                       */
    PANEL_HEAD  = 30,   /* a panel's title row                                    */
    OVERVIEW_H  = 156,  /* the row of gauges and stats                            */
    TILES_W     = 196,  /* the status tiles beside them                           */
    TILES_ROW_H = 72,   /* ... or on a row of their own, on a phone               */
    GAUGES_ROW_H = 132, /* the gauges on a row of their own                       */
    CPU_ROW_H   = 300,  /* the CPU time series, the heatmap and the bar gauge     */
    HEAT_ROW_H  = 200,  /* the heatmap on a row of its own                        */
    MEM_ROW_H   = 256,  /* the memory, network and disk panels                    */
    STAT_H      = 120,  /* a stat panel on a row of its own                       */
    PAGE_ROWS   = 10,   /* process-table rows per page                            */
    ROW_H       = 30,   /* process-table row height                               */
    ROW_H_STACK = 46,   /* ... with its dropped columns on a second line          */
    LIVE_ROW_H  = 260,  /* the Services page's latency chart and slowest board    */
    CARD_MIN_W  = 232,  /* a service card: the grid fits as many as clear this    */
    /* The live band's frame rate, and with it the Services page's whole cost: a
       5-minute plot slides a pixel about every 300 ms, so 30 is smooth. */
    LIVE_FPS    = 30
};

#endif /* APP_THEME_H */
