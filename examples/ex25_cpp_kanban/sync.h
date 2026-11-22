/*
    sync.h - the board on a REST server: rcFetch, JSON and one event stream.

    Sync is off until someone sets a URL in the Sync dialog and presses Connect, so
    a fresh run opens no socket and sends nothing. From that click on:

      GET    /api/board        the lists and cards, and the version they reflect
      POST   /api/cards        a new card (the same for /api/lists)
      PATCH  /api/cards/<id>   a changed card: one body shape, the whole card
      DELETE /api/cards/<id>   a deleted one
      GET    /api/events       server-sent events: one per change anyone makes

    THE RULES THIS FILE KEEPS, each one a contract of rcFetch:
      - A request starts only from an event: a click, a drop, a resync. Every
        rcFetch call is a new request, so none sits in the layout unconditionally.
      - sync_poll() runs at the top of every frame, before anything is drawn. It
        reads each request once, and when one has ended it applies the answer and
        frees it. The vectors move there, where no string is on loan to a frame.
      - What the app keeps is copied out: a response's text is RayClay's only until
        rcFetchFree, and an event's data only to the end of the frame that read it.
      - A change is optimistic: the board changes at once, the request goes out, and
        a failure puts the card back and says why. The undo is simply a copy of the
        Card from before the change, which C++ makes easy.

    A NEW CARD HAS TWO NAMES. It goes on the board at once under a temporary id below 0, and
    the POST's answer names it. Until then nothing can be asked of the server about it, so an
    edit or a delete waits for that answer, and the event stream waits too: the echo of the
    POST can arrive first, and the answer must be what turns the temporary card into the real
    one. A new list has no such stage. Cards could be dropped on it, so it appears when the
    server answers.

    LIVE SYNC. Once the board arrives the app opens one event stream with .reconnect,
    as EventSource does, naming the board's version as its Last-Event-ID, so a change
    made between the two is not missed: the server answers an older id with a resync
    and the app fetches the board again. Every event and every card carries the
    store's version, and a card is replaced only by a newer version of itself, so the
    echo of the app's own change changes nothing. Two windows on one serve.py show
    each other's drags this way.
*/
#ifndef KANBAN_SYNC_H
#define KANBAN_SYNC_H

#include "rayclay.h"
#include "board.h"

constexpr int LOG_MAX = 64;

/* The status the top bar and the dialog show. */
enum SyncState { SYNC_OFF = 0, SYNC_CONNECTING, SYNC_LIVE, SYNC_RECONNECTING, SYNC_OFFLINE };
static const char *const SYNC_WORD[] = { "Sync off", "Connecting", "Live", "Reconnecting",
                                         "Offline" };

enum CallKind { CALL_BOARD, CALL_NEW_CARD, CALL_CARD, CALL_DROP_CARD, CALL_NEW_LIST, CALL_LIST,
                CALL_DROP_LIST };

/* The dialog's "Next request" choice: serve.py reads it from that request's query. */
static const char *const KNOB_NAME[]  = { "Normal", "Slow (2 s)", "Fails (500)" };
static const char *const KNOB_QUERY[] = { "", "?delay=2000", "?fail=500" };

/* One request in flight, and what a failure puts back. */
struct Call {
    RC_Fetch token = 0;
    CallKind kind = CALL_BOARD;
    int      id = 0;                /* the card or list it is about; 0 for the board */
    int      logRow = 0;
    bool     edited = false;        /* a create: the card changed while the POST flew */
    bool     dropped = false;       /* a create: the card was deleted while the POST flew */
    Card     undoCard;
    List     undoList;
    std::vector<Card> undoCards;    /* a deleted list's cards */
};

/* One row of the request log. An event stream keeps a row of its own at the top. With sync
   off the log stays alive: a change writes the row it would have sent, marked local. */
struct LogRow {
    RC_Fetch      token = 0;
    int           seq = 0;          /* 0 = an empty row */
    bool          isLocal = false;
    const char   *method = "";
    std::string   path;
    RC_FetchState state = RC_FETCH_PENDING;
    int           status = 0;
    double        startedAt = 0.0, endedAt = 0.0;
    unsigned long long bytes = 0;
    int           events = 0;
    std::string   sent, body, note;
};

/* An event from the stream, waiting for a create's answer. */
struct HeldEvent {
    std::string name, data;
};

/* A temporary id the server has answered, for the app to follow. */
struct NewId {
    int from, to;
};

struct Sync {
    char        url[200] = "";      /* the dialog's field: empty means off */
    std::string base;               /* what requests go to: the field, tidied */
    bool        on = false;
    bool        live = false;       /* this change goes to the server: the synced board's */
    std::vector<Call> calls;
    std::vector<HeldEvent> held;
    std::vector<NewId> newIds;
    std::array<LogRow, LOG_MAX> log;
    int         logHead = 0, logCount = 0, logSeq = 0;
    int         picked = 0;         /* the seq of the row the dialog shows; 0 = none */
    LogRow      streamRow;
    RC_Fetch    stream = 0;
    bool        streamWasOpen = false;
    bool        wantStream = false;
    int         knob = 0;
    long long   version = 0;        /* the newest change the board reflects */
    int         lastTemp = 0;       /* the last temporary id handed out: -1, -2, ... */
    std::string error;              /* why the last Connect failed */
    std::string notice;             /* one sentence the app's snackbar shows */
};

/* A body being written. rcFetch copies it before it returns, so one buffer serves
   every request; rcJsonWriterFinish returns NULL for a body that does not fit. */
static char g_json[64 * 1024];

/* ---- Words for what came back ---- */

/* Why a request did not succeed, in words: RayClay's reason when no response came,
   else the server's {"error": ...}, else its status. */
static inline std::string why_failed(RC_App *app, const RC_FetchResult &r) {
    if (r.state != RC_FETCH_DONE)
        return r.message ? r.message : "The request was stopped.";
    RC_Json     doc = rcJsonParse(r.text, r.length);
    std::string why = rcJsonString(doc, "error", "");
    rcJsonFree(doc);
    if (why.empty())
        why = rcFormat(rcAppArena(app), "The server answered HTTP %d.", r.status).chars;
    return why;
}

/* ---- The wire format of a card and a list ---- */

/* A date goes out as toISOString writes it, and comes back through rcTimeParse. */
static inline void write_date(RC_JsonWriter *w, const char *key, int day) {
    char text[32];
    if (day && rcTimeFormatUtc(text, sizeof text, unix_of_day(day)))
        rcJsonWriteString(w, key, text);
    else
        rcJsonWriteNull(w, key);
}

static inline int read_date(RC_Json v, const char *key) {
    const char *text = rcJsonString(v, key, "");
    double seconds = 0.0;
    if (!text[0] || !rcTimeParse(text, 0, &seconds))
        return 0;
    const int day = day_of_unix(seconds);
    return day >= 1 && day <= LAST_DAY ? day : 0;
}

/* A field this client has no use for still travels whole: a PATCH carries the entire card,
   so `meta`, which a server or another tool attaches, must go back as it came. It is kept
   as text, because a parsed value lives only as long as its document, and rcJsonWriteValue
   puts it in a body without the app ever looking inside. */
static inline void write_meta(RC_JsonWriter *w, const std::string &meta) {
    if (meta.empty())
        return;
    RC_Json doc = rcJsonParse(meta.c_str(), meta.size());
    rcJsonWriteValue(w, "meta", doc);
    rcJsonFree(doc);
}

static inline std::string read_meta(RC_Json v) {
    static char text[4096];
    RC_Json meta = rcJsonGet(v, "meta");
    if (rcJsonType(meta) != RC_JSON_OBJECT)
        return std::string();
    RC_JsonWriter w = rcJsonWriter(text, sizeof text);
    rcJsonWriteValue(&w, nullptr, meta);
    const char *written = rcJsonWriterFinish(&w);
    return written ? written : "";
}

static inline void write_card(RC_JsonWriter *w, const Card &c) {
    rcJsonBeginObject(w, nullptr);
    rcJsonWriteInt64(w, "list", c.list);
    rcJsonWriteNumber(w, "pos", c.pos);
    rcJsonWriteString(w, "title", c.title.c_str());
    rcJsonWriteString(w, "description", c.description.c_str());
    rcJsonWriteInt64(w, "labels", c.labels);
    rcJsonWriteInt64(w, "members", c.members);
    write_date(w, "due", c.due);
    rcJsonWriteBool(w, "dueDone", c.dueDone);
    rcJsonWriteInt64(w, "priority", c.priority);
    rcJsonWriteInt64(w, "cover", c.cover);
    rcJsonBeginArray(w, "checklist");
    for (const CheckItem &item : c.checklist) {
        rcJsonBeginObject(w, nullptr);
        rcJsonWriteString(w, "text", item.text.c_str());
        rcJsonWriteBool(w, "done", item.done);
        rcJsonEnd(w);
    }
    rcJsonEnd(w);
    rcJsonBeginArray(w, "comments");
    for (const Comment &note : c.comments) {
        rcJsonBeginObject(w, nullptr);
        rcJsonWriteInt64(w, "author", note.author);
        rcJsonWriteString(w, "text", note.text.c_str());
        write_date(w, "at", note.day);
        rcJsonEnd(w);
    }
    rcJsonEnd(w);
    write_meta(w, c.meta);
    rcJsonEnd(w);
}

/* Every integer from the wire is checked before it is narrowed. */
static inline int wire_int(RC_Json v, const char *key, int low, int high, int fallback) {
    long long n = rcJsonInt64(v, key, fallback);
    return n >= low && n <= high ? static_cast<int>(n) : fallback;
}

static inline bool read_card(RC_Json v, Card *c) {
    c->id = wire_int(v, "id", 1, 0x7fffffff, 0);
    if (rcJsonType(v) != RC_JSON_OBJECT || !c->id)
        return false;
    c->list        = wire_int(v, "list", 1, 0x7fffffff, 0);
    c->pos         = rcJsonNumber(v, "pos", 0.0);
    c->title       = rcJsonString(v, "title", "");
    c->description = rcJsonString(v, "description", "");
    c->labels      = static_cast<unsigned>(wire_int(v, "labels", 0, (1 << LABEL_COUNT) - 1, 0));
    c->members     = static_cast<unsigned>(wire_int(v, "members", 0, (1 << MEMBER_COUNT) - 1, 0));
    c->due         = read_date(v, "due");
    c->dueDone     = rcJsonBool(v, "dueDone", false);
    c->priority    = wire_int(v, "priority", 0, PRIORITY_COUNT - 1, 0);
    c->cover       = wire_int(v, "cover", -1, LABEL_COUNT - 1, -1);
    c->version     = rcJsonInt64(v, "version", 0);
    c->meta        = read_meta(v);
    c->checklist.clear();
    RC_Json items = rcJsonGet(v, "checklist");
    for (int i = 0, n = rcJsonCount(items); i < n; i++) {
        RC_Json item = rcJsonAt(items, i);
        c->checklist.push_back({ rcJsonString(item, "text", ""), rcJsonBool(item, "done", false) });
    }
    c->comments.clear();
    RC_Json notes = rcJsonGet(v, "comments");
    for (int i = 0, n = rcJsonCount(notes); i < n; i++) {
        RC_Json note = rcJsonAt(notes, i);
        c->comments.push_back({ wire_int(note, "author", 0, MEMBER_COUNT - 1, ME),
                                rcJsonString(note, "text", ""), read_date(note, "at") });
    }
    return true;
}

/* The wire carries no checklist keys, so the server's copy of a card takes the keys of the
   one it replaces, place by place: the answer to a change leaves each row, and the focus on
   it, where it was. */
static inline void keep_check_keys(const Card &was, Card &got) {
    for (size_t i = 0; i < got.checklist.size() && i < was.checklist.size(); i++)
        got.checklist[i].key = was.checklist[i].key;
}

static inline bool read_list(RC_Json v, List *l) {
    l->id = wire_int(v, "id", 1, 0x7fffffff, 0);
    if (rcJsonType(v) != RC_JSON_OBJECT || !l->id)
        return false;
    l->pos     = rcJsonNumber(v, "pos", 0.0);
    l->title   = rcJsonString(v, "title", "");
    l->version = rcJsonInt64(v, "version", 0);
    return true;
}

/* ---- The request log ---- */

/* A body as the log shows it: a space after each comma and colon between tokens, the
   separators the server's answers already carry, so the text wraps between members rather
   than inside a word. The bytes sent are not touched. */
static inline std::string spaced_json(const char *json) {
    std::string out;
    bool quoted = false;
    for (const char *p = json; *p; p++) {
        out += *p;
        if (quoted && *p == '\\' && p[1])
            out += *++p;
        else if (*p == '"')
            quoted = !quoted;
        else if (!quoted && (*p == ',' || *p == ':'))
            out += ' ';
    }
    return out;
}

/* @p json is the body the request sends, or NULL when it sends none. */
static inline void row_start(RC_App *app, Sync &s, LogRow &e, RC_Fetch token, const char *method,
                             const std::string &path, const char *json) {
    if (s.picked == e.seq)
        s.picked = 0;
    e = LogRow{};
    e.token     = token;
    e.seq       = ++s.logSeq;
    e.method    = method;
    e.path      = path;
    e.startedAt = rcAppTime(app);
    if (json)
        e.sent = spaced_json(json);
}

static inline void row_end(RC_App *app, LogRow &e, const RC_FetchResult &r,
                           const std::string &why) {
    e.state   = r.state;
    e.status  = r.status;
    e.bytes   = r.received;
    e.endedAt = rcAppTime(app);
    e.note    = why;
    if (r.text)
        e.body.assign(r.text, r.length < 2000 ? r.length : 2000);
}

/* ---- Making requests ---- */

/* Send one request, or with sync off only log it. NULL then, as there is nothing to wait
   for; a change keeps its undo in the Call this returns. */
static inline Call *send_request(RC_App *app, Sync &s, CallKind kind, const char *method,
                                 const char *path, const char *json, int id) {
    const int logRow = s.logHead;
    s.logHead = (s.logHead + 1) % LOG_MAX;
    if (s.logCount < LOG_MAX)
        s.logCount++;
    if (!s.live && kind != CALL_BOARD) {
        LogRow &row = s.log[static_cast<size_t>(logRow)];
        row_start(app, s, row, 0, method, path, json);
        row.isLocal = true;
        return nullptr;
    }
    const char *url = rcFormat(rcAppArena(app), "%s%s%s", s.base.c_str(), path,
                               KNOB_QUERY[s.knob]).chars;
    Call call;
    s.knob      = 0;
    call.token  = rcFetch(url, .method = method, .json = json, .timeoutSeconds = 10);
    call.kind   = kind;
    call.id     = id;
    call.logRow = logRow;
    row_start(app, s, s.log[static_cast<size_t>(logRow)], call.token, method, path, json);
    s.calls.push_back(std::move(call));
    return &s.calls.back();
}

static inline const char *card_path(RC_App *app, int id) {
    return rcFormat(rcAppArena(app), "/api/cards/%d", id).chars;
}

static inline const char *list_path(RC_App *app, int id) {
    return rcFormat(rcAppArena(app), "/api/lists/%d", id).chars;
}

/* A row's request ends: what it ended with goes on its row, unless the ring has handed that
   row to a newer request since. Every way a request ends comes here, so no row is left
   reading "..." for good. */
static inline void call_end_row(RC_App *app, Sync &s, const Call &c, const RC_FetchResult &r,
                                const std::string &why) {
    LogRow &row = s.log[static_cast<size_t>(c.logRow)];
    if (row.token == c.token)
        row_end(app, row, r, why);
}

/* GET the board. A newer ask replaces one still in flight. */
static inline void load_board(RC_App *app, Sync &s) {
    for (auto at = s.calls.begin(); at != s.calls.end(); ++at) {
        if (at->kind == CALL_BOARD) {
            rcFetchCancel(at->token);
            call_end_row(app, s, *at, rcFetchResult(at->token), "replaced by a newer one");
            rcFetchFree(at->token);
            s.calls.erase(at);
            break;
        }
    }
    send_request(app, s, CALL_BOARD, "GET", "/api/board", nullptr, 0);
}

/* The event stream: a GET read as server-sent events that reconnects by itself. The
   server comments every 15 s, so 45 s of silence means it is gone. The header list
   is static: rcFetch copies it, and an option set to a local array's address is a
   warning on MSVC. */
static inline void open_stream(RC_App *app, Sync &s) {
    static const char *hdr[2];
    RC_Arena   *mem = rcAppArena(app);
    const char *url = rcFormat(mem, "%s/api/events", s.base.c_str()).chars;
    hdr[0]   = rcFormat(mem, "Last-Event-ID: %lld", s.version).chars;
    s.stream = rcFetch(url, .headers = hdr, .stream = true, .idleSeconds = 45, .reconnect = true);
    row_start(app, s, s.streamRow, s.stream, "SSE", "/api/events", nullptr);
    s.streamRow.sent = hdr[0];
}

static inline void close_stream(RC_App *app, Sync &s) {
    if (!s.stream)
        return;
    rcFetchCancel(s.stream);
    row_end(app, s.streamRow, rcFetchResult(s.stream), "closed");
    s.streamRow.token = 0;
    rcFetchFree(s.stream);
    s.stream = 0;
}

/* The field without the white space around it or a final '/', with http:// in front
   when it names no scheme. */
static inline void set_base(Sync &s) {
    std::string tidy = s.url;
    while (!tidy.empty() && tidy.front() == ' ')
        tidy.erase(0, 1);
    while (!tidy.empty() && (tidy.back() == ' ' || tidy.back() == '/'))
        tidy.pop_back();
    s.base = tidy.find("://") == std::string::npos ? "http://" + tidy : tidy;
}

/* Leave the stream and open another, as a dropped connection would: the status reads
   Reconnecting until the new one is in. The dialog's button shows this on demand. */
static inline void drop_stream(RC_App *app, Sync &s) {
    close_stream(app, s);
    open_stream(app, s);
}

/* The press of Connect. What the board knew is kept on screen until the server's
   copy arrives, and the stream opens once it has. */
static inline void sync_connect(RC_App *app, Sync &s) {
    set_base(s);
    close_stream(app, s);
    s.on = true;
    s.error.clear();
    s.streamWasOpen = false;
    s.version = 0;
    load_board(app, s);
}

/* The press of Disconnect. The board stays as it is, on this device. */
static inline void sync_disconnect(RC_App *app, Sync &s) {
    for (Call &c : s.calls) {
        rcFetchCancel(c.token);
        call_end_row(app, s, c, rcFetchResult(c.token), "canceled");
        rcFetchFree(c.token);
    }
    s.calls.clear();
    s.held.clear();
    close_stream(app, s);
    s.on = false;
}

/* The state of the status dot. */
static inline SyncState sync_state(const Sync &s) {
    if (!s.on)
        return s.error.empty() ? SYNC_OFF : SYNC_OFFLINE;
    if (!s.stream)
        return SYNC_CONNECTING;
    RC_FetchState st = rcFetchResult(s.stream).state;
    if (st == RC_FETCH_OPEN)
        return SYNC_LIVE;
    if (st == RC_FETCH_PENDING)
        return s.streamWasOpen ? SYNC_RECONNECTING : SYNC_CONNECTING;
    return SYNC_OFFLINE;
}

/* ---- The changes the app sends ---- */

/* The body a writer holds, or NULL with a word for the snackbar. */
static inline const char *body_of(Sync &s, RC_JsonWriter *w) {
    const char *body = rcJsonWriterFinish(w);
    if (!body)
        s.notice = "That change is too long to send.";
    return body;
}

/* The POST creating this temporary card, while it flies. */
static inline Call *create_call(Sync &s, int temp) {
    for (Call &c : s.calls)
        if (c.kind == CALL_NEW_CARD && c.id == temp)
            return &c;
    return nullptr;
}

static inline void sync_new_card(RC_App *app, Sync &s, const Card &c) {
    RC_JsonWriter w = rcJsonWriter(g_json, sizeof g_json);
    write_card(&w, c);
    if (const char *body = body_of(s, &w))
        send_request(app, s, CALL_NEW_CARD, "POST", "/api/cards", body, c.id);
}

/* @p before is the card as it was: what a failure puts back. */
static inline void sync_card(RC_App *app, Sync &s, const Card &before, const Card &after) {
    if (s.live && after.id < 0) {
        if (Call *create = create_call(s, after.id))
            create->edited = true;
        return;
    }
    RC_JsonWriter w = rcJsonWriter(g_json, sizeof g_json);
    write_card(&w, after);
    if (const char *body = body_of(s, &w)) {
        Call *call = send_request(app, s, CALL_CARD, "PATCH", card_path(app, after.id), body,
                                  after.id);
        if (call)
            call->undoCard = before;
    }
}

static inline void sync_drop_card(RC_App *app, Sync &s, const Card &before) {
    if (s.live && before.id < 0) {
        if (Call *create = create_call(s, before.id))
            create->dropped = true;
        return;
    }
    Call *call = send_request(app, s, CALL_DROP_CARD, "DELETE", card_path(app, before.id), nullptr,
                              before.id);
    if (call)
        call->undoCard = before;
}

static inline void write_list(RC_JsonWriter *w, const List &l) {
    rcJsonBeginObject(w, nullptr);
    rcJsonWriteString(w, "title", l.title.c_str());
    rcJsonWriteNumber(w, "pos", l.pos);
    rcJsonEnd(w);
}

static inline void sync_new_list(RC_App *app, Sync &s, const List &l) {
    RC_JsonWriter w = rcJsonWriter(g_json, sizeof g_json);
    write_list(&w, l);
    if (const char *body = body_of(s, &w))
        send_request(app, s, CALL_NEW_LIST, "POST", "/api/lists", body, l.id);
}

static inline void sync_list(RC_App *app, Sync &s, const List &before, const List &after) {
    RC_JsonWriter w = rcJsonWriter(g_json, sizeof g_json);
    write_list(&w, after);
    if (const char *body = body_of(s, &w)) {
        Call *call = send_request(app, s, CALL_LIST, "PATCH", list_path(app, after.id), body,
                                  after.id);
        if (call)
            call->undoList = before;
    }
}

static inline void sync_drop_list(RC_App *app, Sync &s, const List &before,
                                  std::vector<Card> cards) {
    Call *call = send_request(app, s, CALL_DROP_LIST, "DELETE", list_path(app, before.id), nullptr,
                              before.id);
    if (call) {
        call->undoList  = before;
        call->undoCards = std::move(cards);
    }
}

/* ---- The answers ---- */

static inline bool card_call_is_on(const Sync &s, int id) {
    for (const Call &c : s.calls)
        if (c.id == id
            && (c.kind == CALL_NEW_CARD || c.kind == CALL_CARD || c.kind == CALL_DROP_CARD))
            return true;
    return false;
}

/* A whole board replaces what the app holds, except where the app knows better: a
   card with a change in flight keeps its own copy until that answer comes, and a
   create in flight is kept. A temporary card whose create is not in flight (Disconnect
   canceled it) goes with the rest of this device's copy: kept, its id could never be
   named, so nothing about it would ever reach the server. */
static inline void take_board(Sync &s, Board &b, RC_Json doc) {
    std::vector<List> lists;
    std::vector<Card> mine = std::move(b.cards);
    RC_Json jsonLists = rcJsonGet(doc, "lists");
    RC_Json jsonCards = rcJsonGet(doc, "cards");

    for (int i = 0, n = rcJsonCount(jsonLists); i < n; i++) {
        List l;
        if (read_list(rcJsonAt(jsonLists, i), &l))
            lists.push_back(std::move(l));
    }
    b.lists.clear();
    for (List &l : lists) {
        if (l.id >= b.nextList)
            b.nextList = l.id + 1;
        place_list(b, std::move(l));
    }
    b.cards.clear();
    for (int i = 0, n = rcJsonCount(jsonCards); i < n; i++) {
        Card c;
        if (!read_card(rcJsonAt(jsonCards, i), &c))
            continue;
        if (c.id >= b.nextCard)
            b.nextCard = c.id + 1;
        if (!card_call_is_on(s, c.id))
            place_card(b, std::move(c));
    }
    for (Card &c : mine)
        if (card_call_is_on(s, c.id))       /* a temporary card's create is CALL_NEW_CARD */
            place_card(b, std::move(c));
    s.version = rcJsonInt64(doc, "version", 0);
}

static inline void board_done(RC_App *app, Sync &s, Board &b, const RC_FetchResult &r,
                              const std::string &why) {
    if (r.state == RC_FETCH_DONE && r.ok) {
        RC_Json doc = rcJsonParse(r.text, r.length);
        if (rcJsonType(rcJsonGet(doc, "cards")) != RC_JSON_ARRAY) {
            s.error = "The answer is not a board. Is this the right address?";
        } else {
            take_board(s, b, doc);
            s.wantStream = true;
            s.notice = "Connected: " + b.name + " now lives on the server.";
        }
        rcJsonFree(doc);
    } else {
        s.error = why;
    }
    if (!s.error.empty()) {
        sync_disconnect(app, s);
        s.notice = "Could not connect. Your board stays on this device.";
    }
}

/* The server has named the card. What the user changed while the POST flew stays, and goes
   out now that there is an id to send it to; a card deleted meanwhile is deleted there too. */
static inline void new_card_done(RC_App *app, Sync &s, Board &b, const Call &call,
                                 const RC_FetchResult &r, const std::string &why) {
    Card *temp = find_card(b, call.id);
    if (r.state != RC_FETCH_DONE || !r.ok) {
        if (temp)
            remove_card(b, call.id);
        s.notice = "Could not add the card: " + why;
        return;
    }
    RC_Json doc = rcJsonParse(r.text, r.length);
    Card    got;
    if (read_card(doc, &got)) {
        s.live = true;
        if (call.dropped) {
            sync_drop_card(app, s, got);
        } else if (temp) {
            Card named = call.edited ? *temp : got;
            named.id      = got.id;
            named.version = got.version;
            named.meta    = got.meta;
            remove_card(b, call.id);
            s.newIds.push_back({ call.id, got.id });
            if (call.edited)
                sync_card(app, s, got, named);
            place_card(b, std::move(named));
        }
    }
    rcJsonFree(doc);
}

static inline void card_done(Sync &s, Board &b, const Call &call, const RC_FetchResult &r,
                             const std::string &why) {
    Card *mine = find_card(b, call.id);
    if (r.state == RC_FETCH_DONE && r.ok) {
        RC_Json doc = rcJsonParse(r.text, r.length);
        Card    got;
        if (mine && read_card(doc, &got) && got.version > mine->version) {
            keep_check_keys(*mine, got);
            remove_card(b, call.id);
            place_card(b, std::move(got));
        }
        rcJsonFree(doc);
        return;
    }
    if (r.state == RC_FETCH_DONE && r.status == 404) {
        remove_card(b, call.id);                     /* deleted elsewhere */
    } else if (mine && mine->version == call.undoCard.version) {
        remove_card(b, call.id);                     /* nothing newer came: put it back */
        place_card(b, call.undoCard);
    }
    s.notice = "Could not save the change: " + why;
}

static inline void drop_card_done(Sync &s, Board &b, const Call &call, const RC_FetchResult &r,
                                  const std::string &why) {
    if (r.state == RC_FETCH_DONE && (r.ok || r.status == 404))
        return;                                      /* gone either way */
    if (!find_card(b, call.id))
        place_card(b, call.undoCard);
    s.notice = "Could not delete the card: " + why;
}

static inline void new_list_done(Sync &s, Board &b, const RC_FetchResult &r,
                                 const std::string &why) {
    if (r.state != RC_FETCH_DONE || !r.ok) {
        s.notice = "Could not add the list: " + why;
        return;
    }
    RC_Json doc = rcJsonParse(r.text, r.length);
    List    got;
    if (read_list(doc, &got) && !find_list(b, got.id))     /* the stream may have brought it */
        place_list(b, std::move(got));
    rcJsonFree(doc);
}

static inline void list_done(Sync &s, Board &b, const Call &call, const RC_FetchResult &r,
                             const std::string &why) {
    if (r.state == RC_FETCH_DONE && r.ok)
        return;
    if (find_list(b, call.id)) {
        remove_list(b, call.id);
        place_list(b, call.undoList);
    }
    s.notice = "Could not save the list: " + why;
}

static inline void drop_list_done(Sync &s, Board &b, const Call &call, const RC_FetchResult &r,
                                  const std::string &why) {
    if (r.state == RC_FETCH_DONE && (r.ok || r.status == 404))
        return;
    if (!find_list(b, call.id)) {
        place_list(b, call.undoList);
        for (const Card &c : call.undoCards)
            if (!find_card(b, c.id))
                place_card(b, c);
    }
    s.notice = "Could not delete the list: " + why;
}

/* ---- The event stream ---- */

/* A card from the stream replaces the app's copy only when it is newer. An update of a card
   the app lacks is one it has deleted, and so is a create whose DELETE is still out: those
   stay out. */
static inline void take_card_event(Sync &s, Board &b, Card got, bool create) {
    Card *mine = find_card(b, got.id);
    if (mine) {
        if (got.version > mine->version) {
            keep_check_keys(*mine, got);
            remove_card(b, got.id);
            place_card(b, std::move(got));
        }
        return;
    }
    if (create && !card_call_is_on(s, got.id))
        place_card(b, std::move(got));
}

static inline void take_list_event(Board &b, List got, bool create) {
    List *mine = find_list(b, got.id);
    if (mine) {
        if (got.version > mine->version) {
            remove_list(b, got.id);
            place_list(b, std::move(got));
        }
    } else if (create) {
        place_list(b, std::move(got));
    }
}

static inline void take_event(RC_App *app, Sync &s, Board &b, const std::string &name,
                              const std::string &data) {
    s.streamRow.events++;
    if (name == "resync") {
        s.streamRow.note = "resync: the board is fetched again";
        s.version = 0;
        load_board(app, s);
        return;
    }
    if (name != "card" && name != "list")
        return;
    RC_Json     doc     = rcJsonParse(data.c_str(), data.size());
    long long   version = rcJsonInt64(doc, "version", 0);
    std::string op      = rcJsonString(doc, "op", "");
    int         id      = wire_int(doc, "id", 1, 0x7fffffff, 0);

    if (version > s.version) {
        s.version = version;
        if (name == "card" && op == "delete") {
            remove_card(b, id);
        } else if (name == "list" && op == "delete") {
            remove_list(b, id);
            for (size_t i = b.cards.size(); i-- > 0;)
                if (b.cards[i].list == id)
                    remove_card(b, b.cards[i].id);
        } else if (name == "card") {
            Card got;
            if (read_card(rcJsonGet(doc, "card"), &got))
                take_card_event(s, b, std::move(got), op == "create");
        } else {
            List got;
            if (read_list(rcJsonGet(doc, "list"), &got))
                take_list_event(b, std::move(got), op == "create");
        }
    }
    s.streamRow.note = rcFormat(rcAppArena(app), "%s %s, version %lld", name.c_str(), op.c_str(),
                                version).chars;
    rcJsonFree(doc);
}

/* While a create is out the stream's events wait, in order, so its answer names the card
   before its echo looks for it. Then the echo finds its card there at the same version. */
static inline void read_stream(RC_App *app, Sync &s, Board &b) {
    if (!s.stream)
        return;
    bool creating = false;
    for (const Call &c : s.calls)
        creating = creating || c.kind == CALL_NEW_CARD;
    if (!creating) {
        std::vector<HeldEvent> held = std::move(s.held);
        s.held.clear();
        for (const HeldEvent &e : held)
            take_event(app, s, b, e.name, e.data);
    }
    for (int i = 0; i < 32; i++) {          /* bounded: a flood cannot hold up a frame */
        RC_FetchMessages m = rcFetchMessages(s.stream, RC_FETCH_FRAMING_SSE);
        if (!m.count)
            break;
        for (int k = 0; k < m.count; k++) {
            HeldEvent e = { m.items[k].event ? m.items[k].event : "",
                            std::string(m.items[k].data, m.items[k].length) };
            if (creating)
                s.held.push_back(std::move(e));
            else
                take_event(app, s, b, e.name, e.data);
        }
    }
    RC_FetchResult r = rcFetchResult(s.stream);
    if (r.state == RC_FETCH_OPEN)
        s.streamWasOpen = true;
    s.streamRow.state  = r.state;
    s.streamRow.status = r.status;
    s.streamRow.bytes  = r.received;
    if (r.state != RC_FETCH_PENDING && r.state != RC_FETCH_OPEN && s.streamRow.endedAt == 0.0) {
        s.streamRow.endedAt = rcAppTime(app);
        if (r.message)
            s.streamRow.note = r.message;
    }
}

/* At the top of every frame, before anything is drawn. */
static inline void sync_poll(RC_App *app, Sync &s, Board &b) {
    for (size_t i = 0; i < s.calls.size();) {
        RC_FetchResult r = rcFetchResult(s.calls[i].token);
        if (r.state == RC_FETCH_PENDING) {
            i++;
            continue;
        }
        Call call = std::move(s.calls[i]);
        s.calls.erase(s.calls.begin() + static_cast<long>(i));
        const bool  ok  = r.state == RC_FETCH_DONE && r.ok;
        std::string why = ok ? std::string() : why_failed(app, r);
        call_end_row(app, s, call, r, why);
        switch (call.kind) {
        case CALL_BOARD:     board_done(app, s, b, r, why); break;
        case CALL_NEW_CARD:  new_card_done(app, s, b, call, r, why); break;
        case CALL_CARD:      card_done(s, b, call, r, why); break;
        case CALL_DROP_CARD: drop_card_done(s, b, call, r, why); break;
        case CALL_NEW_LIST:  new_list_done(s, b, r, why); break;
        case CALL_LIST:      list_done(s, b, call, r, why); break;
        case CALL_DROP_LIST: drop_list_done(s, b, call, r, why); break;
        }
        rcFetchFree(call.token);
    }
    read_stream(app, s, b);
    /* The board came in: open the stream unless one lives. */
    if (s.wantStream) {
        s.wantStream = false;
        RC_FetchState st = rcFetchResult(s.stream).state;
        if (st != RC_FETCH_PENDING && st != RC_FETCH_OPEN) {
            close_stream(app, s);
            open_stream(app, s);
        }
    }
}

#endif /* KANBAN_SYNC_H */
