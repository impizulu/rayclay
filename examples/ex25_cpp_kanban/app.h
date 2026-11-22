/*
    app.h - the app's state, and the one place the board changes.

    The views never mutate a board. A click builds what the card or list should
    become and queues it as a Change; change_apply() runs the queue at the top of the
    next layout, before anything is declared, because the frame in flight still holds
    pointers into these strings and a vector insert would move them (main.cpp, rule 2).
    That single door is also where a change goes out to the server.
*/
#ifndef KANBAN_APP_H
#define KANBAN_APP_H

#include "rayclay.h"
#include "board.h"
#include "sync.h"

/* A value the board should take. A card or list with an id it does not know is new. */
struct Change {
    enum Kind { PUT_CARD, DROP_CARD, PUT_LIST, DROP_LIST, NEW_BOARD, OPEN_BOARD } kind = PUT_CARD;
    Card        card;
    List        list;
    std::string name;
    int         board = 0;
};

/* What the filter bar asks for; an empty filter shows everything. */
struct Filter {
    char     query[64] = "";
    unsigned labels = 0;
    unsigned members = 0;
    int      due = 0;           /* an index into DUE_FILTER */
};

static const char *const DUE_FILTER[] = { "Any date", "Overdue", "Due this week", "No date" };

/* One card being dragged. PRESSED waits for the pointer to travel, so a click stays a
   click. LIFTED follows the pointer and keeps a drop slot open in some list. LANDING
   flies the card from where it was let go into that slot. `gap` is the open slot, and
   `was` is the slot it just left, still closing: two boxes whose heights change together
   are what makes the neighbors slide instead of jump. */
struct DropSlot {
    int   list = -1;
    int   slot = 0;
    float height = 0.0f;
};

struct Drag {
    enum Phase { NONE, PRESSED, LIFTED, LANDING } phase = NONE;
    int      card = 0;
    int      fromList = 0;
    int      fromSlot = 0;
    RC_Vec2  press = { 0.0f, 0.0f };
    RC_Vec2  grab = { 0.0f, 0.0f };     /* the pointer inside the card */
    RC_Vec2  at = { 0.0f, 0.0f };       /* the floating card's top left */
    RC_Vec2  landFrom = { 0.0f, 0.0f };
    float    width = 0.0f, height = 0.0f;
    float    lift = 0.0f;               /* 0 at rest, 1 fully lifted */
    double   landStart = 0.0;
    double   lastFrame = 0.0;
    DropSlot gap, was;
};

struct AppState {
    std::vector<Board>  boards;
    int                 current = 0;
    int                 today = SAMPLE_DAY;     /* today's day number: main.cpp reads it */
    std::vector<Change> changes;
    Sync                sync;
    Filter              filter;
    Drag                drag;
    int                 landedCard = 0;         /* the card and list a move just reached */
    int                 landedList = 0;
    double              landedAt = 0.0;
    bool                labelText = false;      /* label strips show their names */
    bool                filtersOpen = false;    /* the filters, where they fold away */
    bool                switcherOpen = false;
    bool                creatingBoard = false;
    char                boardName[64] = "";
    int                 hoveredCard = 0;
    int                 hoveredList = 0;
    int                 tabList = 0;            /* the one-lane arm's list */
    bool                panning = false;
    float               panFrom = 0.0f;
    RC_Key              boardKey = RC_KEY_NONE; /* a key the board saw go down (main.cpp) */

    int  composerList = 0;                      /* the list whose "Add a card" is open */
    char composer[256] = "";
    bool addingList = false;
    int  revealFrames = 0;                      /* frames left to bring that form in view */
    size_t listsShown = 0;
    char newList[64] = "";
    int  renamingList = 0;
    char rename[64] = "";

    /* The card dialog works on buffers, committed on Enter, on blur and on close. */
    bool cardOpen = false;
    bool cardFresh = false;                     /* opened this frame: fill the buffers */
    int  cardId = 0;
    char title[256] = "";
    char description[2000] = "";
    char comment[1000] = "";
    char checkItem[200] = "";
    bool titleFocused = false;
    bool descriptionFocused = false;
    bool calendarOpen = false;
    int  calendarMonth = 0;

    bool syncOpen = false;
    bool helpOpen = false;

    std::string toast;
    double      toastUntil = 0.0;
    std::vector<Card> undoCards;
    List        undoList;
    bool        undoHasList = false;

    Board &board() { return boards[static_cast<size_t>(current)]; }
};

/* Sync covers the board the server was seeded with, the first one. */
static inline bool is_synced(const AppState &st) { return st.current == 0 && st.sync.on; }

/* A dialog is up. The snackbar gives way to it, as it is what the user turned to (view.h). */
static inline bool dialog_open(const AppState &st) {
    return st.cardOpen || st.syncOpen || st.helpOpen;
}

static inline void say(AppState &st, RC_App *app, const std::string &text) {
    st.toast = text;
    st.toastUntil = rcAppTime(app) + 5.0;
    st.undoCards.clear();
    st.undoHasList = false;
}

static inline void open_card(AppState &st, int id) {
    st.cardOpen  = true;
    st.cardFresh = true;
    st.cardId    = id;
}

static inline void put_card(AppState &st, Card card) {
    Change ch;
    ch.kind = Change::PUT_CARD;
    ch.card = std::move(card);
    st.changes.push_back(std::move(ch));
}

/* The card a change starts from: the board's copy, or the change already queued for it this
   frame. A click that also takes the focus from a field commits that field in the same frame,
   and two changes each copied from the board's card would leave the later one undoing the
   earlier, the text just typed lost. */
static inline Card latest(const AppState &st, const Card &c) {
    for (size_t i = st.changes.size(); i-- > 0;)
        if (st.changes[i].kind == Change::PUT_CARD && st.changes[i].card.id == c.id)
            return st.changes[i].card;
    return c;
}

static inline void drop_card(AppState &st, int id) {
    Change ch;
    ch.kind = Change::DROP_CARD;
    ch.card.id = id;
    st.changes.push_back(std::move(ch));
}

static inline void put_list(AppState &st, List list) {
    Change ch;
    ch.kind = Change::PUT_LIST;
    ch.list = std::move(list);
    st.changes.push_back(std::move(ch));
}

static inline void drop_list(AppState &st, int id) {
    Change ch;
    ch.kind = Change::DROP_LIST;
    ch.list.id = id;
    st.changes.push_back(std::move(ch));
}

static inline void open_board(AppState &st, int index) {
    Change ch;
    ch.kind = Change::OPEN_BOARD;
    ch.board = index;
    st.changes.push_back(std::move(ch));
}

static inline void new_board(AppState &st, const char *name) {
    Change ch;
    ch.kind = Change::NEW_BOARD;
    ch.name = name;
    st.changes.push_back(std::move(ch));
}

/* A move to `slot` of a list, keeping everything else on the card. However it was asked for,
   the card glows where it lands, so the eye finds where it went. When the slot's neighbors
   leave no double between them (pos_for_slot), the list is spaced out again first, each
   card a change of its own, as Sort by due date does. */
static inline void move_card(AppState &st, RC_App *app, const Card &c, int list, int slot) {
    const Board &b = st.board();
    bool fits = true;
    Card next = c;
    next.list = list;
    next.pos  = pos_for_slot(b, list, slot, c.id, &fits);
    if (!fits) {
        int k = 0;
        for (const Card &other : b.cards) {
            if (other.list != list || other.id == c.id)
                continue;
            Card spaced = other;
            spaced.pos = 1024.0 * (k < slot ? k + 1 : k + 2);
            put_card(st, std::move(spaced));
            k++;
        }
        next.pos = 1024.0 * (slot + 1);
    }
    put_card(st, std::move(next));
    st.landedCard = c.id;
    st.landedList = list;
    st.landedAt   = rcAppTime(app);
}

/* How much of the landing glow is left, from 1 down to 0. It asks for its next frame, because
   the app parks once the pointer stops. */
static inline float landing_glow(const AppState &st, RC_App *app) {
    const double left = 1.0 - (rcAppTime(app) - st.landedAt) / 0.6;
    if (left <= 0.0)
        return 0.0f;
    rcWindowRequestFrame(rcAppMainWindow(app));
    return static_cast<float>(left);
}

/* The server has named a card the app added: whatever held the temporary id follows it, so
   an open dialog, a drag or a queued change stays on that card. */
static inline void follow_new_ids(AppState &st) {
    for (const NewId &n : st.sync.newIds) {
        if (st.cardId == n.from)
            st.cardId = n.to;
        if (st.drag.card == n.from)
            st.drag.card = n.to;
        if (st.landedCard == n.from)
            st.landedCard = n.to;
        for (Change &ch : st.changes)
            if (ch.card.id == n.from)
                ch.card.id = n.to;
    }
    st.sync.newIds.clear();
}

static inline void apply_put_card(AppState &st, RC_App *app, Board &b, Card next) {
    Card *mine = next.id ? find_card(b, next.id) : nullptr;
    if (!mine) {
        /* Its id is the board's next, or a temporary one below 0 until the server names it. */
        if (is_synced(st))
            next.id = --st.sync.lastTemp;
        else if (!next.id)
            next.id = b.nextCard++;
        next.version = 0;
        sync_new_card(app, st.sync, place_card(b, std::move(next)));
        return;
    }
    Card before = *mine;
    next.version = before.version;
    remove_card(b, before.id);
    sync_card(app, st.sync, before, place_card(b, std::move(next)));
}

static inline void apply_drop_card(AppState &st, RC_App *app, Board &b, int id) {
    Card *mine = find_card(b, id);
    if (!mine)
        return;
    Card before = *mine;
    remove_card(b, id);
    sync_drop_card(app, st.sync, before);
    say(st, app, "\"" + before.title + "\" was deleted.");
    st.undoCards.push_back(std::move(before));
}

static inline void apply_put_list(AppState &st, RC_App *app, Board &b, List next) {
    List *mine = next.id ? find_list(b, next.id) : nullptr;
    if (!mine) {
        if (is_synced(st)) {
            sync_new_list(app, st.sync, next);   /* it appears with the server's answer */
            return;
        }
        if (!next.id)
            next.id = b.nextList++;
        sync_new_list(app, st.sync, place_list(b, std::move(next)));
        return;
    }
    List before = *mine;
    remove_list(b, before.id);
    sync_list(app, st.sync, before, place_list(b, std::move(next)));
}

static inline void apply_drop_list(AppState &st, RC_App *app, Board &b, int id) {
    List *mine = find_list(b, id);
    if (!mine)
        return;
    List before = *mine;
    std::vector<Card> cards;
    for (const Card &c : b.cards)
        if (c.list == id)
            cards.push_back(c);
    for (const Card &c : cards)
        remove_card(b, c.id);
    remove_list(b, id);
    sync_drop_list(app, st.sync, before, cards);
    say(st, app, "The list \"" + before.title + "\" was deleted.");
    /* A server names a restored list anew, and its cards could not follow in one
       request, so Undo stays with the boards on this device. */
    if (!is_synced(st)) {
        st.undoList    = before;
        st.undoHasList = true;
        st.undoCards   = std::move(cards);
    }
}

/* A new board starts with Trello's three lists and the next background in line. */
static inline void apply_new_board(AppState &st, const std::string &name) {
    static const unsigned BACKGROUND[][2] = { { 0x4C1D95, 0xDB2777 }, { 0x1E3A8A, 0x0891B2 },
                                              { 0x7C2D12, 0xCA8A04 }, { 0x134E4A, 0x2563EB } };
    const size_t pick = st.boards.size() % 4;
    Board b;
    b.name   = name;
    b.prefix = "NEW";
    b.from   = rcHex(BACKGROUND[pick][0]);
    b.to     = rcHex(BACKGROUND[pick][1]);
    const char *const names[LABEL_COUNT] = { "Green", "Yellow", "Orange", "Red", "Purple", "Blue" };
    for (int i = 0; i < LABEL_COUNT; i++)
        b.labelName[i] = names[i];
    seed_lists(b, { "To do", "Doing", "Done" });
    st.boards.push_back(std::move(b));
    st.current = static_cast<int>(st.boards.size()) - 1;
}

/* Run the queue. Everything that can move a string happens here, at the top of a
   frame, with nothing on loan to the frame before it. */
static inline void change_apply(AppState &st, RC_App *app) {
    std::vector<Change> queue = std::move(st.changes);
    st.changes.clear();
    for (Change &ch : queue) {
        Board &b = st.board();
        st.sync.live = is_synced(st);
        switch (ch.kind) {
        case Change::PUT_CARD:   apply_put_card(st, app, b, std::move(ch.card)); break;
        case Change::DROP_CARD:  apply_drop_card(st, app, b, ch.card.id); break;
        case Change::PUT_LIST:   apply_put_list(st, app, b, std::move(ch.list)); break;
        case Change::DROP_LIST:  apply_drop_list(st, app, b, ch.list.id); break;
        case Change::NEW_BOARD:
        case Change::OPEN_BOARD:
            /* A drag names its card and lists by this board's ids, and every board counts
               its own from 1: a landing carried over to the next board would move that
               board's card of the same number. The gesture ends with its board, and so does
               the snackbar's Undo, which would put a deleted card or list back over the
               next board's of the same number. */
            if (ch.kind == Change::NEW_BOARD || ch.board != st.current) {
                st.drag       = Drag{};
                st.landedCard = 0;
                st.undoCards.clear();
                st.undoHasList = false;
            }
            if (ch.kind == Change::NEW_BOARD)
                apply_new_board(st, ch.name);
            else
                st.current = ch.board;
            break;
        }
    }
}

/* Put back what the snackbar's Undo offers. A restored card is new to a server, so it
   comes back through the same door a new card takes. */
static inline void undo_delete(AppState &st) {
    if (st.undoHasList)
        put_list(st, st.undoList);
    for (const Card &c : st.undoCards)
        put_card(st, c);
    st.undoCards.clear();
    st.undoHasList = false;
    st.toast.clear();
}

/* ---- The filter ---- */

static inline char lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 'a' - 'A') : c;
}

static inline bool contains_text(const std::string &text, const char *query) {
    if (!query[0])
        return true;
    for (size_t i = 0; i < text.size(); i++) {
        size_t k = 0;
        while (query[k] && i + k < text.size() && lower(text[i + k]) == lower(query[k]))
            k++;
        if (!query[k])
            return true;
    }
    return false;
}

static inline bool filter_on(const Filter &f) {
    return f.query[0] || f.labels || f.members || f.due;
}

static inline bool card_matches(const Filter &f, const Card &c, int today) {
    if (!contains_text(c.title, f.query))
        return false;
    if (f.labels && !(c.labels & f.labels))
        return false;
    if (f.members && !(c.members & f.members))
        return false;
    switch (f.due) {
    case 1:  return c.due && c.due < today && !c.dueDone;
    case 2:  return c.due >= today && c.due <= today + 7;
    case 3:  return !c.due;
    default: return true;
    }
}

static inline void filter_clear(Filter &f) {
    f = Filter{};
}

#endif /* KANBAN_APP_H */
