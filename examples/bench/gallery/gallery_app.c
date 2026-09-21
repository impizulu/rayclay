/*
    gallery_app.c - the photo gallery's UI.

    A clipped-scroll thumbnail wall in dated sections, with a search box, a tag
    filter and a tile-size control above it, beside a detail pane carrying the
    photograph, its metadata, an editable caption and a details modal. The twelve
    photographs are generated in memory and decoded through rcLoadImageFromMemory,
    so there are no asset files to ship. Side by side where both panes fit; one
    pane at a time below that, with a way back.

    Pure RC_ API - no system includes, and the UI formats no strings.

    Build: cmake --build build --target rayclay_bench_gallery
*/
#define GALLERY_BACKEND_IMPLEMENTATION
#include "gallery_app.h"

#include "icons/rc_icons_panel_left.h"   /* the one-pane arm's back control */
#include "icons/rc_icons_plus.h"          /* the tile-size control */
#include "icons/rc_icons_minus.h"

/* The scripted scenario's length: warmup frames, then a strict hold. */
#define GALLERY_BENCH_WARMUP 80

/* The photo the scenario opens on, SEEDED rather than clicked for. */
#define GALLERY_BENCH_PICK 4

/* Layout constants, px. */
#define GAL_DETAIL_W  380
#define GAL_GRID_GAP  4                   /* mortar between tiles, NOT a margin       */
#define GAL_SECT_PAD  14                  /* the date header's own side gutter        */

/* THE TILE-SIZE CONTROL, the width each step aims a tile at. The column count is
   the width the grid is given divided by this, so one step reflows the wall. */
#define GAL_ZOOM_STEPS 3
#define GAL_TILE_W     220                /* the middle step, and the breakpoint's basis */
static const int GAL_TILE_WIDTHS[GAL_ZOOM_STEPS] = { 320, 220, 156 };

static int gal_tile_width(int zoomStep) {
    if (zoomStep < 0) zoomStep = 0;
    if (zoomStep >= GAL_ZOOM_STEPS) zoomStep = GAL_ZOOM_STEPS - 1;
    return GAL_TILE_WIDTHS[zoomStep];
}

/* The wall's right gutter. rcScrollbar is an OVERLAY - an 8 px bar inset 3 px -
   so it paints over whatever the tiles put there; reserving those 11 px as the
   container's own padding stops the tiles where the bar begins. */
#define GAL_SCROLLBAR_GUTTER 12

/* THE ONE BREAKPOINT, DERIVED FROM THIS APP'S OWN PANES rather than from a table
   of device widths: the detail column plus the two tiles that make the wall a grid
   rather than a list. Below it the two panes take turns. It is measured against
   the width the Body row is given, so a desktop window dragged narrow takes the
   same arm a phone does - which is the only reason it is testable without one. */
#define GAL_ONE_PANE_W  (GAL_DETAIL_W + 2 * GAL_TILE_W + GAL_GRID_GAP)

/* The toolbar band, under the 52 px titlebar. The one-pane arm grows a second
   line for the filter and the count, so the search box can have the first line to
   itself - sharing one narrow line, it was too small to type in. */
#define GAL_TOOLBAR_H    56
#define GAL_COUNT_ROW_H  40

/* The detail pane is two parts and only one has a fixed height: the text column,
   summed from what it declares (18 pad + 30 title + 12 + 18 meta + 12 + 12 label
   + 12 + 60 text area + 12 + 12 + 28 button + 18 pad + 1 rule). THE PHOTOGRAPH
   TAKES EVERY PIXEL THAT LEAVES, because a viewer that stops the picture at a
   fixed height and leaves an empty band under it is showing its layout. The two
   summed are the height below which the pane scrolls instead. */
#define GAL_CHROME_H      (52 + GAL_TOOLBAR_H)
#define GAL_BACK_ROW_H    44
#define GAL_PREVIEW_MIN   230
#define GAL_DETAIL_TEXT_H 245
#define GAL_DETAIL_H      (GAL_PREVIEW_MIN + GAL_DETAIL_TEXT_H)

/* The caption line the scenario types into the rcTextArea. Its LENGTH is the
   point: long enough to soft-wrap to a second row. A newline arrives only through
   the key channel with an ENTER semantic - the text channel drops control bytes. */
static const char GAL__TYPED[] = " Shot at dawn, in soft golden light along the ridge.";

/* Stable ids for the tiles: every interactive element needs one. */
static const char *const THUMB_IDS[GAL_IMG_COUNT] = {
    "thumb00", "thumb01", "thumb02", "thumb03", "thumb04", "thumb05",
    "thumb06", "thumb07", "thumb08", "thumb09", "thumb10", "thumb11",
};

/* A one-time image cache: it outlives gallery_seed's memset, so a re-seed copies
   these handles rather than decoding twelve BMPs again. */
static bool     g_imagesLoaded = false;
static RC_Image g_images[GAL_IMG_COUNT];

/* A GALLERY IS A ROOM, AND A ROOM IS PAINTED NEUTRAL SO THE PICTURES CARRY THE
   COLOUR. A blue-grey chrome competes with every warm frame hanging in it, so the
   preset is re-surfaced in flat grey and blue is spent only on a SELECTED tile and
   on an ACTION. The metrics are the preset's, so the widgets keep their corners. */
RC_Style gallery_style(bool dark) {
    RC_Style s = dark ? rcStyleDark() : rcStyleLight();
    if (dark) {
        s.background = RC_NEUTRAL_950;    /* the wall the tiles hang on   */
        s.surface    = RC_NEUTRAL_900;    /* chrome bands, the detail pane */
        s.surfaceAlt = RC_NEUTRAL_800;
        s.chrome     = RC_NEUTRAL_900;
        s.text       = RC_NEUTRAL_100;
        s.textMuted  = RC_NEUTRAL_400;
        s.border     = RC_NEUTRAL_800;
    } else {
        s.background = RC_NEUTRAL_200;
        s.surface    = RC_NEUTRAL_50;
        s.surfaceAlt = RC_NEUTRAL_200;
        s.chrome     = RC_NEUTRAL_100;
        s.text       = RC_NEUTRAL_900;
        s.textMuted  = RC_NEUTRAL_500;
        s.border     = RC_NEUTRAL_300;
    }
    s.primary      = RC_BLUE_600;
    s.primaryHover = RC_BLUE_500;
    return s;
}

static void gallery_chip(const char *label) {
    RC_Style s = rcGetStyle();
    rcBox(.bg = s.surfaceAlt, .px = 9, .py = 3, .align = "cc",
          .borderRadius = "all-full") {
        rcTextC(label, .font = F_SMALL, .color = s.textMuted);
    }
}

/* An icon button the size of a finger. Polling rcClicked marks the whole box
   clickable, and the pointer cursor comes with that - no cursor call needed. */
static bool nav_button(const char *id, RC_IconCallback icon) {
    RC_Style s = rcGetStyle();
    rcBox(.id = id, .bg = rcIsHovered(id) ? s.surfaceAlt : RC_TRANSPARENT, .align = "cc",
          .borderRadius = "all-lg", .w = "44px", .h = "44px") {
        icon(20.0f, s.textMuted);
    }
    return rcClicked(id);
}

/* A toolbar control. It greys out at the end of its range rather than
   disappearing, so the band never reflows under the pointer. */
static bool tool_button(const char *id, RC_IconCallback icon, bool live,
                        const char *tip) {
    RC_Style s = rcGetStyle();
    rcBox(.id = id, .bg = (live && rcIsHovered(id)) ? s.surfaceAlt : RC_TRANSPARENT,
          .align = "cc", .borderRadius = "all-md", .w = "34px", .h = "34px",
          .tooltip = tip) {
        icon(16.0f, live ? s.text : s.border);
    }
    return live && rcClicked(id);
}

/* Search and tag narrow the same wall, so both go through one call. */
static void gallery_refilter(AppState *st) {
    int t = st->tagFilter;
    if (t < 0 || t >= GAL_TAG_COUNT) t = 0;
    gallery_filter(&st->store, st->search, GAL_TAG_QUERIES[t]);
}

/* The tile-size stepper. Step 0 is the largest, so plus walks down the table. */
static void gallery_zoom_steps(AppState *st) {
    rcRow(.gap = 2, .align = "cl") {
        if (tool_button("zoom_out", rcIconMinus, st->zoomStep < GAL_ZOOM_STEPS - 1,
                        "Smaller tiles"))
            st->zoomStep++;
        if (tool_button("zoom_in", rcIconPlus, st->zoomStep > 0, "Larger tiles"))
            st->zoomStep--;
    }
}

/* A "label ... value" row for the details modal. */
static void info_row(const char *label, const char *value) {
    RC_Style s = rcGetStyle();
    rcRow(.align = "cl", .w = "grow") {
        rcTextC(label, .font = F_SMALL, .color = s.textMuted);
        rcBox(.w = "grow") {}
        rcTextC(value, .font = F_BODY, .color = s.text);
    }
}

/* Draw one photo into a frame the CALLER owns, and report whether it drew - a
   decode can fail, and the caller then draws its own placeholder rather than
   dereferencing a NULL handle. .image STRETCHES its texture to the element, so the
   fit has to be computed here.

   TWO FRAMINGS. `cover` scales by the LARGER ratio, so the frame fills and the
   overflow is cropped: what the wall wants, and the caller must clip for it.
   `contain` takes the smaller ratio, so the whole photograph is visible: what the
   viewer wants. `cover` rounds and `contain` truncates, so neither leaves a seam
   of frame showing nor spills one that does not clip. */
static bool gallery_photo(const RC_Image *img, const GalImage *m, int w, int h, bool cover) {
    if (!img || !img->handle || m->w <= 0 || m->h <= 0)
        return false;
    float scw = (float)w / (float)m->w;
    float sch = (float)h / (float)m->h;
    float sc  = cover ? (scw > sch ? scw : sch) : (scw < sch ? scw : sch);
    int dw = (int)((float)m->w * sc + (cover ? 0.5f : 0.0f));
    int dh = (int)((float)m->h * sc + (cover ? 0.5f : 0.0f));
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    rcBox(.wType = RC_PX(dw), .hType = RC_PX(dh), .image = img) {}
    return true;
}

/* The picture inside a tile, or the "?" a failed decode leaves - its own function
   because both of the tile's element heads contain it. */
static void gallery_tile_photo(AppState *st, int imgIdx, int w, int h) {
    if (!gallery_photo(&st->images[imgIdx], &st->store.img[imgIdx], w, h, true))
        rcTextL("?", .font = F_TITLE, .color = rcGetStyle().textMuted);
}

/* One tile of the wall: the photograph, edge to edge, and nothing else. A caption
   strip under every tile is what makes a grid read as a file listing, and rounded
   tiles at a 4 px gap read as cards - these are photographs.
   TWO ELEMENT HEADS, not one with a ternary: .border's width is a fixed char[] and
   takes a literal only, and a fully transparent border is not free either. */
static void gallery_tile(AppState *st, int imgIdx, int w, int h, bool onePane) {
    RC_Style s = rcGetStyle();
    const char *id = THUMB_IDS[imgIdx];
    bool sel = (imgIdx == st->selected);
    bool hov = rcIsHovered(id);
    if (sel) {
        rcBox(.id = id, .bg = s.surfaceAlt, .align = "cc", .overflow = "hidden",
              .border = { .color = s.primary, .width = "3" },
              .wType = RC_PX(w), .hType = RC_PX(h),
              .overlay = rcAlpha(s.primary, 30)) {
            gallery_tile_photo(st, imgIdx, w, h);
        }
    } else {
        rcBox(.id = id, .bg = s.surfaceAlt, .align = "cc", .overflow = "hidden",
              .wType = RC_PX(w), .hType = RC_PX(h),
              .overlay = hov ? rcAlpha(RC_WHITE, 30) : RC_TRANSPARENT) {
            gallery_tile_photo(st, imgIdx, w, h);
        }
    }
    if (rcClicked(id)) {
        st->selected = imgIdx;
        /* One pane at a time: picking a photo IS the navigation. */
        if (onePane)
            st->detailOpen = true;
    }
}

/* The custom titlebar: brand, spacer and the bundled window controls.
   RC_ID_WINDOW_DRAG makes the band draggable, and a widget inside it loses its
   click to the window move unless RC_ID_WINDOW_NODRAG opts it back out. */
static void gallery_topbar(bool onePane) {
    RC_Style s = rcGetStyle();
    /* CHROME, NOT CONTENT: the drag strip is pinned at .titlebarHeight, so a band
       that scaled with the content zoom would stop matching it. rcUnzoomed()
       counter-scales the row it prefixes; at zoom 1 it changes nothing. */
    rcUnzoomed()
    rcRow(.id = RC_ID_WINDOW_DRAG, .bg = s.chrome, .gap = 10, .px = 14, .align = "cl",
          .w = "grow", .h = "52px") {
        /* Grey, not blue: the accent belongs to a selection and to an action. */
        rcBox(.bg = s.surfaceAlt, .align = "cc", .borderRadius = "all-md", .w = "26px",
              .h = "26px") {
            rcTextL("RC", .font = F_SMALL, .color = s.text);
        }
        /* On a narrow window the title is the band's widest child, and a child
           that will not shrink widens the ROOT past the window and clips every
           pane. So there it lives in a clip box and yields first. */
        if (onePane) {
            rcBox(.overflow = "hidden", .w = "grow") {
                rcTextL("RayClay Gallery", .font = F_TITLE, .color = s.text, .wrap = "n");
            }
        } else {
            rcTextL("RayClay Gallery", .font = F_TITLE, .color = s.text);
            rcBox(.w = "grow") {}
        }
        rcWindowControls();
    }
}

/* The toolbar, and everything in it narrows or resizes the wall below. The theme
   toggle lives HERE and not in the titlebar, because the titlebar is a drag region
   and a widget inside one loses its click to the window move.
   Two element heads rather than one with branches inside: the wide arm is a single
   band and the narrow arm a column of two, so neither tree carries the other's
   wrapper, and narrow the search box gets a line to itself. */
static void gallery_toolbar_wide(AppState *st) {
    RC_Style s = rcGetStyle();
    /* The search box takes ALL the slack rather than sharing it with a spacer: a
       band with a fixed box at one end, a toggle at the other and a void between
       them is the commonest way a toolbar looks unfinished. */
    rcBox(.w = "grow") {
        if (rcTextInput("search", st->search, sizeof st->search,
                         .placeholder = "Search photos"))
            gallery_refilter(st);
    }
    rcBox(.w = "170px") {
        if (rcCombo("tag_filter", &st->tagFilter, GAL_TAG_LABELS, GAL_TAG_COUNT))
            gallery_refilter(st);
    }
    rcTextC(st->store.countStr, .font = F_SMALL, .color = s.textMuted);
    gallery_zoom_steps(st);
    rcBox(.bg = s.border, .w = "1px", .h = "22px") {}
    rcRow(.gap = 8, .align = "cl") {
        rcTextC(st->darkMode ? "Dark" : "Light", .font = F_SMALL, .color = s.textMuted);
        rcToggle("tg_theme", &st->darkMode);
    }
}

static void gallery_toolbar(AppState *st, bool onePane) {
    RC_Style s = rcGetStyle();
    if (onePane) {
        rcColumn(.bg = s.chrome, .w = "grow") {
            rcRow(.gap = 10, .px = 16, .align = "cl", .w = "grow",
                  .hType = RC_PX(GAL_TOOLBAR_H)) {
                rcBox(.w = "grow") {
                    if (rcTextInput("search", st->search, sizeof st->search,
                                     .placeholder = "Search photos"))
                        gallery_refilter(st);
                }
                gallery_zoom_steps(st);
                rcToggle("tg_theme", &st->darkMode);
            }
            /* The second line, which is also where the demo chip docks. */
            rcRow(.gap = 10, .px = 16, .align = "cl", .w = "grow",
                  .hType = RC_PX(GAL_COUNT_ROW_H)) {
                rcBox(.w = "grow", .wMax = 170.0f) {
                    if (rcCombo("tag_filter", &st->tagFilter, GAL_TAG_LABELS,
                                GAL_TAG_COUNT))
                        gallery_refilter(st);
                }
                rcTextC(st->store.countStr, .font = F_SMALL, .color = s.textMuted);
                rcBox(.w = "grow") {}
            }
        }
    } else {
        rcRow(.bg = s.chrome, .gap = 12, .px = 16, .align = "cl", .w = "grow",
              .hType = RC_PX(GAL_TOOLBAR_H)) {
            gallery_toolbar_wide(st);
        }
    }
}

/* One dated section: a header band, then the photographs taken that month.

   EVERY ROW FILLS THE WALL, AND THAT IS THE WHOLE JOB HERE. A fixed column count
   leaves a rectangle of bare background wherever a month's count is not a multiple
   of it, and no photo app a reader has used does that. Two rules avoid it with no
   per-photo measurement: the rows are BALANCED, so four photos in three columns
   are 2 + 2 rather than 3 + 1; and each row then divides the WHOLE wall among the
   tiles it actually holds, the integer remainder spent a pixel at a time across
   the leading tiles. A short row is therefore wider and taller than a full one,
   which is the mosaic a photo wall wants, and the cap stops a lone photograph
   becoming a wall of its own. The tiles crop, so none of this distorts a frame. */
static void gallery_section(AppState *st, const GalSection *sec, int cols, int wallW,
                            bool onePane) {
    RC_Style s = rcGetStyle();
    rcRow(.bg = s.surface, .gap = 10, .pt = 18, .pb = 8, .px = GAL_SECT_PAD,
          .align = "cl", .w = "grow") {
        rcTextC(sec->label, .font = F_MD, .color = s.text);
        rcTextC(sec->countStr, .font = F_SMALL, .color = s.textMuted);
    }
    int rows   = (sec->count + cols - 1) / cols;
    int placed = 0;
    for (int r = 0; r < rows; r++) {
        int left = sec->count - placed;
        int n    = left / (rows - r) + (left % (rows - r) ? 1 : 0);
        int cellW = (wallW - (n - 1) * GAL_GRID_GAP) / n;
        if (cellW < 12) cellW = 12;           /* a 1 px photo at the narrowest window */
        int slack = wallW - (n * cellW + (n - 1) * GAL_GRID_GAP);
        if (slack < 0) slack = 0;
        int cellH = cellW * 2 / 3;            /* 3:2 landscape, the frame most photos are */
        if (cellH > wallW / 3) cellH = wallW / 3;
        rcRow(.gap = GAL_GRID_GAP, .w = "grow") {
            for (int c = 0; c < n; c++)
                gallery_tile(st, st->store.visible[sec->first + placed + c],
                             cellW + (c < slack ? 1 : 0), cellH, onePane);
        }
        placed += n;
    }
}

/* The clipped-scroll wall, broken into dated sections. The column count comes
   from the width the grid is given and the tile width the size control asks for,
   so the wall reflows with the window, the zoom and that control.
   EDGE TO EDGE, WITH ONE GUTTER: the container's only padding is the scrollbar
   gutter, so the only space in the wall is the mortar between tiles. The count is
   taken BEFORE the gutter comes off, so the gutter costs each tile a few pixels
   and never a whole column. Two columns is the floor - one is a list. */
static void gallery_grid(AppState *st, int gridW, bool onePane) {
    RC_Style s = rcGetStyle();
    int tileW = gal_tile_width(st->zoomStep);
    int cols  = (gridW + GAL_GRID_GAP) / (tileW + GAL_GRID_GAP);
    if (cols < 2) cols = 2;
    int wallW = gridW - GAL_SCROLLBAR_GUTTER; /* the width the tiles actually divide */
    if (wallW < 24) wallW = 24;

    rcColumn(.id = "GridScroll", .bg = s.background, .gap = GAL_GRID_GAP,
             .pr = GAL_SCROLLBAR_GUTTER, .scroll = "v", .w = "grow", .h = "grow") {
        if (st->store.visibleCount <= 0) {
            rcColumn(.gap = 6, .align = "cc", .w = "grow", .h = "grow") {
                rcTextL("No photos match.", .font = F_MD, .color = s.textMuted);
                rcTextL("Try another word, or a different tag.", .font = F_SMALL,
                        .color = s.textMuted);
            }
        } else {
            for (int i = 0; i < GAL_SECT_COUNT; i++) {
                /* A filter can empty a month; its header goes with it. */
                if (st->store.sect[i].count > 0)
                    gallery_section(st, &st->store.sect[i], cols, wallW, onePane);
            }
        }
    }
}

/* The detail pane's content: the photograph over its mount, then a padded column
   carrying the title, the metadata, an editable caption (rcTextArea) and the info
   action. paneW and previewH are the mount's box, which sizes the fit. */
static void gallery_detail_body(AppState *st, const GalImage *m, const RC_Image *img,
                                int paneW, int previewH, bool onePane) {
    RC_Style s = rcGetStyle();
    /* THE ONE CONTROL THE ONE-PANE ARM ADDS: alone on the screen, the photo needs
       a way back to the wall. The selection is deliberately left alone - closing a
       view is not changing a selection, and the wide arm still needs one. */
    if (onePane) {
        rcRow(.gap = 8, .px = 8, .align = "cl", .w = "grow",
              .hType = RC_PX(GAL_BACK_ROW_H)) {
            if (nav_button("nav_back", rcIconPanelLeft))
                st->detailOpen = false;
            rcTextL("All photos", .font = F_BODY, .color = s.textMuted);
        }
    }
    /* Aspect-FIT here rather than the wall's crop, because the whole frame has to
       be visible - so a matte shows on the short axis. THE MATTE MUST BE READABLY
       DARKER THAN THE PANE OR IT IS NOT A MOUNT, it is a hole: one tone of
       difference is invisible at screen size in a dark theme, where a black wash
       over the pane's own colour is a real step in both themes. */
    rcBox(.bg = rcAlpha(RC_BLACK, 150), .align = "cc", .overflow = "hidden",
          .w = "grow", .hType = RC_PX(previewH)) {
        if (!gallery_photo(img, m, paneW, previewH, false))
            rcTextL("No preview", .font = F_MD, .color = s.textMuted);
    }
    rcBox(.bg = s.border, .w = "grow", .h = "1px") {}
    rcColumn(.gap = 12, .p = 18, .w = "grow", .h = "grow") {
        /* It wraps rather than clipping: a title one word too long is still one. */
        rcTextC(m->title, .font = F_TITLE, .color = s.text);
        rcRow(.gap = 10, .align = "cl") {
            rcTextC(m->date, .font = F_SMALL, .color = s.textMuted);
            rcTextC(m->dim, .font = F_SMALL, .color = s.textMuted);
            gallery_chip(m->tag);
        }
        rcTextL("Caption", .font = F_SMALL, .color = s.textMuted);
        rcTextArea("gal_caption", st->caption, sizeof st->caption, .rows = 4);

        rcBox(.w = "grow", .h = "grow") {}
        /* The pane's only action, and so the only thing in it allowed to be blue:
           a photo library spends its accent on what is selected and on what to do
           next, and this is the second of those. */
        if (rcButton("btn_info", "Photo info", RC_BTN_PRIMARY))
            st->modalInfo = true;
    }
}

/* The detail pane: a fixed column beside the wall, or alone and growing when it
   is the only pane, and a scroll container where the window is too short for it.

   TRAP: .scroll is a fixed char[] and cannot take a ternary. .className is a
   const char * on the same record and reaches the same clip, so
   `.className = scrolls ? "overflow-y-auto" : ""` is ONE element head - reach for
   that whenever a fixed char[] field is all that stands between you and one head.
   Two heads are kept here only because the arms also differ by .id.

   THE PREVIEW IS THE PANE'S SLACK: where the pane fits it takes the body less the
   text column, so a taller window spends its height on the picture; where it does
   not it yields to 45% of the body, leaving the title in view as the cue that
   there is more below. */
static void gallery_detail(AppState *st, int paneW, int bodyH, bool onePane, bool scrolls) {
    RC_Style s = rcGetStyle();
    int i = st->selected;
    if (i < 0 || i >= GAL_IMG_COUNT)
        return;                              /* guard BEFORE opening the element */
    int previewH = bodyH - GAL_DETAIL_TEXT_H - (onePane ? GAL_BACK_ROW_H : 0);
    if (scrolls && bodyH * 45 / 100 < previewH)
        previewH = bodyH * 45 / 100;
    if (previewH < 100) previewH = 100;
    /* Sync the caption buffer to the selected photo in THIS pass, so the pane never
       shows a one-frame stale caption. SAVE BEFORE LOAD: one buffer serves all
       twelve photos, so whatever the user typed must go back to the photo they are
       leaving before the buffer is reloaded - otherwise the edit is lost with
       nothing on screen saying so. */
    if (st->captionOf != i) {
        gallery_store_caption(&st->store, st->captionOf, st->caption);
        gallery_load_caption(&st->store, i, st->caption, sizeof st->caption);
        st->captionOf = i;
    }
    const GalImage *m = &st->store.img[i];
    const RC_Image *img = &st->images[i];

    if (scrolls) {
        rcColumn(.id = "DetailScroll", .bg = s.surface, .scroll = "v",
                 .h = "grow", .wType = onePane ? RC_GROW : RC_PX(GAL_DETAIL_W)) {
            gallery_detail_body(st, m, img, paneW, previewH, onePane);
        }
    } else {
        rcColumn(.bg = s.surface, .h = "grow",
                 .wType = onePane ? RC_GROW : RC_PX(GAL_DETAIL_W)) {
            gallery_detail_body(st, m, img, paneW, previewH, onePane);
        }
    }
}

/* The photo-details modal, placed OUTSIDE the root so its scrim covers the window.
   Its 360 px card is wider than a phone less its safe bands, so the one-pane arm
   sizes it in vw instead.
   THE DISPLAY FACE LIVES HERE, not in the pane: F_HERO is 56 px, and a heading
   that size needs the whole panel width rather than a 380 px column beside the
   grid. It is also the one place in the app that shows text staying crisp at 2x
   zoom, which is what a face this large is for. */
static void gallery_modal(AppState *st, bool onePane) {
    RC_Style s = rcGetStyle();
    int i = st->selected;
    if (i < 0 || i >= GAL_IMG_COUNT)
        return;
    const GalImage *m = &st->store.img[i];

    if (rcBeginModal("modal_info", &st->modalInfo)) {
        rcColumn(.bg = s.surface, .gap = 12, .p = 18, .borderRadius = "all-xl",
                 .w = onePane ? "84vw" : "360px") {
            rcTextC(m->title, .font = F_HERO, .color = s.text);
            info_row("Taken", m->date);
            info_row("Dimensions", m->dim);
            info_row("Category", m->tag);
            rcRow(.gap = 8) {
                if (rcButton("btn_info_ok", "Close", RC_BTN_PRIMARY))
                    st->modalInfo = false;
            }
        }
        rcEndModal();
    }
}

void gallery_seed(AppState *st, unsigned seed) {
    gallery_memzero(st, sizeof *st);          /* zero all, including padding, then set */
    gallery_backend_seed(&st->store, seed);   /* metadata, display strings, first filter */
    st->selected   = GALLERY_BENCH_PICK;
    st->captionOf  = -1;                      /* force a caption sync on the first pass */
    st->darkMode   = true;
    st->zoomStep   = 1;                       /* the middle tile size */
    st->tagFilter  = 0;                       /* every tag */
    st->detailOpen = false;                   /* narrow: open on the wall, not the photo */
    st->seeded     = true;

    /* One decode per process, not per seed. */
    if (!g_imagesLoaded) {
        /* STATIC, not automatic: this buffer is ~324 KB and the web build's default
           stack is 64 KB. It is written and consumed entirely inside this one-time
           branch, so it holds no state a re-seed could observe. */
        static unsigned char buf[GAL_BMP_CAP];
        for (int i = 0; i < GAL_IMG_COUNT; i++) {
            size_t len = gallery_encode_bmp(&st->store, i, buf, sizeof buf);
            /* RC_LIT spells a compound literal so the line compiles as C and C++. */
            g_images[i] = len ? rcLoadImageFromMemory(buf, (int)len) : RC_LIT(RC_Image){0};
        }
        g_imagesLoaded = true;
    }
    for (int i = 0; i < GAL_IMG_COUNT; i++)
        st->images[i] = g_images[i];
}

void gallery_update(AppState *st, const AppCtx *ctx) {
    (void)st;
    (void)ctx;
    /* Nothing to advance: the content is static, and the caption sync - the only
       state this app moves - runs in gallery_detail, so a selection made this frame
       is reflected in the same frame. */
}

void gallery_layout(AppState *st, const AppCtx *ctx) {
    rcSetStyle(gallery_style(st->darkMode));
    RC_Style s = rcGetStyle();

    /* Safe area in layout units - a phone's status bar, cutout and home indicator.
       Zero on desktop, so Root's padding is a no-op there. */
    RC_Insets safe = app_safe(ctx);

    /* Branch on the SPACE, never the platform. bodyW and bodyH are what the Body
       row is given once Root has spent the safe bands. */
    const int  bodyW   = (int)(app_view_w(ctx) - safe.left - safe.right);
    const bool onePane = bodyW < GAL_ONE_PANE_W;
    const int  bodyH   = (int)(app_view_h(ctx) - safe.top - safe.bottom) - GAL_CHROME_H
                         - (onePane ? GAL_COUNT_ROW_H : 0);
    /* Two ideas, two bools: a window is easily wide enough and too short. */
    const bool detailScrolls = bodyH < GAL_DETAIL_H + (onePane ? GAL_BACK_ROW_H : 0);
    const bool showGrid      = !onePane || !st->detailOpen;
    const bool showDetail    = !onePane || st->detailOpen;

    rcColumn(.id = "Root", .bg = s.background, .pt = (uint16_t)safe.top,
             .pb = (uint16_t)safe.bottom, .pl = (uint16_t)safe.left,
             .pr = (uint16_t)safe.right, .w = "grow", .h = "grow") {
        gallery_topbar(onePane);
        gallery_toolbar(st, onePane);
        rcRow(.id = "Body", .w = "grow", .h = "grow") {
            /* Wide: wall AND detail. One pane: the wall is home and the detail is
               what a tap opens, so the two are exclusive. The wall is handed the
               width it will actually get, because that is what its tiles divide. */
            if (showGrid)
                gallery_grid(st, onePane ? bodyW : bodyW - GAL_DETAIL_W, onePane);
            if (showDetail)
                gallery_detail(st, onePane ? bodyW : GAL_DETAIL_W, bodyH, onePane,
                               detailScrolls);
        }
    }
    gallery_modal(st, onePane);               /* the modal sits outside Root */

    /* EACH BAR CARRIES THE CONDITION ITS PANE CARRIES - the Body block above read
       back, arm for arm. rcScrollbar draws the bar for a container laid out THIS
       frame; name one this frame did not build and the call is dropped with a
       warning. The detail pane takes a second condition because it only clips when
       the height makes it. */
    if (showGrid)                    rcScrollbar("GridScroll");
    if (showDetail && detailScrolls) rcScrollbar("DetailScroll");
}

void gallery_demo_chrome(AppState *st, const AppCtx *ctx) {
    if (ctx->mode != APP_DEMO || !ctx->arena)
        return;
    /* A passive readout: floating, so it never reflows the UI, and PASSTHROUGH, so
       clicks fall through it. Narrow it docks in the toolbar's second line; wide it
       takes the bottom LEFT corner, because the bottom right is the pane's action
       row and a debug chip level with a primary button reads as a button. */
    RC_Insets safe = app_safe(ctx);
    const bool onePane = (int)(app_view_w(ctx) - safe.left - safe.right) < GAL_ONE_PANE_W;
    RC_String hud = onePane
        ? rcFormat(ctx->arena, "%.0f fps", ctx->fps)
        : rcFormat(ctx->arena, "%.0f fps \xc2\xb7 %s", ctx->fps, st->store.countStr);
    const RC_Anchor corner = onePane ? RC_ANCHOR_TOP_RIGHT : RC_ANCHOR_BOTTOM_LEFT;
    const float     dx     = onePane ? -16.0f - safe.right : 16.0f + safe.left;
    const float     dy     = onePane ? safe.top + (float)(GAL_CHROME_H + 8)
                                     : -16.0f - safe.bottom;
    rcBox(.id = "demo_hud", .bg = rcAlpha(RC_BLACK, 150), .px = 10, .py = 5,
           .borderRadius = "all-full",
           .floating = { .to = RC_ATTACH_ROOT, .parent = corner, .element = corner,
                         .offset = { dx, dy },
                         .capture = RC_CAPTURE_PASSTHROUGH }) {
        rcText(hud, .font = F_SMALL, .color = RC_WHITE);
    }
}

void gallery_bench_step(AppState *st, const AppInputSink *in, int frame) {
    (void)st;   /* every action is synthetic input; the selection is the seeded one */
    /* The scripted scenario. Every action goes through the input sink, so the real
       hit-test, focus and caret paths run; at GALLERY_BENCH_WARMUP the app holds.
       The caret blink and tooltip dwell run on real time, not the injected dt, so
       the held frame must carry no focused input and no hovered tooltip - hence the
       blur and off-canvas park below.
       THE CAPTION COORDINATE IS DERIVED, NOT GUESSED: at 1280x720 the pane's stack
       is 108 chrome + the preview (the body less GAL_DETAIL_TEXT_H) + 1 rule + 18
       pad + 30 title + 12 + 18 meta + 12 + 12 label + 12, which puts the text area
       at y 590. Re-derive it whenever that stack changes - a miss types into
       nothing, and nothing asserts on that. */
    if (!in || frame >= GALLERY_BENCH_WARMUP)
        return;                                        /* the HOLD */

    if (frame >= 6 && frame < 12) {
        in->move(in->ctx, 300.0f, 300.0f);            /* hover the grid, then scroll it */
        in->wheel(in->ctx, 0.0f, -1.0f);              /* (pointer over the grid first)   */
    } else if (frame == 16) {
        in->move(in->ctx, 1180.0f, 620.0f);           /* focus the caption (end-clamped) */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == 17) {
        in->button(in->ctx, APP_MBTN_LEFT, false);
    } else if (frame >= 20 && frame < 20 + (int)sizeof(GAL__TYPED) - 1) {
        in->text(in->ctx, (unsigned int)(unsigned char)GAL__TYPED[frame - 20]);  /* type a caption line */
    } else if (frame == GALLERY_BENCH_WARMUP - 2) {
        in->move(in->ctx, -100.0f, -100.0f);          /* blur the caption + park off-canvas: press ... */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == GALLERY_BENCH_WARMUP - 1) {
        in->button(in->ctx, APP_MBTN_LEFT, false);     /* ... release; pointer stays off-canvas into the hold */
    }
}
