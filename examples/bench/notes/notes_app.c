/*
    notes_app.c - the notes app's GUI.

    RayClay types only: no <system> include (raw-memory needs route through
    notes_backend.h) and no rcFormat outside the demo overlay - every dynamic
    string is a backend fixed buffer and every element id comes from a static
    table.

    The palette is the app's own: warm paper, ink, a quiet rail and one
    restrained crimson. A writing tool is judged as a page before it is judged as
    a program, so the editor is a sheet - no field boxes, no rules, one muted
    metadata line - and the chrome is the two columns beside it.
*/
#define NOTES_BACKEND_IMPLEMENTATION
#include "notes_app.h"

#include "icons/rc_icons_settings.h"
#include "icons/rc_icons_panel_left.h"   /* the compact arm's back control */
#include "icons/rc_icons_x.h"            /* a document tab's close control */
#include "icons/rc_icons_house.h"        /* the collections rail           */
#include "icons/rc_icons_folder.h"
#include "icons/rc_icons_chart_column.h"

/* Warmup frames, then the scene HOLDS. Retune with the coordinates below. */
#define NOTES_BENCH_WARMUP 64

/* HOW THE THREE PANES GIVE WAY, derived from this app's own columns rather than a
   table of device widths. The editorial column is the one that must not lose width,
   because its F_TITLE heading is the widest single line in the app, so the note list
   yields first: below NOTES_LIST_SLIM_AT it drops to its slim width, and only when
   even that is not enough do the list and the editor TAKE TURNS - one pane at a
   time, never a squeezed version of both, with the rail's controls folded into the
   list header. The test is the SPACE, never the platform. */
#define NOTES_RAIL_W        64.0f
#define NOTES_LIST_W       320.0f
#define NOTES_LIST_SLIM_W  240.0f
#define NOTES_LIST_SLIM_AT 1000.0f
#define NOTES_EDITOR_MIN_W 495.0f
#define NOTES_ONE_PANE_W   (NOTES_RAIL_W + NOTES_LIST_SLIM_W + 1.0f + NOTES_EDITOR_MIN_W)

/* The demo perf chip floats and must cover no datum, so it takes whichever of three
   places has room: the desktop's bottom-right corner, the compact titlebar's empty
   middle when the window is wide enough, or a band the compact body leaves at its
   foot. The wide arm never docks - its titlebar carries the tabs. */
#define NOTES_HUD_BAND    44
#define NOTES_HUD_DOCK_W  600.0f

/* SIZING AN rcTextArea. Its only height control is `.rows` - the box is rows
   font-px plus padding, and there is no "grow" for a text area - so the rows are
   DERIVED from the height the column has, less the fixed chrome above the area.
   Too short for the floor gets NOTES_ROWS_MIN and the column scrolls. */
#define NOTES_EDITOR_CHROME 219
#define NOTES_AREA_PAD       12
#define NOTES_ROWS_MIN        6
#define NOTES_ROWS_WIDE      18

/* A tab is a FIXED 148 px: growing tabs move every other tab when one opens, so the
   tab under the pointer is not the one that was there when the user set off toward
   it. NOTES_TAB_CHARS is what a label holds at that width. The slop is how far the
   pointer must travel before a click becomes a drag - without one, the jitter in an
   ordinary click would tear the document out instead of selecting it. */
#define NOTES_TAB_CHARS     17
#define NOTES_TEAR_SLOP     8.0f

/* Stable ids by SLOT rather than by note: a tab's identity to the layout is its
   position in the strip, which is what the pointer is actually over. */
static const char *const TAB_IDS[NOTES_MAX_TABS] = {
    "doctab0", "doctab1", "doctab2", "doctab3", "doctab4", "doctab5",
};
static const char *const TAB_CLOSE_IDS[NOTES_MAX_TABS] = {
    "doctabx0", "doctabx1", "doctabx2", "doctabx3", "doctabx4", "doctabx5",
};

/* Unique element ids for the sidebar note rows (the layout engine needs a stable id per
   interactive element; a static table keeps the core rcFormat-free). */
static const char *const NOTE_IDS[NOTE_MAX_NOTES] = {
    "note00", "note01", "note02", "note03", "note04", "note05", "note06", "note07",
    "note08", "note09", "note10", "note11", "note12", "note13", "note14", "note15",
    "note16", "note17", "note18", "note19", "note20", "note21", "note22", "note23",
    "note24", "note25", "note26", "note27", "note28", "note29", "note30", "note31",
};

/* Category options for the publish + settings combos, surfaced in the Preview
   details table. Fixed literals -> length-invariant, rcFormat-free. */
static const char *const CATEGORIES[] = {
    "Engineering", "Design", "Product", "Personal",
};

/* THE PALETTE. Five colours carry the app: warm paper for the sheet, ink for the
   words, a quieter warm grey for the columns beside it, one restrained crimson for
   everything actionable, and a peach wash for the selected row. Everything else is
   one of those five at another weight. A fill that fails contrast is DARKENED,
   never repaired by lightening the text on it. */
#define NOTE_PAPER      0xfbfaf7   /* the sheet: the editor, and every text field   */
#define NOTE_INK        0x292524   /* the words                                     */
#define NOTE_RAIL       0xf0ece5   /* the rail, the note list, the titlebar         */
#define NOTE_SELECTION  0xfee2d5   /* the selected note / collection, a peach wash   */
#define NOTE_ACCENT     0xc2410c   /* the one action colour                         */
#define NOTE_MUTED      0x6b645e   /* metadata, placeholders, inactive labels       */

/* Status accents: a published note reads olive, a draft a quiet grey. In the list
   the 7 px dot is the note's ONLY status carrier and it sits on three different
   grounds, so the draft grey is the lightest that still clears 3:1 on all of them. */
#define NOTE_TINT_PUBLISHED 0x4d7c0f
#define NOTE_TINT_DRAFT     0x857a70

/* How long an edit reads as "Saving" before the header says "Saved" (seconds of
   the injected dt - see the save pair in AppState). */
#define NOTES_SAVE_SETTLE 0.35f

/* The palette as an RC_Style, so every library widget is drawn in it too. THE SHEET
   IS `surface` AND THE COLUMNS ARE `background`, which is the inversion that makes
   the editor borderless: a text field fills itself with `surface`, so a field on the
   sheet IS the sheet. `border` stays a whisper for that reason; every rule the APP
   draws takes `surfaceAlt`, because a line that divides two regions has to be seen. */
RC_Style notes_style(bool dark) {
    RC_Style s = rcStyleLight();
    s.background   = rcHex(dark ? 0x201d1a : NOTE_RAIL);
    s.surface      = rcHex(dark ? 0x191512 : NOTE_PAPER);
    s.surfaceAlt   = rcHex(dark ? 0x2b2622 : 0xe2dbd0);
    s.chrome       = rcHex(dark ? 0x201d1a : NOTE_RAIL);
    s.text         = rcHex(dark ? 0xf4efe8 : NOTE_INK);
    s.textMuted    = rcHex(dark ? 0xa8a09a : NOTE_MUTED);
    s.primary      = rcHex(NOTE_ACCENT);
    s.primaryHover = rcHex(0x9a3412);
    s.danger       = rcHex(0xb91c1c);
    s.dangerHover  = rcHex(0x991b1b);
    s.success      = rcHex(NOTE_TINT_PUBLISHED);
    s.successHover = rcHex(0x3f6212);
    s.warning      = rcHex(0xb45309);
    s.warningHover = rcHex(0x92400e);
    s.border       = rcHex(dark ? 0x241f1c : 0xf6f1e9);
    return s;
}

/* The selection wash: RC_Style has no field for it, because the library never draws
   a "selected row", so the app carries it to the places that draw one. */
static RC_Color note_selection(bool dark) {
    return rcHex(dark ? 0x3a221a : NOTE_SELECTION);
}

/* Map the settings "Editor size" slider (px) to the nearest baked ladder id, so the
   slider VISIBLY steps the Write-tab body text instead of being a cosmetic no-op. */
static NoteFont editor_body_font(float px) {
    if (px < 15.0f) return F_SMALL;
    if (px < 17.0f) return F_BODY;
    if (px < 20.0f) return F_MD;
    return F_HEAD;
}

/* Open note `i`: select it, give it a document tab, sync the editor buffers. The ONE
   writer of both openNote and the tab set, which is why the strip can never show a
   different document from the editor. A full strip replaces the active tab rather
   than refusing to open the note at all. */
static void notes_open(AppState *st, int i) {
    const Note *n = note_at(&st->store, i);
    int         t;

    if (!n)
        return;
    st->openNote = i;
    rcStrCopy(st->title, n->title, sizeof st->title);
    note_tags_csv(n, st->tags, (int)sizeof st->tags);
    note_body_text(n, st->body, (int)sizeof st->body);

    for (t = 0; t < st->tabCount; t++) {
        if (st->tabs[t] == i) {
            st->activeTab = t;
            return;
        }
    }
    if (st->tabCount < NOTES_MAX_TABS) {
        st->tabs[st->tabCount] = i;
        st->activeTab          = st->tabCount;
        st->tabCount++;
    } else {
        st->tabs[st->activeTab] = i;
    }
}

/* Close tab slot `t`. Closing the active tab falls to its LEFT, where the user's eye
   already is. The LAST tab closes too, so openNote goes to -1 - which every backend
   query guards - and the editor buffers are cleared, or a later open would inherit
   the closed note's text. */
static void notes_close_tab(AppState *st, int t) {
    int k;

    if (t < 0 || t >= st->tabCount)
        return;
    for (k = t; k < st->tabCount - 1; k++)
        st->tabs[k] = st->tabs[k + 1];
    st->tabCount--;
    if (st->tabCount == 0) {
        st->activeTab = 0;
        st->openNote  = -1;
        st->title[0]  = '\0';
        st->tags[0]   = '\0';
        st->body[0]   = '\0';
        return;
    }
    if (st->activeTab >= st->tabCount)
        st->activeTab = st->tabCount - 1;
    else if (st->activeTab > t)
        st->activeTab--;
    notes_open(st, st->tabs[st->activeTab]);
}

/* An edit happened: the header says "Saving" until notes_update has spent
   NOTES_SAVE_SETTLE of dt on it. Every editable line in the app routes through
   here, so the indicator can never disagree with what the user just typed. */
static void notes_touch(AppState *st) {
    st->dirty = true;
    st->saveT = 0.0f;
}

/* An app-owned clickable. rcButton hints the pointer cursor itself; a styled box
   has to ask, and polling rcClicked on its id IS the ask.
   TRAP: a hovered element's ANCESTORS count as hovered, so polling a ROW that
   wraps an rcTextInput takes the I-beam off the field. Narrow it back after the
   poll with `if (rcIsHovered(fieldId)) rcSetCursor(RC_CURSOR_TEXT);`. */
static bool nav_button(const char *id, RC_IconCallback icon, bool active,
                       const char *tip) {
    RC_Style s = rcGetStyle();
    rcBox(.id = id,
          .bg = active ? s.surfaceAlt : (rcIsHovered(id) ? s.surface : RC_TRANSPARENT),
          .align = "cc", .borderRadius = "all-lg", .w = "44px", .h = "44px",
          .tooltip = tip) {
        icon(20.0f, active ? s.primary : s.textMuted);
    }
    return rcClicked(id);
}

/* A collection pill (All / Drafts / Published), the compact arm's stand-in for the
   rail. The chosen one takes the selected row's peach wash plus an outline, drawn in
   BOTH states so the row never reflows. A finger gets a 40 px tall pill: the POINTER
   CLASS sizes a hit target, not the width. */
static bool filter_pill(const char *id, const char *label, bool active, bool finger,
                        RC_Color sel) {
    RC_Style s = rcGetStyle();
    rcBox(.id = id,
          .bg = active ? sel : (rcIsHovered(id) ? s.surface : RC_TRANSPARENT),
          .px = 10, .py = 5, .align = "cc", .borderRadius = "all-full",
          .border = { .color = active ? s.primary : RC_TRANSPARENT, .width = "all-1" },
          .hMin = finger ? 40.0f : 0.0f) {
        rcTextC(label, .font = F_SMALL, .color = active ? s.text : s.textMuted);
    }
    return rcClicked(id);
}

/* One document tab. The active tab wears the sheet's ground and a 2 px accent line
   on its top edge, declared in BOTH states (transparent when inactive) so nothing in
   the strip reflows when the selection moves. The close control needs its OWN id: an
   ancestor counts as hovered, so one id over the whole tab would eat the close click.
   `label` is the elided title AppState holds, because a text run borrows its bytes. */
static bool doc_tab(int slot, const char *label, bool active, bool *closed) {
    RC_Style    s      = rcGetStyle();
    const char *id     = TAB_IDS[slot];
    const char *closeId = TAB_CLOSE_IDS[slot];

    *closed = false;
    rcRow(.id = id,
          .bg = active ? s.surface : (rcIsHovered(id) ? s.background : RC_TRANSPARENT),
          .gap = 6, .px = 10, .align = "cl",
          .borderRadius = "t-md", .w = "148px", .h = "34px") {
        /* The accent edge floats rather than leading the row, so it cannot take part
           in the row's gap arithmetic. */
        rcBox(.bg = active ? s.primary : RC_TRANSPARENT,
              .w = "grow", .h = "2px",
              .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                            .element = RC_ANCHOR_TOP_LEFT,
                            .capture = RC_CAPTURE_PASSTHROUGH }) {}
        rcBox(.overflow = "hidden", .w = "grow") {
            rcTextC(label[0] ? label : "Untitled", .font = F_SMALL,
                    .color = active ? s.text : s.textMuted, .wrap = "n");
        }
        rcBox(.id = closeId,
              .bg = rcIsHovered(closeId) ? s.surfaceAlt : RC_TRANSPARENT,
              .align = "cc", .borderRadius = "all-full", .w = "18px", .h = "18px") {
            rcIconX(10.0f, s.textMuted);
        }
    }
    if (rcClicked(closeId)) {
        *closed = true;
        return false;
    }
    return rcClicked(id);
}

/* The tab strip and the tear-out gesture, which are one control: the strip is what
   the gesture measures itself against.
   TRAP: a press anywhere in the RC_ID_WINDOW_DRAG band starts an OS window move, so
   anything polling rcClicked or rcPressed inside it needs an RC_ID_WINDOW_NODRAG
   wrapper or the tabs never click on the desktop. The drop is RECORDED rather than
   acted on - opening a window needs the RC_App, which this file never holds - and
   the gesture is offered only where a second window can exist at all. */
static void notes_tabstrip(AppState *st) {
    RC_Vec2    p         = rcPointer();
    const bool canDetach = rcChildWindowsSupported();
    int        i;

    rcRow(.id = RC_ID_WINDOW_NODRAG, .align = "bl", .h = "34px") {
        rcRow(.id = "TabStrip", .gap = 2, .align = "bl", .h = "34px") {
            for (i = 0; i < st->tabCount; i++) {
                bool closed = false;

                if (doc_tab(i, st->tabLabel[i], i == st->activeTab, &closed))
                    notes_open(st, st->tabs[i]);
                if (closed)
                    notes_close_tab(st, i);
                /* The press that ARMS a possible tear. Polled after the tab's own
                   click so a plain click still selects; the slop test separates them. */
                if (canDetach && rcPressed(TAB_IDS[i])) {
                    st->dragTab     = i;
                    st->dragOriginX = p.x;
                    st->dragOriginY = p.y;
                    st->dragLive    = false;
                }
            }
        }
    }

    if (st->dragTab < 0)
        return;
    if (st->dragTab >= st->tabCount) {        /* the tab closed mid-gesture */
        st->dragTab  = -1;
        st->dragLive = false;
        return;
    }

    if (rcPointerDown(RC_POINTER_LEFT)) {
        float dx = p.x - st->dragOriginX;
        float dy = p.y - st->dragOriginY;

        if (dx * dx + dy * dy > NOTES_TEAR_SLOP * NOTES_TEAR_SLOP) {
            st->dragLive = true;
            rcSetCursor(RC_CURSOR_GRABBING);
        }
        return;
    }

    /* The button is up, so the gesture is over however it ends. A release still
       inside the strip is a rearrange this app does not offer, and doing nothing is
       the right answer to a gesture that was not completed. The tab is NOT closed
       here: notes_tear_settle closes it once a window has actually opened, so a
       refused open leaves the document where the user can still reach it. */
    if (st->dragLive) {
        RC_Box strip = rcGetElementBox("TabStrip");
        bool   inside = strip.found && p.x >= strip.x && p.x <= strip.x + strip.width &&
                        p.y >= strip.y && p.y <= strip.y + strip.height;

        if (!inside) {
            st->tearNote = st->tabs[st->dragTab];
            st->tearTab  = st->dragTab;
            st->tearX    = p.x;
            st->tearY    = p.y;
        }
    }
    st->dragTab  = -1;
    st->dragLive = false;
}

/* The runner's half of the tear-out: close the source tab only when the window it
   was going to opened. */
void notes_tear_settle(AppState *st, bool opened) {
    if (opened && st->tearTab >= 0)
        notes_close_tab(st, st->tearTab);
    st->tearTab = -1;
}

/* The drag preview: a tab-shaped card under the pointer, PASSTHROUGH so it never
   becomes the thing the pointer is over - a ghost that ate its own drop would end
   the gesture the moment it started. */
static void notes_tab_ghost(AppState *st) {
    RC_Style    s = rcGetStyle();
    RC_Vec2     p = rcPointer();
    const Note *n;

    if (!st->dragLive || st->dragTab < 0 || st->dragTab >= st->tabCount)
        return;
    n = note_at(&st->store, st->tabs[st->dragTab]);
    rcRow(.id = "TabGhost", .bg = s.surface, .gap = 6, .px = 10, .align = "cl",
          .borderRadius = "all-md",
          .border = { .color = s.primary, .width = "all-1" },
          .w = "148px", .h = "34px",
          .shadow = { .color = rcAlpha(RC_BLACK, 90), .y = 6, .blur = 16, .spread = -2 },
          .floating = { .to = RC_ATTACH_ROOT, .parent = RC_ANCHOR_TOP_LEFT,
                        .element = RC_ANCHOR_TOP_LEFT,
                        .offset = { p.x - 74.0f, p.y - 17.0f },
                        .zIndex = 900, .capture = RC_CAPTURE_PASSTHROUGH }) {
        rcBox(.overflow = "hidden", .w = "grow") {
            rcTextC(n ? n->title : "Untitled", .font = F_SMALL, .color = s.text,
                    .wrap = "n");
        }
    }
}

/* A Write/Preview tab: a centered label over an underline that lights when active. */
static bool tab_button(const char *id, const char *label, bool active) {
    RC_Style s = rcGetStyle();
    rcColumn(.id = id, .px = 8, .align = "cc", .h = "grow") {
        rcBox(.align = "cc", .h = "grow") {
            rcTextC(label, .font = F_BODY, .color = active ? s.text : s.textMuted);
        }
        rcBox(.bg = active ? s.primary : RC_TRANSPARENT, .borderRadius = "all-full",
              .w = "28px", .h = "2px") {}
    }
    return rcClicked(id);
}

/* A note in the list: title + status dot, one line of preview, the date. A SLIM list
   sheds the preview and wraps the title instead - the least useful line is the one
   to lose when a column narrows, and the title is the one line a list is scanned by.
   SELECTION IS A BACKGROUND PLUS A MARK, never a background alone: a tint is the one
   channel a colour-blind reader loses first. The 3 px bar holds its gutter in both
   states so nothing reflows when the selection moves. */
static bool note_row(const char *id, const Note *n, bool active, bool slim,
                     RC_Color sel) {
    RC_Style s = rcGetStyle();
    bool pub = (n->status == NOTE_PUBLISHED);
    rcRow(.gap = 8, .align = "tl", .w = "grow") {
    rcBox(.bg = active ? s.primary : RC_TRANSPARENT, .borderRadius = "all-full",
          .w = "3px", .h = "38px") {}
    rcColumn(.id = id,
             .bg = active ? sel : (rcIsHovered(id) ? s.surface : RC_TRANSPARENT),
             .gap = 3, .px = 10, .py = 9, .borderRadius = "all-md", .w = "grow") {
        rcRow(.gap = 6, .align = "tl", .w = "grow") {
            if (slim) {
                rcBox(.w = "grow") {
                    rcTextC(n->title, .font = F_BODY, .color = s.text);
                }
            } else {
                rcBox(.overflow = "hidden", .w = "grow") {
                    rcTextC(n->title, .font = F_BODY, .color = s.text, .wrap = "n");
                }
            }
            /* The dot keeps the title's first line, not the row's middle, so a
               title that wraps does not drag its status marker down with it. */
            rcBox(.align = "cc", .h = "18px") {
                rcBox(.bg = rcHex(pub ? NOTE_TINT_PUBLISHED : NOTE_TINT_DRAFT),
                      .borderRadius = "all-full", .w = "7px", .h = "7px") {}
            }
        }
        if (!slim) {
            rcBox(.overflow = "hidden", .w = "grow") {
                rcTextC(n->snippet, .font = F_SMALL, .color = s.textMuted, .wrap = "n");
            }
        }
        rcTextC(n->date, .font = F_SMALL, .color = s.textMuted);
    }
    }
    return rcClicked(id);
}

/* One label/value row of the Preview "Post details" table. */
static void details_row(const char *label, const char *value) {
    RC_Style s = rcGetStyle();
    rcRow(.gap = 12, .align = "cl", .w = "grow") {
        rcBox(.w = "110px") {
            rcTextC(label, .font = F_SMALL, .color = s.textMuted);
        }
        rcBox(.w = "grow") {
            rcTextC(value, .font = F_SMALL, .color = s.text);
        }
    }
}

/* The Write tab: an editorial column on the sheet - the title, one muted metadata
   line, the tags line, then the body as a live multi-line editor. Nothing here is
   boxed or ruled, because a field on the sheet IS the sheet; hover and focus still
   mark the active line, so borderless costs the user no feedback. The 720 px is a
   CEILING, not a width, so a phone and a wide desktop both get a column that fits. */
static void notes_write(AppState *st, int rows, const Note *n) {
    RC_Style s = rcGetStyle();
    NoteFont body_font = editor_body_font(st->editorFontPx);
    bool     pub       = (n && n->status == NOTE_PUBLISHED);
    rcColumn(.id = "WriteScroll", .p = 28, .align = "tc", .scroll = "v", .w = "grow",
             .h = "grow") {
        rcColumn(.gap = 10, .w = "grow", .wMax = 720.0f) {
            if (rcTextInput("title_in", st->title, sizeof st->title,
                            .placeholder = "Untitled note", .font = (uint16_t)F_TITLE))
                notes_touch(st);
            /* The metadata, restrained to ONE muted line. The \xc2\xb7 is the
               Latin-1 middot; the 10 px inset matches the padding a field draws
               inside itself, so this line starts where the title and body start. */
            rcRow(.gap = 8, .pl = 10, .align = "cl", .w = "grow") {
                if (n) {
                    rcTextC(n->date, .font = F_SMALL, .color = s.textMuted);
                    rcTextL("\xc2\xb7", .font = F_SMALL, .color = s.textMuted);
                    rcTextC(n->meta, .font = F_SMALL, .color = s.textMuted);
                    rcTextL("\xc2\xb7", .font = F_SMALL, .color = s.textMuted);
                }
                rcTextC(pub ? "Published" : "Draft", .font = F_SMALL,
                         .color = pub ? rcHex(NOTE_TINT_PUBLISHED) : s.textMuted);
            }
            if (rcTextInput("tags_in", st->tags, sizeof st->tags,
                            .placeholder = "Add tags, comma separated"))
                notes_touch(st);
            if (rcTextArea("body_in", st->body, sizeof st->body,
                           .placeholder = "Write your note", .font = (uint16_t)body_font,
                           .rows = (uint16_t)rows))
                notes_touch(st);
        }
    }
}

/* The Preview tab: the rendered post - display title, byline, the wrapped body, a
   details table and the tag chips. The column is clamped for the same reason
   notes_write's is. */
static void notes_preview(AppState *st, const Note *n) {
    RC_Style s = rcGetStyle();
    if (!n)
        return;                     /* no open note: render nothing (defensive) */
    bool pub = (n->status == NOTE_PUBLISHED);
    rcColumn(.id = "PreviewScroll", .p = 28, .align = "tc", .scroll = "v", .w = "grow",
             .h = "grow") {
        rcColumn(.gap = 12, .w = "grow", .wMax = 720.0f) {
            rcTextC(n->title, .font = F_TITLE, .color = s.text);
            /* byline: author, date, status chip (\xc2\xb7 = the Latin-1 middot) */
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                rcTextL("By You", .font = F_SMALL, .color = s.textMuted);
                rcTextL("\xc2\xb7", .font = F_SMALL, .color = s.textMuted);
                rcTextC(n->date, .font = F_SMALL, .color = s.textMuted);
                rcTextL("\xc2\xb7", .font = F_SMALL, .color = s.textMuted);
                /* The one filled chip in the app: a post that is live. A draft is
                   the same chip outlined, so the two read as one badge in two
                   states rather than as two different badges. */
                rcBox(.bg = pub ? rcHex(NOTE_TINT_PUBLISHED) : RC_TRANSPARENT, .px = 8,
                      .py = 2, .align = "cc", .borderRadius = "all-full",
                      .border = { .color = pub ? RC_TRANSPARENT : s.surfaceAlt, .width = "1" }) {
                    rcTextC(pub ? "Published" : "Draft", .font = F_SMALL,
                             .color = pub ? RC_WHITE : s.textMuted);
                }
            }
            rcBox(.bg = s.surfaceAlt, .w = "grow", .h = "1px") {}
            for (int p = 0; p < n->body.nParas; p++)
                rcTextC(n->body.paras[p], .font = F_BODY, .color = s.text);
            rcBox(.bg = s.surfaceAlt, .w = "grow", .h = "1px") {}
            rcTextL("Post details", .font = F_MD, .color = s.text);
            details_row("Status",   pub ? "Published" : "Draft");
            details_row("Author",   "You");
            details_row("Date",     n->date);
            details_row("Reading",  n->meta);
            details_row("Category", CATEGORIES[st->category]);
            /* What the publish dialog collected, read back where the reader can see
               it - a field whose value never appears is a field that did nothing. */
            details_row("Visibility", st->isPublic ? "Public" : "Unlisted");
            if (st->slug[0])    details_row("Slug",    st->slug);
            if (st->summary[0]) details_row("Summary", st->summary);
            rcRow(.gap = 6, .align = "cl", .w = "grow") {
                for (int t = 0; t < n->tagCount; t++) {
                    rcBox(.px = 8, .py = 3, .borderRadius = "all-full",
                          .border = { .color = s.surfaceAlt, .width = "1" }) {
                        rcTextC(n->tags[t], .font = F_SMALL, .color = s.textMuted);
                    }
                }
            }
        }
    }
}

/* The custom titlebar: brand, the document tabs and the bundled window controls.
   `tabs` is the WIDE arm only - a phone has no room for a strip beside a brand and
   window controls, and there the note list IS the document switcher. */
static void notes_topbar(AppState *st, bool tabs) {
    RC_Style s = rcGetStyle();
    /* CHROME, NOT CONTENT: the OS drag strip is pinned at .titlebarHeight, so a band
       that scaled with the content zoom would stop matching the strip the user can
       drag. rcUnzoomed() counter-scales by 1/zoom and takes the row as its single
       statement; at zoom 1 it resolves to 1.0 and changes nothing. */
    rcUnzoomed()
    rcRow(.id = RC_ID_WINDOW_DRAG, .bg = s.chrome, .gap = 10, .px = 14, .align = "cl",
          .w = "grow", .h = "52px") {
        rcBox(.bg = s.primary, .align = "cc", .borderRadius = "all-md", .w = "26px",
              .h = "26px") {
            rcTextL("N", .font = F_BODY, .color = RC_WHITE);
        }
        /* THE BRAND GOES WHEN THE TABS COME: both name what you are looking at, and
           the tabs do it for the actual document. It comes back when the last tab
           closes, because an empty strip beside a lone mark reads as chrome that
           failed to draw rather than as an app with nothing open. */
        if (!tabs || st->tabCount == 0)
            rcTextL("RayClay Notes", .font = F_HEAD, .color = s.text);
        if (tabs)
            notes_tabstrip(st);
        rcBox(.w = "grow") {}
        rcWindowControls();
    }
}

/* The rail: the collections nav, with settings and the account at its foot. It
   carries no fill of its own, so the left of the window reads as one band of chrome
   and the sheet beside it is the only lit surface. Each destination is the SAME
   filter the compact arm spends a pill row on. */
static void notes_navrail(AppState *st, RC_Color sel) {
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.background, .gap = 6, .py = 12, .align = "tc", .h = "grow",
             .wType = RC_PX(NOTES_RAIL_W)) {
        if (nav_button("nav_all", rcIconHouse, st->folderFilter == 0, "All notes"))
            st->folderFilter = 0;
        if (nav_button("nav_drafts", rcIconFolder, st->folderFilter == 1, "Drafts"))
            st->folderFilter = 1;
        if (nav_button("nav_pub", rcIconChartColumn, st->folderFilter == 2, "Published"))
            st->folderFilter = 2;
        /* A stretchy spacer. It needs a BODY: rcSeparator is a scope, so written
           without braces it would swallow the next statement as its contents. */
        rcSeparator() {}
        if (nav_button("nav_settings", rcIconSettings, false, "Settings")) {
            st->modalSettings = true;
            st->modalPublish  = false;
        }
        rcBox(.bg = sel, .align = "cc", .borderRadius = "all-full",
              .w = "40px", .h = "40px") {
            rcTextL("Y", .font = F_SMALL, .color = s.text);
        }
    }
}

/* The note list. Alone on the screen it IS the screen, so it grows instead of
   holding its 320 px column. NO TITLE BAND: the titlebar already names the app, so
   the search field is the header instead, with the actions on its line. The
   collection pills appear only in the compact arm, where the rail is withheld. */
static void notes_sidebar(AppState *st, bool compact, bool slim, bool finger,
                          RC_Color sel) {
    /* The compact arm is the list ALONE on the screen, so it is never slim. */
    slim = slim && !compact;
    RC_Style s = rcGetStyle();
    rcColumn(.bg = s.background, .gap = 8, .p = 10,
             .w = compact ? "grow" : (slim ? "240px" : "320px"), .h = "grow") {
        rcRow(.gap = 6, .align = "cl", .w = "grow") {
            rcTextInput("search", st->search, sizeof st->search,
                        .placeholder = "Search notes");
            /* A full notebook says so and stops offering the action, rather than
               leaving a control that silently does nothing: the poll is what makes a
               box clickable, so withholding it withholds the pointer cursor too. */
            bool full = note_count(&st->store) >= NOTE_MAX_NOTES;
            rcBox(.id = "btn_add",
                  .bg = (!full && rcIsHovered("btn_add")) ? s.surfaceAlt : RC_TRANSPARENT,
                  .align = "cc", .borderRadius = "all-md", .w = "32px", .h = "32px",
                  .tooltip = full ? "Notebook full" : "New note") {
                rcTextL("+", .font = F_HEAD,
                        .color = full ? s.surfaceAlt : s.textMuted);
            }
            if (!full && rcClicked("btn_add")) {
                int fresh = note_create(&st->store);
                if (fresh >= 0) {
                    notes_open(st, fresh);
                    st->tab        = 0;
                    st->editorOpen = true;   /* a new note is asking for the editor */
                    st->search[0]  = '\0';   /* or the filter would hide it at once */
                }
            }
            /* The rail is withheld in the compact arm, so its two foot controls come
               here rather than vanish: the settings gear (the same id, so it is the
               same control in a new place) and the account avatar, sized to the "+"
               beside them so the header row keeps its height. */
            if (compact) {
                rcBox(.id = "nav_settings",
                      .bg = rcIsHovered("nav_settings") ? s.surfaceAlt : RC_TRANSPARENT,
                      .align = "cc", .borderRadius = "all-md", .w = "32px",
                      .h = "32px", .tooltip = "Settings") {
                    rcIconSettings(18.0f, s.textMuted);
                }
                if (rcClicked("nav_settings")) {
                    st->modalSettings = true;
                    st->modalPublish  = false;
                }
                rcBox(.bg = sel, .align = "cc", .borderRadius = "all-full",
                      .w = "32px", .h = "32px") {
                    rcTextL("Y", .font = F_SMALL, .color = s.text);
                }
            }
        }
        if (compact) {
            rcRow(.gap = 6, .w = "grow") {
                if (filter_pill("pill_all", "All", st->folderFilter == 0, finger, sel))
                    st->folderFilter = 0;
                if (filter_pill("pill_drafts", "Drafts", st->folderFilter == 1, finger, sel))
                    st->folderFilter = 1;
                if (filter_pill("pill_pub", "Published", st->folderFilter == 2, finger, sel))
                    st->folderFilter = 2;
            }
        }
        rcColumn(.id = "NoteList", .gap = 2, .pr = 12, .scroll = "v", .w = "grow",
                 .h = "grow") {
            int n = note_count(&st->store);
            for (int i = 0; i < n; i++) {
                const Note *nt = note_at(&st->store, i);
                if (!nt)                  /* out-of-range query returns NULL; skip the row */
                    continue;
                /* Filter by collection and by search, but ALWAYS keep the open note's
                   row visible so a filter change can never strand the note the main
                   pane is showing. */
                if (i != st->openNote) {
                    if (st->folderFilter == 1 && nt->status != NOTE_DRAFT)     continue;
                    if (st->folderFilter == 2 && nt->status != NOTE_PUBLISHED) continue;
                    if (!note_matches(nt, st->search))                         continue;
                }
                if (note_row(NOTE_IDS[i], nt, i == st->openNote, slim, sel)) {
                    notes_open(st, i);
                    /* One pane at a time: picking a note IS the navigation, so the
                       tap records that the user asked for it (notes_app.h). */
                    st->editorOpen = true;
                }
            }
        }
    }
}

/* The editor pane. `paneH` is the viewport less the safe bands, the titlebar and the
   HUD band, and the body TAKES it: a fixed row count leaves the foot of a desktop
   pane empty, which reads as an editor that stopped rather than a document. */
static void notes_main(AppState *st, bool compact, float paneH, bool fillPane) {
    RC_Style s = rcGetStyle();
    const Note *n = note_at(&st->store, st->openNote);
    int rows = NOTES_ROWS_WIDE;
    if (compact || fillPane) {
        float bodyPx = NOTES_FONT_PX[editor_body_font(st->editorFontPx)];
        /* The demo spends a HUD band too, though nothing lays out in it: the perf
           chip floats in the desktop's bottom-right corner, and a body that took the
           last pixel of the pane would be written under it. */
        float chrome = (float)NOTES_EDITOR_CHROME + (compact ? 0.0f : (float)NOTES_HUD_BAND);
        rows = (int)((paneH - chrome - NOTES_AREA_PAD) / bodyPx);
        if (rows < NOTES_ROWS_MIN)
            rows = NOTES_ROWS_MIN;
    }
    bool pub = (n && n->status == NOTE_PUBLISHED);

    /* NOTHING IS OPEN, reachable by closing or tearing out every tab. It is the
       first screen a user with an empty notebook meets, and an editor that drew a
       text box over a note that is not there would be lying about its own state. */
    if (st->tabCount == 0) {
        rcColumn(.bg = s.surface, .gap = 8, .align = "cc", .w = "grow", .h = "grow") {
            rcTextL("No note open", .font = F_HEAD, .color = s.text);
            rcTextL("Pick one from the list, or press + to start a new note.",
                    .font = F_SMALL, .color = s.textMuted);
        }
        return;
    }

    rcColumn(.bg = s.surface, .w = "grow", .h = "grow") {
        /* The editor header, ON THE SHEET rather than in chrome: Write | Preview,
           the save state and the one primary action. The reading meta is NOT here at
           any width - it is a fact about the document, so it belongs on the document. */
        rcRow(.bg = s.surface, .gap = 10, .px = 20, .align = "cl", .w = "grow",
              .h = "52px") {
            /* The compact arm's one new control: one pane at a time needs a way back
               to the list. openNote is deliberately left alone, because closing a
               view is not changing a selection. */
            if (compact && nav_button("nav_back", rcIconPanelLeft, false, "Notes"))
                st->editorOpen = false;
            if (tab_button("tab_write",   "Write",   st->tab == 0)) st->tab = 0;
            if (tab_button("tab_preview", "Preview", st->tab == 1)) st->tab = 1;
            rcBox(.w = "grow") {}
            /* The save state: a dot and one word. It is the app's answer to "where
               did my typing go", so it is never absent - and with auto-save off it
               says "Unsaved" rather than pretending the edit was written. */
            rcRow(.gap = 6, .align = "cl") {
                rcBox(.bg = st->dirty ? s.warning : rcHex(NOTE_TINT_PUBLISHED),
                      .borderRadius = "all-full", .w = "6px", .h = "6px") {}
                rcTextC(st->dirty ? (st->autoSave ? "Saving" : "Unsaved") : "Saved",
                         .font = F_SMALL, .color = s.textMuted);
            }
            /* A published post is UPDATED, not published again - the label is the
               clearest statement of the note's state the pane can make. */
            if (rcButton("btn_publish", pub ? "Update" : "Publish", RC_BTN_PRIMARY)) {
                st->modalPublish  = true;
                st->modalSettings = false;
            }
        }
        rcBox(.bg = s.surfaceAlt, .w = "grow", .h = "1px") {}
        if (st->tab == 0)
            notes_write(st, rows, n);
        else
            notes_preview(st, n);
    }
}

/* The modals, placed OUTSIDE the root so their scrim covers the whole window.
   TRAP: a fixed px width wider than a phone's viewport puts the dialog's right edge
   and the buttons on it off the screen it was centred in. The compact arm sizes in
   `vw`, which resolves against the layout viewport rather than the parent. */
static void notes_modals(AppState *st, bool compact) {
    RC_Style s = rcGetStyle();

    if (rcBeginModal("modal_publish", &st->modalPublish)) {
        rcColumn(.bg = s.surface, .gap = 12, .p = 18, .borderRadius = "all-xl",
                 .w = compact ? "84vw" : "420px") {
            rcTextL("Publish note", .font = F_TITLE, .color = s.text);
            rcTextL("Review the details before publishing.", .font = F_SMALL, .color = s.textMuted);
            rcRow(.align = "cl", .w = "grow") {
                rcTextL("Public", .font = F_BODY, .color = s.text);
                rcBox(.w = "grow") {}
                rcToggle("tg_public", &st->isPublic);
            }
            rcColumn(.gap = 4, .w = "grow") {
                rcTextL("Slug", .font = F_SMALL, .color = s.textMuted);
                rcTextInput("slug_in", st->slug, sizeof st->slug, .placeholder = "my-note");
            }
            rcColumn(.gap = 4, .w = "grow") {
                rcTextL("Summary", .font = F_SMALL, .color = s.textMuted);
                rcTextInput("summary_in", st->summary, sizeof st->summary,
                             .placeholder = "A short summary");
            }
            rcRow(.gap = 12, .align = "cl", .w = "grow") {
                rcBox(.w = "120px") {
                    rcTextL("Category", .font = F_BODY, .color = s.text);
                }
                rcBox(.w = "grow") { rcCombo("cb_cat", &st->category, CATEGORIES, 4); }
            }
            rcRow(.gap = 8) {
                if (rcButton("btn_pub_confirm", "Publish", RC_BTN_PRIMARY)) {
                    note_publish(&st->store, st->openNote);
                    st->modalPublish = false;
                }
                if (rcButton("btn_pub_cancel", "Cancel", RC_BTN_GHOST))
                    st->modalPublish = false;
            }
        }
        rcEndModal();
    }

    if (rcBeginModal("modal_settings", &st->modalSettings)) {
        rcColumn(.bg = s.surface, .gap = 14, .p = 18, .borderRadius = "all-xl",
                 .w = compact ? "84vw" : "400px") {
            rcTextL("Settings", .font = F_TITLE, .color = s.text);
            rcRow(.align = "cl", .w = "grow") {
                rcTextL("Dark mode", .font = F_BODY, .color = s.text);
                rcBox(.w = "grow") {}
                rcToggle("tg_dark_set", &st->darkMode);
            }
            rcRow(.align = "cl", .w = "grow") {
                rcTextL("Auto-save", .font = F_BODY, .color = s.text);
                rcBox(.w = "grow") {}
                rcToggle("tg_autosave", &st->autoSave);
            }
            rcRow(.gap = 12, .align = "cl", .w = "grow") {
                rcBox(.w = "150px") {
                    rcTextL("Editor size", .font = F_BODY, .color = s.text);
                }
                rcBox(.w = "grow") { rcSlider("sl_fontsize", &st->editorFontPx, 12.0f, 24.0f); }
            }
            rcRow(.gap = 12, .align = "cl", .w = "grow") {
                rcBox(.w = "150px") {
                    rcTextL("Default category", .font = F_BODY, .color = s.text);
                }
                rcBox(.w = "grow") { rcCombo("cb_cat2", &st->category, CATEGORIES, 4); }
            }
            rcRow(.gap = 8) {
                if (rcButton("btn_set_done", "Done", RC_BTN_PRIMARY))
                    st->modalSettings = false;
            }
        }
        rcEndModal();
    }
}

void notes_seed(AppState *st, unsigned seed) {
    note_memzero(st, sizeof *st);       /* zero all, including padding, THEN set */
    note_store_seed(&st->store, seed);
    st->openNote     = 2;
    st->tab          = 0;
    st->folderFilter = 0;
    st->category     = 0;
    st->darkMode     = false;            /* the app is a sheet of paper by default */
    st->autoSave     = true;
    st->isPublic     = false;
    st->editorFontPx = 16.0f;
    st->editorOpen   = false;            /* the compact arm opens on the LIST */
    st->seeded       = true;
    /* The gesture fields are set to "no gesture" rather than left at the memset's
       zero, because zero is a valid tab slot and would read as a drag of tab 0 on
       the very first frame. */
    st->dragTab      = -1;
    st->dragLive     = false;
    st->tearNote     = -1;
    st->tearTab      = -1;
    notes_open(st, 2);                  /* sync the editor buffers AND open its tab */
}

void notes_update(AppState *st, const AppCtx *ctx) {
    note_store_step(&st->store, ctx->dt);   /* dt <= 0 (freeze) => a no-op */
    /* Settle the save indicator on the SAME clock as the backend, and only while
       auto-save is on: with it off, an edit stays "Unsaved" until it is turned back
       on, which is the honest report of what the setting does. */
    if (st->dirty && st->autoSave && ctx->dt > 0.0f) {
        st->saveT += ctx->dt;
        if (st->saveT >= NOTES_SAVE_SETTLE) {
            st->dirty = false;
            st->saveT = 0.0f;
        }
    }
    /* Push the editor buffers back onto the open note, so the Write -> Preview
       round-trip shows what was typed. Idempotent: the backend clamps to its own
       caps, so re-pushing the same buffers every frame never grows or drifts. */
    int len = 0;
    while (st->title[len])
        len++;
    note_set_title(&st->store, st->openNote, st->title, len);
    note_set_tags(&st->store, st->openNote, st->tags);
    /* The tab labels, elided here rather than at draw time because a text run
       borrows the bytes it is given until the frame is rendered. */
    for (int t = 0; t < st->tabCount; t++) {
        const Note *n = note_at(&st->store, st->tabs[t]);
        note_elide(n ? n->title : "Untitled", st->tabLabel[t],
                   NOTES_TAB_LABEL_CAP, NOTES_TAB_CHARS);
    }
}

void notes_layout(AppState *st, const AppCtx *ctx) {
    /* The palette is installed here rather than in the runner, so every caller of
       this function renders the same app. */
    rcSetStyle(notes_style(st->darkMode));
    RC_Style s      = rcGetStyle();
    RC_Color sel    = note_selection(st->darkMode);

    /* The safe area in LAYOUT space: the insets arrive in window px and padding is
       spent where the layout runs, so app_safe divides by the zoom. Zero on desktop. */
    RC_Insets safe = app_safe(ctx);

    /* Every arm decision is made here, off the measured viewport and never off the
       live window, so a desktop window dragged narrow takes the same branch a phone
       does. `finger` is the pointer class and sizes hit targets, never the layout. */
    const float innerW  = app_view_w(ctx) - safe.left - safe.right;
    const bool  compact = innerW < NOTES_ONE_PANE_W;
    const bool  slim    = innerW < NOTES_LIST_SLIM_AT;
    const bool  finger  = ctx->view.coarsePointer;

    /* A narrow compact window has no room for the perf chip in its titlebar, so the
       body ends a band above the safe area and the chip lives there. */
    const uint16_t band   = (compact && innerW < NOTES_HUD_DOCK_W) ? NOTES_HUD_BAND : 0;
    const float    paneH  = app_view_h(ctx) - safe.top - safe.bottom - 52.0f - (float)band;

    rcColumn(.id = "Root", .bg = s.background, .pt = (uint16_t)safe.top,
             .pb = (uint16_t)safe.bottom, .pl = (uint16_t)safe.left,
             .pr = (uint16_t)safe.right, .w = "grow", .h = "grow") {
        notes_topbar(st, !compact);
        rcRow(.id = "Body", .pb = band, .w = "grow", .h = "grow") {
            /* Wide: rail, list, editor. Compact: the list is home and the editor is
               what a tap opens, so the two are exclusive; the rail is withheld and
               its controls fold into the list header. */
            if (!compact)
                notes_navrail(st, sel);
            if (!compact || !st->editorOpen)
                notes_sidebar(st, compact, slim, finger, sel);
            /* One hairline is the whole boundary between the chrome columns and the
               sheet: the two grounds already differ, so anything heavier would be a
               frame drawn around the document. */
            if (!compact)
                rcBox(.bg = s.surfaceAlt, .w = "1px", .h = "grow") {}
            if (!compact || st->editorOpen)
                notes_main(st, compact, paneH, ctx->mode == APP_DEMO);
        }
        /* Declared INSIDE Root so its RC_ATTACH_ROOT offset is measured in the space
           Root is laid out in, and LAST so it draws over its own strip. */
        notes_tab_ghost(st);
    }
    notes_modals(st, compact);            /* modals sit outside Root (full-window scrim) */

    /* A scrollbar named for a container this frame did not declare is dropped with a
       warning, so each bar carries the same condition its pane carries - including
       the empty state, where notes_main declares neither editor pane. */
    const bool listLive   = (!compact || !st->editorOpen);
    const bool editorLive = (!compact || st->editorOpen) && st->tabCount > 0;
    if (listLive)                    rcScrollbar("NoteList");
    if (editorLive && st->tab == 0)  rcScrollbar("WriteScroll");
    if (editorLive && st->tab == 1 && note_at(&st->store, st->openNote))
        rcScrollbar("PreviewScroll");
}

void notes_demo_chrome(AppState *st, const AppCtx *ctx) {
    if (ctx->mode != APP_DEMO || !ctx->arena)
        return;
    /* A floating perf readout, demo-only. PASSTHROUGH so clicks fall through it and
       it never blocks the titlebar controls nor the drag band it may dock in. It is
       anchored to the ROOT, whose box includes the safe bands, so the nudge adds them
       and the chip stays inside the safe area. */
    RC_Insets   safe    = app_safe(ctx);
    const float innerW  = app_view_w(ctx) - safe.left - safe.right;
    const bool  compact = innerW < NOTES_ONE_PANE_W;
    const bool  band    = compact && innerW < NOTES_HUD_DOCK_W;
    const bool  docked  = !band && compact;
    const RC_Anchor at   = docked ? RC_ANCHOR_TOP_CENTER : RC_ANCHOR_BOTTOM_RIGHT;
    const float     offX = docked ? 0.0f : -16.0f - safe.right;
    const float     offY = docked ? safe.top + 14.0f : -16.0f - safe.bottom;   /* (52 - 23) / 2 */
    RC_String hud = rcFormat(ctx->arena, "%.0f fps \xc2\xb7 %d notes \xc2\xb7 %d published",
                                ctx->dt > 0.0f ? 1.0f / ctx->dt : 0.0f,
                                note_count(&st->store),
                                note_published_count(&st->store));
    rcBox(.id = "demo_hud", .bg = rcAlpha(RC_BLACK, 150), .px = 10, .py = 5,
           .borderRadius = "all-full",
           .floating = { .to = RC_ATTACH_ROOT, .parent = at, .element = at,
                         .offset = { offX, offY }, .capture = RC_CAPTURE_PASSTHROUGH }) {
        rcText(hud, .font = F_SMALL, .color = RC_WHITE);
    }
}

/* THE TORN-OFF DOCUMENT WINDOW: one document and no chrome it does not need.
   IT READS THE STORE AND WRITES NOTHING - two editors over one buffer is a problem
   this example is not about - but the title is live, which is what makes it obviously
   ONE document in two places. The empty state is reachable: nothing stops the store
   changing under the note index this window captured. */
void notes_torn_window(AppState *st, int noteIndex) {
    RC_Style    s = rcGetStyle();
    const Note *n = note_at(&st->store, noteIndex);

    rcSetStyle(notes_style(st->darkMode));
    s = rcGetStyle();

    rcColumn(.id = "TornRoot", .bg = s.background, .w = "grow", .h = "grow") {
        rcUnzoomed()
        rcRow(.id = RC_ID_WINDOW_DRAG, .bg = s.chrome, .gap = 10, .px = 14,
              .align = "cl", .w = "grow", .h = "44px") {
            rcBox(.bg = s.primary, .align = "cc", .borderRadius = "all-full",
                  .w = "8px", .h = "8px") {}
            rcBox(.overflow = "hidden", .w = "grow") {
                rcTextC(n ? n->title : "Note", .font = F_BODY, .color = s.text,
                        .wrap = "n");
            }
            rcWindowControls();
        }
        rcColumn(.id = "TornBody", .bg = s.surface, .gap = 12, .p = 24,
                 .scroll = "v", .w = "grow", .h = "grow") {
            if (!n) {
                rcTextL("This note is not in the notebook.", .font = F_BODY,
                        .color = s.textMuted);
            } else {
                int i;

                rcTextC(n->title, .font = F_TITLE, .color = s.text);
                rcRow(.gap = 8, .align = "cl") {
                    rcBox(.bg = rcHex(n->status == NOTE_PUBLISHED ? NOTE_TINT_PUBLISHED
                                                                  : NOTE_TINT_DRAFT),
                          .borderRadius = "all-full", .w = "7px", .h = "7px") {}
                    rcTextC(n->status == NOTE_PUBLISHED ? "Published" : "Draft",
                            .font = F_SMALL, .color = s.textMuted);
                    rcTextC(n->meta, .font = F_SMALL, .color = s.textMuted);
                    rcTextC(n->date, .font = F_SMALL, .color = s.textMuted);
                }
                rcBox(.bg = s.surfaceAlt, .w = "grow", .h = "1px") {}
                /* Paragraph by paragraph, as the preview pane does it: one text run
                   per paragraph is what guarantees the breaks. */
                for (i = 0; i < n->body.nParas; i++)
                    rcTextC(n->body.paras[i], .font = F_BODY, .color = s.text);
            }
        }
    }
    rcScrollbar("TornBody");
}

void notes_bench_step(AppState *st, const AppInputSink *in, int frame) {
    (void)st;   /* every action here is synthetic input, never a direct state write */
    /* The scripted scenario. At/after NOTES_BENCH_WARMUP the app HOLDS, a strict
       no-op, so a double-rendered frame is byte-identical.
       DETERMINISM AT THE HOLD: the caret blink and the tooltip dwell run on real
       time, not on the injected dt, so the frozen frame must carry no focused text
       input and the pointer must rest over nothing with a tooltip; the blur and
       off-canvas park below guarantee both. The coordinates are a first draft for
       1280x720 - retune them to your own layout. */
    if (!in || frame >= NOTES_BENCH_WARMUP)
        return;                                        /* the HOLD */

    if (frame == 6) {
        in->move(in->ctx, 200.0f, 300.0f);            /* park the pointer over NoteList so the wheel targets it. Note 2
                                                          (the long-body DRAFT) is the DEFAULT-open note, so the freeze is
                                                          robustly openNote==2 with NO coord-fragile click-select. */
    } else if (frame >= 10 && frame < 16) {
        in->wheel(in->ctx, 0.0f, -2.0f);              /* scroll the note list (pointer is over it) */
    } else if (frame == 20) {
        in->move(in->ctx, 1150.0f, 150.0f);           /* focus the title line (the sheet's column is x 472..1192, */
                                                      /* the title y 133..175); FAR-RIGHT x clamps the caret      */
        in->button(in->ctx, APP_MBTN_LEFT, true);      /* to end-of-text so the typed chars append cleanly */
    } else if (frame == 21) {
        in->button(in->ctx, APP_MBTN_LEFT, false);
    } else if (frame >= 24 && frame < 36) {
        static const char TYPED[] = " looks great";    /* 12 ASCII chars, one/frame -> a clean end-of-title append */
        in->text(in->ctx, (unsigned int)(unsigned char)TYPED[frame - 24]);
    } else if (frame == 40) {
        in->move(in->ctx, 1225.0f, 78.0f);            /* open the Publish modal (btn_publish, header y~52..104) */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == 41) {
        in->button(in->ctx, APP_MBTN_LEFT, false);
    } else if (frame == 46) {
        in->move(in->ctx, 490.0f, 490.0f);            /* confirm Publish (note 2 -> published post at the freeze) */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == 47) {
        in->button(in->ctx, APP_MBTN_LEFT, false);
    } else if (frame == 52) {
        in->move(in->ctx, 490.0f, 78.0f);             /* switch to the Preview tab (header, NOT the y=26 titlebar) */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == 53) {
        in->button(in->ctx, APP_MBTN_LEFT, false);
    } else if (frame == NOTES_BENCH_WARMUP - 2) {
        in->move(in->ctx, -100.0f, -100.0f);          /* blur the title + park off-canvas: press ... */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == NOTES_BENCH_WARMUP - 1) {
        in->button(in->ctx, APP_MBTN_LEFT, false);     /* ... release; pointer stays off-canvas into the hold */
    }
}
