/*
    host.h - the seam you replace to make ex20 a real system monitor.

    Everything above this file reads a SysHost and knows nothing about where the
    numbers came from. Swap the two functions for /proc, sysctl or the Windows
    performance counters and not a line of the UI changes.

    As shipped the model is a deterministic PRNG behind three server profiles. A
    host's cores, process table and NIC counters do not exist in a browser tab, so a
    fixed seed is what draws the same dashboard on every target. The readings add up
    the way a real machine's do: the processes share out the cores' load and the
    memory in use. Fixed-size storage and no allocation after start-up: a monitor is
    the archetypal run-forever app.
*/

#ifndef APP_HOST_H
#define APP_HOST_H

#include "rayclay.h"

enum {
    SYS_CORES    = 8,
    SYS_PROCS    = 128,
    SYS_NAME_MAX = 24,
    SYS_USER_MAX = 12,
    SYS_UNITS    = 4     /* the daemons a host's status tiles watch */
};

/* Ordered by how much attention the state deserves, so a sort reads right. */
typedef enum SysProcState {
    SYS_SLEEPING = 0,
    SYS_RUNNING,
    SYS_STOPPED
} SysProcState;

typedef struct SysProc {
    char         name[SYS_NAME_MAX];
    char         user[SYS_USER_MAX];
    int32_t      pid;
    float        cpu;        /* percent of ONE core, like top: may exceed 100 */
    float        memMiB;
    float        cpuBase;    /* its usual share of the load                   */
    float        cpuWeight;  /* ... and its share right now                   */
    float        memWeight;  /* its share of the memory in use                */
    SysProcState state;
} SysProc;

/* What one kind of server looks like. */
typedef struct SysProfile {
    const char *name;
    const char *user;              /* who runs the daemons               */
    const char *unit[SYS_UNITS];   /* those daemons                      */
    int         stoppedUnit;       /* the one that is down, or -1        */
    float       busy;              /* scales the load: 1 = reference     */
    float       memTotalGiB, memBaseGiB;
    float       rxKB, txKB, rdKB, wrKB;
    float       diskPct;
    int         upDays;
} SysProfile;

typedef struct SysHost {
    const SysProfile *profile;
    float    core[SYS_CORES];   /* per-core load, percent                       */
    float    plateau[SYS_CORES];/* where each core settles at the reference load */
    float    userShare;         /* how the load splits: the rest is iowait      */
    float    sysShare;
    float    load1;             /* one-minute load average                      */
    float    memUsedGiB;
    float    memCacheGiB;
    float    swapUsedGiB;
    float    swapTotalGiB;
    float    diskPct;
    /* Rates rather than running totals: a rate is bounded by construction, so
       none of these can overflow however long the app runs. */
    float    netRxKB;
    float    netTxKB;
    float    diskRdKB;
    float    diskWrKB;
    double   uptimeSecs;
    float    burst;             /* a load spike decaying back to the plateau    */
    uint32_t tick;              /* samples taken: the swell's phase             */
    SysProc  proc[SYS_PROCS];
    int32_t  procCount;
    uint32_t rng;               /* xorshift state; never zero                   */
} SysHost;

/* xorshift32. Never seeded with 0: xorshift is stuck there. */
static inline uint32_t sysmon_rand(SysHost *h) {
    uint32_t x = h->rng;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    h->rng = x;
    return x;
}

/* Uniform in [0,1); divides as a double so it cannot round up to 1.0. */
static inline float sysmon_unit(SysHost *h) {
    return (float)((double)sysmon_rand(h) / 4294967296.0);
}

/* Pull cur a fraction k toward target, add bounded jitter, clamp to [lo,hi]. The
   pull is what keeps a series bounded: a random walk without it wanders off the axis
   after a few thousand samples. */
static inline float sysmon_drift(SysHost *h, float cur, float target, float k, float jitter,
                                 float lo, float hi) {
    float v = cur + (target - cur) * k + (sysmon_unit(h) - 0.5f) * jitter;

    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return v;
}

/* A triangle wave in [0,1], @p period samples crest to crest. */
static inline float sysmon_wave(uint32_t tick, uint32_t period) {
    float phase = (float)(tick % period) / (float)period;

    return phase < 0.5f ? phase * 2.0f : 2.0f - phase * 2.0f;
}

/* Seed the model. Zeroes first, so a caller may re-seed it to a known state. */
static inline void sysmon_host_init(SysHost *h, const SysProfile *profile, uint32_t seed) {
    static const char *const common[16] = {
        "systemd", "systemd-journald", "systemd-udevd", "sshd", "rsyslogd", "cron",
        "containerd", "dockerd", "node_exporter", "chronyd", "dbus-daemon", "polkitd",
        "irqbalance", "multipathd", "snapd", "unattended-upgr"
    };
    static const SysHost blank = { 0 };
    RC_Arena text = rcArenaInit(256);
    int i;

    *h = blank;
    h->profile = profile;
    h->rng     = seed ? seed : 0x9E3779B9u;
    h->tick    = h->rng % 997u;   /* each host is at its own point in the swell */

    h->memUsedGiB   = profile->memBaseGiB;
    h->memCacheGiB  = (profile->memTotalGiB - profile->memBaseGiB) * 0.6f;
    h->swapTotalGiB = 4.0f;
    h->swapUsedGiB  = 0.3f;
    h->diskPct      = profile->diskPct;
    h->uptimeSecs   = (double)profile->upDays * 86400.0 + 14400.0;
    h->userShare    = 0.68f;
    h->sysShare     = 0.22f;
    h->netRxKB     = profile->rxKB;
    h->netTxKB     = profile->txKB;
    h->diskRdKB    = profile->rdKB;
    h->diskWrKB    = profile->wrKB;

    /* A spread of plateaus, shuffled: a scheduler keeps its cores in no order of load. */
    for (i = 0; i < SYS_CORES; i++) {
        h->core[i]    = 4.0f + sysmon_unit(h) * 20.0f;
        h->plateau[i] = 14.0f + (float)(i * 8);
    }
    for (i = SYS_CORES - 1; i > 0; i--) {
        int   j = (int)(sysmon_rand(h) % (uint32_t)(i + 1));
        float t = h->plateau[i];

        h->plateau[i] = h->plateau[j];
        h->plateau[j] = t;
    }

    /* Its daemons first, then what every server runs, then their workers and the
       kernel's threads, which are many and cost next to nothing. */
    h->procCount = SYS_PROCS;
    for (i = 0; i < SYS_PROCS; i++) {
        SysProc *p = &h->proc[i];

        rcArenaReset(&text);
        if (i < SYS_UNITS) {
            rcStrCopy(p->name, profile->unit[i], SYS_NAME_MAX);
            rcStrCopy(p->user, profile->user, SYS_USER_MAX);
            p->cpuBase   = 8.0f + sysmon_unit(h) * 14.0f;
            p->memWeight = 10.0f + sysmon_unit(h) * 20.0f;
        } else if (i < SYS_UNITS + 16) {
            rcStrCopy(p->name, common[i - SYS_UNITS], SYS_NAME_MAX);
            rcStrCopy(p->user, "root", SYS_USER_MAX);
            p->cpuBase   = 0.2f + sysmon_unit(h) * sysmon_unit(h) * 2.0f;
            p->memWeight = 0.4f + sysmon_unit(h) * 2.0f;
        } else if (i % 2 == 0) {
            rcStrCopy(p->name, rcFormat(&text, "%s-%d", profile->unit[i % 2 + i % 3], i).chars,
                      SYS_NAME_MAX);
            rcStrCopy(p->user, profile->user, SYS_USER_MAX);
            p->cpuBase   = 1.0f + sysmon_unit(h) * sysmon_unit(h) * 7.0f;
            p->memWeight = 1.0f + sysmon_unit(h) * 5.0f;
        } else {
            rcStrCopy(p->name, rcFormat(&text, "kworker/%d:%d", i % SYS_CORES,
                                        i / SYS_CORES).chars, SYS_NAME_MAX);
            rcStrCopy(p->user, "root", SYS_USER_MAX);
            p->cpuBase   = 0.02f + sysmon_unit(h) * 0.1f;
            p->memWeight = 0.02f;
        }
        p->cpuWeight = p->cpuBase;
        p->pid   = 100 + i * 37 + (int32_t)(sysmon_unit(h) * 30.0f);
        p->state = i == profile->stoppedUnit ? SYS_STOPPED : SYS_SLEEPING;
    }
    rcArenaFree(&text);
}

/* Advance the model by one sample, taken @p seconds after the one before. Every figure
   but the uptime moves one step per sample, on the cadence the caller owns; the uptime
   adds the seconds, so it keeps true time at any refresh and across a manual refresh. */
static inline void sysmon_host_sample(SysHost *h, double seconds) {
    const SysProfile *pf = h->profile;
    /* Three waves with no common period, so a half-hour chart never shows one tooth ten
       times. */
    float swell = 0.25f * sysmon_wave(h->tick, 97u) + 0.45f * sysmon_wave(h->tick, 263u)
                + 0.30f * sysmon_wave(h->tick, 997u);
    float level, total = 0.0f, cpuWeights = 0.0f, memWeights = 0.0f;
    int   i;

    /* A slow swell with the odd spike on top, as real traffic has. */
    if (sysmon_unit(h) < 0.012f)
        h->burst = 0.2f + sysmon_unit(h) * 0.35f;
    h->burst *= 0.93f;
    level = pf->busy * (0.8f + 0.35f * swell + h->burst);

    for (i = 0; i < SYS_CORES; i++) {
        /* Each core reverts to its own plateau, so they read as a spread of bands. */
        h->core[i] = sysmon_drift(h, h->core[i], h->plateau[i] * level, 0.25f, 9.0f, 0.0f,
                                   100.0f);
        total += h->core[i];
    }
    h->load1 += (total / 100.0f - h->load1) * 0.08f;

    h->userShare = sysmon_drift(h, h->userShare, 0.68f, 0.1f, 0.03f, 0.45f, 0.8f);
    h->sysShare  = sysmon_drift(h, h->sysShare, 0.2f, 0.1f, 0.02f, 0.1f, 0.3f);

    h->memUsedGiB  = sysmon_drift(h, h->memUsedGiB, pf->memBaseGiB * (0.97f + 0.06f * swell),
                                   0.05f, pf->memTotalGiB * 0.004f, 0.5f,
                                   pf->memTotalGiB * 0.97f);
    h->memCacheGiB = sysmon_drift(h, h->memCacheGiB,
                                  (pf->memTotalGiB - h->memUsedGiB) * 0.6f, 0.05f,
                                  pf->memTotalGiB * 0.002f, 0.0f, pf->memTotalGiB);
    h->swapUsedGiB = sysmon_drift(h, h->swapUsedGiB, 0.35f, 0.04f, 0.01f, 0.0f,
                                  h->swapTotalGiB);
    /* The disk fills a little with every sample, as logs do, until rotation frees what the
       creep took: a disk that only filled would read past 100% within days. */
    h->diskPct += 0.00002f;
    if (h->diskPct >= pf->diskPct + 5.0f)
        h->diskPct = pf->diskPct;
    h->uptimeSecs += seconds;

    /* Non-zero floors: an idle NIC is never silent, and a rate pinned at 0 reads as a
       broken widget. */
    h->netRxKB  = sysmon_drift(h, h->netRxKB, pf->rxKB * level / pf->busy, 0.25f,
                                 pf->rxKB * 0.3f, 6.0f, 80000.0f);
    h->netTxKB  = sysmon_drift(h, h->netTxKB, pf->txKB * level / pf->busy, 0.25f,
                                 pf->txKB * 0.3f, 2.0f, 80000.0f);
    h->diskRdKB = sysmon_drift(h, h->diskRdKB, pf->rdKB, 0.3f, pf->rdKB * 0.8f, 0.0f,
                                 80000.0f);
    h->diskWrKB = sysmon_drift(h, h->diskWrKB, pf->wrKB * (0.8f + 0.4f * swell), 0.3f,
                                 pf->wrKB * 0.8f, 0.0f, 80000.0f);

    /* The processes split what the machine is doing, so the table adds up to the
       panels above it. A stopped process burns nothing and holds nothing. */
    for (i = 0; i < h->procCount; i++) {
        SysProc *p = &h->proc[i];

        p->cpuWeight = sysmon_drift(h, p->cpuWeight, p->cpuBase, 0.1f, p->cpuBase * 0.3f,
                                    0.01f, 40.0f);
        if (p->state != SYS_STOPPED) {
            cpuWeights += p->cpuWeight;
            memWeights += p->memWeight;
        }
    }
    for (i = 0; i < h->procCount; i++) {
        SysProc *p = &h->proc[i];

        if (p->state == SYS_STOPPED) {
            p->cpu = 0.0f;
            p->memMiB = 0.0f;
            continue;
        }
        p->cpu    = total * p->cpuWeight / cpuWeights;
        p->memMiB = h->memUsedGiB * 1024.0f * p->memWeight / memWeights;
        p->state  = p->cpu >= 2.0f ? SYS_RUNNING : SYS_SLEEPING;
    }
    h->tick++;
}

#endif /* APP_HOST_H */
