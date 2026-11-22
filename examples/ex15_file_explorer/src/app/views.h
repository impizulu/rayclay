/*
    views.h - the three ways to look at a folder

    Icons, list and columns, each a virtual list over the same rows (browser.h), so a folder
    of four thousand entries declares what fits in the window. Every row reads rcClicked for
    selection and rcIsHovered for the context menu, whose single body is in app.h.
*/
#ifndef APP_VIEWS_H
#define APP_VIEWS_H

#include "rayclay.h"
#include "app/theme.h"
#include "app/browser.h"
#include "app/parts.h"
#include "app/preview.h"

/* ---- Pointer: the same on every row ---- */

/* A click on a row in the list or the grid: Finder's modifiers, then opening. A finger has
   no double click: one tap opens, and a long press gives the menu. rcClickCount is the
   click's count in a row, as the web's event.detail: 2 is a double click. */
static inline void row_clicked(AppState *st, int id) {
    bool twice = rcClickCount() == 2;

    if (finger()) {
        open_node(st, id);
    } else if (rcModDown(RC_MOD_SHIFT)) {
        select_range(st, id);
    } else if (rcModDown(RC_MOD_PRIMARY)) {
        node_of(st, id)->selected = !node_of(st, id)->selected;
        st->cursor = node_of(st, id)->selected ? id : -1;
        st->anchor = id;
    } else {
        select_only(st, id);
        if (twice) open_node(st, id);
    }
}

/* Keep the row the keyboard moved to in view. A virtual list declares only the rows it
   shows, so rcScrollToRow places it from the list's own geometry, its padding included. */
static inline void reveal_row(const char *scrollId, int index) {
    if (index >= 0) rcScrollToRow(scrollId, index, RC_SCROLL_NEAREST);
}

/* The inline rename field, in place of a name, with the search's clear x once there is
   text: a press on the x leaves the focus in the field, so the rename goes on. Enter and Esc
   are read in browser.h. The row sets the height, so its class is formatted into the frame
   arena, as a row's id is. */
static inline void rename_field(AppState *st, RC_Arena *a, int height) {
    rcBox(.w = "grow") {
        rcTextInput("rename", st->renameBuf, (int)sizeof st->renameBuf,
                    .className = rcFormat(a, "h-[%dpx] px-1 rounded-sm border", height).chars,
                    .placeholder = node_of(st, st->renaming)->name, .clearButton = true);
    }
}

/* A row's icon. A sample picture shows its own pixels, cut square, as a file manager's
   list does; anything else keeps its glyph, and so does a real picture: the app decodes
   only the one being looked at (browser.h). */
static inline void row_art(AppState *st, const Node *n, float size, bool selected) {
    const RC_Image *img = n->scene >= 0 ? scene_image(st, n->scene) : NULL;

    if (img && img->handle) {
        rcBox(.borderRadius = "all-xs", .wType = RC_PX(size), .hType = RC_PX(size),
              .image = img, .imageFit = RC_IMAGE_COVER) {}
    } else {
        file_icon(n, size, selected);
    }
}

/* ---- List view ---- */

/* One sortable heading: the current sort reads heavier and carries its arrow. The first
   click on a heading sorts by it, the next one flips it. */
static inline void list_heading(AppState *st, const char *id, const char *label, SortKey key,
                                int width, bool right) {
    bool on = st->sort == key;

    rcRow(.id = id, .gap = 4, .px = 6, .align = right ? "cr" : "cl", .overflow = "hidden",
          .wType = width ? RC_PX(width) : RC_GROW, .hType = RC_GROW, .className = "wash") {
        rcTextC(label, .font = (uint16_t)(on ? rcFontSemibold(TEXT_SMALL) : 0),
                .color = on ? ui.ink : ui.muted, .size = TEXT_SMALL, .wrap = "n",
                .select = RC_SELECT_NONE);
        if (on) (st->sortDown ? rcIconChevronDown : rcIconChevronUp)(11.0f, ui.muted);
    }
    if (rcClicked(id)) {
        st->sortDown = on ? !st->sortDown : key == SORT_DATE || key == SORT_SIZE;
        st->sort     = key;
        st->dirty    = true;
    }
}

/* The tip for what the width hid. It ends the Name heading, whose least width holds it, so
   it covers no other heading and takes no column of its own. The columns leave in one order,
   so their count says which. */
static inline void columns_tip(int count, RC_Arena *a) {
    static const char *const TIP[4] = {
        "", "Kind is hidden at this width", "Size and Kind are hidden at this width",
        "Date Modified, Size and Kind are hidden at this width"
    };

    rcBox(.px = 6, .align = "cc", .h = "grow") {
        rcBox(.bg = ui.segTrack, .px = 8, .py = 2, .borderRadius = "all-full",
              .tooltip = TIP[count]) {
            rcText(rcFormat(a, "+%d column%s", count, count == 1 ? "" : "s"),
                   .color = ui.muted, .size = TEXT_CAPTION, .wrap = "n", .select = RC_SELECT_NONE);
        }
    }
}

/* The sticky header is a row of its own above the table's scrolling body, drawn by hand for
   what a table's header cannot hold: a sort chevron, a heavier label on the sorted column and
   a hover per heading. It shows the columns the table did not hide, at their widths, and
   ends with the band the body's scrollbar takes under a mouse while the rows overflow, so
   each heading stays over its column. The Name heading's label stands over the rows' icons,
   which a short row sets past its inset and triangle and a tall row past its padding alone.
   @p side holds the safe insets at the view's sides: the band runs into them and pads its
   headings by them, as the rows' box pads the rows. */
static inline void list_header(AppState *st, uint64_t hidden, bool tall, RC_Arena *a,
                               RC_Insets side) {
    bool band  = !finger() && rcGetScrollInfo("list").maxOffsetY > 0.0f;
    int  count = 0, k;

    for (k = 1; k < 4; k++) count += (int)(hidden >> k & 1u);
    rcRow(.bg = ui.barBg, .pl = 8 + side.left, .pr = (float)(band ? 20 : 8) + side.right,
          .align = "cl", .w = "grow", .hType = RC_PX(finger() ? HIT : 26)) {
        rcRow(.pl = (float)(tall ? 2 : 16), .w = "grow", .h = "grow") {
            list_heading(st, "sort_name", "Name", SORT_NAME, 0, false);
            if (count) columns_tip(count, a);
        }
        if (!tall) {
            if (!(hidden & 2u))
                list_heading(st, "sort_date", "Date Modified", SORT_DATE, DATE_W, false);
            if (!(hidden & 4u))
                list_heading(st, "sort_size", "Size", SORT_SIZE, SIZE_W, true);
            if (!(hidden & 8u))
                list_heading(st, "sort_kind", "Kind", SORT_KIND, KIND_W, false);
        }
    }
    rule();
}

/* A folder's triangle. It takes its own clicks: a press goes to the innermost element polled
   under the pointer, so the row around it takes the rest. */
static inline void disclosure(AppState *st, RC_Arena *a, int id, RC_Color color) {
    Node *n = node_of(st, id);

    if (!n->isDir || browsing_flat(st)) {
        rcBox(.h = "grow", .wType = RC_PX(18)) {}
        return;
    }
    {
        const char *tri = rcFormat(a, "tri%d", id).chars;

        rcBox(.id = tri, .align = "cc", .h = "grow", .wType = RC_PX(18)) {
            (n->expanded ? rcIconCaretDown : rcIconCaretRight)(11.0f, color);
        }
        if (rcClicked(tri)) {
            n->expanded = !n->expanded;
            st->dirty   = true;
        }
    }
}

/* A column's cell in a mouse's row: one line, cut with an ellipsis where it runs long. */
static inline void list_cell(const char *text, const char *align, RC_Color color, float rowH) {
    rcBox(.px = 6, .align = align, .overflow = "hidden", .w = "grow", .hType = RC_PX(rowH)) {
        rcTextC(text, .color = color, .size = TEXT_BODY, .wrap = "e", .select = RC_SELECT_NONE);
    }
}

/* A mouse's row: a line per file with its columns, zebra-striped like the Finder's. Every
   column gets its cell, so a value never lands under the wrong heading; a hidden one takes
   no room, and nothing need be drawn in it. */
static inline void list_row(RC_App *app, AppState *st, int index, uint64_t hidden, float rowH) {
    RC_Arena   *a   = rcAppArena(app);
    int         id  = st->rows[index];
    Node       *n   = node_of(st, id);
    const char *row = rcFormat(a, "row%d", id).chars;
    bool        on  = n->selected;
    RC_Color    ink = on ? RC_WHITE : ui.ink, sub = on ? RC_WHITE : ui.muted;

    rcTableRow(.id = row, .className = on          ? "rounded-sm bg-primary"
                                       : index % 2 ? "rounded-sm zebra wash-blue"
                                                   : "rounded-sm wash-blue");
    rcRow(.pl = (float)(4 + 16 * st->depth[index]), .align = "cl", .w = "grow",
          .hType = RC_PX(rowH)) {
        disclosure(st, a, id, on ? RC_WHITE : ui.faint);
        row_art(st, n, 16.0f, on);
        rcRow(.gap = 6, .pl = 6, .pr = 6, .align = "cl", .overflow = "hidden", .w = "grow",
              .h = "grow") {
            if (st->renaming == id) {
                rename_field(st, a, (int)rowH - 2);
            } else {
                line_fill(shown_name(n), TEXT_BODY, ink);
                if (n->tag) tag_dot(n->tag, 8.0f, on);
            }
        }
    }
    rcTableNext();
    if (!(hidden & 2u)) list_cell(date_text(a, n->modified, true), "cl", sub, rowH);
    rcTableNext();
    if (!(hidden & 4u)) list_cell(size_text(a, n), "cr", sub, rowH);
    rcTableNext();
    if (!(hidden & 8u)) list_cell(n->kindName, "cl", sub, rowH);
    if (rcIsHovered(row)) st->hover = id;
    if (rcClicked(row)) row_clicked(st, id);
}

/* A finger's row, or a compact window's: the columns fold into a second line. */
static inline void list_row_tall(RC_App *app, AppState *st, int index, float rowH) {
    RC_Arena   *a   = rcAppArena(app);
    int         id  = st->rows[index];
    Node       *n   = node_of(st, id);
    const char *row = rcFormat(a, "row%d", id).chars;
    bool        on  = n->selected;
    RC_Color    ink = on ? RC_WHITE : ui.ink, sub = on ? RC_WHITE : ui.muted;

    rcTableRow(.id = row, .className = on ? "rounded-md bg-primary" : "rounded-md wash");
    rcRow(.gap = 12, .px = 8, .align = "cl", .w = "grow", .hType = RC_PX(rowH)) {
        row_art(st, n, 32.0f, on);
        rcColumn(.gap = 2, .overflow = "hidden", .w = "grow") {
            if (st->renaming == id) {
                rename_field(st, a, 24);
            } else {
                rcRow(.gap = 6, .align = "cl", .overflow = "hidden", .w = "grow") {
                    line_fill(shown_name(n), TEXT_ROOMY, ink);
                    if (n->tag) tag_dot(n->tag, 8.0f, on);
                }
            }
            rcBox(.overflow = "hidden", .w = "grow") {
                rcText(n->isDir ? rcFormat(a, "%s", date_text(a, n->modified, false))
                                : rcFormat(a, "%s  -  %s", date_text(a, n->modified, false),
                                           size_text(a, n)),
                       .color = sub, .size = TEXT_SMALL, .wrap = "e", .select = RC_SELECT_NONE);
            }
        }
        if (n->isDir) rcIconChevronRight(16.0f, on ? RC_WHITE : ui.faint);
    }
    if (rcIsHovered(row)) st->hover = id;
    if (rcClicked(row)) row_clicked(st, id);
}

/* The rows are a table's: Name keeps NAME_MIN while .priority hides Kind, then Size, then
   Date as the view narrows, and rcTableHiddenColumns says which, so the header and the tip
   follow it. Tall rows fold the columns into a second line, so they take the first one
   alone, hide none and need no tip. */
static inline void list_view(RC_App *app, AppState *st, bool tall, RC_Insets side) {
    bool           rows2  = tall || finger();
    float          rowH   = rows2 ? 56.0f : 24.0f;
    /* Read ahead of the table, this is the last frame's answer, the one on screen. */
    uint64_t       hidden = rows2 ? 0 : rcTableHiddenColumns("list");
    RC_TableColumn cols[4] = {
        { .w = "grow", .wMin = RC_VAL(NAME_MIN) },
        { .wType = RC_PX(DATE_W), .priority = 1 },
        { .wType = RC_PX(SIZE_W), .priority = 2 },
        { .wType = RC_PX(KIND_W), .priority = 3 },
    };

    rcColumn(.w = "grow", .h = "grow") {
        list_header(st, hidden, rows2, rcAppArena(app), side);
        /* A table's body takes no padding, so the rows' inset is this box's. None at the
           foot: there it would stop a scrolled list short of the status bar. */
        rcBox(.pt = 4, .pl = 8 + side.left, .pr = 8 + side.right, .w = "grow", .h = "grow") {
            if (rcBeginTable("list", cols, rows2 ? 1 : 4, .cellPadding = RC_VAL(0))) {
                /* Past rcBeginTable the answer is this frame's: the cells follow it. */
                hidden = rows2 ? 0 : rcTableHiddenColumns("list");
                rcVirtualList(row, "list", st->rowCount, rowH) {
                    if (rows2) list_row_tall(app, st, row.index, rowH);
                    else       list_row(app, st, row.index, hidden, rowH);
                }
                rcEndTable();
            }
            if (st->revealCursor) reveal_row("list", row_index(st, st->cursor));
            rcScrollbar("list");
        }
    }
}

/* ---- Icon view ---- */

/* How many tiles a row of this width holds; the tiles then share the width, so the grid
   always fills the window and never strands a gap on the right. */
static inline int grid_columns(const AppState *st, float width) {
    int n = (int)((width - 24.0f) / (st->iconSize + (float)TILE_PAD_W));

    return n < 1 ? 1 : n;
}

/* A tile's picture: a sample's own pixels stand in for its icon, as the Finder's do. */
static inline void tile_art(AppState *st, int id, float size) {
    const Node     *n   = node_of(st, id);
    const RC_Image *img = n->scene >= 0 ? scene_image(st, n->scene) : NULL;

    if (img && img->handle) {
        rcBox(.align = "cc", .wType = RC_PX(size), .hType = RC_PX(size)) {
            rcBox(.bg = ui.well, .borderRadius = "all-xs",
                  .border = { .color = ui.paperLine, .width = 1 },
                  .wType = RC_PX(size),
                  .hType = RC_PX(size * (float)img->height / (float)img->width),
                  .shadow = { .color = rcRgb(0, 0, 0, 40 / 255.0f), .y = 1.0f, .blur = 3.0f },
                  .image = img, .imageFit = RC_IMAGE_COVER) {}
        }
    } else {
        file_icon(n, size, false);
    }
}

static inline void icon_tile(RC_App *app, AppState *st, int index, float tileW, float tileH) {
    RC_Arena   *a    = rcAppArena(app);
    int         id   = st->rows[index];
    Node       *n    = node_of(st, id);
    const char *tile = rcFormat(a, "tile%d", id).chars;
    bool        on   = n->selected;
    bool        oneLong = !contains(n->name, " ")
                          && rcTextWidth(0, TEXT_SMALL, n->name, text_len(n->name)) > tileW - 20.0f;

    rcColumn(.id = tile, .gap = 4, .pt = 4, .align = "tc", .overflow = "hidden",
             .wType = RC_PX(tileW), .hType = RC_PX(tileH)) {
        /* The art's backdrop lights while the pointer is anywhere on the tile, its name too:
           the look of one box from another's state, which a class does not reach, so it is
           read from rcShowsHover. */
        rcBox(.bg = on ? ui.tileOn : rcShowsHover(tile) ? ui.hover : RC_UNSET_COLOR, .p = 4,
              .borderRadius = "all-lg") {
            tile_art(st, id, st->iconSize);
            /* On the art's corner, where a name of any length cannot move it. */
            if (n->tag) {
                rcBox(.floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_BOTTOM_RIGHT,
                                    .element = RC_ANCHOR_BOTTOM_RIGHT, .offset = { -2.0f, -2.0f },
                                    .capture = RC_CAPTURE_PASSTHROUGH }) {
                    tag_dot(n->tag, 12.0f, true);
                }
            }
        }
        if (st->renaming == id) {
            rcBox(.px = 6, .w = "grow") { rename_field(st, a, 22); }
        } else {
            /* A name wraps at its spaces, to two lines. One word too long for the tile has
               nowhere to break, so it fills the pill and ends in an ellipsis. */
            rcRow(.bg = on ? ui.select : RC_UNSET_COLOR, .px = 6, .py = 1, .align = "cc",
                  .overflow = "hidden", .borderRadius = "all-sm",
                  .wType = oneLong ? RC_PX(tileW - 8.0f) : RC_FIT, .wMax = RC_VAL(tileW - 8.0f)) {
                if (oneLong) {
                    line_fill(shown_name(n), TEXT_SMALL, on ? RC_WHITE : ui.ink);
                } else {
                    rcText(shown_name(n), .color = on ? RC_WHITE : ui.ink, .size = TEXT_SMALL,
                           .textAlign = "c", .select = RC_SELECT_NONE, .lineClamp = 2);
                }
            }
        }
    }
    if (rcIsHovered(tile)) st->hover = id;
    if (rcClicked(tile)) row_clicked(st, id);
}

static inline void icon_view(RC_App *app, AppState *st, float width) {
    int   cols  = grid_columns(st, width);
    float tileW = (width - 24.0f) / (float)cols;
    float tileH = st->iconSize + (float)TILE_PAD_H;
    int   lines = (st->rowCount + cols - 1) / cols;

    rcColumn(.id = "grid", .pt = 8, .pb = 8, .pl = 12, .pr = 12, .scroll = "v",
             .w = "grow", .h = "grow") {
        rcVirtualList(line, "grid", lines, tileH) {
            rcRow(.w = "grow", .hType = RC_PX(tileH)) {
                int i;

                for (i = line.index * cols; i < (line.index + 1) * cols && i < st->rowCount; i++)
                    icon_tile(app, st, i, tileW, tileH);
            }
        }
    }
    if (st->revealCursor) reveal_row("grid", row_index(st, st->cursor) / cols);
    rcScrollbar("grid");
}

/* ---- Column view ---- */

/* A row of one column. The chosen path reads gray, and the item under the keyboard reads in
   the selection's blue. */
static inline void column_row(RC_App *app, AppState *st, int k, int id, bool onPath) {
    RC_Arena   *a   = rcAppArena(app);
    Node       *n   = node_of(st, id);
    const char *row = rcFormat(a, "c%d", id).chars;
    bool        cur = st->cursor == id;
    bool        menu = st->menuOpen && st->menuTarget == id;
    float       h   = finger() ? (float)HIT : 24.0f;

    /* The context menu's row wears a ring where a right click cannot select it (app.h). */
    rcRow(.id = row, .gap = 6, .pl = 6, .pr = 4, .align = "cl", .borderRadius = "all-sm",
          .border = { .color = menu ? ui.select : RC_UNSET_COLOR, .width = 2 }, .w = "grow",
          .hType = RC_PX(h), .className = cur ? "bg-primary" : onPath ? "chosen" : "wash") {
        row_art(st, n, 16.0f, cur);
        if (st->renaming == id) {
            rename_field(st, a, (int)h - 2);
        } else {
            line_fill(shown_name(n), TEXT_BODY, cur ? RC_WHITE : ui.ink);
            if (n->tag) tag_dot(n->tag, 8.0f, cur);
        }
        /* A file keeps the chevron's room, so every tag dot of a column stands on one line. */
        if (n->isDir) rcIconChevronRight(12.0f, cur ? RC_WHITE : ui.faint);
        else          rcBox(.wType = RC_PX(12)) {}
    }
    if (rcIsHovered(row)) {
        st->hover       = id;
        st->hoverColumn = k;
    }
    if (rcClicked(row)) {
        bool twice = rcClickCount() == 2;

        pick_in_column(st, k, id);
        if (twice && !n->isDir) open_node(st, id);
    }
}

/* The sentence where a folder has no rows. */
static inline void folder_empty(const AppState *st, int id) {
    const Node *dir  = &st->tree.nodes[id];
    const char *line = id == st->trash ? "The Trash is empty." : "This folder is empty.";

    if (dir->real && dir->listed == RC_FILE_DENIED)
        line = "You don't have permission to see this folder.";
    else if (dir->real && dir->listed != RC_FILE_OK)
        line = "This folder could not be read.";
    rcColumn(.gap = 8, .px = 16, .align = "cc", .w = "grow", .h = "grow") {
        rcTextC(line, .color = ui.muted, .size = TEXT_BODY, .textAlign = "c",
                .select = RC_SELECT_NONE);
    }
}

/* Above a search's results: where it looked, and the other place it could. Under a finger a
   chip fills the bar's HIT of height. */
static inline void scope_chip(AppState *st, const char *id, const char *label, bool on, bool here) {
    rcBox(.id = id, .px = 8, .py = 2, .align = "cl", .overflow = "hidden",
          .borderRadius = "all-md", .h = finger() ? "grow" : "fit", .wMax = RC_VAL(180.0f),
          .className = on ? "chosen" : "wash") {
        rcTextC(label, .font = (uint16_t)(on ? rcFontSemibold(TEXT_SMALL) : 0),
                .color = on ? ui.ink : ui.muted, .size = TEXT_SMALL, .wrap = "e",
                .select = RC_SELECT_NONE);
    }
    if (rcClicked(id) && !on) {
        st->searchHere = here;
        st->dirty      = true;
    }
}

static inline void scope_bar(AppState *st, RC_Insets side) {
    int root = search_root(st);

    rcRow(.bg = ui.barBg, .gap = 4, .pl = 12 + side.left, .pr = 12 + side.right, .align = "cl",
          .overflow = "hidden", .w = "grow", .hType = RC_PX(finger() ? HIT : 32)) {
        rcTextL("Search:", .color = ui.muted, .size = TEXT_SMALL, .wrap = "n",
                .select = RC_SELECT_NONE);
        scope_chip(st, "scope_all", node_of(st, root)->name, !st->searchHere || st->folder == root,
                   false);
        if (st->folder != root)
            scope_chip(st, "scope_here", node_of(st, st->folder)->name, st->searchHere, true);
        rcSpacer() {}
        if (node_of(st, root)->real) {
            rcTextL("Folders opened so far", .color = ui.muted, .size = TEXT_CAPTION, .wrap = "n",
                    .select = RC_SELECT_NONE);
        }
    }
    rule();
}

/* A column view always shows whole columns: they narrow to make room, and past that the ones
   that do not fit leave whole off the left. @p fit is how many stand beside the preview. */
static inline float column_width(int colCount, float width, int *fit) {
    float room = width - (float)PREVIEW_MIN;
    float w;

    *fit = (int)(room / (float)(COLUMN_MIN + 1));
    if (*fit < 1) *fit = 1;
    if (*fit > colCount) *fit = colCount;
    w = room / (float)*fit - 1.0f;
    return w > (float)COLUMN_W ? (float)COLUMN_W : w;
}

static inline void column_view(RC_App *app, AppState *st, float width) {
    RC_Arena *a     = rcAppArena(app);
    float     rowH  = finger() ? (float)HIT : 24.0f;
    /* With nothing chosen the last pane describes the folder, so it never stands empty. */
    int       shown = st->cursor >= 0 ? st->cursor : st->folder;
    int       fit, k;
    float     colW  = column_width(st->colCount, width, &fit);
    /* The preview takes what the columns leave. A fixed width: a "grow" pane in a sideways
       scroller would be as wide as its longest line. */
    float     paneW = width - (float)fit * (colW + 1.0f);

    rcRow(.id = "columns", .scroll = "h", .w = "grow", .h = "grow") {
        for (k = 0; k < st->colCount; k++) {
            const Column *col  = &st->cols[k];
            const char   *cid  = rcFormat(a, "col%d", k).chars;
            int           next = k + 1 < st->colCount ? st->cols[k + 1].dir : -1;

            rcColumn(.id = cid, .pt = 4, .pb = 4, .pl = 6, .pr = 6, .scroll = "v",
                     .h = "grow", .wType = RC_PX(colW)) {
                if (col->count) {
                    rcVirtualList(row, cid, col->count, rowH) {
                        int id = st->rows[col->start + row.index];

                        column_row(app, st, k, id, id == next);
                    }
                } else {
                    folder_empty(st, col->dir);
                }
            }
            if (st->revealCursor && next < 0) {
                int i;

                for (i = 0; i < col->count; i++)
                    if (st->rows[col->start + i] == st->cursor) reveal_row(cid, i);
            }
            rcScrollbar(cid);
            vline();
        }
        preview_column(app, st, shown, paneW);
    }
    /* A click in a column changes the folder after the columns were built, and they are
       rebuilt on the next frame, so this waits one frame (the end of layout() counts it down).
       The call itself is resolved after this frame's layout, on the columns declared here. */
    if (st->revealColumns == 1) rcScrollIntoView("columns", "preview_col", RC_SCROLL_END);
    rcScrollbar("columns");
}

#endif /* APP_VIEWS_H */
