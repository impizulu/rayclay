/*
    app.h - Glance, the viewer around the selector

    A small viewer gives the selector a job: a rail of the files you opened, a stage
    that shows the current one, an inspector with what is known of it, and an Open
    button (Ctrl or Cmd+O). It starts as a document app does, with its Open panel up.

    The selector itself is file_selector.h; this file shows the two ways to present
    it, picked by the room there is: a centered modal over the dimmed page, and the
    whole screen, as a phone's open sheet, where a modal's margins would leave too
    little (narrower than SHEET_W or shorter than SHEET_H).
*/
#ifndef APP_APP_H
#define APP_APP_H

#include "rayclay.h"
#include "app/theme.h"
#include "app/file_selector.h"
#include "app/samples.h"
#include "platform/platform.h"

#include "icons/rc_icons_briefcase.h"
#include "icons/rc_icons_download.h"
#include "icons/rc_icons_folder.h"
#include "icons/rc_icons_hard_drive.h"
#include "icons/rc_icons_house.h"
#include "icons/rc_icons_images.h"
#include "icons/rc_icons_moon.h"
#include "icons/rc_icons_sun.h"
#include "icons/rc_icons_x.h"

/* THE TWO PRESENTATIONS. SHEET_W is where a modal's body, with its own 20 px of padding
   and MARGIN taken off each side, still fills the selector's table layout, so a modal
   never shows the phone's list. Below it, or on a window too short for a modal's
   margins, the selector is the screen, and keeps SHEET_INSET off the window's edges
   whenever it lays out as a panel. */
enum {
    MODAL_PAD   = 20,
    SHEET_W     = FS_TABLE_AT + 2 * (MARGIN + MODAL_PAD),
    SHEET_H     = 500,
    SHEET_INSET = 16
};

/* The file-type list's choices. Folders always show, whichever is chosen. */
static const FileType FILE_TYPES[] = {
    { "All files",       NULL },
    { "Images",          "png jpg jpeg gif bmp webp svg" },
    { "Documents",       "txt md csv json pdf" },
    { "Audio and video", "mp3 m4a wav ogg flac m3u mp4 m4v mov webm mkv" },
    { "Source code",     "c h cpp cc hpp js ts py sh cmake html htm css" },
};
enum { FILE_TYPE_COUNT = (int)(sizeof FILE_TYPES / sizeof FILE_TYPES[0]) };

/* The sidebar's favorites, folders samples.h fills. */
static const struct {
    const char     *name;
    RC_IconCallback icon;
} FAVORITES[] = {
    { "Documents", rcIconFileText },
    { "Pictures",  rcIconImage },
    { "Music",     rcIconVolume2 },
    { "Projects",  rcIconBriefcase },
    { "Downloads", rcIconDownload },
};
enum { PLACE_PICTURES = 1 };

/* What the page keeps of a file it opened. */
typedef struct {
    char        path[FS_PATH_CAP];
    RC_PathInfo info;
    bool        infoOk;
} Opened;

enum { OPENED_MAX = 8 };

typedef struct {
    FileSelector picker;                /* the selector's whole state: 350 KB */
    char         picked[FS_PATH_CAP];   /* what it answers with               */
    Opened       opened[OPENED_MAX];
    const char  *openedName[OPENED_MAX];/* the narrow page's list             */
    int          openedCount;
    int          current;               /* opened[] on the stage, or -1       */
    bool         opening, closing;      /* a pick, a Close file: next frame   */
    char         appFiles[FS_PATH_CAP]; /* the App files place's folder, or "" */
    bool         copied;                /* its path went to the clipboard     */
    FilePreview  stage;
    bool         dark;                  /* the appearance; zero is light      */
    bool         flip;                  /* change it before the next frame    */
    bool         shown, started;        /* the first frame is drawn; start() ran */
    bool         panelUp;               /* the desktop's panel was up last frame */
    bool         noDialog;              /* the host has no system dialog      */
    RC_FileDialogToken browse;          /* the system's dialog while it is up */
} AppState;

/* ---- The opened files ---- */

static inline void show_current(AppState *st) {
    int i;

    for (i = 0; i < st->openedCount; i++)
        st->openedName[i] = fs_base_name(st->opened[i].path);
    st->copied = false;
    file_preview_set(&st->stage, st->current >= 0 ? st->opened[st->current].path : "");
}

/* Put a file on the stage, adding it to the rail unless it is there already. A full
   rail forgets its oldest. */
static inline void open_file(AppState *st, const char *path) {
    Opened *o;
    int     i;

    for (i = 0; i < st->openedCount; i++) {
        if (fs_compare_names(st->opened[i].path, path) == 0) {
            st->current = i;
            show_current(st);
            return;
        }
    }
    if (st->openedCount == OPENED_MAX) {
        rcReleaseFile(st->opened[0].path);   /* a dialog's copy goes; any other path stays */
        for (i = 1; i < OPENED_MAX; i++) st->opened[i - 1] = st->opened[i];
        st->openedCount--;
    }
    o = &st->opened[st->openedCount];
    rcStrCopy(o->path, path, sizeof o->path);
    o->infoOk   = rcPathInfo(path, &o->info) == RC_FILE_OK;
    st->current = st->openedCount++;
    show_current(st);
}

static inline void close_current(AppState *st) {
    int i;

    if (st->current < 0) return;
    rcReleaseFile(st->opened[st->current].path);
    for (i = st->current + 1; i < st->openedCount; i++) st->opened[i - 1] = st->opened[i];
    st->openedCount--;
    if (st->current >= st->openedCount) st->current = st->openedCount - 1;
    show_current(st);
}

/* The panel comes back in the folder it was left in, as a native one does. */
static inline void choose_file(AppState *st) {
    file_selector_open(&st->picker, st->picker.here, st->picker.rel, NULL, FILE_TYPES,
                       FILE_TYPE_COUNT);
}

/* Second frame: name the places, write the samples on a first run and put two pictures
   on the page. Where the page has room around a panel the app starts as a desktop's
   document app does, with its Open panel up and a picture picked; a phone starts on
   the page, and its Open button brings up the sheet. */
static inline void start(RC_App *app, AppState *st) {
    FileSelector *fs = &st->picker;
    /* The publisher and the app, which name the folder: a real app passes its own. */
    const char   *owned = rcAppDataPath("RayClay", "File Selector");
    char          data[FS_PATH_CAP], path[FS_PATH_CAP];
    RC_PathInfo   assets;
    int           i;

    rcStrCopy(data, owned ? owned : "", sizeof data);
    rcStrCopy(st->appFiles, data, sizeof st->appFiles);
    if (data[0]) {
        seed_samples(rcAppArena(app), data);
        for (i = 0; i < (int)(sizeof FAVORITES / sizeof FAVORITES[0]); i++)
            if (fs_join(path, (int)sizeof path, data, FAVORITES[i].name))
                file_selector_add_place(fs, "Favorites", FAVORITES[i].name, FAVORITES[i].icon,
                                        path);
        file_selector_add_place(fs, "Locations", "App files", rcIconHouse, data);
    }
    if (PLATFORM_HAS_WORKING_FOLDER) {
        file_selector_add_place(fs, "Locations", "Working folder", rcIconFolder, ".");
        /* A run from the repository's root also finds the assets its examples use. */
        if (rcPathInfo("examples/assets", &assets) == RC_FILE_OK && assets.isDirectory)
            file_selector_add_place(fs, "Locations", "Example assets", rcIconImages,
                                    "examples/assets");
    }
    if (data[0]) {
        if (fs_join(path, (int)sizeof path, data, "Pictures/Desert dusk.bmp")) open_file(st, path);
        if (fs_join(path, (int)sizeof path, data, "Pictures/Alpine lake.bmp")) open_file(st, path);
    }
    if (rcViewport().width >= (float)WIDE_W)
        file_selector_open(fs, data[0] ? PLACE_PICTURES : 0, "", "Autumn ridge.bmp", FILE_TYPES,
                           FILE_TYPE_COUNT);
    st->started = true;
}

/* THE WINDOW FIRST, as a web page paints before its fetch answers: the first frame draws
   the page with nothing open, and start(), before the second, writes the samples on a first
   run (four pictures, 6.7 MB) and opens two of them, so the disk never holds the window back.
   A PICK AND A CLOSE TAKE EFFECT HERE, before the frame, as the selector's own changes do: the
   rail, the inspector and the list draw names from opened[]'s own bytes, which a close and a
   pick on a full rail shift, and text is borrowed until the frame is drawn. */
static inline void update(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;

    if (!st->shown)        st->current = -1;
    else if (!st->started) start(app, st);
    if (st->closing) close_current(st);
    if (st->opening) open_file(st, st->picked);
    st->opening = st->closing = false;
}

/* THE SYSTEM'S DIALOG. The panel is the app's own picker over the files the app keeps
   (rcAppDataPath), where no system dialog looks; the user's own files are the system's dialog's
   job. Browse asks for it from the click's own frame, as the web requires, and the answer opens
   like any other file. A host with no dialog answers UNSUPPORTED once, and Browse goes. */
static inline void browse(AppState *st) {
    if (!st->browse) st->browse = rcFileDialogOpen(RC_FILE_DIALOG_FILE, NULL);
}

static inline void browse_answer(AppState *st) {
    RC_FileDialogResult r = rcFileDialogPoll(st->browse);

    if (!st->browse || r.status == RC_FILE_DIALOG_PENDING) return;
    st->browse   = 0;
    st->noDialog = r.status == RC_FILE_DIALOG_UNSUPPORTED;
    if (r.count > 0) {
        st->picker.open = false;
        open_file(st, r.paths[0]);
    }
}

/* Said where the panel starts, so it is never taken for the system's dialog. It stands at the
   right, beside Browse, so the panel's title is the first thing at the left. */
static inline void own_picker_note(AppState *st) {
    RC_Style s = rcGetStyle();

    rcRow(.gap = 12, .pb = 12, .align = "cl", .w = "grow") {
        rcSpacer() {}
        rcTextL("Files this app keeps.", .color = s.textMuted, .size = 13);
        if (!st->noDialog
            && rcButton("browse_panel", PLATFORM_BROWSE_LABEL, .disabled = st->browse != 0,
                        .className = FS_CONTROL(""), .leadingIcon = rcIconHardDrive))
            browse(st);
    }
}

/* ---- The page ---- */

/* The file's name stands in the bar where the rail is; a narrow page names it in the
   list under the stage. On a phone the bar's fill runs under the status bar and to the
   screen's sides, and its content keeps clear of them by the safe insets. */
static inline void top_bar(AppState *st, bool name, RC_Insets safe) {
    RC_Style s = rcGetStyle();

    rcRow(.gap = 12, .pt = safe.top, .pl = 16 + safe.left, .pr = 16 + safe.right, .align = "cl",
          .w = "grow", .hType = RC_PX(BAR_H + safe.top),
          .className = "bg-surface border-b border-border") {
        rcBox(.bg = s.primary, .align = "cc", .borderRadius = "all-md", .wType = RC_PX(28),
              .hType = RC_PX(28)) {
            rcIconImages(16.0f, RC_WHITE);
        }
        rcTextL("Glance", .font = rcFontSemibold(15.0f), .color = s.text);
        rcBox(.overflow = "hidden", .w = "grow") {
            if (name && st->current >= 0)
                rcTextC(st->openedName[st->current], .color = s.textMuted, .size = 13, .wrap = "e");
        }
        rcBox(.tooltip = "Switch between the dark and light appearance") {
            if (rcButton("appearance", "", .className = FS_CONTROL(""), .variant = RC_BTN_GHOST,
                         .leadingIcon = st->dark ? rcIconSun : rcIconMoon))
                st->flip = true;
        }
        rcBox(.tooltip = "Open a file from your computer, in the system's dialog") {
            if (!st->noDialog && rcButton("browse", "Browse...", .disabled = st->browse != 0,
                                          .className = FS_CONTROL(""),
                                          .leadingIcon = rcIconHardDrive))
                browse(st);
        }
        rcBox(.tooltip = "Open one of the app's files (" PLATFORM_OPEN_SHORTCUT ")") {
            if (rcButton("open", "Open", .disabled = !st->picker.placeCount,
                         .className = FS_CONTROL(""), .variant = RC_BTN_PRIMARY,
                         .leadingIcon = rcIconFolderSolid))
                choose_file(st);
        }
    }
}

/* The rail: every file opened, the current one washed, a click puts one back. Each row is
   a Tab stop that Enter and Space put back too, and the column's padding is the room its
   focus ring takes, which the scrolling column would clip. Hovering lifts a row: hover:
   reads the row's own box. A row is named by its file's path, never by its place, which a
   close or a full rail's open shifts: a name that moved would hand the focus to another row. */
static inline void rail(RC_App *app, AppState *st) {
    RC_Style  s = rcGetStyle();
    RC_Arena *m = rcAppArena(app);
    int       i;

    rcColumn(.id = "rail", .gap = 2, .p = 8, .scroll = "v", .h = "grow",
             .wType = RC_PX(RAIL_W), .className = "bg-surface border-r border-border") {
        rcBox(.pt = 4, .pb = 6, .pl = 8) {
            rcTextL("Opened files", .font = rcFontSemibold(11.0f), .color = s.textMuted);
        }
        if (!st->openedCount)
            rcBox(.pl = 8) { rcTextL("Nothing yet.", .color = s.textMuted, .size = 13); }
        for (i = 0; i < st->openedCount; i++) {
            const char   *id   = rcFormat(m, "rail-%s", st->opened[i].path).chars;
            const char   *name = st->openedName[i];
            const FsKind *kind = fs_kind_of(name);

            rcRow(.id = id, .focusable = true, .gap = 8, .px = 8, .align = "cl",
                  .overflow = "hidden", .borderRadius = "all-md", .w = "grow",
                  .className = st->current == i ? FS_CONTROL("bg-foreground/11")
                                                : FS_CONTROL("hover:bg-foreground/5")) {
                kind->icon(16.0f, kind->tint);
                rcTextC(name, .color = s.text, .size = 13, .wrap = "e", .select = RC_SELECT_NONE);
            }
            if (rcClicked(id)) {
                st->current = i;
                show_current(st);
            }
        }
    }
    rcScrollbar("rail");
}

/* The stage: the picture, or the file's kind where there is none to draw. */
static inline void stage(AppState *st) {
    RC_Style s = rcGetStyle();

    /* It hides what overflows, so its box is the room the page gives it, never the size
       of what it drew last frame. */
    rcColumn(.id = "stage", .bg = s.background, .gap = 12, .p = 24, .align = "cc",
             .overflow = "hidden", .w = "grow", .h = "grow") {
        if (st->current < 0) {
            rcIconImages(44.0f, s.textMuted);
            rcTextL("No file open", .font = rcFontSemibold(18.0f), .color = s.text);
            rcTextL("Press " PLATFORM_OPEN_SHORTCUT " or choose Open to pick one.",
                    .color = s.textMuted, .size = 13);
        } else if (file_preview_ready(&st->stage)) {
            /* A picture sizes itself to the stage. Vector art has to be told its side, so
               it alone is given the room the stage had last frame, less the padding. */
            RC_Box box  = rcGetElementBox("stage");
            float  room = (box.width < box.height ? box.width : box.height) - 48.0f;

            file_preview_draw(&st->stage, box.found ? room : 320.0f);
        } else {
            const char   *name = st->openedName[st->current];
            const FsKind *kind = fs_kind_of(name);

            kind->icon(72.0f, kind->tint);
            rcTextC(name, .font = rcFontSemibold(18.0f), .color = s.text, .textAlign = "c");
            rcTextC(kind->label, .color = s.textMuted, .size = 13);
        }
    }
}

/* One label over its value. */
static inline void field(const char *label, RC_String value) {
    RC_Style s = rcGetStyle();

    rcColumn(.gap = 2, .w = "grow") {
        rcTextC(label, .font = rcFontSemibold(11.0f), .color = s.textMuted);
        rcText(value, .color = s.text, .size = 13, .wordBreak = "b");
    }
}

/* The folder a file is in. One under App files is named from that place on, as the sidebar
   names it: the folders above it are the system's, and name the user's account. */
static inline RC_String where_text(RC_Arena *m, const AppState *st, const Opened *o,
                                   const char *name) {
    int skip = st->appFiles[0] && fs_find(o->path, st->appFiles) == 0 ? fs_len(st->appFiles) : 0;

    return rcFormat(m, "%s%.*s", skip ? "App files/" : "", (int)(name - o->path) - skip,
                    o->path + skip);
}

/* What is known of the current file. */
static inline void inspector(RC_App *app, AppState *st) {
    RC_Style  s = rcGetStyle();
    RC_Arena *m = rcAppArena(app);

    rcColumn(.gap = 12, .p = 16, .h = "grow", .wType = RC_PX(INSPECTOR_W),
             .className = "bg-surface border-l border-border") {
        if (st->current < 0) {
            rcTextL("Details of the open file show here.", .color = s.textMuted, .size = 13);
        } else {
            const Opened *o    = &st->opened[st->current];
            const char   *name = st->openedName[st->current];

            rcTextC(name, .font = rcFontSemibold(15.0f), .color = s.text, .wordBreak = "b");
            fs_rule();
            field("Kind", rcStringFromCStr(fs_kind_of(name)->label));
            if (o->infoOk) field("Size", fs_size_text(m, o->info.size));
            if (st->stage.image.handle)
                field("Dimensions", rcFormat(m, "%d x %d", st->stage.image.width,
                                             st->stage.image.height));
            if (o->infoOk) field("Modified", fs_date_text(m, o->info.modifiedUnixSeconds));
            field("Where", where_text(m, st, o, name));
            rcSpacer() {}
            rcColumn(.gap = 8, .w = "grow", .className = "items-stretch") {
                if (rcButton("copy_path", st->copied ? "Path copied" : "Copy path",
                             .className = FS_CONTROL(""))) {
                    rcClipboardSet(o->path);
                    st->copied = true;
                }
                if (rcButton("close_file", "Close file", .className = FS_CONTROL(""),
                             .leadingIcon = rcIconX))
                    st->closing = true;
            }
        }
    }
}

/* Below WIDE_W the rail is a list and the inspector one line, under the stage. Its fill
   runs under a phone's home indicator, as the top bar's does under the status bar. */
static inline void narrow_footer(RC_App *app, AppState *st, RC_Insets safe) {
    RC_Style s = rcGetStyle();

    if (st->current < 0) return;
    rcRow(.gap = 8, .p = 12, .pb = 12 + safe.bottom, .pl = 12 + safe.left, .pr = 12 + safe.right,
          .align = "cl", .w = "grow", .className = "bg-surface border-t border-border") {
        rcBox(.overflow = "hidden", .w = "grow") {
            if (rcCombo("opened_list", &st->current, st->openedName, st->openedCount,
                        .className = FS_CONTROL("")))
                show_current(st);
        }
        if (st->opened[st->current].infoOk)
            rcText(fs_size_text(rcAppArena(app), st->opened[st->current].info.size),
                   .color = s.textMuted, .size = 13, .wrap = "n");
        if (rcButton("close_file", "", .className = FS_CONTROL(""), .leadingIcon = rcIconX))
            st->closing = true;
    }
}

static inline void layout(RC_App *app, void *userData) {
    AppState   *st      = (AppState *)userData;
    RC_Viewport vp      = rcViewport();
    RC_Insets   safe    = vp.safe;                /* a phone's bars and cutout; 0 on a desktop */
    float       w       = vp.width - safe.left - safe.right;
    float       h       = vp.height - safe.top - safe.bottom;
    bool        sheet   = w < (float)SHEET_W || h < (float)SHEET_H;
    /* The screen ends where a soft keyboard begins, so the selector's buttons stay
       above it while the search is being typed in. */
    float       bottom  = safe.bottom > vp.keyboard.bottom ? safe.bottom : vp.keyboard.bottom;
    bool        wasOpen = st->picker.open;
    bool        picked  = false;

    browse_answer(st);
    /* The appearance is state, applied before the frame's first element, or the frame
       comes out half in each. The clear color follows it, and the window's own titlebar
       follows that. */
    if (st->flip) {
        st->flip = false;
        st->dark = !st->dark;
    }
    rcSetStyle(app_style(st->dark));
    rcWindowSetClearColor(rcAppMainWindow(app), rcGetStyle().background);
    if (sheet && st->picker.open) {
        /* THE PHONE'S SHEET: the selector is the screen. It hides itself on Cancel,
           on Android's Back and on a pick, and the page is back. */
        st->picker.inset = SHEET_INSET;
        rcColumn(.id = "root", .bg = rcGetStyle().surface, .pt = safe.top,
                 .pb = (float)bottom, .pl = safe.left, .pr = safe.right,
                 .w = "grow", .h = "grow") {
            rcColumn(.pt = SHEET_INSET, .px = SHEET_INSET, .w = "grow") { own_picker_note(st); }
            picked = file_selector(app, &st->picker, st->picked, (int)sizeof st->picked);
        }
    } else {
        if (!st->picker.open && rcKeyPressed(RC_KEY_O) && rcModDown(RC_MOD_PRIMARY))
            choose_file(st);
        /* THE PAGE draws to the screen's edges: each bar pads its own content by the
           phone's insets, and the row between them keeps clear of the sides, and of the
           home indicator where no footer stands under it. */
        rcColumn(.id = "root", .bg = rcGetStyle().background, .w = "grow", .h = "grow") {
            top_bar(st, w >= (float)WIDE_W, safe);
            rcRow(.pb = w < (float)WIDE_W && st->current >= 0 ? 0.0f : safe.bottom,
                  .pl = safe.left, .pr = safe.right, .w = "grow", .h = "grow") {
                if (w >= (float)WIDE_W) rail(app, st);
                stage(st);
                if (w >= (float)WIDE_W) inspector(app, st);
            }
            if (w < (float)WIDE_W) narrow_footer(app, st, safe);
        }
        /* THE DESKTOP'S PANEL: a modal, its body sized here so the panel and its own
           padding stay MARGIN clear of the window. Esc closes it, as a dialog's Esc
           does; a click outside does not, so a stray click never loses the folder the
           user walked to. */
        if (!sheet && st->picker.open
            && rcBeginModal("picker", &st->picker.open, .noBackdropDismiss = true)) {
            float pw = w - 2.0f * (float)(MARGIN + MODAL_PAD);
            float ph = h - 2.0f * (float)(MARGIN + MODAL_PAD);

            /* A modal focuses its first stop as it opens: the sidebar's first place, or a
               toolbar button, either of which would take the Enter meant to open the pick.
               The search takes the focus instead, on the frame the panel comes up (from
               Open, from start() or back from the sheet): typing goes there anyway, and Enter
               and the up and down arrows still reach the list. */
            if (!st->panelUp) rcSetFocus("fs_filter");
            st->picker.inset = 0.0f;
            rcColumn(.wType = RC_PX(pw < (float)PANEL_W ? pw : (float)PANEL_W),
                     .hType = RC_PX(ph < (float)PANEL_H ? ph : (float)PANEL_H)) {
                own_picker_note(st);
                picked = file_selector(app, &st->picker, st->picked, (int)sizeof st->picked);
            }
            rcEndModal();
        }
    }
    if (picked) st->opening = true;
    /* The selector opened or closed in mid-frame, so this frame drew the other
       screen: ask for the one that shows it, as a flipped appearance does, for the
       one that starts the app and for the one that applies a pick or a close. A
       closed panel lets its picture go. */
    st->shown   = true;
    st->panelUp = !sheet && st->picker.open;
    if (st->picker.open != wasOpen || st->flip || !st->started || st->opening || st->closing)
        rcWindowRequestFrame(rcAppMainWindow(app));
    if (!st->picker.open) file_preview_set(&st->picker.preview, "");
}

#endif /* APP_APP_H */
