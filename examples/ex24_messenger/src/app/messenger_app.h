/*
    messenger_app.h - the messenger's state and its four-function contract.

    Declarations only, with no layout macros, so this is clean to include from C and C++.
    The app core never touches a window or a socket: main.c fills an AppCtx, and a live
    room reaches the core only through the LiveRoom below, which main.c's live_room.h
    keeps up to date and acts on.
*/
#ifndef MESSENGER_APP_H
#define MESSENGER_APP_H

#include "app_ctx.h"           /* AppCtx / AppInputSink / AppMode: the seam */
#include "messenger_backend.h" /* MsgStore, embedded by value in AppState  */

/* The scripted scenario's version. Bump when its rendered output changes. */
#define MESSENGER_BENCH_VERSION 23

enum {
    LIVE_SERVER_CAP = 200, LIVE_URL_CAP = 320, LIVE_NOTE_CAP = 240, LIVE_PROTO_CAP = 64,
    LIVE_EVENT_MAX = 32, LIVE_PEOPLE_MAX = 32, LIVE_NAME_CAP = 97, LIVE_BURST_COUNT = 32
};

/* The caps a live socket opens with, on screen in the room's details: the largest message
   taken in, the received bytes not yet read, and the bufferedAmount past which a send is
   refused. A chat line is at most 2 KB; the send cap is what the 2 MB burst runs into. */
#define LIVE_CAP_MESSAGE  ((size_t)256 * 1024)
#define LIVE_CAP_UNREAD   ((size_t)1024 * 1024)
#define LIVE_CAP_BUFFERED ((size_t)1024 * 1024)

/* Where a channel's lines come from. DEMO is no socket at all, what the app starts in;
   the rest are a socket's life: its handshake on the way, open and asking to join, in the
   room, waiting out a back-off, or closed for good. */
typedef enum {
    LIVE_DEMO = 0, LIVE_CONNECTING, LIVE_JOINING, LIVE_OPEN, LIVE_WAITING, LIVE_CLOSED
} LiveLink;

/* What the screen asks of the socket. Read once a frame, at the top of the next one. */
typedef enum {
    LIVE_ASK_NONE = 0, LIVE_ASK_CONNECT, LIVE_ASK_LEAVE, LIVE_ASK_ROOM, LIVE_ASK_RENAME,
    LIVE_ASK_DROP, LIVE_ASK_CLOSE, LIVE_ASK_RESTART, LIVE_ASK_STALL, LIVE_ASK_BURST
} LiveAsk;

typedef enum { LIVE_TONE_INFO = 0, LIVE_TONE_OK, LIVE_TONE_WARN } LiveTone;

typedef struct {
    double   at;                  /* seconds since launch */
    LiveTone tone;
    char     text[LIVE_NOTE_CAP];
} LiveEvent;

typedef struct {
    int  id;
    char name[LIVE_NAME_CAP];
} LivePerson;

/* A live room as the screen sees it: everything here is a copy, written by live_room.h,
   so the core draws it with no socket call and a scripted run simply never fills it. */
typedef struct {
    LiveLink   link;
    LiveAsk    ask;
    int        askRoom;           /* LIVE_ASK_ROOM and CONNECT: the channel it is for */
    int        room;              /* the channel the socket is on                     */
    bool       offline;           /* nothing answered: demo lines, and why            */
    bool       relative;          /* a page reaches its own server by a relative URL  */
    int        tries;             /* attempts since the last stable open              */
    float      retryIn;           /* seconds until the back-off's next attempt        */
    int        people;            /* LivePerson entries in use                        */
    LivePerson who[LIVE_PEOPLE_MAX];
    int        here[3];           /* how many are in each room, from the rooms list   */
    char       server[LIVE_SERVER_CAP];  /* the Server field, as typed                */
    char       url[LIVE_URL_CAP];        /* the socket's address, as opened           */
    char       protocol[LIVE_PROTO_CAP];
    char       closeText[LIVE_NOTE_CAP * 2];   /* how the last socket closed; "" none */
    char       closeWhy[LIVE_NOTE_CAP];        /* the reason its server gave; "" none */
    char       failText[LIVE_NOTE_CAP];        /* why the first one failed; "" none   */
    int        status;            /* the upgrade's HTTP status; 0 on the web          */
    size_t     buffered, peak;
    unsigned   msgsIn, msgsOut;
    uint64_t   bytesIn, bytesOut, sunk;
    int        burstSent, burstRefused;
    LiveEvent  events[LIVE_EVENT_MAX];
    int        eventHead, eventCount;
} LiveRoom;

typedef enum { NAV_CHATS = 0, NAV_STARRED, NAV_ARCHIVED } MsgNav;
typedef enum { FILTER_ALL = 0, FILTER_UNREAD, FILTER_GROUPS } MsgFilter;

/* App state: a flat, memset-able POD, so messenger_seed can memset then set. No pointer
   into transient memory ever lives here. */
typedef struct {
    MsgStore store;            /* the backend, by value                               */
    RC_Arena frame;            /* this frame's strings: reset at the top of the layout */
    unsigned char frameBytes[64 * 1024];
    float    time;             /* the dots' clock: the injected dt, turned over at 20 s */
    float    typingPhase;      /* the typing dots' place in their cycle               */
    int      openConv;         /* the chat on screen                                  */
    char     drafts[MSG_MAX_CONVERSATIONS][256];  /* what you were writing in each    */
    char     search[64];       /* the list's search                                   */
    char     threadSearch[64]; /* search in this chat                                 */
    bool     threadSearchOn;
    int      matchAt;          /* the current match, from the newest                  */
    int      matchCount;
    uint32_t replyTo;          /* the line the composer quotes; 0 = none              */
    uint32_t reactFor;         /* the line whose reaction bar is up; 0 = none         */
    uint32_t scrollTo;         /* the line to bring into view when it is laid out     */
    uint32_t flashId;          /* the line glowing after a jump                       */
    float    flashLeft;
    int      unreadMarkConv;   /* where the "N unread" divider stands                 */
    uint32_t unreadMarkId;
    int      unreadMarkCount;
    bool     stickerOpen;
    bool     modalAttach, modalSettings, modalNewChat, modalPhoto, modalCall;
    double   callTime;         /* seconds since it rang: a double, as a call may last days */
    bool     callMuted;
    uint32_t photoShown;       /* the line the viewer shows                           */
    int      photoConv;
    char     newChatQuery[64];
    int      attachPick;
    char     attachCaption[128];
    bool     infoOpen;         /* the contact, group or channel details               */
    bool     sidebarCollapsed; /* one pane: the chat list is put away                 */
    bool     channelsCollapsed;
    MsgNav   nav;
    MsgFilter listFilter;
    bool     darkMode;
    bool     enterSends;
    bool     notifications;
    int      wallpaper;
    int      statusCombo;      /* your own presence: 0 online, 1 away, 2 invisible    */
    char     myName[LIVE_NAME_CAP];
    char     toast[160];       /* the banner: words, whose chat, and whether to undo  */
    char     toastTitle[MSG_NAME_CAP];
    int      toastConv;        /* opens on click; -1 = just words                     */
    int      undoConv;         /* the deleted chat Undo brings back; -1 = none        */
    float    toastLeft;
    int      asideConv;        /* the chat asked to be deleted or archived; -1 none   */
    bool     asideArchive;
    int      threadDrawn;      /* the chat this frame's thread pane declared          */
    bool     undoArchive;      /* what Undo reverses: an archive, else a delete       */
    bool     undoOpen;         /* the chat was on screen: Undo opens it again         */
    bool     threadTight;      /* a narrow pane: bubbles wide, hover tools floating   */
    int      exportAsk;        /* the chat "Export chat" asked for; main.c's job; -1 none */
    int      sendKind;         /* a line asked for after the thread: MsgKind + 1; 0 none */
    int      sendArt;
    char     sendText[128];    /* its caption, or a call's note                       */
    LiveRoom live;
    bool     seeded;           /* main.c's lazy-init guard: seed once the renderer is up */
} AppState;

/* The four-function app contract. No RC_App or window handle; the input seam is the
   AppInputSink (app_ctx.h). */
void messenger_seed  (AppState *st, unsigned seed);       /* memset, then build the model */
void messenger_update(AppState *st, const AppCtx *ctx);   /* advance the model by ctx->dt */
void messenger_layout(AppState *st, const AppCtx *ctx);   /* the whole UI */
void messenger_bench_step(AppState *st, const AppInputSink *in, int frame);

/* How long the app can sleep, for a runner that parks between frames: seconds until the
   next change, 0 for "now", negative for "nothing is coming". */
float messenger_wake_in(const AppState *st);

/* A banner with words of the runner's own, up as long as any of the app's: main.c's export
   says how it went through this. */
void messenger_notice(AppState *st, const char *words);

/* The band's height at this size, before the content zoom. The window's drag strip must match
   what the layout draws, so main.c passes it, unzoomed, to rcWindowSetTitlebarHeight. */
float messenger_bar_height(const AppCtx *ctx);

/* The app's shell: the library preset with the messenger's green over it. messenger_layout
   installs it every frame; main.c also needs it before the first frame, for the clear color. */
RC_Style messenger_style(bool dark);

#endif /* MESSENGER_APP_H */
