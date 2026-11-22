/*
    main.c - RayClay 2020s GUI: a project console in the Linear and Vercel mold

    Near-black chrome around an inset page, soft glow, gradient accents, glass
    panels, big confident numbers and a command palette (Cmd/Ctrl+K). Every control
    works on data this file seeds, so the default run opens no socket.

    What to copy from it:

    - Motion without a timeline: each animated number walks toward its target in
      approach(), a frame-rate independent ease that a new target simply redirects.
      The window asks for frames only while something moves or a build runs, and
      sleeps the rest of the time.
    - A chart that morphs between metrics of different scales animates each trace as
      fractions of a full scale and plots them in real units, so a switch morphs the
      shape while the axis fits its own round steps.
    - Theming: an RC_Style for the widgets plus a small Palette struct for what a style
      has no role for (glass, glow, shadow, a picked row's fill), one set per theme.
    - Looks by state: a row's hover is its class's hover:, named once with rcDefineClass,
      not a branch on rcShowsHover; a control's height is its class's h-*, under a mouse
      only: an h-* is exact, and a finger keeps the 44 px floor (mouse_h).
    - A list that fits: columns drop by importance, their values move under the
      commit's first line and a "+N columns" tip says so. It never scrolls sideways,
      and each row paints its own fill, so a selection reads as one.

    One source for desktop, phone and web (cmake --preset web -> dashboard.html).
    Zero-asset: the text is the bundled face, the icons are vector procedures.

    Build target: rayclay_ex05_2020s_gui
*/
#include "rayclay.h"

#include "icons/rc_icons_gallery_vertical_end.h"
#include "icons/rc_icons_house.h"
#include "icons/rc_icons_moon.h"
#include "icons/rc_icons_rayclay_logo.h"
#include "icons/rc_icons_rotate_cw.h"
#include "icons/rc_icons_search.h"
#include "icons/rc_icons_settings.h"
#include "icons/rc_icons_upload.h"

enum { PAGE_OVERVIEW, PAGE_DEPLOYS, PAGE_SETTINGS, PAGE_COUNT };
enum { RANGE_24H, RANGE_7D, RANGE_30D, RANGE_COUNT };
enum { M_REQUESTS, M_VISITORS, M_LATENCY, M_ERRORS, METRIC_COUNT };
enum { THEME_SYSTEM, THEME_LIGHT, THEME_DARK, THEME_COUNT };
enum { ST_READY, ST_BUILDING, ST_FAILED, ST_ANY };
enum { COL_STATUS, COL_BRANCH, COL_TIME, COL_AGE, COL_COUNT };
enum { PROJECT_COUNT = 3, POINTS = 48, MAX_DEPLOYS = 24, BAR_ROWS = 4 };

#define COMPACT_W  880.0f   /* below this the sidebar gives way to a tab bar */
#define BUILD_SECS 6.0      /* how long a simulated deployment takes         */
#define SPENT      642.0f   /* this month's spend, against the limit         */

typedef struct {
    const char *name;
    float       scale[RANGE_COUNT];   /* what a trace's fraction of 1 stands for */
    float       base, swing;          /* the trace's floor and its daily swing   */
    bool        sums;                 /* a count: it adds up and scales with traffic */
} Metric;

static const Metric METRICS[METRIC_COUNT] = {
    { "Requests",    { 100000, 700000, 3000000 }, 0.16f, 0.62f, true },
    { "Visitors",    { 8000, 50000, 200000 },     0.20f, 0.56f, true },
    { "p95 latency", { 250, 250, 250 },           0.40f, 0.20f, false },
    { "Error rate",  { 0.50f, 0.50f, 0.50f },     0.22f, 0.18f, false }, };

static const char *const RANGES[RANGE_COUNT]     = { "24h", "7d", "30d" };
static const char *const RANGE_NOW[RANGE_COUNT]  = { "Last 24 hours", "Last 7 days",
                                                     "Last 30 days" };
static const char *const THEMES[THEME_COUNT]     = { "System", "Light", "Dark" };
static const char *const REGIONS[4] = { "Frankfurt", "N. Virginia", "Tokyo", "Sydney" };
static const char *const PAGES[PAGE_COUNT] = { "Overview", "Deployments", "Settings" };
static const char *const STATUSES[ST_ANY + 1] = { "Ready", "Building", "Failed", "All" };
static const RC_IconCallback ICONS[] = { rcIconHouse, rcIconGalleryVerticalEnd, rcIconSettings };

/* The projects' names, one list for the sidebar's rows and the compact layout's combo. */
static const char *const SLUGS[PROJECT_COUNT] = { "halcyon-web", "halcyon-api", "halcyon-docs" };

/* share is a project's traffic against the largest; trend is how far each metric drifts
   from the last period, so each project tells its own story and some changes are good news. */
static const struct { float share, trend[METRIC_COUNT]; uint32_t from, to; } PROJECTS[] = {
    { 1.00f, { 0.07f, 0.05f, -0.04f, 0.02f }, 0x6366F1, 0xA855F7 },
    { 0.64f, { 0.04f, -0.03f, 0.05f, -0.06f }, 0x06B6D4, 0x3B82F6 },
    { 0.31f, { -0.03f, 0.06f, -0.05f, -0.02f }, 0xF59E0B, 0xEC4899 }, };

/* A project's busiest paths, ranked; each bar is a share of all its requests. */
static const float PATH_SHARES[BAR_ROWS] = { 0.31f, 0.18f, 0.12f, 0.08f };
static const char *const PATHS[PROJECT_COUNT][BAR_ROWS] = {
    { "/", "/pricing", "/blog", "/changelog" },
    { "/v1/events", "/v1/webhooks", "/v1/projects", "/healthz" },
    { "/docs/start", "/docs/cli", "/docs/hooks", "/search" }, };

typedef struct {
    int    project, status;
    bool   production, redeploy;
    int    seconds;
    double startedAt;
    char   sha[8], message[48], branch[16], author[16];
} Deployment;

/* startedAt is on the app clock, so a build seeded 12 minutes before launch reads -720. */
static const Deployment SEED_DEPLOYS[] = {
    { 0, ST_READY, 1, 0, 47, -720, "9f3c1ab", "Cache the pricing page", "main", "Mara" },
    { 0, ST_READY, 0, 0, 52, -3480, "c41d7e2", "Fix docs sidebar flash", "fix/docs", "Theo" },
    { 0, ST_FAILED, 0, 0, 19, -7800, "7ab02f9", "Upgrade the optimizer", "deps/img", "Renata" },
    { 0, ST_READY, 1, 0, 49, -22200, "e07b6d3", "Trim homepage by 38 KB", "main", "Jun" },
    { 0, ST_READY, 1, 0, 46, -174000, "04de1f7", "Redirect old blog routes", "main", "Mara" },
    { 1, ST_READY, 1, 0, 31, -1560, "aa17c3e", "Rate limit event intake", "main", "Renata" },
    { 1, ST_FAILED, 0, 0, 12, -4260, "5d09bb1", "Retry webhooks on 5xx", "feat/retry", "Jun" },
    { 1, ST_READY, 1, 0, 29, -36600, "6e4ab90", "Rotate signing keys", "main", "Renata" },
    { 2, ST_READY, 1, 0, 22, -3900, "d90a4c8", "Document deploy hooks", "main", "Theo" },
    { 2, ST_READY, 0, 0, 24, -43200, "31f6e05", "Fix anchors in CLI guide", "fix/cli", "Jun" },
    { 2, ST_FAILED, 0, 0, 9, -54000, "a3e57c1", "Move search to the edge", "feat/search", "Mara" },
    { 2, ST_READY, 1, 0, 21, -186000, "7c90e2b", "Rewrite the quickstart", "main", "Theo" }, };

/* The commit each project's main is at: what Deploy ships. */
static const Deployment HEADS[PROJECT_COUNT] = {
    { 0, ST_BUILDING, 1, 0, 0, 0, "e5a1c09", "Ship the new pricing table", "main", "Mara" },
    { 1, ST_BUILDING, 1, 0, 0, 0, "07bd34f", "Stream events over HTTP/2", "main", "Renata" },
    { 2, ST_BUILDING, 1, 0, 0, 0, "c9e2718", "Add the API reference", "main", "Theo" }, };

/* Every trace, computed once and only read after that. The totals are read off the traces,
   so no figure on screen can disagree with the line beside it. */
static float TRACES[PROJECT_COUNT][METRIC_COUNT][RANGE_COUNT][2][POINTS];
static float TOTALS[PROJECT_COUNT][METRIC_COUNT][RANGE_COUNT][2];

/* What a style has no role for. Each theme has its own: a black shadow that lifts a card on
   near-black is a dirty smudge on white, and a picked row is a white wash on near-black but
   an accent wash on white. */
typedef struct {
    RC_Gradient page, glass;
    RC_Border   rim;
    RC_Shadow   lift, glow;
    const char *picked;   /* a picked row's class: "pickable" and its fill */
} Palette;

typedef struct {
    Palette    pal;
    RC_Arena  *frame;   /* this frame's arena, for the chart's label callbacks */
    bool       compact, dark, cmdOpen, alerts;
    int        page, project, range, metric, theme, statusFilter, selected, cmdSel;
    int        region[PROJECT_COUNT], deployCount;
    /* shown: the two traces as they animate, in fractions; plot: the same in real units. */
    float      spendLimit, reveal, shown[2][POINTS], plot[2][POINTS], totals[METRIC_COUNT];
    char       cmdQuery[48], deployQuery[48];
    Deployment deploys[MAX_DEPLOYS];
    /* endsAt: the chart's last point, the clock's latest half hour; 0 with no clock reading. */
    double     lastFrame, endsAt;
} AppState;

static RC_Style app_style(bool dark) {
    RC_Style s = dark ? rcStyleDark() : rcStyleLight();

    s.background   = rcHex(dark ? 0x08090A : 0xECEDF0);
    s.surface      = rcHex(dark ? 0x141518 : 0xFFFFFF);
    s.surfaceAlt   = rcHex(dark ? 0x26282D : 0xE6E7EB);
    s.text         = rcHex(dark ? 0xF7F8F8 : 0x16171A);
    s.textMuted    = rcHex(dark ? 0x8F949D : 0x62666D);
    s.border       = rcHex(dark ? 0x2A2C31 : 0xD9DBE0);
    s.primary      = rcHex(dark ? 0x6E78E6 : 0x5661D6);
    s.primaryHover = rcHex(dark ? 0x8A93F0 : 0x4650C2);
    return s;
}

static Palette app_palette(bool dark) {
    Palette p;

    p.page   = RC_LIT(RC_Gradient){ rcHex(dark ? 0x111221 : 0xF4F4FE),
                                    rcHex(dark ? 0x0A0B0D : 0xFBFBFC), "v" };
    p.glass  = RC_LIT(RC_Gradient){ rcWithAlpha(RC_WHITE, dark ? 0.063f : 1.0f),
                                    rcWithAlpha(RC_WHITE, dark ? 0.02f : 0.98f), "v" };
    p.rim    = RC_LIT(RC_Border){ dark ? rcWithAlpha(RC_WHITE, 0.1f) : rcHex(0xDFE1E6), 1 };
    p.lift   = dark ? RC_LIT(RC_Shadow){ rcWithAlpha(RC_BLACK, 0.353f), 0, 8, 24, -8 }
                    : RC_LIT(RC_Shadow){ rcWithAlpha(rcHex(0x1E2340), 0.086f), 0, 4, 14, -4 };
    p.glow   = dark ? RC_LIT(RC_Shadow){ rcWithAlpha(rcHex(0x6366F1), 0.157f), 0, 0, 90, -6 }
                    : RC_LIT(RC_Shadow){ rcWithAlpha(rcHex(0x6366F1), 0.133f), 0, 8, 36, -8 };
    p.picked = dark ? "pickable bg-foreground/9" : "pickable bg-primary/12";
    return p;
}

/* A daily or weekly swell, the project's trend and some hash noise: stable from run to run,
   so capture rows stay put. The swell is sin squared, by Bhaskara's rational sine, as an
   example includes nothing but rayclay.h. Each day or week peaks at its own height, hashed
   per metric, so no two traces repeat one regular wave; the last period shares its peaks,
   so the change between the two stays the project's trend. The day is mixed in by an xor
   before the multiply: a plain sum would step every metric's heights in the same zigzag. */
static float trace_point(int p, int metric, int range, int then, int i) {
    static const float cycles[RANGE_COUNT] = { 1.0f, 7.0f, 4.3f };
    const Metric *m = &METRICS[metric];
    float t = (float)i / (float)(POINTS - 1);
    float turn = cycles[range] * t + 0.92f - 0.08f * (float)metric - 0.05f * (float)p;
    float f = turn - (float)(int)turn;
    float g = 4.0f * f * (1.0f - f), arc = 4.0f * g / (5.0f - g);   /* Bhaskara's sin(pi f) */
    unsigned h = (unsigned)(i + metric * 53 + range * 97 + then * 211 + p * 389) * 2654435761u;
    unsigned c = (unsigned)((int)turn * 40503 ^ (metric * 31 + range * 13 + p * 17)) * 2246822519u;
    float noise = (float)((h >> 13) % 100u) / 100.0f - 0.5f;
    float v = m->base + m->swing * (0.6f + (float)(c >> 26) / 160.0f) * arc * arc + 0.08f * noise;

    if (!then) v += 2.0f * t * PROJECTS[p].trend[metric];   /* averages to the trend */
    if (p == 0 && metric == M_ERRORS && !then && i >= 30 && i < 34) v += 0.34f;   /* incident */
    return v < 0.02f ? 0.02f : (v > 0.98f ? 0.98f : v);
}

static void seed_traces(void) {
    for (int p = 0; p < PROJECT_COUNT; p++) for (int m = 0; m < METRIC_COUNT; m++)
            for (int r = 0; r < RANGE_COUNT; r++) for (int then = 0; then < 2; then++) {
                    float sum = 0.0f;
                    for (int i = 0; i < POINTS; i++)
                        sum += TRACES[p][m][r][then][i] = trace_point(p, m, r, then, i);
                    TOTALS[p][m][r][then] = sum * METRICS[m].scale[r]
                        * (METRICS[m].sums ? PROJECTS[p].share : 1.0f / (float)POINTS);
                }
}

/* A figure in its metric's units. One that animates keeps a fixed precision, so its width
   holds while it counts (44.9M, 45.0M); a tick names a round number and prints it short. */
static RC_String format_value(RC_Arena *mem, int metric, double v, bool tick) {
    if (metric == M_LATENCY) return rcFormat(mem, "%.0f ms", v);
    if (metric == M_ERRORS)  return rcFormat(mem, tick ? "%.3g%%" : "%.2f%%", v);
    if (v >= 999500.0)       return rcFormat(mem, tick ? "%.3gM" : "%.1fM", v / 1e6);
    if (v >= 1000.0)         return rcFormat(mem, tick ? "%.3gk" : "%.1fk", v / 1e3);
    return rcFormat(mem, "%.0f", v);
}

static const char *money(RC_Arena *mem, float v) {
    return rcFormat(mem, "$%s", rcFormatInt(mem, (int64_t)v, NULL).chars).chars;
}

static const char *age(RC_Arena *mem, double secs) {
    if (secs < 60.0)    return "Just now";
    if (secs < 3600.0)  return rcFormat(mem, "%dm ago", (int)(secs / 60.0)).chars;
    if (secs < 86400.0) return rcFormat(mem, "%dh ago", (int)(secs / 3600.0)).chars;
    return rcFormat(mem, "%dd ago", (int)(secs / 86400.0)).chars;
}

static int put_label(RC_String text, char *out, int cap) {
    rcStrCopy(out, text.chars, (size_t)cap);
    return (int)text.length;
}

/* A y tick or a readout, in the selected metric's units. */
static int format_y(double v, char *out, int cap, void *user) {
    const AppState *st = (const AppState *)user;
    return put_label(format_value(st->frame, st->metric, v, true), out, cap);
}

/* A point's moment on the user's clock and calendar. The last point is st->endsAt and each one
   before it is a 48th of the range earlier: half an hour over 24h, 3.5 hours over 7d and 15
   over 30d. With no clock reading, or no answer from the calendar, a tick prints its number. */
static int format_x(double v, char *out, int cap, void *user) {
    static const char *const MONTHS[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
                                            "Aug", "Sep", "Oct", "Nov", "Dec" };
    const AppState *st   = (const AppState *)user;
    const double    days = st->range == RANGE_24H ? 1.0 : (st->range == RANGE_7D ? 7.0 : 30.0);
    const double    at   = st->endsAt - (POINTS - 1 - v) * days * 86400.0 / POINTS;
    RC_LocalTime    t;

    if (st->endsAt <= 0.0 || !rcLocalTime(at, &t)) return -1;   /* -1: the built-in text */
    return put_label(st->range == RANGE_24H ? rcFormat(st->frame, "%02d:%02d", t.hour, t.minute)
                     : rcFormat(st->frame, "%s %d", MONTHS[t.month - 1], t.day), out, cap);
}

/* One animated number: it covers the same share of the remaining gap per unit of time, so
   it eases out at any frame rate and a new target needs no bookkeeping. */
static bool approach(float *value, float target, float rate) {
    const float gap = target - *value, slack = 0.0002f * (target + 1.0f);
    *value = gap <= slack && gap >= -slack ? target : *value + gap * rate;
    return *value != target;
}

static void animate(RC_App *app, AppState *st) {
    const float step = 9.0f * (float)(rcAppTime(app) - st->lastFrame);
    /* A window that slept for minutes resumes from one frame's step, not from a jump. */
    const float rate = step > 0.45f ? 0.31f : step / (1.0f + step);
    bool  moving = approach(&st->reveal, 1.0f, rate);

    st->lastFrame = rcAppTime(app);
    for (int then = 0; then < 2; then++) for (int i = 0; i < POINTS; i++)
            moving |= approach(&st->shown[then][i],
                               TRACES[st->project][st->metric][st->range][then][i], rate);
    for (int m = 0; m < METRIC_COUNT; m++)
        moving |= approach(&st->totals[m], TOTALS[st->project][m][st->range][0], rate);
    /* The next frame while numbers move; else one in 30 s, so a deploy's age keeps time. */
    rcWindowRequestFrameAfter(rcAppMainWindow(app), moving ? 0.0 : 30.0);
}

/* ASCII letters folded by hand: strcasestr is not on every platform this builds for. Only A-Z
   fold: an OR of 32 into every byte would match '@' to '`', '[' to '{' and UTF-8 bytes to
   others. */
static char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 'a' - 'A') : c; }

static bool contains(const char *text, const char *query) {
    for (const char *p = text; ; p++) {
        size_t i = 0;
        while (query[i] && p[i] && lower(p[i]) == lower(query[i])) i++;
        if (!query[i] || !*p) return !query[i];   /* an empty query matches at once */
    }
}

/* Whether the Deployments list shows a build: this project's, past the filter and search. */
static bool listed(const AppState *st, const Deployment *d) {
    return d->project == st->project
        && (st->statusFilter == ST_ANY || d->status == st->statusFilter)
        && (contains(d->message, st->deployQuery) || contains(d->branch, st->deployQuery)
            || contains(d->author, st->deployQuery));
}

/* Opens the full list on one deployment, with nothing filtering it out of view. */
static void show_deploy(AppState *st, int index) {
    st->page           = PAGE_DEPLOYS;
    st->statusFilter   = ST_ANY;
    st->deployQuery[0] = '\0';
    st->selected       = index;
}

/* A new build of a commit; a redeploy keeps the commit, its branch and its author. */
static void start_deploy(RC_App *app, AppState *st, const Deployment *from, bool redeploy) {
    Deployment d = *from;

    d.status    = ST_BUILDING;
    d.redeploy  = redeploy;
    d.startedAt = rcAppTime(app);
    if (st->deployCount < MAX_DEPLOYS) st->deployCount++;   /* full, the oldest drops off */
    for (int i = st->deployCount - 1; i > 0; i--) st->deploys[i] = st->deploys[i - 1];
    st->deploys[0] = d;
    show_deploy(st, 0);
}

static void set_project(AppState *st, int project) {
    st->project  = project;
    st->selected = -1;   /* a selection names a row of the old project's list */
}

/* Builds finish on the app clock. While one runs the window keeps drawing its progress. */
static void tick_builds(RC_App *app, AppState *st) {
    for (int i = 0; i < st->deployCount; i++) {
        Deployment *d = &st->deploys[i];
        if (d->status != ST_BUILDING) continue;
        rcWindowRequestFrame(rcAppMainWindow(app));
        if (rcAppTime(app) - d->startedAt < BUILD_SECS) continue;
        d->status  = ST_READY;
        d->seconds = (int)BUILD_SECS;
    }
}

/* The class of anything selectable: "pickable" tints it under the pointer, and a picked one
   takes its theme's fill. The hover is quieter than the selection on purpose, so the eye
   lands on where you are, not on where the pointer happens to be. */
static const char *row_class(const AppState *st, bool on) {
    return on ? st->pal.picked : "pickable";
}

static void status_dot(int status) {
    RC_Style s = rcGetStyle();
    RC_Color c = status == ST_READY ? s.success : (status == ST_BUILDING ? s.warning : s.danger);
    rcBox(.bg = c, .borderRadius = "all-full", .w = "8px", .h = "8px",
          .shadow = { rcWithAlpha(c, 0.314f), 0, 0, 6, 0 }) {}
}

static void swatch(int project) {
    rcBox(.borderRadius = "all-sm", .w = "16px", .h = "16px",
          .gradient = { rcHex(PROJECTS[project].from), rcHex(PROJECTS[project].to), "d" }) {}
}

/* A control's class height, under a mouse only. An h-* is exact, so on a phone it would cut a
   button, field or combo below the 44 px a coarse pointer gives it; with no class it keeps
   that floor. No pointer-coarse: variant exists yet, so the branch is in C. */
static const char *mouse_h(const char *h) { return rcPointerIsCoarse() ? NULL : h; }

/* A row this file builds and a finger picks (a segment, a sidebar row, a command): 36 px under
   a mouse, and the bundled controls' 44 px floor under a coarse pointer. */
static const char *row_h(void) { return rcPointerIsCoarse() ? "44px" : "36px"; }

static void deploy_button(RC_App *app, AppState *st) {
    if (rcButton("deploy", "Deploy", .className = mouse_h("h-9"), .variant = RC_BTN_PRIMARY,
                 .leadingIcon = rcIconUpload))
        start_deploy(app, st, &HEADS[st->project], false);
}

static void segment(RC_Arena *mem, const char *id, const char *const *labels, int count, int *sel) {
    RC_Style s = rcGetStyle();
    rcRow(.bg = s.surface, .gap = 2, .p = 2, .borderRadius = "all-md",
          .border = { s.border, 1 }, .h = row_h()) {
        for (int i = 0; i < count; i++) {
            const char *seg = rcFormat(mem, "%s_%d", id, i).chars;
            bool on = *sel == i;
            rcBox(.id = seg, .focusable = true, .bg = on ? s.surfaceAlt : RC_UNSET_COLOR, .px = 12,
                  .align = "cc", .borderRadius = "all-sm", .h = "grow") {
                rcTextC(labels[i], .color = on || rcShowsHover(seg) ? s.text : s.textMuted,
                        .size = 13, .select = RC_SELECT_NONE);
            }
            if (rcClicked(seg)) *sel = i;
        }
    }
}

/* A sidebar row: a page with its icon, or a project (icon NULL) with its gradient swatch.
   Compact, it is a tab of the phone's tab bar: its icon over its label, centered in its share
   of the bar, as a phone's tabs are. */
static bool side_row(const AppState *st, const char *id, const char *label, RC_IconCallback icon,
                     int project, bool on) {
    RC_Style s = rcGetStyle();
    RC_Color ink = on ? (st->compact ? s.primary : s.text) : s.textMuted;
    /* A project is marked by its swatch and label; a phone's tab by its accent, as iOS tints it. */
    rcRow(.id = id, .focusable = true, .gap = 10, .px = 10, .align = st->compact ? "cc" : "cl",
          .borderRadius = "all-md", .w = "grow", .h = st->compact ? "44px" : row_h(),
          .className = st->compact ? "flex-col gap-0.5" : row_class(st, on && icon)) {
        if (icon) icon(16.0f, ink);
        else swatch(project);
        rcTextC(label, .color = ink, .size = 14, .select = RC_SELECT_NONE);
    }
    return rcClicked(id);
}

static void sidebar(RC_App *app, AppState *st) {
    rcColumn(.id = "Sidebar", .gap = 2, .pt = 4, .pb = 8, .px = 12, .scroll = "v",
             .h = "grow", .wType = RC_PX(232)) {
        for (int i = 0; i < PAGE_COUNT; i++)
            if (side_row(st, rcFormat(rcAppArena(app), "nav_%d", i).chars, PAGES[i], ICONS[i],
                         -1, st->page == i)) st->page = i;
        rcBox(.pt = 14, .pb = 4, .px = 10)
            rcTextL("Projects", .color = rcGetStyle().textMuted, .size = 12);
        for (int i = 0; i < PROJECT_COUNT; i++)
            if (side_row(st, rcFormat(rcAppArena(app), "proj_%d", i).chars, SLUGS[i], NULL, i,
                         st->project == i)) set_project(st, i);
    }
    rcScrollbar("Sidebar");
}

/* A phone's floating tab bar. The page runs under it, as it does under the home indicator,
   and clears both at the end of its scroll. */
static void tab_bar(RC_App *app, AppState *st, RC_Insets safe) {
    rcRow(.bg = rcGetStyle().surface, .gap = 4, .pt = 4, .pb = (float)(4.0f + safe.bottom),
          .px = 8, .border = { rcGetStyle().border, 1 }, .wType = RC_VW(100),
          .floating = { .to = RC_ATTACH_ROOT, .parent = RC_ANCHOR_BOTTOM_CENTER,
                        .element = RC_ANCHOR_BOTTOM_CENTER, .zIndex = 10 }) {
        for (int i = 0; i < PAGE_COUNT; i++)
            if (side_row(st, rcFormat(rcAppArena(app), "tab_%d", i).chars, PAGES[i], ICONS[i],
                         -1, st->page == i)) st->page = i;
    }
}

/* The window's title band: the OS drags it, so its controls sit in a .noDrag box. It is drawn
   inside rcUnzoomed() because the OS strip it must match does not zoom. */
static void topbar(AppState *st, RC_Insets safe) {
    rcColumn(.pt = safe.top, .pl = safe.left, .pr = safe.right, .w = "grow") {
        rcUnzoomed() {
            /* The band is drawn at window px, so its words wait on the window's width, not on
               the zoomed layout's: dividing by the scope's own scale undoes the zoom. */
            const bool words = rcViewport().width / rcUnzoomedScale() >= COMPACT_W;
            rcRow(.id = RC_ID_WINDOW_DRAG, .gap = 10, .pl = 14, .align = "cl", .w = "grow",
                  .h = "52px") {
                rcIconRayClayLogo(24.0f);
                if (words) rcTextL("RayClay Cloud", .font = rcFontSemibold(14.0f));
                rcSpacer() {}
                rcRow(.gap = 8, .align = "cl", .noDrag = true) {
                    /* Compact has no sidebar: its project list is this combo. */
                    if (st->compact && rcCombo("project", &st->project, SLUGS, PROJECT_COUNT))
                        set_project(st, st->project);
                    if (rcButton("search_btn", words ? "Search" : "", .variant = RC_BTN_GHOST,
                                 .leadingIcon = rcIconSearch)) st->cmdOpen = true;
                }
                rcWindowControls();
            }
        }
    }
}

/* Vercel's analytics head: each metric is a tab with its big number and its trace, and the
   selected one drives the chart under it. The change is read off the two traces. */
static void metric_tab(RC_Arena *mem, AppState *st, int m, float widthPct) {
    RC_Style    s    = rcGetStyle();
    const char *id   = rcFormat(mem, "metric_%d", m).chars;
    const float *tot = TOTALS[st->project][m][st->range];
    const float pct  = 100.0f * (tot[0] / tot[1] - 1.0f);
    /* Latency and errors are the two metrics where falling is the good news. */
    RC_Color    tone = (m >= M_LATENCY ? pct <= 0.0f : pct >= 0.0f) ? s.success : s.danger;
    bool        on   = st->metric == m;

    rcColumn(.id = id, .focusable = true, .gap = 4, .px = 14, .py = 12, .borderRadius = "all-lg",
             .wType = RC_PCT(widthPct), .className = row_class(st, on)) {
        rcRow(.gap = 4, .align = "cl", .w = "grow") {
            rcTextC(METRICS[m].name, .color = on ? s.text : s.textMuted, .size = 13,
                    .select = RC_SELECT_NONE);
            rcSpacer() {}
            rcText(rcFormat(mem, "%+.1f%%", (double)pct), .color = tone, .size = 12,
                   .select = RC_SELECT_NONE);
        }
        rcText(format_value(mem, m, st->totals[m], false), .font = rcFontSemibold(30.0f),
               .color = s.text, .select = RC_SELECT_NONE);
        rcBox(.w = "grow", .h = "24px") {
            rcSparkline(rcFormat(mem, "spark_%d", m).chars, TRACES[st->project][m][st->range][0],
                        POINTS, .color = on ? s.primaryHover : s.textMuted,
                        .min = RC_VAL(0.0f), .max = RC_VAL(1.0f));
        }
    }
    if (rcClicked(id)) st->metric = m;
}

/* The traces animate as fractions, so a switch morphs their shape, and plot in the metric's
   real units at its full scale, so the axis fits its own round steps. A count scales with
   the project's traffic, a latency or a rate does not. RC_Series borrows its arrays until
   the frame is drawn, and these live in AppState. */
static void traffic_chart(AppState *st) {
    RC_Style    s    = rcGetStyle();
    const float full = METRICS[st->metric].scale[st->range]
                     * (METRICS[st->metric].sums ? PROJECTS[st->project].share : 1.0f);
    RC_Series series[2] = {
        { .y = st->plot[1], .count = POINTS, .kind = RC_SERIES_LINE,
          .color = rcWithAlpha(s.textMuted, 0.667f), .label = "Previous period",
          .thickness = 1.5f },
        { .y = st->plot[0], .count = POINTS, .kind = RC_SERIES_AREA, .color = s.primary,
          .label = RANGE_NOW[st->range], .thickness = 2.0f }, };
    for (int then = 0; then < 2; then++)
        for (int i = 0; i < POINTS; i++) st->plot[then][i] = full * st->shown[then][i];
    rcChart("traffic", series, 2, .className = "text-[12px]", .x = { .ticks = 6,
            .format = format_x, .formatUserData = st }, .y = { .ticks = 5, .grid = true,
            .format = format_y, .formatUserData = st }, .legend = true, .tooltip = true);
}

/* One ranked list, Vercel's shape: a row's bar is its share of the leader, grown in by
   st->reveal. The bar floats so the text keeps its own line. A row is as tall as its bar and
   the panel's gap spaces them, so the heading gets the same room above the first bar. */
static void top_paths(RC_Arena *mem, const AppState *st) {
    RC_Style s = rcGetStyle();

    rcColumn(.gap = 8, .border = st->pal.rim, .w = "grow", .h = "grow",
             .gradient = st->pal.glass, .shadow = st->pal.lift, .className = "panel") {
        rcTextL("Top paths", .font = rcFontSemibold(14.0f), .color = s.text);
        for (int i = 0; i < BAR_ROWS; i++) {
            const float share = PATH_SHARES[i];
            rcRow(.gap = 8, .px = 8, .align = "cl", .w = "grow", .h = "28px") {
                rcBox(.bg = rcWithAlpha(s.primary, 0.204f), .borderRadius = "all-sm", .h = "28px",
                      .wType = RC_PCT(100.0f * st->reveal * share / PATH_SHARES[0]),
                      .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_CENTER_LEFT,
                                    .element = RC_ANCHOR_CENTER_LEFT,
                                    .capture = RC_CAPTURE_PASSTHROUGH }) {}
                rcTextC(PATHS[st->project][i], .color = s.text, .size = 13);
                rcSpacer() {}
                rcText(format_value(mem, M_REQUESTS, share * st->totals[M_REQUESTS], false),
                       .color = s.textMuted, .size = 13);
            }
        }
    }
}

static void page_head(const char *title, const char *subtitle) {
    rcColumn(.gap = 2, .w = "grow") {
        rcTextC(title, .className = "tracking-tight", .font = rcFontSemibold(30.0f));
        rcTextC(subtitle, .color = rcGetStyle().textMuted, .size = 14);
    }
}

/* The columns after the commit, in column order, each a fixed width so every row lines up
   under its title. A narrow table drops them by .priority, by importance Status, Age, Duration,
   Branch, so Branch goes first. */
static const struct { const char *head, *align; float w; int priority; } COLUMNS[COL_COUNT] = {
    { "Status", "cl", 100.0f, 1 }, { "Branch", "cl", 140.0f, 4 },
    { "Duration", "cr", 90.0f, 3 }, { "Age", "cr", 90.0f, 2 }, };

/* A deployment's values in the columns `cut` leaves (bit c + 1 drops column c, as
   rcTableHiddenColumns counts the table's columns after Commit): in a row's own boxes, or with
   `table`, in the table's cells. */
static void deploy_columns(uint64_t cut, const char *const *cells, bool table) {
    RC_Style s = rcGetStyle();
    for (int c = 0; c < COL_COUNT; c++) {
        if (table) rcTableNext();
        if (table || !((cut >> (c + 1)) & 1u))
            rcBox(.align = COLUMNS[c].align, .wType = table ? RC_GROW : RC_PX(COLUMNS[c].w)) {
                rcTextC(cells[c], .color = c == COL_STATUS ? s.text : s.textMuted, .size = 13,
                        .wrap = "e", .select = RC_SELECT_NONE);
            }
    }
}

/* The values' separator, a middle dot with a no-break space after it: a line that wraps
   breaks before a dot, so a dot never ends a line. */
#define SEP " \xc2\xb7\xc2\xa0"

/* One deployment as a list row, as Vercel's: its status, the message and under it what names
   the build, plus every column the width could not hold, so nothing is lost to a narrow
   window. The Overview's short list draws it as a box that Enter clicks; the Deployments page
   as a table row (`table`), a Tab stop no key clicks, so Enter is read on it, as a row's
   keydown handler would read it. Its id is its start time in ms, which no two builds share:
   its index shifts at every new build, and would hand its hover and focus to another row.
   Its message and branch are copies in the frame's arena: a run's characters are read when
   the frame is drawn, and the palette's Deploy, declared after the page, moves st->deploys. */
static bool deploy_row(RC_App *app, const AppState *st, int index, uint64_t cut, bool on,
                       bool table) {
    RC_Style          s    = rcGetStyle();
    RC_Arena         *mem  = rcAppArena(app);
    const Deployment *d    = &st->deploys[index];
    const char       *id   = rcFormat(mem, "dep_%.0f", d->startedAt * 1e3).chars;
    const double      ran  = rcAppTime(app) - d->startedAt;
    const char       *line = rcFormat(mem, "%s%s%s" SEP "%s",
                                      d->production ? "Production" SEP : "",
                                      d->redeploy ? "Redeploy of " : "", d->sha, d->author).chars;
    const char       *cells[COL_COUNT], *title = rcFormat(mem, "%s", d->message).chars;

    cells[COL_STATUS] = STATUSES[d->status];
    cells[COL_BRANCH] = rcFormat(mem, "%s", d->branch).chars;
    cells[COL_TIME]   = d->status == ST_BUILDING
                      ? rcFormat(mem, "%d%%", (int)(100.0 * ran / BUILD_SECS)).chars
                      : rcFormat(mem, "%ds", d->seconds).chars;
    cells[COL_AGE]    = age(mem, ran);
    for (int c = 0; c < COL_COUNT; c++)
        if ((cut >> (c + 1)) & 1u) line = rcFormat(mem, "%s" SEP "%s", line, cells[c]).chars;
    if (table) rcTableRow(.id = id, .className = row_class(st, on), .focusable = true);
    rcRow(.id = table ? NULL : id, .focusable = !table, .gap = 12, .px = table ? 0.0f : 12.0f,
          .py = table ? 0.0f : 8.0f, .align = "cl", .borderRadius = "all-md", .w = "grow",
          .hMin = table ? RC_UNSET_VAL : RC_VAL(48),
          .className = table ? NULL : row_class(st, on)) {
        status_dot(d->status);
        rcColumn(.gap = 2, .overflow = "hidden", .w = "grow") {
            rcTextC(title, .color = s.text, .size = 14, .wrap = "e", .select = RC_SELECT_NONE);
            /* The message may be cut, the line under it wraps: a value cut short reads as
               another value, as "5..." would for 52s. */
            rcTextC(line, .color = s.textMuted, .size = 12, .select = RC_SELECT_NONE);
        }
        if (!table) deploy_columns(cut, cells, false);
    }
    if (table) deploy_columns(cut, cells, true);
    return rcClicked(id) || (table && rcKeyPressedOn(id, RC_KEY_ENTER));
}

static void page_overview(RC_App *app, AppState *st) {
    RC_Arena   *mem = rcAppArena(app);
    const int   perRow = rcViewport().width < 620.0f ? 2 : METRIC_COUNT;
    const bool  wide   = rcViewport().width >= 1100.0f;   /* room for the two panels side by side */
    const uint64_t ageOnly = 0xEu;   /* COL_AGE alone: Status, Branch and Duration cut */
    int         count  = 0;

    /* The range sits before Deploy. Where the metrics go two to a row, a phone's width, the
       title and Deploy fill the line, so the range gets a line of its own under them. */
    rcRow(.gap = 12, .align = "cl", .w = "grow", .className = "flex-wrap") {
        page_head("Overview", rcFormat(mem, "%s, %s", SLUGS[st->project],
                                       RANGE_NOW[st->range]).chars);
        if (perRow > 2) segment(mem, "range", RANGES, RANGE_COUNT, &st->range);
        deploy_button(app, st);
    }
    if (perRow == 2) segment(mem, "range", RANGES, RANGE_COUNT, &st->range);
    /* The page's answer gets the glow, and it is the only panel that does. The chart takes
       the height the window has left, within readable bounds. */
    rcColumn(.gap = 4, .p = 8, .borderRadius = "all-xl", .border = st->pal.rim, .w = "grow",
             .h = "grow", .gradient = st->pal.glass, .shadow = st->pal.glow) {
        for (int i = 0; i < METRIC_COUNT; i += perRow) {
            rcRow(.gap = 4, .w = "grow") {
                for (int m = i; m < i + perRow; m++) metric_tab(mem, st, m, 100.0f / (float)perRow);
            }
        }
        rcBox(.pt = 8, .px = 8, .w = "grow", .h = "grow", .className = "min-h-50 max-h-120") {
            traffic_chart(st);
        }
    }
    rcRow(.gap = 16, .w = "grow", .className = wide ? "" : "flex-col") {
        /* The rows pad their own highlight, so the card pads 4 and the heading 12 more: the
           heading, the dots and the ages share the 16 px inset Top paths has. */
        rcColumn(.gap = 2, .p = 4, .border = st->pal.rim, .w = "grow", .h = "grow",
                 .gradient = st->pal.glass, .shadow = st->pal.lift, .className = "panel") {
            rcBox(.pt = 12, .pb = 4, .px = 12)
                rcTextL("Recent deployments", .font = rcFontSemibold(14.0f));
            /* The short list: a row opens that build in the full list. */
            for (int i = 0; i < st->deployCount && count < 3; i++) {
                if (st->deploys[i].project != st->project) continue;
                if (deploy_row(app, st, i, ageOnly, false, false)) show_deploy(st, i);
                count++;
            }
        }
        rcBox(.h = "grow", .wType = wide ? RC_PCT(38) : RC_GROW) { top_paths(mem, st); }
    }
}

static void page_deployments(RC_App *app, AppState *st) {
    RC_Style          s   = rcGetStyle();
    RC_Arena         *mem = rcAppArena(app);
    /* The list is a table: Commit keeps 220 px and takes the rest; the others drop by
       .priority (COLUMNS) as the width goes, each value then riding on the commit's line. */
    RC_TableColumn    cols[1 + COL_COUNT] = { { .header = "Commit", .align = "cl", .w = "grow",
                                                .wMin = RC_VAL(220) } };
    const uint64_t    cut = rcTableHiddenColumns("deploys");
    /* A selection counts only while its row is on screen: Redeploy acts on what you see. */
    const Deployment *sel = st->selected >= 0 && listed(st, &st->deploys[st->selected])
                          ? &st->deploys[st->selected] : NULL;
    int hidden = 0, matches = 0;

    for (int c = 0; c < COL_COUNT; c++) {
        cols[c + 1] = RC_LIT(RC_TableColumn){
            .header = COLUMNS[c].head, .wType = RC_PX(COLUMNS[c].w), .align = COLUMNS[c].align,
            .priority = COLUMNS[c].priority };
        hidden += (cut >> (c + 1)) & 1u;
    }
    rcRow(.gap = 8, .align = "cl", .w = "grow", .className = "flex-wrap") {
        page_head("Deployments", rcFormat(mem, "Builds of %s", SLUGS[st->project]).chars);
        if (rcButton("redeploy", "Redeploy", .disabled = !sel, .className = mouse_h("h-9"),
                     .leadingIcon = rcIconRotateCw)) start_deploy(app, st, sel, true);
        deploy_button(app, st);
    }
    rcRow(.gap = 12, .align = "cl", .w = "grow", .className = st->compact ? "flex-col" : "") {
        rcBox(.w = "grow") {
            rcTextInput("dep_search", st->deployQuery, sizeof st->deployQuery,
                        .className = mouse_h("h-9"),
                        .placeholder = "Filter by message, branch or author",
                        .leadingIcon = rcIconSearch, .clearButton = true);
        }
        segment(mem, "filter", STATUSES, 4, &st->statusFilter);
        if (hidden)   /* the columns the width dropped, over the list's right edge */
            rcText(rcFormat(mem, "+%d column%s", hidden, hidden > 1 ? "s" : ""),
                   .color = s.textMuted, .size = 12);
    }
    /* The panel takes the page's room, at least 320 px: the header stays put and the rows,
       each a Tab stop that Enter or a click picks, scroll under it. */
    rcBox(.p = 4, .borderRadius = "all-xl", .border = st->pal.rim, .w = "grow", .h = "grow",
          .gradient = st->pal.glass, .shadow = st->pal.lift, .className = "min-h-80") {
        if (rcBeginTable("deploys", cols, 1 + COL_COUNT, .headerClassName = "text-[13px]",
                         .cellPadding = RC_VAL(8))) {
            for (int i = 0; i < st->deployCount; i++) {
                if (!listed(st, &st->deploys[i])) continue;
                if (deploy_row(app, st, i, cut, sel == &st->deploys[i], true)) st->selected = i;
                matches++;
            }
            if (matches == 0) {
                rcTableRow();
                rcTextL("No deployments match", .color = s.textMuted, .size = 14);
            }
            rcEndTable();
        }
    }
    rcScrollbar("deploys");
}

/* What a setting is and what it means. Its control sits to the right, every control in one
   right-hand column, so they line up; a phone stacks the two. */
static void setting_label(const char *title, const char *detail) {
    RC_Style s = rcGetStyle();
    rcColumn(.gap = 2, .w = "grow") {
        rcTextC(title, .color = s.text, .size = 14);
        rcTextC(detail, .color = s.textMuted, .size = 13);
    }
}

static void page_settings(RC_App *app, AppState *st) {
    RC_Arena     *mem   = rcAppArena(app);
    const char   *stack = st->compact ? "flex-col items-stretch" : "";
    const RC_Size ctl   = st->compact ? RC_GROW : RC_PX(280);

    rcColumn(.align = "tc", .w = "grow") {
        rcColumn(.gap = 16, .w = "grow", .wMax = RC_VAL(820)) {
            page_head("Settings", rcFormat(mem, "Project settings for %s",
                                           SLUGS[st->project]).chars);
            rcColumn(.gap = 20, .border = st->pal.rim, .w = "grow",
                     .gradient = st->pal.glass, .shadow = st->pal.lift, .className = "panel") {
                rcTextL("General", .font = rcFontSemibold(16.0f));
                rcRow(.gap = 16, .align = "cl", .w = "grow", .className = stack) {
                    setting_label("Production region", "Where production builds run and serve.");
                    rcBox(.wType = ctl) {
                        rcCombo("set_region", &st->region[st->project], REGIONS, 4,
                                .className = mouse_h("h-9"));
                    }
                }
                rcRow(.gap = 16, .align = "cl", .w = "grow", .className = stack) {
                    setting_label("Failure alerts", "Email the owners when production fails.");
                    rcToggle("set_alerts", "", &st->alerts);
                }
                rcRow(.gap = 16, .align = "cl", .w = "grow", .className = stack) {
                    setting_label("Theme", "System follows your desktop or phone.");
                    segment(mem, "theme", THEMES, THEME_COUNT, &st->theme);
                }
            }
            rcColumn(.gap = 20, .border = st->pal.rim, .w = "grow",
                     .gradient = st->pal.glass, .shadow = st->pal.lift, .className = "panel") {
                rcTextL("Billing for all projects", .font = rcFontSemibold(16.0f));
                rcRow(.gap = 16, .align = "cl", .w = "grow", .className = stack) {
                    setting_label("Spend limit", rcFormat(mem, "%s of %s spent this month.",
                                  money(mem, SPENT), money(mem, st->spendLimit)).chars);
                    /* The floor is this month's spend, rounded up to a step. */
                    rcBox(.wType = ctl)
                        rcSlider("set_spend", &st->spendLimit, 650.0f, 2000.0f, .step = 50.0f);
                }
                rcProgress("set_usage", SPENT / st->spendLimit);
            }
        }
    }
}

typedef enum { CMD_PAGE, CMD_PROJECT, CMD_DEPLOY, CMD_THEME } CommandKind;
typedef struct {
    const char     *label, *keys;
    RC_IconCallback icon;   /* NULL: the project's swatch */
    CommandKind     kind;
    int             arg;
} Command;

static const Command COMMANDS[] = {
    { "Go to Overview", "Cmd/Ctrl 1", rcIconHouse, CMD_PAGE, PAGE_OVERVIEW },
    { "Go to Deployments", "Cmd/Ctrl 2", rcIconGalleryVerticalEnd, CMD_PAGE, PAGE_DEPLOYS },
    { "Go to Settings", "Cmd/Ctrl 3", rcIconSettings, CMD_PAGE, PAGE_SETTINGS },
    { "Deploy the latest main", NULL, rcIconUpload, CMD_DEPLOY, 0 },
    { "Switch to halcyon-web", NULL, NULL, CMD_PROJECT, 0 },
    { "Switch to halcyon-api", NULL, NULL, CMD_PROJECT, 1 },
    { "Switch to halcyon-docs", NULL, NULL, CMD_PROJECT, 2 },
    { "Toggle light and dark", NULL, rcIconMoon, CMD_THEME, 0 }, };
enum { COMMAND_COUNT = (int)(sizeof COMMANDS / sizeof COMMANDS[0]) };

/* A command takes you somewhere new, so unlike an Esc it gives no focus back to what held
   it before the palette opened. Run before rcEndModal, the palette closes in this frame,
   and an rcSetFocus(NULL) in that frame wins over the modal's hand-back. */
static void run_command(RC_App *app, AppState *st, const Command *c) {
    st->cmdOpen = false;
    rcSetFocus(NULL);
    switch (c->kind) {
    case CMD_PAGE:    st->page = c->arg; break;
    case CMD_PROJECT: set_project(st, c->arg); break;
    case CMD_THEME:   st->theme = st->dark ? THEME_LIGHT : THEME_DARK; break;
    case CMD_DEPLOY:  start_deploy(app, st, &HEADS[st->project], false); break;
    }
}

/* The palette: type to filter, the arrows move, Enter or a click runs, Esc closes. Only the
   selection is filled, so Enter always runs the row that looks chosen. The list fits its
   matches and scrolls past what the window can hold. */
static void command_palette(RC_App *app, AppState *st) {
    RC_Style    s   = rcGetStyle();
    RC_Arena   *mem = rcAppArena(app);
    /* The room: the window less the modal's margins and padding, and for the list what is left
       below a panel whose top sits a fifth of the way down. Room that runs out, on a short or
       zoomed-in window, caps at 0: the library refuses a negative size. The list's own cap,
       352, holds all eight commands at a finger's 44 px row. */
    const float roomW = rcViewport().width - 72.0f, roomH = 0.8f * rcViewport().height - 120.0f;
    const float width = roomW < 0.0f ? 0.0f : (roomW < 560.0f ? roomW : 560.0f);
    const float listH = roomH < 0.0f ? 0.0f : (roomH < 352.0f ? roomH : 352.0f);
    int   hits[COMMAND_COUNT], count = 0;
    const Command *chosen = NULL;

    /* Top-placed, as Linear's and VS Code's: the input stays put while the list filters. */
    if (!rcBeginModal("palette", &st->cmdOpen, .placement = RC_MODAL_TOP)) {
        st->cmdSel = -1;
        return;
    }
    if (st->cmdSel < 0) {   /* opened this frame */
        st->cmdSel = 0;
        st->cmdQuery[0] = '\0';
        rcSetFocus("cmd_input");
    }
    for (int i = 0; i < COMMAND_COUNT; i++)
        if (contains(COMMANDS[i].label, st->cmdQuery)) hits[count++] = i;
    const bool moved = rcKeyPressed(RC_KEY_DOWN) || rcKeyPressed(RC_KEY_UP);
    if (rcKeyPressed(RC_KEY_DOWN) && count) st->cmdSel = (st->cmdSel + 1) % count;
    if (rcKeyPressed(RC_KEY_UP) && count)   st->cmdSel = (st->cmdSel + count - 1) % count;
    if (st->cmdSel >= count) st->cmdSel = 0;
    if (rcKeyPressed(RC_KEY_ENTER) && count) chosen = &COMMANDS[hits[st->cmdSel]];

    rcColumn(.gap = 8, .wType = RC_PX(width)) {
        rcBox(.w = "grow") {
            /* Emptied, by its x too, the query takes the keyboard back for the next one. */
            if (rcTextInput("cmd_input", st->cmdQuery, sizeof st->cmdQuery,
                            .className = mouse_h("h-10"), .placeholder = "Type a command or search",
                            .leadingIcon = rcIconSearch, .clearButton = true) && !st->cmdQuery[0])
                rcSetFocus("cmd_input");
        }
        rcColumn(.id = "cmd_list", .scroll = "v", .w = "grow", .hMax = RC_VAL(listH)) {
            for (int i = 0; i < count; i++) {
                const Command *c = &COMMANDS[hits[i]];
                const char *id = rcFormat(mem, "cmd_%d", hits[i]).chars;
                const bool on = st->cmdSel == i;
                rcRow(.id = id, .gap = 10, .px = 10, .align = "cl", .borderRadius = "all-md",
                      .w = "grow", .h = row_h(), .className = on ? st->pal.picked : NULL) {
                    if (c->icon) c->icon(16.0f, on ? s.text : s.textMuted);
                    else swatch(c->arg);
                    rcBox(.w = "grow") {
                        rcTextC(c->label, .color = s.text, .size = 14, .select = RC_SELECT_NONE);
                    }
                    if (c->keys) rcBox(.bg = s.background, .px = 6, .py = 1,
                                       .borderRadius = "all-sm", .border = { s.border, 1 }) {
                        rcTextC(c->keys, .color = s.textMuted, .size = 12,
                                .select = RC_SELECT_NONE);
                    }
                }
                if (moved && on) rcScrollIntoView("cmd_list", id, RC_SCROLL_NEAREST);
                if (rcClicked(id)) chosen = c;
            }
            if (count == 0) rcBox(.px = 10, .py = 8) {
                rcTextL("No commands match", .color = s.textMuted, .size = 14);
            }
        }
        rcScrollbar("cmd_list");
    }
    if (chosen) run_command(app, st, chosen);
    rcEndModal();
}

static void layout(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;

    /* System re-reads the OS every frame; RC_APP_EVENT_COLOR_SCHEME_CHANGED wakes one. */
    st->dark = st->theme == THEME_SYSTEM ? rcPreferredColorScheme() != RC_COLOR_SCHEME_LIGHT
                                         : st->theme == THEME_DARK;
    rcSetStyle(app_style(st->dark));
    st->pal = app_palette(st->dark);
    st->frame = rcAppArena(app);
    /* The chart ends at the clock's latest half hour, read every frame, so a window left open
       moves its axis on; the traces are sample traffic and keep their shape. */
    st->endsAt = (double)(long long)(rcUnixTimeSeconds() / 1800.0) * 1800.0;
    RC_Style s = rcGetStyle();
    /* A runtime theme switch has to move the window's clear color too. */
    rcWindowSetClearColor(rcAppMainWindow(app), s.background);

    /* PRIMARY is Cmd on Apple and Ctrl with Alt up elsewhere, so AltGr text never fires one.
       The pages wait while the palette is open: it owns the keyboard. */
    if (rcModDown(RC_MOD_PRIMARY)) {
        if (rcKeyPressed(RC_KEY_K)) st->cmdOpen = !st->cmdOpen;
        for (int i = 0; i < PAGE_COUNT; i++)
            if (!st->cmdOpen && rcKeyPressed((RC_Key)(RC_KEY_1 + i))) st->page = i;
    }
    tick_builds(app, st);
    animate(app, st);

    /* A window under 480 px tall is compact at any width, so a big phone on its side keeps
       its tab bar. A phone draws under its status bar and home indicator. On the wide arm the
       root spends the insets; on the compact arm each band spends its own, so surfaces reach
       the edge, and the page, edge to edge, draws no rim. */
    RC_Viewport vp   = rcViewport();
    st->compact      = vp.width < COMPACT_W || vp.height < 480.0f;
    RC_Insets   root = st->compact ? RC_LIT(RC_Insets){ 0 } : vp.safe;
    RC_Insets   band = st->compact ? vp.safe : RC_LIT(RC_Insets){ 0 };
    const float pad  = st->compact ? 16.0f : 24.0f;

    rcColumn(.id = "Root", .bg = s.background, .pt = (float)root.top, .pb = (float)root.bottom,
             .pl = (float)root.left, .pr = (float)root.right, .w = "grow", .h = "grow") {
        topbar(st, band);
        rcRow(.id = "Body", .pb = (float)(st->compact ? 0 : 8),
              .pr = (float)(st->compact ? 0 : 8), .w = "grow", .h = "grow") {
            if (!st->compact) sidebar(app, st);
            rcColumn(.id = "Content", .gap = 16, .pt = (float)pad,
                     .pb = (float)(pad + band.bottom + (st->compact ? 56 : 0)),
                     .pl = (float)(pad + band.left), .pr = (float)(pad + band.right),
                     .border = st->compact ? RC_LIT(RC_Border){ .width = 0 } : st->pal.rim,
                     .w = "grow", .h = "grow", .gradient = st->pal.page,
                     .className = st->compact ? "overflow-y-auto" : "overflow-y-auto rounded-xl") {
                /* else if: a click may change st->page mid-page; the new page waits a frame. */
                if (st->page == PAGE_OVERVIEW) page_overview(app, st);
                else if (st->page == PAGE_DEPLOYS) page_deployments(app, st);
                else page_settings(app, st);
            }
            rcScrollbar("Content");
        }
    }
    if (st->compact) tab_bar(app, st, band);
    command_palette(app, st);
}

int main(void) {
    static const float baseSize[] = { 14.0f };   /* what fields, buttons and headers draw at */
    static AppState state = { .alerts = true, .theme = THEME_SYSTEM, .statusFilter = ST_ANY,
                              .selected = -1, .cmdSel = -1, .spendLimit = 1000.0f };

    state.deployCount = (int)(sizeof SEED_DEPLOYS / sizeof SEED_DEPLOYS[0]);
    for (int i = 0; i < state.deployCount; i++) state.deploys[i] = SEED_DEPLOYS[i];
    seed_traces();
    /* The numbers and the traces (the first project's Requests) open settled, so what a
       capture reads does not depend on the frame count, and the chart's axis, which fits its
       data, never sweeps up from zero on the first frames. */
    for (int m = 0; m < METRIC_COUNT; m++) state.totals[m] = TOTALS[0][m][RANGE_24H][0];
    for (int then = 0; then < 2; then++)
        for (int i = 0; i < POINTS; i++) state.shown[then][i] = TRACES[0][0][RANGE_24H][then][i];

    /* Every panel is the same shell, one named class. Each site adds the live theme's glass,
       rim and lift from its Palette, which no RC_Style role names. */
    rcDefineClass("panel", "p-4 rounded-xl");
    /* A row you can pick tints under the pointer and eases there. A picked row names its fill
       after this class (Palette.picked), and a fill with no state beats the hover's. Only the
       fill eases: a `color` transition reaches every run and icon inside the box, so a theme
       switch would ease the row's text from the old theme's ink, near-invisible on the new page. */
    rcDefineClass("pickable", "hover:bg-foreground/3 transition-[background-color] duration-150");

    RC_AppOptions opts = {
        .width = 1280, .height = 820, .title = "RayClay Cloud", .fontSizes = baseSize,
        .fontCount = 1, .frame = RC_WINDOW_FRAME_NONE, .layoutCallback = layout,
        .userData = &state, .titlebar = { .height = 52 }, };
    return rcRunApp(&opts);
}
