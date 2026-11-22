/*
    state.h - everything the app knows, in one struct main.c owns as a static
*/
#ifndef APP_STATE_H
#define APP_STATE_H

#include "rayclay.h"
#include "app/settings.h"
#include "app/notes.h"
#include "app/theme.h"
#include "app/update.h"

enum { MAX_TABS = 5, FILTER_CAP = 64, HEX_CAP = 8 };

/* How the settings show at this size: a dialog over the app; a rail and a page filling
   the window; or, one column wide, the rail with each page pushed over it. */
typedef enum { ARM_DIALOG = 0, ARM_SPLIT, ARM_STACK } SettingsArm;

/* The rail's entries, top to bottom: PAGES in pages.h holds one row for each. */
typedef enum {
    PAGE_GENERAL = 0, PAGE_READER, PAGE_APPEARANCE, PAGE_HOTKEYS, PAGE_ACCOUNT, PAGE_PLUGINS,
    PAGE_ABOUT, PAGE_COUNT
} PageId;

/* The accent picker, which keeps its own hue: black and white have none, and dragging
   through them must not throw away the one the user was on. */
typedef struct {
    bool     open;
    float    hue, sat, val;
    int      drag;            /* 0 none, 1 the square, 2 the hue strip */
    char     hex[HEX_CAP];
    uint32_t seen;            /* the accent the fields above describe */
} AccentPicker;

/* The hotkey dialog. A chord that belongs to another command waits here until Replace. */
typedef struct {
    bool        open;
    int         command;
    Chord       pending;
    int         conflict;     /* the command that holds .pending, -1 none */
    const char *refusal;      /* why the last chord cannot be a hotkey    */
} HotkeyCapture;

typedef struct {
    Settings      set;
    Settings      saved;            /* what settings.json holds                     */
    Settings      settled;          /* set as it stood at settledAt                 */
    double        settledAt;        /* when set last changed                        */
    RC_FileStatus saveStatus;
    bool          everSaved;

    bool          settingsOpen;
    bool          settingsSeen;   /* the dialog has opened once */
    int           arm;              /* how the last layout showed them: SettingsArm */
    int           page;             /* the rail's choice                            */
    int           shownPage;        /* the page the pane last drew, for its scroll  */
    bool          pageOpen;         /* one column: the page, not the rail, is up    */
    bool          confirmReset;
    bool          copiedPath;
    char          settingsPath[1024];

    AccentPicker  picker;
    HotkeyCapture capture;
    char          filter[FILTER_CAP];

    Updater       update;
    bool          developer;
    bool          started;          /* the first frame's one-time work is done      */
    bool          dismiss;          /* the app was backgrounded: drop the caret     */
    float         seenZoom;

    /* The host app */
    int           tabs[MAX_TABS];
    int           tabCount;
    int           activeTab;
    int           tabFirst;         /* the leftmost tab the band shows              */
    bool          filesOpen;
    bool          folderOpen[FOLDER_COUNT];
} AppState;

#endif /* APP_STATE_H */
