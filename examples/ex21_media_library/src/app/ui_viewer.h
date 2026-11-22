/*
    ui_viewer.h - the lightbox: a photo opened from the wall, its filmstrip, Info and editor

    Included once by gallery_app.c. The picture is one .floating box whose rectangle eases
    toward where the current layout wants it, and that one rule gives the opening flight
    from the tile, the slide when Info docks, and the turn when a crop changes the shape.
    The box covers itself with the picture, so on the way from a phone's square tile the
    photo opens out of its crop and is never stretched.

    The editor works on real pixels. Each change runs gal_edit_apply over the 1280 px
    original and hands the result to rcImageUpdate, as a canvas's putImageData does; a
    slider drag repaints at the frame rate because the tone curve is a lookup table.
*/
#ifndef UI_VIEWER_H
#define UI_VIEWER_H

#define VIEWER_PANEL  320     /* Info and the editor, docked beside the photo */
#define VIEWER_FILM   72      /* the filmstrip's band                         */
#define VIEWER_THUMB  52
#define VIEWER_ACTION 56      /* a phone's row of actions                     */
#define VIEWER_SHEET  344     /* a phone's editor, under the photo            */
#define VIEWER_STAGE_MIN 120  /* the photo never gets less height than this   */

static void player_open_slideshow(AppState *st);   /* ui_player.h: the Slideshow button */

/* Apple's editor marks itself with yellow, so nobody mistakes it for browsing. */
static RC_Style viewer_style(bool editing) {
    RC_Style s = gallery_style(true);
    s.background = RC_BLACK;
    s.surface = rcHex(0x1c1c1e);
    s.surfaceAlt = rcHex(0x2c2c2e);
    if (editing) {
        s.primary = rcHex(0xffd60a);
        s.primaryHover = rcHex(0xffe033);
    }
    return s;
}

/* A panel goes beside the photo in a wide window, and in a window lying on its side, where a
   sheet under the photo would leave it no height. */
static bool viewer_wide(const GalFrame *f) {
    const float w = (float)f->viewW - f->safe.left - f->safe.right;
    return w >= 760.0f || (w >= 560.0f && w > (float)f->viewH * 1.2f);
}

/* A phone's editor sheet: at most half the window, and it scrolls. */
static float viewer_sheet_h(const GalFrame *f) {
    const float half = (float)f->viewH * 0.5f;
    return half < VIEWER_SHEET ? half : VIEWER_SHEET;
}

static bool viewer_has_film(const AppState *st, const GalFrame *f) {
    return !st->viewer.editing && f->viewH >= 520;
}

/* The box the photo fits into, from this frame's geometry: nothing here waits on a layout. */
static GalRect viewer_stage(const AppState *st, const GalFrame *f) {
    const bool wide = viewer_wide(f);
    const bool panel = st->viewer.editing || st->modalInfo;
    GalRect r;
    r.x = f->safe.left;
    r.y = f->bandH + f->safe.top;
    r.w = (float)f->viewW - f->safe.left - f->safe.right - (wide && panel ? VIEWER_PANEL : 0.0f);
    r.h = (float)f->viewH - r.y - f->safe.bottom - (viewer_has_film(st, f) ? VIEWER_FILM : 0.0f);
    if (!wide)
        r.h -= st->viewer.editing ? viewer_sheet_h(f) : VIEWER_ACTION;
    r.h = r.h < VIEWER_STAGE_MIN ? VIEWER_STAGE_MIN : r.h;
    return r;
}

/* The photo's shape as it stands: the edit in progress, else the one it was saved with. */
static float viewer_aspect(const AppState *st) {
    const GalPhoto *p = &st->store.photo[st->selected];
    return gal_edit_aspect(p->sourceAspect, st->viewer.editing ? &st->viewer.work : &p->edit);
}

static GalRect viewer_fit(GalRect stage, float aspect, float pad) {
    float w = stage.w - pad * 2.0f, h = stage.h - pad * 2.0f;
    if (w / h > aspect)
        w = h * aspect;
    else
        h = w / aspect;
    GalRect r = { stage.x + (stage.w - w) * 0.5f, stage.y + (stage.h - h) * 0.5f, w, h };
    return r;
}

static int viewer_index(const AppState *st) {
    for (int k = 0; k < st->store.visibleCount; k++)
        if (st->store.visible[k] == st->selected)
            return k;
    return -1;
}

/* Opens @p photo, flying up from the tile at @p from, in window coordinates. */
static void viewer_open(AppState *st, int photo, GalRect from) {
    st->selected = photo;
    gallery_sync_caption(st);
    st->viewer.open = true;
    st->viewer.closing = false;
    st->viewer.fade = 0.0f;
    st->viewer.rect = from;
    int k = viewer_index(st);
    st->viewer.filmPos = (float)(k < 0 ? 0 : k);
}

static void viewer_step(AppState *st, int by) {
    int k = viewer_index(st) + by;
    if (k >= 0 && k < st->store.visibleCount) {
        st->selected = st->store.visible[k];
        gallery_sync_caption(st);
        /* A new picture lands in place: only its shape eases. */
    }
}

/* Going back flies the photo home, so first the wall is scrolled to show its tile. */
static void viewer_close(AppState *st, const GalFrame *f) {
    st->viewer.closing = true;
    st->viewer.editing = false;
    st->modalInfo = false;
    if (st->wall.state[st->selected] == GAL_WALL_SHOWN) {
        const GalRect *r = &st->wall.photo[st->selected];
        float y = r->y - st->wallScroll;
        if (y < f->bandH || y + r->h > f->wallH) {
            float room = st->wall.height - f->wallH;
            float want = r->y + r->h * 0.5f - f->wallH * 0.5f;
            st->wallScroll = want < 0.0f ? 0.0f : want > room ? (room < 0.0f ? 0.0f : room) : want;
        }
    }
}

static void viewer_commit_edit(AppState *st, int photo, const GalEdit *edit) {
    GalPhoto *p = &st->store.photo[photo];
    p->edit = *edit;
    p->aspect = gal_edit_aspect(p->sourceAspect, edit);
    st->sharp[photo] = 0;                 /* the tile re-renders with the edit */
    st->filterDirty = true;               /* and the wall reflows to its shape */
}

/* A quarter turn from the toolbar: edits the saved photo directly. */
static void viewer_rotate(AppState *st) {
    GalPhoto *p = &st->store.photo[st->selected];
    GalEdit   e = p->edit;
    GalFull  *slot = full_slot_of(st->selected);
    e.turns = (e.turns + 1) & 3;
    viewer_commit_edit(st, st->selected, &e);
    if (slot && slot->ready)
        full_show(slot, &e);
}

static void viewer_delete(AppState *st, const GalFrame *f) {
    const int k = viewer_index(st);
    const int was = st->selected;
    if (st->store.keep == was)
        st->store.keep = -1;
    if (st->store.visibleCount <= 1) {
        viewer_close(st, f);
    } else {
        st->selected = st->store.visible[k + 1 < st->store.visibleCount ? k + 1 : k - 1];
        gallery_sync_caption(st);
    }
    photo_set_deleted(st, was, st->scope != GAL_SCOPE_DELETED);
}

/* Asks for the open photo at viewer size, then its two neighbors. */
static void viewer_want(const AppState *st, int *want, int *wants) {
    if (!st->viewer.open || st->viewer.closing)
        return;
    const int k = viewer_index(st);
    want[(*wants)++] = st->selected;
    if (k >= 0 && k + 1 < st->store.visibleCount)
        want[(*wants)++] = st->store.visible[k + 1];
    if (k > 0)
        want[(*wants)++] = st->store.visible[k - 1];
}

/* Under the player the viewer is not laid out, so its eases hold where they are until the
   player closes: they are owed no frame then, a paused slideshow over them included. */
static bool viewer_animating(const AppState *st) {
    return st->viewer.open && !st->player.open
        && (st->viewer.fade < 1.0f || st->viewer.closing || st->viewer.easing);
}

static void viewer_begin_edit(AppState *st) {
    st->viewer.work = st->store.photo[st->selected].edit;
    st->viewer.editing = true;
    st->viewer.editTab = GAL_EDIT_ADJUST;
}

/* True when the viewer hides everything under it, so the wall need not be declared. */
static bool viewer_covers(const AppState *st) {
    return st->viewer.open && !st->viewer.closing && st->viewer.fade >= 1.0f;
}

static void viewer_keys(AppState *st, const GalFrame *f) {
    /* A viewer flying home takes no keys: an edit or an Info begun then would outlive it and
       open with whatever photo is opened next. */
    if (st->viewer.closing)
        return;
    if (rcKeyPressed(RC_KEY_ESCAPE)) {
        if (st->viewer.editing) {
            st->viewer.editing = false;
            GalFull *slot = full_slot_of(st->selected);
            if (slot && slot->ready)
                full_show(slot, &st->store.photo[st->selected].edit);
        } else {
            viewer_close(st, f);
        }
        return;
    }
    if (rcIsFocused("gal_caption") || rcModDown(RC_MOD_PRIMARY) || st->viewer.editing)
        return;
    if (rcKeyPressed(RC_KEY_LEFT)) viewer_step(st, -1);
    if (rcKeyPressed(RC_KEY_RIGHT)) viewer_step(st, 1);
    if (rcKeyPressed(RC_KEY_I)) st->modalInfo = !st->modalInfo;
    /* A deleted photo can only be looked at, or recovered. */
    if (st->scope == GAL_SCOPE_DELETED)
        return;
    if (rcKeyPressed(RC_KEY_R)) viewer_rotate(st);
    if (rcKeyPressed(RC_KEY_PERIOD))
        photo_set_favorite(st, st->selected, !st->store.photo[st->selected].favorite);
    if (rcKeyPressed(RC_KEY_E) || rcKeyPressed(RC_KEY_ENTER))
        viewer_begin_edit(st);
}

/* One line of a photo's facts in a dark card. */
static void viewer_fact(const char *text, int size, RC_Color ink) {
    rcTextC(text, .color = ink, .size = (uint16_t)size);
}

/* The record Apple's Info panel keeps: caption, camera, lens, the shutter's numbers, where. */
static void viewer_info_body(AppState *st, const GalFrame *f, const GalPhoto *p) {
    const RC_Style *s = &f->style;
    const char *cell[5] = { p->iso, p->focal, p->ev, p->fstop, p->shutter };
    ui_strong(p->title, 17, s->text);
    viewer_fact(p->when, 13, s->textMuted);
    rcTextArea("gal_caption", st->caption, sizeof st->caption,
               .className = "bg-surface-alt border-surface-alt", .placeholder = "Add a Caption",
               .rows = 3);
    rcColumn(.bg = s->surfaceAlt, .gap = 4, .p = 12, .borderRadius = "all-lg", .w = "grow") {
        ui_strong(p->camera, 13, s->text);
        viewer_fact(p->lens, 12, s->textMuted);
        viewer_fact(p->specs, 12, s->textMuted);
        rcRow(.gap = 4, .pt = 6, .w = "grow", .className = "flex-wrap") {
            for (int k = 0; k < 5; k++) {
                rcBox(.px = 6, .py = 3, .borderRadius = "all-sm",
                      .className = "bg-[#3a3a3c]") {
                    ui_label(cell[k], 11, s->text);
                }
            }
        }
    }
    rcColumn(.bg = s->surfaceAlt, .gap = 4, .p = 12, .borderRadius = "all-lg", .w = "grow") {
        ui_strong(p->place, 13, s->text);
        viewer_fact(p->file, 12, s->textMuted);
    }
    rcRow(.gap = 6, .w = "grow", .className = "flex-wrap") {
        for (int a = 0; a < GAL_ALBUM_COUNT; a++) {
            if (p->albums & (1u << a)) {
                rcBox(.bg = s->surfaceAlt, .px = 10, .py = 4, .borderRadius = "all-full") {
                    ui_label(GAL_ALBUM_NAMES[a], 12, s->text);
                }
            }
        }
    }
}

/* One tone slider: its name and value on a line, the track under them. The value reads as a
   signed percent: "+25", "-40", "0". */
static void edit_slider(AppState *st, const char *id, const char *label, float *v, float lo,
                        bool ready, const GalFrame *f) {
    const int percent = (int)(*v * 100.0f + (*v < 0.0f ? -0.5f : 0.5f));
    rcColumn(.gap = 2, .w = "grow") {
        rcRow(.align = "cl", .w = "grow") {
            ui_label(label, 13, f->style.text);
            rcSpacer() {}
            ui_label(rcFormat(&g_text, percent > 0 ? "+%d" : "%d", percent).chars, 12,
                     f->style.textMuted);
        }
        if (rcSlider(id, v, lo, 1.0f, .disabled = !ready))
            st->viewer.workDirty = true;
    }
}

/* Adjust | Crop, the editor's own switch. */
static void edit_tabs(AppState *st, const GalFrame *f) {
    static const char *const ID[2] = { "edit_adjust", "edit_crop" };
    static const char *const LABEL[2] = { "Adjust", "Crop" };
    rcRow(.gap = 2, .p = 2, .borderRadius = "all-full", .w = "grow",
          .className = "bg-surface-alt") {
        for (int t = 0; t < 2; t++) {
            const bool on = st->viewer.editTab == (GalEditTab)t;
            rcBox(.id = ID[t], .focusable = true, .align = "cc", .borderRadius = "all-full",
                  .w = "grow", .hType = RC_PX(f->coarse ? 44 : 28),
                  .className = on ? "bg-primary" : "hover:bg-[#3a3a3c]") {
                ui_label(LABEL[t], 13, on ? RC_BLACK : f->style.text);
            }
            if (rcClicked(ID[t]))
                st->viewer.editTab = (GalEditTab)t;
        }
    }
}

static void edit_crop_page(AppState *st, const GalFrame *f) {
    static const char *const ID[GAL_CROP_COUNT] = { "crop0", "crop1", "crop2", "crop3", "crop4" };
    GalEdit *w = &st->viewer.work;
    rcColumn(.gap = 4, .w = "grow") {
        for (int c = 0; c < GAL_CROP_COUNT; c++) {
            const bool on = w->crop == c;
            rcRow(.id = ID[c], .focusable = true, .px = 10, .align = "cl",
                  .borderRadius = "all-md", .w = "grow", .hType = RC_PX(f->coarse ? 44 : 32),
                  .className = "hover:bg-surface-alt") {
                ui_label(GAL_CROP_NAMES[c], 13, on ? f->style.primary : f->style.text);
                rcSpacer() {}
                if (on)
                    rcIconCheck(14.0f, f->style.primary);
            }
            if (rcClicked(ID[c])) {
                w->crop = c;
                st->viewer.workDirty = true;
            }
        }
    }
    rcBox(.bg = f->style.border, .w = "grow", .h = "1px") {}
    rcRow(.gap = 8, .w = "grow") {
        if (rcButton("turn_left", "Turn Left", .className = "tap")) {
            w->turns = (w->turns + 3) & 3;
            st->viewer.workDirty = true;
        }
        if (rcButton("turn_right", "Turn Right", .className = "tap")) {
            w->turns = (w->turns + 1) & 3;
            st->viewer.workDirty = true;
        }
    }
}

/* The editor's panel. Holding Compare shows the photo as shot. */
static void edit_panel(AppState *st, const GalFrame *f, bool ready) {
    GalEdit *w = &st->viewer.work;
    edit_tabs(st, f);
    if (st->viewer.editTab == GAL_EDIT_ADJUST) {
        edit_slider(st, "tone_exp", "Exposure", &w->exposure, -1.0f, ready, f);
        edit_slider(st, "tone_con", "Contrast", &w->contrast, -1.0f, ready, f);
        edit_slider(st, "tone_sat", "Saturation", &w->saturation, -1.0f, ready, f);
        edit_slider(st, "tone_war", "Warmth", &w->warmth, -1.0f, ready, f);
        edit_slider(st, "tone_vig", "Vignette", &w->vignette, 0.0f, ready, f);
    } else {
        edit_crop_page(st, f);
    }
    rcSpacer() {}
    rcRow(.gap = 8, .w = "grow") {
        if (rcButton("edit_revert", "Revert to Original", .disabled = gal_edit_is_identity(w),
                     .className = "tap", .variant = RC_BTN_GHOST)) {
            const GalEdit none = { 0 };
            *w = none;
            st->viewer.workDirty = true;
        }
        rcSpacer() {}
        rcBox(.id = "edit_compare", .focusable = true, .px = 12, .align = "cc",
              .borderRadius = "all-md", .tooltip = "Hold to see the photo as shot",
              .className = "tap bg-surface-alt hover:bg-[#3a3a3c]") {
            ui_label("Compare", 13, f->style.text);
        }
    }
    /* A hold begins with a press on the button, never with a drag that crosses it, and lasts
       until the button comes up, wherever the pointer is by then. Space holds it from the
       keyboard. */
    const bool held = rcPressed("edit_compare")
                      || (st->viewer.comparing && rcPointerDown(RC_POINTER_LEFT))
                      || (rcIsFocused("edit_compare") && rcKeyDown(RC_KEY_SPACE));
    if (held != st->viewer.comparing) {
        st->viewer.comparing = held;
        GalFull *slot = full_slot_of(st->selected);
        const GalEdit none = { 0 };
        if (slot && slot->ready)
            full_show(slot, held ? &none : w);
    }
}

/* The strip along the foot: the current photo in the middle, its neighbors sliding by. It
   draws only what fits, so the whole library costs what the window shows. */
static void viewer_film(AppState *st, const GalFrame *f, int index, float fade) {
    const float W = (float)f->viewW - f->safe.left - f->safe.right;
    const float step = (float)VIEWER_THUMB + 4.0f;
    rcBox(.id = "Film", .overflow = "hidden", .w = "grow", .hType = RC_PX(VIEWER_FILM),
          .overlay = rcWithAlpha(RC_BLACK, 1.0f - fade)) {
        for (int k = 0; k < st->store.visibleCount; k++) {
            const int   i = st->store.visible[k];
            const float x = W * 0.5f - (float)VIEWER_THUMB * 0.5f
                            + ((float)k - st->viewer.filmPos) * step;
            if (x < -(float)VIEWER_THUMB || x > W)
                continue;
            const float thumb = (float)VIEWER_THUMB * f->pixels;
            picture_want(st, i, thumb, thumb);
            rcBox(.id = st->filmId[i], .focusable = true, .borderRadius = "all-sm",
                  .border = { .color = k == index ? RC_WHITE : RC_UNSET_COLOR, .width = 2 },
                  .wType = RC_PX(VIEWER_THUMB), .hType = RC_PX(VIEWER_THUMB),
                  .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                                .element = RC_ANCHOR_TOP_LEFT,
                                .offset = { x, (VIEWER_FILM - VIEWER_THUMB) * 0.5f } },
                  .image = &g_thumb[i], .imageFit = RC_IMAGE_COVER) {}
            if (rcClicked(st->filmId[i])) {
                st->selected = i;
                gallery_sync_caption(st);
            }
        }
    }
}

/* What a photo's toolbar can do. On a phone the same buttons sit in a row under the picture. */
static void viewer_actions(AppState *st, const GalFrame *f, const GalPhoto *p) {
    const char    *hot = "hover:bg-surface-alt";
    const float    d = f->coarse ? 44.0f : 32.0f;
    const bool     deleted = st->scope == GAL_SCOPE_DELETED;
    /* The bar still draws while the viewer flies home, but nothing on it starts then: an edit
       or an Info begun would outlive the viewer, and a slideshow would stall its flight. */
    const bool     live = !st->viewer.closing;
    if (ui_icon("viewer_show", rcIconPlay, RC_WHITE, hot, d, 15.0f, true, "Slideshow") && live)
        player_open_slideshow(st);
    if (!deleted && ui_icon("viewer_fav", p->favorite ? rcIconHeartSolid : rcIconHeart,
                            p->favorite ? f->pal.heart : RC_WHITE, hot, d, 17.0f, true,
                            p->favorite ? "Unfavorite" : "Favorite") && live)
        photo_set_favorite(st, st->selected, !p->favorite);
    if (ui_icon("viewer_info", rcIconInfo, st->modalInfo ? f->style.primary : RC_WHITE, hot, d,
                17.0f, true, "Info") && live)
        st->modalInfo = !st->modalInfo;
    if (deleted) {
        if (rcButton("viewer_recover", "Recover", .className = "tap") && live)
            viewer_delete(st, f);
        return;
    }
    if (ui_icon("viewer_turn", rcIconRotateCw, RC_WHITE, hot, d, 16.0f, true, "Rotate") && live)
        viewer_rotate(st);
    if (ui_icon("viewer_del", rcIconTrash2, RC_WHITE, hot, d, 16.0f, true, "Delete") && live)
        viewer_delete(st, f);
    if (rcButton("viewer_edit", "Edit", .className = "tap") && live)
        viewer_begin_edit(st);
}

/* The top bar, the window's titlebar while the viewer is up, and fixed chrome as the wall's
   is: see wall_band. */
static void viewer_top(AppState *st, const GalFrame *f, const GalPhoto *p, float fade) {
    const bool  wide = viewer_wide(f);
    const float inset = f->safe.top * f->zoom;
    rcUnzoomed()
    rcRow(.id = RC_ID_WINDOW_DRAG, .gap = 8, .pt = (float)inset, .pl = 8, .align = "cl",
          .w = "grow", .hType = RC_PX((float)GALLERY_BAND_H + inset),
          .overlay = rcWithAlpha(RC_BLACK, 1.0f - fade)) {
        if (st->viewer.editing) {
            rcRow(.gap = 8, .pl = 8, .align = "cl", .noDrag = true) {
                if (rcButton("edit_cancel", "Cancel", .className = "tap",
                             .variant = RC_BTN_GHOST)) {
                    GalFull *slot = full_slot_of(st->selected);
                    st->viewer.editing = false;
                    if (slot && slot->ready)
                        full_show(slot, &p->edit);
                }
            }
            rcSpacer() {}
            rcRow(.gap = 8, .pr = 12, .align = "cc", .noDrag = true) {
                if (rcButton("edit_done", "Done", .className = "tap text-black",
                             .variant = RC_BTN_PRIMARY)) {
                    viewer_commit_edit(st, st->selected, &st->viewer.work);
                    st->viewer.editing = false;
                }
            }
        } else {
            rcRow(.id = "viewer_back", .focusable = true, .gap = 2, .pr = 10, .align = "cl",
                  .borderRadius = "all-md", .noDrag = true, .hType = RC_PX(f->coarse ? 44 : 36),
                  .className = "hover:bg-surface-alt") {
                rcIconChevronLeft(20.0f, RC_WHITE);
                ui_label(gallery_scope_title(st), 14, RC_WHITE);
            }
            if (rcClicked("viewer_back"))
                viewer_close(st, f);
            if (wide) {
                rcColumn(.gap = 1, .pl = 8) {
                    ui_strong(p->title, 14, RC_WHITE);
                    ui_label(p->when, 12, f->pal.quiet);
                }
            }
            rcSpacer() {}
            if (wide) {
                rcRow(.gap = 4, .pr = 12, .align = "cc", .noDrag = true) {
                    viewer_actions(st, f, p);
                }
            }
        }
        rcWindowControls();
    }
}

static void viewer_arrows(AppState *st, GalRect stage, int index, const GalFrame *f) {
    const char    *look = "bg-black/43 hover:bg-black/75";
    const float    d = f->coarse ? 44.0f : 40.0f;
    const float    y = stage.y + stage.h * 0.5f - d * 0.5f;
    if (index > 0) {
        rcBox(.floating = { .to = RC_ATTACH_ROOT, .parent = RC_ANCHOR_TOP_LEFT,
                            .element = RC_ANCHOR_TOP_LEFT,
                            .offset = { stage.x + 16.0f, y }, .zIndex = 6 }) {
            if (ui_icon("viewer_prev", rcIconChevronLeft, RC_WHITE, look, d, 22.0f, true,
                        "Previous"))
                viewer_step(st, -1);
        }
    }
    if (index + 1 < st->store.visibleCount) {
        rcBox(.floating = { .to = RC_ATTACH_ROOT, .parent = RC_ANCHOR_TOP_LEFT,
                            .element = RC_ANCHOR_TOP_LEFT,
                            .offset = { stage.x + stage.w - 16.0f - d, y }, .zIndex = 6 }) {
            if (ui_icon("viewer_next", rcIconChevronRight, RC_WHITE, look, d, 22.0f, true,
                        "Next"))
                viewer_step(st, 1);
        }
    }
}

static bool viewer_far(float a, float b) { return a - b > 0.5f || b - a > 0.5f; }

static void viewer_layout(AppState *st, const GalFrame *wallFrame) {
    GalViewer *v = &st->viewer;
    if (!v->open || st->player.open)
        return;
    GalFrame frame = *wallFrame;
    frame.style = viewer_style(v->editing);
    const GalFrame *f = &frame;
    /* It stays set past this function: a field's text is drawn at render time, in the frame's
       last style, and the caption field sits on this dark panel. */
    rcSetStyle(frame.style);

    const GalPhoto *p = &st->store.photo[st->selected];
    const int       index = viewer_index(st);
    const float     k = ui_approach(f->dt, 15.0f);

    viewer_keys(st, f);
    if (v->workDirty) {
        GalFull *slot = full_slot_of(st->selected);
        if (slot && slot->ready)
            full_show(slot, &v->work);
        v->workDirty = false;
    }

    /* Ease everything toward where this frame's layout wants it. */
    GalRect target = viewer_fit(viewer_stage(st, f), viewer_aspect(st), viewer_wide(f) ? 12.0f
                                : 0.0f);
    if (v->closing && st->wall.state[st->selected] == GAL_WALL_SHOWN) {
        target = st->wall.photo[st->selected];
        target.x += f->wallX;
        target.y += f->wallY - st->wallScroll;
    }
    v->rect.x = ui_lerp(v->rect.x, target.x, k);
    v->rect.y = ui_lerp(v->rect.y, target.y, k);
    v->rect.w = ui_lerp(v->rect.w, target.w, k);
    v->rect.h = ui_lerp(v->rect.h, target.h, k);
    v->fade = ui_lerp(v->fade, v->closing ? 0.0f : 1.0f, k);
    if (!v->closing && v->fade > 0.995f)
        v->fade = 1.0f;
    if (index >= 0)
        v->filmPos = ui_lerp(v->filmPos, (float)index, ui_approach(f->dt, 18.0f));
    /* These eases have no input behind them, so they ask for frames until they land, and then
       land exactly. */
    v->easing = viewer_far(v->rect.x, target.x) || viewer_far(v->rect.y, target.y)
                || viewer_far(v->rect.w, target.w) || viewer_far(v->rect.h, target.h)
                || (index >= 0 && viewer_far(v->filmPos * VIEWER_THUMB,
                                             (float)index * VIEWER_THUMB));
    if (!v->easing) {
        v->rect = target;
        if (index >= 0)
            v->filmPos = (float)index;
    }
    if (v->closing && v->fade < 0.02f) {
        v->open = false;
        v->easing = false;
        /* A photo unfavorited inside Favorites leaves the list now the viewer is gone. */
        if (st->store.keep >= 0) {
            st->store.keep = -1;
            gallery_refilter(st);
        }
        return;
    }

    /* The lightbox lies over the wall while it fades in and out. Its tiles are floats at z 0,
       so z 3 covers the floats inside them, a check mark and its ring. A float counts its z
       from the float it is declared in, so the photo and the arrows sit 3 above this one, over
       its filmstrip. */
    rcColumn(.id = "Viewer", .bg = rcWithAlpha(RC_BLACK, v->fade), .pb = f->safe.bottom,
             .pl = f->safe.left, .pr = f->safe.right,
             .wType = RC_PX(f->viewW), .hType = RC_PX(f->viewH),
             .floating = { .to = RC_ATTACH_ROOT, .parent = RC_ANCHOR_TOP_LEFT,
                           .element = RC_ANCHOR_TOP_LEFT, .zIndex = 3 }) {
        viewer_top(st, f, p, v->fade);
        rcRow(.w = "grow", .h = "grow") {
            rcBox(.w = "grow", .h = "grow") {}
            if (viewer_wide(f) && v->editing) {
                rcColumn(.id = "EditPanel", .bg = f->pal.panel, .gap = 14, .p = 16,
                         .scroll = "v", .h = "grow",
                         .wType = RC_PX(VIEWER_PANEL),
                         .overlay = rcWithAlpha(RC_BLACK, 1.0f - v->fade)) {
                    const GalFull *slot = full_slot_of(st->selected);
                    edit_panel(st, f, slot && slot->ready);
                }
            } else if (viewer_wide(f) && st->modalInfo) {
                rcColumn(.id = "InfoPanel", .bg = f->pal.panel, .gap = 12, .p = 16,
                         .scroll = "v", .h = "grow",
                         .wType = RC_PX(VIEWER_PANEL),
                         .overlay = rcWithAlpha(RC_BLACK, 1.0f - v->fade)) {
                    viewer_info_body(st, f, p);
                }
            }
        }
        if (!viewer_wide(f) && v->editing) {
            rcColumn(.id = "EditPanel", .bg = f->pal.panel, .gap = 10, .p = 12, .scroll = "v",
                     .w = "grow",
                     .hType = RC_PX(viewer_sheet_h(f))) {
                const GalFull *slot = full_slot_of(st->selected);
                edit_panel(st, f, slot && slot->ready);
            }
        }
        if (viewer_has_film(st, f))
            viewer_film(st, f, index, v->fade);
        if (!viewer_wide(f) && !v->editing) {
            rcRow(.gap = 2, .px = 8, .align = "cc", .w = "grow", .hType = RC_PX(VIEWER_ACTION)) {
                viewer_actions(st, f, p);
            }
        }
    }

    rcBox(.id = "ViewerPhoto", .wType = RC_PX(v->rect.w), .hType = RC_PX(v->rect.h),
          .floating = { .to = RC_ATTACH_ROOT, .parent = RC_ANCHOR_TOP_LEFT,
                        .element = RC_ANCHOR_TOP_LEFT,
                        .offset = { v->rect.x, v->rect.y }, .zIndex = 6 },
          .image = gallery_picture(st->selected), .imageFit = RC_IMAGE_COVER) {}
    if (!v->editing && !v->closing)
        viewer_arrows(st, viewer_stage(st, f), index, f);

    /* Where Info cannot dock it comes up as a modal; Esc and the scrim close it. */
    if (!viewer_wide(f) && st->modalInfo && rcBeginModal("modal_info", &st->modalInfo)) {
        rcColumn(.gap = 12, .w = "78vw") {
            viewer_info_body(st, f, p);
        }
        rcEndModal();
    }
    if (viewer_wide(f) && v->editing)
        rcScrollbar("EditPanel");
    else if (viewer_wide(f) && st->modalInfo)
        rcScrollbar("InfoPanel");
    else if (v->editing)
        rcScrollbar("EditPanel");
}

#endif /* UI_VIEWER_H */
