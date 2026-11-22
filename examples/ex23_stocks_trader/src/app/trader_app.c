/*
    trader_app.c - the terminal's screen, drawn with nothing but the public API.

    THE CHART IS ONE CUSTOM-DRAW ELEMENT. rcChart has no candlestick series, and a
    candle per box would spend thousands of layout elements on what is one picture, so
    the grid, the candles, the volume, the indicators, the drawings and the crosshair
    all come from draw_plot, which rcCustomDraw hands the whole plot box. With the
    viewBox set to the box's width, one viewBox unit is one layout pixel at any zoom. Text
    cannot be drawn from a callback, so every label is an ordinary floating box, clipped
    and zoomed like the rest.

    THE PANELS ARE ORDINARY CONTROLS: rcBeginTable, each row tinted through rcTableRow's
    class; a heading row above it, which never scrolls and takes the sort clicks; and each
    column resolved to one pixel width each frame, from the width there is, which the
    heading row and the table both take. A table here never scrolls sideways: it sheds a
    column before a value can clip.

    EVERY NUMBER ON SCREEN IS TEXT THE STATE KEEPS (the backend's quotes, the plot's ticks,
    the ticket's figures) or text the frame formats into its own arena: AppState.frame, a
    fixed block inside the state that trader_layout resets as it starts. A label or a row id
    lives until its frame is drawn, and nothing is allocated.
*/
#define TRADER_BACKEND_IMPLEMENTATION
#include "trader_app.h"

#include "icons/rc_icons_arrow_down.h"
#include "icons/rc_icons_arrow_up.h"
#include "icons/rc_icons_chart_area.h"
#include "icons/rc_icons_chart_candlestick.h"
#include "icons/rc_icons_chart_line.h"
#include "icons/rc_icons_check.h"
#include "icons/rc_icons_crosshair.h"
#include "icons/rc_icons_eye.h"
#include "icons/rc_icons_eye_off.h"
#include "icons/rc_icons_expand.h"
#include "icons/rc_icons_list.h"
#include "icons/rc_icons_magnet.h"
#include "icons/rc_icons_minus.h"
#include "icons/rc_icons_plus.h"
#include "icons/rc_icons_rayclay_logo.h"
#include "icons/rc_icons_redo_2.h"
#include "icons/rc_icons_rotate_cw.h"
#include "icons/rc_icons_ruler.h"
#include "icons/rc_icons_search.h"
#include "icons/rc_icons_separator_horizontal.h"
#include "icons/rc_icons_settings.h"
#include "icons/rc_icons_shrink.h"
#include "icons/rc_icons_square_function.h"
#include "icons/rc_icons_trash_2.h"
#include "icons/rc_icons_trending_up.h"
#include "icons/rc_icons_undo_2.h"
#include "icons/rc_icons_wallet.h"
#include "icons/rc_icons_x.h"
#include "icons/rc_icons_zoom_in.h"
#include "icons/rc_icons_zoom_out.h"

/* The key RC_MOD_PRIMARY answers to, by the name this platform's users know it by. */
#if defined(__APPLE__)
#define PRIMARY_KEY "Cmd"
#else
#define PRIMARY_KEY "Ctrl"
#endif

/* Warmup frames before the scripted scenario holds still. */
#define TRADER_BENCH_WARMUP 64

#define TR_RAIL_W     44
#define TR_AXIS_W     66      /* the price scale, right of the plot                 */
#define TR_TIME_H     24      /* the time axis under the plot                       */
#define TR_RANGE_H    34      /* the range buttons under that                       */
#define TR_DEPTH_W    224     /* the book's column in the trading panel             */
#define TR_TABBAR_H   52      /* a phone's page bar                                 */
#define TR_RIGHT_PAD  8       /* empty bars right of the newest, room for the price */
#define TR_ROW_H      28      /* a table row on a mouse                             */
#define TR_FINGER_H   44      /* a table row, and any target, under a finger        */
#define TR_FIELD_H    32      /* the ticket's controls; their classes spell it h-8  */
#define TR_BAR_CTL    32      /* a top bar control on a mouse: rcBeginMenu's height */
#define TR_TAB_H      36      /* the trading panel's tab row                        */
#define TR_HEAD_H     24      /* a table's heading row                              */
#define TR_CELL_PL    10      /* a cell's left padding                              */
#define TR_CELL_PR    10      /* its right: the bar keeps a band of its own         */

/* Three shapes, chosen on the space there is. A window too short for a desk takes the
   phone's, whose pages scroll, and a landscape phone lands there too. */
typedef enum { TR_ARM_PHONE = 0, TR_ARM_COMPACT, TR_ARM_DESK } TrArm;

static TrArm tr_arm(const AppCtx *ctx) {
    const RC_Insets safe = app_safe(ctx);
    const float w = app_view_w(ctx) - safe.left - safe.right;
    const float h = app_view_h(ctx) - safe.top - safe.bottom;

    if (w >= 1120.0f && h >= 600.0f)
        return TR_ARM_DESK;
    if (w >= 760.0f && h >= 560.0f)
        return TR_ARM_COMPACT;
    return TR_ARM_PHONE;
}

/* The style, both themes, in one place. The dark set is near-black so the candles carry
   the screen, with one blue for what is selected and green and red only for direction. */
RC_Style trader_style(bool dark) {
    RC_Style s = dark ? rcStyleDark() : rcStyleLight();

    if (dark) {
        s.background   = rcHex(0x0a0b0e);     /* the chart ground      */
        s.surface      = rcHex(0x111318);     /* panels                */
        s.surfaceAlt   = rcHex(0x1c1f26);     /* hover, table headings */
        s.chrome       = rcHex(0x111318);
        s.text         = rcHex(0xe6e8ee);
        s.textMuted    = rcHex(0x8a909d);     /* 5.7:1 on a panel      */
        s.primary      = rcHex(0x2962ff);
        s.primaryHover = rcHex(0x4a7bff);
        s.success      = rcHex(0x0ecb81);
        s.successHover = rcHex(0x35d899);
        s.danger       = rcHex(0xf6465d);
        s.dangerHover  = rcHex(0xff6277);
        s.warning      = rcHex(0xf0b90b);
        s.border       = rcHex(0x262a33);
    } else {
        s.background   = rcHex(0xffffff);
        s.surface      = rcHex(0xffffff);
        s.surfaceAlt   = rcHex(0xf0f3fa);
        s.chrome       = rcHex(0xffffff);
        s.text         = rcHex(0x131722);
        s.textMuted    = rcHex(0x646872);
        s.primary      = rcHex(0x2962ff);
        s.primaryHover = rcHex(0x1e53e5);
        s.success      = rcHex(0x067a67);     /* 5.3:1 on white        */
        s.successHover = rcHex(0x056355);
        s.danger       = rcHex(0xd92d3a);
        s.dangerHover  = rcHex(0xb8232f);
        s.warning      = rcHex(0xb27b00);
        s.border       = rcHex(0xe0e3eb);
    }
    s.radius = 4.0f;
    return s;
}

/* The colors RC_Style has no field for, both themes. */
typedef struct {
    RC_Color accent;          /* the blue as text: the fill's own shade is too dark to read
                                 on near-black                                            */
    RC_Color onSide;          /* text on a side's solid fill, buy or sell: the dark theme's
                                 green and red are both too light for white               */
    RC_Color tag;             /* the crosshair's tags on the axes                         */
    RC_Color sma, ema, rsi;   /* the indicators' lines, one hue each, none green or red   */
} TrPalette;

static TrPalette trader_palette(bool dark) {
    TrPalette p;

    p.accent = dark ? rcHex(0x7aa2ff) : rcHex(0x2962ff);
    p.onSide = dark ? rcHex(0x0a0b0e) : RC_WHITE;
    p.tag    = rcHex(0x3a3f4b);
    p.sma    = rcHex(0xff9800);
    p.ema    = rcHex(0xe040fb);
    p.rsi    = rcHex(0xa78bfa);
    return p;
}

/* The palette in force. trader_layout installs it beside the style, and everything drawn
   reads it as it reads rcGetStyle(). */
static TrPalette pal;

static RC_Color tr_dir(int64_t v) {
    return v >= 0 ? rcGetStyle().success : rcGetStyle().danger;
}

/* A resting order's color on the chart: amber for a stop, its side for a limit. */
static RC_Color order_color(const TrWorking *o) {
    const RC_Style s = rcGetStyle();

    return o->type == TR_STOP ? s.warning : o->side == TR_BUY ? s.success : s.danger;
}

/* A plain run of text, one line, at a size. */
static void run(const char *text, int size, RC_Color color) {
    rcTextC(text, .color = color, .size = (uint16_t)size, .wrap = "n");
}

static void run_bold(const char *text, int size, RC_Color color) {
    rcTextC(text, .font = rcFontSemibold((float)size), .color = color, .wrap = "n");
}

/* A label on the chart. It takes no selection, so a drag across the plot always pans. */
static void chart_run(const char *text, int size, RC_Color color) {
    rcTextC(text, .color = color, .size = (uint16_t)size, .wrap = "n", .select = RC_SELECT_NONE);
}

static void chart_run_bold(const char *text, int size, RC_Color color) {
    rcTextC(text, .font = rcFontSemibold((float)size), .color = color, .wrap = "n",
            .select = RC_SELECT_NONE);
}

/* Stable ids: every row needs one of its own. A row's id comes from its data index, so a sort
   or a scroll never renames it; a resting order's comes from its tag, because its index moves
   as the orders before it fill, and so does a fill's, whose index moves as a full log drops
   its oldest. */
typedef enum {
    ID_WATCH = 0, ID_SPARK, ID_POS, ID_POS_CLOSE, ID_ORDER, ID_ORDER_X, ID_CHIP_X, ID_BID,
    ID_ASK, ID_FIND, ID_PHONE, ID_FILL
} TrIdKind;

/* row_id owns every id that is "r", a letter for the kind ('a' + kind) and a number: "rg0" is
   the x on order 0's chart chip. A literal id must not take that form, or two elements share
   one hover, press and click. The id is written into the frame's arena, which keeps it until
   the frame is drawn, its number by rcFormatIntTo, which calls no printf: every row of every
   frame asks for one. A full arena gives "", as rcFormat does. */
static const char *row_id(AppState *st, TrIdKind kind, int i) {
    char *id = (char *)rcArenaAlloc(&st->frame, 16);   /* "r", the kind, 11 digits at most, NUL */

    if (!id)
        return "";
    id[0] = 'r';
    id[1] = (char)('a' + (int)kind);
    rcFormatIntTo(id + 2, 14, i, "");
    return id;
}

/* A number in the backend's format, written into the frame's arena as rcFormat writes its
   text, so it lives until the frame is drawn. A full arena gives "", as rcFormat does. */
static const char *num_label(AppState *st, int (*format)(int64_t, char *, int), int64_t v) {
    char *out = (char *)rcArenaAlloc(&st->frame, 24);

    if (!out)
        return "";
    format(v, out, 24);
    return out;
}

/* A number field's kind picks a phone's keypad and filters nothing, so a desk's keyboard can
   type letters. What the field holds is cleaned after each frame's edit: digits, and with
   `decimal` one point. */
static void keep_number(char *s, bool decimal) {
    int k = 0;
    bool point = false;

    for (int i = 0; s[i]; i++) {
        if ((s[i] >= '0' && s[i] <= '9') || (decimal && s[i] == '.' && !point)) {
            point = point || s[i] == '.';
            s[k++] = s[i];
        }
    }
    s[k] = '\0';
}

/* The ticket's two fields, parsed by hand: integer cents, never a float's rounding. */
static int parse_qty(const char *s) {
    int v = 0;

    for (int i = 0; s[i] && i < 7; i++) {
        if (s[i] < '0' || s[i] > '9')
            return 0;
        v = v * 10 + (s[i] - '0');
    }
    return v;
}

static int32_t parse_cents(const char *s) {
    int64_t whole = 0;
    int i = 0, frac = 0;

    for (; s[i] && s[i] != '.'; i++) {
        if (s[i] < '0' || s[i] > '9')
            return 0;
        whole = whole * 10 + (s[i] - '0');
        /* Past what int32 cents hold: no price, never one wrapped into another that an order
           would rest or trigger at. */
        if (whole > (INT32_MAX - 99) / 100)
            return 0;
    }
    if (s[i] == '.') {
        if (s[i + 1] >= '0' && s[i + 1] <= '9')
            frac += (s[i + 1] - '0') * 10;
        if (s[i + 1] && s[i + 2] >= '0' && s[i + 2] <= '9')
            frac += s[i + 2] - '0';
    }
    return (int32_t)(whole * 100 + frac);
}

static char lower_ascii(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
}

/* Does `hay` hold `needle`, ignoring case? An empty needle matches everything. */
static bool contains_nocase(const char *hay, const char *needle) {
    if (!needle[0])
        return true;
    for (int i = 0; hay[i]; i++) {
        int j = 0;
        while (needle[j] && lower_ascii(hay[i + j]) == lower_ascii(needle[j]))
            j++;
        if (!needle[j])
            return true;
    }
    return false;
}

/* ---- small controls ------------------------------------------------------- */

/* The look every clickable thing here shares: a tint when selected, a step of surface on
   hover, a deeper one while held. The class reads the box's own hover and press, and its
   theme roles follow the installed style. Polling rcClicked gives the box the pointer cursor. */
static const char *control_class(bool active) {
    return active ? "bg-primary/22" : "hover:bg-surface-alt active:bg-foreground/16";
}

/* The label's ink. A text run has no hover of its own, so it asks the box's. */
static RC_Color control_ink(const char *id, bool active) {
    const RC_Style s = rcGetStyle();

    if (active)
        return pal.accent;
    return rcShowsHover(id) ? s.text : s.textMuted;
}

/* A word button, and a keyboard stop: Tab reaches it, and Enter or Space clicks it. */
static bool text_button(const char *id, const char *label, bool active, int h, const char *tip) {
    rcBox(.id = id, .focusable = true, .px = 8, .align = "cc", .borderRadius = "all-sm",
          .hType = RC_PX(h), .tooltip = tip, .className = control_class(active)) {
        run(label, 13, control_ink(id, active));
    }
    return rcClicked(id);
}

/* A glyph button, and a keyboard stop. .disabled is the library's: it fades the box, shows the
   not-allowed cursor, lets Tab pass it, rcClicked reads false, and its class takes no hover. */
static bool icon_button(const char *id, RC_IconCallback icon, bool active, bool enabled, int side,
                        const char *tip) {
    rcBox(.id = id, .focusable = true, .align = "cc", .borderRadius = "all-sm",
          .disabled = !enabled, .wType = RC_PX(side), .hType = RC_PX(side), .tooltip = tip,
          .className = control_class(active)) {
        icon(18.0f, control_ink(id, active));
    }
    return rcClicked(id);
}

static void vline(int h) {
    rcBox(.bg = rcGetStyle().border, .w = "1px", .hType = RC_PX(h)) {}
}

static void hline(void) {
    rcBox(.bg = rcGetStyle().border, .w = "grow", .h = "1px") {}
}

/* A menu row with a check when it is the current choice. */
static bool check_item(const char *label, bool on) {
    return rcMenuItem(label, .leadingIcon = on ? rcIconCheck : NULL);
}

/* The full-width button that commits an order, in its side's own color, a lighter shade under
   the pointer. It is a keyboard stop: Tab reaches it, and Enter or Space presses it. Disabled,
   the library fades it whole, Tab passes it and its class takes no hover. */
static bool side_button(const char *id, const char *label, bool buy, bool enabled, int h) {
    const char *look = buy ? "bg-success hover:bg-success-hover"
                           : "bg-danger hover:bg-danger-hover";

    rcBox(.id = id, .focusable = true, .align = "cc", .borderRadius = "all-sm",
          .disabled = !enabled, .w = "grow", .hType = RC_PX(h), .className = look) {
        run_bold(label, 14, pal.onSide);
    }
    return rcClicked(id);
}

/* ---- tables --------------------------------------------------------------- */

/* One column: its heading; its longest value, which with the heading sets the width it reads
   whole at; a floor for what no text measures; whether it takes a share of spare width; the
   order it gives way in when the table is narrow (0 never goes; higher goes first); its side. */
typedef struct {
    const char *head;
    const char *widest;
    int         minW;
    bool        grow;
    int         drop;
    bool        right;
} TrCol;

#define TR_MAX_COLS 8

static int column_floor(const TrCol *c) {
    const float head = rcTextWidth(0, 12, c->head, -1);
    const float body = rcTextWidth(0, 13, c->widest, -1);
    const int w = (int)(head > body ? head : body) + 1 + TR_CELL_PL + TR_CELL_PR;

    return w > c->minW ? w : c->minW;
}

/* WHICH COLUMNS FIT, AND HOW WIDE. A table sheds its least important columns until the rest read
   whole, then shares what is left among those that grow. It returns a bitmask and fills `px`
   with one pixel width each, the number the heading row and the table both take, so a heading
   always sits over its values. rcBeginTable's GROW would size its columns on its own. */
static unsigned fit_columns(const TrCol *cols, int n, float avail, int *px, int *hidden) {
    unsigned keep = (1u << n) - 1u;
    int need, growers;

    for (int i = 0; i < n; i++)
        px[i] = column_floor(&cols[i]);
    for (;;) {
        int worst = -1;

        need = 0;
        growers = 0;
        for (int i = 0; i < n; i++) {
            if (!(keep >> i & 1u))
                continue;
            need += px[i];
            growers += cols[i].grow;
            if (cols[i].drop > 0 && (worst < 0 || cols[i].drop > cols[worst].drop))
                worst = i;
        }
        if ((float)need <= avail || worst < 0)
            break;
        keep &= ~(1u << worst);
    }
    int spare = (int)avail - need;
    *hidden = 0;
    for (int i = 0; i < n; i++) {
        if (!(keep >> i & 1u)) {
            ++*hidden;
        } else if (cols[i].grow && spare > 0) {
            const int share = spare / growers--;
            px[i] += share;
            spare -= share;
        }
    }
    return keep;
}

/* The width a table's columns share. Under a mouse a scrolling body gives its bar a 12 px band
   of its own while its rows overflow, so the columns end where the bar begins. The answer is the
   last layout's, the one on screen: a frame whose band flips is laid out again. */
static float body_width(const char *id, float w) {
    const bool band = !rcPointerIsCoarse() && rcGetScrollInfo(id).maxOffsetY > 0.0f;

    return band ? w - 12.0f : w;
}

/* The tip a table shows when it sheds columns, so a reader can tell it from a full one. */
static void hidden_tip(const char *id, int hidden) {
    static const char *const LABEL[] = {
        "", "+1 column", "+2 columns", "+3 columns", "+4 columns", "+5 columns", "+6 columns",
    };
    const RC_Style s = rcGetStyle();

    if (hidden <= 0)
        return;
    rcBox(.id = id, .bg = s.surfaceAlt, .px = 8, .py = 2, .align = "cc", .borderRadius = "all-sm",
          .tooltip = "Widen the window to see every column") {
        run(LABEL[hidden > 6 ? 6 : hidden], 12, s.textMuted);
    }
}

/* The column list rcBeginTable takes, built from what fits. */
static int table_columns(int n, unsigned keep, const int *px, RC_TableColumn *out) {
    int k = 0;

    for (int i = 0; i < n; i++) {
        if (!(keep >> i & 1u))
            continue;
        RC_TableColumn c = { 0 };
        c.wType = RC_PX(px[i]);
        out[k++] = c;
    }
    return k;
}

/* The heading row, outside the table's scroller so it always shows. rcBeginTable's own header
   takes the cells' padding, which is zero here, so this draws it at the same widths. `sort`
   names the column whose values the rows are sorted by, or -1, and a heading given an id in
   `ids` is a sort button and a keyboard stop. The arrow stands on the far side of a
   right-aligned heading's text, which stays over its numbers. */
static void table_head(const TrCol *cols, int n, unsigned keep, const int *px,
                       const char *const *ids, int sort, bool down) {
    const RC_Style s = rcGetStyle();

    rcRow(.bg = s.surface, .align = "cl", .w = "grow", .hType = RC_PX(TR_HEAD_H)) {
        for (int i = 0; i < n; i++) {
            if (!(keep >> i & 1u))
                continue;
            const char *id = ids ? ids[i] : NULL;
            const bool hot = id && rcShowsHover(id);
            RC_IconCallback arrow = sort != i ? NULL : down ? rcIconArrowDown : rcIconArrowUp;

            rcRow(.id = id, .focusable = id != NULL, .gap = 4, .pl = TR_CELL_PL,
                  .pr = TR_CELL_PR, .align = "cl", .h = "grow", .wType = RC_PX(px[i])) {
                if (cols[i].right)
                    rcSpacer() {}
                if (arrow && cols[i].right)
                    arrow(12.0f, s.text);
                run(cols[i].head, 12, hot || sort == i ? s.text : s.textMuted);
                if (arrow && !cols[i].right)
                    arrow(12.0f, s.text);
            }
        }
    }
    hline();
}

/* One cell, padded as its heading is, so a value sits under its heading. */
static void cell(const char *text, int size, RC_Color color, bool right, int rowH) {
    rcRow(.pl = TR_CELL_PL, .pr = TR_CELL_PR, .align = "cl", .overflow = "hidden",
          .w = "grow", .hType = RC_PX(rowH)) {
        if (right)
            rcSpacer() {}
        rcTextC(text, .color = color, .size = (uint16_t)size, .wrap = "e");
    }
}

static void cell_bold(const char *text, RC_Color color, int rowH) {
    rcBox(.pl = TR_CELL_PL, .align = "cl", .w = "grow", .hType = RC_PX(rowH)) {
        run_bold(text, 13, color);
    }
}

/* The side of an order, as a word in a chip: the reading never rests on the color. */
static void cell_side(bool buy, int rowH) {
    const RC_Color c = buy ? rcGetStyle().success : rcGetStyle().danger;

    rcBox(.pl = TR_CELL_PL, .align = "cl", .w = "grow", .hType = RC_PX(rowH)) {
        rcBox(.bg = rcWithAlpha(c, 0.173f), .px = 6, .py = 1, .borderRadius = "all-xs") {
            run_bold(buy ? "BUY" : "SELL", 12, c);
        }
    }
}

/* A small action at a row's end, Close or Cancel: a ghost, so a column of them stays quiet
   beside the figures and takes its fill only under the pointer. */
static bool cell_action(const char *id, const char *label, int rowH) {
    bool hit = false;

    rcBox(.pr = TR_CELL_PR, .align = "cr", .w = "grow", .hType = RC_PX(rowH)) {
        hit = rcButton(id, label, .className = rowH >= TR_FINGER_H ? "h-11" : "h-[22px]",
                       .variant = RC_BTN_GHOST);
    }
    return hit;
}

/* A table whose cells bring their own padding, the heading row's, so the table's is zero. A
   row's tint is a bg-* in its class, which paints the whole row under its cells. */
static bool begin_table(const char *id, const RC_TableColumn *cols, int n) {
    return rcBeginTable(id, cols, n, .cellPadding = RC_VAL(0));
}

/* An empty table says what would fill it. */
static void table_empty(const char *text, const char *detail) {
    const RC_Style s = rcGetStyle();

    rcColumn(.gap = 4, .align = "cc", .w = "grow", .h = "grow") {
        run(text, 14, s.text);
        run(detail, 13, s.textMuted);
    }
}

/* ---- the chart: geometry ---------------------------------------------------- */

static float plot_x(const TrPlot *p, float bar) {
    return (bar + 0.5f - p->first) * p->barSpan;
}

static float plot_y(const TrPlot *p, int32_t price) {
    return p->priceTop + (float)(p->hi - price) * (p->priceBot - p->priceTop)
                       / (float)(p->hi - p->lo);
}

static int32_t plot_price(const TrPlot *p, float y) {
    const int32_t price = p->hi - (int32_t)((y - p->priceTop) * (float)(p->hi - p->lo)
                                            / (p->priceBot - p->priceTop));

    /* Under the lowest bar the scale runs on past zero, but a price is a cent or more: the
       measure divides by the one it starts at, and the menu offers an order at it. */
    return price < 1 ? 1 : price;
}

/* Is this price on the price pane, rather than above it or under the RSI? */
static bool in_price_pane(const TrPlot *p, int32_t price) {
    const float y = plot_y(p, price);

    return y >= 0.0f && y <= p->mainH;
}

/* The x of a moment, on the open interval's grid: a drawing keeps its candle. */
static float plot_x_time(const AppState *st, int64_t t) {
    const TrStore *s = &st->store;
    const int iv = tr_interval_seconds(s->interval);

    return plot_x(&st->plot, (float)(t / iv - s->firstBar));
}

/* A round step for about `target` gridlines: 1, 2 or 5 times a power of ten. */
static int32_t nice_step(int32_t range, int target) {
    int32_t raw = range / (target > 0 ? target : 1);
    int32_t mag = 1;

    if (raw < 1)
        raw = 1;
    while (mag <= raw / 10)
        mag *= 10;
    if (raw <= mag)
        return mag;
    if (raw <= 2 * mag)
        return 2 * mag;
    if (raw <= 5 * mag)
        return 5 * mag;
    return 10 * mag;
}

/* The price axis's ticks. With percent on, the round numbers are percents from the first
   bar on screen, so the scale reads like a return. */
static void plot_price_ticks(AppState *st) {
    TrPlot *p = &st->plot;
    const TrStore *s = &st->store;
    const int target = (int)(p->priceBot - p->priceTop) / 56;
    const int base = p->from <= p->to ? s->bars[p->from].open : 1;

    p->yTicks = 0;
    if (st->percentScale) {
        const int32_t bpsLo = (int32_t)((int64_t)(p->lo - base) * 10000 / base);
        const int32_t bpsHi = (int32_t)((int64_t)(p->hi - base) * 10000 / base);
        const int32_t step = nice_step(bpsHi - bpsLo, target);
        for (int32_t b = bpsHi / step * step; b >= bpsLo && p->yTicks < TR_MAX_YTICKS; b -= step) {
            p->yTickY[p->yTicks] = plot_y(p, base + (int32_t)((int64_t)base * b / 10000));
            tr_fmt_bps(b, p->yTickStr[p->yTicks], (int)sizeof p->yTickStr[0]);
            p->yTicks++;
        }
        return;
    }
    const int32_t step = nice_step(p->hi - p->lo, target);
    for (int32_t v = p->hi / step * step; v >= p->lo && p->yTicks < TR_MAX_YTICKS; v -= step) {
        p->yTickY[p->yTicks] = plot_y(p, v);
        tr_fmt_price(v, p->yTickStr[p->yTicks], (int)sizeof p->yTickStr[0]);
        p->yTicks++;
    }
}

/* The time axis's ticks: the finest round span of the calendar that leaves room for a
   label. A session opens at 09:30, so its first bar reads as its date and the ticks after it
   fall on round clock times. */
static void plot_time_ticks(AppState *st) {
    static const int32_t STEPS[] = {
        300, 900, 1800, 3600, TR_SESSION_SEC, 5 * TR_SESSION_SEC, 10 * TR_SESSION_SEC,
        20 * TR_SESSION_SEC, 60 * TR_SESSION_SEC, 130 * TR_SESSION_SEC, 260 * TR_SESSION_SEC,
    };
    TrPlot *p = &st->plot;
    const TrStore *s = &st->store;
    const int iv = tr_interval_seconds(s->interval);
    int32_t step = STEPS[10];

    for (size_t k = 0; k < sizeof STEPS / sizeof STEPS[0]; k++) {
        if (STEPS[k] >= iv && STEPS[k] % iv == 0 && (float)(STEPS[k] / iv) * p->barSpan >= 84.0f) {
            step = STEPS[k];
            break;
        }
    }
    p->xTicks = 0;
    for (int i = p->from; i <= p->to && p->xTicks < TR_MAX_XTICKS; i++) {
        const int64_t t = tr_bar_time(s, i);
        const bool opens = t % TR_SESSION_SEC == 0;
        const int64_t clock = step < TR_SESSION_SEC ? t % TR_SESSION_SEC + 34200 : t;
        const float x = plot_x(p, (float)i);

        if (clock % step && !(opens && step < TR_SESSION_SEC))
            continue;
        /* A session's open keeps its date, and a round time right after it gives way. */
        if (p->xTicks && x - p->xTickX[p->xTicks - 1] < 56.0f)
            continue;
        p->xTickX[p->xTicks] = x;
        p->xTickDay[p->xTicks] = opens;
        if (opens)
            tr_fmt_day(t, p->xTickStr[p->xTicks], (int)sizeof p->xTickStr[0]);
        else
            tr_fmt_time(t, p->xTickStr[p->xTicks], (int)sizeof p->xTickStr[0]);
        p->xTicks++;
    }
}

/* Keep the newest bar reachable and the history from running off the plot. */
static void clamp_view(AppState *st) {
    const TrPlot *p = &st->plot;
    const float most = (float)st->store.barCount - p->bars * 0.25f;
    const float least = -p->bars * 0.5f;

    if (st->viewEnd > most)
        st->viewEnd = most;
    if (st->viewEnd < least)
        st->viewEnd = least;
}

/* Where everything on the plot goes this frame. The plot's size is last frame's layout:
   exact while the window holds still, one frame late through a resize, and the library
   draws that next frame unasked. */
static void plot_compute(AppState *st, RC_Box box) {
    TrPlot *p = &st->plot;
    const TrStore *s = &st->store;

    p->w = box.width > 1.0f ? box.width : 800.0f;
    p->h = box.height > 1.0f ? box.height : 400.0f;
    p->barSpan = st->barPx;
    p->rsiH = 0.0f;
    if (st->show & TR_SHOW_RSI && p->h >= 240.0f) {
        p->rsiH = p->h * 0.2f;
        p->rsiH = p->rsiH < 56.0f ? 56.0f : p->rsiH > 120.0f ? 120.0f : p->rsiH;
    }
    p->mainH = p->h - (p->rsiH > 0.0f ? p->rsiH + 1.0f : 0.0f);
    p->rsiY = p->mainH + 1.0f;
    p->volH = st->show & TR_SHOW_VOLUME ? p->mainH * 0.16f : 0.0f;
    p->priceTop = 28.0f;
    p->priceBot = p->mainH - (p->volH > 0.0f ? p->volH * 0.7f : 12.0f);
    /* A window short enough leaves the band no height, or less than none, and plot_price
       divides by it: a pixel is the least a band of prices can be. */
    if (p->priceBot < p->priceTop + 1.0f)
        p->priceBot = p->priceTop + 1.0f;

    p->bars = p->w / p->barSpan;
    /* Here, where the view is read, so that no interval, resize or zoom can leave it past the
       bars there are. */
    clamp_view(st);
    p->first = (float)(s->barCount + TR_RIGHT_PAD) - st->viewEnd - p->bars;
    p->from = (int)p->first < 0 ? 0 : (int)p->first;
    p->to = (int)(p->first + p->bars);
    if (p->to > s->barCount - 1)
        p->to = s->barCount - 1;

    int32_t lo = 0, hi = 0;
    p->volMax = 1;
    for (int i = p->from; i <= p->to; i++) {
        const TrBar *b = &s->bars[i];
        if (!lo || b->low < lo)
            lo = b->low;
        if (b->high > hi)
            hi = b->high;
        if (b->volume > p->volMax)
            p->volMax = b->volume;
        if (st->show & TR_SHOW_BB && s->series[TR_SER_BB_UP][i] != TR_NONE) {
            if (s->series[TR_SER_BB_UP][i] > hi)
                hi = s->series[TR_SER_BB_UP][i];
            if (s->series[TR_SER_BB_LO][i] < lo)
                lo = s->series[TR_SER_BB_LO][i];
        }
    }
    if (!hi) {
        hi = tr_selected(s)->price + 100;
        lo = tr_selected(s)->price - 100;
    }
    if (hi - lo < 10)
        hi = lo + 10;
    const int32_t air = (hi - lo) / 30 + 1;
    p->lo = lo - air;
    p->hi = hi + air;

    plot_price_ticks(st);
    plot_time_ticks(st);
}

/* ---- the chart: drawing ----------------------------------------------------- */

/* Everything below draws in layout pixels: `vb` is the plot box's width, so one viewBox unit
   is one layout pixel; the zoom is applied downstream. */
typedef struct {
    RC_BoundingBox b;
    float vb;
} TrCanvas;

static void fill_rect(TrCanvas c, float x, float y, float w, float h, RC_Color color) {
    const RC_IconPoint pts[4] = { { x, y }, { x + w, y }, { x + w, y + h }, { x, y + h } };

    rcIconDrawFilledPolygon(c.b, pts, 4, c.vb, color);
}

static void dash_h(TrCanvas c, float x0, float x1, float y, float on, float off, RC_Color color) {
    for (float x = x0; x < x1; x += on + off)
        fill_rect(c, x, y, on, 1.0f, color);
}

static void dash_v(TrCanvas c, float x, float y0, float y1, RC_Color color) {
    for (float y = y0; y < y1; y += 8.0f)
        fill_rect(c, x, y, 1.0f, 4.0f, color);
}

static float pixel(float v) {
    return (float)(int)(v + 0.5f);
}

/* One indicator as one stroked path, from the first bar on screen that has a value. */
static void draw_series(const AppState *st, TrCanvas c, int ser, RC_Color color, bool rsi) {
    static RC_IconPoint pts[TR_MAX_BARS];
    const TrPlot *p = &st->plot;
    const int32_t *v = st->store.series[ser];
    int n = 0;

    for (int i = p->from; i <= p->to; i++) {
        if (v[i] == TR_NONE)
            continue;
        pts[n].x = plot_x(p, (float)i);
        pts[n].y = rsi ? p->rsiY + p->rsiH * (1.0f - (float)v[i] / 10000.0f) : plot_y(p, v[i]);
        n++;
    }
    if (n >= 2)
        rcIconDrawPolyline(c.b, pts, n, c.vb, 1.5f, false, color);
}

/* The band between two lines, one quad per bar: it can break where a bar has no value, and
   an outline past 256 points would be cut again on every draw. */
static void draw_band(const AppState *st, TrCanvas c, RC_Color color) {
    const TrPlot *p = &st->plot;
    const int32_t *up = st->store.series[TR_SER_BB_UP], *lo = st->store.series[TR_SER_BB_LO];

    for (int i = p->from; i < p->to; i++) {
        if (up[i] == TR_NONE)
            continue;
        const float x0 = plot_x(p, (float)i), x1 = plot_x(p, (float)i + 1.0f);
        const RC_IconPoint q[4] = {
            { x0, plot_y(p, up[i]) }, { x1, plot_y(p, up[i + 1]) },
            { x1, plot_y(p, lo[i + 1]) }, { x0, plot_y(p, lo[i]) },
        };
        rcIconDrawFilledPolygon(c.b, q, 4, c.vb, color);
    }
}

static void draw_bars(const AppState *st, TrCanvas c, const RC_Style *s) {
    const TrPlot *p = &st->plot;
    const TrStore *t = &st->store;
    int bodyW = (int)(p->barSpan * 0.72f);

    if (bodyW < 1)
        bodyW = 1;
    bodyW |= 1;     /* odd, so the wick sits on the body's center pixel */
    if (st->chartType != TR_CHART_CANDLES) {
        static RC_IconPoint pts[TR_MAX_BARS];
        int n = 0;
        for (int i = p->from; i <= p->to; i++) {
            pts[n].x = plot_x(p, (float)i);
            pts[n].y = plot_y(p, t->bars[i].close);
            if (st->chartType == TR_CHART_AREA && n) {
                const RC_IconPoint q[4] = {
                    pts[n - 1], pts[n], { pts[n].x, p->mainH }, { pts[n - 1].x, p->mainH },
                };
                rcIconDrawFilledPolygon(c.b, q, 4, c.vb, rcWithAlpha(s->text, 0.118f));
            }
            n++;
        }
        /* In the text color: the bands behind it are the blue. */
        if (n >= 2)
            rcIconDrawPolyline(c.b, pts, n, c.vb, 2.0f, false, s->text);
        return;
    }
    for (int i = p->from; i <= p->to; i++) {
        const TrBar *b = &t->bars[i];
        const bool up = b->close >= b->open;
        const RC_Color col = up ? s->success : s->danger;
        const float x = pixel(plot_x(p, (float)i) - 0.5f);
        const float top = pixel(plot_y(p, up ? b->close : b->open));
        const float bot = pixel(plot_y(p, up ? b->open : b->close));
        const float yH = pixel(plot_y(p, b->high)), yL = pixel(plot_y(p, b->low));

        fill_rect(c, x, yH, 1.0f, yL - yH, col);
        fill_rect(c, x - (float)(bodyW / 2), top, (float)bodyW,
                  bot - top < 1.0f ? 1.0f : bot - top, col);
    }
}

static void draw_volume(const AppState *st, TrCanvas c, const RC_Style *s) {
    const TrPlot *p = &st->plot;
    int bodyW = (int)(p->barSpan * 0.72f);

    if (bodyW < 1)
        bodyW = 1;
    bodyW |= 1;
    for (int i = p->from; i <= p->to; i++) {
        const TrBar *b = &st->store.bars[i];
        const float h = p->volH * (float)b->volume / (float)p->volMax;
        const float x = pixel(plot_x(p, (float)i) - 0.5f) - (float)(bodyW / 2);

        fill_rect(c, x, p->mainH - h, (float)bodyW, h,
                  rcWithAlpha(b->close >= b->open ? s->success : s->danger, 0.353f));
    }
}

/* A triangle under the bar of each buy and over the bar of each sell. */
static void draw_fills(const AppState *st, TrCanvas c, const RC_Style *s) {
    const TrPlot *p = &st->plot;
    const TrStore *t = &st->store;
    const int iv = tr_interval_seconds(t->interval);

    for (int k = 0; k < t->fillCount; k++) {
        const TrFill *f = &t->fills[k];
        const int bar = (int)(f->time / iv - t->firstBar);
        if (f->inst != t->selected || bar < p->from || bar > p->to)
            continue;
        const float x = plot_x(p, (float)bar);
        if (f->side == TR_BUY) {
            const float y = plot_y(p, t->bars[bar].low) + 6.0f;
            const RC_IconPoint tri[3] = {
                { x, y }, { x + 6.0f, y + 9.0f }, { x - 6.0f, y + 9.0f },
            };
            rcIconDrawFilledPolygon(c.b, tri, 3, c.vb, s->success);
        } else {
            const float y = plot_y(p, t->bars[bar].high) - 6.0f;
            const RC_IconPoint tri[3] = {
                { x, y }, { x - 6.0f, y - 9.0f }, { x + 6.0f, y - 9.0f },
            };
            rcIconDrawFilledPolygon(c.b, tri, 3, c.vb, s->danger);
        }
    }
}

/* What the user drew, the line being placed, and the measurement. */
static void draw_drawings(const AppState *st, TrCanvas c, const RC_Style *s) {
    const TrPlot *p = &st->plot;
    const int inst = st->store.selected;

    for (int k = 0; k < st->drawCount; k++) {
        const TrDrawing *d = &st->drawings[k];
        if (d->inst != inst || st->hideDrawings)
            continue;
        const float y0 = plot_y(p, d->p0);
        if (d->tool == TR_TOOL_HLINE) {
            fill_rect(c, 0.0f, pixel(y0), p->w, 1.0f, s->warning);
            continue;
        }
        const float x0 = plot_x_time(st, d->t0), x1 = plot_x_time(st, d->t1);
        const float y1 = plot_y(p, d->p1);
        rcIconDrawRoundLine(c.b, x0, y0, x1, y1, c.vb, 1.5f, s->warning);
        rcIconDrawFilledCircle(c.b, x0, y0, 3.0f, c.vb, s->warning);
        rcIconDrawFilledCircle(c.b, x1, y1, 3.0f, c.vb, s->warning);
    }
    if (!st->placing && !st->measuring)
        return;
    const float x0 = plot_x_time(st, st->placeT), y0 = plot_y(p, st->placeP);
    const float x1 = st->measuring ? plot_x_time(st, st->measureT1) : p->crossX;
    const float y1 = st->measuring ? plot_y(p, st->measureP1) : p->crossY;
    if (st->tool == TR_TOOL_MEASURE || st->measuring) {
        const int32_t end = st->measuring ? st->measureP1 : p->crossPrice;
        const bool rising = end >= st->placeP;
        const RC_Color tint = rising ? s->success : s->danger;
        const float lx = x0 < x1 ? x0 : x1, ty = y0 < y1 ? y0 : y1;
        const float wx = x0 < x1 ? x1 - x0 : x0 - x1, hy = y0 < y1 ? y1 - y0 : y0 - y1;
        fill_rect(c, lx, ty, wx, hy, rcWithAlpha(tint, 0.173f));
        fill_rect(c, x1 < x0 ? x1 : x0, y1, wx, 1.0f, tint);
        fill_rect(c, x1, ty, 1.0f, hy, tint);
    } else {
        rcIconDrawRoundLine(c.b, x0, y0, x1, y1, c.vb, 1.5f, s->warning);
        rcIconDrawFilledCircle(c.b, x0, y0, 3.0f, c.vb, s->warning);
    }
}

/* The newest close breathes: a ring grows out of it and fades, once every two seconds of
   market time, so a glance tells a live chart from a picture of one. */
static void draw_pulse(const AppState *st, TrCanvas c) {
    const TrPlot *p = &st->plot;
    const TrStore *t = &st->store;
    const int last = t->barCount - 1;
    const float phase = (float)(t->clock * 0.5 - (double)(int64_t)(t->clock * 0.5));
    const RC_Color col = tr_dir(t->bars[last].close - t->bars[last].open);
    const float x = plot_x(p, (float)last), y = plot_y(p, t->bars[last].close);

    if (last < p->from || last > p->to)
        return;
    rcIconDrawCircleStroke(c.b, x, y, 3.0f + phase * 11.0f, c.vb, 1.5f,
                           rcWithAlpha(col, (1.0f - phase) * 200.0f / 255.0f));
    rcIconDrawFilledCircle(c.b, x, y, 3.0f, c.vb, col);
}

/* THE WHOLE PLOT, back to front. It runs in the render pass, reading what plot_compute and
   the update left in the state, so it declares no UI and changes nothing. */
static void draw_plot(RC_BoundingBox b, RC_Color unused, const void *user) {
    const AppState *st = (const AppState *)user;
    const TrPlot *p = &st->plot;
    const TrStore *t = &st->store;
    const RC_Style s = rcGetStyle();
    const TrCanvas c = { b, b.width };   /* rcIconDraw* scale by bounds.width */
    const RC_Color grid = rcWithAlpha(s.border, st->darkMode ? 0.51f : 0.706f);
    const TrInstrument *it = tr_selected(t);

    (void)unused;
    for (int k = 0; k < p->yTicks; k++)
        fill_rect(c, 0.0f, pixel(p->yTickY[k]), p->w, 1.0f, grid);
    for (int k = 0; k < p->xTicks; k++)
        fill_rect(c, pixel(p->xTickX[k]), 0.0f, 1.0f, p->h, grid);
    if (st->show & TR_SHOW_VOLUME)
        draw_volume(st, c, &s);
    if (st->show & TR_SHOW_BB) {
        draw_band(st, c, rcWithAlpha(s.primary, 0.1f));
        draw_series(st, c, TR_SER_BB_UP, rcWithAlpha(s.primaryHover, 0.784f), false);
        draw_series(st, c, TR_SER_BB_LO, rcWithAlpha(s.primaryHover, 0.784f), false);
    }
    if (st->show & TR_SHOW_SMA)
        draw_series(st, c, TR_SER_SMA20, pal.sma, false);
    if (st->show & TR_SHOW_EMA)
        draw_series(st, c, TR_SER_EMA50, pal.ema, false);
    draw_bars(st, c, &s);
    draw_fills(st, c, &s);

    if (it->shares && in_price_pane(p, it->avgCost))
        dash_h(c, 0.0f, p->w, pixel(plot_y(p, it->avgCost)), 6.0f, 4.0f, pal.accent);
    for (int k = 0; k < t->workingCount; k++) {
        const TrWorking *o = &t->working[k];
        if (o->inst == t->selected && in_price_pane(p, o->price))
            dash_h(c, 0.0f, p->w, pixel(plot_y(p, o->price)), 6.0f, 4.0f, order_color(o));
    }
    if (in_price_pane(p, it->price))
        dash_h(c, 0.0f, p->w, pixel(plot_y(p, it->price)), 2.0f, 3.0f, tr_dir(it->change));
    draw_pulse(st, c);
    draw_drawings(st, c, &s);

    if (p->rsiH > 0.0f) {
        const float y70 = p->rsiY + p->rsiH * 0.3f, y30 = p->rsiY + p->rsiH * 0.7f;
        /* The pane's own ground, so a drawing that runs off the price pane stops at it. */
        fill_rect(c, 0.0f, p->mainH, p->w, p->h - p->mainH, s.background);
        fill_rect(c, 0.0f, p->mainH, p->w, 1.0f, s.border);
        fill_rect(c, 0.0f, y70, p->w, y30 - y70, rcWithAlpha(pal.rsi, 0.08f));
        dash_h(c, 0.0f, p->w, pixel(y70), 4.0f, 4.0f, rcWithAlpha(s.textMuted, 0.47f));
        dash_h(c, 0.0f, p->w, pixel(y30), 4.0f, 4.0f, rcWithAlpha(s.textMuted, 0.47f));
        draw_series(st, c, TR_SER_RSI, pal.rsi, true);
    }
    if (p->cross) {
        const RC_Color hair = rcWithAlpha(s.text, 0.59f);
        dash_v(c, pixel(p->crossX), 0.0f, p->h, hair);
        dash_h(c, 0.0f, p->w, pixel(p->crossY), 4.0f, 4.0f, hair);
    }
}

/* ---- the chart: input --------------------------------------------------------- */

/* Zoom around a point of the plot, so the bar under the pointer stays under it. */
static void zoom_at(AppState *st, float x, float factor) {
    const TrPlot *p = &st->plot;
    float px = st->barPx * factor;

    px = px < 2.0f ? 2.0f : px > 40.0f ? 40.0f : px;
    const float under = p->first + x / st->barPx;
    const float bars = p->w / px;
    st->viewEnd = (float)(st->store.barCount + TR_RIGHT_PAD) - bars - (under - x / px);
    st->barPx = px;
    st->range = -1;
    clamp_view(st);
}

static void reset_view(AppState *st) {
    st->barPx = 7.0f;
    st->viewEnd = 0.0f;
    st->range = -1;
}

/* Remove-all is one undo step, so a misclick on the trash never loses every line. */
static void clear_drawings(AppState *st) {
    st->clearedCount = st->drawCount;
    st->drawCount = 0;
    st->drawTop = 0;
}

/* A click on the plot, once the press has not turned into a drag. The tools take it. */
static void plot_click(AppState *st) {
    const TrPlot *p = &st->plot;
    const int64_t t = tr_bar_time(&st->store, p->crossBar);

    st->measuring = false;
    if (st->tool == TR_TOOL_CROSSHAIR || p->crossInRsi)
        return;
    if (st->tool == TR_TOOL_HLINE || st->placing) {
        if (st->tool == TR_TOOL_MEASURE) {
            st->measuring = true;
            st->measureT1 = t;
            st->measureP1 = p->crossPrice;
        } else if (st->drawCount < TR_MAX_DRAWINGS) {
            TrDrawing d = {
                st->tool, st->store.selected, st->placeT, t, st->placeP, p->crossPrice,
            };
            if (st->tool == TR_TOOL_HLINE)
                d.p0 = p->crossPrice;
            st->drawings[st->drawCount++] = d;
            st->drawTop = st->drawCount;
            st->clearedCount = 0;
        }
        st->placing = false;
        if (!st->measuring)
            st->tool = TR_TOOL_CROSSHAIR;
        return;
    }
    st->placing = true;
    st->placeT = t;
    st->placeP = p->crossPrice;
}

/* The pointer over the plot: the crosshair, the wheel, the drag and the click. It reads last
   frame's boxes, which are the ones the user is looking at. */
static void plot_input(AppState *st, RC_Box box, bool finger) {
    TrPlot *p = &st->plot;
    const TrStore *s = &st->store;
    const bool over = box.width > 1.0f && rcShowsHover("plot");
    /* Read only over the plot or in its drag: a frame that reads it draws every move. */
    const RC_Vec2 m = over || st->pressOnPlot ? rcPointer() : RC_LIT(RC_Vec2){ 0 };
    const float x = m.x - box.x, y = m.y - box.y;

    p->cross = false;
    /* A press that ended while no plot was drawn (a phone page, a resize to the phone's shape)
       is over: kept, it would pan the chart under the next drag anywhere. */
    if (!rcPointerDown(RC_POINTER_LEFT) && !rcPointerReleased(RC_POINTER_LEFT))
        st->pressOnPlot = false;
    if (over && s->barCount > 0) {
        int bar = (int)(p->first + x / p->barSpan);
        bar = bar < 0 ? 0 : bar >= s->barCount ? s->barCount - 1 : bar;
        p->cross = true;
        p->crossBar = bar;
        p->crossX = plot_x(p, (float)bar);
        p->crossY = y;
        p->crossInRsi = p->rsiH > 0.0f && y >= p->rsiY;
        p->crossPrice = plot_price(p, p->crossInRsi ? p->mainH : y);
        if (st->magnet && !p->crossInRsi) {
            const TrBar *b = &s->bars[bar];
            const int32_t v[4] = { b->open, b->high, b->low, b->close };
            int32_t best = v[0];
            for (int k = 1; k < 4; k++) {
                const int32_t da = v[k] - p->crossPrice, db = best - p->crossPrice;
                if ((da < 0 ? -da : da) < (db < 0 ? -db : db))
                    best = v[k];
            }
            p->crossPrice = best;
            p->crossY = plot_y(p, best);
        }
    }
    if (over && rcWheelDelta().y != 0.0f)
        zoom_at(st, x, rcWheelDelta().y < 0.0f ? 1.15f : 1.0f / 1.15f);
    if (over && rcPointerPressed(RC_POINTER_LEFT)) {
        st->pressOnPlot = true;
        st->pressMoved = false;
        st->pressX = x;
        st->pressEnd = st->viewEnd;
    }
    if (st->pressOnPlot && rcPointerDown(RC_POINTER_LEFT)) {
        const float dx = x - st->pressX;
        if (dx > 3.0f || dx < -3.0f)
            st->pressMoved = true;
        /* On a finger the drag is the crosshair, as on a broker's phone chart. A tool that
           is placing a line keeps the plot still too. */
        if (st->pressMoved && !finger && st->tool == TR_TOOL_CROSSHAIR) {
            st->viewEnd = st->pressEnd + dx / p->barSpan;
            st->range = -1;
            clamp_view(st);
            rcSetCursor(RC_CURSOR_GRABBING);
        }
    }
    if (st->pressOnPlot && rcPointerReleased(RC_POINTER_LEFT)) {
        st->pressOnPlot = false;
        if (p->cross && (!st->pressMoved || st->tool != TR_TOOL_CROSSHAIR))
            plot_click(st);
    }
}

/* Set the ticket up from a price the user pointed at: on the book, or on the chart. */
static void ticket_at_price(AppState *st, TrSide side, TrOrderType type, int32_t price) {
    st->orderSide = side;
    st->orderType = type;
    tr_fmt_price(price, st->priceText, (int)sizeof st->priceText);
}

/* ---- the chart: panes ---------------------------------------------------------- */

static const char *const IV_LABEL[TR_IV_COUNT] = { "1m", "5m", "15m", "30m", "1D", "1W" };
static const char *const IV_ID[TR_IV_COUNT] = { "iv0", "iv1", "iv2", "iv3", "iv4", "iv5" };

/* The name the legend gives the interval. */
static const char *const IV_NAME[TR_IV_COUNT] = { "1", "5", "15", "30", "1D", "1W" };

/* The ticket's live figures, worked out once a frame from what is typed. */
static void ticket_update(AppState *st) {
    const TrStore *t = &st->store;
    const TrSide side = (TrSide)st->orderSide;
    const TrOrderType type = (TrOrderType)st->orderType;

    st->ticketQty = parse_qty(st->qty);
    st->ticketPrice = type == TR_MARKET ? 0 : parse_cents(st->priceText);
    st->ticketEstimate = tr_estimate(t, side, type, st->ticketQty, st->ticketPrice);
    st->ticketProblem = tr_order_problem(t, side, type, st->ticketQty, st->ticketPrice);
    tr_fmt_price(type == TR_MARKET
                 ? (st->ticketQty ? st->ticketEstimate / st->ticketQty : t->asks[0].price)
                 : st->ticketPrice, st->avgStr, (int)sizeof st->avgStr);
    tr_fmt_cash(st->ticketEstimate, st->estimateStr, (int)sizeof st->estimateStr);
}

/* Open the review of the ticket as it stands, or send it now if confirmations are off. */
static void ticket_submit(AppState *st) {
    if (st->ticketProblem)
        return;
    st->closeInst = -1;
    if (st->confirmOrders) {
        st->modalConfirm = true;
        st->modalTicket = false;
    } else {
        tr_place_order(&st->store, (TrSide)st->orderSide, (TrOrderType)st->orderType, st->ticketQty,
                       st->ticketPrice);
        st->modalTicket = false;
    }
}

/* The chart's Buy and Sell: the ticket's quantity at the market, by the ticket's own path. The
   ticket has a line under its button for why it cannot go, and this pair has only the toast. */
static void quick_trade(AppState *st, TrSide side) {
    st->orderSide = side;
    st->orderType = TR_MARKET;
    ticket_update(st);
    if (st->ticketProblem)
        tr_notice(&st->store, st->ticketProblem);
    else
        ticket_submit(st);
}

/* The legend's OHLC line: the bar under the crosshair, or the one still forming. */
static void legend_ohlc(AppState *st, int bar, bool wide) {
    static const char *const KEY[4] = { "O", "H", "L", "C" };
    const RC_Style s = rcGetStyle();
    const TrStore *t = &st->store;
    const TrBar *b = &t->bars[bar];
    const int32_t vals[4] = { b->open, b->high, b->low, b->close };
    const int32_t prev = bar > 0 ? t->bars[bar - 1].close : b->open;
    const int32_t move = b->close - prev;
    const RC_Color dir = tr_dir(b->close - b->open);

    for (int i = 0; i < 4; i++) {
        rcRow(.gap = 3, .align = "cl") {
            chart_run(KEY[i], 13, s.textMuted);
            chart_run(num_label(st, tr_fmt_price, vals[i]), 13, dir);
        }
    }
    if (wide) {
        rcRow(.gap = 4, .align = "cl") {
            chart_run(num_label(st, tr_fmt_move, move), 13, dir);
            chart_run(num_label(st, tr_fmt_bps, tr_bps(move, prev)), 13, dir);
        }
    }
}

/* A legend row: an indicator's name and its value at the bar, and a click switches it off. The
   x that says so is shown only under the pointer, so it asks rcShowsHover. The row dims the
   plot behind it, as TradingView's legend backing does, so a candle or a line never runs
   through its figures. */
static bool legend_row(const char *id, const char *name, const char *v0, const char *v1,
                       RC_Color c) {
    const RC_Style s = rcGetStyle();

    rcRow(.id = id, .gap = 6, .px = 4, .align = "cl", .borderRadius = "all-xs", .h = "20px",
          .tooltip = "Click to remove", .className = "bg-background/70 hover:bg-surface-alt") {
        chart_run(name, 13, s.text);
        chart_run(v0, 13, c);
        if (v1)
            chart_run(v1, 13, c);
        if (rcShowsHover(id))
            rcIconX(13.0f, s.textMuted);
    }
    return rcClicked(id);
}

/* The rows for what is switched on. A bar too early for an indicator shows its name alone. */
static void legend_indicators(AppState *st, int bar) {
    const TrStore *t = &st->store;
    const int32_t *ser;

    if (st->show & TR_SHOW_VOLUME) {
        const char *vol = num_label(st, tr_fmt_volume, t->bars[bar].volume);
        if (legend_row("lg_vol", "Vol", vol, NULL, tr_dir(t->bars[bar].close - t->bars[bar].open)))
            st->show &= ~(unsigned)TR_SHOW_VOLUME;
    }
    if (st->show & TR_SHOW_BB) {
        const char *up = "", *lo = "";
        ser = t->series[TR_SER_BB_UP];
        if (ser[bar] != TR_NONE) {
            up = num_label(st, tr_fmt_price, ser[bar]);
            lo = num_label(st, tr_fmt_price, t->series[TR_SER_BB_LO][bar]);
        }
        if (legend_row("lg_bb", "BB 20 2", up, lo, rcGetStyle().primaryHover))
            st->show &= ~(unsigned)TR_SHOW_BB;
    }
    if (st->show & TR_SHOW_SMA) {
        const char *sma = "";
        ser = t->series[TR_SER_SMA20];
        if (ser[bar] != TR_NONE)
            sma = num_label(st, tr_fmt_price, ser[bar]);
        if (legend_row("lg_sma", "SMA 20", sma, NULL, pal.sma))
            st->show &= ~(unsigned)TR_SHOW_SMA;
    }
    if (st->show & TR_SHOW_EMA) {
        const char *ema = "";
        ser = t->series[TR_SER_EMA50];
        if (ser[bar] != TR_NONE)
            ema = num_label(st, tr_fmt_price, ser[bar]);
        if (legend_row("lg_ema", "EMA 50", ema, NULL, pal.ema))
            st->show &= ~(unsigned)TR_SHOW_EMA;
    }
}

/* The chart's head, TradingView's top-left block: what this is, and the numbers of the bar
   under the crosshair, written straight onto the plot as TradingView's are. The text passes
   the pointer through; the Buy and Sell quotes and the indicator rows take it. */
static void chart_legend(AppState *st, bool wide, bool rich) {
    const RC_Style s = rcGetStyle();
    const TrStore *t = &st->store;
    const TrInstrument *it = tr_selected(t);
    const int bar = st->plot.cross ? st->plot.crossBar : t->barCount - 1;
    const char *caption = rcFormat(&st->frame, "%s \xc2\xb7 %s \xc2\xb7 %s", it->name,
                                   IV_NAME[t->interval], it->exchange).chars;

    rcColumn(.id = "lg_head", .gap = 2, .px = 8, .py = 4,
             .floating = { .to = RC_ATTACH_PARENT, .capture = RC_CAPTURE_PASSTHROUGH,
                           .clip = RC_CLIP_TO_PARENT }) {
        rcRow(.gap = 12, .align = "cl") {
            chart_run_bold(wide ? caption : it->symbol, 14, s.text);
            rcBox(.bg = s.success, .borderRadius = "all-full", .w = "7px", .h = "7px") {}
            if (wide)
                legend_ohlc(st, bar, true);
        }
        if (!wide) {
            rcRow(.gap = 8, .align = "cl") {
                legend_ohlc(st, bar, false);
            }
        }
    }

    if (!rich)
        return;

    /* The quick trade pair: the inside quote either side of the spread, one click from a
       review of the ticket's quantity. */
    const char *sell = num_label(st, tr_fmt_price, t->bids[0].price);
    const char *buy = num_label(st, tr_fmt_price, t->asks[0].price);
    rcColumn(.id = "lg_rows", .gap = 2, .pb = 2, .pl = 8, .pr = 4,
             .floating = { .to = RC_ATTACH_PARENT, .offset = { 0.0f, wide ? 30.0f : 48.0f },
                           .clip = RC_CLIP_TO_PARENT }) {
        /* A tint at rest and the side's full color under the pointer; the words on it are
           text runs, which have no hover of their own, so they ask the box's. */
        rcRow(.gap = 4, .align = "cl") {
            bool hot = rcShowsHover("qt_sell");
            rcColumn(.id = "qt_sell", .px = 8, .py = 2, .align = "cl", .borderRadius = "all-xs",
                     .tooltip = "Sell at the bid: review the ticket's quantity",
                     .className = "bg-danger/16 hover:bg-danger") {
                chart_run("SELL", 11, hot ? pal.onSide : s.danger);
                chart_run_bold(sell, 13, hot ? pal.onSide : s.danger);
            }
            chart_run(t->spreadStr, 12, s.textMuted);
            hot = rcShowsHover("qt_buy");
            rcColumn(.id = "qt_buy", .px = 8, .py = 2, .align = "cl", .borderRadius = "all-xs",
                     .tooltip = "Buy at the ask: review the ticket's quantity",
                     .className = "bg-success/16 hover:bg-success") {
                chart_run("BUY", 11, hot ? pal.onSide : s.success);
                chart_run_bold(buy, 13, hot ? pal.onSide : s.success);
            }
        }
        const bool toSell = rcClicked("qt_sell"), toBuy = rcClicked("qt_buy");
        if (toSell || toBuy)
            quick_trade(st, toBuy ? TR_BUY : TR_SELL);
        legend_indicators(st, bar);
    }
}

/* A resting order on the chart: its line is drawn in draw_plot, and this chip on the line names
   it and cancels it. It stands back from the newest candles, which are what a trader is reading,
   and steps left of any chip already on its line. */
static void order_chips(AppState *st) {
    const RC_Style s = rcGetStyle();
    const TrPlot *p = &st->plot;
    TrStore *t = &st->store;
    float placed[TR_MAX_WORKING];
    int n = 0, cancel = -1;

    for (int k = 0; k < t->workingCount; k++) {
        const TrWorking *o = &t->working[k];
        if (o->inst != t->selected)
            continue;
        const float y = plot_y(p, o->price);
        if (!in_price_pane(p, o->price))
            continue;
        const RC_Color c = order_color(o);
        const char *id = row_id(st, ID_CHIP_X, o->tag);
        float x = p->w * 0.62f;

        for (int j = 0; j < n; j++)
            if (placed[j] - y > -24.0f && placed[j] - y < 24.0f)
                x -= 132.0f;
        placed[n++] = y;
        rcRow(.bg = s.surface, .gap = 4, .pl = 8, .pr = 2, .align = "cl", .borderRadius = "all-xs",
              .border = { .color = c, .width = 1 }, .h = "22px",
              .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                            .element = RC_ANCHOR_CENTER_RIGHT, .offset = { x, y },
                            .clip = RC_CLIP_TO_PARENT }) {
            chart_run(o->label, 12, c);
            rcBox(.id = id, .align = "cc", .borderRadius = "all-xs", .w = "18px", .h = "18px",
                  .tooltip = "Cancel this order", .className = "hover:bg-surface-alt") {
                rcIconX(12.0f, rcShowsHover(id) ? s.text : s.textMuted);
            }
        }
        if (rcClicked(id))
            cancel = k;
    }
    if (cancel >= 0)
        tr_cancel_order(t, cancel);
}

/* The x a chip on the line at height `y` starts from: the plot's edge, or past the legend while
   the two share a band. The legend's boxes are last frame's, as the plot's are. */
static float past_legend(RC_Box plot, float y) {
    static const char *const LEGEND[2] = { "lg_head", "lg_rows" };
    float x = 8.0f;

    for (int k = 0; k < 2; k++) {
        const RC_Box b = rcGetElementBox(LEGEND[k]);
        const float top = b.y - plot.y, bottom = top + b.height, right = b.x - plot.x + b.width;
        if (b.found && y + 11.0f > top && y - 11.0f < bottom && right + 8.0f > x)
            x = right + 8.0f;
    }
    return x;
}

/* The open position on the chart: its line at the average cost, and a chip with the P&L. */
static void position_chip(const AppState *st, RC_Box box) {
    const RC_Style s = rcGetStyle();
    const TrPlot *p = &st->plot;
    const TrInstrument *it = tr_selected(&st->store);

    if (!it->shares)
        return;
    const float y = plot_y(p, it->avgCost);
    if (!in_price_pane(p, it->avgCost))
        return;
    rcRow(.bg = s.surface, .gap = 8, .px = 8, .align = "cl", .borderRadius = "all-xs",
          .border = { .color = pal.accent, .width = 1 }, .h = "22px",
          .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                        .element = RC_ANCHOR_CENTER_LEFT, .offset = { past_legend(box, y), y },
                        .capture = RC_CAPTURE_PASSTHROUGH, .clip = RC_CLIP_TO_PARENT }) {
        chart_run(it->sharesStr, 12, pal.accent);
        chart_run(it->plStr, 12, tr_dir(it->pl));
    }
}

/* The measurement's reading, beside the box it measures. */
static void measure_label(AppState *st) {
    const TrPlot *p = &st->plot;
    const int64_t t1 = st->measuring ? st->measureT1 : tr_bar_time(&st->store, p->crossBar);
    const int32_t p1 = st->measuring ? st->measureP1 : p->crossPrice;
    const int iv = tr_interval_seconds(st->store.interval);
    const int64_t bars = (t1 > st->placeT ? t1 - st->placeT : st->placeT - t1) / iv;
    float x = plot_x_time(st, t1), y = plot_y(p, p1) - 8.0f;

    if (st->placing ? st->tool != TR_TOOL_MEASURE || !p->cross : !st->measuring)
        return;
    const char *move = num_label(st, tr_fmt_move, p1 - st->placeP);
    const char *pct = num_label(st, tr_fmt_bps, tr_bps(p1 - st->placeP, st->placeP));
    const char *span = rcFormat(&st->frame, "%d %s", (int)bars, bars == 1 ? "bar" : "bars").chars;
    /* It sits over the point it measures to, and stays whole at the plot's edges. */
    const float half = (rcTextWidth(0, 12, move, -1) + rcTextWidth(0, 12, pct, -1)
                        + rcTextWidth(0, 12, span, -1) + 36.0f) * 0.5f;
    x = x < half ? half : x > p->w - half ? p->w - half : x;
    y = y < 28.0f ? 28.0f : y;
    rcRow(.bg = tr_dir(p1 - st->placeP), .gap = 8, .px = 8, .py = 4, .align = "cl",
          .borderRadius = "all-sm",
          .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                        .element = RC_ANCHOR_BOTTOM_CENTER, .offset = { x, y },
                        .capture = RC_CAPTURE_PASSTHROUGH, .clip = RC_CLIP_TO_PARENT }) {
        chart_run_bold(move, 12, pal.onSide);
        chart_run(pct, 12, pal.onSide);
        chart_run(span, 12, pal.onSide);
    }
}

/* "Buy AAPL limit at 184.92", a line of the plot's context menu. */
static const char *order_label(AppState *st, const char *side, const char *symbol,
                               TrOrderType type, const char *price) {
    return rcFormat(&st->frame, "%s %s %s at %s", side, symbol, type == TR_LIMIT ? "limit" : "stop",
                    price).chars;
}

/* The plot itself: the custom draw under its labels, and the menu a right click opens. */
static void chart_plot(AppState *st, bool wide, bool finger) {
    const RC_Style s = rcGetStyle();
    const RC_Box box = rcGetElementBox("plot");
    const TrPlot *p = &st->plot;
    TrStore *t = &st->store;

    plot_compute(st, box);
    plot_input(st, box, finger);
    /* The wheel zooms the chart, so it is kept from every scroller around the plot. */
    rcBox(.id = "plot", .overflow = "hidden", .captureWheel = true, .w = "grow", .h = "grow") {
        rcCustomDraw(draw_plot, st, s.text);
        /* A phone has its big Buy and Sell, and its plot is too small to cover with rows. */
        chart_legend(st, wide, !finger && p->w >= 480.0f && p->h >= 280.0f);
        order_chips(st);
        position_chip(st, box);
        if (st->placing || st->measuring)
            measure_label(st);
        if (p->rsiH > 0.0f) {
            const int bar = p->cross ? p->crossBar : t->barCount - 1;
            const int32_t rsi = t->series[TR_SER_RSI][bar];
            const char *rsiStr = rsi != TR_NONE ? num_label(st, tr_fmt_price, rsi) : "";
            rcRow(.gap = 6, .px = 8, .py = 2, .align = "cl",
                  .floating = { .to = RC_ATTACH_PARENT, .offset = { 0.0f, p->rsiY },
                                .capture = RC_CAPTURE_PASSTHROUGH, .clip = RC_CLIP_TO_PARENT }) {
                chart_run("RSI 14", 13, s.text);
                chart_run(rsiStr, 13, pal.rsi);
            }
        }
    }
    if (rcBeginContextMenu("plot_menu", "plot")) {
        /* Take the price on the frame the menu opens: a right-click and a long press (a left
           press, the finger still down) both open it, and the crosshair is live on that frame. */
        if (!st->menuShown)
            st->menuPrice = p->crossPrice;
        st->menuShown = true;
        const TrInstrument *it = tr_selected(t);
        const char *price = num_label(st, tr_fmt_price, st->menuPrice);

        /* A price above the last takes a sell limit or a buy stop, and one below the reverse,
           so that neither item would fill the moment it is placed. */
        const bool above = st->menuPrice >= it->price;
        const TrOrderType buyType = above ? TR_STOP : TR_LIMIT;
        const TrOrderType sellType = above ? TR_LIMIT : TR_STOP;

        if (rcMenuItem(order_label(st, "Buy", it->symbol, buyType, price)))
            ticket_at_price(st, TR_BUY, buyType, st->menuPrice);
        if (rcMenuItem(order_label(st, "Sell", it->symbol, sellType, price)))
            ticket_at_price(st, TR_SELL, sellType, st->menuPrice);
        if (rcMenuItem("Reset chart view"))
            reset_view(st);
        if (rcMenuItem("Remove drawings", .disabled = st->drawCount == 0,
                       .className = "text-danger hover:bg-danger/10 "
                                    "disabled:text-muted-foreground"))
            clear_drawings(st);
        rcEndContextMenu();
    } else {
        st->menuShown = false;
    }
}

/* A price label on the scale, centered on its y. */
static void axis_label(const char *text, float y, RC_Color color) {
    rcBox(.pl = 8,
          .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                        .element = RC_ANCHOR_CENTER_LEFT, .offset = { 0.0f, y },
                        .capture = RC_CAPTURE_PASSTHROUGH, .clip = RC_CLIP_TO_PARENT }) {
        chart_run(text, 12, color);
    }
}

/* A filled tag on the scale: the last price, a position, an order, the crosshair. */
static void axis_tag(const char *text, float y, RC_Color bg, RC_Color ink) {
    rcBox(.bg = bg, .pl = 8, .align = "cl", .wType = RC_PX(TR_AXIS_W), .hType = RC_PX(20),
          .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                        .element = RC_ANCHOR_CENTER_LEFT, .offset = { 0.0f, y },
                        .capture = RC_CAPTURE_PASSTHROUGH, .clip = RC_CLIP_TO_PARENT }) {
        chart_run(text, 12, ink);
    }
}

/* A band of the scale that a tag has taken. */
typedef struct {
    float y, h;
} TrTagSlot;

/* Where a tag goes: on its price, unless a tag placed before it is there, and then just off that
   one on the side it came from. Tags are placed in the order they matter. */
static float place_tag(TrTagSlot *slots, int *n, float y, float h) {
    for (int pass = 0; pass <= *n; pass++) {
        bool moved = false;
        for (int k = 0; k < *n; k++) {
            const float clear = (slots[k].h + h) * 0.5f;
            const float dy = y - slots[k].y;
            if (dy > -clear && dy < clear) {
                y = slots[k].y + (dy >= 0.0f ? clear : -clear);
                moved = true;
            }
        }
        if (!moved)
            break;
    }
    slots[*n].y = y;
    slots[*n].h = h;
    ++*n;
    return y;
}

static bool under_tag(const TrTagSlot *slots, int n, float y) {
    for (int k = 0; k < n; k++) {
        const float clear = slots[k].h * 0.5f + 8.0f;
        if (y - slots[k].y > -clear && y - slots[k].y < clear)
            return true;
    }
    return false;
}

/* The price scale. THE LAST PRICE TAG IS THE SCREEN'S HEARTBEAT: it moves with every tick and
   counts down to the bar's close underneath. Three prices within a percent are normal on a long
   view, so the tags take their bands first, the last price before all, and a round number gives
   way to any tag on it. */
static void price_axis(AppState *st) {
    const RC_Style s = rcGetStyle();
    const TrPlot *p = &st->plot;
    const TrStore *t = &st->store;
    const TrInstrument *it = tr_selected(t);
    const int iv = tr_interval_seconds(t->interval);
    const int left = iv - (int)(t->now % iv);
    const bool up = it->change >= 0;
    TrTagSlot slots[3 + TR_MAX_WORKING + TR_MAX_DRAWINGS];
    float orderY[TR_MAX_WORKING], lineY[TR_MAX_DRAWINGS];
    float lastY = 0.0f, avgY = 0.0f;
    int taken = 0;
    const char *countdown = "";

    if (left < 3600)
        countdown = rcFormat(&st->frame, "%02d:%02d", left / 60, left % 60).chars;
    const bool showLast = in_price_pane(p, it->price);
    const bool showAvg = it->shares && in_price_pane(p, it->avgCost);

    if (showLast)
        lastY = place_tag(slots, &taken, plot_y(p, it->price), countdown[0] ? 36.0f : 20.0f);
    if (showAvg)
        avgY = place_tag(slots, &taken, plot_y(p, it->avgCost), 20.0f);
    for (int k = 0; k < t->workingCount; k++) {
        const TrWorking *o = &t->working[k];
        if (o->inst == t->selected && in_price_pane(p, o->price))
            orderY[k] = place_tag(slots, &taken, plot_y(p, o->price), 20.0f);
    }
    for (int k = 0; k < st->drawCount && !st->hideDrawings; k++) {
        const TrDrawing *d = &st->drawings[k];
        if (d->tool == TR_TOOL_HLINE && d->inst == t->selected && in_price_pane(p, d->p0))
            lineY[k] = place_tag(slots, &taken, plot_y(p, d->p0), 20.0f);
    }
    /* The crosshair's tag stays whole at the pane's ends and moves no other: it draws last, over
       what it passes. A plain label gives way to it, as to any tag. */
    const float crossY = p->crossY < 10.0f ? 10.0f
                       : p->crossY > p->h - 10.0f ? p->h - 10.0f : p->crossY;

    if (p->cross) {
        slots[taken].y = crossY;
        slots[taken].h = 20.0f;
        taken++;
    }

    rcBox(.bg = s.background, .overflow = "hidden", .h = "grow", .wType = RC_PX(TR_AXIS_W)) {
        for (int k = 0; k < p->yTicks; k++)
            if (p->yTickY[k] > 8.0f && p->yTickY[k] < p->mainH - 8.0f
                && !under_tag(slots, taken, p->yTickY[k]))
                axis_label(p->yTickStr[k], p->yTickY[k], s.textMuted);
        if (p->rsiH > 0.0f) {
            const float y70 = p->rsiY + p->rsiH * 0.3f, y30 = p->rsiY + p->rsiH * 0.7f;
            if (!under_tag(slots, taken, y70))
                axis_label("70", y70, s.textMuted);
            if (!under_tag(slots, taken, y30))
                axis_label("30", y30, s.textMuted);
        }
        if (showAvg)
            axis_tag(num_label(st, tr_fmt_price, it->avgCost), avgY, s.primary, RC_WHITE);
        for (int k = 0; k < t->workingCount; k++) {
            const TrWorking *o = &t->working[k];
            if (o->inst == t->selected && in_price_pane(p, o->price))
                axis_tag(o->priceStr, orderY[k], order_color(o),
                         o->type == TR_STOP ? RC_BLACK : pal.onSide);
        }
        for (int k = 0; k < st->drawCount && !st->hideDrawings; k++) {
            const TrDrawing *d = &st->drawings[k];
            if (d->tool == TR_TOOL_HLINE && d->inst == t->selected && in_price_pane(p, d->p0))
                axis_tag(num_label(st, tr_fmt_price, d->p0), lineY[k], s.warning, RC_BLACK);
        }
        if (showLast) {
            rcColumn(.bg = up ? s.success : s.danger, .pl = 8, .py = 2, .align = "cl",
                     .wType = RC_PX(TR_AXIS_W),
                     .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                                   .element = RC_ANCHOR_CENTER_LEFT,
                                   .offset = { 0.0f, lastY },
                                   .capture = RC_CAPTURE_PASSTHROUGH,
                                   .clip = RC_CLIP_TO_PARENT }) {
                chart_run_bold(it->priceStr, 12, pal.onSide);
                if (countdown[0])
                    chart_run(countdown, 11, pal.onSide);
            }
        }
        if (p->cross) {
            const char *cross;
            if (p->crossInRsi) {
                const float v = (p->crossY - p->rsiY) / p->rsiH;
                cross = num_label(st, tr_fmt_price, (int64_t)((1.0f - v) * 10000.0f));
            } else if (st->percentScale && p->from <= p->to) {
                const int32_t base = t->bars[p->from].open;
                cross = num_label(st, tr_fmt_bps, tr_bps(p->crossPrice - base, base));
            } else {
                cross = num_label(st, tr_fmt_price, p->crossPrice);
            }
            axis_tag(cross, crossY, pal.tag, RC_WHITE);
        }
    }
}

/* The time scale under the plot. The crosshair's date sits on it, over the ticks. */
static void time_axis(AppState *st) {
    const RC_Style s = rcGetStyle();
    const TrPlot *p = &st->plot;
    const TrStore *t = &st->store;

    rcRow(.bg = s.background, .w = "grow", .hType = RC_PX(TR_TIME_H)) {
        rcBox(.overflow = "hidden", .w = "grow", .h = "grow") {
            /* The date is centered on the crosshair unless that would cross an edge of the
               plot, and a tick that would peek out from under it gives way. */
            char *stamp = p->cross ? (char *)rcArenaAlloc(&st->frame, 28) : NULL;
            float tagX = 0.0f, tagW = 0.0f;

            if (stamp) {
                tr_fmt_stamp(tr_bar_time(t, p->crossBar), tr_interval_seconds(t->interval), stamp,
                             28);
                tagW = rcTextWidth(0, 12, stamp, -1) + 17.0f;
                tagX = p->crossX - tagW * 0.5f;
                tagX = tagX > p->w - tagW ? p->w - tagW : tagX;
                tagX = tagX < 0.0f ? 0.0f : tagX;
            }
            for (int k = 0; k < p->xTicks; k++) {
                const float x = p->xTickX[k];
                const float half = rcTextWidth(0, 12, p->xTickStr[k], -1) * 0.5f + 2.0f;

                if (x < half || x > p->w - half)
                    continue;
                if (stamp && x + half > tagX && x - half < tagX + tagW)
                    continue;
                rcBox(.floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_CENTER_LEFT,
                                    .element = RC_ANCHOR_CENTER, .offset = { x, 0.0f },
                                    .capture = RC_CAPTURE_PASSTHROUGH,
                                    .clip = RC_CLIP_TO_PARENT }) {
                    if (p->xTickDay[k])
                        chart_run_bold(p->xTickStr[k], 12, s.text);
                    else
                        chart_run(p->xTickStr[k], 12, s.textMuted);
                }
            }
            if (stamp) {
                rcBox(.bg = pal.tag, .px = 8, .align = "cc", .borderRadius = "all-xs",
                      .h = "22px",
                      .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_CENTER_LEFT,
                                    .element = RC_ANCHOR_CENTER_LEFT, .offset = { tagX, 0.0f },
                                    .capture = RC_CAPTURE_PASSTHROUGH,
                                    .clip = RC_CLIP_TO_PARENT }) {
                    chart_run(stamp, 12, RC_WHITE);
                }
            }
        }
        rcBox(.bg = s.border, .w = "1px", .h = "grow") {}
        rcBox(.align = "cc", .h = "grow", .wType = RC_PX(TR_AXIS_W - 1)) {
            const bool moved = st->viewEnd != 0.0f || st->range >= 0 || st->barPx != 7.0f;
            if (icon_button("view_reset", rcIconRotateCw, false, moved, 22, "Reset the chart view"))
                reset_view(st);
        }
    }
}

/* The range buttons under the chart: each sets an interval and a zoom that fit that span
   across the plot. */
static const struct {
    const char *label, *id;
    int interval, bars;
} TR_RANGES[] = {
    { "1D",  "range_1d",  TR_IV_5M,  78 },  { "5D",  "range_5d",  TR_IV_15M, 130 },
    { "1M",  "range_1m",  TR_IV_30M, 273 }, { "3M",  "range_3m",  TR_IV_1D,  63 },
    { "6M",  "range_6m",  TR_IV_1D,  126 }, { "YTD", "range_ytd", TR_IV_1D,  218 },
    { "1Y",  "range_1y",  TR_IV_1D,  252 }, { "All", "range_all", TR_IV_1W,  104 },
};

/* A new interval opens on its newest bars: a pan back means a different span on each grid. */
static void set_interval(AppState *st, int interval) {
    tr_set_interval(&st->store, interval);
    st->viewEnd = 0.0f;
    st->range = -1;
}

static void set_range(AppState *st, int r) {
    const float w = st->plot.w;

    tr_set_interval(&st->store, TR_RANGES[r].interval);
    st->barPx = w / (float)(TR_RANGES[r].bars + TR_RIGHT_PAD);
    st->barPx = st->barPx < 2.0f ? 2.0f : st->barPx;
    st->viewEnd = 0.0f;
    st->range = r;
}

static void range_bar(AppState *st, float paneW) {
    const RC_Style s = rcGetStyle();
    const TrStore *t = &st->store;

    rcRow(.bg = s.surface, .gap = 2, .px = 8, .align = "cl", .w = "grow",
          .hType = RC_PX(TR_RANGE_H)) {
        for (size_t r = 0; r < sizeof TR_RANGES / sizeof TR_RANGES[0]; r++)
            if (text_button(TR_RANGES[r].id, TR_RANGES[r].label, st->range == (int)r, 26, NULL))
                st->pickRange = (int)r;
        rcSpacer() {}
        if (paneW >= 480.0f) {
            /* The device's clock when main.c reads one, else the simulated market's. The market
               keeps its own calendar, so its axes may disagree with the device: the hover
               says so. */
            const RC_LocalTime *lt = &st->localNow;
            const char *dateText = t->dateStr, *timeText = t->clockStr, *zone = "New York";
            if (lt->year) {
                dateText = rcFormat(&st->frame, "%s %s %d", TR_DAYS[(lt->weekday + 6) % 7],
                                    TR_MONTHS[lt->month - 1], lt->day).chars;
                timeText = rcFormat(&st->frame, "%02d:%02d:%02d", lt->hour, lt->minute,
                                    lt->second).chars;
                zone = "local time";
            }
            /* The clock's words take a gap of their own: the buttons' 2 px would crowd them. */
            rcRow(.gap = 6, .pr = 6, .align = "cl",
                  .tooltip = lt->year ? "Your device's clock; the simulated market keeps its "
                                        "own dates and times" : NULL) {
                if (paneW >= 620.0f)
                    run(dateText, 13, s.textMuted);
                run(timeText, 13, s.text);
                run(zone, 12, s.textMuted);
            }
            vline(16);
        }
        if (text_button("scale_pct", "%", st->percentScale, 26, "Read the scale as percent"))
            st->percentScale = !st->percentScale;
    }
}

/* The chart pane, plot and scales, with the range buttons beneath. */
static void chart_pane(AppState *st, float paneW, bool finger, bool rangeRow) {
    const RC_Style s = rcGetStyle();

    rcColumn(.bg = s.background, .w = "grow", .h = "grow") {
        rcRow(.w = "grow", .h = "grow") {
            chart_plot(st, paneW >= 620.0f, finger);
            rcBox(.bg = s.border, .w = "1px", .h = "grow") {}
            price_axis(st);
        }
        hline();
        time_axis(st);
        if (rangeRow) {
            hline();
            range_bar(st, paneW);
        }
    }
}

/* ---- the watchlist ------------------------------------------------------------- */

enum { WC_SYM, WC_TREND, WC_LAST, WC_PCT, WC_COUNT };
static const TrCol WATCH_COLS[WC_COUNT] = {
    { "Symbol", "GOOGL",   0,  true,  0, false }, { "",     "",        40, true,  1, false },
    { "Last",   "1324.00", 0,  false, 0, true  }, { "Chg%", "-10.00%", 0,  false, 0, true  },
};
static const char *const WATCH_HEAD_IDS[WC_COUNT] = { "wh0", NULL, "wh2", "wh3" };

/* The rows the watchlist shows, in the order it shows them. */
static int watch_order(const AppState *st, int *order) {
    const TrStore *t = &st->store;
    int n = 0;

    for (int i = 0; i < t->count; i++) {
        const TrInstrument *it = &t->inst[i];
        if ((st->watchFilter == 1 && it->changeBps <= 0) ||
            (st->watchFilter == 2 && it->changeBps >= 0) ||
            (st->watchFilter == 3 && !it->shares))
            continue;
        order[n++] = i;
    }
    if (st->sortColumn < 0)
        return n;
    /* An insertion sort: two dozen rows, already nearly sorted from the last frame's quotes. */
    for (int a = 1; a < n; a++) {
        const int v = order[a];
        int b = a;
        for (; b > 0; b--) {
            const TrInstrument *x = &t->inst[order[b - 1]], *y = &t->inst[v];
            int64_t cmp = st->sortColumn == WC_LAST ? (int64_t)x->price - y->price
                        : st->sortColumn == WC_PCT ? (int64_t)x->changeBps - y->changeBps
                        : (int64_t)tr_symbol_order(x, y);
            if (st->sortDown)
                cmp = -cmp;
            if (cmp <= 0)
                break;
            order[b] = order[b - 1];
        }
        order[b] = v;
    }
    return n;
}

/* A trend from the instrument's own session, in its direction's color. An area keeps 0 in an
   auto-fit range, so the range is pinned to the values, with a band under the lowest so the
   line never runs along the box's floor. */
static void trend_band(const char *id, const float *values, int count, RC_Color color) {
    float lo = values[0], hi = lo;

    for (int k = 1; k < count; k++) {
        if (values[k] < lo)
            lo = values[k];
        if (values[k] > hi)
            hi = values[k];
    }
    const float span = hi > lo ? hi - lo : 1.0f;
    rcSparkline(id, values, count, .kind = RC_SERIES_AREA, .color = color,
                .min = RC_VAL(lo - span * 0.25f), .max = RC_VAL(hi + span * 0.08f),
                .thickness = 1.25f);
}

static void trend(AppState *st, int i, const TrInstrument *it, RC_Color color) {
    trend_band(row_id(st, ID_SPARK, i), it->spark, TR_SPARK_N, color);
}

/* The right column's top: the board of symbols. Every row is a way in: it opens the chart,
   the ticket and the book on that symbol. A quote that just ticked flashes its direction. */
static void watchlist(AppState *st, float paneW, int rowH) {
    static const char *const FILTERS[] = { "All", "Gainers", "Losers", "Holdings" };
    const RC_Style s = rcGetStyle();
    TrStore *t = &st->store;
    int order[TR_MAX_INSTRUMENTS];
    const int n = watch_order(st, order);
    int px[WC_COUNT], hidden = 0;
    const unsigned keep = fit_columns(WATCH_COLS, WC_COUNT, body_width("watch_table", paneW), px,
                                      &hidden);
    RC_TableColumn cols[WC_COUNT];
    const int ncols = table_columns(WC_COUNT, keep, px, cols);
    int picked = -1;

    rcRow(.gap = 8, .px = 12, .align = "cl", .w = "grow", .h = "36px") {
        run_bold("Watchlist", 14, s.text);
        run(rcFormat(&st->frame, "%d", n).chars, 12, s.textMuted);
        rcSpacer() {}
        hidden_tip("wl_hidden", hidden);
        if (rcBeginMenu("wl_filter", FILTERS[st->watchFilter], .caret = true)) {
            for (int f = 0; f < 4; f++)
                if (check_item(FILTERS[f], st->watchFilter == f))
                    st->watchFilter = f;
            rcEndMenu();
        }
    }
    table_head(WATCH_COLS, WC_COUNT, keep, px, WATCH_HEAD_IDS, st->sortColumn, st->sortDown);
    for (int c = 0; c < WC_COUNT; c++) {
        if (keep >> c & 1u && WATCH_HEAD_IDS[c] && rcClicked(WATCH_HEAD_IDS[c])) {
            if (st->sortColumn != c) {
                st->sortColumn = c;
                st->sortDown = c != WC_SYM;
            } else if (st->sortDown == (c != WC_SYM)) {
                st->sortDown = !st->sortDown;
            } else {
                st->sortColumn = -1;
            }
        }
    }
    rcBox(.w = "grow", .h = "grow") {
        if (n == 0) {
            table_empty("Nothing to show", "No symbol passes this filter");
        } else if (begin_table("watch_table", cols, ncols)) {
            for (int r = 0; r < n; r++) {
                const int i = order[r];
                const TrInstrument *it = &t->inst[i];
                const char *id = row_id(st, ID_WATCH, i);
                const bool sel = i == t->selected;
                const RC_Color dir = tr_dir(it->changeBps);

                rcTableRow(.id = id, .className = sel ? "bg-primary/14" : "hover:bg-surface-alt");
                rcRow(.align = "cl", .w = "grow", .hType = RC_PX(rowH)) {
                    rcBox(.bg = sel ? s.primary : RC_UNSET_COLOR, .w = "2px", .h = "grow") {}
                    rcBox(.pl = 8) {
                        run_bold(it->symbol, 13, s.text);
                    }
                }
                if (keep >> WC_TREND & 1u) {
                    rcTableNext();
                    rcBox(.px = 4, .py = 7, .w = "grow", .hType = RC_PX(rowH)) {
                        trend(st, i, it, dir);
                    }
                }
                rcTableNext();
                {
                    const float f = it->flash < 0.0f ? -it->flash : it->flash;
                    const RC_Color lit = rcWithAlpha(it->flash > 0.0f ? s.success : s.danger,
                                                 f * 90.0f / 255.0f);
                    /* The flash's own 4 px stands in the cell's padding: the digits stay put. */
                    rcBox(.pr = TR_CELL_PR - 4, .align = "cr", .w = "grow",
                          .hType = RC_PX(rowH)) {
                        rcBox(.bg = f > 0.02f ? lit : RC_UNSET_COLOR, .px = 4, .align = "cr",
                              .borderRadius = "all-xs", .h = "20px") {
                            run(it->priceStr, 13, s.text);
                        }
                    }
                }
                rcTableNext();
                rcBox(.pr = TR_CELL_PR, .align = "cr", .w = "grow", .hType = RC_PX(rowH)) {
                    run(it->changePctStr, 13, dir);
                }
                if (rcClicked(id))
                    picked = i;
            }
            rcEndTable();
        }
    }
    if (n > 0)
        rcScrollbar("watch_table");
    if (picked >= 0) {
        st->pickSymbol = picked;
        st->priceText[0] = '\0';
        st->page = TR_PAGE_CHART;
    }
}

/* ---- the order ticket ----------------------------------------------------------- */

static const char *const ORDER_TYPES[] = { "Market", "Limit", "Stop" };

/* A Buy or Sell segment, and a keyboard stop: Tab reaches it, and Enter or Space chooses it.
   The chosen side fills with its own color, so the ticket says which way it is about to trade
   before anything else is read; the other takes a tint of its color under the pointer. */
static bool side_segment(const char *id, const char *label, bool active, bool buy, int h) {
    const RC_Style s = rcGetStyle();
    const char *look = active ? (buy ? "bg-success" : "bg-danger")
                     : buy ? "hover:bg-success/16" : "hover:bg-danger/16";

    rcBox(.id = id, .focusable = true, .align = "cc", .borderRadius = "all-sm", .w = "grow",
          .hType = RC_PX(h), .className = look) {
        run_bold(label, 14, active ? pal.onSide : s.textMuted);
    }
    return rcClicked(id);
}

/* A ticket row: a label flush left and whatever it labels flush right, one baseline. */
static void ticket_value(const char *label, const char *value, RC_Color color, bool bold) {
    const RC_Style s = rcGetStyle();

    rcRow(.align = "cl", .w = "grow", .h = "22px") {
        run(label, 13, s.textMuted);
        rcSpacer() {}
        if (bold)
            run_bold(value, 14, color);
        else
            run(value, 13, color);
    }
}

static void qty_step(AppState *st, int delta) {
    const int v = st->ticketQty + delta;

    /* The field holds seven digits, and a step past them would keep only the low seven. */
    tr_fmt_uint((uint64_t)(v < 1 ? 1 : v > 9999999 ? 9999999 : v), st->qty, (int)sizeof st->qty);
}

/* The ticket, Robinhood's: a side, a type, how many, what it comes to, and one big button.
   Everything it says is live, down to why the button is off. */
static void order_ticket(AppState *st, bool finger) {
    const RC_Style s = rcGetStyle();
    const TrInstrument *it = tr_selected(&st->store);
    const bool buy = st->orderSide == TR_BUY;
    const int h = finger ? TR_FINGER_H : TR_FIELD_H;
    /* h, in the class form the fields, the steppers and the type menu take: h-11 is
       TR_FINGER_H, h-8 TR_FIELD_H. */
    const char *fieldClass = finger ? "h-11" : "h-8";

    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        run_bold(it->symbol, 16, s.text);
        run(it->priceStr, 14, tr_dir(it->change));
        rcSpacer() {}
        run(it->changePctStr, 13, tr_dir(it->change));
    }
    rcRow(.bg = s.background, .gap = 2, .p = 2, .borderRadius = "all-md", .w = "grow") {
        if (side_segment("seg_buy", "Buy", buy, true, h - 4))
            st->orderSide = TR_BUY;
        if (side_segment("seg_sell", "Sell", !buy, false, h - 4))
            st->orderSide = TR_SELL;
    }
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        run("Order type", 13, s.textMuted);
        rcSpacer() {}
        rcBox(.wType = RC_PX(128)) {
            if (rcCombo("order_type", &st->orderType, ORDER_TYPES, 3, .className = fieldClass)
                && st->orderType != TR_MARKET && !st->priceText[0])
                tr_fmt_price(it->price, st->priceText, (int)sizeof st->priceText);
        }
    }
    rcRow(.gap = 4, .align = "cl", .w = "grow") {
        run("Shares", 13, s.textMuted);
        rcSpacer() {}
        if (rcButton("qty_less", "", .disabled = st->ticketQty <= 1, .className = fieldClass,
                     .leadingIcon = rcIconMinus))
            qty_step(st, -1);
        rcBox(.wType = RC_PX(96)) {
            rcTextInput("qty_in", st->qty, (int)sizeof st->qty, .className = fieldClass,
                        .placeholder = "0", .inputKind = RC_INPUT_NUMBER, .clearButton = true);
        }
        keep_number(st->qty, false);
        if (rcButton("qty_more", "", .className = fieldClass,
                     .leadingIcon = rcIconPlus))
            qty_step(st, 1);
    }
    if (st->orderType != TR_MARKET) {
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            run(st->orderType == TR_LIMIT ? "Limit price" : "Stop price", 13, s.textMuted);
            rcSpacer() {}
            rcBox(.wType = RC_PX(128)) {
                rcTextInput("price_in", st->priceText, (int)sizeof st->priceText,
                            .className = fieldClass, .placeholder = "0.00",
                            .inputKind = RC_INPUT_DECIMAL, .clearButton = true);
            }
            keep_number(st->priceText, true);
        }
    }
    hline();
    rcColumn(.w = "grow") {
        ticket_value(st->orderType == TR_MARKET ? "Est. price" : "Price", st->avgStr, s.text,
                     false);
        ticket_value(buy ? "Estimated cost" : "Estimated credit", st->estimateStr, s.text, true);
    }
    const char *title = rcFormat(&st->frame, "Review %s %s", buy ? "buy" : "sell",
                                 it->symbol).chars;
    if (side_button("btn_review", title, buy, !st->ticketProblem, finger ? 48 : 40))
        ticket_submit(st);
    /* The press that sends a review belongs to the review, not to the field behind it. */
    if (!st->modalConfirm && (rcKeyPressedOn("qty_in", RC_KEY_ENTER)
                              || rcKeyPressedOn("price_in", RC_KEY_ENTER)))
        ticket_submit(st);
    rcRow(.gap = 4, .align = "cc", .w = "grow", .h = "18px") {
        if (st->ticketProblem) {
            run(st->ticketProblem, 12, s.danger);
        } else {
            run(st->store.cashStr, 12, s.text);
            run("buying power available", 12, s.textMuted);
        }
    }
}

/* ---- the order book ---------------------------------------------------------------- */

/* One ladder row: a depth bar growing from the right behind the price and the size. A click
   puts the price on the ticket, on the side that would rest there. */
static void ladder_row(AppState *st, const char *id, const TrLevel *lv, bool bid, int maxSize,
                       int h) {
    const RC_Style s = rcGetStyle();
    const RC_Color col = bid ? s.success : s.danger;
    int pct = (int)((int64_t)lv->size * 100 / maxSize);

    pct = pct < 2 ? 2 : pct;
    rcRow(.id = id, .focusable = true, .align = "cl", .w = "grow", .hType = RC_PX(h),
          .tooltip = bid ? "Buy limit at this price" : "Sell limit at this price",
          .className = "hover:bg-surface-alt") {
        rcSpacer() {}
        rcBox(.bg = rcWithAlpha(col, 0.173f), .h = "grow", .wType = RC_PCT(pct)) {}
        rcRow(.px = 8, .align = "cl", .w = "grow", .h = "grow",
              .floating = { .to = RC_ATTACH_PARENT, .capture = RC_CAPTURE_PASSTHROUGH,
                            .clip = RC_CLIP_TO_PARENT }) {
            run(lv->priceStr, 12, col);
            rcSpacer() {}
            run(lv->sizeStr, 12, s.text);
        }
    }
    if (rcClicked(id))
        ticket_at_price(st, bid ? TR_BUY : TR_SELL, TR_LIMIT, lv->price);
}

/* The depth ladder: asks over bids, the best of each meeting at the last price. In a wide, short
   tab the two sides stand next to each other, bids on the left as a book's columns read, which
   shows twice the levels in the same height. */
static void depth_ladder(AppState *st, int availH, bool sideBySide) {
    const RC_Style s = rcGetStyle();
    const TrStore *t = &st->store;
    const TrInstrument *it = tr_selected(t);
    int levels = (availH - 34 - 26) / (sideBySide ? 1 : 2) / 17;
    int maxSize = 1;

    levels = levels < 1 ? 1 : levels > TR_BOOK_LEVELS ? TR_BOOK_LEVELS : levels;
    for (int l = 0; l < levels; l++) {
        if (t->asks[l].size > maxSize)
            maxSize = t->asks[l].size;
        if (t->bids[l].size > maxSize)
            maxSize = t->bids[l].size;
    }
    rcRow(.gap = 8, .px = 12, .align = "cl", .w = "grow", .h = "34px") {
        run_bold("Order book", 13, s.text);
        rcSpacer() {}
        run("Spread", 12, s.textMuted);
        run(t->spreadStr, 12, s.text);
    }
    if (!sideBySide) {
        for (int l = levels - 1; l >= 0; l--)
            ladder_row(st, row_id(st, ID_ASK, l), &t->asks[l], false, maxSize, 17);
    }
    rcRow(.bg = s.surfaceAlt, .gap = 8, .px = 8, .align = "cl", .w = "grow", .h = "26px") {
        run_bold(it->priceStr, 14, tr_dir(it->change));
        rcSpacer() {}
        run(it->changeStr, 12, tr_dir(it->change));
    }
    if (sideBySide) {
        rcRow(.gap = 1, .w = "grow") {
            rcColumn(.w = "grow") {
                for (int l = 0; l < levels; l++)
                    ladder_row(st, row_id(st, ID_BID, l), &t->bids[l], true, maxSize, 17);
            }
            rcColumn(.w = "grow") {
                for (int l = 0; l < levels; l++)
                    ladder_row(st, row_id(st, ID_ASK, l), &t->asks[l], false, maxSize, 17);
            }
        }
    } else {
        for (int l = 0; l < levels; l++)
            ladder_row(st, row_id(st, ID_BID, l), &t->bids[l], true, maxSize, 17);
    }
}

/* ---- the trading panel ------------------------------------------------------------ */

enum { PC_SYM, PC_QTY, PC_AVG, PC_LAST, PC_VALUE, PC_PL, PC_PCT, PC_ACT, PC_COUNT };
static const TrCol POS_COLS[PC_COUNT] = {
    { "Symbol", "GOOGL", 0, true, 0, false }, { "Qty", "99999", 0, true, 0, true },
    { "Avg price", "1324.00", 0, true, 3, true }, { "Last", "1324.00", 0, true, 4, true },
    { "Market value", "$999,999.00", 0, true, 2, true },
    { "Unrealized P&L", "+$99,999.00", 0, true, 0, true },
    { "P&L %", "+100.00%", 0, true, 1, true }, { "", "", 92, false, 0, true },
};

static void positions_table(AppState *st, unsigned keep, const int *px, int rowH) {
    TrStore *t = &st->store;
    RC_TableColumn cols[PC_COUNT];
    const int ncols = table_columns(PC_COUNT, keep, px, cols);
    int picked = -1, closing = -1;

    if (t->holdings == 0) {
        table_empty("No open positions", "Fills from the ticket appear here");
        return;
    }
    table_head(POS_COLS, PC_COUNT, keep, px, NULL, -1, false);
    rcBox(.w = "grow", .h = "grow") {
        if (begin_table("pos_table", cols, ncols)) {
            const RC_Style s = rcGetStyle();
            for (int i = 0; i < t->count; i++) {
                const TrInstrument *it = &t->inst[i];
                if (!it->shares)
                    continue;
                const char *id = row_id(st, ID_POS, i);
                const RC_Color pl = tr_dir(it->pl);

                rcTableRow(.id = id, .className = i == t->selected ? "bg-primary/14"
                                                                   : "hover:bg-surface-alt");
                cell_bold(it->symbol, s.text, rowH);
                rcTableNext();
                cell(it->sharesStr, 13, s.text, true, rowH);
                if (keep >> PC_AVG & 1u) {
                    rcTableNext();
                    cell(it->avgStr, 13, s.text, true, rowH);
                }
                if (keep >> PC_LAST & 1u) {
                    rcTableNext();
                    cell(it->priceStr, 13, s.text, true, rowH);
                }
                if (keep >> PC_VALUE & 1u) {
                    rcTableNext();
                    cell(it->valueStr, 13, s.text, true, rowH);
                }
                rcTableNext();
                cell(it->plStr, 13, pl, true, rowH);
                if (keep >> PC_PCT & 1u) {
                    rcTableNext();
                    cell(it->plPctStr, 13, pl, true, rowH);
                }
                rcTableNext();
                if (cell_action(row_id(st, ID_POS_CLOSE, i), "Close", rowH))
                    closing = i;
                else if (rcClicked(id))
                    picked = i;
            }
            rcEndTable();
        }
    }
    rcScrollbar("pos_table");
    if (picked >= 0) {
        st->pickSymbol = picked;
        st->priceText[0] = '\0';
    }
    if (closing >= 0) {
        if (st->confirmOrders) {
            st->closeInst = closing;
            st->modalConfirm = true;
        } else {
            tr_close_position(t, closing);
        }
    }
}

enum { OC_SYM, OC_SIDE, OC_TYPE, OC_QTY, OC_PRICE, OC_LAST, OC_TIME, OC_ACT, OC_COUNT };
static const TrCol ORDER_COLS[OC_COUNT] = {
    { "Symbol", "GOOGL", 0, true, 0, false }, { "Side", "", 64, true, 0, false },
    { "Type", "Market", 0, true, 3, false }, { "Qty", "99999", 0, true, 0, true },
    { "Price", "1324.00", 0, true, 0, true }, { "Last", "1324.00", 0, true, 2, true },
    { "Placed", "12:42", 0, true, 1, true }, { "", "", 92, false, 0, true },
};

static void orders_table(AppState *st, unsigned keep, const int *px, int rowH) {
    TrStore *t = &st->store;
    RC_TableColumn cols[OC_COUNT];
    const int ncols = table_columns(OC_COUNT, keep, px, cols);
    int cancel = -1, picked = -1;

    if (t->workingCount == 0) {
        table_empty("No working orders", "A limit or stop order rests here until it fills");
        return;
    }
    table_head(ORDER_COLS, OC_COUNT, keep, px, NULL, -1, false);
    rcBox(.w = "grow", .h = "grow") {
        if (begin_table("ord_table", cols, ncols)) {
            const RC_Style s = rcGetStyle();
            for (int k = 0; k < t->workingCount; k++) {
                const TrWorking *o = &t->working[k];
                const TrInstrument *it = &t->inst[o->inst];
                const char *id = row_id(st, ID_ORDER, o->tag);

                rcTableRow(.id = id, .className = "hover:bg-surface-alt");
                cell_bold(it->symbol, s.text, rowH);
                rcTableNext();
                cell_side(o->side == TR_BUY, rowH);
                if (keep >> OC_TYPE & 1u) {
                    rcTableNext();
                    cell(ORDER_TYPES[o->type], 13, s.textMuted, false, rowH);
                }
                rcTableNext();
                cell(o->qtyStr, 13, s.text, true, rowH);
                rcTableNext();
                cell(o->priceStr, 13, s.text, true, rowH);
                if (keep >> OC_LAST & 1u) {
                    rcTableNext();
                    cell(it->priceStr, 13, s.textMuted, true, rowH);
                }
                if (keep >> OC_TIME & 1u) {
                    rcTableNext();
                    cell(o->placedStr, 13, s.textMuted, true, rowH);
                }
                rcTableNext();
                if (cell_action(row_id(st, ID_ORDER_X, o->tag), "Cancel", rowH))
                    cancel = k;
                else if (rcClicked(id))
                    picked = o->inst;
            }
            rcEndTable();
        }
    }
    rcScrollbar("ord_table");
    if (picked >= 0)
        st->pickSymbol = picked;
    if (cancel >= 0)
        tr_cancel_order(t, cancel);
}

enum { HC_TIME, HC_SYM, HC_SIDE, HC_QTY, HC_PRICE, HC_VALUE, HC_PL, HC_COUNT };
static const TrCol FILL_COLS[HC_COUNT] = {
    { "Time", "Oct 30 12:42", 0, true, 0, false }, { "Symbol", "GOOGL", 0, true, 0, false },
    { "Side", "", 64, true, 0, false }, { "Qty", "99999", 0, true, 3, true },
    { "Price", "1324.00", 0, true, 0, true }, { "Value", "$999,999.00", 0, true, 1, true },
    { "Realized P&L", "+$99,999.00", 0, true, 2, true },
};

/* The fill log, newest first, each row a real entry in the account's books. */
static void history_table(AppState *st, unsigned keep, const int *px, int rowH) {
    TrStore *t = &st->store;
    RC_TableColumn cols[HC_COUNT];
    const int ncols = table_columns(HC_COUNT, keep, px, cols);
    const RC_Style s = rcGetStyle();
    int picked = -1;

    if (t->fillCount == 0) {
        table_empty("No fills yet", "Every fill from the ticket is logged here");
        return;
    }
    table_head(FILL_COLS, HC_COUNT, keep, px, NULL, -1, false);
    rcBox(.w = "grow", .h = "grow") {
        if (begin_table("fill_table", cols, ncols)) {
            for (int k = t->fillCount - 1; k >= 0; k--) {
                const TrFill *f = &t->fills[k];
                const char *id = row_id(st, ID_FILL, f->tag);

                rcTableRow(.id = id, .className = "hover:bg-surface-alt");
                cell(f->timeStr, 13, s.textMuted, false, rowH);
                rcTableNext();
                cell_bold(t->inst[f->inst].symbol, s.text, rowH);
                rcTableNext();
                cell_side(f->side == TR_BUY, rowH);
                if (keep >> HC_QTY & 1u) {
                    rcTableNext();
                    cell(f->qtyStr, 13, s.text, true, rowH);
                }
                rcTableNext();
                cell(f->priceStr, 13, s.text, true, rowH);
                if (keep >> HC_VALUE & 1u) {
                    rcTableNext();
                    cell(f->valueStr, 13, s.text, true, rowH);
                }
                if (keep >> HC_PL & 1u) {
                    rcTableNext();
                    cell(f->realizedStr, 13,
                         f->side == TR_SELL ? tr_dir(f->realized) : s.textMuted, true, rowH);
                }
                if (rcClicked(id))
                    picked = f->inst;
            }
            rcEndTable();
        }
    }
    rcScrollbar("fill_table");
    if (picked >= 0) {
        st->pickSymbol = picked;
        st->priceText[0] = '\0';
    }
}

/* The equity curve's axes, in the words a reader wants: dollars and clock times. */
static int equity_axis_money(double v, char *out, int cap, void *user) {
    const int k = tr_fmt_cash((int64_t)(v * 100.0), out, cap) - 3;

    (void)user;
    out[k] = '\0';     /* a tick reads in whole dollars */
    return k;
}

static int equity_readout_money(double v, char *out, int cap, void *user) {
    (void)user;
    return tr_fmt_cash((int64_t)(v * 100.0), out, cap);
}

static int equity_axis_time(double v, char *out, int cap, void *user) {
    const TrStore *t = (const TrStore *)user;

    return tr_fmt_time(t->equityStart + (int64_t)v * t->equityStep, out, cap);
}

/* A figure of the account: its name over its value. */
static void account_figure(const char *label, const char *value, RC_Color color) {
    const RC_Style s = rcGetStyle();

    rcRow(.align = "cl", .w = "grow", .h = "24px") {
        run(label, 13, s.textMuted);
        rcSpacer() {}
        run(value, 13, color);
    }
}

/* Net liquidation in another currency, beside the menu that picks it. The pick is the
   go-ahead: the rates come from the network, so nothing is fetched while it reads USD. */
static void account_currency(AppState *st, int64_t equity) {
    const RC_Style s = rcGetStyle();
    TrFx *fx = &st->fx;
    const char *value = "";
    RC_Color ink = s.textMuted;

    if (fx->pick && fx->state == TR_FX_IDLE)
        fx->want = true;
    if (fx->pick && fx->state == TR_FX_READY) {
        tr_fmt_amount((int64_t)((double)equity * fx->rate[fx->pick] + 0.5), fx->amount,
                      (int)sizeof fx->amount);
        value = fx->amount;
        ink = s.text;
    } else if (fx->pick) {
        value = fx->state == TR_FX_FAILED ? "Unavailable" : "Loading...";
    }
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        run("Show in", 13, s.textMuted);
        if (rcBeginMenu("fx_pick", TR_FX_CODES[fx->pick], .caret = true)) {
            for (int c = 0; c < TR_FX_COUNT; c++)
                if (check_item(TR_FX_CODES[c], fx->pick == c))
                    fx->pick = c;
            rcEndMenu();
        }
        rcSpacer() {}
        run(value, 13, ink);
    }
    if (fx->pick && fx->state == TR_FX_READY) {
        rcRow(.gap = 4, .align = "cl", .w = "grow") {
            run("ECB reference rate of", 11, s.textMuted);
            run(fx->date, 11, s.textMuted);
        }
    } else if (fx->pick && fx->state == TR_FX_FAILED) {
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            run("The ECB's rates did not load.", 11, s.textMuted);
            if (rcButton("fx_retry", "Retry", .variant = RC_BTN_GHOST))
                fx->want = true;
        }
    }
}

/* The Account tab: the figures beside the session's equity curve. This chart IS rcChart: it
   needs nothing a line chart does not have, and its readout stays in front of everything
   and inside the window on its own. */
static void account_panel(AppState *st) {
    const RC_Style s = rcGetStyle();
    TrStore *t = &st->store;
    RC_Series equity = { 0 };
    RC_Axis x = { 0 }, y = { 0 };

    equity.y = t->equityCurve;
    equity.count = TR_EQUITY_N;
    equity.kind = RC_SERIES_AREA;
    equity.color = pal.accent;
    equity.label = "Equity";
    equity.thickness = 1.5f;
    x.format = equity_axis_time;
    x.formatUserData = t;
    x.ticks = 5;
    x.min = RC_VAL(0.0f);
    x.max = RC_VAL((float)(TR_EQUITY_N - 1));
    y.format = equity_axis_money;
    y.readoutFormat = equity_readout_money;
    y.ticks = 4;
    y.grid = true;
    {
        float lo = t->equityCurve[0], hi = lo;
        for (int k = 1; k < TR_EQUITY_N; k++) {
            if (t->equityCurve[k] < lo)
                lo = t->equityCurve[k];
            if (t->equityCurve[k] > hi)
                hi = t->equityCurve[k];
        }
        const float air = hi > lo ? (hi - lo) * 0.1f : 1.0f;
        y.min = RC_VAL(lo - air);
        y.max = RC_VAL(hi + air);
    }

    rcRow(.gap = 16, .px = 12, .py = 8, .w = "grow", .h = "grow") {
        rcColumn(.wType = RC_PX(232)) {
            account_figure("Net liquidation", t->equityStr, s.text);
            account_figure("Cash", t->cashStr, s.text);
            account_figure("Unrealized P&L", t->unrealizedStr, tr_dir(t->unrealized));
            account_figure("Realized P&L", t->realizedStr, tr_dir(t->realized));
            account_figure("Today", t->dayPlStr, tr_dir(t->dayPl));
            account_currency(st, t->equity);
        }
        rcBox(.w = "grow", .h = "grow") {
            rcChart("equity_chart", &equity, 1, .className = "text-[12px]", .x = x, .y = y,
                    .tooltip = true);
        }
    }
}

/* A tab over the bottom panel, and a keyboard stop: Tab reaches it, and Enter or Space opens it. */
static bool panel_tab(const char *id, const char *label, const char *count, bool active, int px) {
    const RC_Style s = rcGetStyle();

    /* .pt matches the underline, so the label sits on the row's center line, level with the
       account strip and the +N tip beside the tabs. */
    rcColumn(.id = id, .focusable = true, .pt = 2, .px = (float)px, .align = "cc", .h = "grow") {
        rcBox(.h = "grow") {}
        rcRow(.gap = 6, .align = "cl") {
            run(label, 13, active || rcShowsHover(id) ? s.text : s.textMuted);
            if (count)
                run(count, 12, s.textMuted);
        }
        rcBox(.h = "grow") {}
        rcBox(.bg = active ? s.primary : RC_UNSET_COLOR, .w = "grow", .h = "2px") {}
    }
    return rcClicked(id);
}

/* One figure in the panel's account strip. */
static void strip_figure(const char *label, const char *value, RC_Color color) {
    rcRow(.gap = 6, .align = "cl") {
        run(label, 12, rcGetStyle().textMuted);
        run(value, 13, color);
    }
}

/* The trading panel under the chart: tabs on the left, the account strip on the right, the
   book in a column of its own when there is room. The open table's columns are resolved first,
   because its +N tip stands in the tab row above it, in flow, where it can cover nothing. */
static void trading_panel(AppState *st, TrArm arm, float panelW, int panelH, bool finger) {
    const RC_Style s = rcGetStyle();
    TrStore *t = &st->store;
    const int rowH = finger ? TR_FINGER_H : TR_ROW_H;
    const bool depthColumn = arm == TR_ARM_DESK;
    const float tableW = panelW - (depthColumn ? (float)TR_DEPTH_W + 1.0f : 0.0f);
    const int tabPx = depthColumn ? 12 : 8;
    int px[TR_MAX_COLS], hidden = 0;
    unsigned keep = 0;
    int tab = st->tab;

    if (depthColumn && tab == TR_TAB_DEPTH)
        tab = TR_TAB_POSITIONS;
    if (tab == TR_TAB_POSITIONS && t->holdings)
        keep = fit_columns(POS_COLS, PC_COUNT, body_width("pos_table", tableW), px, &hidden);
    else if (tab == TR_TAB_ORDERS && t->workingCount)
        keep = fit_columns(ORDER_COLS, OC_COUNT, body_width("ord_table", tableW), px, &hidden);
    else if (tab == TR_TAB_HISTORY && t->fillCount)
        keep = fit_columns(FILL_COLS, HC_COUNT, body_width("fill_table", tableW), px, &hidden);
    const char *nPos = rcFormat(&st->frame, "%d", t->holdings).chars;
    const char *nOrd = rcFormat(&st->frame, "%d", t->workingCount).chars;
    const char *nFil = rcFormat(&st->frame, "%d", t->fillCount).chars;
    rcRow(.bg = s.border, .gap = 1, .w = "grow", .hType = RC_PX(panelH)) {
        rcColumn(.bg = s.surface, .w = "grow", .h = "grow") {
            rcRow(.gap = 2, .pr = 8, .align = "cl", .w = "grow", .hType = RC_PX(TR_TAB_H)) {
                if (panel_tab("tab_pos", "Positions", nPos, tab == TR_TAB_POSITIONS, tabPx))
                    st->tab = TR_TAB_POSITIONS;
                if (panel_tab("tab_ord", "Orders", nOrd, tab == TR_TAB_ORDERS, tabPx))
                    st->tab = TR_TAB_ORDERS;
                if (panel_tab("tab_his", "History", nFil, tab == TR_TAB_HISTORY, tabPx))
                    st->tab = TR_TAB_HISTORY;
                if (panel_tab("tab_acc", "Account", NULL, tab == TR_TAB_ACCOUNT, tabPx))
                    st->tab = TR_TAB_ACCOUNT;
                if (!depthColumn && panel_tab("tab_dep", "Book", NULL, tab == TR_TAB_DEPTH, tabPx))
                    st->tab = TR_TAB_DEPTH;
                rcSpacer() {}
                if (tableW >= 660.0f) {
                    strip_figure("Equity", t->equityStr, s.text);
                    rcBox(.w = "12px") {}
                    strip_figure("Unrealized", t->unrealizedStr, tr_dir(t->unrealized));
                    rcBox(.w = "4px") {}
                }
                hidden_tip("panel_hidden", hidden);
            }
            hline();
            switch (tab) {
            case TR_TAB_POSITIONS:
                positions_table(st, keep, px, rowH);
                break;
            case TR_TAB_ORDERS:
                orders_table(st, keep, px, rowH);
                break;
            case TR_TAB_HISTORY:
                history_table(st, keep, px, rowH);
                break;
            case TR_TAB_ACCOUNT:
                account_panel(st);
                break;
            default:
                depth_ladder(st, panelH - TR_TAB_H - 1, true);
                break;
            }
        }
        if (depthColumn) {
            rcColumn(.bg = s.surface, .overflow = "hidden", .h = "grow",
                     .wType = RC_PX(TR_DEPTH_W)) {
                depth_ladder(st, panelH, false);
            }
        }
    }
}

/* ---- chrome ----------------------------------------------------------------------- */

static const struct {
    const char *label;
    RC_IconCallback icon;
} TR_CHART_TYPES[TR_CHART_COUNT] = {
    { "Candles", rcIconChartCandlestick }, { "Line", rcIconChartLine }, { "Area", rcIconChartArea },
};

static void toggle_indicator_item(AppState *st, const char *label, unsigned bit) {
    if (check_item(label, st->show & bit))
        st->show ^= bit;
}

static void indicator_items(AppState *st) {
    toggle_indicator_item(st, "Volume", TR_SHOW_VOLUME);
    toggle_indicator_item(st, "Bollinger Bands 20, 2", TR_SHOW_BB);
    toggle_indicator_item(st, "SMA 20", TR_SHOW_SMA);
    toggle_indicator_item(st, "EMA 50", TR_SHOW_EMA);
    toggle_indicator_item(st, "RSI 14", TR_SHOW_RSI);
}

/* The phone's one menu for how the chart looks: what the desk's two toolbar menus hold. */
static void chart_menu(AppState *st) {
    if (rcBeginMenu("chart_menu", "", .leadingIcon = rcIconSquareFunction)) {
        for (int i = 0; i < TR_CHART_COUNT; i++)
            if (check_item(TR_CHART_TYPES[i].label, st->chartType == i))
                st->chartType = i;
        indicator_items(st);
        rcEndMenu();
    }
}

static void open_search(AppState *st) {
    st->modalSearch = true;
    st->searchFocus = true;
    st->search[0] = '\0';
    st->searchRow = 0;
}

static void undo_drawing(AppState *st) {
    if (st->drawCount == 0 && st->clearedCount > 0) {
        st->drawCount = st->drawTop = st->clearedCount;
        st->clearedCount = 0;
    } else if (st->drawCount > 0) {
        st->drawCount--;
    }
}

static void redo_drawing(AppState *st) {
    if (st->drawCount < st->drawTop)
        st->drawCount++;
}

/* The chart's own controls. They sit in the titlebar, each group marked .noDrag so it takes
   its clicks, and the band's empty stretch still moves the window. */
static void chart_toolbar(AppState *st, bool roomy) {
    const RC_Style s = rcGetStyle();
    const TrInstrument *it = tr_selected(&st->store);
    const TrStore *t = &st->store;

    rcRow(.id = "sym_btn", .focusable = true, .gap = 6, .px = 8, .align = "cl",
          .borderRadius = "all-sm", .noDrag = true, .hType = RC_PX(TR_BAR_CTL),
          .tooltip = "Search symbols (" PRIMARY_KEY "+K)", .className = control_class(false)) {
        rcIconSearch(15.0f, s.textMuted);
        run_bold(it->symbol, 14, s.text);
    }
    if (rcClicked("sym_btn"))
        open_search(st);
    vline(20);
    rcRow(.gap = 1, .align = "cl", .noDrag = true) {
        if (roomy) {
            for (int i = 0; i < TR_IV_COUNT; i++)
                if (text_button(IV_ID[i], IV_LABEL[i], t->interval == i, TR_BAR_CTL, NULL))
                    set_interval(st, i);
        } else if (rcBeginMenu("iv_menu", IV_LABEL[t->interval], .caret = true)) {
            for (int i = 0; i < TR_IV_COUNT; i++)
                if (check_item(IV_LABEL[i], t->interval == i))
                    set_interval(st, i);
            rcEndMenu();
        }
    }
    vline(20);
    rcRow(.gap = 4, .align = "cl", .noDrag = true) {
        if (rcBeginMenu("type_menu", "", .caret = true,
                        .leadingIcon = TR_CHART_TYPES[st->chartType].icon)) {
            for (int i = 0; i < TR_CHART_COUNT; i++)
                if (rcMenuItem(TR_CHART_TYPES[i].label, .leadingIcon = TR_CHART_TYPES[i].icon))
                    st->chartType = i;
            rcEndMenu();
        }
        if (rcBeginMenu("ind_menu", roomy ? "Indicators" : "", .caret = true,
                        .leadingIcon = rcIconSquareFunction)) {
            indicator_items(st);
            rcEndMenu();
        }
    }
    vline(20);
    rcRow(.gap = 2, .align = "cl", .noDrag = true) {
        if (icon_button("undo", rcIconUndo2, false, st->drawCount > 0 || st->clearedCount > 0,
                        TR_BAR_CTL, "Undo the last drawing (" PRIMARY_KEY "+Z)"))
            undo_drawing(st);
        if (icon_button("redo", rcIconRedo2, false, st->drawCount < st->drawTop, TR_BAR_CTL,
                        "Redo (" PRIMARY_KEY "+Y)"))
            redo_drawing(st);
    }
}

/* The desks' top band. It is the window's titlebar too: RC_ID_WINDOW_DRAG moves the window from
   anywhere not marked .noDrag, and the band keeps its height under zoom, because the drag
   strip the OS tracks is pinned at .titlebar.height. */
static void top_bar(AppState *st, TrArm arm, float viewW) {
    const RC_Style s = rcGetStyle();
    /* On a desktop the window controls close the band at the window's edge, as an OS
       titlebar's do. A phone draws none and has no focus button, so its last control
       keeps the margin the band's left end has. */
    const float endPad = arm == TR_ARM_PHONE && rcWindowControlsWidth() <= 0.0f ? 12.0f : 0.0f;

    rcUnzoomed()
    rcRow(.id = RC_ID_WINDOW_DRAG, .bg = s.chrome, .gap = 8, .pl = 12, .pr = endPad, .align = "cl",
          .w = "grow", .hType = RC_PX(TR_TOPBAR_H)) {
        rcIconRayClayLogo(22.0f);
        if (arm == TR_ARM_PHONE) {
            run_bold("RayClay Markets", 14, s.text);
        } else {
            chart_toolbar(st, viewW >= 1000.0f);
        }
        rcSpacer() {}
        if (viewW >= 480.0f) {
            rcRow(.bg = rcWithAlpha(s.primary, 0.157f), .gap = 6, .px = 8, .align = "cl",
                  .borderRadius = "all-sm", .noDrag = true, .h = "22px",
                  .tooltip = "A simulated market and simulated money") {
                rcBox(.bg = s.success, .borderRadius = "all-full", .w = "6px", .h = "6px") {}
                run("Paper account", 12, pal.accent);
            }
        }
        if (arm != TR_ARM_PHONE) {
            rcRow(.gap = 2, .align = "cl", .noDrag = true) {
                if (icon_button("focus_btn", st->focusMode ? rcIconShrink : rcIconExpand,
                                st->focusMode, true, TR_BAR_CTL,
                                st->focusMode ? "Show the panels (F)" : "Chart only (F)"))
                    st->focusMode = !st->focusMode;
            }
        }
        rcWindowControls();
    }
}

static void tool_button(AppState *st, const char *id, RC_IconCallback icon, TrTool tool, int side,
                        const char *tip) {
    if (icon_button(id, icon, st->tool == (int)tool, true, side, tip)) {
        st->tool = tool;
        st->placing = false;
        st->measuring = false;
    }
}

/* The left rail: drawing tools, then the view, then what happens to the drawings. */
static void tool_rail(AppState *st, bool finger) {
    const RC_Style s = rcGetStyle();
    const int side = finger ? TR_FINGER_H : 32;
    const float mid = st->plot.w * 0.5f;

    rcColumn(.bg = s.surface, .gap = 2, .py = 6, .align = "tc", .h = "grow",
             .wType = RC_PX(TR_RAIL_W)) {
        tool_button(st, "t_cross", rcIconCrosshair, TR_TOOL_CROSSHAIR, side, "Crosshair (Esc)");
        tool_button(st, "t_trend", rcIconTrendingUp, TR_TOOL_TREND, side,
                    "Trend line: click two points");
        tool_button(st, "t_hline", rcIconSeparatorHorizontal, TR_TOOL_HLINE, side,
                    "Horizontal line: click a price");
        tool_button(st, "t_meas", rcIconRuler, TR_TOOL_MEASURE, side, "Measure: click two points");
        rcBox(.bg = s.border, .w = "24px", .h = "1px") {}
        if (icon_button("z_in", rcIconZoomIn, false, st->barPx < 40.0f, side, "Zoom in (+)"))
            zoom_at(st, mid, 1.3f);
        if (icon_button("z_out", rcIconZoomOut, false, st->barPx > 2.0f, side, "Zoom out (-)"))
            zoom_at(st, mid, 1.0f / 1.3f);
        rcBox(.bg = s.border, .w = "24px", .h = "1px") {}
        if (icon_button("t_magnet", rcIconMagnet, st->magnet, true, side,
                        "Magnet: snap to open, high, low, close"))
            st->magnet = !st->magnet;
        if (icon_button("t_hide", st->hideDrawings ? rcIconEyeOff : rcIconEye, st->hideDrawings,
                        true, side, st->hideDrawings ? "Show drawings" : "Hide drawings"))
            st->hideDrawings = !st->hideDrawings;
        if (icon_button("t_trash", rcIconTrash2, false, st->drawCount > 0, side,
                        "Remove all drawings"))
            clear_drawings(st);
        rcSpacer() {}
        if (icon_button("t_set", rcIconSettings, false, true, side, "Settings"))
            st->modalSettings = true;
    }
}

/* ---- the phone's pages ---------------------------------------------------------------- */

/* A row of the phone's list, the one every retail broker opens on: name, trend, price, and a
   filled pill for the move. */
static void phone_watch_row(AppState *st, int i) {
    const RC_Style s = rcGetStyle();
    TrStore *t = &st->store;
    const TrInstrument *it = &t->inst[i];
    const char *id = row_id(st, ID_PHONE, i);
    const bool up = it->changeBps >= 0;

    rcRow(.id = id, .gap = 12, .px = 16, .align = "cl", .w = "grow", .h = "60px",
          .className = "hover:bg-surface-alt") {
        rcColumn(.gap = 2, .overflow = "hidden", .w = "grow") {
            run_bold(it->symbol, 14, s.text);
            /* As tall as the pill across the row, so both lines sit level with the price's. */
            rcBox(.align = "cl", .w = "grow", .h = "20px") {
                rcTextC(it->name, .color = s.textMuted, .size = 12, .wrap = "e");
            }
        }
        rcBox(.w = "72px", .h = "28px") {
            trend(st, i, it, tr_dir(it->changeBps));
        }
        rcColumn(.gap = 2, .align = "cr", .w = "84px") {
            run(it->priceStr, 14, s.text);
            /* A shared minimum width, so the pills line up down the column; a longer move
               widens its own. */
            rcBox(.bg = up ? s.success : s.danger, .px = 6, .align = "cc",
                  .borderRadius = "all-xs", .h = "20px", .wMin = RC_VAL(56)) {
                run(it->changePctStr, 12, pal.onSide);
            }
        }
    }
    if (rcClicked(id)) {
        st->pickSymbol = i;
        st->priceText[0] = '\0';
        st->page = TR_PAGE_CHART;
    }
}

/* The spans the phone's curve takes, Robinhood's chips, and the words its change reads with.
   trader_update hands the store the one in force. */
static const struct {
    const char *label, *id, *word;
    int sessions;
} TR_SPANS[] = {
    { "1D", "sp0", "today", 1 },       { "1W", "sp1", "past week", 5 },
    { "1M", "sp2", "past month", 21 }, { "3M", "sp3", "past 3 months", 63 },
    { "1Y", "sp4", "past year", 252 },
};

/* The phone's curve over a hairline at what its change is measured from. A sparkline draws one
   line, so this is rcChart with both axes hidden, its range pinned around the two with room
   under the curve, as trend_band's is. */
static void equity_curve(AppState *st, float base, RC_Color color) {
    static const float ENDS[2] = { 0.0f, (float)(TR_EQUITY_N - 1) };
    const TrStore *t = &st->store;
    float *level = (float *)rcArenaAlloc(&st->frame, sizeof(float) * 2);
    RC_Series series[2] = { { 0 }, { 0 } };
    RC_Axis x = { 0 }, y = { 0 };
    float lo = base, hi = base;

    for (int k = 0; k < TR_EQUITY_N; k++) {
        lo = t->equityCurve[k] < lo ? t->equityCurve[k] : lo;
        hi = t->equityCurve[k] > hi ? t->equityCurve[k] : hi;
    }
    const float span = hi > lo ? hi - lo : 1.0f;
    series[0].y = t->equityCurve;
    series[0].count = TR_EQUITY_N;
    series[0].kind = RC_SERIES_AREA;
    series[0].color = color;
    series[0].thickness = 1.25f;
    if (level) {               /* a full arena draws the curve alone */
        level[0] = level[1] = base;
        series[1].y = level;
        series[1].x = ENDS;
        series[1].count = 2;
        series[1].kind = RC_SERIES_LINE;
        series[1].color = rcGetStyle().textMuted;
        series[1].thickness = 1.0f;
    }
    x.hide = y.hide = true;
    x.min = RC_VAL(0.0f);
    x.max = RC_VAL((float)(TR_EQUITY_N - 1));
    y.min = RC_VAL(lo - span * 0.25f);
    y.max = RC_VAL(hi + span * 0.08f);
    rcChart("phone_equity", series, level ? 2 : 1, .x = x, .y = y);
}

static void phone_watchlist(AppState *st, bool shortWindow, bool finger) {
    const RC_Style s = rcGetStyle();
    const TrStore *t = &st->store;
    const int span = st->equitySpan;
    /* A day's change is from the last close, as a broker's "today" is; a longer span's is from
       where its curve starts. */
    const int64_t change = span == 0 ? t->dayPl : t->equity - t->equityOpen;
    const char *changeStr = span == 0 ? t->dayPlStr : num_label(st, tr_fmt_pl, change);

    rcColumn(.id = "phone_watch", .scroll = "v", .w = "grow", .h = "grow") {
        /* A short window keeps the figure and gives the height back to the list. */
        if (shortWindow) {
            rcRow(.gap = 8, .px = 16, .py = 8, .align = "cl", .w = "grow") {
                run_bold(t->equityStr, 20, s.text);
                run(changeStr, 14, tr_dir(change));
                run(TR_SPANS[span].word, 13, s.textMuted);
            }
        } else {
            rcColumn(.gap = 2, .px = 16, .py = 12, .w = "grow") {
                run("Portfolio value", 13, s.textMuted);
                run_bold(t->equityStr, 30, s.text);
                rcRow(.gap = 6, .align = "cl") {
                    run(changeStr, 14, tr_dir(change));
                    run(TR_SPANS[span].word, 13, s.textMuted);
                }
            }
            /* The account's curve under its figure, as a phone broker leads with it, and the
               chips that pick its span under that, the chosen one in the curve's own color. */
            rcBox(.px = 16, .w = "grow", .h = "96px") {
                equity_curve(st, (float)(t->equity - change) / 100.0f, tr_dir(change));
            }
            rcRow(.pt = 4, .pb = 8, .px = 16, .align = "cl", .w = "grow") {
                for (size_t r = 0; r < sizeof TR_SPANS / sizeof TR_SPANS[0]; r++) {
                    const char *id = TR_SPANS[r].id;
                    const bool on = span == (int)r;
                    const RC_Color ink = on ? tr_dir(change) : control_ink(id, false);
                    rcBox(.align = "cc", .w = "grow") {
                        rcBox(.id = id, .focusable = true, .px = 8, .align = "cc",
                              .borderRadius = "all-sm", .hType = RC_PX(finger ? TR_FINGER_H : 28),
                              .className = !on ? control_class(false)
                                           : change >= 0 ? "bg-success/16" : "bg-danger/16") {
                            run(TR_SPANS[r].label, 13, ink);
                        }
                    }
                    if (rcClicked(id))
                        st->equitySpan = (int)r;
                }
            }
        }
        hline();
        for (int i = 0; i < t->count; i++)
            phone_watch_row(st, i);
    }
    rcScrollbar("phone_watch");
}

/* The phone's order sheet opens on a side. */
static void phone_trade_button(AppState *st, const char *id, bool buy, int h) {
    if (side_button(id, buy ? "Buy" : "Sell", buy, true, h)) {
        st->orderSide = buy ? TR_BUY : TR_SELL;
        st->modalTicket = true;
    }
}

/* The phone's chart page: the price big, the chart filling what is left, the two trade
   buttons always in reach. A short window folds it all onto one line. */
static void phone_chart(AppState *st, float viewW, bool shortWindow, bool finger) {
    const RC_Style s = rcGetStyle();
    TrStore *t = &st->store;
    const TrInstrument *it = tr_selected(t);
    /* Every control on the top row is as tall as rcBeginMenu's button, at either pointer. */
    const int ctl = finger ? TR_FINGER_H : TR_BAR_CTL;

    rcColumn(.w = "grow", .h = "grow") {
        rcRow(.gap = 12, .px = 12, .py = (float)(shortWindow ? 4 : 8), .align = "cl",
              .w = "grow") {
            rcRow(.id = "sym_btn", .focusable = true, .gap = 6, .px = 8, .align = "cl",
                  .borderRadius = "all-sm", .hType = RC_PX(ctl),
                  .className = control_class(false)) {
                rcIconSearch(15.0f, s.textMuted);
                run_bold(it->symbol, 16, s.text);
            }
            if (rcClicked("sym_btn"))
                open_search(st);
            /* The move sits on the price's baseline, as a quote reads. */
            rcRow(.gap = 12, .className = "items-baseline") {
                run_bold(it->priceStr, shortWindow ? 20 : 24, s.text);
                /* The first thing to go when the row runs out of room. */
                if (viewW >= (shortWindow ? 640.0f : 400.0f))
                    run(it->changePctStr, 14, tr_dir(it->change));
            }
            rcSpacer() {}
            if (rcBeginMenu("iv_menu", IV_LABEL[t->interval], .caret = true)) {
                for (int i = 0; i < TR_IV_COUNT; i++)
                    if (check_item(IV_LABEL[i], t->interval == i))
                        set_interval(st, i);
                rcEndMenu();
            }
            chart_menu(st);
            if (shortWindow) {
                rcBox(.w = "96px") {
                    phone_trade_button(st, "act_buy", true, ctl);
                }
                rcBox(.w = "96px") {
                    phone_trade_button(st, "act_sell", false, ctl);
                }
            }
        }
        chart_pane(st, viewW, true, !shortWindow);
        if (!shortWindow) {
            rcRow(.bg = s.surface, .gap = 8, .px = 12, .py = 8, .w = "grow") {
                phone_trade_button(st, "act_buy", true, 44);
                phone_trade_button(st, "act_sell", false, 44);
            }
        }
    }
}

/* A holding on the phone: what it is on the left, what it is worth on the right. */
static void phone_position(AppState *st, int i) {
    const RC_Style s = rcGetStyle();
    TrStore *t = &st->store;
    const TrInstrument *it = &t->inst[i];
    const char *id = row_id(st, ID_POS, i);

    rcRow(.id = id, .gap = 12, .px = 16, .align = "cl", .w = "grow", .h = "56px",
          .className = "hover:bg-surface-alt") {
        rcColumn(.gap = 2, .w = "grow") {
            run_bold(it->symbol, 14, s.text);
            rcRow(.gap = 4, .align = "cl") {
                run(it->sharesStr, 12, s.textMuted);
                run("at", 12, s.textMuted);
                run(it->avgStr, 12, s.textMuted);
            }
        }
        rcColumn(.gap = 2, .align = "cr") {
            run(it->valueStr, 14, s.text);
            run(it->plStr, 12, tr_dir(it->pl));
        }
    }
    if (rcClicked(id)) {
        st->pickSymbol = i;
        st->priceText[0] = '\0';
        st->page = TR_PAGE_CHART;
    }
}

static void phone_portfolio(AppState *st) {
    const RC_Style s = rcGetStyle();
    TrStore *t = &st->store;
    int cancel = -1;

    rcColumn(.id = "phone_port", .scroll = "v", .w = "grow", .h = "grow") {
        rcColumn(.gap = 2, .px = 16, .py = 12, .w = "grow") {
            run("Net liquidation", 13, s.textMuted);
            run_bold(t->equityStr, 30, s.text);
            rcBox(.h = "8px") {}
            account_figure("Cash", t->cashStr, s.text);
            account_figure("Unrealized P&L", t->unrealizedStr, tr_dir(t->unrealized));
            account_figure("Realized P&L", t->realizedStr, tr_dir(t->realized));
        }
        hline();
        rcRow(.pt = 12, .pb = 4, .px = 16, .w = "grow") {
            run_bold("Positions", 14, s.text);
        }
        if (t->holdings == 0) {
            rcBox(.px = 16, .py = 8) {
                run("No open positions", 13, s.textMuted);
            }
        }
        for (int i = 0; i < t->count; i++)
            if (t->inst[i].shares)
                phone_position(st, i);
        rcRow(.pt = 12, .pb = 4, .px = 16, .w = "grow") {
            run_bold("Working orders", 14, s.text);
        }
        if (t->workingCount == 0) {
            rcBox(.px = 16, .py = 8) {
                run("None", 13, s.textMuted);
            }
        }
        for (int k = 0; k < t->workingCount; k++) {
            const TrWorking *o = &t->working[k];
            rcRow(.gap = 8, .px = 16, .align = "cl", .w = "grow", .h = "48px") {
                run_bold(t->inst[o->inst].symbol, 14, s.text);
                run(o->label, 13, s.textMuted);
                run(o->priceStr, 13, s.text);
                rcSpacer() {}
                if (rcButton(row_id(st, ID_ORDER_X, o->tag), "Cancel"))
                    cancel = k;
            }
        }
        rcBox(.h = "16px") {}
    }
    rcScrollbar("phone_port");
    if (cancel >= 0)
        tr_cancel_order(t, cancel);
}

/* A page in the phone's bottom bar, and a keyboard stop: Tab reaches it, and Enter or Space
   opens it. */
static bool page_tab(const char *id, RC_IconCallback icon, const char *label, bool active) {
    const RC_Style s = rcGetStyle();
    const RC_Color ink = active ? pal.accent : s.textMuted;

    rcColumn(.id = id, .focusable = true, .gap = 2, .align = "cc", .w = "grow", .h = "grow") {
        icon(20.0f, ink);
        run(label, 11, ink);
    }
    return rcClicked(id);
}

/* The page in force. A chart-first terminal opens on its chart in a short window, where a list
   has no room to read, until the user asks for a page. */
static int phone_page(const AppState *st, bool shortWindow) {
    if (shortWindow && !st->pageAsked && st->page == TR_PAGE_WATCHLIST)
        return TR_PAGE_CHART;
    return st->page;
}

static void phone_tabbar(AppState *st, bool shortWindow) {
    const RC_Style s = rcGetStyle();
    const int page = phone_page(st, shortWindow);

    rcRow(.bg = s.surface, .w = "grow", .hType = RC_PX(shortWindow ? 44 : TR_TABBAR_H)) {
        if (page_tab("pg_watch", rcIconList, "Watchlist", page == TR_PAGE_WATCHLIST)) {
            st->page = TR_PAGE_WATCHLIST;
            st->pageAsked = true;
        }
        if (page_tab("pg_chart", rcIconChartCandlestick, "Chart", page == TR_PAGE_CHART))
            st->page = TR_PAGE_CHART;
        if (page_tab("pg_port", rcIconWallet, "Portfolio", page == TR_PAGE_PORTFOLIO))
            st->page = TR_PAGE_PORTFOLIO;
        if (page_tab("pg_set", rcIconSettings, "Settings", false))
            st->modalSettings = true;
    }
}

/* ---- dialogs --------------------------------------------------------------------------- */

/* Every order goes through here: what it is, what it comes to, and the one button that sends
   it. A pending Close of a position is the same review of a market sale. */
static void confirm_modal(AppState *st, bool phone) {
    const RC_Style s = rcGetStyle();
    TrStore *t = &st->store;

    if (!rcBeginModal("modal_confirm", &st->modalConfirm)) {
        st->confirmShown = false;
        return;
    }
    const bool opening = !st->confirmShown;

    st->confirmShown = true;
    const bool closing = st->closeInst >= 0;
    const int inst = closing ? st->closeInst : t->selected;
    const TrInstrument *it = &t->inst[inst];
    const bool buy = !closing && st->orderSide == TR_BUY;
    const int qty = closing ? it->shares : st->ticketQty;
    const int type = closing ? TR_MARKET : st->orderType;
    const int64_t value = closing ? tr_close_value(t, inst) : st->ticketEstimate;
    const char *line = rcFormat(&st->frame, "%s %d %s", buy ? "Buy" : "Sell", qty,
                                it->symbol).chars;
    const int64_t price = type == TR_MARKET && qty ? value / qty : st->ticketPrice;
    const char *avg = num_label(st, tr_fmt_price, price);
    const char *est = num_label(st, tr_fmt_cash, value);
    const char *after = num_label(st, tr_fmt_cash, t->cash + (buy ? -value : value));
    const bool finger = rcPointerIsCoarse();

    rcColumn(.gap = 16, .w = phone ? "84vw" : "380px") {
        run_bold("Review order", 16, s.textMuted);
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            rcBox(.bg = buy ? s.success : s.danger, .borderRadius = "all-full", .w = "10px",
                  .h = "10px") {}
            run_bold(line, 24, s.text);
        }
        rcColumn(.gap = 2, .w = "grow") {
            ticket_value("Company", it->name, s.text, false);
            ticket_value("Order type", ORDER_TYPES[type], s.text, false);
            ticket_value(type == TR_MARKET ? "Est. price"
                         : type == TR_LIMIT ? "Limit price" : "Stop price", avg, s.text, false);
            ticket_value(buy ? "Estimated cost" : "Estimated credit", est, s.text, true);
            ticket_value("Buying power after", after, s.textMuted, false);
        }
        rcRow(.gap = 8, .w = "grow") {
            if (rcButton("btn_cancel", "Cancel", .className = finger ? "h-11" : "h-10"))
                st->modalConfirm = false;
            if (side_button("btn_confirm", buy ? "Place buy order" : "Place sell order", buy, true,
                            finger ? TR_FINGER_H : 40)) {
                if (closing)
                    tr_close_position(t, inst);
                else
                    tr_place_order(t, (TrSide)st->orderSide, (TrOrderType)st->orderType,
                                   st->ticketQty, st->ticketPrice);
                st->modalConfirm = false;
            }
        }
    }
    /* Placing the order is the dialog's primary action, so it takes focus as the dialog opens:
       Enter places the order and Tab reaches Cancel. A key goes to the control focused when its
       frame began, so the Enter that opened the dialog places nothing. */
    if (opening)
        rcSetFocus("btn_confirm");
    rcEndModal();
}

/* The symbol search, the terminal's fastest way anywhere: type, arrow, Enter. */
static void search_modal(AppState *st, bool phone) {
    const RC_Style s = rcGetStyle();
    TrStore *t = &st->store;
    int match[TR_MAX_INSTRUMENTS], n = 0;

    if (!rcBeginModal("modal_search", &st->modalSearch))
        return;
    for (int i = 0; i < t->count; i++)
        if (contains_nocase(t->inst[i].symbol, st->search) ||
            contains_nocase(t->inst[i].name, st->search))
            match[n++] = i;
    const bool down = rcKeyPressed(RC_KEY_DOWN) && st->searchRow < n - 1;
    const bool up = rcKeyPressed(RC_KEY_UP) && st->searchRow > 0;

    st->searchRow += down - up;
    if (st->searchRow >= n)
        st->searchRow = n > 0 ? n - 1 : 0;
    if (down || up)
        rcScrollIntoView("find_list", row_id(st, ID_FIND, match[st->searchRow]), RC_SCROLL_NEAREST);
    rcColumn(.gap = 12, .w = phone ? "84vw" : "480px", .hType = RC_PX(phone ? 300 : 400)) {
        rcBox(.w = "grow") {
            /* What Enter opens follows the typing: the best match is the first. */
            if (rcTextInput("find_in", st->search, (int)sizeof st->search,
                            .className = rcPointerIsCoarse() ? "h-11" : "h-9",
                            .placeholder = "Search symbol or company",
                            .leadingIcon = rcIconSearch, .clearButton = true))
                st->searchRow = 0;
            if (st->searchFocus) {
                rcSetFocus("find_in");
                st->searchFocus = false;
            }
        }
        rcColumn(.id = "find_list", .scroll = "v", .w = "grow", .h = "grow") {
            if (n == 0) {
                rcBox(.align = "cc", .w = "grow", .h = "120px") {
                    run("No symbol matches", 14, s.textMuted);
                }
            }
            for (int r = 0; r < n; r++) {
                const int i = match[r];
                const TrInstrument *it = &t->inst[i];
                const char *id = row_id(st, ID_FIND, i);
                const bool cur = r == st->searchRow;

                rcRow(.id = id, .gap = 12, .pl = 12, .pr = 16, .align = "cl",
                      .borderRadius = "all-sm", .w = "grow", .h = "36px",
                      .className = control_class(cur)) {
                    rcBox(.wType = RC_PX(56)) {
                        run_bold(it->symbol, 14, s.text);
                    }
                    rcBox(.overflow = "hidden", .w = "grow") {
                        rcTextC(it->name, .color = s.textMuted, .size = 13, .wrap = "e");
                    }
                    run(it->priceStr, 13, s.text);
                    rcBox(.align = "cr", .wType = RC_PX(60)) {
                        run(it->changePctStr, 13, tr_dir(it->changeBps));
                    }
                }
                if (rcClicked(id) || (cur && rcKeyPressedOn("find_in", RC_KEY_ENTER))) {
                    st->pickSymbol = i;
                    st->priceText[0] = '\0';
                    st->modalSearch = false;
                    st->page = TR_PAGE_CHART;
                }
            }
        }
        rcScrollbar("find_list");
    }
    rcEndModal();
}

static void settings_modal(AppState *st, bool phone) {
    static const char *const SPEEDS[] = { "Real time", "10x", "60x" };
    static const int SPEED_VALUES[] = { 1, 10, 60 };
    const RC_Style s = rcGetStyle();

    if (!rcBeginModal("modal_settings", &st->modalSettings))
        return;
    rcColumn(.gap = 16, .w = phone ? "84vw" : "380px") {
        run_bold("Settings", 20, s.text);
        rcRow(.align = "cl", .w = "grow") {
            run("Dark theme", 14, s.text);
            rcSpacer() {}
            rcToggle("set_dark", "", &st->darkMode);
        }
        rcRow(.align = "cl", .w = "grow") {
            run("Review orders before sending", 14, s.text);
            rcSpacer() {}
            rcToggle("set_confirm", "", &st->confirmOrders);
        }
        rcRow(.align = "cl", .w = "grow") {
            run("Market speed", 14, s.text);
            rcSpacer() {}
            rcBox(.wType = RC_PX(128)) {
                if (rcCombo("set_speed", &st->speedIndex, SPEEDS, 3))
                    st->store.speed = SPEED_VALUES[st->speedIndex];
            }
        }
        hline();
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            rcColumn(.gap = 2, .w = "grow") {
                run("Reset the account", 14, s.text);
                run("Back to the opening deposit", 12, s.textMuted);
            }
            if (rcButton("set_reset", "Reset", .variant = RC_BTN_DANGER))
                st->modalReset = true;
        }
        rcRow(.align = "cr", .w = "grow") {
            if (rcButton("set_done", "Done", .variant = RC_BTN_PRIMARY))
                st->modalSettings = false;
        }
    }
    /* A second dialog, over this one, because it cannot be undone. */
    if (rcBeginModal("modal_reset", &st->modalReset)) {
        rcColumn(.gap = 16, .w = phone ? "76vw" : "340px") {
            run_bold("Reset the account?", 16, s.text);
            rcTextL("Every position, order and fill goes, and the cash returns to the opening "
                    "deposit.", .color = s.textMuted, .size = 14);
            rcRow(.gap = 8, .align = "cr", .w = "grow") {
                if (rcButton("reset_no", "Cancel"))
                    st->modalReset = false;
                if (rcButton("reset_yes", "Reset account", .variant = RC_BTN_DANGER)) {
                    tr_reset_account(&st->store);
                    st->modalReset = false;
                }
            }
        }
        rcEndModal();
    }
    rcEndModal();
}

static void ticket_modal(AppState *st, bool phone) {
    if (!rcBeginModal("modal_ticket", &st->modalTicket))
        return;
    rcColumn(.gap = 12, .w = phone ? "84vw" : "340px") {
        order_ticket(st, true);
    }
    rcEndModal();
}

/* What the account just did, for four seconds, over everything: at the top right under the
   bar, clear of the tables and the ticket the trade came from. A dialog that opens takes the
   eye, so it closes the toast; one that a dialog's own button raises still shows. */
static void toast(AppState *st) {
    const RC_Style s = rcGetStyle();
    const bool dialog = st->modalConfirm || st->modalSettings || st->modalSearch
                     || st->modalTicket;

    if (dialog && !st->dialogUp)
        st->toastLeft = 0.0f;
    st->dialogUp = dialog;
    if (st->toastLeft <= 0.0f)
        return;
    rcRow(.id = "toast", .bg = s.surfaceAlt, .gap = 8, .px = 12, .py = 8, .align = "cl",
          .borderRadius = "all-md", .border = { .color = s.border, .width = 1 },
          .shadow = { .color = rcWithAlpha(RC_BLACK, 0.47f), .y = 4.0f, .blur = 16.0f },
          .floating = { .to = RC_ATTACH_ELEMENT, .toId = RC_ID_WINDOW_DRAG,
                        .parent = RC_ANCHOR_BOTTOM_RIGHT, .element = RC_ANCHOR_TOP_RIGHT,
                        .offset = { -12.0f, 8.0f }, .zIndex = INT16_MAX,
                        .capture = RC_CAPTURE_PASSTHROUGH }) {
        rcBox(.bg = pal.accent, .borderRadius = "all-full", .w = "8px", .h = "8px") {}
        run(st->store.event, 14, s.text);
    }
}

/* ---- the app's four functions ----------------------------------------------------------- */

void trader_seed(AppState *st, unsigned seed) {
    tr_zero(st, sizeof *st);
    tr_store_seed(&st->store, seed);
    st->frame.buffer = st->frameBytes;
    st->frame.bufferLength = sizeof st->frameBytes;
    st->store.selected = 0;
    st->show = TR_SHOW_VOLUME | TR_SHOW_BB | TR_SHOW_SMA | TR_SHOW_RSI;
    st->barPx = 7.0f;
    st->range = -1;
    st->pickSymbol = -1;
    st->pickRange = -1;
    st->sortColumn = -1;
    st->orderSide = TR_BUY;
    st->orderType = TR_MARKET;
    st->page = TR_PAGE_WATCHLIST;
    st->closeInst = -1;
    st->darkMode = true;
    st->confirmOrders = true;
    /* A default order size, so the ticket opens live rather than disabled. */
    st->qty[0] = '1';
    st->qty[1] = '0';
    st->toastSeq = st->store.eventSeq;
    st->seeded = true;
}

void trader_update(AppState *st, const AppCtx *ctx) {
    if (st->pickSymbol >= 0 && st->pickSymbol != st->store.selected) {
        /* A line half placed, or a measurement, is priced on the symbol it began on. */
        st->placing = st->measuring = false;
        tr_select(&st->store, st->pickSymbol);
    }
    if (st->pickRange >= 0)
        set_range(st, st->pickRange);
    st->pickSymbol = -1;
    st->pickRange = -1;
    /* The phone's chips span its curve; the desk's Account tab reads one session. */
    tr_store_equity_span(&st->store, tr_arm(ctx) == TR_ARM_PHONE
                                     ? TR_SPANS[st->equitySpan].sessions : 1);
    tr_store_step(&st->store, ctx->dt);
    ticket_update(st);
    if (st->store.eventSeq != st->toastSeq) {
        st->toastSeq = st->store.eventSeq;
        st->toastLeft = 4.0f;
    } else if (st->toastLeft > 0.0f) {
        st->toastLeft -= ctx->dt;
    }
}

/* Is a text field taking the keyboard? Then the plain keys are text, not shortcuts. */
static bool typing(void) {
    return rcIsFocused("qty_in") || rcIsFocused("price_in") || rcIsFocused("find_in");
}

static void shortcuts(AppState *st) {
    if (rcIsModalOpen())
        return;
    if (rcKeyPressed(RC_KEY_K) && rcModDown(RC_MOD_PRIMARY))
        open_search(st);
    if ((st->placing || st->measuring || st->tool != TR_TOOL_CROSSHAIR)
        && rcKeyPressed(RC_KEY_ESCAPE)) {
        st->placing = false;
        st->measuring = false;
        st->tool = TR_TOOL_CROSSHAIR;
    }
    if (typing())
        return;
    if (rcKeyPressed(RC_KEY_Z) && rcModDown(RC_MOD_PRIMARY))
        undo_drawing(st);
    if (rcKeyPressed(RC_KEY_Y) && rcModDown(RC_MOD_PRIMARY))
        redo_drawing(st);
    /* The plain keys below stay plain: Ctrl+= is the window's zoom, and Ctrl+F is no chart key. */
    if (rcModDown(RC_MOD_PRIMARY) || rcModDown(RC_MOD_CTRL) || rcModDown(RC_MOD_ALT))
        return;
    if (rcKeyPressed(RC_KEY_F))
        st->focusMode = !st->focusMode;
    if (rcKeyPressed(RC_KEY_EQUAL))
        zoom_at(st, st->plot.w * 0.5f, 1.3f);
    if (rcKeyPressed(RC_KEY_MINUS))
        zoom_at(st, st->plot.w * 0.5f, 1.0f / 1.3f);
    if (rcKeyPressed(RC_KEY_LEFT)) {
        st->viewEnd += 10.0f;
        st->range = -1;
        clamp_view(st);
    }
    if (rcKeyPressed(RC_KEY_RIGHT)) {
        st->viewEnd -= 10.0f;
        st->range = -1;
        clamp_view(st);
    }
}

/* The panel's height: about a quarter of the window, ending on a whole row, so that the first
   frame cuts no row through its glyphs. */
static int panel_height(float vh, int rowH) {
    const int chrome = TR_TAB_H + 1 + TR_HEAD_H + 1;
    const int rows = ((int)(vh * 0.27f) - chrome + rowH / 2) / rowH;

    return chrome + (rows < 3 ? 3 : rows > 8 ? 8 : rows) * rowH;
}

/* The watchlist's height: what the ticket leaves, down to a whole row, so that the ticket keeps
   its whole height with Review showing. Both boxes are last frame's, as the plot's is. */
static int watch_height(int rowH) {
    const RC_Box col = rcGetElementBox("right_col"), body = rcGetElementBox("ticket_body");
    const int chrome = 36 + TR_HEAD_H + 1;
    const int rows = ((int)(col.height - body.height) - 1 - chrome) / rowH;

    return chrome + (rows < 1 ? 1 : rows) * rowH;
}

void trader_layout(AppState *st, const AppCtx *ctx) {
    rcArenaReset(&st->frame);
    rcSetStyle(trader_style(st->darkMode));
    pal = trader_palette(st->darkMode);
    const RC_Style s = rcGetStyle();
    const float vw = app_view_w(ctx);
    const float vh = app_view_h(ctx);
    const TrArm arm = tr_arm(ctx);
    const bool phone = arm == TR_ARM_PHONE;
    const bool finger = ctx->view.coarsePointer;
    const RC_Insets safe = app_safe(ctx);
    const float usableW = vw - safe.left - safe.right;

    /* The order sheet is the phone's. Wider, the ticket has a column of its own, so a resize out
       of the phone's shape closes the sheet rather than declare the ticket's ids twice. */
    if (!phone)
        st->modalTicket = false;
    shortcuts(st);

    /* The right column widens with the window, up to the point the chart would not gain. */
    float rightW = arm == TR_ARM_DESK ? (float)(int)(usableW * 0.25f) : 272.0f;
    rightW = rightW < 272.0f ? 272.0f : rightW > 380.0f ? 380.0f : rightW;
    const float centerW = usableW - (float)TR_RAIL_W - 1.0f
                        - (st->focusMode ? 0.0f : rightW + 1.0f);
    const int panelH = panel_height(vh, finger ? TR_FINGER_H : TR_ROW_H);
    const int watchRowH = finger ? TR_FINGER_H : 30;

    rcColumn(.id = "root", .bg = s.background, .pt = safe.top,
             .pb = safe.bottom, .pl = safe.left, .pr = safe.right,
             .w = "grow", .h = "grow") {
        top_bar(st, arm, vw);
        hline();
        if (phone) {
            rcBox(.w = "grow", .h = "grow") {
                const int page = phone_page(st, vh < 520.0f);

                if (page == TR_PAGE_WATCHLIST)
                    phone_watchlist(st, vh < 520.0f, finger);
                else if (page == TR_PAGE_CHART)
                    phone_chart(st, usableW, vh < 520.0f, finger);
                else
                    phone_portfolio(st);
            }
            hline();
            phone_tabbar(st, vh < 520.0f);
        } else {
            /* The panes sit on the border color one pixel apart, so every seam is a hairline
               without a single separator element. */
            rcRow(.bg = s.border, .gap = 1, .w = "grow", .h = "grow") {
                tool_rail(st, finger);
                rcColumn(.bg = s.border, .gap = 1, .w = "grow", .h = "grow") {
                    chart_pane(st, centerW, finger, true);
                    if (!st->focusMode)
                        trading_panel(st, arm, centerW, panelH, finger);
                }
                if (!st->focusMode) {
                    rcColumn(.id = "right_col", .bg = s.border, .gap = 1, .h = "grow",
                             .wType = RC_PX(rightW)) {
                        rcColumn(.bg = s.surface, .w = "grow",
                                 .hType = RC_PX(watch_height(watchRowH))) {
                            watchlist(st, rightW, watchRowH);
                        }
                        rcColumn(.id = "ticket", .bg = s.surface, .align = "tl", .scroll = "v",
                                 .w = "grow", .h = "grow") {
                            rcColumn(.id = "ticket_body", .gap = 10, .p = 12, .w = "grow") {
                                order_ticket(st, finger);
                            }
                        }
                        rcScrollbar("ticket");
                    }
                }
            }
        }
    }
    confirm_modal(st, phone);
    search_modal(st, phone);
    settings_modal(st, phone);
    ticket_modal(st, phone);
    toast(st);
}

/* Press an element by asking where it is: the same box the hit test uses, so the script
   survives a layout change and every action runs the real input path. */
static void bench_press(const AppInputSink *in, const char *id) {
    const RC_Box box = rcGetElementBox(id);

    if (box.found) {
        in->move(in->ctx, box.x + box.width * 0.5f, box.y + box.height * 0.5f);
        in->button(in->ctx, APP_MBTN_LEFT, true);
    }
}

/* The scripted scenario: open a symbol, scroll the board, type a quantity, review and confirm an
   order, then hold still, so a double-rendered frame is identical. The hold needs no focused
   input and no hovered control, because a caret blink and a tooltip dwell read a real clock. */
void trader_bench_step(AppState *st, const AppInputSink *in, int frame) {
    if (!in || frame >= TRADER_BENCH_WARMUP)
        return;

    if (frame == 6) {
        bench_press(in, row_id(st, ID_WATCH, 2));        /* NVDA, the watchlist's third row */
    } else if (frame == 7) {
        in->button(in->ctx, APP_MBTN_LEFT, false);
    } else if (frame >= 10 && frame < 16) {
        in->wheel(in->ctx, 0.0f, -2.0f);             /* scroll the board, pointer over it */
    } else if (frame == 20) {
        bench_press(in, "qty_in");                   /* focus the quantity */
    } else if (frame == 21) {
        in->button(in->ctx, APP_MBTN_LEFT, false);
    } else if (frame == 24) {
        in->text(in->ctx, (unsigned int)'5');        /* the ticket's 10 becomes 105 shares */
    } else if (frame == 32) {
        bench_press(in, "btn_review");               /* Review opens the confirm dialog */
    } else if (frame == 33) {
        in->button(in->ctx, APP_MBTN_LEFT, false);
    } else if (frame == 40) {
        bench_press(in, "btn_confirm");              /* Place the order: a fill appears */
    } else if (frame == 41) {
        in->button(in->ctx, APP_MBTN_LEFT, false);
    } else if (frame == TRADER_BENCH_WARMUP - 2) {
        in->move(in->ctx, -100.0f, -100.0f);         /* blur the field, park off-canvas ... */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == TRADER_BENCH_WARMUP - 1) {
        in->button(in->ctx, APP_MBTN_LEFT, false);   /* ... and stay there into the hold */
    }
}
