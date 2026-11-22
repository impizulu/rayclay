/*
    preview.h - what a file looks like before it is opened

    One piece of art and one block of facts, shown in three places: the last pane of the
    column view, the info pane beside the list, and Quick Look. A picture shows its pixels at
    once, a document its text on a sheet of paper, anything else its large icon.
*/
#ifndef APP_PREVIEW_H
#define APP_PREVIEW_H

#include "rayclay.h"
#include "app/theme.h"
#include "app/browser.h"
#include "app/parts.h"

/* The bundled mono face, read from disk the first time a code file is previewed. One bake
   serves every size this app draws it at. If the file is missing it is the default face. */
static inline uint16_t mono_face(AppState *st) {
    if (!st->monoTried) {
        st->monoTried = true;
        st->mono      = rcLoadFont(FONT_MONO_TTF, (float)TEXT_BODY);
    }
    return st->mono;
}

/* A row of a sample's table: the name takes what the numbers leave, and each number lines up
   on the right, however wide its digits are. The cells borrow the sample's own bytes. */
static inline void paper_row(const char *line, const char *end, int size, bool strong) {
    float numberW = (float)size * 4.6f;
    bool  first   = true;

    rcRow(.gap = 6, .py = 2, .w = "grow") {
        while (line <= end) {
            const char *stop = line;
            RC_String   cell = rcStringFromCStr(line);

            while (stop < end && *stop != '\t') stop++;
            cell.length = (int)(stop - line);
            rcBox(.align = first ? "cl" : "cr", .overflow = "hidden",
                  .wType = first ? RC_GROW : RC_PX(numberW)) {
                rcText(cell, .font = (uint16_t)(strong ? rcFontSemibold((float)size) : 0),
                       .color = ui.ink, .size = (uint16_t)size, .wrap = "e",
                       .select = RC_SELECT_NONE);
            }
            first = false;
            line  = stop + 1;
        }
    }
}

/* A sample document's first page: real text on a sheet, cut where the sheet ends. A line holding
   tabs is a row of a table, whose head and whose total read stronger. A document's first line
   is its title; code and plain text are set in the mono face. */
static inline void paper_sheet(AppState *st, const Node *n, float width, float height, int size) {
    bool        mono = n->kind == KIND_CODE || n->kind == KIND_TEXT;
    const char *line = n->body;
    bool        title = !mono, head = true;

    rcColumn(.bg = RC_WHITE, .p = (float)(width / 12.0f), .overflow = "hidden",
             .borderRadius = "all-xs", .border = { .color = ui.paperLine, .width = 1 },
             .wType = RC_PX(width), .hType = RC_PX(height),
             .shadow = { .color = rcRgb(0, 0, 0, 36 / 255.0f), .y = 2.0f, .blur = 10.0f }) {
        while (*line) {
            const char *end = line;
            bool        row = false;

            for (; *end && *end != '\n'; end++) row = row || *end == '\t';
            if (end == line) {
                rcBox(.hType = RC_PX(size / 2 + 2)) {}
            } else if (row) {
                bool total = line[0] == 'T' && line[1] == 'o' && line[2] == 't';

                paper_row(line, end, size, head || total);
                if (head) rule();
                head = false;
            } else {
                RC_String text = rcStringFromCStr(line);

                text.length = (int)(end - line);
                rcText(text, .font = (uint16_t)(mono ? mono_face(st)
                                                     : title ? rcFontSemibold((float)size) : 0),
                       .color = ui.ink, .size = (uint16_t)size, .select = RC_SELECT_NONE);
                title = false;
            }
            line = *end ? end + 1 : end;
        }
    }
}

/* The art, as large as fits @p width by @p height and never stretched. */
static inline void preview_art(AppState *st, int id, float width, float height) {
    const Node     *n   = node_of(st, id);
    const RC_Image *img = n->isDir ? NULL : node_image(st, id);

    if (img && img->handle) {
        float h = width * (float)img->height / (float)img->width;
        float w = width;

        if (h > height) {
            w = height * (float)img->width / (float)img->height;
            h = height;
        }
        rcBox(.bg = ui.well, .borderRadius = "all-sm", .wType = RC_PX(w), .hType = RC_PX(h),
              .shadow = { .color = rcRgb(0, 0, 0, 40 / 255.0f), .y = 1.0f, .blur = 6.0f },
              .image = img, .imageFit = RC_IMAGE_COVER) {}
    } else if (n->body) {
        /* A page stands upright; a spreadsheet's lies on its side, to give its columns room. */
        float aspect = n->kind == KIND_SHEET ? 1.0f / 0.77f : 0.77f;
        float paperH = height, paperW = paperH * aspect;

        if (paperW > width) {
            paperW = width;
            paperH = paperW / aspect;
        }
        paper_sheet(st, n, paperW, paperH, width >= 500.0f ? TEXT_BODY : TEXT_CAPTION);
    } else {
        float side = height < width ? height : width;

        file_icon(n, side > 128.0f ? 128.0f : side, false);
    }
}

/* A folder's item count, the way its own rows count: hidden names stay out. */
static inline int child_count(AppState *st, int dir) {
    const Node *d = node_of(st, dir);
    int         id, n = 0;

    if (d->smart != SMART_NONE) {
        for (id = 0; id < st->tree.high; id++) n += smart_takes(st, d, id);
        return n;
    }
    for (id = d->first; id >= 0; id = node_of(st, id)->next)
        n += !hidden_name(st, node_of(st, id));
    return n;
}

/* One fact: its label to the left, its value to the right, as the Finder's panel sets them. */
static inline void fact(const char *label, const char *value) {
    rcRow(.gap = 12, .w = "grow") {
        rcBox(.wType = RC_PX(72)) {
            rcTextC(label, .color = ui.muted, .size = TEXT_SMALL, .select = RC_SELECT_NONE);
        }
        rcBox(.w = "grow") {
            rcTextC(value, .color = ui.ink, .size = TEXT_SMALL, .select = RC_SELECT_NONE);
        }
    }
}

/* The seven tag dots: a click sets the tag, another clears it. Tags live on the sample
   drive only, as no RayClay call stores one with a real file. @p scope keeps these ids apart
   from the same row's under an open dialog. */
static inline void tag_picker(AppState *st, RC_Arena *a, int id, const char *scope) {
    Node *n = node_of(st, id);
    int   t;

    rcRow(.gap = 6, .align = "cl", .w = "grow") {
        for (t = TAG_RED; t < TAG_END; t++) {
            const char *dot = rcFormat(a, "%s_tag%d", scope, t).chars;
            bool        on  = n->tag == t;

            rcBox(.id = dot, .align = "cc", .borderRadius = "all-full",
                  .border = { .color = on ? ui.ink : RC_UNSET_COLOR, .width = 2 },
                  .wType = RC_PX(finger() ? 32 : 20), .hType = RC_PX(finger() ? 32 : 20),
                  .tooltip = node_of(st, st->tags[t])->name) {
                tag_dot(t, on || rcShowsHover(dot) ? 14.0f : 12.0f, false);
            }
            if (rcClicked(dot)) {
                n->tag    = on ? TAG_NONE : (int8_t)t;
                st->dirty = true;
            }
        }
    }
}

/* The folders above an item, as the Finder's panel words them: a line that wraps at its
   spaces, where a path has none to wrap at. Measured, then written from its end into one
   allocation: a text per level would cost the frame arena the square of the depth, and a
   folder tree, or a link to its own folder, can go hundreds of levels down. */
static inline const char *where_text(AppState *st, RC_Arena *a, int id) {
    int   len = -3, at, k, j;
    char *out;

    for (k = id; k >= 0; k = node_of(st, k)->parent) len += text_len(node_of(st, k)->name) + 3;
    out = (char *)rcArenaAlloc(a, (size_t)len + 1);
    if (!out) return "";
    at      = len;
    out[at] = '\0';
    for (k = id; k >= 0; k = node_of(st, k)->parent) {
        const char *name = node_of(st, k)->name;

        at -= text_len(name);
        for (j = 0; name[j]; j++) out[at + j] = name[j];
        if (at > 0) {
            out[--at] = ' ';
            out[--at] = '>';
            out[--at] = ' ';
        }
    }
    return out;
}

/* The block of facts below the art: only what is known. A real folder not listed yet has no
   count to give, and a fixed place has no date or parent, so it has no Information at all. */
static inline void preview_facts(RC_App *app, AppState *st, int id, const char *scope) {
    RC_Arena       *a   = rcAppArena(app);
    const Node     *n   = node_of(st, id);
    const RC_Image *img = n->isDir ? NULL : node_image(st, id);
    bool            counted = !n->real || (n->wasListed && n->listed == RC_FILE_OK);
    int             items   = n->isDir && counted ? child_count(st, id) : 0;
    bool            facts   = n->modified > 0.0 || (img && img->handle) || n->parent >= 0;
    bool            tags    = !n->real && n->parent >= 0 && !in_trash(st, id);

    /* With nothing under them to line up with, the name and count sit under the art's middle. */
    rcColumn(.gap = 2, .align = facts || tags ? "tl" : "tc", .w = "grow") {
        rcText(shown_name(n), .font = rcFontSemibold(TEXT_TITLE), .color = ui.ink);
        rcText(!n->isDir ? rcFormat(a, "%s  -  %s", n->kindName, size_text(a, n))
                         : counted ? rcFormat(a, "Folder  -  %d item%s", items,
                                              items == 1 ? "" : "s")
                                   : rcFormat(a, "Folder"),
               .color = ui.muted, .size = TEXT_SMALL, .select = RC_SELECT_NONE);
    }
    if (facts) {
        rule();
        rcColumn(.gap = 6, .w = "grow") {
            section_title("Information", ui.ink);
            if (n->modified > 0.0) fact("Modified", date_text(a, n->modified, true));
            if (n->scene >= 0) {
                const Scene *pic = &SCENES[n->scene];

                fact("Dimensions", rcFormat(a, "%d x %d", pic->fileW, pic->fileH).chars);
            } else if (img && img->handle) {
                fact("Dimensions", rcFormat(a, "%d x %d", img->width, img->height).chars);
            }
            if (n->parent >= 0) fact("Where", where_text(st, a, n->parent));
        }
    }
    if (tags) {
        rule();
        rcColumn(.gap = 6, .w = "grow") {
            section_title("Tags", ui.ink);
            tag_picker(st, a, id, scope);
        }
    }
}

/* The last pane of the column view: the moment a file is chosen, it is there. However wide the
   pane, its block keeps a reading width, in the middle. A folder's block is centered with the
   safe "Cc": on a short window it starts at the top, where the scroll can reach it. */
static inline void preview_column(RC_App *app, AppState *st, int id, float paneW) {
    float blockW = paneW - 32.0f > (float)PREVIEW_BLOCK ? (float)PREVIEW_BLOCK : paneW - 32.0f;
    float artH   = rcViewport().height - 360.0f;
    const Node *n = node_of(st, id);

    if (artH > 360.0f) artH = 360.0f;
    if (artH < 120.0f) artH = 120.0f;
    rcColumn(.id = "preview_col", .p = 16, .align = n->isDir ? "Cc" : "tc",
             .scroll = "v", .h = "grow", .wType = RC_PX(paneW)) {
        rcColumn(.gap = 12, .align = "tc", .wType = RC_PX(blockW)) {
            preview_art(st, id, blockW, artH);
            preview_facts(app, st, id, "pane");
            if (id == st->cursor && (!n->isDir || can_rename(st, id))) {
                rcRow(.gap = 8, .w = "grow") {
                    if (!n->isDir && rcButton("peek_open", "Quick Look", .className = button_h()))
                        st->peek = true;
                    if (can_rename(st, id)
                        && rcButton("peek_rename", "Rename", .className = button_h()))
                        start_rename(st, id);
                }
            }
        }
    }
    rcScrollbar("preview_col");
}

/* The info pane, beside the list and the grid: the selected item, or the folder itself
   when nothing is selected, as the Finder's preview pane does. A search's results are not
   the folder's, so with none of them selected the pane counts them instead. */
static inline void info_pane(RC_App *app, AppState *st) {
    RC_Arena *a     = rcAppArena(app);
    int       count = selected_count(st);
    int       id    = st->cursor >= 0 ? st->cursor : st->folder;
    float     width = (float)INFO_W - 32.0f;

    rcColumn(.id = "info", .gap = 12, .p = 16, .align = "tc", .scroll = "v", .h = "grow",
             .wType = RC_PX(INFO_W)) {
        if (count > 1) {
            rcColumn(.gap = 8, .align = "cc", .w = "grow", .hType = RC_PX(200)) {
                rcIconFolderTile(96.0f);
                rcText(rcFormat(a, "%d items selected", count), .font = rcFontSemibold(TEXT_TITLE),
                       .color = ui.ink, .select = RC_SELECT_NONE);
            }
        } else if (st->cursor < 0 && st->search[0]) {
            rcColumn(.gap = 8, .align = "cc", .w = "grow", .hType = RC_PX(200)) {
                rcIconSearch(48.0f, ui.faint);
                rcText(rcFormat(a, "%d result%s", st->rowCount, st->rowCount == 1 ? "" : "s"),
                       .font = rcFontSemibold(TEXT_TITLE), .color = ui.ink,
                       .select = RC_SELECT_NONE);
                rcText(rcFormat(a, "in %s", node_of(st, search_scope(st))->name), .color = ui.muted,
                       .size = TEXT_SMALL, .textAlign = "c", .select = RC_SELECT_NONE);
            }
        } else {
            rcBox(.align = "cc", .w = "grow", .hType = RC_PX(200)) {
                preview_art(st, id, width, 200.0f);
            }
            preview_facts(app, st, id, "info");
        }
    }
    rcScrollbar("info");
}

/* Get Info: the same art and facts in a dialog, for the views with no pane beside them. */
static inline void info_dialog(RC_App *app, AppState *st) {
    RC_Viewport vp = rcViewport();
    float       panelW = vp.width - 48.0f > 340.0f ? 340.0f : vp.width - 48.0f;
    float       artH   = vp.height - 420.0f > 180.0f ? 180.0f : vp.height - 420.0f;

    if (!st->infoOpen) return;
    if (artH < 72.0f) artH = 72.0f;
    if (rcBeginModal("getinfo", &st->infoOpen)) {
        rcColumn(.gap = 12, .wType = RC_PX(panelW)) {
            rcRow(.align = "cl", .w = "grow") {
                rcBox(.w = "grow") {
                    rcTextL("Info", .font = rcFontSemibold(TEXT_BODY), .color = ui.muted,
                            .select = RC_SELECT_NONE);
                }
                if (tool_button("info_close", rcIconX, "Close", false, false))
                    st->infoOpen = false;
            }
            rcBox(.align = "cc", .w = "grow", .hType = RC_PX(artH)) {
                preview_art(st, st->infoNode, panelW, artH);
            }
            preview_facts(app, st, st->infoNode, "dialog");
        }
        rcEndModal();
    }
}

/* Quick Look: Space on anything, the file's art as large as the window allows. Esc and the
   scrim close it, which the modal does by itself; the arrows keep moving underneath
   (app.h), so a folder of pictures can be flipped through. What has no art but its icon, a
   folder above all, gets a panel only as wide as the icon and its facts, side by side as the
   Finder sets them, rather than a large empty sheet. */
static inline void quick_look(RC_App *app, AppState *st) {
    RC_Viewport     vp = rcViewport();
    float           panelW, artH, widest;
    const Node     *n;
    const RC_Image *img;
    bool            art;

    if (st->cursor < 0) st->peek = false;
    if (!st->peek) return;
    n      = node_of(st, st->cursor);
    img    = n->isDir ? NULL : node_image(st, st->cursor);
    art    = (img && img->handle) || n->body;
    widest = art ? 760.0f : 520.0f;
    panelW = vp.width - 96.0f > widest ? widest : vp.width - 96.0f;
    artH   = vp.height - 200.0f > 520.0f ? 520.0f : vp.height - 200.0f;
    if (artH < 120.0f) artH = 120.0f;
    if (rcBeginModal("quicklook", &st->peek)) {
        rcColumn(.gap = 12, .wType = RC_PX(panelW)) {
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                file_icon(n, 16.0f, false);
                rcBox(.overflow = "hidden", .w = "grow") {
                    rcText(shown_name(n), .font = rcFontSemibold(TEXT_BODY), .color = ui.ink,
                           .wrap = "e", .select = RC_SELECT_NONE);
                }
                if (!n->isDir)
                    rcTextC(size_text(rcAppArena(app), n), .color = ui.muted, .size = TEXT_SMALL,
                            .select = RC_SELECT_NONE);
                if (tool_button("peek_close", rcIconX, "Close", false, false))
                    st->peek = false;
            }
            if (art) {
                rcBox(.align = "cc", .w = "grow", .hType = RC_PX(artH)) {
                    preview_art(st, st->cursor, panelW, artH);
                }
            } else {
                /* On a narrow panel, a phone's, the tags have no room beside the icon, so
                   the facts go under it, as the info pane sets them. */
                bool narrow = panelW < 400.0f;

                rcBox(.gap = (float)(narrow ? 12 : 20), .w = "grow",
                      .className = narrow ? "flex-col items-center" : "flex-row items-center") {
                    file_icon(n, narrow ? 64.0f : 128.0f, false);
                    rcColumn(.gap = 12, .w = "grow") {
                        preview_facts(app, st, st->cursor, "peek");
                    }
                }
            }
        }
        rcEndModal();
    }
}

#endif /* APP_PREVIEW_H */
