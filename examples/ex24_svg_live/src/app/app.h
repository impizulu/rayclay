/*
    app.h - the whole UI: state, and the layout that renders it

    One header, no .c beside it, every function `static inline`. main.c is the
    only translation unit, so there is nothing to link and no forward
    declarations. `static inline` rather than plain `static` because an unused
    `static` function is a -Wunused-function error under -Werror, and a header
    should not force its includer to call everything in it.

    Visual CHOICES live in theme.h; anything an OS does differently lives in
    platform/platform.h, which for this app is empty. When this file outgrows
    your head, split a SCREEN out of it into its own header beside this one.
*/
#ifndef APP_APP_H
#define APP_APP_H

#include "rayclay.h"
#include "app/theme.h"
#include "platform/platform.h"

#include "icons/rc_icons_chart_column.h"
#include "icons/rc_icons_expand.h"
#include "icons/rc_icons_folder.h"
#include "icons/rc_icons_maximize.h"
#include "icons/rc_icons_minimize.h"
#include "icons/rc_icons_minus.h"
#include "icons/rc_icons_panel_left.h"
#include "icons/rc_icons_panel_right.h"
#include "icons/rc_icons_rayclay_logo.h"
#include "icons/rc_icons_settings.h"
#include "icons/rc_icons_shrink.h"
#include "icons/rc_icons_x.h"

/* Where the .svg sources live: repo-relative, so the app runs from the repo
   root. Never an absolute path - it would bake this machine's home into the
   binary. */
#ifndef RC_EX24_SVG_DIR
    #define RC_EX24_SVG_DIR "examples/assets/icons/"
#endif

/* One row of the picker: the same artwork reachable by every route.

   TWO DRAW SLOTS BECAUSE THE GENERATOR EMITS TWO SIGNATURES, and the artwork
   decides which. Markup painted entirely in `currentColor` becomes an
   RC_IconCallback; the moment one shape bakes a concrete fill the colour
   parameter goes away. Keeping both is what lets the UI say WHY a colour
   argument is being ignored. */
typedef struct {
    const char     *label;
    const char     *file;      /* NULL => lives only in the source */
    RC_IconCallback draw;      /* NULL => no generated header      */
    void          (*drawBaked)(float);   /* set instead of `draw` when the
                                            artwork paints its own colours */
} Art;

/* Markup in the source, not read from disk - what rcLoadSvgFromMemory is for,
   and the one artwork here the BY PATH route cannot draw. */
static const char INLINE_SVG[] =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32' fill='none'"
    " stroke='currentcolor' stroke-linecap='round' stroke-linejoin='round' stroke-width='2'>"
    "<path d='M13 2 L13 6 11 7 8 4 4 8 7 11 6 13 2 13 2 19 6 19 7 21 4 24 8 28 11 25 13 26"
    " 13 30 19 30 19 26 21 25 24 28 28 24 25 21 26 19 30 19 30 13 26 13 25 11 28 8 24 4 21 7"
    " 19 6 19 2 Z'/>"
    "<circle cx='16' cy='16' r='4'/>"
    "</svg>";

/* Paths are joined by the preprocessor, so a path costs no run-time formatting
   and no scratch arena. The RayClay mark is the widest thing the parser is asked
   to do here, and the one artwork that bakes its own fills. */
static const Art ART[] = {
    { "rayclay mark", RC_EX24_SVG_DIR "rayclay-logo.svg", NULL,              rcIconRayClayLogo },
    { "settings",     RC_EX24_SVG_DIR "settings.svg",     rcIconSettings,    NULL },
    { "settings (inline)", NULL,                          rcIconSettings,    NULL },
    { "folder",       RC_EX24_SVG_DIR "folder.svg",       rcIconFolder,      NULL },
    { "chart-column", RC_EX24_SVG_DIR "chart-column.svg", rcIconChartColumn, NULL },
    { "panel-left",   RC_EX24_SVG_DIR "panel-left.svg",   rcIconPanelLeft,   NULL },
    { "panel-right",  RC_EX24_SVG_DIR "panel-right.svg",  rcIconPanelRight,  NULL },
    { "maximize",     RC_EX24_SVG_DIR "maximize.svg",     rcIconMaximize,    NULL },
    { "minimize",     RC_EX24_SVG_DIR "minimize.svg",     rcIconMinimize,    NULL },
    { "expand",       RC_EX24_SVG_DIR "expand.svg",       rcIconExpand,      NULL },
    { "shrink",       RC_EX24_SVG_DIR "shrink.svg",       rcIconShrink,      NULL },
    { "minus",        RC_EX24_SVG_DIR "minus.svg",        rcIconMinus,       NULL },
    { "x",            RC_EX24_SVG_DIR "x.svg",            rcIconX,           NULL },
};
enum { ART_COUNT = (int)(sizeof ART / sizeof ART[0]) };

/* One literal id per entry: an element id must outlive the frame, and an
   rcFormat'd one lives in the frame arena (see TINT_ID in theme.h). */
static const char *const ART_ID[ART_COUNT] = {
    "art0", "art1", "art2",  "art3",  "art4",  "art5", "art6",
    "art7", "art8", "art9",  "art10", "art11", "art12",
};

/* chart-column: a line artwork, so the tint argument has a currentColor to
   resolve to and the swatches do something the moment the app opens. */
enum { ART_OPENING = 4 };

/* The three routes, in the order a developer should consider them. The index is
   also the index into ROUTE[] in theme.h. */
enum { R_PATH, R_OWNED, R_GEN, R_COUNT };

/* `tag` is the only string in the app drawn in a route colour. `parses` and
   `frees` are what actually differs between the three cards once the pictures
   have agreed, so they are printed as a two-row spec under every artwork. */
static const struct {
    const char *id;
    const char *tag;
    const char *title;
    const char *parses;
    const char *frees;
} PANEL[R_COUNT] = {
    { "pan_path",  "BY PATH",   "Drawn straight from the file",
      "on first use",  "the library" },
    { "pan_owned", "HANDLE",    "Drawn from a handle you own",
      "when you load", "you"         },
    { "pan_gen",   "GENERATED", "Generated at build time",
      "never",         "nothing"     },
};

typedef struct {
    RC_Svg *svg;        /* the HANDLE route only; swapped, never accumulated */
    int     sel;        /* index into ART                                    */
    int     tint;
    float   size;       /* points, driven by the slider                      */
    bool    loadFailed;     /* the LAST load attempt returned NULL - diagnostic
                               only. st->svg remains the lifetime answer; this
                               exists so a failed load and a deliberate Unload
                               do not read identically to the user. */
} AppState;

/* Load the selected artwork into the handle we own, replacing whatever was
   live. Unload FIRST, so only one handle is ever alive. rcUnloadSvg NULLs the
   pointer for us, so st->svg alone answers "is it loaded?". */
static inline void load_selected(AppState *st)
{
    rcUnloadSvg(&st->svg);
    st->svg = ART[st->sel].file
           ? rcLoadSvg(ART[st->sel].file)
           : rcLoadSvgFromMemory(INLINE_SVG, (int)(sizeof INLINE_SVG - 1));
    st->loadFailed = (st->svg == NULL);
}

/* THE ONE WIDTH THIS APP CHANGES SHAPE AT, derived from its own panes: the rail
   beside three cards wide enough to print a whole call without truncating it.
   Compare against rcViewport().width - the space the layout actually has - so a
   desktop window dragged narrow takes the same branch a phone does.

   Below it the panels STACK rather than take turns: the app's thesis is that
   three routes draw the same picture, and a tab strip would hide the very
   comparison it exists to make. */
#define SVG_WIDE_W    996.0f

/* A stacked panel FITS its contents with a floor, never a fixed height: the
   slider reaches 220 px of artwork, which a fixed slot would overflow. */
#define SVG_STACK_MIN_H 300.0f

/* The artwork's bed, at its smallest. The slider bottoms out at 24 px, and a
   floor keeps the three beds the same size and shape whatever it says. Above
   the floor the bed grows with the card. */
#define SVG_BED_MIN_H 168.0f

/* THE ARTWORK GRID, the narrow arm's picker: chips of equal width dealt into
   rows by hand, because there is no wrapping row. The column count is what the
   widest chip allows, so a phone in portrait deals two and a wider window more;
   a chip narrower than its label wraps it rather than clipping. */
#define SVG_CHIP_W     168.0f
#define SVG_CHIP_GAP   4
#define SVG_GRID_PAD   8

/* A CARD IS AS WIDE AS ITS WIDEST WORD. A long unbreakable token in a caption
   takes a grow-width card hostage and the other two split what is left, so the
   captions print RC_EX24_SVG_DIR rather than the path it expands to; the value
   is shown once, over the picker.

   The two blocks whose length the content chooses - the call and the state
   line - get a floor rather than fitting, so one card's extra line cannot stand
   its artwork on a shorter bed than its neighbours'. */
#define SVG_TEXT_SLOT 36.0f

/* One line of a card's spec. The key column is a fixed width so the values line
   up across all three cards and the comparison can be read sideways. */
static inline void spec_row(const char *key, const char *value)
{
    RC_Style s = rcGetStyle();

    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        rcBox(.wType = RC_PX(56)) { rcTextC(key, .color = s.textMuted); }
        rcBox(.w = "grow") { rcTextC(value, .color = s.text); }
    }
}

/* One preview card: the identifier, the call that draws it, the artwork on its
   bed, and the two facts that differ between the routes. ONLY THE WORDS AND THE
   IDENTIFIER COLOUR MAY DIFFER - the bed, the artwork's size and the colour
   handed to the draw call are one expression each, written once and shared. */
static inline void panel(RC_App *app, AppState *st, int route, const char *height,
                         float minHeight)
{
    const Art  *art = &ART[st->sel];
    RC_Style    s   = rcGetStyle();
    /* The file name without the directory the macro supplies; `sizeof - 1` is
       that macro's length, so the split costs nothing at run time. */
    const char *base = art->file ? art->file + sizeof(RC_EX24_SVG_DIR) - 1 : NULL;
    const char *call;
    /* Amber is reserved for a file that would not open or markup that would not
       parse. "This route cannot draw this artwork" is a fact about the artwork,
       not a fault, and stays in body grey. */
    const char *note;
    const char *parses = PANEL[route].parses;
    const char *frees  = PANEL[route].frees;
    bool        faulty = false;

    switch (route) {
    case R_PATH:
        call = !art->file
             ? "no file to point at - this one is a C string"
             : rcFormat(rcAppArena(app), "rcSvg(RC_EX24_SVG_DIR \"%s\", size, color)",
                        base).chars;
        faulty = art->file && st->loadFailed;
        note   = !art->file  ? "nothing on disk for this route to open"
               : faulty      ? "that path did not open"
                             : "parsed on first use, then cached by the library";
        if (!art->file) parses = frees = "-";
        break;
    case R_OWNED:
        call = art->file ? "rcLoadSvg(path) -> rcSvgHandle(svg, size, color)"
                         : "rcLoadSvgFromMemory(bytes) -> rcSvgHandle(...)";
        faulty = st->loadFailed;
        note   = faulty   ? "the load returned NULL"
               : st->svg  ? "one handle, live, and yours to free"
                          : "freed - press Load to parse it again";
        break;
    default:
        call = art->drawBaked ? "rcIcon<Name>(size) - the artwork carries its colours"
             : art->draw      ? "rcIcon<Name>(size, color) - compiled in"
                              : "no generated header for this one";
        note = (art->draw || art->drawBaked)
             ? "compiled in: no file to ship, no parser to run"
             : "this artwork was never put through the converter";
        if (!art->draw && !art->drawBaked) parses = frees = "-";
        break;
    }

    rcColumn(.id = PANEL[route].id, .bg = s.surface, .gap = 10, .p = PAD,
             .borderRadius = "all-lg", .border = { .color = s.border, .width = "1px" },
             .w = "grow", .h = height, .hMin = minHeight) {
        /* A rule rather than a swatch beside the name: it spans the card, so
           the three are told apart from across the room. */
        rcBox(.bg = ROUTE[route], .borderRadius = "all-sm",
              .w = "grow", .hType = RC_PX(3)) {}
        rcTextC(PANEL[route].tag, .color = ROUTE[route], .letterSpacing = 1);
        rcTextC(PANEL[route].title, .color = s.text);
        rcBox(.w = "grow", .h = "fit", .hMin = SVG_TEXT_SLOT) {
            rcTextC(call, .color = s.textMuted);
        }
        rcBox(.bg = s.surfaceAlt, .align = "cc", .borderRadius = "all-md",
              .border = { .color = s.border, .width = "1px" },
              .w = "grow", .h = "grow", .hMin = SVG_BED_MIN_H) {
            if (route == R_PATH && art->file) {
                rcSvg(art->file, st->size, TINTS[st->tint]);
            } else if (route == R_OWNED && st->svg) {
                rcSvgHandle(st->svg, st->size, TINTS[st->tint]);
            } else if (route == R_GEN && art->draw) {
                art->draw(st->size, TINTS[st->tint]);
            } else if (route == R_GEN && art->drawBaked) {
                art->drawBaked(st->size);
            }
        }
        /* The pictures agree by design, so the card has to say what does NOT.
           Same two keys on all three cards, so the answers read as a table. */
        rcColumn(.gap = 4, .w = "grow") {
            spec_row("Parses", parses);
            spec_row("Frees",  frees);
        }
        rcBox(.w = "grow", .h = "fit", .hMin = SVG_TEXT_SLOT) {
            rcTextC(note, .color = faulty ? s.warning : s.textMuted);
        }
    }
}

/* The picker's heading, and the one place the directory's VALUE appears - the
   first question anyone asks when a card turns amber to say a path did not
   open, so the line turns amber with it. */
static inline void svg_rail_header(AppState *st)
{
    RC_Style s = rcGetStyle();

    rcTextC("ARTWORK", .color = s.textMuted, .letterSpacing = 1);
    rcTextC(RC_EX24_SVG_DIR, .color = st->loadFailed ? s.warning : s.textMuted);
}

/* The entry's own artwork, from whichever generated signature it carries. An
   icon picker that shows no icons is the one thing this one may not be. */
static inline void svg_art_glyph(int i, float size, RC_Color color)
{
    if (ART[i].draw)           ART[i].draw(size, color);
    else if (ART[i].drawBaked) ART[i].drawBaked(size);
}

/* One entry, in both arrangements: glyph, then name, on a surface that says
   whether it is the selection. rcClicked turns a styled row into a button and
   hands it the pointer cursor. */
static inline void svg_art_entry(AppState *st, int i, bool grid)
{
    RC_Style    s  = rcGetStyle();
    const char *id = ART_ID[i];
    bool        on = (i == st->sel);
    RC_Color    fg = on ? RC_WHITE : s.text;

    rcRow(.id = id,
          .bg = on ? s.primary
                   : rcAlpha(s.border, rcIsHovered(id) ? 110 : (grid ? 50 : 0)),
          .gap = 8, .px = SVG_GRID_PAD, .py = 7, .align = "cl",
          .borderRadius = "all-md", .w = "grow", .h = grid ? "grow" : "fit") {
        svg_art_glyph(i, 14.0f, fg);
        rcTextC(ART[i].label, .color = fg);
    }
    if (rcClicked(id)) {
        st->sel = i;
        load_selected(st);
    }
}

/* The grid: rows of `cols` chips, the last row padded with empty grow boxes so
   its chips keep the width of every other row's. @p inner is the width the rows
   have, spelled by the caller, because a fit-width row cannot be asked how wide
   it will be before it is laid out. */
static inline void svg_rail_grid(AppState *st, float inner)
{
    int cols = (int)((inner + SVG_CHIP_GAP) / (SVG_CHIP_W + SVG_CHIP_GAP));

    if (cols < 1) cols = 1;
    if (cols > ART_COUNT) cols = ART_COUNT;
    for (int first = 0; first < ART_COUNT; first += cols) {
        rcRow(.gap = SVG_CHIP_GAP, .w = "grow") {
            for (int i = first; i < first + cols; i++) {
                if (i < ART_COUNT)
                    svg_art_entry(st, i, true);
                else
                    rcBox(.w = "grow") {}
            }
        }
    }
}

/* The artwork picker: the same entries in both arrangements - a column beside
   the cards when there is room, a grid above them when there is not. */
static inline void svg_rail(AppState *st, bool beside)
{
    RC_Style s = rcGetStyle();

    if (beside) {
        rcColumn(.id = "rail", .bg = s.surface, .gap = 4, .p = 10,
                 .borderRadius = "all-lg", .h = "grow", .wType = RC_PX(RAIL_W)) {
            svg_rail_header(st);
            for (int i = 0; i < ART_COUNT; i++)
                svg_art_entry(st, i, false);
        }
    } else {
        /* NO SCROLL OF ITS OWN, so a finger anywhere on the grid moves the page:
           a pan that reaches the end of an INNER container stops there rather
           than moving the parent. The rows' width is the viewport less the safe
           bands the root spends, the body's padding and this shell's own. */
        RC_Viewport vp = rcViewport();
        float inner = vp.width - vp.safe.left - vp.safe.right
                    - 2.0f * (float)PAD - 2.0f * SVG_GRID_PAD;

        rcColumn(.id = "rail", .bg = s.surface, .gap = SVG_CHIP_GAP,
                 .p = SVG_GRID_PAD, .borderRadius = "all-lg", .w = "grow") {
            svg_rail_header(st);
            svg_rail_grid(st, inner);
        }
    }
}

/* The lifetime is yours on exactly one of the three routes, so the button acts
   on exactly one of the three cards. */
static inline void svg_life_button(RC_App *app, AppState *st)
{
    if (rcButton("life", st->svg ? "Unload" : "Load", RC_BTN_PRIMARY)) {
        if (st->svg) {
            /* Unload right here, inside the callback that drew it. rcUnloadSvg
               clears your handle now and releases the artwork once the frame has
               been drawn, so the card above keeps its picture for this frame and
               nothing is left pointing at freed bytes. No deferral to write. */
            rcUnloadSvg(&st->svg);
            st->loadFailed = false;     /* a deliberate free is not a failure */
        }
        else
            load_selected(st);
        rcWindowRequestFrame(rcAppMainWindow(app));
    }
}

/* Size, tint and the handle's lifetime, as three labelled rows. Stacked, each
   label sits OVER its control: six finger-sized swatches and their gaps already
   fill a phone's width, and a fixed-width child is never compressed. The
   swatches size to the pointer, not to the platform (rcPointerIsCoarse). */
static inline void svg_controls(RC_App *app, AppState *st, bool beside)
{
    RC_Style s      = rcGetStyle();
    float    swatch = rcPointerIsCoarse() ? 36.0f : 26.0f;
    /* An artwork that bakes its own fills leaves no currentColor for the tint
       argument to resolve to, so the swatches are dimmed to say so rather than
       looking live and doing nothing. The choice still moves, and takes effect
       the moment a line artwork is picked. */
    bool     tintLive = (ART[st->sel].drawBaked == NULL);

    rcColumn(.bg = s.surface, .gap = 12, .p = PAD, .borderRadius = "all-lg",
             .w = "grow") {
        if (!beside) rcTextC("Size", .color = s.textMuted);
        rcRow(.gap = 12, .align = "cl", .w = "grow") {
            if (beside)
                rcBox(.wType = RC_PX(64)) { rcTextC("Size", .color = s.textMuted); }
            rcBox(.w = "grow") { rcSlider("size", &st->size, 24.0f, 220.0f); }
            /* .wrap = "n": beside a grow slider the readout is the row's only
               fit-width text, so a narrow card would break "140 px" in two. */
            rcText(rcFormat(rcAppArena(app), "%d px", (int)st->size),
                   .color = s.text, .wrap = "n");
        }

        if (!beside) rcTextC("Tint", .color = s.textMuted);
        rcRow(.gap = 12, .align = "cl", .w = "grow") {
            if (beside)
                rcBox(.wType = RC_PX(64)) { rcTextC("Tint", .color = s.textMuted); }
            rcRow(.gap = 8, .align = "cl") {
                for (int i = 0; i < TINT_COUNT; i++) {
                    const char *tid = TINT_ID[i];
                    /* The chosen swatch is cut out twice - a dark halo inside it
                       and a bright ring around it - because .border.width is a
                       char[12] rather than a pointer and cannot take a ternary,
                       so one mark has to read against all six tints AND against
                       the card behind them. */
                    rcBox(.bg = i == st->tint ? s.primaryHover : RC_TRANSPARENT,
                          .p = 2, .borderRadius = "all-md") {
                        rcBox(.id = tid,
                              .bg = tintLive ? TINTS[i] : rcAlpha(TINTS[i], 70),
                              .borderRadius = "all-sm",
                              .border = { .color = i == st->tint ? s.background
                                                                 : s.border,
                                          .width = "2px" },
                              .wType = RC_PX(swatch), .hType = RC_PX(swatch)) {}
                    }
                    /* rcClicked turns any styled rcBox into a button, so the
                       SWATCH itself is the control - no separate marker. */
                    if (rcClicked(tid)) st->tint = i;
                }
            }
        }

        if (!beside) rcTextC("Handle", .color = s.textMuted);
        rcRow(.gap = 12, .align = "cl", .w = "grow") {
            if (beside)
                rcBox(.wType = RC_PX(64)) { rcTextC("Handle", .color = s.textMuted); }
            svg_life_button(app, st);
            rcBox(.w = "grow") {}
        }

        /* The one sentence about the SELECTION, under the controls that change
           it. Amber only on the first arm: the rest are states the app is meant
           to be in. */
        rcTextC(st->loadFailed
                ? "That file did not open. The path is relative, so launch from the "
                  "repository root - or point RC_EX24_SVG_DIR at your own folder."
                : !st->svg
                ? "Handle freed. The other two cards are unaffected - one is the "
                  "library's to free, the other is code."
                : ART[st->sel].drawBaked
                ? "This artwork paints its own colours, so all three routes ignore the "
                  "tint - there is no currentColor left for it to resolve to. Pick a "
                  "line artwork to watch the same argument do something."
                : ART[st->sel].file
                  ? "A file on disk. rcSvg opens it once and caches it; the handle "
                    "beside it is a second, separate copy that you own."
                  : "Markup embedded in the source - one executable, no converter, "
                    "and nothing for rcSvg to point at.",
                .color = st->loadFailed ? s.warning : s.textMuted);
    }
}

/* The lesson, stated once and at the top, because three cards drawing the same
   picture only reads as a comparison once someone has been told that the
   pictures AGREEING is the result. */
static inline void svg_thesis(void)
{
    RC_Style s = rcGetStyle();

    rcColumn(.gap = 2, .w = "grow") {
        rcTextL("One artwork, three routes", .font = F_TITLE, .color = s.text);
        rcTextL("The pictures are identical by design. What differs is what you "
                "ship, when it parses and who frees it.", .color = s.textMuted);
    }
}

static inline void layout(RC_App *app, void *userData)
{
    AppState *st = (AppState *)userData;
    RC_Style s = rcGetStyle();


    /* SAFE AREA. A phone draws the window edge to edge, UNDER the status bar and
       UNDER the home indicator. Ask for the margins and spend them ONCE, at the
       root: they belong to the window, not to any widget. rcViewport().safe
       hands them over ALREADY IN LAYOUT UNITS - never divide
       rcGetSafeAreaInsets() by the zoom instead, which is wrong under
       RC_ZOOM_OPTICAL. They are {0,0,0,0} on desktop unless RAYCLAY_SAFE_INSETS
       stands a phone's bands in, so this is one code path everywhere. */
    RC_Insets safe = rcViewport().safe;

    /* The arrangement, decided once: do the three cards still fit side by side
       with the picker beside them? There is no second question to ask. */
    bool wide = rcViewport().width >= SVG_WIDE_W;

    rcColumn(.id = "root", .bg = s.background, .pt = (uint16_t)(safe.top),
             .pb = (uint16_t)(safe.bottom), .pl = (uint16_t)(safe.left),
             .pr = (uint16_t)(safe.right), .w = "grow", .h = "grow") {
        /* THE ID IS THE BEHAVIOUR: RayClay grants a drag to exactly one element,
           the one tagged RC_ID_WINDOW_DRAG, and any other id leaves the window
           unmovable with nothing on screen to say why. Chrome, not content:
           .titlebarHeight freezes that strip in physical px, so the band is
           drawn inside rcUnzoomed() rather than growing with the zoom. */
        rcUnzoomed() {
            rcRow(.id = RC_ID_WINDOW_DRAG, .bg = s.surface, .gap = 10, .px = 14,
                  .align = "cl", .w = "grow", .hType = RC_PX(BAR_H)) {
                rcSvg(RC_EX24_SVG_DIR "settings.svg", 20.0f, s.text);
                rcTextL("SVG Live", .color = s.text);
                rcBox(.w = "grow") {}
                rcWindowControls();
            }
        }

        if (wide) {
            rcColumn(.gap = PAD, .p = PAD, .w = "grow", .h = "grow") {
                svg_thesis();
                rcRow(.gap = PAD, .w = "grow", .h = "grow") {
                    svg_rail(st, true);
                    rcColumn(.gap = PAD, .w = "grow", .h = "grow") {
                        rcRow(.gap = PAD, .w = "grow", .h = "grow") {
                            for (int r = 0; r < R_COUNT; r++)
                                panel(app, st, r, "grow", 0.0f);
                        }
                        svg_controls(app, st, true);
                    }
                }
            }
        } else {
            /* NARROW: one scrolling column, and THE CONTROLS COME BEFORE THE
               CARDS - three stacked cards are taller than any phone, so controls
               under them are controls nobody finds. */
            rcColumn(.id = "body", .gap = PAD, .p = PAD, .scroll = "v",
                     .w = "grow", .h = "grow") {
                svg_thesis();
                svg_rail(st, false);
                svg_controls(app, st, false);
                for (int r = 0; r < R_COUNT; r++)
                    panel(app, st, r, "fit", SVG_STACK_MIN_H);
            }
        }
    }

    /* A SCROLL CONTAINER GETS NO BAR UNLESS YOU ASK FOR ONE. Declared outside
       the root so it floats over the column it names, and guarded by the same
       condition that container has - naming one this frame did not build warns
       and is dropped. */
    if (!wide)
        rcScrollbar("body");
}

#endif /* APP_APP_H */
