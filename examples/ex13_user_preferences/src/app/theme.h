/*
    theme.h - the design tokens, and the one function that turns settings into a style

    Nothing draws a color of its own: every surface, text and accent comes from
    rcGetStyle() at the moment it is drawn, or from a class that names one of its roles
    (bg-surface, text-foreground), so Base color scheme and Accent color restyle the
    whole window by installing one RC_Style per frame, and Corner radius by naming the
    corners again (app_corners).

    A color is a call, a #define or a brace initializer: rcRgb and rcHex are not
    constant expressions in C, so a file-scope static const cannot take one.
*/
#ifndef APP_THEME_H
#define APP_THEME_H

#include "rayclay.h"

/* The type scale. Slot 0 of RC_AppOptions.fontSizes is T_BODY, because the bundled
   controls draw their text in slot 0. */
enum { T_CHIP = 12, T_HINT = 13, T_BODY = 14, T_NAME = 15, T_HEAD = 16, T_TITLE = 20 };
enum { T_NOTE_H2 = 22, T_NOTE_TITLE = 30 };

/* The window: a title band, then the host app, with the settings dialog above them. */
enum { BAR_H = 40, RIBBON_W = 48, FILES_W = 240, OUTLINE_W = 216, STATUS_H = 28 };

/* The settings dialog. Below DIALOG_MIN_W or DIALOG_MIN_H it stops being a dialog and
   fills the window; below SPLIT_MIN_W the rail and the page take turns, as a phone's
   settings do. Filling the window, RAIL_GAP parts the rail from the page; in the dialog
   the page runs to the panel's edge and the rail keeps RAIL_PAD all round. */
enum { DIALOG_MAX_W = 1120, DIALOG_MAX_H = 880, DIALOG_MIN_W = 760, DIALOG_MIN_H = 540 };
enum { DIALOG_FULL_W = 820, SPLIT_MIN_W = 600, RAIL_W = 208, RAIL_GAP = 16, RAIL_PAD = 12 };

/* The title band: what its buttons and the "+N" tab chip take from the tabs, and a tab's
   width without its close button at the narrowest it still names its note. */
enum { BAND_CHROME_W = 280, TAB_MIN_W = 92 };

/* A settings row: its name and line on the left, its control on the right, down to a
   pane of ROW_STACK_W, where the control moves under the words. */
enum { ROW_STACK_W = 500, SELECT_W = 200, SLIDER_W = 180, SLIDER_VALUE_W = 56, FIELD_W = 280 };

/* Every control in a row is one height: 32 under a mouse, a finger's 44 on a phone. An h-*
   in .className is exact on a button, a field and a combo alike. */
static inline const char *control_class(void) {
    return rcPointerIsCoarse() ? "h-11" : "h-8";
}

/* Whether the accent is pale enough that white on it would not read: it takes dark ink. */
static inline bool accent_is_pale(void) {
    RC_Color a = rcGetStyle().primary;

    return a.r * 0.299f + a.g * 0.587f + a.b * 0.114f > 170.0f;
}

/* A primary button: a control's height, and on a pale accent the dark ink of
   Palette.accentInk over the button's own white label. */
static inline const char *primary_class(void) {
    static const char *const CLASS[2][2] = {
        { "h-8", "h-8 text-[#141414]" }, { "h-11", "h-11 text-[#141414]" },
    };

    return CLASS[rcPointerIsCoarse()][accent_is_pale()];
}

static inline RC_Color color_mix(RC_Color a, RC_Color b, float t) {
    return rcRgb(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
                  (a.a + (b.a - a.a) * t) / 255.0f);
}

/* THE CORNERS FOLLOW THE CORNER RADIUS SETTING. A bundled control derives its own from
   RC_Style.radius; .borderRadius is a fixed string, so an app's box takes them as a class,
   by one of five names: a card rounds at the style's radius, and an item, what sits in a
   card, at three quarters of that, the ratio the bundled controls use; a tab rounds its top
   corners only, at an item's size, and a task's tick box at half an item's. The dialog's
   page, a sheet, rounds its right corners only, at the panel's own: rcBeginModal's XL
   corner, 1.5 x the radius. */
static inline const char *corner_class(float px) {
    static const char *const CLASS[] = {
        "rounded-[0px]", "rounded-[1px]", "rounded-[2px]", "rounded-[3px]", "rounded-[4px]",
        "rounded-[5px]", "rounded-[6px]", "rounded-[7px]", "rounded-[8px]", "rounded-[9px]",
        "rounded-[10px]", "rounded-[11px]", "rounded-[12px]", "rounded-[13px]", "rounded-[14px]",
        "rounded-[15px]", "rounded-[16px]",
    };
    int i = (int)(px + 0.5f);

    return CLASS[i < 0 ? 0 : i > 16 ? 16 : i];
}

/* The slider's step is 2, so this table has one rung for each stop. */
static inline const char *tab_class(float radius) {
    static const char *const CLASS[] = {
        "rounded-t-[0px]", "rounded-t-[2px]", "rounded-t-[3px]", "rounded-t-[5px]",
        "rounded-t-[6px]", "rounded-t-[8px]", "rounded-t-[9px]", "rounded-t-[11px]",
        "rounded-t-[12px]",
    };
    int i = (int)(radius * 0.5f + 0.5f);

    return CLASS[i < 0 ? 0 : i > 8 ? 8 : i];
}

static inline const char *sheet_class(float radius) {
    static const char *const CLASS[] = {
        "rounded-r-[0px]", "rounded-r-[3px]", "rounded-r-[6px]", "rounded-r-[9px]",
        "rounded-r-[12px]", "rounded-r-[15px]", "rounded-r-[18px]", "rounded-r-[21px]",
        "rounded-r-[24px]",
    };
    int i = (int)(radius * 0.5f + 0.5f);

    return CLASS[i < 0 ? 0 : i > 8 ? 8 : i];
}

/* A new radius is a theme switch, the moment rcDefineClass is for: the five names are
   defined again, and every box that names one follows. update() asks each frame. */
static inline void app_corners(float radius) {
    static float named = -1.0f;   /* the radius the names hold */

    if (radius == named) return;
    named = radius;
    rcDefineClass("card", corner_class(radius));
    rcDefineClass("item", corner_class(radius * 0.75f));
    rcDefineClass("tab", tab_class(radius));
    rcDefineClass("sheet", sheet_class(radius));
    rcDefineClass("tick", corner_class(radius * 0.375f));
}

/* Whether the installed style is a dark one, whatever Base color scheme says: System
   follows the OS. */
static inline bool style_is_dark(void) {
    RC_Color c = rcGetStyle().background;
    return c.r + c.g + c.b < 384.0f;
}

static inline RC_Style app_style(bool dark, RC_Color accent, float radius) {
    RC_Style s = dark ? rcStyleDark() : rcStyleLight();

    if (dark) {
        s.background = rcHex(0x1b1b1b);
        s.surface    = rcHex(0x2a2a2a);
        s.surfaceAlt = rcHex(0x3a3a3a);
        s.chrome     = rcHex(0x151515);
        s.text       = rcHex(0xdedede);
        s.textMuted  = rcHex(0xa0a0a0);
        s.border     = rcHex(0x3e3e3e);
    } else {
        s.background = rcHex(0xeeeeee);
        s.surface    = rcHex(0xffffff);
        s.surfaceAlt = rcHex(0xf1f1f1);
        s.chrome     = rcHex(0xe4e4e4);
        s.text       = rcHex(0x222222);
        s.textMuted  = rcHex(0x666666);
        s.border     = rcHex(0xd8d8d8);
    }
    s.primary      = accent;
    s.primaryHover = color_mix(accent, RC_WHITE, 0.14f);
    s.radius       = radius;
    return s;
}

/* THE LOOKS RC_Style HAS NO FIELD FOR, as classes over the roles it has, so they follow a
   theme change as its own colors do. Each lays the text color over whatever is under it:
   a lit button, tab or list row at 8%; a lit row of a card at 5%, fainter, so a switch's
   off track still shows on it; the rail's page and the file list's open note at 15%. A box
   names one after its corners ("item lit"): the name that holds a state goes last. main()
   defines them once, at start-up. */
static inline void app_classes(void) {
    rcDefineClass("lit", "hover:bg-foreground/8");
    rcDefineClass("lit-row", "hover:bg-foreground/5");
    rcDefineClass("selected", "bg-foreground/15");
}

/* THE INKS RC_Style HAS NO FIELD FOR, derived from the installed style, so they follow a
   theme change as the style's own colors do. */
typedef struct {
    RC_Color accentInk;   /* on the accent: a pale accent takes dark ink              */
    RC_Color warnInk;     /* text beside a warning, in a shade that reads on the page */
    RC_Color goodInk;     /* and beside a success                                     */
} Palette;

static inline Palette app_palette(void) {
    bool    dark = style_is_dark();
    Palette p;

    p.accentInk = accent_is_pale() ? rcHex(0x141414) : RC_WHITE;
    p.warnInk   = dark ? RC_AMBER_400 : RC_AMBER_800;
    p.goodInk   = dark ? RC_EMERALD_400 : RC_EMERALD_700;
    return p;
}

#endif /* APP_THEME_H */
