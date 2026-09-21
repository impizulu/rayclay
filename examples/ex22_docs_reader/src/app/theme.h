/*
    theme.h - this app's design tokens.

    The palette, the type ramp, the face paths and the measurements the layout
    is derived from. Values only: it is the file you edit to restyle the app
    without reading the layout.
*/
#ifndef APP_THEME_H
#define APP_THEME_H

#include "rayclay.h"

/* CMake injects these. The defaults are what a reader launched from the
   repository root would use anyway, which keeps the file readable on its own. */
#ifndef RC_EX22_FONT_DIR
    #define RC_EX22_FONT_DIR "examples/assets/fonts/"
#endif
#ifndef RC_EX22_ICON
    #define RC_EX22_ICON "examples/assets/logos/rayclay-logo-1024.png"
#endif

#define FONT_BODY_TTF  RC_EX22_FONT_DIR "Lato-Regular.ttf"
#define FONT_BOLD_TTF  RC_EX22_FONT_DIR "Lato-Bold.ttf"
#define FONT_MONO_TTF  RC_EX22_FONT_DIR "AdwaitaMono-Regular.ttf"

/* BAKE sizes, not drawn sizes: the renderer scales a glyph from the size it was
   baked at to the size the run asks for. So the ladder is baked at the LARGEST
   step the control offers and every smaller step draws it down - baking small
   and drawing up softens the reader who asked for bigger type. */
#define BAKE_H1     34
#define BAKE_H2     24
#define BAKE_BODY   19
#define BAKE_SMALL  14
#define BAKE_CODE   16

/* What the text-size control selects: a whole ramp per step rather than a
   multiplier, so the heading-to-body relationship is decided here. */
typedef struct {
    uint16_t h1, h2, body, small, code;   /* drawn sizes                      */
    uint16_t bodyLine;                    /* prose leading (lead, para, list) */
    uint16_t noteLine;                    /* the aside, set one size down     */
    uint16_t codeLine;                    /* the monospace listing            */
} TypeStep;

static const TypeStep type_steps[] = {
    { 26, 19, 15, 12, 13,  25, 19, 19 },
    { 30, 21, 17, 13, 14,  28, 21, 21 },
    { 34, 24, 19, 14, 16,  31, 23, 24 }
};

#define TYPE_STEP_COUNT   ((int)(sizeof(type_steps) / sizeof(type_steps[0])))
#define TYPE_STEP_DEFAULT 1

/* The size the control draws its OWN letter at, one per step: it shows the ramp
   rather than naming it, and the toolbar measures the group from this array. A
   fourth step means a fourth entry here, a fourth ramp above and a fourth id in
   reader_size_group. */
static const uint16_t size_chip_px[TYPE_STEP_COUNT] = { 11, 14, 18 };

/* A reader is paper, so the app opens on paper and keeps a dark arm for the room
   it is read in. Both arms are the same struct, so every call site reads a ROLE.
   CONTRAST IS A MEASUREMENT: ink on paper 14.4:1, muted ink 5.3:1, accent 6.9:1
   and white on the accent fill 7.6:1, against the 4.5:1 body text wants. When a
   fill misses, darken the fill - lightening the text is how a palette goes pale.
   A colour token must be a #define or a brace-initialised struct, never a
   file-scope `static const RC_Color X = rcRgb(...)`: rcRgb expands to a compound
   literal, which C99 does not accept as a constant expression. */
typedef struct {
    RC_Color paper;        /* the page                                        */
    RC_Color paperAlt;     /* the rail, and the well a control group sits in  */
    RC_Color raised;       /* a field or card lifted off the page             */
    RC_Color ink;          /* body text and headings                          */
    RC_Color inkMuted;     /* captions, the status bar, an aside              */
    RC_Color border;       /* hairlines                                       */
    RC_Color accent;       /* section headings, the active chip, progress     */
    RC_Color accentHover;  /* the accent under the pointer                    */
    RC_Color accentInk;    /* text ON an accent fill                          */
    RC_Color codeBg;       /* the listing panel                               */
    RC_Color codeInk;      /* the listing itself                              */
    RC_Color hover;        /* the wash under a pointer on a plain row         */
    RC_Color warn;         /* the measure outside its comfortable range       */
    bool     dark;
} ReaderPalette;

static const ReaderPalette reader_paper_palette = {
    { 247, 242, 232, 255 },
    { 239, 232, 218, 255 },
    { 252, 250, 244, 255 },
    {  36,  33,  29, 255 },
    { 107,  99,  87, 255 },
    { 222, 215, 202, 255 },
    {  47,  93,  58, 255 },
    {  38,  77,  48, 255 },
    { 255, 255, 255, 255 },
    { 237, 230, 214, 255 },
    {  42,  74,  51, 255 },
    { 232, 224, 206, 255 },
    { 138,  90,  18, 255 },
    false
};

static const ReaderPalette reader_dark_palette = {
    {  27,  26,  24, 255 },
    {  35,  34,  32, 255 },
    {  40,  38,  35, 255 },
    { 233, 228, 218, 255 },
    { 158, 150, 138, 255 },
    {  51,  49,  45, 255 },
    { 143, 191, 154, 255 },
    { 168, 208, 177, 255 },
    {  27,  26,  24, 255 },
    {  31,  34,  32, 255 },
    { 200, 216, 196, 255 },
    {  43,  41,  38, 255 },
    { 217, 164,  65, 255 },
    true
};

/* The column is derived from the TYPE, not picked in pixels: a pixel cap is a
   guess about a font. TARGET_CHARS sits in the upper half of the 45-75 range
   typographers ask for; TOC_MIN_PX is the width below which keeping the contents
   rail would take the column under it. */
#define TARGET_CHARS    68.0f
#define TOC_MIN_PX      980.0f

/* Wide enough that every entry sits on ONE line in either face, chevron and
   padding included. TOC_ROW_MIN is the height floor a finger needs (44 pt); the
   mouse arm pays nothing for it. */
#define TOC_W_PX        300.0f
#define TOC_ROW_MIN     44

/* The gutter between the rail and the article: the article sits beside the rail
   and the slack falls to its right. */
#define PAGE_GUTTER_PX  64.0f

/* The toolbar's metrics. A chip is its label plus CHIP_PAD_X either side, which
   is what lets the header MEASURE its controls rather than guess a breakpoint.
   SEARCH_ONE_ROW_PX is both the width the field is GIVEN and the width the fit
   budget spends on it: price them differently and the toolbar stacks on the
   wrong frame. CODE_PAD_PX is the listing panel's padding, which the layout adds
   back when it sizes that panel to the widest line it holds. */
#define CHIP_H          30
#define CHIP_PAD_X      11
#define CHIP_TEXT_PX    13
#define HEADER_PAD_X    16
#define SEARCH_ONE_ROW_PX 240.0f
#define CODE_PAD_PX     14

#endif /* APP_THEME_H */
