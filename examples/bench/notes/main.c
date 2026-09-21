/*
    RayClay Notes - a note-taking and blog editor.

    Demonstrates: a custom titlebar carrying a document tab strip, a three-pane
    layout that folds to one pane on a phone, a multi-line rcTextArea, modal
    dialogs, a runtime light/dark theme switch, and tearing a document out into
    its own window with rcAppOpenWindow.

    Build: cmake --build build --target rayclay_bench_notes
    Web:   cmake --preset web
    RAYCLAY_MAX_FRAMES=N draws N frames and exits - a headless smoke test.
*/
#include "notes_app.h"

/* THE TORN-OFF DOCUMENT WINDOWS. notes_layout holds no RC_App, so it records a
   request - which note, and where the pointer let go - and this file turns the
   request into a window.
   A SLOT PER WINDOW, and the slot is what the callbacks read:
   RC_WindowOptions.userData is a BORROWED pointer, so it has to outlive the window.
   A static table does, and it gives the state callback somewhere to record the
   close; the note index cannot travel in AppState, because two torn windows would
   share the one field. */
#define NOTES_MAX_TORN 4

typedef struct {
    AppState *st;
    int       note;
    bool      live;      /* the slot is in use; cleared when the window closes */
} TornDoc;

static TornDoc g_torn[NOTES_MAX_TORN];

static void torn_layout(RC_Window *w, void *userData) {
    TornDoc *t = (TornDoc *)userData;
    (void)w;
    notes_torn_window(t->st, t->note);
}

/* Free the slot the moment the surface is gone, so a user who tears the same note
   out again gets a window rather than silently nothing once four have been opened
   and closed. FAILED frees it for the same reason READY does not need to. */
static void torn_state(RC_Window *w, RC_WindowState state, void *userData) {
    TornDoc *t = (TornDoc *)userData;
    (void)w;
    if (state == RC_WINDOW_CLOSED || state == RC_WINDOW_FAILED)
        t->live = false;
}

/* Consume one tear-out request. It clears the request whatever happens - a refusal
   that left it set would retry every frame for the life of the app - and tells the
   app whether a window opened, so the tab is closed only when it did. */
static void torn_pump(RC_App *app, AppState *st) {
    RC_Window *primary;
    int        slot;

    if (st->tearNote < 0)
        return;

    for (slot = 0; slot < NOTES_MAX_TORN; slot++)
        if (!g_torn[slot].live)
            break;
    if (slot == NOTES_MAX_TORN) {      /* every slot is already open */
        st->tearNote = -1;
        notes_tear_settle(st, false);
        return;
    }

    g_torn[slot].st   = st;
    g_torn[slot].note = st->tearNote;
    g_torn[slot].live = true;

    primary = rcAppMainWindow(app);
    {
        /* In struct order: C++20 requires designated initialisers in declaration
           order, and this example builds under both compilers. */
        RC_WindowOptions o = {
            .title       = "Note",
            .width       = 480,
            .height      = 560,
            .minWidth    = 320,
            .minHeight   = 240,
            /* Where the pointer let go, less half the window's width, so the window
               appears under the hand that dropped it rather than with its corner
               there. RC_WINDOW_PLACE_AT is a request a window manager may decline. */
            .placement   = RC_WINDOW_PLACE_AT,
            .x           = (int)st->tearX - 240,
            .y           = (int)st->tearY - 40,
            /* Focus is the default here, and that is a decision: a window the user
               just dragged out by hand is the window they are looking at. A panel an
               app pops up unasked should set RC_WINDOW_FOCUS_NONE instead. */
            .owner       = rcWindowOwnerSupported(primary) ? primary : NULL,
            .nativeFrame = true,
            .titlebar    = { .custom = true },
            .clearColor  = rcGetStyle().background,
            /* On demand, unlike the main window: this surface draws a document and
               waits. */
            .renderMode    = RC_RENDER_ON_DEMAND,
            .layoutCallback = torn_layout,
            .stateCallback  = torn_state,
            .userData       = &g_torn[slot],
        };
        /* NULL means no window: invalid input, allocation failure, or a target with
           no native child windows at all. The document stays in this window until
           one really opens, so it can never vanish into a refused request. */
        bool opened = (rcAppOpenWindow(app, &o) != NULL);
        if (!opened)
            g_torn[slot].live = false;
        notes_tear_settle(st, opened);
    }
    st->tearNote = -1;
}

static void demo_update(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    if (!st->seeded)                        /* seed once the renderer/GL is up */
        notes_seed(st, 0xFACADEu);
    AppCtx ctx = app_demo_ctx(app);
    notes_update(st, &ctx);
}
static void demo_layout(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    AppCtx ctx = app_demo_ctx(app);
    notes_layout(st, &ctx);
    notes_demo_chrome(st, &ctx);

    /* THE SECOND LINE A RUNTIME THEME SWITCH NEEDS. A window's clear colour is a
       snapshot taken at create, not a live link to the style, so the old ground
       shows wherever the layout does not cover the window - the edge during a live
       resize, and the frame held back while the layout arena grows.
       Call it unconditionally: the theme toggle is a widget INSIDE the layout, so a
       "has it changed?" guard would read last frame's style and latch the wrong
       ground. The library only acts when the colour actually moves. */
    rcWindowSetClearColor(rcAppMainWindow(app), rcGetStyle().background);

    /* AFTER the layout, because the layout is what sets the request. */
    torn_pump(app, st);
}

int main(void) {
    static AppState state;   /* zero-init; notes_seed fills it on frame 1 */

    /* The app's palette, so the window's clear colour matches the first frame it
       will draw; notes_layout installs it again every frame. */
    rcSetStyle(notes_style(false));

    RC_AppOptions opts = {
        .width               = 1280,
        .height              = 720,
        .title               = "RayClay Notes",
        .clearColor          = rcGetStyle().background,
        .fontSizes           = NOTES_FONT_PX,       /* the ladder the GUI also reads (notes_app.h) */
        .fontCount           = F_COUNT,
        .scratchArenaBytes   = 4096,      /* backs rcFormat in the demo HUD only */
        .startLayoutElements = 8192,      /* a dense multi-pane app: many elements/frame */
        .nativeFrame         = true,
        .titlebarHeight      = 52,
        .updateCallback      = demo_update,
        .layoutCallback      = demo_layout,
        .userData            = &state,
        .titlebar            = { .custom = true },   /* the topbar IS the titlebar */
        /* The save cadence advances on a clock and the overlay reports FPS, so this
           demo draws every frame. A normal app should NOT copy this: stay on
           RC_RENDER_ON_DEMAND (the default) and call rcWindowRequestFrame() when
           your state changes, which is what holds an idle window at ~0 CPU. */
        .renderMode          = RC_RENDER_CONTINUOUS,
    };

    return rcRunApp(&opts);
}
