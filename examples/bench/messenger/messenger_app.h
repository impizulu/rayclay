/*
    messenger_app.h - the messenger app's types and prototypes.

    Declarations only: no RayClay layout macros and no system includes, so this is
    clean to include from C and from C++.

    The app is a desktop chat client - a conversation list with presence, unread
    weight and live search, a bubble transcript with delivery state, an anchored
    composer, a contact profile, and settings. Three panes where they fit; one
    pane at a time on a narrow window.
*/
#ifndef MESSENGER_APP_H
#define MESSENGER_APP_H

#include "bench_app.h"        /* the shared AppCtx / AppInputSink / AppMode contract */
#include "messenger_backend.h" /* MsgStore, embedded by value in AppState  */

/* The scripted scenario's version. Bump when its rendered output changes. */
#define MESSENGER_BENCH_VERSION 7

/* Font ladder. The GUI and the demo runner share the index set, so they never
   desync. */
typedef enum { F_SMALL = 0, F_BODY, F_HEAD, F_TITLE, F_COUNT } MsgFont;

/* App state - a flat, memset-able POD, so messenger_seed can memset then set.
   No pointer into transient memory ever lives here. */
typedef struct {
    MsgStore store;         /* the backend, BY VALUE (the seed's memset zeroes it) */
    uint64_t tick;          /* app frame counter; app-level UI timers key off it   */
    int      openConv;      /* the open conversation index                         */
    char     composer[256]; /* the rcTextInput composer buffer (ASCII)            */
    char     search[64];    /* the sidebar search buffer                           */
    bool     modalAttach;   /* the attach / emoji picker modal (rcBeginModal open) */
    bool     modalSettings; /* the settings modal (mutually exclusive with attach) */
    bool     infoOpen;      /* the right-hand info drawer (a contact's profile)    */
    bool     sidebarCollapsed; /* nav-rail toggle: hide the conversation sidebar    */
    bool     darkMode;      /* theme toggle                                        */
    bool     readReceipts;  /* draw the delivery mark on your own messages         */
    bool     attachOriginal;/* attach modal: send the picture at full size         */
    int      attachPick;    /* attach modal: the chosen tile, -1 = nothing chosen  */
    float    notifVolume;   /* notification volume; 0 mutes, and the header says so */
    int      statusCombo;   /* your own presence: 0 online, 1 away, 2 offline      */
    bool     seeded;        /* demo lazy-init guard (seed once the renderer is up) */
} AppState;

/* The four-function app contract (+ the demo-only chrome). No RC_App / window handle;
   the input seam is the shared AppInputSink (bench_app.h). */
void messenger_seed  (AppState *st, unsigned seed);            /* memset, then build the model */
void messenger_update(AppState *st, const AppCtx *ctx);        /* advance the model by ctx->dt */
void messenger_layout(AppState *st, const AppCtx *ctx);        /* the whole UI */
void messenger_demo_chrome(AppState *st, const AppCtx *ctx);   /* demo-only overlay */
void messenger_bench_step(AppState *st, const AppInputSink *in, int frame);

/* The app's shell: the library preset with this app's charcoal/warm-neutral surfaces
   and teal accent over it. messenger_layout installs it every frame; the demo runner
   also needs it before the first frame, for the window's clear colour. */
RC_Style messenger_theme(bool dark);

#endif /* MESSENGER_APP_H */
