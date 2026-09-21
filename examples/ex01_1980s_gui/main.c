/*
    main.c - RayClay "1980s desktop" example (early Macintosh calculator)

    A working four-function calculator in the strict 1-bit look of System 1
    (1984): black on white, square 1px chrome, a striped title bar, and keys
    that invert under the pointer. Same source -> desktop, web and phone.

    Shows: a custom monochrome RC_Style, a hand-drawn title bar wired to the
    runner through the RC_ID_WINDOW_* ids, menus, a baked font ladder, and a
    layout that sizes its keys from the space it measures rather than from a
    table of devices. Zero-asset - the bundled face is baked at startup.

    Build target: rayclay_ex01_1980s_gui
*/

#include "rayclay.h"

/* Font ladder baked from the bundled face at these sizes - zero-asset. */
typedef enum { F_BODY = 0, F_DISPLAY, F_DISPLAY_LG, F_COUNT } AppFont;

/* Calculator state (lives in .userData - zero per-frame heap allocation). */
typedef struct {
    double acc;    /* accumulated left-hand operand                       */
    double cur;    /* number currently being entered / shown              */
    char   op;     /* pending operator '+','-','*','/', or 0 for none     */
    bool   fresh;  /* true when the next digit should start a new number  */
} AppState;

static void calc_digit(AppState *st, int d) {
    if (st->fresh) {
        st->cur   = 0;
        st->fresh = false;
    }
    st->cur = st->cur * 10.0 + (double)d;
}

static double calc_eval(double acc, double cur, char op) {
    switch (op) {
    case '+': return acc + cur;
    case '-': return acc - cur;
    case '*': return acc * cur;
    case '/': return cur != 0.0 ? acc / cur : 0.0;
    default:  return cur;
    }
}

/* An operator key folds the pending op, then arms the next one; '=' is
   calc_op(st, 0). The fold runs only when an operand was actually entered
   (fresh == false), so repeating or changing an operator never applies cur twice. */
static void calc_op(AppState *st, char op) {
    if (!st->fresh)
        st->acc = st->op ? calc_eval(st->acc, st->cur, st->op) : st->cur;
    st->cur   = st->acc;
    st->op    = op;
    st->fresh = true;
}

static void calc_clear(AppState *st) {
    st->acc = st->cur = 0;
    st->op    = 0;
    st->fresh = true;
}

/* A raw box restyled into a 1-bit key: white, INVERTED to black under the
   pointer, which is how the era showed a press. It fills its row on both axes;
   KEY_ROW owns the height. A NULL label draws the minus BAR instead of a glyph -
   the bundled face is a Latin-1 subset, so the only minus it can set is a short
   hyphen, visibly lighter than the divide, multiply and plus keys beside it. */
static bool key(const char *id, const char *label) {
    bool hot = rcIsHovered(id);
    rcBox(.id = id, .bg = hot ? RC_BLACK : RC_WHITE, .align = "cc",
          .border = { .color = RC_BLACK, .width = "1px" }, .w = "grow", .h = "grow") {
        if (label)
            rcTextC(label, .font = F_BODY, .color = hot ? RC_WHITE : RC_BLACK);
        else
            rcBox(.bg = hot ? RC_WHITE : RC_BLACK, .w = "11px", .h = "2px") {}
    }
    return rcClicked(id);
}

/* One row of the 4x4 grid. The rows GROW into whatever height the face has left,
   so a key is capped at a SQUARE - the side that four keys and three gaps leave
   of the width - and floored at 44, the touch-target minimum every platform
   guideline agrees on. Min beats max, so a very narrow window gets 44. */
#define KEY_ROW(cap) rcRow(.gap = 8, .w = "grow", .h = "grow", .hMin = 44, .hMax = (cap))

/* The widest a key may get, so a large window gets a calculator-sized face
   rather than a keypad stretched across it. */
#define KEY_MAX 88.0f

/* The six 1px rules of a System 1 title bar; one run goes each side of the
   name, which sits in a clear white gap between them. */
static void title_stripes(void) {
    rcColumn(.gap = 2, .align = "cc", .w = "grow") {
        for (int i = 0; i < 6; i++) {
            rcBox(.bg = RC_BLACK, .w = "grow", .h = "1px") {}
        }
    }
}

/* A single black hairline: the seam between two bands of chrome. */
static void hairline(void) {
    rcBox(.bg = RC_BLACK, .w = "grow", .h = "1px") {}
}

static void layout(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;

    /* SAFE AREA. A phone draws the window edge to edge, UNDER the status bar and
       the home indicator, and nothing moves content out of the way for you.
       rcViewport().safe hands the margins over ALREADY IN LAYOUT UNITS - spend
       them once here at the root. Do NOT divide rcGetSafeAreaInsets() by the zoom
       yourself; that is wrong under RC_ZOOM_OPTICAL and is per-axis besides.
       Zero on desktop unless RAYCLAY_SAFE_INSETS stands a phone's bands in, so
       this is one code path everywhere. */
    RC_Insets safe = rcViewport().safe;

    /* A close box acts on an OS window, and web and mobile have none: there the
       RC_ID_WINDOW_* ids are inert, so a box drawn on those targets is dead
       chrome. rcChildWindowsSupported() is the public answer to "does this
       target have windows", so there is no #ifdef. */
    bool windowed = rcChildWindowsSupported();

    /* One 1px frame; every band inside it is parted by a single hairline, so no
       two rules ever abut. Top-CENTRE reaches the face alone - the bars span. */
    rcColumn(.id = "Root", .bg = RC_WHITE, .pt = (uint16_t)(safe.top),
             .pb = (uint16_t)(safe.bottom), .pl = (uint16_t)(safe.left),
             .pr = (uint16_t)(safe.right), .align = "tc",
             .border = { .color = RC_BLACK, .width = "1px" },
             .w = "grow", .h = "grow") {
        /* Title bar: striped, draggable, close box left, centred name. Its
           hairline is INSIDE the band, so the whole 22px is tagged and matches
           .titlebarHeight - the OS drag strip, frozen in physical px, which is
           also why rcUnzoomed() holds the band at a constant on-screen size. */
        rcUnzoomed() {
            rcColumn(.id = RC_ID_WINDOW_DRAG, .bg = RC_WHITE,
                     .w = "grow", .h = "22px") {
                rcRow(.gap = 6, .px = 6, .align = "cc", .w = "grow", .h = "grow") {
                    if (windowed) {
                        bool over = rcIsHovered(RC_ID_WINDOW_CLOSE);
                        /* Hand-drawn chrome polls hover, not rcClicked, so it
                           has to name the pointer cursor itself. */
                        if (over) rcSetCursor(RC_CURSOR_POINTER);
                        rcBox(.id = RC_ID_WINDOW_CLOSE, .bg = over ? RC_BLACK : RC_WHITE,
                              .border = { .color = RC_BLACK, .width = "1px" },
                              .w = "14px", .h = "14px") {}
                    }
                    title_stripes();
                    rcBox(.bg = RC_WHITE, .px = 6, .align = "cc") {
                        rcTextL("Calculator", .font = F_BODY, .color = RC_BLACK);
                    }
                    title_stripes();
                    /* Mirror spacer keeps the title optically centred. */
                    if (windowed) {
                        rcBox(.w = "14px") {}
                    }
                }
                hairline();
            }
        }

        /* Menu bar. Deliberately no .h: a fixed height does not shrink its
           children, they overflow it - omitting it means FIT. .py keeps a
           trigger's own edge off the hairlines above and below. */
        rcRow(.id = "MenuBar", .bg = RC_WHITE, .gap = 16, .px = 8, .py = 3,
              .align = "cl", .w = "grow") {
            if (rcBeginMenu("m_file", "File")) {
                if (rcMenuItem("Quit")) rcAppRequestClose(app);
                rcEndMenu();
            }
            if (rcBeginMenu("m_edit", "Edit")) {
                if (rcMenuItem("Clear")) calc_clear(st);
                rcEndMenu();
            }
        }
        hairline();

        /* The square key side: rcViewport().width - the width the face is laid
           out in, which no device table can tell you - less the safe bands and
           the face's 12px padding, shared by four keys and three 8px gaps. */
        RC_Viewport vp = rcViewport();
        float faceW    = vp.width - safe.left - safe.right - 2.0f * 12.0f;
        if (faceW > KEY_MAX * 4.0f + 8.0f * 3.0f)
            faceW = KEY_MAX * 4.0f + 8.0f * 3.0f;
        float keySide  = (faceW - 3.0f * 8.0f) / 4.0f;
        if (keySide < 44.0f) keySide = 44.0f;   /* the floor wins; see KEY_ROW */

        /* Room for a big number gets one; measured height, not a platform. */
        bool tall = vp.height >= 600.0f;

        /* BOTTOM-ALIGNED, because the keypad belongs under the thumb: every
           child is capped, so a tall screen's leftover lands above the display. */
        rcColumn(.id = "Face", .gap = 8, .p = 12, .align = "bc",
                 .w = "grow", .h = "grow", .wMax = faceW + 2.0f * 12.0f) {
            /* The display takes the slack: a tall screen makes the readout
               taller, never the keys. */
            rcBox(.id = "Display", .bg = RC_WHITE, .px = 10, .py = 8,
                  .align = "br", .border = { .color = RC_BLACK, .width = "1px" },
                  .w = "grow", .h = "grow", .hMin = 48, .hMax = keySide * 2.0f) {
                /* rcText BORROWS these bytes until the frame is drawn - the
                   frame arena outlives the frame, a local buffer would not. */
                RC_String v = rcFormat(rcAppArena(app), "%g", st->cur);
                /* Cast: a ternary over two enumerators is an int, and C++
                   refuses the narrowing into .font that C99 accepts. */
                uint16_t face = (uint16_t)(tall ? F_DISPLAY_LG : F_DISPLAY);
                rcText(v, .font = face, .color = RC_BLACK);
            }
            KEY_ROW(keySide) {
                if (key("k7", "7")) calc_digit(st, 7);
                if (key("k8", "8")) calc_digit(st, 8);
                if (key("k9", "9")) calc_digit(st, 9);
                if (key("kdiv", "\xc3\xb7")) calc_op(st, '/');
            }
            KEY_ROW(keySide) {
                if (key("k4", "4")) calc_digit(st, 4);
                if (key("k5", "5")) calc_digit(st, 5);
                if (key("k6", "6")) calc_digit(st, 6);
                if (key("kmul", "\xc3\x97")) calc_op(st, '*');
            }
            KEY_ROW(keySide) {
                if (key("k1", "1")) calc_digit(st, 1);
                if (key("k2", "2")) calc_digit(st, 2);
                if (key("k3", "3")) calc_digit(st, 3);
                if (key("ksub", NULL)) calc_op(st, '-');
            }
            KEY_ROW(keySide) {
                if (key("kclr", "C")) calc_clear(st);
                if (key("k0", "0")) calc_digit(st, 0);
                if (key("keq", "=")) calc_op(st, 0);
                if (key("kadd", "+")) calc_op(st, '+');
            }
        }
    }
}

int main(void) {
    AppState state = { .cur = 0 };

    static const float fontSizes[F_COUNT] = {
        [F_BODY]       = 18.0f,
        [F_DISPLAY]    = 30.0f,
        /* Slot 0 is what every library widget draws its own text at, so a new
           size goes on the END: reordering would resize the menus and title. */
        [F_DISPLAY_LG] = 56.0f,
    };

    /* Strict 1-bit monochrome, built from Light so the library widgets keep
       sane metrics and then flattened. .radius = 0 squares EVERY widget - pills
       and radio dots included - which is the whole period look in one field. */
    RC_Style mono   = rcStyleLight();
    mono.background = RC_WHITE;
    mono.surface    = RC_WHITE;
    mono.surfaceAlt = RC_WHITE;
    mono.chrome     = RC_WHITE;
    mono.text       = RC_BLACK;
    mono.textMuted  = RC_BLACK;
    mono.border     = RC_BLACK;
    mono.primary    = RC_BLACK;
    mono.radius     = 0.0f;
    rcSetStyle(mono);

    RC_AppOptions opts = {
        .width          = 320,
        .height         = 360,
        .title          = "Calculator",
        .fontSizes      = fontSizes,
        .fontCount      = F_COUNT,
        .scratchArenaBytes = 4096,   /* backs rcFormat (the display readout) */
        .nativeFrame    = true,
        .titlebarHeight = 22,
        .layoutCallback = layout,
        .userData       = &state,
        .titlebar       = { .custom = true },   /* we draw the System-1 bar ourselves */
    };

    return rcRunApp(&opts);
}
