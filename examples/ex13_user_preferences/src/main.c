/*
 *  ex13 - RayClay Preferences   |   a settings dialog to copy, and a signed update check
 *
 *  The settings of a notes app: a category rail on the left, a page of rows on the right,
 *  and every row a name, a line of explanation and one control. Each setting applies as
 *  you change it, to the whole window, and is on disk a moment later: change the accent
 *  and every toggle recolors under the pointer; press Esc and the notes behind the dialog
 *  already read the way you just set them. A window with no room for a dialog gets the
 *  same rail and pages filling it, and a phone gets the rail with each page pushed over it.
 *
 *  src/app/pane.h is the part to lift: a row is one call, and a page is a list of them.
 *  src/app/settings.h is the rest of the pattern: one struct, one JSON file under
 *  rcAppDataPath, saved by comparing the struct with what is on disk.
 *
 *  src/app/update.h is the update check. The About page asks a signed manifest whether a
 *  newer build exists, downloads it and restarts into it: run this example's serve.py,
 *  then open About and turn on Developer options to point the check at it.
 *
 *  Shows: rcBeginModal and its stack, rcToggle, rcCombo, rcSlider, rcTextInput with its
 *  .inputKind keyboards and .clearButton, rcTextArea, rcViewport().keyboard and .safe, a
 *  custom title band, rcSetStyle at run time, rcKeyPressed and rcModDown for hotkeys,
 *  rcPointer and rcGetElementBox for a color picker, rcSaveFile, rcLoadFile, rcJsonParse,
 *  rcJsonWriter, rcUpdateCheck, rcUpdateResult, rcUpdateApply and rcUpdateRelaunch.
 *
 *  Build target: rayclay_ex13_user_preferences
 */
#include "app/app.h"

/* A change still waiting out SAVE_DELAY goes to disk here. A page left with the keyboard
   up should not come back with it up, so on a touch screen this asks for the caret to be
   dropped and update() does it on the next frame: nothing may draw from here. */
static void on_app_event(RC_AppEvent event, void *user) {
    AppState *st = (AppState *)user;

    if (event != RC_APP_EVENT_SUSPENDED && event != RC_APP_EVENT_TERMINATING) return;
    settings_flush(st);
    if (rcPointerIsCoarse()) st->dismiss = true;
}

/* What the window opens on: the settings as saved, and the tab they left open. */
static void seed(AppState *st) {
    int i;

    settings_load(&st->set);
    st->settingsOpen = st->set.openAtStart;
    settings_path(st->settingsPath, (int)sizeof st->settingsPath);
    st->filesOpen = true;
    for (i = 0; i < FOLDER_COUNT; i++) st->folderOpen[i] = true;
    tabs_open(st, st->set.reopenLast && st->set.lastNote < NOTE_COUNT ? st->set.lastNote : 0);
    /* Opening that tab is not a change to save. */
    st->saved = st->settled = st->set;
}

int main(void) {
    static AppState state;
    static float    sizes[] = { (float)T_BODY };   /* slot 0: what every control draws in */
    int             result;

    seed(&state);
    rcSetStyle(app_style(state.set.scheme != SCHEME_LIGHT, rcHex(state.set.accent),
                         state.set.radius));
    app_classes();
    rcSetAppEventHandler(on_app_event, &state);

    RC_AppOptions opts = {
        .width = 1280, .height = 820, .title = "RayClay Preferences",
        .clearColor = rcGetStyle().background,
        .fontSizes = sizes, .fontCount = 1,
        .scratchArenaBytes = 65536,          /* backs every rcFormat in a frame */
        .frame = RC_WINDOW_FRAME_NONE,
        .updateCallback = update, .layoutCallback = layout, .userData = &state,
        .titlebar = { .height = BAR_H },     /* band() is the whole chrome      */
        .renderMode = RC_RENDER_ON_DEMAND,   /* parks between input and blinks  */
        /* A phone app that is portrait-first says so. iOS narrows the Info.plist
           set to it; Android applies it when the window opens, so the APK's
           manifest declares the same lock (ORIENTATION portrait where the build
           adds this example) or the app turns after its first frame. */
        .orientation = RC_ORIENTATION_PORTRAIT,
    };
    result = rcRunApp(&opts);
    settings_flush(&state);
    return result;
}
