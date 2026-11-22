/*
    browser.h - where the user is, what is selected, and every command

    The screen files draw this state and call these functions; nothing here declares an
    element. A command that frees a node, reuses its name or lists a folder only records
    the wish: apply_pending() carries it out at the top of the next frame, because text
    already declared this frame still reads those names until the frame is drawn.

    A real file is left alone until the user acts, and then only through rcMakeDirectory
    and rcRemovePath, behind a confirmation. The sample drive changes in memory.
*/
#ifndef APP_BROWSER_H
#define APP_BROWSER_H

#include "rayclay.h"
#include "app/tree.h"
#include "app/samples.h"

typedef enum { VIEW_ICONS = 0, VIEW_LIST, VIEW_COLUMNS } ViewMode;
typedef enum { SORT_NAME = 0, SORT_DATE, SORT_SIZE, SORT_KIND } SortKey;
typedef enum { CONFIRM_NONE = 0, CONFIRM_DELETE, CONFIRM_EMPTY_TRASH } Confirm;

enum { HISTORY_MAX = 64, COLS_MAX = 32, TOAST_CAP = 160, LOCATION_MAX = ROOT_MAX - 1 };

typedef struct { int dir, start, count; } Column;

typedef struct {
    Tree     tree;
    bool     seeded;
    /* The fixed places: every one of them a node, so the sidebar, history and path bar
       treat them alike. */
    int      sampleRoot, trash, recents, computer, tags[TAG_END];
    int      favorites[6];
    int      locations[LOCATION_MAX];   /* folders the user opened or dropped */
    int      locationCount;
    /* Where the user is. */
    int      place;                     /* the sidebar's entry this came from */
    int      folder;                    /* the folder shown                   */
    int      cursor;                    /* the item the keyboard is on; -1    */
    int      anchor;                    /* where a Shift range starts         */
    int      history[HISTORY_MAX];
    int      historyLen, historyAt;
    /* What the view shows, rebuilt only when dirty. */
    ViewMode view;
    SortKey  sort;
    bool     sortDown;
    bool     showHidden;
    char     search[64];
    bool     searchHere;                /* this folder, not the whole drive   */
    int      rows[NODE_MAX];
    uint8_t  depth[NODE_MAX];
    int      sortBuf[NODE_MAX];
    int      rowCount;
    Column   cols[COLS_MAX];            /* the column view's folders          */
    int      colCount;
    bool     dirty, relist;
    bool     revealCursor;
    float    lastViewWidth;             /* a resize shows the columns' end again */
    int      revealColumns;             /* frames until the columns scroll to the end */
    /* Pointer memory. */
    int      hover;                     /* the row under the pointer this frame */
    int      hoverColumn;
    char     typed[32];                 /* type-to-select: the letters so far */
    double   typedUntil;
    bool     menuOpen;                  /* the context menu, last frame       */
    int      menuTarget;                /* the row it opened on, or -1        */
    int      menuColumn;
    bool     menuUp, menuUpNow;         /* any menu, last frame and this one  */
    /* Chrome. */
    bool     sidebarHidden, infoShown, pathHidden, drawer;
    float    iconSize;
    /* Inline rename. */
    int      renaming;                  /* a node, or -1 */
    bool     renameFocused;
    bool     renameCancel;              /* Esc came while it was up           */
    char     renameBuf[NAME_CAP];
    /* Wishes for the next frame. */
    bool     wantNewFolder, wantDelete, wantEmptyTrash;
    int      wantEject;                 /* a location index + 1 */
    /* Overlays. */
    bool     peek;
    bool     infoOpen;                  /* the Get Info dialog                */
    int      infoNode;
    bool     confirmOpen;
    Confirm  confirm;
    char     toast[TOAST_CAP];
    double   toastUntil;
    /* The picture under the cursor. */
    RC_Image pictures[SAMPLE_COUNT];
    RC_Image peekImage;
    int      peekNode;
    uint16_t mono;                      /* the code face, once one is shown   */
    bool     monoTried;
    /* The folder dialog. */
    RC_FileDialogToken ask;
    bool     openUnsupported;
} AppState;

/* ---- Small reads ---- */

static inline Node *node_of(AppState *st, int id) { return &st->tree.nodes[id]; }

static inline bool in_trash(const AppState *st, int id) {
    return id != st->trash && descends_from(&st->tree, id, st->trash);
}

static inline bool browsing_flat(const AppState *st) {
    return st->search[0] || st->tree.nodes[st->folder].smart != SMART_NONE;
}

/* The view actually drawn: results of a search read as a list, the way the Finder's do. In
   columns the selection is one item, the cursor: rebuild_rows() keeps it so. */
static inline ViewMode shown_view(const AppState *st) {
    return st->search[0] && st->view == VIEW_COLUMNS ? VIEW_LIST : st->view;
}

static inline void say(RC_App *app, AppState *st, const char *msg) {
    rcStrCopy(st->toast, msg, sizeof st->toast);
    st->toastUntil = rcAppTime(app) + 3.5;
}

/* ---- Selection ---- */

static inline void clear_selection(AppState *st) {
    int i;

    for (i = 0; i < st->tree.high; i++) st->tree.nodes[i].selected = false;
}

static inline void select_only(AppState *st, int id) {
    clear_selection(st);
    st->cursor = st->anchor = id;
    if (id >= 0) node_of(st, id)->selected = true;
    st->revealCursor = id >= 0;
}

static inline int selected_count(AppState *st) {
    int i, n = 0;

    for (i = 0; i < st->rowCount; i++) n += node_of(st, st->rows[i])->selected;
    return n;
}

/* Where @p id sits in the rows, or -1. */
static inline int row_index(const AppState *st, int id) {
    int i;

    for (i = 0; i < st->rowCount; i++)
        if (st->rows[i] == id) return i;
    return -1;
}

/* The sidebar's own places: their entries point at them, so they keep their name and stay. */
static inline bool is_fixed(AppState *st, int id) {
    int i;

    if (node_of(st, id)->parent < 0) return true;
    for (i = 0; i < 6; i++)
        if (st->favorites[i] == id) return true;
    return false;
}

/* Shift-click, and Shift with an arrow: everything from the anchor to @p id. */
static inline void select_range(AppState *st, int id) {
    int to = row_index(st, id), from = row_index(st, st->anchor), low, high;

    if (from < 0) from = to;
    low  = from < to ? from : to;
    high = from < to ? to : from;
    clear_selection(st);
    for (; low <= high && low >= 0; low++) node_of(st, st->rows[low])->selected = true;
    st->cursor = id;
    st->revealCursor = true;
}

/* ---- Rows ---- */

/* The order of two siblings. Folders stay on top only when sorting by name, the Finder's
   default for "Keep folders on top". Ties fall to the name, so a sort is deterministic. */
static inline bool sorts_before(const Node *a, const Node *b, SortKey key, bool down) {
    int c = 0;

    if (key == SORT_NAME && a->isDir != b->isDir) return a->isDir;
    if (key == SORT_DATE) c = a->modified < b->modified ? -1 : a->modified > b->modified;
    if (key == SORT_SIZE) c = a->size < b->size ? -1 : a->size > b->size;
    if (key == SORT_KIND) c = name_compare(a->kindName, b->kindName);
    if (!c) c = name_compare(a->name, b->name);
    return down ? c > 0 : c < 0;
}

/* A stable insertion sort: a folder is at most NODE_MAX rows and this runs only when what
   is shown changes, never per frame. */
static inline void sort_ids(const AppState *st, int *ids, int count, SortKey key, bool down) {
    const Node *nodes = st->tree.nodes;
    int         i, j;

    for (i = 1; i < count; i++) {
        int id = ids[i];

        for (j = i; j > 0 && sorts_before(&nodes[id], &nodes[ids[j - 1]], key, down); j--)
            ids[j] = ids[j - 1];
        ids[j] = id;
    }
}

static inline bool hidden_name(const AppState *st, const Node *n) {
    return !st->showHidden && n->name[0] == '.';
}

/* Does a smart folder's rule take this node? They read the sample drive: a real folder has
   no tags, and what it happens to have listed is not what was recent. */
static inline bool smart_takes(const AppState *st, const Node *dir, int id) {
    const Node *c = &st->tree.nodes[id];

    if (!c->used || c->real || c->parent < 0 || in_trash(st, id) || hidden_name(st, c))
        return false;
    return dir->smart == SMART_RECENTS ? !c->isDir : c->tag == dir->tag;
}

/* A smart folder's members, sorted. */
static inline int smart_members(AppState *st, const Node *dir, int *out) {
    int i, n = 0;

    for (i = 0; i < st->tree.high; i++)
        if (smart_takes(st, dir, i)) out[n++] = i;
    sort_ids(st, out, n, st->sort, st->sortDown);
    return n;
}

/* The sorted children of @p dir, written from @p out. */
static inline int gather_children(AppState *st, int dir, int *out) {
    const Node *d = node_of(st, dir);
    int         id, n = 0;

    if (d->smart != SMART_NONE) return smart_members(st, d, out);
    for (id = d->first; id >= 0; id = node_of(st, id)->next)
        if (!hidden_name(st, node_of(st, id))) out[n++] = id;
    sort_ids(st, out, n, st->sort, st->sortDown);
    return n;
}

/* The list view's rows: a folder's children, each open disclosure triangle's children
   right under it. @p buf is the free end of sortBuf, a stack shared by the recursion. */
static inline void flatten(AppState *st, int dir, int depth, int *buf) {
    int count = gather_children(st, dir, buf), i;

    for (i = 0; i < count && st->rowCount < NODE_MAX; i++) {
        Node *n = node_of(st, buf[i]);

        st->depth[st->rowCount] = (uint8_t)(depth > 255 ? 255 : depth);
        st->rows[st->rowCount++] = buf[i];
        /* A smart folder's flat list has no triangles: a folder opened in its own parent's
           list stays shut here, or its children would list as members of a rule they fail. */
        if (n->isDir && n->expanded && !browsing_flat(st)) {
            if (n->real && !n->wasListed) list_folder(&st->tree, buf[i]);
            flatten(st, buf[i], depth + 1, buf + count);
        }
    }
}

/* The column view's folders: the sidebar entry the user came from, down to the folder
   shown, each with its span of rows. */
static inline void build_columns(AppState *st) {
    int chain[COLS_MAX], n = 0, id, k;

    for (id = st->folder; id >= 0 && n < COLS_MAX; id = node_of(st, id)->parent) {
        chain[n++] = id;
        if (id == st->place) break;
    }
    st->colCount = n;
    for (k = 0; k < n; k++) {
        Column *c = &st->cols[k];
        Node   *d;

        c->dir   = chain[n - 1 - k];
        c->start = st->rowCount;
        d = node_of(st, c->dir);
        if (d->real && !d->wasListed) list_folder(&st->tree, c->dir);
        c->count = gather_children(st, c->dir, st->rows + st->rowCount);
        st->rowCount += c->count;
    }
}

/* The whole of what the folder belongs to: its drive, or the Trash. A smart folder has no
   parents to climb, and it draws from the sample drive. */
static inline int search_root(AppState *st) {
    int id = st->folder;

    if (node_of(st, id)->smart != SMART_NONE) return st->sampleRoot;
    while (node_of(st, id)->parent >= 0) id = node_of(st, id)->parent;
    return id;
}

static inline int search_scope(AppState *st) {
    return st->searchHere ? st->folder : search_root(st);
}

/* Search starts wide and names its scope, as the Finder's does, and the scope bar narrows it
   to this folder. It reads what the tree holds: the whole sample drive, and of a real folder
   what has been listed so far. */
static inline void build_results(AppState *st) {
    int         scope = search_scope(st), i, n = 0;
    const Node *dir   = node_of(st, scope);

    for (i = 0; i < st->tree.high; i++) {
        const Node *c = &st->tree.nodes[i];

        if (!c->used || !contains(c->name, st->search) || hidden_name(st, c)) continue;
        if (dir->smart != SMART_NONE ? smart_takes(st, dir, i)
                                     : i != scope && descends_from(&st->tree, i, scope))
            st->rows[n++] = i;
    }
    sort_ids(st, st->rows, n, st->sort, st->sortDown);
    st->rowCount = n;
}

/* After nodes were freed: nothing may keep an id the pool can hand out again. */
static inline void forget_gone(AppState *st) {
    const Node *nodes = st->tree.nodes;
    int         i, kept = 0, at = 0;

    for (i = 0; i < st->historyLen; i++) {
        if (!nodes[st->history[i]].used) continue;
        if (i <= st->historyAt) at = kept;
        st->history[kept++] = st->history[i];
    }
    st->historyLen = kept;
    st->historyAt  = at;
    for (i = 0; i < st->tree.high; i++)
        if (nodes[i].used && nodes[i].home >= 0 && !nodes[nodes[i].home].used)
            st->tree.nodes[i].home = -1;
    if (st->cursor >= 0 && !nodes[st->cursor].used) st->cursor = -1;
    if (st->anchor >= 0 && !nodes[st->anchor].used) st->anchor = -1;
    if (st->peekNode >= 0 && !nodes[st->peekNode].used) {
        rcUnloadImage(&st->peekImage);
        st->peekNode = -1;
    }
    /* A rename, Get Info and the row menu end with their item. Kept, the rename would make a
       folder at its bare name, and each would act on whatever node takes the id next. */
    if (st->renaming >= 0 && !nodes[st->renaming].used) st->renaming = -1;
    if (st->infoNode >= 0 && !nodes[st->infoNode].used) {
        st->infoOpen = false;
        st->infoNode = -1;
    }
    if (st->menuTarget >= 0 && !nodes[st->menuTarget].used) st->menuTarget = -1;
}

/* Bring the rows in line with the state. */
static inline void rebuild_rows(RC_App *app, AppState *st) {
    Node *dir = node_of(st, st->folder);

    if (dir->real && st->relist) list_folder(&st->tree, st->folder);
    st->relist   = false;
    st->rowCount = 0;
    st->colCount = 0;
    if (st->search[0]) {
        build_results(st);
    } else if (st->view == VIEW_COLUMNS) {
        build_columns(st);
    } else if (st->view == VIEW_LIST) {
        flatten(st, st->folder, 0, st->sortBuf);
    } else {
        st->rowCount = gather_children(st, st->folder, st->rows);
    }
    forget_gone(st);
    /* Nothing stays selected out of sight. The cursor keeps a row, or in columns the folder
       shown; and a column draws only the cursor, so the selection is the cursor there. */
    {
        bool columns = shown_view(st) == VIEW_COLUMNS;

        if (st->cursor >= 0 && row_index(st, st->cursor) < 0
            && !(columns && st->cursor == st->folder)) {
            clear_selection(st);
            st->cursor = st->anchor = -1;
        }
        if (columns) {
            clear_selection(st);
            if (st->cursor >= 0) node_of(st, st->cursor)->selected = true;
        }
    }
    rcWindowSetTitle(rcAppMainWindow(app), dir->name);
    st->dirty = false;
}

/* ---- Moving ---- */

static inline void show_folder(AppState *st, int id) {
    if (node_of(st, id)->real) {
        forget_other_folders(&st->tree, id);
        rcUnloadImage(&st->peekImage);
        st->peekNode = -1;
        forget_gone(st);
    }
    st->folder = id;
    if (!descends_from(&st->tree, id, st->place)) st->place = id;
    clear_selection(st);
    st->cursor        = -1;
    st->anchor        = -1;
    st->search[0]     = '\0';
    st->searchHere    = false;
    st->dirty         = true;
    st->relist        = true;
    st->revealColumns = 2;
}

/* Go to a folder as a new step in history. */
static inline void navigate(AppState *st, int id) {
    if (id == st->folder) return;
    show_folder(st, id);
    if (st->historyAt + 1 == HISTORY_MAX) {
        int i;

        for (i = 1; i < HISTORY_MAX; i++) st->history[i - 1] = st->history[i];
        st->historyAt--;
    }
    st->history[++st->historyAt] = id;
    st->historyLen = st->historyAt + 1;
}

/* A sidebar entry: the column view starts over from it. Recents opens newest first, and
   leaving it puts the names back in order, as the Finder keeps its own sort per folder. */
static inline void go_place(AppState *st, int id) {
    if (id == st->recents || st->place == st->recents) {
        st->sort     = id == st->recents ? SORT_DATE : SORT_NAME;
        st->sortDown = id == st->recents;
    }
    st->place = id;
    st->dirty = true;
    navigate(st, id);
    st->drawer = false;
}

/* History never names a folder that has gone: forget_gone() takes it out. */
static inline void go_history(AppState *st, int step) {
    int at = st->historyAt + step;

    if (at < 0 || at >= st->historyLen) return;
    st->historyAt = at;
    show_folder(st, st->history[at]);
    if (st->view == VIEW_COLUMNS) st->cursor = st->folder;
}

static inline bool can_go(const AppState *st, int step) {
    int at = st->historyAt + step;

    return at >= 0 && at < st->historyLen;
}

static inline void go_up(AppState *st) {
    int parent = node_of(st, st->folder)->parent;

    if (parent < 0) return;
    if (st->place == st->folder) st->place = parent;
    navigate(st, parent);
}

/* A step out of a folder that is about to go, so the view never shows what has left. */
static inline void leave_if_inside(AppState *st, int id) {
    if (descends_from(&st->tree, st->folder, id)) navigate(st, node_of(st, id)->parent);
}

/* A click in column @p k: a folder opens as the next column, a file shows its preview. */
static inline void pick_in_column(AppState *st, int k, int id) {
    int dir = st->cols[k].dir;

    if (node_of(st, id)->isDir)  navigate(st, id);
    else if (dir != st->folder)  navigate(st, dir);
    clear_selection(st);
    node_of(st, id)->selected = true;
    st->cursor = st->anchor = id;
    st->revealCursor  = true;
    st->revealColumns = 2;
}

/* What opening means: a folder is entered, and a file gets Quick Look, as no call hands
   a file to the system's own app. */
static inline void open_node(AppState *st, int id) {
    if (node_of(st, id)->isDir) {
        navigate(st, id);
        if (st->view == VIEW_COLUMNS) st->cursor = id;
    } else {
        st->cursor = id;
        st->peek   = true;
    }
}

/* ---- Rename and new folder ---- */

/* The field starts empty with the current name as its placeholder, so what the user types
   replaces the name, Enter alone keeps it, and a file keeps its extension unless a new one
   is typed. */
static inline void start_rename(AppState *st, int id) {
    st->renameBuf[0]  = '\0';
    st->renaming      = id;
    st->renameFocused = false;
    st->renameCancel  = false;
    st->revealCursor  = true;
    rcSetFocus("rename");
}

/* Can this node take a new name now? The field stands in a row, so it needs one on screen.
   No RayClay call renames a file on disk, so a real node can only take its name while it is
   still a new folder, and the Trash keeps what it holds as it was. */
static inline bool can_rename(AppState *st, int id) {
    const Node *n = node_of(st, id);

    if (is_fixed(st, id) || in_trash(st, id) || row_index(st, id) < 0) return false;
    return !n->real || n->pending;
}

static inline void finish_rename(RC_App *app, AppState *st, bool keep) {
    Node *n = node_of(st, st->renaming);
    bool  taken;
    int   id, i;

    id = st->renaming;
    st->renaming = -1;
    st->dirty    = true;
    if (!st->renameBuf[0]) {
        rcStrCopy(st->renameBuf, n->name, sizeof st->renameBuf);
    } else if (!n->isDir && !ext_of(st->renameBuf)[0] && ext_of(n->name)[0]) {
        rcStrCopy(st->renameBuf, rcFormat(rcAppArena(app), "%s.%s", st->renameBuf,
                                          ext_of(n->name)).chars, sizeof st->renameBuf);
    }
    for (i = 0; keep && st->renameBuf[i]; i++)
        if (st->renameBuf[i] == '/' || st->renameBuf[i] == '\\') {
            say(app, st, "A name can't contain a slash.");
            keep = false;
        }
    taken = keep && !same_text(st->renameBuf, n->name) && n->parent >= 0
            && child_named(&st->tree, n->parent, st->renameBuf) >= 0;
    if (taken) {
        say(app, st, rcFormat(rcAppArena(app), "The name \"%s\" is already taken.",
                              st->renameBuf).chars);
        keep = false;
    }
    if (!keep) {
        if (n->pending) {
            node_free(&st->tree, id);
            forget_gone(st);
        }
        return;
    }
    rcStrCopy(n->name, st->renameBuf, sizeof n->name);
    classify(n);
    if (n->pending && n->real) {
        char          path[PATH_CAP];
        RC_FileStatus made = node_path(&st->tree, id, path, sizeof path)
                                 ? rcMakeDirectory(path, 0) : RC_FILE_INVALID;

        if (made != RC_FILE_OK) {
            say(app, st, "The folder could not be created here.");
            node_free(&st->tree, id);
            forget_gone(st);
            return;
        }
    }
    n->pending = false;
    if (node_of(st, st->place)->smart == SMART_NONE) st->revealCursor = true;
}

/* A field the frame stops declaring loses focus, and a field takes its own Esc, so both
   read here as the focus going away: Esc cancels, anything else keeps the name. The field
   blurs later in the frame Esc arrives on, and a quick tap is up again by the next one, so
   the key is read now and remembered. */
static inline void tick_rename(RC_App *app, AppState *st) {
    bool focused;

    if (st->renaming < 0) return;
    focused = rcIsFocused("rename");
    if (focused) st->renameFocused = true;
    if (rcKeyPressedOn("rename", RC_KEY_ESCAPE)) st->renameCancel = true;
    if (rcKeyPressedOn("rename", RC_KEY_ENTER)) {
        rcSetFocus(NULL);
        finish_rename(app, st, true);
    } else if (st->renameFocused && !focused) {
        finish_rename(app, st, !st->renameCancel);
    }
}

static inline bool can_change_folder(const AppState *st) {
    return !descends_from(&st->tree, st->folder, st->trash) && !browsing_flat(st);
}

/* A new folder appears in the list with its field open for typing. On a real folder it
   reaches the disk only when the name is confirmed. */
static inline void make_new_folder(RC_App *app, AppState *st) {
    char name[NAME_CAP];
    int  n = 1, id;

    rcStrCopy(name, "untitled folder", sizeof name);
    while (child_named(&st->tree, st->folder, name) >= 0) {
        RC_String next = rcFormat(rcAppArena(app), "untitled folder %d", ++n);

        rcStrCopy(name, next.chars, sizeof name);
    }
    id = node_add(&st->tree, st->folder, name, true);
    if (id < 0) {
        say(app, st, "There is no room for another item.");
        return;
    }
    node_of(st, id)->pending  = true;
    node_of(st, id)->modified = rcUnixTimeSeconds();
    select_only(st, id);
    start_rename(st, id);
    st->dirty = true;
}

/* ---- Trash, delete, duplicate ---- */

/* Every selected row; in columns, where a folder can be chosen without a row of its own,
   the cursor. The commands walk this copy, as they change the tree the rows were built from. */
static inline int selection_ids(AppState *st, int *out) {
    int i, n = 0;

    for (i = 0; i < st->rowCount; i++)
        if (node_of(st, st->rows[i])->selected) out[n++] = st->rows[i];
    if (!n && st->cursor >= 0 && shown_view(st) == VIEW_COLUMNS) out[n++] = st->cursor;
    return n;
}

/* The row the selection moves to once @p ids have left: the next one, or else the one before.
   A folder's rows sit beside other folders' in columns and in an open list, so it stays
   among the same parent's, and a flat list of results has only neighbors. */
static inline int row_after(AppState *st, const int *ids, int count) {
    int parent = node_of(st, ids[0])->parent;
    int first  = row_index(st, ids[0]), last = row_index(st, ids[count - 1]), i;

    if (first < 0 || last < 0) return -1;
    for (i = last + 1; i < st->rowCount; i++) {
        const Node *n = node_of(st, st->rows[i]);

        if (!n->selected && (browsing_flat(st) || n->parent == parent)) return st->rows[i];
    }
    for (i = first - 1; i >= 0; i--) {
        const Node *n = node_of(st, st->rows[i]);

        if (!n->selected && (browsing_flat(st) || n->parent == parent)) return st->rows[i];
    }
    return -1;
}

/* Into the Trash, keeping where each came from. A folder being shown is stepped out of first,
   so the view stays in its parent, as the Finder's does. */
static inline void trash_selection(RC_App *app, AppState *st) {
    int count = selection_ids(st, st->sortBuf), i, next, moved = 0;

    if (!count) return;
    if (in_trash(st, st->sortBuf[0])
        || (node_of(st, st->sortBuf[0])->real && !is_fixed(st, st->sortBuf[0]))) {
        st->confirm     = CONFIRM_DELETE;
        st->confirmOpen = true;
        return;
    }
    next = row_after(st, st->sortBuf, count);
    for (i = 0; i < count; i++) {
        int   id = st->sortBuf[i];
        Node *n  = node_of(st, id);

        if (is_fixed(st, id)) continue;
        leave_if_inside(st, id);
        n->home = n->parent;
        node_unlink(&st->tree, id);
        node_link(&st->tree, id, st->trash);
        moved++;
    }
    if (!moved) {
        say(app, st, "The sidebar's places stay where they are.");
        return;
    }
    say(app, st, rcFormat(rcAppArena(app), "Moved %d item%s to the Trash.", moved,
                          moved == 1 ? "" : "s").chars);
    select_only(st, next);
    st->dirty = true;
}

static inline void put_back(RC_App *app, AppState *st) {
    int count = selection_ids(st, st->sortBuf), i, done = 0, taken = 0, home = -1;

    for (i = 0; i < count; i++) {
        int   id = st->sortBuf[i];
        Node *n  = node_of(st, id);

        if (n->home < 0 || !node_of(st, n->home)->used || in_trash(st, n->home)) continue;
        if (child_named(&st->tree, n->home, n->name) >= 0) {
            taken++;
            continue;
        }
        leave_if_inside(st, id);
        home = n->home;
        node_unlink(&st->tree, id);
        node_link(&st->tree, id, home);
        done++;
    }
    if (done == 1)
        say(app, st, rcFormat(rcAppArena(app), "Put back in %s.", node_of(st, home)->name).chars);
    else if (done)
        say(app, st, rcFormat(rcAppArena(app), "Put back %d items.", done).chars);
    else if (taken)
        say(app, st, "That name is taken in its old folder, so this stays in the Trash.");
    else if (count)
        say(app, st, "The original folder is gone, so this stays in the Trash.");
    select_only(st, -1);
    st->dirty = true;
}

/* Carried out from the confirmation, on the next frame. A real folder with anything in it
   is refused: this app never removes a tree of the user's files. */
static inline void delete_selection(RC_App *app, AppState *st) {
    int           count = selection_ids(st, st->sortBuf), i;
    int           next  = count ? row_after(st, st->sortBuf, count) : -1;
    RC_FileStatus first = RC_FILE_OK;

    for (i = 0; i < count; i++) {
        static char path[PATH_CAP];
        int         id = st->sortBuf[i];

        /* Selected along with a folder already freed here. */
        if (!node_of(st, id)->used) continue;
        if (node_of(st, id)->real) {
            RC_FileStatus r = node_path(&st->tree, id, path, sizeof path)
                                  ? rcRemovePath(path, 0) : RC_FILE_INVALID;

            if (r != RC_FILE_OK) {
                if (first == RC_FILE_OK) first = r;
                continue;
            }
        }
        leave_if_inside(st, id);
        node_free(&st->tree, id);
    }
    forget_gone(st);
    if (first == RC_FILE_NOT_EMPTY)
        say(app, st, "That folder isn't empty, so it was left alone.");
    else if (first == RC_FILE_DENIED)
        say(app, st, "You don't have permission to delete that.");
    else if (first != RC_FILE_OK)
        say(app, st, "That could not be deleted.");
    if (first == RC_FILE_OK) select_only(st, next >= 0 && node_of(st, next)->used ? next : -1);
    st->dirty  = true;
    st->relist = true;
}

static inline void empty_trash(AppState *st) {
    if (in_trash(st, st->folder)) navigate(st, st->trash);
    while (node_of(st, st->trash)->first >= 0) node_free(&st->tree, node_of(st, st->trash)->first);
    forget_gone(st);
    select_only(st, -1);
    st->dirty = true;
}

/* "Name copy.ext", or "Name copy 2.ext" once that is taken. A long name gives up the end of
   its stem, never its tail: a tail cut off reads the same for every number, so no free name
   would ever turn up. Built in place, as a format on every turn could fill the frame arena. */
static inline void copy_name(AppState *st, const Node *n, char *out) {
    const char *ext  = n->isDir ? "" : ext_of(n->name);
    int         stem = text_len(n->name) - (ext[0] ? text_len(ext) + 1 : 0), k = 1;

    do {
        char tail[NAME_CAP + 16] = " copy";
        int  at = 5, keep;

        if (k > 1) {
            tail[at++] = ' ';
            at += (int)rcFormatIntTo(tail + at, sizeof tail - (size_t)at, k, "");
        }
        if (ext[0]) {
            tail[at++] = '.';
            rcStrCopy(tail + at, ext, sizeof tail - (size_t)at);
        }
        keep = NAME_CAP - 1 - text_len(tail);
        keep = keep < 0 ? 0 : keep < stem ? keep : stem;
        rcStrCopy(out, n->name, (size_t)keep + 1);
        rcStrCopy(out + text_len(out), tail, (size_t)(NAME_CAP - text_len(out)));
        k++;
    } while (child_named(&st->tree, n->parent, out) >= 0);
}

static inline int clone_tree(AppState *st, int id, int parent, const char *name) {
    const Node *from = node_of(st, id);
    int         copy = node_add(&st->tree, parent, name, from->isDir), child;

    if (copy < 0) return -1;
    node_of(st, copy)->size     = from->size;
    node_of(st, copy)->modified = from->modified;
    node_of(st, copy)->body     = from->body;
    node_of(st, copy)->scene    = from->scene;
    for (child = from->first; child >= 0; child = node_of(st, child)->next)
        clone_tree(st, child, copy, node_of(st, child)->name);
    return copy;
}

static inline void duplicate_selection(RC_App *app, AppState *st) {
    int  count = selection_ids(st, st->sortBuf), i, last = -1;
    char name[NAME_CAP];

    for (i = 0; i < count; i++) {
        const Node *n = node_of(st, st->sortBuf[i]);

        if (n->real || n->parent < 0) continue;
        copy_name(st, n, name);
        last = clone_tree(st, st->sortBuf[i], n->parent, name);
    }
    if (last >= 0) select_only(st, last);
    else if (count) say(app, st, "There is no room for another item.");
    st->dirty = true;
}

static inline void copy_path(RC_App *app, AppState *st, int id) {
    static char path[PATH_CAP];

    if (!node_path(&st->tree, id, path, sizeof path)) return;
    rcClipboardSet(path);
    say(app, st, "Path copied.");
}

/* Get Info: a dialog of its own, as the Finder's Cmd+I opens a window. */
static inline void open_info(AppState *st, int id) {
    st->infoNode = id;
    st->infoOpen = true;
}

/* What Get Info reads when nothing names a row: the selected item, or the folder itself. */
static inline int info_target(const AppState *st) {
    return st->cursor >= 0 ? st->cursor : st->folder;
}

/* A view's change of mode. Columns have an item on the cursor from the start, so their
   last pane and their commands have something to act on. */
static inline void set_view(AppState *st, ViewMode mode) {
    st->view  = mode;
    st->dirty = true;
    if (mode == VIEW_COLUMNS && st->cursor < 0) st->cursor = st->folder;
}

/* ---- Locations: real folders ---- */

/* Add a real folder to the sidebar, or find it there, and go to it. A folder the user
   picked on the web or on a phone is RayClay's private copy of it: eject gives that back
   (rcReleaseFile), and on a desktop it is the user's own, and is left alone. */
static inline int add_location(RC_App *app, AppState *st, const char *path) {
    char name[NAME_CAP];
    int  i, id, n;

    for (i = 0; i < st->locationCount; i++) {
        int at = st->locations[i];

        if (same_text(st->tree.rootPath[node_of(st, at)->root - 1], path)) return at;
    }
    if (st->locationCount == LOCATION_MAX || text_len(path) >= PATH_CAP) {
        say(app, st, "Eject a folder from the sidebar first.");
        return -1;
    }
    rcStrCopy(name, base_name(path), sizeof name);
    n = text_len(name);
    if (n > 1 && (name[n - 1] == '/' || name[n - 1] == '\\')) name[n - 1] = '\0';
    id = node_add(&st->tree, -1, name, true);
    if (id < 0) return -1;
    node_of(st, id)->real = true;
    node_of(st, id)->root = (uint8_t)(st->locationCount + 2);
    rcStrCopy(st->tree.rootPath[st->locationCount + 1], path, PATH_CAP);
    st->locations[st->locationCount++] = id;
    return id;
}

static inline void eject_location(AppState *st, int index) {
    int id = st->locations[index], i;

    if (descends_from(&st->tree, st->folder, id)) {
        st->place = st->sampleRoot;
        navigate(st, st->sampleRoot);
    }
    rcReleaseFile(st->tree.rootPath[index + 1]);
    node_free(&st->tree, id);
    forget_gone(st);
    for (i = index; i + 1 < st->locationCount; i++) {
        st->locations[i] = st->locations[i + 1];
        node_of(st, st->locations[i])->root = (uint8_t)(i + 2);
        rcStrCopy(st->tree.rootPath[i + 1], st->tree.rootPath[i + 2], PATH_CAP);
    }
    st->locationCount--;
}

/* A folder dropped on the window opens; a file opens its folder, with the file selected. */
static inline void take_drop(RC_App *app, AppState *st) {
    RC_PathInfo info;
    const char *p = rcDroppedFile(0);
    char        path[PATH_CAP], fileName[NAME_CAP];
    int         id, cut, i;

    /* Only the first item opens. The rest, and a first one that cannot, are let go: on the web
       each is RayClay's copy, held in the page's memory until it is released. */
    for (i = 1; i < rcDroppedFileCount(); i++) rcReleaseFile(rcDroppedFile(i));
    if (!p || text_len(p) >= PATH_CAP) {
        rcReleaseFile(p);
        return;
    }
    rcStrCopy(path, p, sizeof path);
    fileName[0] = '\0';
    if (rcPathInfo(path, &info) == RC_FILE_OK && !info.isDirectory) {
        cut = (int)(base_name(path) - path);
        rcStrCopy(fileName, path + cut, sizeof fileName);
        path[cut] = '\0';
    }
    id = add_location(app, st, path);
    if (id < 0) {
        rcReleaseFile(p);
        return;
    }
    st->place = id;
    navigate(st, id);
    if (fileName[0]) {
        int file;

        /* The listing frees what left the disk, so the rows are rebuilt, and forget_gone
           runs, even when the folder was the one shown. */
        list_folder(&st->tree, id);
        st->dirty = true;
        file = child_named(&st->tree, id, fileName);
        if (file >= 0) select_only(st, file);
    }
}

/* The folder dialog's answer, read every frame. */
static inline void read_dialog(RC_App *app, AppState *st) {
    RC_FileDialogResult r = rcFileDialogPoll(st->ask);
    int                 id;

    switch (r.status) {
    case RC_FILE_DIALOG_DONE:
        st->ask = 0;
        id = add_location(app, st, r.folder);
        if (id >= 0) {
            st->place = id;
            navigate(st, id);
        } else {
            rcReleaseFile(r.folder);    /* a copy no sidebar entry holds is never ejected */
        }
        break;
    case RC_FILE_DIALOG_UNSUPPORTED:
        st->ask = 0;
        st->openUnsupported = true;     /* this host never shows one: the entry goes */
        break;
    case RC_FILE_DIALOG_REFUSED:
        st->ask = 0;
        say(app, st, "The folder dialog could not open just now. Try again.");
        break;
    case RC_FILE_DIALOG_FAILED:
        st->ask = 0;
        say(app, st, "That folder could not be opened.");
        break;
    case RC_FILE_DIALOG_CANCELED:
        st->ask = 0;
        break;
    default:
        break;
    }
}

/* ---- Pictures ---- */

/* A sample picture, decoded the first time something draws it, and kept. */
static inline const RC_Image *scene_image(AppState *st, int scene) {
    static unsigned char bmp[SAMPLE_CAP];   /* 600 KB: static, not on the stack */
    RC_Image            *img = &st->pictures[scene];

    if (!img->handle) {
        int len = sample_bmp(&SCENES[scene], bmp, (int)sizeof bmp);

        *img = rcLoadImageFromMemory(bmp, len);
    }
    return img;
}

/* The picture of a node: a sample's is shared and kept, and a real file's is loaded only
   while it is the one looked at: a load keeps the file's bytes until its first draw, and a
   folder can hold thousands. */
static inline const RC_Image *node_image(AppState *st, int id) {
    const Node *n = node_of(st, id);

    if (n->scene >= 0) return scene_image(st, n->scene);
    if (!n->real || n->kind != KIND_IMAGE) return NULL;
    if (st->peekNode != id) {
        static char path[PATH_CAP];

        rcUnloadImage(&st->peekImage);
        st->peekNode = id;
        if (node_path(&st->tree, id, path, sizeof path)) st->peekImage = rcLoadImage(path);
    }
    return st->peekImage.handle ? &st->peekImage : NULL;
}

/* ---- Start and the top of every frame ---- */

/* The app starts in the Finder's signature view, three folders deep with a picture
   selected, so the first thing seen is a real preview. A window too narrow for columns
   starts on the same folder's parent as a list. */
static inline void seed(AppState *st, bool compact) {
    static const char *const FAVORITE[6] = {
        "Applications", "Desktop", "Documents", "Downloads", "Pictures", "Music"
    };
    Tree *t = &st->tree;
    int   i, docs, rebrand, moodboard, picture, logos;

    st->seeded     = true;
    st->renaming   = -1;
    st->cursor     = -1;
    st->anchor     = -1;
    st->peekNode   = -1;
    st->hover      = -1;
    st->menuTarget = -1;
    st->infoNode   = -1;
    st->iconSize   = 72.0f;
    st->infoShown  = true;
    st->revealColumns = 2;

    st->recents    = node_add(t, -1, "Recents", true);
    node_at(t, st->recents)->smart = SMART_RECENTS;
    st->sampleRoot = node_add(t, -1, "Sample Drive", true);
    seed_rows(t, st->sampleRoot, SAMPLE_ROWS, (int)(sizeof SAMPLE_ROWS / sizeof SAMPLE_ROWS[0]));
    st->trash      = node_add(t, -1, "Trash", true);
    seed_rows(t, st->trash, SAMPLE_TRASH, (int)(sizeof SAMPLE_TRASH / sizeof SAMPLE_TRASH[0]));
    st->computer   = node_add(t, -1, "This Computer", true);
    node_at(t, st->computer)->real = true;
    node_at(t, st->computer)->root = 1;
    rcStrCopy(t->rootPath[0], "/", PATH_CAP);
    for (i = 0; i < 6; i++) st->favorites[i] = child_named(t, st->sampleRoot, FAVORITE[i]);
    for (i = TAG_RED; i < TAG_END; i++) {
        static const char *const NAME[TAG_END] = {
            "", "Red", "Orange", "Yellow", "Green", "Blue", "Purple", "Gray"
        };

        st->tags[i] = node_add(t, -1, NAME[i], true);
        node_at(t, st->tags[i])->smart = SMART_TAG;
        node_at(t, st->tags[i])->tag   = (int8_t)i;
    }

    docs      = st->favorites[2];
    rebrand   = child_named(t, docs, "Harbor Cafe Rebrand");
    moodboard = child_named(t, rebrand, "Moodboard");
    picture   = child_named(t, moodboard, "Dawn over the harbor.jpg");
    /* Where the Trash's first items came from, for Put Back. */
    logos = child_named(t, rebrand, "Logos");
    node_at(t, child_named(t, st->trash, "Old Logo Drafts"))->home = logos;
    node_at(t, child_named(t, st->trash, "Untitled 3.txt"))->home  = st->favorites[1];
    st->place      = docs;
    st->history[0] = compact ? rebrand : moodboard;
    st->historyLen = 1;
    st->folder     = st->history[0];
    st->view       = compact ? VIEW_LIST : VIEW_COLUMNS;
    st->dirty      = true;
    if (!compact) select_only(st, picture);
}

/* What the last frame wished for, now that its text is on screen. */
static inline void apply_pending(RC_App *app, AppState *st) {
    tick_rename(app, st);
    if (st->wantDelete)     delete_selection(app, st);
    if (st->wantEmptyTrash) empty_trash(st);
    if (st->wantEject)      eject_location(st, st->wantEject - 1);
    st->wantDelete = st->wantEmptyTrash = false;
    st->wantEject  = 0;
    if (st->wantNewFolder) {
        st->wantNewFolder = false;
        if (can_change_folder(st)) make_new_folder(app, st);
    }
    read_dialog(app, st);
    if (rcDroppedFileCount()) take_drop(app, st);
    if (st->dirty) rebuild_rows(app, st);
}

#endif /* APP_BROWSER_H */
