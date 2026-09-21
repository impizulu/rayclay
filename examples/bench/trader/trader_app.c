/*
    trader_app.c - the terminal's screen, drawn with nothing but the public API.

    Shows: a three-pane desktop layout that folds to one pane on a narrow window,
    a candlestick chart built from plain rcBox rects, a depth ladder, a gradient
    and shadow on a card, a modal, a combo, text inputs and a live theme switch.

    EVERY NUMBER ON SCREEN IS A PRECOMPUTED BACKEND BUFFER. The UI never formats
    one, which is what lets this same code draw with no scratch arena at all; the
    few strings it does assemble go into function-static buffers by hand.
*/
#define TRADER_BACKEND_IMPLEMENTATION
#include "trader_app.h"

/* Nav-rail glyphs from the shared example icon set (as the messenger/notes do). */
#include "icons/rc_icons_panel_left.h"
#include "icons/rc_icons_expand.h"
#include "icons/rc_icons_settings.h"
#include "icons/rc_icons_chart_column.h"   /* the compact tab bar's Markets glyph */

/* Warmup frames before the scripted scenario holds still. */
#define TRADER_BENCH_WARMUP 64

/* THE ONE BREAKPOINT, DERIVED FROM THIS APP'S OWN PANES rather than from a table
   of device widths: the rail, the watchlist, the order panel and the width the
   detail pane needs for a legible 48-candle year. Below it the watchlist and the
   detail TAKE TURNS, and the rail's controls become a bottom tab bar. Compared
   against the ZOOM-CORRECTED viewport, so the condition is the SPACE and never
   the platform - which is what makes this arm testable anywhere. */
#define TR_ONE_PANE_W 1026.0f

/* A one-pane window this wide can carry the watchlist as a TABLE - symbol, trend,
   last, change and change percent - instead of a symbol at one edge and a price
   at the other with a void between them. Below it the row folds back to a list. */
#define TR_LIST_TABLE_W 660.0f

/* The compact tab bar's height, and the band the compact body leaves above it for
   the demo readout so the chip never floats over a row. */
#define TR_TABBAR_H 56

#define TR_HUD_BAND 44

/* TWO CYANS, AND THEY ARE NOT INTERCHANGEABLE. `.primary` is a FILL under white
   text, so it has to be dark enough to carry it. TR_CYAN is a LINE and a LABEL on
   a dark panel, which the fill shade is too dark to be - and on the light theme's
   white it fails contrast, so there both roles collapse onto the fill shade.

   A colour token is a #define or a brace-initialised RC_Color, never a file-scope
   `static const RC_Color`: rcRgb/rcHex expand to compound literals, which are not
   constant expressions in C99. */
#define TR_CYAN 0x38bdf8         /* accent line/label, dark theme only */

/* Direction colours come off the STYLE (.success / .danger), so Light mode gets
   shades that survive a white background instead of the dark theme's bright pair. */
static RC_Color tr_up_color(void)   { return rcGetStyle().success; }
static RC_Color tr_down_color(void) { return rcGetStyle().danger; }

/* Stable ids: every interactive element needs one that does not move frame to
   frame, and an id table is the cheapest way to give a list of rows one each. */
static const char *const WATCH_IDS[TR_MAX_INSTRUMENTS] = {
    "wr00", "wr01", "wr02", "wr03", "wr04", "wr05", "wr06", "wr07",
    "wr08", "wr09", "wr10", "wr11", "wr12", "wr13", "wr14", "wr15",
    "wr16", "wr17", "wr18", "wr19", "wr20", "wr21", "wr22", "wr23",
};

/* The two holdings lists get DIFFERENT ids for the same instrument: they are
   never on screen together, and one id meaning two elements would make hover and
   click follow whichever of them drew last. */
static const char *const POS_IDS[TR_MAX_INSTRUMENTS] = {
    "po00", "po01", "po02", "po03", "po04", "po05", "po06", "po07",
    "po08", "po09", "po10", "po11", "po12", "po13", "po14", "po15",
    "po16", "po17", "po18", "po19", "po20", "po21", "po22", "po23",
};
static const char *const PORT_IDS[TR_MAX_INSTRUMENTS] = {
    "pf00", "pf01", "pf02", "pf03", "pf04", "pf05", "pf06", "pf07",
    "pf08", "pf09", "pf10", "pf11", "pf12", "pf13", "pf14", "pf15",
    "pf16", "pf17", "pf18", "pf19", "pf20", "pf21", "pf22", "pf23",
};

/* One id per candle, so the chart can tell which one the pointer is over. */
static const char *const CANDLE_IDS[TR_MAX_CANDLES] = {
    "cd00", "cd01", "cd02", "cd03", "cd04", "cd05", "cd06", "cd07",
    "cd08", "cd09", "cd10", "cd11", "cd12", "cd13", "cd14", "cd15",
    "cd16", "cd17", "cd18", "cd19", "cd20", "cd21", "cd22", "cd23",
    "cd24", "cd25", "cd26", "cd27", "cd28", "cd29", "cd30", "cd31",
    "cd32", "cd33", "cd34", "cd35", "cd36", "cd37", "cd38", "cd39",
    "cd40", "cd41", "cd42", "cd43", "cd44", "cd45", "cd46", "cd47",
};

static const char *const ORDER_TYPES[] = { "Market", "Limit" };
static const char *const WATCH_VIEWS[] = { "All", "Gainers", "Losers" };

static RC_Color tr_updown(int64_t v) { return v >= 0 ? tr_up_color() : tr_down_color(); }

/* Text ON a filled accent. The dark theme's accents are bright enough that
   near-black carries them and white does not; the light theme's are the reverse. */
static RC_Color tr_on_accent(bool dark) { return dark ? rcHex(0x0b0f14) : RC_WHITE; }

/* The whole palette, both themes, in one place: everything drawn reads it back
   through rcGetStyle(), so the terminal restyles from here alone. Not static,
   because the demo runner needs the same ground for the window's clear colour. */
RC_Style trader_style(bool dark) {
    if (!dark) {
        RC_Style s = rcStyleLight();
        s.background   = rcHex(0xf4f7fa);
        s.surface      = rcHex(0xffffff);
        s.surfaceAlt   = rcHex(0xe8eef5);
        s.chrome       = rcHex(0xffffff);
        s.text         = rcHex(0x0b0f14);
        s.textMuted    = rcHex(0x5a6a7a);
        s.primary      = rcHex(0x0284c7);
        s.primaryHover = rcHex(0x0369a1);
        s.success      = rcHex(0x15803d);   /* 4.8:1 on white; the bright green is 1.9 */
        s.successHover = rcHex(0x166534);
        s.danger       = rcHex(0xb91c1c);
        s.dangerHover  = rcHex(0x991b1b);
        s.border       = rcHex(0xd6dee8);
        return s;
    }
    RC_Style s = rcStyleDark();
    s.background   = rcHex(0x0b0f14);       /* the terminal ground  */
    s.surface      = rcHex(0x141a22);       /* watchlist, order panel, tab bar */
    s.surfaceAlt   = rcHex(0x1c2531);       /* rows, chips, inset tracks       */
    s.chrome       = rcHex(0x0e141b);
    s.text         = rcHex(0xe6edf3);
    s.textMuted    = rcHex(0x8b9cb0);       /* 6.3:1 on the panel   */
    s.primary      = rcHex(0x0284c7);
    s.primaryHover = rcHex(0x0369a1);
    s.success      = rcHex(0x22c55e);
    s.successHover = rcHex(0x16a34a);
    s.danger       = rcHex(0xef4444);
    s.dangerHover  = rcHex(0xdc2626);
    s.border       = rcHex(0x1f2a36);
    return s;
}

/* Parse the order-form qty buffer to an int (pure C; no libc atoi in the .c). */
static int qty_to_int(const char *s) {
    int v = 0;
    for (int i = 0; s[i] && i < 9; i++) {
        if (s[i] < '0' || s[i] > '9')
            break;
        v = v * 10 + (s[i] - '0');
    }
    return v;
}

/* Parse a "142.50"-style price buffer to integer cents (pure C; the backend keeps
   cents). Up to two fractional digits are read; anything non-numeric yields 0, which
   the backend reads as "use the market price". */
static int px_to_cents(const char *s) {
    int64_t whole = 0;
    int i = 0;
    for (; s[i] && s[i] != '.' && i < 9; i++) {
        if (s[i] < '0' || s[i] > '9')
            return 0;
        whole = whole * 10 + (s[i] - '0');
    }
    int frac = 0;
    if (s[i] == '.') {
        i++;
        if (s[i] >= '0' && s[i] <= '9') frac += (s[i++] - '0') * 10;
        if (s[i] >= '0' && s[i] <= '9') frac +=  s[i]   - '0';
    }
    int64_t cents = whole * 100 + frac;
    if (cents > 2000000000LL) cents = 2000000000LL;   /* clamp like the backend buffers */
    return (int)cents;
}

/* Lowercase one ASCII byte (the instrument corpus is ASCII/Latin-1). */
static char tr_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

/* Case-insensitive substring test: does `hay` contain `needle`? An empty needle
   matches everything (an empty search box filters nothing). Pure C, no libc. */
static bool tr_ci_contains(const char *hay, const char *needle) {
    if (!needle[0])
        return true;
    for (int i = 0; hay[i]; i++) {
        int j = 0;
        while (needle[j] && tr_lower(hay[i + j]) == tr_lower(needle[j]))
            j++;
        if (!needle[j])
            return true;
    }
    return false;
}

/* A nav-rail icon button. Polling rcClicked is what gives a hand-rolled box the
   pointer cursor; `tip` names the glyph on dwell, because a rail has no labels. */
static bool nav_button(const char *id, RC_IconCallback icon, bool active, const char *tip) {
    RC_Style s = rcGetStyle();
    rcBox(.id = id,
          .bg = active ? s.surfaceAlt : (rcIsHovered(id) ? s.surface : RC_TRANSPARENT),
          .align = "cc", .borderRadius = "all-lg", .w = "44px", .h = "44px",
          .tooltip = tip) {
        icon(20.0f, active ? s.primary : s.textMuted);
    }
    return rcClicked(id);
}

/* A Buy/Sell segmented toggle. THE INACTIVE HALF IS A FILLED TRACK ONE STEP OFF
   THE PANEL, never the panel's own surface: painted that colour it has no edge at
   all and the pair reads as one button beside a stray word. Hovering washes the
   segment in its OWN side colour, saying what the click would select. */
static bool seg_button(const char *id, const char *label, bool active, RC_Color tint,
                       bool dark) {
    RC_Style s = rcGetStyle();
    rcBox(.id = id,
          .bg = active ? tint : (rcIsHovered(id) ? rcAlpha(tint, 60) : s.surfaceAlt),
          .align = "cc", .borderRadius = "all-md", .w = "grow", .h = "34px") {
        rcTextC(label, .font = F_BODY, .color = active ? tr_on_accent(dark) : s.text);
    }
    return rcClicked(id);
}

/* A direction arrow built from four stacked bars: the bundled face is Latin-1 and
   holds no arrow glyph. Every signed number carries one, so direction never rests
   on red against green alone. */
static void dir_arrow(bool up, RC_Color col) {
    rcColumn(.gap = 0, .align = "cc", .w = "8px") {
        rcBox(.bg = col, .w = up ? "2px" : "8px", .h = "2px") {}
        rcBox(.bg = col, .w = up ? "4px" : "6px", .h = "2px") {}
        rcBox(.bg = col, .w = up ? "6px" : "4px", .h = "2px") {}
        rcBox(.bg = col, .w = up ? "8px" : "2px", .h = "2px") {}
    }
}

/* A chart-timeframe segment (1D/1W/1M/1Y), which reslices the candle window. The
   timeframe is the chart's primary control, so it is a filled segmented control
   rather than a row of small captions under a hairline. */
static bool tf_seg(const char *id, const char *label, bool active) {
    RC_Style s = rcGetStyle();
    rcBox(.id = id,
          .bg = active ? s.primary : (rcIsHovered(id) ? s.surface : RC_TRANSPARENT),
          .px = 12, .align = "cc", .borderRadius = "all-md", .w = "46px", .h = "28px") {
        rcTextC(label, .font = F_BODY, .color = active ? RC_WHITE : s.textMuted);
    }
    return rcClicked(id);
}

/* A watchlist filter pill. A finger gets a 40 px tall target, a mouse 23: the
   POINTER CLASS sizes a hit target, never the width, so a touch laptop at 1280
   gets the big one and a narrow mouse window does not. */
static bool watch_pill(const char *id, const char *label, bool active, bool finger) {
    RC_Style s = rcGetStyle();
    rcBox(.id = id,
          .bg = active ? s.primary : (rcIsHovered(id) ? s.surfaceAlt : RC_TRANSPARENT),
          .px = 10, .py = 5, .align = "cc", .borderRadius = "all-full",
          .hMin = finger ? 40.0f : 0.0f) {
        rcTextC(label, .font = F_SMALL, .color = active ? RC_WHITE : s.textMuted);
    }
    return rcClicked(id);
}

/* A row-sized spark of the instrument's OWN trailing closes, so a row and the
   chart can never disagree. Hand-rolled bars rather than rcSparkline because every
   height here is an integer off the backend's cents; `grow` widens them to fill a
   table row instead of holding a fixed strip. */
#define TR_SPARK_BARS 10      /* the list row's trailing window   */
#define TR_SPARK_WIDE 36      /* the table row has room for more  */
#define TR_SPARK_H    22

static void spark(const TrInstrument *it, RC_Color col, int bars, bool grow) {
    const int first = TR_MAX_CANDLES - bars;
    int32_t lo = it->candles[first].close, hi = lo;
    for (int c = first + 1; c < TR_MAX_CANDLES; c++) {
        if (it->candles[c].close < lo) lo = it->candles[c].close;
        if (it->candles[c].close > hi) hi = it->candles[c].close;
    }
    int32_t range = hi - lo;
    if (range < 1) range = 1;                         /* flat-series div-by-zero guard */
    rcRow(.gap = (uint16_t)(grow ? 1 : 2), .align = "bl", .w = grow ? "grow" : NULL,
          .hType = RC_PX(TR_SPARK_H)) {
        for (int c = first; c < TR_MAX_CANDLES; c++) {
            /* a 3 px floor keeps the lowest bar a mark rather than nothing */
            int h = (int)((int64_t)(it->candles[c].close - lo) * (TR_SPARK_H - 3) / range) + 3;
            /* .borderRadius and .align are fixed char arrays, not pointers, so a
               token is chosen by branching rather than by a ternary */
            if (grow)
                rcBox(.bg = rcAlpha(col, 150), .borderRadius = "all-2", .w = "grow",
                      .hType = RC_PX(h)) {}
            else
                rcBox(.bg = rcAlpha(col, 150), .borderRadius = "all-sm", .w = "3px",
                      .hType = RC_PX(h)) {}
        }
    }
}

/* The watchlist's table columns, shared by the header and the rows so the two
   cannot drift apart. */
#define TR_WL_SYMBOL_W "196px"
#define TR_WL_LAST_W    "92px"
#define TR_WL_CHG_W     "88px"
#define TR_WL_PCT_W     "84px"

/* The watchlist's column headings, emitted once above the rows in the table arm.
   Its padding mirrors a row's plus the scroller's gutter, so the headings sit over
   the columns they name. */
static void watch_head(void) {
    RC_Style s = rcGetStyle();
    rcRow(.gap = 8, .pl = 7, .pr = 22, .align = "cl", .w = "grow") {
        rcBox(.w = "3px") {}
        rcBox(.w = TR_WL_SYMBOL_W) { rcTextL("Symbol", .font = F_SMALL, .color = s.textMuted); }
        rcBox(.w = "grow")         { rcTextL("Trend",  .font = F_SMALL, .color = s.textMuted); }
        rcBox(.align = "cr", .w = TR_WL_LAST_W) { rcTextL("Last",  .font = F_SMALL, .color = s.textMuted); }
        rcBox(.align = "cr", .w = TR_WL_CHG_W)  { rcTextL("Chg",   .font = F_SMALL, .color = s.textMuted); }
        rcBox(.align = "cr", .w = TR_WL_PCT_W)  { rcTextL("Chg %", .font = F_SMALL, .color = s.textMuted); }
    }
}

/* A watchlist row. The whole row is a click target, and polling rcClicked is what
   turns a styled row into a button, pointer cursor included.

   TWO SHAPES FOR ONE ROW. Narrow, it is a list. `table` is the wide one-pane arm,
   where leaving the name column to take all the slack would put a symbol at one
   edge and a price at the other with a void between - so there the symbol column
   is fixed, the SPARK takes the slack, and the extra width buys the change in
   money as a column of its own. THE SELECTED ROW CARRIES A FILLED BAR: one step
   of surface brightness cannot say which of twenty rows the ticket is showing. */
static bool watch_row(const char *id, const TrInstrument *it, bool active, bool table) {
    RC_Style s = rcGetStyle();
    RC_Color dir = tr_updown(it->changeBps);
    rcRow(.id = id,
          .bg = active ? s.surfaceAlt : (rcIsHovered(id) ? s.surface : RC_TRANSPARENT),
          .gap = 8, .pl = 7, .pr = 10, .align = "cl", .borderRadius = "all-md",
          .w = "grow", .h = "52px") {
        rcBox(.bg = active ? s.primary : RC_TRANSPARENT, .borderRadius = "all-full",
              .w = "3px", .h = "34px") {}
        rcColumn(.gap = 2, .w = table ? TR_WL_SYMBOL_W : "grow") {
            rcTextC(it->symbol, .font = F_BODY, .color = s.text);
            rcBox(.overflow = "hidden", .w = "grow") {
                rcTextC(it->name, .font = F_SMALL, .color = s.textMuted, .wrap = "n");
            }
        }
        spark(it, dir, table ? TR_SPARK_WIDE : TR_SPARK_BARS, table);
        if (table) {
            rcBox(.align = "cr", .w = TR_WL_LAST_W) {
                rcTextC(it->priceStr, .font = F_BODY, .color = s.text);
            }
            rcBox(.align = "cr", .w = TR_WL_CHG_W) {
                rcTextC(it->changeAbsStr, .font = F_SMALL, .color = dir);
            }
            rcBox(.align = "cr", .w = TR_WL_PCT_W) {
                rcRow(.gap = 4, .align = "cr") {
                    dir_arrow(it->changeBps >= 0, dir);
                    rcTextC(it->changeStr, .font = F_SMALL, .color = dir);
                }
            }
        } else {
            rcColumn(.gap = 2, .align = "tr") {
                rcTextC(it->priceStr, .font = F_BODY, .color = s.text);
                rcRow(.gap = 4, .align = "cr") {
                    dir_arrow(it->changeBps >= 0, dir);
                    rcTextC(it->changeStr, .font = F_SMALL, .color = dir);
                }
            }
        }
    }
    return rcClicked(id);
}

/* One order-book level. THE TWO SIDES MIRROR: the bid bars grow away from the
   spread and the ask bars towards it, so the ladder reads as one depth profile
   rather than as two copies of the same column. */
static void book_level(const TrLevel *lv, bool bid, int barPx) {
    RC_Style s = rcGetStyle();
    rcRow(.gap = 6, .px = 8, .align = "cl", .w = "grow", .h = "20px") {
        rcBox(.w = "56px") {
            rcTextC(lv->priceStr, .font = F_SMALL, .color = bid ? tr_up_color() : tr_down_color());
        }
        rcBox(.align = "cr", .w = "38px") {
            rcTextC(lv->sizeStr, .font = F_SMALL, .color = s.textMuted);
        }
        if (bid) rcBox(.w = "grow") {}
        rcBox(.bg = rcAlpha(bid ? tr_up_color() : tr_down_color(), 90),
              .borderRadius = "all-sm", .h = "8px", .wType = RC_PX(barPx)) {}
        if (!bid) rcBox(.w = "grow") {}
    }
}

/* WHICH CANDLE THE POINTER IS OVER, or -1. Polled BEFORE the chart is declared,
   so the readout above it names the candle the chart highlights: hover is the
   state of the frame just drawn, which is what makes that ordering work. */
static int hovered_candle(int start) {
    for (int c = start; c < TR_MAX_CANDLES; c++)
        if (rcIsHovered(CANDLE_IDS[c]))
            return c;
    return -1;
}

/* The OHLC line every terminal puts at the head of its chart: the candle under
   the pointer, or the most recent one otherwise. */
static void ohlc_readout(const TrInstrument *it, int idx) {
    static const char *const KEY[4] = { "O", "H", "L", "C" };
    static char buf[4][16];
    RC_Style s = rcGetStyle();
    const TrCandle *cd = &it->candles[idx];
    const int32_t v[4] = { cd->open, cd->high, cd->low, cd->close };
    RC_Color dir = cd->close >= cd->open ? tr_up_color() : tr_down_color();

    for (int i = 0; i < 4; i++)
        tr__fmt_cents(v[i], buf[i], (int)sizeof buf[i]);
    rcRow(.gap = 10, .align = "cl", .overflow = "hidden", .w = "grow") {
        for (int i = 0; i < 4; i++) {
            rcRow(.gap = 3, .align = "cl") {
                rcTextC(KEY[i], .font = F_SMALL, .color = s.textMuted, .wrap = "n");
                rcTextC(buf[i], .font = F_SMALL, .color = dir, .wrap = "n");
            }
        }
    }
}

/* The candlestick chart: each candle is a column of five stacked integer-height
   segments - top gap, upper wick, body, lower wick, bottom gap - mapped from the
   shown window's range. Fewer candles simply grow wider, which reads as a zoom. */
#define TR_AXIS_W     52.0f   /* price-scale gutter, px */
#define TR_AXIS_TICKS 4       /* 5 labels: hi, three between, lo */
#define TR_TIME_H     14      /* the time axis under the plot */

/* The x axis, as offsets back from now. The series is a simulated walk, so a real
   date would be a fiction where an offset is not. */
static const char *const TR_TIME_LABELS[4][TR_AXIS_TICKS + 1] = {
    { "-12h", "-9h",  "-6h",  "-3h", "now" },
    { "-7d",  "-5d",  "-3d",  "-1d", "now" },
    { "-30d", "-22d", "-15d", "-7d", "now" },
    { "-12M", "-9M",  "-6M",  "-3M", "now" },
};

static void candle_chart(const TrInstrument *it, int count, int chart_h, int tf, int hover) {
    if (count > TR_MAX_CANDLES) count = TR_MAX_CANDLES;
    if (count < 1)             count = 1;
    int start = TR_MAX_CANDLES - count;               /* show the trailing `count` candles */
    int32_t lo = it->candles[start].low, hi = it->candles[start].high;
    for (int c = start + 1; c < TR_MAX_CANDLES; c++) {
        if (it->candles[c].low  < lo) lo = it->candles[c].low;
        if (it->candles[c].high > hi) hi = it->candles[c].high;
    }
    int32_t range = hi - lo;
    if (range < 1) range = 1;                         /* flat-series div-by-zero guard */

    /* THE PRICE SCALE. A candlestick chart without one is decoration: the
       shape is readable but no value can be read off it. The labels are the
       shown window's range, so they re-derive when the timeframe reslices it. */
    static char tickStr[TR_AXIS_TICKS + 1][16];
    for (int t = 0; t <= TR_AXIS_TICKS; t++) {
        int32_t v = hi - (int32_t)((int64_t)range * t / TR_AXIS_TICKS);
        tr__fmt_cents(v, tickStr[t], (int)sizeof tickStr[t]);
    }

    RC_Style st_ = rcGetStyle();
    rcColumn(.gap = 4, .w = "grow") {
    rcRow(.gap = 6, .w = "grow", .hType = RC_PX(chart_h)) {
    rcRow(.gap = 2, .w = "grow", .hType = RC_PX(chart_h)) {
        for (int c = start; c < TR_MAX_CANDLES; c++) {
            const TrCandle *cd = &it->candles[c];
            bool up = (cd->close >= cd->open);
            int32_t bodyTop = up ? cd->close : cd->open;   /* the higher price */
            int32_t bodyBot = up ? cd->open  : cd->close;
            int topGap = (int)((int64_t)(hi - cd->high)      * chart_h / range);
            int upWick = (int)((int64_t)(cd->high - bodyTop) * chart_h / range);
            int body   = (int)((int64_t)(bodyTop - bodyBot)  * chart_h / range);
            int loWick = (int)((int64_t)(bodyBot - cd->low)  * chart_h / range);
            if (body < 1) body = 1;                          /* a doji still shows a line */
            int botGap = chart_h - topGap - upWick - body - loWick;   /* absorbs the rounding */
            if (botGap < 0) botGap = 0;
            RC_Color col = up ? tr_up_color() : tr_down_color();
            /* the candle under the pointer keeps its colour and a band behind it
               while the rest fade back - the crosshair a ladder chart wants */
            bool hot = (c == hover);
            if (hover >= 0 && !hot) col = rcAlpha(col, 110);
            rcColumn(.id = CANDLE_IDS[c], .bg = hot ? rcAlpha(st_.text, 20) : RC_TRANSPARENT,
                     .align = "tc", .w = "grow", .hType = RC_PX(chart_h)) {
                if (topGap > 0) rcBox(.hType = RC_PX(topGap)) {}
                if (upWick > 0) rcBox(.bg = col, .w = "2px", .hType = RC_PX(upWick)) {}
                rcBox(.bg = col, .w = "grow", .hType = RC_PX(body)) {}
                if (loWick > 0) rcBox(.bg = col, .w = "2px", .hType = RC_PX(loWick)) {}
                if (botGap > 0) rcBox(.hType = RC_PX(botGap)) {}
            }
        }
    }
        /* The axis gutter: one label per tick, each at the top of its band, so
           label t reads the price t/TR_AXIS_TICKS of the way down the plot. */
        rcColumn(.wType = RC_PX(TR_AXIS_W), .hType = RC_PX(chart_h)) {
            for (int t = 0; t <= TR_AXIS_TICKS; t++) {
                rcTextC(tickStr[t], .font = F_SMALL, .color = st_.textMuted);
                if (t < TR_AXIS_TICKS) rcBox(.w = "grow", .h = "grow") {}
            }
        }
    }
        rcRow(.gap = 6, .w = "grow", .hType = RC_PX(TR_TIME_H)) {
            /* first label flush left, last flush right, the rest evenly spread -
               the same spacer idiom the price gutter uses */
            rcRow(.align = "cl", .w = "grow") {
                for (int t = 0; t <= TR_AXIS_TICKS; t++) {
                    rcTextC(TR_TIME_LABELS[tf & 3][t], .font = F_SMALL,
                             .color = st_.textMuted);
                    if (t < TR_AXIS_TICKS) rcBox(.w = "grow") {}
                }
            }
            rcBox(.wType = RC_PX(TR_AXIS_W)) {}
        }
    }
}

/* The market-status pill: a gradient fill REQUIRES a stable .id, and one element
   per frame carries it - the titlebar on the wide arm, a home pane's title row on
   the compact one. */
static void market_pill(bool dark) {
    RC_Style s = rcGetStyle();
    rcBox(.id = "mkt_pill", .px = 10, .py = 3, .align = "cc", .borderRadius = "all-full",
           .gradient = { .from = s.success, .to = s.successHover, .dir = "h" }) {
        rcTextL("MARKET OPEN", .font = F_SMALL, .color = tr_on_accent(dark));
    }
}

/* The custom titlebar. RC_ID_WINDOW_DRAG makes the band draggable, so it carries
   NO app widget - a toggle here loses its click to the window move - and only the
   window controls are exempt from the drag. The theme toggle lives in Settings.

   THE COMPACT ARM KEEPS THE BRAND AND THE CONTROLS AND MOVES THE PILL: brand,
   pill and the control cluster together want more width than a phone has, and
   both wrapped onto a second line. The window still has to close, so the cluster
   stays; the pill reappears on the title row of both home panes instead. */
static void trader_topbar(bool onePane, bool dark) {
    RC_Style s = rcGetStyle();
    /* CHROME, NOT CONTENT: the drag strip the user grabs is pinned at
       .titlebarHeight, so a band that scaled with the content zoom would stop
       matching it. rcUnzoomed() counter-scales the row by 1/zoom. */
    rcUnzoomed()
    rcRow(.id = RC_ID_WINDOW_DRAG, .bg = s.chrome, .gap = 10, .px = 14, .align = "cl",
          .w = "grow", .hType = RC_PX(TR_TOPBAR_H)) {
        rcBox(.bg = s.primary, .align = "cc", .borderRadius = "all-md", .w = "26px",
              .h = "26px") {
            rcTextL("RC", .font = F_SMALL, .color = RC_WHITE);
        }
        if (onePane) {
            rcTextL("RayClay Markets", .font = F_HEAD, .color = s.text, .wrap = "n");
        } else {
            rcTextL("RayClay Markets", .font = F_HEAD, .color = s.text);
            market_pill(dark);
        }
        rcBox(.w = "grow") {}
        rcWindowControls();
    }
}

static void trader_navrail(AppState *st) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surface, .gap = 6, .py = 12, .align = "tc", .w = "64px",
             .h = "grow") {
        if (nav_button("nav_markets",   rcIconPanelLeft, st->navTab == 0, "Markets"))   st->navTab = 0;
        if (nav_button("nav_portfolio", rcIconExpand,    st->navTab == 1, "Portfolio")) st->navTab = 1;
        /* a hairline closes the group of destinations, so the gap below it reads
           as deliberate rather than as a rail that ran out of entries */
        rcBox(.bg = s.border, .w = "28px", .h = "1px") {}
        rcBox(.w = "grow", .h = "grow") {}
        if (nav_button("nav_settings", rcIconSettings, false, "Settings")) {
            st->modalSettings = true;
            st->modalConfirm  = false;
        }
    }
}

/* One item of the compact tab bar: glyph over label, the whole cell a click target. */
static bool tab_item(const char *id, RC_IconCallback icon, const char *label, bool active) {
    RC_Style s = rcGetStyle();
    rcColumn(.id = id, .bg = rcIsHovered(id) ? s.surfaceAlt : RC_TRANSPARENT, .gap = 3,
             .align = "cc", .borderRadius = "all-md", .w = "grow", .h = "grow") {
        /* THE CURRENT TAB NEEDS A SHAPE, not just a hue: the icon and the label
           both turning one colour looks like two channels and is one. The pill is
           that shape, declared in BOTH states and transparent when the tab is not
           current, so nothing reflows. */
        rcBox(.bg = active ? s.primary : RC_TRANSPARENT, .borderRadius = "all-full",
              .wType = RC_PX(18), .hType = RC_PX(3)) {}
        icon(20.0f, active ? s.primary : s.textMuted);
        rcTextC(label, .font = F_SMALL, .color = active ? s.primary : s.textMuted);
    }
    return rcClicked(id);
}

/* The compact arm's stand-in for the nav rail: the same three controls under the
   SAME ids in a new place. Tapping Markets also pops the detail page back to the
   list, as tapping the current tab does on a phone. It carries the chart glyph
   because panel-left is the detail page's back control, and one glyph must not
   mean two things on one screen. */
static void trader_tabbar(AppState *st) {
    RC_Style s = rcGetStyle();
    rcRow(.bg = s.surface, .gap = 4, .px = 8, .py = 4, .w = "grow",
          .hType = RC_PX(TR_TABBAR_H)) {
        if (tab_item("nav_markets", rcIconChartColumn, "Markets", st->navTab == 0)) {
            st->navTab     = 0;
            st->detailOpen = false;
        }
        if (tab_item("nav_portfolio", rcIconExpand, "Portfolio", st->navTab == 1))
            st->navTab = 1;
        if (tab_item("nav_settings", rcIconSettings, "Settings", false)) {
            st->modalSettings = true;
            st->modalConfirm  = false;
        }
    }
}

/* The watchlist. Alone on the screen it IS the screen, so it grows instead of
   holding its 300 px column: the fixed width is what makes three panes fit, and
   a one-pane window has no three panes. Given enough of that width it becomes a
   TABLE with its own headings rather than a list stretched across the glass. */
static void trader_watchlist(AppState *st, bool onePane, bool table, bool finger) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surface, .gap = 8, .p = 10, .w = onePane ? "grow" : "300px",
             .h = "grow") {
        if (onePane) {
            /* the titlebar's market-status pill, in the room the compact band lacks */
            rcRow(.gap = 10, .align = "cl", .w = "grow") {
                rcTextL("Markets", .font = F_TITLE, .color = s.text);
                market_pill(st->darkMode);
            }
        } else {
            rcTextL("Markets", .font = F_TITLE, .color = s.text);
        }
        rcTextInput("search", st->search, sizeof st->search, .placeholder = "Search symbol");
        rcRow(.gap = 6, .w = "grow") {
            if (watch_pill("wf_all",  WATCH_VIEWS[0], st->watchFilter == 0, finger)) st->watchFilter = 0;
            if (watch_pill("wf_gain", WATCH_VIEWS[1], st->watchFilter == 1, finger)) st->watchFilter = 1;
            if (watch_pill("wf_lose", WATCH_VIEWS[2], st->watchFilter == 2, finger)) st->watchFilter = 2;
        }
        if (table)
            watch_head();
        rcColumn(.id = "WatchScroll", .gap = 3, .pr = 12, .scroll = "v", .w = "grow",
                 .h = "grow") {
            int n = tr_count(&st->store);
            for (int i = 0; i < n; i++) {
                const TrInstrument *it = tr_at(&st->store, i);
                if (!it)                    /* i < tr_count() so never NULL; guard silences -fanalyzer */
                    continue;
                /* filter by change sign + the search box, but ALWAYS keep the
                   selected row visible (its detail/order panels are what's open) */
                if (i != st->store.selected) {
                    if (st->watchFilter == 1 && it->changeBps <  0) continue;
                    if (st->watchFilter == 2 && it->changeBps >= 0) continue;
                    if (!tr_ci_contains(it->symbol, st->search) &&
                        !tr_ci_contains(it->name,   st->search)) continue;
                }
                if (watch_row(WATCH_IDS[i], it, i == st->store.selected, table)) {
                    tr_select(&st->store, i);
                    /* One pane at a time: picking a symbol IS the navigation, so
                       the tap also records that the user asked for the detail
                       page. Recorded in every arm, so a window dragged narrow
                       after a click lands on what was clicked. */
                    st->detailOpen = true;
                }
            }
        }
    }
}

/* "NVDA \xc2\xb7 1Y" - which instrument and which timeframe, at the chart's own head.
   Assembled by hand into a static buffer so the UI formats nothing. U+00B7 is in
   Latin-1, so the bundled face has it. */
static const char *chart_caption(const TrInstrument *it, int tf) {
    static const char *const TF_LABELS[] = { "1D", "1W", "1M", "1Y" };
    static char buf[24];
    const char *lab = TF_LABELS[tf & 3];
    int k = 0;
    for (int i = 0; it->symbol[i] && k < (int)sizeof buf - 8; i++)
        buf[k++] = it->symbol[i];
    buf[k++] = ' '; buf[k++] = '\xc2'; buf[k++] = '\xb7'; buf[k++] = ' ';
    for (int i = 0; lab[i] && k < (int)sizeof buf - 1; i++)
        buf[k++] = lab[i];
    buf[k] = '\0';
    return buf;
}

/* THE DAY RANGE, as a track with the live price marked inside it. It answers "is
   this near the top or the bottom of today" without a second chart and without a
   second thing to interact with. */
static void range_track(const TrInstrument *it, RC_Color marker, int trackW) {
    RC_Style s = rcGetStyle();
    int32_t span = it->dayHigh - it->dayLow;
    if (span < 1) span = 1;                           /* flat-session div-by-zero guard */
    int at = (int)((int64_t)(it->price - it->dayLow) * (trackW - 3) / span);
    if (at < 0)            at = 0;
    if (at > trackW - 3)   at = trackW - 3;
    rcRow(.bg = s.surfaceAlt, .align = "cl", .overflow = "hidden",
          .borderRadius = "all-full", .h = "8px", .wType = RC_PX(trackW)) {
        if (at > 0)
            rcBox(.bg = rcAlpha(marker, 120), .h = "8px", .wType = RC_PX(at)) {}
        rcBox(.bg = s.text, .w = "3px", .h = "8px") {}
    }
}

/* The detail pane's content. ONE body for two containers - the wide arm's column
   and the compact arm's scrolling page - so the two cannot drift apart. */
static void detail_body(AppState *st, const TrInstrument *it, bool onePane, int chartH) {
    RC_Style s = rcGetStyle();
    /* The bright cyan is a LABEL on a dark panel and unreadable on the light
       theme's white, where the fill shade takes both roles (the palette note). */
    RC_Color accent = st->darkMode ? rcHex(TR_CYAN) : s.primary;
    RC_Color dir    = tr_updown(it->changeBps);
    /* identity: this pane, the watchlist row and the order ticket are all showing
       one selection, and this is where that selection is stated */
    rcRow(.gap = 10, .align = "cl", .w = "grow") {
        /* One pane at a time needs a way back to the list. The SELECTION is
           deliberately left alone: the wide layout needs one, and closing a view
           is not changing it. */
        if (onePane && nav_button("nav_back", rcIconPanelLeft, false, "Back"))
            st->detailOpen = false;
        rcTextC(it->symbol, .font = F_TITLE, .color = s.text);
        rcBox(.overflow = "hidden", .w = "grow") {
            rcTextC(it->name, .font = F_BODY, .color = s.textMuted, .wrap = "n");
        }
        /* rcMenuItem RETURNS TRUE ON ACTIVATION, and an item whose return value is
           dropped is a row that does nothing when clicked. Each of these moves
           state the reader can see change on this same screen. */
        if (rcBeginMenu("menu_actions", "...")) {
            if (rcMenuItem("Buy"))              st->orderSide = TR_BUY;
            if (rcMenuItem("Sell"))             st->orderSide = TR_SELL;
            if (rcMenuItem("Full year chart"))  st->tf = 3;
            rcEndMenu();
        }
    }
    /* the HERO price plus the direction chip: an arrow and a signed percent on a
       tint of its own colour, so the reading survives colour blindness and a
       greyscale screenshot */
    rcRow(.gap = 12, .align = "bl", .w = "grow") {
        rcTextC(it->priceStr, .font = F_HERO, .color = s.text);
        rcRow(.bg = rcAlpha(dir, 38), .gap = 6, .px = 10, .py = 5, .align = "cc",
              .borderRadius = "all-full") {
            dir_arrow(it->changeBps >= 0, dir);
            rcTextC(it->changeStr, .font = F_MD, .color = dir);
        }
        rcTextL("since open", .font = F_SMALL, .color = s.textMuted);
    }
    /* the day range, and on the wide arm the day's two bounds either side of it */
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        rcTextL("Day range", .font = F_SMALL, .color = s.textMuted);
        rcTextC(it->dayLowStr, .font = F_SMALL, .color = s.text);
        range_track(it, accent, 160);
        rcTextC(it->dayHighStr, .font = F_SMALL, .color = s.text);
    }
    /* The chart's own head: the caption names instrument and timeframe, the OHLC
       readout tracks the candle under the pointer, and the segmented control
       changes the window. All three sit on the chart's band rather than above the
       price, because the timeframe is a property of the chart and nothing else. */
    static const int TF_CANDLES[] = { 12, 24, 36, 48 };   /* 1D/1W/1M/1Y trailing window */
    const int shown = TF_CANDLES[st->tf];
    const int hover = hovered_candle(TR_MAX_CANDLES - shown);
    rcRow(.gap = 10, .align = "cl", .w = "grow") {
        rcTextC(chart_caption(it, st->tf), .font = F_BODY, .color = accent);
        ohlc_readout(it, hover >= 0 ? hover : TR_MAX_CANDLES - 1);
        rcRow(.bg = s.surfaceAlt, .gap = 2, .p = 3, .align = "cc",
              .borderRadius = "all-lg") {
            static const char *const TFS[] = { "1D", "1W", "1M", "1Y" };
            static const char *const TF_IDS[] = { "tf0", "tf1", "tf2", "tf3" };
            for (int i = 0; i < 4; i++)
                if (tf_seg(TF_IDS[i], TFS[i], st->tf == i)) st->tf = i;
        }
    }
    /* the chart card: a gradient and a shadow both REQUIRE a stable .id, and the
       gradient is what anchors the shadow. Themed, so it reads right on the light
       palette too; the candle colours keep their own meaning. */
    RC_Color cardFrom = st->darkMode ? rcHex(0x18222f) : s.surface;
    RC_Color cardTo   = st->darkMode ? rcHex(0x0d131b) : s.surfaceAlt;
    rcBox(.id = "chart_card", .p = 12, .borderRadius = "all-lg", .w = "grow",
          .gradient = { .from = cardFrom, .to = cardTo, .dir = "v" },
          .shadow = { .color = rcAlpha(RC_BLACK, st->darkMode ? 90 : 30),
                       .y = 6.0f, .blur = 20.0f }) {
        candle_chart(it, shown, chartH, st->tf, hover);
    }
    /* THE ORDER BOOK IS TWO COLUMNS, NOT SIXTEEN STACKED ROWS: bids beside asks
       costs a third of the height, which is height the chart takes instead, and it
       is the shape a depth ladder has in every terminal. The spread is the one
       number the ladder exists to tell you, so it is stated. */
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        rcTextL("Order book", .font = F_SMALL, .color = s.textMuted);
        rcBox(.w = "grow") {}
        rcTextL("Spread", .font = F_SMALL, .color = s.textMuted);
        rcTextC(st->store.spreadStr, .font = F_SMALL, .color = s.text);
    }
    rcRow(.gap = 12, .w = "grow") {
        int maxSz = 1;
        for (int i = 0; i < TR_MAX_LEVELS; i++) {
            if (st->store.asks[i].size > maxSz) maxSz = st->store.asks[i].size;
            if (st->store.bids[i].size > maxSz) maxSz = st->store.bids[i].size;
        }
        rcColumn(.gap = 1, .w = "grow") {
            rcRow(.gap = 6, .px = 8, .align = "cl", .w = "grow", .h = "18px") {
                rcBox(.w = "56px") { rcTextL("Bids", .font = F_SMALL, .color = tr_up_color()); }
                rcBox(.align = "cr", .w = "38px") {
                    rcTextL("Size", .font = F_SMALL, .color = s.textMuted);
                }
            }
            for (int i = 0; i < TR_MAX_LEVELS; i++)
                book_level(&st->store.bids[i], true,
                            (int)((int64_t)st->store.bids[i].size * 52 / maxSz) + 1);
        }
        rcColumn(.gap = 1, .w = "grow") {
            rcRow(.gap = 6, .px = 8, .align = "cl", .w = "grow", .h = "18px") {
                rcBox(.w = "56px") { rcTextL("Asks", .font = F_SMALL, .color = tr_down_color()); }
                rcBox(.align = "cr", .w = "38px") {
                    rcTextL("Size", .font = F_SMALL, .color = s.textMuted);
                }
            }
            for (int i = 0; i < TR_MAX_LEVELS; i++)
                book_level(&st->store.asks[i], false,
                            (int)((int64_t)st->store.asks[i].size * 52 / maxSz) + 1);
        }
    }
}

/* THE CHART IS THIS PANE'S DOMINANT BLOCK, not one card among equals: it takes
   whatever height the pane's fixed furniture does not need, so a taller window
   spends its extra pixels on the candles rather than on air under the book.
   MEASURE TR_CHART_FIXED OFF A SCREENSHOT rather than adding up the source - a
   figure a few px short spends them on the chart and pushes the book's last level
   under the window edge. */
#define TR_CHART_FIXED  472
#define TR_CHART_MIN    200
#define TR_CHART_MAX    460
#define TR_CHART_COMPACT 200          /* the compact page scrolls, so it states a height */

static int chart_height(float paneH) {
    int h = (int)paneH - TR_CHART_FIXED;
    if (h < TR_CHART_MIN) h = TR_CHART_MIN;
    if (h > TR_CHART_MAX) h = TR_CHART_MAX;
    return h;
}

/* THE WIDE PANE SCROLLS TOO, because chart_height has a FLOOR and the furniture
   under it does not: once the pane is shorter than TR_CHART_FIXED + TR_CHART_MIN
   the chart stops giving height back and the book would run off the bottom edge.
   A desktop window dragged short reaches that long before a phone does, and the
   breakpoint is WIDTH, so neither hands the page to the compact arm. The scroller
   costs nothing while it fits - no bar is drawn for content inside its view. */
static void trader_detail(AppState *st, int chartH) {
    RC_Style s = rcGetStyle();
    const TrInstrument *it = tr_selected(&st->store);
    if (!it)
        return;                         /* no selection: guard BEFORE opening the element
                                           (a return inside would skip the close) */
    rcColumn(.id = "DetailWide", .bg = s.background, .gap = 12, .p = 16, .scroll = "v",
             .w = "grow", .h = "grow") {
        detail_body(st, it, false, chartH);
    }
}

/* The order form: side, quantity, type, the Limit price it reveals, the estimated
   cost and the Place order button. Its rows grow to the container it sits in - the
   wide arm's 320 px panel or the compact page's card. */
static void order_form(AppState *st) {
    RC_Style s = rcGetStyle();
    const TrInstrument *it = tr_selected(&st->store);
    /* THE TICKET NAMES ITS INSTRUMENT, and carries its live price: a ticket that
       named none would leave a user who changed the selection with nothing on the
       pane that buys to tell them what the button would buy. */
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        rcTextL("Order", .font = F_TITLE, .color = s.text);
        rcBox(.w = "grow") {}
        if (it) {
            rcRow(.bg = s.surfaceAlt, .gap = 6, .px = 10, .py = 5, .align = "cc",
                  .borderRadius = "all-full") {
                rcTextC(it->symbol, .font = F_SMALL, .color = s.text);
                rcTextC(it->priceStr, .font = F_SMALL, .color = s.textMuted);
            }
        }
    }
    rcRow(.gap = 6, .w = "grow") {
        if (seg_button("seg_buy", "Buy", st->orderSide == TR_BUY, tr_up_color(), st->darkMode))
            st->orderSide = TR_BUY;
        if (seg_button("seg_sell", "Sell", st->orderSide == TR_SELL, tr_down_color(), st->darkMode))
            st->orderSide = TR_SELL;
    }
    rcColumn(.gap = 4, .w = "grow") {
        rcTextL("Quantity", .font = F_SMALL, .color = s.textMuted);
        rcTextInput("qty_in", st->qty, sizeof st->qty, .placeholder = "0");
    }
    rcColumn(.gap = 4, .w = "grow") {
        rcTextL("Order type", .font = F_SMALL, .color = s.textMuted);
        rcCombo("cb_ordertype", &st->orderType, ORDER_TYPES, 2);
    }
    if (st->orderType == 1) {
        rcColumn(.gap = 4, .w = "grow") {
            rcTextL("Limit price", .font = F_SMALL, .color = s.textMuted);
            rcTextInput("limit_in", st->limitPx, sizeof st->limitPx, .placeholder = "0.00");
        }
    }
    const int  qty   = qty_to_int(st->qty);
    const bool ready = (qty > 0 && it != NULL);
    rcRow(.align = "cl", .w = "grow") {
        rcTextL("Est. cost", .font = F_SMALL, .color = s.textMuted);
        rcBox(.w = "grow") {}
        if (ready)
            rcTextC(st->store.estCostStr, .font = F_BODY, .color = s.text);
        else
            rcTextL("Enter a quantity", .font = F_BODY, .color = s.textMuted);
    }
    /* THE PRIMARY ACTION IS FULL WIDTH AND WEARS THE SIDE'S OWN COLOUR, because
       every other control in this ticket is full width and because a blue button
       under a green Buy segment does not say what it is about to do. It is a
       hand-rolled box for the width; polling rcClicked is what makes it a button,
       pointer cursor included, and NOT polling it while the quantity is empty is
       what makes the dimmed state a real disabled state rather than a paint job. */
    RC_Color side   = st->orderSide == TR_BUY ? tr_up_color() : tr_down_color();
    RC_Color sideHi = st->orderSide == TR_BUY ? s.successHover : s.dangerHover;
    const bool hot  = ready && rcIsHovered("btn_place");
    rcBox(.id = "btn_place", .bg = ready ? (hot ? sideHi : side) : rcAlpha(side, 70),
          .align = "cc", .borderRadius = "all-md", .w = "grow", .h = "38px") {
        rcRow(.gap = 6, .align = "cc") {
            rcTextC(st->orderSide == TR_BUY ? "Buy" : "Sell", .font = F_BODY,
                     .color = ready ? tr_on_accent(st->darkMode) : s.textMuted);
            if (it)
                rcTextC(it->symbol, .font = F_BODY,
                         .color = ready ? tr_on_accent(st->darkMode) : s.textMuted);
        }
    }
    if (ready && rcClicked("btn_place")) {
        /* the Settings "Confirm dialogs" toggle gates the confirm step */
        if (st->confirmDialogs) {
            st->modalConfirm  = true;
            st->modalSettings = false;
        } else {
            tr_place_order(&st->store, (TrSide)st->orderSide, qty);
        }
    }
}

/* The positions heading (with the cash balance) and its rows. The rows go into
   whatever column the caller opened - the wide arm's own scroller, or the compact
   page, where a nested scroller would fight the page's. */
static void positions_heading(AppState *st) {
    RC_Style s = rcGetStyle();
    rcRow(.gap = 6, .align = "cl", .w = "grow") {
        rcTextL("Positions", .font = F_SMALL, .color = s.textMuted);
        rcBox(.w = "grow") {}
        /* the balance is NAMED: a bare figure at the head of a holdings list reads
           as what the holdings are worth */
        rcTextL("Cash", .font = F_SMALL, .color = s.textMuted);
        rcTextC(st->store.cashStr, .font = F_SMALL, .color = s.text);
    }
}

static void positions_rows(AppState *st) {
    RC_Style s = rcGetStyle();
    int n = tr_count(&st->store);
    for (int i = 0; i < n; i++) {
        const TrInstrument *p = tr_at(&st->store, i);
        if (!p || p->position == 0)   /* !p: i < tr_count() so never NULL; guard silences -fanalyzer */
            continue;
        const char *id = POS_IDS[i];
        bool sel = (i == st->store.selected);
        int64_t pl = (int64_t)p->position * (p->price - p->avgCost);
        /* A HOLDING IS A WAY IN, NOT A READOUT: clicking one opens it in the chart
           and in the ticket above, the same selection the watchlist sets, and
           polling rcClicked is what gives the row its pointer cursor. The selected
           row is a TINT OF THE ACCENT rather than another step of surface, which
           on this palette would be darker and read as a hole. */
        rcRow(.id = id,
              .bg = sel ? rcAlpha(s.primary, 56)
                        : (rcIsHovered(id) ? rcAlpha(s.primary, 26) : s.surfaceAlt),
              .gap = 8, .pl = 5, .pr = 8, .align = "cl", .borderRadius = "all-md",
              .w = "grow", .h = "34px") {
            rcBox(.bg = sel ? s.primary : RC_TRANSPARENT, .borderRadius = "all-full",
                  .w = "3px", .h = "20px") {}
            rcBox(.w = "56px") { rcTextC(p->symbol, .font = F_SMALL, .color = s.text); }
            rcTextC(p->priceStr, .font = F_SMALL, .color = s.textMuted);
            rcBox(.w = "grow") {}
            /* a flat P/L is neither direction: no arrow, and the muted colour the
               rest of the row's secondary numbers use */
            if (pl != 0)
                dir_arrow(pl > 0, tr_updown(pl));
            rcTextC(p->plStr, .font = F_SMALL,
                     .color = pl != 0 ? tr_updown(pl) : s.textMuted);
        }
        if (rcClicked(id))
            tr_select(&st->store, i);
    }
}

/* THE FILL LOG: a ticket that ends in a confirmation and shows nothing for it
   leaves the user to take the fill on trust. The side is carried by the line's
   first word, so the reading never depends on the colour. */
static void fills_block(AppState *st) {
    RC_Style s = rcGetStyle();
    int n = tr_order_count(&st->store);
    rcTextL("Fills", .font = F_SMALL, .color = s.textMuted);
    if (n <= 0) {
        rcRow(.px = 8, .align = "cl", .w = "grow", .h = "26px") {
            rcTextL("No fills yet", .font = F_SMALL, .color = s.textMuted);
        }
        return;
    }
    for (int i = n - 1; i >= 0 && i > n - 4; i--) {
        const char *line = st->store.orders[i].line;
        rcRow(.bg = s.surfaceAlt, .px = 8, .align = "cl", .borderRadius = "all-md",
              .w = "grow", .h = "26px") {
            rcTextC(line, .font = F_SMALL,
                     .color = line[0] == 'B' ? tr_up_color() : tr_down_color());
        }
    }
}

/* The wide arm's order panel: the ticket, what the ticket just did, then what you
   hold. The fill log sits directly under the ticket because it is that ticket's
   receipt; the positions take the slack below it. */
static void trader_orderpanel(AppState *st) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.surface, .gap = 10, .p = 14, .w = "320px", .h = "grow") {
        order_form(st);
        rcBox(.bg = s.border, .w = "grow", .h = "1px") {}
        fills_block(st);
        rcBox(.bg = s.border, .w = "grow", .h = "1px") {}
        positions_heading(st);
        rcColumn(.id = "PosScroll", .gap = 4, .pr = 12, .scroll = "v", .w = "grow",
                 .h = "grow") {
            positions_rows(st);
        }
    }
}

/* The compact arm's detail PAGE: the detail pane, the order form and the
   positions as ONE scrolling column, so every control of the wide arm's two
   right-hand panes is still reachable. The page is the scroller, so the positions
   get none of their own. */
static void trader_detail_page(AppState *st) {
    RC_Style s = rcGetStyle();
    const TrInstrument *it = tr_selected(&st->store);
    if (!it)
        return;                         /* no selection: guard BEFORE opening the element */
    rcColumn(.id = "DetailScroll", .bg = s.background, .gap = 12, .p = 16, .scroll = "v",
             .w = "grow", .h = "grow") {
        detail_body(st, it, true, TR_CHART_COMPACT);
        rcColumn(.bg = s.surface, .gap = 10, .p = 14, .borderRadius = "all-lg",
                 .w = "grow") {
            order_form(st);
            rcBox(.bg = s.border, .w = "grow", .h = "1px") {}
            fills_block(st);
            rcBox(.bg = s.border, .w = "grow", .h = "1px") {}
            positions_heading(st);
            rcColumn(.gap = 4, .w = "grow") {
                positions_rows(st);
            }
        }
    }
}

/* The Portfolio view: a full-width holdings table, plus portfolio cash. THE
   COMPACT ARM KEEPS EVERY VALUE AND RESHAPES THE ROW - the name moves under the
   symbol, the numeric columns shrink to what their widest buffer needs, a long
   name wraps and the row grows to hold it, and the folded column is announced
   beside the title. */
static void trader_portfolio(AppState *st, bool onePane) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.background, .gap = 12, .p = 16, .w = "grow", .h = "grow") {
        rcRow(.gap = 10, .align = "cl", .w = "grow") {
            rcTextL("Portfolio", .font = F_TITLE, .color = s.text);
            if (onePane)
                market_pill(st->darkMode);   /* the titlebar's pill: on both compact home panes */
            rcBox(.w = "grow") {}
            /* A FOLDED COLUMN IS TOLD, NEVER DROPPED IN SILENCE: the compact arm
               carries Name under each symbol, and the count is how a reader tells
               a table showing everything from one that is not. */
            if (onePane) {
                rcBox(.id = "port_folded", .bg = rcAlpha(s.border, 120), .px = 8, .py = 3,
                      .align = "cl", .borderRadius = "all-full",
                      .tooltip = "Name is on each row's second line.") {
                    rcTextL("+1 column", .font = F_SMALL, .color = s.textMuted);
                }
            }
            rcTextL("Cash", .font = F_SMALL, .color = s.textMuted);
            rcTextC(st->store.cashStr, .font = F_BODY, .color = s.text);
        }
        /* the numeric column widths, wide / compact (the note above) */
        const char *wShares = onePane ? "44px" : "80px";
        const char *wPrice  = onePane ? "52px" : "90px";
        const char *wPl     = onePane ? "72px" : "110px";
        /* column headings (align the numeric ones right, matching the rows below).
           The leading 3 px box is the width of a row's selection bar: without it
           every numeric heading sits one bar plus one gap off its own column. */
        rcRow(.gap = 10, .px = 10, .align = "cl", .w = "grow") {
            rcBox(.w = "3px") {}
            rcBox(.w = onePane ? "grow" : "70px") {
                rcTextL("Symbol", .font = F_SMALL, .color = s.textMuted);
            }
            if (!onePane) {
                rcBox(.w = "grow")    { rcTextL("Name",   .font = F_SMALL, .color = s.textMuted); }
            }
            rcBox(.align = "cr", .w = wShares) { rcTextL("Shares", .font = F_SMALL, .color = s.textMuted); }
            rcBox(.align = "cr", .w = wPrice)  { rcTextL("Price",  .font = F_SMALL, .color = s.textMuted); }
            rcBox(.align = "cr", .w = wPl)     { rcTextL("P/L",    .font = F_SMALL, .color = s.textMuted); }
        }
        rcColumn(.id = "PortScroll", .gap = 4, .pr = 12, .scroll = "v", .w = "grow",
                 .h = "grow") {
            int n = tr_count(&st->store);
            for (int i = 0; i < n; i++) {
                const TrInstrument *p = tr_at(&st->store, i);
                if (!p || p->position == 0)   /* !p: i < tr_count() so never NULL; guard silences -fanalyzer */
                    continue;
                int64_t pl = (int64_t)p->position * (p->price - p->avgCost);
                const char *id = PORT_IDS[i];
                bool sel = (i == st->store.selected);
                /* the same selection the watchlist and the ticket carry: a holding
                   picked here is the instrument the Markets tab opens on */
                rcRow(.id = id,
                      .bg = sel ? rcAlpha(s.primary, 56)
                                : (rcIsHovered(id) ? rcAlpha(s.primary, 26) : s.surfaceAlt),
                      .gap = 10, .px = 10, .py = (uint16_t)(onePane ? 8 : 0),
                      .align = "cl", .borderRadius = "all-md", .w = "grow",
                      .h = onePane ? NULL : "44px", .hMin = onePane ? 52.0f : 0.0f) {
                    rcBox(.bg = sel ? s.primary : RC_TRANSPARENT,
                          .borderRadius = "all-full", .w = "3px", .h = "24px") {}
                    if (onePane) {
                        rcColumn(.gap = 2, .w = "grow") {
                            rcTextC(p->symbol, .font = F_BODY, .color = s.text);
                            rcTextC(p->name, .font = F_SMALL, .color = s.textMuted);
                        }
                    } else {
                        rcBox(.w = "70px") { rcTextC(p->symbol, .font = F_BODY, .color = s.text); }
                        rcBox(.overflow = "hidden", .w = "grow") {
                            rcTextC(p->name, .font = F_SMALL, .color = s.textMuted, .wrap = "n");
                        }
                    }
                    rcBox(.align = "cr", .w = wShares) { rcTextC(p->positionStr, .font = F_BODY, .color = s.text); }
                    rcBox(.align = "cr", .w = wPrice)  { rcTextC(p->priceStr,    .font = F_BODY, .color = s.text); }
                    rcBox(.align = "cr", .w = wPl) {
                        rcRow(.gap = 4, .align = "cr") {
                            if (pl != 0)          /* flat is neither direction */
                                dir_arrow(pl > 0, tr_updown(pl));
                            rcTextC(p->plStr, .font = F_BODY,
                                     .color = pl != 0 ? tr_updown(pl) : s.textMuted);
                        }
                    }
                }
                if (rcClicked(id))
                    tr_select(&st->store, i);
            }
        }
    }
}

/* The modals, placed OUTSIDE the root so their scrim covers the whole window.

   A FIXED 400 px IS WIDER THAN A PHONE, and a dialog centred in a narrower
   viewport loses its right-hand edge and the buttons on it. The compact arm sizes
   them in `vw` instead, which resolves against the layout viewport and is right
   in both zoom modes. */
static void trader_modals(AppState *st, bool onePane) {
    RC_Style s = rcGetStyle();
    const TrInstrument *it = tr_selected(&st->store);

    if (rcBeginModal("modal_confirm", &st->modalConfirm)) {
        rcColumn(.bg = s.surface, .gap = 12, .p = 18, .borderRadius = "all-xl",
                 .w = onePane ? "84vw" : "400px") {
            rcTextL("Confirm order", .font = F_TITLE, .color = s.text);
            rcRow(.gap = 6, .align = "cl", .w = "grow") {
                rcTextC(st->orderSide == TR_BUY ? "Buy" : "Sell", .font = F_BODY,
                         .color = st->orderSide == TR_BUY ? tr_up_color() : tr_down_color());
                rcTextC(st->qty[0] ? st->qty : "0", .font = F_BODY, .color = s.text);
                rcTextL("shares of", .font = F_BODY, .color = s.textMuted);
                rcTextC(it ? it->symbol : "--", .font = F_BODY, .color = s.text);
            }
            rcRow(.align = "cl", .w = "grow") {
                rcTextL("Est. cost", .font = F_SMALL, .color = s.textMuted);
                rcBox(.w = "grow") {}
                rcTextC(st->store.estCostStr, .font = F_BODY, .color = s.text);
            }
            rcRow(.gap = 8) {
                if (rcButton("btn_confirm", "Confirm", RC_BTN_PRIMARY)) {
                    tr_place_order(&st->store, (TrSide)st->orderSide, qty_to_int(st->qty));
                    st->modalConfirm = false;
                }
                if (rcButton("btn_cancel", "Cancel", RC_BTN_DEFAULT))
                    st->modalConfirm = false;
            }
        }
        rcEndModal();
    }

    if (rcBeginModal("modal_settings", &st->modalSettings)) {
        rcColumn(.bg = s.surface, .gap = 14, .p = 18, .borderRadius = "all-xl",
                 .w = onePane ? "84vw" : "400px") {
            rcTextL("Settings", .font = F_TITLE, .color = s.text);
            rcRow(.align = "cl", .w = "grow") {
                rcTextL("Dark mode", .font = F_BODY, .color = s.text);
                rcBox(.w = "grow") {}
                rcToggle("tg_dark_set", &st->darkMode);
            }
            rcRow(.align = "cl", .w = "grow") {
                rcTextL("Confirm dialogs", .font = F_BODY, .color = s.text);
                rcBox(.w = "grow") {}
                rcToggle("tg_confirm", &st->confirmDialogs);
            }
            rcRow(.gap = 12, .align = "cl", .w = "grow") {
                rcBox(.w = "140px") {
                    rcTextL("Watchlist view", .font = F_BODY, .color = s.text);
                }
                rcBox(.w = "grow") { rcCombo("cb_view", &st->watchFilter, WATCH_VIEWS, 3); }
            }
            rcRow(.gap = 8) {
                if (rcButton("btn_set_done", "Done", RC_BTN_PRIMARY))
                    st->modalSettings = false;
            }
        }
        rcEndModal();
    }
}

void trader_seed(AppState *st, unsigned seed) {
    tr_memzero(st, sizeof *st);          /* zero all, padding included, THEN set fields */
    tr_store_seed(&st->store, seed);
    st->navTab       = 0;
    st->watchFilter  = 0;
    st->tf           = 3;                 /* default to "1Y": the full 48-candle window */
    st->orderSide    = TR_BUY;
    st->orderType    = 0;
    st->darkMode     = true;
    st->confirmDialogs = true;
    st->detailOpen   = false;             /* the compact arm opens on the LIST, not on AAPL */
    /* A DEFAULT ORDER SIZE, so the ticket opens live: an empty quantity means a
       zero estimate and a disabled button, which is a poor first thing to meet. */
    st->qty[0]       = '1';
    st->qty[1]       = '0';
    st->seeded       = true;
}

void trader_update(AppState *st, const AppCtx *ctx) {
    tr_store_step(&st->store, ctx->dt);   /* dt <= 0 (freeze) => a no-op */
    /* Push the order price + qty into the backend so the est-cost buffer is precomputed
       (idempotent; the core displays only the precomputed string, never rcFormat). A
       Limit order prices at the user's price; set it BEFORE tr_set_qty reads it. */
    tr_set_order_px(&st->store, st->orderType == 1 ? px_to_cents(st->limitPx) : 0);
    tr_set_qty(&st->store, qty_to_int(st->qty));
}

void trader_layout(AppState *st, const AppCtx *ctx) {
    /* THE PALETTE IS INSTALLED HERE, in the shared core rather than in main.c, so
       every driver of this app renders the same app a user sees. */
    rcSetStyle(trader_style(st->darkMode));
    RC_Style s = rcGetStyle();

    /* One pane or several, and whether a one-pane window has the width for the
       table row. Read off AppCtx and NEVER off the live window, so a headless run
       at a fixed size takes the same branch as a real window that size. */
    const bool onePane = app_view_w(ctx) < TR_ONE_PANE_W;
    const bool table   = onePane && app_view_w(ctx) >= TR_LIST_TABLE_W;
    /* The pointer class the same way; it sizes the filter pills' hit targets and
       never the layout. */
    const bool finger  = ctx->view.coarsePointer;

    /* SAFE AREA, in layout space: the insets arrive in window px and padding is
       spent in layout px, so app_safe divides by the zoom. All zero on desktop. */
    RC_Insets safe = app_safe(ctx);

    /* The compact body stops a band short of the tab bar so the demo readout has
       somewhere to float that is over no row on any page. Zero on the wide arm,
       which has an empty corner under the order panel already. */
    const uint16_t band = onePane ? TR_HUD_BAND : 0;

    rcColumn(.id = "Root", .bg = s.background, .pt = (uint16_t)safe.top,
             .pb = (uint16_t)safe.bottom, .pl = (uint16_t)safe.left,
             .pr = (uint16_t)safe.right, .w = "grow", .h = "grow") {
        trader_topbar(onePane, st->darkMode);
        rcRow(.id = "Body", .pb = band, .w = "grow", .h = "grow") {
            /* Wide: rail, then the tab's panes side by side. Compact: the rail is
               withheld (its controls are the tab bar below), and on the Markets tab
               the watchlist is home and the detail page is what a tap opens, so the
               two are exclusive. */
            if (!onePane)
                trader_navrail(st);
            if (st->navTab == 1) {
                trader_portfolio(st, onePane);   /* Portfolio tab: the full-width holdings view */
            } else if (!onePane) {
                trader_watchlist(st, false, false, finger);
                trader_detail(st, chart_height(app_view_h(ctx) - (float)TR_TOPBAR_H));
                trader_orderpanel(st);
            } else if (st->detailOpen) {
                trader_detail_page(st);
            } else {
                trader_watchlist(st, true, table, finger);
            }
        }
        if (onePane)
            trader_tabbar(st);
    }
    trader_modals(st, onePane);     /* modals sit outside Root (full-window scrim) */

    /* EACH BAR CARRIES THE CONDITION ITS CONTAINER CARRIES. rcScrollbar draws the
       bar for a container laid out THIS frame; name one the frame did not build
       and the call is dropped with a warning, because from the outside a
       conditional container and a misspelt id are the same event. So this is the
       mirror of the Body block above, arm for arm. */
    const bool markets = st->navTab != 1;
    if (markets && (!onePane || !st->detailOpen)) rcScrollbar("WatchScroll");
    if (markets && !onePane)                      rcScrollbar("PosScroll");
    if (!markets)                                 rcScrollbar("PortScroll");
    if (markets && (!onePane || st->detailOpen))
        rcScrollbar(onePane ? "DetailScroll" : "DetailWide");
}

void trader_demo_chrome(AppState *st, const AppCtx *ctx) {
    if (ctx->mode != APP_DEMO || !ctx->arena)
        return;
    /* A floating perf readout, PASSTHROUGH so it never blocks a control. DEMO
       CHROME NEVER TAKES A SLOT A REAL APP NEEDS - the middle of a titlebar is the
       document-title slot, so this always sits in the bottom-right corner: under
       the order panel on the wide arm, and in the band the compact body leaves
       above the tab bar. Anchored to the ROOT, whose box includes the safe bands,
       so the nudge adds them and the chip stays inside the safe area. */
    const RC_Insets safe    = app_safe(ctx);
    const bool      onePane = app_view_w(ctx) < TR_ONE_PANE_W;
    const float     offY    = onePane ? -(16.0f + (float)TR_TABBAR_H + safe.bottom)
                                      : -16.0f;
    RC_String hud = rcFormat(ctx->arena, "%.0f fps \xc2\xb7 %d symbols \xc2\xb7 %d fills",
                                ctx->dt > 0.0f ? 1.0f / ctx->dt : 0.0f,
                                tr_count(&st->store), tr_order_count(&st->store));
    rcBox(.id = "demo_hud", .bg = rcAlpha(RC_BLACK, 150), .px = 10, .py = 5,
           .borderRadius = "all-full",
           .floating = { .to = RC_ATTACH_ROOT, .parent = RC_ANCHOR_BOTTOM_RIGHT,
                         .element = RC_ANCHOR_BOTTOM_RIGHT,
                         .offset = { -16.0f - safe.right, offY },
                         .capture = RC_CAPTURE_PASSTHROUGH }) {
        rcText(hud, .font = F_SMALL, .color = RC_WHITE);
    }
}

void trader_bench_step(AppState *st, const AppInputSink *in, int frame) {
    (void)st;   /* every action this app has is synthetic input */
    /* The scripted scenario: select a ticker, scroll, type a quantity, place an
       order and confirm it, then hold still, so a double-rendered frame is
       identical. The hold needs no focused input and no hovered control, because
       a caret blink and a tooltip dwell both read a real clock.

       EVERY COORDINATE BELOW IS THE CENTRE OF THE CONTROL IT AIMS AT, measured off
       the running window and not added up from the source. One that lands in the
       gap between two controls silently costs the whole action it was there to
       drive, so re-measure them whenever this layout's rhythm moves. */
    if (!in || frame >= TRADER_BENCH_WARMUP)
        return;                                        /* the HOLD */

    if (frame == 6) {
        in->move(in->ctx, 150.0f, 294.0f);            /* select an instrument (BEFORE any scroll) */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == 7) {
        in->button(in->ctx, APP_MBTN_LEFT, false);
    } else if (frame >= 10 && frame < 16) {
        in->wheel(in->ctx, 0.0f, -2.0f);              /* scroll the watchlist (pointer over it) */
    } else if (frame == 20) {
        in->move(in->ctx, 1180.0f, 174.0f);           /* focus the qty input (far-right x clamps the caret) */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == 21) {
        in->button(in->ctx, APP_MBTN_LEFT, false);
    } else if (frame >= 24 && frame < 27) {
        static const char QTY[] = "100";              /* type a fixed quantity */
        in->text(in->ctx, (unsigned int)(unsigned char)QTY[frame - 24]);
    } else if (frame == 32) {
        in->move(in->ctx, 1120.0f, 294.0f);           /* the order button -> opens the confirm modal */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == 33) {
        in->button(in->ctx, APP_MBTN_LEFT, false);
    } else if (frame == 40) {
        in->move(in->ctx, 490.0f, 405.0f);            /* Confirm -> tr_place_order (a fill appears) */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == 41) {
        in->button(in->ctx, APP_MBTN_LEFT, false);
    } else if (frame == TRADER_BENCH_WARMUP - 2) {
        in->move(in->ctx, -100.0f, -100.0f);          /* blur any input + park off-canvas: press ... */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == TRADER_BENCH_WARMUP - 1) {
        in->button(in->ctx, APP_MBTN_LEFT, false);     /* ... release; pointer stays off-canvas into the hold */
    }
}
