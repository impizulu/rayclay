/*
    platformer_backend.h - the auto-runner's model and logic, no RayClay at all.

    A seeded endless runner over a finite handcrafted course: the hero runs right
    at a fixed screen x while the world scrolls past, a stepped heightfield makes
    the ground rise and fall, coins reward jumps, parallax bands scroll behind.

    The whole simulation is integer or 1/256-px fixed-point, so a rendered
    position is identical on every machine, and pl_world_step advances exactly
    one tick per call - the caller gates it on dt > 0. Every number the HUD shows
    is formatted here into a fixed buffer on that tick.

    Single-header: define PLATFORMER_BACKEND_IMPLEMENTATION in exactly one TU
    before including it. That TU also gets pl_memzero, which is why the GUI file
    needs no system header of its own.
*/
#ifndef PLATFORMER_BACKEND_H
#define PLATFORMER_BACKEND_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef PLDEF
#define PLDEF static
#endif

/* -- fixed design space (the bench Stage is 1280x720 minus the 52px titlebar) -- */
#define PL_DESIGN_W       1280
#define PL_DESIGN_H        668     /* 720 - 52 titlebar; terrain Y lives in [0, this) */
#define PL_HERO_SCREEN_X   220     /* the hero's fixed on-stage x (px)                */

/* -- capacities (fixed arrays => memset-able POD, deterministic) ---------------
   EVERY BAND HAS TO OUTLAST THE COURSE, and a slower band needs less world to do
   it: a layer drifting at 1/n is still on screen after n times the scroll. So the
   ground-speed arrays (segments, coins, tufts) span the whole PL_LEVEL_LEN, the
   half-speed buildings span half of it, and the 1/8-speed stars an eighth. Size
   one too small and it simply stops - the run carries on over nothing. */
#define PL_MAX_SEGMENTS     96     /* terrain heightfield spans (last = flat tail)   */
#define PL_MAX_COINS        56
#define PL_MAX_HILLS        12     /* far parallax, 1/4 speed                        */
#define PL_MAX_BUILDINGS    56     /* mid parallax, 1/2 speed                        */
#define PL_MAX_TUFTS        72     /* foreground ground-plane decor                  */
#define PL_MAX_STARS        56     /* night sky, the slowest band at 1/8             */
#define PL_MAX_PARTICLES    12     /* dust                                           */

/* -- fixed-point + physics (units: 1/256 px, per one fixed tick) --------------
   The set is chosen together: the jump peaks at 112 px after 48 ticks and covers
   288 world px, an arc tall enough to clear any step and short enough to stay on
   a 720 px stage. */
#define PL_SUB             256     /* fixed-point scale: PL_SUB units == 1 px        */
#define PL_SPEED             6     /* world px advanced per tick                     */
#define PL_GRAVITY         100     /* downward accel, sub-units / tick               */
#define PL_JUMP_IMPULSE   2400     /* upward launch velocity, sub-units (negative=up)*/
#define PL_VY_TERM        3000     /* terminal downward velocity clamp (no overflow) */
#define PL_RUN_PHASES        8     /* run-cycle length (step % this) - nonzero const */
#define PL_HINT_TTL         72     /* ticks the how-to-play hint takes to fade out    */

/* -- terrain generator (constructively gap-free and always on screen) ---------
   The grass line lives in the lower third of the design space: everything below
   it is underground, and underground the player cannot use is wasted screen. */
#define PL_SEG_MIN_W        72
#define PL_SEG_MAX_W       220
#define PL_GROUND_HI       440     /* highest terrain top = smallest y               */
#define PL_GROUND_LO       560     /* lowest  terrain top = largest  y               */
#define PL_MAX_STEP_DY      32     /* |adjacent topY delta| (< jump peak, reads as a step) */
#define PL_LEVEL_LEN     16000     /* the flat tail reaches here (clamp covers beyond)*/

/* -- hero + coin geometry (px) ------------------------------------------------- */
#define PL_HERO_W           26
#define PL_HERO_H           40
#define PL_COIN_R           11     /* radius; 2*R + hero width >= 2*SPEED, so no coin is tunnelled */
#define PL_COIN_LIFT_LO     18     /* low coins: collected just by running under (auto-score) */
#define PL_COIN_LIFT_HI     72     /* high coins: reachable only by a jump (rewards the JUMP)  */

/* One terrain span: [startX, endX) at screen-space top `topY` (all px). */
typedef struct { int32_t startX, endX, topY; } PlSegment;

/* A collectible coin: world x + screen-space centre y, one-shot collected latch. */
typedef struct { int32_t x, y; bool collected; } PlCoin;

/* A parallax / decor rectangle: world x + on-screen size + tint (0xRRGGBB). */
typedef struct { int32_t x, w, h; uint32_t tint; } PlDecor;

/* A dust particle: fixed-point pos/vel + integer time-to-live (0 == inactive). */
typedef struct { int32_t x256, y256, vx256, vy256, ttl; } PlParticle;

typedef struct {
    /* hero + camera (the only per-tick mutating physics) */
    int32_t  heroFoot256;             /* ABSOLUTE screen-space foot y (down+, >= 0)  */
    int32_t  vy256;                   /* vertical velocity (negative == upward)      */
    bool     grounded;                /* on the terrain this tick                    */
    int32_t  scrollX;                 /* world px scrolled (monotonic, >= 0)         */
    uint32_t step;                    /* fixed-tick counter (drives run cycle)       */
    int32_t  score;                   /* coins collected                             */
    int32_t  best;                    /* session best (in-memory only, never saved)  */
    uint32_t partHead;                /* dust ring-buffer write cursor               */
    uint32_t jumps;                   /* jumps taken; the hint fades after the first */
    int32_t  hintTtl;                 /* < 0 before the first jump, then counts to 0 */

    /* the immutable seeded level */
    PlSegment  seg[PL_MAX_SEGMENTS];   int segCount;
    PlCoin     coin[PL_MAX_COINS];     int coinCount;
    PlDecor    hill[PL_MAX_HILLS];     int hillCount;   /* far parallax   */
    PlDecor    bldg[PL_MAX_BUILDINGS]; int bldgCount;   /* mid parallax   */
    PlDecor    tuft[PL_MAX_TUFTS];     int tuftCount;   /* ground-plane   */
    /* Two sky bands rather than one: a single drift rate reads as a flat backdrop
       sliding past, so the stars crawl at 1/8 and the moon at 1/16. */
    PlDecor    star[PL_MAX_STARS];     int starCount;
    PlDecor    moon;
    PlParticle part[PL_MAX_PARTICLES];

    /* precomputed fixed-width HUD strings (rcFormat-free frozen core) */
    char scoreStr[6];                 /* "000"                                       */
    char distStr[8];                  /* "0000 m"                                    */
    char bestStr[6];                  /* "000"                                       */

    uint32_t rng;                     /* xorshift state - SEED-TIME ONLY             */
} PlWorld;

/* World x under the hero this frame (camera + fixed screen x). */
static inline int32_t pl_hero_world_x(const PlWorld *w) { return w->scrollX + PL_HERO_SCREEN_X; }

/* Ground-plane screen x for a world-x object (terrain, coins, tufts, hero). */
static inline int32_t pl_ground_x(const PlWorld *w, int32_t worldX) { return worldX - w->scrollX; }

/* Parallax screen x: worldX - scrollX*num/den. A den of 1 tracks the ground, a
   larger one drifts slower (distance), a numerator over den runs FASTER than the
   ground, which is what a foreground band does. */
static inline int32_t pl_parallax_x(int32_t worldX, int32_t scrollX, int32_t num, int32_t den) {
    return worldX - (int32_t)((int64_t)scrollX * num / den);
}

/* Cull on the sprite's EXTENT, not its anchor: visible iff the box overlaps
   [-pad, stageW+pad]. Callers pass a pad at least as wide as the sprite drawn. */
static inline bool pl_visible(int32_t screenX, int32_t w, int32_t pad, int32_t stageW) {
    return (screenX + w > -pad) && (screenX < stageW + pad);
}

#ifdef PLATFORMER_BACKEND_IMPLEMENTATION

#include <string.h>

/* Lifecycle + per-tick + the terrain query - forward-declared here (impl-only). */
PLDEF void    pl_memzero(void *p, size_t n);
PLDEF void    pl_world_seed(PlWorld *w, unsigned seed);
PLDEF void    pl_world_step(PlWorld *w);
PLDEF void    pl_world_jump(PlWorld *w);
PLDEF int32_t pl_ground_top(const PlWorld *w, int32_t worldX);

PLDEF void pl_memzero(void *p, size_t n) { memset(p, 0, n); }

/* xorshift32 - deterministic, used ONLY at seed time (never on a measured frame). */
static uint32_t pl__rng(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return (*state = x);
}

/* An inclusive-range random in [lo, hi], seed-time only. */
static int32_t pl__rng_range(uint32_t *state, int32_t lo, int32_t hi) {
    if (hi <= lo)
        return lo;
    return lo + (int32_t)(pl__rng(state) % (uint32_t)(hi - lo + 1));
}

/* -- manual, locale-free formatters (mirrors the trader backend) --------------- */

/* Unsigned int -> decimal, no padding: a score reads "7", not "007". */
static void pl__uint_str(uint32_t v, char *out, int cap) {
    char tmp[12];
    int n = 0;
    do { tmp[n++] = (char)('0' + v % 10u); v /= 10u; } while (v && n < 11);
    if (n > cap - 1)
        n = cap - 1;
    for (int i = 0; i < n; i++)
        out[i] = tmp[n - 1 - i];
    out[n] = '\0';
}

/* Refresh the three HUD strings from the integer sim state (called on the tick). */
static void pl__fmt_hud(PlWorld *w) {
    uint32_t coins = (uint32_t)(w->score < 0 ? 0 : w->score);
    uint32_t best  = (uint32_t)(w->best  < 0 ? 0 : w->best);
    uint32_t dist  = (uint32_t)(w->scrollX < 0 ? 0 : w->scrollX) / 10u;   /* px -> "metres" */
    if (dist > 99999u)                    /* saturate the odometer (no mod-100000 wrap) */
        dist = 99999u;
    pl__uint_str(coins, w->scoreStr, (int)sizeof w->scoreStr);
    pl__uint_str(best,  w->bestStr,  (int)sizeof w->bestStr);
    pl__uint_str(dist,  w->distStr,  (int)sizeof w->distStr - 2);         /* leave room for " m" */
    {
        int k = 0;
        while (w->distStr[k] && k < (int)sizeof w->distStr - 3)
            k++;
        w->distStr[k++] = ' ';
        w->distStr[k++] = 'm';
        w->distStr[k]   = '\0';
    }
}

/* Terrain lookup, valid for ANY worldX: segments are contiguous with a long flat
   tail, and past the tail the query clamps to the last segment. */
PLDEF int32_t pl_ground_top(const PlWorld *w, int32_t worldX) {
    int last = w->segCount - 1;
    if (last < 0)
        return PL_GROUND_LO;                       /* empty-state safety (never in practice) */
    for (int i = 0; i <= last; i++) {
        if (worldX < w->seg[i].endX)
            return w->seg[i].topY;
    }
    return w->seg[last].topY;                       /* flat run-out on the last segment */
}

/* Build the contiguous, gap-free terrain: a bounded random-walk of tops, each span
   PL_SEG_MIN_W..PL_SEG_MAX_W wide, with a long flat starter and a long flat tail. */
static void pl__gen_terrain(PlWorld *w) {
    int32_t x   = 0;
    int32_t top = (PL_GROUND_HI + PL_GROUND_LO) / 2;
    int n = PL_MAX_SEGMENTS;
    for (int i = 0; i < n; i++) {
        int32_t width;
        if (i == 0)
            width = 360;                            /* a flat runway to spawn + settle on */
        else if (i == n - 1)
            width = PL_LEVEL_LEN - x;               /* the flat tail reaches PL_LEVEL_LEN  */
        else
            width = pl__rng_range(&w->rng, PL_SEG_MIN_W, PL_SEG_MAX_W);
        if (width < 1)
            width = 1;
        w->seg[i].startX = x;
        w->seg[i].endX   = x + width;
        w->seg[i].topY   = top;
        x += width;
        /* walk the NEXT top by a bounded step, clamped into the ground band */
        int32_t dy = pl__rng_range(&w->rng, -PL_MAX_STEP_DY, PL_MAX_STEP_DY);
        top += dy;
        if (top < PL_GROUND_HI) top = PL_GROUND_HI;
        if (top > PL_GROUND_LO) top = PL_GROUND_LO;
    }
    w->segCount = n;
}

/* Scatter coins above the terrain, every third one high enough to need a jump. */
static void pl__gen_coins(PlWorld *w) {
    int n = PL_MAX_COINS;
    int32_t x = 520;
    for (int i = 0; i < n; i++) {
        int32_t gap  = pl__rng_range(&w->rng, 180, 340);
        int32_t lift = (i % 3 == 0) ? PL_COIN_LIFT_HI : PL_COIN_LIFT_LO;   /* every 3rd needs a jump */
        x += gap;
        w->coin[i].x         = x;
        w->coin[i].y         = pl_ground_top(w, x) - lift;
        w->coin[i].collected = false;
    }
    w->coinCount = n;
}

/* Far/mid parallax decor + foreground tufts: fixed worldX, bounded sizes/tints. */
/* Pale, and only just: a star that is white reads as a dead pixel on this sky. */
static const uint32_t STAR_TINTS[4] = { 0xbfc8ea, 0x9aa6d8, 0xe6e9fa, 0x8f9ccb };

static void pl__gen_decor(PlWorld *w) {
    /* The far band is a purple close to the sky at that height, so it RECEDES;
       the city below it is navy and the ground brown. Three layers, three
       distances, and the eye reads them in that order. */
    static const uint32_t HILL_TINTS[] = { 0x2a2452, 0x30295c, 0x372f68 };
    static const uint32_t BLDG_TINTS[] = { 0x1f2749, 0x252d54, 0x2b3560, 0x30386a };
    int32_t x;

    x = 0;
    for (int i = 0; i < PL_MAX_HILLS; i++) {
        w->hill[i].w    = pl__rng_range(&w->rng, 340, 560);
        w->hill[i].h    = pl__rng_range(&w->rng, 260, 400);   /* clear of the skyline */
        w->hill[i].x    = x;
        w->hill[i].tint = HILL_TINTS[i % (int)(sizeof HILL_TINTS / sizeof *HILL_TINTS)];
        x += pl__rng_range(&w->rng, 340, 520);
    }
    w->hillCount = PL_MAX_HILLS;

    x = 60;
    for (int i = 0; i < PL_MAX_BUILDINGS; i++) {
        w->bldg[i].w    = pl__rng_range(&w->rng, 60, 150);
        w->bldg[i].h    = pl__rng_range(&w->rng, 120, 340);
        w->bldg[i].x    = x;
        w->bldg[i].tint = BLDG_TINTS[i % (int)(sizeof BLDG_TINTS / sizeof *BLDG_TINTS)];
        x += pl__rng_range(&w->rng, 110, 240);   /* sky between them, not a wall */
    }
    w->bldgCount = PL_MAX_BUILDINGS;

    x = 40;
    for (int i = 0; i < PL_MAX_TUFTS; i++) {
        w->tuft[i].w    = pl__rng_range(&w->rng, 14, 30);
        w->tuft[i].h    = pl__rng_range(&w->rng, 8, 18);
        w->tuft[i].x    = x;
        w->tuft[i].tint = 0x2f7d4f;
        x += pl__rng_range(&w->rng, 120, 260);
    }
    w->tuftCount = PL_MAX_TUFTS;

    /* Stars drift at 1/8, so a 2600 px span of them covers the whole course. Their
       y stops well above PL_GROUND_HI, so no star can land inside the terrain's
       range whatever the heightfield did. */
    x = 0;
    for (int i = 0; i < PL_MAX_STARS; i++) {
        w->star[i].w    = pl__rng_range(&w->rng, 2, 5);
        w->star[i].h    = w->star[i].w;
        w->star[i].x    = x;
        w->star[i].tint = STAR_TINTS[i % (int)(sizeof STAR_TINTS / sizeof *STAR_TINTS)];
        x += pl__rng_range(&w->rng, 28, 66);
    }
    w->starCount = PL_MAX_STARS;
    /* One moon, placed rather than rolled: the single brightest thing in the sky is
       a composition decision, and a seed that sometimes puts it behind a hill is
       not a nicer picture, only a less predictable one. */
    w->moon.x    = 760;
    w->moon.w    = 72;
    w->moon.h    = 72;
    w->moon.tint = 0xe8e4f4;
}

/* Scatter one index over [0, span) with a standard integer-hash finaliser. The
   xor-shifts matter: a bare multiply leaves the low bits marching in step with
   the index, and decor whose x also marches then reads as a diagonal line. */
static inline uint32_t pl_scatter(uint32_t i, uint32_t salt, uint32_t span) {
    uint32_t h = i + salt;
    h ^= h >> 15; h *= 2246822519u;
    h ^= h >> 13; h *= 3266489917u;
    h ^= h >> 16;
    return span ? h % span : 0u;
}

/* A star's VERTICAL place, derived from its index rather than stored, so the sky
   costs the same memory whatever its depth. */
static inline int32_t pl_star_y(int i) {
    return 14 + (int32_t)pl_scatter((uint32_t)i, 0x9E3779B9u, (uint32_t)(PL_GROUND_HI - 150));
}

PLDEF void pl_world_seed(PlWorld *w, unsigned seed) {
    pl_memzero(w, sizeof *w);
    w->rng = seed ? seed : 0x9E3779B9u;             /* never a zero xorshift state */
    pl__gen_terrain(w);
    pl__gen_coins(w);
    pl__gen_decor(w);
    /* place the hero standing on segment 0, at rest (the memset left him falling) */
    w->grounded    = true;
    w->vy256       = 0;
    w->heroFoot256 = pl_ground_top(w, pl_hero_world_x(w)) * PL_SUB;
    w->score       = 0;
    w->best        = 0;
    w->hintTtl     = -1;                            /* not started; the hint is solid */
    pl__fmt_hud(w);
}

PLDEF void pl_world_jump(PlWorld *w) {
    if (w->grounded) {                              /* no double-jump; idempotent while airborne */
        w->grounded = false;
        w->vy256    = -PL_JUMP_IMPULSE;
        w->jumps++;
        if (w->jumps == 1u)
            w->hintTtl = PL_HINT_TTL;               /* the hint has been read: fade it */
    }
}

/* Advance the dust ring: age all live particles, emit one at the hero's feet on a
   fixed cadence while grounded. Pure function of the count-driven step. */
static void pl__particles_step(PlWorld *w) {
    for (int i = 0; i < PL_MAX_PARTICLES; i++) {
        if (w->part[i].ttl > 0) {
            w->part[i].x256 += w->part[i].vx256;
            w->part[i].y256 += w->part[i].vy256;
            w->part[i].ttl--;
        }
    }
    if (w->grounded && (w->step % 5u) == 0u) {
        PlParticle *p = &w->part[w->partHead % PL_MAX_PARTICLES];
        int32_t footY = w->heroFoot256 >> 8;
        p->x256  = (PL_HERO_SCREEN_X - PL_HERO_W / 2) * PL_SUB;
        p->y256  = (footY - 4) * PL_SUB;
        p->vx256 = -(PL_SPEED * PL_SUB) - 40;       /* drift back faster than the scroll */
        p->vy256 = -70;                             /* gentle rise                       */
        p->ttl   = 18;
        w->partHead++;
    }
}

/* Collect any coin the hero overlaps (one-shot latch, dt>0 tick only, tunnel-proof
   by the PL_COIN_R + PL_HERO_W >= 2*PL_SPEED sizing). */
static void pl__collect(PlWorld *w) {
    int32_t hx = pl_hero_world_x(w);
    int32_t footY = w->heroFoot256 >> 8;
    int32_t heroTop = footY - PL_HERO_H;
    for (int i = 0; i < w->coinCount; i++) {
        if (w->coin[i].collected)
            continue;
        int32_t dx = w->coin[i].x - hx;
        if (dx < 0) dx = -dx;
        int32_t cy = w->coin[i].y;
        bool overX = dx <= (PL_HERO_W / 2 + PL_COIN_R);
        bool overY = (cy + PL_COIN_R >= heroTop) && (cy - PL_COIN_R <= footY);
        if (overX && overY) {
            w->coin[i].collected = true;
            w->score++;
        }
    }
}

PLDEF void pl_world_step(PlWorld *w) {
    w->step++;
    w->scrollX += PL_SPEED;
    if (w->hintTtl > 0)
        w->hintTtl--;

    int32_t gtop256 = pl_ground_top(w, pl_hero_world_x(w)) * PL_SUB;
    if (w->grounded) {
        w->heroFoot256 = gtop256;                   /* Dino-hug: snap to the terrain top */
        w->vy256       = 0;
    } else {
        w->vy256 += PL_GRAVITY;
        if (w->vy256 > PL_VY_TERM)
            w->vy256 = PL_VY_TERM;                  /* terminal clamp: no unbounded growth */
        w->heroFoot256 += w->vy256;
        if (w->vy256 >= 0 && w->heroFoot256 >= gtop256) {   /* <= cross-land + snap-clamp */
            w->heroFoot256 = gtop256;
            w->vy256       = 0;
            w->grounded    = true;
        }
    }

    pl__collect(w);
    pl__particles_step(w);
    if (w->score > w->best)
        w->best = w->score;
    pl__fmt_hud(w);
}

#endif /* PLATFORMER_BACKEND_IMPLEMENTATION */
#endif /* PLATFORMER_BACKEND_H */
