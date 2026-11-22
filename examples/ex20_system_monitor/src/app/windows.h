/*
    windows.h - floating panels: one panel, its own native window.

    rcAppOpenWindow gives a desktop app more than one native surface under the same
    RC_App and the same thread. Each extra window has its own layout callback, frame
    admission and scratch arena.

    The four rules, in the order they bite:
    1. A non-NULL record is not a ready window: an open from inside a callback comes
       back RC_WINDOW_PENDING, so keep the content docked until READY.
    2. NULL means the open was refused outright: options, memory, or shutdown.
    3. Closing completes after the frame unwinds; release the record in the CLOSED
       transition, which an OS close button reaches too.
    4. rcWindowRelease invalidates the pointer the moment it returns true.

    Owning a window, placing it, pinning it and minimizing it are all requests. Each
    has a *Supported query that says whether this session can do it and a getter that
    says what happened. Ask before drawing a control; read back before reporting.
*/
#ifndef APP_WINDOWS_H
#define APP_WINDOWS_H

#include "app/panels.h"

#include "icons/rc_icons_minus.h"
#include "icons/rc_icons_shrink.h"
#include "icons/rc_icons_square.h"

/* Ids are string literals, never rcFormat'd: this table lives for the whole run, and a
   frame-arena string lasts only until the next frame begins. The creation minimum is
   the only place an app says "smaller than this and my content stops working". */
static const struct {
    const char *title, *detachId, *dockId, *hideId, *raiseId, *slotId;
    int w, h, minW, minH;
} PANEL_WINDOW[PANEL_COUNT] = {
    { "CPU usage", "detach_chart", "dock_chart", "hide_chart", "raise_chart",
      "slot_chart", 720, 400, 420, 340 },
    { "CPU cores", "detach_cores", "dock_cores", "hide_cores", "raise_cores",
      "slot_cores", 380, 380, 320, 320 },
    { "Processes", "detach_table", "dock_table", "hide_table", "raise_table",
      "slot_table", 780, 480, 360, 260 },
};

/* The panel has left: PENDING keeps it here, FAILED is released first. */
static inline bool panel_floating(const AppState *st, Panel p) {
    const RC_Window *w = st->slot[p].window;
    RC_WindowState state = w ? rcWindowState(w) : RC_WINDOW_CLOSED;

    return w && (state == RC_WINDOW_READY || state == RC_WINDOW_CLOSING);
}

static void panel_window_layout(RC_Window *w, void *user);
static void panel_window_state(RC_Window *w, RC_WindowState state, void *user);

/* Where this panel's window should be born. False means wherever the host likes, which
   is not a failure: a client that is never told where it is cannot place itself. */
static inline bool panel_open_at(AppState *st, Panel p, int *x, int *y, int *w, int *h) {
    RC_Window    *primary = rcAppMainWindow(st->app);
    PanelGeometry g       = st->slot[p].geom;
    RC_Dimensions d;
    int           mx, my;

    if (!rcWindowPositionSupported())
        return false;
    if (g.valid) {
        *x = g.x;
        *y = g.y;
        *w = g.w;
        *h = g.h;
        return true;
    }
    if (!rcWindowPosition(primary, &mx, &my))
        return false;
    d  = rcWindowSize(primary);
    *w = PANEL_WINDOW[p].w;
    *h = PANEL_WINDOW[p].h;
    *x = mx + (int)d.width + 12;
    *y = my + (int)p * 28;
    return true;
}

static inline void open_panel_window(AppState *st, Panel p) {
    RC_Window *primary = rcAppMainWindow(st->app);
    int  bornX = 0, bornY = 0;
    int  bornW = PANEL_WINDOW[p].w, bornH = PANEL_WINDOW[p].h;
    bool at = panel_open_at(st, p, &bornX, &bornY, &bornW, &bornH);

    /* Zero fields take the main window's defaults. Strings are copied before open
       returns; userData is borrowed for the record's life. The creation-state fields
       are the point of this call: each is visible if applied a frame late, as a window
       that opens in the wrong place and jumps. An owned window is a child, staying
       above the window it was torn from. */
    RC_WindowOptions o = {
        .title      = PANEL_WINDOW[p].title,
        .width      = bornW,
        .height     = bornH,
        .minWidth   = PANEL_WINDOW[p].minW,
        .minHeight  = PANEL_WINDOW[p].minH,
        .placement  = at ? RC_WINDOW_PLACE_AT : RC_WINDOW_PLACE_DEFAULT,
        .x          = bornX,
        .y          = bornY,
        .owner      = rcWindowOwnerSupported() ? primary : NULL,
        .frame = RC_WINDOW_FRAME_TITLEBAR,
        .clearColor = rcGetStyle().background,
        /* A floating monitor parks like the main one; update() wakes it. */
        .renderMode = RC_RENDER_ON_DEMAND,
        .scratchArenaBytes = 32768,
        .layoutCallback = panel_window_layout,
        .stateCallback  = panel_window_state,
        .userData       = &st->slot[p],
    };
    RC_Window *win = rcAppOpenWindow(st->app, &o);

    if (!win) {
        /* Rule 2, on a desktop that said it could: the app cannot tell which refusal
           it was, so it stops offering the verb and says so. */
        st->singleSurface = true;
        return;
    }
    st->slot[p].window       = win;
    st->slot[p].pinRequested = false;
    st->slot[p].pinAsked     = 0;
    st->slot[p].pinned       = false;
    st->slot[p].pinReadUntil = 0.0;
    st->slot[p].pinReadAt    = 0.0;
    st->slot[p].titledBy     = SORT_COUNT;
    st->lastOpenError        = RC_WINDOW_ERROR_NONE;
}

/* A detached panel renames itself when its content changes: "Processes" is the only
   word the task switcher has for it. On a change, never every frame: a rename is a
   window-system round trip and a taskbar repaint. */
static inline void retitle_floating_panels(AppState *st, RC_App *app) {
    PanelSlot  *slot = &st->slot[PANEL_TABLE];
    const char *name;

    if (!panel_floating(st, PANEL_TABLE))
        return;
    if (slot->titledBy == st->sortKey && slot->titledDesc == st->sortDesc)
        return;

    name = rcFormat(rcAppArena(app), "%s \xc2\xb7 %s %s", PANEL_WINDOW[PANEL_TABLE].title,
                    COLUMN[st->sortKey].label, st->sortDesc ? "desc" : "asc").chars;
    if (rcWindowSetTitle(slot->window, name)) {
        slot->titledBy   = st->sortKey;
        slot->titledDesc = st->sortDesc;
    }
}

/* The detach control, offered only where it can work: in the main window, for a docked
   panel, on a target with more than one surface. Gated on capability, never on width. */
static inline bool detach_offered(const AppState *st, Panel p) {
    return st->app && rcAppCurrentWindow(st->app) == rcAppMainWindow(st->app)
        && rcChildWindowsSupported() && !st->singleSurface && !st->slot[p].window;
}

static inline void detach_button(AppState *st, Panel p) {
    if (!detach_offered(st, p))
        return;
    if (icon_button(PANEL_WINDOW[p].detachId, rcIconExpand, rcGetStyle().textMuted,
                    "Open this panel in its own window", true))
        open_panel_window(st, p);
}

/* What the main window shows in a floating panel's place: what that window is doing,
   and two ways to act on it without going to find it. */
static inline void dock_placeholder(AppState *st, Panel p, const char *w, const char *h) {
    RC_Style   s      = rcGetStyle();
    Palette    pal    = app_palette();
    RC_Window *win    = st->slot[p].window;
    bool       hidden = rcWindowIsMinimized(win);

    dash_panel(.id = PANEL_WINDOW[p].slotId, .bg = rcWithAlpha(s.surface, 0.627f), .w = w, .h = h,
               .hMin = RC_VAL(120)) {
        rcRow(.gap = 6, .px = 8, .align = "cl", .w = "grow", .hType = RC_PX(PANEL_HEAD)) {
            /* The panel's name is the raise control, as in every window list. Restore
               first, then raise: whether rcWindowRaise also un-minimizes is the one
               thing it does not promise. */
            rcBox(.id = PANEL_WINDOW[p].raiseId, .focusable = true,
                  .tooltip = "Bring this window to the front") {
                rcTextC(PANEL_WINDOW[p].title, .font = rcFontSemibold(14),
                        .color = rcShowsHover(PANEL_WINDOW[p].raiseId) ? pal.link : s.text);
            }
            if (rcClicked(PANEL_WINDOW[p].raiseId)) {
                if (rcWindowIsMinimized(win))
                    rcWindowRestore(win);
                rcWindowRaise(win);
            }
            rcSpacer() {}
            /* Hide is not dock: docking closes the window and loses where the user put
               it, hiding parks it and leaves the geometry alone. */
            if (hidden) {
                if (icon_button(PANEL_WINDOW[p].hideId, rcIconSquare, s.textMuted,
                                "Show this panel's window again", true))
                    rcWindowRestore(win);
            } else {
                if (icon_button(PANEL_WINDOW[p].hideId, rcIconMinus, s.textMuted,
                                "Minimize this panel's window", true))
                    rcWindowMinimize(win);
            }
            if (icon_button(PANEL_WINDOW[p].dockId, rcIconShrink, s.textMuted,
                            "Bring this panel back", true))
                rcWindowRequestClose(win);
        }
        /* Each line is read back from the window system, so a refused pin is not
           reported here as a pin. */
        rcBox(.align = "cc", .w = "grow", .h = "grow") {
            if (hidden)
                rcTextL("In its own window, minimized", .color = s.textMuted, .size = 13);
            else if (st->slot[p].pinned)
                rcTextL("In its own window, pinned on top", .color = s.textMuted, .size = 13);
            else
                rcTextL("In its own window", .color = s.textMuted, .size = 13);
        }
    }
}

/* @p w and @p h size the panel in its slot: "grow" where a row of fixed height holds
   it, "fit" where it sits in the scrolling page and is as tall as its content. */
static inline void panel_draw(AppState *st, Panel p, RC_Arena *mem, const char *w, const char *h) {
    if (panel_floating(st, p))
        dock_placeholder(st, p, w, h);
    else
        panel_body(st, p, mem, w, h);
}

/* Park this panel against the main window's right edge and match its height. Both
   calls are requests, which is why the geometry remembered is read back every frame. */
static inline void snap_beside_main(RC_Window *w) {
    RC_Window    *primary = rcAppMainWindow(rcWindowApp(w));
    RC_Dimensions md      = rcWindowSize(primary);
    RC_Dimensions d       = rcWindowSize(w);
    int           mx, my;

    if (!rcWindowPosition(primary, &mx, &my))
        return;
    rcWindowSetPosition(w, mx + (int)md.width + 12, my);
    rcWindowSetSize(w, (int)d.width, (int)md.height);
}

/* The floating window's own chrome, opening with the tag its row header carries: the panel
   shows a simulated server. The button follows our request and says what the next click
   does; the caption beside it reports what the desktop actually did. */
static inline void window_footer(RC_Window *w, PanelSlot *slot) {
    RC_Style s   = rcGetStyle();
    double   now = rcAppTime(rcWindowApp(w));

    if (!slot->pinAsked) {
        slot->pinAsked  = rcWindowTopmostSupported() ? 2 : 1;
        slot->pinReadAt = now - 1.0;   /* read below, on this frame */
    }
    if (slot->pinAsked == 2 && (now < slot->pinReadUntil || now - slot->pinReadAt >= 1.0)) {
        slot->pinned    = rcWindowIsTopmost(w);
        slot->pinReadAt = now;
        if (now < slot->pinReadUntil)   /* a manager may apply a pin a few frames late */
            rcWindowRequestFrameAfter(w, 0.1);
    }

    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        tag("simulated");
        if (slot->pinAsked == 2) {
            if (rcButton("float_pin", slot->pinRequested ? "Unpin" : "Pin on top",
                         .className = "h-7", .variant = RC_BTN_GHOST)) {
                slot->pinRequested = !slot->pinRequested;
                rcWindowSetTopmost(w, slot->pinRequested);
                slot->pinReadUntil = now + 1.0;
            }
        }
        /* A session may refuse client positioning outright, and a maximized window is
           the manager's to arrange. */
        if (rcWindowPositionSupported() && !rcWindowIsMaximized(w)) {
            if (rcButton("float_snap", "Snap beside", .className = "h-7", .variant = RC_BTN_GHOST))
                snap_beside_main(w);
        }
        if (rcButton("float_dock", "Dock", .className = "h-7", .variant = RC_BTN_GHOST))
            rcWindowRequestClose(w);
        if (slot->pinned)
            rcTextL("Above other windows", .color = s.textMuted, .size = 12);
        else if (slot->pinRequested)
            rcTextL("Pin refused by this window manager", .color = s.warning, .size = 12);
    }
}

static void panel_window_layout(RC_Window *w, void *user) {
    PanelSlot *slot = (PanelSlot *)user;
    RC_Style   s    = rcGetStyle();
    /* This window's arena, named rather than inferred: rcAppArena falls back to the
       primary's for a window that owns none. */
    RC_Arena  *mem  = rcWindowArena(w);

    /* Remember where this window is, so the next open can be born there. A maximized
       window is skipped: its geometry is the screen's, not the user's choice. */
    if (!rcWindowIsMaximized(w)) {
        int px, py;

        if (rcWindowPosition(w, &px, &py)) {
            RC_Dimensions live = rcWindowSize(w);

            slot->geom.x     = px;
            slot->geom.y     = py;
            slot->geom.w     = (int)live.width;
            slot->geom.h     = (int)live.height;
            slot->geom.valid = true;
        }
    }

    rcColumn(.id = "float_root", .bg = s.background, .gap = GUTTER, .p = GUTTER, .w = "grow",
             .h = "grow") {
        panel_body(slot->st, slot->panel, mem, "grow", "grow");
        window_footer(w, slot);
    }
}

/* No default, so a state added to the enum is a compile error here. */
static void panel_window_state(RC_Window *w, RC_WindowState state, void *user) {
    PanelSlot *slot = (PanelSlot *)user;
    AppState  *st   = slot->st;
    RC_App    *app  = rcWindowApp(w);

    switch (state) {
    case RC_WINDOW_READY:
        /* The main window is parked; ask it for the frame that shows the panel swapped
           for its placeholder. */
        rcWindowRequestFrame(rcAppMainWindow(app));
        break;
    case RC_WINDOW_FAILED:
        st->lastOpenError = rcWindowError(w);
        if (rcWindowRelease(w))
            slot->window = NULL;
        rcWindowRequestFrame(rcAppMainWindow(app));
        break;
    case RC_WINDOW_CLOSED:
        if (rcWindowRelease(w))
            slot->window = NULL;
        slot->pinRequested = false;
        slot->pinned       = false;
        rcWindowRequestFrame(rcAppMainWindow(app));   /* the panel comes home */
        break;
    case RC_WINDOW_PENDING:
    case RC_WINDOW_CLOSING:
        break;
    }
}

#endif /* APP_WINDOWS_H */
