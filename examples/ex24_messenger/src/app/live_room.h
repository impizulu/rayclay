/*
    live_room.h - the socket: every message the app sends, and what it does with each one
    it gets

    One rcWebSocket per channel, its events read once a frame, what they carry copied into
    the store (messenger_backend.h), and the back-off that brings a dropped room back. Only
    main.c includes this: the app core draws a room from AppState.live and asks for a change
    by setting live.ask, so a scripted run links no socket at all.

    THE RULES THIS FILE KEEPS, each one a contract of rcWebSocket:
      - A socket opens only from an event: a click, a timer that has come due, a channel
        picked. Every rcWebSocket call is a new connection, so none sits in the layout, and
        the first frame opens none: the demo room is what the app starts in.
      - live_poll() runs at the top of every frame, before anything is drawn, and is the ONE
        place a socket's events are read (two readers would split the messages). Its CLOSE
        is read with the state too, because a token RayClay could not make has no events.
      - What the app keeps is copied out: an event's data is RayClay's only to the end of
        the frame that read it, and the state's strings until the socket is freed.
      - A send is checked: rcWebSocketSendText returns false, with nothing sent, unless the
        socket is OPEN, the text is valid UTF-8, its bufferedAmount stays within
        .maxBufferedBytes and RayClay's 64 MiB network budget has room (live_send_json).

    RECONNECTING. A room that drops without a clean close (1006: the network, a server that
    went away) comes back by itself, after 1 s, then 2, 4, 8, 16 and at most 30, each with a
    little jitter so a room full of apps does not reconnect in step; the count starts again
    once a socket has stayed open for LIVE_STABLE_AFTER. A clean close is the server's decision
    and is kept, with its code and reason on screen, except the three codes that mean "come
    back": 1001 (going away), 1012 (restarting) and 1013 (try again later). Nor are these
    retried, because the next attempt meets the same answer: an address RayClay refused
    (INVALID), UNAVAILABLE from a socket that had events (no backend, no CA certificates for
    wss), a certificate (SECURITY), a server that breaks the protocol or refuses the
    subprotocol (PROTOCOL), and an upgrade refused with a 4xx.

    YOUR OWN LINES. A line shows at once, marked as on its way; the server's echo (this
    run's client tag and the same nonce) marks it delivered. A socket that closes first
    leaves it "Not delivered", with Send again: it may still have arrived, and the room's
    history says so once it is back, with the same tag and nonce, so the welcome drops the
    copy it already holds.
*/
#ifndef LIVE_ROOM_H
#define LIVE_ROOM_H

#include "messenger_app.h"

/* Where the app looks on a native host: serve.py listens here and prints it. An IPv4
   address, as serve.py binds, so no lookup of "localhost" tries IPv6 first. A page asks
   its own server instead, by a relative URL, which is how a page serve.py serves reaches
   it (empty Server field). */
#define LIVE_DEFAULT_SERVER "ws://127.0.0.1:8789"

/* The subprotocol offered, which serve.py selects. rcWebSocket copies the list; a static
   one avoids MSVC's warning C4221 about an option set to a local array's address. */
#define LIVE_PROTOCOL "rayclay.chat.v1"
static const char *const LIVE_PROTOCOLS[] = { LIVE_PROTOCOL, NULL };

#define LIVE_CONNECT_SECONDS 10.0
#define LIVE_BACKOFF_FIRST   1.0
#define LIVE_BACKOFF_MAX     30.0
#define LIVE_STABLE_AFTER    10.0
#define LIVE_BURST_BYTES     (64 * 1024)
#define LIVE_TEXT_CAP        2001    /* serve.py's 500 characters, at four bytes each */
#define LIVE_JSON_CAP        (6 * LIVE_TEXT_CAP + 128)

/* The socket's own bookkeeping, which nothing but this file reads. */
typedef struct {
    RC_WebSocket ws;        /* the room's socket; 0 = none                          */
    RC_WebSocket leaving;   /* the last room's, closing; 0 = none                   */
    int          leftConv;
    double       openedAt, retryAt;   /* rcAppTime                                  */
    uint32_t     seed;      /* the back-off's jitter, never 0                       */
    char         client[12];/* this run's tag: with a nonce it names one of your lines */
    int          youId;
    char         you[LIVE_NAME_CAP];
    int          nonce;     /* the last one handed out                              */
    bool         first;     /* the socket on its way is the first one since Connect */
} LiveSocket;

static LiveSocket g_sock;

/* A message being written. rcWebSocketSendText copies it before it returns, so one buffer
   serves every send. LIVE_JSON_CAP holds the longest at its worst escaping, yet every
   message is still checked: rcJsonWriterFinish returns NULL for one that does not fit. */
static char g_json[LIVE_JSON_CAP];

/* The burst: zeros, sent as binary, which serve.py counts and drops. */
static const unsigned char g_burst[LIVE_BURST_BYTES] = { 0 };

static inline void live_event(RC_App *app, AppState *st, LiveTone tone, const char *text) {
    LiveRoom  *lv = &st->live;
    LiveEvent *e  = &lv->events[lv->eventHead];

    lv->eventHead = (lv->eventHead + 1) % LIVE_EVENT_MAX;
    if (lv->eventCount < LIVE_EVENT_MAX)
        lv->eventCount++;
    e->at   = rcAppTime(app);
    e->tone = tone;
    rcStrCopy(e->text, text, sizeof e->text);
}

static inline const char *live_error_name(RC_WebSocketError e) {
    static const char *const NAME[] = {
        "", "NETWORK", "TIMEOUT", "SECURITY", "REFUSED", "PROTOCOL", "TOO_LARGE", "INVALID",
        "UNAVAILABLE"
    };

    return (int)e >= 0 && (int)e < 9 ? NAME[e] : "";
}

/* An id from the wire, or 0 when it is not one: serve.py's are positive and fit 31 bits;
   the int64 is checked before it is narrowed, as every integer from the wire must be. */
static inline int live_wire_id(int64_t v) {
    return v > 0 && v <= 0x7fffffff ? (int)v : 0;
}

/* A number in [0, 1) for the jitter: xorshift32 on the run's seed. */
static inline double live_jitter(void) {
    uint32_t x = g_sock.seed ? g_sock.seed : 0x9E3779B9u;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_sock.seed = x;
    return (double)(x >> 8) / 16777216.0;
}

/* The run's own randomness, taken once: the jitter's seed and the client tag. A clock would
   not do: a server that restarts drops every app in the same second, and apps seeded alike
   reconnect in step, the very thing the jitter is there to break. rcRandomBytes asks the
   operating system's generator, on every host, so each run starts out of step with the
   others. The seed is never 0, where xorshift32 would stay; a generator that cannot answer
   leaves zeros and returns false. */
static inline void live_seed_run(RC_App *app) {
    if (!rcRandomBytes(&g_sock.seed, sizeof g_sock.seed) || !g_sock.seed)
        g_sock.seed = 0x9E3779B9u;
    live_jitter();
    rcStrCopy(g_sock.client, rcFormat(rcAppArena(app), "%08x", (unsigned)g_sock.seed).chars,
              sizeof g_sock.client);
}

/* Which address a socket takes is asked at run time: a relative URL passes rcFetchCheck on
   the web only, so a page asks its own server and an app asks serve.py's, with no #ifdef. */
static inline void live_init(RC_App *app, AppState *st) {
    st->live.relative = rcFetchCheck("room/general") == NULL;
    rcStrCopy(st->live.server, st->live.relative ? "" : LIVE_DEFAULT_SERVER,
              sizeof st->live.server);
    live_seed_run(app);
}

/* ---- Where the socket goes ---- */

/* The base of every URL: the field without the white space around it or a final '/', with
   ws:// in front when it names no scheme (http:// and https:// are taken as ws:// and
   wss://, as a browser takes them). Empty is this page's own server where relative URLs
   resolve (the web), else the default. */
static inline const char *live_room_url(RC_App *app, const AppState *st, int conv) {
    char tidy[LIVE_SERVER_CAP];
    const char *src = st->live.server;
    const char *room = st->store.conv[conv].name;
    bool scheme = false;

    while (*src == ' ')
        src++;
    rcStrCopy(tidy, src[0] || st->live.relative ? src : LIVE_DEFAULT_SERVER, sizeof tidy);
    int n = rcStringFromCStr(tidy).length;

    while (n > 0 && (tidy[n - 1] == '/' || tidy[n - 1] == ' '))
        tidy[--n] = '\0';
    for (int i = 0; tidy[i] && !scheme; i++)
        scheme = tidy[i] == ':' && tidy[i + 1] == '/' && tidy[i + 2] == '/';
    if (!tidy[0])
        return rcFormat(rcAppArena(app), "room/%s", room).chars;
    return rcFormat(rcAppArena(app), "%s%s/room/%s", scheme ? "" : "ws://", tidy, room).chars;
}

/* ---- Opening and letting go ---- */

/* Open the room. The options are copied: the caps, the subprotocol, and a connect deadline
   (an OPEN socket has no idle one). */
static inline void live_open(RC_App *app, AppState *st, int conv) {
    LiveRoom   *lv  = &st->live;
    const char *url = live_room_url(app, st, conv);

    rcStrCopy(lv->url, url, sizeof lv->url);
    g_sock.ws = rcWebSocket(url, .protocols = LIVE_PROTOCOLS,
                            .connectTimeoutSeconds = LIVE_CONNECT_SECONDS,
                            .maxMessageBytes = LIVE_CAP_MESSAGE, .maxUnreadBytes = LIVE_CAP_UNREAD,
                            .maxBufferedBytes = LIVE_CAP_BUFFERED);
    lv->link     = LIVE_CONNECTING;
    lv->room     = conv;
    lv->status   = 0;
    lv->buffered = 0;
    lv->protocol[0] = '\0';
    st->store.liveConv = conv;
    /* The demo's people leave this room: nobody simulated is typing in it, or about to answer.
       The other chats keep theirs, as a messenger's other chats go on while you are in one. */
    st->store.conv[conv].typing[0]  = '\0';
    st->store.conv[conv].typingLeft = 0.0f;
    st->store.conv[conv].replyIn    = 0.0f;
    st->store.conv[conv].replyQuote = 0;
    live_event(app, st, LIVE_TONE_INFO, rcFormat(rcAppArena(app), "Connecting to %s", url).chars);
}

/* Your lines still on their way in a room will get no echo: not delivered. */
static inline void live_fail_pending(AppState *st, int conv) {
    MsgConversation *c = &st->store.conv[conv];

    for (int i = 0; i < c->count; i++)
        if (c->items[i].dir == MSG_DIR_OUTGOING && c->items[i].deliv == MSG_DELIV_SENDING
            && c->items[i].nonce)
            c->items[i].deliv = MSG_DELIV_FAILED;
}

/* Let the room's socket go. An OPEN one closes with 1000 and @p reason, and is kept as
   `leaving` until it reads CLOSED, so the Events list shows how it ended. Where the stack
   still reads after its own Close (curl, WinHTTP, the Apple arm), echoes on their way land
   in their room too; the browser drops what comes once it is not OPEN, so there a line
   still on its way reads Not delivered, until the room's history shows it arrived. Any
   other socket is freed at once. */
static inline void live_let_go(RC_App *app, AppState *st, const char *reason) {
    LiveRoom *lv = &st->live;

    if (!g_sock.ws)
        return;
    if (g_sock.leaving) {
        live_fail_pending(st, g_sock.leftConv);
        rcWebSocketFree(g_sock.leaving);
        g_sock.leaving = 0;
    }
    if (rcWebSocketState(g_sock.ws).readyState == RC_WEBSOCKET_OPEN
        && rcWebSocketClose(g_sock.ws, 1000, reason)) {
        g_sock.leaving  = g_sock.ws;
        g_sock.leftConv = lv->room;
        live_event(app, st, LIVE_TONE_INFO,
                   rcFormat(rcAppArena(app), "Closing #%s: 1000, %s",
                            st->store.conv[lv->room].name, reason).chars);
    } else {
        live_fail_pending(st, lv->room);
        rcWebSocketFree(g_sock.ws);
    }
    g_sock.ws  = 0;
    lv->people = 0;
}

/* Back to the demo: no socket, and the channels on their simulated lines again. */
static inline void live_take_demo(RC_App *app, AppState *st, const char *reason) {
    live_let_go(app, st, reason);
    st->live.link = LIVE_DEMO;
    st->store.liveConv = -1;
    msg_seed_channels(&st->store);
}

/* ---- Sending ---- */

/* Send one JSON object the writer holds. NULL once it is sent, else why not, in words, read
   from the state that refused it. The writer's text is always UTF-8. */
static inline const char *live_send_json(RC_App *app, AppState *st, RC_JsonWriter *w) {
    RC_Arena         *a = rcAppArena(app);
    RC_WebSocketState s;
    const char       *text;
    size_t            need;

    rcJsonEnd(w);
    text = rcJsonWriterFinish(w);
    if (!text)
        return "Not sent: the message does not fit the app's buffer.";
    if (rcWebSocketSendText(g_sock.ws, text, w->len)) {
        st->live.msgsOut++;
        st->live.bytesOut += w->len;
        return NULL;
    }
    s    = rcWebSocketState(g_sock.ws);
    need = w->len < 64 ? 64 : w->len;
    if (s.readyState != RC_WEBSOCKET_OPEN)
        return "Not sent: the room is not open.";
    if (s.bufferedAmount + need > LIVE_CAP_BUFFERED)
        return rcFormat(a, "Not sent: %zu bytes are still waiting to go out.",
                        s.bufferedAmount).chars;
    return "Not sent: RayClay's 64 MiB network budget is spent.";
}

/* Your queued lines go out here, one a frame's walk: the composer put them in the thread,
   so a line shows at once whether the socket takes it now or refuses it. */
static inline void live_send_queued(RC_App *app, AppState *st) {
    MsgConversation *c = &st->store.conv[st->live.room];

    if (st->live.link != LIVE_OPEN)
        return;
    for (int i = 0; i < c->count; i++) {
        MsgMessage *m = &c->items[i];

        if (m->dir != MSG_DIR_OUTGOING || m->deliv != MSG_DELIV_QUEUED)
            continue;
        RC_JsonWriter w = rcJsonWriter(g_json, sizeof g_json);
        const char   *why;

        rcJsonBeginObject(&w, NULL);
        rcJsonWriteString(&w, "type", "say");
        rcJsonWriteString(&w, "text", m->text);
        rcJsonWriteInt64(&w, "nonce", g_sock.nonce + 1);
        why = live_send_json(app, st, &w);
        if (why) {
            m->deliv = MSG_DELIV_FAILED;
            live_event(app, st, LIVE_TONE_WARN, why);
        } else {
            m->nonce = ++g_sock.nonce;
            m->deliv = MSG_DELIV_SENDING;
        }
    }
}

/* What the Simulate menu asks: serve.py does each on purpose (its header says what). */
static inline void live_simulate(RC_App *app, AppState *st, const char *what) {
    RC_JsonWriter w = rcJsonWriter(g_json, sizeof g_json);
    const char   *why;

    rcJsonBeginObject(&w, NULL);
    rcJsonWriteString(&w, "type", "simulate");
    rcJsonWriteString(&w, "what", what);
    why = live_send_json(app, st, &w);
    live_event(app, st, why ? LIVE_TONE_WARN : LIVE_TONE_INFO,
               why ? why : rcFormat(rcAppArena(app), "Asked the server to %s", what).chars);
}

/* 2 MB at once, in 64 KB binary messages, all in one frame. bufferedAmount falls only
   between frames, as the bytes go out, so from an empty queue 15 fit under the 1 MB cap (each
   send is counted with the few dozen bytes RayClay keeps beside its 64 KB) and 17 are refused
   on the spot, each refusal counted, whether the server reads or not. What differs is after:
   a server that reads takes those 15 in the next frames; stall it first, and what the
   operating system's socket buffers cannot hold waits in bufferedAmount (the meter) until
   the stall ends. */
static inline void live_burst(RC_App *app, AppState *st) {
    LiveRoom *lv = &st->live;

    lv->burstSent = lv->burstRefused = 0;
    for (int i = 0; i < LIVE_BURST_COUNT; i++) {
        if (rcWebSocketSendBinary(g_sock.ws, g_burst, sizeof g_burst)) {
            lv->burstSent++;
            lv->msgsOut++;
            lv->bytesOut += sizeof g_burst;
        } else {
            lv->burstRefused++;
        }
    }
    /* The cap stopping the rest is the burst working, so it reads as news, not a warning. */
    live_event(app, st, lv->burstRefused ? LIVE_TONE_INFO : LIVE_TONE_OK,
               rcFormat(rcAppArena(app), "Burst: %d of %d sent, %d stopped at the send cap",
                        lv->burstSent, LIVE_BURST_COUNT, lv->burstRefused).chars);
}

static inline void live_rename(RC_App *app, AppState *st) {
    RC_JsonWriter w = rcJsonWriter(g_json, sizeof g_json);
    const char   *why;

    rcJsonBeginObject(&w, NULL);
    rcJsonWriteString(&w, "type", "name");
    rcJsonWriteString(&w, "name", st->myName);
    why = live_send_json(app, st, &w);
    if (why)
        live_event(app, st, LIVE_TONE_WARN, why);
}

/* ---- What the server says ---- */

static inline void live_take_rooms(AppState *st, RC_Json list) {
    const int n = rcJsonCount(list);

    for (int i = 0; i < n; i++) {
        const RC_Json v = rcJsonAt(list, i);
        const char *name = rcJsonString(v, "name", "");
        const int64_t k = rcJsonInt64(v, "people", 0);

        for (int r = 0; r < 3; r++)
            if (msg_same_text(name, st->store.conv[C_GENERAL + r].name))
                st->live.here[r] = k >= 0 && k <= LIVE_PEOPLE_MAX ? (int)k : 0;
    }
}

/* Whether a line from the wire is one of yours: this run's client tag. */
static inline bool live_is_mine(RC_Json v) {
    return g_sock.client[0] && msg_same_text(rcJsonString(v, "client", ""), g_sock.client);
}

/* A wire time on the app's own clock, which the demo chats beside the room run on: as long
   before the app's now as it was before the wall clock's. main.c sets that now from the wall
   clock each frame, so a line reads the local time its server stamped (serve.py: Unix seconds).
   Returns seconds since midnight, and @p day gets how many days back, for the day's pill. */
static inline uint32_t live_wire_clock(const AppState *st, int64_t at, uint8_t *day) {
    /* Checked as every integer from the wire is: a time before 1970 is 1970, so the
       subtraction below cannot overflow (an int64 near INT64_MIN would). */
    const int64_t when = at > 0 ? at : 0;
    const int64_t t    = (int64_t)st->store.nowClock - ((int64_t)rcUnixTimeSeconds() - when);
    const int64_t date = (t >= 0 ? t : t - 86399) / 86400;
    const int64_t back = (int64_t)(st->store.nowClock / 86400u) - date;

    *day = (uint8_t)(back <= 0 ? 0 : (back < 7 ? back : 7));
    return (uint32_t)(t - date * 86400);
}

/* The one door a line from the wire takes into the store, so it is where the wire's strings
   meet the app's caps: serve.py's names and lines fit them, but any server may send up to
   the 256 KB message cap, and the list's preview, a thread's pill and the export each
   format a line into a fixed arena. rcStrCopy cuts between characters. */
static inline MsgMessage *live_add_line(AppState *st, int conv, const char *who, const char *text,
                                        int64_t at, MsgKind kind, MsgDir dir) {
    static char name[LIVE_NAME_CAP], line[LIVE_TEXT_CAP];
    uint8_t day;
    const uint32_t clock = live_wire_clock(st, at, &day);

    rcStrCopy(name, who, sizeof name);
    rcStrCopy(line, text, sizeof line);
    return msg_live_line(&st->store, conv, name, line, clock, day, kind, dir);
}

/* A line from the wire: an echo, a broadcast or the history. One of yours keeps its nonce,
   so a line not delivered can be matched to it. */
static inline void live_take_line(AppState *st, int conv, RC_Json v) {
    const bool mine = live_is_mine(v);
    MsgMessage *m = live_add_line(st, conv, rcJsonString(v, "name", "?"),
                                  rcJsonString(v, "text", ""),
                                  rcJsonInt64(v, "at", 0), MSG_KIND_TEXT,
                                  mine ? MSG_DIR_OUTGOING : MSG_DIR_INCOMING);

    if (m) {
        const int64_t nonce = rcJsonInt64(v, "nonce", 0);

        m->wire  = live_wire_id(rcJsonInt64(v, "id", 0));
        m->nonce = mine ? live_wire_id(nonce) : 0;
    }
}

/* The welcome: who you are, who is here, and the room's history, which takes the place of
   the lines the room had, except your own not delivered, kept at the end to send again,
   unless the history holds the line (this run's tag and its nonce): then it did arrive. Not
   delivered is FAILED, and QUEUED too: Send again on a line while the room is away queues it
   for the open room, and dropping it here would lose the line the user asked to resend. A
   line with no nonce never went out (refused at its send, or queued as the room dropped), so
   the history cannot hold it: it is kept as it is. */
static inline void live_take_welcome(RC_App *app, AppState *st, int conv, RC_Json doc) {
    enum { KEEP_MAX = 16 };
    static char keepText[KEEP_MAX][LIVE_TEXT_CAP];   /* 32 KB: static, not on the stack */
    int keepNonce[KEEP_MAX];
    uint8_t keepDeliv[KEEP_MAX];
    int kept = 0;
    LiveRoom *lv = &st->live;
    MsgConversation *c = &st->store.conv[conv];
    const RC_Json history = rcJsonGet(doc, "history"), people = rcJsonGet(doc, "people");

    for (int i = 0; i < c->count && kept < KEEP_MAX; i++) {
        const MsgMessage *m = &c->items[i];

        if (m->dir == MSG_DIR_OUTGOING
            && (m->deliv == MSG_DELIV_FAILED || m->deliv == MSG_DELIV_QUEUED)) {
            rcStrCopy(keepText[kept], m->text, sizeof keepText[0]);
            keepDeliv[kept] = m->deliv;
            keepNonce[kept++] = m->nonce;
        }
    }
    msg_clear_thread(&st->store, conv);
    for (int i = 0, n = rcJsonCount(history); i < n; i++)
        live_take_line(st, conv, rcJsonAt(history, i));
    for (int k = 0; k < kept; k++) {
        bool arrived = false;

        for (int i = 0; keepNonce[k] && i < c->count; i++)
            arrived = arrived || (c->items[i].dir == MSG_DIR_OUTGOING
                                  && c->items[i].nonce == keepNonce[k]);
        if (!arrived) {
            MsgMessage *m = live_add_line(st, conv, g_sock.you, keepText[k],
                                          (int64_t)rcUnixTimeSeconds(), MSG_KIND_TEXT,
                                          MSG_DIR_OUTGOING);

            if (m) {
                m->deliv = keepDeliv[k];
                m->nonce = keepNonce[k];
            }
        }
    }
    lv->people = 0;
    for (int i = 0, n = rcJsonCount(people); i < n && lv->people < LIVE_PEOPLE_MAX; i++) {
        const RC_Json v = rcJsonAt(people, i);
        const int id = live_wire_id(rcJsonInt64(v, "id", 0));

        if (id) {
            lv->who[lv->people].id = id;
            rcStrCopy(lv->who[lv->people].name, rcJsonString(v, "name", "?"), LIVE_NAME_CAP);
            lv->people++;
        }
    }
    g_sock.youId = live_wire_id(rcJsonInt64(doc, "you.id", 0));
    rcStrCopy(g_sock.you, rcJsonString(doc, "you.name", "You"), sizeof g_sock.you);
    rcStrCopy(st->myName, g_sock.you, sizeof st->myName);
    live_take_rooms(st, rcJsonGet(doc, "rooms"));
    lv->link  = LIVE_OPEN;
    lv->offline = false;
    live_event(app, st, LIVE_TONE_OK,
               rcFormat(rcAppArena(app), "Joined #%s as %s, %d here", c->name, g_sock.you,
                        lv->people).chars);
}

/* A line someone said. Your own echo (this run's tag, the nonce) marks your line delivered,
   even one already read as not delivered, and gives it the server's time, as every other line
   there has; a line the room already has (its history after a reconnect) is not added twice. */
static inline void live_take_said(AppState *st, int conv, RC_Json doc) {
    MsgConversation *c = &st->store.conv[conv];
    const int nonce = live_wire_id(rcJsonInt64(doc, "nonce", 0));
    const int wire  = live_wire_id(rcJsonInt64(doc, "id", 0));

    for (int i = 0; i < c->count; i++) {
        MsgMessage *m = &c->items[i];

        if (live_is_mine(doc) && nonce && m->nonce == nonce && m->dir == MSG_DIR_OUTGOING) {
            const int64_t at = rcJsonInt64(doc, "at", 0);

            m->wire  = wire;
            m->deliv = MSG_DELIV_READ;
            if (at) {
                uint8_t day;
                const uint32_t clock = live_wire_clock(st, at, &day);

                msg_line_set_time(&st->store, conv, m, clock, day);
            }
            return;
        }
        if (wire && m->wire == wire)
            return;
    }
    live_take_line(st, conv, doc);
    if (c->count)
        msg_arrived(&st->store, conv, &c->items[c->count - 1]);
}

/* Someone came, went or took a new name: the people list, and a line. A new name says the old
   one too, "Sam is now Ada", or the room could not tell who Ada was. */
static inline void live_take_person(RC_App *app, AppState *st, int conv, RC_Json doc,
                                    const char *what) {
    LiveRoom *lv = &st->live;
    const int id = live_wire_id(rcJsonInt64(doc, "person.id", 0));
    const char *name = rcJsonString(doc, "person.name", "?");
    const char *was = rcJsonString(doc, "was", "");
    const bool renamed = msg_same_text(what, "renamed");
    const int64_t when = rcJsonInt64(doc, "at", (int64_t)rcUnixTimeSeconds());
    int at = 0;

    if (!id)
        return;
    while (at < lv->people && lv->who[at].id != id)
        at++;
    if (msg_same_text(what, "left")) {
        for (int k = at; k + 1 < lv->people; k++)
            lv->who[k] = lv->who[k + 1];
        if (at < lv->people)
            lv->people--;
    } else if (at < LIVE_PEOPLE_MAX) {
        lv->who[at].id = id;
        rcStrCopy(lv->who[at].name, name, LIVE_NAME_CAP);
        if (at == lv->people)
            lv->people++;
    }
    if (id == g_sock.youId && renamed)
        rcStrCopy(g_sock.you, name, sizeof g_sock.you);
    if (renamed && was[0])
        live_add_line(st, conv, was, rcFormat(rcAppArena(app), "is now %s", name).chars, when,
                      MSG_KIND_EVENT, MSG_DIR_INCOMING);
    else
        live_add_line(st, conv, name, what, when, MSG_KIND_EVENT, MSG_DIR_INCOMING);
}

/* One text message from the server, a JSON object. @p shown is false for the room being
   left, whose late echoes still land but whose news does not. */
static inline void live_take_message(RC_App *app, AppState *st, int conv, const char *data,
                                     size_t length, bool shown) {
    const RC_Json doc = rcJsonParse(data, length);
    const char *type = rcJsonString(doc, "type", "");

    if (msg_same_text(type, "message")) {
        live_take_said(st, conv, doc);
    } else if (!shown) {
        /* the room is being left: nothing else it says matters now */
    } else if (msg_same_text(type, "welcome")) {
        live_take_welcome(app, st, conv, doc);
    } else if (msg_same_text(type, "joined")) {
        live_take_person(app, st, conv, doc, "joined");
    } else if (msg_same_text(type, "left")) {
        live_take_person(app, st, conv, doc, "left");
    } else if (msg_same_text(type, "renamed")) {
        live_take_person(app, st, conv, doc, "renamed");
    } else if (msg_same_text(type, "rooms")) {
        live_take_rooms(st, rcJsonGet(doc, "rooms"));
    } else if (msg_same_text(type, "sink")) {
        st->live.sunk = rcJsonUint64(doc, "bytes", st->live.sunk);
    } else if (msg_same_text(type, "error") || msg_same_text(type, "note")) {
        live_event(app, st, msg_same_text(type, "error") ? LIVE_TONE_WARN : LIVE_TONE_INFO,
                   rcFormat(rcAppArena(app), "The server: %s",
                            rcJsonString(doc, "text", "")).chars);
    } else {
        live_event(app, st, LIVE_TONE_WARN,
                   rcFormat(rcAppArena(app), "Not understood: a message of type \"%s\"%s%s", type,
                            rcJsonError(doc) ? ", " : "", rcJsonError(doc) ? rcJsonError(doc)
                                : "").chars);
    }
    rcJsonFree(doc);
}

/* ---- The socket's own events ---- */

/* OPEN: the handshake is done. The first message is the join, with your name and this run's
   tag. */
static inline void live_take_open(RC_App *app, AppState *st) {
    LiveRoom *lv = &st->live;
    RC_Arena *a = rcAppArena(app);
    const RC_WebSocketState s = rcWebSocketState(g_sock.ws);
    RC_JsonWriter w = rcJsonWriter(g_json, sizeof g_json);
    const char *why;

    lv->link        = LIVE_JOINING;
    g_sock.openedAt = rcAppTime(app);
    g_sock.first    = false;
    lv->status      = s.status;
    rcStrCopy(lv->protocol, s.protocol, sizeof lv->protocol);
    live_event(app, st, LIVE_TONE_OK,
               rcFormat(a, "Open: %s%s%s", lv->protocol[0] ? "subprotocol " : "no subprotocol",
                        lv->protocol,
                        lv->status ? rcFormat(a, ", HTTP %d", lv->status).chars : "").chars);
    rcJsonBeginObject(&w, NULL);
    rcJsonWriteString(&w, "type", "join");
    rcJsonWriteString(&w, "name", st->myName);
    rcJsonWriteString(&w, "client", g_sock.client);
    why = live_send_json(app, st, &w);
    if (why)
        live_event(app, st, LIVE_TONE_WARN, why);
}

/* Whether a close is worth another attempt (RECONNECTING above says why). @p fixed: the
   token had no events, so its UNAVAILABLE is RayClay out of sockets or memory for now. */
static inline bool live_comes_back(int code, bool clean, RC_WebSocketError error, int status,
                                   bool fixed) {
    if (code == 1001 || code == 1012 || code == 1013)
        return true;
    if (clean)
        return false;
    if (error == RC_WEBSOCKET_ERROR_UNAVAILABLE)
        return fixed;
    if (error == RC_WEBSOCKET_ERROR_INVALID || error == RC_WEBSOCKET_ERROR_SECURITY
        || error == RC_WEBSOCKET_ERROR_PROTOCOL)
        return false;
    return !(error == RC_WEBSOCKET_ERROR_REFUSED && status >= 400 && status < 500);
}

static inline void live_schedule_retry(RC_App *app, AppState *st) {
    LiveRoom *lv = &st->live;
    double wait = LIVE_BACKOFF_FIRST;

    for (int i = 0; i < lv->tries && wait < LIVE_BACKOFF_MAX; i++)
        wait *= 2.0;
    if (wait > LIVE_BACKOFF_MAX)
        wait = LIVE_BACKOFF_MAX;
    wait *= 0.8 + 0.4 * live_jitter();                 /* +-20 % */
    lv->tries++;
    lv->link = LIVE_WAITING;
    g_sock.retryAt = rcAppTime(app) + wait;
    live_event(app, st, LIVE_TONE_INFO,
               rcFormat(rcAppArena(app), "Reconnecting in %.1f s (attempt %d)", wait,
                        lv->tries).chars);
}

/* The first socket since Connect failed, so there is no room to come back to. Nothing
   answering is the common case (no serve.py running); the demo room stays, and the strip
   says what happened, in words, with Retry; an UNAVAILABLE shows RayClay's own reason. */
static inline void live_first_failed(RC_App *app, AppState *st, RC_WebSocketError e, int status,
                                     const char *why) {
    LiveRoom *lv = &st->live;
    RC_Arena *a = rcAppArena(app);
    const char *text;

    if (e == RC_WEBSOCKET_ERROR_UNAVAILABLE)
        text = rcFormat(a, "Live rooms cannot open here: %s.",
                        why ? why : "no reason given").chars;
    else if (e == RC_WEBSOCKET_ERROR_REFUSED)
        text = status ? rcFormat(a, "The chat server refused the connection (HTTP %d).",
                                 status).chars
                      : "The chat server refused the connection.";
    else if (e == RC_WEBSOCKET_ERROR_INVALID)
        text = "That address is not one a socket can open: ws:// or wss://, a host and a port.";
    else if (e == RC_WEBSOCKET_ERROR_PROTOCOL)
        text = "That server does not speak WebSocket as this app needs.";
    else if (e == RC_WEBSOCKET_ERROR_SECURITY)
        text = "The connection was not allowed. Check the certificate or the host's policy.";
    else
        text = rcFormat(a, "Could not reach the chat server at %s. Is serve.py running?",
                        lv->url).chars;
    rcStrCopy(lv->failText, text, sizeof lv->failText);
    lv->offline = true;
    lv->link = LIVE_DEMO;
    st->store.liveConv = -1;
}

/* CLOSE, read from the event or, for a token that has none (@p fixed), from the state: what
   the close was, copied before the socket is freed, and what follows. */
static inline void live_take_close(RC_App *app, AppState *st, int code, const char *reason,
                                   bool clean, bool fixed) {
    LiveRoom *lv = &st->live;
    RC_Arena *a = rcAppArena(app);
    const RC_WebSocketState s = rcWebSocketState(g_sock.ws);
    const bool comes = live_comes_back(code, clean, s.error, s.status, fixed);

    rcStrCopy(lv->closeText,
              rcFormat(a, "%d, %s%s%s%s%s", code, clean ? "clean" : "not clean",
                       reason[0] ? ", " : "", reason, s.message ? ": " : "",
                       s.message ? s.message : "").chars,
              sizeof lv->closeText);
    rcStrCopy(lv->closeWhy, reason, sizeof lv->closeWhy);
    lv->status = s.status;
    live_event(app, st, clean ? LIVE_TONE_INFO : LIVE_TONE_WARN,
               rcFormat(a, "Closed: %s%s%s%s", lv->closeText, s.error ? " (" : "",
                        live_error_name(s.error), s.error ? ")" : "").chars);
    live_fail_pending(st, lv->room);
    lv->people = 0;
    if (g_sock.first) {
        live_first_failed(app, st, s.error, s.status, s.message);
    } else {
        lv->link = LIVE_CLOSED;
        if (comes)
            live_schedule_retry(app, st);
    }
    rcWebSocketFree(g_sock.ws);
    g_sock.ws = 0;
}

/* The last room's socket, closing: what its stack still delivers lands in its room, its
   CLOSE is logged, and it is freed once it reads CLOSED. */
static inline void live_poll_leaving(RC_App *app, AppState *st) {
    RC_WebSocketEvent e;

    if (!g_sock.leaving)
        return;
    while (rcWebSocketNext(g_sock.leaving, &e)) {
        if (e.type == RC_WEBSOCKET_EVENT_MESSAGE && !e.binary)
            live_take_message(app, st, g_sock.leftConv, e.data, e.length, false);
        else if (e.type == RC_WEBSOCKET_EVENT_CLOSE)
            live_event(app, st, LIVE_TONE_INFO,
                       rcFormat(rcAppArena(app), "Left #%s: %d, %s%s%s",
                                st->store.conv[g_sock.leftConv].name, e.code,
                                e.wasClean ? "clean" : "not clean", e.reason[0] ? ", " : "",
                                e.reason).chars);
    }
    if (rcWebSocketState(g_sock.leaving).readyState == RC_WEBSOCKET_CLOSED) {
        live_fail_pending(st, g_sock.leftConv);
        rcWebSocketFree(g_sock.leaving);
        g_sock.leaving = 0;
    }
}

/* What the screen asked for last frame. */
static inline void live_serve_ask(RC_App *app, AppState *st) {
    LiveRoom *lv = &st->live;
    const LiveAsk ask = lv->ask;
    const int room = lv->askRoom;

    lv->ask = LIVE_ASK_NONE;
    switch (ask) {
    case LIVE_ASK_CONNECT:
        live_let_go(app, st, "Connecting again");
        lv->tries = 0;
        lv->offline = false;
        g_sock.first = lv->link == LIVE_DEMO;
        live_open(app, st, st->store.conv[room].kind == MSG_CONV_CHANNEL ? room : C_GENERAL);
        break;
    case LIVE_ASK_LEAVE:
        live_take_demo(app, st, "Back to the demo room");
        lv->offline = false;
        break;
    case LIVE_ASK_ROOM:
        /* The room keeps the lines it had, until its welcome brings the server's history. */
        live_let_go(app, st, "Switched rooms");
        lv->tries = 0;
        live_open(app, st, room);
        break;
    case LIVE_ASK_RENAME:
        live_rename(app, st);
        break;
    case LIVE_ASK_DROP:
        live_simulate(app, st, "drop");
        break;
    case LIVE_ASK_CLOSE:
        live_simulate(app, st, "close");
        break;
    case LIVE_ASK_RESTART:
        live_simulate(app, st, "restart");
        break;
    case LIVE_ASK_STALL:
        live_simulate(app, st, "stall");
        break;
    case LIVE_ASK_BURST:
        live_burst(app, st);
        break;
    default:
        break;
    }
}

/* At the top of every frame, before anything is drawn: what the screen asked, the events
   that came, then the state, then the clocks: the back-off's next attempt, the reset once a
   socket is stable, and the send buffer while it drains. RayClay bounds the events itself
   (at most 256 messages a socket a frame, the rest in the frames after, drawn with no wait),
   and a socket reads CLOSED only once every message before its CLOSE was handed over, so the
   state read after the loop sees CLOSED with the CLOSE already taken, unless the token is a
   fixed one, which has no events at all. */
static inline void live_poll(RC_App *app, AppState *st) {
    LiveRoom *lv = &st->live;
    RC_Window *win = rcAppMainWindow(app);
    const double now = rcAppTime(app);
    RC_WebSocketEvent e;

    live_serve_ask(app, st);
    live_poll_leaving(app, st);
    while (g_sock.ws && rcWebSocketNext(g_sock.ws, &e)) {
        switch (e.type) {
        case RC_WEBSOCKET_EVENT_OPEN:
            live_take_open(app, st);
            break;
        case RC_WEBSOCKET_EVENT_MESSAGE:
            lv->msgsIn++;
            lv->bytesIn += e.length;
            if (!e.binary)
                live_take_message(app, st, lv->room, e.data, e.length, true);
            break;
        case RC_WEBSOCKET_EVENT_CLOSE:
            live_take_close(app, st, e.code, e.reason, e.wasClean, false);
            break;
        default:
            break;     /* ERROR: the CLOSE after it carries the why */
        }
    }
    if (g_sock.ws) {
        const RC_WebSocketState s = rcWebSocketState(g_sock.ws);

        if (s.readyState == RC_WEBSOCKET_CLOSED) {
            live_take_close(app, st, s.closeCode, s.closeReason, s.wasClean,
                            true);   /* a fixed token */
        } else {
            lv->buffered = s.bufferedAmount;
            if (lv->buffered > lv->peak)
                lv->peak = lv->buffered;
            /* RayClay draws a frame when it reaches 0; these draw the meter on its way down. */
            if (lv->buffered)
                rcWindowRequestFrameAfter(win, 0.1);
        }
    }
    if (!g_sock.ws)
        lv->buffered = 0;
    if (lv->link == LIVE_WAITING) {
        const double left = g_sock.retryAt - now;
        /* The countdown shows whole seconds rounded up, so its next frame is where the shown
           number turns over: a second after this one would hold "2 s" past its time. */
        const double tick = left - (double)(int)left;

        lv->retryIn = left > 0.0 ? (float)left : 0.0f;
        if (left <= 0.0)
            live_open(app, st, lv->room);
        else
            rcWindowRequestFrameAfter(win, tick > 0.0 ? tick : 1.0);
    }
    if (lv->link == LIVE_OPEN && lv->tries) {
        if (now - g_sock.openedAt >= LIVE_STABLE_AFTER)
            lv->tries = 0;
        else
            rcWindowRequestFrameAfter(win, LIVE_STABLE_AFTER - (now - g_sock.openedAt));
    }
    live_send_queued(app, st);
}

#endif /* LIVE_ROOM_H */
