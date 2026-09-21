/*
    main.c - RayClay 2020s GUI: a modern SaaS dashboard

    A single-screen app: custom title bar, a four-tab sidebar and a page per
    tab. Demonstrates rcChart and rcSparkline, rcTextInput / rcToggle /
    rcSlider / rcCombo, rcDefineClass, procedural icons and a custom titlebar -
    one source that builds native and web (cmake --preset web ->
    dashboard.html). Zero-asset: the bundled font is baked at runtime.

    Below CONSOLE_TWO_PANE_W the sidebar is withheld, its four tabs become a
    segment strip under the topbar and the content takes the width. Every
    control and every datum of the wide arm is on the narrow arm too.

    Build target: rayclay_ex05_2020s_gui
*/
#include "rayclay.h"

#include "icons/rc_icons_rayclay_logo.h"
#include "icons/rc_icons_settings.h"
#include "icons/rc_icons_panel_left.h"
#include "icons/rc_icons_chart_column.h"
#include "icons/rc_icons_folder.h"

/* Font ladder baked from the bundled face at these sizes - zero-asset. The gaps
   between the top three rungs are the hierarchy: F_HEAD titles a panel, F_STAT
   is a stat cell's figure, F_HERO is the one number a page is read for. */
typedef enum { F_SMALL = 0, F_BODY, F_HEAD, F_STAT, F_HERO, F_COUNT } AppFont;

/* Thousands separated by hand: printf's grouping flag is a locale feature, and
   one source cannot rely on it across four platforms. Good to 999,999. */
static RC_String thousands(RC_Arena *mem, int n) {
    return n < 1000 ? rcFormat(mem, "%d", n)
                    : rcFormat(mem, "%d,%03d", n / 1000, n % 1000);
}

/* File scope because two pages read it: the Settings combo writes st->plan and
   the sidebar prints the name it chose. */
static const char *const PLANS[] = { "Free", "Pro", "Team", "Enterprise" };
enum { PLAN_COUNT = (int)(sizeof PLANS / sizeof PLANS[0]) };

typedef struct {
    bool  darkMode;       /* global theme toggle                        */
    int   page;           /* 0=Dashboard 1=Analytics 2=Files 3=Settings */
    int   fileSel;        /* Files: the selected row                    */
    bool  notifications;  /* Settings: toggle                           */
    char  project[48];    /* Settings: text field                       */
    float budget;         /* Settings: slider (0..1 -> $0..$5 000)      */
    int   plan;           /* Settings: dropdown                         */
} AppState;

/* THIS APP'S PALETTE, installed over a preset rather than accepted from it:
   neutral zinc surfaces and ONE electric violet, spent only on things the user
   can act on, so violet always means "this is live". The status hues stay
   semantic - emerald healthy, amber worth a look, rose wrong - and each arm
   picks the shade that is legible on its own surfaces.

   The RC_* tokens are #defines, which is what lets them sit in an expression
   here: a file-scope `static const RC_Color X = rcRgb(...)` is not a constant
   expression in C99. Both arms derive from a preset, so a field this app does
   not care about still gets the library's answer. */
static RC_Style console_style(bool dark) {
    RC_Style s = dark ? rcStyleDark() : rcStyleLight();

    s.background   = dark ? RC_ZINC_950 : RC_ZINC_100;
    s.surface      = dark ? RC_ZINC_900 : RC_WHITE;
    s.surfaceAlt   = dark ? RC_ZINC_800 : RC_ZINC_100;
    s.chrome       = dark ? RC_ZINC_900 : RC_WHITE;
    s.text         = dark ? RC_ZINC_50  : RC_ZINC_900;
    s.textMuted    = dark ? RC_ZINC_400 : RC_ZINC_500;
    s.border       = dark ? RC_ZINC_800 : RC_ZINC_200;
    s.primary      = RC_VIOLET_600;
    s.primaryHover = RC_VIOLET_500;
    s.success      = dark ? RC_EMERALD_500 : RC_EMERALD_600;
    s.successHover = dark ? RC_EMERALD_400 : RC_EMERALD_700;
    s.warning      = dark ? RC_AMBER_500   : RC_AMBER_600;
    s.warningHover = dark ? RC_AMBER_400   : RC_AMBER_700;
    s.danger       = dark ? RC_ROSE_500    : RC_ROSE_600;
    s.dangerHover  = dark ? RC_ROSE_400    : RC_ROSE_500;
    return s;
}

/* THE ONE BREAKPOINT, derived from this app's own panes rather than a table of
   device widths: the sidebar, plus a row of three metric cells and the gutters
   around them. Below it the sidebar is withheld, its four targets move to a
   segment strip under the topbar and the content column takes the whole width.
   Nothing is squeezed and nothing is dropped - a cell that does not fit its row
   is taken by the next one, and the Files table carries its two narrow columns
   under each name.

   Compare against rcViewport().width - the space the layout actually has - so a
   desktop window dragged narrow takes the same branch a phone does. NEVER
   compare against rcGetWindowDimensions() divided by zoom: that is correct under
   RC_ZOOM_LAYOUT and wrong under RC_ZOOM_OPTICAL. */
#define SIDEBAR_W       220   /* the nav column, px                           */
#define CONTENT_PAD     20    /* the content column's gutters                 */
#define STAT_GAP        14    /* between cells in a row                       */
#define STAT_CELL_MIN_W 132   /* the widest line, 111, inside the panel's 16s */
#define STAT_CELL_W     (STAT_CELL_MIN_W + 2 * STAT_GAP)   /* 160: with air   */
#define STAT_CELLS      3     /* cells in a full row                          */
#define CONSOLE_TWO_PANE_W \
    (float)(SIDEBAR_W + 2 * CONTENT_PAD + STAT_CELLS * STAT_CELL_W + (STAT_CELLS - 1) * STAT_GAP)

/* What one frame's width decides, computed ONCE per frame in fit_viewport()
   and handed down: no page asks the viewport for itself, so every site
   branches on the same number. */
typedef struct {
    bool compact;      /* below CONSOLE_TWO_PANE_W: sidebar withheld, strip shown */
    bool railCards;    /* the sidebar has room for its live cards                 */
    bool cellSpark;    /* a metric cell is wide enough to carry a sparkline       */
    int  cellsPerRow;  /* metric cells one row holds without squeezing any        */
} Fit;

static Fit fit_viewport(void) {
    RC_Viewport vp = rcViewport();
    Fit fit;
    fit.compact = vp.width < CONSOLE_TWO_PANE_W;
    /* The content column's inner width. A cell row WRAPS by this arithmetic
       rather than by squeezing, down to what a cell's widest line needs. */
    float content_w = vp.width - vp.safe.left - vp.safe.right
                    - (fit.compact ? 0.0f : (float)SIDEBAR_W) - 2.0f * CONTENT_PAD;
    int   n = (int)((content_w + STAT_GAP) / (STAT_CELL_MIN_W + STAT_GAP));
    fit.cellsPerRow = n < 1 ? 1 : n > STAT_CELLS ? STAT_CELLS : n;
    /* What HEIGHT and remaining WIDTH decide. The sidebar's live cards sit
       between the nav and the badge pinned at its foot, so a short window would
       push the badge off the bottom; a metric cell earns a sparkline only once
       its figure and trend have room to spare beside one. */
    fit.railCards = vp.height >= 640.0f;
    fit.cellSpark = (content_w - 32.0f - (float)((fit.cellsPerRow - 1) * STAT_GAP))
                    / (float)fit.cellsPerRow >= 250.0f;
    return fit;
}

/* Sidebar nav row: a whole-row click target. rcClicked turns any styled rcRow
   into a button, so the row itself is the control - and the poll is what hands
   it the pointer cursor. THE CURRENT DESTINATION IS NOT CARRIED BY COLOUR
   ALONE: a fill, a leading rule and full-strength text, one of them a SHAPE, so
   the state survives a reader who cannot separate the two greys. The rule is
   declared for every row and is transparent when it is not current, so nothing
   shifts as the page changes. */
static bool nav_btn(const char *id, const char *label, RC_IconCallback icon, bool active) {
    RC_Style s = rcGetStyle();
    rcRow(.id = id,
          .bg = active ? s.surfaceAlt
                       : (rcIsHovered(id) ? rcAlpha(s.primary, 40) : RC_TRANSPARENT),
          .gap = 8, .pl = 8, .pr = 10, .align = "cl", .borderRadius = "all-md",
          .w = "grow", .h = "40px") {
        rcBox(.bg = active ? s.primary : RC_TRANSPARENT, .borderRadius = "all-full",
              .w = "3px", .h = "18px") {}
        icon(16.0f, active ? s.text : s.textMuted);
        rcTextC(label, .font = F_BODY, .color = active ? s.text : s.textMuted);
    }
    return rcClicked(id);
}

/* The pill and the readout are helpers because each is drawn from ONE of two
   places per frame - the topbar on the wide arm, the foot of the page on the
   compact arm - and a datum drawn from two sites must not be able to drift. */
static void console_pill(void) {
    RC_Style s = rcGetStyle();
    rcBox(.bg = s.surfaceAlt, .px = 8, .py = 3, .borderRadius = "all-full") {
        rcTextL("Console", .font = F_SMALL, .color = s.textMuted);
    }
}

/* A freshness stamp, in the product's vocabulary rather than the developer's.
   It still proves the loop is running, and the value is an rcFormat into the
   FRAME arena, which rcText keeps a pointer to until the frame is drawn - a
   stack buffer here would draw blank. */
static void live_readout(RC_App *app) {
    RC_Style  s    = rcGetStyle();
    /* 60 frames to the "second" - a display cadence, not a clock. */
    RC_String seen = rcFormat(rcAppArena(app), "updated %llus ago",
                              (unsigned long long)((rcWindowSchedStats(rcAppMainWindow(app)).admitted / 60) % 60));

    rcRow(.gap = 6, .align = "cc") {
        rcBox(.bg = s.success, .borderRadius = "all-full", .w = "6px", .h = "6px") {}
        rcText(seen, .font = F_SMALL, .color = s.textMuted);
    }
}

/* `band` is the part of the safe area this band sits on. Its SURFACE spans the
   window and its CONTENT is padded in by the insets, which is what a native app
   does under a status bar. The surface is a plain column OUTSIDE rcUnzoomed():
   the insets are layout units, and inside that scope a number is counter-scaled
   by the zoom. */
static void topbar(RC_App *app, AppState *st, const Fit *fit, RC_Insets band) {
    RC_Style s = rcGetStyle();
    /* The whole band is the OS drag region; the theme cluster keeps its click by
       opting OUT with RC_ID_WINDOW_NODRAG, the desktop twin of CSS
       `-webkit-app-region: no-drag`. Inert on web, where there is no drag.

       Chrome, not content. RC_AppOptions.titlebarHeight freezes the OS drag
       strip in physical px, so a band that grew with the content zoom would stop
       matching the strip the OS lets you drag - draw it inside rcUnzoomed(). */
    rcColumn(.bg = s.chrome, .pt = (uint16_t)band.top, .pl = (uint16_t)band.left,
             .pr = (uint16_t)band.right, .w = "grow") {
        rcUnzoomed() {
            rcRow(.id = RC_ID_WINDOW_DRAG, .bg = s.chrome, .gap = 12, .px = 14,
                  .align = "cl", .w = "grow", .h = "56px") {
                rcIconRayClayLogo(28.0f);
                rcTextL("RayClay", .font = F_HEAD, .color = s.text);
                /* The band is a child of the ROOT, so whatever does not fit here
                   pushes every page past the right edge. On the compact arm it
                   keeps the name, the theme control and the window-control
                   cluster; the pill and the readout move to the foot of the page
                   in layout(), so nothing is withheld. */
                if (!fit->compact) console_pill();
                rcBox(.w = "grow") {}
                if (!fit->compact) live_readout(app);
                /* The ROW is the target, 44 px tall for a finger and the label
                   included, the way a settings row toggles when its text is
                   tapped. The switch's own box still works: a press is owned by
                   the LAST poller, so one tap on the knob flips the theme once,
                   through the row, never twice. */
                rcRow(.id = RC_ID_WINDOW_NODRAG, .align = "cl") {
                    rcRow(.id = "tg_theme_row", .gap = 8, .align = "cl", .h = "44px") {
                        rcTextC(st->darkMode ? "Dark" : "Light", .font = F_SMALL, .color = s.textMuted);
                        rcToggle("tg_theme", &st->darkMode);
                    }
                    if (rcClicked("tg_theme_row")) st->darkMode = !st->darkMode;
                }
                rcWindowControls();
            }
        }
    }
}

/* The gradient brand badge: the sidebar's foot on the desktop, the end of the
   scrolling content on the compact arm - in flow either way, never floating. */
static void brand_badge(void) {
    rcRow(.id = "sb_badge", .gap = 10, .p = 10, .align = "cl",
          .borderRadius = "all-lg", .w = "grow",
          .gradient = { .from = RC_VIOLET_600, .to = RC_INDIGO_600, .dir = "d" },
          .shadow = { .color = rcAlpha(RC_VIOLET_900, 140), .y = 4, .blur = 14, .spread = -2 }) {
        rcIconRayClayLogo(24.0f);
        rcColumn(.gap = 2) {
            rcTextL("RayClay",           .font = F_SMALL, .color = RC_WHITE);
            /* RC_VERSION is a string literal, so it concatenates at compile
               time and the badge can never drift from the library. */
            rcTextL("v" RC_VERSION " \xc2\xb7 one source",
                     .font = F_SMALL, .color = rcAlpha(RC_WHITE, 200));
        }
    }
}

/* A group heading, the way a console sidebar separates a workspace from its
   administration. Four rows with no headings read as an unfinished list. */
/* The budget the Settings page sets, read back on every page: the sidebar is
   the one surface always on screen, so a figure changed on one page and watched
   here is the shortest demonstration that a widget writes its state the frame
   it changes. */
static void rail_usage(RC_App *app, AppState *st) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surfaceAlt, .gap = 8, .p = 10, .borderRadius = "all-lg",
             .w = "grow") {
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            rcBox(.w = "grow") {
                rcTextL("Monthly budget", .font = F_SMALL, .color = s.textMuted);
            }
            rcBox(.bg = rcAlpha(s.primary, 60), .px = 6, .py = 2,
                  .borderRadius = "all-full") {
                rcTextC(PLANS[st->plan], .font = F_SMALL, .color = s.primaryHover);
            }
        }
        rcProgress("pb_budget", st->budget);
        rcText(rcFormat(rcAppArena(app), "$%s of $5,000",
                         thousands(rcAppArena(app),
                                   (int)(st->budget * 5000.0f + 0.5f)).chars),
                .font = F_SMALL, .color = s.text);
    }
}

/* Service health, the block an ops console keeps in view on every page. The
   dot carries the state so the row answers "is anything amber" before a word of
   it is read. */
static void rail_status(void) {
    RC_Style    s        = rcGetStyle();
    const char *name[3]  = { "API", "Web app", "Jobs" };
    const char *state[3] = { "healthy", "healthy", "degraded" };
    RC_Color    dot[3]   = { s.successHover, s.successHover, s.warningHover };

    rcColumn(.bg = s.surfaceAlt, .gap = 8, .p = 10, .borderRadius = "all-lg",
             .w = "grow") {
        rcTextL("Services", .font = F_SMALL, .color = s.textMuted);
        for (int i = 0; i < 3; i++) {
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                rcBox(.bg = dot[i], .borderRadius = "all-full", .w = "8px",
                      .h = "8px") {}
                rcBox(.w = "grow") {
                    rcTextC(name[i], .font = F_SMALL, .color = s.text);
                }
                rcTextC(state[i], .font = F_SMALL, .color = dot[i]);
            }
        }
    }
}

static void sidebar(RC_App *app, AppState *st, const Fit *fit) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surface, .gap = 4, .p = 12, .h = "grow", .wType = RC_PX(SIDEBAR_W)) {
        if (nav_btn("nav_dash", "Dashboard", rcIconPanelLeft,   st->page == 0)) st->page = 0;
        if (nav_btn("nav_ana",  "Analytics", rcIconChartColumn, st->page == 1)) st->page = 1;
        if (nav_btn("nav_fil",  "Files",     rcIconFolder,      st->page == 2)) st->page = 2;
        if (nav_btn("nav_set",  "Settings",  rcIconSettings,    st->page == 3)) st->page = 3;
        if (fit->railCards) {
            rcColumn(.gap = 10, .pt = 14, .w = "grow") {
                rail_usage(app, st);
                rail_status();
            }
        }
        rcBox(.w = "grow", .h = "grow") {}
        brand_badge();
    }
}

/* The sidebar row as a segment: the glyph OVER its name, every segment named.
   A finger never sees a tooltip, so a bare glyph makes a phone user tap it to
   learn what it is. 44 px tall for a finger; the sidebar's leading rule becomes
   a rule above the glyph, because a segment strip is read across. */
static bool nav_tab(const char *id, const char *label, RC_IconCallback icon, bool active) {
    RC_Style s = rcGetStyle();
    rcColumn(.id = id,
             .bg = active ? s.surfaceAlt
                          : (rcIsHovered(id) ? rcAlpha(s.primary, 40) : RC_TRANSPARENT),
             .gap = 3, .px = 10, .align = "cc", .borderRadius = "all-md", .w = "grow",
             .h = "44px") {
        rcBox(.bg = active ? s.primary : RC_TRANSPARENT, .borderRadius = "all-full",
              .w = "18px", .h = "3px") {}
        icon(16.0f, active ? s.text : s.textMuted);
        rcTextC(label, .font = F_SMALL, .color = active ? s.text : s.textMuted);
    }
    return rcClicked(id);
}

/* The compact arm's navigation: the sidebar's four targets as one strip under
   the topbar, so the reader's order (band, navigation, page) is the desktop's.
   Same ids as the sidebar - one arm is emitted per frame, so hover and click
   state carry across a resize. */
static void nav_strip(AppState *st, RC_Insets band) {
    RC_Style s = rcGetStyle();
    rcRow(.bg = s.surface, .gap = 4, .pl = (uint16_t)(12.0f + band.left),
          .pr = (uint16_t)(12.0f + band.right), .py = 4, .w = "grow") {
        if (nav_tab("nav_dash", "Dashboard", rcIconPanelLeft,   st->page == 0)) st->page = 0;
        if (nav_tab("nav_ana",  "Analytics", rcIconChartColumn, st->page == 1)) st->page = 1;
        if (nav_tab("nav_fil",  "Files",     rcIconFolder,      st->page == 2)) st->page = 2;
        if (nav_tab("nav_set",  "Settings",  rcIconSettings,    st->page == 3)) st->page = 3;
    }
}

/* One supporting metric: a status rule, a muted label, the number, the trend,
   and the fortnight behind it. A CELL on a shared surface rather than a card of
   its own - the page already has one thing in a raised panel, and a row of
   raised cards beside it makes the reader choose between them. The rule carries
   the trend's own semantic colour full height, so the row answers "is anything
   amber" from across the desk, before a single number is read.

   rcSparkline GROWS on both axes, so it draws NOTHING unless it is inside a
   box with a size. It is withheld rather than squeezed when the cell is narrow:
   a 40 px trace is a smudge, and the figure is what the cell is for. */
static void metric_cell(const char *id, const char *label, const char *value,
                        const char *trend, RC_Color accent,
                        const float *spark, int sparkCount) {
    RC_Style s = rcGetStyle();
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        rcBox(.bg = accent, .borderRadius = "all-full", .w = "3px", .h = "grow") {}
        rcColumn(.gap = 3, .w = "grow") {
            rcTextC(label, .font = F_SMALL, .color = s.textMuted);
            /* F_STAT, not F_HEAD: a KPI is a number, not a heading, and at the
               panel title's size the eye has no reason to land on it first. */
            rcTextC(value, .font = F_STAT,  .color = s.text);
            rcTextC(trend, .font = F_SMALL, .color = accent);
        }
        if (spark) {
            rcBox(.w = "grow", .h = "46px") {
                rcSparkline(id, spark, sparkCount,
                             RC_LIT(RC_SparklineOptions){ .kind = RC_SERIES_AREA,
                                                          .color = rcAlpha(accent, 150) });
            }
        }
    }
}

typedef struct {
    const char *id, *label, *value, *trend;
    RC_Color    accent;
    const float *spark;
    int          sparkCount;
} StatCell;

/* The supporting metrics: one panel, `per_row` cells to a row. Every cell
   GROWs, so a wrapping row would never break a line - the count comes from
   fit_viewport, the same number every row on every page uses. */
static void metric_strip(const StatCell *cells, int n, int per_row, bool spark) {
    RC_Style s = rcGetStyle();
    rcColumn(.id = "panel_metrics", .bg = s.surface, .gap = 16,
             .border = { .color = s.border, .width = "1px" }, .w = "grow",
             .className = "panel") {
        for (int i = 0; i < n; i += per_row) {
            rcRow(.gap = STAT_GAP, .w = "grow") {
                for (int j = i; j < n && j < i + per_row; j++)
                    metric_cell(cells[j].id, cells[j].label, cells[j].value,
                                cells[j].trend, cells[j].accent,
                                spark ? cells[j].spark : NULL, cells[j].sparkCount);
            }
        }
    }
}

/* A row of the activity feed. .hMin alone is FIT WITH A FLOOR, so a title that
   does not fit wraps under the chip and makes its row taller rather than
   clipping, while the one-line rows keep their 44 px. */
static void feed_row(const char *text, const char *badge, RC_Color dot) {
    RC_Style s = rcGetStyle();
    /* No hover state: a feed row is READ, not clicked - the activity list is
       what the console tells you, not a menu. A surface that lights up under
       the pointer is a promise, whatever colour it is drawn in. */
    rcRow(.bg = s.surfaceAlt, .gap = 10,
          .px = 12, .py = 8, .align = "cl", .borderRadius = "all-md", .w = "grow",
          .hMin = 44) {
        rcBox(.bg = dot, .borderRadius = "all-full", .w = "8px", .h = "8px") {}
        rcBox(.align = "cl", .w = "grow") {
            rcTextC(text, .font = F_BODY, .color = s.text);
        }
        rcBox(.bg = s.surface, .px = 10, .py = 4, .borderRadius = "all-full",
               .border = { .color = s.border, .width = "1px" }) {
            rcTextC(badge, .font = F_SMALL, .color = s.textMuted);
        }
    }
}

/* Requests per hour across a day, today and yesterday. FILE-SCOPE AND CONST
   because RC_Series BORROWS its arrays until the frame is rendered: a local
   would dangle. Both pages plot the pair. */
static const float req_24h[] = {
    12, 8, 6, 5, 4, 6, 14, 32, 58, 71, 78, 82,
    88, 79, 74, 80, 91, 97, 85, 66, 48, 34, 22, 15,
};
static const float req_24h_prev[] = {
    10, 7, 5, 4, 4, 5, 12, 27, 49, 60, 66, 70,
    74, 68, 63, 67, 76, 81, 71, 56, 41, 29, 19, 13,
};

/* Twelve weeks behind each supporting metric, so a stat cell shows the SHAPE of
   its trend beside the figure rather than leaving 180 px of cell empty. */
enum { SPARK_N = 12 };
static const float users_12w[SPARK_N] = {
    980, 1004, 1060, 1042, 1098, 1120, 1155, 1140, 1190, 1216, 1252, 1284,
};
static const float latency_12w[SPARK_N] = {
    112, 118, 114, 121, 117, 124, 119, 126, 122, 130, 125, 128,
};
static const float uptime_12w[SPARK_N] = {
    99.92f, 99.99f, 99.97f, 100.0f, 99.95f, 99.99f,
    99.98f, 99.99f, 99.90f, 99.99f, 99.98f, 99.98f,
};
static const float peak_12w[SPARK_N] = {
    0.81f, 0.85f, 0.79f, 0.88f, 0.90f, 0.86f,
    0.91f, 0.87f, 0.93f, 0.90f, 0.92f, 0.93f,
};
static const float avg_12w[SPARK_N] = {
    0.66f, 0.69f, 0.65f, 0.71f, 0.72f, 0.70f,
    0.74f, 0.71f, 0.75f, 0.73f, 0.74f, 0.74f,
};
static const float dip_12w[SPARK_N] = {
    -0.40f, -0.44f, -0.38f, -0.47f, -0.42f, -0.49f,
    -0.44f, -0.51f, -0.45f, -0.48f, -0.43f, -0.46f,
};

/* What the headline says, READ OFF THE TRACES instead of typed beside them, so
   no figure in the panel can disagree with the line it sits next to. A
   hand-typed "+18.2%" is correct until someone edits one datum. */
typedef struct {
    int   today;      /* requests summed across the 24 hours */
    int   yesterday;
    int   peak;       /* the busiest hour's rate             */
    int   peakHour;   /* and which hour that was             */
    float delta;      /* today against yesterday, per cent   */
} Traffic;

static Traffic traffic_summary(void) {
    Traffic t = { 0 };
    for (int i = 0; i < 24; i++) {
        t.today     += (int)req_24h[i];
        t.yesterday += (int)req_24h_prev[i];
        if ((int)req_24h[i] > t.peak) {
            t.peak     = (int)req_24h[i];
            t.peakHour = i;
        }
    }
    if (t.yesterday > 0)
        t.delta = 100.0f * (float)(t.today - t.yesterday) / (float)t.yesterday;
    return t;
}

/* THE HEADLINE FIGURE, the change as a tinted chip in its own semantic colour,
   the two readings that qualify it, and THE ONE ACTION THE PAGE OFFERS. The
   filled accent button is the only one on the Dashboard, which is what makes it
   the obvious next step rather than one option among several. */
static void hero_figure(RC_Arena *mem, AppState *st, const Traffic *t, bool compact) {
    RC_Style s     = rcGetStyle();
    RC_Color trend = t->delta >= 0.0f ? s.successHover : s.warningHover;

    rcColumn(.gap = 8, .w = compact ? "grow" : "230px") {
        rcTextL("Requests today", .font = F_SMALL, .color = s.textMuted);
        rcText(thousands(mem, t->today), .font = F_HERO, .color = s.text);
        rcRow(.bg = rcAlpha(trend, 46), .gap = 6, .px = 8, .py = 4, .align = "cl",
              .borderRadius = "all-full") {
            rcBox(.bg = trend, .borderRadius = "all-full", .w = "8px", .h = "8px") {}
            rcText(rcFormat(mem, "%+.1f%% vs yesterday", t->delta),
                    .font = F_SMALL, .color = trend);
        }
        rcText(rcFormat(mem, "Peak %d/hr at %02d:00 \xc2\xb7 yesterday %s",
                          t->peak, t->peakHour, thousands(mem, t->yesterday).chars),
                .font = F_SMALL, .color = s.textMuted);
        if (rcButton("btn_hours", "Open Analytics", RC_BTN_PRIMARY)) st->page = 1;
    }
}

/* The trace that figure summarises. Yesterday is declared FIRST because series
   draw in order and a comparison belongs behind its subject. The guide and the
   colour-matched markers are what make a two-series readout attributable: the
   tooltip lists two numbers and cannot say on its own which line each came
   from. */
static void hero_chart(const Fit *fit) {
    RC_Style s = rcGetStyle();
    rcBox(.w = "grow", .h = fit->compact ? "150px" : "176px") {
        RC_Series ser[2] = {
            { .y = req_24h_prev, .count = 24, .kind = RC_SERIES_LINE,
              .color = s.textMuted, .label = "yesterday", .thickness = 1.5f },
            { .y = req_24h, .count = 24, .kind = RC_SERIES_AREA,
              .color = s.primary, .label = "today" },
        };
        rcChart("db_trend", ser, 2,
                 RC_LIT(RC_ChartOptions){ .x = { .ticks = 6 },
                                    .y = { .label = "requests / hr", .grid = true },
                                    .legend = true,
                                    .tooltip = RC_CHART_TOOLTIP_NEAREST,
                                    .tooltipPlace = RC_TOOLTIP_PLACE_CORNER,
                                    .hoverGuide = true, .hoverMarkers = true });
    }
}

/* THE PAGE'S ANSWER IS THE RAISED PANEL and everything supporting it is flat -
   the whole hierarchy, and why the shadow is a signal here rather than
   decoration. Figure beside trace on the wide arm, above it on the compact one,
   out of the same two helpers either way. */
static void traffic_hero(RC_App *app, AppState *st, const Fit *fit) {
    RC_Style  s   = rcGetStyle();
    RC_Arena *mem = rcAppArena(app);
    Traffic   t   = traffic_summary();

    rcColumn(.id = "panel_traffic", .bg = s.surface, .gap = 14,
             .border = { .color = s.border, .width = "1px" }, .w = "grow",
             .shadow = { .color = rcAlpha(RC_BLACK, 110), .y = 10, .blur = 30,
                         .spread = -6 },
             .className = "panel") {
        rcRow(.align = "cl", .w = "grow") {
            rcBox(.w = "grow") {
                rcTextL("Traffic", .font = F_HEAD, .color = s.text);
            }
            rcTextL("Last 24 hours", .font = F_SMALL, .color = s.textMuted);
        }
        if (fit->compact) {
            rcColumn(.gap = 16, .w = "grow") {
                hero_figure(mem, st, &t, true);
                hero_chart(fit);
            }
        } else {
            rcRow(.gap = 24, .align = "cl", .w = "grow") {
                hero_figure(mem, st, &t, false);
                hero_chart(fit);
            }
        }
    }
}

static void page_dashboard(RC_App *app, AppState *st, const Fit *fit) {
    RC_Style s = rcGetStyle();
    /* The three readings a traffic number is qualified by, each with the colour
       its own state earns: a latency that has grown is amber whether or not it
       has broken anything yet. */
    const StatCell cells[STAT_CELLS] = {
        { "sc_users",   "Active users", "1,284",  "+12.4% this week",
          s.successHover, users_12w,   SPARK_N },
        { "sc_latency", "p95 latency",  "128 ms", "+9 ms vs last week",
          s.warningHover, latency_12w, SPARK_N },
        { "sc_uptime",  "Uptime",       "99.98%", "30-day average",
          s.successHover, uptime_12w,  SPARK_N },
    };
    rcColumn(.gap = 2) {
        rcTextL("Dashboard",                    .font = F_HEAD,  .color = s.text);
        rcTextL("Live metrics - one C source.", .font = F_SMALL, .color = s.textMuted);
    }
    traffic_hero(app, st, fit);
    metric_strip(cells, STAT_CELLS, fit->cellsPerRow, fit->cellSpark);
    rcColumn(.id = "panel_activity", .bg = s.surface,
             .border = { .color = s.border, .width = "1px" },
             .w = "grow",
             .className = "panel gap-2.5") {
        rcRow(.align = "cl", .w = "grow") {
            rcBox(.w = "grow") {
                rcTextL("Recent activity", .font = F_HEAD, .color = s.text);
            }
            rcTextL("Today", .font = F_SMALL, .color = s.textMuted);
        }
        /* U+00A0 keeps the number with its unit when the title wraps (the
           wrapper breaks on ' ' only): "243" / "KB" on two lines is a wrap the
           Pixel's width produced. Spelled as bytes so the glue is visible;
           its advance is a space's, so nothing moves where the line fits. */
        feed_row("Deploy succeeded - web bundle 243\xc2\xa0KB", "ci",      s.successHover);
        feed_row("New sign-up from the landing page",       "user",    s.textMuted);
        feed_row("Invoice #1042 paid",                      "billing", s.warningHover);
        feed_row("Renderer benchmark within budget",        "perf",    s.successHover);
        feed_row("Login from a new device",                 "alert",   s.danger);
    }
}

static void page_analytics(const Fit *fit) {
    RC_Style s = rcGetStyle();
    const StatCell cells[STAT_CELLS] = {
        { "sc_peak", "Peak day",    "Thu 93%", "highest this week",
          s.successHover, peak_12w, SPARK_N },
        { "sc_avg",  "Avg daily",   "74%",     "vs 68% last week",
          s.successHover, avg_12w,  SPARK_N },
        { "sc_dip",  "Weekend dip", "-46%",    "vs weekday avg",
          s.warningHover, dip_12w,  SPARK_N },
    };
    static const struct {
        const char *day; float val; const char *pct;
    } bars[] = {
        {"Mon", 0.62f, "62%"}, {"Tue", 0.84f, "84%"}, {"Wed", 0.71f, "71%"},
        {"Thu", 0.93f, "93%"}, {"Fri", 0.88f, "88%"}, {"Sat", 0.45f, "45%"},
        {"Sun", 0.38f, "38%"},
    };
    static const char *const pb_ids[] = {
        "pb_mon", "pb_tue", "pb_wed", "pb_thu", "pb_fri", "pb_sat", "pb_sun",
    };

    rcColumn(.gap = 2) {
        rcTextL("Analytics",                          .font = F_HEAD,  .color = s.text);
        rcTextL("Request volume - today and this week.", .font = F_SMALL, .color = s.textMuted);
    }
    /* THIS PAGE'S ANSWER, so this is the panel that is raised: the same pair of
       traces the Dashboard summarises, at full height and with the hours
       readable. rcChart GROWs, so it is wrapped in a sized box. */
    rcColumn(.id = "panel_hourly", .bg = s.surface,
             .border = { .color = s.border, .width = "1px" },
             .w = "grow",
             .shadow = { .color = rcAlpha(RC_BLACK, 110), .y = 10, .blur = 30,
                         .spread = -6 },
             .className = "panel gap-2.5") {
        rcRow(.align = "cl", .w = "grow") {
            rcBox(.w = "grow") {
                rcTextL("Hourly requests", .font = F_HEAD, .color = s.text);
            }
            rcTextL("Today against yesterday", .font = F_SMALL, .color = s.textMuted);
        }
        rcBox(.w = "grow", .h = "190px") {
            RC_Series ser[2] = {
                { .y = req_24h_prev, .count = 24, .kind = RC_SERIES_LINE,
                  .color = s.textMuted, .label = "yesterday", .thickness = 1.5f },
                { .y = req_24h, .count = 24, .kind = RC_SERIES_AREA,
                  .color = s.primary, .label = "today" },
            };
            rcChart("an_hourly", ser, 2,
                     RC_LIT(RC_ChartOptions){ .x = { .ticks = 6 },
                                        .y = { .label = "requests / hr", .grid = true },
                                        .legend = true,
                                        .tooltip = RC_CHART_TOOLTIP_NEAREST,
                                        .tooltipPlace = RC_TOOLTIP_PLACE_CORNER,
                                        .hoverGuide = true, .hoverMarkers = true });
        }
    }
    rcColumn(.id = "panel_weekly", .bg = s.surface,
             .border = { .color = s.border, .width = "1px" },
             .w = "grow",
             .className = "panel gap-2.5") {
        rcTextL("Weekly traffic", .font = F_HEAD, .color = s.text);
        for (int i = 0; i < 7; i++) {
            rcRow(.gap = 10, .align = "cl", .w = "grow") {
                rcBox(.w = "36px") {
                    rcTextC(bars[i].day, .font = F_SMALL, .color = s.textMuted);
                }
                rcBox(.w = "grow") { rcProgress(pb_ids[i], bars[i].val); }
                rcBox(.w = "36px") {
                    rcTextC(bars[i].pct, .font = F_SMALL, .color = s.textMuted);
                }
            }
        }
    }
    metric_strip(cells, STAT_CELLS, fit->cellsPerRow, fit->cellSpark);
}

/* A TABLE MUST FIT THE WIDTH IT IS GIVEN. Nobody scrolls a table sideways to
   find a column, so the narrow arm DROPS the two a file list is least often
   read for and carries their values under each name instead, with a chip at the
   top right saying how many went - without it a reader cannot tell a table that
   is showing everything from one that is not. */
#define FILE_COLS 4   /* name, type, size, date */

static void page_files(RC_App *app, AppState *st, const Fit *fit) {
    RC_Style  s   = rcGetStyle();
    RC_Arena *mem = rcAppArena(app);
    static const struct {
        const char *name; const char *type; const char *size; const char *date;
    } files[] = {
        {"rayclay.h",         "header", "142 KB", "Jun 26"},
        {"main.c",            "source", "38 KB",  "Jun 25"},
        {"app/theme.h",       "header", "29 KB",  "Jun 28"},
        {"roboto-latin1.ttf", "font",   "14 KB",  "Jun 19"},
        {"CMakeLists.txt",    "config", "8 KB",   "Jun 22"},
        {"dashboard.html",    "web",    "623 B",  "Jun 26"},
    };
    static const char *const row_ids[] = {
        "file0", "file1", "file2", "file3", "file4", "file5",
    };

    rcColumn(.gap = 2) {
        rcTextL("Files",                                .font = F_HEAD,  .color = s.text);
        rcTextL("Everything this app ships.", .font = F_SMALL, .color = s.textMuted);
    }
    const int rows  = (int)(sizeof files / sizeof files[0]);
    const int shown = fit->compact ? 2 : FILE_COLS;

    rcColumn(.id = "panel_files", .bg = s.surface,
             .border = { .color = s.border, .width = "1px" },
             .w = "grow",
             .shadow = { .color = rcAlpha(RC_BLACK, 110), .y = 10, .blur = 30,
                         .spread = -6 },
             .className = "panel gap-0.5") {
        rcRow(.pb = 6, .align = "cl", .w = "grow") {
            rcText(rcFormat(mem, "%d files", rows), .font = F_SMALL, .color = s.textMuted);
            rcBox(.w = "grow") {}
            /* The tooltip says WHERE the values went, in one short line. A
               tooltip that fits is pushed left until it is inside the viewport,
               but one WIDER than the viewport is painted off the right edge -
               no wrap, no ellipsis, no warning. Keeping the line short is your
               job, not the library's. */
            if (shown < FILE_COLS) {
                rcBox(.id = "files_hidden", .bg = s.surfaceAlt, .px = 8, .py = 3,
                      .borderRadius = "all-full",
                      .tooltip = "Size and date are under each name.") {
                    rcText(rcFormat(mem, "+%d columns", FILE_COLS - shown),
                            .font = F_SMALL, .color = s.textMuted);
                }
            }
        }
        /* The header carries the body row's 3 px selection rule as a spacer, so
           the two line up whatever the selection is. */
        rcRow(.gap = 8, .px = 12, .align = "cl", .w = "grow", .h = "36px") {
            rcBox(.w = "3px") {}
            rcBox(.w = "grow") { rcTextL("Name", .font = F_SMALL, .color = s.textMuted); }
            rcBox(.w = "72px") { rcTextL("Type", .font = F_SMALL, .color = s.textMuted); }
            if (shown == FILE_COLS) {
                rcBox(.w = "72px") { rcTextL("Size", .font = F_SMALL, .color = s.textMuted); }
                rcBox(.w = "60px") { rcTextL("Date", .font = F_SMALL, .color = s.textMuted); }
            }
        }
        for (int i = 0; i < rows; i++) {
            /* .hMin rather than a fixed height: the compact row carries a
               second line and a fixed 44 would clip it, while a floor leaves
               the wide rows exactly as tall as they were. */
            bool picked = (st->fileSel == i);
            rcRow(.id = row_ids[i],
                  .bg = picked ? rcAlpha(s.primary, 46)
                               : (rcIsHovered(row_ids[i]) ? s.surfaceAlt
                                                          : RC_TRANSPARENT),
                  .gap = 8, .px = 12, .align = "cl", .borderRadius = "all-md",
                  .w = "grow", .hMin = 44) {
                rcBox(.bg = picked ? s.primary : RC_TRANSPARENT,
                      .borderRadius = "all-full", .w = "3px", .h = "18px") {}
                rcColumn(.gap = 2, .w = "grow") {
                    rcTextC(files[i].name, .font = F_BODY, .color = s.text);
                    if (shown < FILE_COLS) {
                        rcText(rcFormat(mem, "%s \xc2\xb7 %s", files[i].size, files[i].date),
                                .font = F_SMALL, .color = s.textMuted);
                    }
                }
                rcBox(.w = "72px") {
                    rcBox(.bg = s.surfaceAlt, .px = 8, .py = 3, .borderRadius = "all-full") {
                        rcTextC(files[i].type, .font = F_SMALL, .color = s.textMuted);
                    }
                }
                if (shown == FILE_COLS) {
                    rcBox(.w = "72px") {
                        rcTextC(files[i].size, .font = F_SMALL, .color = s.textMuted);
                    }
                    rcBox(.w = "60px") {
                        rcTextC(files[i].date, .font = F_SMALL, .color = s.textMuted);
                    }
                }
            }
            /* Polled OUTSIDE the row: rcClicked is what makes the row a button
               and what gives it the pointer cursor, and a row that reacts to the
               pointer without one is a promise the app does not keep. */
            if (rcClicked(row_ids[i])) st->fileSel = i;
        }
    }
}

static void page_settings(RC_App *app, AppState *st, const Fit *fit) {
    RC_Style s = rcGetStyle();
    rcColumn(.gap = 2) {
        rcTextL("Settings",             .font = F_HEAD,  .color = s.text);
        rcTextL("Project configuration.",.font = F_SMALL, .color = s.textMuted);
    }
    rcColumn(.id = "panel_settings", .bg = s.surface,
             .border = { .color = s.border, .width = "1px" },
             .w = "grow",
             .shadow = { .color = rcAlpha(RC_BLACK, 110), .y = 10, .blur = 30,
                         .spread = -6 },
             .className = "panel gap-3.5") {
        rcRow(.gap = 12, .align = "cl", .w = "grow") {
            rcBox(.w = "120px") {
                rcTextL("Project name", .font = F_SMALL, .color = s.textMuted);
            }
            rcBox(.w = "grow") {
                rcTextInput("in_project", st->project, sizeof st->project,
                             .placeholder = "my-rayclay-app");
            }
        }
        rcRow(.gap = 12, .align = "cl", .w = "grow") {
            rcBox(.w = "120px") {
                rcTextL("Notifications", .font = F_SMALL, .color = s.textMuted);
            }
            rcToggle("tg_notify", &st->notifications);
            rcTextC(st->notifications ? "On" : "Off",
                     .font = F_SMALL, .color = s.textMuted);
        }
        rcRow(.gap = 12, .align = "cl", .w = "grow") {
            rcBox(.w = "120px") {
                rcTextL("Monthly budget", .font = F_SMALL, .color = s.textMuted);
            }
            rcBox(.w = "grow") { rcSlider("sl_budget", &st->budget, 0.0f, 1.0f); }
            RC_String amt = rcFormat(rcAppArena(app), "$%s",
                                      thousands(rcAppArena(app),
                                                (int)(st->budget * 5000.0f + 0.5f)).chars);
            rcText(amt, .font = F_SMALL, .color = s.textMuted);
        }
        rcRow(.gap = 12, .align = "cl", .w = "grow") {
            rcBox(.w = "120px") {
                rcTextL("Plan", .font = F_SMALL, .color = s.textMuted);
            }
            /* 120 + 12 + 220 inside the panel's and the column's padding is
               424 px, so on the compact arm the combo takes what is left of
               the row instead of pushing the root past 411. */
            rcBox(.w = fit->compact ? "grow" : "220px") {
                rcCombo("cb_plan", &st->plan, PLANS, PLAN_COUNT);
            }
        }
        /* Settings apply live (immediate mode: every widget writes AppState the
           frame it changes), so there is no dead "Save" - just a working reset. */
        rcRow(.gap = 10, .align = "cl") {
            if (rcButton("btn_reset", "Reset to defaults", RC_BTN_DEFAULT)) {
                st->project[0]    = '\0';
                st->notifications = true;
                st->budget        = 0.6f;
                st->plan          = 1;
            }
            rcTextL("Changes apply instantly.", .font = F_SMALL, .color = s.textMuted);
        }
    }
}

static void layout(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    rcSetStyle(console_style(st->darkMode));
    RC_Style   s  = rcGetStyle();
    /* A RUNTIME THEME SWITCH HAS TO MOVE THE WINDOW TOO. rcSetStyle changes every
       colour the UI draws with, but the window's clear colour is resolved once at
       creation, so without this line the old theme stays wherever your layout does
       not cover the window. Safe every frame: the setter is change-gated. */
    rcWindowSetClearColor(rcAppMainWindow(app), s.background);

    /* SAFE AREA. A phone draws the window edge to edge, UNDER the status bar and
       UNDER the home indicator, and nothing moves your content out of the way.
       Ask for the margins and spend them ONCE, here at the root: they belong to
       the window, not to any widget. rcViewport().safe hands them over ALREADY
       IN LAYOUT UNITS, so there is no arithmetic to get wrong. NEVER divide
       rcGetSafeAreaInsets() by the zoom instead - right under RC_ZOOM_LAYOUT,
       wrong under RC_ZOOM_OPTICAL. They are {0,0,0,0} on desktop unless
       RAYCLAY_SAFE_INSETS stands a phone's bands in, so this is one code path on
       every platform.

       SPENT BY WHOEVER SITS ON THEM. On the wide arm that is the root. On the
       compact arm the chrome takes its own - the band the top and the sides, the
       strip and the page the sides - so their surfaces reach the window edge and
       only their content moves in. `band` is what the compact chrome spends. */
    RC_Insets safe = rcViewport().safe;
    Fit       fit  = fit_viewport();
    RC_Insets band = { 0 };
    if (fit.compact) {
        band.top   = safe.top;
        band.left  = safe.left;
        band.right = safe.right;
    }

    rcColumn(.id = "Root", .bg = s.background, .pt = (uint16_t)(safe.top - band.top),
             .pb = (uint16_t)(safe.bottom), .pl = (uint16_t)(safe.left - band.left),
             .pr = (uint16_t)(safe.right - band.right), .w = "grow", .h = "grow") {
        topbar(app, st, &fit, band);
        if (fit.compact) nav_strip(st, band);
        rcRow(.id = "Body", .pl = (uint16_t)band.left, .pr = (uint16_t)band.right,
              .w = "grow", .h = "grow") {
            if (!fit.compact) sidebar(app, st, &fit);
            rcColumn(.id = "Content", .bg = s.background, .gap = 16, .p = CONTENT_PAD,
                     .scroll = "v", .w = "grow", .h = "grow") {
                switch (st->page) {
                case 0: page_dashboard(app, st, &fit); break;
                case 1: page_analytics(&fit);        break;
                case 2: page_files(app, st, &fit);   break;
                case 3: page_settings(app, st, &fit); break;
                default: break;
                }
                /* The badge lost its sidebar, so it closes the page instead: a
                   spacer parks it at the bottom of a short page and it scrolls
                   into view on a long one. The pill and the readout the topbar
                   could not hold sit beside it; the column has a width floor so
                   the badge's edge does not move when the readout gains a
                   digit. */
                if (fit.compact) {
                    rcBox(.w = "grow", .h = "grow") {}
                    rcRow(.gap = 10, .align = "cl", .w = "grow") {
                        brand_badge();
                        rcColumn(.gap = 4, .align = "cr", .wMin = 120) {
                            console_pill();
                            live_readout(app);
                        }
                    }
                }
            }
        }
    }
    rcScrollbar("Content");
}

int main(void) {
    AppState state = {
        .darkMode      = true,
        .notifications = true,
        .budget        = 0.6f,
        .plan          = 1,
    };

    static const float fontSizes[F_COUNT] = {
        [F_SMALL] = 13.0f,
        [F_BODY]  = 15.0f,
        [F_HEAD]  = 20.0f,
        [F_STAT]  = 30.0f,
        [F_HERO]  = 46.0f,
    };

    rcSetStyle(console_style(true));

    /* Every panel on this screen is the same shell, so it is one named class in
       the .className (Tailwind v4) grammar and each site adds only the gap it
       wants. Colours stay typed fields because they come from the live theme,
       which a class string cannot name - and a typed field overrides the class,
       so the two compose in the direction you expect. */
    rcDefineClass("panel", "p-4 rounded-xl");

    RC_AppOptions opts = {
        .width          = 1180,
        .height         = 760,
        .title          = "RayClay Console",
        .clearColor     = rcGetStyle().background,
        .fontSizes      = fontSizes,
        .fontCount      = F_COUNT,
        .scratchArenaBytes = 4096,   /* backs every rcFormat in a frame */
        .nativeFrame    = true,
        .titlebarHeight = 56,
        .layoutCallback       = layout,
        .userData       = &state,
        .titlebar       = { .custom = true },   /* the dashboard topbar IS the titlebar */
        /* A per-frame readout IS an animation: the text changes every frame, so
           the picture never settles and the window can never park. A deliberate
           trade for a console-styled demo. An app with no such readout should
           leave the default RC_RENDER_ON_DEMAND and park at ~0 CPU. */
        .renderMode     = RC_RENDER_CONTINUOUS,
    };

    return rcRunApp(&opts);
}
