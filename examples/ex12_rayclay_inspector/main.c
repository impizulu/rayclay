/*
    ex12_rayclay_inspector - a live diagnostics harness built with RayClay.

    A health strip across the top, a control column on the left, and a tabbed
    diagnostics pane (Metrics / System / Scheduler / Log) on the right behind a
    draggable divider. Below INSP_SPLIT_MIN_W the same content reflows into one
    scrolling column. "Pop out" moves the pane into a second window.

    Shows rcSetLogSink, rcClipboardSet, rcBeginSplitPane, rcBeginTable, rcChart,
    rcSparkline, rcProcessCpuPercent / rcProcessMemoryBytes, rcClipSlotCounts,
    rcWindowSchedStats and rcAppOpenWindow.

    Build target: rayclay_ex12_rayclay_inspector. Honours RAYCLAY_MAX_FRAMES.
*/

#include "rayclay.h"
#include "inspector_log.h"

/* Font slots, in load order into RC_AppOptions.fontSizes: the index is .font. */
typedef enum {
    F_SMALL = 0,   /* log lines, muted captions */
    F_BODY,        /* labels, values            */
    F_HEAD,        /* panel + section headings   */
    F_TITLE,       /* the top strip title        */
    F_COUNT
} InspFont;

/* Rolling history depth for the resource graphs (samples per series). At the
   render cadence (~10 Hz) that spans ~12 s; at the OS cadence (~1 Hz), ~2 min. */
#define INSP_HIST 120
/* Startup pushes to discard - see perf_warm. */
#define INSP_PERF_WARMUP 3

/* Clipboard export scratch. An export that outgrows it truncates and SAYS SO. */
#define INSP_EXPORT_CAP 32768

/* The split arm's floor, in logical px. The app branches on AVAILABLE SPACE
   (rcViewport()) and never on the OS, so a desktop window dragged narrow lays out
   exactly as a phone does; below either number it is one scrolling column carrying
   the same controls and the same panels, nothing withheld.

   Each width is its pane's widest unwrapped row plus INSP_SCROLL_GUTTER, the gutter
   a scrolling column reserves so rcScrollbar floats clear of the cards' border. The
   diagnostics floor holds the tab row WITH the pop-out control and a readable plot. */
#define INSP_SCROLL_GUTTER  14
#define INSP_CONTROLS_MIN_W (443 + INSP_SCROLL_GUTTER)
#define INSP_SIDEBAR_MIN_W  (420 + INSP_SCROLL_GUTTER)
#define INSP_SPLIT_MIN_W    (16 + INSP_CONTROLS_MIN_W + 6 + INSP_SIDEBAR_MIN_W + 16)
#define INSP_SPLIT_MIN_H    691
/* Under this width the log table drops SEQ and LEVEL rather than scroll
   sideways, and says so with a "+2 columns" chip. */
#define INSP_LOG_FULL_W     404

/* The watch window: the diagnostics pane in a surface of its own. It can be
   dragged to any size, so its creation minimum is the only place this app gets to
   say "narrower than this and my content stops working". */
#define INSP_WATCH_PAD    12
#define INSP_WATCH_W      620
#define INSP_WATCH_H      640
#define INSP_WATCH_MIN_W  (INSP_SIDEBAR_MIN_W + 2 * INSP_WATCH_PAD)
#define INSP_WATCH_MIN_H  360

/* A dialog is FIT-sized, so nothing above it bounds it: the body takes its design
   size where the window affords it and what the viewport leaves otherwise. */
#define INSP_DIALOG_MARGIN  (2 * (20 + 1) + 2 * 16)
#define INSP_EXPORT_CHROME  126

/* The frame's geometry, decided once at the top of layout() and handed down. */
typedef struct {
    bool  stacked;    /* one scrolling column instead of the split            */
    float dialog_w;   /* the widest a dialog body may be, viewport minus chrome */
    float dialog_h;   /* the tallest a dialog body may be before it scrolls    */
    float log_max_h;  /* the stacked log's ceiling before its own scroller    */
    float log_w;      /* the width the log table gets; picks its column set   */
} InspArm;

/* Pane tabs. INSP_TAB_COUNT is the array length and never a tab. */
typedef enum {
    INSP_TAB_PERF = 0,
    INSP_TAB_SYS,
    INSP_TAB_SCHED,
    INSP_TAB_LOG,
    INSP_TAB_COUNT
} InspTab;

/* Severity - the only thing in this app that earns a colour. */
typedef enum {
    INSP_OK = 0,   /* nothing to report - the dot stays the muted text gray */
    INSP_WARN,     /* amber */
    INSP_BAD       /* the style's danger red */
} InspHealth;

typedef struct {
    long  frame;              /* advanced once per update; proves the loop is live */

    /* Inspector controls. Every one changes what the tool shows or produces. */
    int   clicks;            /* click-registered probe: "did that click land?"   */
    int   tab;               /* InspTab: which diagnostic the pane is showing    */
    bool  darkMode;          /* theme toggle -> rcSetStyle AND rcWindowSetClearColor */
    bool  barShown;          /* the MAIN window's titlebar fold; see layout()    */
    bool  watchBarShown;     /* the WATCH window's own fold - per surface, not app-wide */
    bool  showDetail;        /* log table: add the SEQ and LEVEL columns         */
    int   levelFilter;       /* log: 0 = all, 1 = warnings and errors, 2 = errors */
    int   srcFilter;         /* log: 0 = both sources, 1 = [APP], 2 = [RAY]      */
    float histFrac;          /* [0,1] share of the history the graphs plot       */
    bool  exportOpen;        /* the EXPORT modal                                 */
    bool  inspectOpen;       /* the non-modal live INSPECT panel                 */
    bool  inspectSticky;     /* -> .noBackdropDismiss (keeps it open)            */
    int   exportKind;        /* 0 = the filtered log, 1 = the metrics as CSV     */
    int   exportCopies;      /* successful rcClipboardSet calls this run         */
    bool  exportTrunc;       /* the buffer filled before the data ran out        */
    int   exportLen;         /* bytes live in exportBuf                          */
    char  exportBuf[INSP_EXPORT_CAP];

    int   trig_badfont;      /* >0: draw one out-of-range-font element, then decrement */

    InspectorLog log;        /* rcSetLogSink target + the app's own [APP] notes */

    /* Render metrics, ~10 Hz. Stored chronologically (oldest first) so rcChart
       plots them with no ring-to-linear copy. */
    float  perf_fps[INSP_HIST];
    float  perf_ms[INSP_HIST];      /* frame time, milliseconds        */
    int    perf_len;                /* live samples (<= INSP_HIST)     */
    /* Startup frames are not the app running, and both chart axes auto-fit, so one
       startup outlier sets the SCALE and flattens everything else. Discard them. */
    int    perf_warm;               /* pushes still to discard          */
    double perf_next_at;            /* next sample, on the app clock   */

    /* OS metrics, ~1 Hz. rcProcessCpuPercent accounts CPU since its PREVIOUS
       call, so it is read on this coarse cadence and nowhere else. */
    float  sys_cpu[INSP_HIST];      /* CPU %, clamped >= 0 for the plot */
    float  sys_mem[INSP_HIST];      /* resident memory, MB             */
    int    sys_len;
    double sys_next_at;
    float  cur_cpu;                 /* last CPU% reading (< 0 => unavailable, e.g. web)  */
    float  cur_mem_mb;              /* last resident MB   (< 0 => platform offers none)  */

    /* Sampled per FRAME and stored as a DELTA: the raw values only ramp, and a wake
       loop is a rate that spikes, not a total that grows. */
    float    sched_spur[INSP_HIST]; /* spurious wakes since last frame */
    float    sched_adm[INSP_HIST];  /* frames admitted since last frame */
    int      sched_len;
    uint64_t sched_prev_spur;       /* previous sample, for differencing */
    uint64_t sched_prev_adm;
    uint64_t sched_prev_waits;
    int      sched_dry;             /* consecutive samples with NO new park */
    RC_SchedStats sched_cur;        /* last snapshot; the rows read this */
    bool     continuous;            /* the render-mode toggle, see sched_panel */

    float cur_arena_occ;            /* scratch-arena occupancy, read end-of-layout */

    int   last_win_w, last_win_h;   /* resize detection for the [APP] log */
    long  last_total;               /* log.total at last follow; drives rcScrollToBottom */
    float split_frac;               /* pane-1 (controls) share of the split, drag-updated;
                                       kept across a reflow into the stack, where it is idle */

    /* NULL while the pane is docked here. The geometry is READ BACK every frame the
       window draws: the manager clamps what the user drags. */
    RC_Window *watch;
    bool       watchRefused;        /* an open failed on a desktop that said it could */
    bool       watchGeom;           /* the four below have been read back at least once */
    int        watchX, watchY, watchW, watchH;

    /* Frames actually PRESENTED - the render witness; see main(). */
    unsigned long frames_drawn;
} AppState;

/* rcSetLogSink target. `msg` is valid only for the call, so the ring COPIES it.
   It can fire mid-frame, so this does one cheap thing and never calls back in. */
static void on_log(RC_LogLevel level, const char *msg, void *user)
{
    AppState *st = (AppState *)user;
    insp_log_push(&st->log, INSP_SRC_RAY, (int)level, msg);
}

/* frameEndCallback fires on a frame that was actually PRESENTED, never on an idle
   frame that was skipped - which is what makes a count here a render witness. */
static void frame_end(RC_App *app, void *userData)
{
    AppState *st = (AppState *)userData;
    (void)app;
    st->frames_drawn++;
}

/* Append one [APP] line. The ring copies the bytes, so an arena string is fine. */
static void applog(AppState *st, RC_String s)
{
    insp_log_push_len(&st->log, INSP_SRC_APP, RC_LOG_INFO, s.chars, s.length);
}

static float clamp01(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

/* How many of the NEWEST samples the graphs plot; never below the two a line needs. */
static int hist_window(const AppState *st, int len)
{
    int n = (int)((float)len * st->histFrac + 0.5f);
    if (len < 2)
        return len;
    return n < 2 ? 2 : n;
}

/* Colour here is signal, not decoration: neutral gray surfaces, BLUE only for
   selection, AMBER and RED only for severity. Overridden field by field, never
   built from a zeroed struct, so a style field added tomorrow gets its default. */
static RC_Style insp_style(bool dark)
{
    RC_Style s = dark ? rcStyleDark() : rcStyleLight();

    if (dark) {
        s.background = RC_ZINC_950;
        s.surface    = RC_ZINC_900;
        s.surfaceAlt = RC_ZINC_800;
        s.chrome     = RC_ZINC_900;
        s.text       = RC_ZINC_50;
        s.textMuted  = RC_ZINC_400;
        s.border     = RC_ZINC_700;
    } else {
        s.background = RC_ZINC_100;
        s.surface    = RC_WHITE;
        s.surfaceAlt = RC_ZINC_50;
        s.chrome     = RC_ZINC_200;
        s.text       = RC_ZINC_900;
        s.textMuted  = RC_ZINC_500;
        s.border     = RC_ZINC_300;
    }
    s.primary      = RC_BLUE_600;
    s.primaryHover = RC_BLUE_500;
    /* The one severity colour this app writes SENTENCES in, so it has to read on
       both grounds: AMBER_500 falls to about 2:1 against a white surface. */
    s.warning      = dark ? RC_AMBER_500 : RC_AMBER_700;
    return s;
}

/* The rose a SENTENCE is written in, which is not the rose a BUTTON is filled with:
   s.danger has to carry white on a filled button, and the same value read as 13 px
   text on a dark panel falls under AA contrast. No value serves both roles. */
static RC_Color danger_ink(bool dark)
{
    return dark ? RC_ROSE_500 : rcGetStyle().danger;
}

/* The severity colour for a dot or a sentence. */
static RC_Color health_color(bool dark, int health)
{
    RC_Style s = rcGetStyle();
    return health == INSP_BAD  ? danger_ink(dark)
         : health == INSP_WARN ? s.warning
         :                       s.textMuted;
}

/* A panel label, not a headline: the data carries the contrast, not the chrome. */
static void section_heading(const char *title)
{
    RC_Style s = rcGetStyle();
    rcTextC(title, .font = F_HEAD, .color = s.textMuted);
}

/* ONE predicate, so an export can never disagree with the panel it came from. */
static bool log_visible(const AppState *st, const InspLogLine *l)
{
    if (st->levelFilter == 1 && l->level != RC_LOG_WARNING && l->level != RC_LOG_ERROR)
        return false;
    if (st->levelFilter == 2 && l->level != RC_LOG_ERROR)
        return false;
    if (st->srcFilter == 1 && l->source != INSP_SRC_APP)
        return false;
    if (st->srcFilter == 2 && l->source != INSP_SRC_RAY)
        return false;
    return true;
}

static int log_visible_count(const AppState *st)
{
    int count = insp_log_count(&st->log), n = 0;
    for (int i = 0; i < count; i++) {
        if (log_visible(st, insp_log_at(&st->log, i)))
            n++;
    }
    return n;
}

/* The worst thing the log holds, over every HELD line and not the visible ones: a
   health tile that went quiet because you filtered the errors out is no tile. */
static void log_severity(const AppState *st, int *errors, int *warnings)
{
    int count = insp_log_count(&st->log);

    *errors = *warnings = 0;
    for (int i = 0; i < count; i++) {
        int level = insp_log_at(&st->log, i)->level;
        if (level == RC_LOG_ERROR)
            (*errors)++;
        else if (level == RC_LOG_WARNING)
            (*warnings)++;
    }
}

static const char *level_name(int level)
{
    return level == RC_LOG_ERROR   ? "ERROR"
         : level == RC_LOG_WARNING ? "WARN"
         :                           "info";
}

/* A fixed buffer and a bounded appender, never concatenated arena strings: the
   arena is PER-FRAME and an export is hundreds of rcFormat calls in one frame. */

/* Appends until full, then returns false so the caller can report a truncation. */
static bool exp_add(AppState *st, RC_String s)
{
    int i = 0;
    while (i < s.length && st->exportLen < INSP_EXPORT_CAP - 1)
        st->exportBuf[st->exportLen++] = s.chars[i++];
    st->exportBuf[st->exportLen] = '\0';
    return i == s.length;
}

static bool exp_lit(AppState *st, const char *literal)
{
    return exp_add(st, rcStringFromCStr(literal));
}

/* Render the CURRENTLY VISIBLE log to exportBuf, in the panel's own order. */
static void export_build_log(RC_App *app, AppState *st)
{
    RC_Arena *mem = rcAppArena(app);
    int count = insp_log_count(&st->log);
    bool ok;

    st->exportLen = 0;
    st->exportBuf[0] = '\0';
    /* SPACE-ALIGNED, not tab-separated: RayClay draws no glyph and no advance for
       '\t', so a tabbed line renders as one run-on string in the preview. */
    ok = exp_lit(st, "# RayClay Inspector - log export\n"
                     "# seq  src  level  message\n");
    for (int i = 0; i < count && ok; i++) {
        const InspLogLine *l = insp_log_at(&st->log, i);
        if (!log_visible(st, l))
            continue;
        ok = exp_add(st, rcFormat(mem, "%-5ld %-4s %-5s  %s\n", l->seq,
                                   l->source == INSP_SRC_RAY ? "RAY" : "APP",
                                   level_name(l->level), l->msg));
    }
    st->exportTrunc = !ok;
}

/* Both metric histories as CSV, as two tables: they are sampled on DIFFERENT
   cadences, so a shared row index would imply a correspondence that does not exist. */
static void export_build_metrics(RC_App *app, AppState *st)
{
    RC_Arena *mem = rcAppArena(app);
    bool ok;

    st->exportLen = 0;
    st->exportBuf[0] = '\0';
    ok = exp_lit(st, "# RayClay Inspector - metrics export\n"
                     "# render metrics, sampled ~10 Hz\n"
                     "sample,fps,frame_ms\n");
    for (int i = 0; i < st->perf_len && ok; i++)
        ok = exp_add(st, rcFormat(mem, "%d,%.1f,%.3f\n", i,
                                   st->perf_fps[i], st->perf_ms[i]));
    if (ok)
        ok = exp_lit(st, "\n# OS metrics, sampled ~1 Hz\n"
                         "sample,cpu_percent,resident_mb\n");
    for (int i = 0; i < st->sys_len && ok; i++)
        ok = exp_add(st, rcFormat(mem, "%d,%.1f,%.2f\n", i,
                                   st->sys_cpu[i], st->sys_mem[i]));
    st->exportTrunc = !ok;
}

static void export_build(RC_App *app, AppState *st)
{
    if (st->exportKind == 0)
        export_build_log(app, st);
    else
        export_build_metrics(app, st);
}

/* One key/value row, with the label column fixed so the values line up. */
static void inspect_row(const char *label, RC_String value, float labelW)
{
    RC_Style s = rcGetStyle();
    rcRow(.gap = 10, .align = "cl", .w = "grow") {
        rcBox(.wType = RC_PX(labelW)) { rcTextC(label, .font = F_SMALL, .color = s.textMuted); }
        rcBox(.align = "cl", .w = "grow") { rcText(value, .font = F_SMALL, .color = s.text); }
    }
}

/* Re-read from public getters EVERY FRAME, and non-modal so they answer live. */
static void inspect_panel(RC_App *app, AppState *st, const InspArm *arm)
{
    RC_Style      s    = rcGetStyle();
    RC_Arena     *mem  = rcAppArena(app);
    RC_Dimensions d    = rcGetWindowDimensions();
    RC_ZoomMode   zm   = rcWindowZoomMode(rcAppMainWindow(app));

    /* The design width where the window affords it, less where it does not. */
    float w      = arm->dialog_w < 480.0f ? arm->dialog_w : 480.0f;
    float labelW = arm->stacked ? 108.0f : 148.0f;

    rcColumn(.id = "InspectBody", .gap = 6, .scroll = "v", .wType = RC_PX(w),
             .hMax = arm->dialog_h) {
        rcTextL("LIVE STATE", .font = F_HEAD, .color = s.text);
        /* Logical pixels: the space the layout is actually solved in. */
        inspect_row("window (logical)",
                    rcFormat(mem, "%.0f x %.0f px", d.width, d.height), labelW);
        inspect_row("zoom",
                    rcFormat(mem, "%.0f%%  -  %s", rcWindowZoom(rcAppMainWindow(app)) * 100.0f,
                              zm == RC_ZOOM_OPTICAL ? "optical magnify" : "layout reflow"),
                    labelW);
        inspect_row("frame",
                    rcFormat(mem, "%.0f fps, %.2f ms",
                              rcAppFPS(app), rcWindowFrameTime(rcAppMainWindow(app)) * 1000.0f), labelW);
        inspect_row("scratch arena",
                    rcFormat(mem, "%d%% of %d bytes",
                              (int)(st->cur_arena_occ * 100.0f + 0.5f),
                              (int)mem->bufferLength), labelW);
        inspect_row("log ring",
                    rcFormat(mem, "%ld pushed, %d held, %d shown",
                              st->log.total, insp_log_count(&st->log),
                              log_visible_count(st)), labelW);
        /* No split in the stacked arm, so say what the layout IS. */
        inspect_row("split pane",
                    arm->stacked
                        ? rcFormat(mem, "stacked: window under %d x %d px",
                                   INSP_SPLIT_MIN_W, INSP_SPLIT_MIN_H)
                        : rcFormat(mem, "%.0f%% controls / %.0f%% diagnostics",
                                   st->split_frac * 100.0f, (1.0f - st->split_frac) * 100.0f),
                    labelW);
        /* False in a stock build: the debug overlay is opt-in at build time. */
        inspect_row("debug tools",
                    rcStringFromCStr(rcAppIsDebugEnabled(app)
                                     ? "enabled (-DRC_DEBUG_TOOLS=1)"
                                     : "off (the default; opt in at build time)"),
                    labelW);
        inspect_row("clicks registered", rcFormat(mem, "%d", st->clicks), labelW);
        rcTextL("Resize the window, zoom with the primary modifier and +/-/0, or "
                "drag the split - every row re-reads the library each frame.",
                .font = F_SMALL, .color = s.textMuted);
    }
    /* Floating: inside the modal scope, or it layers against the root. */
    rcScrollbar("InspectBody");
}

/* The controls a narrow window cannot keep on one line with their readouts, placed
   by controls() in the shape its arm affords: side by side, or label over widget. */
static void view_detail_checkbox(RC_App *app, AppState *st)
{
    if (rcCheckbox("cb_detail", "Seq + level columns", &st->showDetail))
        applog(st, rcFormat(rcAppArena(app), "log columns = %s",
                             st->showDetail ? "seq/level/src/message" : "src/message"));
}

static void view_theme_toggle(RC_App *app, AppState *st)
{
    RC_Style s = rcGetStyle();
    rcTextL("Theme", .font = F_SMALL, .color = s.textMuted);
    if (rcToggle("tg_dark", &st->darkMode))
        applog(st, rcFormat(rcAppArena(app), "theme = %s",
                             st->darkMode ? "dark" : "light"));
    rcTextC(st->darkMode ? "Dark" : "Light", .font = F_SMALL, .color = s.textMuted);
}

static void graph_window_slider(AppState *st)
{
    rcSlider("sl_hist", &st->histFrac, 0.1f, 1.0f);
}

static void graph_window_count(RC_App *app, AppState *st)
{
    RC_Style  s   = rcGetStyle();
    RC_String win = rcFormat(rcAppArena(app), "newest %d of %d samples",
                             hist_window(st, st->perf_len), st->perf_len);
    rcText(win, .font = F_SMALL, .color = s.textMuted);
}

static void source_combo(RC_App *app, AppState *st)
{
    static const char *const sources[] = { "APP + RAY", "APP only", "RAY only" };
    if (rcCombo("cb_source", &st->srcFilter, sources, 3))
        applog(st, rcFormat(rcAppArena(app), "log source filter = %s",
                             sources[st->srcFilter]));
}

/* The filter's effect, as a number: a filter you cannot see the result of is
   indistinguishable from one that is not wired up. */
static void source_count(RC_App *app, AppState *st)
{
    RC_Style  s     = rcGetStyle();
    RC_String shown = rcFormat(rcAppArena(app), "%d of %d lines shown",
                               log_visible_count(st), insp_log_count(&st->log));
    rcText(shown, .font = F_SMALL, .color = s.textMuted);
}

static void export_inspect_buttons(RC_App *app, AppState *st)
{
    if (rcButton("btn_export", "Export...", RC_BTN_DEFAULT)) {
        st->exportOpen = true;
        export_build(app, st);
        applog(st, rcStringFromCStr("export dialog opened"));
    }
    if (rcButton("btn_inspect", "Live inspect", RC_BTN_DEFAULT)) {
        st->inspectOpen = true;
        applog(st, rcStringFromCStr("live inspect panel opened"));
    }
}

static void export_copies(RC_App *app, AppState *st)
{
    RC_Style  s      = rcGetStyle();
    RC_String copied = rcFormat(rcAppArena(app), "clipboard copies: %d",
                                st->exportCopies);
    rcText(copied, .font = F_SMALL, .color = s.textMuted);
}

/* Each control logs on a DISCRETE change, never per frame. */
static void controls(RC_App *app, AppState *st, const InspArm *arm)
{
    RC_Style s = rcGetStyle();

    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading("BUTTONS");
        rcRow(.gap = 10, .align = "cl") {
            if (rcButton("btn_primary", "Primary", RC_BTN_PRIMARY)) {
                st->clicks++;
                applog(st, rcFormat(rcAppArena(app), "click ok: btn 'Primary' (clicks=%d)", st->clicks));
            }
            if (rcButton("btn_default", "Default", RC_BTN_DEFAULT)) {
                st->clicks++;
                applog(st, rcFormat(rcAppArena(app), "click ok: btn 'Default' (clicks=%d)", st->clicks));
            }
            if (rcButton("btn_reset", "Reset", RC_BTN_DANGER)) {
                st->clicks = 0;
                applog(st, rcStringFromCStr("click ok: btn 'Reset' (clicks=0)"));
            }
        }
    }

    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading("VIEW");
        /* rcCheckbox and rcRadio put their id on the row that holds the glyph AND
           its label, so the hit target is the whole label, not the 18 px box. */
        if (arm->stacked) {
            rcRow(.gap = 16, .align = "cl") { view_detail_checkbox(app, st); }
            rcRow(.gap = 16, .align = "cl") { view_theme_toggle(app, st); }
        } else {
            rcRow(.gap = 16, .align = "cl") {
                view_detail_checkbox(app, st);
                view_theme_toggle(app, st);
            }
        }

        /* A FRACTION, not a count: the history is still filling at second 3. */
        if (arm->stacked) {
            rcRow(.gap = 12, .align = "cl", .w = "grow") {
                rcTextL("Graph window", .font = F_SMALL, .color = s.textMuted);
                rcBox(.w = "grow") {}
                graph_window_count(app, st);
            }
            rcBox(.w = "grow") { graph_window_slider(st); }
        } else {
            rcRow(.gap = 12, .align = "cl") {
                rcTextL("Graph window", .font = F_SMALL, .color = s.textMuted);
                rcBox(.w = "180px") { graph_window_slider(st); }
                graph_window_count(app, st);
            }
        }
    }

    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading("LOG FILTERS");
        rcRow(.gap = 16, .align = "cl") {
            rcTextL("Level", .font = F_SMALL, .color = s.textMuted);
            if (rcRadio("rb_all", "All", &st->levelFilter, 0))
                applog(st, rcStringFromCStr("log level filter = all"));
            if (rcRadio("rb_warn", "Warnings+", &st->levelFilter, 1))
                applog(st, rcStringFromCStr("log level filter = warnings and errors"));
            if (rcRadio("rb_err", "Errors", &st->levelFilter, 2))
                applog(st, rcStringFromCStr("log level filter = errors only"));
        }

        if (arm->stacked) {
            rcRow(.gap = 12, .align = "cl", .w = "grow") {
                rcTextL("Source", .font = F_SMALL, .color = s.textMuted);
                rcBox(.w = "grow") {}
                source_count(app, st);
            }
            rcBox(.w = "grow") { source_combo(app, st); }
        } else {
            rcRow(.gap = 12, .align = "cl") {
                rcTextL("Source", .font = F_SMALL, .color = s.textMuted);
                rcBox(.w = "170px") { source_combo(app, st); }
                source_count(app, st);
            }
        }
    }

    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading("EXPORT AND INSPECT");
        if (arm->stacked) {
            rcRow(.gap = 10, .align = "cl") { export_inspect_buttons(app, st); }
            export_copies(app, st);
        } else {
            rcRow(.gap = 10, .align = "cl") {
                export_inspect_buttons(app, st);
                export_copies(app, st);
            }
        }

        /* MODAL: freezing a still-logging app is what makes the byte count true. */
        if (rcBeginModal("dlg_export", &st->exportOpen)) {
            rcTextL("Export to clipboard", .font = F_HEAD, .color = s.text);
            rcRow(.gap = 16, .align = "cl") {
                if (rcRadio("ex_log", "Filtered log", &st->exportKind, 0))
                    export_build(app, st);
                if (rcRadio("ex_csv", "Metrics (CSV)", &st->exportKind, 1))
                    export_build(app, st);
            }
            RC_String size = rcFormat(rcAppArena(app), "%d bytes%s",
                                       st->exportLen,
                                       st->exportTrunc ? "  (TRUNCATED - buffer full)" : "");
            rcText(size, .font = F_SMALL,
                    .color = st->exportTrunc ? danger_ink(st->darkMode) : s.textMuted);

            /* A preview, because a clipboard copy is otherwise invisible. */
            float pw = arm->dialog_w < 560.0f ? arm->dialog_w : 560.0f;
            float ph = arm->dialog_h - (float)INSP_EXPORT_CHROME;
            if (ph > 220.0f) ph = 220.0f;
            if (ph < 96.0f)  ph = 96.0f;    /* a 4-line floor beats an empty box */
            rcColumn(.id = "ExportPreview", .bg = s.surfaceAlt, .p = 10, .scroll = "v",
                     .borderRadius = "all-md", .wType = RC_PX(pw), .hType = RC_PX(ph)) {
                rcTextC(st->exportBuf, .font = F_SMALL, .color = s.text);
            }
            /* Floating too - the same scope rule as the inspect panel's bar. */
            rcScrollbar("ExportPreview");

            rcRow(.gap = 10, .align = "cl") {
                if (rcButton("ex_copy", "Copy", RC_BTN_PRIMARY)) {
                    /* exp_add null-terminates, so the C-string call is safe. */
                    rcClipboardSet(st->exportBuf);
                    st->exportCopies++;
                    applog(st, rcFormat(rcAppArena(app), "copied %d bytes of %s to the clipboard",
                                         st->exportLen,
                                         st->exportKind == 0 ? "log" : "metrics CSV"));
                    st->exportOpen = false;
                }
                if (rcButton("ex_cancel", "Cancel", RC_BTN_DEFAULT))
                    st->exportOpen = false;
            }
            rcEndModal();
        }

        /* NON-MODAL, so the numbers move while you work behind it.
           RC_MODALITY_NON_MODAL and .noBackdropDismiss are a PAIR: without the
           second, the first click outside closes it again. */
        if (rcBeginModal("insp_live", &st->inspectOpen,
                         .noBackdropDismiss = st->inspectSticky,
                         .modality          = RC_MODALITY_NON_MODAL)) {
            inspect_panel(app, st, arm);
            rcRow(.gap = 10, .align = "cl") {
                rcCheckbox("pp_sticky", "Stay open when I click away", &st->inspectSticky);
                if (rcButton("pp_close", "Close", RC_BTN_DEFAULT))
                    st->inspectOpen = false;
            }
            rcEndModal();
        }
    }

    /* Buttons that provoke a REAL RayClay diagnostic, so the [RAY] stream reddens
       on demand - which is what makes this a harness and not a mock-up. */
    rcColumn(.bg = s.surface, .gap = 12, .p = 16, .borderRadius = "all-xl", .w = "grow") {
        section_heading("ERROR TRIGGERS");
        rcRow(.gap = 10, .align = "cl") {
            if (rcButton("trig_font", "Invalid font", RC_BTN_DANGER)) {
                st->trig_badfont = 1;   /* draw one out-of-range-font element below */
                applog(st, rcStringFromCStr("trigger: invalid font slot (expect [RAY] error)"));
            }
            if (rcButton("trig_clear", "Clear log", RC_BTN_GHOST)) {
                st->log.total = 0;      /* reset the ring; nothing to free */
            }
        }
        /* An out-of-range font slot makes RayClay log an RC_LOG_ERROR and fall
           back to the default face. Drawn for one frame after the click. */
        if (st->trig_badfont > 0) {
            rcTextC("bad-font probe (invalid slot 240)", .font = 240,
                    .color = danger_ink(st->darkMode));
            st->trig_badfont--;
        }
    }
}

/* One resource row. Only a figure that can be WRONG spends a colour. */
static void resource_row(const char *id, const char *label, RC_String value,
                         float frac, RC_Color valueColor)
{
    RC_Style s = rcGetStyle();
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        rcBox(.w = "64px") { rcTextC(label, .font = F_SMALL, .color = s.textMuted); }
        rcBox(.w = "grow") { rcProgress(id, clamp01(frac)); }
        rcBox(.align = "cr", .w = "96px") { rcText(value, .font = F_SMALL, .color = valueColor); }
    }
}

/* PERFORMANCE panel: fps, frame time, the runner's scratch-arena occupancy and
   the clip-slot census, each with the worst value across the history window. */
static void resource_panel(RC_App *app, AppState *st, const InspArm *arm)
{
    RC_Style s = rcGetStyle();

    float fps = rcAppFPS(app);
    float ms  = rcWindowFrameTime(rcAppMainWindow(app)) * 1000.0f;

    float fps_min = fps, ms_max = ms;
    for (int i = 0; i < st->perf_len; i++) {
        if (st->perf_fps[i] < fps_min) fps_min = st->perf_fps[i];
        if (st->perf_ms[i]  > ms_max)  ms_max  = st->perf_ms[i];
    }

    /* "grow" in the split, where the panel has the pane to itself; "fit" inside
       the scrolling stack, where a viewport fraction answers to nothing on screen. */
    rcColumn(.id = "ResPanel", .bg = s.surface, .gap = 10, .p = 14,
             .borderRadius = "all-lg", .w = "grow", .h = arm->stacked ? "fit" : "grow") {
        rcRow(.align = "cl", .w = "grow") { section_heading("PERFORMANCE"); }
        /* Against a 120 target, so 60 sits mid-bar and the headroom shows. */
        resource_row("res_fps", st->continuous ? "FPS" : "wakes/s",
                     rcFormat(rcAppArena(app), "%.0f (min %.0f)", fps, fps_min),
                     fps / 120.0f, s.text);
        /* frame-time against a 33 ms (30 fps) ceiling: full bar == a slow frame. */
        resource_row("res_ms", "frame",
                     rcFormat(rcAppArena(app), "%.1f ms (max %.1f)", ms, ms_max),
                     ms / 33.0f, s.text);
        /* the runner's per-frame scratch arena; occupancy measured end-of-layout. */
        resource_row("res_arena", "scratch",
                     rcFormat(rcAppArena(app), "%d%%", (int)(st->cur_arena_occ * 100.0f + 0.5f)),
                     st->cur_arena_occ, s.text);
        /* THE CLIP-SLOT POOL - the one instrument here that reports a FAULT and not
           a cost. Every scroll container needs a slot to remember where it is
           scrolled to, and one refused a slot LOSES ITS OFFSET. */
        RC_ClipSlotCounts clip = rcClipSlotCounts();
        /* Refused NOW is red; over the pool at some point is amber, because those
           offsets are already spent. One severity drives the value AND the caption. */
        int clipHealth = clip.rejected > 0                  ? INSP_BAD
                       : clip.highWaterMark > clip.capacity ? INSP_WARN
                       :                                      INSP_OK;
        resource_row("res_clip", "clips",
                     rcFormat(rcAppArena(app), "%d/%d", clip.stored, clip.capacity),
                     clip.capacity > 0 ? (float)clip.stored / (float)clip.capacity : 0.0f,
                     /* plain until there is something to say - the strip's rule */
                     clipHealth == INSP_OK ? s.text : health_color(st->darkMode, clipHealth));
        /* Always drawn, healthy or not: a line that appears only on a fault
           reflows the panel as you read it. It claims only what the fields support
           - a pass can be refused while demand sits below capacity. */
        rcText(clipHealth == INSP_BAD
                   ? rcFormat(rcAppArena(app), "%d refused this pass - those scroll offsets are lost",
                              clip.rejected)
               : clipHealth == INSP_WARN
                   ? rcFormat(rcAppArena(app), "peak %d over a pool of %d - offsets were lost earlier",
                              clip.highWaterMark, clip.capacity)
               : rcAppIsDebugEnabled(app)
                   ? rcFormat(rcAppArena(app),
                              "peak %d - none refused this pass, inspector slots included",
                              clip.highWaterMark)
                   : rcFormat(rcAppArena(app), "peak %d - none refused this pass",
                              clip.highWaterMark),
                .font = F_SMALL, .color = health_color(st->darkMode, clipHealth));

        int pn = hist_window(st, st->perf_len);
        int po = st->perf_len - pn;
        /* The x axis is hidden - a rolling trend has no timestamps worth labelling
           - so the span is captioned here, at the 100 ms sample cadence. */
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            rcBox(.w = "grow") {}
            rcText(rcFormat(rcAppArena(app), "last %.1f s", (float)pn * 0.1f),
                    .font = F_SMALL, .color = s.textMuted, .wrap = "n");
        }
        /* Two series on SEPARATE y axes: fps reads high and frame time low, so a
           shared axis would flatten one. RC_Series BORROWS .y and .label until
           rcRender, which the static arrays and literals satisfy. rcChart GROWs,
           so it needs a sized box: the panel's leftover, or a fixed strip. */
        rcBox(.w = "grow", .h = arm->stacked ? "96px" : "grow", .hMin = 96.0f) {
            RC_Series ser[] = {
                { .y = st->perf_fps + po, .count = pn, .kind = RC_SERIES_LINE,
                  .color = s.primary,  .label = "fps" },
                { .y = st->perf_ms + po,  .count = pn, .kind = RC_SERIES_LINE,
                  .color = s.warning, .label = "ms",  .axis = 1 },
            };
            rcChart("perf_chart", ser, 2,
                     RC_LIT(RC_ChartOptions){ .x = { .hide = true },
                                              .y = { .grid = true },
                                              .legend = true,
                                              .tooltip = RC_CHART_TOOLTIP_NEAREST,
                                              .tooltipPlace = RC_TOOLTIP_PLACE_CORNER });
        }
    }
}

/* One SYSTEM row: label, value, and a live rcSparkline. `hist` is BORROWED until
   rcRender, which the static rings satisfy. @p tall is 36 px instead of 22: an AREA
   sparkline auto-fits, so past about 40 px a flat series fills its whole box. */
static void sys_row(const char *sparkId, const char *label, RC_String value,
                    const float *hist, int histLen, RC_Color color, bool tall)
{
    RC_Style s = rcGetStyle();
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        rcBox(.w = "40px") { rcTextC(label, .font = F_SMALL, .color = s.textMuted); }
        rcBox(.align = "cr", .w = "88px") { rcText(value, .font = F_SMALL, .color = s.text); }
        rcBox(.w = "grow", .h = tall ? "36px" : "22px") {
            rcSparkline(sparkId, hist, histLen,
                         RC_LIT(RC_SparklineOptions){ .kind = RC_SERIES_AREA,
                                                     .color = color });
        }
    }
}

/* SYSTEM panel: this process's CPU share and resident memory. Both read "n/a"
   where the platform offers no view - the panel never prints a sentinel. */
static void system_panel(RC_App *app, AppState *st, const InspArm *arm)
{
    RC_Style s = rcGetStyle();

    float cpu_peak = 0.0f;
    for (int i = 0; i < st->sys_len; i++) {
        if (st->sys_cpu[i] > cpu_peak) cpu_peak = st->sys_cpu[i];
    }

    RC_String cpu_val = st->cur_cpu < 0.0f
        ? rcStringFromCStr("n/a")
        : rcFormat(rcAppArena(app), "%.0f%% (pk %.0f)", st->cur_cpu, cpu_peak);
    RC_String mem_val = st->cur_mem_mb < 0.0f
        ? rcStringFromCStr("n/a")
        : rcFormat(rcAppArena(app), "%.1f MB", st->cur_mem_mb);

    /* "fit" in both arms: a card stretched around two strips reads as unfinished. */
    rcColumn(.id = "SysPanel", .bg = s.surface, .gap = 10, .p = 14,
             .borderRadius = "all-lg", .w = "grow", .h = "fit") {
        rcRow(.align = "cl", .w = "grow") { section_heading("SYSTEM"); }
        /* The same window as the chart, so both describe the same span of time. */
        int sn = hist_window(st, st->sys_len);
        int so = st->sys_len - sn;
        sys_row("spk_cpu", "CPU", cpu_val, st->sys_cpu + so, sn, s.primary,
                !arm->stacked);
        sys_row("spk_mem", "RAM", mem_val, st->sys_mem + so, sn, s.warning,
                !arm->stacked);
        /* rcProcessCpuPercent is a share of ONE core, like top, and it counts the
           WHOLE process - so a figure in the hundreds is routine, not a fault. */
        rcTextL("CPU is a share of ONE core, like top: over 100% means several "
                "cores. It counts the whole process - a software GL driver's "
                "worker threads included.",
                .font = F_SMALL, .color = s.textMuted);
    }
}

/* SCHEDULER panel: the frame-admission counters, live. A climbing `spurious` rate
   is the signature of a wake loop. Every counter is meaningless unless the app
   parks, which is why the mode toggle is part of the panel. */
static void sched_row(const char *label, RC_String value, RC_Color color)
{
    RC_Style s = rcGetStyle();
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        rcBox(.w = "104px") { rcTextC(label, .font = F_SMALL, .color = s.textMuted); }
        rcBox(.align = "cl", .w = "grow") { rcText(value, .font = F_SMALL, .color = color); }
    }
}

static void sched_panel(RC_App *app, AppState *st, const InspArm *arm)
{
    RC_Style      s   = rcGetStyle();
    RC_Arena     *mem = rcAppArena(app);
    RC_SchedStats sc  = st->sched_cur;

    /* The denominator is `waits`, never `admitted`: those are two different
       populations, and an app that parks a lot and draws rarely is healthy. */
    float wasted = sc.waits ? 100.0f * (float)sc.spurious / (float)sc.waits : 0.0f;

    /* Read from evidence, never from our own flag: RAYCLAY_RENDER_MODE overrides
       the app's choice at startup. The evidence is a DELTA - `waits` is cumulative,
       so `waits > 0` latches on the first park forever. */
    bool parking = st->sched_dry < INSP_HIST;

    /* Collapsed when the app does not park: every counter below is about the park
       loop, so in continuous mode they are a structural zero meaning "not
       applicable", and six zero rows invite exactly that misreading. */
    rcColumn(.id = "SchedPanel", .bg = s.surface, .gap = 8, .p = 14,
             .borderRadius = "all-lg", .w = "grow", .h = "fit") {
        rcRow(.gap = 10, .align = "cl", .w = "grow") {
            section_heading("SCHEDULER");
            rcTextC(parking ? "parking" : "not parking",
                    .font = F_SMALL, .color = parking ? s.primary : s.textMuted);
            rcBox(.w = "grow") {}
            if (rcButton("btn_mode", st->continuous ? "go on demand" : "go continuous",
                         RC_BTN_DEFAULT)) {
                st->continuous = !st->continuous;
                rcWindowSetContinuousRendering(rcAppMainWindow(app), st->continuous);
                insp_log_push(&st->log, INSP_SRC_APP, RC_LOG_INFO,
                              st->continuous
                                  ? "render mode -> CONTINUOUS (the scheduler stops parking)"
                                  : "render mode -> ON DEMAND (the counters come alive)");
            }
        }

        if (!parking) {
            /* One honest line instead of six zeros. */
            rcTextL("Counters are about PARKS, so they stay 0 while the app draws "
                    "every frame. Switch to on demand to see them move.",
                    .font = F_SMALL, .color = s.textMuted);
        } else {
            int n = hist_window(st, st->sched_len);
            int o = st->sched_len - n;
            /* The label carries the unit: these are per-FRAME deltas. */
            sys_row("spk_spur", "spur/frame", rcFormat(mem, "%llu total", (unsigned long long)sc.spurious),
                    st->sched_spur + o, n, s.warning, !arm->stacked);
            sys_row("spk_adm", "adm/frame", rcFormat(mem, "%llu total", (unsigned long long)sc.admitted),
                    st->sched_adm + o, n, s.primary, !arm->stacked);

            /* No amber on this figure: this app wakes itself every second to stay
               live, and each self-wake costs one park charged as spurious, so a
               warning keyed on it could never go out. The number stays. */
            sched_row("wasted parks",
                      rcFormat(mem, "%.0f%%  (%llu of %llu)", wasted,
                               (unsigned long long)sc.spurious,
                               (unsigned long long)sc.waits),
                      s.text);
            rcTextL("this app wakes itself 1x/sec to stay live; each self-wake "
                    "costs one extra park, so the % above is its own cost.",
                    .font = F_SMALL, .color = s.textMuted);
            sched_row("repaints", rcFormat(mem, "%llu drawn outside the admit path",
                                           (unsigned long long)sc.refreshRepaints), s.textMuted);

            /* rcFrameReasonName labels each reason. RC_FRAME_REASON_COUNT is the
               array length and not a reason, so the loop stops BELOW it. */
            rcRow(.gap = 10, .align = "cl", .w = "grow") {
                for (int i = 0; i < RC_FRAME_REASON_COUNT; i++) {
                    if (!sc.byReason[i]) continue;
                    rcTextC(rcFormat(mem, "%s %llu", rcFrameReasonName((RC_FrameReason)i),
                                     (unsigned long long)sc.byReason[i]).chars,
                            .font = F_SMALL, .color = s.textMuted);
                }
            }
            /* Gated on the counter, so the note retires itself once one is wired. */
            if (!sc.byReason[RC_FRAME_EXPOSE]) {
                rcTextL("expose reads 0 because nothing raises it yet - that is "
                        "\"cannot report\", not \"did not happen\".",
                        .font = F_SMALL, .color = s.textMuted);
            }
        }
    }
}

/* The live log as a table: a sticky header, oldest-first terminal order, and a body
   scrolled by the table's own id - the id rcScrollbar and rcScrollToBottom name. */
static void log_panel(RC_App *app, AppState *st, const InspArm *arm)
{
    RC_Style s = rcGetStyle();

    /* "grow" takes the pane; inside the stack, fit between two bounds instead. */
    rcColumn(.id = "LogPanel", .bg = s.surface, .gap = 8, .p = 14,
             .borderRadius = "all-lg", .w = "grow", .h = arm->stacked ? "fit" : "grow",
             .hMin = arm->stacked ? 160.0f : 0.0f,
             .hMax = arm->stacked ? arm->log_max_h : 0.0f) {
        int held = insp_log_count(&st->log), shown = log_visible_count(st);
        /* THE TABLE ALWAYS FITS ITS WIDTH: a narrow pane drops SEQ and LEVEL
           rather than scroll sideways. See INSP_LOG_FULL_W. */
        bool detail  = st->showDetail && arm->log_w >= (float)INSP_LOG_FULL_W;
        int  dropped = (st->showDetail && !detail) ? 2 : 0;

        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            section_heading("LOG");
            /* A log that silently omits its own contents is a tool that lies. */
            RC_String n = shown == held
                ? rcFormat(rcAppArena(app), "%d lines", held)
                : rcFormat(rcAppArena(app), "%d of %d lines (filtered)", shown, held);
            rcText(n, .font = F_SMALL,
                    .color = shown == held ? s.textMuted : s.warning);
            rcBox(.w = "grow") {}
            /* A table that silently drops columns is one the reader misreads. */
            if (dropped) {
                rcBox(.id = "log_hidden", .bg = s.surfaceAlt, .px = 7, .py = 2,
                      .borderRadius = "all-full",
                      .tooltip = "SEQ and LEVEL need a wider pane.") {
                    rcText(rcFormat(rcAppArena(app), "+%d columns", dropped),
                            .font = F_SMALL, .color = s.textMuted);
                }
            }
        }
        /* "tl" top-aligns a wrapped message; the checkbox changes the column SET. */
        RC_TableColumn wide[] = {
            { .header = "SEQ",     .align = "tl", .w = "48px" },
            { .header = "SRC",     .align = "tl", .w = "52px" },
            { .header = "LEVEL",   .align = "tl", .w = "56px" },
            { .header = "MESSAGE", .align = "tl", .w = "grow" },
        };
        RC_TableColumn narrow[] = {
            { .header = "SRC",     .align = "tl", .w = "52px" },
            { .header = "MESSAGE", .align = "tl", .w = "grow" },
        };
        RC_TableColumn *cols = detail ? wide : narrow;
        int ncols = detail ? 4 : 2;

        rcBox(.bg = s.surfaceAlt, .borderRadius = "all-md", .w = "grow", .h = "grow") {
            if (rcBeginTable("LogScroll", cols, ncols, RC_LIT(RC_TableOptions){0})) {
                /* An empty body says WHY it is empty, in the MESSAGE column: a
                   header over a blank box reads as a table that failed to draw. */
                if (shown == 0) {
                    rcTableRow();
                    for (int k = 1; k < ncols; k++)
                        rcTableNext();
                    rcTextC(held > 0 ? "every line is hidden by the filters above"
                                     : "nothing logged yet",
                            .font = F_SMALL, .color = s.textMuted);
                }
                for (int i = 0; i < held; i++) {
                    const InspLogLine *l = insp_log_at(&st->log, i);
                    if (!log_visible(st, l))
                        continue;
                    bool ray = l->source == INSP_SRC_RAY;
                    RC_Color c = l->level == RC_LOG_ERROR   ? danger_ink(st->darkMode)
                                 : l->level == RC_LOG_WARNING ? s.warning
                                 : ray                        ? s.textMuted
                                 :                              s.text;
                    rcTableRow();
                    if (detail) {
                        rcText(rcFormat(rcAppArena(app), "%ld", l->seq),
                                .font = F_SMALL, .color = s.textMuted);
                        rcTableNext();
                    }
                    rcTextC(ray ? "[RAY]" : "[APP]", .font = F_SMALL,
                             .color = ray ? s.primary : s.textMuted);
                    rcTableNext();
                    if (detail) {
                        rcTextC(level_name(l->level), .font = F_SMALL, .color = c);
                        rcTableNext();
                    }
                    /* No .wrap: word-wrap is the default, so a long line wraps
                       inside the GROW cell. ("w" is not a valid wrap value.) */
                    rcTextC(l->msg, .font = F_SMALL, .color = c);
                }
                rcEndTable();
            }
        }
    }
}

/* ONE panel at a time, and the strip above says which one has something to say. */

/* Tab names in enum order, in ONE place. */
static const char *const INSP_TAB_NAME[INSP_TAB_COUNT] = {
    "Metrics", "System", "Scheduler", "Log"
};

/* Select a tab and say so in the log, once, on the frame it changes. */
static void select_tab(RC_App *app, AppState *st, int tab)
{
    if (st->tab == tab)
        return;
    st->tab = tab;
    applog(st, rcFormat(rcAppArena(app), "panel = %s", INSP_TAB_NAME[tab]));
}

/* One tab pill, blue when it is the tab you are on. POLLING rcClicked IS ALSO THE
   CURSOR: it marks the box clickable and RayClay hints the pointer hand.
   rcIsHovered supplies the hover fill only and does not do that. */
static void tab_button(RC_App *app, AppState *st, const char *id, int tab)
{
    RC_Style s   = rcGetStyle();
    bool     on  = st->tab == tab;
    bool     hot = rcIsHovered(id);

    rcRow(.id = id, .bg = on ? s.primary : (hot ? s.surfaceAlt : s.surface),
          .px = 12, .py = 6, .align = "cc", .borderRadius = "all-md",
          .border = { .color = on ? s.primary : s.border, .width = "1px" }) {
        rcTextC(INSP_TAB_NAME[tab], .font = F_SMALL,
                .color = on ? RC_WHITE : s.textMuted, .wrap = "n");
    }
    if (rcClicked(id))
        select_tab(app, st, tab);
}

/* THE WATCH WINDOW - the diagnostics pane in a second surface, so the panels stay
   on screen while the reader works in the app they are debugging.

   Three creation fields, each answering a different question:
     .focusPolicy = RC_WINDOW_FOCUS_NONE governs how the window is SHOWN, not what a
        later click does, and the manager may refuse it, so nothing reports it done.
     .taskbar = RC_WINDOW_TASKBAR_SKIP is INDEPENDENT of .owner: ownership decides
        stacking, the window TYPE decides listing, so a tool panel asks for both.
     .owner keeps it above the window it was torn from. rcWindowOwnerSupported() is
        false on mobile and the web, which is why watch_placeholder() below is the
        only way back on those sessions.

   Lifecycle, in the order it bites:
   1. A non-NULL RC_Window * is not a READY window - opened inside a callback it
      comes back PENDING, so keep the content here until READY is observed.
   2. NULL means the open was refused outright: latch it and stop offering the verb.
   3. Closing completes after the frame unwinds, so release on the CLOSED
      transition, in ONE place shared with the window's own close button.
   4. rcWindowRelease invalidates the pointer the moment it returns true:
      `if (rcWindowRelease(w)) st->watch = NULL;` - every time.

   rcAppArena() hands back the arena of the surface BEING DRAWN, so the panel code
   below formats correctly in both windows with no edit. */

static void open_watch_window(RC_App *app, AppState *st);
static void watch_window_update(RC_Window *w, void *user);
static void watch_window_layout(RC_Window *w, void *user);
static void watch_window_state(RC_Window *w, RC_WindowState state, void *user);

/* The record exists and its window is up or on its way down; PENDING keeps the
   pane in the main window (rule 1). */
static bool watch_floating(const AppState *st)
{
    RC_WindowState state = st->watch ? rcWindowState(st->watch) : RC_WINDOW_CLOSED;

    return st->watch && (state == RC_WINDOW_READY || state == RC_WINDOW_CLOSING);
}

/* A text button in the tabs' own shape, so the row reads as one control strip.
   rcClicked is the cursor hint here too - the same rule as tab_button. */
static bool pane_button(const char *id, const char *label, const char *tip)
{
    RC_Style s   = rcGetStyle();
    bool     hot = rcIsHovered(id);

    rcRow(.id = id, .bg = hot ? s.surfaceAlt : s.surface, .px = 10, .py = 6,
          .align = "cc", .borderRadius = "all-md",
          .border = { .color = s.border, .width = "1px" }, .tooltip = tip) {
        rcTextC(label, .font = F_SMALL, .color = hot ? s.text : s.textMuted,
                .wrap = "n");
    }
    return rcClicked(id);
}

/* The pop-out control, in the tab row's trailing slot. DRAWN ONLY IN THE MAIN
   WINDOW: the watch window draws this same row, where it would be circular. */
static void watch_control(RC_App *app, AppState *st)
{
    if (rcAppCurrentWindow(app) != rcAppMainWindow(app))
        return;
    if (!rcChildWindowsSupported() || st->watchRefused || st->watch)
        return;
    if (pane_button("watch_open", "Pop out",
                    "Open the diagnostics in a window that does not take focus"))
        open_watch_window(app, st);
}

static void diagnostics(RC_App *app, AppState *st, const InspArm *arm)
{
    /* Literal ids: an id is hashed at the call, so none of these is built. */
    static const char *const tabId[INSP_TAB_COUNT] = {
        "tab_perf", "tab_sys", "tab_sched", "tab_log"
    };

    rcRow(.gap = 6, .align = "cl", .w = "grow") {
        for (int i = 0; i < INSP_TAB_COUNT; i++)
            tab_button(app, st, tabId[i], i);
        /* A zero-basis grow box pushes the pop-out control to the trailing edge. */
        rcBox(.w = "grow") {}
        watch_control(app, st);
    }

    switch (st->tab) {
    case INSP_TAB_SYS:   system_panel(app, st, arm);   break;
    case INSP_TAB_SCHED: sched_panel(app, st, arm);    break;
    case INSP_TAB_LOG:   log_panel(app, st, arm);      break;
    default:             resource_panel(app, st, arm); break;
    }
}


/* Follow the newest log line, but only while the reader is parked at the bottom -
   if they have scrolled up to read history, stop yanking them down. Driven off the
   MONOTONIC log.total, not insp_log_count, which saturates. Call it BEFORE
   rcScrollbar, and only from the surface that drew the log: two callers would race
   for one last_total. */
static void follow_log(AppState *st)
{
    if (st->tab != INSP_TAB_LOG || st->log.total == st->last_total)
        return;
    if (rcIsScrolledToBottom("LogScroll"))
        rcScrollToBottom("LogScroll");
    st->last_total = st->log.total;
}

/* What the main window shows in the pane's place: .taskbar = SKIP took the watch
   window out of the desktop's own list, which is what leaves the app owing the user
   a way back to it.

   PUT THE ID ON THE TEXT'S OWN BOX, NEVER ON THE ROW: an ancestor counts as
   hovered, so an id on the row makes rcClicked true for a press anywhere inside it,
   the buttons beside it included. And RESTORE BEFORE RAISE. */
static void watch_placeholder(AppState *st)
{
    RC_Style   s      = rcGetStyle();
    RC_Window *w      = st->watch;
    bool       hidden = rcWindowIsMinimized(w);

    /* FIT, not GROW: a placeholder is a NOTICE, as tall as what it says, and the
       empty pane below it reports that the content is somewhere else. */
    rcColumn(.id = "watch_slot", .bg = rcAlpha(s.surface, 160), .gap = 10, .p = 16,
             .borderRadius = "all-lg",
             .border = { .color = s.border, .width = "1px" },
             .w = "grow", .h = "fit") {
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            rcBox(.id = "watch_raise",
                  .tooltip = "Bring the diagnostics window to the front") {
                rcTextL("Diagnostics", .font = F_HEAD,
                        .color = rcIsHovered("watch_raise") ? s.text : s.textMuted,
                        .wrap = "n");
            }
            if (rcClicked("watch_raise")) {
                if (rcWindowIsMinimized(w))
                    rcWindowRestore(w);
                rcWindowRaise(w);
            }
            rcBox(.w = "grow") {}
            /* Hide is not Dock: docking closes the window and loses where the user
               put it, hiding parks it and leaves the geometry alone. */
            if (hidden) {
                if (pane_button("watch_show", "Show",
                                "Show the diagnostics window again"))
                    rcWindowRestore(w);
            } else {
                if (pane_button("watch_hide", "Hide",
                                "Minimise the diagnostics window"))
                    rcWindowMinimize(w);
            }
            if (pane_button("watch_dock", "Dock",
                            "Bring the diagnostics back into this window"))
                rcWindowRequestClose(w);
        }
        /* Read back from the window system, never restated from what was asked -
           which is why the focus line below is worded as a request. */
        if (hidden) {
            rcTextL("in its own window, minimised", .font = F_SMALL,
                    .color = s.textMuted);
        } else {
            rcTextL("in its own window", .font = F_SMALL, .color = s.textMuted);
        }
        rcTextL("Opened without asking for focus, and kept out of the taskbar.",
                .font = F_SMALL, .color = s.textMuted);
    }
}

/* Where the window should be BORN. Returns false when the honest answer is
   "wherever the host likes": a Wayland client is never told where it is. It comes
   back where it was last open, or beside the main window. */
static bool watch_open_at(RC_App *app, AppState *st, int *x, int *y, int *w, int *h)
{
    RC_Window    *primary = rcAppMainWindow(app);
    RC_Dimensions d;
    int           mx, my;

    if (!rcWindowPositionSupported(primary))
        return false;
    if (st->watchGeom) {
        *x = st->watchX;
        *y = st->watchY;
        *w = st->watchW;
        *h = st->watchH;
        return true;
    }
    if (!rcWindowPosition(primary, &mx, &my))
        return false;
    d  = rcWindowDimensions(primary);
    *w = INSP_WATCH_W;
    *h = INSP_WATCH_H;
    *x = mx + (int)d.width + 12;
    *y = my;
    return true;
}

static void open_watch_window(RC_App *app, AppState *st)
{
    RC_Window *primary = rcAppMainWindow(app);
    int        bx = 0, by = 0, bw = INSP_WATCH_W, bh = INSP_WATCH_H;
    bool       at = watch_open_at(app, st, &bx, &by, &bw, &bh);

    /* Zero fields take the main window's defaults; strings are copied before the
       call returns; userData is BORROWED for the record's life. Field order follows
       the struct: an out-of-order initialiser is legal C99 and a C++ error. */
    RC_WindowOptions o = {
        .title       = "Inspector - diagnostics",
        .width       = bw,
        .height      = bh,
        .minWidth    = INSP_WATCH_MIN_W,
        .minHeight   = INSP_WATCH_MIN_H,
        .placement   = at ? RC_WINDOW_PLACE_AT : RC_WINDOW_PLACE_DEFAULT,
        .x           = bx,
        .y           = by,
        .focusPolicy = RC_WINDOW_FOCUS_NONE,
        .taskbar     = RC_WINDOW_TASKBAR_SKIP,
        .owner       = rcWindowOwnerSupported(primary) ? primary : NULL,
        .nativeFrame = true,
        /* .custom draws no band, so this window's rcTitlebar emits (and folds) it. */
        .titlebar    = { .custom = true },
        .clearColor  = rcGetStyle().background,
        /* CONTINUOUS only because this pane PLOTS the frame loop. */
        .renderMode  = RC_RENDER_CONTINUOUS,
        /* Its own scratch arena, sized for what this window actually formats. */
        .scratchArenaBytes = 32 * 1024,
        .updateCallback    = watch_window_update,
        .layoutCallback    = watch_window_layout,
        .stateCallback     = watch_window_state,
        .userData          = st,
    };
    RC_Window *win = rcAppOpenWindow(app, &o);

    if (!win) {
        /* Rule 2: refused outright - stop offering the verb. */
        st->watchRefused = true;
        applog(st, rcStringFromCStr("diagnostics window refused at open"));
        return;
    }
    st->watch = win;
    applog(st, rcStringFromCStr("diagnostics popped out"));
}

/* INPUT IS PER WINDOW, and this callback is why the Ctrl/Cmd+T chord keeps working
   after you click the watch window: RC_AppOptions.updateCallback runs for the
   PRIMARY surface only, so each extra window polls its own input. */
static void watch_window_update(RC_Window *w, void *user)
{
    AppState *st = (AppState *)user;

    (void)w;   /* the chord is already scoped to the window being updated */

    if (rcModDown(RC_MOD_PRIMARY) && rcKeyPressed(RC_KEY_T))
        st->watchBarShown = !st->watchBarShown;
}

static void watch_window_layout(RC_Window *w, void *user)
{
    AppState *st  = (AppState *)user;
    RC_App   *app = rcWindowApp(w);
    RC_Style  s;
    InspArm   arm;

    /* A theme toggled in the main window has to reach this one, and the CLEAR
       COLOUR is resolved per window. Both setters are change-gated. */
    rcSetStyle(insp_style(st->darkMode));
    s = rcGetStyle();
    rcWindowSetClearColor(w, s.background);

    /* Remember where the window is, so the next open is born there - but not while
       MAXIMISED, whose geometry is the screen's and not the user's choice. */
    if (!rcWindowIsMaximized(w)) {
        int px, py;

        if (rcWindowPosition(w, &px, &py)) {
            RC_Dimensions live = rcWindowDimensions(w);

            st->watchX    = px;
            st->watchY    = py;
            st->watchW    = (int)live.width;
            st->watchH    = (int)live.height;
            st->watchGeom = true;
        }
    }

    /* THIS window's arm. Never stacked, and the dialogs belong to controls(). */
    {
        RC_Viewport vp = rcViewport();

        arm.stacked   = false;
        arm.dialog_w  = 0.0f;
        arm.dialog_h  = 0.0f;
        arm.log_max_h = 0.0f;
        arm.log_w     = vp.width - vp.safe.left - vp.safe.right
                      - 2.0f * (float)INSP_WATCH_PAD
                      - (float)INSP_SCROLL_GUTTER - 2.0f * 14.0f;
    }

    rcColumn(.id = "WatchRoot", .bg = s.background, .w = "grow", .h = "grow") {
        /* THE BAND IS THE ROOT'S CHILD AND THE PADDING IS NOT. rcTitlebar sizes its
           band to the whole visible rect, so inside a padded parent it is wider than
           its parent and the root grows to band + padding. Pad the inner column. */
        if (st->watchBarShown) {
            /* Hoisted into a named local because C++ forbids taking the address
               of a compound literal; the band's options never vary at runtime. */
            static const RC_TitlebarOptions bar = { .title = "Inspector - diagnostics" };
            rcTitlebar(&bar);
        } else {
            /* A FOLDED BAR MUST LEAVE A DRAG REGION: a borderless window that
               draws nothing cannot be moved. rcUnzoomed because rcTitlebar
               counter-scales itself and a hand-built rail does not. */
            rcUnzoomed() {
                rcBox(.id = RC_ID_WINDOW_DRAG, .bg = s.primary, .w = "grow", .h = "4px") {}
            }
        }
        rcColumn(.p = INSP_WATCH_PAD, .w = "grow", .h = "grow") {
            /* A panel's height is CONTENT-dependent, so the pane scrolls. */
            rcColumn(.id = "WatchPane", .gap = 12, .pr = INSP_SCROLL_GUTTER,
                     .scroll = "v", .w = "grow", .h = "grow") {
                diagnostics(app, st, &arm);
            }
        }
    }

    /* The follow and the bars belong to the surface that declared the containers. */
    follow_log(st);
    rcScrollbar("WatchPane");
    if (st->tab == INSP_TAB_LOG)
        rcScrollbar("LogScroll");
}

/* Every case is spelled and there is no default, so a state added to the enum is a
   compile error here rather than a silent no-op. */
static void watch_window_state(RC_Window *w, RC_WindowState state, void *user)
{
    AppState *st  = (AppState *)user;
    RC_App   *app = rcWindowApp(w);

    switch (state) {
    case RC_WINDOW_READY:
        /* Wake the main window so it swaps the pane for its placeholder. */
        rcWindowRequestFrame(rcAppMainWindow(app));
        break;
    case RC_WINDOW_FAILED:
        st->watchRefused = true;
        applog(st, rcStringFromCStr("diagnostics window failed after open"));
        if (rcWindowRelease(w))                      /* rule 4 */
            st->watch = NULL;
        rcWindowRequestFrame(rcAppMainWindow(app));
        break;
    case RC_WINDOW_CLOSED:
        if (rcWindowRelease(w))                      /* rule 4 */
            st->watch = NULL;
        applog(st, rcStringFromCStr("diagnostics docked"));
        rcWindowRequestFrame(rcAppMainWindow(app));  /* the pane comes home */
        break;
    case RC_WINDOW_PENDING:
    case RC_WINDOW_CLOSING:
        break;
    }
}

/* One tile: a dot for severity, a label, the value, and a note carrying the unit. */
static void health_tile(RC_App *app, AppState *st, const char *id, const char *label,
                        RC_String value, RC_String note, int health, int tab)
{
    RC_Style s   = rcGetStyle();
    bool     on  = st->tab == tab;
    bool     hot = rcIsHovered(id);

    rcColumn(.id = id, .bg = hot ? s.surfaceAlt : s.surface, .gap = 5, .px = 12, .py = 10,
             .borderRadius = "all-lg",
             .border = { .color = on ? s.primary : s.border, .width = "1px" },
             .w = "grow") {
        rcRow(.gap = 7, .align = "cl", .w = "grow") {
            rcBox(.bg = health_color(st->darkMode, health), .borderRadius = "all-full",
                  .w = "8px", .h = "8px") {}
            rcTextC(label, .font = F_SMALL, .color = s.textMuted, .wrap = "n");
        }
        /* Colour only when there IS severity: a healthy strip is plain. */
        rcText(value, .font = F_BODY,
                .color = health == INSP_OK ? s.text : health_color(st->darkMode, health), .wrap = "n");
        rcText(note, .font = F_SMALL, .color = s.textMuted, .wrap = "n");
    }
    /* See tab_button: the poll is the cursor hint as well as the click. */
    if (rcClicked(id)) {
        select_tab(app, st, tab);
        /* The panel may be in the watch window - bring it where it is read. */
        if (st->watch && rcWindowState(st->watch) == RC_WINDOW_READY) {
            if (rcWindowIsMinimized(st->watch))
                rcWindowRestore(st->watch);
            rcWindowRaise(st->watch);
        }
    }
}

/* The screen's answer to "is anything wrong?", given before any panel is read - and
   the way in, because clicking a tile opens the tab that explains it. It sits in
   the LAYOUT, outside every scroller, so it never covers what it reports on. */
static void health_strip(RC_App *app, AppState *st, const InspArm *arm)
{
    RC_Arena *mem = rcAppArena(app);
    int       errors, warnings;

    static const char *const tileId[INSP_TAB_COUNT] = {
        "hs_arena", "hs_sys", "hs_sched", "hs_log"
    };
    static const char *const tileLabel[INSP_TAB_COUNT] = {
        "SCRATCH ARENA", "SYSTEM", "SCHEDULER", "LOG"
    };
    RC_String value[INSP_TAB_COUNT];
    RC_String note[INSP_TAB_COUNT];
    int       health[INSP_TAB_COUNT];

    log_severity(st, &errors, &warnings);

    /* ARENA. The one resource this app can exhaust; an overrun is lost text. */
    health[INSP_TAB_PERF] = st->cur_arena_occ > 0.9f ? INSP_BAD
                          : st->cur_arena_occ > 0.7f ? INSP_WARN
                          :                            INSP_OK;
    value[INSP_TAB_PERF]  = rcFormat(mem, "%d%% used",
                                     (int)(st->cur_arena_occ * 100.0f + 0.5f));
    note[INSP_TAB_PERF]   = rcFormat(mem, "of %d KB scratch",
                                     (int)(mem->bufferLength / 1024));

    /* SYSTEM. THE HEADLINE IS THE FIGURE THE DOT JUDGES, so both are resident
       memory: a tile headlining a CPU share with a dot keyed on memory reads as
       broken the moment the share passes 100. Judged only on a FULL window. */
    health[INSP_TAB_SYS] = (st->sys_len == INSP_HIST && st->sys_mem[0] > 0.0f &&
                            st->sys_mem[INSP_HIST - 1] > st->sys_mem[0] * 1.25f)
                         ? INSP_WARN : INSP_OK;
    value[INSP_TAB_SYS]  = st->cur_mem_mb < 0.0f
                         ? rcStringFromCStr("memory n/a")
                         : rcFormat(mem, "%.1f MB", st->cur_mem_mb);
    note[INSP_TAB_SYS]   = st->cur_cpu < 0.0f
                         ? rcStringFromCStr("resident - CPU n/a here")
                         : rcFormat(mem, "resident - %.1f cores busy",
                                    st->cur_cpu / 100.0f);

    /* SCHEDULER. Its ONE severity is a wake loop: on demand, and not parked once in
       a full window. The wasted-park figure is not one - see sched_panel. */
    {
        bool parking = st->sched_dry < INSP_HIST;
        health[INSP_TAB_SCHED] = (!st->continuous && !parking) ? INSP_WARN : INSP_OK;
        value[INSP_TAB_SCHED]  = rcStringFromCStr(st->continuous ? "continuous"
                                                                 : "on demand");
        note[INSP_TAB_SCHED]   = rcStringFromCStr(parking ? "parking between frames"
                                                          : "drawing every frame");
    }

    /* LOG. An error in the ring has to reach the reader whichever panel is open,
       which is why this is a tile and not just the LOG tab's own heading. */
    health[INSP_TAB_LOG] = errors ? INSP_BAD : warnings ? INSP_WARN : INSP_OK;
    value[INSP_TAB_LOG]  = errors   ? rcFormat(mem, "%d error%s", errors,
                                               errors == 1 ? "" : "s")
                         : warnings ? rcFormat(mem, "%d warning%s", warnings,
                                               warnings == 1 ? "" : "s")
                         :            rcStringFromCStr("clean");
    note[INSP_TAB_LOG]   = rcFormat(mem, "%d lines held", insp_log_count(&st->log));

    /* Four across where the row affords it, two by two where it does not: a tile
       whose note is clipped is a tile lying about its unit. */
    rcColumn(.id = "Health", .gap = 10, .w = "grow") {
        if (arm->stacked) {
            for (int r = 0; r < 2; r++) {
                rcRow(.gap = 10, .w = "grow") {
                    for (int i = r * 2; i < r * 2 + 2; i++)
                        health_tile(app, st, tileId[i], tileLabel[i], value[i],
                                    note[i], health[i], i);
                }
            }
        } else {
            rcRow(.gap = 10, .w = "grow") {
                for (int i = 0; i < INSP_TAB_COUNT; i++)
                    health_tile(app, st, tileId[i], tileLabel[i], value[i],
                                note[i], health[i], i);
            }
        }
    }
}

/* Pointer users only: the fold is a keystroke, so on a touch screen this would be
   a hint for a control that is not there. Keyed on the POINTER, not on the arm. */
static void toolbar_hint(const AppState *st)
{
    if (rcPointerIsCoarse())
        return;
    RC_Style s = rcGetStyle();
    rcTextC(st->barShown ? "Ctrl/Cmd+T hides the titlebar"
                          : "Ctrl/Cmd+T shows the titlebar",
             .font = F_SMALL, .color = s.textMuted, .wrap = "n");
}

static void layout(RC_App *app, void *userData)
{
    AppState *st = (AppState *)userData;
    rcSetStyle(insp_style(st->darkMode));
    RC_Style s = rcGetStyle();
    /* A runtime theme switch has to move the WINDOW too: rcSetStyle changes every
       colour the UI draws with, but the clear colour is resolved per window at
       creation, and it is ALL you see on a frame RayClay holds back. */
    rcWindowSetClearColor(rcAppMainWindow(app), s.background);

    /* SAFE AREA. A phone draws edge to edge, UNDER the status bar and the home
       indicator, and nothing moves content out of the way for you. rcViewport().safe
       is ALREADY IN LAYOUT UNITS, and {0,0,0,0} unless RAYCLAY_SAFE_INSETS is set. */
    RC_Viewport vp   = rcViewport();
    RC_Insets   safe = vp.safe;

    /* THE ARM, decided once. dialog_w/h are what a dialog body may take: the
       viewport inside the safe area, less the dialog's own chrome. */
    InspArm arm = {
        .stacked  = vp.width < (float)INSP_SPLIT_MIN_W || vp.height < (float)INSP_SPLIT_MIN_H,
        .dialog_w = vp.width  - safe.left - safe.right  - (float)INSP_DIALOG_MARGIN,
        .dialog_h = vp.height - safe.top  - safe.bottom - (float)INSP_DIALOG_MARGIN,
        .log_max_h = 0.6f * (vp.height - safe.top - safe.bottom),
    };

    /* The width the log table will get, which is what picks its column set.
       st->split_frac is the app's copy of the divider, so mid-drag this reads one
       frame behind - a column set that settles late, and nothing else. */
    {
        float content_w = vp.width - safe.left - safe.right - 2.0f * 16.0f;
        float card_w    = (float)INSP_SCROLL_GUTTER + 2.0f * 14.0f;
        arm.log_w = arm.stacked
                  ? content_w - card_w
                  : (1.0f - st->split_frac) * content_w - 6.0f - card_w;
    }

    /* TOP and BOTTOM safe-area bands are the ROOT'S - the status bar and the home
       indicator run the full width, chrome included. The SIDE bands go on the inner
       column BELOW the titlebar, for the reason the watch window's root gives: a
       band inside side padding carries every card past the right edge. */
    rcColumn(.id = "Root", .bg = s.background, .pt = (uint16_t)(safe.top),
             .pb = (uint16_t)(safe.bottom), .w = "grow", .h = "grow") {
        /* THE BUNDLED BAND, DRAWN BY THE APP. .titlebar.custom tells the runner to
           draw none; rcTitlebar then emits the same band wherever it is called,
           still tagged RC_ID_WINDOW_DRAG and still carrying working OS controls. */
        if (st->barShown) {
            /* Hoisted into a named local because C++ forbids taking the address
               of a compound literal; the band's options never vary at runtime. */
            static const RC_TitlebarOptions bar = { .title = "RayClay Inspector" };
            rcTitlebar(&bar);
        } else {
            /* The folded rail is chrome, so it is drawn inside rcUnzoomed():
               rcTitlebar counter-scales itself and a hand-built rail does not. */
            rcUnzoomed() {
                rcBox(.id = RC_ID_WINDOW_DRAG, .bg = s.primary, .w = "grow", .h = "4px") {}
            }
        }

        rcColumn(.pl = (uint16_t)(safe.left), .pr = (uint16_t)(safe.right), .w = "grow",
                 .h = "grow") {
            /* A distinct surface, NOT s.chrome, so it reads as the app's own strip
               and not a second titlebar. In the stack the hint and the readout
               stack, or the hint squeezes the title out of a narrow strip. */
            rcRow(.bg = s.surface, .gap = 14, .px = 14, .align = "cl", .w = "grow",
                  .h = "44px") {
                rcBox(.align = "cl", .overflow = "hidden", .w = "grow", .h = "grow") {
                    rcTextL("live test harness",
                             .font = F_TITLE, .color = s.text, .wrap = "n");
                }
                /* On demand, rcAppFPS measures the rate frames were ASKED for. */
                RC_String readout = rcFormat(rcAppArena(app), "%.0f %s - frame %ld",
                                                rcAppFPS(app),
                                                st->continuous ? "FPS" : "wakes/s",
                                                st->frame);
                if (arm.stacked) {
                    rcColumn(.gap = 2, .align = "cr") {
                        toolbar_hint(st);
                        rcText(readout, .font = F_SMALL, .color = s.textMuted, .wrap = "n");
                    }
                } else {
                    toolbar_hint(st);
                    rcText(readout, .font = F_SMALL, .color = s.textMuted);
                }
            }

            /* Padding matches the Content row's, so tiles line up with cards. */
            rcColumn(.pt = 16, .px = 16, .w = "grow") {
                health_strip(app, st, &arm);
            }

            rcRow(.id = "Content", .p = 16, .w = "grow", .h = "grow") {
                if (arm.stacked) {
                    /* THE STACK: the tab row and its panel FIRST, the controls
                       under them - a health tile opens a tab, so the tab row has
                       to be within a thumb of the strip. */
                    rcColumn(.id = "Stack", .gap = 16, .pr = INSP_SCROLL_GUTTER,
                             .scroll = "v", .w = "grow", .h = "grow") {
                        /* ONE SURFACE DRAWS THE PANE AT A TIME: an id is declared
                           per frame per window, so drawing it in both places would
                           declare "LogScroll" twice. */
                        if (watch_floating(st))
                            watch_placeholder(st);
                        else
                            diagnostics(app, st, &arm);
                        controls(app, st, &arm);
                    }
                } else {
                    /* A draggable split, st->split_frac being pane 1's share. THE
                       CLAMP IS THE PANES' MINIMUMS, not the library's 0.05..0.95, so
                       the handle stops before a pane clips its own rows. */
                    float extent = vp.width - safe.left - safe.right - 2.0f * 16.0f;
                    RC_SplitOptions split = {
                        .minFraction = (float)INSP_CONTROLS_MIN_W / extent,
                        .maxFraction = 1.0f - (float)(INSP_SIDEBAR_MIN_W + 6) / extent,
                    };
                    if (rcBeginSplitPane("shell", RC_SPLIT_ROW, &st->split_frac, split)) {
                        /* LEFT pane: the controls that aim the diagnostics. */
                        rcColumn(.id = "ColControls", .gap = 16, .pr = INSP_SCROLL_GUTTER,
                                 .scroll = "v", .w = "grow", .h = "grow") {
                            controls(app, st, &arm);
                        }
                        rcSplitHandle();
                        /* RIGHT pane: the tab row and the panel it selects. It
                           scrolls because a panel's height is CONTENT-dependent. */
                        rcColumn(.id = "Sidebar", .gap = 12, .pr = INSP_SCROLL_GUTTER,
                                 .scroll = "v", .w = "grow", .h = "grow") {
                            if (watch_floating(st))
                                watch_placeholder(st);
                            else
                                diagnostics(app, st, &arm);
                        }
                        rcEndSplitPane();
                    }
                }
            }
        }
    }

    /* Only while the pane is HERE: once it pops out, the watch window's frame
       declares the table and makes this call instead. See follow_log. */
    if (!watch_floating(st))
        follow_log(st);

    /* One bar per scroll container, the log's being the table body named by the id
       passed to rcBeginTable. Each is a floating element that layers above it. */
    if (arm.stacked) {
        rcScrollbar("Stack");
    } else {
        rcScrollbar("ColControls");
        rcScrollbar("Sidebar");
    }
    /* The table body exists only while its tab is the open one. */
    if (st->tab == INSP_TAB_LOG && !watch_floating(st))
        rcScrollbar("LogScroll");

    /* Scratch-arena occupancy, now that the frame's formatting is done. */
    RC_Arena *arena = rcAppArena(app);
    st->cur_arena_occ = arena->bufferLength
                      ? (float)((double)arena->currOffset / (double)arena->bufferLength)
                      : 0.0f;
}

/* One sample into a paired history; one call keeps the two series aligned. */
static void hist_push(float *series_a, float *series_b, int *len,
                      float a, float b)
{
    if (*len < INSP_HIST) {
        series_a[*len] = a;
        series_b[*len] = b;
        (*len)++;
        return;
    }
    for (int i = 1; i < INSP_HIST; i++) {
        series_a[i - 1] = series_a[i];
        series_b[i - 1] = series_b[i];
    }
    series_a[INSP_HIST - 1] = a;
    series_b[INSP_HIST - 1] = b;
}

static void update(RC_App *app, void *userData)
{
    AppState *st = (AppState *)userData;

    /* The fix for "my overlay only updates when I touch the app": every sampler here
       runs inside the frame loop, so an idle on-demand app takes NO samples and the
       panels freeze. A one-shot timer keeps the app genuinely PARKED between ticks.
       It costs the performance panel some meaning, which the panel says on screen:
       rcWindowFrameTime is the WALL GAP between frames, so a 1 Hz self-wake makes
       the "frame" row report our own cadence. */
    rcWindowRequestFrameAfter(rcAppMainWindow(app), 1.0);

    /* RC_MOD_PRIMARY is Cmd on a native macOS build and Ctrl everywhere else, and
       the letter query is logical, so one binding is right on every target. */
    if (rcModDown(RC_MOD_PRIMARY) && rcKeyPressed(RC_KEY_T))
        st->barShown = !st->barShown;

    /* Resize detection. rcGetWindowDimensions is logical pixels, zoom-independent.
       The first frame only seeds the baseline, so a relaunch logs no phantom. */
    RC_Dimensions dims = rcGetWindowDimensions();
    int w = (int)dims.width;
    int h = (int)dims.height;
    if (st->last_win_w != 0 && (w != st->last_win_w || h != st->last_win_h)) {
        RC_String line = rcFormat(rcAppArena(app), "resize  %dx%d -> %dx%d",
                                     st->last_win_w, st->last_win_h, w, h);
        insp_log_push_len(&st->log, INSP_SRC_APP, RC_LOG_INFO, line.chars, line.length);
    }
    st->last_win_w = w;
    st->last_win_h = h;

    /* Both cadences are deadlines on rcAppTime, the app's own monotonic clock, and
       NOT a running sum of rcWindowFrameTime: that is a moving average that DISCARDS
       any interval of a second or more, so a self-waking app never advances it. */
    double now = rcAppTime(app);

    /* Render metrics ~every 100 ms (a responsive ~12 s trend). */
    if (now >= st->perf_next_at) {
        if (st->perf_warm < INSP_PERF_WARMUP)
            st->perf_warm++;        /* see perf_warm: startup is not a reading */
        else
            hist_push(st->perf_fps, st->perf_ms, &st->perf_len,
                      rcAppFPS(app), rcWindowFrameTime(rcAppMainWindow(app)) * 1000.0f);
        st->perf_next_at = now + 0.1;
    }

    /* rcProcessCpuPercent accounts CPU since the PREVIOUS call, so it is called
       here and nowhere else. A reading below zero means "unavailable". */
    if (now >= st->sys_next_at) {
        float  cpu   = rcProcessCpuPercent();
        size_t bytes = rcProcessMemoryBytes();
        st->cur_cpu    = cpu;
        st->cur_mem_mb = bytes ? (float)bytes / (1024.0f * 1024.0f) : -1.0f;
        hist_push(st->sys_cpu, st->sys_mem, &st->sys_len,
                  cpu < 0.0f ? 0.0f : cpu,
                  st->cur_mem_mb < 0.0f ? 0.0f : st->cur_mem_mb);

        st->sys_next_at = now + 1.0;
    }

    /* Every frame, never on the 1 Hz tick: the scheduler panel is the one thing
       that has to keep working precisely when the app is asleep. */
    RC_SchedStats sc = rcWindowSchedStats(rcAppMainWindow(app));
    hist_push(st->sched_spur, st->sched_adm, &st->sched_len,
              (float)(sc.spurious - st->sched_prev_spur),
              (float)(sc.admitted - st->sched_prev_adm));

    /* The mode oracle, and it must be a DELTA: `waits` is cumulative, so `waits > 0`
       is a LATCH that answers "parked" forever once the app has parked once. Count
       dry samples rather than test one - a frame can admit without parking. */
    st->sched_dry = (sc.waits != st->sched_prev_waits) ? 0 : st->sched_dry + 1;
    if (st->sched_dry > INSP_HIST)
        st->sched_dry = INSP_HIST;

    st->sched_prev_spur  = sc.spurious;
    st->sched_prev_adm   = sc.admitted;
    st->sched_prev_waits = sc.waits;
    st->sched_cur        = sc;

    st->frame++;
}

int main(void)
{
    /* Static: the log sink writes into it and it must outlive the run. */
    static AppState state = {
        .darkMode      = true,
        .barShown      = true,
        .watchBarShown = true,
        .histFrac      = 1.0f,  /* the whole history until you narrow it */
        .inspectSticky = true,  /* open in the correct non-modal pairing */
        .cur_cpu     = -1.0f,   /* "n/a" until the first ~1 Hz OS sample lands */
        .cur_mem_mb  = -1.0f,
        .continuous  = true,    /* MUST match .renderMode below; the toggle reads it */
        /* Pane-1 (controls) share; the pane being READ is the larger one. */
        .split_frac    = 0.42f,
    };

    /* Baked from the bundled face at each size - no asset to ship. */
    static const float fontSizes[F_COUNT] = {
        [F_SMALL] = 13.0f,
        [F_BODY]  = 16.0f,
        [F_HEAD]  = 20.0f,
        [F_TITLE] = 26.0f,
    };

    /* Installed before the window, so the clear colour below is read from it. */
    rcSetStyle(insp_style(state.darkMode));

    /* Install the sink BEFORE rcRunApp so diagnostics emitted while the window
       and backends come up are captured too. NULL would restore stderr. */
    rcSetLogSink(on_log, &state);

    RC_AppOptions opts = {
        .width             = 1200,
        .height            = 760,
        .title             = "RayClay Inspector",
        .clearColor        = rcGetStyle().background,
        .fontSizes         = fontSizes,
        .fontCount         = F_COUNT,
        .scratchArenaBytes = 64 * 1024,
        .nativeFrame       = true,   /* borderless + the bundled titlebar */
        .updateCallback          = update,
        .layoutCallback          = layout,
        .frameEndCallback  = frame_end,
        .userData          = &state,
        /* .custom means the runner draws no band; layout() emits it instead. */
        .titlebar          = { .custom = true },
        /* The exception, not the pattern. This app MEASURES the frame loop, so a
           timed step would report its own cadence as the frame cost. */
        .renderMode        = RC_RENDER_CONTINUOUS,
    };

    int rc = rcRunApp(&opts);

    /* THE RENDER WITNESS. This example installs rcSetLogSink, so the runner's
       "rendered N of N" line goes into the in-app ring instead of stdout and a
       stdout-grepping smoke test cannot see it. frame_end counts PRESENTED frames,
       so exit 0 means it drew. Do not tighten this to `!= budget`: frameEndCallback
       also fires for the repaints a live resize drives, which the runner excludes. */
    if (rc == 0 && state.frames_drawn == 0)
        rc = 3;

    return rc;
}
