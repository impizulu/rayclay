/*
    notes_theme.h - this app's design tokens: the palette, the reading type ramp and
    the column widths the layout is derived from. Values only: edit this file to
    restyle the app without reading the layout.

    The look is Bear's: a graphite sidebar, warm paper beside it, one red for what can
    be clicked. A writing tool is judged as a page before it is judged as a program.
*/
#ifndef NOTES_THEME_H
#define NOTES_THEME_H

#include "rayclay.h"

/* Every call site reads a ROLE, so both themes are the same struct. CONTRAST IS A
   MEASUREMENT, on every ground a role sits on: muted ink clears 4.5:1 on the selected
   row, a code comment on the code fill, the link red on a hovered row. When a color
   misses, darken it on paper and lighten it at night; never the other way round. The
   accent is for marks; text in red is `link`. */
typedef struct {
    RC_Color sidebar;        /* the graphite column                                */
    RC_Color sidebarHover;   /* a row and a ghost button under the pointer there   */
    RC_Color sidebarInk;
    RC_Color sidebarMuted;
    RC_Color list;           /* the note list, a step deeper than the page         */
    RC_Color paper;          /* the page                                           */
    RC_Color ink;            /* headings and body text                             */
    RC_Color inkSoft;        /* a preview line, a quote                            */
    RC_Color inkMuted;       /* dates, counts, a status bar                        */
    RC_Color rule;           /* hairlines                                          */
    RC_Color accent;         /* the one red: the selection bar, bullets, a pin     */
    RC_Color link;           /* the same red where it is text on paper             */
    RC_Color markFill;       /* ==highlighted== text                               */
    RC_Color codeFill;       /* a fence's panel and an inline code chip            */
    RC_Color tableHead;      /* a table's header row                               */
    RC_Color tableZebra;     /* its even rows                                      */
    RC_Color control;        /* the well a segmented control sits in               */
    RC_Color toast;          /* the undo toast's fill                              */
    RC_Color toastInk;
    /* A callout's bar and fill, one pair per kind. */
    RC_Color noteBar, noteFill;
    RC_Color tipBar, tipFill;
    RC_Color warnBar, warnFill;
    RC_Color dangerBar, dangerFill;
    /* Syntax colors, on codeFill. */
    RC_Color codeInk, codeComment, codeKeyword, codeString, codeNumber, codeType, codeFunc;
} NotesPalette;

static inline NotesPalette notes_palette(bool dark) {
    NotesPalette t;

    t.sidebar       = rcHex(dark ? 0x191817 : 0x2e2c2a);
    t.sidebarHover  = rcHex(dark ? 0x272523 : 0x3a3835);
    t.sidebarInk    = rcHex(0xeae6df);
    t.sidebarMuted  = rcHex(dark ? 0x9b968e : 0xaeaaa2);
    t.list          = rcHex(dark ? 0x201f1d : 0xf6f3ec);
    t.paper         = rcHex(dark ? 0x262422 : 0xfdfbf7);
    t.ink           = rcHex(dark ? 0xece8e1 : 0x2b2926);
    t.inkSoft       = rcHex(dark ? 0xc6c1b9 : 0x57534c);
    t.inkMuted      = rcHex(dark ? 0xa39e96 : 0x69645d);
    t.rule          = rcHex(dark ? 0x3b3835 : 0xe5e0d6);
    t.accent        = rcHex(dark ? 0xe8655b : 0xd5433a);
    t.link          = rcHex(dark ? 0xf0837a : 0xbd362d);
    t.markFill      = rcHex(dark ? 0x5a4a1c : 0xfbe99a);
    t.codeFill      = rcHex(dark ? 0x1e1d1b : 0xf4f0e8);
    t.tableHead     = rcHex(dark ? 0x302e2b : 0xf1ede4);
    t.tableZebra    = rcHex(dark ? 0x2b2927 : 0xf9f6f0);
    t.control       = rcHex(dark ? 0x35322f : 0xece8df);
    t.toast         = rcHex(dark ? 0xece8e1 : 0x2e2c2a);
    t.toastInk      = rcHex(dark ? 0x2b2926 : 0xf6f3ec);
    t.noteBar       = rcHex(dark ? 0x6ba3e0 : 0x3b78c2);
    t.noteFill      = rcHex(dark ? 0x25303b : 0xeef3f9);
    t.tipBar        = rcHex(dark ? 0x7dbb6a : 0x4b8b3b);
    t.tipFill       = rcHex(dark ? 0x28332a : 0xeff5ea);
    t.warnBar       = rcHex(dark ? 0xe8a33d : 0xe08a1e);
    t.warnFill      = rcHex(dark ? 0x3a3020 : 0xfdf5e8);
    t.dangerBar     = rcHex(dark ? 0xe8655b : 0xd5433a);
    t.dangerFill    = rcHex(dark ? 0x3b2624 : 0xfcefed);
    t.codeInk       = rcHex(dark ? 0xdcd7cf : 0x3a3733);
    t.codeComment   = rcHex(dark ? 0x8f8a82 : 0x6d6860);
    t.codeKeyword   = rcHex(dark ? 0xd98cc9 : 0x9c2c7c);
    t.codeString    = rcHex(dark ? 0x9cc98a : 0x3c7320);
    t.codeNumber    = rcHex(dark ? 0xe0a56b : 0xa8520c);
    t.codeType      = rcHex(dark ? 0x7fb6e8 : 0x1f62a8);
    t.codeFunc      = rcHex(dark ? 0xe3c079 : 0x7a5b0a);
    return t;
}

/* THE PALETTE'S LOOKS AS CLASSES. A class string is resolved once and cached, so it cannot
   read the palette each frame as `.color = t->ink` does: the looks a class names are
   defined here for each theme, and notes_install_theme redefines them when the theme
   changes. A color the palette shares with RC_Style needs no name, because a role token
   (bg-surface, text-primary) follows the installed style (notes_style). */
static inline void notes_define_classes(bool dark) {
    /* inkSoft, for the glyph of an icon button and the label of a ghost one. */
    rcDefineClass("ink-soft", dark ? "text-[#c6c1b9]" : "text-[#57534c]");
    /* A row of the graphite column under the pointer (sidebarHover), and selected. */
    rcDefineClass("side-row", dark ? "hover:bg-[#272523]" : "hover:bg-[#3a3835]");
    rcDefineClass("side-row-on", dark ? "bg-[#343230]" : "bg-[#4b4845]");
    /* The graphite column's scrollbar in the column's colors, where the theme's bar would
       draw the page's: sidebarHover for its track, sidebarMuted for its thumb, sidebarInk
       with the pointer on the thumb and the accent while it is held. */
    rcDefineClass("side-scrollbar", dark ? "bg-[#272523] accent-[#9b968e] "
                                           "hover:accent-[#eae6df] active:accent-primary"
                                         : "bg-[#3a3835] accent-[#aeaaa2] "
                                           "hover:accent-[#eae6df] active:accent-primary");
    /* A row of the note list, the backlinks and the contents, as the side rows. */
    rcDefineClass("list-row", dark ? "hover:bg-[#2a2826]" : "hover:bg-[#efebe2]");
    rcDefineClass("list-row-on", dark ? "bg-[#33302d]" : "bg-[#e9e4d9]");
    /* A tag pill in the page. */
    rcDefineClass("tag-pill", dark ? "bg-[#3a3734]" : "bg-[#ece7dd]");
}

/* THE TYPE RAMP: what the text-size control selects. A whole ramp per step rather
   than a multiplier, so the heading-to-body relationship is decided here. Each (face,
   size) pair takes one of 16 font slots the first time it draws, so the sizes recur
   across steps and the chrome shares them: regular 13 14 16 18, bold 14 16 18 20 24 32,
   italic 14 16 18, mono 13 14. Fifteen pairs, with every step walked in one run. */
typedef struct {
    uint16_t h1, h2, h3, body, code;
    uint16_t bodyLine, codeLine;
} NotesTypeStep;

static const NotesTypeStep NOTES_TYPE_STEPS[] = {
    { 24, 20, 16, 14, 13, 22, 20 },
    { 32, 24, 20, 16, 13, 26, 21 },
    { 32, 24, 20, 18, 14, 29, 22 },
};

/* THE READING COLUMN IS DERIVED FROM THE TYPE, not picked in pixels: a pixel cap is a
   guess about a font. These are characters per line, inside the 45 to 75 that
   typographers ask for, and notes_measure_px turns them into a width at the current
   size, so a larger step buys a wider column to hold the same count. */
static const float NOTES_MEASURES[] = { 56.0f, 68.0f, 75.0f };

/* The columns. The editor is the pane that must not lose width, so the sidebar gives
   way first, then the list, and below NOTES_ONE_PANE_W the panes take turns. */
#define NOTES_SIDEBAR_W   240.0f
#define NOTES_LIST_W      304.0f
#define NOTES_LIST_SLIM_W 272.0f
#define NOTES_EDITOR_MIN  600.0f
#define NOTES_ONE_PANE_W  720.0f
#define NOTES_TOC_W       264.0f
#define NOTES_SHORT_H     480.0f   /* below this height the editor has no status bar */

#endif /* NOTES_THEME_H */
