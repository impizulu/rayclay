/*
    main.c - RayClay "1990s" example: a Windows 95 desktop

    A whole desktop in one C file: wallpaper, icons, a Start menu, a
    taskbar with a clock, and overlapping windows you drag and minimize. In them run a
    real Minesweeper and the Display Properties sheet, whose Apply repaints the desktop
    and every window, and whose screen savers fly.

    Shows: floating layers as movable windows (the active one at a higher .zIndex),
    3D controls built from nested boxes and rcClicked, popups that share one open-popup
    rule, a working copy behind OK / Cancel / Apply, and an animation that asks for
    frames only while it is on screen.

    Build target: rayclay_ex02_1990s_gui
*/

#include "rayclay.h"
#include "icons/rc_icons_caret_down.h"
#include "icons/rc_icons_flag_filled.h"

enum { W_MINE, W_DISP, WIN_COUNT };

typedef struct { int wallpaper, saver, scheme; } Settings;

typedef struct {
    bool     mine[81], shown[81], flag[81];
    int      state, flags, hit, secs;   /* state: 0 ready, 1 playing, 2 won, 3 lost */
    double   start;
    unsigned seed;
} Mines;

typedef struct { float x, y; bool open, min; } Win;

typedef struct {
    Settings    live, set;                  /* the sheet's working copy, and what is applied */
    Win         win[WIN_COUNT];
    /* saver: the screen saver running, 0 for none (see track_input) */
    int         top, tab, drag, saver;                /* top: the window declared last */
    const char *popup;                                /* the open popup's id, a literal */
    float       deskW, deskH, cell;                   /* cell: a Minesweeper square's side */
    bool        popupHot, off, flagging, phone;       /* hot: pointer on the popup or opener */
    double      lastInput;
    RC_Vec2     lastPointer, grab;                    /* grab: where the bar was taken */
    Mines       mines;
} AppState;

typedef struct { RC_Color face, light, shadow, cap, desk; } Palette;

/* One row per scheme, in Palette's order. rcHex is a call, so the table holds the hex. */
static const uint32_t SCHEMES[][5] = {
    { 0xc0c0c0, 0xdfdfdf, 0x808080, 0x000080, 0x008080 },   /* Windows Standard */
    { 0xd5ccbb, 0xeae6dd, 0xa28d68, 0x008080, 0xa28d68 },   /* Desert */
    { 0x90b0a0, 0xc8d8d0, 0x588078, 0x582868, 0x588078 },   /* Eggplant */
    { 0xcfafb7, 0xe7d7db, 0x9f6070, 0x800040, 0x9f6070 },   /* Rose */
};

/* Apply swaps a whole palette: each frame, layout installs the applied scheme's palette in
   `ui`, and every control below paints from it. */
static Palette app_palette(int scheme) {
    const uint32_t *c = SCHEMES[scheme];
    return RC_LIT(Palette){ rcHex(c[0]), rcHex(c[1]), rcHex(c[2]), rcHex(c[3]), rcHex(c[4]) };
}
static Palette ui;

/* A finger gets 44px to aim at, the minimum every platform guideline agrees on; a mouse
   sets no minimum, so a control is as tall as its content. */
#define COARSE    rcPointerIsCoarse()
#define TOUCH_MIN (COARSE ? RC_VAL(44) : RC_UNSET_VAL)
/* Minesweeper's window, title bar in, is its nine rows of cells and 92px more; 128 under a
   finger, whose title bar is 22px taller and whose 44px flag button makes the LCD row 14px
   taller. */
#define MINE_H(cell) (9.0f * (cell) + (COARSE ? 128.0f : 92.0f))
/* Its width is its nine columns of cells and 22px more, and never under 192px: the counter
   row needs that with a finger's flag button in it, where a short desk shrinks the cells. */
#define MINE_W(cell) (9.0f * (cell) + 22.0f < 192.0f ? 192.0f : 9.0f * (cell) + 22.0f)

/* A 1995 control depresses under the press instead of lighting up under the pointer, and
   pops up when the pointer is dragged off it. rcShowsActive is the pressed look and asks
   for no frame, where rcPointerDown would draw every frame the button is held. */
#define HELD(id) (rcShowsActive(id) && rcIsHovered(id))

/* A desktop's labels are not text to select: the select-none class on Root (and on the
   shutdown screen, declared outside it) covers every run inside, as CSS's user-select
   reaches a subtree. */
#define INK(s, c)  rcTextC(s, .color = (c))
#define BOLD(s, c) rcTextC(s, .font = rcFontSemibold(13.0f), .color = (c))

/* A 3D edge is two tones on each side, light top and left, dark bottom and right, so it
   is four nested boxes, each padded on two sides so only those two show its color, then
   the face. The caller's braces hold what stands on the face, centered. Each layer is
   min-w-0, as on the web, so a bevel the caller lets shrink (a task button) shrinks whole. */
#define BEVEL(key, tl1, br1, tl2, br2, face, ...)                                             \
    rcBox(.id = (key), .bg = (tl1), .pt = 1, .pl = 1, __VA_ARGS__)                            \
    rcBox(.bg = (br1), .pb = 1, .pr = 1, .w = "grow", .h = "grow", .className = "min-w-0")    \
    rcBox(.bg = (tl2), .pt = 1, .pl = 1, .w = "grow", .h = "grow", .className = "min-w-0")    \
    rcBox(.bg = (br2), .pb = 1, .pr = 1, .w = "grow", .h = "grow", .className = "min-w-0")    \
    rcBox(.bg = (face), .align = "cc", .w = "grow", .h = "grow", .className = "min-w-0")
#define BUTTON(key, down, ...)                                                  \
    BEVEL(key, (down) ? RC_BLACK : RC_WHITE, (down) ? RC_WHITE : RC_BLACK,      \
          (down) ? ui.shadow : ui.light, (down) ? ui.light : ui.shadow, ui.face, __VA_ARGS__)
/* A title bar's button, 16x14 as 1995's, and 36 square under a finger. */
#define CAPBTN(k) BUTTON(k, HELD(k), .w = COARSE ? "36px" : "16px", .h = COARSE ? "36px" : "14px")
#define RAISED(k, ...) BEVEL(k, RC_WHITE, RC_BLACK, ui.light, ui.shadow, ui.face, __VA_ARGS__)
#define SUNKEN(key, paper, ...) \
    BEVEL(key, ui.shadow, RC_WHITE, RC_BLACK, ui.light, (paper), __VA_ARGS__)

/* A float counts its z from the float it is declared in, so the art in a float lands one
   above it: the desk icons' at 1, the window behind at 2 with its art at 3, and the active
   window at 4, over all of that. Popups sit in a band above them, the icons a popup holds
   one higher, the screen above all. */
#define Z_POPUP 100

/* The Display Properties sheet below its title bar, monitor shown; without it, 190. It is
   sized for its tallest page, Screen Saver's, where the Preview button makes the combo's row
   taller, so OK, Cancel and Apply keep their place on every tab. */
#define SHEET_H 330.0f

/* The 1995 glyphs, pixel for pixel: one row of bits per pixel row, leftmost in bit 0. */
typedef uint16_t Bitmap[12];
static const Bitmap G_SMILE = { 0, 0, 0x198, 0x198, 0, 0, 0x204, 0x108, 0x0f0 };
static const Bitmap G_DEAD = { 0, 0x294, 0x108, 0x294, 0, 0, 0, 0x0f0, 0x108, 0x204 };
static const Bitmap G_COOL = { 0, 0, 0x7fe, 0x39c, 0x198, 0, 0x204, 0x108, 0x0f0 };
static const Bitmap G_CLOSE = { 0xc3, 0x66, 0x3c, 0x18, 0x3c, 0x66, 0xc3 };
static const Bitmap G_MIN = { 0, 0, 0, 0, 0, 0x3f, 0x3f };

/* A bitmap, one box per pixel, so its strokes are whole pixels. */
static void sprite(const uint16_t *rows, int w, int h, RC_Color c) {
    rcColumn() for (int y = 0; y < h; y++)
        rcRow() for (int x = 0; x < w; x++)
            rcBox(.bg = (rows[y] >> x) & 1 ? c : RC_UNSET_COLOR, .w = "1px", .h = "1px") {}
}

/* One float: the brush of the icons and the screen savers. It passes the pointer on,
   so an icon's button still answers under its pixels. */
static void dot(float x, float y, float w, float h, RC_Color c, int z) {
    rcBox(.bg = c, .wType = RC_PX(w), .hType = RC_PX(h),
          .floating = { .to = RC_ATTACH_PARENT, .offset = { x, y }, .zIndex = (int16_t)z,
                        .capture = RC_CAPTURE_PASSTHROUGH }) {}
}

/* The icons: rectangles on a 32x32 grid, ended by a zero entry. Every edge is even, so
   the 16px title-bar and taskbar copies land on whole pixels too. */
typedef struct { uint8_t x, y, w, h; uint32_t c; } Px;
static const Px ART_PC[] = {
    { 2, 2, 28, 20, 0 }, { 4, 4, 24, 16, 0xc0c0c0 }, { 6, 6, 20, 12, 0x008080 },
    { 12, 22, 8, 2, 0x808080 }, { 2, 24, 28, 6, 0 }, { 4, 26, 24, 2, 0xc0c0c0 }, { 0 } };
static const Px ART_MINE[] = { { 14, 2, 4, 28, 0 }, { 2, 14, 28, 4, 0 }, { 8, 8, 16, 16, 0 },
                               { 10, 10, 4, 4, 0xffffff }, { 0 } };
static const Px ART_FLAG[] = { { 2, 4, 12, 12, 0xff0000 }, { 18, 4, 12, 12, 0x00c000 },
                               { 2, 18, 12, 12, 0x0000ff }, { 18, 18, 12, 12, 0xffe000 }, { 0 } };

static void art(const Px *p, float x, float y, float size, int z) {
    float k = size / 32.0f;
    for (; p->w; p++) dot(x + p->x * k, y + p->y * k, p->w * k, p->h * k, rcHex(p->c), z);
}

static void icon(const Px *p, float size, int z) {
    rcBox(.wType = RC_PX(size), .hType = RC_PX(size)) art(p, 0.0f, 0.0f, size, z);
}

/* The window on top is the one declared last. */
static void raise_window(AppState *st, int n) {
    st->top    = n;
    st->win[n] = RC_LIT(Win){ st->win[n].x, st->win[n].y, true, false };
}

/* A disabled button is not polled, so it keeps the arrow cursor. `dflt` draws the black
   ring of the default action, transparent on the others so all keep one geometry. */
static bool push_button(const char *id, const char *label, bool dflt, bool disabled) {
    rcBox(.p = 1, .className = dflt ? "border border-black" : "border border-transparent")
    BUTTON(id, !disabled && HELD(id), .wMin = RC_VAL(75), .hMin = TOUCH_MIN)
    rcBox(.px = 12, .py = 3) INK(label, disabled ? ui.shadow : RC_BLACK);
    return !disabled && rcClicked(id);
}

/* The selected tab stands two pixels taller with no bottom edge, so it and the page are
   one surface; the others sit on the white rule that is the page's top edge. */
static void tab_button(const char *id, const char *label, int *tab, int index) {
    bool on = *tab == index;
    rcColumn(.id = id, .pt = (float)(on ? 0 : 2)) {
        rcBox(.bg = RC_WHITE, .pt = 1, .pl = 1, .w = "grow")
        rcBox(.bg = RC_BLACK, .pr = 1, .w = "grow", .h = "grow")
        rcBox(.bg = ui.face, .pt = (float)(on ? 6 : 4), .pb = 4, .px = 10, .align = "cc",
              .w = "grow", .hMin = TOUCH_MIN) INK(label, RC_BLACK);
        rcBox(.bg = on ? ui.face : RC_WHITE, .w = "grow", .h = "1px") {}
    }
    if (rcClicked(id)) *tab = index;
}

/* One row of any popup: the scheme's caption color while it shows hover, or while `marked`. */
static bool menu_row(AppState *st, const char *id, const char *label, const Px *p, bool marked) {
    bool lit = rcShowsHover(id) || marked;
    st->popupHot |= rcIsHovered(id);
    rcRow(.id = id, .bg = lit ? ui.cap : RC_UNSET_COLOR, .gap = 8, .px = 8, .py = 3, .align = "cl",
          .w = "grow", .wMin = RC_VAL(120), .hMin = TOUCH_MIN) {
        if (p) icon(p, 32.0f, Z_POPUP + 1);
        INK(label, lit ? RC_WHITE : RC_BLACK);
    }
    bool hit = rcClicked(id);
    if (hit) st->popup = NULL;
    return hit;
}

/* A sunken field and a floating list of menu rows: the open row is the caption color with
   white text, as 1995 drew it. `label` may be NULL where the tab already names the field. */
static void combo(RC_App *app, AppState *st, const char *id, const char *label, int *sel,
                  const char *const *items, int n) {
    const char *list = rcFormat(rcAppArena(app), "%s_list", id).chars;
    st->popupHot |= rcIsHovered(id) || rcIsHovered(list);
    if (rcClicked(id)) st->popup = st->popup == id ? NULL : id;
    rcRow(.gap = 10, .align = "cl", .w = "grow") {
        if (label) INK(label, RC_BLACK);
        SUNKEN(id, RC_WHITE, .w = "grow", .hMin = TOUCH_MIN)
        rcRow(.pl = 4, .align = "cl", .w = "grow", .h = "grow") {
            rcBox(.w = "grow") INK(items[*sel], RC_BLACK);
            BUTTON(NULL, st->popup == id, .w = "17px", .h = "grow", .hMin = RC_VAL(18))
                rcIconCaretDown(12.0f, RC_BLACK);
        }
    }
    if (st->popup != id) return;
    rcColumn(.id = list, .bg = RC_WHITE, .w = "grow",
             .floating = { .to = RC_ATTACH_ELEMENT, .toId = id, .parent = RC_ANCHOR_BOTTOM_LEFT,
                           .element = RC_ANCHOR_TOP_LEFT, .zIndex = Z_POPUP,
                           .clip = RC_CLIP_NONE }, .className = "border border-black") {
        for (int i = 0; i < n; i++)
            if (menu_row(st, rcFormat(rcAppArena(app), "%s_%d", id, i).chars, items[i], NULL,
                         i == *sel && !rcShowsHover(list))) *sel = i;
    }
}

static float frac(double v) { return (float)(v - (double)(long)v); }

/* An integer hash, so a star's phase, x and y (one salt each) are unrelated numbers. */
static float rnd(unsigned i, unsigned salt) {
    unsigned h = (i * 2654435761u) ^ salt, k = (h ^ (h >> 15)) * 0x2c1b3c6du;
    return (float)((k ^ (k >> 12)) / 4294967296.0);
}

/* One scene for the monitor's glass and the whole screen: Flying Windows and Starfield
   are one flight, out from the center and nearer as it goes. */
static void saver_scene(int saver, float w, float h, int z) {
    double t = rcMonotonicSeconds();
    dot(0.0f, 0.0f, w, h, RC_BLACK, z);
    for (unsigned i = 0, n = saver == 1 ? 14 : 110; i < n; i++) {
        float u = frac(t * 0.2 + rnd(i, 0x51u));
        float x = w * 0.5f + (rnd(i, 0x9e37u) * 2.0f - 1.0f) * u * u * w * 0.9f;
        float y = h * 0.5f + (rnd(i, 0x7f4au) * 2.0f - 1.0f) * u * u * h * 0.9f;
        if (saver == 1) art(ART_FLAG, x, y, 4.0f + u * u * w * 0.14f, z);
        else dot(x, y, 1.0f + u * u * w * 0.004f, 1.0f + u * u * w * 0.004f, RC_WHITE, z);
    }
}

/* One painter for the desktop and for the desktop in the monitor's glass. */
static void wallpaper(const Settings *s, float w, float h) {
    static const uint32_t SKY[][2] = { { 0, 0 }, { 0x2868c8, 0xa8d0f0 }, { 0x301860, 0xf09048 },
                                       { 0x002850, 0x00a0a0 }, { 0x104020, 0x70a040 } };
    if (s->wallpaper) rcBox(.wType = RC_PX(w), .hType = RC_PX(h),
              .gradient = { rcHex(SKY[s->wallpaper][0]), rcHex(SKY[s->wallpaper][1]), "v" },
              .floating = { .to = RC_ATTACH_PARENT, .capture = RC_CAPTURE_PASSTHROUGH }) {}
}

/* What is on the glass depends on the tab, so no control on this sheet is decoration. `s`
   is the sheet's working copy: the glass shows the picks, not yet what is applied. The
   Appearance tab shows the picked scheme's desktop and a window painted in it, as 1995's
   preview did, so a scheme shows before Apply; the wallpaper would hide its desktop. */
static void monitor(const Settings *s, int tab) {
    const Palette sc = app_palette(s->scheme);
    rcColumn(.align = "tc", .w = "grow") {
        RAISED(NULL, .w = "fit") rcBox(.p = 9)
        rcBox(.bg = sc.desk, .p = 16, .overflow = "hidden", .w = "176px", .h = "132px") {
            if (tab == 1 && s->saver) saver_scene(s->saver, 176.0f, 132.0f, 0);
            else if (tab != 2) wallpaper(s, 176.0f, 132.0f);
            else BEVEL(NULL, RC_WHITE, RC_BLACK, sc.light, sc.shadow, sc.face,
                       .w = "grow", .h = "grow")
                rcColumn(.p = 1, .w = "grow", .h = "grow")
                rcRow(.bg = sc.cap, .p = 3, .w = "grow")
                    BOLD("Active Window", RC_WHITE);
        }
        RAISED(NULL, .w = "28px", .h = "8px") {} RAISED(NULL, .w = "104px", .h = "9px") {}
    }
}

static void display_body(RC_App *app, AppState *st) {
    static const char *const WALLS[]  = { "(None)", "Sky", "Sunset", "Lagoon", "Forest" };
    static const char *const SAVERS[] = { "(None)", "Flying Windows", "Starfield" };
    static const char *const NAMES[]  = { "Windows Standard", "Desert", "Eggplant", "Rose" };
    Settings *s = &st->live;

    rcColumn(.gap = 8, .p = 8, .w = "grow", .h = "grow") {
        rcColumn(.w = "grow", .h = "grow") {
            rcRow(.align = "bl", .w = "grow") {
                tab_button("tab_bg", "Background", &st->tab, 0);
                tab_button("tab_ss", "Screen Saver", &st->tab, 1);
                tab_button("tab_ap", "Appearance", &st->tab, 2);
                rcColumn(.w = "grow", .h = "grow", .className = "border-b border-white") {}
            }
            /* The page: white left edge, black and gray right and bottom. */
            rcBox(.bg = RC_WHITE, .pl = 1, .w = "grow", .h = "grow")
            rcBox(.bg = RC_BLACK, .pb = 1, .pr = 1, .w = "grow", .h = "grow")
            rcBox(.bg = ui.shadow, .pb = 1, .pr = 1, .w = "grow", .h = "grow")
            rcColumn(.bg = ui.face, .gap = 10, .p = 14, .w = "grow", .h = "grow") {
                /* Withheld rather than squeezed on a short screen: the controls are the job. */
                if (st->deskH >= SHEET_H + 22.0f) monitor(s, st->tab);
                if (st->tab == 0) combo(app, st, "cb_wall", "Wallpaper:", &s->wallpaper, WALLS, 5);
                if (st->tab == 2) combo(app, st, "cb_scheme", "Scheme:", &s->scheme, NAMES, 4);
                if (st->tab == 1) {
                    /* 1995's row: the saver, then Preview; it starts after 5 idle minutes. */
                    rcRow(.gap = 10, .align = "cl", .w = "grow") {
                        combo(app, st, "cb_saver", NULL, &s->saver, SAVERS, 3);
                        if (push_button("ss_go", "Preview", false, !s->saver)) st->saver = s->saver;
                    }
                    /* On demand: the glass asks for frames only while it flies. */
                    if (s->saver && st->deskH >= SHEET_H + 22.0f)
                        rcWindowRequestFrame(rcAppMainWindow(app));
                }
            }
        }
        /* OK commits and closes, Cancel discards and closes, Apply commits and stays. */
        rcRow(.gap = 6, .align = "cr", .w = "grow") {
            /* Enter is the default button's key, while this sheet is on top, no list is open
               and no screen saver covers it. */
            bool ok = push_button("act_ok", "OK", true, false) ||
                      (st->top == W_DISP && !st->popup && !st->saver && rcKeyPressed(RC_KEY_ENTER));
            bool no = push_button("act_cancel", "Cancel", false, false);
            if (push_button("act_apply", "Apply", false, s->wallpaper == st->set.wallpaper &&
                            s->saver == st->set.saver && s->scheme == st->set.scheme) || ok)
                st->set = st->live;
            if (ok || no) st->win[W_DISP].open = false;
        }
    }
}

/* Counts the mines around a cell; `flood` opens the cells around it as well. */
static int mines_near(Mines *m, int c, bool flood) {
    int n = 0;
    for (int y = c / 9 - 1; y <= c / 9 + 1; y++) for (int x = c % 9 - 1; x <= c % 9 + 1; x++) {
        int at = y * 9 + x;
        if (x < 0 || x > 8 || y < 0 || y > 8) continue;
        n += m->mine[at];
        if (!flood || m->shown[at] || m->flag[at]) continue;
        m->shown[at] = true;
        if (!mines_near(m, at, false)) mines_near(m, at, true);
    }
    return n;
}

/* The mines are laid on the first click, around it, so the first click is never a loss.
   The clock stops where the game ends. */
static void mines_click(Mines *m, int c) {
    int shown = 0;
    if (m->state >= 2 || m->flag[c]) return;
    for (int laid = 0; m->state == 0 && laid < 10;) {
        m->seed = m->seed * 1103515245u + 12345u;
        int p   = (int)((m->seed >> 16) % 81);
        if (p != c && !m->mine[p]) { m->mine[p] = true; laid++; }
    }
    if (m->state == 0) m->start = rcMonotonicSeconds();
    m->state = m->mine[c] ? 3 : 1;
    m->hit   = c;
    m->secs  = (int)(rcMonotonicSeconds() - m->start);
    if (m->state == 3) return;
    m->shown[c] = true;
    if (!mines_near(m, c, false)) mines_near(m, c, true);
    for (int i = 0; i < 81; i++) shown += m->shown[i];
    if (shown == 71) m->state = 2;
}

/* The ink of each count, 1 to 8, as Windows 95 paints them; a 0 shows no digit. */
static const uint32_t INKS[] = { 0, 0xff, 0x8000, 0xff0000, 0x80, 0x800000, 0x8080, 0, 0x808080 };

/* `flagging` turns a tap into the right-click that plants a flag. */
static void mines_cell(RC_App *app, Mines *m, int c, float size, bool flagging) {
    const char *id = rcFormat(rcAppArena(app), "mc%d", c).chars;
    bool boom = m->state == 3 && m->mine[c] && !m->flag[c];

    if (m->shown[c] || boom) {
        int n = boom ? 0 : mines_near(m, c, false);
        rcBox(.bg = ui.shadow, .pt = 1, .pl = 1, .wType = RC_PX(size), .hType = RC_PX(size))
        rcBox(.bg = boom && c == m->hit ? rcHex(0xff0000) : ui.face, .align = "cc", .w = "grow",
              .h = "grow") {
            if (boom) icon(ART_MINE, 16.0f, 0);
            else if (n) rcText(rcFormat(rcAppArena(app), "%d", n), .font = rcFontSemibold(14.0f),
                               .color = rcHex(INKS[n]));
        }
        return;
    }
    /* A cell is raised by white and gray alone; held, it flattens like an open one. */
    bool down = m->state < 2 && !m->flag[c] && !flagging && HELD(id);
    BEVEL(id, down ? ui.shadow : RC_WHITE, down ? ui.face : ui.shadow, down ? ui.face : RC_WHITE,
          down ? ui.face : ui.shadow, ui.face, .wType = RC_PX(size), .hType = RC_PX(size))
        if (m->flag[c] || m->state == 2) rcIconFlagFilled(14.0f, rcHex(0xff0000));
    /* rcContextClicked is the right button's press or a finger's long press: a flag's gesture. */
    bool tap = rcClicked(id);
    if (tap && !flagging) mines_click(m, c);
    else if ((tap || rcContextClicked(id)) && m->state < 2) {
        m->flag[c] = !m->flag[c];
        m->flags  += m->flag[c] ? 1 : -1;
    }
}

/* Three seven-segment digits, 13x23 each, red on black; an unlit segment glows faintly.
   A segment's bit in SEG runs a (top) to g (middle); a minus is g alone. */
static void lcd(int value) {
    static const uint8_t SEG[10] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f };
    static const float   AT[7][4] = { { 4, 2, 9, 2 }, { 13, 4, 2, 9 }, { 13, 14, 2, 9 },
        { 4, 23, 9, 2 }, { 2, 14, 2, 9 }, { 2, 4, 2, 9 }, { 4, 12.5f, 9, 2 } };
    int     v = value < 0 ? -value % 100 : value > 999 ? 999 : value;
    int     mask[3] = { value < 0 ? 0x40 : SEG[v / 100 % 10], SEG[v / 10 % 10], SEG[v % 10] };
    rcBox(.bg = RC_BLACK, .w = "45px", .h = "27px") for (int k = 0; k < 21; k++)
        dot((float)(k / 7 * 14) + AT[k % 7][0], AT[k % 7][1], AT[k % 7][2], AT[k % 7][3],
            rcHex(mask[k / 7] >> (k % 7) & 1 ? 0xff0000 : 0x3c0000), 0);
}

/* `again` is F2 while this window is the active one: the smile's key. */
static void mines_body(RC_App *app, AppState *st, bool again) {
    Mines *m = &st->mines;

    if (m->state == 1) rcWindowRequestFrameAfter(rcAppMainWindow(app), 1.0);
    rcColumn(.gap = 6, .p = 6, .align = "tc", .w = "grow", .h = "grow") {
        SUNKEN(NULL, ui.face, .w = "grow") rcRow(.p = 4, .align = "cl", .w = "grow") {
            lcd(m->state == 2 ? 0 : 10 - m->flags);
            rcBox(.align = "cc", .w = "grow")
            BUTTON("smile", HELD("smile"), .w = "28px", .h = "28px")
            rcBox(.bg = rcHex(0xffe000), .align = "cc", .w = "20px", .h = "20px",
                  .className = "rounded-full border border-black")
                sprite(m->state == 3 ? G_DEAD : m->state == 2 ? G_COOL : G_SMILE, 12, 12, RC_BLACK);
            /* A finger's long press plants a flag; a flag mode turns its taps into flags too. */
            if (COARSE) BUTTON("flag_mode", st->flagging, .w = "44px", .h = "44px")
                rcIconFlagFilled(20.0f, rcHex(0xff0000));
            if (rcClicked("flag_mode")) st->flagging = !st->flagging;
            lcd(m->state == 1 ? (int)(rcMonotonicSeconds() - m->start) : m->secs);
        }
        SUNKEN(NULL, ui.face, .w = "fit")
        rcRow(.wType = RC_PX(9.0f * st->cell), .className = "flex-wrap")
            for (int c = 0; c < 81; c++) mines_cell(app, m, c, st->cell, st->flagging);
    }
    if (rcClicked("smile") || again) *m = RC_LIT(Mines){ .seed = m->seed };
}

/* A desktop icon: one click opens it, where the era asked for two. */
static void desk_icon(AppState *st, const char *id, const char *label, const Px *p, int opens) {
    rcColumn(.id = id, .gap = 4, .align = "tc", .w = "88px") {
        icon(p, 32.0f, 0);
        rcBox(.bg = HELD(id) ? ui.cap : RC_UNSET_COLOR, .px = 3, .py = 1) INK(label, RC_WHITE);
    }
    if (rcClicked(id)) raise_window(st, opens);
}

typedef struct { const char *id, *bar, *close, *title; const Px *art; } WinInfo;
static const WinInfo WINS[WIN_COUNT] = {
    { "w_mine", "w_mine_bar", "w_mine_x", "Minesweeper", ART_MINE },
    { "w_disp", "w_disp_bar", "w_disp_x", "Display Properties", ART_PC } };

static float clamp0(float v, float hi) { v = v > hi ? hi : v; return v > 0.0f ? v : 0.0f; }

static void window(RC_App *app, AppState *st, int n, bool active, RC_Insets safe) {
    const WinInfo *i     = &WINS[n];
    Win           *w     = &st->win[n];
    float          capH  = COARSE ? 44.0f : 22.0f;
    float          sheet = (st->deskH >= SHEET_H + 22.0f ? SHEET_H : 190.0f) + COARSE * 70.0f;
    float          ww    = clamp0(n == W_MINE ? MINE_W(st->cell) : 406.0f, st->deskW);
    float          wh    = clamp0(n == W_MINE ? MINE_H(st->cell) : sheet + capH, st->deskH);
    bool           press = rcPointerPressed(RC_POINTER_LEFT);
    /* Read only in the gesture: a frame that reads the pointer draws every move after it. */
    RC_Vec2        at    = press || st->drag == n ? rcPointer() : RC_LIT(RC_Vec2){ 0 };
    if (!w->open || w->min) return;

    /* The press on a title bar starts the drag; the window then follows the pointer, so
       a fast drag that leaves the bar still moves it. */
    if (press && rcIsHovered(i->id)) raise_window(st, n);
    if (press && rcIsHovered(i->bar)) {
        st->drag = n;
        st->grab = RC_LIT(RC_Vec2){ at.x - w->x, at.y - w->y };
    }
    /* Kept on the desktop at every window size, not just while it is dragged. */
    w->x = clamp0(st->drag == n ? at.x - st->grab.x : w->x, st->deskW - ww);
    w->y = clamp0(st->drag == n ? at.y - st->grab.y : w->y, st->deskH - wh);

    RAISED(i->id, .wType = RC_PX(ww), .hType = RC_PX(wh), .floating = { .to = RC_ATTACH_ROOT,
           .offset = { w->x + safe.left, w->y + safe.top }, .zIndex = (int16_t)(active ? 4 : 2) })
    rcColumn(.p = 1, .w = "grow", .h = "grow") {
        /* One flat color, as 1995 drew a title bar; the fade came three years later. */
        rcRow(.bg = active ? ui.cap : ui.shadow, .gap = 2, .pr = 2, .align = "cl", .w = "grow",
              .hType = RC_PX(capH)) {
            rcRow(.id = i->bar, .gap = 4, .pl = 3, .align = "cl", .w = "grow", .h = "grow") {
                icon(i->art, 16.0f, 0);
                /* White on a window behind too, where 1995 wrote in the face color: that is
                   too faint on the shadow band under Rose and Eggplant. The muted band is
                   what says the window is behind. */
                BOLD(i->title, RC_WHITE);
            }
            /* An application window minimizes; a property sheet, as in 1995, only closes. */
            if (n == W_MINE) CAPBTN("w_mine_min") sprite(G_MIN, 8, 7, RC_BLACK);
            CAPBTN(i->close) sprite(G_CLOSE, 8, 7, RC_BLACK);
            if (n == W_MINE && rcClicked("w_mine_min")) w->min = true;
            if (rcClicked(i->close)) w->open = false;
        }
        if (n == W_MINE) mines_body(app, st, active && rcKeyPressed(RC_KEY_F2));
        else display_body(app, st);
    }
}

static const char START[] = "start";

/* The Start menu: a band in the caption color beside 32px rows, which set its height; the band's
   letters stand at its foot. */
static void start_menu(AppState *st) {
    static const char *const BAND[] = { "R", "C", "9", "5" };
    RAISED(NULL, .w = "fit",
           .floating = { .to = RC_ATTACH_ELEMENT, .toId = START, .parent = RC_ANCHOR_TOP_LEFT,
                         .element = RC_ANCHOR_BOTTOM_LEFT, .zIndex = Z_POPUP,
                         .clip = RC_CLIP_NONE }) rcRow(.p = 1, .w = "grow") {
        rcColumn(.pb = 6, .align = "bc", .w = "24px", .h = "grow",
                 .gradient = { RC_BLACK, ui.cap, "v" })
            for (int i = 0; i < 4; i++) BOLD(BAND[i], RC_WHITE);
        rcColumn(.w = "fit") {
            if (menu_row(st, "sm_mine", "Minesweeper", ART_MINE, false)) raise_window(st, W_MINE);
            if (menu_row(st, "sm_disp", "Settings", ART_PC, false)) raise_window(st, W_DISP);
            /* An etched rule: shadow over a highlight. */
            rcBox(.bg = ui.shadow, .w = "grow", .h = "2px", .className = "border-b border-white") {}
            if (menu_row(st, "sm_off", "Shut Down...", ART_PC, false)) st->off = true;
        }
    }
}

static void taskbar(RC_App *app, AppState *st, int top, float h, RC_Insets safe) {
    /* The clock reads the user's local time, as Windows 95's did. It shows minutes, so the
       app wakes at the turn of each minute and sleeps between. Where the system cannot say
       the local time there is no clock, as with the taskbar's clock turned off: never a
       false one. rcLocalTime zeroes `now` then, so its year reads 0. */
    RC_LocalTime now = { 0 };
    rcWindowRequestFrameAfter(rcAppMainWindow(app),
                              rcLocalTime(rcUnixTimeSeconds(), &now) ? 60.5 - now.second : 60.0);

    /* On a phone the bar's face runs to the screen's edges, under the cutout and the home
       indicator, as a band does; its buttons and clock stay inside the safe area. */
    rcColumn(.id = "Taskbar", .bg = ui.face, .pb = safe.bottom, .pl = safe.left, .pr = safe.right,
             .w = "grow", .className = "border-t border-white")
    rcRow(.gap = 3, .p = 2, .align = "cl", .w = "grow", .hType = RC_PX(h)) {
        BUTTON("start", st->popup == START || HELD("start"), .h = "grow")
        rcRow(.gap = 3, .px = 5, .align = "cc", .h = "grow") {
            icon(ART_FLAG, 16.0f, 0);
            BOLD("Start", RC_BLACK);
        }
        st->popupHot |= rcIsHovered("start");
        if (rcClicked("start")) st->popup = st->popup == START ? NULL : START;
        if (st->popup == START) start_menu(st);
        /* The task buttons share the whole strip, up to 160px each; what they leave is the
           strip's own, at its end, so the clock stays at the right. */
        rcRow(.gap = 3, .w = "grow", .h = "grow", .className = "min-w-0")
        for (int n = 0; n < WIN_COUNT; n++) {
            const char *id = rcFormat(rcAppArena(app), "task%d", n).chars;
            if (!st->win[n].open) continue;
            /* Held down while its window is the active one, up otherwise; held, it has the
               light face and a bold label, as 1995's did. */
            BUTTON(id, n == top, .w = "grow", .h = "grow", .wMax = RC_VAL(160),
                   .className = "min-w-0")
            rcRow(.bg = n == top ? ui.light : RC_UNSET_COLOR, .gap = 4, .px = 4, .align = "cl",
                  .w = "grow", .h = "grow", .className = "truncate min-w-0") {
                icon(WINS[n].art, 16.0f, 0);
                rcTextC(WINS[n].title, .className = "truncate",
                        .font = (uint16_t)(n == top ? rcFontSemibold(13.0f) : 0),
                        .color = RC_BLACK);
            }
            if (rcClicked(id) && n == top) st->win[n].min = true;
            else if (rcClicked(id)) raise_window(st, n);
        }
        if (now.year) SUNKEN(NULL, ui.face, .h = "grow") rcBox(.px = 8)
            INK(rcFormat(rcAppArena(app), "%d:%02d %s", now.hour % 12 ? now.hour % 12 : 12,
                         now.minute, now.hour >= 12 ? "PM" : "AM").chars, RC_BLACK);
    }
}

/* Whole-screen layers: the running screen saver, and the famous last words. */
static void full_screen(RC_App *app, AppState *st, RC_Viewport vp) {
    RC_Float cover = { .to = RC_ATTACH_ROOT, .zIndex = Z_POPUP + 2, .capture = RC_CAPTURE_ON };
    if (st->saver) {
        rcBox(.w = "100%", .h = "100%", .floating = cover)
            saver_scene(st->saver, vp.width, vp.height, Z_POPUP + 3);
        rcWindowRequestFrame(rcAppMainWindow(app));
    }
    if (st->off) {
        rcColumn(.bg = RC_BLACK, .gap = 16, .p = 24, .align = "cc", .w = "100%", .h = "100%",
                 .floating = cover, .className = "select-none") {
            rcTextC("It is now safe to turn off your computer.", .font = rcFontSemibold(24.0f),
                    .color = rcHex(0xf09030));
            INK("Click to restart.", rcHex(0x808080));
        }
        if (rcPointerPressed(RC_POINTER_LEFT)) st->off = false;
    }
}

/* Any use wakes a running saver and restarts the wait. The clock's wake at each minute is
   what lets an idle desktop notice the wait has run out. The wait runs the APPLIED saver
   and Preview the sheet's pick, so `saver` names the one on screen: while the sheet is
   open its picks are not what is applied, and a "(None)" there must not fly at all. A
   Preview runs until the next use. A frame that reads the pointer draws every move after
   it, so it is read only on a click's edges, on frame one, while a saver runs and once the
   wait is up: a move made since restarts the wait then, so the saver starts 5 minutes
   after the last click and 5 to 10 after the last move. */
static void track_input(AppState *st) {
    bool    use = rcPointerPressed(RC_POINTER_LEFT) || rcPointerReleased(RC_POINTER_LEFT) ||
                  rcPointerPressed(RC_POINTER_RIGHT) || (st->saver && rcKeyPressed(RC_KEY_ESCAPE));
    RC_Vec2 p   = use || st->saver || st->lastInput == 0.0 ||
                  rcMonotonicSeconds() - st->lastInput >= 300.0 ? rcPointer() : st->lastPointer;
    if (use || st->lastInput == 0.0 || p.x != st->lastPointer.x || p.y != st->lastPointer.y) {
        st->lastPointer = p;
        st->lastInput   = rcMonotonicSeconds();
        st->saver       = 0;
    }
    if (!st->saver && rcMonotonicSeconds() - st->lastInput >= 300.0) st->saver = st->set.saver;
}

static void layout(RC_App *app, void *userData) {
    AppState   *st   = (AppState *)userData;
    RC_Viewport vp   = rcViewport();
    RC_Insets   safe = vp.safe;      /* phones draw under the bars: spend them once, here */
    float       bar  = COARSE ? 44.0f : 28.0f, lastW = st->deskW;   /* lastW: deskW last frame */

    ui        = app_palette(st->set.scheme);
    st->deskW = vp.width - safe.left - safe.right;
    st->deskH = vp.height - safe.top - safe.bottom - bar - 1.0f;   /* 1: the taskbar's border-t */
    /* A cell is 24px under a mouse and 34px under a finger, less where a short desk (a phone
       on its side) would cut the board: then the cells share what Minesweeper leaves of the
       desk, in whole pixels so that nine fill a row exactly. */
    st->cell  = COARSE ? clamp0((float)((int)(st->deskH - MINE_H(0.0f)) / 9), 34.0f) : 24.0f;
    /* The narrowest desk starts on Minesweeper alone: the sheet opens from the Display icon. */
    if (st->deskW < 360.0f && !st->phone) st->win[W_DISP].open = false;
    st->phone = st->deskW < 360.0f;
    /* A desk too narrow for the two side by side stacks them each time its width changes:
       Minesweeper 8px from the right edge, beside the icons where it clears their 104px and
       under them where it does not, and the sheet 6px below it. On a desk too short for
       both, window's clamp keeps the sheet on the desk, so it slides up behind Minesweeper.
       That is why Minesweeper goes in front: the larger window shows around the smaller
       one, never the reverse. A drag puts them anywhere until the desk's width changes. */
    if (st->deskW < 648.0f && st->deskW != lastW) {
        st->win[W_MINE].x = st->deskW - MINE_W(st->cell) - 8.0f;
        st->win[W_MINE].y = st->win[W_MINE].x < 104.0f ? 148.0f : 8.0f;
        st->win[W_DISP].x = 8.0f;
        st->win[W_DISP].y = st->win[W_MINE].y + MINE_H(st->cell) + 6.0f;
        st->top           = W_MINE;
    }
    /* Read once the desk has had its say above, and held for the frame: a press may raise
       mid-frame. */
    int last = st->top, back = !st->top, top = last;
    if (st->win[top].min || !st->win[top].open) top = back;
    if (st->win[top].min || !st->win[top].open) top = -1;
    if (st->drag >= 0 && !rcPointerDown(RC_POINTER_LEFT)) st->drag = -1;
    if (!st->win[W_DISP].open) st->live = st->set;   /* closing discards what was not applied */
    /* Esc closes the open popup, else the sheet on top. Read before track_input, so the Esc
       that wakes a running screen saver does neither. */
    if (st->popup && !st->saver && rcKeyPressed(RC_KEY_ESCAPE)) st->popup = NULL;
    else if (top == W_DISP && !st->saver && rcKeyPressed(RC_KEY_ESCAPE)) st->win[top].open = false;
    track_input(st);

    /* The desk and the taskbar run to the screen's sides; what sits on them keeps clear. */
    rcColumn(.id = "Root", .bg = ui.desk, .pt = safe.top, .w = "grow", .h = "grow",
             .className = "select-none") {
        rcBox(.id = "Desktop", .w = "grow", .h = "grow") {
            wallpaper(&st->set, vp.width, st->deskH);
            rcColumn(.gap = 16, .floating = { .to = RC_ATTACH_PARENT,
                     .offset = { safe.left + 8.0f, 12.0f }, .capture = RC_CAPTURE_PASSTHROUGH }) {
                desk_icon(st, "i_pc", "Display", ART_PC, W_DISP);
                desk_icon(st, "i_mine", "Minesweeper", ART_MINE, W_MINE);
            }
            /* The window behind, then the active one, each in its z band (see Z_POPUP). */
            window(app, st, back, back == top, safe);
            window(app, st, last, last == top, safe);
        }
        taskbar(app, st, top, bar, safe);
    }
    full_screen(app, st, vp);

    if (rcPointerPressed(RC_POINTER_LEFT) && !st->popupHot) st->popup = NULL;
    st->popupHot = false;
}

int main(void) {
    static AppState state = {
        /* The open sheet shows what is set, so Apply starts gray; the desktop starts bare. */
        .live  = { .saver = 1 }, .set = { .saver = 1 },
        .win   = { { 640.0f, 24.0f, true, false }, { 214.0f, 139.0f, true, false } },
        .top   = W_DISP, .drag = -1, .mines = { .seed = 1995 }, };
    static const float sizes[] = { 13.0f };   /* 1995's body size */
    RC_AppOptions opts = {   /* 1024x768, the era's high res */
        .width = 1024, .height = 768, .title = "RayClay 95", .fontSizes = sizes, .fontCount = 1,
        .scratchArenaBytes = 32768, .startLayoutElements = 8192,   /* rcFormat ids; mine pixels */
        .layoutCallback    = layout, .userData = &state, };
    return rcRunApp(&opts);
}
