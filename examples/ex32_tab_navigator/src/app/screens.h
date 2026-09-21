/*
    screens.h - the four tabs: each one a list, and the detail it pushes

    The shells in app.h know only that a tab has a list_screen and a detail_screen
    and that both draw into whatever column they are handed. That is the whole
    contract, and it is what lets both arms show the same screens.

    Every list is a scroll container carrying its tab's LIST_ID and every detail
    one carrying DETAIL_ID: app.h keeps the scroll offset by those ids.

    DO NOT `return` EARLY OUT OF A CONTAINER BODY. The brace body is a
    macro-generated loop, so a return skips the close and you get a completed
    frame that is the wrong frame. Empty states are else branches.
*/
#ifndef APP_SCREENS_H
#define APP_SCREENS_H

/* The hairline under a row, inset to the text's left edge. It lives INSIDE the
   row's fixed pitch, so a virtual list's arithmetic still holds. */
static inline void hairline(int inset)
{
    rcRow(.pl = (uint16_t)inset, .w = "grow", .hType = RC_PX(1)) {
        rcBox(.bg = rcGetStyle().border, .w = "grow", .h = "grow") {}
    }
}

/* A row's fill: the pushed item's row is lit IN ITS TAB'S ACCENT so the wide arm
   shows which item the pane beside it holds; otherwise a hover wash. */
static inline RC_Color row_fill(const AppState *st, Tab t, int index, const char *id)
{
    if (st->nav[t].depth > 0 && st->nav[t].item == index)
        return rcAlpha(tab_accent(st, t), 46);
    return rcAlpha(rcGetStyle().border, rcIsHovered(id) ? 70 : 0);
}

/* The empty state every list shares. Never a bare "nothing found" - a list empty
   because of a filter has to say which filter, or the app looks broken. */
static inline void nothing_here(const char *title, const char *hint)
{
    RC_Style s = rcGetStyle();
    rcColumn(.gap = 6, .p = 40, .align = "tc", .w = "grow") {
        rcTextC(title, .font = F_BODY, .color = s.text);
        rcTextC(hint, .font = F_SMALL, .color = s.textMuted);
    }
}

/* HOME: a feed of 300 posts, and what you have done with them.
   rcVirtualList declares the VIEWPORT, not the dataset: about a dozen rows are
   ever on the element tree, so 300 posts cost what 12 do. The PITCH is the one
   number it needs exact, and each row's id is its DATA index.
   The filter strip sits OUTSIDE the scroll container, so it never stands between
   rcVirtualList and the container it measures its window against. */
static inline void home_list(RC_App *app, AppState *st)
{
    RC_Style       s      = rcGetStyle();
    RC_Arena      *m      = rcAppArena(app);
    const RC_Color accent = tab_accent(st, TAB_HOME);
    const int      pitch  = st->compactRows ? ROW_H_COMPACT : ROW_H;
    const int      disc   = st->compactRows ? AVATAR_COMPACT : AVATAR;
    const uint16_t px     = body_px(st);
    int            i;

    rcRow(.gap = 8, .px = PAD, .py = 10, .align = "cl", .w = "grow") {
        for (i = 0; i < FEED_FILTERS; i++) {
            if (filter_chip(FEED_ID[i], rcFormat(m, "%s %d", FEED_NAME[i], post_count(st, i)),
                            st->feedFilter == i, accent, false)) {
                st->feedFilter = i;
                rebuild_feed(st);
            }
        }
    }
    rule();
    rcColumn(.id = "home_list", .scroll = "v", .w = "grow", .h = "grow",
             .scrollOffset = { 0.0f, st->nav[TAB_HOME].listY }) {
        if (!st->feedCount) {
            if (st->feedFilter == FEED_SAVED)
                nothing_here("No post is saved yet.", "Open one and turn Save this post on.");
            else
                nothing_here("Nothing left unread.", "Every post in the feed has been opened.");
        }
        rcVirtualList(row, "home_list", st->feedCount, (float)pitch) {
            const int   idx = st->feed[row.index];
            const Post *p   = &st->post[idx];
            const char *id  = rcFormat(m, "post%d", idx).chars;

            rcColumn(.id = id, .bg = row_fill(st, TAB_HOME, idx, id), .w = "grow",
                     .hType = RC_PX(pitch)) {
                rcRow(.gap = 10, .px = PAD, .align = "cl", .w = "grow", .h = "grow") {
                    row_mark(!p->read, st->nav[TAB_HOME].depth > 0 && st->nav[TAB_HOME].item == idx,
                             accent);
                    avatar(m, AVATAR_TINT[p->tint], WHO[p->who][0], disc);
                    /* .wrap = "n" on every line: a fixed-pitch row has no second
                       line to give, and the full text is one tap away. */
                    rcColumn(.gap = 3, .w = "grow") {
                        rcText(post_title(m, p), .font = F_BODY,
                                .color = title_ink(!p->read), .size = px, .wrap = "n");
                        if (!st->compactRows)
                            rcTextC(SUBJECT[p->noun].lede, .font = F_SMALL, .color = s.textMuted,
                                     .wrap = "n");
                        rcRow(.gap = 8, .align = "cl") {
                            rcText(rcFormat(m, "%s  %s", WHO[p->who], age_text(m, p->age).chars),
                                    .font = F_SMALL, .color = s.textMuted, .wrap = "n");
                            /* NEUTRAL, NOT THE ACCENT: the accent already says
                               "this tab", "this filter" and "unread" here, and a
                               fourth meaning on one hue is no signal at all. */
                            if (p->saved) chip("Saved", s.textMuted, rcAlpha(s.border, 130));
                        }
                    }
                    rcIconChevronRight(18.0f, s.border);
                }
                hairline((int)PAD + MARK_W + 10 + disc + 10);
            }
            /* Opening a post reads it: the mark goes, the title mutes, and the
               Unread chip's count drops on the same frame. */
            if (rcClicked(id)) {
                st->post[idx].read = true;
                push(st, TAB_HOME, idx);
            }
        }
    }
}

static inline void home_detail(RC_App *app, AppState *st)
{
    RC_Style       s      = rcGetStyle();
    RC_Arena      *m      = rcAppArena(app);
    const RC_Color accent = tab_accent(st, TAB_HOME);
    const int      item   = st->nav[TAB_HOME].item;
    Post          *p      = &st->post[item];
    const uint16_t px     = body_px(st);
    int            i, pos = -1;

    /* Where this post sits in the FILTERED feed: "3 of 36 in Saved" is an answer,
       "3 of 300" while looking at a filter is not. A post CAN be absent - open one
       from Unread and the next rebuild drops it - and -1 says so. */
    for (i = 0; i < st->feedCount; i++)
        if (st->feed[i] == item) { pos = i; break; }

    rcColumn(.id = "home_detail", .p = PAD, .scroll = "v", .w = "grow", .h = "grow") {
        rcColumn(.gap = 14, .w = "grow", .wMax = (float)READ_W) {
            /* The subject, the one chip this screen carries, and what the other
               paragraphs are drawn from. */
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                chip(SUBJECT[p->noun].noun, accent, rcAlpha(accent, 38));
            }
            rcText(post_title(m, p), .font = F_TITLE, .color = s.text);
            rcTextC(SUBJECT[p->noun].lede, .font = F_BODY, .color = s.textMuted,
                     .size = (uint16_t)(px + 1), .lineHeight = (uint16_t)(px + 9));
            rcRow(.gap = 10, .align = "cl", .w = "grow") {
                avatar(m, AVATAR_TINT[p->tint], WHO[p->who][0], AVATAR_COMPACT);
                rcColumn(.gap = 2) {
                    rcTextC(WHO[p->who], .font = F_BODY, .color = s.text);
                    rcText(rcFormat(m, "%s ago", age_text(m, p->age).chars), .font = F_SMALL, .color = s.textMuted);
                }
            }
            rule();
            /* .lineHeight REPLACES the font-derived line box, so it tracks the
               slider: 7 px of leading over whatever size the reader chose. */
            for (i = 0; i < 3; i++)
                rcTextC(paragraph(p, i), .font = F_BODY, .color = s.text, .size = px,
                         .lineHeight = (uint16_t)(px + 7));
            rule();
            rcRow(.gap = 12, .align = "cl", .w = "grow") {
                rcTextL("Save this post", .font = F_BODY, .color = s.text);
                rcBox(.w = "grow") {}
                rcToggle("home_save", &p->saved);
            }
            /* "Next" REPLACES the top of the stack rather than pushing a second
               level, so the header keeps one back arrow and back still lands on
               the list. It walks the FILTERED feed. */
            rcRow(.gap = 12, .align = "cl", .w = "grow") {
                if (action("home_next", "Next post", accent, hit_size()) && st->feedCount) {
                    const int next = st->feed[(pos + 1) % st->feedCount];

                    st->post[next].read = true;
                    push(st, TAB_HOME, next);
                }
                if (pos >= 0)
                    rcText(rcFormat(m, "%d of %d in %s", pos + 1, st->feedCount,
                                    FEED_NAME[st->feedFilter]),
                            .font = F_SMALL, .color = s.textMuted);
                else
                    rcText(rcFormat(m, "Not in %s", FEED_NAME[st->feedFilter]),
                            .font = F_SMALL, .color = s.textMuted);
            }
        }
    }
}

/* The category strip is a GRID because a chip cloud has no single width: one
   more than the table, three to a row, fitted to whatever CATEGORY holds. */
enum { CAT_CHIPS = 1 + N(CATEGORY), CAT_PER_ROW = 3 };

/* SEARCH: a query and a category over 120 named items.
   A plain loop, not a virtual list: 120 rows are cheap to declare, so the feed is
   where declaring the viewport starts to pay and this is where it would only add
   a moving part. A ROW HERE IS A RESULT, NOT A POST. */
static inline void search_list(RC_App *app, AppState *st)
{
    RC_Style       s      = rcGetStyle();
    RC_Arena      *m      = rcAppArena(app);
    const RC_Color accent = tab_accent(st, TAB_SEARCH);
    const uint16_t px     = body_px(st);
    int            i, r;

    /* THE FIELD'S TARGET IS THE WHOLE ROW. rcTextInput sizes its box from its
       font, under the HIT_MIN a finger needs, so the row is HIT_MIN tall and
       forwards a tap that lands on it but not on the box. */
    rcRow(.id = "search_bar", .gap = 8, .px = PAD, .align = "cl", .w = "grow",
          .hType = RC_PX(HIT_MIN)) {
        rcBox(.w = "grow") {
            if (rcTextInput("search_q", st->query, sizeof st->query,
                             .placeholder = "Search 120 items", .font = F_BODY))
                rebuild_matches(st);
        }
        rcText(rcFormat(m, "%d of %d", st->matchCount, ITEMS), .font = F_SMALL, .color = s.textMuted, .wrap = "n");
        if (st->query[0]) {
            rcBox(.id = "search_clear", .bg = rcAlpha(s.border, rcIsHovered("search_clear") ? 90 : 0),
                  .align = "cc", .borderRadius = "all-full",
                  .wType = RC_PX(HIT_MIN - 8), .hType = RC_PX(HIT_MIN - 8)) {
                rcIconX(15.0f, s.textMuted);
            }
        }
    }
    /* THE ANCESTOR'S POLL MUST NOT RUN WHILE THE PILL IS UNDER THE POINTER, and
       the operand order below is the whole fix. One press latch is kept per
       frame and the LAST poll to see the pointer over it wins. "search_clear"
       sits INSIDE "search_bar", so a press on the X sets the latch and the row's
       own rcClicked - which the pointer is also over - immediately takes it back.
       The release then matches the row, the X never fires, and nothing warns.
       Asking the cheap hover questions FIRST short-circuits the row's poll away
       on exactly the frames the X needs it gone. */
    if (st->query[0] && rcClicked("search_clear")) {
        st->query[0] = '\0';
        rebuild_matches(st);
        rcSetFocus("search_q");      /* clearing a field leaves you IN it */
    } else if (!rcIsHovered("search_q") && !rcIsHovered("search_clear") && rcClicked("search_bar")) {
        rcSetFocus("search_q");
    }
    /* AND GIVE THE FIELD ITS I-BEAM BACK. rcClicked above hints the hand for the
       ROW, and a hovered element's ancestors count as hovered. The frame cursor is
       last-writer-wins, and the hand is right over the row's hit slop, so narrow
       the I-beam onto the field rather than dropping the poll. */
    if (rcIsHovered("search_q"))
        rcSetCursor(RC_CURSOR_TEXT);
    /* THREE TO A ROW, counted from the table, so a sixth category lands in the
       grid on its own. A chip strip that scrolled sideways would hide a filter
       behind a gesture; two short rows show every one of them at LIST_W.
       EVERY PILL THE SAME WIDTH, and the empty tail below is what keeps it that
       way: the pills GROW to share their row, so a final row holding one chip
       would stretch that chip across the whole strip and the grid would stop
       looking like a grid. The spacers give the short row the siblings it needs. */
    rcColumn(.gap = 8, .px = PAD, .py = 10, .w = "grow") {
        for (r = 0; r * CAT_PER_ROW < CAT_CHIPS; r++) {
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                for (i = r * CAT_PER_ROW; i < CAT_CHIPS && i < (r + 1) * CAT_PER_ROW; i++) {
                    const char *id = rcFormat(m, "cat%d", i).chars;

                    if (filter_chip(id, i ? rcStringFromCStr(CATEGORY[i - 1]) : rcStringFromCStr("All"),
                                    st->category == i, accent, true)) {
                        st->category = i;
                        rebuild_matches(st);
                    }
                }
                for (; i < (r + 1) * CAT_PER_ROW; i++)
                    rcBox(.w = "grow") {}
            }
        }
    }
    rule();
    rcColumn(.id = "search_list", .scroll = "v", .w = "grow", .h = "grow",
             .scrollOffset = { 0.0f, st->nav[TAB_SEARCH].listY }) {
        if (!st->matchCount)
            nothing_here("No item matches that.", "Try another category, or clear the field.");
        for (i = 0; i < st->matchCount; i++) {
            const int   idx = st->match[i];
            const Item *it  = &st->item[idx];
            const char *id  = rcFormat(m, "item%d", idx).chars;

            rcColumn(.id = id, .bg = row_fill(st, TAB_SEARCH, idx, id), .w = "grow",
                     .hType = RC_PX(ROW_H_PLAIN)) {
                rcRow(.gap = 12, .px = PAD, .align = "cl", .w = "grow", .h = "grow") {
                    /* A catalogue has no unread, so this column only ever carries
                       the OPEN bar; it is kept on every row so the names stay
                       aligned. */
                    row_mark(false, st->nav[TAB_SEARCH].depth > 0 && st->nav[TAB_SEARCH].item == idx,
                             accent);
                    rcColumn(.gap = 4, .w = "grow") {
                        rcText(item_name(m, it), .font = F_BODY, .color = s.text, .size = px, .wrap = "n");
                        rcRow(.gap = 6, .align = "cl") {
                            chip(CATEGORY[it->cat], accent, rcAlpha(accent, 34));
                            rcTextC(TAG[it->tag[0]], .font = F_SMALL, .color = s.textMuted, .wrap = "n");
                        }
                    }
                    rcText(rcFormat(m, "%u g", (unsigned)it->grams), .font = F_BODY, .color = s.text,
                            .wrap = "n");
                    rcIconChevronRight(18.0f, s.border);
                }
                hairline((int)PAD + MARK_W + 12);
            }
            if (rcClicked(id)) push(st, TAB_SEARCH, idx);
        }
    }
}

static inline void search_detail(RC_App *app, AppState *st)
{
    RC_Style       s      = rcGetStyle();
    RC_Arena      *m      = rcAppArena(app);
    const RC_Color accent = tab_accent(st, TAB_SEARCH);
    const Item    *it     = &st->item[st->nav[TAB_SEARCH].item];
    int            i;

    rcColumn(.id = "search_detail", .p = PAD, .scroll = "v", .w = "grow", .h = "grow") {
        rcColumn(.gap = 14, .w = "grow", .wMax = (float)READ_W) {
            rcText(item_name(m, it), .font = F_TITLE, .color = s.text);
            /* The category is what a chip in the strip can FILTER by, so it
               wears the accent; the tags are description and stay neutral. */
            rcRow(.gap = 6, .align = "cl", .w = "grow") {
                chip(CATEGORY[it->cat], accent, rcAlpha(accent, 38));
                for (i = 0; i < 3; i++) chip(TAG[it->tag[i]], s.textMuted, rcAlpha(s.border, 110));
            }
            rule();
            field("Category", rcStringFromCStr(CATEGORY[it->cat]));
            field("Weight",   rcFormat(m, "%u.%u kg", (unsigned)it->grams / 1000u,
                                       ((unsigned)it->grams % 1000u) / 100u));
            field("Added",    rcFormat(m, "%u days ago", (unsigned)it->addedDays));
            rule();
            rcText(rcFormat(m, "Filed under %s. One of %d items the Search tab filters as you type; "
                               "the count beside the field is the whole answer, and the list is "
                               "the same index order the records were seeded in.",
                            CATEGORY[it->cat], ITEMS),
                    .font = F_BODY, .color = s.textMuted, .size = body_px(st),
                    .lineHeight = (uint16_t)(body_px(st) + 7));
        }
    }
}

/* ALERTS: two boxes, newest first, with an unread mark.
   THE ONE LIST IN THIS APP WHOSE ROWS LEAVE IT. An alert you archive is somewhere
   you can go and look at rather than gone, and switching boxes pops the stack,
   because a detail pushed from one box has no place over the other. */
static inline void alerts_list(RC_App *app, AppState *st)
{
    RC_Style       s      = rcGetStyle();
    RC_Arena      *m      = rcAppArena(app);
    const RC_Color accent = tab_accent(st, TAB_ALERTS);
    const uint16_t px     = body_px(st);
    int            i, shown = 0;

    rcRow(.gap = 8, .px = PAD, .py = 10, .align = "cl", .w = "grow") {
        for (i = 0; i < 2; i++) {
            if (filter_chip(BOX_ID[i], rcFormat(m, "%s %d", BOX_NAME[i], box_count(st, i != 0)),
                            st->showArchived == (i != 0), accent, false)) {
                st->showArchived = i != 0;
                pop(st, TAB_ALERTS);
            }
        }
    }
    rule();
    rcColumn(.id = "alerts_list", .scroll = "v", .w = "grow", .h = "grow",
             .scrollOffset = { 0.0f, st->nav[TAB_ALERTS].listY }) {
        for (i = 0; i < ALERTS; i++) {
            const Alert *a      = &st->alert[i];
            const char  *id     = rcFormat(m, "alert%d", i).chars;
            const bool   unread = a->unread && !a->archived;

            if (a->archived != st->showArchived) continue;
            shown++;
            rcColumn(.id = id, .bg = row_fill(st, TAB_ALERTS, i, id), .w = "grow",
                     .hType = RC_PX(ROW_H_PLAIN)) {
                rcRow(.gap = 10, .px = PAD, .align = "cl", .w = "grow", .h = "grow") {
                    row_mark(unread, st->nav[TAB_ALERTS].depth > 0 && st->nav[TAB_ALERTS].item == i,
                             accent);
                    rcColumn(.gap = 4, .w = "grow") {
                        rcTextC(ALERT_KIND[a->text].text, .font = F_BODY,
                                 .color = title_ink(unread), .size = px, .wrap = "n");
                        /* The severity chip carries its WORD, so the scale
                           survives a reader who cannot separate amber from red. */
                        rcRow(.gap = 8, .align = "cl") {
                            chip(SEVERITY[a->severity], severity_color(st, a->severity),
                                 rcAlpha(severity_color(st, a->severity), 34));
                            rcText(stamp_text(m, a->minutesAgo), .font = F_SMALL, .color = s.textMuted,
                                    .wrap = "n");
                        }
                    }
                    rcIconChevronRight(18.0f, s.border);
                }
                hairline((int)PAD + MARK_W + 10);
            }
            /* Opening an alert reads it: the mark goes and the bar item's count
               recounts on the same frame. */
            if (rcClicked(id)) {
                st->alert[i].unread = false;
                push(st, TAB_ALERTS, i);
            }
        }
        if (!shown) {
            if (st->showArchived) nothing_here("The archive is empty.", "Open an alert and archive it.");
            else                  nothing_here("Inbox zero.", "Everything here has been archived.");
        }
    }
}

static inline void alerts_detail(RC_App *app, AppState *st)
{
    RC_Style  s   = rcGetStyle();
    RC_Arena *m   = rcAppArena(app);
    const int idx    = st->nav[TAB_ALERTS].item;
    Alert    *a      = &st->alert[idx];
    RC_Color  sev    = severity_color(st, a->severity);
    RC_Color  accent = tab_accent(st, TAB_ALERTS);

    rcColumn(.id = "alerts_detail", .p = PAD, .scroll = "v", .w = "grow", .h = "grow") {
        rcColumn(.gap = 14, .w = "grow", .wMax = (float)READ_W) {
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                chip(SEVERITY[a->severity], sev, rcAlpha(sev, 40));
                if (a->archived)
                    chip("Archived", s.textMuted, rcAlpha(s.border, 110));
                rcText(stamp_text(m, a->minutesAgo), .font = F_SMALL, .color = s.textMuted);
            }
            rcTextC(ALERT_KIND[a->text].text, .font = F_TITLE, .color = s.text);
            rule();
            field("Severity", rcStringFromCStr(SEVERITY[a->severity]));
            field("Raised",   stamp_text(m, a->minutesAgo));
            field("Box",      rcStringFromCStr(a->archived ? "Archived" : "Inbox"));
            rule();
            rcText(rcFormat(m, "Raised %s and filed as %s. Archiving moves it to the other box "
                               "and takes you back; the counts on the two chips and the number on "
                               "the Alerts tab all follow from the same records.",
                            stamp_text(m, a->minutesAgo).chars, SEVERITY[a->severity]),
                    .font = F_BODY, .color = s.text, .size = body_px(st),
                    .lineHeight = (uint16_t)(body_px(st) + 7));
            rule();
            /* ONE ACTION, AND IT IS THE ONE THIS BOX ALLOWS: the inbox archives,
               the archive restores. Either pops, because the list is where you
               belong once an item has moved out of it. */
            rcRow(.w = "grow") {
                if (a->archived) {
                    if (action("alerts_restore", "Move to inbox", accent, hit_size())) {
                        a->archived = false;
                        pop(st, TAB_ALERTS);
                    }
                } else {
                    if (action("alerts_archive", "Archive", accent, hit_size())) {
                        a->archived = true;
                        pop(st, TAB_ALERTS);
                    }
                }
            }
        }
    }
}

/* PROFILE: the account, what the other tabs hold, and settings. */

/* The label cell of a settings row: fixed so the controls line up. */
static inline void setting_label(const char *label)
{
    rcBox(.wType = RC_PX(112)) { rcTextC(label, .font = F_BODY, .color = rcGetStyle().text); }
}

/* A section heading: the one piece of type in this app smaller than the thing it
   names, which is how a settings screen says "a group starts here". */
static inline void section(const char *label)
{
    rcBox(.pt = 18, .pb = 6, .px = PAD) {
        rcTextC(label, .font = F_MICRO, .color = rcGetStyle().textMuted);
    }
}

/* ONE CELL OF THE COUNTS CARD. Deliberately not a button and not accented: a
   tile that looked tappable would promise a fourth way to navigate. */
static inline void stat_tile(RC_String value, const char *label)
{
    RC_Style s = rcGetStyle();

    rcColumn(.gap = 2, .align = "cc", .w = "grow") {
        rcText(value, .font = F_TITLE, .color = s.text);
        rcTextC(label, .font = F_MICRO, .color = s.textMuted);
    }
}

static inline void profile_list(RC_App *app, AppState *st)
{
    RC_Style       s      = rcGetStyle();
    RC_Arena      *m      = rcAppArena(app);
    const RC_Color accent = tab_accent(st, TAB_PROFILE);

    rcColumn(.id = "profile_list", .scroll = "v", .w = "grow", .h = "grow",
             .scrollOffset = { 0.0f, st->nav[TAB_PROFILE].listY }) {
        rcRow(.gap = 14, .p = PAD, .align = "cl", .w = "grow") {
            avatar(m, accent, st->name[0] ? st->name[0] : '?', AVATAR_LARGE);
            rcColumn(.gap = 2) {
                rcTextC(st->name[0] ? st->name : "Your name", .font = F_HEAD, .color = s.text);
                rcTextL("RayClay Pocket", .font = F_SMALL, .color = s.textMuted);
            }
        }
        /* The other three tabs, as three numbers, each counted out of the same
           records the tab that owns it draws from, so the card cannot drift. */
        rcBox(.pb = 4, .px = PAD, .w = "grow") {
            rcRow(.bg = s.surface, .py = 14, .align = "cc", .borderRadius = "all-lg",
                  .border = { .color = s.border, .width = "1px" }, .w = "grow") {
                stat_tile(rcFormat(m, "%d", unread_count(st)), "Unread");
                rcBox(.bg = s.border, .wType = RC_PX(1), .hType = RC_PX(30)) {}
                stat_tile(rcFormat(m, "%d", post_count(st, FEED_SAVED)), "Saved");
                rcBox(.bg = s.border, .wType = RC_PX(1), .hType = RC_PX(30)) {}
                stat_tile(rcFormat(m, "%d", box_count(st, true)), "Archived");
            }
        }
        section("SETTINGS");
        rule();
        rcRow(.gap = 12, .px = PAD, .align = "cl", .w = "grow", .hType = RC_PX(ROW_H_PLAIN)) {
            setting_label("Name");
            rcBox(.w = "grow") {
                rcTextInput("profile_name", st->name, sizeof st->name, .placeholder = "Type a name", .font = F_BODY);
            }
        }
        /* The toggle reaches into another tab: the Home feed's pitch. Nothing to
           synchronise - the feed reads the flag on its next frame. */
        rcRow(.gap = 12, .px = PAD, .align = "cl", .w = "grow", .hType = RC_PX(ROW_H_PLAIN)) {
            setting_label("Compact rows");
            rcBox(.w = "grow") {}
            rcToggle("profile_compact", &st->compactRows);
        }
        rcRow(.gap = 12, .px = PAD, .align = "cl", .w = "grow", .hType = RC_PX(ROW_H_PLAIN)) {
            setting_label("Theme");
            rcBox(.w = "grow") {}
            rcCombo("profile_theme", &st->theme, THEME_NAME, N(THEME_NAME));
        }
        rcRow(.gap = 12, .px = PAD, .align = "cl", .w = "grow", .hType = RC_PX(ROW_H_PLAIN)) {
            setting_label("Text size");
            rcBox(.w = "grow") { rcSlider("profile_text", &st->textSize, (float)TEXT_MIN, (float)TEXT_MAX); }
            rcBox(.wType = RC_PX(40)) {
                rcText(rcFormat(m, "%u px", (unsigned)body_px(st)), .font = F_SMALL, .color = s.textMuted);
            }
        }
        section("ABOUT");
        rule();
        rcRow(.id = "profile_about", .bg = rcAlpha(s.border, rcIsHovered("profile_about") ? 70 : 0),
              .gap = 12, .px = PAD, .align = "cl", .w = "grow", .hType = RC_PX(ROW_H_PLAIN)) {
            rcTextL("About RayClay Pocket", .font = F_BODY, .color = s.text);
            rcBox(.w = "grow") {}
            rcTextL(RC_VERSION, .font = F_SMALL, .color = s.textMuted);
            rcIconChevronRight(18.0f, s.border);
        }
        if (rcClicked("profile_about")) push(st, TAB_PROFILE, 0);
        rule();
        /* A settings list ends; it does not just stop. */
        rcColumn(.gap = 4, .p = 24, .align = "tc", .w = "grow") {
            rcTextL("One C file for the desktop, the browser, iOS and Android.",
                     .font = F_SMALL, .color = s.textMuted, .textAlign = "c");
        }
    }
}

/* One rule of the navigation model, where a reader of the APP will meet it. */
static inline void about_line(const char *title, const char *body, uint16_t px)
{
    RC_Style s = rcGetStyle();

    rcColumn(.gap = 3, .w = "grow") {
        rcTextC(title, .font = F_BODY, .color = s.text);
        rcTextC(body, .font = F_SMALL, .color = s.textMuted, .lineHeight = (uint16_t)(px + 5));
    }
}

static inline void profile_detail(RC_App *app, AppState *st)
{
    RC_Style       s  = rcGetStyle();
    RC_Arena      *m  = rcAppArena(app);
    const uint16_t px = body_px(st);

    rcColumn(.id = "profile_detail", .p = PAD, .align = "tc", .scroll = "v", .w = "grow", .h = "grow") {
        rcColumn(.gap = 10, .align = "tc", .w = "grow", .wMax = (float)READ_W) {
            rcBox(.pt = 24, .pb = 8) { rcIconRayClayLogo(72.0f); }
            rcTextL("RayClay Pocket", .font = F_TITLE, .color = s.text);
            rcTextL("Version " RC_VERSION, .font = F_SMALL, .color = s.textMuted);
            rcBox(.h = "8px") {}
            rcText(rcFormat(m, "One source. This app is the same C file on the desktop, in the browser, "
                               "on iOS and on Android. It opens at 393 by 852, the phone it was designed "
                               "for, and grows a rail and a second pane only past %d units.", WIDE_W),
                    .font = F_BODY, .color = s.text, .size = px, .lineHeight = (uint16_t)(px + 7), .textAlign = "c");
            rcBox(.h = "8px") {}
            rule();
            rcColumn(.gap = 14, .pt = 4, .w = "grow") {
                about_line("A stack for every tab",
                           "Each tab keeps its own depth, its own item and its own scroll "
                           "position, and switching tabs never pops one.", px);
                about_line("The lit tab pops to its root",
                           "Tapping the tab you are already on goes back to its list. The pill "
                           "over an unlit tab says that tab has a screen waiting.", px);
                about_line("One back control",
                           "The arrow, the Escape key and Android's back button are the same "
                           "path: close the keyboard, else pop, else return to Home.", px);
                about_line("One accent per tab",
                           "The surfaces belong to the theme; the accent belongs to the tab "
                           "you are on, down to the toggles and the buttons.", px);
            }
            if (st->name[0])
                rcText(rcFormat(m, "Signed in as %s.", st->name), .font = F_SMALL, .color = s.textMuted);
        }
    }
}

/* THE WIDE ARM'S SECOND PANE WITH NOTHING OPEN IN IT: a screen with a state
   rather than a placeholder, each tab saying what IT holds. Nothing here carries
   an id - the list beside it is where a tap belongs. */
static inline void empty_pane(RC_App *app, AppState *st, Tab t)
{
    RC_Style       s      = rcGetStyle();
    RC_Arena      *m      = rcAppArena(app);
    const RC_Color accent = tab_accent(st, t);
    RC_String      head   = rcStringFromCStr("RayClay Pocket");
    RC_String      sub    = rcStringFromCStr("Version " RC_VERSION);
    const char    *hint   = "Open About for the four navigation rules.";

    switch (t) {
    case TAB_HOME:
        head = rcFormat(m, "%d posts", POSTS);
        sub  = rcFormat(m, "%d unread, %d saved", post_count(st, FEED_UNREAD),
                        post_count(st, FEED_SAVED));
        hint = "Pick a post from the feed to read it.";
        break;
    case TAB_SEARCH:
        head = rcFormat(m, "%d of %d items", st->matchCount, ITEMS);
        sub  = st->category ? rcFormat(m, "Filed under %s", CATEGORY[st->category - 1])
                            : rcStringFromCStr("Every category");
        hint = "Pick a result to see what it is.";
        break;
    case TAB_ALERTS:
        head = rcFormat(m, "%d unread", unread_count(st));
        sub  = rcFormat(m, "%d in the inbox, %d archived", box_count(st, false),
                        box_count(st, true));
        hint = "Pick an alert to read it.";
        break;
    case TAB_PROFILE:
    case TAB_COUNT:
        break;
    }

    rcColumn(.gap = 6, .align = "cc", .w = "grow", .h = "grow") {
        rcBox(.pb = 10) { TAB_ICON[t](40.0f, rcAlpha(accent, 90)); }
        rcText(head, .font = F_TITLE, .color = s.text);
        rcText(sub, .font = F_BODY, .color = s.textMuted);
        rcTextC(hint, .font = F_SMALL, .color = s.textMuted);
    }
}

#endif /* APP_SCREENS_H */
