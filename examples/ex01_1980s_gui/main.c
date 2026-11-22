/*
    main.c - RayClay "1980s desktop" example (the 1984 Macintosh)

    A System 1 desktop in strict 1-bit: a 50% dithered desktop, a menu bar that
    really opens, overlapping windows, the Finder, a working Calculator and the
    Note Pad. Press a title bar and drag: the window waits under a moving
    outline, as the original did to save the CPU.

    Everything is written in Macintosh pixels. The window sits at a fixed 200%
    zoom, so "1px" is one chunky Mac pixel on the device grid and the viewport
    reads as the Mac's own small screen. Three things keep it 1-bit:
      - a font drawn on a pixel grid, whose 20 px em puts one font pixel on one
        Mac pixel (examples/assets/fonts/Jersey10-OFL.txt says how it differs
        from Jersey 10);
      - hard black-on-white boxes: no radius, and a shadow only as a solid band
        with no blur;
      - one checker tile made on the CPU with rcImageUpdate, at the device's own
        resolution and drawn at exactly its own size, so that sampling cannot
        blur it to gray.

    Shows: floating windows with a z-order, a menu bar that doubles as the OS
    window's drag strip, a text area in a custom RC_Style, a pressed look written
    as an active: class, keyboard entry, and windows placed from the measured
    viewport. Same source for desktop, web and phone.

    Build target: rayclay_ex01_1980s_gui
*/

#include "rayclay.h"

#define MAC_ZOOM 2.0f    /* device pixels per Mac pixel, before the display's own scale */
#define MENU_H   20.0f
#define TITLE_H  19.0f
#define KEY_W    28.0f
#define KEY_H    22.0f   /* 44 window px at MAC_ZOOM, the touch-target minimum */
#define TILE     240     /* device px: whole checker periods for cells of 1 to 6 px */

#define OUTLINE .border = { .color = RC_BLACK, .width = 1 }
#define LABEL(text, ink) rcTextC((text), .color = (ink), .select = RC_SELECT_NONE)

/* Drawn into this app's font, so they sit on the pixel grid like every letter. */
#define CMD   "\xe2\x8c\x98"   /* the cloverleaf */
#define MARK  "\xee\x80\x80"   /* the RayClay mark, where the Apple was */

/* Two colors, and .radius = 0 squares every widget. */
static RC_Style app_style(void) {
    RC_Style s   = rcStyleLight();
    s.background = s.surface = s.surfaceAlt = s.chrome = RC_WHITE;
    s.text       = s.textMuted = s.primary = s.primaryHover = s.border = RC_BLACK;
    s.radius     = 0.0f;
    return s;
}

typedef enum { W_DISK, W_CALC, W_NOTE, W_COUNT, W_ABOUT, W_NONE } WinId;
typedef enum { M_NONE, M_LOGO, M_FILE, M_EDIT, M_VIEW, M_SPECIAL, M_COUNT } MenuId;
typedef enum { PIC_DISK, PIC_TRASH, PIC_FULL, PIC_MAC, PIC_CALC, PIC_NOTE, PIC_COUNT } Picture;
typedef enum { I_NONE, I_DISK, I_TRASH, I_HEAD, I_SYSTEM, I_CALC, I_NOTE, I_COUNT } ItemId;

static const struct { const char *id, *title, *close, *name; float w; } WINS[W_COUNT] = {
    { "wDisk", "wDiskT", "wDiskX", "System Disk", 300.0f },
    { "wCalc", "wCalcT", "wCalcX", "Calculator",  142.0f },
    { "wNote", "wNoteT", "wNoteX", "Note Pad",    300.0f },
};

static const struct { const char *id, *label; float w; } MENUS[M_COUNT] = {
    { "", "", 0.0f }, { "mLogo", MARK, 176.0f }, { "mFile", "File", 120.0f },
    { "mEdit", "Edit", 120.0f }, { "mView", "View", 104.0f }, { "mSpecial", "Special", 148.0f },
};

/* The disk's three files add up to the 162K the Finder reports; the Trash holds 8K
   more. I_HEAD, which never shows selected, heads the Finder's list by Name. */
static const struct { const char *id, *name, *size; Picture pic; WinId opens; } ITEMS[I_COUNT] = {
    { "",        "",            "",     PIC_DISK, W_NONE  },
    { "iDisk",   "System Disk", "",     PIC_DISK, W_DISK  },
    { "iTrash",  "Trash",       "",     PIC_FULL, W_NONE  },
    { "iHead",   "Name",        "Size", PIC_DISK, W_NONE  },
    { "iSystem", "System",      "141K", PIC_MAC,  W_ABOUT },
    { "iCalc",   "Calculator",  "6K",   PIC_CALC, W_CALC  },
    { "iNote",   "Note Pad",    "15K",  PIC_NOTE, W_NOTE  },
};

/* The keys, row by row, each with its keypad key; a digit also takes its top-row
   key. These sit in the same place on every layout, so no operator depends on a
   US keyboard. A label is also the key's element id and the character calc_key
   reads. */
static const struct { const char *label; RC_Key pad; } KEYS[18] = {
    { "C", RC_KEY_ESCAPE }, { "E", RC_KEY_BACKSPACE }, { "=", RC_KEY_ENTER },
    { "x", RC_KEY_KP_MULTIPLY }, { "7", RC_KEY_KP_7 }, { "8", RC_KEY_KP_8 }, { "9", RC_KEY_KP_9 },
    { "/", RC_KEY_KP_DIVIDE }, { "4", RC_KEY_KP_4 }, { "5", RC_KEY_KP_5 }, { "6", RC_KEY_KP_6 },
    { "-", RC_KEY_KP_SUBTRACT }, { "1", RC_KEY_KP_1 }, { "2", RC_KEY_KP_2 }, { "3", RC_KEY_KP_3 },
    { "+", RC_KEY_KP_ADD }, { "0", RC_KEY_KP_0 }, { ".", RC_KEY_KP_DECIMAL },
};

/* Each icon is five outlined (0) or filled (1) boxes in a 32x32 cell, the size of a
   real 1984 icon. The full Trash bulges and a sheet pushes its lid up and aside, so
   its outline differs from the empty can's and Empty Trash shows on the desktop. */
typedef struct { int8_t x, y, w, h, ink; } Shape;
static const Shape PICS[PIC_COUNT][5] = {
    { {1,0,30,32,0}, {8,0,16,11,0}, {17,2,4,7,1}, {6,15,20,17,0}, {9,22,14,1,1} },
    { {11,0,10,4,0}, {3,3,26,4,0}, {5,6,22,26,0}, {12,10,1,18,1}, {19,10,1,18,1} },
    { {7,3,11,9,0}, {3,9,26,23,0}, {10,0,22,4,0}, {11,13,1,15,1}, {20,13,1,15,1} },
    { {4,0,24,26,0}, {7,3,18,14,0}, {18,21,7,2,1}, {7,26,18,6,0}, {10,7,8,1,1} },
    { {4,0,24,32,0}, {7,3,18,7,0}, {7,13,18,4,1}, {7,19,18,4,1}, {7,25,18,4,1} },
    { {5,0,22,32,0}, {9,7,14,1,1}, {9,13,14,1,1}, {9,19,14,1,1}, {20,25,7,7,1} },
};

typedef struct {
    float    x[W_COUNT], y[W_COUNT];
    bool     open[W_COUNT];
    WinId    z[W_COUNT];      /* back to front */
    bool     placed, roomy, about, byName, trashFull, fresh, menuWasOpen;
    MenuId   menu;
    /* selected and pressed (this frame's) are ItemIds; drag is the WinId + 1 under
       the title bar's outline, 0 for none */
    int      selected, pressed, drag;
    RC_Vec2  grab;
    double   acc, cur, div;   /* Calculator: the typed entry is cur / div, div 0 before a point */
    char     entry[20], op, note[400];   /* entry is what the display shows */
    RC_Image gray;            /* the checker tile, rebuilt when the device pixel changes */
    int      cell, side;      /* its cell and its side, in device px */
    float    px;              /* device px per Mac pixel */
} AppState;

static float clampf(float v, float lo, float hi) {
    return v > hi ? hi : v < lo ? lo : v;
}

static WinId front(const AppState *st) {
    for (int i = W_COUNT - 1; i >= 0; i--)
        if (st->open[st->z[i]]) return st->z[i];
    return W_NONE;
}

static void raise_win(AppState *st, WinId w) {
    int i = 0;
    while (st->z[i] != w) i++;
    for (; i < W_COUNT - 1; i++) st->z[i] = st->z[i + 1];
    st->z[W_COUNT - 1] = w;
    /* The caret follows the front window, so digits reach the Calculator. */
    rcSetFocus(w == W_NOTE ? "note" : NULL);
}

static void open_win(AppState *st, WinId w) {
    if (w == W_ABOUT) st->about = true;
    if (w < W_COUNT) {
        st->open[w] = true;
        raise_win(st, w);
    }
}

/* A roomy screen spreads the windows; a small one stacks them, the Calculator in
   front, and leaves the icon column on the right edge uncovered. A narrow screen
   starts the Finder under the disk, so a Note Pad opened at the top covers its name
   whole; elsewhere the Note Pad opens one title bar below the Finder, as an edge
   through the Finder's title would cut its name in half. The Calculator (190 tall)
   stands 10 px off the bottom and about an icon's width from the Trash, so it reads
   as placed, not wedged. Under 400 px of height that would put its top within a few
   pixels of the Finder's bottom edge (listed by Name, the Finder stands 19 px taller),
   so there it starts no lower than the foot of the Finder's icon names: 28 px or more
   of overlap reads as two stacked windows, where a sliver reads as a mistake. */
static void clean_up(AppState *st, RC_Viewport vp) {
    bool narrow = vp.width < 400.0f;
    st->x[W_DISK] = st->roomy ? 28.0f : 4.0f;
    st->y[W_DISK] = st->roomy ? 40.0f : narrow ? MENU_H + 66.0f : MENU_H + 4.0f;
    st->x[W_CALC] = st->roomy ? 344.0f : narrow ? 2.0f : vp.width - 232.0f;
    st->y[W_CALC] = st->roomy ? 56.0f : vp.height >= 400.0f ? vp.height - 200.0f
                  : clampf(vp.height - 200.0f, MENU_H, MENU_H + 112.0f);
    st->x[W_NOTE] = st->roomy ? 96.0f : 8.0f;
    st->y[W_NOTE] = st->roomy ? 214.0f : narrow ? MENU_H + 12.0f : st->y[W_DISK] + TITLE_H + 1.0f;
}

static void calc_clear(AppState *st) {
    st->acc   = st->cur = st->div = 0.0;
    st->op    = 0;
    st->fresh = true;
    rcStrCopy(st->entry, "0", sizeof st->entry);
}

/* An operator folds the pending one, then arms the next; '=' arms none. The fold
   runs only when a number was typed, so changing your mind about an operator is
   safe. Adding 0.0 turns the -0.0 of "-3 x 0" into the 0 a person expects. */
static void calc_op(RC_Arena *arena, AppState *st, char op) {
    if (!st->fresh) {
        double cur = st->div > 0.0 ? st->cur / st->div : st->cur;
        if (st->op == '/' && cur == 0.0) {
            calc_clear(st);
            rcStrCopy(st->entry, "Error", sizeof st->entry);
            return;
        }
        st->acc = st->op == '+' ? st->acc + cur : st->op == '-' ? st->acc - cur
                : st->op == 'x' ? st->acc * cur : st->op == '/' ? st->acc / cur : cur;
        rcStrCopy(st->entry, rcFormat(arena, "%.10g", st->acc + 0.0).chars, sizeof st->entry);
    }
    st->op    = op;
    st->fresh = true;
    st->cur   = st->div = 0.0;   /* the next digit starts a new entry */
}

/* The display keeps the text as typed, trailing zeros and all; cur and div keep
   its value alongside, so nothing has to parse it back. */
static void calc_key(RC_Arena *arena, AppState *st, char c) {
    if (c == 'C') {
        calc_clear(st);
    } else if (c == 'E') {
        rcStrCopy(st->entry, "0", sizeof st->entry);
        st->cur = st->div = 0.0;
    } else if ((c >= '0' && c <= '9') || c == '.') {
        /* A fresh entry or a lone 0 restarts: only a point keeps the 0 before it. */
        if (st->fresh || (st->entry[0] == '0' && !st->entry[1]))
            rcStrCopy(st->entry, c == '.' ? "0" : "", sizeof st->entry);
        st->fresh = false;
        int n = (int)rcStringFromCStr(st->entry).length;
        if (n < 12 && !(c == '.' && st->div > 0.0)) {
            st->entry[n]     = c;
            st->entry[n + 1] = '\0';
            st->cur = c == '.' ? st->cur : st->cur * 10.0 + (c - '0');
            st->div = c == '.' ? 1.0 : st->div * 10.0;
        }
    } else {
        calc_op(arena, st, c == '=' ? 0 : c);
    }
}

/* With a key event `e`, whether it presses key i: a press, not an auto-repeat, of
   its keypad key or, for a digit, its top-row key; '=' takes either Enter. With
   NULL, whether key i's keyboard key is held (rcKeyDown), to show it pressed.
   Primary chords are the menus' shortcuts, so they never type, and a key that met
   an open menu belongs to the menu: Esc closes it without clearing the display.
   Esc is Android's Back, and reading it held or consuming its event claims Back
   for the app, so Esc counts only while Clear has something to clear (KEYS[0]);
   else Back leaves the app. So C does not invert under a held Esc: its press has
   cleared the display. */
static bool typed(const AppState *st, int i, const RC_KeyEvent *e) {
    bool idle = !i && !st->op && st->acc == 0.0 && st->entry[0] == '0' && !st->entry[1];
    if (idle || front(st) != W_CALC || st->menuWasOpen || st->about || rcModDown(RC_MOD_PRIMARY))
        return false;
    char   c   = KEYS[i].label[0];
    RC_Key top = c >= '0' && c <= '9' ? (RC_Key)(RC_KEY_0 + (c - '0'))
               : c == '=' ? RC_KEY_KP_ENTER : KEYS[i].pad;
    if (!e) return rcKeyDown(KEYS[i].pad) || rcKeyDown(top);
    return !e->repeat && (e->key == KEYS[i].pad || e->key == top);
}

/* A key inverts while it is held, by the pointer or its keyboard key: the era
   had no hover state. rcShowsActive is the pressed look and asks for no frame
   (rcPointerDown would draw every frame a finger rests); rcIsHovered pops the key
   back up when the pointer slides off, as in 1984. rcKeyDown asks for another
   frame while it answers true. */
static void key(const AppState *st, int i, float w) {
    const char *id = KEYS[i].label;
    bool held = (rcShowsActive(id) && rcIsHovered(id)) || typed(st, i, NULL);
    rcBox(.id = id, .bg = held ? RC_BLACK : RC_WHITE, .align = "cc", OUTLINE, .wType = RC_PX(w),
          .hType = RC_PX(KEY_H)) { LABEL(id, held ? RC_WHITE : RC_BLACK); }
}

static void calculator(RC_Arena *arena, AppState *st) {
    /* Keys are read before the display is declared, so the readout drawn this
       frame shows the keys pressed in it. rcKeyEvent gives the frame's presses in
       the order they were typed, each its own event, where rcKeyPressed holds one
       press of a key a frame and no order between keys: "21" typed in one frame
       gap reads 21, and "11" is two digits. Consuming the event a key takes is
       what claims an Esc's Android Back. */
    for (int i = 0; i < 18; i++)
        if (rcClicked(KEYS[i].label)) calc_key(arena, st, KEYS[i].label[0]);
    RC_KeyEvent e;
    for (int n = 0; rcKeyEvent(n, &e); n++)
        for (int i = 0; i < 18; i++)
            if (typed(st, i, &e)) { rcKeyEventConsume(n); calc_key(arena, st, KEYS[i].label[0]); }
    rcColumn(.gap = 4, .p = 8, .w = "grow") {
        rcBox(.px = 6, .align = "cr", OUTLINE, .w = "grow", .hType = RC_PX(KEY_H)) {
            rcTextC(st->entry, .color = RC_BLACK);
        }
        for (int r = 0; r < 5; r++) {
            rcRow(.gap = 4) {
                for (int c = 0; c < (r < 4 ? 4 : 2); c++)
                    key(st, r * 4 + c, r < 4 ? KEY_W : 2.0f * KEY_W + 4.0f);
            }
        }
    }
}

static void picture(Picture pic, RC_Color ink, RC_Color paper) {
    rcBox(.w = "32px", .h = "32px") {
        for (int i = 0; i < 5; i++) {
            const Shape *s = &PICS[pic][i];
            RC_Float fl = { .to = RC_ATTACH_PARENT, .offset = { (float)s->x, (float)s->y },
                            .capture = RC_CAPTURE_PASSTHROUGH };
            rcBox(.bg = s->ink ? ink : paper, .border = { .color = ink, .width = 1 },
                  .wType = RC_PX(s->w), .hType = RC_PX(s->h), .floating = fl) {}
        }
    }
}

/* An item is its picture over its name, or one row of a list, inverted while
   selected. Either way it keeps its id, so selecting and opening work alike. */
static void item(AppState *st, int i, bool listed) {
    bool     sel   = i != I_HEAD && st->selected == i;
    RC_Color ink   = sel ? RC_WHITE : RC_BLACK, paper = sel ? RC_BLACK : RC_WHITE;
    if (listed) {
        rcRow(.id = ITEMS[i].id, .bg = paper, .px = 4, .w = "grow") {
            rcBox(.w = "grow") { LABEL(ITEMS[i].name, ink); }
            LABEL(ITEMS[i].size, ink);
        }
    } else {
        rcColumn(.id = ITEMS[i].id, .gap = 2, .align = "tc", .w = "84px") {
            picture(i == I_TRASH && !st->trashFull ? PIC_TRASH : ITEMS[i].pic, ink, paper);
            rcBox(.bg = paper, .px = 2) { LABEL(ITEMS[i].name, ink); }
        }
    }
    if (rcPointerPressed(RC_POINTER_LEFT) && rcIsHovered(ITEMS[i].id)) st->pressed = i;
}

static void hairline(void) {
    rcBox(.bg = RC_BLACK, .w = "grow", .h = "1px") {}
}

/* The figures sit on one line where they fit and wrap as a web page's flex-wrap row
   does where they do not: a figure that has no room moves to the next line whole. */
static void finder(RC_App *app, AppState *st) {
    int kb = st->trashFull ? 170 : 162;   /* the Trash stays on the disk until it is emptied */
    /* Growing fills the narrower side slot first, so the two end up equal and
       the middle figure sits on the window's center line. */
    rcRow(.px = 6, .align = "cl", .w = "grow", .hMin = RC_VAL(18.0f), .className = "flex-wrap") {
        rcBox(.w = "grow") { LABEL("3 items", RC_BLACK); }
        LABEL(rcFormat(rcAppArena(app), "%dK in disk", kb).chars, RC_BLACK);
        rcBox(.align = "cr", .w = "grow") {
            LABEL(rcFormat(rcAppArena(app), "%dK available", 400 - kb).chars, RC_BLACK);
        }
    }
    hairline();
    /* Blank scroll bars: System 1 draws them so while everything fits. The View
       menu's "by Name" turns the icons into a list, as the Finder did. A list takes
       less air than icons, which leaves a phone's Finder room to stand clear of the
       disk's name above it and of the Trash below. */
    rcRow(.w = "grow") {
        rcRow(.gap = (float)(st->byName ? 0 : 4), .px = 8, .w = "grow",
              .className = st->byName ? "flex-col py-[8px]" : "flex-wrap py-[12px]") {
            for (int i = st->byName ? I_HEAD : I_SYSTEM; i < I_COUNT; i++) {
                item(st, i, st->byName);
                if (i == I_HEAD) hairline();   /* a rule sets the list's header off */
            }
        }
        rcBox(.bg = RC_BLACK, .w = "1px", .h = "grow") {} rcBox(.w = "15px") {}
    }
    hairline();
    rcRow(.align = "cr", .w = "grow", .h = "15px") {   /* the rule runs on to the corner */
        rcBox(.bg = RC_BLACK, .w = "1px", .h = "grow") {} rcBox(.w = "15px") {}
    }
}

/* Copies of the gray tile, each at exactly its own size so that every texel lands
   on one device pixel, cover w x h of the parent, whose overflow cuts the edges.
   Under the in-flow tree (z -1) over a black clear color they are the desktop;
   over a menu row (z 1001, above the panel) white pixels over ink gray it out, as
   System 1 did. */
static void gray(const AppState *st, float w, float h, int16_t z) {
    float side = (float)st->side / st->px;
    for (float y = 0.0f; y < h; y += side)
        for (float x = 0.0f; x < w; x += side) {
            RC_Float fl = { .to = RC_ATTACH_PARENT, .offset = { x, y }, .zIndex = z,
                            .capture = RC_CAPTURE_PASSTHROUGH };
            rcBox(.wType = RC_PX(side), .hType = RC_PX(side), .floating = fl, .image = &st->gray) {}
        }
}

/* Releasing on a row chooses it, after a click or after holding the title down
   and sliding here, the 1984 way. rcClicked is what gives it the pointer cursor.
   The row lights by rcShowsHover, the hover look; the release tests rcIsHovered,
   since a finger's lift already shows no hover. */
static bool menu_item(AppState *st, const char *id, const char *label, const char *keys,
                      bool enabled, bool checked) {
    bool     over = enabled && rcIsHovered(id), lit = over && rcShowsHover(id);
    RC_Color ink = lit ? RC_WHITE : RC_BLACK;
    rcRow(.id = id, .bg = lit ? RC_BLACK : RC_WHITE, .pr = 8, .align = "cl", .overflow = "hidden",
          .w = "grow", .hType = RC_PX(KEY_H)) {
        rcBox(.align = "cc", .w = "16px") { if (checked) LABEL("\xe2\x9c\x93", ink); }
        LABEL(label, ink);
        rcSpacer() {}
        if (keys) LABEL(keys, ink);
        if (!enabled) gray(st, MENUS[st->menu].w, KEY_H, 1001);
    }
    bool chosen = enabled && (rcClicked(id) || (over && rcPointerReleased(RC_POINTER_LEFT)));
    if (chosen) st->menu = M_NONE;
    return chosen;
}

static void menu_rule(const AppState *st) {
    rcBox(.bg = RC_BLACK, .overflow = "hidden", .w = "grow", .h = "1px") {
        gray(st, MENUS[st->menu].w, 1.0f, 1001);
    }
}

static void close_front(AppState *st) {
    if (front(st) != W_NONE) st->open[front(st)] = false;
}

/* A panel hangs from its title, slid left as far as it takes to keep it on the screen. */
static void menu_panel(RC_App *app, AppState *st, RC_Viewport vp) {
    WinId    f    = front(st);
    float    x    = rcGetElementBox(MENUS[st->menu].id).x;
    float    dx   = clampf(vp.width - vp.safe.right - x - MENUS[st->menu].w - 1.0f, -x, 0.0f);
    RC_Float fl   = { .to = RC_ATTACH_ELEMENT, .toId = MENUS[st->menu].id,
                      .parent = RC_ANCHOR_BOTTOM_LEFT, .element = RC_ANCHOR_TOP_LEFT,
                      .offset = { dx, 0.0f }, .zIndex = 1000, .clip = RC_CLIP_NONE };
    /* The 1px of padding keeps the rows, and the gray over a dimmed one, off the outline. */
    rcColumn(.id = "mPanel", .bg = RC_WHITE, .p = 1, OUTLINE, .wType = RC_PX(MENUS[st->menu].w),
             .floating = fl, .className = "shadow-[1px_1px_0_black]") {
        if (st->menu == M_LOGO) {
            if (menu_item(st, "miAbout", "About This Desktop", NULL, true, false)) st->about = true;
            menu_rule(st);
            if (menu_item(st, "miCalc", "Calculator", NULL, true, false)) open_win(st, W_CALC);
            if (menu_item(st, "miNote", "Note Pad", NULL, true, false)) open_win(st, W_NOTE);
        } else if (st->menu == M_FILE) {
            bool canOpen = ITEMS[st->selected].opens != W_NONE;
            if (menu_item(st, "miOpen", "Open", CMD "O", canOpen, false))
                open_win(st, ITEMS[st->selected].opens);
            if (menu_item(st, "miClose", "Close", CMD "W", f != W_NONE, false)) close_front(st);
            menu_rule(st);
            /* Dim where the run's end would leave a web page or an iOS app frozen. */
            if (menu_item(st, "miQuit", "Quit", CMD "Q", rcChildWindowsSupported(), false))
                rcAppRequestClose(app);
        } else if (st->menu == M_EDIT) {
            if (menu_item(st, "miCopy", "Copy", CMD "C", f == W_CALC, false))
                rcClipboardSet(st->entry);
            if (menu_item(st, "miClear", "Clear", NULL, f == W_CALC, false)) calc_clear(st);
        } else if (st->menu == M_VIEW) {
            if (menu_item(st, "miIcon", "by Icon", NULL, true, !st->byName)) st->byName = false;
            if (menu_item(st, "miName", "by Name", NULL, true, st->byName)) st->byName = true;
        } else {
            if (menu_item(st, "miClean", "Arrange Windows", NULL, true, false)) clean_up(st, vp);
            if (menu_item(st, "miEmpty", "Empty Trash", NULL, st->trashFull, false))
                st->trashFull = false;
        }
    }
}

/* A press on a title opens it, and sliding to another switches, so a menu works
   held down or click by click. The menu bar is also the OS drag strip: the menus
   opt out of it, and the empty rest of the bar still moves the window. */
static void menu_bar(AppState *st) {
    /* A phone's narrow screen holds all five titles only with less padding. */
    float    vw   = rcViewport().width;
    float    pad  = vw < 180.0f ? 2.0f : vw < 260.0f ? 4.0f : 7.0f;
    bool     over = false;
    rcColumn(.id = RC_ID_WINDOW_DRAG, .bg = RC_WHITE, .w = "grow", .hType = RC_PX(MENU_H)) {
        rcRow(.pl = 4, .align = "cl", .noDrag = true, .h = "grow") {
            for (MenuId m = M_LOGO; m < M_COUNT; m = (MenuId)(m + 1)) {
                const char *id   = MENUS[m].id;
                bool        open = st->menu == m;
                rcBox(.id = id, .bg = open ? RC_BLACK : RC_WHITE, .px = pad, .align = "cc",
                      .h = "grow") {
                    LABEL(MENUS[m].label, open ? RC_WHITE : RC_BLACK);
                }
                if (!rcIsHovered(id)) continue;
                over = true;
                if (rcPressed(id)) st->menu = open ? M_NONE : m;
                else if (st->menu != M_NONE)
                    st->menu = m;
            }
        }
        hairline();
    }
    if ((!over && rcPointerPressed(RC_POINTER_LEFT) && !rcIsHovered("mPanel"))
        || (st->menu != M_NONE && rcKeyPressed(RC_KEY_ESCAPE)))
        st->menu = M_NONE;
}

/* Fills the tile with a checker of white and clear cells, each `cell` device pixels
   square. Every pixel is all zeros or all ones, so the byte order cannot matter. The
   side is a whole number of periods, so tiles laid edge to edge never show a seam. */
static void screen_pattern(RC_Window *win, AppState *st) {
    static uint32_t pixels[TILE * TILE];
    st->px = rcWindowZoom(win) * rcWindowContentScale(win);
    int cell = (int)(st->px + 0.5f), side = TILE / (2 * cell) * 2 * cell;
    if (cell == st->cell) return;
    for (int y = 0; y < side; y++)
        for (int x = 0; x < side; x++)
            pixels[y * side + x] = ((x / cell + y / cell) & 1) ? 0xFFFFFFFFu : 0u;
    rcImageUpdate(&st->gray, pixels, side, side, 0, RC_PIXEL_RGBA8);
    st->cell = cell;
    st->side = side;
}

static void shortcuts(RC_App *app, AppState *st) {
    if (!rcModDown(RC_MOD_PRIMARY)) return;
    if (rcKeyPressed(RC_KEY_W)) close_front(st);
    if (rcKeyPressed(RC_KEY_O)) open_win(st, ITEMS[st->selected].opens);
    if (rcKeyPressed(RC_KEY_C) && front(st) == W_CALC) rcClipboardSet(st->entry);
    if (rcKeyPressed(RC_KEY_Q) && rcChildWindowsSupported()) rcAppRequestClose(app);
}

/* A press raises the window under it; a window above another captures the pointer,
   so at most one answers. On a title bar it starts a drag that moves only an
   outline; the release moves the window. */
static void window_gestures(AppState *st) {
    RC_Vec2 p = st->drag || rcPointerPressed(RC_POINTER_LEFT) ? rcPointer() : RC_LIT(RC_Vec2){ 0 };
    for (int w = 0; w < W_COUNT && rcPointerPressed(RC_POINTER_LEFT); w++) {
        if (!st->open[w] || !rcIsHovered(WINS[w].id)) continue;
        raise_win(st, (WinId)w);
        if (rcIsHovered(WINS[w].title) && !rcIsHovered(WINS[w].close)) {
            st->drag = w + 1;
            st->grab = RC_LIT(RC_Vec2){ p.x - st->x[w], p.y - st->y[w] };
        }
    }
    if (st->drag && !rcPointerDown(RC_POINTER_LEFT)) {
        st->x[st->drag - 1] = p.x - st->grab.x;
        st->y[st->drag - 1] = p.y - st->grab.y;
        st->drag = 0;
    }
}

/* Six 1px rules a pixel apart. */
static void stripes(const char *w) {
    rcColumn(.gap = 1, .w = w) {
        for (int i = 0; i < 6; i++) hairline();
    }
}

/* Only the front window has stripes and a close box. The box is drawn at its 1984 size, and a
   float centered on it takes the press: as large as the box under a mouse, KEY_H square under
   a finger. Such a float counts as its parent's child, so the box's active: look and the title
   bar's hover both read a press on it. A float counts its z from the float it is declared
   in: the Finder's icons land 1 above their window, the Note Pad's scroll bar and focus ring
   2, so `z` steps by 3 and the window in front covers all of the one behind. */
static void window_frame(RC_App *app, AppState *st, WinId w, float width, int16_t z) {
    bool     active = front(st) == w;
    RC_Size  hit    = RC_PX(rcPointerIsCoarse() ? KEY_H : 11.0f);
    RC_Float fl     = { .to = RC_ATTACH_PARENT, .offset = { st->x[w], st->y[w] }, .zIndex = z };
    rcColumn(.id = WINS[w].id, .bg = RC_WHITE, OUTLINE, .wType = RC_PX(width),
             .floating = fl, .className = "shadow-[1px_1px_0_black]") {
        rcRow(.id = WINS[w].title, .px = 1, .align = "cc", .w = "grow", .hType = RC_PX(TITLE_H)) {
            if (active) {
                stripes("8px");
                rcBox(.px = 2) {
                    rcBox(OUTLINE, .className = "size-[11px] bg-white active:bg-black") {
                        rcBox(.id = WINS[w].close, .wType = hit, .hType = hit,
                              .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_CENTER,
                                            .element = RC_ANCHOR_CENTER }) {}
                    }
                }
                stripes("grow");
            }
            /* A title bar never wraps: a narrow window cuts its name short instead. */
            rcBox(.px = 6, .overflow = "hidden", .className = "min-w-0") {
                rcTextC(WINS[w].name, .color = RC_BLACK, .wrap = "e", .select = RC_SELECT_NONE);
            }
            if (active) {
                stripes("grow");
                stripes("23px");   /* as wide as the close box's run: the name stays centered */
            }
        }
        if (active && rcClicked(WINS[w].close)) st->open[w] = false;
        hairline();
        if (w == W_CALC)
            calculator(rcAppArena(app), st);
        else if (w == W_DISK)
            finder(app, st);
        else
            rcTextArea("note", st->note, (int)sizeof st->note,
                       .className = "w-full h-[134px] border-0");
        /* The drag's outline is a float of the window itself: growing, it takes the window's
           size as laid out in this frame, and it stands as far off as the pointer has gone. */
        if (st->drag == (int)w + 1) {
            RC_Vec2  p  = rcPointer();
            RC_Float at = { .to = RC_ATTACH_PARENT,
                            .offset = { p.x - st->grab.x - st->x[w], p.y - st->grab.y - st->y[w] },
                            .zIndex = 500, .capture = RC_CAPTURE_PASSTHROUGH };
            rcBox(OUTLINE, .w = "grow", .h = "grow", .floating = at) {}
        }
    }
}

/* A short screen packs the box tight, so it never reaches the menu bar. */
static void about_box(AppState *st, bool low) {
    if (rcBeginModal("about", &st->about, .modality = RC_MODALITY_NON_MODAL)) {
        rcColumn(.gap = (float)(low ? 0 : 8), .px = 12, .py = (float)(low ? 2 : 12),
                 .align = "tc", .border = { .color = RC_BLACK, .width = 2 }) {
            rcTextL(MARK, .color = RC_BLACK, .size = 40);
            rcTextL("RayClay Desktop", .color = RC_BLACK);
            rcTextL("One C file, 1-bit on purpose.", .color = RC_BLACK);
            /* The default button's hover fill is the style's border, black under a black
               label. A class fill takes no hover shade, so this one inverts only while
               pressed, as in 1984. */
            if (rcButton("aboutOk", "OK",
                         .className = "h-[22px] bg-white active:bg-black active:text-white"))
                st->about = false;
        }
        rcEndModal();
    }
}

/* A press commits the selection, or opens a double-clicked item. A press on bare
   desktop leaves pressed at I_NONE, which drops the selection, as the Finder did. */
static void selection(AppState *st) {
    if (!rcPointerPressed(RC_POINTER_LEFT)) return;
    if (rcClickCount() == 2 && st->pressed == st->selected)
        open_win(st, ITEMS[st->pressed].opens);
    st->selected = st->pressed;
}

static void layout(RC_App *app, void *userData) {
    AppState   *st   = (AppState *)userData;
    RC_Viewport vp   = rcViewport();
    RC_Insets   safe = vp.safe;
    RC_Window  *win  = rcAppMainWindow(app);
    float       sw   = vp.width - safe.left - safe.right, sh = vp.height - safe.top - safe.bottom;
    st->menuWasOpen = st->menu != M_NONE;

    /* Crossing the roomy line, the first frame included, deals the windows again. */
    bool roomy = vp.width >= 600.0f && vp.height >= 380.0f;
    if (rcWindowZoom(win) != MAC_ZOOM) {
        rcWindowSetZoom(win, MAC_ZOOM);
    } else if (!st->placed || roomy != st->roomy) {
        st->roomy        = roomy;
        st->placed       = true;
        st->open[W_NOTE] = roomy;
        /* A phone, either way up, opens on the Finder and the Trash whole: there the
           Calculator would cover both, so it opens from its icon. (Mac px.) */
        st->open[W_CALC] = vp.width >= 300.0f && vp.height >= 300.0f;
        st->byName       = vp.width < 400.0f;   /* a narrow Finder lists, so it stays short */
        clean_up(st, vp);
    }
    screen_pattern(win, st);
    shortcuts(app, st);

    rcColumn(.id = "Root", .pt = safe.top, .pb = safe.bottom,
             .pl = safe.left, .pr = safe.right, .w = "grow", .h = "grow") {
        rcColumn(.id = "Screen", .overflow = "hidden", .w = "grow", .h = "grow") {
            gray(st, sw, sh, -1);
            menu_bar(st);
            if (st->menu != M_NONE) menu_panel(app, st, vp);
            rcColumn(.id = "Desk", .p = 4, .align = "tr", .w = "grow", .h = "grow") {
                item(st, I_DISK, false);
                rcSpacer() {}
                item(st, I_TRASH, false);
            }
            if (!st->menuWasOpen) window_gestures(st);
            for (int i = 0; i < W_COUNT; i++) {
                WinId w = st->z[i];
                /* A shrunken screen can never strand a window or its title bar. */
                float width = clampf(WINS[w].w, 40.0f, sw - 8.0f);
                st->x[w] = clampf(st->x[w], 32.0f - width, sw - 32.0f);
                st->y[w] = clampf(st->y[w], MENU_H, sh - TITLE_H);
                if (st->open[w]) window_frame(app, st, w, width, (int16_t)(3 * i));
            }
            about_box(st, sh < 260.0f);
        }
    }
    /* A press that met a menu, or opened one, keeps the selection for File > Open. */
    if (!st->menuWasOpen && st->menu == M_NONE) selection(st);
    st->pressed = I_NONE;
    /* What a press or a release chose was declared too late to draw in this frame. */
    if (rcPointerPressed(RC_POINTER_LEFT) || rcPointerReleased(RC_POINTER_LEFT))
        rcWindowRequestFrame(win);
}

int main(void) {
    static AppState state = {
        .open = { true, true, false }, .z = { W_DISK, W_NOTE, W_CALC }, .trashFull = true,
        .fresh = true, .entry = "0",
        .note = "Back up the System Disk.\nOrder a box of 400K disks.\nCall the dealer about "
                "the 512K upgrade.",
    };

    /* The grid font at one font pixel per Mac pixel: what the library's widgets draw at.
       The About box's mark asks for two with .size, baked on first use like this one,
       with fontOversample's one sample, so its edges stay pure black and white. */
    static const float sizes[1] = { 20.0f };

    rcSetStyle(app_style());

    RC_AppOptions opts = {
        .width = 1280, .height = 820, .title = "System Disk",
        .clearColor = RC_BLACK,   /* the desktop's ink, under the clear half of the gray tile */
        .fontPath  = "examples/assets/fonts/Jersey10-Grid.ttf",
        .fontSizes = sizes, .fontCount = 1,
        .fontOversample = 1,      /* pure black and white take no sub-pixel placement */
        .frame = RC_WINDOW_FRAME_NONE,
        .layoutCallback = layout, .userData = &state,
        .zoom     = { .disabled = true },   /* a whole factor keeps every Mac pixel whole */
        .titlebar = { .height = (int)(MENU_H * MAC_ZOOM) }, /* the menu bar is the title bar */
        .minWidth = 400, .minHeight = 400,
    };

    return rcRunApp(&opts);
}
