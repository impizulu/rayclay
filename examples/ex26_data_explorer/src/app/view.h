/*
    view.h - the model: what is filtered, what is derived, and the one pass that
    derives it

    Arithmetic over the orders: it selects rows, tallies them and orders them.
    Nothing here opens an element or reads a style, so you can change how this app
    filters or groups with app.h closed, and read every panel in app.h without
    opening this.

    A Tally is the whole trick. It holds sums, never a metric, so one scan answers
    all five metrics at once: picking another metric re-derives a few floats and
    scans nothing, and only a new filter or group-by has to touch the rows.
*/
#ifndef APP_VIEW_H
#define APP_VIEW_H

#include "rayclay.h"

#include "app/theme.h"
#include "app/orders.h"

typedef enum Metric {
    M_REVENUE = 0,
    M_ORDERS,
    M_AOV,
    M_UNITS,
    M_REFUNDS,
    M_COUNT
} Metric;

/* The table's columns, in the order they read left to right. */
typedef enum Col {
    C_ORDER = 0,
    C_DATE,
    C_CUSTOMER,
    C_PRODUCT,
    C_CATEGORY,
    C_STATUS,
    C_REGION,
    C_CHANNEL,
    C_PAYMENT,
    C_UNITS,
    C_DISCOUNT,
    C_REVENUE,
    C_COUNT
} Col;

typedef enum Interval { BY_DAY = 0, BY_WEEK, BY_MONTH } Interval;

typedef struct Tally {
    int64_t cents;
    int     orders;
    int     units;
    int     refunds;
} Tally;

typedef struct Filters {
    uint32_t pick[DIM_COUNT];   /* one bit per value; 0 means this facet is off   */
    int      dayFrom, dayTo;    /* inclusive                                      */
    float    minValue;          /* dollars                                        */
    float    maxValue;          /* VALUE_TOP means no limit                       */
    char     query[QUERY_CAP];  /* the query field's own buffer                   */
} Filters;

/** Everything derived from (orders, filters, group-by), rebuilt as one unit: if the
    charts and the row list could be refreshed separately, one day the chart would
    describe a selection the table is not showing. */
typedef struct View {
    int32_t  sel[ORDER_COUNT];      /* row indices that pass, in sort order        */
    int32_t  spare[ORDER_COUNT];    /* the merge sort's other half                 */
    int      count;

    Tally    total;
    int      rangeOrders;           /* every order in the date range, filtered or not */
    Tally    prior;                 /* the same filters over the period before     */
    bool     hasPrior;

    /* What each value would match if its own facet were off, the number a facet
       has to show: checking a second value adds that many, not nothing. */
    int      facet[DIM_COUNT][DIM_MAX];

    Interval interval;
    int      buckets;
    int      bucketDay[BUCKET_MAX + 1];       /* the day each bar starts on, then the end */
    const char *bucketLabel[BUCKET_MAX + 1];  /* the chart borrows these                  */
    const char *span;                         /* the x axis's title: the years shown      */
    Tally    bucket[BUCKET_MAX];
    Tally    context[BUCKET_MAX];   /* every order in range, filtered or not       */
    Tally    split[DIM_MAX][BUCKET_MAX];
    Tally    group[DIM_MAX];        /* as facet[]: the group-by facet left off     */

    int      groups;                /* values of the group-by, ranked by metric    */
    int      rank[DIM_MAX];
    float    groupMax;              /* the longest bar in the top list             */
    float    groupSum;
    /* What the tallies, the ranking and the arrays below were derived for. A control writes
       AppState's metric and group-by mid-frame, so what draws from the view reads these. */
    Metric   metric;
    int      groupDim;

    /* The arrays the charts read, re-derived whenever the metric moves. */
    float    trend[BUCKET_MAX];
    float    trendContext[BUCKET_MAX];
    float    splitY[SPLIT_MAX][BUCKET_MAX];
    float    spark[M_COUNT][BUCKET_MAX];
    float    hist[HIST_BINS];       /* order value, the value facet left off       */

    double   scanMs;                /* the last rebuild's own cost                 */
    int      rebuilds;
    RC_Arena labels;                /* the bars' names, replaced each rebuild      */
} View;

/** One metric out of a Tally. */
static inline float metric_value(const Tally *t, Metric m) {
    switch (m) {
    case M_REVENUE: return (float)t->cents / 100.0f;
    case M_ORDERS:  return (float)t->orders;
    case M_AOV:     return t->orders ? (float)t->cents / 100.0f / (float)t->orders : 0.0f;
    case M_UNITS:   return (float)t->units;
    default:        return t->orders ? 100.0f * (float)t->refunds / (float)t->orders : 0.0f;
    }
}

static inline void tally_add(Tally *t, const Order *o) {
    t->cents += o->cents;
    t->orders++;
    t->units += o->units;
    t->refunds += o->dim[DIM_STATUS] == STATUS_REFUNDED;
}

/** The value a row sorts on. Name tables are alphabetical, so an index sorts by name. */
static inline int32_t sort_value(const Order *o, int32_t row, Col col) {
    switch (col) {
    case C_ORDER:    return row;
    case C_DATE:     return row;    /* the array is in day order, and a day's ties follow */
    case C_CUSTOMER: return o->first * FIRST_NAMES + o->last;
    case C_PRODUCT:  return o->dim[DIM_PRODUCT];
    case C_CATEGORY: return o->dim[DIM_CATEGORY];
    case C_STATUS:   return o->dim[DIM_STATUS];
    case C_REGION:   return o->dim[DIM_REGION];
    case C_CHANNEL:  return o->dim[DIM_CHANNEL];
    case C_PAYMENT:  return o->dim[DIM_PAYMENT];
    case C_UNITS:    return o->units;
    case C_DISCOUNT: return o->discount;
    default:         return o->cents;
    }
}

/** Order the selection, bottom-up merge sort. YOUR app should just call qsort; these
    examples use no libc, so one had to be written, and a merge is stable: equal rows
    keep the order they had, which is what a second click on another header expects. */
static inline void view_sort(View *v, const OrderBook *b, Col col, bool desc) {
    int32_t *from = v->sel, *to = v->spare, *swap;
    int      width, i;

    for (width = 1; width < v->count; width *= 2) {
        for (i = 0; i < v->count; i += 2 * width) {
            int mid = i + width < v->count ? i + width : v->count;
            int end = i + 2 * width < v->count ? i + 2 * width : v->count;
            int l = i, r = mid, at = i;

            while (l < mid && r < end) {
                int32_t a = sort_value(&b->row[from[l]], from[l], col);
                int32_t c = sort_value(&b->row[from[r]], from[r], col);
                bool    takeRight = desc ? c > a : c < a;

                to[at++] = takeRight ? from[r++] : from[l++];
            }
            while (l < mid)
                to[at++] = from[l++];
            while (r < end)
                to[at++] = from[r++];
        }
        swap = from;
        from = to;
        to   = swap;
    }
    if (from != v->sel) {
        for (i = 0; i < v->count; i++)
            v->sel[i] = from[i];
    }
}

static inline char fold_char(char ch) {
    return ch >= 'A' && ch <= 'Z' ? (char)(ch - 'A' + 'a') : ch;
}

/** Does @p text contain @p needle, ignoring case? Needle already folded. */
static inline bool text_has(const char *text, const char *needle) {
    const char *t, *h, *n;

    for (t = text; *t; t++) {
        for (h = t, n = needle; *h && *n && fold_char(*h) == *n; h++, n++) {}
        if (!*n)
            return true;
    }
    return false;
}

/** Does @p text start with @p needle, ignoring case? Needle already folded. */
static inline bool text_starts(const char *text, const char *needle) {
    for (; *needle; text++, needle++) {
        if (fold_char(*text) != *needle)
            return false;
    }
    return true;
}

/** The dimension a query is on its way to naming, or -1 for free text: region:west, and
    every step of typing it from the first letter. That text is a filter, not a search, so
    the scan leaves it out and the result stays put until Enter makes the chip. */
static inline int query_dim(const char *query) {
    char key[QUERY_CAP];
    int  n = 0, d;

    while (query[n] && query[n] != ':') {
        key[n] = fold_char(query[n]);
        n++;
    }
    key[n] = '\0';
    for (d = 0; n > 0 && d < DIM_COUNT; d++) {
        if (text_starts(DIM_KEY[d], key))
            return d;
    }
    return -1;
}

/** The query's free text against one row: the customer's whole name, the product,
    the order number. The name is spelled into a buffer so "maya pa" matches across
    the space; nothing here reaches RayClay, so a stack buffer is fine. */
static inline bool row_matches(const Order *o, int32_t row, const char *needle) {
    char        name[32], digits[8];
    const char *part;
    int         n = 0, id = ORDER_FIRST_ID + row, d = 6;

    for (part = FIRST_NAME[o->first]; *part && n < 15; part++)
        name[n++] = *part;
    name[n++] = ' ';
    for (part = LAST_NAME[o->last]; *part && n < 30; part++)
        name[n++] = *part;
    name[n] = '\0';
    if (text_has(name, needle) || text_has(PRODUCT[o->dim[DIM_PRODUCT]].name, needle))
        return true;

    digits[d] = '\0';
    while (d > 0) {
        digits[--d] = (char)('0' + id % 10);
        id /= 10;
    }
    return text_has(digits, needle);
}

/** The bar a day falls in. Only the three intervals' arithmetic, kept in one place
    so the scan and the labels cannot disagree. */
static inline int bucket_of(const OrderBook *b, const View *v, const Filters *f, int day) {
    switch (v->interval) {
    case BY_DAY:   return day - f->dayFrom;
    case BY_WEEK:  return (day - f->dayFrom) / 7;
    default:       return b->monthOf[day] - b->monthOf[f->dayFrom];
    }
}

/** Choose the interval from the range, as Datadog's rollup does: a click that narrows
    the dates to one bar therefore redraws that bar as its own days, which is drill-down
    with no drill-down code. Then name the bars. */
static inline void plan_buckets(View *v, const OrderBook *b, const Filters *f) {
    int span = f->dayTo - f->dayFrom + 1, day;

    v->interval = span <= 62 ? BY_DAY : span <= 400 ? BY_WEEK : BY_MONTH;
    v->buckets  = 0;
    rcArenaReset(&v->labels);
    for (day = f->dayFrom; day <= f->dayTo; day++) {
        int i = bucket_of(b, v, f, day);

        if (i == v->buckets) {
            int y, m, d;

            day_to_date(day, &y, &m, &d);
            v->bucketDay[i] = day;
            if (v->interval == BY_MONTH)
                v->bucketLabel[i] = rcFormat(&v->labels, "%s %d", MONTH_NAME[m], y).chars;
            else
                v->bucketLabel[i] = rcFormat(&v->labels, "%s %d", MONTH_NAME[m], d).chars;
            v->buckets++;
        }
    }
    v->bucketDay[v->buckets]   = f->dayTo + 1;
    v->bucketLabel[v->buckets] = NULL;
    /* Day and week labels leave the year out, so the axis's title names it. It is the
       same at every width, where the ticks a width can hold are not. */
    {
        int y0, m0, d0, y1, m1, d1;

        day_to_date(f->dayFrom, &y0, &m0, &d0);
        day_to_date(f->dayTo, &y1, &m1, &d1);
        v->span = v->interval == BY_MONTH ? NULL
                : y0 == y1 ? rcFormat(&v->labels, "%d", y0).chars
                : rcFormat(&v->labels, "%s %d to %s %d",
                           MONTH_NAME[m0], y0, MONTH_NAME[m1], y1).chars;
    }
}

/** Everything a metric change has to redo, and nothing more: floats out of tallies,
    and the top list's ranking. */
static inline void view_derive(View *v, Metric metric, int groupDim) {
    int size = dim_size(groupDim), i, j, m;

    v->metric   = metric;
    v->groupDim = groupDim;
    v->groups   = size;
    v->groupSum = 0.0f;
    v->groupMax = 0.0f;
    for (i = 0; i < size; i++) {
        float value = metric_value(&v->group[i], metric);
        int   at = i;

        /* An insertion sort: at most 32 values. */
        while (at > 0 && metric_value(&v->group[v->rank[at - 1]], metric) < value) {
            v->rank[at] = v->rank[at - 1];
            at--;
        }
        v->rank[at] = i;
        v->groupSum += value;
        if (value > v->groupMax)
            v->groupMax = value;
    }
    for (i = 0; i < v->buckets; i++) {
        v->trend[i]        = metric_value(&v->bucket[i], metric);
        v->trendContext[i] = metric_value(&v->context[i], metric);
        for (j = 0; j < SPLIT_MAX && j < size; j++)
            v->splitY[j][i] = metric_value(&v->split[v->rank[j]][i], metric);
        for (m = 0; m < M_COUNT; m++)
            v->spark[m][i] = metric_value(&v->bucket[i], (Metric)m);
    }
}

/** The one scan, in one place. Call it when a control moves, not every frame.

    One pass answers every panel because each row first counts HOW MANY filters it
    misses. Zero: it is in the selection and every panel counts it. Exactly one: it
    is out, but the facet that excluded it still counts it, so a facet shows what
    checking that value would add. Two or more: nobody counts it. */
static inline void view_rebuild(View *v, const OrderBook *b, const Filters *f,
                                int groupDim, Metric metric, Col col, bool desc) {
    enum { MISS_VALUE = DIM_COUNT };
    const Tally none = { 0 };
    double started = rcMonotonicSeconds();
    char   needle[QUERY_CAP];
    int    priorFrom, i, d, nn = 0;

    for (i = 0; query_dim(f->query) < 0 && f->query[i] && nn < QUERY_CAP - 1; i++)
        needle[nn++] = fold_char(f->query[i]);
    needle[nn] = '\0';

    plan_buckets(v, b, f);
    priorFrom   = f->dayFrom - (f->dayTo - f->dayFrom + 1);
    v->hasPrior = priorFrom >= 0;
    v->count = 0;
    v->rangeOrders = 0;
    v->total = none;
    v->prior = none;
    for (d = 0; d < DIM_COUNT; d++) {
        for (i = 0; i < DIM_MAX; i++)
            v->facet[d][i] = 0;
    }
    for (i = 0; i < DIM_MAX; i++) {
        int k;

        v->group[i] = none;
        for (k = 0; k < v->buckets; k++)
            v->split[i][k] = none;
    }
    for (i = 0; i < v->buckets; i++) {
        v->bucket[i]  = none;
        v->context[i] = none;
    }
    for (i = 0; i < HIST_BINS; i++)
        v->hist[i] = 0.0f;

    for (i = 0; i < ORDER_COUNT; i++) {
        const Order *o = &b->row[i];
        float dollars  = (float)o->cents / 100.0f;
        bool  inRange  = o->day >= f->dayFrom && o->day <= f->dayTo;
        bool  inPrior  = v->hasPrior && o->day >= priorFrom && o->day < f->dayFrom;
        int   misses   = 0, missed = -1, k;

        if (!inRange && !inPrior)
            continue;
        if (inRange) {
            tally_add(&v->context[bucket_of(b, v, f, o->day)], o);
            v->rangeOrders++;
        }
        /* First despite its cost: a query has no facet to count its misses, so a row it
           rejects is out of every panel and the other tests can be skipped. */
        if (nn && !row_matches(o, i, needle))
            continue;

        for (d = 0; d < DIM_COUNT; d++) {
            if (f->pick[d] && !((f->pick[d] >> o->dim[d]) & 1u)) {
                misses++;
                missed = d;
            }
        }
        if (dollars < f->minValue ||
            (f->maxValue < (float)VALUE_TOP && dollars > f->maxValue)) {
            misses++;
            missed = MISS_VALUE;
        }

        if (!inRange) {
            if (misses == 0)
                tally_add(&v->prior, o);
            continue;
        }
        if (misses > 1)
            continue;

        if (misses == 0 || missed == MISS_VALUE) {
            k = (int)(dollars * (float)HIST_BINS / (float)VALUE_TOP);
            v->hist[k < HIST_BINS ? k : HIST_BINS - 1] += 1.0f;
        }
        if (misses == 1) {
            if (missed != MISS_VALUE) {
                v->facet[missed][o->dim[missed]]++;
                if (missed == groupDim)
                    tally_add(&v->group[o->dim[groupDim]], o);
            }
            continue;
        }

        v->sel[v->count++] = i;
        tally_add(&v->total, o);
        k = bucket_of(b, v, f, o->day);
        tally_add(&v->bucket[k], o);
        tally_add(&v->group[o->dim[groupDim]], o);
        tally_add(&v->split[o->dim[groupDim]][k], o);
        for (d = 0; d < DIM_COUNT; d++)
            v->facet[d][o->dim[d]]++;
    }

    view_derive(v, metric, groupDim);
    view_sort(v, b, col, desc);
    v->scanMs = (rcMonotonicSeconds() - started) * 1000.0;
    /* The note asks only whether a rebuild followed the first: counting stops at two, so a
       slider dragged for a long session cannot overflow it. */
    if (v->rebuilds < 2)
        v->rebuilds++;
}

#endif /* APP_VIEW_H */
