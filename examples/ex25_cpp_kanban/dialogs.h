/*
    dialogs.h - the card's detail, the Sync dialog and the shortcut sheet.

    Each is an rcBeginModal: Esc and a click on the scrim close it for free. The card dialog
    follows Trello for the main column (title, description, checklist, activity) and Linear
    for the side rail, where every property is one click away.
*/
#ifndef KANBAN_DIALOGS_H
#define KANBAN_DIALOGS_H

#include "rayclay.h"
#include "app.h"
#include "view.h"

#include "icons/rc_icons_check.h"
#include "icons/rc_icons_chevron_left.h"
#include "icons/rc_icons_chevron_right.h"
#include "icons/rc_icons_circle_check.h"
#include "icons/rc_icons_clock.h"
#include "icons/rc_icons_file_text.h"
#include "icons/rc_icons_message_square.h"
#include "icons/rc_icons_rotate_cw.h"
#include "icons/rc_icons_trash_2.h"
#include "icons/rc_icons_triangle_alert.h"
#include "icons/rc_icons_x.h"

/* ---- The card ---- */

/* Whether a buffer holds an edit. The dialog fills it with the card's text as far as it fits
   (rcStrCopy), and a server takes more than these buffers hold (serve.py: 250 characters of
   title, up to 1,000 bytes): compared with the card itself, a dialog only opened and closed
   would send that text back cut short. */
static inline bool field_edited(const std::string &text, const char *field, size_t cap) {
    std::string fit(cap, '\0');
    rcStrCopy(&fit[0], text.c_str(), cap);
    return std::string(fit.c_str()) != field;
}

/* Text edits collect in the dialog's buffers and reach the card when they are done, so a
   word of typing is one change, and one PATCH when the board is synced. */
static inline void commit_title(AppState &st, const Card &c) {
    if (st.title[0] && field_edited(c.title, st.title, sizeof st.title)) {
        Card next = latest(st, c);
        next.title = st.title;
        put_card(st, std::move(next));
    }
}

static inline void commit_description(AppState &st, const Card &c) {
    if (field_edited(c.description, st.description, sizeof st.description)) {
        Card next = latest(st, c);
        next.description = st.description;
        put_card(st, std::move(next));
    }
}

/* One day of the month grid, with today ringed and the due date filled. */
static inline void calendar_day(AppState &st, RC_App *app, const Card &c, int day, int number) {
    RC_Arena *mem = rcAppArena(app);
    const char *id = rcFormat(mem, "day%d", day).chars;
    const bool  on = c.due == day;
    const RC_Color edge = day == st.today && !on ? PAL.accent : RC_UNSET_COLOR;
    /* The due date keeps its fill under the pointer: the typed .bg beats the class. */
    rcBox(.id = id, .focusable = true, .bg = on ? PAL.accent : RC_UNSET_COLOR, .align = "cc",
          .borderRadius = "all-md", .border = { .color = edge, .width = 1 }, .w = "grow",
          .h = "28px", .className = "hover:bg-surface-alt") {
        small_text(rcFormat(mem, "%d", number).chars, on ? RC_WHITE : PAL.ink);
    }
    if (rcClicked(id)) {
        Card next = latest(st, c);
        next.due = day;
        put_card(st, std::move(next));
    }
}

/* The month grid: click a day, step the months, or clear the date beside it. */
static inline void due_calendar(AppState &st, RC_App *app, const Card &c) {
    static const char *const WEEK[7] = { "S", "M", "T", "W", "T", "F", "S" };
    const int  first = first_of_month(st.calendarMonth);
    const Date shown = date_of(first);
    const int  days  = month_days(shown.year, shown.month);
    const int  lead  = weekday_of(first);

    rcColumn(.gap = 4, .w = "grow") {
        rcRow(.align = "cl", .w = "grow") {
            if (icon_button("cal-prev", rcIconChevronLeft, "Previous month")
                && st.calendarMonth > 0)
                st.calendarMonth--;
            rcBox(.align = "cc", .w = "grow") {
                strong_text(rcFormat(rcAppArena(app), "%s %d", MONTH_NAME[shown.month],
                                     shown.year).chars);
            }
            if (icon_button("cal-next", rcIconChevronRight, "Next month")
                && st.calendarMonth < LAST_MONTH)
                st.calendarMonth++;
        }
        rcRow(.w = "grow") {
            for (int col = 0; col < 7; col++) {
                rcBox(.align = "cc", .w = "grow", .h = "24px") {
                    section_label(WEEK[col]);
                }
            }
        }
        for (int week = 0; (week * 7) - lead + 1 <= days; week++) {
            rcRow(.w = "grow") {
                for (int col = 0; col < 7; col++) {
                    const int number = week * 7 + col - lead + 1;
                    if (number < 1 || number > days) {
                        rcBox(.w = "grow", .h = "28px") {}
                    } else {
                        calendar_day(st, app, c, first + number - 1, number);
                    }
                }
            }
        }
    }
}

static inline void toggle_member(AppState &st, RC_App *app, const Card &c, int i) {
    const char *id = rcFormat(rcAppArena(app), "card-member%d", i).chars;
    rcBox(.id = id, .focusable = true, .tooltip = MEMBERS[i].name) {
        avatar(i, 28.0f, false, !(c.members & (1u << i)));
    }
    if (rcClicked(id)) {
        Card next = latest(st, c);
        next.members ^= 1u << i;
        put_card(st, std::move(next));
    }
}

static inline void toggle_label(AppState &st, RC_App *app, const Board &b, const Card &c, int i) {
    const char *id = rcFormat(rcAppArena(app), "card-label%d", i).chars;
    const bool  on = c.labels & (1u << i);
    /* Off, a default button's look: the lane's gray, the line's under the pointer. On, the
       label's own color, which the typed .bg keeps under the pointer too. */
    rcRow(.id = id, .focusable = true, .bg = on ? LABEL_COLOR[i] : RC_UNSET_COLOR, .gap = 8,
          .px = 8, .align = "cl", .borderRadius = "all-sm", .w = "grow", .h = "24px",
          .className = "bg-surface-alt hover:bg-border") {
        if (!on)
            rcBox(.bg = LABEL_COLOR[i], .borderRadius = "all-xs", .w = "12px", .h = "12px") {}
        rcBox(.w = "grow") {
            small_text(b.labelName[i], PAL.ink);
        }
        if (on)
            rcIconCheck(14.0f, PAL.ink);
    }
    if (rcClicked(id)) {
        Card next = latest(st, c);
        next.labels ^= 1u << i;
        put_card(st, std::move(next));
    }
}

static inline void pick_cover(AppState &st, RC_App *app, const Board &b, const Card &c, int i) {
    const char *id = rcFormat(rcAppArena(app), "card-cover%d", i + 1).chars;
    rcBox(.id = id, .focusable = true, .bg = i < 0 ? PAL.lane : LABEL_COLOR[i], .align = "cc",
          .borderRadius = "all-sm",
          .border = { .color = c.cover == i ? PAL.ink : PAL.line, .width = 2 },
          .w = "grow", .h = "24px", .tooltip = i < 0 ? "No cover" : b.labelName[i]) {
        if (i < 0)
            rcIconX(14.0f, PAL.muted);
    }
    if (rcClicked(id)) {
        Card next = latest(st, c);
        next.cover = i;
        put_card(st, std::move(next));
    }
}

/* The combo's list is rebuilt each frame from the board, in the frame arena. */
static inline void list_picker(AppState &st, RC_App *app, const Card &c) {
    Board &b = st.board();
    const int   n = static_cast<int>(b.lists.size());
    const char **names = static_cast<const char **>(
        rcArenaAlloc(rcAppArena(app), sizeof(const char *) * b.lists.size()));
    int index = 0;
    for (int i = 0; i < n && names; i++) {
        const List &l = b.lists[static_cast<size_t>(i)];
        names[i] = l.title.c_str();
        if (l.id == c.list)
            index = i;
    }
    if (names && rcCombo("card-list", &index, names, n, .className = control_class())) {
        const int to = b.lists[static_cast<size_t>(index)].id;
        move_card(st, app, latest(st, c), to, cards_in(b, to));
    }
}

static inline void due_row(AppState &st, RC_App *app, const Card &c) {
    const Date  d    = date_of(c.due ? c.due : st.today);
    const char *text = c.due ? rcFormat(rcAppArena(app), "%s %d", MONTH_NAME[d.month], d.day).chars
                             : "Set a date";
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        if (rcButton("card-due", text, .className = control_class(), .leadingIcon = rcIconClock)) {
            st.calendarOpen = !st.calendarOpen;
            st.calendarMonth = (d.year - 2026) * 12 + d.month;
        }
        rcSpacer() {}
        if (c.due && icon_button("card-due-clear", rcIconX, "Remove the date")) {
            Card next = latest(st, c);
            next.due = 0;
            next.dueDone = false;
            put_card(st, std::move(next));
            st.calendarOpen = false;
        }
    }
    if (c.due) {
        bool done = c.dueDone;
        if (rcCheckbox("card-due-done", "Mark as done", &done)) {
            Card next = latest(st, c);
            next.dueDone = done;
            put_card(st, std::move(next));
        }
    }
    if (st.calendarOpen)
        due_calendar(st, app, c);
}

/* The rail: one labeled property after another. */
static inline void card_rail(AppState &st, RC_App *app, const Card &c) {
    const Board &b = st.board();

    section_label("List");
    list_picker(st, app, c);

    section_label("Priority");
    int priority = c.priority;
    if (rcCombo("card-priority", &priority, PRIORITY_NAME, PRIORITY_COUNT,
                .className = control_class())) {
        Card next = latest(st, c);
        next.priority = priority;
        put_card(st, std::move(next));
    }

    section_label("Members");
    rcRow(.gap = 4, .align = "cl", .w = "grow") {
        for (int i = 0; i < MEMBER_COUNT; i++)
            toggle_member(st, app, c, i);
    }

    section_label("Labels");
    rcColumn(.gap = 4, .w = "grow") {
        for (int i = 0; i < LABEL_COUNT; i++)
            toggle_label(st, app, b, c, i);
    }

    section_label("Due date");
    due_row(st, app, c);

    section_label("Cover");
    rcRow(.gap = 4, .align = "cl", .w = "grow") {
        for (int i = -1; i < LABEL_COUNT; i++)
            pick_cover(st, app, b, c, i);
    }

    rule();
    if (rcButton("card-delete", "Delete card",
                 .className = by_pointer("h-8 text-danger", "h-11 text-danger"),
                 .leadingIcon = rcIconTrash2)) {
        drop_card(st, c.id);
        st.cardOpen = false;
    }
}

static inline void section_title(RC_IconCallback icon, const char *text) {
    icon(16.0f, PAL.ink2);
    strong_text(text, 16.0f);
}

/* The checklist: a bar that fills, a row per item, and a field to add one. */
static inline void card_checklist(AppState &st, RC_App *app, const Card &c) {
    RC_Arena *mem = rcAppArena(app);
    const int total = static_cast<int>(c.checklist.size());
    int done = 0;
    for (const CheckItem &item : c.checklist)
        done += item.done;

    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        section_title(rcIconCircleCheck, "Checklist");
        rcSpacer() {}
        if (total)
            small_text(rcFormat(mem, "%d of %d", done, total).chars);
    }
    if (total)
        rcProgress("check-progress", static_cast<float>(done) / static_cast<float>(total));
    for (const CheckItem &item : c.checklist) {
        const char *boxId = rcFormat(mem, "check%d", item.key).chars;
        const char *xId   = rcFormat(mem, "check-x%d", item.key).chars;
        rcRow(.gap = 4, .align = "cl", .w = "grow") {
            bool on = item.done;
            rcBox(.overflow = "hidden", .w = "grow") {
                if (rcCheckbox(boxId, item.text.c_str(), &on)) {
                    Card next = latest(st, c);
                    /* A change queued earlier this frame can hold another checklist: change
                       the item only where it still is. */
                    const int at = item_at(next.checklist, item.key);
                    if (at >= 0) {
                        next.checklist[static_cast<size_t>(at)].done = on;
                        put_card(st, std::move(next));
                    }
                }
            }
            if (icon_button(xId, rcIconX, "Delete this item")) {
                Card next = latest(st, c);
                const int at = item_at(next.checklist, item.key);
                if (at >= 0) {
                    next.checklist.erase(next.checklist.begin() + at);
                    put_card(st, std::move(next));
                }
            }
        }
    }
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        rcTextInput("check-add", st.checkItem, sizeof st.checkItem,
                    .className = control_class(), .placeholder = "Add an item",
                    .clearButton = true);
        const bool enter = rcKeyPressedOn("check-add", RC_KEY_ENTER);
        if ((rcButton("check-add-button", "Add", .disabled = !st.checkItem[0],
                      .className = control_class()) || enter)
            && st.checkItem[0]) {
            Card next = latest(st, c);
            next.checklist.push_back({ st.checkItem, false });
            put_card(st, std::move(next));
            st.checkItem[0] = '\0';
            rcSetFocus("check-add");
        }
    }
}

static inline void card_activity(AppState &st, RC_App *app, const Card &c) {
    RC_Arena *mem = rcAppArena(app);

    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        section_title(rcIconMessageSquare, "Activity");
    }
    rcRow(.gap = 8, .align = "tl", .w = "grow") {
        avatar(ME, 32.0f, false, false);
        rcColumn(.gap = 8, .w = "grow") {
            rcTextArea("comment", st.comment, sizeof st.comment, .placeholder = "Write a comment",
                       .rows = 2);
            if (st.comment[0]
                && rcButton("comment-save", "Save", .className = control_class(),
                            .variant = RC_BTN_PRIMARY)) {
                Card next = latest(st, c);
                next.comments.push_back({ ME, st.comment, st.today });
                put_card(st, std::move(next));
                st.comment[0] = '\0';
            }
        }
    }
    /* Newest first. */
    for (size_t i = c.comments.size(); i-- > 0;) {
        const Comment &note = c.comments[i];
        const Date d = date_of(note.day);
        rcRow(.gap = 8, .align = "tl", .w = "grow") {
            avatar(note.author, 32.0f, false, false);
            rcColumn(.gap = 4, .w = "grow") {
                rcRow(.gap = 8, .align = "cl") {
                    strong_text(MEMBERS[note.author].name);
                    small_text(rcFormat(mem, "%s %d", MONTH_NAME[d.month], d.day).chars);
                }
                rcBox(.bg = PAL.lane, .px = 12, .py = 8, .borderRadius = "all-lg", .w = "grow") {
                    rcTextC(note.text.c_str(), .color = PAL.ink, .size = 14, .wordBreak = "a");
                }
            }
        }
    }
}

/* A narrow dialog wraps the title in two rows, as a one-line field would cut it. It is still
   a title: Enter finishes it rather than leaving a line break in it. The one-line field has
   the x every such field has, which keeps the focus where it was: the blur or the close
   commits. The title's face is a font id known only at run time, so the class that names it
   is formatted each frame. */
static inline void card_title(AppState &st, RC_App *app, const Card &c, bool wide) {
    RC_Arena *mem = rcAppArena(app);
    if (wide) {
        rcTextInput("card-title", st.title, sizeof st.title,
                    .className = rcFormat(mem, "%s font-[%d]", by_pointer("h-10", "h-11"),
                                          rcFontSemibold(20)).chars,
                    .clearButton = true);
    } else {
        rcTextArea("card-title", st.title, sizeof st.title,
                   .className = rcFormat(mem, "font-[%d]", rcFontSemibold(20)).chars, .rows = 2);
    }
    const bool titleNow = rcIsFocused("card-title");
    char *lineBreak = st.title;
    while (*lineBreak && *lineBreak != '\n')
        lineBreak++;
    const bool broke = *lineBreak == '\n';
    if (broke) {
        for (char *p = lineBreak; *p; p++)   /* the rest moves left over the break */
            p[0] = p[1];
        rcSetFocus(nullptr);
    }
    if ((st.titleFocused && !titleNow) || rcKeyPressedOn("card-title", RC_KEY_ENTER) || broke)
        commit_title(st, c);
    st.titleFocused = titleNow;
}

static inline void card_main(AppState &st, RC_App *app, const Card &c, bool wide) {
    card_title(st, app, c, wide);

    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        section_title(rcIconFileText, "Description");
    }
    rcTextArea("card-description", st.description, sizeof st.description,
               .placeholder = "Add a more detailed description", .rows = 5);
    const bool descriptionNow = rcIsFocused("card-description");
    if (st.descriptionFocused && !descriptionNow)
        commit_description(st, c);
    st.descriptionFocused = descriptionNow;

    rcBox(.h = "4px") {}
    card_checklist(st, app, c);
    rcBox(.h = "4px") {}
    card_activity(st, app, c);
}

static inline void card_dialog(AppState &st, RC_App *app, const RC_Viewport &view) {
    if (!st.cardOpen)
        return;
    Board &b = st.board();
    const Card *c = find_card(b, st.cardId);
    if (!c) {                   /* deleted, here or in another window */
        st.cardOpen = false;
        return;
    }
    if (st.cardFresh) {
        st.cardFresh = false;
        rcStrCopy(st.title, c->title.c_str(), sizeof st.title);
        rcStrCopy(st.description, c->description.c_str(), sizeof st.description);
        st.comment[0] = st.checkItem[0] = '\0';
        st.calendarOpen = false;
    }
    /* A panel never grows past the safe area, and a body wider than that is cut at its edge:
       the body takes the safe width less the panel's padding and a margin. */
    const float room  = view.width - view.safe.left - view.safe.right - 72.0f;
    const bool  wide  = room >= 648.0f;
    const float width = room < 768.0f ? room : 768.0f;
    const List *list  = find_list(b, c->list);

    if (rcBeginModal("card", &st.cardOpen)) {
        rcColumn(.gap = 12, .wType = RC_PX(width)) {
            if (c->cover >= 0)
                rcBox(.bg = LABEL_COLOR[c->cover], .borderRadius = "all-lg", .w = "grow",
                      .h = "56px") {}
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                rcBox(.overflow = "hidden", .w = "grow") {
                    rcText(rcFormat(rcAppArena(app), "%s in %s", card_code(rcAppArena(app), b, *c),
                                    list ? list->title.c_str() : ""),
                           .color = PAL.muted, .size = 12, .wrap = "e", .select = RC_SELECT_NONE);
                }
                if (icon_button("card-close", rcIconX, "Close (Esc)"))
                    st.cardOpen = false;
            }
            if (wide) {
                rcRow(.gap = 24, .align = "tl", .w = "grow") {
                    rcColumn(.gap = 12, .w = "grow") { card_main(st, app, *c, wide); }
                    rcColumn(.gap = 8, .w = "208px") { card_rail(st, app, *c); }
                }
            } else {
                rcColumn(.gap = 12, .w = "grow") {
                    card_main(st, app, *c, wide);
                    card_rail(st, app, *c);
                }
            }
        }
        rcEndModal();
    }
    if (!st.cardOpen) {         /* however it closed, typing is not lost */
        commit_title(st, *c);
        commit_description(st, *c);
    }
}

/* ---- The Sync dialog ---- */

static inline RC_Color status_color(const LogRow &e) {
    if (e.isLocal || e.state == RC_FETCH_PENDING || e.state == RC_FETCH_CANCELED)
        return PAL.muted;
    if (e.state == RC_FETCH_OPEN || (e.status >= 200 && e.status < 300))
        return rcHex(0x216E4E);
    return PAL.danger;
}

static inline const char *status_text(RC_App *app, const LogRow &e) {
    if (e.isLocal)
        return "local";
    switch (e.state) {
    case RC_FETCH_PENDING:  return "...";
    case RC_FETCH_OPEN:     return "open";
    case RC_FETCH_CANCELED: return "canceled";
    case RC_FETCH_FAILED:   return "failed";
    default:                return rcFormat(rcAppArena(app), "%d", e.status).chars;
    }
}

/* One cell of the log: a line that ends in an ellipsis at its column's edge. */
static inline void log_cell(const char *text, RC_Color ink) {
    rcTextC(text, .color = ink, .size = 12, .wrap = "e", .select = RC_SELECT_NONE);
}

/* One request. The row is the target and carries the fill, so it answers as one. Every row
   declares all five cells: a column the table has hidden takes no room and shows nothing. */
static inline void request_row(AppState &st, RC_App *app, const LogRow &e) {
    RC_Arena *mem = rcAppArena(app);
    const char *id = rcFormat(mem, "req%d", e.seq).chars;
    const int   ms = static_cast<int>((e.endedAt - e.startedAt) * 1000.0);

    rcTableRow(.id = id,
               .className = st.sync.picked == e.seq ? "row-picked" : "hover:bg-surface-alt");
    log_cell(e.method, PAL.ink);
    rcTableNext();
    log_cell(e.path.c_str(), PAL.ink);
    rcTableNext();
    log_cell(status_text(app, e), status_color(e));
    rcTableNext();
    log_cell(e.endedAt > 0.0 ? rcFormat(mem, "%d ms", ms).chars : "", PAL.muted);
    rcTableNext();
    log_cell(e.isLocal ? "" : rcFormatInt(mem, static_cast<int64_t>(e.bytes), ",").chars,
             PAL.muted);
    if (rcClicked(id))
        st.sync.picked = e.seq;
}

/* A table never scrolls sideways: the columns that do not fit leave, the highest .priority
   first, and the tip at its top right counts them. The header is the table's own, so it stays
   put over the rows. */
static inline void request_table(AppState &st, RC_App *app, float width) {
    Sync &s = st.sync;
    RC_Arena *mem = rcAppArena(app);
    const bool any = s.logCount > 0 || s.streamRow.seq;
    /* Under a mouse the scrollbar takes its own 12 px past the last column while the rows
       overflow, so no number sits under it. A finger's shows only while it scrolls. */
    const RC_TableColumn cols[5] = {
        { .header = "Method", .wType = RC_PX(72), .align = "cl" },
        { .header = "Path", .align = "cl" },
        { .header = "Status", .wType = RC_PX(72), .align = "cr" },
        { .header = "Time", .wType = RC_PX(72), .align = "cr", .priority = 1 },
        { .header = "Bytes", .wType = RC_PX(72), .align = "cr", .priority = 2 },
    };
    /* Read ahead of the table, this is the last frame's answer, which is the one on screen. */
    const uint64_t hidden  = any ? rcTableHiddenColumns("requests") : 0;
    const int      dropped = static_cast<int>((hidden >> 3 & 1) + (hidden >> 4 & 1));

    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        strong_text("Requests");
        small_text(rcFormat(mem, "%d", s.logCount + (s.streamRow.seq ? 1 : 0)).chars);
        if (!s.on && width >= 480.0f)
            small_text("Sync is off: each change shows what it would send");
        rcSpacer() {}
        if (dropped)
            small_text(rcFormat(mem, "+%d column%s", dropped, dropped == 1 ? "" : "s").chars);
    }
    rcColumn(.borderRadius = "all-md", .border = { .color = PAL.line, .width = 1 }, .w = "grow",
             .h = "200px") {
        if (!any) {
            rcBox(.align = "cc", .w = "grow", .h = "grow") {
                small_text("Move a card, and what it sends shows here.");
            }
        } else if (rcBeginTable("requests", cols, 5)) {
            if (s.streamRow.seq)
                request_row(st, app, s.streamRow);
            for (int k = 0; k < s.logCount; k++) {
                const int row = (s.logHead - 1 - k + LOG_MAX) % LOG_MAX;
                request_row(st, app, s.log[static_cast<size_t>(row)]);
            }
            rcEndTable();
            rcScrollbar("requests");
        }
    }
}

/* What went over the wire for the picked row. Its box keeps one height, picked or not, so a
   click on a row does not move the rows. A body breaks at its spaces (spaced_json, sync.h),
   and only a word too long for a line of its own splits. */
static inline void request_detail(const Sync &s) {
    const LogRow *pick = s.picked && s.streamRow.seq == s.picked ? &s.streamRow : nullptr;
    for (const LogRow &e : s.log)
        if (s.picked && e.seq == s.picked)
            pick = &e;
    const auto block = [](const char *label, const std::string &text) {
        if (text.empty())
            return;
        section_label(label);
        rcTextC(text.c_str(), .color = PAL.ink, .size = 12, .wordBreak = "a");
    };
    rcColumn(.id = "request-detail", .bg = PAL.lane, .gap = 4, .p = 12, .scroll = "v",
             .borderRadius = "all-md", .w = "grow", .h = "128px") {
        if (pick) {
            block("Sent", pick->sent);
            block("Answered", pick->body);
            block("Note", pick->note);
        } else {
            small_text("Pick a request to see what it sent.");
        }
    }
    rcScrollbar("request-detail");
}

static inline void sync_form(Sync &s, RC_App *app) {
    section_label("Server URL");
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        /* Connected, the field is off and its address stays: a disabled field draws no x. */
        rcTextInput("sync-url", s.url, sizeof s.url, .disabled = s.on,
                    .className = control_class(), .placeholder = "http://127.0.0.1:8787",
                    .inputKind = RC_INPUT_URL, .clearButton = true);
        if (s.on) {
            if (rcButton("sync-disconnect", "Disconnect", .className = control_class()))
                sync_disconnect(app, s);
        } else if (rcButton("sync-connect", "Connect", .disabled = !s.url[0],
                            .className = control_class(), .variant = RC_BTN_PRIMARY)) {
            sync_connect(app, s);
        }
    }
    if (!s.on && !s.error.empty()) {
        rcRow(.bg = rcHex(0xFFECEB), .gap = 8, .p = 8, .align = "cl", .borderRadius = "all-md",
              .w = "grow") {
            rcIconTriangleAlert(16.0f, PAL.danger);
            rcBox(.w = "grow") {
                rcTextC(s.error.c_str(), .color = PAL.danger, .size = 12);
            }
        }
    }
    rcTextL("Point this at serve.py, the small server beside this example.", .color = PAL.muted,
            .size = 12);
    rcRow(.gap = 8, .align = "cl", .w = "grow", .className = "flex-wrap") {
        small_text("Next request");
        rcBox(.w = "132px") {
            rcCombo("sync-knob", &s.knob, KNOB_NAME, 3, .className = control_class());
        }
        rcSpacer() {}
        if (rcButton("sync-drop", "Drop stream", .disabled = !s.stream,
                     .className = control_class()))
            drop_stream(app, s);
    }
}

static inline void sync_dialog(AppState &st, RC_App *app, const RC_Viewport &view) {
    if (!st.syncOpen)
        return;
    const float room  = view.width - view.safe.left - view.safe.right - 72.0f;
    const float width = room < 640.0f ? room : 640.0f;

    if (rcBeginModal("sync", &st.syncOpen)) {
        rcColumn(.gap = 12, .wType = RC_PX(width)) {
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                rcIconRotateCw(20.0f, PAL.ink);
                strong_text("Sync with a server", 20.0f);
                rcSpacer() {}
                if (icon_button("sync-close", rcIconX, "Close (Esc)"))
                    st.syncOpen = false;
            }
            const char *boardName = st.boards.front().name.c_str();
            rcText(rcFormat(rcAppArena(app),
                            "Off until you connect: the app opens no socket before then. "
                            "Connecting replaces %s here with the server's copy. From then on it "
                            "goes through REST calls and one event stream, so two windows show "
                            "each other's moves.",
                            boardName),
                   .color = PAL.ink2, .size = 14, .wordBreak = "a");
            sync_form(st.sync, app);
            request_table(st, app, width);
            request_detail(st.sync);
        }
        rcEndModal();
    }
}

/* ---- The shortcut sheet ---- */

static inline void help_dialog(AppState &st, const RC_Viewport &view) {
    static const char *const KEYS[][2] = {
        { "n", "New card in the list under the pointer" },
        { "f or /", "Filter the cards" },
        { "x", "Clear the filter" },
        { "q", "Only my cards" },
        { "b", "Switch boards" },
        { "l", "Show label names" },
        { ", and .", "Move the card under the pointer one list left or right" },
        { "Enter", "Open the card under the pointer" },
        { "Esc", "Cancel a drag, close a dialog" },
        { "?", "This sheet" },
    };
    if (!st.helpOpen)
        return;
    const float room  = view.width - view.safe.left - view.safe.right - 72.0f;
    const float width = room < 480.0f ? room : 480.0f;
    if (rcBeginModal("shortcuts", &st.helpOpen)) {
        rcColumn(.gap = 8, .wType = RC_PX(width)) {
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                strong_text("Keyboard shortcuts", 20.0f);
                rcSpacer() {}
                if (icon_button("help-close", rcIconX, "Close (Esc)"))
                    st.helpOpen = false;
            }
            for (const auto &key : KEYS) {
                rcRow(.gap = 12, .py = 4, .align = "cl", .w = "grow") {
                    rcBox(.w = "72px") {
                        rcBox(.bg = PAL.lane, .px = 8, .py = 2, .borderRadius = "all-sm") {
                            strong_text(key[0], 12.0f);
                        }
                    }
                    rcBox(.w = "grow") {
                        rcTextC(key[1], .color = PAL.ink2, .size = 14);
                    }
                }
            }
        }
        rcEndModal();
    }
}

#endif /* KANBAN_DIALOGS_H */
