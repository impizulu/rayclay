/*
    panels.h - everything around the code: rail, explorer, search, outline, tabs, breadcrumb,
    SVG preview, bottom panel and status bar

    These are plain rows and columns over the state. A long list is an rcVirtualList, each row's
    id comes from the data, and a label is unselectable because this is chrome, not a document.
*/
#ifndef APP_PANELS_H
#define APP_PANELS_H

#include "rayclay.h"
#include "app/commands.h"
#include "app/palette.h"

#include "icons/rc_icons_arrow_down.h"
#include "icons/rc_icons_arrow_up.h"
#include "icons/rc_icons_chevron_down.h"
#include "icons/rc_icons_chevron_right.h"
#include "icons/rc_icons_circle_alert.h"
#include "icons/rc_icons_circle_x.h"
#include "icons/rc_icons_ellipsis_vertical.h"
#include "icons/rc_icons_file_text.h"
#include "icons/rc_icons_info.h"
#include "icons/rc_icons_list.h"
#include "icons/rc_icons_search.h"
#include "icons/rc_icons_settings.h"
#include "icons/rc_icons_triangle_alert.h"
#include "icons/rc_icons_x.h"

/* The logo and every SVG path this app opens by name. Repo-relative, so a run from the repo
   root or from the program's folder finds it; a web or phone build points it at its bundle. */
#ifndef RC_EX28_ICON_DIR
    #define RC_EX28_ICON_DIR "examples/assets/icons/"
#endif

static inline void label(const char *s, int size, RC_Color c) {
    rcTextC(s, .color = c, .size = (uint16_t)size, .wrap = "n", .select = RC_SELECT_NONE);
}

/* Words cut with an ellipsis at their box. The box must clip and have a width. */
static inline void label_cut(const char *s, int size, RC_Color c) {
    rcTextC(s, .color = c, .size = (uint16_t)size, .wrap = "e", .select = RC_SELECT_NONE);
}

static inline const char *upper(RC_Arena *a, const char *s) {
    RC_String out = rcFormat(a, "%s", s);
    char     *c   = (char *)out.chars;

    for (; *c; c++)
        if (*c >= 'a' && *c <= 'z')
            *c = (char)(*c - 32);
    return out.chars;
}

static inline float list_row_h(const AppState *st) {
    return rcPointerIsCoarse() || st->compact ? 40.0f : (float)ROW_H;
}

static inline void hline(void) { rcBox(.bg = theme()->edge, .w = "grow", .hType = RC_PX(1)) {} }

/* A sidebar's or panel's small capitals heading. */
static inline void section_head(const char *title) {
    rcRow(.px = 20, .align = "cl", .w = "grow", .hType = RC_PX(PANEL_TAB_H)) {
        label(title, SZ_MICRO, theme()->muted);
    }
}

/* ---- The activity rail. ---- */

static inline void rail_item(AppState *st, const char *id, RC_IconCallback icon, bool on, Command c,
                             const char *tip) {
    const Palette *t    = theme();
    bool           over = rcShowsHover(id);

    rcRow(.id = id, .align = "cl", .wType = RC_PX(ACTIVITY_W), .hType = RC_PX(ACTIVITY_W),
          .tooltip = tip) {
        rcBox(.bg = on ? t->railOn : RC_UNSET_COLOR, .h = "grow", .wType = RC_PX(2)) {}
        rcBox(.align = "cc", .h = "grow", .wType = RC_PX(ACTIVITY_W - 2)) {
            icon(24.0f, on || over ? t->railOn : t->railOff);
        }
    }
    if (rcClicked(id)) {
        /* The selected view's icon folds the sidebar away, another icon brings its view. */
        if (on && c != CMD_SETTINGS)
            st->sideOpen = false;
        else
            post_command(st, c);
    }
}

static inline void activity_rail(AppState *st) {
    const Palette *t = theme();

    rcColumn(.bg = t->activity, .h = "grow", .wType = RC_PX(ACTIVITY_W)) {
        rail_item(st, "rail_files", rcIconFileText, st->sideOpen && st->sideView == SIDE_EXPLORER,
                  CMD_EXPLORER, "Explorer (" PRIMARY_KEY "+Shift+E)");
        rail_item(st, "rail_find", rcIconSearch, st->sideOpen && st->sideView == SIDE_SEARCH,
                  CMD_SEARCH, "Search (" PRIMARY_KEY "+Shift+F)");
        rcSpacer() {}
        rail_item(st, "rail_gear", rcIconSettings, st->settingsOpen, CMD_SETTINGS,
                  "Settings (" PRIMARY_KEY "+,)");
    }
}

/* ---- The explorer. ---- */

static inline void open_node(AppState *st, int node) {
    if (open_file(st, st->ws.node[node].path)) {
        st->screen    = SCREEN_CODE;
        st->wantCaret = !rcPointerIsCoarse();
    }
}

static inline void tree_row(AppState *st, int node, const char *id) {
    const Palette *t = theme();
    Node          *n = &st->ws.node[node];
    Doc           *d = active_doc(st);
    bool           on = d && !n->dir && same_text(d->path, n->path);
    int            tab = n->dir ? -1 : tab_of(st, n->path);
    bool           dirty = tab >= 0 && doc_dirty(&st->docs[tab]);
    const char    *ctx = rcFormat(st->arena, "%s_ctx", id).chars;

    /* A list row lights under the pointer by its class, in the theme's hover role. The selected
       row's fill is the palette's own, a typed .bg, which beats the class as an inline style
       does; RC_UNSET_COLOR sets no fill at all, so any other row shows the hover. */
    rcRow(.id = id, .bg = on ? t->active : RC_UNSET_COLOR, .gap = 4,
          .pl = (float)(8 + n->depth * 12), .pr = 16, .align = "cl", .overflow = "hidden",
          .w = "grow", .hType = RC_PX(list_row_h(st)), .className = "hover:bg-surface-alt") {
        if (n->dir)
            (n->open ? rcIconChevronDown : rcIconChevronRight)(16.0f, t->text);
        else
            file_mark_box(n->name, 16.0f);
        rcRow(.align = "cl", .overflow = "hidden", .w = "grow") {
            label_cut(n->name, SZ_UI, on ? t->bright : t->text);
        }
        if (dirty)
            rcBox(.bg = t->text, .borderRadius = "all-full", .wType = RC_PX(8),
                  .hType = RC_PX(8)) {}
    }
    if (rcClicked(id)) {
        if (n->dir) {
            n->open = !n->open;
            tree_rows(&st->ws);
        } else {
            open_node(st, node);
        }
    }
    if (rcBeginContextMenu(ctx, id)) {
        st->menuOpen = true;
        if (!n->dir && rcMenuItem("Open"))
            open_node(st, node);
        if (rcMenuItem("Copy Path"))
            rcClipboardSet(n->path);
        rcEndContextMenu();
    }
}

/* Bring row `at` of a list of `count` rows into view. rcScrollToRow places it from the list's
   own geometry after this frame's layout, so a folder unfolded this frame counts. The first
   reveal of a list puts a whole row on top, `above` rows before `at` where the range and the
   view's height allow; every later one is the least move, so a row clicked in view stays under
   the pointer. */
static inline void reveal_row(const AppState *st, const char *id, int at, int count, int above,
                              bool *placed) {
    float rowH  = list_row_h(st);
    float viewH = rcGetElementBox(id).height;
    int   fit   = (int)(viewH / rowH) - 1;    /* the most rows above `at` that leave it whole */
    int   last  = (int)(((float)count * rowH - viewH) / rowH);
    int   top;

    if (*placed) {
        rcScrollToRow(id, at, RC_SCROLL_NEAREST);
        return;
    }
    if (above > fit)
        above = fit > 0 ? fit : 0;  /* a short view (a phone on its side) keeps `at` whole */
    top = at - above;
    if (top > last)
        top = last;     /* the last whole row the range can put on top */
    rcScrollToRow(id, top > 0 ? top : 0, RC_SCROLL_START);
    *placed = true;
}

/* Bring a node into view. A tree first declared this frame opens at its .scrollOffset on the
   next (rcRunApp draws that one), so the reveal waits for it. */
static inline void tree_scroll_to_node(AppState *st) {
    int i;

    if (st->revealNode < 0 || !rcGetScrollInfo("tree").found)
        return;
    for (i = 0; i < st->ws.nRows && st->ws.rows[i] != st->revealNode; i++) {}
    if (i < st->ws.nRows)
        reveal_row(st, "tree", i, st->ws.nRows, 4, &st->treePlaced);
    st->revealNode = -1;
}

static inline void tree_view(AppState *st) {
    Workspace *ws = &st->ws;

    tree_scroll_to_node(st);
    rcColumn(.id = "tree", .scroll = "v", .w = "grow", .h = "grow",
             .scrollOffset = { 0.0f, st->treeY }) {
        rcVirtualList(row, "tree", ws->nRows, list_row_h(st), .overscan = 3) {
            tree_row(st, ws->rows[row.index],
                     rcFormat(st->arena, "n%d", ws->rows[row.index]).chars);
        }
    }
    st->treeY = rcGetScrollInfo("tree").offsetY;
    rcScrollbar("tree");
}

/* The outline of the file on show: a real section of the explorer, as in its leader. */
static inline void outline_view(AppState *st) {
    const Palette *t = theme();
    Doc           *d = active_doc(st);
    int            caretLine = d && !d->unreadable ? doc_line_of(d, d->caret) : -1;
    const Symbol  *here = d && !d->unreadable ? symbol_at(d, caretLine) : NULL;

    /* Follow the caret: when it enters another symbol, that row comes into view. A file's new
       list counts at once; an outline first declared this frame waits a frame, as the tree. */
    if (here != st->outlineHere && rcGetScrollInfo("outline").found) {
        if (here)
            reveal_row(st, "outline", (int)(here - d->sym), d->nSym, 2, &st->outlinePlaced);
        st->outlineHere = here;
    }
    rcColumn(.id = "outline", .scroll = "v", .w = "grow", .h = "grow",
             .scrollOffset = { 0.0f, st->outlineY }) {
        if (d && d->nSym) {
            rcVirtualList(row, "outline", d->nSym, list_row_h(st), .overscan = 3) {
                const Symbol *s  = &d->sym[row.index];
                const char   *id = rcFormat(st->arena, "sym%d", row.index).chars;
                RC_String     name = { .isStaticallyAllocated = false, .length = s->nameLen,
                                       .chars = d->text + s->name };
                bool          on = s == here;

                rcRow(.id = id, .bg = on ? t->active : RC_UNSET_COLOR, .gap = 8, .pl = 24, .pr = 8,
                      .align = "cl", .overflow = "hidden", .w = "grow",
                      .hType = RC_PX(list_row_h(st)), .className = "hover:bg-surface-alt") {
                    rcTextC(s->kind == SYM_FUNCTION ? "f" : s->kind == SYM_TYPE ? "T"
                            : s->kind == SYM_HEADING ? "#" : "k",
                            .font = st->mono, .color = s->kind == SYM_FUNCTION
                            ? t->syntax[TOK_CONTROL] : t->syntax[TOK_TYPE], .size = SZ_SMALL,
                            .select = RC_SELECT_NONE);
                    rcText(name, .color = on ? t->bright : t->text, .size = SZ_UI, .wrap = "e",
                           .select = RC_SELECT_NONE);
                }
                if (rcClicked(id)) {
                    go_to(st, d, s->line, 0);
                    st->screen = SCREEN_CODE;
                }
            }
        } else {
            rcRow(.pl = 24, .align = "cl", .w = "grow", .hType = RC_PX(ROW_H)) {
                label(d ? "No symbols in this file." : "No editor is open.", SZ_SMALL, t->muted);
            }
        }
    }
    st->outlineY = rcGetScrollInfo("outline").offsetY;
    rcScrollbar("outline");
}

/* A foldable section heading: the whole row is the button. */
static inline void fold_head(const char *id, const char *title, bool *open) {
    const Palette *t = theme();

    rcRow(.id = id, .gap = 2, .pl = 4, .align = "cl", .w = "grow", .hType = RC_PX(ROW_H),
          .className = "hover:bg-surface-alt") {
        (*open ? rcIconChevronDown : rcIconChevronRight)(16.0f, t->text);
        label(title, SZ_MICRO, t->text);
    }
    if (rcClicked(id))
        *open = !*open;
}

static inline void collapse_all(Workspace *ws) {
    int i;

    for (i = 0; i < ws->count; i++)
        ws->node[i].open = false;
    tree_rows(ws);
}

static inline void explorer_view(AppState *st) {
    const Palette *t = theme();

    rcRow(.pl = 20, .pr = 8, .align = "cl", .w = "grow", .hType = RC_PX(PANEL_TAB_H)) {
        label("EXPLORER", SZ_MICRO, t->muted);
        rcSpacer() {}
        /* A ghost button shows only under the pointer, as a toolbar's "more" does. */
        if (rcBeginMenu("explorer_menu", "", .variant = RC_BTN_GHOST,
                        .leadingIcon = rcIconEllipsisVertical)) {
            st->menuOpen = true;
            if (rcMenuItem("Open Folder..."))
                post_command(st, CMD_OPEN_FOLDER);
            if (rcMenuItem("Collapse Folders"))
                collapse_all(&st->ws);
            if (rcMenuItem("Refresh", .disabled = st->ws.samples))
                post_command(st, CMD_REFRESH);
            rcEndMenu();
        }
    }
    fold_head("root_head", upper(st->arena, st->ws.rootName), &st->rootOpen);
    rcColumn(.w = "grow", .h = "grow") {
        if (st->rootOpen)
            tree_view(st);
    }
    hline();
    fold_head("outline_head", "OUTLINE", &st->outlineOpen);
    if (st->outlineOpen) {
        /* A share of the column rather than a fixed height: a short window keeps its tree. */
        rcColumn(.w = "grow", .h = "28%") { outline_view(st); }
    }
}

/* ---- Find in files. ---- */

static inline void result_row(AppState *st, int index, const char *id) {
    const Palette *t = theme();
    Search        *s = &st->search;
    const Result  *r = &s->results[index];
    const Node    *n = &st->ws.node[r->node];

    /* Its file was read again this frame: the next frame searches the new bytes. The row still
       takes its place, as every row of a virtual list is rowHeight tall, or later rows shift. */
    if (!n->read || r->at + r->len > n->size) {
        rcBox(.w = "grow", .hType = RC_PX(list_row_h(st))) {}
        return;
    }
    rcRow(.id = id, .gap = 6, .pl = (float)(r->head ? 8 : 28), .pr = 16, .align = "cl",
          .overflow = "hidden", .w = "grow", .hType = RC_PX(list_row_h(st)),
          .className = "hover:bg-surface-alt") {
        if (r->head) {
            file_mark_box(n->name, 16.0f);
            label(n->name, SZ_UI, t->bright);
            rcRow(.overflow = "hidden", .w = "grow") {
                label_cut(n->path, SZ_SMALL, t->muted);
            }
            rcBox(.bg = t->badge, .px = 6, .borderRadius = "all-full") {
                rcText(rcFormat(st->arena, "%d", r->matches), .color = t->text, .size = SZ_MICRO,
                       .wrap = "n", .select = RC_SELECT_NONE);
            }
        } else {
            /* The line from a little before the match, the match on a colored wash. */
            const char *line = n->bytes + r->at;
            int         from = r->col > 16 ? r->col - 16 : 0, qn = str_len(s->seen);
            RC_String   pre  = { false, r->col - from, line + from };
            RC_String   hit  = { false, qn, line + r->col };
            RC_String   post = { false, r->len - r->col - qn, line + r->col + qn };

            while (from < r->col && is_blank(line[from])) {
                from++;
                pre.chars++;
                pre.length--;
            }
            if (post.length > 200)
                post.length = 200;
            rcRow(.align = "cl", .overflow = "hidden", .w = "grow") {
                rcText(pre, .color = t->text, .size = SZ_UI, .wrap = "n", .select = RC_SELECT_NONE);
                rcBox(.bg = t->match) {
                    rcText(hit, .color = t->bright, .size = SZ_UI, .wrap = "n",
                           .select = RC_SELECT_NONE);
                }
                rcText(post, .color = t->text, .size = SZ_UI, .wrap = "n",
                       .select = RC_SELECT_NONE);
            }
        }
    }
    /* Posted, as a button's command is: opening may read the file again and free the bytes
       this frame's rows above, this one included, still draw from. */
    if (rcClicked(id) && !r->head) {
        st->openNode = r->node;
        st->openLine = r->line;
        st->openCol  = r->col;
        post_command(st, CMD_OPEN_NODE);
    }
}

static inline void search_view(AppState *st) {
    const Palette *t = theme();
    Search        *s = &st->search;

    if (!same_text(s->query, s->seen) || st->ws.reread)
        search_start(st);
    search_step(st);
    section_head("SEARCH");
    rcRow(.gap = 4, .px = 12, .align = "cl", .w = "grow") {
        rcBox(.w = "grow") {
            /* A change, the x's emptying too, restarts the walk at the next frame's top: ask
               for it. */
            if (rcTextInput("search_q", s->query, (int)sizeof s->query,
                            .className = control_h(st, "h-7"),
                            .placeholder = "Search in files", .leadingIcon = rcIconSearch,
                            .clearButton = true))
                rcWindowRequestFrame(rcAppMainWindow(st->app));
        }
        if (toggle_chip("search_case", "Aa", s->matchCase, "Match case")) {
            s->matchCase = !s->matchCase;
            search_start(st);
        }
    }
    rcRow(.px = 14, .align = "cl", .w = "grow", .hType = RC_PX(28)) {
        if (s->query[0]) {
            rcText(rcFormat(st->arena, s->running ? "Searching... %d results in %d files"
                                                  : "%d results in %d files",
                            s->matches, s->filesHit),
                   .color = t->muted, .size = SZ_SMALL, .wrap = "n", .select = RC_SELECT_NONE);
        } else {
            label("Type to search every file in the folder.", SZ_SMALL, t->muted);
        }
    }
    rcColumn(.id = "results", .scroll = "v", .w = "grow", .h = "grow") {
        rcVirtualList(row, "results", s->n, list_row_h(st), .overscan = 3) {
            result_row(st, row.index, rcFormat(st->arena, "res%d", row.index).chars);
        }
    }
    rcScrollbar("results");
}

/* ---- Tabs and the breadcrumb. ---- */

static inline void tab_item(AppState *st, int index) {
    const Palette *t = theme();
    Doc           *d = &st->docs[index];
    const char    *id = rcFormat(st->arena, "tab-%s", d->scrollId).chars;
    const char    *xid = rcFormat(st->arena, "tabx-%s", d->scrollId).chars;
    const char    *ctx = rcFormat(st->arena, "tabc-%s", d->scrollId).chars;
    bool           on = index == st->active;
    bool           over = rcShowsHover(id);
    bool           dirty = doc_dirty(d);
    bool           pick, shut;

    rcColumn(.id = id, .bg = on ? t->editor : RC_UNSET_COLOR, .h = "grow",
             .className = "tab-rest") {
        rcBox(.bg = on ? t->accent : RC_UNSET_COLOR, .w = "grow", .hType = RC_PX(2)) {}
        rcRow(.gap = 6, .pl = 10, .pr = 6, .align = "cl", .h = "grow") {
            file_mark_box(d->name, 16.0f);
            label(d->name, SZ_UI, on ? t->bright : t->muted);
            rcBox(.id = xid, .align = "cc", .borderRadius = "all-sm", .wType = RC_PX(20),
                  .hType = RC_PX(20), .tooltip = "Close (" PRIMARY_KEY "+W)",
                  .className = "tab-close") {
                if (dirty && !rcShowsHover(xid))
                    rcBox(.bg = t->text, .borderRadius = "all-full", .wType = RC_PX(8),
                          .hType = RC_PX(8)) {}
                else if (on || over || rcPointerIsCoarse())
                    rcIconX(14.0f, t->text);
            }
        }
    }
    rcBox(.bg = t->sidebar, .h = "grow", .wType = RC_PX(1)) {}
    shut = rcClicked(xid) || (over && rcPointerPressed(RC_POINTER_MIDDLE));
    pick = rcClicked(id);
    if (rcBeginContextMenu(ctx, id)) {
        st->menuOpen = true;
        if (rcMenuItem("Close"))
            shut = true;
        if (rcMenuItem("Close Others")) {
            rcStrCopy(st->closeTab, d->scrollId, sizeof st->closeTab);
            post_command(st, CMD_CLOSE_OTHERS);
        }
        if (rcMenuItem("Copy Path", .disabled = !d->path[0]))
            rcClipboardSet(d->path);
        rcEndContextMenu();
    }
    /* By id: a close that runs before this one, a save's answer or a second X in the frame,
       moves every tab after it, and an index would then name the wrong file. */
    if (shut) {
        rcStrCopy(st->closeTab, d->scrollId, sizeof st->closeTab);
        post_command(st, CMD_CLOSE_INDEX);
    } else if (pick) {
        st->active = index;
        st->wantCaret = !rcPointerIsCoarse();
    }
}

static inline void tab_strip(AppState *st) {
    const Palette *t = theme();
    const Doc     *d = active_doc(st);
    int            i;

    /* Another tab came to the front: once it has a box, the strip shows it whole. */
    if (d && !same_text(st->tabShown, d->scrollId)) {
        const char *id = rcFormat(st->arena, "tab-%s", d->scrollId).chars;

        if (rcGetElementBox(id).found) {
            rcScrollIntoView("tabs", id, RC_SCROLL_NEAREST);
            rcStrCopy(st->tabShown, d->scrollId, sizeof st->tabShown);
        }
        rcWindowRequestFrame(rcAppMainWindow(st->app));
    }
    rcRow(.id = "tabs", .bg = t->sidebar, .scroll = "h", .w = "grow", .hType = RC_PX(TAB_H)) {
        for (i = 0; i < st->nDocs; i++)
            tab_item(st, i);
    }
    rcScrollbar("tabs");
}

/* The path, a piece per folder, then the symbol the caret is in. */
static inline void breadcrumb(const Doc *d, bool compact) {
    const Palette *t = theme();
    const char    *p = d->path[0] ? d->path : d->name;
    const Symbol  *sym = d->unreadable ? NULL : symbol_at(d, doc_line_of(d, d->caret));

    rcRow(.bg = t->editor, .gap = 2, .pl = 14, .align = "cl", .overflow = "hidden", .w = "grow",
          .hType = RC_PX(CRUMB_H)) {
        while (*p) {
            int n = 0;
            RC_String piece;

            while (p[n] && p[n] != '/' && p[n] != '\\')
                n++;
            piece = RC_LIT(RC_String){ false, n, p };
            /* An absolute path starts with a separator; a phone names the file alone. */
            if (n && (!compact || !p[n])) {
                rcText(piece, .color = p[n] ? t->muted : t->text, .size = SZ_SMALL, .wrap = "n",
                       .select = RC_SELECT_NONE);
                if (p[n] || sym)
                    rcIconChevronRight(14.0f, t->muted);
            }
            p += n + (p[n] != 0);
        }
        if (sym) {
            RC_String name = { false, sym->nameLen, d->text + sym->name };

            rcText(name, .color = t->text, .size = SZ_SMALL, .wrap = "n", .select = RC_SELECT_NONE);
        }
    }
}

/* ---- The find bar. ---- */

static inline void find_bar(AppState *st, float width) {
    const Palette *t = theme();
    Find          *f = &st->find;
    bool           shift = rcModDown(RC_MOD_SHIFT);

    if (rcKeyPressedOn("find_q", RC_KEY_ENTER))
        post_command(st, shift ? CMD_FIND_PREV : CMD_FIND_NEXT);
    if (rcKeyPressedOn("find_r", RC_KEY_ENTER))
        post_command(st, CMD_REPLACE_ONE);
    rcRow(.id = "find_bar", .bg = t->widget, .gap = 4, .p = 4, .align = "tl",
          .borderRadius = "all-md", .border = { .color = t->edge, .width = 1 },
          .wType = RC_PX(width),
          .shadow = { .color = rcWithAlpha(RC_BLACK, 0.353f), .y = 4, .blur = 12 },
          .floating = { .to = RC_ATTACH_ELEMENT, .toId = PANE_ID, .parent = RC_ANCHOR_TOP_RIGHT,
                        .element = RC_ANCHOR_TOP_RIGHT, .offset = { -16.0f, 0.0f },
                        .zIndex = 40 }) {
        if (icon_chip("find_more", f->replaceOpen ? rcIconChevronDown : rcIconChevronRight, false,
                      "Toggle Replace"))
            f->replaceOpen = !f->replaceOpen;
        rcColumn(.gap = 4, .w = "grow") {
            rcRow(.gap = 4, .align = "cl", .w = "grow") {
                rcBox(.w = "grow") {
                    /* The hits follow the query at the next frame's top: ask for it. */
                    if (rcTextInput("find_q", f->query, (int)sizeof f->query,
                                    .className = control_h(st, "h-[26px]"), .placeholder = "Find",
                                    .clearButton = true))
                        rcWindowRequestFrame(rcAppMainWindow(st->app));
                }
                if (toggle_chip("find_case", "Aa", f->matchCase, "Match Case"))
                    f->matchCase = !f->matchCase;
                if (toggle_chip("find_word", "ab", f->wholeWord, "Match Whole Word"))
                    f->wholeWord = !f->wholeWord;
                rcBox(.align = "cc", .wType = RC_PX(72)) {
                    if (!f->query[0])
                        label("No results", SZ_SMALL, t->muted);
                    else if (!f->nHits)
                        label("No results", SZ_SMALL, t->error);
                    else
                        rcText(rcFormat(st->arena, f->nHits == MAX_HITS ? "%d of %d+" : "%d of %d",
                                        f->current + 1, f->nHits),
                               .color = t->text, .size = SZ_SMALL, .wrap = "n",
                               .select = RC_SELECT_NONE);
                }
                if (icon_chip("find_prev", rcIconArrowUp, !f->nHits, "Previous Match (Shift+F3)"))
                    post_command(st, CMD_FIND_PREV);
                if (icon_chip("find_next", rcIconArrowDown, !f->nHits, "Next Match (F3)"))
                    post_command(st, CMD_FIND_NEXT);
                if (icon_chip("find_close", rcIconX, false, "Close (Esc)")) {
                    f->open = false;
                    st->wantCaret = !rcPointerIsCoarse();
                }
            }
            if (f->replaceOpen) {
                rcRow(.gap = 4, .align = "cl", .w = "grow") {
                    rcBox(.w = "grow") {
                        rcTextInput("find_r", f->with, (int)sizeof f->with,
                                    .className = control_h(st, "h-[26px]"),
                                    .placeholder = "Replace",
                                    .clearButton = true);
                    }
                    if (rcButton("find_one", "Replace",
                                 .disabled = !f->nHits, .className = control_h(st, "h-[26px]")))
                        post_command(st, CMD_REPLACE_ONE);
                    if (rcButton("find_all", "All",
                                 .disabled = !f->nHits, .className = control_h(st, "h-[26px]")))
                        post_command(st, CMD_REPLACE_ALL);
                }
            }
        }
    }
}

/* ---- The SVG preview. ---- */

static const RC_Color SVG_TINTS[] = {
    { 212, 212, 212, 255 }, { 55, 148, 255, 255 }, { 78, 201, 176, 255 },
    { 220, 220, 170, 255 }, { 206, 145, 120, 255 }, { 241, 76, 76, 255 },
};

static inline void checker(int cols, int rows, RC_Color dark) {
    int y, x;

    for (y = 0; y < rows; y++) {
        rcRow(.gap = 12, .pl = (float)(y % 2 ? 12 : 0), .hType = RC_PX(12)) {
            for (x = y % 2; x < cols; x += 2)
                rcBox(.bg = dark, .wType = RC_PX(12), .hType = RC_PX(12)) {}
        }
    }
}

static inline bool segment(const char *id, const char *text, bool on, const char *tip) {
    const Palette *t = theme();

    rcBox(.id = id, .bg = on ? t->accent : RC_UNSET_COLOR, .px = 8, .align = "cc",
          .borderRadius = "all-sm", .hType = RC_PX(24), .tooltip = tip,
          .className = "segment-rest") {
        label(text, SZ_SMALL, on ? RC_WHITE : t->text);
    }
    return rcClicked(id);
}

/* The art itself. The HANDLE route draws this buffer, parsed once per edit (check_svg), so a
   number you type moves the drawing. The PATH route draws the file by name, as <img src> does,
   and rcSvg caches it for the run. Nothing else differs between them. */
static inline void svg_art(AppState *st, Doc *d, RC_Color tint) {
    const Palette *t = theme();

    if (st->route == ROUTE_PATH && d->path[0] && !d->memoryOnly)
        rcSvg(d->path, st->svgSize, tint);
    else if (d->svg)
        rcSvgHandle(d->svg, st->svgSize, tint);
    else
        label("This markup does not parse as an SVG yet.", SZ_UI, t->error);
}

static inline void svg_preview(AppState *st, Doc *d) {
    const Palette *t = theme();
    RC_Box         art = rcGetElementBox("svg_art");
    RC_Color       ground = st->ground == GROUND_LIGHT ? rcRgb(245, 245, 245) : rcRgb(24, 24, 24);
    RC_Color       dark = st->ground == GROUND_CHECKER ? t->tabOff : ground;
    RC_Color       tint = SVG_TINTS[st->tint];
    int            i;

    rcColumn(.bg = t->sidebar, .gap = 12, .pb = 12, .h = "grow",
             .wType = st->compact ? RC_GROW : RC_PX(PREVIEW_W)) {
        rcRow(.gap = 4, .pl = 12, .pr = 8, .align = "cl", .w = "grow",
              .hType = RC_PX(PANEL_TAB_H)) {
            label("PREVIEW", SZ_MICRO, t->muted);
            rcSpacer() {}
            if (segment("route_handle", "Buffer", st->route == ROUTE_HANDLE,
                        "This buffer, unsaved edits included"))
                st->route = ROUTE_HANDLE;
            if (!d->memoryOnly && d->path[0]
                && segment("route_path", "Disk", st->route == ROUTE_PATH,
                           "The file as first read this run: rcSvg caches it by path"))
                st->route = ROUTE_PATH;
        }
        rcColumn(.id = "svg_art", .bg = st->ground == GROUND_CHECKER ? t->editor : ground,
                 .overflow = "hidden", .w = "grow", .h = "grow") {
            if (art.found && st->ground == GROUND_CHECKER)
                checker((int)(art.width / 12.0f) + 1, (int)(art.height / 12.0f) + 1, dark);
            rcBox(.floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_CENTER,
                                .element = RC_ANCHOR_CENTER, .capture = RC_CAPTURE_PASSTHROUGH,
                                .clip = RC_CLIP_TO_PARENT }) {
                svg_art(st, d, tint);
            }
        }
        rcColumn(.gap = 8, .px = 12, .w = "grow") {
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                label("Size", SZ_SMALL, t->text);
                rcBox(.w = "grow") {
                    rcSlider("svg_size", &st->svgSize, 16.0f, 256.0f, .step = 1.0f);
                }
                rcText(rcFormat(st->arena, "%d px", (int)st->svgSize), .font = st->mono,
                       .color = t->muted, .size = SZ_SMALL, .wrap = "n", .select = RC_SELECT_NONE);
            }
            rcRow(.gap = 6, .align = "cl", .w = "grow") {
                label("Color", SZ_SMALL, t->text);
                for (i = 0; i < COUNT_OF(SVG_TINTS); i++) {
                    const char *id = rcFormat(st->arena, "tint%d", i).chars;

                    rcBox(.id = id, .bg = SVG_TINTS[i], .borderRadius = "all-sm",
                          .border = { .color = i == st->tint ? t->bright : t->edge,
                                      .width = 2 },
                          .wType = RC_PX(st->compact ? 36 : 20),
                          .hType = RC_PX(st->compact ? 36 : 20)) {}
                    if (rcClicked(id))
                        st->tint = i;
                }
            }
            rcRow(.gap = 4, .align = "cl", .w = "grow") {
                label("Ground", SZ_SMALL, t->text);
                rcBox(.wType = RC_PX(4)) {}
                if (segment("ground_checker", "Checker", st->ground == GROUND_CHECKER, NULL))
                    st->ground = GROUND_CHECKER;
                if (segment("ground_dark", "Dark", st->ground == GROUND_DARK, NULL))
                    st->ground = GROUND_DARK;
                if (segment("ground_light", "Light", st->ground == GROUND_LIGHT, NULL))
                    st->ground = GROUND_LIGHT;
            }
            rcBox(.bg = t->editor, .p = 8, .borderRadius = "all-sm", .w = "grow") {
                if (st->route == ROUTE_PATH && !d->memoryOnly && d->path[0])
                    rcText(rcFormat(st->arena, "rcSvg(\"%s\", %d, color);", d->path,
                                    (int)st->svgSize),
                           .font = st->mono, .color = t->syntax[TOK_FUNCTION], .size = SZ_SMALL,
                           .select = RC_SELECT_NONE);
                else
                    rcText(rcFormat(st->arena, "rcSvgHandle(svg, %d, color);", (int)st->svgSize),
                           .font = st->mono, .color = t->syntax[TOK_FUNCTION], .size = SZ_SMALL,
                           .select = RC_SELECT_NONE);
            }
        }
    }
}

/* ---- The empty editor. ---- */

static inline void start_row(AppState *st, Command c) {
    const Palette *t = theme();
    const char    *id = rcFormat(st->arena, "start%d", (int)c).chars;

    rcRow(.id = id, .gap = 24, .px = 8, .align = "cl", .borderRadius = "all-sm", .w = "grow",
          .hType = RC_PX(st->compact ? HIT : 28), .className = "hover:bg-surface-alt") {
        label(command_label(c), SZ_UI, t->text);
        rcSpacer() {}
        label(command_keys(st->arena, c), SZ_SMALL, t->muted);
    }
    if (rcClicked(id))
        post_command(st, c);
}

static inline void welcome_view(AppState *st) {
    const Palette *t = theme();

    rcColumn(.bg = t->editor, .gap = 16, .align = "cc", .w = "grow", .h = "grow") {
        rcColumn(.gap = 4, .wType = RC_PX(360)) {
            rcRow(.gap = 12, .pb = 16, .align = "cl") {
                rcSvg(RC_EX28_ICON_DIR "rayclay-logo.svg", 40.0f, t->muted);
                rcColumn(.gap = 2) {
                    label("No editor is open", SZ_HEAD, t->bright);
                    label("Start here, or pick a file from the explorer.", SZ_SMALL, t->muted);
                }
            }
            start_row(st, CMD_QUICK_OPEN);
            start_row(st, CMD_PALETTE);
            start_row(st, CMD_NEW_FILE);
            start_row(st, CMD_OPEN_FILE);
            start_row(st, CMD_OPEN_FOLDER);
        }
    }
}

/* ---- The bottom panel. ---- */

static inline void severity_icon(int sev, float size) {
    const Palette *t = theme();

    if (sev == SEV_ERROR)
        rcIconCircleX(size, t->error);
    else if (sev == SEV_WARNING)
        rcIconTriangleAlert(size, t->warning);
    else
        rcIconInfo(size, t->info);
}

/* The filter's test, case-blind. */
static inline bool text_contains(const char *hay, const char *needle) {
    int n = str_len(needle);

    if (!n)
        return true;
    return find_in_line(hay, str_len(hay), needle, n, false, 0) >= 0;
}

static inline void problems_view(AppState *st) {
    const Palette *t = theme();
    int            shown = 0, i, k;

    rcColumn(.id = "problems", .scroll = "v", .w = "grow", .h = "grow") {
        for (i = 0; i < st->nDocs; i++) {
            Doc *d = &st->docs[i];
            int  hits = 0;

            for (k = 0; k < d->nProb; k++)
                hits += text_contains(d->prob[k].msg, st->problemFilter);
            if (hits) {
                rcRow(.gap = 6, .pl = 12, .align = "cl", .w = "grow", .hType = RC_PX(ROW_H)) {
                    file_mark_box(d->name, 16.0f);
                    label(d->name, SZ_UI, t->text);
                    label(d->path, SZ_SMALL, t->muted);
                    rcBox(.bg = t->badge, .px = 6, .borderRadius = "all-full") {
                        rcText(rcFormat(st->arena, "%d", hits), .color = t->text, .size = SZ_MICRO,
                               .wrap = "n", .select = RC_SELECT_NONE);
                    }
                }
            }
            for (k = 0; k < d->nProb; k++) {
                const Problem *p = &d->prob[k];
                const char    *id;

                if (!text_contains(p->msg, st->problemFilter))
                    continue;
                id = rcFormat(st->arena, "pr%d_%d", i, k).chars;
                shown++;
                rcRow(.id = id, .gap = 8, .pl = 34, .pr = 12, .align = "cl", .overflow = "hidden",
                      .w = "grow", .hType = RC_PX(list_row_h(st)),
                      .className = "hover:bg-surface-alt") {
                    severity_icon(p->severity, 15.0f);
                    label(p->msg, SZ_UI, t->text);
                    rcText(rcFormat(st->arena, "[Ln %d, Col %d]", p->line + 1, p->col + 1),
                           .color = t->muted, .size = SZ_SMALL, .wrap = "n",
                           .select = RC_SELECT_NONE);
                }
                if (rcClicked(id)) {
                    st->active = i;
                    go_to(st, d, p->line, p->col);
                    st->screen = SCREEN_CODE;
                }
            }
        }
        if (!shown && st->started) {
            rcRow(.pl = 20, .align = "cl", .w = "grow", .hType = RC_PX(32)) {
                label(st->problemFilter[0] ? "No problems match the filter."
                                           : "No problems have been detected in the open files.",
                      SZ_UI, t->muted);
            }
        }
    }
    rcScrollbar("problems");
}

static inline void output_view(AppState *st) {
    const Palette *t = theme();
    int            i;

    rcColumn(.id = "output", .py = 6, .scroll = "v", .scrollStickToEnd = true,
             .w = "grow", .h = "grow") {
        for (i = 0; i < st->nLog; i++) {
            const LogLine *l = &st->log[i];

            rcRow(.gap = 12, .px = (float)(st->compact ? 8 : 20), .align = "tl", .w = "grow") {
                rcText(rcFormat(st->arena, "%7.1fs", (double)l->time), .font = st->mono,
                       .color = t->muted, .size = SZ_SMALL, .lineHeight = 18, .wrap = "n");
                rcTextC(l->level == SEV_ERROR ? "error"
                        : l->level == SEV_WARNING ? "warn " : "info ",
                        .font = st->mono, .color = l->level == SEV_ERROR ? t->error
                        : l->level == SEV_WARNING ? t->warning : t->info, .size = SZ_SMALL,
                        .lineHeight = 18, .wrap = "n");
                rcBox(.w = "grow") {
                    rcTextC(l->text, .font = st->mono, .color = t->text, .size = SZ_SMALL,
                            .lineHeight = 18);
                }
            }
        }
    }
    rcScrollbar("output");
}

static inline int problem_total(const AppState *st) {
    int i, n = 0;

    for (i = 0; i < st->nDocs; i++)
        n += st->docs[i].nProb;
    return n;
}

static inline void panel_tab(AppState *st, const char *id, const char *title, int tab, int badge) {
    const Palette *t  = theme();
    bool           on = st->panelTab == tab;

    rcColumn(.id = id, .px = 10, .align = "tc", .h = "grow") {
        rcRow(.gap = 6, .align = "cc", .h = "grow") {
            label(title, SZ_MICRO, on || rcShowsHover(id) ? t->bright : t->muted);
            if (badge > 0) {
                rcBox(.bg = t->badge, .px = 6, .borderRadius = "all-full") {
                    rcText(rcFormat(st->arena, "%d", badge), .color = t->text, .size = SZ_MICRO,
                           .wrap = "n", .select = RC_SELECT_NONE);
                }
            }
        }
        rcBox(.bg = on ? t->accent : RC_UNSET_COLOR, .w = "grow", .hType = RC_PX(1)) {}
    }
    if (rcClicked(id))
        st->panelTab = tab;
}

static inline void bottom_panel(AppState *st) {
    const Palette *t = theme();

    rcColumn(.bg = t->editor, .w = "grow", .h = "grow", .className = "min-w-0") {
        rcBox(.bg = t->edge, .w = "grow", .hType = RC_PX(1)) {}
        /* At least a tab strip tall: a finger's filter field (control_h) makes it taller. */
        rcRow(.gap = 4, .pl = 8, .pr = 8, .align = "cl", .w = "grow",
              .hMin = RC_VAL(PANEL_TAB_H)) {
            panel_tab(st, "panel_problems", "PROBLEMS", PANEL_PROBLEMS, problem_total(st));
            panel_tab(st, "panel_output", "OUTPUT", PANEL_OUTPUT, 0);
            if (!st->compact)
                rcSpacer() {}
            if (st->panelTab == PANEL_PROBLEMS) {
                rcBox(.w = "grow", .wMax = RC_VAL(220)) {
                    /* No frame to ask for: the list below reads the filter after the field. */
                    rcTextInput("problem_filter", st->problemFilter, (int)sizeof st->problemFilter,
                                .className = control_h(st, "h-6"), .placeholder = "Filter",
                                .clearButton = true);
                }
            }
            /* A phone's panel is a screen of its own: the bottom navigation leaves it. */
            if (!st->compact
                && icon_chip("panel_close", rcIconX, false, "Close Panel (" PRIMARY_KEY "+J)"))
                st->panelOpen = false;
        }
        if (st->panelTab == PANEL_PROBLEMS)
            problems_view(st);
        else
            output_view(st);
    }
}

/* ---- The status bar. ---- */

/* A status item lights in the theme's primary-hover role, the palette's statusOn. */
static inline bool status_item(const char *id, const char *text, const char *tip) {
    rcRow(.id = id, .px = 8, .align = "cl", .h = "grow", .tooltip = tip,
          .className = "hover:bg-primary-hover") {
        label(text, SZ_SMALL, theme()->statusText);
    }
    return rcClicked(id);
}

/* A fact, not a control: it takes no hover and no click. */
static inline void status_text(const char *text) {
    rcRow(.px = 8, .align = "cl", .h = "grow") { label(text, SZ_SMALL, theme()->statusText); }
}

static inline void status_bar(AppState *st) {
    const Palette *t = theme();
    Doc           *d = active_doc(st);
    int            errors = 0, warnings = 0, infos = 0, i;

    for (i = 0; i < st->nDocs; i++) {
        errors   += st->docs[i].errors;
        warnings += st->docs[i].warnings;
        infos    += st->docs[i].nProb - st->docs[i].errors - st->docs[i].warnings;
    }
    rcRow(.bg = t->status, .pl = 6, .pr = 6, .align = "cl", .w = "grow", .hType = RC_PX(STATUS_H)) {
        const char *id = "status_problems";

        rcRow(.id = id, .gap = 4, .px = 6, .align = "cl", .h = "grow",
              .tooltip = "Problems (" PRIMARY_KEY "+Shift+M)",
              .className = "hover:bg-primary-hover") {
            rcIconCircleX(14.0f, t->statusText);
            rcText(rcFormat(st->arena, "%d", errors), .color = t->statusText, .size = SZ_SMALL,
                   .wrap = "n", .select = RC_SELECT_NONE);
            rcIconTriangleAlert(14.0f, t->statusText);
            rcText(rcFormat(st->arena, "%d", warnings), .color = t->statusText, .size = SZ_SMALL,
                   .wrap = "n", .select = RC_SELECT_NONE);
            rcIconInfo(14.0f, t->statusText);
            rcText(rcFormat(st->arena, "%d", infos), .color = t->statusText, .size = SZ_SMALL,
                   .wrap = "n", .select = RC_SELECT_NONE);
        }
        if (rcClicked(id))
            post_command(st, CMD_PROBLEMS);
        rcSpacer() {}
        if (d && !d->unreadable) {
            int ln = doc_line_of(d, d->caret), col = 1, k;

            for (k = d->line[ln]; k < d->caret; k++)
                col += !is_tail_byte(d->text[k]);
            if (status_item("status_pos", has_sel(d) && !st->compact
                            ? rcFormat(st->arena, "Ln %d, Col %d (%d selected)", ln + 1, col,
                                       sel_end(d) - sel_start(d)).chars
                            : rcFormat(st->arena, "Ln %d, Col %d", ln + 1, col).chars,
                            "Go to Line (" PRIMARY_KEY "+G)"))
                post_command(st, CMD_GOTO_LINE);
            /* A phone keeps the language and gives up what is only worth a glance on a desk. */
            if (!st->compact) {
                status_text(d->tabs ? "Tabs" : "Spaces: 4");
                status_text("UTF-8");
                status_text(d->crlf ? "CRLF" : "LF");
            }
            status_text(lang_label(d->name));
        }
    }
}

/* ---- A phone's bottom navigation: the rail, moved under the thumb. ---- */

/* `count` above 0 hangs on the icon's corner, as the desktop panel's tab shows it. */
static inline void nav_item(AppState *st, const char *id, RC_IconCallback icon, const char *text,
                            int screen, int count) {
    const Palette *t  = theme();
    bool           on = st->screen == screen
                        || (screen == SCREEN_PROBLEMS && st->screen == SCREEN_OUTPUT);
    RC_Color       c  = on ? t->railOn : t->railOff;

    rcColumn(.id = id, .gap = 2, .align = "cc", .w = "grow", .h = "grow") {
        rcBox() {
            icon(22.0f, c);
            if (count > 0) {
                rcBox(.bg = t->badge, .px = 5, .align = "cc", .borderRadius = "all-full",
                      .hType = RC_PX(16), .wMin = RC_VAL(16.0f),
                      .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_RIGHT,
                                    .element = RC_ANCHOR_CENTER, .offset = { 4.0f, 2.0f } }) {
                    rcText(rcFormat(st->arena, "%d", count), .color = t->text, .size = SZ_MICRO,
                           .wrap = "n", .select = RC_SELECT_NONE);
                }
            }
        }
        label(text, SZ_MICRO, c);
    }
    if (rcClicked(id)) {
        st->screen = screen;
        if (screen == SCREEN_PROBLEMS)
            st->panelTab = PANEL_PROBLEMS;
    }
}

static inline void bottom_nav(AppState *st, float safeBottom) {
    const Palette *t = theme();

    rcRow(.bg = t->activity, .pb = (float)safeBottom, .w = "grow",
          .hType = RC_PX((float)NAV_H + safeBottom)) {
        nav_item(st, "nav_code", rcIconFileText, "Code", SCREEN_CODE, 0);
        nav_item(st, "nav_files", rcIconList, "Files", SCREEN_FILES, 0);
        nav_item(st, "nav_find", rcIconSearch, "Search", SCREEN_SEARCH, 0);
        nav_item(st, "nav_problems", rcIconCircleAlert, "Problems", SCREEN_PROBLEMS,
                 problem_total(st));
    }
}

#endif /* APP_PANELS_H */
