/*
    main.c - Pulse Dash: a one-button rhythm runner across three courses.

    Shows: a game that is a RayClay app. A custom titlebar that plays the course's
    waveform, a floating scene of vector sprites, one input (SPACE, a click or a tap)
    on desktop, web and phone with no platform #ifdef, and rendering that follows the
    game: continuous while something moves, and asleep at about zero CPU when the
    player pauses or leaves a menu alone.

    The course, the scene and the screens are app/platformer_app.c; this file is the
    window's side of it: the clock, the display face and the saved progress.

    Run it from the repository root or from its own folder: the display face is read
    from examples/assets/fonts/. Launched elsewhere it draws in the bundled face.
    Build and run the rayclay_ex29_dash_game target. RAYCLAY_MAX_FRAMES=N draws N
    frames and exits.
*/
#include "app/platformer_app.h"

#define PL_DISPLAY_FONT "examples/assets/fonts/RussoOne-Regular.ttf"
#define PL_SAVE_NAME    "progress.json"
/* The most a saved count reads back as: three of them still add up in an int, as the title
   screen's total does, and play never reaches it. */
#define PL_SAVED_MAX    100000000

/* The saved progress is one small JSON file in the app's data folder, which is
   IndexedDB on the web and survives an uninstall nowhere it should not. */
static const char *progress_path(RC_App *app) {
    const char *dir = rcAppDataPath("RayClay", "PulseDash");

    return dir ? rcFormat(rcAppArena(app), "%s%s", dir, PL_SAVE_NAME).chars : "";
}

/* A saved figure, within what the game can have written. The file is the user's to edit, and
   a percent past 100 or a count near the int's end would overflow the next attempt's count,
   the title's total and the level card's bar arithmetic. */
static int32_t progress_int(RC_Json one, const char *key, int32_t top) {
    const int64_t v = rcJsonInt64(one, key, 0);

    return v < 0 ? 0 : v > top ? top : (int32_t)v;
}

static void progress_load(RC_App *app, AppState *st) {
    const char *path = progress_path(app);
    void *bytes = NULL;
    size_t length = 0;

    if (!path[0] || rcLoadFile(path, &bytes, &length) != RC_FILE_OK)
        return;
    {
        RC_Json doc = rcJsonParse((const char *)bytes, length);
        RC_Json levels = rcJsonGet(doc, "levels");

        st->skin = (int)rcJsonInt64(doc, "skin", 0);
        if (st->skin < 0 || st->skin >= PL_SKIN_COUNT)
            st->skin = 0;
        for (int i = 0; i < PL_LEVEL_COUNT && i < rcJsonCount(levels); i++) {
            RC_Json one = rcJsonAt(levels, i);
            PlRecord *r = &st->record[i];

            r->bestPct     = progress_int(one, "best", 100);
            r->practicePct = progress_int(one, "practice", 100);
            r->bestScore   = progress_int(one, "score", PL_SAVED_MAX);
            r->attempts    = progress_int(one, "attempts", PL_SAVED_MAX);
            r->coins       = (uint8_t)progress_int(one, "coins", UINT8_MAX);
        }
        rcJsonFree(doc);
    }
    rcFreeFile(bytes);
}

static void progress_save(RC_App *app, const AppState *st) {
    const char *path = progress_path(app);
    char buf[1024];
    RC_JsonWriter w = rcJsonWriter(buf, sizeof buf);
    const char *text;

    rcJsonBeginObject(&w, NULL);
    rcJsonWriteInt64(&w, "skin", st->skin);
    rcJsonBeginArray(&w, "levels");
    for (int i = 0; i < PL_LEVEL_COUNT; i++) {
        const PlRecord *r = &st->record[i];

        rcJsonBeginObject(&w, NULL);
        rcJsonWriteInt64(&w, "best", r->bestPct);
        rcJsonWriteInt64(&w, "practice", r->practicePct);
        rcJsonWriteInt64(&w, "score", r->bestScore);
        rcJsonWriteInt64(&w, "attempts", r->attempts);
        rcJsonWriteInt64(&w, "coins", r->coins);
        rcJsonEnd(&w);
    }
    rcJsonEnd(&w);
    rcJsonEnd(&w);
    text = rcJsonWriterFinish(&w);
    if (text && path[0])
        rcSaveFile(path, text, w.len, 0);
}

static void demo_update(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    RC_Window *window = rcAppMainWindow(app);
    const bool focused = rcWindowIsFocused(window);
    AppCtx ctx;

    if (!st->seeded) {                      /* seed once the renderer is up */
        platformer_seed(st, 0x1A2B3C4Du);
        st->display = rcLoadFont(PL_DISPLAY_FONT, 40.0f);
        progress_load(app, st);
        platformer_show_title(st);
    }

    /* ctx.dt is the true delta of this frame, unsmoothed: on-demand frames arrive whenever
       they like, and the sim's capped bank, not a clamp here, turns a stall into a pause. */
    ctx = app_demo_ctx(app);

    /* Walking away from a live run must not kill it. */
    if (!st->windowBlurred && !focused)
        platformer_pause(st);
    st->windowBlurred = !focused;

    platformer_update(st, &ctx);

    if (st->recordDirty) {
        progress_save(app, st);
        st->recordDirty = false;
    }
    rcWindowSetRenderMode(window, platformer_wants_frames(st) ? RC_RENDER_CONTINUOUS
                                                              : RC_RENDER_ON_DEMAND);
    rcSetKeepScreenOn(st->screen == SCREEN_PLAY && !st->paused);
}

static void demo_layout(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    AppCtx ctx = app_demo_ctx(app);

    platformer_layout(st, &ctx);
}

int main(void) {
    static AppState state;   /* zero-init; platformer_seed fills it on frame 1 */

    RC_AppOptions opts = {
        .width               = 1280,
        .height              = 720,
        .title               = "Pulse Dash",
        .clearColor          = rcHex(0x120a2e),   /* Midnight Drive's sky, as frame 1 paints it */
        .startLayoutElements = 4096,      /* the scene peaks near 300 floating roots */
        .frame               = RC_WINDOW_FRAME_NONE,
        .updateCallback      = demo_update,
        .layoutCallback      = demo_layout,
        .userData            = &state,
        .titlebar            = { .height = PL_TOPBAR_H }, /* the topbar IS the titlebar */
        .minWidth            = 360,       /* the smallest window the layout is drawn for */
        .minHeight           = 400,
        /* It starts drawing every frame for the demo behind the title screen.
           demo_update then decides, frame by frame, whether anything is moving. */
        .renderMode          = RC_RENDER_CONTINUOUS,
    };

    return rcRunApp(&opts);
}
