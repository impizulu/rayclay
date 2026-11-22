/*
    platformer_app.h - the app's contract: types and prototypes only, so it
    includes cleanly from C and from C++.

    Two drivers can share it: main.c, which opens a real window, and a headless
    one that feeds the same core a fixed dt, a fixed seed and synthetic
    input. The core runs identically either way - only the SOURCE of the clock,
    the seed and the input differs.
*/
#ifndef PLATFORMER_APP_H
#define PLATFORMER_APP_H

#include "app_ctx.h"              /* AppCtx / AppInputSink / AppMode: the seam */
#include "platformer_backend.h"   /* PlWorld, embedded by value in AppState    */

/* Bump ONLY when the scripted path's rendered output changes. */
#define PLATFORMER_BENCH_VERSION 12

/* The titlebar's height. main.c pins the window's drag strip to it and the layout
   measures the play area against it, so the two must be one number. */
#define PL_TOPBAR_H 44

/* Seconds of no input before a menu's demo stops and the window goes to sleep. */
#define PL_MENU_IDLE_SECONDS 60.0f

#define PL_SKIN_COUNT 6

typedef enum { SCREEN_TITLE = 0, SCREEN_LEVELS, SCREEN_PLAY } PlScreen;

/* What the player has earned on one course. main.c saves these. */
typedef struct {
    int32_t bestPct;
    int32_t practicePct;
    int32_t bestScore;
    int32_t attempts;
    uint8_t coins;          /* the most taken in one winning run */
} PlRecord;

/* How to draw one hero. rcIconEmit keeps the pointer until the frame is drawn, so
   a pose lives in AppState rather than on the stack. */
typedef struct {
    int32_t  turn;      /* 1/1024 of a full turn */
    RC_Color ink;
    RC_Color eye;
    int      face;
} PlPose;

enum {
    POSE_HERO = 0, POSE_TITLEBAR, POSE_LOGO, POSE_TILE, POSE_SKIN0,
    POSE_COUNT = POSE_SKIN0 + PL_SKIN_COUNT
};

/* App state - one flat struct that owns nothing outside itself, so platformer_seed can
   zero it and rebuild. The run lives in `world`; everything else is the shell around it. */
typedef struct {
    PlWorld  world;
    float    tickAcc;        /* seconds banked toward the next fixed tick          */
    bool     jumpQueued;     /* set by layout on a press; consumed on a tick       */
    bool     jumpHeld;       /* a press that began on the playfield is still down  */
    bool     seeded;         /* demo lazy-init guard                               */

    PlScreen screen;
    int      selected;       /* the course a run or the level card is on           */
    bool     practice;
    int      skin;
    PlRecord record[PL_LEVEL_COUNT];
    int      courseCoins[PL_LEVEL_COUNT];   /* coins on each course, counted at seed */
    bool     recordDirty;    /* main.c saves on the next frame, then clears this   */

    bool     paused;         /* bound to the pause modal                           */
    bool     skinsOpen;      /* bound to the cube modal                            */
    bool     wonOpen;        /* bound to the level-complete modal                  */
    bool     panelFocused;   /* the open modal has focused its default button      */
    bool     newBest;        /* this death or win beat the record                  */
    int      idleDeaths;     /* attempts in a row with no press at all             */
    bool     pressedThisRun;

    float    menuIdle;       /* seconds a menu has had no input                    */
    bool     windowBlurred;  /* main.c: the window was unfocused last frame        */

    uint16_t display;        /* font slot of the display face; 0 is the bundled one */
    PlPose   pose[POSE_COUNT];
    char     bestStr[24];    /* rewritten when a run ends                          */
    char     statBuf[3][16];
    char     coinStr[12];    /* the level card's count                             */
} AppState;

void platformer_seed  (AppState *st, unsigned seed);            /* zero then build   */
void platformer_update(AppState *st, const AppCtx *ctx);        /* dt <= 0 is a no-op */
void platformer_layout(AppState *st, const AppCtx *ctx);        /* the UI, both modes */
void platformer_bench_step(AppState *st, const AppInputSink *in, int frame);

/* The window's side of the app, for main.c. A bench has no window and calls neither. */
void platformer_show_title(AppState *st);
void platformer_pause(AppState *st);
bool platformer_wants_frames(const AppState *st);

#endif /* PLATFORMER_APP_H */
