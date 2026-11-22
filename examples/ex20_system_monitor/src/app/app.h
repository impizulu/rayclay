/*
    app.h - the dashboard: its sampling clock, its chrome and its two pages.

    The look is Grafana's, because a monitoring dashboard is what this is and Grafana is
    the one most people have seen: a breadcrumb and star in the titlebar, the time-range
    and refresh pickers on a toolbar, a row of template variables, collapsible rows, and
    the panels in panels.h. Every control changes the data on screen; none of them is a
    picture of a control. The titlebar folds away under Ctrl or Cmd + B, leaving a rail
    lit by the load of each core and the strip the window still drags by.

    One translation unit: main.c includes this, and every function is `static inline`,
    so a header never forces its includer to call everything in it.
*/
#ifndef APP_APP_H
#define APP_APP_H

#include "rayclay.h"
#include "app/state.h"
#include "app/panels.h"
#include "app/windows.h"
#include "app/services.h"
#include "app/art.h"
#include "platform/platform.h"

#include "icons/rc_icons_check.h"
#include "icons/rc_icons_clock.h"
#include "icons/rc_icons_rayclay_logo.h"
#include "icons/rc_icons_rotate_cw.h"
#include "icons/rc_icons_star.h"
#include "icons/rc_icons_star_filled.h"
#include "icons/rc_icons_x.h"

/* Fraction of the remaining distance the titlebar's fold covers per frame, and the
   distance at which it snaps. An on-demand window stays awake for as long as an ease
   takes, so these two numbers are what the animation costs. */
#define EASE_K    0.35f
#define EASE_SNAP 1.0f

/* The chrome's own widths, beside the pages' in theme.h. */
#define SYS_BAR_ROW_W 1120.0f  /* the tabs, the variables and the pickers share a row */
#define SYS_TABS_ROW_W 600.0f  /* below this the tabs take a row of their own        */
/* The full breadcrumb, in unzoomed px. Past a phone on its side (852), so a phone never
   has it in one orientation only; the palette holds where its two links go. */
#define SYS_CRUMBS_W   880.0f
#define SYS_SEARCH_W   960.0f  /* the search field in the titlebar, in unzoomed px   */

/* The pages the tabs switch, and the box each scrolls in. */
static const struct { const char *tabId, *label, *scrollId; } PAGE_DEF[PAGE_COUNT] = {
    { "tab_resources", "Resources", "page"     },
    { "tab_services",  "Services",  "svc_page" },
};

/* The collapsible rows: their ids, titles and how many panels a folded one hides. */
static const struct { const char *id, *title; int panels; } ROW_DEF[ROW_COUNT] = {
    { "row_device",   "This device",               6 },
    { "row_overview", "Overview",                  10 },
    { "row_cpu",      "CPU",                       3 },
    { "row_memnet",   "Memory, network and disk",  3 },
    { "row_procs",    "Workload",                  1 },
    { "row_live",     "Live traffic",              5 },
    { "row_fleet",    "Service health",            FLEET_SERVICES },
};

/* ---- SAMPLING ---------------------------------------------------------------- */

/* One sample of every server, at @p minutes on the shared axis, into @p slot, taken
   @p seconds after the one before. */
static inline void servers_sample(AppState *st, int slot, float minutes, double seconds) {
    int h, i;

    for (h = 0; h < HOST_COUNT; h++) {
        HostView      *v = &st->view[h];
        const SysHost *m = &v->host;
        float sum = 0.0f;

        sysmon_host_sample(&v->host, seconds);
        for (i = 0; i < SYS_CORES; i++) {
            hist_put(v->core[i], slot, m->core[i]);
            sum += m->core[i];
        }
        hist_put(v->cpu, slot, sum / (float)SYS_CORES);
        hist_put(v->userShare, slot, m->userShare);
        hist_put(v->sysShare, slot, m->sysShare);
        hist_put(v->memUsed, slot, m->memUsedGiB);
        hist_put(v->memCache, slot, m->memCacheGiB);
        hist_put(v->swap, slot, m->swapUsedGiB);
        hist_put(v->rx, slot, m->netRxKB);
        hist_put(v->tx, slot, m->netTxKB);
        hist_put(v->rd, slot, m->diskRdKB);
        hist_put(v->wr, slot, m->diskWrKB);
        hist_put(v->load1, slot, m->load1);
    }
    hist_put(st->at, slot, minutes);
    st->samples++;
}

/* The axis counts minutes from a UTC midnight, in the floats a chart takes. Counted from launch
   they would climb for as long as the app runs, and a float that large loses first the steps the
   live band slides by, within days, then the second itself. So once the oldest sample held is
   past a midnight the origin moves a day on: every time held shifts back 1440 minutes, a
   multiple of every tick step, and the labels wrap the day, so the axes read as they did. */
static inline void roll_day(AppState *st) {
    int i;

    while (st->at[0] >= 1440.0f) {
        for (i = 0; i < HISTORY; i++)
            st->at[i] -= 1440.0f;
        st->clockBase -= 86400.0;
        st->dayUnix   += 86400.0;
        st->zoomFrom  -= 1440.0f;
        st->zoomTo    -= 1440.0f;
        st->brushFrom -= 1440.0f;
    }
}

/* The axis labels are civil time, counted on from the launch by rcAppTime. On most hosts
   app time stops while the system sleeps, so each sample checks the count against the wall
   clock and moves it on by any time slept. Only forward: the slots stay in time order, and
   a clock set back is left alone. The Local zone's offset is read again too, so a change to
   or from daylight saving time reaches the labels. */
static inline void follow_wall_clock(AppState *st, double now) {
    double       wall  = rcUnixTimeSeconds();
    double       ahead = wall - (st->dayUnix + st->clockBase + now);
    RC_LocalTime local;

    if (ahead > 2.0)   /* well past the drift between the two clocks */
        st->clockBase += ahead;
    if (wall > 0.0 && rcLocalTime(wall, &local))
        st->localMinutes = local.utcOffsetSeconds / 60;
}

/* Seed every host and the dashboard's defaults. Called once, before rcRunApp. */
static inline void app_init(AppState *st) {
    int h, i;

    gauge_init();
    for (h = 0; h < HOST_COUNT; h++) {
        HostView *v = &st->view[h];

        sysmon_host_init(&v->host, &HOST_PROFILE[h], 0x53595300u + (uint32_t)h * 0x9E3779B9u);
        for (i = 0; i < SYS_PROCS; i++)
            v->order[i] = i;
    }
    st->sortKey    = SORT_CPU;
    st->sortDesc   = true;
    st->range      = 1;   /* Last 5 minutes */
    st->refresh    = 1;   /* 1s             */
    st->barOpen    = true;
    st->barH       = (float)BAR_OPEN_H;
    st->tab        = PAGE_RESOURCES;
    st->crossChart = -1;
    st->brushChart = -1;

    fleet_init(&st->fleet, 0x464C456Fu);
    st->regionItems[0] = st->groupItems[0] = "All";
    for (i = 0; i < FLEET_REGIONS; i++)
        st->regionItems[i + 1] = FLEET_REGION[i];
    for (i = 0; i < FLEET_GROUPS; i++)
        st->groupItems[i + 1] = FLEET_GROUP[i];
    for (i = 0; i < PANEL_COUNT; i++) {
        st->slot[i].st    = st;
        st->slot[i].panel = (Panel)i;
    }
}

/* Move *cur a fixed fraction of the way to target; snap when the gap closes. True while
   still moving, the condition for asking for another frame. */
static inline bool ease_to(float *cur, float target, float k, float snap) {
    float gap = target - *cur;

    if (gap < 0.0f) gap = -gap;
    if (gap <= snap) {
        *cur = target;
        return false;
    }
    *cur += (target - *cur) * k;
    return true;
}

/* The crosshair spans the Resources page's charts: which one the pointer is on, and the
   moment under it. A mouse drag on a chart becomes a zoom of every panel when it lets go;
   a click is not a drag, so it takes a few pixels. A finger on a chart is scrolling the
   page, so it starts no drag. Read before layout, against the boxes the user is looking
   at. */
static inline void track_pointer(AppState *st) {
    float   from, to;
    int     c;

    st->crossChart = -1;
    if (st->tab != PAGE_RESOURCES) {
        st->brushChart = -1;
        return;
    }
    bool    over = st->brushChart >= 0;   /* off every chart, read no position */
    for (c = 0; c < CHART_LAT && !over; c++)
        over = rcIsHovered(CHART_DEF[c].chart);
    RC_Vec2 p = over ? rcPointer() : RC_LIT(RC_Vec2){ 0 };
    view_window(st, &from, &to);
    if (st->brushChart >= 0) {
        RC_Box r = rcChartPlotRect(CHART_DEF[st->brushChart].chart);
        float  x = r.width > 0.0f ? (p.x - r.x) / r.width : 0.0f;

        x = x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;
        st->brushTo = from + x * (to - from);
        if (rcPointerReleased(RC_POINTER_LEFT)) {
            float lo = st->brushFrom < st->brushTo ? st->brushFrom : st->brushTo;
            float hi = st->brushFrom < st->brushTo ? st->brushTo : st->brushFrom;

            if (r.width > 0.0f && (hi - lo) / (to - from) * r.width >= 6.0f) {
                st->zoomed   = true;
                st->zoomFrom = lo;
                st->zoomTo   = hi - lo < ZOOM_MIN_SPAN ? lo + ZOOM_MIN_SPAN : hi;
            }
            st->brushChart = -1;
        }
        return;
    }
    for (c = 0; c < CHART_LAT; c++) {
        RC_Box r = rcChartPlotRect(CHART_DEF[c].chart);

        if (!r.found || r.width <= 0.0f || p.x < r.x || p.x > r.x + r.width || p.y < r.y
            || p.y > r.y + r.height || !rcIsHovered(CHART_DEF[c].chart))
            continue;
        st->crossChart = c;
        st->crossAt    = from + (p.x - r.x) / r.width * (to - from);
        if (rcPointerPressed(RC_POINTER_LEFT) && !rcPointerIsCoarse()) {
            st->brushChart = c;
            st->brushFrom  = st->brushTo = st->crossAt;
        }
        return;
    }
}

/* Pacing an on-demand app. A RayClay window parks when nothing changes, so the app
   decides when to wake: rcWindowRequestFrameAfter is "wake me in N seconds", re-armed
   every frame, and rcAppTime is the monotonic clock the sample gate reads. Do not
   accumulate rcWindowFrameTime instead: it answers how long the last frame was, never
   what time it is. Gate on the clock, not on input: a monitor that stops monitoring
   while you use the mouse is not a monitor. */
static inline void update(RC_App *app, void *userData) {
    AppState  *st      = (AppState *)userData;
    RC_Window *win     = rcAppMainWindow(app);
    double     now     = rcAppTime(app);
    int        period  = REFRESH_SEC[st->refresh];
    bool       sampled = false;
    int        p;

    st->app = app;   /* the detach verb runs inside layout, which has no app */

    /* RC_MOD_PRIMARY is Cmd on a macOS or iOS build, Cmd or Ctrl on the web and Ctrl
       everywhere else. B is the chord editors fold a sidebar with; K is Grafana's own
       search. T, N and W are the browser's, and a web page cannot claim them. Z zooms
       back out; in a focused field it is also the field's undo, which rcKeyPressed
       still reports, so the zoom is left alone while either field has focus. */
    if (rcModDown(RC_MOD_PRIMARY) && rcKeyPressed(RC_KEY_B))
        st->barOpen = !st->barOpen;
    if (rcModDown(RC_MOD_PRIMARY) && rcKeyPressed(RC_KEY_K))
        st->paletteOpen = !st->paletteOpen;
    if (rcModDown(RC_MOD_PRIMARY) && rcKeyPressed(RC_KEY_Z)
        && !rcIsFocused("proc_q") && !rcIsFocused("pal_q"))
        st->zoomed = false;

    if (!st->primed) {
        /* First frame: fill every server's history as though the app had been sampling
           once a second, writing each slot in place, then take the device's first
           reading: memory at once, and the baseline each CPU interval starts from. The
           wall clock is civil time for the axis labels only, never used for pacing: the
           OS may step it. */
        double wall = rcUnixTimeSeconds();
        float  nowM;
        int    i;

        st->dayUnix   = (double)((long long)(wall / 86400.0)) * 86400.0;
        st->clockBase = wall - st->dayUnix - now;
        follow_wall_clock(st, now);   /* reads the Local zone's offset */
        nowM = (float)((st->clockBase + now) / 60.0);
        for (i = 0; i < HISTORY; i++)
            servers_sample(st, i, nowM - (float)(HISTORY - i) / 60.0f, 1.0);
        servers_sample(st, -1, nowM, 1.0);
        device_sample(&st->device, 1.0);
        st->fleetAt = now;   /* fleet_init primed the band as of this moment */
        st->primed  = true;
        sampled     = true;
    } else if (st->refreshNow || (period > 0 && now - st->lastSampleAt >= (double)period)) {
        follow_wall_clock(st, now);
        servers_sample(st, -1, (float)((st->clockBase + now) / 60.0), now - st->lastSampleAt);
        device_sample(&st->device, now - st->lastSampleAt);
        sampled = true;
    }

    /* A floating panel parks like this window, so the sample is what admits its next
       frame: a minimized one never, a background one every other sample, a focused or
       pinned one every sample. */
    if (sampled) {
        roll_day(st);
        st->lastSampleAt = now;
        st->refreshNow   = false;
        resort(st);
        for (p = 0; p < PANEL_COUNT; p++) {
            RC_Window *w = st->slot[p].window;

            if (!w || rcWindowIsMinimized(w))
                continue;
            if (rcWindowIsFocused(w) || st->slot[p].pinned) {
                rcWindowRequestFrame(w);
                continue;
            }
            st->slot[p].skips = (unsigned char)!st->slot[p].skips;
            if (!st->slot[p].skips)
                rcWindowRequestFrame(w);
        }
    }

    /* The live band's clock: one fleet sample per whole second of app time, caught up
       after the window parks. "Off" holds the band where it is, and the Refresh button
       moves it one sample, as it does every other panel. */
    if (period == 0) {
        if (sampled) {
            fleet_sample(&st->fleet);
            st->bandAt = now;
        }
        st->fleetAt = now;
    } else {
        int steps;

        st->bandAt = now;

        for (steps = 0; steps < HISTORY && now - st->fleetAt >= 1.0; steps++) {
            fleet_sample(&st->fleet);
            st->fleetAt += 1.0;
        }
        if (now - st->fleetAt >= 1.0)   /* parked for longer than the history */
            st->fleetAt = now;
    }

    track_pointer(st);
    retitle_floating_panels(st, app);

    st->animating = ease_to(&st->barH, st->barOpen ? (float)BAR_OPEN_H : (float)BAR_RAIL_H,
                            EASE_K, EASE_SNAP);

    /* The band you draw and the strip the OS drags by are two rectangles, and keeping
       them equal is the app's job: left alone, a folded bar keeps swallowing clicks
       across the height the open one covered. */
    rcWindowSetTitlebarHeight(win, (int)st->barH);

    /* Wake for the next sample, measured from the last one rather than from this frame,
       so a burst of input frames cannot keep pushing the sample back. Refresh "Off" arms
       nothing. While the live band is on screen it moves on every frame, so the wake
       comes LIVE_FPS times a second instead. Leave the page or fold the band and the
       cards alone are left: they never move, so they never ask. */
    if (st->animating) {
        rcWindowRequestFrame(win);
    } else if (period > 0) {
        double due  = st->lastSampleAt + (double)period - now;
        bool   live = st->tab == PAGE_SERVICES && !st->rowFolded[ROW_LIVE];

        if (live && due > 1.0 / (double)LIVE_FPS)
            due = 1.0 / (double)LIVE_FPS;
        rcWindowRequestFrameAfter(win, due > 0.01 ? due : 0.01);
    }
}

/* ---- THE TITLEBAR ------------------------------------------------------------ */

/* What a folded titlebar leaves: one cell per logical core, lit by its own load, across
   the full width so it cannot be read as a progress bar. */
static inline void core_rail(AppState *st, float height) {
    RC_Style     s    = rcGetStyle();
    const float *core = st->view[st->host].host.core;
    int i;

    rcRow(.id = "tb_rail", .bg = s.background, .gap = 1, .w = "grow", .hType = RC_PX(height)) {
        for (i = 0; i < SYS_CORES; i++) {
            float   v = core[i];
            /* Alpha carries the load, the hue carries the band. */
            uint8_t a = (uint8_t)(70.0f + (v > 100.0f ? 100.0f : v) * 1.85f);

            rcBox(.bg = rcWithAlpha(load_color(v), a / 255.0f), .w = "grow", .h = "grow") {}
        }
    }
}

/* One breadcrumb that goes somewhere. A box read with rcClicked answers only the pointer;
   .focusable makes it a Tab stop too, which Enter and Space click. */
static inline bool crumb(const char *id, const char *label) {
    RC_Style s   = rcGetStyle();
    Palette  pal = app_palette();

    rcBox(.id = id, .focusable = true, .py = (float)(rcPointerIsCoarse() ? 12 : 0),
          .noDrag = true) {
        rcTextC(label, .color = rcShowsHover(id) ? pal.link : s.text, .size = 14);
    }
    return rcClicked(id);
}

/* Home goes to the first page's top; Dashboards opens the palette, where the pages and
   everything else are listed. The dashboard's own name is where you are, so it is text. */
static inline void breadcrumb(AppState *st, bool full) {
    RC_Style s   = rcGetStyle();
    Palette  pal = app_palette();

    if (full) {
        if (crumb("crumb_home", "Home")) {
            st->tab = PAGE_RESOURCES;
            st->pageY[PAGE_RESOURCES] = 0.0f;
            rcScrollToTop(PAGE_DEF[PAGE_RESOURCES].scrollId);
        }
        rcIconChevronRight(14.0f, s.textMuted);
        if (crumb("crumb_dash", "Dashboards"))
            st->paletteOpen = true;
        rcIconChevronRight(14.0f, s.textMuted);
    }
    rcBox(.className = "truncate min-w-0") {   /* gives way in a narrow band, as on the web */
        rcTextL("System Monitor", .color = s.textMuted, .size = 14);
    }
    rcRow(.noDrag = true) {
        if (icon_button("tb_star", st->starred ? rcIconStarFilled : rcIconStar,
                        st->starred ? pal.orange : s.textMuted,
                        st->starred ? "Remove from favorites" : "Mark as favorite", true))
            st->starred = !st->starred;
    }
}

/* Grafana's search, which opens the palette. The whole pill is the button, and lights
   under the pointer. */
static inline void search_pill(AppState *st) {
    RC_Style s = rcGetStyle();

    rcRow(.id = "tb_search", .focusable = true, .gap = 8, .px = 8, .align = "cl",
          .borderRadius = "all-xs", .border = { .color = s.border, .width = 1 },
          .noDrag = true, .w = "340px", .hType = RC_PX(rcPointerIsCoarse() ? 44 : 28),
          .className = "bg-background hover:bg-surface-alt") {
        rcIconSearch(14.0f, s.textMuted);
        rcTextL("Search or jump to...", .color = s.textMuted, .size = 13);
        rcSpacer() {}
        rcTextL(PRIMARY_KEY "+K", .color = s.textMuted, .size = 12);
    }
    if (rcClicked("tb_search"))
        st->paletteOpen = true;
}

/* `RC_WINDOW_FRAME_NONE` means the runner draws nothing and this band is the whole chrome.
   RC_ID_WINDOW_DRAG makes a region move the window, .noDrag carves a clickable hole back
   out of it, and rcWindowControls draws the bundled buttons, each tagging itself. All
   three are inert on the web, where the browser tab is the chrome: one source, one
   target drawing less. */
static inline void titlebar(RC_App *app, AppState *st) {
    RC_Style s = rcGetStyle();
    /* Chrome, not content: the band keeps its size at every zoom, so its branches are
       measured in unzoomed px, the layout width times the zoom. */
    float wide = content_width() * rcWindowZoom(rcAppMainWindow(app));
    /* Open and settled, a hairline is enough; the rail is what a fold leaves. */
    float railH = st->barOpen && st->barH >= (float)BAR_OPEN_H ? 0.0f : (float)BAR_RAIL_H;
    float bandH = st->barH - railH;
    /* The band's fill reaches past a phone's side cutout and its content stays clear of
       it; unzoomed, the band pads in the getter's window px. */
    RC_Insets safe = rcGetSafeAreaInsets();

    if (bandH < 0.0f) bandH = 0.0f;
    rcUnzoomed() {
        rcColumn(.id = RC_ID_WINDOW_DRAG, .bg = s.chrome, .pl = safe.left, .pr = safe.right,
                 .border = { .color = s.border }, .w = "grow", .hType = RC_PX(st->barH),
                 .className = "border-b") {
            /* The band collapses to nothing while the rail stays: a folded titlebar with
               no drag region leaves a window that cannot be moved. */
            if (bandH >= 6.0f) {
                rcRow(.align = "tl", .w = "grow", .hType = RC_PX(bandH)) {
                    rcRow(.gap = 8, .pl = 12, .align = "cl", .w = "grow", .h = "grow",
                          .className = "min-w-0") {
                        rcIconRayClayLogo(20.0f);
                        breadcrumb(st, wide >= SYS_CRUMBS_W);
                        if (wide < SYS_SEARCH_W) {
                            rcRow(.noDrag = true) {
                                if (icon_button("tb_search_btn", rcIconSearch, s.textMuted,
                                                "Search or jump to (" PRIMARY_KEY "+K)", true))
                                    st->paletteOpen = true;
                            }
                        }
                    }
                    if (wide >= SYS_SEARCH_W) {
                        rcRow(.align = "cc", .h = "grow") {
                            search_pill(st);
                        }
                    }
                    /* Growing too only to center the search; narrower, the title gives way
                       and the close slab stays on screen. */
                    rcRow(.align = "tr", .w = wide >= SYS_SEARCH_W ? "grow" : "fit", .h = "grow") {
                        /* As tall as the band as it folds, and flush to the corner, so
                           the OS rounds the close slab with the window. Unsized, a slab
                           is the whole title strip, the rail too, so the band's height is
                           written: a length known only at run time is a formatted class. */
                        rcWindowControls(.className = rcFormat(rcAppArena(app), "h-[%dpx]",
                                                               (int)bandH).chars);
                    }
                }
            }
            if (railH > 0.0f)
                core_rail(st, railH);
        }
    }
}

/* ---- THE TOOLBAR ------------------------------------------------------------- */

/* One template variable: Grafana's label chip joined to its dropdown. */
static inline bool var_picker(const char *label, const char *id, int *sel,
                              const char *const *items, int count, const char *w) {
    RC_Style s = rcGetStyle();
    Palette pal = app_palette();
    bool changed = false;

    rcRow(.align = "cl") {
        /* No right edge: the combo's own border is the one hairline between them. */
        rcBox(.bg = s.surfaceAlt, .px = 8, .align = "cc", .borderRadius = "l-xs",
              .border = { .color = s.border }, .h = "grow", .className = "border-l border-y") {
            rcTextC(label, .color = pal.link, .size = 13);
        }
        rcBox(.w = w) {
            changed = rcCombo(id, sel, items, count, .className = control_h("h-7.5"));
        }
    }
    return changed;
}

/* Each page has its own variables, as each Grafana dashboard does. */
static inline void variables_row(AppState *st, bool inBar) {
    /* flex-wrap needs a bounded width, so it only wraps opening the page. */
    rcRow(.id = "vars", .gap = GUTTER, .align = "cl", .w = inBar ? "fit" : "grow",
          .className = "flex-wrap") {
        if (st->tab == PAGE_SERVICES) {
            var_picker("Region", "var_region", &st->svcRegion, st->regionItems,
                       FLEET_REGIONS + 1, "112px");
            var_picker("Tier", "var_tier", &st->svcTier, TIER_LABEL, FLEET_TIERS + 1, "72px");
            var_picker("Group", "var_group", &st->svcGroup, st->groupItems, FLEET_GROUPS + 1,
                       "112px");
        } else {
            if (var_picker("Host", "var_host", &st->host, HOST_LABEL, HOST_COUNT, "112px")) {
                st->page = 0;
                resort(st);
            }
            var_picker("Cores", "var_cores", &st->cores, CORE_LABEL, SYS_CORES + 1, "80px");
            if (var_picker("State", "var_state", &st->stateFilter, STATE_LABEL, STATE_FILTERS,
                           "104px"))
                st->page = 0;
        }
    }
}

/* The page tabs, Grafana's: the label over the gradient bar that marks the page you are
   on. The Services tab carries how many services are down, so an outage shows from the
   Resources page too. */
static inline void page_tabs(AppState *st, RC_Arena *mem, bool fill) {
    RC_Style s    = rcGetStyle();
    Palette  pal  = app_palette();
    float    tabH = rcPointerIsCoarse() ? 44.0f : 40.0f;
    int      down = st->fleet.count[SVC_DOWN];
    int      p;

    /* Before any tab is drawn, so this frame marks exactly one of them. */
    for (p = 0; p < PAGE_COUNT; p++)
        if (rcClicked(PAGE_DEF[p].tabId))
            st->tab = (Page)p;
    rcRow(.gap = 4, .align = "cl", .w = fill ? "grow" : "fit") {
        for (p = 0; p < PAGE_COUNT; p++) {
            const char *id = PAGE_DEF[p].tabId;
            bool        on = st->tab == (Page)p;

            rcColumn(.id = id, .focusable = true, .px = 8, .w = fill ? "grow" : "fit",
                     .hType = RC_PX(tabH)) {
                rcRow(.gap = 6, .align = "cc", .w = "grow", .h = "grow") {
                    rcTextC(PAGE_DEF[p].label, .font = rcFontSemibold(14),
                            .color = on || rcShowsHover(id) ? s.text : s.textMuted);
                    if (p == PAGE_SERVICES && down > 0) {
                        /* Dark on the red: 5.4:1. */
                        rcBox(.bg = health_color(SVC_DOWN), .px = 6, .borderRadius = "all-full") {
                            rcText(rcFormat(mem, "%d", down), .color = s.background, .size = 12);
                        }
                    }
                }
                if (on)
                    rcBox(.w = "grow", .h = "2px",
                          .gradient = { .from = pal.tabFrom, .to = pal.tabTo, .dir = "h" }) {}
                else
                    rcBox(.w = "grow", .h = "2px") {}
            }
        }
    }
}

/* A caption over a group of the time menu's items. A menu's popup is a column, so a box
   that is no item sits among its items as it would in any column. */
static inline void menu_caption(const char *text) {
    rcBox(.pt = 6, .pb = 2, .px = 10) {
        rcTextC(text, .font = rcFontSemibold(12), .color = rcGetStyle().textMuted);
    }
}

/* The time picker, Grafana's quick ranges over its time zones, as one menu. rcBeginMenu
   draws the button, keeps the popup inside the safe area, walks its items with Tab and
   closes on a choice, Esc or a press outside, so the app keeps only what a choice
   changes. A check marks the range and the zone in force. The label holds one line: in a
   phone's row, where the button fills what the other controls leave, it gives way first
   and ends in an ellipsis. */
static inline void time_menu(AppState *st, RC_Arena *mem, bool finger, bool tiny, bool fill) {
    const char *label = RANGE_LABEL[st->range];
    const char *look  = finger ? (fill ? "h-11 flex-1 min-w-0 truncate" : "h-11 truncate")
                               : (fill ? "h-7.5 flex-1 min-w-0 truncate" : "h-7.5 truncate");
    int         i;

    if (st->zoomed) {
        int from = clock_seconds(st->zoomFrom, zone_minutes(st));
        int to   = clock_seconds(st->zoomTo, zone_minutes(st));

        label = rcFormat(mem, "%02d:%02d:%02d to %02d:%02d:%02d", from / 3600, from / 60 % 60,
                         from % 60, to / 3600, to / 60 % 60, to % 60).chars;
    }
    /* As Grafana's picker, a zone is named only when it is not the user's own. */
    if (st->zone != ZONE_LOCAL)
        label = rcFormat(mem, "%s %s", label, ZONE_LABEL[st->zone]).chars;
    if (rcBeginMenu("nav_time", label, .className = look, .caret = true,
                    .leadingIcon = tiny ? NULL : rcIconClock)) {
        menu_caption("Quick ranges");
        for (i = 0; i < RANGE_COUNT; i++) {
            if (rcMenuItem(RANGE_LABEL[i],
                           .leadingIcon = !st->zoomed && st->range == i ? rcIconCheck : NULL)) {
                st->range  = i;
                st->zoomed = false;
            }
        }
        menu_caption("Time zone");
        for (i = 0; i < ZONE_COUNT; i++)
            if (rcMenuItem(ZONE_LABEL[i], .leadingIcon = st->zone == i ? rcIconCheck : NULL))
                st->zone = i;
        rcEndMenu();
    }
}

/* How much history the panels show, how often they refresh, and the titlebar's fold,
   where Grafana keeps its own collapse control. */
static inline void time_controls(AppState *st, RC_Arena *mem, bool fill) {
    RC_Style   s      = rcGetStyle();
    RC_Window *win    = st->app ? rcAppMainWindow(st->app) : NULL;
    /* The menu stands 30 px tall under a mouse, 44 under a finger, as the combo does. */
    bool       finger = rcPointerIsCoarse();
    /* On the narrowest phones the clock and the divider give way to the controls. */
    bool       tiny   = content_width() < 340.0f;

    rcRow(.gap = 4, .align = "cl", .w = fill ? "grow" : "fit") {
        time_menu(st, mem, finger, tiny, fill);
        if (st->zoomed && icon_button("nav_zoom_out", rcIconX, s.text,
                                      "Zoom back out (" PRIMARY_KEY "+Z)", true))
            st->zoomed = false;
        /* A sample taken on request, whatever the interval says: update() reads the flag,
           so ask for the frame that runs it. */
        if (icon_button("nav_refresh", rcIconRotateCw, s.text, "Refresh dashboard", true)) {
            st->refreshNow = true;
            if (win) rcWindowRequestFrame(win);
        }
        rcBox(.w = "68px") {
            /* This frame's update() already armed the old interval, so a new one needs a
               frame of its own to take effect. */
            if (rcCombo("nav_every", &st->refresh, REFRESH_LABEL, REFRESH_COUNT,
                        .className = control_h("h-7.5")) && win)
                rcWindowRequestFrame(win);
        }
        if (!tiny)
            rcBox(.bg = s.border, .w = "1px", .h = "20px") {}
        if (icon_button("tb_fold", st->barOpen ? rcIconChevronUp : rcIconChevronDown, s.textMuted,
                        st->barOpen ? "Fold the titlebar (" PRIMARY_KEY "+B)"
                                    : "Show the titlebar (" PRIMARY_KEY "+B)", true))
            st->barOpen = !st->barOpen;
    }
}

/* Three arrangements, widest first: everything on one row; the tabs beside the pickers,
   the variables opening the page; and on a phone the tabs on a row of their own. */
static inline void top_bar(AppState *st, RC_Arena *mem, bool varsInBar) {
    RC_Style  s    = rcGetStyle();
    RC_Insets safe = rcViewport().safe;

    /* As the titlebar: the fill to the screen's sides, the controls inside the cutout. */
    rcColumn(.id = "nav", .bg = s.chrome, .pl = (float)PAGE_PAD + safe.left,
             .pr = (float)PAGE_PAD + safe.right,
             .border = { .color = s.border }, .w = "grow", .className = "border-b") {
        if (varsInBar) {
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                page_tabs(st, mem, false);
                rcBox(.bg = s.border, .w = "1px", .h = "20px") {}
                variables_row(st, true);
                rcSpacer() {}
                time_controls(st, mem, false);
            }
        } else if (content_width() >= SYS_TABS_ROW_W) {
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                page_tabs(st, mem, false);
                rcSpacer() {}
                time_controls(st, mem, false);
            }
        } else {
            page_tabs(st, mem, true);
            rcBox(.py = 4, .w = "grow") {
                time_controls(st, mem, true);
            }
        }
    }
}

/* ---- THE COMMAND PALETTE ----------------------------------------------------- */

typedef enum PaletteKind {
    PAL_PAGE = 0, PAL_RANGE, PAL_REFRESH, PAL_HOST, PAL_REFRESH_NOW, PAL_FOLD, PAL_ROWS, PAL_DOWN
} PaletteKind;

static const struct { const char *label; PaletteKind kind; int arg; } PALETTE[] = {
    { "Go to Resources",            PAL_PAGE,    PAGE_RESOURCES },
    { "Go to Services",             PAL_PAGE,    PAGE_SERVICES  },
    { "Show the services that are down", PAL_DOWN, 0 },
    { "Host: web-01 (simulated)",   PAL_HOST,    HOST_WEB   },
    { "Host: db-01 (simulated)",    PAL_HOST,    HOST_DB    },
    { "Host: cache-01 (simulated)", PAL_HOST,    HOST_CACHE },
    { "Time range: Last 1 minute",  PAL_RANGE,   0 },
    { "Time range: Last 5 minutes", PAL_RANGE,   1 },
    { "Time range: Last 15 minutes", PAL_RANGE,  2 },
    { "Time range: Last 30 minutes", PAL_RANGE,  3 },
    { "Refresh now",                PAL_REFRESH_NOW, 0 },
    { "Refresh interval: Off",      PAL_REFRESH, 0 },
    { "Refresh interval: 1s",       PAL_REFRESH, 1 },
    { "Refresh interval: 5s",       PAL_REFRESH, 2 },
    { "Refresh interval: 10s",      PAL_REFRESH, 3 },
    { "Refresh interval: 30s",      PAL_REFRESH, 4 },
    { "Fold or show the titlebar",  PAL_FOLD,    0 },
    { "Collapse all rows",          PAL_ROWS,    1 },
    { "Expand all rows",            PAL_ROWS,    0 },
};
enum { PALETTE_COUNT = sizeof PALETTE / sizeof PALETTE[0] };

static inline void palette_run(RC_App *app, AppState *st, int item) {
    int arg = PALETTE[item].arg, i;

    switch (PALETTE[item].kind) {
    case PAL_PAGE:    st->tab = (Page)arg;   break;
    case PAL_RANGE:
        st->range  = arg;
        st->zoomed = false;
        break;
    case PAL_REFRESH: st->refresh = arg;     break;
    case PAL_HOST:
        st->host = arg;
        st->page = 0;
        resort(st);
        break;
    case PAL_REFRESH_NOW: st->refreshNow = true; break;
    case PAL_FOLD:    st->barOpen = !st->barOpen; break;
    case PAL_ROWS:
        for (i = 0; i < ROW_COUNT; i++)
            st->rowFolded[i] = arg != 0;
        break;
    case PAL_DOWN:
        st->tab = PAGE_SERVICES;
        clear_service_filters(st);
        st->svcHealth = SVC_HEALTH_COUNT - SVC_DOWN;
        st->rowFolded[ROW_FLEET] = false;
        st->jumpToCards = true;
        break;
    }
    st->paletteOpen = false;
    rcWindowRequestFrame(rcAppMainWindow(app));
}

/* Grafana's Ctrl+K: type to narrow, arrows to move, Enter to run. A modal, so Esc and a
   click outside close it, and the library traps focus inside. */
static inline void palette(RC_App *app, AppState *st, RC_Arena *mem) {
    static bool wasOpen = false;
    RC_Style s = rcGetStyle();
    float    w = content_width() - 64.0f;
    int      match[PALETTE_COUNT];
    int      n = 0, i;
    bool     moved;

    if (!st->paletteOpen) {
        wasOpen = false;
        return;
    }
    /* Before the modal's first frame, so the field it covers takes the focus at once. */
    if (!wasOpen) {
        rcSetFocus("pal_q");
        st->paletteQuery[0] = '\0';
        st->paletteSel = 0;
        wasOpen = true;
    }
    for (i = 0; i < PALETTE_COUNT; i++)
        if (text_has(PALETTE[i].label, st->paletteQuery))
            match[n++] = i;
    if (st->paletteSel >= n) st->paletteSel = n > 0 ? n - 1 : 0;
    moved = (rcKeyPressed(RC_KEY_DOWN) && st->paletteSel + 1 < n)
         || (rcKeyPressed(RC_KEY_UP) && st->paletteSel > 0);
    if (moved)
        st->paletteSel += rcKeyPressed(RC_KEY_DOWN) ? 1 : -1;

    if (rcBeginModal("palette", &st->paletteOpen)) {
        rcColumn(.gap = 8, .wType = RC_PX(w > 520.0f ? 520.0f : w)) {
            /* The matches were taken before this edit, so the next frame lists the new query;
               emptied, by its x or by typing, the list starts over at its first row. */
            if (rcTextInput("pal_q", st->paletteQuery, sizeof st->paletteQuery,
                            .className = rcPointerIsCoarse() ? NULL : "h-9",
                            .placeholder = "Search or jump to...", .leadingIcon = rcIconSearch,
                            .clearButton = true)) {
                if (!st->paletteQuery[0])
                    st->paletteSel = 0;
                rcWindowRequestFrame(rcAppMainWindow(app));
            }
            rcColumn(.id = "pal_list", .scroll = "v", .w = "grow", .hType = RC_PX(320)) {
                for (i = 0; i < n; i++) {
                    const char *id = rcFormat(mem, "pal_%d", match[i]).chars;

                    /* The arrows' pick stays lit; any other row lights under the pointer. */
                    rcRow(.id = id, .px = 8, .align = "cl", .w = "grow",
                          .hType = RC_PX(rcPointerIsCoarse() ? 44 : 30),
                          .className = i == st->paletteSel ? "bg-surface-alt"
                                                           : "hover:bg-surface-alt") {
                        rcTextC(PALETTE[match[i]].label, .color = s.text, .size = 14);
                    }
                    if (rcClicked(id))
                        palette_run(app, st, match[i]);
                }
                if (n == 0) {
                    rcBox(.p = 16, .align = "cc", .w = "grow") {
                        rcTextL("Nothing matches", .color = s.textMuted, .size = 14);
                    }
                }
            }
            /* Only when the keys moved it, or the wheel could never scroll away. */
            if (moved)
                rcScrollIntoView("pal_list", rcFormat(mem, "pal_%d", match[st->paletteSel]).chars,
                                 RC_SCROLL_NEAREST);
            rcScrollbar("pal_list");
        }
        rcEndModal();
    }
    if (st->paletteOpen && n > 0 && rcKeyPressed(RC_KEY_ENTER))
        palette_run(app, st, match[st->paletteSel]);
}

/* ---- THE RESOURCES PAGE ------------------------------------------------------ */

/* A Grafana row header. Returns whether its panels are shown, already reflecting a click
   this frame, so the fold never lags a frame behind. The title is the target, not the
   whole width: the empty stretch beside a row header is where people click to do nothing.
   The tag after the title says where the row's figures come from, in text a finger can
   read too: This device is real, every other row a simulated server or fleet. */
static inline bool dash_row(AppState *st, DashRow r, RC_Arena *mem) {
    RC_Style    s      = rcGetStyle();
    const char *id     = ROW_DEF[r].id;
    int         panels = ROW_DEF[r].panels;
    bool        hot;

    if (rcClicked(id))
        st->rowFolded[r] = !st->rowFolded[r];
    hot = rcShowsHover(id);
    rcRow(.align = "cl", .w = "grow", .hType = RC_PX(rcPointerIsCoarse() ? 44 : 28)) {
        rcRow(.id = id, .focusable = true, .gap = 6, .pr = 8, .align = "cl", .h = "grow") {
            if (st->rowFolded[r])
                rcIconChevronRight(16.0f, hot ? s.text : s.textMuted);
            else
                rcIconChevronDown(16.0f, hot ? s.text : s.textMuted);
            rcTextC(ROW_DEF[r].title, .font = rcFontSemibold(16), .color = s.text);
            tag(r == ROW_DEVICE ? "real" : "simulated");
            if (st->rowFolded[r])
                rcText(rcFormat(mem, "(%d %s)", panels, panels == 1 ? "panel" : "panels"),
                       .color = s.textMuted, .size = 13);
        }
    }
    return !st->rowFolded[r];
}

/* The overview's three stats. A rate has no threshold, so it takes a series color. */
enum { STAT_LOAD = 0, STAT_NET, STAT_UP, STAT_COUNT };
static const StatDef STAT_DEF[STAT_COUNT] = {
    { "c_load", "c_load_spark", "i_load", "Load average", 0 },
    { "c_net",  "c_net_spark",  "i_net",  "Network in",   1 },
    { "c_up",   "c_up_spark",   "i_up",   "Uptime",       2 },
};

static inline void overview_stats(AppState *st, RC_Arena *mem, StatValue *d) {
    const HostView *v     = &st->view[st->host];
    const SysHost  *h     = &v->host;
    Palette         pal   = app_palette();
    int             first = range_first(st);
    int             count = range_last(st) - first + 1;
    int             days  = (int)(h->uptimeSecs / 86400.0);
    int             hours = (int)(h->uptimeSecs / 3600.0) % 24;
    float           loadPct = h->load1 / (float)SYS_CORES * 100.0f;
    float           peak  = series_peak(v->rx + first, count);

    d[STAT_LOAD].value = rcFormat(mem, "%.2f", (double)h->load1).chars;
    d[STAT_LOAD].tip   = "One-minute load average, shared across 8 cores";
    d[STAT_LOAD].color = load_color(loadPct);
    d[STAT_LOAD].spark = v->load1 + first;
    d[STAT_LOAD].count = count;
    d[STAT_LOAD].max   = (float)SYS_CORES;
    d[STAT_LOAD].kind  = RC_SERIES_AREA;

    if (h->netRxKB >= 1000.0f) {
        d[STAT_NET].value = rcFormat(mem, "%.1f", (double)h->netRxKB / 1000.0).chars;
        d[STAT_NET].unit  = "MB/s";
    } else {
        d[STAT_NET].value = rcFormat(mem, "%.0f", (double)h->netRxKB).chars;
        d[STAT_NET].unit  = "KB/s";
    }
    d[STAT_NET].tip   = rcFormat(mem, "Receive rate; %s out",
                                 value_text(mem, h->netTxKB, UNIT_RATE)).chars;
    d[STAT_NET].color = pal.blue;
    d[STAT_NET].spark = v->rx + first;
    d[STAT_NET].count = count;
    /* A tenth of headroom, so the busiest sample does not clip flat against the top. */
    d[STAT_NET].max   = peak > 0.0f ? peak * 1.1f : 0.0f;
    d[STAT_NET].kind  = RC_SERIES_AREA;

    d[STAT_UP].value   = rcFormat(mem, "%dd %dh", days, hours).chars;
    d[STAT_UP].tip     = "Since this simulated server's last boot";
    /* The server is named in the figure, so even a screenshot cropped to it says whose. */
    d[STAT_UP].caption = rcFormat(mem, "%s, since boot", HOST_LABEL[st->host]).chars;
    d[STAT_UP].color   = pal.green;
}

/* The server's three gauges. part: 0 all three, 1 CPU and memory, 2 the disk alone. */
static inline void overview_gauges(AppState *st, RC_Arena *mem, float size, int part) {
    const HostView *v      = &st->view[st->host];
    const SysHost  *h      = &v->host;
    float           cpu    = v->cpu[HISTORY - 1];
    float           memPct = h->memUsedGiB / h->profile->memTotalGiB * 100.0f;

    if (part != 2) {
        gauge_panel("g_cpu", "i_g_cpu", "CPU busy", "Mean of this host's cores",
                    &st->gauge[GAUGE_CPU], cpu, load_color(cpu),
                    rcFormat(mem, "%.0f%%", (double)cpu).chars, size);
        gauge_panel("g_mem", "i_g_mem", "Memory used",
                    rcFormat(mem, "%.1f of %.0f GiB", (double)h->memUsedGiB,
                             (double)h->profile->memTotalGiB).chars,
                    &st->gauge[GAUGE_MEM], memPct, load_color(memPct),
                    rcFormat(mem, "%.0f%%", (double)memPct).chars, size);
    }
    if (part != 1)
        gauge_panel("g_disk", "i_g_disk", "Disk used",
                    "Space used on the root volume, /",
                    &st->gauge[GAUGE_DISK], h->diskPct, load_color(h->diskPct),
                    rcFormat(mem, "%.0f%%", (double)h->diskPct).chars, size);
}

/* Fills the width at any size: all seven panels across; tiles and gauges over the stats;
   or, on a phone, the tiles over rows of two panels, as Grafana stacks, so no title is cut. */
static inline void overview_row(AppState *st, RC_Arena *mem) {
    float     cw    = content_width();
    float     inner = cw - 2.0f * (float)PAGE_PAD - (rcPointerIsCoarse() ? 0.0f : 10.0f);
    bool      wide  = cw >= SYS_WIDE_W;
    bool      mid   = !wide && cw >= SYS_MID_W;
    const SysHost *h = &st->view[st->host].host;
    StatValue d[STAT_COUNT] = { { 0 } };
    float     cell, size, tall;
    uint16_t  big;
    int       i;

    overview_stats(st, mem, d);
    if (wide)
        cell = (inner - (float)TILES_W - 6.0f * (float)GUTTER) / 6.0f;
    else if (mid)
        cell = (inner - (float)TILES_W - 3.0f * (float)GUTTER) / 3.0f;
    else
        cell = (inner - (float)GUTTER) / 2.0f;
    big  = (uint16_t)(cell >= 150.0f ? 44 : 30);
    tall = wide || mid ? (float)OVERVIEW_H : (float)GAUGES_ROW_H;
    size = (tall - (float)PANEL_HEAD - 8.0f) / 0.78f;
    if (size > cell - 12.0f) size = cell - 12.0f;

    if (wide || mid) {
        rcRow(.gap = GUTTER, .w = "grow", .hType = RC_PX(OVERVIEW_H)) {
            rcBox(.h = "grow", .wType = RC_PX(TILES_W)) {
                status_tiles(h, false);
            }
            overview_gauges(st, mem, size, 0);
            if (wide)
                for (i = 0; i < STAT_COUNT; i++)
                    stat_panel(st, &STAT_DEF[i], &d[i], big);
        }
    } else {
        rcBox(.w = "grow", .hType = RC_PX(TILES_ROW_H)) {
            status_tiles(h, true);
        }
        rcRow(.gap = GUTTER, .w = "grow", .hType = RC_PX(tall)) {
            overview_gauges(st, mem, size, 1);
        }
        rcRow(.gap = GUTTER, .w = "grow", .hType = RC_PX(tall)) {
            overview_gauges(st, mem, size, 2);
            stat_panel(st, &STAT_DEF[0], &d[0], big);
        }
        rcRow(.gap = GUTTER, .w = "grow", .hType = RC_PX(STAT_H)) {
            stat_panel(st, &STAT_DEF[1], &d[1], big);
            stat_panel(st, &STAT_DEF[2], &d[2], big);
        }
    }
    if (mid) {
        rcRow(.gap = GUTTER, .w = "grow", .hType = RC_PX(STAT_H)) {
            for (i = 0; i < STAT_COUNT; i++)
                stat_panel(st, &STAT_DEF[i], &d[i], big);
        }
    }
}

/* The This device row: the four readings device.h takes, then what a frame costs, every
   figure real. Read inside a layout callback, the frame counts report the previous drawn
   frame. All six panels are always drawn: a reading the platform does not give keeps its
   panel and says so, and one still waiting for its first interval shows no number, so no
   figure on screen is one nobody measured. */
enum { DEV_CPU = 0, DEV_MEM, DEV_APP_CPU, DEV_APP_MEM, DEV_ELEMENTS, DEV_FRAMES, DEV_COUNT };
static const StatDef DEV_DEF[DEV_COUNT] = {
    { "c_dev_cpu", "c_dev_cpu_spark", "i_dev_cpu", "Device CPU",        3 },
    { "c_dev_mem", "c_dev_mem_spark", "i_dev_mem", "Device memory",     4 },
    { "c_app_cpu", "c_app_cpu_spark", "i_app_cpu", "This app's CPU",    5 },
    { "c_app_mem", "c_app_mem_spark", "i_app_mem", "This app's memory", 6 },
    { "c_decl",    "c_decl_spark",    "i_decl",    "Elements a frame",  7 },
    { "c_parks",   "c_parks_spark",   "i_parks",   "Frames drawn",      8 },
};

/* One reading's stat: its newest value with @p decimals, and its history inside the time
   range. @p caption is the live one; NULL draws none, for a caption that needs a value. */
static inline void reading_stat(const AppState *st, RC_Arena *mem, const Reading *r,
                                int decimals, const char *caption, StatValue *d) {
    int first = series_first(st, r->count);

    d->spark   = r->hist + first;
    d->count   = HISTORY - first;
    d->caption = r->absent ? "Not reported here" : caption;
    if (r->count > 0)
        d->value = rcFormat(mem, "%.*f", decimals, (double)reading_now(r)).chars;
}

static inline void device_row(RC_App *app, AppState *st, RC_Arena *mem, int perRow) {
    const Device  *dev = &st->device;
    RC_Window     *win = rcAppMainWindow(app);
    RC_FrameCounts fc  = rcWindowFrameCounts(win);
    RC_SchedStats  ss  = rcWindowSchedStats(win);
    RC_Style       s   = rcGetStyle();
    Palette        pal = app_palette();
    StatValue      d[DEV_COUNT] = { { 0 } };
    float          cpuPeak;
    int            row, i;

    reading_stat(st, mem, &dev->machineCpu, 0, "of all cores", &d[DEV_CPU]);
    d[DEV_CPU].unit  = "%";
    d[DEV_CPU].tip   = "rcMachineCpuPercent(): every core busy is 100%";
    d[DEV_CPU].color = load_color(reading_now(&dev->machineCpu));
    d[DEV_CPU].max   = 100.0f;
    d[DEV_CPU].kind  = RC_SERIES_AREA;

    reading_stat(st, mem, &dev->machineMem, 0,
                 dev->machineMem.count > 0 ? rcFormat(mem, "%.1f of %.1f GiB", dev->usedGiB,
                                                      dev->totalGiB).chars : NULL,
                 &d[DEV_MEM]);
    d[DEV_MEM].unit  = "%";
    d[DEV_MEM].tip   = "rcMachineMemoryBytes(): the total less what the OS reports as available";
    d[DEV_MEM].color = load_color(reading_now(&dev->machineMem));
    d[DEV_MEM].max   = 100.0f;
    d[DEV_MEM].kind  = RC_SERIES_AREA;

    reading_stat(st, mem, &dev->appCpu, 1, "of one core", &d[DEV_APP_CPU]);
    cpuPeak = series_peak(d[DEV_APP_CPU].spark, d[DEV_APP_CPU].count);
    d[DEV_APP_CPU].unit  = "%";
    d[DEV_APP_CPU].tip   = "rcProcessCpuPercent(): one core busy is 100%, so it can pass 100";
    d[DEV_APP_CPU].color = load_color(reading_now(&dev->appCpu));
    d[DEV_APP_CPU].max   = cpuPeak > 100.0f ? cpuPeak : 100.0f;
    d[DEV_APP_CPU].kind  = RC_SERIES_AREA;

    reading_stat(st, mem, &dev->appMem, 1,
                 dev->appMem.count > 0 ? rcFormat(mem, "peak %.1f MiB", dev->appPeakMiB).chars
                                       : NULL,
                 &d[DEV_APP_MEM]);
    d[DEV_APP_MEM].unit  = "MiB";
    d[DEV_APP_MEM].tip   = "rcProcessMemoryBytes(): resident memory; in a browser, the WASM heap";
    d[DEV_APP_MEM].color = pal.purple;
    /* Half again the peak: a steady line sits two thirds up the box, clear of the caption. */
    d[DEV_APP_MEM].max   = series_peak(d[DEV_APP_MEM].spark, d[DEV_APP_MEM].count) * 1.5f;
    d[DEV_APP_MEM].kind  = RC_SERIES_LINE;

    d[DEV_ELEMENTS].value = rcFormat(mem, "%u", (unsigned)fc.declared).chars;
    d[DEV_ELEMENTS].color = s.text;
    d[DEV_ELEMENTS].tip   = rcFormat(mem, "Declared by the layout; %u reach the screen as draw "
                                     "commands", (unsigned)fc.drawCommands).chars;
    d[DEV_FRAMES].value = rcFormat(mem, "%llu", (unsigned long long)ss.admitted).chars;
    d[DEV_FRAMES].color = s.text;
    d[DEV_FRAMES].tip   = rcFormat(mem, "The window parked %llu times: it draws only when a "
                                   "sample, an input or an animation asks",
                                   (unsigned long long)ss.waits).chars;

    for (row = 0; row < DEV_COUNT; row += perRow) {
        rcRow(.gap = GUTTER, .w = "grow", .hType = RC_PX(STAT_H)) {
            for (i = row; i < row + perRow && i < DEV_COUNT; i++)
                stat_panel(st, &DEV_DEF[i], &d[i], 30);
        }
    }
}

/* Where the detachable panels cannot leave, the page says so under its last row. True
   from the first frame where it is true: nobody has to click a control that cannot work
   to be told about it. */
static inline void docking_note(const AppState *st, RC_Arena *mem) {
    RC_Style s = rcGetStyle();

    if (!rcChildWindowsSupported())
        rcTextL("Panels stay docked: this target has one window", .color = s.textMuted,
                .size = 12);
    else if (st->singleSurface)
        rcTextL("Panels stay docked: this desktop refused a new window", .color = s.textMuted,
                .size = 12);
    else if (st->lastOpenError != RC_WINDOW_ERROR_NONE)
        rcText(rcFormat(mem, "The last detach failed: RC_WindowError %d",
                        (int)st->lastOpenError), .color = s.warning, .size = 12);
}

static inline void page_body(RC_App *app, AppState *st, RC_Arena *mem, bool varsInBar) {
    float cw   = content_width();
    bool  wide = cw >= SYS_WIDE_W;
    bool  mid  = !wide && cw >= SYS_MID_W;

    /* The device first, then the pickers that choose among the servers below it. */
    if (dash_row(st, ROW_DEVICE, mem))
        device_row(app, st, mem, wide ? 6 : mid ? 3 : 2);
    if (!varsInBar)
        variables_row(st, false);
    if (dash_row(st, ROW_OVERVIEW, mem))
        overview_row(st, mem);

    /* Each slot draws its panel, or the placeholder saying where it went. */
    if (dash_row(st, ROW_CPU, mem)) {
        if (wide || mid) {
            rcRow(.gap = GUTTER, .w = "grow", .hType = RC_PX(CPU_ROW_H)) {
                panel_draw(st, PANEL_CHART, mem, "grow", "grow");
                if (wide)
                    heat_panel(st, mem, "380px", "grow");
                panel_draw(st, PANEL_CORES, mem, wide ? "300px" : "280px", "grow");
            }
            if (mid)
                rcBox(.w = "grow", .hType = RC_PX(HEAT_ROW_H)) {
                    heat_panel(st, mem, "grow", "grow");
                }
        } else {
            /* Stacked, not withheld: a narrow window shows everything a wide one shows,
               scrolling allowed. */
            rcBox(.w = "grow", .hType = RC_PX(CPU_ROW_H + 40)) {
                panel_draw(st, PANEL_CHART, mem, "grow", "grow");
            }
            panel_draw(st, PANEL_CORES, mem, "grow", "fit");
            rcBox(.w = "grow", .hType = RC_PX(HEAT_ROW_H)) {
                heat_panel(st, mem, "grow", "grow");
            }
        }
    }
    if (dash_row(st, ROW_MEMNET, mem)) {
        if (wide) {
            rcRow(.gap = GUTTER, .w = "grow", .hType = RC_PX(MEM_ROW_H)) {
                memory_panel(st, mem);
                network_panel(st, mem);
                disk_panel(st, mem);
            }
        } else if (mid) {
            rcRow(.gap = GUTTER, .w = "grow", .hType = RC_PX(MEM_ROW_H)) {
                memory_panel(st, mem);
                network_panel(st, mem);
            }
            rcBox(.w = "grow", .hType = RC_PX(MEM_ROW_H - 40)) { disk_panel(st, mem); }
        } else {
            rcBox(.w = "grow", .hType = RC_PX(MEM_ROW_H)) { memory_panel(st, mem); }
            rcBox(.w = "grow", .hType = RC_PX(MEM_ROW_H)) { network_panel(st, mem); }
            rcBox(.w = "grow", .hType = RC_PX(MEM_ROW_H)) { disk_panel(st, mem); }
        }
    }
    if (dash_row(st, ROW_PROCS, mem))
        panel_draw(st, PANEL_TABLE, mem, "grow", "fit");
    docking_note(st, mem);
}

static inline void services_body(RC_App *app, AppState *st, RC_Arena *mem, bool varsInBar) {
    if (!varsInBar)
        variables_row(st, false);
    if (dash_row(st, ROW_LIVE, mem))
        live_band(app, st, mem);
    if (dash_row(st, ROW_FLEET, mem))
        fleet_cards(st, mem);
}

static inline void layout(RC_App *app, void *userData) {
    AppState *st  = (AppState *)userData;
    RC_Style  s   = rcGetStyle();
    RC_Arena *mem = rcAppArena(app);
    /* A phone draws the window edge to edge, under the status bar, the side cutout and
       the home indicator. Spend each margin once: the root takes the top; the two bands
       pad their own content by the sides, so their fill runs to the screen's edge; the
       page sits inside the sides; and the bottom pads the end of the page's scrolled
       content, which clears the indicator at its end. They are {0,0,0,0} on a desktop,
       so this is one code path everywhere. */
    RC_Insets safe = rcViewport().safe;
    /* The variables share the top bar's row where it has room, which keeps them in view
       while the page scrolls; narrower, they open the page instead. */
    bool varsInBar = content_width() >= SYS_BAR_ROW_W;

    if (!st->auroraPainted) {
        aurora_paint(&st->aurora);
        st->auroraPainted = true;
    }
    /* The aurora stays still behind the page, and every panel is glass over it. */
    rcColumn(.id = "root", .bg = s.background, .pt = safe.top, .w = "grow", .h = "grow",
             .image = &st->aurora, .imageFit = RC_IMAGE_COVER) {
        titlebar(app, st);
        top_bar(st, mem, varsInBar);
        rcColumn(.pl = safe.left, .pr = safe.right, .w = "grow", .h = "grow") {
            /* A mouse's scrollbar takes its own 12 px band; a finger's overlays the content, as
               in every phone browser. The tab picks the container, and .scrollOffset puts a
               page that comes back where its reader left it. */
            rcColumn(.id = PAGE_DEF[st->tab].scrollId, .gap = GUTTER, .p = PAGE_PAD,
                     .pb = (float)((float)PAGE_PAD + safe.bottom), .scroll = "v",
                     .w = "grow", .h = "grow", .scrollOffset = { 0.0f, st->pageY[st->tab] }) {
                if (st->tab == PAGE_SERVICES)
                    services_body(app, st, mem, varsInBar);
                else
                    page_body(app, st, mem, varsInBar);
            }
            /* The alert's jump waits for a frame after one that laid out the cards' toolbar,
               then runs once: a page coming back takes its .scrollOffset at the next frame's
               open, which would undo a move made on its first frame. */
            if (st->jumpToCards && st->tab == PAGE_SERVICES) {
                if (rcGetElementBox("svc_toolbar").height > 0.0f) {
                    rcScrollIntoView("svc_page", "svc_toolbar", RC_SCROLL_START);
                    st->jumpToCards = false;
                } else {
                    rcWindowRequestFrame(rcAppMainWindow(app));
                }
            }
            {
                RC_ScrollInfo si = rcGetScrollInfo(PAGE_DEF[st->tab].scrollId);

                if (si.found)
                    st->pageY[st->tab] = si.offsetY;
            }
            rcScrollbar(PAGE_DEF[st->tab].scrollId);
        }
    }
    palette(app, st, mem);
}

#endif /* APP_APP_H */
