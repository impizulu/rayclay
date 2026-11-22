/*
    app.h - the frame

    Reads the keyboard, draws the window from the state in browser.h, and floats the overlays
    above it: the context menu, Quick Look, the delete confirmation, the toast and the drop
    hint. From SIDEBAR_W the sidebar stays open; narrower, it slides in as a drawer. Below
    COMPACT_W the view is a list of tall rows, as on a phone, and search takes its own row.
*/
#ifndef APP_APP_H
#define APP_APP_H

#include "rayclay.h"
#include "app/theme.h"
#include "app/browser.h"
#include "app/parts.h"
#include "app/preview.h"
#include "app/views.h"
#include "app/chrome.h"

/* ---- Keyboard: the Finder's ---- */

/* The next row in the list or the grid. */
static inline void move_cursor(AppState *st, int step, bool extend) {
    int at = row_index(st, st->cursor), to = at < 0 ? 0 : at + step;

    if (!st->rowCount) return;
    if (to < 0) to = 0;
    if (to >= st->rowCount) to = st->rowCount - 1;
    if (extend) select_range(st, st->rows[to]);
    else        select_only(st, st->rows[to]);
}

/* The arrows in the column view walk the tree: left and right change column. */
static inline void move_in_columns(AppState *st, int dx, int dy) {
    int k, i;

    if (!st->colCount) return;
    /* The column holding the cursor: the last one, or, on a folder, the one before it. */
    k = st->cursor == st->folder && st->colCount > 1 ? st->colCount - 2 : st->colCount - 1;
    if (dx < 0) {
        if (k > 0) pick_in_column(st, k - 1, st->cols[k].dir);
        return;
    }
    if (dx > 0) {
        const Column *last = &st->cols[st->colCount - 1];

        if (st->cursor == st->folder && last->count)
            pick_in_column(st, st->colCount - 1, st->rows[last->start]);
        else if (st->cursor >= 0 && node_of(st, st->cursor)->isDir)
            pick_in_column(st, st->colCount - 1, st->cursor);
        return;
    }
    {
        const Column *col = &st->cols[k];
        int           at  = -1;

        for (i = 0; i < col->count; i++)
            if (st->rows[col->start + i] == st->cursor) at = i;
        at += dy;
        if (at < 0) at = 0;
        if (at < col->count) pick_in_column(st, k, st->rows[col->start + at]);
    }
}

/* Type a name and the selection jumps to it, as in the Finder: letters typed within a
   second of each other spell one prefix. */
static inline void type_to_select(RC_App *app, AppState *st) {
    int k, i, len = text_len(st->typed);

    for (k = RC_KEY_A; k <= RC_KEY_Z; k++) {
        if (!rcKeyPressed((RC_Key)k)) continue;
        rcSetFocus(NULL);   /* a typed name is the view's, as an arrow key is */
        if (rcAppTime(app) > st->typedUntil || len + 1 >= (int)sizeof st->typed) len = 0;
        st->typed[len++] = (char)('a' + (k - RC_KEY_A));
        st->typed[len]   = '\0';
        st->typedUntil   = rcAppTime(app) + 1.0;
        for (i = 0; i < st->rowCount; i++) {
            int         id = st->rows[i];
            const char *name = node_of(st, id)->name;
            int         j;

            for (j = 0; j < len && lower(name[j]) == st->typed[j]; j++) {}
            if (j < len) continue;
            if (shown_view(st) != VIEW_COLUMNS) {
                select_only(st, id);
                return;
            }
            if (i >= st->cols[st->colCount - 1].start) {
                pick_in_column(st, st->colCount - 1, id);
                return;
            }
        }
    }
}

static inline void read_keys(RC_App *app, AppState *st, const Shell *s) {
    bool typing  = rcIsFocused("search");
    bool primary = rcModDown(RC_MOD_PRIMARY), shift = rcModDown(RC_MOD_SHIFT);
    int  cols    = shown_view(st) == VIEW_ICONS ? grid_columns(st, s->viewWidth) : 1;

    /* An open menu takes the arrows and Return, and a dialog holds the window. */
    if (st->menuUp || st->infoOpen || st->confirmOpen) return;
    if (primary && rcKeyPressed(RC_KEY_F)) rcSetFocus("search");
    if (typing) return;
    /* The arrows move the selection in every view, so they take the keyboard back to it, as
       a press on a row does: a button Tab left focused would take the Space and Return after
       them, which are Quick Look and rename. A focused slider answers its own arrows, which
       read false here. */
    if (rcKeyPressed(RC_KEY_UP) || rcKeyPressed(RC_KEY_DOWN) || rcKeyPressed(RC_KEY_LEFT)
        || rcKeyPressed(RC_KEY_RIGHT))
        rcSetFocus(NULL);

    /* Quick Look stays up while the arrows flip through the folder; Space, Esc (which the
       modal takes) and its close button close it. */
    if (primary && !st->peek) {
        if (rcKeyPressed(RC_KEY_LEFT_BRACKET))  go_history(st, -1);
        if (rcKeyPressed(RC_KEY_RIGHT_BRACKET)) go_history(st, 1);
        if (rcKeyPressed(RC_KEY_UP))            go_up(st);
        if (rcKeyPressed(RC_KEY_DOWN) && st->cursor >= 0) open_node(st, st->cursor);
        if (rcKeyPressed(RC_KEY_O) && st->cursor >= 0)    open_node(st, st->cursor);
        if (rcKeyPressed(RC_KEY_I))             open_info(st, info_target(st));
        if (rcKeyPressed(RC_KEY_1))             set_view(st, VIEW_ICONS);
        if (rcKeyPressed(RC_KEY_2))             set_view(st, VIEW_LIST);
        if (rcKeyPressed(RC_KEY_3) && !s->compact) set_view(st, VIEW_COLUMNS);
        if (shift && rcKeyPressed(RC_KEY_N)) {
            if (can_change_folder(st)) st->wantNewFolder = true;
            else                       say(app, st, "A new folder can't go here.");
        }
        if (rcKeyPressed(RC_KEY_A) && shown_view(st) != VIEW_COLUMNS) {
            int i;

            for (i = 0; i < st->rowCount; i++) node_of(st, st->rows[i])->selected = true;
        }
        if (shift && rcKeyPressed(RC_KEY_PERIOD)) {
            st->showHidden = !st->showHidden;
            st->dirty      = true;
        }
        if (rcKeyPressed(RC_KEY_BACKSPACE)) trash_selection(app, st);
        return;
    }

    if (shown_view(st) == VIEW_COLUMNS) {
        if (rcKeyPressed(RC_KEY_LEFT))  move_in_columns(st, -1, 0);
        if (rcKeyPressed(RC_KEY_RIGHT)) move_in_columns(st, 1, 0);
        if (rcKeyPressed(RC_KEY_UP))    move_in_columns(st, 0, -1);
        if (rcKeyPressed(RC_KEY_DOWN))  move_in_columns(st, 0, 1);
    } else {
        if (rcKeyPressed(RC_KEY_UP))    move_cursor(st, -cols, shift);
        if (rcKeyPressed(RC_KEY_DOWN))  move_cursor(st, cols, shift);
        if (cols > 1 && rcKeyPressed(RC_KEY_LEFT))  move_cursor(st, -1, shift);
        if (cols > 1 && rcKeyPressed(RC_KEY_RIGHT)) move_cursor(st, 1, shift);
    }
    if (!st->peek) type_to_select(app, st);
    if (!st->peek && rcKeyPressed(RC_KEY_DELETE)) trash_selection(app, st);
    if (st->cursor < 0) return;
    if (rcKeyPressed(RC_KEY_SPACE)) st->peek = !st->peek;
    if (!st->peek && rcKeyPressed(RC_KEY_ENTER) && can_rename(st, st->cursor))
        start_rename(st, st->cursor);
}

/* ---- The context menu ---- */

/* One menu for the whole view. RayClay opens it where the pointer is, and the row under the
   pointer on that frame is what it acts on. That row is selected, the way the Finder selects
   it, so no other row stays lit beside it. In columns only a file of the last column can be:
   selecting a folder would open it, and a row further left would close the columns after
   it, so such a row wears a ring (views.h) until a command is chosen. */
static inline void context_target(AppState *st) {
    const Node *n;

    if (st->menuOpen) return;
    st->menuTarget = st->hover;
    st->menuColumn = st->hoverColumn;
    if (st->menuTarget < 0) return;
    n = node_of(st, st->menuTarget);
    if (n->selected) return;
    if (shown_view(st) != VIEW_COLUMNS)
        select_only(st, st->menuTarget);
    else if (!n->isDir && st->cols[st->menuColumn].dir == st->folder)
        pick_in_column(st, st->menuColumn, st->menuTarget);
}

/* A chosen command starts from its target selected, and from there it is the ordinary one. */
static inline void menu_pick(AppState *st) {
    if (shown_view(st) == VIEW_COLUMNS) pick_in_column(st, st->menuColumn, st->menuTarget);
}

static inline void row_menu(RC_App *app, AppState *st, int id) {
    const Node *n     = node_of(st, id);
    bool        trash = in_trash(st, id);

    if (rcMenuItem(n->isDir ? "Open" : "Quick Look")) {
        menu_pick(st);
        open_node(st, id);
    }
    if (rcMenuItem("Get Info")) {
        menu_pick(st);
        open_info(st, id);
    }
    if (can_rename(st, id) && rcMenuItem("Rename")) {
        menu_pick(st);
        start_rename(st, id);
    }
    if (!n->real && !trash && !is_fixed(st, id) && rcMenuItem("Duplicate")) {
        menu_pick(st);
        duplicate_selection(app, st);
    }
    if (rcMenuItem("Copy Path")) copy_path(app, st, id);
    if (is_fixed(st, id)) return;
    menu_rule();
    if (trash && n->parent == st->trash && rcMenuItem("Put Back")) {
        menu_pick(st);
        put_back(app, st);
    }
    if (rcMenuItem(n->real || trash ? "Delete Immediately..." : "Move to Trash",
                   .className = "text-danger hover:bg-danger/10")) {
        menu_pick(st);
        trash_selection(app, st);
    }
}

/* The folder's own menu, on the empty space around the rows. */
static inline void background_menu(AppState *st) {
    if (rcMenuItem("New Folder", .disabled = !can_change_folder(st))) st->wantNewFolder = true;
    if (rcMenuItem("Get Info")) open_info(st, st->folder);
    if (rcMenuItem(st->showHidden ? "Hide Hidden Files" : "Show Hidden Files")) {
        st->showHidden = !st->showHidden;
        st->dirty      = true;
    }
    if (st->folder == st->trash && node_of(st, st->trash)->first >= 0) {
        menu_rule();
        if (rcMenuItem("Empty Trash...")) {
            st->confirm     = CONFIRM_EMPTY_TRASH;
            st->confirmOpen = true;
        }
    }
}

static inline void context_menu(RC_App *app, AppState *st) {
    bool open = rcBeginContextMenu("ctx", "browse");

    if (open) {
        st->menuUpNow = true;
        context_target(st);
        if (st->menuTarget >= 0) row_menu(app, st, st->menuTarget);
        else                     background_menu(st);
        rcEndContextMenu();
    }
    /* The rows have already read this for their ring, so a change takes one more frame. */
    if (open != st->menuOpen) rcWindowRequestFrame(rcAppMainWindow(app));
    st->menuOpen = open;
}

/* ---- The overlays ---- */

static inline void confirm_dialog(AppState *st, RC_Arena *a) {
    bool emptying = st->confirm == CONFIRM_EMPTY_TRASH;
    int  count;

    if (!st->confirmOpen) return;
    count = emptying ? 0 : selection_ids(st, st->sortBuf);
    if (rcBeginModal("confirm", &st->confirmOpen)) {
        rcColumn(.gap = 12, .wType = RC_PX(340)) {
            rcTextC(emptying ? "Empty the Trash?"
                             : count == 1 ? rcFormat(a, "Delete \"%s\"?",
                                                     node_of(st, st->sortBuf[0])->name).chars
                                          : rcFormat(a, "Delete %d items?", count).chars,
                    .font = rcFontSemibold(TEXT_TITLE), .color = ui.ink);
            rcTextL("This can't be undone.", .color = ui.muted, .size = TEXT_BODY);
            /* The modal focuses its first stop, Cancel, so Return cancels; Tab reaches Delete. */
            rcRow(.gap = 8, .align = "cr", .w = "grow") {
                if (rcButton("confirm_no", "Cancel", .className = button_h()))
                    st->confirmOpen = false;
                if (rcButton("confirm_yes", emptying ? "Empty Trash" : "Delete",
                             .className = button_h(), .variant = RC_BTN_DANGER)) {
                    st->confirmOpen = false;
                    if (emptying) st->wantEmptyTrash = true;
                    else          st->wantDelete = true;
                }
            }
        }
        rcEndModal();
    }
}

/* The answer to the last action, gone by itself. The app parks between inputs, so while it
   shows it asks for the frame that takes it away. It floats from the foot of the view, not
   the window's, so it stands clear of the bars under the view and of a phone's keyboard. */
static inline void toast(RC_App *app, const AppState *st) {
    double left = st->toastUntil - rcAppTime(app);

    if (!st->toast[0] || left <= 0.0) return;
    rcWindowRequestFrameAfter(rcAppMainWindow(app), left + 0.05);
    rcBox(.bg = rcRgb(29, 29, 31, 235 / 255.0f), .px = 14, .py = 8, .borderRadius = "all-lg",
          .shadow = { .color = rcRgb(0, 0, 0, 60 / 255.0f), .y = 4.0f, .blur = 12.0f },
          .floating = { .to = RC_ATTACH_ELEMENT, .toId = "browse",
                        .parent = RC_ANCHOR_BOTTOM_CENTER, .element = RC_ANCHOR_BOTTOM_CENTER,
                        .offset = { 0.0f, -12.0f }, .zIndex = 10,
                        .capture = RC_CAPTURE_PASSTHROUGH }) {
        rcTextC(st->toast, .color = RC_WHITE, .size = TEXT_BODY, .select = RC_SELECT_NONE);
    }
}

/* While a drag is over the window: what is dragged is unknown until the release, so the words
   promise nothing about it. It stands where the toast does, and over it. */
static inline void drop_hint(void) {
    rcRow(.bg = ui.select, .gap = 8, .px = 16, .py = 10, .align = "cl", .borderRadius = "all-full",
          .shadow = { .color = rcRgb(0, 0, 0, 60 / 255.0f), .y = 4.0f, .blur = 12.0f },
          .floating = { .to = RC_ATTACH_ELEMENT, .toId = "browse",
                        .parent = RC_ANCHOR_BOTTOM_CENTER, .element = RC_ANCHOR_BOTTOM_CENTER,
                        .offset = { 0.0f, -12.0f }, .zIndex = 11,
                        .capture = RC_CAPTURE_PASSTHROUGH }) {
        rcIconFolder(16.0f, RC_WHITE);
        rcTextL("Drop to open in Files", .color = RC_WHITE, .size = TEXT_BODY,
                .select = RC_SELECT_NONE);
    }
}

/* The sidebar on a window too narrow to keep it: over the view, on a scrim that closes it.
   It floats from the window's corner, so it pads the top, bottom and left insets itself and
   fills them with the sidebar's color. */
static inline void sidebar_drawer(RC_App *app, AppState *st, RC_Insets safe) {
    if (!st->drawer) return;
    rcBox(.id = "drawer_scrim", .bg = rcRgb(0, 0, 0, 60 / 255.0f), .w = "100vw", .h = "100vh",
          .floating = { .to = RC_ATTACH_ROOT, .zIndex = 20 }) {
        rcColumn(.bg = ui.sidebarBg, .pt = safe.top, .pb = safe.bottom, .pl = safe.left,
                 .h = "grow", .wType = RC_PX(SIDE_W + safe.left)) {
            sidebar(app, st);
        }
    }
    if (rcClicked("drawer_scrim")) st->drawer = false;
}

/* ---- The view ---- */

/* The rows, or a sentence where there are none. The toolbar and the sidebar have just had
   their clicks, so anything they changed is brought in line before the view is drawn. Its
   bands run into the safe insets at its sides; what it shows stands inside them, where a
   scrollbar is clear of a cutout too. */
static inline void browse_pane(RC_App *app, AppState *st, const Shell *s) {
    RC_Insets side = s->edge;
    ViewMode  view;

    if (s->info) side.right = 0.0f;   /* the info pane holds that side */
    if (st->dirty) rebuild_rows(app, st);
    view = shown_view(st);
    st->hover = -1;
    rcColumn(.id = "browse", .bg = ui.canvas, .w = "grow", .h = "grow", .className = "min-w-0") {
        if (st->search[0]) {
            scope_bar(st, side);
        } else if (descends_from(&st->tree, st->folder, st->trash)
                   && node_of(st, st->trash)->first >= 0) {
            rcRow(.bg = ui.barBg, .gap = 8, .pl = 12 + side.left, .pr = 12 + side.right, .py = 6,
                  .align = "cl", .w = "grow") {
                rcBox(.w = "grow") {
                    rcTextL("Trash", .font = rcFontSemibold(TEXT_BODY), .color = ui.ink,
                            .select = RC_SELECT_NONE);
                }
                if (rcButton("empty_trash", "Empty", .className = button_h())) {
                    st->confirm     = CONFIRM_EMPTY_TRASH;
                    st->confirmOpen = true;
                }
            }
            rule();
        }
        if (view == VIEW_COLUMNS && st->colCount) {
            rcBox(.pl = side.left, .pr = side.right, .w = "grow", .h = "grow") {
                column_view(app, st, s->viewWidth);
            }
        } else if (!st->rowCount) {
            if (st->search[0]) {
                rcColumn(.gap = 12, .pl = side.left, .pr = side.right, .align = "cc", .w = "grow",
                         .h = "grow") {
                    rcIconSearch(32.0f, ui.faint);
                    rcText(rcFormat(rcAppArena(app), "No results for \"%s\" in %s", st->search,
                                    node_of(st, search_scope(st))->name),
                           .color = ui.muted, .size = TEXT_BODY, .select = RC_SELECT_NONE);
                    if (rcButton("clear_search", "Clear Search", .className = button_h())) {
                        st->search[0] = '\0';
                        st->dirty     = true;
                    }
                }
            } else {
                rcBox(.pl = side.left, .pr = side.right, .w = "grow", .h = "grow") {
                    folder_empty(st, st->folder);
                }
            }
        } else if (view == VIEW_ICONS) {
            rcBox(.pl = side.left, .pr = side.right, .w = "grow", .h = "grow") {
                icon_view(app, st, s->viewWidth);
            }
        } else {
            list_view(app, st, s->compact, side);
        }
    }
    /* A click on the empty space clears the selection, but a preview is there to be read. */
    if (rcClicked("browse") && !st->search[0] && !rcIsHovered("preview_col"))
        select_only(st, st->view == VIEW_COLUMNS ? st->folder : -1);
    context_menu(app, st);
}

/* ---- The frame ---- */

static inline void layout(RC_App *app, void *userData) {
    AppState   *st   = (AppState *)userData;
    RC_Viewport vp   = rcViewport();
    RC_Insets   safe = vp.safe;
    float       bottom = safe.bottom > vp.keyboard.bottom ? safe.bottom : vp.keyboard.bottom;
    bool        renaming = st->renaming >= 0;
    Shell       s;

    if (!st->seeded)
        seed(st, vp.width < COMPACT_W || vp.height - vp.safe.top - vp.safe.bottom < COMPACT_H);
    st->menuUp    = st->menuUpNow;
    st->menuUpNow = false;
    apply_pending(app, st);
    s = measure_shell(app, st);
    if (s.compact && st->view == VIEW_COLUMNS) {
        st->view  = VIEW_LIST;
        st->dirty = true;
        rebuild_rows(app, st);
    }
    if (s.viewWidth != st->lastViewWidth) st->revealColumns = 2;
    st->lastViewWidth = s.viewWidth;
    /* Read before apply_pending ended the rename: the Enter that confirmed a name must not
       also start the next rename. */
    if (!renaming) read_keys(app, st, &s);

    /* Escape leaves the drawer, then a finger's goes up the tree. A desktop's closes what is
       open and otherwise does nothing, as the Finder's does. It is read only where it does
       one of those, so at the top Android's Back stays the system's and leaves the app. */
    if (s.sidebarFits) st->drawer = false;
    const bool climbs = finger() && node_of(st, st->folder)->parent >= 0;
    if ((st->drawer || climbs) && rcKeyPressed(RC_KEY_ESCAPE)) {
        if (st->drawer) st->drawer = false;
        else            go_up(st);
    }

    /* The bands reach the window's edges, a phone's status bar, cutout and home indicator
       included, and each pads what it holds by the safe inset there (Shell.edge). The root
       lifts the frame only by what a keyboard covers past the bottom inset. */
    rcColumn(.id = "root", .bg = ui.canvas, .pb = bottom - safe.bottom, .w = "grow", .h = "grow") {
        toolbar(st, &s);
        /* A click there may have changed the view: the rest of the frame reads the new shape. */
        s = measure_shell(app, st);
        rcRow(.w = "grow", .h = "grow") {
            if (s.sidebar) {
                /* As in the drawer, the sidebar's color fills the insets it meets. */
                rcColumn(.bg = ui.sidebarBg, .pb = safe.bottom, .pl = safe.left, .h = "grow") {
                    sidebar(app, st);
                }
                vline();
            }
            /* min-w-0, as a web page's main pane beside its sidebar: the window sets its width. */
            rcColumn(.w = "grow", .h = "grow", .className = "min-w-0") {
                if (s.narrow) large_title(st, &s);
                if (s.searchRow) {
                    rcRow(.bg = ui.barBg, .pl = 8 + s.edge.left, .pr = 8 + s.edge.right, .py = 6,
                          .w = "grow") {
                        search_field(st, 0.0f);
                    }
                    rule();
                }
                rcRow(.w = "grow", .h = "grow") {
                    browse_pane(app, st, &s);
                    if (s.info) {
                        vline();
                        rcBox(.pr = s.edge.right, .h = "grow") { info_pane(app, st); }
                    }
                }
                if (s.pathBar) {
                    rule();
                    path_bar(st, rcAppArena(app), &s);
                }
                rule();
                status_bar(st, rcAppArena(app), &s);
            }
        }
    }
    sidebar_drawer(app, st, safe);
    quick_look(app, st);
    info_dialog(app, st);
    confirm_dialog(st, rcAppArena(app));
    toast(app, st);
    if (rcIsDragOver(NULL)) drop_hint();

    st->revealCursor = false;
    /* The frame that scrolls the columns is one more to draw; the frame that shows the move,
       rcScrollIntoView asks for itself. */
    if (st->dirty || st->wantNewFolder || st->wantDelete || st->wantEmptyTrash || st->wantEject
        || st->revealColumns > 1)
        rcWindowRequestFrame(rcAppMainWindow(app));
    if (st->revealColumns > 0) st->revealColumns--;
}

#endif /* APP_APP_H */
