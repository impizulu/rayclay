/*
    theme.h - this app's design tokens

    The Finder's colors, type scale and geometry, as its daily users know them: a light
    sidebar with blue glyphs, a white canvas, zebra rows, and the system's selection
    blue. One file to edit to restyle the app.
*/
#ifndef APP_THEME_H
#define APP_THEME_H

#include "rayclay.h"

/* BAR_H is also RC_TitlebarOptions.height. SIDE_W and INFO_W are the two panes around
   the view; a finger gets HIT of height wherever a mouse gets less. A column narrows from
   COLUMN_W to COLUMN_MIN before the preview, at least PREVIEW_MIN, pushes it off the left;
   PREVIEW_BLOCK caps what the preview holds, so a wide pane does not stretch it. */
enum { BAR_H = 52, SIDE_W = 208, INFO_W = 288, COLUMN_W = 232, COLUMN_MIN = 160, PREVIEW_MIN = 320,
       PREVIEW_BLOCK = 420, HIT = 44 };

/* Every control in the toolbar is this tall under a mouse, and HIT under a finger:
   tool_button's side, and h-8 or h-11 in the menus' and the search field's classes, so a
   ghost menu lines up with an icon button. */
enum { TOOL_H = 32 };

/* The band's width budget, in its own unzoomed pixels. BAND_LEAD is what the sidebar's
   button takes when no sidebar sits under the band; the window's controls take what
   rcWindowControlsWidth() says, none on the web or a phone. The folder's name keeps 128 px:
   with less, search folds to a magnifier, and then Info leaves for the More menu. */
enum { BAND_LEAD = 40, BAND_WITH_SEARCH = 676, BAND_WITH_INFO = 492 };

/* The sizes the app changes shape at, against the layout's: from WIDE_W the info pane fits
   beside the view, from SIDEBAR_W the sidebar stays open, and below COMPACT_W the view is
   the page: tall rows under the folder's name set large, the search on a row of its own.
   Under COMPACT_H tall, a phone on its side, the rows stay tall, the sidebar is a drawer and
   the columns are off as well, but the band keeps the name and, where it fits, the search.
   PATH_H is the least height that keeps the path bar. */
#define WIDE_W    1100.0f
#define SIDEBAR_W  760.0f
#define COMPACT_W  560.0f
#define COMPACT_H  440.0f
#define PATH_H     480.0f

/* The list's columns. NAME_MIN is the least the name keeps; the rest leave in the order
   Kind, Size, Date as the view narrows, and a "+N columns" tip counts what left. */
enum { NAME_MIN = 220, DATE_W = 176, SIZE_W = 84, KIND_W = 136 };

/* Icon view: the slider's range, and what a tile adds around the icon. */
#define ICON_MIN 48.0f
#define ICON_MAX 128.0f
enum { TILE_PAD_W = 48, TILE_PAD_H = 60 };

/* The type scale. RC_AppOptions.fontSizes[0] is TEXT_BODY, so every library widget reads
   at the Finder's density. */
enum { TEXT_CAPTION = 11, TEXT_SMALL = 12, TEXT_BODY = 13, TEXT_ROOMY = 14, TEXT_TITLE = 15,
       TEXT_INITIAL = 24 };

/* Code and plain text preview in this face, read the first time one is shown. */
#ifndef RC_EX15_FONT_DIR
    #define RC_EX15_FONT_DIR "examples/assets/fonts/"
#endif
#define FONT_MONO_TTF RC_EX15_FONT_DIR "AdwaitaMono-Regular.ttf"

/* The panes' colors. One that carries words clears 4.5:1 (WCAG AA) on the ground it is drawn
   on: ink and muted on canvas and zebra, muted on the sidebar, white on select. The washes
   under the pointer and the fills of what is chosen are classes (app_classes, below). */
typedef struct {
    RC_Color ink, muted, faint;
    RC_Color canvas, zebra, line, barBg, sidebarBg, well, paperLine;
    RC_Color hover, tileOn;
    RC_Color accent, select, segTrack;
} Palette;

static inline Palette app_palette(void) {
    Palette p;

    p.ink       = rcRgb( 29,  29,  31);
    p.muted     = rcRgb( 99,  99, 102);
    p.faint     = rcRgb(142, 142, 147);
    p.canvas    = rcRgb(255, 255, 255);
    p.zebra     = rcRgb(245, 245, 247);
    p.line      = rcRgb(222, 222, 225);
    p.barBg     = rcRgb(250, 250, 251);
    p.sidebarBg = rcRgb(236, 235, 239);
    p.well      = rcRgb(246, 246, 248);
    p.paperLine = rcRgb(214, 214, 219);
    p.hover     = rcRgb(0, 0, 0, 10 / 255.0f);
    p.tileOn    = rcRgb(0, 0, 0, 22 / 255.0f);
    p.accent    = rcRgb(  0, 122, 255);
    p.select    = rcRgb(  0,  99, 225);
    p.segTrack  = rcRgb(235, 235, 238);
    return p;
}

/* The style the library's own widgets read: menus, fields, buttons, scrollbars and the
   modal panel, derived from the same colors so they cannot drift from the panes. */
static inline RC_Style app_style(void) {
    const Palette p = app_palette();
    RC_Style      s = rcStyleLight();

    s.background   = p.canvas;
    s.surface      = p.canvas;
    s.surfaceAlt   = p.zebra;
    s.chrome       = p.barBg;
    s.text         = p.ink;
    s.textMuted    = p.muted;
    s.primary      = p.select;
    s.primaryHover = rcRgb(0, 86, 196);
    s.border       = p.line;
    s.radius       = 6.0f;
    return s;
}

/* The looks the panes repeat, named once at start-up. A state prefix reads the box's own
   state, so a row lights under the pointer with no rcShowsHover branch, and what the app
   chose, the open place or a toggled tool, picks one name or the other in C. A row of the
   list writes "zebra wash-blue": a name's hover: gives way to the names after it, so the one
   that holds a state goes last. The selection's blue and the zebra are the style's own
   primary and surfaceAlt (bg-primary, bg-surface-alt), which follow rcSetStyle. */
static inline void app_classes(void) {
    rcDefineClass("wash", "hover:bg-[#0000000a]");          /* the palette's hover */
    rcDefineClass("wash-blue", "hover:bg-[#ecf0f7]");       /* a list row, an icon button */
    rcDefineClass("zebra", "bg-surface-alt");               /* every other row of the list */
    rcDefineClass("chosen", "bg-[#dcdce0]");                /* a toggled tool, the open path */
    rcDefineClass("side-wash", "hover:bg-[#0000000c]");     /* a sidebar entry */
    rcDefineClass("side-chosen", "bg-[#00000018]");         /* the entry the user is inside */
}

/* The live palette, set with the style in main() before the first frame. */
static Palette ui;

#endif /* APP_THEME_H */
