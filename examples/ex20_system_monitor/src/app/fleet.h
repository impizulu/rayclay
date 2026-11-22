/*
    fleet.h - the seam you replace to make the Services page read a real fleet.

    A Fleet is two things that change at opposite rates, and the Services page is
    built on keeping them apart. The INVENTORY - 48 services, their owners and
    regions, their last health check - is written once by fleet_init and never
    again. The LIVE BAND - the fleet's p99 latency, its request rate, the open
    alert's clock - is what fleet_sample advances, one second at a time. Swap
    fleet_init for your service registry and fleet_sample for your metrics
    stream, and keep that split: services.h's "THE STATIC ISLAND" is what it buys.

    Every string the inventory shows is formatted HERE, once, into fixed storage,
    so the layout that declares 48 cards formats nothing. The arena that does it
    lives only inside fleet_init.

    Deterministic like host.h: xorshift from a fixed seed, no clock, no libc, and
    fixed-size storage, so a run-forever monitor never allocates after start-up.
*/

#ifndef APP_FLEET_H
#define APP_FLEET_H

#include "rayclay.h"
#include "app/theme.h"

enum {
    FLEET_SERVICES = 48,
    FLEET_GROUPS   = 8,
    FLEET_REGIONS  = 4,
    FLEET_TIERS    = 3,
    FLEET_SLOWEST  = 5,        /* the slowest-services board                 */
    FLEET_TEXT_MAX = 24
};

/** Ordered by how much attention the state deserves, so a sort reads right. */
typedef enum SvcHealth {
    SVC_OK = 0,
    SVC_DEGRADED,
    SVC_DOWN,
    SVC_HEALTH_COUNT
} SvcHealth;

typedef struct Service {
    char      name[FLEET_TEXT_MAX];
    char      owner[FLEET_TEXT_MAX];
    char      rps[FLEET_TEXT_MAX];  /**< steady request rate, "1.2k"           */
    char      p99[FLEET_TEXT_MAX];  /**< steady p99 latency, "84 ms"           */
    float     reqPerSec;
    float     p99Ms;
    int       group;                /**< index into FLEET_GROUP                 */
    int       region;               /**< index into FLEET_REGION                */
    int       tier;                 /**< 1..FLEET_TIERS; 1 is customer-facing   */
    SvcHealth health;               /**< the last health check: seeded, fixed   */
} Service;

typedef struct Fleet {
    /* THE INVENTORY: written by fleet_init, read-only after it. */
    Service  svc[FLEET_SERVICES];
    int      order[FLEET_SERVICES];     /**< draw order: worst health first     */
    int      count[SVC_HEALTH_COUNT];   /**< services in each state             */
    int      slowest[FLEET_SERVICES];   /**< by steady p99, slowest first       */
    int      incident;                  /**< the service the open alert names   */
    float    baseRate;                  /**< the inventory's summed request rate */

    /* THE LIVE BAND, oldest first, one slot per second, the same HISTORY the
       host panels keep, so the dashboard's time range reads both. */
    float    p99[HISTORY];              /**< the fleet's p99 latency, ms        */
    float    rate[HISTORY];             /**< requests per second, fleet-wide    */
    /* Requests served up to the newest sample. A double, because a float runs
       out of whole numbers at 16.7 million and this passes that in a minute. */
    double   served;
    uint32_t alertSecs;                 /**< how long the open alert has fired  */
    uint32_t tick;                      /**< samples taken: the swell's phase   */
    uint32_t rng;                       /**< xorshift state; never zero         */
} Fleet;

static const char *const FLEET_REGION[FLEET_REGIONS] = {
    "us-east", "us-west", "eu-west", "ap-south"
};
static const char *const FLEET_GROUP[FLEET_GROUPS] = {
    "Edge", "Identity", "Payments", "Catalog", "Search", "Media", "Analytics", "Platform"
};

/* xorshift32, as host.h's. Never seeded with 0 - xorshift is stuck there. */
static inline uint32_t fleet_rand(Fleet *f) {
    uint32_t x = f->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    f->rng = x;
    return x;
}

static inline float fleet_unit(Fleet *f) {
    return (float)((double)fleet_rand(f) / 4294967296.0);
}

/* The pull toward a target is what keeps a walk BOUNDED (see host.h). */
static inline float fleet_drift(Fleet *f, float cur, float target, float k, float jitter,
                                float lo, float hi) {
    float v = cur + (target - cur) * k + (fleet_unit(f) - 0.5f) * jitter;

    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return v;
}

/* A triangle wave in [0,1], @p period seconds crest to crest. */
static inline float fleet_wave(uint32_t tick, uint32_t period) {
    float phase = (float)(tick % period) / (float)period;

    return phase < 0.5f ? phase * 2.0f : 2.0f - phase * 2.0f;
}

/** Advance the live band by one second. The inventory is not touched: nothing
    here may write it, or the page's static island is gone. */
static inline void fleet_sample(Fleet *f) {
    /* The swell that traffic and latency both ride, so the alert line is crossed
       in bursts, when the fleet is busiest, rather than by scattered noise. Three
       waves with no common period, so a half-hour chart never shows one tooth
       ten times. */
    float swell = 0.25f * fleet_wave(f->tick, 83u) + 0.45f * fleet_wave(f->tick, 241u)
                + 0.30f * fleet_wave(f->tick, 967u);
    int   i;

    /* The rate that held over the second just ended, counted BEFORE it moves:
       a caller that extrapolates served at the newest rate then meets the next
       sample exactly where it left off, so the counter never steps back. */
    f->served += (double)f->rate[HISTORY - 1];
    for (i = 0; i + 1 < HISTORY; i++) {
        f->p99[i]  = f->p99[i + 1];
        f->rate[i] = f->rate[i + 1];
    }
    f->p99[HISTORY - 1]  = fleet_drift(f, f->p99[HISTORY - 2], 52.0f + 26.0f * swell,
                                        0.12f, 14.0f, 12.0f, 120.0f);
    f->rate[HISTORY - 1] = fleet_drift(f, f->rate[HISTORY - 2],
                                        f->baseRate * (0.85f + 0.3f * swell), 0.2f,
                                        f->baseRate * 0.06f, f->baseRate * 0.5f,
                                        f->baseRate * 1.5f);
    f->tick++;
    f->alertSecs++;
}

/** Seed the fleet: the inventory, its roll-ups, and a primed live band, as
    though the dashboard had been streaming for its whole history. */
static inline void fleet_init(Fleet *f, uint32_t seed) {
    static const char *const noun[12] = {
        "gateway", "auth", "ledger", "catalog", "search", "media",
        "metrics", "scheduler", "session", "billing", "inventory", "notify"
    };
    static const char *const suffix[4] = { "-api", "-worker", "-cache", "-sync" };
    static const char *const owner[6] = {
        "team-atlas", "team-borealis", "team-cinder", "team-delta", "team-ember",
        "team-flux"
    };
    /* Latency follows health, under the 75 ms alert line while OK: indexed by
       SvcHealth, the lowest p99 and how many values from it a draw lands on. */
    static const uint32_t p99Lo[SVC_HEALTH_COUNT]   = { 8, 76, 180 };
    static const uint32_t p99Span[SVC_HEALTH_COUNT] = { 63, 104, 71 };
    RC_Arena text = rcArenaInit(256);
    bool     taken[FLEET_SERVICES];
    int      i, k, n = 0;

    /* Every other field is written below; these are the ones that accumulate. */
    for (k = 0; k < SVC_HEALTH_COUNT; k++)
        f->count[k] = 0;
    f->baseRate = 0.0f;
    f->served   = 0.0;
    f->tick     = 0;
    f->rng      = seed ? seed : 0x9E3779B9u;

    for (i = 0; i < FLEET_SERVICES; i++) {
        Service *sv = &f->svc[i];
        uint32_t r;

        rcArenaReset(&text);
        rcStrCopy(sv->name, rcFormat(&text, "%s%s", noun[i % 12],
                                     suffix[(i / 12) % 4]).chars, sizeof sv->name);
        rcStrCopy(sv->owner, owner[fleet_rand(f) % 6u], sizeof sv->owner);
        sv->group  = i % FLEET_GROUPS;
        sv->region = (int)(fleet_rand(f) % FLEET_REGIONS);
        sv->tier   = 1 + (int)(fleet_rand(f) % FLEET_TIERS);
        /* Mostly healthy, a few degraded, two down: a real fleet's shape. */
        r = fleet_rand(f) % 100u;
        sv->health = r < 78u ? SVC_OK : r < 96u ? SVC_DEGRADED : SVC_DOWN;
        sv->reqPerSec = (float)(40u + fleet_rand(f) % 9000u);
        sv->p99Ms     = (float)(p99Lo[sv->health] + fleet_rand(f) % p99Span[sv->health]);

        if (sv->reqPerSec >= 1000.0f)
            rcStrCopy(sv->rps, rcFormat(&text, "%.1fk", (double)sv->reqPerSec / 1000.0).chars,
                      sizeof sv->rps);
        else
            rcStrCopy(sv->rps, rcFormat(&text, "%.0f", (double)sv->reqPerSec).chars,
                      sizeof sv->rps);
        rcStrCopy(sv->p99, rcFormat(&text, "%.0f ms", (double)sv->p99Ms).chars,
                  sizeof sv->p99);

        f->count[sv->health]++;
        f->baseRate += sv->reqPerSec;
    }
    rcArenaFree(&text);

    /* WORST FIRST, catalog order within a state: an operator should never hunt
       a wall of 48 cards for the two that are down. */
    for (k = SVC_HEALTH_COUNT - 1; k >= 0; k--)
        for (i = 0; i < FLEET_SERVICES; i++)
            if ((int)f->svc[i].health == k)
                f->order[n++] = i;
    f->incident = f->order[0];

    /* Slowest first, by a selection pass: exact, and no comparator. */
    for (i = 0; i < FLEET_SERVICES; i++)
        taken[i] = false;
    for (k = 0; k < FLEET_SERVICES; k++) {
        int pick = 0;

        while (taken[pick])
            pick++;
        for (i = pick + 1; i < FLEET_SERVICES; i++)
            if (!taken[i] && f->svc[i].p99Ms > f->svc[pick].p99Ms)
                pick = i;
        taken[pick]   = true;
        f->slowest[k] = pick;
    }

    f->p99[HISTORY - 1]  = 52.0f;
    f->rate[HISTORY - 1] = f->baseRate;
    for (i = 0; i < HISTORY; i++)
        fleet_sample(f);
    f->alertSecs = 191;   /* the alert opened a little over three minutes ago */
}

#endif /* APP_FLEET_H */
