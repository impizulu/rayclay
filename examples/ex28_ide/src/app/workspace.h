/*
    workspace.h - the open folder, opening and saving files, find in files

    The folder is walked once with rcListDirectory and held as one array in display order. That
    one array serves the tree, the quick-open palette and find in files.

    THE ONE WRITE IN THIS APP IS rcSaveFile IN doc_write, reached only from an explicit Save: the
    command, the unsaved-changes dialog's Save or a conflict's Overwrite. It is atomic: a crash
    mid-save leaves the old file whole. A save first checks the file's time, and asks before
    writing over a file that changed behind the editor's back.

    A FILE'S BYTES STAY FOR THE RUN. A Node keeps rcLoadFile's buffer as the find-in-files cache,
    and reads again only when the file's time moves.
*/
#ifndef APP_WORKSPACE_H
#define APP_WORKSPACE_H

#include "rayclay.h"
#include "app/samples.h"
#include "app/state.h"

static inline const char *file_status_text(RC_FileStatus s) {
    switch (s) {
    case RC_FILE_OK:        return "ok";
    case RC_FILE_NOT_FOUND: return "no such file";
    case RC_FILE_DENIED:    return "permission denied";
    case RC_FILE_FULL:      return "the disk is full";
    case RC_FILE_BUSY:      return "the file is in use";
    case RC_FILE_WRONG_TYPE: return "that is a folder";
    default:                return "the file could not be read or written";
    }
}

static inline char *names_copy(Workspace *ws, const char *a, const char *b) {
    int   na = str_len(a), nb = b ? str_len(b) : 0, sep = b && na ? 1 : 0;
    char *out = (char *)rcArenaAlloc(&ws->names, (size_t)(na + sep + nb + 1));

    if (!out)
        return NULL;
    move_bytes(out, a, na);
    if (sep)
        out[na] = '/';
    if (nb)
        move_bytes(out + na + sep, b, nb);
    out[na + sep + nb] = '\0';
    return out;
}

static inline bool collect_entry(const char *name, bool isDirectory, void *user) {
    Workspace *ws = (Workspace *)user;
    char      *copy;

    if (name[0] == '.' || ws->stackTop == MAX_NODES)    /* dot files stay out of the tree */
        return true;
    copy = names_copy(ws, name, NULL);
    if (copy) {
        ws->stack[ws->stackTop].name = copy;
        ws->stack[ws->stackTop].dir  = isDirectory;
        ws->stackTop++;
    }
    return true;
}

/* The file system lists in its own order. Folders first, then by name, the same on every disk,
   which is what makes the default run's tree the same everywhere. */
static inline bool entry_after(const Entry *a, const Entry *b) {
    return a->dir != b->dir ? b->dir : compare_names(a->name, b->name) > 0;
}

static inline void sort_entries(Entry *e, int n) {
    int i, j;

    for (i = 1; i < n; i++) {
        Entry key = e[i];

        for (j = i; j > 0 && entry_after(&e[j - 1], &key); j--)
            e[j] = e[j - 1];
        e[j] = key;
    }
}

/* Depth first, so a folder's subtree follows it in the array. */
static inline void walk(Workspace *ws, const char *path, int depth) {
    int base = ws->stackTop, i;

    rcListDirectory(path, collect_entry, ws);
    sort_entries(ws->stack + base, ws->stackTop - base);
    for (i = base; i < ws->stackTop && ws->count < MAX_NODES; i++) {
        Entry e    = ws->stack[i];
        int   self = ws->count;
        Node *n    = &ws->node[self];
        char *full = names_copy(ws, path, e.name);

        if (!full)
            break;
        *n = RC_LIT(Node){ .path = full, .name = e.name, .depth = (uint8_t)depth, .dir = e.dir };
        ws->count++;
        ws->files   += !e.dir;
        ws->folders += e.dir;
        if (e.dir && depth < MAX_DEPTH)
            walk(ws, full, depth + 1);
        ws->node[self].end = ws->count;
    }
    ws->stackTop = base;
}

/* The rows the tree draws: every node, jumping over each folded folder. */
static inline void tree_rows(Workspace *ws) {
    int i = 0;

    ws->nRows = 0;
    while (i < ws->count) {
        const Node *n = &ws->node[i];

        ws->rows[ws->nRows++] = i;
        i = n->dir && !n->open ? n->end : i + 1;
    }
}

static inline void search_start(AppState *st) {
    Search *s = &st->search;

    rcStrCopy(s->seen, s->query, sizeof s->seen);
    s->n = s->filesHit = s->matches = 0;
    s->next = 0;
    s->running = s->query[0] != '\0';
    st->ws.reread = false;
}

/* A new walk: every result and pick held an index into the old one, so they start over. The
   folders that were open stay open, by path, so a Refresh leaves the tree as it was. */
static inline bool workspace_open(AppState *st, const char *path) {
    Workspace  *ws = &st->ws;
    RC_PathInfo info;
    RC_Arena    old;
    const char *wasOpen[256];
    int         nOpen = 0, i, k;

    if (rcPathInfo(path, &info) != RC_FILE_OK || !info.isDirectory)
        return false;
    for (i = 0; i < ws->count && nOpen < COUNT_OF(wasOpen); i++)
        if (ws->node[i].dir && ws->node[i].open)
            wasOpen[nOpen++] = ws->node[i].path;
    old = ws->names;
    for (i = 0; i < ws->count; i++)         /* a file read from disk holds rcLoadFile's bytes */
        if (ws->node[i].read && !ws->node[i].inMemory)
            rcFreeFile((void *)ws->node[i].bytes);
    ws->names = rcArenaInit(4u << 20);
    ws->count = ws->files = ws->folders = ws->stackTop = 0;
    ws->samples = false;
    rcStrCopy(ws->rootPath, path, sizeof ws->rootPath);
    rcStrCopy(ws->rootName, base_name(path), sizeof ws->rootName);
    walk(ws, path, 0);
    for (i = 0; i < ws->count; i++)
        for (k = 0; k < nOpen && !ws->node[i].open; k++)
            ws->node[i].open = ws->node[i].dir && same_text(ws->node[i].path, wasOpen[k]);
    rcArenaFree(&old);
    tree_rows(ws);
    search_start(st);
    st->pal.seen[0] = 1;        /* no query equals this, so the next frame lists */
    st->pal.seen[1] = '\0';
    st->revealNode  = -1;
    st->openNode    = -1;       /* a row's open, still queued, named a node of the old walk */
    st->shownDoc[0] = '\0';
    out_log(st, SEV_INFO, rcFormat(st->arena, "Opened %s: %d files in %d folders", ws->rootName,
                                   ws->files, ws->folders).chars);
    if (ws->count == MAX_NODES)
        out_log(st, SEV_WARNING, rcFormat(st->arena, "The folder has more than %d entries: the "
                                          "rest are not shown", MAX_NODES).chars);
    return true;
}

/* No folder to read: the sample project, its bytes in memory. */
static inline void workspace_samples(AppState *st) {
    Workspace *ws = &st->ws;
    int        i;

    rcArenaFree(&ws->names);
    ws->names = rcArenaInit(4096);
    ws->count = ws->folders = 0;
    ws->samples = true;
    rcStrCopy(ws->rootPath, "", sizeof ws->rootPath);
    rcStrCopy(ws->rootName, "harbor-tide", sizeof ws->rootName);
    for (i = 0; i < COUNT_OF(SAMPLES); i++) {
        ws->node[ws->count++] = RC_LIT(Node){ .path = SAMPLES[i].name, .name = SAMPLES[i].name,
                                              .bytes = SAMPLES[i].bytes, .size = SAMPLES[i].size,
                                              .end = i + 1, .read = true, .inMemory = true };
    }
    ws->files = ws->count;
    tree_rows(ws);
    out_log(st, SEV_INFO, "No examples/ folder here: opened the sample project in memory");
}

static inline int node_for_path(const Workspace *ws, const char *path) {
    int i;

    for (i = 0; i < ws->count; i++)
        if (same_text(ws->node[i].path, path))
            return i;
    return -1;
}

/* Unfold every folder above a node, so the tree can show it. */
static inline void tree_reveal(Workspace *ws, int node) {
    int i;

    for (i = 0; i < node; i++)
        if (ws->node[i].dir && ws->node[i].end > node)
            ws->node[i].open = true;
    tree_rows(ws);
}

/* A file's bytes, read once and read again only when its time moves. */
static inline bool node_read(Workspace *ws, Node *n) {
    RC_PathInfo info;
    void       *bytes;
    size_t      size;

    if (n->inMemory)
        return true;
    if (rcPathInfo(n->path, &info) != RC_FILE_OK || info.size > (uint64_t)MAX_FILE_BYTES)
        return false;
    if (n->read && info.modifiedUnixSeconds == n->time)
        return true;
    if (rcLoadFile(n->path, &bytes, &size) != RC_FILE_OK)
        return false;
    ws->reread = ws->reread || n->read;     /* results held offsets into the old bytes */
    if (n->read)
        rcFreeFile((void *)n->bytes);
    n->bytes = (const char *)bytes;
    n->size  = (int)size;
    n->time  = info.modifiedUnixSeconds;
    n->read  = true;
    return true;
}

static inline bool looks_binary(const char *bytes, int n) {
    int i;

    for (i = 0; i < n && i < 8192; i++)
        if (bytes[i] == '\0')
            return true;
    return false;
}

/* A new tab with this name and path, empty. It takes the next stable id, so its scroll
   container is its own: returning to a tab restores the place through .scrollOffset. */
static inline Doc *doc_new(AppState *st, const char *name, const char *path) {
    Doc *d;

    if (st->nDocs == MAX_TABS) {
        out_log(st, SEV_WARNING, "Too many open editors: close one first");
        return NULL;
    }
    d = &st->docs[st->nDocs];
    *d = RC_LIT(Doc){ .lang = lang_for(name), .savedTop = 0 };
    rcStrCopy(d->name, name, sizeof d->name);
    rcStrCopy(d->path, path, sizeof d->path);
    rcStrCopy(d->scrollId, rcFormat(st->arena, "code%u", st->nextDocId++).chars,
              sizeof d->scrollId);
    d->wantX = -1.0f;
    st->active = st->nDocs++;
    return d;
}

static inline void doc_fill(Doc *d, const char *bytes, int size, const char *why) {
    if (why) {
        d->unreadable = true;
        rcStrCopy(d->notice, why, sizeof d->notice);
        doc_set_text(d, "", 0);
    } else {
        doc_set_text(d, bytes, size);
    }
}

static inline int tab_of(const AppState *st, const char *path) {
    int i;

    for (i = 0; i < st->nDocs; i++)
        if (same_text(st->docs[i].path, path))
            return i;
    return -1;
}

/* The one way a file becomes a tab: from the tree, the palette, a dialog or a drop. */
static inline Doc *open_file(AppState *st, const char *path) {
    int   node = node_for_path(&st->ws, path), tab = tab_of(st, path);
    Doc  *d;
    Node  loose = { .path = path };     /* a file outside the folder: read straight from disk */
    Node *n = node >= 0 ? &st->ws.node[node] : &loose;

    if (tab >= 0) {
        st->active = tab;
        return &st->docs[tab];
    }
    d = doc_new(st, base_name(path), path);
    if (!d)
        return NULL;
    /* A refused file has no size to report: the node keeps whatever an earlier read left. */
    if (!node_read(&st->ws, n)) {
        doc_fill(d, NULL, 0, "This file is too large or cannot be read.");
        out_log(st, SEV_WARNING, rcFormat(st->arena, "Could not read %s", path).chars);
    } else {
        if (looks_binary(n->bytes, n->size))
            doc_fill(d, NULL, 0, "This file is binary, so it has no text to edit.");
        else
            doc_fill(d, n->bytes, n->size, NULL);
        out_log(st, SEV_INFO, rcFormat(st->arena, "Opened %s (%d bytes)", path, n->size).chars);
    }
    d->diskTime   = n->time;
    d->memoryOnly = n->inMemory || st->ws.imported;     /* an imported folder is a copy too */
    if (n == &loose && loose.read)      /* no node keeps these bytes: the tab has its copy */
        rcFreeFile((void *)loose.bytes);
    return d;
}

/* Write the buffer: the app's only write. */
static inline bool doc_write(AppState *st, Doc *d) {
    RC_FileStatus s = rcSaveFile(d->path, d->text, (size_t)d->len, 0);
    RC_PathInfo   info;

    if (s != RC_FILE_OK) {
        const char *why = rcFormat(st->arena, "Could not save %s: %s", d->path,
                                   file_status_text(s)).chars;

        out_log(st, SEV_ERROR, why);
        rcStrCopy(st->alert, why, sizeof st->alert);
        st->alertOpen = true;
        return false;
    }
    if (rcPathInfo(d->path, &info) == RC_FILE_OK)
        d->diskTime = info.modifiedUnixSeconds;
    d->savedTop = d->top;
    out_log(st, SEV_INFO, rcFormat(st->arena, "Saved %s (%d bytes)", d->path, d->len).chars);
    return true;
}

/* Has the file moved since this buffer read it? */
static inline bool disk_changed(const Doc *d) {
    RC_PathInfo info;

    return d->path[0] && rcPathInfo(d->path, &info) == RC_FILE_OK
        && info.modifiedUnixSeconds != d->diskTime;
}

/* Back to the file as it is on disk, or the sample as shipped. A revert is one undoable edit. */
static inline void doc_revert(AppState *st, Doc *d) {
    int   node = node_for_path(&st->ws, d->path);
    void *bytes = NULL;
    size_t size = 0;
    RC_PathInfo info = { .size = 0 };   /* read below when no path was looked at */
    bool  done;

    if (node >= 0 && st->ws.samples) {
        done = doc_edit(d, 0, d->len, st->ws.node[node].bytes, st->ws.node[node].size,
                        EDIT_OTHER);
    } else if (d->memoryOnly && !d->imported && (node < 0 || !st->ws.imported)) {
        return;     /* a sample whose project closed, or a file of a released import */
    } else if (d->path[0] && rcPathInfo(d->path, &info) == RC_FILE_OK
               && info.size <= (uint64_t)DOC_BYTES
               && rcLoadFile(d->path, &bytes, &size) == RC_FILE_OK) {
        /* The size bound is the buffer's (doc_edit): a file grown past it is not read whole,
           and its length is never cut to fit an int. */
        done = doc_edit(d, 0, d->len, (const char *)bytes, (int)size, EDIT_OTHER);
        rcFreeFile(bytes);                  /* the document keeps its own copy */
        if (done && rcPathInfo(d->path, &info) == RC_FILE_OK)
            d->diskTime = info.modifiedUnixSeconds;
    } else {
        if (info.size > (uint64_t)DOC_BYTES)
            d->atLimit = true;      /* a reached limit says so (state.h) */
        return;
    }
    if (!done)              /* refused: the buffer is still the edited one, not the file */
        return;
    d->savedTop = d->top;
    d->caret = d->anchor = 0;
}

static inline bool text_at(const char *s, const char *q, int qn, bool matchCase) {
    int k;

    for (k = 0; k < qn; k++)
        if (matchCase ? s[k] != q[k] : lower(s[k]) != lower(q[k]))
            return false;
    return true;
}

/* Find in files: the next match in one line at or after `from`, or -1. */
static inline int find_in_line(const char *s, int n, const char *q, int qn, bool matchCase,
                               int from) {
    int i;

    for (i = from; i + qn <= n; i++)
        if (text_at(s + i, q, qn, matchCase))
            return i;
    return -1;
}

/* One file's worth of results, a heading and then a row per match. */
static inline void search_file(AppState *st, int node) {
    Search     *s  = &st->search;
    Node       *n  = &st->ws.node[node];
    int         qn = str_len(s->query), at = 0, ln = 0, head = -1;

    if (n->dir || !node_read(&st->ws, n) || looks_binary(n->bytes, n->size))
        return;
    while (at < n->size && s->n < MAX_RESULTS) {
        int end = at, col;

        while (end < n->size && n->bytes[end] != '\n')
            end++;
        col = find_in_line(n->bytes + at, end - at, s->query, qn, s->matchCase, 0);
        while (col >= 0 && s->n < MAX_RESULTS - 1) {
            if (head < 0) {
                head = s->n;
                s->results[s->n++] = RC_LIT(Result){ .node = node, .head = true };
                s->filesHit++;
            }
            s->results[head].matches++;
            s->results[s->n++] = RC_LIT(Result){ .node = node, .line = ln, .col = col, .at = at,
                                                 .len = end - at };
            s->matches++;
            col = find_in_line(n->bytes + at, end - at, s->query, qn, s->matchCase, col + qn);
        }
        at = end + 1;
        ln++;
    }
}

/* A few dozen files a frame keeps the first results instant and the window live. */
static inline void search_step(AppState *st) {
    Search *s = &st->search;
    int     budget = 48;

    if (!s->running)
        return;
    while (s->next < st->ws.count && budget-- > 0)
        search_file(st, s->next++);
    if (s->next >= st->ws.count) {
        s->running = false;
        out_log(st, SEV_INFO, rcFormat(st->arena, "Search for \"%s\": %d matches in %d files",
                                       s->query, s->matches, s->filesHit).chars);
        if (s->n >= MAX_RESULTS - 1)
            out_log(st, SEV_WARNING, rcFormat(st->arena, "The search stopped at %d results",
                                              MAX_RESULTS).chars);
    }
    rcWindowRequestFrame(rcAppMainWindow(st->app));
}

#endif /* APP_WORKSPACE_H */
