/*
    app.h - the whole UI: state, the operations on it, and the layout.

    ONE header, no .c beside it: main.c is the only translation unit, so there is
    nothing to link and no forward declarations.

    WHERE THINGS GO. The dataset and the pure logic that scans it are in
    issues.h. A visual CHOICE is in theme.h. What is left is here, in the order
    you would read it: the state struct, the operations that take a pointer to
    it, then the panes, then update and layout.
*/
#ifndef APP_APP_H
#define APP_APP_H

#include "rayclay.h"
#include "app/theme.h"
#include "app/issues.h"
#include "platform/platform.h"

#include "icons/rc_icons_rayclay_logo.h"
#include "icons/rc_icons_panel_left.h"
#include "icons/rc_icons_panel_right.h"
#include "icons/rc_icons_minimize.h"
#include "icons/rc_icons_maximize.h"
#include "icons/rc_icons_shrink.h"   /* the restore glyph, when the window IS maximised */
#include "icons/rc_icons_x.h"

/* The whole app in one struct. `sel` is the permutation: filtering reorders
   32-bit indices, never records, which is why the rail, the list and the detail
   pane cannot disagree about what passed. */
typedef struct {
    Issue   issue[ISSUES];
    int32_t sel[ISSUES];                 /* indices that pass, in list order  */
    /* Where the counting sort places them before they are copied back: it
       cannot run in place without losing the stability the tie-break needs. */
    int32_t order[ISSUES];
    int     count, rebuilds;
    /* THE THREE FACET COUNTS: how many of these there are inside the chosen
       view, which is what makes a zero next to a filter worth reading. */
    int     perView[VIEWS], perPrio[4], perArea[AREAS];
    char    countText[VIEWS][12], prioText[4][12], areaText[AREAS][12];
    int     view, selRow, selIssue, sort;
    /* TWO rail bools, not one. Wide, the rail is a COLUMN beside the content and
       defaults OPEN because there is room. Compact, it is a FILTER SHEET that
       replaces the content and must default CLOSED, or the app opens on a filter
       panel instead of on the issues. */
    bool    prio[4], area[AREAS], railOpen, railSheet, dirty;
    /* "The user ASKED for this issue", which is not "an issue is selected":
       rebuild() always selects a row so the wide layout never shows an empty
       pane, and on a phone that would open the app on an arbitrary issue. */
    bool    detailOpen;
    char    query[QCAP];
} AppState;

static inline RC_String num(RC_Arena *m, int v, const char *suffix)
{
    char b[16];
    cat_grouped(b, (int)sizeof b, 0, (unsigned)(v < 0 ? 0 : v));
    return rcFormat(m, "%s%s", b, suffix);
}

static inline void seed(AppState *st, uint32_t s)
{
    unsigned tenths = 0;              /* how far back the walk has got, so far */
    int i;
    for (i = 0; i < ISSUES; i++) {
        Issue   *it = &st->issue[i];
        unsigned r, hours, days, heat;

        it->num      = (uint32_t)(ISSUES - i) + 1000u;
        it->verb     = (uint16_t)(nxt(&s) % (unsigned)N(ACTION));
        it->noun     = (uint16_t)(nxt(&s) % (unsigned)N(OBJECT));
        it->area     = (uint16_t)(nxt(&s) % (unsigned)N(AREA));
        it->who      = (uint8_t)(nxt(&s) % (unsigned)N(WHO));
        /* THE AGE IS THE FIXTURE'S SPINE, and ACCUMULATED is what matters: the
           age can only ever go up, so the array's order IS the age order and
           "newest first" is a claim the list keeps for free. */
        tenths      += nxt(&s) % (unsigned)INTAKE_GAP;
        hours        = 1u + tenths / 10u;
        days         = hours / 24u;
        it->age      = (uint16_t)hours;
        /* Skewed, as a real backlog is: a uniform draw puts 25,000 fires in the
           inbox and makes the priority filter meaningless. */
        r = nxt(&s) % 100u;
        it->prio  = (uint8_t)(r < 3u ? 0 : r < 15u ? 1 : r < 55u ? 2 : 3);
        /* A queue that is WORKED closes its old work, so the chance of still
           being open falls with age - which is what makes the inbox read as this
           month's problems with a tail behind it. */
        r = days < 2u ? 96u : days < 7u ? 87u : days < 30u ? 60u :
            days < 120u ? 32u : 11u;
        it->open  = (uint8_t)(nxt(&s) % 100u < r);
        /* Attention follows severity LOOSELY, and the looseness is what earns
           the fourth sort its place: a count that is a FUNCTION of the priority
           makes "most discussed" a second copy of "priority". Capped inside
           COMMENT_BINS, so that sort stays one counting pass. */
        heat = (3u - (unsigned)it->prio) * 3u + days / 240u + nxt(&s) % 24u;
        if (nxt(&s) % 9u == 0u) heat += 12u;
        if (heat > (unsigned)COMMENT_BINS - 2u) heat = (unsigned)COMMENT_BINS - 2u;
        it->comments = (uint16_t)heat;
        it->flags = (uint8_t)((nxt(&s) % 100u < 9u ? F_MINE : 0u) |
                              (nxt(&s) % 100u < 6u ? F_MENTION : 0u));
    }
}

/* THE LIST'S ORDER, IN ONE PLACE. Two of the four sorts cost nothing, because
   the age is monotone in the array index. The other two are STABLE COUNTING
   sorts over a bounded key: O(count), no comparison function, and stability is
   what keeps newest-first as the tie-break inside every other order. It runs on
   a CHANGE, like the scan it follows, never on a frame. */
static inline void sort_rows(AppState *st)
{
    int start[COMMENT_BINS];
    int bins = sort_bins(st->sort);
    int i, j, at, n;

    if (st->sort == S_NEWEST) return;
    if (st->sort == S_OLDEST) {
        for (i = 0, j = st->count - 1; i < j; i++, j--) {
            int32_t t  = st->sel[i];
            st->sel[i] = st->sel[j];
            st->sel[j] = t;
        }
        return;
    }
    for (i = 0; i < bins; i++) start[i] = 0;
    for (i = 0; i < st->count; i++) start[sort_key(&st->issue[st->sel[i]], st->sort)]++;
    for (i = 0, at = 0; i < bins; i++) { n = start[i]; start[i] = at; at += n; }
    for (i = 0; i < st->count; i++) {
        int k = sort_key(&st->issue[st->sel[i]], st->sort);
        st->order[start[k]++] = st->sel[i];
    }
    for (i = 0; i < st->count; i++) st->sel[i] = st->order[i];
}

/* The ONE O(issues) pass, and it runs on a keystroke - never on a frame. It
   carries the selection across BY IDENTITY, so the row you were reading keeps
   its highlight if it survives the new filter.
   AN EMPTY FACET MEANS "ALL OF IT", which is why the predicates below test
   `any...` first rather than an all-false table. */
static inline void rebuild(AppState *st)
{
    char q[QCAP], t[96];
    bool anyPrio = false, anyArea = false;
    int  i, v;

    for (i = 0; i < 4; i++)     { anyPrio |= st->prio[i]; st->perPrio[i] = 0; }
    for (i = 0; i < AREAS; i++) { anyArea |= st->area[i]; st->perArea[i] = 0; }
    for (i = 0; i < QCAP - 1 && st->query[i]; i++) q[i] = lower(st->query[i]);
    q[i] = '\0';
    for (v = 0; v < VIEWS; v++) st->perView[v] = 0;

    st->count = 0;
    for (i = 0; i < ISSUES; i++) {
        const Issue *it = &st->issue[i];

        for (v = 0; v < VIEWS; v++) if (in_view(it, v)) st->perView[v]++;
        if (!in_view(it, st->view)) continue;
        /* Counted before the facets narrow anything, so each row states what it
           would bring in rather than what is left after it was excluded. */
        st->perPrio[it->prio]++;
        st->perArea[it->area]++;
        if (anyPrio && !st->prio[it->prio]) continue;
        if (anyArea && !st->area[it->area]) continue;
        if (q[0]) {
            title_of(it, t, (int)sizeof t);
            if (!has(t, q) && !has(WHO[it->who], q) && !has(AREA[it->area], q)) continue;
        }
        st->sel[st->count++] = (int32_t)i;
    }
    sort_rows(st);
    st->rebuilds++;
    for (v = 0; v < VIEWS; v++) cat_grouped(st->countText[v], 12, 0, (unsigned)st->perView[v]);
    for (i = 0; i < 4; i++)     cat_grouped(st->prioText[i], 12, 0, (unsigned)st->perPrio[i]);
    for (i = 0; i < AREAS; i++) cat_grouped(st->areaText[i], 12, 0, (unsigned)st->perArea[i]);

    /* AFTER the sort, never before it: selRow is a position in the list and the
       sort is what decides positions. */
    st->selRow = -1;
    for (i = 0; st->selIssue >= 0 && i < st->count; i++)
        if (st->sel[i] == st->selIssue) { st->selRow = i; break; }
    if (st->selRow < 0) {
        st->selRow   = st->count ? 0 : -1;
        st->selIssue = st->count ? (int)st->sel[0] : -1;
    }
    st->dirty = false;
}

/* THE ONE BREAKPOINT, DERIVED FROM THIS APP'S OWN PANES rather than from a table
   of device widths: the rail, the detail pane, the two rules between them, and a
   list row that stops being legible below about 328 px.

     228 + 1 + 328 + 1 + 360 = 918

   Below that the three panes cannot sit side by side, so they TAKE TURNS: rail,
   or list, or detail - never a squeezed version of all three. No second
   breakpoint on purpose: a stage that dropped the rail would leave its toggle
   controlling nothing.
   Compared against `rcViewport().width` - the space the layout actually has - so
   a desktop window dragged narrow gets the compact arm for the same reason a
   phone does. Never against rcGetWindowDimensions() divided by zoom: right under
   RC_ZOOM_LAYOUT, wrong under RC_ZOOM_OPTICAL. */
#define INBOX_THREE_PANE_W 918.0f

/* THE LIST ROW HAS TWO SHAPES, AND THE LIST'S OWN WIDTH PICKS ONE. Nothing is
   hidden by it: the same seven things are in the row either way. Below about
   670 px the title would have to wrap, and a wrapped title in a fixed-pitch
   virtual row does not get taller - it draws over the row beneath. So a narrower
   list takes the two-line row and the taller pitch that goes with it.
   The width is the LIST's, not the window's, and it is derived from the same
   numbers layout() places the panes with rather than read back from the last
   frame: this app is RC_RENDER_ON_DEMAND, and a stale width may be the last
   thing drawn until the next input. */
#define INBOX_ONE_LINE_ROW_W 670.0f

static inline float list_width(const AppState *st)
{
    RC_Viewport v = rcViewport();
    float       w = v.width - v.safe.left - v.safe.right;

    if (v.width >= INBOX_THREE_PANE_W)
        w -= (st->railOpen ? (float)RAIL_W + 1.0f : 0.0f) + 1.0f + (float)DETAIL_W;
    return w;
}

/* One reader, so the pane that draws the rows and the operation that scrolls to
   one cannot disagree about the pitch. */
static inline int row_pitch(const AppState *st)
{
    return list_width(st) >= INBOX_ONE_LINE_ROW_W ? ROW_H : ROW_H_TALL;
}

/* Keep the keyboard selection on screen, and with a virtual list that is not
   cosmetic: only the visible rows are declared at all, so a selection that walks
   out of the window is drawn by nothing while the detail pane keeps changing for
   a row the user cannot see. The viewport height falls out of the travel:
   maxOffsetY is content minus viewport. */
static inline void reveal_selected(AppState *st)
{
    RC_ScrollInfo si = rcGetScrollInfo("list");
    float contentH, viewH, top, bottom;
    float pitch = (float)row_pitch(st);

    if (!si.found || st->selRow < 0) return;

    contentH = (float)st->count * pitch;
    viewH    = contentH - si.maxOffsetY;
    top      = (float)st->selRow * pitch;
    bottom   = top + pitch;

    /* Minimum movement, so a row that is already visible never lurches the list.
       rcScrollBy is positive-DOWN, like the DOM's element.scrollBy. */
    if (top < si.offsetY)
        rcScrollBy("list", 0.0f, top - si.offsetY);
    else if (bottom > si.offsetY + viewH)
        rcScrollBy("list", 0.0f, bottom - (si.offsetY + viewH));
}

static inline void move(AppState *st, int step)
{
    int n = st->selRow < 0 ? 0 : st->selRow + step;
    if (!st->count) return;
    st->selRow   = n < 0 ? 0 : n > st->count - 1 ? st->count - 1 : n;
    st->selIssue   = (int)st->sel[st->selRow];
    st->detailOpen = true;      /* a keyboard move is the user asking, same as a click */
    reveal_selected(st);
}
/* Severity's ink, out of theme.h's table. NOT rcGetStyle()'s danger / warning,
   which are button fills and also what a destructive control uses. */
static inline RC_Color prio_color(int p)
{
    return PRIO_INK[p < 0 ? 0 : p > 3 ? 3 : p];
}

/* The priority badge. THE SHAPE CARRIES THE TIER, not the hue alone: filled for
   a fire and a this-week, hollow for the normal case, bare text for the bottom
   tier - because four fifths of a real backlog is P2 or P3, and two greys of the
   same weight are one column of noise. Fill and border are declared in every
   state, transparent where absent, so no chip changes size with its tier. */
static inline void prio_chip(int p)
{
    RC_Color ink  = prio_color(p);
    RC_Color fill = p <= 1 ? rcAlpha(ink, 45) : RC_TRANSPARENT;
    RC_Color edge = p == 2 ? rcAlpha(ink, 110) : RC_TRANSPARENT;

    rcBox(.bg = fill, .px = 6, .py = 2, .align = "cc", .borderRadius = "all-sm",
          .border = { .color = edge, .width = "all-1" }) {
        rcTextC(PRIO[p], .font = F_MICRO, .color = ink);
    }
}

/* The area pill, inked from AREA_INK. A category on a hundred rows in one grey
   is texture without information; the name beside the hue is what keeps it
   legible to a reader who cannot separate two of them. */
static inline void area_pill(int a)
{
    RC_Color ink = AREA_INK[a < 0 ? 0 : a >= AREAS ? AREAS - 1 : a];

    rcBox(.bg = rcAlpha(ink, 32), .px = 7, .py = 2, .align = "cc",
          .borderRadius = "all-full") {
        rcTextC(AREA[a], .font = F_MICRO, .color = ink);
    }
}

/* ---- chrome. .titlebar.custom means the runner draws nothing and this band
   IS the titlebar; RC_ID_WINDOW_DRAG moves the window, and an interactive
   child opts back out (the desktop twin of -webkit-app-region: no-drag). --- */
static inline void titlebar(RC_App *app, AppState *st)
{
    RC_Style   s         = rcGetStyle();
    const bool wide      = rcViewport().width >= INBOX_THREE_PANE_W;
    const bool railShown = wide ? st->railOpen : st->railSheet;
    const bool over      = rcIsHovered("panel");
    RC_Color   tint      = over ? s.text : s.textMuted;

    /* Chrome, not content: RC_AppOptions.titlebarHeight freezes the OS drag
       strip in physical px, so rcUnzoomed() keeps the drawn band matching the
       strip the OS lets you drag at any content zoom. */
    rcUnzoomed() {
        rcRow(.id = RC_ID_WINDOW_DRAG, .bg = s.chrome, .gap = 10, .px = 12,
              .align = "cl", .w = "grow", .hType = RC_PX(BAR_H)) {
            /* THE PANEL TOGGLE BELONGS ON THE LEFT, beside the pane it opens and
               well away from the window buttons - which is where every app with
               a side rail puts it. On the one-pane arm it is the app's ONLY route
               to the filters, so it also carries a resting fill: an icon with no
               background on a titlebar reads as window chrome, and a user who
               takes it for one never finds the views.
               30 x 26 under a mouse, HIT_MIN square where a finger has to hit it;
               the band is 46 tall, so 44 sits inside it with a pixel to spare. */
            rcRow(.id = RC_ID_WINDOW_NODRAG, .align = "cc") {
                rcBox(.id = "panel",
                      .bg = rcAlpha(s.border, over ? 120 : (wide ? 0 : 70)),
                      .align = "cc", .borderRadius = "all-md",
                      .wType = RC_PX(wide ? 30 : HIT_MIN), .hType = RC_PX(wide ? 26 : HIT_MIN),
                      .tooltip = railShown ? "Hide views and filters"
                                           : "Show views and filters") {
                    if (railShown) rcIconPanelLeft(15.0f, tint); else rcIconPanelRight(15.0f, tint);
                }
                /* ONE control, driving whichever rail this width has. */
                if (rcClicked("panel")) {
                    if (wide)
                        st->railOpen = !st->railOpen;
                    else
                        st->railSheet = !st->railSheet;
                }
            }
            rcIconRayClayLogo(22.0f);
            /* The open count is printed beside the thing it counts - the rail's
               Inbox row and the list's own header - so the band does not say it a
               third time. One pane wide there is no rail, so it takes the
               subtitle's place instead of disappearing. */
            rcColumn(.gap = 0) {
                rcTextL("Inbox", .font = F_BODY, .color = s.text);
                if (wide)
                    rcTextL("RayClay " RC_VERSION " \xc2\xb7 one source, every target",
                             .font = F_MICRO, .color = s.textMuted);
                else
                    rcText(num(rcAppArena(app), st->perView[V_INBOX], " open"),
                            .font = F_MICRO, .color = s.textMuted, .wrap = "n");
            }
            rcBox(.w = "grow") {}
            rcRow(.gap = 2, .align = "cc") {
                rcWindowControlButton(RC_WINCTL_MINIMIZE, rcIconMinimize, 14.0f);
                /* One id, two actions, so the glyph has to say which.
                   rcIsWindowMaximized answers false where nothing maximises. */
                rcWindowControlButton(RC_WINCTL_MAXIMIZE,
                                      rcIsWindowMaximized() ? rcIconShrink : rcIconMaximize,
                                      14.0f);
                rcWindowControlButton(RC_WINCTL_CLOSE,    rcIconX,        14.0f);
            }
        }
    }
}

/* ONE FACET ROW, used by all three lists in the rail: a swatch, a label, and the
   count of what it would bring in. The whole row is the target. */
static inline bool facet_row(const char *id, RC_Color swatch, const char *label,
                            const char *count, bool on)
{
    RC_Style s  = rcGetStyle();
    RC_Color fg = on ? s.text : s.textMuted;

    rcRow(.id = id,
          .bg = on ? rcAlpha(s.primary, 60) : rcAlpha(s.border, rcIsHovered(id) ? 90 : 0),
          .gap = 9, .px = 10, .align = "cl", .borderRadius = "all-md",
          .w = "grow", .hType = RC_PX(FACET_ROW_H)) {
        rcBox(.bg = swatch, .borderRadius = "all-full", .w = "9px", .h = "9px") {}
        rcTextC(label, .font = F_SMALL, .color = fg);
        rcBox(.w = "grow") {}
        rcTextC(count, .font = F_MICRO, .color = s.textMuted);
    }
    return rcClicked(id);
}

static inline void facet_label(const char *text)
{
    rcBox(.pt = 6, .pb = 2, .w = "grow") {
        rcTextC(text, .font = F_MICRO, .color = rcGetStyle().textMuted);
    }
}

/* THE RAIL IS THE FILTER SURFACE, and every facet in it is one the list actually
   draws: the view, the priority chip on every row, and the area pill beside it.
   An attribute that is painted on a hundred rows and filterable from nowhere is
   decoration, so all three are here.
   VIEW is single-select and PRIORITY and AREA are multi-select, which is the
   ordinary distinction between "which queue" and "narrow it"; an empty facet
   means all of it, so the app opens on everything. */
static inline void rail(RC_App *app, AppState *st, bool alone)
{
    static const char *const ID[VIEWS] = { "v0", "v1", "v2", "v3" };
    static const char *const PID[4]    = { "p0", "p1", "p2", "p3" };
    static const char *const AID[AREAS] = { "a0", "a1", "a2", "a3",
                                            "a4", "a5", "a6", "a7" };
    RC_Style s = rcGetStyle();
    char     total[16];
    int      i;

    /* RAIL_W is what makes THREE panes fit, so the only pane grows instead. */
    rcColumn(.bg = s.surface, .h = "grow",
             .wType = alone ? RC_GROW : RC_PX(RAIL_W)) {
        /* The facets SCROLL and the footer does not, so a short window loses
           nothing. */
        rcColumn(.id = "rail", .gap = 2, .px = 12, .py = 6, .scroll = "v",
                 .w = "grow", .h = "grow") {
            facet_label("VIEWS");
            for (i = 0; i < VIEWS; i++) {
                if (facet_row(ID[i], st->view == i ? s.primary : s.border, VIEW[i],
                              st->countText[i], st->view == i) && st->view != i) {
                    st->view  = i;
                    st->dirty = true;
                }
            }
            facet_label("PRIORITY");
            for (i = 0; i < 4; i++) {
                if (facet_row(PID[i], prio_color(i), PRIO[i], st->prioText[i], st->prio[i])) {
                    st->prio[i] = !st->prio[i];
                    st->dirty   = true;
                }
            }
            facet_label("AREA");
            for (i = 0; i < AREAS; i++) {
                if (facet_row(AID[i], AREA_INK[i], AREA[i], st->areaText[i], st->area[i])) {
                    st->area[i] = !st->area[i];
                    st->dirty   = true;
                }
            }
        }
        rcBox(.bg = s.border, .w = "grow", .hType = RC_PX(1)) {}
        rcColumn(.gap = 2, .px = 12, .py = 10, .w = "grow") {
            cat_grouped(total, (int)sizeof total, 0, (unsigned)ISSUES);
            rcText(rcFormat(rcAppArena(app), "%s issues \xc2\xb7 rebuild #%d", total,
                             st->rebuilds), .font = F_MICRO, .color = s.textMuted);
            /* STATE THE PROPERTY, DO NOT INSTRUCT THE READER: the counter above
               is the evidence, and a window drag never moves it. */
            rcTextL("the list rebuilds only when a filter changes", .font = F_MICRO,
                     .color = s.textMuted);
        }
    }
}

/* One undo per filter the strip can show, so "Clear" leaves none of them on. */
static inline void clear_filters(AppState *st)
{
    int i;

    for (i = 0; i < 4; i++)     st->prio[i] = false;
    for (i = 0; i < AREAS; i++) st->area[i] = false;
    st->query[0] = '\0';
    st->dirty    = true;
}

/* A chip in the applied-filter strip: what the list is filtered TO. A statement
   and not a control, so it carries no id and claims no click. */
static inline void filter_chip(RC_String text, RC_Color ink)
{
    RC_Style s = rcGetStyle();

    rcBox(.bg = rcAlpha(s.border, 90), .px = 8, .py = 3, .align = "cc",
          .borderRadius = "all-full") {
        rcText(text, .font = F_MICRO, .color = ink, .wrap = "n");
    }
}

/* THE STRIP THAT SAYS WHAT THIS LIST IS. Four controls can narrow 100,000 issues
   down to a dozen, and on a narrow window the rail is behind a sheet - so without
   this line a list left on P0 yesterday reads as an app with nothing in it. It
   carries the one action a statement of state owes the reader: undo it. The sort
   control sits at the other end, because "newest first" is the other half of the
   sentence "what am I looking at". */
static inline void filters(RC_App *app, AppState *st, bool roomy)
{
    RC_Style  s     = rcGetStyle();
    RC_Arena *m     = rcAppArena(app);
    char      chosen[32], seen[24];
    int       i, at = 0, cut, first = -1, firstArea = -1, areaN = 0;
    RC_String areaText;

    for (i = 0; i < 4; i++) {
        if (!st->prio[i]) continue;
        if (first >= 0) at = cat(chosen, (int)sizeof chosen, at, ", ");
        else            first = i;
        at = cat(chosen, (int)sizeof chosen, at, PRIO[i]);
    }
    /* Eight area names joined would outrun the strip, so the chip names the
       first and counts the rest. */
    for (i = 0; i < AREAS; i++) {
        if (!st->area[i]) continue;
        if (firstArea < 0) firstArea = i;
        areaN++;
    }
    areaText = firstArea < 0 ? rcStringFromCStr("")
             : areaN == 1    ? rcStringFromCStr(AREA[firstArea])
                             : rcFormat(m, "%s +%d", AREA[firstArea], areaN - 1);

    /* ONE SENTENCE, ON ONE LINE OR TWO - never half a sentence. The strip cannot
       hold the labels, the chips, the clear verb and the combo inside a phone's
       width, so narrow puts the sort half on its own line rather than dropping a
       label: no datum should be reachable only in landscape. */
    if (!roomy) {
        rcColumn(.bg = s.surface, .gap = 6, .pb = 10, .px = 14, .w = "grow") {
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                rcTextL("Showing", .font = F_MICRO, .color = s.textMuted);
                filter_chip(rcStringFromCStr(VIEW[st->view]), s.text);
                if (first >= 0)
                    filter_chip(rcFormat(m, "%s", chosen), prio_color(first));
                if (firstArea >= 0)
                    filter_chip(areaText, AREA_INK[firstArea]);
                rcBox(.w = "grow") {}
                if (first >= 0 || firstArea >= 0 || st->query[0]) {
                    bool over = rcIsHovered("clear");

                    rcBox(.id = "clear", .bg = rcAlpha(s.border, over ? 120 : 0),
                          .px = 8, .py = 3, .align = "cc", .borderRadius = "all-full") {
                        rcTextL("Clear", .font = F_MICRO,
                                 .color = over ? s.text : s.textMuted);
                    }
                    if (rcClicked("clear")) clear_filters(st);
                }
            }
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                rcTextL("Sort", .font = F_MICRO, .color = s.textMuted);
                rcBox(.w = "grow") {
                    if (rcCombo("sort", &st->sort, SORT, SORTS)) st->dirty = true;
                }
            }
        }
        return;
    }

    rcRow(.bg = s.surface, .gap = 8, .pb = 10, .px = 14, .align = "cl", .w = "grow") {
        rcTextL("Showing", .font = F_MICRO, .color = s.textMuted);
        filter_chip(rcStringFromCStr(VIEW[st->view]), s.text);
        /* Named in full and inked with the most urgent: one chip per priority
           would grow the strip past the width a phone has. */
        if (first >= 0)
            filter_chip(rcFormat(m, "%s", chosen), prio_color(first));
        if (firstArea >= 0)
            filter_chip(areaText, AREA_INK[firstArea]);
        /* The query chip only where there is room: narrow, the field it came
           from is 30 px above. Three ASCII dots, because the bundled face bakes
           Latin-1 and U+2026 draws as a missing glyph. */
        if (st->query[0]) {
            cut = cat(seen, (int)sizeof seen, 0, st->query);
            if (st->query[cut]) cat(seen, (int)sizeof seen, cut - 3, "...");
            filter_chip(rcFormat(m, "\"%s\"", seen), s.text);
        }
        rcBox(.w = "grow") {}
        if (first >= 0 || firstArea >= 0 || st->query[0]) {
            bool over = rcIsHovered("clear");

            /* rcClicked is what marks this clickable, and the hand cursor
               follows from that. */
            rcBox(.id = "clear", .bg = rcAlpha(s.border, over ? 120 : 0), .px = 8, .py = 3,
                  .align = "cc", .borderRadius = "all-full") {
                rcTextL("Clear filters", .font = F_MICRO,
                         .color = over ? s.text : s.textMuted);
            }
            if (rcClicked("clear")) clear_filters(st);
        }
        rcTextL("Sort", .font = F_MICRO, .color = s.textMuted);
        /* rcCombo is width-GROW, so the width is the caller's to decide. */
        rcBox(.w = "148px") {
            if (rcCombo("sort", &st->sort, SORT, SORTS)) st->dirty = true;
        }
    }
}

/* ---- the list. 100,000 rows, about thirty declared. ---------------------- */
static inline void list(RC_App *app, AppState *st)
{
    RC_Style   s     = rcGetStyle();
    RC_Arena  *m     = rcAppArena(app);
    const bool roomy = list_width(st) >= INBOX_ONE_LINE_ROW_W;

    rcRow(.bg = s.surface, .gap = 10, .px = 14, .align = "cl", .w = "grow",
          .hType = RC_PX(52)) {
        rcBox(.w = "grow") {
            /* The bundled face bakes ASCII and Latin-1, so an ellipsis draws as
               a replacement glyph: widen the range or stay inside it. The library
               logs each DISTINCT codepoint past the cap once, by name. */
            if (rcTextInput("q", st->query, sizeof st->query,
                             .placeholder = "Filter 100,000 issues...", .font = F_SMALL))
                st->dirty = true;
        }
        if (st->query[0]) {
            rcBox(.id = "clr", .bg = rcAlpha(s.border, rcIsHovered("clr") ? 120 : 0),
                  .align = "cc", .borderRadius = "all-md", .w = "26px", .h = "26px") {
                rcIconX(13.0f, rcIsHovered("clr") ? s.text : s.textMuted);
            }
            if (rcClicked("clr")) { st->query[0] = '\0'; st->dirty = true; }
        }
        rcText(num(m, st->count, " shown"), .font = F_SMALL, .color = s.textMuted);
    }
    filters(app, st, roomy);
    rcBox(.bg = s.border, .w = "grow", .hType = RC_PX(1)) {}

    rcColumn(.id = "list", .bg = s.background, .scroll = "v", .w = "grow", .h = "grow") {
        if (!st->count)
            rcColumn(.gap = 6, .p = 40, .align = "tc", .w = "grow") {
                rcTextL("Nothing matches that filter.", .font = F_BODY, .color = s.textMuted);
            }
        /* The pitch is read ONCE per frame, outside the loop: rcVirtualList
           sizes its spacers from it, and every row must really be that tall. */
        const int  pitch   = row_pitch(st);
        const bool twoLine = !roomy;

        rcVirtualList(row, "list", st->count, (float)pitch) {
            const Issue *it = &st->issue[st->sel[row.index]];
            /* rcFormat's result is NUL-terminated, so .chars is legal as an id;
               arena memory lasts this frame, and an id is hashed as it is used. */
            const char  *id    = rcFormat(m, "r%d", row.index).chars;
            bool         on    = row.index == st->selRow;
            RC_String    title = rcFormat(m, "%s in %s", ACTION[it->verb], OBJECT[it->noun]);
            RC_String    numS  = rcFormat(m, "#%u", it->num);
            RC_String    who   = rcFormat(m, "@%s", WHO[it->who]);
            /* The key the fourth sort orders BY, on the row it orders. The
               column holds its width even where there is no thread, so the
               columns stay in line. */
            RC_String    talk  = rcFormat(m, "%u %s", (unsigned)it->comments,
                                           it->comments == 1u ? "reply" : "replies");
            Age          a     = age_of((unsigned)it->age);
            /* One letter of unit, so the column is three characters wide at
               any age. */
            RC_String    age   = rcFormat(m, "%u%s", a.value, a.unit);
            RC_Color     fg    = it->open ? s.text : s.textMuted;

            rcRow(.id = id, .bg = on ? rcAlpha(s.primary, 70)
                            : rcAlpha(s.border, rcIsHovered(id) ? 80 : (row.index & 1) ? 24 : 0),
                  .gap = 10, .pl = 11, .pr = 14, .align = "cl",
                  .w = "grow", .hType = RC_PX(pitch)) {
                /* SELECTION GETS A SECOND CHANNEL. A tinted fill is a hue
                   difference, which a colour-vision deficiency, a glare-washed
                   screen and a greyscale print all lose; the 3 px rule is a
                   POSITION difference and survives all three. The row's left
                   padding drops 14 -> 11 so the rule plus its gap restores the
                   text inset and no column shifts when a row is picked. */
                rcBox(.bg = on ? s.primary : RC_TRANSPARENT, .borderRadius = "all-full",
                      .w = "3px", .h = "18px") {}
                /* .wrap = "n" in BOTH shapes: a fixed-pitch row has no second
                   line to give, and the full title is one tap away. */
                if (twoLine) {
                    /* The narrow shape: the chip, the title and the age share
                       the FIRST line, and the meta sits indented under it. */
                    rcColumn(.gap = 3, .w = "grow") {
                        rcRow(.gap = 10, .align = "cl", .w = "grow") {
                            prio_chip(it->prio);
                            rcText(title, .font = F_SMALL, .color = fg, .wrap = "n");
                            rcBox(.w = "grow") {}
                            rcText(age, .font = F_MICRO, .color = s.textMuted);
                        }
                        rcRow(.gap = 8, .pl = 40, .align = "cl", .w = "grow") {
                            rcText(numS, .font = F_MICRO, .color = s.textMuted);
                            area_pill(it->area);
                            rcText(who, .font = F_MICRO, .color = s.textMuted);
                            if (it->comments)
                                rcText(talk, .font = F_MICRO, .color = s.textMuted,
                                        .wrap = "n");
                        }
                    }
                } else {
                    prio_chip(it->prio);
                    rcText(numS, .font = F_MICRO, .color = s.textMuted);
                    rcText(title, .font = F_SMALL, .color = fg, .wrap = "n");
                    rcBox(.w = "grow") {}
                    area_pill(it->area);
                    rcBox(.w = "62px") {
                        if (it->comments)
                            rcText(talk, .font = F_MICRO, .color = s.textMuted, .wrap = "n");
                    }
                    rcBox(.w = "84px") { rcText(who, .font = F_MICRO, .color = s.textMuted); }
                    rcBox(.w = "46px") { rcText(age, .font = F_MICRO, .color = s.textMuted); }
                }
            }
            if (rcClicked(id)) { st->selRow = row.index; st->selIssue = (int)st->sel[row.index];
                                 st->detailOpen = true; }
        }
    }
}

static inline void field(const char *label, RC_String value, RC_Color fg)
{
    RC_Style s = rcGetStyle();
    rcRow(.gap = 10, .align = "cl", .w = "grow") {
        rcBox(.w = "92px") { rcTextC(label, .font = F_MICRO, .color = s.textMuted); }
        rcText(value, .font = F_SMALL, .color = fg);
    }
}

/* ONE REPLY: avatar, name, when, then the line, because a thread is scanned by
   WHO and WHEN and only then read. The avatar is an INITIAL ON A TINTED DISC
   derived from the name, so one person is one colour down the thread. */
/* The 26 initials as LITERALS. A text run BORROWS its characters and draws them
   at the end of the frame, so a `char[2]` built on the stack here compiles, runs
   and paints whatever is at that address by then. A table of literals cannot
   have that bug and needs no arena. */
static const char *const INITIAL[26] = {
    "A","B","C","D","E","F","G","H","I","J","K","L","M",
    "N","O","P","Q","R","S","T","U","V","W","X","Y","Z" };

static inline void reply_row(RC_Arena *m, const Reply *r)
{
    RC_Style    s   = rcGetStyle();
    Age         ago = age_of(r->hoursAgo);
    /* The clamp is belt and braces: a name added later cannot index off the end. */
    unsigned    li  = (unsigned)(r->who[0] - 'a');
    const char *cap = INITIAL[li < 26u ? li : 0u];

    rcRow(.gap = 10, .align = "tl", .w = "grow") {
        rcBox(.bg = rcAlpha(prio_color((int)((unsigned char)r->who[0] % 4u)), 60),
              .align = "cc", .borderRadius = "all-full", .w = "22px", .h = "22px") {
            rcTextC(cap, .font = F_MICRO, .color = s.text);
        }
        rcColumn(.gap = 3, .w = "grow") {
            rcRow(.gap = 6, .align = "cl", .w = "grow") {
                rcTextC(r->who, .font = F_MICRO, .color = s.text);
                /* An hour is the smallest unit here, so everything inside the
                   first one rounds to zero - and "0h ago" reads as a stopped
                   clock rather than as recent. */
                if (ago.value == 0u)
                    rcTextL("just now", .font = F_MICRO, .color = s.textMuted);
                else
                    rcText(rcFormat(m, "%u%s ago", ago.value, ago.unit), .font = F_MICRO,
                            .color = s.textMuted);
            }
            rcTextC(r->text, .font = F_SMALL, .color = s.textMuted);
        }
    }
}

/* THE ACTIVITY THREAD, because a count is not a conversation. It shows the LAST
   few, since those decide what happens next, and the ones it does not show are
   COUNTED rather than silently dropped. */
static inline void thread(RC_Arena *m, const Issue *it)
{
    RC_Style  s      = rcGetStyle();
    const int total  = (int)it->comments;
    const int shown  = total < DETAIL_REPLIES ? total : DETAIL_REPLIES;
    const int first  = total - shown;
    int       k;

    rcBox(.bg = s.border, .w = "grow", .hType = RC_PX(1)) {}
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        rcTextL("ACTIVITY", .font = F_MICRO, .color = s.textMuted);
        rcBox(.w = "grow") {}
        if (first > 0)
            rcText(rcFormat(m, "%d earlier", first), .font = F_MICRO, .color = s.textMuted);
    }
    if (total == 0) {
        rcTextL("No replies yet.", .font = F_SMALL, .color = s.textMuted);
        return;
    }
    for (k = first; k < total; k++) {
        Reply r = reply_of(it, k);
        reply_row(m, &r);
    }
}

/* THE ONE MUTATION, and its button has two shapes: rcButton sets its height from
   the default face, which a mouse hits and a finger does not, so the one-pane arm
   draws a HIT_MIN-tall box in the widget's own colours instead. */
static inline void close_button(AppState *st, Issue *it, bool onePane)
{
    RC_Style    s     = rcGetStyle();
    const char *label = it->open ? "Close issue" : "Reopen";
    bool        hit;

    if (onePane) {
        bool     over = rcIsHovered("act");
        RC_Color fill = it->open ? (over ? s.primaryHover : s.primary)
                                 : (over ? s.border : s.surfaceAlt);

        rcBox(.id = "act", .bg = fill, .align = "cc", .borderRadius = "all-md", .w = "grow",
              .hType = RC_PX(HIT_MIN)) {
            rcTextC(label, .font = F_BODY, .color = it->open ? RC_WHITE : s.text);
        }
        hit = rcClicked("act");
    } else {
        hit = rcButton("act", label, it->open ? RC_BTN_PRIMARY : RC_BTN_DEFAULT);
    }
    if (hit) {
        it->open  = (uint8_t)!it->open;
        st->dirty = true;
    }
}

/* Do not `return` early out of a container body: the brace body is a
   macro-generated loop, so a return skips the close and the frame is wrong.
   The empty state is an else branch. */
static inline void detail(RC_App *app, AppState *st, bool onePane)
{
    RC_Style  s = rcGetStyle();
    RC_Arena *m = rcAppArena(app);
    Issue    *it;
    Age       detailAge;

    /* One pane at a time means the detail is the WHOLE window. */
    rcColumn(.id = "detail", .bg = s.surface, .gap = 12, .p = 18, .scroll = "v",
             .h = "grow", .wType = onePane ? RC_GROW : RC_PX(DETAIL_W)) {
        /* THE ONLY CONTROL THE COMPACT ARM ADDS, because nothing else here means
           "back". Same glyph the titlebar uses for "show the pane to my left". */
        if (onePane) {
            rcRow(.id = "back", .gap = 6, .align = "cl", .w = "grow",
                  .hType = RC_PX(HIT_MIN)) {
                rcIconPanelLeft(15.0f, s.textMuted);
                rcTextL("All issues", .font = F_SMALL, .color = s.textMuted);
            }
            if (rcClicked("back")) {
                /* selIssue is deliberately LEFT ALONE: closing the detail is a
                   VIEW change, not a selection change. */
                st->detailOpen = false;
            }
            rcBox(.bg = s.border, .w = "grow", .hType = RC_PX(1)) {}
        }
        if (st->selIssue < 0) {
            rcTextL("No issue selected.", .font = F_BODY, .color = s.textMuted);
        } else {
            it = &st->issue[st->selIssue];
            detailAge = age_of((unsigned)it->age);
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                rcBox(.bg = it->open ? rcAlpha(s.success, 70) : rcAlpha(s.border, 120),
                      .px = 8, .py = 3, .borderRadius = "all-full") {
                    rcTextC(it->open ? "Open" : "Closed", .font = F_MICRO,
                             .color = it->open ? s.success : s.textMuted);
                }
                rcText(rcFormat(m, "#%u", it->num), .font = F_MICRO, .color = s.textMuted);
            }
            rcText(rcFormat(m, "%s in %s", ACTION[it->verb], OBJECT[it->noun]),
                    .font = F_TITLE, .color = s.text);
            rcBox(.bg = s.border, .w = "grow", .hType = RC_PX(1)) {}
            field("Priority", rcStringFromCStr(PRIO[it->prio]), prio_color(it->prio));
            field("Area",     rcStringFromCStr(AREA[it->area]), s.text);
            field("Assignee", rcStringFromCStr(WHO[it->who]),   s.text);
            /* The plural is the caller's: rcFormat has no rule for it. */
            field("Opened",   rcFormat(m, "%u %s%s ago", detailAge.value, detailAge.word,
                                        detailAge.value == 1u ? "" : "s"), s.text);
            field("Comments", rcFormat(m, "%u", (unsigned)it->comments), s.text);
            rcBox(.bg = s.border, .w = "grow", .hType = RC_PX(1)) {}
            rcText(rcFormat(m, "Reported against %s. Reproduced on desktop and in the browser "
                                "from one source.", AREA[it->area]),
                    .font = F_SMALL, .color = s.textMuted);
            /* The action follows the description with a small gap, not the
               pane's foot, where a grow spacer would strand it. */
            rcBox(.hType = RC_PX(8)) {}
            close_button(st, it, onePane);
            /* The thread goes UNDER the action, which is what this pane is FOR. */
            thread(m, it);
        }
    }
}

static inline void update(RC_App *app, void *userData)
{
    AppState *st = (AppState *)userData;
    (void)app;
    if (rcKeyPressed(RC_KEY_DOWN)) move(st, 1);
    if (rcKeyPressed(RC_KEY_UP))   move(st, -1);
    if (rcKeyPressed(RC_KEY_ESCAPE) && st->query[0]) { st->query[0] = '\0'; st->dirty = true; }
    if (st->dirty) rebuild(st);
}

static inline void layout(RC_App *app, void *userData)
{
    AppState *st = (AppState *)userData;
    RC_Style s = rcGetStyle();

    /* SAFE AREA. A phone draws the window edge to edge, under the status bar and
       the home indicator, and nothing moves content out of the way for you. Spend
       the margins ONCE, here at the root; rcViewport().safe hands them over
       ALREADY IN LAYOUT UNITS, and they are {0,0,0,0} on desktop. */
    RC_Viewport view = rcViewport();
    RC_Insets   safe = view.safe;

    /* DECLARED here and ASSIGNED below, so the scrollbars read the same values
       the panes were built from. They cannot be hoisted above titlebar(), which
       toggles the rail. */
    bool showList = false, showDetail = false, showRail = false;

    rcColumn(.id = "root", .bg = s.background, .pt = (uint16_t)(safe.top),
             .pb = (uint16_t)(safe.bottom), .pl = (uint16_t)(safe.left),
             .pr = (uint16_t)(safe.right), .w = "grow", .h = "grow") {
        titlebar(app, st);
        /* Pane rules are 1 px boxes: RC_Border takes ONE all-sides width, so "a
           line down this edge only" is a sibling, not a property. Branch on the
           SPACE, never on the platform. */
        const bool wide       = view.width >= INBOX_THREE_PANE_W;
        const bool onePane    = !wide;
        /* Narrow, the panes take turns: the rail when it is open, then the
           detail once an issue is picked, then the list. */
        const bool railAlone  = onePane && st->railSheet;
        showRail   = onePane ? st->railSheet : st->railOpen;
        showDetail = wide || (!railAlone && st->detailOpen);
        showList   = !railAlone && !(onePane && st->detailOpen);

        rcRow(.w = "grow", .h = "grow") {
            if (showRail) {
                rail(app, st, railAlone);
                if (!railAlone)
                    rcBox(.bg = s.border, .h = "grow", .wType = RC_PX(1)) {}
            }
            if (showList)
                rcColumn(.w = "grow", .h = "grow") { list(app, st); }
            if (showList && showDetail)
                rcBox(.bg = s.border, .h = "grow", .wType = RC_PX(1)) {}
            if (showDetail)
                detail(app, st, onePane);
        }
    }

    /* A scroll container gets no bar unless you ask for one. Declared outside the
       root so they float over it, and after every rcScrollBy on the same id.
       EACH BAR CARRIES THE CONDITION ITS PANE CARRIES: the panes take turns, and
       an unconditional call would name a container this frame did not build -
       which the library cannot tell from a misspelt id. */
    if (showRail)   rcScrollbar("rail");
    if (showList)   rcScrollbar("list");
    if (showDetail) rcScrollbar("detail");

    /* update() runs before layout(), so a control changed here is serviced on
       the NEXT frame - and this app is RC_RENDER_ON_DEMAND, so ask for that
       frame or none may come until unrelated input arrives. One request at the
       end covers every st->dirty site, and any added later. */
    if (st->dirty)
        rcWindowRequestFrame(rcAppMainWindow(app));
}

#endif /* APP_APP_H */
