/*
    state.h - everything the app remembers, in one struct

    The UI is declared from this every frame. Limits are fixed and named: a static struct of
    arrays costs no pages until they are touched, and a limit a user can reach says so where it
    is hit (Output, Problems or the find bar's count) instead of failing later.
*/
#ifndef APP_STATE_H
#define APP_STATE_H

#include "rayclay.h"
#include "app/analysis.h"
#include "app/doc.h"
#include "app/theme.h"

enum {
    MAX_TABS = 24, MAX_NODES = 8192, MAX_DEPTH = 12, MAX_LOG = 256, MAX_HITS = 20000,
    MAX_RESULTS = 2000, MAX_PICKS = 64, SINK_BYTES = 1 << 18, QUERY_BYTES = 128,
    MAX_FILE_BYTES = 8 << 20, MARK_BYTES = 4096
};

/* One entry of the open folder. The array is in display order, a folder followed by its whole
   subtree, so `end` is where the subtree stops and folding a folder is a jump over it. */
typedef struct {
    const char *path, *name;
    const char *bytes;      /* the file as last read: the find-in-files cache */
    int         size;
    double      time;       /* the file's time when read                      */
    int         end;
    uint8_t     depth;
    bool        dir, open, read;
    bool        inMemory;   /* a sample: its bytes are the program's own      */
} Node;

typedef struct {
    const char *name;
    bool        dir;
} Entry;

typedef struct {
    char     rootName[NAME_BYTES];
    char     rootPath[PATH_BYTES];
    bool     samples;           /* the in-memory project, no folder behind it */
    bool     imported;          /* rootPath is a dialog's private copy        */
    bool     reread;            /* a file's bytes moved: results are stale    */
    Node     node[MAX_NODES];
    int      count, files, folders;
    int      rows[MAX_NODES];   /* the unfolded nodes, what the tree draws    */
    int      nRows;
    Entry    stack[MAX_NODES];  /* a walk's pending names, deepest on top     */
    int      stackTop;
    RC_Arena names;             /* every path, for the life of the folder     */
} Workspace;

/* Find in the open file. Hits are rebuilt when the query, its options or the text change. */
typedef struct {
    bool     open, replaceOpen, matchCase, wholeWord;
    bool     focusQuery;        /* put the caret in the field next frame      */
    char     query[QUERY_BYTES], with[QUERY_BYTES];
    char     seen[QUERY_BYTES]; /* the query the hits are for                 */
    bool     seenCase, seenWord;
    unsigned seenRev;
    int      seenDoc;
    int      hits[MAX_HITS];    /* the byte offset of each match              */
    int      nHits, current;    /* current is -1 until a hit is chosen        */
} Find;

/* Find in files: read a few files a frame, so a large folder never holds up a frame. */
typedef struct {
    int     node, line, col;
    int     at, len;            /* the matched line's bytes in the node       */
    bool    head;               /* a file's heading row, not a match          */
    int     matches;            /* a heading: how many below it               */
} Result;

typedef struct {
    char   query[QUERY_BYTES];
    char   seen[QUERY_BYTES];
    bool   running, matchCase;
    int    next;                /* the next node to read                      */
    int    filesHit, matches;
    Result results[MAX_RESULTS];
    int    n;
} Search;

/* The palette holds four lists in one field, the way its leader does: a plain query finds
   files, ">" commands, "@" symbols of this file and ":" a line. */
enum { PICK_FILE, PICK_COMMAND, PICK_SYMBOL };

typedef struct {
    int kind, index, score;
    int hits[FUZZY_MAX_HITS], nHits;
} Pick;

/* The palette's state, typed after VS Code's name for it, Quick Open: Palette is the colors. */
typedef struct {
    bool open;
    char query[QUERY_BYTES];
    char seen[QUERY_BYTES];
    int  seenDoc;               /* the tab and the edit the symbol picks index */
    unsigned seenRev;
    bool scrollToPick;
    Pick picks[MAX_PICKS];
    int  n, sel;
} QuickOpen;

/* The time is the app clock's double: a float of it reads a tenth of a second wrong after
   twelve days. */
typedef struct {
    double time;
    int    level;               /* SEV_* */
    char   text[120];
} LogLine;

/* Bottom panel tabs, sidebar views and, on a phone, the one screen on show. */
enum { PANEL_PROBLEMS, PANEL_OUTPUT };
enum { SIDE_EXPLORER, SIDE_SEARCH };
enum { SCREEN_CODE, SCREEN_FILES, SCREEN_SEARCH, SCREEN_PROBLEMS, SCREEN_OUTPUT, SCREEN_COUNT };

/* Why the unsaved-changes dialog is up. */
enum { CLOSE_NONE, CLOSE_TAB, CLOSE_ALL, CLOSE_APP };

/* What the live preview draws: the buffer through its handle, or the file by path. */
enum { ROUTE_HANDLE, ROUTE_PATH };
enum { GROUND_CHECKER, GROUND_DARK, GROUND_LIGHT };

typedef struct {
    RC_App   *app;
    RC_Arena *arena;            /* this frame's, for rcFormat                 */
    double    now;
    float     cell;             /* the width of one code character            */
    uint16_t  mono;             /* the code font's slot                       */
    bool      shown;            /* the first frame is drawn: the next loads   */
    bool      started;          /* the folder, files and font are read        */

    /* The open files, in tab order. */
    Doc       docs[MAX_TABS];
    int       nDocs, active;    /* active is -1 with no tab open              */
    unsigned  nextDocId, untitled;  /* counted for the run: they wrap, never overflow */

    Workspace ws;
    Find      find;
    Search    search;
    QuickOpen pal;
    LogLine   log[MAX_LOG];
    int       nLog;

    /* The shape of the window. */
    bool      dark, zen, sideOpen, panelOpen, minimapOn, outlineOpen, rootOpen;
    bool      compact;          /* a phone's window: one screen at a time     */
    bool      wasShort;         /* the window last read as too short to fold  */
    char      paneId[16];       /* the code view this frame declared          */
    int       screen, sideView, panelTab;
    float     sideFrac, panelFrac;
    float     barH;             /* eased, so the bar folds rather than jumps  */
    char      problemFilter[QUERY_BYTES];
    int       revealNode;       /* a tree node to scroll to, -1 for none      */
    int       openNode, openLine, openCol;  /* CMD_OPEN_NODE's file and place; line -1: none */
    float     treeY, outlineY;  /* each list's place, kept while it is folded */
    bool      treePlaced, outlinePlaced;    /* each list's first reveal is done */
    float     viewH;            /* the code view's height when last read      */
    char      tabShown[16];     /* the tab last brought into the strip's view */
    const Symbol *outlineHere;  /* the symbol the outline last followed to    */

    /* The editor's keyboard: a real text field at the caret takes what is typed. */
    char      sink[SINK_BYTES];
    bool      wantCaret;        /* the editor owns the keyboard               */
    bool      chordK;           /* the primary+K half of a two-key shortcut   */
    int       swallowFrames;    /* the field's next letter finished a chord   */
    bool      menuOpen;         /* a popup is up: keys go to nothing          */
    bool      focusSearch;      /* put the caret in the search field          */
    char      shownDoc[16];     /* the tab the tree last revealed             */

    /* The editor's mouse. */
    bool      dragging;
    bool      miniDrag;         /* a press on the minimap is still held       */
    int       miniTop;          /* the minimap's first line at that press     */
    int       clicks;           /* the press's count: 2 a word, 3 the line     */
    int       dragStart, dragEnd;   /* the word or line a double click grabbed */
    int       reveal;           /* scroll the caret into view: REVEAL_*       */
    RC_ClipboardToken pasteToken;
    int       bracketA, bracketB;   /* the pair at the caret, -1 for none      */
    int       wordAt, wordLen;      /* the word under the caret, 0 long if none */
    uint8_t   marks[MARK_BYTES];    /* one line's backgrounds while it draws  */

    /* What the declared UI asked for this frame, run at the top of the next (commands.h). */
    int       queued[8];
    int       nQueued;

    /* Dialogs. */
    int       closeWhy;
    char      closeTab[16];     /* a posted close's tab, by id: indexes move  */
    char      closeId[16];      /* the tab a close is waiting on              */
    char      saveId[16];       /* the tab a save dialog is for               */
    int       saveTop;          /* its undo position when the bytes went out  */
    char      conflictId[16];   /* a conflict's own: a save dialog may be up  */
    bool      closeOpen, conflictOpen, aboutOpen, settingsOpen, alertOpen;
    char      alert[160];       /* why a save failed                          */
    RC_FileDialogToken askFile, askFolder, askSaveAs;

    /* The SVG preview. */
    bool      previewOn;
    int       route, ground, tint;
    float     svgSize;
} AppState;

enum { REVEAL_NONE, REVEAL_NEAREST, REVEAL_CENTER };

static inline Doc *active_doc(AppState *st) {
    return st->active >= 0 ? &st->docs[st->active] : NULL;
}

/* A tab by its stable id: an index moves when an earlier tab closes. */
static inline Doc *doc_by_id(AppState *st, const char *id) {
    int i;

    for (i = 0; i < st->nDocs; i++)
        if (same_text(st->docs[i].scrollId, id))
            return &st->docs[i];
    return NULL;
}

static inline void out_log(AppState *st, int level, const char *text) {
    LogLine *l;

    if (st->nLog == MAX_LOG) {
        int i;

        for (i = 1; i < MAX_LOG; i++)
            st->log[i - 1] = st->log[i];
        st->nLog--;
    }
    l = &st->log[st->nLog++];
    l->time  = st->now;
    l->level = level;
    rcStrCopy(l->text, text, sizeof l->text);
}

#endif /* APP_STATE_H */
