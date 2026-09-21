/*
    main.c - RayClay "1990s" example (Windows 95 "Display Properties" dialog)

    A period-accurate settings dialog: battleship-silver surfaces, hard SQUARE
    corners, two-tone 3D bevels (light top-left, dark bottom-right), a solid-blue
    caption bar, and a three-tab property sheet whose monitor preview answers
    every control on the page. Same source -> desktop, web and phone.

    Shows: a custom RC_Style, hand-drawn chrome wired to the runner through the
    RC_ID_WINDOW_* ids, combos, radios, a checkbox, a slider, and an
    OK / Cancel / Apply row that commits or discards a snapshot. Zero-asset -
    the bundled face is baked at startup, the caption glyphs are procedural.

    Build target: rayclay_ex02_1990s_gui
*/

#include "rayclay.h"

/* Monochrome control glyphs from the shared example assets (compiled in). */
#include "icons/rc_icons_minus.h"
#include "icons/rc_icons_maximize.h"
#include "icons/rc_icons_shrink.h"   /* the restore glyph, when the window IS maximised */
#include "icons/rc_icons_x.h"

/* Font ladder baked from the bundled face at these sizes - zero-asset. */
typedef enum { F_BODY = 0, F_TITLE, F_COUNT } AppFont;

typedef struct {
    int  tab;           /* 0=Background 1=Screen Saver 2=Appearance    */

    /* Background tab */
    int  wallpaper;     /* combo selection                             */
    int  placement;     /* radio group: 0=Center 1=Tile 2=Stretch      */
    bool pattern;       /* checkbox                                    */

    /* Screen Saver tab */
    int   saver;        /* combo selection                            */
    float wait;         /* slider minutes (1..60)                     */
    bool  password;     /* checkbox                                   */

    /* Appearance tab */
    int   scheme;       /* combo selection                            */
    int   item;         /* combo selection                            */
    float itemSize;     /* slider (0..30)                             */

    bool  applied;      /* Apply acknowledged; any control change clears it */
} AppState;

/* A property sheet edits a WORKING COPY and keeps the last committed one beside
   it, which is what lets Cancel mean "discard" rather than "close". */
typedef struct {
    AppState live;
    AppState committed;
} AppSheet;

/* Win95 silver bevel tones. Kept local so the whole 3D look tunes in one place. */
#define BEVEL_LIGHT RC_GRAY_100   /* highlight edge (top-left)     */
#define BEVEL_DARK  RC_GRAY_600   /* shadow edge (bottom-right)    */
#define BEVEL_FACE  RC_GRAY_300   /* the silver control face       */

/* A 1995 radio, hand-rolled. RC_Style.radius = 0 squares EVERY control here,
   radio rings included, so rcRadio would be pixel-identical to rcCheckbox - the
   one distinction a settings dialog cannot lose. The "rounded-[Npx]" class takes
   a pixel radius straight rather than scaling RC_Style.radius, so it gives a true
   circle without touching the theme every other control depends on. */
static bool win95_radio(const char *id, const char *label, int *selected, int index) {
    bool on = (*selected == index);

    rcRow(.id = id, .gap = 7, .align = "cl") {
        /* The dot is declared in BOTH states - transparent when this option is
           not the chosen one - so picking a different option moves no pixel
           except the dot itself. */
        rcBox(.bg = BEVEL_DARK, .align = "cc", .w = "16px", .h = "16px",
              .className = "rounded-[8px]") {
            rcBox(.bg = RC_WHITE, .align = "cc", .w = "14px", .h = "14px",
                  .className = "rounded-[7px]") {
                rcBox(.bg = on ? RC_BLACK : RC_TRANSPARENT, .w = "6px", .h = "6px",
                      .className = "rounded-[3px]") {}
            }
        }
        rcTextC(label, .font = F_BODY, .color = RC_BLACK);
    }
    /* The whole row is the target, label included, as the real control was. */
    if (rcClicked(id)) {
        *selected = index;
        return true;
    }
    return false;
}

/* A two-tone 3D bevel cannot be one border (that draws a uniform ring), so it is
   NESTED and each ring is ONE-SIDED via padding: the outer box pads TOP+LEFT
   only, so its highlight tone shows on just those two edges; the inner box pads
   BOTTOM+RIGHT only, so its shadow tone shows there; the silver face sits on
   top. `dflt` draws the 1px black ring a Win95 dialog gave its default action -
   a transparent ring on the others, so all three keep the same geometry. */
static bool bevel_button(const char *id, const char *label, bool dflt) {
    /* HELD, not hovered: a 1995 button depressed under the press rather than
       lighting up under the pointer, so rcIsHovered plus rcPointerDown is the
       pair that matches the era, and the swapped tones are the depressed look. */
    bool     in = rcIsHovered(id) && rcPointerDown(RC_POINTER_LEFT);
    RC_Color tl = in ? BEVEL_DARK  : BEVEL_LIGHT;   /* top-left edge     */
    RC_Color br = in ? BEVEL_LIGHT : BEVEL_DARK;    /* bottom-right edge */

    rcBox(.p = 1, .border = { .color = dflt ? RC_BLACK : RC_TRANSPARENT,
                              .width = "1px" }) {
        rcBox(.id = id, .bg = tl, .pt = 2, .pl = 2) {
            rcBox(.bg = br, .pb = 2, .pr = 2, .w = "grow", .h = "grow") {
                rcBox(.bg = BEVEL_FACE, .px = 12, .py = 5, .align = "cc",
                      .w = "grow", .h = "grow") {
                    rcTextC(label, .font = F_BODY, .color = RC_BLACK,
                            .textAlign = "c");
                }
            }
        }
    }
    return rcClicked(id);
}

/* A property-sheet tab. Every tab is RAISED - a sunken one reads as a pressed
   button - and the selected one stands three pixels taller with no bottom
   shadow. THE STRIP, NOT THE PAGE, DRAWS THE PAGE'S TOP EDGE, so the selected
   tab can punch a hole in that line and its face and the page's face become one
   surface. Both arms come to the same height whichever tab is chosen. */
static bool tab_button(const char *id, const char *label, bool active) {
    rcColumn(.id = id) {
        if (!active) rcBox(.w = "grow", .h = "3px") {}
        rcBox(.bg = BEVEL_LIGHT, .pt = 2, .pl = 2, .w = "grow") {
            rcBox(.bg = BEVEL_DARK, .pb = (uint16_t)(active ? 0 : 2), .pr = 2,
                  .w = "grow", .h = "grow") {
                rcBox(.bg = BEVEL_FACE, .px = 12, .py = 5, .align = "cc",
                      .w = "grow", .h = "grow") {
                    rcTextC(label, .font = F_BODY, .color = RC_BLACK);
                }
            }
        }
        rcBox(.bg = active ? BEVEL_FACE : BEVEL_DARK, .w = "grow",
              .h = active ? "7px" : "2px") {}
    }
    return rcClicked(id);
}

/* The page's top edge where there is no tab: between two of them, and on past
   the last one to the right-hand side of the sheet. */
static void tab_edge(const char *w) {
    rcColumn(.w = w, .h = "grow") {
        rcBox(.w = "grow", .h = "grow") {}
        rcBox(.bg = BEVEL_DARK, .w = "grow", .h = "2px") {}
    }
}

/* A 16x14 raised silver square with a black glyph, tagged with a window-control
   id so the runner performs the OS action. Hand-drawn chrome polls hover rather
   than rcClicked, so it names the pointer cursor itself. */
static void caption_button(const char *winId, RC_IconCallback icon) {
    bool over = rcIsHovered(winId);
    if (over) rcSetCursor(RC_CURSOR_POINTER);
    bool in = over && rcPointerDown(RC_POINTER_LEFT);

    rcBox(.id = winId, .bg = in ? BEVEL_DARK : BEVEL_LIGHT, .pt = 1, .pl = 1) {
        rcBox(.bg = in ? BEVEL_LIGHT : BEVEL_DARK, .pb = 1, .pr = 1) {
            rcBox(.bg = BEVEL_FACE, .align = "cc", .w = "14px", .h = "12px") {
                icon(8.0f, RC_BLACK);
            }
        }
    }
}

static void field_label(const char *label) {
    rcTextC(label, .font = F_BODY, .color = RC_BLACK);
}

/* A Display Properties box was never resizable, so the content well keeps its
   own width and centres; only the chrome, the tab strip and the action row
   span, which is what a dialog stretched by a window manager looks like. */
#define BODY_MAX 440.0f

/* The preview glass. Every scene is sized against these two, so the CRT can be
   resized without re-tuning any of them. */
#define SCREEN_W "192px"
#define SCREEN_H "144px"

/* The desktop colour each wallpaper stands for. A name and a colour is the whole
   model: an example about a 1995 property sheet should not grow an image loader. */
static RC_Color wall_color(int wallpaper) {
    switch (wallpaper) {
    case 1:  return RC_SKY_300;       /* Clouds  */
    case 2:  return RC_CYAN_400;      /* Bubbles */
    case 3:  return RC_INDIGO_400;    /* Setup   */
    case 4:  return RC_EMERALD_600;   /* Forest  */
    case 5:  return RC_BLUE_500;      /* Waves   */
    default: return RC_TEAL_700;      /* (None): the bare desktop              */
    }
}

/* The caption colour each Appearance scheme is known by. */
static RC_Color scheme_color(int scheme) {
    switch (scheme) {
    case 1:  return RC_AMBER_700;     /* Desert    */
    case 2:  return RC_PURPLE_800;    /* Eggplant  */
    case 3:  return RC_ROSE_700;      /* Rose      */
    case 4:  return RC_TEAL_700;      /* Teal (VGA) */
    default: return RC_BLUE_900;      /* Windows Standard */
    }
}

/* One wallpaper block, at the size the placement asks for. */
static void wall_block(const char *w, const char *h, RC_Color c) {
    rcBox(.bg = c, .w = w, .h = h) {}
}

/* What is on the glass, by tab: three small scenes, each a direct reading of the
   controls under it, so no control on this dialog is decoration. */
static void monitor_screen(const AppState *st) {
    RC_Color desk = st->pattern ? RC_TEAL_800 : RC_TEAL_900;

    if (st->tab == 1) {
        /* A screen saver has taken the screen, unless the choice is (None), in
           which case the desktop is what you would still be looking at. */
        if (st->saver != 0) {
            rcColumn(.bg = RC_BLACK, .gap = 10, .align = "cc",
                     .w = SCREEN_W, .h = SCREEN_H) {
                rcRow(.gap = 7, .align = "cc") {
                    rcBox(.bg = RC_SKY_300, .w = "6px", .h = "6px") {}
                    rcBox(.bg = RC_WHITE, .w = "10px", .h = "10px") {}
                    rcBox(.bg = RC_SKY_500, .w = "4px", .h = "4px") {}
                }
                /* Password protected: the saver dismisses to a sign-in box, so
                   the preview shows one. */
                if (st->password) {
                    rcBox(.bg = BEVEL_DARK, .p = 2, .align = "cc",
                          .w = "64px", .h = "14px") {
                        rcBox(.bg = RC_WHITE, .w = "grow", .h = "grow") {}
                    }
                }
            }
            return;
        }
    }
    if (st->tab == 2) {
        /* Scheme picks the caption colour, Item picks WHICH part Size applies
           to, and only the chosen part takes a share of the slider - so all
           three controls move something, and the multipliers keep every part on
           the glass at the top of its range. */
        float sz    = st->itemSize;
        float capH  = st->item == 1 ? 10.0f + sz * 0.55f : 14.0f;
        float menuH = st->item == 2 ?  7.0f + sz * 0.37f : 11.0f;
        float bord  = st->item == 3 ?  1.0f + sz * 0.16f :  2.0f;
        float iconS = st->item == 4 ? 12.0f + sz * 0.60f : 17.0f;
        float deskP = st->item == 0 ?  5.0f + sz * 0.33f : 12.0f;

        rcColumn(.bg = desk, .gap = 5, .p = (uint16_t)deskP, .align = "tl",
                 .w = SCREEN_W, .h = SCREEN_H) {
            /* The desktop icon: the fifth item, and the only one that lives on
               the desktop rather than in the window. */
            rcBox(.bg = RC_SKY_300, .wType = RC_PX(iconS), .hType = RC_PX(iconS)) {}
            /* The window: caption, menu strip, body, inside a border whose
               thickness is the "Window" item. */
            rcColumn(.bg = BEVEL_DARK, .gap = 0, .p = (uint16_t)bord,
                     .w = "grow", .h = "grow") {
                rcRow(.bg = scheme_color(st->scheme), .px = 4, .align = "cl",
                      .w = "grow", .hType = RC_PX(capH)) {
                    rcTextL("Window", .font = F_BODY, .color = RC_WHITE);
                }
                rcRow(.bg = BEVEL_FACE, .gap = 6, .px = 3, .align = "cl",
                      .w = "grow", .hType = RC_PX(menuH)) {
                    rcBox(.bg = BEVEL_DARK, .w = "10px", .h = "1px") {}
                    rcBox(.bg = BEVEL_DARK, .w = "14px", .h = "1px") {}
                }
                rcBox(.bg = BEVEL_FACE, .w = "grow", .h = "grow") {}
            }
        }
        return;
    }
    /* Background: the desktop, then the wallpaper laid on it the chosen way. */
    rcColumn(.bg = desk, .gap = 5, .align = "cc", .w = SCREEN_W, .h = SCREEN_H) {
        if (st->wallpaper == 0)
            return;
        if (st->placement == 2) {              /* Stretch: it is the desktop */
            wall_block("grow", "grow", wall_color(st->wallpaper));
        } else if (st->placement == 1) {       /* Tile: two rows of three     */
            for (int row = 0; row < 2; row++) {
                rcRow(.gap = 5, .align = "cc") {
                    for (int col = 0; col < 3; col++)
                        wall_block("55px", "43px", wall_color(st->wallpaper));
                }
            }
        } else {                               /* Center: one, in the middle  */
            wall_block("76px", "57px", wall_color(st->wallpaper));
        }
    }
}

/* The monitor's neck and base. Their face is a shade DARKER than the dialog: a
   BEVEL_FACE block on a BEVEL_FACE panel is the panel's own colour and reads as
   a hollow outline rather than as an object. */
static void stand_block(const char *w, const char *h) {
    rcBox(.bg = BEVEL_LIGHT, .pt = 1, .pl = 1, .w = w, .h = h) {
        rcBox(.bg = BEVEL_DARK, .pb = 1, .pr = 1, .w = "grow", .h = "grow") {
            rcBox(.bg = RC_GRAY_400, .w = "grow", .h = "grow") {}
        }
    }
}

/* WITHHELD RATHER THAN SQUEEZED where the page is short: a preview that pushes
   OK off the bottom has traded the dialog's job for its decoration. A HEIGHT,
   not a platform - a desktop window dragged short hits it too. */
#define PREVIEW_MIN_H 470

static void monitor_preview(const AppState *st) {
    if (rcViewport().height < PREVIEW_MIN_H)
        return;
    /* The preview is the page's one growing child, so a taller dialog enlarges
       the CRT's surround instead of pooling empty silver under the controls. */
    rcRow(.align = "cc", .w = "grow", .h = "grow") {
        rcColumn(.align = "cc") {
            rcColumn(.bg = BEVEL_LIGHT, .pt = 2, .pl = 2, .align = "cc") {
                rcColumn(.bg = BEVEL_DARK, .pb = 2, .pr = 2, .align = "cc") {
                    rcBox(.bg = BEVEL_FACE, .p = 9, .align = "cc") {
                        rcBox(.bg = RC_BLACK, .p = 2, .align = "cc") {
                            monitor_screen(st);
                        }
                    }
                }
            }
            stand_block("26px", "11px");
            stand_block("104px", "10px");
        }
    }
}

static void tab_background(AppState *st) {
    static const char *const walls[] = {
        "(None)", "Clouds", "Bubbles", "Setup", "Forest", "Waves",
    };
    rcColumn(.gap = 12, .p = 14, .w = "grow", .h = "grow", .wMax = BODY_MAX) {
        monitor_preview(st);
        field_label("Wallpaper:");
        rcBox(.w = "220px") {
            if (rcCombo("bg_wall", &st->wallpaper, walls,
                        (int)(sizeof walls / sizeof walls[0]))) st->applied = false;
        }
        field_label("Display:");
        rcRow(.gap = 16, .align = "cl") {
            if (win95_radio("bg_center",  "Center",  &st->placement, 0)) st->applied = false;
            if (win95_radio("bg_tile",    "Tile",    &st->placement, 1)) st->applied = false;
            if (win95_radio("bg_stretch", "Stretch", &st->placement, 2)) st->applied = false;
        }
        if (rcCheckbox("bg_pat", "Show desktop pattern", &st->pattern)) st->applied = false;
    }
}

static void tab_screensaver(RC_App *app, AppState *st) {
    static const char *const savers[] = {
        "(None)", "Flying Windows", "Mystify", "Starfield", "Marquee",
    };
    rcColumn(.gap = 12, .p = 14, .w = "grow", .h = "grow", .wMax = BODY_MAX) {
        monitor_preview(st);
        field_label("Screen Saver:");
        rcBox(.w = "220px") {
            if (rcCombo("ss_pick", &st->saver, savers,
                        (int)(sizeof savers / sizeof savers[0]))) st->applied = false;
        }
        /* THE ROW HAS TO SPAN, or the "grow" inside it has nothing to grow
           into: the track collapses to its handle and a slider you cannot drag
           is left looking like a small blue square. */
        rcRow(.gap = 10, .align = "cl", .w = "grow") {
            field_label("Wait:");
            rcBox(.w = "grow") {
                if (rcSlider("ss_wait", &st->wait, 1.0f, 60.0f)) st->applied = false;
            }
            /* rcText BORROWS these bytes until the frame is drawn - the frame
               arena outlives the frame, a local buffer would not. */
            RC_String mins = rcFormat(rcAppArena(app), "%d min",
                                         (int)(st->wait + 0.5f));
            rcText(mins, .font = F_BODY, .color = RC_BLACK);
        }
        if (rcCheckbox("ss_pw", "Password protected", &st->password)) st->applied = false;
    }
}

static void tab_appearance(AppState *st) {
    static const char *const schemes[] = {
        "Windows Standard", "Desert", "Eggplant", "Rose", "Teal (VGA)",
    };
    static const char *const items[] = {
        "Desktop", "Active Title Bar", "Menu", "Window", "Icon",
    };
    rcColumn(.gap = 12, .p = 14, .w = "grow", .h = "grow", .wMax = BODY_MAX) {
        monitor_preview(st);
        field_label("Scheme:");
        rcBox(.w = "260px") {
            if (rcCombo("ap_scheme", &st->scheme, schemes,
                        (int)(sizeof schemes / sizeof schemes[0]))) st->applied = false;
        }
        field_label("Item:");
        rcBox(.w = "260px") {
            if (rcCombo("ap_item", &st->item, items,
                        (int)(sizeof items / sizeof items[0]))) st->applied = false;
        }
        rcRow(.gap = 10, .align = "cl", .w = "grow") {
            field_label("Size:");
            rcBox(.w = "grow") {
                if (rcSlider("ap_size", &st->itemSize, 0.0f, 30.0f)) st->applied = false;
            }
        }
    }
}

static void layout(RC_App *app, void *userData) {
    AppSheet *sh = (AppSheet *)userData;
    AppState *st = &sh->live;
    RC_Style  s  = rcGetStyle();

    /* SAFE AREA. A phone draws the window edge to edge, UNDER the status bar and
       the home indicator, and nothing moves content out of the way for you.
       rcViewport().safe hands the margins over ALREADY IN LAYOUT UNITS - spend
       them once here at the root. Do NOT divide rcGetSafeAreaInsets() by the zoom
       yourself; that is wrong under RC_ZOOM_OPTICAL and is per-axis besides.
       Zero on desktop unless RAYCLAY_SAFE_INSETS stands a phone's bands in, so
       this is one code path everywhere. */
    RC_Insets safe = rcViewport().safe;

    /* Window controls act on an OS window, and web and mobile have none: there
       the RC_ID_WINDOW_* ids are inert, so these buttons would be dead chrome.
       rcChildWindowsSupported() is the public answer to "does this target have
       windows", so there is no #ifdef. OK and Cancel still close the app. */
    bool windowed = rcChildWindowsSupported();

    rcColumn(.id = "Root", .bg = s.background, .pt = (uint16_t)(safe.top),
             .pb = (uint16_t)(safe.bottom), .pl = (uint16_t)(safe.left),
             .pr = (uint16_t)(safe.right), .w = "grow", .h = "grow") {

        /* Caption band: native drag plus hand-drawn silver buttons. rcUnzoomed()
           holds it at a constant on-screen size, which is what keeps it matching
           the OS drag strip .titlebarHeight freezes in physical px. */
        rcUnzoomed() {
            rcRow(.id = RC_ID_WINDOW_DRAG, .bg = s.chrome, .gap = 8, .px = 6,
                  .align = "cl", .w = "grow", .h = "24px") {
                rcTextL("Display Properties", .font = F_TITLE, .color = RC_WHITE);
                rcBox(.w = "grow") {}
                if (windowed) {
                    rcRow(.gap = 2, .align = "cl") {
                        caption_button(RC_ID_WINDOW_MINIMIZE, rcIconMinus);
                        /* One id, TWO actions: RC_ID_WINDOW_MAXIMIZE is maximise
                           AND restore, so the glyph must show which one the next
                           click does. */
                        caption_button(RC_ID_WINDOW_MAXIMIZE,
                                       rcIsWindowMaximized() ? rcIconShrink
                                                             : rcIconMaximize);
                        caption_button(RC_ID_WINDOW_CLOSE, rcIconX);
                    }
                }
            }
        }

        rcColumn(.bg = s.background, .gap = 8, .p = 10, .w = "grow", .h = "grow") {

            /* No gap anywhere in this pair: the strip stands ON the page, and
               the tab_edge runs its top rule across every part the tabs do not
               cover. The page therefore opens with its LEFT edge only. */
            rcColumn(.gap = 0, .w = "grow", .h = "grow") {
                rcRow(.gap = 0, .w = "grow") {
                    if (tab_button("tab_bg", "Background",   st->tab == 0)) st->tab = 0;
                    tab_edge("3px");
                    if (tab_button("tab_ss", "Screen Saver", st->tab == 1)) st->tab = 1;
                    tab_edge("3px");
                    if (tab_button("tab_ap", "Appearance",   st->tab == 2)) st->tab = 2;
                    tab_edge("grow");
                }

                rcBox(.id = "content", .bg = BEVEL_DARK, .pl = 2,
                      .w = "grow", .h = "grow") {
                    rcBox(.bg = BEVEL_LIGHT, .pb = 2, .pr = 2,
                          .w = "grow", .h = "grow") {
                        /* "tc" reaches the tab body alone: it is the only child
                           with a width of its own, everything else here grows. */
                        rcBox(.bg = s.surface, .align = "tc", .w = "grow", .h = "grow") {
                            switch (st->tab) {
                            case 0: tab_background(st);           break;
                            case 1: tab_screensaver(app, st);     break;
                            case 2: tab_appearance(st);           break;
                            default: break;
                            }
                        }
                    }
                }
            }

            /* Three buttons, three contracts - which is what a property sheet
               is for. OK commits the working copy and closes, Cancel restores
               the last committed one and closes, Apply commits and stays. */
            rcRow(.gap = 6, .align = "cr", .w = "grow") {
                if (st->applied)
                    rcTextL("Applied.", .font = F_BODY, .color = RC_GRAY_700);
                rcBox(.w = "grow") {}
                if (bevel_button("act_ok", "OK", true)) {
                    sh->committed = sh->live;
                    rcAppRequestClose(app);
                }
                if (bevel_button("act_cancel", "Cancel", false)) {
                    sh->live = sh->committed;
                    rcAppRequestClose(app);
                }
                if (bevel_button("act_apply", "Apply", false)) {
                    sh->live.applied = true;
                    sh->committed    = sh->live;
                }
            }
        }
    }
}

int main(void) {
    AppSheet sheet = {
        .live = {
            .tab       = 0,
            .wallpaper = 1,
            .placement = 0,
            .pattern   = true,
            .saver     = 1,
            .wait      = 15.0f,
            .password  = false,
            .scheme    = 0,
            .item      = 1,
            .itemSize  = 10.0f,
        },
    };
    sheet.committed = sheet.live;

    static const float fontSizes[F_COUNT] = {
        [F_BODY]  = 14.0f,
        [F_TITLE] = 15.0f,
    };

    /* Custom Win95 palette: silver surfaces, blue caption, square corners. */
    RC_Style s      = rcStyleLight();
    s.background     = RC_GRAY_300;
    s.surface        = RC_GRAY_300;
    s.surfaceAlt     = RC_GRAY_200;
    s.chrome         = RC_BLUE_900;
    s.text           = RC_BLACK;
    s.textMuted      = RC_GRAY_700;
    s.border         = RC_GRAY_600;
    s.radius         = 0.0f;
    /* The accent reaches the radio dot and the check box fill, the only two
       widgets here that paint in it; the caption navy ties the two together. */
    s.primary        = RC_BLUE_900;
    s.primaryHover   = RC_BLUE_800;
    rcSetStyle(s);

    RC_AppOptions opts = {
        .width          = 460,
        .height         = 520,
        .title          = "Display Properties",
        .clearColor     = RC_TEAL_700,   /* the period teal - only ever seen if
                                            the opaque root does not cover the
                                            window, e.g. mid-resize */
        .fontSizes      = fontSizes,
        .fontCount      = F_COUNT,
        .scratchArenaBytes = 4096,   /* backs rcFormat (the "N min" readout) */
        .nativeFrame    = true,
        .titlebarHeight = 24,
        .layoutCallback = layout,
        .userData       = &sheet,
        .titlebar       = { .custom = true },   /* we draw the Win95 bar ourselves */
    };

    return rcRunApp(&opts);
}
