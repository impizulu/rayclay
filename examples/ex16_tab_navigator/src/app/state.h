/*
    state.h - what the app knows, and every rule that changes it

    No element is declared here. The screens read this state and call these functions; a tap
    changes a field, and the next frame's declaration draws the result.
*/
#ifndef APP_STATE_H
#define APP_STATE_H

#include "rayclay.h"
#include "app/theme.h"
#include "app/data.h"
#include "app/navigator.h"

/* The separator every meta line uses: a middle dot, in the bundled face's Latin-1. */
#define DOT " \xC2\xB7 "

typedef enum { TAB_HOME, TAB_SEARCH, TAB_ALERTS, TAB_PROFILE, TAB_COUNT } Tab;

/* What a route can show, the app's half of a NavRoute. Its item is a post, a person or a topic. */
typedef enum {
    ROUTE_ROOT, ROUTE_POST, ROUTE_PERSON, ROUTE_TOPIC, ROUTE_SETTINGS, ROUTE_EDIT_PROFILE
} RouteKind;

typedef enum { SHEET_NONE, SHEET_COMPOSE, SHEET_MENU, SHEET_FEED } SheetKind;

typedef enum { FEED_FOR_YOU, FEED_FOLLOWING, FEED_MODES } FeedMode;
typedef enum { THEME_SYSTEM, THEME_LIGHT, THEME_DARK, THEME_MODES } ThemeMode;
typedef enum { ACT_ALL, ACT_REPLIES, ACT_LIKES, ACT_FOLLOWS, ACT_FILTERS } ActivityFilter;
typedef enum { UNDO_NONE, UNDO_HIDE, UNDO_MUTE } UndoKind;

enum {
    MY_POST_CAP = 8, POST_TEXT_CAP = 281, REPLY_TEXT_CAP = 200, MY_REPLY_CAP = 16,
    POST_CAP = N(SEED) + N(INCOMING) + MY_POST_CAP, ACTIVITY_COUNT = 120, QUERY_CAP = 48,
    NAME_CAP = 40, BIO_CAP = 160, TOAST_CAP = 64
};

typedef struct {
    PersonId    author;
    TopicId     topic;
    PhotoId     photo;
    const char *text;
    int         minutes;      /* how long ago; a smaller number is newer */
    int         likes, reposts, replies;
    int         seedReplies;  /* how many of `replies` come from REPLY_LINE */
    int         listIndex;    /* walks REPLY_LINE, so two posts open on different replies */
    bool        liked, reposted, saved, hidden;
} Post;

typedef struct {
    int  post;
    char text[REPLY_TEXT_CAP];
} MyReply;

typedef struct {
    ActivityKind kind;
    PersonId     person;
    int          post;      /* -1 for a follow */
    int          minutes;
    bool         unread;
} Activity;

/* Everything the app knows, in one struct main.c owns as a static. `byTime`, `feed` and the hit
   lists are permutations: indices into the arrays, so no record is ever copied. */
typedef struct {
    /* The content, as this reader has changed it */
    Post     post[POST_CAP];
    int      postCount;
    char     myPost[MY_POST_CAP][POST_TEXT_CAP];
    int      myPostCount;
    MyReply  myReply[MY_REPLY_CAP];
    int      myReplyCount;
    Activity activity[ACTIVITY_COUNT];
    bool     following[PERSON_COUNT], muted[PERSON_COUNT], followsTopic[TOPIC_COUNT];
    bool     ringSeen[RINGED_COUNT];
    int      incoming;           /* the next post a refresh will find */
    char     myName[NAME_CAP], myBio[BIO_CAP];

    /* What the lists show right now */
    int      byTime[POST_CAP];
    int      feed[POST_CAP];
    int      feedCount;
    FeedMode feedMode;
    int      todayWeekday, todayMonth, todayDay;   /* Home's date line: 0 Sunday, 1-12, 1-31 */
    bool     sortTop;
    int      shownActivity[ACTIVITY_COUNT];
    int      shownActivityCount;
    int      openActivity;       /* the row the wide arm's pane has open, or -1 */
    int      openTrend;          /* the same, for a trending row, since a tile opens topics too */
    ActivityFilter activityFilter;
    char     query[QUERY_CAP];
    int      personHit[PERSON_COUNT];
    int      personHitCount;
    int      postHit[POST_CAP];
    int      postHitCount;
    int      profileMode;        /* 0 my posts, 1 my saved posts */

    /* Settings */
    ThemeMode theme;
    bool     dark;               /* what `theme` came to this frame */
    int      textStep;           /* 0 .. TEXT_STEPS - 1 */
    bool     compactFeed, reduceMotion;
    bool     notifyLikes, notifyReplies, notifyFollows;

    /* Navigation */
    Navigator nav;

    /* Motion and transient UI */
    double   now;                /* rcAppTime, read once a frame */
    double   lastFrame;
    RC_Image photo[PHOTO_COUNT], face[PERSON_COUNT];
    bool     shown, picturesReady;   /* the first frame is drawn; the second loads the pictures */

    bool     pulling, pullWasDrag, refreshing;
    float    pull, pullFromY;
    double   refreshStart;
    int      spinnerSpokes;      /* how many of the eight are lit; a pull lights them in turn */

    int      popPost;            /* the post whose heart just jumped, or -1 */
    double   popStart;

    SheetKind sheet;
    double   sheetStart;
    float    sheetT, sheetFrom, sheetTo;  /* 0 hidden .. 1 fully up, as NavStack's slide */
    bool     sheetDragging;
    float    sheetDragY;         /* where the finger came down */
    int      sheetPost;          /* the post a menu is about, or -1 for only its person */
    PersonId sheetPerson;
    bool     focusCompose;       /* compose just opened: its field takes focus (sheet_view) */

    char     draft[POST_TEXT_CAP];
    PhotoId  draftPhoto;
    TopicId  draftTopic;

    char     replyDraft[REPLY_TEXT_CAP];
    bool     focusReply, revealReply;

    char     toastText[TOAST_CAP];   /* empty when no toast is up */
    UndoKind toastUndo;
    int      toastArg;           /* the post hidden, or the person muted */
    double   toastUntil;
    bool     toastHeld;          /* Undo had focus or the pointer last frame: the clock waits */
} AppState;

/* The four tabs, as tables keyed by Tab so both shells spell an id exactly once. */
static const char *const TAB_TITLE[TAB_COUNT] = { "Home", "Explore", "Activity", "Profile" };
static const char *const TAB_ID[TAB_COUNT]    = { "tab_home", "tab_search", "tab_alerts",
    "tab_profile" };

static int text_length(const char *text) {
    int n = 0;

    while (text[n])
        n++;
    return n;
}

static const char *person_name(const AppState *st, PersonId who) {
    if (who != P_ME)
        return PERSON[who].name;
    return st->myName[0] ? st->myName : "You";
}

/* A follower count that moves when you tap Follow, so the two numbers can never disagree. */
static int followers_of(const AppState *st, PersonId who) {
    return PERSON[who].followers + (int)st->following[who] - (int)PERSON[who].followed;
}

static int following_count(const AppState *st) {
    int i, n = 0;

    for (i = 0; i < PERSON_COUNT; i++)
        n += st->following[i];
    return n;
}

/* rcFormat, never a stack buffer: rcText does not copy, and the frame arena lives exactly as long
   as the frame that draws the string. */
static RC_String count_text(RC_Arena *m, int n) {
    if (n < 1000)
        return rcFormat(m, "%d", n);
    if (n < 10000)
        return rcFormat(m, "%d.%dK", n / 1000, n % 1000 / 100);
    return rcFormat(m, "%dK", n / 1000);
}

static RC_String age_text(RC_Arena *m, int minutes) {
    if (minutes < 1)
        return rcStringFromCStr("now");
    if (minutes < 60)
        return rcFormat(m, "%dm", minutes);
    if (minutes < 1440)
        return rcFormat(m, "%dh", minutes / 60);
    return rcFormat(m, "%dd", minutes / 1440);
}

static bool post_shows(const AppState *st, int i) {
    return !st->post[i].hidden && !st->muted[st->post[i].author];
}

/* Newest first, and of two equally new the later arrival first. An insertion sort: a few dozen
   records, and it runs on a change, never on a frame. */
static void rebuild_order(AppState *st) {
    int i, j;

    for (i = 0; i < st->postCount; i++) {
        for (j = i; j > 0 && st->post[st->byTime[j - 1]].minutes >= st->post[i].minutes; j--)
            st->byTime[j] = st->byTime[j - 1];
        st->byTime[j] = i;
    }
}

static void rebuild_feed(AppState *st) {
    int i, j;

    st->feedCount = 0;
    for (i = 0; i < st->postCount; i++) {
        const int   idx = st->byTime[i];
        const Post *p   = &st->post[idx];

        if (!post_shows(st, idx))
            continue;
        if (st->feedMode == FEED_FOLLOWING && p->author != P_ME && !st->following[p->author])
            continue;
        st->feed[st->feedCount++] = idx;
    }
    if (st->sortTop) {
        for (i = 1; i < st->feedCount; i++) {
            const int idx = st->feed[i];

            for (j = i; j > 0 && st->post[st->feed[j - 1]].likes < st->post[idx].likes; j--)
                st->feed[j] = st->feed[j - 1];
            st->feed[j] = idx;
        }
    }
}

static bool activity_on(const AppState *st, const Activity *a) {
    switch (a->kind) {
    case ACT_LIKE:
    case ACT_REPOST:
        return st->notifyLikes;
    case ACT_REPLY:
    case ACT_MENTION:
        return st->notifyReplies;
    case ACT_FOLLOW:
        return st->notifyFollows;
    case ACT_KINDS:
        break;
    }
    return false;
}

static void rebuild_activity(AppState *st) {
    int i;

    st->shownActivityCount = 0;
    for (i = 0; i < ACTIVITY_COUNT; i++) {
        const Activity *a = &st->activity[i];
        const bool      replies = a->kind == ACT_REPLY || a->kind == ACT_MENTION;
        const bool      likes   = a->kind == ACT_LIKE || a->kind == ACT_REPOST;

        if (!activity_on(st, a))
            continue;
        if ((st->activityFilter == ACT_REPLIES && !replies) ||
            (st->activityFilter == ACT_LIKES && !likes) ||
            (st->activityFilter == ACT_FOLLOWS && a->kind != ACT_FOLLOW))
            continue;
        st->shownActivity[st->shownActivityCount++] = i;
    }
}

/* The badge counts what the notification settings let through, whatever the filter chip says. */
static int unread_count(const AppState *st) {
    int i, n = 0;

    for (i = 0; i < ACTIVITY_COUNT; i++)
        n += st->activity[i].unread && activity_on(st, &st->activity[i]);
    return n;
}

static void mark_all_read(AppState *st) {
    int i;

    for (i = 0; i < ACTIVITY_COUNT; i++)
        st->activity[i].unread = false;
}

/* The Explore search: a scan on a change, never on a frame. */
static char lower(char c) {
    return (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
}

static bool contains(const char *hay, const char *needle) {
    int i, j;

    for (i = 0; hay[i]; i++) {
        for (j = 0; needle[j] && hay[i + j] && lower(hay[i + j]) == lower(needle[j]); j++) {}
        if (!needle[j])
            return true;
    }
    return false;
}

static void rebuild_search(AppState *st) {
    int i;

    st->personHitCount = st->postHitCount = 0;
    if (!st->query[0])
        return;
    for (i = 1; i < PERSON_COUNT; i++)
        if (contains(PERSON[i].name, st->query) || contains(PERSON[i].handle, st->query))
            st->personHit[st->personHitCount++] = i;
    for (i = 0; i < st->postCount; i++) {
        const int idx = st->byTime[i];

        if (post_shows(st, idx) && contains(st->post[idx].text, st->query))
            st->postHit[st->postHitCount++] = idx;
    }
}

/* The one call after anything that changes which posts can show: every list built from them. */
static void content_changed(AppState *st) {
    rebuild_feed(st);
    rebuild_search(st);
}

static Post seed_post(const PostSeed *s, int listIndex) {
    Post p = { .author = s->author, .topic = s->topic, .photo = s->photo, .text = s->text,
               .minutes = s->minutes, .likes = s->likes, .reposts = s->reposts,
               .replies = s->replies, .seedReplies = s->replies, .listIndex = listIndex };

    return p;
}

/* The state a first launch sees. A reader's own posts seed the Activity tab, which is every
   reaction to them, walked so each screenful holds every kind and a different face per row. */
static void app_seed(AppState *st) {
    int mine[N(SEED)], mineCount = 0, i;

    for (i = 0; i < N(SEED); i++) {
        st->post[i] = seed_post(&SEED[i], i);
        if (SEED[i].author == P_ME)
            mine[mineCount++] = i;
    }
    st->postCount = N(SEED);
    st->todayWeekday = 4, st->todayMonth = 11, st->todayDay = 5;
    /* A reader who has been here before: Saved is not empty, and a heart is already lit. */
    st->post[5].saved = st->post[18].saved = true;
    st->post[3].liked = true;
    for (i = 0; i < PERSON_COUNT; i++)
        st->following[i] = PERSON[i].followed;
    st->followsTopic[T_CITIES] = st->followsTopic[T_BOOKS] = true;

    for (i = 0; i < ACTIVITY_COUNT; i++) {
        Activity *a = &st->activity[i];

        a->kind    = (ActivityKind)((i * 3) % ACT_KINDS);
        a->person  = (PersonId)(1 + (i * 5) % (PERSON_COUNT - 1));
        a->post    = a->kind == ACT_FOLLOW ? -1 : mine[i % mineCount];
        a->minutes = 4 + i * 37 + (i * i) % 17;
        a->unread  = i < 7;
    }

    rcStrCopy(st->myName, PERSON[P_ME].name, sizeof st->myName);
    rcStrCopy(st->myBio, PERSON[P_ME].bio, sizeof st->myBio);
    st->notifyLikes = st->notifyReplies = st->notifyFollows = true;
    st->popPost = -1;
    st->openActivity = st->openTrend = -1;
    st->draftPhoto = NO_PHOTO;
    nav_init(&st->nav, TAB_TITLE, TAB_COUNT);
    rebuild_order(st);
    rebuild_feed(st);
    rebuild_activity(st);
}

/* Once, on the second frame (update, in app.h), so the first is on screen while the files decode.
   rcLoadImage reads and decodes here; each texture is made at the picture's first draw. */
static void load_pictures(RC_Arena *m, AppState *st) {
    int i;

    for (i = 0; i < PHOTO_COUNT; i++)
        st->photo[i] = rcLoadImage(PHOTO_FILE[i]);
    for (i = 0; i < PERSON_COUNT; i++)
        st->face[i] = rcLoadImage(rcFormat(m, PICTURE_DIR "faces/%s.png", PERSON[i].handle).chars);
    st->picturesReady = true;
}

/* The `scope` of the wide arm's pane when nothing is open in it: a depth no stack reaches. */
enum { SCOPE_PANE = NAV_DEPTH + 1 };

static const char *route_title(const AppState *st, RouteKind kind, int item) {
    switch (kind) {
    case ROUTE_ROOT:         break;
    case ROUTE_POST:         return "Post";
    case ROUTE_PERSON:       return person_name(st, (PersonId)item);
    case ROUTE_TOPIC:        return TOPIC[item].name;
    case ROUTE_SETTINGS:     return "Settings";
    case ROUTE_EDIT_PROFILE: return "Edit profile";
    }
    return "";
}

/* Every push in the app. A drag that ended on a card is not a tap on it. */
static void navigate(AppState *st, int from, RouteKind kind, int item) {
    if (!st->pullWasDrag)
        nav_push(&st->nav, from == SCOPE_PANE ? 0 : from, (int)kind, item,
                 route_title(st, kind, item));
}

/* A sheet's rise is NavStack's slide over again: from wherever it stands to `to`, by the clock. */
static void move_sheet(AppState *st, float to) {
    st->sheetFrom = st->sheetT;
    st->sheetTo = to;
    st->sheetStart = st->now;
}

/* A sheet is a dialog (sheet_view): the library takes focus to its first stop and gives it back
   once it has closed. Compose sends it to the field instead, as you open it to write. */
static void open_sheet(AppState *st, SheetKind kind) {
    st->sheet = kind;
    st->sheetT = 0.0f;
    move_sheet(st, 1.0f);
    st->focusCompose = kind == SHEET_COMPOSE;
}

static void close_sheet(AppState *st) {
    if (st->sheet != SHEET_NONE && st->sheetTo != 0.0f)
        move_sheet(st, 0.0f);
}

static void step_sheet(AppState *st) {
    float t, e;

    if (st->sheet == SHEET_NONE || st->sheetDragging)
        return;
    t = (float)((st->now - st->sheetStart) / SHEET_SECONDS);
    t = t > 1.0f || st->reduceMotion ? 1.0f : t;
    e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
    st->sheetT = st->sheetFrom + (st->sheetTo - st->sheetFrom) * e;
    if (st->sheetTo == 0.0f && t >= 1.0f)
        st->sheet = SHEET_NONE;
}

static void start_refresh(AppState *st) {
    if (!st->refreshing) {
        st->refreshing = true;
        st->refreshStart = st->now;
    }
}

static void toast(AppState *st, const char *text, UndoKind undo, int arg) {
    rcStrCopy(st->toastText, text, sizeof st->toastText);
    st->toastUndo = undo;
    st->toastArg = arg;
    st->toastUntil = st->now + (undo == UNDO_NONE ? 2.5 : 5.0);
}

static void toast_undo(AppState *st) {
    if (st->toastUndo == UNDO_HIDE)
        st->post[st->toastArg].hidden = false;
    else if (st->toastUndo == UNDO_MUTE)
        st->muted[st->toastArg] = false;
    content_changed(st);
    st->toastText[0] = '\0';
}

/* Hidden from where you are reading it too: its own page pops, and Undo brings it back to the
   lists. */
static void hide_post(AppState *st, int idx) {
    const NavStack *n = &st->nav.stack[st->nav.tab];

    st->post[idx].hidden = true;
    content_changed(st);
    toast(st, "Post hidden", UNDO_HIDE, idx);
    if (n->route[n->depth].kind == ROUTE_POST && n->route[n->depth].item == idx)
        nav_pop(&st->nav);
}

static void toggle_like(AppState *st, int idx) {
    Post *p = &st->post[idx];

    p->liked = !p->liked;
    p->likes += p->liked ? 1 : -1;
    if (p->liked) {
        st->popPost = idx;
        st->popStart = st->now;
    }
}

static void toggle_repost(AppState *st, int idx) {
    Post *p = &st->post[idx];

    p->reposted = !p->reposted;
    p->reposts += p->reposted ? 1 : -1;
}

static void toggle_save(AppState *st, int idx) {
    Post *p = &st->post[idx];

    p->saved = !p->saved;
    toast(st, p->saved ? "Saved to your list" : "Removed from your list", UNDO_NONE, 0);
}

static void toggle_follow(AppState *st, PersonId who) {
    st->following[who] = !st->following[who];
    content_changed(st);
}

/* The heart's jump, as a scale on the icon's size: up quickly, back down slowly. Icons are vector,
   so this costs nothing. Text must not do this: each size of a font bakes it once. */
static float heart_scale(const AppState *st, int idx) {
    float t;

    if (st->popPost != idx)
        return 1.0f;
    t = (float)((st->now - st->popStart) / POP_SECONDS);
    if (t >= 1.0f)
        return 1.0f;
    return 1.0f + 0.45f * (t < 0.3f ? t / 0.3f : 1.0f - (t - 0.3f) / 0.7f);
}

/* Finds what a refresh brings in: the next few of INCOMING, or nothing, and says which. */
static void finish_refresh(AppState *st) {
    int added = 0;

    st->refreshing = false;
    st->pull = (float)PULL_REST;   /* the band springs shut from where it stood */
    while (added < REFRESH_BATCH && st->incoming < N(INCOMING)) {
        st->post[st->postCount] = seed_post(&INCOMING[st->incoming], st->postCount);
        st->postCount++;
        st->incoming++;
        added++;
    }
    if (added) {
        rebuild_order(st);
        content_changed(st);
        toast(st, "3 new posts", UNDO_NONE, 0);
    } else {
        toast(st, "You are all caught up", UNDO_NONE, 0);
    }
}

static void publish_draft(AppState *st) {
    Post p = { .author = P_ME, .topic = st->draftTopic, .photo = st->draftPhoto,
               .minutes = 0, .listIndex = st->postCount };

    rcStrCopy(st->myPost[st->myPostCount], st->draft, POST_TEXT_CAP);
    p.text = st->myPost[st->myPostCount++];
    st->post[st->postCount++] = p;
    st->draft[0] = '\0';
    st->draftPhoto = NO_PHOTO;
    st->feedMode = FEED_FOR_YOU;
    rebuild_order(st);
    content_changed(st);
    toast(st, "Your post is live", UNDO_NONE, 0);
}

static void publish_reply(AppState *st, int idx) {
    MyReply *r;

    if (!st->replyDraft[0] || st->myReplyCount == MY_REPLY_CAP)
        return;
    r = &st->myReply[st->myReplyCount++];
    r->post = idx;
    rcStrCopy(r->text, st->replyDraft, sizeof r->text);
    st->replyDraft[0] = '\0';
    st->post[idx].replies++;
    st->revealReply = true;
    toast(st, "Reply posted", UNDO_NONE, 0);
}

#endif /* APP_STATE_H */
