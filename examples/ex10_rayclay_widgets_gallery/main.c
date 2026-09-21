/*  main.c - RayClay widgets gallery

    One window, one source file, and a labelled specimen of every RayClay widget
    and style, so any of them can be found and copied.

    A Jump-to bar under the title filters the list and scrolls to a specimen.
    One source, two arms: two columns at 1020 layout units or wider, one
    scrolling column below that (GALLERY_TWO_COLUMN_W).

    Zero-asset: the bundled font and the procedural icons need no files. The one
    demo PNG comes from RC_DEMO_LOGO - run from the repository root, or it falls
    back to a card synthesised in demo_image.h.

    Build target: rayclay_ex10_rayclay_widgets_gallery.  */

#include "rayclay.h"

#include "icons/rc_icons_settings.h"
#include "icons/rc_icons_panel_left.h"
#include "icons/rc_icons_panel_right.h"
#include "icons/rc_icons_minus.h"
#include "icons/rc_icons_x.h"
#include "icons/rc_icons_expand.h"
#include "icons/rc_icons_shrink.h"
#include "icons/rc_icons_maximize.h"
#include "icons/rc_icons_minimize.h"
#include "icons/rc_icons_rayclay_logo.h"        /* full-colour logo: rcIcon...(size)         */
#include "icons/rc_icons_rayclay_logo_mono.h"   /* line-art logo:   rcIcon...(size, colour)  */

/* The one demo image. rcLoadImage resolves against the process working
   directory, so this path finds the file only when the gallery is launched from
   the repository root; anywhere else the IMAGE section falls back to bytes it
   synthesises in demo_image.h. */
#ifndef RC_DEMO_LOGO
    #define RC_DEMO_LOGO "examples/assets/logos/rayclay-logo-1024.png"
#endif

#include "demo_image.h"

/* Font slots, in the order RC_AppOptions.fontSizes loads them. With no fontPath
   they are baked from the bundled face. The index is .font in RC_TextOptions. */
typedef enum { F_SMALL = 0, F_BODY, F_TITLE, F_BIG, F_COUNT } AppFont;

/* Slot 0's baked size. SLOT 0 IS WHAT EVERY LIBRARY WIDGET DRAWS TEXT AT when it
   is not handed a .font - button labels, combo values, menu items, table headers,
   tooltips and rcChart's tick labels all read it. */
enum { SZ_SMALL = 14 };

/* A CAP on the zoom-stop labels, not a count: a longer ladder shows its first
   ZOOM_STOPS_MAX entries rather than writing past the array. */
enum { ZOOM_STOPS_MAX = 24 };

typedef struct {
    long  frame;
    int   clicks;
    bool  showDetails;
    bool  darkMode;        /* drives the active theme                          */
    char  name[64];        /* every editor buffer on this page is app-owned    */
    char  secret[32];
    char  draft[320];      /* seeded in main()                                 */
    float volume;          /* 0..1                                             */
    int   quality;         /* radio group: 0 Low, 1 Medium, 2 High             */
    int   preset;
    int   tableRow;        /* row picked in section_table; -1 = nothing        */
    char  menuLast[24];    /* last menu item activated                         */
    int   selCopies;
    bool  modalOpen;
    bool  inspectorOpen;
    bool  inspectorSticky; /* -> .noBackdropDismiss; the flag that keeps it open */
    float splitFrac;
    float prevZoom;        /* last-seen zoom factor; the badge compares to it  */
    float zoomBadgeSecs;   /* badge time-to-live in seconds; >0 = visible      */
    double prevTime;       /* rcAppTime last frame; 0 until the first is taken */
    bool  opticalZoom;     /* false = layout reflow, true = optical magnify    */

    /* Zoom-stop picker. The LABELS are ours to own: rcCombo BORROWS its items and
       needs them to outlive the frame. The FACTORS are never copied - they come
       from rcAppZoomLadder every frame, so this cannot drift from the keyboard. */
    int   zoomStop;
    char  zoomLabel[ZOOM_STOPS_MAX][8];
    const char *zoomLabelPtr[ZOOM_STOPS_MAX];

    /* Drag-to-zoom (section_charts): the x window over demo_zoom, in DATA units.
       Owning these floats IS the zoom feature. */
    float zoomLo, zoomHi;
    bool  brushing;        /* true between the press edge and the release edge */
    float brushA, brushB;
    int   tipPlace;        /* combo index == RC_ChartTooltipPlace              */
    bool  hoverGuide;
    bool  hoverMarkers;

    /* Drag-scrub (section_gestures) - the pointer plus the button reads. */
    float scrub;           /* 0..100                                          */
    float scrubAtPress;
    float scrubAnchorX;
    bool  scrubbing;
    int   scrubCommits;    /* completed drags; proves the release edge fired   */

    /* Keyboard (section_keyboard) - edge against level. */
    int   spacePresses;
    int   spaceReleases;
    int   submits;
    int   nudge;

    /* Clipboard (section_clipboard). */
    int   clipState;       /* 0 = untried, 1 = text delivered, 2 = none        */
    int   clipCopies;
    RC_ClipboardToken clipToken;  /* the read in flight; 0 = none              */
    int   clipWait;        /* frames left before the read is called abandoned  */
    char  clipLast[128];   /* copied OUT of library memory                     */

    /* Image lifecycle (section_images). The buttons only RECORD an intent;
       update() acts on it, because a layout callback declares a frame rather
       than changing what the frame draws from. */
    int   imageAction;     /* 0 = idle, 1 = free the texture, 2 = decode again */
    int   imageLoads;
    bool  imageFromMem;    /* the PNG was not on disk, so the card was made    */

    /* Display and scheduling (section_display). The frame stamp is what makes a
       park observable: parked, it barely moves. */
    bool  continuous;
    long  wakeArmedFrame;
    int   wakesArmed;

    /* Live icons (section_live_icons). */
    float hue;             /* [0,1), advanced once per update()               */
    float hueSpeed;        /* cycles per second                               */
    float hueSpread;       /* per-icon offset -> a colour wave across the row */
    bool  hueFrozen;
    bool  animHeld;        /* update()'s rcIsModalOpen() sample               */
    unsigned rng;          /* xorshift32 state; never 0                       */

    /* Jump-to navigator (jump_bar). */
    char  search[32];      /* filter text; empty = every specimen             */
    int   jumpCat;         /* -1 = every group, else a GalleryCat             */
    bool  jumpRight;       /* the readout follows ColRight (wide arm only)    */

    /* Own arena (section_arena) - ours, so entries outlive the frame. */
    RC_Arena   logArena;
    RC_String *logLines;   /* the ARRAY lives in the arena too                */
    int        logCount;
    int        logSeq;
} AppState;

/* Entries kept by the own-arena demo. Deliberately small: filling it is the point. */
#define LOG_MAX 6

/* Live-icon support: a PRNG and a hue ramp, longhand because an example uses
   RayClay and the C standard library and nothing else. */

/* xorshift32 (Marsaglia) - period 2^32-1, and never 0 from a non-zero seed. */
static unsigned xorshift32(unsigned *state) {
    unsigned x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* A uniform float in [0,1), from the top 24 bits - a float's mantissa width. */
static float rng_unit(unsigned *state) {
    return (float)(xorshift32(state) >> 8) * (1.0f / 16777216.0f);
}

/* Fold a hue back into [0,1) with one truncation; callers keep |hue| < 2. */
static float hue_wrap(float hue) {
    hue -= (float)(int)hue;
    return hue < 0.0f ? hue + 1.0f : hue;
}

/* HSV -> RGB: the wheel is six linear ramps, so this needs no trigonometry.
   `hue` must be in [0,1) (see hue_wrap); sat and val are in [0,1]. */
static RC_Color hue_color(float hue, float sat, float val) {
    float h = hue * 6.0f;          /* [0,6) -> sextant index + fraction */
    int   i = (int)h;
    float f = h - (float)i;
    float p = val * (1.0f - sat);
    float q = val * (1.0f - sat * f);
    float t = val * (1.0f - sat * (1.0f - f));
    float r, g, b;
    switch (i) {
        case 0:  r = val; g = t;   b = p;   break;
        case 1:  r = q;   g = val; b = p;   break;
        case 2:  r = p;   g = val; b = t;   break;
        case 3:  r = p;   g = q;   b = val; break;
        case 4:  r = t;   g = p;   b = val; break;
        default: r = val; g = p;   b = q;   break;   /* i == 5 (and, defensively, 6) */
    }
    return RC_LIT(RC_Color){ r * 255.0f, g * 255.0f, b * 255.0f, 255.0f };
}

/* THE SPECIMEN REGISTRY. Every heading below is an anchor carrying an element
   id, and every anchor has a row here: the Jump-to bar walks this table to
   filter, to scroll, and to say which specimen you are on. Written once and
   expanded twice, so a heading, its chip and the counter cannot drift apart.
   The order is the reading order, which is also the order one column stacks in.
   The GROUP is what makes 26 specimens navigable. */
typedef enum {
    CAT_PRIMITIVES = 0, CAT_CONTROLS, CAT_DATA, CAT_INPUT, CAT_RUNTIME, CAT_COUNT
} GalleryCat;

#define GALLERY_SECTIONS(X)                                                     \
    X(RECTANGLES, "rect",    "Rectangles",            CAT_PRIMITIVES)           \
    X(ROUNDING,   "round",   "Rounded corners",       CAT_PRIMITIVES)           \
    X(GRADIENTS,  "grad",    "Gradients",             CAT_PRIMITIVES)           \
    X(SHADOWS,    "shadow",  "Shadows",               CAT_PRIMITIVES)           \
    X(OVERLAY,    "overlay", "Overlay tint",          CAT_PRIMITIVES)           \
    X(FLOATING,   "float",   "Floating",              CAT_PRIMITIVES)           \
    X(IMAGES,     "image",   "Image and logo",        CAT_PRIMITIVES)           \
    X(BORDERS,    "border",  "Borders",               CAT_PRIMITIVES)           \
    X(TEXT,       "text",    "Text",                  CAT_PRIMITIVES)           \
    X(WIDGETS,    "widget",  "Widgets",               CAT_CONTROLS)             \
    X(CONTROLS,   "control", "More widgets",          CAT_CONTROLS)             \
    X(CHARTS,     "chart",   "Charts",                CAT_DATA)                 \
    X(MANY,       "many",    "Many series",           CAT_DATA)                 \
    X(DRAGZOOM,   "dzoom",   "Drag to zoom",          CAT_DATA)                 \
    X(TABLE,      "table",   "Table",                 CAT_DATA)                 \
    X(BIGTABLE,   "bigtab",  "Big table",             CAT_DATA)                 \
    X(SPLITPANE,  "split",   "Split pane",            CAT_DATA)                 \
    X(ICONS,      "icon",    "Icons",                 CAT_PRIMITIVES)           \
    X(GESTURES,   "gesture", "Gestures",              CAT_INPUT)                \
    X(KEYBOARD,   "key",     "Keyboard",              CAT_INPUT)                \
    X(CLIPBOARD,  "clip",    "Clipboard",             CAT_INPUT)                \
    X(LIVEICONS,  "live",    "Live icons",            CAT_RUNTIME)              \
    X(SCROLL,     "scroll",  "Scroll and scissor",    CAT_DATA)                 \
    X(ZOOM,       "zoom",    "Zoom",                  CAT_RUNTIME)              \
    X(ARENA,      "arena",   "Your own arena",        CAT_RUNTIME)              \
    X(DISPLAY,    "display", "Display and scheduling", CAT_RUNTIME)

#define GAL_ENUM(name, slug, title, cat) SEC_##name,
#define GAL_ROW(name, slug, title, cat)  { "sec_" slug, "jmp_" slug, title, cat },

typedef enum { GALLERY_SECTIONS(GAL_ENUM) SECTION_COUNT } SectionId;

typedef struct {
    const char *anchor;  /* the heading's element id: what a jump scrolls to    */
    const char *chip;    /* the navigator chip's id; element ids must be unique */
    const char *title;   /* what the heading says, and what the chip says       */
    GalleryCat  cat;     /* which group the chip filters under                  */
} GallerySection;

static const GallerySection g_sections[] = { GALLERY_SECTIONS(GAL_ROW) };

static const char *cat_name(GalleryCat cat) {
    switch (cat) {
        case CAT_PRIMITIVES: return "Primitives";
        case CAT_CONTROLS:   return "Controls";
        case CAT_DATA:       return "Data";
        case CAT_INPUT:      return "Input";
        default:             return "Runtime";
    }
}

/* A category never colours a surface - only a 6px dot beside its heading, the
   dot on its chip, and that chip's outline while you are reading it. Not the
   RC_Style accents, which already mean something else here. */
static RC_Color cat_tint(GalleryCat cat) {
    switch (cat) {
        case CAT_PRIMITIVES: return RC_SKY_500;
        case CAT_CONTROLS:   return RC_EMERALD_500;
        case CAT_DATA:       return RC_AMBER_500;
        case CAT_INPUT:      return RC_VIOLET_500;
        default:             return RC_SLATE_500;
    }
}

/* ASCII case-folding and a substring test, written out because an example uses
   no libc. haystack[i + j] cannot run off the end: the terminator compares
   unequal to any needle byte that is still left. */
static char lower_ascii(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

static bool contains_ci(const char *haystack, const char *needle) {
    if (!needle || !needle[0]) return true;
    for (int i = 0; haystack[i]; i++) {
        int j = 0;
        while (needle[j] && lower_ascii(haystack[i + j]) == lower_ascii(needle[j]))
            j++;
        if (!needle[j]) return true;
    }
    return false;
}

/* A specimen matches on its own name OR its group's, so "input" brings back the
   whole Input group rather than nothing. */
static bool section_matches(const GallerySection *g, int cat, const char *filter) {
    if (cat >= 0 && (int)g->cat != cat) return false;
    return contains_ci(g->title, filter) || contains_ci(cat_name(g->cat), filter);
}

/* A section heading: the component's NAME, then the one line a reader needs in
   order to spell it. Two runs rather than one string, because a gallery is read
   by SCANNING names. AN EXPLICIT FIELD BEATS A CLASS, exactly as an inline style
   beats a CSS class: .color stays a field because the theme is live, while
   letterSpacing has no per-slot spelling and a class is the only way to say it. */
static void section_heading(SectionId sec, const char *note) {
    RC_Style s = rcGetStyle();
    const GallerySection *g = &g_sections[sec];

    rcRow(.id = g->anchor, .gap = 8, .align = "cl") {
        rcBox(.bg = cat_tint(g->cat), .borderRadius = "all-full",
              .w = "6px", .h = "6px") {}
        rcTextC(g->title, .font = F_BODY, .color = s.text);
        if (note)
            rcTextC(note, .className = "tracking-wide", .font = F_SMALL,
                    .color = s.textMuted);
    }
}

/* One LABELLED radius specimen: the shape, and under it the spelling that
   produced it. A macro rather than a function because .borderRadius is a char[]
   in the DSL and has to be a literal at the call site. */
#define RADIUS_SPECIMEN(radius)                                                   \
    rcColumn(.gap = 6, .align = "tc", .w = "56px") {                              \
        rcBox(.bg = RC_SLATE_600, .borderRadius = radius, .w = "56px",            \
              .h = "40px") {}                                                     \
        rcTextL(radius, .font = F_SMALL, .color = rcGetStyle().textMuted,         \
                .wrap = "n");                                                     \
    }

static void swatch(RC_Color color) {
    rcBox(.bg = color, .borderRadius = "all-md", .w = "56px", .h = "40px") {}
}

/* One icon tile: a square surface that brightens on hover. The colour is a plain
   argument, so the same tile serves the static grid and the live one below. */
static void icon_tile(const char *id, RC_IconCallback icon, RC_Color color) {
    rcBox(.id = id,
          .bg = rcIsHovered(id) ? rcGetStyle().surfaceAlt : rcGetStyle().surface,
          .align = "cc", .borderRadius = "all-lg", .w = "44px", .h = "44px") {
        icon(22.0f, color);
    }
}

/* The direction of a wrapping row, decided at runtime. rcRow and rcColumn are
   two macros, so a container that is a row where there is width and a column
   where there is not cannot be spelled with either; flex-row / flex-col is the
   one public spelling that takes a value. Each row below is two half-rows that
   must break as a PAIR, which is why this is spelled rather than left to wrap. */
#define WRAP_DIR(stack) ((stack) ? "flex-col" : "flex-row")

static void section_rectangles(bool stack) {
    rcColumn(.bg = rcGetStyle().surface, .gap = 12, .p = 16, .borderRadius = "all-xl",
             .w = "grow") {
        section_heading(SEC_RECTANGLES, NULL);
        rcBox(.gap = 10, .className = WRAP_DIR(stack)) {
            rcRow(.gap = 10, .align = "cl") {
                swatch(rcGetStyle().primary);
                swatch(rcGetStyle().danger);
                swatch(rcGetStyle().surfaceAlt);
            }
            /* A spacer between the halves. It is a child of the row, so the
               row's gap is charged each side of it. */
            if (!stack)
                rcMargin(.w = "24px");
            rcRow(.gap = 10, .align = "cl") {
                swatch(RC_INDIGO_500);
                swatch(RC_EMERALD_500);
                swatch(RC_AMBER_500);
            }
        }
    }
}

static void section_rounding(bool stack) {
    rcColumn(.bg = rcGetStyle().surface, .gap = 12, .p = 16, .borderRadius = "all-xl",
             .w = "grow") {
        section_heading(SEC_ROUNDING, "per-corner");
        rcBox(.gap = 10, .className = WRAP_DIR(stack)) {
            rcRow(.gap = 10, .align = "tl") {
                RADIUS_SPECIMEN("all-sm")
                RADIUS_SPECIMEN("all-lg")
                RADIUS_SPECIMEN("all-2xl")
                RADIUS_SPECIMEN("all-full")
            }
            rcRow(.gap = 10, .align = "tl") {
                RADIUS_SPECIMEN("t-xl")     /* top only   */
                RADIUS_SPECIMEN("l-xl")     /* left only  */
                RADIUS_SPECIMEN("tr-2xl")   /* one corner */
            }
        }
    }
}

static void section_gradients(bool stack) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_GRADIENTS, "two stops, any direction");
        rcBox(.gap = 10, .className = WRAP_DIR(stack)) {
            rcRow(.gap = 10, .align = "cl") {
                /* .gradient is keyed by .id, so each of these needs one. */
                rcBox(.id = "GradV", .borderRadius = "all-md", .w = "56px", .h = "40px",
                      .gradient = { .from = RC_INDIGO_600, .to = RC_ROSE_600,   .dir = "v" }) {}
                rcBox(.id = "GradH", .borderRadius = "all-md", .w = "56px", .h = "40px",
                      .gradient = { .from = RC_EMERALD_500, .to = RC_INDIGO_500, .dir = "h" }) {}
                rcBox(.id = "GradD", .borderRadius = "all-md", .w = "56px", .h = "40px",
                      .gradient = { .from = RC_AMBER_500, .to = RC_ROSE_600,    .dir = "d" }) {}
                rcBox(.id = "GradU", .borderRadius = "all-md", .w = "56px", .h = "40px",
                      .gradient = { .from = RC_INDIGO_500, .to = RC_EMERALD_500, .dir = "u" }) {}
            }
            rcRow(.gap = 10, .align = "cl") {
                /* Large radius + pill: the gradient honours the rounded geometry. */
                rcBox(.id = "GradRound", .borderRadius = "all-2xl", .w = "56px", .h = "40px",
                      .gradient = { .from = RC_INDIGO_600, .to = RC_AMBER_500,   .dir = "v" }) {}
                rcBox(.id = "GradPill", .borderRadius = "all-full", .w = "56px", .h = "40px",
                      .gradient = { .from = RC_ROSE_600, .to = RC_INDIGO_600,    .dir = "h" }) {}
            }
        }
    }
}

static void section_shadows(bool stack) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_SHADOWS, "soft, drawn behind the fill");
        /* A shadow is keyed by .id and drawn BEHIND the element's fill, so each
           card needs a visible .bg or .gradient to anchor it. This is the widest
           row in the gallery and the one that sets GALLERY_TWO_COLUMN_W. */
        rcBox(.gap = 28, .py = 14, .className = WRAP_DIR(stack)) {
            rcRow(.gap = 28, .align = "cl") {
                rcBox(.id = "ShDrop", .bg = s.surfaceAlt, .borderRadius = "all-lg",
                      .w = "64px", .h = "48px",
                      .shadow = { .color = { 0, 0, 0, 110 }, .y = 4, .blur = 12 }) {}
                rcBox(.id = "ShSoft", .bg = s.surfaceAlt, .borderRadius = "all-lg",
                      .w = "64px", .h = "48px",
                      .shadow = { .color = { 0, 0, 0, 90 }, .y = 8, .blur = 22 }) {}
                rcBox(.id = "ShCast", .bg = s.surfaceAlt, .borderRadius = "all-lg",
                      .w = "64px", .h = "48px",
                      .shadow = { .color = { 0, 0, 0, 120 }, .x = 8, .y = 8, .blur = 10 }) {}
            }
            rcRow(.gap = 28, .align = "cl") {
                /* Negative spread - a tight shadow hugging the box. */
                rcBox(.id = "ShTight", .bg = s.surfaceAlt, .borderRadius = "all-lg",
                      .w = "64px", .h = "48px",
                      .shadow = { .color = { 0, 0, 0, 140 }, .y = 6, .blur = 8, .spread = -3 }) {}
                /* Coloured glow: no offset + a wide blur reads as a halo. */
                rcBox(.id = "ShGlow", .bg = s.surfaceAlt, .borderRadius = "all-full",
                      .w = "64px", .h = "48px",
                      .shadow = { .color = { 99, 102, 241, 170 }, .blur = 18 }) {}
                /* Shadow + gradient: the gradient supplies the fill the shadow anchors to. */
                rcBox(.id = "ShGrad", .borderRadius = "all-lg", .w = "64px", .h = "48px",
                      .gradient = { .from = RC_INDIGO_600, .to = RC_ROSE_600, .dir = "v" },
                      .shadow   = { .color = { 0, 0, 0, 120 }, .y = 6, .blur = 14 }) {}
            }
        }
    }
}

/* One mini-card shown under a subtree overlay tint. The overlay recolours the
   card, every swatch and the caption in one pass. */
static void overlay_card(const char *label, RC_Color overlay) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surfaceAlt, .gap = 6, .p = 10, .borderRadius = "all-lg",
             .w = "grow", .overlay = overlay) {
        rcRow(.gap = 6) {
            rcBox(.bg = RC_INDIGO_500, .borderRadius = "all-sm", .w = "grow",
                  .h = "22px") {}
            rcBox(.bg = RC_EMERALD_500, .borderRadius = "all-sm", .w = "grow",
                  .h = "22px") {}
        }
        rcBox(.bg = RC_AMBER_500, .borderRadius = "all-sm", .w = "grow", .h = "22px") {}
        rcTextC(label, .font = F_SMALL, .color = s.text);
    }
}

static void section_overlay(bool stack) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_OVERLAY, "mixes a whole subtree");
        /* The same mini-card under four tints: .overlay applies to the element
           AND all its children at once. Two spellings of one colour, side by
           side: the CSS string when you are copying a value out of a design,
           rcAlphaF when the fraction is something your code computed.
           THE TRAP: `rcAlpha` is the 0-255 sibling, so rcAlpha(RC_WHITE, 0.30f)
           truncates to 0 and hands you a transparent colour with no warning. */
        rcBox(.gap = 14, .align = "tl", .className = WRAP_DIR(stack)) {
            rcRow(.gap = 14, .align = "tl") {
                overlay_card("none",       rcColor("transparent"));
                overlay_card("white 30%",  rcAlphaF(RC_WHITE, 0.30f));
            }
            rcRow(.gap = 14, .align = "tl") {
                overlay_card("black 45%",  rcColor("rgba(0,0,0,0.45)"));
                overlay_card("indigo 40%", rcColor("rgba(99,102,241,0.4)"));
            }
        }
    }
}

static void section_floating(RC_App *app, AppState *st) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_FLOATING, "out of flow: tooltip, menu, popover");
        /* An anchor "button" carrying a popover that floats out of layout flow.
           WHITE, NOT s.surface, for the label: an accent in RC_Style is a FILL
           and carries no ink with it, and on the dark theme s.surface reads
           2.84:1 on the primary fill, under AA. White is 6.29:1 in both presets
           and is what the library inks its own primary button label with. */
        rcBox(.id = "fl_anchor", .bg = s.primary, .align = "cc",
              .borderRadius = "all-md", .w = "160px", .h = "40px") {
            rcTextL("Anchor button", .color = RC_WHITE);
            rcColumn(.id = "fl_popover", .bg = s.surfaceAlt, .gap = 4, .p = 10,
                     .borderRadius = "all-md",
                     .border = { .color = s.border, .width = "1px" },
                     .floating = { .to      = RC_ATTACH_PARENT,
                                    .parent  = RC_ANCHOR_BOTTOM_LEFT,
                                    .element = RC_ANCHOR_TOP_LEFT,
                                    .offset  = { 0, 6 },
                                    .zIndex = 1000 }) {
                rcTextL("Floating popover", .color = s.text);
                rcTextL("anchored under the button", .color = s.textMuted);
            }
        }
        /* Content below, which the popover overlaps (proving z-order + out-of-flow). */
        rcRow(.gap = 10, .align = "cl") {
            rcBox(.bg = s.surfaceAlt, .borderRadius = "all-md", .w = "120px",
                  .h = "48px") {}
            rcBox(.bg = s.surfaceAlt, .borderRadius = "all-md", .w = "120px",
                  .h = "48px") {}
        }
        /* Tooltip: hover and dwell to reveal a floating label on top, which
           passes clicks through to what is underneath (.tooltip needs an .id).
           The label names the gesture the pointer at hand can make - keyed on
           rcPointerIsCoarse, never on the OS or on the width. */
        rcBox(.id = "tip_hover", .bg = s.surfaceAlt, .align = "cc",
              .borderRadius = "all-md", .w = "160px", .h = "40px",
              .tooltip = "Tooltips float on top and pass clicks through") {
            rcTextC(rcPointerIsCoarse() ? "Hold me for a tooltip" : "Hover me for a tooltip",
                    .color = s.text);
        }

        /* Menu (click to open) plus a context menu on the target box; choosing
           an item or clicking away dismisses either. EVERY rcMenuItem RETURNS
           TRUE ON ACTIVATE and that return is the whole widget - discard it and
           you have drawn a picture of a menu. */
        rcRow(.gap = 10, .align = "cl") {
            if (rcBeginMenu("menu_edit", "Edit")) {
                if (rcMenuItem("Undo"))
                    rcStrCopy(st->menuLast, "Undo", sizeof st->menuLast);
                if (rcMenuItem("Redo"))
                    rcStrCopy(st->menuLast, "Redo", sizeof st->menuLast);
                if (rcMenuItem("Preferences..."))
                    rcStrCopy(st->menuLast, "Preferences", sizeof st->menuLast);
                rcEndMenu();
            }
            rcBox(.id = "ctx_target", .bg = s.surfaceAlt, .align = "cc",
                  .borderRadius = "all-md", .w = "200px", .h = "40px") {
                rcTextC(rcPointerIsCoarse() ? "Long-press me" : "Right-click me",
                        .color = s.textMuted);
            }
        }
        if (rcBeginContextMenu("ctx_menu", "ctx_target")) {
            if (rcMenuItem("Cut"))
                rcStrCopy(st->menuLast, "Cut", sizeof st->menuLast);
            if (rcMenuItem("Copy"))
                rcStrCopy(st->menuLast, "Copy", sizeof st->menuLast);
            if (rcMenuItem("Paste"))
                rcStrCopy(st->menuLast, "Paste", sizeof st->menuLast);
            rcEndContextMenu();
        }
        rcText(rcFormat(rcAppArena(app), "chose: %s",
                        st->menuLast[0] ? st->menuLast : "nothing yet"),
               .font = F_SMALL, .color = s.textMuted);
    }
}

/* ZOOM. A RayClay desktop app zooms like a browser out of the box; the one
   thing that is invisible is the RESET binding, which is why this panel exists.
   Everything is read back from rcWindowZoom / rcWindowZoomMode rather than
   mirrored from app state, so the panel cannot drift from the window. Under
   optical zoom the pan is cursor-anchored only, so magnifying past the window
   edge gives you nowhere to travel. */

/* Index of the ladder stop nearest `factor`, compared in LOG space: zoom is
   multiplicative, so a linear |a-b| would call 100% the nearest stop to 145%.
   For ascending stops that is whether factor^2 exceeds their product. */
static int nearest_stop(const float *stops, uint16_t count, float factor)
{
    uint16_t i;

    if (count == 0)
        return 0;
    for (i = 0; i + 1 < count; i++) {
        if (factor * factor <= stops[i] * stops[i + 1])
            return (int)i;
    }
    return (int)count - 1;
}

static void section_zoom(RC_App *app, AppState *st)
{
    RC_Style s      = rcGetStyle();
    float    factor = rcWindowZoom(rcAppMainWindow(app));
    bool     optical = rcWindowZoomMode(rcAppMainWindow(app)) == RC_ZOOM_OPTICAL;

    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_ZOOM, "rcWindowZoom / rcWindowSetZoom / rcAppZoomLadder");

        rcRow(.gap = 14, .align = "cl") {
            rcText(rcFormat(rcAppArena(app), "%.0f%%", factor * 100.0f),
                    .font = F_BIG, .color = s.text);
            rcColumn(.gap = 2) {
                rcTextC(optical ? "optical - magnify the rendered surface"
                                : "layout - reflow, like a browser",
                         .font = F_SMALL, .color = s.textMuted);
                /* Spelled per target: the same binary runs on the web, where
                   the BROWSER owns Ctrl +/-/wheel and RayClay does not take
                   them. The picker below works everywhere. */
                rcTextL("Desktop: Ctrl and + / - walk the stops below, Ctrl 0 "
                        "resets to 100%, Ctrl and the wheel is continuous and "
                        "lands between them (Cmd on macOS). Web: the browser "
                        "owns those keys, so use the picker.",
                        .font = F_SMALL, .color = s.textMuted);
            }
        }

        /* THE STOPS ARE READ BACK FROM THE LIBRARY, NEVER RESTATED. A hard-coded
           50/100/150/200 is a second copy of the table rcAppZoomLadder owns, and
           it keeps offering factors the keyboard never visits. */
        {
            uint16_t stopCount = 0;
            const float *stops = rcAppZoomLadder(app, &stopCount);

            if (!stops || stopCount == 0) {
                /* The honest empty, and the reason the getter can return NULL:
                   this app asked for continuous keyboard zoom with .step. */
                rcTextL("continuous keyboard zoom (.zoom.step is set), so there "
                        "are no stops to list.",
                        .font = F_SMALL, .color = s.textMuted);
            } else {
                /* The labels are COPIED into storage this app owns, and LIFETIME
                   is the reason: rcFormat's .chars is a valid C string but lives
                   in the frame arena, while rcCombo BORROWS its items and needs
                   them to outlive the frame. */
                if (stopCount > ZOOM_STOPS_MAX)
                    stopCount = ZOOM_STOPS_MAX;
                for (uint16_t i = 0; i < stopCount; i++) {
                    RC_String pct = rcFormat(rcAppArena(app), "%d%%",
                                              (int)(stops[i] * 100.0f + 0.5f));
                    rcStrCopy(st->zoomLabel[i], pct.chars, sizeof st->zoomLabel[i]);
                    st->zoomLabelPtr[i] = st->zoomLabel[i];
                }
                /* The keyboard and the wheel move the factor behind our back, so
                   the combo shows the nearest stop, not the last thing clicked. */
                st->zoomStop = nearest_stop(stops, stopCount, factor);

                rcRow(.gap = 10, .align = "cl") {
                    rcTextL("Stops", .font = F_SMALL, .color = s.textMuted);
                    rcBox(.w = "110px") {
                        if (rcCombo("cb_zoomstop", &st->zoomStop,
                                     st->zoomLabelPtr, (int)stopCount))
                            rcWindowSetZoom(rcAppMainWindow(app), stops[st->zoomStop]);
                    }
                    rcToggle("tg_zoommode", &st->opticalZoom);
                    rcTextC(optical ? "Optical" : "Layout",
                             .font = F_SMALL, .color = s.textMuted);
                }
            }
        }

        /* PAN. Enabled for this app in main() - it is off by default, because a
           browser does not pan and that is the out-of-the-box contract. */
        rcRow(.gap = 8, .align = "cl") {
            rcTextL("PAN:", .font = F_SMALL, .color = s.textMuted);
            rcTextC(optical
                    ? "hold Space and drag with the left button, Figma-style. "
                      "Space is suppressed while a text field has focus, so "
                      "typing a space never drags the view."
                    : "layout zoom reflows into the window, so there is nothing "
                      "outside it to reach - switch to optical to pan.",
                    .font = F_SMALL,
                    .color = optical ? s.textMuted : s.warning);
        }
    }
}

/* Loaded once in update(), because rcLoadImage needs the renderer up. */
static RC_Image g_demo_image;

/* Put a picture in g_demo_image, and report whether it came from memory. The
   FILE is tried first because that is the call a real app makes; a synthesised
   card stands in when the path does not resolve, which keeps the section
   truthful from any working directory. */
static bool demo_image_load(void) {
    unsigned char bmp[DEMO_CARD_CAP];   /* ~37 KB, one-shot path only */
    int len;

    g_demo_image = rcLoadImage(RC_DEMO_LOGO);
    if (g_demo_image.handle)
        return false;

    len = demo_card_bmp(bmp, (int)sizeof bmp);
    if (len > 0)
        g_demo_image = rcLoadImageFromMemory(bmp, len);
    return true;
}

static void section_images(RC_App *app, AppState *st, bool stack) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_IMAGES, "raster PNG against procedural vector");
        rcBox(.gap = 14, .className = WRAP_DIR(stack)) {
            rcRow(.gap = 14, .align = "cl") {
                /* Left: the decoded raster - two sizes and a tint. An RC_Image
                   is an RC_Image; nothing here knows which decoder ran. */
                if (g_demo_image.handle) {
                    rcBox(.w = "96px", .h = "96px", .image = &g_demo_image) {}
                    rcBox(.w = "56px", .h = "56px", .image = &g_demo_image) {}
                    rcBox(.bg = rcColor("#6366f1c8"), .w = "96px", .h = "96px",
                          .image = &g_demo_image) {}
                } else {
                    /* Only reachable via "Free texture" below: a failed decode
                       is covered by the fallback. */
                    rcBox(.bg = s.surfaceAlt, .align = "cc", .borderRadius = "all-lg",
                          .w = "96px", .h = "96px") {
                        rcTextL("freed", .font = F_SMALL, .color = s.textMuted);
                    }
                }
            }
            if (!stack)
                rcMargin(.w = "16px");
            rcRow(.gap = 14, .align = "cl") {
                /* Right: the same logo as a resolution-free vector icon, no
                   file. This one takes (size) alone because its artwork bakes
                   its own palette; the line-art variant in LIVE ICONS takes a
                   colour too and is re-tintable every frame. */
                rcBox(.align = "cc", .w = "96px", .h = "96px") { rcIconRayClayLogo(96.0f); }
                rcBox(.align = "cc", .w = "56px", .h = "56px") { rcIconRayClayLogo(56.0f); }
            }
        }

        /* THE LIFECYCLE, the part that is easy to get wrong. A vector icon costs
           nothing to free - it is code. A raster image is a GPU texture plus its
           decoded pixels, and rcUnloadImage is the one call that releases it.
           rcLoadImage decodes afresh every call and does NOT cache by path, so
           assigning a second RC_Image over a live one strands the first texture
           forever, and there is a hard ceiling of 128. Free, then load. */
        rcBox(.gap = 8, .align = "cl", .className = WRAP_DIR(stack)) {
            rcRow(.gap = 8, .align = "cl") {
                if (rcButton("img_free", "Free texture", RC_BTN_DEFAULT))
                    st->imageAction = 1;
                if (rcButton("img_load", "Re-decode", RC_BTN_DEFAULT))
                    st->imageAction = 2;
            }
            RC_String tally = rcFormat(rcAppArena(app), "%s   decodes: %d",
                                        g_demo_image.handle ? "resident" : "freed",
                                        st->imageLoads);
            rcText(tally, .font = F_SMALL, .color = s.textMuted, .wrap = "n");
        }
        /* Name the entry point that produced what is on screen: the whole point
           of a fallback is that you can SEE which one ran. */
        if (st->imageFromMem)
            rcTextL("rcLoadImageFromMemory - synthesised in demo_image.h, because "
                    "the PNG is not at the relative path this build was given. "
                    "Launch from the repository root to decode the real file.",
                    .font = F_SMALL, .color = s.warning);
        else
            rcTextL("rcLoadImage - decoded from the PNG on disk.",
                    .font = F_SMALL, .color = s.textMuted);
    }
}

static void section_borders(void) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_BORDERS, NULL);
        rcRow(.gap = 10, .align = "cl") {
            rcBox(.borderRadius = "all-md",
                  .border = { .color = s.border, .width = "1px" }, .w = "56px",
                  .h = "40px") {}
            rcBox(.borderRadius = "all-lg",
                  .border = { .color = s.primary, .width = "all-2px" }, .w = "56px",
                  .h = "40px") {}
            rcBox(.borderRadius = "all-full",
                  .border = { .color = s.danger, .width = "all-3px" }, .w = "56px",
                  .h = "40px") {}
            rcBox(.bg = s.surfaceAlt, .borderRadius = "all-md",
                  .border = { .color = s.text, .width = "1px" }, .w = "56px",
                  .h = "40px") {}
        }
    }
}

static void section_text(void) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surface, .gap = 10, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_TEXT, NULL);
        rcTextL("Big heading", .font = F_BIG, .color = s.text);
        rcTextL("Title text in the primary accent",
            .font = F_TITLE, .color = s.primary);
        rcTextL("Body copy in muted grey. RayClay measures, wraps and draws "
                 "every glyph itself, so text stays crisp at any size.",
            .font = F_BODY, .color = s.textMuted, .lineHeight = 26);
        rcTextL("Small label / caption", .font = F_SMALL, .color = s.danger);

        /* THE SAME RUN, SPELLED AS UTILITIES: RC_TextOptions carries the same
           .className grammar rcBox takes. Leading and tracking have no per-slot
           spelling, so a class is the only way to say them; SIZE is better taken
           from a slot, which carries an atlas baked at that size. */
        rcTextL("Tracking and leading, spelled as classes",
            .className = "tracking-wide leading-relaxed", .font = F_BODY,
            .color = s.text);

        /* PRECEDENCE: AN EXPLICIT FIELD BEATS A CLASS. The class asks for
           text-2xl (24px) and .size says 30, so this draws at 30. .font = F_TITLE
           is the point rather than decoration: F_TITLE is baked at 30, so this
           comes off an atlas at its own size and is crisp, where dropping it
           would scale slot 0's 14px atlas. A class sets a size, never bakes one. */
        rcTextL("Explicit .size = 30 beats the class's text-2xl",
            .className = "text-2xl", .font = F_TITLE, .color = s.textMuted,
            .size = 30);
        /* The symbol group is glued with U+00A0, the no-break space: the wrapper
           breaks on ' ' only, so the guillemets move as a pair. */
        rcTextL("Latin-1: àâäéèêëîïôöùûüç ñ - ¿Olé?  "
                "«\xc2\xa0£\xc2\xa0©\xc2\xa0®\xc2\xa0»",
            .font = F_BODY, .color = s.text);
        /* A long unbreakable word (.wrap = "n") cut off by .overflow = "hidden"
           instead of spilling past the 160px box. */
        rcBox(.bg = RC_SLATE_600, .px = 8, .align = "cl", .overflow = "hidden",
              .borderRadius = "all-sm", .w = "160px", .h = "28px") {
            rcTextL("supercalifragilisticexpialidocious",
                .font = F_SMALL, .color = s.text, .wrap = "n");
        }
    }
}

static void section_icons(void) {
    RC_Color ink = rcGetStyle().text;
    rcColumn(.bg = rcGetStyle().surface, .gap = 12, .p = 16, .borderRadius = "all-xl",
             .w = "grow") {
        section_heading(SEC_ICONS, "drawn from vectors, not files");
        rcRow(.gap = 8, .align = "cl") {
            icon_tile("ic_settings",    rcIconSettings,   ink);
            icon_tile("ic_panel_left",  rcIconPanelLeft,  ink);
            icon_tile("ic_panel_right", rcIconPanelRight, ink);
            icon_tile("ic_minus",       rcIconMinus,      ink);
            icon_tile("ic_x",           rcIconX,          ink);
        }
        rcRow(.gap = 8, .align = "cl") {
            icon_tile("ic_expand",   rcIconExpand,   ink);
            icon_tile("ic_shrink",   rcIconShrink,   ink);
            icon_tile("ic_maximize", rcIconMaximize, ink);
            icon_tile("ic_minimize", rcIconMinimize, ink);
        }
    }
}

/* The payoff of a procedural icon over a raster one. RC_IconCallback is
   `void (*)(float size, RC_Color color)` and both arguments are per-frame values:
   the geometry is re-stroked at exactly the size and colour asked for, and no
   asset is re-exported. An icon generated from all-`currentColor` artwork takes
   a colour and is tintable; one with a baked palette takes (size) alone. */

/* Gestures - rcPointer plus the button reads. rcClicked answers "was this
   element activated?", a COMPLETED press-then-release, so it cannot describe a
   gesture in flight. These can: latch on the press, track while held, commit on
   the release. A drag-scrub moves by how far the pointer TRAVELLED, so it needs
   no rect; mapping a pointer onto CONTENT does, which is what rcGetElementBox
   and rcChartPlotRect are for. A drag IS input, so the frames arrive free. */
static void section_gestures(RC_App *app, AppState *st) {
    RC_Style s = rcGetStyle();
    /* rcPointer() is CONTENT space - zoom and pan already undone - so it is
       directly comparable with layout geometry at any zoom. */
    RC_Vec2 p = rcPointer();

    /* THE CURSOR IS PART OF THE AFFORDANCE, and a surface built from raw pointer
       reads has to say so itself: polling rcClicked or rcPressed would hand this
       element the clickable hand for free, but it is DRAGGED and polls neither. */
    if (rcIsHovered("scrub_field") || st->scrubbing)
        rcSetCursor(st->scrubbing ? RC_CURSOR_GRABBING : RC_CURSOR_GRAB);

    if (rcPointerPressed(RC_POINTER_LEFT) && rcIsHovered("scrub_field")) {
        st->scrubbing    = true;
        st->scrubAtPress = st->scrub;
        st->scrubAnchorX = p.x;
    } else if (st->scrubbing && rcPointerDown(RC_POINTER_LEFT)) {
        /* 0.5 units per content px: a full 0..100 sweep is a 200px drag. */
        float v = st->scrubAtPress + (p.x - st->scrubAnchorX) * 0.5f;

        st->scrub = v < 0.0f ? 0.0f : (v > 100.0f ? 100.0f : v);
    } else if (st->scrubbing && rcPointerReleased(RC_POINTER_LEFT)) {
        st->scrubbing = false;
        st->scrubCommits++;
    }

    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_GESTURES, "rcPointer and the button reads");
        rcTextL("Press and drag sideways anywhere on the field. Keep dragging past its edge - the value keeps tracking, and the release still lands.",
                 .font = F_SMALL, .color = s.textMuted);

        /* Highlighting on .scrubbing, not on hover: the gesture owns the field
           until the button comes up, wherever the pointer has wandered to. */
        rcBox(.id = "scrub_field", .bg = st->scrubbing ? s.primary : s.surfaceAlt,
              .align = "cc", .borderRadius = "all-lg", .w = "grow", .h = "56px",
              .tooltip = "Drag left/right to scrub") {
            RC_String v = rcFormat(rcAppArena(app), "%.1f", st->scrub);

            rcText(v, .font = F_BIG,
                    .color = st->scrubbing ? RC_WHITE : s.text);
        }

        rcRow(.gap = 10, .align = "cl", .w = "grow") {
            RC_String st8 = rcFormat(rcAppArena(app), "%s   committed drags: %d",
                                        st->scrubbing ? "dragging" : "idle",
                                        st->scrubCommits);

            rcText(st8, .font = F_SMALL, .color = s.textMuted);
            rcBox(.w = "grow") {}
            if (rcButton("scrub_reset", "Reset", RC_BTN_DEFAULT)) {
                st->scrub        = 50.0f;
                st->scrubCommits = 0;
            }
        }
    }
}

/* Keyboard: the two reads people confuse, plus the query that makes a shortcut
   portable. Everything is a COUNTER rather than a timed flash: a per-frame
   countdown would stall on demand, because a key held with no new event
   produces no frames. */
static void section_keyboard(RC_App *app, AppState *st) {
    RC_Style s    = rcGetStyle();
    bool     held = rcKeyDown(RC_KEY_SPACE);   /* LEVEL: true every frame it is down */

    /* EDGE: one count per physical press. Auto-repeat does NOT re-fire it, which is
       what you want for a command and the wrong thing for "move while held". */
    if (rcKeyPressed(RC_KEY_SPACE))
        st->spacePresses++;

        /* The closing edge. A press-only counter cannot tell "still held" from
           "over", so anything that must END when the key ends belongs here. */
    if (rcKeyReleased(RC_KEY_SPACE))
        st->spaceReleases++;

        /* RC_MOD_PRIMARY is Cmd on a native macOS build and Ctrl everywhere
           else, so one line is the correct accelerator on every platform - never
           test RC_KEY_LEFT_CTRL yourself. */
    if (rcModDown(RC_MOD_PRIMARY) && rcKeyPressed(RC_KEY_ENTER))
        st->submits++;

    /* Arrows, not WASD: letter keys are LOGICAL, so RC_KEY_W is wherever the user's
       layout puts W - it is not the physical key next to A on AZERTY. */
    if (rcKeyPressed(RC_KEY_LEFT))  st->nudge--;
    if (rcKeyPressed(RC_KEY_RIGHT)) st->nudge++;

    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_KEYBOARD, "rcKeyPressed / rcKeyDown / rcModDown");
        rcTextL("Click the window first, then try it: Space, the Left/Right arrows, and Cmd+Enter (Ctrl+Enter off macOS).",
                 .font = F_SMALL, .color = s.textMuted);

            /* EACH DATUM IS ITS OWN TEXT RUN IN A ROW WITH A REAL GAP, never one
               string padded with spaces: the face is proportional, so a run of
               spaces is not a tab stop and will not line up. */
        rcRow(.gap = 10, .align = "cl", .w = "grow") {
            /* The level read drives a live chip: it lights on the press edge and
               clears on the release edge, and both of those are input. */
            rcBox(.bg = held ? s.primary : s.surfaceAlt, .align = "cc",
                  .borderRadius = "all-lg", .w = "120px", .h = "44px") {
                rcTextL("SPACE", .font = F_SMALL,
                         .color = held ? RC_WHITE : s.textMuted);
            }
            rcBox(.w = "grow") {}
            if (rcButton("kbd_reset", "Reset", RC_BTN_DEFAULT)) {
                st->spacePresses  = 0;
                st->spaceReleases = 0;
                st->submits       = 0;
                st->nudge         = 0;
            }
        }
        rcColumn(.gap = 4, .w = "grow") {
            rcRow(.gap = 18, .align = "cl") {
                rcText(rcFormat(rcAppArena(app), "pressed %d / released %d",
                                st->spacePresses, st->spaceReleases),
                       .font = F_SMALL, .color = s.text, .wrap = "n");
                rcText(rcFormat(rcAppArena(app), "rcKeyDown now: %s", held ? "yes" : "no"),
                       .font = F_SMALL, .color = s.text, .wrap = "n");
            }
            rcRow(.gap = 18, .align = "cl") {
                rcText(rcFormat(rcAppArena(app), "PRIMARY+Enter submits: %d", st->submits),
                       .font = F_SMALL, .color = s.textMuted, .wrap = "n");
                rcText(rcFormat(rcAppArena(app), "arrows: %d", st->nudge),
                       .font = F_SMALL, .color = s.textMuted, .wrap = "n");
            }
        }

        /* THE SOFT KEYBOARD, the one piece of mobile behaviour an app cannot get
           for free. Focusing an editor does not raise it and there is no
           keyboard-avoidance, so you ask explicitly and tell the OS where the
           caret is. Both calls are NO-OPS on desktop and the web, which is what
           makes this one source rather than an #ifdef. */
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            rcTextL("Soft keyboard", .font = F_SMALL, .color = s.textMuted);
            rcBox(.w = "grow") {}
            if (rcButton("kbd_ime_show", "Show", RC_BTN_DEFAULT)) {
                RC_Box caret = rcGetElementBox("in_name");  /* the Name field, declared above */

                if (caret.found)
                    rcSetImeCaretRect(caret.x, caret.y, caret.width, caret.height);
                rcSetSoftKeyboardVisible(true);
            }
            if (rcButton("kbd_ime_hide", "Hide", RC_BTN_DEFAULT))
                rcSetSoftKeyboardVisible(false);
        }
        rcTextL("On a phone these raise and dismiss the on-screen keyboard; on desktop and the web they do nothing.",
                 .font = F_SMALL, .color = s.textMuted);
    }
}

/* Copy/paste against the real system clipboard.

   A read is REQUEST-then-POLL, never a blocking get: that is the only form that
   works on every target, because a browser resolves a clipboard read
   asynchronously and a synchronous read cannot exist there at all.

   Do not probe at startup. There is no capability query, rcClipboardGet answers
   only under a synchronous backend, and a browser refuses an unprompted read
   outright - it needs a secure context and a user gesture. Requesting from
   inside the button handler satisfies both, which is what a real app does
   anyway. One source, no #ifdef, honest on every target. */
static void section_clipboard(RC_App *app, AppState *st) {
    RC_Style s = rcGetStyle();

    /* Collect an outstanding read. Poll never blocks and answers exactly once;
       it returns NULL for "still pending", "denied" and "already collected"
       alike, so bound the wait rather than polling forever. */
    if (st->clipToken) {
        const char *got = rcClipboardPoll(st->clipToken);
        if (got) {
            rcStrCopy(st->clipLast, got, sizeof st->clipLast);  /* RayClay owns `got` */
            st->clipToken = 0;
            st->clipState = 1;
        } else if (--st->clipWait <= 0) {
            rcStrCopy(st->clipLast, "(no text delivered)", sizeof st->clipLast);
            st->clipToken = 0;
            st->clipState = 2;
        } else {
                /* A countdown measured in frames only counts if those frames
                   happen. A delivery wakes the app itself, so this covers the
                   case the timeout exists for: a backend that never answers. */
            rcWindowRequestFrame(rcAppMainWindow(app));
        }
    }

    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_CLIPBOARD, "rcClipboardSet / Request + Poll");

        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            rcBox(.bg = st->clipState == 1 ? s.successHover
                       : st->clipState == 2 ? s.warningHover
                                            : s.textMuted,
                  .borderRadius = "all-full", .w = "10px", .h = "10px") {}
            /* Three calls rather than a ternary: rcTextL takes a string LITERAL.
               For a runtime C string use rcTextC; rcText takes an RC_String. */
            if (st->clipState == 1) {
                rcTextL("Reads work here - copy in another app, then press Paste again.",
                         .font = F_SMALL, .color = s.textMuted);
            } else if (st->clipState == 2) {
                rcTextL("Answered with no text: an empty clipboard and a refused read look alike.",
                         .font = F_SMALL, .color = s.textMuted);
            } else {
                rcTextL("Press Paste to exercise a read - a browser grants one only on a user gesture.",
                         .font = F_SMALL, .color = s.textMuted);
            }
        }

        rcRow(.gap = 10, .align = "cl", .w = "grow") {
            if (rcButton("clip_copy", "Copy a line", RC_BTN_PRIMARY)) {
                RC_String line = rcFormat(rcAppArena(app),
                                             "RayClay copied this at frame %ld.", st->frame);
                /* rcFormat hands back a LENGTH-counted RC_String; the clipboard
                   takes a C string, and the arena always NUL-terminates. */
                rcClipboardSet(line.chars);
                st->clipCopies++;
            }
            if (rcButton("clip_paste", "Paste", RC_BTN_DEFAULT)) {
                st->clipToken   = rcClipboardRequest();
                st->clipWait    = 30;
                st->clipLast[0] = '\0';
                /* Under the on-demand default a pending read needs a frame to be
                   collected in, so ask for the frames the countdown above is
                   measured in rather than assuming they arrive. */
                rcWindowRequestFrame(rcAppMainWindow(app));
            }
        }

        RC_String stat = rcFormat(rcAppArena(app), "copies: %d        pasted: %s",
                                     st->clipCopies,
                                     st->clipToken     ? "waiting..."
                                     : st->clipLast[0] ? st->clipLast
                                                       : "(nothing yet)");
        rcText(stat, .font = F_SMALL, .color = s.text);
        rcTextL("A read is mirrored into a fixed buffer: text over 4095 bytes arrives truncated, with one warning naming both sizes.",
                 .font = F_SMALL, .color = s.textMuted);
        rcTextL("On the web this is the browser's own clipboard, so it needs https or localhost; on an insecure origin a read is refused, not crashed.",
                 .font = F_SMALL, .color = s.textMuted);
    }
}

static void section_live_icons(RC_App *app, AppState *st) {
    RC_Style s = rcGetStyle();
    /* One base colour per frame; each tile offsets the hue into a wave. */
    RC_Color live = hue_color(st->hue, 0.85f, 1.0f);
    RC_Color wave[9];
    for (int i = 0; i < 9; i++)
        wave[i] = hue_color(hue_wrap(st->hue + (float)i * st->hueSpread), 0.85f, 1.0f);

    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_LIVEICONS, "colour is a per-frame argument");

        rcRow(.gap = 14, .align = "cl") {
            rcBox(.align = "cc", .w = "96px", .h = "96px") {
                rcIconRayClayLogoMono(96.0f, live);
            }
            rcColumn(.gap = 4, .w = "grow") {
                rcTextL("RC_IconCallback(size, colour)", .font = F_SMALL, .color = s.text);
                RC_String rgb = rcFormat(rcAppArena(app), "rgb(%d, %d, %d)",
                                            (int)live.r, (int)live.g, (int)live.b);
                rcText(rgb, .font = F_SMALL, .color = s.textMuted);
                rcTextL("redrawn from vectors every frame",
                         .font = F_SMALL, .color = s.textMuted);
            }
        }

        rcRow(.gap = 8, .align = "cl") {
            icon_tile("lv_settings",    rcIconSettings,   wave[0]);
            icon_tile("lv_panel_left",  rcIconPanelLeft,  wave[1]);
            icon_tile("lv_panel_right", rcIconPanelRight, wave[2]);
            icon_tile("lv_minus",       rcIconMinus,      wave[3]);
            icon_tile("lv_x",           rcIconX,          wave[4]);
        }
        rcRow(.gap = 8, .align = "cl") {
            icon_tile("lv_expand",   rcIconExpand,   wave[5]);
            icon_tile("lv_shrink",   rcIconShrink,   wave[6]);
            icon_tile("lv_maximize", rcIconMaximize, wave[7]);
            icon_tile("lv_minimize", rcIconMinimize, wave[8]);
        }

        /* Freeze holds one frame's colours so a single one can be inspected. */
        rcRow(.gap = 12, .align = "cl") {
            if (rcButton("btn_hue_rand", "Randomise", RC_BTN_DEFAULT)) {
                st->hue       = rng_unit(&st->rng);
                st->hueSpread = 0.02f + 0.14f * rng_unit(&st->rng);
            }
            rcToggle("tg_hue_freeze", &st->hueFrozen);
            /* Name what the toggle COSTS, not just which way it is set. The
               third state is not the toggle at all - it is update() having
               sampled rcIsModalOpen() and stood the animation down. */
            rcTextC(st->animHeld  ? "Held - a modal dialog is open (rcIsModalOpen)"
                    : st->hueFrozen ? "Frozen - this window is parked at ~0 CPU"
                                    : "Cycling - requesting a frame per tick",
                     .font = F_SMALL, .color = s.textMuted);
        }
        rcRow(.gap = 12, .align = "cl") {
            rcTextL("Speed", .font = F_SMALL, .color = s.textMuted);
            rcBox(.w = "160px") { rcSlider("sl_hue_speed", &st->hueSpeed, 0.02f, 0.60f); }
            RC_String cps = rcFormat(rcAppArena(app), "%.2f cyc/s", st->hueSpeed);
            rcText(cps, .font = F_SMALL, .color = s.textMuted);
        }
    }
}

/* OWNING AN ARENA. Every other rcFormat in this file writes into rcAppArena(app),
   the runner's scratch, which is reset for you at the top of every frame. That is
   right for a label you rebuild each frame and wrong for anything that must
   survive into the next one. rcArenaInit takes a byte budget, rcArenaAlloc is a
   pointer bump, rcArenaReset reclaims everything at once, and there is no
   per-allocation free - that is the trade.

   THE FOOTGUN: rcArenaReset rewinds the WHOLE arena, so every pointer you took
   from it - including the entry array below - dangles afterwards. */
static void arena_log_clear(AppState *st) {
    rcArenaReset(&st->logArena);
    /* MUST come after the reset, and its result MUST be re-stored: the previous
       array pointer died with the rewind above. */
    st->logLines = (RC_String *)rcArenaAlloc(&st->logArena,
                                             sizeof(RC_String) * LOG_MAX);
    st->logCount = 0;
}

static void section_arena(RC_App *app, AppState *st) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_ARENA, "rcArenaInit / Alloc / Reset / Free");

        rcRow(.gap = 8, .align = "cl") {
            if (rcButton("arena_add", "Log an event", RC_BTN_PRIMARY)
                && st->logLines && st->logCount < LOG_MAX) {
                /* Formatted into OUR arena, so it is still here next frame -
                   the same call against rcAppArena(app) would be gone. */
                st->logLines[st->logCount++] =
                    rcFormat(&st->logArena, "event %d  -  logged on frame %ld",
                             ++st->logSeq, st->frame);
            }
            if (rcButton("arena_clear", "Clear", RC_BTN_DEFAULT))
                arena_log_clear(st);
        }

        /* currOffset and bufferLength are public: watching the bump pointer
           climb is the clearest picture of what an arena is. size_t through %lu
           with a cast rather than %zu, which mingw's CRT may not honour. */
        RC_String used = rcFormat(rcAppArena(app),
                                  "%lu of %lu bytes used   %d of %d entries%s",
                                  (unsigned long)st->logArena.currOffset,
                                  (unsigned long)st->logArena.bufferLength,
                                  st->logCount, LOG_MAX,
                                  st->logCount >= LOG_MAX ? "   (full)" : "");
        rcText(used, .font = F_SMALL, .color = s.textMuted);

        rcColumn(.bg = s.surfaceAlt, .gap = 6, .p = 10, .borderRadius = "all-lg",
                 .w = "grow") {
            if (st->logCount == 0) {
                rcTextL("Nothing logged yet - the arena is empty.",
                         .font = F_SMALL, .color = s.textMuted);
            } else {
                for (int i = 0; i < st->logCount; i++)
                    rcText(st->logLines[i], .font = F_SMALL, .color = s.text);
            }
        }
    }
}

/* A fixed-height, vertically scrolled list - and the calls that move one from
   CODE: rcScrollToTop / rcScrollToBottom jump to an end, rcScrollBy nudges by a
   pixel delta, rcGetScrollInfo reads back where it landed. The buttons are
   declared BEFORE the list on purpose: a scroll call takes effect where you make
   it, so driving the container before it is laid out moves it on THIS frame.

   ONE SIGN TRAP:
     rcScrollBy(id, 0, dy)   positive-DOWN, like the DOM's element.scrollBy
     RC_ScrollInfo.offsetY   positive-DOWN, like element.scrollTop  -> AGREES
     rcScrollDeltaY()        positive-UP, and in wheel NOTCHES, not pixels */
static void section_scroll(RC_App *app, bool stack) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_SCROLL, NULL);

        /* Drive the container first - see the ordering note above. */
        rcBox(.gap = 8, .className = WRAP_DIR(stack)) {
            rcRow(.gap = 8, .align = "cl") {
                if (rcButton("scr_top", "Top", RC_BTN_DEFAULT))
                    rcScrollToTop("ScrollArea");
                if (rcButton("scr_pgup", "Page up", RC_BTN_DEFAULT))
                    rcScrollBy("ScrollArea", 0.0f, -160.0f);  /* up = NEGATIVE */
            }
            rcRow(.gap = 8, .align = "cl") {
                if (rcButton("scr_pgdn", "Page down", RC_BTN_DEFAULT))
                    rcScrollBy("ScrollArea", 0.0f, +160.0f);  /* ~one viewport */
                if (rcButton("scr_bottom", "Bottom", RC_BTN_DEFAULT))
                    rcScrollToBottom("ScrollArea");
            }
        }

        /* .found stays false until the container has been laid out once, so the
           first frame reports "not laid out" instead of a confident 0 of 0. */
        RC_ScrollInfo sc = rcGetScrollInfo("ScrollArea");
        RC_String pos;
        if (sc.found) {
            int pct = sc.maxOffsetY > 0.0f
                    ? (int)(sc.offsetY / sc.maxOffsetY * 100.0f + 0.5f) : 100;
            pos = rcFormat(rcAppArena(app),
                           "offset %.0f of %.0f px  (%d%%)   wheel %+.0f notches this frame",
                           sc.offsetY, sc.maxOffsetY, pct, rcScrollDeltaY());
        } else {
            pos = rcFormat(rcAppArena(app), "ScrollArea has not been laid out yet");
        }
        rcText(pos, .font = F_SMALL, .color = s.textMuted);

        rcColumn(.id = "ScrollArea", .bg = s.surfaceAlt, .gap = 8, .p = 10,
                 .scroll = "v", .borderRadius = "all-lg", .w = "grow", .h = "180px") {
            for (int i = 0; i < 20; i++) {
                rcRow(.bg = s.surface, .px = 12, .align = "cl", .borderRadius = "all-md",
                      .w = "grow", .h = "32px") {
                    /* Distinct per-row labels, so the wheel visibly moves the
                       list: identical rows at the wheel's own pitch do not. */
                    RC_String label = rcFormat(rcAppArena(app),
                        "Row %2d  -  scrolled & clipped to the box", i + 1);
                    rcText(label, .font = F_SMALL, .color = s.textMuted);
                }
            }
        }
    }
}

/* A NON-MODAL popup: rcBeginModal with .modality = RC_MODALITY_NON_MODAL, the
   same call as the modal dialog below minus the scrim, so the app behind stays
   live and the panel can be left open.

   THE TRAP: RC_MODALITY_NON_MODAL ALONE is not "leave it open and keep working".
   Modality and DISMISSAL are separate axes, and turning off the first does not
   touch the second - an outside press still closes the panel, and with no scrim
   "outside" means anywhere in the app. Staying open is the PAIR:

       .modality = RC_MODALITY_NON_MODAL, .noBackdropDismiss = true

   The checkbox flips that second flag at runtime, so both halves are reachable. */
static void inspector_panel(RC_App *app, AppState *st) {
    RC_Style s = rcGetStyle();
    /* Designators in DECLARATION order - .noBackdropDismiss before .modality.
       C++20 requires it and g++ hard-errors on the other order. */
    if (!rcBeginModal("inspector", &st->inspectorOpen,
                      .noBackdropDismiss = st->inspectorSticky, /* ...and THIS is what keeps it open */
                      .modality          = RC_MODALITY_NON_MODAL)) /* no scrim -> the app stays live */
        return;

    rcTextL("Inspector (non-modal)", .font = F_BODY, .color = s.text);
    rcTextL("Leave me open. Drag Volume behind me and watch these move.",
             .font = F_SMALL, .color = s.textMuted);

    /* These read the SAME state the widgets behind are editing, and keep moving
       while the panel is open - which is the proof nothing is blocked. */
    RC_String live = rcFormat(rcAppArena(app),
                                 "volume %d%%   quality %d   clicks %d   frame %ld",
                                 (int)(st->volume * 100.0f + 0.5f),
                                 st->quality, st->clicks, st->frame);
    rcText(live, .font = F_SMALL, .color = s.text);

    rcCheckbox("insp_sticky", "Stay open when I click away", &st->inspectorSticky);
    rcTextC(st->inspectorSticky
                 ? "RC_MODALITY_NON_MODAL + noBackdropDismiss: clicks outside are ignored."
                 : "RC_MODALITY_NON_MODAL alone: the next click outside CLOSES this panel.",
             .font = F_SMALL, .color = st->inspectorSticky ? s.textMuted : s.danger);

    if (rcButton("insp_close", "Close", RC_BTN_DEFAULT))
        st->inspectorOpen = false;

    rcEndModal();
}

static void section_widgets(RC_App *app, AppState *st, bool stack) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_WIDGETS, "native, interactive");
        rcRow(.gap = 10, .align = "cl") {
            if (rcButton("btn_primary", "Primary", RC_BTN_PRIMARY)) st->clicks++;
            if (rcButton("btn_default", "Default", RC_BTN_DEFAULT)) st->clicks++;
            if (rcButton("btn_reset",   "Reset",   RC_BTN_DANGER))  st->clicks = 0;
            if (rcButton("btn_ghost",   "Ghost",   RC_BTN_GHOST))   st->clicks++;
        }
        RC_String clicks = rcFormat(rcAppArena(app), "clicks: %d", st->clicks);
        rcText(clicks, .font = F_SMALL, .color = s.textMuted);

        rcRow(.gap = 16, .align = "cl") {
            rcCheckbox("cb_details", "Show details", &st->showDetails);
            rcToggle("tg_dark", &st->darkMode);
            rcTextC(st->darkMode ? "Dark theme" : "Light theme",
                     .font = F_SMALL, .color = s.textMuted);
        }
        if (st->showDetails) {
            rcTextL("Details shown because the checkbox is checked.",
                     .font = F_SMALL, .color = s.textMuted);
        }

            /* Text inputs: click to focus, type, Ctrl+A/C/V. The accents in the
               placeholder are load-bearing - text YOU supply renders the full
               Latin-1 window. (Typed input is filtered to ASCII, and a
               placeholder is drawn, never edited.) */
        rcBox(.gap = 10, .align = "cl", .className = WRAP_DIR(stack)) {
            rcBox(.w = "220px") {
                rcTextInput("in_name", st->name, sizeof st->name,
                             .placeholder = "Your name (e.g. Zoë Müller)");
            }
            rcRow(.gap = 10, .align = "cl") {
                rcBox(.w = "160px") {
                    rcTextInput("in_secret", st->secret, sizeof st->secret,
                                 .placeholder = "Password", .password = true);
                }
                /* rcIsFocused asks the FIELD, by id, rather than tracking focus
                   yourself: focus also moves by Tab and by clicking away, so an
                   app-owned "isEditing" flag drifts the first time it does. */
                rcBox(.bg = (rcIsFocused("in_name") || rcIsFocused("in_secret"))
                           ? s.successHover : s.textMuted,
                      .borderRadius = "all-full", .w = "10px", .h = "10px") {}
            }
        }
        RC_String hello = rcFormat(rcAppArena(app),
                                      st->name[0] ? "Hello, %s!" : "(type a name above)",
                                      st->name);
        rcText(hello, .font = F_SMALL, .color = s.textMuted);

        /* SELECTABLE TEXT. rcTextArea is rcTextInput with .multiline preset:
           Enter inserts a newline, long lines soft-wrap, Up/Down move by ROW,
           and the editor drives YOUR buffer - there is no widget object holding
           it. STATIC TEXT IS SELECTABLE TOO, and by default, so every label and
           cell here can be dragged across and copied. What this field adds is
           EDITING; .select is for the other direction. */
        rcBox(.py = 4, .w = "grow") {
            rcTextArea("draft", st->draft, sizeof st->draft,
                       .placeholder = "Type here, then drag across what you typed",
                       .font = F_SMALL, .rows = 5);
        }
        rcTextL("That box is EDITABLE - typing is what it adds. Selection is on "
                 "everywhere by default, so drag across this caption, a heading or "
                 "a table cell and copy it. Chrome opts out with .select.",
                 .font = F_SMALL, .color = s.textMuted);

        /* THE OPT-OUT, SHOWN RATHER THAN DESCRIBED: drag across both and only
           the first highlights. rcSelectable(false) is RC_SELECT_NONE. */
        rcRow(.gap = 12, .align = "cl", .w = "grow") {
            rcTextL("Drag across me: I highlight (the default).",
                    .font = F_SMALL, .color = s.text);
            rcTextL("Drag across me: I do not (.select = rcSelectable(false)).",
                    .font = F_SMALL, .color = s.textMuted,
                    .select = rcSelectable(false));
        }

        /* TAKING THE SELECTION WITH NO KEYBOARD: rcSelectAll and rcCopySelection
           on ordinary buttons, because a phone cannot make the accelerator.
           Both take the RELEASE EDGE, which is what rcButton gives you, and that
           works for Copy because a press DISMISSES a selection rather than
           ending it - the span stays readable until the pointer comes up.
           rcHasSelection drives the VARIANT, never the declaration: gating the
           declaration would take the control out of the layout mid-gesture. */
        rcRow(.gap = 10, .align = "cl", .w = "grow") {
            bool live = rcHasSelection();

            if (rcButton("sel_all", "Select all", RC_BTN_DEFAULT))
                rcSelectAll();

            if (rcButton("sel_copy", "Copy selection",
                         live ? RC_BTN_PRIMARY : RC_BTN_DEFAULT))
                st->selCopies += rcCopySelection() ? 1 : 0;

            /* rcTextC, not rcTextL: rcTextL takes a compile-time LITERAL, and a
               ternary is not one. */
            rcTextC(live ? "Selected - press Copy, or the primary modifier and C."
                         : "Nothing selected: drag across a line, or press Select all.",
                     .font = F_SMALL, .color = s.textMuted);
            if (st->selCopies) {
                rcText(rcFormat(rcAppArena(app), "%d copied", st->selCopies),
                        .font = F_SMALL, .color = s.textMuted);
            }
        }

        /* MODAL vs NON-MODAL: the modal draws a scrim and the app behind goes
           dead; the inspector draws none and the gallery keeps working. */
        rcRow(.gap = 10, .align = "cl") {
            if (rcButton("btn_modal", "Open dialog", RC_BTN_DEFAULT))
                st->modalOpen = true;
            if (rcButton("btn_inspector", "Open inspector", RC_BTN_DEFAULT))
                st->inspectorOpen = true;
        }
        if (rcBeginModal("dlg_demo", &st->modalOpen)) {
            rcTextL("Delete this item?", .color = s.text);
            rcTextL("This can't be undone.", .font = F_SMALL, .color = s.textMuted);
            rcRow(.gap = 10, .align = "cl") {
                if (rcButton("dlg_cancel", "Cancel", RC_BTN_DEFAULT)) st->modalOpen = false;
                if (rcButton("dlg_delete", "Delete", RC_BTN_DANGER))  st->modalOpen = false;
            }
            rcEndModal();
        }
        inspector_panel(app, st);
    }
}

static void section_controls(RC_App *app, AppState *st, bool narrow) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_CONTROLS, "slider / progress / radio");

        /* The slider, the bar and the combo are fixed widths where there is room
           and fill the line where there is not. */
        rcRow(.gap = 12, .align = "cl", .w = "grow") {
            rcTextL("Volume", .font = F_SMALL, .color = s.textMuted);
            rcBox(.w = narrow ? "grow" : "220px") {
                rcSlider("sl_volume", &st->volume, 0.0f, 1.0f);
            }
            RC_String pct = rcFormat(rcAppArena(app), "%d%%",
                                        (int)(st->volume * 100.0f + 0.5f));
            rcText(pct, .font = F_SMALL, .color = s.textMuted);
        }

        rcRow(.gap = 12, .align = "cl", .w = "grow") {
            rcTextL("Loading", .font = F_SMALL, .color = s.textMuted);
            /* Animated on its own, so it is clearly a separate widget. */
            float t    = (float)(st->frame % 480) / 480.0f;
            float load = t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;
            rcBox(.w = narrow ? "grow" : "260px") { rcProgress("pr_load", load); }
        }

        rcRow(.gap = 16, .align = "cl") {
            rcRadio("rb_low",  "Low",    &st->quality, 0);
            rcRadio("rb_med",  "Medium", &st->quality, 1);
            rcRadio("rb_high", "High",   &st->quality, 2);
        }
        const char *qn = st->quality == 0 ? "Low"
                       : st->quality == 1 ? "Medium" : "High";
        RC_String q = rcFormat(rcAppArena(app), "quality: %s", qn);
        rcText(q, .font = F_SMALL, .color = s.textMuted);

        /* A combo - its popup floats above the content below it. */
        static const char *const presets[] = {
            "Default", "Compact", "Comfortable", "Spacious"
        };
        rcRow(.gap = 12, .align = "cl", .w = "grow") {
            rcTextL("Preset", .font = F_SMALL, .color = s.textMuted);
            rcBox(.w = narrow ? "grow" : "220px") {
                rcCombo("cb_preset", &st->preset, presets, 4);
            }
        }
        rcText(rcFormat(rcAppArena(app), "preset: %s", presets[st->preset]),
               .font = F_SMALL, .color = s.textMuted);
    }
}

static void update(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    /* Load the one demo image once (the renderer is up by the first update). */
    static bool imageReady = false;
    if (!imageReady) {
        st->imageFromMem = demo_image_load();
        imageReady       = true;
        st->imageLoads++;
    }

    /* The image-lifecycle buttons land here rather than in the layout callback:
       layout DECLARES a frame, so changing what that frame draws from while it
       is being declared is the wrong shape to teach. */
    if (st->imageAction == 1) {
        rcUnloadImage(&g_demo_image);
    } else if (st->imageAction == 2 && !g_demo_image.handle) {
        st->imageFromMem = demo_image_load();
        st->imageLoads++;
    }
    st->imageAction = 0;

    /* Zoom-badge trigger: a zoom gesture is applied before the callbacks, so
       poll-and-compare IS the change trigger and no callback is needed. */
    float z = rcWindowZoom(rcAppMainWindow(app));
    if (z != st->prevZoom) {
        if (st->prevZoom > 0.0f)
            st->zoomBadgeSecs = 1.5f;
        st->prevZoom = z;
    }
    /* ONE true delta per frame, and everything that moves reads it. rcAppTime is
       the app's own monotonic clock, so subtracting last frame's reading gives
       the exact wall gap. rcWindowFrameTime cannot: it is a moving average AND
       it discards any interval of a second or more. */
    double nowSecs = rcAppTime(app);
    float  dt      = st->prevTime > 0.0 ? (float)(nowSecs - st->prevTime) : 0.0f;
    st->prevTime = nowSecs;
    /* Clamped after a stall - a debugger breakpoint, a window drag - so one huge
       frame steps the hue and the badge rather than jumping them. */
    if (dt > 0.1f) dt = 0.1f;

    if (st->zoomBadgeSecs > 0.0f)
        st->zoomBadgeSecs -= dt;

    /* Drive the zoom-mode toggle into the window once per frame; the label in
       section_zoom reads it back with rcWindowZoomMode. */
    rcWindowSetZoomMode(rcAppMainWindow(app), st->opticalZoom ? RC_ZOOM_OPTICAL : RC_ZOOM_LAYOUT);

    /* Hold the animation while a MODAL dialog is up. This is what rcIsModalOpen
       is for: not a guard against the dialog drawing wrongly, but the app
       declining to burn frames on something nobody can see - a video, a poll, a
       simulation tick. And it DISCRIMINATES: the non-modal inspector leaves this
       false and the colours keep cycling, because modality is the question being
       asked, not visibility. */
    const bool inDialog = rcIsModalOpen();
    st->animHeld = inDialog;   /* sampled ONCE, here; the label reads this back */

    if (!st->hueFrozen && !inDialog)
        st->hue = hue_wrap(st->hue + dt * st->hueSpeed);

    /* Two things move with no input: the hue and the badge fading out. RayClay
       draws only when asked, so keep asking while either is running. */
    if ((!st->hueFrozen && !inDialog) || st->zoomBadgeSecs > 0.0f)
        rcWindowRequestFrame(rcAppMainWindow(app));

    st->frame++;
}

/* Dataviz. Every dataset below is file-scope and static, which satisfies the
   "y/x arrays are BORROWED until rcRender()" contract for free. */

static const float demo_rev[]    = { 42, 55, 48, 61, 58, 72, 69, 81, 77, 90, 85, 98 };
static const float demo_target[] = { 50, 52, 54, 56, 58, 60, 66, 70, 74, 80, 86, 92 };
/* Independent spot-checks over the same months, drawn as SCATTER: discrete
   observations, and a line through them would imply a continuity they lack. */
static const float demo_audit[]  = { 45, 51, 50, 63, 55, 70, 72, 79, 80, 88, 83, 96 };
static const float demo_spark[]  = { 3, 5, 4, 7, 6, 9, 8, 6, 7, 10, 9, 12, 11, 13, 12, 15 };

enum { MANY_SERIES = 16, MANY_POINTS = 24 };
static float demo_many[MANY_SERIES][MANY_POINTS];

static void many_series_fill(void) {
    static bool filled = false;

    if (filled)
        return;
    for (int s = 0; s < MANY_SERIES; s++) {
        for (int i = 0; i < MANY_POINTS; i++) {
            /* Deterministic, so the frame is byte-identical run to run. */
            int wobble = (s * 7 + i * 13) % 29;
            demo_many[s][i] = 40.0f + (float)s * 3.0f + (float)wobble;
        }
    }
    filled = true;
}

/* A long enough trace that zooming into it is worth doing, and deterministic. */
enum { ZOOM_POINTS = 240 };
static float demo_zoom[ZOOM_POINTS];

static void zoom_series_fill(void) {
    static bool filled = false;

    if (filled)
        return;
    for (int i = 0; i < ZOOM_POINTS; i++) {
        /* A slow rise with two ripples on it, so a zoomed-in window shows
           detail that is invisible at the full range. */
        int fast = (i * 17) % 23;
        int slow = (i * 5)  % 61;

        demo_zoom[i] = 30.0f + (float)i * 0.25f + (float)fast * 0.8f
                                 + (float)slow * 0.35f;
    }
    filled = true;
}

static void section_charts(RC_App *app, AppState *st, bool stack) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_CHARTS, "rcChart / rcSparkline");
        /* A multi-series chart, and the one place ALL THREE kinds meet: revenue
           BARS behind a target LINE with audit SCATTER over both. rcChart GROWS
           to fill, so it is wrapped in a sized box. */
        rcBox(.w = "grow", .h = "160px") {
            RC_Series ser[] = {
                { .y = demo_rev,    .count = 12, .kind = RC_SERIES_BAR,  .label = "revenue" },
                { .y = demo_target, .count = 12, .kind = RC_SERIES_LINE,
                  .color = s.primary, .label = "target", .thickness = 2.0f },
                /* SCATTER draws markers and no line. Note .thickness means the
                   marker RADIUS on this kind, not a stroke width; 0 gives 3. */
                { .y = demo_audit,  .count = 12, .kind = RC_SERIES_SCATTER,
                  .color = RC_AMBER_500, .label = "audit", .thickness = 3.5f },
            };
            rcChart("gal_chart", ser, 3,
                     RC_LIT(RC_ChartOptions){.y = { .grid = true }, .legend = true,
                                       .tooltip      = RC_CHART_TOOLTIP_NEAREST,
                                       .tooltipPlace = (RC_ChartTooltipPlace)st->tipPlace,
                                       .hoverGuide   = st->hoverGuide,
                                       .hoverMarkers = st->hoverMarkers});
        }
        /* WHICH datum the readout names follows the mark geometry: a LINE is
           continuous, so every x has a reading, while a BAR or SCATTER is
           discrete and the pointer has to be ON one. WHETHER there is a readout
           (.tooltip) and WHERE it sits (.tooltipPlace) are separate fields -
           cursor follows the pointer, corner parks opposite it so it never
           covers the data, fixed pins at .tooltipAnchor. Every mode is clamped
           into the visible view. The combo index IS the enum value. */
        rcBox(.gap = 10, .align = "cl", .w = "grow", .className = WRAP_DIR(stack)) {
            rcRow(.gap = 10, .align = "cl") {
                rcTextL("tooltip placement", .font = F_SMALL, .color = s.textMuted);
                static const char *const places[] = { "cursor", "corner", "fixed" };
                rcBox(.w = "150px") {
                    rcCombo("cb_tipplace", &st->tipPlace, places, 3);
                }
            }
            /* The panel gives the numbers; these two say WHICH LINE each came
               from. Both are independent of .tooltip. */
            rcRow(.gap = 10, .align = "cl") {
                rcCheckbox("cb_hguide",   "guide",   &st->hoverGuide);
                rcCheckbox("cb_hmarkers", "markers", &st->hoverMarkers);
            }
        }
        /* The same trace three ways in the bare inline form, where the whole box
           IS the plot: a table cell, a tile, a live strip. */
        rcRow(.gap = 10, .align = "cl", .w = "grow") {
            rcBox(.w = "grow", .h = "32px") {
                rcSparkline("gal_sl_line", demo_spark, 16, RC_LIT(RC_SparklineOptions){0});
            }
            rcBox(.w = "grow", .h = "32px") {
                rcSparkline("gal_sl_area", demo_spark, 16,
                             RC_LIT(RC_SparklineOptions){ .kind = RC_SERIES_AREA, .color = s.primary });
            }
            rcBox(.w = "grow", .h = "32px") {
                rcSparkline("gal_sl_bar", demo_spark, 16,
                             RC_LIT(RC_SparklineOptions){ .kind = RC_SERIES_BAR, .color = RC_EMERALD_500 });
            }
        }

        /* MANY SERIES. Charts draw up to 16; hand rcChart more and it warns
           once, draws the first 16, and adds a "+N more" chip in the plot, so a
           truncated dashboard says so instead of reading as "those metrics went
           flat". The cap is a READABILITY limit - past about a dozen hues, set
           .color yourself. The RC_Series descriptors are COPIED, so this local
           array is fine; it is the .y arrays that are borrowed. */
        many_series_fill();
        section_heading(SEC_MANY, "sixteen, the cap");
        rcBox(.w = "grow", .h = "180px") {
            RC_Series many[MANY_SERIES];

            for (int i = 0; i < MANY_SERIES; i++)
                many[i] = RC_LIT(RC_Series){ .y     = demo_many[i],
                                       .count = MANY_POINTS,
                                       .kind  = RC_SERIES_LINE };
            rcChart("gal_many", many, MANY_SERIES,
                     RC_LIT(RC_ChartOptions){ .y = { .grid = true }, });
        }
        rcTextL("Sixteen auto-coloured series in one plot. Beyond ~12 hues nobody can tell two lines apart - assign .color yourself.",
                 .font = F_SMALL, .color = s.textMuted);

        /* DRAG-TO-ZOOM. No zoom widget is needed: the view is two floats this
           app owns, and immediate mode re-plots at whatever they hold. What the
           library supplies is the MAPPING - and rcChartPlotRect, NOT
           rcGetElementBox(chartId), because the chart sizes its plot INSIDE the
           box it was given and the outer box includes whatever chrome it chose. */
        zoom_series_fill();
        section_heading(SEC_DRAGZOOM, "rcChartPlotRect and the pointer reads");
        rcBox(.w = "grow", .h = "180px") {
            RC_Series z = { .y     = demo_zoom,
                            .count = ZOOM_POINTS,
                            .kind  = RC_SERIES_LINE,
                            .color = s.primary };
            RC_Box       plot = rcChartPlotRect("gal_zoom");
            RC_Vec2 p    = rcPointer();

            /* The WIDTH check is the readiness test, not .found: .found answers
               "does an element with this id exist?", and an element's first
               frame answers TRUE with an all-zero rect. */
            if (plot.found && plot.width > 0.0f) {
                float t     = (p.x - plot.x) / plot.width;
                float dataX = st->zoomLo + t * (st->zoomHi - st->zoomLo);

                /* Same reason as the scrub field above: a brush is a drag, so it
                   names its own cursor. Nothing polls this plot for a click. */
                if (rcIsHovered("gal_zoom") || st->brushing)
                    rcSetCursor(st->brushing ? RC_CURSOR_GRABBING : RC_CURSOR_GRAB);

                if (rcPointerPressed(RC_POINTER_LEFT) && rcIsHovered("gal_zoom")) {
                    st->brushing = true;
                    st->brushA   = st->brushB = dataX;
                } else if (st->brushing && rcPointerDown(RC_POINTER_LEFT)) {
                    st->brushB = dataX;
                } else if (st->brushing && rcPointerReleased(RC_POINTER_LEFT)) {
                    float a = st->brushA, b = st->brushB;

                    st->brushing = false;
                    if (b < a) { float sw = a; a = b; b = sw; }
                    /* Clamp to the domain, and reject a click with no drag -
                       it would collapse the range with no way back but Reset. */
                    if (a < 0.0f) a = 0.0f;
                    if (b > (float)(ZOOM_POINTS - 1)) b = (float)(ZOOM_POINTS - 1);
                    if (b - a > 1.0f) { st->zoomLo = a; st->zoomHi = b; }
                }
            }
            rcChart("gal_zoom", &z, 1,
                     RC_LIT(RC_ChartOptions){ .x = { .min = st->zoomLo, .max = st->zoomHi },
                                        .y = { .grid = true },
                                        .tooltip = RC_CHART_TOOLTIP_NEAREST,
                                        .tooltipPlace = RC_TOOLTIP_PLACE_CORNER });
        }
        rcRow(.gap = 10, .align = "cl", .w = "grow") {
            RC_String rng = rcFormat(rcAppArena(app), "%s  x: %.0f - %.0f  of  0 - %d",
                                        st->brushing ? "brushing" : "drag across the plot",
                                        (double)st->zoomLo, (double)st->zoomHi,
                                        ZOOM_POINTS - 1);

            rcText(rng, .font = F_SMALL, .color = s.textMuted);
            rcBox(.w = "grow") {}
            if (rcButton("zoom_reset", "Reset", RC_BTN_DEFAULT)) {
                st->zoomLo   = 0.0f;
                st->zoomHi   = (float)(ZOOM_POINTS - 1);
                st->brushing = false;
            }
        }
    }
}

static void section_table(RC_App *app, AppState *st) {
    RC_Style s = rcGetStyle();
    /* Eight rows in a short box, so the body scrolls under the sticky header. */
    static const char *const names[]  = { "Alpha", "Bravo", "Charlie", "Delta",
                                          "Echo", "Foxtrot", "Golf", "Hotel" };
    static const char *const values[] = { "1,240", "980", "2,015", "1,660",
                                          "740", "1,905", "612", "1,430" };
    static const char *const deltas[] = { "+4.2%", "-1.1%", "+8.0%", "+2.7%",
                                          "-0.5%", "+3.3%", "-2.4%", "+1.8%" };
    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_TABLE, "rcBeginTable: sticky header, pick a row");
        /* Column widths read as CSS strings, just like .w on a box. */
        RC_TableColumn cols[] = {
            { .header = "Name",   .align = "cl", .w = "grow" },
            { .header = "Value",  .align = "cr", .w = "76px" },
            { .header = "Change", .align = "cr", .w = "76px" },
        };
        rcBox(.w = "grow", .h = "132px") {
            /* cellPad 10 (default 6) keeps the right-aligned numerics clear of
               the overlay scrollbar riding the table body's right edge. */
            if (rcBeginTable("gal_table", cols, 3,
                             RC_LIT(RC_TableOptions){ .cellPadding = RC_VAL(10) })) {
                for (int i = 0; i < 8; i++) {
                    /* rcTableRowId, not rcTableRow: it makes the ROW the
                       hit-test target. Build the id from the DATA index, never
                       the screen position, or a scroll renames every row. */
                    const char *rowId = rcFormat(rcAppArena(app), "gal_row%d", i).chars;
                    RC_Color    ink   = st->tableRow == i ? s.primary : s.text;

                    rcTableRowId(rowId);
                    if (rcClicked(rowId))
                        st->tableRow = i;
                    rcTextC(names[i],  .font = F_SMALL, .color = ink);
                    rcTableNext();
                    rcTextC(values[i], .font = F_SMALL, .color = ink);
                    rcTableNext();
                    rcTextC(deltas[i], .font = F_SMALL,
                             .color = deltas[i][0] == '+' ? s.successHover : s.danger);
                }
                rcEndTable();
            }
        }
        rcText(rcFormat(rcAppArena(app), "selected: %s",
                        st->tableRow >= 0 ? names[st->tableRow] : "click a row"),
               .font = F_SMALL, .color = s.textMuted);
    }
}

/* BIG TABLE. Layout charges per DECLARED element, so declaring 5,000 rows to
   show seven costs more than an entire gallery frame, and culling cannot help -
   an element must be sized and positioned before anything knows it is offscreen.
   rcVirtualList emits a top spacer, the visible window and a bottom spacer.

   Two rules that bite. Pass the row PITCH, not the cell height: a 26px cell with
   4px of padding each side is a pitch of 34, and handing the list 26 makes it
   run short. And cell text comes from rcFormat, never a stack buffer - rcText
   BORROWS the pointer and the cell is drawn after this scope ends. A row *id*
   may be a stack buffer; text may not.

   Nothing holds the dataset in RAM: each visible cell is synthesised from its
   row index, the shape a real app has when rows arrive from a socket. */
enum { BIGTABLE_ROWS = 5000, BIGTABLE_ROW_H = 26,
       BIGTABLE_CELL_PAD = 4, BIGTABLE_PITCH = BIGTABLE_ROW_H + 2 * BIGTABLE_CELL_PAD };

static void section_bigtable(RC_App *app) {
    RC_Style  s   = rcGetStyle();
    RC_Arena *mem = rcAppArena(app);
    static const char *const kinds[] = { "temp", "humid", "press", "lux",
                                         "co2",  "pm25",  "volt",  "flow" };

    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_BIGTABLE,
                        "rcVirtualList: 5,000 rows, only the visible window declared");
        RC_TableColumn cols[] = {
            { .header = "#",       .align = "cr", .w = "64px" },
            { .header = "Sensor",  .align = "cl", .w = "grow" },
            { .header = "Reading", .align = "cr", .w = "92px" },
        };
        /* 26vh, NOT a pixel height. Layout zoom shrinks the logical viewport, so
           a fixed box eventually grows taller than the whole layout and
           rcVirtualList clamps with a warning. A viewport unit is a fraction of
           that same shrinking viewport, so the ratio holds at every zoom. */
        rcBox(.w = "grow", .h = "26vh") {
            if (rcBeginTable("gal_bigtable", cols, 3,
                              RC_LIT(RC_TableOptions){ .cellPadding = RC_VAL(BIGTABLE_CELL_PAD) })) {
                rcVirtualList(row, "gal_bigtable", BIGTABLE_ROWS, BIGTABLE_PITCH) {
                    /* Deterministic stand-in for real data - no stored array. */
                    int  v   = (row.index * 37) % 900 + 100;
                    bool hot = v > 800;
                    rcTableRow();
                    rcBox(.px = 8, .align = "cr", .w = "grow", .h = "26px") {
                        rcText(rcFormat(mem, "%d", row.index + 1),
                                .font = F_SMALL, .color = s.textMuted);
                    }
                    rcTableNext();
                    rcBox(.px = 8, .align = "cl", .w = "grow", .h = "26px") {
                        rcText(rcFormat(mem, "%s-%04d", kinds[row.index & 7], row.index),
                                .font = F_SMALL, .color = s.text);
                    }
                    rcTableNext();
                    rcBox(.px = 8, .align = "cr", .w = "grow", .h = "26px") {
                        rcText(rcFormat(mem, "%d.%d", v / 10, v % 10),
                                .font = F_SMALL, .color = hot ? s.successHover : s.text);
                    }
                }
                rcEndTable();
            }
        }
        rcScrollbar("gal_bigtable");
        rcTextL("Scroll it: the frame cost is flat in row count - only the visible rows are declared.",
                .font = F_SMALL, .color = s.textMuted);
    }
}

static void section_splitpane(AppState *st) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_SPLITPANE, "rcBeginSplitPane, drag the divider");
        rcBox(.w = "grow", .h = "120px") {
            if (rcBeginSplitPane("gal_split", RC_SPLIT_ROW, &st->splitFrac,
                                  RC_LIT(RC_SplitOptions){0})) {
                rcColumn(.bg = s.surfaceAlt, .align = "cc", .borderRadius = "all-lg",
                         .w = "grow", .h = "grow") {
                    rcTextL("Pane 1", .font = F_BODY, .color = s.text);
                }
                rcSplitHandle();
                rcColumn(.bg = s.surfaceAlt, .align = "cc", .borderRadius = "all-lg",
                         .w = "grow", .h = "grow") {
                    rcTextL("Pane 2", .font = F_BODY, .color = s.textMuted);
                }
                rcEndSplitPane();
            }
        }
    }
}

/* What the app knows about its own surface, and how it schedules work - the
   panel you want when a layout misbehaves on a machine you do not own.

   CONTENT SCALE is the DISPLAY's factor: a 2x HiDPI panel reads 2.0. It is NOT
   rcWindowZoom, which is a separate multiplier the app owns; the two are shown
   side by side and are never added together. rcGetElementBox is the layout
   debugger, and `found` means the id EXISTS, not that its rect is ready - test
   `found && width > 0`, in that order.

   On-demand is the default, and the two ways out of it are opposites:
   rcWindowSetContinuousRendering burns a frame forever, rcWindowRequestFrameAfter
   arms exactly ONE wake and goes back to sleep. */
static void section_display(RC_App *app, AppState *st) {
    RC_Style s = rcGetStyle();

    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading(SEC_DISPLAY, "what the app knows about itself");

        /* Measured on the element declared further down this same section. */
        RC_Box   probe  = rcGetElementBox("diag_probe");
        RC_Insets safe  = rcGetSafeAreaInsets();
        RC_String scale = rcFormat(rcAppArena(app),
                                      "content scale %.2fx (display)      zoom %.0f%% (app)",
                                      rcGetContentScale(), rcWindowZoom(rcAppMainWindow(app)) * 100.0f);
        RC_String inset = rcFormat(rcAppArena(app),
                                      "safe-area insets  t%.0f r%.0f b%.0f l%.0f   (0 unless the host has bands)",
                                      safe.top, safe.right, safe.bottom, safe.left);
        /* BOTH halves of the guard, in this order - see the note above. */
        RC_String box   = (probe.found && probe.width > 0.0f)
                        ? rcFormat(rcAppArena(app),
                                      "diag_probe box  x%.0f y%.0f  %.0f x %.0f",
                                      probe.x, probe.y, probe.width, probe.height)
                        : rcFormat(rcAppArena(app),
                                      "diag_probe box  measuring... (first frame: found, rect still 0)");

        rcText(scale, .font = F_SMALL, .color = s.text);
        rcText(inset, .font = F_SMALL, .color = s.textMuted);
        rcText(box,   .font = F_SMALL, .color = s.textMuted);

        /* The element being measured. An id is the entire requirement. */
        rcRow(.id = "diag_probe", .bg = s.surfaceAlt, .gap = 8, .px = 10, .align = "cl",
              .borderRadius = "all-lg", .w = "grow", .h = "34px") {
            rcTextL("measured element", .font = F_SMALL, .color = s.textMuted);
        }

        /* rcWindowSetZoom REFLOWS the layout, so text re-bakes crisp at the new
           size rather than being scaled. */
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            rcTextL("Zoom", .font = F_SMALL, .color = s.textMuted);
            if (rcButton("diag_zoom_out", "75%",  RC_BTN_DEFAULT)) rcWindowSetZoom(rcAppMainWindow(app), 0.75f);
            if (rcButton("diag_zoom_100", "100%", RC_BTN_DEFAULT)) rcWindowSetZoom(rcAppMainWindow(app), 1.00f);
            if (rcButton("diag_zoom_in",  "125%", RC_BTN_DEFAULT)) rcWindowSetZoom(rcAppMainWindow(app), 1.25f);
        }

        /* PINNING, and the two questions the API keeps apart. rcTopmostSupported
           asks whether the affordance should exist on this machine at all - it
           is false on a Wayland desktop and on the web - so ask it BEFORE
           drawing the control. rcIsWindowTopmost asks what the window system
           actually did, so the toggle is seeded from it every frame and a
           manager that declines shows the control falling back by itself. */
        if (rcTopmostSupported()) {
            bool pinned = rcIsWindowTopmost();
            rcRow(.gap = 10, .align = "cl", .w = "grow") {
                if (rcToggle("diag_topmost", &pinned))
                    rcSetWindowTopmost(pinned);
                rcTextL("keep this window above the others", .font = F_SMALL,
                         .color = s.textMuted);
            }
        } else {
            rcRow(.gap = 10, .align = "cl", .w = "grow") {
                rcTextL("this session cannot pin a window above others",
                         .font = F_SMALL, .color = s.textMuted);
            }
        }

        /* THE REM BASIS. Every `rem` length in a .className string resolves
           against this one number. Typed pixel fields are unaffected, which is
           the difference between the two styling surfaces. */
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            RC_String rem = rcFormat(rcAppArena(app), "root font size %.0f px",
                                        (double)rcRootFontSize());
            rcText(rem, .font = F_SMALL, .color = s.textMuted);
            rcBox(.w = "grow") {}
            if (rcButton("diag_rem_14", "14", RC_BTN_DEFAULT)) rcSetRootFontSize(14.0f);
            if (rcButton("diag_rem_16", "16", RC_BTN_DEFAULT)) rcSetRootFontSize(16.0f);
            if (rcButton("diag_rem_18", "18", RC_BTN_DEFAULT)) rcSetRootFontSize(18.0f);
        }

        /* The continuous toggle is a switch because it is a STATE you leave on;
           the wake is a button because it is a one-shot. */
        rcRow(.gap = 10, .align = "cl", .w = "grow") {
            if (rcToggle("diag_continuous", &st->continuous)) {
                rcWindowSetContinuousRendering(rcAppMainWindow(app), st->continuous);
            }
            rcTextL("continuous rendering", .font = F_SMALL, .color = s.textMuted);
        }
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            if (rcButton("diag_wake", "Wake me in 1s", RC_BTN_DEFAULT)) {
                rcWindowRequestFrameAfter(rcAppMainWindow(app), 1.0);
                st->wakeArmedFrame = st->frame;
                st->wakesArmed++;
            }
            /* Frames elapsed since arming is the observable proof the app really
               parked and really came back: idle, it barely moves. */
            RC_String w = rcFormat(rcAppArena(app),
                                      "armed %d, +%ld frames since",
                                      st->wakesArmed,
                                      st->wakesArmed ? st->frame - st->wakeArmedFrame : 0L);
            rcText(w, .font = F_SMALL, .color = s.textMuted);
        }

        /* The inspector is compiled OUT by default, which is why a stock build
           reads "off (compiled out)" rather than a plain "off". */
        {
            RC_String d = rcFormat(rcAppArena(app), "layout inspector: %s",
#if RC_DEBUG_TOOLS
                                      rcAppIsDebugEnabled(app) ? "on" : "off");
#else
                                      "off (compiled out; -DRC_DEBUG_TOOLS=1)");
#endif
            rcText(d, .font = F_SMALL, .color = s.textMuted);
        }
    }
}

/* TWO WIDTHS, DERIVED FROM THIS GALLERY'S OWN COLUMNS rather than from a table
   of phone sizes, and measured against the viewport LESS its safe-area insets
   because that is the width the root hands the Content row.

   TWO COLUMNS. The widest section, SHADOWS, needs 556; ColRight is 394; the
   Content row spends 20px each side plus 16 between them: 1020. Below that the
   sections stack into ONE scrolling column in the same order.

   STACK. Only under 620 is that one column too narrow for a row of fixed-width
   swatches, so that - not the two-column width - is where WRAP_DIR breaks each
   row into two stacked halves. One flag for both would leave a 750px-wide card
   carrying a layout drawn for a 300px phone.

   NEVER branch a layout on the OS: a desktop window dragged narrow takes the
   same arm a phone does, which is what makes both arms testable without one. */
#define GALLERY_TWO_COLUMN_W 1020.0f
#define GALLERY_STACK_W      620.0f
/* Every scrolling column reserves this for its own overlay scrollbar: the bar
   floats over the container's right edge, so without a lane it is painted
   across a full-width card's border and its rounded corner. */
#define GALLERY_SCROLL_GUTTER 14

/* The sections, in the order the wide arm shows them: ColLeft top to bottom,
   then ColRight. One column calls both, so the reading order on a phone is the
   reading order on the desktop. */
static void sections_left(RC_App *app, AppState *st, bool narrow, bool stack) {
    section_rectangles(stack);
    section_rounding(stack);
    section_gradients(stack);
    section_shadows(stack);
    section_overlay(stack);
    section_floating(app, st);
    section_images(app, st, stack);
    section_borders();
    section_text();
    section_widgets(app, st, stack);
    section_controls(app, st, narrow);
    section_charts(app, st, stack);
    section_table(app, st);
    section_bigtable(app);
    section_splitpane(st);
}

static void sections_right(RC_App *app, AppState *st, bool stack) {
    section_icons();
    section_gestures(app, st);
    section_keyboard(app, st);
    section_clipboard(app, st);
    section_live_icons(app, st);
    section_scroll(app, stack);
    section_zoom(app, st);
    section_arena(app, st);
    section_display(app, st);
}

/* THE JUMP-TO BAR - where am I, what else is there, and how do I reach it. It
   is ordinary app code over four public calls: rcGetElementBox, rcScrollBy,
   rcIsHovered and rcClicked. */

/* How far above the heading a jump lands - the section card's own top padding,
   so the card's edge comes to rest under the bar. */
enum { JUMP_LEAD = 16 };

/* The wrapped chip list, sized from the CHIP so the two cannot drift: four whole
   rows and a deliberate half, which is how a list says "there is more below". A
   remainder of a few pixels would read as a rendering artifact instead. */
enum { JUMP_CHIP_H = 24, JUMP_CHIP_GAP = 6,
       JUMP_LIST_H = 4 * JUMP_CHIP_H + 4 * JUMP_CHIP_GAP + JUMP_CHIP_H / 2 };

/* Which scroll column a section landed in. An anchor's x answers it directly, so
   there is no second table to keep in step. The arm matters because a ColRight
   box left from before the window narrowed must not win the comparison. */
static const char *section_column(RC_Box anchor, bool narrow) {
    if (narrow) return "ColLeft";
    RC_Box right = rcGetElementBox("ColRight");
    if (right.found && right.width > 0.0f && anchor.x >= right.x) return "ColRight";
    return "ColLeft";
}

/* Scroll the column holding `anchor` until the anchor sits just under its top:
   two boxes and a subtraction, because rcGetElementBox reports both in the same
   space. GUARD ON THE EXTENT, not on .found - a just-declared element answers
   found with an all-zero rect, and (0 - 0) would scroll the column to its top.
   NOT .scrollOffset, which is for the frame a container COMES BACK and lands a
   frame late; these columns are declared every frame. */
static void jump_to(RC_App *app, const char *anchor, bool narrow) {
    RC_Box a = rcGetElementBox(anchor);
    if (!a.found || a.height <= 0.0f) return;

    const char *col = section_column(a, narrow);
    RC_Box      c   = rcGetElementBox(col);
    if (!c.found || c.height <= 0.0f) return;

    rcScrollBy(col, 0.0f, a.y - c.y - (float)JUMP_LEAD);
    rcWindowRequestFrame(rcAppMainWindow(app));
}

/* The specimen a reader is on: the last anchor in `col` whose top has reached
   the column's top edge. Every box here is the LAST laid-out frame's, which is
   what a "where am I" readout wants. THE BAND HAS TO BE THE JUMP'S OWN LEAD, or
   the readout contradicts the click that caused it. */
static int current_section(const char *col) {
    RC_Box c     = rcGetElementBox(col);
    int    first = -1, last = -1;

    if (c.found && c.height > 0.0f) {
        float band = c.y + (float)JUMP_LEAD + 6.0f;

        for (int i = 0; i < (int)SECTION_COUNT; i++) {
            RC_Box a = rcGetElementBox(g_sections[i].anchor);
            if (!a.found || a.height <= 0.0f) continue;
            if (a.x < c.x || a.x >= c.x + c.width) continue;   /* the other column */
            if (first < 0)    first = i;
            if (a.y <= band)  last  = i;
        }
    }
    return last >= 0 ? last : (first >= 0 ? first : 0);
}

/* One navigator chip. THE CURSOR IS ALREADY RIGHT HERE: polling rcClicked IS the
   hint, because a polled element gets the clickable hand for free. The scrub
   field and the chart brush name theirs only because they want GRAB. */
static bool nav_chip(const char *id, const char *label, RC_Color tint, bool current) {
    RC_Style s   = rcGetStyle();
    bool     hot = rcIsHovered(id);

    rcRow(.id = id, .bg = (current || hot) ? s.surfaceAlt : s.surface, .gap = 6,
          .px = 10, .py = 5, .align = "cc", .borderRadius = "all-full",
          .border = { .color = current ? tint : s.border, .width = "1px" }) {
        rcBox(.bg = tint, .borderRadius = "all-full", .w = "6px", .h = "6px") {}
        rcTextC(label, .font = F_SMALL, .color = current ? s.text : s.textMuted,
                .wrap = "n");
    }
    return rcClicked(id);
}

/* The specimen chips, in reading order, into whichever container the arm chose
   for them. ONE function rather than a copy per arm, so the two cannot drift
   into offering a reader different specimens at different window widths. */
static void jump_chips(RC_App *app, AppState *st, int chipCat, int cur, bool narrow) {
    bool any = false;

    for (int i = 0; i < (int)SECTION_COUNT; i++) {
        const GallerySection *e = &g_sections[i];
        if (!section_matches(e, chipCat, st->search)) continue;
        any = true;
        if (nav_chip(e->chip, e->title, cat_tint(e->cat), i == cur))
            jump_to(app, e->anchor, narrow);
    }
    if (!any)
        rcTextL("No specimen matches that filter.", .font = F_SMALL,
                .color = rcGetStyle().textMuted);
}

static void jump_bar(RC_App *app, AppState *st, bool narrow, bool stack) {
    RC_Style s = rcGetStyle();
    /* One id per group. An element id is hashed from the string it is given and
       has to outlive the frame, so these are literals sitting beside the names
       they mark rather than anything built per frame. */
    static const char *const catChip[CAT_COUNT] = {
        "jmpcat_prim", "jmpcat_ctrl", "jmpcat_data", "jmpcat_input", "jmpcat_rt"
    };
    static const char *const catLineId[2] = { "JumpGroups", "JumpGroups2" };

    /* The readout follows the column the pointer is over, and REMEMBERS it, so
       walking up to this bar to click a chip does not snap it back to the left.
       One column on the narrow arm, so there is nothing to remember. */
    if (narrow)                       st->jumpRight = false;
    else if (rcIsHovered("ColRight")) st->jumpRight = true;
    else if (rcIsHovered("ColLeft"))  st->jumpRight = false;

    const char           *col = st->jumpRight ? "ColRight" : "ColLeft";
    int                   cur = current_section(col);
    const GallerySection *g   = &g_sections[cur];

    /* WHICH CHIPS TO SHOW. A filter searches every group, because a reader who
       types "zoom" is not thinking in groups. With no filter the row is ONE
       group's worth - ten chips at most - and that group is the one being read
       unless a click pinned another. */
    bool filtering = st->search[0] != '\0';
    int  activeCat = st->jumpCat >= 0 ? st->jumpCat : (int)g->cat;
    int  chipCat   = filtering ? -1 : activeCat;

    rcColumn(.id = "JumpBar", .bg = s.chrome, .gap = 8, .px = 12, .py = 10,
             .w = "grow") {
        /* Search on the left, position on the right, stacked once there is no
           room for both - the same WRAP_DIR split every fixed-width row uses. */
        rcBox(.gap = 10, .align = "cl", .w = "grow", .className = WRAP_DIR(stack)) {
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                rcTextL("Jump to", .font = F_SMALL, .color = s.textMuted);
                rcBox(.w = stack ? "grow" : "240px") {
                    rcTextInput("jump_search", st->search, sizeof st->search,
                                .placeholder = "Filter specimens or a group",
                                .font = F_SMALL);
                }
                if (filtering && nav_chip("jump_clear", "Clear", s.textMuted, false))
                    st->search[0] = '\0';
            }
            /* WHERE YOU ARE: the group in its tint, the specimen's name, and the
               same fraction the rail at the bottom of the bar draws. */
            rcRow(.gap = 8, .align = "cc") {
                rcBox(.bg = cat_tint(g->cat), .borderRadius = "all-full",
                      .w = "8px", .h = "8px") {}
                rcTextC(cat_name(g->cat), .font = F_SMALL, .color = s.textMuted,
                        .wrap = "n");
                rcTextC(g->title, .font = F_SMALL, .color = s.text, .wrap = "n");
                rcText(rcFormat(rcAppArena(app), "%d of %d", cur + 1,
                                (int)SECTION_COUNT),
                       .font = F_SMALL, .color = s.textMuted);
            }
        }

        /* The five groups. Clicking one PINS the list to it; clicking the pinned
           one again releases it back to following whatever is being read. Three
           to a line on a phone, where five clear a 420px window and the last is
           off the edge of a 360px one - a group whose chip cannot be seen is a
           group that cannot be pinned. */
        rcColumn(.gap = 6, .w = "grow") {
            int perLine = stack ? 3 : (int)CAT_COUNT;
            for (int line = 0; line * perLine < (int)CAT_COUNT; line++) {
                rcRow(.id = catLineId[line], .gap = 6, .align = "cl", .w = "grow") {
                    for (int i = line * perLine;
                         i < (line + 1) * perLine && i < (int)CAT_COUNT; i++) {
                        if (nav_chip(catChip[i], cat_name((GalleryCat)i),
                                     cat_tint((GalleryCat)i),
                                     !filtering && activeCat == i))
                            st->jumpCat = (st->jumpCat == i) ? -1 : i;
                    }
                }
            }
        }

        /* The specimens, and the one place the two arms differ in KIND rather
           than in size. With width, one strip. Without, the chips WRAP inside a
           height-capped scroller - a strip there would run off the edge on the
           axis rcScrollbar does not serve and the wheel does not move.
           flex-wrap is a CLASS, and it needs a constrained main axis: a FIT row
           resolves to the sum of its children on one line and never wraps. */
        if (narrow) {
            rcColumn(.id = "JumpChips", .align = "tl", .scroll = "v", .w = "grow",
                     .hMax = (float)JUMP_LIST_H) {
                rcRow(.gap = JUMP_CHIP_GAP, .align = "cl", .w = "grow",
                      .className = "flex-wrap") {
                    jump_chips(app, st, chipCat, cur, narrow);
                }
            }
        } else {
            rcRow(.id = "JumpChips", .gap = JUMP_CHIP_GAP, .align = "cl",
                  .scroll = "h", .w = "grow") {
                jump_chips(app, st, chipCat, cur, narrow);
            }
        }

        /* PROGRESS, as the fraction the readout just spelled out. Deliberately
           the specimen count and not the scroll offset: the two columns scroll
           independently, so an offset-driven rail would jump whenever the
           pointer crossed between them. */
        rcBox(.bg = s.surfaceAlt, .borderRadius = "all-full", .w = "grow",
              .h = "4px") {
            rcBox(.bg = cat_tint(g->cat), .borderRadius = "all-full", .h = "grow",
                  .wType = RC_PCT((float)(cur + 1) * 100.0f / (float)SECTION_COUNT)) {}
        }
    }
}

static void layout(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    rcSetStyle(st->darkMode ? rcStyleDark() : rcStyleLight());
    RC_Style  s     = rcGetStyle();
    /* A runtime theme switch has to move the window too: rcSetStyle changes every
       colour the UI draws with, but the window's own clear colour is resolved
       once at creation. Without this line the old theme's background stays
       wherever the layout does not cover the window. Safe every frame - the
       setter is change-gated. */
    rcWindowSetClearColor(rcAppMainWindow(app), s.background);

    /* Sizing is the CSS-like string DSL (.w = "grow" / "380px" / "50%"); the
       typed .wType = RC_GROW / RC_PX(..) form is the equivalent fast path, and
       the two resolve 1:1.

       SAFE AREA. A phone draws the window edge to edge, UNDER the status bar and
       the home indicator, and nothing moves content out of the way for you. Ask
       for the margins and spend them ONCE, here at the root. rcViewport().safe
       hands them over ALREADY IN LAYOUT UNITS - never divide
       rcGetSafeAreaInsets() by the zoom factor instead, which is right under
       RC_ZOOM_LAYOUT and wrong under RC_ZOOM_OPTICAL. */
    RC_Viewport vp   = rcViewport();
    RC_Insets   safe = vp.safe;
    /* Two facts about the WINDOW, never about the OS: whether the two columns
       fit side by side, and whether one column is too narrow for a swatch row. */
    float avail = vp.width - safe.left - safe.right;
    bool  narrow = avail < GALLERY_TWO_COLUMN_W;
    bool  stack  = avail < GALLERY_STACK_W;

    rcColumn(.id = "Root", .bg = s.background, .pt = (uint16_t)(safe.top),
             .pb = (uint16_t)(safe.bottom), .pl = (uint16_t)(safe.left),
             .pr = (uint16_t)(safe.right), .w = "grow", .h = "grow") {

        /* Info strip. The BUNDLED titlebar above it - drawn by the runner under
           nativeFrame, zero app code - owns window drag and the min/max/close
           controls; this row is plain content. */
        rcRow(.bg = s.chrome, .gap = 14, .px = 12, .align = "cl", .w = "grow",
              .h = "44px") {
            rcIconSettings(24, s.primary);
            /* The title grows, so it is the first thing to clip as the window
               narrows and the live readout keeps priority. On a phone the
               readout leaves ~150px beside the icon, so the title drops to the
               name alone rather than a subtitle cut mid-word - two literals,
               never a buffer: rcTextC borrows the pointer for the frame.
               THE PREDICATE IS `narrow`, NOT `stack`, AND THAT IS LOAD-BEARING. A
               phone is narrow in BOTH orientations - 852 and 393 are both under
               1020 - but it stacks only in portrait. Key this to `stack` and the
               title changes when the handset is turned: present in landscape, gone
               in portrait, which is content lost on rotation and what
               mobile_census fails on. */
            rcBox(.align = "cl", .overflow = "hidden", .w = "grow", .h = "grow") {
                rcTextC(narrow ? "RayClay" : "RayClay - native renderer gallery",
                    .font = F_TITLE, .color = s.text, .wrap = "n");
            }
            /* THE LABEL FOLLOWS THE RENDER MODE. rcAppFPS smooths the WALL GAP
               between frames, so on demand it reports how often something asked
               for a frame, not how fast one could be drawn. */
            RC_String readout = rcFormat(rcAppArena(app), "%.0f %s  -  frame %ld",
                                            rcAppFPS(app),
                                            st->continuous ? "FPS" : "wakes/s",
                                            st->frame);
            rcText(readout, .font = F_SMALL, .color = s.textMuted);
        }

        /* The navigator, declared BEFORE the columns it indexes: a chip's jump
           writes a column's scroll position, and a container reads that position
           as it opens, so the jump lands on THIS frame rather than the next. */
        jump_bar(app, st, narrow, stack);

        /* Two independently scrolling columns - or one carrying all of them. The
           page margin drops to 12 on the narrow arm, where 20 plus 16 of card
           padding each side would leave the widest rows short of room. ColLeft
           keeps its id in both arms, so its scroll offset survives a rotation
           and the scrollbar below binds to it either way. */
        rcRow(.id = "Content", .gap = 16, .p = (uint16_t)(narrow ? 12 : 20), .w = "grow",
              .h = "grow") {
            rcColumn(.id = "ColLeft", .gap = 16, .pr = GALLERY_SCROLL_GUTTER,
                     .scroll = "v", .w = "grow", .h = "grow") {
                sections_left(app, st, narrow, stack);
                if (narrow)
                    sections_right(app, st, stack);
            }
            if (!narrow) {
                /* A fixed width is safe HERE and only here: a fixed child is
                   never compressed, which is exactly how a fixed column falls
                   off a phone - but this arm runs only at 1020 or wider. */
                rcColumn(.id = "ColRight", .gap = 16, .pr = GALLERY_SCROLL_GUTTER,
                         .scroll = "v", .w = "394px", .h = "grow") {
                    sections_right(app, st, stack);
                }
            }
        }
    }

    /* Zoom badge: while the timer runs, float a "125%" pill top-centre over
       everything, out of flow and root-anchored. It names the RESET binding -
       Ctrl 0, RC_ZoomOptions.bindZoomReset - which is the one part of zoom that
       is otherwise invisible, and it is suppressed at exactly 100%.
       .zIndex is an int16_t and ties break on declaration order, so INT16_MAX is
       the one value that means "above every scope", a modal's scrim included. */
    if (st->zoomBadgeSecs > 0.0f) {
        float zf = rcWindowZoom(rcAppMainWindow(app));
        RC_String zl = rcFormat(rcAppArena(app), "%.0f%%", zf * 100.0f);
        rcRow(.id = "zoom_badge", .bg = s.surfaceAlt, .gap = 10, .px = 14, .py = 8,
              .align = "cc", .borderRadius = "all-full",
              .border = { .color = s.border, .width = "1px" },
              .floating = { .to      = RC_ATTACH_ROOT,
                             .parent  = RC_ANCHOR_TOP_CENTER,
                             .element = RC_ANCHOR_TOP_CENTER,
                             .offset  = { 0, 56 },
                             .zIndex = INT16_MAX }) {
            rcText(zl, .font = F_BODY, .color = s.text);
            if (zf < 0.999f || zf > 1.001f)
                rcTextL("Ctrl 0 resets", .font = F_SMALL, .color = s.textMuted);
        }
    }

    /* Draggable scrollbars for the scroll containers, declared here so each one
       layers above the container it names; they auto-hide when content fits.

       DO NOT guard on the pointer - the library asks that question itself, and
       on a finger it draws a thin indicator that cannot swallow a tap, though a
       half-second press on it does grab it. There is no `if (!rcPointerIsCoarse())`
       to write.

       DO guard on the CONTAINER: name one this frame did not build and the call
       is dropped with a warning. The two below carry their arm's condition; the
       rest are unconditional because those containers always exist. */
    rcScrollbar("ColLeft");
    if (narrow)  rcScrollbar("JumpChips");
    if (!narrow) rcScrollbar("ColRight");
    rcScrollbar("ScrollArea");
    rcScrollbar("gal_table");
}

int main(void) {
    AppState st = {
        .frame = 0, .darkMode = true,
        /* SEEDED: an empty text area demonstrates nothing, so there is text on
           the first frame to drag across. ASCII only - editing is byte-indexed,
           so a backspace over multibyte UTF-8 is memory-safe but can split a
           character. The name PLACEHOLDER below is accented on purpose: a
           placeholder is drawn, never edited. */
        .draft = "Drag across this line to select it. Double-click picks a word.\n"
                 "Ctrl+A selects all, Ctrl+C copies, Ctrl+V pastes.\n"
                 "\n"
                 "The editor works over YOUR buffer - this text lives in a plain\n"
                 "char array in the app, not in a widget object.",
        .volume = 0.5f, .quality = 1,
        .tableRow = -1,      /* the table opens with no row picked            */
        /* The correct pairing for a non-modal panel; unchecking it is the trap. */
        .inspectorSticky = true,
        .splitFrac = 0.5f,   /* the split pane starts centred                 */
        .zoomLo = 0.0f, .zoomHi = (float)(ZOOM_POINTS - 1),
        /* The hover affordances and CORNER placement are all off by default, so
           no existing chart changes under an app; a gallery has the opposite
           duty, and the checkboxes above turn each one back off.
           DESIGNATOR ORDER IS THE STRUCT'S: g++ hard-errors on an initialiser
           that runs out of order. */
        .tipPlace = (int)RC_TOOLTIP_PLACE_CORNER,
        .hoverGuide = true, .hoverMarkers = true,
        .scrub = 50.0f,      /* drag-scrub demo starts mid-range       */
        /* A full hue revolution every ~7s, the wave spanning ~half the wheel
           across the nine tiles. rng must start non-zero - xorshift32 fixes 0. */
        .hue = 0.55f, .hueSpeed = 0.15f, .hueSpread = 0.06f,
        /* Start FROZEN, so the gallery demonstrates the on-demand win rather than
           contradicting it: a live hue requests a frame every tick. The icons
           are still fully coloured, they just do not cycle, and the toggle is
           one click away. */
        .hueFrozen = true,
        .rng = 0x2545F491u,
        /* -1, not 0, because 0 is a real group: a zero-initialised field must
           not silently mean "filtered". */
        .jumpCat = -1,
    };

    static const float fontSizes[F_COUNT] = {
        [F_SMALL] = (float)SZ_SMALL,
        [F_BODY]  = 18.0f,
        [F_TITLE] = 30.0f,
        [F_BIG]   = 44.0f,
    };

    rcSetStyle(rcStyleDark());

    RC_AppOptions opts = {
        .width             = 1100,
        .height            = 720,
        .title             = "RayClay Gallery",
        .clearColor        = rcGetStyle().background,
        /* No fontPath: the bundled face is baked at each of fontSizes (zero-asset). */
        .fontSizes         = fontSizes,
        .fontCount         = F_COUNT,
        .scratchArenaBytes = 64 * 1024,
        .nativeFrame       = true,   /* borderless + the BUNDLED titlebar (runner-drawn) */
        .updateCallback          = update,
        .layoutCallback          = layout,
        .userData          = &st,
        /* OPT IN to drag-to-pan: it is OFF by default because out of the box a
           RayClay app zooms like a browser, and a browser has no pan. Only
           optical zoom can pan - layout zoom reflows into the window. */
        .zoom              = { .pan = true },
    };

    /* Our own arena for section_arena, separate from .scratchArenaBytes above,
       which belongs to the runner and is reset every frame. 4 KiB holds LOG_MAX
       entries and is small enough to drive to "full" by hand. */
    st.logArena = rcArenaInit(4 * 1024);
    arena_log_clear(&st);   /* carves the entry array out of the fresh arena */

    /* RAYCLAY_MAX_FRAMES=N renders N frames then exits through the normal
       teardown (unset or 0 runs until the window closes), so this binary doubles
       as a windowed-lifecycle leak target with no app-side code. */
    int rc = rcRunApp(&opts);

    /* We allocated it, so we free it: the runner owns only the arena it made
       from .scratchArenaBytes. */
    rcArenaFree(&st.logArena);
    return rc;
}
