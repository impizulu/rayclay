/*
    main.c - RayClay Messenger: the runner, and the door to a live room

    The chats are the app (src/app/messenger_app.c); this file is the only one that touches a
    window or a socket. It seeds the app once the renderer is up, gives it a real clock, and
    parks between changes: the app says how long nothing will happen and the runner sleeps
    that long, so an idle window draws nothing while the scripted people pause.

    THE LIVE ROOM. The Channels group opens on a demo room, simulated people on a short
    script, and no socket: the default run opens none and needs no server. Press "Connect to
    a live room" in a channel, with the server that comes with it running:

      python3 examples/ex24_messenger/serve.py      (Python 3.9+, standard library)

    and the channel becomes a room on that server. Connect two windows to one serve.py, or a
    window and the web build, and each sees the other. The channel's details (click its name)
    show the socket itself: its address and subprotocol, its send buffer against the cap, its
    traffic, how the last one closed (the code and the reason), and every event in order; its
    Simulate menu asks serve.py to drop the connection, close the room, restart or stall, or
    sends a 2 MB burst, so every state can be seen on purpose.

      rcWebSocket(url, .protocols, ...caps)   open once, from an event; a token comes back
      rcWebSocketNext(ws, &e)                 every frame: OPEN, MESSAGE..., ERROR, CLOSE
      rcWebSocketState(ws)                    readyState, bufferedAmount, the close fields
      rcWebSocketSendText(ws, text, n)        false, with nothing sent, past the send cap
      rcWebSocketSendBinary(ws, bytes, n)     the burst, in 64 KB messages
      rcWebSocketClose(ws, 1000, reason)      the clean way out, when you change rooms
      rcWebSocketFree(ws)                     done with it
      rcJsonParse / RC_JsonWriter             every message is one JSON object

    The calls are in src/app/live_room.h. A room that drops without a clean close comes back
    by itself, backing off from 1 s to 30 s; a close the server meant is kept, with its code
    and reason on screen. When nothing answers, the channel stays on its demo lines, under a
    strip that says so, with Retry. A socket that cannot open here (no backend, no CA
    certificates, RC_NO_NET) reads CLOSED / UNAVAILABLE, and the strip shows RayClay's
    reason. A page reaches its own server by a
    relative URL, so serve.py --web <dir> serves the page and its rooms from one origin.

    A WORKER THREAD. "Export chat" writes the transcript from a thread of its own, then wakes
    the parked window with rcAppWake: the one call by which another thread reaches the app.
    See export_job below, and msg_worker_start in messenger_backend.h.

    One source for the desktop, the web and the phones. Headless smoke: RAYCLAY_MAX_FRAMES=N
    draws N frames and exits 0.

    Build: cmake --build build --target rayclay_ex24_messenger
*/
#define MESSENGER_WORKER_IMPLEMENTATION
#include "app/live_room.h"

/* What the banner says about an export: the app's own banner, and the request is over. */
static void show_export_result(AppState *st, const char *words) {
    messenger_notice(st, words);
    st->exportAsk = -1;
}

typedef struct {
    AppState state;
    int      barHeight;
} Runner;

/* The export: a whole transcript, written from a thread of its own. The disk is the one thing
   here that can stall for longer than a frame, so the frame hands the bytes over and goes on.
   rcSaveFile runs on any thread; the job stores its answer in the flag first, then calls
   rcAppWake, the one call by which another thread reaches the running app. The window may be
   parked with nothing else due, and this is what brings it back to read the flag. */
enum { EXPORT_IDLE = 0, EXPORT_RUNNING, EXPORT_DONE, EXPORT_FAILED };

static struct {
    volatile long     state;
    char              path[512];
    char              bytes[128 * 1024];
    size_t            length;
    RC_SaveCheckpoint stored;   /* a DONE export's checkpoint, until it answers */
} g_export;

static void export_job(void *arg) {
    const RC_FileStatus status = rcSaveFile(g_export.path, g_export.bytes, g_export.length, 0);

    (void)arg;
    msg_flag_set(&g_export.state, status == RC_FILE_OK ? EXPORT_DONE : EXPORT_FAILED);
    rcAppWake();
}

/* The transcript is written here, on the main thread, because the store is the main thread's:
   the worker only ever sees these finished bytes. */
static void export_start(RC_App *app, AppState *st, int conv) {
    const MsgConversation *c = &st->store.conv[conv];
    const char *dir = rcAppDataPath("RayClay", "Messenger");
    size_t n = 0;

    if (!dir) {
        show_export_result(st, "Exporting needs app storage, which this host refuses.");
        return;
    }
    rcStrCopy(g_export.path, rcFormat(rcAppArena(app), "%s%s.txt", dir, c->name).chars,
              sizeof g_export.path);
    for (int i = 0; i < c->count; i++) {
        const MsgMessage *m = &c->items[i];
        const char *who = m->dir == MSG_DIR_OUTGOING ? st->myName
                        : (m->who && c->kind != MSG_CONV_DIRECT ? m->who : c->name);
        const RC_String line = rcFormat(rcAppArena(app), "[%s %s] %s: %s\n",
                                        msg_day_word(&st->store, m->day), m->ts, who,
                                        msg_snippet(m));

        for (int k = 0; k < (int)line.length && n < sizeof g_export.bytes; k++)
            g_export.bytes[n++] = line.chars[k];
    }
    g_export.length = n;
    msg_flag_set(&g_export.state, EXPORT_RUNNING);
    msg_worker_start(export_job, NULL);
}

/* Each frame: a new request, and the answer of a finished job. Three steps are told apart:
   RUNNING (the menu item is off), DONE (rcSaveFile wrote the file: on the web, "storing") and
   "Saved", which waits for its checkpoint to read STORED: on the web the browser storing the
   page's file, on a desktop or a phone at once. A refusal is said, and the user's next Export
   is the retry: nothing retries by itself. */
static void export_poll(RC_App *app, AppState *st) {
    const long state = msg_flag_get(&g_export.state);

    if (st->exportAsk >= 0 && state == EXPORT_IDLE && !g_export.stored)
        export_start(app, st, st->exportAsk);
    if (state == EXPORT_DONE || state == EXPORT_FAILED) {
        if (state == EXPORT_DONE) {
            g_export.stored = rcSaveCheckpoint();
            if (rcSavePoll(g_export.stored) == RC_SAVE_PENDING)
                messenger_notice(st, "Exported; the browser is storing it.");
        } else {
            show_export_result(st, "The chat could not be saved.");
        }
        msg_flag_set(&g_export.state, EXPORT_IDLE);
        st->exportAsk = -1;
    }
    if (g_export.stored) {
        const RC_SaveStatus stored = rcSavePoll(g_export.stored);

        if (stored == RC_SAVE_PENDING)
            return;
        /* The request ended at DONE: an Export asked meanwhile waits above and is kept. */
        messenger_notice(st, stored == RC_SAVE_STORED
                                 ? rcFormat(rcAppArena(app), "Saved to %s", g_export.path).chars
                                 : "The browser did not store the chat: export it again.");
        g_export.stored = 0;
    }
}

/* The wall clock, as the user's own clock and calendar read it, is the app's now: a line you
   send carries the time you sent it, a live room's line the local time its server stamped,
   and the sample chats keep their ages, so a line from two minutes ago still reads so. With
   no reading the app stays on its demo clock, noon on a Thursday. */
static void set_real_now(AppState *st) {
    const double now = rcUnixTimeSeconds();
    RC_LocalTime local;

    if (now > 0.0 && rcLocalTime(now, &local)) {
        const int64_t seconds = (int64_t)now + local.utcOffsetSeconds;   /* on the user's clock */

        msg_store_set_now(&st->store, (int32_t)(seconds / 86400), (uint32_t)(seconds % 86400));
    }
}

static void demo_update(RC_App *app, void *userData) {
    Runner *run = (Runner *)userData;
    AppState *st = &run->state;
    /* Its dt is the frame's true delta, a park included: a call's timer and a toast count
       the seconds that really passed, however far apart a parked window's frames are. */
    const AppCtx ctx = app_demo_ctx(app);

    if (!st->seeded) {              /* seed once the renderer is up */
        messenger_seed(st, 0xC0FFEEu);
        live_init(app, st);
    }
    set_real_now(st);
    live_poll(app, st);
    messenger_update(st, &ctx);
    /* After the update: this frame's dt is the time the window was parked, and the banner an
       answer raises should not start out that much older. */
    export_poll(app, st);
}

static void demo_layout(RC_App *app, void *userData) {
    Runner *run = (Runner *)userData;
    AppState *st = &run->state;
    RC_Window *win = rcAppMainWindow(app);
    const AppCtx ctx = app_demo_ctx(app);

    /* The band's height follows the layout: slim on a desktop, an app bar on a phone. The band
       is unzoomed, so the drag strip is the same number at every zoom. */
    const int barHeight = (int)messenger_bar_height(&ctx);

    if (barHeight != run->barHeight) {
        rcWindowSetTitlebarHeight(win, barHeight);
        run->barHeight = barHeight;
    }
    messenger_layout(st, &ctx);

    /* The window's clear color is a snapshot taken at create, not a live link to the style,
       so push it every frame or the old ground shows wherever the layout does not cover the
       window. The call is change-gated inside the library. */
    rcWindowSetClearColor(win, rcGetStyle().background);

    /* Ask for exactly the frames the app needs; the rest of the time it sleeps. Midnight is
       one: today's lines turn into yesterday's. */
    float wake = messenger_wake_in(st);
    const float midnight = (float)(86400u - st->store.nowClock);

    if (st->store.wallClock && (wake < 0.0f || wake > midnight))
        wake = midnight;
    if (wake == 0.0f)
        rcWindowRequestFrame(win);
    else if (wake > 0.0f)
        rcWindowRequestFrameAfter(win, (double)wake);
}

int main(void) {
    static Runner runner;   /* zero-init, and too large for the stack */

    /* The one font size named here is the default face's, which the fields, menus and
       tooltips draw in: the rest are written where they are used, in messenger_app.c. */
    static const float body_size[] = { 14.0f };

    rcSetStyle(messenger_style(false));

    RC_AppOptions opts = {
        .width               = 1280,
        .height              = 720,
        .title               = "RayClay Messenger",
        .clearColor          = rcGetStyle().background,
        .fontSizes           = body_size,
        .fontCount           = 1,
        /* Backs every rcFormat in the live room's log lines and URLs. */
        .scratchArenaBytes   = 256 * 1024,
        .startLayoutElements = 16384,     /* a whole thread is declared, every line of it */
        .frame               = RC_WINDOW_FRAME_NONE,
        .updateCallback      = demo_update,
        .layoutCallback      = demo_layout,
        .userData            = &runner,
        .titlebar            = { .height = 40 },     /* the band IS the titlebar */
        .renderMode          = RC_RENDER_ON_DEMAND,
    };

    return rcRunApp(&opts);
}
