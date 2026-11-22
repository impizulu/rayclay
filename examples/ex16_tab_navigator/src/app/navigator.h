/*
    navigator.h - the tab navigator: copy this file into your app, with the two icons it includes

    What a phone app's navigation is, as one component: tabs with badges, a stack of screens under
    each tab, a real push, a swipe back from the left edge, and large titles that collapse into the
    top band. It knows nothing about your content. A route is two ints you choose, and you draw a
    route in one callback.

        static Navigator nav;
        nav_init(&nav, tab_titles, 4);                once
        nav_update(&nav, rcAppTime(app));             each frame, in the update callback
        nav_stage(app, &nav, draw_screen, user, top); where the screens go, in the layout callback
        nav_tab_item(app, &nav, &tab, i, false);      each tab, in a row or a rail of your own
        nav_push(&nav, sc->depth, KIND, item, title); from any screen, on a tap

    THE STAGE IS WHY THE PUSH LOOKS REAL. While a stack's slide is moving, two screens are declared
    at once, each a float in one clipped box: the top one riding its offset in from the right, the
    one beneath it drifting a little left and dimming. A float's offset moves nothing else in the
    layout, so both are laid out once at their full size and only slide.

    SWITCHING TABS NEVER POPS, AND A COVERED SCREEN KEEPS ITS PLACE. RayClay drops a scroll offset
    the frame after its container stops being declared, so each route saves its own from
    rcGetScrollInfo and hands it back through .scrollOffset. A screen does that by giving its scroll
    container sc->scrollId and .scrollOffset = { 0.0f, sc->scrollY }.

    THE WIDE ARM. Set nav.wide when the window has room for two panes. The root of the tab then goes
    in a list column of your own (nav_root_screen), and the stage holds only what was pushed from
    it: a tap in the list replaces the pane, as a mail app's list does, and a tap in the pane
    pushes.

    A LOW WINDOW. Set nav.low when the window is too short for a large title, as a phone on its side
    is: nav_large_title then declares nothing, and a root's band shows its title from the start.

    THE PRESS. nav_init defines the class nav-press, and every tappable box here names it in its
    .className: name it on your own tappable boxes, and a press reads the same on all of them.
    Each also declares .focusable, a Tab stop that Enter and Space press with the focus ring
    around it, as a web app's buttons are: declare it on yours too, or the keys never reach them.
*/
#ifndef APP_NAVIGATOR_H
#define APP_NAVIGATOR_H

#include "rayclay.h"

#include "icons/rc_icons_chevron_left.h"
#include "icons/rc_icons_x.h"

enum {
    NAV_MAX_TABS = 5, NAV_DEPTH = 8,
    NAV_BAND_H = 44,      /* the top band, under the safe area */
    NAV_TITLE_H = 52,     /* a large title, in the scrolling content */
    NAV_TARGET = 44,      /* the smallest thing a finger is asked to hit */
    NAV_GUTTER = 16,
    NAV_EDGE_W = 16       /* the strip along the left edge a swipe back starts on */
};

#define NAV_PUSH_SECONDS 0.32
#define NAV_UNDER_SHIFT  0.28f   /* how far a covered screen drifts, of the stage's width */
#define NAV_UNDER_DIM    0.18f   /* and how dark it gets, as the black wash's alpha */

static const RC_Color NAV_BADGE = { 239, 35, 60, 255 };   /* a count's red, in either theme */

/* What a route shows is yours: `kind` and `item` are never read here. Kind 0 is a tab's root. */
typedef struct {
    int         kind, item;
    const char *title;     /* the band's title, the back control's label; yours to keep alive */
    float       scrollY;
} NavRoute;

/* `slide` is the top route's place: 1 sits on screen, 0 waits off the right edge. A push runs it up
   from 0, a pop down to 0 and only then takes the route off, so the screen leaving is still
   declared for the whole of its exit. A finger on the left edge drives it directly. */
typedef struct {
    NavRoute route[NAV_DEPTH];
    int      depth;
    float    slide, slideFrom, slideTo;
    double   slideStart;   /* the clock, or < 0 while it rests */
    bool     dragging;
    float    dragX;        /* where the finger came down */
} NavStack;

typedef struct {
    NavStack stack[NAV_MAX_TABS];
    int      tab, tabCount;
    bool     wide;         /* a list column beside the stage; yours to set each frame */
    bool     low;          /* too short for large titles, a phone on its side; yours to set too */
    bool     instant;      /* no sliding: a reduce-motion setting */
    double   now;
} Navigator;

/* One tab, as the bar or rail draws it this frame: a count over 0 is a badge, else a dot may be. */
typedef struct {
    const char     *id;
    RC_IconCallback icon, iconLit;
    int             badge;
    bool            dot;
} NavTab;

/* One screen, as nav_stage hands it to your draw callback. */
typedef struct {
    int             tab, depth;
    const NavRoute *route;
    const char     *scrollId;   /* give this to the screen's one scroll container */
    float           scrollY;    /* and this to its .scrollOffset */
    float           topInset;   /* the safe area the screen's top band sits on */
    bool            pane;       /* the wide arm's pane, with nothing pushed into it */
} NavScreen;

typedef void (*NavDrawScreen)(RC_App *app, void *user, const NavScreen *sc);

/* A button in a screen's top band: an icon, or a word when `icon` is NULL. An icon's `label` is its
   tooltip, since a glyph alone does not say what it opens. */
typedef struct {
    const char     *id;
    RC_IconCallback icon;
    const char     *label;
    bool            disabled;
} NavButton;

static float nav_clamp01(float v) {
    return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}

static void nav_init(Navigator *nav, const char *const *titles, int tabCount) {
    int i;

    /* THE FILL OF ANYTHING TAPPABLE. hover: is false once a finger lifts, so a phone never leaves a
       row lit, and active: makes a press one step stronger under both. */
    rcDefineClass("nav-press", "hover:bg-foreground/5 active:bg-foreground/10");
    nav->tabCount = tabCount;
    for (i = 0; i < tabCount; i++) {
        nav->stack[i].route[0].title = titles[i];
        nav->stack[i].slide = nav->stack[i].slideTo = 1.0f;
        nav->stack[i].slideStart = -1.0;
    }
}

/* Runs every stack's slide. It eases out, fast at first and settling, as a phone's push does. */
static void nav_update(Navigator *nav, double now) {
    int i;

    nav->now = now;
    for (i = 0; i < nav->tabCount; i++) {
        NavStack *n = &nav->stack[i];
        float     t, e;

        if (n->slideStart < 0.0)
            continue;
        t = (float)((now - n->slideStart) / NAV_PUSH_SECONDS);
        t = t > 1.0f ? 1.0f : t;
        e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
        n->slide = n->slideFrom + (n->slideTo - n->slideFrom) * e;
        if (t >= 1.0f) {
            n->slideStart = -1.0;
            if (n->slideTo == 0.0f)
                n->depth--;
            n->slide = n->slideTo = 1.0f;
        }
    }
}

/* True while a slide runs: ask for the next frame, because RayClay draws only when something
   happens. */
static bool nav_is_moving(const Navigator *nav) {
    int i;

    for (i = 0; i < nav->tabCount; i++)
        if (nav->stack[i].slideStart >= 0.0)
            return true;
    return false;
}

/* `from` is the depth of the screen that asked, so a tap on a screen on its way out goes nowhere.
   At the stack's cap the top route is replaced, so a walk from post to author never has to end. */
static void nav_push(Navigator *nav, int from, int kind, int item, const char *title) {
    NavStack *n = &nav->stack[nav->tab];

    if (nav->wide && from == 0) {
        n->depth = 1;
        n->route[1] = RC_LIT(NavRoute){ kind, item, title, 0.0f };
        n->slide = n->slideTo = 1.0f;
        n->slideStart = -1.0;
        return;
    }
    if (from != n->depth || n->slideTo == 0.0f)
        return;
    if (n->depth + 1 >= NAV_DEPTH)
        n->depth--;
    n->route[++n->depth] = RC_LIT(NavRoute){ kind, item, title, 0.0f };
    n->slideFrom = 0.0f;
    n->slideTo = 1.0f;
    n->slide = nav->instant ? 1.0f : 0.0f;
    n->slideStart = nav->instant ? -1.0 : nav->now;
}

static void nav_pop(Navigator *nav) {
    NavStack *n = &nav->stack[nav->tab];

    if (n->depth == 0 || n->slideTo == 0.0f)
        return;
    /* The pop takes the slide from a finger on the edge: with both driving it, the clock would
       take the route off and the finger's release pop the one beneath, down past the root. */
    n->dragging = false;
    if (nav->instant) {
        /* At rest, too: a push still sliding from before the setting would go on drawing a
           layer under the new top, which from the first screen is one below the root. */
        n->depth--;
        n->slide = n->slideTo = 1.0f;
        n->slideStart = -1.0;
        return;
    }
    n->slideFrom = n->slide;
    n->slideTo = 0.0f;
    n->slideStart = nav->now;
}

/* THE BACK KEY's rule: pop, else go to the first tab. False at the first tab's root, where Back
   belongs to the system. */
static bool nav_can_back(const Navigator *nav) {
    return nav->stack[nav->tab].depth > 0 || nav->tab != 0;
}

static void nav_back(Navigator *nav) {
    if (nav->stack[nav->tab].depth > 0)
        nav_pop(nav);
    else
        nav->tab = 0;
}

/* A root screen's scroll id: tab and depth, then the route's kind and item, so a new post at the
   same depth starts at its top by being a new id. */
static const char *nav_scroll_id(RC_Arena *m, int tab, int depth, const NavRoute *r) {
    return rcFormat(m, "nav_%d_%d_%d_%d", tab, depth, r->kind, r->item).chars;
}

/* The bar's rule, every phone's. Another tab switches to it. The lit tab goes home: with a screen
   open it pops to the root in one slide, and at the root it scrolls to the top. Returns true when
   it was at the top already, the one tap an app may give its own meaning: check for new posts. */
static bool nav_select(RC_App *app, Navigator *nav, int t) {
    NavStack *n = &nav->stack[t];

    if (nav->tab != t) {
        nav->tab = t;
    } else if (n->depth > 0) {
        n->route[1] = n->route[n->depth];
        n->depth = 1;
        nav_pop(nav);
    } else {
        const char *id = nav_scroll_id(rcAppArena(app), t, 0, &n->route[0]);

        if (rcGetScrollInfo(id).offsetY <= 0.0f)
            return true;
        rcScrollToTop(id);
    }
    return false;
}

/* One tab; the whole cell is the target, and the same id serves a bar and a rail. Filled while lit,
   with what is waiting on it. Passthrough keeps a badge off the cell's own tap. */
static bool nav_tab_item(RC_App *app, Navigator *nav, const NavTab *tab, int index, bool inRail) {
    RC_Style       s  = rcGetStyle();
    const bool     on = nav->tab == index;
    const RC_Color fg = on ? s.primary : s.textMuted;
    bool           top = false;

    rcColumn(.id = tab->id, .focusable = true, .gap = 3, .align = "cc", .borderRadius = "all-12",
             .w = "grow", .hType = inRail ? RC_PX(62) : RC_GROW, .className = "nav-press") {
        rcBox(.align = "cc") {
            (on ? tab->iconLit : tab->icon)(24.0f, fg);
            if (tab->badge > 0) {
                rcBox(.bg = NAV_BADGE, .px = 5, .align = "cc", .borderRadius = "all-full",
                      .hType = RC_PX(18), .wMin = RC_VAL(18.0f),
                      .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_RIGHT,
                                    .element = RC_ANCHOR_TOP_LEFT, .offset = { -9.0f, -5.0f },
                                    .capture = RC_CAPTURE_PASSTHROUGH }) {
                    rcText(rcFormat(rcAppArena(app), "%d", tab->badge), .font = rcFontSemibold(11),
                           .color = RC_WHITE, .wrap = "n", .select = RC_SELECT_NONE);
                }
            } else if (tab->dot) {
                rcBox(.bg = s.primary, .borderRadius = "all-full", .wType = RC_PX(8),
                      .hType = RC_PX(8),
                      .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_RIGHT,
                                    .element = RC_ANCHOR_TOP_LEFT, .offset = { -3.0f, -1.0f },
                                    .capture = RC_CAPTURE_PASSTHROUGH }) {}
            }
        }
        rcTextC(nav->stack[index].route[0].title, .font = (uint16_t)(on ? rcFontSemibold(11) : 0),
                .color = fg, .size = 11, .wrap = "n", .select = RC_SELECT_NONE);
    }
    if (rcClicked(tab->id))
        top = nav_select(app, nav, index);
    return top;
}

/* One screen on the stage, at its offset, with its scrollbar: under a mouse the bar takes a 12 px
   band, so a screen keeps its width as it slides under another and back. A float counts its z
   from the float it is declared in, so what floats in the screen underneath (its bar, a badge,
   the pinned tabs) sits at 1 and its rings at 2: the top screen that slides over it takes z 3,
   which covers them all (rayclay.h, RC_Float). */
static void nav_layer(RC_App *app, Navigator *nav, NavDrawScreen draw, void *user, int depth,
                      float x, float dim, float topInset) {
    RC_Style       s    = rcGetStyle();
    NavStack      *n    = &nav->stack[nav->tab];
    NavRoute      *r    = &n->route[depth];
    const bool     pane = nav->wide && depth == 0;
    const char    *id   = pane ? "nav_pane" : nav_scroll_id(rcAppArena(app), nav->tab, depth, r);
    RC_ScrollInfo  si   = rcGetScrollInfo(id);
    const float    y    = si.found ? si.offsetY : r->scrollY;
    NavScreen      sc   = { nav->tab, depth, r, id, y, topInset, pane };
    const bool     lit  = depth == n->depth && x > 0.0f;
    const RC_Color edge = lit ? rcWithAlpha(RC_BLACK, 0.235f) : RC_UNSET_COLOR;
    const RC_Color wash = dim > 0.0f ? rcWithAlpha(RC_BLACK, dim) : RC_UNSET_COLOR;

    rcColumn(.bg = s.background, .w = "grow", .h = "grow",
             .shadow = { .color = edge, .x = -4.0f, .blur = 16.0f },
             .floating = { .to = RC_ATTACH_PARENT, .offset = { x, 0.0f },
                           .zIndex = (int16_t)(lit ? 3 : 0) }, .overlay = wash) {
        draw(app, user, &sc);
        rcScrollbar(id);
    }
    if (!pane)
        r->scrollY = sc.scrollY;
}

/* The band's top padding: the safe area, and never under 4 px. A focus ring draws 4 px outside a
   control, and the band's controls are as tall as the band, so where no safe area stands above
   it (a desktop window, the wide arm) the window's top edge would cut their rings. */
static float nav_band_top(float topInset) { return topInset > 4.0f ? topInset : 4.0f; }

/* THE STAGE. A press on the strip along its left edge starts the swipe back: the strip is a float
   that takes the press, so nothing under it also reads a tap, and the band above it keeps its back
   control. It reads the press with rcIsHovered, not rcPressed, because a swipe is no button and
   should not show a button's hand. While the finger is down it drives the slide directly; the
   release pops past 40 percent of the width, else returns. On the wide arm depth 0 here is the
   pane with nothing open in it: your callback sees sc->pane, and draws what that pane should show.
*/
static void nav_stage(RC_App *app, Navigator *nav, NavDrawScreen draw, void *user, float topInset) {
    NavStack    *n   = &nav->stack[nav->tab];
    const RC_Box box = rcGetElementBox("nav_stage");
    const float  w   = box.width > 0.0f ? box.width : rcViewport().width;

    if (n->dragging) {
        if (rcPointerDown(RC_POINTER_LEFT)) {
            n->slide = 1.0f - nav_clamp01((rcPointer().x - n->dragX) / w);
        } else {
            n->dragging = false;
            n->slideFrom = n->slide;
            n->slideTo = n->slide < 0.6f ? 0.0f : 1.0f;
            n->slideStart = nav->now;
        }
    }
    rcBox(.id = "nav_stage", .overflow = "hidden", .w = "grow", .h = "grow") {
        if (n->slide < 1.0f)
            nav_layer(app, nav, draw, user, n->depth - 1, -NAV_UNDER_SHIFT * n->slide * w,
                      NAV_UNDER_DIM * n->slide, topInset);
        nav_layer(app, nav, draw, user, n->depth, (1.0f - n->slide) * w, 0.0f, topInset);
        if (n->depth > 0) {
            const float under = (float)NAV_BAND_H + nav_band_top(topInset);

            rcBox(.id = "nav_edge", .h = "grow", .wType = RC_PX(NAV_EDGE_W),
                  .floating = { .to = RC_ATTACH_PARENT, .offset = { 0.0f, under } }) {}
            if (rcPointerPressed(RC_POINTER_LEFT) && rcIsHovered("nav_edge") &&
                n->slideStart < 0.0) {
                n->dragging = true;
                n->dragX = rcPointer().x;
            }
        }
    }
}

/* The wide arm's list column: the tab's root, in flow, never sliding. */
static void nav_root_screen(RC_App *app, Navigator *nav, NavDrawScreen draw, void *user) {
    NavRoute     *r  = &nav->stack[nav->tab].route[0];
    const char   *id = nav_scroll_id(rcAppArena(app), nav->tab, 0, r);
    RC_ScrollInfo si = rcGetScrollInfo(id);
    NavScreen     sc = { nav->tab, 0, r, id, si.found ? si.offsetY : r->scrollY, 0.0f, false };

    draw(app, user, &sc);
    rcScrollbar(id);
    r->scrollY = sc.scrollY;
}

/* THE TOP BAND. Pushed, it shows the back control and the title. At a tab's root the title lives in
   the content as a large title (nav_large_title), and the band's small one fades in as that scrolls
   under it. A low window has no large title, as a phone on its side has none, so there a root's
   band shows its title from the start. The surface spans the width and takes the safe inset as
   padding, so the status bar sits on it. `eyebrow` is an optional line for a root's empty left
   side. Returns the button tapped, or -1. */
static int nav_top_band(RC_App *app, Navigator *nav, const NavScreen *sc, const char *eyebrow,
                        const NavButton *buttons, int count) {
    RC_Style    s      = rcGetStyle();
    RC_Arena   *m      = rcAppArena(app);
    const bool  root   = sc->depth == 0;
    const float fade   = root && !nav->low ? nav_clamp01((sc->scrollY - 30.0f) / 16.0f) : 1.0f;
    const float top    = nav_band_top(sc->topInset);
    const char *backId = rcFormat(m, "nav_back_%d", sc->depth).chars;
    int         tapped = -1, i;

    rcRow(.bg = s.background, .pt = (float)top, .pl = (float)(root ? NAV_GUTTER : 4),
          .pr = 4, .align = "cc", .w = "grow", .hType = RC_PX(NAV_BAND_H + (int)top)) {
        /* No clip here: the back control clips its own label, and a clip around it, as tall as
           the control, would cut the focus ring drawn 4 px outside it. */
        rcRow(.align = "cl", .w = "grow", .hType = RC_PX(NAV_BAND_H), .className = "min-w-0") {
            if (root && eyebrow && fade < 1.0f) {
                rcTextC(eyebrow, .font = rcFontSemibold(13),
                        .color = rcWithAlpha(s.textMuted, 1.0f - fade),
                        .wrap = "e", .select = RC_SELECT_NONE);
            } else if (!root) {
                /* The back control says where it goes. On the wide arm the first pushed screen has
                   only the list beside it to go back to, so it closes instead. */
                rcRow(.id = backId, .focusable = true, .pr = 8, .align = "cl", .overflow = "hidden",
                      .borderRadius = "all-full", .hType = RC_PX(NAV_TARGET),
                      .wMin = RC_VAL(NAV_TARGET), .className = "nav-press") {
                    if (nav->wide && sc->depth == 1) {
                        rcBox(.pl = 11) {
                            rcIconX(22.0f, s.primary);
                        }
                    } else {
                        rcIconChevronLeft(28.0f, s.primary);
                        rcTextC(nav->stack[sc->tab].route[sc->depth - 1].title, .color = s.primary,
                                .size = 16, .wrap = "e", .select = RC_SELECT_NONE);
                    }
                }
                if (rcClicked(backId))
                    nav_pop(nav);
            }
        }
        /* A title that has fully faded is not declared, so it takes no width from the buttons. */
        if (fade > 0.0f)
            rcTextC(sc->route->title, .font = rcFontSemibold(17),
                    .color = rcWithAlpha(s.text, fade), .wrap = "n", .select = RC_SELECT_NONE);
        /* With the title faded out, the buttons give the date the band's spare width. A word is a
           ghost rcButton in the accent, h-11 (NAV_TARGET) tall; a class's color stands on a
           disabled button, so the muted one is spelled for that state. */
        rcRow(.align = "cr", .w = root && fade <= 0.0f ? "fit" : "grow",
              .hType = RC_PX(NAV_BAND_H)) {
            for (i = 0; i < count; i++) {
                const NavButton *b = &buttons[i];

                if (b->icon) {
                    rcBox(.id = b->id, .focusable = true, .align = "cc", .borderRadius = "all-full",
                          .wType = RC_PX(NAV_TARGET), .hType = RC_PX(NAV_TARGET),
                          .tooltip = b->label, .className = "nav-press") {
                        b->icon(22.0f, s.text);
                    }
                    if (rcClicked(b->id))
                        tapped = i;
                } else if (rcButton(b->id, b->label, .disabled = b->disabled,
                                    .className = "h-11 text-primary disabled:text-muted-foreground",
                                    .variant = RC_BTN_GHOST)) {
                    tapped = i;
                }
            }
        }
    }
    /* At a root's top the rule takes the ground's color, so the band and the large title read as
       one surface until the list scrolls. */
    rcBox(.w = "grow", .hType = RC_PX(1),
          .className = root && sc->scrollY < 1.0f ? "border-t border-background"
                                                  : "border-t border-border") {}
    return tapped;
}

/* A tab root's large title: first in its scroll container, so it scrolls under the band. A low
   window declares none: the band carries the title there. */
static void nav_large_title(const Navigator *nav, const char *title) {
    if (nav->low)
        return;
    rcBox(.pb = 8, .px = NAV_GUTTER, .align = "bl", .w = "grow", .hType = RC_PX(NAV_TITLE_H)) {
        rcTextC(title, .font = rcFontSemibold(34), .color = rcGetStyle().text, .lineHeight = 41,
                .wrap = "n", .select = RC_SELECT_NONE);
    }
}

#endif /* APP_NAVIGATOR_H */
