/*
    gallery_app.c - a photo library in the shape of Apple Photos

    The photographs are the interface. One wall runs edge to edge behind a toolbar of glass
    controls, under a floating sidebar tinted by the pictures themselves. This file is the
    shell: pictures, the wall and its zoom, the sidebar and the phone's tabs. The lightbox and
    its editor are in ui_viewer.h, and the Memories player in ui_player.h.

    What to copy from it:

      The zoom. Rows cannot animate between densities, because a row's contents change. So
      each photo is placed by its own rectangle (a .floating box inside the scroll pane, see
      wall_draw), and a zoom eases every rectangle from where it stood to where the new
      layout puts it. Only the photos in the window are declared.

      The pictures. rcImageUpdate takes pixels the app computed, and one RC_Image can change
      size between calls. Each tile opens with a 24 px rendering, blurred by being scaled up,
      and is replaced when its turn comes by one sized to the tile on this display, a band of
      rows a frame (gallery_pump), so no frame stalls and a zoom sharpens where it lands. The
      viewer does the same at 1280 px.

      The frames. Nothing here moves by itself under RC_RENDER_ON_DEMAND: gallery_wants_frame
      says when a frame is owed, and main.c asks for it.

    The library is a demo: favorites, captions, edits and deletions last until the app closes,
    and the next run starts from the same catalog.

    Pure RC_ API, no system includes. Target: rayclay_ex21_media_library
*/
#define GALLERY_BACKEND_IMPLEMENTATION
#include "gallery_app.h"

#include "icons/rc_icons_captions.h"
#include "icons/rc_icons_check.h"
#include "icons/rc_icons_chevron_left.h"
#include "icons/rc_icons_chevron_right.h"
#include "icons/rc_icons_circle_check.h"
#include "icons/rc_icons_clock.h"
#include "icons/rc_icons_folder.h"
#include "icons/rc_icons_gallery_vertical_end.h"
#include "icons/rc_icons_heart.h"
#include "icons/rc_icons_heart_solid.h"
#include "icons/rc_icons_images.h"
#include "icons/rc_icons_info.h"
#include "icons/rc_icons_list.h"
#include "icons/rc_icons_minus.h"
#include "icons/rc_icons_moon.h"
#include "icons/rc_icons_pause.h"
#include "icons/rc_icons_play.h"
#include "icons/rc_icons_plus.h"
#include "icons/rc_icons_rotate_cw.h"
#include "icons/rc_icons_search.h"
#include "icons/rc_icons_sun.h"
#include "icons/rc_icons_trash_2.h"
#include "icons/rc_icons_triangle_alert.h"
#include "icons/rc_icons_volume_2.h"
#include "icons/rc_icons_volume_x.h"
#include "icons/rc_icons_x.h"

/* The scripted scenario: warmup frames, then a strict hold, and the photo it settles on. */
#define GALLERY_BENCH_WARMUP 80
#define GALLERY_BENCH_PICK   4

#define GAL_SIDE_W     236    /* the sidebar's column, its 8 px inset included             */
#define GAL_TABBAR_H   54
#define GAL_DOCK_MIN_W 840    /* a sidebar and a toolbar on one line need this much        */
#define GAL_DOCK_MIN_H 500    /* and the sidebar's rows this much height: a short window,
                                 or a phone on its side, gets the tabs instead             */
#define GAL_MOTION_SEC 0.34f  /* a zoom's length                                           */
#define GAL_BAND_TITLE 420    /* the toolbar's width in its own px, less the window's controls,
                                 from which it keeps its title and date, cut short if need be */
#define GAL_BAND_FIELD 872    /* its width from which the search field stays open          */
#define GAL_BAND_ZOOM  1000   /* and from which a docked bar shows the zoom capsule        */
#define GAL_JOB_PIXELS (GAL_THUMB_EDGE * GAL_THUMB_EDGE)  /* a picture job's work a frame  */
#define GAL_TEXT_BYTES 4096   /* a frame's formatted text: a count, a clock, a transcript's
                                 times                                                     */

/* All Photos' target row heights, largest first, for a wall about 1040 px wide. */
static const float GAL_ROW_H[GAL_ZOOM_STEPS] = { 360.0f, 248.0f, 176.0f, 120.0f, 84.0f };

/* The phone's All Photos is a grid of squares, and these are its target cell widths at the
   same five steps: the columns are the count that comes nearest, so a phone held upright
   shows three at rest and one on its side six. */
static const float GAL_CELL_W[GAL_ZOOM_STEPS] = { 400.0f, 200.0f, 136.0f, 100.0f, 76.0f };

/* The colors RC_Style has no field for, which the app draws with. The looks its controls take
   are classes: see gallery_classes. */
typedef struct {
    RC_Color sideFill;         /* the sidebar's pale, under its photo's tint   */
    RC_Color sideEdge;         /* the sidebar's hairline                       */
    RC_Color sideMuted;        /* its quiet text, which sits on a tinted fill  */
    RC_Color onWhite;          /* the ink of a white control over a photograph */
    /* The viewer and the player are one dark room in either theme. */
    RC_Color panel;            /* a panel beside the picture                   */
    RC_Color quiet, faint;     /* a panel's second and third voice             */
    RC_Color heart;            /* a favorite                                   */
    RC_Color caution;          /* a film that cannot play                      */
} GalPalette;

/* This frame's geometry, measured once and read everywhere below. */
typedef struct {
    RC_Style   style;
    GalPalette pal;
    int        viewW, viewH;   /* the layout viewport                      */
    RC_Insets  safe;
    float      zoom;           /* window px per layout px                  */
    float      pixels;         /* device px per layout px                  */
    float      bandH;          /* the toolbar's height in layout px        */
    float      bandFoot;       /* its lower edge in the pane               */
    float      tap;            /* an icon control's size for this pointer,
                                  the height the class "tap" gives          */
    float      wallW, wallH;   /* the wall's box                           */
    float      wallX, wallY;   /* its top left in the window               */
    bool       docked;         /* a sidebar beside the wall, else tabs     */
    bool       coarse;
    bool       over;           /* the toolbar floats over photographs      */
    float      dt;
} GalFrame;

/* Pictures: handles that outlive gallery_seed's memset. */
typedef struct {
    int            photo;      /* -1 = empty                               */
    bool           ready;
    uint32_t       used;
    RC_Image       image;      /* what the viewer draws, edits applied     */
    unsigned char *pixels;     /* as shot: what the editor reworks         */
    int            w, h;
} GalFull;

static RC_Image       g_thumb[GAL_PHOTO_COUNT];
static RC_Image       g_ambient;
static GalFull        g_full[GAL_FULL_SLOTS];
static PgJob          g_job;       /* the viewer picture rendering over several frames */
static PgJob          g_tileJob;   /* a tile's, the same way                           */
static PgJob          g_smallJob;  /* a placeholder or the sidebar's, rendered at once */
static int            g_jobPhoto = -1, g_jobSlot = -1, g_tilePhoto = -1;
static RC_Arena       g_arena;
static RC_Arena       g_text;      /* this frame's formatted text: see gallery_layout  */
static unsigned char *g_edited;
static unsigned char *g_tile;
static unsigned char  g_small[GAL_TINY_EDGE * GAL_TINY_EDGE * 4];

static GalPalette gallery_palette(bool dark) {
    GalPalette p;
    p.sideFill   = dark ? rcHex(0x202022) : rcHex(0xf4f2f4);
    p.sideEdge   = dark ? rcWithAlpha(RC_WHITE, 0.1f) : rcWithAlpha(RC_WHITE, 0.784f);
    p.sideMuted  = dark ? rcHex(0x98989d) : rcHex(0x5c5c61);
    p.onWhite    = rcHex(0x1d1d1f);
    p.panel      = rcHex(0x161618);
    p.quiet      = rcHex(0xaeaeb2);
    p.faint      = rcHex(0x8e8e93);
    p.heart      = rcHex(0xff375f);
    p.caution    = rcHex(0xffd60a);
    return p;
}

/* A photo library is a neutral room: white or black walls, one blue for what is selected. */
RC_Style gallery_style(bool dark) {
    RC_Style s = dark ? rcStyleDark() : rcStyleLight();
    s.background   = dark ? rcHex(0x000000) : RC_WHITE;
    s.surface      = dark ? rcHex(0x1c1c1e) : RC_WHITE;
    s.surfaceAlt   = dark ? rcHex(0x2c2c2e) : rcHex(0xf2f2f7);
    s.chrome       = dark ? rcHex(0x1c1c1e) : rcHex(0xf5f5f7);
    s.text         = dark ? rcHex(0xf5f5f7) : rcHex(0x1d1d1f);
    s.textMuted    = dark ? rcHex(0x98989d) : rcHex(0x6e6e73);
    s.border       = dark ? rcHex(0x38383a) : rcHex(0xd2d2d7);
    s.primary      = dark ? rcHex(0x0a84ff) : rcHex(0x007aff);
    s.primaryHover = dark ? rcHex(0x409cff) : rcHex(0x0068d9);
    return s;
}

/* The looks that repeat, named once, as a stylesheet names them. A class that names a theme
   role (bg-surface-alt, hover:bg-foreground/16) follows rcSetStyle by itself; the few looks
   that differ by theme or by pointer are written again when either changes, as a page's
   media rules switch, so no string that uses one needs a second spelling. Every layout calls
   it, and it does nothing until the theme or the pointer changes. */
static void gallery_classes(bool dark, bool coarse) {
    static int defined = -1;
    const int now = (dark ? 1 : 0) | (coarse ? 2 : 0);
    if (now == defined)
        return;
    defined = now;
    /* A control over a photograph: dense dark glass reads on any picture, in either theme. */
    rcDefineClass("glass", "bg-[#1c1c1ec8]");
    rcDefineClass("glass-hover", "hover:bg-[#46464abe]");
    /* A control on the plain page, and the segment chosen there. */
    rcDefineClass("control", dark ? "bg-surface-alt border-surface-alt"
                                  : "bg-[#e9e9ee] border-[#e9e9ee]");
    rcDefineClass("control-hover", "hover:bg-foreground/16");
    rcDefineClass("segment-on", dark ? "bg-[#636366]" : "bg-white");
    /* The sidebar's selected and hovered rows, over its tinted glass. */
    rcDefineClass("row-on", dark ? "bg-white/11" : "bg-black/8");
    rcDefineClass("row-hover", dark ? "hover:bg-white/5" : "hover:bg-black/4");
    /* A labeled control's height: the icon buttons' size for this pointer (GalFrame.tap). */
    rcDefineClass("tap", coarse ? "h-11" : "h-8");
    /* A ghost button's label over dark glass or a scrim, dimmed as a dead icon beside it. */
    rcDefineClass("on-dark", "text-white disabled:text-white/50");
}

/* ---- small things every screen uses ---- */

static void ui_label(const char *text, int size, RC_Color color) {
    rcTextC(text, .color = color, .size = (uint16_t)size, .wrap = "n", .select = RC_SELECT_NONE);
}

static void ui_strong(const char *text, int size, RC_Color color) {
    rcTextC(text, .font = rcFontSemibold((float)size), .color = color, .wrap = "n",
            .select = RC_SELECT_NONE);
}

/* A round icon button, its fill and hover shade the classes in @p look. A live one is
   .focusable: a Tab stop that Enter and Space click, as they click an rcButton. A dead one is
   .disabled, as a <button disabled> is: it keeps its place, so a bar never reflows under the
   pointer, and it fades, takes no hover, no click and no focus. */
static bool ui_icon(const char *id, RC_IconCallback icon, RC_Color ink, const char *look,
                    float size, float glyph, bool live, const char *tip) {
    rcBox(.id = id, .focusable = true, .align = "cc", .borderRadius = "all-full",
          .disabled = !live, .wType = RC_PX(size), .hType = RC_PX(size), .tooltip = tip,
          .className = look) {
        icon(glyph, ink);
    }
    return rcClicked(id);
}

static float ui_lerp(float a, float b, float t) { return a + (b - a) * t; }

/* An ease that needs no clock of its own: move this share of the rest of the way each frame.
   It stays exact at any frame rate because it is a function of dt. */
static float ui_approach(float dt, float speed) {
    float x = dt * speed;
    return x >= 3.0f ? 1.0f : 1.0f - 1.0f / (1.0f + x + 0.5f * x * x);
}

/* ---- pictures ---- */

static void gallery_buffers(void) {
    if (g_edited)
        return;
    /* Square is the largest a photo gets: 1280 x 1280 for each slot, a tile being rendered and
       the editor's output. */
    const size_t one = (size_t)GAL_FULL_EDGE * GAL_FULL_EDGE * 4u;
    g_arena = rcArenaInit(one * (GAL_FULL_SLOTS + 2) + 64u);
    for (int s = 0; s < GAL_FULL_SLOTS; s++)
        g_full[s].pixels = (unsigned char *)rcArenaAlloc(&g_arena, one);
    g_edited = (unsigned char *)rcArenaAlloc(&g_arena, one);
    g_tile = (unsigned char *)rcArenaAlloc(&g_arena, one);
}

/* Writes a slot's picture through an edit: the viewer's image, at once. */
static void full_show(GalFull *f, const GalEdit *edit) {
    int w, h;
    gal_edit_apply(f->pixels, f->w, f->h, edit, g_edited, &w, &h);
    rcImageUpdate(&f->image, g_edited, w, h, 0, RC_PIXEL_RGBA8);
}

static GalFull *full_slot_of(int photo) {
    for (int s = 0; s < GAL_FULL_SLOTS; s++)
        if (g_full[s].photo == photo)
            return &g_full[s];
    return NULL;
}

/* The sharpest picture of a photo there is right now. */
static const RC_Image *gallery_picture(int photo) {
    const GalFull *f = full_slot_of(photo);
    return f && f->ready ? &f->image : &g_thumb[photo];
}

/* Starts rendering @p photo at viewer size, in the slot nobody asked for longest ago. */
static void full_start(AppState *st, int photo) {
    int best = 0;
    for (int s = 1; s < GAL_FULL_SLOTS; s++)
        if (g_full[s].used < g_full[best].used)
            best = s;
    GalFull *f = &g_full[best];
    PgShot   shot;
    gallery_buffers();
    f->photo = photo;
    f->ready = false;
    f->used = st->frame;
    gal_photo_size(&st->store.photo[photo], GAL_FULL_EDGE, &f->w, &f->h);
    gal_photo_shot(&st->store.photo[photo], &shot);
    pg_begin(&g_job, &shot, f->pixels, f->w, f->h);
    g_jobPhoto = photo;
    g_jobSlot = best;
}

/* A tile's picture can always be rendered again, so it belongs in rcAppCachePath: no phone
   backs that folder up, and the system may empty it, so a missing file only means rendering
   it again. What the user made (favorites, captions) would go under rcAppDataPath instead.
   The name carries the generator's version, so a new generator never shows an old picture. */
static bool thumb_path(char *path, int cap, int photo, int edge) {
    const char *dir = rcAppCachePath("RayClay", "Photos");
    int pos = 0;
    if (!dir)
        return false;
    gal_put(path, &pos, cap, dir);
    gal_put(path, &pos, cap, "thumb-" GAL_PICTURE_VERSION "-");
    gal_put_int(path, &pos, cap, photo);
    gal_put(path, &pos, cap, "-");
    gal_put_int(path, &pos, cap, edge);
    gal_put(path, &pos, cap, ".rgba");
    /* A path cut at the buffer's end names some other file, which a save would write over: the
       tile renders instead. */
    return pos < cap - 1;
}

/* A box @p w by @p h in device px, a photo covering it: the picture's long edge that takes is
   the next size up that shows no steps, doubling from the smallest so a library keeps few
   sizes. A tile whose shape differs from the photo's is cropped, and needs the longer edge. */
static void picture_want(AppState *st, int photo, float w, float h) {
    const float a = st->store.photo[photo].aspect;
    const float along = a >= 1.0f ? h * a : w / a, across = a >= 1.0f ? w : h;
    const float longEdge = along > across ? along : across;
    int edge = GAL_THUMB_EDGE;
    while (edge < GAL_FULL_EDGE && longEdge > (float)edge * 1.2f)
        edge *= 2;
    if (st->wanted[photo] != st->frame)
        st->wantEdge[photo] = 0;
    st->wanted[photo] = st->frame;
    if (st->wantEdge[photo] < edge)
        st->wantEdge[photo] = (uint16_t)edge;
}

/* True while a tile is on screen: its place asked for a picture this frame or the last. */
static bool picture_shown(const AppState *st, int photo) {
    return st->wanted[photo] != 0u && st->wanted[photo] + 1u >= st->frame;
}

/* True when a tile on screen is owed a sharper picture than it has. */
static bool picture_owed(const AppState *st, int photo) {
    return picture_shown(st, photo) && st->sharp[photo] < st->wantEdge[photo];
}

/* Off screen, sharp pictures stay while they fit in a screenful of grid tiles (32 at 640 px,
   in long edges squared), so the viewer's open and close and a scroll back find them sharp. */
#define GAL_KEEP_AREA (32LL * 640 * 640)

/* The tile whose picture is bigger than it should be, or -1, as a browser lets decoded images
   go. A tile on screen holds the size its place calls for, so a zoom out steps it down. Off
   screen, the pictures seen last stay sharp while they fit GAL_KEEP_AREA (or the screen's own,
   if larger) and the oldest beyond go back to the smallest. So the pictures stay within two
   screens however long a session zooms and scrolls; counted in tiles, never time, as the pump
   is. */
static int picture_spare(const AppState *st) {
    long long shown = 0, kept = 0;
    int       oldest = -1;
    for (int i = 0; i < GAL_PHOTO_COUNT; i++) {
        const long long edge = st->sharp[i];
        if (picture_shown(st, i)) {
            if (st->sharp[i] > st->wantEdge[i])
                return i;
            shown += (long long)st->wantEdge[i] * st->wantEdge[i];
        } else if (st->sharp[i] > GAL_THUMB_EDGE) {
            kept += edge * edge;
            if (oldest < 0 || st->wanted[i] < st->wanted[oldest])
                oldest = i;
        }
    }
    return kept > (shown > GAL_KEEP_AREA ? shown : GAL_KEEP_AREA) ? oldest : -1;
}

/* Hands a tile its picture. The cached file is the photo as shot, so an edit is applied here. */
static void tile_show(AppState *st, int photo, const unsigned char *pixels, int w, int h) {
    const GalEdit *edit = &st->store.photo[photo].edit;
    if (gal_edit_is_identity(edit)) {
        rcImageUpdate(&g_thumb[photo], pixels, w, h, 0, RC_PIXEL_RGBA8);
    } else {
        int ow, oh;
        gal_edit_apply(pixels, w, h, edit, g_edited, &ow, &oh);
        rcImageUpdate(&g_thumb[photo], g_edited, ow, oh, 0, RC_PIXEL_RGBA8);
    }
    st->sharp[photo] = (uint16_t)(w > h ? w : h);
}

/* Fetches a tile's picture from the cache at once, else starts rendering it. */
static void tile_start(AppState *st, int photo, int edge, bool useCache) {
    const GalPhoto *p = &st->store.photo[photo];
    char   path[640];
    void  *file = NULL;
    size_t length = 0;
    int    w, h;
    gal_photo_size(p, edge, &w, &h);
    gallery_buffers();
    if (useCache && thumb_path(path, sizeof path, photo, edge)
        && rcLoadFile(path, &file, &length) == RC_FILE_OK
        && length == (size_t)w * (size_t)h * 4u) {
        tile_show(st, photo, (const unsigned char *)file, w, h);
    } else {
        PgShot shot;
        gal_photo_shot(p, &shot);
        pg_begin(&g_tileJob, &shot, g_tile, w, h);
        g_tilePhoto = photo;
    }
    rcFreeFile(file);
}

/* The next stretch of the tile being rendered; once whole it is saved and shown. */
static void tile_step(AppState *st, bool useCache) {
    if (!pg_step(&g_tileJob, GAL_JOB_PIXELS / g_tileJob.w))
        return;
    const int w = g_tileJob.w, h = g_tileJob.h;
    char path[640];
    if (useCache && thumb_path(path, sizeof path, g_tilePhoto, w > h ? w : h))
        rcSaveFile(path, g_tile, (size_t)w * (size_t)h * 4u, 0);
    tile_show(st, g_tilePhoto, g_tile, w, h);
    g_tilePhoto = -1;
}

static bool viewer_animating(const AppState *st);

/* One picture job a frame, nearest the reader first: the open photo at viewer size, then any
   tile on screen smaller than its place calls for, then the open photo's neighbors, so an arrow
   key lands sharp. Everything counts frames, never time, which keeps the scripted run
   deterministic. */
static void gallery_pump(AppState *st, const int *fullWant, int fullCount, bool useCache) {
    st->working = false;
    if (g_jobPhoto >= 0) {
        st->working = true;
        if (pg_step(&g_job, GAL_JOB_PIXELS / g_job.w)) {
            GalFull *f = &g_full[g_jobSlot];
            f->ready = true;
            full_show(f, &st->store.photo[g_jobPhoto].edit);
            g_jobPhoto = -1;
        }
        return;
    }
    for (int k = 0; k < fullCount; k++) {
        GalFull *slot = full_slot_of(fullWant[k]);
        if (slot)
            slot->used = st->frame;
    }
    if (fullCount > 0 && !full_slot_of(fullWant[0])) {
        full_start(st, fullWant[0]);
        st->working = true;
        return;
    }
    /* Nothing renders while something flies: the motion is what the eye is on. */
    if (st->morph < 1.0f || viewer_animating(st))
        return;
    if (g_tilePhoto < 0) {
        /* A blurred tile comes before one that could only get sharper. */
        int next = -1;
        for (int i = 0; i < GAL_PHOTO_COUNT; i++)
            if (picture_owed(st, i) && (next < 0 || st->sharp[i] < st->sharp[next]))
                next = i;
        if (next < 0) {
            /* Only once nothing on screen waits: the eye's tiles first, then the memory. */
            const int spare = picture_spare(st);
            if (spare >= 0) {
                tile_start(st, spare, picture_shown(st, spare) ? st->wantEdge[spare]
                                                                : GAL_THUMB_EDGE, useCache);
                st->working = true;
            }
        } else {
            tile_start(st, next, st->wantEdge[next], useCache);
            st->working = true;
        }
    }
    if (g_tilePhoto >= 0) {
        st->working = true;
        tile_step(st, useCache);
    }
    if (st->working)
        return;
    for (int k = 1; k < fullCount; k++) {
        if (!full_slot_of(fullWant[k])) {
            full_start(st, fullWant[k]);
            st->working = true;
            return;
        }
    }
}

/* The sidebar's tint: a photo rendered at 12 x 18 and scaled up, which blurs it for free. It
   only changes when the scope does. */
static void gallery_ambient(AppState *st, int photo) {
    if (photo < 0 || st->ambientOf == photo)
        return;
    PgShot shot;
    gal_photo_shot(&st->store.photo[photo], &shot);
    pg_begin(&g_smallJob, &shot, g_small, 12, 18);
    pg_step(&g_smallJob, 18);
    rcImageUpdate(&g_ambient, g_small, 12, 18, 0, RC_PIXEL_RGBA8);
    st->ambientOf = photo;
}

/* ---- the library's scope ---- */

static void gallery_refilter(AppState *st) {
    gallery_filter(&st->store, st->scope, st->album, st->search);
    st->filterDirty = true;
    if (st->store.visibleCount > 0)
        gallery_ambient(st, st->store.run[0].cover);
}

static void gallery_stop_selecting(AppState *st) {
    st->selecting = false;
    st->pickedCount = 0;
    gal_zero(st->picked, sizeof st->picked);
}

/* A search belongs to the place it was made in: going anywhere else ends it. */
static void gallery_pick_scope(AppState *st, GalTab tab, int scope, int album) {
    st->wallToTop = true;
    st->tab = tab;
    st->scope = scope;
    st->album = album;
    gallery_stop_selecting(st);
    if (tab != GAL_TAB_SEARCH) {
        st->search[0] = '\0';
        st->searchOpen = false;
    }
    gallery_refilter(st);
}

/* Esc in a focused field only blurs it. The next one ends the search: the field folds away and
   the wall gets everything back. */
static void gallery_end_search(AppState *st) {
    st->searchOpen = false;
    if (st->search[0]) {
        st->search[0] = '\0';
        st->wallToTop = true;
        gallery_refilter(st);
    }
}

static const char *gallery_scope_title(const AppState *st) {
    if (st->tab == GAL_TAB_MEMORIES) return "Memories";
    if (st->tab == GAL_TAB_ALBUMS)   return "Albums";
    if (st->search[0])               return "Search";
    switch (st->scope) {
    case GAL_SCOPE_FAVORITES: return "Favorites";
    case GAL_SCOPE_RECENTS:   return "Recents";
    case GAL_SCOPE_DELETED:   return "Recently Deleted";
    case GAL_SCOPE_ALBUM:     return GAL_ALBUM_NAMES[st->album];
    default:                  return "Library";
    }
}

/* Keeps the caption buffer on the open photo: save before load, since one buffer serves all
   ninety-six and whatever was typed must go back to the photo being left. */
static void gallery_sync_caption(AppState *st) {
    if (st->captionOf == st->selected)
        return;
    if (st->captionOf >= 0)
        gal_copy(st->store.photo[st->captionOf].caption, st->caption, GAL_CAPTION_CAP);
    gal_copy(st->caption, st->store.photo[st->selected].caption, GAL_CAPTION_CAP);
    st->captionOf = st->selected;
}

/* Unfavoriting inside Favorites must not pull the photo out from under the viewer: it stays
   in the list until the viewer closes. */
static void photo_set_favorite(AppState *st, int photo, bool on) {
    st->store.photo[photo].favorite = on;
    if (st->viewer.open)
        st->store.keep = photo;
    gallery_refilter(st);
}

/* A deletion only moves the photo to Recently Deleted; recovering moves it back. */
static void photo_set_deleted(AppState *st, int photo, bool on) {
    st->store.photo[photo].deleted = on;
    gallery_refilter(st);
}

#include "ui_viewer.h"
#include "ui_player.h"

/* ---- the wall ---- */

static float wall_ease(float t) {
    float u = 1.0f - pg_clamp(t);
    return 1.0f - u * u * u;
}

static GalRect rect_shift(GalRect r, float dy) {
    r.y -= dy;
    return r;
}

/* Where photo @p i is in the wall's own box right now, and how opaque, mid-zoom or at rest. */
static bool wall_place(const AppState *st, int i, float scroll, GalRect *r, float *alpha) {
    const int to = st->wall.state[i];
    if (st->morph >= 1.0f) {
        if (to != GAL_WALL_SHOWN)
            return false;
        *r = rect_shift(st->wall.photo[i], scroll);
        *alpha = 1.0f;
        return true;
    }
    const int from = st->fromState[i];
    if (from == GAL_WALL_ABSENT && to == GAL_WALL_ABSENT)
        return false;
    const float e = wall_ease(st->morph);
    GalRect end = st->from[i];
    if (to != GAL_WALL_ABSENT)
        end = rect_shift(st->wall.photo[i], st->scrollTarget);
    GalRect start = from != GAL_WALL_ABSENT ? st->from[i] : end;
    r->x = ui_lerp(start.x, end.x, e);
    r->y = ui_lerp(start.y, end.y, e);
    r->w = ui_lerp(start.w, end.w, e);
    r->h = ui_lerp(start.h, end.h, e);
    *alpha = ui_lerp(from == GAL_WALL_SHOWN ? 1.0f : 0.0f, to == GAL_WALL_SHOWN ? 1.0f : 0.0f, e);
    return *alpha > 0.01f;
}

/* The inputs a wall is built from, rebuilt when any of them changes. */
static uint32_t wall_key(const AppState *st) {
    return (uint32_t)st->view | ((uint32_t)st->zoomStep << 3);
}

/* All Photos' row height beside the sidebar, where the wall is a justified flow. A short scope
   scales every step up until its default one about fills the window, so six photos do not sit
   in a corner of it and the zoom keeps its order. */
static float wall_row_h(const AppState *st, const GalFrame *f) {
    float k = f->wallW / 1040.0f;
    k = k < 0.62f ? 0.62f : k > 1.3f ? 1.3f : k;
    const float base = GAL_ROW_H[GAL_ZOOM_DEFAULT] * k;
    const float room = f->wallH - f->bandFoot;
    const float fill = gal_fill_row_h(&st->store, f->wallW, room);
    const float h = GAL_ROW_H[st->zoomStep] * k * (fill > base ? fill / base : 1.0f);
    return h > f->wallH * 0.8f ? f->wallH * 0.8f : h;
}

/* The phone's columns at this zoom step: see GAL_CELL_W. */
static int wall_grid_cols(const AppState *st, const GalFrame *f) {
    const int cols = (int)(f->wallW / GAL_CELL_W[st->zoomStep] + 0.5f);
    return cols < 1 ? 1 : cols;
}

/* Rebuilds the wall when its inputs moved, and starts a zoom unless only the window changed or
   a new scope replaced the photos: a resize should follow the pointer, not animate behind it,
   and a new album opens at rest at its top, as Photos switches albums. Flown in from their
   places in the old scope, its photos would leave the wall empty for the first frames. */
static void wall_sync(AppState *st, const GalFrame *f) {
    const uint32_t key = wall_key(st);
    const bool     resized = st->wallW != (int)f->wallW || st->wallH != (int)f->wallH;
    if (key == st->wallKey && !resized && !st->filterDirty)
        return;
    const bool moves = st->wallW != 0 && !resized && !st->wallToTop;
    const float top = f->over ? 0.0f : f->bandFoot + 8.0f;
    const float bottom = f->docked ? 0.0f : 72.0f;
    float old = st->wallScroll;
    if (!moves)
        st->yearsFrom = -1;   /* a new scope or window size starts Years afresh */

    if (moves) {
        /* Years fits the whole library in the window where it can, so its middle photo says
           little about the reader's place: leaving it with no photo clicked goes back to the
           photo held on the way in while that photo is still on screen. A phone's Years
           scrolls, and one scrolled past that photo keeps the middle photo instead. wall_key
           keeps the view in its low bits. */
        if (st->anchor < 0 && st->yearsFrom >= 0
            && (st->wallKey & 7u) == (uint32_t)GAL_VIEW_YEARS) {
            GalRect r;
            float   a;
            if (wall_place(st, st->yearsFrom, old, &r, &a) && r.y + r.h * 0.5f >= f->bandFoot
                && r.y + r.h * 0.5f <= f->wallH)
                st->anchor = st->yearsFrom;
        }
        /* Hold the photo nearest the middle of the window in place, unless a click chose one. */
        if (st->anchor < 0) {
            float best = 1e9f;
            for (int k = 0; k < st->store.visibleCount; k++) {
                int     i = st->store.visible[k];
                GalRect r;
                float   a;
                if (!wall_place(st, i, old, &r, &a))
                    continue;
                float d = r.y + r.h * 0.5f - f->wallH * 0.5f;
                d = d < 0.0f ? -d : d;
                if (d < best) {
                    best = d;
                    st->anchor = i;
                }
            }
        }
        if (st->view == GAL_VIEW_YEARS)
            st->yearsFrom = st->anchor;
        for (int i = 0; i < GAL_PHOTO_COUNT; i++) {
            GalRect r;
            float   a;
            if (st->morph >= 1.0f && st->wall.state[i] == GAL_WALL_FOLDED) {
                st->from[i] = rect_shift(st->wall.photo[i], old);
                st->fromState[i] = GAL_WALL_FOLDED;
            } else if (wall_place(st, i, old, &r, &a)) {
                st->from[i] = r;
                st->fromState[i] = GAL_WALL_SHOWN;
            } else {
                st->fromState[i] = GAL_WALL_ABSENT;
            }
        }
        st->morph = 0.0f;
    } else if (st->wallToTop) {
        st->morph = 1.0f;   /* a zoom under way would fly the old scope's photos in */
    }

    if (st->view == GAL_VIEW_YEARS) {
        gal_wall_years(&st->wall, &st->store, f->wallW, f->wallH, top, 56.0f, bottom);
    } else if (st->view == GAL_VIEW_MONTHS) {
        gal_wall_months(&st->wall, &st->store, f->wallW, top, bottom);
    } else if (f->docked) {
        gal_wall_flow(&st->wall, &st->store, f->wallW, wall_row_h(st, f), top, bottom);
    } else {
        gal_wall_grid(&st->wall, &st->store, f->wallW, wall_grid_cols(st, f), top, bottom);
    }

    float room = st->wall.height - f->wallH;
    room = room < 0.0f ? 0.0f : room;
    float target = old;
    if (st->wallToTop) {
        target = 0.0f;
    } else if (moves && st->anchor >= 0 && st->wall.state[st->anchor] != GAL_WALL_ABSENT
               && st->fromState[st->anchor] != GAL_WALL_ABSENT) {
        const GalRect *n = &st->wall.photo[st->anchor];
        target = n->y + n->h * 0.5f - (st->from[st->anchor].y + st->from[st->anchor].h * 0.5f);
    } else if (moves) {
        target = 0.0f;
    }
    st->scrollTarget = target < 0.0f ? 0.0f : target > room ? room : target;
    st->scrollPending = moves || st->wallToTop;
    st->anchor = -1;
    st->wallToTop = false;
    st->wallKey = key;
    st->wallW = (int)f->wallW;
    st->wallH = (int)f->wallH;
    st->filterDirty = false;
}

/* One level closer, around one photo: what a click on a year's mosaic or a month's card does. */
static void wall_zoom_to(AppState *st, GalView view, int anchor) {
    st->view = view;
    st->anchor = anchor;
}

/* A tile is clicked or its check is. In Years and Months a click zooms in around the photo;
   in All Photos it opens the photo, unless a selection is under way. */
static void wall_activate(AppState *st, int i, GalRect view, const GalFrame *f) {
    if (st->selecting && st->view == GAL_VIEW_ALL) {
        st->picked[i] = !st->picked[i];
        st->pickedCount += st->picked[i] ? 1 : -1;
    } else if (st->view == GAL_VIEW_YEARS) {
        wall_zoom_to(st, GAL_VIEW_MONTHS, i);
    } else if (st->view == GAL_VIEW_MONTHS) {
        wall_zoom_to(st, GAL_VIEW_ALL, i);
    } else {
        GalRect from = view;
        from.x += f->wallX;
        from.y += f->wallY;
        viewer_open(st, i, from);
    }
}

/* One tile: the picture, and only while it matters a check, a heart, a month's title. */
static void wall_tile(AppState *st, int i, GalRect r, float alpha, const GalFrame *f) {
    const RC_Style *s = &f->style;
    const char *id = st->tileId[i];
    const bool  on = st->picked[i] != 0;
    const bool  card = st->view == GAL_VIEW_MONTHS && st->morph >= 1.0f;
    const RC_Color tint = rcWithAlpha(RC_WHITE, alpha);
    bool checked = false;

    picture_want(st, i, r.w * f->pixels, r.h * f->pixels);
    rcBox(.id = id, .focusable = true, .bg = s->surfaceAlt, .p = 8, .align = "bl",
          .wType = RC_PX(r.w), .hType = RC_PX(r.h),
          .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                        .element = RC_ANCHOR_TOP_LEFT, .offset = { r.x, r.y } },
          .image = &g_thumb[i], .imageFit = RC_IMAGE_COVER,
          .imageTint = alpha < 1.0f ? tint : RC_UNSET_COLOR,
          .className = on ? "border-3 border-primary"
                          : st->view == GAL_VIEW_MONTHS ? "rounded-xl" : NULL) {
        if (card) {
            for (int k = 0; k < st->store.runCount; k++) {
                const GalRun *run = &st->store.run[k];
                if (run->cover != i)
                    continue;
                /* The scrim runs well past the text, so a bright sky behind it stays dark
                   enough to read at the second line too. */
                rcColumn(.id = st->cardId[i], .gap = 2, .p = 16, .wType = RC_PX(r.w),
                         .hType = RC_PX(r.h * 0.75f < 150.0f ? r.h * 0.75f : 150.0f),
                         .gradient = { .from = rcWithAlpha(RC_BLACK, 0.882f),
                                       .to = rcWithAlpha(RC_BLACK, 0), .dir = "v" },
                         .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                                       .element = RC_ANCHOR_TOP_LEFT,
                                       .capture = RC_CAPTURE_PASSTHROUGH },
                         .className = "rounded-t-xl") {
                    ui_strong(run->label, 20, RC_WHITE);
                    ui_label(run->place, 13, RC_WHITE);
                }
            }
        }
        /* The heart lies in the tile's own flow, its padded bottom-left corner: a float here
           would count its z from the tile and draw over a month's cover folding onto it. */
        if (st->store.photo[i].favorite && r.w >= 60.0f && !card)
            rcIconHeartSolid(14.0f, RC_WHITE);
        const char *check = st->checkId[i];
        if (st->view == GAL_VIEW_ALL && r.w >= 72.0f
            && (rcShowsHover(id) || rcShowsHover(check) || st->selecting || on)) {
            /* The mark is 22 px on any pointer. The box around it is what takes the press. */
            const float hit = f->coarse ? 44.0f : 30.0f, inset = 19.0f - hit * 0.5f;
            rcBox(.id = check, .align = "cc", .wType = RC_PX(hit), .hType = RC_PX(hit),
                  .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                                .element = RC_ANCHOR_TOP_LEFT,
                                .offset = { inset, r.y < f->bandFoot ? f->bandFoot + inset - r.y
                                                                     : inset } },
                  .tooltip = on ? "Deselect" : "Select") {
                rcBox(.bg = on ? s->primary : rcWithAlpha(RC_BLACK, 0.275f), .align = "cc",
                      .borderRadius = "all-full", .border = { .color = RC_WHITE, .width = 2 },
                      .w = "22px", .h = "22px") {
                    if (on)
                        rcIconCheck(12.0f, RC_WHITE);
                }
            }
            if (rcClicked(check)) {
                st->selecting = true;
                st->picked[i] = !on;
                st->pickedCount += on ? -1 : 1;
                checked = true;
            }
        }
    }
    /* A held Enter repeats its click on the focused tile: a photo already flying up stays. */
    if (!checked && rcClicked(id) && !st->viewer.open)
        wall_activate(st, i, r, f);
}

/* A year's heading in the overview: display type, the places in gray, as Apple sets them. */
static void wall_year_heads(const AppState *st, float scroll, const GalFrame *f) {
    const RC_Style *s = &f->style;
    if (st->view != GAL_VIEW_YEARS || st->morph < 1.0f)
        return;
    for (int k = 0; k < st->wall.headCount; k++) {
        const GalYear *y = &st->store.year[k];
        GalRect r = rect_shift(st->wall.head[k], scroll);
        if (r.y + r.h < 0.0f || r.y > f->wallH)
            continue;
        rcRow(.gap = 12, .pb = 10, .align = "bl", .wType = RC_PX(r.w), .hType = RC_PX(r.h),
              .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                            .element = RC_ANCHOR_TOP_LEFT, .offset = { r.x, r.y },
                            .capture = RC_CAPTURE_PASSTHROUGH }) {
            ui_strong(y->label, 22, s->text);
            ui_label(y->places, 14, s->textMuted);
            rcSpacer() {}
            ui_label(y->count, 13, s->textMuted);
        }
    }
}

/* Where trip k's pill would sit, and the room it has up to the next trip's pill on the same
   row, else to the wall's edge; a negative room where it is hidden. */
static float pill_room(const AppState *st, float scroll, const GalFrame *f, float foot, int k,
                       GalRect *at) {
    const int first = st->store.visible[st->store.run[k].first];
    const GalRect r = rect_shift(st->wall.photo[first], scroll);
    if (r.y < f->bandFoot + 4.0f || r.y + 32.0f > foot)
        return -1.0f;
    /* A check mark takes this corner while the pointer is on the tile. */
    if (rcShowsHover(st->tileId[first]) || rcShowsHover(st->checkId[first]))
        return -1.0f;
    float right = f->wallW - 8.0f;
    if (k + 1 < st->store.runCount) {
        const GalRect *n = &st->wall.photo[st->store.visible[st->store.run[k + 1].first]];
        if (n->y == st->wall.photo[first].y)
            right = n->x;
    }
    *at = r;
    return right - r.x - 16.0f;
}

/* A trip's first photo carries its place and dates, on a glass pill over the grid: there is
   no gap between runs to set a heading in, and that is the point of an edge-to-edge wall. */
static void wall_run_pills(const AppState *st, float scroll, const GalFrame *f) {
    if (st->view != GAL_VIEW_ALL || st->morph < 1.0f || st->zoomStep > 3 || st->selecting)
        return;
    const uint16_t bold = rcFontSemibold(12.0f);
    /* A phone's view switcher floats over the foot of the wall, and a pill keeps 48 px clear
       of it, so the two never read as one crowded cluster. */
    const float foot = f->wallH - (f->docked ? 0.0f : f->coarse ? 106.0f : 94.0f);
    /* One view, one format: every pill shows its dates, or none does, so no trip reads as
       undated beside one that is. A place that cannot fit shows nothing. Each trip is
       measured once, and a hidden one (a negative room) not at all: it can neither show
       nor decide the format. */
    struct { const GalRun *run; GalRect at; } shown[GAL_SHOOT_COUNT];
    int count = 0;
    bool dated = true;
    for (int k = 0; k < st->store.runCount; k++) {
        GalRect r;
        const float room = pill_room(st, scroll, f, foot, k, &r);
        if (room < 0.0f)
            continue;
        const GalRun *run = &st->store.run[k];
        const float place = rcTextWidth(bold, 12, run->place, -1) + 20.0f;
        if (place > room)
            continue;
        if (dated && place + 6.0f + rcTextWidth(0, 12, run->range, -1) > room)
            dated = false;
        shown[count].run = run;
        shown[count].at = r;
        count++;
    }
    for (int i = 0; i < count; i++) {
        const GalRect r = shown[i].at;
        rcRow(.gap = 6, .px = 10, .py = 4, .align = "cl", .borderRadius = "all-full",
              .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                            .element = RC_ANCHOR_TOP_LEFT, .offset = { r.x + 8.0f, r.y + 8.0f },
                            .capture = RC_CAPTURE_PASSTHROUGH },
              .className = "glass") {
            ui_strong(shown[i].run->place, 12, RC_WHITE);
            if (dated)
                ui_label(shown[i].run->range, 12, rcWithAlpha(RC_WHITE, 0.804f));
        }
    }
}

static void wall_empty(const AppState *st, const GalFrame *f) {
    const RC_Style *s = &f->style;
    const char *head = "No Photos", *sub = "Photos you keep here will appear in this place.";
    if (st->search[0]) {
        head = "No Results";
        sub = "Try a place, a month or an album.";
    } else if (st->scope == GAL_SCOPE_FAVORITES) {
        head = "No Favorites";
        sub = "Press the heart on a photo to keep it here.";
    } else if (st->scope == GAL_SCOPE_DELETED) {
        head = "Nothing Deleted";
        sub = "Deleted photos wait here until you recover them.";
    }
    rcColumn(.gap = 8, .align = "cc", .w = "grow", .h = "grow") {
        rcIconImages(44.0f, s->textMuted);
        ui_strong(head, 20, s->text);
        ui_label(sub, 14, s->textMuted);
    }
}

/* The wall. Every tile is a float inside the scroll pane, placed in the pane's own box: the
   spacer gives the pane its scroll range, and the tiles read the offset themselves, so at
   rest a tile sits at its place minus the scroll and mid-zoom at the eased rectangle, with
   one path through the code. */
static void wall_draw(AppState *st, const GalFrame *f) {
    wall_sync(st, f);
    RC_ScrollInfo info = rcGetScrollInfo("Wall");
    float scroll = info.found ? info.offsetY : st->wallScroll;

    /* A new height's range is known a frame after it is laid out, so the scroll follows then. */
    if (st->scrollPending) {
        float room = st->wall.height - f->wallH;
        room = room < 0.0f ? 0.0f : room;
        if (!info.found) {
            st->wallScroll = st->scrollTarget;
            scroll = st->scrollTarget;
            st->scrollPending = false;
        } else if ((info.maxOffsetY > room - 1.0f && info.maxOffsetY < room + 1.0f)
                   || st->morph >= 1.0f) {
            rcScrollBy("Wall", 0.0f, st->scrollTarget - scroll);
            scroll = st->scrollTarget;
            st->scrollPending = false;
        }
    }
    st->wallScroll = scroll;

    if (st->store.visibleCount == 0) {
        rcColumn(.id = "Wall", .pt = (float)f->bandFoot, .w = "grow", .h = "grow") {
            wall_empty(st, f);
        }
        return;
    }

    rcColumn(.id = "Wall", .scroll = "v", .w = "grow", .h = "grow",
             .scrollOffset = { 0.0f, st->wallScroll }) {
        rcBox(.w = "grow", .hType = RC_PX(st->wall.height)) {}
        /* A folded photo draws first so its month's cover lies on top of it. */
        for (int pass = 0; pass < 2; pass++) {
            for (int i = 0; i < GAL_PHOTO_COUNT; i++) {
                const bool folded = st->wall.state[i] == GAL_WALL_FOLDED
                                    || (st->morph < 1.0f && st->fromState[i] == GAL_WALL_FOLDED);
                GalRect r;
                float   a;
                if (folded != (pass == 0) || !wall_place(st, i, scroll, &r, &a))
                    continue;
                if (r.y + r.h < 0.0f || r.y > f->wallH)
                    continue;
                wall_tile(st, i, r, a, f);
            }
        }
        wall_year_heads(st, scroll, f);
        wall_run_pills(st, scroll, f);
    }
}

/* ---- the toolbar ---- */

/* The controls read differently over a photograph and over the plain page: their ink, and the
   classes of a capsule's fill, a control's hover shade and a button that takes both. */
typedef struct { RC_Color ink, muted; const char *fill, *hover, *button; } GalBandInk;

static GalBandInk band_ink(const GalFrame *f) {
    GalBandInk b;
    if (f->over) {
        b.ink = RC_WHITE;
        b.muted = rcWithAlpha(RC_WHITE, 0.843f);
        b.fill = "glass";
        b.hover = "glass-hover";
        b.button = "glass glass-hover";
    } else {
        b.ink = f->style.text;
        b.muted = f->style.textMuted;
        b.fill = "control";
        b.hover = "control-hover";
        b.button = "control control-hover";
    }
    return b;
}

/* Apple's zoom capsule. Step 0 is the largest tile, so plus walks down the table. It only
   means something in All Photos; elsewhere it moves between the three views. */
static void band_zoom(AppState *st, const GalFrame *f, const GalBandInk *b) {
    const float d = f->coarse ? 44.0f : 30.0f;
    const bool  all = st->view == GAL_VIEW_ALL;
    const bool  canOut = st->view != GAL_VIEW_YEARS;
    const bool  canIn = !all || st->zoomStep > 0;
    rcRow(.p = 1, .align = "cc", .borderRadius = "all-full", .className = b->fill) {
        if (ui_icon("zoom_out", rcIconMinus, b->ink, b->hover, d, 14.0f, canOut,
                    "Smaller thumbnails")) {
            if (!all)
                st->view = st->view > GAL_VIEW_YEARS ? (GalView)(st->view - 1) : st->view;
            else if (st->zoomStep < GAL_ZOOM_STEPS - 1)
                st->zoomStep++;
            else
                st->view = GAL_VIEW_MONTHS;
        }
        if (ui_icon("zoom_in", rcIconPlus, b->ink, b->hover, d, 14.0f, canIn,
                    "Larger thumbnails")) {
            if (!all)
                st->view = (GalView)(st->view + 1);
            else
                st->zoomStep--;
        }
    }
}

/* Years | Months | All Photos: three densities of the same wall, one click apart. */
static void band_segments(AppState *st, const GalFrame *f, const GalBandInk *b) {
    static const char *const LABEL[GAL_VIEW_COUNT] = { "Years", "Months", "All Photos" };
    static const char *const ID[GAL_VIEW_COUNT] = { "view_years", "view_months", "view_all" };
    static const int WIDE[GAL_VIEW_COUNT] = { 60, 70, 86 };
    const int h = f->coarse ? 40 : 28;
    rcRow(.gap = 2, .p = 2, .align = "cc", .borderRadius = "all-full", .className = b->fill) {
        for (int v = 0; v < GAL_VIEW_COUNT; v++) {
            const bool on = st->view == (GalView)v;
            RC_Color ink = on && f->over ? f->pal.onWhite : b->ink;
            rcBox(.id = ID[v], .focusable = true, .align = "cc", .borderRadius = "all-full",
                  .wType = RC_PX(WIDE[v]), .hType = RC_PX(h),
                  .className = !on ? b->hover : f->over ? "bg-white/92" : "segment-on") {
                ui_label(LABEL[v], 13, ink);
            }
            if (rcClicked(ID[v]) && !on)
                st->view = (GalView)v;
        }
    }
}

/* The search box grows to @p maxW, or as far as the row lets it at 0. Its class takes the
   tap height of the controls beside it and their fill (over a photograph, the theme's
   surface-alt). Its clear x empties the query and returns true, so the wall refilters
   and goes back to the top as for any edit. */
static void band_search(AppState *st, float maxW, const GalFrame *f) {
    const char *look = f->over ? "rounded-full tap bg-surface-alt border-surface-alt"
                               : "rounded-full tap control";
    rcBox(.pr = (float)(f->docked ? 12 : 0), .noDrag = true, .w = "grow",
          .wMax = maxW > 0.0f ? RC_VAL(maxW) : RC_UNSET_VAL) {
        if (rcTextInput("search", st->search, sizeof st->search, .className = look,
                        .placeholder = "Search", .leadingIcon = rcIconSearch,
                        .clearButton = true)) {
            st->wallToTop = true;
            gallery_refilter(st);
        }
    }
}

/* Where the reader is: the trip under the toolbar, in the title's second line. */
static const char *band_subtitle(const AppState *st, float scroll, const GalFrame *f) {
    if (st->store.visibleCount == 0)
        return st->store.countStr;
    if (st->view != GAL_VIEW_ALL || st->search[0])
        return st->search[0] ? st->store.countStr : st->store.range;
    for (int k = 0; k < st->store.visibleCount; k++) {
        int i = st->store.visible[k];
        const GalRect *r = &st->wall.photo[i];
        if (r->y + r->h - scroll > f->bandH * 0.5f) {
            for (int q = 0; q < st->store.runCount; q++)
                if (st->store.photo[st->store.visible[st->store.run[q].first]].shoot
                    == st->store.photo[i].shoot)
                    return st->store.run[q].range;
        }
    }
    return st->store.range;
}

/* The toolbar, which is also the titlebar: RC_ID_WINDOW_DRAG makes its bare parts move the
   window, and every control sits in a .noDrag subtree, because a control outside one would
   have its click eaten by the window move. It is chrome, not content: the strip the system
   drags by and the window's own buttons keep their size when the content zooms, so the bar is
   declared inside rcUnzoomed() and keeps GALLERY_BAND_H with them. What that scope draws is in
   window px, so the two lengths it takes from the zoomed layout, the wall's width and the
   status bar's height, are multiplied by the zoom first. Only what shows is decided here; the
   row itself sizes the title and the field, so nothing can be pushed off the window's edge. */
static void wall_band(AppState *st, const GalFrame *f) {
    const GalBandInk b = band_ink(f);
    const bool wall = st->tab == GAL_TAB_LIBRARY || st->tab == GAL_TAB_SEARCH;
    const bool phone = !f->docked;
    const float room = f->wallW * f->zoom, inset = (f->bandFoot - f->bandH) * f->zoom;
    const bool roomy = !phone && room >= GAL_BAND_FIELD;
    const bool searching = phone && st->tab == GAL_TAB_SEARCH;
    const bool fieldOut = wall && !phone && (roomy || st->searchOpen);
    const bool squeezed = !phone && wall
                          && (room - rcWindowControlsWidth() < GAL_BAND_TITLE
                              || (st->searchOpen && !roomy));

    if (f->over) {
        rcBox(.id = "band_scrim", .w = "grow", .hType = RC_PX(f->bandH + f->bandFoot),
              .gradient = { .from = rcWithAlpha(RC_BLACK, 0.51f), .to = rcWithAlpha(RC_BLACK, 0),
                            .dir = "v" },
              .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                            .element = RC_ANCHOR_TOP_LEFT, .zIndex = 2,
                            .capture = RC_CAPTURE_PASSTHROUGH }) {}
    }
    rcUnzoomed()
    rcRow(.id = RC_ID_WINDOW_DRAG, .gap = 8, .pt = (float)inset, .pl = 16, .align = "cl",
          .w = "grow", .hType = RC_PX((float)GALLERY_BAND_H + inset),
          .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                        .element = RC_ANCHOR_TOP_LEFT, .zIndex = 3 },
          .className = f->over ? NULL : "bg-background/92") {
        if (searching) {
            band_search(st, 0.0f, f);
        } else if (!squeezed) {
            rcColumn(.gap = 1, .overflow = "hidden", .w = "grow", .className = "min-w-0") {
                rcTextC(gallery_scope_title(st), .font = rcFontSemibold(phone ? 20.0f : 15.0f),
                        .color = b.ink, .wrap = "e", .select = RC_SELECT_NONE);
                rcTextC(wall ? band_subtitle(st, st->wallScroll, f)
                             : st->tab == GAL_TAB_MEMORIES ? "Stories from your trips"
                                                           : "Your collections",
                        .color = b.muted, .size = 12, .wrap = "e", .select = RC_SELECT_NONE);
            }
        } else if (!fieldOut) {
            rcSpacer() {}
        }
        rcRow(.gap = 8, .pr = (float)(fieldOut ? 0 : 12), .align = "cc",
              .noDrag = true) {
            if (wall && st->store.visibleCount > 0 && !searching) {
                /* A short bar gives the capsule's room to the title and its dates: the
                   segments still change the view, and the - and = keys the tile size. */
                if (phone || room >= GAL_BAND_ZOOM)
                    band_zoom(st, f, &b);
                if (!phone && !(st->searchOpen && !roomy))
                    band_segments(st, f, &b);
                if (!phone) {
                    if (ui_icon("slideshow", rcIconPlay, b.ink, b.button, f->tap, 14.0f, true,
                                "Slideshow"))
                        player_open_slideshow(st);
                }
                /* Only All Photos has tiles to pick: elsewhere a click zooms a level. */
                if (st->view == GAL_VIEW_ALL
                    && ui_icon("select", rcIconCircleCheck,
                               st->selecting ? f->style.primary : b.ink, b.button, f->tap,
                               16.0f, true, st->selecting ? "Stop selecting" : "Select")) {
                    if (st->selecting)
                        gallery_stop_selecting(st);
                    else
                        st->selecting = true;
                }
            }
            if (wall && !phone && !fieldOut
                && ui_icon("search_open", rcIconSearch, b.ink, b.button, f->tap, 16.0f, true,
                           "Search")) {
                st->searchOpen = true;
                rcSetFocus("search");
            }
            if (phone && ui_icon("appearance", st->darkMode ? rcIconSun : rcIconMoon, b.ink,
                                 b.button, f->tap, 16.0f, true,
                                 st->darkMode ? "Light appearance" : "Dark appearance"))
                st->darkMode = !st->darkMode;
        }
        if (fieldOut)
            band_search(st, roomy ? 200.0f : 0.0f, f);
        /* The window's own controls draw their glyphs, and the wash minimize and maximize show
           under the pointer, in the style's text color, which in the light theme is near-black
           and barely shows on the scrim: over photographs their class makes both white. They
           draw nothing on the web and on a phone. */
        rcWindowControls(.className = f->over ? "text-white" : NULL);
    }
}

/* The phone's floating view switcher, over the foot of the wall as iOS draws it. iOS blurs
   what lies behind it; here the glass class is dense instead, so the labels read on any row
   of photographs. */
static void wall_float_switcher(AppState *st, const GalFrame *f) {
    GalFrame over = *f;
    over.over = true;
    const GalBandInk b = band_ink(&over);
    rcRow(.id = "ViewSwitch",
          .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_BOTTOM_CENTER,
                        .element = RC_ANCHOR_BOTTOM_CENTER, .offset = { 0.0f, -14.0f },
                        .zIndex = 3 }) {
        band_segments(st, &over, &b);
    }
}

/* The floating bar a selection brings up: what can be done to everything picked at once. */
static void wall_selection_bar(AppState *st, const GalFrame *f) {
    if (!st->selecting || st->view != GAL_VIEW_ALL)
        return;
    const bool deleted = st->scope == GAL_SCOPE_DELETED;
    const bool some = st->pickedCount > 0;
    rcRow(.id = "SelectBar", .gap = 4, .pl = 16, .pr = 6, .py = 6, .align = "cl",
          .borderRadius = "all-full",
          .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_BOTTOM_CENTER,
                        .element = RC_ANCHOR_BOTTOM_CENTER,
                        .offset = { 0.0f, f->docked ? -20.0f : -62.0f }, .zIndex = 3 },
          .className = "glass") {
        rcBox(.pr = 8) {
            ui_label(rcFormat(&g_text, "%d Selected", st->pickedCount).chars, 13, RC_WHITE);
        }
        if (!deleted && ui_icon("sel_fav", rcIconHeart, RC_WHITE, "glass-hover", f->tap, 16.0f,
                                some, "Favorite")) {
            for (int i = 0; i < GAL_PHOTO_COUNT; i++)
                if (st->picked[i])
                    st->store.photo[i].favorite = true;
            gallery_refilter(st);
        }
        /* A ghost lights to the theme's surface-alt and presses to a shade of it, near-white
           in the light theme under this white label: on the glass it takes the glass's hover
           shade for both, under the pointer as the icons beside it do, and with Space held. */
        if (deleted && rcButton("sel_recover", "Recover", .disabled = !some,
                                .className = "tap on-dark glass-hover active:bg-[#46464abe]",
                                .variant = RC_BTN_GHOST)) {
            for (int i = 0; i < GAL_PHOTO_COUNT; i++)
                if (st->picked[i])
                    st->store.photo[i].deleted = false;
            gallery_stop_selecting(st);
            gallery_refilter(st);
        }
        if (!deleted && ui_icon("sel_del", rcIconTrash2, RC_WHITE, "glass-hover", f->tap, 16.0f,
                                some, "Delete"))
            st->confirmDelete = true;
        if (ui_icon("sel_done", rcIconX, RC_WHITE, "glass-hover", f->tap, 16.0f, true, "Done"))
            gallery_stop_selecting(st);
    }
}

static void gallery_confirm_delete(AppState *st, const GalFrame *f) {
    if (!rcBeginModal("modal_delete", &st->confirmDelete))
        return;
    rcColumn(.gap = 14, .w = "300px") {
        rcColumn(.gap = 4, .w = "grow") {
            rcText(rcFormat(&g_text, "Delete %d Photo%s?", st->pickedCount,
                            st->pickedCount == 1 ? "" : "s"),
                   .font = rcFontSemibold(17.0f), .color = f->style.text);
            rcTextL("They stay in Recently Deleted until you recover them.",
                    .color = f->style.textMuted, .size = 13);
        }
        rcRow(.gap = 8, .w = "grow") {
            rcSpacer() {}
            if (rcButton("del_cancel", "Cancel"))
                st->confirmDelete = false;
            if (rcButton("del_ok", "Delete", .variant = RC_BTN_DANGER)) {
                for (int i = 0; i < GAL_PHOTO_COUNT; i++)
                    if (st->picked[i])
                        st->store.photo[i].deleted = true;
                st->confirmDelete = false;
                /* The scope stays, so the wall closes the gaps where the reader is, as
                   Photos does, rather than opening at its top as a new scope does. */
                gallery_stop_selecting(st);
                gallery_refilter(st);
            }
        }
    }
    rcEndModal();
}

/* ---- the sidebar ---- */

static bool side_row(const char *id, RC_IconCallback icon, const RC_Image *thumb, const char *label,
                     const char *count, bool on, const GalFrame *f) {
    const RC_Style *s = &f->style;
    rcRow(.id = id, .focusable = true, .gap = 10, .px = 8, .align = "cl",
          .borderRadius = "all-lg", .w = "grow", .hType = RC_PX(f->coarse ? 44 : 30),
          .className = on ? "row-on" : "row-hover") {
        if (icon) {
            rcBox(.align = "cc", .w = "20px", .h = "20px") {
                icon(16.0f, on ? s->primary : f->pal.sideMuted);
            }
        } else {
            rcBox(.bg = s->surfaceAlt, .borderRadius = "all-sm", .w = "20px", .h = "20px",
                  .image = thumb, .imageFit = RC_IMAGE_COVER) {}
        }
        /* A long album name ends in an ellipsis rather than pushing its count out. */
        rcBox(.overflow = "hidden", .w = "grow") {
            rcTextC(label, .color = s->text, .size = 13, .wrap = "e", .select = RC_SELECT_NONE);
        }
        if (count)
            ui_label(count, 12, f->pal.sideMuted);
    }
    return rcClicked(id);
}

static void side_heading(const char *label, const GalFrame *f) {
    rcRow(.pt = 14, .pb = 4, .px = 8, .w = "grow") {
        ui_strong(label, 12, f->pal.sideMuted);
    }
}

/* The sidebar floats above the wall on an 8 px inset. Its fill is a tiny rendering of the
   scope's first photo, faint over the theme's pale, so it picks up the library's colors as real
   glass would and a dark photo cannot take the text's contrast with it. .imageTint sets the
   picture's opacity; the .bg paints behind it. A window too short for every row scrolls it, and
   its bar, having a class, takes its own 12 px band under every pointer: nothing pads for it. */
static void gallery_sidebar(AppState *st, const GalFrame *f) {
    static const char *const ALBUM_IDS[GAL_ALBUM_COUNT] = {
        "side_a0", "side_a1", "side_a2", "side_a3", "side_a4", "side_a5",
    };
    const GalStore *d = &st->store;
    const RC_Color  tint = rcWithAlpha(RC_WHITE, st->darkMode ? 0.08f : 0.11f);
    const bool      lib = st->tab == GAL_TAB_LIBRARY;

    rcColumn(.p = 8, .h = "grow", .wType = RC_PX(GAL_SIDE_W)) {
        rcColumn(.id = "Sidebar", .bg = f->pal.sideFill, .gap = 1,
                 .p = 10, .px = 12, .scroll = "v", .borderRadius = "all-2xl",
                 .border = { .color = f->pal.sideEdge, .width = 1 }, .w = "grow", .h = "grow",
                 .shadow = { .color = rcWithAlpha(RC_BLACK, st->darkMode ? 0.43f : 0.133f),
                             .x = 0, .y = 4, .blur = 18, .spread = 0 },
                 .image = &g_ambient, .imageFit = RC_IMAGE_COVER, .imageTint = tint) {
            if (side_row("side_library", rcIconImages, NULL, "Library", d->libraryStr,
                         lib && st->scope == GAL_SCOPE_LIBRARY && !st->search[0], f))
                gallery_pick_scope(st, GAL_TAB_LIBRARY, GAL_SCOPE_LIBRARY, 0);
            if (side_row("side_memories", rcIconGalleryVerticalEnd, NULL, "Memories", NULL,
                         st->tab == GAL_TAB_MEMORIES, f))
                gallery_pick_scope(st, GAL_TAB_MEMORIES, GAL_SCOPE_LIBRARY, 0);
            side_heading("Pinned", f);
            if (side_row("side_favorites", rcIconHeart, NULL, "Favorites", d->favoritesStr,
                         lib && st->scope == GAL_SCOPE_FAVORITES, f))
                gallery_pick_scope(st, GAL_TAB_LIBRARY, GAL_SCOPE_FAVORITES, 0);
            if (side_row("side_recents", rcIconClock, NULL, "Recents", d->recentsStr,
                         lib && st->scope == GAL_SCOPE_RECENTS, f))
                gallery_pick_scope(st, GAL_TAB_LIBRARY, GAL_SCOPE_RECENTS, 0);
            if (side_row("side_deleted", rcIconTrash2, NULL, "Recently Deleted", d->deletedStr,
                         lib && st->scope == GAL_SCOPE_DELETED, f))
                gallery_pick_scope(st, GAL_TAB_LIBRARY, GAL_SCOPE_DELETED, 0);
            side_heading("Albums", f);
            for (int a = 0; a < GAL_ALBUM_COUNT; a++) {
                int cover = d->album[a].cover;
                if (side_row(ALBUM_IDS[a], NULL, cover >= 0 ? &g_thumb[cover] : NULL,
                             GAL_ALBUM_NAMES[a],
                             d->album[a].count, lib && st->scope == GAL_SCOPE_ALBUM
                                 && st->album == a, f))
                    gallery_pick_scope(st, GAL_TAB_LIBRARY, GAL_SCOPE_ALBUM, a);
                if (cover >= 0)
                    picture_want(st, cover, 20.0f * f->pixels, 20.0f * f->pixels);
            }
            rcSpacer() {}
            if (side_row("side_theme", st->darkMode ? rcIconSun : rcIconMoon, NULL,
                         st->darkMode ? "Light Appearance" : "Dark Appearance", NULL, false, f))
                st->darkMode = !st->darkMode;
        }
    }
}

/* ---- the phone's tabs and the two pages a tab can show ---- */

static bool tab_item(const char *id, RC_IconCallback icon, const char *label, bool on,
                     const GalFrame *f) {
    RC_Color ink = on ? f->style.primary : f->style.textMuted;
    rcColumn(.id = id, .focusable = true, .gap = 2, .align = "cc", .w = "grow", .h = "grow") {
        icon(22.0f, ink);
        ui_label(label, 11, ink);
    }
    return rcClicked(id);
}

static void gallery_tabbar(AppState *st, const GalFrame *f) {
    rcBox(.bg = f->style.border, .w = "grow", .h = "1px") {}
    rcRow(.bg = f->style.chrome, .pb = f->safe.bottom, .pl = f->safe.left, .pr = f->safe.right,
          .w = "grow", .hType = RC_PX(GAL_TABBAR_H + f->safe.bottom)) {
        if (tab_item("tab_library", rcIconImages, "Library", st->tab == GAL_TAB_LIBRARY, f))
            gallery_pick_scope(st, GAL_TAB_LIBRARY, GAL_SCOPE_LIBRARY, 0);
        if (tab_item("tab_memories", rcIconGalleryVerticalEnd, "Memories",
                     st->tab == GAL_TAB_MEMORIES, f))
            gallery_pick_scope(st, GAL_TAB_MEMORIES, GAL_SCOPE_LIBRARY, 0);
        if (tab_item("tab_albums", rcIconFolder, "Albums", st->tab == GAL_TAB_ALBUMS, f))
            gallery_pick_scope(st, GAL_TAB_ALBUMS, GAL_SCOPE_LIBRARY, 0);
        if (tab_item("tab_search", rcIconSearch, "Search", st->tab == GAL_TAB_SEARCH, f)) {
            gallery_pick_scope(st, GAL_TAB_SEARCH, GAL_SCOPE_LIBRARY, 0);
            rcSetFocus("search");
        }
    }
}

/* One memory's card, its key photo full bleed. Its play button lights while the pointer is
   anywhere on the card: a state prefix reads only its own element, so the card's hover is
   asked for by name. */
static void memory_card(AppState *st, int c, float w, float h, const GalFrame *f) {
    static const char *const ID[GAL_MEMORY_COUNT] = { "mem0", "mem1", "mem2", "mem3", "mem4" };
    static const char *const SCRIM[GAL_MEMORY_COUNT] = { "mems0", "mems1", "mems2", "mems3",
                                                        "mems4" };
    const GalMemory *m = &GAL_MEMORIES[c];
    const int  cover = st->store.memoryCover[c];
    const bool hot = rcShowsHover(ID[c]);
    picture_want(st, cover, w * f->pixels, h * f->pixels);
    rcColumn(.id = ID[c], .focusable = true, .bg = f->style.surfaceAlt, .align = "bl",
             .borderRadius = "all-xl", .wType = RC_PX(w), .hType = RC_PX(h),
             .image = &g_thumb[cover], .imageFit = RC_IMAGE_COVER) {
        rcColumn(.id = SCRIM[c], .gap = 2, .p = 16, .w = "grow",
                 .gradient = { .from = rcWithAlpha(RC_BLACK, 0),
                               .to = rcWithAlpha(RC_BLACK, 0.667f), .dir = "v" },
                 .className = "rounded-b-xl") {
            rcRow(.gap = 10, .align = "cl", .w = "grow") {
                rcColumn(.gap = 2, .w = "grow") {
                    ui_strong(m->title, 20, RC_WHITE);
                    ui_label(m->subtitle, 13, rcWithAlpha(RC_WHITE, 0.882f));
                }
                rcBox(.align = "cc", .borderRadius = "all-full", .w = "40px", .h = "40px",
                      .className = hot ? "bg-white" : "glass") {
                    rcIconPlay(16.0f, hot ? f->pal.onWhite : RC_WHITE);
                }
            }
        }
    }
    if (rcClicked(ID[c]))
        player_open_memory(st, c);
}

/* The Memories page fills the window: two wide cards over three, sized from the room there
   is. On a narrow window they stack, and the page scrolls. */
static void gallery_memories_page(AppState *st, const GalFrame *f) {
    const float pad = 16.0f, gap = 12.0f;
    const float top = f->bandFoot + 8.0f;
    const float W = f->wallW - 2.0f * pad;
    const bool  stack = f->wallW < 560.0f;
    float rowH = (f->wallH - top - pad - gap) * 0.5f;
    rowH = rowH < 180.0f ? 180.0f : rowH;

    rcColumn(.id = "MemoryScroll", .gap = (float)gap, .pt = (float)top, .pb = (float)pad,
             .px = (float)pad, .scroll = "v", .w = "grow", .h = "grow") {
        if (stack) {
            for (int c = 0; c < GAL_MEMORY_COUNT; c++)
                memory_card(st, c, W, W * 0.62f, f);
        } else {
            rcRow(.gap = (float)gap) {
                for (int c = 0; c < 2; c++)
                    memory_card(st, c, (W - gap) * 0.5f, rowH, f);
            }
            rcRow(.gap = (float)gap) {
                for (int c = 2; c < GAL_MEMORY_COUNT; c++)
                    memory_card(st, c, (W - gap * 2.0f) / 3.0f, rowH, f);
            }
        }
    }
}

/* The phone's Albums tab: covers two to a row, the smart albums first, and under them the one
   place a deleted photo can be got back from where there is no sidebar. */
static void gallery_albums_page(AppState *st, const GalFrame *f) {
    static const char *const ID[GAL_ALBUM_COUNT + 2] = {
        "alb_f", "alb_r", "alb_0", "alb_1", "alb_2", "alb_3", "alb_4", "alb_5",
    };
    const GalStore *d = &st->store;
    const uint16_t pad = 16, gap = 14;
    const float cw = (f->wallW - 2.0f * pad - (float)gap) / 2.0f;

    rcColumn(.id = "AlbumScroll", .gap = 16, .pt = (float)(f->bandFoot + 8.0f), .pb = 24,
             .px = pad, .scroll = "v", .w = "grow", .h = "grow") {
        for (int r = 0; r < GAL_ALBUM_COUNT + 2; r += 2) {
            rcRow(.gap = gap) {
                for (int c = r; c < r + 2; c++) {
                    const int a = c - 2;
                    const int cover = c == 0 ? d->favoritesCover : c == 1 ? d->recentsCover
                                                                          : d->album[a].cover;
                    const char *name = c == 0 ? "Favorites"
                                       : c == 1 ? "Recents" : GAL_ALBUM_NAMES[a];
                    const char *num = c == 0 ? d->favoritesStr
                                      : c == 1 ? d->recentsStr : d->album[a].count;
                    if (cover >= 0)
                        picture_want(st, cover, cw * f->pixels, cw * f->pixels);
                    rcColumn(.id = ID[c], .focusable = true, .gap = 2, .wType = RC_PX(cw)) {
                        rcBox(.bg = f->style.surfaceAlt, .borderRadius = "all-lg",
                              .wType = RC_PX(cw),
                              .hType = RC_PX(cw), .image = cover >= 0 ? &g_thumb[cover] : NULL,
                              .imageFit = RC_IMAGE_COVER, .overlay = rcShowsHover(ID[c])
                              ? rcWithAlpha(RC_WHITE, 0.118f) : RC_UNSET_COLOR) {}
                        rcBox(.pt = 6) { ui_label(name, 14, f->style.text); }
                        ui_label(num, 12, f->style.textMuted);
                    }
                    if (rcClicked(ID[c]))
                        gallery_pick_scope(st, GAL_TAB_LIBRARY,
                                           c == 0 ? GAL_SCOPE_FAVORITES : c == 1 ? GAL_SCOPE_RECENTS
                                                                                 : GAL_SCOPE_ALBUM,
                                           a < 0 ? 0 : a);
                }
            }
        }
        if (side_row("alb_deleted", rcIconTrash2, NULL, "Recently Deleted", d->deletedStr, false,
                     f))
            gallery_pick_scope(st, GAL_TAB_LIBRARY, GAL_SCOPE_DELETED, 0);
    }
}

/* The main pane: a page, and over it the toolbar and whatever else floats. The viewer brings
   its own bar and needs the titlebar's ids, which are one each, so the wall's chrome stands
   down the moment a photo opens. Those floats sit at z 2 and 3, over the wall's scrollbar;
   the viewer lifts itself over the wall's tiles (ui_viewer.h), and while the player is open
   the page is not declared at all. */
static void gallery_pane(AppState *st, const GalFrame *f, bool chrome) {
    rcColumn(.id = "Pane", .w = "grow", .h = "grow") {
        if (st->tab == GAL_TAB_MEMORIES) {
            gallery_memories_page(st, f);
        } else if (st->tab == GAL_TAB_ALBUMS) {
            gallery_albums_page(st, f);
        } else {
            wall_draw(st, f);
            if (chrome && !f->docked && st->store.visibleCount > 0)
                wall_float_switcher(st, f);
            if (chrome)
                wall_selection_bar(st, f);
        }
        if (chrome)
            wall_band(st, f);
    }
}

/* Shortcuts, as Photos takes them. A field that has focus has the keyboard to itself. */
static void gallery_keys(AppState *st) {
    if (rcIsFocused("search") || rcIsFocused("gal_caption") || rcModDown(RC_MOD_PRIMARY))
        return;
    /* Read only where it steps back: at the root Android's Back stays the system's. */
    if ((st->selecting || st->searchOpen || st->search[0] || st->view != GAL_VIEW_ALL)
        && rcKeyPressed(RC_KEY_ESCAPE)) {
        if (st->selecting) {
            gallery_stop_selecting(st);
        } else if (st->searchOpen || st->search[0]) {
            gallery_end_search(st);
        } else if (st->view != GAL_VIEW_ALL) {
            st->view = GAL_VIEW_ALL;
        }
    }
    /* The keys that move a page work on whichever page is up. */
    const char *page = st->tab == GAL_TAB_MEMORIES ? "MemoryScroll"
                       : st->tab == GAL_TAB_ALBUMS ? "AlbumScroll"
                       : st->store.visibleCount > 0 ? "Wall" : NULL;
    if (page) {
        if (rcKeyPressed(RC_KEY_HOME)) rcScrollToTop(page);
        if (rcKeyPressed(RC_KEY_END)) rcScrollToBottom(page);
        if (rcKeyPressed(RC_KEY_PAGE_DOWN)) rcScrollBy(page, 0.0f, 480.0f);
        if (rcKeyPressed(RC_KEY_PAGE_UP)) rcScrollBy(page, 0.0f, -480.0f);
    }
    if (st->tab != GAL_TAB_LIBRARY)
        return;
    if (rcKeyPressed(RC_KEY_1)) st->view = GAL_VIEW_YEARS;
    if (rcKeyPressed(RC_KEY_2)) st->view = GAL_VIEW_MONTHS;
    if (rcKeyPressed(RC_KEY_3)) st->view = GAL_VIEW_ALL;
    if (rcKeyPressed(RC_KEY_EQUAL) && st->view == GAL_VIEW_ALL && st->zoomStep > 0)
        st->zoomStep--;
    if (rcKeyPressed(RC_KEY_MINUS) && st->view == GAL_VIEW_ALL && st->zoomStep < GAL_ZOOM_STEPS - 1)
        st->zoomStep++;
    /* The field takes focus next frame: the slash's own character is delivered with this one. */
    if (rcKeyPressed(RC_KEY_SLASH)) {
        st->searchOpen = true;
        st->focusSearch = true;
    }
}

/* ---- the contract ---- */

void gallery_seed(AppState *st, unsigned seed) {
    (void)seed;              /* the catalog is curated: the seed is for the contract's symmetry */
    gal_zero(st, sizeof *st);
    gallery_backend_seed(&st->store);
    if (!g_text.buffer)
        g_text = rcArenaInit(GAL_TEXT_BYTES);
    st->selected = GALLERY_BENCH_PICK;
    st->captionOf = -1;
    st->view = GAL_VIEW_ALL;
    st->zoomStep = GAL_ZOOM_DEFAULT;
    st->morph = 1.0f;
    st->anchor = -1;
    st->yearsFrom = -1;
    st->ambientOf = -1;
    st->player.rate = 1.0f;
    st->player.captions = true;
    st->seeded = true;

    static const char HEX[] = "0123456789abcdef";
    for (int i = 0; i < GAL_PHOTO_COUNT; i++) {
        const char a = HEX[(i >> 4) & 15], c = HEX[i & 15];
        const char t[4] = { 't', a, c, 0 }, k[4] = { 'k', a, c, 0 }, m[4] = { 'm', a, c, 0 };
        const char d[4] = { 'c', a, c, 0 }, n[4] = { 'l', a, c, 0 };
        gal_copy(st->tileId[i], t, 8);
        gal_copy(st->checkId[i], k, 8);
        gal_copy(st->filmId[i], m, 8);
        gal_copy(st->cardId[i], d, 8);
        gal_copy(st->lineId[i], n, 8);
    }

    /* Every tile starts as a small rendering of its own picture, cheap enough to do them all
       here, so the wall opens full and sharpens in place. */
    for (int i = 0; i < GAL_PHOTO_COUNT; i++) {
        const GalPhoto *p = &st->store.photo[i];
        PgShot shot;
        int    w, h;
        gal_photo_size(p, GAL_TINY_EDGE, &w, &h);
        gal_photo_shot(p, &shot);
        pg_begin(&g_smallJob, &shot, g_small, w, h);
        pg_step(&g_smallJob, h);
        rcImageUpdate(&g_thumb[i], g_small, w, h, 0, RC_PIXEL_RGBA8);
    }
    for (int s = 0; s < GAL_FULL_SLOTS; s++)
        g_full[s].photo = -1;
    g_jobPhoto = -1;
    g_tilePhoto = -1;
    gallery_ambient(st, st->store.run[0].cover);
}

void gallery_update(AppState *st, const AppCtx *ctx) {
    /* Layout eases on this delta, whichever runner supplied it. */
    st->dt = ctx->dt;
    /* dt 0 is the benchmark's freeze: nothing advances, so two renders match byte for byte. */
    if (ctx->dt <= 0.0f)
        return;
    st->frame++;
    if (st->morph < 1.0f) {
        st->morph += ctx->dt / GAL_MOTION_SEC;
        if (st->morph > 1.0f)
            st->morph = 1.0f;
    }
    int want[GAL_FULL_SLOTS], wants = 0;
    viewer_want(st, want, &wants);
    player_update(st, ctx->dt, want, &wants);
    /* The scripted run keeps its hands off the disk. */
    gallery_pump(st, want, wants, ctx->mode != APP_BENCH);
}

bool gallery_wants_frame(const AppState *st) {
    bool owed = false;
    for (int i = 0; i < GAL_PHOTO_COUNT; i++)
        owed = owed || picture_owed(st, i);
    return owed || picture_spare(st) >= 0 || st->working || st->focusSearch || st->morph < 1.0f
        || viewer_animating(st) || player_animating(st);
}

/* The wall's scrollbars lie over photographs and tinted glass: a thumb, and no track of their
   own. A bar with a class is one bar under every pointer, so a finger gets this thumb too. */
static void wall_scrollbar(const char *id) {
    rcScrollbar(id, .className = "bg-transparent");
}

void gallery_layout(AppState *st, const AppCtx *ctx) {
    GalFrame f;
    /* rcFormat writes this frame's text into the arena, where it has to last until the frame
       is drawn: a reset here, at the top of the next one, takes it all back with no free. */
    rcArenaReset(&g_text);
    rcSetStyle(gallery_style(st->darkMode));
    f.style = rcGetStyle();
    f.pal = gallery_palette(st->darkMode);
    gallery_sync_caption(st);

    f.safe = app_safe(ctx);
    f.viewW = (int)app_view_w(ctx);
    f.viewH = (int)app_view_h(ctx);
    f.zoom = ctx->zoom > 0.0f ? ctx->zoom : 1.0f;
    f.pixels = f.zoom * (st->density > 0.0f ? st->density : 1.0f);
    f.coarse = ctx->view.coarsePointer;
    f.tap = f.coarse ? 44.0f : 32.0f;
    gallery_classes(st->darkMode, f.coarse);
    f.dt = st->dt;
    /* The toolbar keeps its size in window px, so the room it takes from the layout under it
       shrinks as the content zooms in. */
    f.bandH = (float)GALLERY_BAND_H / f.zoom;
    /* Branch on the space, never the platform. */
    f.docked = (float)f.viewW - f.safe.left - f.safe.right >= GAL_DOCK_MIN_W
               && (float)f.viewH - f.safe.top - f.safe.bottom >= GAL_DOCK_MIN_H;
    f.bandFoot = f.bandH + (f.docked ? 0.0f : f.safe.top);
    f.wallX = f.safe.left + (f.docked ? GAL_SIDE_W : 0.0f);
    f.wallY = f.docked ? f.safe.top : 0.0f;
    f.wallW = (float)f.viewW - f.safe.left - f.safe.right - (f.docked ? GAL_SIDE_W : 0.0f);
    f.wallH = (float)f.viewH - f.wallY - (f.docked ? f.safe.bottom
                                          : GAL_TABBAR_H + f.safe.bottom + 1.0f);
    /* The toolbar floats over photographs only where a full row is there to carry it. */
    f.over = st->view == GAL_VIEW_ALL && (st->tab == GAL_TAB_LIBRARY || st->tab == GAL_TAB_SEARCH)
             && (f.docked ? gal_wall_fills_row(&st->store, f.wallW, wall_row_h(st, &f))
                          : st->store.visibleCount >= wall_grid_cols(st, &f));

    /* A click below may open an overlay. It starts on the next frame: this one has already
       declared the titlebar's ids in the wall's toolbar. */
    const bool viewerUp = st->viewer.open, playerUp = st->player.open;
    const bool paneUp = !viewer_covers(st) && !player_covers(st);
    if (st->focusSearch) {
        rcSetFocus("search");
        st->focusSearch = false;
    }
    if (!viewerUp && !playerUp)
        gallery_keys(st);

    /* Docked, the insets pad the whole room. On a phone the wall runs under the status bar and
       the tab bar under the home indicator and a side cutout: each pads its own content. */
    rcRow(.id = "Root", .bg = f.style.background, .pt = (float)(f.docked ? f.safe.top : 0.0f),
          .pb = (float)(f.docked ? f.safe.bottom : 0.0f),
          .pl = (float)(f.docked ? f.safe.left : 0.0f),
          .pr = (float)(f.docked ? f.safe.right : 0.0f), .w = "grow", .h = "grow") {
        if (paneUp) {
            if (f.docked) {
                gallery_sidebar(st, &f);
                gallery_pane(st, &f, !viewerUp);
            } else {
                rcColumn(.w = "grow", .h = "grow") {
                    rcColumn(.pl = f.safe.left, .pr = f.safe.right, .w = "grow", .h = "grow") {
                        gallery_pane(st, &f, !viewerUp);
                    }
                    gallery_tabbar(st, &f);
                }
            }
        }
    }
    gallery_confirm_delete(st, &f);
    if (viewerUp)
        viewer_layout(st, &f);
    if (playerUp)
        player_layout(st, &f);

    /* A bar goes with the pane that declared it this frame. */
    if (paneUp) {
        if (st->tab == GAL_TAB_MEMORIES)
            wall_scrollbar("MemoryScroll");
        else if (st->tab == GAL_TAB_ALBUMS)
            wall_scrollbar("AlbumScroll");
        else if (st->store.visibleCount > 0)
            wall_scrollbar("Wall");
        if (f.docked)
            wall_scrollbar("Sidebar");
    }
}

/* Presses the element @p id on this frame and lets go on the next, wherever it is laid out, so
   the scenario never carries a coordinate a layout change could move. */
static void bench_press(const AppInputSink *in, const char *id) {
    RC_Box box = rcGetElementBox(id);
    in->move(in->ctx, box.x + box.width * 0.5f, box.y + box.height * 0.5f);
    in->button(in->ctx, APP_MBTN_LEFT, true);
}

/* The scripted scenario, driven through the input sink so the real hit test, focus and caret
   paths run: hover and scroll the wall, open the seeded photo, open its Info and type a
   caption, go back. It holds on the wall with nothing focused or hovered, which is the frame
   the benchmark measures. It opens no media and no socket. */
void gallery_bench_step(AppState *st, const AppInputSink *in, int frame) {
    static const char TYPED[] = "Worth the early alarm.";
    if (!in || frame >= GALLERY_BENCH_WARMUP)
        return;
    if (frame >= 6 && frame < 12) {
        in->move(in->ctx, 700.0f, 400.0f);
        in->wheel(in->ctx, 0.0f, -1.0f);
    } else if (frame == 14) {
        bench_press(in, st->tileId[GALLERY_BENCH_PICK]);
    } else if (frame == 28) {
        bench_press(in, "viewer_info");
    } else if (frame == 32) {
        bench_press(in, "gal_caption");
    } else if (frame >= 34 && frame < 34 + (int)sizeof TYPED - 1) {
        in->text(in->ctx, (unsigned int)(unsigned char)TYPED[frame - 34]);
    } else if (frame == 58) {
        bench_press(in, "viewer_back");
    } else if (frame == 15 || frame == 29 || frame == 33 || frame == 59) {
        in->button(in->ctx, APP_MBTN_LEFT, false);
    } else if (frame == GALLERY_BENCH_WARMUP - 2) {
        in->move(in->ctx, -100.0f, -100.0f);      /* blur the caption and park off-canvas */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == GALLERY_BENCH_WARMUP - 1) {
        in->button(in->ctx, APP_MBTN_LEFT, false);
    }
}
