/*
    photo_gen.h - the library's pictures, computed

    Every photograph in this app is a function of (u, v) evaluated once per pixel:
    layered noise for landforms, a light source, atmospheric haze and a little grain.
    There are no files to ship and no two runs differ, which is what keeps the app's
    scripted run deterministic. Nothing here knows about RayClay; the output is RGBA8
    the app hands to rcImageUpdate.

    A picture renders in rows, so a large one can be spread over a few frames:

        pg_begin(&job, &shot, pixels, width, height);
        while (!pg_step(&job, 64)) ... draw a frame ...

    The same shot renders the same composition at any size, so a 24 px placeholder, a
    320 px tile and a 1280 px viewer image are all one picture at three resolutions.
*/
#ifndef PHOTO_GEN_H
#define PHOTO_GEN_H

#define PG_MAX_EDGE 1280
#define PG_LAYERS   6

typedef enum {
    PG_ALPINE = 0,   /* ridges fading into haze, snow, an optional still lake */
    PG_FOREST,       /* conifer lines receding into morning fog               */
    PG_COAST,        /* a low sun over open water, headlands in silhouette    */
    PG_BEACH,        /* turquoise shallows breaking on pale sand              */
    PG_DUNES,        /* wind-cut sand, one lit face and one shaded            */
    PG_FIELD,        /* flower rows running to a vanishing point              */
    PG_SKYLINE,      /* towers at blue hour, lit windows, a harbor reflection */
    PG_AURORA,       /* stars, green curtains, a mountain in silhouette       */
    PG_FLOWER,       /* one bloom filling the frame over soft green           */
    PG_BOKEH,        /* city lights thrown out of focus                       */
    PG_FALLS,        /* a waterfall on a dark mossy cliff                     */
    PG_COFFEE        /* a cup on a wooden table, from above                   */
} PgScene;

/* One look: the colors a scene reads by role. Hex is 0xRRGGBB. */
typedef struct {
    uint32_t zenith, mid, horizon;   /* the sky, top to skyline                     */
    uint32_t sun, cloudLit, cloudDark;
    uint32_t massLit, massShade;     /* rock, trees, sand, petals: the subject      */
    uint32_t haze;                   /* what distance fades to                      */
    uint32_t extra, extra2;          /* water, soil, a flower's heart, by scene     */
    float    sunX, sunY, sunPower;   /* in frame units                              */
    float    cloud;                  /* cover threshold: .46 heavy, .64 sparse, 1 no */
    float    snow;                   /* how far snow reaches down a ridge           */
} PgStyle;

typedef struct {
    PgScene scene;
    PgStyle look;
    unsigned seed;
    float   gain;      /* per-shot exposure, around 1  */
    float   warm;      /* per-shot white balance: above 0 warmer, below cooler */
    int     lake;      /* mirror the scene in still water */
    int     flip;      /* the frame left to right: the same place, faced the other way */
} PgShot;

typedef struct { float r, g, b; } PgColor;

typedef struct {
    PgShot  shot;
    PgColor zenith, mid, horizon, sun, cloudLit, cloudDark;
    PgColor massLit, massShade, haze, extra, extra2;
    float   asp, px;             /* width / height; one pixel in v */
    int     w, h, y, oct;        /* size, next row, noise octaves  */
    float   col[PG_LAYERS][PG_MAX_EDGE];  /* per-column ridge tops: a scene's silhouettes */
    float   discX[40], discY[40], discR[40];
    PgColor discC[40];
    unsigned char *out;
} PgJob;

static inline float pg_clamp(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }
static inline float pg_mixf(float a, float b, float t) { return a + (b - a) * t; }
static inline float pg_abs(float x) { return x < 0.0f ? -x : x; }
static inline float pg_floor(float x) { int i = (int)x; return (float)(i - (x < (float)i)); }

static inline float pg_smooth(float e0, float e1, float x) {
    float t = pg_clamp((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}

static inline PgColor pg_hex(uint32_t c) {
    PgColor o = { (float)((c >> 16) & 255u) / 255.0f, (float)((c >> 8) & 255u) / 255.0f,
                  (float)(c & 255u) / 255.0f };
    return o;
}

static inline PgColor pg_mix(PgColor a, PgColor b, float t) {
    PgColor o = { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
    return o;
}

static inline PgColor pg_add(PgColor a, PgColor b, float k) {
    PgColor o = { a.r + b.r * k, a.g + b.g * k, a.b + b.b * k };
    return o;
}

static inline PgColor pg_scale(PgColor a, float k) {
    PgColor o = { a.r * k, a.g * k, a.b * k };
    return o;
}

static inline uint32_t pg_hash(int x, int y, unsigned seed) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + seed * 2654435761u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static inline float pg_rand(int x, int y, unsigned seed) {
    return (float)(pg_hash(x, y, seed) & 0xffffu) / 65535.0f;
}

/* Sine by a parabola in turns: close enough for a petal or a ripple, and no libm. */
static inline float pg_sin(float x) {
    float t = x * 0.15915494f;
    t -= pg_floor(t + 0.5f);
    float y = 8.0f * t - 16.0f * t * pg_abs(t);
    return y * (0.775f + 0.225f * pg_abs(y));
}

static inline float pg_cos(float x) { return pg_sin(x + 1.5707963f); }

static inline float pg_sqrt(float x) {
    if (x <= 0.0f)
        return 0.0f;
    union { float f; uint32_t u; } v = { x };
    v.u = (v.u >> 1) + 0x1fc00000u;
    float r = v.f;
    r = 0.5f * (r + x / r);
    return 0.5f * (r + x / r);
}

static inline float pg_atan2(float y, float x) {
    float ax = pg_abs(x), ay = pg_abs(y);
    float hi = ax > ay ? ax : ay, lo = ax > ay ? ay : ax;
    if (hi <= 0.0f)
        return 0.0f;
    float a = lo / hi, s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) r = 1.5707963f - r;
    if (x < 0.0f) r = 3.1415927f - r;
    return y < 0.0f ? -r : r;
}

static float pg_noise(float x, float y, unsigned seed) {
    float fx0 = pg_floor(x), fy0 = pg_floor(y);
    int   ix = (int)fx0, iy = (int)fy0;
    float fx = x - fx0, fy = y - fy0;
    float sx = fx * fx * (3.0f - 2.0f * fx), sy = fy * fy * (3.0f - 2.0f * fy);
    float a = pg_rand(ix, iy, seed), b = pg_rand(ix + 1, iy, seed);
    float c = pg_rand(ix, iy + 1, seed), d = pg_rand(ix + 1, iy + 1, seed);
    return pg_mixf(pg_mixf(a, b, sx), pg_mixf(c, d, sx), sy);
}

static float pg_noise1(float x, unsigned seed) {
    float fx0 = pg_floor(x);
    int   ix = (int)fx0;
    float fx = x - fx0;
    float sx = fx * fx * (3.0f - 2.0f * fx);
    return pg_mixf(pg_rand(ix, 0, seed), pg_rand(ix + 1, 0, seed), sx);
}

static float pg_fbm(float x, float y, int oct, unsigned seed) {
    float sum = 0.0f, amp = 0.5f, total = 0.0f;
    for (int i = 0; i < oct; i++) {
        sum += amp * pg_noise(x, y, seed + (unsigned)i);
        total += amp;
        x = x * 2.03f + 17.1f;
        y = y * 2.01f + 9.3f;
        amp *= 0.5f;
    }
    return sum / total;
}

/* Peaks, not hills: folding the noise at its midpoint turns its smooth tops into creases. */
static float pg_ridges(float x, int oct, unsigned seed) {
    float sum = 0.0f, amp = 0.6f, total = 0.0f;
    for (int i = 0; i < oct; i++) {
        float n = 1.0f - pg_abs(2.0f * pg_noise1(x, seed + (unsigned)i) - 1.0f);
        sum += amp * n * n;
        total += amp;
        x = x * 2.13f + 7.7f;
        amp *= 0.47f;
    }
    return sum / total;
}

/* Conifers along a line: overlapping cones, each with its own height and spread. */
static float pg_treeline(float x, float hmax, unsigned seed) {
    int   i = (int)pg_floor(x);
    float best = 0.0f;
    for (int k = i - 1; k <= i + 1; k++) {
        float cx = (float)k + pg_rand(k, 1, seed);
        float hh = hmax * (0.55f + 0.45f * pg_rand(k, 2, seed));
        float wd = 0.55f + 0.35f * pg_rand(k, 3, seed);
        float d = pg_abs(x - cx) / wd;
        if (d < 1.0f) {
            float cone = hh * (1.0f - d);
            if (cone > best)
                best = cone;
        }
    }
    return best;
}

/* The sky every outdoor scene shares: a three-stop gradient, a sun with a core and a
   wide glow, and clouds lit from the sun's side. */
static PgColor pg_sky(const PgJob *j, float u, float v, float horizon) {
    const PgStyle *s = &j->shot.look;
    float   t = pg_clamp(v / horizon);
    PgColor c = pg_mix(j->zenith, j->mid, pg_smooth(0.0f, 0.62f, t));
    c = pg_mix(c, j->horizon, pg_smooth(0.45f, 1.0f, t));

    float dx = (u - s->sunX) * j->asp, dy = v - s->sunY;
    float d2 = dx * dx + dy * dy;
    float glow = s->sunPower / (1.0f + d2 * 18.0f);
    float core = s->sunPower / (1.0f + d2 * 900.0f);
    c = pg_add(c, j->sun, glow * 0.38f + core * 0.80f);

    if (s->cloud < 1.0f) {
        float cx = u * j->asp * 2.4f + 3.0f, cy = v * 7.0f;
        float warp = pg_noise(cx * 0.8f, cy * 0.8f, j->shot.seed + 7u);
        float n = pg_fbm(cx + warp * 1.2f, cy + warp * 0.7f, j->oct, j->shot.seed + 3u);
        float cover = pg_smooth(s->cloud, s->cloud + 0.16f, n);
        if (cover > 0.0f) {
            float lit = pg_clamp(glow * 1.8f + pg_smooth(s->cloud, s->cloud + 0.3f, n) * 0.6f);
            PgColor cc = pg_mix(j->cloudDark, j->cloudLit, lit);
            c = pg_mix(c, cc, cover * (1.0f - t * t * 0.55f));
        }
    }
    return c;
}

/* A slope's lit and shaded faces. A ridge's steepness at a point runs on down the mountain
   at an angle, so reading the ridge line a little to one side of the pixel, further the
   deeper the pixel lies, draws the long diagonal faces of real rock from the silhouette
   alone. @return the rise or fall there, positive where the ground falls to the right. */
static float pg_face(const PgJob *j, int l, int x, float u, float depth, float reach) {
    const int span = j->w / 80 + 1;
    float wobble = pg_noise(u * j->asp * 5.0f, depth * 16.0f, j->shot.seed + 70u) - 0.5f;
    int   c = x + (int)((depth * reach + wobble * 0.05f) * (float)j->w);
    int   a = c - span, b = c + span;
    a = a < 0 ? 0 : a >= j->w ? j->w - 1 : a;
    b = b < 0 ? 0 : b >= j->w ? j->w - 1 : b;
    return j->col[l][b] - j->col[l][a];
}

/* ---- PG_ALPINE ---- */

static const float PG_ALPINE_BASE[4] = { 0.40f, 0.50f, 0.60f, 0.74f };
static const float PG_ALPINE_AMP[4]  = { 0.20f, 0.20f, 0.14f, 0.07f };
static const float PG_ALPINE_HAZE[4] = { 0.50f, 0.28f, 0.12f, 0.04f };

static void pg_alpine_begin(PgJob *j) {
    for (int l = 0; l < 4; l++) {
        for (int x = 0; x < j->w; x++) {
            float u = (float)x / (float)j->w * j->asp;
            float r = pg_ridges(u * (1.3f + (float)l * 0.6f) + (float)l * 11.0f, 6 + (j->w > 600),
                                j->shot.seed + (unsigned)l * 31u);
            float top = PG_ALPINE_BASE[l] - PG_ALPINE_AMP[l] * r * 1.9f;
            if (l == 3)
                top -= pg_treeline(u * 40.0f, 0.04f, j->shot.seed + 9u);
            j->col[l][x] = top;
        }
    }
}

static PgColor pg_alpine_layer(const PgJob *j, int l, int x, float u, float v, float depth) {
    const PgStyle *s = &j->shot.look;
    const float px = u * j->asp;
    if (l == 3) {                                     /* the near slope: dark trees */
        float n = pg_fbm(px * 50.0f, v * 120.0f, 2, j->shot.seed + 50u);
        return pg_mix(pg_scale(j->massShade, 0.55f), j->extra2, n * 0.70f);
    }
    float face = pg_face(j, l, x, u, depth, 0.7f) * (s->sunX >= 0.5f ? 1.0f : -1.0f);
    float n = pg_fbm(px * 8.0f, v * 13.0f, j->oct, j->shot.seed + (unsigned)l * 13u + 40u);
    float lit = pg_smooth(-0.010f, 0.010f, face) * 0.72f + pg_smooth(0.34f, 0.66f, n) * 0.28f;
    PgColor rock = pg_mix(j->massShade, j->massLit, lit);
    float reach = s->snow * (0.025f + 0.17f * n) * (1.25f - (float)l * 0.20f);
    if (reach > 0.0f) {
        float snowy = pg_smooth(reach, reach * 0.66f, depth);
        PgColor snow = pg_mix(pg_mix(j->haze, pg_hex(0x9fb2cf), 0.6f), pg_hex(0xfdfeff), lit);
        rock = pg_mix(rock, snow, snowy);
    }
    float fog = PG_ALPINE_HAZE[l] + 0.40f * pg_smooth(0.02f, 0.30f, depth);
    return pg_mix(rock, j->haze, pg_clamp(fog));
}

static PgColor pg_alpine_at(const PgJob *j, int x, float u, float v, float horizon) {
    for (int l = 3; l >= 0; l--) {
        float depth = v - j->col[l][x];
        if (depth <= 0.0f)
            continue;
        PgColor c = pg_alpine_layer(j, l, x, u, v, depth);
        float cov = depth / j->px;
        if (cov >= 1.0f)
            return c;
        /* A silhouette's edge pixel is part sky: blend it with what lies behind. */
        PgColor behind = pg_sky(j, u, v, horizon);
        for (int b = l - 1; b >= 0; b--) {
            float d2 = v - j->col[b][x];
            if (d2 > 0.0f) {
                behind = pg_alpine_layer(j, b, x, u, v, d2);
                break;
            }
        }
        return pg_mix(behind, c, cov);
    }
    return pg_sky(j, u, v, horizon);
}

static PgColor pg_alpine(const PgJob *j, int x, float u, float v) {
    const float shore = 0.70f;
    if (!j->shot.lake || v < shore)
        return pg_alpine_at(j, x, u, v, j->shot.lake ? shore : 0.74f);
    /* Still water is the scene upside down, broken by a little horizontal ripple. */
    float depth = (v - shore) / (1.0f - shore);
    float ripple = (pg_noise(u * j->asp * 6.0f, 2.0f / (depth + 0.08f), j->shot.seed + 60u) - 0.5f)
                   * (0.006f + depth * 0.05f);
    int   mx = x + (int)(ripple * (float)j->w);
    mx = mx < 0 ? 0 : mx >= j->w ? j->w - 1 : mx;
    float mv = shore - (v - shore) * 1.15f;
    PgColor c = pg_alpine_at(j, mx, (float)mx / (float)j->w, mv < 0.0f ? 0.0f : mv, shore);
    c = pg_mix(c, j->extra, 0.20f + depth * 0.50f);
    if (v < shore + 0.006f)
        c = pg_scale(c, 0.55f);
    return c;
}

/* ---- PG_FOREST ---- */

static const float PG_FOREST_BASE[5] = { 0.42f, 0.53f, 0.64f, 0.77f, 0.92f };
static const float PG_FOREST_TREE[5] = { 0.04f, 0.055f, 0.075f, 0.10f, 0.13f };
static const float PG_FOREST_FREQ[5] = { 96.0f, 72.0f, 52.0f, 38.0f, 26.0f };
static const float PG_FOREST_NEAR[5] = { 0.14f, 0.30f, 0.52f, 0.76f, 0.97f };

static void pg_forest_begin(PgJob *j) {
    for (int l = 0; l < 5; l++) {
        unsigned seed = j->shot.seed + (unsigned)l * 17u;
        for (int x = 0; x < j->w; x++) {
            float u = (float)x / (float)j->w * j->asp;
            float hill = pg_noise1(u * 1.6f + (float)l * 5.0f, seed) * 0.12f;
            /* Fall means broadleaf: a lumpy line of crowns rather than a row of spires. */
            float trees = j->shot.look.extra != 0u
                          ? pg_fbm(u * PG_FOREST_FREQ[l] * 1.1f, 1.0f, 4,
                                   seed + 3u) * PG_FOREST_TREE[l]
                          : pg_treeline(u * PG_FOREST_FREQ[l], PG_FOREST_TREE[l], seed + 3u);
            /* Boughs break a cone's straight edge. */
            trees += pg_fbm(u * PG_FOREST_FREQ[l] * 6.0f, 2.0f, 3, seed + 7u)
                     * PG_FOREST_TREE[l] * 0.22f;
            j->col[l][x] = PG_FOREST_BASE[l] - hill - trees;
        }
    }
}

static PgColor pg_forest_layer(const PgJob *j, int l, float u, float v, float depth) {
    const float px = u * j->asp;
    PgColor tree;
    if (j->shot.look.extra != 0u) {
        /* Fall color: crowns of noise, ramped from rust through orange to gold, with a few
           evergreens left standing. */
        const float f = PG_FOREST_FREQ[l] * 1.2f;
        float crown = pg_fbm(px * f, v * f * 1.4f, 5, j->shot.seed + (unsigned)l + 90u);
        float tint = pg_fbm(px * f * 0.30f, v * f * 0.42f, 3, j->shot.seed + (unsigned)l + 5u);
        tree = pg_mix(j->massShade, j->massLit, pg_smooth(0.26f, 0.72f, crown));
        tree = pg_mix(tree, j->extra, pg_smooth(0.46f, 0.70f, tint) * pg_smooth(0.30f, 0.66f,
                crown));
        tree = pg_mix(tree, j->extra2, pg_smooth(0.40f, 0.28f, tint) * 0.7f);
    } else {
        /* Stretched downward, the noise reads as trunks and crowns standing side by side. */
        float n = pg_fbm(px * (46.0f + (float)l * 30.0f), v * (9.0f + (float)l * 7.0f), 3,
                         j->shot.seed + (unsigned)l + 90u);
        tree = pg_mix(j->massShade, j->massLit, pg_smooth(0.34f, 0.78f, n) * 0.55f);
    }
    /* Each ridge stands in fog: thin near its top, thick toward the ridge in front. */
    const float near = PG_FOREST_NEAR[l];
    float fog = (1.0f - near) + near * 0.70f * pg_smooth(0.02f, 0.18f,
                                                         depth) * (1.0f - near * 0.6f);
    return pg_mix(tree, j->haze, pg_clamp(fog));
}

static PgColor pg_forest(const PgJob *j, int x, float u, float v) {
    for (int l = 4; l >= 0; l--) {
        float depth = v - j->col[l][x];
        if (depth <= 0.0f)
            continue;
        PgColor c = pg_forest_layer(j, l, u, v, depth);
        float cov = depth / j->px;
        if (cov >= 1.0f)
            return c;
        /* An edge pixel is part what lies behind it, or the outline shows. */
        for (int b = l - 1; b >= 0; b--) {
            float d2 = v - j->col[b][x];
            if (d2 > 0.0f)
                return pg_mix(pg_forest_layer(j, b, u, v, d2), c, cov);
        }
        return pg_mix(pg_sky(j, u, v, 0.62f), c, cov);
    }
    return pg_sky(j, u, v, 0.62f);
}

/* ---- PG_COAST ---- */

static void pg_coast_begin(PgJob *j) {
    float flip = (j->shot.seed & 1u) ? 1.0f : 0.0f;
    for (int x = 0; x < j->w; x++) {
        float u = (float)x / (float)j->w;
        float p = flip > 0.5f ? u : 1.0f - u;
        float xx = u * j->asp;
        float nearLand = pg_smooth(0.58f, 0.10f, p) * (0.20f + 0.10f * pg_fbm(xx * 3.0f, 1.0f, 4,
                j->shot.seed));
        float farLand = pg_smooth(0.92f, 0.45f, 1.0f - p) * 0.07f * (0.5f + pg_fbm(xx * 5.0f, 2.0f,
                3, j->shot.seed + 1u));
        j->col[0][x] = nearLand;
        j->col[1][x] = farLand;
    }
}

static PgColor pg_coast(const PgJob *j, int x, float u, float v) {
    const PgStyle *s = &j->shot.look;
    const float horizon = 0.56f;
    if (v < horizon) {
        PgColor c = pg_sky(j, u, v, horizon);
        float   far = horizon - j->col[1][x], near = horizon - j->col[0][x];
        if (j->col[1][x] > 0.001f && v > far)
            c = pg_mix(c, pg_mix(j->massShade, j->haze, 0.62f), pg_clamp((v - far) / j->px));
        if (j->col[0][x] > 0.001f && v > near)
            c = pg_mix(c, pg_mix(j->massShade, j->haze, 0.10f + 0.20f * pg_smooth(0.0f, 0.2f,
                    v - near)),
                       pg_clamp((v - near) / j->px));
        return c;
    }
    /* Water is a plane in perspective: x/z and 1/z make the waves squash toward the horizon. */
    float depth = (v - horizon) / (1.0f - horizon);
    float z = 1.0f / (depth + 0.045f);
    float wx = (u - 0.5f) * j->asp * z * 1.3f, wz = z * 1.6f;
    float n = pg_fbm(wx, wz, j->oct, j->shot.seed + 20u);
    PgColor sea = pg_mix(j->extra, j->extra2, pg_clamp(depth * 1.3f));
    sea = pg_mix(sea, j->horizon, (1.0f - pg_clamp(depth * 4.0f)) * 0.55f);
    sea = pg_scale(sea, 0.78f + n * 0.5f);
    float spread = 0.03f + depth * 0.24f;
    float dx = (u - s->sunX) / spread;
    float column = s->sunPower / (1.0f + dx * dx);
    float glint = pg_smooth(0.50f, 0.68f, n);
    return pg_add(sea, j->sun, column * (0.25f + glint * 0.95f));
}

/* ---- PG_BEACH ---- */

static PgColor pg_beach(const PgJob *j, int x, float u, float v) {
    const float horizon = 0.38f;
    (void)x;
    if (v < horizon)
        return pg_sky(j, u, v, horizon);
    float xx = u * j->asp;
    float depth = (v - horizon) / (1.0f - horizon);
    float shore = 0.56f + 0.14f * (u - 0.5f) + 0.05f * pg_fbm(xx * 1.8f, 3.0f, 3, j->shot.seed);
    float z = 1.0f / (depth + 0.06f);
    if (depth < shore) {
        float n = pg_fbm((u - 0.5f) * j->asp * z * 1.2f, z * 2.2f, j->oct, j->shot.seed + 5u);
        float t = depth / shore;
        PgColor sea = pg_mix(j->extra, j->massLit, pg_smooth(0.06f, 0.70f, t));
        sea = pg_mix(sea, j->extra2, pg_smooth(0.72f, 1.0f, t));
        sea = pg_scale(sea, 0.90f + n * 0.22f);
        /* Foam: broken white lines parallel to the shore, thickening at its edge. */
        float line = pg_abs(pg_sin((t * 22.0f + n * 3.0f)));
        float foam = pg_smooth(0.90f, 0.99f, line) * pg_smooth(0.45f, 1.0f, t) * pg_smooth(0.42f,
                0.62f, n);
        foam += pg_smooth(0.93f, 1.0f, t + (n - 0.5f) * 0.10f);
        return pg_mix(sea, pg_hex(0xf6fbfa), pg_clamp(foam));
    }
    float t = (depth - shore) / (1.0f - shore + 0.001f);
    float grain = pg_noise(xx * 90.0f, v * 180.0f, j->shot.seed + 8u);
    float dune = pg_fbm(xx * 4.0f, v * 11.0f, 3, j->shot.seed + 9u);
    PgColor wet = pg_mix(j->extra2, j->massShade, 0.68f);
    PgColor sand = pg_mix(j->massShade, pg_hex(0xf6ecd2), pg_smooth(0.32f, 0.68f, dune));
    PgColor c = pg_mix(wet, sand, pg_smooth(0.03f, 0.22f, t));
    return pg_scale(c, 0.97f + grain * 0.07f);
}

/* ---- PG_DUNES ---- */

static const float PG_DUNE_BASE[5] = { 0.46f, 0.56f, 0.68f, 0.82f, 0.99f };
static const float PG_DUNE_AMP[5]  = { 0.06f, 0.09f, 0.13f, 0.17f, 0.20f };

static void pg_dunes_begin(PgJob *j) {
    for (int l = 0; l < 5; l++) {
        unsigned seed = j->shot.seed + (unsigned)l * 23u;
        for (int x = 0; x < j->w; x++) {
            float u = (float)x / (float)j->w * j->asp;
            float n = pg_noise1(u * (1.8f - (float)l * 0.2f) + (float)l * 3.0f, seed);
            n += 0.20f * pg_noise1(u * 5.0f, seed + 1u);
            j->col[l][x] = PG_DUNE_BASE[l] - PG_DUNE_AMP[l] * n;
        }
    }
}

static PgColor pg_dune_layer(const PgJob *j, int l, int x, float u, float v, float depth) {
    float face = pg_face(j, l, x, u, depth, 0.85f) * (j->shot.look.sunX >= 0.5f ? 1.0f : -1.0f);
    float lit = pg_smooth(-0.0015f, 0.0015f, face);
    PgColor c = pg_mix(j->massShade, j->massLit, lit);
    /* Wind ripples, fine and only on the lit faces. */
    float ripple = pg_sin(depth * (240.0f + (float)l * 120.0f) + u * j->asp * 20.0f
                          + pg_noise(u * j->asp * 6.0f, v * 18.0f, 77u) * 5.0f);
    c = pg_scale(c, 1.0f + ripple * 0.030f * lit * (float)l * 0.25f);
    return pg_mix(c, j->haze, 0.62f - (float)l * 0.15f);
}

static PgColor pg_dunes(const PgJob *j, int x, float u, float v) {
    for (int l = 4; l >= 0; l--) {
        float depth = v - j->col[l][x];
        if (depth <= 0.0f)
            continue;
        PgColor c = pg_dune_layer(j, l, x, u, v, depth);
        float cov = depth / j->px;
        if (cov >= 1.0f)
            return c;
        for (int b = l - 1; b >= 0; b--) {
            float d2 = v - j->col[b][x];
            if (d2 > 0.0f)
                return pg_mix(pg_dune_layer(j, b, x, u, v, d2), c, cov);
        }
        return pg_mix(pg_sky(j, u, v, 0.50f), c, cov);
    }
    return pg_sky(j, u, v, 0.50f);
}

/* ---- PG_FIELD ---- */

static PgColor pg_field(const PgJob *j, int x, float u, float v) {
    const float horizon = 0.44f;
    (void)x;
    float xx = u * j->asp;
    float tree = horizon - 0.018f - 0.025f * pg_noise1(xx * 15.0f, j->shot.seed)
                         - 0.018f * pg_noise1(xx * 47.0f, j->shot.seed + 1u);
    if (v < tree)
        return pg_sky(j, u, v, tree);
    if (v < horizon)
        return pg_mix(pg_scale(j->massShade, 0.50f), j->haze, 0.34f);
    float depth = (v - horizon) / (1.0f - horizon);
    float z = 1.0f / (depth + 0.012f);
    float vp = 0.42f + (float)(j->shot.seed % 16u) * 0.012f;
    float row = (u - vp) * j->asp * z * 2.6f;
    float rowf = row - pg_floor(row);
    float tri = pg_abs(rowf - 0.5f) * 2.0f;
    float blur = pg_clamp(0.42f / (z * 0.08f + 0.001f));    /* rows merge toward the horizon */
    float fill = pg_smooth(0.86f + (1.0f - blur) * 0.5f, 0.68f - (1.0f - blur) * 0.5f, tri);
    float n = pg_noise(row * 12.0f, z * 5.0f, j->shot.seed + 4u);
    float bloom = pg_noise(row * 40.0f, z * 26.0f, j->shot.seed + 5u);
    PgColor hue = j->massLit;
    if (j->shot.look.extra2 != 0u) {                        /* tulips: each row its own color */
        int id = (int)pg_floor(row);
        float pick = pg_rand(id, 3, j->shot.seed);
        hue = pick < 0.33f ? j->massLit : pick < 0.66f ? j->extra2 : pg_hex(0xfff0a8);
    }
    PgColor flower = pg_mix(j->massShade, hue, 0.34f + bloom * 0.70f);
    flower = pg_scale(flower, 0.85f + n * 0.30f);
    PgColor soil = pg_mix(j->extra, pg_scale(j->extra, 0.6f), n);
    PgColor c = pg_mix(soil, flower, fill);
    return pg_mix(j->haze, c, pg_smooth(0.0f, 0.16f, depth));
}

/* ---- PG_SKYLINE ---- */

/* Towers per column: col[0] the far row's tops, col[1] the near row's, col[2] and col[3]
   a near tower's id and its left edge, so its windows stay on one grid. */
static void pg_skyline_begin(PgJob *j) {
    float x = 0.0f;
    int   id = 0;
    for (int k = 0; k < j->w; k++) {
        j->col[0][k] = 2.0f;
        j->col[1][k] = 2.0f;     /* below the frame: a gap between towers is open sky */
    }
    while (x < 1.0f) {
        float wd = (0.018f + pg_rand(id, 1, j->shot.seed) * 0.05f) / j->asp;
        float tall = pg_rand(id, 2, j->shot.seed);
        float top = 0.62f - 0.08f - tall * tall * 0.34f;
        int   from = (int)(x * (float)j->w), to = (int)((x + wd) * (float)j->w);
        for (int k = from; k < to && k < j->w; k++)
            j->col[0][k] = top;
        x += wd;
        id++;
    }
    x = -0.02f;
    while (x < 1.0f) {
        float wd = (0.035f + pg_rand(id, 1, j->shot.seed) * 0.075f) / j->asp;
        float tall = pg_rand(id, 2, j->shot.seed);
        float top = 0.70f - 0.04f - tall * tall * tall * 0.46f;
        int   from = (int)(x * (float)j->w), to = (int)((x + wd) * (float)j->w);
        from = from < 0 ? 0 : from;
        for (int k = from; k < to && k < j->w; k++) {
            j->col[1][k] = top;
            j->col[2][k] = (float)id;
            j->col[3][k] = x;
        }
        x += wd + 0.002f;
        id++;
    }
}

static PgColor pg_skyline_at(const PgJob *j, int x, float u, float v) {
    const float horizon = 0.72f;
    PgColor c = pg_sky(j, u, v, horizon);
    if (v > j->col[0][x]) {
        PgColor far = pg_mix(j->massShade, j->haze, 0.50f);
        if (pg_rand((int)(u * j->asp * 260.0f), (int)(v * 300.0f), j->shot.seed) > 0.93f)
            far = pg_mix(far, j->extra, 0.40f);
        c = far;
    }
    if (v > j->col[1][x]) {
        int     id = (int)j->col[2][x];
        PgColor body = pg_mix(j->massShade, j->massLit, pg_rand(id, 5, j->shot.seed) * 0.45f);
        /* Air stands between the viewer and a tower's foot, and the sky lights its head. */
        c = pg_mix(body, j->haze, 0.06f + pg_smooth(0.40f, 0.72f, v) * 0.30f);
        c = pg_add(c, j->horizon, 0.08f * pg_smooth(0.12f, 0.0f, v - j->col[1][x]));
        float wx = (u - j->col[3][x]) * j->asp * 150.0f;
        float wy = (v - j->col[1][x]) * 120.0f;
        float fx = wx - pg_floor(wx), fy = wy - pg_floor(wy);
        if (fx > 0.22f && fx < 0.78f && fy > 0.25f && fy < 0.75f) {
            float on = pg_rand((int)pg_floor(wx), (int)pg_floor(wy), j->shot.seed + (unsigned)id);
            if (on > 0.72f)
                c = pg_mix(c, pg_mix(j->extra, j->extra2, pg_rand(id, 7, j->shot.seed)),
                           0.42f + (on - 0.72f));
        }
    }
    return c;
}

static PgColor pg_skyline(const PgJob *j, int x, float u, float v) {
    const float horizon = 0.72f;
    if (v < horizon)
        return pg_skyline_at(j, x, u, v);
    float depth = (v - horizon) / (1.0f - horizon);
    float z = 1.0f / (depth + 0.05f);
    float n = pg_fbm((u - 0.5f) * j->asp * z * 1.1f, z * 2.6f, 3, j->shot.seed + 21u);
    int   mx = x + (int)((n - 0.5f) * (0.004f + depth * 0.045f) * (float)j->w);
    mx = mx < 0 ? 0 : mx >= j->w ? j->w - 1 : mx;
    float mv = horizon - (v - horizon) * 1.25f;
    PgColor c = pg_skyline_at(j, mx, (float)mx / (float)j->w, mv);
    /* Water breaks a reflection into bands, more of them toward the viewer. */
    c = pg_scale(c, (0.74f - depth * 0.22f) * (0.72f + 0.56f * n));
    if (v < horizon + 0.003f)
        c = pg_mix(c, j->massShade, 0.4f);
    return c;
}

/* ---- PG_AURORA ---- */

static void pg_aurora_begin(PgJob *j) {
    for (int x = 0; x < j->w; x++) {
        float u = (float)x / (float)j->w * j->asp;
        j->col[0][x] = 0.70f - pg_ridges(u * 1.5f, 5, j->shot.seed) * 0.26f;
        j->col[1][x] = 0.50f + (pg_fbm(u * 1.2f, 3.0f, 3, j->shot.seed + 3u) - 0.5f) * 0.46f;
        j->col[2][x] = pg_fbm(u * 24.0f, 1.0f, 4, j->shot.seed + 4u);
        j->col[3][x] = 0.30f + (pg_fbm(u * 1.7f, 7.0f, 3, j->shot.seed + 5u) - 0.5f) * 0.40f;
        j->col[4][x] = pg_fbm(u * 38.0f, 2.0f, 4, j->shot.seed + 6u);
    }
}

/* One curtain: bright along its lower edge, thinning upward, broken into vertical rays. */
static float pg_curtain(float v, float low, float rays) {
    float above = low - v;
    if (above < 0.0f)
        return pg_smooth(0.03f, 0.0f, -above) * 0.55f;
    return pg_smooth(0.36f, 0.0f, above) * (0.18f + pg_smooth(0.36f, 0.68f, rays) * 0.92f);
}

static PgColor pg_aurora_at(const PgJob *j, int x, float u, float v) {
    float xx = u * j->asp;
    PgColor c = pg_mix(j->zenith, j->horizon, pg_smooth(0.0f, 0.75f, v));
    /* Stars: one chance per lattice cell, off-center inside it so no grid shows. */
    float sx = xx * 130.0f, sy = v * 130.0f;
    int   cx = (int)pg_floor(sx), cy = (int)pg_floor(sy);
    float pick = pg_rand(cx, cy, j->shot.seed + 11u);
    if (pick > 0.86f) {
        float ox = sx - (float)cx - pg_rand(cx, cy, 2u), oy = sy - (float)cy - pg_rand(cx, cy, 3u);
        c = pg_add(c, pg_hex(0xffffff), (pick - 0.86f) * 6.0f * pg_smooth(0.04f, 0.0f,
                ox * ox + oy * oy));
    }
    float near = pg_curtain(v, j->col[1][x], j->col[2][x]);
    float far = pg_curtain(v, j->col[3][x], j->col[4][x]) * 0.60f;
    PgColor hi = pg_mix(j->massLit, j->extra, pg_smooth(0.06f, 0.34f, j->col[1][x] - v));
    PgColor hi2 = pg_mix(j->massLit, j->extra, pg_smooth(0.02f, 0.26f, j->col[3][x] - v));
    c = pg_add(c, hi, near * 0.80f);
    c = pg_add(c, hi2, far * 0.80f);
    float top = j->col[0][x];
    if (v > top) {
        float n = pg_fbm(xx * 30.0f, v * 40.0f, 3, j->shot.seed + 13u);
        PgColor rock = pg_mix(pg_scale(j->massShade, 0.6f), pg_add(j->massShade, j->massLit, 0.20f),
                              pg_smooth(0.08f, 0.0f, v - top) * n);
        c = pg_mix(c, rock, pg_clamp((v - top) / j->px));
    }
    return c;
}

static PgColor pg_aurora(const PgJob *j, int x, float u, float v) {
    const float shore = 0.74f;
    if (v < shore)
        return pg_aurora_at(j, x, u, v);
    float depth = (v - shore) / (1.0f - shore);
    float ripple = (pg_noise(u * j->asp * 5.0f, 2.0f / (depth + 0.1f),
                             91u) - 0.5f) * (0.003f + depth * 0.02f);
    int   mx = x + (int)(ripple * (float)j->w);
    mx = mx < 0 ? 0 : mx >= j->w ? j->w - 1 : mx;
    float mv = shore - (v - shore) * 1.1f;
    PgColor c = pg_aurora_at(j, mx, (float)mx / (float)j->w, mv < 0.0f ? 0.0f : mv);
    c = pg_scale(c, 0.72f - depth * 0.20f);
    if (v < shore + 0.004f)
        c = pg_scale(j->massShade, 0.35f);
    return c;
}

/* ---- PG_FLOWER ---- */

static void pg_bokeh_begin(PgJob *j, int count, float rmin, float rmax) {
    for (int i = 0; i < count && i < 40; i++) {
        j->discX[i] = pg_rand(i, 1, j->shot.seed);
        j->discY[i] = pg_rand(i, 2, j->shot.seed);
        j->discR[i] = rmin + (rmax - rmin) * pg_rand(i, 3, j->shot.seed);
        float pick = pg_rand(i, 4, j->shot.seed);
        j->discC[i] = pick < 0.45f ? j->massLit : pick < 0.8f ? j->extra : j->extra2;
    }
}

static PgColor pg_flower(const PgJob *j, int x, float u, float v) {
    (void)x;
    float dx = (u - j->shot.look.sunX) * j->asp, dy = v - j->shot.look.sunY;
    PgColor bg = pg_mix(j->zenith, j->horizon, v);
    float   wash = pg_fbm(u * j->asp * 3.2f, v * 3.2f, 4, j->shot.seed + 2u);
    bg = pg_mix(bg, j->mid, pg_smooth(0.28f, 0.66f, wash));

    float d = pg_sqrt(dx * dx + dy * dy);
    float a = pg_atan2(dy, dx);
    const float R = (0.36f + pg_rand((int)j->shot.seed, 11, 3u) * 0.14f)
                    * (j->asp < 1.0f ? j->asp : 1.0f) + 0.04f;
    /* A close lens leaves little in focus, and light falls across the bloom from one side. */
    const float soft = R * 0.022f + j->px;
    const float light = 1.0f + (-dx * 0.55f - dy * 0.80f) / R * 0.14f;
    const float tex = pg_fbm(dx * 34.0f + 7.0f, dy * 34.0f + 3.0f, 3, j->shot.seed + 11u);
    /* Whorls from the outside in, so an inner petal lies over an outer one. */
    PgColor c = bg;
    for (int k = 0; k < 4; k++) {
        float scale = 1.0f - (float)k * 0.21f;
        float petals = 9.0f + (float)k * 2.0f + (float)(j->shot.seed % 4u);
        float phase = (float)k * 1.3f + (float)(j->shot.seed % 7u);
        float wave = pg_abs(pg_cos((a + phase) * petals * 0.5f));
        float edge = R * scale * (0.62f + 0.38f * wave);
        float wobble = pg_noise(a * 3.0f + 9.0f, (float)k, j->shot.seed) * 0.03f;
        edge += wobble;
        if (d < edge) {
            float t = d / edge;
            float vein = 0.5f + 0.5f * pg_sin(a * petals * 7.0f + d * 12.0f);
            PgColor p = pg_mix(j->massShade, j->massLit, pg_smooth(0.30f, 0.95f,
                                                                   t * (0.65f + wave * 0.45f)));
            p = pg_scale(p, (0.90f + vein * 0.04f + tex * 0.14f) * light);
            p = pg_scale(p, 0.62f + 0.38f * pg_smooth(0.0f, 0.42f, wave));
            float cov = pg_clamp((edge - d) / soft);
            c = pg_mix(c, p, cov);
        }
    }
    float heart = R * 0.20f;
    if (d < heart) {
        float seeds = pg_noise(a * 9.0f, d * 180.0f, j->shot.seed + 6u);
        PgColor h = pg_mix(j->extra, j->extra2, pg_smooth(0.15f, 0.85f, seeds));
        c = pg_mix(c, h, pg_clamp((heart - d) / (j->px * 2.0f)));
    }
    return c;
}

/* ---- PG_BOKEH ---- */

static PgColor pg_bokeh(const PgJob *j, int x, float u, float v) {
    (void)x;
    float xx = u * j->asp;
    PgColor c = pg_mix(j->zenith, j->horizon, pg_smooth(0.1f, 1.0f, v));
    for (int i = 0; i < 40; i++) {
        float dx = xx - j->discX[i] * j->asp, dy = v - j->discY[i], r = j->discR[i];
        float d2 = dx * dx + dy * dy;
        if (d2 < r * r) {
            float d = pg_sqrt(d2) / r;
            float k = pg_smooth(1.0f, 0.93f, d) * (0.20f + 0.24f * d * d * d);
            c = pg_add(c, j->discC[i], k);
        }
    }
    return c;
}

/* ---- PG_FALLS ---- */

/* The cliff's top per column: col[0] the broken lip the water pours from, col[1] the skyline,
   which is that lip under stands of conifers, so the cliff never ends in a straight cut. */
static void pg_falls_begin(PgJob *j) {
    for (int x = 0; x < j->w; x++) {
        float xx = (float)x / (float)j->w * j->asp;
        float stand = pg_smooth(0.35f, 0.55f, pg_noise1(xx * 1.6f, j->shot.seed + 14u));
        j->col[0][x] = 0.02f + 0.24f * pg_fbm(xx * 4.0f, 0.5f, 4, j->shot.seed + 7u);
        j->col[1][x] = j->col[0][x] - stand * pg_treeline(xx * 20.0f, 0.07f, j->shot.seed + 13u);
    }
}

static PgColor pg_falls(const PgJob *j, int x, float u, float v) {
    float xx = u * j->asp;
    /* Wet rock: broad faces under a fine, even grain, with no direction to it, since noise
       stretched along two axes weaves into plaid. Moss is a slow, faint change of color across
       the cliff, which reads as a tone the rock takes and not as shapes laid on it. */
    float faces = pg_fbm(xx * 4.0f, v * 4.0f, j->oct, j->shot.seed + 1u);
    float rough = pg_fbm(xx * 26.0f, v * 26.0f, j->oct, j->shot.seed + 12u);
    PgColor rock = pg_mix(j->massShade, j->massLit, pg_smooth(0.28f, 0.74f,
                                                              faces * 0.60f + rough * 0.40f));
    float moss = pg_smooth(0.34f, 0.66f, pg_fbm(xx * 2.2f, v * 2.2f, 3, j->shot.seed + 3u));
    PgColor c = pg_mix(rock, j->extra2, moss * 0.30f);
    float lip = j->col[0][x], rim = j->col[1][x];
    if (v < lip)   /* the conifers on the lip: dark, so they read as trees and not as rock */
        c = pg_mix(j->massShade, j->extra2, 0.45f);
    /* One horizon for every column: the lip's own would step the sky's gradient column by
       column. */
    if (v < rim)
        c = pg_mix(pg_sky(j, u, v, 0.26f), c, pg_clamp((v - rim + j->px) / j->px));
    /* The water: a column widening as it falls, streaked along the fall. */
    float sway = (pg_noise1(v * 2.4f, j->shot.seed + 4u) - 0.5f) * 0.05f;
    float cx = 0.5f + sway + ((float)(j->shot.seed % 5u) - 2.0f) * 0.07f;
    float halfw = (0.035f + v * 0.085f) / j->asp * 1.5f;
    float dx = pg_abs(u - cx);
    if (v > lip) {
        float edge = pg_smooth(halfw * 1.1f, halfw * 0.72f, dx);
        float streak = pg_fbm(u * j->asp * 95.0f, v * 2.6f, 3, j->shot.seed + 5u);
        PgColor water = pg_mix(j->haze, pg_hex(0xffffff), pg_smooth(0.32f, 0.62f, streak));
        c = pg_mix(c, water, edge * (0.82f + streak * 0.2f));
    }
    /* Spray hides the plunge pool. */
    float pool = 0.84f + 0.04f * pg_noise1(xx * 3.0f, j->shot.seed + 6u);
    if (v > pool) {
        float t = (v - pool) / (1.0f - pool);
        PgColor pond = pg_mix(j->extra, pg_scale(j->extra, 0.55f), t);
        float foam = pg_fbm(xx * 24.0f, v * 44.0f, 3, j->shot.seed + 8u);
        c = pg_mix(pond, pg_hex(0xf2f6f6), pg_smooth(0.50f, 0.72f, foam) * (1.0f - t) * 0.8f);
    }
    float spray = pg_smooth(0.22f, 0.0f, pg_abs(v - pool)) * pg_smooth(0.34f, 0.0f, dx)
                  * (0.5f + pg_fbm(xx * 8.0f, v * 8.0f, 3, j->shot.seed + 9u));
    return pg_mix(c, j->haze, pg_clamp(spray));
}

/* ---- PG_COFFEE ---- */

static PgColor pg_coffee(const PgJob *j, int x, float u, float v) {
    (void)x;
    float xx = u * j->asp;
    float warp = pg_noise(xx * 2.0f, v * 2.0f, j->shot.seed + 2u) * 0.05f;
    float plank = (v + warp * 0.2f) * 5.0f;
    float id = pg_floor(plank);
    float grain = pg_fbm((xx + pg_rand((int)id, 1, j->shot.seed) * 9.0f) * 2.4f, (v + warp) * 70.0f,
                         4, j->shot.seed + 3u);
    PgColor wood = pg_mix(j->massShade, j->massLit, pg_smooth(0.30f, 0.70f, grain));
    wood = pg_scale(wood, 0.82f + pg_rand((int)id, 2, j->shot.seed) * 0.26f);
    if (plank - id < 0.018f)
        wood = pg_scale(wood, 0.45f);

    float cx = (0.5f + ((float)(j->shot.seed % 5u) - 2.0f) * 0.04f) * j->asp;
    float cy = 0.52f;
    float scale = j->asp < 1.0f ? j->asp : 1.0f;
    float dx = xx - cx, dy = v - cy;
    float d = pg_sqrt(dx * dx + dy * dy) / scale;
    float sx = dx + 0.045f * scale, sy = dy + 0.05f * scale;
    float sd = pg_sqrt(sx * sx + sy * sy) / scale;
    PgColor c = pg_scale(wood, 1.0f - 0.58f * pg_smooth(0.50f, 0.34f, sd));
    const float saucer = 0.38f, cup = 0.235f, rim = 0.20f;
    if (d < saucer) {
        PgColor plate = pg_scale(pg_hex(0xf4f1ea), 0.78f + 0.20f * pg_smooth(saucer, saucer * 0.72f,
                d)
                                 - 0.10f * pg_smooth(cup + 0.08f, cup, d));
        c = pg_mix(c, plate, pg_clamp((saucer - d) / (j->px * 1.6f)));
    }
    if (d < cup)
        c = pg_mix(c, pg_scale(pg_hex(0xfafafa), 0.92f + (0.5f - v) * 0.2f),
                   pg_clamp((cup - d) / (j->px * 1.6f)));
    if (d < rim) {
        float a = pg_atan2(dy, dx);
        float crema = pg_fbm(dx * 40.0f + 3.0f, dy * 40.0f, 3, j->shot.seed + 4u);
        PgColor cf = pg_mix(j->extra, j->extra2, pg_smooth(rim, rim * 0.4f,
                                                           d) * (0.7f + crema * 0.5f));
        /* A latte rosetta: a heart, its lobes pulled by a sine so the milk looks poured. */
        float hx = dx / (rim * scale), hy = -(dy / (rim * scale)) + 0.12f;
        float q = hx * hx + hy * hy - 0.30f;
        float heart = q * q * q - hx * hx * hy * hy * hy * 1.2f;
        float swirl = 0.5f + 0.5f * pg_sin(d * 90.0f + a * 1.0f);
        if (heart < 0.0f)
            cf = pg_mix(cf, pg_hex(0xf2dfc2), 0.55f + swirl * 0.40f);
        c = pg_mix(c, cf, pg_clamp((rim - d) / (j->px * 1.6f)));
    }
    return c;
}

/* Loads the shot's colors and fills in what a scene computes per column. */
static void pg_begin(PgJob *j, const PgShot *shot, unsigned char *out, int w, int h) {
    const PgStyle *s = &shot->look;
    j->shot = *shot;
    j->zenith = pg_hex(s->zenith);
    j->mid = pg_hex(s->mid);
    j->horizon = pg_hex(s->horizon);
    j->sun = pg_hex(s->sun);
    j->cloudLit = pg_hex(s->cloudLit);
    j->cloudDark = pg_hex(s->cloudDark);
    j->massLit = pg_hex(s->massLit);
    j->massShade = pg_hex(s->massShade);
    j->haze = pg_hex(s->haze);
    j->extra = pg_hex(s->extra);
    j->extra2 = pg_hex(s->extra2);
    j->w = w < PG_MAX_EDGE ? w : PG_MAX_EDGE;
    j->h = h;
    j->y = 0;
    j->asp = (float)j->w / (float)h;
    j->px = 1.0f / (float)h;
    j->oct = w > 600 ? 6 : w > 80 ? 5 : 3;
    j->out = out;
    switch (shot->scene) {
    case PG_ALPINE:  pg_alpine_begin(j); break;
    case PG_FOREST:  pg_forest_begin(j); break;
    case PG_COAST:   pg_coast_begin(j); break;
    case PG_DUNES:   pg_dunes_begin(j); break;
    case PG_SKYLINE: pg_skyline_begin(j); break;
    case PG_AURORA:  pg_aurora_begin(j); break;
    case PG_FALLS:   pg_falls_begin(j); break;
    case PG_BOKEH:   pg_bokeh_begin(j, 40, 0.035f, 0.11f); break;
    default: break;
    }
}

static PgColor pg_pixel(const PgJob *j, int x, float u, float v) {
    switch (j->shot.scene) {
    case PG_FOREST:  return pg_forest(j, x, u, v);
    case PG_COAST:   return pg_coast(j, x, u, v);
    case PG_BEACH:   return pg_beach(j, x, u, v);
    case PG_DUNES:   return pg_dunes(j, x, u, v);
    case PG_FIELD:   return pg_field(j, x, u, v);
    case PG_SKYLINE: return pg_skyline(j, x, u, v);
    case PG_AURORA:  return pg_aurora(j, x, u, v);
    case PG_FLOWER:  return pg_flower(j, x, u, v);
    case PG_BOKEH:   return pg_bokeh(j, x, u, v);
    case PG_FALLS:   return pg_falls(j, x, u, v);
    case PG_COFFEE:  return pg_coffee(j, x, u, v);
    default:         return pg_alpine(j, x, u, v);
    }
}

/* Renders up to @p rows more rows; true once the picture is whole. Every picture ends the
   same way a camera does: a lens's vignette, a tone curve, a little grain. */
static bool pg_step(PgJob *j, int rows) {
    const float gain = j->shot.gain, warm = j->shot.warm;
    int end = j->y + rows < j->h ? j->y + rows : j->h;
    for (int y = j->y; y < end; y++) {
        float v = ((float)y + 0.5f) / (float)j->h;
        unsigned char *o = j->out + (size_t)y * (size_t)j->w * 4u;
        for (int x = 0; x < j->w; x++, o += 4) {
            /* A flipped shot reads the scene's columns from the far side. */
            const int sx = j->shot.flip ? j->w - 1 - x : x;
            float u = ((float)sx + 0.5f) / (float)j->w;
            PgColor c = pg_pixel(j, sx, u, v);
            float vx = (u - 0.5f) * 1.2f, vy = (v - 0.5f) * 1.2f;
            float vig = gain * (1.0f - (vx * vx + vy * vy) * 0.46f);
            int grain = (int)(pg_hash(x, y, j->shot.seed) & 7u) - 3;
            float cr = pg_clamp(c.r * vig * (1.0f + warm)), cg = pg_clamp(c.g * vig),
                  cb = pg_clamp(c.b * vig * (1.0f - warm));
            cr = pg_mixf(cr, cr * cr * (3.0f - 2.0f * cr), 0.32f);
            cg = pg_mixf(cg, cg * cg * (3.0f - 2.0f * cg), 0.32f);
            cb = pg_mixf(cb, cb * cb * (3.0f - 2.0f * cb), 0.32f);
            int r = (int)(cr * 255.0f) + grain, g = (int)(cg * 255.0f) + grain,
                b = (int)(cb * 255.0f) + grain;
            o[0] = (unsigned char)(r < 0 ? 0 : r > 255 ? 255 : r);
            o[1] = (unsigned char)(g < 0 ? 0 : g > 255 ? 255 : g);
            o[2] = (unsigned char)(b < 0 ? 0 : b > 255 ? 255 : b);
            o[3] = 255;
        }
    }
    j->y = end;
    return j->y >= j->h;
}

#endif /* PHOTO_GEN_H */
