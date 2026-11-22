/*
    file_selector.h - the Open panel of a desktop app, drawn by the app itself

    THE ONE CALL TO COPY. This file stands alone: copy it, keep a FileSelector in
    your state, name the places its sidebar lists, open it when the user asks to
    choose a file, and declare it where it shows - a centered modal on a desktop,
    the whole screen on a phone (app.h does both):

        file_selector_add_place(&st->picker, "Favorites", "Documents", rcIconFileText, docs);
        file_selector_open(&st->picker, 0, "", NULL, FILE_TYPES, FILE_TYPE_COUNT);
        ...
        if (file_selector(app, &st->picker, st->path, (int)sizeof st->path))
            ...                       st->path is the file picked, this frame

    It fills the box it is declared in, takes its colors from rcGetStyle(), and
    hides itself (st->picker.open goes false) on Cancel, on Esc or Android's Back,
    and on a pick. Set st->picker.inset where that box is the whole window: a modal
    pads its body, and a window does not.

    WHY NOT rcFileDialogOpen. The system's dialog (ex15_file_explorer) is the one
    for the user's own documents. This one is for the files the APP keeps - its
    rcAppDataPath, where neither a phone's dialog nor a browser's ever looks - and
    for a picker that must look and behave the same on every host.

    ONE LISTING PER FOLDER. rcListDirectory hands over names in the file system's
    order and rcPathInfo reads each one's size and date; both happen once, when
    the folder opens, into fixed arrays. The search, the file type and the sort
    then only reorder an index array, and the rows draw through rcVirtualList, so
    a folder of 4,000 entries costs a frame what 10 do.

    WHAT THE USER CHANGES TAKES EFFECT ON THE NEXT FRAME. A row's name is drawn
    from the listing's own bytes, and text is borrowed until the frame is drawn,
    so a click that opened a folder in mid-declaration would rewrite the names of
    the rows above it. Every change is recorded and applied at the top of the next
    call instead, and the call asks for that frame.

    THE TABLE NEVER SCROLLS SIDEWAYS. Its columns are declared in order of
    importance, and it shows as many as leave the name its minimum width. What
    did not fit is named by a small "+N columns" tip at its top right.
*/
#ifndef APP_FILE_SELECTOR_H
#define APP_FILE_SELECTOR_H

#include "rayclay.h"

#include "icons/rc_icons_arrow_down.h"
#include "icons/rc_icons_arrow_up.h"
#include "icons/rc_icons_chart_column.h"
#include "icons/rc_icons_check.h"
#include "icons/rc_icons_chevron_left.h"
#include "icons/rc_icons_chevron_right.h"
#include "icons/rc_icons_circle_alert.h"
#include "icons/rc_icons_circle_play.h"
#include "icons/rc_icons_code.h"
#include "icons/rc_icons_file_text.h"
#include "icons/rc_icons_folder_solid.h"
#include "icons/rc_icons_image.h"
#include "icons/rc_icons_search.h"
#include "icons/rc_icons_volume_2.h"

/* A folder lists at most FS_ENTRY_MAX entries whose names take FS_NAMES_CAP
   bytes; past either the listing stops and the count says so. */
enum {
    FS_PATH_CAP    = 1024,
    FS_ENTRY_MAX   = 4096,
    FS_NAMES_CAP   = 128 * 1024,
    FS_FILTER_CAP  = 64,
    FS_TYPE_MAX    = 8,
    FS_PLACE_MAX   = 12,
    FS_HISTORY_MAX = 16
};

/* BRANCH ON ITS OWN WIDTH, as a container query does. The preview pane goes first and
   the sidebar next, each before the table has to give up a column; below FS_TABLE_AT
   the entries are a phone's list of two-line rows. A presentation that may be narrower
   than the list's width is the app's to rule out: app.h sizes its modal from this. */
enum {
    FS_PREVIEW_AT = 1020,  /* the preview pane shows from this width */
    FS_SIDEBAR_AT = 780,   /* the sidebar; a list of places below it */
    FS_TABLE_AT   = 512,
    FS_TITLE_AT   = 420    /* below this height a panel with no sidebar has no title */
};

/* A VIRTUAL LIST NEEDS EVERY ROW EXACTLY ITS PITCH TALL, so the row shapes are
   constants, and a finger gets a taller one. */
enum {
    FS_ROW_H       = 30,
    FS_ROW_TOUCH   = 44,
    FS_ROW_TALL    = 60,   /* a two-line row                                  */
    FS_LIST_PAD    = 6,    /* the table's inset around its rows               */
    FS_ROW_PAD     = 10,   /* a row's own inset, so a name clears the wash    */
    FS_COL_GAP     = 12,
    FS_NAME_MIN    = 180,  /* the width the name column never goes below      */
    FS_TIP_W       = 72,   /* the "+N columns" tip                            */
    FS_SIDEBAR_W   = 176,
    FS_PREVIEW_W   = 232,
    FS_THUMB       = 132,  /* vector art's side in the preview pane           */
    FS_VECTOR_MAX  = 480,  /* vector art is drawn no larger: it is an icon    */
    FS_CRUMB_PAD   = 28,   /* what a crumb's button adds to its label         */
    FS_CRUMB_SEP   = 18    /* the chevron after it, and the bar's gaps        */
};

/* The type scale: dense data at 13, its labels at 12 and 11, as a desktop's
   native list is set. A phone's rows are read at arm's length, so they step up. */
enum { FS_PX_TITLE = 15, FS_PX_TEXT = 13, FS_PX_SMALL = 12, FS_PX_LABEL = 11,
       FS_PX_PHONE_TITLE = 18, FS_PX_PHONE = 15 };

/* U+00B7, the middle dot, as its UTF-8 bytes, which keeps the source ASCII. */
#define FS_DOT "\xc2\xb7"

/* What an entry is: the name a Kind column shows, and its icon and tint. A tint
   is mid-lightness, so it reads on a light style and on a dark one. */
typedef struct {
    const char     *exts;    /* lower case and space separated: "jpg jpeg" */
    const char     *label;
    RC_IconCallback icon;
    RC_Color        tint;
} FsKind;

#define FS_TINT_FOLDER  {  88, 170, 245, 255 }
#define FS_TINT_PICTURE {  72, 196, 142, 255 }
#define FS_TINT_MEDIA   { 236, 104, 150, 255 }
#define FS_TINT_CODE    { 240, 160,  72, 255 }
#define FS_TINT_DATA    { 150, 142, 246, 255 }
#define FS_TINT_PAGE    { 150, 154, 162, 255 }

static const FsKind FS_KIND_FOLDER = { "", "Folder", rcIconFolderSolid, FS_TINT_FOLDER };
static const FsKind FS_KIND_OTHER  = { "", "Document", rcIconFileText, FS_TINT_PAGE };

static const FsKind FS_KINDS[] = {
    { "png",            "PNG image",       rcIconImage,      FS_TINT_PICTURE },
    { "jpg jpeg",       "JPEG image",      rcIconImage,      FS_TINT_PICTURE },
    { "gif",            "GIF image",       rcIconImage,      FS_TINT_PICTURE },
    { "bmp",            "BMP image",       rcIconImage,      FS_TINT_PICTURE },
    { "webp",           "WebP image",      rcIconImage,      FS_TINT_PICTURE },
    { "svg",            "SVG image",       rcIconImage,      FS_TINT_PICTURE },
    { "mp4 m4v mov",    "MPEG-4 movie",    rcIconCirclePlay, FS_TINT_MEDIA   },
    { "webm mkv",       "WebM movie",      rcIconCirclePlay, FS_TINT_MEDIA   },
    { "mp3",            "MP3 audio",       rcIconVolume2,    FS_TINT_MEDIA   },
    { "m4a",            "AAC audio",       rcIconVolume2,    FS_TINT_MEDIA   },
    { "wav",            "WAV audio",       rcIconVolume2,    FS_TINT_MEDIA   },
    { "ogg",            "Ogg audio",       rcIconVolume2,    FS_TINT_MEDIA   },
    { "flac",           "FLAC audio",      rcIconVolume2,    FS_TINT_MEDIA   },
    { "m3u",            "Playlist",        rcIconVolume2,    FS_TINT_MEDIA   },
    { "c",              "C source",        rcIconCode,       FS_TINT_CODE    },
    { "h",              "C header",        rcIconCode,       FS_TINT_CODE    },
    { "cpp cc hpp",     "C++ source",      rcIconCode,       FS_TINT_CODE    },
    { "js",             "JavaScript",      rcIconCode,       FS_TINT_CODE    },
    { "ts",             "TypeScript",      rcIconCode,       FS_TINT_CODE    },
    { "py",             "Python script",   rcIconCode,       FS_TINT_CODE    },
    { "sh",             "Shell script",    rcIconCode,       FS_TINT_CODE    },
    { "cmake",          "CMake script",    rcIconCode,       FS_TINT_CODE    },
    { "html htm",       "Web page",        rcIconCode,       FS_TINT_CODE    },
    { "css",            "Style sheet",     rcIconCode,       FS_TINT_CODE    },
    { "json",           "JSON data",       rcIconCode,       FS_TINT_DATA    },
    { "csv",            "CSV spreadsheet", rcIconChartColumn, FS_TINT_DATA   },
    { "xml",            "XML data",        rcIconCode,       FS_TINT_DATA    },
    { "md",             "Markdown",        rcIconFileText,   FS_TINT_PAGE    },
    { "txt",            "Plain text",      rcIconFileText,   FS_TINT_PAGE    },
    { "pdf",            "PDF document",    rcIconFileText,   FS_TINT_PAGE    },
    { "ttf otf",        "Font",            rcIconFileText,   FS_TINT_PAGE    },
};

/* One choice of the file-type list: its label, and the extensions it keeps,
   lower case and space separated. NULL keeps every file. Folders always show, so
   a filtered folder can still be walked. */
typedef struct {
    const char *label;
    const char *exts;
} FileType;

/* One row of the sidebar. A section's name is shown where it changes. */
typedef struct {
    const char     *section;
    const char     *label;
    RC_IconCallback icon;
    char            path[FS_PATH_CAP];
} FsPlace;

/* The columns IN ORDER OF IMPORTANCE: the table drops them from the end. */
typedef enum { FS_BY_NAME = 0, FS_BY_DATE, FS_BY_SIZE, FS_BY_KIND, FS_BY_COUNT } FsSort;

typedef struct {
    int           name;      /* the name's offset in FileSelector.names        */
    bool          folder;
    const FsKind *kind;
    uint64_t      size;
    double        modified;  /* seconds since 1970, UTC; 0 = not known         */
} FsEntry;

/* A picture for a preview: loaded when the path changes and let go when it changes
   again, drawn as often as the frame asks. Vector art too: rcSvg would keep every
   file it was shown for the whole run, and draw none past its 1,024th. */
typedef struct {
    char     path[FS_PATH_CAP];
    RC_Image image;
    RC_Svg  *svg;
    bool     vector;
} FilePreview;

/* A folder visited, for Back and Forward. */
typedef struct {
    int  place;
    char rel[FS_PATH_CAP];
} FsStop;

/* Everything the selector knows, kept by the app: zero is a closed selector
   sorted by name. Large (about 350 KB, most of it the listing), so keep it static. */
typedef struct {
    bool            open;                     /* shown; the app reads it       */
    float           inset;                    /* the panel's own margin; 0 in a modal */
    bool            menuOpen;                 /* a list of its own is down     */
    FsPlace         places[FS_PLACE_MAX];
    const char     *placeLabel[FS_PLACE_MAX]; /* the narrow panel's list       */
    int             placeCount;
    int             here;                     /* the place it is in            */
    char            rel[FS_PATH_CAP];         /* the folder below it, "" at it */
    int             nextPlace;                /* where the next frame goes     */
    char            next[FS_PATH_CAP];
    char            from[FS_PATH_CAP];        /* the name to pick on arrival   */
    bool            going;
    bool            remember;                 /* the trip is a new history stop */
    FsStop          history[FS_HISTORY_MAX];
    int             historyCount, historyAt;
    const FileType *types;
    const char     *typeLabel[FS_TYPE_MAX];   /* the list's items              */
    int             typeCount, type;
    char            filter[FS_FILTER_CAP];
    FsSort          sort;
    bool            desc;
    RC_FileStatus   status;                   /* the last listing's answer     */
    FsEntry         entry[FS_ENTRY_MAX];
    int             count;
    bool            full;                     /* the listing stopped early     */
    char            names[FS_NAMES_CAP];
    int             namesUsed;
    int             view[FS_ENTRY_MAX];       /* entries shown, in order       */
    int             shown;
    int             pick;                     /* the selected entry, or -1     */
    bool            dirty;                    /* view[] is out of date         */
    bool            reveal;                   /* scroll the list to the pick   */
    bool            again;                    /* a change wants the next frame */
    bool            submit;                   /* a double click asks to open   */
    bool            entered;                  /* the last click opened a folder */
    FilePreview     preview;
} FileSelector;

/* ---- Strings, with no libc: an example includes rayclay.h alone ---- */

static inline int fs_len(const char *s) {
    int n = 0;

    while (s[n]) n++;
    return n;
}

static inline char fs_lower(char c) {
    return (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
}

static inline bool fs_is_sep(char c) {
    return c == '/' || c == '\\';
}

/* a, a separator and b into dst: a alone for an empty b, b alone for an a of
   "." (the working directory's name for itself), "." for both empty. A
   separator a already ends in (rcAppDataPath ends in one) is not doubled.
   False, with dst emptied, when the path does not fit. */
static inline bool fs_join(char *dst, int cap, const char *a, const char *b) {
    int  la, lb, n = 0, i;
    bool sep;

    if (a[0] == '.' && !a[1])
        a = "";
    la  = fs_len(a);
    lb  = fs_len(b);
    sep = la > 0 && lb > 0 && !fs_is_sep(a[la - 1]);
    if (la + (sep ? 1 : 0) + lb + 2 > cap) {
        dst[0] = '\0';
        return false;
    }
    for (i = 0; i < la; i++) dst[n++] = a[i];
    if (sep) dst[n++] = '/';
    for (i = 0; i < lb; i++) dst[n++] = b[i];
    if (!n) dst[n++] = '.';
    dst[n] = '\0';
    return true;
}

/* The last part of a path. */
static inline const char *fs_base_name(const char *path) {
    const char *name = path;
    int         i;

    for (i = 0; path[i]; i++)
        if (fs_is_sep(path[i]) && path[i + 1]) name = path + i + 1;
    return name;
}

/* Case-blind, then byte order, so "notes" and "Notes" still have one order. */
static inline int fs_compare_names(const char *a, const char *b) {
    int i;

    for (i = 0; a[i] && fs_lower(a[i]) == fs_lower(b[i]); i++) {}
    if (fs_lower(a[i]) != fs_lower(b[i]))
        return (unsigned char)fs_lower(a[i]) < (unsigned char)fs_lower(b[i]) ? -1 : 1;
    for (i = 0; a[i] && a[i] == b[i]; i++) {}
    return (unsigned char)a[i] < (unsigned char)b[i] ? -1 : a[i] == b[i] ? 0 : 1;
}

/* Where the search text starts in the name, case-blind, or -1. */
static inline int fs_find(const char *name, const char *filter) {
    int i, j;

    for (i = 0; name[i]; i++) {
        for (j = 0; filter[j] && name[i + j]; j++)
            if (fs_lower(name[i + j]) != fs_lower(filter[j])) break;
        if (!filter[j]) return i;
    }
    return filter[0] ? -1 : 0;
}

/* The extension after a name's last dot ("" for none, and for ".profile"). */
static inline const char *fs_ext(const char *name) {
    const char *dot = NULL;
    int         i;

    if (!name[0]) return "";
    for (i = 1; name[i]; i++)
        if (name[i] == '.') dot = name + i + 1;
    return dot ? dot : "";
}

/* Is the name's extension one of a space-separated list's? */
static inline bool fs_has_ext(const char *name, const char *exts) {
    const char *ext = fs_ext(name);
    int         n   = fs_len(ext), i = 0;

    while (n && exts[i]) {
        int len = 0, k;

        while (exts[i + len] && exts[i + len] != ' ') len++;
        for (k = 0; k < n && k < len && fs_lower(ext[k]) == exts[i + k]; k++) {}
        if (k == n && len == n) return true;
        i += len;
        while (exts[i] == ' ') i++;
    }
    return false;
}

/* The kind a file's name says it is. */
static inline const FsKind *fs_kind_of(const char *name) {
    int i;

    for (i = 0; i < (int)(sizeof FS_KINDS / sizeof FS_KINDS[0]); i++)
        if (fs_has_ext(name, FS_KINDS[i].exts)) return &FS_KINDS[i];
    return &FS_KIND_OTHER;
}

/* ---- The preview: a picture from a path, loaded once ---- */

static inline bool fs_is_raster(const char *name) {
    return fs_has_ext(name, "png jpg jpeg gif bmp webp");
}

/* Show `path`, or nothing for "" or a file that is not a picture. rcUnloadImage
   and rcUnloadSvg are safe with the old picture still declared this frame. */
static inline void file_preview_set(FilePreview *p, const char *path) {
    const char *name = fs_base_name(path);

    if (fs_compare_names(p->path, path) == 0) return;
    if (p->image.handle) rcUnloadImage(&p->image);
    rcUnloadSvg(&p->svg);
    p->vector = fs_has_ext(name, "svg");
    rcStrCopy(p->path, path, sizeof p->path);
    if (fs_is_raster(name)) p->image = rcLoadImage(path);
    if (p->vector) p->svg = rcLoadSvg(path);
}

static inline bool file_preview_ready(const FilePreview *p) {
    return p->vector || p->image.handle;
}

/* Draw it in the box it is declared in. A picture fills that box and RC_IMAGE_SCALE_DOWN
   shows it whole, at its own size or smaller, since stretching one past its pixels only
   blurs it. Vector art is drawn square at a size it has to be given: `side`, and no more
   than FS_VECTOR_MAX. */
static inline void file_preview_draw(const FilePreview *p, float side) {
    if (side < 16.0f) side = 16.0f;
    if (p->vector)
        rcSvgHandle(p->svg, side < (float)FS_VECTOR_MAX ? side : (float)FS_VECTOR_MAX,
                    rcGetStyle().text);
    else
        rcBox(.w = "grow", .h = "grow", .image = &p->image,
              .imageFit = RC_IMAGE_SCALE_DOWN) {}
}

/* ---- The listing ---- */

/* rcListDirectory's callback: one name into the pool. A name starting with a
   dot is hidden, as every open panel hides it. */
static inline bool fs_take(const char *name, bool isDirectory, void *user) {
    FileSelector *fs = (FileSelector *)user;
    int           n  = fs_len(name), i;
    FsEntry      *e;

    if (name[0] == '.') return true;
    if (fs->count == FS_ENTRY_MAX || fs->namesUsed + n + 1 > FS_NAMES_CAP) {
        fs->full = true;
        return false;
    }
    e = &fs->entry[fs->count++];
    e->name     = fs->namesUsed;
    e->folder   = isDirectory;
    e->kind     = &FS_KIND_OTHER;
    e->size     = 0;
    e->modified = 0.0;
    for (i = 0; i <= n; i++) fs->names[fs->namesUsed + i] = name[i];
    fs->namesUsed += n + 1;
    return true;
}

static inline const char *fs_name(const FileSelector *fs, int entry) {
    return fs->names + fs->entry[entry].name;
}

static inline const char *fs_root(const FileSelector *fs) {
    return fs->places[fs->here].path;
}

/* List the current folder, then read each entry's size and date: the listing
   gives a name and whether it is a folder, and rcPathInfo the rest. */
static inline void fs_list(FileSelector *fs) {
    char dir[FS_PATH_CAP], path[FS_PATH_CAP];
    int  i;

    fs->count = fs->namesUsed = 0;
    fs->full  = false;
    fs->pick  = -1;
    fs_join(dir, (int)sizeof dir, fs_root(fs), fs->rel);
    fs->status = rcListDirectory(dir, fs_take, fs);
    for (i = 0; i < fs->count; i++) {
        FsEntry    *e = &fs->entry[i];
        RC_PathInfo info;

        if (fs_join(path, (int)sizeof path, dir, fs_name(fs, i))
            && rcPathInfo(path, &info) == RC_FILE_OK) {
            e->folder   = info.isDirectory;
            e->size     = info.size;
            e->modified = info.modifiedUnixSeconds;
        }
        e->kind = e->folder ? &FS_KIND_FOLDER : fs_kind_of(fs_name(fs, i));
    }
}

/* Folders first in either direction, then the sort column, then the name. */
static inline int fs_order(const FileSelector *fs, int a, int b) {
    const FsEntry *x = &fs->entry[a], *y = &fs->entry[b];
    int            c = 0;

    if (x->folder != y->folder) return x->folder ? -1 : 1;
    if (fs->sort == FS_BY_SIZE && x->size != y->size)
        c = x->size < y->size ? -1 : 1;
    else if (fs->sort == FS_BY_DATE && x->modified != y->modified)
        c = x->modified < y->modified ? -1 : 1;
    else if (fs->sort == FS_BY_KIND)
        c = fs_compare_names(x->kind->label, y->kind->label);
    if (!c) c = fs_compare_names(fs_name(fs, a), fs_name(fs, b));
    return fs->desc ? -c : c;
}

/* Heapsort, because an example uses no libc - YOUR app should call qsort. It
   allocates nothing, never recurses, and its n log n is a worst case. */
static inline void fs_sift(FileSelector *fs, int hole, int n) {
    int held = fs->view[hole];

    for (;;) {
        int child = 2 * hole + 1;

        if (child >= n) break;
        if (child + 1 < n && fs_order(fs, fs->view[child + 1], fs->view[child]) > 0) child++;
        if (fs_order(fs, fs->view[child], held) <= 0) break;
        fs->view[hole] = fs->view[child];
        hole = child;
    }
    fs->view[hole] = held;
}

/* Where an entry sits in view[], or -1 where the search hides it. */
static inline int fs_row_of(const FileSelector *fs, int entry) {
    int i;

    for (i = 0; i < fs->shown; i++)
        if (fs->view[i] == entry) return i;
    return -1;
}

/* The entries the search and the file type keep, in the sort's order. While a
   search is typed the first match is picked, so Enter opens it. */
static inline void fs_rebuild(FileSelector *fs) {
    const char *exts = fs->typeCount ? fs->types[fs->type].exts : NULL;
    int         i;

    fs->shown = 0;
    for (i = 0; i < fs->count; i++) {
        const char *name = fs_name(fs, i);

        if (fs_find(name, fs->filter) < 0) continue;
        if (!fs->entry[i].folder && exts && !fs_has_ext(name, exts)) continue;
        fs->view[fs->shown++] = i;
    }
    for (i = fs->shown / 2 - 1; i >= 0; i--) fs_sift(fs, i, fs->shown);
    for (i = fs->shown - 1; i > 0; i--) {
        int top = fs->view[0];

        fs->view[0] = fs->view[i];
        fs->view[i] = top;
        fs_sift(fs, 0, i);
    }
    if (fs_row_of(fs, fs->pick) < 0)
        fs->pick = fs->filter[0] && fs->shown ? fs->view[0] : -1;
}

/* ---- Moving around: each is a request, carried out by fs_settle ---- */

static inline void fs_go(FileSelector *fs, int place, const char *rel) {
    fs->nextPlace = place;
    rcStrCopy(fs->next, rel, sizeof fs->next);
    fs->from[0]  = '\0';
    fs->going    = true;
    fs->remember = true;
}

static inline void fs_enter(FileSelector *fs, int entry) {
    char rel[FS_PATH_CAP];

    if (fs_join(rel, (int)sizeof rel, fs->rel, fs_name(fs, entry))) fs_go(fs, fs->here, rel);
}

/* Up one folder, remembering the one it came out of so that row is picked
   again, as a desktop's open panel does. */
static inline void fs_up(FileSelector *fs) {
    char rel[FS_PATH_CAP];
    int  i, cut = 0;

    if (!fs->rel[0]) return;
    for (i = 0; fs->rel[i]; i++)
        if (fs_is_sep(fs->rel[i])) cut = i;
    rcStrCopy(rel, fs->rel, sizeof rel);
    rel[cut] = '\0';
    fs_go(fs, fs->here, rel);
    rcStrCopy(fs->from, fs->rel + (cut ? cut + 1 : 0), sizeof fs->from);
}

/* The path bar's crumb `depth` (0 = the place): rel cut after that many names. */
static inline void fs_go_depth(FileSelector *fs, int depth) {
    char rel[FS_PATH_CAP];
    int  i, n = 0;

    rcStrCopy(rel, fs->rel, sizeof rel);
    for (i = 0; rel[i]; i++)
        if (fs_is_sep(rel[i]) && ++n == depth) break;
    if (!depth) i = 0;
    rel[i] = '\0';
    fs_go(fs, fs->here, rel);
}

/* Back and Forward walk the stops already made, and make no new one. */
static inline void fs_go_stop(FileSelector *fs, int at) {
    if (at < 0 || at >= fs->historyCount) return;
    fs_go(fs, fs->history[at].place, fs->history[at].rel);
    fs->remember  = false;
    fs->historyAt = at;
}

/* A new stop drops the ones Forward could have reached, and the oldest when the
   list is full. The folder it is already in is no new stop. */
static inline void fs_remember(FileSelector *fs) {
    int i;

    if (fs->historyAt >= 0 && fs->history[fs->historyAt].place == fs->here
        && fs_compare_names(fs->history[fs->historyAt].rel, fs->rel) == 0)
        return;
    if (fs->historyAt == FS_HISTORY_MAX - 1) {
        for (i = 1; i < FS_HISTORY_MAX; i++) fs->history[i - 1] = fs->history[i];
        fs->historyAt--;
    }
    fs->historyAt++;
    fs->historyCount = fs->historyAt + 1;
    fs->history[fs->historyAt].place = fs->here;
    rcStrCopy(fs->history[fs->historyAt].rel, fs->rel, sizeof fs->history[0].rel);
}

/* Add a row to the sidebar. `section` and `label` are borrowed: pass literals. */
static inline void file_selector_add_place(FileSelector *fs, const char *section,
                                           const char *label, RC_IconCallback icon,
                                           const char *path) {
    FsPlace *p;

    if (fs->placeCount == FS_PLACE_MAX) return;
    p = &fs->places[fs->placeCount];
    p->section = section;
    p->label   = label;
    p->icon    = icon;
    rcStrCopy(p->path, path, sizeof p->path);
    fs->placeLabel[fs->placeCount++] = label;
}

/* Show the selector in place number `place`, in the folder `start` below it (""
   for the place itself), with the entry `name` picked if it is there (NULL for
   none). `types` are the file-type list's choices, the first selected; NULL for
   none. The sort, and a type already chosen, carry over from the last time. */
static inline void file_selector_open(FileSelector *fs, int place, const char *start,
                                      const char *name, const FileType *types, int typeCount) {
    int i;

    if (place < 0 || place >= fs->placeCount) return;
    fs->types     = types;
    fs->typeCount = types ? (typeCount < FS_TYPE_MAX ? typeCount : FS_TYPE_MAX) : 0;
    for (i = 0; i < fs->typeCount; i++) fs->typeLabel[i] = types[i].label;
    if (fs->type >= fs->typeCount) fs->type = 0;
    fs->historyCount = 0;
    fs->historyAt    = -1;
    fs_go(fs, place, start ? start : "");
    rcStrCopy(fs->from, name ? name : "", sizeof fs->from);
    fs->open = true;
}

/* Last frame's requests, before anything is declared. */
static inline void fs_settle(FileSelector *fs) {
    if (fs->going) {
        int i;

        fs->here = fs->nextPlace;
        rcStrCopy(fs->rel, fs->next, sizeof fs->rel);
        fs->going     = false;
        fs->filter[0] = '\0';
        if (fs->remember) fs_remember(fs);
        fs_list(fs);
        for (i = 0; fs->from[0] && i < fs->count; i++)
            if (fs_compare_names(fs_name(fs, i), fs->from) == 0) fs->pick = i;
        fs_rebuild(fs);
        fs->dirty  = false;
        fs->reveal = fs->pick >= 0;
        if (rcGetScrollInfo("fs_list").found)  /* a list declared anew opens at the top */
            rcScrollToTop("fs_list");
    }
    if (fs->dirty) {
        fs_rebuild(fs);
        fs->dirty = false;
    }
}

/* Keep the preview on the pick, once a frame, before anything is declared. */
static inline void fs_sync_preview(FileSelector *fs) {
    char dir[FS_PATH_CAP], path[FS_PATH_CAP];

    if (fs->pick < 0 || fs->entry[fs->pick].folder
        || !fs_join(dir, (int)sizeof dir, fs_root(fs), fs->rel)
        || !fs_join(path, (int)sizeof path, dir, fs_name(fs, fs->pick)))
        path[0] = '\0';
    file_preview_set(&fs->preview, path);
}

/* Scroll the list the least that shows the pick whole. The row may be one rcVirtualList
   has not declared: rcScrollToRow places it from the list's own pitch and inset, after
   this frame's layout, so a list declared anew this frame is the one it scrolls. */
static inline void fs_reveal(const FileSelector *fs) {
    int row = fs_row_of(fs, fs->pick);

    if (row >= 0) rcScrollToRow("fs_list", row, RC_SCROLL_NEAREST);
}

/* The picked file's whole path into out; false if it does not fit. */
static inline bool fs_pick_path(const FileSelector *fs, char *out, int cap) {
    char dir[FS_PATH_CAP];

    return fs_join(dir, (int)sizeof dir, fs_root(fs), fs->rel)
        && fs_join(out, cap, dir, fs_name(fs, fs->pick));
}

/* Open the pick: a folder is entered, a file is the answer. */
static inline bool fs_activate(FileSelector *fs, char *out, int cap) {
    if (fs->pick < 0) return false;
    if (fs->entry[fs->pick].folder) {
        fs_enter(fs, fs->pick);
        return false;
    }
    if (!fs_pick_path(fs, out, cap)) return false;
    fs->open = false;
    return true;
}

/* TYPING GOES TO THE SEARCH, as in every open panel. The focus moves on the
   key's own frame, before the field is declared, and the field then takes that
   key's character with the rest of the frame's typing, so none is lost. A
   shortcut (a command key held) is not typing. */
static inline bool fs_typing(void) {
    int k;

    if (rcModDown(RC_MOD_PRIMARY) || rcModDown(RC_MOD_CTRL) || rcModDown(RC_MOD_ALT)
        || rcModDown(RC_MOD_SUPER))
        return false;
    for (k = RC_KEY_A; k <= RC_KEY_9; k++)       /* the letters, then the digits */
        if (rcKeyPressed((RC_Key)k)) return true;
    return rcKeyPressed(RC_KEY_PERIOD) || rcKeyPressed(RC_KEY_MINUS);
}

/* Move the pick by `by` rows, stopping at the ends. With nothing picked, down counts
   from just above the first row and up from just below the last. */
static inline void fs_step(FileSelector *fs, int by) {
    int row = fs_row_of(fs, fs->pick);

    if (!fs->shown) return;
    row = row < 0 ? (by > 0 ? by - 1 : fs->shown + by) : row + by;
    if (row < 0) row = 0;
    if (row >= fs->shown) row = fs->shown - 1;
    fs->pick   = fs->view[row];
    fs->reveal = true;
}

/* The keyboard of every open panel. The arrows, Page Up and Page Down, Home and
   End move the pick; Enter opens it; Backspace or Left goes up a folder and Right
   enters one, unless the search has the keyboard; Alt with an arrow walks the
   history; Esc (Android's Back) cancels. A focused field and an open dialog take
   Esc themselves, so a press closes one thing. While one of its lists is down the
   table is out of reach, so no key changes a pick the user cannot see. */
static inline bool fs_keys(FileSelector *fs, char *out, int cap, float pitch) {
    RC_Box box     = rcGetElementBox("fs_list");
    int    page    = box.found ? (int)(box.height / pitch) - 1 : 8;
    bool   inField = rcIsFocused("fs_filter");

    if (fs->menuOpen) return false;
    /* The list's keys turn the keyboard to the list, as a press on a row would: a place, a
       heading, a chip or a button left focused would take the Enter that follows and press
       itself again instead of opening the pick. The search keeps its own keys. */
    if (!inField && (rcKeyPressed(RC_KEY_UP) || rcKeyPressed(RC_KEY_DOWN)
                     || rcKeyPressed(RC_KEY_LEFT) || rcKeyPressed(RC_KEY_RIGHT)
                     || rcKeyPressed(RC_KEY_PAGE_UP) || rcKeyPressed(RC_KEY_PAGE_DOWN)
                     || rcKeyPressed(RC_KEY_HOME) || rcKeyPressed(RC_KEY_END)
                     || rcKeyPressed(RC_KEY_BACKSPACE)))
        rcSetFocus(NULL);
    if (rcModDown(RC_MOD_ALT)) {
        if (rcKeyPressed(RC_KEY_LEFT))  fs_go_stop(fs, fs->historyAt - 1);
        if (rcKeyPressed(RC_KEY_RIGHT)) fs_go_stop(fs, fs->historyAt + 1);
        if (rcKeyPressed(RC_KEY_UP))    fs_up(fs);
    } else {
        if (rcKeyPressed(RC_KEY_DOWN))      fs_step(fs, 1);
        if (rcKeyPressed(RC_KEY_UP))        fs_step(fs, -1);
        if (rcKeyPressed(RC_KEY_PAGE_DOWN)) fs_step(fs, page > 1 ? page : 1);
        if (rcKeyPressed(RC_KEY_PAGE_UP))   fs_step(fs, -(page > 1 ? page : 1));
        if (!inField) {
            if (rcKeyPressed(RC_KEY_HOME)) fs_step(fs, -fs->shown);
            if (rcKeyPressed(RC_KEY_END))  fs_step(fs, fs->shown);
            if (rcKeyPressed(RC_KEY_LEFT)) fs_up(fs);
            if (rcKeyPressed(RC_KEY_RIGHT) && fs->pick >= 0 && fs->entry[fs->pick].folder)
                fs_enter(fs, fs->pick);
        }
    }
    if (rcKeyPressed(RC_KEY_BACKSPACE) && !inField) fs_up(fs);
    if (rcKeyPressed(RC_KEY_ESCAPE)) fs->open = false;
    if (rcKeyPressed(RC_KEY_F) && rcModDown(RC_MOD_PRIMARY)) rcSetFocus("fs_filter");
    if (!inField && fs_typing()) rcSetFocus("fs_filter");
    return rcKeyPressed(RC_KEY_ENTER) && fs_activate(fs, out, cap);
}

/* ---- Pieces ---- */

/* The one height every control is given, the fields, the buttons and fs_menu's pop-up menus
   alike, and the rows that stand beside them, so a row of them lines up: h-8, 32 px, and h-11,
   44 px under a finger. A class's h-* is exact, so the pointer picks the string, and either
   pick is a literal the class engine resolves once. The control's own utilities follow the
   height: FS_CONTROL("justify-between"), or FS_CONTROL("") for the height alone. */
#define FS_CONTROL(more) (rcPointerIsCoarse() ? "h-11 " more : "h-8 " more)

/* rcClicked for a place, a heading or a sort chip, a .focusable box that Enter and Space press.
   A press on one leaves the keys with the list, as a native panel's sidebar and headings do:
   the box would keep the focus, and the Enter that follows the arrow keys would press it again
   instead of opening the pick. A key's click keeps the focus (rcClickCount reads 0 for one),
   so Tab goes on from there until a list key (fs_keys) turns the keyboard to the list. */
static inline bool fs_clicked(const char *id) {
    if (!rcClicked(id)) return false;
    if (rcClickCount() > 0) rcSetFocus(NULL);
    return true;
}

/* "12 KB", the decimal units a desktop's own file manager shows. */
static inline RC_String fs_size_text(RC_Arena *m, uint64_t n) {
    if (n < 1000u)       return rcFormat(m, "%u bytes", (unsigned)n);
    if (n < 1000000u)    return rcFormat(m, "%.0f KB", (double)n / 1e3);
    if (n < 1000000000u) return rcFormat(m, "%.1f MB", (double)n / 1e6);
    return rcFormat(m, "%.1f GB", (double)n / 1e9);
}

/* A moment's calendar day on the user's clock, counted from 1970: the local date's number, so
   two of them differ by the days a calendar shows between them. */
static inline int64_t fs_local_day(double t, const RC_LocalTime *at) {
    const int64_t s = (int64_t)t + at->utcOffsetSeconds;

    return (s >= 0 ? s : s - 86399) / 86400;
}

/* "Today at 18:21", "Yesterday at 18:21", "Nov 5, 2026 at 18:21", in the user's time zone,
   as a desktop's own file manager shows a date. */
static inline RC_String fs_date_text(RC_Arena *m, double t) {
    static const char *const MONTH[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    const double now = rcUnixTimeSeconds();
    RC_LocalTime at, today;
    int64_t      age;

    if (t <= 0.0 || !rcLocalTime(t, &at) || !rcLocalTime(now, &today)) return rcStringFromCStr("");
    age = fs_local_day(now, &today) - fs_local_day(t, &at);
    if (age == 0) return rcFormat(m, "Today at %02d:%02d", at.hour, at.minute);
    if (age == 1) return rcFormat(m, "Yesterday at %02d:%02d", at.hour, at.minute);
    return rcFormat(m, "%s %d, %d at %02d:%02d", MONTH[at.month - 1], at.day, at.year, at.hour,
                    at.minute);
}

/* What a failed listing means, in words; fs->status keeps the code for the app. */
static inline const char *fs_status_text(RC_FileStatus st) {
    switch (st) {
    case RC_FILE_NOT_FOUND:   return "It was moved or deleted.";
    case RC_FILE_WRONG_TYPE:  return "It is a file, not a folder.";
    case RC_FILE_DENIED:      return "You do not have permission to open it.";
    case RC_FILE_BUSY:        return "Another program is using it.";
    case RC_FILE_INVALID:     return "Its path is not one this system accepts.";
    case RC_FILE_UNSUPPORTED: return "This build cannot list folders.";
    case RC_FILE_OK: case RC_FILE_NOT_EMPTY: case RC_FILE_FULL: case RC_FILE_IO: break;
    }
    return "It could not be read.";
}

/* A hairline: a 1 px border, which is one device pixel at any zoom. */
static inline void fs_rule(void) {
    rcBox(.w = "grow", .hType = RC_PX(1), .className = "border-t border-border") {}
}

/* ---- The sidebar ---- */

static inline void fs_sidebar(RC_Arena *m, FileSelector *fs) {
    RC_Style    s       = rcGetStyle();
    const char *section = NULL;
    int         i;

    /* Each place is a Tab stop that Enter and Space open. The column scrolls, so it clips,
       a focus ring too, and a ring reaches 4 px past its row (2 px wide, 2 px out): the left
       and bottom padding are its room, and the scrollbar takes its own room on the right,
       clear of a row's wash. The title shares the toolbar's height, so the two stand on one line
       across the panel. The place it is in is washed, and hovering lifts a row: hover: reads
       the row's own box. */
    rcColumn(.id = "fs_places", .gap = 2, .pb = 4, .pl = 4, .scroll = "v", .h = "grow",
             .wType = RC_PX(FS_SIDEBAR_W)) {
        rcRow(.pl = 8, .align = "cl", .w = "grow", .className = FS_CONTROL("")) {
            rcTextL("Open a file", .font = rcFontSemibold((float)FS_PX_TITLE), .color = s.text);
        }
        for (i = 0; i < fs->placeCount; i++) {
            const FsPlace *p  = &fs->places[i];
            const char    *id = rcFormat(m, "fs_place%d", i).chars;

            if (p->section != section) {
                section = p->section;
                rcBox(.pt = 12, .pb = 4, .pl = 8) {
                    rcTextC(section, .font = rcFontSemibold((float)FS_PX_LABEL),
                            .color = s.textMuted);
                }
            }
            rcRow(.id = id, .focusable = true, .gap = 8, .px = 8, .align = "cl",
                  .borderRadius = "all-md", .w = "grow",
                  .className = fs->here == i ? FS_CONTROL("bg-foreground/11")
                                             : FS_CONTROL("hover:bg-foreground/5")) {
                RC_Color tint = FS_TINT_FOLDER;

                p->icon(16.0f, tint);
                rcTextC(p->label, .color = s.text, .size = FS_PX_TEXT, .wrap = "n",
                        .select = RC_SELECT_NONE);
            }
            if (fs_clicked(id)) {
                fs_go(fs, i, "");
                fs->again = true;
            }
        }
    }
    rcScrollbar("fs_places");
}

/* A native panel's lists are pop-up menus, the current choice checked: the index chosen
   this frame, or -1. rcBeginMenu says when one is down, which is what lets fs_keys leave
   the table alone meanwhile. Where its parent stretches the button (fs_sheet), the caret
   keeps the right end, as a pop-up button's does. */
static inline int fs_menu(FileSelector *fs, const char *id, const char *const *labels, int count,
                          int current) {
    int chosen = -1, i;

    if (rcBeginMenu(id, labels[current], .className = FS_CONTROL("justify-between"),
                    .caret = true)) {
        fs->menuOpen = true;
        for (i = 0; i < count; i++)
            if (rcMenuItem(labels[i], .leadingIcon = i == current ? rcIconCheck : NULL))
                chosen = i;
        rcEndMenu();
    }
    return chosen;
}

/* The sidebar's places as a list, where a narrow panel has no room for one. */
static inline void fs_place_list(FileSelector *fs) {
    int chosen = fs_menu(fs, "fs_place", fs->placeLabel, fs->placeCount, fs->here);

    if (chosen >= 0) {
        fs_go(fs, chosen, "");
        fs->again = true;
    }
}

/* ---- The toolbar ---- */

/* One crumb's room in the bar: its label in the controls' font, and what goes with it. */
static inline float fs_crumb_w(const char *text, int length) {
    return rcTextWidth(0, 0, text, length) + (float)(FS_CRUMB_PAD + FS_CRUMB_SEP);
}

/* The place, then the folders below it, each a ghost button that goes back there. THE
   BAR ENDS WHERE THE TABLE IS: the current folder is the last crumb, plain text that
   takes an ellipsis where it must, and never goes. The crumbs before it are named while
   they fit, nearest first, and "..." stands in for the rest. */
static inline void fs_crumbs(RC_App *app, FileSelector *fs) {
    /* The place and every folder rel can name, a byte and a separator each: a shorter
       list would end the bar on a folder above the one listed. */
    enum { CRUMB_MAX = FS_PATH_CAP / 2 + 1 };
    RC_Style    s    = rcGetStyle();
    RC_Arena   *m    = rcAppArena(app);
    RC_Box      box  = rcGetElementBox("fs_crumbs");
    float       dots = fs_crumb_w("...", 3), room;
    const char *seg[CRUMB_MAX], *leaf;
    int         len[CRUMB_MAX], n = 1, i, first;

    seg[0] = fs->places[fs->here].label;
    len[0] = fs_len(seg[0]);
    for (i = 0; fs->rel[i] && n < CRUMB_MAX; ) {
        seg[n] = fs->rel + i;
        while (fs->rel[i] && !fs_is_sep(fs->rel[i])) i++;
        len[n] = (int)(fs->rel + i - seg[n]);
        n++;
        while (fs_is_sep(fs->rel[i])) i++;
    }
    if (!box.found) fs->again = true;              /* it opens with the current folder alone */
    first = n - 1;
    room  = box.width - fs_crumb_w(seg[first], len[first]);
    while (first > 0) {
        float cost = fs_crumb_w(seg[first - 1], len[first - 1]);

        if (cost + (first > 1 ? dots : 0.0f) > room) break;
        room -= cost;
        first--;
    }
    leaf = rcFormat(m, "%.*s", len[n - 1], seg[n - 1]).chars;
    /* `truncate` holds each label on one line: a squeezed button's label would
       wrap at its spaces. A name the bar cut is whole one hover away. */
    rcRow(.id = "fs_crumbs", .gap = 2, .align = "cl", .overflow = "hidden", .w = "grow",
          .className = "truncate") {
        if (first > 0) {
            if (rcButton("fs_crumb_more", "...",
                         .className = FS_CONTROL("text-muted-foreground ring-inset"),
                         .variant = RC_BTN_GHOST))
                fs_go_depth(fs, first - 1);
            rcIconChevronRight(14.0f, s.textMuted);
        }
        for (i = first; i < n - 1; i++) {
            if (rcButton(rcFormat(m, "fs_crumb%d", i).chars,
                         rcFormat(m, "%.*s", len[i], seg[i]).chars,
                         .className = FS_CONTROL("text-muted-foreground ring-inset"),
                         .variant = RC_BTN_GHOST))
                fs_go_depth(fs, i);
            rcIconChevronRight(14.0f, s.textMuted);
        }
        rcRow(.pl = FS_CRUMB_PAD / 2, .align = "cl", .overflow = "hidden", .w = "grow",
              .tooltip = room < (first > 0 ? dots : 0.0f) ? leaf : NULL) {
            rcTextC(leaf, .color = s.text, .wrap = "e", .select = RC_SELECT_NONE);
        }
    }
}

/* Back, Forward and Up, each disabled where it has nowhere to go. An icon says less
   than a word, so each names itself under the pointer. */
static inline void fs_nav_buttons(FileSelector *fs) {
    rcRow(.gap = 2, .align = "cl") {
        rcBox(.tooltip = "Back") {
            if (rcButton("fs_back", "", .disabled = fs->historyAt <= 0,
                         .className = FS_CONTROL(""), .variant = RC_BTN_GHOST,
                         .leadingIcon = rcIconChevronLeft))
                fs_go_stop(fs, fs->historyAt - 1);
        }
        rcBox(.tooltip = "Forward") {
            if (rcButton("fs_forward", "", .disabled = fs->historyAt + 1 >= fs->historyCount,
                         .className = FS_CONTROL(""), .variant = RC_BTN_GHOST,
                         .leadingIcon = rcIconChevronRight))
                fs_go_stop(fs, fs->historyAt + 1);
        }
        rcBox(.tooltip = "Enclosing folder") {
            if (rcButton("fs_up", "", .disabled = !fs->rel[0], .className = FS_CONTROL(""),
                         .variant = RC_BTN_GHOST, .leadingIcon = rcIconArrowUp))
                fs_up(fs);
        }
    }
}

/* The search and the file-type list. Either one only reorders view[], on the next
   frame. The field's class is FS_CONTROL's height; its clear x empties it as typing does,
   and the call's true marks the view stale either way. */
static inline void fs_filter_field(FileSelector *fs) {
    if (rcTextInput("fs_filter", fs->filter, (int)sizeof fs->filter,
                    .className = rcPointerIsCoarse() ? "h-11" : "h-8", .placeholder = "Search",
                    .leadingIcon = rcIconSearch, .clearButton = true))
        fs->dirty = fs->again = true;
}

static inline void fs_type_list(FileSelector *fs) {
    int chosen = -1;

    if (fs->typeCount) chosen = fs_menu(fs, "fs_type", fs->typeLabel, fs->typeCount, fs->type);
    if (chosen >= 0) {
        fs->type  = chosen;
        fs->dirty = fs->again = true;
    }
}

/* ---- Sorting ---- */

static const struct {
    const char *id, *label;
    int         width;       /* 0 for the name, which takes the rest */
    const char *align;
} FS_COLUMN[FS_BY_COUNT] = {
    { "fs_sort_name", "Name",     0,   "cl" },
    { "fs_sort_date", "Modified", 150, "cl" },
    { "fs_sort_size", "Size",     68,  "cr" },
    { "fs_sort_kind", "Kind",     116, "cl" },
};

/* How many columns a table of this width shows, in FS_COLUMN's order: all of them
   where they fit, else as many as leave room for the tip that names the rest. */
static inline int fs_columns_that_fit(float width) {
    float room = width - (float)(FS_NAME_MIN + 2 * (FS_LIST_PAD + FS_ROW_PAD));
    float all  = 0.0f;
    int   n;

    for (n = 1; n < FS_BY_COUNT; n++) all += (float)(FS_COL_GAP + FS_COLUMN[n].width);
    if (all <= room) return FS_BY_COUNT;
    room -= (float)(FS_COL_GAP + FS_TIP_W);
    for (n = 1; n < FS_BY_COUNT && (float)(FS_COL_GAP + FS_COLUMN[n].width) <= room; n++)
        room -= (float)(FS_COL_GAP + FS_COLUMN[n].width);
    return n;
}

/* A click on the sorted column flips it; another column sorts ascending, except
   the two where the most is the interesting end. */
static inline void fs_sort_by(FileSelector *fs, FsSort key) {
    if (fs->sort == key) {
        fs->desc = !fs->desc;
    } else {
        fs->sort = key;
        fs->desc = key == FS_BY_SIZE || key == FS_BY_DATE;
    }
    fs->dirty = fs->again = true;
}

static inline void fs_sort_arrow(const FileSelector *fs, FsSort key, RC_Color c) {
    if (fs->sort == key) (fs->desc ? rcIconArrowDown : rcIconArrowUp)(12.0f, c);
}

/* The table's heading, built as the row below it is, so every label stands over
   its column. A heading is the sort control: a Tab stop that Enter and Space press,
   read by fs_clicked; hovered, it takes the ink. Its focus ring reaches 4 px past it
   (2 px wide, 2 px out) and the table clips, so the row keeps 4 px above and below. */
static inline void fs_table_head(RC_Arena *m, FileSelector *fs, int cols) {
    static const char *const HIDDEN[FS_BY_COUNT] = {
        "", "Kind is hidden at this width", "Size and Kind are hidden at this width",
        "Modified, Size and Kind are hidden at this width"
    };
    RC_Style s     = rcGetStyle();
    int      extra = FS_BY_COUNT - cols, k;

    rcRow(.gap = FS_COL_GAP, .pl = FS_LIST_PAD + FS_ROW_PAD, .pr = FS_LIST_PAD + FS_ROW_PAD,
          .py = 4, .align = "cl", .w = "grow", .hType = RC_PX(28)) {
        for (k = 0; k < cols; k++) {
            bool     lit = fs->sort == (FsSort)k || rcShowsHover(FS_COLUMN[k].id);
            RC_Color ink = lit ? s.text : s.textMuted;

            rcRow(.id = FS_COLUMN[k].id, .focusable = true, .gap = 4, .align = FS_COLUMN[k].align,
                  .h = "grow", .wType = k == FS_BY_NAME ? RC_GROW : RC_PX(FS_COLUMN[k].width)) {
                rcTextC(FS_COLUMN[k].label, .font = rcFontSemibold((float)FS_PX_SMALL),
                        .color = ink, .wrap = "n", .select = RC_SELECT_NONE);
                fs_sort_arrow(fs, (FsSort)k, ink);
            }
            if (fs_clicked(FS_COLUMN[k].id)) fs_sort_by(fs, (FsSort)k);
        }
        if (extra) {
            rcBox(.align = "cr", .wType = RC_PX(FS_TIP_W), .tooltip = HIDDEN[extra]) {
                rcText(rcFormat(m, "+%d column%s", extra, extra == 1 ? "" : "s"),
                       .color = s.textMuted, .size = FS_PX_LABEL, .wrap = "n");
            }
        }
    }
}

/* The list's sort control, where there are no columns: the same ids as chips, Tab stops as
   the headings are, with the count at the end of the strip where the width leaves room for
   all of it. */
static inline void fs_sort_chips(FileSelector *fs, RC_String count) {
    RC_Style s     = rcGetStyle();
    RC_Box   strip = rcGetElementBox("fs_strip"), chips = rcGetElementBox("fs_chips");
    float    need  = rcTextWidth(0, FS_PX_TEXT, count.chars, (int)count.length) + 16.0f;
    int      k;

    if (!strip.found) fs->again = true;
    rcRow(.id = "fs_strip", .gap = 8, .align = "cl", .w = "grow") {
        rcRow(.id = "fs_chips", .gap = 8, .align = "cl") {
            for (k = 0; k < FS_BY_KIND; k++) {
                bool     on  = fs->sort == (FsSort)k;
                RC_Color ink = on ? RC_WHITE : s.text;

                rcRow(.id = FS_COLUMN[k].id, .focusable = true, .gap = 4, .px = 12, .align = "cc",
                      .borderRadius = "all-full", .hType = RC_PX(rcPointerIsCoarse() ? 40 : 32),
                      .className = on ? "bg-primary border border-primary"
                                      : "border border-border") {
                    rcTextC(FS_COLUMN[k].label, .color = ink, .size = FS_PX_TEXT, .wrap = "n",
                            .select = RC_SELECT_NONE);
                    fs_sort_arrow(fs, (FsSort)k, ink);
                }
                if (fs_clicked(FS_COLUMN[k].id)) fs_sort_by(fs, (FsSort)k);
            }
        }
        rcSpacer() {}
        if (strip.found && chips.found && need <= strip.width - chips.width)
            rcText(count, .color = s.textMuted, .size = FS_PX_TEXT, .wrap = "n");
    }
}

/* ---- The rows ---- */

/* A click on a folder opens it and a click on a file picks it, so a finger needs
   no double tap. A mouse's habit still works: a quick second click on the picked
   file opens it, and the second click of a double click on a folder, which lands
   in the folder just opened, is ignored. rcClickCount is the click's count in a
   row, as the web's event.detail: 2 is a double click. */
static inline void fs_row_click(FileSelector *fs, int entry, const char *id) {
    int count;

    if (!rcClicked(id)) return;
    count = rcClickCount();
    if (count > 1 && fs->entered) return;
    fs->entered = fs->entry[entry].folder;
    if (fs->entered) {
        fs_enter(fs, entry);
    } else {
        fs->submit = fs->pick == entry && count == 2;
        fs->pick   = entry;
    }
    fs->again = true;
}

/* The picked row carries the accent, as a native list's does, with white on it.
   Every other row of the table alternates, and hovering lifts a row: hover: reads the
   row's own box. */
static inline const char *fs_row_class(const FileSelector *fs, int entry, int row) {
    if (fs->pick == entry) return "bg-primary";
    return row % 2 ? "bg-foreground/3 hover:bg-foreground/9" : "hover:bg-foreground/9";
}

/* An entry's icon: its kind's tint, white on the accent. */
static inline void fs_entry_icon(const FsEntry *e, float size, bool picked) {
    e->kind->icon(size, picked ? RC_WHITE : e->kind->tint);
}

/* A row's name, with the search's match lit in the heavier weight: three runs side by
   side, which is why a search shows no ellipsis; the row clips a long name. Row text is
   chrome, as a native list's is: a drag on a row selects no text. */
static inline void fs_row_name(RC_Arena *m, const FileSelector *fs, const char *name, RC_Color ink,
                               uint16_t size) {
    int at = fs->filter[0] ? fs_find(name, fs->filter) : -1;
    int n  = fs_len(fs->filter);

    if (at < 0) {
        rcTextC(name, .color = ink, .size = size, .wrap = "e", .select = RC_SELECT_NONE);
        return;
    }
    rcRow(.align = "cl", .overflow = "hidden", .w = "grow") {
        if (at)
            rcText(rcFormat(m, "%.*s", at, name), .color = ink, .size = size, .wrap = "n",
                   .select = RC_SELECT_NONE);
        rcText(rcFormat(m, "%.*s", n, name + at), .font = rcFontSemibold((float)size), .color = ink,
               .wrap = "n", .select = RC_SELECT_NONE);
        if (name[at + n])
            rcTextC(name + at + n, .color = ink, .size = size, .wrap = "n",
                    .select = RC_SELECT_NONE);
    }
}

static inline void fs_table_row(RC_App *app, FileSelector *fs, int entry, int row, int cols) {
    RC_Arena      *m   = rcAppArena(app);
    RC_Style       s   = rcGetStyle();
    const FsEntry *e   = &fs->entry[entry];
    const char    *id  = rcFormat(m, "fs_row%d", entry).chars;   /* from the data, not the place */
    bool           on  = fs->pick == entry;
    RC_Color       ink = on ? RC_WHITE : s.text;
    RC_Color       sub = on ? rcWithAlpha(RC_WHITE, 0.863f) : s.textMuted;
    int            k;
    /* The tip stands over this space, so every value stays under its heading. */
    float          pr  = (float)(FS_ROW_PAD + (cols < FS_BY_COUNT ? FS_COL_GAP + FS_TIP_W : 0));

    rcRow(.id = id, .gap = FS_COL_GAP, .pl = FS_ROW_PAD, .pr = pr, .align = "cl",
          .borderRadius = "all-md", .w = "grow",
          .hType = RC_PX(rcPointerIsCoarse() ? FS_ROW_TOUCH : FS_ROW_H),
          .className = fs_row_class(fs, entry, row)) {
        rcRow(.gap = 8, .align = "cl", .overflow = "hidden", .w = "grow") {
            fs_entry_icon(e, 16.0f, on);
            fs_row_name(m, fs, fs_name(fs, entry), ink, FS_PX_TEXT);
        }
        for (k = 1; k < cols; k++) {
            rcRow(.align = FS_COLUMN[k].align, .overflow = "hidden",
                  .wType = RC_PX(FS_COLUMN[k].width)) {
                if (k == FS_BY_DATE)
                    rcText(fs_date_text(m, e->modified), .color = sub, .size = FS_PX_TEXT,
                           .wrap = "n", .select = RC_SELECT_NONE);
                else if (k == FS_BY_SIZE)
                    rcText(e->folder ? rcStringFromCStr("--") : fs_size_text(m, e->size),
                           .color = sub, .size = FS_PX_TEXT, .wrap = "n", .select = RC_SELECT_NONE);
                else if (k == FS_BY_KIND)
                    rcTextC(e->kind->label, .color = sub, .size = FS_PX_TEXT, .wrap = "n",
                            .select = RC_SELECT_NONE);
            }
        }
    }
    fs_row_click(fs, entry, id);
}

/* The phone's row: the name over its size and date, and a chevron on a folder,
   which a tap enters. It has the table row's look less the stripes, with a rule
   under it. */
static inline void fs_list_row(RC_App *app, FileSelector *fs, int entry) {
    RC_Arena      *m   = rcAppArena(app);
    RC_Style       s   = rcGetStyle();
    const FsEntry *e   = &fs->entry[entry];
    const char    *id  = rcFormat(m, "fs_row%d", entry).chars;
    bool           on  = fs->pick == entry;
    RC_Color       ink = on ? RC_WHITE : s.text;
    RC_Color       sub = on ? rcWithAlpha(RC_WHITE, 0.863f) : s.textMuted;

    rcRow(.id = id, .gap = 14, .px = 16, .align = "cl", .w = "grow", .hType = RC_PX(FS_ROW_TALL),
          .className = on ? "bg-primary border-b border-border"
                          : "border-b border-border hover:bg-foreground/9") {
        fs_entry_icon(e, 24.0f, on);
        rcColumn(.gap = 3, .overflow = "hidden", .w = "grow") {
            fs_row_name(m, fs, fs_name(fs, entry), ink, FS_PX_PHONE);
            rcRow(.gap = 5, .align = "cl") {
                if (!e->folder) {
                    rcText(fs_size_text(m, e->size), .color = sub, .size = FS_PX_TEXT,
                           .wrap = "n", .select = RC_SELECT_NONE);
                    rcTextL(FS_DOT, .color = sub, .size = FS_PX_TEXT, .select = RC_SELECT_NONE);
                }
                rcText(fs_date_text(m, e->modified), .color = sub, .size = FS_PX_TEXT,
                       .wrap = "n", .select = RC_SELECT_NONE);
            }
        }
        if (e->folder) rcIconChevronRight(16.0f, sub);
    }
    fs_row_click(fs, entry, id);
}

/* A centered message in the list's place: an empty folder, a search that hides
   everything, or a folder that could not be listed. */
static inline void fs_message(RC_App *app, FileSelector *fs) {
    RC_Style    s     = rcGetStyle();
    RC_Arena   *m     = rcAppArena(app);
    bool        error = fs->status != RC_FILE_OK;
    const char *title, *line;

    if (error) {
        title = "This folder cannot be opened";
        line  = fs_status_text(fs->status);
    } else if (!fs->count) {
        title = "This folder is empty";
        line  = "Nothing has been saved here yet.";
    } else if (fs->filter[0]) {
        title = "No results";
        line  = rcFormat(m, "No name in this folder contains \"%s\".", fs->filter).chars;
    } else {
        title = "No files of this type";
        line  = "Choose All files to see everything in this folder.";
    }
    rcColumn(.gap = 8, .p = 24, .align = "cc", .w = "grow", .h = "grow") {
        if (error)           rcIconCircleAlert(32.0f, s.danger);
        else if (!fs->count) rcIconFolderSolid(32.0f, s.textMuted);
        else                 rcIconSearch(32.0f, s.textMuted);
        rcTextC(title, .font = rcFontSemibold((float)FS_PX_TEXT), .color = s.text,
                .textAlign = "c");
        rcTextC(line, .color = s.textMuted, .size = FS_PX_TEXT, .textAlign = "c");
        if (error && rcButton("fs_retry", "Try again", .className = FS_CONTROL("")))
            fs_go(fs, fs->here, fs->rel);
    }
}

/* The rows, or the message in their place. */
static inline void fs_body(RC_App *app, FileSelector *fs, bool table, int cols) {
    float    py    = table ? (float)FS_LIST_PAD : 0.0f;
    float    pitch = !table ? (float)FS_ROW_TALL
                            : (float)(rcPointerIsCoarse() ? FS_ROW_TOUCH : FS_ROW_H);

    if (fs->status != RC_FILE_OK || !fs->shown) {
        fs_message(app, fs);
        return;
    }
    rcColumn(.id = "fs_list", .pt = py, .pb = py, .px = py, .scroll = "v", .w = "grow",
             .h = "grow") {
        rcVirtualList(row, "fs_list", fs->shown, pitch) {
            if (table) fs_table_row(app, fs, fs->view[row.index], row.index, cols);
            else       fs_list_row(app, fs, fs->view[row.index]);
        }
    }
}

/* "4 folders, 12 files", or what the search left of them. A folder that could not
   be listed has no count: the message says why. */
static inline RC_String fs_count_text(RC_Arena *m, const FileSelector *fs) {
    int i, folders = 0, files;

    if (fs->status != RC_FILE_OK) return rcStringFromCStr("");
    if (!fs->count) return rcStringFromCStr("Empty");
    if (fs->full) return rcFormat(m, "First %d items", fs->count);
    if (fs->shown != fs->count) return rcFormat(m, "%d of %d items", fs->shown, fs->count);
    for (i = 0; i < fs->count; i++) folders += fs->entry[i].folder;
    files = fs->count - folders;
    if (!files)   return rcFormat(m, "%d folder%s", folders, folders == 1 ? "" : "s");
    if (!folders) return rcFormat(m, "%d file%s", files, files == 1 ? "" : "s");
    return rcFormat(m, "%d folder%s, %d file%s", folders, folders == 1 ? "" : "s",
                    files, files == 1 ? "" : "s");
}

/* The table: its heading, its rows, and a status line naming the pick where no
   preview pane does. */
static inline void fs_table(RC_App *app, FileSelector *fs, RC_String count, bool preview) {
    RC_Style  s   = rcGetStyle();
    RC_Arena *m   = rcAppArena(app);
    RC_Box    box = rcGetElementBox("fs_table");
    int       cols;

    /* The frame it first opens has no box yet: it draws every column, then asks
       for the frame that measures. */
    cols = box.found ? fs_columns_that_fit(box.width) : FS_BY_COUNT;
    if (!box.found) fs->again = true;

    rcColumn(.id = "fs_table", .p = 1, .overflow = "hidden", .borderRadius = "all-lg",
             .w = "grow", .h = "grow", .className = "bg-background border border-border") {
        fs_table_head(m, fs, cols);
        fs_rule();
        fs_body(app, fs, true, cols);
        fs_rule();
        rcRow(.gap = 8, .px = FS_LIST_PAD + FS_ROW_PAD, .align = "cl", .w = "grow",
              .hType = RC_PX(26)) {
            rcText(count, .color = s.textMuted, .size = FS_PX_SMALL, .wrap = "n");
            rcRow(.gap = 8, .align = "cr", .overflow = "hidden", .w = "grow") {
                if (!preview && fs->pick >= 0 && !fs->entry[fs->pick].folder) {
                    rcTextC(fs_name(fs, fs->pick), .color = s.text, .size = FS_PX_SMALL,
                            .wrap = "e");
                    rcTextL(FS_DOT, .color = s.textMuted, .size = FS_PX_SMALL);
                    rcText(fs_size_text(m, fs->entry[fs->pick].size), .color = s.textMuted,
                           .size = FS_PX_SMALL, .wrap = "n");
                }
            }
        }
    }
}

/* One label and its value in the preview pane. */
static inline void fs_fact(const char *label, RC_String value) {
    RC_Style s = rcGetStyle();

    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        rcTextC(label, .color = s.textMuted, .size = FS_PX_SMALL, .wrap = "n");
        rcSpacer() {}
        rcText(value, .color = s.text, .size = FS_PX_SMALL, .wrap = "n");
    }
}

/* The pane beside the table, as a native panel's preview column is: the picked
   file's picture, or its kind's icon, over what is known of it. */
static inline void fs_preview_pane(RC_Arena *m, FileSelector *fs) {
    RC_Style s = rcGetStyle();

    rcColumn(.gap = 12, .p = 12, .overflow = "hidden", .borderRadius = "all-lg", .h = "grow",
             .wType = RC_PX(FS_PREVIEW_W), .className = "bg-background border border-border") {
        if (fs->pick < 0) {
            rcColumn(.gap = 8, .align = "cc", .w = "grow", .h = "grow") {
                rcIconImage(32.0f, s.textMuted);
                rcTextL("Select a file to preview it", .color = s.textMuted,
                        .size = FS_PX_TEXT, .textAlign = "c");
            }
        } else {
            const FsEntry *e = &fs->entry[fs->pick];

            rcBox(.bg = s.surface, .p = 8, .align = "cc", .overflow = "hidden",
                  .borderRadius = "all-md", .w = "grow", .hType = RC_PX(148)) {
                if (!e->folder && file_preview_ready(&fs->preview))
                    file_preview_draw(&fs->preview, (float)FS_THUMB);
                else
                    e->kind->icon(56.0f, e->kind->tint);
            }
            rcColumn(.gap = 2, .w = "grow") {
                rcTextC(fs_name(fs, fs->pick), .font = rcFontSemibold((float)FS_PX_TEXT),
                        .color = s.text, .wordBreak = "b");
                rcTextC(e->kind->label, .color = s.textMuted, .size = FS_PX_SMALL);
            }
            fs_rule();
            if (!e->folder) fs_fact("Size", fs_size_text(m, e->size));
            if (!e->folder && fs->preview.image.handle)
                fs_fact("Dimensions", rcFormat(m, "%d x %d", fs->preview.image.width,
                                               fs->preview.image.height));
            fs_fact("Modified", fs_date_text(m, e->modified));
        }
    }
}

/* Cancel and Open. Open waits for a pick, and enters a picked folder. */
static inline bool fs_buttons(FileSelector *fs, char *out, int cap, bool grow) {
    bool picked = false;

    /* On a phone the two share the width: items-stretch widens each button to
       its half, as a column's children stretch in CSS. */
    rcColumn(.w = grow ? "grow" : "fit", .className = "items-stretch") {
        if (rcButton("fs_cancel", "Cancel", .className = FS_CONTROL(""))) fs->open = false;
    }
    rcColumn(.w = grow ? "grow" : "fit", .className = "items-stretch") {
        if (rcButton("fs_open", "Open", .disabled = fs->pick < 0, .className = FS_CONTROL(""),
                     .variant = fs->pick < 0 ? RC_BTN_DEFAULT : RC_BTN_PRIMARY))
            picked = fs_activate(fs, out, cap);
    }
    return picked;
}

/* ---- The call ---- */

/* A desktop's panel: the sidebar, then a column of toolbar, table and buttons. With no
   sidebar to carry its title, the title stands over them where the height allows. */
static inline bool fs_panel(RC_App *app, FileSelector *fs, char *out, int cap, float width,
                            bool tall, RC_String count) {
    bool sidebar = width >= (float)FS_SIDEBAR_AT && fs->placeCount > 1;
    bool preview = width >= (float)FS_PREVIEW_AT;
    bool picked  = false;

    rcRow(.id = "fs", .gap = 8, .p = fs->inset, .w = "grow", .h = "grow") {
        if (sidebar) fs_sidebar(rcAppArena(app), fs);
        rcColumn(.gap = 12, .w = "grow", .h = "grow") {
            if (!sidebar && tall)
                rcTextL("Open a file", .font = rcFontSemibold((float)FS_PX_TITLE),
                        .color = rcGetStyle().text);
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                fs_nav_buttons(fs);
                fs_crumbs(app, fs);
                rcBox(.wType = RC_PX(180)) { fs_filter_field(fs); }
            }
            rcRow(.gap = 12, .w = "grow", .h = "grow") {
                fs_table(app, fs, count, preview);
                if (preview) fs_preview_pane(rcAppArena(app), fs);
            }
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                if (!sidebar && fs->placeCount > 1) fs_place_list(fs);
                fs_type_list(fs);
                rcSpacer() {}
                if (fs_buttons(fs, out, cap, false)) picked = true;
            }
        }
    }
    return picked;
}

/* A phone's sheet: one column, with two-line rows. */
static inline bool fs_sheet(RC_App *app, FileSelector *fs, char *out, int cap, RC_String count) {
    RC_Style s      = rcGetStyle();
    bool     picked = false;

    rcColumn(.id = "fs", .bg = s.surface, .w = "grow", .h = "grow") {
        rcColumn(.gap = 12, .pt = 16, .pb = 12, .px = 16, .w = "grow") {
            /* The title over the search in one column and the two lists in the next, which
               items-stretch widens to the wider of them: the lists share both edges, and
               every row being one control tall, each stands beside its own row. A lone
               list (one place, or no file types) stands beside the search: the column
               takes the row's height and justify-end sets the list at its foot. With no
               list at all the search takes the whole width. */
            rcRow(.gap = 8, .w = "grow") {
                rcColumn(.gap = 12, .w = "grow") {
                    rcRow(.align = "cl", .w = "grow", .className = FS_CONTROL("")) {
                        rcTextL("Open a file", .font = rcFontSemibold((float)FS_PX_PHONE_TITLE),
                                .color = s.text);
                    }
                    rcBox(.w = "grow") { fs_filter_field(fs); }
                }
                if (fs->placeCount > 1 || fs->typeCount) {
                    rcColumn(.gap = 12, .h = "grow", .className = "items-stretch justify-end") {
                        if (fs->placeCount > 1) fs_place_list(fs);
                        fs_type_list(fs);
                    }
                }
            }
            /* A phone has one way back, and only where there is somewhere to go. The path
               stands over the list it names. */
            if (fs->rel[0]) {
                rcRow(.gap = 8, .align = "cl", .w = "grow") {
                    if (rcButton("fs_up", "", .className = FS_CONTROL(""),
                                 .variant = RC_BTN_GHOST, .leadingIcon = rcIconChevronLeft))
                        fs_up(fs);
                    fs_crumbs(app, fs);
                }
            }
            fs_sort_chips(fs, count);
        }
        fs_rule();
        fs_body(app, fs, false, 1);
        fs_rule();
        rcRow(.gap = 12, .p = 16, .align = "cl", .w = "grow") {
            if (fs_buttons(fs, out, cap, true)) picked = true;
        }
    }
    return picked;
}

/* Declare the selector, filling its parent; true on the frame a file is picked,
   its path in out (cap bytes). Declares nothing while it is closed. */
static inline bool file_selector(RC_App *app, FileSelector *fs, char *out, int cap) {
    RC_Box    own = rcGetElementBox("fs");
    /* Last frame's box, or the viewport on the frame it first opens. */
    float     width  = own.found ? own.width : rcViewport().width;
    float     height = own.found ? own.height : rcViewport().height;
    bool      table = width >= (float)FS_TABLE_AT;
    float     pitch = !table ? (float)FS_ROW_TALL
                             : (float)(rcPointerIsCoarse() ? FS_ROW_TOUCH : FS_ROW_H);
    bool      picked;
    RC_String count;

    if (!fs->open) return false;
    fs_settle(fs);
    picked = fs_keys(fs, out, cap, pitch);
    fs->menuOpen = false;                           /* the frame's own lists say again */
    fs_settle(fs);                                  /* what the keys asked for */
    fs_sync_preview(fs);
    if (fs->reveal) {
        fs->reveal = false;
        fs_reveal(fs);
    }
    count = fs_count_text(rcAppArena(app), fs);

    if (table) {
        if (fs_panel(app, fs, out, cap, width, height >= (float)FS_TITLE_AT, count)) picked = true;
    } else {
        if (fs_sheet(app, fs, out, cap, count)) picked = true;
    }
    /* A scroll container gets no bar unless asked, and only one this frame
       declared may be named. */
    if (fs->status == RC_FILE_OK && fs->shown) rcScrollbar("fs_list");
    if (fs->submit) {
        fs->submit = false;
        if (fs_activate(fs, out, cap)) picked = true;
    }
    if (fs->going || fs->dirty || fs->again)
        rcWindowRequestFrame(rcAppCurrentWindow(app));
    fs->again = false;
    return picked;
}

#endif /* APP_FILE_SELECTOR_H */
