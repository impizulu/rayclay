/*
    chrome.h - the window around the view

    The toolbar is the title bar: the band that drags the window, the sidebar's color over
    the sidebar, then back and forward, the folder's name, the view switcher, sort, more,
    search, and the window's controls in the corner. Below it, the sidebar; below the view,
    the path bar and the status bar.
*/
#ifndef APP_CHROME_H
#define APP_CHROME_H

#include "rayclay.h"
#include "app/theme.h"
#include "app/browser.h"
#include "app/parts.h"

/* The window's shape this frame, decided once from the space there is. */
typedef struct {
    float width, height;
    float zoom;
    bool  sidebarFits;      /* wide enough to keep it open */
    bool  sidebar;          /* and the user has not hidden it */
    bool  narrow;           /* a phone held upright: the name and search leave the band */
    bool  compact;          /* that, or a phone on its side: tall rows, no sidebar, no columns */
    bool  infoFits;         /* the info pane has room, in a view that uses it */
    bool  info;             /* and the user has it open */
    bool  infoButton;       /* the band has room for its button */
    bool  searchRow;        /* a phone's search: a row of its own above the view */
    bool  searchIcon;       /* a short desktop band shows a magnifier that opens the field */
    bool  pathFits;
    bool  pathBar;
    float viewWidth;        /* what the view is left */
    /* The safe insets the bands meet: a phone's status bar, cutout and home indicator. A band
       runs into them in its own color and pads what it holds by them. The docked sidebar
       holds the left side. */
    RC_Insets edge;
} Shell;

static inline Shell measure_shell(RC_App *app, const AppState *st) {
    RC_Viewport vp = rcViewport();
    Shell       s;
    float       band;

    s.width       = vp.width - vp.safe.left - vp.safe.right;
    s.height      = vp.height - vp.safe.top - vp.safe.bottom;
    s.zoom        = rcWindowZoom(rcAppMainWindow(app));
    s.narrow      = s.width < COMPACT_W;
    s.compact     = s.narrow || s.height < COMPACT_H;
    s.sidebarFits = s.width >= SIDEBAR_W && !s.compact;
    s.sidebar     = s.sidebarFits && !st->sidebarHidden;
    s.infoFits    = s.width >= WIDE_W && shown_view(st) != VIEW_COLUMNS;
    s.info        = st->infoShown && s.infoFits;
    s.pathFits    = !s.compact && s.height >= PATH_H;
    s.pathBar     = s.pathFits && !st->pathHidden;
    s.viewWidth   = s.width - (s.sidebar ? (float)SIDE_W + 1.0f : 0.0f)
                    - (s.info ? (float)INFO_W + 1.0f : 0.0f);
    s.edge        = vp.safe;
    if (s.sidebar) s.edge.left = 0.0f;
    /* The band is drawn unzoomed, so what it holds is judged in its own pixels, the ones
       rcWindowControlsWidth() answers in. */
    band = s.width * s.zoom - (s.sidebar ? (float)SIDE_W * s.zoom + 1.0f : (float)BAND_LEAD)
           - rcWindowControlsWidth();
    s.searchRow  = s.narrow || (s.compact && band < (float)BAND_WITH_SEARCH);
    s.searchIcon = !s.compact && band < (float)BAND_WITH_SEARCH;
    s.infoButton = !s.compact && band >= (float)BAND_WITH_INFO;
    return s;
}

/* ---- The toolbar ---- */

/* One segment of the view switcher: the chosen one is a raised white chip. With the track's
   1 px of padding the switcher is as tall as the toolbar's other controls (theme.h). The
   segments sit 1 px apart, so a segment's focus ring is drawn inside it (ring-inset). */
static inline void view_segment(AppState *st, const char *id, RC_IconCallback icon,
                                const char *tip, ViewMode mode) {
    bool  on   = shown_view(st) == mode;
    float side = (float)(finger() ? HIT : TOOL_H) - 2.0f;

    /* A search's results have no columns to show, so that view is off while it runs. */
    rcBox(.id = id, .focusable = true, .align = "cc", .borderRadius = "all-sm",
          .disabled = mode == VIEW_COLUMNS && st->search[0],
          .wType = RC_PX(finger() ? HIT : TOOL_H), .hType = RC_PX(side),
          .shadow = { .color = on ? rcRgb(0, 0, 0, 48 / 255.0f) : RC_UNSET_COLOR, .y = 1.0f,
                      .blur = 2.0f },
          .tooltip = tip, .className = on ? "bg-white ring-inset" : "wash ring-inset") {
        icon(15.0f, on ? ui.ink : ui.muted);
    }
    if (box_clicked(id)) set_view(st, mode);
}

/* The three views, or on a narrow toolbar one button that flips between icons and list. */
static inline void view_switcher(AppState *st, bool compact) {
    if (compact) {
        bool icons = st->view == VIEW_ICONS;

        if (tool_button("view_flip", icons ? rcIconList : rcIconLayoutGrid,
                        icons ? "as List" : "as Icons", false, false))
            set_view(st, icons ? VIEW_LIST : VIEW_ICONS);
        return;
    }
    rcRow(.id = "views", .bg = ui.segTrack, .gap = 1, .p = 1, .align = "cl",
          .borderRadius = "all-md") {
        view_segment(st, "view_icons", rcIconLayoutGrid, "as Icons", VIEW_ICONS);
        view_segment(st, "view_list", rcIconList, "as List", VIEW_LIST);
        view_segment(st, "view_cols", rcIconColumns3, "as Columns", VIEW_COLUMNS);
    }
}

/* A Sort item: its check says which is current. */
static inline void sort_item(AppState *st, const char *label, SortKey key) {
    if (rcMenuItem(label, .leadingIcon = st->sort == key ? rcIconCheck : NULL)) {
        st->sort  = key;
        st->dirty = true;
    }
}

static inline void menu_rule(void) {
    rcBox(.py = 4, .w = "grow") { rule(); }
}

static inline void sort_menu(AppState *st, bool caret) {
    if (rcBeginMenu("sort", "", .className = finger() ? "h-11" : "h-8", .caret = caret,
                    .variant = RC_BTN_GHOST, .leadingIcon = rcIconArrowUpDown)) {
        st->menuUpNow = true;
        sort_item(st, "Name", SORT_NAME);
        sort_item(st, "Date Modified", SORT_DATE);
        sort_item(st, "Size", SORT_SIZE);
        sort_item(st, "Kind", SORT_KIND);
        menu_rule();
        if (rcMenuItem("Ascending", .leadingIcon = st->sortDown ? NULL : rcIconCheck)) {
            st->sortDown = false;
            st->dirty    = true;
        }
        if (rcMenuItem("Descending", .leadingIcon = st->sortDown ? rcIconCheck : NULL)) {
            st->sortDown = true;
            st->dirty    = true;
        }
        rcEndMenu();
    }
}

/* Ask for the folder dialog from the click itself: a browser shows a picker only within a
   user action, and RayClay keeps that rule on every host. A click while a dialog is still up,
   as a Linux one leaves the window live, is refused: the answer to come is the first
   request's, so its token stays. The web's picker gives way to the new one instead. */
static inline void ask_folder(AppState *st) {
    RC_FileDialogToken asked = rcFileDialogOpen(RC_FILE_DIALOG_FOLDER, NULL);

    if (rcFileDialogPoll(st->ask).status == RC_FILE_DIALOG_PENDING)
        (void)rcFileDialogPoll(asked);      /* collects its REFUSED */
    else
        st->ask = asked;
}

static inline void more_menu(AppState *st, const Shell *s) {
    if (rcBeginMenu("more", "", .className = finger() ? "h-11" : "h-8", .variant = RC_BTN_GHOST,
                    .leadingIcon = rcIconEllipsis)) {
        st->menuUpNow = true;
        if (rcMenuItem("New Folder", .disabled = !can_change_folder(st))) st->wantNewFolder = true;
        if (rcMenuItem("Quick Look", .disabled = st->cursor < 0)) st->peek = true;
        if (rcMenuItem("Get Info")) open_info(st, info_target(st));
        if (!st->openUnsupported && rcMenuItem("Open Folder...")) ask_folder(st);
        menu_rule();
        if (rcMenuItem(st->showHidden ? "Hide Hidden Files" : "Show Hidden Files")) {
            st->showHidden = !st->showHidden;
            st->dirty      = true;
        }
        if (s->pathFits && rcMenuItem(st->pathHidden ? "Show Path Bar" : "Hide Path Bar"))
            st->pathHidden = !st->pathHidden;
        if (s->sidebarFits && rcMenuItem(st->sidebarHidden ? "Show Sidebar" : "Hide Sidebar"))
            st->sidebarHidden = !st->sidebarHidden;
        rcEndMenu();
    }
}

/* The search field, with its clear x once there is text. A clear is a change as typing is,
   so either one rebuilds the rows. */
static inline void search_field(AppState *st, float width) {
    rcBox(.wType = width > 0.0f ? RC_PX(width) : RC_GROW) {
        if (rcTextInput("search", st->search, (int)sizeof st->search,
                        .className = finger() ? "h-11 rounded-md border px-2"
                                              : "h-8 rounded-md border px-2",
                        .placeholder = "Search", .leadingIcon = rcIconSearch,
                        .clearButton = true))
            st->dirty = true;
    }
}

/* What the band holds left of the window's controls. A wrapper carries the menus' tooltips,
   as rcBeginMenu has none. */
static inline void tools(AppState *st, const Shell *s) {
    /* A short band shows the field while it is focused or holds a query, as the Finder's
       magnifier does; empty and left, it folds back. The magnifier's click and Ctrl/Cmd+F
       focus it, which this reads from the next frame on. */
    const bool searching = s->searchIcon && (st->search[0] || rcIsFocused("search"));

    if (!s->sidebar) {
        rcRow(.noDrag = true) {
            if (tool_button("toggle_side", rcIconPanelLeft, "Show the sidebar", st->drawer,
                            false)) {
                if (s->sidebarFits) st->sidebarHidden = false;
                else                st->drawer = !st->drawer;
            }
        }
    }
    rcRow(.gap = 2, .noDrag = true) {
        /* A phone's back climbs to the parent where there is no history, as iOS Files does. */
        const bool climbs = s->compact && !can_go(st, -1) && node_of(st, st->folder)->parent >= 0;

        if (tool_button("back", rcIconChevronLeft, climbs ? "Enclosing folder" : "Back", false,
                        !can_go(st, -1) && !climbs)) {
            if (climbs) go_up(st);
            else        go_history(st, -1);
        }
        if (!s->compact && s->width * s->zoom >= 480.0f
            && tool_button("forward", rcIconChevronRight, "Forward", false, !can_go(st, 1)))
            go_history(st, 1);
    }
    /* An upright phone gives the name a line of its own (large_title), so the tools keep
       the band. */
    if (!searching) {
        rcBox(.pl = 4, .overflow = "hidden", .w = "grow", .className = "min-w-0") {
            if (!s->narrow)
                rcText(shown_name(node_of(st, st->folder)), .font = rcFontSemibold(TEXT_TITLE),
                       .color = ui.ink, .wrap = "e", .select = RC_SELECT_NONE);
        }
    }
    /* An open field in a short band takes the name's room: these tools grow, and it fills. */
    rcRow(.gap = 8, .align = "cl", .noDrag = true, .w = searching ? "grow" : "fit") {
        view_switcher(st, s->compact);
        rcBox(.tooltip = "Sort") { sort_menu(st, !s->compact); }
        if (s->infoButton
            && tool_button("info_toggle", rcIconInfo,
                           s->infoFits ? "Show or hide the Info pane" : "Get Info", s->info,
                           false)) {
            if (s->infoFits) st->infoShown = !st->infoShown;
            else             open_info(st, info_target(st));
        }
        rcBox(.tooltip = "More") { more_menu(st, s); }
        if (s->searchIcon && !searching) {
            if (tool_button("search_open", rcIconSearch, "Search", false, false))
                rcSetFocus("search");
        } else if (!s->searchRow) {
            search_field(st, searching ? 0.0f : 176.0f);
        }
    }
}

/* The folder's name on an upright phone, set large above the search, as iOS Files and
   Android's do. On its side the band has the room, and the view needs the height. Its inset
   is the rows' and the Name heading's, so the three start on one line. */
static inline void large_title(AppState *st, const Shell *s) {
    rcRow(.bg = ui.barBg, .pt = 4, .pl = 16 + s->edge.left, .pr = 16 + s->edge.right,
          .align = "cl", .w = "grow") {
        rcBox(.overflow = "hidden", .w = "grow", .className = "min-w-0") {
            rcText(shown_name(node_of(st, st->folder)), .font = rcFontSemibold(26.0f),
                   .color = ui.ink, .wrap = "e", .select = RC_SELECT_NONE);
        }
    }
}

/* The band. It is drawn inside rcUnzoomed(): the OS's drag strip was fixed at .titlebar.height
   when the window opened, so the band keeps that height at every zoom. Sidebar widths are
   multiplied out of the zoom for the same reason, so the band's left block lines up with the
   sidebar below it. On a phone the band runs up under the status bar and out under a
   cutout, and its tools keep clear of both; in this scope an inset is read in the band's
   own pixels, rcGetSafeAreaInsets()'s. */
static inline void toolbar(AppState *st, const Shell *s) {
    RC_Insets safe = rcGetSafeAreaInsets();

    rcUnzoomed() {
        rcRow(.id = RC_ID_WINDOW_DRAG, .bg = ui.barBg, .align = "cl", .w = "grow",
              .hType = RC_PX(BAR_H + safe.top)) {
            if (s->sidebar) {
                rcBox(.bg = ui.sidebarBg, .h = "grow",
                      .wType = RC_PX((float)SIDE_W * s->zoom + safe.left)) {}
                vline();
            }
            /* The window's controls hold the corner: this row shrinks with the window
               (min-w-0, as on the web), and the tools clip in it before they could push
               the controls out of a narrow window. */
            rcRow(.pt = safe.top, .pl = s->sidebar ? 0.0f : safe.left, .pr = safe.right,
                  .border = { .color = ui.line }, .w = "grow", .h = "grow",
                  .className = "border-b min-w-0") {
                rcRow(.gap = 8, .px = 12, .align = "cl", .overflow = "hidden", .w = "grow",
                      .h = "grow") {
                    tools(st, s);
                }
                rcWindowControls();
            }
        }
    }
}

/* ---- The sidebar ---- */

/* One entry: a blue glyph and a name, and a gray pill while the user is inside it. Like
   every entry of the sidebar it is a Tab stop, its focus ring drawn inside it (ring-inset):
   the sidebar scrolls, and would clip a ring drawn around a row as wide as it. */
static inline void nav_row(AppState *st, const char *id, int target, RC_IconCallback icon,
                           const char *label, RC_Color iconColor) {
    bool on = st->place == target;

    rcRow(.id = id, .focusable = true, .gap = 8, .px = 8, .align = "cl",
          .borderRadius = "all-md", .w = "grow", .hType = RC_PX(finger() ? HIT : 28),
          .className = on ? "side-chosen ring-inset" : "side-wash ring-inset") {
        /* A tag's dot sits in a glyph's cell, so every name in the sidebar starts on one line. */
        if (icon) {
            icon(16.0f, iconColor);
        } else {
            rcBox(.align = "cc", .wType = RC_PX(16)) {
                tag_dot(node_of(st, target)->tag, 10.0f, false);
            }
        }
        line_fill(rcStringFromCStr(label), TEXT_BODY, ui.ink);
    }
    if (box_clicked(id)) go_place(st, target);
}

static inline void sidebar_head(const char *title) {
    rcBox(.pt = 12, .pb = 4, .pl = 8) { section_title(title, ui.muted); }
}

/* An opened folder's entry also carries its eject, shown under the pointer or, for a finger,
   on the open one, HIT square. The press goes to the innermost element polled under the
   pointer. */
static inline void location_row(AppState *st, RC_Arena *a, int index) {
    int         target = st->locations[index];
    const char *id     = rcFormat(a, "nav_loc%d", index).chars;
    const char *eject  = rcFormat(a, "eject%d", index).chars;
    bool        on     = st->place == target;
    float       side   = finger() ? (float)HIT : 20.0f;

    rcRow(.id = id, .focusable = true, .gap = 8, .pl = 8, .pr = 4, .align = "cl",
          .borderRadius = "all-md", .w = "grow", .hType = RC_PX(finger() ? HIT : 28),
          .className = on ? "side-chosen ring-inset" : "side-wash ring-inset") {
        rcIconFolder(16.0f, ui.accent);
        line_fill(shown_name(node_of(st, target)), TEXT_BODY, ui.ink);
        if (rcShowsHover(id) || (finger() && on)) {
            rcBox(.id = eject, .align = "cc", .borderRadius = "all-sm", .wType = RC_PX(side),
                  .hType = RC_PX(side), .tooltip = "Eject") {
                rcIconX(12.0f, rcShowsHover(eject) ? ui.ink : ui.muted);
            }
            if (rcClicked(eject)) st->wantEject = index + 1;
        }
    }
    if (box_clicked(id)) go_place(st, target);
}

static inline void sidebar(RC_App *app, AppState *st) {
    static const char *const FAV_ID[6] = {
        "nav_apps", "nav_desktop", "nav_docs", "nav_downloads", "nav_pictures", "nav_music"
    };
    static const char *const TAG_ID[TAG_END] = {
        "", "nav_tag1", "nav_tag2", "nav_tag3", "nav_tag4", "nav_tag5", "nav_tag6", "nav_tag7"
    };
    const RC_IconCallback FAV_ICON[6] = {
        rcIconAppWindow, rcIconMonitor, rcIconFileText, rcIconDownload, rcIconImage, rcIconMusic
    };
    RC_Arena *a = rcAppArena(app);
    int       i;

    rcColumn(.id = "sidebar", .bg = ui.sidebarBg, .gap = 1, .pb = 12, .pl = 8,
             .pr = 8, .scroll = "v", .h = "grow", .wType = RC_PX(SIDE_W)) {
        sidebar_head("Favorites");
        nav_row(st, "nav_recents", st->recents, rcIconClock, "Recents", ui.accent);
        for (i = 0; i < 6; i++)
            nav_row(st, FAV_ID[i], st->favorites[i], FAV_ICON[i],
                    node_of(st, st->favorites[i])->name, ui.accent);
        sidebar_head("Locations");
        nav_row(st, "nav_sample", st->sampleRoot, rcIconHardDrive, "Sample Drive", ui.muted);
        nav_row(st, "nav_computer", st->computer, rcIconMonitor, "This Computer", ui.muted);
        for (i = 0; i < st->locationCount; i++) location_row(st, a, i);
        nav_row(st, "nav_trash", st->trash, rcIconTrash2, "Trash", ui.muted);
        if (!st->openUnsupported) {
            rcRow(.id = "open_folder", .focusable = true, .gap = 8, .px = 8, .align = "cl",
                  .borderRadius = "all-md", .w = "grow", .hType = RC_PX(finger() ? HIT : 28),
                  .className = "side-wash ring-inset") {
                /* The system's folder dialog, the way to the user's own files: an action, so in
                   the accent, not a place's gray. */
                rcIconFolderPlus(16.0f, ui.accent);
                line_fill(rcStringFromCStr("Open Folder..."), TEXT_BODY, ui.accent);
            }
            if (box_clicked("open_folder")) ask_folder(st);
        }
        sidebar_head("Tags");
        for (i = TAG_RED; i < TAG_END; i++)
            nav_row(st, TAG_ID[i], st->tags[i], NULL, node_of(st, st->tags[i])->name, ui.muted);
    }
    rcScrollbar("sidebar");
}

/* ---- The bars under the view ---- */

/* One step of the path: its icon and name, a click goes there. A Tab stop, its focus ring
   drawn inside it (ring-inset): the bar clips what reaches past its height. */
static inline void crumb(AppState *st, RC_Arena *a, int id, bool last) {
    const Node *n = node_of(st, id);
    const char *c = rcFormat(a, "crumb%d", id).chars;

    rcRow(.id = c, .focusable = true, .gap = 4, .px = 4, .align = "cl", .borderRadius = "all-sm",
          .h = "grow", .className = "wash ring-inset") {
        file_icon(n, 14.0f, false);
        rcText(shown_name(n), .color = last ? ui.ink : ui.muted, .size = TEXT_CAPTION, .wrap = "n",
               .select = RC_SELECT_NONE);
    }
    if (box_clicked(c)) open_node(st, id);
}

/* The path of what is selected, deepest step on the right. Steps that do not fit leave from
   the top of the tree, measured with the font, as a Finder window narrows. */
static inline void path_bar(AppState *st, RC_Arena *a, const Shell *s) {
    int   steps[32], n = 0, i;
    float room = s->viewWidth - 24.0f;
    int   id = st->cursor >= 0 ? st->cursor : st->folder;

    for (; id >= 0 && n < 32; id = node_of(st, id)->parent) {
        const char *name = node_of(st, id)->name;
        float       w    = rcTextWidth(0, TEXT_CAPTION, name, text_len(name)) + 42.0f;

        if (w > room && n) break;
        room -= w;
        steps[n++] = id;
    }
    rcRow(.id = "pathbar", .bg = ui.barBg, .pl = 8 + s->edge.left, .pr = 8 + s->edge.right,
          .align = "cl", .overflow = "hidden", .w = "grow", .hType = RC_PX(26)) {
        for (i = n - 1; i >= 0; i--) {
            crumb(st, a, steps[i], i == 0);
            if (i) rcIconChevronRight(10.0f, ui.faint);
        }
    }
}

/* The count in the middle, and in the grid the size slider on the right, as the Finder's. No
   free space beside it: no RayClay call reads a volume's, and a made-up figure would read as
   the device's own. In columns the count is the last column's, and a folder chosen there is
   the folder shown. On a phone the bar runs down under the home indicator. */
static inline void status_bar(AppState *st, RC_Arena *a, const Shell *s) {
    const Node *dir     = node_of(st, st->folder);
    bool        columns = shown_view(st) == VIEW_COLUMNS && st->colCount;
    int         rows    = columns ? st->cols[st->colCount - 1].count : st->rowCount;
    int         sel     = !columns ? selected_count(st)
                                   : st->cursor >= 0 && st->cursor != st->folder;
    RC_String   text    = sel ? rcFormat(a, "%d of %d selected", sel, rows)
                              : rcFormat(a, "%d item%s", rows, rows == 1 ? "" : "s");

    if (dir->cut) text = rcFormat(a, "Showing the first %d items", rows);
    rcRow(.bg = ui.barBg, .pb = s->edge.bottom, .pl = 12 + s->edge.left,
          .pr = 12 + s->edge.right, .align = "cc", .w = "grow",
          .hType = RC_PX((finger() ? 32 : 26) + s->edge.bottom)) {
        rcSpacer() {}
        rcText(text, .color = ui.muted, .size = TEXT_CAPTION, .wrap = "n",
               .select = RC_SELECT_NONE);
        rcRow(.align = "cr", .w = "grow") {
            if (shown_view(st) == VIEW_ICONS) {
                rcBox(.wType = RC_PX(96)) {
                    rcSlider("icon_size", &st->iconSize, ICON_MIN, ICON_MAX, .step = 8.0f);
                }
            }
        }
    }
}

#endif /* APP_CHROME_H */
