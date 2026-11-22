/*
    services.h - the Services page, and the app's lesson in what a frame costs.

    THE STATIC ISLAND. An immediate-mode frame declares the whole UI again, so what a
    frame costs turns less on what is on screen than on what changed since the last
    one. This page puts both extremes on one screen. The live band at the top moves on
    every frame it is shown: its chart slides and its counter rolls. The 48 cards under
    it never change at all, and they are written so that costs as little as it can:
    - Their data is written once, by fleet_init, and update() never touches it.
    - Every string on them was formatted there, into fixed storage, so declaring 48
      cards formats nothing and takes nothing from the frame arena.
    - Their text is the same bytes on every frame, and RayClay measures text by its
      content and keeps the answer, so after the first frame each run is a lookup.
    - Nothing on them asks for a frame. The band does, LIVE_FPS times a second and only
      while it is on screen (update() is where): fold it, or turn refresh off, and the
      window parks with every card still drawn.
*/
#ifndef APP_SERVICES_H
#define APP_SERVICES_H

#include "app/panels.h"

/* The Services page's variables. A tier has no name, so it is spelled here; the Region
   and Group lists are fleet.h's names with "All" in front, built in app_init. */
static const char *const TIER_LABEL[FLEET_TIERS + 1] = { "All", "T1", "T2", "T3" };

/* The health pills, Grafana's radio button group: one state at a time, worst first as
   the cards are. Pill p > 0 shows the state SVC_HEALTH_COUNT - p. */
static const struct { const char *id, *label; } HEALTH_PILL[SVC_HEALTH_COUNT + 1] = {
    { "hp_all", "All" }, { "hp_down", "Down" }, { "hp_degraded", "Degraded" }, { "hp_ok", "OK" },
};

static inline const char *health_word(SvcHealth h) {
    return HEALTH_PILL[SVC_HEALTH_COUNT - (int)h].label;
}

/* Does a service pass the filters? The health pill is asked separately (@p withHealth
   false), because the pills count under the other three. */
static inline bool service_shown(const AppState *st, const Service *sv, bool withHealth) {
    if (st->svcRegion > 0 && sv->region != st->svcRegion - 1) return false;
    if (st->svcTier > 0 && sv->tier != st->svcTier)           return false;
    if (st->svcGroup > 0 && sv->group != st->svcGroup - 1)    return false;
    return !withHealth || st->svcHealth == 0
        || (int)sv->health == SVC_HEALTH_COUNT - st->svcHealth;
}

static inline void clear_service_filters(AppState *st) {
    st->svcRegion = st->svcTier = st->svcGroup = st->svcHealth = 0;
}

/* The oldest sample the time range shows: at or just past its left edge, so the line
   runs into the edge rather than starting short of it. The plot clips its own geometry. */
static inline int fleet_first(const AppState *st) {
    int first = HISTORY - 1 - RANGE_SEC[st->range];

    return first > 0 ? first : 0;
}

/* The firing alert, Grafana's alert list, one rule over the whole fleet. Its clock is the
   band's once-a-second part, and the panel is a control: a click shows the services that
   are down, which is what someone reading an alert wants next. */
static inline void alert_panel(RC_App *app, AppState *st, RC_Arena *mem) {
    RC_Style       s      = rcGetStyle();
    const Fleet   *f      = &st->fleet;
    const Service *sv     = &f->svc[f->incident];
    bool           firing = f->count[SVC_DOWN] > 0;

    if (firing && rcClicked("p_alert")) {
        clear_service_filters(st);
        st->svcHealth = SVC_HEALTH_COUNT - SVC_DOWN;
        st->rowFolded[ROW_FLEET] = false;
        st->jumpToCards = true;   /* layout() scrolls once the cards are laid out */
        rcWindowRequestFrame(rcAppMainWindow(app));
    }
    /* A Tab stop that lights under the pointer only while it fires, as it is a control
       only then. */
    dash_panel(.id = "p_alert", .focusable = firing, .w = "grow", .h = "grow",
               .tooltip = firing ? "Show the services that are down" : NULL,
               .className = firing ? "bg-surface hover:bg-surface-alt" : "bg-surface") {
        rcRow(.gap = 6, .px = 8, .align = "cl", .w = "grow", .hType = RC_PX(PANEL_HEAD)) {
            panel_title("Alerts", "i_alert",
                        "One alert rule over the whole fleet, whatever the variables pick: a "
                        "service's error rate over its budget");
        }
        rcColumn(.gap = 6, .pb = 8, .px = 8, .align = "cl", .w = "grow", .h = "grow") {
            if (firing) {
                RC_Color alarm = health_color(sv->health);
                unsigned secs  = f->alertSecs;

                rcRow(.gap = 6, .align = "cl") {
                    rcIconCircleAlert(16.0f, alarm);
                    rcTextL("Elevated error rate", .font = rcFontSemibold(14), .color = s.text);
                }
                rcText(rcFormat(mem, "%s \xc2\xb7 %s \xc2\xb7 %s", sv->name, sv->owner,
                                FLEET_REGION[sv->region]),
                       .color = s.textMuted, .size = 13);
                rcRow(.gap = 8, .align = "cl") {
                    /* Dark on the red, as the tab's count is. */
                    rcBox(.bg = alarm, .px = 8, .py = 1, .borderRadius = "all-full") {
                        rcTextL("Firing", .color = s.background, .size = 12);
                    }
                    /* An alert can fire for days: past an hour its clock gains the hours. */
                    rcText(secs >= 3600u ? rcFormat(mem, "for %u:%02u:%02u", secs / 3600u,
                                                    secs / 60u % 60u, secs % 60u)
                                         : rcFormat(mem, "for %02u:%02u", secs / 60u, secs % 60u),
                           .color = s.text, .size = 13);
                }
            } else {
                rcRow(.gap = 6, .align = "cl") {
                    rcIconCircleCheck(16.0f, health_color(SVC_OK));
                    rcTextL("No alerts firing", .font = rcFontSemibold(14), .color = s.text);
                }
            }
        }
    }
}

/* The live band's stat panels, drawn by the Resources page's own stat_panel. */
enum { LIVE_SERVED = 0, LIVE_P99, LIVE_STATS };
static const StatDef LIVE_STAT_DEF[LIVE_STATS] = {
    { "c_served", "c_served_spark", "i_served", "Requests",         9 },
    { "c_p99",    "c_p99_spark",    "i_p99",    "p99 latency",     10 },
};

static inline void live_stats(AppState *st, RC_Arena *mem, double now, StatValue *d) {
    const Fleet *f     = &st->fleet;
    int          first = fleet_first(st);
    Palette      pal   = app_palette();
    /* Requests served as of now: the count at the newest sample, carried on at the
       newest rate. fleet_sample counts that same rate when it closes the second, so
       the next sample takes over exactly where this leaves off. */
    double       n     = f->served + (double)f->rate[HISTORY - 1] * (now - st->fleetAt);
    float        p99   = f->p99[HISTORY - 1];

    /* The rolling number. Two decimals of a million turn over many times a second at
       this rate, and the font's digits share one width, so it rolls in place. */
    d[LIVE_SERVED].value = n >= 1e9 ? rcFormat(mem, "%.3f", n / 1e9).chars
                                    : rcFormat(mem, "%.2f", n / 1e6).chars;
    d[LIVE_SERVED].unit  = n >= 1e9 ? "B" : "M";
    d[LIVE_SERVED].color = pal.blue;
    d[LIVE_SERVED].tip   = rcFormat(mem, "%.0f requests a second now across the whole fleet, "
                                    "the rate the trend draws",
                                    (double)f->rate[HISTORY - 1]).chars;
    d[LIVE_SERVED].spark = f->rate + first;
    d[LIVE_SERVED].count = HISTORY - first;
    d[LIVE_SERVED].max   = series_peak(f->rate + first, HISTORY - first) * 1.1f;
    d[LIVE_SERVED].kind  = RC_SERIES_AREA;

    d[LIVE_P99].value = rcFormat(mem, "%.0f", (double)p99).chars;
    d[LIVE_P99].unit  = "ms";
    d[LIVE_P99].color = latency_color(p99);
    d[LIVE_P99].tip   = rcFormat(mem, "The slowest 1%% of the whole fleet's requests; the alert "
                                 "fires above %.0f ms", (double)LAT_WARN).chars;
    d[LIVE_P99].spark = f->p99 + first;
    d[LIVE_P99].count = HISTORY - first;
    d[LIVE_P99].max   = LAT_TOP;
    d[LIVE_P99].kind  = RC_SERIES_AREA;
}

/* "Live", breathing once a second on the clock, so there is nothing to store and nothing
   that moves while the band is paused. */
static inline void live_badge(AppState *st, double now) {
    RC_Style s     = rcGetStyle();
    Palette  pal   = app_palette();
    bool     live  = REFRESH_SEC[st->refresh] > 0;
    float    phase = (float)(now - (double)(long long)now);
    float    tri   = phase < 0.5f ? phase * 2.0f : 2.0f - phase * 2.0f;

    rcRow(.gap = 6, .pr = 4, .align = "cl") {
        status_dot(live ? rcWithAlpha(pal.blue, 0.353f + 0.647f * tri) : s.textMuted);
        if (live)
            rcTextL("Live", .color = s.textMuted, .size = 12);
        else
            rcTextL("Paused: refresh is off", .color = pal.orange, .size = 12);
    }
}

/* The latency time series with its alert rule drawn the way Grafana draws a threshold:
   a red line across the plot, and every second over it marked. The x axis runs to now
   rather than to the newest sample, which slides the plot a little on every frame
   between samples, as a strip chart's paper moves. Refresh Off stops the paper. */
static inline void latency_panel(AppState *st, RC_Arena *mem, double now, const char *w,
                                 const char *h) {
    static float x[HISTORY], limitX[2], limitY[2], hotX[HISTORY], hotY[HISTORY];
    const Fleet *f      = &st->fleet;
    float        xNow   = (float)((st->clockBase + st->bandAt) / 60.0);
    float        lag    = REFRESH_SEC[st->refresh] > 0 ? (float)(now - st->fleetAt) : 0.0f;
    RC_Axis      xAxis  = time_axis(st, xNow - (float)RANGE_SEC[st->range] / 60.0f, xNow);
    int          first  = fleet_first(st);
    int          hot    = 0, i;
    LegendEntry  e[1];
    Palette      pal    = app_palette();

    for (i = first; i < HISTORY; i++) {
        x[i] = xNow - (lag + (float)(HISTORY - 1 - i)) / 60.0f;
        if (f->p99[i] > LAT_WARN) {
            hotX[hot] = x[i];
            hotY[hot] = f->p99[i];
            hot++;
        }
    }
    limitX[0] = xAxis.min.value;
    limitX[1] = xNow;
    limitY[0] = limitY[1] = LAT_WARN;
    entry_set(&e[0], "lg_lat", "p99", pal.blue, f->p99, x, first, HISTORY - first, RC_SERIES_AREA);

    {
        /* Neither mark carries a label, so neither has a row in the hover readout. */
        RC_Series marks[2] = {
            { .y = limitY, .x = limitX, .count = 2, .kind = RC_SERIES_LINE,
              .color = health_color(SVC_DOWN), .thickness = 1.0f },
            { .y = hotY, .x = hotX, .count = hot, .kind = RC_SERIES_SCATTER,
              .color = health_color(SVC_DOWN), .thickness = 2.0f },
        };

        dash_panel(.id = "p_latency", .w = w, .h = h) {
            rcRow(.gap = 6, .px = 8, .align = "cl", .w = "grow", .hType = RC_PX(PANEL_HEAD)) {
                panel_title("p99 latency", "i_latency",
                            "The whole fleet's slowest 1% of requests, one sample a second; "
                            "red marks a second over the alert line");
                rcSpacer() {}
                live_badge(st, now);
            }
            RC_Axis yAxis = value_axis(mem, 0.0f, LAT_TOP, " ms");

            yAxis.readoutFormat = whole_readout;
            plot(st, mem, CHART_LAT, e, 1, UNIT_MS, xAxis, yAxis, marks, hot > 0 ? 2 : 1);
        }
    }
}

/* The slowest services the Region, Tier and Group variables leave: the bar gauge again,
   one bar per service, in its health color. fleet_init fixed their order and formatted
   their numbers, so like the cards this draws without formatting anything. */
static inline void slowest_panel(AppState *st, const char *w, const char *h) {
    RC_Style     s    = rcGetStyle();
    const Fleet *f    = &st->fleet;
    const Service *board[FLEET_SLOWEST];
    float        top;
    int          n = 0, k;

    for (k = 0; k < FLEET_SERVICES && n < FLEET_SLOWEST; k++)
        if (service_shown(st, &f->svc[f->slowest[k]], false))
            board[n++] = &f->svc[f->slowest[k]];
    /* The slowest bar stops a tenth short of the end, so it does not read as a limit. */
    top = n > 0 ? board[0]->p99Ms * 1.1f : 1.0f;

    dash_panel(.id = "p_slowest", .w = w, .h = h) {
        rcRow(.gap = 6, .px = 8, .align = "cl", .w = "grow", .hType = RC_PX(PANEL_HEAD)) {
            panel_title("Slowest services", "i_slowest",
                        "p99 at each service's last health check, slowest first, under the "
                        "Region, Tier and Group variables");
        }
        rcColumn(.gap = 4, .pb = 8, .px = 8, .w = "grow", .h = h) {
            if (n == 0) {
                rcBox(.p = 16, .align = "cc", .w = "grow", .h = h) {
                    rcTextL("No services match", .color = s.textMuted, .size = 14);
                }
            }
            for (k = 0; k < n; k++) {
                const Service *sv = board[k];
                RC_Color       c  = health_color(sv->health);

                /* As the core bars: a share of the panel's height, from 28 px to 56. */
                rcRow(.gap = 8, .align = "cl", .w = "grow", .h = "grow", .hMin = RC_VAL(28.0f),
                      .hMax = RC_VAL(56)) {
                    rcBox(.w = "112px") {
                        rcTextC(sv->name, .color = s.textMuted, .size = 12);
                    }
                    rcBox(.bg = s.surfaceAlt, .w = "grow", .h = "grow") {
                        rcBox(.h = "grow", .wType = RC_PCT(sv->p99Ms / top * 100.0f),
                              .gradient = { .from = rcWithAlpha(c, 0.353f), .to = c,
                                            .dir = "h" }) {}
                    }
                    rcBox(.align = "cr", .w = "60px") {
                        rcTextC(sv->p99, .color = c, .size = 16);
                    }
                }
            }
        }
    }
}

static inline void budget_gauge(AppState *st, RC_Arena *mem, float budget, float size) {
    gauge_panel("g_budget", "i_budget", "Error budget",
                "How much of the whole fleet's error budget this month's outages have spent",
                &st->gauge[GAUGE_FLEET], budget, load_color(budget),
                rcFormat(mem, "%.0f%%", (double)budget).chars, size);
}

/* The band's panels in two arrangements: rows across, or one column. */
static inline void live_band(RC_App *app, AppState *st, RC_Arena *mem) {
    double    now     = rcAppTime(app);
    float     cw      = content_width();
    bool      stacked = cw < SYS_MID_W;
    uint16_t  big     = (uint16_t)(cw >= 900.0f ? 32 : 24);
    /* Two points of budget for a service that is down, half a point for a degraded one,
       out of a quarter of the fleet: a figure fixed by the inventory, like the cards. */
    float     budget  = ((float)st->fleet.count[SVC_DOWN] * 2.0f
                         + (float)st->fleet.count[SVC_DEGRADED] * 0.5f)
                        / ((float)FLEET_SERVICES * 0.25f) * 100.0f;
    float     gaugeSize = ((float)STAT_H - (float)PANEL_HEAD - 8.0f) / 0.78f;
    StatValue d[LIVE_STATS] = { { 0 } };

    if (budget > 100.0f) budget = 100.0f;
    live_stats(st, mem, now, d);
    if (stacked) {
        rcBox(.w = "grow", .hType = RC_PX(STAT_H)) { alert_panel(app, st, mem); }
        rcRow(.gap = GUTTER, .w = "grow", .hType = RC_PX(STAT_H)) {
            stat_panel(st, &LIVE_STAT_DEF[LIVE_SERVED], &d[LIVE_SERVED], big);
            stat_panel(st, &LIVE_STAT_DEF[LIVE_P99], &d[LIVE_P99], big);
            budget_gauge(st, mem, budget, gaugeSize);
        }
        rcBox(.w = "grow", .hType = RC_PX(LIVE_ROW_H)) {
            latency_panel(st, mem, now, "grow", "grow");
        }
        slowest_panel(st, "grow", "fit");
    } else {
        rcRow(.gap = GUTTER, .w = "grow", .hType = RC_PX(STAT_H)) {
            alert_panel(app, st, mem);
            stat_panel(st, &LIVE_STAT_DEF[LIVE_SERVED], &d[LIVE_SERVED], big);
            stat_panel(st, &LIVE_STAT_DEF[LIVE_P99], &d[LIVE_P99], big);
            budget_gauge(st, mem, budget, gaugeSize);
        }
        rcRow(.gap = GUTTER, .w = "grow", .hType = RC_PX(LIVE_ROW_H)) {
            latency_panel(st, mem, now, "grow", "grow");
            slowest_panel(st, cw >= SYS_WIDE_W ? "400px" : "300px", "grow");
        }
    }
}

/* The health pills, each with its count under the page's other filters, so a pill never
   promises cards the variables have already ruled out. */
static inline void health_pills(AppState *st, RC_Arena *mem) {
    RC_Style s   = rcGetStyle();
    float    pad = rcPointerIsCoarse() ? 14.0f : 5.0f;
    int      count[SVC_HEALTH_COUNT + 1] = { 0 };
    int      i, p;

    for (i = 0; i < FLEET_SERVICES; i++) {
        const Service *sv = &st->fleet.svc[i];

        if (!service_shown(st, sv, false))
            continue;
        count[0]++;
        count[SVC_HEALTH_COUNT - (int)sv->health]++;
    }
    rcRow(.align = "cl", .borderRadius = "all-xs",
          .border = { .color = s.border, .width = 1 }) {
        for (p = 0; p <= SVC_HEALTH_COUNT; p++) {
            const char *id = HEALTH_PILL[p].id;
            bool        on;

            if (rcClicked(id))
                st->svcHealth = p;
            on = st->svcHealth == p;
            rcRow(.id = id, .focusable = true, .bg = on ? s.surfaceAlt : RC_UNSET_COLOR,
                  .gap = 6, .px = 10, .py = pad, .align = "cc") {
                if (p > 0)
                    status_dot(health_color((SvcHealth)(SVC_HEALTH_COUNT - p)));
                rcTextC(HEALTH_PILL[p].label,
                        .color = on || rcShowsHover(id) ? s.text : s.textMuted, .size = 13);
                rcText(rcFormat(mem, "%d", count[p]), .color = s.textMuted, .size = 13);
            }
        }
    }
}

/* One card: a dash_panel with the health down its left edge, a surface rather than a
   glyph because among 48 cards the eye scans edges. */
static inline void service_card(const Service *sv) {
    RC_Style s = rcGetStyle();
    RC_Color c = health_color(sv->health);

    dash_panel(.w = "grow") {
        rcRow(.w = "grow") {
            rcBox(.bg = c, .borderRadius = "l-xs", .w = "3px", .h = "grow") {}
            rcColumn(.gap = 6, .px = 12, .py = 8, .w = "grow") {
                rcRow(.gap = 8, .align = "cl", .w = "grow") {
                    rcTextC(sv->name, .font = rcFontSemibold(14), .color = s.text);
                    rcSpacer() {}
                    rcRow(.gap = 6, .align = "cl") {
                        status_dot(c);
                        rcTextC(health_word(sv->health), .color = c, .size = 12);
                    }
                }
                rcRow(.gap = 6, .align = "cl", .w = "grow") {
                    rcTextC(sv->owner, .color = s.textMuted, .size = 12);
                    rcSpacer() {}
                    tag(FLEET_REGION[sv->region]);
                    tag(TIER_LABEL[sv->tier]);
                }
                rcRow(.gap = 4, .align = "cl", .w = "grow") {
                    rcTextC(sv->rps, .color = s.text, .size = 13);
                    rcTextL("req/s", .color = s.textMuted, .size = 12);
                    rcSpacer() {}
                    rcTextC(sv->p99, .color = s.text, .size = 13);
                    rcTextL("p99", .color = s.textMuted, .size = 12);
                }
            }
        }
    }
}

/* The cards that pass the filters, worst first, as many to a row as clear CARD_MIN_W. A
   short last row is padded with empty slots, so its cards keep the width of the rows
   above. A filter that leaves nothing says so: an empty grid reads as a broken page. */
static inline void fleet_cards(AppState *st, RC_Arena *mem) {
    RC_Style     s = rcGetStyle();
    const Fleet *f = &st->fleet;
    float        inner = content_width() - 2.0f * (float)PAGE_PAD - 10.0f;
    int          cols  = (int)((inner + (float)GUTTER) / (float)(CARD_MIN_W + GUTTER));
    int          shown[FLEET_SERVICES];
    int          n = 0, i, k;
    bool         narrowed = st->svcRegion || st->svcTier || st->svcGroup || st->svcHealth;

    if (cols < 1) cols = 1;
    for (i = 0; i < FLEET_SERVICES; i++)
        if (service_shown(st, &f->svc[f->order[i]], true))
            shown[n++] = f->order[i];

    rcRow(.id = "svc_toolbar", .gap = 8, .align = "cl", .w = "grow", .className = "flex-wrap") {
        health_pills(st, mem);
        rcSpacer() {}
        rcText(rcFormat(mem, "%d of %d services", n, FLEET_SERVICES), .color = s.textMuted,
               .size = 12);
        if (narrowed && n > 0) {
            if (rcButton("svc_clear", "Clear filters", .className = control_h("h-7"),
                         .variant = RC_BTN_GHOST))
                clear_service_filters(st);
        }
    }
    if (n == 0) {
        dash_panel(.w = "grow") {
            rcColumn(.gap = 8, .p = 24, .align = "cc", .w = "grow") {
                rcTextL("No services match", .font = rcFontSemibold(14), .color = s.text);
                rcTextL("Widen Region, Tier or Group, or pick All.", .color = s.textMuted,
                        .size = 13);
                if (rcButton("svc_clear", "Clear filters", .className = control_h("h-7.5"),
                             .variant = RC_BTN_PRIMARY))
                    clear_service_filters(st);
            }
        }
        return;
    }
    for (i = 0; i < n; i += cols) {
        rcRow(.gap = GUTTER, .w = "grow") {
            for (k = i; k < i + cols; k++) {
                if (k < n)
                    service_card(&f->svc[shown[k]]);
                else
                    rcBox(.w = "grow") {}
            }
        }
    }
}

#endif /* APP_SERVICES_H */
