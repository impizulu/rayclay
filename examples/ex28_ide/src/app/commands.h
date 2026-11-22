/*
    commands.h - everything the app can do, as one table

    A menu item, a palette row and a shortcut all run the same Command, and a shortcut hint is
    read from the same row that binds it, so the three can never disagree. Adding a command is
    one line here and one case in run_command.

    A BUTTON POSTS, THE TOP OF THE FRAME RUNS. Text runs borrow the document's bytes until the
    frame is drawn, so a click cannot edit or free them while the UI is being declared. A click
    posts its command, and the next frame, which post_command asks for at once, runs it first.
*/
#ifndef APP_COMMANDS_H
#define APP_COMMANDS_H

#include "rayclay.h"
#include "app/editor.h"
#include "platform/platform.h"

typedef enum {
    CMD_NEW_FILE, CMD_OPEN_FILE, CMD_OPEN_FOLDER, CMD_SAVE, CMD_SAVE_AS, CMD_REVERT,
    CMD_CLOSE_TAB, CMD_CLOSE_ALL, CMD_QUIT,
    CMD_UNDO, CMD_REDO, CMD_CUT, CMD_COPY, CMD_PASTE, CMD_SELECT_ALL, CMD_FIND, CMD_REPLACE,
    CMD_FIND_NEXT, CMD_FIND_PREV, CMD_COMMENT, CMD_DELETE_LINE, CMD_DUPLICATE_LINE,
    CMD_MOVE_UP, CMD_MOVE_DOWN, CMD_INDENT, CMD_OUTDENT, CMD_SELECT_LINE,
    CMD_PALETTE, CMD_EXPLORER, CMD_SEARCH, CMD_TOGGLE_SIDEBAR, CMD_PROBLEMS, CMD_OUTPUT,
    CMD_TOGGLE_PANEL, CMD_MINIMAP, CMD_ZEN, CMD_THEME, CMD_PREVIEW, CMD_SETTINGS,
    CMD_QUICK_OPEN, CMD_GOTO_LINE, CMD_GOTO_SYMBOL, CMD_NEXT_TAB, CMD_PREV_TAB,
    CMD_NEXT_PROBLEM, CMD_PREV_PROBLEM, CMD_ABOUT,
    /* These have no title: they act on what a button or a dialog said, not from the palette. */
    CMD_REPLACE_ONE, CMD_REPLACE_ALL, CMD_CLOSE_INDEX, CMD_CLOSE_OTHERS, CMD_CLOSE_SAVE,
    CMD_CLOSE_DROP, CMD_CONFLICT_OVERWRITE, CMD_CONFLICT_RELOAD, CMD_REFRESH, CMD_OPEN_NODE,
    CMD_COUNT
} Command;

/* What must be held. FIELD: the text field under the keyboard does it, so the row only names
   it. EDITOR: the document's own, and a field with the keyboard keeps the key for itself. */
enum { M_PRIMARY = 1, M_SHIFT = 2, M_ALT = 4, M_CHORD = 8, M_FIELD = 16, M_EDITOR = 32 };

typedef struct {
    const char *title;
    RC_Key      key;
    uint8_t     mods;
} CommandInfo;

static const CommandInfo COMMANDS[CMD_COUNT] = {
    [CMD_NEW_FILE]       = { "File: New File", RC_KEY_N, M_PRIMARY },
    [CMD_OPEN_FILE]      = { "File: Open File...", RC_KEY_O, M_PRIMARY },
    [CMD_OPEN_FOLDER]    = { "File: Open Folder...", RC_KEY_NONE, 0 },
    [CMD_SAVE]           = { "File: Save", RC_KEY_S, M_PRIMARY },
    [CMD_SAVE_AS]        = { "File: Save As...", RC_KEY_S, M_PRIMARY | M_SHIFT },
    [CMD_REVERT]         = { "File: Revert File", RC_KEY_NONE, 0 },
    [CMD_CLOSE_TAB]      = { "File: Close Editor", RC_KEY_W, M_PRIMARY },
    [CMD_CLOSE_ALL]      = { "File: Close All Editors", RC_KEY_NONE, 0 },
    [CMD_QUIT]           = { "File: Quit", RC_KEY_Q, M_PRIMARY },
    [CMD_UNDO]           = { "Edit: Undo", RC_KEY_Z, M_PRIMARY | M_EDITOR },
    [CMD_REDO]           = { "Edit: Redo", RC_KEY_Y, M_PRIMARY | M_EDITOR },
    [CMD_CUT]            = { "Edit: Cut", RC_KEY_X, M_PRIMARY | M_EDITOR },
    [CMD_COPY]           = { "Edit: Copy", RC_KEY_C, M_PRIMARY | M_EDITOR },
    [CMD_PASTE]          = { "Edit: Paste", RC_KEY_V, M_PRIMARY | M_FIELD },
    [CMD_SELECT_ALL]     = { "Edit: Select All", RC_KEY_A, M_PRIMARY | M_EDITOR },
    [CMD_FIND]           = { "Edit: Find", RC_KEY_F, M_PRIMARY },
    [CMD_REPLACE]        = { "Edit: Replace", RC_KEY_H, M_PRIMARY },
    [CMD_FIND_NEXT]      = { "Edit: Find Next", RC_KEY_F3, 0 },
    [CMD_FIND_PREV]      = { "Edit: Find Previous", RC_KEY_F3, M_SHIFT },
    [CMD_COMMENT]        = { "Edit: Toggle Line Comment", RC_KEY_SLASH, M_PRIMARY | M_EDITOR },
    [CMD_DELETE_LINE]    = { "Edit: Delete Line", RC_KEY_K, M_PRIMARY | M_SHIFT | M_EDITOR },
    [CMD_DUPLICATE_LINE] = { "Edit: Duplicate Line", RC_KEY_DOWN, M_SHIFT | M_ALT | M_EDITOR },
    [CMD_MOVE_UP]        = { "Edit: Move Line Up", RC_KEY_UP, M_ALT | M_EDITOR },
    [CMD_MOVE_DOWN]      = { "Edit: Move Line Down", RC_KEY_DOWN, M_ALT | M_EDITOR },
    [CMD_INDENT]         = { "Edit: Indent Line", RC_KEY_RIGHT_BRACKET, M_PRIMARY | M_EDITOR },
    [CMD_OUTDENT]        = { "Edit: Outdent Line", RC_KEY_LEFT_BRACKET, M_PRIMARY | M_EDITOR },
    [CMD_SELECT_LINE]    = { "Edit: Select Line", RC_KEY_L, M_PRIMARY | M_EDITOR },
    [CMD_PALETTE]        = { "View: Command Palette", RC_KEY_P, M_PRIMARY | M_SHIFT },
    [CMD_EXPLORER]       = { "View: Explorer", RC_KEY_E, M_PRIMARY | M_SHIFT },
    [CMD_SEARCH]         = { "View: Search", RC_KEY_F, M_PRIMARY | M_SHIFT },
    [CMD_TOGGLE_SIDEBAR] = { "View: Toggle Sidebar", RC_KEY_B, M_PRIMARY },
    [CMD_PROBLEMS]       = { "View: Problems", RC_KEY_M, M_PRIMARY | M_SHIFT },
    [CMD_OUTPUT]         = { "View: Output", RC_KEY_U, M_PRIMARY | M_SHIFT },
    [CMD_TOGGLE_PANEL]   = { "View: Toggle Panel", RC_KEY_J, M_PRIMARY },
    [CMD_MINIMAP]        = { "View: Toggle Minimap", RC_KEY_NONE, 0 },
    [CMD_ZEN]            = { "View: Toggle Zen Mode", RC_KEY_Z, M_PRIMARY | M_CHORD },
    [CMD_THEME]          = { "View: Toggle Light Theme", RC_KEY_NONE, 0 },
    [CMD_PREVIEW]        = { "View: Toggle SVG Preview", RC_KEY_NONE, 0 },
    [CMD_SETTINGS]       = { "File: Settings", RC_KEY_COMMA, M_PRIMARY },
    [CMD_QUICK_OPEN]     = { "Go: Go to File...", RC_KEY_P, M_PRIMARY },
    [CMD_GOTO_LINE]      = { "Go: Go to Line...", RC_KEY_G, M_PRIMARY },
    [CMD_GOTO_SYMBOL]    = { "Go: Go to Symbol...", RC_KEY_O, M_PRIMARY | M_SHIFT },
    [CMD_NEXT_TAB]       = { "Go: Next Editor", RC_KEY_PAGE_DOWN, M_PRIMARY },
    [CMD_PREV_TAB]       = { "Go: Previous Editor", RC_KEY_PAGE_UP, M_PRIMARY },
    [CMD_NEXT_PROBLEM]   = { "Go: Next Problem", RC_KEY_F8, 0 },
    [CMD_PREV_PROBLEM]   = { "Go: Previous Problem", RC_KEY_F8, M_SHIFT },
    [CMD_ABOUT]          = { "Help: About", RC_KEY_NONE, 0 },
};

/* A row in a menu is the title without its "File: " head. */
static inline const char *command_label(Command c) {
    const char *s = COMMANDS[c].title;

    while (*s && *s != ':')
        s++;
    return *s ? s + 2 : COMMANDS[c].title;
}

static inline const char *key_name(RC_Key k, char *one) {
    switch (k) {
    case RC_KEY_F3:            return "F3";
    case RC_KEY_F8:            return "F8";
    case RC_KEY_UP:            return "Up";
    case RC_KEY_DOWN:          return "Down";
    case RC_KEY_PAGE_UP:       return "PgUp";
    case RC_KEY_PAGE_DOWN:     return "PgDn";
    case RC_KEY_SLASH:         return "/";
    case RC_KEY_COMMA:         return ",";
    case RC_KEY_LEFT_BRACKET:  return "[";
    case RC_KEY_RIGHT_BRACKET: return "]";
    default:
        one[0] = (char)('A' + ((int)k - (int)RC_KEY_A));
        one[1] = '\0';
        return one;
    }
}

/* The hint a menu and the palette show. "" for a command with no key. */
static inline const char *command_keys(RC_Arena *a, Command c) {
    const CommandInfo *info = &COMMANDS[c];
    char               one[2];

    if (info->key == RC_KEY_NONE)
        return "";
    return rcFormat(a, "%s%s%s%s%s", info->mods & M_PRIMARY ? PRIMARY_KEY "+" : "",
                    info->mods & M_SHIFT ? "Shift+" : "", info->mods & M_ALT ? "Alt+" : "",
                    info->mods & M_CHORD ? "K " : "", key_name(info->key, one)).chars;
}

/* ---- Tabs. ---- */

static inline void remove_tab(AppState *st, int index) {
    int i;

    if (st->docs[index].imported)
        rcReleaseFile(st->docs[index].path);
    doc_release(&st->docs[index]);
    for (i = index + 1; i < st->nDocs; i++)
        st->docs[i - 1] = st->docs[i];
    st->nDocs--;
    if (st->active > index || st->active >= st->nDocs)
        st->active--;
    if (!st->nDocs)
        st->active = -1;
    st->find.seenDoc = -1;
    st->pal.seen[0] = 1;        /* symbol picks held this tab's index: the palette lists again */
    st->pal.seen[1] = '\0';
}

/* Closing a file with unsaved changes always asks: nothing is lost or written unasked. */
static inline void ask_about_tab(AppState *st, int why, int index) {
    st->closeWhy  = why;
    st->closeOpen = true;
    rcStrCopy(st->closeId, st->docs[index].scrollId, sizeof st->closeId);
}

static inline void close_tab(AppState *st, int index) {
    if (doc_dirty(&st->docs[index]))
        ask_about_tab(st, CLOSE_TAB, index);
    else
        remove_tab(st, index);
}

/* Close All, from the last tab: clean tabs go, and a dirty one stops the walk to ask. */
static inline void close_all_step(AppState *st) {
    while (st->nDocs && !doc_dirty(&st->docs[st->nDocs - 1]))
        remove_tab(st, st->nDocs - 1);
    if (st->nDocs)
        ask_about_tab(st, CLOSE_ALL, st->nDocs - 1);
    else
        st->closeWhy = CLOSE_NONE;
}

static inline int dirty_count(const AppState *st) {
    int i, n = 0;

    for (i = 0; i < st->nDocs; i++)
        n += doc_dirty(&st->docs[i]);
    return n;
}

/* ---- Saving. ---- */

enum { SAVE_DONE, SAVE_ASKING, SAVE_FAILED };

/* The bytes go to the user through a save dialog. rcFileDialogSave never says where they went
   (it has no path to give), so the buffer stays what it was, under its own name. One save at a
   time: a second, refused while the first is up, would take the token whose answer the first
   one's buffer and a waiting close still need. A save always answers, so the wait ends. */
static inline void hand_over(AppState *st, Doc *d) {
    if (st->askSaveAs)
        return;
    rcStrCopy(st->saveId, d->scrollId, sizeof st->saveId);
    st->saveTop   = d->top;
    st->askSaveAs = rcFileDialogSave(d->name, d->text, (size_t)d->len);
}

/* An explicit Save, and the one road to rcSaveFile that does not first ask. A buffer with no
   file behind it (a new file, a sample) has nowhere to write, so its bytes are handed over. */
static inline int save_doc(AppState *st, Doc *d) {
    if (d->unreadable)
        return SAVE_DONE;
    if (!d->path[0] || d->memoryOnly) {
        hand_over(st, d);
        return SAVE_ASKING;
    }
    if (disk_changed(d)) {
        /* Not saveId: a save dialog still up for another tab answers to that. */
        rcStrCopy(st->conflictId, d->scrollId, sizeof st->conflictId);
        st->conflictOpen = true;
        rcSetFocus("conflict_cancel");  /* a reflexive Enter neither overwrites nor reloads */
        return SAVE_ASKING;
    }
    return doc_write(st, d) ? SAVE_DONE : SAVE_FAILED;
}

/* The tab a close stopped at is saved, or the user let it go. */
static inline void close_drop_tab(AppState *st) {
    Doc *d = doc_by_id(st, st->closeId);

    if (d)
        remove_tab(st, (int)(d - st->docs));
    if (st->closeWhy == CLOSE_ALL)
        close_all_step(st);
    else
        st->closeWhy = CLOSE_NONE;
}

/* The unsaved-changes dialog's Save, and what a save's answer comes back to. A save that has to
   ask (a dialog, a conflict) stops here until it is answered; one that fails ends the close, with
   the tab still open and the reason on screen. */
static inline void close_save(AppState *st) {
    int r, i;

    if (st->closeWhy == CLOSE_APP) {
        for (i = 0; i < st->nDocs; i++) {
            if (!doc_dirty(&st->docs[i]))
                continue;
            r = save_doc(st, &st->docs[i]);
            if (r == SAVE_ASKING)
                return;
            if (r == SAVE_FAILED) {
                st->closeWhy = CLOSE_NONE;
                return;
            }
        }
        st->closeWhy = CLOSE_NONE;
        rcAppRequestClose(st->app);
    } else {
        Doc *d = doc_by_id(st, st->closeId);

        r = !d || !doc_dirty(d) ? SAVE_DONE : save_doc(st, d);
        if (r == SAVE_DONE)
            close_drop_tab(st);
        else if (r == SAVE_FAILED)
            st->closeWhy = CLOSE_NONE;
    }
}

static inline void new_file(AppState *st) {
    Doc *d = doc_new(st, rcFormat(st->arena, "untitled-%u.c", ++st->untitled).chars, "");

    if (d) {
        doc_set_text(d, "", 0);
        st->wantCaret = true;
    }
}

/* ---- The palette and the panels, opened. ---- */

static inline void open_palette(AppState *st, const char *prefix) {
    QuickOpen *p = &st->pal;

    p->open = true;
    rcStrCopy(p->query, prefix, sizeof p->query);
    p->seen[0] = 1;             /* no query equals this, so the next frame lists */
    p->seen[1] = '\0';
    p->sel = 0;
}

static inline void show_panel(AppState *st, int tab) {
    st->panelTab  = tab;
    st->panelOpen = true;
    st->screen    = tab == PANEL_PROBLEMS ? SCREEN_PROBLEMS : SCREEN_OUTPUT;
}

static inline void show_side(AppState *st, int view) {
    st->sideView = view;
    st->sideOpen = true;
    st->screen   = view == SIDE_EXPLORER ? SCREEN_FILES : SCREEN_SEARCH;
}

/* Open the find bar, seeded with a one-line selection. */
static inline void open_find(AppState *st, Doc *d, bool replace) {
    Find *f = &st->find;

    f->open = true;
    f->replaceOpen = replace;
    f->focusQuery  = true;
    if (d && has_sel(d) && sel_end(d) - sel_start(d) < QUERY_BYTES
        && doc_line_of(d, sel_start(d)) == doc_line_of(d, sel_end(d))) {
        int n = sel_end(d) - sel_start(d);

        move_bytes(f->query, d->text + sel_start(d), n);
        f->query[n] = '\0';
    }
}

/* F8: the next problem in this file, wrapping. */
static inline void step_problem(AppState *st, Doc *d, int dir) {
    int ln = doc_line_of(d, d->caret), best = -1, i;

    for (i = 0; i < d->nProb; i++) {
        int k = dir > 0 ? i : d->nProb - 1 - i;

        if (dir > 0 ? d->prob[k].line > ln : d->prob[k].line < ln) {
            best = k;
            i = d->nProb;
        }
    }
    if (best < 0 && d->nProb)
        best = dir > 0 ? 0 : d->nProb - 1;
    if (best >= 0)
        go_to(st, d, d->prob[best].line, d->prob[best].col);
}

static inline bool command_enabled(const AppState *st, Command c) {
    bool editable = st->active >= 0 && !st->docs[st->active].unreadable;

    switch (c) {
    case CMD_SAVE: case CMD_SAVE_AS: case CMD_UNDO: case CMD_REDO: case CMD_CUT: case CMD_COPY:
    case CMD_PASTE: case CMD_SELECT_ALL: case CMD_FIND: case CMD_REPLACE: case CMD_FIND_NEXT:
    case CMD_FIND_PREV: case CMD_COMMENT: case CMD_DELETE_LINE: case CMD_DUPLICATE_LINE:
    case CMD_MOVE_UP: case CMD_MOVE_DOWN: case CMD_INDENT: case CMD_OUTDENT: case CMD_SELECT_LINE:
    case CMD_GOTO_LINE: case CMD_GOTO_SYMBOL: case CMD_NEXT_PROBLEM: case CMD_PREV_PROBLEM:
    case CMD_REPLACE_ONE: case CMD_REPLACE_ALL:
        return editable;
    case CMD_REVERT:
        return editable && (st->docs[st->active].path[0] != '\0');
    case CMD_CLOSE_TAB: case CMD_CLOSE_ALL: case CMD_NEXT_TAB: case CMD_PREV_TAB:
        return st->active >= 0;
    case CMD_PREVIEW:
        return st->active >= 0 && ends_with(st->docs[st->active].name, ".svg");
    case CMD_TOGGLE_SIDEBAR: case CMD_TOGGLE_PANEL: case CMD_MINIMAP:
        return !st->compact;
    default:
        return true;
    }
}

static inline void post_command(AppState *st, Command c) {
    if (st->nQueued < COUNT_OF(st->queued))
        st->queued[st->nQueued++] = (int)c;
    rcWindowRequestFrame(rcAppMainWindow(st->app));
}

/* While a file or folder dialog is up, a desktop or a phone refuses another at once, and the
   first one's answer is still to come; on the web the new one goes ahead and the first is
   answered CANCELED. So a held request gives way only to one that was not refused. */
static inline void ask_dialog(RC_FileDialogToken *held, RC_FileDialogKind kind) {
    RC_FileDialogToken asked = rcFileDialogOpen(kind, NULL);

    if (!*held || rcFileDialogPoll(asked).status != RC_FILE_DIALOG_REFUSED)
        *held = asked;
}

/* A results or palette row's file, opened at the top of the frame after its click: reading it
   again frees the bytes the result rows declared in the click's frame still draw from. */
static inline void open_posted(AppState *st) {
    Doc *d = st->openNode >= 0 && st->openNode < st->ws.count
             ? open_file(st, st->ws.node[st->openNode].path) : NULL;

    if (!d)
        return;
    if (st->openLine >= 0)
        go_to(st, d, st->openLine, st->openCol);
    st->screen = SCREEN_CODE;
}

static inline void run_command(AppState *st, int cmd) {
    Command c = (Command)cmd;
    Doc    *d = active_doc(st);
    Doc    *x;
    int     i;

    if (!command_enabled(st, c))
        return;
    switch (c) {
    case CMD_NEW_FILE:    new_file(st); break;
    case CMD_OPEN_FILE:   ask_dialog(&st->askFile, RC_FILE_DIALOG_FILE); break;
    case CMD_OPEN_FOLDER: ask_dialog(&st->askFolder, RC_FILE_DIALOG_FOLDER); break;
    case CMD_SAVE:        save_doc(st, d); break;
    case CMD_SAVE_AS:     hand_over(st, d); break;
    case CMD_REVERT:      doc_revert(st, d); break;
    case CMD_CLOSE_TAB:   close_tab(st, st->active); break;
    case CMD_CLOSE_ALL:   close_all_step(st); break;
    case CMD_QUIT:
        if (dirty_count(st)) {
            st->closeWhy  = CLOSE_APP;
            st->closeOpen = true;
        } else {
            rcAppRequestClose(st->app);
        }
        break;
    case CMD_UNDO:        doc_undo(d); st->reveal = REVEAL_NEAREST; break;
    case CMD_REDO:        doc_redo(d); st->reveal = REVEAL_NEAREST; break;
    case CMD_CUT:         copy_selection(d, true); st->reveal = REVEAL_NEAREST; break;
    case CMD_COPY:        copy_selection(d, false); break;
    case CMD_PASTE:       st->pasteToken = rcClipboardRequest(); break;
    case CMD_SELECT_ALL:  d->anchor = 0; d->caret = d->len; break;
    case CMD_FIND:        open_find(st, d, false); break;
    case CMD_REPLACE:     open_find(st, d, true); break;
    case CMD_FIND_NEXT:   find_step(st, d, 1); break;
    case CMD_FIND_PREV:   find_step(st, d, -1); break;
    case CMD_COMMENT:     edit_block(d, BLOCK_COMMENT); break;
    case CMD_DELETE_LINE: delete_lines(st, d); break;
    case CMD_DUPLICATE_LINE: duplicate_lines(st, d); break;
    case CMD_MOVE_UP:     move_lines(st, d, -1); break;
    case CMD_MOVE_DOWN:   move_lines(st, d, 1); break;
    case CMD_INDENT:      edit_block(d, BLOCK_INDENT); break;
    case CMD_OUTDENT:     edit_block(d, BLOCK_OUTDENT); break;
    case CMD_SELECT_LINE: {
        int ln = doc_line_of(d, d->caret);

        d->anchor = d->line[ln];
        set_caret(st, d, ln + 1 < d->lines ? d->line[ln + 1] : d->len, true);
        break;
    }
    case CMD_PALETTE:     open_palette(st, ">"); break;
    case CMD_EXPLORER:    show_side(st, SIDE_EXPLORER); break;
    case CMD_SEARCH:
        show_side(st, SIDE_SEARCH);
        st->focusSearch = true;
        break;
    case CMD_TOGGLE_SIDEBAR: st->sideOpen = !st->sideOpen; break;
    case CMD_PROBLEMS:    show_panel(st, PANEL_PROBLEMS); break;
    case CMD_OUTPUT:      show_panel(st, PANEL_OUTPUT); break;
    case CMD_TOGGLE_PANEL: st->panelOpen = !st->panelOpen; break;
    case CMD_MINIMAP:     st->minimapOn = !st->minimapOn; break;
    case CMD_ZEN:         st->zen = !st->zen; break;
    case CMD_THEME:
        st->dark = !st->dark;
        apply_theme(st->dark);
        rcWindowSetClearColor(rcAppMainWindow(st->app), theme()->editor);
        break;
    case CMD_PREVIEW:     st->previewOn = !st->previewOn; break;
    case CMD_SETTINGS:    st->settingsOpen = true; break;
    case CMD_QUICK_OPEN:  open_palette(st, ""); break;
    case CMD_GOTO_LINE:   open_palette(st, ":"); break;
    case CMD_GOTO_SYMBOL: open_palette(st, "@"); break;
    case CMD_NEXT_TAB:    st->active = (st->active + 1) % st->nDocs; break;
    case CMD_PREV_TAB:    st->active = (st->active + st->nDocs - 1) % st->nDocs; break;
    case CMD_NEXT_PROBLEM: step_problem(st, d, 1); break;
    case CMD_PREV_PROBLEM: step_problem(st, d, -1); break;
    case CMD_ABOUT:       st->aboutOpen = true; break;
    case CMD_REPLACE_ONE: replace_current(st, d); break;
    case CMD_REPLACE_ALL: replace_all(st, d); break;
    case CMD_CLOSE_INDEX:
        x = doc_by_id(st, st->closeTab);
        if (x)
            close_tab(st, (int)(x - st->docs));
        break;
    case CMD_REFRESH: {
        char path[PATH_BYTES];

        rcStrCopy(path, st->ws.rootPath, sizeof path);
        workspace_open(st, path);
        break;
    }
    case CMD_OPEN_NODE:   open_posted(st); break;
    case CMD_CLOSE_SAVE:  close_save(st); break;
    case CMD_CLOSE_DROP:
        if (st->closeWhy == CLOSE_APP)
            rcAppRequestClose(st->app);
        else
            close_drop_tab(st);
        break;
    case CMD_CONFLICT_OVERWRITE:
        x = doc_by_id(st, st->conflictId);
        if (x && doc_write(st, x)) {
            if (st->closeWhy != CLOSE_NONE)
                close_save(st);
        } else {
            st->closeWhy = CLOSE_NONE;
        }
        break;
    case CMD_CONFLICT_RELOAD:
        x = doc_by_id(st, st->conflictId);
        if (x)
            doc_revert(st, x);
        st->closeWhy = CLOSE_NONE;      /* the tab stays, showing the file as it is now */
        break;
    case CMD_CLOSE_OTHERS: {
        int keep;

        x    = doc_by_id(st, st->closeTab);
        keep = x ? (int)(x - st->docs) : -1;    /* a tab gone meanwhile keeps every tab */
        for (i = st->nDocs - 1; keep >= 0 && i >= 0; i--)
            if (i != keep && !doc_dirty(&st->docs[i]))
                remove_tab(st, i);
        break;
    }
    default:              break;
    }
}

/* The keys that are only ever held: a chord waits through them. rcKeyEvent lists the frame's
   presses, so any other key, one RC_Key does not name included, ends the wait. */
static inline bool plain_key_pressed(void) {
    RC_KeyEvent e;
    int         i;

    for (i = 0; rcKeyEvent(i, &e); i++)
        if (!e.repeat && (e.key < RC_KEY_LEFT_SHIFT || e.key > RC_KEY_RIGHT_SUPER))
            return true;
    return false;
}

/* A field other than the editor's holds the keyboard, so typing is that field's. */
static inline bool field_has_keys(void) {
    return rcIsFocused("find_q") || rcIsFocused("find_r") || rcIsFocused("search_q")
        || rcIsFocused("problem_filter");
}

/* Read every shortcut from the table. A row needs exactly its modifiers, so Primary+S is not
   also Primary+Shift+S. The two-key Zen chord arms on Primary+K and waits for one more key: a Z
   fires it, and any other key ends the wait and does what it always does. With the primary key
   let go, that Z also types into the field, so its letter is taken back (take_typed). */
static inline void shortcut_keys(AppState *st) {
    bool primary = rcModDown(RC_MOD_PRIMARY), shift = rcModDown(RC_MOD_SHIFT);
    bool alt     = rcModDown(RC_MOD_ALT);
    bool editor  = rcIsFocused(SINK_ID);
    int  i;

    if (st->chordK) {
        if (rcKeyPressed(RC_KEY_Z)) {
            st->chordK = false;
            st->swallowFrames = primary ? 0 : 2;
            run_command(st, CMD_ZEN);
            return;
        }
        if (plain_key_pressed())
            st->chordK = false;
    }
    if (primary && !shift && !alt && rcKeyPressed(RC_KEY_K)) {
        st->chordK = !field_has_keys();     /* its Z would land in that field */
        return;
    }
    if (primary && shift && editor && rcKeyPressed(RC_KEY_Z)) {
        run_command(st, CMD_REDO);
        return;
    }
    if (rcModDown(RC_MOD_CTRL) && rcKeyPressed(RC_KEY_TAB)) {
        run_command(st, shift ? CMD_PREV_TAB : CMD_NEXT_TAB);
        return;
    }
    for (i = 0; i < CMD_COUNT; i++) {
        const CommandInfo *c = &COMMANDS[i];

        if (c->key == RC_KEY_NONE || (c->mods & (M_FIELD | M_CHORD)))
            continue;
        if (!!(c->mods & M_PRIMARY) != primary || !!(c->mods & M_SHIFT) != shift
            || !!(c->mods & M_ALT) != alt)
            continue;
        if ((c->mods & M_EDITOR) && !editor)
            continue;
        if (rcKeyPressed(c->key)) {
            run_command(st, (Command)i);
            return;
        }
    }
}

#endif /* APP_COMMANDS_H */
