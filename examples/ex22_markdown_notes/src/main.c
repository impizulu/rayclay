/*
    RayClay Notes - Bear's three panes with Obsidian's links, on one Markdown buffer.

    Demonstrates, in src/app/:
      notes_markdown.h  Markdown rendered live by walking the text: headings, lists,
                        task lists, emphasis, tables, callouts, highlighted code,
                        inline HTML. No document tree: an edit shows on the next frame.
      notes_app.c       a three-pane layout that folds to one pane on a phone, a text
                        area over the note's own buffer, Write / Split / Preview with
                        rcBeginSplitPane, tags, pin, search, delete with undo, wiki
                        links and backlinks; Ctrl+G (Cmd+G) selects the search's
                        next match in the text area (rcSetFieldSelection).

    THE DOCS READER (open a note, press the book icon):
      this file         a font family registered one file per weight, a log sink read
                        for a load that failed, a custom window icon, drawing on demand,
                        and a notebook kept in the app data folder when Settings asks.
      notes_app.c       a contents rail with a scroll spy (notes_scroll_spy), search over
                        the sections, reading progress off the scroll offset, a one-pane
                        arm, and notes_measure_px, a reading column derived from the
                        font's own measurements.
      notes_theme.h     a whole type ramp per text-size step.

    The fonts and the icon are read from examples/assets/, by a path relative to the
    working directory, else on a desktop to the program's folder, where the CMake build
    copies them. Found in neither, the registrations fail, RayClay keeps drawing in the
    bundled face, and the status bar says so.

    Build: cmake --build build --target rayclay_ex22_markdown_notes
    RAYCLAY_MAX_FRAMES=N draws N frames and exits, a headless smoke test.
*/
#include "app/notes_app.h"

#ifndef RC_EX22_FONT_DIR
    #define RC_EX22_FONT_DIR "examples/assets/fonts/"
#endif
#ifndef RC_EX22_ICON
    #define RC_EX22_ICON "examples/assets/logos/rayclay-logo-1024.png"
#endif

/* The extra windows "Open in new window" makes. RC_WindowOptions.userData is BORROWED,
   so it must outlive the window: a static table does, and gives the state callback
   somewhere to record the close. */
#define NOTES_MAX_WINDOWS 4

typedef struct {
    AppState  *st;
    int        note;
    bool       live;
    RC_Window *window;
} NoteWindow;

static NoteWindow g_windows[NOTES_MAX_WINDOWS];

/* THE LEVEL IS THE VERDICT, NOT THE WORDING. Within a font call an ERROR means the load
   was refused and you hold the default slot; a WARNING means the face loaded with
   something worth knowing. The sentences are open-ended. The line still goes to the
   platform log, because a sink replaces it. */
static bool g_font_error;

static void font_log(RC_LogLevel level, const char *msg, void *user) {
    (void)user;
    if (level == RC_LOG_ERROR)
        g_font_error = true;
    rcLogWrite(level, NULL, msg);
}

/* ONE FILE PER WEIGHT: point two weights at one file and both calls succeed, return
   different ids, and render identical outlines. Registration knows about files; each
   id then draws at any .size. The sink is only for these calls, so it comes off again.
   Slot 0, the regular weight at 14, came from RC_AppOptions.fontPath. A text field draws
   its slot's own size and takes no .size, so the source area's other two sizes each need
   a slot registered for them. */
static void load_faces(AppState *st) {
    rcSetLogSink(font_log, NULL);
    st->faces.bold   = rcRegisterFont("Lato", RC_WEIGHT_BOLD,
                                      RC_EX22_FONT_DIR "Lato-Bold.ttf", 16);
    st->faces.italic = rcRegisterFont("Lato Italic", RC_WEIGHT_REGULAR,
                                      RC_EX22_FONT_DIR "Lato-Italic.ttf", 16);
    st->faces.mono   = rcRegisterFont("Adwaita Mono", RC_WEIGHT_REGULAR,
                                      RC_EX22_FONT_DIR "AdwaitaMono-Regular.ttf", 14);
    st->faces.source[0] = 0;
    st->faces.source[1] = rcRegisterFont("Lato", RC_WEIGHT_REGULAR,
                                         RC_EX22_FONT_DIR "Lato-Regular.ttf", 16);
    st->faces.source[2] = rcRegisterFont("Lato", RC_WEIGHT_REGULAR,
                                         RC_EX22_FONT_DIR "Lato-Regular.ttf", 18);
    st->faces.failed = g_font_error;
    rcSetLogSink(NULL, NULL);
}

/* THE NOTEBOOK ON DISK, off until Settings turns it on, so a default run reads and
   writes nothing and opens on the sample notebook. Saved a second after the last edit,
   so a burst of typing is one write, and rcSaveFile's write is atomic: a crash leaves
   the previous file, never half of a new one. Three steps are told apart: a save
   ATTEMPTED (triedRevision), ACCEPTED (rcSaveFile answered OK: savedRevision) and
   STORED (its rcSaveCheckpoint read STORED: storedRevision). On a desktop or a phone
   the last two are one frame; on the web the browser writes the page's file to
   IndexedDB later, and may refuse. A failure or a refusal keeps the note unsaved
   and shows Retry: the next edit or that button saves again, never the next frame. */
static char g_file_text[NOTE_MAX_NOTES * (NOTE_BODY_CAP + 64)];

static const char *notebook_path(void) {
    static char path[1024];
    const char *dir = rcAppDataPath("RayClay", "Notes");

    if (!dir)
        return NULL;
    rcStrCopy(path, dir, sizeof path);
    {
        size_t k = 0;

        while (path[k])
            k++;
        /* The folder's path already ends in the platform's separator. */
        rcStrCopy(path + k, "notebook.txt", sizeof path - k);
    }
    return path;
}

static void load_notebook(AppState *st) {
    const char *path = notebook_path();
    void       *bytes = NULL;
    size_t      length = 0;

    if (path && rcLoadFile(path, &bytes, &length) == RC_FILE_OK) {
        if (!notes_read_notebook(st, (const char *)bytes))
            notes_notice(st, "The saved notebook could not be read");
        rcFreeFile(bytes);
    }
}

/* Saves an edit once typing has paused a second; flush saves an outstanding one now. */
static void keep_notebook(AppState *st, double now, bool flush) {
    static uint32_t lastRevision;
    static double   changedAt;
    static bool     wasKept;
    const char     *path;
    bool            due;

    if (st->store.revision != lastRevision) {
        lastRevision = st->store.revision;
        changedAt    = now;
    }
    due = st->store.revision != st->triedRevision && (flush || now - changedAt >= 1.0);

    if (st->keepOnDevice && (!wasKept || st->saveRetry || due)) {
        st->saveRetry     = false;
        st->triedRevision = st->store.revision;
        path = notebook_path();
        if (!path || rcSaveFile(path, g_file_text,
                                notes_write_notebook(st, g_file_text, sizeof g_file_text),
                                RC_FILE_PRIVATE | RC_FILE_PARENTS) != RC_FILE_OK) {
            notes_notice(st, "The notebook could not be saved");
            st->saveFailed = true;
        } else {
            st->savedRevision      = st->store.revision;
            st->checkpointRevision = st->store.revision;
            st->saveCheckpoint     = rcSaveCheckpoint();
            st->saveFailed         = false;
            st->storeRefused       = false;
        }
    } else if (!st->keepOnDevice && wasKept) {
        path = notebook_path();
        if (path)
            rcRemovePath(path, 0);
    }
    wasKept = st->keepOnDevice;
    /* A refused checkpoint may still turn STORED (a later write-back), so it is kept;
       the runner wakes this app when the answer moves. */
    if (st->saveCheckpoint) {
        const RC_SaveStatus stored = rcSavePoll(st->saveCheckpoint);

        if (stored == RC_SAVE_STORED) {
            st->storedRevision = st->checkpointRevision;
            st->saveCheckpoint = 0;
            st->storeRefused   = false;
        } else if (stored == RC_SAVE_REFUSED && !st->storeRefused) {
            notes_notice(st, "The browser did not store the notebook");
            st->storeRefused = true;
        }
    }
}

/* A code block's Copy formats its text into the arena of the window it was pressed in, and
   a note window's arena is reset by that window's next frame: the clipboard takes the text
   in the frame that asked, never on the main window's next frame. */
static void run_copy(AppState *st) {
    if (st->requestCopy) {
        rcClipboardSet(st->requestCopy);
        st->requestCopy = NULL;
    }
}

/* ONE CLOCK FOR EVERY WINDOW. A Copy pressed in a note window starts the Copied label's
   countdown while the main window may sleep for half a minute; each window's frame advances
   the clock, so the main window's next frame counts only the time since the press. A window's
   own rcWindowFrameTime counts from that window's last frame, so two windows drawing would
   charge the same seconds twice: the delta is taken from this one clock instead. */
static double g_clock;

/* THE REAL TIME OF DAY. A note's age, the Today section and an edit's stamp come from the
   wall clock in the user's own zone, read on every frame, so they are right after a sleep.
   rcLocalTime zeroes its answer when the OS has none, and the ages then count in UTC. */
static void read_wall_clock(AppState *st) {
    double       wall = rcUnixTimeSeconds();
    RC_LocalTime local;

    if (wall > 0.0) {
        rcLocalTime(wall, &local);
        notes_set_clock(st, wall, local.utcOffsetSeconds);
    }
}

static void advance_clock(RC_App *app, AppState *st, double now) {
    AppCtx ctx = app_demo_ctx(app);

    read_wall_clock(st);
    ctx.dt  = (float)(now - g_clock);
    g_clock = now;
    notes_update(st, &ctx);
}

static void window_update(RC_Window *w, void *userData) {
    RC_App *app = rcWindowApp(w);

    advance_clock(app, ((NoteWindow *)userData)->st, rcAppTime(app));
}

static void window_layout(RC_Window *w, void *userData) {
    NoteWindow *nw = (NoteWindow *)userData;

    notes_note_window(nw->st, nw->note, rcWindowArena(w));
    run_copy(nw->st);
    /* The main window keeps the Copied label's countdown drawing (demo_layout), and it may
       be asleep: a Copy pressed here wakes it. */
    if (nw->st->copiedLeft > 0.0f)
        rcWindowRequestFrameAfter(rcAppMainWindow(rcWindowApp(w)), 0.25);
}

/* The moment the surface is gone, hand the record back and free the slot, so a later
   open finds room. An OS close alone does not release a record: without the release,
   each open and close would keep one until the app exits. */
static void window_state(RC_Window *w, RC_WindowState state, void *userData) {
    NoteWindow *nw = (NoteWindow *)userData;

    if (state == RC_WINDOW_CLOSED || state == RC_WINDOW_FAILED) {
        rcWindowRelease(w);
        nw->window = NULL;
        nw->live   = false;
    }
}

static void open_note_window(RC_App *app, AppState *st, int note) {
    RC_Window *primary = rcAppMainWindow(app);
    int        slot    = 0;

    while (slot < NOTES_MAX_WINDOWS && g_windows[slot].live)
        slot++;
    if (slot == NOTES_MAX_WINDOWS)
        return;
    g_windows[slot].st   = st;
    g_windows[slot].note = note;
    g_windows[slot].live = true;
    {
        /* In struct order: C++20 requires designated initializers in declaration order,
           and this example builds under both compilers. */
        RC_WindowOptions o = {
            .title       = "Note",
            .width       = 560,
            .height      = 680,
            .minWidth    = 320,
            .minHeight   = 240,
            .owner       = rcWindowOwnerSupported() ? primary : NULL,
            .frame       = RC_WINDOW_FRAME_NONE,
            .titlebar    = { .height = 44 },
            .clearColor  = rcGetStyle().surface,
            /* On demand, unlike a game: this surface draws a document and waits. */
            .renderMode        = RC_RENDER_ON_DEMAND,
            /* The page formats an id per link and button into this window's arena. */
            .scratchArenaBytes = 128 * 1024,
            .updateCallback = window_update,
            .layoutCallback = window_layout,
            .stateCallback  = window_state,
            .userData       = &g_windows[slot],
        };

        /* NULL means the open failed: the note simply stays where it is. A target with
           no child windows never asks (AppState.canOpenWindows). */
        g_windows[slot].window = rcAppOpenWindow(app, &o);
        if (g_windows[slot].window == NULL)
            g_windows[slot].live = false;
    }
}

/* The suggested file name: the note's title without the bytes a file name may not hold
   on some platform (rcFileDialogSave lists them), and without a final dot or space. */
static void export_name(const char *title, char *out, size_t cap) {
    static const char refused[] = "/\\:<>\"|?*";
    size_t            k = 0;

    for (const char *p = title; *p && k + 4 < cap; p++) {
        bool keep = (unsigned char)*p >= 0x20;

        for (const char *r = refused; *r; r++)
            keep = keep && *p != *r;
        if (keep)
            out[k++] = *p;
    }
    while (k > 0 && (out[k - 1] == '.' || out[k - 1] == ' '))
        k--;
    rcStrCopy(out + k, k ? ".md" : "note.md", cap - k);
}

/* What the layout asked for, done outside it: notes_app.c holds no RC_App and links no
   window, file or browser call, which is what lets the bench run it headless. Each
   request here waits for an explicit click, so the default run opens no socket and no
   file dialog. */
static void run_requests(RC_App *app, AppState *st) {
    static RC_FileDialogToken exportToken;

    if (st->requestUrl[0]) {
        if (!rcOpenUrl(st->requestUrl))
            notes_notice(st, "That link could not be opened");
        st->requestUrl[0] = '\0';
    }
    run_copy(st);
    if (st->requestExport >= 0) {
        const Note *n = note_at(&st->store, st->requestExport);

        /* One save at a time: a second while the first's dialog is up is refused, and its
           token would take the first's place, whose answer would then go unread. */
        if (n && !exportToken) {
            char   name[NOTE_TITLE_CAP + 8];
            size_t len = 0;

            export_name(n->title, name, sizeof name);
            while (n->body[len])
                len++;
            exportToken = rcFileDialogSave(name, n->body, len);
        }
        st->requestExport = -1;
    }
    /* Polled every frame, starting with the one that asked: a save that is refused, or a
       platform with no dialog to show, answers at the call. */
    if (exportToken) {
        RC_FileDialogResult answer = rcFileDialogPoll(exportToken);

        if (answer.status != RC_FILE_DIALOG_PENDING)
            exportToken = 0;
        if (answer.status == RC_FILE_DIALOG_DONE)
            notes_notice(st, "Exported");
        else if (answer.status != RC_FILE_DIALOG_PENDING &&
                 answer.status != RC_FILE_DIALOG_CANCELED)
            notes_notice(st, "The note could not be exported");
    }
    if (st->requestWindow >= 0) {
        open_note_window(app, st, st->requestWindow);
        st->requestWindow = -1;
    }
}

static void demo_update(RC_App *app, void *userData) {
    AppState *st  = (AppState *)userData;
    double    now = rcAppTime(app);

    if (!st->seeded) {                 /* seed once the window and its fonts are up */
        notes_seed(st, 0xFACADEu);
        /* After the seed, which zeroes the state: a phone, a tablet and a browser tab have
           one window, and the note's menu offers "Open in new window" only where it can. */
        st->canOpenWindows = rcChildWindowsSupported();
        load_faces(st);
        read_wall_clock(st);   /* first, so a saved notebook's times read against it */
        load_notebook(st);
        g_clock = now;
    }
    advance_clock(app, st, now);
}

static void demo_layout(RC_App *app, void *userData) {
    AppState  *st     = (AppState *)userData;
    RC_Window *window = rcAppMainWindow(app);
    AppCtx     ctx    = app_demo_ctx(app);

    notes_layout(st, &ctx);

    /* THE SECOND LINE A RUNTIME THEME SWITCH NEEDS. A window's clear color is a snapshot
       taken at create, so the old ground shows wherever the layout does not cover the
       window. Unconditional, because the library only acts when the color moves. */
    rcWindowSetClearColor(window, rcGetStyle().surface);

    run_requests(app, st);
    keep_notebook(st, rcAppTime(app), false);

    /* A note window names its note by slot, and the slot of a note deleted for good is the
       next new note's: it lets go in the frame of the delete, before a new note can take it. */
    for (int w = 0; w < NOTES_MAX_WINDOWS; w++)
        if (g_windows[w].live && !note_at(&st->store, g_windows[w].note))
            g_windows[w].note = -1;

    /* DRAWING ON DEMAND: an idle window draws nothing, so anything that changes without
       input has to ask. The toast and the Copied label count down; the age labels turn
       over once a minute; a box read this frame describes last frame, so when the scroll
       spy's answer moved it asks for one more. */
    if (st->requestFrame) {
        st->requestFrame = false;
        rcWindowRequestFrame(window);
    } else if (st->toastLeft > 0.0f || st->copiedLeft > 0.0f ||
               (st->keepOnDevice && st->store.revision != st->triedRevision)) {
        rcWindowRequestFrameAfter(window, 0.25);
    } else {
        rcWindowRequestFrameAfter(window, 30.0);
    }

    /* A note window draws this notebook, and nothing in that window tells it when the note,
       the theme or the Copied label changes here: each frame here asks for one there. */
    for (int w = 0; w < NOTES_MAX_WINDOWS; w++)
        if (g_windows[w].live)
            rcWindowRequestFrame(g_windows[w].window);
}

/* An edit still waiting out the pause is saved as the app is hidden, left or closed: a
   phone may end a hidden app without another frame. On the web this runs inside the page's
   event, before the write-back that event starts. */
static void on_app_event(RC_AppEvent event, void *userData) {
    AppState *st = (AppState *)userData;

    if (st->seeded && (event == RC_APP_EVENT_SUSPENDED || event == RC_APP_EVENT_TERMINATING ||
                       event == RC_APP_EVENT_CONTEXT_LOST))
        keep_notebook(st, 0.0, true);
}

int main(void) {
    static AppState     state;   /* zero-init; notes_seed fills it on frame 1 */
    static const float  uiSize[] = { 14.0f };   /* slot 0: what every control draws at */
    int                 result;

    /* The app's palette, so the window's clear color matches the first frame. */
    rcSetStyle(notes_style(false));

    RC_AppOptions opts = {
        .width               = 1280,
        .height              = 820,
        .title               = "RayClay Notes",
        .clearColor          = rcGetStyle().surface,
        .fontPath            = RC_EX22_FONT_DIR "Lato-Regular.ttf",
        .fontSizes           = uiSize,
        .fontCount           = 1,
        .startLayoutElements = 8192,   /* a long note declares many elements a frame */
        .frame               = RC_WINDOW_FRAME_NONE,
        .updateCallback      = demo_update,
        .layoutCallback      = demo_layout,
        .userData            = &state,
        .titlebar            = { .height = 48 },     /* the band IS the titlebar */
        .iconPath            = RC_EX22_ICON,
    };

    rcSetAppEventHandler(on_app_event, &state);
    result = rcRunApp(&opts);
    if (state.seeded)
        keep_notebook(&state, 0.0, true);   /* a desktop window closed mid-pause */
    return result;
}
