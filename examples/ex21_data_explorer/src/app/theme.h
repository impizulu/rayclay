/*
================================================================================
    theme.h - this app's design tokens
================================================================================
    Every value the UI reads that is a CHOICE rather than a mechanism: the type
    ramp, the histogram's resolution, the plotting budget, the row height.
    Values only - nothing here calls RayClay, holds state or includes anything.
    It is the file you edit to restyle the app without reading the layout.

    A COLOUR MUST BE A #define, NEVER a file-scope static const: rcRgb and
    rcRgba expand to compound literals and rcHex is a function, so none of the
    three is a constant expression in C99. The app's palette is built inside
    explorer_style() in app.h and handed to rcSetStyle once.
================================================================================
*/
#ifndef APP_THEME_H
#define APP_THEME_H

/* Font slots, in load order into RC_AppOptions.fontSizes; the index is the
   .font value in RC_TextOptions. Baked from the bundled face (zero-asset). */
enum { F_MICRO = 0, F_SMALL, F_HEAD, F_STAT, F_COUNT };

enum {
    HIST_BINS   = 24,    /* radius histogram resolution                       */
    HIST_MAX_R  = 16,    /* Earth radii the histogram and the radius sliders
                            span. The catalogue's own largest planet, so no
                            bar, and no travel on either slider, is spent on
                            range that has nothing in it.                     */
    SCATTER_MAX = 1200,  /* markers actually plotted; see plot_scatter        */
    SCATTER_P   = 100,   /* scatter x window, days                            */
    /* The scatter's y window is the catalogue's own largest radius, not a
       round number above it: an axis pinned past the data is a band of empty
       plot the reader has to discount. Anything above it is COUNTED, and the
       status line says how many. */
    SCATTER_R   = 16,    /* scatter y window, Earth radii                     */
    ROW_H       = 26,    /* table row height                                  */
    ROW_H_TWO   = 42     /* the same row with a second MICRO line under it    */
};

#endif /* APP_THEME_H */
