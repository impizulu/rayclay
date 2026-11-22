/*
    RayClay Icon Converter: drop SVG files on the window, see how each survives at small sizes,
    and export it as a header-only RayClay icon you call like any other.

    What to copy from here:
    - Three doors, one list: rcDroppedFile, rcFileDialogOpen and a typed folder all end in
      add_file, so a file reads, converts and names itself the same way whichever door it used.
    - Work that could stall a frame (parsing a folder, writing a batch) runs a few items each
      frame and asks for the next one with rcWindowRequestFrame, so the window stays live.
    - rcIconEmit custom elements draw the list thumbnails, the size strip and the artboard, with
      its anchor overlay, from one parsed icon.
    - rcVirtualList rows of borrowed rcText runs make a syntax-colored listing of any length.
    - RC_AppOptions.titlebar swaps window glyphs for callbacks shaped like the headers this writes.

    One layout: three panes, or a list beside two tabs, or three tabs, on available width alone.
    It reads and writes local files, so it is built for a desktop.

    Build target: rayclay_ex10_util_rc_icon_converter
*/

#include "rayclay.h"

/* The preview half of rc_svg2icon.h is opt-in: defining this links the icon runtime it draws
   through. An offline generator leaves it undefined. */
#define RC_SVG2ICON_PREVIEW
#include "rc_svg2icon.h"

#include "icons/rc_icons_check.h"
#include "icons/rc_icons_circle_check.h"
#include "icons/rc_icons_circle_x.h"
#include "icons/rc_icons_code.h"
#include "icons/rc_icons_download.h"
#include "icons/rc_icons_folder.h"
#include "icons/rc_icons_info.h"
#include "icons/rc_icons_list.h"
#include "icons/rc_icons_minus.h"
#include "icons/rc_icons_plus.h"
#include "icons/rc_icons_search.h"
#include "icons/rc_icons_triangle_alert.h"
#include "icons/rc_icons_upload.h"
#include "icons/rc_icons_x.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Bounded state: every collection is a fixed array. The heap holds each file's bytes, kept so a
   quality change reconverts without a disk read, and each file's parsed icon. */
enum {
    ICONV_MAX_FILES   = 256,
    ICONV_PATH_CAP    = 640,
    ICONV_NAME_CAP    = 96,
    ICONV_SYMBOL_CAP  = 112,
    ICONV_SVG_CAP     = 64 * 1024,   /* largest SVG it parses */
    ICONV_EMIT_CAP    = 64 * 1024,   /* largest header it writes */
    ICONV_MAX_LINES   = 4096,
    ICONV_LOG_LINES   = 64,
    ICONV_LOG_CAP     = 200,
    ICONV_PER_FRAME   = 8,           /* conversions between two frames */
    ICONV_WRITES      = 2,           /* writes between two frames: a save syncs to disk */
    ICONV_CELLS       = 16,          /* checker cells per artboard side */
    ICONV_BATCH_W     = 264,
    ICONV_EXPORT_W    = 400,
    ICONV_WIDE_W      = 1120,        /* three panes from here up */
    ICONV_NARROW_W    = 720,         /* below it, one stage at a time */
    ICONV_LINE_H      = 18,
    ICONV_GUTTER_W    = 40
};

/* Where headers go while the output Folder is empty, which is what its placeholder shows. */
#define ICONV_OUT_DIR "out/icons"

typedef enum { ICON_PENDING, ICON_OK, ICON_WARN, ICON_FAIL } IconState;
typedef enum { SHOW_ALL, SHOW_WARNINGS, SHOW_FAILED } BatchShow;
typedef enum { GROUND_CHECKER, GROUND_LIGHT, GROUND_DARK } Ground;
typedef enum { TAB_ICONS, TAB_PREVIEW, TAB_EXPORT } Tab;
typedef enum { LOG_INFO, LOG_DONE, LOG_FAIL } LogKind;

typedef struct {
    char       path[ICONV_PATH_CAP];
    char       stem[ICONV_NAME_CAP];     /* names the file and the symbol; the user may edit it */
    char       symbol[ICONV_SYMBOL_CAP];
    char      *svg;                      /* the file's bytes; NULL when it could not be read */
    int        svgLen;
    RcSvgIcon *icon;
    IconState  state;
    bool       checked;                  /* goes out with the batch */
    bool       queued;                   /* in the running batch and not written yet */
    int        headerBytes;
    unsigned   key;                      /* names the file's row ids; a sort carries it along */
} IconEntry;

typedef struct {
    IconEntry files[ICONV_MAX_FILES];
    int       fileCount;
    unsigned  nextKey;                   /* the last key given; no two files share one */
    int       selected;                  /* index into files, or -1 */
    bool      reveal;                    /* scroll the selected row into view, once */
    bool      listDrawn;                 /* this frame laid out the list, so reveal can run */
    float     listHeight;                /* the list's height when last laid out */
    int       removeIndex;               /* file to drop at the next frame's start, or -1 */

    char      folder[ICONV_PATH_CAP];
    char      filter[64];
    BatchShow show;
    int       visible[ICONV_MAX_FILES];  /* files passing the filter, rebuilt each frame */
    int       visibleCount;

    /* Tallies of files, rebuilt each frame. */
    int       okCount, warnCount, failCount, pendingCount, checkedCount;
    long      totalBytes;

    char      outDir[ICONV_PATH_CAP];
    float     curveSteps, arcStep, precision;

    Ground    ground;
    int       tint;                      /* 0: the ground's contrast color */
    int       zoomStep;
    int       recenter;                  /* frames until the canvas scrolls to its middle */
    bool      canvasDrawn;               /* this frame laid out the canvas, so recenter counts */
    bool      anchors;
    bool      original;
    RC_Svg   *originalSvg;
    bool      originalTried;             /* loaded once per selection, even when it failed */

    char      code[ICONV_EMIT_CAP];      /* the selected icon's header */
    int       codeLen;
    int       lineStart[ICONV_MAX_LINES + 1];
    bool      lineIsComment[ICONV_MAX_LINES];
    int       lineCount;
    int       longestLine;

    /* A batch marks its files when it starts, so a removal, a drop or a sort that moves the list
       under it can neither skip a file nor write one twice. */
    bool      exporting;
    int       exportDone, exportFailed;

    RC_FileDialogToken openToken;
    bool      openReplaces;              /* a folder replaces the list, files join it */
    bool      noDialogs;                 /* this host shows none: stop offering them */

    char      log[ICONV_LOG_LINES][ICONV_LOG_CAP];
    LogKind   logKind[ICONV_LOG_LINES];
    int       logCount, logHead;
    bool      logOpen;

    Tab       tab;
    uint16_t  mono;
    bool      started;
    bool      shown, opened;            /* the first frame is drawn; the start folder was read */
    double    copiedUntil;
    bool      rowMenuUp;                 /* a row's context menu was open last frame */
} AppState;

enum { ZOOM_STEPS = 8, ZOOM_FIT = 3, TINT_COUNT = 6 };

static const int   ZOOMS[ZOOM_STEPS] = { 25, 50, 75, 100, 150, 200, 300, 400 };
static const char *const TINTS[] = { "", "#0C8CE9", "#14AE5C", "#FF8A00", "#F24822", "#9747FF" };
static const char *const TINT_NAMES[] = { "Contrast", "Blue", "Green", "Orange", "Red", "Purple" };
static const char *const SWATCH_IDS[] = { "t0", "t1", "t2", "t3", "t4", "t5" };

/* Figma's dark workspace: a graphite canvas under lifted panels, and one blue that means
   selected. Yellow is a parser warning and red a step that did not happen. */
static RC_Style app_style(void) {
    RC_Style s = rcStyleDark();
    s.background   = rcColor("#1E1E1E");
    s.surface      = rcColor("#2C2C2C");
    s.surfaceAlt   = rcColor("#383838");
    s.chrome       = rcColor("#2C2C2C");
    s.text         = rcColor("#F5F5F5");
    s.textMuted    = rcColor("#ABABAB");
    s.border       = rcColor("#444444");
    s.primary      = rcColor("#0B78CC");
    s.primaryHover = rcColor("#0C8CE9");
    s.success      = rcColor("#3CCB7F");
    s.warning      = rcColor("#FFCD29");
    s.danger       = rcColor("#FF7262");
    s.radius       = 5.0f;
    return s;
}

/* The colors RC_Style has no field for: Figma's selection blue, the artboard's grounds and
   what reads on the light one, and VS Code Dark+'s syntax colors, which most readers of C
   parse without thinking. */
typedef struct {
    RC_Color selection;
    RC_Color groundLight, groundDark, checkerPale, checkerDeep;
    RC_Color inkOnLight, captionOnLight;
    RC_Color code, comment, directive, number, keyword, type, function, lineNumber;
} Palette;

static Palette app_palette(void) {
    Palette p;

    p.selection      = rcColor("#0C8CE9");
    p.groundLight    = rcColor("#FFFFFF");
    p.groundDark     = rcColor("#141414");
    p.checkerPale    = rcColor("#3A3A3A");
    p.checkerDeep    = rcColor("#303030");
    p.inkOnLight     = rcColor("#1E1E1E");
    p.captionOnLight = rcColor("#6B6B6B");
    p.code           = rcColor("#D4D4D4");
    p.comment        = rcColor("#6A9955");
    p.directive      = rcColor("#C586C0");
    p.number         = rcColor("#B5CEA8");
    p.keyword        = rcColor("#569CD6");
    p.type           = rcColor("#4EC9B0");
    p.function       = rcColor("#DCDCAA");
    p.lineNumber     = rcColor("#6E7681");
    return p;
}

/* The palette in force. main fills it once, beside rcSetStyle, and everything drawn reads it as
   it reads rcGetStyle(): a color string is parsed at startup, never per row or per frame. */
static Palette pal;

static void log_event(AppState *st, LogKind kind, const char *fmt, ...) RC_PRINTF_FMT(3, 4);

/* A fixed ring, oldest overwritten. The severity is stored by the site that reports, so a
   rewording never changes a color. A long line is cut by rcStrCopy, between UTF-8 characters,
   so a long non-ASCII path never ends in half of one. */
static void log_event(AppState *st, LogKind kind, const char *fmt, ...) {
    int     idx = (st->logHead + st->logCount) % ICONV_LOG_LINES;
    char    line[ICONV_LOG_CAP * 2];
    va_list ap;

    if (st->logCount < ICONV_LOG_LINES)
        st->logCount++;
    else
        st->logHead = (st->logHead + 1) % ICONV_LOG_LINES;
    st->logKind[idx] = kind;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    rcStrCopy(st->log[idx], line, ICONV_LOG_CAP);
}

static const char *plural_s(int n) {
    return n == 1 ? "" : "s";
}

static const char *path_leaf(const char *path) {
    const char *leaf = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\')
            leaf = p + 1;
    }
    return leaf;
}

static bool ends_with_svg(const char *name) {
    size_t n = strlen(name);
    return n >= 4 && name[n - 4] == '.' && tolower((unsigned char)name[n - 3]) == 's' &&
           tolower((unsigned char)name[n - 2]) == 'v' &&
           tolower((unsigned char)name[n - 1]) == 'g';
}

/* Without case, as macOS and Windows compare file names: Bell.h and bell.h are one file. */
static int compare_nocase(const char *x, const char *y) {
    for (; *x && *y; x++, y++) {
        int d = tolower((unsigned char)*x) - tolower((unsigned char)*y);
        if (d != 0)
            return d;
    }
    return (unsigned char)*x - (unsigned char)*y;
}

static bool contains_nocase(const char *haystack, const char *needle) {
    size_t n = strlen(needle);
    for (; *haystack; haystack++) {
        size_t i = 0;
        while (i < n && haystack[i] &&
               tolower((unsigned char)haystack[i]) == tolower((unsigned char)needle[i]))
            i++;
        if (i == n)
            return true;
    }
    return n == 0;
}

static RC_String format_bytes(RC_Arena *ar, long bytes) {
    if (bytes < 1024)
        return rcFormat(ar, "%ld B", bytes);
    if (bytes < 1024 * 1024)
        return rcFormat(ar, "%.1f KB", (double)bytes / 1024.0);
    return rcFormat(ar, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
}

/* Strip the EXTENSION, not the first dot: "chevron.right.svg" cut at the first dot would write
   chevron.h over an unrelated icon. A long name is cut by rcStrCopy, between UTF-8 characters:
   a stem cut inside one is no file name macOS accepts, so its header could never be saved. */
static void file_stem(const char *leaf, char *stem, int cap) {
    int len = (int)strlen(leaf) - (ends_with_svg(leaf) ? 4 : 0);

    rcStrCopy(stem, leaf, (size_t)(len + 1 < cap ? len + 1 : cap));
    for (char *p = stem; *p; p++)
        *p = (char)tolower((unsigned char)*p);
    if (!stem[0])
        snprintf(stem, (size_t)cap, "icon");
}

/* Whether one shape's bounds meet the viewBox. Coordinates or a transform can place every
   shape off the artboard, and such a file would export a header that draws nothing. */
static bool box_meets_view(const RcSvgIcon *icon, float x0, float y0, float x1, float y1) {
    return x1 >= 0.0f && y1 >= 0.0f && x0 <= icon->viewW && y0 <= icon->viewH;
}

static bool draws_in_view(const RcSvgIcon *icon) {
    for (int i = 0; i < icon->opCount; i++) {
        const RcSvgOp *op = &icon->ops[i];
        float x0 = op->p[0], y0 = op->p[1], x1 = op->p[0], y1 = op->p[1];

        if (op->pointCount > 0) {
            const RC_IconPoint *pt = &icon->points[op->pointOff];
            x0 = x1 = pt[0].x;
            y0 = y1 = pt[0].y;
            for (int k = 1; k < op->pointCount; k++) {
                x0 = fminf(x0, pt[k].x);
                x1 = fmaxf(x1, pt[k].x);
                y0 = fminf(y0, pt[k].y);
                y1 = fmaxf(y1, pt[k].y);
            }
        } else if (op->kind == RC_SVG_ROUND_LINE) {
            x0 = fminf(op->p[0], op->p[2]);
            x1 = fmaxf(op->p[0], op->p[2]);
            y0 = fminf(op->p[1], op->p[3]);
            y1 = fmaxf(op->p[1], op->p[3]);
        } else if (op->kind == RC_SVG_RRECT_STROKE) {
            x1 = op->p[0] + op->p[2];
            y1 = op->p[1] + op->p[3];
        } else if (op->kind == RC_SVG_FILLED_ELLIPSE) {
            x0 -= op->p[2];
            x1 += op->p[2];
            y0 -= op->p[3];
            y1 += op->p[3];
        } else {
            x0 -= op->p[2];
            x1 += op->p[2];
            y0 -= op->p[2];
            y1 += op->p[2];
        }
        if (box_meets_view(icon, x0, y0, x1, y1))
            return true;
    }
    return false;
}

/* Whether every number the header would print is finite. A coordinate past a float's range
   ("1e39", or relative steps that add up past it) parses as inf, which prints as "inf.0f", a
   token no compiler takes. RayClay's own SVG loader refuses such a file too. */
static bool geometry_is_finite(const RcSvgIcon *icon) {
    for (int i = 0; i < icon->opCount; i++) {
        const RcSvgOp *op = &icon->ops[i];

        for (int k = 0; k < 5; k++) {
            if (!isfinite(op->p[k]))
                return false;
        }
        if (!isfinite(op->stroke))
            return false;
    }
    for (int i = 0; i < icon->pointCount; i++) {
        if (!isfinite(icon->points[i].x) || !isfinite(icon->points[i].y))
            return false;
    }
    return isfinite(icon->viewW) && isfinite(icon->viewH);
}

/* Parse and measure one file. A failed read leaves its reason in icon->err. */
static char iconv_scratch[ICONV_EMIT_CAP];

static void convert_entry(const AppState *st, IconEntry *e) {
    int written;

    if (!e->svg) {
        e->state = ICON_FAIL;
        return;
    }
    if (!rc_svg2icon_parse(e->svg, e->svgLen, (int)st->curveSteps, st->arcStep, e->icon) ||
        !e->icon->ok) {
        e->state = ICON_FAIL;
        e->headerBytes = 0;
        return;
    }
    /* A header that draws nothing is no icon: a batch should say so rather than write it. */
    if (e->icon->opCount == 0) {
        snprintf(e->icon->err, sizeof e->icon->err, "It holds no shape RayClay can draw");
        e->icon->ok = false;
        e->state = ICON_FAIL;
        e->headerBytes = 0;
        return;
    }
    if (!geometry_is_finite(e->icon)) {
        snprintf(e->icon->err, sizeof e->icon->err, "A number in it is past a float's range");
        e->icon->ok = false;
        e->state = ICON_FAIL;
        e->headerBytes = 0;
        return;
    }
    if (!draws_in_view(e->icon)) {
        snprintf(e->icon->err, sizeof e->icon->err, "Nothing it draws falls inside its viewBox");
        e->icon->ok = false;
        e->state = ICON_FAIL;
        e->headerBytes = 0;
        return;
    }
    written = rc_svg2icon_emit(e->icon, e->stem, path_leaf(e->path), (int)st->precision,
                               iconv_scratch, ICONV_EMIT_CAP);
    if (written < 0) {
        snprintf(e->icon->err, sizeof e->icon->err, "the header passes %d KB",
                 ICONV_EMIT_CAP / 1024);
        e->icon->ok = false;
        e->state = ICON_FAIL;
        e->headerBytes = 0;
        return;
    }
    e->headerBytes = written;
    e->state = e->icon->warnCount > 0 ? ICON_WARN : ICON_OK;
}

/* Split the selected icon's header into lines for the listing. The text is borrowed by rcText
   runs, so nothing here copies a line. */
static void refresh_code(AppState *st) {
    st->codeLen = 0;
    st->lineCount = 0;
    st->longestLine = 0;
    st->code[0] = '\0';
    if (st->selected < 0)
        return;

    IconEntry *e = &st->files[st->selected];
    if (e->state == ICON_PENDING)
        convert_entry(st, e);
    if (e->state == ICON_FAIL)
        return;

    int written = rc_svg2icon_emit(e->icon, e->stem, path_leaf(e->path), (int)st->precision,
                                   st->code, ICONV_EMIT_CAP);
    if (written < 0)
        return;
    st->codeLen = written;
    st->lineStart[0] = 0;
    bool inComment = false;
    for (int i = 0; i < written && st->lineCount < ICONV_MAX_LINES; i++) {
        if (st->code[i] == '\n' || i == written - 1) {
            const char *line = st->code + st->lineStart[st->lineCount];
            int         len  = i - st->lineStart[st->lineCount];

            /* The generated header's one comment is whole lines. */
            if (strncmp(line, "/*", 2) == 0)
                inComment = true;
            st->lineIsComment[st->lineCount] = inComment;
            if (len >= 2 && line[len - 2] == '*' && line[len - 1] == '/')
                inComment = false;
            if (len > st->longestLine)
                st->longestLine = len;
            st->lineStart[++st->lineCount] = i + 1;
        }
    }
}

static void select_entry(AppState *st, int index) {
    if (index == st->selected && st->selected >= 0)
        return;
    st->selected = index;
    st->reveal = true;
    if (st->originalSvg)
        rcUnloadSvg(&st->originalSvg);
    st->originalTried = false;
    refresh_code(st);
}

/* A quality change reconverts the selection now and every other file a few per frame. */
static void quality_changed(AppState *st) {
    for (int i = 0; i < st->fileCount; i++) {
        if (st->files[i].svg)
            st->files[i].state = ICON_PENDING;
    }
    refresh_code(st);
}

static void free_entry(IconEntry *e) {
    /* rcLoadFile's bytes go back through rcFreeFile: free() frees them only where the app and
       the library share one C library, which a DLL built against another does not. */
    rcFreeFile(e->svg);
    free(e->icon);
    /* On a phone or the web the path names RayClay's private copy; on a desktop this leaves the
       user's own file alone. */
    rcReleaseFile(e->path);
}

static void clear_files(AppState *st) {
    for (int i = 0; i < st->fileCount; i++)
        free_entry(&st->files[i]);
    st->fileCount = 0;
    st->selected = -1;
    if (st->originalSvg)
        rcUnloadSvg(&st->originalSvg);
    st->originalTried = false;
    refresh_code(st);
}

static void remove_entry(AppState *st, int index) {
    int next = st->selected;

    free_entry(&st->files[index]);
    memmove(&st->files[index], &st->files[index + 1],
            (size_t)(st->fileCount - index - 1) * sizeof st->files[0]);
    st->fileCount--;
    if (next > index || next >= st->fileCount)
        next--;
    st->selected = -1;
    select_entry(st, next);
}

/* Returns the file other than skip whose output would collide with stem and symbol, or -1.
   Two names collide when they differ only in case, as macOS and Windows file names ignore it, or
   when they make one C name: bell-solid and bell_solid both become rcIconBellSolid and share one
   include guard, so a project including both would silently lose one icon. */
static int name_owner(const AppState *st, int skip, const char *stem, const char *symbol) {
    for (int i = 0; i < st->fileCount; i++) {
        if (i != skip && (compare_nocase(st->files[i].stem, stem) == 0 ||
                          compare_nocase(st->files[i].symbol, symbol) == 0))
            return i;
    }
    return -1;
}

/* Two folders can both hold a star.svg, and the second must not overwrite the first's header. */
static void unique_stem(AppState *st, char *stem, char *symbol) {
    char base[ICONV_NAME_CAP - 15];   /* room left for the suffix; cut between UTF-8 characters */
    int  suffix = 2;

    rcStrCopy(base, stem, sizeof base);
    rc_svg2icon_symbol(stem, symbol, ICONV_SYMBOL_CAP);
    while (name_owner(st, -1, stem, symbol) >= 0) {
        snprintf(stem, ICONV_NAME_CAP, "%s-%d", base, suffix++);
        rc_svg2icon_symbol(stem, symbol, ICONV_SYMBOL_CAP);
    }
}

/* One file joins the list, with its bytes in hand. Returns whether the list grew. A path it
   turns away is released: on a phone or the web it names RayClay's private copy, which no entry
   will release later. A path already listed is that entry's own file, so it stays. */
static bool add_file(AppState *st, const char *path) {
    for (int i = 0; i < st->fileCount; i++) {
        if (strcmp(st->files[i].path, path) == 0)
            return false;
    }
    if (!ends_with_svg(path) || strlen(path) >= ICONV_PATH_CAP) {
        rcReleaseFile(path);
        return false;
    }
    if (st->fileCount >= ICONV_MAX_FILES) {
        log_event(st, LOG_FAIL, "The list is full at %d files: %s was not added",
                  ICONV_MAX_FILES, path_leaf(path));
        rcReleaseFile(path);
        return false;
    }

    IconEntry *e = &st->files[st->fileCount];
    memset(e, 0, sizeof *e);
    e->icon = (RcSvgIcon *)calloc(1, sizeof *e->icon);
    if (!e->icon) {
        log_event(st, LOG_FAIL, "Out of memory: %s was not added", path_leaf(path));
        rcReleaseFile(path);
        return false;
    }
    snprintf(e->path, sizeof e->path, "%s", path);
    file_stem(path_leaf(path), e->stem, ICONV_NAME_CAP);
    unique_stem(st, e->stem, e->symbol);
    e->key = ++st->nextKey;
    e->checked = true;

    void  *bytes = NULL;
    size_t len   = 0;
    RC_PathInfo info;
    if (rcPathInfo(path, &info) == RC_FILE_OK && info.size > ICONV_SVG_CAP) {
        snprintf(e->icon->err, sizeof e->icon->err, "Larger than the %d KB limit",
                 ICONV_SVG_CAP / 1024);
    } else if (rcLoadFile(path, &bytes, &len) != RC_FILE_OK) {
        snprintf(e->icon->err, sizeof e->icon->err, "Could not read the file");
    } else {
        e->svg = (char *)bytes;
        e->svgLen = (int)len;
    }
    e->state = e->svg ? ICON_PENDING : ICON_FAIL;
    st->fileCount++;
    return true;
}

typedef struct {
    AppState   *st;
    const char *dir;
    int         added;
} ScanJob;

/* A typed folder adds the files directly in it, as a file manager lists them; the folder dialog
   answers with every file under the folder it picked (RC_FILE_DIALOG_FOLDER). A folder named
   icons.svg is not an icon, so a folder is skipped whatever its name. */
static bool scan_entry(const char *name, bool isDirectory, void *user) {
    ScanJob *job = (ScanJob *)user;
    char     path[ICONV_PATH_CAP * 2];
    size_t   n = strlen(job->dir);
    bool     sep = n > 0 && (job->dir[n - 1] == '/' || job->dir[n - 1] == '\\');

    if (isDirectory)
        return true;
    snprintf(path, sizeof path, "%s%s%s", job->dir, sep ? "" : "/", name);
    if (add_file(job->st, path))
        job->added++;
    return true;
}

/* A dropped or typed path: a file, or a folder of files. Returns how many joined the list, or -1
   once it has logged why the path could not be read. */
static int add_path(AppState *st, const char *path) {
    RC_PathInfo info;
    ScanJob     job = { st, path, 0 };

    if (rcPathInfo(path, &info) != RC_FILE_OK) {
        log_event(st, LOG_FAIL, "Nothing to open at %s", path);
        return -1;
    }
    if (!info.isDirectory)
        return add_file(st, path) ? 1 : 0;
    if (rcListDirectory(path, scan_entry, &job) != RC_FILE_OK) {
        log_event(st, LOG_FAIL, "Could not list %s", path);
        return job.added > 0 ? job.added : -1;
    }
    return job.added;
}

static int entry_cmp(const void *a, const void *b) {
    return compare_nocase(path_leaf(((const IconEntry *)a)->path),
                          path_leaf(((const IconEntry *)b)->path));
}

/* unique_stem keeps the names it gives apart, but the Name field can type one onto another.
   Returns the other file whose header would collide with index's, or -1. */
static int stem_owner(const AppState *st, int index) {
    return name_owner(st, index, st->files[index].stem, st->files[index].symbol);
}

/* The file system hands names over in its own order, so sort, and keep the selection on its
   file rather than on its old index. */
static void finish_adding(AppState *st, int added, bool reported, const char *from) {
    char keep[ICONV_PATH_CAP] = "";

    if (st->selected >= 0)
        snprintf(keep, sizeof keep, "%s", st->files[st->selected].path);
    qsort(st->files, (size_t)st->fileCount, sizeof st->files[0], entry_cmp);
    st->selected = -1;
    for (int i = 0; i < st->fileCount && keep[0]; i++) {
        if (strcmp(st->files[i].path, keep) == 0)
            st->selected = i;
    }
    if (st->selected < 0 && st->fileCount > 0)
        select_entry(st, 0);
    else
        refresh_code(st);
    /* A path that failed has said why: a second line here would bury it. */
    if (added > 0)
        log_event(st, LOG_INFO, "Added %d icon%s from %s", added, plural_s(added), from);
    else if (!reported)
        log_event(st, LOG_INFO, "No new SVG files in %s", from);
}

/* A typed folder replaces the list, but only once it is there: a typo must not empty it. */
static void open_folder(AppState *st, const char *path) {
    RC_PathInfo info;
    int         added;

    if (rcPathInfo(path, &info) != RC_FILE_OK) {
        log_event(st, LOG_FAIL, "Nothing to open at %s", path);
        return;
    }
    clear_files(st);
    added = add_path(st, path);
    finish_adding(st, added > 0 ? added : 0, added < 0, path);
}

/* A dialog still open REFUSES another at once (rcFileDialogOpen): a second click or Ctrl+O
   while a zenity window is up asks for one. That refusal must not take the open dialog's token,
   or the files the user then picks are never collected: only a request that went ahead does. */
static void ask_for_files(AppState *st, RC_FileDialogKind kind) {
    RC_FileDialogToken token = rcFileDialogOpen(kind, ".svg");

    if (st->openToken && rcFileDialogPoll(token).status == RC_FILE_DIALOG_REFUSED) {
        log_event(st, LOG_INFO, "A file dialog is already open");
        return;
    }
    st->openReplaces = kind == RC_FILE_DIALOG_FOLDER;
    st->openToken = token;
}

/* Collect a dialog's answer. Once a host says UNSUPPORTED it never will show one, so the buttons
   go and the drop and the typed path remain. */
static bool dialog_answered(AppState *st, RC_FileDialogToken *token, RC_FileDialogResult *r) {
    *r = rcFileDialogPoll(*token);
    if (r->status == RC_FILE_DIALOG_NONE || r->status == RC_FILE_DIALOG_PENDING)
        return false;
    *token = 0;
    if (r->status == RC_FILE_DIALOG_UNSUPPORTED) {
        st->noDialogs = true;
        log_event(st, LOG_INFO, "This system shows no file dialog: drop files or type a path");
    } else if (r->status == RC_FILE_DIALOG_REFUSED) {
        log_event(st, LOG_FAIL, "The file dialog did not open: try again");
    } else if (r->status == RC_FILE_DIALOG_FAILED) {
        log_event(st, LOG_FAIL, "The file dialog could not hand over its choice");
    }
    return r->status == RC_FILE_DIALOG_DONE;
}

static void poll_dialogs(AppState *st) {
    RC_FileDialogResult r;

    if (dialog_answered(st, &st->openToken, &r)) {
        int added = 0;
        if (st->openReplaces)
            clear_files(st);
        if (r.folder)
            snprintf(st->folder, sizeof st->folder, "%s", r.folder);
        for (int i = 0; i < r.count; i++)
            added += add_file(st, r.paths[i]) ? 1 : 0;
        finish_adding(st, added, false, r.folder ? r.folder : "the file dialog");
    }
}

static void handle_drops(AppState *st) {
    int  count  = rcDroppedFileCount();
    int  added  = 0;
    bool failed = false;

    if (count == 0)
        return;
    for (int i = 0; i < count; i++) {
        int n = add_path(st, rcDroppedFile(i));
        if (n < 0)
            failed = true;
        else
            added += n;
    }
    finish_adding(st, added, failed, count == 1 ? path_leaf(rcDroppedFile(0)) : "the drop");
}

/* Write one header. rcSaveFile writes the whole file or nothing, so a failed export never leaves
   half a header for a build to include. Two icons of one name would write one file, the second
   over the first, so neither goes until one is renamed. */
static bool save_header(AppState *st, int index) {
    IconEntry *e = &st->files[index];
    char       path[ICONV_PATH_CAP + ICONV_NAME_CAP + 4];
    int        written, owner;

    if (strchr(e->stem, '/') || strchr(e->stem, '\\') || !e->stem[0]) {
        log_event(st, LOG_FAIL, "\"%s\" cannot name a file", e->stem);
        return false;
    }
    owner = stem_owner(st, index);
    if (owner >= 0) {
        log_event(st, LOG_FAIL, "%s.h was not written: %s already makes %s", e->stem,
                  path_leaf(st->files[owner].path), st->files[owner].symbol);
        return false;
    }
    if (e->state == ICON_PENDING)
        convert_entry(st, e);
    if (e->state == ICON_FAIL) {
        log_event(st, LOG_FAIL, "%s: %s", path_leaf(e->path), e->icon->err);
        return false;
    }
    written = rc_svg2icon_emit(e->icon, e->stem, path_leaf(e->path), (int)st->precision,
                               iconv_scratch, ICONV_EMIT_CAP);
    snprintf(path, sizeof path, "%s/%s.h", st->outDir[0] ? st->outDir : ICONV_OUT_DIR, e->stem);
    if (written < 0 || rcSaveFile(path, iconv_scratch, (size_t)written, RC_FILE_PARENTS) !=
                       RC_FILE_OK) {
        log_event(st, LOG_FAIL, "Could not write %s", path);
        return false;
    }
    return true;
}

static void save_selected(AppState *st) {
    IconEntry *e;

    if (st->selected < 0)
        return;
    e = &st->files[st->selected];
    if (save_header(st, st->selected))
        log_event(st, LOG_DONE, "Saved %s/%s.h", st->outDir[0] ? st->outDir : ICONV_OUT_DIR,
                  e->stem);
}

static void copy_selected(RC_App *app, AppState *st) {
    if (st->codeLen == 0)
        return;
    rcClipboardSet(st->code);
    st->copiedUntil = rcAppTime(app) + 1.5;
    /* An idle app parks, so ask for the frame that turns the button's label back. */
    rcWindowRequestFrameAfter(rcAppMainWindow(app), 1.6);
    log_event(st, LOG_DONE, "Copied %s to the clipboard", st->files[st->selected].symbol);
}

static void start_batch(AppState *st) {
    if (st->exporting || st->checkedCount == 0)
        return;
    for (int i = 0; i < st->fileCount; i++)
        st->files[i].queued = st->files[i].checked && st->files[i].state != ICON_FAIL
                              && stem_owner(st, i) < 0;
    st->exporting = true;
    st->exportDone = 0;
    st->exportFailed = 0;
}

/* Walks the marks, not an index: the list may have been sorted, cut or cleared since. A file
   unchecked meanwhile stays home, as it did when the walk reached it. */
static void batch_step(AppState *st) {
    int  work = 0;
    bool left = false;

    for (int index = 0; index < st->fileCount; index++) {
        if (!st->files[index].queued)
            continue;
        if (work == ICONV_WRITES) {
            left = true;
            break;
        }
        st->files[index].queued = false;
        if (!st->files[index].checked || st->files[index].state == ICON_FAIL)
            continue;
        work++;
        if (save_header(st, index))
            st->exportDone++;
        else
            st->exportFailed++;
    }
    if (!left) {
        const char *dir = st->outDir[0] ? st->outDir : ICONV_OUT_DIR;
        st->exporting = false;
        if (st->exportFailed == 0)
            log_event(st, LOG_DONE, "Exported %d header%s to %s", st->exportDone,
                      plural_s(st->exportDone), dir);
        else
            log_event(st, LOG_FAIL, "Exported %d of %d headers to %s", st->exportDone,
                      st->exportDone + st->exportFailed, dir);
    }
}

/* The frame's numbers, from one pass: a list this short costs nothing to walk. */
static void tally_files(AppState *st) {
    st->okCount = st->warnCount = st->failCount = st->pendingCount = st->checkedCount = 0;
    st->totalBytes = 0;
    st->visibleCount = 0;
    for (int i = 0; i < st->fileCount; i++) {
        const IconEntry *e = &st->files[i];
        bool shown = st->show == SHOW_ALL || (st->show == SHOW_WARNINGS && e->state == ICON_WARN) ||
                     (st->show == SHOW_FAILED && e->state == ICON_FAIL);

        st->okCount      += e->state == ICON_OK;
        st->warnCount    += e->state == ICON_WARN;
        st->failCount    += e->state == ICON_FAIL;
        st->pendingCount += e->state == ICON_PENDING;
        /* A failed file or a clashing name cannot be written, so neither joins the batch. */
        st->checkedCount += e->checked && e->state != ICON_FAIL && stem_owner(st, i) < 0;
        st->totalBytes   += e->headerBytes;
        if (shown && contains_nocase(path_leaf(e->path), st->filter))
            st->visible[st->visibleCount++] = i;
    }
}

static void update(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    int       work = 0;

    if (!st->started) {
        st->started = true;
        st->mono = rcLoadFont("examples/assets/fonts/AdwaitaMono-Regular.ttf", 12.0f);
    }
    /* THE WINDOW FIRST: the first frame draws the empty panes, and this, on the next, reads the
       start folder (some 140 files in the repository), so the reading never holds the window
       back. Opening on a filled list shows what the tool does. Elsewhere the folder is simply
       not there and the empty state asks for one. */
    if (!st->opened && st->shown) {
        st->opened = true;
        RC_PathInfo info;
        if (rcPathInfo(st->folder, &info) == RC_FILE_OK)
            open_folder(st, st->folder);
        /* A bell's curves show more of the tool than whichever file sorts first. */
        for (int i = 0; i < st->fileCount; i++) {
            if (strcmp(path_leaf(st->files[i].path), "bell.svg") == 0)
                select_entry(st, i);
        }
    }
    for (int i = 0; i < st->fileCount && work < ICONV_PER_FRAME; i++) {
        if (st->files[i].state == ICON_PENDING) {
            convert_entry(st, &st->files[i]);
            work++;
        }
    }
    if (st->exporting)
        batch_step(st);
    /* An idle app parks until something asks for a frame. */
    if (work > 0 || st->exporting)
        rcWindowRequestFrame(rcAppMainWindow(app));
}

static const RcSvgIcon *selected_icon(const AppState *st) {
    if (st->selected < 0)
        return NULL;
    const IconEntry *e = &st->files[st->selected];
    return e->state == ICON_FAIL || e->state == ICON_PENDING ? NULL : e->icon;
}

/* What an icon is drawn in: the chosen tint, or the ground's contrast color. */
static RC_Color icon_color(const AppState *st, bool onLight) {
    if (st->tint > 0)
        return rcColor(TINTS[st->tint]);
    return onLight ? pal.inkOnLight : rcGetStyle().text;
}

/* Custom draw callbacks. rcIconEmit hands each one the element's box and its own pointer. */

static void plain_icon_draw(RC_BoundingBox b, RC_Color color, const void *userData) {
    rc_svg2icon_draw((const RcSvgIcon *)userData, b, color);
}

static void quad_at(RC_IconPoint *q, float x, float y, float w, float h) {
    q[0].x = x;      q[0].y = y;
    q[1].x = x + w;  q[1].y = y;
    q[2].x = x + w;  q[2].y = y + h;
    q[3].x = x;      q[3].y = y + h;
}

static void anchor_dot(RC_BoundingBox b, float vb, float r, float x, float y, RC_Color ink) {
    rcIconDrawFilledCircle(b, x, y, r, vb, RC_WHITE);
    rcIconDrawFilledCircle(b, x, y, r * 0.6f, vb, ink);
}

/* Outline mode: the viewBox grid, every path as a hairline and every stored point as a node. The
   curve steps and arc step sliders change what this shows, because a curve is stored flattened. */
static void anchors_draw(const RcSvgIcon *icon, RC_BoundingBox b) {
    const float vb   = icon->viewW > icon->viewH ? icon->viewW : icon->viewH;
    const float unit = vb / b.width;
    const float line = 1.5f * unit;
    const float dot  = 3.5f * unit;
    RC_Color    edge = pal.selection;
    RC_Color    grid = rcWithAlpha(edge, 0.28f);

    for (int i = 0; vb <= 64.0f && (float)i <= vb; i++) {
        rcIconDrawRoundLine(b, (float)i, 0.0f, (float)i, vb, vb, 0.8f * unit, grid);
        rcIconDrawRoundLine(b, 0.0f, (float)i, vb, (float)i, vb, 0.8f * unit, grid);
    }
    for (int oi = 0; oi < icon->opCount; oi++) {
        const RcSvgOp      *op  = &icon->ops[oi];
        const RC_IconPoint *pts = icon->points + op->pointOff;
        const float        *p   = op->p;

        switch (op->kind) {
        case RC_SVG_ROUND_LINE:
            rcIconDrawRoundLine(b, p[0], p[1], p[2], p[3], vb, line, edge);
            anchor_dot(b, vb, dot, p[0], p[1], edge);
            anchor_dot(b, vb, dot, p[2], p[3], edge);
            break;
        case RC_SVG_POLYLINE:
        case RC_SVG_FILLED_POLY:
            rcIconDrawPolyline(b, pts, op->pointCount, vb, line,
                               op->closed || op->kind == RC_SVG_FILLED_POLY, edge);
            for (int i = 0; i < op->pointCount; i++)
                anchor_dot(b, vb, dot, pts[i].x, pts[i].y, edge);
            break;
        case RC_SVG_CIRCLE_STROKE:
        case RC_SVG_FILLED_CIRCLE:
            rcIconDrawCircleStroke(b, p[0], p[1], p[2], vb, line, edge);
            anchor_dot(b, vb, dot, p[0] - p[2], p[1], edge);
            anchor_dot(b, vb, dot, p[0] + p[2], p[1], edge);
            anchor_dot(b, vb, dot, p[0], p[1] - p[2], edge);
            anchor_dot(b, vb, dot, p[0], p[1] + p[2], edge);
            break;
        case RC_SVG_RRECT_STROKE:
            rcIconDrawRoundedRectStroke(b, p[0], p[1], p[2], p[3], p[4], vb, line, edge);
            anchor_dot(b, vb, dot, p[0], p[1], edge);
            anchor_dot(b, vb, dot, p[0] + p[2], p[1], edge);
            anchor_dot(b, vb, dot, p[0] + p[2], p[1] + p[3], edge);
            anchor_dot(b, vb, dot, p[0], p[1] + p[3], edge);
            break;
        case RC_SVG_FILLED_ELLIPSE:
            anchor_dot(b, vb, dot, p[0] - p[2], p[1], edge);
            anchor_dot(b, vb, dot, p[0] + p[2], p[1], edge);
            anchor_dot(b, vb, dot, p[0], p[1] - p[3], edge);
            anchor_dot(b, vb, dot, p[0], p[1] + p[3], edge);
            break;
        case RC_SVG_FILLED_QUADS:
            /* A fill cut by its fill-rule: the quads it fills, as hairlines. Their
               corners are the cut's, not the path's nodes, so no dots. */
            for (int q = 0; q + 4 <= op->pointCount; q += 4)
                rcIconDrawPolyline(b, pts + q, 4, vb, line, true, edge);
            break;
        }
    }
}

/* The ground and the icon are one element, so nothing can put them out of register. The pale
   tone goes down once as a single quad and the deep cells over it, because abutting fills show a
   seam where the rasterizer rounds the two sides differently. */
static void artboard_draw(RC_BoundingBox b, RC_Color color, const void *userData) {
    const AppState  *st   = (const AppState *)userData;
    const RcSvgIcon *icon = selected_icon(st);
    const float      vb   = (float)ICONV_CELLS;
    RC_IconPoint     quad[4];
    RC_Color pale = st->ground == GROUND_LIGHT ? pal.groundLight
                  : st->ground == GROUND_DARK  ? pal.groundDark : pal.checkerPale;

    quad_at(quad, 0.0f, 0.0f, vb, vb);
    rcIconDrawFilledPolygon(b, quad, 4, vb, pale);
    if (st->ground == GROUND_CHECKER) {
        for (int y = 0; y < ICONV_CELLS; y++) {
            for (int x = y & 1; x < ICONV_CELLS; x += 2) {
                quad_at(quad, (float)x, (float)y, 1.0f, 1.0f);
                rcIconDrawFilledPolygon(b, quad, 4, vb, pal.checkerDeep);
            }
        }
    }
    if (!icon || st->original)
        return;

    /* One cell of margin, so a glyph that runs to its viewBox edge still shows ground around it. */
    RC_BoundingBox inner = b;
    float          m = b.width / vb;
    inner.x += m;  inner.width  -= 2.0f * m;
    inner.y += m;  inner.height -= 2.0f * m;
    if (st->anchors) {
        /* Mixed to an opaque color rather than made translucent: a stroke's segments overlap at
           each join, and alpha would show every overlap as a darker rib. */
        RC_Color under = st->ground == GROUND_LIGHT ? pal.groundLight : pal.checkerDeep;
        RC_Color dim   = { under.r + (color.r - under.r) * 0.4f,
                           under.g + (color.g - under.g) * 0.4f,
                           under.b + (color.b - under.b) * 0.4f, 255.0f };
        rc_svg2icon_draw(icon, inner, dim);
        anchors_draw(icon, inner);
    } else {
        rc_svg2icon_draw(icon, inner, color);
    }
}

/* Small controls shared by all three panes. */

static void section_title(const char *title) {
    rcTextC(title, .font = rcFontSemibold(12.0f), .color = rcGetStyle().text, .wrap = "n");
}

static void hairline(void) {
    rcBox(.bg = rcGetStyle().border, .w = "grow", .h = "1px") {}
}

/* A button with no label says what it does on dwell. Under a finger it drops h-7 and keeps
   rcButton's own 44 px row. */
static bool tool_button(const char *id, const char *tip, RC_IconCallback icon, bool disabled) {
    const char *size = rcViewport().coarsePointer ? NULL : "h-7";
    bool        hit  = false;

    rcBox(.tooltip = tip) {
        hit = rcButton(id, "", .disabled = disabled, .className = size, .variant = RC_BTN_GHOST,
                       .leadingIcon = icon);
    }
    return hit;
}

/* A segment is a box read with rcClicked, so .focusable puts it in the Tab order beside the
   bundled controls. It grows to a finger's height under a coarse pointer. */
static bool seg_button(const char *id, const char *label, bool active) {
    RC_Style s = rcGetStyle();
    float    h = rcViewport().coarsePointer ? 40.0f : 24.0f;

    rcBox(.id = id, .focusable = true, .px = 8, .align = "cc", .borderRadius = "all-sm",
          .w = "grow", .hType = RC_PX(h),
          .className = active ? "bg-primary" : "hover:bg-border") {
        rcTextC(label, .color = active ? RC_WHITE : s.textMuted, .size = 12, .wrap = "n",
                .select = RC_SELECT_NONE);
    }
    return rcClicked(id);
}

/* The left pane: where icons come in, and which of them go out. */

static void list_row(RC_App *app, AppState *st, int index, float rowH) {
    RC_Style   s  = rcGetStyle();
    RC_Arena  *ar = rcAppArena(app);
    IconEntry *e  = &st->files[index];
    bool       on = index == st->selected;
    /* The ids follow the file, not its place: a sort, a removal or the filter moves the row, and
       its focus, press and open menu move with it. */
    const char *id   = rcFormat(ar, "row-%u", e->key).chars;
    const char *box  = rcFormat(ar, "chk-%u", e->key).chars;
    const char *menu = rcFormat(ar, "ctx-%u", e->key).chars;

    /* The selection is a typed fill, which beats the class's hover as an inline style beats a
       stylesheet; RC_UNSET_COLOR sets no fill, so the hover shows on every other row. */
    rcRow(.id = id, .bg = on ? rcWithAlpha(pal.selection, 0.32f) : RC_UNSET_COLOR,
          .gap = 8, .pl = 12, .pr = 8, .align = "cl", .overflow = "hidden", .w = "grow",
          .hType = RC_PX(rowH), .className = "hover:bg-surface-alt") {
        /* Art its coordinates or a transform put outside its viewBox stays in its slot. */
        rcBox(.overflow = "hidden") {
            rcIconEmit(20.0f, s.text, plain_icon_draw, e->icon);
        }
        rcBox(.align = "cl", .overflow = "hidden", .w = "grow") {
            rcTextC(path_leaf(e->path), .color = e->state == ICON_FAIL ? s.textMuted : s.text,
                    .wrap = "e", .select = RC_SELECT_NONE);
        }
        /* A name another file holds is as unexportable as a failed file, so it wears that mark,
           stays out of the batch, and its box shows clear and cannot be checked. */
        const bool cannotExport = e->state == ICON_FAIL || stem_owner(st, index) >= 0;
        if (cannotExport)
            rcIconCircleX(14.0f, s.danger);
        else if (e->state == ICON_WARN)
            rcIconTriangleAlert(14.0f, s.warning);
        bool cannot = false;
        if (cannotExport) rcCheckbox(box, "", &cannot, .disabled = true);
        else               rcCheckbox(box, "", &e->checked);
    }
    if (rcClicked(id)) {
        select_entry(st, index);
        st->tab = TAB_PREVIEW;
    }
    if (rcBeginContextMenu(menu, id)) {
        st->rowMenuUp = true;
        if (rcMenuItem("Copy C", .disabled = e->state == ICON_FAIL)) {
            select_entry(st, index);
            copy_selected(app, st);
        }
        if (rcMenuItem("Save header",
                       .disabled = e->state == ICON_FAIL || stem_owner(st, index) >= 0)) {
            select_entry(st, index);
            save_selected(st);
        }
        if (rcMenuItem("Remove from list", .className = "text-danger hover:bg-danger/10"))
            st->removeIndex = index;
        rcEndContextMenu();
    }
}

static void pane_batch(RC_App *app, AppState *st, bool side) {
    RC_Style  s    = rcGetStyle();
    RC_Arena *ar   = rcAppArena(app);
    bool      finger = rcViewport().coarsePointer;
    float     rowH = finger ? 44.0f : 32.0f;
    st->listDrawn = true;
    rcColumn(.id = "pane_batch", .bg = s.surface, .border = { .color = s.border }, .h = "grow",
             .wType = side ? RC_PX(ICONV_BATCH_W) : RC_GROW,
             .className = side ? "border-r" : NULL) {
        rcRow(.gap = 4, .pt = 8, .pb = 4, .pl = 12, .pr = 8, .align = "cl", .w = "grow") {
            rcTextL("Icons", .font = rcFontSemibold(14.0f), .wrap = "n");
            rcBox(.w = "4px") {}
            rcBox(.bg = s.surfaceAlt, .px = 8, .py = 1, .borderRadius = "all-full") {
                rcText(rcFormat(ar, "%d", st->fileCount), .color = s.textMuted, .size = 12,
                       .wrap = "n");
            }
            rcSpacer() {}
            if (!st->noDialogs) {
                if (tool_button("open_folder", "Open folder", rcIconFolder, false))
                    ask_for_files(st, RC_FILE_DIALOG_FOLDER);
                if (tool_button("add_files", "Add SVG files", rcIconPlus, false))
                    ask_for_files(st, RC_FILE_DIALOG_FILES);
            }
        }
        rcColumn(.gap = 8, .pb = 8, .px = 12, .w = "grow") {
            rcTextInput("folder", st->folder, ICONV_PATH_CAP, .className = "h-7",
                        .placeholder = "Type a folder, press Enter", .leadingIcon = rcIconFolder,
                        .clearButton = true);
            rcTextInput("filter", st->filter, (int)sizeof st->filter, .className = "h-7",
                        .placeholder = "Filter icons", .leadingIcon = rcIconSearch,
                        .clearButton = true);
            rcRow(.bg = s.surfaceAlt, .gap = 2, .p = 2, .borderRadius = "all-md", .w = "grow") {
                if (seg_button("show_all", rcFormat(ar, "All %d", st->fileCount).chars,
                               st->show == SHOW_ALL))
                    st->show = SHOW_ALL;
                if (seg_button("show_warn", rcFormat(ar, "Warnings %d", st->warnCount).chars,
                               st->show == SHOW_WARNINGS))
                    st->show = SHOW_WARNINGS;
                if (seg_button("show_fail", rcFormat(ar, "Failed %d", st->failCount).chars,
                               st->show == SHOW_FAILED))
                    st->show = SHOW_FAILED;
            }
        }
        hairline();
        /* Rows are virtual, so the selected one may not be declared: rcScrollToRow reveals it
           from the list's own pitch. */
        RC_Box view = rcGetElementBox("list");
        /* A window or tab that shrinks the list keeps the selected row on show. */
        if (view.found && view.height != st->listHeight) {
            st->listHeight = view.height;
            st->reveal = true;
        }
        if (st->reveal) {
            for (int i = 0; i < st->visibleCount; i++)
                if (st->visible[i] == st->selected)
                    rcScrollToRow("list", i, RC_SCROLL_NEAREST);
            st->reveal = false;
        }
        /* The bar takes its own 12 px of the pane while the rows overflow, so a row's checkbox
           and its fill both end where the bar begins. */
        rcColumn(.id = "list", .scroll = "v", .w = "grow", .h = "grow") {
            /* Before the start folder is read the list says nothing, rather than "nothing". */
            if (st->visibleCount == 0 && st->opened) {
                rcColumn(.p = 16, .w = "grow") {
                    rcTextC(st->fileCount == 0 ? "Nothing here yet." : "No icons match.",
                            .color = s.textMuted);
                }
            }
            rcVirtualList(row, "list", st->visibleCount, rowH) {
                list_row(app, st, st->visible[row.index], rowH);
            }
        }
        rcScrollbar("list");
        hairline();
        rcRow(.gap = 4, .pl = 12, .pr = 8, .py = 4, .align = "cl", .w = "grow") {
            rcText(rcFormat(ar, "%d of %d in batch", st->checkedCount, st->fileCount),
                   .color = s.textMuted, .size = 12, .wrap = "n");
            rcSpacer() {}
            bool setAll  = rcButton("batch_all", "All", .className = "h-6",
                                    .variant = RC_BTN_GHOST);
            bool setNone = rcButton("batch_none", "None", .className = "h-6",
                                    .variant = RC_BTN_GHOST);
            /* They act on the rows on show, so a filter picks what a batch takes. */
            for (int i = 0; i < st->visibleCount && (setAll || setNone); i++)
                st->files[st->visible[i]].checked = setAll;
        }
    }
}

/* The center pane: the artboard, the tools that change it, and the size strip. */

static void canvas_header(RC_App *app, AppState *st, bool stacked) {
    RC_Style s = rcGetStyle();

    rcBox(.bg = s.surface, .gap = 8, .px = 16, .py = 8, .w = "grow",
          .className = stacked ? "flex flex-col" : "flex flex-row items-center") {
        rcRow(.gap = 8, .align = "cl", .overflow = "hidden", .w = "grow") {
            if (st->selected >= 0) {
                const IconEntry *e = &st->files[st->selected];
                const RcSvgIcon *ic = e->icon;
                rcTextC(path_leaf(e->path), .font = rcFontSemibold(15.0f), .wrap = "n");
                if (selected_icon(st)) {
                    rcBox(.align = "cl", .overflow = "hidden", .w = "grow") {
                        rcText(rcFormat(rcAppArena(app), "%.0f x %.0f, %d shape%s, %d point%s, %s",
                                        ic->viewW, ic->viewH, ic->opCount, plural_s(ic->opCount),
                                        ic->pointCount, plural_s(ic->pointCount),
                                        ic->colored ? "color" : "mono"),
                               .color = s.textMuted, .size = 12, .wrap = "e");
                    }
                }
            } else {
                rcTextL("No icon selected", .font = rcFontSemibold(15.0f), .color = s.textMuted);
            }
        }
        rcRow(.gap = 8, .align = "cl", .disabled = !selected_icon(st)) {
            rcRow(.bg = s.surfaceAlt, .gap = 2, .p = 2, .borderRadius = "all-md") {
                if (seg_button("view_conv", "Converted", !st->original))
                    st->original = false;
                if (seg_button("view_orig", "Original", st->original))
                    st->original = true;
            }
            rcRow(.bg = s.surfaceAlt, .p = 2, .borderRadius = "all-md",
                  .disabled = st->original) {
                if (seg_button("view_anchors", "Anchors", st->anchors && !st->original))
                    st->anchors = !st->anchors;
            }
        }
    }
    hairline();
}

/* Above the artboard: what the parser could not honor, in its own words. */
static void warning_strip(const RcSvgIcon *icon) {
    RC_Style s = rcGetStyle();

    rcColumn(.bg = rcWithAlpha(s.warning, 0.12f), .gap = 4, .px = 16, .py = 8, .w = "grow") {
        for (int i = 0; i < icon->warnCount && i < 3; i++) {
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                rcIconTriangleAlert(14.0f, s.warning);
                rcBox(.w = "grow") {
                    rcTextC(icon->warn[i], .color = s.warning, .size = 12);
                }
            }
        }
    }
}

static void drop_prompt(AppState *st) {
    RC_Style s = rcGetStyle();

    rcColumn(.gap = 12, .p = 32, .align = "cc", .borderRadius = "all-lg",
             .border = { .color = s.border, .width = 1 }) {
        rcIconUpload(40.0f, s.textMuted);
        rcTextL("Drop SVG files or a folder here", .font = rcFontSemibold(16.0f));
        rcTextL("Each one becomes a C header you call like any RayClay icon.",
                .color = s.textMuted, .size = 13);
        if (!st->noDialogs) {
            rcRow(.gap = 8, .pt = 4) {
                if (rcButton("empty_open", "Open folder", .className = "h-8",
                             .variant = RC_BTN_PRIMARY, .leadingIcon = rcIconFolder))
                    ask_for_files(st, RC_FILE_DIALOG_FOLDER);
                if (rcButton("empty_add", "Add files", .className = "h-8",
                             .leadingIcon = rcIconPlus))
                    ask_for_files(st, RC_FILE_DIALOG_FILES);
            }
        }
    }
}

static void artboard(AppState *st) {
    RC_Style  s    = rcGetStyle();
    RC_Box    area = rcGetElementBox("canvas");
    float     fit  = area.found ? (area.width < area.height ? area.width : area.height) - 40.0f
                                : 320.0f;
    float     size = floorf((fit < 96.0f ? 96.0f : fit) * (float)ZOOMS[st->zoomStep] / 100.0f);
    const bool onLight = st->ground == GROUND_LIGHT;
    const RcSvgIcon *icon = selected_icon(st);

    rcBox(.id = "artboard", .overflow = "hidden", .border = { .color = s.border, .width = 1 }) {
        rcIconEmit(size, icon_color(st, onLight), artboard_draw, st);
        if (st->original && icon) {
            /* The same bytes through the library's own SVG path, drawn over the same ground. */
            IconEntry *e = &st->files[st->selected];
            /* Once per selection: a file the library's parser refuses would be parsed again,
               and log its error again, on every frame the artboard draws. */
            if (!st->originalSvg && !st->originalTried) {
                st->originalSvg = rcLoadSvgFromMemory(e->svg, e->svgLen);
                st->originalTried = true;
            }
            if (st->originalSvg) {
                rcBox(.floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_CENTER,
                                    .element = RC_ANCHOR_CENTER }) {
                    rcSvgHandle(st->originalSvg, size * (1.0f - 2.0f / (float)ICONV_CELLS),
                                icon_color(st, onLight));
                }
            }
        }
    }
}

/* A zoom keeps the artboard's middle in view, as a design tool's does. The new size is laid out
   the frame after the click and its scroll range is known the frame after that. */
static void set_zoom(AppState *st, int step) {
    st->zoomStep = step;
    st->recenter = 2;
}

static void canvas_tools(RC_App *app, AppState *st) {
    RC_Style s = rcGetStyle();
    RC_Arena *ar = rcAppArena(app);
    const RcSvgIcon *icon = selected_icon(st);
    const bool finger = rcViewport().coarsePointer;

    hairline();
    rcRow(.bg = s.surface, .gap = 8, .px = 16, .py = 6, .align = "cl", .w = "grow",
          .className = "flex-wrap") {
        rcRow(.bg = s.surfaceAlt, .gap = 2, .p = 2, .borderRadius = "all-md") {
            if (seg_button("ground_checker", "Checker", st->ground == GROUND_CHECKER))
                st->ground = GROUND_CHECKER;
            if (seg_button("ground_light", "Light", st->ground == GROUND_LIGHT))
                st->ground = GROUND_LIGHT;
            if (seg_button("ground_dark", "Dark", st->ground == GROUND_DARK))
                st->ground = GROUND_DARK;
        }
        /* Color artwork brings its own colors, so a tint has nothing to do on it. Under a finger
           each swatch pads its 14 px dot out to a 42 px target. */
        rcRow(.gap = finger ? 8.0f : 4.0f, .px = 4, .align = "cl",
              .disabled = !icon || icon->colored) {
            for (int i = 0; i < TINT_COUNT; i++) {
                RC_Color ink = i == 0 ? s.text : rcColor(TINTS[i]);
                rcBox(.id = SWATCH_IDS[i], .focusable = true, .p = finger ? 13.0f : 2.0f,
                      .borderRadius = "all-full", .tooltip = TINT_NAMES[i],
                      .className = st->tint == i ? "border border-foreground"
                                   : "border border-transparent hover:border-muted-foreground") {
                    rcBox(.bg = ink, .borderRadius = "all-full", .w = "14px", .h = "14px") {}
                }
                if (rcClicked(SWATCH_IDS[i]))
                    st->tint = i;
            }
        }
        rcRow(.gap = 2, .align = "cr", .w = "grow") {
            if (tool_button("zoom_out", "Zoom out", rcIconMinus, st->zoomStep == 0))
                set_zoom(st, st->zoomStep - 1);
            /* A fixed width, so "25%" and "Fit" leave the zoom buttons where the pointer is. */
            rcBox(.w = "48px", .tooltip = "Back to fit") {
                const char *label = st->zoomStep == ZOOM_FIT
                                  ? "Fit" : rcFormat(ar, "%d%%", ZOOMS[st->zoomStep]).chars;
                if (seg_button("zoom_fit", label, false))
                    set_zoom(st, ZOOM_FIT);
            }
            if (tool_button("zoom_in", "Zoom in", rcIconPlus, st->zoomStep == ZOOM_STEPS - 1))
                set_zoom(st, st->zoomStep + 1);
        }
    }
}

static void size_group(RC_App *app, const AppState *st, bool onLight) {
    static const int SIZES[] = { 16, 20, 24, 32, 48 };
    RC_Style s = rcGetStyle();
    RC_Arena *ar = rcAppArena(app);
    const RcSvgIcon *icon = selected_icon(st);

    /* Bottom-aligned, so the five captions share one baseline. */
    rcRow(.bg = onLight ? pal.groundLight : s.background, .gap = 16, .pt = 8, .pb = 6, .px = 16,
          .align = "bc", .borderRadius = "all-md",
          .border = { .color = s.border, .width = 1 }, .w = "grow") {
        for (int i = 0; i < 5; i++) {
            rcColumn(.gap = 4, .align = "bc") {
                rcBox(.overflow = "hidden") {
                    rcIconEmit((float)SIZES[i], icon_color(st, onLight), plain_icon_draw, icon);
                }
                rcText(rcFormat(ar, "%d", SIZES[i]),
                       .color = onLight ? pal.captionOnLight : s.textMuted, .size = 11,
                       .wrap = "n", .select = RC_SELECT_NONE);
            }
        }
    }
}

static void pane_canvas(RC_App *app, AppState *st, bool stacked) {
    RC_Style s = rcGetStyle();
    const RcSvgIcon *icon = selected_icon(st);
    const IconEntry *sel  = st->selected >= 0 ? &st->files[st->selected] : NULL;
    /* A short window keeps the artboard and gives up the strip. */
    bool showStrip = icon && rcViewport().height >= 560.0f;

    st->canvasDrawn = true;
    /* min-w-0, as on the web: a zoomed artboard scrolls the canvas instead of widening the pane. */
    rcColumn(.id = "pane_canvas", .bg = s.background, .w = "grow", .h = "grow",
             .className = "min-w-0") {
        canvas_header(app, st, stacked);
        /* A file that failed still says what the parser met on the way. */
        if (sel && sel->state != ICON_PENDING && sel->icon->warnCount > 0)
            warning_strip(sel->icon);
        rcBox(.id = "canvas", .p = 16, .align = "CC", .scroll = "b", .w = "grow", .h = "grow",
              .hMin = RC_VAL(120.0f)) {
            if (st->fileCount == 0) {
                if (st->opened)
                    drop_prompt(st);
            } else if (st->selected >= 0 && st->files[st->selected].state == ICON_FAIL) {
                rcColumn(.gap = 8, .align = "cc") {
                    rcIconCircleX(40.0f, s.danger);
                    rcTextL("This file did not convert", .font = rcFontSemibold(16.0f));
                    rcTextC(st->files[st->selected].icon->err, .color = s.textMuted);
                }
            } else {
                artboard(st);
            }
        }
        if (st->recenter > 0 && --st->recenter == 0) {
            RC_ScrollInfo info = rcGetScrollInfo("canvas");
            rcScrollBy("canvas", info.maxOffsetX * 0.5f - info.offsetX,
                       info.maxOffsetY * 0.5f - info.offsetY);
            rcWindowRequestFrame(rcAppMainWindow(app));
        }
        rcScrollbar("canvas");
        canvas_tools(app, st);
        if (showStrip) {
            rcRow(.bg = s.surface, .gap = 8, .pt = 4, .pb = 12, .px = 16, .align = "bc",
                  .w = "grow", .className = "flex-wrap") {
                /* A phone keeps the artboard large and shows the strip on the current ground. */
                size_group(app, st, stacked && st->ground == GROUND_LIGHT);
                if (!stacked) size_group(app, st, true);
            }
        }
    }
}

/* The right pane: what the file will be called, how fine it is, and the C itself. */

static void form_label(const char *label) {
    rcBox(.w = "84px") {
        rcTextC(label, .color = rcGetStyle().textMuted, .size = 12, .wrap = "n");
    }
}

static void quality_row(RC_App *app, AppState *st, const char *id, const char *label, float *value,
                        float min, float max, float step, const char *fmt) {
    bool moved = false;

    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        form_label(label);
        rcBox(.align = "cl", .w = "grow") {
            moved = rcSlider(id, value, min, max, .step = step);
        }
        rcBox(.align = "cr", .w = "52px") {
            rcText(rcFormat(rcAppArena(app), fmt, (double)*value), .size = 12, .wrap = "n");
        }
    }
    if (moved)
        quality_changed(st);
}

static bool word_is(const char *word, int len, const char *name) {
    return (int)strlen(name) == len && strncmp(word, name, (size_t)len) == 0;
}

/* A word's syntax color, when it has one: numbers, keywords, RayClay's types and its calls. */
static bool token_color(const char *word, int len, RC_Color *out) {
    static const char *const KEYWORDS[] = { "static", "inline", "void", "const", "float", "int",
                                            "bool", "true", "false", "NULL" };
    if (isdigit((unsigned char)word[0]) || (word[0] == '.' && len > 1)) {
        *out = pal.number;
        return true;
    }
    for (size_t i = 0; i < sizeof KEYWORDS / sizeof KEYWORDS[0]; i++) {
        if (word_is(word, len, KEYWORDS[i])) {
            *out = pal.keyword;
            return true;
        }
    }
    if (len > 3 && strncmp(word, "RC_", 3) == 0) {
        *out = pal.type;
        return true;
    }
    if (len > 2 && strncmp(word, "rc", 2) == 0 && isupper((unsigned char)word[2])) {
        *out = pal.function;
        return true;
    }
    return false;
}

static void code_run(const AppState *st, const char *text, int len, RC_Color color) {
    RC_String run = { .length = len, .chars = text };

    if (len > 0)
        rcText(run, .font = st->mono, .color = color, .wrap = "n");
}

/* One line of the listing, as runs that borrow the header text: only visible lines cost. */
static void code_line(const AppState *st, int line) {
    const char *text = st->code + st->lineStart[line];
    int         len  = st->lineStart[line + 1] - st->lineStart[line] - 1;
    int         run  = 0;
    int         i    = 0;

    if (st->lineIsComment[line] || (len > 0 && text[0] == '#')) {
        code_run(st, text, len, st->lineIsComment[line] ? pal.comment : pal.directive);
        return;
    }
    while (i < len) {
        if (isalnum((unsigned char)text[i]) || text[i] == '_') {
            int      start = i;
            RC_Color color;

            while (i < len && (isalnum((unsigned char)text[i]) || text[i] == '_' || text[i] == '.'))
                i++;
            if (token_color(text + start, i - start, &color)) {
                code_run(st, text + run, start - run, pal.code);
                code_run(st, text + start, i - start, color);
                run = i;
            }
        } else {
            i++;
        }
    }
    code_run(st, text + run, len - run, pal.code);
}

static void code_listing(RC_App *app, AppState *st) {
    RC_Style s     = rcGetStyle();
    float    chars = rcTextWidth(st->mono, 12, "0", 1);

    /* Each bar takes its own 12 px of the pane while the listing overflows that way, so
       scrolled to an end, the longest line's end and the last line clear them. */
    rcColumn(.id = "code", .bg = s.background, .pt = 4, .scroll = "b",
             .borderRadius = "all-md", .border = { .color = s.border, .width = 1 },
             .w = "grow", .h = "grow", .hMin = RC_VAL(140.0f)) {
        rcVirtualList(row, "code", st->lineCount, (float)ICONV_LINE_H) {
            /* Every row is as wide as the longest, so the sideways range stays put while the
               list declares different rows. */
            rcRow(.align = "cl", .hType = RC_PX(ICONV_LINE_H),
                  .wMin = RC_VAL((float)ICONV_GUTTER_W + 8.0f + chars * (float)st->longestLine)) {
                rcBox(.pr = 12, .align = "cr", .w = "40px") {
                    rcText(rcFormat(rcAppArena(app), "%d", row.index + 1), .font = st->mono,
                           .color = pal.lineNumber, .wrap = "n", .select = RC_SELECT_NONE);
                }
                code_line(st, row.index);
            }
        }
    }
    rcScrollbar("code");
}

/* The export pane's settings: what the selected icon is called, where it goes, how fine it is,
   and the C it makes. */
static void export_settings(RC_App *app, AppState *st, int owner) {
    RC_Style  s     = rcGetStyle();
    RC_Arena *ar    = rcAppArena(app);
    bool      have  = st->selected >= 0;
    bool      taken = owner >= 0;
    /* The form's own fields, 28 px tall. A field's text starts 10 px in, 9 and the frame's
       hairline, and so does the "Call as" value under them. A name another file already makes
       is edged in the theme's danger red. */
    const char *fieldClass = "h-7 pl-[9px]";
    const char *nameClass  = taken ? "h-7 pl-[9px] border-danger" : fieldClass;

    rcColumn(.gap = 8, .pt = 14, .pb = 12, .px = 12, .disabled = !have, .w = "grow") {
        section_title("Output");
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            form_label("Name");
            /* A disabled field still needs a buffer while nothing is selected. */
            static char noStem[2];
            char *stem = have ? st->files[st->selected].stem : noStem;
            /* The x empties the name the way typing does, so the symbol and the header follow. */
            if (rcTextInput("stem", stem, have ? ICONV_NAME_CAP : 2, .disabled = !have,
                            .className = nameClass, .clearButton = true) && have) {
                IconEntry *e = &st->files[st->selected];
                rc_svg2icon_symbol(e->stem, e->symbol, ICONV_SYMBOL_CAP);
                refresh_code(st);
            }
        }
        if (taken) {
            rcRow(.gap = 6, .pl = 92, .align = "cl", .w = "grow") {
                rcIconCircleX(14.0f, s.danger);
                rcBox(.align = "cl", .overflow = "hidden", .w = "grow") {
                    rcText(rcFormat(ar, "%s already makes %s",
                                    path_leaf(st->files[owner].path),
                                    st->files[owner].symbol),
                           .color = s.danger, .size = 12, .wrap = "e");
                }
            }
        }
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            form_label("Folder");
            /* Typed, not picked: RC_FILE_DIALOG_FOLDER hands over every file under a folder,
               which is a way in, not a place to write. */
            rcTextInput("outdir", st->outDir, ICONV_PATH_CAP, .className = fieldClass,
                        .placeholder = ICONV_OUT_DIR, .clearButton = true);
        }
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            form_label("Call as");
            rcBox(.pl = 10, .align = "cl", .overflow = "hidden", .w = "grow") {
                /* A colored icon bakes its own colors, so its function takes the size alone. */
                const IconEntry *e = have ? &st->files[st->selected] : NULL;
                bool colored = e && e->state != ICON_FAIL && e->state != ICON_PENDING && e->icon
                               && e->icon->colored;
                rcTextC(e ? rcFormat(ar, "%s(size%s)", e->symbol,
                                     colored ? "" : ", color").chars : "-",
                        .font = st->mono, .color = pal.function, .wrap = "e");
            }
        }
    }
    hairline();
    rcColumn(.gap = 8, .pt = 12, .pb = 12, .px = 12, .w = "grow") {
        section_title("Quality");
        quality_row(app, st, "q_curve", "Curve steps", &st->curveSteps, 2.0f, 32.0f, 1.0f,
                    "%.0f");
        quality_row(app, st, "q_arc", "Arc step", &st->arcStep, 2.0f, 30.0f, 0.5f,
                    "%.1f deg");
        quality_row(app, st, "q_prec", "Precision", &st->precision, 0.0f, 6.0f, 1.0f,
                    "%.0f dp");
    }
    hairline();
    /* It clips, so it takes the room left rather than the listing's whole length, and past
       its floor the settings scroll. */
    rcColumn(.gap = 8, .pt = 12, .pb = 12, .px = 12, .overflow = "hidden", .w = "grow",
             .h = "grow", .hMin = RC_VAL(220.0f)) {
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            section_title("Generated C");
            rcSpacer() {}
            if (have && st->codeLen > 0) {
                rcText(rcFormat(ar, "%d lines, %s", st->lineCount,
                                format_bytes(ar, st->codeLen).chars),
                       .color = s.textMuted, .size = 12, .wrap = "n");
            }
        }
        if (st->codeLen > 0) {
            code_listing(app, st);
        } else {
            rcBox(.bg = s.background, .p = 16, .align = "cc", .borderRadius = "all-md",
                  .border = { .color = s.border, .width = 1 }, .w = "grow", .h = "grow") {
                rcTextC(have ? "This file did not convert, so it has no header."
                             : "Pick an icon to see its header.",
                        .color = s.textMuted, .size = 12);
            }
        }
    }
}

static void pane_export(RC_App *app, AppState *st, bool side) {
    RC_Style  s    = rcGetStyle();
    RC_Arena *ar   = rcAppArena(app);
    bool      have = st->selected >= 0;
    int       owner = have ? stem_owner(st, st->selected) : -1;
    bool      taken = owner >= 0;

    /* The settings scroll in a short window; the actions stay pinned at the bottom, as Figma
       pins its Export button. The scrollbar takes its own room beside the settings. */
    rcColumn(.id = "pane_export", .bg = s.surface, .border = { .color = s.border },
             .h = "grow", .wType = side ? RC_PX(ICONV_EXPORT_W) : RC_GROW,
             .className = side ? "border-l" : NULL) {
        rcColumn(.id = "export_body", .scroll = "v", .w = "grow", .h = "grow") {
            export_settings(app, st, owner);
        }
        rcScrollbar("export_body");
        hairline();
        /* The actions end where the settings do, 12 px in: the bar's band is the settings' own. */
        rcColumn(.gap = 8, .px = 12, .py = 12, .w = "grow") {
            if (st->exporting) {
                rcProgress("batch_progress", (float)(st->exportDone + st->exportFailed) /
                                             (float)(st->checkedCount > 0 ? st->checkedCount : 1));
            }
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                bool copied = rcAppTime(app) < st->copiedUntil;
                /* A window keeps only its earliest one-shot timer, and a second copy ends later
                   than the first: every frame that shows "Copied" asks for the frame that ends
                   it. */
                if (copied)
                    rcWindowRequestFrameAfter(rcAppMainWindow(app),
                                              st->copiedUntil - rcAppTime(app));
                if (rcButton("copy_c", copied ? "Copied" : "Copy C",
                             .disabled = st->codeLen == 0, .className = "h-8",
                             .leadingIcon = copied ? rcIconCheck : rcIconCode))
                    copy_selected(app, st);
                if (rcButton("save_h", "Save .h",
                             .disabled = st->codeLen == 0 || taken, .className = "h-8",
                             .leadingIcon = rcIconDownload))
                    save_selected(st);
                rcSpacer() {}
                if (rcButton("export_all", rcFormat(ar, "Export %d icon%s", st->checkedCount,
                                                    plural_s(st->checkedCount)).chars,
                             .disabled = st->checkedCount == 0 || st->exporting,
                             .className = "h-8", .variant = RC_BTN_PRIMARY))
                    start_batch(st);
            }
        }
    }
}

/* The bottom bar: the last thing that happened, the whole batch in one line, and the way in to
   the full activity log. */
static void status_bar(RC_App *app, AppState *st, float bottom) {
    RC_Style  s  = rcGetStyle();
    RC_Arena *ar = rcAppArena(app);

    hairline();
    rcRow(.bg = s.surface, .gap = 8, .pb = (float)bottom, .pl = 12, .pr = 4, .align = "cl",
          .w = "grow", .hMin = RC_VAL(28.0f + bottom)) {
        rcRow(.gap = 6, .align = "cl", .overflow = "hidden", .w = "grow") {
            if (st->exporting) {
                rcText(rcFormat(ar, "Exporting %d of %d", st->exportDone + st->exportFailed,
                                st->checkedCount), .color = s.textMuted, .size = 12, .wrap = "n");
            } else if (st->pendingCount > 0) {
                rcText(rcFormat(ar, "Converting, %d left", st->pendingCount), .color = s.textMuted,
                       .size = 12, .wrap = "n");
            } else if (st->logCount > 0) {
                int     last = (st->logHead + st->logCount - 1) % ICONV_LOG_LINES;
                LogKind kind = st->logKind[last];
                if (kind == LOG_FAIL)
                    rcIconCircleX(14.0f, s.danger);
                else if (kind == LOG_DONE)
                    rcIconCircleCheck(14.0f, s.success);
                else
                    rcIconInfo(14.0f, s.textMuted);
                rcBox(.align = "cl", .overflow = "hidden", .w = "grow") {
                    rcTextC(st->log[last], .color = kind == LOG_FAIL ? s.danger : s.textMuted,
                            .size = 12, .wrap = "e");
                }
            }
        }
        if (rcViewport().width >= (float)ICONV_NARROW_W) {
            rcText(rcFormat(ar, "%d ready", st->okCount + st->warnCount), .color = s.textMuted,
                   .size = 12, .wrap = "n");
            if (st->warnCount > 0) {
                rcText(rcFormat(ar, "%d warning%s", st->warnCount, plural_s(st->warnCount)),
                       .color = s.warning, .size = 12, .wrap = "n");
            }
            if (st->failCount > 0) {
                rcText(rcFormat(ar, "%d failed", st->failCount), .color = s.danger, .size = 12,
                       .wrap = "n");
            }
            rcText(rcFormat(ar, "%s of C", format_bytes(ar, st->totalBytes).chars),
                   .color = s.textMuted, .size = 12, .wrap = "n");
        }
        if (rcButton("activity", "Activity", .className = "h-6", .variant = RC_BTN_GHOST,
                     .leadingIcon = rcIconList))
            st->logOpen = true;
    }
}

static void activity_modal(AppState *st) {
    RC_Style s = rcGetStyle();

    if (!rcBeginModal("activity_modal", &st->logOpen))
        return;
    rcColumn(.gap = 12, .w = "520px", .wMax = RC_VAL(rcViewport().width - 48.0f)) {
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            rcTextL("Activity", .font = rcFontSemibold(15.0f));
            rcSpacer() {}
            if (rcButton("log_clear", "Clear", .disabled = st->logCount == 0, .className = "h-7",
                         .variant = RC_BTN_GHOST)) {
                st->logCount = 0;
                st->logHead = 0;
            }
        }
        rcColumn(.id = "log_list", .bg = s.background, .gap = 4, .p = 12, .scroll = "v",
                 .borderRadius = "all-md", .w = "grow", .h = "300px") {
            if (st->logCount == 0)
                rcTextL("Nothing yet.", .color = s.textMuted);
            for (int i = 0; i < st->logCount; i++) {
                int idx = (st->logHead + i) % ICONV_LOG_LINES;
                rcTextC(st->log[idx],
                        .color = st->logKind[idx] == LOG_FAIL ? s.danger
                               : st->logKind[idx] == LOG_DONE ? s.success : s.textMuted,
                        .size = 12);
            }
        }
        rcScrollbar("log_list");
        rcRow(.align = "cr", .w = "grow") {
            if (rcButton("log_close", "Close", .className = "h-8", .variant = RC_BTN_PRIMARY))
                st->logOpen = false;
        }
    }
    rcEndModal();
}

/* Up and Down walk what the filter shows; a focused field keeps its own keys. */
static void handle_keys(RC_App *app, AppState *st) {
    bool primary = rcModDown(RC_MOD_PRIMARY);
    bool typing  = rcIsFocused("folder") || rcIsFocused("filter") || rcIsFocused("stem") ||
                   rcIsFocused("outdir");

    /* A row's open menu owns the keys, as a dialog does; the flag is last frame's. */
    const bool menuUp = st->rowMenuUp;
    st->rowMenuUp = false;
    if (rcIsModalOpen() || menuUp)
        return;
    if (rcKeyPressedOn("folder", RC_KEY_ENTER))
        open_folder(st, st->folder);
    if (primary && rcKeyPressed(RC_KEY_F))
        rcSetFocus("filter");
    if (primary && rcKeyPressed(RC_KEY_O) && !st->noDialogs)
        ask_for_files(st, RC_FILE_DIALOG_FOLDER);
    if (primary && rcKeyPressed(RC_KEY_S))
        save_selected(st);
    if (primary && rcKeyPressed(RC_KEY_E))
        start_batch(st);
    if (st->filter[0] && rcKeyPressed(RC_KEY_ESCAPE))
        st->filter[0] = '\0';
    if (typing)
        return;
    if (primary && rcKeyPressed(RC_KEY_C) && !rcHasSelection())
        copy_selected(app, st);
    if (rcKeyPressed(RC_KEY_SPACE) && st->selected >= 0)
        st->files[st->selected].checked = !st->files[st->selected].checked;
    if (rcKeyPressed(RC_KEY_DELETE) && st->selected >= 0)
        st->removeIndex = st->selected;
    if (rcKeyPressed(RC_KEY_UP) || rcKeyPressed(RC_KEY_DOWN)) {
        int step = rcKeyPressed(RC_KEY_UP) ? -1 : 1;
        int to   = 0;    /* a selection the filter hides starts over from the top */
        for (int i = 0; i < st->visibleCount; i++) {
            if (st->visible[i] == st->selected)
                to = i + step;
        }
        if (to >= 0 && to < st->visibleCount)
            select_entry(st, st->visible[to]);
    }
}

static bool view_tab(const char *id, const char *label, bool active) {
    RC_Style s = rcGetStyle();

    rcColumn(.id = id, .focusable = true, .align = "cc", .w = "grow",
             .className = active ? NULL : "hover:bg-surface-alt") {
        rcBox(.align = "cc", .w = "grow", .h = "33px") {
            rcTextC(label, .font = (uint16_t)(active ? rcFontSemibold(13.0f) : 0),
                    .color = active ? s.text : s.textMuted, .wrap = "n",
                    .select = RC_SELECT_NONE);
        }
        rcBox(.bg = active ? pal.selection : RC_UNSET_COLOR, .w = "grow", .h = "2px") {}
    }
    return rcClicked(id);
}

/* Below ICONV_WIDE_W the two working stages share one column behind tabs; below ICONV_NARROW_W the
   list joins them. The same pane functions draw every arm, so a resize carries all state across. */
static void tabbed_stage(RC_App *app, AppState *st, bool withList) {
    RC_Style s = rcGetStyle();

    if (!withList && st->tab == TAB_ICONS)
        st->tab = TAB_PREVIEW;
    rcColumn(.w = "grow", .h = "grow", .className = "min-w-0") {
        rcRow(.bg = s.surface, .w = "grow") {
            if (withList && view_tab("tab_icons", "Icons", st->tab == TAB_ICONS))
                st->tab = TAB_ICONS;
            if (view_tab("tab_preview", "Preview", st->tab == TAB_PREVIEW))
                st->tab = TAB_PREVIEW;
            if (view_tab("tab_export", "Export", st->tab == TAB_EXPORT))
                st->tab = TAB_EXPORT;
        }
        hairline();
        if (st->tab == TAB_ICONS)
            pane_batch(app, st, false);
        else if (st->tab == TAB_PREVIEW)
            pane_canvas(app, st, withList);
        else
            pane_export(app, st, false);
    }
}

/* A mono header this tool writes has the void (float size, RC_Color color) shape RC_IconCallback
   wants, so a converted icon drops straight onto a window control. A glyph draws in the color the
   bar passes in, or it goes invisible against the filled hover slab. A NULL state falls back to
   .normal. */
enum { TBAR_VIEWBOX = 16 };

static void tbar_draw_minimize(RC_BoundingBox b, RC_Color color, const void *userData) {
    (void)userData;
    rcIconDrawRoundLine(b, 4.5f, 8.0f, 11.5f, 8.0f, (float)TBAR_VIEWBOX, 2.05f, color);
}

/* Minimize is a dash on every desktop, so the override shows on hover: the chevron dropping onto
   the rail is the motion the button performs. */
static void tbar_draw_minimize_hover(RC_BoundingBox b, RC_Color color, const void *userData) {
    const float vb = (float)TBAR_VIEWBOX, stroke = 2.05f;

    (void)userData;
    rcIconDrawRoundLine(b, 4.5f, 4.5f, 8.0f, 8.0f, vb, stroke, color);
    rcIconDrawRoundLine(b, 8.0f, 8.0f, 11.5f, 4.5f, vb, stroke, color);
    rcIconDrawRoundLine(b, 4.5f, 11.5f, 11.5f, 11.5f, vb, stroke, color);
}

static void tbar_draw_maximize(RC_BoundingBox b, RC_Color color, const void *userData) {
    (void)userData;
    rcIconDrawRoundedRectStroke(b, 4.0f, 4.0f, 8.0f, 8.0f, 1.6f, (float)TBAR_VIEWBOX, 1.9f, color);
}

static void tbar_minimize(float size, RC_Color color) {
    rcIconEmit(size, color, tbar_draw_minimize, NULL);
}

static void tbar_minimize_hover(float size, RC_Color color) {
    rcIconEmit(size, color, tbar_draw_minimize_hover, NULL);
}

static void tbar_maximize(float size, RC_Color color) {
    rcIconEmit(size, color, tbar_draw_maximize, NULL);
}

static void layout(RC_App *app, void *userData) {
    AppState   *st   = (AppState *)userData;
    RC_Style    s    = rcGetStyle();
    RC_Viewport vp   = rcViewport();
    RC_Insets   safe = vp.safe;
    float       w    = vp.width - safe.left - safe.right;

    st->listDrawn = false;
    st->canvasDrawn = false;
    /* A removal waits for here: the rows of the frame that asked were still reading the list. */
    if (st->removeIndex >= 0) {
        log_event(st, LOG_INFO, "Removed %s", path_leaf(st->files[st->removeIndex].path));
        remove_entry(st, st->removeIndex);
        st->removeIndex = -1;
    }
    handle_drops(st);
    poll_dialogs(st);
    tally_files(st);
    handle_keys(app, st);

    /* A phone draws edge to edge. rcViewport().safe is already in layout units: spend each inset
       once, here at the root, and the bottom one in the status bar so its fill reaches the edge. */
    rcColumn(.id = "root", .bg = s.background, .pt = safe.top, .pl = safe.left,
             .pr = safe.right, .w = "grow", .h = "grow") {
        rcRow(.id = "body", .w = "grow", .h = "grow") {
            if (w >= (float)ICONV_WIDE_W) {
                pane_batch(app, st, true);
                pane_canvas(app, st, false);
                pane_export(app, st, true);
            } else if (w >= (float)ICONV_NARROW_W) {
                pane_batch(app, st, true);
                tabbed_stage(app, st, false);
            } else {
                tabbed_stage(app, st, true);
            }
        }
        status_bar(app, st, safe.bottom);
    }
    activity_modal(st);
    /* The list drew before this frame's click, so ask for the frame that shows what it changed.
       A recenter counts only on frames that draw the canvas, so it asks only on those: while a
       tab or a resize hides the canvas mid-count, the window parks. The first frame asks for
       the one that reads the start folder (update()). */
    st->shown = true;
    if (!st->opened || (st->reveal && st->listDrawn) || st->removeIndex >= 0 ||
        (st->recenter > 0 && st->canvasDrawn))
        rcWindowRequestFrame(rcAppMainWindow(app));

    if (rcIsDragOver(NULL)) {
        RC_Color drop = pal.selection;

        rcColumn(.bg = rcWithAlpha(drop, 0.22f), .align = "cc",
                 .border = { .color = drop, .width = 2 },
                 .w = "100vw", .h = "100vh",
                 .floating = { .to = RC_ATTACH_ROOT, .zIndex = INT16_MAX }) {
          /* A solid card, so the words read over a filled preview too. */
          rcColumn(.bg = rcGetStyle().surface, .gap = 8, .p = 20, .align = "cc",
                   .borderRadius = "all-md", .border = { .color = drop, .width = 1 }) {
            rcIconUpload(48.0f, rcGetStyle().text);
            rcTextL("Drop to add to the batch", .font = rcFontSemibold(16.0f),
                    .color = rcGetStyle().text);
          }
        }
    }
}

int main(void) {
    static AppState state;   /* large fixed arrays: keep it off the stack */
    /* Slot 0 is what buttons, fields and tooltips draw: a dense tool's 13 px. */
    static const float sizes[] = { 13.0f };

    state.selected    = -1;
    state.removeIndex = -1;
    state.tab        = TAB_PREVIEW;
    state.curveSteps = 16.0f;
    state.arcStep    = 7.5f;
    state.precision  = 3.0f;
    state.zoomStep   = ZOOM_FIT;
    /* Relative paths that open from the repository root and from the program's own folder, where
       a desktop build copies examples/assets. */
    snprintf(state.folder, sizeof state.folder, "%s", "examples/assets/icons");
    snprintf(state.outDir, sizeof state.outDir, "%s", ICONV_OUT_DIR);

    rcSetStyle(app_style());
    pal = app_palette();

    RC_AppOptions opts = {
        .width             = 1280,
        .height            = 820,
        .title             = "RayClay Icon Converter",
        .fontSizes         = sizes,
        .fontCount         = 1,
        .scratchArenaBytes = 64 * 1024,
        .frame             = RC_WINDOW_FRAME_TITLEBAR,
        .updateCallback    = update,
        .layoutCallback    = layout,
        .userData          = &state,
        /* Per-button glyph overrides. Close keeps the bundled glyph, because the control a user
           reaches for when an app misbehaves should stay conventional. */
        .titlebar = {
            .minimize = { .normal = tbar_minimize, .hover = tbar_minimize_hover },
            .maximize = { .normal = tbar_maximize },
        },
    };

    return rcRunApp(&opts);
}
