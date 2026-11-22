/*
    messenger_backend.h - the messenger's model: conversations, messages, reactions,
    delivery ticks, the scripted people who keep it alive, and search.

    Pure C, no RayClay dependency, no clock, no rand(), no I/O: it takes an injected
    `dt` and is reproducible frame for frame, so a scripted run and a window run the same
    code. "Now" is injected too: a window's runner hands in the wall clock (msg_store_set_now),
    and a run that hands in none stays at noon on a Thursday. Fixed capacities, so the store
    is a flat value the seed can memset. The one thing here that is not the model is main.c's
    worker thread, behind its own macro below.

    Usage (define the implementation in exactly one TU):
        #define MESSENGER_BACKEND_IMPLEMENTATION
        #include "messenger_backend.h"
*/
#ifndef MESSENGER_BACKEND_H
#define MESSENGER_BACKEND_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MSG_MAX_CONVERSATIONS  20
#define MSG_MAX_MSGS_PER_CONV  96
#define MSG_TEXT_POOL_BYTES    (64 * 1024)
#define MSG_NAME_CAP           32
#define MSG_REACTION_COUNT      6
#define MSG_MAX_OUTBOX          8
#define MSG_EPOCH_SECONDS   43200u   /* the demo's "now": 12:00:00, on a Thursday */
#define MSG_ENTER_SECONDS   0.22f    /* how long a new line takes to open up */

typedef enum { MSG_KIND_TEXT = 0, MSG_KIND_PHOTO, MSG_KIND_STICKER, MSG_KIND_EVENT } MsgKind;
typedef enum { MSG_DIR_INCOMING = 0, MSG_DIR_OUTGOING } MsgDir;

/* The ticks of an outgoing message, in the order a send walks them. READ is 0, so a
   memset store and an incoming message both read as settled. QUEUED is a line a live
   room has yet to send (live_room.h takes it from there), FAILED a line that did not go. */
typedef enum {
    MSG_DELIV_READ = 0,
    MSG_DELIV_DELIVERED,
    MSG_DELIV_SENT,
    MSG_DELIV_SENDING,
    MSG_DELIV_QUEUED,
    MSG_DELIV_FAILED
} MsgDelivery;

typedef enum {
    MSG_PRESENCE_ONLINE = 0,
    MSG_PRESENCE_AWAY,
    MSG_PRESENCE_OFFLINE
} MsgPresence;

typedef enum { MSG_CONV_DIRECT = 0, MSG_CONV_GROUP, MSG_CONV_CHANNEL } MsgConvKind;

/* One message. `text` points into the immutable corpus or the store's text pool, and
   `who` names the sender wherever a thread has more than one other person. */
typedef struct {
    const char *text;
    const char *who;
    uint32_t    id;          /* stable for the run: replies and element ids use it   */
    uint32_t    replyTo;     /* the id of the line this one quotes; 0 = none         */
    int32_t     wire;        /* a live room's id for the line; 0 here                */
    int32_t     nonce;       /* a live room: the number your line's echo carries     */
    uint32_t    clock;       /* seconds since midnight                               */
    float       age;         /* seconds since it arrived: drives the entrance        */
    uint16_t    textLen;
    uint8_t     kind;        /* MsgKind                                              */
    uint8_t     dir;         /* MsgDir                                               */
    uint8_t     deliv;       /* MsgDelivery                                          */
    uint8_t     art;         /* a photo's or a sticker's index                       */
    uint8_t     day;         /* days before today                                    */
    uint8_t     reactions[MSG_REACTION_COUNT];  /* how many of each                  */
    uint8_t     reacted;     /* a bit per reaction: yours                            */
    bool        pooled;      /* its strings live in the text pool                    */
    bool        starred;
    bool        deleted;     /* you deleted it: the thread shows the tombstone       */
    char        ts[6];       /* "HH:MM"                                              */
} MsgMessage;

typedef struct {
    char        name[MSG_NAME_CAP];
    char        initials[3];
    uint8_t     kind;        /* MsgConvKind                                          */
    uint8_t     presence;    /* MsgPresence                                          */
    uint8_t     person;      /* who a direct chat is with, who answers in a group    */
    uint32_t    tintA, tintB;/* the avatar's gradient                                */
    const char *about;       /* a person's status, a group's or channel's topic      */
    const char *detail;      /* a phone number, or who is in the group               */
    bool        pinned, muted, archived, hidden, markedUnread;
    uint16_t    unread;
    char        badge[8];
    uint32_t    lastStamp;   /* sorts the list: newest is largest                    */
    char        lastTime[12];/* "11:58", "Yesterday", "Tuesday"                      */
    char        typing[48];  /* "typing..." or "Theo is typing...", "" = nobody      */
    float       typingLeft;  /* seconds of it left                                   */
    int         script;      /* the next canned reply                                */
    float       replyIn;     /* seconds until the answer to your line starts; 0 none */
    uint32_t    replyQuote;  /* the id the next answer quotes; 0 = none              */
    int32_t     count;
    MsgMessage  items[MSG_MAX_MSGS_PER_CONV];
} MsgConversation;

typedef struct {
    MsgConversation conv[MSG_MAX_CONVERSATIONS];
    int32_t         convCount;
    int32_t         openConv;              /* the chat on screen: no unread piles up there */
    int32_t         liveConv;              /* the channel a live socket owns; -1 = none   */
    bool            readReceipts;          /* off: no read ticks either way               */
    uint32_t        nowClock;              /* now: seconds since midnight                 */
    int32_t         today;                 /* days since 1970-01-01: the demo's 0, Thursday */
    int64_t         shift;                 /* how far the corpus moved off the demo's now */
    bool            wallClock;             /* a runner sets now, so dt leaves it alone    */
    float           accum;                 /* dt integrator -> whole-second ticks         */
    float           now;                   /* seconds since launch, for the timeline      */
    uint32_t        nextId;
    int32_t         typeCursor;            /* the timeline's next "starts typing"         */
    int32_t         sayCursor;             /* the timeline's next arrival                 */
    int32_t         arrivedConv;           /* a line just came here: the UI takes it; -1  */
    uint32_t        arrivedId;
    char            textPool[MSG_TEXT_POOL_BYTES];
    int32_t         textUsed;
    int32_t         outConv[MSG_MAX_OUTBOX];  /* sends in flight: the conversation ...    */
    uint32_t        outId  [MSG_MAX_OUTBOX];  /* ... the message ...                      */
    float           outAge [MSG_MAX_OUTBOX];  /* ... and how long it has been going       */
    int32_t         outCount;
    int32_t         order[MSG_MAX_CONVERSATIONS]; /* chats, as the list shows them        */
    int32_t         orderCount;
    char            totalBadge[4];
} MsgStore;

/* A case-insensitive ASCII substring test: the search filters. An empty or NULL query
   matches everything. */
static inline bool msg_text_matches(const char *text, const char *query) {
    if (!query || !query[0])
        return true;
    for (const char *base = text; base && *base; base++) {
        const char *a = base, *b = query;

        while (*a && *b) {
            char ca = *a, cb = *b;

            if (ca >= 'A' && ca <= 'Z') ca += 32;
            if (cb >= 'A' && cb <= 'Z') cb += 32;
            if (ca != cb)
                break;
            a++;
            b++;
        }
        if (!*b)
            return true;
    }
    return false;
}

static inline const MsgConversation *msg_conversation_at(const MsgStore *st, int i) {
    return (i >= 0 && i < st->convCount) ? &st->conv[i] : NULL;
}

static inline const MsgMessage *msg_last(const MsgConversation *c) {
    return c->count > 0 ? &c->items[c->count - 1] : NULL;
}

/* A line's stand-in for a place that shows it in a few words: a list row, a quote. */
static inline const char *msg_snippet(const MsgMessage *m) {
    if (m->deleted)
        return "This message was deleted";
    if (m->kind == MSG_KIND_PHOTO && !m->textLen)
        return "Photo";
    if (m->kind == MSG_KIND_STICKER)
        return "Sticker";
    if (m->kind == MSG_KIND_EVENT && !m->textLen)
        return m->who;
    return m->text;
}

static inline const char *msg_presence_label(uint8_t presence) {
    switch (presence) {
    case MSG_PRESENCE_AWAY:    return "away";
    case MSG_PRESENCE_OFFLINE: return "offline";
    default:                   return "online";
    }
}

static inline const char *msg_unread_badge(const MsgStore *st) { return st->totalBadge; }

enum {
    P_YOU = 0, P_MAYA, P_THEO, P_PRIYA, P_LENA, P_JONAS, P_DANIEL, P_MOM, P_TOMAS, P_DENTIST,
    P_KEN, P_ANA, P_ROB, P_SOFIA, P_CREW, P_APT, P_BOOKS, P_OLDAPT, P_GENERAL, P_DESIGN,
    P_RANDOM, P_COUNT
};

typedef struct {
    const char *name;     /* how a list and a header name them     */
    const char *shortName;/* how a group's bubble names them       */
    const char *about;
    const char *detail;
    uint32_t    tintA, tintB;
    uint8_t     presence;
    uint8_t     kind;
} MsgPerson;

/* The conversations, in store order, which is also what the element ids are built from. */
enum {
    C_MAYA = 0, C_CREW, C_MOM, C_PRIYA, C_APT, C_DANIEL, C_LENA, C_BOOKS, C_TOMAS, C_DENTIST,
    C_JONAS, C_KEN, C_OLDAPT, C_GENERAL, C_DESIGN, C_RANDOM, C_SEED_COUNT
};

/* Who the new-chat list offers: everyone but you, the groups and the channels. */
#define MSG_CONTACT_FIRST P_MAYA
#define MSG_CONTACT_LAST  P_SOFIA

void msg_memzero(void *p, size_t n);
bool msg_same_text(const char *a, const char *b);
void msg_store_seed(MsgStore *st);
void msg_store_step(MsgStore *st, float dt);   /* dt <= 0 => no-op (the freeze) */
void msg_store_set_now(MsgStore *st, int32_t date, uint32_t clock);
float msg_next_change(const MsgStore *st);
int  msg_send(MsgStore *st, int convo, MsgKind kind, int art, const char *text, int len,
              uint32_t replyTo, const char *who);
void msg_mark_read(MsgStore *st, int convo);
void msg_mark_unread(MsgStore *st, int convo);
void msg_inject_incoming(MsgStore *st, int convo);
int  msg_retry_failed(MsgStore *st, int convo);
MsgMessage *msg_find(MsgStore *st, int convo, uint32_t id);
void msg_toggle_reaction(MsgMessage *m, int reaction);
void msg_order_rebuild(MsgStore *st);
int  msg_start_chat(MsgStore *st, int person);
const MsgPerson *msg_person(int person);
void msg_seed_channels(MsgStore *st);
MsgMessage *msg_live_line(MsgStore *st, int convo, const char *who, const char *text,
                          uint32_t clock, uint8_t day, MsgKind kind, MsgDir dir);
void msg_clear_thread(MsgStore *st, int convo);
void msg_line_set_time(MsgStore *st, int convo, MsgMessage *m, uint32_t clock, uint8_t day);
void msg_arrived(MsgStore *st, int convo, MsgMessage *m);
const char *msg_day_word(const MsgStore *st, int day);
void msg_initials(const char *name, char out[3]);

/* A thread for the one job that should not hold up a frame, for main.c alone. RayClay has no
   thread call, so this is plain platform C, and it lives here because this is the one file of
   the app that may include a system header. Where a build has no
   threads (a web build without them) the job simply runs in the caller. The flag is how the
   thread hands its answer over: rcAppWake wakes the UI, and the UI's next frame reads this. */
#ifdef MESSENGER_WORKER_IMPLEMENTATION
#if defined(_WIN32)
#include <process.h>
#include <intrin.h>
static inline void msg_flag_set(volatile long *flag, long value) {
    _InterlockedExchange(flag, value);
}
static inline long msg_flag_get(volatile long *flag) {
    return _InterlockedCompareExchange(flag, 0, 0);
}
static inline void msg_worker_start(void (*job)(void *), void *arg) {
    if (_beginthread(job, 0, arg) == (uintptr_t)-1)
        job(arg);
}
#else
#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
#include <pthread.h>
#define MSG_WORKER_HAS_THREADS 1
#endif
static inline void msg_flag_set(volatile long *flag, long value) {
    __atomic_store_n(flag, value, __ATOMIC_RELEASE);
}
static inline long msg_flag_get(volatile long *flag) {
    return __atomic_load_n(flag, __ATOMIC_ACQUIRE);
}
#ifdef MSG_WORKER_HAS_THREADS
typedef struct { void (*job)(void *); void *arg; } MsgWorkerStart;
static inline void *msg_worker_main(void *p) {
    const MsgWorkerStart *start = (const MsgWorkerStart *)p;

    start->job(start->arg);
    return NULL;
}
static inline void msg_worker_start(void (*job)(void *), void *arg) {
    static MsgWorkerStart start;    /* one job at a time: the caller checks */
    pthread_t thread;

    start.job = job;
    start.arg = arg;
    if (pthread_create(&thread, NULL, msg_worker_main, &start) == 0)
        pthread_detach(thread);
    else
        job(arg);
}
#else
static inline void msg_worker_start(void (*job)(void *), void *arg) {
    job(arg);
}
#endif /* MSG_WORKER_HAS_THREADS */
#endif /* _WIN32 */
#endif /* MESSENGER_WORKER_IMPLEMENTATION */

#ifdef MESSENGER_BACKEND_IMPLEMENTATION

#include <stdio.h>
#include <string.h>

/* ---- The cast ------------------------------------------------------------------- */

/* Real people have uneven data: a status or none to speak of, some of them away, and an
   avatar gradient of their own. */
static const MsgPerson MSG_PEOPLE[P_COUNT] = {
    { "You", "You", "", "", 0x6b7c85, 0x4a5a63, 0, 0 },
    { "Maya Okafor", "Maya", "Out on the trail most weekends",
                  "+1 (415) 555-0142", 0xf2709c, 0xff9472, MSG_PRESENCE_ONLINE, 0 },
    { "Theo Marsh", "Theo", "Biscuit's human", "+1 (415) 555-0178",
                  0x4facfe, 0x00c6fb, MSG_PRESENCE_ONLINE, 0 },
    { "Priya Raman", "Priya", "In meetings until 3", "+1 (628) 555-0119",
                  0xa18cd1, 0xfbc2eb, MSG_PRESENCE_AWAY, 0 },
    { "Lena Fischer", "Lena", "Urgent calls only", "+49 30 5550 1187",
                  0x43e97b, 0x38d9a9, MSG_PRESENCE_ONLINE, 0 },
    { "Jonas Weber", "Jonas", "Making things slightly better", "+49 30 5550 1423",
                  0xfa709a, 0xfee140, MSG_PRESENCE_OFFLINE, 0 },
    { "Daniel Reyes", "Daniel", "Available", "+1 (510) 555-0167",
                  0x30cfd0, 0x5a67d8, MSG_PRESENCE_ONLINE, 0 },
    { "Mom", "Mom", "Gardening", "+1 (503) 555-0104",
                  0xf6a35c, 0xf76b8a, MSG_PRESENCE_OFFLINE, 0 },
    { "Tom\xc3\xa1s Herrera", "Tom\xc3\xa1s", "Hablamos pronto", "+34 915 550 133",
                  0x667eea, 0x764ba2, MSG_PRESENCE_AWAY, 0 },
    { "Bright Smile Dental", "Bright Smile", "Mon to Fri, 8 to 5",
                    "+1 (415) 555-0190", 0x13b3a5, 0x2dd4bf, MSG_PRESENCE_OFFLINE, 0 },
    { "Ken Sato", "Ken", "At the airport", "+81 3 5550 0162",
                  0x5f9ea0, 0x8fd3f4, MSG_PRESENCE_OFFLINE, 0 },
    { "Ana Silva", "Ana", "Reading", "+351 21 555 0126",
                  0xd57eeb, 0xfccb90, MSG_PRESENCE_ONLINE, 0 },
    { "Rob Keller", "Rob", "New number, same Rob", "+1 (206) 555-0153",
                  0x96a7b8, 0x667b8c, MSG_PRESENCE_OFFLINE, 0 },
    { "Sofia Marin", "Sofia", "Book club secretary", "+1 (415) 555-0131",
                  0xff8177, 0xb12a5b, MSG_PRESENCE_ONLINE, 0 },
    { "Weekend Hike Crew", "", "Saturday, Eagle Lake, no excuses",
                  "Maya, Theo, Priya, You", 0x11998e, 0x38ef7d, 0, MSG_CONV_GROUP },
    { "Apt 4B", "", "Rent, repairs and whose turn it is",
                  "Jonas, Lena, You", 0xf7971e, 0xffd200, 0, MSG_CONV_GROUP },
    { "Book Club", "", "One book a month, no spoilers",
                  "Sofia, Ana, Lena, You", 0x7f53ac, 0x647dee, 0, MSG_CONV_GROUP },
    { "Old Apartment", "", "Moved out in June",
                    "Rob, You", 0x8e9eab, 0x6b7a85, 0, MSG_CONV_GROUP },
    { "general", "", "Say hello. Everyone starts here.",
                    "Maya, Theo, Priya, You", 0, 0, 0, MSG_CONV_CHANNEL },
    { "design", "", "Mockups, palettes and pixels.",
                    "Jonas, Maya, You", 0, 0, 0, MSG_CONV_CHANNEL },
    { "random", "", "Anything goes.",
                    "Theo, Lena, You", 0, 0, 0, MSG_CONV_CHANNEL },
};

/* ---- The threads, written out: they are what a first look reads ------------------ */

typedef struct {
    uint8_t     day;     /* days before today          */
    uint16_t    at;      /* the time, as 1802          */
    uint8_t     from;    /* P_YOU or a member          */
    uint8_t     kind;    /* MsgKind                    */
    uint8_t     art;     /* a photo's, a sticker's     */
    const char *text;
    int8_t      quote;   /* a line of this thread, 1-based; 0 = none */
    int8_t      react;   /* a reaction + 1; 0 = none   */
    uint8_t     reactN;  /* how many gave it           */
    bool        mine;    /* you gave it too            */
    bool        star;
    uint8_t     deliv;   /* your line's ticks          */
} MsgSeedLine;

/* R_ names match messenger_art.h's first six stickers. */
enum { R_THUMB = 1, R_HEART, R_JOY, R_WOW, R_SAD, R_FIRE };
/* Sticker and photo numbers, spelled where the corpus uses them (messenger_art.h). */
enum { STK_JOY = 2, STK_TADA = 11, PH_RIDGE = 0, PH_LAKE = 1, PH_FOREST = 4, PH_CITY = 3 };

static const MsgSeedLine SEED_MAYA[] = {
    { .day = 2, .at = 1912, .from = P_MAYA, .text = "Did you ever find boots that fit?" },
    { .day = 2, .at = 1920, .from = P_YOU,
     .text = "Finally. The ones from the shop on Pine Street." },
    { .day = 2, .at = 1921, .from = P_YOU, .text = "Half a size up, and thick socks." },
    { .day = 2, .at = 1923, .from = P_MAYA, .text = "Told you! Nobody listens to me about socks." },
    { .day = 1, .at = 1802, .from = P_MAYA,
      .text = "Are we still on for Saturday? The forecast looks better than last week." },
    { .day = 1, .at = 1804, .from = P_YOU,
      .text = "Yes! I booked the cabin near Eagle Lake for Saturday night." },
    { .day = 1, .at = 1804, .from = P_YOU,
     .text = "Two nights were sold out, so it is just the one." },
    { .day = 1, .at = 1806, .from = P_MAYA, .text = "One is plenty. My knees will thank you.",
      .react = R_JOY, .reactN = 1, .mine = true },
    { .day = 1, .at = 1831, .from = P_MAYA, .kind = MSG_KIND_PHOTO, .art = PH_RIDGE,
      .text = "Saw this from the ridge trail in May. We have to go back.", .star = true },
    { .day = 1, .at = 1833, .from = P_YOU, .text = "That view!", .react = R_HEART, .reactN = 1 },
    { .day = 0, .at = 912, .from = P_YOU,
      .text = "Morning. Can you bring the big thermos? Mine cracked." },
    { .day = 0, .at = 940, .from = P_MAYA,
      .text = "Sure. The blue one, or the one that actually keeps coffee hot?" },
    { .day = 0, .at = 941, .from = P_YOU, .text = "The second one, please." },
    { .day = 0, .at = 1105, .from = P_MAYA,
      .text = "Also, Theo asked if he can bring Biscuit. Thoughts?" },
    { .day = 0, .at = 1107, .from = P_YOU,
      .text = "Only if Biscuit promises not to eat my sandwich again.", .quote = 14,
      .react = R_JOY, .reactN = 2 },
    { .day = 0, .at = 1107, .from = P_MAYA, .kind = MSG_KIND_STICKER, .art = STK_JOY },
    { .day = 0, .at = 1152, .from = P_MAYA,
      .text = "I can pick you up at seven if you want to leave your car at home." },
    { .day = 0, .at = 1158, .from = P_YOU, .text = "Perfect, the gear will be by the door." },
};

static const MsgSeedLine SEED_CREW[] = {
    { .day = 3, .at = 1830, .from = P_MAYA, .text = "Saturday hike, who is in?" },
    { .day = 3, .at = 1834, .from = P_THEO, .text = "Me and Biscuit." },
    { .day = 3, .at = 1836, .from = P_YOU, .text = "In. Eagle Lake again?" },
    { .day = 3, .at = 1841, .from = P_PRIYA, .text = "I am in if we are back by Sunday lunch." },
    { .day = 1, .at = 2010, .from = P_THEO,
      .text = "Who is driving on Saturday? My car fits four and one very large dog." },
    { .day = 1, .at = 2012, .from = P_MAYA,
      .text = "I can take two more, picking up from the east side." },
    { .day = 1, .at = 2015, .from = P_YOU, .text = "I am with Maya. Priya, do you want a seat?" },
    { .day = 1, .at = 2040, .from = P_PRIYA,
      .text = "Yes please. I will bring the first aid kit and far too many granola bars.",
      .react = R_THUMB, .reactN = 3, .mine = true },
    { .day = 1, .at = 2052, .from = P_THEO, .text = "Do we need a permit for the upper trail?" },
    { .day = 1, .at = 2055, .from = P_MAYA,
     .text = "Not after October. I checked with the ranger." },
    { .day = 0, .at = 1012, .from = P_THEO, .kind = MSG_KIND_PHOTO, .art = PH_FOREST,
      .text = "The upper loop reopened last week. This is the view from the fork." },
    { .day = 0, .at = 1014, .from = P_YOU, .text = "Upper loop it is.", .quote = 11 },
    { .day = 0, .at = 1102, .from = P_PRIYA, .text = "How long is it with the lake detour?" },
    { .day = 0, .at = 1105, .from = P_MAYA,
     .text = "About eleven miles. Five hours at Theo's pace." },
    { .day = 0, .at = 1106, .from = P_THEO, .text = "Six if Biscuit sees a squirrel.",
      .react = R_JOY, .reactN = 2 },
    { .day = 0, .at = 1129, .from = P_MAYA, .text = "Should we start early and beat the heat?" },
    { .day = 0, .at = 1133, .from = P_THEO, .text = "Meet at the trailhead at 8:30?" },
};

static const MsgSeedLine SEED_MOM[] = {
    { .day = 4, .at = 1012, .from = P_MOM, .text = "Hello dear! We went to the farmers market." },
    { .day = 4, .at = 1013, .from = P_MOM, .text = "Your father bought eleven pumpkins." },
    { .day = 4, .at = 1102, .from = P_YOU, .text = "Eleven? For what?" },
    { .day = 4, .at = 1110, .from = P_MOM,
     .text = "He says soup. I say we will be eating it until March." },
    { .day = 2, .at = 1730, .from = P_MOM, .text = "Dad finally fixed the garden gate." },
    { .day = 2, .at = 1744, .from = P_YOU, .text = "Only took him two years!" },
    { .day = 2, .at = 1746, .from = P_MOM, .text = "Don't tell him I told you. He is very proud." },
    { .day = 1, .at = 2015, .from = P_MOM, .text = "Are you coming for Thanksgiving?" },
    { .day = 1, .at = 2031, .from = P_YOU,
     .text = "Yes. I land on Wednesday night, flight gets in at nine." },
    { .day = 1, .at = 2033, .from = P_MOM, .text = "Wonderful. I will make up the blue room.",
      .react = R_HEART, .reactN = 1, .mine = true },
    { .day = 0, .at = 833, .from = P_YOU, .text = "Can you send me Grandma's cornbread recipe?" },
    { .day = 0, .at = 907, .from = P_MOM,
     .text = "It is on a card somewhere. I will look for it later." },
    { .day = 0, .at = 1041, .from = P_MOM, .text = "Did you eat today?" },
};

static const MsgSeedLine SEED_PRIYA[] = {
    { .day = 2, .at = 1420, .from = P_PRIYA, .text = "Do you have ten minutes for the Q3 review?" },
    { .day = 2, .at = 1422, .from = P_YOU, .text = "After my 3 o'clock. What is it about?" },
    { .day = 2, .at = 1423, .from = P_PRIYA,
     .text = "Finance wants the churn numbers split by region." },
    { .day = 2, .at = 1540, .from = P_YOU,
     .text = "Sent you the sheet. EMEA is the only one that moved." },
    { .day = 1, .at = 1012, .from = P_PRIYA,
     .text = "EMEA moved because of the Berlin renewal, right?" },
    { .day = 1, .at = 1018, .from = P_YOU, .text = "Exactly. Without it we are flat." },
    { .day = 1, .at = 1019, .from = P_PRIYA, .text = "Good, that is an easy story to tell.",
      .react = R_THUMB, .reactN = 1, .mine = true },
    { .day = 0, .at = 908, .from = P_PRIYA,
      .text = "The Q3 deck is in the shared folder. Slide 12 needs your numbers." },
    { .day = 0, .at = 915, .from = P_YOU, .text = "On it. Is two o'clock soon enough?" },
    { .day = 0, .at = 917, .from = P_PRIYA, .text = "Two works. Thank you!" },
    { .day = 0, .at = 1022, .from = P_YOU, .text = "Numbers are in. Revenue line moved a little." },
    { .day = 0, .at = 1023, .from = P_YOU,
     .text = "Marked it in the speaker notes, so you are not surprised.",
      .star = true },
};

static const MsgSeedLine SEED_APT[] = {
    { .day = 3, .at = 2110, .from = P_LENA,
     .text = "Whoever finished the oat milk, I know where you live." },
    { .day = 3, .at = 2112, .from = P_JONAS, .text = "In the same apartment, yes." },
    { .day = 3, .at = 2114, .from = P_YOU, .text = "It was me. Buying more tomorrow.",
      .react = R_HEART, .reactN = 1 },
    { .day = 1, .at = 1830, .from = P_JONAS,
     .text = "Trash goes out tonight. Lena, is it your turn?" },
    { .day = 1, .at = 1834, .from = P_LENA, .text = "It is Sam's. I did it last week." },
    { .day = 1, .at = 1836, .from = P_YOU, .text = "On my way." },
    { .day = 1, .at = 1915, .from = P_JONAS,
     .text = "Rent is due Friday. I already sent my share." },
    { .day = 1, .at = 1921, .from = P_LENA, .text = "Mine goes out tomorrow." },
    { .day = 0, .at = 948, .from = P_LENA, .text = "The kitchen faucet is dripping again." },
    { .day = 0, .at = 951, .from = P_JONAS,
     .text = "I called the plumber. He can come today at three." },
    { .day = 0, .at = 953, .from = P_LENA, .text = "I am at work until six." },
    { .day = 0, .at = 1002, .from = P_YOU, .text = "Can someone let the plumber in at three?",
      .deliv = MSG_DELIV_FAILED },
};

static const MsgSeedLine SEED_DANIEL[] = {
    { .day = 3, .at = 1215, .from = P_DANIEL,
     .text = "Lunch this week? I am near your office on Friday." },
    { .day = 3, .at = 1240, .from = P_YOU, .text = "Friday is good. The ramen place?" },
    { .day = 3, .at = 1241, .from = P_DANIEL, .text = "The ramen place. Twelve thirty." },
    { .day = 1, .at = 2130, .from = P_DANIEL, .text = "Are you watching the game?" },
    { .day = 1, .at = 2132, .from = P_YOU, .text = "No spoilers! I am forty minutes behind." },
    { .day = 1, .at = 2133, .from = P_DANIEL, .text = "Then stay off your phone." },
    { .day = 1, .at = 2214, .from = P_YOU, .text = "How did that goal even count?" },
    { .day = 1, .at = 2215, .from = P_DANIEL, .text = "Nobody knows. Not even the referee.",
      .react = R_JOY, .reactN = 1, .mine = true },
    { .day = 0, .at = 836, .from = P_YOU, .text = "Have you seen my laptop charger anywhere?" },
    { .day = 0, .at = 840, .from = P_YOU, .text = "I had it at your place last weekend." },
    { .day = 0, .at = 902, .from = P_DANIEL, .text = "Found it." },
    { .day = 0, .at = 902, .from = P_DANIEL, .text = "It was in your bag the whole time." },
};

static const MsgSeedLine SEED_LENA[] = {
    { .day = 4, .at = 1945, .from = P_LENA, .text = "My shoulder is killing me again." },
    { .day = 4, .at = 1952, .from = P_YOU, .text = "From the climbing gym?" },
    { .day = 4, .at = 1953, .from = P_LENA, .text = "From carrying your couch, more likely." },
    { .day = 4, .at = 1955, .from = P_YOU, .text = "I said I was sorry. I bought pizza!" },
    { .day = 2, .at = 1101, .from = P_LENA,
     .text = "How do you say physiotherapist in English again?" },
    { .day = 2, .at = 1108, .from = P_YOU,
     .text = "You just said it. Physical therapist works too." },
    { .day = 1, .at = 1640, .from = P_LENA, .text = "Do you still have the name of that physio?" },
    { .day = 1, .at = 1648, .from = P_YOU,
     .text = "Dr. Amara Cole, on Elm Street. Tell her I sent you." },
    { .day = 1, .at = 1650, .from = P_LENA, .text = "Thanks, that helps a lot!",
      .react = R_HEART, .reactN = 1, .mine = true },
    { .day = 1, .at = 1702, .from = P_LENA, .text = "She has an opening on Monday at noon." },
    { .day = 1, .at = 1704, .from = P_YOU, .text = "Take it. She books out for weeks." },
};

static const MsgSeedLine SEED_BOOKS[] = {
    { .day = 4, .at = 2010, .from = P_SOFIA, .text = "What did everyone think of the ending?" },
    { .day = 4, .at = 2014, .from = P_ANA,
     .text = "I threw the book across the room. In a good way." },
    { .day = 4, .at = 2015, .from = P_LENA, .text = "I did not see that coming at all." },
    { .day = 4, .at = 2022, .from = P_YOU, .text = "I read the last chapter twice." },
    { .day = 2, .at = 2102, .from = P_SOFIA, .text = "Next month: The Left Hand of Darkness." },
    { .day = 2, .at = 2108, .from = P_ANA,
     .text = "I have been meaning to read that for ten years." },
    { .day = 2, .at = 2110, .from = P_YOU, .text = "Same. Audiobook or paper?" },
    { .day = 2, .at = 2111, .from = P_SOFIA, .text = "Whatever gets you to the end, Sam." },
    { .day = 1, .at = 1211, .from = P_LENA, .text = "Halfway through. No spoilers, please." },
    { .day = 1, .at = 1250, .from = P_ANA,
     .text = "The ice crossing chapter. That is all I will say." },
    { .day = 1, .at = 1317, .from = P_SOFIA, .text = "We meet at my place on the 19th." },
    { .day = 1, .at = 1320, .from = P_LENA, .text = "I will bring the wine.", .react = R_FIRE,
     .reactN = 2 },
};

static const MsgSeedLine SEED_TOMAS[] = {
    { .day = 3, .at = 1830, .from = P_TOMAS,
     .text = "Ya estoy en Madrid. Hace un calor incre\xc3\xad""ble." },
    { .day = 3, .at = 1902, .from = P_YOU,
     .text = "Disfruta. Aqu\xc3\xad ya llueve todos los d\xc3\xad""as." },
    { .day = 2, .at = 1015, .from = P_TOMAS,
     .text = "\xc2\xbfTodav\xc3\xad""a vienes en noviembre?" },
    { .day = 2, .at = 1040, .from = P_YOU, .text = "Si encuentro vuelo barato, s\xc3\xad." },
    { .day = 2, .at = 1042, .from = P_TOMAS, .text = "Tengo un sof\xc3\xa1 con tu nombre." },
    { .day = 1, .at = 1109, .from = P_TOMAS,
     .text = "\xc2\xbfNos vemos ma\xc3\xb1""ana por videollamada?" },
    { .day = 1, .at = 1130, .from = P_YOU, .text = "S\xc3\xad, a las diez, hora tuya." },
    { .day = 1, .at = 1132, .from = P_TOMAS, .text = "Perfecto. Te llamo esta tarde." },
};

static const MsgSeedLine SEED_DENTIST[] = {
    { .day = 4, .at = 830, .from = P_DENTIST,
      .text = "Hello Sam, it has been six months since your last visit. Reply BOOK to schedule." },
    { .day = 4, .at = 1140, .from = P_YOU, .text = "BOOK" },
    { .day = 4, .at = 1141, .from = P_DENTIST,
     .text = "Thanks! We have 9:00 AM a week from today." },
    { .day = 4, .at = 1142, .from = P_DENTIST,
     .text = "Reply YES to take it, or call our office for other times." },
    { .day = 4, .at = 1150, .from = P_YOU, .text = "YES" },
    { .day = 2, .at = 900, .from = P_DENTIST,
      .text = "Reminder: your cleaning is in five days at 9:00 AM. Reply C to confirm." },
    { .day = 2, .at = 1215, .from = P_YOU, .text = "C" },
    { .day = 2, .at = 1216, .from = P_DENTIST, .text = "Thanks, see you then!" },
};

static const MsgSeedLine SEED_JONAS[] = {
    { .day = 5, .at = 1610, .from = P_JONAS, .text = "Moving day. Do you still have the van?" },
    { .day = 5, .at = 1622, .from = P_YOU, .text = "Until six. Do you need two trips?" },
    { .day = 5, .at = 1623, .from = P_JONAS, .text = "Three. I own too many plants." },
    { .day = 3, .at = 1944, .from = P_JONAS, .kind = MSG_KIND_PHOTO, .art = PH_CITY,
      .text = "The view from the new place. Worth the stairs." },
    { .day = 3, .at = 1950, .from = P_YOU, .text = "Jealous. What floor is that?" },
    { .day = 3, .at = 1951, .from = P_JONAS, .text = "Sixth. No elevator." },
    { .day = 3, .at = 1953, .from = P_YOU, .text = "So I will never see this view in person.",
      .react = R_JOY, .reactN = 1 },
    { .day = 3, .at = 2008, .from = P_JONAS, .text = "Housewarming on the 14th. Bring your legs." },
};

static const MsgSeedLine SEED_KEN[] = {
    { .day = 5, .at = 1022, .from = P_KEN, .text = "Which terminal are you flying from?" },
    { .day = 5, .at = 1030, .from = P_YOU, .text = "Terminal 2. Gate is not up yet." },
    { .day = 4, .at = 1603, .from = P_KEN, .text = "Boarding now. Call me when you land?" },
    { .day = 4, .at = 2230, .from = P_YOU,
     .text = "Landed. Calling you tomorrow, it is late there." },
};

static const MsgSeedLine SEED_OLDAPT[] = {
    { .day = 6, .at = 1402, .from = P_ROB, .text = "Landlord signed the checkout form." },
    { .day = 6, .at = 1410, .from = P_YOU, .text = "Finally. Any deductions?" },
    { .day = 5, .at = 1120, .from = P_ROB, .text = "None. The deposit came through. All set." },
};

/* A channel's demo lines end on what serve.py's rooms start with, line for line, so a window
   that connects to a live room later meets the same conversation. The lines before them are
   the demo room's own history. */
static const MsgSeedLine SEED_GENERAL[] = {
    { .day = 1, .at = 930, .from = P_PRIYA,
     .text = "Reminder: all-hands moves to the big room at two." },
    { .day = 1, .at = 932, .from = P_THEO, .text = "The one with the broken projector?" },
    { .day = 1, .at = 933, .from = P_PRIYA, .text = "That one. Bring a laptop, just in case." },
    { .day = 1, .at = 1420, .from = P_MAYA,
     .text = "Slides are up in the shared folder, for anyone who missed it." },
    { .day = 1, .at = 1645, .from = P_THEO,
     .text = "Facilities says someone will look at the projector tomorrow.",
      .react = R_THUMB, .reactN = 2 },
    { .at = 1146, .from = P_MAYA, .text = "Morning, everyone." },
    { .at = 1148, .from = P_THEO, .text = "Morning! The projector in the big room works again." },
    { .at = 1149, .from = P_PRIYA, .text = "Nice. Was it the cable after all?" },
    { .at = 1151, .from = P_THEO, .text = "That one. It was loose the whole time.",
      .react = R_JOY, .reactN = 2 },
};

static const MsgSeedLine SEED_DESIGN[] = {
    { .day = 2, .at = 1510, .from = P_MAYA, .text = "Do we have a decision on the avatar shape?" },
    { .day = 2, .at = 1516, .from = P_JONAS,
     .text = "Round in chats, square in rooms. People read them differently." },
    { .day = 1, .at = 1122, .from = P_JONAS, .text = "Trying a warmer gray for the dividers." },
    { .day = 1, .at = 1140, .from = P_MAYA, .text = "Looks calmer. Does it hold up in dark mode?" },
    { .day = 1, .at = 1158, .from = P_JONAS, .kind = MSG_KIND_PHOTO, .art = PH_LAKE,
      .text = "Moodboard for the new wallpaper tint." },
    { .at = 1130, .from = P_JONAS, .text = "I pushed the dark palette for the settings page." },
    { .at = 1134, .from = P_MAYA, .text = "The muted text reads much better on it.",
      .react = R_FIRE, .reactN = 1 },
};

static const MsgSeedLine SEED_RANDOM[] = {
    { .day = 3, .at = 1205, .from = P_LENA,
     .text = "Is anyone else's coffee machine making that noise?" },
    { .day = 3, .at = 1208, .from = P_THEO,
     .text = "That noise is the coffee machine asking to be descaled." },
    { .day = 1, .at = 1241, .from = P_THEO, .text = "Biscuit ate a whole baguette this morning." },
    { .day = 1, .at = 1244, .from = P_LENA, .text = "Is he okay?" },
    { .day = 1, .at = 1245, .from = P_THEO, .text = "He is thrilled.", .react = R_JOY,
     .reactN = 3 },
    { .at = 1120, .from = P_THEO, .text = "Lunch at the usual place?" },
    { .at = 1122, .from = P_LENA, .text = "Only if they still have the soup." },
};

typedef struct {
    uint8_t            person;
    uint8_t            unread;
    bool               pinned, muted, archived;
    const MsgSeedLine *lines;
    int                lineCount;
} MsgSeedConv;

#define MSG_LINES(a) a, (int)(sizeof a / sizeof a[0])
static const MsgSeedConv MSG_CONVS[C_SEED_COUNT] = {
    { P_MAYA,    0, true, false, false, MSG_LINES(SEED_MAYA) },
    { P_CREW,    3, true, false, false, MSG_LINES(SEED_CREW) },
    { P_MOM, 1, false, false, false, MSG_LINES(SEED_MOM) },
    { P_PRIYA, 0, false, false, false, MSG_LINES(SEED_PRIYA) },
    { P_APT, 0, false, false, false, MSG_LINES(SEED_APT) },
    { P_DANIEL, 2, false, false, false, MSG_LINES(SEED_DANIEL) },
    { P_LENA, 0, false, false, false, MSG_LINES(SEED_LENA) },
    { P_BOOKS, 5, false, true, false, MSG_LINES(SEED_BOOKS) },
    { P_TOMAS, 0, false, false, false, MSG_LINES(SEED_TOMAS) },
    { P_DENTIST, 0, false, false, false, MSG_LINES(SEED_DENTIST) },
    { P_JONAS, 0, false, false, false, MSG_LINES(SEED_JONAS) },
    { P_KEN, 0, false, false, true, MSG_LINES(SEED_KEN) },
    { P_OLDAPT, 0, false, false, true, MSG_LINES(SEED_OLDAPT) },
    { P_GENERAL, 2, false, false, false, MSG_LINES(SEED_GENERAL) },
    { P_DESIGN, 0, false, false, false, MSG_LINES(SEED_DESIGN) },
    { P_RANDOM, 0, false, false, false, MSG_LINES(SEED_RANDOM) },
};

/* ---- The timeline: what happens if you just watch -------------------------------- */

/* A line comes at `at` seconds after launch, its sender typing for `lead` before that.
   Sorted by arrival; two cursors walk it, so typing and arrival never need a queue. */
typedef struct {
    float       at, lead;
    uint8_t     conv, from, kind, art;
    const char *text;
} MsgBeat;

static const MsgBeat MSG_TIMELINE[] = {
    { 3.0f, 3.0f, C_MAYA, P_MAYA, MSG_KIND_TEXT, 0, "Should I bring snacks, or are you on it?" },
    { 9.0f, 2.5f, C_GENERAL, P_MAYA, MSG_KIND_TEXT, 0,
      "Has anyone seen the notes from Tuesday's meeting?" },
    { 12.0f, 7.0f, C_MAYA, P_MAYA, MSG_KIND_TEXT, 0,
      "Also, Biscuit wants to know if there is room in the car." },
    { 14.0f, 3.0f, C_CREW, P_PRIYA, MSG_KIND_TEXT, 0,
     "Granola bars are packed. All forty of them." },
    { 16.0f, 2.0f, C_GENERAL, P_PRIYA, MSG_KIND_TEXT, 0,
      "They are in the shared folder, under this month." },
    { 21.0f, 2.0f, C_DANIEL, P_DANIEL, MSG_KIND_TEXT, 0, "You owe me a coffee for this." },
    { 24.0f, 2.0f, C_DESIGN, P_JONAS, MSG_KIND_TEXT, 0, "Next up: the empty states." },
    { 30.0f, 2.5f, C_GENERAL, P_THEO, MSG_KIND_TEXT, 0,
      "I added the budget figures to them last night." },
    { 36.0f, 0.0f, C_GENERAL, P_LENA, MSG_KIND_EVENT, 0, "joined" },
    { 40.0f, 2.0f, C_GENERAL, P_LENA, MSG_KIND_TEXT, 0, "Hi all. Just catching up." },
    { 46.0f, 3.0f, C_MOM, P_MOM, MSG_KIND_TEXT, 0,
     "Call me when you get a minute. Nothing urgent!" },
    { 52.0f, 2.5f, C_DESIGN, P_MAYA, MSG_KIND_TEXT, 0,
     "Keep them short. One line and one action." },
    { 60.0f, 2.0f, C_RANDOM, P_THEO, MSG_KIND_TEXT, 0, "They do. Noon?" },
    { 67.0f, 1.5f, C_RANDOM, P_LENA, MSG_KIND_TEXT, 0, "Noon works." },
    { 75.0f, 2.0f, C_CREW, P_THEO, MSG_KIND_STICKER, STK_TADA, NULL },
    { 90.0f, 0.0f, C_DESIGN, P_MAYA, MSG_KIND_EVENT, 0, "left" },
};
#define MSG_TIMELINE_LEN ((int)(sizeof MSG_TIMELINE / sizeof MSG_TIMELINE[0]))

/* What each chat says back when you write to it, in turn. */
static const char *const MSG_REPLIES[C_SEED_COUNT][4] = {
    { "Deal. I will grab the good trail mix.", "Ha, fair enough.",
                    "See you Saturday then.", "Sending you the address now." },
    { "Works for me.", "Good call.", "Noted, thanks!", "Biscuit approves." },
    { "Good. Eat something green too.", "Love you, call me soon.",
                    "Dad says hello.", "Okay dear." },
    { "Perfect, thank you.", "Let me look after this meeting.",
                    "That matches my numbers.", "Great, I will update the deck." },
    { "I can do that.", "Whose turn is it to buy trash bags?",
                    "Sounds good.", "Thanks for handling it." },
    { "Coffee first, then we talk.", "Tomorrow works.",
                    "Ha! Told you.", "On my way." },
    { "You are the best.", "Booked for next week.", "Will do!", "Thanks again." },
    { "Page 200 and I have opinions.", "I will bring snacks.",
                    "Good question for the 19th.", "No spoilers!" },
    { "Vale, hablamos.", "\xc2\xa1Genial!", "Claro que s\xc3\xad.", "Hasta luego." },
    { "We have noted your reply. Thank you.", "Our office opens at 8:00 AM.",
                    "Please call us to change your visit.", "See you then." },
    { "Sixth floor, I swear.", "Come by and see it.",
                    "My legs are getting strong.", "Haha, thanks." },
    { "Safe travels.", "Talk tomorrow.", "Sounds good.", "Thanks!" },
    { "All done on my side.", "Thanks!", "Good luck.", "Take care." },
    { "Good point.", "Agreed. Let us try it.", "Thanks, noted.",
                    "I will look at it after lunch." },
    { "Good point.", "Agreed. Let us try it.", "Thanks, noted.", "Ha, fair enough." },
    { "Ha, fair enough.", "Good point.", "Noted.", "Agreed." },
};

/* A chat started from the contact list has no script of its own. */
static const char *const MSG_NEW_CHAT_REPLIES[4] = {
    "Hi! Good to hear from you.", "Sounds good.", "Let me get back to you on that.", "Talk soon!",
};

/* Who answers in a group or a channel, in turn. */
static const uint8_t MSG_ANSWERERS[C_SEED_COUNT][3] = {
    { P_MAYA, P_MAYA, P_MAYA }, { P_THEO, P_MAYA, P_PRIYA }, { P_MOM, P_MOM, P_MOM },
    { P_PRIYA, P_PRIYA, P_PRIYA }, { P_LENA, P_JONAS, P_LENA }, { P_DANIEL, P_DANIEL, P_DANIEL },
    { P_LENA, P_LENA, P_LENA }, { P_SOFIA, P_ANA, P_LENA }, { P_TOMAS, P_TOMAS, P_TOMAS },
    { P_DENTIST, P_DENTIST, P_DENTIST }, { P_JONAS, P_JONAS, P_JONAS }, { P_KEN, P_KEN, P_KEN },
    { P_ROB, P_ROB, P_ROB }, { P_THEO, P_PRIYA, P_MAYA }, { P_MAYA, P_JONAS, P_MAYA },
    { P_LENA, P_THEO, P_LENA },
};

/* ---- Small pieces ---------------------------------------------------------------- */

void msg_memzero(void *p, size_t n) {
    memset(p, 0, n);
}

/* Whether two strings say the same, where either may be NULL: the app's one string compare,
   here because the app's own files call no libc. */
bool msg_same_text(const char *a, const char *b) {
    return a && b ? strcmp(a, b) == 0 : a == b;
}

const MsgPerson *msg_person(int person) {
    return &MSG_PEOPLE[person];
}

static void msg_format_clock(uint32_t s, char out[6]) {
    snprintf(out, 6, "%02u:%02u", (s / 3600u) % 24u, (s / 60u) % 60u);
}

/* A time counted from today's midnight, turned into its time of day and how many days before
   today it falls; a week or more back is day 7, "Earlier". */
static uint8_t msg_day_of(int64_t *t) {
    int day = 0;

    for (; *t < 0; *t += 86400)
        day++;
    return (uint8_t)(day < 7 ? day : 7);
}

/* A line's day as a chat list names it: today, yesterday, then the weekday this week, counted
   from the store's date (day 0, 1970-01-01, was a Thursday). */
const char *msg_day_word(const MsgStore *st, int day) {
    static const char *const WEEKDAYS[7] = {
        "Thursday", "Friday", "Saturday", "Sunday", "Monday", "Tuesday", "Wednesday",
    };

    if (day == 0 || day == 1)
        return day ? "Yesterday" : "Today";
    return day > 1 && day < 7 ? WEEKDAYS[((st->today - day) % 7 + 7) % 7] : "Earlier";
}

static void msg_set_badge(MsgConversation *c) {
    if (c->unread == 0)
        c->badge[0] = '\0';
    else if (c->unread > 99)
        memcpy(c->badge, "99+", 4);
    else
        snprintf(c->badge, sizeof c->badge, "%u", (unsigned)c->unread);
}

static void msg_set_total_badge(MsgStore *st) {
    int total = 0;

    for (int i = 0; i < st->convCount; i++) {
        const MsgConversation *c = &st->conv[i];

        if (!c->hidden && !c->archived && !c->muted)
            total += c->unread;
    }
    if (total <= 0)
        st->totalBadge[0] = '\0';
    else if (total > 99)
        memcpy(st->totalBadge, "99+", 4);
    else
        snprintf(st->totalBadge, sizeof st->totalBadge, "%d", total);
}

/* Two initials, first word and last; a one-word name keeps one. */
void msg_initials(const char *name, char out[3]) {
    const char *space = strrchr(name, ' ');

    out[0] = name[0];
    out[1] = space ? space[1] : '\0';
    out[2] = '\0';
}

/* The list row's right-hand word and sort key follow the conversation's last line. */
static void msg_set_last(const MsgStore *st, MsgConversation *c) {
    const MsgMessage *m = c->count ? &c->items[c->count - 1] : NULL;

    if (!m) {
        c->lastTime[0] = '\0';
        return;
    }
    c->lastStamp = (uint32_t)(30 - m->day) * 86400u + m->clock;
    snprintf(c->lastTime, sizeof c->lastTime, "%s", m->day ? msg_day_word(st, m->day) : m->ts);
}

/* The pool is a ring: when it wraps, whatever it is about to overwrite is dropped from
   its thread, oldest first by construction, so a long live session keeps its newest lines
   and never points at freed text. */
static void msg_evict(MsgStore *st, const char *lo, const char *hi) {
    for (int k = 0; k < st->convCount; k++) {
        MsgConversation *c = &st->conv[k];

        for (int i = c->count - 1; i >= 0; i--) {
            const MsgMessage *m = &c->items[i];
            const char *start = m->who && m->pooled ? m->who : m->text;

            if (!m->pooled || start >= hi || m->text + m->textLen + 1 <= lo)
                continue;
            memmove(&c->items[i], &c->items[i + 1],
                    (size_t)(c->count - i - 1) * sizeof c->items[0]);
            c->count--;
        }
    }
}

static char *msg_pool_take(MsgStore *st, int need) {
    if (need > MSG_TEXT_POOL_BYTES)
        return NULL;
    if (st->textUsed + need > MSG_TEXT_POOL_BYTES)
        st->textUsed = 0;
    char *dst = &st->textPool[st->textUsed];

    msg_evict(st, dst, dst + need);
    st->textUsed += need;
    return dst;
}

/* A slot at the thread's end: over its oldest line once the thread is full. */
static MsgMessage *msg_slot(MsgStore *st, MsgConversation *c) {
    if (c->count == MSG_MAX_MSGS_PER_CONV) {
        memmove(&c->items[0], &c->items[1], (size_t)(c->count - 1) * sizeof c->items[0]);
        c->count--;
    }
    MsgMessage *m = &c->items[c->count++];

    memset(m, 0, sizeof *m);
    m->id = st->nextId++;
    return m;
}

static MsgMessage *msg_append(MsgStore *st, MsgConversation *c, MsgKind kind, MsgDir dir,
                              const char *text, const char *who, uint32_t clock, uint8_t day) {
    MsgMessage *m = msg_slot(st, c);

    m->text    = text ? text : "";
    m->textLen = (uint16_t)strlen(m->text);
    m->who     = who;
    m->kind    = (uint8_t)kind;
    m->dir     = (uint8_t)dir;
    m->clock   = clock;
    m->day     = day;
    m->age     = MSG_ENTER_SECONDS;
    msg_format_clock(clock, m->ts);
    return m;
}

static void msg_seed_thread(MsgStore *st, MsgConversation *c, const MsgSeedConv *def) {
    uint32_t ids[MSG_MAX_MSGS_PER_CONV];

    c->count = 0;
    for (int k = 0; k < def->lineCount && k < MSG_MAX_MSGS_PER_CONV; k++) {
        const MsgSeedLine *l = &def->lines[k];
        const bool mine = l->from == P_YOU;
        /* Its time from today's midnight, moved as far as a wall clock moved now: same age. */
        int64_t t = (int64_t)(l->at / 100) * 3600 + (int64_t)(l->at % 100) * 60
                  - 86400 * (int64_t)l->day + st->shift;
        const uint8_t day = msg_day_of(&t);
        MsgMessage *m = msg_append(st, c, (MsgKind)l->kind,
                                    mine ? MSG_DIR_OUTGOING : MSG_DIR_INCOMING, l->text,
                                    MSG_PEOPLE[l->from].shortName, (uint32_t)t, day);

        ids[k]     = m->id;
        m->art     = l->art;
        m->deliv   = l->deliv;
        m->starred = l->star;
        if (l->quote > 0 && l->quote <= k)
            m->replyTo = ids[l->quote - 1];
        if (l->react > 0) {
            m->reactions[l->react - 1] = l->reactN;
            m->reacted = l->mine ? (uint8_t)(1u << (l->react - 1)) : 0;
        }
    }
    msg_set_last(st, c);
}

/* Build one conversation from the corpus; every seeded chat goes through here. */
static void msg_seed_conv(MsgStore *st, int i) {
    const MsgSeedConv *def = &MSG_CONVS[i];
    const MsgPerson   *p   = &MSG_PEOPLE[def->person];
    MsgConversation   *c   = &st->conv[i];

    snprintf(c->name, sizeof c->name, "%s", p->name);
    msg_initials(p->name, c->initials);
    c->kind     = p->kind;
    c->presence = p->presence;
    c->person   = def->person;
    c->tintA    = p->tintA;
    c->tintB    = p->tintB;
    c->about    = p->about;
    c->detail   = p->detail;
    c->pinned   = def->pinned;
    c->muted    = def->muted;
    c->archived = def->archived;
    c->unread   = def->unread;
    msg_set_badge(c);
    msg_seed_thread(st, c, def);
}

void msg_store_seed(MsgStore *st) {
    memset(st, 0, sizeof *st);
    st->nowClock    = MSG_EPOCH_SECONDS;
    st->nextId      = 1;
    st->liveConv    = -1;
    st->arrivedConv = -1;
    st->convCount   = C_SEED_COUNT;
    st->readReceipts = true;
    for (int i = 0; i < C_SEED_COUNT; i++)
        msg_seed_conv(st, i);
    msg_set_total_badge(st);
    msg_order_rebuild(st);
}

/* Back to the channels' demo lines: leaving a live room gives the rooms back as they
   were, with no line a server sent. */
void msg_seed_channels(MsgStore *st) {
    for (int i = C_GENERAL; i <= C_RANDOM; i++) {
        msg_seed_thread(st, &st->conv[i], &MSG_CONVS[i]);
        st->conv[i].unread = 0;
        msg_set_badge(&st->conv[i]);
    }
    msg_set_total_badge(st);
}

void msg_clear_thread(MsgStore *st, int convo) {
    st->conv[convo].count = 0;
    msg_set_last(st, &st->conv[convo]);
}

MsgMessage *msg_find(MsgStore *st, int convo, uint32_t id) {
    if (convo < 0 || convo >= st->convCount || !id)
        return NULL;
    MsgConversation *c = &st->conv[convo];

    for (int i = c->count - 1; i >= 0; i--)
        if (c->items[i].id == id)
            return &c->items[i];
    return NULL;
}

/* A line has come into a thread: its entrance, the list's word, the unread count and the
   UI's notice. */
void msg_arrived(MsgStore *st, int convo, MsgMessage *m) {
    MsgConversation *c = &st->conv[convo];

    m->age = 0.0f;
    msg_set_last(st, c);
    msg_order_rebuild(st);
    if (convo != st->openConv) {
        /* It stops at the top: a busy live room behind another chat passes 65,535 lines in a
           long enough session, and one more would wrap the count around to no badge at all. */
        if (c->unread < UINT16_MAX)
            c->unread++;
        msg_set_badge(c);
        msg_set_total_badge(st);
        st->arrivedConv = convo;
        st->arrivedId   = m->id;
    }
}

/* Someone says a line, by the script or by answering you. A timed answer still being written
   keeps its dots and still comes; the script's open-ended dots end with the line they led to.
   A person who writes to you is online, so a one-to-one chat's header says so from then on. */
static void msg_say(MsgStore *st, int convo, int from, MsgKind kind, int art, const char *text,
                    uint32_t quote) {
    MsgConversation *c = &st->conv[convo];
    MsgMessage *m = msg_append(st, c, kind, MSG_DIR_INCOMING, text, MSG_PEOPLE[from].shortName,
                                st->nowClock, 0);

    m->art     = (uint8_t)art;
    m->replyTo = quote;
    if (c->kind == MSG_CONV_DIRECT)
        c->presence = MSG_PRESENCE_ONLINE;
    if (c->typingLeft <= 0.0f)
        c->typing[0] = '\0';
    msg_arrived(st, convo, m);
}

/* "typing..." in a one-to-one chat, "Theo is typing..." where it could be anyone. */
static void msg_start_typing(MsgConversation *c, int from, float seconds) {
    if (c->kind == MSG_CONV_DIRECT)
        snprintf(c->typing, sizeof c->typing, "typing...");
    else
        snprintf(c->typing, sizeof c->typing, "%s is typing...", MSG_PEOPLE[from].shortName);
    c->typingLeft = seconds;
}

/* The ticks of a send walk on the same injected clock as everything else, and the last
   one, read, waits for someone who is online. */
static void msg_step_outbox(MsgStore *st, float dt) {
    for (int i = st->outCount - 1; i >= 0; i--) {
        MsgMessage *m = msg_find(st, st->outConv[i], st->outId[i]);
        const MsgConversation *c = &st->conv[st->outConv[i]];
        bool done = true;

        st->outAge[i] += dt;
        if (m) {
            const float age = st->outAge[i];
            const bool canRead = st->readReceipts && c->presence == MSG_PRESENCE_ONLINE;

            if (age < 0.6f) {
                m->deliv = MSG_DELIV_SENDING;
                done = false;
            } else if (age < 1.3f) {
                m->deliv = MSG_DELIV_SENT;
                done = false;
            } else if (age < 2.2f || !canRead) {
                m->deliv = MSG_DELIV_DELIVERED;
                done = canRead ? false : age >= 2.2f;
            } else {
                m->deliv = MSG_DELIV_READ;
            }
        }
        if (done) {
            for (int j = i + 1; j < st->outCount; j++) {
                st->outConv[j - 1] = st->outConv[j];
                st->outId  [j - 1] = st->outId  [j];
                st->outAge [j - 1] = st->outAge [j];
            }
            st->outCount--;
        }
    }
}

static const char *msg_reply_line(const MsgConversation *c, int k) {
    return k < C_SEED_COUNT ? MSG_REPLIES[k][c->script % 4]
                            : MSG_NEW_CHAT_REPLIES[c->script % 4];
}

static int msg_answerer(const MsgConversation *c, int k) {
    if (c->kind == MSG_CONV_DIRECT || k >= C_SEED_COUNT)
        return c->person;
    return MSG_ANSWERERS[k][c->script % 3];
}

/* Your line is answered: a pause, the typing dots for as long as the answer would take
   to write, then the answer. A chat a live socket owns never starts this (msg_send). */
static void msg_step_conv(MsgStore *st, int k, float dt) {
    MsgConversation *c = &st->conv[k];

    if (c->replyIn > 0.0f) {
        c->replyIn -= dt;
        if (c->replyIn <= 0.0f)
            msg_start_typing(c, msg_answerer(c, k),
                              0.9f + 0.035f * (float)strlen(msg_reply_line(c, k)));
    }
    if (c->typingLeft > 0.0f) {
        c->typingLeft -= dt;
        if (c->typingLeft <= 0.0f && c->replyIn <= 0.0f && c->typing[0]) {
            const uint32_t quote = c->replyQuote;

            c->replyQuote = 0;
            msg_say(st, k, msg_answerer(c, k), MSG_KIND_TEXT, 0, msg_reply_line(c, k), quote);
            c->script++;
        }
    }
}

/* The timeline's beats that have come due. A channel a live socket owns takes no
   simulated line, and a conversation already answering you is not interrupted. */
static void msg_step_timeline(MsgStore *st) {
    while (st->typeCursor < MSG_TIMELINE_LEN) {
        const MsgBeat *b = &MSG_TIMELINE[st->typeCursor];
        MsgConversation *c = &st->conv[b->conv];

        if (st->now < b->at - b->lead)
            break;
        if (b->lead > 0.0f && b->conv != st->liveConv && c->replyIn <= 0.0f
            && c->typingLeft <= 0.0f)
            msg_start_typing(c, b->from, 0.0f);
        st->typeCursor++;
    }
    while (st->sayCursor < MSG_TIMELINE_LEN) {
        const MsgBeat *b = &MSG_TIMELINE[st->sayCursor];

        if (st->now < b->at)
            break;
        st->sayCursor++;
        if (b->conv == st->liveConv)
            continue;
        if (b->kind == MSG_KIND_EVENT) {
            MsgMessage *m = msg_append(st, &st->conv[b->conv], MSG_KIND_EVENT, MSG_DIR_INCOMING,
                                        b->text, MSG_PEOPLE[b->from].shortName, st->nowClock, 0);

            m->age = 0.0f;
        } else {
            msg_say(st, b->conv, b->from, (MsgKind)b->kind, b->art, b->text, 0);
        }
    }
}

void msg_store_step(MsgStore *st, float dt) {
    if (dt <= 0.0f)                      /* the freeze: a strict no-op */
        return;
    st->now   += dt;
    st->accum += st->wallClock ? 0.0f : dt;   /* a wall clock sets nowClock itself */
    while (st->accum >= 1.0f) {
        st->accum -= 1.0f;
        st->nowClock++;
    }
    msg_step_outbox(st, dt);
    for (int k = 0; k < st->convCount; k++) {
        MsgConversation *c = &st->conv[k];

        msg_step_conv(st, k, dt);
        /* Only the newest lines are still arriving: walk back until a settled one. */
        for (int i = c->count - 1; i >= 0 && c->items[i].age < MSG_ENTER_SECONDS; i--)
            c->items[i].age += dt;
    }
    msg_step_timeline(st);
}

static float msg_sooner(float next, float t) {
    if (t < 0.0f)
        t = 0.0f;
    return t < next ? t : next;
}

/* How long nothing here will change, in seconds, so an idle window can park until then:
   0 means something is moving now, and a negative value means nothing is scheduled. */
float msg_next_change(const MsgStore *st) {
    const float none = 1.0e9f;
    float next = none;

    if (st->sayCursor < MSG_TIMELINE_LEN)
        next = msg_sooner(next, MSG_TIMELINE[st->sayCursor].at - st->now);
    if (st->typeCursor < MSG_TIMELINE_LEN) {
        const MsgBeat *type = &MSG_TIMELINE[st->typeCursor];

        next = msg_sooner(next, type->at - type->lead - st->now);
    }
    for (int i = 0; i < st->outCount; i++) {
        const float a = st->outAge[i];

        next = msg_sooner(next, a < 0.6f ? 0.6f - a : (a < 1.3f ? 1.3f - a : 2.2f - a));
    }
    for (int k = 0; k < st->convCount; k++) {
        const MsgConversation *c = &st->conv[k];

        if (c->replyIn > 0.0f)
            next = msg_sooner(next, c->replyIn);
        if (c->typingLeft > 0.0f)
            next = msg_sooner(next, c->typingLeft);
        if (c->count && c->items[c->count - 1].age < MSG_ENTER_SECONDS)
            next = 0.0f;
    }
    return next == none ? -1.0f : next;
}

/* Your line: it enters the thread on its way, and the chat answers. A channel a live
   socket owns sends it for real (live_room.h), so it stays queued and nothing answers.
   Returns its index, or -1 with nothing changed when the pool cannot hold it. */
int msg_send(MsgStore *st, int convo, MsgKind kind, int art, const char *text, int len,
                    uint32_t replyTo, const char *who) {
    if (convo < 0 || convo >= st->convCount)
        return -1;
    MsgConversation *c = &st->conv[convo];
    const char *owned = "";
    const char *ownedWho = NULL;

    if (len > 0 || who) {
        const int whoLen = who ? (int)strlen(who) + 1 : 0;
        char *dst = msg_pool_take(st, whoLen + len + 1);

        if (!dst)
            return -1;
        if (who) {
            memcpy(dst, who, (size_t)whoLen);
            ownedWho = dst;
            dst += whoLen;
        }
        if (len > 0)
            memcpy(dst, text, (size_t)len);
        dst[len > 0 ? len : 0] = '\0';
        owned = dst;
    }
    /* Every room, a live one too, runs on the app's one clock (live_room.h maps the wire's). */
    MsgMessage *m = msg_append(st, c, kind, MSG_DIR_OUTGOING, owned, ownedWho, st->nowClock, 0);

    m->pooled  = len > 0 || who;
    m->art     = (uint8_t)art;
    m->replyTo = replyTo;
    m->age     = 0.0f;
    msg_set_last(st, c);
    msg_order_rebuild(st);
    if (convo == st->liveConv) {
        m->deliv = MSG_DELIV_QUEUED;
    } else {
        m->deliv = MSG_DELIV_SENDING;
        if (st->outCount == MSG_MAX_OUTBOX) {
            /* The oldest in flight has had its moment: it reads delivered from here. */
            MsgMessage *old = msg_find(st, st->outConv[0], st->outId[0]);

            if (old)
                old->deliv = MSG_DELIV_DELIVERED;
            memmove(&st->outConv[0], &st->outConv[1], (MSG_MAX_OUTBOX - 1) * sizeof st->outConv[0]);
            memmove(&st->outId[0], &st->outId[1], (MSG_MAX_OUTBOX - 1) * sizeof st->outId[0]);
            memmove(&st->outAge[0], &st->outAge[1], (MSG_MAX_OUTBOX - 1) * sizeof st->outAge[0]);
            st->outCount--;
        }
        st->outConv[st->outCount] = convo;
        st->outId  [st->outCount] = m->id;
        st->outAge [st->outCount] = 0.0f;
        st->outCount++;
        if (c->replyIn <= 0.0f && c->typingLeft <= 0.0f) {
            c->typing[0] = '\0';
            c->replyIn   = 2.2f;
        }
    }
    return c->count - 1;
}

/* A line a live room delivers: it owns a copy of what the socket's event held. */
MsgMessage *msg_live_line(MsgStore *st, int convo, const char *who, const char *text,
                                 uint32_t clock, uint8_t day, MsgKind kind, MsgDir dir) {
    const int whoLen = (int)strlen(who) + 1;
    const int len = (int)strlen(text);
    char *dst = msg_pool_take(st, whoLen + len + 1);

    if (!dst)
        return NULL;
    memcpy(dst, who, (size_t)whoLen);
    memcpy(dst + whoLen, text, (size_t)len + 1);
    MsgMessage *m = msg_append(st, &st->conv[convo], kind, dir, dst + whoLen, dst, clock, day);

    m->pooled = true;
    msg_set_last(st, &st->conv[convo]);
    return m;
}

/* A line takes the time its room stamped on it. */
void msg_line_set_time(MsgStore *st, int convo, MsgMessage *m, uint32_t clock, uint8_t day) {
    m->clock = clock;
    m->day   = day;
    msg_format_clock(clock, m->ts);
    msg_set_last(st, &st->conv[convo]);
}

/* "Now" from a wall clock: the user's date, in days since 1970-01-01, and the time of day.
   The first call moves every line, and the corpus a channel is seeded from again, as far as
   now moves, so each line keeps its age. After it a line stays at its time, a new date makes
   it older, and the clock is the runner's alone: msg_store_step leaves it to these calls. */
void msg_store_set_now(MsgStore *st, int32_t date, uint32_t clock) {
    const int64_t shift = st->wallClock ? -86400 * ((int64_t)date - st->today)
                                        : (int64_t)clock - (int64_t)st->nowClock;
    const bool moved = shift != 0 || date != st->today;   /* the list names weekdays too */

    st->wallClock = true;
    st->today     = date;
    st->nowClock  = clock;
    st->shift    += shift;
    for (int k = 0; moved && k < st->convCount; k++) {
        MsgConversation *c = &st->conv[k];

        for (int i = 0; i < c->count; i++) {
            int64_t t = (int64_t)c->items[i].clock - 86400 * (int64_t)c->items[i].day + shift;
            const uint8_t day = msg_day_of(&t);

            msg_line_set_time(st, k, &c->items[i], (uint32_t)t, day);
        }
    }
}

void msg_mark_read(MsgStore *st, int convo) {
    if (convo < 0 || convo >= st->convCount)
        return;
    st->conv[convo].unread = 0;
    st->conv[convo].markedUnread = false;
    msg_set_badge(&st->conv[convo]);
    msg_set_total_badge(st);
}

/* "Mark as unread": the dot with no number, as the app everyone knows draws it. */
void msg_mark_unread(MsgStore *st, int convo) {
    if (convo < 0 || convo >= st->convCount)
        return;
    st->conv[convo].markedUnread = true;
    msg_set_total_badge(st);
}

void msg_inject_incoming(MsgStore *st, int convo) {
    if (convo < 0 || convo >= st->convCount)
        return;
    msg_say(st, convo, st->conv[convo].person, MSG_KIND_TEXT, 0,
             "Just saw the photos from Sunday. So nice.", 0);
}

/* Put every failed line in a conversation back on its way. Returns how many. */
int msg_retry_failed(MsgStore *st, int convo) {
    if (convo < 0 || convo >= st->convCount)
        return 0;
    MsgConversation *c = &st->conv[convo];
    int requeued = 0;

    for (int i = 0; i < c->count; i++) {
        if (c->items[i].deliv != MSG_DELIV_FAILED || st->outCount == MSG_MAX_OUTBOX)
            continue;
        c->items[i].deliv = MSG_DELIV_SENDING;
        st->outConv[st->outCount] = convo;
        st->outId  [st->outCount] = c->items[i].id;
        st->outAge [st->outCount] = 0.0f;
        st->outCount++;
        requeued++;
    }
    return requeued;
}

/* Your reaction goes on or comes off; only one of yours stands on a line at a time. */
void msg_toggle_reaction(MsgMessage *m, int reaction) {
    const uint8_t bit = (uint8_t)(1u << reaction);

    if (m->reacted & bit) {
        m->reacted = 0;
        if (m->reactions[reaction])
            m->reactions[reaction]--;
        return;
    }
    for (int k = 0; k < MSG_REACTION_COUNT; k++)
        if ((m->reacted & (1u << k)) && m->reactions[k])
            m->reactions[k]--;
    m->reacted = bit;
    m->reactions[reaction]++;
}

/* The list's order: pinned first, then newest first. Insertion sort over at most twenty
   rows, so the order can be rebuilt every frame and need not be invalidated; the total on the
   Chats tab follows the same flags, so it is rebuilt with it. */
void msg_order_rebuild(MsgStore *st) {
    int n = 0;

    msg_set_total_badge(st);
    for (int i = 0; i < st->convCount; i++) {
        const MsgConversation *c = &st->conv[i];

        if (c->kind == MSG_CONV_CHANNEL || c->hidden)
            continue;
        int at = n++;

        while (at > 0) {
            const MsgConversation *o = &st->conv[st->order[at - 1]];

            if (o->pinned > c->pinned || (o->pinned == c->pinned && o->lastStamp >= c->lastStamp))
                break;
            st->order[at] = st->order[at - 1];
            at--;
        }
        st->order[at] = i;
    }
    st->orderCount = n;
}

/* A new chat from the contact list: the one you already have, else a fresh one. */
int msg_start_chat(MsgStore *st, int person) {
    for (int i = 0; i < st->convCount; i++) {
        if (st->conv[i].person == person && st->conv[i].kind == MSG_CONV_DIRECT) {
            st->conv[i].hidden = false;
            st->conv[i].archived = false;
            return i;
        }
    }
    if (st->convCount == MSG_MAX_CONVERSATIONS)
        return -1;
    const MsgPerson *p = &MSG_PEOPLE[person];
    MsgConversation *c = &st->conv[st->convCount];

    memset(c, 0, sizeof *c);
    snprintf(c->name, sizeof c->name, "%s", p->name);
    msg_initials(p->name, c->initials);
    c->person   = (uint8_t)person;
    c->presence = p->presence;
    c->tintA    = p->tintA;
    c->tintB    = p->tintB;
    c->about    = p->about;
    c->detail   = p->detail;
    c->lastStamp = (uint32_t)30 * 86400u + st->nowClock;
    return st->convCount++;
}

#endif /* MESSENGER_BACKEND_IMPLEMENTATION */
#endif /* MESSENGER_BACKEND_H */
