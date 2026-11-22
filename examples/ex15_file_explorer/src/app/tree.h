/*
    tree.h - the files this app shows: one tree, two sources

    Every folder and file on screen is a Node in one fixed pool. The sample drive is
    seeded from a table (sample_tree.h) and lives in memory only, so the first run reads
    no disk. A real folder fills its nodes from rcListDirectory and rcPathInfo when the
    user opens it, and refreshes them each time, keeping the ids of what is still there.
    The views never ask which source a node came from: only the few commands that change
    a real file do (browser.h).
*/
#ifndef APP_TREE_H
#define APP_TREE_H

#include "rayclay.h"
#include "app/sample_tree.h"

/* A folder larger than the pool shows its first entries and the status bar says so. */
enum { NODE_MAX = 4096, NAME_CAP = 256, PATH_CAP = 1024, ROOT_MAX = 8 };

typedef enum {
    KIND_FOLDER = 0, KIND_APP, KIND_IMAGE, KIND_PDF, KIND_DOC, KIND_TEXT, KIND_CODE, KIND_SHEET,
    KIND_SLIDES, KIND_AUDIO, KIND_VIDEO, KIND_ARCHIVE, KIND_DISK, KIND_DRAWING, KIND_OTHER
} Kind;

/* What a node without a parent is, beyond a plain root: a smart folder lists its children
   by a rule (browser.h) instead of holding any. */
typedef enum { SMART_NONE = 0, SMART_RECENTS, SMART_TAG } Smart;

typedef struct {
    char          name[NAME_CAP];
    int           parent, first, next;  /* -1 ends a chain                              */
    int           home;                 /* the folder Put Back returns a trashed item to */
    uint64_t      size;
    double        modified;
    const char   *kindName;
    const char   *body;                 /* a sample's text, for its preview             */
    Kind          kind;
    Smart         smart;
    RC_FileStatus listed;               /* a real folder's last listing                 */
    int8_t        tag;                  /* TAG_*: Tags in the info pane, or SMART_TAG's */
    int8_t        scene;                /* a sample picture (samples.h), or -1          */
    uint8_t       root;                 /* 1 + its index in Tree.rootPath; 0 = no root  */
    bool          used, isDir, real;
    bool          wasListed;            /* a real folder listed at least once           */
    bool          expanded;             /* the list view's disclosure triangle          */
    bool          selected;
    bool          seen;                 /* still there, during a refresh                */
    bool          pending;              /* a new folder that has no name yet            */
    bool          cut;                  /* its listing stopped because the pool ran out */
} Node;

typedef struct {
    Node nodes[NODE_MAX];
    int  high;                          /* every used node sits below this              */
    int  nextFree;                      /* no free node sits below this                 */
    char rootPath[ROOT_MAX][PATH_CAP];
} Tree;

/* ---- Text: an example includes no libc, so these few are written here ---- */

static inline int text_len(const char *s) { return s ? (int)rcStringFromCStr(s).length : 0; }

static inline char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c; }

static inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

static inline bool same_text(const char *a, const char *b) {
    for (; *a && *a == *b; a++, b++) {}
    return *a == *b;
}

/* True when @p hay holds @p needle, ignoring ASCII case. */
static inline bool contains(const char *hay, const char *needle) {
    int i, j;

    for (i = 0; hay[i]; i++) {
        for (j = 0; needle[j] && lower(hay[i + j]) == lower(needle[j]); j++) {}
        if (!needle[j]) return true;
    }
    return !needle[0];
}

/* The part after the last separator. Windows paths use '\\', and either one splits on
   every host, as a '\\' inside a Linux file name is rare. */
static inline const char *base_name(const char *path) {
    const char *base = path;

    for (; *path; path++)
        if ((*path == '/' || *path == '\\') && path[1]) base = path + 1;
    return base;
}

/* The extension without its dot, or "": a leading dot names a hidden file. */
static inline const char *ext_of(const char *name) {
    const char *dot = NULL, *p;

    for (p = name; *p; p++)
        if (*p == '.') dot = p;
    return dot && dot != name ? dot + 1 : "";
}

/* The Finder's order: ignoring case, with a run of digits read as a number, so
   "Chapter 2" comes before "Chapter 10". */
static inline int name_compare(const char *a, const char *b) {
    while (*a && *b) {
        if (is_digit(*a) && is_digit(*b)) {
            int la = 0, lb = 0, i;

            while (*a == '0') a++;
            while (*b == '0') b++;
            while (is_digit(a[la])) la++;
            while (is_digit(b[lb])) lb++;
            if (la != lb) return la - lb;
            for (i = 0; i < la; i++)
                if (a[i] != b[i]) return a[i] - b[i];
            a += la;
            b += lb;
            continue;
        }
        if (lower(*a) != lower(*b)) return (unsigned char)lower(*a) - (unsigned char)lower(*b);
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* ---- Kinds ---- */

typedef struct { const char *ext, *name; Kind kind; } KindRow;

static const KindRow KINDS[] = {
    { "app",  "Application",       KIND_APP },     { "png",  "PNG image",       KIND_IMAGE },
    { "jpg",  "JPEG image",        KIND_IMAGE },   { "jpeg", "JPEG image",      KIND_IMAGE },
    { "gif",  "GIF image",         KIND_IMAGE },   { "webp", "WebP image",      KIND_IMAGE },
    { "bmp",  "BMP image",         KIND_IMAGE },   { "pdf",  "PDF document",    KIND_PDF },
    { "doc",  "Word document",     KIND_DOC },     { "docx", "Word document",   KIND_DOC },
    { "rtf",  "Rich text",         KIND_DOC },     { "txt",  "Plain text",      KIND_TEXT },
    { "md",   "Markdown",          KIND_TEXT },    { "bib",  "BibTeX database", KIND_TEXT },
    { "csv",  "CSV table",         KIND_SHEET },   { "xls",  "Spreadsheet",     KIND_SHEET },
    { "xlsx", "Spreadsheet",       KIND_SHEET },   { "ppt",  "Presentation",    KIND_SLIDES },
    { "pptx", "Presentation",      KIND_SLIDES },  { "c",    "C source",        KIND_CODE },
    { "h",    "C header",          KIND_CODE },    { "cpp",  "C++ source",      KIND_CODE },
    { "py",   "Python script",     KIND_CODE },    { "js",   "JavaScript",      KIND_CODE },
    { "json", "JSON",              KIND_CODE },    { "sh",   "Shell script",    KIND_CODE },
    { "html", "HTML",              KIND_CODE },    { "css",  "CSS",             KIND_CODE },
    { "mp3",  "MP3 audio",         KIND_AUDIO },   { "m4a",  "AAC audio",       KIND_AUDIO },
    { "wav",  "WAV audio",         KIND_AUDIO },   { "flac", "FLAC audio",      KIND_AUDIO },
    { "ogg",  "Ogg audio",         KIND_AUDIO },   { "mov",  "QuickTime movie", KIND_VIDEO },
    { "mp4",  "MPEG-4 movie",      KIND_VIDEO },   { "webm", "WebM movie",      KIND_VIDEO },
    { "mkv",  "Matroska movie",    KIND_VIDEO },   { "zip",  "ZIP archive",     KIND_ARCHIVE },
    { "gz",   "Gzip archive",      KIND_ARCHIVE }, { "tar",  "Tar archive",     KIND_ARCHIVE },
    { "7z",   "7-Zip archive",     KIND_ARCHIVE }, { "dmg",  "Disk image",      KIND_DISK },
    { "iso",  "Disk image",        KIND_DISK },    { "pkg",  "Installer",       KIND_DISK },
    { "svg",  "SVG image",         KIND_DRAWING },
};
enum { KIND_ROWS = (int)(sizeof KINDS / sizeof KINDS[0]) };

static inline void classify(Node *n) {
    const char *ext = ext_of(n->name);
    int         i, j;

    n->kind     = n->isDir ? KIND_FOLDER : KIND_OTHER;
    n->kindName = n->isDir ? "Folder" : "Document";
    if (n->isDir && !ext[0]) return;
    for (i = 0; i < KIND_ROWS; i++) {
        for (j = 0; ext[j] && lower(ext[j]) == KINDS[i].ext[j]; j++) {}
        if (ext[j] || KINDS[i].ext[j]) continue;
        /* A folder named like a file is still a folder, an app bundle excepted. */
        if (n->isDir && KINDS[i].kind != KIND_APP) return;
        n->kind     = KINDS[i].kind;
        n->kindName = KINDS[i].name;
        return;
    }
}

/* ---- The pool ---- */

static inline Node *node_at(Tree *t, int id) { return &t->nodes[id]; }

/* Link @p id in as the first child of @p parent. Order means nothing here: the views
   sort what they show. */
static inline void node_link(Tree *t, int id, int parent) {
    t->nodes[id].parent = parent;
    t->nodes[id].next   = t->nodes[parent].first;
    t->nodes[parent].first = id;
}

static inline void node_unlink(Tree *t, int id) {
    int  parent = t->nodes[id].parent;
    int *at;

    if (parent < 0) return;
    at = &t->nodes[parent].first;
    while (*at != id) at = &t->nodes[*at].next;
    *at = t->nodes[id].next;
    t->nodes[id].parent = -1;
}

/* A new node, or -1 when the pool is full or the name does not fit: a cut name would
   address another file on disk. */
static inline int node_add(Tree *t, int parent, const char *name, bool isDir) {
    Node *n;
    int   id;

    if (text_len(name) >= NAME_CAP) return -1;
    for (id = t->nextFree; id < NODE_MAX && t->nodes[id].used; id++) {}
    if (id == NODE_MAX) return -1;
    t->nextFree = id + 1;
    if (t->high < id + 1) t->high = id + 1;
    n = &t->nodes[id];
    *n = RC_LIT(Node){ .parent = -1, .first = -1, .next = -1, .home = -1, .scene = -1,
                       .used = true, .isDir = isDir };
    rcStrCopy(n->name, name, sizeof n->name);
    classify(n);
    if (parent >= 0) {
        n->real = t->nodes[parent].real;
        node_link(t, id, parent);
    }
    return id;
}

/* Forget a node and everything under it. Its text may still be on this frame's draw
   list, so callers do this at the top of a frame (browser.h, apply_pending). */
static inline void node_free(Tree *t, int id) {
    int child = t->nodes[id].first;

    while (child >= 0) {
        int next = t->nodes[child].next;

        node_free(t, child);
        child = next;
    }
    node_unlink(t, id);
    t->nodes[id].used = false;
    if (id < t->nextFree) t->nextFree = id;
}

static inline int child_named(const Tree *t, int dir, const char *name) {
    int id;

    for (id = t->nodes[dir].first; id >= 0; id = t->nodes[id].next)
        if (same_text(t->nodes[id].name, name)) return id;
    return -1;
}

/* True when @p id is @p ancestor or lies under it. */
static inline bool descends_from(const Tree *t, int id, int ancestor) {
    for (; id >= 0; id = t->nodes[id].parent)
        if (id == ancestor) return true;
    return false;
}

/* The path of a node, for the calls that want one. False when it does not fit. A sample
   node's root has no path of its own, so a sample path starts at the drive's name. */
static inline bool node_path(const Tree *t, int id, char *out, int cap) {
    const Node *n = &t->nodes[id];
    int         len, i;

    if (n->root) {
        rcStrCopy(out, t->rootPath[n->root - 1], (size_t)cap);
        return text_len(t->rootPath[n->root - 1]) < cap;
    }
    if (n->parent < 0) {
        rcStrCopy(out, n->name, (size_t)cap);
        return true;
    }
    if (!node_path(t, n->parent, out, cap)) return false;
    len = text_len(out);
    if (len && out[len - 1] != '/' && out[len - 1] != '\\') {
        if (len + 1 >= cap) return false;
        out[len++] = '/';
    }
    for (i = 0; n->name[i]; i++) {
        if (len + 1 >= cap) return false;
        out[len++] = n->name[i];
    }
    out[len] = '\0';
    return true;
}

/* ---- A real folder ---- */

typedef struct {
    Tree *tree;
    int   dir;
    char  path[PATH_CAP];
    int   prefix;           /* the folder's path and its separator */
} Listing;

/* One entry of rcListDirectory. A node still there keeps its id, so history, selection
   and an open disclosure triangle survive a refresh. */
static inline bool take_entry(const char *name, bool isDir, void *user) {
    Listing    *l = (Listing *)user;
    RC_PathInfo info;
    int         id = child_named(l->tree, l->dir, name);

    if (id < 0) id = node_add(l->tree, l->dir, name, isDir);
    if (id < 0) {
        if (text_len(name) >= NAME_CAP) return true;    /* a name too long is skipped */
        node_at(l->tree, l->dir)->cut = true;
        return false;                                   /* a full pool stops the listing */
    }
    rcStrCopy(l->path + l->prefix, name, sizeof l->path - (size_t)l->prefix);
    node_at(l->tree, id)->seen  = true;
    node_at(l->tree, id)->isDir = isDir;
    /* A path too long for the buffer is cut short, and the cut one can name another file. */
    if (l->prefix + text_len(name) < PATH_CAP && rcPathInfo(l->path, &info) == RC_FILE_OK) {
        node_at(l->tree, id)->size     = info.size;
        node_at(l->tree, id)->modified = info.modifiedUnixSeconds;
    }
    classify(node_at(l->tree, id));
    return true;
}

/* The pool is a window on the disk, not a copy of it: once it is half full, every real
   folder off the path to @p keep lets go of what it listed, to be listed again on demand. */
static inline void forget_other_folders(Tree *t, int keep) {
    int used = 0, i, id;

    for (i = 0; i < t->high; i++) used += t->nodes[i].used;
    if (used < NODE_MAX / 2) return;
    for (id = keep; id >= 0 && t->nodes[id].parent >= 0; id = t->nodes[id].parent) {
        int sibling;

        for (sibling = t->nodes[t->nodes[id].parent].first; sibling >= 0;
             sibling = t->nodes[sibling].next) {
            if (sibling == id) continue;
            while (t->nodes[sibling].first >= 0) node_free(t, t->nodes[sibling].first);
            t->nodes[sibling].wasListed = false;
        }
    }
}

/* List a real folder into its node, as the disk has it now. Sorting is the view's: the
   file system's own order means nothing (rcListDirectory). */
static inline void list_folder(Tree *t, int dir) {
    static Listing l;   /* holds a whole path: static, not on the stack */
    int            id, next;

    t->nodes[dir].cut = false;
    /* One byte short of the buffer: the separator below may follow the path. */
    if (!node_path(t, dir, l.path, sizeof l.path - 1)) {
        t->nodes[dir].listed = RC_FILE_INVALID;
        return;
    }
    l.tree   = t;
    l.dir    = dir;
    l.prefix = text_len(l.path);
    if (l.prefix && l.path[l.prefix - 1] != '/' && l.path[l.prefix - 1] != '\\')
        l.path[l.prefix++] = '/';
    l.path[l.prefix] = '\0';
    for (id = t->nodes[dir].first; id >= 0; id = t->nodes[id].next) t->nodes[id].seen = false;
    t->nodes[dir].listed = rcListDirectory(l.path, take_entry, &l);
    t->nodes[dir].wasListed = true;
    for (id = t->nodes[dir].first; id >= 0; id = next) {
        next = t->nodes[id].next;
        if (!t->nodes[id].seen) node_free(t, id);
    }
}

/* ---- The sample drive ---- */

/* Read one outline into the tree under @p under. Each row's parent is the last folder
   seen one level up. */
static inline void seed_rows(Tree *t, int under, const SampleRow *rows, int count) {
    int stack[8], i;

    stack[0] = under;
    for (i = 0; i < count; i++) {
        const SampleRow *r   = &rows[i];
        int              len = text_len(r->name);
        char             name[NAME_CAP];
        Node            *n;
        int              id;

        rcStrCopy(name, r->name, sizeof name);
        if (name[len - 1] == '/') name[len - 1] = '\0';
        id = node_add(t, stack[r->depth], name, r->name[len - 1] == '/');
        if (id < 0) return;
        n = node_at(t, id);
        rcTimeParse(r->when, (size_t)text_len(r->when), &n->modified);
        n->size  = r->size;
        n->tag   = (int8_t)r->tag;
        n->scene = (int8_t)r->scene;
        n->body  = r->body;
        stack[r->depth + 1] = id;
    }
}

#endif /* APP_TREE_H */
