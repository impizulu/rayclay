/*
    RayClay Icon Converter - scan a folder of *.svg, preview each icon, and
    export it as a header-only RayClay icon you can call like any other.

    Shows: rcIconEmit custom elements, rcTextInput, rcButton, a scrollable
    selectable list with rcScrollbar, rcClipboardSet, and the bundled titlebar
    with per-button glyph overrides. One layout becomes three columns or three
    tabs on available width alone.

    Run: rayclay_ex11_rayclay_icon_converter, then point "Input directory" at a
    folder of SVGs and press Scan.

    Reads and writes local files, so it is built for a desktop.
*/

#include "rayclay.h"

/* The preview half of rc_svg2icon.h is opt-in: defining this links the icon
   runtime it draws through. An offline generator leaves it undefined. */
#define RC_SVG2ICON_PREVIEW
#include "rc_svg2icon.h"

#include <stdio.h>
#include <stdarg.h>   /* va_list: log_fmt      */
#include <string.h>
#include <stdlib.h>   /* qsort                 */
#include <ctype.h>    /* tolower: name_cmp + stem */
#ifdef _WIN32
    #include <windows.h>  /* FindFirstFileA directory scan */
    #include <direct.h>   /* _mkdir */
#else
    #include <dirent.h>   /* opendir/readdir directory scan (no dirent on MSVC) */
    #include <sys/stat.h> /* mkdir  */
#endif

/* Font ladder baked from the bundled face - no asset files. */
typedef enum { F_SMALL = 0, F_BODY, F_TITLE, F_COUNT } AppFont;

/* The tool's own palette. Graphite chrome, so the only saturated thing in the
   window is the artwork on the preview tiles. Three accents carry meaning and
   nothing else uses them: cyan is SELECTED, amber is a parser warning, red is a
   step that did not happen. */
static RC_Style iconv_style(void) {
    RC_Style s = rcStyleDark();
    s.background   = RC_ZINC_950;
    s.surface      = RC_ZINC_900;
    s.surfaceAlt   = RC_ZINC_800;
    /* The window bar takes the BACKGROUND rather than the panel surface, so the
       tool's own header band is the one raised strip at the top of the window. */
    s.chrome       = RC_ZINC_950;
    s.text         = RC_ZINC_100;
    s.textMuted    = RC_ZINC_400;
    s.border       = RC_ZINC_700;
    s.primary      = RC_CYAN_600;
    s.primaryHover = RC_CYAN_500;
    /* The 400 tiers: these three read as TEXT on a graphite panel, not as a
       button fill, where the preset's 600 tiers are too dark at 13 px. */
    s.success      = RC_EMERALD_400;
    s.warning      = RC_AMBER_400;
    s.danger       = RC_RED_400;
    return s;
}

/* The two layout arms. The app branches on AVAILABLE WIDTH (rcViewport), never
   on the OS, so a desktop window dragged narrow lays out exactly as a small
   screen does. The threshold is the narrowest window that still shows all three
   stages, summed from the panes themselves; below it the three panes become
   three tabs and one stage has the window to itself. */
#define ICONV_SOURCES_W    280   /* sources column, three-pane arm            */
#define ICONV_EXPORT_W     320   /* export column, three-pane arm             */
#define ICONV_PANE_PAD      18   /* preview pane padding, each side           */
#define ICONV_TILE_GAP      12   /* between the light tile and the dark one   */
#define ICONV_TILE_MIN     120   /* tile floor: a 24-unit glyph still reads   */
/* What a tile costs besides its square: the 1 px frame each side, the 6 px gap
   under it, and the caption. The height budget below subtracts exactly this. */
#define ICONV_TILE_CHROME  (2 + 6 + 13)
#define ICONV_THREE_PANE_W (ICONV_SOURCES_W + ICONV_EXPORT_W + 2 * ICONV_PANE_PAD + \
                            2 * ICONV_TILE_MIN + ICONV_TILE_GAP)
/* The file list's row height, gap and padding. These MUST track the .h, .gap
   and .p spelled on the list itself: the list is capped to a whole number of
   pitches so its bottom row is never sliced through the x-height, and a drift
   here would quantise to the wrong grid. */
#define ICONV_ROW_H         30
#define ICONV_ROW_GAP        2
#define ICONV_LIST_PAD       6
#define ICONV_ROW_PITCH    (ICONV_ROW_H + ICONV_ROW_GAP)

/* The tabbed arm's three stages, in pipeline order. */
typedef enum { TAB_SOURCES = 0, TAB_PREVIEW, TAB_EXPORT } IconvTab;

/* Status-log severity, which is what gives the log its colour. */
typedef enum { LOG_INFO = 0, LOG_DONE, LOG_FAIL } IconvLogKind;

/* Bounded, heap-free state: every collection is a fixed array, so the tool runs
   with zero heap allocation. rcFormat uses the app's frame arena. */

#define ICONV_MAX_FILES  256   /* scanned *.svg entries                        */
#define ICONV_NAME_CAP   128   /* per-entry file name (stem + ".svg")          */
#define ICONV_DIR_CAP    512   /* input / output directory buffers             */
#define ICONV_SVG_CAP    (64 * 1024)  /* bounded SVG read buffer               */
#define ICONV_EMIT_CAP   (64 * 1024)  /* bounded header emit buffer            */
#define ICONV_LOG_LINES  64    /* status-log ring                              */
#define ICONV_LOG_CAP    160   /* per-log-line text                            */
#define ICONV_SYMBOL_CAP  96   /* "rcIcon" + the PascalCased stem               */

typedef struct {
    char files[ICONV_MAX_FILES][ICONV_NAME_CAP]; /* names as scanned           */
    char symbol[ICONV_MAX_FILES][ICONV_SYMBOL_CAP]; /* rcIcon<Pascal> per file */
    char rowId[ICONV_MAX_FILES][8];              /* stable "f0".."f255" ids    */
    int  fileCount;
    int  selected;               /* index into files[], or -1                  */

    char inDir[ICONV_DIR_CAP];   /* source directory (rcTextInput buffer)     */
    char outDir[ICONV_DIR_CAP];  /* export directory (rcTextInput buffer)     */

    RcSvgIcon icon;              /* the currently-parsed selection             */
    bool      haveIcon;          /* icon holds a valid parse                   */
    IconvTab  tab;               /* tabbed arm: which stage is on screen       */

    char log[ICONV_LOG_LINES][ICONV_LOG_CAP]; /* status-log ring               */
    IconvLogKind logKind[ICONV_LOG_LINES];    /* severity of that line         */
    int  logCount;               /* number of live lines (caps at ring size)   */
    int  logHead;                /* index of the OLDEST live line              */

    bool pendingScan;            /* scan on the first frame                    */
} AppState;

/* Status log: a fixed ring, oldest overwritten, no allocation. Claim the next
   slot and tag it; the severity is STORED by the site that reports, never
   derived from the message text - otherwise every rewording is a colour change. */
static char *log_slot(AppState *st, IconvLogKind kind) {
    int idx;
    if (st->logCount < ICONV_LOG_LINES) {
        idx = (st->logHead + st->logCount) % ICONV_LOG_LINES;
        st->logCount++;
    } else {
        idx = st->logHead;
        st->logHead = (st->logHead + 1) % ICONV_LOG_LINES;
    }
    st->logKind[idx] = kind;
    return st->log[idx];
}

/* The ordinary report. vsnprintf never overflows the fixed slot and always
   NUL-terminates. RC_PRINTF_FMT keeps -Wformat checking every call site's
   literal; it sits among the declaration specifiers because a trailing
   attribute on a function DEFINITION is rejected. */
static void RC_PRINTF_FMT(2, 3) log_fmt(AppState *st, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(log_slot(st, LOG_INFO), ICONV_LOG_CAP, fmt, ap);
    va_end(ap);
}

/* The same ring, for a step that produced something the user asked for. */
static void RC_PRINTF_FMT(2, 3) log_done(AppState *st, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(log_slot(st, LOG_DONE), ICONV_LOG_CAP, fmt, ap);
    va_end(ap);
}

/* The same ring, for a step that did not happen. */
static void RC_PRINTF_FMT(2, 3) log_fail(AppState *st, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(log_slot(st, LOG_FAIL), ICONV_LOG_CAP, fmt, ap);
    va_end(ap);
}

/* Filesystem helpers. All defensive: a NULL or failed handle returns. */

/* Case-insensitive ".svg" suffix test. */
static bool ends_with_svg(const char *name) {
    size_t n = strlen(name);
    if (n < 4) return false;
    const char *e = name + (n - 4);
    return (e[0] == '.') &&
           (e[1] == 's' || e[1] == 'S') &&
           (e[2] == 'v' || e[2] == 'V') &&
           (e[3] == 'g' || e[3] == 'G');
}

/* Lowercase stem of a scanned name: it names the output file (<stem>.h) and
   seeds the icon name, which rc_svg2icon_emit PascalCases.
   THE TRAP: strip the EXTENSION, not the first dot. "chevron.right.svg" cut at
   the first '.' writes chevron.h over an unrelated icon, silently - the writer
   opens with "wb". A dot left in the stem is harmless. */
static void file_stem(const char *name, char *stem, int stemCap) {
    size_t nameLen = strlen(name);
    size_t stemLen = ends_with_svg(name) ? nameLen - 4 : nameLen;
    int    si      = 0;

    for (size_t i = 0; i < stemLen; i++) {
        if (si < stemCap - 1) stem[si++] = (char)tolower((unsigned char)name[i]);
    }
    stem[si] = '\0';
    if (si == 0) snprintf(stem, (size_t)stemCap, "icon");
}

/* Ordering for the scanned list: case-insensitive, with a bytewise tie-break so
   case-equal names sort deterministically (qsort itself is unstable). */
static int name_cmp(const void *a, const void *b) {
    const char *x = (const char *)a, *y = (const char *)b;
    for (; *x && *y; x++, y++) {
        int cx = tolower((unsigned char)*x), cy = tolower((unsigned char)*y);
        if (cx != cy) return cx - cy;
    }
    if (*x || *y) return (unsigned char)*x - (unsigned char)*y;
    return strcmp((const char *)a, (const char *)b);
}

/* Scan st->inDir for *.svg into st->files[], sorted, cap ICONV_MAX_FILES.
   Clears any prior scan + selection first. Logs the outcome. */
static void scan_dir(AppState *st) {
    st->fileCount = 0;
    st->selected  = -1;
    st->haveIcon  = false;

    const char *dir = st->inDir[0] ? st->inDir : ".";
#ifdef _WIN32
    char pat[ICONV_DIR_CAP + 8];
    snprintf(pat, sizeof pat, "%s\\*.svg", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        /* No *.svg match is a normal empty scan, not an open failure. */
        if (GetLastError() != ERROR_FILE_NOT_FOUND) {
            log_fail(st, "scan: cannot open directory");
            return;
        }
    } else {
        do {
            if (st->fileCount >= ICONV_MAX_FILES) break;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            if (!ends_with_svg(fd.cFileName)) continue;
            snprintf(st->files[st->fileCount], ICONV_NAME_CAP, "%.*s",
                     ICONV_NAME_CAP - 1, fd.cFileName);
            st->fileCount++;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    DIR *d = opendir(dir);
    if (!d) {
        log_fail(st, "scan: cannot open directory");
        return;
    }
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && st->fileCount < ICONV_MAX_FILES) {
        if (!ends_with_svg(ent->d_name)) continue;
        snprintf(st->files[st->fileCount], ICONV_NAME_CAP, "%.*s",
                 ICONV_NAME_CAP - 1, ent->d_name);
        st->fileCount++;
    }
    closedir(d);
#endif

    if (st->fileCount > 1) {
        qsort(st->files, (size_t)st->fileCount, ICONV_NAME_CAP, name_cmp);
    }

    /* The name the generated header will export, derived once here rather than
       per frame: it is a pure string transform, so it needs no parse. */
    for (int i = 0; i < st->fileCount; i++) {
        char stem[ICONV_NAME_CAP];
        file_stem(st->files[i], stem, (int)sizeof stem);
        rc_svg2icon_symbol(stem, st->symbol[i], ICONV_SYMBOL_CAP);
    }

    log_fmt(st, "scanned %.110s: %d svg file(s)", dir, st->fileCount);
}

/* Join dir + '/' + leaf into out[cap] (skips a redundant separator). */
static void path_join(char *out, int cap, const char *dir, const char *leaf) {
    if (dir && dir[0]) {
        size_t n = strlen(dir);
        char sep = (dir[n - 1] == '/' || dir[n - 1] == '\\') ? '\0' : '/';
        if (sep) snprintf(out, cap, "%s/%s", dir, leaf);
        else     snprintf(out, cap, "%s%s", dir, leaf);
    } else {
        snprintf(out, cap, "%s", leaf);
    }
}

/* mkdir the export dir (idempotent; ignores "already exists"). */
static void ensure_dir(const char *dir) {
    if (!dir || !dir[0]) return;
#ifdef _WIN32
    _mkdir(dir);
#else
    mkdir(dir, 0755);
#endif
}

/* Read a whole file into buf[cap] (NUL-terminated). Returns bytes read, or -1
   on failure. A file larger than the buffer fills it to cap-1 and sets
   *truncated; pass NULL if you genuinely do not care.
   THE TRAP: `got == cap - 1` is not the truncation test - a file of exactly
   cap-1 bytes fills the buffer and is complete. Ask the file if anything is
   left instead. A half-read SVG parses into a plausible, wrong icon. */
static int read_file(const char *path, char *buf, int cap, bool *truncated) {
    FILE  *f = fopen(path, "rb");
    size_t got;

    if (truncated) *truncated = false;
    if (!f) return -1;
    got = fread(buf, 1, (size_t)(cap - 1), f);
    if (truncated && got == (size_t)(cap - 1) && fgetc(f) != EOF)
        *truncated = true;
    fclose(f);
    buf[got] = '\0';
    return (int)got;
}

/* Selection and export actions: these run on user action, never per frame. */

/* Read + parse files[idx] into *out, which is what keeps the tool to ONE
   ICONV_SVG_CAP buffer. Returns false, having logged why, on failure. */
static bool load_svg_icon(AppState *st, int idx, RcSvgIcon *out) {
    char path[ICONV_DIR_CAP + ICONV_NAME_CAP];
    path_join(path, (int)sizeof path, st->inDir[0] ? st->inDir : ".",
              st->files[idx]);

    static char svg[ICONV_SVG_CAP];   /* one static buffer (shared across calls): bounded, off the stack */
    bool truncated;
    int  len = read_file(path, svg, ICONV_SVG_CAP, &truncated);
    if (len < 0) {
        log_fail(st, "open failed: %s", st->files[idx]);
        return false;
    }
    if (truncated) {
        log_fail(st, "larger than the %d byte buffer, not read whole: %.60s",
                 ICONV_SVG_CAP - 1, st->files[idx]);
        return false;
    }
    if (!rc_svg2icon_parse(svg, len, 16, 7.5f, out) || !out->ok) {
        log_fail(st, "parse failed: %.80s (%.60s)", out->err, st->files[idx]);
        return false;
    }
    return true;
}

/* Load + parse files[idx] into st->icon, updating the preview. Logs failures. */
static void select_file(AppState *st, int idx) {
    if (idx < 0 || idx >= st->fileCount) return;
    st->selected = idx;
    st->haveIcon = load_svg_icon(st, idx, &st->icon);
}

/* The emitted header text. One file-scope buffer: a header is either being
   written to a file or handed to the clipboard, never both. */
static char iconv_emit[ICONV_EMIT_CAP];

/* Parse files[idx] and emit its header into iconv_emit, filling stem[] with the
   output name. Returns the emitted length, or -1 having logged why. Copy and
   Export both route through here, so the clipboard text and the written file
   are byte-identical. */
static int emit_header(AppState *st, int idx, char *stem, int stemCap) {
    if (idx < 0 || idx >= st->fileCount) return -1;

    static RcSvgIcon tmp;   /* keep the big struct off the stack */
    if (!load_svg_icon(st, idx, &tmp)) return -1;

    file_stem(st->files[idx], stem, stemCap);

    int written = rc_svg2icon_emit(&tmp, stem, st->files[idx], 3,
                                   iconv_emit, ICONV_EMIT_CAP);
    if (written < 0) {
        log_fail(st, "emit overflow: %s", st->files[idx]);
        return -1;
    }
    return written;
}

/* Emit files[idx] to <outDir>/<stem>.h. Logs the outcome. */
static void export_one(AppState *st, int idx) {
    char stem[ICONV_NAME_CAP];
    int  written = emit_header(st, idx, stem, (int)sizeof stem);
    if (written < 0) return;

    ensure_dir(st->outDir[0] ? st->outDir : "out");

    char outLeaf[ICONV_NAME_CAP];
    /* Cap the stem so the ".h" suffix always fits (gcc format-truncation). */
    snprintf(outLeaf, sizeof outLeaf, "%.*s.h", (int)sizeof outLeaf - 3, stem);
    char outPath[ICONV_DIR_CAP + ICONV_NAME_CAP];
    path_join(outPath, (int)sizeof outPath, st->outDir[0] ? st->outDir : "out",
              outLeaf);

    FILE *of = fopen(outPath, "wb");
    if (!of) {
        log_fail(st, "write failed: %s.h", stem);
        return;
    }
    size_t wrote = fwrite(iconv_emit, 1, (size_t)written, of);
    fclose(of);

    if (wrote == (size_t)written) log_done(st, "generated %s.h", stem);
    else                          log_fail(st, "short write: %s.h", stem);
}

/* Put the selected file's generated header on the system clipboard. It emits
   through the same route Export does, so the two agree byte for byte. */
static void copy_selected(AppState *st) {
    char stem[ICONV_NAME_CAP];
    if (st->selected < 0) {
        log_fail(st, "copy: no file selected");
        return;
    }
    int written = emit_header(st, st->selected, stem, (int)sizeof stem);
    if (written < 0) return;

    rcClipboardSet(iconv_emit);
    log_done(st, "copied %s.h to the clipboard (%d bytes)", stem, written);
}

/* Export every scanned file. Each failure logs + continues (never aborts). */
static void export_all(AppState *st) {
    if (st->fileCount == 0) {
        log_fail(st, "export all: nothing scanned");
        return;
    }
    for (int i = 0; i < st->fileCount; i++) export_one(st, i);
    log_done(st, "export all: %d file(s) written to %s", st->fileCount,
             st->outDir[0] ? st->outDir : "out");
}

/* Live preview custom element. rcIconEmit emits a size x size CUSTOM element
   whose callback draws the parsed ops exactly as a generated header would.
   THE CHECKER AND THE ICON ARE ONE ELEMENT, drawn in the one box rcIconEmit
   hands the callback, so nothing can put them out of register. The viewBox size
   IS the cell count, so one cell is one unit at every tile size and zoom. */
enum { ICONV_CHECKER_CELLS = 8 };

/* One axis-aligned quad in viewBox units, written into q[4]. */
static void checker_quad(RC_IconPoint *q, float x, float y, float w, float h) {
    q[0].x = x;      q[0].y = y;
    q[1].x = x + w;  q[1].y = y;
    q[2].x = x + w;  q[2].y = y + h;
    q[3].x = x;      q[3].y = y + h;
}

/* The pale tone goes down once as a single quad and the deep cells over it, so
   two neighbours of the same tone never share an edge - abutting fills show a
   seam wherever the rasteriser rounds the two sides differently. */
static void preview_tile_draw(RC_BoundingBox b, RC_Color stroke,
                              const RcSvgIcon *icon, RC_Color pale, RC_Color deep) {
    const float  vb = (float)ICONV_CHECKER_CELLS;
    RC_IconPoint quad[4];

    checker_quad(quad, 0.0f, 0.0f, vb, vb);
    rcIconDrawFilledPolygon(b, quad, 4, vb, pale);
    for (int cy = 0; cy < ICONV_CHECKER_CELLS; cy++) {
        for (int cx = (cy & 1); cx < ICONV_CHECKER_CELLS; cx += 2) {
            checker_quad(quad, (float)cx, (float)cy, 1.0f, 1.0f);
            rcIconDrawFilledPolygon(b, quad, 4, vb, deep);
        }
    }

    /* A one-cell margin inside the tile, so a glyph that runs to the edge of its
       own viewBox still has checker visible on every side of it. */
    RC_BoundingBox inner = b;
    float m = b.width / vb;
    inner.x += m;  inner.width  -= 2.0f * m;
    inner.y += m;  inner.height -= 2.0f * m;
    rc_svg2icon_draw(icon, inner, stroke);
}

/* The two grounds an icon has to survive: only the tone pair differs. */
static void preview_cb_light(RC_BoundingBox b, RC_Color c, const void *ud) {
    preview_tile_draw(b, c, (const RcSvgIcon *)ud, RC_ZINC_50, RC_ZINC_300);
}

static void preview_cb_dark(RC_BoundingBox b, RC_Color c, const void *ud) {
    preview_tile_draw(b, c, (const RcSvgIcon *)ud, RC_ZINC_800, RC_ZINC_700);
}

/* No checkerboard: at 16 px a checker is louder than the glyph on it. */
static void preview_plain(RC_BoundingBox b, RC_Color c, const void *ud) {
    rc_svg2icon_draw((const RcSvgIcon *)ud, b, c);
}

/* THE SIZE LADDER, and it is the question this tool exists to answer: a glyph
   that reads at 200 px can close up into a blob at 16.
   BOTTOM-ALIGNED, so the four captions share one baseline. */
static void preview_ladder(const RcSvgIcon *icon) {
    RC_Style s = rcGetStyle();
    static const float      PX[]    = { 16.0f, 24.0f, 32.0f, 48.0f };
    static const char *const IDS[]  = { "lad_16", "lad_24", "lad_32", "lad_48" };
    static const char *const LBL[]  = { "16", "24", "32", "48" };
    int i;

    rcRow(.id = "prev_ladder", .gap = 14, .align = "bc") {
        for (i = 0; i < 4; i++) {
            rcColumn(.id = IDS[i], .gap = 6, .align = "cc") {
                rcBox(.bg = s.surface, .p = 10, .align = "cc",
                      .borderRadius = "all-md",
                      .border = { .color = s.border, .width = "1px" }) {
                    rcIconEmit(PX[i], s.text, preview_plain, icon);
                }
                rcTextC(LBL[i], .font = F_SMALL, .color = s.textMuted);
            }
        }
    }
}

/* A muted count beside a heading, the shape a file tree uses to say how much is
   in it without a second line. */
static void heading_count(RC_App *app, const char *label, int count) {
    RC_Style s = rcGetStyle();
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        rcTextC(label, .font = F_TITLE, .color = s.text, .wrap = "n");
        if (count > 0) {
            rcBox(.bg = s.surfaceAlt, .px = 8, .py = 2, .align = "cc",
                  .borderRadius = "all-full") {
                rcText(rcFormat(rcAppArena(app), "%d", count),
                        .font = F_SMALL, .color = s.textMuted, .wrap = "n");
            }
        }
    }
}

static void panel_sources(RC_App *app, AppState *st, bool compact) {
    RC_Style s = rcGetStyle();

    /* WHOLE ROWS ONLY: the list's own measured height rounded down to a whole
       number of pitches, so the bottom row is never sliced through its
       x-height. It is LAST FRAME'S box, the immediate-mode norm, and `found` is
       false on the very first frame - where an unset .hMax means no ceiling. */
    RC_Box listBox = rcGetElementBox("list_files");
    float  listMax = 0.0f;
    if (listBox.found) {
        int rows = (int)((listBox.height - 2.0f * ICONV_LIST_PAD + ICONV_ROW_GAP)
                         / (float)ICONV_ROW_PITCH);
        if (rows > 0) {
            listMax = (float)(rows * ICONV_ROW_PITCH - ICONV_ROW_GAP
                              + 2 * ICONV_LIST_PAD);
        }
    }

    /* On the tabbed arm the stage IS the window, so a drawn border lands as a
       hairline pinned along the window edge. The width stays (a fixed char array
       takes a literal only) and the colour goes. */
    rcColumn(.id = "col_src", .bg = s.surface, .gap = 10, .p = 14,
             .border = { .color = compact ? RC_TRANSPARENT : s.border,
                         .width = "1px" }, .h = "grow",
             .wType = compact ? RC_GROW : RC_PX(ICONV_SOURCES_W)) {
        /* The tab strip already names the stage on the tabbed arm. */
        if (!compact) heading_count(app, "Sources", st->fileCount);
        rcTextL("Input directory", .font = F_SMALL, .color = s.textMuted);
        rcBox(.w = "grow") {
            rcTextInput("in_dir", st->inDir, sizeof st->inDir,
                         .placeholder = "./icons");
        }
        if (rcButton("btn_scan", "Scan", RC_BTN_PRIMARY)) scan_dir(st);

        /* Only emit the hint when empty; a "" text node is a dead gap otherwise. */
        if (!st->fileCount) {
            rcTextL("(no *.svg found - Scan a directory)",
                     .font = F_SMALL, .color = s.textMuted);
        }

        rcColumn(.id = "list_files", .bg = s.surfaceAlt, .gap = ICONV_ROW_GAP,
                 .p = ICONV_LIST_PAD,
                 .scroll = "v", .borderRadius = "all-md", .w = "grow", .h = "grow",
                 .hMax = listMax) {
            for (int i = 0; i < st->fileCount; i++) {
                bool sel = (i == st->selected);
                /* SELECTION IS A BACKGROUND PLUS A MARK: the tint and the cyan
                   ink are both COLOUR, one channel wearing two hats. The 3 px
                   bar is the second channel and holds its gutter in both states,
                   so the list does not shift when the choice moves. */
                rcRow(.id = st->rowId[i],
                      .bg = sel ? rcAlpha(s.primary, 70)
                                : (rcIsHovered(st->rowId[i]) ? s.surface
                                                             : RC_TRANSPARENT),
                      .gap = 7, .pl = 5, .pr = 10, .align = "cl",
                      .borderRadius = "all-sm", .w = "grow",
                      .hType = RC_PX(ICONV_ROW_H)) {
                    rcBox(.bg = sel ? s.primary : RC_TRANSPARENT,
                          .borderRadius = "all-full", .w = "3px", .h = "18px") {}
                    rcTextC(st->files[i], .font = F_BODY,
                             .color = sel ? RC_CYAN_300 : s.text, .wrap = "n");
                    /* The full-width arm has room for the name the export will
                       give this icon; the 280 px column does not. */
                    if (compact) {
                        rcSeparator() {}
                        rcTextC(st->symbol[i], .font = F_SMALL,
                                 .color = s.textMuted, .wrap = "n");
                    }
                }
                if (rcClicked(st->rowId[i])) {
                    select_file(st, i);
                    /* One stage at a time on the tabbed arm: picking a source
                       means "show me it". */
                    if (compact) st->tab = TAB_PREVIEW;
                }
            }
        }
        /* The scrollbar for "list_files" is declared once in layout(). */
    }
}

/* Centre column: live preview. TWO TILES, ONE PER GROUND - an icon ships into a
   light interface and a dark one, and a stroke that disappears on either is a
   defect this window has to SHOW rather than hide behind a control. */

static void preview_tile(const RcSvgIcon *icon, const char *id,
                         const char *label, float tile, bool dark) {
    RC_Style s = rcGetStyle();
    rcColumn(.id = id, .gap = 6, .align = "tc") {
        /* No radius on the frame: the checker fills the element's own square
           bounds, so a rounded frame would curve away from corners it fills. */
        rcBox(.border = { .color = s.border, .width = "1px" }) {
            rcIconEmit(tile, dark ? RC_ZINC_50 : RC_ZINC_900,
                       dark ? preview_cb_dark : preview_cb_light, icon);
        }
        rcTextC(label, .font = F_SMALL, .color = s.textMuted);
    }
}

/* What the parser made of the selection. A warning is AMBER and never red: the
   parse produced a usable icon and said what it could not honour. */
static void preview_details(RC_App *app, AppState *st) {
    RC_Style s = rcGetStyle();
    if (!st->haveIcon) {
        rcTextL("No parse loaded. Pick a file in Sources.",
                 .font = F_SMALL, .color = s.textMuted);
        return;
    }
    const RcSvgIcon *ic = &st->icon;
    RC_Arena *ar = rcAppArena(app);

    RC_String name = rcFormat(ar, "file: %s",
                                 st->selected >= 0 ? st->files[st->selected] : "?");
    RC_String view = rcFormat(ar, "viewBox: %.0f x %.0f", ic->viewW, ic->viewH);
    RC_String info = rcFormat(ar, "ops: %d  \xc2\xb7  %s  \xc2\xb7  %d point(s)",
                                 ic->opCount, ic->colored ? "colored" : "mono",
                                 ic->pointCount);
    rcText(name, .font = F_SMALL, .color = s.text);
    rcText(view, .font = F_SMALL, .color = s.textMuted);
    rcText(info, .font = F_SMALL, .color = s.textMuted);
    /* A clean parse SAYS SO: silence where warnings would be is
       indistinguishable from a panel that forgot to draw them. */
    if (ic->warnCount == 0) {
        rcTextL("no warnings", .font = F_SMALL, .color = s.success);
    }
    for (int i = 0; i < ic->warnCount && i < RC_SVG_MAX_WARN; i++) {
        RC_String w = rcFormat(ar, "warn: %s", ic->warn[i] ? ic->warn[i] : "?");
        rcText(w, .font = F_SMALL, .color = s.warning);
    }
}

static void panel_preview(RC_App *app, AppState *st, float tile, bool compact) {
    RC_Style s = rcGetStyle();
    /* NULL while nothing is parsed: rc_svg2icon_draw takes a NULL icon and draws
       nothing, so a failed selection cannot leave the previous one on the tiles. */
    const RcSvgIcon *icon = st->haveIcon ? &st->icon : NULL;

    /* The top padding matches the side columns' rather than the pane's, so the
       three stage headings share one baseline across the three-pane arm. */
    rcColumn(.id = "col_center", .bg = s.background, .gap = 12, .p = ICONV_PANE_PAD,
             .pt = 14, .align = "tc", .w = "grow", .h = "grow") {
        /* Its own full-width row, so the heading does not inherit the column's
           centring and the three stage headings share one left edge. */
        if (!compact) {
            rcRow(.align = "cl", .w = "grow") {
                rcTextL("Preview", .font = F_TITLE, .color = s.text);
            }
        }
        /* THE SLACK BELONGS TO THE TILES, not to the details panel: a panel
           given .h = "grow" to hold four lines becomes the largest object in the
           window and competes with the artwork it describes. */
        rcColumn(.id = "prev_stage", .align = "cc", .w = "grow", .h = "grow") {
            rcColumn(.gap = 22, .align = "cc") {
                rcRow(.gap = ICONV_TILE_GAP, .align = "tc") {
                    preview_tile(icon, "tile_light", "light", tile, false);
                    preview_tile(icon, "tile_dark",  "dark",  tile, true);
                }
                preview_ladder(icon);
            }
        }
        rcColumn(.id = "prev_meta", .bg = s.surface, .gap = 4, .p = 12,
                 .borderRadius = "all-lg",
                 .border = { .color = s.border, .width = "1px" }, .w = "grow") {
            preview_details(app, st);
        }
    }
}

/* One label / value line of the export summary. rcSeparator is the library's
   stretchy spacer, so the value sits hard against the card's right edge. */
static void export_stat(const char *label, RC_String value, RC_Color color) {
    RC_Style s = rcGetStyle();
    rcRow(.gap = 10, .align = "cl", .w = "grow") {
        rcTextC(label, .font = F_SMALL, .color = s.textMuted, .wrap = "n");
        rcSeparator() {}
        rcText(value, .font = F_SMALL, .color = color, .wrap = "n");
    }
}

/* WHAT THE BUTTONS ABOVE WILL DO, before either is pressed: the file that gets
   written, the symbol the reader then calls, and the size of the batch. Every
   value is state the app already holds and the symbol comes from the converter's
   own transform, so nothing here can disagree with what Export produces. */
static void export_summary(RC_App *app, AppState *st) {
    RC_Style  s  = rcGetStyle();
    RC_Arena *ar = rcAppArena(app);
    const char *dir = st->outDir[0] ? st->outDir : "out";

    rcColumn(.id = "exp_summary", .bg = s.surfaceAlt, .gap = 7, .p = 12,
             .borderRadius = "all-md", .w = "grow") {
        if (st->selected >= 0) {
            char stem[ICONV_NAME_CAP], symbol[160];
            file_stem(st->files[st->selected], stem, (int)sizeof stem);
            rc_svg2icon_symbol(stem, symbol, (int)sizeof symbol);
            export_stat("Writes", rcFormat(ar, "%s/%s.h", dir, stem), s.text);
            export_stat("Call it as", rcFormat(ar, "%s()", symbol), RC_CYAN_300);
        } else {
            export_stat("Writes", rcStringFromCStr("pick a source first"),
                        s.textMuted);
            export_stat("Call it as", rcStringFromCStr("-"), s.textMuted);
        }
        rcBox(.bg = s.border, .w = "grow", .h = "1px") {}
        export_stat("Selection", st->selected >= 0
                        ? rcFormat(ar, "%d of %d", st->selected + 1, st->fileCount)
                        : rcFormat(ar, "none of %d", st->fileCount), s.text);
        export_stat("Export all writes",
                    rcFormat(ar, "%d header(s)", st->fileCount), s.text);
    }
}

static void panel_export(RC_App *app, AppState *st, bool compact) {
    RC_Style s = rcGetStyle();
    rcColumn(.id = "col_exp", .bg = s.surface, .gap = 10, .p = 14,
             .border = { .color = compact ? RC_TRANSPARENT : s.border,
                         .width = "1px" }, .h = "grow",
             .wType = compact ? RC_GROW : RC_PX(ICONV_EXPORT_W)) {
        if (!compact) rcTextL("Export", .font = F_TITLE, .color = s.text);
        rcTextL("Output directory", .font = F_SMALL, .color = s.textMuted);
        rcBox(.w = "grow") {
            rcTextInput("out_dir", st->outDir, sizeof st->outDir,
                         .placeholder = "out");
        }
        rcRow(.gap = 8, .w = "grow") {
            if (rcButton("btn_exp_sel", "Export selected", RC_BTN_PRIMARY)) {
                if (st->selected >= 0) export_one(st, st->selected);
                else                   log_fail(st, "export: no file selected");
            }
            if (rcButton("btn_exp_all", "Export all", RC_BTN_DEFAULT)) {
                export_all(st);
            }
        }
        export_summary(app, st);

        rcRow(.gap = 10, .align = "cl", .w = "grow") {
            rcTextL("Status log", .font = F_SMALL, .color = s.textMuted);
            rcSeparator() {}
            if (st->logCount > 0 && rcButton("btn_log_clear", "Clear", RC_BTN_GHOST)) {
                st->logCount = 0;
                st->logHead  = 0;
            }
        }
        /* An inset console rather than a raised card: this region is usually
           empty, and a filled slab that size would be the loudest shape here. */
        rcColumn(.id = "list_log", .bg = s.background, .gap = 3, .p = 10,
                 .scroll = "v", .borderRadius = "all-md",
                 .border = { .color = s.border, .width = "1px" },
                 .w = "grow", .h = "grow") {
            if (st->logCount == 0) {
                rcTextL("(no activity yet)", .font = F_SMALL, .color = s.textMuted);
            }
            for (int i = 0; i < st->logCount; i++) {
                int idx = (st->logHead + i) % ICONV_LOG_LINES;
                /* SEVERITY IS COLOUR, and it is the only place this window uses
                   green or red. rcTextC BORROWS the pointer: the ring lives in
                   the app's own static state, so it outlives the frame with no
                   arena copy. */
                rcTextC(st->log[idx], .font = F_SMALL,
                        .color = st->logKind[idx] == LOG_FAIL ? s.danger
                               : st->logKind[idx] == LOG_DONE ? s.success
                                                              : s.textMuted,
                        .wrap = "n");
            }
        }
        /* Scrollbar overlay for "list_log" is drawn once in layout(). */
    }
}

/* Header band, above both arms: the one fact every stage has to agree on -
   which file is loaded - beside the one action that needs no directory. */
static void panel_header(AppState *st) {
    RC_Style s = rcGetStyle();
    rcRow(.id = "hdr", .bg = s.surface, .gap = 12, .px = 14, .py = 10,
          .align = "cl", .w = "grow") {
        /* The name may be long and the button must not be pushed off the edge
           by it, so the name's box grows and CLIPS. */
        rcBox(.align = "cl", .overflow = "hidden", .w = "grow") {
            if (st->selected >= 0) {
                rcTextC(st->files[st->selected], .font = F_BODY,
                         .color = s.text, .wrap = "n");
            } else {
                rcTextL("no file loaded", .font = F_BODY,
                         .color = s.textMuted, .wrap = "n");
            }
        }
        if (rcButton("btn_copy", "Copy header", RC_BTN_DEFAULT)) copy_selected(st);
    }
}

/* Tabbed arm: the stage picker, one segment per stage in pipeline order. */
static bool iconv_tab(const char *id, const char *label, bool active) {
    RC_Style s = rcGetStyle();
    /* THE CURRENT TAB IS A FILL PLUS AN UNDERLINE: a fill is colour, and a tab
       that says which one you are on with colour alone says it once. */
    rcColumn(.id = id, .gap = 0, .w = "grow") {
        rcRow(.bg = active ? rcAlpha(s.primary, 70)
                           : (rcIsHovered(id) ? s.surfaceAlt : s.surface),
              .align = "cc", .borderRadius = "all-md", .w = "grow", .h = "33px") {
            rcTextC(label, .font = F_BODY,
                     .color = active ? RC_CYAN_300 : s.textMuted, .wrap = "n");
        }
        rcBox(.bg = active ? s.primary : RC_TRANSPARENT, .w = "grow", .h = "3px") {}
    }
    return rcClicked(id);
}

static void update(RC_App *app, void *userData) {
    (void)app;
    AppState *st = (AppState *)userData;
    /* Deferred first-frame scan: scan_dir reports into the status log, and the
       log belongs to a frame. */
    if (st->pendingScan) {
        st->pendingScan = false;
        scan_dir(st);
        /* Preview the first icon rather than opening on an empty canvas. */
        if (st->fileCount > 0)
            select_file(st, 0);
    }
}

/* Custom titlebar glyphs. RC_AppOptions.titlebar replaces the bundled bar's
   glyphs per button, and every header this tool writes already has the
   `void (float size, RC_Color color)` shape RC_IconCallback wants - so a
   converted icon drops straight onto a window control with no adapter.

   Three rules the bar applies:
     1. a NULL state falls back to .normal - maximize sets only .normal, so its
        hover and press reuse that one glyph;
     2. an all-NULL set keeps the bundled glyph - close is left untouched;
     3. the glyph must draw in the `color` the bar passes in, never one of its
        own, or it goes invisible against the filled hover slab.

   Close is deliberately not overridden: the control a user reaches for when an
   app misbehaves should stay conventional. */
enum { TBAR_VIEWBOX = 16 };          /* the bundled glyphs' viewBox, matched */

static void tbar_draw_minimize(RC_BoundingBox bounds, RC_Color color,
                               const void *userData) {
    (void)userData;
    const float vb = (float)TBAR_VIEWBOX, stroke = 2.05f;

    /* THE RESTING GLYPH IS THE UNIVERSAL DASH, and that is a constraint rather
       than a preference: minimise is a dash on every desktop, so anything else
       here reads as "collapse". The override is still plainly visible, because
       the HOVER state below adds a mark the bundled glyph never has. */
    rcIconDrawRoundLine(bounds,  4.5f, 8.0f, 11.5f, 8.0f, vb, stroke, color);
}

static void tbar_draw_minimize_hover(RC_BoundingBox bounds, RC_Color color,
                                     const void *userData) {
    (void)userData;
    const float vb = (float)TBAR_VIEWBOX, stroke = 2.05f;

    /* Hover: the same rail, plus the chevron dropping onto it - the motion the
       button performs. */
    rcIconDrawRoundLine(bounds,  4.5f, 4.5f,  8.0f, 8.0f, vb, stroke, color);
    rcIconDrawRoundLine(bounds,  8.0f, 8.0f, 11.5f, 4.5f, vb, stroke, color);
    rcIconDrawRoundLine(bounds,  4.5f, 11.5f, 11.5f, 11.5f, vb, stroke, color);
}

static void tbar_draw_maximize(RC_BoundingBox bounds, RC_Color color,
                               const void *userData) {
    (void)userData;
    rcIconDrawRoundedRectStroke(bounds, 4.0f, 4.0f, 8.0f, 8.0f, 1.6f,
                                (float)TBAR_VIEWBOX, 1.9f, color);
}

/* The RC_IconCallback wrappers. Thin by design: rcIconEmit places the glyph and
   applies the unzoom scale. */
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
    AppState *st = (AppState *)userData;
    rcSetStyle(iconv_style());
    RC_Style s = rcGetStyle();

    /* No hand-rolled titlebar: under nativeFrame the bundled bar is drawn above
       this layout with zero app code.
       SAFE AREA. A phone draws the window edge to edge, under the status bar and
       the home indicator, and nothing moves content out of the way for you.
       rcViewport().safe hands the margins over ALREADY IN LAYOUT UNITS - spend
       them once, here at the root. NEVER divide rcGetSafeAreaInsets() by the zoom
       factor instead: that is wrong under RC_ZOOM_OPTICAL, and the conversion is
       per-axis. They are {0,0,0,0} on desktop, so this is one code path. */
    RC_Viewport vp   = rcViewport();
    RC_Insets   safe = vp.safe;

    /* THE ARM, AND THE ONE NUMBER THE PREVIEW NEEDS FROM IT: a tile is half of
       what the pane leaves inside its padding and the gap between the pair,
       clamped so the two never outgrow the pane and never shrink past a size a
       24-unit glyph can be judged at. */
    bool  compact = vp.width < (float)ICONV_THREE_PANE_W;
    float paneW   = vp.width - safe.left - safe.right
                  - (compact ? 0.0f : (float)(ICONV_SOURCES_W + ICONV_EXPORT_W));
    float tile    = (paneW - 2.0f * ICONV_PANE_PAD - ICONV_TILE_GAP) * 0.5f;
    /* A TILE IS BOUNDED BY BOTH AXES, and the height bound is READ BACK rather
       than re-derived: `prev_stage` is the container the heading and the details
       panel leave, so its measured height IS the budget, and it stays correct
       when the details panel gains a line. It is LAST FRAME'S box - the
       immediate-mode norm - and `found` is false on the very first frame. */
    {
        RC_Box stage  = rcGetElementBox("prev_stage");
        RC_Box ladder = rcGetElementBox("prev_ladder");

        if (stage.found && stage.height > 0.0f) {
            /* The ladder's own measured height, so neither number is written
               down twice. */
            float byHeight = stage.height - (float)ICONV_TILE_CHROME
                           - (ladder.found ? ladder.height + 22.0f : 0.0f);

            if (byHeight < tile) tile = byHeight;
        }
    }
    /* The pane bounds the tile on both axes, so a floor is the only clamp left. */
    if (tile < (float)ICONV_TILE_MIN) tile = (float)ICONV_TILE_MIN;

    rcColumn(.id = "Root", .bg = s.background, .pt = (uint16_t)(safe.top),
             .pb = (uint16_t)(safe.bottom), .pl = (uint16_t)(safe.left),
             .pr = (uint16_t)(safe.right), .w = "grow", .h = "grow") {
        panel_header(st);
        if (compact) {
            rcRow(.gap = 6, .px = 12, .py = 8, .w = "grow") {
                if (iconv_tab("tab_src",  "Sources", st->tab == TAB_SOURCES)) st->tab = TAB_SOURCES;
                if (iconv_tab("tab_prev", "Preview", st->tab == TAB_PREVIEW)) st->tab = TAB_PREVIEW;
                if (iconv_tab("tab_exp",  "Export",  st->tab == TAB_EXPORT))  st->tab = TAB_EXPORT;
            }
            /* One stage, the whole window, through the SAME stage functions
               the three-pane arm calls - so a resize carries state across. */
            if (st->tab == TAB_SOURCES)      panel_sources(app, st, true);
            else if (st->tab == TAB_PREVIEW) panel_preview(app, st, tile, true);
            else                             panel_export(app, st, true);
        } else {
            rcRow(.id = "Body", .w = "grow", .h = "grow") {
                panel_sources(app, st, false);
                panel_preview(app, st, tile, false);
                panel_export(app, st, false);
            }
        }
    }
    /* A scrollbar names a container, so declare only the ones this frame drew. */
    if (!compact || st->tab == TAB_SOURCES) rcScrollbar("list_files");
    if (!compact || st->tab == TAB_EXPORT) rcScrollbar("list_log");
}

/* int main(VOID). The mobile builds reach this through a shim that declares it
   as `int (void)`; an `int (int, char **)` definition called that way is
   undefined behaviour, and it links because the two units never see each
   other's prototype. The input directory is set in the app's own field. */
int main(void) {
    static AppState state;   /* large fixed arrays => keep it out of the stack */
    state.selected = -1;
    state.tab      = TAB_SOURCES;

    /* Pre-format stable per-row element ids ("f0".."f255"). */
    for (int i = 0; i < ICONV_MAX_FILES; i++) {
        snprintf(state.rowId[i], sizeof state.rowId[i], "f%d", i);
    }

    /* Input-dir default: the examples' shared assets folder, a RELATIVE repo
       path. A shipped binary is pointed elsewhere through the in-app field. */
    snprintf(state.inDir, sizeof state.inDir, "%s", "examples/assets/icons");
    /* Scan on the first frame: a converter that opens empty on a directory it
       has already filled in shows nothing of what it does. */
    state.pendingScan = true;
    snprintf(state.outDir, sizeof state.outDir, "%s", "out");

    static const float fontSizes[F_COUNT] = {
        [F_SMALL] = 13.0f,
        [F_BODY]  = 15.0f,
        [F_TITLE] = 20.0f,
    };

    rcSetStyle(iconv_style());

    RC_AppOptions opts = {
        .width            = 1000,
        .height           = 680,
        .title            = "RayClay Icon Converter",
        .clearColor       = rcGetStyle().background,
        .fontSizes        = fontSizes,
        .fontCount        = F_COUNT,
        .scratchArenaBytes = 16384,
        .nativeFrame      = true,   /* borderless + the BUNDLED titlebar (runner-drawn) */
        .updateCallback         = update,
        .layoutCallback         = layout,
        .userData         = &state,
        /* Per-button glyph overrides. `close` is absent on purpose: an all-NULL
           set keeps the bundled glyph. */
        .titlebar = {
            .minimize = { .normal = tbar_minimize, .hover = tbar_minimize_hover },
            .maximize = { .normal = tbar_maximize },
        },
    };

    return rcRunApp(&opts);
}
