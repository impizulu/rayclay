/*
    opsdash_backend.h - the dashboard's model: 48 services with fixed metadata,
    plus a live telemetry band (a latency ring, a request counter, an incident
    timer).

    Pure C99, no RayClay dependency, deterministic: no wall clock, no rand(), no
    I/O. ops_seed() fills the inventory; ops_tick() advances ONLY the live band,
    and only when dt > 0. Every displayed number is preformatted here into a
    fixed buffer, so the GUI never needs a format arena.

    Single implementation, in exactly one TU:
        #define OPSDASH_BACKEND_IMPLEMENTATION
        #include "opsdash_backend.h"
*/
#ifndef OPSDASH_BACKEND_H
#define OPSDASH_BACKEND_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef OPSDEF
#define OPSDEF static
#endif

#define OPS_SVC_COUNT    48    /* the service inventory */
#define OPS_GROUP_COUNT   8    /* left-nav groups; every service is in exactly one */
#define OPS_SPARK_COUNT  24    /* the latency ring: one sample per second */
#define OPS_REGION_COUNT  4
#define OPS_TOP_COUNT     5    /* the rail's slowest-services board */
#define OPS_NAME_CAP     24
#define OPS_NUM_CAP      12

/* Health is ordered by severity so the GUI can compare, and the roll-up is a max. */
typedef enum { OPS_OK = 0, OPS_WARN, OPS_DOWN, OPS_HEALTH_COUNT } OpsHealth;

typedef struct {
    char     name[OPS_NAME_CAP];
    char     owner[OPS_NAME_CAP];
    char     rps[OPS_NUM_CAP];      /* steady-state request rate, e.g. "1.2k" */
    char     p99[OPS_NUM_CAP];      /* steady-state p99 latency, e.g. "84 ms" */
    uint16_t p99Ms;                 /* the same latency as a number, for ranking */
    uint8_t  group;                 /* 0..OPS_GROUP_COUNT-1 */
    uint8_t  region;                /* index into OPS_REGIONS */
    uint8_t  tier;                  /* 1..3; tier 1 is customer-facing */
    uint8_t  health;                /* OpsHealth - FIXED at seed; never ticks */
} OpsService;

/* The live band. Everything here changes on a dt > 0 tick and nothing else does. */
typedef struct {
    uint8_t  spark[OPS_SPARK_COUNT]; /* 0..100 latency samples, oldest first */
    uint32_t reqTotal;               /* monotonic request counter */
    uint32_t incidentSecs;           /* time the open incident has been running */
    float    accum;                  /* fractional-second carry, so ticks are dt-exact */
    float    pulse;                  /* 0..1 triangle wave for the incident banner */
    bool     pulseUp;
    char     reqText[OPS_NUM_CAP];   /* reqTotal, formatted */
    char     p99Text[OPS_NUM_CAP];   /* newest spark sample, formatted "NN ms" */
    char     upText[OPS_NUM_CAP];    /* incidentSecs, formatted "MM:SS" */
    /* The ring's own range, so the sparkline can state the scale it is drawn at
       instead of leaving the reader to guess. */
    uint8_t  sparkLo, sparkHi;
    char     loText[OPS_NUM_CAP];
    char     hiText[OPS_NUM_CAP];
} OpsLive;

typedef struct {
    OpsService svc[OPS_SVC_COUNT];
    OpsLive    live;
    /* The grid's draw order: service indices, worst health first. Seeded and never
       written again, exactly like the health it reads, so the inventory stays the
       unchanged subtree this app exists to be. */
    uint8_t    order[OPS_SVC_COUNT];
    uint8_t    groupHealth[OPS_GROUP_COUNT]; /* max health per group; seeded, static */
    uint16_t   groupCount[OPS_GROUP_COUNT];  /* services per group; seeded, static */
    /* Fleet roll-up for the summary row. Seeded and static like the health it counts,
       and preformatted here because the frozen core calls no rcFormat and has no
       arena to format into: the strings must already exist by the time it draws. */
    uint16_t   healthCount[OPS_HEALTH_COUNT];
    char       healthText[OPS_HEALTH_COUNT][OPS_NUM_CAP];
    char       totalText[OPS_NUM_CAP];
    /* Two more seeded roll-ups the panes read: services per region, and the
       OPS_TOP_COUNT slowest services worst first. */
    uint16_t   regionCount[OPS_REGION_COUNT];
    char       regionText[OPS_REGION_COUNT][OPS_NUM_CAP];
    uint8_t    topLatency[OPS_TOP_COUNT];
    uint32_t   rng;
} OpsStore;

OPSDEF const char *const OPS_REGIONS[OPS_REGION_COUNT] = {
    "us-east", "us-west", "eu-west", "ap-south",
};
/* The titlebar's scope filters. Index 0 is "everything", so a plain rcCombo over
   these is a filter with an off position and needs no second control. */
OPSDEF const char *const OPS_REGION_FILTER[OPS_REGION_COUNT + 1] = {
    "All regions", "us-east", "us-west", "eu-west", "ap-south",
};
OPSDEF const char *const OPS_TIER_FILTER[4] = { "All tiers", "T1", "T2", "T3" };
OPSDEF const char *const OPS_GROUPS[OPS_GROUP_COUNT] = {
    "Edge",  "Identity", "Payments", "Catalog",
    "Search", "Media",   "Analytics", "Platform",
};

/* Declared and defined only under IMPLEMENTATION, so a TU that includes this header
   for the types alone never sees a static prototype it does not define. */
#ifdef OPSDASH_BACKEND_IMPLEMENTATION

#include <string.h>

OPSDEF void ops_memzero(void *p, size_t n);
OPSDEF void ops_seed(OpsStore *s, unsigned seed);
OPSDEF void ops_tick(OpsStore *s, float dt);

OPSDEF void ops_memzero(void *p, size_t n) { memset(p, 0, n); }

/* xorshift32 - a deterministic stream, so a re-seed reproduces the fleet exactly.
   Seed 0 would latch the generator at 0, so it is folded to a non-zero constant. */
static uint32_t ops__rand(uint32_t *st) {
    uint32_t x = *st ? *st : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *st = x;
    return x;
}

/* Append a decimal u32 at `at`, returning the new end. Caller guarantees the room:
   every call site below writes into an OPS_NUM_CAP buffer, and a u32 is at most 10
   digits, so the widest string this can produce ("4294967295") still fits. */
static size_t ops__u32(char *dst, size_t at, uint32_t v) {
    char tmp[10];
    size_t n = 0;
    do { tmp[n++] = (char)('0' + (v % 10u)); v /= 10u; } while (v);
    while (n) dst[at++] = tmp[--n];
    return at;
}

/* "1.2k" above a thousand, plain digits below - the readout a dashboard would show. */
static void ops__rate(char *dst, size_t cap, uint32_t v) {
    size_t at = 0;
    if (v >= 1000u) {
        at = ops__u32(dst, at, v / 1000u);
        dst[at++] = '.';
        dst[at++] = (char)('0' + ((v / 100u) % 10u));
        dst[at++] = 'k';
    } else {
        at = ops__u32(dst, at, v);
    }
    dst[at < cap ? at : cap - 1] = '\0';
}

static void ops__ms(char *dst, size_t cap, uint32_t v) {
    size_t at = ops__u32(dst, 0, v);
    dst[at++] = ' ';
    dst[at++] = 'm';
    dst[at++] = 's';
    dst[at < cap ? at : cap - 1] = '\0';
}

/* "MM:SS", zero-padded, so the string WIDTH is stable and the row never reflows. */
static void ops__clock(char *dst, size_t cap, uint32_t secs) {
    uint32_t m = (secs / 60u) % 100u, s = secs % 60u;
    size_t at = 0;
    dst[at++] = (char)('0' + (m / 10u));
    dst[at++] = (char)('0' + (m % 10u));
    dst[at++] = ':';
    dst[at++] = (char)('0' + (s / 10u));
    dst[at++] = (char)('0' + (s % 10u));
    dst[at < cap ? at : cap - 1] = '\0';
}

/* The ring's low and high sample, and their labels. A sparkline scaled 0-100 turns
   a live series into a flat picket fence, so the GUI scales to this range instead
   and prints it beside the bars. */
static void ops__range(OpsLive *l) {
    uint8_t lo = l->spark[0], hi = l->spark[0];
    for (int i = 1; i < OPS_SPARK_COUNT; i++) {
        if (l->spark[i] < lo) lo = l->spark[i];
        if (l->spark[i] > hi) hi = l->spark[i];
    }
    l->sparkLo = lo;
    l->sparkHi = hi;
    ops__ms(l->loText, OPS_NUM_CAP, lo);
    ops__ms(l->hiText, OPS_NUM_CAP, hi);
}

static void ops__copy(char *dst, size_t cap, const char *src) {
    size_t i = 0;
    while (src[i] && i + 1 < cap) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

/* Names are built from a noun x suffix table rather than a 48-entry literal list, so
   OPS_SVC_COUNT can move without a second edit. */
static const char *const OPS__NOUN[12] = {
    "gateway", "auth",    "ledger",  "catalog",
    "search",  "media",   "metrics", "scheduler",
    "session", "billing", "inventory", "notify",
};
static const char *const OPS__SUFFIX[4] = { "-api", "-worker", "-cache", "-sync" };
static const char *const OPS__OWNER[6] = {
    "team-atlas", "team-borealis", "team-cinder",
    "team-delta", "team-ember",    "team-flux",
};

OPSDEF void ops_seed(OpsStore *s, unsigned seed) {
    ops_memzero(s, sizeof *s);
    s->rng = (uint32_t)seed;

    for (int i = 0; i < OPS_SVC_COUNT; i++) {
        OpsService *sv = &s->svc[i];
        size_t at = 0;
        const char *noun = OPS__NOUN[i % 12];
        const char *sfx  = OPS__SUFFIX[(i / 12) % 4];
        while (*noun && at + 1 < OPS_NAME_CAP) sv->name[at++] = *noun++;
        while (*sfx  && at + 1 < OPS_NAME_CAP) sv->name[at++] = *sfx++;
        sv->name[at] = '\0';

        ops__copy(sv->owner, OPS_NAME_CAP, OPS__OWNER[ops__rand(&s->rng) % 6u]);
        sv->group  = (uint8_t)(i % OPS_GROUP_COUNT);
        sv->region = (uint8_t)(ops__rand(&s->rng) % 4u);
        sv->tier   = (uint8_t)(1u + (ops__rand(&s->rng) % 3u));

        /* Mostly healthy, a few warnings, two hard downs - a real fleet's shape, and
           it gives the card grid three visually distinct states to draw. */
        uint32_t r = ops__rand(&s->rng) % 100u;
        sv->health = (uint8_t)(r < 78u ? OPS_OK : (r < 96u ? OPS_WARN : OPS_DOWN));

        ops__rate(sv->rps, OPS_NUM_CAP, 40u + (ops__rand(&s->rng) % 9000u));
        sv->p99Ms = (uint16_t)(8u + (ops__rand(&s->rng) % 240u));
        ops__ms  (sv->p99, OPS_NUM_CAP, sv->p99Ms);

        s->regionCount[sv->region & (OPS_REGION_COUNT - 1)]++;
        s->groupCount[sv->group]++;
        if (sv->health > s->groupHealth[sv->group])
            s->groupHealth[sv->group] = sv->health;
        if (sv->health < OPS_HEALTH_COUNT)
            s->healthCount[sv->health]++;
    }

    /* FAILURES FIRST. Three severity passes over the inventory, each keeping index
       order within its band, so the result is stable and the same on every machine.
       An operator reading a wall of 48 cards should never have to hunt the three
       that are down; putting them at the top is what a status page does. */
    {
        int n = 0;
        for (int h = OPS_HEALTH_COUNT - 1; h >= 0; h--)
            for (int i = 0; i < OPS_SVC_COUNT; i++)
                if (s->svc[i].health == (uint8_t)h)
                    s->order[n++] = (uint8_t)i;
    }

    /* The slowest OPS_TOP_COUNT services, worst first: a selection sort over a
       scratch copy, which is exact and needs no comparator. */
    {
        uint16_t best;
        bool     taken[OPS_SVC_COUNT];
        for (int i = 0; i < OPS_SVC_COUNT; i++)
            taken[i] = false;
        for (int k = 0; k < OPS_TOP_COUNT; k++) {
            int pick = 0;
            best = 0;
            for (int i = 0; i < OPS_SVC_COUNT; i++)
                if (!taken[i] && s->svc[i].p99Ms >= best) {
                    best = s->svc[i].p99Ms;
                    pick = i;
                }
            taken[pick]       = true;
            s->topLatency[k]  = (uint8_t)pick;
        }
    }

    for (int i = 0; i < OPS_HEALTH_COUNT; i++)
        ops__rate(s->healthText[i], OPS_NUM_CAP, s->healthCount[i]);
    for (int i = 0; i < OPS_REGION_COUNT; i++)
        ops__rate(s->regionText[i], OPS_NUM_CAP, s->regionCount[i]);
    ops__rate(s->totalText, OPS_NUM_CAP, (uint32_t)OPS_SVC_COUNT);

    for (int i = 0; i < OPS_SPARK_COUNT; i++)
        s->live.spark[i] = (uint8_t)(30u + (ops__rand(&s->rng) % 40u));

    s->live.reqTotal = 1000u;
    s->live.pulseUp  = true;
    ops__rate (s->live.reqText, OPS_NUM_CAP, s->live.reqTotal);
    ops__ms   (s->live.p99Text, OPS_NUM_CAP, s->live.spark[OPS_SPARK_COUNT - 1]);
    ops__clock(s->live.upText,  OPS_NUM_CAP, 0u);
    ops__range(&s->live);
}

/* Advance ONLY the live band. dt <= 0 is the freeze: it returns before touching
   anything, so the scene stands still. */
OPSDEF void ops_tick(OpsStore *s, float dt) {
    if (dt <= 0.0f) return;

    /* The pulse is a triangle wave rather than a sine: no libm, and it is exactly
       reproducible from dt alone, which the two-frame-difference bench needs. */
    s->live.pulse += (s->live.pulseUp ? dt : -dt);
    if (s->live.pulse >= 1.0f) { s->live.pulse = 1.0f; s->live.pulseUp = false; }
    if (s->live.pulse <= 0.0f) { s->live.pulse = 0.0f; s->live.pulseUp = true;  }

    /* Roll the sparkline and the counters on a whole-second boundary, carrying the
       remainder, so the readout advances at the same rate under any frame pacing. */
    s->live.accum += dt;
    while (s->live.accum >= 1.0f) {
        s->live.accum -= 1.0f;

        for (int i = 0; i + 1 < OPS_SPARK_COUNT; i++)
            s->live.spark[i] = s->live.spark[i + 1];
        uint8_t prev = s->live.spark[OPS_SPARK_COUNT - 2];
        int32_t next = (int32_t)prev + (int32_t)(ops__rand(&s->rng) % 21u) - 10;
        if (next < 5)   next = 5;
        if (next > 100) next = 100;
        s->live.spark[OPS_SPARK_COUNT - 1] = (uint8_t)next;

        s->live.reqTotal    += 7u + (ops__rand(&s->rng) % 40u);
        s->live.incidentSecs += 1u;

        ops__rate (s->live.reqText, OPS_NUM_CAP, s->live.reqTotal);
        ops__ms   (s->live.p99Text, OPS_NUM_CAP, s->live.spark[OPS_SPARK_COUNT - 1]);
        ops__clock(s->live.upText,  OPS_NUM_CAP, s->live.incidentSecs);
        ops__range(&s->live);
    }
}

#endif /* OPSDASH_BACKEND_IMPLEMENTATION */
#endif /* OPSDASH_BACKEND_H */
