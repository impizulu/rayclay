/*
    app.h - the whole UI: state, and the layout that renders it

    One header, no .c beside it, every function `static inline`: main.c is the only
    translation unit, so there is nothing to link and no filename typed twice.

    Where things go: state and everything that draws here, the arithmetic behind it
    in view.h, visual choices in theme.h, the rows in orders.h (the dataset seam),
    and anything OS-specific in platform/platform.h, which is empty.

    The frame's one rule: a control only records what changed. Everything above it
    has already been drawn this frame, so the work runs at the top of the next one,
    and the frame request at the end of layout() makes that frame come.
*/
#ifndef APP_APP_H
#define APP_APP_H

#include "rayclay.h"

#include "icons/rc_icons_arrow_down.h"
#include "icons/rc_icons_arrow_up.h"
#include "icons/rc_icons_bookmark.h"
#include "icons/rc_icons_chart_column.h"
#include "icons/rc_icons_chevron_down.h"
#include "icons/rc_icons_chevron_right.h"
#include "icons/rc_icons_download.h"
#include "icons/rc_icons_search.h"
#include "icons/rc_icons_x.h"

#include "app/theme.h"
#include "app/orders.h"
#include "app/view.h"
#include "platform/platform.h"

typedef struct Column {
    const char *label;
    const char *width;     /* a fixed width, or "grow" to share what is left      */
    const char *sortId;    /* a literal: an id must outlive the frame             */
    float       wMin;      /* a grow column's least width; a fixed one is its own */
    bool        numeric;   /* right-align, as every table of numbers does         */
} Column;

/* A tile's trend keeps its scale near its own level: the band is the mean plus or minus the
   larger of the widest move and 15% of the mean, so a 1% wobble draws nearly flat and a 30%
   move still fills it. Fitted to its own min and max, a tile draws a small move as a cliff. */
static inline void spark_range(const float *y, int n, float *lo, float *hi) {
    float sum = 0.0f, far = 0.0f, mean, half;
    int   i;

    for (i = 0; i < n; i++)
        sum += y[i];
    mean = sum / (float)n;
    for (i = 0; i < n; i++) {
        float d = y[i] > mean ? y[i] - mean : mean - y[i];

        far = d > far ? d : far;
    }
    half = mean * 0.15f;
    half = half < 0.0f ? -half : half;
    half = far > half ? far : half;
    if (half <= 0.0f)
        half = 1.0f;
    *lo = mean - half;
    *hi = mean + half;
}
/* One table drives the header row, the sort keys and the cells, so a column cannot
   drift out of alignment with its own heading. */
static const Column COLUMN[C_COUNT] = {
    { "Order",    "88px",  "h_order",      0.0f, false },
    { "Date",     "108px", "h_date",       0.0f, false },
    { "Customer", "grow",  "h_customer", 148.0f, false },
    { "Product",  "grow",  "h_product",  176.0f, false },
    { "Category", "104px", "h_category",   0.0f, false },
    { "Status",   "108px", "h_status",     0.0f, false },
    { "Region",   "100px", "h_region",     0.0f, false },
    { "Channel",  "108px", "h_channel",    0.0f, false },
    { "Payment",  "104px", "h_payment",    0.0f, false },
    { "Units",    "64px",  "h_units",      0.0f, true },
    { "Discount", "80px",  "h_discount",   0.0f, true },
    { "Revenue",  "100px", "h_revenue",    0.0f, true }
};

/* The order columns stay in as the table narrows: what a reader asks of an order
   first is which one, who, how much and when. Picking a hidden column moves it up
   this list, so the list lives in state and the reader can change it. */
static const uint8_t COLUMN_IMPORTANCE[C_COUNT] = {
    C_ORDER, C_CUSTOMER, C_REVENUE, C_DATE, C_PRODUCT, C_STATUS, C_CATEGORY, C_UNITS,
    C_REGION, C_CHANNEL, C_DISCOUNT, C_PAYMENT
};

enum { PANE_CHARTS = 0, PANE_ORDERS, PANE_FILTERS, PANE_COUNT };
static const char *const PANE_LABEL[PANE_COUNT] = { "Charts", "Orders", "Filters" };
static const char *const PANE_ID[PANE_COUNT]    = { "pane0", "pane1", "pane2" };

static const char *const METRIC_NAME[M_COUNT] = {
    "Revenue", "Orders", "Avg order value", "Units sold", "Refund rate"
};
static const char *const METRIC_ID[M_COUNT]  = { "kpi0", "kpi1", "kpi2", "kpi3", "kpi4" };
static const char *const SPARK_ID[M_COUNT]   = { "sp0", "sp1", "sp2", "sp3", "sp4" };

/* The time ranges, counted back from the last day of data, each a whole number of
   weeks so no chart ends on a stub of a bar. */
enum { RANGE_COUNT = 5, RANGE_DEFAULT = 3, RANGE_ALL = 4 };
static const char *const RANGE_NAME[RANGE_COUNT] = {
    "Past 7 days", "Past 4 weeks", "Past 13 weeks", "Past 52 weeks", "All 24 months"
};
static const int RANGE_DAYS[RANGE_COUNT] = { 7, 28, 91, 364, ORDER_DAYS };

/* A saved view is nothing but filter state, so applying one is a few assignments. */
typedef struct SavedView {
    const char *name;
    int         dimA;        /* the facet whose values it checks, -1 for none     */
    uint32_t    bitsA;
    int         dimB;
    uint32_t    bitsB;
    int         range;
    int         metric;
    int         groupDim;
    float       minValue;
} SavedView;

enum { VIEW_COUNT = 5 };
static const SavedView SAVED_VIEW[VIEW_COUNT] = {
    { "All orders",              -1, 0, -1, 0, RANGE_DEFAULT, M_REVENUE, DIM_CATEGORY, 0.0f },
    /* A rate over every order, named for what it shows: a view that checked Refunded would
       list refunds, but under it every order is one and the Refund rate tile says 100%. */
    { "Refund rate by channel",  -1, 0, -1, 0, 2, M_REFUNDS, DIM_CHANNEL, 0.0f },
    { "Mobile app revenue",      DIM_CHANNEL, 3u, -1, 0, RANGE_DEFAULT, M_REVENUE,
      DIM_CATEGORY, 0.0f },
    { "Footwear in the West",    DIM_CATEGORY, 1u << 4, DIM_REGION, 1u << 5, RANGE_ALL,
      M_REVENUE, DIM_PRODUCT, 0.0f },
    { "Baskets over $300",       -1, 0, -1, 0, RANGE_DEFAULT, M_AOV, DIM_REGION, 300.0f }
};

/* Everything the layout decides from the space it has, derived once and read below,
   so two branches cannot disagree about the width they are in. */
typedef struct Fit {
    bool  desk;       /* the rail beside the main column                 */
    bool  docks;      /* an order opens in the side panel, not a modal   */
    bool  drawer;     /* the order panel beside the main column          */
    bool  coarse;     /* a finger: taller rows and targets               */
    bool  sideBySide; /* the trend and the top list share a row          */
    bool  tabsInline; /* the pane tabs share the query bar's row         */
    float rail;
    float main;       /* the main column's width                         */
    bool  kpiRow;     /* all five KPIs share one row                     */
    bool  low;        /* a short window: the KPIs give height back       */
    float tile;       /* one KPI tile's width                            */
    float rowH;
    float queryH;     /* the query bar, and the pane tabs beside it      */
} Fit;

typedef struct AppState {
    OrderBook book;
    View      view;
    Filters   filter;
    RC_App   *app;
    Palette   pal;             /* colors RC_Style has no role for that code reads */
    int       metric;
    int       groupDim;
    bool      lines;           /* the trend splits into lines rather than bars    */
    Col       sortCol;
    bool      sortDesc;
    int       range;           /* into RANGE_NAME, or -1 once a chart set the dates */
    int       rangeBefore;     /* where removing the date chip goes back to       */
    int       selected;        /* row index, or -1                                */
    bool      reveal;          /* scroll the table to the selected row            */
    bool      toTop;           /* a new result: back to the table's first row     */
    uint8_t   importance[C_COUNT];
    bool      sectionOpen[DIM_COUNT + 1];
    float     split;           /* the charts' share of the main column            */
    int       pane;            /* narrow layouts only                             */

    /* What changed this frame, for the top of the next one. */
    bool      rescan;
    bool      rederive;
    bool      resort;

    /* A drag across the trend chart, in bars. */
    bool      brushing;
    int       brushFrom;
    int       brushTo;

    /* The confirmation after an export. */
    RC_FileDialogToken saveToken;
    int       savedRows;
    char      toast[96];
    double    toastUntil;
} AppState;

/* ------------------------------------------------------------------ formatting */

/** Every printed count goes through here: "12,000" is read at a glance. */
static inline RC_String fmt_count(RC_Arena *mem, int64_t n) {
    return rcFormatInt(mem, n, ",");
}

static inline RC_String fmt_money(RC_Arena *mem, int64_t cents) {
    return rcFormat(mem, "$%s.%02d", fmt_count(mem, cents / 100).chars, (int)(cents % 100));
}

/** A metric, at the precision that metric is read at. */
static inline RC_String fmt_metric(RC_Arena *mem, int metric, double v) {
    switch (metric) {
    case M_REVENUE: return rcFormat(mem, "$%s", fmt_count(mem, (int64_t)(v + 0.5)).chars);
    case M_AOV:     return rcFormat(mem, "$%.2f", v);
    case M_REFUNDS: return rcFormat(mem, "%.1f%%", v);
    default:        return fmt_count(mem, (int64_t)(v + 0.5));
    }
}

static inline RC_String fmt_date(RC_Arena *mem, int day) {
    int y, m, d;

    day_to_date(day, &y, &m, &d);
    return rcFormat(mem, "%s %d, %d", MONTH_NAME[m], d, y);
}

static inline RC_String fmt_customer(RC_Arena *mem, const Order *o) {
    return rcFormat(mem, "%s %s", FIRST_NAME[o->first], LAST_NAME[o->last]);
}

/** A count and its noun: "1 row", "6,822 rows". */
static inline RC_String fmt_rows(RC_Arena *mem, int n) {
    return rcFormat(mem, "%s %s", fmt_count(mem, n).chars, n == 1 ? "row" : "rows");
}

/** "+1 column", "+5 columns". */
static inline RC_String fmt_hidden(RC_Arena *mem, int n) {
    return rcFormat(mem, "+%d %s", n, n == 1 ? "column" : "columns");
}

/** The y axis ticks: short, because a tick is read against its neighbors, never alone. */
static inline int chart_tick(double v, char *out, int cap, void *user) {
    AppState   *st  = (AppState *)user;
    RC_Arena   *mem = rcAppArena(st->app);
    Metric      m   = st->view.metric;
    const char *cur = m == M_REVENUE || m == M_AOV ? "$" : "";
    RC_String   s;

    if (m == M_REFUNDS)
        s = rcFormat(mem, "%.0f%%", v);
    else if (v >= 1000000.0)
        s = rcFormat(mem, "%s%.1fM", cur, v / 1000000.0);
    else if (v >= 1000.0 && (int)v % 1000 == 0)
        s = rcFormat(mem, "%s%.0fK", cur, v / 1000.0);
    else if (v >= 1000.0)
        s = rcFormat(mem, "%s%.1fK", cur, v / 1000.0);
    else
        s = rcFormat(mem, "%s%.0f", cur, v);
    rcStrCopy(out, s.chars, (size_t)cap);
    return s.length;
}

/** The hover readout prints the exact figure the tick rounded away. */
static inline int chart_readout(double v, char *out, int cap, void *user) {
    AppState *st = (AppState *)user;
    RC_String s  = fmt_metric(rcAppArena(st->app), st->view.metric, v);

    rcStrCopy(out, s.chars, (size_t)cap);
    return s.length;
}

/* ----------------------------------------------------------------- small pieces */

static inline void rule_h(RC_Color color) {
    rcBox(.bg = color, .w = "grow", .hType = RC_PX(1)) {}
}

static inline void rule_v(RC_Color color) {
    rcBox(.bg = color, .h = "grow", .wType = RC_PX(1)) {}
}

static inline void dot(RC_Color color, float side) {
    rcBox(.bg = color, .borderRadius = "all-full", .wType = RC_PX(side), .hType = RC_PX(side)) {}
}

/** A button's or a combo's class: 32 px, or a finger's 44 under a coarse pointer. An h-* is
    exact on every pointer, so the finger height has to be asked for as the mouse one is. */
static inline const char *control_class(void) {
    return rcPointerIsCoarse() ? "h-11" : "h-8";
}

/** A text link: the accent color, darker under the pointer. The caller polls the id. */
static inline void link_text(const char *id, const char *label) {
    RC_Style s = rcGetStyle();

    rcRow(.id = id, .focusable = true, .align = "cl") {
        rcTextC(label, .color = rcShowsHover(id) ? s.primaryHover : s.primary, .size = 13,
                .select = RC_SELECT_NONE);
    }
}

/** One segment of a segmented control. The track is the caller's row. */
static inline bool segment(const char *id, const char *label, bool on, float height) {
    RC_Style s = rcGetStyle();

    rcBox(.id = id, .focusable = true, .px = 12, .align = "cc", .borderRadius = "all-sm",
          .w = "grow", .hType = RC_PX(height),
          .className = on ? "bg-surface border border-border"
                          : "border border-transparent hover:bg-border") {
        rcTextC(label, .color = on ? s.text : s.textMuted, .size = 13,
                .select = RC_SELECT_NONE);
    }
    return rcClicked(id);
}

/* ---------------------------------------------------------------- state changes */

static inline void filter_reset(AppState *st) {
    int d;

    for (d = 0; d < DIM_COUNT; d++)
        st->filter.pick[d] = 0;
    st->filter.minValue = 0.0f;
    st->filter.maxValue = (float)VALUE_TOP;
    st->filter.query[0] = '\0';
}

static inline void set_range(AppState *st, int range) {
    st->range          = range;
    st->filter.dayTo   = ORDER_DAYS - 1;
    st->filter.dayFrom = ORDER_DAYS - RANGE_DAYS[range];
    st->rescan         = true;
    /* A brush counts the bars of the dates it began on: new dates are new bars, and its
       release would read the old bars' days out of the new plan. A key can do this mid-drag. */
    st->brushing       = false;
}

/** Clear all: the filters and the dates, not what the charts are set to show. */
static inline void clear_all(AppState *st) {
    filter_reset(st);
    set_range(st, RANGE_DEFAULT);
}

static inline void apply_saved_view(AppState *st, int i) {
    const SavedView *sv = &SAVED_VIEW[i];

    filter_reset(st);
    if (sv->dimA >= 0)
        st->filter.pick[sv->dimA] = sv->bitsA;
    if (sv->dimB >= 0)
        st->filter.pick[sv->dimB] = sv->bitsB;
    st->filter.minValue = sv->minValue;
    st->metric    = sv->metric;
    st->groupDim  = sv->groupDim;
    set_range(st, sv->range);
}

/** Check or uncheck one facet value, from wherever it was clicked. */
static inline void toggle_pick(AppState *st, int dim, int value) {
    st->filter.pick[dim] ^= 1u << value;
    st->rescan = true;
}

/** The selection's place in the current order, or -1 if a filter took it out. */
static inline int selected_rank(const View *v, int selected) {
    int i;

    for (i = 0; i < v->count; i++) {
        if (v->sel[i] == selected)
            return i;
    }
    return -1;
}

/** A row's whole export line; @p sep is ',' for a file, a tab for pasting into a sheet. */
static inline RC_String export_line(RC_Arena *mem, const OrderBook *b, int32_t row, char sep) {
    const Order *o = &b->row[row];
    int y, m, d;

    day_to_date(o->day, &y, &m, &d);
    return rcFormat(mem, "%d%c%04d-%02d-%02d%c%s %s%c%s%c%s%c%s%c%s%c%s%c%s%c%s%c%d%c%d%c%d.%02d\n",
                    ORDER_FIRST_ID + row, sep, y, m + 1, d, sep,
                    FIRST_NAME[o->first], LAST_NAME[o->last], sep,
                    PRODUCT[o->dim[DIM_PRODUCT]].name, sep,
                    CATEGORY_NAME[o->dim[DIM_CATEGORY]], sep,
                    STATUS_NAME[o->dim[DIM_STATUS]], sep,
                    REGION_NAME[o->dim[DIM_REGION]], sep,
                    CHANNEL_NAME[o->dim[DIM_CHANNEL]], sep,
                    PAYMENT_NAME[o->dim[DIM_PAYMENT]], sep,
                    SEGMENT_NAME[o->dim[DIM_SEGMENT]], sep,
                    o->units, sep, o->discount, sep, o->cents / 100, o->cents % 100);
}

static inline void show_toast(AppState *st, RC_String text) {
    rcStrCopy(st->toast, text.chars, sizeof st->toast);
    st->toastUntil = rcAppTime(st->app) + 4.0;
    rcWindowRequestFrameAfter(rcAppMainWindow(st->app), 4.05);
}

/** Write the filtered rows out: to a file the user names, or to the clipboard.
    Two arenas: one line at a time in `line`, reset per row, so the 12,000th row
    costs the same as the first, and the finished bytes in `out`. Both are the
    export's own and die with it: the save and the clipboard each take a copy. */
static inline void export_rows(AppState *st, bool toFile) {
    enum { EXPORT_LINE_BYTES = 256 };
    static const char *const HEAD =
        "Order,Date,Customer,Product,Category,Status,Region,Channel,Payment,Segment,"
        "Units,Discount,Revenue\n";
    const View *v = &st->view;
    char        sep = toFile ? ',' : '\t';
    size_t      cap = (size_t)(v->count + 2) * EXPORT_LINE_BYTES;
    RC_Arena    out = rcArenaInit(cap);
    RC_Arena    line = rcArenaInit(EXPORT_LINE_BYTES * 2);
    char       *bytes = (char *)rcArenaAlloc(&out, cap);
    size_t      n = 0;
    const char *h;
    int         i;

    if (bytes) {
        for (h = HEAD; *h; h++)
            bytes[n++] = *h == ',' ? sep : *h;
        for (i = 0; i < v->count; i++) {
            RC_String text = export_line(&line, &st->book, v->sel[i], sep);
            int       k;

            for (k = 0; k < text.length; k++)
                bytes[n++] = text.chars[k];
            rcArenaReset(&line);
        }
        bytes[n] = '\0';
        if (toFile) {
            st->saveToken = rcFileDialogSave("ridgeline-orders.csv", bytes, n);
            st->savedRows = v->count;
        } else {
            rcClipboardSet(bytes);
            show_toast(st, rcFormat(rcAppArena(st->app), "Copied %s to the clipboard",
                                    fmt_rows(rcAppArena(st->app), v->count).chars));
        }
    }
    rcArenaFree(&line);
    rcArenaFree(&out);
}

/** The save dialog answers on a later frame, and its answer wakes a parked app; polling is
    free, so this runs every frame and asks for none. */
static inline void poll_save(AppState *st) {
    RC_Arena           *mem = rcAppArena(st->app);
    RC_FileDialogResult r;

    if (!st->saveToken)
        return;
    r = rcFileDialogPoll(st->saveToken);
    if (r.status == RC_FILE_DIALOG_PENDING)
        return;
    st->saveToken = 0;
    if (r.status == RC_FILE_DIALOG_DONE)
        show_toast(st, rcFormat(mem, "Saved %s as ridgeline-orders.csv",
                                fmt_rows(mem, st->savedRows).chars));
    else if (r.status == RC_FILE_DIALOG_UNSUPPORTED)
        show_toast(st, rcStringFromCStr("No save dialog here. Copy the rows instead."));
    else if (r.status != RC_FILE_DIALOG_CANCELED)
        show_toast(st, rcStringFromCStr("The file was not saved. Copy the rows instead."));
}

/** Turn a typed `region:west` into its chip, so the query bar is a real query bar.
    Anything else stays in the field as free text, which filters as it is typed. */
static inline void query_submit(AppState *st) {
    char *q = st->filter.query;
    char  want[QUERY_CAP];
    int   d = query_dim(q), n = 0, i, v;

    while (q[n] && q[n] != ':')
        n++;
    if (d < 0 || !q[n] || !q[n + 1])
        return;
    for (i = 0; q[n + 1 + i]; i++)
        want[i] = fold_char(q[n + 1 + i]);
    want[i] = '\0';
    /* A value that starts with the word comes first, so region:west is West and not
       Midwest; then any value that contains it. */
    for (v = 0; v < dim_size(d) && !text_starts(dim_value(d, v), want); v++) {}
    if (v == dim_size(d)) {
        for (v = 0; v < dim_size(d) && !text_has(dim_value(d, v), want); v++) {}
    }
    if (v == dim_size(d)) {
        show_toast(st, rcFormat(rcAppArena(st->app), "No %s matches \"%s\"", DIM_KEY[d], want));
        return;
    }
    st->filter.pick[d] |= 1u << v;
    q[0] = '\0';
    st->rescan = true;
}

/* ---------------------------------------------------------------------- the band */

/* The band's widgets read the same style roles as every other widget, so a lent
   style is all it takes to give the combo and the menus their dark look. */
static inline RC_Style band_style(RC_Style base, const Palette *pal) {
    base.surface    = pal->bandRaised;
    base.surfaceAlt = pal->bandBorder;
    base.text       = pal->bandText;
    base.textMuted  = pal->bandMuted;
    base.border     = pal->bandBorder;
    return base;
}

static inline void saved_views_menu(AppState *st, bool words) {
    int i;

    if (rcBeginMenu("m_views", words ? "Saved views" : "", .caret = words,
                    .leadingIcon = rcIconBookmark)) {
        for (i = 0; i < VIEW_COUNT; i++) {
            if (rcMenuItem(SAVED_VIEW[i].name))
                apply_saved_view(st, i);
        }
        rcEndMenu();
    }
}

static inline void export_menu(AppState *st, bool words) {
    if (rcBeginMenu("m_export", words ? "Export" : "", .caret = words,
                    .leadingIcon = rcIconDownload)) {
        /* One save at a time: where the dialog leaves the window live, a second is refused
           and would take the token the first one's answer comes back on. */
        if (rcMenuItem("Download CSV", .disabled = st->view.count == 0 || st->saveToken))
            export_rows(st, true);
        if (rcMenuItem("Copy rows for a spreadsheet", .disabled = st->view.count == 0))
            export_rows(st, false);
        rcEndMenu();
    }
}

static inline void range_combo(AppState *st) {
    if (rcCombo("range", &st->range, RANGE_NAME, RANGE_COUNT, .className = control_class(),
                .placeholder = "Custom dates"))
        set_range(st, st->range);
}

/** The top band, which is also the titlebar: the whole band drags the window and its
    controls opt out, the desktop twin of CSS `-webkit-app-region: no-drag`. Chrome, not
    content, so it keeps its size at any zoom. */
static inline void top_band(AppState *st, const Fit *fit, RC_Insets safe, float windowW) {
    RC_Style base = rcGetStyle();
    RC_Style s    = band_style(base, &st->pal);
    bool     words = false;

    rcSetStyle(s);
    rcColumn(.bg = s.chrome, .pt = safe.top, .pl = safe.left,
             .pr = safe.right, .w = "grow") {
        rcUnzoomed() {
            /* This scope draws at window px, so the room is the window's and not the zoomed
               layout's: dividing by the scope's own scale undoes the zoom. */
            words = windowW / rcUnzoomedScale() >= EXP_MENU_LABEL_W;
            rcRow(.id = RC_ID_WINDOW_DRAG, .bg = s.chrome, .gap = 10, .pl = 16, .align = "cl",
                  .w = "grow", .hType = RC_PX(BAND_H)) {
                rcBox(.bg = s.primary, .align = "cc", .borderRadius = "all-md",
                      .wType = RC_PX(30), .hType = RC_PX(30)) {
                    rcIconChartColumn(17.0f, RC_WHITE);
                }
                if (words) {
                    rcTextL("Ridgeline Outfitters", .color = s.textMuted, .size = 14);
                    rcTextL("/", .color = s.textMuted, .size = 14);
                }
                rcTextL("Orders", .font = rcFontSemibold(15), .color = s.text);
                rcSpacer() {}
                rcRow(.gap = 8, .pr = 12, .align = "cl", .noDrag = true) {
                    saved_views_menu(st, words);
                    if (fit->desk) {
                        rcBox(.wType = RC_PX(164)) {
                            range_combo(st);
                        }
                    }
                    export_menu(st, words);
                }
                rcWindowControls();
            }
        }
    }
    rcSetStyle(base);
}

/* --------------------------------------------------------------------- the query */

/* The chips: one per facet in use, however many of its values are checked, so the bar
   stays one line until the query is really complicated. The chip's class deepens it under
   the pointer; the x is a glyph, with no state of its own, so it reads rcShowsHover. A chip
   is a Tab stop as tall as the row that scrolls it, which would clip a ring drawn around it,
   so ring-inset draws its focus ring inside it, as every stop here that meets an edge does. */
static inline void chip(const char *id, RC_String key, RC_String value, float height) {
    RC_Style s = rcGetStyle();

    rcRow(.id = id, .focusable = true, .gap = 6, .pl = 10, .pr = 8, .align = "cl",
          .borderRadius = "all-sm", .hType = RC_PX(height), .tooltip = "Remove this filter",
          .className = "chip shrink-0 ring-inset") {
        rcText(key, .color = s.textMuted, .size = 13, .select = RC_SELECT_NONE);
        rcText(value, .font = rcFontSemibold(13), .color = s.primaryHover,
               .select = RC_SELECT_NONE);
        rcIconX(13.0f, rcShowsHover(id) ? s.primaryHover : s.textMuted);
    }
}

static inline bool any_filter(const AppState *st) {
    const Filters *f = &st->filter;
    int d;

    for (d = 0; d < DIM_COUNT; d++) {
        if (f->pick[d])
            return true;
    }
    return st->range < 0 || f->query[0] || f->minValue > 0.0f || f->maxValue < (float)VALUE_TOP;
}

static inline void chip_date(AppState *st, RC_Arena *mem, float h) {
    if (st->range >= 0)
        return;
    chip("c_date", rcStringFromCStr("Dates"),
         rcFormat(mem, "%s - %s", fmt_date(mem, st->filter.dayFrom).chars,
                  fmt_date(mem, st->filter.dayTo).chars), h);
    if (rcClicked("c_date"))
        set_range(st, st->rangeBefore);
}

/* Literals, one per dimension: an id must outlive the frame. */
static const char *const CHIP_ID[DIM_COUNT] = {
    "c_d0", "c_d1", "c_d2", "c_d3", "c_d4", "c_d5", "c_d6"
};

static inline void chip_dims(AppState *st, RC_Arena *mem, float h) {
    int d, v;

    for (d = 0; d < DIM_COUNT; d++) {
        uint32_t    bits = st->filter.pick[d];
        const char *first = NULL;
        int         count = 0;

        if (!bits)
            continue;
        for (v = 0; v < dim_size(d); v++) {
            if ((bits >> v) & 1u) {
                if (!first)
                    first = dim_value(d, v);
                count++;
            }
        }
        chip(CHIP_ID[d], rcStringFromCStr(DIM_NAME[d]),
             count == 1 ? rcStringFromCStr(first) : rcFormat(mem, "%s +%d", first, count - 1),
             h);
        if (rcClicked(CHIP_ID[d])) {
            st->filter.pick[d] = 0;
            st->rescan = true;
        }
    }
}

static inline void chip_value(AppState *st, RC_Arena *mem, float h) {
    Filters *f = &st->filter;

    if (f->minValue <= 0.0f && f->maxValue >= (float)VALUE_TOP)
        return;
    chip("c_value", rcStringFromCStr("Order value"),
         f->maxValue >= (float)VALUE_TOP
             ? rcFormat(mem, "over $%.0f", (double)f->minValue)
             : rcFormat(mem, "$%.0f - $%.0f", (double)f->minValue, (double)f->maxValue),
         h);
    if (rcClicked("c_value")) {
        f->minValue = 0.0f;
        f->maxValue = (float)VALUE_TOP;
        st->rescan  = true;
    }
}

/** Backspace on an empty query takes off the last chip, as a mail client's To field does. */
static inline void chip_backspace(AppState *st) {
    Filters *f = &st->filter;
    int      d;

    if (f->minValue > 0.0f || f->maxValue < (float)VALUE_TOP) {
        f->minValue = 0.0f;
        f->maxValue = (float)VALUE_TOP;
        st->rescan  = true;
        return;
    }
    for (d = DIM_COUNT - 1; d >= 0; d--) {
        if (f->pick[d]) {
            f->pick[d] = 0;
            st->rescan = true;
            return;
        }
    }
    if (st->range < 0)
        set_range(st, st->rangeBefore);
}

/** The query bar: a field that holds chips. It is one bordered box wrapping a borderless
    rcTextInput, so a click anywhere in it focuses the field and the focus ring belongs
    to the whole box. */
static inline void query_field(AppState *st, RC_Arena *mem, const Fit *fit) {
    RC_Style s       = rcGetStyle();
    bool     focused = rcIsFocused("q");
    bool     wasEmpty = st->filter.query[0] == '\0';
    float    pad     = fit->coarse ? 4.0f : 6.0f;
    /* The bar's height is set, border in, so focus thickens its border inward instead of
       pushing the page down 2 px. The chips fill it less the padding and the 1 px a side. */
    float    chipH   = fit->queryH - 2.0f * (pad + 1.0f);
    /* Under a mouse the chips' scrollbar takes a 12 px band below them while they overflow
       (the last layout's answer, the one on screen). The query bar then gives that band its
       padding and lines its items up at the top, so the chips keep their height above it. */
    bool     chipBar = !fit->coarse && rcGetScrollInfo("q_chips").maxOffsetX > 0.0f;

    rcRow(.id = "qbar", .gap = 6, .pl = 12, .pr = 6, .py = chipBar ? 0.0f : pad,
          .align = chipBar ? "tl" : "cl", .borderRadius = "all-md", .w = "grow",
          .hType = RC_PX(fit->queryH),
          .className = focused ? "bg-surface border-2 border-primary min-w-0"
                               : "bg-surface border border-border min-w-0") {
        rcBox(.align = "cc", .hType = RC_PX(chipH)) {
            rcIconSearch(16.0f, s.textMuted);
        }
        /* ONE LINE, WHATEVER IT HOLDS: a chip that a slider or the trend's brush adds as it
           moves must not wrap the bar and slide the control out from under the pointer. A
           chip keeps its size (shrink-0, in chip) and this scrolling row gives way instead:
           past their room the chips scroll sideways, while the field keeps its 140 px, its
           clear x included. As in CSS, the chips' width still counts toward each flex item
           above the row, so the bar and the band's box beside the tabs carry min-w-0, or they
           would widen past the window. */
        rcRow(.id = "q_chips", .gap = 6, .align = "cl", .scroll = "h") {
            chip_date(st, mem, chipH);
            chip_dims(st, mem, chipH);
            chip_value(st, mem, chipH);
        }
        rcScrollbar("q_chips");
        /* The field fills a box as tall as the chips, so the two line up at any height. */
        rcBox(.w = "grow", .hType = RC_PX(chipH), .wMin = RC_VAL(140.0f)) {
            if (rcTextInput("q", st->filter.query, QUERY_CAP,
                            .className = "h-full border-0 bg-transparent pl-1 pr-1",
                            .placeholder = fit->desk && !any_filter(st)
                                ? "Search customer, product or order. Try region:west"
                                : "Search orders",
                            .clearButton = true))
                st->rescan = true;
        }
    }
    if (rcClicked("qbar"))
        rcSetFocus("q");
    if (rcKeyPressedOn("q", RC_KEY_ENTER))
        query_submit(st);
    if (wasEmpty && rcKeyPressedOn("q", RC_KEY_BACKSPACE))
        chip_backspace(st);
}

/** What came back: the count and, once the reader has moved something, how little
    that took. Hidden on the opening frame so a capture of it stays deterministic. */
static inline void result_note(AppState *st, RC_Arena *mem) {
    RC_Style    s = rcGetStyle();
    const View *v = &st->view;

    rcRow(.gap = 6, .align = "cl", .className = "items-baseline") {
        rcText(fmt_count(mem, v->count), .font = rcFontSemibold(14),
               .color = v->count ? s.text : st->pal.down);
        rcTextC(v->count == 1 ? "order" : "orders", .color = s.textMuted, .size = 13);
        if (v->rebuilds > 1) {
            rcBox(.id = "scan_ms",
                  .tooltip = "Every filter rescans all 12,000 orders, on this thread.") {
                rcText(v->scanMs < 0.1 ? rcStringFromCStr("in under 0.1 ms")
                                       : rcFormat(mem, "in %.1f ms", v->scanMs),
                       .color = s.textMuted, .size = 13);
            }
        }
    }
}

static inline void query_band(AppState *st, RC_Arena *mem, const Fit *fit, RC_Insets safe,
                              float bottom) {
    RC_Style s = rcGetStyle();

    rcRow(.bg = s.surface, .gap = 12, .pt = 10, .pb = bottom, .pl = 16.0f + safe.left,
          .pr = 16.0f + safe.right, .align = "cl", .w = "grow") {
        query_field(st, mem, fit);
        if (fit->desk) {
            result_note(st, mem);
            if (any_filter(st)) {
                link_text("clear_all", "Clear all");
                if (rcClicked("clear_all"))
                    clear_all(st);
            }
        }
    }
}

/* ------------------------------------------------------------------- the facets */

/* Literals, one per dimension. The value-range section is the last slot. */
static const char *const FACET_HEAD[DIM_COUNT + 1] = {
    "fh0", "fh1", "fh2", "fh3", "fh4", "fh5", "fh6", "fh7"
};
static const char *const FACET_CLEAR[DIM_COUNT + 1] = {
    "fc0", "fc1", "fc2", "fc3", "fc4", "fc5", "fc6", "fc7"
};

/** A facet section's header: chevron, name, how many are checked, and the way out. */
static inline bool section_head(AppState *st, RC_Arena *mem, int slot, const char *name,
                                int picked, bool active) {
    RC_Style s       = rcGetStyle();
    bool     cleared = false;

    /* A fixed height: the badge is taller than the name, and a row that grew when a value
       was checked would move every row below it out from under the pointer. */
    rcRow(.id = FACET_HEAD[slot], .focusable = true, .gap = 6, .px = 16, .align = "cl",
          .w = "grow", .hType = RC_PX(38), .className = "ring-inset") {
        (st->sectionOpen[slot] ? rcIconChevronDown : rcIconChevronRight)(14.0f, s.textMuted);
        rcTextC(name, .font = rcFontSemibold(13), .color = s.text, .select = RC_SELECT_NONE);
        if (picked > 0) {
            rcBox(.bg = s.primary, .px = 6, .align = "cc", .borderRadius = "all-full",
                  .hType = RC_PX(18)) {
                rcText(rcFormat(mem, "%d", picked), .color = RC_WHITE, .size = 12);
            }
        }
        rcSpacer() {}
        if (active) {
            link_text(FACET_CLEAR[slot], "Clear");
            cleared = rcClicked(FACET_CLEAR[slot]);
        }
    }
    if (rcClicked(FACET_HEAD[slot]))
        st->sectionOpen[slot] = !st->sectionOpen[slot];
    return cleared;
}

/** One value of a facet: the checkbox, then its count, or "Only" under the pointer, which
    is how a Datadog facet narrows to one value in one click. rcCheckbox's own label takes
    no ellipsis and a long product name must end in one, so the label is the row's: the
    box and the row each take a click, and either toggles the value. */
static inline void facet_row(AppState *st, RC_Arena *mem, int dim, int value, bool coarse) {
    RC_Style    s      = rcGetStyle();
    const char *rowId  = rcFormat(mem, "fr%d_%d", dim, value).chars;
    const char *boxId  = rcFormat(mem, "fb%d_%d", dim, value).chars;
    const char *onlyId = rcFormat(mem, "fo%d_%d", dim, value).chars;
    bool        on     = (st->filter.pick[dim] >> value) & 1u;

    rcRow(.id = rowId, .pl = 16, .pr = 16, .align = "cl", .w = "grow",
          .hType = coarse ? RC_FIT : RC_PX(28), .className = "hover:bg-surface-alt") {
        if (rcCheckbox(boxId, "", &on))
            toggle_pick(st, dim, value);
        rcBox(.pr = 8, .overflow = "hidden", .w = "grow") {
            rcTextC(dim_value(dim, value), .color = s.text, .size = 14, .wrap = "e",
                    .select = RC_SELECT_NONE);
        }
        if (!coarse && rcShowsHover(rowId)) {
            link_text(onlyId, "Only");
            if (rcClicked(onlyId)) {
                st->filter.pick[dim] = 1u << value;
                st->rescan = true;
            }
        } else {
            rcText(fmt_count(mem, st->view.facet[dim][value]), .color = s.textMuted, .size = 12);
        }
    }
    if (rcClicked(rowId))
        toggle_pick(st, dim, value);
}

static inline void facet_section(AppState *st, RC_Arena *mem, int dim, bool coarse) {
    RC_Style s = rcGetStyle();
    uint32_t bits = st->filter.pick[dim];
    int      picked = 0, v;

    for (v = 0; v < dim_size(dim); v++)
        picked += (int)((bits >> v) & 1u);

    rcColumn(.border = { .color = s.border }, .w = "grow", .className = "border-b") {
        if (section_head(st, mem, dim, DIM_NAME[dim], picked, picked > 0)) {
            st->filter.pick[dim] = 0;
            st->rescan = true;
        }
        if (st->sectionOpen[dim]) {
            rcColumn(.pb = 8, .w = "grow") {
                for (v = 0; v < dim_size(dim); v++)
                    facet_row(st, mem, dim, v, coarse);
            }
        }
    }
}

/** The measure facet: the shape of the data over two sliders. rcSlider holds one value, so
    each end of the range is its own slider, and each is kept on its own side of the other
    so the interval cannot turn inside out. */
static inline void value_section(AppState *st, RC_Arena *mem) {
    RC_Style s = rcGetStyle();
    Filters *f = &st->filter;
    bool     active = f->minValue > 0.0f || f->maxValue < (float)VALUE_TOP;

    rcColumn(.border = { .color = s.border }, .w = "grow", .className = "border-b") {
        if (section_head(st, mem, DIM_COUNT, "Order value", 0, active)) {
            f->minValue = 0.0f;
            f->maxValue = (float)VALUE_TOP;
            st->rescan  = true;
        }
        if (st->sectionOpen[DIM_COUNT]) {
            rcColumn(.gap = 6, .pb = 14, .px = 16, .w = "grow") {
                rcBox(.w = "grow", .hType = RC_PX(36)) {
                    rcSparkline("value_hist", st->view.hist, HIST_BINS, .kind = RC_SERIES_BAR,
                                .color = s.primary);
                }
                rcRow(.align = "cl", .w = "grow") {
                    rcText(rcFormat(mem, "$%.0f", (double)f->minValue), .color = s.text,
                           .size = 13);
                    rcSpacer() {}
                    rcText(f->maxValue >= (float)VALUE_TOP ? rcFormat(mem, "$%d+", VALUE_TOP)
                                                           : rcFormat(mem, "$%.0f",
                                                                      (double)f->maxValue),
                           .color = s.text, .size = 13);
                }
                if (rcSlider("v_min", &f->minValue, 0.0f, (float)VALUE_TOP, .step = 5.0f)) {
                    if (f->maxValue < f->minValue)
                        f->maxValue = f->minValue;
                    st->rescan = true;
                }
                if (rcSlider("v_max", &f->maxValue, 0.0f, (float)VALUE_TOP, .step = 5.0f)) {
                    if (f->minValue > f->maxValue)
                        f->minValue = f->maxValue;
                    st->rescan = true;
                }
            }
        }
    }
}

static inline void facet_list(AppState *st, RC_Arena *mem, bool coarse) {
    int d;

    for (d = DIM_CATEGORY; d <= DIM_STATUS; d++)
        facet_section(st, mem, d, coarse);
    value_section(st, mem);
    for (d = DIM_PAYMENT; d < DIM_COUNT; d++)
        facet_section(st, mem, d, coarse);
}

/** The desktop rail. It scrolls beside the table, never around it: a virtualized list
    needs a viewport that is the window's, not its parent's content. */
static inline void facet_rail(AppState *st, RC_Arena *mem, const Fit *fit) {
    RC_Style s = rcGetStyle();

    rcColumn(.bg = st->pal.rail, .border = { .color = s.border }, .h = "grow",
             .wType = RC_PX(fit->rail), .className = "border-r") {
        rcColumn(.id = "rail", .pr = 4, .scroll = "v", .w = "grow", .h = "grow") {
            facet_list(st, mem, fit->coarse);
        }
        rcScrollbar("rail");
    }
}

/* ------------------------------------------------------------------- the numbers */

/** Change against the same span before it, or nothing when the data starts too late to
    have one. Refund rate compares in points, since a percent of a percent misleads, and
    going up is the bad direction there. */
static inline void kpi_delta(AppState *st, RC_Arena *mem, int m, bool words) {
    const View *v = &st->view;
    RC_Style    s = rcGetStyle();
    float       now = metric_value(&v->total, (Metric)m);
    float       was = metric_value(&v->prior, (Metric)m);
    float       change = m == M_REFUNDS ? now - was
                       : was > 0.0f       ? 100.0f * (now - was) / was : 0.0f;
    float       size = change < 0.0f ? -change : change;
    bool        flat = size < 0.05f;   /* what prints as 0.0 has no direction to show */
    bool        good;
    RC_Color    tone;

    if (!v->total.orders) {
        rcTextL("no orders", .color = s.textMuted, .size = 12);
        return;
    }
    if (!v->hasPrior || !v->prior.orders) {
        rcTextL("no earlier period", .color = s.textMuted, .size = 12);
        return;
    }
    good = m == M_REFUNDS ? change <= 0.0f : change >= 0.0f;
    tone = flat ? s.textMuted : good ? st->pal.up : st->pal.down;
    rcRow(.gap = 3, .align = "cl") {
        if (!flat)
            (change >= 0.0f ? rcIconArrowUp : rcIconArrowDown)(12.0f, tone);
        rcText(rcFormat(mem, m == M_REFUNDS ? "%.1f pts" : "%.1f%%", (double)size),
               .color = tone, .size = 12);
        if (words)
            rcTextL("vs before", .color = s.textMuted, .size = 12);
    }
}

/** A KPI that is also the metric picker: click a number and every chart reads it. The figure
    owns its line and the trend is a soft band across the tile's foot, as on a stat panel. In a
    short window (a phone on its side) a tile keeps to its name and its number, and the
    change and the trend give their height to the chart below. */
static inline void kpi_tile(AppState *st, RC_Arena *mem, int m, const Fit *fit, bool half) {
    RC_Style    s  = rcGetStyle();
    const View *v  = &st->view;
    const char *id = METRIC_ID[m];
    bool        on = (int)v->metric == m;
    /* A year's revenue, "$1,026,673", is 121 px at 24 px and 111 px at 22, and a tile pads
       it by 32: the figure steps down a size as the tile narrows, so it is never cut. */
    float       figure = fit->low || fit->tile < 150.0f ? 18.0f
                       : fit->tile >= 200.0f              ? 24.0f : 22.0f;

    /* As tall as the strip's row, whatever its own content: the tiles' fills and their 2 px
       bottom edges then end on one line, and the names start on one line above. The tile is
       min-w-0, so a long name is cut by its clip below instead of widening its tile past the
       others; a half tile is 50% wide. */
    rcColumn(.id = id, .focusable = true, .gap = 2, .px = 16, .py = (float)(fit->low ? 6 : 12),
             .w = half ? "50%" : "grow", .h = "grow",
             .className = on ? "kpi-on min-w-0 ring-inset" : "kpi min-w-0 ring-inset") {
        /* One line: a name that wraps makes its tile a line taller than the rest. */
        rcBox(.overflow = "hidden", .w = "grow") {
            rcTextC(METRIC_NAME[m], .color = s.textMuted, .size = 13, .wrap = "e",
                    .select = RC_SELECT_NONE);
        }
        rcText(v->total.orders ? fmt_metric(mem, m, metric_value(&v->total, (Metric)m))
                               : rcStringFromCStr("-"),
               .font = rcFontSemibold(figure), .color = on ? s.primaryHover : s.text);
        if (!fit->low)
            kpi_delta(st, mem, m, fit->tile >= 200.0f);
        if (!fit->low && v->buckets > 1 && v->total.orders) {
            float lo, hi;
            int   n = v->buckets;

            /* An order younger than REFUND_DAYS_MAX cannot have been refunded yet, so the
               newest bars of the refund rate read near zero whatever the rate is. The trend
               ends at the last bar old enough to know. */
            if (m == M_REFUNDS) {
                while (n > 2 && v->bucketDay[n] > ORDER_DAYS - REFUND_DAYS_MAX)
                    n--;
            }
            spark_range(v->spark[m], n, &lo, &hi);
            rcBox(.pt = 6, .w = "grow", .hType = RC_PX(30)) {
                rcSparkline(SPARK_ID[m], v->spark[m], n, .kind = RC_SERIES_AREA,
                            .color = on ? s.primary : rcWithAlpha(s.textMuted, 0.5f),
                            .min = RC_VAL(lo), .max = RC_VAL(hi));
            }
        }
    }
    if (rcClicked(id)) {
        st->metric   = m;
        st->rederive = true;
    }
}

static inline void kpi_strip(AppState *st, RC_Arena *mem, const Fit *fit) {
    RC_Style s = rcGetStyle();
    int      m;

    if (fit->kpiRow) {
        rcRow(.bg = s.surface, .border = { .color = s.border }, .w = "grow",
              .className = "border-y") {
            for (m = 0; m < M_COUNT; m++) {
                if (m)
                    rule_v(s.border);
                kpi_tile(st, mem, m, fit, false);
            }
        }
        return;
    }
    /* Too narrow for one row: two across, the last one alone, and rules between. A left
       tile of exactly half keeps the rule on one line down the strip: "grow" would share
       the space by content, and each row would draw it somewhere else. */
    rcColumn(.bg = s.surface, .border = { .color = s.border }, .w = "grow",
             .className = "border-y") {
        for (m = 0; m < M_COUNT; m += 2) {
            if (m)
                rule_h(s.border);
            rcRow(.w = "grow") {
                kpi_tile(st, mem, m, fit, m + 1 < M_COUNT);
                if (m + 1 < M_COUNT) {
                    rule_v(s.border);
                    kpi_tile(st, mem, m + 1, fit, false);
                }
            }
        }
    }
}

/* ------------------------------------------------------------------- the charts */

/** The bar under a pointer x. With bars in the chart each category owns a slot one unit
    wide; a chart of lines only runs from the first category to the last. */
static inline int bar_at(const View *v, RC_Box plot, float x, bool lines) {
    float t = (x - plot.x) / plot.width;
    int   i = lines ? (int)(t * (float)(v->buckets - 1) + 0.5f) : (int)(t * (float)v->buckets);

    if (i < 0)
        i = 0;
    if (i >= v->buckets)
        i = v->buckets - 1;
    return i;
}

/** Narrow the dates to bars @p a .. @p b. The interval follows the range, so one month's
    bar redraws as its own days: drill-down, with no code for it. */
static inline void pick_bars(AppState *st, int a, int b) {
    const View *v = &st->view;
    int lo = a < b ? a : b, hi = a < b ? b : a;

    if (st->range >= 0)
        st->rangeBefore = st->range;
    st->range          = -1;
    st->filter.dayFrom = v->bucketDay[lo];
    st->filter.dayTo   = v->bucketDay[hi + 1] - 1;
    st->rescan         = true;
}

/** Click a bar or drag across several. A chart is not a widget with a callback and does
    not need to be: rcChartPlotRect says where the data is, rcPointer where the cursor
    is, and rcPressed says a press landed and gives the plot its hand cursor. A finger
    gets the plain tap, because its drag belongs to the pane's scroll. */
static inline void trend_brush(AppState *st, bool coarse) {
    RC_Style    s    = rcGetStyle();
    const View *v    = &st->view;
    RC_Box      plot = rcChartPlotRect("trend");
    RC_Box      box  = rcGetElementBox("trend_box");
    RC_Vec2     p    = st->brushing || rcPressed("trend") || rcClicked("trend")
                           ? rcPointer() : RC_LIT(RC_Vec2){ 0 };

    if (coarse) {
        if (rcClicked("trend") && plot.found && plot.width > 0.0f && v->buckets > 1) {
            int at = bar_at(v, plot, p.x, st->lines);

            pick_bars(st, at, at);
        }
        return;
    }
    /* The second half: .found only means the id exists, and an element's first frame
       is found with an all-zero rect. */
    if (!plot.found || !box.found || plot.width <= 0.0f || v->buckets < 2) {
        st->brushing = false;
        return;
    }
    if (rcPressed("trend") && p.x >= plot.x && p.x <= plot.x + plot.width) {
        st->brushing  = true;
        st->brushFrom = bar_at(v, plot, p.x, st->lines);
    }
    if (!st->brushing)
        return;

    st->brushTo = bar_at(v, plot, p.x, st->lines);
    {
        /* The dragged span, as a tint floating over the plot, in the same slots the bars
           sit in. PASSTHROUGH: the pointer still belongs to the chart beneath. */
        int   lo    = st->brushFrom < st->brushTo ? st->brushFrom : st->brushTo;
        int   hi    = st->brushFrom < st->brushTo ? st->brushTo : st->brushFrom;
        float slots = st->lines ? (float)(v->buckets - 1) : (float)v->buckets;
        float slot  = plot.width / (slots > 0.0f ? slots : 1.0f);
        float left  = (float)lo * slot;
        float width = st->lines ? (float)(hi - lo) * slot + 2.0f : (float)(hi - lo + 1) * slot;

        rcBox(.bg = rcWithAlpha(s.primary, 0.19f),
              .wType = RC_PX(width), .hType = RC_PX(plot.height),
              .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                            .element = RC_ANCHOR_TOP_LEFT,
                            .offset = { plot.x - box.x + left, plot.y - box.y },
                            .capture = RC_CAPTURE_PASSTHROUGH }) {}
    }
    /* After the band, so the release frame keeps it: the chart above was declared with the
       old range, and the zoomed one draws on the next frame. */
    if (rcPointerReleased(RC_POINTER_LEFT) || !rcPointerDown(RC_POINTER_LEFT)) {
        st->brushing = false;
        pick_bars(st, st->brushFrom, st->brushTo);
    }
}

/** A group's color, the same on its chart line and its top-list row. By value where every
    value has a hue, so a group keeps its color as the ranking moves; by rank where there
    are too many values for that, so no two drawn lines share one. Past the hues is the long
    tail, which no line draws. */
static inline RC_Color group_hue(const Palette *pal, int groupDim, int value, int rank) {
    int at = dim_size(groupDim) > GROUP_HUES ? rank : value;

    return at < GROUP_HUES ? pal->hue[at] : pal->context;
}

static inline void trend_chart(AppState *st, const Fit *fit) {
    RC_Style    s = rcGetStyle();
    View       *v = &st->view;
    RC_Series   series[SPLIT_MAX + 1] = { 0 };
    int         n = 0, j;
    /* The band only behind a selection big enough to read against it: a small one is drawn
       alone, so the axis fits it and its shape shows. An empty result keeps the band, which
       is then all the axis has to fit. */
    bool        narrowed = v->total.orders < v->rangeOrders &&
                           (v->total.orders == 0 || v->total.orders * 4 >= v->rangeOrders);
    /* A bar stays a bar: a day or two drawn at a slot's share would fill the plot. */
    float       barShare = v->buckets < 8 ? 0.74f * (float)v->buckets / 8.0f : 0.74f;

    if (st->lines && v->total.orders) {
        uint32_t bits = st->filter.pick[v->groupDim];

        for (j = 0; j < SPLIT_MAX && j < v->groups; j++) {
            int value = v->rank[j];

            /* A value the facet leaves out is ranked in the top list but has no line. */
            if (v->group[value].orders == 0 || (bits && !((bits >> value) & 1u)))
                continue;
            series[n].y     = v->splitY[j];
            series[n].count = v->buckets;
            series[n].kind  = RC_SERIES_LINE;
            series[n].color = group_hue(&st->pal, v->groupDim, value, j);
            series[n].label = dim_value(v->groupDim, value);
            n++;
        }
    } else {
        /* Every order in range, as a pale band behind the bars: without it a filter that
           removed 90% of the data still looks like the whole story. */
        if (narrowed) {
            series[n].y     = v->trendContext;
            series[n].count = v->buckets;
            series[n].kind  = RC_SERIES_AREA;
            series[n].color = st->pal.context;
            series[n].label = "All orders";
            n++;
        }
        series[n].y     = v->trend;
        series[n].count = v->buckets;
        series[n].kind  = RC_SERIES_BAR;
        series[n].color = s.primary;
        series[n].label = METRIC_NAME[v->metric];
        n++;
    }

    rcBox(.id = "trend_box", .w = "grow", .h = "grow") {
        rcChart("trend", series, n,
                .className    = "text-[12px]",
                .x = { .label  = v->span,
                       .ticks  = (uint8_t)(fit->main >= 800.0f && fit->desk ? 8 : 6),
                       .labels = v->bucketLabel },
                .y = { .ticks = 4, .grid = true, .format = chart_tick, .formatUserData = st,
                       .readoutFormat = chart_readout },
                /* A narrow chart wraps its legend onto more lines, each centered. */
                .legend       = st->lines && v->total.orders,
                .tooltip      = true,
                .barWidth     = barShare);
        trend_brush(st, fit->coarse);
    }
}

/** Does this group get a row? Not one with no orders, unless the reader checked it, which
    must stay in reach to uncheck. */
static inline bool top_row_shown(const AppState *st, int value) {
    const View *v = &st->view;

    return v->group[value].orders > 0 || ((st->filter.pick[v->groupDim] >> value) & 1u);
}

/** The list's heading and the group-by picker, here because this is the panel it changes:
    bars total every group, and only Lines mode splits the chart too. */
static inline void top_list_head(AppState *st) {
    RC_Style s = rcGetStyle();

    rcRow(.gap = 6, .pl = 8, .pr = 16, .align = "cl", .w = "grow") {
        rcBox(.overflow = "hidden", .w = "grow") {
            rcTextC(METRIC_NAME[st->view.metric], .font = rcFontSemibold(13), .color = s.text,
                    .wrap = "e");
        }
        rcTextL("by", .color = s.textMuted, .size = 13);
        rcBox(.wType = RC_PX(120)) {
            if (rcCombo("group", &st->groupDim, DIM_NAME, DIM_COUNT, .className = control_class()))
                st->rescan = true;
        }
    }
}

/** A ranked list: the leader's "Top List", and the part of "click a bar to filter" that a
    vertical bar chart cannot read well: long names, exact values, shares that add up.
    It fills the height it is given, or with @p capped it is as tall as its rows up to
    400 px: in a pane that scrolls there is no height to fill, and the rows know their own. */
static inline void top_list(AppState *st, RC_Arena *mem, bool capped) {
    RC_Style    s = rcGetStyle();
    View       *v = &st->view;
    uint32_t    bits = st->filter.pick[v->groupDim];
    bool        additive = v->metric != M_AOV && v->metric != M_REFUNDS;
    int         i;

    rcColumn(.id = "toplist", .gap = 2, .pr = 8, .scroll = "v", .w = "grow",
             .h = capped ? "fit" : "grow",
             .hMax = capped ? RC_VAL(400) : RC_UNSET_VAL) {
        for (i = 0; i < v->groups; i++) {
            int         value = v->rank[i];
            const char *id    = rcFormat(mem, "g%d", value).chars;
            float       val   = metric_value(&v->group[value], v->metric);
            /* The ratio first: it is exactly 1 at the leader, where 100 * val / max rounds
               past 100 for about one value in fifteen, and RC_PCT refuses that. */
            float       share = v->groupMax > 0.0f ? 100.0f * (val / v->groupMax) : 0.0f;
            bool        on    = (bits >> value) & 1u;
            RC_Color    hue   = group_hue(&st->pal, v->groupDim, value, i);

            if (!top_row_shown(st, value))
                continue;
            rcColumn(.id = id, .focusable = true, .gap = 3, .px = 8, .py = 4,
                     .borderRadius = "all-sm", .w = "grow",
                     .tooltip = on ? "Remove this filter" : "Filter to this",
                     .className = on ? "tint ring-inset" : "hover:bg-surface-alt ring-inset") {
                rcRow(.gap = 8, .align = "cl", .w = "grow") {
                    dot(bits && !on ? rcWithAlpha(hue, 0.353f) : hue, 8.0f);
                    rcBox(.overflow = "hidden", .w = "grow") {
                        rcTextC(dim_value(v->groupDim, value),
                                .color = bits && !on ? s.textMuted : s.text, .size = 13,
                                .wrap = "e", .select = RC_SELECT_NONE);
                    }
                    rcText(fmt_metric(mem, v->metric, val), .font = rcFontSemibold(13),
                           .color = bits && !on ? s.textMuted : s.text);
                    if (additive) {
                        rcRow(.align = "cr", .wType = RC_PX(40)) {
                            rcText(rcFormat(mem, "%.0f%%", v->groupSum > 0.0f
                                            ? (double)(100.0f * val / v->groupSum) : 0.0),
                                   .color = s.textMuted, .size = 12);
                        }
                    }
                }
                rcRow(.bg = s.surfaceAlt, .overflow = "hidden", .borderRadius = "all-full",
                      .w = "grow", .hType = RC_PX(4)) {
                    rcBox(.bg = bits && !on ? rcWithAlpha(hue, 0.353f) : hue, .h = "grow",
                          .wType = RC_PCT(share)) {}
                }
            }
            if (rcClicked(id))
                toggle_pick(st, v->groupDim, value);
        }
    }
    rcScrollbar("toplist");
}

/** The sentence that reads the chart. It says only what the chart draws: the group-by joins
    it in Lines mode, the one mode that splits the chart. */
static inline void viz_head(AppState *st, const Fit *fit) {
    RC_Style s = rcGetStyle();
    static const char *const EVERY[3] = { "day", "week", "month" };
    bool     stack = fit->main < 560.0f;

    rcRow(.gap = 8, .pt = 12, .pb = 8, .px = 16, .align = "cl", .w = "grow",
          .className = stack ? "flex-wrap" : NULL) {
        rcTextL("Show", .color = s.textMuted, .size = 13);
        rcBox(.wType = RC_PX(148)) {
            if (rcCombo("metric", &st->metric, METRIC_NAME, M_COUNT, .className = control_class()))
                st->rederive = true;
        }
        rcRow(.gap = 8, .align = "cl") {
            rcTextL("per", .color = s.textMuted, .size = 13);
            rcTextC(EVERY[st->view.interval], .font = rcFontSemibold(13), .color = s.text);
            if (st->lines) {
                rcTextL("by", .color = s.textMuted, .size = 13);
                rcTextC(DIM_NAME[st->view.groupDim], .font = rcFontSemibold(13), .color = s.text);
            }
        }
        rcSpacer() {}
        rcRow(.bg = s.surfaceAlt, .gap = 2, .p = 2, .borderRadius = "all-md",
              .wType = RC_PX(140)) {
            if (segment("v_bars", "Bars", !st->lines, 28.0f))
                st->lines = false;
            if (segment("v_lines", "Lines", st->lines, 28.0f))
                st->lines = true;
        }
    }
}

/* ---------------------------------------------------------------------- the table */

/** The list arm's "+N columns": plain words, not a control, since a row's own panel holds
    the rest, and a finger has no tooltip to be told so by. */
static inline void columns_tip(RC_Arena *mem) {
    RC_Style s = rcGetStyle();

    rcRow(.id = "t_more", .gap = 4, .align = "cl") {
        rcText(fmt_hidden(mem, C_COUNT - LIST_SHOWS), .color = s.textMuted, .size = 12);
        rcTextL("in each order", .color = s.textMuted, .size = 12);
    }
}

/** "+N columns" at the table's top right: a drop nobody is told about reads as a table that
    never had them. It opens on the hidden columns. A pick goes in right after the order's
    number, ahead of anything that could crowd it out, so it always shows; what does not
    fit takes its place in this menu, and the reader decides what the width buys. */
static inline void more_columns(AppState *st, RC_Arena *mem, uint64_t hidden) {
    int i, n = 0;

    for (i = 0; i < C_COUNT; i++)
        n += (int)(hidden >> i & 1u);
    if (!n)
        return;
    if (rcBeginMenu("t_more", fmt_hidden(mem, n).chars, .caret = true)) {
        for (i = 0; i < C_COUNT; i++) {
            int c = st->importance[i];

            if ((hidden >> c & 1u) && rcMenuItem(COLUMN[c].label)) {
                int at;

                for (at = i; at > 1; at--)
                    st->importance[at] = st->importance[at - 1];
                st->importance[1] = (uint8_t)c;
                /* This frame's heading row has the old columns: the next one has the new. */
                rcWindowRequestFrame(rcAppMainWindow(st->app));
                break;
            }
        }
        rcEndMenu();
    }
}

static inline void sort_by(AppState *st, int c) {
    if (st->sortCol == (Col)c) {
        st->sortDesc = !st->sortDesc;
    } else {
        st->sortCol  = (Col)c;
        st->sortDesc = COLUMN[c].numeric || c == C_DATE || c == C_ORDER;
    }
    st->resort = true;
}

/** The sortable header. rcBeginTable's own header takes plain text in one style, and this
    one needs a sort arrow and a hover. It sits outside the scrolling body, which is all
    a sticky header is, and it ends with the 12 px band the body's bar takes under a mouse
    while the rows overflow (the last layout's answer, the one on screen). Its cells take
    the columns' widths and least widths, so a heading stays over its column. */
static inline void table_header(AppState *st, uint64_t hidden) {
    RC_Style s = rcGetStyle();
    int      c;

    const bool band = !rcPointerIsCoarse() && rcGetScrollInfo("orders").maxOffsetY > 0.0f;

    rcRow(.bg = st->pal.rail, .pr = (float)(band ? 12 : 0),
          .border = { .color = s.border }, .w = "grow",
          .className = "border-y") {
        for (c = 0; c < C_COUNT; c++) {
            const Column *col    = &COLUMN[c];
            bool          active = st->sortCol == (Col)c;
            bool          over;
            RC_IconCallback arrow = st->sortDesc ? rcIconArrowDown : rcIconArrowUp;

            if (hidden >> c & 1u)
                continue;
            over = rcShowsHover(col->sortId);
            rcRow(.id = col->sortId, .focusable = true, .gap = 4,
                  .pl = (float)(c == C_ORDER ? 16 : 12), .pr = 12,
                  .align = col->numeric ? "cr" : "cl", .w = col->width,
                  .hType = RC_PX(32), .wMin = RC_VAL(col->wMin), .className = "ring-inset") {
                if (active && col->numeric)
                    arrow(12.0f, s.primary);
                rcTextC(col->label, .font = rcFontSemibold(12),
                        .color = active || over ? s.text : s.textMuted,
                        .select = RC_SELECT_NONE);
                if (active && !col->numeric)
                    arrow(12.0f, s.primary);
            }
            if (rcClicked(col->sortId))
                sort_by(st, c);
        }
    }
}

static inline void status_pill(const Palette *pal, int status) {
    rcRow(.bg = pal->statusFill[status], .px = 8, .align = "cc", .borderRadius = "all-full",
          .hType = RC_PX(22)) {
        rcTextC(STATUS_NAME[status], .color = pal->statusInk[status], .size = 12);
    }
}

static inline void order_cell(const AppState *st, RC_Arena *mem, const Order *o, int32_t row,
                              int c) {
    RC_Style s = rcGetStyle();

    switch (c) {
    case C_ORDER:
        rcText(rcFormat(mem, "#%d", ORDER_FIRST_ID + row),
               .color = row == st->selected ? s.primaryHover : s.textMuted, .size = 13);
        break;
    case C_DATE:
        rcText(fmt_date(mem, o->day), .color = s.text, .size = 13);
        break;
    case C_CUSTOMER:
        rcText(fmt_customer(mem, o), .color = s.text, .size = 13, .wrap = "e");
        break;
    case C_PRODUCT:
        rcTextC(PRODUCT[o->dim[DIM_PRODUCT]].name, .color = s.text, .size = 13, .wrap = "e");
        break;
    case C_CATEGORY:
        rcTextC(CATEGORY_NAME[o->dim[DIM_CATEGORY]], .color = s.text, .size = 13);
        break;
    case C_STATUS:
        status_pill(&st->pal, o->dim[DIM_STATUS]);
        break;
    case C_REGION:
        rcTextC(REGION_NAME[o->dim[DIM_REGION]], .color = s.text, .size = 13);
        break;
    case C_CHANNEL:
        rcTextC(CHANNEL_NAME[o->dim[DIM_CHANNEL]], .color = s.text, .size = 13);
        break;
    case C_PAYMENT:
        rcTextC(PAYMENT_NAME[o->dim[DIM_PAYMENT]], .color = s.text, .size = 13);
        break;
    case C_UNITS:
        rcText(rcFormat(mem, "%d", o->units), .color = s.text, .size = 13);
        break;
    case C_DISCOUNT:
        rcText(o->discount ? rcFormat(mem, "%d%%", o->discount) : rcStringFromCStr("-"),
               .color = o->discount ? s.text : s.textMuted, .size = 13);
        break;
    default:
        rcText(fmt_money(mem, o->cents), .font = rcFontSemibold(13), .color = s.text);
        break;
    }
}

/** One row. The id comes from the data index: under rcVirtualList the screen position IS
    the scroll window, so an id built from it renames every row on every scroll. The row
    carries the tint, under its cells; the cells are flush, draw the rule below the row, and
    each is pinned to the row height the virtual list paces its spacers by. A row is a Tab
    stop with its ring inside it, and no key clicks a table row, so Enter is read on it. */
static inline void order_row(AppState *st, RC_Arena *mem, uint64_t hidden, int32_t row,
                             float rowH) {
    const Order *o      = &st->book.row[row];
    const char  *id     = rcFormat(mem, "o%d", row).chars;
    bool         chosen = row == st->selected;
    RC_Color     rule   = st->pal.rule;
    int          c;

    rcTableRow(.id = id,
               .className = chosen ? "tint ring-inset" : "hover:bg-surface-alt ring-inset",
               .focusable = true);
    for (c = 0; c < C_COUNT; c++) {
        /* Every column gets its cell, so a value never lands under the wrong heading. A
           hidden one takes no room, and nothing need be drawn in it. */
        if (c > 0)
            rcTableNext();
        if (hidden >> c & 1u)
            continue;
        rcRow(.pl = (float)(c == C_ORDER ? 16 : 12), .pr = 12,
              .align = COLUMN[c].numeric ? "cr" : "cl",
              .overflow = "hidden", .border = { .color = rule }, .w = "grow",
              .hType = RC_PX(rowH), .className = "border-b") {
            order_cell(st, mem, o, row, c);
        }
    }
    if (rcClicked(id) || rcKeyPressedOn(id, RC_KEY_ENTER)) {
        st->selected = chosen ? -1 : row;
        st->reveal   = false;
    }
}

/** A link the user cannot see is not a link, so a row chosen by key stays in view.
    rcScrollToRow reaches a row the list has not declared and waits for its layout. */
static inline void reveal_selected(AppState *st) {
    int rank = selected_rank(&st->view, st->selected);

    if (rank >= 0)
        rcScrollToRow("orders", rank, RC_SCROLL_NEAREST);
    st->reveal = false;
}

/** Under a phone's width the columns fold into the row itself, as a store's order list does:
    who and how much on the first line, what on the second, the rest on the third. Whatever
    still has no place is one tap away in the order's own panel, and the tip says so. */
static inline void list_row(AppState *st, RC_Arena *mem, int32_t row, float rowH) {
    RC_Style     s      = rcGetStyle();
    const Order *o      = &st->book.row[row];
    const char  *id     = rcFormat(mem, "o%d", row).chars;
    RC_Color     rule   = st->pal.rule;

    rcTableRow(.id = id,
               .className = row == st->selected ? "tint ring-inset"
                                                : "hover:bg-surface-alt ring-inset",
               .focusable = true);
    rcColumn(.gap = 3, .pl = 16, .pr = 8, .align = "cl", .overflow = "hidden",
             .border = { .color = rule }, .w = "grow", .hType = RC_PX(rowH),
             .className = "border-b") {
        rcText(fmt_customer(mem, o), .font = rcFontSemibold(14), .color = s.text, .wrap = "e");
        rcTextC(PRODUCT[o->dim[DIM_PRODUCT]].name, .color = s.text, .size = 13, .wrap = "e");
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            rcText(rcFormat(mem, "#%d", ORDER_FIRST_ID + row), .color = s.textMuted, .size = 12);
            rcText(fmt_date(mem, o->day), .color = s.textMuted, .size = 12);
            rcTextC(CATEGORY_NAME[o->dim[DIM_CATEGORY]], .color = s.textMuted, .size = 12);
        }
    }
    rcTableNext();
    rcColumn(.gap = 6, .pr = 4, .align = "cr", .border = { .color = rule }, .w = "grow",
             .hType = RC_PX(rowH), .className = "border-b") {
        rcText(fmt_money(mem, o->cents), .font = rcFontSemibold(14), .color = s.text);
        status_pill(&st->pal, o->dim[DIM_STATUS]);
    }
    if (rcClicked(id) || rcKeyPressedOn(id, RC_KEY_ENTER)) {
        st->selected = row == st->selected ? -1 : row;
        st->reveal   = false;
    }
}

/** The sort control where the headings have folded away: a heading that is not on screen
    cannot be the only way to sort by it. One chip per column, in a row a thumb flicks
    sideways, keeping the header's own ids because they are the same controls. */
static inline void sort_chips(AppState *st) {
    RC_Style s = rcGetStyle();
    int      c;

    rcRow(.id = "t_sort", .gap = 6, .pb = 8, .px = 16, .align = "cl", .scroll = "h", .w = "grow") {
        for (c = 0; c < C_COUNT; c++) {
            const Column *col    = &COLUMN[c];
            bool          active = st->sortCol == (Col)c;

            rcRow(.id = col->sortId, .focusable = true, .gap = 4, .px = 10, .align = "cc",
                  .borderRadius = "all-full", .hType = RC_PX(rcPointerIsCoarse() ? 36 : 28),
                  .className = active ? "tint border border-primary ring-inset"
                                      : "border border-border hover:bg-surface-alt ring-inset") {
                rcTextC(col->label, .color = active ? s.primaryHover : s.text, .size = 13,
                        .select = RC_SELECT_NONE);
                if (active)
                    (st->sortDesc ? rcIconArrowDown : rcIconArrowUp)(12.0f, s.primary);
            }
            if (rcClicked(col->sortId))
                sort_by(st, c);
        }
    }
    rcScrollbar("t_sort");
}

/** The empty state is a sentence, and it names the way back out. */
static inline void table_empty(void) {
    RC_Style s = rcGetStyle();

    rcColumn(.id = "t_empty", .gap = 6, .p = 24, .align = "cc", .w = "grow", .h = "grow") {
        rcTextL("No orders match this query", .font = rcFontSemibold(15), .color = s.text);
        rcTextL("Remove a chip, or Clear all, to widen it.", .color = s.textMuted, .size = 13);
    }
}

/** The table's columns. A column's place in the reader's order
    is its .priority: the order's number, first, always shows, and as the table narrows the
    least important hides first. Customer and Product hold their .wMin meanwhile, so a lesser
    column goes before either is squeezed. */
static inline int table_columns(const AppState *st, bool list, RC_TableColumn *cols) {
    int rank[C_COUNT] = { 0 };
    int i, n = 0;

    if (list) {
        /* The first cell takes the row, the second the money and the status. */
        cols[n++] = RC_LIT(RC_TableColumn){ .w = "grow" };
        cols[n++] = RC_LIT(RC_TableColumn){ .w = "112px" };
    } else {
        for (i = 0; i < C_COUNT; i++)
            rank[st->importance[i]] = i;
        for (i = 0; i < C_COUNT; i++)
            cols[n++] = RC_LIT(RC_TableColumn){ .w = COLUMN[i].width, .priority = rank[i],
                                                .wMin = RC_VAL(COLUMN[i].wMin) };
    }
    return n;
}

/** The table: title, "+N columns", sticky header and the virtualized rows. It never scrolls
    sideways: the columns that do not fit hide, and in a pane narrower than EXP_LIST_W they
    fold into the rows. */
static inline void table_pane(AppState *st, RC_Arena *mem, float width, const Fit *fit) {
    RC_Style       s = rcGetStyle();
    bool           list = width < EXP_LIST_W;
    bool           rows = st->view.count > 0;
    /* Read ahead of the table, this is the last frame's answer, which is the one on screen.
       With no rows there is no table, so there are no headings to set over it. */
    uint64_t       hidden = list || !rows ? 0 : rcTableHiddenColumns("orders");
    RC_TableColumn cols[C_COUNT];
    float          rowH = list ? (float)ROW_H_LIST : fit->rowH;

    rcColumn(.bg = s.surface, .w = "grow", .h = "grow") {
        rcRow(.gap = 8, .px = 16, .align = "cl", .w = "grow", .hType = RC_PX(44)) {
            rcTextL("Orders", .font = rcFontSemibold(14), .color = s.text);
            rcText(fmt_rows(mem, st->view.count), .color = s.textMuted, .size = 13);
            rcSpacer() {}
            if (list)
                columns_tip(mem);
            else if (rows)
                more_columns(st, mem, hidden);
        }
        if (list)
            sort_chips(st);
        else if (rows)
            table_header(st, hidden);
        if (!rows) {
            table_empty();
        } else {
            /* Built after the menu, so a column picked this frame is in the rows already. */
            int n = table_columns(st, list, cols);

            rcBox(.w = "grow", .h = "grow") {
                if (rcBeginTable("orders", cols, n, .cellPadding = RC_VAL(0))) {
                    /* Past rcBeginTable the answer is this frame's: the cells follow it. */
                    const uint64_t drawn = list ? 0 : rcTableHiddenColumns("orders");

                    rcVirtualList(row, "orders", st->view.count, rowH) {
                        if (list)
                            list_row(st, mem, st->view.sel[row.index], rowH);
                        else
                            order_row(st, mem, drawn, st->view.sel[row.index], rowH);
                    }
                    rcEndTable();
                }
                /* Scroll calls come before the bar, the same frame the rows were declared. */
                if (st->toTop) {
                    rcScrollToTop("orders");
                    st->toTop = false;
                }
                if (st->reveal && st->selected >= 0)
                    reveal_selected(st);
                rcScrollbar("orders");
            }
        }
    }
}

/* ---------------------------------------------------------------- the order panel */

static inline void fact(const char *label, RC_String value) {
    RC_Style s = rcGetStyle();

    rcRow(.gap = 12, .align = "cl", .w = "grow") {
        rcTextC(label, .color = s.textMuted, .size = 13);
        rcSpacer() {}
        rcText(value, .color = s.text, .size = 13);
    }
}

/** A step of the fulfillment timeline. The stages have no dates of their own in the data,
    so orders.h derives them from the row, which is enough for a demo. */
static inline void step(RC_Arena *mem, const char *label, int day, bool done, bool last) {
    RC_Style s = rcGetStyle();

    rcRow(.gap = 10, .w = "grow") {
        rcColumn(.gap = 2, .align = "tc", .wType = RC_PX(10)) {
            dot(done ? s.primary : s.border, 10.0f);
            if (!last)
                rcBox(.bg = s.border, .wType = RC_PX(2), .hType = RC_PX(22)) {}
        }
        rcRow(.gap = 8, .w = "grow") {
            rcTextC(label, .color = done ? s.text : s.textMuted, .size = 13);
            rcSpacer() {}
            if (done)
                rcText(fmt_date(mem, day), .color = s.textMuted, .size = 13);
        }
    }
}

/** A button in the order's body changed the result. The side panel stays beside it; a modal
    would sit over it, so the modal closes and the reader lands on the new rows. */
static inline void leave_order(AppState *st, bool inDrawer) {
    st->rescan = true;
    if (!inDrawer) {
        st->selected = -1;
        st->pane     = PANE_ORDERS;
    }
}

/** Everything about one order. The same body sits in the side panel and, where there is no
    room for one, in a modal, so the two cannot disagree. */
static inline void order_body(AppState *st, RC_Arena *mem, bool inDrawer) {
    RC_Style     s      = rcGetStyle();
    int32_t      row    = st->selected;
    const Order *o      = &st->book.row[row];
    int          status = o->dim[DIM_STATUS];
    const Product *p    = &PRODUCT[o->dim[DIM_PRODUCT]];
    int64_t      gross  = (int64_t)p->cents * o->units;
    bool         refunded = status == STATUS_REFUNDED;

    rcColumn(.gap = 16, .w = "grow") {
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            rcText(rcFormat(mem, "Order #%d", ORDER_FIRST_ID + row), .font = rcFontSemibold(18),
                   .color = s.text);
            rcSpacer() {}
            if (rcButton("d_close", "", .className = control_class(), .variant = RC_BTN_GHOST,
                         .leadingIcon = rcIconX))
                st->selected = -1;
        }
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            status_pill(&st->pal, status);
            rcText(fmt_date(mem, o->day), .color = s.textMuted, .size = 13);
        }
        /* The actions under the heading, as a detail pane keeps them: a short window then
           scrolls the facts below, never the buttons, out of sight. */
        rcRow(.gap = 8, .w = "grow", .className = "flex-wrap") {
            /* A query of the whole name: every order from this customer, a click away. */
            if (rcButton("d_customer", "Their orders", .className = control_class())) {
                rcStrCopy(st->filter.query, fmt_customer(mem, o).chars, QUERY_CAP);
                leave_order(st, inDrawer);
            }
            if (rcButton("d_product", "This product", .className = control_class())) {
                st->filter.pick[DIM_PRODUCT] = 1u << o->dim[DIM_PRODUCT];
                leave_order(st, inDrawer);
            }
            if (rcButton("d_copy", "Copy", .className = control_class())) {
                rcClipboardSet(export_line(mem, &st->book, row, '\t').chars);
                show_toast(st, rcStringFromCStr("Copied this order"));
            }
        }
        rule_h(st->pal.rule);
        rcColumn(.gap = 8, .w = "grow") {
            rcTextL("Customer", .font = rcFontSemibold(13), .color = s.text);
            fact("Name", fmt_customer(mem, o));
            fact("Segment", rcStringFromCStr(SEGMENT_NAME[o->dim[DIM_SEGMENT]]));
            fact("Region", rcStringFromCStr(REGION_NAME[o->dim[DIM_REGION]]));
        }
        rule_h(st->pal.rule);
        rcColumn(.gap = 8, .w = "grow") {
            rcTextL("Items", .font = rcFontSemibold(13), .color = s.text);
            rcTextC(p->name, .color = s.text, .size = 13);
            fact(CATEGORY_NAME[p->category],
                 rcFormat(mem, "%d x %s", o->units, fmt_money(mem, p->cents).chars));
            fact("Subtotal", fmt_money(mem, gross));
            if (o->discount)
                fact(rcFormat(mem, "Discount %d%%", o->discount).chars,
                     rcFormat(mem, "-%s", fmt_money(mem, gross - o->cents).chars));
            rcRow(.gap = 12, .align = "cl", .w = "grow") {
                rcTextL("Total", .font = rcFontSemibold(14), .color = s.text);
                rcSpacer() {}
                rcText(fmt_money(mem, o->cents), .font = rcFontSemibold(14), .color = s.text);
            }
        }
        rule_h(st->pal.rule);
        rcColumn(.gap = 8, .w = "grow") {
            rcTextL("Payment and fulfillment", .font = rcFontSemibold(13), .color = s.text);
            fact("Channel", rcStringFromCStr(CHANNEL_NAME[o->dim[DIM_CHANNEL]]));
            fact("Paid with", rcStringFromCStr(PAYMENT_NAME[o->dim[DIM_PAYMENT]]));
        }
        rcColumn(.gap = 2, .w = "grow") {
            if (o->dim[DIM_CHANNEL] == CHANNEL_RETAIL) {
                step(mem, "Purchased in store", o->day, true, !refunded);
            } else {
                step(mem, "Placed", o->day, true, false);
                step(mem, "Shipped", order_shipped_day(o, row), status != STATUS_PROCESSING, false);
                step(mem, "Delivered", order_delivered_day(o, row),
                     status == STATUS_DELIVERED || refunded, !refunded);
            }
            if (refunded)
                step(mem, "Refunded", order_refunded_day(o, row), true, true);
        }
    }
}

/* ------------------------------------------------------------------- the panes */

/** The charts' pane for a window with no room for both side by side, or the two side by
    side. A pane that scrolls has no height to share out, so the chart's is fixed there (a
    chart in a box that sizes to its content has only its 150 px floor) and the list is as
    tall as its rows. */
static inline void viz_charts(AppState *st, RC_Arena *mem, const Fit *fit, bool scrolls) {
    if (fit->sideBySide) {
        rcRow(.w = "grow", .hType = scrolls ? RC_PX(280) : RC_GROW) {
            rcColumn(.pb = 8, .pl = 16, .pr = 12, .h = "grow", .wType = RC_PCT(62)) {
                trend_chart(st, fit);
            }
            rule_v(st->pal.rule);
            /* No right padding: the list's own 16 px end the picker and the bars on the edge
               the switch above and the table's head below end on. */
            rcColumn(.gap = 4, .pb = 8, .pl = 8, .w = "grow", .h = "grow") {
                top_list_head(st);
                top_list(st, mem, false);
            }
        }
    } else {
        /* The chart ends 20 px and the list 16 px from the pane's edge, the scrollbar band
           the pane takes under a mouse while it scrolls counted in. */
        float lane = scrolls && !fit->coarse ? 12.0f : 0.0f;

        rcBox(.pl = 12, .pr = 20.0f - lane, .w = "grow", .hType = RC_PX(250)) {
            trend_chart(st, fit);
        }
        rcColumn(.gap = 4, .pb = 12, .pl = 8, .pr = 16.0f - lane, .w = "grow",
                 .hType = scrolls ? RC_FIT : RC_GROW) {
            top_list_head(st);
            top_list(st, mem, scrolls);
        }
    }
}

static inline void desk_main(AppState *st, RC_Arena *mem, const Fit *fit) {
    RC_Style s = rcGetStyle();

    /* min-w-0, as a flex item beside the rail: the table's columns would otherwise be this
       column's least width, widen it past the window and leave .priority nothing to hide. */
    rcColumn(.bg = s.surface, .w = "grow", .h = "grow", .className = "min-w-0") {
        kpi_strip(st, mem, fit);
        if (rcBeginSplitPane("split", RC_SPLIT_COLUMN, &st->split, .minFraction = 0.28f,
                             .maxFraction = 0.72f)) {
            rcColumn(.w = "grow", .h = "grow") {
                viz_head(st, fit);
                viz_charts(st, mem, fit, false);
            }
            rcSplitHandle();
            table_pane(st, mem, fit->main, fit);
            rcEndSplitPane();
        }
    }
}

/** A narrow window: one pane at a time, each with the whole window, because this arm exists
    since the panes stopped fitting, not because this is a phone. */
static inline void pane_tabs(AppState *st, const Fit *fit, int filters) {
    RC_Style s = rcGetStyle();
    /* The query bar's height, less the track's 2 px a side, so a row of the two lines up. */
    float    height = fit->queryH - 4.0f;
    int      i;

    rcRow(.bg = s.surfaceAlt, .gap = 2, .p = 2, .borderRadius = "all-md",
          .w = fit->tabsInline ? "264px" : "grow") {
        for (i = 0; i < PANE_COUNT; i++) {
            const char *label = PANE_LABEL[i];

            if (i == PANE_FILTERS && filters > 0)
                label = rcFormat(rcAppArena(st->app), "Filters (%d)", filters).chars;
            if (segment(PANE_ID[i], label, st->pane == i, height))
                st->pane = i;
        }
    }
}

static inline void pane_body(AppState *st, RC_Arena *mem, const Fit *fit) {
    RC_Style s = rcGetStyle();

    if (st->pane == PANE_CHARTS) {
        /* Under a mouse rcScrollbar takes its own 12 px of the pane while it scrolls: the bar
           covers no tile, and the top list's own bar is not under it. A finger's thumb is a
           fading overlay, and there the bands run to the edge. */
        rcColumn(.id = "pane_charts", .bg = s.surface, .scroll = "v", .w = "grow", .h = "grow") {
            kpi_strip(st, mem, fit);
            viz_head(st, fit);
            viz_charts(st, mem, fit, true);
        }
        rcScrollbar("pane_charts");
    } else if (st->pane == PANE_ORDERS) {
        rcColumn(.bg = s.surface, .w = "grow", .h = "grow") {
            table_pane(st, mem, fit->main, fit);
        }
    } else {
        rcColumn(.id = "pane_filters", .bg = st->pal.rail, .scroll = "v", .w = "grow",
                 .h = "grow") {
            rcRow(.gap = 8, .px = 16, .py = 12, .align = "cl", .w = "grow") {
                rcTextL("Dates", .font = rcFontSemibold(13), .color = s.text);
                rcBox(.w = "grow") {
                    range_combo(st);
                }
                if (any_filter(st)) {
                    link_text("clear_all", "Clear all");
                    if (rcClicked("clear_all"))
                        clear_all(st);
                }
            }
            rule_h(s.border);
            facet_list(st, mem, fit->coarse);
        }
        rcScrollbar("pane_filters");
    }
}

/* --------------------------------------------------------------------- the frame */

static inline Fit fit_for(const AppState *st, RC_Insets safe) {
    RC_Viewport vp = rcViewport();
    float       vw = vp.width - safe.left - safe.right;
    float       vh = vp.height - safe.top - safe.bottom;
    Fit         f;

    f.coarse = rcPointerIsCoarse();
    f.desk   = vw >= EXP_DESK_MIN_W && vh >= EXP_DESK_MIN_H;
    f.rail   = f.desk ? (vw >= EXP_RAIL_ROOMY_W ? EXP_RAIL_W : EXP_RAIL_NARROW) : 0.0f;
    f.main   = vw - f.rail;
    /* The side panel only where the main column keeps a real table beside it. Whether it
       fits is decided with no order open, because a click opens one after this is read: the
       frame of the click then draws no modal for the panel that docks on the next. */
    f.docks  = f.desk && f.main - EXP_DRAWER_W >= EXP_DRAWER_MIN;
    f.drawer = f.docks && st->selected >= 0;
    if (f.drawer)
        f.main -= EXP_DRAWER_W;
    f.kpiRow = f.main >= EXP_KPI_ROW_W;
    f.low    = vh < EXP_LOW_H;
    f.tile   = f.kpiRow ? f.main / (float)M_COUNT : f.main / 2.0f;
    f.sideBySide = f.main >= EXP_CHARTS_SIDE;
    f.tabsInline = vw >= EXP_TABS_INLINE;
    f.rowH       = f.coarse ? (float)ROW_H_TOUCH : (float)ROW_H;
    f.queryH     = f.coarse ? (float)QUERY_H_TOUCH : (float)QUERY_H;
    return f;
}

/** The chips' count, for the Filters tab's label. */
static inline int filters_in_use(const AppState *st) {
    const Filters *f = &st->filter;
    int d, n = st->range < 0 ? 1 : 0;

    for (d = 0; d < DIM_COUNT; d++)
        n += f->pick[d] ? 1 : 0;
    n += f->minValue > 0.0f || f->maxValue < (float)VALUE_TOP ? 1 : 0;
    return n;
}

static inline void toast_view(AppState *st) {
    RC_Style s = rcGetStyle();

    if (rcAppTime(st->app) >= st->toastUntil)
        return;
    /* Only the earliest timer is kept: a toast shown while an older one's was armed wakes at
       that one, so re-arm until this one ends. */
    rcWindowRequestFrameAfter(rcAppMainWindow(st->app),
                              st->toastUntil - rcAppTime(st->app) + 0.05);
    rcRow(.bg = s.chrome, .px = 16, .py = 10, .borderRadius = "all-md",
          .shadow = { rcWithAlpha(RC_BLACK, 0.235f), 0.0f, 4.0f, 16.0f, 0.0f },
          .floating = { .to = RC_ATTACH_ROOT, .parent = RC_ANCHOR_BOTTOM_CENTER,
                        .element = RC_ANCHOR_BOTTOM_CENTER, .offset = { 0.0f, -24.0f },
                        /* The band the library keeps over a modal: a toast from inside one
                           has to stay readable above its scrim. */
                        .zIndex = INT16_MAX, .capture = RC_CAPTURE_PASSTHROUGH }) {
        rcTextC(st->toast, .color = st->pal.bandText, .size = 13);
    }
}

/** Step the selection through the rows, wherever the current sort put them. Only with an
    order open or the pointer on the table: keys reach the app whatever else is up, and
    with a dropdown open a Down must not open an order behind it. */
static inline void arrow_keys(AppState *st) {
    const View *v = &st->view;
    int step_by = rcKeyPressed(RC_KEY_DOWN) ? 1 : rcKeyPressed(RC_KEY_UP) ? -1 : 0;
    int rank;

    if (!step_by || !v->count || rcIsFocused("q") ||
        (st->selected < 0 && !rcIsHovered("orders")))
        return;
    rank = st->selected >= 0 ? selected_rank(v, st->selected) + step_by : 0;
    if (rank >= 0 && rank < v->count) {
        st->selected = v->sel[rank];
        st->reveal   = true;
    }
}

static inline void shortcuts(AppState *st) {
    if ((rcKeyPressed(RC_KEY_SLASH) && !rcIsFocused("q")) ||
        (rcKeyPressed(RC_KEY_K) && rcModDown(RC_MOD_PRIMARY)))
        rcSetFocus("q");
    if ((st->brushing || st->selected >= 0) && rcKeyPressed(RC_KEY_ESCAPE)) {
        if (st->brushing)
            st->brushing = false;
        else if (st->selected >= 0)
            st->selected = -1;
    }
    arrow_keys(st);
}

static inline void layout(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    int       chosenBefore;

    st->app = app;

    /* The frame's rule (the file header): the work a control asked for runs here, before
       anything is declared, so every panel below reads one consistent answer. */
    if (st->rescan) {
        view_rebuild(&st->view, &st->book, &st->filter, st->groupDim, (Metric)st->metric,
                     st->sortCol, st->sortDesc);
        if (st->selected >= 0 && selected_rank(&st->view, st->selected) < 0)
            st->selected = -1;
        st->toTop  = true;
        st->rescan = false;
    } else if (st->rederive) {
        view_derive(&st->view, (Metric)st->metric, st->groupDim);
    }
    st->rederive = false;
    if (st->resort) {
        view_sort(&st->view, &st->book, st->sortCol, st->sortDesc);
        st->toTop  = true;
        st->resort = false;
    }
    chosenBefore = st->selected;

    {
        RC_Arena *mem  = rcAppArena(app);
        RC_Style  s    = rcGetStyle();
        /* A phone draws under the status bar and the home indicator; these margins go once
           each, on the bands that touch those edges. All zero on a desktop. */
        RC_Insets safe = rcViewport().safe;
        Fit       fit  = fit_for(st, safe);

        poll_save(st);
        rcColumn(.id = "root", .bg = s.background, .w = "grow", .h = "grow") {
            top_band(st, &fit, safe, rcViewport().width - safe.left - safe.right);
            if (fit.desk) {
                query_band(st, mem, &fit, safe, 10);
                rcRow(.id = "body", .pl = safe.left, .pr = safe.right,
                      .border = { .color = s.border }, .w = "grow", .h = "grow",
                      .className = "border-t") {
                    facet_rail(st, mem, &fit);
                    desk_main(st, mem, &fit);
                    if (fit.drawer) {
                        rcColumn(.id = "drawer", .bg = s.surface, .p = 20, .scroll = "v",
                                 .border = { .color = s.border }, .h = "grow",
                                 .wType = RC_PX(EXP_DRAWER_W), .className = "border-l") {
                            order_body(st, mem, true);
                        }
                        rcScrollbar("drawer");
                    }
                }
            } else {
                if (fit.tabsInline) {
                    /* The band's own 16 px on the right is the gap to the tabs, as wide as the
                       margins; the right safe inset belongs past the tabs, at the edge. The
                       band's box is min-w-0 so its chips scroll instead of pushing the tabs out
                       (query_field). */
                    rcRow(.bg = s.surface, .pr = (float)(16.0f + safe.right), .align = "cl",
                          .w = "grow") {
                        rcBox(.w = "grow", .className = "min-w-0") {
                            query_band(st, mem, &fit, RC_LIT(RC_Insets){ .left = safe.left }, 10);
                        }
                        pane_tabs(st, &fit, filters_in_use(st));
                    }
                } else {
                    query_band(st, mem, &fit, safe, 8);
                    rcRow(.bg = s.surface, .pb = 10, .pl = (float)(16.0f + safe.left),
                          .pr = (float)(16.0f + safe.right), .w = "grow") {
                        pane_tabs(st, &fit, filters_in_use(st));
                    }
                }
                /* The pane's own color fills these margins, so under the home indicator
                   and beside a cutout it reaches the screen edge, as the bands above do. */
                rcColumn(.bg = st->pane == PANE_FILTERS ? st->pal.rail : s.surface,
                         .pb = safe.bottom, .pl = safe.left, .pr = safe.right,
                         .border = { .color = s.border }, .w = "grow", .h = "grow",
                         .className = "border-t") {
                    pane_body(st, mem, &fit);
                }
            }
            toast_view(st);
        }

        /* Where the side panel does not fit, the same body in a modal. Its open flag is
           the selection itself, so there is one state and Esc clears it through the bool. */
        if (st->selected >= 0 && !fit.docks) {
            bool open = true;

            if (rcBeginModal("order_modal", &open)) {
                float room = rcViewport().width - safe.left - safe.right - 64.0f;

                rcColumn(.wType = RC_PX(room < 380.0f ? room : 380.0f)) {
                    order_body(st, mem, false);
                }
                rcEndModal();
            }
            if (!open)
                st->selected = -1;
        }
        shortcuts(st);
    }

    /* A brush ends with its press. The chart commits it on the release, but a zoom or a pane
       change can take the chart away mid-drag, and then nothing else would end it: the request
       below would keep the window drawing for good, and the chart's return apply it. */
    if (st->brushing && !rcPointerDown(RC_POINTER_LEFT))
        st->brushing = false;
    if (st->rescan || st->rederive || st->resort || st->reveal || st->brushing ||
        st->selected != chosenBefore)
        rcWindowRequestFrame(rcAppMainWindow(app));
}

#endif /* APP_APP_H */
