/*
    theme.h - this app's design tokens: the palette, the type ramp, the geometry.
    It is the file you edit to restyle the app without reading the layout.

    THREE FAMILIES OF COLOUR AND THEY NEVER OVERLAP: blue is selection and
    action, rose and amber are severity, and the area hues are a categorical set
    kept off both. Severity is never carried by colour alone - the P0..P3 label
    is drawn beside every ink and the chip's SHAPE falls away with the tier.

    A COLOUR MUST BE A #define OR A BRACE-INITIALISED RC_Color, NEVER
    `static const RC_Color X = rcRgb(...)`: those names expand to COMPOUND
    LITERALS, which are not constant expressions at file scope in C99. Inside
    inbox_style() a compound literal is an ordinary expression.
*/
#ifndef APP_THEME_H
#define APP_THEME_H

#include "rayclay.h"

/* The type ramp. Four slots of the sixteen RayClay offers, decided at startup
   and held for the run; main.c fills the sizes that go with them. */
enum { F_MICRO = 0, F_SMALL, F_BODY, F_TITLE, F_COUNT };

/* One ink per priority. Luminance runs WITH severity: both greys are dimmer
   than the amber, so the brightest mark in a column of P2s is the one fire. */
static const RC_Color PRIO_INK[4] = {
    { 251, 113, 133, 255 },   /* P0 - rose 400,  a fire                      */
    { 251, 191,  36, 255 },   /* P1 - amber 400, this week                   */
    { 148, 163, 184, 255 },   /* P2 - slate 400, the normal case             */
    { 120, 137, 161, 255 },   /* P3 - dimmer still, when someone gets to it  */
};

/* One hue per AREA[] entry, in the same order, at one lightness so no area
   outranks another. They ink the pill on every list row and the rail's AREA
   facet. */
static const RC_Color AREA_INK[8] = {
    { 167, 139, 250, 255 },   /* Editor   - violet 400  */
    {  45, 212, 191, 255 },   /* Sync     - teal 400    */
    { 232, 121, 249, 255 },   /* Renderer - fuchsia 400 */
    {  56, 189, 248, 255 },   /* Platform - sky 400     */
    { 163, 230,  53, 255 },   /* Search   - lime 400    */
    { 165, 180, 252, 255 },   /* Mobile   - indigo 300  */
    {  34, 211, 238, 255 },   /* Desktop  - cyan 400    */
    { 249, 168, 212, 255 },   /* Web      - pink 300    */
};

/* The style the whole app reads; every field not named here is the preset's. */
static inline RC_Style inbox_style(void)
{
    RC_Style s = rcStyleDark();

    /* Three slate surfaces, darkest FIRST: the scrolling list is the well the
       panels sit above, which is what makes three panes read as three. */
    s.background   = RC_SLATE_950;
    s.surface      = RC_SLATE_900;
    s.surfaceAlt   = RC_SLATE_800;
    s.chrome       = RC_SLATE_900;
    s.border       = RC_SLATE_700;
    /* 100 rather than 50: white on slate at 13 px vibrates, and a list is read
       a hundred rows at a time. */
    s.text         = RC_SLATE_100;
    s.textMuted    = RC_SLATE_400;
    /* Blue is the selected row, the focused control and the open combo. */
    s.primary      = RC_BLUE_600;
    s.primaryHover = RC_BLUE_500;
    /* Read as TEXT on a dark panel, so the 400 tiers rather than the 600
       fills the preset ships. */
    s.success      = RC_EMERALD_400;
    s.danger       = RC_ROSE_400;
    s.warning      = RC_AMBER_400;
    return s;
}

/* Geometry that is a choice. ROW_H is the one-line list row and ROW_H_TALL the
   two-line row a narrow list takes instead, which also clears the 44 pt a finger
   needs. BAR_H is also RC_AppOptions.titlebarHeight, so it is the height of the
   strip the OS lets you drag. RAIL_W and DETAIL_W are what make three panes fit.
   FACET_ROW_H is one pitch for all three filter lists. */
enum { ROW_H = 34, ROW_H_TALL = 52, BAR_H = 46 };
enum { RAIL_W = 228, DETAIL_W = 360, FACET_ROW_H = 32 };

/* How many replies the detail pane renders; the rest are counted, not dropped. */
enum { DETAIL_REPLIES = 6 };

/* The smallest target a finger can be asked to hit: 44 pt. Keyed on the ARM
   rather than on the pointer, so a desktop window at a phone's width shows
   exactly what the phone will. */
enum { HIT_MIN = 44 };

#endif /* APP_THEME_H */
