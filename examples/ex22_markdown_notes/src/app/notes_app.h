/*
    notes_app.h - the notes app's contract: its state and its hooks.

    Types and prototypes only, no RayClay element DSL, so it includes cleanly from C
    and from C++. The app is four functions over AppState; main.c runs them against a
    real window and the benchmark against a scripted one.
*/
#ifndef NOTES_APP_H
#define NOTES_APP_H

#include "app_ctx.h"         /* AppCtx / AppInputSink / AppMode: the seam */
#include "notes_backend.h"   /* NoteStore, embedded by value in AppState  */

/* The frozen bench scenario's version. Bump ONLY when the scripted path's rendered
   output changes, never for demo-only chrome. */
#define NOTES_BENCH_VERSION 22

/* How the open note is shown. Reader mode is separate (AppState.reader): it replaces
   the whole window, not just the editor pane. */
typedef enum { VIEW_WRITE = 0, VIEW_SPLIT, VIEW_PREVIEW } NoteView;

/* Which pane has the space when not all of them fit. See notes_layout. */
typedef enum { PANE_SIDEBAR = 0, PANE_LIST, PANE_EDITOR } NotePane;

/* What the undo toast can take back. */
typedef enum { UNDO_NONE = 0, UNDO_TRASH, UNDO_ARCHIVE } NoteUndo;

/* The faces the runner registered. Slot 0 is Lato Regular at 14, the size every library
   control draws, because a control draws its slot's own size and takes no .size. A failed
   load, and the scripted run, leave these at 0: everything still draws, in slot 0. */
typedef struct {
    uint16_t bold;
    uint16_t italic;
    uint16_t mono;
    uint16_t source[3];    /* regular at each step's body size, for the text area */
    bool     failed;       /* the runner could not read the font files */
} NoteFaces;

#define NOTES_MAX_HEADINGS 48
#define NOTES_MAX_BACKLINKS 8

/* One heading of the open note: an entry in the reader's contents. */
typedef struct {
    int level;
    int offset;            /* where its line starts in the body */
    int length;            /* its text, without the # signs     */
} NoteHeading;

/* App state: a flat memset-able POD, so notes_seed can zero it and set fields. */
typedef struct {
    NoteStore   store;          /* the notebook, BY VALUE                             */
    NoteFaces   faces;
    RC_Arena    arena;          /* this frame's formatted ids and counts              */
    int         openNote;       /* the open note's slot, -1 = none                    */
    NoteSection section;        /* what the list is showing                           */
    char        tag[NOTE_TAG_CAP];   /* the tag path, for NOTE_SEC_TAG                */
    NoteView    view;
    NotePane    pane;           /* the one-pane arm's current pane                    */
    bool        sidebarHidden;  /* the wide arm's sidebar, Ctrl+\                     */
    bool        notesOpen;      /* the sidebar's Notes group is expanded              */
    int         order[NOTE_MAX_NOTES];  /* the list's rows this frame                 */
    int         orderCount;
    bool        searchOpen;
    char        search[64];

    /* The docs reader: a contents rail and a scroll spy over one note. */
    bool        reader;
    bool        readerContents; /* the one-pane arm is showing the contents           */
    char        tocSearch[48];  /* filters the contents by section text               */
    NoteHeading headings[NOTES_MAX_HEADINGS];
    int         headingCount;
    int         firstSection;   /* the first section: 1 when heading 0 is the title   */
    int         activeHeading;  /* the section on screen: the scroll spy's answer     */
    int         pendingHeading; /* a section to reach, -1 = none                      */
    int         pickedHeading;  /* the section picked, until the page moves           */
    float       readOffset;     /* where the page was, to notice the reader moving it */
    float       readProgress;

    /* Reading preferences. */
    int         textStep;       /* index into the type ramp: small, medium, large     */
    int         widthStep;      /* index into the measures: narrow, normal, wide      */
    bool        dark;
    bool        keepOnDevice;   /* save the notebook to the app data folder           */
    uint32_t    triedRevision;  /* the revision the last save attempted               */
    uint32_t    savedRevision;  /* the revision the saved file holds (accepted)       */
    uint32_t    storedRevision; /* the revision confirmed in storage (rcSavePoll)     */
    uint32_t    checkpointRevision;  /* the revision saveCheckpoint is about          */
    RC_SaveCheckpoint saveCheckpoint;  /* a save's checkpoint not yet STORED, or 0    */
    bool        saveFailed;     /* the last save's rcSaveFile failed                  */
    bool        storeRefused;   /* storage refused the last accepted save (the web)   */
    bool        saveRetry;      /* Retry was pressed: save again this frame           */

    /* The open note's backlinks, recomputed on an edit or a note switch. */
    int         backlinks[NOTES_MAX_BACKLINKS];
    int         backlinkCount;
    bool        backlinksStale;

    bool        focusField;     /* a new note or an append: focus its text area       */
    float       pageY;          /* where the open note's page was, to hand back       */
    int         pageToEnd;      /* frames until an append's page jumps to its end     */
    bool        modalSettings;
    bool        modalEmptyTrash;

    /* The toast and what its Undo takes back. */
    NoteUndo    undoKind;
    int         undoNote;
    float       toastLeft;      /* seconds; the toast is up while above zero          */
    char        toastText[64];
    float       splitFraction;
    const char *copiedBlock;    /* the code block whose button says Copied            */
    float       copiedLeft;

    /* REQUESTS TO THE RUNNER. This file's code holds no RC_App and links no window,
       file or browser call, so the scripted run stays free of all three: the layout
       records what the user asked for and main.c does it after the frame. */
    char        requestUrl[512];
    const char *requestCopy;    /* frame-arena text for the clipboard                 */
    int         requestExport;  /* a note to hand to the save dialog, -1 = none       */
    int         requestWindow;  /* a note to open in its own window, -1 = none        */
    bool        canOpenWindows; /* the runner's answer: child windows exist here      */
    bool        requestFrame;   /* a box read this frame moved: one more frame        */
    bool        seeded;         /* the runner seeds once, when the window is up       */
} AppState;

/* The app's palette as a style. notes_layout installs it every frame, so the demo and
   the bench render the same app; the runner also asks for it before the window opens,
   to key the clear color. */
RC_Style notes_style(bool dark);

/* The four-function app contract. */
void notes_seed  (AppState *st, unsigned seed);
void notes_update(AppState *st, const AppCtx *ctx);         /* advance by ctx->dt */
void notes_layout(AppState *st, const AppCtx *ctx);         /* the whole UI */
void notes_bench_step(AppState *st, const AppInputSink *in, int frame);

/* The wall clock, rcUnixTimeSeconds, and the zone's offset, from the runner before each
   update. The first call dates the sample notes back from it, as the seed dated them back
   from its fixed demo moment, so call it before reading a saved notebook. Without it, as in
   the scripted run, the clock is that demo moment advanced by ctx->dt. */
void notes_set_clock(AppState *st, double unixSeconds, int utcOffset);

/* A line in the toast with nothing to undo: what the runner has to say about a request. */
void notes_notice(AppState *st, const char *text);

/* One note rendered read-only, drawn into its own window by the runner. */
void notes_note_window(AppState *st, int noteIndex, RC_Arena *arena);

/* The notebook as text, for the runner to put in a file and hand back. Text in and text
   out: this side links no file call. Reading returns false on a damaged file. */
size_t notes_write_notebook(const AppState *st, char *out, size_t cap);
bool   notes_read_notebook(AppState *st, const char *text);

#endif /* NOTES_APP_H */
