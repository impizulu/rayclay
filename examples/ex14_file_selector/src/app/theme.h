/*
    theme.h - this app's design tokens

    A desktop's two appearances: graphite or silver panels, a system blue for the one
    action and the selection, hairlines that barely show. The selector reads only
    rcGetStyle(), so this file restyles the page and the selector together, and the
    button in the page's top bar flips both while they run.
*/
#ifndef APP_THEME_H
#define APP_THEME_H

#include "rayclay.h"

/* A modal's body is PANEL_W x PANEL_H at most, kept MARGIN clear of the window's edges. */
enum { PANEL_W = 1040, PANEL_H = 680, MARGIN = 32 };

/* The page around it: a rail and an inspector beside the stage from WIDE_W. */
enum { WIDE_W = 900, RAIL_W = 220, INSPECTOR_W = 264, BAR_H = 52 };

/* EVERY COLOR THAT CARRIES WORDS CLEARS 4.5:1 (WCAG AA) on the grounds it is drawn
   on: text and muted on every panel, white on accent. The selected row carries
   white, never text. Re-check the ratios when you change one of these. */
static inline RC_Style app_style(bool dark) {
    RC_Style s = dark ? rcStyleDark() : rcStyleLight();

    if (dark) {
        s.background = rcRgb( 27,  27,  29);   /* the stage and the table      */
        s.surface    = rcRgb( 42,  42,  45);   /* the panel, the page's chrome */
        s.surfaceAlt = rcRgb( 58,  58,  62);   /* a control's fill             */
        s.text       = rcRgb(242, 242, 244);
        s.textMuted  = rcRgb(166, 166, 172);
        s.danger     = rcRgb(255,  99,  88);
        s.border     = rcRgb( 64,  64,  68);
    } else {
        s.background = rcRgb(250, 250, 251);
        s.surface    = rcRgb(236, 236, 239);
        s.surfaceAlt = rcRgb(255, 255, 255);
        s.text       = rcRgb( 29,  29,  31);
        s.textMuted  = rcRgb( 92,  92,  99);
        s.danger     = rcRgb(192,  28,  40);
        s.border     = rcRgb(208, 208, 213);
    }
    s.chrome       = s.background;
    s.primary      = rcRgb( 11,  98, 214);     /* the action, the selected row */
    s.primaryHover = rcRgb( 10, 108, 224);
    s.radius       = 8.0f;
    return s;
}

#endif /* APP_THEME_H */
