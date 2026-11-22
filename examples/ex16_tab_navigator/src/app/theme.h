/*
    theme.h - this app's design tokens

    Every value the UI reads that is a CHOICE rather than a mechanism. Nothing here
    declares an element and nothing here holds state.

    THE LOOK IS A PHONE'S OWN. White (or true black) edge to edge, bands of tone
    between posts instead of bordered cards, black and white for the accent, and an
    orange-to-magenta gradient kept for the two things that should catch an eye: a
    person's ring and the compose button. Hearts are rose and reposts green, by convention.
*/
#ifndef APP_THEME_H
#define APP_THEME_H

#include "rayclay.h"

/* The spacing scale, and the tab bar, which adds the safe inset below itself so the home indicator
   sits on its color. A post page's reply bar is one field tall with 6 px around it. */
enum { GUTTER = 16, BAR_H = 56, TARGET = 44, ICON = 22, FEED_GAP = 8, REPLY_BAR_H = TARGET + 12 };

/* THE WIDE ARM. A rail needs RAIL_W, a post stays legible down to LIST_W, a detail pane wants
   DETAIL_MIN_W. WIDE_W is their sum, so the breakpoint is spelled from its parts rather than
   pulled from a table of device widths. The sum clears 852, the phone this app is designed
   for turned on its side, which keeps the phone shell. */
enum { RAIL_W = 88, LIST_W = 360, LIST_W_ROOMY = 420, DETAIL_MIN_W = 420 };
enum { WIDE_W = RAIL_W + LIST_W + DETAIL_MIN_W, ROOMY_W = 1100 };

/* A WINDOW UNDER LOW_H IS LOW: a phone on its side, or a desktop window at a high zoom. Its roots
   drop the large title and Home its people row, so the posts keep the height. A phone held
   upright is taller than this. */
enum { LOW_H = 500 };

/* A reading measure, so a paragraph in a wide window is a column and not a line as long as the
   pane; the feed takes the same cap, and a sheet its own (the sheet class, main.c). */
enum { READ_W = 680 };

/* On a post's page in the wide arm's pane, the percent of the window's height its picture may take:
   a 4:3 picture across that pane would push the actions and the first replies off the screen. */
enum { PANE_PHOTO_VH = 40 };

#define SHEET_SECONDS 0.26

/* Pull to refresh: the pointer's travel reads at half speed, as a rubber band does, and past
   PULL_FIRE the release refreshes. PULL_REST is the band's height while the spinner runs. */
enum { PULL_FIRE = 56, PULL_REST = 52 };
#define REFRESH_SECONDS 1.1

/* A liked heart's jump, which heart_scale draws and is_animating waits out. */
#define POP_SECONDS 0.36

/* THE TWO THEMES: the preset with this app's role colors over it. Light is the phone's white with a
   grouped gray; dark is true black, so an OLED phone's feed sits on nothing at all, with lifted
   cells on it and a step above those for what sits in a cell: a switch's track, a segmented
   control's. The accent is black on white and white on black, as Threads draws it, and it carries
   through RC_Style.primary into the library's own widgets, down to every focus ring; a switch
   takes a blue of its own (toggle_row), since its knob is white. The radius makes a phone's pill of
   every rcButton and field at the height this app gives them. */
static RC_Style app_style(bool dark) {
    RC_Style s = dark ? rcStyleDark() : rcStyleLight();

    s.radius = (float)TARGET / 2.0f;
    if (dark) {
        s.background = s.chrome = rcRgb(0, 0, 0);
        s.surface      = rcRgb( 28,  28,  32);
        s.surfaceAlt   = rcRgb( 58,  58,  64);
        s.text         = rcRgb(245, 245, 247);
        s.textMuted    = rcRgb(154, 154, 165);
        s.border       = rcRgb( 44,  44,  50);
        s.primary      = rcRgb(255, 255, 255);
        s.primaryHover = rcRgb(219, 219, 219);
    } else {
        s.background = s.surface = s.chrome = rcRgb(255, 255, 255);
        s.surfaceAlt   = rcRgb(242, 242, 247);
        s.text         = rcRgb( 17,  17,  22);
        s.textMuted    = rcRgb(104, 104, 116);
        s.border       = rcRgb(226, 226, 232);
        s.primary      = rcRgb(  0,   0,   0);
        s.primaryHover = rcRgb( 38,  38,  38);
    }
    return s;
}

/* The colors RC_Style has no field for, the same in both themes. */
static const struct {
    RC_Color brandFrom, brandTo, heart, repost;
} PALETTE = {
    .brandFrom = { 245, 133,  41, 255 },   /* orange      */
    .brandTo   = { 221,  42, 123, 255 },   /* magenta     */
    .heart     = { 244,  63,  94, 255 },   /* rose 500    */
    .repost    = {  16, 185, 129, 255 },   /* emerald 500 */
};

/* Text size is a setting with four stops. A stop is a real size, baked once, so the setting
   spends a known handful of the 16 font slots however often it moves. */
enum { TEXT_STEPS = 4 };
static const uint16_t POST_PX[TEXT_STEPS] = { 15, 16, 18, 20 };
static const uint16_t POST_LH[TEXT_STEPS] = { 21, 23, 26, 29 };
static const uint16_t READ_PX[TEXT_STEPS] = { 16, 18, 20, 22 };
static const uint16_t READ_LH[TEXT_STEPS] = { 24, 28, 31, 33 };

#endif /* APP_THEME_H */
