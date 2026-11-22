/*  main.c - RayClay ex04: a 2010s Material Design tasks app.

    Material as Google's Lollipop-era apps wore it: a vivid indigo app bar whose band runs on
    behind the first cards, a navigation drawer, white cards on a gray page, a details side
    sheet, chips, a pink floating action button on the seam between two surfaces, and a
    snackbar whose UNDO really undoes.

    Motion is the point of this design language, and it needs no animation system: an animation
    is a number computed from rcAppTime, fed into a size or an offset, with one
    rcWindowRequestFrame while it runs. Three kinds are shown: a press ripple (a float cut by the
    box it is declared in), a shared-element move (a ticked row lifts off and lands in Completed
    while the list closes up behind it) and a slide (the snackbar rising over the drawer's empty
    foot, or across the bottom of a narrow window).

    Also shows a custom app bar that doubles as the OS drag region, fields bound to one record,
    rcCheckbox, rcBeginModal, rcViewport breakpoints, safe-area insets and keyboard shortcuts.
    Zero-asset. One source, desktop, web and mobile.

    Build target: rayclay_ex04_2010s_gui
*/

#include "rayclay.h"

#include "icons/rc_icons_briefcase.h"
#include "icons/rc_icons_flag.h"
#include "icons/rc_icons_house.h"
#include "icons/rc_icons_inbox.h"
#include "icons/rc_icons_plus.h"
#include "icons/rc_icons_search.h"
#include "icons/rc_icons_star.h"
#include "icons/rc_icons_star_filled.h"
#include "icons/rc_icons_sun.h"
#include "icons/rc_icons_trash_2.h"
#include "icons/rc_icons_x.h"

/* Material's measures, in dp. */
#define BAR_H       64
#define DRAWER_W    256
#define FAB_SIZE    56
#define SNACK_H     48
#define MOVE_SECS   0.42
#define MAX_TASKS   48

/* Indigo 500 for chrome, pink A200 and A400 as the accent. */
#define INDIGO      rcHex(0x3f51b5)
#define INDIGO_TINT rcHex(0xe8eaf6)
#define ACCENT      rcHex(0xff4081)
#define ACCENT_DARK rcHex(0xf50057)
#define INK         rcHex(0x212121)
#define MUTED       rcHex(0x757575)
#define HAIRLINE    rcHex(0xe0e0e0)
#define PAGE        rcHex(0xeeeeee)

/* The widgets' look: the pink accent on checkboxes and focused fields, and with its hover shade
   on the FAB, whose class names the two roles; gray field rules and Material's 2 dp corners. */
static RC_Style app_style(void) {
    RC_Style s = rcStyleLight();

    s.primary      = ACCENT;
    s.primaryHover = ACCENT_DARK;
    s.border       = rcHex(0x9e9e9e);
    s.radius       = 2.0f;
    return s;
}

typedef enum { DUE_TODAY, DUE_TOMORROW, DUE_LATER, DUE_SOMEDAY, DUE_COUNT } Due;
typedef enum { LABEL_NONE, LABEL_WORK, LABEL_HOME, LABEL_ERRANDS, LABEL_COUNT } Label;
typedef enum { VIEW_INBOX, VIEW_TODAY, VIEW_STARRED, VIEW_WORK, VIEW_HOME, VIEW_ERRANDS,
               VIEW_COUNT } View;

static const char *const DUE_NAMES[DUE_COUNT]     = { "Today", "Tomorrow", "Later", "Someday" };
static const char *const LABEL_NAMES[LABEL_COUNT] = { "No label", "Work", "Home", "Errands" };
static const uint32_t    LABEL_INKS[LABEL_COUNT]  = { 0x616161, 0x1565c0, 0x2e7d32, 0xc75b00 };
static const char *const VIEW_NAMES[] = { "Inbox", "Today", "Starred", "Work", "Home", "Errands" };
static const RC_IconCallback VIEW_ICONS[] = {
    rcIconInbox, rcIconSun, rcIconStar, rcIconBriefcase, rcIconHouse, rcIconFlag, };

typedef struct {
    char   title[64], notes[200];
    Due    due;
    Label  label;
    bool   done, starred, deleted;
    double movedAt;   /* when it last changed place; 0 = at rest */
    RC_Box from;      /* its box when it left, where its flight starts */
} Task;

/* Tasks never shift in the array: a delete is a flag, so a row's id, its text pointer and UNDO
   all stay valid. */
typedef struct {
    Task        tasks[MAX_TASKS];
    int         count, selected, undoTask, toggleAsk;   /* a tick lands late: see list_pane */
    View        view;
    char        query[48];
    bool        editOpen, wide, animating;
    double      now, snackStart, snackUntil, rippleStart;
    const char *snackText;
    char        rippleId[24];
    RC_Vec2     rippleAt;
} AppState;

static const char *item_id(RC_App *app, const char *kind, int i) {
    return rcFormat(rcAppArena(app), "%s_%d", kind, i).chars;
}

static float eased(double seconds) {
    float p = seconds <= 0.0 ? 0.0f : seconds >= MOVE_SECS ? 1.0f : (float)(seconds / MOVE_SECS);
    return p * p * (3.0f - 2.0f * p);
}

static char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 'a' - 'A') : c; }

/* A task row's height, in dp: a phone's title takes two lines. */
static float row_h(void) { return rcViewport().width < 600.0f ? 84.0f : 64.0f; }

static bool same_text(const char *a, const char *b) {
    for (; *a && *a == *b; a++, b++) {}   /* walk on while they agree */
    return *a == *b;
}

/* Whether text holds needle, ignoring case: what the search box asks of a task. */
static bool contains(const char *text, const char *needle) {
    for (;; text++) {
        int i = 0;
        while (needle[i] && lower(text[i]) == lower(needle[i])) i++;
        if (!needle[i] || !*text)   /* all of it matched here (an empty needle at once), or */
            return !needle[i];      /* the text ran out with no match */
    }
}

static bool in_nav_view(const Task *t, View view) {
    return view == VIEW_INBOX || (view == VIEW_TODAY && t->due == DUE_TODAY)
           || (view == VIEW_STARRED && t->starred)
           || (view >= VIEW_WORK && (int)t->label == (int)view - VIEW_WORK + LABEL_WORK);
}

/* A task lives in one list. While it moves it also keeps a closing gap in the list it left. */
static bool lives_in(const Task *t, bool doneList) { return !t->deleted && t->done == doneList; }

static bool leaving(const Task *t, bool doneList) {
    return t->movedAt != 0 && (t->deleted ? t->done == doneList : t->done != doneList);
}

/* Whether a task has a place in this card: one per due date, and Completed (due < 0). */
static bool in_card(const AppState *st, const Task *t, int due, bool doneList) {
    return (lives_in(t, doneList) || leaving(t, doneList)) && (due < 0 || (int)t->due == due)
           && in_nav_view(t, st->view)
           && (contains(t->title, st->query) || contains(t->notes, st->query));
}

static void offer_undo(AppState *st, int i, const char *text) {
    if (st->now >= st->snackUntil) st->snackStart = st->now;
    st->undoTask   = i;
    st->snackText  = text;
    st->snackUntil = st->now + 4.0;
}

static void toggle_task(RC_App *app, AppState *st, int i) {
    Task *t = &st->tasks[i];

    t->from    = rcGetElementBox(item_id(app, "row", i));
    t->done    = !t->done;
    t->movedAt = st->now;
    if (t->done) offer_undo(st, i, "Task completed");
}

/* Every change of selection comes here, because Google Tasks drops a task left without a title. */
static void select_task(AppState *st, int i) {
    if (st->selected >= 0 && st->selected != i && !st->tasks[st->selected].title[0])
        st->tasks[st->selected].deleted = true;
    st->selected = i;
    st->editOpen = i >= 0;
}

static void delete_task(AppState *st, int i) {
    select_task(st, -1);
    st->tasks[i].deleted = true;
    st->tasks[i].movedAt = st->now;
    offer_undo(st, i, "Task deleted");
}

/* Google Tasks' flow: the new task exists at once, in the Inbox, with its title focused. A
   delete only flags its task, so once the array is full a new task takes back the first slot
   deleted for good, any but the one the snackbar's UNDO names; else every delete, and every new
   task left untitled, would keep its slot, and after 38 of them N and the FAB would do nothing. */
static void new_task(AppState *st) {
    int i = st->count < MAX_TASKS ? st->count : 0;

    while (i < st->count && !(st->tasks[i].deleted && i != st->undoTask)) i++;
    if (i == MAX_TASKS) return;
    st->tasks[i] = RC_LIT(Task){ 0 };   /* a slot taken back starts as a new one */
    st->view     = VIEW_INBOX;
    st->query[0] = '\0';
    select_task(st, i == st->count ? st->count++ : i);
    rcSetFocus("task_title");
}

/* Material's touch ripple: ink grows from the press point inside the box that declared it, whose
   overflow cuts it to shape, and fades as it fills. A round button needs no clip: its ripple is
   centered and stops at the circle. Its z of 3 is above the FAB and under any dialog, so a row's
   ripple stays under the dialog it opened; in a modal's body the library counts that z from the
   panel up. One press at a time, so one ripple is enough, and it is the innermost control's, as
   Material ripples only what was pressed: rcPressed is true on every box under the pointer, so
   a box with controls inside declares its ripple before them, and theirs take the press over. */
static void ripple(AppState *st, const char *id, RC_Color ink, bool round) {
    RC_Box box = rcGetElementBox(id);

    if (rcPressed(id)) {
        rcStrCopy(st->rippleId, id, sizeof st->rippleId);
        st->rippleAt    = RC_LIT(RC_Vec2){ rcPointer().x - box.x, rcPointer().y - box.y };
        st->rippleStart = st->now;
    }
    float left = 1.0f - (float)((st->now - st->rippleStart) / 0.45);
    if (!same_text(st->rippleId, id) || left <= 0.0f) return;
    float   size = (round ? box.width : 2.0f * (box.width + box.height)) * (1.0f - left * left);
    RC_Vec2 at   = round ? RC_LIT(RC_Vec2){ box.width / 2.0f, box.height / 2.0f } : st->rippleAt;

    rcBox(.bg = rcWithAlpha(ink, (uint8_t)(48.0f * left) / 255.0f), .borderRadius = "all-full",
          .wType = RC_PX(size), .hType = RC_PX(size),
          .floating = { .to = RC_ATTACH_PARENT, .element = RC_ANCHOR_CENTER, .offset = at,
                        .zIndex = 3, .capture = RC_CAPTURE_PASSTHROUGH }) {}
    st->animating = true;
}

/* Under the pointer the button washes in its own ink. Its class (gray-ink or pink-ink, defined in
   main) names that ink, which bg-current takes. The glyph and its ripple take a color, not a
   class, so the caller hands the same ink over as one too. Like every button this file builds
   from a box (chips, drawer rows, task rows, the FAB), it is .focusable: Tab reaches it, and
   Enter or Space reads as rcClicked. A key presses nothing, so it starts no ripple. */
static bool icon_button(AppState *st, const char *id, RC_IconCallback icon, RC_Color ink,
                        const char *inkClass, const char *tip) {
    RC_Size size = RC_PX(rcPointerIsCoarse() ? 44 : 40);

    rcBox(.id = id, .focusable = true, .align = "cc", .borderRadius = "all-full",
          .wType = size, .hType = size, .tooltip = tip, .className = inkClass) {
        icon(20.0f, ink);
        ripple(st, id, ink, true);
    }
    return rcClicked(id);
}

/* A row of single-choice chips, wrapping or scrolling sideways. Returns the chosen index. A
   sliding row runs to the edge of its box and keeps its end inset inside the scroll, as
   Material's chips do: a chip slides off under the edge, and the last one rests 24 px short.
   Each wears the chip class, a gray that darkens under the pointer. The chosen chip's typed
   fill beats it, and the others' RC_UNSET_COLOR sets none, which leaves the class's gray. The
   sliding row stands on the indigo band, where Material's chips show no bar: its track is
   transparent, and so is its thumb until the pointer is on it, and the chip cut off at the edge
   is what says the row slides. */
static int chips(RC_App *app, AppState *st, const char *kind, const char *const *names, int count,
                 int selected, RC_Color onFill, bool slides) {
    rcRow(.id = slides ? kind : NULL, .gap = 8, .scroll = slides ? "h" : NULL, .w = "grow",
          .className = slides ? "min-w-0 pr-6" : "flex-wrap") {
        for (int i = 0; i < count; i++) {
            const char *id = item_id(app, kind, i);
            bool        on = i == selected;

            rcRow(.id = id, .focusable = true, .bg = on ? onFill : RC_UNSET_COLOR, .px = 12,
                  .align = "cc", .overflow = "hidden", .borderRadius = "all-full",
                  .hType = RC_PX(32), .className = slides ? "shrink-0 chip" : "chip") {
                rcTextC(names[i], .color = on ? RC_WHITE : INK, .size = 13);
                ripple(st, id, on ? RC_WHITE : INK, false);
            }
            if (rcClicked(id)) selected = i;
        }
    }
    if (slides)
        rcScrollbar(kind, .className = "bg-transparent accent-transparent hover:accent-white/70");
    return selected;
}

/* One form wherever a task is edited: the side sheet, or a dialog on a narrow window. Its fields
   are bound to the record itself, so the row updates as you type. */
static void details(RC_App *app, AppState *st) {
    Task *t = &st->tasks[st->selected];   /* read first: a delete in the header deselects */

    rcRow(.align = "cl", .w = "grow", .hType = RC_PX(BAR_H)) {
        rcTextL("Details", .font = rcFontSemibold(20.0f), .color = INK);
        rcSpacer() {}
        if (icon_button(st, "task_trash", rcIconTrash2, MUTED, "gray-ink", "Delete"))
            delete_task(st, st->selected);
        if (icon_button(st, "task_close", rcIconX, MUTED, "gray-ink", "Close")) select_task(st, -1);
    }
    rcTextInput("task_title", t->title, sizeof t->title, .className = "underlined",
                .placeholder = "Title", .clearButton = true);
    rcTextArea("task_notes", t->notes, sizeof t->notes, .className = "underlined",
               .placeholder = "Add details", .rows = 4);
    rcTextL("Due", .color = MUTED, .size = 12);
    t->due = (Due)chips(app, st, "due", DUE_NAMES, DUE_COUNT, (int)t->due, INDIGO, false);
    rcTextL("Label", .color = MUTED, .size = 12);
    t->label = (Label)chips(app, st, "label", LABEL_NAMES, LABEL_COUNT, (int)t->label, INDIGO,
                            false);
}

/* The label views stand under a rule and Material's subheader. The foot below the last row stays
   empty: the snackbar rises there, which is why layout shows the drawer from 480 px of height. */
static void drawer(RC_App *app, AppState *st) {
    rcColumn(.bg = RC_WHITE, .pt = 8, .h = "grow", .wType = RC_PX(DRAWER_W),
             .className = "border-r border-gray-300") {
        for (int v = 0; v < VIEW_COUNT; v++) {
            const char *id     = item_id(app, "nav", v);
            bool        active = (int)st->view == v;
            int         n      = 0;

            for (int i = 0; i < st->count; i++)
                n += lives_in(&st->tasks[i], false) && in_nav_view(&st->tasks[i], (View)v);
            if (v == VIEW_WORK)
                rcBox(.pt = 16, .pl = 24, .border = { .color = HAIRLINE }, .w = "grow",
                      .className = "border-t") {
                    rcTextL("Labels", .font = rcFontSemibold(14.0f), .color = MUTED);
                }
            /* Material's gray 100 under the pointer, Tailwind's neutral-100; the view shown
               keeps its tint, as a typed fill beats the class. */
            rcRow(.id = id, .focusable = true, .bg = active ? INDIGO_TINT : RC_UNSET_COLOR,
                  .gap = 16, .pl = 24, .pr = 16, .align = "cl", .overflow = "hidden", .w = "grow",
                  .hType = RC_PX(48), .className = "hover:bg-neutral-100") {
                VIEW_ICONS[v](20.0f, active ? INDIGO : MUTED);
                rcTextC(VIEW_NAMES[v], .font = (uint16_t)(active ? rcFontSemibold(14.0f) : 0),
                        .color = active ? INDIGO : INK, .size = 14, .select = RC_SELECT_NONE);
                rcSpacer() {}
                /* The label's size, so the two centered runs share one baseline. */
                rcText(rcFormat(rcAppArena(app), "%d", n), .color = active ? INDIGO : MUTED,
                       .size = 14, .select = RC_SELECT_NONE);
                ripple(st, id, INK, false);
            }
            if (rcClicked(id)) {
                st->view = (View)v;
                rcScrollToTop("list");
            }
        }
    }
}

static void label_chip(const Task *t) {
    if (!t->label) return;
    RC_Color ink = rcHex(LABEL_INKS[t->label]);
    rcBox(.bg = rcWithAlpha(ink, 0.1f), .px = 8, .py = 2, .borderRadius = "all-full") {
        rcTextC(LABEL_NAMES[t->label], .color = ink, .size = 12, .select = RC_SELECT_NONE);
    }
}

/* What a row shows. The row draws it live; its copy in flight draws it too, its controls inert
   under a pass-through float, so the row that lifts off is the row that lands. Its check box
   takes Material's 2 dp edge, where the theme's hairline reads as a desktop form's. */
static void row_face(RC_App *app, AppState *st, int i, bool live) {
    Task *t       = &st->tasks[i];
    bool  checked = t->done, tiny = rcViewport().width < 600.0f;

    if (rcCheckbox(item_id(app, live ? "chk" : "fly_chk", i), "", &checked,
                   .className = "border-2") && live)
        st->toggleAsk = i;
    /* The bundled box answers its own press, so the row's ripple, declared first, lets go. */
    if (live && rcPressed(item_id(app, "chk", i))) st->rippleId[0] = '\0';
    rcColumn(.gap = 2, .overflow = "hidden", .w = "grow") {
        rcTextC(t->title[0] ? t->title : "Untitled task", .color = t->done ? MUTED : INK,
                .size = 16, .wrap = tiny ? NULL : "e", .select = RC_SELECT_NONE,
                .lineClamp = (uint8_t)(tiny ? 2 : 0));
        /* A phone sets the label under the title, as Google Tasks does: the title keeps the row. */
        rcRow(.gap = 6, .align = "cl", .overflow = "hidden", .w = "grow") {
            if (tiny) label_chip(t);
            if (t->notes[0])
                rcTextC(t->notes, .color = MUTED, .size = 14, .wrap = "e",
                        .select = RC_SELECT_NONE);
        }
    }
    if (!tiny) label_chip(t);
    if (icon_button(st, item_id(app, live ? "star" : "fly_star", i),
                    t->starred ? rcIconStarFilled : rcIconStar, t->starred ? ACCENT : MUTED,
                    t->starred ? "pink-ink" : "gray-ink", t->starred ? "Unstar" : "Star") && live)
        t->starred = !t->starred;
}

static void task_row(RC_App *app, AppState *st, int i) {
    const char *id = item_id(app, "row", i);

    rcRow(.id = id, .focusable = true,
          .bg = i == st->selected && st->wide ? INDIGO_TINT : RC_UNSET_COLOR, .gap = 16, .pl = 16,
          .pr = 8, .align = "cl", .overflow = "hidden", .w = "grow", .hType = RC_PX(row_h()),
          .className = "hover:bg-neutral-100") {
        ripple(st, id, INK, false);   /* first: the checkbox and the star take their presses */
        row_face(app, st, i, true);
    }
    if (rcClicked(id)) select_task(st, i);
}

static void flights(RC_App *app, AppState *st) {
    for (int i = 0; i < st->count; i++) {
        Task  *t  = &st->tasks[i];
        RC_Box to = rcGetElementBox(item_id(app, "row", i));

        if (t->movedAt == 0 || t->deleted || t->from.width <= 0.0f || !to.found) continue;
        float   e  = eased(st->now - t->movedAt);
        RC_Vec2 at = { t->from.x + (to.x - t->from.x) * e, t->from.y + (to.y - t->from.y) * e };

        rcRow(.bg = RC_WHITE, .gap = 16, .pl = 16, .pr = 8, .align = "cl",
              .wType = RC_PX(t->from.width), .hType = RC_PX(row_h()),
              .shadow = { rcWithAlpha(RC_BLACK, 0.314f), 0, 6, 16, 0 },
              .floating = { .to = RC_ATTACH_ROOT, .offset = at, .zIndex = 4,
                            .capture = RC_CAPTURE_PASSTHROUGH }) {
            row_face(app, st, i, false);
        }
    }
}

/* One card of rows; returns false, drawing nothing, while it would be empty. */
static bool task_card(RC_App *app, AppState *st, int due, bool doneList) {
    int first = 0;

    while (first < st->count && !in_card(st, &st->tasks[first], due, doneList)) first++;
    if (first == st->count) return false;

    rcColumn(.bg = RC_WHITE, .borderRadius = "2", .w = "grow",
             .shadow = { rcWithAlpha(RC_BLACK, 0.196f), 0, 1, 3, 0 }) {
        rcRow(.px = 16, .align = "cl", .w = "grow", .hType = RC_PX(48)) {
            rcTextC(doneList ? "Completed" : DUE_NAMES[due], .font = rcFontSemibold(14.0f),
                    .color = doneList ? MUTED : INDIGO);
        }
        /* A row at rest is the row. A moving one is a plain box of an eased height: the place it
           left closes up, the place it is going opens up, and flights() draws it in the air. */
        for (int i = 0; i < st->count; i++) {
            Task *t = &st->tasks[i];
            float e = eased(st->now - t->movedAt);

            if (!in_card(st, t, due, doneList)) continue;
            if (t->movedAt == 0) task_row(app, st, i);
            else if (leaving(t, doneList))
                rcBox(.w = "grow", .hType = RC_PX(row_h() * (1.0f - e))) {}
            else
                rcBox(.id = item_id(app, "row", i), .w = "grow", .hType = RC_PX(row_h() * e)) {}
        }
    }
    return true;
}

/* The band is the page header's own background, so it is as tall as the header in the frame that
   lays it out, and it scrolls away with it. A float of the band's size, 48 px lower and at a
   negative z, carries it on under the first card. Nothing else in the page paints over that
   float: the gray page is the window's clear color. Where the chips show, the large title
   drops, as Material's toolbar collapses on a phone: the lit chip names the view, and a title
   over it would name it twice. A row's tick lands after the last row, because a task changing
   list mid-frame would be declared twice under one id. */
static void list_pane(RC_App *app, AppState *st, RC_Viewport vp, bool chipsNav) {
    /* A phone's gutters are narrower. Neither is room for rcScrollbar's bar: under a mouse it
       takes a band of its own. */
    float pad   = vp.width < 600.0f ? 12.0f : 24.0f;
    float shown = eased(st->now - st->snackStart) * eased(st->snackUntil - st->now);
    float room  = chipsNav ? SNACK_H * shown : 0.0f;   /* what the snackbar takes from the list */
    bool  any   = false;
    /* Narrow, the list reaches the window's sides, its band as the bar does, and its content
       spends the side insets; beside the drawer the page spends them (layout). */
    RC_Insets side = chipsNav ? vp.safe : RC_LIT(RC_Insets){ 0 };

    st->animating = st->animating || (shown > 0.0f && shown < 1.0f);

    rcColumn(.id = "main", .w = "grow", .h = "grow", .className = "min-w-0") {
        rcColumn(.id = "list", .pb = (float)(FAB_SIZE + 48 + vp.safe.bottom + room),
                 .align = "tc", .scroll = "v", .w = "grow", .h = "grow") {
            rcColumn(.bg = INDIGO, .align = "tc", .w = "grow") {
                rcBox(.bg = INDIGO, .w = "grow", .h = "grow",
                      .floating = { .to = RC_ATTACH_PARENT, .offset = { 0, 48 }, .zIndex = -1 }) {}
                rcColumn(.gap = 12, .pt = 16, .pb = 24, .pl = pad + 16 + side.left,
                         .pr = side.right, .w = "grow", .wMax = RC_VAL(960.0f)) {
                    if (!chipsNav) rcTextC(VIEW_NAMES[st->view], .color = RC_WHITE, .size = 34);
                    if (chipsNav)
                        st->view = (View)chips(app, st, "view", VIEW_NAMES, VIEW_COUNT,
                                               (int)st->view, ACCENT, true);
                }
            }
            rcColumn(.gap = 16, .pl = pad + side.left, .pr = pad + side.right, .w = "grow",
                     .wMax = RC_VAL(960.0f)) {
                for (int d = 0; d < DUE_COUNT; d++)
                    any = task_card(app, st, d, false) || any;
                any = task_card(app, st, -1, true) || any;
                if (!any)
                    rcColumn(.bg = RC_WHITE, .gap = 16, .py = 48, .align = "cc", .w = "grow") {
                        (st->query[0] ? rcIconSearch : VIEW_ICONS[st->view])(48.0f, HAIRLINE);
                        rcTextC(st->query[0] ? "No task matches your search."
                                             : "Nothing here yet.", .color = MUTED);
                    }
            }
        }
        rcScrollbar("list");
        if (st->toggleAsk >= 0) toggle_task(app, st, st->toggleAsk);
        st->toggleAsk = -1;

        /* Material's snackbar floats with elevation. With the drawer shown it rises over the
           drawer's empty foot, where Google Inbox put it, and covers no row. Narrow, it spans the
           window's foot and the list's padding grows by its height, so any row scrolls clear. A
           float on the root takes no padding from the page, so it pads its text by the side
           insets itself. */
        if (shown > 0.0f) {
            float edge = chipsNav ? 0.0f : 8.0f;
            float h    = SNACK_H + (chipsNav ? vp.safe.bottom : 0.0f);

            rcRow(.bg = rcHex(0x323232), .gap = 8, .pb = h - SNACK_H, .pl = 16 + vp.safe.left,
                  .pr = 8 + vp.safe.right, .align = "cl", .borderRadius = "2",
                  .wType = RC_PX(chipsNav ? vp.width : DRAWER_W - 2.0f * edge), .hType = RC_PX(h),
                  .shadow = { rcWithAlpha(RC_BLACK, 0.235f), 0, 3, 5, 0 },
                  .floating = { .to = RC_ATTACH_ROOT, .parent = RC_ANCHOR_BOTTOM_LEFT,
                                .element = RC_ANCHOR_BOTTOM_LEFT,
                                .offset = { edge, (h + edge) * (1.0f - shown) - edge },
                                .zIndex = 5 }) {
                rcTextC(st->snackText, .color = RC_WHITE, .size = 14);
                rcSpacer() {}
                /* UNDO takes the offer, so a double click undoes once: its second press, as
                   the bar slides away, would toggle the task the first brought back. A task
                   unticked by hand since has nothing left to undo. Its label is Material's
                   pink A100. Hovered and pressed it lays a white wash on the dark bar: the
                   ghost's own hover fill is the theme's near-white surface, where the pink
                   label would barely read, and a class fill beats the variant's. */
                if (rcButton("undo", "UNDO",
                             .className = "text-[#ff80ab] hover:bg-white/10 active:bg-white/20",
                             .variant = RC_BTN_GHOST) && st->undoTask >= 0) {
                    if (st->tasks[st->undoTask].deleted)
                        select_task(st, st->undoTask);   /* back as it was: in its sheet */
                    else if (st->tasks[st->undoTask].done)
                        toggle_task(app, st, st->undoTask);
                    st->tasks[st->undoTask].deleted = false;
                    st->snackUntil = st->now + MOVE_SECS;
                    st->undoTask   = -1;
                }
            }
        }

        /* Wide, the FAB rides the seam between the list and the side sheet, Material's anchor.
           Its class (fab, defined in main) fills it with the theme's accent and its hover
           shade, and lifts it under the pointer, as Material's elevation does. */
        rcBox(.id = "fab", .focusable = true, .align = "cc",
              .borderRadius = "all-full", .wType = RC_PX(FAB_SIZE), .hType = RC_PX(FAB_SIZE),
              .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_BOTTOM_RIGHT,
                            .element = RC_ANCHOR_BOTTOM_RIGHT,
                            .offset = { st->wide && st->selected >= 0 ? FAB_SIZE / 2.0f
                                                                      : -pad - side.right,
                                        -(24.0f + vp.safe.bottom + room) }, .zIndex = 2 },
              .tooltip = "New task (N)", .className = "fab") {
            rcIconPlus(24.0f, RC_WHITE);
            ripple(st, "fab", RC_WHITE, true);
        }
        if (rcClicked("fab")) new_task(st);
    }
}

/* The bar is the OS drag region and paints under the status bar: the surface spans the window,
   the content is padded in by the safe insets. rcUnzoomed holds it at the unzoomed height the OS
   drag strip is frozen at. The window controls' class sizes their glyphs and inks them white,
   and the minimize and maximize wash takes that ink: it lightens the indigo, as Material's does. */
static void app_bar(AppState *st, bool chipsNav) {
    const RC_Insets safe = rcGetSafeAreaInsets();   /* window px: the unzoomed band's unit */
    /* The search field's class. Its h-* is exact, so it sets 40 px under a mouse only: under a
       finger the field keeps the 44 px floor, as the icon buttons grow to 44. */
    const char *field = rcPointerIsCoarse() ? "bg-white border-0" : "h-10 bg-white border-0";

    rcUnzoomed() {
        rcRow(.id = RC_ID_WINDOW_DRAG, .bg = INDIGO, .pt = safe.top, .pl = safe.left + 24,
              .pr = safe.right, .align = "cl", .w = "grow", .hType = RC_PX(BAR_H + safe.top)) {
            rcRow(.pr = 12, .align = "cl", .wType = chipsNav ? RC_FIT : RC_PX(DRAWER_W - 24)) {
                rcTextL("Tasks", .font = rcFontSemibold(20.0f), .color = RC_WHITE);
            }
            /* The field grows to its cap inside a box that takes the rest, so the window controls
               keep the right edge: a spacer beside the field would take half the room, as two
               grows share it. Focused, the field lifts off the bar, as Inbox's did: the box
               around it reads rcIsFocused, as a class's focus: reads only its own element. While
               it holds text, its clear button sits inside it at the right. */
            rcRow(.pr = 16, .w = "grow") {
                rcRow(.bg = RC_WHITE, .align = "cl", .borderRadius = "2", .noDrag = true,
                      .w = "grow", .wMax = RC_VAL(560.0f),
                      .shadow = { rcWithAlpha(RC_BLACK, rcIsFocused("search") ? 0.353f : 0),
                                  0, 2, 6, 0 }) {
                    rcTextInput("search", st->query, sizeof st->query,
                                .className = field, .placeholder = "Search",
                                .leadingIcon = rcIconSearch, .clearButton = true);
                }
            }
            rcWindowControls(.className = "text-white icon-5");
        }
    }
}

static void layout(RC_App *app, void *userData) {
    AppState   *st       = (AppState *)userData;
    RC_Viewport vp       = rcViewport();
    /* Filter chips replace the drawer on a narrow window, and on one under 480 px tall: a phone
       on its side keeps the phone's layout, and a drawer any shorter would have the snackbar
       over its last row. */
    bool        chipsNav = vp.width < 840.0f || vp.height < 480.0f;
    /* Beside the drawer the page spends the side insets, once; narrow, list_pane's content
       does, so its indigo band runs on to the window's sides under the bar. */
    RC_Insets   page     = chipsNav ? RC_LIT(RC_Insets){ 0 } : vp.safe;

    st->now       = rcAppTime(app);
    st->wide      = vp.width >= 1120.0f;         /* below it, details open in a dialog */
    st->animating = false;
    for (int i = 0; i < st->count; i++) {
        if (st->now - st->tasks[i].movedAt >= MOVE_SECS) st->tasks[i].movedAt = 0;
        st->animating = st->animating || st->tasks[i].movedAt != 0;
    }
    /* The details follow what the list shows: a closed dialog, another view or a search that
       hides the task each let go of it. Return in the dialog's Title is its Done, as in
       Material's add-task dialog; read before the dialog is declared, so it closes this frame. */
    Task *sel     = st->selected >= 0 ? &st->tasks[st->selected] : NULL;
    bool  confirm = rcKeyPressedOn("task_title", RC_KEY_ENTER);
    if (sel && ((!st->wide && (!st->editOpen || confirm)) || !in_card(st, sel, -1, sel->done)))
        select_task(st, -1);

    /* Shortcuts read only while no field has focus, so a letter in a field stays a letter, and
       not under the dialog, which owns the keys while it is up. Each focuses a field, which
       would also type the key: so the keys are consumed here, before any field is declared. */
    bool shortcuts = (st->wide || !st->editOpen) && !rcIsFocused("search")
                     && !rcIsFocused("task_title") && !rcIsFocused("task_notes");
    RC_KeyEvent e;
    for (int i = 0; shortcuts && rcKeyEvent(i, &e); i++)
        if (e.key == RC_KEY_N || e.key == RC_KEY_SLASH) rcKeyEventConsume(i);
    if (shortcuts && rcKeyPressed(RC_KEY_N)) new_task(st);
    if (shortcuts && rcKeyPressed(RC_KEY_SLASH)) rcSetFocus("search");

    rcColumn(.id = "Root", .w = "grow", .h = "grow") {
        app_bar(st, chipsNav);
        rcRow(.pl = page.left, .pr = page.right, .w = "grow", .h = "grow") {
            if (!chipsNav) drawer(app, st);
            list_pane(app, st, vp, chipsNav);
            /* Wide, the details are a side sheet beside the list; narrow, a dialog (below). */
            if (st->wide && st->selected >= 0) {
                rcColumn(.id = "sheet", .bg = RC_WHITE, .gap = 16, .pb = 24, .px = 24,
                         .scroll = "v", .h = "grow", .wType = RC_PX(360),
                         .shadow = { rcWithAlpha(RC_BLACK, 0.18f), -1, 0, 5, 0 }) {
                    details(app, st);
                }
                rcScrollbar("sheet");
            }
        }
    }
    /* The bar's elevation. In-flow boxes of equal z paint in declaration order, so the bar's
       own shadow would lie under the page declared after it unless the bar took a z (RC_Float's
       notes); this strip on the bar's foot carries it instead. */
    rcBox(.bg = INDIGO, .w = "grow", .h = "4px",
          .shadow = { rcWithAlpha(RC_BLACK, 0.235f), 0, 3, 3, 0 },
          .floating = { .to = RC_ATTACH_ELEMENT, .toId = RC_ID_WINDOW_DRAG,
                        .parent = RC_ANCHOR_BOTTOM_LEFT, .element = RC_ANCHOR_BOTTOM_LEFT,
                        .zIndex = 1, .capture = RC_CAPTURE_PASSTHROUGH }) {}
    flights(app, st);

    if (!st->wide && st->selected >= 0 && st->editOpen && rcBeginModal("edit", &st->editOpen)) {
        /* No room left, in a tiny or zoomed-in window, caps it at 0: a negative is refused. */
        rcColumn(.gap = 16, .w = "480px", .wMax = RC_VAL(vp.width > 72.0f ? vp.width - 72.0f : 0)) {
            details(app, st);
        }
        rcEndModal();
    }

    /* The snackbar's exit is read off the clock, not off its ease: that rounds to 1.0f for the
       exit's first ~36 us, when the wait for the exit's start is already past, a wait the
       library ignores, and a frame there would leave the bar up until the next input. */
    if (st->animating || (st->now < st->snackUntil && st->snackUntil - st->now <= MOVE_SECS))
        rcWindowRequestFrame(rcAppMainWindow(app));
    else if (st->now < st->snackUntil)
        rcWindowRequestFrameAfter(rcAppMainWindow(app), st->snackUntil - MOVE_SECS - st->now);
}

int main(void) {
    static AppState state = {
        .tasks = {
            { .title = "Send the Q3 budget to finance", .notes = "Priya needs it by 3 pm.",
              .label = LABEL_WORK, .starred = true },
            { .title = "Call Dr. Alvarez", .notes = "Move the checkup.", .label = LABEL_HOME },
            { .title = "Get the dry cleaning", .notes = "Ticket 4471.", .label = LABEL_ERRANDS },
            { .title = "Review Maya's patch", .notes = "400 lines.", .label = LABEL_WORK },
            { .title = "Book the team offsite", .notes = "Lakeview holds Dec 11 until Friday.",
              .due = DUE_TOMORROW, .label = LABEL_WORK, .starred = true },
            { .title = "Renew the car registration", .due = DUE_TOMORROW, .label = LABEL_ERRANDS },
            { .title = "Plan Sam's birthday dinner", .notes = "Table for eight, with a vegan menu.",
              .due = DUE_LATER, .label = LABEL_HOME, .starred = true },
            { .title = "Draft the 2027 roadmap", .due = DUE_LATER, .label = LABEL_WORK },
            { .title = "Learn to make sourdough", .due = DUE_SOMEDAY, .label = LABEL_HOME },
            { .title = "Water the plants", .label = LABEL_HOME, .done = true },
        },
        .count = 10, .selected = -1, .undoTask = -1, .toggleAsk = -1, .snackText = "", };

    rcSetStyle(app_style());
    /* The looks that repeat. A chip at rest is Material's gray 200, and gray 300 under the
       pointer. An underlined field is Material's: one rule along its foot, 2 px while it has
       focus, on square corners, its text flush with the rule's start. border-0 takes each side
       hairline's pixel out of the field's padding, so the class writes the field's own 6 px
       above the text and 10 px after it. An icon button's ink is its text color, which
       bg-current reads: under the pointer it washes in that ink at 12%. The FAB's class is
       named too, as its string runs past the 95 bytes one cached .className holds: the accent
       and its hover shade, and Material's elevation, deeper under the pointer. */
    rcDefineClass("chip", "bg-[#eeeeee] hover:bg-[#e0e0e0] select-none");
    rcDefineClass("underlined",
                  "pl-0 pt-1.5 pr-2.5 border-0 border-b rounded-none focus:border-b-2");
    rcDefineClass("gray-ink", "text-[#757575] hover:bg-current/12");
    rcDefineClass("pink-ink", "text-[#ff4081] hover:bg-current/12");
    rcDefineClass("fab", "bg-primary hover:bg-primary-hover shadow-[0_4px_8px_#00000050] "
                         "hover:shadow-[0_8px_16px_#0000006e]");

    RC_AppOptions opts = {
        .width = 1280, .height = 820, .title = "RayClay Tasks", .clearColor = PAGE,
        .frame = RC_WINDOW_FRAME_NONE, .layoutCallback = layout,
        .userData = &state, .titlebar = { .height = BAR_H }, };
    return rcRunApp(&opts);
}
