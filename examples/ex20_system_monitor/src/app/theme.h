/* theme.h - this app's design tokens: the palette, the type ramp, the chart
   history length, the titlebar heights the fold animates between, and the
   process table's row pitch. Nothing here calls RayClay and nothing here holds
   state, so there is no .c beside it. This is the file you edit to restyle the
   app without reading the layout.

   A colour token must be a #define or a brace-initialised RC_Color, never a
   file-scope `static const RC_Color X = rcRgb(...)`: rcRgb expands to a
   compound literal, which is not a constant expression in C99. A function
   returning an RC_Style has no such problem, which is why sys_style() is one.

   RC_AppOptions holds a POINTER to the fontSizes array rather than a copy, so
   that storage must outlive rcRunApp - main()'s own frame is where it lives. */
#ifndef APP_THEME_H
#define APP_THEME_H

/* The two series identities. Cyan is a LINE and a TEXT colour on a panel; the
   violet keeps resident memory distinguishable from the load series by more
   than its position. Neither is a threshold - see load_color for those. */
#define SYS_TREND_CYAN rcRgb( 56, 189, 213)
#define SYS_MEM_VIOLET rcRgb(168, 140, 246)

/* The app's whole palette, installed once in main(); every colour drawn is read
   back from rcGetStyle(). The *Hover greens and ambers are the saturated
   threshold shades load_color() paints a band with; the bases are the darker
   action shades a fill under white text needs. The chrome sits BELOW the canvas
   so the branded titlebar reads as chrome rather than as another panel. */
static inline RC_Style sys_style(void)
{
    RC_Style s = rcStyleDark();

    s.background   = rcRgb( 21,  23,  28);   /* canvas                        */
    s.surface      = rcRgb( 30,  33,  40);   /* panel                         */
    s.surfaceAlt   = rcRgb( 42,  46,  55);   /* inset: table header, chips    */
    s.chrome       = rcRgb( 16,  18,  22);   /* the branded titlebar          */
    s.text         = rcRgb(233, 237, 243);
    s.textMuted    = rcRgb(148, 157, 173);   /* 5.90:1 on the panel           */
    s.primary      = rcRgb(  7, 122, 160);   /* fill; white on it is 4.93:1   */
    s.primaryHover = rcRgb(  6, 105, 138);
    s.danger       = rcRgb(240,  82,  96);
    s.dangerHover  = rcRgb(214,  58,  74);
    s.success      = rcRgb( 38, 141, 105);
    s.successHover = rcRgb( 61, 191, 142);
    s.warning      = rcRgb(214, 158,  46);
    s.warningHover = rcRgb(240, 180,  58);
    s.border       = rcRgb( 48,  53,  63);
    s.radius       = 6.0f;
    return s;
}

/* Font slots, in load order into RC_AppOptions.fontSizes; the index is the
   .font value in RC_TextOptions. Baked from the bundled face (zero-asset). */
enum { F_MICRO = 0, F_SMALL, F_BODY, F_STAT, F_COUNT };

/* The BAKED pixel size of each slot. SLOT 0 MATTERS BEYOND THIS APP: every
   library widget that draws text without a .font reads it - button and checkbox
   labels, combo values, menu items, table headers, tooltips, rcChart's tick
   labels. Order the ladder with that in mind. */
enum {
    SZ_MICRO = 11, SZ_SMALL = 13, SZ_BODY = 15, SZ_STAT = 26
};

enum {
    HISTORY      = 90,   /* samples kept for the charts                        */
    BAR_OPEN_H   = 48,   /* titlebar height, expanded, everything on one row   */
    BAR_STACK_H  = 64,   /* expanded in a phone-shaped window, on two rows     */
    BAR_RAIL_H   = 5,    /* ... and folded. NOT draggable on Windows           */
    ROW_H        = 26,   /* process-table CELL height - NOT the row pitch      */
    /* FLUSH, AND THAT IS WHAT MAKES THE ZEBRA A ZEBRA. cellPadding is a gap
       around EVERY cell, so any non-zero value leaves an unpainted seam at each
       column boundary and a striped row reads as five separate bars. Explicit
       rather than omitted: a zero-init RC_TableOptions gets the house 6 px, and
       only RC_VAL(0) is genuinely flush. */
    CELL_PAD     = 0,
    ROW_PITCH    = ROW_H + 2 * CELL_PAD
};

#endif /* APP_THEME_H */
