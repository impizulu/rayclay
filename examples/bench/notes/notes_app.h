/*
    notes_app.h - the notes app's contract: AppState, the font ladder, the hooks.

    Types and prototypes only - no RayClay element DSL and no libc - so it
    includes cleanly from C and from C++. The app is a note-taking and blog
    editor: a sidebar of notes, a document tab strip, a Write/Preview split, a
    publish dialog and a settings dialog.
*/
#ifndef NOTES_APP_H
#define NOTES_APP_H

#include "bench_app.h"       /* the shared AppCtx / AppInputSink / AppMode contract */
#include "notes_backend.h"   /* NoteStore, embedded by value in AppState            */

/* The frozen bench scenario's version. Bump ONLY when the scripted path's
   rendered output changes - never for demo-only chrome. */
#define NOTES_BENCH_VERSION 7

/* Font ladder, baked from the bundled face. Shared by the GUI and the runner, so
   the two cannot desync; the editor derives a text area's height from a size
   here, which is why the GUI reads the table rather than only the ids. */
typedef enum { F_SMALL = 0, F_BODY, F_MD, F_HEAD, F_TITLE, F_COUNT } NoteFont;
static const float NOTES_FONT_PX[F_COUNT] = { 13.0f, 16.0f, 18.0f, 22.0f, 30.0f };

/* Six open documents is what the tab strip holds at a readable width. */
#define NOTES_MAX_TABS 6
#define NOTES_TAB_LABEL_CAP 24

/* App state: a flat memset-able POD, so notes_seed can memset then set fields. */
typedef struct {
    NoteStore store;         /* the backend, BY VALUE (the seed's memset zeroes it)   */
    int   openNote;          /* the open note index, -1 = none                        */
    /* tabs[] is the SET of open documents and activeTab says which of them openNote
       is. notes_open is the one writer of both, which is what stops the strip and
       the editor from ever disagreeing. */
    int   tabs[NOTES_MAX_TABS];
    int   tabCount;
    int   activeTab;         /* index into tabs[], not a note index                   */
    /* The tab labels, elided to the tab's width. They live here because a text run
       BORROWS the bytes you hand it until the frame is drawn, so a stack buffer
       would be gone by then. notes_update refreshes them. */
    char  tabLabel[NOTES_MAX_TABS][NOTES_TAB_LABEL_CAP];
    /* The tear-out gesture; -1 is "no gesture", which is why the seed sets these
       explicitly rather than leaving them at the memset's zero. */
    int   dragTab;           /* tab index under the press                             */
    float dragOriginX;       /* where the press landed, for the slop test             */
    float dragOriginY;
    bool  dragLive;          /* travelled past the slop: the ghost is up              */
    /* The tear-out REQUEST. This file holds no RC_App, so it cannot open a window:
       it records which note to detach and where the pointer let go, and the runner
       turns that into a window. tearTab is the tab to close once one opens. */
    int   tearNote;          /* note index to open in its own window, -1 = none       */
    int   tearTab;           /* the tab it came from, -1 = none                       */
    float tearX, tearY;      /* the drop point, in window px                          */
    int   tab;               /* main pane: 0 = Write, 1 = Preview                     */
    int   folderFilter;      /* collections: 0 = All, 1 = Drafts, 2 = Published       */
    int   category;          /* publish/settings combo index into the category list   */
    char  search[64];        /* filters the note list by title and preview            */
    char  title[NOTE_TITLE_CAP];  /* sized to the store slot, so the editor never     */
                                  /* holds more than note_set_title persists          */
    char  body[4096];        /* the Write tab's multi-line body editor                */
    char  tags[96];          /* the tags line; note_set_tags persists it              */
    char  slug[64];          /* the publish dialog's slug                             */
    char  summary[192];      /* the publish dialog's summary                          */
    bool  modalPublish;      /* the publish dialog (mutually exclusive with settings) */
    bool  modalSettings;
    bool  isPublic;          /* publish dialog: shown in the Preview details table    */
    bool  autoSave;          /* settings: off means an edit stays "Unsaved"           */
    bool  darkMode;          /* settings: the night half of notes_style()             */
    /* The save indicator. An edit sets dirty and rewinds saveT; notes_update spends
       the injected dt and clears both, which is what turns "Saving" into "Saved". */
    bool  dirty;
    float saveT;
    /* "The user asked for this note", which is not the same as "a note is open":
       the seed always opens one so the wide layout never shows an empty editor, and
       on a phone that would mean opening on an arbitrary note instead of the list.
       Only a note tap or "+" sets it; only the compact back control clears it. */
    bool  editorOpen;
    float editorFontPx;      /* settings slider: steps the Write-tab body font (12-24) */
    bool  seeded;            /* demo lazy-init guard (seed once the renderer is up)    */
} AppState;

/* The app's palette. notes_layout installs it every frame, so the demo and the
   bench arm render the same app; the runner also asks for it before the window
   opens, to key the clear colour. */
RC_Style notes_style(bool dark);

/* The four-function app contract, plus the demo-only chrome. */
void notes_seed  (AppState *st, unsigned seed);
void notes_update(AppState *st, const AppCtx *ctx);         /* advance by ctx->dt */
void notes_layout(AppState *st, const AppCtx *ctx);         /* the whole UI */
void notes_demo_chrome(AppState *st, const AppCtx *ctx);    /* demo-only overlay */

/* One torn-off document, drawn into its own window by the runner. */
void notes_torn_window(AppState *st, int noteIndex);
/* Settle a tear-out: close the source tab only once the window actually opened,
   so a target with no second window keeps the document rather than losing it. */
void notes_tear_settle(AppState *st, bool opened);
void notes_bench_step(AppState *st, const AppInputSink *in, int frame);

#endif /* NOTES_APP_H */
