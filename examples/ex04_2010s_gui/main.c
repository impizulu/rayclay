/*  main.c - RayClay ex04: a 2010s Material Design tasks app.

    Flat Material (circa 2014): a bold indigo app bar, a pink floating action
    button, white cards with soft drop shadows on a light grey canvas. Add
    tasks, check them off, delete a card, hide completed items with a toggle.

    Shows a custom app bar that doubles as the OS drag region, rcTextInput,
    rcCheckbox, rcToggle, a scrolling list with rcScrollbar, a floating action
    button and safe-area insets. Zero-asset: the bundled Latin-1 font is baked
    at runtime and the icons are compiled-in headers. One source, desktop and web.

    Build target: rayclay_ex04_2010s_gui
*/

#include "rayclay.h"

#include "icons/rc_icons_maximize.h"
#include "icons/rc_icons_minus.h"
#include "icons/rc_icons_plus.h"
#include "icons/rc_icons_x.h"

/* The FAB's geometry, spelled once: 56 dp is the Material FAB and 24 dp is its
   margin from the edges. The list and the cards reserve this pair between them. */
#define FAB_SIZE    56
#define FAB_MARGIN  24

#define MAX_TASKS 32

/* Font ladder baked from the bundled face at these sizes. */
typedef enum { F_SMALL = 0, F_BODY, F_TITLE, F_COUNT } AppFont;

typedef struct {
    char text[48];
    bool done;
} Task;

typedef struct {
    Task  tasks[MAX_TASKS];
    int   count;
    char  input[48];     /* new-task text-input buffer              */
    bool  showDone;      /* include completed tasks in the list     */
    int   pendingDelete; /* -1 = none, never 0; applied at the top of the next layout */
} AppState;

static void add_task(AppState *st) {
    if (st->count >= MAX_TASKS || !st->input[0])
        return;
    rcStrCopy(st->tasks[st->count].text, st->input, sizeof st->tasks[0].text);
    st->tasks[st->count].done = false;
    st->count++;
    st->input[0] = '\0';
}

static void delete_task(AppState *st, int i) {
    for (int j = i; j < st->count - 1; j++)
        st->tasks[j] = st->tasks[j + 1];
    st->count--;
}

/* A window control draws its glyph through an RC_IconCallback, so the glyph picks
   its own ink - these three sit on saturated indigo beside a white title. */
static void ctl_minimize(float size, RC_Color ink) { (void)ink; rcIconMinus(size, RC_WHITE); }
static void ctl_maximize(float size, RC_Color ink) { (void)ink; rcIconMaximize(size, RC_WHITE); }
static void ctl_close(float size, RC_Color ink)    { (void)ink; rcIconX(size, RC_WHITE); }

/* The indigo app bar is also the native drag region, and it paints UNDER the
   status bar as Material does: the SURFACE spans the window and the CONTENT is
   padded in by the safe insets. Spend the insets on the outer column, NOT inside
   rcUnzoomed() - a number written in there is counter-scaled by the zoom. The
   band itself is held at the physical .titlebarHeight the OS drag strip is frozen
   at. .id is required: .shadow is keyed by element id, and an id-less element
   drops its shadow and warns. */
static void appbar(RC_Insets safe) {
    RC_Style s = rcGetStyle();

    rcColumn(.id = "appbar", .bg = s.chrome, .pt = (uint16_t)safe.top,
             .pl = (uint16_t)safe.left, .pr = (uint16_t)safe.right, .w = "grow",
             .shadow = { rcAlpha(RC_BLACK, 40), 0, 2, 6, 0 }) {
        rcUnzoomed() {
            rcRow(.id = RC_ID_WINDOW_DRAG, .bg = s.chrome, .gap = 12, .px = 16,
                  .align = "cl", .w = "grow", .h = "56px") {
                rcTextL("Tasks", .font = F_TITLE, .color = RC_WHITE);
                rcBox(.w = "grow") {}
                rcRow(.gap = 2, .align = "cc") {
                    rcWindowControlButton(RC_WINCTL_MINIMIZE, ctl_minimize, 16.0f);
                    rcWindowControlButton(RC_WINCTL_MAXIMIZE, ctl_maximize, 16.0f);
                    rcWindowControlButton(RC_WINCTL_CLOSE,    ctl_close,    16.0f);
                }
            }
        }
    }
}

/* A white Material card: a bound checkbox, the task text (muted when done) and a
   delete X. Returns the index the user asked to delete, or -1. */
static int task_card(RC_App *app, AppState *st, int i) {
    RC_Arena   *mem = rcAppArena(app);
    RC_Style    s   = rcGetStyle();
    const char *card_id  = rcFormat(mem, "card_%d", i).chars;
    const char *check_id = rcFormat(mem, "chk_%d",  i).chars;
    const char *del_id   = rcFormat(mem, "del_%d",  i).chars;
    int del = -1;

    /* The card runs the full width of the list and carries the FAB's horizontal
       clearance as its own right padding, so the surface reaches the edge and the
       delete X does not. */
    rcBox(.id = card_id, .bg = s.surface, .pl = 12, .pr = FAB_SIZE + FAB_MARGIN,
          .py = 8, .borderRadius = "all-lg", .w = "grow",
          .shadow = { rcAlpha(RC_BLACK, 28), 0, 2, 8, 0 }) {
        rcRow(.gap = 12, .align = "cl", .w = "grow") {
            rcCheckbox(check_id, "", &st->tasks[i].done);
            /* Word-wrapped, never clipped - a long title must survive a phone in
               portrait. .align = "cl" keeps the checkbox and the X centred as the
               card grows a line. */
            rcBox(.w = "grow") {
                rcTextC(st->tasks[i].text, .font = F_BODY,
                         .color = st->tasks[i].done ? s.textMuted : s.text);
            }
            /* The only destructive control in the app, so its hover has to read as
               destructive: a red tint and a red icon, two channels not one faint fill. */
            bool delHot = rcIsHovered(del_id);
            rcBox(.id = del_id,
                  .bg = delHot ? rcAlpha(RC_RED_500, 40) : RC_TRANSPARENT,
                  .align = "cc", .borderRadius = "all-full", .w = "32px", .h = "32px") {
                rcIconX(16.0f, delHot ? RC_RED_600 : s.textMuted);
            }
        }
    }
    if (rcClicked(del_id))
        del = i;
    return del;
}

static void layout(RC_App *app, void *userData) {
    AppState *st  = (AppState *)userData;
    RC_Arena *mem = rcAppArena(app);
    RC_Style  s   = rcGetStyle();

    /* The delete lands HERE, before anything is drawn. rcTextC does not copy: the
       cards hand RayClay pointers straight into st->tasks[i].text and the library
       keeps them until the frame is drawn, so compacting the array mid-frame would
       aim every retained pointer one slot high. */
    if (st->pendingDelete >= 0) {
        delete_task(st, st->pendingDelete);
        st->pendingDelete = -1;
    }

    int del = -1;   /* set by a card this frame; applied at the top of the next */

    /* SAFE AREA. A phone draws edge to edge, under the status bar and the home
       indicator. rcViewport().safe gives the margins ALREADY IN LAYOUT UNITS.
       Spend each exactly once: top and sides to the app bar (which paints under
       the status bar) and to the content column, the bottom to the root. */
    RC_Insets safe = rcViewport().safe;

    rcColumn(.id = "Root", .bg = s.background, .pb = (uint16_t)(safe.bottom),
             .w = "grow", .h = "grow") {

        appbar(safe);

        rcColumn(.bg = s.background, .gap = 16, .pt = 20, .pb = 20,
                 .pl = (uint16_t)(20 + safe.left), .pr = (uint16_t)(20 + safe.right),
                 .w = "grow", .h = "grow") {

            rcRow(.id = "addcard", .bg = s.surface, .gap = 10, .px = 12, .py = 8,
                  .align = "cl", .borderRadius = "all-lg", .w = "grow",
                  .shadow = { rcAlpha(RC_BLACK, 28), 0, 2, 8, 0 }) {
                rcBox(.w = "grow") {
                    rcTextInput("new", st->input, sizeof st->input,
                                 .placeholder = "Add a task");
                }
                /* Return adds the task, keyed on the FIELD having focus rather than
                   the window: Return while the toggle is focused belongs to it. */
                if (rcButton("btn_add", "Add", RC_BTN_PRIMARY)
                    || (rcIsFocused("new") && rcKeyPressed(RC_KEY_ENTER)))
                    add_task(st);
            }

            /* Above the list, so the bottom-right FAB never covers this toggle. */
            rcRow(.gap = 10, .align = "cl", .w = "grow") {
                RC_String info = rcFormat(mem, "%d task%s",
                                             st->count, st->count == 1 ? "" : "s");
                rcText(info, .font = F_SMALL, .color = s.textMuted);
                rcBox(.w = "grow") {}
                rcTextL("Show completed", .font = F_SMALL, .color = s.textMuted);
                rcToggle("tg_done", &st->showDone);
            }

            /* Counted HERE, not at the top of layout(): the "add" button above runs
               first and can raise st->count. */
            int shown = 0;
            for (int i = 0; i < st->count; i++)
                if (st->showDone || !st->tasks[i].done) shown++;

            /* THE FAB'S FOOTPRINT IS A NO-CONTENT ZONE ON BOTH AXES. The foot is
               reserved here so the last card scrolls clear of the button; the
               horizontal half of the reserve lives in the card's own padding, so
               every card is clear of it at every scroll offset, not just the end.
               The bottom safe inset is not added: the root already spends it. */
            rcColumn(.id = "list", .gap = 10, .pb = FAB_SIZE + FAB_MARGIN,
                     .scroll = "v", .w = "grow", .h = "grow") {
                if (shown == 0) {
                    rcColumn(.gap = 6, .align = "cc", .w = "grow", .h = "grow") {
                        rcTextL("All clear", .font = F_TITLE, .color = s.textMuted);
                        rcTextL("Add a task above, or tap the + button.",
                                 .font = F_SMALL, .color = s.textMuted);
                    }
                }
                for (int i = 0; i < st->count; i++) {
                    if (!st->showDone && st->tasks[i].done)
                        continue;
                    int d = task_card(app, st, i);
                    if (d >= 0) del = d;
                }
            }
        }
    }

    /* The deferral is only correct if a frame actually follows, so ask for one. */
    if (del >= 0) {
        st->pendingDelete = del;
        rcWindowRequestFrame(rcAppMainWindow(app));
    }

    /* Scrollbar + FAB are floating - place them OUTSIDE the root column. */
    rcScrollbar("list");

    /* Floating action button, pinned FAB_MARGIN in from the SAFE edges: the root's
       insets do not reach a float anchored to the root, so the offset adds them.
       zIndex 2 puts it above rcScrollbar's thumb, which declares itself at z 1.
       Clicking it focuses the input, or adds the task if one is typed. */
    rcBox(.id = "fab", .bg = rcIsHovered("fab") ? RC_PINK_500 : RC_PINK_400,
          .align = "cc", .borderRadius = "all-full",
          .wType = RC_PX(FAB_SIZE), .hType = RC_PX(FAB_SIZE),
          .shadow = { rcAlpha(RC_BLACK, 60), 0, 6, 10, 0 },
          .floating = { .to = RC_ATTACH_ROOT, .parent = RC_ANCHOR_BOTTOM_RIGHT,
                         .element = RC_ANCHOR_BOTTOM_RIGHT,
                         .offset = { -(FAB_MARGIN + safe.right),
                                     -(FAB_MARGIN + safe.bottom) },
                         .zIndex = 2 }) {
        rcIconPlus(24.0f, RC_WHITE);   /* Material's 24 dp icon, not a text glyph */
    }
    if (rcClicked("fab")) {
        if (st->input[0]) {
            add_task(st);
            /* Read after the list was built, so the new task belongs to the next
               frame - and on demand that frame has to be asked for. */
            rcWindowRequestFrame(rcAppMainWindow(app));
        } else {
            rcSetFocus("new");
        }
    }
}

/* No updateCallback: nothing here changes except in response to input, so the
   runner parks in the OS event loop at ~0 CPU between clicks. */

int main(void) {
    /* Enough rows that the list reads like a list, two of them already done so
       the "Show completed" toggle has something to hide. */
    static const char *const starters[] = {
        "Ship the Material tasks example",
        "Bake the font at runtime (zero-asset)",
        "Try the pink action button",
        "Draft the release notes",
        "Review the layout pass on a phone viewport",
        "Reply to the design feedback thread",
        "Book the venue for the team offsite",
        "Renew the signing certificate",
        "Water the office plant",
    };
    enum { STARTER_COUNT = (int)(sizeof starters / sizeof starters[0]) };

    static AppState state = { .showDone = true, .pendingDelete = -1 };

    /* Derived from the array and clamped to the buffer - never a second literal. */
    state.count = STARTER_COUNT < MAX_TASKS ? STARTER_COUNT : MAX_TASKS;
    for (int i = 0; i < state.count; i++)
        rcStrCopy(state.tasks[i].text, starters[i], sizeof state.tasks[0].text);
    state.tasks[1].done = true;
    state.tasks[5].done = true;

    static const float fontSizes[F_COUNT] = {
        [F_SMALL] = 13.0f,
        [F_BODY]  = 16.0f,
        [F_TITLE] = 20.0f,
    };

    /* Light Material palette: grey canvas, white cards, indigo chrome/accent. */
    RC_Style st = rcStyleLight();
    st.background = RC_GRAY_100;
    st.surface    = RC_WHITE;
    st.chrome     = RC_INDIGO_500;
    st.text       = RC_GRAY_900;
    st.textMuted  = RC_GRAY_500;
    st.primary    = RC_INDIGO_500;
    st.radius     = 8.0f;
    rcSetStyle(st);

    RC_AppOptions opts = {
        .width             = 720,
        .height            = 620,
        .title             = "Tasks - RayClay",
        .fontSizes         = fontSizes,
        .fontCount         = F_COUNT,
        .scratchArenaBytes = 4096,
        .nativeFrame       = true,
        .titlebarHeight    = 56,
        .layoutCallback    = layout,
        .userData          = &state,
        .titlebar          = { .custom = true },   /* the Material app bar IS the titlebar */
    };
    return rcRunApp(&opts);
}
