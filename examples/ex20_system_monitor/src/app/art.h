/*
    art.h - the two pictures this app paints for itself with rcImageUpdate: the aurora
    behind the panels, and the CPU heatmap, which has one column of each core's load for
    each step of the time range. A small picture drawn large is smoothed by the sampler,
    which keeps the aurora soft at any window size. The heatmap gives each core 32 rows,
    near a pixel each at its usual size, so the blend stays within a pixel or two and the
    bands keep crisp edges. Each picture costs one quad to draw.
*/
#ifndef APP_ART_H
#define APP_ART_H

enum { AURORA_W = 96, AURORA_H = 60, HEAT_W = 120, HEAT_CORE_H = 32,
       HEAT_H = HEAT_CORE_H * SYS_CORES };

/* The sky: a few wide glows added over the canvas, each fading to nothing at its radius.
   Positions are fractions of the picture; radii are of its height. */
static inline void aurora_paint(RC_Image *out) {
    static const struct { float x, y, radius, r, g, b; } GLOW[] = {
        { 0.10f, 0.00f, 0.95f,  78.0f,  60.0f, 205.0f },   /* indigo, from the top left */
        { 0.86f, 0.22f, 0.85f,   8.0f, 128.0f, 142.0f },   /* teal, on the right        */
        { 0.42f, 1.10f, 0.85f, 104.0f,  36.0f, 150.0f },   /* violet, at the foot       */
    };
    static uint8_t px[AURORA_W * AURORA_H * 4];
    int x, y, g;

    for (y = 0; y < AURORA_H; y++) {
        for (x = 0; x < AURORA_W; x++) {
            uint8_t *p = &px[(y * AURORA_W + x) * 4];
            float    r = 11.0f, gr = 13.0f, b = 22.0f;

            for (g = 0; g < (int)(sizeof GLOW / sizeof GLOW[0]); g++) {
                float dx = ((float)x / (float)AURORA_W - GLOW[g].x) * 1.6f;
                float dy = (float)y / (float)AURORA_H - GLOW[g].y;
                float d  = (dx * dx + dy * dy) / (GLOW[g].radius * GLOW[g].radius);
                float f  = d < 1.0f ? (1.0f - d) * (1.0f - d) * (1.0f - d) * 0.62f : 0.0f;

                r  += f * GLOW[g].r;
                gr += f * GLOW[g].g;
                b  += f * GLOW[g].b;
            }
            p[0] = (uint8_t)(r > 255.0f ? 255.0f : r);
            p[1] = (uint8_t)(gr > 255.0f ? 255.0f : gr);
            p[2] = (uint8_t)(b > 255.0f ? 255.0f : b);
            p[3] = 255;
        }
    }
    rcImageUpdate(out, px, AURORA_W, AURORA_H, 0, RC_PIXEL_RGBA8);
}

/* The heat scale's stops, idle to flat out: night, violet, magenta, orange, candlelight. */
static const uint8_t HEAT_STOP[5][3] = {
    { 18, 14, 48 }, { 88, 34, 134 }, { 188, 56, 122 }, { 248, 122, 62 }, { 253, 228, 130 }
};

static inline RC_Color heat_stop(int i) {
    return rcRgb(HEAT_STOP[i][0], HEAT_STOP[i][1], HEAT_STOP[i][2]);
}

/* Repaints only when the picture would change: a new sample, another host or range. */
static inline void heat_refresh(AppState *st) {
    static uint8_t px[HEAT_W * HEAT_H * 4];
    const HostView *v = &st->view[st->host];
    int first = range_first(st), last = range_last(st), span = last - first + 1;
    int c, x, i, row;

    if (st->heat.width && st->heatSamples == st->samples && st->heatHost == st->host
        && st->heatFirst == first && st->heatLast == last)
        return;
    st->heatSamples = st->samples;
    st->heatHost    = st->host;
    st->heatFirst   = first;
    st->heatLast    = last;
    for (c = 0; c < SYS_CORES; c++) {
        for (x = 0; x < HEAT_W; x++) {
            int   from = first + span * x / HEAT_W;
            int   to   = first + span * (x + 1) / HEAT_W;
            float sum  = 0.0f, t;
            int   stop;

            if (to <= from)
                to = from + 1;
            for (i = from; i < to; i++)
                sum += v->core[c][i];
            t    = sum / (float)(to - from) / 100.0f * 4.0f;
            t    = t < 0.0f ? 0.0f : t > 3.999f ? 3.999f : t;
            stop = (int)t;
            t   -= (float)stop;
            for (row = 0; row < HEAT_CORE_H; row++) {
                uint8_t *p = &px[((c * HEAT_CORE_H + row) * HEAT_W + x) * 4];

                for (i = 0; i < 3; i++)
                    p[i] = (uint8_t)((float)HEAT_STOP[stop][i]
                                     + ((float)HEAT_STOP[stop + 1][i]
                                        - (float)HEAT_STOP[stop][i]) * t);
                p[3] = 255;
            }
        }
    }
    rcImageUpdate(&st->heat, px, HEAT_W, HEAT_H, 0, RC_PIXEL_RGBA8);
}

/* What the pointer is over, in the panel's title row: the core, the moment, the load. */
static inline void heat_readout(const AppState *st, RC_Arena *mem) {
    const HostView *v    = &st->view[st->host];
    RC_Box          box  = rcGetElementBox("heat_img");
    RC_Vec2         at   = rcIsHovered("heat_img") ? rcPointer() : RC_LIT(RC_Vec2){ 0 };
    int             first = range_first(st), span = range_last(st) - first + 1;
    int             core, slot;
    char            when[16];

    if (!rcIsHovered("heat_img") || box.width <= 0.0f || box.height <= 0.0f || span <= 0)
        return;
    core = (int)((at.y - box.y) / box.height * (float)SYS_CORES);
    slot = first + (int)((at.x - box.x) / box.width * (float)span);
    core = core < 0 ? 0 : core >= SYS_CORES ? SYS_CORES - 1 : core;
    slot = slot < first ? first : slot >= first + span ? first + span - 1 : slot;
    clock_text(st, st->at[slot], true, when, (int)sizeof when);
    rcText(rcFormat(mem, "%s  %s  %.0f%%", CORE_LABEL[core + 1], when, (double)v->core[core][slot]),
           .color = load_color(v->core[core][slot]), .size = 12, .wrap = "n");
}

/* Every core at every moment of the range, at a glance: where a line chart averages a
   stuck core away, this shows it as a bright band. */
static inline void heat_panel(AppState *st, RC_Arena *mem, const char *w, const char *h) {
    RC_Style s = rcGetStyle();
    float    from, to;
    char     start[16], end[16];   /* a run keeps its text's address: rcFormat copies these */
    int      i;

    heat_refresh(st);
    view_window(st, &from, &to);
    clock_text(st, from, false, start, (int)sizeof start);
    clock_text(st, to, false, end, (int)sizeof end);

    dash_panel(.id = "p_heat", .w = w, .h = h) {
        rcRow(.gap = 6, .px = 8, .align = "cl", .w = "grow", .hType = RC_PX(PANEL_HEAD)) {
            panel_title("CPU heatmap", "i_heat",
                        "Each core's load over the time range: the brighter, the busier. "
                        "Hover to read one moment.");
            rcSpacer() {}
            heat_readout(st, mem);
        }
        rcRow(.gap = 6, .pb = 8, .px = 8, .w = "grow", .h = "grow") {
            rcColumn(.pb = 22, .h = "grow", .wType = RC_PX(32)) {
                for (i = 0; i < SYS_CORES; i++) {
                    rcBox(.align = "cl", .h = "grow") {
                        rcTextC(CORE_LABEL[i + 1], .color = s.textMuted, .size = 12);
                    }
                }
            }
            rcColumn(.gap = 4, .w = "grow", .h = "grow", .className = "min-w-0") {
                rcBox(.id = "heat_img", .w = "grow", .h = "grow", .image = &st->heat) {}
                rcRow(.gap = 4, .align = "cl", .w = "grow", .hType = RC_PX(18)) {
                    rcText(rcFormat(mem, "%s", start), .color = s.textMuted, .size = 12);
                    rcSpacer() {}
                    /* The scale, in the same stops the picture is painted from. */
                    rcTextL("0", .color = s.textMuted, .size = 12);
                    for (i = 0; i < 4; i++) {
                        rcBox(.h = "6px", .wType = RC_PX(14),
                              .gradient = { .from = heat_stop(i), .to = heat_stop(i + 1),
                                            .dir = "h" }) {}
                    }
                    rcTextL("100%", .color = s.textMuted, .size = 12);
                    rcSpacer() {}
                    rcText(rcFormat(mem, "%s", end), .color = s.textMuted, .size = 12);
                }
            }
        }
    }
}

#endif /* APP_ART_H */
