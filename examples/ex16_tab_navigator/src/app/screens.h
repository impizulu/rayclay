/*
    screens.h - everything a route can show

    A screen is a top band over one scroll container, and it declares both: what the band says,
    and what scrolls under it. app.h decides which screens are on stage and where. A screen only
    knows its NavScreen, whose scrollId is its one scroll container, so navigator.h can keep its
    place.

    AN ID NAMES ONE ELEMENT IN ONE FRAME, and the same post can be on two screens at once: the feed
    under a sliding page, the list beside a pane. So every id here carries the depth of the screen
    that declares it, the `scope`, as well as its data index.

    DO NOT `return` EARLY OUT OF A CONTAINER BODY. The brace body is a macro-generated loop, so a
    return skips the close and you get a completed frame that is the wrong frame. Empty states are
    else branches.
*/
#ifndef APP_SCREENS_H
#define APP_SCREENS_H

#include "rayclay.h"
#include "app/parts.h"

/* ------------------------------------------------------------------ the parts of a post */

/* The wide arm lights the row its pane has open, so the two panes read as one. */
static bool row_is_open(const AppState *st, int scope, RouteKind kind, int item) {
    const NavStack *n = &st->nav.stack[st->nav.tab];

    return st->nav.wide && scope == 0 && n->depth >= 1 && n->route[1].kind == (int)kind &&
           n->route[1].item == item;
}

/* A row's class: the accent's tint while the pane has it open, else the press, but not under a
   pointer pulling the feed down, which is not pressing the post it happened to land on.
   A row as wide as its list keeps that fill edge to edge. */
static const char *row_class(const AppState *st, int scope, RouteKind kind, int item) {
    if (row_is_open(st, scope, kind, item))
        return "bg-primary/10 ring-inset";
    return st->pull > 4.0f ? "ring-inset" : "nav-press ring-inset";
}

static bool follow_button(RC_Arena *m, AppState *st, int scope, int tag, PersonId who) {
    const char *id = rcFormat(m, "follow_%d_%d_%d", scope, tag, (int)who).chars;
    const bool  on = st->following[who];

    if (pill_button(id, on ? "Following" : "Follow", !on)) {
        toggle_follow(st, who);
        return true;
    }
    return false;
}

static void copy_post_link(RC_Arena *m, AppState *st, int idx) {
    const char *at = PERSON[st->post[idx].author].handle;

    rcClipboardSet(rcFormat(m, "https://perch.example/@%s/%d", at, idx).chars);
    toast(st, "Link copied", UNDO_NONE, 0);
}

/* An icon, its count, and a target no smaller than a finger. The icon has a box of its own, so a
   heart that jumps moves nothing beside it. */
static bool post_action(const char *id, RC_IconCallback icon, float size, RC_Color ink,
                        RC_String count) {
    rcRow(.id = id, .focusable = true, .gap = 2, .pr = (float)(count.length ? 8 : 0),
          .align = "cc", .borderRadius = "all-full", .hType = RC_PX(TARGET), .wMin = RC_VAL(TARGET),
          .className = "nav-press") {
        rcBox(.align = "cc", .wType = RC_PX(32), .hType = RC_PX(32)) {
            icon(size, ink);
        }
        if (count.length)
            rcText(count, .color = ink, .size = 13, .wrap = "n", .select = RC_SELECT_NONE);
    }
    return rcClicked(id);
}

/* The row every post ends in. Each control does what its icon says, on the frame it is tapped. */
static void post_actions(RC_App *app, AppState *st, int idx, int scope, bool onItsPage) {
    RC_Style    s       = rcGetStyle();
    RC_Arena   *m       = rcAppArena(app);
    Post       *p       = &st->post[idx];
    const char *replyId = rcFormat(m, "reply_%d_%d", scope, idx).chars;
    const char *boostId = rcFormat(m, "boost_%d_%d", scope, idx).chars;
    const char *likeId  = rcFormat(m, "like_%d_%d", scope, idx).chars;
    const char *saveId  = rcFormat(m, "save_%d_%d", scope, idx).chars;
    const char *shareId = rcFormat(m, "share_%d_%d", scope, idx).chars;
    RC_String   none    = rcStringFromCStr("");

    rcRow(.w = "grow", .hType = RC_PX(TARGET), .className = "items-center justify-between") {
        if (post_action(replyId, rcIconMessageCircle, 20.0f, s.textMuted,
                        p->replies ? count_text(m, p->replies) : none)) {
            st->focusReply = true;
            if (!onItsPage)
                navigate(st, scope, ROUTE_POST, idx);
        }
        if (post_action(boostId, rcIconRepeat, 20.0f, p->reposted ? PALETTE.repost : s.textMuted,
                        p->reposts ? count_text(m, p->reposts) : none))
            toggle_repost(st, idx);
        if (post_action(likeId, p->liked ? rcIconHeartFilled : rcIconHeart,
                        20.0f * heart_scale(st, idx), p->liked ? PALETTE.heart : s.textMuted,
                        p->likes ? count_text(m, p->likes) : none))
            toggle_like(st, idx);
        if (post_action(saveId, p->saved ? rcIconBookmarkFilled : rcIconBookmark, 20.0f,
                        p->saved ? s.primary : s.textMuted, none))
            toggle_save(st, idx);
        if (post_action(shareId, rcIconShare, 20.0f, s.textMuted, none))
            copy_post_link(m, st, idx);
    }
}

/* A post's picture, 4:3 across its column. RC_FIT leaves the height to the ratio. A pinned height
   (RC_PX, RC_VH) is a ceiling: a ratio box is never taller than it, so where the column's width
   asks for more, the box is wider than 4:3 and RC_IMAGE_COVER cuts the picture's top and bottom. */
static void post_photo(const AppState *st, int idx, RC_Size height) {
    const Post *p = &st->post[idx];

    if (p->photo != NO_PHOTO && !st->compactFeed)
        rcBox(.borderRadius = "all-12", .w = "grow", .hType = height, .aspectRatio = 4.0f / 3.0f,
              .image = &st->photo[p->photo], .imageFit = RC_IMAGE_COVER) {}
}

/* Face and name open the author; the rest of the row belongs to the post. */
static void post_header(RC_App *app, AppState *st, int idx, int scope) {
    RC_Style       s      = rcGetStyle();
    RC_Arena      *m      = rcAppArena(app);
    const PersonId who    = st->post[idx].author;
    const char    *person = rcFormat(m, "who_%d_%d", scope, idx).chars;
    const char    *more   = rcFormat(m, "more_%d_%d", scope, idx).chars;
    const RC_String age   = age_text(m, st->post[idx].minutes);
    const RC_String handle = rcFormat(m, "@%s", PERSON[who].handle);
    const RC_String when   = rcFormat(m, DOT "%s", age.chars);

    rcRow(.gap = 4, .align = "cl", .w = "grow") {
        rcRow(.id = person, .focusable = true, .gap = 10, .py = 2, .align = "cl",
              .overflow = "hidden", .w = "grow") {
            avatar(st, who, 40);
            rcColumn(.gap = 1, .overflow = "hidden", .w = "grow") {
                rcTextC(person_name(st, who), .font = rcFontSemibold(15), .color = s.text,
                        .wrap = "e", .select = RC_SELECT_NONE);
                /* The handle gives way first: the age is what a reader looks for. */
                rcRow(.align = "cl", .w = "grow") {
                    rcBox(.overflow = "hidden", .className = "min-w-0") {
                        rcText(handle, .color = s.textMuted, .size = 13, .wrap = "e",
                               .select = RC_SELECT_NONE);
                    }
                    rcText(when, .color = s.textMuted, .size = 13, .wrap = "n",
                           .select = RC_SELECT_NONE);
                }
            }
        }
        if (rcClicked(person))
            navigate(st, scope, ROUTE_PERSON, (int)who);
        if (who != P_ME && !st->following[who])
            follow_button(m, st, scope, idx, who);
        if (icon_button(more, rcIconEllipsis, 20.0f, s.textMuted, true)) {
            st->sheetPost = idx;
            st->sheetPerson = who;
            open_sheet(st, SHEET_MENU);
        }
    }
}

/* One post in a list. The whole card opens it; everything inside takes its own tap first. */
static void post_card(RC_App *app, AppState *st, int idx, int scope) {
    RC_Style    s    = rcGetStyle();
    const Post *p    = &st->post[idx];
    const char *id   = rcFormat(rcAppArena(app), "post_%d_%d", scope, idx).chars;
    const int   step = st->textStep;

    rcColumn(.id = id, .focusable = true, .gap = 10, .pt = 12, .pb = 2, .px = GUTTER, .w = "grow",
             .className = row_class(st, scope, ROUTE_POST, idx)) {
        post_header(app, st, idx, scope);
        rcTextC(p->text, .color = s.text, .size = POST_PX[step], .lineHeight = POST_LH[step],
                .select = RC_SELECT_NONE);
        post_photo(st, idx, RC_FIT);
        post_actions(app, st, idx, scope, false);
    }
    if (rcClicked(id))
        navigate(st, scope, ROUTE_POST, idx);
    feed_gap(st);
}

/* ------------------------------------------------------------------ pull to refresh */

/* THE PULL IS THE APP'S OWN GESTURE, A MOUSE'S. At the very top of the feed a drag down has
   nothing left to scroll, so the app reads the pointer itself, at half speed, and opens a band
   above the list. A release past PULL_FIRE refreshes, and anything shorter springs back (update,
   in app.h). Once the list has moved, this press is a scroll and not a pull: a scrollbar's drag or
   the wheel ends it. A finger's drag in the feed is the feed's own pan, which cancels the press
   (rayclay.h, captureTouch), so on a phone a tap on the lit Home tab refreshes. */
static void refresh_band(AppState *st, const NavScreen *sc) {
    RC_Style s = rcGetStyle();
    float    h;

    if (rcPointerPressed(RC_POINTER_LEFT) && rcIsHovered(sc->scrollId) && sc->scrollY <= 0.0f &&
        !st->refreshing) {
        st->pulling = true;
        st->pullFromY = rcPointer().y;
    }
    if (st->pulling && sc->scrollY > 0.0f)
        st->pulling = false;
    if (st->pulling) {
        const float dy = rcPointer().y - st->pullFromY;

        if (rcPointerDown(RC_POINTER_LEFT)) {
            st->pull = dy > 0.0f ? dy * 0.5f : 0.0f;
        } else {
            st->pulling = false;
            st->pullWasDrag = st->pull > 4.0f;
            if (st->pull >= (float)PULL_FIRE)
                start_refresh(st);
        }
    }
    h = st->refreshing ? (float)PULL_REST : st->pull;
    st->spinnerSpokes = (int)(st->pull / (float)PULL_FIRE * 8.0f);
    if (h >= 1.0f) {
        rcRow(.align = "cc", .overflow = "hidden", .w = "grow", .hType = RC_PX((int)h)) {
            rcIconEmit(24.0f, s.textMuted, draw_spinner, st);
        }
    }
}

/* ------------------------------------------------------------------ Home */

static const char *const FEED_LABEL[FEED_MODES] = { "For you", "Following" };

static RC_String first_name(RC_Arena *m, const char *name) {
    int n = 0;

    while (name[n] && name[n] != ' ')
        n++;
    return rcFormat(m, "%.*s", n, name);
}

/* The people row. A ring of the brand's gradient says they have posted since you last looked, and
   a thin gray one that you have been to see. A tap opens their page. Following shows only them. */
static void people_strip(RC_App *app, AppState *st, int scope) {
    RC_Style  s = rcGetStyle();
    RC_Arena *m = rcAppArena(app);
    int       i;

    rcRow(.id = "people", .gap = 12, .pt = 12, .pb = 8, .px = GUTTER, .scroll = "h", .w = "grow") {
        for (i = 0; i < RINGED_COUNT; i++) {
            const PersonId who  = RINGED[i];
            const char    *id   = rcFormat(m, "ring_%d", i).chars;
            const bool     seen = st->ringSeen[i];

            if (st->feedMode == FEED_FOLLOWING && !st->following[who])
                continue;
            rcColumn(.id = id, .focusable = true, .gap = 6, .align = "tc", .wType = RC_PX(68)) {
                rcBox(.bg = seen ? s.border : RC_UNSET_COLOR, .align = "cc",
                      .borderRadius = "all-full", .wType = RC_PX(64), .hType = RC_PX(64),
                      .gradient = { .from = seen ? RC_UNSET_COLOR : PALETTE.brandFrom,
                                    .to = seen ? RC_UNSET_COLOR : PALETTE.brandTo, .dir = "u" }) {
                    rcBox(.bg = s.background, .align = "cc", .borderRadius = "all-full",
                          .wType = RC_PX(60), .hType = RC_PX(60)) {
                        avatar(st, who, 54);
                    }
                }
                rcText(first_name(m, PERSON[who].name), .color = seen ? s.textMuted : s.text,
                       .size = 11, .wrap = "n", .select = RC_SELECT_NONE);
            }
            if (rcClicked(id)) {
                st->ringSeen[i] = true;
                navigate(st, scope, ROUTE_PERSON, (int)who);
            }
        }
    }
    /* Under a mouse the strip's bar is a thumb with no track under the faces. A finger keeps the
       fading indicator, which any class would replace with a track (rayclay.h, rcScrollbar). */
    rcScrollbar("people", .className = rcPointerIsCoarse() ? "" : "bg-transparent");
    feed_gap(st);
}

static void home_root(RC_App *app, AppState *st, const NavScreen *sc) {
    RC_Style        s   = rcGetStyle();
    RC_Arena       *m   = rcAppArena(app);
    const NavButton btn = { "home_sort", rcIconArrowUpDown, "Feed options", false };
    int             mode = (int)st->feedMode, i;
    const char     *today = rcFormat(m, "%s, %s %d", DAY_NAME[st->todayWeekday],
                                     MONTH_NAME[st->todayMonth - 1], st->todayDay).chars;

    if (nav_top_band(app, &st->nav, sc, today, &btn, 1) == 0)
        open_sheet(st, SHEET_FEED);
    refresh_band(st, sc);
    rcColumn(.id = sc->scrollId, .bg = s.background, .align = "tC", .scroll = "v", .w = "grow",
             .h = "grow", .scrollOffset = { 0.0f, sc->scrollY }) {
        rcColumn(.w = "grow", .wMax = RC_VAL(READ_W)) {
            nav_large_title(&st->nav, "Home");
            if (underline_tabs(m, "feed", FEED_LABEL, FEED_MODES, &mode, true)) {
                st->feedMode = (FeedMode)mode;
                rebuild_feed(st);
            }
            if (!st->nav.low)
                people_strip(app, st, sc->depth);
            for (i = 0; i < st->feedCount; i++)
                post_card(app, st, st->feed[i], sc->depth);
            if (st->feedCount == 0)
                empty_state(rcIconUsers, "No posts to show",
                            "Follow a few people from Explore and their posts land here.");
            else
                rcRow(.gap = 8, .py = 24, .align = "cc", .w = "grow") {
                    rcIconCheck(16.0f, s.textMuted);
                    rcTextL("You are all caught up", .color = s.textMuted, .size = 13,
                            .select = RC_SELECT_NONE);
                }
        }
    }
    /* The tabs stay reachable once the list has scrolled them away: the same strip again, pinned
       over the list's top edge. A float on the list by id stays put while its content scrolls. In
       a low window no large title stands over them, so they are pinned from the first pixel.
       The copy takes no Tab stop: the strip in the list is that stop, right after the band, as a
       page's sticky tabs are, so the keys meet one set of tabs and in reading order. */
    if (sc->scrollY >= (st->nav.low ? 1.0f : (float)NAV_TITLE_H)) {
        rcColumn(.bg = s.background, .w = "grow", .wMax = RC_VAL(READ_W),
                 .floating = { .to = RC_ATTACH_ELEMENT, .toId = sc->scrollId,
                               .parent = RC_ANCHOR_TOP_CENTER, .element = RC_ANCHOR_TOP_CENTER }) {
            if (underline_tabs(m, "feedtop", FEED_LABEL, FEED_MODES, &mode, false)) {
                st->feedMode = (FeedMode)mode;
                rebuild_feed(st);
                rcScrollToTop(sc->scrollId);
            }
        }
    }
}

/* ------------------------------------------------------------------ a post's page */

/* A pill field's frame, h-11 (TARGET) tall. Its fill is surface on the dark ground and
   surfaceAlt on the light, so each ground is its own literal. A clear x needs no padding of its
   own: the field holds the x's room in its right padding (rayclay.h, .clearButton). */
static const char *pill_field_class(const AppState *st) {
    return st->dark ? "h-11 rounded-full bg-surface border-transparent px-4"
                    : "h-11 rounded-full bg-surface-alt border-transparent px-4";
}

static void stat_pair(RC_Arena *m, int value, const char *label) {
    RC_Style s = rcGetStyle();

    rcRow(.gap = 4, .align = "cl") {
        rcText(rcFormatInt(m, value, ","), .font = rcFontSemibold(13), .color = s.text,
               .select = RC_SELECT_NONE);
        rcTextC(label, .color = s.textMuted, .size = 13, .select = RC_SELECT_NONE);
    }
}

/* Who wrote a post's i-th reply: walked through everyone, and never the post's own author. */
static PersonId reply_author(const Post *p, int i) {
    const int who = 1 + (p->listIndex + i * 3 + 2) % (PERSON_COUNT - 1);

    return (PersonId)(who == (int)p->author ? (who % (PERSON_COUNT - 1)) + 1 : who);
}

static void reply_row(const AppState *st, RC_Arena *m, const char *id, PersonId who,
                      const char *text, int minutes) {
    RC_Style s = rcGetStyle();

    rcRow(.id = id, .gap = 10, .px = GUTTER, .py = 10, .w = "grow") {
        avatar(st, who, 32);
        rcColumn(.gap = 2, .w = "grow") {
            rcRow(.gap = 6, .align = "cl", .w = "grow") {
                rcTextC(person_name(st, who), .font = rcFontSemibold(13), .color = s.text);
                rcText(age_text(m, minutes), .color = s.textMuted, .size = 13);
            }
            rcTextC(text, .color = s.text, .size = POST_PX[st->textStep],
                    .lineHeight = POST_LH[st->textStep]);
        }
    }
}

/* The reply field, pinned under the page. It stays above the keyboard by the keyboard's own height,
   which is the tab bar's room as well: phone_shell hides the bar while a keyboard is up. Its row
   keeps the post's reading measure, so in a wide pane the field lines up with the post above it. */
static void reply_bar(RC_App *app, AppState *st, const NavScreen *sc, int idx) {
    RC_Style    s     = rcGetStyle();
    RC_Arena   *m     = rcAppArena(app);
    const char *field = rcFormat(m, "replybox_%d", sc->depth).chars;
    const char *send  = rcFormat(m, "replysend_%d", sc->depth).chars;
    const float kb    = rcViewport().keyboard.bottom;
    /* As compose's Post, Send reads off once the replies are full, rather than doing nothing. */
    const bool  ready = st->replyDraft[0] != '\0' && st->myReplyCount < MY_REPLY_CAP;

    if (st->focusReply) {
        rcSetFocus(field);
        st->focusReply = false;
    }
    if (ready && rcKeyPressedOn(field, RC_KEY_ENTER))
        publish_reply(st, idx);
    hairline();
    rcRow(.bg = s.background, .pt = 6, .pb = (float)(kb > 0.0f ? kb : 6.0f), .align = "tc",
          .w = "grow") {
        rcRow(.gap = 4, .pl = GUTTER, .pr = 6, .align = "cl", .w = "grow", .wMax = RC_VAL(READ_W)) {
            avatar(st, P_ME, 32);
            rcBox(.pl = 6, .w = "grow") {
                rcTextInput(field, st->replyDraft, REPLY_TEXT_CAP,
                            .className = pill_field_class(st),
                            .placeholder = "Post your reply");
            }
            if (icon_button(send, rcIconSendSolid, 22.0f, ready ? s.primary : s.textMuted, ready)) {
                publish_reply(st, idx);
                /* Send stops being a stop once the draft is gone, so a key's press would leave
                   nothing focused: it hands the focus back to the field, as a chat app does. A
                   tap has already put the phone's keyboard down, so it does not raise it. */
                st->focusReply = rcClickCount() == 0;
            }
        }
    }
}

static void post_screen(RC_App *app, AppState *st, const NavScreen *sc, int idx) {
    RC_Style    s    = rcGetStyle();
    RC_Arena   *m    = rcAppArena(app);
    const Post *p    = &st->post[idx];
    const int   step = st->textStep;
    const char *tag  = rcFormat(m, "tag_%d", sc->depth).chars;
    const char *mine = rcFormat(m, "myreply_%d", st->myReplyCount - 1).chars;
    int         i;

    /* A reply you just sent lands where you can see it, on its own post. rcScrollIntoView finds the
       new row after this frame's layout, so it needs no frame or check of its own. */
    if (st->revealReply && st->myReply[st->myReplyCount - 1].post == idx) {
        rcScrollIntoView(sc->scrollId, mine, RC_SCROLL_NEAREST);
        st->revealReply = false;
    }
    nav_top_band(app, &st->nav, sc, NULL, NULL, 0);
    rcColumn(.id = sc->scrollId, .bg = s.background, .align = "tC", .scroll = "v", .w = "grow",
             .h = "grow", .scrollOffset = { 0.0f, sc->scrollY }) {
        rcColumn(.gap = 12, .pt = 12, .px = GUTTER, .w = "grow", .wMax = RC_VAL(READ_W)) {
            post_header(app, st, idx, sc->depth);
            rcTextC(p->text, .color = s.text, .size = READ_PX[step], .lineHeight = READ_LH[step]);
            post_photo(st, idx, st->nav.wide ? RC_VH(PANE_PHOTO_VH) : RC_FIT);
            rcRow(.gap = 4, .align = "cl", .w = "grow") {
                rcText(age_text(m, p->minutes), .color = s.textMuted, .size = 13);
                /* A finger's target is TARGET tall; a mouse's keeps the detail's rhythm. */
                rcBox(.id = tag, .focusable = true, .px = 6, .align = "cc",
                      .hType = RC_PX(rcPointerIsCoarse() ? TARGET : 24)) {
                    rcTextC(TOPIC[p->topic].name, .font = rcFontSemibold(13), .color = s.primary,
                            .select = RC_SELECT_NONE);
                }
                if (rcClicked(tag))
                    navigate(st, sc->depth, ROUTE_TOPIC, (int)p->topic);
            }
            hairline();
            rcRow(.gap = 16, .align = "cl", .w = "grow") {
                stat_pair(m, p->likes, "likes");
                stat_pair(m, p->reposts, "reposts");
                stat_pair(m, p->replies, "replies");
            }
            hairline();
            post_actions(app, st, idx, sc->depth, true);
        }
        rcColumn(.w = "grow", .wMax = RC_VAL(READ_W)) {
            rcBox(.px = GUTTER, .w = "grow") {
                hairline();
            }
            /* Newest first: what this reader just wrote lands where they are looking. */
            for (i = st->myReplyCount - 1; i >= 0; i--) {
                const char *id = rcFormat(m, "myreply_%d", i).chars;

                if (st->myReply[i].post == idx)
                    reply_row(st, m, id, P_ME, st->myReply[i].text, 0);
            }
            for (i = 0; i < p->seedReplies; i++) {
                const int line = (p->listIndex * 5 + i * 7) % N(REPLY_LINE);

                reply_row(st, m, NULL, reply_author(p, i), REPLY_LINE[line], p->minutes / (i + 2));
            }
            rcBox(.h = "16") {}
        }
    }
    reply_bar(app, st, sc, idx);
}

/* ------------------------------------------------------------------ people and topics */

/* Every post the filter lets through, newest first. A profile and a topic are this one list. */
static int posts_where(RC_App *app, AppState *st, const NavScreen *sc, int author, int topic,
                       bool saved) {
    int i, shown = 0;

    for (i = 0; i < st->postCount; i++) {
        const int   idx = st->byTime[i];
        const Post *p   = &st->post[idx];

        if (!post_shows(st, idx) || (author >= 0 && (int)p->author != author) ||
            (topic >= 0 && (int)p->topic != topic) || (saved && !p->saved))
            continue;
        post_card(app, st, idx, sc->depth);
        shown++;
    }
    return shown;
}

/* A gradient band with the face riding its bottom edge, then who this is. The action row beside
   the face is the caller's, because it is what differs between someone else and you. */
static void profile_banner(const AppState *st, PersonId who) {
    RC_Style s = rcGetStyle();

    rcBox(.w = "grow", .hType = RC_PX(92),
          .gradient = { .from = PERSON[who].from, .to = PERSON[who].to, .dir = "h" }) {
        rcBox(.bg = s.background, .p = 3, .borderRadius = "all-full",
              .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_BOTTOM_LEFT,
                            .element = RC_ANCHOR_TOP_LEFT, .offset = { (float)GUTTER, -40.0f },
                            .capture = RC_CAPTURE_PASSTHROUGH }) {
            avatar(st, who, 76);
        }
    }
}

static void profile_facts(RC_Arena *m, const AppState *st, PersonId who, const char *bio) {
    RC_Style s = rcGetStyle();

    rcColumn(.gap = 8, .pb = 12, .px = GUTTER, .w = "grow") {
        rcColumn(.gap = 1, .w = "grow") {
            rcTextC(person_name(st, who), .font = rcFontSemibold(22), .color = s.text,
                    .lineHeight = 28);
            rcText(rcFormat(m, "@%s" DOT "%s", PERSON[who].handle, PERSON[who].place),
                   .color = s.textMuted, .size = 15);
        }
        rcTextC(bio, .color = s.text, .size = 15, .lineHeight = 21);
        rcRow(.gap = 16, .align = "cl", .w = "grow") {
            stat_pair(m, who == P_ME ? following_count(st) : PERSON[who].following, "following");
            stat_pair(m, followers_of(st, who), "followers");
        }
    }
}

static void person_screen(RC_App *app, AppState *st, const NavScreen *sc, PersonId who) {
    RC_Style        s   = rcGetStyle();
    RC_Arena       *m   = rcAppArena(app);
    const NavButton btn = { rcFormat(m, "pmenu_%d", sc->depth).chars, rcIconEllipsis, "More",
                            false };

    if (nav_top_band(app, &st->nav, sc, NULL, &btn, who == P_ME ? 0 : 1) == 0) {
        st->sheetPost = -1;
        st->sheetPerson = who;
        open_sheet(st, SHEET_MENU);
    }
    rcColumn(.id = sc->scrollId, .bg = s.background, .align = "tC", .scroll = "v", .w = "grow",
             .h = "grow", .scrollOffset = { 0.0f, sc->scrollY }) {
        rcColumn(.w = "grow", .wMax = RC_VAL(READ_W)) {
            profile_banner(st, who);
            rcRow(.pr = GUTTER, .align = "cr", .w = "grow", .hType = RC_PX(52)) {
                if (who != P_ME)
                    follow_button(m, st, sc->depth, 200, who);
            }
            profile_facts(m, st, who, PERSON[who].bio);
            feed_gap(st);
            if (st->muted[who]) {
                empty_state(rcIconVolumeX, "You muted this person",
                            "Their posts stay out of your feed. Open the menu above to unmute.");
            } else if (!posts_where(app, st, sc, (int)who, -1, false)) {
                empty_state(rcIconInbox, "No posts yet",
                            "When there are, they will show up here.");
            }
        }
    }
}

static void topic_screen(RC_App *app, AppState *st, const NavScreen *sc, TopicId topic) {
    RC_Style    s      = rcGetStyle();
    RC_Arena   *m      = rcAppArena(app);
    const Topic *tp    = &TOPIC[topic];
    const char *follow = rcFormat(m, "tfollow_%d_%d", sc->depth, (int)topic).chars;

    nav_top_band(app, &st->nav, sc, NULL, NULL, 0);
    rcColumn(.id = sc->scrollId, .bg = s.background, .align = "tC", .scroll = "v", .w = "grow",
             .h = "grow", .scrollOffset = { 0.0f, sc->scrollY }) {
        rcColumn(.w = "grow", .wMax = RC_VAL(READ_W)) {
            rcBox(.align = "bl", .w = "grow", .aspectRatio = 2.4f, .image = &st->photo[tp->photo],
                  .imageFit = RC_IMAGE_COVER) {
                rcBox(.p = GUTTER, .align = "bl", .w = "grow", .h = "grow",
                      .gradient = { .from = rcWithAlpha(RC_BLACK, 0),
                                    .to = rcWithAlpha(RC_BLACK, 0.667f), .dir = "v" }) {
                    rcTextC(tp->name, .font = rcFontSemibold(34), .color = RC_WHITE,
                            .select = RC_SELECT_NONE);
                }
            }
            rcRow(.gap = 12, .px = GUTTER, .py = 8, .align = "cl", .w = "grow") {
                rcBox(.w = "grow") {
                    rcTextC(tp->about, .color = s.textMuted, .size = 15, .lineHeight = 21);
                }
                if (pill_button(follow, st->followsTopic[topic] ? "Following" : "Follow topic",
                                !st->followsTopic[topic]))
                    st->followsTopic[topic] = !st->followsTopic[topic];
            }
            feed_gap(st);
            if (!posts_where(app, st, sc, -1, (int)topic, false))
                empty_state(rcIconInbox, "Quiet here for now",
                            "Everything in this topic is muted or hidden.");
        }
    }
}

/* ------------------------------------------------------------------ Explore */

static void person_row(RC_App *app, AppState *st, int scope, PersonId who) {
    RC_Style    s  = rcGetStyle();
    RC_Arena   *m  = rcAppArena(app);
    const char *id = rcFormat(m, "person_%d_%d", scope, (int)who).chars;

    rcRow(.id = id, .focusable = true, .gap = 10, .pl = GUTTER, .pr = GUTTER, .py = 8,
          .align = "cl", .overflow = "hidden", .w = "grow",
          .className = row_class(st, scope, ROUTE_PERSON, (int)who)) {
        avatar(st, who, 44);
        rcColumn(.gap = 1, .overflow = "hidden", .w = "grow") {
            rcTextC(PERSON[who].name, .font = rcFontSemibold(15), .color = s.text, .wrap = "e",
                    .select = RC_SELECT_NONE);
            rcTextC(PERSON[who].bio, .color = s.textMuted, .size = 13, .wrap = "e",
                    .select = RC_SELECT_NONE);
        }
        follow_button(m, st, scope, 300, who);
    }
    if (rcClicked(id))
        navigate(st, scope, ROUTE_PERSON, (int)who);
}

/* What is being talked about, numbered: on Explore, and in the wide arm's empty pane. A tile opens
   topics as well, so a row lights only when it is the one tapped. */
static void trending_list(RC_App *app, AppState *st, int scope) {
    RC_Style  s = rcGetStyle();
    RC_Arena *m = rcAppArena(app);
    int       i;

    for (i = 0; i < N(TREND); i++) {
        const char     *id    = rcFormat(m, "trend_%d_%d", scope, i).chars;
        const RC_String n     = count_text(m, TREND[i].posts);
        const char     *what  = TOPIC[TREND[i].topic].name;
        const RC_String meta  = rcFormat(m, "%s" DOT "%s posts", what, n.chars);
        const bool      open  = st->openTrend == i &&
                                row_is_open(st, scope, ROUTE_TOPIC, (int)TREND[i].topic);

        rcRow(.id = id, .focusable = true, .gap = 12, .pl = GUTTER, .pr = 8, .align = "cl",
              .w = "grow", .hType = RC_PX(60),
              .className = open ? "bg-primary/10 ring-inset" : "nav-press ring-inset") {
            rcBox(.align = "cc", .wType = RC_PX(20)) {
                rcText(rcFormat(m, "%d", i + 1), .font = rcFontSemibold(17), .color = s.primary,
                       .select = RC_SELECT_NONE);
            }
            rcColumn(.gap = 1, .w = "grow") {
                rcTextC(TREND[i].label, .font = rcFontSemibold(15), .color = s.text,
                        .select = RC_SELECT_NONE);
                rcText(meta, .color = s.textMuted, .size = 13, .select = RC_SELECT_NONE);
            }
            rcIconChevronRight(18.0f, s.textMuted);
        }
        if (rcClicked(id)) {
            st->openTrend = i;
            navigate(st, scope, ROUTE_TOPIC, (int)TREND[i].topic);
        }
    }
}

/* Up to five people this reader did not follow at launch, leaving out the author of the feed's
   top post, whose card already offers a Follow of its own. */
static void people_to_follow(RC_App *app, AppState *st, int scope) {
    const int top = st->feedCount > 0 ? (int)st->post[st->feed[0]].author : -1;
    int       i, shown = 0;

    for (i = 1; i < PERSON_COUNT && shown < 5; i++) {
        if (!PERSON[i].followed && i != top) {
            person_row(app, st, scope, (PersonId)i);
            shown++;
        }
    }
}

static void topic_tile(RC_Arena *m, AppState *st, int scope, int topic) {
    const char    *id    = rcFormat(m, "topic_%d_%d", scope, topic).chars;
    const RC_Color clear = rcWithAlpha(RC_BLACK, 0), shade = rcWithAlpha(RC_BLACK, 0.59f);

    rcBox(.id = id, .focusable = true, .align = "bl", .overflow = "hidden",
          .borderRadius = "all-12", .w = "grow", .aspectRatio = 1.6f,
          .image = &st->photo[TOPIC[topic].photo], .imageFit = RC_IMAGE_COVER) {
        rcBox(.p = 10, .align = "bl", .borderRadius = "all-12", .w = "grow", .h = "grow",
              .gradient = { .from = clear, .to = shade, .dir = "v" }) {
            rcTextC(TOPIC[topic].name, .font = rcFontSemibold(15), .color = RC_WHITE,
                    .select = RC_SELECT_NONE);
        }
    }
    if (rcClicked(id)) {
        st->openTrend = -1;
        navigate(st, scope, ROUTE_TOPIC, topic);
    }
}

/* Topics as a photo grid, two to a row at any width: two equal cells that each take half. */
static void topic_grid(RC_App *app, AppState *st, int scope) {
    RC_Arena *m = rcAppArena(app);
    int       r;

    rcColumn(.gap = 8, .px = GUTTER, .w = "grow") {
        for (r = 0; r * 2 < TOPIC_COUNT; r++) {
            rcRow(.gap = 8, .w = "grow") {
                topic_tile(m, st, scope, r * 2);
                topic_tile(m, st, scope, r * 2 + 1);
            }
        }
    }
}

static void explore_root(RC_App *app, AppState *st, const NavScreen *sc) {
    RC_Style s = rcGetStyle();
    int      i;

    nav_top_band(app, &st->nav, sc, NULL, NULL, 0);
    rcColumn(.id = sc->scrollId, .bg = s.background, .align = "tC", .scroll = "v", .w = "grow",
             .h = "grow", .scrollOffset = { 0.0f, sc->scrollY }) {
        rcColumn(.w = "grow", .wMax = RC_VAL(READ_W)) {
            nav_large_title(&st->nav, "Explore");
            rcBox(.pb = 8, .px = GUTTER, .w = "grow") {
                rcBox(.w = "grow") {
                    if (rcTextInput("explore_q", st->query, QUERY_CAP,
                                    .className = pill_field_class(st),
                                    .placeholder = "Search people and posts",
                                    .leadingIcon = rcIconSearch, .clearButton = true))
                        rebuild_search(st);
                }
            }
            if (st->query[0]) {
                if (st->personHitCount) {
                    section_label("People");
                    for (i = 0; i < st->personHitCount; i++)
                        person_row(app, st, sc->depth, (PersonId)st->personHit[i]);
                }
                if (st->postHitCount) {
                    section_label("Posts");
                    rcBox(.h = "8") {}
                    feed_gap(st);
                    for (i = 0; i < st->postHitCount; i++)
                        post_card(app, st, st->postHit[i], sc->depth);
                }
                if (!st->personHitCount && !st->postHitCount)
                    empty_state(rcIconSearch, "No matches",
                                "Try a person's name, a handle or a word from a post.");
            } else {
                section_label("Trending now");
                trending_list(app, st, sc->depth);
                section_label("Topics");
                topic_grid(app, st, sc->depth);
                section_label("Who to follow");
                people_to_follow(app, st, sc->depth);
                rcBox(.h = "16") {}
            }
        }
    }
}

/* THE WIDE ARM'S PANE WITH NOTHING OPEN IN IT stays worth looking at, as a big social site's right
   column does. These are Explore's own parts, so a tap here opens in this pane as anywhere else;
   beside Explore itself, whose list already holds them, it shows the feed instead. */
static void discover_pane(RC_App *app, AppState *st, const NavScreen *sc) {
    RC_Style s = rcGetStyle();
    int      i;

    rcColumn(.id = sc->scrollId, .bg = s.background, .align = "tC", .scroll = "v", .w = "grow",
             .h = "grow") {
        rcColumn(.w = "grow", .wMax = RC_VAL(READ_W)) {
            rcBox(.h = "12") {}
            if (st->nav.tab == TAB_SEARCH) {
                section_label("From your feed");
                rcBox(.h = "4") {}
                for (i = 0; i < st->feedCount && i < 6; i++)
                    post_card(app, st, st->feed[i], SCOPE_PANE);
            } else {
                section_label("Trending now");
                trending_list(app, st, SCOPE_PANE);
                section_label("Who to follow");
                people_to_follow(app, st, SCOPE_PANE);
                section_label("Topics");
                topic_grid(app, st, SCOPE_PANE);
                rcBox(.h = "16") {}
            }
        }
    }
}

/* ------------------------------------------------------------------ Activity */

static const char *const ACT_FILTER_LABEL[ACT_FILTERS] = { "All", "Replies", "Likes", "Follows" };

enum { ACTIVITY_ROW_H = 72 };

/* What the row's corner badge shows and in which color: one glance says what happened. The icon
   takes the page's ground, which reads on these fills in both themes. */
static void kind_badge(ActivityKind kind) {
    RC_Style        s    = rcGetStyle();
    RC_IconCallback icon = rcIconMessageCircle;
    RC_Color        fill = s.primary;

    if (kind == ACT_LIKE) {
        icon = rcIconHeartFilled;
        fill = PALETTE.heart;
    } else if (kind == ACT_REPOST) {
        icon = rcIconRepeat;
        fill = PALETTE.repost;
    } else if (kind == ACT_FOLLOW) {
        icon = rcIconUserPlus;
    }
    rcBox(.bg = fill, .align = "cc", .borderRadius = "all-full",
          .border = { .color = s.background, .width = 2 }, .wType = RC_PX(22),
          .hType = RC_PX(22),
          .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_BOTTOM_RIGHT,
                        .element = RC_ANCHOR_BOTTOM_RIGHT, .offset = { 4.0f, 4.0f },
                        .capture = RC_CAPTURE_PASSTHROUGH }) {
        icon(12.0f, s.background);
    }
}

/* rcVirtualList declares the viewport rather than the dataset, so 120 rows cost what ten do. Its
   one demand is an exact pitch, which is why every row here is ACTIVITY_ROW_H, read or unread.
   A feed of pictures cannot make that promise, which is why Home declares its posts in a loop. */
static void activity_root(RC_App *app, AppState *st, const NavScreen *sc) {
    RC_Style        s      = rcGetStyle();
    RC_Arena       *m      = rcAppArena(app);
    const int       unread = unread_count(st);
    const NavButton btn    = { "activity_all", NULL, "Mark all read", unread == 0 };
    NavScreen       head   = *sc;
    int             i;

    /* The title and chips stay put over a list whose rows the virtual list measures, so the band
       has nothing to collapse into it. */
    head.scrollY = 0.0f;
    if (nav_top_band(app, &st->nav, &head, NULL, &btn, 1) == 0)
        mark_all_read(st);
    rcColumn(.bg = s.background, .w = "grow") {
        nav_large_title(&st->nav, "Activity");
        rcRow(.gap = 8, .px = GUTTER, .align = "cl", .w = "grow") {
            for (i = 0; i < ACT_FILTERS; i++) {
                if (chip(rcFormat(m, "actf_%d", i).chars, ACT_FILTER_LABEL[i],
                         (int)st->activityFilter == i)) {
                    st->activityFilter = (ActivityFilter)i;
                    rebuild_activity(st);
                    rcScrollToTop(sc->scrollId);
                }
            }
        }
    }
    hairline();
    rcColumn(.id = sc->scrollId, .bg = s.background, .scroll = "v", .w = "grow", .h = "grow",
             .scrollOffset = { 0.0f, sc->scrollY }) {
        if (st->shownActivityCount == 0)
            empty_state(rcIconBell, "Nothing here yet",
                        "Reactions to your posts show up here. Check what Settings lets through.");
        rcVirtualList(row, sc->scrollId, st->shownActivityCount, (float)ACTIVITY_ROW_H) {
            const int      ai   = st->shownActivity[row.index];
            Activity      *a    = &st->activity[ai];
            const char    *id   = rcFormat(m, "act_%d", ai).chars;
            const bool     open = st->nav.wide && st->nav.stack[sc->tab].depth > 0 &&
                                  st->openActivity == ai;
            const RC_Color tint = open ? rcWithAlpha(s.primary, 0.1f) :
                                  a->unread ? rcWithAlpha(s.primary, 0.063f) : RC_UNSET_COLOR;

            rcRow(.id = id, .focusable = true, .bg = tint, .gap = 12, .pl = GUTTER, .pr = 12,
                  .align = "cl", .overflow = "hidden", .w = "grow",
                  .hType = RC_PX(ACTIVITY_ROW_H), .overlay = press_wash(id),
                  .className = "ring-inset") {
                rcBox(.wType = RC_PX(44), .hType = RC_PX(44)) {
                    avatar(st, a->person, 44);
                    kind_badge(a->kind);
                }
                rcColumn(.gap = 2, .overflow = "hidden", .w = "grow") {
                    rcRow(.gap = 4, .align = "cl", .overflow = "hidden", .w = "grow") {
                        rcTextC(PERSON[a->person].name, .font = rcFontSemibold(15), .color = s.text,
                                .wrap = "n", .select = RC_SELECT_NONE);
                        rcTextC(ACT_TEXT[a->kind], .color = s.text, .size = 15, .wrap = "e",
                                .select = RC_SELECT_NONE);
                    }
                    rcTextC(a->post >= 0 ? st->post[a->post].text : PERSON[a->person].bio,
                            .color = s.textMuted, .size = 13, .wrap = "e",
                            .select = RC_SELECT_NONE);
                }
                rcColumn(.gap = 6, .align = "cr", .wType = RC_PX(30)) {
                    rcText(age_text(m, a->minutes), .color = s.textMuted, .size = 13, .wrap = "n",
                           .select = RC_SELECT_NONE);
                    rcBox(.bg = a->unread ? s.primary : RC_UNSET_COLOR, .borderRadius = "all-full",
                          .wType = RC_PX(8), .hType = RC_PX(8)) {}
                }
            }
            if (rcClicked(id)) {
                a->unread = false;
                st->openActivity = ai;
                if (a->post >= 0)
                    navigate(st, sc->depth, ROUTE_POST, a->post);
                else
                    navigate(st, sc->depth, ROUTE_PERSON, (int)a->person);
            }
        }
    }
}

/* ------------------------------------------------------------------ Profile */

static const char *const PROFILE_LABEL[] = { "Posts", "Saved" };

static void profile_root(RC_App *app, AppState *st, const NavScreen *sc) {
    RC_Style        s   = rcGetStyle();
    RC_Arena       *m   = rcAppArena(app);
    const NavButton btn = { "open_settings", rcIconSettings, "Settings", false };

    if (nav_top_band(app, &st->nav, sc, NULL, &btn, 1) == 0)
        navigate(st, sc->depth, ROUTE_SETTINGS, 0);
    rcColumn(.id = sc->scrollId, .bg = s.background, .align = "tC", .scroll = "v", .w = "grow",
             .h = "grow", .scrollOffset = { 0.0f, sc->scrollY }) {
        rcColumn(.w = "grow", .wMax = RC_VAL(READ_W)) {
            profile_banner(st, P_ME);
            rcRow(.gap = 8, .pr = GUTTER, .align = "cr", .w = "grow", .hType = RC_PX(52)) {
                if (pill_button("edit_profile", "Edit profile", false))
                    navigate(st, sc->depth, ROUTE_EDIT_PROFILE, 0);
                if (pill_button("share_profile", "Share", false)) {
                    rcClipboardSet("https://perch.example/@alexrivera");
                    toast(st, "Profile link copied", UNDO_NONE, 0);
                }
            }
            profile_facts(m, st, P_ME, st->myBio[0] ? st->myBio : "No bio yet.");
            underline_tabs(m, "mine", PROFILE_LABEL, N(PROFILE_LABEL), &st->profileMode, true);
            if (st->profileMode == 0) {
                if (!posts_where(app, st, sc, (int)P_ME, -1, false))
                    empty_state(rcIconInbox, "Nothing posted yet",
                                "Tap the plus to write your first post.");
            } else if (!posts_where(app, st, sc, -1, -1, true)) {
                empty_state(rcIconBookmark, "Nothing saved yet",
                            "Tap the bookmark on any post and it will wait for you here.");
            }
        }
    }
}

/* ------------------------------------------------------------------ Settings */

/* A grouped screen is the one place the tones swap: gray ground with white cells in the light
   theme, black ground with gray cells in the dark one, as a phone's own settings are. A cell is
   RC_Style.surface in both, and what sits in a cell has surfaceAlt, a step off it, to show on. */
static RC_Color group_ground(const AppState *st) {
    return st->dark ? rcGetStyle().background : rcGetStyle().surfaceAlt;
}

static void group_label(const char *text) {
    rcBox(.pt = 20, .pb = 6, .px = GUTTER, .w = "grow") {
        rcTextC(text, .font = rcFontSemibold(13), .color = rcGetStyle().textMuted,
                .select = RC_SELECT_NONE);
    }
}

/* One switch row: its label, and a line saying what it changes, because a setting should. The whole
   row takes the tap, since rcToggle's own pill is smaller than a finger. To the keys the switch is
   the row's one stop, as a page's switch inside its label is, so the row is not .focusable. The
   on track is a blue of its own: the accent is white in dark, and so is the switch's knob. */
static bool toggle_row(RC_Arena *m, const char *id, const char *label, const char *hint,
                       bool *value) {
    RC_Style    s       = rcGetStyle();
    const char *row     = rcFormat(m, "%s_row", id).chars;
    bool        changed = false;

    rcRow(.id = row, .gap = 12, .px = GUTTER, .py = 10, .align = "cl", .w = "grow",
          .hMin = RC_VAL(TARGET), .className = "nav-press") {
        rcColumn(.gap = 2, .w = "grow") {
            rcTextC(label, .color = s.text, .size = 15, .select = RC_SELECT_NONE);
            rcTextC(hint, .color = s.textMuted, .size = 13, .lineHeight = 18,
                    .select = RC_SELECT_NONE);
        }
        changed = rcToggle(id, "", value, .className = "accent-[#0095f6]");
    }
    if (rcClicked(row)) {
        *value = !*value;
        changed = true;
    }
    return changed;
}

static const char *const THEME_LABEL[THEME_MODES] = { "System", "Light", "Dark" };
static const char *const TEXT_LABEL[TEXT_STEPS] = { "Default", "Large", "Larger", "Largest" };

static void settings_screen(RC_App *app, AppState *st, const NavScreen *sc) {
    RC_Style   s = rcGetStyle();
    RC_Arena  *m = rcAppArena(app);
    int        theme = (int)st->theme;
    bool       notify = false;

    nav_top_band(app, &st->nav, sc, NULL, NULL, 0);
    rcColumn(.id = sc->scrollId, .bg = group_ground(st), .align = "tC", .scroll = "v", .w = "grow",
             .h = "grow", .scrollOffset = { 0.0f, sc->scrollY }) {
        rcColumn(.w = "grow", .wMax = RC_VAL(READ_W)) {
            group_label("APPEARANCE");
            rcColumn(.bg = s.surface, .w = "grow") {
                rcBox(.px = GUTTER, .py = 10, .w = "grow") {
                    if (segmented(m, "theme", THEME_LABEL, THEME_MODES, &theme))
                        st->theme = (ThemeMode)theme;
                }
                hairline();
                rcColumn(.gap = 8, .px = GUTTER, .py = 10, .w = "grow") {
                    rcTextL("Text size", .color = s.text, .size = 15, .select = RC_SELECT_NONE);
                    segmented(m, "text", TEXT_LABEL, TEXT_STEPS, &st->textStep);
                }
                hairline();
                toggle_row(m, "set_motion", "Reduce motion",
                           "Screens and sheets appear without sliding.", &st->reduceMotion);
            }
            group_label("NOTIFICATIONS");
            rcColumn(.bg = s.surface, .w = "grow") {
                notify |= toggle_row(m, "set_likes", "Likes and reposts",
                                     "Count toward the Activity badge.", &st->notifyLikes);
                hairline();
                notify |= toggle_row(m, "set_replies", "Replies and mentions",
                                     "Count toward the Activity badge.", &st->notifyReplies);
                hairline();
                notify |= toggle_row(m, "set_follows", "New followers",
                                     "Count toward the Activity badge.", &st->notifyFollows);
            }
            rcBox(.h = "24") {}
        }
    }
    if (notify)
        rebuild_activity(st);
}

static void edit_profile_screen(RC_App *app, AppState *st, const NavScreen *sc) {
    RC_Style        s   = rcGetStyle();
    RC_Arena       *m   = rcAppArena(app);
    const NavButton btn = { "edit_done", NULL, "Done", false };

    if (nav_top_band(app, &st->nav, sc, NULL, &btn, 1) == 0)
        nav_pop(&st->nav);
    rcColumn(.id = sc->scrollId, .bg = s.background, .align = "tC", .scroll = "v", .w = "grow",
             .h = "grow", .scrollOffset = { 0.0f, sc->scrollY }) {
        rcColumn(.gap = 8, .p = GUTTER, .w = "grow", .wMax = RC_VAL(READ_W)) {
            rcRow(.pb = 8, .align = "cc", .w = "grow") {
                avatar(st, P_ME, 76);
            }
            rcTextL("Name", .font = rcFontSemibold(13), .color = s.textMuted);
            /* Bound straight to the state: what you type is the name everywhere, this frame. */
            rcBox(.w = "grow") {
                rcTextInput("edit_name", st->myName, NAME_CAP, .className = "h-11",
                            .placeholder = "Your name", .clearButton = true);
            }
            rcBox(.h = "8") {}
            rcTextL("Bio", .font = rcFontSemibold(13), .color = s.textMuted);
            rcTextArea("edit_bio", st->myBio, BIO_CAP, .placeholder = "A line about you",
                       .rows = 4);
            rcText(rcFormat(m, "%d of %d", text_length(st->myBio), BIO_CAP - 1),
                   .color = s.textMuted, .size = 13);
        }
    }
}

/* ------------------------------------------------------------------ the sheets */

static bool sheet_row(RC_Arena *m, const char *key, RC_IconCallback icon, const char *label,
                      RC_Color ink) {
    const char *id = rcFormat(m, "sheet_%s", key).chars;

    rcRow(.id = id, .focusable = true, .gap = 14, .px = GUTTER, .align = "cl", .w = "grow",
          .hType = RC_PX(52), .className = "nav-press ring-inset") {
        icon((float)ICON, ink);
        rcTextC(label, .color = ink, .size = 16, .select = RC_SELECT_NONE);
    }
    return rcClicked(id);
}

/* What you can do about a post and the person who wrote it. A person's own page opens the same
   sheet with no post in it. */
static void menu_sheet(RC_App *app, AppState *st) {
    RC_Style       s   = rcGetStyle();
    RC_Arena      *m   = rcAppArena(app);
    const int      idx = st->sheetPost;
    const PersonId who = st->sheetPerson;
    const char    *at  = PERSON[who].handle;

    if (who != P_ME) {
        if (sheet_row(m, "follow", rcIconUserPlus,
                      rcFormat(m, "%s @%s", st->following[who] ? "Unfollow" : "Follow", at).chars,
                      s.text)) {
            toggle_follow(st, who);
            close_sheet(st);
        }
        if (sheet_row(m, "mute", rcIconVolumeX,
                      rcFormat(m, "%s @%s", st->muted[who] ? "Unmute" : "Mute", at).chars,
                      s.text)) {
            st->muted[who] = !st->muted[who];
            content_changed(st);
            if (st->muted[who])
                toast(st, rcFormat(m, "Muted @%s", at).chars, UNDO_MUTE, (int)who);
            close_sheet(st);
        }
    }
    if (idx >= 0 && sheet_row(m, "hide", rcIconBan, "Hide this post", s.text)) {
        hide_post(st, idx);
        close_sheet(st);
    }
    if (sheet_row(m, "copy", rcIconLink, "Copy link", s.text)) {
        if (idx >= 0) {
            copy_post_link(m, st, idx);
        } else {
            rcClipboardSet(rcFormat(m, "https://perch.example/@%s", at).chars);
            toast(st, "Link copied", UNDO_NONE, 0);
        }
        close_sheet(st);
    }
}

static const char *const SORT_LABEL[] = { "Latest", "Most liked" };

static void feed_sheet(RC_App *app, AppState *st) {
    RC_Arena *m    = rcAppArena(app);
    int       sort = st->sortTop;

    rcBox(.pb = 8, .px = GUTTER, .w = "grow") {
        if (segmented(m, "sort", SORT_LABEL, N(SORT_LABEL), &sort)) {
            st->sortTop = sort != 0;
            rebuild_feed(st);
        }
    }
    hairline();
    toggle_row(m, "sheet_compact", "Compact feed", "Posts drop their pictures and sit closer.",
               &st->compactFeed);
}

/* Writing a post. It goes to the top of the feed on Post; Cancel leaves the draft for next time, as
   every phone app does. The field gives up rows to a keyboard, and what is left scrolls. */
static void compose_sheet(RC_App *app, AppState *st) {
    RC_Style  s      = rcGetStyle();
    RC_Arena *m      = rcAppArena(app);
    const int used = text_length(st->draft);
    const int rows = rcViewport().keyboard.bottom > 0.0f ? 3 : 5;
    int       i;

    rcColumn(.gap = 12, .w = "grow") {
        rcRow(.gap = 10, .px = GUTTER, .align = "tl", .w = "grow") {
            avatar(st, P_ME, 40);
            rcColumn(.gap = 4, .w = "grow") {
                rcTextC(person_name(st, P_ME), .font = rcFontSemibold(15), .color = s.text);
                rcTextArea("compose_text", st->draft, POST_TEXT_CAP,
                           .className = "border-0 bg-transparent p-0",
                           .placeholder = "What are you reading?", .rows = (uint16_t)rows);
            }
        }
        rcRow(.px = GUTTER, .w = "grow") {
            rcSpacer() {}
            rcText(rcFormat(m, "%d of %d", used, POST_TEXT_CAP - 1), .color = s.textMuted,
                   .size = 13);
        }
        /* A row of the app's own pictures to attach, because a picker needs something to pick. It
           runs to the sheet's edges, as Home's people strip does, and its padding is the room a
           picture's focus ring needs inside the strip's clip: 4 px out from the picture. Its bar
           takes its own room below while the strip overflows. */
        rcRow(.id = "compose_photos", .gap = 8, .pt = 4, .pb = 4, .px = GUTTER, .scroll = "h",
              .w = "grow") {
            for (i = 0; i < PHOTO_COUNT; i++) {
                const char *id = rcFormat(m, "attach_%d", i).chars;
                const bool  on = (int)st->draftPhoto == i;

                rcBox(.id = id, .focusable = true, .bg = on ? s.primary : RC_UNSET_COLOR, .p = 2,
                      .borderRadius = "all-10") {
                    rcBox(.align = "cc", .borderRadius = "all-8", .wType = RC_PX(64),
                          .hType = RC_PX(64), .image = &st->photo[i], .imageFit = RC_IMAGE_COVER) {
                        if (on)
                            rcIconCheck(24.0f, RC_WHITE);
                    }
                }
                if (rcClicked(id))
                    st->draftPhoto = on ? NO_PHOTO : (PhotoId)i;
            }
        }
        rcScrollbar("compose_photos");
        rcRow(.gap = 6, .px = GUTTER, .w = "grow", .className = "flex-wrap") {
            for (i = 0; i < TOPIC_COUNT; i++) {
                if (chip(rcFormat(m, "dtopic_%d", i).chars, TOPIC[i].name,
                         (int)st->draftTopic == i))
                    st->draftTopic = (TopicId)i;
            }
        }
    }
}

/* After Post: Home, at the top, where the new post is. */
static void show_new_post(RC_App *app, AppState *st) {
    NavStack *home = &st->nav.stack[TAB_HOME];

    st->nav.tab = TAB_HOME;
    home->depth = 0;
    /* At rest at the root: a slide or a swipe still running there would end a step below it. */
    home->slide = home->slideTo = 1.0f;
    home->slideStart = -1.0;
    home->dragging = false;
    home->route[0].scrollY = 0.0f;
    rcScrollToTop(nav_scroll_id(rcAppArena(app), TAB_HOME, 0, &home->route[0]));
}

/* THE SHEET IS A DIALOG: a bottom sheet on the phone shape, centered when the window is wide. The
   library brings the scrim, the inert screen behind, Esc and Android's Back, and the focus: in on
   open (its first stop, Close, or compose's field) and back to the stop it was opened from
   once it has closed. It stays open while it slides away, so the rise runs both ways by .offset,
   and a close the library asks for (Esc, Back, a press on the scrim) is that slide down.

   IT NEVER OUTGROWS THE WINDOW: the dialog caps it at the room the safe area leaves and scrolls
   what is taller. The phone's sheet rides on a keyboard, as a phone's does, so what you write
   stays in view above it with the rest of the sheet. THE HEAD IS THE GRABBER: a finger on it
   drives the sheet as nav_stage's drives its slide, and a release past 30 percent closes it. The
   head keeps a finger's up-and-down drag (.captureTouch), so a sheet whose body scrolls follows
   it rather than scrolling. */
static void sheet_view(RC_App *app, AppState *st) {
    RC_Style     s    = rcGetStyle();
    RC_Viewport  vp   = rcViewport();
    const bool   wide = st->nav.wide;
    const RC_Box box  = rcGetElementBox("sheet");
    const float  h    = box.height > 0.0f ? box.height : vp.height;
    const float  rise = wide ? 24.0f : h;
    /* The dialog pads the bottom inset itself, and a keyboard covers that inset and more. */
    const float  kb   = !wide && vp.keyboard.bottom > vp.safe.bottom
                        ? vp.keyboard.bottom - vp.safe.bottom : 0.0f;
    const bool   compose = st->sheet == SHEET_COMPOSE;
    const bool   ready   = st->draft[0] != '\0' && st->myPostCount < MY_POST_CAP;
    const char  *title   = compose ? "New post" : st->sheet == SHEET_FEED ? "Feed" : NULL;
    /* Up or rising while it heads for 1; once it heads for 0 it is closed and slides away as the
       dialog's exit: drawn, but no dialog, so focus went back to the stop that opened it on the
       frame it closed and the screen behind is live again. */
    bool         open    = st->sheetTo != 0.0f;

    if (!title)
        title = st->sheetPost >= 0 ? "Post" : person_name(st, st->sheetPerson);
    if (st->sheetDragging) {
        if (rcPointerDown(RC_POINTER_LEFT)) {
            st->sheetT = 1.0f - nav_clamp01((rcPointer().y - st->sheetDragY) / h);
        } else {
            st->sheetDragging = false;
            move_sheet(st, st->sheetT < 0.7f ? 0.0f : 1.0f);
        }
    }

    if (!rcBeginModal("sheet", &open,
                      .className = wide ? "sheet rounded-2xl" : "sheet rounded-t-2xl",
                      .placement = wide ? RC_MODAL_CENTER : RC_MODAL_BOTTOM,
                      .offset = { 0.0f, (1.0f - st->sheetT) * rise }, .exiting = !open))
        return;
    if (st->focusCompose) {
        rcSetFocus("compose_text");
        st->focusCompose = false;
    }
    rcColumn(.id = "sheet_head", .gap = 4, .captureTouch = RC_TOUCH_AXIS_Y, .w = "grow") {
        if (!wide)
            rcRow(.align = "cc", .w = "grow") {
                rcBox(.bg = s.border, .borderRadius = "all-full", .wType = RC_PX(36),
                      .hType = RC_PX(4)) {}
            }
        rcRow(.pl = 6, .pr = 6, .align = "cc", .w = "grow", .hType = RC_PX(TARGET)) {
            rcRow(.align = "cl", .w = "grow") {
                if (rcButton("sheet_cancel", compose ? "Cancel" : "Close",
                             .className = "h-11 text-primary", .variant = RC_BTN_GHOST))
                    close_sheet(st);
            }
            rcTextC(title, .font = rcFontSemibold(17), .color = s.text, .wrap = "n",
                    .select = RC_SELECT_NONE);
            rcRow(.pr = 6, .align = "cr", .w = "grow") {
                if (compose && rcButton("compose_post", "Post", .disabled = !ready,
                                        .className = "h-11", .variant = RC_BTN_PRIMARY)) {
                    publish_draft(st);
                    show_new_post(app, st);
                    close_sheet(st);
                }
            }
        }
    }
    if (!wide && rcPointerPressed(RC_POINTER_LEFT) && rcIsHovered("sheet_head")) {
        st->sheetDragging = true;
        st->sheetDragY = rcPointer().y;
    }
    /* The body's 4 px above and below are the room a row's focus ring needs at either end. */
    rcColumn(.pt = 4, .pb = 4.0f + kb, .w = "grow") {
        switch (st->sheet) {
        case SHEET_COMPOSE: compose_sheet(app, st); break;
        case SHEET_MENU:    menu_sheet(app, st); break;
        case SHEET_FEED:    feed_sheet(app, st); break;
        case SHEET_NONE:    break;
        }
    }
    rcEndModal();
    if (!open)
        close_sheet(st);
}

/* ------------------------------------------------------------------ the toast */

/* A word about what just happened, with Undo where there is one to offer. It stands clear of what
   holds the bottom edge: the tab bar or a keyboard, and a post page's reply bar over either.
   Undo is a ghost button whose word is set as the toast's, 15 px semibold, in the page's ground,
   since the toast's ground is the page's text; its hover and press, toast-press (main.c), take
   nav-press's steps in that ground. Like any button it is a Tab stop that Enter and Space click,
   with the focus ring around it. While Undo has focus or the pointer, the toast stays (update,
   in app.h). */
static void toast_view(AppState *st) {
    RC_Style        s      = rcGetStyle();
    RC_Viewport     vp     = rcViewport();
    const NavStack *n      = &st->nav.stack[st->nav.tab];
    const float     kb     = vp.keyboard.bottom;
    const float     base   = kb > 0.0f ? kb : st->nav.wide ? 16.0f : (float)BAR_H + vp.safe.bottom;
    const bool      onPost = n->route[n->depth].kind == ROUTE_POST;
    const float     bottom = base + (onPost ? (float)REPLY_BAR_H : 0.0f) + 12.0f;

    rcRow(.bg = s.text, .gap = 12, .pl = GUTTER, .pr = (float)(st->toastUndo ? 6 : GUTTER),
          .align = "cl", .borderRadius = "all-full", .hType = RC_PX(TARGET),
          .shadow = { .color = rcWithAlpha(RC_BLACK, 0.275f), .y = 4.0f, .blur = 16.0f },
          .floating = { .to = RC_ATTACH_ROOT, .parent = RC_ANCHOR_BOTTOM_CENTER,
                        .element = RC_ANCHOR_BOTTOM_CENTER, .offset = { 0.0f, -bottom },
                        .zIndex = 8, .clip = RC_CLIP_NONE }) {   /* over a screen's floats */
        rcTextC(st->toastText, .color = s.background, .size = 15, .wrap = "n",
                .select = RC_SELECT_NONE);
        if (st->toastUndo != UNDO_NONE &&
            rcButton("toast_undo", "Undo",
                     .className = "h-11 px-2.5 rounded-full text-[15px] font-semibold "
                                  "text-background toast-press",
                     .variant = RC_BTN_GHOST))
            toast_undo(st);
    }
}

#endif /* APP_SCREENS_H */
