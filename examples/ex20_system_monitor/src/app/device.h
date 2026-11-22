/*
    device.h - where RayClay's readings of the device it runs on enter: the four calls
    and nothing else. host.h models servers this app cannot read; here every figure is
    real, and one the platform does not report stays empty, never filled from the model.

    Data only: the This device row in app.h draws it. Each reading keeps a history of
    its own, as a host's figures do, in fixed storage inside AppState.
*/
#ifndef APP_DEVICE_H
#define APP_DEVICE_H

#include "rayclay.h"
#include "app/theme.h"

/* Write one sample: into @p slot while priming, or pushed onto the newest end
   (slot < 0), which drops the oldest. The servers' histories use it too. */
static inline void hist_put(float *hist, int slot, float v) {
    int i;

    if (slot >= 0) {
        hist[slot] = v;
        return;
    }
    for (i = 0; i + 1 < HISTORY; i++)
        hist[i] = hist[i + 1];
    hist[HISTORY - 1] = v;
}

/* A CPU reading measures the interval since the previous call, so its first call only
   takes the baseline, and an Apple tick counter that has not advanced yet answers false
   too (rayclay.h, rcMachineCpuPercent). One with nothing after this many tries is judged
   absent. A memory reading needs no baseline: its first false answer is final. */
enum { DEVICE_TRIES = 3 };

/* One reading: the newest `count` slots of hist hold it, oldest first. */
typedef struct Reading {
    float hist[HISTORY];
    int   count;
    bool  absent;                          /* the platform does not report it     */
} Reading;

typedef struct Device {
    RC_CpuSampler machineSince, appSince;  /* one per CPU reading, never shared   */
    Reading machineCpu;                    /* percent, every core busy = 100      */
    Reading machineMem;                    /* percent of the total in use         */
    Reading appCpu;                        /* percent of ONE core                 */
    Reading appMem;                        /* MiB resident                        */
    double  usedGiB, totalGiB, appPeakMiB; /* from the latest true memory answers */
    int     tries;                         /* spaced samples, up to DEVICE_TRIES  */
} Device;

/* The newest reading, meaningful once count > 0. */
static inline float reading_now(const Reading *r) {
    return r->hist[HISTORY - 1];
}

/* A true answer joins the history; a false one pushes nothing, so the last value stays
   on screen. @p settled: a false answer means the platform has no such reading. */
static inline void reading_take(Reading *r, bool ok, float v, bool settled) {
    if (ok) {
        hist_put(r->hist, -1, v);
        if (r->count < HISTORY) r->count++;
    }
    r->absent = r->count == 0 && settled;
}

/* One sample of the device, @p seconds after the one before. Each call is made exactly
   once, because a CPU reading's interval runs from the previous call with its own sampler.
   A call that answers false writes 0 to its figures, so a figure is kept only from a true
   answer. */
static inline void device_sample(Device *d, double seconds) {
    size_t total = 0, avail = 0, rss = 0, peak = 0;
    float  machine = 0.0f, app = 0.0f, memPct = 0.0f;
    bool   machineOk = rcMachineCpuPercent(&d->machineSince, &machine);
    bool   appOk     = rcProcessCpuPercent(&d->appSince, &app);
    bool   memOk     = rcMachineMemoryBytes(&total, &avail) && total > 0;
    bool   rssOk     = rcProcessMemoryBytes(&rss, &peak);

    /* A try counts only over a second or more, the smallest useful window on macOS
       (rayclay.h), so Refresh clicked twice in a hurry cannot judge a reading absent. */
    if (d->tries < DEVICE_TRIES && seconds >= 1.0) d->tries++;
    if (memOk) {
        /* "Available" is an estimate and is not promised to stay below the total; size_t
           arithmetic would turn one bad reading into eighteen exabytes in use. */
        size_t used = avail > total ? 0 : total - avail;

        d->usedGiB  = (double)used / 1073741824.0;
        d->totalGiB = (double)total / 1073741824.0;
        memPct      = (float)((double)used / (double)total * 100.0);
    }
    if (rssOk)
        d->appPeakMiB = (double)peak / 1048576.0;
    reading_take(&d->machineCpu, machineOk, machine, d->tries >= DEVICE_TRIES);
    reading_take(&d->machineMem, memOk, memPct, true);
    reading_take(&d->appCpu, appOk, app, d->tries >= DEVICE_TRIES);
    reading_take(&d->appMem, rssOk, (float)((double)rss / 1048576.0), true);
}

#endif /* APP_DEVICE_H */
