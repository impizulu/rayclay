/*
    opsdash_app.c - the Fleet Operations dashboard's GUI.

    RayClay types only: no <system> include (raw-memory needs route through
    opsdash_backend.h) and no rcFormat outside the demo overlay - every dynamic
    string is a backend fixed buffer and every element id comes from a static
    table.

    Two arms, one set of controls: below OPS_STACK_W the three panes become one
    scrolling page (rail, chips, cards); above it they sit side by side. Every
    branch reads the viewport off AppCtx, never off a window.
*/
#define OPSDASH_BACKEND_IMPLEMENTATION
#include "opsdash_app.h"

/* Warmup frames, then the scene HOLDS. Retune with the coordinates below. */
#define OPSDASH_BENCH_WARMUP 80

/* Layout constants, in logical units: RayClay scales them by the display zoom, so
   the scene reflows rather than pixelates. */
#define OPS_NAV_W      190
#define OPS_RAIL_W     260
#define OPS_CARD_W     240
#define OPS_CARD_GAP    12
#define OPS_SPARK_H     56
#define OPS_SPARK_HOT   75     /* above this a sample is drawn amber */
#define OPS_AXIS_W      44
#define OPS_TOPBAR_H    52
#define OPS_INCIDENT_SVC 0     /* the service the open incident is about */

/* THE ONE BREAKPOINT, derived from this app's own panes rather than a table of
   device widths: the fixed nav and rail, plus one card inside its gutters, is the
   narrowest width at which the middle pane is a grid at all. Below it the page
   STACKS into one scrolling column - the live rail on top, because that is the
   headline of an ops dashboard, then the fleet filter, then the cards. The test is
   the SPACE, never the platform. */
#define OPS_STACK_W   722.0f

/* The rail's natural height, so a window too short for it scrolls the rail rather
   than losing its foot. */
#define OPS_RAIL_H    620.0f

/* The stacked arm's fleet chips: a health dot, the widest group label and their
   gutters, with air. Chips per row come from the row's own width. */
#define OPS_CHIP_W     96.0f
#define OPS_CHIP_GAP    8

/* The demo perf chip floats and must cover no datum, so it takes whichever of three
   places has room: the desktop's bottom-right corner, the titlebar's middle when the
   window is wide enough for it (OPS_HUD_DOCK_W), or a band the stacked page leaves
   at the foot (OPS_HUD_BAND). opsdash_demo_chrome picks; opsdash_layout reserves. */
#define OPS_HUD_BAND    44
#define OPS_HUD_DOCK_W  720.0f

/* Element ids must be stable POINTERS across frames - never a per-frame scratch
   buffer, which garbles the inspector and the duplicate-id report even though the
   hashing itself would still be correct. */
static const char *const CARD_IDS[OPS_SVC_COUNT] = {
    "svc00", "svc01", "svc02", "svc03", "svc04", "svc05", "svc06", "svc07",
    "svc08", "svc09", "svc10", "svc11", "svc12", "svc13", "svc14", "svc15",
    "svc16", "svc17", "svc18", "svc19", "svc20", "svc21", "svc22", "svc23",
    "svc24", "svc25", "svc26", "svc27", "svc28", "svc29", "svc30", "svc31",
    "svc32", "svc33", "svc34", "svc35", "svc36", "svc37", "svc38", "svc39",
    "svc40", "svc41", "svc42", "svc43", "svc44", "svc45", "svc46", "svc47",
};
static const char *const NAV_IDS[OPS_GROUP_COUNT] = {
    "nav0", "nav1", "nav2", "nav3", "nav4", "nav5", "nav6", "nav7",
};

static const char *const TIER_LABEL[4] = { "T?", "T1", "T2", "T3" };
static const char *const HEALTH_LABEL[OPS_HEALTH_COUNT] = { "OK", "WARN", "DOWN" };

/* Health reads as a colour everywhere in this app, so it resolves in one place -
   which is also what lets a consumer's custom style reach all three states. */
static RC_Color ops_health_color(uint8_t health) {
    RC_Style s = rcGetStyle();
    if (health == OPS_DOWN) return s.danger;
    if (health == OPS_WARN) return s.warningHover;
    return s.successHover;
}

/* A filled status dot. */
static void ops_dot(uint8_t health) {
    rcBox(.bg = ops_health_color(health), .borderRadius = "all-full", .w = "8px",
          .h = "8px") {}
}

/* One tile of the fleet summary: the number large, its label small beneath. A count
   of zero is drawn muted rather than in its health colour, so a red "0 DOWN" can
   never read as an alarm. `.wrap = "n"` keeps a squeezed tile's label on one line. */
static void ops_stat(const char *value, const char *label, RC_Color tint, bool live) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surface, .gap = 2, .p = 12, .borderRadius = "all-md",
             .border = { .color = s.border, .width = "all-1" }, .w = "grow") {
        rcTextC(value, .font = F_MD, .color = live ? tint : s.textMuted, .wrap = "n");
        rcTextC(label, .font = F_SMALL, .color = s.textMuted, .wrap = "n");
    }
}

/* The fleet summary row. A dashboard's first row answers "is anything wrong" before
   the reader parses a single card; without it, 48 uniform cards have to be counted
   by eye to learn what one number could have said. */
static void ops_summary(AppState *st) {
    RC_Style s = rcGetStyle();
    const OpsStore *b = &st->store;
    rcRow(.gap = 12, .w = "grow") {
        ops_stat(b->totalText,               "SERVICES", s.text,                    true);
        ops_stat(b->healthText[OPS_OK],      "HEALTHY",  ops_health_color(OPS_OK),   b->healthCount[OPS_OK]   > 0);
        ops_stat(b->healthText[OPS_WARN],    "DEGRADED", ops_health_color(OPS_WARN), b->healthCount[OPS_WARN] > 0);
        ops_stat(b->healthText[OPS_DOWN],    "DOWN",     ops_health_color(OPS_DOWN), b->healthCount[OPS_DOWN] > 0);
    }
}

/* A tag pill (rounded, muted). */
static void ops_chip(const char *label) {
    RC_Style s = rcGetStyle();
    rcBox(.bg = s.surfaceAlt, .px = 8, .py = 2, .align = "cc",
          .borderRadius = "all-full") {
        rcTextC(label, .font = F_SMALL, .color = s.textMuted);
    }
}

/* Does this service pass every filter now in force? Four independent questions and
   one answer, so a pane never has to re-derive the rule. */
static bool ops_visible(const AppState *st, const OpsService *sv) {
    if (st->group >= 0 && sv->group != (uint8_t)st->group)                return false;
    if (st->onlyUnhealthy && sv->health == OPS_OK)                        return false;
    if (st->regionFilter > 0 && (sv->region & 3u) != (unsigned)(st->regionFilter - 1))
        return false;
    if (st->tierFilter > 0 && sv->tier != (uint8_t)st->tierFilter)        return false;
    return true;
}

/* One service card. The cards GROW to share their row rather than holding a fixed
   width, so the grid's right edge lines up with the summary row above it at every
   window width instead of leaving a ragged strip of bare canvas. */
static void ops_card(AppState *st, int idx) {
    RC_Style          s  = rcGetStyle();
    const OpsService *sv = &st->store.svc[idx];
    const char       *id = CARD_IDS[idx];
    bool sel = (idx == st->selected);
    bool hov = rcIsHovered(id);

    /* A failing card must be findable WITHOUT reading it: among 48 cards an 8 px dot
       is not enough, because the eye scans surfaces rather than glyphs. Selection
       still wins the border, because that is the state the user just caused. The dot
       and the word stay, so nothing here is carried by colour alone. */
    RC_Color edge = sel            ? s.primary
                  : sv->health != OPS_OK ? ops_health_color(sv->health)
                                          : s.border;

    /* A card is a PANEL: it rests one step above the canvas and lifts another on
       hover. Resting a card AT the canvas colour turns a wall of panels into a wall
       of outlines, which is the grammar of a form. */
    rcColumn(.id = id, .bg = (sel || hov) ? s.surfaceAlt : s.surface,
             .gap = 7, .p = 10, .borderRadius = "all-md",
             .border = { .color = edge, .width = "all-1" }, .w = "grow") {
        rcRow(.gap = 7, .align = "cl", .w = "grow") {
            ops_dot(sv->health);
            rcTextC(sv->name, .font = F_BODY, .color = s.text);
            rcBox(.w = "grow") {}
            ops_chip(TIER_LABEL[sv->tier <= 3 ? sv->tier : 0]);
        }
        rcRow(.gap = 6, .align = "cl", .w = "grow") {
            rcTextC(sv->owner, .font = F_SMALL, .color = s.textMuted);
            rcBox(.w = "grow") {}
            rcTextC(OPS_REGIONS[sv->region & 3u], .font = F_SMALL, .color = s.textMuted);
        }
        rcRow(.gap = 12, .align = "cl", .w = "grow") {
            rcTextC(sv->rps, .font = F_MONO, .color = s.text);
            rcTextC(sv->p99, .font = F_MONO, .color = s.textMuted);
            rcBox(.w = "grow") {}
            rcTextC(HEALTH_LABEL[sv->health < OPS_HEALTH_COUNT ? sv->health : 0],
                    .font = F_SMALL, .color = ops_health_color(sv->health));
        }
    }
}

/* One legend line at the foot of the nav: a dot, what it means, and how many the
   fleet has of it. */
static void ops_legend_row(uint8_t health, const char *label, const char *count) {
    RC_Style s = rcGetStyle();
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        ops_dot(health);
        rcTextC(label, .font = F_SMALL, .color = s.textMuted);
        rcBox(.w = "grow") {}
        rcTextC(count, .font = F_MONO, .color = s.text);
    }
}

/* The left nav: the eight fleet groups with a roll-up dot each, and at its foot the
   key to those dots. The legend is pinned to the bottom rather than stacked under
   the groups, so the column reads as a navigation pane with a footer instead of a
   short list above a void. */
static void ops_nav(AppState *st) {
    RC_Style        s = rcGetStyle();
    const OpsStore *b = &st->store;
    rcColumn(.bg = s.surface, .gap = 4, .p = 12, .w = "190px", .h = "grow") {
        rcTextL("FLEET", .font = F_SMALL, .color = s.textMuted);
        rcBox(.h = "6px") {}
        for (int g = 0; g < OPS_GROUP_COUNT; g++) {
            const char *id  = NAV_IDS[g];
            bool        act = (st->group == g);
            rcRow(.id = id,
                  .bg = act ? s.surfaceAlt : (rcIsHovered(id) ? s.background : RC_TRANSPARENT),
                  .gap = 8, .px = 9, .py = 7, .align = "cl", .borderRadius = "all-sm",
                  .w = "grow") {
                ops_dot(st->store.groupHealth[g]);
                rcTextC(OPS_GROUPS[g], .font = F_BODY,
                        .color = act ? s.text : s.textMuted);
            }
        }
        rcBox(.h = "grow") {}
        rcBox(.bg = s.border, .w = "grow", .h = "1px") {}
        rcBox(.h = "6px") {}
        rcTextL("HEALTH", .font = F_SMALL, .color = s.textMuted);
        rcBox(.h = "2px") {}
        ops_legend_row(OPS_OK,   "Healthy",  b->healthText[OPS_OK]);
        ops_legend_row(OPS_WARN, "Degraded", b->healthText[OPS_WARN]);
        ops_legend_row(OPS_DOWN, "Down",     b->healthText[OPS_DOWN]);
        rcBox(.h = "6px") {}
        rcTextL("production", .font = F_SMALL, .color = s.textMuted);
    }
}

/* The stacked arm's substitute for ops_nav: the same groups, ids and toggle, as a
   chip grid instead of a column that would push the cards off the screen. The rows
   are cut here rather than left to wrapping because this is a GRID - every chip takes
   an equal share of the row, so the last row lines up with the ones above it. */
static void ops_nav_chips(AppState *st, float rowW) {
    RC_Style s = rcGetStyle();
    int perRow = (int)((rowW + OPS_CHIP_GAP) / (OPS_CHIP_W + OPS_CHIP_GAP));
    if (perRow < 1) perRow = 1;
    if (perRow > OPS_GROUP_COUNT) perRow = OPS_GROUP_COUNT;
    float chipW = (rowW - (float)((perRow - 1) * OPS_CHIP_GAP)) / (float)perRow;

    rcColumn(.gap = OPS_CHIP_GAP, .w = "grow") {
        rcTextL("FLEET", .font = F_SMALL, .color = s.textMuted);
        for (int g = 0; g < OPS_GROUP_COUNT; g += perRow) {
            rcRow(.gap = OPS_CHIP_GAP, .w = "grow") {
                for (int k = g; k < g + perRow && k < OPS_GROUP_COUNT; k++) {
                    const char *id  = NAV_IDS[k];
                    bool        act = (st->group == k);
                    /* The active chip takes the "Unhealthy only" pill's look, so the
                       page's filters read as one family, and py 12 makes it finger
                       sized. WHITE on the active fill, because a 14 px label owes the
                       reader 4.5:1 and the canvas colour does not clear it. */
                    rcRow(.id = id,
                          .bg = act ? s.primary : (rcIsHovered(id) ? s.surfaceAlt : s.surface),
                          .gap = 8, .px = 9, .py = 12, .align = "cc",
                          .borderRadius = "all-full",
                          .border = { .color = act ? s.primary : s.border, .width = "all-1" },
                          .wType = RC_PX(chipW)) {
                        ops_dot(st->store.groupHealth[k]);
                        rcTextC(OPS_GROUPS[k], .font = F_BODY,
                                .color = act ? RC_WHITE : s.text);
                    }
                }
            }
        }
    }
}

/* The card grid, `cols` across, shared by both arms. Filtered-out services are
   skipped without opening an element, so a filtered grid declares fewer elements
   rather than declaring hidden ones. A short last row is padded to `cols` empty
   slots, which is what keeps its cards the width of the rows above. */
static void ops_grid(AppState *st, int cols) {
    RC_Style s = rcGetStyle();
    for (int i = 0; i < OPS_SVC_COUNT; ) {
        int placed = 0;
        rcRow(.gap = OPS_CARD_GAP, .align = "tl", .w = "grow") {
            while (i < OPS_SVC_COUNT && placed < cols) {
                int idx = st->store.order[i];

                if (ops_visible(st, &st->store.svc[idx]))
                    { ops_card(st, idx); placed++; }
                i++;
            }
            for (int k = placed; k < cols; k++)
                rcBox(.w = "grow") {}
        }
        if (placed == 0) break;   /* the filter emptied the tail */
    }
    rcBox(.h = "8px") {}
    rcTextL("End of fleet", .font = F_SMALL, .color = s.textMuted);
}

/* The fleet pane's contents: the summary, then either the grid or the reason it is
   empty. BOTH arms call this, because a summary that appears only on a desktop is
   missing from the one screen with no room to count cards by eye. */
static void ops_fleet_body(AppState *st, int cols, bool withSummary) {
    if (withSummary)
        ops_summary(st);
    /* A filter that matches nothing must SAY so: an empty pane reads as a broken
       dashboard, and here it is usually the best news the app can deliver. */
    if (st->onlyUnhealthy && st->store.healthCount[OPS_WARN] == 0
                          && st->store.healthCount[OPS_DOWN] == 0) {
        RC_Style s = rcGetStyle();
        rcColumn(.bg = s.surface, .gap = 4, .p = 24, .align = "cc",
                 .borderRadius = "all-md", .w = "grow") {
            rcTextL("No unhealthy services", .font = F_BODY, .color = s.text);
            rcTextL("Every service in the fleet is reporting OK.",
                    .font = F_SMALL, .color = s.textMuted);
        }
    } else {
        ops_grid(st, cols);
    }
}

/* The inventory pane, wide arm only: the grid inside its own scroll container. The
   stacked arm lays the same grid straight into the page column, because a list that
   scrolls inside a page that scrolls is two thumbs for one finger. `innerW` is the
   viewport less the safe insets, in layout units. */
static void ops_inventory(AppState *st, float innerW) {
    /* Columns are derived from the live viewport, so the grid genuinely reflows. */
    int avail = (int)innerW - OPS_NAV_W - OPS_RAIL_W - 48;
    int cols  = avail / (OPS_CARD_W + OPS_CARD_GAP);
    if (cols < 1) cols = 1;
    if (cols > 4) cols = 4;

    rcColumn(.id = "InvScroll", .gap = 12, .p = 16, .scroll = "v", .w = "grow",
             .h = "grow") {
        ops_fleet_body(st, cols, true);
    }

    /* A finger press-drag already pans the scroll container under it and a desktop
       has its wheel, so the bar is an indicator rather than the only way in - but 48
       cards is a long list and the bar is the one thing on screen that says how far
       it runs, so a row cut by the fold reads as scrollable rather than cropped. */
    rcScrollbar("InvScroll");
}

/* The sparkline: 24 bars re-read from the live ring every frame, beside a gutter
   that states the scale. SCALED TO THE RING'S OWN RANGE, not to 0-100, or a series
   whose variance is a third of the band reads as a flat picket fence - and the range
   is stated, because a rescaled series without its numbers is worse than a flat one. */
static void ops_sparkline(const OpsLive *live) {
    RC_Style s    = rcGetStyle();
    int      lo   = live->sparkLo;
    int      span = live->sparkHi - lo;
    if (span < 1) span = 1;

    rcRow(.gap = 8, .align = "tl", .w = "grow") {
        rcRow(.gap = 2, .align = "bl", .w = "grow", .hType = RC_PX(OPS_SPARK_H)) {
            for (int i = 0; i < OPS_SPARK_COUNT; i++) {
                int  v = live->spark[i];
                int  h = 4 + ((v - lo) * (OPS_SPARK_H - 4)) / span;
                bool hot = v > OPS_SPARK_HOT;
                rcBox(.bg = hot ? s.warningHover : OPS_DATA_BLUE,
                      .borderRadius = "t-sm", .w = "grow", .hType = RC_PX(h)) {}
            }
        }
        /* The axis gutter, top and bottom only: two numbers are the whole scale of a
           sparkline, and a third would cost a line for no reading it enables. */
        rcColumn(.align = "tr", .wType = RC_PX(OPS_AXIS_W), .hType = RC_PX(OPS_SPARK_H)) {
            rcTextC(live->hiText, .font = F_SMALL, .color = s.textMuted, .wrap = "n");
            rcBox(.h = "grow") {}
            rcTextC(live->loText, .font = F_SMALL, .color = s.textMuted, .wrap = "n");
        }
    }
}

/* The incident banner, the highest-priority object on the screen. Its tint follows
   the pulse, so it changes on every frame; it is also a CONTROL - clicking it selects
   the service it names, which is what an operator reading it wants next, and polling
   rcClicked is what gives it the pointer cursor. */
static void ops_incident(AppState *st) {
    RC_Style          s    = rcGetStyle();
    const OpsLive    *live = &st->store.live;
    const OpsService *sv   = &st->store.svc[OPS_INCIDENT_SVC];

    rcColumn(.id = "incident",
             .bg = rcAlpha(s.danger, (uint8_t)(40 + (int)(live->pulse * 70.0f))),
             .gap = 5, .p = 11, .borderRadius = "all-md", .w = "grow") {
        rcRow(.gap = 7, .align = "cl", .w = "grow") {
            ops_dot(OPS_DOWN);
            rcTextL("ACTIVE INCIDENT", .font = F_SMALL, .color = s.text);
        }
        /* The service is named from the model, not from a literal, so the banner can
           never outlive the row it points at. Full-strength text: the red tint
           already carries the severity, so the words do not have to be quiet too. */
        rcRow(.gap = 5, .align = "cl", .w = "grow") {
            rcTextC(sv->name, .font = F_SMALL, .color = s.text, .wrap = "n");
            rcTextL("elevated errors", .font = F_SMALL, .color = s.text);
        }
        rcRow(.align = "cl", .w = "grow") {
            rcTextL("open for", .font = F_SMALL, .color = s.textMuted);
            rcBox(.w = "grow") {}
            rcTextC(live->upText, .font = F_MONO, .color = s.text);
        }
    }
    if (rcClicked("incident"))
        st->selected = OPS_INCIDENT_SVC;
}

/* One row of the slowest-services board: the name, then its steady-state p99. */
static void ops_top_row(const OpsService *sv) {
    RC_Style s = rcGetStyle();
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        ops_dot(sv->health);
        rcBox(.overflow = "hidden", .w = "grow") {
            rcTextC(sv->name, .font = F_SMALL, .color = s.text, .wrap = "n");
        }
        rcTextC(sv->p99, .font = F_MONO, .color = s.textMuted, .wrap = "n");
    }
}

/* A small section heading inside the rail. */
static void ops_rail_heading(const char *label) {
    RC_Style s = rcGetStyle();
    rcBox(.bg = s.border, .w = "grow", .h = "1px") {}
    rcTextC(label, .font = F_SMALL, .color = s.textMuted);
}

/* The rail's children, in the order they are declared. Split from the column that
   holds them so the stacked arm can rearrange the same content. */
static void ops_rail_body(AppState *st) {
    RC_Style        s    = rcGetStyle();
    const OpsStore *b    = &st->store;
    const OpsLive  *live = &b->live;

    ops_incident(st);

    /* The range label is the ring's OWN length - OPS_SPARK_COUNT samples at one per
       second - because a range that does not match the data under it is worse than
       no range at all. */
    rcRow(.align = "cl", .w = "grow") {
        rcTextL("p99 LATENCY", .font = F_SMALL, .color = s.textMuted);
        rcBox(.w = "grow") {}
        rcTextL("last 24 s", .font = F_SMALL, .color = s.textMuted);
    }
    ops_sparkline(live);

    rcRow(.align = "cl", .w = "grow") {
        rcTextL("now", .font = F_SMALL, .color = s.textMuted);
        rcBox(.w = "grow") {}
        rcTextC(live->p99Text, .font = F_MD, .color = s.text);
    }
    rcRow(.align = "cl", .w = "grow") {
        rcTextL("requests", .font = F_SMALL, .color = s.textMuted);
        rcBox(.w = "grow") {}
        rcTextC(live->reqText, .font = F_MD, .color = s.text);
    }

    ops_rail_heading("SELECTED");
    {
        const OpsService *sv = &b->svc[st->selected];
        rcColumn(.gap = 5, .w = "grow") {
            rcTextC(sv->name, .font = F_MD, .color = s.text);
            rcRow(.gap = 6, .align = "cl", .w = "grow") {
                ops_chip(TIER_LABEL[sv->tier <= 3 ? sv->tier : 0]);
                ops_chip(OPS_REGIONS[sv->region & 3u]);
            }
            rcTextC(sv->owner, .font = F_SMALL, .color = s.textMuted);
        }
    }

    /* The two boards that answer "what next" once the banner has been read: the
       slowest services, and where the fleet actually runs. Both are seeded roll-ups,
       so they cost two preformatted strings each and no per-frame work. */
    ops_rail_heading("SLOWEST p99");
    rcColumn(.gap = 5, .w = "grow") {
        for (int k = 0; k < OPS_TOP_COUNT; k++)
            ops_top_row(&b->svc[b->topLatency[k]]);
    }

    ops_rail_heading("REGIONS");
    for (int r = 0; r < OPS_REGION_COUNT; r += 2) {
        rcRow(.gap = 8, .w = "grow") {
            for (int k = r; k < r + 2 && k < OPS_REGION_COUNT; k++) {
                rcRow(.gap = 6, .align = "cl", .w = "grow") {
                    rcTextC(OPS_REGIONS[k], .font = F_SMALL, .color = s.textMuted,
                            .wrap = "n");
                    rcBox(.w = "grow") {}
                    rcTextC(b->regionText[k], .font = F_MONO, .color = s.text);
                }
            }
        }
    }
}

/* The wide arm's rail. Two declarations of the same column, because `.scroll` is a
   char[2] and cannot take a ternary: the plain one is the desktop pane, and a window
   shorter than OPS_RAIL_H gets the scrolling one with the bar that belongs to it, so
   the boards at its foot stay reachable rather than being cut off at the edge. */
static void ops_telemetry(AppState *st, bool scrolls) {
    RC_Style s = rcGetStyle();
    if (scrolls) {
        rcColumn(.id = "RailScroll", .bg = s.surface, .gap = 12, .p = 14, .scroll = "v",
                 .w = "260px", .h = "grow") {
            ops_rail_body(st);
        }
        rcScrollbar("RailScroll");
    } else {
        rcColumn(.bg = s.surface, .gap = 12, .p = 14, .w = "260px", .h = "grow") {
            ops_rail_body(st);
        }
    }
}

/* The stacked arm's rail: the same banner, sparkline, readouts and selection, as a
   full-width panel at the head of the page - the incident and the live latency are
   what a phone is pulled out to check. The rearrangement is only horizontal, and the
   rail's two seeded boards are withheld, because a phone that has to scroll past
   them to reach the fleet is worse off for having them. */
static void ops_telemetry_panel(AppState *st) {
    RC_Style          s    = rcGetStyle();
    const OpsLive    *live = &st->store.live;
    const OpsService *sv   = &st->store.svc[st->selected];

    rcColumn(.bg = s.surface, .gap = 12, .p = 14, .borderRadius = "all-md", .w = "grow") {
        ops_incident(st);

        rcRow(.gap = 14, .align = "tl", .w = "grow") {
            rcColumn(.gap = 4, .w = "grow") {
                rcRow(.align = "cl", .w = "grow") {
                    rcTextL("p99 LATENCY", .font = F_SMALL, .color = s.textMuted);
                    rcBox(.w = "grow") {}
                    rcTextL("last 24 s", .font = F_SMALL, .color = s.textMuted);
                }
                ops_sparkline(live);
            }
            rcColumn(.gap = 4, .align = "tr") {
                rcTextL("now", .font = F_SMALL, .color = s.textMuted);
                rcTextC(live->p99Text, .font = F_MD, .color = s.text);
                rcTextL("requests", .font = F_SMALL, .color = s.textMuted);
                rcTextC(live->reqText, .font = F_MD, .color = s.text);
            }
        }

        rcBox(.bg = s.border, .w = "grow", .h = "1px") {}

        rcRow(.align = "cl", .w = "grow") {
            rcTextL("SELECTED", .font = F_SMALL, .color = s.textMuted);
            rcBox(.w = "grow") {}
            rcTextC(sv->owner, .font = F_SMALL, .color = s.textMuted);
        }
        rcRow(.gap = 6, .align = "cl", .w = "grow") {
            rcTextC(sv->name, .font = F_MD, .color = s.text);
            ops_chip(TIER_LABEL[sv->tier <= 3 ? sv->tier : 0]);
            ops_chip(OPS_REGIONS[sv->region & 3u]);
        }
    }
}

void opsdash_seed(AppState *st, unsigned seed) {
    ops_memzero(st, sizeof *st);
    ops_seed(&st->store, seed);
    st->group        = -1;      /* all groups */
    st->regionFilter = 0;       /* all regions */
    st->tierFilter   = 0;       /* all tiers */
    st->selected     = 0;
    st->seeded       = true;
}

void opsdash_update(AppState *st, const AppCtx *ctx) {
    ops_tick(&st->store, ctx->dt);   /* dt == 0 freezes the band; see opsdash_app.h */
}

/* A titlebar scope filter. The combo draws itself; the box only pins its width, so
   the band does not reflow as the chosen label changes length. */
static void ops_scope_combo(const char *id, int *value, const char *const *items,
                            int count, float width) {
    rcBox(.wType = RC_PX(width)) {
        rcCombo(id, value, items, count);
    }
}

void opsdash_layout(AppState *st, const AppCtx *ctx) {
    rcSetStyle(ops_style());   /* installed here, so every caller draws one picture */
    RC_Style s = rcGetStyle();

    /* Selection and filters are driven from the layout pass, because that is where
       the hit test lives.
       rcClicked, NOT hovered-and-pressed. The pair reads the same on a quick click
       and is wrong in two ways that matter: it fires on the PRESS edge, so the user
       cannot slide off and let go to cancel, and it never marks the element as
       clickable, so these cards would show the ARROW cursor while every other button
       in the app shows the hand. */
    for (int i = 0; i < OPS_SVC_COUNT; i++)
        if (rcClicked(CARD_IDS[i]))
            st->selected = i;
    for (int g = 0; g < OPS_GROUP_COUNT; g++)
        if (rcClicked(NAV_IDS[g]))
            st->group = (st->group == g) ? -1 : g;

    /* The safe area in LAYOUT space: the insets arrive in window px and padding is
       spent where the layout runs, so app_safe divides by the zoom. Zero on desktop. */
    RC_Insets safe = app_safe(ctx);

    /* Two bools, one idea each, both read off the measured viewport and never off the
       live window. `stacked` is about WIDTH: below OPS_STACK_W the three panes become
       one scrolling column. `railScrolls` is about HEIGHT in the wide arm: the body
       under the topbar is shorter than the rail's content, so the rail scrolls
       instead of losing its foot. */
    const float innerW      = app_view_w(ctx) - safe.left - safe.right;
    const float bodyH       = app_view_h(ctx) - safe.top - safe.bottom - OPS_TOPBAR_H;
    const bool  stacked     = innerW < OPS_STACK_W;
    const bool  railScrolls = !stacked && bodyH < OPS_RAIL_H;

    /* On a portrait phone the stacked page reaches the foot, so the root ends a HUD
       band above the safe area and the demo chip floats in it over no card. */
    const bool hudBand = stacked && innerW < OPS_HUD_DOCK_W;
    rcColumn(.bg = s.background, .pt = (uint16_t)safe.top,
             .pb = (uint16_t)(safe.bottom + (hudBand ? OPS_HUD_BAND : 0)),
             .pl = (uint16_t)safe.left, .pr = (uint16_t)safe.right, .w = "grow",
             .h = "grow") {
        /* CHROME, NOT CONTENT. The OS drag strip is pinned at .titlebarHeight, so a
           band that scaled with the content zoom would stop matching the strip the
           user can drag. rcUnzoomed() counter-scales by 1/zoom and takes the row as
           its single statement; at zoom 1 it resolves to 1.0 and changes nothing. */
        rcUnzoomed()
        rcRow(.id = RC_ID_WINDOW_DRAG, .bg = s.chrome, .gap = 12, .px = 16,
              .align = "cl", .w = "grow", .hType = RC_PX(OPS_TOPBAR_H)) {
            /* `.wrap = "n"`: the band is pinned at OPS_TOPBAR_H to match the OS drag
               strip, so a title that wraps does not get a second line, it gets an
               overflowing one. */
            rcTextL("Fleet Operations", .font = F_TITLE, .color = s.text, .wrap = "n");
            /* The band's middle stays bare, and that is the drag handle: it is the
               only part of a custom titlebar a user can reliably grab. */
            rcBox(.w = "grow") {}
            /* THE SCOPE CONTROLS, where a reader expects to narrow what they are
               looking at. All three compose in ops_visible.
               TRAP: a press anywhere in the RC_ID_WINDOW_DRAG band starts an OS window
               move, so every widget in it has to sit inside ONE RC_ID_WINDOW_NODRAG
               subtree - the hit test looks that id up once - or its clicks are eaten
               on the desktop. The combos are withheld below OPS_STACK_W. */
            rcRow(.id = RC_ID_WINDOW_NODRAG, .gap = 10, .align = "cc") {
                if (!stacked) {
                    ops_scope_combo("filt_region", &st->regionFilter,
                                    OPS_REGION_FILTER, OPS_REGION_COUNT + 1, 132.0f);
                    ops_scope_combo("filt_tier", &st->tierFilter,
                                    OPS_TIER_FILTER, 4, 104.0f);
                    /* The refresh cadence, stated rather than offered: the model
                       advances one sample a second, and a control that claimed
                       otherwise would be describing an app this is not. */
                    rcTextL("refresh 1 s", .font = F_SMALL, .color = s.textMuted,
                            .wrap = "n");
                }
                /* The border is load-bearing: in this theme `surface` and `chrome`
                   are close enough that an unbordered pill on the titlebar reads as
                   bare text and nobody discovers it is a control. A finger gets a
                   taller pill - the pointer class sizes a hit target. */
                rcRow(.id = "filt_unhealthy", .bg = st->onlyUnhealthy ? s.primary
                                              : (rcIsHovered("filt_unhealthy") ? s.surfaceAlt
                                                                               : s.surface),
                      .gap = 7, .px = 10, .py = (uint16_t)(ctx->view.coarsePointer ? 13 : 5),
                      .align = "cc", .borderRadius = "all-full",
                      .border = { .color = st->onlyUnhealthy ? s.primary : s.border,
                                  .width = "all-1" }) {
                    /* THE LABEL SHORTENS BEFORE THE BAND DOES: the pill's second word
                       is the only thing on this row that costs nothing to lose. */
                    rcTextC(stacked ? "Unhealthy" : "Unhealthy only", .font = F_SMALL,
                            .color = st->onlyUnhealthy ? RC_WHITE : s.text,
                            .wrap = "n");
                }
            }
            /* `.titlebar.custom` means the runner draws no window controls, so an app
               that omits this call leaves the window with no minimise, maximise or
               close. Declared last so the cluster keeps the edge the OS puts it on;
               emits nothing on web or mobile. */
            rcWindowControls();
        }
        if (stacked) {
            /* One page, one scroller: the live panel, then the fleet chips, then the
               cards as wide as the page, with the same 16 px gutters and 12 px gap the
               wide grid uses. Columns come from the page's own width: 393 wide holds
               one card, a 700 px desktop window two. */
            int avail = (int)innerW - 32;
            int cols  = avail / (OPS_CARD_W + OPS_CARD_GAP);
            if (cols < 1) cols = 1;
            if (cols > 4) cols = 4;
            rcColumn(.id = "Page", .gap = 12, .p = 16, .scroll = "v", .w = "grow",
                     .h = "grow") {
                /* THE FIRST THING A PHONE SHOWS IS THE ANSWER, not the way to ask:
                   the counts say whether anything is wrong, the incident says what,
                   and the filters are how you dig. A screen that opens on its own
                   controls makes the reader scroll to learn there was no problem. */
                ops_summary(st);
                ops_telemetry_panel(st);
                ops_nav_chips(st, innerW - 32.0f);
                ops_fleet_body(st, cols, false);
            }
        } else {
            rcRow(.w = "grow", .h = "grow") {
                ops_nav(st);
                ops_inventory(st, innerW);
                ops_telemetry(st, railScrolls);
            }
        }
    }
    if (stacked) rcScrollbar("Page");

    if (rcClicked("filt_unhealthy"))
        st->onlyUnhealthy = !st->onlyUnhealthy;
}

void opsdash_demo_chrome(AppState *st, const AppCtx *ctx) {
    if (ctx->mode != APP_DEMO || !ctx->arena)
        return;
    /* A floating perf readout, demo-only. PASSTHROUGH so clicks fall through it and
       it never blocks the titlebar controls nor the drag band it may dock in. It is
       anchored to the ROOT, whose box includes the safe bands, so the nudge adds them
       and the chip stays inside the safe area. */
    const RC_Insets safe        = app_safe(ctx);
    const float     innerW      = app_view_w(ctx) - safe.left - safe.right;
    const float     bodyH       = app_view_h(ctx) - safe.top - safe.bottom - OPS_TOPBAR_H;
    const bool      stacked     = innerW < OPS_STACK_W;
    const bool      railScrolls = !stacked && bodyH < OPS_RAIL_H;
    const bool      band        = stacked && innerW < OPS_HUD_DOCK_W;
    const bool      docked      = !band && (stacked || railScrolls);
    const RC_Anchor at   = docked ? RC_ANCHOR_TOP_CENTER : RC_ANCHOR_BOTTOM_RIGHT;
    const float     offX = docked ? 0.0f : -16.0f - safe.right;
    const float     offY = docked ? safe.top + 15.0f : -16.0f - safe.bottom;   /* (52 - 22) / 2 */
    RC_String hud = rcFormat(ctx->arena, "%.0f fps \xc2\xb7 %s req \xc2\xb7 zoom %.2f",
                             ctx->dt > 0.0f ? 1.0f / ctx->dt : 0.0f,
                             st->store.live.reqText, (double)ctx->zoom);
    rcBox(.id = "demo_hud", .bg = rcAlpha(RC_BLACK, 150), .px = 10, .py = 5,
          .borderRadius = "all-full",
          .floating = { .to = RC_ATTACH_ROOT, .parent = at, .element = at,
                        .offset = { offX, offY }, .capture = RC_CAPTURE_PASSTHROUGH }) {
        rcText(hud, .font = F_SMALL, .color = RC_WHITE);
    }
}

void opsdash_bench_step(AppState *st, const AppInputSink *in, int frame) {
    (void)st;   /* every action here is synthetic input, never a direct state write */
    /* The scripted scenario. At/after OPSDASH_BENCH_WARMUP the app HOLDS, a strict
       no-op, so a double-rendered frame is byte-identical: the scene is left
       scrolled, one card selected, the pointer parked off-canvas so no hover tint
       depends on a real clock. The coordinates are a first draft for 1280x720. */
    if (!in || frame >= OPSDASH_BENCH_WARMUP)
        return;                                        /* the HOLD */

    if (frame >= 6 && frame < 14) {
        in->move(in->ctx, 520.0f, 380.0f);             /* hover the grid, then scroll it */
        in->wheel(in->ctx, 0.0f, -1.0f);
    } else if (frame == 20) {
        in->move(in->ctx, 520.0f, 300.0f);             /* select a card: press ... */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == 21) {
        in->button(in->ctx, APP_MBTN_LEFT, false);      /* ... release */
    } else if (frame == 30) {
        in->move(in->ctx, 90.0f, 150.0f);              /* filter to one nav group: press ... */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == 31) {
        in->button(in->ctx, APP_MBTN_LEFT, false);
    } else if (frame == 40) {
        in->move(in->ctx, 90.0f, 150.0f);              /* ... and back to all groups */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == 41) {
        in->button(in->ctx, APP_MBTN_LEFT, false);
    } else if (frame == OPSDASH_BENCH_WARMUP - 1) {
        in->move(in->ctx, -100.0f, -100.0f);           /* park off-canvas into the hold */
    }
}
