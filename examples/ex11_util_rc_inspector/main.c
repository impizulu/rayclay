/*
    main.c - RayClay Inspector: browser developer tools for a live RayClay page

    The left pane runs a real page in this same window, and the dark pane beside it
    inspects that page the way Chrome DevTools inspects a tab; in a narrow window the
    tools dock under the page, as DevTools docks to the bottom, and in a short, wide
    one (a phone on its side) they scroll beside it:

      Layout       the element tree, a box model read back with rcGetElementBox, and
                   a picker: hover the page, click an element, land on its declaration
      Performance  a frame chart and a flame chart of this app's own frame, from a
                   small profiler you can copy into any RayClay app
      Memory       resident memory, snapshots you can compare, and a real test
                   allocation that moves the graph
      Console      the app's lines and RayClay's own diagnostics, through rcSetLogSink

    Three patterns are worth copying. console_logf() is a printf-style helper over
    rcFormatV that sends each line to the panel and to the platform log with
    rcLogWrite. prof_begin() and prof_end() time any block of a layout callback with
    rcAppTime. A table that must never scroll sideways gives its lesser columns a
    .priority and its main one a .wMin, so a narrow pane hides the least important
    first: see bottom_up_table().

    Shortcuts follow DevTools: Ctrl/Cmd+Shift+C picks an element, Esc toggles the
    console drawer, Ctrl/Cmd+E starts and stops recording, arrow keys walk the tree.

    The monospaced face is read from examples/assets/fonts, relative to the source
    root, so run this from there.

    Build target: rayclay_ex11_util_rc_inspector. Honors RAYCLAY_MAX_FRAMES.
*/

#include "rayclay.h"
#include "icons/rc_icons_ban.h"
#include "icons/rc_icons_caret_down.h"
#include "icons/rc_icons_caret_right.h"
#include "icons/rc_icons_circle_dot.h"
#include "icons/rc_icons_circle_x.h"
#include "icons/rc_icons_ellipsis_vertical.h"
#include "icons/rc_icons_minus.h"
#include "icons/rc_icons_moon.h"
#include "icons/rc_icons_mouse_pointer_square_dashed.h"
#include "icons/rc_icons_plus.h"
#include "icons/rc_icons_rotate_cw.h"
#include "icons/rc_icons_sun.h"
#include "icons/rc_icons_triangle_alert.h"
#include "icons/rc_icons_x.h"
#include "inspector_log.h"

#define MONO_FONT_PATH "examples/assets/fonts/AdwaitaMono-Regular.ttf"

/* The side-by-side arm needs a page and a panel that both still work. */
#define SIDE_MIN_W      860.0f
#define SIDE_MIN_H      480.0f
#define PAGE_MIN_W      280.0f
#define PANEL_MIN_W     520.0f
#define STACK_MIN_W     354.0f  /* the tools as one scrolling column: the table's 342 + bar lane */
#define HANDLE_W        3.0f
#define NARROW_W        480.0f  /* a phone's width: the strip drops its theme button */
#define PHONE_SIDE_H    440.0f  /* shorter: a phone on its side, laid out as upright */

#define PROF_FRAMES     120
#define PROF_SCOPES     48
#define PROF_STACK      16
#define BUDGET_MS       (1000.0f / 60.0f)
#define METRIC_HIST     120     /* 10 Hz: twelve seconds of FPS and frame work */
#define HEAP_HIST       240     /* 2 Hz: two minutes of resident memory and CPU */
#define MEM_SNAPS       8
#define RETAIN_BYTES    (16 * 1024 * 1024)
#define RETAIN_MAX      8
#define EXPORT_CAP      32768
#define PAGE_NODES_MAX  32
#define ORDER_ROW_H     36      /* an order row's height, which rcVirtualList is told */

typedef enum { TAB_LAYOUT, TAB_PERF, TAB_MEMORY, TAB_CONSOLE, TAB_COUNT } Tab;
typedef enum { DETAIL_SUMMARY, DETAIL_BOTTOM_UP, DETAIL_SCHED, DETAIL_COUNT } DetailTab;
typedef enum { CAT_SCRIPT, CAT_LAYOUT, CAT_PAINT, CAT_IDLE, CAT_COUNT } Category;
typedef enum { SORT_SELF, SORT_TOTAL, SORT_MAX, SORT_CALLS } SortKey;
typedef enum { TARGET_CHECKOUT, TARGET_ORDERS_50, TARGET_ORDERS_500, TARGET_ORDERS_2000,
               TARGET_ORDERS_VIRTUAL, TARGET_COUNT } Target;

/* One timed block. Times are milliseconds from the start of its frame. */
typedef struct {
    const char *name;
    float       startMs, durMs;
    int8_t      parent;
    uint8_t     depth, cat;
} ProfScope;

typedef struct {
    unsigned long index;
    float         workMs;       /* update + layout + the library's solve, draw, present */
    float         idleMs;       /* from this frame's end to the next one's start */
    int           count;
    ProfScope     scope[PROF_SCOPES];
} ProfFrame;

/* One row of the bottom-up table: a scope summed over the recorded frames. */
typedef struct {
    const char *name;
    uint8_t     cat;
    float       selfMs, totalMs, maxMs;
    int         calls;
} ProfRow;

/* One element of a page, as the inspector knows it. An element's measured box comes
   from rcGetElementBox; what was declared is written here beside the page's own code. */
typedef struct {
    uint8_t     depth;
    const char *kind, *id;
    uint16_t    pt, pr, pb, pl, gap;
    const char *decl;
    bool        withCoupon;     /* listed only while the coupon is applied */
    uint8_t     bb;             /* the bottom border in px: border-b */
} PageNode;

typedef struct {
    double t;
    size_t resident, peak, retained;
    int    arenaUsed, arenaCap;
} MemSnapshot;

typedef struct {
    /* Selected panels and the drawer. */
    int   tab, detail;
    bool  dark, drawer, keysOpen;
    bool  sideArm;          /* the last layout drew tools_pane, the one arm with a drawer */
    float pageFrac, drawerFrac, stackFrac, besideFrac;

    /* The page under inspection. */
    int   target;
    int   roast, size, qty, cart;
    bool  coupon;
    double addedAt;       /* the app clock's double: a float's step grows with the session */

    /* The element tree and the picker. */
    int     sel;
    int     treeHover;      /* row under the pointer this frame, or -1 */
    int     pickHover;      /* element under the picker this frame, or -1 */
    bool    picking;
    uint8_t collapsed[PAGE_NODES_MAX];

    /* The profiler: a frame being built, and a ring of finished ones. */
    bool           recording;
    ProfFrame      live;
    int            stack[PROF_STACK];
    int            sp;
    double         frameStart, layoutEnd, lastFrameEnd;
    ProfFrame      ring[PROF_FRAMES];
    int            ringHead, ringLen;
    ProfFrame      shown;           /* the frame the flame chart draws */
    const char    *selScope;        /* a literal, so it matches across frames */
    const char    *zoomScope;       /* the block the flame chart spreads across its width */
    double         shownNext;
    ProfRow        rows[PROF_SCOPES];
    int            rowCount;
    int            sort;
    unsigned long  frameNo;
    float          barMs[PROF_FRAMES], budget[PROF_FRAMES], barX[PROF_FRAMES];
    unsigned long  barFrame[PROF_FRAMES];
    RC_Color       barColor[PROF_FRAMES];
    bool           continuous;

    /* Live metrics. */
    double now, metricNext, heapNext, readoutNext;
    float  fpsHist[METRIC_HIST], workHist[METRIC_HIST];
    float  fpsShown;                /* the FPS readout: last second's mean, once a second */
    int    metricLen;
    float  heapT[HEAP_HIST], heapMb[HEAP_HIST], cpuPct[HEAP_HIST];
    int    heapLen;
    float  cpuNow;
    bool   cpuReads;                /* a CPU reading has come: never on the web */
    RC_CpuSampler cpuSince;         /* rcProcessCpuPercent's interval: zero = none yet */
    int    arenaUsed, arenaCap;
    RC_SchedStats sched;

    /* Memory panel. */
    MemSnapshot snaps[MEM_SNAPS];
    int         snapCount, snapSel;     /* snapSel -1 is the live view */
    RC_Arena    retained[RETAIN_MAX];
    int         retainedCount;

    /* Console. */
    InspectorLog log;
    char          filter[48];
    int           levelFilter;

    char exportBuf[EXPORT_CAP];
    int  exportLen;

    bool   started;
    double budgetWarnedAt;
} AppState;

static uint16_t monoFont;       /* set once, on the first frame */

/* Scopes the code compares by address, so each is one object, whatever a compiler pools. */
static const char SCOPE_FRAME[]  = "Frame";
static const char SCOPE_LAYOUT[] = "Layout";

/* --------------------------------------------------------------------------
   Theme: Chrome DevTools' own palette. The page keeps a light style of its own.
   -------------------------------------------------------------------------- */

/* The colors RC_Style has no field for: the profiler's categories, the element tree's
   markup, the console's levels, the box model and the frame bars. */
typedef struct {
    RC_Color cat[CAT_COUNT];
    RC_Color tag, attr, value;
    RC_Color err, warn, ok, errRow, warnRow, onAccent;
    RC_Color boxBorder, boxPadding, boxContent, boxInk;
    RC_Color selRow;
    RC_Color barOk, barSlow, barJanky;      /* frame bars, apart from the text colors */
} Palette;

static RC_Style app_style(bool dark) {
    RC_Style s = dark ? rcStyleDark() : rcStyleLight();

    if (dark) {
        s.background   = rcHex(0x202124);
        s.surface      = rcHex(0x292a2d);
        s.surfaceAlt   = rcHex(0x35363a);
        s.chrome       = rcHex(0x35363a);
        s.text         = rcHex(0xe8eaed);
        s.textMuted    = rcHex(0x9aa0a6);
        s.border       = rcHex(0x3c4043);
        s.primary      = rcHex(0x8ab4f8);
        s.primaryHover = rcHex(0xaecbfa);
        s.danger       = rcHex(0xf28b82);
        s.dangerHover  = rcHex(0xf6aea9);
        s.warning      = rcHex(0xfdd663);
    } else {
        s.background   = RC_WHITE;
        s.surface      = rcHex(0xf1f3f4);
        s.surfaceAlt   = rcHex(0xe8eaed);
        s.chrome       = rcHex(0xdee1e6);
        s.text         = rcHex(0x202124);
        s.textMuted    = rcHex(0x5f6368);
        s.border       = rcHex(0xdadce0);
        s.primary      = rcHex(0x1a73e8);
        s.primaryHover = rcHex(0x1967d2);
        /* The console's error red (Palette.err), so the record dot and its label are one red.
           Text in it reads 5.2:1 on the toolbar; #d93025 would read 4.3:1, under AA's 4.5. */
        s.danger       = rcHex(0xc5221f);
        s.dangerHover  = rcHex(0xb31412);
        s.warning      = rcHex(0xe37400);
    }
    s.radius = 3.0f;
    return s;
}

static Palette app_palette(bool dark) {
    Palette pal;

    pal.cat[CAT_SCRIPT] = rcHex(0xf2c94c);
    pal.cat[CAT_LAYOUT] = rcHex(0xb18cf0);
    pal.cat[CAT_PAINT]  = rcHex(0x6fc98a);
    pal.cat[CAT_IDLE]   = dark ? rcHex(0x5f6368) : rcHex(0xbdc1c6);
    pal.boxBorder  = rcHex(0xe8c872);
    pal.boxPadding = rcHex(0xb7d7a8);
    pal.boxContent = rcHex(0x8cb6d6);
    pal.boxInk     = rcHex(0x202124);
    pal.barOk      = dark ? rcHex(0x81c995) : rcHex(0x34a853);
    pal.barSlow    = rcHex(0xf9ab00);
    pal.barJanky   = dark ? rcHex(0xf28b82) : rcHex(0xea4335);
    if (dark) {
        pal.tag      = rcHex(0x5db0d7);
        pal.attr     = rcHex(0x9bbbdc);
        pal.value    = rcHex(0xf29766);
        pal.err      = rcHex(0xf28b82);
        pal.warn     = rcHex(0xfdd663);
        pal.ok       = rcHex(0x81c995);
        pal.errRow   = rcWithAlpha(pal.err, 0.118f);
        pal.warnRow  = rcWithAlpha(pal.warn, 0.1f);
        pal.onAccent = rcHex(0x202124);
        pal.selRow   = rcHex(0x174ea6);
    } else {
        pal.tag      = rcHex(0x881280);
        pal.attr     = rcHex(0x994500);
        pal.value    = rcHex(0x1a1aa6);
        pal.err      = rcHex(0xc5221f);
        pal.warn     = rcHex(0x8a4b00);
        pal.ok       = rcHex(0x1e8e3e);
        pal.errRow   = rcWithAlpha(rcHex(0xd93025), 0.086f);
        pal.warnRow  = rcWithAlpha(rcHex(0xfbbc04), 0.157f);
        pal.onAccent = RC_WHITE;
        pal.selRow   = rcHex(0xcfe8fc);
    }
    return pal;
}

/* A control's height, as a class: DevTools' dense 24 px under a mouse, a finger's 44
   otherwise. A class names no pointer, so each pointer's height is a literal of its own. */
static const char *ctl_h(void) {
    return rcPointerIsCoarse() ? "h-11" : "h-6";
}

/* A small button's: 20 px under a mouse. */
static const char *small_btn_h(void) {
    return rcPointerIsCoarse() ? "h-11" : "h-5";
}

static uint16_t row_h(void) {
    return rcPointerIsCoarse() ? 44 : 22;
}

/* --------------------------------------------------------------------------
   The console's two entry points
   -------------------------------------------------------------------------- */

/* Every app line goes to the panel and to the platform's log, so a developer running
   from a terminal sees what the panel shows. */
static void console_logf(RC_App *app, AppState *st, RC_LogLevel level, const char *format, ...)
    RC_PRINTF_FMT(4, 5);

static void console_logf(RC_App *app, AppState *st, RC_LogLevel level, const char *format, ...) {
    va_list args;

    va_start(args, format);
    RC_String line = rcFormatV(rcAppArena(app), format, args);
    va_end(args);
    insp_log_push(&st->log, INSP_SRC_APP, (int)level, st->now, line.chars);
    rcLogWrite(level, "inspector", line.chars);
}

/* rcSetLogSink target: `msg` is valid for this call only and the ring copies it. A sink
   replaces the platform log, so rcLogWrite passes the line on to keep it visible there.
   The sink runs on the thread that logged. This app logs from the main thread only; an app
   whose worker threads log (rcFetch, media) hands lines to its panel through a lock. */
static void on_log(RC_LogLevel level, const char *msg, void *user) {
    AppState *st = (AppState *)user;

    insp_log_push(&st->log, INSP_SRC_RAY, (int)level, st->now, msg);
    rcLogWrite(level, NULL, msg);
}

/* --------------------------------------------------------------------------
   The profiler. prof_begin and prof_end pair around any block of code, and each
   frame's scopes form a tree: copy these two functions to measure your own app.
   -------------------------------------------------------------------------- */

static void prof_begin(const RC_App *app, AppState *st, const char *name, int cat) {
    ProfFrame *f = &st->live;
    int        i = f->count < PROF_SCOPES ? f->count++ : -1;    /* full: drop it, keep the pairs */

    if (i >= 0) {
        ProfScope *sc = &f->scope[i];

        sc->name    = name;
        sc->startMs = (float)((rcAppTime(app) - st->frameStart) * 1000.0);
        sc->durMs   = 0.0f;
        sc->parent  = (int8_t)(st->sp > 0 ? st->stack[st->sp - 1] : -1);
        sc->depth   = (uint8_t)st->sp;
        sc->cat     = (uint8_t)cat;
    }
    if (st->sp < PROF_STACK)
        st->stack[st->sp++] = i;
}

static void prof_end(const RC_App *app, AppState *st) {
    if (st->sp > 0) {
        int i = st->stack[--st->sp];

        if (i >= 0) {
            ProfScope *sc = &st->live.scope[i];

            sc->durMs = (float)((rcAppTime(app) - st->frameStart) * 1000.0) - sc->startMs;
        }
    }
}

/* The i-th recorded frame, oldest first. */
static const ProfFrame *ring_at(const AppState *st, int i) {
    int oldest = (st->ringHead - st->ringLen + PROF_FRAMES) % PROF_FRAMES;

    return &st->ring[(oldest + i) % PROF_FRAMES];
}

static void rows_sort(AppState *st) {
    for (int i = 1; i < st->rowCount; i++) {
        ProfRow row = st->rows[i];
        float   key = st->sort == SORT_TOTAL ? row.totalMs
                    : st->sort == SORT_MAX   ? row.maxMs
                    : st->sort == SORT_CALLS ? (float)row.calls
                    :                          row.selfMs;
        int     j   = i - 1;

        while (j >= 0) {
            const ProfRow *o = &st->rows[j];
            float other = st->sort == SORT_TOTAL ? o->totalMs
                        : st->sort == SORT_MAX   ? o->maxMs
                        : st->sort == SORT_CALLS ? (float)o->calls
                        :                          o->selfMs;
            if (other >= key)
                break;
            st->rows[j + 1] = st->rows[j];
            j--;
        }
        st->rows[j + 1] = row;
    }
}

/* Sums every recorded frame by scope name. A name is a literal, so a pointer compare is
   enough. Self time is what is left of a scope after its children. */
static void rows_rebuild(AppState *st) {
    st->rowCount = 0;
    for (int n = 0; n < st->ringLen; n++) {
        const ProfFrame *f = ring_at(st, n);

        for (int i = 0; i < f->count; i++) {
            const ProfScope *sc = &f->scope[i];
            float self = sc->durMs;
            int   r    = 0;

            for (int c = i + 1; c < f->count; c++) {
                if (f->scope[c].parent == i)
                    self -= f->scope[c].durMs;
            }
            while (r < st->rowCount && st->rows[r].name != sc->name)
                r++;
            if (r == st->rowCount) {
                if (r == PROF_SCOPES)
                    continue;
                st->rows[r].name    = sc->name;
                st->rows[r].cat     = sc->cat;
                st->rows[r].selfMs  = 0.0f;
                st->rows[r].totalMs = 0.0f;
                st->rows[r].maxMs   = 0.0f;
                st->rows[r].calls   = 0;
                st->rowCount++;
            }
            st->rows[r].selfMs  += self;
            st->rows[r].totalMs += sc->durMs;
            st->rows[r].calls++;
            if (sc->durMs > st->rows[r].maxMs)
                st->rows[r].maxMs = sc->durMs;
        }
    }
    rows_sort(st);
}

/* frameEndCallback fires only on a frame that drew, so this is where a frame's work
   ends: what happened since layout() returned is the library solving, drawing and, if
   the picture changed, presenting. */
static void frame_end(RC_App *app, void *userData) {
    AppState  *st  = (AppState *)userData;
    ProfFrame *f   = &st->live;
    double     now = rcAppTime(app);

    if (f->count == 0)      /* a repaint outside update and layout: no frame to close */
        return;
    if (f->count < PROF_SCOPES) {
        ProfScope *sc = &f->scope[f->count++];

        sc->name    = "Solve, draw, present";
        sc->startMs = (float)((st->layoutEnd - st->frameStart) * 1000.0);
        sc->durMs   = (float)((now - st->layoutEnd) * 1000.0);
        sc->parent  = 0;
        sc->depth   = 1;
        sc->cat     = CAT_PAINT;
    }
    f->scope[0].durMs = (float)((now - st->frameStart) * 1000.0);
    f->workMs = f->scope[0].durMs;
    st->lastFrameEnd = now;
    if (st->recording) {
        st->ring[st->ringHead] = *f;
        st->ringHead = (st->ringHead + 1) % PROF_FRAMES;
        if (st->ringLen < PROF_FRAMES)
            st->ringLen++;
    }
    f->count = 0;
}

/* Called at the top of update(): the gap since the last frame ended is the idle time of
   the frame the ring holds last, and a new frame's root scope opens. */
static void frame_begin(RC_App *app, AppState *st) {
    double now = rcAppTime(app);

    if (st->recording && st->ringLen > 0 && st->lastFrameEnd > 0.0) {
        ProfFrame *prev = &st->ring[(st->ringHead - 1 + PROF_FRAMES) % PROF_FRAMES];

        prev->idleMs = (float)((now - st->lastFrameEnd) * 1000.0);
    }
    st->live.count = 0;
    st->live.index = ++st->frameNo;
    st->live.idleMs = 0.0f;
    st->sp = 0;
    st->frameStart = now;
    prof_begin(app, st, SCOPE_FRAME, CAT_SCRIPT);
}

/* The flame chart and its numbers hold still while you read them: one update a second while
   recording, as a browser's FPS meter does. */
static void shown_refresh(AppState *st) {
    if (st->recording && st->ringLen > 1 && st->now >= st->shownNext) {
        /* The newest whole frame has its idle time filled in only from the next frame on. */
        st->shown = *ring_at(st, st->ringLen - 2);
        rows_rebuild(st);
        st->shownNext = st->now + 1.0;
    }
}

/* Appends one whole line, or nothing once the buffer is full. */
static bool export_add(AppState *st, RC_String line) {
    if (st->exportLen + line.length >= EXPORT_CAP)
        return false;
    rcStrCopy(st->exportBuf + st->exportLen, line.chars,
              (size_t)(EXPORT_CAP - st->exportLen));
    st->exportLen += line.length;
    return true;
}

static void trace_export(RC_App *app, AppState *st) {
    RC_Arena *mem = rcAppArena(app);

    st->exportLen = 0;
    st->exportBuf[0] = '\0';
    for (int i = -1; i < st->ringLen; i++) {
        RC_String line = i < 0 ? rcFormat(mem, "frame,work_ms,idle_ms\n")
                               : rcFormat(mem, "%lu,%.3f,%.3f\n", ring_at(st, i)->index,
                                          ring_at(st, i)->workMs, ring_at(st, i)->idleMs);
        if (!export_add(st, line))
            break;
    }
    rcClipboardSet(st->exportBuf);
    console_logf(app, st, RC_LOG_INFO, "copied %d frames of trace, %d bytes, as CSV",
                 st->ringLen, st->exportLen);
}

/* --------------------------------------------------------------------------
   Shared toolbar pieces
   -------------------------------------------------------------------------- */

/* A tool's ink: muted, the accent while its tool is on, or red while it records. */
typedef enum { INK_MUTED, INK_ON, INK_RECORDING, INK_COUNT } ToolInk;

/* An icon button's class: its 12 px glyph padded out to DevTools' 28 x 24 px toolbar button,
   or to a finger's 44 square, in its ink. The "more" menu takes the muted one, so it matches
   the buttons by it. */
static const char *tool_class(ToolInk ink) {
    static const char *const MOUSE[INK_COUNT] = {
        "h-6 px-2 text-muted-foreground", "h-6 px-2 text-primary", "h-6 px-2 text-danger",
    };
    static const char *const FINGER[INK_COUNT] = {
        "h-11 px-4 text-muted-foreground", "h-11 px-4 text-primary", "h-11 px-4 text-danger",
    };

    return (rcPointerIsCoarse() ? FINGER : MOUSE)[ink];
}

/* An icon-only ghost button; the box around it carries the tooltip. */
static bool tool_icon(const char *id, RC_IconCallback icon, ToolInk ink, const char *tip) {
    bool clicked = false;

    rcBox(.tooltip = tip) {
        clicked = rcButton(id, "", .className = tool_class(ink), .variant = RC_BTN_GHOST,
                           .leadingIcon = icon);
    }
    return clicked;
}

static void tool_divider(void) {
    rcBox(.className = "bg-border w-px h-4") {}
}

/* An underlined tab, in the strip or in a panel's lower pane. It is a Tab stop as tall as the
   strip that scrolls it, which would clip a ring drawn around it, so ring-inset draws its
   focus ring inside it, as on every other box here that takes Tab. */
static bool tab_label(const char *id, const char *label, bool on, uint16_t size) {
    RC_Style s   = rcGetStyle();
    bool     hot = rcShowsHover(id);

    rcColumn(.id = id, .focusable = true,
             .hType = RC_PX(rcPointerIsCoarse() ? 44.0f : size > 12 ? 30.0f : 26.0f),
             .className = on ? "ring-inset" : "ring-inset hover:bg-surface-alt") {
        rcRow(.px = (float)(rcViewport().width < NARROW_W ? 8 : 11), .align = "cc",
              .h = "grow") {
            /* A text run has no hover of its own: the label reads its tab's. */
            rcTextC(label, .color = on ? s.primary : hot ? s.text : s.textMuted, .size = size,
                    .wrap = "n", .select = RC_SELECT_NONE);
        }
        rcBox(.bg = on ? s.primary : rcWithAlpha(s.primary, 0), .w = "grow", .h = "2px") {}
    }
    return rcClicked(id);
}

/* Chart readouts at a set precision while the ticks keep their round steps. `user` is
   the frame arena, handed over as the axis's .formatUserData. */
static int readout_tenths(double v, char *out, int cap, void *user) {
    RC_String t = rcFormat((RC_Arena *)user, "%.1f", v);

    rcStrCopy(out, t.chars, (size_t)cap);
    return (int)t.length;
}

static int readout_hundredths(double v, char *out, int cap, void *user) {
    RC_String t = rcFormat((RC_Arena *)user, "%.2f", v);

    rcStrCopy(out, t.chars, (size_t)cap);
    return (int)t.length;
}

static void mono_num(RC_String text, RC_Color ink) {
    rcText(text, .font = monoFont, .color = ink, .wrap = "n");
}

/* --------------------------------------------------------------------------
   The page: ordinary RayClay code, with a table of what it declares
   -------------------------------------------------------------------------- */

#define PAGE_BG    rcHex(0xf7f4ef)
#define PAGE_CARD  RC_WHITE
#define PAGE_INK   rcHex(0x2b211a)
#define PAGE_MUTED rcHex(0x75675b)
#define PAGE_BRAND rcHex(0x7a3e16)

/* Each declaration is its call in page_checkout or order_row, one option a line, so it
   reads whole in a narrow sidebar. */
static const PageNode CHECKOUT_NODES[] = {
    { 0, "rcColumn", "page", 0, 0, 0, 0, 0,
      "rcColumn(.id = \"page\",\n"
      "    .bg = PAGE_BG,\n"
      "    .w = \"grow\",\n"
      "    .h = \"grow\")", false, 0 },
    { 1, "rcRow", "nav", 12, 16, 12, 16, 8,
      "rcRow(.id = \"nav\",\n"
      "    .bg = PAGE_CARD,\n"
      "    .gap = 8,\n"
      "    .px = 16,\n"
      "    .py = 12,\n"
      "    .align = \"cl\",\n"
      "    .w = \"grow\",\n"
      "    .className = \"border-b\")", false, 1 },
    { 2, "rcBox", "brand", 0, 0, 0, 0, 0,
      "rcBox(.id = \"brand\")", false, 0 },
    { 2, "rcRow", "cart_chip", 4, 10, 4, 10, 0,
      "rcRow(.id = \"cart_chip\",\n"
      "    .bg = PAGE_BRAND,\n"
      "    .px = 10,\n"
      "    .py = 4,\n"
      "    .align = \"cc\",\n"
      "    .borderRadius = \"all-full\")", false, 0 },
    { 1, "rcColumn", "page_body", 16, 16, 16, 16, 12,
      "rcColumn(.id = \"page_body\",\n"
      "    .gap = 12,\n"
      "    .p = 16,\n"
      "    .scroll = \"v\",\n"
      "    .w = \"grow\",\n"
      "    .h = \"grow\")", false, 0 },
    { 2, "rcColumn", "hero", 16, 16, 16, 16, 4,
      "rcColumn(.id = \"hero\",\n"
      "    .gap = 4,\n"
      "    .p = 16,\n"
      "    .align = \"bl\",\n"
      "    .borderRadius = \"all-md\",\n"
      "    .w = \"grow\",\n"
      "    .h = \"132px\",\n"
      "    .gradient = {\n"
      "        .from = rcHex(0x8a4b1f),\n"
      "        .to = rcHex(0x2f1a0e),\n"
      "        .dir = \"v\" })", false, 0 },
    { 2, "rcColumn", "options", 12, 12, 12, 12, 12,
      "rcColumn(.id = \"options\",\n"
      "    .bg = PAGE_CARD,\n"
      "    .gap = 12,\n"
      "    .p = 12,\n"
      "    .borderRadius = \"all-md\",\n"
      "    .w = \"grow\")", false, 0 },
    { 3, "rcRow", "roast_row", 0, 0, 0, 0, 16,
      "rcRow(.id = \"roast_row\",\n"
      "    .gap = 16,\n"
      "    .align = \"cl\",\n"
      "    .w = \"grow\")", false, 0 },
    { 4, "rcRadio", "roast_0", 0, 0, 0, 0, 0,
      "rcRadio(\"roast_0\", \"Light\",\n"
      "    &st->roast, 0)", false, 0 },
    { 4, "rcRadio", "roast_1", 0, 0, 0, 0, 0,
      "rcRadio(\"roast_1\", \"Medium\",\n"
      "    &st->roast, 1)", false, 0 },
    { 4, "rcRadio", "roast_2", 0, 0, 0, 0, 0,
      "rcRadio(\"roast_2\", \"Dark\",\n"
      "    &st->roast, 2)", false, 0 },
    { 3, "rcRow", "size_row", 0, 0, 0, 0, 8,
      "rcRow(.id = \"size_row\",\n"
      "    .gap = 8,\n"
      "    .align = \"cl\",\n"
      "    .w = \"grow\")", false, 0 },
    { 4, "rcCombo", "size_combo", 0, 0, 0, 0, 0,
      "rcCombo(\"size_combo\", &st->size,\n"
      "    SIZES, 3,\n"
      "    .className = page_ctl_h())", false, 0 },
    { 3, "rcRow", "qty_row", 0, 0, 0, 0, 8,
      "rcRow(.id = \"qty_row\",\n"
      "    .gap = 8,\n"
      "    .align = \"cl\",\n"
      "    .w = \"grow\")", false, 0 },
    { 4, "rcButton", "qty_minus", 0, 0, 0, 0, 0,
      "rcButton(\"qty_minus\", \"\",\n"
      "    .disabled = st->qty <= 1,\n"
      "    .className = page_ctl_h(),\n"
      "    .leadingIcon = rcIconMinus)", false, 0 },
    { 4, "rcBox", "qty_value", 0, 0, 0, 0, 0,
      "rcBox(.id = \"qty_value\",\n"
      "    .align = \"cc\",\n"
      "    .w = \"28px\")", false, 0 },
    { 4, "rcButton", "qty_plus", 0, 0, 0, 0, 0,
      "rcButton(\"qty_plus\", \"\",\n"
      "    .disabled = st->qty >= 9,\n"
      "    .className = page_ctl_h(),\n"
      "    .leadingIcon = rcIconPlus)", false, 0 },
    { 2, "rcColumn", "summary", 12, 12, 12, 12, 6,
      "rcColumn(.id = \"summary\",\n"
      "    .bg = PAGE_CARD,\n"
      "    .gap = 6,\n"
      "    .p = 12,\n"
      "    .borderRadius = \"all-md\",\n"
      "    .w = \"grow\")", false, 0 },
    { 3, "rcRow", "sum_sub", 0, 0, 0, 0, 0,
      "summary_row(app, \"sum_sub\",\n"
      "    \"Subtotal\",\n"
      "    subtotal, false)", false, 0 },
    { 3, "rcRow", "sum_disc", 0, 0, 0, 0, 0,
      "summary_row(app, \"sum_disc\",\n"
      "    \"Coupon BEANS10\",\n"
      "    -discount, false)", true, 0 },
    { 3, "rcRow", "sum_ship", 0, 0, 0, 0, 0,
      "summary_row(app, \"sum_ship\",\n"
      "    \"Shipping\",\n"
      "    shipping, false)", false, 0 },
    { 3, "rcRow", "sum_tax", 0, 0, 0, 0, 0,
      "summary_row(app, \"sum_tax\",\n"
      "    \"Tax (8%)\",\n"
      "    tax, false)", false, 0 },
    { 3, "rcRow", "sum_total", 0, 0, 0, 0, 0,
      "summary_row(app, \"sum_total\",\n"
      "    \"Total\",\n"
      "    total, true)", false, 0 },
    { 2, "rcRow", "coupon", 8, 12, 8, 12, 8,
      "rcRow(.id = \"coupon\",\n"
      "    .bg = PAGE_CARD,\n"
      "    .gap = 8,\n"
      "    .px = 12,\n"
      "    .py = 8,\n"
      "    .align = \"cl\",\n"
      "    .borderRadius = \"all-md\",\n"
      "    .w = \"grow\")", false, 0 },
    { 3, "rcButton", "coupon_apply", 0, 0, 0, 0, 0,
      "rcButton(\"coupon_apply\",\n"
      "    st->coupon ? \"Remove\"\n"
      "        : \"Apply\",\n"
      "    .className = page_ctl_h())", false, 0 },
    { 2, "rcButton", "buy", 0, 0, 0, 0, 0,
      "rcButton(\"buy\", buy,\n"
      "    .className = rcPointerIsCoarse()\n"
      "        ? \"w-full h-11\"\n"
      "        : \"w-full h-[34px]\",\n"
      "    .variant = RC_BTN_PRIMARY)", false, 0 },
    { 2, "rcColumn", "details", 12, 12, 12, 12, 6,
      "rcColumn(.id = \"details\",\n"
      "    .bg = PAGE_CARD,\n"
      "    .gap = 6,\n"
      "    .p = 12,\n"
      "    .borderRadius = \"all-md\",\n"
      "    .w = \"grow\")", false, 0 },
};

static const PageNode ORDER_NODES[] = {
    { 0, "rcColumn", "orders", 0, 0, 0, 0, 0,
      "rcColumn(.id = \"orders\",\n"
      "    .bg = PAGE_CARD,\n"
      "    .w = \"grow\",\n"
      "    .h = \"grow\")", false, 0 },
    { 1, "rcRow", "ord_bar", 12, 16, 12, 16, 8,
      "rcRow(.id = \"ord_bar\",\n"
      "    .gap = 8,\n"
      "    .px = 16,\n"
      "    .py = 12,\n"
      "    .align = \"cl\",\n"
      "    .w = \"grow\")", false, 0 },
    { 2, "rcBox", "ord_title", 0, 0, 0, 0, 0,
      "rcBox(.id = \"ord_title\")", false, 0 },
    { 2, "rcBox", "ord_count", 0, 0, 0, 0, 0,
      "rcBox(.id = \"ord_count\")", false, 0 },
    { 1, "rcRow", "ord_head", 6, 16, 6, 16, 8,
      "rcRow(.id = \"ord_head\",\n"
      "    .bg = PAGE_BG,\n"
      "    .gap = 8,\n"
      "    .px = 16,\n"
      "    .py = 6,\n"
      "    .w = \"grow\")", false, 0 },
    { 1, "rcColumn", "ord_list", 0, 0, 0, 0, 0,
      "rcColumn(.id = \"ord_list\",\n"
      "    .scroll = \"v\",\n"
      "    .w = \"grow\",\n"
      "    .h = \"grow\")", false, 0 },
    { 2, "rcRow", "ord0", 0, 16, 0, 16, 8,
      "rcRow(.id = id,\n"
      "    .gap = 8,\n"
      "    .px = 16,\n"
      "    .align = \"cl\",\n"
      "    .w = \"grow\",\n"
      "    .hType = RC_PX(ORDER_ROW_H),\n"
      "    .className = \"border-b \"\n"
      "        \"hover:bg-[#f3efe8]\")", false, 1 },
    { 2, "rcRow", "ord1", 0, 16, 0, 16, 8,
      "rcRow(.id = id,\n"
      "    .gap = 8,\n"
      "    .px = 16,\n"
      "    .align = \"cl\",\n"
      "    .w = \"grow\",\n"
      "    .hType = RC_PX(ORDER_ROW_H),\n"
      "    .className = \"border-b \"\n"
      "        \"hover:bg-[#f3efe8]\")", false, 1 },
    { 2, "rcRow", "ord2", 0, 16, 0, 16, 8,
      "rcRow(.id = id,\n"
      "    .gap = 8,\n"
      "    .px = 16,\n"
      "    .align = \"cl\",\n"
      "    .w = \"grow\",\n"
      "    .hType = RC_PX(ORDER_ROW_H),\n"
      "    .className = \"border-b \"\n"
      "        \"hover:bg-[#f3efe8]\")", false, 1 },
};

static const char *const TARGET_NAME[TARGET_COUNT] = {
    "Product page",
    "Orders: 50 rows",
    "Orders: 500 rows",
    "Orders: 2,000 rows",
    "Orders: 2,000, virtual list",
};

static const PageNode *page_nodes(const AppState *st, int *count) {
    if (st->target == TARGET_CHECKOUT) {
        *count = (int)(sizeof CHECKOUT_NODES / sizeof CHECKOUT_NODES[0]);
        return CHECKOUT_NODES;
    }
    *count = (int)(sizeof ORDER_NODES / sizeof ORDER_NODES[0]);
    return ORDER_NODES;
}

static void select_target(RC_App *app, AppState *st) {
    st->sel = 0;
    for (int i = 0; i < PAGE_NODES_MAX; i++)
        st->collapsed[i] = 0;
    st->picking = false;
    console_logf(app, st, RC_LOG_INFO, "target: %s", TARGET_NAME[st->target]);
}

/* The page's controls: a storefront's 34 px under a mouse, a finger's 44 otherwise. */
static const char *page_ctl_h(void) {
    return rcPointerIsCoarse() ? "h-11" : "h-[34px]";
}

static void summary_row(RC_App *app, const char *id, const char *label, int cents, bool strong) {
    int       a = cents < 0 ? -cents : cents;
    RC_String v = rcFormat(rcAppArena(app), "%s$%d.%02d", cents < 0 ? "-" : "", a / 100, a % 100);

    rcRow(.id = id, .align = "cl", .w = "grow") {
        rcTextC(label, .font = (uint16_t)(strong ? rcFontSemibold(15.0f) : 0),
                .color = strong ? PAGE_INK : PAGE_MUTED, .size = (uint16_t)(strong ? 15 : 14),
                .wrap = "n");
        rcSpacer() {}
        rcText(v, .font = (uint16_t)(strong ? rcFontSemibold(15.0f) : 0), .color = PAGE_INK,
               .size = (uint16_t)(strong ? 15 : 14), .wrap = "n");
    }
}

static void detail_row(const char *label, const char *value) {
    rcRow(.gap = 8, .w = "grow") {
        rcBox(.w = "72px") { rcTextC(label, .color = PAGE_MUTED, .size = 13, .wrap = "n"); }
        rcBox(.w = "grow") { rcTextC(value, .color = PAGE_INK, .size = 13); }
    }
}

static void page_checkout(RC_App *app, AppState *st) {
    static const char *const SIZES[3]  = { "250 g bag", "500 g bag", "1 kg bag" };
    static const int         PRICE[3]  = { 1850, 3400, 6200 };
    RC_Arena *mem      = rcAppArena(app);
    int       subtotal = PRICE[st->size] * st->qty;
    int       discount = st->coupon ? subtotal / 10 : 0;
    int       shipping = subtotal - discount >= 4000 ? 0 : 490;
    int       tax      = ((subtotal - discount) * 8 + 50) / 100;
    int       total    = subtotal - discount + shipping + tax;

    rcColumn(.id = "page", .bg = PAGE_BG, .w = "grow", .h = "grow") {
        prof_begin(app, st, "page.nav", CAT_LAYOUT);
        rcRow(.id = "nav", .bg = PAGE_CARD, .gap = 8, .px = 16, .py = 12, .align = "cl",
              .w = "grow", .className = "border-b") {
            rcBox(.id = "brand") {
                rcTextL("Kestrel Coffee", .font = rcFontSemibold(16.0f), .color = PAGE_BRAND,
                        .wrap = "n");
            }
            rcSpacer() {}
            rcRow(.id = "cart_chip", .bg = PAGE_BRAND, .px = 10, .py = 4, .align = "cc",
                  .borderRadius = "all-full") {
                rcText(rcFormat(mem, "Cart %d", st->cart), .color = RC_WHITE, .wrap = "n");
            }
        }
        prof_end(app, st);

        rcColumn(.id = "page_body", .gap = 12, .p = 16, .scroll = "v", .w = "grow", .h = "grow") {
            prof_begin(app, st, "page.hero", CAT_LAYOUT);
            rcColumn(.id = "hero", .gap = 4, .p = 16, .align = "bl", .borderRadius = "all-md",
                     .w = "grow", .h = "132px",
                     .gradient = { .from = rcHex(0x8a4b1f), .to = rcHex(0x2f1a0e), .dir = "v" }) {
                rcTextL("SINGLE ORIGIN", .color = rcHex(0xe9c9a8), .size = 11, .wrap = "n");
                rcTextL("Ethiopia Yirgacheffe", .font = rcFontSemibold(20.0f), .color = RC_WHITE);
                rcTextL("Washed. Jasmine, bergamot, honey.", .color = rcHex(0xf0dfcd), .size = 13);
            }
            prof_end(app, st);

            prof_begin(app, st, "page.options", CAT_LAYOUT);
            rcColumn(.id = "options", .bg = PAGE_CARD, .gap = 12, .p = 12,
                     .borderRadius = "all-md", .w = "grow") {
                rcRow(.id = "roast_row", .gap = 16, .align = "cl", .w = "grow") {
                    rcRadio("roast_0", "Light", &st->roast, 0);
                    rcRadio("roast_1", "Medium", &st->roast, 1);
                    rcRadio("roast_2", "Dark", &st->roast, 2);
                }
                rcRow(.id = "size_row", .gap = 8, .align = "cl", .w = "grow") {
                    rcBox(.w = "grow") {
                        rcCombo("size_combo", &st->size, SIZES, 3, .className = page_ctl_h());
                    }
                }
                rcRow(.id = "qty_row", .gap = 8, .align = "cl", .w = "grow") {
                    rcTextL("Quantity", .color = PAGE_MUTED, .size = 14, .wrap = "n");
                    rcSpacer() {}
                    /* Icons rather than "-" and "+": the two characters differ in width and
                       the two icons do not, so the pair of buttons match. */
                    if (rcButton("qty_minus", "", .disabled = st->qty <= 1,
                                 .className = page_ctl_h(), .leadingIcon = rcIconMinus))
                        st->qty--;
                    rcBox(.id = "qty_value", .align = "cc", .w = "28px") {
                        rcText(rcFormat(mem, "%d", st->qty), .color = PAGE_INK, .size = 15,
                               .wrap = "n");
                    }
                    if (rcButton("qty_plus", "", .disabled = st->qty >= 9,
                                 .className = page_ctl_h(), .leadingIcon = rcIconPlus))
                        st->qty++;
                }
            }
            prof_end(app, st);

            prof_begin(app, st, "page.summary", CAT_LAYOUT);
            rcColumn(.id = "summary", .bg = PAGE_CARD, .gap = 6, .p = 12,
                     .borderRadius = "all-md", .w = "grow") {
                summary_row(app, "sum_sub", "Subtotal", subtotal, false);
                if (discount)
                    summary_row(app, "sum_disc", "Coupon BEANS10", -discount, false);
                summary_row(app, "sum_ship", "Shipping", shipping, false);
                summary_row(app, "sum_tax", "Tax (8%)", tax, false);
                summary_row(app, "sum_total", "Total", total, true);
            }
            prof_end(app, st);

            /* The shop's coupon, offered under the total as storefronts do. */
            rcRow(.id = "coupon", .bg = PAGE_CARD, .gap = 8, .px = 12, .py = 8, .align = "cl",
                  .borderRadius = "all-md", .w = "grow") {
                rcBox(.w = "grow") {
                    rcTextC(st->coupon ? "BEANS10 applied: 10% off" : "Save 10% with BEANS10",
                            .color = st->coupon ? rcHex(0x2e7d32) : PAGE_INK, .size = 14);
                }
                if (rcButton("coupon_apply", st->coupon ? "Remove" : "Apply",
                             .className = page_ctl_h())) {
                    st->coupon = !st->coupon;
                    console_logf(app, st, RC_LOG_INFO, "coupon BEANS10 %s",
                                 st->coupon ? "applied: 10% off" : "removed");
                }
            }

            const char *buy = rcFormat(mem, "Add to cart - $%d.%02d", total / 100,
                                       total % 100).chars;

            /* The page's one call to action spans the column, as a store's does, as tall as
               the page's other controls. */
            if (rcButton("buy", buy,
                         .className = rcPointerIsCoarse() ? "w-full h-11" : "w-full h-[34px]",
                         .variant = RC_BTN_PRIMARY)) {
                st->cart += st->qty;
                st->addedAt = st->now;
                console_logf(app, st, RC_LOG_INFO, "cart: added %d x %s, total $%d.%02d",
                             st->qty, SIZES[st->size], total / 100, total % 100);
            }
            if (st->addedAt > 0.0 && st->now - st->addedAt < 2.5)
                rcTextL("Added to your cart.", .color = rcHex(0x2e7d32), .size = 13);

            rcColumn(.id = "details", .bg = PAGE_CARD, .gap = 6, .p = 12,
                     .borderRadius = "all-md", .w = "grow") {
                detail_row("Origin", "Gedeo Zone, Ethiopia");
                detail_row("Altitude", "1,900 to 2,200 m");
                detail_row("Roasted", "Every Tuesday in small batches");
                detail_row("Shipping", "Free over $40, next business day");
            }
        }
    }
}

/* One order, from nothing but its index, so a run is the same every time and the row count
   is the only thing that changes the cost. */
static void order_row(RC_App *app, int i) {
    static const char *const FIRST[8] = { "Amara", "Bruno", "Chen", "Dalia", "Emil", "Farah",
                                          "Goran", "Hana" };
    static const char *const LAST[8]  = { "Okafor", "Silva", "Wei", "Haddad", "Larsen", "Khan",
                                          "Petrov", "Sato" };
    static const char *const STATE[4] = { "Paid", "Packed", "Shipped", "Refunded" };
    RC_Arena   *mem    = rcAppArena(app);
    unsigned    h      = (unsigned)i * 2654435761u;
    int         cents  = 1200 + (int)((h >> 8) % 18000);
    int         state  = (int)((h >> 20) % 4);
    const char *id     = rcFormat(mem, "ord%d", i).chars;

    rcRow(.id = id, .gap = 8, .px = 16, .align = "cl", .w = "grow", .hType = RC_PX(ORDER_ROW_H),
          .className = "border-b hover:bg-[#f3efe8]") {
        rcBox(.w = "56px") {
            rcText(rcFormat(mem, "#%d", 4100 + i), .font = monoFont, .color = PAGE_MUTED,
                   .wrap = "n");
        }
        rcBox(.overflow = "hidden", .w = "grow") {
            rcText(rcFormat(mem, "%s %s", FIRST[h >> 29], LAST[(h >> 26) & 7]), .color = PAGE_INK,
                   .size = 13, .wrap = "n");
        }
        rcBox(.align = "cr", .w = "64px") {
            rcText(rcFormat(mem, "$%d.%02d", cents / 100, cents % 100), .font = monoFont,
                   .color = PAGE_INK, .wrap = "n");
        }
        rcBox(.align = "cr", .w = "64px") {
            rcTextC(STATE[state], .color = state == 3 ? rcHex(0xb3261e) : PAGE_MUTED, .wrap = "n");
        }
    }
}

static void page_orders(RC_App *app, AppState *st) {
    int rows = st->target == TARGET_ORDERS_50  ? 50
             : st->target == TARGET_ORDERS_500 ? 500
             :                                   2000;

    rcColumn(.id = "orders", .bg = PAGE_CARD, .w = "grow", .h = "grow") {
        rcRow(.id = "ord_bar", .gap = 8, .px = 16, .py = 12, .align = "cl", .w = "grow") {
            rcBox(.id = "ord_title") {
                rcTextL("Orders", .font = rcFontSemibold(16.0f), .color = PAGE_INK, .wrap = "n");
            }
            rcBox(.id = "ord_count") {
                char n[16];

                rcFormatIntTo(n, sizeof n, rows, ",");
                rcText(rcFormat(rcAppArena(app), "%s rows", n), .color = PAGE_MUTED, .wrap = "n");
            }
        }
        rcRow(.id = "ord_head", .bg = PAGE_BG, .gap = 8, .px = 16, .py = 6, .w = "grow") {
            rcBox(.w = "56px") { rcTextL("Order", .color = PAGE_MUTED, .wrap = "n"); }
            rcBox(.w = "grow") { rcTextL("Customer", .color = PAGE_MUTED, .wrap = "n"); }
            rcBox(.align = "cr", .w = "64px") {
                rcTextL("Total", .color = PAGE_MUTED, .wrap = "n");
            }
            rcBox(.align = "cr", .w = "64px") {
                rcTextL("Status", .color = PAGE_MUTED, .wrap = "n");
            }
        }
        prof_begin(app, st, "page.rows", CAT_LAYOUT);
        rcColumn(.id = "ord_list", .scroll = "v", .w = "grow", .h = "grow") {
            if (st->target == TARGET_ORDERS_VIRTUAL) {
                rcVirtualList(row, "ord_list", rows, ORDER_ROW_H) {
                    order_row(app, row.index);
                }
            } else {
                for (int i = 0; i < rows; i++)
                    order_row(app, i);
            }
        }
        prof_end(app, st);
    }
}

/* DevTools' highlight: the element tinted and a chip naming it. Attached to the element by
   id, so it scrolls and clips with it, and it passes the pointer through. */
static void page_highlight(RC_App *app, const PageNode *n) {
    RC_Box b = rcGetElementBox(n->id);
    RC_Box v = rcGetElementBox("page_view");

    if (!b.found)
        return;

    /* The chip stays over the page: on the element's far side from the pane's edge, and inside
       an element that fills the pane. */
    bool      right  = b.x + b.width / 2.0f > v.x + v.width / 2.0f;
    bool      above  = b.y + b.height + 32.0f > v.y + v.height;
    bool      inside = above && b.y - 32.0f < v.y;
    RC_Anchor at     = inside  ? RC_ANCHOR_TOP_LEFT
                     : above   ? (right ? RC_ANCHOR_TOP_RIGHT : RC_ANCHOR_TOP_LEFT)
                     :           (right ? RC_ANCHOR_BOTTOM_RIGHT : RC_ANCHOR_BOTTOM_LEFT);
    RC_Anchor corner = inside  ? RC_ANCHOR_TOP_LEFT
                     : above   ? (right ? RC_ANCHOR_BOTTOM_RIGHT : RC_ANCHOR_BOTTOM_LEFT)
                     :           (right ? RC_ANCHOR_TOP_RIGHT : RC_ANCHOR_TOP_LEFT);
    float     dy     = above && !inside ? -4.0f : 4.0f;

    rcBox(.bg = rcWithAlpha(rcHex(0x6fa8dc), 0.47f),
          .wType = RC_PX(b.width), .hType = RC_PX(b.height),
          .floating = { .to = RC_ATTACH_ELEMENT, .toId = n->id, .parent = RC_ANCHOR_TOP_LEFT,
                        .element = RC_ANCHOR_TOP_LEFT, .zIndex = 40,
                        .capture = RC_CAPTURE_PASSTHROUGH }) {}
    rcRow(.bg = rcHex(0x202124), .gap = 8, .px = 8, .py = 4, .borderRadius = "all-sm",
          .floating = { .to = RC_ATTACH_ELEMENT, .toId = n->id, .parent = at, .element = corner,
                        .offset = { 0.0f, dy }, .zIndex = 41,
                        .capture = RC_CAPTURE_PASSTHROUGH, .clip = RC_CLIP_NONE }) {
        rcText(rcFormat(rcAppArena(app), "%s#%s", n->kind, n->id), .font = monoFont,
               .color = rcHex(0x8ab4f8), .wrap = "n");
        rcText(rcFormat(rcAppArena(app), "%.0f x %.0f", b.width, b.height), .font = monoFont,
               .color = rcHex(0xe8eaed), .wrap = "n");
    }
}

/* The picker hit-tests by geometry, deepest element first, so the page below it does not
   have to cooperate: a float over the page takes the press and the page's own controls
   see nothing until the pick is done. */
static void picker_update(RC_App *app, AppState *st, const PageNode *nodes, int count) {
    RC_Vec2 p = rcPointer();
    RC_Box  v = rcGetElementBox("page_view");

    st->pickHover = -1;
    if (v.found && p.x >= v.x && p.y >= v.y && p.x < v.x + v.width && p.y < v.y + v.height) {
        for (int i = count - 1; i >= 0 && st->pickHover < 0; i--) {
            RC_Box b = rcGetElementBox(nodes[i].id);

            if (b.found && p.x >= b.x && p.y >= b.y && p.x < b.x + b.width && p.y < b.y + b.height)
                st->pickHover = i;
        }
    }
    rcBox(.id = "pick_shield", .wType = RC_PX(v.width), .hType = RC_PX(v.height),
          .floating = { .to = RC_ATTACH_ELEMENT, .toId = "page_view",
                        .parent = RC_ANCHOR_TOP_LEFT, .element = RC_ANCHOR_TOP_LEFT,
                        .zIndex = 39 }) {}
    if (rcClicked("pick_shield") && st->pickHover >= 0) {
        int depth = nodes[st->pickHover].depth;

        st->sel     = st->pickHover;
        st->tab     = TAB_LAYOUT;
        st->picking = false;
        /* Open every ancestor, so the picked row is on screen in the tree. */
        for (int i = st->pickHover - 1; i >= 0; i--) {
            if (nodes[i].depth < depth) {
                st->collapsed[i] = 0;
                depth = nodes[i].depth;
            }
        }
        console_logf(app, st, RC_LOG_INFO, "picked %s#%s", nodes[st->sel].kind, nodes[st->sel].id);
    }
}

static void page_pane(RC_App *app, AppState *st) {
    RC_Style s = rcGetStyle();
    RC_Box   v = rcGetElementBox("page_view");

    rcColumn(.id = "PagePane", .bg = s.surface, .w = "grow", .h = "grow") {
        rcRow(.gap = 4, .px = 4, .align = "cl", .w = "grow",
              .hType = RC_PX(rcPointerIsCoarse() ? 48.0f : 30.0f),
              .className = "bg-surface border-b") {
            rcBox(.w = "grow") {
                if (rcCombo("target", &st->target, TARGET_NAME, TARGET_COUNT, .className = ctl_h()))
                    select_target(app, st);
            }
            if (tool_icon("page_reload", rcIconRotateCw, INK_MUTED, "Reload the page")) {
                st->roast = 1;
                st->size = 0;
                st->qty = 2;
                st->cart = 0;
                st->coupon = false;
                st->addedAt = 0.0;
                rcScrollToTop(st->target == TARGET_CHECKOUT ? "page_body" : "ord_list");
                console_logf(app, st, RC_LOG_INFO, "page reloaded: %s", TARGET_NAME[st->target]);
            }
            rcBox(.px = 4) {
                rcText(rcFormat(rcAppArena(app), "%.0f x %.0f", v.width, v.height),
                       .font = monoFont, .color = s.textMuted, .wrap = "n");
            }
        }
        /* The page gets its own style while it is declared: a light storefront inside a
           dark tool, as any page is under DevTools. Its bar is called in that style too, so
           the track and thumb are the page's. */
        prof_begin(app, st, "Page", CAT_LAYOUT);
        rcBox(.id = "page_view", .bg = PAGE_BG, .w = "grow", .h = "grow") {
            RC_Style light = rcStyleLight();

            light.primary = PAGE_BRAND;
            rcSetStyle(light);
            if (st->target == TARGET_CHECKOUT)
                page_checkout(app, st);
            else
                page_orders(app, st);
            rcScrollbar(st->target == TARGET_CHECKOUT ? "page_body" : "ord_list");
            rcSetStyle(s);
        }
        prof_end(app, st);
    }
}

/* --------------------------------------------------------------------------
   Layout panel: the element tree and the box model
   -------------------------------------------------------------------------- */

static bool node_has_children(const PageNode *nodes, int count, int i) {
    return i + 1 < count && nodes[i + 1].depth > nodes[i].depth;
}

static bool node_visible(const AppState *st, const PageNode *nodes, int i) {
    int depth = nodes[i].depth;

    if (nodes[i].withCoupon && !st->coupon)
        return false;
    for (int a = i - 1; a >= 0 && depth > 0; a--) {
        if (nodes[a].depth < depth) {
            if (st->collapsed[a])
                return false;
            depth = nodes[a].depth;
        }
    }
    return true;
}

static void tree_row(RC_App *app, AppState *st, const PageNode *nodes, int count, int i,
                     bool sizes, const Palette *pal) {
    RC_Style    s   = rcGetStyle();
    RC_Arena   *mem = rcAppArena(app);
    const PageNode *n = &nodes[i];
    const char *row = rcFormat(mem, "tr%d", i).chars;
    const char *tw  = rcFormat(mem, "tw%d", i).chars;
    bool        on  = st->sel == i;
    bool        kids = node_has_children(nodes, count, i);
    RC_Box      b   = rcGetElementBox(n->id);

    if (rcShowsHover(row))
        st->treeHover = i;
    /* A typed fill beats the class, so the selected row keeps its own under the pointer; the
       others' is RC_UNSET_COLOR, which sets none, and the class's hover shade shows. */
    rcRow(.id = row, .bg = on ? pal->selRow : RC_UNSET_COLOR, .pl = (float)(4 + n->depth * 14),
          .pr = 12, .align = "cl", .w = "grow", .hType = RC_PX((float)row_h()),
          .className = "hover:bg-surface-alt") {
        rcBox(.id = kids ? tw : NULL, .align = "cc", .h = "grow",
              .wType = RC_PX(rcPointerIsCoarse() ? 44.0f : 16.0f)) {
            if (kids)
                (st->collapsed[i] ? rcIconCaretRight : rcIconCaretDown)(10.0f, s.textMuted);
        }
        rcText(rcFormat(mem, "<%s", n->kind), .font = monoFont, .color = pal->tag, .wrap = "n");
        rcTextL(" id", .font = monoFont, .color = pal->attr, .wrap = "n");
        rcTextL("=", .font = monoFont, .color = s.textMuted, .wrap = "n");
        rcText(rcFormat(mem, "\"%s\"", n->id), .font = monoFont, .color = pal->value, .wrap = "n");
        rcTextL(">", .font = monoFont, .color = pal->tag, .wrap = "n");
        rcSpacer(.wMin = RC_VAL(8.0f)) {}
        if (sizes && b.found)
            rcText(rcFormat(mem, "%.0f x %.0f", b.width, b.height), .font = monoFont,
                   .color = s.textMuted, .wrap = "n");
    }
    if (kids && rcClicked(tw))
        st->collapsed[i] = !st->collapsed[i];
    if (rcClicked(row))
        st->sel = i;
}

/* Arrow keys walk the visible rows, as DevTools' tree does. */
static void tree_keys(AppState *st, const PageNode *nodes, int count) {
    int step = rcKeyPressed(RC_KEY_DOWN) ? 1 : rcKeyPressed(RC_KEY_UP) ? -1 : 0;

    if (rcIsFocused("log_filter"))
        return;
    if (step) {
        for (int i = st->sel + step; i >= 0 && i < count; i += step) {
            if (node_visible(st, nodes, i)) {
                st->sel = i;
                break;
            }
        }
    }
    if (rcKeyPressed(RC_KEY_LEFT)) {
        if (node_has_children(nodes, count, st->sel) && !st->collapsed[st->sel]) {
            st->collapsed[st->sel] = 1;
        } else {
            for (int i = st->sel - 1; i >= 0; i--) {
                if (nodes[i].depth < nodes[st->sel].depth) {
                    st->sel = i;
                    break;
                }
            }
        }
    }
    if (rcKeyPressed(RC_KEY_RIGHT))
        st->collapsed[st->sel] = 0;
}

/* DevTools' breadcrumb under the tree: the path to the selected element, every step live. */
static void tree_breadcrumb(RC_App *app, AppState *st, const PageNode *nodes) {
    RC_Style  s = rcGetStyle();
    RC_Arena *mem = rcAppArena(app);
    int       path[8], n = 0, depth = nodes[st->sel].depth + 1;

    for (int i = st->sel; i >= 0 && n < 8; i--) {
        if (nodes[i].depth < depth) {
            path[n++] = i;
            depth = nodes[i].depth;
        }
    }
    rcRow(.id = "crumbs", .gap = 2, .px = 8, .align = "cl", .scroll = "h", .w = "grow",
          .hType = RC_PX(rcPointerIsCoarse() ? 44.0f : 26.0f),
          .className = "bg-surface border-t") {
        for (int i = n - 1; i >= 0; i--) {
            const char *id = rcFormat(mem, "cr%d", path[i]).chars;

            rcBox(.id = id, .focusable = true, .px = 4, .align = "cc", .borderRadius = "all-sm",
                  .hType = RC_PX(rcPointerIsCoarse() ? 44.0f : 20.0f),
                  .className = "ring-inset hover:bg-surface-alt") {
                rcTextC(nodes[path[i]].id, .font = monoFont,
                        .color = i == 0 ? s.text : s.textMuted, .wrap = "n");
            }
            if (rcClicked(id))
                st->sel = path[i];
            if (i > 0)
                rcTextL(">", .font = monoFont, .color = s.textMuted, .wrap = "n");
        }
    }
}

static void box_number(RC_App *app, int value, RC_Color ink) {
    rcText(rcFormat(rcAppArena(app), "%d", value), .font = monoFont, .color = ink, .wrap = "n");
}

/* The diagram every web developer reads at a glance: nested bands with their numbers on
   each side. Clay has no margin, so a parent's gap is listed in the table below instead. */
static void box_model(RC_App *app, const PageNode *n, RC_Box b, const Palette *pal) {
    float cw = b.width  - (float)(n->pl + n->pr);
    float ch = b.height - (float)(n->pt + n->pb + n->bb);

    rcColumn(.bg = pal->boxBorder, .gap = 4, .p = 6, .align = "cc", .borderRadius = "all-sm",
             .w = "grow", .wMax = RC_VAL(340.0f)) {
        rcRow(.w = "grow") {
            rcTextL("border", .color = pal->boxInk, .size = 11, .wrap = "n");
            rcSpacer() {}
            box_number(app, 0, pal->boxInk);
            rcSpacer() {}
        }
        rcColumn(.bg = pal->boxPadding, .gap = 4, .p = 6, .align = "cc", .w = "grow") {
            rcRow(.w = "grow") {
                rcTextL("padding", .color = pal->boxInk, .size = 11, .wrap = "n");
                rcSpacer() {}
                box_number(app, n->pt, pal->boxInk);
                rcSpacer() {}
            }
            rcRow(.gap = 12, .align = "cc", .w = "grow") {
                box_number(app, n->pl, pal->boxInk);
                rcBox(.bg = pal->boxContent, .py = 10, .align = "cc", .w = "grow") {
                    rcText(rcFormat(rcAppArena(app), "%.0f x %.0f", cw, ch), .font = monoFont,
                           .color = pal->boxInk, .wrap = "n");
                }
                box_number(app, n->pr, pal->boxInk);
            }
            box_number(app, n->pb, pal->boxInk);
        }
        box_number(app, n->bb, pal->boxInk);
    }
}

static void kv_row(const char *label, RC_String value) {
    RC_Style s = rcGetStyle();

    rcRow(.gap = 8, .px = 12, .align = "cl", .w = "grow", .hType = RC_PX(22.0f)) {
        rcBox(.w = "84px") { rcTextC(label, .color = s.textMuted, .wrap = "n"); }
        rcBox(.overflow = "hidden", .w = "grow") { mono_num(value, s.text); }
    }
}

static void pane_title(const char *title) {
    RC_Style s = rcGetStyle();

    rcRow(.px = 12, .align = "cl", .w = "grow", .hType = RC_PX(26.0f),
          .className = "bg-surface border-y") {
        rcTextC(title, .font = rcFontSemibold(12.0f), .color = s.text, .wrap = "n");
    }
}

static void element_sidebar(RC_App *app, AppState *st, const PageNode *n, const Palette *pal) {
    RC_Style  s   = rcGetStyle();
    RC_Arena *mem = rcAppArena(app);
    RC_Box    b   = rcGetElementBox(n->id);

    /* An element is measured only while it is laid out: a virtual list declares only the
       rows in view. */
    pane_title("Box model");
    rcRow(.p = 12, .align = "cc", .w = "grow") {
        if (b.found)
            box_model(app, n, b, pal);
        else
            rcTextL("Not laid out right now: scroll the page to it.",
                    .color = s.textMuted);
    }
    pane_title("Computed");
    rcColumn(.py = 4, .w = "grow") {
        kv_row("element", rcFormat(mem, "%s#%s", n->kind, n->id));
        if (b.found) {
            kv_row("x, y", rcFormat(mem, "%.0f, %.0f", b.x, b.y));
            kv_row("width", rcFormat(mem, "%.0f px", b.width));
            kv_row("height", rcFormat(mem, "%.0f px", b.height));
        }
        kv_row("padding", rcFormat(mem, "%d %d %d %d", n->pt, n->pr, n->pb, n->pl));
        kv_row("gap", rcFormat(mem, "%d px", n->gap));
    }
    pane_title("Declaration");
    rcColumn(.gap = 8, .p = 12, .w = "grow") {
        rcBox(.id = "DeclScroll", .scroll = "h", .w = "grow") {
            rcTextC(n->decl, .font = monoFont, .color = s.text, .wrap = "n");
        }
        if (rcButton("copy_decl", "Copy declaration", .className = ctl_h())) {
            rcClipboardSet(n->decl);
            console_logf(app, st, RC_LOG_INFO, "copied the declaration of #%s", n->id);
        }
    }
}

static void layout_panel(RC_App *app, AppState *st, float panelW, bool stacked) {
    Palette  pal = app_palette(st->dark);
    int      count;
    const PageNode *nodes = page_nodes(st, &count);

    if (st->sel >= count)
        st->sel = 0;
    tree_keys(st, nodes, count);
    st->treeHover = -1;
    if (stacked) {
        rcColumn(.pt = 4, .w = "grow") {
            for (int i = 0; i < count; i++) {
                if (node_visible(st, nodes, i))
                    tree_row(app, st, nodes, count, i, panelW >= 360.0f, &pal);
            }
            tree_breadcrumb(app, st, nodes);
            element_sidebar(app, st, &nodes[st->sel], &pal);
        }
    } else {
        rcRow(.w = "grow", .h = "grow") {
            rcColumn(.w = "grow", .h = "grow") {
                rcColumn(.id = "TreeScroll", .py = 4, .scroll = "v", .w = "grow", .h = "grow") {
                    for (int i = 0; i < count; i++) {
                        if (node_visible(st, nodes, i))
                            tree_row(app, st, nodes, count, i, panelW >= 760.0f, &pal);
                    }
                }
                tree_breadcrumb(app, st, nodes);
            }
            rcColumn(.id = "SideScroll", .scroll = "v",
                     .w = panelW >= 760.0f ? "300px" : "256px", .h = "grow",
                     .className = "bg-background border-l") {
                element_sidebar(app, st, &nodes[st->sel], &pal);
            }
        }
    }
}

/* --------------------------------------------------------------------------
   Performance panel
   -------------------------------------------------------------------------- */

static int frame_label(double v, char *out, int cap, void *user) {
    const AppState *st = (const AppState *)user;
    int i = (int)(v + 0.5);

    if (i < 0 || i >= st->ringLen)
        return -1;
    rcStrCopy(out, "Frame ", (size_t)cap);
    return cap > 6 ? 6 + (int)rcFormatIntTo(out + 6, (size_t)(cap - 6),
                                            (int64_t)st->barFrame[i], "")
                   : -1;
}

static void frame_bars(RC_App *app, AppState *st, const Palette *pal) {
    static const float TOPS[] = { 20.0f, 40.0f, 100.0f, 200.0f, 400.0f, 1000.0f };
    RC_Style s    = rcGetStyle();
    float    peak = 0.0f, top = TOPS[0];

    for (int i = 0; i < st->ringLen; i++) {
        const ProfFrame *f = ring_at(st, i);

        st->barX[i]     = (float)i;
        st->barMs[i]    = f->workMs;
        st->barFrame[i] = f->index;
        st->budget[i]   = BUDGET_MS;
        st->barColor[i] = f->index == st->shown.index  ? s.primary
                        : f->workMs < BUDGET_MS        ? pal->barOk
                        : f->workMs < 2.0f * BUDGET_MS ? pal->barSlow
                        :                                pal->barJanky;
        if (f->workMs > peak)
            peak = f->workMs;
    }
    /* A round range keeps the axis to three labels in a short plot, however far a frame
       spikes. */
    for (int i = 0; i < (int)(sizeof TOPS / sizeof TOPS[0]) && peak * 1.1f > top; i++)
        top = TOPS[i];
    RC_Series series[2] = {
        { .y = st->barMs, .x = st->barX, .count = st->ringLen, .kind = RC_SERIES_BAR,
          .color = pal->barOk, .label = "Work", .colors = st->barColor },
        { .y = st->budget, .x = st->barX, .count = st->ringLen, .kind = RC_SERIES_LINE,
          .color = rcWithAlpha(s.textMuted, 0.627f), .label = "60 fps budget", .thickness = 1.0f },
    };
    rcBox(.id = "bars_box", .w = "grow", .h = "grow") {
        rcChart("bars", series, 2,
                .className = "text-[11px]",
                .x = { .min = RC_VAL(0.0f), .max = RC_VAL(PROF_FRAMES - 1), .hide = true,
                       .format = frame_label, .formatUserData = st },
                .y = { .min = RC_VAL(0.0f), .max = RC_VAL(top), .ticks = 2, .grid = true,
                       .decimals = RC_VAL(0), .unit = " ms", .formatUserData = rcAppArena(app),
                       .readoutFormat = readout_hundredths },
                .tooltip = true,
                .barWidth = 0.8f);
    }
    /* A press on a bar stops the recording on that frame, as a click on DevTools' frame
       strip does: the flame chart then holds what you pointed at. */
    if (rcPointerPressed(RC_POINTER_LEFT) && rcIsHovered("bars") && st->ringLen > 0) {
        RC_Box plot = rcChartPlotRect("bars");

        if (plot.found && plot.width > 0.0f) {
            /* The plot spans x = 0 .. PROF_FRAMES - 1, the range pinned above, and bar i is
               centered on x = i, so the bar under the pointer is the nearest whole x. */
            float x = (rcPointer().x - plot.x) / plot.width * (float)(PROF_FRAMES - 1);
            int   i = (int)(x + 0.5f);

            if (x >= -0.5f && i < st->ringLen) {
                st->shown = *ring_at(st, i);
                st->recording = false;
                rows_rebuild(st);
            }
        }
    }
}

/* A share as a percent width: RC_PCT takes 0 to 100 and warns outside it, and a measured
   self time can round to just below zero. */
static float pct_of(float part, float whole) {
    float pct = whole > 0.0f ? part / whole * 100.0f : 0.0f;

    return pct < 0.0f ? 0.0f : pct > 100.0f ? 100.0f : pct;
}

/* The range the flame chart shows: the zoomed block, or the whole frame when this frame has
   no such block. Presenting a frame usually dwarfs declaring it, so opening on Layout shows
   the part of the frame this app's own code decides. */
static const ProfScope *flame_range(const ProfFrame *f, const char *zoom) {
    for (int i = 0; i < f->count; i++) {
        if (f->scope[i].name == zoom && f->scope[i].durMs > 0.0f)
            return &f->scope[i];
    }
    return &f->scope[0];
}

/* A ruling step as DevTools picks one: the smallest 1, 2 or 5 times a power of ten that cuts
   the range into five parts or fewer, so every tick reads as a round number. */
static float ruling_step(float range) {
    static const float NICE[3] = { 1.0f, 2.0f, 5.0f };
    float decade = 0.001f;

    for (int d = 0; d < 12; d++, decade *= 10.0f) {
        for (int i = 0; i < 3; i++) {
            if (5.0f * NICE[i] * decade >= range)
                return NICE[i] * decade;
        }
    }
    return range / 5.0f;
}

static bool scope_has_children(const ProfFrame *f, int i) {
    for (int c = i + 1; c < f->count; c++) {
        if (f->scope[c].parent == i)
            return true;
    }
    return false;
}

static void flame_chart(RC_App *app, AppState *st, const Palette *pal) {
    RC_Style   s     = rcGetStyle();
    RC_Arena  *mem   = rcAppArena(app);
    ProfFrame *f     = &st->shown;
    const ProfScope *range = flame_range(f, st->zoomScope);
    float      from  = range->startMs;
    float      total = range->durMs > 0.0f ? range->durMs : 1.0f;
    float      step  = ruling_step(total);
    float      w     = rcGetElementBox("flame").width;
    int        deepest = 0;
    float      rh    = rcPointerIsCoarse() ? 44.0f : 20.0f;

    for (int i = 0; i < f->count; i++) {
        if (f->scope[i].depth > deepest)
            deepest = f->scope[i].depth;
    }
    /* Rulings on a round step, sharing the blocks' percent widths. The range's end cuts the
       last one short, and drops it when too little is left to hold its label. */
    rcRow(.px = 8, .w = "grow", .h = "18px") {
        for (int t = 0; total - (float)t * step >= 0.5f * step; t++) {
            float left = total - (float)t * step;

            rcBox(.pl = 4, .align = "cl", .h = "grow",
                  .wType = RC_PCT(pct_of(left < step ? left : step, total)),
                  .className = "border-l") {
                rcText(rcFormat(mem, "%g ms", (double)((float)t * step)), .color = s.textMuted,
                       .size = 11, .wrap = "n", .select = RC_SELECT_NONE);
            }
        }
    }
    rcColumn(.id = "flame", .gap = 1, .px = 8, .w = "grow") {
        for (int d = 0; d <= deepest; d++) {
            float at = from;

            rcRow(.overflow = "hidden", .w = "grow", .hType = RC_PX(rh)) {
                for (int i = 0; i < f->count; i++) {
                    const ProfScope *sc = &f->scope[i];
                    float lo = sc->startMs > from ? sc->startMs : from;
                    float hi = sc->startMs + sc->durMs;

                    if (hi > from + total)
                        hi = from + total;
                    if (sc->depth != d || hi <= lo)
                        continue;
                    float       gap = pct_of(lo - at, total);
                    float       pct = pct_of(hi - lo, total);
                    const char *id  = rcFormat(mem, "fl%d", i).chars;
                    bool        hot = rcShowsHover(id);
                    bool        on  = st->selScope == sc->name;
                    RC_Color    bg  = pal->cat[sc->cat];

                    if (gap > 0.0f)
                        rcBox(.h = "grow", .wType = RC_PCT(gap)) {}
                    /* A block never thins below a sliver: it still has to be hoverable. */
                    if (pct < 0.2f)
                        pct = 0.2f;
                    rcBox(.id = id, .bg = hot ? rcWithAlpha(bg, 0.784f) : bg, .pl = 4,
                          .align = "cl", .overflow = "hidden",
                          .border = { .color = on ? s.text : rcWithAlpha(s.background, 1.0f),
                                      .width = 1 },
                          .h = "grow", .wType = RC_PCT(pct),
                          .tooltip = rcFormat(mem, "%s  %.3f ms", sc->name, sc->durMs).chars) {
                        if (pct * w / 100.0f > 36.0f)
                            rcTextC(sc->name, .color = rcHex(0x202124), .wrap = "e",
                                    .select = RC_SELECT_NONE);
                    }
                    if (rcClicked(id)) {
                        st->selScope = sc->name;
                        st->detail = DETAIL_SUMMARY;
                        if (scope_has_children(f, i))
                            st->zoomScope = sc->name;
                    }
                    at = hi;
                }
            }
        }
    }
}

/* The stacked bar and legend DevTools shows for a range: where this frame's time went. */
static void category_row(RC_App *app, const char *name, RC_Color swatch, float ms, float total) {
    RC_Style  s   = rcGetStyle();
    RC_Arena *mem = rcAppArena(app);

    rcRow(.gap = 8, .align = "cl", .w = "grow", .hType = RC_PX(22.0f)) {
        rcBox(.bg = swatch, .borderRadius = "all-xs", .w = "10px", .h = "10px") {}
        rcTextC(name, .color = s.text, .wrap = "n");
        rcSpacer() {}
        rcBox(.align = "cr", .w = "80px") { mono_num(rcFormat(mem, "%.3f ms", ms), s.text); }
        rcBox(.align = "cr", .w = "52px") {
            mono_num(rcFormat(mem, "%.1f%%", total > 0.0f ? ms / total * 100.0f : 0.0f),
                     s.textMuted);
        }
    }
}

static void summary_view(RC_App *app, AppState *st, const Palette *pal) {
    RC_Style   s   = rcGetStyle();
    RC_Arena  *mem = rcAppArena(app);
    ProfFrame *f   = &st->shown;
    float      cat[CAT_COUNT] = { 0 };
    const ProfScope *picked = NULL;
    float      self = 0.0f;
    static const char *const NAME[CAT_COUNT] = { "Update", "Declare layout",
                                                 "Solve, draw, present", "Idle" };

    /* Self time per category, which sums to the frame's work exactly. */
    for (int i = 0; i < f->count; i++) {
        float own = f->scope[i].durMs;

        for (int c = i + 1; c < f->count; c++) {
            if (f->scope[c].parent == i)
                own -= f->scope[c].durMs;
        }
        cat[f->scope[i].cat] += own;
        if (f->scope[i].name == st->selScope) {
            picked = &f->scope[i];
            self = own;
        }
    }
    cat[CAT_IDLE] = f->idleMs;
    float total = f->workMs + f->idleMs;

    rcColumn(.id = "SummaryScroll", .gap = 12, .p = 12, .scroll = "v", .w = "grow", .h = "grow") {
        rcRow(.overflow = "hidden", .borderRadius = "all-sm", .w = "grow", .h = "8px") {
            for (int c = 0; c < CAT_COUNT; c++) {
                rcBox(.bg = pal->cat[c], .h = "grow", .wType = RC_PCT(pct_of(cat[c], total))) {}
            }
        }
        rcColumn(.w = "grow") {
            for (int c = 0; c < CAT_COUNT; c++)
                category_row(app, NAME[c], pal->cat[c], cat[c], total);
        }
        rcColumn(.gap = 2, .pt = 8, .w = "grow", .className = "border-t") {
            if (picked) {
                rcText(rcFormat(mem, "%s: %.3f ms total, %.3f ms self, %.1f%% of the frame's work",
                                picked->name, picked->durMs, self,
                                f->workMs > 0.0f ? picked->durMs / f->workMs * 100.0f : 0.0f),
                       .color = s.text);
            } else {
                rcTextL("Click a block in the flame chart to read it here.", .color = s.textMuted);
            }
        }
    }
}

/* The tables pad every cell, the header's too, so the header's text clears the rule under
   it. A cell holds a row less that padding, so a row stays row_h() tall. */
#define CELL_PAD 4

static float cell_h(void) {
    return (float)(row_h() - 2 * CELL_PAD);
}

/* A number, flush with its column's right edge, as the header above it is. */
static void num_cell(RC_String text, RC_Color ink) {
    rcBox(.align = "cr", .w = "grow", .hType = RC_PX(cell_h())) {
        mono_num(text, ink);
    }
}

/* The table never scrolls sideways: a narrow pane hides Calls, then Max, by .priority, and
   Activity keeps 150 px, so the rest still fit STACK_MIN_W. sideW, the room a phone on its
   side adds past the upright width the tools are laid out at, goes to Activity, so the table
   hides what it hides upright. */
static void bottom_up_table(RC_App *app, AppState *st, const Palette *pal, float sideW) {
    RC_TableColumn cols[6] = {
        { .header = "Self (avg)",  .align = "cr", .w = "92px", .id = "h_self" },
        { .header = "Total (avg)", .align = "cr", .w = "92px", .id = "h_total" },
        { .header = "Max",         .align = "cr", .w = "84px", .id = "h_max", .priority = 1 },
        { .header = "Calls",       .align = "cr", .w = "64px", .id = "h_calls", .priority = 2 },
        { .w = "8px" },     /* empty: keeps Activity's text off the numbers before it */
        { .header = "Activity",    .align = "cl", .w = "grow", .wMin = RC_VAL(150.0f + sideW) },
    };
    RC_Style  s    = rcGetStyle();
    RC_Arena *mem  = rcAppArena(app);
    float     frames = st->ringLen > 0 ? (float)st->ringLen : 1.0f;

    /* The first four columns are in SortKey order; the sorted one's header wears the arrow. */
    cols[st->sort].header = rcFormat(mem, "%s \xE2\x86\x93", cols[st->sort].header).chars;
    if (rcBeginTable("BottomUp", cols, 6, .cellPadding = RC_VAL(CELL_PAD))) {
        for (int r = 0; r < st->rowCount; r++) {
            const ProfRow *row = &st->rows[r];
            /* From the scope, not r: the rows re-sort while recording and on a header click. */
            const char    *id  = rcFormat(mem, "bu-%s", row->name).chars;

            rcTableRow(.id = id, .className = st->selScope == row->name
                                              ? "bg-primary/15" : "hover:bg-surface-alt");
            num_cell(rcFormat(mem, "%.3f ms", row->selfMs / frames), s.text);
            rcTableNext();
            num_cell(rcFormat(mem, "%.3f ms", row->totalMs / frames), s.text);
            rcTableNext();
            num_cell(rcFormat(mem, "%.3f ms", row->maxMs), s.textMuted);
            rcTableNext();
            num_cell(rcFormat(mem, "%d", row->calls), s.textMuted);
            rcTableNext();
            rcTableNext();
            rcRow(.gap = 8, .align = "cl", .w = "grow", .hType = RC_PX(cell_h())) {
                rcBox(.bg = pal->cat[row->cat], .borderRadius = "all-xs", .w = "10px",
                      .h = "10px") {}
                rcTextC(row->name, .color = s.text, .wrap = "n");
            }
            if (rcClicked(id))
                st->selScope = row->name;
        }
        rcEndTable();
    }
    if (rcClicked("h_self"))  { st->sort = SORT_SELF;  rows_sort(st); }
    if (rcClicked("h_total")) { st->sort = SORT_TOTAL; rows_sort(st); }
    if (rcClicked("h_max"))   { st->sort = SORT_MAX;   rows_sort(st); }
    if (rcClicked("h_calls")) { st->sort = SORT_CALLS; rows_sort(st); }
}

/* The frame scheduler's own counters. The park counts read zero in continuous mode,
   and the Frame loop combo above is what brings them alive. */
static void sched_counters(RC_App *app, const RC_SchedStats *sc) {
    RC_Style  s   = rcGetStyle();
    RC_Arena *mem = rcAppArena(app);

    rcBox(.px = 12, .py = 6, .w = "grow") {
        rcTextL("Frame loop", .font = rcFontSemibold(12.0f), .color = s.text);
    }
    kv_row("admitted", rcFormat(mem, "%llu frames", (unsigned long long)sc->admitted));
    kv_row("parks", rcFormat(mem, "%llu", (unsigned long long)sc->waits));
    kv_row("spurious", rcFormat(mem, "%llu", (unsigned long long)sc->spurious));
    kv_row("deadlines", rcFormat(mem, "%llu", (unsigned long long)sc->deadlineWakes));
    kv_row("repaints", rcFormat(mem, "%llu", (unsigned long long)sc->refreshRepaints));
}

static void sched_reasons(RC_App *app, const RC_SchedStats *sc) {
    RC_Style  s   = rcGetStyle();
    RC_Arena *mem = rcAppArena(app);

    rcBox(.px = 12, .py = 6, .w = "grow") {
        rcTextL("Frames admitted, by reason", .font = rcFontSemibold(12.0f), .color = s.text);
    }
    for (int i = 0; i < RC_FRAME_REASON_COUNT; i++) {
        if (sc->byReason[i])
            kv_row(rcFrameReasonName((RC_FrameReason)i),
                   rcFormat(mem, "%llu", (unsigned long long)sc->byReason[i]));
    }
}

static void sched_view(RC_App *app, const AppState *st, float panelW) {
    if (panelW >= 560.0f) {
        rcRow(.id = "SchedScroll", .py = 6, .scroll = "v", .w = "grow", .h = "grow") {
            rcColumn(.w = "grow") { sched_counters(app, &st->sched); }
            rcColumn(.w = "grow") { sched_reasons(app, &st->sched); }
        }
    } else {
        rcColumn(.id = "SchedScroll", .py = 6, .scroll = "v", .w = "grow", .h = "grow") {
            sched_counters(app, &st->sched);
            sched_reasons(app, &st->sched);
        }
    }
}

/* One live figure with its recent trend, DevTools' Performance monitor in a strip. */
static void monitor_tile(const char *id, const char *label, RC_String value, const float *hist,
                         int n, RC_Color color) {
    RC_Style s = rcGetStyle();

    rcRow(.gap = 16, .px = 12, .align = "cl", .w = "grow", .h = "grow", .className = "border-r") {
        rcColumn(.w = "fit") {
            rcTextC(label, .color = s.textMuted, .size = 11, .wrap = "n");
            rcText(value, .font = rcFontSemibold(15.0f), .color = s.text, .wrap = "n");
        }
        rcBox(.w = "grow", .h = "32px") {
            rcSparkline(id, hist, n, .kind = RC_SERIES_AREA, .color = color);
        }
    }
}

static void monitor_cpu_heap(RC_App *app, const AppState *st, const Palette *pal) {
    RC_Arena *mem = rcAppArena(app);

    /* This process's, as a percentage of ONE core, like top. No trend where no reading comes. */
    monitor_tile("sp_cpu", "App CPU", st->cpuNow < 0.0f ? rcStringFromCStr("n/a")
                                                        : rcFormat(mem, "%.0f%%", st->cpuNow),
                 st->cpuPct, st->cpuReads ? st->heapLen : 0, pal->cat[CAT_SCRIPT]);
    monitor_tile("sp_heap", "Resident",
                 rcFormat(mem, "%.1f MiB", st->heapLen ? st->heapMb[st->heapLen - 1] : 0.0f),
                 st->heapMb, st->heapLen, rcGetStyle().primary);
}

static void perf_toolbar(RC_App *app, AppState *st, const Palette *pal, float panelW) {
    static const char *const LOOPS[2] = { "Continuous", "On demand" };
    RC_Style s = rcGetStyle();
    int      loop = st->continuous ? 0 : 1;

    rcRow(.gap = 8, .px = 4, .align = "cl", .w = "grow",
          .hType = RC_PX(rcPointerIsCoarse() ? 48.0f : 30.0f), .className = "bg-surface border-b") {
        if (tool_icon("rec", rcIconCircleDot, st->recording ? INK_RECORDING : INK_MUTED,
                      st->recording ? "Stop recording (Ctrl/Cmd+E)" : "Record (Ctrl/Cmd+E)"))
            st->recording = !st->recording;
        if (tool_icon("rec_clear", rcIconBan, INK_MUTED, "Clear the recording")) {
            st->ringLen = 0;
            st->rowCount = 0;
            st->shown.count = 0;
            st->shown.workMs = st->shown.idleMs = 0.0f;
            st->selScope = NULL;
        }
        tool_divider();
        if (panelW >= 620.0f)
            rcTextL("Frame loop", .color = s.textMuted, .wrap = "n");
        rcBox(.w = "110px") {
            if (rcCombo("loop", &loop, LOOPS, 2, .className = ctl_h())) {
                st->continuous = loop == 0;
                rcWindowSetRenderMode(rcAppMainWindow(app), st->continuous ? RC_RENDER_CONTINUOUS
                                                                           : RC_RENDER_ON_DEMAND);
                console_logf(app, st, RC_LOG_INFO, "frame loop: %s", LOOPS[loop]);
            }
        }
        rcSpacer() {}
        rcBox(.pl = 8, .pr = 16) {
            rcTextC(st->recording ? "Recording" : "Stopped: click a bar, or press Ctrl/Cmd+E",
                    .color = st->recording ? pal->err : s.textMuted, .wrap = "n");
        }
    }
}

static void perf_panel(RC_App *app, AppState *st, float panelW, float sideW, bool stacked) {
    RC_Style  s   = rcGetStyle();
    Palette   pal = app_palette(st->dark);
    RC_Arena *mem = rcAppArena(app);
    static const char *const DETAIL_NAME[DETAIL_COUNT] = { "Summary", "Bottom-up", "Scheduler" };
    static const char *const DETAIL_ID[DETAIL_COUNT]   = { "d_sum", "d_bu", "d_sched" };

    prof_begin(app, st, "panel.toolbar", CAT_LAYOUT);
    perf_toolbar(app, st, &pal, panelW);
    prof_end(app, st);

    prof_begin(app, st, "panel.monitor", CAT_LAYOUT);
    /* Four across where they fit, two by two where they do not. */
    rcColumn(.w = "grow", .className = "border-b") {
        rcRow(.w = "grow", .h = "52px") {
            /* Drawing on demand, 1 / frame time counts the wakes, not how fast a frame is. */
            monitor_tile("sp_fps", st->continuous ? "FPS" : "Wake rate",
                         rcFormat(mem, "%.0f", st->fpsShown), st->fpsHist,
                         st->metricLen, pal.ok);
            monitor_tile("sp_work", "Frame work", rcFormat(mem, "%.2f ms", st->shown.workMs),
                         st->workHist, st->metricLen, pal.cat[CAT_LAYOUT]);
            if (panelW >= 620.0f)
                monitor_cpu_heap(app, st, &pal);
        }
        if (panelW < 620.0f) {
            rcRow(.w = "grow", .h = "52px", .className = "border-t") {
                monitor_cpu_heap(app, st, &pal);
            }
        }
    }
    prof_end(app, st);

    prof_begin(app, st, "panel.frames", CAT_LAYOUT);
    rcBox(.pt = 6, .px = 8, .w = "grow", .h = "76px") {
        frame_bars(app, st, &pal);
    }
    prof_end(app, st);

    prof_begin(app, st, "panel.flame", CAT_LAYOUT);
    rcRow(.gap = 12, .px = 12, .align = "cl", .w = "grow",
          .hType = RC_PX(rcPointerIsCoarse() ? 48.0f : 24.0f)) {
        if (st->shown.count) {
            rcText(rcFormat(mem, "Frame %lu", st->shown.index), .font = rcFontSemibold(12.0f),
                   .color = s.text, .wrap = "n");
            if (panelW >= 360.0f)
                rcText(rcFormat(mem, "%.2f ms work", st->shown.workMs), .font = monoFont,
                       .color = s.text, .wrap = "n");
            if (panelW >= 520.0f)
                rcText(rcFormat(mem, "%.2f ms idle", st->shown.idleMs), .font = monoFont,
                       .color = s.textMuted, .wrap = "n");
            rcSpacer() {}
            if (st->zoomScope == SCOPE_LAYOUT) {
                if (rcButton("fl_zoom", "Show whole frame", .className = small_btn_h(),
                             .variant = RC_BTN_GHOST))
                    st->zoomScope = SCOPE_FRAME;
            } else {
                if (rcButton("fl_zoom", "Zoom to layout", .className = small_btn_h(),
                             .variant = RC_BTN_GHOST))
                    st->zoomScope = SCOPE_LAYOUT;
            }
        } else {
            rcTextL("No recording yet", .color = s.textMuted, .wrap = "n");
        }
    }
    if (st->shown.count) {
        flame_chart(app, st, &pal);
    } else {
        rcBox(.align = "cc", .w = "grow", .h = "96px") {
            rcTextL("Press the record button or Ctrl/Cmd+E to capture frames.",
                    .color = s.textMuted);
        }
    }
    prof_end(app, st);

    prof_begin(app, st, "panel.details", CAT_LAYOUT);
    rcRow(.pl = 4, .align = "cl", .w = "grow", .className = "bg-surface border-y") {
        for (int i = 0; i < DETAIL_COUNT; i++) {
            if (tab_label(DETAIL_ID[i], DETAIL_NAME[i], st->detail == i, 12))
                st->detail = i;
        }
        rcSpacer() {}
        if (st->detail == DETAIL_BOTTOM_UP) {
            /* Last frame's, as the table is declared below: Calls hides first, then Max. */
            uint64_t hid    = rcTableHiddenColumns("BottomUp");
            int      hidden = (int)(hid >> 2 & 1) + (int)(hid >> 3 & 1);

            if (hidden > 0) {
                rcBox(.px = 8, .align = "cc", .h = "26px",
                      .tooltip = hidden > 1 ? "Widen the pane to see Max and Calls"
                                            : "Widen the pane to see Calls") {
                    rcTextC(hidden > 1 ? "+2 columns" : "+1 column", .color = s.textMuted,
                            .wrap = "n");
                }
            }
        }
    }
    rcColumn(.w = "grow", .h = stacked ? "280px" : "grow", .hMin = RC_VAL(120.0f)) {
        if (st->detail == DETAIL_SUMMARY)
            summary_view(app, st, &pal);
        else if (st->detail == DETAIL_BOTTOM_UP)
            bottom_up_table(app, st, &pal, sideW);
        else
            sched_view(app, st, panelW);
    }
    prof_end(app, st);
}

/* --------------------------------------------------------------------------
   Memory panel
   -------------------------------------------------------------------------- */

static MemSnapshot mem_read(const AppState *st) {
    MemSnapshot m;

    m.t         = st->now;
    (void)rcProcessMemoryBytes(&m.resident, &m.peak);   /* 0 and 0 with no reading */
    m.retained  = (size_t)st->retainedCount * RETAIN_BYTES;
    m.arenaUsed = st->arenaUsed;
    m.arenaCap  = st->arenaCap;
    return m;
}

static RC_String mb_text(RC_Arena *mem, size_t bytes) {
    return rcFormat(mem, "%.2f MiB", (double)bytes / (1024.0 * 1024.0));
}

/* One allocation row. Every row but the two totals is part of resident memory, and the rest
   line makes them add up, so the table never claims more than the OS reported. */
static void mem_row(RC_App *app, const char *id, const char *label, size_t bytes, size_t resident,
                    long long delta, bool hasDelta, bool strong) {
    RC_Style  s    = rcGetStyle();
    RC_Arena *mem  = rcAppArena(app);
    float     pct  = resident ? (float)((double)bytes / (double)resident * 100.0) : 0.0f;

    /* The first and last columns stay empty, each cell still opened so the columns line up
       under the header. */
    rcTableRow(.id = id, .className = "hover:bg-surface-alt");
    rcTableNext();
    rcRow(.align = "cl", .w = "grow", .hType = RC_PX(cell_h())) {
        rcTextC(label, .font = (uint16_t)(strong ? rcFontSemibold(12.0f) : 0), .color = s.text,
                .wrap = "n");
    }
    rcTableNext();
    num_cell(mb_text(mem, bytes), s.text);
    rcTableNext();
    num_cell(rcFormat(mem, "%.1f%%", pct), s.textMuted);
    rcTableNext();
    num_cell(!hasDelta ? rcStringFromCStr("-")
                       : rcFormat(mem, "%+.2f MiB", (double)delta / (1024.0 * 1024.0)),
             !hasDelta || delta == 0 ? s.textMuted : s.text);
    rcTableNext();
}

static void mem_table(RC_App *app, const MemSnapshot *m, const MemSnapshot *base,
                      float sideW) {
    RC_Style s = rcGetStyle();
    /* The empty edge columns and the cells' padding put the text 12 px in, under the row
       of text above the table. A narrow pane hides Change, then Share, by .priority, and
       Allocation keeps 240 px, room for its longest name, plus sideW (bottom_up_table). */
    const RC_TableColumn cols[6] = {
        { .w = "8px" },
        { .header = "Allocation", .align = "cl", .w = "grow", .wMin = RC_VAL(240.0f + sideW) },
        { .header = "Size",       .align = "cr", .w = "84px" },
        { .header = "Share",      .align = "cr", .w = "64px", .priority = 1 },
        { .header = "Change",     .align = "cr", .w = "88px", .priority = 2 },
        { .w = "8px" },
    };
    /* Last frame's, as the table is declared below: Change hides first, then Share. */
    uint64_t hid     = rcTableHiddenColumns("MemTable");
    int      hidden  = (int)(hid >> 3 & 1) + (int)(hid >> 4 & 1);
    size_t   statics = sizeof(AppState);
    size_t   known   = m->retained + statics + (size_t)m->arenaCap;
    size_t   rest    = m->resident > known ? m->resident - known : 0;
    size_t   bKnown  = base ? base->retained + statics + (size_t)base->arenaCap : 0;
    size_t   bRest   = base && base->resident > bKnown ? base->resident - bKnown : 0;

    rcRow(.px = 12, .align = "cl", .w = "grow", .h = "24px") {
        rcTextC(base ? "Compared with the snapshot before" : "Take a snapshot to compare",
                .color = s.textMuted, .wrap = "n");
        rcSpacer() {}
        if (hidden > 0) {
            rcBox(.tooltip = hidden > 1 ? "Widen the pane to see Share and Change"
                                        : "Widen the pane to see Change") {
                rcTextC(hidden > 1 ? "+2 columns" : "+1 column", .color = s.textMuted,
                        .wrap = "n");
            }
        }
    }
    if (rcBeginTable("MemTable", cols, 6, .cellPadding = RC_VAL(CELL_PAD))) {
        mem_row(app, "m0", "Resident set (rcProcessMemoryBytes)", m->resident, m->resident,
                base ? (long long)m->resident - (long long)base->resident : 0, base != NULL,
                true);
        mem_row(app, "m1", "Library, graphics, fonts (the rest)", rest, m->resident,
                base ? (long long)rest - (long long)bRest : 0, base != NULL, false);
        mem_row(app, "m2", "Retained test buffers", m->retained, m->resident,
                base ? (long long)m->retained - (long long)base->retained : 0, base != NULL,
                false);
        mem_row(app, "m3", "This app's state (static)", statics, m->resident, 0, base != NULL,
                false);
        mem_row(app, "m4", "Scratch arena capacity", (size_t)m->arenaCap, m->resident, 0,
                base != NULL, false);
        mem_row(app, "m5", "Peak resident", m->peak, m->resident,
                base ? (long long)m->peak - (long long)base->peak : 0, base != NULL, true);
        rcEndTable();
    }
}

static void mem_retain(RC_App *app, AppState *st) {
    RC_Arena       block = rcArenaInit(RETAIN_BYTES);
    unsigned char *bytes = (unsigned char *)rcArenaAlloc(&block, RETAIN_BYTES);

    if (bytes) {
        /* Touch a byte a page, so the OS really backs it and resident memory shows it. Only
           what rcArenaAlloc hands out is the app's to write. */
        for (size_t i = 0; i < RETAIN_BYTES; i += 4096)
            bytes[i] = 0xA5;
        st->retained[st->retainedCount++] = block;
        console_logf(app, st, RC_LOG_INFO, "retained %d x 16 MiB", st->retainedCount);
    } else {
        rcArenaFree(&block);
        console_logf(app, st, RC_LOG_WARNING, "test allocation refused at %d blocks",
                     st->retainedCount);
    }
}

static void mem_release(RC_App *app, AppState *st) {
    while (st->retainedCount > 0)
        rcArenaFree(&st->retained[--st->retainedCount]);
    console_logf(app, st, RC_LOG_INFO, "released the test buffers");
}

static void snapshot_item(RC_App *app, AppState *st, int i, bool stacked) {
    RC_Style    s   = rcGetStyle();
    RC_Arena   *mem = rcAppArena(app);
    const char *id  = i < 0 ? "snap_live" : rcFormat(mem, "snap%d", i).chars;
    bool        on  = st->snapSel == i;
    size_t      bytes = i < 0 ? 0 : st->snaps[i].resident;

    if (i < 0)
        (void)rcProcessMemoryBytes(&bytes, NULL);
    rcColumn(.id = id, .focusable = true, .bg = on ? app_palette(st->dark).selRow : RC_UNSET_COLOR,
             .gap = 2, .px = 12, .py = 6, .w = stacked ? "160px" : "grow",
             .hMin = rcPointerIsCoarse() ? RC_VAL(44.0f) : RC_UNSET_VAL,
             .className = "ring-inset hover:bg-surface-alt") {
        if (i < 0)
            rcTextL("Live", .color = s.text, .wrap = "n");
        else
            rcText(rcFormat(mem, "Snapshot %d", i + 1), .color = s.text, .wrap = "n");
        rcText(i < 0 ? rcFormat(mem, "%.1f MiB now", (double)bytes / 1048576.0)
                     : rcFormat(mem, "%.1f MiB at %.1f s", (double)bytes / 1048576.0,
                                st->snaps[i].t),
               .font = monoFont, .color = on ? s.text : s.textMuted, .wrap = "n");
    }
    if (rcClicked(id))
        st->snapSel = i;
}

static void memory_panel(RC_App *app, AppState *st, float sideW, bool stacked) {
    RC_Style s   = rcGetStyle();
    Palette  pal = app_palette(st->dark);

    rcRow(.id = "MemTools", .gap = 8, .px = 8, .align = "cl", .scroll = "h", .w = "grow",
          .hType = RC_PX(rcPointerIsCoarse() ? 48.0f : 30.0f), .className = "bg-surface border-b") {
        if (rcButton("snap_take", "Take snapshot",
                     .disabled = st->snapCount == MEM_SNAPS, .className = ctl_h())) {
            st->snaps[st->snapCount] = mem_read(st);
            st->snapSel = st->snapCount++;
            console_logf(app, st, RC_LOG_INFO, "snapshot %d: %.1f MiB resident", st->snapCount,
                         (double)st->snaps[st->snapSel].resident / 1048576.0);
        }
        tool_divider();
        if (rcButton("mem_retain", "Retain 16 MiB",
                     .disabled = st->retainedCount == RETAIN_MAX, .className = ctl_h()))
            mem_retain(app, st);
        if (rcButton("mem_free", "Release",
                     .disabled = st->retainedCount == 0, .className = ctl_h()))
            mem_release(app, st);
        rcSpacer(.wMin = RC_VAL(8.0f)) {}
        if (!stacked)
            rcText(rcFormat(rcAppArena(app), "%d of %d retained", st->retainedCount, RETAIN_MAX),
                   .font = monoFont, .color = s.textMuted, .wrap = "n");
    }

    /* The snapshots sit in DevTools' Profiles sidebar, or in a row when the pane is narrow:
       the row pans sideways, with its bar in a lane of its own under the items. A row that
       scrolls clips, so, as CSS's overflow: auto, it has no content floor: shrink-0 keeps
       the scrolling Stack from squashing it. */
    if (stacked) {
        rcRow(.id = "SnapList", .scroll = "h", .w = "grow",
              .className = "border-b shrink-0") {
            snapshot_item(app, st, -1, true);
            for (int i = 0; i < st->snapCount; i++)
                snapshot_item(app, st, i, true);
        }
    }
    rcRow(.w = "grow", .h = stacked ? "fit" : "grow") {
        if (!stacked) {
            rcColumn(.id = "SnapList", .scroll = "v", .w = "176px", .h = "grow",
                     .className = "border-r") {
                rcBox(.px = 12, .py = 6) {
                    rcTextL("HEAP SNAPSHOTS", .font = rcFontSemibold(11.0f), .color = s.textMuted,
                            .wrap = "n");
                }
                snapshot_item(app, st, -1, false);
                for (int i = 0; i < st->snapCount; i++)
                    snapshot_item(app, st, i, false);
            }
        }
        rcColumn(.w = "grow", .h = stacked ? "fit" : "grow") {
            rcBox(.pt = 8, .px = 8, .w = "grow", .h = stacked ? "160px" : "grow",
                  .hMin = RC_VAL(120.0f)) {
                RC_Series series[2] = {
                    { .y = st->heapMb, .x = st->heapT, .count = st->heapLen,
                      .kind = RC_SERIES_AREA, .color = s.primary, .label = "Resident",
                      .thickness = 1.5f },
                    { .y = st->cpuPct, .x = st->heapT, .count = st->heapLen,
                      .kind = RC_SERIES_LINE, .color = pal.cat[CAT_SCRIPT], .label = "App CPU",
                      .thickness = 1.0f, .axis = 1 },
                };
                rcChart("heap", series, st->cpuReads ? 2 : 1,
                        .className = "text-[11px]",
                        .x = { .ticks = 5, .grid = true, .decimals = RC_VAL(0), .unit = " s",
                               .formatUserData = rcAppArena(app), .readoutFormat = readout_tenths },
                        .y = { .min = RC_VAL(0.0f), .ticks = 3, .grid = true, .decimals = RC_VAL(1),
                               .unit = " MiB" },
                        .y2 = { .min = RC_VAL(0.0f), .ticks = 3, .decimals = RC_VAL(0),
                                .unit = "%" },
                        .legend = true,
                        .tooltip = true);
            }
            rcColumn(.pt = 4, .w = "grow") {
                MemSnapshot live = mem_read(st);
                const MemSnapshot *m    = st->snapSel < 0 ? &live : &st->snaps[st->snapSel];
                const MemSnapshot *base = st->snapSel > 0  ? &st->snaps[st->snapSel - 1]
                                        : st->snapSel < 0 && st->snapCount > 0
                                            ? &st->snaps[st->snapCount - 1] : NULL;
                mem_table(app, m, base, sideW);
            }
        }
    }
}

/* --------------------------------------------------------------------------
   Console: the drawer under every panel, or a panel of its own
   -------------------------------------------------------------------------- */

static char lower_ascii(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static bool text_matches(const char *text, const char *needle) {
    if (!needle[0])
        return true;
    for (const char *h = text; *h; h++) {
        const char *a = h, *b = needle;

        while (*a && *b && lower_ascii(*a) == lower_ascii(*b)) {
            a++;
            b++;
        }
        if (!*b)
            return true;
    }
    return false;
}

static bool log_visible(const AppState *st, const InspLogLine *l) {
    if (st->levelFilter == 1 && l->level == RC_LOG_INFO)
        return false;
    if (st->levelFilter == 2 && l->level != RC_LOG_ERROR)
        return false;
    return text_matches(l->msg, st->filter);
}

static void log_counts(const AppState *st, int *errors, int *warnings) {
    *errors = *warnings = 0;
    for (int i = 0; i < st->log.held; i++) {
        int level = insp_log_at(&st->log, i)->level;

        if (level == RC_LOG_ERROR)
            (*errors)++;
        else if (level == RC_LOG_WARNING)
            (*warnings)++;
    }
}

static void log_export(RC_App *app, AppState *st) {
    RC_Arena *mem = rcAppArena(app);

    st->exportLen = 0;
    st->exportBuf[0] = '\0';
    for (int i = 0; i < st->log.held; i++) {
        const InspLogLine *l = insp_log_at(&st->log, i);
        RC_String          line;

        if (!log_visible(st, l))
            continue;
        line = rcFormat(mem, "%.3f\t%s\t%s\n", l->time,
                        l->source == INSP_SRC_RAY ? "rayclay" : "inspector", l->msg);
        if (!export_add(st, line))
            break;
    }
    rcClipboardSet(st->exportBuf);
    console_logf(app, st, RC_LOG_INFO, "copied the console, %d bytes", st->exportLen);
}

static void console_toolbar(AppState *st, bool closable, float panelW) {
    static const char *const LEVELS[3] = { "Default levels", "Warnings + errors", "Errors only" };
    RC_Style s = rcGetStyle();
    bool     finger = rcPointerIsCoarse();

    rcRow(.gap = 4, .px = 4, .align = "cl", .w = "grow", .hType = RC_PX(finger ? 48.0f : 30.0f),
          .className = "bg-surface border-b") {
        if (closable && panelW >= 560.0f) {
            rcBox(.px = 8) {
                rcTextL("Console", .font = rcFontSemibold(12.0f), .color = s.text, .wrap = "n");
            }
        }
        if (tool_icon("log_clear", rcIconBan, INK_MUTED, "Clear console"))
            insp_log_clear(&st->log);
        rcBox(.w = "grow") {
            rcTextInput("log_filter", st->filter, (int)sizeof st->filter, .className = ctl_h(),
                        .placeholder = "Filter", .clearButton = true);
        }
        rcBox(.w = "136px") {
            rcCombo("log_level", &st->levelFilter, LEVELS, 3, .className = ctl_h());
        }
        if (closable && tool_icon("log_close", rcIconX, INK_MUTED, "Close drawer (Esc)"))
            st->drawer = false;
    }
}

/* The log follows its newest line while the reader is parked at the bottom and holds still
   once they scroll up: .scrollStickToEnd. A time or a word too wide for its column is cut at
   the column's edge, so the columns stay in line. */
static void console_body(RC_App *app, AppState *st) {
    RC_Style  s     = rcGetStyle();
    Palette   pal   = app_palette(st->dark);
    RC_Arena *mem   = rcAppArena(app);
    int       shown = 0;

    rcColumn(.id = "LogScroll", .scroll = "v", .scrollStickToEnd = true, .w = "grow",
             .h = "grow") {
        for (int i = 0; i < st->log.held; i++) {
            const InspLogLine *l = insp_log_at(&st->log, i);

            if (!log_visible(st, l))
                continue;
            bool     err  = l->level == RC_LOG_ERROR;
            bool     warn = l->level == RC_LOG_WARNING;
            RC_Color ink  = err ? pal.err : warn ? pal.warn : s.text;

            shown++;
            rcRow(.bg = err ? pal.errRow : warn ? pal.warnRow : RC_UNSET_COLOR, .w = "grow",
                  .className = "border-b") {
                rcBox(.pl = 12, .py = 3, .overflow = "hidden", .w = "64px") {
                    rcText(rcFormat(mem, "%.3f", l->time), .font = monoFont, .color = s.textMuted,
                           .wrap = "n");
                }
                rcRow(.gap = 6, .px = 4, .py = 3, .w = "grow") {
                    if (err || warn)
                        (err ? rcIconCircleX : rcIconTriangleAlert)(12.0f, ink);
                    /* A copy: the ring rewrites its oldest line in place, and a line logged
                       later in this frame (a pick, a library warning) would land in text the
                       frame has not drawn yet. */
                    rcBox(.overflow = "hidden", .w = "grow") {
                        rcText(rcFormat(mem, "%s", l->msg), .font = monoFont, .color = ink);
                    }
                }
                rcBox(.pr = 16, .py = 3, .align = "tr", .w = "84px") {
                    rcTextC(l->source == INSP_SRC_RAY ? "rayclay" : "inspector",
                            .color = s.textMuted, .wrap = "n");
                }
            }
        }
        if (shown == 0) {
            rcBox(.p = 12, .align = "cc", .w = "grow", .h = "grow") {
                rcTextC(st->log.held ? "Every line is hidden by the filter."
                                     : "Nothing logged yet.",
                        .color = s.textMuted);
            }
        }
    }
}

/* --------------------------------------------------------------------------
   The tab strip
   -------------------------------------------------------------------------- */

static const char *const TAB_NAME[TAB_COUNT] = { "Layout", "Performance", "Memory", "Console" };
static const char *const TAB_ID[TAB_COUNT]   = { "t_layout", "t_perf", "t_mem", "t_log" };

static void log_badge(RC_App *app, AppState *st, const char *id, RC_IconCallback icon, RC_Color ink,
                      int count, const char *tip) {
    RC_Style s = rcGetStyle();

    rcRow(.id = id, .focusable = true, .gap = 4, .px = 6, .align = "cc", .borderRadius = "all-sm",
          .hType = RC_PX(rcPointerIsCoarse() ? 44.0f : 24.0f), .tooltip = tip,
          .className = "ring-inset hover:bg-surface-alt") {
        icon(13.0f, ink);
        rcText(rcFormat(rcAppArena(app), "%d", count), .font = monoFont, .color = s.text,
               .wrap = "n");
    }
    if (rcClicked(id))
        st->tab = TAB_CONSOLE;
}

static void more_menu(RC_App *app, AppState *st, bool theme) {
    static const unsigned char notAnImage[] = { 'R', 'C', 'L', 'Y' };

    if (rcBeginMenu("more", "", .className = tool_class(INK_MUTED), .variant = RC_BTN_GHOST,
                    .leadingIcon = rcIconEllipsisVertical)) {
        if (st->sideArm && rcMenuItem(st->drawer ? "Hide console drawer" : "Show console drawer"))
            st->drawer = !st->drawer;
        if (rcMenuItem("Copy console"))
            log_export(app, st);
        if (rcMenuItem("Copy trace as CSV"))
            trace_export(app, st);
        if (rcMenuItem("Trigger a library warning")) {
            console_logf(app, st, RC_LOG_INFO, "decoding four bytes that are no image");
            rcLoadImageFromMemory(notAnImage, (int)sizeof notAnImage);
        }
        if (rcMenuItem("Keyboard shortcuts"))
            st->keysOpen = true;
        if (theme && rcMenuItem(st->dark ? "Use the light theme" : "Use the dark theme"))
            st->dark = !st->dark;
        rcEndMenu();
    }
}

static void tab_strip(RC_App *app, AppState *st, float panelW) {
    RC_Style s   = rcGetStyle();
    Palette  pal = app_palette(st->dark);
    int      errors, warnings;

    log_counts(st, &errors, &warnings);
    /* The tabs scroll sideways in a narrow window; the tools at the end stay in reach. */
    rcRow(.gap = 2, .pl = 4, .pr = 4, .align = "cl", .w = "grow",
          .className = "bg-surface border-b") {
        if (tool_icon("picker", rcIconMousePointerSquareDashed, st->picking ? INK_ON : INK_MUTED,
                      "Select an element in the page (Ctrl/Cmd+Shift+C)"))
            st->picking = !st->picking;
        tool_divider();
        rcRow(.id = "TabStrip", .gap = 2, .align = "cl", .scroll = "h", .w = "grow") {
            for (int i = 0; i < TAB_COUNT; i++) {
                /* A phone shortens the longest tab, so all four fit and none is cut. */
                if (tab_label(TAB_ID[i], i == TAB_PERF && panelW < NARROW_W ? "Perf" : TAB_NAME[i],
                              st->tab == i, 13))
                    st->tab = i;
            }
        }
        /* On a phone the counts wait on the Console tab, one tap away. */
        if (errors && panelW >= NARROW_W)
            log_badge(app, st, "b_err", rcIconCircleX, pal.err, errors, "Errors: open the console");
        if (warnings && panelW >= NARROW_W)
            log_badge(app, st, "b_warn", rcIconTriangleAlert, pal.warn, warnings,
                      "Warnings: open the console");
        if (panelW >= 640.0f) {
            rcBox(.px = 6) {
                rcText(rcFormat(rcAppArena(app), st->continuous ? "%.0f fps" : "%.0f wakes/s",
                                st->fpsShown), .font = monoFont,
                       .color = s.textMuted, .wrap = "n");
            }
        }
        /* On a phone the theme moves into the menu, so every tab fits the strip. */
        if (panelW >= NARROW_W) {
            tool_divider();
            if (tool_icon("theme", st->dark ? rcIconSun : rcIconMoon, INK_MUTED,
                          st->dark ? "Use the light theme" : "Use the dark theme"))
                st->dark = !st->dark;
        }
        more_menu(app, st, panelW < NARROW_W);
    }
}

static void keys_modal(AppState *st) {
    static const char *const KEYS[][2] = {
        { "Ctrl/Cmd+Shift+C", "Select an element in the page" },
        { "Esc", "Cancel the picker, or toggle the console drawer" },
        { "Ctrl/Cmd+E", "Start or stop recording" },
        { "Up, Down", "Walk the element tree" },
        { "Left, Right", "Collapse or expand an element" },
    };
    RC_Style s = rcGetStyle();

    if (rcBeginModal("keys_modal", &st->keysOpen)) {
        rcColumn(.gap = 8, .w = "grow") {
            rcTextL("Keyboard shortcuts", .font = rcFontSemibold(15.0f), .color = s.text);
            for (int i = 0; i < (int)(sizeof KEYS / sizeof KEYS[0]); i++) {
                rcRow(.gap = 12, .align = "cl", .w = "grow") {
                    rcBox(.w = "136px") {
                        rcTextC(KEYS[i][0], .font = monoFont, .color = s.text, .wrap = "n");
                    }
                    rcTextC(KEYS[i][1], .color = s.textMuted);
                }
            }
            if (rcButton("keys_close", "Close", .className = ctl_h()))
                st->keysOpen = false;
        }
        rcEndModal();
    }
}

/* --------------------------------------------------------------------------
   The frame
   -------------------------------------------------------------------------- */

/* panelW is the width the panel lays out at; sideW is the room past it that a phone on its
   side adds (tools_stack), which only the tables' main columns take. */
static void panel_body(RC_App *app, AppState *st, float panelW, float sideW, bool stacked) {
    switch (st->tab) {
    case TAB_LAYOUT:
        prof_begin(app, st, "Panel: Layout", CAT_LAYOUT);
        layout_panel(app, st, panelW, stacked);
        prof_end(app, st);
        break;
    case TAB_MEMORY:
        prof_begin(app, st, "Panel: Memory", CAT_LAYOUT);
        memory_panel(app, st, sideW, stacked);
        prof_end(app, st);
        break;
    case TAB_CONSOLE:
        prof_begin(app, st, "Panel: Console", CAT_LAYOUT);
        console_toolbar(st, false, panelW);
        rcColumn(.w = "grow", .h = "grow", .hMin = RC_VAL(200.0f)) { console_body(app, st); }
        prof_end(app, st);
        break;
    default:
        prof_begin(app, st, "Panel: Performance", CAT_LAYOUT);
        perf_panel(app, st, panelW, sideW, stacked);
        prof_end(app, st);
        break;
    }
}

/* DevTools' right pane: the strip, the panel and, when open, the drawer under it. */
static void tools_pane(RC_App *app, AppState *st, float panelW) {
    prof_begin(app, st, "Tab strip", CAT_LAYOUT);
    tab_strip(app, st, panelW);
    prof_end(app, st);
    /* Read after the strip, whose click may have just opened the Console panel: the console's
       ids can be declared once a frame. */
    if (st->drawer && st->tab != TAB_CONSOLE) {
        if (rcBeginSplitPane("drawer_split", RC_SPLIT_COLUMN, &st->drawerFrac,
                             .minFraction = 0.35f, .maxFraction = 0.85f,
                             .handleThickness = HANDLE_W)) {
            rcColumn(.w = "grow", .h = "grow") { panel_body(app, st, panelW, 0.0f, false); }
            rcSplitHandle();
            rcColumn(.w = "grow", .h = "grow") {
                prof_begin(app, st, "Console drawer", CAT_LAYOUT);
                console_toolbar(st, true, panelW);
                console_body(app, st);
                prof_end(app, st);
            }
            rcEndSplitPane();
        }
    } else {
        rcColumn(.w = "grow", .h = "grow") { panel_body(app, st, panelW, 0.0f, false); }
    }
}

/* The tools where the window is too small for tools_pane: the strip, then the panel in one
   column that scrolls, with no drawer. In a window under PHONE_SIDE_H tall they are laid out
   at the short side, as upright, so turning a phone keeps every control and readout. */
static void tools_stack(RC_App *app, AppState *st, float paneW, float winH, float safeBottom) {
    float toolsW = winH < PHONE_SIDE_H && winH < paneW ? winH : paneW;

    tab_strip(app, st, toolsW);
    rcColumn(.id = "Stack", .pb = safeBottom, .scroll = "v",
             .w = "grow", .h = "grow") {
        panel_body(app, st, toolsW, paneW - toolsW, true);
    }
}

static void layout(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;

    prof_begin(app, st, SCOPE_LAYOUT, CAT_LAYOUT);
    rcSetStyle(app_style(st->dark));

    RC_Style    s    = rcGetStyle();
    RC_Viewport vp   = rcViewport();
    RC_Insets   safe = vp.safe;
    float       w    = vp.width  - safe.left - safe.right;
    float       h    = vp.height - safe.top  - safe.bottom;
    bool        side = w >= SIDE_MIN_W && h >= SIDE_MIN_H;
    /* Too short for the panes of the side arm, yet wide: a phone on its side. The page keeps
       the window's height, as it does beside DevTools docked right, and the tools scroll. */
    bool        beside = h < SIDE_MIN_H && w >= PAGE_MIN_W + STACK_MIN_W + HANDLE_W;
    int         count;
    const PageNode *nodes = page_nodes(st, &count);

    rcWindowSetClearColor(rcAppMainWindow(app), s.background);
    if (st->sel >= count)
        st->sel = 0;
    st->sideArm = side;

    rcColumn(.id = "Root", .bg = s.background, .pt = safe.top,
             .pb = (float)(side || beside ? safe.bottom : 0.0f), .w = "grow", .h = "grow") {
        rcColumn(.pl = safe.left, .pr = safe.right, .w = "grow", .h = "grow") {
            if (side) {
                float panelW = (1.0f - st->pageFrac) * w - HANDLE_W;

                /* The handle stops where a pane would start clipping its own rows. */
                if (rcBeginSplitPane("shell", RC_SPLIT_ROW, &st->pageFrac,
                                     .minFraction = PAGE_MIN_W / w,
                                     .maxFraction = 1.0f - (PANEL_MIN_W + HANDLE_W) / w,
                                     .handleThickness = HANDLE_W)) {
                    rcColumn(.w = "grow", .h = "grow") { page_pane(app, st); }
                    rcSplitHandle();
                    rcColumn(.w = "grow", .h = "grow") { tools_pane(app, st, panelW); }
                    rcEndSplitPane();
                }
            } else {
                /* Smaller: the tools dock under the page, as DevTools docks to the bottom,
                   so the page stays laid out and every element can be measured, or sit
                   beside it in a short, wide window, as DevTools docked right. It is one
                   split either way up, so a phone turned on its side declares what it did
                   upright. Docked, the tools' column runs to the window's bottom edge and
                   pads its end by the safe band there; beside, Root pads both panes. */
                if (rcBeginSplitPane("dock", beside ? RC_SPLIT_ROW : RC_SPLIT_COLUMN,
                                     beside ? &st->besideFrac : &st->stackFrac,
                                     .minFraction = beside ? PAGE_MIN_W / w : 0.25f,
                                     .maxFraction = beside ? 1.0f - (STACK_MIN_W + HANDLE_W) / w
                                                           : 0.6f,
                                     .handleThickness = HANDLE_W)) {
                    rcColumn(.w = "grow", .h = "grow") { page_pane(app, st); }
                    rcSplitHandle();
                    rcColumn(.w = "grow", .h = "grow") {
                        if (beside)
                            tools_stack(app, st, (1.0f - st->besideFrac) * w - HANDLE_W, h, 0.0f);
                        else
                            tools_stack(app, st, w, h, safe.bottom);
                    }
                    rcEndSplitPane();
                }
            }
        }
    }

    /* Overlays last, so a tree row hovered in this frame already lights the page. */
    if (st->picking) {
        picker_update(app, st, nodes, count);
        if (st->pickHover >= 0)
            page_highlight(app, &nodes[st->pickHover]);
    } else if (st->tab == TAB_LAYOUT) {
        page_highlight(app, &nodes[st->treeHover >= 0 ? st->treeHover : st->sel]);
    }
    keys_modal(st);

    /* A bar for each tools pane that scrolls this frame; the page calls its own (page_pane).
       The one-line strips that pan sideways (the tabs, the breadcrumb, a declaration, the
       memory toolbar) take none: under a mouse a bar takes 12 px of its edge, which such a
       line has no height to give. */
    if (!side)
        rcScrollbar("Stack");
    if (st->tab == TAB_LAYOUT && side) {
        rcScrollbar("TreeScroll");
        rcScrollbar("SideScroll");
    }
    if (st->tab == TAB_PERF) {
        rcScrollbar(st->detail == DETAIL_BOTTOM_UP ? "BottomUp"
                    : st->detail == DETAIL_SCHED   ? "SchedScroll"
                    :                                "SummaryScroll");
    }
    if (st->tab == TAB_MEMORY) {
        rcScrollbar("SnapList");
        rcScrollbar("MemTable");
    }
    if (st->tab == TAB_CONSOLE || (side && st->drawer))
        rcScrollbar("LogScroll");

    RC_Arena *arena = rcAppArena(app);

    st->arenaUsed = (int)arena->currOffset;
    st->arenaCap  = (int)arena->bufferLength;
    prof_end(app, st);
    st->layoutEnd = rcAppTime(app);
}

static void hist_push(float *hist, int *len, int cap, float value, bool advance) {
    if (*len < cap) {
        hist[*len] = value;
        if (advance)
            (*len)++;
        return;
    }
    for (int i = 1; i < cap; i++)
        hist[i - 1] = hist[i];
    hist[cap - 1] = value;
}

static void metrics_sample(RC_App *app, AppState *st) {
    /* The frame that just ended left its work in `live`, recording or not: the trend is the
       loop's own, as DevTools' monitor is, while the figure beside it is the shown frame's. */
    if (st->now >= st->metricNext) {
        const float frameTime = rcWindowFrameTime(rcAppMainWindow(app));
        const float fps = frameTime > 0.0f ? 1.0f / frameTime : 0.0f;
        hist_push(st->fpsHist, &st->metricLen, METRIC_HIST, fps, false);
        hist_push(st->workHist, &st->metricLen, METRIC_HIST, st->live.workMs, true);
        st->metricNext = st->now + 0.1;
    }
    /* A number redrawn every frame flickers past reading: show the last second's mean. */
    if (st->now >= st->readoutNext && st->metricLen > 0) {
        int   from = st->metricLen > 10 ? st->metricLen - 10 : 0;
        float sum  = 0.0f;
        for (int i = from; i < st->metricLen; i++)
            sum += st->fpsHist[i];
        st->fpsShown    = sum / (float)(st->metricLen - from);
        st->readoutNext = st->now + 1.0;
    }
    /* rcProcessCpuPercent accounts CPU since the previous call with the same sampler,
       so it runs on this one cadence; main() took the baseline. false is a platform
       with no reading: cpuNow below zero shows n/a, and the trend stays undrawn. */
    if (st->now >= st->heapNext) {
        size_t rss = 0;
        bool   cpu = rcProcessCpuPercent(&st->cpuSince, &st->cpuNow);

        (void)rcProcessMemoryBytes(&rss, NULL);
        if (!cpu)
            st->cpuNow = -1.0f;
        st->cpuReads = st->cpuReads || cpu;
        hist_push(st->heapT, &st->heapLen, HEAP_HIST, (float)st->now, false);
        hist_push(st->cpuPct, &st->heapLen, HEAP_HIST, cpu ? st->cpuNow : 0.0f, false);
        hist_push(st->heapMb, &st->heapLen, HEAP_HIST,
                  (float)((double)rss / 1048576.0), true);
        st->heapNext = st->now + 0.5;
    }
    st->sched = rcWindowSchedStats(rcAppMainWindow(app));
}

static void update(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;

    st->now = rcAppTime(app);
    frame_begin(app, st);
    prof_begin(app, st, "Update", CAT_SCRIPT);

    if (!st->started) {
        st->started = true;
        monoFont = rcLoadFont(MONO_FONT_PATH, 12.0f);
        console_logf(app, st, RC_LOG_INFO, "inspector attached: %s", TARGET_NAME[st->target]);
        console_logf(app, st, RC_LOG_INFO, "frame budget %.1f ms, scratch arena %d KB", BUDGET_MS,
                     (int)(rcAppArena(app)->bufferLength / 1024));
    }

    /* A frame loop on demand parks until asked, so the panels ask once a second. */
    rcWindowRequestFrameAfter(rcAppMainWindow(app), 1.0);

    if (rcModDown(RC_MOD_PRIMARY) && rcKeyPressed(RC_KEY_E))
        st->recording = !st->recording;
    if (rcModDown(RC_MOD_PRIMARY) && rcModDown(RC_MOD_SHIFT) && rcKeyPressed(RC_KEY_C))
        st->picking = !st->picking;
    /* An Esc a popup or a field took reads false here, so this never fights a combo. Only the
       side arm draws the drawer, and a finger has no key to summon it: where Esc has nothing
       on screen to close, Android's Back stays the system's. */
    if ((st->picking || (st->sideArm && (st->drawer || !rcPointerIsCoarse()))) &&
        rcKeyPressed(RC_KEY_ESCAPE)) {
        if (st->picking)
            st->picking = false;
        else
            st->drawer = !st->drawer;
    }

    /* A slow frame is what a console warning is for, once the app has settled. */
    if (st->ringLen > 0 && st->now > 3.0 && st->now - st->budgetWarnedAt > 5.0 &&
        st->recording) {
        const ProfFrame *last = ring_at(st, st->ringLen - 1);

        if (last->workMs > 3.0f * BUDGET_MS) {
            st->budgetWarnedAt = st->now;
            /* The panel only: a slow frame on a busy machine is no fault of the program,
               so it does not belong in the platform's log. */
            insp_log_push(&st->log, INSP_SRC_APP, RC_LOG_WARNING, st->now,
                          rcFormat(rcAppArena(app), "frame %lu took %.1f ms, %.1f x the budget",
                                   last->index, last->workMs, last->workMs / BUDGET_MS).chars);
        }
    }

    metrics_sample(app, st);
    shown_refresh(st);
    prof_end(app, st);
}

int main(void) {
    /* Static: the log sink writes into it and it must outlive the run. */
    static AppState state = {
        .tab = TAB_PERF,
        .detail = DETAIL_BOTTOM_UP,
        .dark = true,
        .drawer = true,
        .pageFrac = 0.32f,
        .drawerFrac = 0.76f,
        .stackFrac = 0.42f,
        .besideFrac = 0.32f,
        .roast = 1,
        .qty = 2,
        .sel = 6,       /* the options card: a selection worth reading on first open */
        .treeHover = -1,
        .pickHover = -1,
        .recording = true,
        .zoomScope = SCOPE_LAYOUT,
        .continuous = true,
        .metricNext = 1.0,      /* the first second is the window coming up: no trend yet */
        .cpuNow = -1.0f,
        .snapSel = -1,
    };
    /* Slot 0 is what rcButton, tooltips and chart labels draw in: DevTools is a 12 px tool. */
    static const float FONT_SIZES[] = { 12.0f };

    rcSetStyle(app_style(state.dark));
    /* Before rcRunApp, so diagnostics from the window coming up reach the console too. */
    rcSetLogSink(on_log, &state);
    /* The CPU baseline, taken here so the first sample is already a reading. */
    (void)rcProcessCpuPercent(&state.cpuSince, NULL);

    RC_AppOptions opts = {
        .width             = 1280,
        .height            = 820,
        .title             = "RayClay Inspector",
        .clearColor        = rcGetStyle().background,
        .fontSizes         = FONT_SIZES,
        .fontCount         = 1,
        /* The orders page formats four strings a row, and 2,000 rows is the point of it. */
        .scratchArenaBytes = 1024 * 1024,
        /* And nine elements a row: room for them up front, so switching targets never stalls. */
        .startLayoutElements = 32 * 1024,
        .frame             = RC_WINDOW_FRAME_TITLEBAR,
        .updateCallback    = update,
        .layoutCallback    = layout,
        .frameEndCallback  = frame_end,
        .userData          = &state,
        /* This app measures the frame loop, so a timed step would report its own cadence. */
        .renderMode        = RC_RENDER_CONTINUOUS,
    };

    /* Nothing after rcRunApp: on the web it never returns. Retained test buffers go back on
       Release (mem_release), or to the OS when the process ends. */
    return rcRunApp(&opts);
}
