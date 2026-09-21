/*
================================================================================
    view.h - the model: what is filtered, what is derived, and the one pass
    that derives it
================================================================================
    Arithmetic over the catalogue: it selects rows, orders them, thins them and
    tallies them. Nothing here opens an element, reads a style or names an RC_
    type, so you can change how this app filters, sorts or samples with app.h
    closed, and read every panel in app.h without opening this.
================================================================================
*/
#ifndef APP_VIEW_H
#define APP_VIEW_H

#include "rayclay.h"

#include "app/theme.h"
#include "app/catalog.h"

typedef enum SortKey {
    K_NAME = 0, K_METHOD, K_YEAR, K_RADIUS, K_PERIOD, K_DISTANCE, K_COUNT
} SortKey;

typedef struct Filters {
    bool  method[CAT_METHODS];
    float sinceYear;    /* discovered in this year or later                  */
    float maxDistance;  /* parsecs; the slider's top stop means "no limit"   */
    float minRadius;    /* Earth radii                                       */
    float maxRadius;
} Filters;

/** Everything derived from (catalogue, filters, sort), rebuilt as ONE unit: if
    the chart arrays and the row list can be refreshed separately, one day the
    chart will describe a selection the table is not showing. */
typedef struct View {
    int32_t sel[CAT_COUNT];   /* catalogue indices that pass, in sort order   */
    int     count;

    float   yearX[CAT_YEARS]; /* shared x for both year series                */
    float   yearAll[CAT_YEARS];
    float   yearSel[CAT_YEARS];

    float   histX[HIST_BINS];
    float   histSel[HIST_BINS];

    float   scatterX[SCATTER_MAX];
    float   scatterY[SCATTER_MAX];
    /* The catalogue row each plotted marker came from. A clickable plot has to
       answer "which planet is this dot", and the sample's own index cannot:
       marker k is not row k of anything. */
    int32_t scatterIdx[SCATTER_MAX];
    int     scatterCount;
    int     scatterStride;    /* 1 = every point; >1 = a sample              */
    int     scatterOutside;   /* passed the filter, off the plotted window   */

    int     byMethod[CAT_METHODS];
    float   meanRadius;
    float   meanDistance;
    int     nearest;          /* catalogue index of the closest match, or -1 */

    /* The farthest row in the catalogue, so the distance slider's top stop is
       the data's own maximum. A hard-coded stop either excludes rows nobody
       asked to exclude or wastes most of the track. */
    float   distanceMax;

    int     rebuilds;         /* how many times view_rebuild has run         */
} View;

/** The value a row sorts on. One float per key keeps the ORDER in one place:
    add a column and there is exactly one function to extend. Sorting by name is
    sorting by (survey, host number), which is what the designation reads as and
    avoids building 12,000 strings; both parts fit a float exactly. */
static inline float sort_value(const CatPlanet *p, SortKey key)
{
    switch (key) {
    case K_NAME:     return (float)(p->survey * 10000 + p->host);
    case K_METHOD:   return (float)p->method;
    case K_YEAR:     return (float)p->year;
    case K_RADIUS:   return p->radius;
    case K_PERIOD:   return p->period;
    default:         return p->distance;
    }
}

/** Sift `hole` down a max-heap of `n` indices: the whole of heapsort, called
    from two places in view_sort. */
static inline void heap_sift(int32_t *a, int n, int hole, const Catalog *c,
                      SortKey key, bool desc)
{
    int32_t held = a[hole];
    float   heldV = sort_value(&c->row[held], key);

    for (;;) {
        int child = 2 * hole + 1;
        float childV, rightV;

        if (child >= n)
            break;
        childV = sort_value(&c->row[a[child]], key);
        if (child + 1 < n) {
            rightV = sort_value(&c->row[a[child + 1]], key);
            /* Descending sorts build a MIN-heap, so one routine serves both
               directions and the two orders cannot drift apart. */
            if (desc ? rightV < childV : rightV > childV) {
                child++;
                childV = rightV;
            }
        }
        if (desc ? childV >= heldV : childV <= heldV)
            break;
        a[hole] = a[child];
        hole = child;
    }
    a[hole] = held;
}

/** Order the selection in place. Heapsort, because these examples use no libc
    - YOUR app should just call qsort; nothing about RayClay asks you to write a
    sort. Given one had to be written, heapsort allocates nothing, never
    recurses, and its O(n log n) is a worst case. */
static inline void view_sort(View *v, const Catalog *c, SortKey key, bool desc)
{
    int i;

    for (i = v->count / 2 - 1; i >= 0; i--)
        heap_sift(v->sel, v->count, i, c, key, desc);

    for (i = v->count - 1; i > 0; i--) {
        int32_t top = v->sel[0];

        v->sel[0] = v->sel[i];
        v->sel[i] = top;
        heap_sift(v->sel, i, 0, c, key, desc);
    }
}

/** Take up to SCATTER_MAX points off the selection, evenly strided.
 *
 *  A scatter marker is a disc and a disc is a triangle fan, so 12,000 of them
 *  is tens of thousands of vertices in one frame - past the per-frame budget,
 *  and an over-budget frame is refused WHOLE: a blank window, not a clipped
 *  scene you might mistake for the data. A scatter communicates DENSITY, and a
 *  uniform sample has the same density everywhere the full cloud does.
 *
 *  Strided, not "the first 1,200": the selection is still in catalogue order
 *  here, so a prefix would show whichever rows were generated first. Points
 *  outside the pinned window are counted, never clamped - clamping piles a
 *  false ridge of markers onto the plot edge.
 */
static inline void plot_scatter(View *v, const Catalog *c)
{
    int i;

    /* CEILING division: a floor overruns SCATTER_MAX for every count that is
       not an exact multiple, and the plot would drop the tail of the run. */
    v->scatterStride  = v->count > SCATTER_MAX
                      ? (v->count + SCATTER_MAX - 1) / SCATTER_MAX : 1;
    v->scatterCount   = 0;
    v->scatterOutside = 0;

    for (i = 0; i < v->count; i += v->scatterStride) {
        const CatPlanet *p = &c->row[v->sel[i]];

        if (p->period > (float)SCATTER_P || p->radius > (float)SCATTER_R) {
            v->scatterOutside++;
            continue;
        }
        if (v->scatterCount >= SCATTER_MAX)
            break;
        v->scatterX[v->scatterCount]   = p->period;
        v->scatterY[v->scatterCount]   = p->radius;
        v->scatterIdx[v->scatterCount] = v->sel[i];
        v->scatterCount++;
    }
}

/** Everything derived, in one pass and one place. CALL THIS WHEN A CONTROL
    MOVES, NOT EVERY FRAME: one pass fills the selection, both chart series, the
    tally and the means, then the sample and the sort, in that order. */
static inline void view_rebuild(View *v, const Catalog *c, const Filters *f,
                         SortKey key, bool desc)
{
    double sumR = 0.0, sumD = 0.0;
    float  nearestD = 0.0f;
    int    i;

    v->count   = 0;
    v->nearest = -1;
    for (i = 0; i < CAT_YEARS; i++)
        v->yearSel[i] = 0.0f;
    for (i = 0; i < HIST_BINS; i++)
        v->histSel[i] = 0.0f;
    for (i = 0; i < CAT_METHODS; i++)
        v->byMethod[i] = 0;

    for (i = 0; i < CAT_COUNT; i++) {
        const CatPlanet *p = &c->row[i];
        int bin;

        if (!f->method[p->method])
            continue;
        if ((float)p->year < f->sinceYear)
            continue;
        if (p->distance > f->maxDistance)
            continue;
        if (p->radius < f->minRadius || p->radius > f->maxRadius)
            continue;

        v->sel[v->count++] = i;
        v->yearSel[p->year - CAT_YEAR_FIRST] += 1.0f;
        v->byMethod[p->method]++;
        sumR += (double)p->radius;
        sumD += (double)p->distance;
        if (v->nearest < 0 || p->distance < nearestD) {
            v->nearest = i;
            nearestD   = p->distance;
        }

        /* The top bin is half-open upward, so a giant above the histogram's
           span lands somewhere visible instead of being silently dropped. */
        bin = (int)(p->radius * (float)HIST_BINS / (float)HIST_MAX_R);
        if (bin < 0)
            bin = 0;
        if (bin >= HIST_BINS)
            bin = HIST_BINS - 1;
        v->histSel[bin] += 1.0f;
    }

    v->meanRadius   = v->count ? (float)(sumR / v->count) : 0.0f;
    v->meanDistance = v->count ? (float)(sumD / v->count) : 0.0f;

    plot_scatter(v, c);
    view_sort(v, c, key, desc);
    v->rebuilds++;
}

/** The parts of the view that never change: the chart x axes, and the "all
    12,000" context series the year chart draws behind the selection. */
static inline void view_init(View *v, const Catalog *c)
{
    int i;

    for (i = 0; i < CAT_YEARS; i++) {
        v->yearX[i]   = (float)(CAT_YEAR_FIRST + i);
        v->yearAll[i] = 0.0f;
    }
    v->distanceMax = 0.0f;
    for (i = 0; i < CAT_COUNT; i++) {
        v->yearAll[c->row[i].year - CAT_YEAR_FIRST] += 1.0f;
        if (c->row[i].distance > v->distanceMax)
            v->distanceMax = c->row[i].distance;
    }

    /* Bin centres, so a bar sits over the range it counts rather than starting
       at it. */
    for (i = 0; i < HIST_BINS; i++)
        v->histX[i] = ((float)i + 0.5f) * (float)HIST_MAX_R / (float)HIST_BINS;
}

static inline void filters_reset(Filters *f, float distanceMax)
{
    int i;

    for (i = 0; i < CAT_METHODS; i++)
        f->method[i] = true;
    f->sinceYear   = (float)CAT_YEAR_FIRST;
    f->maxDistance = distanceMax;
    f->minRadius   = 0.0f;
    f->maxRadius   = (float)HIST_MAX_R;
}

#endif /* APP_VIEW_H */
