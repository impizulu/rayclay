/*
    messenger_backend.h - the messenger app's model: conversations, messages,
    presence, delivery state, the outbox and the incoming schedule.

    Pure C99, no RayClay dependency, no clock, no rand(), no I/O: it takes an
    injected `dt` and is reproducible frame for frame. Every timestamp is a
    fixed-length "HH:MM" precomputed at seed, and every displayed number is
    formatted here into a fixed buffer, so the GUI never formats a string.

    Usage (define the implementation in exactly one TU):
        #define MESSENGER_BACKEND_IMPLEMENTATION
        #include "messenger_backend.h"

    It also owns the raw-memory helpers (msg_memzero) so the GUI stays free of
    system includes.
*/
#ifndef MESSENGER_BACKEND_H
#define MESSENGER_BACKEND_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* MSGDEF: `static` by default (header-only, one TU). Kept as a seam in case a
   future build wants external linkage. */
#ifndef MSGDEF
#define MSGDEF static
#endif

/* Fixed capacities - the store is a flat value type (memset-seedable, zero
   malloc), sized to honour the RAM tenet (~35 KB). Seed uses a subset. */
#define MSG_MAX_CONVERSATIONS   16
#define MSG_MAX_MSGS_PER_CONV   64
#define MSG_TEXT_POOL_BYTES   4096
#define MSG_NAME_CAP            24
#define MSG_EPOCH_SECONDS    43200   /* the frozen "now" == 12:00:00, no calendar */

typedef enum { MSG_KIND_TEXT = 0, MSG_KIND_SYSTEM } MsgKind;
typedef enum { MSG_DIR_INCOMING = 0, MSG_DIR_OUTGOING } MsgDir;

/* The delivery state of an OUTGOING message. SENT is 0, so a memset store reads
   as delivered and an incoming message never renders a mark. FAILED and OFFLINE
   are terminal until msg_retry_failed moves them back to SENDING. */
typedef enum {
    MSG_DELIV_SENT = 0,
    MSG_DELIV_SENDING,
    MSG_DELIV_FAILED,
    MSG_DELIV_OFFLINE
} MsgDelivery;

/* A contact's presence. ONLINE is 0 so the zero value is the friendly one, which
   matters because a memset store is what messenger_seed starts from. */
typedef enum {
    MSG_PRESENCE_ONLINE = 0,
    MSG_PRESENCE_AWAY,
    MSG_PRESENCE_OFFLINE
} MsgPresence;

/* One message. `text` points either into the immutable corpus (seed messages) or
   into the store's textPool (runtime-sent). `ts` is precomputed "HH:MM". */
typedef struct {
    const char *text;
    uint16_t    textLen;
    uint8_t     author;      /* index into the conversation's participant naming */
    uint8_t     deliv;       /* MsgDelivery; outgoing only (incoming reads SENT)  */
    uint32_t    clock;       /* seconds-since-midnight this message carries       */
    char        ts[6];       /* "HH:MM" + NUL, ALWAYS 5 glyphs (length-invariant) */
    uint8_t     kind;        /* MsgKind  */
    uint8_t     dir;         /* MsgDir   */
} MsgMessage;

/* One conversation: a contiguous message run (O(1) index, cache-friendly). */
typedef struct {
    char       name[MSG_NAME_CAP];
    char       preview[40];      /* last-message preview for the sidebar row */
    char       badge[4];         /* unread badge text, "" / "1".."9" / "9+"  */
    char       initials[3];      /* procedural avatar glyphs (Latin-1)       */
    uint8_t    presence;         /* MsgPresence; sits in initials' pad byte   */
    uint32_t   accent;           /* avatar tint (0xRRGGBB), seeded per convo  */
    uint16_t   unread;
    uint32_t   lastClock;        /* for the sidebar time column               */
    char       lastTs[6];        /* precomputed "HH:MM" of the last message   */
    int32_t    count;            /* messages in `items`                       */
    MsgMessage items[MSG_MAX_MSGS_PER_CONV];
} MsgConversation;

/* THE OUTBOX: the messages in flight, which is what makes "sending" a state
   rather than an animation. Fixed capacity, so a send allocates nothing and the
   per-frame advance walks eight entries, never every message. */
#define MSG_MAX_OUTBOX 8
#define MSG_SEND_SECONDS 1.2f            /* how long a send stays in flight */

typedef struct {
    MsgConversation conv[MSG_MAX_CONVERSATIONS];
    int32_t         convCount;
    uint32_t        nowClock;              /* current sim time, seconds-since-midnight */
    float           accum;                 /* dt integrator -> whole-second ticks       */
    char            textPool[MSG_TEXT_POOL_BYTES];
    int32_t         textUsed;
    uint32_t        rng;                   /* xorshift32 state (seed-time only)         */
    int32_t         outConv[MSG_MAX_OUTBOX];  /* the in-flight sends: conversation ... */
    int32_t         outIdx [MSG_MAX_OUTBOX];  /* ... message index ...                 */
    float           outAge [MSG_MAX_OUTBOX];  /* ... and seconds since it was sent     */
    int32_t         outCount;
    char            totalBadge[4];         /* "" / "1".."99" / "99+", unread everywhere  */
} MsgStore;

/* Case-insensitive ASCII substring test - the sidebar search filter. An empty or
   NULL query matches everything. Self-contained (no libc) so the pure-RC GUI can
   call it directly, like the accessors below. */
static inline bool msg_name_matches(const char *name, const char *query) {
    if (!query || !query[0])
        return true;
    for (const char *base = name; *base; base++) {
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

static inline int msg_conversation_count(const MsgStore *st) { return st->convCount; }
static inline const MsgConversation *msg_conversation_at(const MsgStore *st, int i) {
    return (i >= 0 && i < st->convCount) ? &st->conv[i] : NULL;
}
static inline int msg_thread_count(const MsgStore *st, int convo) {
    return (convo >= 0 && convo < st->convCount) ? st->conv[convo].count : 0;
}
static inline const MsgMessage *msg_thread_at(const MsgStore *st, int convo, int i) {
    if (convo < 0 || convo >= st->convCount)
        return NULL;
    const MsgConversation *c = &st->conv[convo];
    return (i >= 0 && i < c->count) ? &c->items[i] : NULL;
}
static inline int msg_unread_total(const MsgStore *st) {
    int total = 0;
    for (int i = 0; i < st->convCount; i++)
        total += st->conv[i].unread;
    return total;
}
/* The aggregate unread count as TEXT. The GUI core is rcFormat-free, so the store
   keeps the string the header row draws and every mutation that moves the count
   refreshes it (msg__set_total_badge). */
static inline const char *msg_unread_badge(const MsgStore *st) { return st->totalBadge; }

/* The labels. Static storage, fixed spellings, so the GUI draws a state without
   formatting anything - and so the four states are named in exactly one place. */
static inline const char *msg_presence_label(uint8_t presence) {
    switch (presence) {
    case MSG_PRESENCE_AWAY:    return "away";
    case MSG_PRESENCE_OFFLINE: return "offline";
    default:                   return "online";
    }
}
static inline const char *msg_delivery_label(uint8_t deliv) {
    switch (deliv) {
    case MSG_DELIV_SENDING: return "sending";
    case MSG_DELIV_FAILED:  return "failed";
    case MSG_DELIV_OFFLINE: return "queued";
    default:                return "sent";
    }
}

/* The notification volume as text, in ten-percent steps. Static storage, because
   the GUI formats nothing. */
static inline const char *msg_volume_label(float v) {
    static const char *const STEPS[11] = {
        "Off", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%"
    };
    int step = (int)(v * 10.0f + 0.5f);
    if (step < 0)  step = 0;
    if (step > 10) step = 10;
    return STEPS[step];
}

/* The worst delivery state anywhere in a conversation, which is what the composer
   banner reports: FAILED outranks OFFLINE, and everything else is silence. Returns
   MSG_DELIV_SENT when there is nothing to say. */
static inline uint8_t msg_worst_delivery(const MsgStore *st, int convo) {
    if (convo < 0 || convo >= st->convCount)
        return MSG_DELIV_SENT;
    const MsgConversation *c = &st->conv[convo];
    uint8_t worst = MSG_DELIV_SENT;
    for (int i = 0; i < c->count; i++) {
        uint8_t d = c->items[i].deliv;
        if (d == MSG_DELIV_FAILED)
            return MSG_DELIV_FAILED;
        if (d == MSG_DELIV_OFFLINE)
            worst = MSG_DELIV_OFFLINE;
    }
    return worst;
}

#ifdef MESSENGER_BACKEND_IMPLEMENTATION

#include <string.h>

/* The non-inline API is declared AND defined only under IMPLEMENTATION, so a TU
   that needs just the types and queries never gets a bare `static` prototype.
   Forward declarations first, for the mutual reference in step(). */
MSGDEF void msg_memzero(void *p, size_t n);
MSGDEF void msg_store_seed(MsgStore *st, unsigned seed);
MSGDEF void msg_store_step(MsgStore *st, float dt);   /* dt <= 0 => no-op (freeze) */
MSGDEF int  msg_send_text(MsgStore *st, int convo, const char *text, int len);
MSGDEF void msg_mark_read(MsgStore *st, int convo);
MSGDEF void msg_inject_incoming(MsgStore *st, int convo);
MSGDEF int  msg_retry_failed(MsgStore *st, int convo);   /* -> messages requeued */
MSGDEF int  msg_format_clock(uint32_t secondsOfDay, char *out, int cap);  /* -> "HH:MM" */

MSGDEF void msg_memzero(void *p, size_t n) { memset(p, 0, n); }

/* xorshift32 - deterministic, and used only at seed time, so no frame pays for
   it. */
static uint32_t msg__rng_next(MsgStore *st) {
    uint32_t x = st->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    st->rng = x;
    return x;
}
static uint32_t msg__rng_range(MsgStore *st, uint32_t lo, uint32_t hi) {
    return lo + msg__rng_next(st) % (hi - lo + 1u);
}

MSGDEF int msg_format_clock(uint32_t s, char *out, int cap) {
    if (cap < 6)
        return 0;
    unsigned hh = (s / 3600u) % 24u, mm = (s / 60u) % 60u;
    out[0] = (char)('0' + hh / 10u);
    out[1] = (char)('0' + hh % 10u);
    out[2] = ':';
    out[3] = (char)('0' + mm / 10u);
    out[4] = (char)('0' + mm % 10u);
    out[5] = '\0';
    return 5;
}

/* Seed corpus - immutable, ASCII/Latin-1 only (the bundled face is Latin-1; a
   non-Latin-1 byte renders blank AND shifts run widths, breaking determinism). */
static const char *const MSG__NAMES[] = {
    "Ada Lovelace", "Grace Hopper", "Alan Turing", "Katherine J.",
    "Linus T.", "Margaret H.", "Dennis R.", "Barbara L.",
    "Ken Thompson", "Radia P.", "Guido v. R.", "Anita B.",
    "Design Team", "RayClay CI", "Release Bot", "Mom",
};
static const char *const MSG__INITIALS[] = {
    "AL", "GH", "AT", "KJ", "LT", "MH", "DR", "BL",
    "KT", "RP", "GR", "AB", "DT", "CI", "RB", "MO",
};
/* Presence per CONTACT, not per row: it is a property of the person, so it does not
   shuffle when the list reorders. A chat list where everyone is online reads as
   generated, which is the same tell the all-identical timestamps were. */
static const uint8_t MSG__PRESENCE[] = {
    MSG_PRESENCE_ONLINE,  MSG_PRESENCE_ONLINE,  MSG_PRESENCE_AWAY,    MSG_PRESENCE_OFFLINE,
    MSG_PRESENCE_ONLINE,  MSG_PRESENCE_OFFLINE, MSG_PRESENCE_AWAY,    MSG_PRESENCE_ONLINE,
    MSG_PRESENCE_OFFLINE, MSG_PRESENCE_ONLINE,  MSG_PRESENCE_AWAY,    MSG_PRESENCE_OFFLINE,
    MSG_PRESENCE_ONLINE,  MSG_PRESENCE_ONLINE,  MSG_PRESENCE_AWAY,    MSG_PRESENCE_OFFLINE,
};
static const uint32_t MSG__ACCENTS[] = {
    0x6366f1, 0x10b981, 0xf59e0b, 0xec4899, 0x8b5cf6, 0x06b6d4, 0xef4444, 0x84cc16,
    0x3b82f6, 0xf97316, 0x14b8a6, 0xa855f7, 0x64748b, 0x22c55e, 0x0ea5e9, 0xe11d48,
};
/* The short line a list row shows. It is also the LAST message of that thread
   (below), so the row and the transcript can never disagree - a row that reads one
   thing while the thread under it ends with another is the fastest way to tell a
   reader the data was made up. */
static const char *const MSG__PREVIEWS[] = {
    "See you at the standup!", "The build is green.", "Ship it when ready.",
    "Nice work on the demo.", "Can you review the PR?", "Lunch at noon?",
    "Pushed the fix.", "Thanks, that helps.", "On my way.", "Let's sync later.",
    "Deploy succeeded.", "Call me back.",
};
/* Thread bodies of varied length: some wrap to two lines, some are a single word,
   because a transcript of evenly sized paragraphs does not look like a chat. */
static const char *const MSG__BODIES[] = {
    "Hey! Are you free to look at the new dashboard this afternoon?",
    "Sure - give me ten minutes to finish this pass.",
    "The search field filters the list as you type now.",
    "Nice.",
    "One source builds the desktop window and the web page.",
    "Does it still feel quick with sixty conversations open?",
    "Yes, the list stays smooth all the way down the backlog.",
    "Great, that was the risk.",
    "I pushed the branch so you can click through it yourself.",
    "Let me pull it.",
    "The attach sheet sends at full size or reduced, your choice.",
    "Good, I always wanted that.",
    "Sending you the mockups now.",
    "Got them. The narrow layout is the one I would ship.",
    "That is exactly what we wanted.",
    "Then let us put it in front of people this week.",
};

static void msg__set_badge(MsgConversation *c) {
    if (c->unread == 0)
        c->badge[0] = '\0';
    else if (c->unread < 10) {
        c->badge[0] = (char)('0' + c->unread);
        c->badge[1] = '\0';
    } else {
        c->badge[0] = '9'; c->badge[1] = '+'; c->badge[2] = '\0';
    }
}

/* The aggregate unread badge, "" / "1".."99" / "99+". Written by hand rather than
   by snprintf: the backend is libc-light on purpose and the GUI must never format. */
static void msg__set_total_badge(MsgStore *st) {
    int total = msg_unread_total(st);
    if (total <= 0) {
        st->totalBadge[0] = '\0';
    } else if (total < 10) {
        st->totalBadge[0] = (char)('0' + total);
        st->totalBadge[1] = '\0';
    } else if (total < 100) {
        st->totalBadge[0] = (char)('0' + total / 10);
        st->totalBadge[1] = (char)('0' + total % 10);
        st->totalBadge[2] = '\0';
    } else {
        st->totalBadge[0] = '9'; st->totalBadge[1] = '9';
        st->totalBadge[2] = '+'; st->totalBadge[3] = '\0';
    }
}

/* Put one message into the outbox. Full outbox = the oldest entry is resolved as
   SENT and its slot reused, so a send is never refused for want of a slot. */
static void msg__outbox_push(MsgStore *st, int convo, int idx) {
    if (st->outCount >= MSG_MAX_OUTBOX) {
        st->conv[st->outConv[0]].items[st->outIdx[0]].deliv = MSG_DELIV_SENT;
        for (int i = 1; i < st->outCount; i++) {
            st->outConv[i - 1] = st->outConv[i];
            st->outIdx [i - 1] = st->outIdx [i];
            st->outAge [i - 1] = st->outAge [i];
        }
        st->outCount--;
    }
    st->outConv[st->outCount] = convo;
    st->outIdx [st->outCount] = idx;
    st->outAge [st->outCount] = 0.0f;
    st->outCount++;
}

/* Copy a fixed-cap, NUL-terminated preview of the last message into the sidebar row. */
static void msg__set_preview(MsgConversation *c, const char *text) {
    size_t pl = strlen(text);
    if (pl >= sizeof c->preview)
        pl = sizeof c->preview - 1;
    memcpy(c->preview, text, pl);
    c->preview[pl] = '\0';
}

/* Append a message to a conversation, clamped to capacity. Returns its index or
   -1 if the conversation is full. `clock` is seconds-since-midnight. */
static int msg__append(MsgConversation *c, const char *text, int len,
                       MsgDir dir, uint32_t clock) {
    if (c->count >= MSG_MAX_MSGS_PER_CONV)
        return -1;
    MsgMessage *m = &c->items[c->count];
    m->text    = text;
    m->textLen = (uint16_t)len;
    m->author  = 0;
    m->clock   = clock;
    m->kind    = MSG_KIND_TEXT;
    m->dir     = (uint8_t)dir;
    msg_format_clock(clock, m->ts, sizeof m->ts);
    c->lastClock = clock;
    memcpy(c->lastTs, m->ts, sizeof c->lastTs);
    return c->count++;
}

MSGDEF void msg_store_seed(MsgStore *st, unsigned seed) {
    msg_memzero(st, sizeof *st);
    st->rng      = seed ? seed : 0x9e3779b9u;
    st->nowClock = MSG_EPOCH_SECONDS;
    st->accum    = 0.0f;

    const int nNames  = (int)(sizeof MSG__NAMES  / sizeof *MSG__NAMES);
    const int nBodies = (int)(sizeof MSG__BODIES / sizeof *MSG__BODIES);
    const int nPrev   = (int)(sizeof MSG__PREVIEWS / sizeof *MSG__PREVIEWS);

    st->convCount = 12;
    for (int i = 0; i < st->convCount; i++) {
        MsgConversation *c = &st->conv[i];
        int who = i % nNames;
        /* fixed-cap copies (no <string.h> reach from the caller; done here) */
        size_t nl = strlen(MSG__NAMES[who]);
        if (nl >= sizeof c->name) nl = sizeof c->name - 1;
        memcpy(c->name, MSG__NAMES[who], nl);
        c->name[nl] = '\0';
        c->initials[0] = MSG__INITIALS[who][0];
        c->initials[1] = MSG__INITIALS[who][1];
        c->initials[2] = '\0';
        c->accent   = MSG__ACCENTS[who];
        c->presence = MSG__PRESENCE[who];

        c->unread = (uint16_t)((i == 0) ? 0 : msg__rng_range(st, 0, (i % 3 == 0) ? 12 : 4));
        msg__set_badge(c);

        /* The first conversation is the open thread with a full backlog. */
        int backlog = (i == 0) ? 48 : (int)msg__rng_range(st, 2, 6);
        if (backlog > MSG_MAX_MSGS_PER_CONV) backlog = MSG_MAX_MSGS_PER_CONV;
        /* EACH CONVERSATION ENDS AT ITS OWN LAST-ACTIVITY TIME, because a chat
           list whose twelve rows all read the same minute is the loudest tell
           there is that the data was generated. A real client is ordered by
           recency, so conversation 0 is newest and the rest walk backwards about
           27 minutes at a time, with a deterministic jitter that draws no rng. */
        uint32_t endClock = MSG_EPOCH_SECONDS
                          - (uint32_t)i * 1607u
                          - (uint32_t)((i * 37u) % 11u) * 60u;
        uint32_t clock = endClock - (uint32_t)backlog * 73u;  /* spaced ~1.2 min */
        for (int m = 0; m < backlog; m++) {
            /* The thread ENDS on the line the sidebar row shows. */
            const char *body = (m == backlog - 1) ? MSG__PREVIEWS[i % nPrev]
                                                  : MSG__BODIES[(i * 7 + m) % nBodies];
            MsgDir dir = (m & 1) ? MSG_DIR_OUTGOING : MSG_DIR_INCOMING;
            int idx = msg__append(c, body, (int)strlen(body), dir, clock);
            /* THE OPEN THREAD CARRIES THE TWO TERMINAL DELIVERY STATES, because
               both sit in a real backlog until the sender acts. SENDING is
               transient and is not seeded. */
            if (i == 0 && dir == MSG_DIR_OUTGOING && idx >= 0) {
                if (m == 5)
                    c->items[idx].deliv = MSG_DELIV_FAILED;
                else if (m == 9)
                    c->items[idx].deliv = MSG_DELIV_OFFLINE;
            }
            clock += 73u;
        }
        msg__set_preview(c, MSG__PREVIEWS[i % nPrev]);
    }
    msg__set_total_badge(st);
    /* Nothing is scheduled to arrive on the clock: an incoming message is an
       event, so msg_inject_incoming is called at the moment one arrives. */
}

MSGDEF void msg_store_step(MsgStore *st, float dt) {
    if (dt <= 0.0f)                      /* the freeze: a strict no-op */
        return;
    st->accum += dt;
    while (st->accum >= 1.0f) {          /* advance whole sim-seconds deterministically */
        st->accum -= 1.0f;
        st->nowClock++;                 /* drives the timestamp on any message sent now */
    }
    /* Every in-flight send ages on the SAME injected dt as the clock, so "sending"
       resolves on simulated time and freezes exactly when the app does. Walked back
       to front so removing a resolved entry cannot skip the next one. */
    for (int i = st->outCount - 1; i >= 0; i--) {
        st->outAge[i] += dt;
        if (st->outAge[i] < MSG_SEND_SECONDS)
            continue;
        st->conv[st->outConv[i]].items[st->outIdx[i]].deliv = MSG_DELIV_SENT;
        for (int j = i + 1; j < st->outCount; j++) {
            st->outConv[j - 1] = st->outConv[j];
            st->outIdx [j - 1] = st->outIdx [j];
            st->outAge [j - 1] = st->outAge [j];
        }
        st->outCount--;
    }
}

MSGDEF int msg_send_text(MsgStore *st, int convo, const char *text, int len) {
    if (convo < 0 || convo >= st->convCount || len <= 0)
        return -1;
    if (st->textUsed + len + 1 > MSG_TEXT_POOL_BYTES)
        return -1;
    char *dst = &st->textPool[st->textUsed];
    memcpy(dst, text, (size_t)len);
    dst[len] = '\0';
    st->textUsed += len + 1;
    int idx = msg__append(&st->conv[convo], dst, len, MSG_DIR_OUTGOING, st->nowClock);
    if (idx >= 0) {
        /* A sent message is IN FLIGHT, not delivered: it enters the thread marked
           SENDING and the outbox resolves it on the injected clock. */
        st->conv[convo].items[idx].deliv = MSG_DELIV_SENDING;
        msg__outbox_push(st, convo, idx);
    }
    msg__set_preview(&st->conv[convo], dst);   /* keep the sidebar preview == last message */
    return idx;
}

MSGDEF void msg_mark_read(MsgStore *st, int convo) {
    if (convo < 0 || convo >= st->convCount)
        return;
    st->conv[convo].unread = 0;
    msg__set_badge(&st->conv[convo]);
    msg__set_total_badge(st);
}

MSGDEF void msg_inject_incoming(MsgStore *st, int convo) {
    if (convo < 0 || convo >= st->convCount)
        return;
    static const char INCOMING[] = "Just saw the trend row land - looks great.";
    MsgConversation *c = &st->conv[convo];
    msg__append(c, INCOMING, (int)(sizeof INCOMING - 1), MSG_DIR_INCOMING, st->nowClock);
    msg__set_preview(c, INCOMING);
    c->unread++;
    msg__set_badge(c);
    msg__set_total_badge(st);
}

/* Put every stalled message in a conversation back in flight - what the composer's
   failure banner does. Returns how many were requeued (0 = nothing was stalled). */
MSGDEF int msg_retry_failed(MsgStore *st, int convo) {
    if (convo < 0 || convo >= st->convCount)
        return 0;
    MsgConversation *c = &st->conv[convo];
    int requeued = 0;
    for (int i = 0; i < c->count; i++) {
        if (c->items[i].deliv != MSG_DELIV_FAILED && c->items[i].deliv != MSG_DELIV_OFFLINE)
            continue;
        c->items[i].deliv = MSG_DELIV_SENDING;
        msg__outbox_push(st, convo, i);
        requeued++;
    }
    return requeued;
}

#endif /* MESSENGER_BACKEND_IMPLEMENTATION */
#endif /* MESSENGER_BACKEND_H */
