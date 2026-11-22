/*
    platformer_backend.h - the rhythm runner's model and rules, no RayClay at all.

    The hero runs right at a fixed speed while the player decides only when to
    jump. A course is a grid of cells compiled from the ASCII bars in
    platformer_levels.h, and one beat is four cells of travel, so the course, the
    pulse of the background and the waveform in the titlebar share one clock: how
    far the world has scrolled.

    The whole simulation is integer or 1/256-px fixed-point, so a run replays
    exactly on every machine, and pl_world_step advances one tick per call; the
    caller runs it PL_TICK_HZ times a second off its own clock. Every number the
    HUD shows is formatted here into a fixed buffer on that tick.

    Single-header: define PLATFORMER_BACKEND_IMPLEMENTATION in exactly one TU
    before including it.
*/
#ifndef PLATFORMER_BACKEND_H
#define PLATFORMER_BACKEND_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef PLDEF
#define PLDEF static
#endif

/* The world's own units. One Stage px per world px at scale 1. */
#define PL_DESIGN_H       676      /* the Stage height at scale 1                     */
#define PL_GROUND_H       128      /* the band under the ground line, at scale 1      */
#define PL_HERO_X         220      /* how far the hero runs ahead of scrollX          */
#define PL_CELL            48
#define PL_ROWS             6
#define PL_BAR_CELLS       16
#define PL_BEAT_CELLS       4
#define PL_BEATS_PER_BAR    4
#define PL_MAX_BARS        40
#define PL_MAX_COLS       (PL_MAX_BARS * PL_BAR_CELLS)
#define PL_LEVEL_COUNT      3
#define PL_MAX_PARTICLES  128
#define PL_WAVE_BARS      180      /* samples in a course's waveform                  */

/* Physics, in 1/256 px per tick. The set is chosen together: at PL_SPEED_NORMAL one
   jump takes exactly 32 ticks, which is one beat and four cells, and peaks at 108 px,
   which clears two cells. A pad launches 195 px high for 43 ticks. */
#define PL_SUB            256
#define PL_TICK_HZ         60
#define PL_SPEED_NORMAL     6      /* px per tick: 112.5 beats per minute             */
#define PL_SPEED_FAST       8      /* px per tick: 150 beats per minute               */
#define PL_GRAVITY        216
#define PL_JUMP_V        3564
#define PL_PAD_V         4752
#define PL_VY_TERM       4800      /* fastest fall; a cell cannot be tunneled through */

/* The hit box is smaller than the 44 px hero on purpose, and a landing forgives a
   few px: a player who feels they cleared it should have cleared it. */
#define PL_HERO_SIZE       44
#define PL_HERO_HALF       16
#define PL_HERO_FEET       20
#define PL_HERO_TALL       38
#define PL_SNAP            12
#define PL_SPIKE_INSET     16
#define PL_SPIKE_TALL      22
#define PL_PAD_TALL        14
#define PL_PRESS_BUFFER     5      /* ticks a press waits for the next landing        */

#define PL_DEATH_TICKS     62      /* the death stamp holds, then the next attempt    */
#define PL_DEATH_SKIP      18      /* from here a press restarts at once              */
#define PL_WIN_FLY_TICKS   24      /* the hero flies through the gate, then bursts    */
#define PL_WIN_TICKS       54      /* the win's picture holds, then the panel         */
#define PL_CHECKPOINT_BARS  2      /* practice mode marks the ground at most this often */
#define PL_CHECKPOINT_CALM 60      /* ticks a respawn must have before its first jump */

typedef enum {
    PL_CELL_EMPTY = 0, PL_CELL_BLOCK, PL_CELL_SPIKE, PL_CELL_SPIKE_DOWN, PL_CELL_ORB,
    PL_CELL_PAD, PL_CELL_COIN
} PlCellKind;
#define PL_CELL_USED 0x80          /* set on a taken coin or a fired orb; drawn apart */

typedef enum { PL_RUN = 0, PL_DEAD, PL_WON } PlPhase;

typedef enum { PL_PART_DUST = 0, PL_PART_TRAIL, PL_PART_BURST, PL_PART_SPARK, PL_PART_CONFETTI }
    PlPartKind;

/* One level's colors. Every field is 0xRRGGBB; a course fades between two of these. */
typedef struct {
    uint32_t skyTop, skyBot, tile, pillar, groundTop, groundBot, line, accent;
} PlTheme;

/* One bar of a course, top row first: see platformer_levels.h. */
typedef struct { const char *row[PL_ROWS]; } PlBar;

typedef struct {
    const char *name, *blurb, *difficulty;
    int stars, speed;
    const PlBar *const *bars;
    int barCount;
    PlTheme theme[2];
} PlLevelInfo;

/* A particle, in world px and height above the ground, so it stays where it was
   thrown while the camera moves on. */
typedef struct {
    int32_t x256, y256, vx256, vy256;
    int16_t ttl, life;
    uint8_t kind, size, tone;
} PlParticle;

/* The part of a run the bot can rehearse on a copy. */
typedef struct {
    int32_t scrollX, y256, vy256;
    bool    grounded;
} PlBody;

typedef struct {
    /* The hero and the camera. */
    int32_t  scrollX;                   /* world px scrolled                          */
    int32_t  y256;                      /* feet above the ground; up is positive      */
    int32_t  vy256;
    bool     grounded;
    int32_t  spin;                      /* the hero's turn, in 1/1024 of a full turn  */
    PlPhase  phase;
    int32_t  phaseTicks;
    uint32_t step;                      /* ticks this attempt; the run's own clock    */
    uint32_t clock;                     /* ticks since the course loaded; never stops */
    int      pressBuffer;

    /* This attempt. */
    int32_t  score;                     /* spikes cleared, orbs, pads and coins       */
    uint32_t jumps;                     /* this attempt                               */
    int32_t  attempt;
    uint32_t totalTicks;                /* across attempts, for the win panel         */
    uint8_t  coins;                     /* taken this attempt                         */
    uint8_t  coinTotal;                 /* on the whole course                        */
    int32_t  scoredCol;                 /* spikes behind this column have been scored */
    int32_t  startX;                    /* where this attempt began                   */

    /* Practice mode. */
    bool     practice;
    bool     hasCheckpoint;
    PlBody   checkpoint;
    int32_t  checkpointBar;

    /* The course. */
    uint8_t  level;
    int32_t  speed;
    int32_t  cols;
    int32_t  goalX;                     /* the gate; reaching it wins                 */
    uint8_t  cell[PL_MAX_COLS][PL_ROWS];
    uint8_t  wave[PL_WAVE_BARS];        /* 0-255: how busy the course is there        */

    PlParticle part[PL_MAX_PARTICLES];
    uint32_t partHead;

    /* Fixed-width HUD strings, refreshed on the tick. */
    char scoreStr[12];
    char pctStr[8];
    char coinStr[12];
    char attemptStr[24];
    char timeStr[12];
} PlWorld;

/* The fraction of the course behind the hero, 0 to 1. */
static inline float pl_progress(const PlWorld *w) {
    float p = (float)(w->scrollX + PL_HERO_X) / (float)w->goalX;
    return p < 0.0f ? 0.0f : p > 1.0f ? 1.0f : p;
}

static inline int pl_percent(const PlWorld *w) {
    return (int)(pl_progress(w) * 100.0f);
}

/* Where this tick falls inside the beat: 0 on the beat, approaching 1 just before
   the next one. A course is scored in cells of travel, so this is its metronome. */
static inline float pl_beat_phase(const PlWorld *w) {
    const int32_t beat = PL_BEAT_CELLS * PL_CELL;
    return (float)(w->scrollX % beat) / (float)beat;
}

/* The light of a beat: bright at the attack, gone well before the next one. */
static inline float pl_pulse(const PlWorld *w) {
    const float fall = 1.0f - pl_beat_phase(w);
    return fall * fall * fall;
}

/* Scatter one index over [0, span) with a standard integer-hash finalizer. Decor
   derives its place from its index, so a skyline as wide as the course costs no
   memory. The xor-shifts matter: a bare multiply leaves low bits marching with the
   index, and decor whose x also marches reads as a diagonal line. */
static inline uint32_t pl_scatter(uint32_t i, uint32_t salt, uint32_t span) {
    uint32_t h = i + salt;
    h ^= h >> 15; h *= 2246822519u;
    h ^= h >> 13; h *= 3266489917u;
    h ^= h >> 16;
    return span ? h % span : 0u;
}

#ifdef PLATFORMER_BACKEND_IMPLEMENTATION

#include <string.h>
#include <math.h>
#include "platformer_levels.h"

/* Sine and cosine of a turn in 1/1024 of a circle, so the GUI file draws a rotated
   hero without a system header of its own. */
static inline float pl_sin(int32_t turn) {
    return sinf((float)turn * 6.2831853f / 1024.0f);
}

static inline float pl_cos(int32_t turn) {
    return cosf((float)turn * 6.2831853f / 1024.0f);
}

/* -- formatting ---------------------------------------------------------------- */

/* Unsigned int to decimal, no padding, no locale. */
static int pl_uint_str(uint32_t v, char *out, int cap) {
    char tmp[12];
    int n = 0;
    do { tmp[n++] = (char)('0' + v % 10u); v /= 10u; } while (v && n < 11);
    if (n > cap - 1)
        n = cap - 1;
    for (int i = 0; i < n; i++)
        out[i] = tmp[n - 1 - i];
    out[n] = '\0';
    return n;
}

/* Prefix, number, suffix; the shape of every HUD string. */
PLDEF void pl_fmt(char *out, int cap, const char *prefix, uint32_t v, const char *suffix) {
    int n = 0;
    while (*prefix && n < cap - 1)
        out[n++] = *prefix++;
    n += pl_uint_str(v, out + n, cap - n);
    while (*suffix && n < cap - 1)
        out[n++] = *suffix++;
    out[n] = '\0';
}

/* "1:04", for a time in ticks. */
PLDEF void pl_fmt_time(char *out, int cap, uint32_t ticks) {
    const uint32_t seconds = ticks / PL_TICK_HZ;
    pl_fmt(out, cap, "", seconds / 60u, seconds % 60u < 10u ? ":0" : ":");
    {
        int n = (int)strlen(out);
        pl_uint_str(seconds % 60u, out + n, cap - n);
    }
}

/* "12 / 74": how a count of coins reads everywhere it is shown. */
PLDEF void pl_fmt_coins(char *out, int cap, uint32_t have, uint32_t of) {
    char tail[8];

    pl_fmt(tail, (int)sizeof tail, " / ", of, "");
    pl_fmt(out, cap, "", have, tail);
}

static void pl_fmt_hud(PlWorld *w) {
    pl_fmt(w->scoreStr, (int)sizeof w->scoreStr, "", (uint32_t)w->score, "");
    pl_fmt(w->pctStr, (int)sizeof w->pctStr, "", (uint32_t)pl_percent(w), "%");
    pl_fmt_coins(w->coinStr, (int)sizeof w->coinStr, w->coins, w->coinTotal);
    pl_fmt(w->attemptStr, (int)sizeof w->attemptStr, "Attempt ", (uint32_t)w->attempt, "");
    pl_fmt_time(w->timeStr, (int)sizeof w->timeStr, w->totalTicks);
}

/* -- the course ---------------------------------------------------------------- */

static PlCellKind pl_cell_kind(char c) {
    switch (c) {
    case '#': return PL_CELL_BLOCK;
    case '^': return PL_CELL_SPIKE;
    case 'v': return PL_CELL_SPIKE_DOWN;
    case 'o': return PL_CELL_ORB;
    case 'p': return PL_CELL_PAD;
    case '$': return PL_CELL_COIN;
    default:  return PL_CELL_EMPTY;
    }
}

static inline uint8_t pl_cell_at(const PlWorld *w, int32_t col, int32_t row) {
    if (col < 0 || col >= w->cols || row < 0 || row >= PL_ROWS)
        return PL_CELL_EMPTY;
    return w->cell[col][row];
}

/* A course's waveform is how much happens there, with the beat's own texture over
   it, so each course draws a shape of its own and quiet stretches read as quiet. */
static void pl_build_wave(PlWorld *w) {
    for (int i = 0; i < PL_WAVE_BARS; i++) {
        const int32_t mid = (int32_t)i * w->cols / PL_WAVE_BARS;
        int busy = 0;
        int v;

        for (int32_t col = mid - 6; col <= mid + 6; col++) {
            for (int32_t row = 0; row < PL_ROWS; row++)
                busy += pl_cell_at(w, col, row) != PL_CELL_EMPTY;
        }
        if (busy > 20)
            busy = 20;
        v = 46 + busy * 8 + (int)pl_scatter((uint32_t)i, 0x51ED270Bu, 44u);
        if (i % 4 == 0)
            v += 24;
        w->wave[i] = (uint8_t)(v > 255 ? 255 : v);
    }
}

/* Forget everything the last attempt did to the course. */
static void pl_clear_used(PlWorld *w) {
    for (int32_t col = 0; col < w->cols; col++) {
        for (int32_t row = 0; row < PL_ROWS; row++)
            w->cell[col][row] &= (uint8_t)~PL_CELL_USED;
    }
}

static void pl_place_hero(PlWorld *w, const PlBody *b) {
    w->scrollX     = b->scrollX;
    w->y256        = b->y256;
    w->vy256       = b->vy256;
    w->grounded    = b->grounded;
    w->spin        = 0;
    w->phase       = PL_RUN;
    w->phaseTicks  = 0;
    w->pressBuffer = 0;
    w->startX      = b->scrollX;
    w->scoredCol   = (b->scrollX + PL_HERO_X) / PL_CELL;
}

/* A fresh attempt from the start of the course, or in practice mode from the last
   checkpoint. Clear hasCheckpoint first to send a practice run back to the start.
   The attempt counter belongs to the caller of pl_world_load: it is the one number
   a restart must not reset. */
PLDEF void pl_world_restart(PlWorld *w) {
    const PlBody start = { 0, 0, 0, true };

    if (w->practice && w->hasCheckpoint) {
        pl_place_hero(w, &w->checkpoint);
    } else {
        pl_place_hero(w, &start);
        pl_clear_used(w);
        w->score = 0;
        w->coins = 0;
        w->checkpointBar = 0;
    }
    w->jumps = 0;
    w->step  = 0;
    w->attempt++;
    pl_fmt_hud(w);
}

/* How many coins a course's bars hold. */
PLDEF int pl_course_coins(int level) {
    const PlLevelInfo *info = &PL_LEVELS[level];
    int count = 0;

    for (int b = 0; b < info->barCount; b++) {
        for (int r = 0; r < PL_ROWS; r++) {
            for (int c = 0; c < PL_BAR_CELLS; c++)
                count += info->bars[b]->row[r][c] == '$';
        }
    }
    return count;
}

/* Compile a course from its bars and stand the hero at the start of attempt 1. */
PLDEF void pl_world_load(PlWorld *w, int level, bool practice) {
    const PlLevelInfo *info = &PL_LEVELS[level];

    memset(w, 0, sizeof *w);
    w->level    = (uint8_t)level;
    w->speed    = info->speed;
    w->practice = practice;
    w->cols     = info->barCount * PL_BAR_CELLS;
    w->goalX    = (w->cols - PL_BAR_CELLS / 2) * PL_CELL;
    w->coinTotal = (uint8_t)pl_course_coins(level);
    for (int b = 0; b < info->barCount; b++) {
        for (int r = 0; r < PL_ROWS; r++) {
            const char *line = info->bars[b]->row[r];
            for (int c = 0; c < PL_BAR_CELLS; c++) {
                w->cell[b * PL_BAR_CELLS + c][PL_ROWS - 1 - r] = (uint8_t)pl_cell_kind(line[c]);
            }
        }
    }
    pl_build_wave(w);
    pl_world_restart(w);
}

/* -- one tick of physics ------------------------------------------------------- */

typedef enum {
    PL_EV_DEAD = 1, PL_EV_JUMP = 2, PL_EV_LAND = 4, PL_EV_ORB = 8, PL_EV_PAD = 16
} PlEvent;

/* True when the hero's box overlaps a live cell of this kind. */
static bool pl_touching(const PlWorld *w, const PlBody *b, PlCellKind kind) {
    const int32_t hx = b->scrollX + PL_HERO_X;
    const int32_t y  = b->y256 / PL_SUB;

    for (int32_t col = (hx - PL_HERO_FEET) / PL_CELL; col <= (hx + PL_HERO_FEET) / PL_CELL; col++) {
        for (int32_t row = y / PL_CELL; row <= (y + PL_HERO_TALL) / PL_CELL; row++) {
            if (pl_cell_at(w, col, row) == kind)
                return true;
        }
    }
    return false;
}

/* True when a block or the ground sits exactly under the hero's feet. */
static bool pl_has_support(const PlWorld *w, const PlBody *b) {
    const int32_t hx = b->scrollX + PL_HERO_X;
    const int32_t y  = b->y256 / PL_SUB;

    if (y <= 0)
        return true;
    if (y % PL_CELL != 0)
        return false;
    for (int32_t col = (hx - PL_HERO_FEET) / PL_CELL; col <= (hx + PL_HERO_FEET) / PL_CELL; col++) {
        if (pl_cell_at(w, col, y / PL_CELL - 1) == PL_CELL_BLOCK)
            return true;
    }
    return false;
}

/* The rules, as one function over a body, so the real hero and the bot's rehearsals
   cannot disagree. hold jumps whenever the hero is on something; press also fires
   an orb in midair. Returns the PlEvent bits of what happened. */
static unsigned pl_advance(const PlWorld *w, PlBody *b, bool hold, bool press) {
    unsigned ev = 0;

    b->scrollX += w->speed;
    if (b->grounded && !pl_has_support(w, b))
        b->grounded = false;
    if (b->grounded && (hold || press)) {
        b->vy256    = PL_JUMP_V;
        b->grounded = false;
        ev |= PL_EV_JUMP;
    } else if (!b->grounded && press && pl_touching(w, b, PL_CELL_ORB)) {
        b->vy256 = PL_JUMP_V;
        ev |= PL_EV_ORB;
    }
    if (!b->grounded) {
        b->vy256 -= PL_GRAVITY;
        if (b->vy256 < -PL_VY_TERM)
            b->vy256 = -PL_VY_TERM;
        b->y256 += b->vy256;
    }

    {
        const int32_t hx   = b->scrollX + PL_HERO_X;
        const int32_t col0 = (hx - PL_HERO_FEET) / PL_CELL;
        const int32_t col1 = (hx + PL_HERO_FEET) / PL_CELL;

        for (int32_t col = col0; col <= col1 && !(ev & PL_EV_DEAD); col++) {
            const int32_t left = col * PL_CELL;
            const bool over_wall = hx + PL_HERO_HALF > left && hx - PL_HERO_HALF < left + PL_CELL;
            const bool over_feet = hx + PL_HERO_FEET > left && hx - PL_HERO_FEET < left + PL_CELL;
            const bool over_spike = hx + PL_HERO_HALF > left + PL_SPIKE_INSET
                                 && hx - PL_HERO_HALF < left + PL_CELL - PL_SPIKE_INSET;

            for (int32_t row = 0; row < PL_ROWS; row++) {
                const int32_t floor256 = row * PL_CELL * PL_SUB;
                const int32_t top256   = floor256 + PL_CELL * PL_SUB;
                const int32_t head256  = b->y256 + PL_HERO_TALL * PL_SUB;

                switch (pl_cell_at(w, col, row)) {
                case PL_CELL_BLOCK:
                    if (!over_feet || b->y256 >= top256 || head256 <= floor256)
                        break;
                    if (b->vy256 <= 0 && top256 - b->y256 <= PL_SNAP * PL_SUB - b->vy256) {
                        ev |= b->grounded ? 0u : (unsigned)PL_EV_LAND;
                        b->y256     = top256;
                        b->vy256    = 0;
                        b->grounded = true;
                    } else if (b->vy256 > 0 && top256 - b->y256 <= PL_SNAP * PL_SUB) {
                        break;      /* rising past the lip: let it through */
                    } else if (over_wall) {
                        ev |= PL_EV_DEAD;
                    }
                    break;
                case PL_CELL_SPIKE:
                    if (over_spike && b->y256 < floor256 + PL_SPIKE_TALL * PL_SUB
                        && head256 > floor256)
                        ev |= PL_EV_DEAD;
                    break;
                case PL_CELL_SPIKE_DOWN:
                    if (over_spike && b->y256 < top256 && head256 > top256 - PL_SPIKE_TALL * PL_SUB)
                        ev |= PL_EV_DEAD;
                    break;
                case PL_CELL_PAD:
                    if (over_feet && b->vy256 <= 0 && b->y256 < floor256 + PL_PAD_TALL * PL_SUB
                        && head256 > floor256) {
                        b->vy256    = PL_PAD_V;
                        b->grounded = false;
                        ev |= PL_EV_PAD;
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }

    if (b->y256 <= 0 && b->vy256 <= 0) {
        ev |= b->grounded ? 0u : (unsigned)PL_EV_LAND;
        b->y256     = 0;
        b->vy256    = 0;
        b->grounded = true;
    }
    return ev;
}

/* Rehearse on a copy: true when this body gets through the next `ticks` of the
   course without another press. */
static bool pl_run_is_safe(const PlWorld *w, PlBody b, int ticks) {
    for (int t = 0; t < ticks; t++) {
        if (pl_advance(w, &b, false, false) & PL_EV_DEAD)
            return false;
    }
    return true;
}

/* -- particles ----------------------------------------------------------------- */

static void pl_emit(PlWorld *w, PlPartKind kind, int32_t x, int32_t y, int32_t vx, int32_t vy,
                    int ttl, int size, int tone) {
    PlParticle *p = &w->part[w->partHead++ % PL_MAX_PARTICLES];

    p->x256 = x * PL_SUB;
    p->y256 = y * PL_SUB;
    p->vx256 = vx;
    p->vy256 = vy;
    p->ttl = (int16_t)ttl;
    p->life = (int16_t)ttl;
    p->kind = (uint8_t)kind;
    p->size = (uint8_t)size;
    p->tone = (uint8_t)tone;
}

/* A ring of fragments flung from a point: the death burst and an orb's spark. */
static void pl_burst(PlWorld *w, PlPartKind kind, int32_t x, int32_t y, int count, int speed,
                     int ttl) {
    for (int i = 0; i < count; i++) {
        const uint32_t seed = w->clock * 31u + (uint32_t)i;
        const int32_t turn = (int32_t)((uint32_t)i * 1024u / (uint32_t)count)
                           + (int32_t)pl_scatter(seed, 0x2545F491u, 60u);
        const int32_t pace = speed / 2 + (int32_t)pl_scatter(seed, 0x85EBCA6Bu, (uint32_t)speed);
        pl_emit(w, kind, x, y, (int32_t)(pl_cos(turn) * (float)pace),
                 (int32_t)(pl_sin(turn) * (float)pace),
                 ttl / 2 + (int)pl_scatter(seed, 0x27D4EB2Fu, (uint32_t)ttl),
                 6 + (int)pl_scatter(seed, 0xC2B2AE35u, 10u), (int)pl_scatter(seed, 3u, 3u));
    }
}

static void pl_particles_step(PlWorld *w) {
    for (int i = 0; i < PL_MAX_PARTICLES; i++) {
        PlParticle *p = &w->part[i];

        if (p->ttl <= 0)
            continue;
        p->x256 += p->vx256;
        p->y256 += p->vy256;
        p->ttl--;
        if (p->kind == PL_PART_BURST || p->kind == PL_PART_CONFETTI)
            p->vy256 -= p->kind == PL_PART_BURST ? 40 : 14;
        if (p->y256 < 0) {
            p->y256 = 0;
            p->vy256 = -p->vy256 / 2;
        }
    }
}

/* -- one tick of a run --------------------------------------------------------- */

/* Score everything the hero has just passed, and take what it is touching. */
static void pl_collect(PlWorld *w, const PlBody *b, unsigned ev) {
    const int32_t hx   = b->scrollX + PL_HERO_X;
    const int32_t y    = b->y256 / PL_SUB;
    const int32_t col0 = (hx - PL_HERO_FEET) / PL_CELL;
    const int32_t col1 = (hx + PL_HERO_FEET) / PL_CELL;

    while (w->scoredCol < (hx - PL_HERO_HALF) / PL_CELL) {
        for (int32_t row = 0; row < PL_ROWS; row++) {
            const uint8_t kind = pl_cell_at(w, w->scoredCol, row);
            if (kind == PL_CELL_SPIKE || kind == PL_CELL_SPIKE_DOWN)
                w->score += 10;
        }
        w->scoredCol++;
    }
    for (int32_t col = col0; col <= col1; col++) {
        for (int32_t row = y / PL_CELL; row <= (y + PL_HERO_TALL) / PL_CELL; row++) {
            const uint8_t kind = pl_cell_at(w, col, row);
            const bool orb = kind == PL_CELL_ORB && (ev & PL_EV_ORB);

            if (kind != PL_CELL_COIN && !orb)
                continue;
            w->cell[col][row] |= PL_CELL_USED;
            pl_burst(w, PL_PART_SPARK, col * PL_CELL + PL_CELL / 2,
                     row * PL_CELL + PL_CELL / 2, 14, 700, 30);
            if (orb) {
                w->score += 25;
            } else {
                w->coins++;
                w->score += 100;
            }
        }
    }
    if (ev & PL_EV_PAD)
        w->score += 25;
}

/* The trail: dust at the back corner on the ground, a fading echo in the air. */
static void pl_emit_trail(PlWorld *w) {
    const int32_t hx = w->scrollX + PL_HERO_X;
    const int32_t y  = w->y256 / PL_SUB;

    if (w->grounded) {
        if (w->step % 3u == 0u) {
            const uint32_t seed = w->step;
            pl_emit(w, PL_PART_DUST, hx - PL_HERO_SIZE / 2, 3 + (int32_t)pl_scatter(seed, 7u, 6u),
                     -90 - (int32_t)pl_scatter(seed, 11u, 150u),
                     60 + (int32_t)pl_scatter(seed, 13u, 180u), 22, 5, 0);
        }
    } else if (w->step % 2u == 0u) {
        pl_emit(w, PL_PART_TRAIL, hx - 8, y + PL_HERO_SIZE / 2, 0, 0, 16, 26, 0);
    }
}

/* Advance a run by one tick. hold is the button held down; press is its edge. */
PLDEF void pl_world_step(PlWorld *w, bool hold, bool press) {
    w->clock++;
    if (press)
        w->pressBuffer = PL_PRESS_BUFFER;
    else if (w->pressBuffer > 0)
        w->pressBuffer--;

    if (w->phase == PL_RUN) {
        PlBody b = { w->scrollX, w->y256, w->vy256, w->grounded };
        const unsigned ev = pl_advance(w, &b, hold, w->pressBuffer > 0);

        w->scrollX  = b.scrollX;
        w->y256     = b.y256;
        w->vy256    = b.vy256;
        w->grounded = b.grounded;
        w->step++;
        w->totalTicks++;
        if (ev & (PL_EV_JUMP | PL_EV_ORB)) {
            w->pressBuffer = 0;
            w->jumps++;
        }
        /* The hero turns half a circle over a jump and settles on the nearest quarter turn
           when it lands, which is what makes the orb look like it is rolling. */
        if (!w->grounded) {
            w->spin += 16;
        } else {
            const int32_t settled = ((w->spin + 128) / 256) * 256;
            w->spin += (settled - w->spin) / 2;
            if (w->spin % 1024 == 0)
                w->spin = 0;
        }
        pl_collect(w, &b, ev);
        pl_emit_trail(w);
        if ((ev & PL_EV_LAND) && (w->step > 4u)) {
            for (int i = 0; i < 6; i++) {
                pl_emit(w, PL_PART_DUST, w->scrollX + PL_HERO_X - PL_HERO_SIZE / 2 + i * 8,
                        w->y256 / PL_SUB + 2, -60 + i * 24,
                        120 + (int32_t)pl_scatter(w->step + (uint32_t)i, 19u, 160u), 18, 6, 0);
            }
        }
        if (ev & (PL_EV_PAD | PL_EV_ORB))
            pl_burst(w, PL_PART_SPARK, w->scrollX + PL_HERO_X, w->y256 / PL_SUB + 8, 12, 600, 24);

        /* A checkpoint is only banked where a respawn gets a full second before anything
           it must jump, so coming back to one is never a trap. */
        if (w->practice && w->grounded && !(ev & PL_EV_DEAD)) {
            const int32_t bar = w->scrollX / (PL_BAR_CELLS * PL_CELL);
            if (bar >= w->checkpointBar + PL_CHECKPOINT_BARS
                && pl_run_is_safe(w, b, PL_CHECKPOINT_CALM)) {
                w->checkpoint.scrollX  = w->scrollX;
                w->checkpoint.y256     = w->y256;
                w->checkpoint.vy256    = 0;
                w->checkpoint.grounded = true;
                w->hasCheckpoint = true;
                w->checkpointBar = bar;
            }
        }
        if (ev & PL_EV_DEAD) {
            w->phase = PL_DEAD;
            w->phaseTicks = 0;
            pl_burst(w, PL_PART_BURST, w->scrollX + PL_HERO_X, w->y256 / PL_SUB + PL_HERO_SIZE / 2,
                     34, 1500, 44);
        } else if (w->scrollX + PL_HERO_X >= w->goalX) {
            w->phase = PL_WON;
            w->phaseTicks = 0;
            for (int i = 0; i < 90; i++) {
                const uint32_t seed = (uint32_t)i * 97u;
                pl_emit(w, PL_PART_CONFETTI,
                        w->scrollX - 200 + (int32_t)pl_scatter(seed, 3u, 1600u),
                        200 + (int32_t)pl_scatter(seed, 5u, 300u),
                        -240 + (int32_t)pl_scatter(seed, 7u, 480u),
                        200 + (int32_t)pl_scatter(seed, 11u, 500u),
                        100 + (int)pl_scatter(seed, 13u, 70u),
                        6 + (int)pl_scatter(seed, 17u, 8u), (int)pl_scatter(seed, 19u, 3u));
            }
        }
    } else {
        /* A won course stays on its panel for as long as the player leaves it: the count
           stops at its top rather than wrap to the negative ticks that replay the fly-out. */
        if (w->phaseTicks < INT32_MAX)
            w->phaseTicks++;
        if (w->phase == PL_WON) {
            /* The hero flies through the gate on a shallow arc and bursts into light. */
            if (w->phaseTicks <= PL_WIN_FLY_TICKS) {
                w->scrollX += w->speed;
                w->y256    += 1000;
                w->spin    += 32;
            }
            if (w->phaseTicks == PL_WIN_FLY_TICKS) {
                pl_burst(w, PL_PART_BURST, w->scrollX + PL_HERO_X,
                         w->y256 / PL_SUB + PL_HERO_SIZE / 2, 34, 1500, 44);
            }
        }
    }
    pl_particles_step(w);
    pl_fmt_hud(w);
}

/* -- the attract-mode bot ------------------------------------------------------ */

/* True when pressing here carries this body to a landing it can leave again. */
static bool pl_press_lands(const PlWorld *w, PlBody b) {
    if (pl_advance(w, &b, false, true) & PL_EV_DEAD)
        return false;
    for (int t = 0; t < 70 && !b.grounded; t++) {
        if (pl_advance(w, &b, false, false) & PL_EV_DEAD)
            return false;
    }
    return pl_run_is_safe(w, b, 4);
}

/* The menus' demo player: when running on would kill it, press at the last tick a
   press still works. It looks one landing ahead and no further, so it sometimes
   dies, and a retry looks fine behind a menu. */
PLDEF bool pl_bot_wants_press(const PlWorld *w) {
    const PlBody b = { w->scrollX, w->y256, w->vy256, w->grounded };
    PlBody later = b;

    if (pl_run_is_safe(w, b, 48))
        return false;
    if (!b.grounded)
        return pl_touching(w, &b, PL_CELL_ORB) && pl_press_lands(w, b);
    for (int t = 0; t < 48; t++) {
        if (pl_advance(w, &later, false, false) & PL_EV_DEAD)
            break;
        if (pl_press_lands(w, later))
            return false;
    }
    return true;
}

#endif /* PLATFORMER_BACKEND_IMPLEMENTATION */
#endif /* PLATFORMER_BACKEND_H */
