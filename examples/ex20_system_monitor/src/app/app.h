/* app.h - the whole UI: the state, and the layout that renders it.

   One header, no .c beside it, every function `static inline`: main.c is the
   only translation unit. `static inline` rather than plain `static` because an
   unused `static` function is a -Wunused-function error under -Werror, and a
   header must not force its includer to call everything in it.

   A visual choice - the palette, the type ramp, the table's measurements - is
   in theme.h. The numbers come from host.h, the seam you replace to make this
   a real monitor. */
#ifndef APP_APP_H
#define APP_APP_H

#include "rayclay.h"
#include "app/theme.h"
#include "app/host.h"
#include "platform/platform.h"

#include "icons/rc_icons_rayclay_logo.h"
#include "icons/rc_icons_chart_column.h"
#include "icons/rc_icons_minimize.h"
#include "icons/rc_icons_maximize.h"
#include "icons/rc_icons_shrink.h"
#include "icons/rc_icons_expand.h"
#include "icons/rc_icons_minus.h"
#include "icons/rc_icons_square.h"
#include "icons/rc_icons_x.h"

/* Seconds between samples. OS CPU accounting is 1-10 ms granular, so anything
   faster than a steady ~1 Hz reads noise rather than load. */
#define SAMPLE_PERIOD 1.0

/* Fraction of the remaining distance an animation covers per frame, and the
   distance at which it snaps (see ease_to). THESE TWO NUMBERS ARE THE APP'S IDLE
   COST: an on-demand window stays awake for as long as an ease takes. */
#define EASE_K    0.35f
#define EASE_SNAP 1.0f

/* Sortable columns. ONE table drives both the clickable header row and the
   table's column widths, so the two can never drift apart. */
typedef enum SortKey {
    SORT_NAME = 0, SORT_PID, SORT_CPU, SORT_MEM, SORT_STATE, SORT_COUNT
} SortKey;

/* `right` rather than an align string: .align is a char[3], which C99 lets you
   initialise from a string LITERAL but not from a pointer expression. */
static const struct { const char *label, *w; bool right; } COLUMN[SORT_COUNT] = {
    { "PROCESS", "grow",  false },
    { "PID",     "78px",  true  },
    { "CPU %",   "88px",  true  },
    { "MEMORY",  "104px", true  },
    { "STATE",   "96px",  false }
};

/* THE WIDTHS THIS APP CHANGES SHAPE AT, each compared against rcViewport().width
   - the space actually being laid out, so a desktop window dragged narrow takes
   the same branch a phone does. rcGetWindowDimensions() would be wrong here:
   under RC_ZOOM_LAYOUT the layout viewport is window/zoom. */
#define SYS_TABLE_FULL_W 624.0f      /* below this the table drops columns      */
#define SYS_CARDS_ROW_W  704.0f      /* below this the four stat cards go 2x2   */
#define SYS_CHART_ROW_W  540.0f      /* below this the chart and cores stack    */
#define SYS_CAPTION_BESIDE_W 740.0f  /* below this the chart caption takes a line */

/* THE HEIGHT THE PAGE STARTS SCROLLING AT. Every pane the wide arrangement
   shows is drawn in a narrow window too, stacked, so the column holding them
   scrolls. The table is the one pane whose height is not fixed: it grows into
   whatever the window leaves, down to a floor, and the page scrolls below that.
   There the table declares every row and is NOT a scroller of its own: a finger
   pan goes to the innermost scroll container under it, so a table that scrolled
   itself would trap the swipe, and rcVirtualList clamps the window it samples to
   the surface height, so it would draw one screenful of rows and a blank tail. */
#define SYS_WIDE_FIXED_H   444.0f  /* fixed rows above the table, cards in one row */
#define SYS_CARDS_STACK_H  130.0f  /* what the 2x2 card arrangement adds to that   */
#define SYS_TABLE_CHROME_H 106.0f  /* card padding, title row, header, borders     */
/* The table's floor is four rows. Below it the page scrolls instead and the
   table runs to every row, which is the only arrangement in which a
   128-process list is still a process list. */
#define SYS_TABLE_MIN_H   (SYS_TABLE_CHROME_H + 4.0f * (float)ROW_PITCH)
#define SYS_CHART_STACK_H 189.0f   /* chart plus the caption line it gains stacked */

/* DETACHABLE PANELS. One RC_Window pointer per slot, because that pointer IS
   the state machine: NULL means docked, non-NULL means a record exists and
   rcWindowState says what it is. The slot is also the window's userData, which
   the library BORROWS for the record's whole life - so it lives in AppState. */
typedef enum Panel {
    PANEL_CHART = 0,
    PANEL_CORES,
    PANEL_TABLE,
    PANEL_COUNT
} Panel;

typedef struct AppState AppState;

/* Where a panel's window was when it last drew, replayed into the next open.
   `valid` is false until a floating window has reported once: a coordinate
   nobody measured is not a coordinate. */
typedef struct PanelGeometry {
    bool valid;
    int  x, y, w, h;
} PanelGeometry;

typedef struct PanelSlot {
    AppState     *st;
    Panel         panel;
    RC_Window    *window;        /* NULL = docked in the main window */
    /* OUR request, kept apart from what the window system reports: a refused pin
       reads back false from rcWindowIsTopmost, and the footer says so. */
    bool          pinRequested;
    unsigned char skips;         /* half-rate phase for a background panel    */
    PanelGeometry geom;
    /* What the native title currently states. SORT_COUNT is the born-with-no-suffix
       value, so the first frame after a detach always writes. */
    SortKey       titledBy;
    bool          titledDesc;
} PanelSlot;

struct AppState {
    SysHost host;

    /* This process. selfCpu is -1 where the platform has no process view at all
       (a browser tab), which the card renders as "n/a" rather than a zero. */
    float   selfCpu;
    size_t  selfMem;
    size_t  selfPeak;         /* high-water RSS over the process's whole life */

    /* The whole machine where the platform has a reading, the simulation where it
       has not. The two flags say which, and every pane that reads them prints it. */
    bool    machineCpuReal;
    bool    machineMemReal;
    float   machineCpu;       /* all cores = 100, unlike selfCpu's one core   */
    size_t  machineTotal;     /* bytes, what this OS calls installed/usable   */
    size_t  machineAvail;     /* bytes, an estimate on every platform         */
    double  sampledAtUnix;    /* civil time of the last sample; 0 = none      */

    /* History, oldest first, newest last. Plain arrays rather than ring buffers
       because rcChart and rcSparkline take a CONTIGUOUS span. */
    float   cpuHist[HISTORY];   /* simulated: busiest core                  */
    float   machHist[HISTORY];  /* REAL: whole-machine CPU, all cores = 100  */
    float   memHist[HISTORY];   /* host memory used, percent; real or model  */
    float   selfHist[HISTORY];  /* REAL: this process's own CPU              */
    float   rssHist[HISTORY];   /* REAL: this process's resident set, MiB    */
    float   netHist[HISTORY];   /* simulated: receive rate, KiB/s            */
    /* THE CEILING FOR THE NETWORK SPARKLINE. rcSparkline's min and max are
       both-or-nothing: {0,0} auto-fits BOTH ends, which lifts the floor to the
       series' own minimum and turns a rate drifting around 420 KiB/s into a
       full-height block that reads as saturation. */
    float   netPeak;
    /* How many of the REAL histories' slots hold a reading. A simulated series is
       seeded before the first frame; a real one cannot be. */
    int     machCount;
    int     selfCount;
    int     rssCount;
    int     samples;
    double  lastSampleAt;       /* rcAppTime() when the last real sample ran  */

    /* `order` is an index permutation: the model is never reordered, so a
       selection survives a re-sort and the pids stay put. */
    int     order[SYS_PROCS];
    SortKey sortKey;
    bool    sortDesc;

    bool    barOpen;
    float   barH;        /* animated, px                                       */
    float   loadShown;   /* animated 0..100, drives the bar's load rail        */

    bool    animating;   /* an ease is still in flight; ask for another frame */
    bool    primed;      /* the history and the CPU interval are seeded        */

    /* rcChildWindowsSupported() answers "can this target open a second window at
       all". `singleSurface` is the second line, because capability is not
       availability: a desktop open can still come back NULL. */
    RC_App   *app;                 /* set by update(); read by the detach verb */
    PanelSlot slot[PANEL_COUNT];
    bool      singleSurface;
    RC_WindowError lastOpenError;  /* why the last detach FAILED; NONE = it did not */
};

/* YOUR OWN ELEMENT MACRO. rcBeginComponent and rcEndComponent are public
   precisely so an app can write a fourth element beside rcBox, rcRow and
   rcColumn. `card` is that: it takes the same designated-initialiser options
   every other element takes and layers a surface colour, padding, radius and
   border underneath them. The defaults go in rcBeginComponent's SECOND argument
   rather than into the options literal: sharing one designated-initialiser list
   warns (-Winitializer-overrides) at every call site that overrides a default.

   A default is only overridable to a NON-ZERO value - the options record has no
   "unset" state distinct from zero, which is why .bg is not defaulted. And a
   ZERO BASE DECLARATION LAYS ITS CHILDREN OUT LEFT TO RIGHT, so a card over
   RC_LIT(RC_ElementDeclaration){0} is sideways: SET THE DIRECTION WITH A CLASS,
   `.className = "flex-col"`, which reaches a custom element as it reaches rcBox. */

static inline RC_ElementDeclaration card_defaults(void)
{
    RC_ElementDeclaration zero = { 0 };

    return rcParseComponentOptions(
        RC_LIT(RC_ComponentOptions){ ._reserved = 0, .p = 14,
                                     .borderRadius = "all-lg",
                                     .className = "flex-col" }, zero);
}

#define SYS__JOIN2(a, b) a##b
#define SYS__JOIN(a, b)  SYS__JOIN2(a, b)

/* The outer loop runs the body once and always closes the card, so `break` out
   of it is safe; __LINE__ keeps the guard unique so cards nest. */
#define card(...) SYS__CARD_(SYS__JOIN(sys__card_, __LINE__), __VA_ARGS__)
#define SYS__CARD_(guard, ...)                                                 \
    for (int guard = 1;                                                        \
         guard && (rcBeginComponent(                                           \
                       RC_LIT(RC_ComponentOptions){ ._reserved = 0,            \
                                                    __VA_ARGS__ },             \
                       card_defaults()), 1);                                   \
         rcEndComponent(), guard = 0)

/* Move *cur a fixed fraction of the way to target; snap when the gap closes.
   Returns true WHILE STILL MOVING, which is the condition for asking for another
   frame. Frame-rate dependent by construction - fine for a 200 ms chrome
   transition, wrong for anything longer, which wants rcAppTime's seconds. */
static inline bool ease_to(float *cur, float target, float k, float snap)
{
    float gap = target - *cur;

    if (gap < 0.0f) gap = -gap;
    if (gap <= snap) {
        *cur = target;
        return false;
    }
    *cur += (target - *cur) * k;
    return true;
}

/* Load-band colour, read from the theme, so a restyle carries the monitor. */
static inline RC_Color load_color(float percent)
{
    RC_Style s = rcGetStyle();

    if (percent >= 85.0f) return s.danger;
    if (percent >= 60.0f) return s.warningHover;
    return s.successHover;
}

/* The same three bands as a WORD: a band that is only a colour is unreadable to
   a colour-blind reader. The two sit one line apart so they cannot drift. */
static inline const char *load_word(float percent)
{
    if (percent >= 85.0f) return "CRITICAL";
    if (percent >= 60.0f) return "ELEVATED";
    return "NOMINAL";
}

static inline const char *state_label(SysProcState st)
{
    switch (st) {
    case SYS_RUNNING: return "running";
    case SYS_STOPPED: return "stopped";
    default:          return "sleeping";
    }
}

static inline void history_push(float *hist, int n, float v)
{
    int i;

    for (i = 0; i + 1 < n; i++)
        hist[i] = hist[i + 1];
    hist[n - 1] = v;
}

/* Insertion sort rather than qsort: C99's qsort has no context parameter, so
   the key would become a file-static the comparator reads. Ascending; the
   caller applies the direction. */
static inline bool proc_before(const SysHost *h, SortKey key, int a, int b)
{
    const SysProc *p = &h->proc[a], *q = &h->proc[b];

    switch (key) {
    case SORT_PID:   return p->pid    < q->pid;
    case SORT_CPU:   return p->cpu    < q->cpu;
    case SORT_MEM:   return p->memMiB < q->memMiB;
    case SORT_STATE: return p->state  < q->state;
    case SORT_NAME:
    default: {
        const char *x = p->name, *y = q->name;
        while (*x && *x == *y) { x++; y++; }
        return (unsigned char)*x < (unsigned char)*y;
    }
    }
}

static inline void resort(AppState *st)
{
    int i, j;

    for (i = 1; i < st->host.procCount; i++) {
        int key = st->order[i];

        for (j = i - 1; j >= 0; j--) {
            bool before = proc_before(&st->host, st->sortKey, key, st->order[j]);

            if (st->sortDesc) before = !before;
            if (!before) break;
            st->order[j + 1] = st->order[j];
        }
        st->order[j + 1] = key;
    }
}

/* What a tap on a column means, in ONE place: the header cell and the chip row
   both call it, so the two controls cannot drift apart. */
static inline void sort_by(AppState *st, SortKey key)
{
    if (st->sortKey == key) {
        st->sortDesc = !st->sortDesc;
    } else {
        st->sortKey = key;
        /* Text reads best A-Z, numbers biggest-first: the direction follows
           the column, not the last one used. */
        st->sortDesc = (key != SORT_NAME);
    }
    resort(st);
}

/* "Available" is an estimate and is not promised to stay below the total;
   size_t arithmetic would turn one bad reading into eighteen exabytes in use. */
static inline size_t machine_used(const AppState *st)
{
    return st->machineAvail > st->machineTotal ? 0 : st->machineTotal - st->machineAvail;
}

/* @p readReal is false only while priming the charts on the first frame: a rate
   needs an interval, and 90 back-to-back calls have none between them. */
static inline void sample(AppState *st, bool readReal)
{
    float busiest = 0.0f;
    int i;

    sysmon_host_sample(&st->host);

    /* rcProcessCpuPercent measures the interval since the PREVIOUS call, so calling
       it exactly once per sample is what makes the number mean anything. Its first
       call has no interval and answers 0; a browser tab answers -1. */
    if (readReal) {
        st->selfCpu  = rcProcessCpuPercent();
        st->selfMem  = rcProcessMemoryBytes();
        /* The HIGH-WATER mark: a leak already freed is invisible in resident and
           obvious in peak. It never falls, so it is a caption, not a sparkline. */
        st->selfPeak = rcProcessPeakMemoryBytes();

        /* rcMachineCpuPercent is the whole machine with ALL cores = 100, a
           different scale from selfCpu's one core = 100. rcMachineMemoryBytes
           answers false where it has no reading, and the sizes are then 0. */
        size_t total = 0, avail = 0;
        float  machine = rcMachineCpuPercent();

        st->machineCpuReal = machine >= 0.0f;
        st->machineCpu     = st->machineCpuReal ? machine : 0.0f;
        st->machineMemReal = rcMachineMemoryBytes(&total, &avail);
        st->machineTotal   = total;
        st->machineAvail   = avail;
        st->sampledAtUnix  = rcUnixTimeSeconds();
    }

    for (i = 0; i < SYS_CORES; i++)
        if (st->host.core[i] > busiest) busiest = st->host.core[i];

    /* Only a real reading enters a real history: -1 is "no such thing on this
       platform" and would chart as a line below the axis. */
    if (readReal && st->selfCpu >= 0.0f) {
        history_push(st->selfHist, HISTORY, st->selfCpu);
        if (st->selfCount < HISTORY)
            st->selfCount++;
    }
    if (readReal && st->machineCpuReal) {
        history_push(st->machHist, HISTORY, st->machineCpu);
        if (st->machCount < HISTORY)
            st->machCount++;
    }
    /* Same rule: 0 resident bytes means "no such thing here". Stored in MiB,
       the unit the card prints, so nothing downstream converts twice. */
    if (readReal && st->selfMem != 0) {
        history_push(st->rssHist, HISTORY,
                     (float)((double)st->selfMem / (1024.0 * 1024.0)));
        if (st->rssCount < HISTORY)
            st->rssCount++;
    }
    history_push(st->cpuHist, HISTORY, busiest);
    /* The high-water is taken here rather than scanned at draw time: it is the peak
       over the whole RUN, so a later quiet stretch reads as quiet. */
    history_push(st->netHist, HISTORY, st->host.netRxKiB);
    if (st->host.netRxKiB > st->netPeak)
        st->netPeak = st->host.netRxKiB;
    if (st->machineMemReal && st->machineTotal > 0) {
        history_push(st->memHist, HISTORY,
                     (float)((double)machine_used(st) / (double)st->machineTotal * 100.0));
    } else {
        history_push(st->memHist, HISTORY,
                     st->host.memUsedMiB / st->host.memTotalMiB * 100.0f);
    }
    st->samples++;
    resort(st);
}

/* PACING AN ON-DEMAND APP.

   A RayClay window parks when nothing changes, so the app decides when to wake:
   rcWindowRequestFrameAfter is "wake me in N seconds", re-armed every frame, and
   rcAppTime(app) is the monotonic clock the sample gate reads.

   DO NOT accumulate rcWindowFrameTime instead. It answers "how long was the last
   frame", never "what time is it": it is smoothed, and a frame longer than a
   second is discarded rather than clamped - exactly what a parked app produces.

   Gate on the clock, not on "did the user do nothing this frame?": a monitor
   that stops monitoring while you use the mouse is not a monitor. */

/* Defined with the panel code below; update() is the only caller. */
static inline void retitle_floating_panels(AppState *st, RC_App *app);

static inline void update(RC_App *app, void *userData)
{
    AppState *st  = (AppState *)userData;
    double    now = rcAppTime(app);
    bool      sampled = false;
    int       p;

    st->app = app;   /* the detach verb runs inside layout, which has no app */

    /* RC_MOD_PRIMARY is Cmd on a native macOS build and Ctrl everywhere else; the
       letter query is logical, so it is T on every keyboard layout. */
    if (rcModDown(RC_MOD_PRIMARY) && rcKeyPressed(RC_KEY_T))
        st->barOpen = !st->barOpen;

    if (!st->primed) {
        /* First frame: prime rcProcessCpuPercent, whose first call has no
           interval to divide by, and fill the simulated histories. */
        int i;

        for (i = 0; i < HISTORY; i++)
            sample(st, false);
        sample(st, true);          /* one real read, to start the CPU interval */
        st->lastSampleAt = now;
        st->primed = true;
        sampled = true;
    } else if (now - st->lastSampleAt >= SAMPLE_PERIOD) {
        sample(st, true);
        st->lastSampleAt = now;
        sampled = true;
    }

    /* A floating panel parks like this window, so the sample is what admits its
       next frame - and WHICH PANELS ARE WORTH WAKING is what the read-only half of
       the rcWindow* family is for: a minimised one never, a background one every
       other sample, a focused or pinned one every sample. `skips` is per slot so a
       second panel cannot shift the first one's phase. */
    if (sampled) {
        for (p = 0; p < PANEL_COUNT; p++) {
            RC_Window *win = st->slot[p].window;

            if (!win || rcWindowIsMinimized(win))
                continue;
            if (rcWindowIsFocused(win) || rcWindowIsTopmost(win)) {
                rcWindowRequestFrame(win);
                continue;
            }
            st->slot[p].skips = (unsigned char)!st->slot[p].skips;
            if (!st->slot[p].skips)
                rcWindowRequestFrame(win);
        }
    }

    retitle_floating_panels(st, app);

    /* One row where the window is wide enough, two where it is not. Read here so a
       rotation eases the band between the two heights exactly as a fold does. */
    float openH = rcViewport().width >= SYS_CARDS_ROW_W ? (float)BAR_OPEN_H
                                                        : (float)BAR_STACK_H;

    st->animating  = ease_to(&st->barH, st->barOpen ? openH : (float)BAR_RAIL_H,
                             EASE_K, EASE_SNAP);
    st->animating |= ease_to(&st->loadShown,
                             st->machineCpuReal ? st->machHist[HISTORY - 1]
                                                : st->cpuHist[HISTORY - 1],
                             EASE_K, EASE_SNAP);

    /* THE BAND YOU DRAW AND THE STRIP THE OS DRAGS BY ARE TWO RECTANGLES, and
       keeping them equal is the app's job: leave the strip alone and a folded bar
       keeps swallowing clicks across the height the open one covered. */
    rcWindowSetTitlebarHeight(rcAppMainWindow(app), (int)st->barH);

    if (st->animating) rcWindowRequestFrame(rcAppMainWindow(app));
    else               rcWindowRequestFrameAfter(rcAppMainWindow(app), SAMPLE_PERIOD);
}

/* THE TITLEBAR. `.titlebar.custom` means the runner draws nothing and this band
   is the whole chrome. The ids carry the behaviour: RC_ID_WINDOW_DRAG makes a
   region move the window, RC_ID_WINDOW_NODRAG carves a clickable hole back out
   of it, and rcWindowControlButton tags itself. All three are inert on web,
   where the browser tab is the chrome - one source, one target drawing less. */

static inline void window_controls(void)
{
    /* The middle slab shows what the NEXT click does. rcIsWindowMaximized
       answers false on web, where the slab emits nothing anyway. */
    RC_IconCallback maxGlyph = rcIsWindowMaximized() ? rcIconShrink
                                                     : rcIconMaximize;

    /* No RC_ID_WINDOW_NODRAG wrapper: each control carries its own
       RC_ID_WINDOW_* id and the drag hit-test already treats those as holes. */
    rcRow(.gap = 2, .align = "cc") {
        rcWindowControlButton(RC_WINCTL_MINIMIZE, rcIconMinimize, 14.0f);
        rcWindowControlButton(RC_WINCTL_MAXIMIZE, maxGlyph,       14.0f);
        rcWindowControlButton(RC_WINCTL_CLOSE,    rcIconX,        14.0f);
    }
}

/* The core rail across the foot of the band: one cell per logical core, lit by
   its own load. It spans the full width at every load, so it cannot be read as a
   progress bar, and it is the readout that survives the fold. */
static inline void core_rail(AppState *st, float height)
{
    RC_Style s = rcGetStyle();
    int i;

    rcRow(.id = "tb_rail", .bg = rcAlpha(s.border, 90), .gap = 1, .w = "grow",
          .hType = RC_PX(height)) {
        for (i = 0; i < SYS_CORES; i++) {
            float   v = st->host.core[i];
            /* Alpha carries the load, the hue carries the band. */
            uint8_t a = (uint8_t)(60.0f + (v > 100.0f ? 100.0f : v) * 1.95f);

            rcBox(.bg = rcAlpha(load_color(v), a), .w = "grow", .h = "grow") {}
        }
    }
}

/* One literal, drawn by both arrangements, so the wording cannot drift. */
#define SYS_SUBTITLE "RayClay " RC_VERSION " \xc2\xb7 one source, every target"

/* The load TREND in the chrome. The figure belongs to the page header below, so
   the band carries the shape only, and a LINE rather than a filled area: a 22 px
   band is too short for a fill to be read as a proportion. */
static inline void chrome_spark(AppState *st, const char *w, const char *h)
{
    /* A real series is drawn from its first reading, not from the zeros before
       it, and the newest sample is the last slot. */
    int          n    = st->machineCpuReal ? st->machCount : HISTORY;
    const float *hist = st->machineCpuReal ? st->machHist : st->cpuHist;

    rcBox(.id = "tb_spark", .w = w, .h = h) {
        rcSparkline("tb_spark_line", hist + HISTORY - n, n,
                     RC_LIT(RC_SparklineOptions){
                         .kind = RC_SERIES_LINE,
                         .color = load_color(st->loadShown),
                         .min = 0.0f, .max = 100.0f });
    }
}

/* The fold chip IS interactive, so it opts out of the drag by hand. */
static inline void fold_button(AppState *st)
{
    rcRow(.id = RC_ID_WINDOW_NODRAG, .gap = 6, .align = "cc") {
        if (rcButton("tb_fold", "fold", RC_BTN_GHOST))
            st->barOpen = false;
    }
}

static inline void titlebar(AppState *st)
{
    RC_Style s = rcGetStyle();
    float railH = st->barH < (float)BAR_RAIL_H ? st->barH : (float)BAR_RAIL_H;
    float bandH = st->barH - railH;
    /* THE TITLEBAR IS A CHILD OF THE ROOT, SO ITS MINIMUM IS THE WINDOW'S.
       Everything in the band on one row floors it near 600 px, the root expands
       to fit, and every growing element below inherits that width and is clipped.
       Below SYS_CARDS_ROW_W the band keeps all of it and draws it on TWO rows. */
    bool roomy = rcViewport().width >= SYS_CARDS_ROW_W;

    /* Chrome, not content: the OS drag strip is in physical px, so without this
       scope the drawn band would grow with the content zoom and stop matching it. */
    rcUnzoomed() {
        rcColumn(.id = RC_ID_WINDOW_DRAG, .bg = s.chrome, .w = "grow",
                 .hType = RC_PX(st->barH)) {
            /* The band collapses to nothing while the rail stays: a folded titlebar with no
               drag region leaves a window that cannot be moved. */
            if (bandH >= 6.0f && roomy) {
                rcRow(.gap = 10, .px = 12, .align = "cl", .w = "grow",
                      .hType = RC_PX(bandH)) {
                    rcIconRayClayLogo(22.0f);
                    rcColumn(.gap = 0) {
                        rcTextL("System Monitor", .font = F_BODY, .color = s.text);
                        rcTextL(SYS_SUBTITLE, .font = F_MICRO, .color = s.textMuted);
                    }
                    rcBox(.w = "grow") {}
                    chrome_spark(st, "132px", "22px");
                    fold_button(st);
                    window_controls();
                }
            } else if (bandH >= 6.0f) {
                /* Two rows, with the same ids, text and controls as the one above. The
                   first row's gap is 6 rather than 10 because at 10 the title wraps on a
                   411 dp phone once its gesture zones are taken off. */
                rcColumn(.gap = 4, .px = 12, .align = "cl", .w = "grow",
                         .hType = RC_PX(bandH)) {
                    rcRow(.gap = 6, .align = "cl", .w = "grow") {
                        rcIconRayClayLogo(22.0f);
                        rcTextL("System Monitor", .font = F_BODY, .color = s.text);
                        rcBox(.w = "grow") {}
                        fold_button(st);
                        window_controls();
                    }
                    rcRow(.gap = 10, .align = "cl", .w = "grow") {
                        rcTextL(SYS_SUBTITLE, .font = F_MICRO, .color = s.textMuted);
                        rcBox(.w = "grow") {}
                        chrome_spark(st, "72px", "16px");
                    }
                }
            }
            core_rail(st, railH);
        }
    }
}

/* Shown only while the bar is folded, so a pointer-only user is never
   stranded without the shortcut. */
static inline void fold_hint(AppState *st)
{
    RC_Style s = rcGetStyle();

    if (st->barOpen) return;
    rcRow(.id = "fold_hint", .bg = s.surfaceAlt, .gap = 8, .px = 10, .py = 5,
          .align = "cc", .borderRadius = "all-full",
          .border = { .color = s.border, .width = "1px" }) {
        rcTextL("titlebar folded", .font = F_MICRO, .color = s.textMuted);
        if (rcButton("hint_show", "show", RC_BTN_GHOST))
            st->barOpen = true;
    }
}

/* FLOATING PANELS - one panel, its own native window.

   rcAppOpenWindow gives a desktop app more than one native surface under the
   same RC_App and the same thread. Each extra window has its own layout
   callback, frame admission and scratch arena.

   THE FOUR RULES, in the order they bite:
   1. A non-NULL record is NOT a ready window: an open from inside a callback
      comes back RC_WINDOW_PENDING, so keep the content docked until READY.
   2. NULL means the open was refused outright - options, memory, or shutdown.
   3. Closing completes after the frame unwinds; release the record in the
      CLOSED transition, which an OS close button reaches too.
   4. rcWindowRelease invalidates the pointer the moment it returns true.

   Owning a window, placing it, pinning it and minimising it are all REQUESTS.
   Each has a *Supported query that says whether this session can do it and a
   getter that says what happened. Ask before drawing a control; read back
   before reporting a state. */

/* Ids are string literals, never rcFormat'd: an id built in the per-frame arena
   is read back after that arena has been reset. The creation MINIMUM is the only
   place an app says "smaller than this and my content stops working" - the
   chart's is the tallest, because its y-axis labels collide first. */
static const struct {
    const char *title, *detachId, *dockId, *hideId, *raiseId, *slotId;
    int w, h, minW, minH;
} PANEL_WINDOW[PANEL_COUNT] = {
    { "Load chart",    "detach_chart", "dock_chart", "hide_chart", "raise_chart",
      "slot_chart", 560, 320, 360, 320 },
    { "Logical cores", "detach_cores", "dock_cores", "hide_cores", "raise_cores",
      "slot_cores", 380, 360, 320, 220 },
    { "Processes",     "detach_table", "dock_table", "hide_table", "raise_table",
      "slot_table", 760, 480, 360, 240 },
};

/* The panel has left: PENDING keeps it here, FAILED is released first. */
static inline bool panel_floating(const AppState *st, Panel p)
{
    const RC_Window *w = st->slot[p].window;
    RC_WindowState state = w ? rcWindowState(w) : RC_WINDOW_CLOSED;

    return w && (state == RC_WINDOW_READY || state == RC_WINDOW_CLOSING);
}

/* One small icon control. A plain rcBox with an id is a button. */
static inline bool icon_button(const char *id, RC_IconCallback icon, const char *tip)
{
    RC_Style s = rcGetStyle();

    rcBox(.id = id, .bg = rcIsHovered(id) ? s.surfaceAlt : RC_TRANSPARENT,
          .p = 4, .align = "cc", .borderRadius = "all-sm", .tooltip = tip) {
        icon(13.0f, s.textMuted);
    }
    return rcClicked(id);
}

static inline void panel_body(AppState *st, Panel p, RC_Arena *mem);
static void panel_window_layout(RC_Window *w, void *user);
static void panel_window_state(RC_Window *w, RC_WindowState state, void *user);

/* Where this panel's window should be BORN, in virtual-screen coordinates.
   False means "wherever the host likes", which is not a failure: a client that is
   never told where it is cannot place itself. Two sources - where the panel was
   when it was last floating, or just past the main window's right edge, stepped
   per panel so three do not land on each other. */
static inline bool panel_open_at(AppState *st, Panel p, int *x, int *y,
                                 int *w, int *h)
{
    RC_Window    *primary = rcAppMainWindow(st->app);
    PanelGeometry g       = st->slot[p].geom;
    RC_Dimensions d;
    int           mx, my;

    if (!rcWindowPositionSupported(primary))
        return false;
    if (g.valid) {
        *x = g.x;
        *y = g.y;
        *w = g.w;
        *h = g.h;
        return true;
    }
    if (!rcWindowPosition(primary, &mx, &my))
        return false;
    d  = rcWindowDimensions(primary);
    *w = PANEL_WINDOW[p].w;
    *h = PANEL_WINDOW[p].h;
    *x = mx + (int)d.width + 12;
    *y = my + (int)p * 28;
    return true;
}

static inline void open_panel_window(AppState *st, Panel p)
{
    RC_Window *primary = rcAppMainWindow(st->app);
    int  bornX = 0, bornY = 0;
    int  bornW = PANEL_WINDOW[p].w, bornH = PANEL_WINDOW[p].h;
    bool at = panel_open_at(st, p, &bornX, &bornY, &bornW, &bornH);

    /* Zero fields take the main window's defaults. Strings are copied before open
       returns; userData is borrowed for the record's life.

       THE FOUR CREATION-STATE FIELDS ARE THE POINT OF THIS CALL: every one is
       VISIBLE if applied a frame late, as a window that opens in the wrong place and
       jumps, or behind its parent and then rises. .owner is the one that changes
       what the panel IS - an owned window is a CHILD, staying above the window it
       was torn from, where an unowned one is a second application that can be lost
       behind it. .focusPolicy is left at its default, which takes focus: a panel the
       user just tore off should be the window they get. */
    RC_WindowOptions o = {
        .title      = PANEL_WINDOW[p].title,
        .width      = bornW,
        .height     = bornH,
        .minWidth   = PANEL_WINDOW[p].minW,
        .minHeight  = PANEL_WINDOW[p].minH,
        .placement  = at ? RC_WINDOW_PLACE_AT : RC_WINDOW_PLACE_DEFAULT,
        .x          = bornX,
        .y          = bornY,
        .owner      = rcWindowOwnerSupported(primary) ? primary : NULL,
        .nativeFrame = true,
        .clearColor = rcGetStyle().background,
        /* A floating monitor parks like the main one; update() wakes it. */
        .renderMode = RC_RENDER_ON_DEMAND,
        .scratchArenaBytes = 16384,
        .layoutCallback = panel_window_layout,
        .stateCallback  = panel_window_state,
        .userData       = &st->slot[p],
    };
    RC_Window *win = rcAppOpenWindow(st->app, &o);

    if (!win) {
        /* Rule 2, on a desktop that said it could: the app cannot tell which
           refusal it was, so it stops offering the verb and says so. */
        st->singleSurface = true;
        return;
    }
    st->slot[p].window       = win;
    st->slot[p].pinRequested = false;
    st->slot[p].titledBy     = SORT_COUNT;   /* no suffix yet */
    st->lastOpenError        = RC_WINDOW_ERROR_NONE;
}

/* A DETACHED PANEL RENAMES ITSELF WHEN ITS CONTENT CHANGES: "Processes" is the
   only word the task switcher has for it, and it does not say which of the five
   orders the user left it in. ON A CHANGE, NEVER EVERY FRAME - a rename is a
   window-system round trip and a taskbar repaint. */
static inline void retitle_floating_panels(AppState *st, RC_App *app)
{
    PanelSlot  *slot = &st->slot[PANEL_TABLE];
    const char *name;

    if (!panel_floating(st, PANEL_TABLE))
        return;
    if (slot->titledBy == st->sortKey && slot->titledDesc == st->sortDesc)
        return;

    name = rcFormat(rcAppArena(app), "%s \xc2\xb7 %s %s",
                    PANEL_WINDOW[PANEL_TABLE].title,
                    COLUMN[st->sortKey].label,
                    st->sortDesc ? "desc" : "asc").chars;

    if (rcWindowSetTitle(slot->window, name)) {
        slot->titledBy   = st->sortKey;
        slot->titledDesc = st->sortDesc;
    }
}

/* The detach control, offered only where it can work: in the MAIN window, for a
   docked panel, on a target with more than one surface. GATED ON CAPABILITY,
   NEVER ON WIDTH - a phone cannot open one whatever its width. */
static inline void detach_button(AppState *st, Panel p)
{
    if (!st->app || rcAppCurrentWindow(st->app) != rcAppMainWindow(st->app))
        return;
    if (!rcChildWindowsSupported() || st->singleSurface)
        return;
    if (st->slot[p].window)
        return;
    if (icon_button(PANEL_WINDOW[p].detachId, rcIconExpand,
                    "Open this panel in its own window"))
        open_panel_window(st, p);
}

/* What the main window shows in a floating panel's place: what that window is
   doing now, and two ways to act on it without going to find it. */
static inline void dock_placeholder(AppState *st, Panel p, const char *w)
{
    RC_Style   s   = rcGetStyle();
    RC_Window *win = st->slot[p].window;
    bool       hidden = rcWindowIsMinimized(win);

    card(.id = PANEL_WINDOW[p].slotId, .bg = rcAlpha(s.surface, 160), .gap = 8,
         .border = { .color = s.border, .width = "1px" }, .w = w, .h = "grow") {
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            /* THE PANEL'S NAME IS THE RAISE CONTROL, as it is in every window list
               the user has met. RESTORE FIRST, THEN RAISE: whether rcWindowRaise
               ALSO un-minimises is the one thing it does not promise. THE ID IS ON
               THE TEXT'S OWN BOX, NEVER ON THE ROW - an element's ANCESTORS report
               as hovered, so an id on the row would make rcClicked true for a press
               anywhere inside it. */
            rcBox(.id = PANEL_WINDOW[p].raiseId,
                  .tooltip = "Bring this window to the front") {
                rcTextC(PANEL_WINDOW[p].title, .font = F_MICRO,
                        .color = rcIsHovered(PANEL_WINDOW[p].raiseId)
                                 ? s.text : s.textMuted);
            }
            if (rcClicked(PANEL_WINDOW[p].raiseId)) {
                if (rcWindowIsMinimized(win))
                    rcWindowRestore(win);
                rcWindowRaise(win);
            }
            rcBox(.w = "grow") {}
            /* Hide is not dock: docking closes the window and loses where the user
               put it, hiding parks it and leaves the geometry alone. A bar and a
               square, not the minimise/maximise glyphs, which are the same four
               inward arrows as the dock control beside them. */
            if (hidden) {
                if (icon_button(PANEL_WINDOW[p].hideId, rcIconSquare,
                                "Show this panel's window again"))
                    rcWindowRestore(win);
            } else {
                if (icon_button(PANEL_WINDOW[p].hideId, rcIconMinus,
                                "Minimise this panel's window"))
                    rcWindowMinimize(win);
            }
            if (icon_button(PANEL_WINDOW[p].dockId, rcIconShrink,
                            "Bring this panel back"))
                rcWindowRequestClose(win);
        }
        rcBox(.w = "grow", .h = "grow") {}
        /* Literals, not a ternary: rcTextL takes a string LITERAL. Each line is
           read back from the window system, so a refused pin is not reported
           here as a pin. */
        if (hidden) {
            rcTextL("in its own window, minimised", .font = F_MICRO,
                     .color = s.textMuted);
        } else if (rcWindowIsTopmost(win)) {
            rcTextL("in its own window, pinned on top", .font = F_MICRO,
                     .color = s.textMuted);
        } else {
            rcTextL("in its own window", .font = F_MICRO, .color = s.textMuted);
        }
    }
}

static inline void panel_draw(AppState *st, Panel p, RC_Arena *mem, const char *w)
{
    if (panel_floating(st, p))
        dock_placeholder(st, p, w);
    else
        panel_body(st, p, mem);
}

/* Park this panel against the main window's right edge and match its height.
   BOTH CALLS ARE REQUESTS, which is why the geometry this app remembers is read
   back every frame rather than assumed from what was asked for here. */
static inline void snap_beside_main(RC_Window *w)
{
    RC_Window    *primary = rcAppMainWindow(rcWindowApp(w));
    RC_Dimensions md      = rcWindowDimensions(primary);
    RC_Dimensions d       = rcWindowDimensions(w);
    int           mx, my;

    if (!rcWindowPosition(primary, &mx, &my))
        return;
    rcWindowSetPosition(w, mx + (int)md.width + 12, my);
    rcWindowSetSize(w, (int)d.width, (int)md.height);
}

/* The floating window's own chrome, and its per-window counters. */
static inline void window_footer(RC_Window *w, PanelSlot *slot, RC_Arena *mem)
{
    RC_Style       s  = rcGetStyle();
    RC_Dimensions  d  = rcWindowDimensions(w);
    RC_FrameCounts fc = rcWindowFrameCounts(w);
    RC_SchedStats  ss = rcWindowSchedStats(w);
    float          ft = rcWindowFrameTime(w);
    bool           pinned = rcWindowIsTopmost(w);

    /* Two rows, because a 380 px window has no line long enough for both. The
       button follows OUR request and says what the next click does; the caption
       beside it reports what the desktop actually did. */
    rcRow(.gap = 10, .px = 2, .align = "cl", .w = "grow", .h = "fit") {
        if (rcWindowTopmostSupported(w)) {
            if (rcButton("float_pin", slot->pinRequested ? "unpin" : "pin on top",
                         RC_BTN_GHOST)) {
                slot->pinRequested = !slot->pinRequested;
                rcWindowSetTopmost(w, slot->pinRequested);   /* a request */
            }
        }
        /* Two reasons to withhold the same button: a session may refuse client
           positioning outright, and a maximised window is the manager's to arrange. */
        if (rcWindowPositionSupported(w) && !rcWindowIsMaximized(w)) {
            if (rcButton("float_snap", "snap beside", RC_BTN_GHOST))
                snap_beside_main(w);
        }
        if (rcButton("float_dock", "dock", RC_BTN_GHOST))
            rcWindowRequestClose(w);
        if (pinned) {
            rcTextL("above other windows", .font = F_MICRO, .color = s.textMuted);
        } else if (slot->pinRequested) {
            /* Capability said yes and the window manager still declined. */
            rcTextL("pin refused by this window manager", .font = F_MICRO,
                     .color = s.warning);
        }
    }
    rcText(rcFormat(mem, "%.0fx%.0f @ %.2fx \xc2\xb7 %.0f fps \xc2\xb7 "
                         "%u declared -> %u drawn \xc2\xb7 %llu frames, %llu parks",
                    (double)d.width, (double)d.height,
                    (double)rcWindowContentScale(w),
                    ft > 0.0f ? 1.0 / (double)ft : 0.0,
                    (unsigned)fc.declared, (unsigned)fc.drawCommands,
                    (unsigned long long)ss.admitted, (unsigned long long)ss.waits),
            .font = F_MICRO, .color = s.textMuted);
}

static void panel_window_layout(RC_Window *w, void *user)
{
    PanelSlot *slot = (PanelSlot *)user;
    RC_Style   s    = rcGetStyle();
    /* THIS window's arena, named rather than inferred: rcAppArena falls back to the
       PRIMARY's arena for a window that owns none. */
    RC_Arena  *mem  = rcWindowArena(w);

    /* Remember where this window is, every frame it draws, so the next open can be
       born there. A maximised window is skipped: its geometry is the screen's, not
       the user's choice of where the panel lives. */
    if (!rcWindowIsMaximized(w)) {
        int px, py;

        if (rcWindowPosition(w, &px, &py)) {
            RC_Dimensions live = rcWindowDimensions(w);

            slot->geom.x     = px;
            slot->geom.y     = py;
            slot->geom.w     = (int)live.width;
            slot->geom.h     = (int)live.height;
            slot->geom.valid = true;
        }
    }

    rcColumn(.id = "float_root", .bg = s.background, .gap = 10, .p = 12,
             .w = "grow", .h = "grow") {
        panel_body(slot->st, slot->panel, mem);
        window_footer(w, slot, mem);
    }
}

/* No default, so a state added to the enum is a compile error here. */
static void panel_window_state(RC_Window *w, RC_WindowState state, void *user)
{
    PanelSlot *slot = (PanelSlot *)user;
    AppState  *st   = slot->st;
    RC_App    *app  = rcWindowApp(w);

    switch (state) {
    case RC_WINDOW_READY:
        /* The main window is parked; ask it for the frame that shows the
           panel swapped for its placeholder. */
        rcWindowRequestFrame(rcAppMainWindow(app));
        break;
    case RC_WINDOW_FAILED:
        st->lastOpenError = rcWindowError(w);
        if (rcWindowRelease(w))            /* rule 4 */
            slot->window = NULL;
        rcWindowRequestFrame(rcAppMainWindow(app));
        break;
    case RC_WINDOW_CLOSED:
        if (rcWindowRelease(w))            /* rule 4 */
            slot->window = NULL;
        slot->pinRequested = false;
        rcWindowRequestFrame(rcAppMainWindow(app));   /* the panel comes home */
        break;
    case RC_WINDOW_PENDING:
    case RC_WINDOW_CLOSING:
        break;
    }
}

/* One stat card, through the macro at the top of this file.
   @p sparkId must differ from @p id: a widget's id IS an element id, so reusing
   the card's declares it twice and the loser takes the winner's box.
   @p sparkKind IS THE DISTINCTION BETWEEN THE TWO FAMILIES OF CARD: a reading
   with a known scale is an AREA from 0, because the filled proportion IS the
   quantity; one with no scale is a LINE on an auto-fitted axis. */
static inline void stat_card(const char *id, const char *sparkId, const char *label,
                       const char *value, const char *unit, RC_Color accent,
                       const float *spark, int sparkCount, float sparkMax,
                       RC_SeriesKind sparkKind, const char *tip)
{
    RC_Style s = rcGetStyle();

    card(.id = id, .bg = s.surface, .gap = 8,
         .border = { .color = s.border, .width = "1px" },
         .w = "grow", .h = "grow", .tooltip = tip) {
        rcTextC(label, .font = F_MICRO, .color = s.textMuted);
        /* A FIT-WIDTH ROW'S MINIMUM IS THE SUM OF ITS CHILDREN, and that
           minimum is inherited by the whole growing column it sits in - one
           unnoticed row can clip every pane on the page. Value beside unit is
           such a row; stacked, the floor is the wider of the two instead. */
        if (rcViewport().width >= SYS_CARDS_ROW_W) {
            rcRow(.gap = 4, .align = "bl") {
                rcTextC(value, .font = F_STAT, .color = accent);
                rcTextC(unit, .font = F_SMALL, .color = s.textMuted);
            }
        } else {
            rcColumn(.gap = 0) {
                rcTextC(value, .font = F_STAT, .color = accent);
                rcTextC(unit, .font = F_SMALL, .color = s.textMuted);
            }
        }
        if (spark) {
            rcBox(.w = "grow", .h = "26px") {
                /* sparkMax 0 leaves the axis auto-fitting. Pin it when the series
                   barely moves, or auto-fit magnifies the noise into a solid block;
                   leave it auto when the series can exceed the pin. */
                rcSparkline(sparkId, spark + HISTORY - sparkCount, sparkCount,
                             RC_LIT(RC_SparklineOptions){ .kind = sparkKind,
                                                          .color = accent,
                                                          .min = 0.0f,
                                                          .max = sparkMax });
            }
        }
    }
}

/* The cards as DATA, so the row can wrap without a second copy of it. */
typedef struct {
    const char *id, *sparkId, *label, *value, *unit, *tip;
    RC_Color    accent;
    const float *spark;
    float       sparkMax;
    int         sparkCount;   /* readings in the array's tail; see stat_card */
    RC_SeriesKind sparkKind;  /* AREA for a percentage, LINE otherwise       */
} StatCardDesc;

static inline void cards(RC_App *app, AppState *st)
{
    RC_Arena *mem = rcAppArena(app);
    RC_Style s = rcGetStyle();
    const SysHost *h = &st->host;
    StatCardDesc d[4];
    int n = 0, i, perRow, row;

    /* COLOUR ON A NUMBER MEANS A THRESHOLD, NEVER DECORATION. The two readings with
       a scale to be judged against take load_color(); the two with no scale take a
       series identity instead, because an amber that means nothing teaches the
       reader to ignore the amber that does. */
    d[n++] = (StatCardDesc){
        "c_cpu", "c_cpu_spark", "THIS PROCESS \xc2\xb7 CPU",
        st->selfCpu < 0.0f ? "n/a"
                           : rcFormat(mem, "%.1f", (double)st->selfCpu).chars,
        st->selfCpu < 0.0f ? "no reading here" : "% of one core",
        "Real: rcProcessCpuPercent(), sampled once per interval",
        st->selfCpu < 0.0f ? s.textMuted : load_color(st->selfCpu),
        st->selfCpu < 0.0f ? NULL : st->selfHist, 0.0f, st->selfCount, RC_SERIES_AREA
    };
    d[n++] = (StatCardDesc){
        "c_rss", "c_rss_spark", "THIS PROCESS \xc2\xb7 RESIDENT",
        st->selfMem == 0 ? "n/a"
            : rcFormat(mem, "%.1f", (double)st->selfMem / (1024.0 * 1024.0)).chars,
        st->selfMem == 0  ? "no reading here"
            : st->selfPeak == 0 ? "MiB"
            : rcFormat(mem, "MiB \xc2\xb7 peak %.1f",
                          (double)st->selfPeak / (1024.0 * 1024.0)).chars,
        "Real: rcProcessMemoryBytes() - RSS, working set, or the wasm heap",
        SYS_MEM_VIOLET,
        st->selfMem == 0 ? NULL : st->rssHist,
        /* AUTO-FITTED, not 0-to-peak: a stable process sits within a few
           percent of its own high-water, so an axis anchored at 0 draws the
           same near-full band every frame. Growth is a SHAPE. */
        0.0f, st->rssCount, RC_SERIES_LINE
    };
    /* Real where the platform has a reading, the model where it has not, and the
       tooltip names which. Used is total minus available, an estimate everywhere. */
    if (st->machineMemReal && st->machineTotal > 0) {
        double totalGiB = (double)st->machineTotal / (1024.0 * 1024.0 * 1024.0);
        double usedGiB  = (double)machine_used(st) / (1024.0 * 1024.0 * 1024.0);

        d[n++] = (StatCardDesc){
            "c_mem", "c_mem_spark", "MACHINE MEMORY",
            rcFormat(mem, "%.1f", usedGiB).chars,
            rcFormat(mem, "of %.0f GiB in use", totalGiB).chars,
            "Real: rcMachineMemoryBytes() - total minus the OS's available estimate",
            load_color((float)(usedGiB / totalGiB * 100.0)), st->memHist, 100.0f,
            HISTORY, RC_SERIES_AREA
        };
    } else {
        d[n++] = (StatCardDesc){
            "c_mem", "c_mem_spark", "HOST MEMORY",
            rcFormat(mem, "%.1f", (double)(h->memUsedMiB / 1024.0f)).chars,
            rcFormat(mem, "of %.0f GiB", (double)(h->memTotalMiB / 1024.0f)).chars,
            "Simulated: app/host.h - this platform reports no machine memory",
            load_color(h->memUsedMiB / h->memTotalMiB * 100.0f), st->memHist, 100.0f,
            HISTORY, RC_SERIES_AREA
        };
    }
    d[n++] = (StatCardDesc){
        "c_net", "c_net_spark", "NETWORK",
        rcFormat(mem, "%.0f", (double)h->netRxKiB).chars,
        rcFormat(mem, "KiB/s down \xc2\xb7 %.0f up", (double)h->netTxKiB).chars,
        "Simulated: app/host.h",
        s.text, st->netHist,
        /* A tenth of headroom, so the busiest sample stops just below the top
           edge instead of clipping flat against it and reading as a limit. */
        st->netPeak > 0.0f ? st->netPeak * 1.1f : 0.0f,
        HISTORY, RC_SERIES_LINE
    };

    /* All four across, or two when four would leave each card too narrow to read
       its own label. A card that does not fit is taken by the next row. */
    perRow = rcViewport().width >= SYS_CARDS_ROW_W ? n : 2;

    for (row = 0; row < n; row += perRow) {
        /* A FIXED height on the row, not fit. The card macro puts a growing
           column inside each card, and a growing child of a fit-height parent
           overflows its parent's border - visible as a sparkline drawn below
           its own card. */
        rcRow(.gap = 12, .w = "grow",
              .hType = RC_PX(perRow == n ? 122 : 120)) {
            for (i = row; i < row + perRow && i < n; i++) {
                stat_card(d[i].id, d[i].sparkId, d[i].label,
                          d[i].value, d[i].unit, d[i].accent, d[i].spark,
                          d[i].sparkCount, d[i].sparkMax, d[i].sparkKind, d[i].tip);
            }
            /* The last row of an odd count would otherwise stretch its one card
               across the width and read as a different component. */
            for (; i < row + perRow; i++)
                rcBox(.w = "grow") {}
        }
    }
}

static const char *const CORE_FILL_ID[SYS_CORES] = {
    "core_fill_0", "core_fill_1", "core_fill_2", "core_fill_3",
    "core_fill_4", "core_fill_5", "core_fill_6", "core_fill_7"
};

/* @p mem is the arena of the SURFACE being drawn: rcAppArena is always the MAIN
   window's, and a child drawing into it would fill it and leave rcFormat
   returning empty strings. */
static inline void core_strip_body(AppState *st, RC_Arena *mem)
{
    RC_Style s = rcGetStyle();
    int i;

    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        rcIconChartColumn(15.0f, s.textMuted);
        rcTextL("LOGICAL CORES", .font = F_MICRO, .color = s.textMuted);
        rcBox(.w = "grow") {}
        detach_button(st, PANEL_CORES);
    }
    for (i = 0; i < SYS_CORES; i++) {
        float v = st->host.core[i];

        rcRow(.gap = 8, .align = "cc", .w = "grow") {
            rcText(rcFormat(mem, "cpu%d", i), .font = F_MICRO,
                    .color = s.textMuted);
            rcBox(.bg = rcAlpha(s.border, 110), .borderRadius = "all-full",
                  .w = "grow", .h = "10px") {
                rcBox(.id = CORE_FILL_ID[i], .bg = load_color(v),
                      .borderRadius = "all-full", .h = "grow", .wType = RC_PCT(v)) {}
            }
            rcBox(.align = "cr", .w = "40px") {
                rcText(rcFormat(mem, "%3.0f%%", (double)v), .font = F_MICRO,
                        .color = s.text);
            }
        }
    }
}

static inline void core_strip(AppState *st, RC_Arena *mem)
{
    RC_Style s = rcGetStyle();

    /* 300 px is what makes this sit BESIDE a growing chart. Alone in its row it
       must GROW instead, or it leaves dead background beside itself. */
    if (rcViewport().width < SYS_CHART_ROW_W ||
        (st->app && rcAppCurrentWindow(st->app) != rcAppMainWindow(st->app))) {
        card(.id = "p_cores", .bg = s.surface, .gap = 8,
             .border = { .color = s.border, .width = "1px" }, .w = "grow", .h = "grow") {
            core_strip_body(st, mem);
        }
        return;
    }
    card(.id = "p_cores", .bg = s.surface, .gap = 8,
         .border = { .color = s.border, .width = "1px" }, .w = "300px", .h = "grow") {
        core_strip_body(st, mem);
    }
}

/* THE CEILING THE PLOT IS DRAWN AGAINST. A CPU chart must keep its zero, or it
   lies about proportion, but pinning the top at 100 spends four fifths of the
   largest panel on the page on headroom a normal machine never reaches. The top
   follows the data, snapped up to a gridline so it does not jitter. */
static inline float chart_ceiling(const float *hist, int n)
{
    float peak = 0.0f;
    int   bands, i;

    for (i = 0; i < n; i++)
        if (hist[i] > peak) peak = hist[i];
    bands = (int)(peak * 1.25f) / 20 + 1;      /* a quarter of headroom */
    if (bands > 5) bands = 5;
    return (float)(bands * 20);
}

/* The chart plots the REAL whole-machine load where the platform reports one
   and the simulated busiest core where it does not, and its title says which. */
static inline void load_chart(AppState *st)
{
    RC_Style s = rcGetStyle();
    bool real = st->machineCpuReal;
    /* Beside the title while the row is wide enough, under it when not. */
    RC_Viewport vp = rcViewport();
    bool beside = vp.width - vp.safe.left - vp.safe.right >= SYS_CAPTION_BESIDE_W;
    const char *caption = real ? "real: rcMachineCpuPercent, all cores = 100"
                               : "simulated; 1 s per sample, timed by rcAppTime";
    /* Explicit x positions, so a real history only n samples old draws as n points
       at the RIGHT end and grows leftwards, the way a strip chart fills. */
    static float axis[HISTORY];
    static bool  axisReady;
    int          n    = real ? st->machCount : HISTORY;
    const float *hist = real ? st->machHist : st->cpuHist;

    if (!axisReady) {
        int i;

        /* Seconds ago: the newest slot is 0 and older ones run negative.
           SAMPLE_PERIOD is 1 s, so one slot is one second. */
        for (i = 0; i < HISTORY; i++)
            axis[i] = (float)(i - (HISTORY - 1));
        axisReady = true;
    }

    card(.id = "p_chart", .bg = s.surface, .gap = 8,
         .border = { .color = s.border, .width = "1px" }, .w = "grow", .h = "grow") {
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            if (real) {
                rcTextL("MACHINE CPU", .font = F_MICRO,
                         .color = s.textMuted);
            } else {
                rcTextL("BUSIEST CORE", .font = F_MICRO,
                         .color = s.textMuted);
            }
            rcBox(.w = "grow") {}
            if (beside)
                rcTextC(caption, .font = F_MICRO, .color = s.textMuted);
            detach_button(st, PANEL_CHART);
        }
        if (!beside)
            rcTextC(caption, .font = F_MICRO, .color = s.textMuted);
        rcBox(.w = "grow", .h = "grow") {
            if (n < 2) {
                /* One reading is not a line, and a grid holding a single point reads
                   as a broken panel rather than a new one. */
                rcBox(.align = "cc", .w = "grow", .h = "grow") {
                    rcTextL("Sampling...", .font = F_MICRO, .color = s.textMuted);
                }
            } else {
                RC_Series ser[] = {
                    { .y = hist + HISTORY - n, .x = axis + HISTORY - n, .count = n,
                      .kind = RC_SERIES_AREA, .color = SYS_TREND_CYAN,
                      .label = real ? "machine %" : "busiest core %" }
                };
                /* THE X WINDOW SPANS THE SAMPLES THAT EXIST, NOT THE BUFFER: a real
                   series cannot be seeded, so pinning the axis to the whole buffer
                   would put its first readings in the last 2% of the panel. .fontSize
                   unset means font slot 0's baked size, so no tick label is resampled. */
                rcChart("sys_chart", ser, 1,
                         RC_LIT(RC_ChartOptions){
                             .x = { .min = -(float)(n - 1), .max = 0.0f,
                                    .label = "seconds ago", .ticks = 6 },
                             .y = { .min = 0.0f,
                                    .max = chart_ceiling(hist + HISTORY - n, n),
                                    .grid = true },
                             .tooltip = RC_CHART_TOOLTIP_NEAREST,
                             .tooltipPlace = RC_TOOLTIP_PLACE_CORNER,
                             .hoverGuide = true, .hoverMarkers = true });
            }
        }
    }
}

/* One process cell's CONTENT, hoisted out of the row loop because .align is a
   char[3] and cannot take a ternary. @p sub draws the same text a size down, for
   the second line a compact row carries. */
static inline void proc_cell(RC_Arena *mem, RC_Style s, const SysProc *p, int col,
                             bool sub)
{
    uint16_t font = sub ? F_MICRO : F_SMALL;

    switch (col) {
    case SORT_NAME:
        rcTextC(p->name, .font = font, .color = s.text);
        break;
    case SORT_PID:
        rcText(rcFormat(mem, "%d", p->pid), .font = font, .color = s.textMuted);
        break;
    case SORT_CPU:
        rcText(rcFormat(mem, "%.1f", (double)p->cpu), .font = font,
                .color = load_color(p->cpu));
        break;
    case SORT_MEM:
        rcText(rcFormat(mem, "%.0f MiB", (double)p->memMiB), .font = font,
                .color = s.textMuted);
        break;
    default:
        rcTextC(state_label(p->state), .font = F_MICRO,
                 .color = p->state == SYS_RUNNING ? s.successHover
                        : p->state == SYS_STOPPED ? s.danger : s.textMuted);
        break;
    }
}

/* The NAME cell of a compact row: the name, and under it every column the
   compact table does not draw. Nothing the wide table shows is missing; only the
   active key has a column of its own to be read down. */
static inline void proc_name_cell(RC_Arena *mem, RC_Style s, const SysProc *p,
                                  const SortKey *vis, int visCount)
{
    rcColumn(.gap = 0) {
        proc_cell(mem, s, p, SORT_NAME, false);
        rcRow(.gap = 8, .align = "cl") {
            for (int c = 0; c < SORT_COUNT; c++) {
                bool drawn = false;

                for (int v = 0; v < visCount; v++)
                    if ((int)vis[v] == c) drawn = true;
                if (!drawn)
                    proc_cell(mem, s, p, c, true);
            }
        }
    }
}

/* The columns to draw at this width, in draw order. The header and the body
   both iterate this, so they cannot drift apart. NEVER drop the column being
   sorted by: a live sort with no visible control cannot be reversed. Returns
   the count. */
static inline int visible_columns(const AppState *st, float width, SortKey *out)
{
    int n = 0;

    if (width >= SYS_TABLE_FULL_W) {
        for (n = 0; n < SORT_COUNT; n++)
            out[n] = (SortKey)n;
        return n;
    }
    out[n++] = SORT_NAME;
    /* Sorting BY name still needs a second column, and CPU is the one a process
       list is read for. */
    out[n++] = (st->sortKey == SORT_NAME) ? SORT_CPU : st->sortKey;
    return n;
}

/* ONE set of ids, carried by the header cells when every column is drawn and by
   the chip row when they are not. */
static const char *const SORT_ID[SORT_COUNT] = {
    "th_name", "th_pid", "th_cpu", "th_mem", "th_state"
};

/* The header row, hand-drawn rather than left to the table's own header because
   a header cell has to be a hit target. The widths come from the same COLUMN
   table the body uses, so the two always line up. */
static inline void table_header(AppState *st, const SortKey *vis, int visCount,
                                bool interactive)
{
    RC_Style s = rcGetStyle();
    int i;

    rcRow(.bg = s.surfaceAlt, .gap = 0, .w = "grow", .h = "30px") {
        for (int v = 0; v < visCount; v++) {
            bool active;

            i = (int)vis[v];
            active = (st->sortKey == (SortKey)i);
            bool hot = interactive && rcIsHovered(SORT_ID[i]);

            rcRow(.id = interactive ? SORT_ID[i] : NULL,
                  .bg = hot ? rcAlpha(s.primary, 70) : s.surfaceAlt,
                  .gap = 4, .px = 8, .align = "cl", .w = COLUMN[i].w, .h = "grow") {
                if (COLUMN[i].right) rcBox(.w = "grow") {}
                rcTextC(COLUMN[i].label, .font = F_MICRO,
                         .color = active ? SYS_TREND_CYAN : s.textMuted);
                if (active) {
                    rcTextC(st->sortDesc ? "v" : "^",
                             .font = F_MICRO, .color = SYS_TREND_CYAN);
                }
                if (!COLUMN[i].right) rcBox(.w = "grow") {}
            }
            /* POLLING rcClicked IS WHAT MAKES A STYLED BOX A BUTTON: it fires on
               the release edge, and the pointer over it gets the clickable hand
               without a cursor call. */
            if (interactive && rcClicked(SORT_ID[i]))
                sort_by(st, (SortKey)i);
        }
    }
}

/* The sort control a compact table would otherwise lose: a column that is not
   drawn cannot be clicked. One chip per key, under the header's own ids. */
static inline void sort_chips(AppState *st)
{
    RC_Style s = rcGetStyle();
    /* SIZED BY THE POINTER, NEVER BY THE WIDTH: 7 px above and below an
       11 px label is a 25 px chip under a mouse, 16 makes it 43 under a
       finger. rcPointerIsCoarse latches on the first touch. */
    uint16_t chipPad = rcPointerIsCoarse() ? 16 : 7;
    int i;

    rcRow(.id = "sort_chips", .bg = s.surfaceAlt, .gap = 4, .p = 4,
          .borderRadius = "all-md", .w = "grow") {
        for (i = 0; i < SORT_COUNT; i++) {
            bool on = (st->sortKey == (SortKey)i);

            rcRow(.id = SORT_ID[i],
                  .bg = on ? s.primary
                           : rcAlpha(s.border, rcIsHovered(SORT_ID[i]) ? 90 : 0),
                  .gap = 3, .py = chipPad, .align = "cc", .borderRadius = "all-sm",
                  .w = "grow") {
                rcTextC(COLUMN[i].label, .font = F_MICRO,
                         .color = on ? RC_WHITE : s.textMuted);
                if (on) {
                    rcTextC(st->sortDesc ? "v" : "^", .font = F_MICRO,
                             .color = RC_WHITE);
                }
            }
            if (rcClicked(SORT_ID[i]))
                sort_by(st, (SortKey)i);
        }
    }
}

/* One row, from the SAME visible list the header drew. Every cell is pinned to
   ROW_H: rcVirtualList sizes its spacers from that number, so a row that is not
   really that tall skews the scroll position. */
static inline void proc_row(RC_Arena *mem, RC_Style s, const SysProc *p, int index,
                            const SortKey *vis, int visCount)
{
    /* THE STRIPE IS THE ROW SEPARATOR, so it has to be visible. Alpha on
       s.border rather than a literal tint, so the same expression steps
       the right way in a light theme and a dark one. */
    RC_Color rowBg = (index & 1) ? rcAlpha(s.border, 120) : s.surface;

    rcTableRow();
    for (int v = 0; v < visCount; v++) {
        int c = (int)vis[v];

        if (v)
            rcTableNext();
        if (COLUMN[c].right) {
            rcBox(.bg = rowBg, .px = 8, .align = "cr",
                  .w = "grow", .hType = RC_PX(ROW_H)) {
                proc_cell(mem, s, p, c, false);
            }
        } else if (c == SORT_NAME && visCount < SORT_COUNT) {
            rcBox(.bg = rowBg, .px = 8, .align = "cl",
                  .w = "grow", .hType = RC_PX(ROW_H)) {
                proc_name_cell(mem, s, p, vis, visCount);
            }
        } else {
            rcBox(.bg = rowBg, .px = 8, .align = "cl",
                  .w = "grow", .hType = RC_PX(ROW_H)) {
                proc_cell(mem, s, p, c, false);
            }
        }
    }
}

/* Does the page scroll? ONE function, read by layout() to pick the page shape
   and by process_table() to pick the table's. The bar's height is the animated
   one, so folding it is counted. */
static inline bool page_scrolls(const AppState *st)
{
    RC_Viewport view   = rcViewport();
    float       innerH = view.height - view.safe.top - view.safe.bottom - st->barH;
    /* THE FIXED BLOCK IS NOT ONE NUMBER: it depends on whether the cards have
       stacked. Measure the wrong arm and this guard believes a page fits that
       does not, and the table's "grow" then resolves to nothing - a header with
       no rows under it. */
    float       fixedH = SYS_WIDE_FIXED_H
                       + (view.width >= SYS_CARDS_ROW_W ? 0.0f : SYS_CARDS_STACK_H);

    return view.width < SYS_CHART_ROW_W
        || innerH < fixedH + SYS_TABLE_MIN_H;
}

static inline void process_table(AppState *st, RC_Arena *mem)
{
    RC_Style s = rcGetStyle();
    RC_TableColumn cols[SORT_COUNT];
    SortKey vis[SORT_COUNT];
    int visCount = visible_columns(st, rcViewport().width, vis);
    /* Every row on a scrolling page; a window of them wherever the table is
       bounded by something of its own. */
    bool allRows = st->app
                && rcAppCurrentWindow(st->app) == rcAppMainWindow(st->app)
                && page_scrolls(st);
    int i;

    for (i = 0; i < visCount; i++) {
        RC_TableColumn c = { 0 };

        c.header = NULL;                 /* the clickable header row is ours */
        c.w      = COLUMN[vis[i]].w;
        rcStrCopy(c.align, COLUMN[vis[i]].right ? "cr" : "cl", sizeof c.align);
        cols[i] = c;
    }

    card(.id = "p_procs", .bg = s.surface, .gap = 0,
         .border = { .color = s.border, .width = "1px" }, .w = "grow", .h = "grow") {
        rcRow(.gap = 8, .pb = 10, .align = "cl", .w = "grow") {
            rcTextL("PROCESSES", .font = F_MICRO, .color = s.textMuted);
            /* The verb follows the POINTER, not the width: a finger taps. */
            rcText(rcFormat(mem, "%d simulated \xc2\xb7 %s a column to sort",
                              st->host.procCount,
                              rcPointerIsCoarse() ? "tap" : "click"),
                    .font = F_MICRO, .color = s.textMuted);
            rcBox(.w = "grow") {}
            /* HOW MANY COLUMNS THE WIDTH TOOK. The table always fits - narrow it
               and columns are dropped rather than a sideways scrollbar appearing -
               so the count sits top right, where a list's overflow badge belongs. */
            if (visCount < SORT_COUNT) {
                rcBox(.id = "proc_hidden", .bg = s.surfaceAlt, .px = 7, .py = 2,
                      .borderRadius = "all-full",
                      .tooltip = "Sort by a hidden column with the chips below.") {
                    rcText(rcFormat(mem, "+%d columns", SORT_COUNT - visCount),
                            .font = F_MICRO, .color = s.textMuted);
                }
            }
            detach_button(st, PANEL_TABLE);
        }
        if (visCount < SORT_COUNT)
            sort_chips(st);
        table_header(st, vis, visCount, visCount == SORT_COUNT);
        rcBox(.w = "grow", .h = "grow") {
            if (rcBeginTable("proc_table", cols, visCount,
                              RC_LIT(RC_TableOptions){
                                  .cellPadding = RC_VAL(CELL_PAD) })) {
                /* PITCH IS NOT CELL HEIGHT, and it is easy to get wrong in a way that
                   compiles. rcVirtualList sizes its spacers from the number you pass,
                   so it must be the distance from one row's top to the next: the cell
                   height PLUS the cell padding, top and bottom. */
                if (allRows) {
                    for (i = 0; i < st->host.procCount; i++)
                        proc_row(mem, s, &st->host.proc[st->order[i]], i,
                                 vis, visCount);
                } else {
                    rcVirtualList(row, "proc_table", st->host.procCount,
                                   (float)ROW_PITCH) {
                        proc_row(mem, s, &st->host.proc[st->order[row.index]],
                                 row.index, vis, visCount);
                    }
                }
                rcEndTable();
            }
            /* Draws nothing when the body holds every row. */
            rcScrollbar("proc_table");
        }
    }
}

/* The three panels behind one name, so two surfaces cannot drift. */
static inline void panel_body(AppState *st, Panel p, RC_Arena *mem)
{
    switch (p) {
    case PANEL_CHART: load_chart(st);         break;
    case PANEL_CORES: core_strip(st, mem);    break;
    case PANEL_TABLE: process_table(st, mem); break;
    case PANEL_COUNT:                         break;
    }
}

/* The declared:drawn ratio is the first thing to look at when a frame feels
   expensive. Read inside a layout callback it reports the PREVIOUS drawn frame;
   use RC_AppOptions.frameEndCallback for this frame's own figures. Both fields
   are 0 until the first frame has been drawn. */

/* Civil time is for a caption and never for pacing: the OS may step it. Whole
   seconds since midnight UTC, no zone. NULL before the first sample lands. */
static inline const char *sampled_text(RC_Arena *mem, const AppState *st)
{
    long day;

    if (st->sampledAtUnix <= 0.0)
        return NULL;
    day = (long)st->sampledAtUnix % 86400L;
    return rcFormat(mem, "sampled %02ld:%02ld:%02ld UTC",
                    day / 3600L, (day / 60L) % 60L, day % 60L).chars;
}

static inline void status_bar(RC_App *app, AppState *st)
{
    RC_Arena *mem = rcAppArena(app);
    RC_Style s = rcGetStyle();
    RC_FrameCounts fc = rcWindowFrameCounts(rcAppMainWindow(app));

    /* Narrow, the row keeps the two short facts and the rest go under it, the long
       provenance sentence in a grow-width box so it wraps at the column's width
       instead of widening it. */
    bool wide = rcViewport().width >= SYS_CARDS_ROW_W;

    /* Built once for either arrangement. ASCII "->" on purpose: the bundled
       face is Latin-1, so U+2192 would draw as the missing-glyph box. */
    const char *declared = rcFormat(mem, "%u declared -> %u drawn",
                                    (unsigned)fc.declared,
                                    (unsigned)fc.drawCommands).chars;
    const char *parks = (st->machineCpuReal && st->machineMemReal)
        ? "parks between samples \xc2\xb7 real: this process, the machine \xc2\xb7 "
          "simulated: cores, net, processes"
        : "parks between samples \xc2\xb7 real: this process \xc2\xb7 "
          "simulated: host (app/host.h)";
    const char *sampled = sampled_text(mem, st);
    const char *notice = NULL;
    RC_Color noticeColor = s.textMuted;

    if (!rcChildWindowsSupported()) {
        /* True from the FIRST frame: the capability does not depend on
           anything having been tried, so nobody has to click a control that
           cannot work to be told about it. */
        notice = "no child windows on this target: panels stay docked";
    } else if (st->singleSurface) {
        notice = "this desktop refused a detach: panels stay docked";
    } else if (st->lastOpenError != RC_WINDOW_ERROR_NONE) {
        notice = rcFormat(mem, "the last detach failed: RC_WindowError %d",
                          (int)st->lastOpenError).chars;
        noticeColor = s.warning;
    }

    if (wide) {
        rcRow(.gap = 14, .px = 4, .align = "cl", .w = "grow", .h = "26px") {
            rcText(rcFormat(mem, "sample %d", st->samples), .font = F_MICRO,
                    .color = s.textMuted);
            rcText(rcFormat(mem, "%.0f fps", (double)rcAppFPS(app)),
                    .font = F_MICRO, .color = s.textMuted);
            rcTextC(declared, .font = F_MICRO, .color = s.textMuted);
            /* Renderer left of this rule, data right of it. */
            rcBox(.bg = s.border, .wType = RC_PX(1), .hType = RC_PX(14)) {}
            rcTextC(parks, .font = F_MICRO, .color = s.textMuted);
            if (notice)
                rcTextC(notice, .font = F_MICRO, .color = noticeColor);
            rcBox(.w = "grow") {}
            fold_hint(st);
        }
        return;
    }
    rcColumn(.gap = 4, .w = "grow") {
        rcRow(.gap = 14, .px = 4, .align = "cl", .w = "grow", .h = "26px") {
            rcText(rcFormat(mem, "sample %d", st->samples), .font = F_MICRO,
                    .color = s.textMuted);
            rcText(rcFormat(mem, "%.0f fps", (double)rcAppFPS(app)),
                    .font = F_MICRO, .color = s.textMuted);
            rcBox(.w = "grow") {}
            fold_hint(st);
        }
        rcRow(.gap = 14, .px = 4, .align = "cl", .w = "grow") {
            rcTextC(declared, .font = F_MICRO, .color = s.textMuted);
            if (sampled)
                rcTextC(sampled, .font = F_MICRO, .color = s.textMuted);
        }
        rcBox(.px = 4, .w = "grow") {
            rcTextC(parks, .font = F_MICRO, .color = s.textMuted);
        }
        if (notice) {
            rcBox(.px = 4, .w = "grow") {
                rcTextC(notice, .font = F_MICRO, .color = noticeColor);
            }
        }
    }
}

/* THE ONE QUESTION THE PAGE ANSWERS BEFORE ANY OTHER: how hard is this machine
   working, and is the number current. Deliberately NOT a card - a dashboard's
   header is not one of its panels. */
static inline void pressure_strip(AppState *st, RC_Arena *mem)
{
    RC_Style    s      = rcGetStyle();
    RC_Color    lit    = load_color(st->loadShown);
    const char *source = st->machineCpuReal
        ? "real \xc2\xb7 rcMachineCpuPercent"
        : "simulated \xc2\xb7 app/host.h";
    const char *sampled = sampled_text(mem, st);
    const bool  wide    = rcViewport().width >= SYS_CARDS_ROW_W;

    /* NOTHING IS WITHHELD WHEN NARROW, only rearranged: a width test that DROPS a
       text run rather than moving it is how a datum ends up reachable at one size
       only. */
    rcColumn(.gap = 4, .px = 4, .w = "grow") {
        rcRow(.gap = 10, .align = "cl", .w = "grow") {
            rcTextL("MACHINE LOAD", .font = F_MICRO, .color = s.textMuted);
            rcText(rcFormat(mem, "%.0f%%", (double)st->loadShown),
                    .font = F_STAT, .color = lit);
            rcRow(.bg = rcAlpha(lit, 34), .px = 8, .py = 3, .align = "cc",
                  .borderRadius = "all-full",
                  .border = { .color = rcAlpha(lit, 140), .width = "1px" }) {
                rcTextC(load_word(st->loadShown), .font = F_MICRO, .color = lit);
            }
            rcBox(.w = "grow") {}
            if (wide) {
                rcTextC(source, .font = F_MICRO, .color = s.textMuted);
                if (sampled)
                    rcTextC(sampled, .font = F_MICRO, .color = s.textMuted);
            }
        }
        if (!wide) {
            rcRow(.gap = 10, .align = "cl", .w = "grow") {
                rcTextC(source, .font = F_MICRO, .color = s.textMuted);
                rcBox(.w = "grow") {}
                if (sampled)
                    rcTextC(sampled, .font = F_MICRO, .color = s.textMuted);
            }
        }
    }
}

static inline void page_body(RC_App *app, AppState *st, RC_Arena *mem, bool stacked)
{
    pressure_strip(st, mem);
    cards(app, st);
    /* Each slot draws its panel, or the placeholder saying where it went. */
    if (!stacked) {
        rcRow(.gap = 12, .w = "grow", .h = "232px") {
            panel_draw(st, PANEL_CHART, mem, "grow");
            panel_draw(st, PANEL_CORES, mem, "300px");
        }
    } else {
        /* STACKED, NOT WITHHELD: a narrow window shows everything a wide one
           shows, scrolling allowed. The chart keeps a fixed height because
           rcChart GROWS ON BOTH AXES and must be sized by the box around it. */
        rcBox(.w = "grow", .hType = RC_PX(SYS_CHART_STACK_H)) {
            panel_draw(st, PANEL_CHART, mem, "grow");
        }
        rcBox(.w = "grow") {
            panel_draw(st, PANEL_CORES, mem, "grow");
        }
    }
    /* "grow" on both kinds of page, and it means two different things: a
       scrolling column never compresses its children, so the card resolves
       to its content - every row - while the column that fits compresses
       it to the room left. */
    panel_draw(st, PANEL_TABLE, mem, "grow");
    status_bar(app, st);
}

static inline void layout(RC_App *app, void *userData)
{
    AppState *st = (AppState *)userData;
    RC_Style s = rcGetStyle();

    /* SAFE AREA. A phone draws the window edge to edge, UNDER the status bar and
       the home indicator, and nothing moves content out of the way for you. Ask for
       the margins and spend them ONCE, at the root: they belong to the window, not
       to any widget, and rcViewport().safe hands them over ALREADY IN LAYOUT UNITS.
       They are {0,0,0,0} on a desktop, so this is one code path everywhere. */
    RC_Viewport view = rcViewport();
    RC_Insets safe = view.safe;
    RC_Arena *mem = rcAppArena(app);
    /* Two bools, one idea each: `stacked` is about WIDTH, `pageScrolls`
       about HEIGHT. */
    bool stacked     = view.width < SYS_CHART_ROW_W;
    bool pageScrolls = page_scrolls(st);

    rcColumn(.id = "root", .bg = s.background, .pt = (uint16_t)(safe.top),
             .pb = (uint16_t)(safe.bottom), .pl = (uint16_t)(safe.left),
             .pr = (uint16_t)(safe.right), .w = "grow", .h = "grow") {
        titlebar(st);
        /* Two declarations of the same page, because .scroll is a char[2] that
           cannot take a ternary - and because a plain column compresses its
           growing children into the window while a scrolling one never does. */
        if (pageScrolls) {
            rcColumn(.id = "page", .gap = 12, .p = 14, .scroll = "v",
                     .w = "grow", .h = "grow") {
                page_body(app, st, mem, stacked);
            }
            rcScrollbar("page");
        } else {
            rcColumn(.id = "page", .gap = 12, .p = 14, .w = "grow", .h = "grow") {
                page_body(app, st, mem, stacked);
            }
        }
    }
}

#endif /* APP_APP_H */
