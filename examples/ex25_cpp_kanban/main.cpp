/*
    main.cpp - Laneway: a Trello-style kanban board written in C++.

    Boards with gradient backgrounds, lists, cards with label strips, avatars and due
    dates, a card dialog, a filter bar, and real drag and drop: pick up a card, watch the
    neighbors slide apart, let go and it lands in the open slot. Right-click or long-press
    a card to move it without a drag, which is how a finger moves one.

    RayClay is C99; your application does not have to be. The header is wrapped in
    extern "C" and the element DSL is the same text in both languages. Four differences
    bite, and all four are visible in this code:

      1. DESIGNATED INITIALIZERS FOLLOW DECLARATION ORDER in C++20, and g++ hard-errors
         where clang++ only warns. Every rcRow, rcColumn and rcBox, and the RC_AppOptions
         below, list their fields in rayclay.h's order.
      2. RAYCLAY BORROWS YOUR STRINGS UNTIL THE FRAME IS DRAWN, which is after your layout
         callback returns. A std::string in a vector is fine to hand over as long as the
         vector does not move while the frame is in flight. So the views never change a
         board: a click queues a Change, applied at the top of the next layout (app.h).
      3. A CAPTURE-LESS LAMBDA IS A C FUNCTION POINTER: it converts to RC_LayoutCallback,
         and userData carries the AppState across.
      4. ARRAY DESIGNATORS ARE C ONLY: every table here is positional.

    THE REST FOLD. The board can live on a server, and the same app teaches the network:
    rcFetch for each change, the JSON reader and writer, one event stream that keeps two
    windows in step, optimistic changes that roll back when a request fails, and a log of
    every request. Due dates cross the wire as RFC 3339 (rcTimeParse), and a field the app
    does not read goes back whole (rcJsonWriteValue). It is off until a URL is set in the
    Sync dialog, so a fresh run opens no socket. Start the server that comes with it, then
    press Connect:

        python3 examples/ex25_cpp_kanban/serve.py

    The dialog's Next request choice makes one request slow or fail, and Drop stream cuts the
    event stream, so a rollback and a reconnect can be seen on demand. On Android, Back with
    nothing open leaves the app, which is also in this file.

    Read board.h for the data, app.h for how it changes, view.h for the board on screen
    (the drag is the part to copy), dialogs.h for the modals and sync.h for the network.
    Linking the rayclay target sets C++20 for your C++ TUs and, under g++ and clang, adds
    -Wno-missing-field-initializers.

    Build target: rayclay_ex25_cpp_kanban
*/

#include "rayclay.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "board.h"
#include "sync.h"
#include "app.h"
#include "view.h"
#include "dialogs.h"

/* Today's day number: the date on the user's own clock and calendar (rcLocalTime's offset
   moves the wall clock into the user's zone). A clock with no reading (0), a system with no
   answer, or a date before Feb 2026 or past the calendar's end keeps the sample's day. */
static int today_from_clock() {
    const double now = rcUnixTimeSeconds();
    RC_LocalTime local;
    if (now <= 0.0 || !rcLocalTime(now, &local))
        return SAMPLE_DAY;
    const int day = day_of_unix(now + local.utcOffsetSeconds);
    return day > 31 && day <= LAST_DAY ? day : SAMPLE_DAY;
}

/* True while a keystroke belongs to a text field rather than to the board. */
static bool typing() {
    static const char *const FIELDS[] = { "filter-query", "composer", "new-list", "rename",
                                          "new-board" };
    if (rcIsModalOpen())
        return true;
    for (const char *id : FIELDS)
        if (rcIsFocused(id))
            return true;
    return false;
}

/* A key that opens a text field acts on its release, because the field takes the character of
   a key still going down. A release counts only after a press the board saw, so a key held
   over from typing in a field is not a shortcut when that field closes. */
static bool board_key_up(AppState &st, RC_Key key) {
    if (rcKeyPressed(key))
        st.boardKey = key;
    if (!rcKeyReleased(key) || st.boardKey != key)
        return false;
    st.boardKey = RC_KEY_NONE;
    return true;
}

/* Trello's single-key shortcuts, with the pointer as the selection. A chord belongs to the
   system. */
static void shortcuts(AppState &st, RC_App *app) {
    Board &b = st.board();
    if (typing() || st.drag.phase != Drag::NONE) {
        st.boardKey = RC_KEY_NONE;
        return;
    }
    if (rcModDown(RC_MOD_CTRL) || rcModDown(RC_MOD_ALT) || rcModDown(RC_MOD_SUPER))
        return;
    const Card *hovered = find_card(b, st.hoveredCard);

    if (rcKeyPressed(RC_KEY_SLASH) && rcModDown(RC_MOD_SHIFT)) {
        st.helpOpen = true;
    } else if (board_key_up(st, RC_KEY_F) || board_key_up(st, RC_KEY_SLASH)) {
        st.filtersOpen = true;
        rcSetFocus("filter-query");
    } else if (board_key_up(st, RC_KEY_N)) {
        if (!b.lists.empty())
            open_composer(st, find_list(b, st.hoveredList) ? st.hoveredList : b.lists.front().id);
    } else if (rcKeyPressed(RC_KEY_X)) {
        filter_clear(st.filter);
    } else if (rcKeyPressed(RC_KEY_Q)) {
        st.filter.members ^= 1u << ME;
    } else if (rcKeyPressed(RC_KEY_B)) {
        st.switcherOpen = !st.switcherOpen;
    } else if (rcKeyPressed(RC_KEY_L)) {
        st.labelText = !st.labelText;
    } else if (hovered && rcKeyPressed(RC_KEY_ENTER)) {
        open_card(st, hovered->id);
    } else if (hovered && (rcKeyPressed(RC_KEY_COMMA) || rcKeyPressed(RC_KEY_PERIOD))) {
        const int step = rcKeyPressed(RC_KEY_COMMA) ? -1 : 1;
        for (size_t i = 0; i < b.lists.size(); i++) {
            if (b.lists[i].id != hovered->list)
                continue;
            const long to = static_cast<long>(i) + step;
            if (to >= 0 && to < static_cast<long>(b.lists.size()))
                fly_card(st, app, *hovered, b.lists[static_cast<size_t>(to)].id);
            break;
        }
    } else if (st.filtersOpen && rcKeyPressed(RC_KEY_ESCAPE)) {
        st.filtersOpen = false;   /* unread with nothing open: Android's Back stays the system's */
    }
}

static void layout(RC_App *app, void *userData) {
    AppState &st = *static_cast<AppState *>(userData);

    const RC_Viewport view = rcViewport();
    st.today = today_from_clock();      /* the first frame after midnight reads the new day */
    /* An idle app draws no frame at midnight by itself, so it asks for the one a second past
       it. Only the earliest request is kept, so every frame asks again: the snackbar's sooner
       one wins while it shows, and the frame that ends it asks for midnight once more. */
    const double now = rcUnixTimeSeconds();
    RC_LocalTime local;
    if (now > 0.0 && rcLocalTime(now, &local))
        rcWindowRequestFrameAfter(rcAppMainWindow(app), 86401.0 - local.hour * 3600.0
                                                        - local.minute * 60.0 - local.second);

    /* Everything that can move a string, before anything is declared (rule 2). The drag
       and the keys go first, so a drop lands in the frame it finishes. */
    sync_poll(app, st.sync, st.boards.front());
    follow_new_ids(st);
    drag_update(st, app, view);
    shortcuts(st, app);
    change_apply(st, app);
    /* A notice waits for the dialog to close: under its scrim it could not be read. */
    if (!st.sync.notice.empty() && !dialog_open(st)) {
        say(st, app, st.sync.notice);
        st.sync.notice.clear();
    }
    st.hoveredCard = 0;
    st.hoveredList = 0;

    const Board &b = st.board();
    rcColumn(.id = "root", .w = "grow", .h = "grow",
             .gradient = { .from = b.from, .to = b.to, .dir = "d" }) {
        top_bar(st, app, view);
        filter_bar(st, app, view);
        board_area(st, app, view);
        drag_float(st, app);
        snackbar(st, app, view);
        card_dialog(st, app, view);
        sync_dialog(st, app, view);
        help_dialog(st, view);
    }

    /* A queued change is only correct if a frame follows, and nothing else wakes an
       on-demand app once the pointer stops moving. */
    if (!st.changes.empty())
        rcWindowRequestFrame(rcAppMainWindow(app));
}

/* Trello's light controls; the board's own ground comes from its gradient. A tooltip draws
   a `surface` box with `text` ink, so those stay light and dark. */
static RC_Style app_style() {
    RC_Style s = rcStyleLight();
    s.background   = PAL.lane;
    s.surface      = RC_WHITE;
    s.surfaceAlt   = PAL.lane;
    s.text         = PAL.ink;
    s.textMuted    = PAL.muted;
    s.primary      = PAL.accent;
    s.primaryHover = PAL.accent2;
    s.border       = PAL.line;
    s.danger       = PAL.danger;
    s.radius       = 6.0f;
    return s;
}

int main() {
    static AppState state;  /* the request log and the dialog's buffers: not on the stack */
    state.boards.push_back(atlas_board());
    state.boards.push_back(website_board());
    state.boards.push_back(kitchen_board());
    state.today = today_from_clock();
    for (Board &b : state.boards)
        shift_days(b, state.today - SAMPLE_DAY);

    rcSetStyle(app_style());
    define_classes();

    /* Font slot 0 is the size every library control and tooltip draws its text at. */
    static const float BODY[] = { 14.0f };

    /* Brace form, fields in declaration order (rule 1). The callback is a capture-less
       lambda and userData brings the AppState back (rule 3). */
    RC_AppOptions opts{
        .width             = 1280,
        .height            = 820,
        .title             = "Laneway - kanban boards",
        .clearColor        = rcHex(0x0B4FA8),
        .fontSizes         = BODY,
        .fontCount         = 1,
        .scratchArenaBytes = 256 * 1024,
        .layoutCallback    = [](RC_App *app, void *userData) { layout(app, userData); },
        .userData          = &state,
        .minWidth          = 360,
        .minHeight         = 400,
    };
    return rcRunApp(&opts);
}
