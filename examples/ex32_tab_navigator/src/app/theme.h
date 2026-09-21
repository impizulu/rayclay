/*
    theme.h - this app's design tokens

    Every value the UI reads that is a CHOICE rather than a mechanism. Nothing
    here calls RayClay and nothing here holds state.

    THE SURFACE IS THE THEME'S, THE ACCENT IS THE TAB'S. The Profile tab switches
    the theme at run time, so every surface, text and line colour comes from
    rcGetStyle() at the moment it is drawn. What is left here is what a theme
    cannot answer: the avatar tints and one accent per tab.

    A COLOUR MUST BE A #define, NEVER A static const. rcRgb / rcRgba expand to
    compound literals, which are not constant expressions at file scope in C99;
    brace-initialised RC_Color values are ordinary aggregate initialisers.
*/
#ifndef APP_THEME_H
#define APP_THEME_H

#include "rayclay.h"

/* The type ramp: five of the sixteen slots RayClay offers. F_HEAD is the
   pushed-screen title and F_TITLE the large title of a tab's root. */
enum { F_MICRO = 0, F_SMALL, F_BODY, F_HEAD, F_TITLE, F_COUNT };

/* THE PHONE SHELL. Each band adds the safe inset on its own side, so the status
   bar sits on the header's colour and the home indicator on the bar's. */
enum { HEADER_H = 56, BAR_H = 56 };

/* THE WIDE ARM. A rail needs RAIL_W, a list row stays legible down to LIST_W, a
   detail pane wants DETAIL_MIN_W; WIDE_W is their sum, so the breakpoint is
   spelled from its parts rather than pulled from a table of device widths. */
enum { RAIL_W = 88, LIST_W = 360, DETAIL_MIN_W = 452 };
enum { WIDE_W = RAIL_W + LIST_W + DETAIL_MIN_W };

/* Finger sizes. 48 is the floor every platform guideline agrees on; once
   rcPointerIsCoarse() has seen a real touch the app grows toward 56. HIT_MIN is
   the smallest any guideline allows and is what a control inside a band keeps. */
enum { HIT_FINE = 48, HIT_COARSE = 56, HIT_MIN = 44 };

/* Row pitches. A VIRTUAL LIST NEEDS EVERY ROW EXACTLY ITS PITCH TALL, which is
   why these are constants and not "fit". The Home feed has two, chosen by the
   Profile toggle; the roomy one carries a third line the compact one drops. */
enum { ROW_H = 84, ROW_H_COMPACT = 56, ROW_H_PLAIN = 64 };
enum { AVATAR = 40, AVATAR_COMPACT = 32, AVATAR_LARGE = 56, PAD = 16, ICON = 22 };

/* THE STATE MARKS, every one a SHAPE first and a colour second, so none of the
   four states this app shows - selected tab, open stack, unread row, chosen
   filter - is legible only to someone who can tell two hues apart. */
enum { MARK_W = 3, MARK_H = 28, PILL_W = 18, PILL_H = 3, BADGE_H = 18 };

/* The text-size slider's range, in px: body text scales, chrome keeps the ramp. */
enum { TEXT_MIN = 13, TEXT_MAX = 19 };

/* A reading measure, so on the wide arm a paragraph is a column and not a line
   900 px long. */
enum { READ_W = 720 };

/* Eight avatar tints, Tailwind's 400 row so they read on both themes. */
static const RC_Color AVATAR_TINT[] = {
    { 248, 113, 113, 255 }, { 251, 146,  60, 255 }, { 250, 204,  21, 255 }, {  74, 222, 128, 255 },
    {  45, 212, 191, 255 }, {  96, 165, 250, 255 }, { 167, 139, 250, 255 }, { 244, 114, 182, 255 },
};
enum { AVATAR_TINT_COUNT = (int)(sizeof AVATAR_TINT / sizeof AVATAR_TINT[0]) };

/* ONE ACCENT PER NAVIGATION MODEL, and only ever ONE ON SCREEN AT A TIME. Each
   tab carries its own hue through the lit bar item, the unread mark, the chosen
   filter and the row open in the pane beside it; every other surface is the
   theme's, and an unlit bar item stays muted.

   TWO ROWS, BECAUSE A TINT IS NOT READABLE ON BOTH GROUNDS. Tailwind's 400 row
   carries on near-black and disappears on white; the 600 row is the reverse. The
   app picks the row from the theme it just installed, which it can do because the
   theme here is a SETTING it owns, never a guess about a colour it was handed. */
static const RC_Color TAB_ACCENT_DARK[] = {
    {  56, 189, 248, 255 },   /* Home: the feed        sky 400     */
    { 167, 139, 250, 255 },   /* Search: the query     violet 400  */
    { 251, 191,  36, 255 },   /* Alerts: the inbox     amber 400   */
    {  52, 211, 153, 255 },   /* Profile: the account  emerald 400 */
};
static const RC_Color TAB_ACCENT_LIGHT[] = {
    {   2, 132, 199, 255 },   /* sky 600     */
    { 124,  58, 237, 255 },   /* violet 600  */
    { 217, 119,   6, 255 },   /* amber 600   */
    {   5, 150, 105, 255 },   /* emerald 600 */
};

#endif /* APP_THEME_H */
