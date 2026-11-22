/*
    app.h - the shell: the start-up, the title bar, the dialogs and the layout

    One translation unit: main.c includes this and every function is `static inline`. Read the
    app from layout() at the bottom, which is the frame in order: take input, change the state,
    then declare the UI from that state.
*/
#ifndef APP_APP_H
#define APP_APP_H

#include "rayclay.h"
#include "app/panels.h"
#include "platform/platform.h"

#include "icons/rc_icons_list.h"
#include "icons/rc_icons_menu.h"
#include "icons/rc_icons_panel_left.h"
#include "icons/rc_icons_panel_right.h"
#include "icons/rc_icons_rayclay_logo.h"

#ifndef RC_EX28_FONT_DIR
    #define RC_EX28_FONT_DIR "examples/assets/fonts/"
#endif

/* The folder it opens on, and the file: the app's own source, so the first thing on screen is
   real code and the tree around it is real too. */
#define START_FOLDER "examples"
#define START_FILE   "examples/ex28_ide/src/app/editor.h"
#define START_SYMBOL "editor_keys"

/* Below this layout width one screen shows at a time, under a bottom navigation. A large phone
   on its side is wider than that, so a finger's window shorter than COMPACT_H is compact too:
   the two orientations hold the same app. A mouse's short window keeps the desktop shell. */
#define COMPACT_W 860.0f
#define COMPACT_H 500.0f
/* Below this the minimap's 72 px are better spent on code; the compact shell never draws one. */
#define MINIMAP_MIN_W 860.0f
/* Below this window height the bottom panel starts folded. */
#define PANEL_MIN_H 560.0f
/* Zen mode's reading measure: the code is never wider. */
#define ZEN_W 1040.0f

/* ---- The start-up. ---- */

/* Find a symbol by name, to open a file at the code that matters. */
static inline int symbol_line(const Doc *d, const char *name) {
    int n = str_len(name), i;

    for (i = 0; i < d->nSym; i++)
        if (d->sym[i].nameLen == n && same_bytes(d->text + d->sym[i].name, name, n))
            return d->sym[i].line;
    return -1;
}

/* The folder walk, the files it opens and the code font. update() runs this on the second
   frame, so the first is drawn before any of them is read (docs/getting-started.md). */
static inline void app_start(AppState *st) {
    Doc        *d = NULL;
    RC_PathInfo info;

    st->started = true;
    st->now = 0.0;      /* the start-up lines read 0.0s on every machine */
    st->mono = rcLoadFont(RC_EX28_FONT_DIR "AdwaitaMono-Regular.ttf", (float)SZ_CODE);
    st->cell = rcTextWidth(st->mono, SZ_CODE, "M", 1);
    /* A phone bundle carries examples/assets but not this source: open the tree only where
       its start file is. The logo's tab puts the live SVG preview one click away. */
    if (rcPathInfo(START_FILE, &info) == RC_FILE_OK && workspace_open(st, START_FOLDER)) {
        open_file(st, "examples/ex28_ide/src/main.c");
        open_file(st, "examples/assets/icons/rayclay-logo.svg");
        d = open_file(st, START_FILE);
    } else {
        workspace_samples(st);
        open_file(st, "README.md");
        open_file(st, "logo.svg");
        d = open_file(st, "tide.c");
    }
    if (d) {
        doc_analyze(d, st->arena);
        go_to(st, d, symbol_line(d, START_SYMBOL), 0);
    }
    st->wantCaret = !rcPointerIsCoarse();
}

/* The explorer follows the editor: when another file comes to the front, its row is shown. */
static inline void follow_active_tab(AppState *st, const Doc *d) {
    int node;

    if (!d || same_text(st->shownDoc, d->scrollId))
        return;
    rcStrCopy(st->shownDoc, d->scrollId, sizeof st->shownDoc);
    node = node_for_path(&st->ws, d->path);
    if (node >= 0) {
        tree_reveal(&st->ws, node);
        st->revealNode = node;
    }
}

/* What a dialog or a drop hands over. Where it is a private copy the user cannot get back to,
   a Save must go to the user, and the copy is released with the tab. */
static inline void open_handed(AppState *st, const char *path) {
    Doc *d = open_file(st, path);

    if (d && HANDED_PATHS_ARE_COPIES) {
        d->memoryOnly = true;
        d->imported   = true;
    } else if (HANDED_PATHS_ARE_COPIES) {
        rcReleaseFile(path);        /* no tab took the copy, so no tab's close releases it */
    }
    st->screen = SCREEN_CODE;
}

/* A folder answer is a private copy there too, released when another folder takes its place:
   its tabs already hold their text, and every import would otherwise stay for the run. */
static inline void open_handed_folder(AppState *st, const char *folder) {
    char was[PATH_BYTES];
    bool wasCopy = st->ws.imported;

    rcStrCopy(was, st->ws.rootPath, sizeof was);
    if (workspace_open(st, folder)) {
        st->ws.imported = HANDED_PATHS_ARE_COPIES;
        if (wasCopy)
            rcReleaseFile(was);
    } else if (HANDED_PATHS_ARE_COPIES) {
        rcReleaseFile(folder);
    }
}

/* Answers that arrive between frames: file dialogs, a drop, a paste from a menu. */
static inline void poll_requests(AppState *st) {
    RC_FileDialogResult r;
    Doc                *d;
    int                 i;

    /* A token is held only while its dialog is up (ask_dialog, hand_over). */
    r = rcFileDialogPoll(st->askFile);
    if (r.status != RC_FILE_DIALOG_PENDING)
        st->askFile = 0;
    if (r.status == RC_FILE_DIALOG_DONE && r.count > 0) {
        open_handed(st, r.paths[0]);
    } else if (r.status == RC_FILE_DIALOG_UNSUPPORTED) {
        out_log(st, SEV_WARNING, "This system has no file dialog");
    }
    r = rcFileDialogPoll(st->askFolder);
    if (r.status != RC_FILE_DIALOG_PENDING)
        st->askFolder = 0;
    if (r.status == RC_FILE_DIALOG_DONE && r.folder)
        open_handed_folder(st, r.folder);
    else if (r.status == RC_FILE_DIALOG_UNSUPPORTED)
        out_log(st, SEV_WARNING, "This system has no folder dialog");
    r = rcFileDialogPoll(st->askSaveAs);
    if (r.status != RC_FILE_DIALOG_PENDING)
        st->askSaveAs = 0;
    d = doc_by_id(st, st->saveId);
    if (r.status == RC_FILE_DIALOG_DONE) {
        if (d && (!d->path[0] || d->memoryOnly))
            d->savedTop = st->saveTop;
        if (d)
            out_log(st, SEV_INFO, rcFormat(st->arena, "Handed over %s (%d bytes)", d->name,
                                           d->len).chars);
        if (st->closeWhy != CLOSE_NONE)
            close_save(st);
    } else if (r.status == RC_FILE_DIALOG_CANCELED) {
        st->closeWhy = CLOSE_NONE;
    } else if (r.status >= RC_FILE_DIALOG_REFUSED) {
        rcStrCopy(st->alert, "This system could not show a save dialog, so nothing was saved.",
                  sizeof st->alert);
        out_log(st, SEV_ERROR, st->alert);
        st->alertOpen = true;
        st->closeWhy  = CLOSE_NONE;
    }
    for (i = 0; i < rcDroppedFileCount(); i++)
        open_handed(st, rcDroppedFile(i));
    if (st->pasteToken) {
        const RC_ClipboardResult clip = rcClipboardPoll(st->pasteToken);

        if (clip.status != RC_CLIPBOARD_PENDING)
            st->pasteToken = 0;   /* answered, refused or superseded: stop waiting */
        if (clip.status == RC_CLIPBOARD_DONE && clip.text[0]) {
            d = active_doc(st);
            if (d && !d->unreadable) {
                doc_insert(d, clip.text, str_len(clip.text), EDIT_OTHER);
                st->reveal = REVEAL_NEAREST;
            }
        }
    }
}

/* A focused find field takes the press to blur, yet the key still means "close the find bar", so
   its own key counts. With nothing to back out of the key goes unread, and Android's Back stays
   the system's. */
static inline void escape_key(AppState *st, Doc *d) {
    if (st->pal.open || st->menuOpen || rcIsModalOpen()
        || (!st->zen && !st->find.open && !(d && has_sel(d))))
        return;
    if (!rcKeyPressed(RC_KEY_ESCAPE) && !rcKeyPressedOn("find_q", RC_KEY_ESCAPE)
        && !rcKeyPressedOn("find_r", RC_KEY_ESCAPE))
        return;
    if (st->zen) {
        st->zen = false;
    } else if (st->find.open) {
        st->find.open = false;
    } else if (d && has_sel(d)) {
        d->anchor = d->caret;
    }
    st->wantCaret = !rcPointerIsCoarse();
}

/* Keyboard input for the editor, then its mouse. All of it runs before a line is declared. */
static inline void editor_input(AppState *st, Doc *d) {
    bool modal = rcIsModalOpen() || st->pal.open;

    if (st->sink[0]) {
        take_typed(st, d);
        rcWindowRequestFrame(rcAppMainWindow(st->app));
    }
    if (st->swallowFrames)
        st->swallowFrames--;
    if (!modal && rcIsFocused(SINK_ID))
        editor_keys(st, d);
    if (!modal) {
        if (rcPointerIsCoarse()) {
            /* A press may be the start of a scroll: only a tap places the caret. */
            if (rcClicked(d->scrollId)) {
                set_caret(st, d, position_at_pointer(st, d), false);
                st->wantCaret = true;
            }
        } else {
            editor_mouse(st, d);
        }
    }
    update_brackets(st, d);
    update_word(st, d);
    if (st->find.open) {
        unsigned rev = st->find.seenRev;
        bool     same = same_text(st->find.seen, st->find.query);

        find_refresh(st, d);
        /* A query being typed jumps to its first hit; an edit in the text does not. */
        if (!same && rev == st->find.seenRev)
            find_select(st, d);
    }
    apply_reveal(st, d);
}

/* The field that takes the keyboard, last in the frame so Tab stays the editor's (not on the web,
   where the browser takes it: rcTextInput's notes on Tab). It sits on the caret, so its own
   caret and a composition draw in the text. A float attached to a scroller by id is never cut by
   that scroller (RC_Float's notes), so a clipping box the view's size holds it: scrolled away,
   the caret is cut at the view's edge. PASSTHROUGH leaves clicks to the code beneath. */
static inline void caret_field(AppState *st, Doc *d) {
    RC_ScrollInfo si  = rcGetScrollInfo(d->scrollId);
    RC_Box        box = rcGetElementBox(d->scrollId);
    int           ln  = doc_line_of(d, d->caret);
    float         x   = line_x(st, d, ln, d->caret - d->line[ln]) + (float)CODE_PAD - si.offsetX;
    float         y   = (float)ln * (float)LINE_H - si.offsetY;
    /* h-5 and leading-5 are a code line's LINE_H, 20 px. */
    const char   *look = rcFormat(st->arena, "h-5 p-0 border-0 bg-transparent font-[%d] leading-5",
                                  st->mono).chars;

    rcBox(.overflow = "hidden", .wType = RC_PX(box.width), .hType = RC_PX(box.height),
          .floating = { .to = RC_ATTACH_ELEMENT, .toId = d->scrollId,
                        .parent = RC_ANCHOR_TOP_LEFT, .element = RC_ANCHOR_TOP_LEFT,
                        .capture = RC_CAPTURE_PASSTHROUGH }) {
        rcBox(.wType = RC_PX(320), .hType = RC_PX(LINE_H),
              .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                            .element = RC_ANCHOR_TOP_LEFT, .offset = { x, y },
                            .capture = RC_CAPTURE_PASSTHROUGH }) {
            rcTextArea(SINK_ID, st->sink, SINK_BYTES, .className = look);
        }
    }
}

/* Who gets the keyboard once the frame is declared. The editor takes it back whenever no other
   field holds it, so a click on a tab or a button never leaves the caret stranded. */
static inline void settle_focus(AppState *st, const Doc *d) {
    if (st->pal.open)
        return;
    if (st->menuOpen) {
        /* A popup leaves a field its focus, so typing would reach the document behind it. */
        if (rcIsFocused(SINK_ID))
            rcSetFocus(NULL);
        return;
    }
    if (st->focusSearch && st->sideView == SIDE_SEARCH) {
        rcSetFocus("search_q");
        st->focusSearch = false;
        return;
    }
    if (st->find.focusQuery && st->find.open) {
        rcSetFocus("find_q");
        st->find.focusQuery = false;
        return;
    }
    /* Not under the save conflict: a later rcSetFocus wins over the Cancel save_doc focuses. */
    if (st->wantCaret && d && !d->unreadable && !st->conflictOpen && !field_has_keys())
        rcSetFocus(SINK_ID);
}

/* ---- The title bar. ---- */

/* A menu of commands, under a word or, with "" for the word, under the icon alone. */
static inline void menu_of(AppState *st, const char *id, const char *title, RC_IconCallback icon,
                           const Command *items, int count) {
    int i;

    /* A ghost button has no box of its own: on the bar it shows only under the pointer. */
    if (rcBeginMenu(id, title, .variant = RC_BTN_GHOST, .leadingIcon = icon)) {
        st->menuOpen = true;
        for (i = 0; i < count; i++) {
            const char *keys = st->compact || rcPointerIsCoarse()
                               ? "" : command_keys(st->arena, items[i]);

            /* rcMenuItem draws one label, so the shortcut travels in it. */
            if (rcMenuItem(keys[0] ? rcFormat(st->arena, "%s  (%s)", command_label(items[i]),
                                              keys).chars
                                   : command_label(items[i]),
                           .disabled = !command_enabled(st, items[i])))
                post_command(st, items[i]);
        }
        rcEndMenu();
    }
}

static const Command MENU_FILE[] = {
    CMD_NEW_FILE, CMD_OPEN_FILE, CMD_OPEN_FOLDER, CMD_SAVE, CMD_SAVE_AS, CMD_REVERT, CMD_SETTINGS,
    CMD_CLOSE_TAB, CMD_CLOSE_ALL, CMD_QUIT,
};
static const Command MENU_EDIT[] = {
    CMD_UNDO, CMD_REDO, CMD_CUT, CMD_COPY, CMD_PASTE, CMD_SELECT_ALL, CMD_FIND, CMD_REPLACE,
    CMD_COMMENT, CMD_DUPLICATE_LINE, CMD_DELETE_LINE, CMD_MOVE_UP, CMD_MOVE_DOWN,
};
static const Command MENU_VIEW[] = {
    CMD_PALETTE, CMD_EXPLORER, CMD_SEARCH, CMD_PROBLEMS, CMD_OUTPUT, CMD_TOGGLE_SIDEBAR,
    CMD_TOGGLE_PANEL, CMD_MINIMAP, CMD_PREVIEW, CMD_ZEN, CMD_THEME,
};
static const Command MENU_GO[] = {
    CMD_QUICK_OPEN, CMD_GOTO_LINE, CMD_GOTO_SYMBOL, CMD_NEXT_TAB, CMD_PREV_TAB, CMD_NEXT_PROBLEM,
    CMD_PREV_PROBLEM,
};
static const Command MENU_HELP[] = { CMD_PALETTE, CMD_ABOUT };

/* The compact window's single menu holds what the phone can do without a physical keyboard. */
static const Command MENU_PHONE[] = {
    CMD_PALETTE, CMD_QUICK_OPEN, CMD_NEW_FILE, CMD_SAVE, CMD_FIND, CMD_REPLACE, CMD_UNDO, CMD_REDO,
    CMD_CLOSE_TAB, CMD_THEME, CMD_ABOUT,
};

/* The pill in the middle is the command center: it opens the palette and names the folder. */
/* As wide as its box, which caps it: it shrinks with a narrow window before the window's
   controls could be pushed out, and the folder's name ends in an ellipsis. */
static inline void command_center(AppState *st) {
    const Palette *t  = theme();
    const char    *id = "command_center";

    rcRow(.id = id, .gap = 8, .px = 10, .align = "cc", .overflow = "hidden",
          .borderRadius = "all-md", .border = { .color = t->edge, .width = 1 }, .w = "grow",
          .hType = RC_PX(24), .tooltip = "Go to File (" PRIMARY_KEY "+P)",
          .className = "cmd-pill min-w-0") {
        rcIconSearch(14.0f, t->muted);
        label_cut(st->ws.rootName, SZ_SMALL, t->text);
    }
    if (rcClicked(id))
        post_command(st, CMD_QUICK_OPEN);
}

static inline bool bar_chip(const char *id, RC_IconCallback icon, bool on, const char *tip) {
    const Palette *t = theme();

    rcBox(.id = id, .align = "cc", .borderRadius = "all-sm", .wType = RC_PX(26),
          .hType = RC_PX(24), .tooltip = tip, .className = "bar-chip") {
        icon(16.0f, on ? t->bright : t->muted);
    }
    return rcClicked(id);
}

/* THE ID IS THE BEHAVIOR: a press on RC_ID_WINDOW_DRAG moves the window, and an element with
   .noDrag is where a click stays a click. .titlebar.height is unzoomed px, so the band is drawn in
   rcUnzoomed() to keep the two rectangles equal. The bar folds on Zen mode but keeps a rail:
   with no drag region at all the window could not be moved. `roomy` is the wide shell with
   room for its five menus and its layout chips; the compact shell has one menu and no panels
   to toggle. */
static inline void title_bar(AppState *st, bool roomy) {
    const Palette *t = theme();

    rcUnzoomed() {
        rcColumn(.id = RC_ID_WINDOW_DRAG, .bg = t->title, .overflow = "hidden", .w = "grow",
                 .hType = RC_PX(st->barH)) {
            if (st->barH > (float)BAR_RAIL_H + 4.0f) {
                rcRow(.gap = 4, .align = "cl", .w = "grow", .hType = RC_PX(st->barH)) {
                    /* Two sides that grow alike keep the command center on the window's middle,
                       as VS Code's is, until one side's own content needs the room. */
                    rcRow(.align = "cl", .w = "grow") {
                        rcRow(.gap = 2, .pl = 10, .align = "cl", .noDrag = true) {
                            rcSvg(RC_EX28_ICON_DIR "rayclay-logo.svg", 18.0f, t->text);
                            rcBox(.wType = RC_PX(6)) {}
                            if (roomy) {
                                menu_of(st, "menu_file", "File", NULL, MENU_FILE,
                                        COUNT_OF(MENU_FILE));
                                menu_of(st, "menu_edit", "Edit", NULL, MENU_EDIT,
                                        COUNT_OF(MENU_EDIT));
                                menu_of(st, "menu_view", "View", NULL, MENU_VIEW,
                                        COUNT_OF(MENU_VIEW));
                                menu_of(st, "menu_go", "Go", NULL, MENU_GO, COUNT_OF(MENU_GO));
                                menu_of(st, "menu_help", "Help", NULL, MENU_HELP,
                                        COUNT_OF(MENU_HELP));
                            } else {
                                menu_of(st, "menu_phone", "", rcIconMenu, MENU_PHONE,
                                        COUNT_OF(MENU_PHONE));
                            }
                        }
                    }
                    rcBox(.noDrag = true, .w = "grow", .wMax = RC_VAL(roomy ? 360.0f : 120.0f),
                          .className = "min-w-0") {
                        command_center(st);
                    }
                    rcRow(.align = "cr", .w = "grow") {
                        rcRow(.gap = 2, .align = "cl", .noDrag = true) {
                            if (roomy) {
                                if (bar_chip("bar_side", rcIconPanelLeft, st->sideOpen,
                                             "Toggle Sidebar (" PRIMARY_KEY "+B)"))
                                    post_command(st, CMD_TOGGLE_SIDEBAR);
                                if (bar_chip("bar_panel", rcIconList, st->panelOpen,
                                             "Toggle Panel (" PRIMARY_KEY "+J)"))
                                    post_command(st, CMD_TOGGLE_PANEL);
                                if (bar_chip("bar_minimap", rcIconPanelRight, st->minimapOn,
                                             "Toggle Minimap"))
                                    post_command(st, CMD_MINIMAP);
                            }
                            rcWindowControls();
                        }
                    }
                }
            }
        }
    }
}

/* ---- Dialogs. ---- */

/* A dialog's column: its own measure, narrowed so it fits a phone inside the panel's padding. */
static inline float dialog_w(float wanted) {
    float room = rcViewport().width - 64.0f;

    return wanted < room ? wanted : room;
}

/* A file closed from inside the app is never dropped without asking: the tab's X, Close All and
   File > Quit all come here, and so do the window's own X, Cmd+Q and a log-off (on_app_event,
   main.c). Save takes the same road as Ctrl+S, the disk check and the dialog for a file with no
   path included, and the close goes on only once the file is safe. */
static inline void unsaved_dialog(AppState *st) {
    const Palette *t = theme();
    bool           save = false, discard = false, cancel = false;

    if (!st->closeOpen)
        return;
    if (rcBeginModal("unsaved", &st->closeOpen)) {
        Doc *d = st->closeWhy == CLOSE_APP ? NULL : doc_by_id(st, st->closeId);

        rcColumn(.gap = 6, .wType = RC_PX(dialog_w(380.0f))) {
            rcText(d ? rcFormat(st->arena, "Do you want to save the changes you made to %s?",
                                d->name)
                     : rcFormat(st->arena, "Do you want to save the changes to %d files?",
                                dirty_count(st)),
                   .color = t->bright, .size = SZ_HEAD, .select = RC_SELECT_NONE);
            label("Your changes will be lost if you don't save them.", SZ_UI, t->muted);
        }
        rcRow(.gap = 8, .pt = 8, .align = "cr", .w = "grow") {
            save    = rcButton("unsaved_save", "Save", .variant = RC_BTN_PRIMARY);
            discard = rcButton("unsaved_drop", "Don't Save");
            cancel  = rcButton("unsaved_cancel", "Cancel");
        }
        rcEndModal();
    }
    if (save || discard) {
        st->closeOpen = false;
        post_command(st, save ? CMD_CLOSE_SAVE : CMD_CLOSE_DROP);
    } else if (cancel || !st->closeOpen) {      /* Esc and the scrim are a Cancel too */
        st->closeOpen = false;
        st->closeWhy  = CLOSE_NONE;
    }
}

static inline void conflict_dialog(AppState *st) {
    const Palette *t = theme();
    const Doc     *d = doc_by_id(st, st->conflictId);
    bool           over = false, reload = false, cancel = false;

    if (!st->conflictOpen || !d)
        return;
    if (rcBeginModal("conflict", &st->conflictOpen)) {
        rcColumn(.gap = 6, .wType = RC_PX(dialog_w(420.0f))) {
            rcText(rcFormat(st->arena, "%s changed on disk since you opened it.", d->name),
                   .color = t->bright, .size = SZ_HEAD, .select = RC_SELECT_NONE);
            label("Overwrite it with this buffer, or reload the file and lose your edits?", SZ_UI,
                  t->muted);
        }
        rcRow(.gap = 8, .pt = 8, .align = "cr", .w = "grow") {
            over   = rcButton("conflict_over", "Overwrite", .variant = RC_BTN_DANGER);
            reload = rcButton("conflict_reload", "Reload");
            cancel = rcButton("conflict_cancel", "Cancel");
        }
        rcEndModal();
    }
    if (over || reload) {
        st->conflictOpen = false;
        post_command(st, over ? CMD_CONFLICT_OVERWRITE : CMD_CONFLICT_RELOAD);
    } else if (cancel || !st->conflictOpen) {
        st->conflictOpen = false;
        st->closeWhy     = CLOSE_NONE;
    }
}

/* A failed save says so where it cannot be missed: the panel may be folded, or on a phone. */
static inline void alert_dialog(AppState *st) {
    const Palette *t = theme();

    if (rcBeginModal("alert", &st->alertOpen)) {
        bool done = false;

        rcColumn(.gap = 6, .wType = RC_PX(dialog_w(420.0f))) {
            label("The file was not saved", SZ_HEAD, t->bright);
            rcTextC(st->alert, .color = t->muted, .size = SZ_UI, .select = RC_SELECT_NONE);
        }
        rcRow(.pt = 8, .align = "cr", .w = "grow") {
            done = rcButton("alert_ok", "OK", .variant = RC_BTN_PRIMARY);
        }
        rcEndModal();
        if (done)
            st->alertOpen = false;
    }
}

static inline void about_dialog(AppState *st) {
    const Palette *t = theme();

    if (rcBeginModal("about", &st->aboutOpen)) {
        bool done = false;

        rcRow(.gap = 16, .align = "cl", .wType = RC_PX(dialog_w(420.0f))) {
            rcIconRayClayLogo(56.0f);
            rcColumn(.gap = 4, .w = "grow") {
                label("RayClay IDE", SZ_HEAD, t->bright);
                rcText(rcFormat(st->arena,
                                "RayClay %s. One C source for desktop, mobile, and the web.",
                                RC_VERSION),
                       .color = t->text, .size = SZ_UI, .select = RC_SELECT_NONE);
                rcText(rcFormat(st->arena, "%d files in %s, %d open.", st->ws.files,
                                st->ws.rootName, st->nDocs),
                       .color = t->muted, .size = SZ_SMALL, .select = RC_SELECT_NONE);
            }
        }
        rcRow(.pt = 8, .align = "cr", .w = "grow") {
            done = rcButton("about_ok", "Close", .variant = RC_BTN_PRIMARY);
        }
        rcEndModal();
        if (done)
            st->aboutOpen = false;
    }
}

/* Settings hold what a viewer would look for, each a real control over real state. */
static inline void settings_row(const char *name, const char *note, bool *value, const char *id) {
    const Palette *t = theme();

    rcRow(.gap = 12, .align = "cl", .w = "grow") {
        rcColumn(.gap = 2, .w = "grow") {
            label(name, SZ_UI, t->bright);
            label(note, SZ_SMALL, t->muted);
        }
        rcToggle(id, "", value);
    }
}

static inline void settings_dialog(AppState *st) {
    const Palette *t = theme();

    if (rcBeginModal("settings", &st->settingsOpen)) {
        bool done = false, light = !st->dark;

        rcColumn(.gap = 16, .wType = RC_PX(dialog_w(420.0f))) {
            label("Settings", SZ_HEAD, t->bright);
            settings_row("Light theme", "Light+ colors for the chrome and the code", &light,
                         "set_light");
            settings_row("Minimap", "The whole file at a glance, beside the code", &st->minimapOn,
                         "set_mini");
            settings_row("Outline", "The file's symbols under the explorer", &st->outlineOpen,
                         "set_outline");
            settings_row("SVG preview", "Draw an open .svg beside its code", &st->previewOn,
                         "set_preview");
            rcRow(.gap = 8, .pt = 4, .align = "cr", .w = "grow") {
                done = rcButton("settings_ok", "Done", .variant = RC_BTN_PRIMARY);
            }
        }
        rcEndModal();
        if (light == st->dark)
            post_command(st, CMD_THEME);
        if (done)
            st->settingsOpen = false;
    }
}

/* ---- The context menu of the code. ---- */

static inline void code_menu(AppState *st, Doc *d) {
    if (rcBeginContextMenu("code_menu", d->scrollId)) {
        st->menuOpen = true;
        if (rcMenuItem("Cut"))
            post_command(st, CMD_CUT);
        if (rcMenuItem("Copy"))
            post_command(st, CMD_COPY);
        if (rcMenuItem("Paste"))
            post_command(st, CMD_PASTE);
        if (rcMenuItem("Select All"))
            post_command(st, CMD_SELECT_ALL);
        if (rcMenuItem("Toggle Line Comment", .disabled = !comment_mark(d)))
            post_command(st, CMD_COMMENT);
        if (rcMenuItem("Go to Symbol..."))
            post_command(st, CMD_GOTO_SYMBOL);
        rcEndContextMenu();
    }
}

/* ---- The code area: tabs, breadcrumb, gutter, code, minimap, preview. ---- */

static inline void code_pane(AppState *st, Doc *d, float paneWidth) {
    const Palette *t  = theme();
    RC_ScrollInfo  si = rcGetScrollInfo(d->scrollId);
    float          gutter = gutter_width(st, d);
    bool           svg = ends_with(d->name, ".svg") && st->previewOn;
    bool           mini = st->minimapOn && paneWidth >= MINIMAP_MIN_W && !st->compact
                          && !st->zen && !svg;
    float          codeW = (float)d->longest * st->cell + (float)CODE_PAD * 2.0f + 16.0f;
    int            extra = view_lines(d) - 2;

    rcStrCopy(st->paneId, d->scrollId, sizeof st->paneId);
    /* A phone shows the preview or the code, with a toggle above them. */
    if (st->compact && ends_with(d->name, ".svg")) {
        rcRow(.bg = t->sidebar, .gap = 4, .p = 6, .align = "cl", .w = "grow") {
            if (segment("svg_code", "Code", !st->previewOn, NULL))
                st->previewOn = false;
            if (segment("svg_preview", "Preview", st->previewOn, NULL))
                st->previewOn = true;
        }
    }
    if (svg && st->compact) {
        st->paneId[0] = '\0';     /* no code view on show: no field for the keyboard */
        svg_preview(st, d);
    } else {
        rcRow(.id = PANE_ID, .bg = t->editor, .w = "grow", .h = "grow") {
            gutter_view(st, d, gutter, si.offsetY);
            rcColumn(.id = d->scrollId, .bg = t->editor, .scroll = "b", .w = "grow", .h = "grow",
                     .className = "min-w-0", .scrollOffset = { d->scrollX, d->scrollY }) {
                /* Rows past the last line let the file scroll up off the top of the view, the
                   way its leader's does, and every row is the longest line's width, so the
                   sideways range holds still as rows come and go. */
                rcVirtualList(row, d->scrollId, d->lines + (extra > 0 ? extra : 0), LINE_H,
                              .overscan = 3) {
                    code_row(st, d, row.index, codeW);
                }
            }
            if (mini)
                minimap_view(st, d, si.offsetY, si.maxOffsetY);
            else
                st->miniDrag = false;
            rcScrollbar(d->scrollId);   /* after the minimap's rcScrollBy: the bar reads it */
            if (svg) {
                rcBox(.bg = t->edge, .h = "grow", .wType = RC_PX(1)) {}
                svg_preview(st, d);
            }
        }
        si = rcGetScrollInfo(d->scrollId);
        d->scrollX = si.offsetX;
        d->scrollY = si.offsetY;
        code_menu(st, d);
        if (st->find.open) {
            float w = paneWidth - 32.0f;

            find_bar(st, w < 440.0f ? w : 440.0f);
        }
    }
}

static inline void notice_view(const Doc *d) {
    const Palette *t = theme();

    rcColumn(.bg = t->editor, .gap = 6, .align = "cc", .w = "grow", .h = "grow") {
        label(d->name, SZ_HEAD, t->text);
        label(d->notice, SZ_UI, t->muted);
    }
}

static inline void editor_area(AppState *st, float width) {
    Doc *d = active_doc(st);

    /* min-w-0, as a web IDE's: a long line scrolls the code, and never widens the pane. */
    rcColumn(.bg = theme()->editor, .w = "grow", .h = "grow", .className = "min-w-0") {
        if (st->nDocs && !st->zen)
            tab_strip(st);
        d = active_doc(st);
        if (!d) {
            if (st->started)        /* the first frame keeps the editor empty */
                welcome_view(st);
        } else {
            if (!st->zen)
                breadcrumb(d, st->compact);
            if (d->unreadable)
                notice_view(d);
            else
                code_pane(st, d, width);
        }
    }
}

static inline void sidebar(AppState *st) {
    rcColumn(.bg = theme()->sidebar, .w = "grow", .h = "grow") {
        if (st->sideView == SIDE_EXPLORER)
            explorer_view(st);
        else
            search_view(st);
    }
}

/* The wide shell: rail, sidebar, code over panel. Splits are the library's: drag a divider. */
static inline void wide_shell(AppState *st, float width) {
    float side = st->sideOpen ? st->sideFrac * (width - (float)ACTIVITY_W) : 0.0f;

    rcRow(.w = "grow", .h = "grow") {
        activity_rail(st);
        /* What the rail leaves is one column. A split takes its width from a column around it
           (min-w-0: not from what the panes hold); straight in this row it would be as wide as
           its longest line of code, and push the panel's controls off the window. */
        rcColumn(.w = "grow", .h = "grow", .className = "min-w-0") {
            if (st->sideOpen) {
                if (rcBeginSplitPane("side_split", RC_SPLIT_ROW, &st->sideFrac,
                                     .minFraction = 0.12f, .maxFraction = 0.5f,
                                     .handleThickness = 3.0f)) {
                    sidebar(st);
                    rcSplitHandle();
                    /* min-w-0, as a web IDE's editor pane: the split sets its width, not its
                       tabs. */
                    rcColumn(.w = "grow", .h = "grow", .className = "min-w-0") {
                        if (st->panelOpen) {
                            if (rcBeginSplitPane("panel_split", RC_SPLIT_COLUMN, &st->panelFrac,
                                                 .minFraction = 0.25f, .maxFraction = 0.9f,
                                                 .handleThickness = 3.0f)) {
                                editor_area(st, width - (float)ACTIVITY_W - side - 3.0f);
                                rcSplitHandle();
                                bottom_panel(st);
                                rcEndSplitPane();
                            }
                        } else {
                            editor_area(st, width - (float)ACTIVITY_W - side - 3.0f);
                        }
                    }
                    rcEndSplitPane();
                }
            } else if (st->panelOpen) {
                if (rcBeginSplitPane("panel_split", RC_SPLIT_COLUMN, &st->panelFrac,
                                     .minFraction = 0.25f, .maxFraction = 0.9f,
                                     .handleThickness = 3.0f)) {
                    editor_area(st, width - (float)ACTIVITY_W);
                    rcSplitHandle();
                    bottom_panel(st);
                    rcEndSplitPane();
                }
            } else {
                editor_area(st, width - (float)ACTIVITY_W);
            }
        }
    }
}

/* The compact shell: one screen at a time, chosen from the bottom. */
static inline void compact_shell(AppState *st, float width, float safeBottom, bool typing) {
    rcColumn(.w = "grow", .h = "grow") {
        if (st->screen == SCREEN_FILES)
            explorer_view(st);
        else if (st->screen == SCREEN_SEARCH)
            search_view(st);
        else if (st->screen == SCREEN_PROBLEMS || st->screen == SCREEN_OUTPUT)
            bottom_panel(st);
        else
            editor_area(st, width);
    }
    if (!typing) {          /* the keyboard is up: every line it leaves goes to the code */
        status_bar(st);
        bottom_nav(st, safeBottom);
    }
}

/* The title bar's height eases toward open or the rail. It is a frame request away from the
   next step, and rcWindowSetTitlebarHeight keeps the OS's drag strip equal to what is drawn. */
static inline void ease_bar(AppState *st) {
    float target = st->zen ? (float)BAR_RAIL_H : (float)BAR_H;
    float step   = (target - st->barH) * 0.28f;

    if (st->barH == target)
        return;
    st->barH = step > -0.5f && step < 0.5f ? target : st->barH + step;
    rcWindowSetTitlebarHeight(rcAppMainWindow(st->app), (int)st->barH);
    rcWindowRequestFrame(rcAppMainWindow(st->app));
}

/* ---- The frame. ---- */

/* THE WINDOW FIRST, as a web page paints before its data arrives: the first frame draws the
   shell around an empty editor, and this, before the second frame's layout, reads the folder,
   the files and the code font, so that frame shows them. */
static inline void update(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;

    (void)app;
    if (st->shown && !st->started)
        app_start(st);
}

static inline void layout(RC_App *app, void *userData) {
    AppState   *st = (AppState *)userData;
    RC_Viewport vp = rcViewport();
    RC_Insets   safe = vp.safe;
    float       keyboard = vp.keyboard.bottom;   /* layout units, as the root's padding */
    float       width = vp.width - safe.left - safe.right;
    float       zoom = rcWindowZoom(rcAppMainWindow(app));
    bool        compact = width < COMPACT_W || (rcPointerIsCoarse() && vp.height < COMPACT_H);
    int         i;
    Doc        *d;

    st->app   = app;
    st->arena = rcAppArena(app);
    st->now   = rcAppTime(app);
    /* A list's rows change height with the shape of the window: its place is found again. */
    if (compact != st->compact) {
        st->shownDoc[0] = '\0';
        st->outlineHere = NULL;
    }
    st->compact = compact;
    st->paneId[0] = '\0';
    /* A window dragged short gives its height to the code; drawn tall again, it gets it back.
       The panel and the outline stay yours to toggle in between. */
    if ((vp.height < PANEL_MIN_H) != st->wasShort) {
        st->wasShort    = vp.height < PANEL_MIN_H;
        st->panelOpen   = !st->wasShort;
        st->outlineOpen = !st->wasShort;
    }
    if (!st->shown) {
        st->shown = true;
        rcWindowRequestFrame(rcAppMainWindow(app));     /* the frame whose update loads */
    }
    poll_requests(st);
    for (i = 0; i < st->nQueued; i++)
        run_command(st, st->queued[i]);
    st->nQueued = 0;
    if (!st->pal.open && !st->menuOpen && !rcIsModalOpen())
        shortcut_keys(st);
    for (i = 0; i < st->nDocs; i++) {
        Doc *x = &st->docs[i];

        if (x->atLimit) {       /* a limit the user reached says so (state.h) */
            out_log(st, SEV_WARNING, rcFormat(st->arena, "%s would pass %d MB: that edit was "
                                              "not made", x->name, DOC_BYTES >> 20).chars);
            x->atLimit = false;
        }
        doc_analyze(x, st->arena);
    }
    d = active_doc(st);
    follow_active_tab(st, d);
    escape_key(st, d);
    if (d && !d->unreadable)
        editor_input(st, d);
    ease_bar(st);
    st->menuOpen = false;       /* the frame's popups say so again as they are declared */

    rcColumn(.id = "root", .bg = theme()->editor, .pt = safe.top,
             .pb = (float)(keyboard > 0.0f ? keyboard : st->compact ? 0.0f : safe.bottom),
             .pl = safe.left, .pr = safe.right, .w = "grow", .h = "grow") {
        /* The bar is drawn unzoomed, so its room is the width in the bar's own px. */
        title_bar(st, !st->compact && width * zoom >= COMPACT_W);
        if (st->zen) {
            /* Zen keeps a reading measure: the row centers a column capped at it. */
            rcRow(.bg = theme()->editor, .align = "tc", .w = "grow", .h = "grow") {
                rcColumn(.w = "grow", .h = "grow", .wMax = RC_VAL(ZEN_W), .className = "min-w-0") {
                    editor_area(st, width < ZEN_W ? width : ZEN_W);
                }
            }
        } else if (st->compact) {
            compact_shell(st, width, safe.bottom, keyboard > 0.0f);
        } else {
            wide_shell(st, width);
            status_bar(st);
        }
    }
    palette_view(st, width - 32.0f < 620.0f ? width - 32.0f : 620.0f);
    unsaved_dialog(st);
    conflict_dialog(st);
    alert_dialog(st);
    d = active_doc(st);
    about_dialog(st);
    settings_dialog(st);
    /* Only over a code view this frame declared: a file opened from the palette a moment ago
       draws from the next frame. */
    if (d && same_text(st->paneId, d->scrollId))
        caret_field(st, d);
    settle_focus(st, d);
}

#endif /* APP_APP_H */
