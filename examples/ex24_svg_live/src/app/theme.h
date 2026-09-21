/*
    theme.h - this app's design tokens

    Every value the UI reads that is a CHOICE rather than a mechanism: the
    palette, the one-off geometry, the type ramp. Nothing here calls RayClay and
    nothing holds state, which is why there is no .c beside it. It is the file
    you edit to restyle the app - tokens.ts, in C.

    A COLOUR MUST BE A BRACE INITIALISER OR A #define, NEVER A static const:
    rcRgb expands to a compound literal, which is not a constant expression in
    C99. ROUTE and TINTS below are ordinary aggregate initialisers and are fine,
    and a function returning an RC_Style has no such problem - which is why
    svg_style() is one.
*/
#ifndef APP_THEME_H
#define APP_THEME_H

#include "rayclay.h"

enum { BAR_H = 44, RAIL_W = 216, PAD = 18 };

/* The type ramp, baked from the bundled face. SLOT 0 REACHES BEYOND THIS APP:
   every library widget that draws text without a .font reads it. */
enum { F_BODY = 0, F_TITLE, F_COUNT };
static const float FONT_SIZES[F_COUNT] = { 16.0f, 22.0f };

/* A graphite canvas: window, card, and the bed the artwork sits on differ only
   in lightness. `primary` is a steel blue rather than an accent hue on purpose -
   it fills the selected artwork button, inches from three route identifiers, and
   must not read as a fourth one. */
static inline RC_Style svg_style(void)
{
    RC_Style s = rcStyleDark();

    s.background   = rcRgb( 15,  18,  23);   /* window                        */
    s.surface      = rcRgb( 24,  28,  35);   /* the cards and the rail        */
    s.surfaceAlt   = rcRgb( 31,  36,  44);   /* the bed the artwork sits on   */
    s.chrome       = rcRgb( 24,  28,  35);
    s.text         = rcRgb(226, 232, 240);
    s.textMuted    = rcRgb(141, 152, 168);
    s.primary      = rcRgb( 52,  80, 115);
    s.primaryHover = rcRgb( 66, 100, 142);
    s.border       = rcRgb( 46,  53,  65);
    /* Amber means the parser or the file system had something to say, and it
       means nothing else anywhere in this app. */
    s.warning      = rcRgb(251, 191,  36);
    s.warningHover = rcRgb(253, 211,  77);
    s.radius       = 6.0f;
    return s;
}

/* THE THREE ROUTE IDENTIFIERS, in the order of the R_* enum in app.h. NEVER
   PASSED TO A DRAW CALL: they colour the rule and the name at the top of a card
   and stop there, because a route that tinted its own artwork would stop the
   three panes being comparable. */
static const RC_Color ROUTE[] = {
    {  34, 211, 238, 255 },   /* BY PATH   */
    { 232, 121, 249, 255 },   /* HANDLE    */
    { 129, 140, 248, 255 },   /* GENERATED */
};

/* The colour ARGUMENT, offered as six values so "colour is a per-call argument,
   not a property of the handle" is something you do rather than something you
   read. Deliberately no member of ROUTE, and deliberately not amber. */
static const RC_Color TINTS[] = {
    { 226, 232, 240, 255 }, { 148, 163, 184, 255 }, {  56, 189, 248, 255 },
    {  45, 212, 191, 255 }, { 163, 230,  53, 255 }, { 244,  63,  94, 255 },
};
enum { TINT_COUNT = (int)(sizeof TINTS / sizeof TINTS[0]) };

/* AN ELEMENT ID MUST OUTLIVE THE FRAME, so every id in this app is a literal
   from a table rather than an rcFormat into the frame arena, which is reset
   once the frame has been drawn. */
static const char *const TINT_ID[TINT_COUNT] = {
    "tint0", "tint1", "tint2", "tint3", "tint4", "tint5",
};

#endif /* APP_THEME_H */
