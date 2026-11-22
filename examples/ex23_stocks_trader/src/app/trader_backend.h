/*
    trader_backend.h - the market and the brokerage account, with no RayClay in it.

    A PRICE IS A PURE FUNCTION OF (INSTRUMENT, TIME). A sum of noise octaves, each
    twice as long and sqrt(2) times as tall as the last, gives a walk with no stored
    history: any bar at any interval is sampled straight from it, so a 5 minute chart
    and a daily chart of one stock always agree, the newest close is always the live
    quote, and nothing depends on a wall clock or on rand(). Time is seconds of
    trading time, sessions laid end to end, on a calendar of weekdays.

    The account is real double-entry: every fill moves cash, shares and the average
    cost, a sale books realized P&L, and the seeded portfolio is built by replaying
    trades through that same path, so the numbers on screen add up from the first
    frame. Prices are integer cents, and every number the UI shows is formatted here,
    into a buffer the caller passes with its size: the backend allocates nothing.

    Single-header: define TRADER_BACKEND_IMPLEMENTATION in exactly one TU before
    including it.
*/
#ifndef TRADER_BACKEND_H
#define TRADER_BACKEND_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef TRDEF
#define TRDEF static
#endif

#define TR_MAX_INSTRUMENTS 24
#define TR_MAX_BARS       480      /* the series kept for the open chart              */
#define TR_SPARK_N         32      /* one session, sampled, for a watchlist trend     */
#define TR_BOOK_LEVELS      8      /* order book depth per side                       */
#define TR_MAX_WORKING     16      /* resting limit and stop orders                   */
#define TR_MAX_FILLS       64      /* the fill log keeps the newest                   */
#define TR_EQUITY_N       180      /* the account's equity curve, across its span     */
#define TR_SESSION_SEC  23400      /* 09:30 to 16:00 New York                         */
#define TR_TICKS_PER_SEC    4      /* the feed's resolution                           */
#define TR_NONE            (-1)    /* an indicator with no value yet                  */

typedef enum { TR_BUY = 0, TR_SELL } TrSide;
typedef enum { TR_MARKET = 0, TR_LIMIT, TR_STOP } TrOrderType;

/* Every interval divides a session evenly, so each session starts a new bar. */
typedef enum {
    TR_IV_1M = 0, TR_IV_5M, TR_IV_15M, TR_IV_30M, TR_IV_1D, TR_IV_1W, TR_IV_COUNT
} TrInterval;

/* The indicator lines the chart can show, one array each, in cents; RSI in
   hundredths of a percent. */
typedef enum {
    TR_SER_SMA20 = 0, TR_SER_EMA50, TR_SER_BB_UP, TR_SER_BB_LO, TR_SER_RSI, TR_SER_COUNT
} TrSeries;

typedef struct {
    int32_t open, high, low, close, volume;
} TrBar;

typedef struct {
    char    symbol[8];
    char    name[28];
    char    exchange[8];
    int32_t list;                /* the price the walk is scaled to at the start time */
    int32_t volPct;              /* this stock's volatility, percent of the model's   */
    int32_t driftE6;             /* its trend per session, millionths of its price    */
    int64_t norm;                /* the walk's value at the start time                */
    uint32_t key;                /* its octaves' hash key                             */
    int32_t baseVol;             /* shares traded per minute on an average day        */

    int32_t price;               /* the live quote, cents                             */
    int32_t prevClose;           /* the last session's close                          */
    int32_t change;              /* price - prevClose                                 */
    int32_t changeBps;
    int32_t shares;              /* whole shares held; this account never goes short  */
    int32_t avgCost;
    int64_t pl;                  /* unrealized P&L                                    */
    int32_t plBps;
    float   flash;               /* 1 after an up tick, -1 after a down tick, decays  */
    float   spark[TR_SPARK_N];   /* the last session, ending at the live quote        */

    char    priceStr[12];
    char    changeStr[12];
    char    changePctStr[12];
    char    sharesStr[12];
    char    avgStr[12];
    char    valueStr[20];
    char    plStr[20];
    char    plPctStr[12];
} TrInstrument;

typedef struct {
    int32_t price, size;
    char    priceStr[12];
    char    sizeStr[10];
} TrLevel;

/* An order resting in the book until the price reaches it. */
typedef struct {
    int32_t inst;
    int32_t side;
    int32_t type;
    int32_t qty;
    int32_t price;
    int32_t tag;                 /* its own number below TR_MAX_WORKING while it rests */
    int64_t placed;              /* seconds                                           */
    char    qtyStr[12];
    char    priceStr[12];
    char    placedStr[12];
    char    label[24];           /* "BUY LMT 50"                                      */
} TrWorking;

typedef struct {
    int32_t inst;
    int32_t side;
    int32_t type;
    int32_t qty;
    int32_t price;
    int64_t realized;
    int64_t time;                /* seconds                                           */
    int32_t tag;                 /* its own number below TR_MAX_FILLS while it is kept */
    char    qtyStr[12];
    char    priceStr[12];
    char    timeStr[20];
    char    valueStr[20];
    char    realizedStr[20];
} TrFill;

typedef struct {
    TrInstrument inst[TR_MAX_INSTRUMENTS];
    int32_t      count;
    int32_t      selected;

    double       clock;          /* seconds this run has simulated                    */
    int32_t      speed;          /* market seconds per real second                    */
    int64_t      tick;
    int64_t      now;            /* seconds                                           */
    int64_t      day;            /* the trading day of now                            */
    char         clockStr[12];   /* "12:42:07"                                        */
    char         dateStr[20];    /* "Thu Nov 5 2026"                                  */

    /* The open chart: the selected instrument at the selected interval. */
    int32_t      interval;
    int32_t      barCount;
    int64_t      firstBar;       /* bars[0]'s index on this interval's grid           */
    TrBar        bars[TR_MAX_BARS];
    int32_t      series[TR_SER_COUNT][TR_MAX_BARS];

    TrLevel      bids[TR_BOOK_LEVELS];
    TrLevel      asks[TR_BOOK_LEVELS];
    int32_t      spread;
    char         spreadStr[12];

    TrWorking    working[TR_MAX_WORKING];
    int32_t      workingCount;
    TrFill       fills[TR_MAX_FILLS];
    int32_t      fillCount;
    uint32_t     fillSerial;     /* fills ever logged: the kept ones are consecutive  */

    int64_t      deposit;        /* the money the account opened with                 */
    int64_t      cash;
    int64_t      realized;
    int64_t      unrealized;
    int64_t      dayPl;
    int64_t      equity;
    int32_t      holdings;
    char         cashStr[20];
    char         equityStr[20];
    char         unrealizedStr[20];
    char         realizedStr[20];
    char         dayPlStr[20];

    float        equityCurve[TR_EQUITY_N];
    int32_t      equityStep;     /* seconds between its samples: its span / (N - 1)   */
    int64_t      equityStart;    /* the curve's first sample, seconds                 */
    int64_t      equityOpen;     /* the account at equityStart, cents                 */
    int64_t      lastSample;     /* the newest sample's grid index                    */

    /* The last thing the account did, for the UI to announce. */
    char         event[64];
    uint32_t     eventSeq;
} TrStore;

/* -- queries ---------------------------------------------------------------- */
static inline int tr_count(const TrStore *s) {
    return s->count;
}

static inline const TrInstrument *tr_at(const TrStore *s, int i) {
    return (i >= 0 && i < s->count) ? &s->inst[i] : NULL;
}

static inline const TrInstrument *tr_selected(const TrStore *s) {
    return tr_at(s, s->selected);
}

#ifdef TRADER_BACKEND_IMPLEMENTATION

#include <string.h>

/* The seconds each bar spans, indexed by TrInterval. */
static const int32_t TR_IV_SEC[TR_IV_COUNT] = {
    60, 300, 900, 1800, TR_SESSION_SEC, 5 * TR_SESSION_SEC,
};

/* The run opens on Thursday Nov 5 2026, 12:42 New York: trading day 523 counted from
   Monday Nov 4 2024, three hours into the session, so the first chart has a morning. */
#define TR_EPOCH_DAYS 20031            /* days from 1970 to Nov 4 2024 */
#define TR_START_SEC  (523 * (int64_t)TR_SESSION_SEC + 11520)

/* -- formatting ------------------------------------------------------------- */

/* Unsigned integer to text; returns the count written. */
TRDEF int tr_fmt_uint(uint64_t v, char *out, int cap) {
    char tmp[24];
    int n = 0;

    do {
        tmp[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v && n < 23);
    if (n > cap - 1)
        n = cap - 1;
    for (int i = 0; i < n; i++)
        out[i] = tmp[n - 1 - i];
    out[n] = '\0';
    return n;
}

/* Copy src after the text already in dst, never past cap. */
TRDEF void tr_cat(char *dst, int cap, const char *src) {
    int k = (int)strlen(dst);

    while (*src && k < cap - 1)
        dst[k++] = *src++;
    dst[k] = '\0';
}

/* Whole number and two decimals, the sign first when it is asked for. A price is a
   number and a balance is money: `dollar` and `group` make the second from the first. */
static int tr_fmt_money(int64_t cents, char *out, int cap, bool plus, bool dollar, bool group) {
    char digits[24];
    int k = 0;
    uint64_t a = (uint64_t)(cents < 0 ? -cents : cents);
    int n = tr_fmt_uint(a / 100u, digits, (int)sizeof digits);

    if (cents < 0 && k < cap - 1)
        out[k++] = '-';
    if (cents > 0 && plus && k < cap - 1)
        out[k++] = '+';
    if (dollar && k < cap - 1)
        out[k++] = '$';
    for (int i = 0; i < n && k < cap - 1; i++) {
        if (group && i && (n - i) % 3 == 0 && k < cap - 1)
            out[k++] = ',';
        out[k++] = digits[i];
    }
    if (k < cap - 3) {
        out[k++] = '.';
        out[k++] = (char)('0' + a / 10u % 10u);
        out[k++] = (char)('0' + a % 10u);
    }
    out[k] = '\0';
    return k;
}

/* "184.92": a price, as a quote column shows it. */
TRDEF int tr_fmt_price(int64_t cents, char *out, int cap) {
    return tr_fmt_money(cents, out, cap, false, false, false);
}

/* "+1.86": a move in a price. */
TRDEF int tr_fmt_move(int64_t cents, char *out, int cap) {
    return tr_fmt_money(cents, out, cap, true, false, false);
}

/* "$1,074,091.75": a balance. */
TRDEF int tr_fmt_cash(int64_t cents, char *out, int cap) {
    return tr_fmt_money(cents, out, cap, false, true, true);
}

/* "1,074,091.75": an amount in a currency named beside it. */
TRDEF int tr_fmt_amount(int64_t cents, char *out, int cap) {
    return tr_fmt_money(cents, out, cap, false, false, true);
}

/* "+$1,240.50": a gain or a loss. */
TRDEF int tr_fmt_pl(int64_t cents, char *out, int cap) {
    return tr_fmt_money(cents, out, cap, true, true, true);
}

/* Basis points of a base, rounded half away from zero, as a quote screen's percent is. */
TRDEF int32_t tr_bps(int64_t move, int64_t base) {
    return base > 0 ? (int32_t)((move * 10000 + (move < 0 ? -base : base) / 2) / base) : 0;
}

/* "+1.23%" from basis points. A signed percent always carries its sign. */
TRDEF int tr_fmt_bps(int64_t bps, char *out, int cap) {
    int k = tr_fmt_money(bps, out, cap, true, false, false);

    if (k < cap - 1) {
        out[k++] = '%';
        out[k] = '\0';
    }
    return k;
}

/* "12.4M": a volume column is read for its magnitude. */
TRDEF int tr_fmt_volume(int64_t v, char *out, int cap) {
    static const struct { int64_t unit; char tag; } TIERS[] = {
        { 1000000000, 'B' }, { 1000000, 'M' }, { 1000, 'K' },
    };

    for (int t = 0; t < 3; t++) {
        if (v < TIERS[t].unit)
            continue;
        int64_t tenths = v * 10 / TIERS[t].unit;
        int k = tr_fmt_uint((uint64_t)(tenths / 10), out, cap);
        if (tenths < 1000 && k < cap - 2) {
            out[k++] = '.';
            out[k++] = (char)('0' + tenths % 10);
        }
        if (k < cap - 1)
            out[k++] = TIERS[t].tag;
        out[k] = '\0';
        return k;
    }
    return tr_fmt_uint((uint64_t)(v > 0 ? v : 0), out, cap);
}

/* -- the calendar: trading seconds to dates and clock times ------------------ */

/* A civil date from days since 1970 (Howard Hinnant's algorithm). */
static void tr_civil(int64_t z, int *y, int *m, int *d) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp  = (5 * doy + 2) / 153;

    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yoe + era * 400 + (*m <= 2));
}

static const char *const TR_MONTHS[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};
/* A trading day is a weekday, TR_DAYS[day % 5]; the device's own week reads all seven. */
static const char *const TR_DAYS[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };

/* The clock of a trading second: HH:MM, with :SS when asked for. */
static void tr_fmt_clock(int64_t sec, bool seconds, char *out, int cap) {
    const int inDay = (int)(sec % TR_SESSION_SEC) + 9 * 3600 + 30 * 60;
    const int f[3] = { inDay / 3600, inDay / 60 % 60, inDay % 60 };
    int k = 0;

    for (int i = 0; i < (seconds ? 3 : 2) && k + 3 < cap; i++) {
        if (i)
            out[k++] = ':';
        out[k++] = (char)('0' + f[i] / 10);
        out[k++] = (char)('0' + f[i] % 10);
    }
    out[k] = '\0';
}

/* "Nov 5", the name a day's first bar gets on the time axis. */
TRDEF void tr_fmt_day(int64_t sec, char *out, int cap) {
    const int64_t day = sec / TR_SESSION_SEC;
    int y, m, d;

    tr_civil(TR_EPOCH_DAYS + day / 5 * 7 + day % 5, &y, &m, &d);
    out[0] = '\0';
    tr_cat(out, cap, TR_MONTHS[m - 1]);
    tr_cat(out, cap, " ");
    tr_fmt_uint((uint64_t)d, out + strlen(out), cap - (int)strlen(out));
}

/* "12:45" within a day. */
TRDEF int tr_fmt_time(int64_t sec, char *out, int cap) {
    tr_fmt_clock(sec, false, out, cap);
    return (int)strlen(out);
}

/* "Thu Nov 5 '26 12:45", the crosshair's reading; a daily bar has no clock. */
TRDEF void tr_fmt_stamp(int64_t sec, int ivSec, char *out, int cap) {
    const int64_t day = sec / TR_SESSION_SEC;
    int y, m, d;

    tr_civil(TR_EPOCH_DAYS + day / 5 * 7 + day % 5, &y, &m, &d);
    out[0] = '\0';
    tr_cat(out, cap, TR_DAYS[day % 5]);
    tr_cat(out, cap, " ");
    tr_cat(out, cap, TR_MONTHS[m - 1]);
    tr_cat(out, cap, " ");
    tr_fmt_uint((uint64_t)d, out + strlen(out), cap - (int)strlen(out));
    tr_cat(out, cap, " '");
    tr_fmt_uint((uint64_t)(y % 100), out + strlen(out), cap - (int)strlen(out));
    if (ivSec < TR_SESSION_SEC) {
        char clock[8];
        tr_fmt_clock(sec, false, clock, (int)sizeof clock);
        tr_cat(out, cap, " ");
        tr_cat(out, cap, clock);
    }
}

/* -- the price model --------------------------------------------------------- */

/* A stateless integer hash of two words. */
static uint32_t tr_mix(uint32_t a, uint32_t b) {
    uint32_t x = a * 0x9e3779b1u ^ b;

    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

/* The octaves' heights in millionths of the price, from a period of one second up to
   about 90 sessions. Each is sqrt(2) taller than the last, which is what makes the sum
   walk like a market at every zoom. */
#define TR_OCTAVES 22
static const int32_t TR_AMP[TR_OCTAVES] = {
    125, 177, 250, 354, 500, 707, 1000, 1414, 2000, 2828, 4000,
    5657, 8000, 11314, 16000, 22627, 32000, 45255, 64000, 90510, 128000, 181019,
};

/* The walk at a tick, in millionths. Smooth value noise, in integers so every platform
   draws the same chart. */
static int64_t tr_walk(uint32_t key, int64_t tick) {
    int64_t sum = 0;

    for (int k = 0; k < TR_OCTAVES; k++) {
        const int64_t period = (int64_t)TR_TICKS_PER_SEC << k;
        const int64_t i = tick / period;
        const int64_t f = tick % period * 1024 / period;
        const int64_t u = (f * f * (3072 - 2 * f)) >> 20;
        const uint32_t oct = key + (uint32_t)k * 0x01000193u;
        const int64_t a = (int64_t)(tr_mix(oct, (uint32_t)i) % 2001u) - 1000;
        const int64_t b = (int64_t)(tr_mix(oct, (uint32_t)i + 1u) % 2001u) - 1000;

        sum += (a + (b - a) * u / 1024) * TR_AMP[k];
    }
    return sum / 1000;
}

static int64_t tr_model(const TrInstrument *it, int64_t tick) {
    const int64_t sessions = (tick - TR_START_SEC * TR_TICKS_PER_SEC) * it->driftE6
                           / (TR_SESSION_SEC * TR_TICKS_PER_SEC);

    return tr_walk(it->key, tick) * it->volPct / 100 + sessions;
}

/* The price of an instrument at a tick: its walk, scaled to its listed price at the
   start time. */
static int32_t tr_price_at(const TrInstrument *it, int64_t tick) {
    const int64_t p = it->list * (1000000 + tr_model(it, tick)) / (1000000 + it->norm);

    return p < 100 ? 100 : (int32_t)p;
}

/* One bar of the open chart. High and low come from samples inside the bar, stretched a
   little by a hash so that a quiet bar still has a wick; volume follows the bar's range
   and, within a day, the morning and closing rush. A bar still forming ends at now. */
static void tr_build_bar(const TrInstrument *it, int64_t index, int ivSec, int64_t now, TrBar *b) {
    const int64_t t0 = index * ivSec;
    int64_t t1 = t0 + ivSec;
    enum { SAMPLES = 8 };

    if (t1 > now)
        t1 = now;
    b->open  = tr_price_at(it, t0 * TR_TICKS_PER_SEC);
    b->close = tr_price_at(it, t1 * TR_TICKS_PER_SEC);
    b->high  = b->open > b->close ? b->open : b->close;
    b->low   = b->open < b->close ? b->open : b->close;
    for (int j = 1; j < SAMPLES; j++) {
        const int32_t p = tr_price_at(it, (t0 * SAMPLES + (t1 - t0) * j) * TR_TICKS_PER_SEC
                                           / SAMPLES);
        if (p > b->high)
            b->high = p;
        if (p < b->low)
            b->low = p;
    }
    const uint32_t h = tr_mix(it->key ^ 0x51ed27u, (uint32_t)index ^ (uint32_t)ivSec);
    const int32_t range = b->high - b->low;
    b->high += range * (int32_t)(h % 37u) / 100;
    b->low  -= range * (int32_t)(h >> 8 & 31u) / 100;
    /* A wide bar near the floor would stretch its low past zero: no wick goes under the
       price floor tr_price_at quotes at. */
    if (b->low < 100)
        b->low = 100;

    int64_t v = (int64_t)it->baseVol * (t1 - t0) / 60;
    v = v * (40 + (h >> 16) % 120u) / 100;
    v = v * (70 + (int64_t)range * 3000 / b->open) / 100;
    if (ivSec < TR_SESSION_SEC) {
        const int64_t x = t0 % TR_SESSION_SEC * 200 / TR_SESSION_SEC - 100;
        v = v * (60 + x * x / 80) / 100;
    }
    b->volume = v > 2000000000 ? 2000000000 : (int32_t)v;
}

/* -- indicators -------------------------------------------------------------- */

static int64_t tr_isqrt(int64_t v) {
    int64_t x = v, y = (v + 1) / 2;

    while (y < x) {
        x = y;
        y = (x + v / x) / 2;
    }
    return x;
}

/* SMA 20, Bollinger Bands on it, EMA 50 and Wilder's RSI 14, over the whole series. They
   run in integers on one pass; the bands' variance is the only inner loop. */
static void tr_indicators(TrStore *s) {
    const int n = s->barCount;
    int64_t sum = 0, ema = 0, gain = 0, loss = 0;

    for (int i = 0; i < n; i++) {
        const int32_t c = s->bars[i].close;

        sum += c;
        if (i >= 20)
            sum -= s->bars[i - 20].close;
        if (i >= 19) {
            const int64_t mean = sum / 20;
            int64_t var = 0;
            for (int j = i - 19; j <= i; j++) {
                const int64_t d = s->bars[j].close - mean;
                var += d * d;
            }
            const int64_t band = 2 * tr_isqrt(var / 20);
            s->series[TR_SER_SMA20][i] = (int32_t)mean;
            s->series[TR_SER_BB_UP][i] = (int32_t)(mean + band);
            s->series[TR_SER_BB_LO][i] = (int32_t)(mean - band);
        } else {
            s->series[TR_SER_SMA20][i] = TR_NONE;
            s->series[TR_SER_BB_UP][i] = TR_NONE;
            s->series[TR_SER_BB_LO][i] = TR_NONE;
        }

        ema = i ? ema + ((int64_t)c * 256 - ema) * 2 / 51 : (int64_t)c * 256;
        s->series[TR_SER_EMA50][i] = i >= 49 ? (int32_t)(ema / 256) : TR_NONE;

        if (i) {
            const int64_t d = (int64_t)(c - s->bars[i - 1].close) * 256;
            const int64_t up = d > 0 ? d : 0, down = d < 0 ? -d : 0;
            if (i <= 14) {
                gain += up;
                loss += down;
                if (i == 14) {
                    gain /= 14;
                    loss /= 14;
                }
            } else {
                gain = (gain * 13 + up) / 14;
                loss = (loss * 13 + down) / 14;
            }
        }
        s->series[TR_SER_RSI][i] = i < 14 ? TR_NONE
                                 : gain + loss > 0 ? (int32_t)(10000 * gain / (gain + loss))
                                 : 5000;
    }
}

/* -- the order book ---------------------------------------------------------- */

/* A ladder around an instrument's quote: the inside spread and the step grow with the
   price, the sizes grow with depth and shuffle every couple of seconds. */
static void tr_book_for(const TrInstrument *it, int64_t tick, TrLevel *bids, TrLevel *asks) {
    const int32_t px   = it->price;
    const int32_t step = px < 5000 ? 1 : px < 20000 ? 2 : px < 60000 ? 5 : 10;
    const int32_t half = px / 10000 + 1;
    const uint32_t era = (uint32_t)(tick / (2 * TR_TICKS_PER_SEC));

    for (int l = 0; l < TR_BOOK_LEVELS; l++) {
        bids[l].price = px - half - l * step;
        asks[l].price = px + half + l * step;
        if (bids[l].price < 1)
            bids[l].price = 1;
        bids[l].size = (int32_t)(tr_mix(it->key + (uint32_t)l, era) % 40u + 4u) * (l + 2) * 10;
        asks[l].size = (int32_t)(tr_mix(it->key - (uint32_t)l, era) % 40u + 4u) * (l + 2) * 10;
    }
}

/* What a market order pays: it takes each level until it is done, so size costs slippage.
   Past the last level it finishes at that level's price. */
static int64_t tr_walk_book(const TrLevel *side, int qty) {
    int64_t notional = 0;
    int left = qty;

    for (int l = 0; l < TR_BOOK_LEVELS && left > 0; l++) {
        const int take = left < side[l].size ? left : side[l].size;
        notional += (int64_t)take * side[l].price;
        left -= take;
    }
    return notional + (int64_t)left * side[TR_BOOK_LEVELS - 1].price;
}

static void tr_format_book(TrStore *s) {
    for (int l = 0; l < TR_BOOK_LEVELS; l++) {
        tr_fmt_price(s->bids[l].price, s->bids[l].priceStr, (int)sizeof s->bids[l].priceStr);
        tr_fmt_price(s->asks[l].price, s->asks[l].priceStr, (int)sizeof s->asks[l].priceStr);
        tr_fmt_uint((uint64_t)s->bids[l].size, s->bids[l].sizeStr, (int)sizeof s->bids[l].sizeStr);
        tr_fmt_uint((uint64_t)s->asks[l].size, s->asks[l].sizeStr, (int)sizeof s->asks[l].sizeStr);
    }
    s->spread = s->asks[0].price - s->bids[0].price;
    tr_fmt_price(s->spread, s->spreadStr, (int)sizeof s->spreadStr);
}

/* -- the account ------------------------------------------------------------- */

static void tr_format_instrument(TrInstrument *it) {
    const int64_t value = (int64_t)it->shares * it->price;

    it->change    = it->price - it->prevClose;
    it->changeBps = tr_bps(it->change, it->prevClose);
    it->pl        = (int64_t)it->shares * (it->price - it->avgCost);
    it->plBps     = tr_bps(it->price - it->avgCost, it->avgCost);
    tr_fmt_price(it->price, it->priceStr, (int)sizeof it->priceStr);
    tr_fmt_move(it->change, it->changeStr, (int)sizeof it->changeStr);
    tr_fmt_bps(it->changeBps, it->changePctStr, (int)sizeof it->changePctStr);
    if (!it->shares)
        return;
    tr_fmt_uint((uint64_t)it->shares, it->sharesStr, (int)sizeof it->sharesStr);
    tr_fmt_price(it->avgCost, it->avgStr, (int)sizeof it->avgStr);
    tr_fmt_cash(value, it->valueStr, (int)sizeof it->valueStr);
    tr_fmt_pl(it->pl, it->plStr, (int)sizeof it->plStr);
    tr_fmt_bps(it->plBps, it->plPctStr, (int)sizeof it->plPctStr);
}

/* The account line: cash plus every holding at market. */
static void tr_format_totals(TrStore *s) {
    int64_t market = 0, unrealized = 0, day = 0;
    int held = 0;

    for (int i = 0; i < s->count; i++) {
        const TrInstrument *it = &s->inst[i];
        if (!it->shares)
            continue;
        market     += (int64_t)it->shares * it->price;
        unrealized += it->pl;
        day        += (int64_t)it->shares * it->change;
        held++;
    }
    s->holdings   = held;
    s->unrealized = unrealized;
    s->dayPl      = day;
    s->equity     = s->cash + market;
    tr_fmt_cash(s->cash, s->cashStr, (int)sizeof s->cashStr);
    tr_fmt_cash(s->equity, s->equityStr, (int)sizeof s->equityStr);
    tr_fmt_pl(unrealized, s->unrealizedStr, (int)sizeof s->unrealizedStr);
    tr_fmt_pl(s->realized, s->realizedStr, (int)sizeof s->realizedStr);
    tr_fmt_pl(day, s->dayPlStr, (int)sizeof s->dayPlStr);
}

/* A fill moves everything: cash, shares, the average cost, and on a sale realized P&L.
   The newest fills stay; the log drops its oldest when full. */
static void tr_apply_fill(TrStore *s, int inst, TrSide side, TrOrderType type, int qty,
                          int64_t notional, int64_t when) {
    TrInstrument *it = &s->inst[inst];
    int64_t realized = 0;

    if (side == TR_BUY) {
        const int64_t held = it->shares;
        it->avgCost = (int32_t)((held * it->avgCost + notional) / (held + qty));
        it->shares += qty;
        s->cash -= notional;
    } else {
        realized = notional - (int64_t)qty * it->avgCost;
        it->shares -= qty;
        if (!it->shares)
            it->avgCost = 0;
        s->cash += notional;
        s->realized += realized;
    }
    tr_format_instrument(it);

    if (s->fillCount == TR_MAX_FILLS) {
        memmove(&s->fills[0], &s->fills[1], sizeof s->fills[0] * (TR_MAX_FILLS - 1));
        s->fillCount--;
    }
    TrFill *f = &s->fills[s->fillCount++];
    f->inst = inst;
    f->side = side;
    f->type = type;
    f->qty = qty;
    f->price = (int32_t)(notional / qty);
    f->realized = realized;
    f->time = when;
    /* A full log drops its oldest and moves the rest up, so a row cannot be named by its
       index. The serials of the kept fills are consecutive, so no two share a remainder. */
    f->tag = (int32_t)(s->fillSerial++ % TR_MAX_FILLS);
    tr_fmt_uint((uint64_t)qty, f->qtyStr, (int)sizeof f->qtyStr);
    tr_fmt_price(f->price, f->priceStr, (int)sizeof f->priceStr);
    tr_fmt_day(when, f->timeStr, (int)sizeof f->timeStr);
    tr_cat(f->timeStr, (int)sizeof f->timeStr, " ");
    tr_fmt_clock(when, false, f->timeStr + strlen(f->timeStr),
                  (int)sizeof f->timeStr - (int)strlen(f->timeStr));
    tr_fmt_cash(notional, f->valueStr, (int)sizeof f->valueStr);
    if (side == TR_SELL)
        tr_fmt_pl(realized, f->realizedStr, (int)sizeof f->realizedStr);
    else {
        f->realizedStr[0] = '\0';
        tr_cat(f->realizedStr, (int)sizeof f->realizedStr, "-");
    }
}

/* "Bought 10 AAPL at 184.92", or whatever else happened. */
static void tr_announce(TrStore *s, const char *lead, int inst, int qty, int32_t price) {
    char num[16];

    s->event[0] = '\0';
    tr_cat(s->event, (int)sizeof s->event, lead);
    tr_cat(s->event, (int)sizeof s->event, " ");
    tr_fmt_uint((uint64_t)qty, num, (int)sizeof num);
    tr_cat(s->event, (int)sizeof s->event, num);
    tr_cat(s->event, (int)sizeof s->event, " ");
    tr_cat(s->event, (int)sizeof s->event, s->inst[inst].symbol);
    tr_cat(s->event, (int)sizeof s->event, " at ");
    tr_fmt_price(price, num, (int)sizeof num);
    tr_cat(s->event, (int)sizeof s->event, num);
    s->eventSeq++;
}

/* A market order: filled now, against the ladder. */
static void tr_fill_market(TrStore *s, int inst, TrSide side, TrOrderType type, int qty) {
    TrLevel bids[TR_BOOK_LEVELS], asks[TR_BOOK_LEVELS];
    int64_t notional;

    tr_book_for(&s->inst[inst], s->tick, bids, asks);
    notional = tr_walk_book(side == TR_BUY ? asks : bids, qty);
    tr_apply_fill(s, inst, side, type, qty, notional, s->now);
    tr_announce(s, side == TR_BUY ? "Bought" : "Sold", inst, qty, (int32_t)(notional / qty));
}

static void tr_add_working(TrStore *s, int inst, TrSide side, TrOrderType type, int qty,
                           int32_t price) {
    bool taken[TR_MAX_WORKING] = { false };
    int32_t tag = 0;

    if (s->workingCount == TR_MAX_WORKING)
        return;
    /* The smallest number no resting order holds. It stays with this order as the list closes
       up under it, so the rows' ids follow the order: a press on one order's Cancel cannot be
       released onto the order that a fill moves into its row. */
    for (int w = 0; w < s->workingCount; w++)
        taken[s->working[w].tag] = true;
    while (taken[tag])
        tag++;
    TrWorking *o = &s->working[s->workingCount++];
    o->inst = inst;
    o->side = side;
    o->type = type;
    o->qty = qty;
    o->price = price;
    o->tag = tag;
    o->placed = s->now;
    tr_fmt_uint((uint64_t)qty, o->qtyStr, (int)sizeof o->qtyStr);
    tr_fmt_price(price, o->priceStr, (int)sizeof o->priceStr);
    tr_fmt_clock(s->now, false, o->placedStr, (int)sizeof o->placedStr);
    o->label[0] = '\0';
    tr_cat(o->label, (int)sizeof o->label, side == TR_BUY ? "BUY " : "SELL ");
    tr_cat(o->label, (int)sizeof o->label, type == TR_LIMIT ? "LMT " : "STP ");
    tr_cat(o->label, (int)sizeof o->label, o->qtyStr);
}

static void tr_remove_working(TrStore *s, int w) {
    memmove(&s->working[w], &s->working[w + 1],
            sizeof s->working[0] * (size_t)(s->workingCount - w - 1));
    s->workingCount--;
}

/* A resting order fills once the quote reaches it: a limit at its own price, a stop at
   the market once triggered. An order the account cannot honor is rejected. */
static void tr_match_working(TrStore *s) {
    for (int w = s->workingCount - 1; w >= 0; w--) {
        const TrWorking o = s->working[w];
        const TrInstrument *it = &s->inst[o.inst];
        const int32_t half = it->price / 10000 + 1;
        const int32_t bid = it->price - half, ask = it->price + half;
        bool hit;

        if (o.type == TR_LIMIT)
            hit = o.side == TR_BUY ? ask <= o.price : bid >= o.price;
        else
            hit = o.side == TR_BUY ? it->price >= o.price : it->price <= o.price;
        if (!hit)
            continue;
        const int64_t cost = (int64_t)o.qty * o.price;
        tr_remove_working(s, w);
        if (o.side == TR_BUY ? s->cash < cost : it->shares < o.qty) {
            tr_announce(s, "Rejected", o.inst, o.qty, o.price);
        } else if (o.type == TR_LIMIT) {
            tr_apply_fill(s, o.inst, (TrSide)o.side, TR_LIMIT, o.qty, cost, s->now);
            tr_announce(s, o.side == TR_BUY ? "Limit filled: bought" : "Limit filled: sold",
                        o.inst, o.qty, o.price);
        } else {
            tr_fill_market(s, o.inst, (TrSide)o.side, TR_STOP, o.qty);
        }
        tr_format_totals(s);
    }
}

/* -- the open chart ---------------------------------------------------------- */

/* A bar's range always holds its close. */
static void tr_stretch_bar(TrBar *b) {
    if (b->close > b->high)
        b->high = b->close;
    if (b->close < b->low)
        b->low = b->close;
}

static void tr_rebuild_series(TrStore *s) {
    const TrInstrument *it = tr_selected(s);
    const int ivSec = TR_IV_SEC[s->interval];
    const int64_t last = s->now / ivSec;

    s->barCount = last + 1 < TR_MAX_BARS ? (int32_t)(last + 1) : TR_MAX_BARS;
    s->firstBar = last + 1 - s->barCount;
    for (int i = 0; i < s->barCount; i++)
        tr_build_bar(it, s->firstBar + i, ivSec, s->now, &s->bars[i]);
    s->bars[s->barCount - 1].close = it->price;
    tr_stretch_bar(&s->bars[s->barCount - 1]);
    tr_indicators(s);
}

/* The newest bar follows the live quote: a new bar rebuilds the series, and otherwise only
   its close and its range move. */
static void tr_update_series(TrStore *s) {
    const TrInstrument *it = tr_selected(s);
    const int ivSec = TR_IV_SEC[s->interval];

    if (s->now / ivSec != s->firstBar + s->barCount - 1) {
        tr_rebuild_series(s);
        return;
    }
    TrBar *b = &s->bars[s->barCount - 1];
    b->close = it->price;
    tr_stretch_bar(b);
    tr_indicators(s);
}

/* -- the store --------------------------------------------------------------- */

/* Two symbols in alphabetical order, for a sorted watchlist. */
TRDEF int tr_symbol_order(const TrInstrument *a, const TrInstrument *b) {
    return strcmp(a->symbol, b->symbol);
}

/* The app's way to zero its state: it names no libc call, and this file is where libc is. */
TRDEF void tr_zero(void *p, size_t n) {
    memset(p, 0, n);
}

static void tr_build_spark(TrInstrument *it, int64_t now) {
    for (int k = 0; k < TR_SPARK_N; k++) {
        const int64_t back = (int64_t)(TR_SPARK_N - 1 - k) * TR_SESSION_SEC / (TR_SPARK_N - 1);
        const int64_t t = now > back ? now - back : 0;
        it->spark[k] = (float)tr_price_at(it, t * TR_TICKS_PER_SEC);
    }
}

/* The account in cents at a moment, holding what it holds now. */
static int64_t tr_equity_at(const TrStore *s, int64_t t) {
    int64_t v = s->cash;

    for (int i = 0; i < s->count; i++) {
        const TrInstrument *it = &s->inst[i];
        if (it->shares)
            v += (int64_t)it->shares * tr_price_at(it, t * TR_TICKS_PER_SEC);
    }
    return v;
}

/* The equity curve runs the holdings backward over its span. A curve that replayed
   every old trade would differ only by what the trades themselves earned. */
static void tr_build_equity(TrStore *s) {
    const int64_t grid = s->now / s->equityStep;

    s->lastSample  = grid;
    s->equityStart = (grid - (TR_EQUITY_N - 1)) * s->equityStep;
    s->equityOpen  = tr_equity_at(s, s->equityStart);
    for (int k = 0; k < TR_EQUITY_N; k++) {
        const int64_t t = s->equityStart + (int64_t)k * s->equityStep;
        s->equityCurve[k] = (float)tr_equity_at(s, t) / 100.0f;
    }
    s->equityCurve[TR_EQUITY_N - 1] = (float)s->equity / 100.0f;
}

/* The session's calendar line and clock. */
static void tr_format_clock(TrStore *s) {
    const int64_t day = s->now / TR_SESSION_SEC;

    tr_fmt_clock(s->now, true, s->clockStr, (int)sizeof s->clockStr);
    s->dateStr[0] = '\0';
    tr_cat(s->dateStr, (int)sizeof s->dateStr, TR_DAYS[day % 5]);
    tr_cat(s->dateStr, (int)sizeof s->dateStr, " ");
    tr_fmt_day(s->now, s->dateStr + 4, (int)sizeof s->dateStr - 4);
}

/* The listed universe: symbol, name, exchange, listed price, volatility, trend. */
static const struct {
    const char *symbol, *name, *exchange;
    int32_t price, vol, drift;
} TR_LIST[TR_MAX_INSTRUMENTS] = {
    { "AAPL", "Apple Inc.",               "NASDAQ", 18732,  60, 520 },
    { "MSFT", "Microsoft Corp.",          "NASDAQ", 42150,  54, 640 },
    { "NVDA", "NVIDIA Corp.",             "NASDAQ", 89640, 114, 1650 },
    { "AMZN", "Amazon.com Inc.",          "NASDAQ", 17820,  72, 580 },
    { "GOOG", "Alphabet Inc.",            "NASDAQ", 15230,  66, 430 },
    { "META", "Meta Platforms Inc.",      "NASDAQ", 49810,  84, 940 },
    { "TSLA", "Tesla Inc.",               "NASDAQ", 24560, 138, 310 },
    { "AMD",  "Advanced Micro Devices",   "NASDAQ", 16340, 114, 520 },
    { "NFLX", "Netflix Inc.",             "NASDAQ", 58720,  90, 860 },
    { "INTC", "Intel Corp.",              "NASDAQ",  4310,  84, -220 },
    { "ORCL", "Oracle Corp.",             "NYSE",   12690,  54, 370 },
    { "CRM",  "Salesforce Inc.",          "NYSE",   27840,  72, 260 },
    { "ADBE", "Adobe Inc.",               "NASDAQ", 55120,  72, 180 },
    { "PYPL", "PayPal Holdings Inc.",     "NASDAQ",  6580,  90, -160 },
    { "UBER", "Uber Technologies Inc.",   "NYSE",    7240,  90, 610 },
    { "SHOP", "Shopify Inc.",             "NYSE",    8130, 120, 480 },
    { "SQ",   "Block Inc.",               "NYSE",    7690, 114, 90 },
    { "ARM",  "Arm Holdings plc",         "NASDAQ", 13520, 126, 740 },
    { "AVGO", "Broadcom Inc.",            "NASDAQ", 132400,  78, 900 },
    { "QCOM", "Qualcomm Inc.",            "NASDAQ", 17250,  66, 210 },
    { "MU",   "Micron Technology Inc.",   "NASDAQ", 10480, 102, 660 },
    { "TXN",  "Texas Instruments Inc.",   "NASDAQ", 19870,  48, 240 },
    { "NOW",  "ServiceNow Inc.",          "NYSE",   78610,  72, 720 },
    { "PANW", "Palo Alto Networks Inc.",  "NASDAQ", 31240,  78, 580 },
};

/* The demo account's history, replayed through the fill path so its cash, average costs
   and realized P&L are the sum of real trades. `when` is seconds before the start time;
   the two AAPL trades closest to it land on the first chart a viewer sees. */
static void tr_seed_account(TrStore *s) {
    static const struct {
        int inst;
        TrSide side;
        int qty;
        int32_t when;
    } TRADES[] = {
        { 1, TR_BUY,  35, 9 * TR_SESSION_SEC + 4260 },
        { 2, TR_BUY,  60, 8 * TR_SESSION_SEC - 3780 },
        { 5, TR_BUY,  18, 6 * TR_SESSION_SEC + 7140 },
        { 7, TR_BUY, 150, 4 * TR_SESSION_SEC - 8520 },
        { 6, TR_BUY,  25, 3 * TR_SESSION_SEC + 1620 },
        { 2, TR_SELL, 20, 2 * TR_SESSION_SEC - 5460 },
        { 0, TR_BUY, 150, 20000 },
        { 0, TR_SELL, 30, 9000 },
    };

    s->deposit = 25000000;
    s->cash = s->deposit;
    for (size_t k = 0; k < sizeof TRADES / sizeof TRADES[0]; k++) {
        const int64_t when = s->now - TRADES[k].when;
        const TrInstrument *it = &s->inst[TRADES[k].inst];
        const int64_t notional = (int64_t)TRADES[k].qty * tr_price_at(it, when * TR_TICKS_PER_SEC);

        tr_apply_fill(s, TRADES[k].inst, TRADES[k].side, TR_MARKET, TRADES[k].qty, notional, when);
    }
    tr_add_working(s, 0, TR_BUY, TR_LIMIT, 50, s->inst[0].price / 50 * 50 - 50);
    tr_add_working(s, 2, TR_SELL, TR_LIMIT, 10, s->inst[2].price / 100 * 102);
    tr_add_working(s, 6, TR_SELL, TR_STOP, 25, s->inst[6].price / 100 * 97);
}

/* The book and the series for whatever is open. */
static void tr_refresh_open(TrStore *s) {
    tr_book_for(tr_selected(s), s->tick, s->bids, s->asks);
    tr_format_book(s);
    tr_rebuild_series(s);
}

TRDEF void tr_store_seed(TrStore *s, unsigned seed) {
    memset(s, 0, sizeof *s);
    s->speed = 1;
    s->tick = TR_START_SEC * TR_TICKS_PER_SEC;
    s->now = TR_START_SEC;
    s->day = s->now / TR_SESSION_SEC;
    s->interval = TR_IV_5M;
    s->count = TR_MAX_INSTRUMENTS;
    s->equityStep = TR_SESSION_SEC / (TR_EQUITY_N - 1);    /* the curve spans a session */

    for (int i = 0; i < s->count; i++) {
        TrInstrument *it = &s->inst[i];

        /* The store was just zeroed, so each of these is empty and tr_cat fills it. */
        tr_cat(it->symbol, (int)sizeof it->symbol, TR_LIST[i].symbol);
        tr_cat(it->name, (int)sizeof it->name, TR_LIST[i].name);
        tr_cat(it->exchange, (int)sizeof it->exchange, TR_LIST[i].exchange);
        it->list = TR_LIST[i].price;
        it->volPct = TR_LIST[i].vol;
        it->driftE6 = TR_LIST[i].drift;
        it->key = tr_mix(seed, (uint32_t)i + 1u);
        it->baseVol = 12000 + (int32_t)(tr_mix(it->key, 7u) % 90000u);
        it->norm = tr_model(it, s->tick);
        it->price = tr_price_at(it, s->tick);
        it->prevClose = tr_price_at(it, (s->now / TR_SESSION_SEC) * TR_SESSION_SEC
                                         * TR_TICKS_PER_SEC);
        tr_build_spark(it, s->now);
        tr_format_instrument(it);
    }
    tr_seed_account(s);
    tr_format_totals(s);
    tr_build_equity(s);
    tr_format_clock(s);
    tr_refresh_open(s);
}

/* Say something that is no trade: a refused order, a reset. */
TRDEF void tr_notice(TrStore *s, const char *text) {
    s->event[0] = '\0';
    tr_cat(s->event, (int)sizeof s->event, text);
    s->eventSeq++;
}

/* Empty the account and open it again: the positions go, the market stays. */
TRDEF void tr_reset_account(TrStore *s) {
    for (int i = 0; i < s->count; i++) {
        s->inst[i].shares = 0;
        s->inst[i].avgCost = 0;
        tr_format_instrument(&s->inst[i]);
    }
    s->workingCount = 0;
    s->fillCount = 0;
    s->realized = 0;
    s->cash = s->deposit;
    tr_format_totals(s);
    tr_build_equity(s);
    tr_notice(s, "Account reset to the opening deposit");
}

/* Span the equity curve over that many sessions. A price is a function of time, so a new span
   is sampled at once, and asking for the span in force changes nothing. */
TRDEF void tr_store_equity_span(TrStore *s, int sessions) {
    const int32_t step = sessions * TR_SESSION_SEC / (TR_EQUITY_N - 1);

    if (step != s->equityStep) {
        s->equityStep = step;
        tr_build_equity(s);
    }
}

/* One feed tick: every quote moves, the resting orders meet them, the open chart and the
   account follow. */
static void tr_tick(TrStore *s) {
    const int64_t day = s->now / TR_SESSION_SEC;
    const bool newDay = day != s->day;

    s->day = day;
    for (int i = 0; i < s->count; i++) {
        TrInstrument *it = &s->inst[i];
        const int32_t was = it->price;

        if (newDay)
            it->prevClose = was;
        /* The open symbol quotes on every tick. The others trade unevenly, about once in
           six, so a board of them shimmers rather than strobes. */
        if (i != s->selected && !newDay && tr_mix(it->key, (uint32_t)s->tick) % 6u)
            continue;
        it->price = tr_price_at(it, s->tick);
        if (it->price != was)
            it->flash = it->price > was ? 1.0f : -1.0f;
        it->spark[TR_SPARK_N - 1] = (float)it->price;
        tr_format_instrument(it);
    }
    /* One trend per tick is resampled, so the whole list stays fresh for no real cost. */
    tr_build_spark(&s->inst[s->tick % s->count], s->now);

    tr_match_working(s);
    tr_format_totals(s);
    tr_book_for(tr_selected(s), s->tick, s->bids, s->asks);
    tr_format_book(s);
    tr_update_series(s);
    tr_format_clock(s);

    const int64_t grid = s->now / s->equityStep;
    if (grid != s->lastSample) {
        memmove(&s->equityCurve[0], &s->equityCurve[1],
                sizeof s->equityCurve[0] * (TR_EQUITY_N - 1));
        s->lastSample = grid;
        s->equityStart += s->equityStep;
        s->equityOpen = tr_equity_at(s, s->equityStart);
    }
    s->equityCurve[TR_EQUITY_N - 1] = (float)s->equity / 100.0f;
}

/* Advance by dt seconds of real time; dt <= 0 is the freeze, a strict no-op. */
TRDEF void tr_store_step(TrStore *s, float dt) {
    if (dt <= 0.0f)
        return;
    for (int i = 0; i < s->count; i++) {
        TrInstrument *it = &s->inst[i];
        const float fade = dt * 2.5f;

        if (it->flash > 0.0f)
            it->flash = it->flash > fade ? it->flash - fade : 0.0f;
        else if (it->flash < 0.0f)
            it->flash = -it->flash > fade ? it->flash + fade : 0.0f;
    }
    s->clock += (double)dt * s->speed;
    const int64_t tick = TR_START_SEC * TR_TICKS_PER_SEC + (int64_t)(s->clock * TR_TICKS_PER_SEC);
    if (tick == s->tick)
        return;
    s->tick = tick;
    s->now = tick / TR_TICKS_PER_SEC;
    tr_tick(s);
}

TRDEF void tr_select(TrStore *s, int i) {
    if (i < 0 || i >= s->count || i == s->selected)
        return;
    s->selected = i;
    tr_refresh_open(s);
}

TRDEF void tr_set_interval(TrStore *s, int interval) {
    if (interval < 0 || interval >= TR_IV_COUNT || interval == s->interval)
        return;
    s->interval = interval;
    tr_rebuild_series(s);
}

TRDEF int tr_interval_seconds(int interval) {
    return TR_IV_SEC[interval];
}

/* A bar's start in seconds, for any bar index on the open chart. */
TRDEF int64_t tr_bar_time(const TrStore *s, int bar) {
    return (s->firstBar + bar) * TR_IV_SEC[s->interval];
}

/* What an order would cost or bring in right now. Market walks the book, a limit is its
   own price, a stop is the book once triggered. */
TRDEF int64_t tr_estimate(const TrStore *s, TrSide side, TrOrderType type, int qty, int32_t price) {
    if (qty <= 0)
        return 0;
    if (type == TR_LIMIT && price > 0)
        return (int64_t)qty * price;
    return tr_walk_book(side == TR_BUY ? s->asks : s->bids, qty);
}

/* What selling a whole position brings in right now, against that symbol's own book. */
TRDEF int64_t tr_close_value(const TrStore *s, int inst) {
    TrLevel bids[TR_BOOK_LEVELS], asks[TR_BOOK_LEVELS];

    tr_book_for(&s->inst[inst], s->tick, bids, asks);
    return tr_walk_book(bids, s->inst[inst].shares);
}

/* Why this order cannot go, or NULL when it can. Plain words, for the ticket. */
TRDEF const char *tr_order_problem(const TrStore *s, TrSide side, TrOrderType type, int qty,
                                   int32_t price) {
    const TrInstrument *it = tr_selected(s);

    if (qty <= 0)
        return "Enter a number of shares";
    if (type != TR_MARKET && price <= 0)
        return type == TR_LIMIT ? "Enter a limit price" : "Enter a stop price";
    if (type != TR_MARKET && s->workingCount == TR_MAX_WORKING)
        return "Too many open orders";
    if (side == TR_BUY && tr_estimate(s, side, type, qty, price) > s->cash)
        return "Not enough buying power";
    if (side == TR_SELL && it->shares < qty)
        return it->shares ? "You own fewer shares than that" : "You own no shares of this stock";
    return NULL;
}

/* Place an order on the open instrument: a market order walks the book, a limit that
   crosses the spread fills at the touch, anything else rests until the quote reaches it. */
TRDEF void tr_place_order(TrStore *s, TrSide side, TrOrderType type, int qty, int32_t price) {
    const int inst = s->selected;

    if (tr_order_problem(s, side, type, qty, price))
        return;
    const int32_t touch = side == TR_BUY ? s->asks[0].price : s->bids[0].price;

    if (type == TR_MARKET) {
        tr_fill_market(s, inst, side, type, qty);
    } else if (type == TR_LIMIT && (side == TR_BUY ? touch <= price : touch >= price)) {
        tr_apply_fill(s, inst, side, type, qty, (int64_t)qty * touch, s->now);
        tr_announce(s, side == TR_BUY ? "Bought" : "Sold", inst, qty, touch);
    } else {
        tr_add_working(s, inst, side, type, qty, price);
        tr_announce(s, side == TR_BUY ? "Working: buy" : "Working: sell", inst, qty, price);
    }
    tr_format_totals(s);
}

/* Sell a whole position at the market. */
TRDEF void tr_close_position(TrStore *s, int inst) {
    if (inst < 0 || inst >= s->count || !s->inst[inst].shares)
        return;
    tr_fill_market(s, inst, TR_SELL, TR_MARKET, s->inst[inst].shares);
    tr_format_totals(s);
}

TRDEF void tr_cancel_order(TrStore *s, int w) {
    if (w < 0 || w >= s->workingCount)
        return;
    tr_announce(s, "Canceled", s->working[w].inst, s->working[w].qty, s->working[w].price);
    tr_remove_working(s, w);
}

#endif /* TRADER_BACKEND_IMPLEMENTATION */
#endif /* TRADER_BACKEND_H */
