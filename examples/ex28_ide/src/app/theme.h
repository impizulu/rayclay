/*
    theme.h - the two color themes, the metrics and the type scale

    A code editor is read for hours, so its colors are roles: Dark+ and Light+ fill one
    Palette of the same shape, token colors included, and every view reads the live one
    through theme(). A color is a brace initializer here, because rcRgb expands to a compound
    literal, which a static const may not hold in C.
*/
#ifndef APP_THEME_H
#define APP_THEME_H

#include "rayclay.h"

/* The type scale, in CSS px. Editors run small and dense: chrome at 13, the code at 14, tree
   section headings at 11. */
enum { SZ_MICRO = 11, SZ_SMALL = 12, SZ_UI = 13, SZ_CODE = 14, SZ_HEAD = 16 };

/* BAR_H is also RC_TitlebarOptions.height. HIT is the least a finger is asked to hit. */
enum {
    BAR_H = 36, BAR_RAIL_H = 6, ACTIVITY_W = 48, TAB_H = 36, CRUMB_H = 24, ROW_H = 22,
    LINE_H = 20, STATUS_H = 24, PANEL_TAB_H = 36, MINIMAP_W = 72, MINI_LINE = 3,
    NAV_H = 56, HIT = 44, PREVIEW_W = 320
};

/* What a piece of source is. tokenize_line in syntax.h assigns it, the theme colors it. */
enum {
    TOK_PLAIN, TOK_KEYWORD, TOK_CONTROL, TOK_STRING, TOK_NUMBER, TOK_COMMENT, TOK_FUNCTION,
    TOK_TYPE, TOK_CONSTANT, TOK_VARIABLE, TOK_PREPROC, TOK_HEADING, TOK_LINK, TOK_TAG,
    TOK_ATTR, TOK_COUNT
};

typedef struct {
    bool     dark;
    RC_Color title;      /* the title bar                                   */
    RC_Color activity;   /* the activity rail                               */
    RC_Color railOn;     /* the rail's selected icon                        */
    RC_Color railOff;    /* the rail's other icons                          */
    RC_Color sidebar;    /* explorer, search, outline                       */
    RC_Color editor;     /* behind the code                                 */
    RC_Color tabOff;     /* a background tab                                */
    RC_Color edge;       /* every hairline                                  */
    RC_Color hover;      /* a list row under the pointer                    */
    RC_Color active;     /* the selected list row                           */
    RC_Color pick;       /* the palette's chosen row                        */
    RC_Color input;      /* a field's well                                  */
    RC_Color accent;     /* focus, the active tab's top line, links         */
    RC_Color text;       /* chrome text                                     */
    RC_Color bright;     /* the active tab, a selected row                  */
    RC_Color muted;      /* captions, paths, inactive tabs                  */
    RC_Color gutter;     /* line numbers                                    */
    RC_Color gutterOn;   /* the caret line's number                         */
    RC_Color curLine;    /* the caret line's wash                           */
    RC_Color selection;  /* selected text                                   */
    RC_Color word;       /* other uses of the word under the caret          */
    RC_Color match;      /* a find hit                                      */
    RC_Color matchOn;    /* the current find hit                            */
    RC_Color bracket;    /* the pair at the caret                           */
    RC_Color guide;      /* indent guides                                   */
    RC_Color widget;     /* find bar and palette                            */
    RC_Color status;     /* the status bar, always the product's blue       */
    RC_Color statusText; /* words on the status bar                         */
    RC_Color statusOn;   /* a status item under the pointer                 */
    RC_Color error;
    RC_Color warning;
    RC_Color info;
    RC_Color badge;      /* a count's pill                                  */
    RC_Color syntax[TOK_COUNT];
} Palette;

static const Palette THEME_DARK = {
    .dark = true,
    .title = { 50, 50, 51, 255 }, .activity = { 44, 44, 45, 255 },
    .railOn = { 255, 255, 255, 255 }, .railOff = { 140, 140, 142, 255 },
    .sidebar = { 37, 37, 38, 255 }, .editor = { 30, 30, 30, 255 },
    .tabOff = { 45, 45, 45, 255 }, .edge = { 60, 60, 60, 255 },
    .hover = { 42, 45, 46, 255 }, .active = { 55, 55, 61, 255 },
    .pick = { 4, 57, 94, 255 },
    .input = { 60, 60, 60, 255 }, .accent = { 0, 122, 204, 255 },
    .text = { 204, 204, 204, 255 }, .bright = { 255, 255, 255, 255 },
    .muted = { 150, 150, 150, 255 }, .gutter = { 133, 133, 133, 255 },
    .gutterOn = { 198, 198, 198, 255 }, .curLine = { 42, 42, 42, 255 },
    .selection = { 38, 79, 120, 255 }, .word = { 58, 61, 65, 255 },
    .match = { 81, 92, 106, 255 },
    .matchOn = { 158, 106, 3, 255 }, .bracket = { 11, 115, 80, 255 },
    .guide = { 64, 64, 64, 255 }, .widget = { 37, 37, 38, 255 },
    .status = { 0, 122, 204, 255 }, .statusText = { 255, 255, 255, 255 },
    .statusOn = { 31, 139, 212, 255 }, .error = { 241, 76, 76, 255 },
    .warning = { 204, 167, 0, 255 }, .info = { 55, 148, 255, 255 },
    .badge = { 77, 77, 77, 255 },
    .syntax = {
        [TOK_PLAIN] = { 212, 212, 212, 255 }, [TOK_KEYWORD] = { 86, 156, 214, 255 },
        [TOK_CONTROL] = { 197, 134, 192, 255 }, [TOK_STRING] = { 206, 145, 120, 255 },
        [TOK_NUMBER] = { 181, 206, 168, 255 }, [TOK_COMMENT] = { 106, 153, 85, 255 },
        [TOK_FUNCTION] = { 220, 220, 170, 255 }, [TOK_TYPE] = { 78, 201, 176, 255 },
        [TOK_CONSTANT] = { 79, 193, 255, 255 }, [TOK_VARIABLE] = { 156, 220, 254, 255 },
        [TOK_PREPROC] = { 197, 134, 192, 255 }, [TOK_HEADING] = { 86, 156, 214, 255 },
        [TOK_LINK] = { 206, 145, 120, 255 }, [TOK_TAG] = { 86, 156, 214, 255 },
        [TOK_ATTR] = { 156, 220, 254, 255 },
    },
};

static const Palette THEME_LIGHT = {
    .dark = false,
    .title = { 221, 221, 221, 255 }, .activity = { 44, 44, 44, 255 },
    .railOn = { 255, 255, 255, 255 }, .railOff = { 150, 150, 150, 255 },
    .sidebar = { 243, 243, 243, 255 }, .editor = { 255, 255, 255, 255 },
    .tabOff = { 236, 236, 236, 255 }, .edge = { 212, 212, 212, 255 },
    .hover = { 232, 232, 232, 255 }, .active = { 228, 230, 241, 255 },
    .pick = { 214, 235, 255, 255 },
    .input = { 255, 255, 255, 255 }, .accent = { 0, 122, 204, 255 },
    .text = { 68, 68, 68, 255 }, .bright = { 20, 20, 20, 255 },
    .muted = { 110, 110, 110, 255 }, .gutter = { 35, 120, 147, 255 },
    .gutterOn = { 11, 33, 111, 255 }, .curLine = { 240, 240, 240, 255 },
    .selection = { 173, 214, 255, 255 }, .word = { 225, 229, 234, 255 },
    .match = { 200, 220, 170, 255 },
    .matchOn = { 244, 190, 90, 255 }, .bracket = { 180, 228, 210, 255 },
    .guide = { 217, 217, 217, 255 }, .widget = { 243, 243, 243, 255 },
    .status = { 0, 122, 204, 255 }, .statusText = { 255, 255, 255, 255 },
    .statusOn = { 31, 139, 212, 255 }, .error = { 205, 49, 49, 255 },
    .warning = { 148, 104, 0, 255 }, .info = { 26, 133, 255, 255 },
    .badge = { 196, 196, 196, 255 },
    .syntax = {
        [TOK_PLAIN] = { 0, 0, 0, 255 }, [TOK_KEYWORD] = { 0, 0, 255, 255 },
        [TOK_CONTROL] = { 175, 0, 219, 255 }, [TOK_STRING] = { 163, 21, 21, 255 },
        [TOK_NUMBER] = { 9, 134, 88, 255 }, [TOK_COMMENT] = { 0, 128, 0, 255 },
        [TOK_FUNCTION] = { 121, 94, 38, 255 }, [TOK_TYPE] = { 38, 127, 153, 255 },
        [TOK_CONSTANT] = { 0, 112, 193, 255 }, [TOK_VARIABLE] = { 0, 16, 128, 255 },
        [TOK_PREPROC] = { 175, 0, 219, 255 }, [TOK_HEADING] = { 0, 0, 128, 255 },
        [TOK_LINK] = { 163, 21, 21, 255 }, [TOK_TAG] = { 128, 0, 0, 255 },
        [TOK_ATTR] = { 228, 0, 0, 255 },
    },
};

static inline Palette app_palette(bool dark) {
    return dark ? THEME_DARK : THEME_LIGHT;
}

/* The bundled menus, fields, buttons and scrollbars read RC_Style, and so does a class's theme
   role (hover:bg-surface-alt is the palette's hover), so the palette fills it too. Installed
   between frames: a style change takes effect from the frame's first element. */
static inline RC_Style app_style(bool dark) {
    Palette  t = app_palette(dark);
    RC_Style s = dark ? rcStyleDark() : rcStyleLight();

    s.background   = t.editor;
    s.surface      = t.widget;
    s.surfaceAlt   = t.hover;
    s.chrome       = t.title;
    s.text         = t.text;
    s.textMuted    = t.muted;
    s.primary      = t.accent;
    s.primaryHover = t.statusOn;
    s.danger       = t.error;
    s.warning      = t.warning;
    s.border       = t.edge;
    s.radius       = 4.0f;
    return s;
}

/* The chrome's boxes light under the pointer by their class. Where the fill is the theme's
   hover role the class says bg-surface-alt; the palette's other fills have no role, and a class
   names a color literally, so each theme spells its palette's own: the command center's faint
   well (bright at 6 percent), a title bar chip's wash (bright at 11), a background tab
   (tabOff), a tab's close square and an unpicked segment (input). A typed .bg (a picked tab, a
   picked segment) beats the class, so only the unpicked state is written here. */
static inline void app_classes(bool dark) {
    rcDefineClass("cmd-pill", dark ? "bg-[#ffffff10] hover:bg-surface-alt"
                                   : "bg-[#14141410] hover:bg-surface-alt");
    rcDefineClass("bar-chip", dark ? "hover:bg-[#ffffff1c]" : "hover:bg-[#1414141c]");
    rcDefineClass("tab-rest", dark ? "bg-[#2d2d2d] hover:bg-surface-alt"
                                   : "bg-[#ececec] hover:bg-surface-alt");
    rcDefineClass("tab-close", dark ? "hover:bg-[#3c3c3c]" : "hover:bg-[#ffffff]");
    rcDefineClass("segment-rest", dark ? "bg-[#3c3c3c] hover:bg-surface-alt"
                                       : "bg-[#ffffff] hover:bg-surface-alt");
}

/* The live palette. apply_theme sets it, the style it fills and the classes in one call, so
   they cannot drift apart: main.c before the window opens, and the theme command between
   frames. */
static Palette g_theme;
static inline const Palette *theme(void) { return &g_theme; }

static inline void apply_theme(bool dark) {
    g_theme = app_palette(dark);
    rcSetStyle(app_style(dark));
    app_classes(dark);
}

#endif /* APP_THEME_H */
