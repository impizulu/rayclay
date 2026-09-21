/* host.h - the seam you replace to make ex20 a real system monitor.
 *
 * Everything above this file reads a SysHost and knows nothing about where the
 * numbers came from. Swap the two functions for /proc, sysctl or the Windows
 * performance counters and not a line of the UI changes.
 *
 * As shipped the model is a deterministic PRNG. A host's cores, process table
 * and NIC counters do not exist in a browser tab and need a different collector
 * on every desktop, so a fixed seed is what draws the same frame on every
 * target. Fixed-size storage and no allocation: a monitor is the archetypal
 * run-forever app. rayclay.h supplies the fixed-width types and rcStrCopy, so
 * there is no libc dependency here either.
 */

#ifndef APP_HOST_H
#define APP_HOST_H

#include "rayclay.h"

enum {
    SYS_CORES     = 8,    /* simulated logical CPUs                            */
    SYS_PROCS     = 128,  /* simulated process table - fixed, never grows      */
    SYS_NAME_MAX  = 20
};

/** Ordered by how much attention the state deserves, so a sort reads right. */
typedef enum SysProcState {
    SYS_SLEEPING = 0,
    SYS_RUNNING,
    SYS_STOPPED
} SysProcState;

typedef struct SysProc {
    char         name[SYS_NAME_MAX];  /**< NUL-terminated; suffixed in init    */
    int32_t      pid;
    float        cpu;      /**< percent of ONE core, like top - may exceed 100 */
    float        memMiB;
    SysProcState state;    /**< drives the row colour AND the state sort order */
} SysProc;

typedef struct SysHost {
    float    core[SYS_CORES];   /**< per-core load, percent                    */
    float    memUsedMiB;
    float    memTotalMiB;
    float    swapUsedMiB;
    float    swapTotalMiB;
    /* Rates, not running totals: a rate is bounded by construction, so none of
       these can overflow however long the app runs. */
    float    netRxKiB;
    float    netTxKiB;
    float    diskRdKiB;
    float    diskWrKiB;
    SysProc  proc[SYS_PROCS];   /**< fixed-size: the model never allocates     */
    int32_t  procCount;
    uint32_t rng;               /**< xorshift state; never zero (see init)     */
} SysHost;

/* xorshift32. Never seeded with 0 - xorshift is stuck there. */
static inline uint32_t sysmon__rand(SysHost *h)
{
    uint32_t x = h->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    h->rng = x;
    return x;
}

/** Uniform in [0,1); divides as a double so it cannot round up to 1.0. */
static inline float sysmon__unit(SysHost *h)
{
    return (float)((double)sysmon__rand(h) / 4294967296.0);
}

/** Pull @p cur a fraction @p k toward @p target, add bounded jitter, clamp to
    [lo,hi]. The pull is what keeps the series BOUNDED: a random walk without it
    wanders off the axis after a few thousand samples. */
static inline float sysmon__drift(SysHost *h, float cur, float target, float k,
                           float jitter, float lo, float hi)
{
    float v = cur + (target - cur) * k + (sysmon__unit(h) - 0.5f) * jitter;
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return v;
}

/** Seed the model. Zeroes first, so a caller may hold SysHost by value and
    re-seed it to a known state at any time. */
static inline void sysmon_host_init(SysHost *h, uint32_t seed)
{
    static const char *stem[] = {
        "kernel_task", "init", "logind", "netd", "audiod", "indexer",
        "compositor", "renderer", "webview", "backup", "sync-agent",
        "db-writer", "cache", "scheduler", "telemetry", "updater"
    };
    static const SysHost blank = {0};   /* explicit: C++ needs a const initialised */
    int i;

    *h = blank;                   /* struct assignment, so no memset needed */
    h->rng = seed ? seed : 0x9E3779B9u;   /* xorshift cannot start at zero */

    h->memTotalMiB  = 16384.0f;
    h->memUsedMiB   = 6200.0f;
    h->swapTotalMiB = 4096.0f;
    h->swapUsedMiB  = 320.0f;

    for (i = 0; i < SYS_CORES; i++)
        h->core[i] = 4.0f + sysmon__unit(h) * 20.0f;

    h->procCount = SYS_PROCS;
    for (i = 0; i < SYS_PROCS; i++) {
        SysProc *p = &h->proc[i];
        int n = 0;

        rcStrCopy(p->name, stem[i & 15], SYS_NAME_MAX - 4);
        while (p->name[n]) n++;
        if (i >= 16) {
            p->name[n++] = '-';
            p->name[n++] = (char)('0' + (i / 100) % 10);
            p->name[n++] = (char)('0' + (i / 10) % 10);
            p->name[n++] = (char)('0' + i % 10);
            p->name[n]   = '\0';
        }

        p->pid    = 100 + i * 7 + (int32_t)(sysmon__unit(h) * 5.0f);
        p->cpu    = sysmon__unit(h) * sysmon__unit(h) * 60.0f;  /* long tail */
        p->memMiB = 4.0f + sysmon__unit(h) * sysmon__unit(h) * 900.0f;
        p->state  = (i % 11 == 0) ? SYS_RUNNING
                  : (i % 37 == 0) ? SYS_STOPPED : SYS_SLEEPING;
    }
}

/** Advance the model by one sample interval. Takes no dt on purpose: the caller
    samples on a fixed cadence it owns, so "one step" is the only unit this model
    needs and there is no frame delta to pass by mistake. */
static inline void sysmon_host_sample(SysHost *h)
{
    float busiest = 0.0f;
    int i;

    for (i = 0; i < SYS_CORES; i++) {
        /* Each core reverts to its own plateau, so the strip shows a spread. */
        float plateau = 12.0f + (float)(i * 9);
        h->core[i] = sysmon__drift(h, h->core[i], plateau, 0.25f, 34.0f, 0.0f, 100.0f);
        if (h->core[i] > busiest) busiest = h->core[i];
    }

    h->memUsedMiB  = sysmon__drift(h, h->memUsedMiB, 7000.0f, 0.05f, 180.0f,
                                   512.0f, h->memTotalMiB);
    h->swapUsedMiB = sysmon__drift(h, h->swapUsedMiB, 380.0f, 0.04f, 24.0f,
                                   0.0f, h->swapTotalMiB);

    /* Non-zero floors: an idle NIC is never silent, and a rate pinned at 0
       reads as a broken widget. */
    h->netRxKiB  = sysmon__drift(h, h->netRxKiB,  420.0f, 0.30f, 480.0f, 6.0f, 12000.0f);
    h->netTxKiB  = sysmon__drift(h, h->netTxKiB,   90.0f, 0.30f, 150.0f, 2.0f,  6000.0f);
    h->diskRdKiB = sysmon__drift(h, h->diskRdKiB, 150.0f, 0.35f, 600.0f, 0.0f, 24000.0f);
    h->diskWrKiB = sysmon__drift(h, h->diskWrKiB, 240.0f, 0.35f, 600.0f, 0.0f, 24000.0f);

    for (i = 0; i < h->procCount; i++) {
        SysProc *p = &h->proc[i];

        /* A stopped process burns nothing: that is what the state sort is for. */
        if (p->state == SYS_STOPPED) {
            p->cpu = 0.0f;
            continue;
        }
        p->cpu    = sysmon__drift(h, p->cpu, busiest * 0.22f, 0.20f, 18.0f, 0.0f, 240.0f);
        p->memMiB = sysmon__drift(h, p->memMiB, p->memMiB, 0.0f, 6.0f, 2.0f, 4096.0f);
        p->state  = (p->cpu > 12.0f) ? SYS_RUNNING : SYS_SLEEPING;
    }
}

#endif /* APP_HOST_H */
