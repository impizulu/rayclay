/*
    view.h - the board on screen: the bars over it, its lists, its cards and the drag.

    All of it is declared every frame from the AppState struct, and nothing here changes a board
    directly: a click queues a Change (app.h). Sizes are plain CSS numbers written at each
    call, because RayClay bakes a size the first time it is used.
*/
#ifndef KANBAN_VIEW_H
#define KANBAN_VIEW_H

#include "rayclay.h"
#include "app.h"

#include "icons/rc_icons_check.h"
#include "icons/rc_icons_chevron_down.h"
#include "icons/rc_icons_chevron_left.h"
#include "icons/rc_icons_chevron_right.h"
#include "icons/rc_icons_circle_check.h"
#include "icons/rc_icons_clock.h"
#include "icons/rc_icons_ellipsis_vertical.h"
#include "icons/rc_icons_file_text.h"
#include "icons/rc_icons_info.h"
#include "icons/rc_icons_list.h"
#include "icons/rc_icons_message_square.h"
#include "icons/rc_icons_plus.h"
#include "icons/rc_icons_rotate_cw.h"
#include "icons/rc_icons_search.h"
#include "icons/rc_icons_star.h"
#include "icons/rc_icons_star_filled.h"
#include "icons/rc_icons_trash_2.h"
#include "icons/rc_icons_user.h"
#include "icons/rc_icons_x.h"

/* The colors are Trello's: dark blue ink on cool gray lists and white cards. The chrome reads
   them from PAL, and app_style (main.cpp) hands the library's controls theirs, so a class names
   those by the theme's roles: surface-alt is the lane, border the line, danger the red. A
   status color (a due date's, a priority's, a star's, the sync dot's) is spelled where it is
   decided. */
struct Palette {
    RC_Color ink, ink2, muted;
    RC_Color line, lane;
    RC_Color accent, accent2;
    RC_Color shade;     /* every shadow's navy */
    RC_Color danger;
};

static inline Palette app_palette() {
    return {
        .ink      = rcHex(0x172B4D),
        .ink2     = rcHex(0x44546F),
        .muted    = rcHex(0x626F86),
        .line     = rcHex(0xDCDFE4),
        .lane     = rcHex(0xF1F2F4),
        .accent   = rcHex(0x0C66E4),
        .accent2  = rcHex(0x0055CC),
        .shade    = rcHex(0x091E42),
        .danger   = rcHex(0xAE2E24),
    };
}

/* The board has one look, so the whole app reads one palette. */
static const Palette PAL = app_palette();

static const RC_Shadow CARD_SHADOW  = { rcWithAlpha(PAL.shade, 0.25f), 0.0f, 1.0f, 2.0f, 0.0f };
static const RC_Shadow POPUP_SHADOW = { rcWithAlpha(PAL.shade, 0.353f), 0.0f, 8.0f, 24.0f, 0.0f };

/* Under this width one list shows at a time, behind tabs, as on Trello's phone app. */
constexpr float ONE_LANE_W = 600.0f;
/* Under this width, or this height (a phone on its side), the top bar drops its wordmark and
   words. */
constexpr float ROOMY_W = 760.0f;
constexpr float ROOMY_H = 440.0f;
/* From this width the filter bar fits one row, its Clear and its count included, and stays
   up. Under it, or where the bar is not roomy, the filters fold away behind the top bar's
   filter button, as Trello keeps its own behind one, and open on a row that wraps. */
constexpr float FILTER_ROW_W = 1240.0f;
/* The "Add another list" column at a board's right end. */
constexpr float ADD_LIST_W = 176.0f;

static inline bool one_lane(const RC_Viewport &view) { return view.width < ONE_LANE_W; }
static inline bool roomy(const RC_Viewport &view) {
    return view.width >= ROOMY_W && view.height >= ROOMY_H;
}

/* A finger needs a taller target: the same rule rcButton and rcCombo keep. */
static inline uint16_t control_h() { return rcPointerIsCoarse() ? 44 : 32; }

/* That height in a control's .className, where the rest of its look goes with it: one string
   for a mouse and one for a finger. */
static inline const char *by_pointer(const char *mouse, const char *finger) {
    return rcPointerIsCoarse() ? finger : mouse;
}

static inline const char *control_class() { return by_pointer("h-8", "h-11"); }

/* The looks that repeat, named once at start-up, as Tailwind's @apply names them (main.cpp).
   A glass control sits on a board's gradient: a translucent white fill, lighter under the
   pointer and lighter again while it is pressed. Held on, it keeps the pressed fill. */
static inline void define_classes() {
    rcDefineClass("glass", "px-2 icon-4 text-white font-semibold "
                           "bg-white/15 hover:bg-white/25 active:bg-white/35");
    rcDefineClass("glass-on", "glass bg-white/35");
    /* A picked row's fill: the open board in the switcher, the request shown in the log. */
    rcDefineClass("row-picked", "bg-[#e9f2ff]");
}

static inline float px(float value) { return static_cast<float>(static_cast<uint16_t>(value)); }

static inline const char *card_id(RC_App *app, int id) {
    return rcFormat(rcAppArena(app), "card%d", id).chars;
}

/* ---- Small parts ---- */

/* The app's chrome text: one line, and not selectable, as a button's label is not. */
static inline void small_text(const char *text, RC_Color color = PAL.muted) {
    rcTextC(text, .color = color, .size = 12, .wrap = "n", .select = RC_SELECT_NONE);
}

static inline void strong_text(const char *text, float size = 14.0f, RC_Color color = PAL.ink) {
    rcTextC(text, .font = rcFontSemibold(size), .color = color, .wrap = "n",
            .select = RC_SELECT_NONE);
}

static inline void section_label(const char *text) { strong_text(text, 12.0f, PAL.muted); }

static inline void rule() {
    rcBox(.bg = PAL.line, .w = "grow", .h = "1px") {}
}

/* A round face with initials. `ring` marks a member a filter has picked. */
static inline void avatar(int member, float size, bool ring, bool dim) {
    const Member &m = MEMBERS[member];
    rcBox(.bg = dim ? rcWithAlpha(m.color, 0.43f) : m.color, .align = "cc",
          .borderRadius = "all-full",
          .border = { .color = ring ? RC_WHITE : RC_UNSET_COLOR, .width = 2 },
          .wType = RC_PX(size), .hType = RC_PX(size)) {
        strong_text(m.initials, size >= 28.0f ? 12.0f : 11.0f, RC_WHITE);
    }
}

/* The gradient thumbnail a board goes by in the switcher. */
static inline void board_thumb(const Board &b, float w, float h) {
    rcBox(.borderRadius = "all-sm", .wType = RC_PX(w), .hType = RC_PX(h),
          .gradient = { .from = b.from, .to = b.to, .dir = "d" }) {}
}

/* A press focuses a Tab stop, ring hidden, and a focused stop answers Enter and Space itself:
   after a click on help and the dialog's close, which gives the focus back, Enter would open
   help again, not the card under the pointer. So a pointer's click on the bars' controls lets
   the focus go, as rcButton's notes advise, and a key's click keeps it, so Tab goes on from
   there. A caller's rcSetFocus after this still wins. A finger's tap still focuses what it
   tapped, at the end of the layout, but no card lies under a finger. */
static inline bool unfocus_on_pointer(bool clicked) {
    if (clicked && rcPointerReleased(RC_POINTER_LEFT))
        rcSetFocus(nullptr);
    return clicked;
}

/* A button in the bars over the gradient: an rcButton whose class, glass, gives it a fill that
   beats the variant's own, and glass-on while it is held `on`. Alone, its icon stands in a
   square of the bar's control height. The box around it carries the tooltip, as
   icon_button's does, and keeps a label on one line where a bar runs short. */
static inline bool glass_button(const char *id, const char *text, RC_IconCallback icon,
                                const char *tip = nullptr, bool on = false) {
    static const char *const LOOK[2][2] = {     /* [under a finger][held on] */
        { "h-8 min-w-8 glass", "h-8 min-w-8 glass-on" },
        { "h-11 min-w-11 glass", "h-11 min-w-11 glass-on" },
    };
    bool clicked = false;
    rcBox(.tooltip = tip, .className = "shrink-0") {
        clicked = rcButton(id, text, .className = LOOK[rcPointerIsCoarse()][on],
                           .variant = RC_BTN_GHOST, .leadingIcon = icon);
    }
    return unfocus_on_pointer(clicked);
}

/* A ghost control's fill under the pointer is the theme's surface-alt, which is the lane's
   own gray, so on a lane it takes the line's, as a default button's hover does. */
static inline const char *lane_ghost_class() {
    return by_pointer("h-8 hover:bg-border", "h-11 hover:bg-border");
}

/* An icon alone on a light ground: a composer's close, a dialog's close. rcButton has no
   tooltip of its own, so the box around it carries one. */
static inline bool icon_button(const char *id, RC_IconCallback icon, const char *tip,
                               bool onLane = false) {
    bool clicked = false;
    rcBox(.tooltip = tip) {
        clicked = rcButton(id, "", .className = onLane ? lane_ghost_class() : control_class(),
                           .variant = RC_BTN_GHOST, .leadingIcon = icon);
    }
    return clicked;
}

/* A card reads AT-12 once it has an id, and AT- while the server has yet to name it. */
static inline const char *card_code(RC_Arena *mem, const Board &b, const Card &c) {
    return c.id > 0 ? rcFormat(mem, "%s-%d", b.prefix, c.id).chars
                    : rcFormat(mem, "%s-", b.prefix).chars;
}

/* ---- The card ---- */

/* Linear's priority mark: three bars that fill with the priority, red when urgent. */
static inline void priority_bars(int priority) {
    const RC_Color on = priority == 4 ? rcHex(0xC9372C) : PAL.ink2;
    rcRow(.gap = 2, .align = "br", .h = "12px") {
        for (int i = 0; i < 3; i++) {
            const float h = 5.0f + 3.5f * static_cast<float>(i);
            rcBox(.bg = priority > i ? on : PAL.line, .borderRadius = "all-xs", .w = "3px",
                  .hType = RC_PX(h)) {}
        }
    }
}

/* The due date chip: red once late, yellow by tomorrow, green once done. */
static inline void due_chip(RC_App *app, const Card &c, int today) {
    const Date d = date_of(c.due);
    RC_Color bg = RC_UNSET_COLOR, ink = PAL.ink2;
    if (c.dueDone) {
        bg  = rcHex(0x4BCE97);
        ink = rcHex(0x164B35);
    } else if (c.due < today) {
        bg  = rcHex(0xFFD5D2);
        ink = PAL.danger;
    } else if (c.due <= today + 1) {
        bg  = rcHex(0xF8E6A0);
        ink = rcHex(0x533F04);
    }
    rcRow(.bg = bg, .gap = 4, .px = 4, .py = 2, .align = "cl", .borderRadius = "all-sm") {
        if (c.dueDone)
            rcIconCircleCheck(14.0f, ink);
        else
            rcIconClock(14.0f, ink);
        small_text(rcFormat(rcAppArena(app), "%s %d", MONTH_NAME[d.month], d.day).chars, ink);
    }
}

static inline void badge(RC_IconCallback icon, const char *text) {
    rcRow(.gap = 4, .align = "cl") {
        icon(14.0f, PAL.ink2);
        small_text(text, PAL.ink2);
    }
}

/* The grip a finger drags a card by, where a finger elsewhere on the card pans the list: its
   .captureTouch keeps the finger's drag either way from the list's scroll, as CSS's
   touch-action: none, so the drag reads rcPointer as a mouse's does. The copy under the
   finger draws it with no id and claims nothing. */
static inline void card_grip(const char *id) {
    rcColumn(.id = id, .gap = 3, .align = "cc",
             .captureTouch = id ? RC_TOUCH_AXIS_BOTH : RC_TouchAxis{}, .w = "44px", .h = "44px") {
        for (int r = 0; r < 3; r++) {
            rcRow(.gap = 3) {
                rcBox(.bg = PAL.ink2, .borderRadius = "all-full", .w = "4px", .h = "4px") {}
                rcBox(.bg = PAL.ink2, .borderRadius = "all-full", .w = "4px", .h = "4px") {}
            }
        }
    }
}

static inline const char *grip_id(RC_App *app, const Card &c) {
    return rcFormat(rcAppArena(app), "grip%d", c.id).chars;
}

/* What every card shows, in its list or under the pointer. A card's text is never
   selectable, because a press on it starts a drag. */
static inline void card_body(AppState &st, RC_App *app, const Board &b, const Card &c, bool live) {
    RC_Arena *mem = rcAppArena(app);
    const int items = static_cast<int>(c.checklist.size());
    int checked = 0;
    for (const CheckItem &item : c.checklist)
        checked += item.done;
    const bool badges = c.due || c.priority || !c.description.empty() || !c.comments.empty()
                        || !c.checklist.empty() || c.members;
    const char *stripsId = live ? rcFormat(mem, "strips%d", c.id).chars : nullptr;

    if (c.cover >= 0)
        rcBox(.bg = LABEL_COLOR[c.cover], .borderRadius = "t-lg", .w = "grow", .h = "32px") {}
    rcColumn(.gap = 4, .pt = 8, .pb = 8, .px = 12, .w = "grow") {
        /* Centered: the code reads on the strips' line, 8 px bars or named chips alike. */
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            /* The strips are one target: a click shows every label's name, as on Trello. The
               blank beside them belongs to the card. They are no Tab stop, as the card is
               none: the pointer is the selection here, and the keyboard's way is l. */
            if (c.labels) {
                rcRow(.id = stripsId, .gap = 4, .align = "cl", .className = "flex-wrap") {
                    for (int i = 0; i < LABEL_COUNT; i++) {
                        if (!(c.labels & (1u << i)))
                            continue;
                        if (st.labelText) {
                            rcBox(.bg = LABEL_COLOR[i], .px = 8, .align = "cl",
                                  .borderRadius = "all-sm", .h = "16px") {
                                strong_text(b.labelName[i], 11.0f);
                            }
                        } else {
                            rcBox(.bg = LABEL_COLOR[i], .borderRadius = "all-sm", .w = "32px",
                                  .h = "8px") {}
                        }
                    }
                }
            }
            rcSpacer() {}
            small_text(card_code(mem, b, c));
            if (rcPointerIsCoarse())
                card_grip(live ? grip_id(app, c) : nullptr);
        }
        if (live && c.labels && rcClicked(stripsId))
            st.labelText = !st.labelText;
        /* What a user typed keeps its words whole, and a pasted URL still has to fit a narrow
           card: .wordBreak "a" is CSS's overflow-wrap: anywhere, which splits only a word too
           long for a line of its own. */
        rcTextC(c.title.c_str(), .color = PAL.ink, .size = 14, .select = RC_SELECT_NONE,
                .wordBreak = "a");
        if (badges) {
            /* The faces wrap under the badges when both do not fit, as on Trello. */
            rcRow(.gap = 8, .pt = 4, .align = "cl", .w = "grow", .className = "flex-wrap") {
                rcRow(.gap = 8, .align = "cl") {
                    if (c.priority)
                        priority_bars(c.priority);
                    if (c.due)
                        due_chip(app, c, st.today);
                    if (!c.description.empty())
                        rcIconList(14.0f, PAL.ink2);
                    if (!c.comments.empty())
                        badge(rcIconMessageSquare,
                              rcFormat(mem, "%d", static_cast<int>(c.comments.size())).chars);
                    if (items)
                        badge(rcIconCircleCheck, rcFormat(mem, "%d/%d", checked, items).chars);
                }
                rcRow(.gap = 4, .align = "cr", .w = "grow") {
                    for (int i = 0; i < MEMBER_COUNT; i++)
                        if (c.members & (1u << i))
                            avatar(i, 24.0f, false, false);
                }
            }
        }
    }
}

static inline void drag_land(AppState &st, RC_App *app) {
    Drag &d = st.drag;
    d.phase     = Drag::LANDING;
    d.landFrom  = d.at;
    d.landStart = rcAppTime(app);
}

/* The press has traveled: the card lifts. The slot it left stays open at full height, so
   nothing else moves yet. */
static inline void drag_lift(Drag &d, const Board &b, const Card &c) {
    d.phase    = Drag::LIFTED;
    d.fromList = c.list;
    d.fromSlot = 0;
    for (const Card &other : b.cards) {
        if (other.id == c.id)
            break;
        d.fromSlot += other.list == c.list;
    }
    d.gap.list   = c.list;
    d.gap.slot   = d.fromSlot;
    d.gap.height = d.height + 8.0f;
    d.was        = DropSlot{};
}

/* A move from a menu or a key takes the end of a drag's flight: the card rises out of its slot
   and lands at the foot of the other list while the neighbors slide. A list not on screen, as
   on the one-lane arm, gets the move and its glow alone. */
static inline void fly_card(AppState &st, RC_App *app, const Card &c, int list) {
    Board &b = st.board();
    const RC_Box from   = rcGetElementBox(card_id(app, c.id));
    const char  *laneId = rcFormat(rcAppArena(app), "lane%d", list).chars;
    const int    slot   = cards_in(b, list);
    /* One flight at a time: the drag holds one card, and a menu's move started over a landing
       still in the air would reset it, and the move it was flying to would never be made. */
    if (!from.found || !rcGetElementBox(laneId).found || st.drag.phase != Drag::NONE) {
        move_card(st, app, c, list, slot);
        return;
    }
    Drag &d = st.drag;
    d = Drag{};
    d.card   = c.id;
    d.at     = { from.x, from.y };
    d.width  = from.width;
    d.height = from.height;
    d.lift   = 1.0f;
    drag_lift(d, b, c);
    d.was = d.gap;
    d.gap = { list, slot, 1.0f };
    drag_land(st, app);
    rcScrollToBottom(laneId);
}

/* A card's right-click or long-press menu: every way to move a card without a drag, which
   is how a finger moves one. */
static inline void card_menu(AppState &st, RC_App *app, const Card &c, const char *id) {
    Board &b = st.board();
    RC_Arena *mem = rcAppArena(app);
    const bool mine = c.members & (1u << ME);
    if (!rcBeginContextMenu(rcFormat(mem, "cardmenu%d", c.id).chars, id))
        return;
    if (rcMenuItem("Open card", .leadingIcon = rcIconFileText))
        open_card(st, c.id);
    if (rcMenuItem(mine ? "Leave card" : "Join card", .leadingIcon = rcIconUser)) {
        Card next = c;
        next.members ^= 1u << ME;
        put_card(st, std::move(next));
    }
    for (const List &l : b.lists) {
        if (l.id == c.list)
            continue;
        const char *label = rcFormat(mem, "Move to %s", l.title.c_str()).chars;
        if (rcMenuItem(label, .leadingIcon = rcIconChevronRight))
            fly_card(st, app, c, l.id);
    }
    if (rcMenuItem("Delete card", .className = "text-danger hover:bg-danger/10",
                   .leadingIcon = rcIconTrash2))
        drop_card(st, c.id);
    rcEndContextMenu();
}

/* One card in its list. A press arms the drag, and the same press released without traveling
   is rcClicked's click, which opens it. A finger pans the list from the card and drags it by
   its grip; a tap opens it, and the menu above still moves it without a drag. */
static inline void card_in_list(AppState &st, RC_App *app, const Board &b, const Card &c) {
    const char *id = card_id(app, c.id);
    Drag &d = st.drag;
    const bool hot = d.phase <= Drag::PRESSED && rcShowsHover(id);
    const float glow = c.id == st.landedCard ? landing_glow(st, app) : 0.0f;
    const RC_Color edge = hot ? PAL.accent : rcWithAlpha(PAL.accent, glow);

    if (rcIsHovered(id))
        st.hoveredCard = c.id;
    rcColumn(.id = id, .bg = RC_WHITE, .borderRadius = "all-lg", .w = "grow",
             .shadow = CARD_SHADOW) {
        card_body(st, app, b, c, true);
        /* The edge lies over the card, as CSS draws an inset ring: a border would take room
           inside the card, and a cover would stop short of its sides. */
        if (hot || glow > 0.0f)
            rcBox(.borderRadius = "all-lg", .border = { .color = edge, .width = 2 },
                  .w = "grow", .h = "grow",
                  .floating = { .to = RC_ATTACH_PARENT, .capture = RC_CAPTURE_PASSTHROUGH }) {}
    }
    if (d.phase == Drag::NONE && rcPressed(rcPointerIsCoarse() ? grip_id(app, c) : id)) {
        const RC_Box  box = rcGetElementBox(id);
        const RC_Vec2 p   = rcPointer();
        d.phase  = Drag::PRESSED;
        d.card   = c.id;
        d.press  = p;
        d.grab   = { p.x - box.x, p.y - box.y };
        d.at     = { box.x, box.y };
        d.width  = box.width;
        d.height = box.height;
    }
    if (rcClicked(id) && d.phase != Drag::LIFTED)
        open_card(st, c.id);
    card_menu(st, app, c, id);
    rcBox(.h = "8px") {}
}

/* ---- Drag and drop ---- */

/* The slot under the pointer in one list, counting the cards around the dragged one. A card's
   box is read from the last layout, where an open slot has already pushed the cards below it
   down, which keeps the answer stable while the slot moves. */
static inline int slot_under(AppState &st, RC_App *app, const List &l, float y) {
    int k = 0;
    for (const Card &c : st.board().cards) {
        if (c.list != l.id || c.id == st.drag.card)
            continue;
        if (card_matches(st.filter, c, st.today)) {
            const RC_Box box = rcGetElementBox(card_id(app, c.id));
            if (box.found && y < box.y + box.height / 2.0f)
                return k;
        }
        k++;
    }
    return k;
}

/* How far to scroll while the pointer rests near an edge: faster the closer it gets. */
static inline float edge_speed(float at, float low, float high, float dt) {
    const float EDGE = 56.0f, SPEED = 900.0f;
    if (at < low + EDGE)
        return -SPEED * dt * (low + EDGE - at) / EDGE;
    if (at > high - EDGE)
        return SPEED * dt * (at - high + EDGE) / EDGE;
    return 0.0f;
}

/* The nearest list by x, so the gap between two lists still has an answer. */
static inline const List *list_near(AppState &st, RC_App *app, float x) {
    const List *over = nullptr;
    float nearest = 1e9f;
    for (const List &l : st.board().lists) {
        const RC_Box box = rcGetElementBox(rcFormat(rcAppArena(app), "list%d", l.id).chars);
        if (!box.found || box.width <= 0.0f)
            continue;
        const float right = box.x + box.width;
        const float off   = x < box.x ? box.x - x : x > right ? x - right : 0.0f;
        if (off < nearest) {
            nearest = off;
            over = &l;
        }
    }
    return over;
}

/* Where the lifted card would land, and the scrolling that brings more of the board in. */
static inline void drag_track(AppState &st, RC_App *app, const RC_Viewport &view, float dt) {
    Drag &d = st.drag;
    const RC_Vec2 p = rcPointer();
    const List *over = list_near(st, app, p.x);
    if (!over)
        return;
    const char  *laneId = rcFormat(rcAppArena(app), "lane%d", over->id).chars;
    const RC_Box lane   = rcGetElementBox(laneId);
    if (lane.found && p.x >= lane.x && p.x <= lane.x + lane.width)
        rcScrollBy(laneId, 0.0f, edge_speed(p.y, lane.y, lane.y + lane.height, dt));
    if (!one_lane(view)) {
        const RC_Box board = rcGetElementBox("board");
        if (board.found)
            rcScrollBy("board", edge_speed(p.x, board.x, board.x + board.width, dt), 0.0f);
    }
    const int slot = slot_under(st, app, *over, p.y);
    if (over->id != d.gap.list || slot != d.gap.slot) {
        /* Back to the slot still closing: it reopens from where it was, without a jump. */
        const float reopened = over->id == d.was.list && slot == d.was.slot ? d.was.height : 0.0f;
        d.was = d.gap;
        d.gap.list   = over->id;
        d.gap.slot   = slot;
        d.gap.height = reopened;
    }
}

/* The ease both slot heights follow: a fixed share of the distance per 60 Hz frame, whatever
   the frame rate, so the neighbors slide at one speed. */
static inline float approach(float value, float target, float dt) {
    const float rate = dt * 18.0f > 1.0f ? 1.0f : dt * 18.0f;
    const float next = value + (target - value) * rate;
    return target - next < 0.5f && target - next > -0.5f ? target : next;
}

/* A short flight with no bounce: the card decelerates into the open slot, then the move is
   queued and lands in this same frame (main.cpp runs the queue after this). A slot that has
   only just opened has no box yet, so the card holds still for that frame. */
static inline void drag_fly(AppState &st, RC_App *app, const Card &c, double now) {
    Drag &d = st.drag;
    const float  t  = static_cast<float>((now - d.landStart) / 0.18);
    const RC_Box to = rcGetElementBox("dropgap");
    if (t >= 1.0f) {
        if (c.list != d.gap.list || d.gap.slot != d.fromSlot) {
            move_card(st, app, c, d.gap.list, d.gap.slot);
        } else {
            st.landedCard = c.id;
            st.landedAt   = now;
        }
        d = Drag{};
        return;
    }
    if (!to.found)
        return;
    const float e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
    d.at   = { d.landFrom.x + (to.x - d.landFrom.x) * e, d.landFrom.y + (to.y - d.landFrom.y) * e };
    d.lift = 1.0f - e;
}

/* The whole gesture, once a frame before anything is declared: the press edge is taken by the
   card itself, and every later edge is read here, wherever the pointer has gone. */
static inline void drag_update(AppState &st, RC_App *app, const RC_Viewport &view) {
    Board &b = st.board();
    Drag  &d = st.drag;
    if (d.phase == Drag::NONE)
        return;
    const Card *c = find_card(b, d.card);
    if (!c) {               /* another window deleted it */
        d = Drag{};
        return;
    }
    const double  now = rcAppTime(app);
    const float   dt  = d.lastFrame > 0.0 ? static_cast<float>(now - d.lastFrame) : 0.016f;
    const RC_Vec2 p   = rcPointer();
    d.lastFrame = now;

    if (d.phase == Drag::PRESSED) {
        const float dx = p.x - d.press.x, dy = p.y - d.press.y;
        if (!rcPointerDown(RC_POINTER_LEFT))
            d = Drag{};
        else if (dx * dx + dy * dy > 25.0f)
            drag_lift(d, b, *c);
        return;
    }
    d.gap.height = approach(d.gap.height, d.height + 8.0f, dt);
    d.was.height = approach(d.was.height, 0.0f, dt);
    if (d.phase == Drag::LIFTED) {
        d.at   = { p.x - d.grab.x, p.y - d.grab.y };
        d.lift = d.lift + dt / 0.12f > 1.0f ? 1.0f : d.lift + dt / 0.12f;
        rcSetCursor(RC_CURSOR_GRABBING);
        if (rcKeyPressed(RC_KEY_ESCAPE)) {
            d.was = d.gap;
            d.gap.list   = d.fromList;
            d.gap.slot   = d.fromSlot;
            d.gap.height = 0.0f;
            drag_land(st, app);
        } else if (!rcPointerDown(RC_POINTER_LEFT)) {
            drag_land(st, app);
        } else {
            drag_track(st, app, view, dt);
        }
    } else {
        drag_fly(st, app, *c, now);
    }
    rcWindowRequestFrame(rcAppMainWindow(app));
}

/* An open slot is a pair of boxes: the outer one holds the height that animates, and the inner
   one leaves the 8 px between cards unpainted. */
static inline void drop_slot(const char *id, float height) {
    if (height <= 0.0f)
        return;
    rcBox(.id = id, .w = "grow", .hType = RC_PX(height)) {
        rcBox(.bg = rcWithAlpha(PAL.shade, 0.11f), .borderRadius = "all-lg", .w = "grow",
              .hType = RC_PX(height > 8.0f ? height - 8.0f : 0.0f)) {}
    }
}

/* The lifted card, over everything, letting the pointer through to what lies below it. A lift
   reads through the rise, the grown edge and the shadow that deepens with it. */
static inline void drag_float(AppState &st, RC_App *app) {
    Drag &d = st.drag;
    if (d.phase != Drag::LIFTED && d.phase != Drag::LANDING)
        return;
    const Board &b = st.board();
    const Card  *c = find_card(st.board(), d.card);
    if (!c)
        return;
    const float     grow = 3.0f * d.lift;
    const RC_Vec2   at   = { d.at.x - grow, d.at.y - grow - 4.0f * d.lift };
    const RC_Shadow deep = { rcWithAlpha(PAL.shade, 0.251f + 0.102f * d.lift), 0.0f,
                             1.0f + 8.0f * d.lift, 2.0f + 22.0f * d.lift, 0.0f };
    rcBox(.bg = RC_WHITE, .p = px(grow + 0.5f), .borderRadius = "all-lg",
          .wType = RC_PX(d.width + 2.0f * grow), .shadow = deep,
          .floating = { .to = RC_ATTACH_ROOT, .offset = at, .zIndex = 2000,
                        .capture = RC_CAPTURE_PASSTHROUGH, .clip = RC_CLIP_NONE }) {
        rcColumn(.wType = RC_PX(d.width)) {
            card_body(st, app, b, *c, false);
        }
    }
}

/* ---- A list ---- */

static inline void close_composer(AppState &st) {
    st.composerList = 0;
    st.composer[0] = '\0';
}

static inline void open_composer(AppState &st, int list) {
    st.composerList = list;
    st.composer[0] = '\0';
    rcSetFocus("composer");
}

/* The composer at a list's foot: Enter adds and stays open for the next card, as Trello's
   does, and Esc or the X closes it. */
static inline void card_composer(AppState &st, const List &l) {
    rcColumn(.gap = 8, .pb = 8, .w = "grow") {
        rcTextInput("composer", st.composer, sizeof st.composer, .className = control_class(),
                    .placeholder = "Enter a title", .clearButton = true);
        rcRow(.gap = 4, .align = "cl", .w = "grow") {
            const bool enter = rcKeyPressedOn("composer", RC_KEY_ENTER);
            if ((rcButton("composer-add", "Add card", .className = control_class(),
                          .variant = RC_BTN_PRIMARY) || enter)
                && st.composer[0]) {
                Card c;
                c.list  = l.id;
                c.title = st.composer;
                c.pos   = pos_for_slot(st.board(), l.id, cards_in(st.board(), l.id), 0);
                put_card(st, std::move(c));
                st.composer[0] = '\0';
                rcSetFocus("composer");
            }
            if (icon_button("composer-x", rcIconX, "Close", true))
                close_composer(st);
        }
    }
    /* The field takes an Esc to blur, so its own key closes the composer in one press. */
    if ((rcKeyPressed(RC_KEY_ESCAPE) || rcKeyPressedOn("composer", RC_KEY_ESCAPE))
        && !rcIsModalOpen())
        close_composer(st);
}

static inline void begin_rename(AppState &st, const List &l) {
    st.renamingList = l.id;
    rcStrCopy(st.rename, l.title.c_str(), sizeof st.rename);
    rcSetFocus("rename");
}

/* Selection order into fresh positions: dated cards first, the soonest on top. */
static inline void sort_by_due(AppState &st, int list) {
    std::vector<Card> cards;
    for (const Card &c : st.board().cards)
        if (c.list == list)
            cards.push_back(c);
    double pos = 1024.0;
    while (!cards.empty()) {
        size_t pick = 0;
        for (size_t i = 1; i < cards.size(); i++) {
            const int due  = cards[i].due ? cards[i].due : LAST_DAY + 1;
            const int best = cards[pick].due ? cards[pick].due : LAST_DAY + 1;
            if (due < best)
                pick = i;
        }
        Card next = std::move(cards[pick]);
        cards.erase(cards.begin() + static_cast<long>(pick));
        next.pos = pos;
        pos += 1024.0;
        put_card(st, std::move(next));
    }
}

/* Halfway between the lists on either side of where it goes. Halving one gap again and
   again, as moving a list back and forth does, runs out of doubles after about fifty moves,
   and the list then lands on a neighbor's pos and stays put: with no double strictly between
   the two, every list is spaced out again in the order the move makes, one change each. */
static inline void move_list(AppState &st, const List &l, int index, int step) {
    const std::vector<List> &lists = st.board().lists;
    const int    last   = static_cast<int>(lists.size()) - 1;
    const int    to     = index + step;
    const int    past   = to + step;
    const double beside = lists[static_cast<size_t>(to)].pos;
    const double beyond = past < 0 ? 0.0 : past > last ? beside + 2048.0
                                                       : lists[static_cast<size_t>(past)].pos;
    List next = l;
    next.pos = (beside + beyond) / 2.0;
    if ((next.pos - beside) * (next.pos - beyond) < 0.0) {
        put_list(st, std::move(next));
        return;
    }
    std::vector<List> order;
    for (const List &other : lists)
        if (other.id != l.id)
            order.push_back(other);
    order.insert(order.begin() + static_cast<long>(to), l);
    for (size_t i = 0; i < order.size(); i++) {
        order[i].pos = 1024.0 * static_cast<double>(i + 1);
        put_list(st, std::move(order[i]));
    }
}

/* The menu behind a list's three dots: a ghost on the lane, which keeps its hover fill while
   it is open. */
static inline void list_menu(AppState &st, const List &l, int index, const char *menuId) {
    Board &b = st.board();
    if (!rcBeginMenu(menuId, "",
                     .className = by_pointer("h-8 hover:bg-border open:bg-border",
                                             "h-11 hover:bg-border open:bg-border"),
                     .variant = RC_BTN_GHOST, .leadingIcon = rcIconEllipsisVertical))
        return;
    if (rcMenuItem("Add a card", .leadingIcon = rcIconPlus))
        open_composer(st, l.id);
    if (rcMenuItem("Rename list", .leadingIcon = rcIconFileText))
        begin_rename(st, l);
    if (rcMenuItem("Move list left", .disabled = index == 0, .leadingIcon = rcIconChevronLeft))
        move_list(st, l, index, -1);
    if (rcMenuItem("Move list right", .disabled = index == static_cast<int>(b.lists.size()) - 1,
                   .leadingIcon = rcIconChevronRight))
        move_list(st, l, index, 1);
    if (rcMenuItem("Sort by due date", .disabled = cards_in(b, l.id) < 2,
                   .leadingIcon = rcIconClock))
        sort_by_due(st, l.id);
    if (rcMenuItem("Delete list", .className = "text-danger hover:bg-danger/10",
                   .leadingIcon = rcIconTrash2))
        drop_list(st, l.id);
    rcEndMenu();
}

/* The inline rename. The frame that asks focuses the field only at its end, so a blur counts
   once the field has a box; its x keeps the focus where it was, so it is no blur. Esc walks
   away with no change. */
static inline void rename_finish(AppState &st, const List &l) {
    const bool away = !rcIsFocused("rename") && rcGetElementBox("rename").width > 0.0f;
    const bool done = rcKeyPressedOn("rename", RC_KEY_ENTER) || away;
    if (rcKeyPressedOn("rename", RC_KEY_ESCAPE)) {
        st.renamingList = 0;
    } else if (done) {
        if (st.rename[0] && l.title != st.rename) {
            List next = l;
            next.title = st.rename;
            put_list(st, std::move(next));
        }
        st.renamingList = 0;
    }
}

/* A list's count, as its head and its tab show it: under a filter, how many of them match. */
static inline const char *list_count(AppState &st, RC_App *app, const List &l) {
    const Board &b = st.board();
    RC_Arena *mem = rcAppArena(app);
    const int count = cards_in(b, l.id);
    if (!filter_on(st.filter))
        return rcFormat(mem, "%d", count).chars;
    int shown = 0;
    for (const Card &c : b.cards)
        shown += c.list == l.id && card_matches(st.filter, c, st.today);
    return rcFormat(mem, "%d of %d", shown, count).chars;
}

/* A list's head: its title, which a click renames, its count and its menu. `tabbed`, on the
   one-lane arm, the tab above already counts the list, so the head leaves the count to it.
   It keeps the title: the strip of tabs scrolls, so the open list's tab can be out of sight,
   and every control a wide window has, a phone has too. */
static inline void list_header(AppState &st, RC_App *app, const List &l, int index, bool tabbed) {
    RC_Arena *mem = rcAppArena(app);
    const char *titleId = rcFormat(mem, "listtitle%d", l.id).chars;
    const char *menuId  = rcFormat(mem, "listmenu%d", l.id).chars;

    rcRow(.gap = 8, .pl = 12, .pr = 8, .py = 4, .align = "cl", .w = "grow") {
        if (st.renamingList == l.id) {
            rcTextInput("rename", st.rename, sizeof st.rename, .className = control_class(),
                        .clearButton = true);
            rename_finish(st, l);
        } else {
            rcBox(.id = titleId, .focusable = true, .overflow = "hidden", .w = "grow") {
                rcTextC(l.title.c_str(), .font = rcFontSemibold(14), .color = PAL.ink, .wrap = "e",
                        .select = RC_SELECT_NONE);
            }
            if (rcClicked(titleId))
                begin_rename(st, l);
            if (!tabbed)
                small_text(list_count(st, app, l));
        }
        list_menu(st, l, index, menuId);
    }
}

/* A list's cards, with the drop slots among them. A slot's number counts the cards around
   the dragged one, filtered or not, so a drop lands right under a filter too. */
static inline void list_cards(AppState &st, RC_App *app, const List &l, bool dragging) {
    const Board &b = st.board();
    const Drag  &d = st.drag;
    int k = 0;
    for (const Card &c : b.cards) {
        if (c.list != l.id || (dragging && c.id == d.card))
            continue;
        if (dragging && d.was.list == l.id && d.was.slot == k)
            drop_slot("dropwas", d.was.height);
        if (dragging && d.gap.list == l.id && d.gap.slot == k)
            drop_slot("dropgap", d.gap.height);
        k++;
        if (card_matches(st.filter, c, st.today))
            card_in_list(st, app, b, c);
    }
    if (dragging && d.was.list == l.id && d.was.slot >= k)
        drop_slot("dropwas", d.was.height);
    if (dragging && d.gap.list == l.id && d.gap.slot >= k)
        drop_slot("dropgap", d.gap.height);
}

static inline void list_column(AppState &st, RC_App *app, const List &l, int index, float width) {
    const Board &b = st.board();
    const Drag  &d = st.drag;
    RC_Arena *mem = rcAppArena(app);
    const char *listId   = rcFormat(mem, "list%d", l.id).chars;
    const char *laneId   = rcFormat(mem, "lane%d", l.id).chars;
    const char *addId    = rcFormat(mem, "add%d", l.id).chars;
    const bool  dragging = d.phase == Drag::LIFTED || d.phase == Drag::LANDING;
    int shown = 0, count = 0;
    for (const Card &c : b.cards) {
        if (c.list == l.id) {
            count++;
            shown += card_matches(st.filter, c, st.today);
        }
    }
    if (rcIsHovered(listId))
        st.hoveredList = l.id;
    /* While the cards overflow under a mouse the scrollbar takes its own 12 px of the
       lane, as a browser's does, so it never covers a card's edge. */
    /* A list fits its cards and stops at the board's height, where the scroll column in it
       gives way: a scroller compresses first. */
    rcColumn(.id = listId, .bg = PAL.lane, .pt = 4, .pb = 8, .borderRadius = "all-xl",
             .wType = width > 0.0f ? RC_PX(width) : RC_GROW) {
        list_header(st, app, l, index, width <= 0.0f);
        rcColumn(.id = laneId, .pt = 4, .pl = 8, .pr = px(8.0f), .scroll = "v",
                 .w = "grow") {
            list_cards(st, app, l, dragging);
            if (count == 0 && !dragging && st.composerList != l.id) {
                rcBox(.px = 4, .py = 8, .w = "grow") {
                    small_text("No cards yet");
                }
            } else if (count > 0 && shown == 0 && !dragging) {
                rcBox(.px = 4, .py = 8, .w = "grow") {
                    small_text("No matching cards");
                }
            }
            if (st.composerList == l.id)
                card_composer(st, l);
        }
        rcScrollbar(laneId);
        if (st.composerList != l.id) {
            rcRow(.pt = 4, .px = 8, .w = "grow") {
                if (rcButton(addId, "Add a card",
                             .className = by_pointer("h-8 text-[#44546f] hover:bg-border",
                                                     "h-11 text-[#44546f] hover:bg-border"),
                             .variant = RC_BTN_GHOST, .leadingIcon = rcIconPlus))
                    open_composer(st, l.id);
            }
        }
    }
}

/* The last column of a board: a translucent button that turns into a small form. The button
   is a row of the app's own, a column wide with its label at the left; .focusable makes it a
   Tab stop that Enter and Space click, as they do an rcButton. */
static inline void add_list_column(AppState &st) {
    Board &b = st.board();
    if (!st.addingList) {
        rcRow(.id = "add-list", .focusable = true, .gap = 8, .px = 12, .align = "cl",
              .borderRadius = "all-xl", .h = "44px", .wType = RC_PX(ADD_LIST_W),
              .className = "bg-white/22 hover:bg-white/31 active:bg-white/40") {
            rcIconPlus(16.0f, RC_WHITE);
            strong_text("Add another list", 14.0f, RC_WHITE);
        }
        if (rcClicked("add-list")) {
            st.addingList = true;
            st.revealFrames = 3;
            st.newList[0] = '\0';
            rcSetFocus("new-list");
        }
        return;
    }
    rcColumn(.id = "new-list-form", .bg = PAL.lane, .gap = 8, .p = 8, .borderRadius = "all-xl",
             .wType = RC_PX(256)) {
        rcTextInput("new-list", st.newList, sizeof st.newList, .className = control_class(),
                    .placeholder = "Enter list name", .clearButton = true);
        rcRow(.gap = 4, .align = "cl") {
            const bool enter = rcKeyPressedOn("new-list", RC_KEY_ENTER);
            if ((rcButton("new-list-add", "Add list", .className = control_class(),
                          .variant = RC_BTN_PRIMARY) || enter)
                && st.newList[0]) {
                List l;
                l.title = st.newList;
                l.pos   = b.lists.empty() ? 1024.0 : b.lists.back().pos + 1024.0;
                put_list(st, std::move(l));
                st.newList[0] = '\0';
                rcSetFocus("new-list");
            }
            if (icon_button("new-list-x", rcIconX, "Close", true))
                st.addingList = false;
        }
    }
    if ((rcKeyPressed(RC_KEY_ESCAPE) || rcKeyPressedOn("new-list", RC_KEY_ESCAPE))
        && !rcIsModalOpen())
        st.addingList = false;
}

/* The one-lane arm's tabs: every list by name and count, scrolling sideways when there are
   more than fit. They start and end where the bar above and the list below do, 12 px in from
   the safe area. */
static inline void list_tabs(AppState &st, RC_App *app, const RC_Viewport &view) {
    const Board &b = st.board();
    RC_Arena *mem = rcAppArena(app);
    rcRow(.id = "tabs", .bg = rcWithAlpha(RC_BLACK, 0.157f), .gap = 4,
          .pl = px(12.0f + view.safe.left), .pr = px(12.0f + view.safe.right), .py = 8,
          .align = "cl", .scroll = "h", .w = "grow") {
        for (size_t i = 0; i < b.lists.size(); i++) {
            const List &l  = b.lists[i];
            const char *id = rcFormat(mem, "seg%d", l.id).chars;
            const bool  on = l.id == st.tabList;
            /* A card moved from here leaves this list on screen: its new tab answers. */
            const float glow = l.id == st.landedList ? landing_glow(st, app) : 0.0f;
            /* The open tab is white under the pointer too: the typed .bg beats the class. */
            rcRow(.id = id, .focusable = true, .bg = on ? RC_WHITE : RC_UNSET_COLOR, .gap = 8,
                  .px = 12, .align = "cc", .borderRadius = "all-md",
                  .border = { .color = rcWithAlpha(RC_WHITE, glow), .width = 2 },
                  .hType = RC_PX(control_h()),
                  .className = "bg-white/14 hover:bg-white/25 active:bg-white/35") {
                strong_text(l.title.c_str(), 14.0f, on ? PAL.ink : RC_WHITE);
                small_text(list_count(st, app, l), on ? PAL.muted : RC_WHITE);
            }
            if (unfocus_on_pointer(rcClicked(id))
                || (st.drag.phase == Drag::LIFTED && rcIsHovered(id)))
                st.tabList = l.id;
        }
        if (glass_button("add-list", "Add another list", rcIconPlus, "Add another list")) {
            st.addingList = !st.addingList;
            st.newList[0] = '\0';
            rcSetFocus("new-list");
        }
    }
    rcScrollbar("tabs", .className = by_pointer("bg-black/20 accent-white/50", ""));
}

/* A press on the bare background drags the whole board sideways, as on Trello. */
static inline void board_pan(AppState &st) {
    const RC_Box board = rcGetElementBox("board");
    const RC_Vec2 p = st.panning || rcPointerPressed(RC_POINTER_LEFT) ? rcPointer() : RC_Vec2{};
    /* The strip along the bottom belongs to the scrollbar's thumb. */
    if (rcPointerPressed(RC_POINTER_LEFT) && rcIsHovered("board") && !st.hoveredList
        && !rcIsHovered("add-list") && !rcIsHovered("new-list-form")
        && st.drag.phase == Drag::NONE && p.y < board.y + board.height - 16.0f) {
        st.panning = true;
        st.panFrom = p.x;
    }
    if (!st.panning)
        return;
    if (rcPointerDown(RC_POINTER_LEFT)) {
        rcScrollBy("board", st.panFrom - p.x, 0.0f);
        st.panFrom = p.x;
        rcSetCursor(RC_CURSOR_GRABBING);
    } else {
        st.panning = false;
    }
}

static inline void board_area(AppState &st, RC_App *app, const RC_Viewport &view) {
    Board &b = st.board();
    const float bottom = px(12.0f + view.safe.bottom);
    const float left   = px(12.0f + view.safe.left);
    const float right  = px(12.0f + view.safe.right);

    if (one_lane(view)) {
        if (!find_list(b, st.tabList))
            st.tabList = b.lists.empty() ? 0 : b.lists.front().id;
        list_tabs(st, app, view);
        rcColumn(.id = "board", .gap = 8, .pt = 8, .pb = bottom, .pl = left, .pr = right,
                 .w = "grow", .h = "grow") {
            if (st.addingList)
                add_list_column(st);
            for (size_t i = 0; i < b.lists.size(); i++)
                if (b.lists[i].id == st.tabList)
                    list_column(st, app, b.lists[i], static_cast<int>(i), 0.0f);
        }
        return;
    }
    /* All lists one width, as on Trello. It shares the window out so the last column lands on
       the right edge, within bounds: past them the board scrolls, or simply ends. */
    const float count = static_cast<float>(b.lists.size());
    const float room  = view.width - left - right - ADD_LIST_W - 8.0f * count;
    const float share = count > 0.0f ? room / count : room;
    const float width = share < 256.0f ? 256.0f : share > 320.0f ? 320.0f : share;
    /* A board that scrolls x clips y as well, as CSS's does, and a list in it would keep its
       cards' whole height past the board's foot. A column the board's height around each list
       holds it there, as the one-lane board holds its list. A scroller's padding scrolls with
       its content, so the safe area stays outside the board: on a phone on its side the lists
       stop at the camera cutout, as the bars above do, and never slide under it. */
    rcRow(.pl = px(view.safe.left), .pr = px(view.safe.right), .w = "grow", .h = "grow") {
        rcRow(.id = "board", .gap = 8, .pt = 12, .pb = bottom, .pl = 12, .pr = 12, .align = "tl",
              .scroll = "h", .w = "grow", .h = "grow") {
            for (size_t i = 0; i < b.lists.size(); i++) {
                rcColumn(.h = "grow") {
                    list_column(st, app, b.lists[i], static_cast<int>(i), width);
                }
            }
            add_list_column(st);
        }
    }
    board_pan(st);
    /* The form at the far end comes into view when it opens and when a list lands before it.
       A box and a scroll range are the last layout's, so this looks again for a few frames,
       until they have caught up with what was just declared. */
    if (st.addingList && b.lists.size() != st.listsShown)
        st.revealFrames = 3;
    st.listsShown = b.lists.size();
    if (st.revealFrames > 0) {
        st.revealFrames--;
        const RC_Box form = rcGetElementBox("new-list-form");
        const RC_Box area = rcGetElementBox("board");
        const float  past = form.x + form.width + 12.0f - area.x - area.width;
        if (form.found && area.found && past > 0.0f)
            rcScrollBy("board", past, 0.0f);
        rcWindowRequestFrame(rcAppMainWindow(app));
    }
    rcScrollbar("board", .className = by_pointer("bg-black/20 accent-white/60", ""));
}

/* ---- The bars over the board ---- */

static inline void close_switcher(AppState &st) {
    st.switcherOpen = false;
    st.creatingBoard = false;
}

static inline void switcher_row(AppState &st, RC_App *app, size_t index) {
    RC_Arena *mem = rcAppArena(app);
    const Board &row = st.boards[index];
    const char *id   = rcFormat(mem, "board-row%d", static_cast<int>(index)).chars;
    const bool  on   = static_cast<int>(index) == st.current;

    /* The open board keeps its fill under the pointer: its class names no hover. */
    rcRow(.id = id, .focusable = true, .gap = 12, .p = 8, .align = "cl",
          .borderRadius = "all-md", .w = "grow",
          .className = on ? "row-picked" : "hover:bg-surface-alt") {
        board_thumb(row, 40.0f, 32.0f);
        rcColumn(.overflow = "hidden", .w = "grow") {
            rcTextC(row.name.c_str(), .font = rcFontSemibold(14), .color = PAL.ink, .wrap = "e",
                    .select = RC_SELECT_NONE);
            small_text(rcFormat(mem, "%d lists, %d cards", static_cast<int>(row.lists.size()),
                                static_cast<int>(row.cards.size())).chars);
        }
        if (row.starred)
            rcIconStarFilled(16.0f, rcHex(0xE2B203));
    }
    if (rcClicked(id)) {
        open_board(st, static_cast<int>(index));
        close_switcher(st);
    }
}

/* A panel of our own, because a row here holds a thumbnail, two lines of text and a star, and
   an rcMenuItem holds one label. It closes the way a menu does. */
static inline void board_switcher(AppState &st, RC_App *app, float nameMax, bool tight) {
    Board &b = st.board();

    /* A row of the app's own: a thumbnail and a name that ellipsizes are more than an
       rcButton's icon and label hold. .focusable makes it a Tab stop that Enter and Space
       click. min-w-0, as on the web: the bar shrinks this button and its name ellipsizes. A
       tight bar keeps the name alone, as a phone app's header is its screen's title. */
    rcRow(.id = "switcher-button", .focusable = true, .gap = 8, .px = 8, .align = "cl",
          .borderRadius = "all-md", .hType = RC_PX(control_h()),
          .className = st.switcherOpen ? "min-w-0 bg-white/25"
                                       : "min-w-0 hover:bg-white/19 active:bg-white/25") {
        if (!tight) board_thumb(b, 24.0f, 16.0f);
        rcBox(.overflow = "hidden", .wMax = RC_VAL(nameMax)) {
            rcTextC(b.name.c_str(), .font = rcFontSemibold(16), .color = RC_WHITE, .wrap = "e",
                    .select = RC_SELECT_NONE);
        }
        if (!tight) rcIconChevronDown(16.0f, RC_WHITE);
    }
    if (unfocus_on_pointer(rcClicked("switcher-button"))) {
        st.switcherOpen = !st.switcherOpen;
        st.creatingBoard = false;
    }
    if (!st.switcherOpen)
        return;

    rcColumn(.id = "switcher", .bg = RC_WHITE, .gap = 4, .p = 8, .borderRadius = "all-lg",
             .w = "288px", .shadow = POPUP_SHADOW,
             .floating = { .to = RC_ATTACH_ELEMENT, .toId = "switcher-button",
                           .parent = RC_ANCHOR_BOTTOM_LEFT, .element = RC_ANCHOR_TOP_LEFT,
                           .offset = { 0.0f, 4.0f }, .zIndex = 1000, .clip = RC_CLIP_NONE }) {
        rcBox(.pt = 4, .pb = 4, .px = 8) {
            section_label("Your boards");
        }
        for (size_t i = 0; i < st.boards.size(); i++)
            switcher_row(st, app, i);
        rule();
        if (st.creatingBoard) {
            rcColumn(.gap = 8, .p = 8, .w = "grow") {
                rcTextInput("new-board", st.boardName, sizeof st.boardName,
                            .className = control_class(), .placeholder = "Board name",
                            .clearButton = true);
                const bool enter = rcKeyPressedOn("new-board", RC_KEY_ENTER);
                if ((rcButton("new-board-add", "Create board", .disabled = !st.boardName[0],
                              .className = control_class(), .variant = RC_BTN_PRIMARY) || enter)
                    && st.boardName[0]) {
                    new_board(st, st.boardName);
                    st.boardName[0] = '\0';
                    close_switcher(st);
                }
            }
        } else if (rcButton("create-board", "Create a board", .className = control_class(),
                            .variant = RC_BTN_GHOST, .leadingIcon = rcIconPlus)) {
            st.creatingBoard = true;
            rcSetFocus("new-board");
        }
    }
    const bool outside = rcPointerPressed(RC_POINTER_LEFT) && !rcIsHovered("switcher")
                         && !rcIsHovered("switcher-button");
    if (outside || rcKeyPressed(RC_KEY_ESCAPE))
        close_switcher(st);
}

/* The status at the top right is a button, the way into the clearly labeled dialog. It is a
   .focusable row for its dot, which keeps the status color: an rcButton's icon takes its
   label's. */
static inline void sync_button(AppState &st, bool compact) {
    static const unsigned DOT[] = { 0x8590A2, 0xF5CD47, 0x4BCE97, 0xF5CD47, 0xF87168 };
    const SyncState state = st.current == 0 ? sync_state(st.sync) : SYNC_OFF;
    rcRow(.id = "sync-button", .focusable = true, .gap = 8, .px = 12, .align = "cc",
          .borderRadius = "all-md", .hType = RC_PX(control_h()),
          .tooltip = "Sync this board with a server",
          .className = "bg-white/14 hover:bg-white/25 active:bg-white/35") {
        rcBox(.bg = rcHex(DOT[state]), .borderRadius = "all-full", .w = "8px", .h = "8px") {}
        if (compact)
            rcIconRotateCw(16.0f, RC_WHITE);
        else
            strong_text(SYNC_WORD[state], 14.0f, RC_WHITE);
    }
    if (unfocus_on_pointer(rcClicked("sync-button")))
        st.syncOpen = true;
}

static inline void top_bar(AppState &st, RC_App *app, const RC_Viewport &view) {
    Board &b = st.board();
    const bool roomy = ::roomy(view);

    rcRow(.bg = rcWithAlpha(RC_BLACK, 0.353f), .gap = 8, .pt = px(8.0f + view.safe.top), .pb = 8,
          .pl = px(12.0f + view.safe.left), .pr = px(12.0f + view.safe.right), .align = "cl",
          .w = "grow") {
        /* The product's mark: three lanes, staggered as cards on a board fall. */
        rcRow(.bg = RC_WHITE, .gap = 2, .p = 4, .align = "bc", .borderRadius = "all-md",
              .w = "24px", .h = "24px") {
            rcBox(.bg = PAL.accent, .borderRadius = "all-full", .w = "4px", .h = "10px") {}
            rcBox(.bg = PAL.accent, .borderRadius = "all-full", .w = "4px", .h = "16px") {}
            rcBox(.bg = PAL.accent, .borderRadius = "all-full", .w = "4px", .h = "6px") {}
        }
        if (roomy) {
            strong_text("Laneway", 16.0f, RC_WHITE);
            rcBox(.bg = rcWithAlpha(RC_WHITE, 0.235f), .w = "1px", .h = "20px") {}
        }
        /* The bar's other controls keep their width; the name gets what is left. A phone's
           bar folds the star and the shortcuts into one menu, so the name stays readable,
           and the narrowest folds the filter button too. That button is here wherever the
           filters fold (FILTER_ROW_W), which costs a roomy bar 40 px of the name, and it
           stays lit while a filter is on, as Trello's Filter button does: folded away, a
           filter in force still shows. */
        const bool  folded = !roomy || view.width < FILTER_ROW_W;
        const float room   = view.width - (!roomy ? 330.0f : folded ? 600.0f : 560.0f);
        const bool  tight  = room < 120.0f;
        const bool  tiny   = view.width < 360.0f;
        board_switcher(st, app, tight || room > 320.0f ? 320.0f : room, tight);
        if (roomy && glass_button("star", "", b.starred ? rcIconStarFilled : rcIconStar,
                                  b.starred ? "Remove the star" : "Star this board"))
            b.starred = !b.starred;
        rcSpacer() {}
        if (glass_button("new-card", roomy ? "New card" : "", rcIconPlus, "New card (n)")
            && !b.lists.empty())
            open_composer(st, find_list(b, st.hoveredList) ? st.hoveredList : b.lists.front().id);
        if (folded && !tiny
            && glass_button("filter-toggle", "", rcIconSearch, "Filter cards (f)",
                            st.filtersOpen || filter_on(st.filter)))
            st.filtersOpen = !st.filtersOpen;
        sync_button(st, !roomy);
        if (roomy && glass_button("help", "", rcIconInfo, "Keyboard shortcuts (?)"))
            st.helpOpen = true;
        /* Bare, as a phone's bar menu is, with the glass buttons' fills under the pointer,
           pressed and open: a ghost's own is the lane's light gray, under a white glyph. */
        if (!roomy && rcBeginMenu("bar-more", "",
                                  .className = by_pointer(
                                      "h-8 text-white hover:bg-white/25 active:bg-white/35 "
                                      "open:bg-white/35",
                                      "h-11 text-white hover:bg-white/25 active:bg-white/35 "
                                      "open:bg-white/35"),
                                  .variant = RC_BTN_GHOST,
                                  .leadingIcon = rcIconEllipsisVertical)) {
            if (tiny && rcMenuItem("Filter cards"))
                st.filtersOpen = !st.filtersOpen;
            if (rcMenuItem(b.starred ? "Remove the star" : "Star this board"))
                b.starred = !b.starred;
            if (rcMenuItem("Keyboard shortcuts"))
                st.helpOpen = true;
            rcEndMenu();
        }
    }
}

/* One label color in the filter: a glass chip with a dot of its color while it is off, filled
   with that color and checked while it is on, so a filtered board reads as one at a glance. */
static inline void label_toggle(AppState &st, RC_App *app, int index) {
    const char *id = rcFormat(rcAppArena(app), "f-label%d", index).chars;
    const bool  on = st.filter.labels & (1u << index);
    /* Named, as a filter should be: color alone would not say what it selects. The typed .bg
       keeps an applied filter's fill under the pointer too. The dot and the check take the same
       room, so turning a filter on moves nothing beside it. */
    rcRow(.id = id, .focusable = true, .bg = on ? LABEL_COLOR[index] : RC_UNSET_COLOR, .gap = 6,
          .px = 8, .align = "cc", .borderRadius = "all-sm", .h = "22px",
          .className = "bg-white/14 hover:bg-white/25 active:bg-white/35") {
        if (on)
            rcIconCheck(10.0f, PAL.ink);
        else
            rcBox(.bg = LABEL_COLOR[index], .borderRadius = "all-full", .w = "10px", .h = "10px") {}
        rcTextC(st.board().labelName[index], .color = on ? PAL.ink : RC_WHITE, .size = 12,
                .wrap = "n", .select = RC_SELECT_NONE);
    }
    if (unfocus_on_pointer(rcClicked(id)))
        st.filter.labels ^= 1u << index;
}

static inline void member_toggle(AppState &st, RC_App *app, int index) {
    const char *id = rcFormat(rcAppArena(app), "f-member%d", index).chars;
    const bool  on = st.filter.members & (1u << index);
    rcBox(.id = id, .focusable = true, .tooltip = MEMBERS[index].name) {
        avatar(index, 28.0f, on, st.filter.members && !on);
    }
    if (unfocus_on_pointer(rcClicked(id)))
        st.filter.members ^= 1u << index;
}

/* The filter bar, Linear's row on Trello's filter. Everything filters as it changes. */
static inline void filter_controls(AppState &st, RC_App *app, bool wrap) {
    const Board &b = st.board();
    RC_Arena *mem = rcAppArena(app);

    /* Wrapping, the field keeps a usable width, and what does not fit beside it takes a line. */
    rcBox(.wType = wrap ? RC_GROW : RC_PX(220), .wMin = wrap ? RC_VAL(200.0f) : RC_UNSET_VAL) {
        rcTextInput("filter-query", st.filter.query, sizeof st.filter.query,
                    .className = control_class(), .placeholder = "Filter cards",
                    .leadingIcon = rcIconSearch, .clearButton = true);
    }
    rcRow(.gap = 4, .align = "cl") {
        for (int i = 0; i < LABEL_COUNT; i++)
            label_toggle(st, app, i);
    }
    rcRow(.gap = 4, .align = "cl") {
        for (int i = 0; i < MEMBER_COUNT; i++)
            member_toggle(st, app, i);
    }
    rcBox(.w = "136px") {
        rcCombo("f-due", &st.filter.due, DUE_FILTER, 4, .className = control_class());
    }
    if (filter_on(st.filter) && glass_button("f-clear", "Clear", rcIconX, "Clear the filter (x)"))
        filter_clear(st.filter);
    if (!wrap)
        rcSpacer() {}
    /* Counted after every control above, so the count follows a change on its own frame. */
    const int total = static_cast<int>(b.cards.size());
    int shown = 0;
    for (const Card &c : b.cards)
        shown += card_matches(st.filter, c, st.today);
    small_text(filter_on(st.filter) ? rcFormat(mem, "%d of %d cards", shown, total).chars
                                    : rcFormat(mem, "%d cards", total).chars,
               RC_WHITE);
}

static inline void filter_bar(AppState &st, RC_App *app, const RC_Viewport &view) {
    const float left  = px(12.0f + view.safe.left);
    const float right = px(12.0f + view.safe.right);
    if (roomy(view) && view.width >= FILTER_ROW_W) {
        rcRow(.bg = rcWithAlpha(RC_BLACK, 0.22f), .gap = 16, .pl = left, .pr = right, .py = 8,
              .align = "cl", .w = "grow") {
            filter_controls(st, app, false);
        }
    } else if (st.filtersOpen) {
        rcRow(.bg = rcWithAlpha(RC_BLACK, 0.22f), .gap = 8, .pl = left, .pr = right, .py = 8,
              .align = "cl", .w = "grow", .className = "flex-wrap") {
            filter_controls(st, app, true);
        }
    }
}

/* The answer to a change, with its Undo after a delete. It sits at the bottom right of the
   board, clear of the lists' heads and the "Add another list" column, attached to the root's
   box, whose foot and right edge are the window's, so it stays put while the lists scroll,
   and a dialog opening takes it away, so it never lies dimmed behind what the user turned
   to. Its z sits under the library's popups (1000), so a card's menu opened beside it draws
   over it. The app parks between inputs, so while the bar shows it asks for the frame that
   takes it away. */
static inline void snackbar(AppState &st, RC_App *app, const RC_Viewport &view) {
    if (dialog_open(st))
        st.toast.clear();
    const double left = st.toastUntil - rcAppTime(app);
    if (st.toast.empty() || left <= 0.0)
        return;
    rcWindowRequestFrameAfter(rcAppMainWindow(app), left + 0.05);
    /* 12 px inside the safe area, as the board's lists are, on both sides and at the foot.
       An on-screen keyboard covers the window's foot without resizing it, and a notice can
       come while the composer keeps it up, so the toast stands above the keyboard too. */
    const float room = view.width - 24.0f - view.safe.left - view.safe.right;
    const float most = room > 0.0f ? room : 0.0f;
    const float kb   = view.keyboard.bottom;
    const float foot = kb > view.safe.bottom ? kb : view.safe.bottom;
    rcRow(.bg = PAL.ink, .gap = 12, .pl = 16, .pr = 8, .py = 8, .align = "cl",
          .borderRadius = "all-lg", .wMax = RC_VAL(most), .shadow = POPUP_SHADOW,
          .floating = { .to = RC_ATTACH_ELEMENT, .toId = "root", .parent = RC_ANCHOR_BOTTOM_RIGHT,
                        .element = RC_ANCHOR_BOTTOM_RIGHT,
                        .offset = { -12.0f - view.safe.right, -12.0f - foot }, .zIndex = 900,
                        .clip = RC_CLIP_NONE }) {
        rcTextC(st.toast.c_str(), .color = RC_WHITE, .size = 14, .wordBreak = "a");
        if ((!st.undoCards.empty() || st.undoHasList) && glass_button("undo", "Undo", nullptr))
            undo_delete(st);
    }
}

#endif /* KANBAN_VIEW_H */
