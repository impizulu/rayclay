/*
    platformer_app.c - Pulse Dash's screens, drawn with nothing but the public API.

    Shows: a FLOATING SCENE (every sprite is a floating child of one Stage box, at
    its own offset and z, marked RC_CAPTURE_PASSTHROUGH so a press falls through to
    the playfield), vector sprites with rcIconEmit, one design space mapped onto
    whatever window the user gives, and a custom titlebar that is part of the game:
    the course's waveform plays across it and a tiny runner jumps along its top.

    The scene is rebuilt from the world every frame and holds no state of its own,
    so the screens only choose what to declare over it.
*/
#define PLATFORMER_BACKEND_IMPLEMENTATION
#include "platformer_app.h"
#include "icons/rc_icons_arrow_left.h"
#include "icons/rc_icons_chevron_left.h"
#include "icons/rc_icons_chevron_right.h"
#include "icons/rc_icons_list.h"
#include "icons/rc_icons_pause.h"
#include "icons/rc_icons_play.h"
#include "icons/rc_icons_rotate_cw.h"
#include "icons/rc_icons_star.h"
#include "icons/rc_icons_star_filled.h"

/* Warmup frames before the scripted scenario holds still. */
#define PLATFORMER_BENCH_WARMUP 128

/* The stacking order, back to front. Explicit, so a sprite's place in the picture
   does not depend on the order it was declared in. */
enum {
    Z_TILES = 5, Z_SUN = 6, Z_FAR = 8, Z_NEAR = 10, Z_MOTES = 12, Z_GROUND = 20, Z_OBJECT = 30,
    Z_TRAIL = 36, Z_HERO = 40, Z_PARTS = 50, Z_FLASH = 60, Z_HUD = 70, Z_UI = 80, Z_UI_TOP = 82,
    Z_TITLEBAR = 90
};

/* The chrome's own spacing scale, in Stage px and never scaled with the world. */
#define PL_EDGE 16

/* The play HUD's row, from the top of the Stage: the sun stays under it. */
#define PL_HUD_H 64.0f

typedef struct {
    const char *name;
    uint32_t body, eye;
    int face;
} PlSkin;

static const PlSkin SKINS[PL_SKIN_COUNT] = {
    { "Volt",   0xffd60a, 0x0b1020, 0 },
    { "Ion",    0x35e0ff, 0x05121f, 1 },
    { "Ember",  0xff6a2c, 0x1f0700, 2 },
    { "Mint",   0x54f2a0, 0x04170d, 0 },
    { "Plasma", 0xff4fd8, 0x1e0417, 1 },
    { "Ghost",  0xf1f5ff, 0x1a2140, 2 },
};
static const char *const SKIN_IDS[PL_SKIN_COUNT] = {
    "Skin0", "Skin1", "Skin2", "Skin3", "Skin4", "Skin5"
};
static const char *const DOT_IDS[PL_LEVEL_COUNT] = { "Dot0", "Dot1", "Dot2" };

/* -- color -------------------------------------------------------------------- */

/* What the sprites keep on every course, while the palette below changes under them. */
#define C_HAZARD 0x06061a
#define C_COIN   0xffd21f
#define C_ORB    0x6ff7e8
#define C_PAD    0xffa63d

typedef struct {
    RC_Color skyTop, skyBot, tile, pillar, groundTop, groundBot, line, accent, panel, muted;
    float pulse;
} PlPalette;

static RC_Color mix(RC_Color a, RC_Color b, float t) {
    RC_Color c;
    c.r = a.r + (b.r - a.r) * t;
    c.g = a.g + (b.g - a.g) * t;
    c.b = a.b + (b.b - a.b) * t;
    c.a = a.a + (b.a - a.a) * t;
    return c;
}

/* A course alternates two themes every eight bars, fading across the first bar of
   each section, like the color changes a rhythm game cues on the music. */
static PlPalette platformer_palette(const PlWorld *w) {
    const PlLevelInfo *info = &PL_LEVELS[w->level];
    const int32_t barPx = PL_BAR_CELLS * PL_CELL;
    const int32_t section = w->scrollX / (barPx * 8);
    const float into = (float)(w->scrollX - section * barPx * 8) / (float)barPx;
    const float t = into < 1.0f ? into : 1.0f;
    const PlTheme *to = &info->theme[section & 1];
    const PlTheme *from = section > 0 ? &info->theme[(section + 1) & 1] : to;
    PlPalette p;

    p.skyTop    = mix(rcHex(from->skyTop), rcHex(to->skyTop), t);
    p.skyBot    = mix(rcHex(from->skyBot), rcHex(to->skyBot), t);
    p.tile      = mix(rcHex(from->tile), rcHex(to->tile), t);
    p.pillar    = mix(rcHex(from->pillar), rcHex(to->pillar), t);
    p.groundTop = mix(rcHex(from->groundTop), rcHex(to->groundTop), t);
    p.groundBot = mix(rcHex(from->groundBot), rcHex(to->groundBot), t);
    p.line      = mix(rcHex(from->line), rcHex(to->line), t);
    p.accent    = mix(rcHex(from->accent), rcHex(to->accent), t);
    p.panel     = mix(p.groundBot, RC_BLACK, 0.35f);
    p.muted     = mix(RC_WHITE, p.skyBot, 0.30f);
    p.pulse     = pl_pulse(w);
    p.skyBot    = mix(p.skyBot, p.tile, p.pulse * 0.30f);
    return p;
}

/* The theme the bundled controls draw from, so a dialog belongs to the course. */
static RC_Style platformer_style(const PlPalette *p) {
    RC_Style s = rcStyleDark();

    s.background   = p->skyTop;
    s.surface      = p->panel;
    s.surfaceAlt   = mix(p->panel, RC_WHITE, 0.10f);
    s.chrome       = mix(p->skyTop, RC_BLACK, 0.35f);
    s.text         = RC_WHITE;
    s.textMuted    = p->muted;
    s.primary      = p->accent;
    s.primaryHover = mix(p->accent, RC_WHITE, 0.22f);
    s.border       = rcWithAlpha(p->accent, 0.376f);
    return s;
}

/* -- the view ------------------------------------------------------------------ */

typedef struct {
    float s;                /* world px -> Stage px                     */
    float stageW, stageH;
    float groundY;          /* the Stage y of the ground line           */
    float camX;             /* the world x at the Stage's left edge     */
    float visible;          /* the Stage's width in world px            */
    float shakeX, shakeY;
} PlView;

/* One scale maps the world onto the Stage: the height decides it, and a narrow
   window lowers it further so the player still sees what is coming. The band under
   the ground grows on a tall window, so a portrait Stage has no acre of empty sky
   at its foot. Read off AppCtx and never off the live window, so a headless run at
   a fixed size takes the same branch as a real window that size. */
static PlView view_for(const PlWorld *w, float stageW, float stageH, bool live) {
    PlView v;
    const float fit = stageW / 900.0f;
    float s = stageH / (float)PL_DESIGN_H;
    float ground, heroAt;

    if (s > fit) s = fit;
    if (s < 0.62f) s = 0.62f;
    if (s > 2.0f) s = 2.0f;
    if (stageH < 1.0f) stageH = 1.0f;
    ground = (float)PL_GROUND_H * s;
    if (ground < stageH * 0.2f)
        ground = stageH * 0.2f;
    heroAt = stageW / s * 0.22f;
    if (heroAt > (float)PL_HERO_X)
        heroAt = (float)PL_HERO_X;

    v.s = s;
    v.stageW = stageW;
    v.stageH = stageH;
    /* Keep a px of sky at any height, since the whole scene sizes itself from it. */
    v.groundY = stageH - ground < 1.0f ? 1.0f : stageH - ground;
    v.camX = (float)(w->scrollX + PL_HERO_X) - heroAt;
    v.visible = stageW / s;
    v.shakeX = 0.0f;
    v.shakeY = 0.0f;
    /* Only the player's own death shakes: a menu's demo must not rattle the menu. */
    if (live && w->phase == PL_DEAD && w->phaseTicks < 16) {
        const float amp = (float)(16 - w->phaseTicks) * 0.8f * s;
        v.shakeX = ((float)pl_scatter(w->clock, 101u, 200u) / 100.0f - 1.0f) * amp;
        v.shakeY = ((float)pl_scatter(w->clock, 211u, 200u) / 100.0f - 1.0f) * amp;
    }
    return v;
}

static float view_x(const PlView *v, float worldX) {
    return (worldX - v->camX) * v->s + v->shakeX;
}

/* A height above the ground, on the Stage. */
static float view_y(const PlView *v, float height) {
    return v->groundY - height * v->s + v->shakeY;
}

/* A scene sprite: attached to the Stage at its own offset, on its own z, and
   PASSTHROUGH so it never eats the press that makes the hero jump. Nothing clips
   them: the window cuts three edges, and the titlebar floats over the fourth, so a
   sprite costs no scissor change and the whole scene draws as one batch. */
static RC_Float at(float x, float y, int z) {
    return RC_LIT(RC_Float){ .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                             .element = RC_ANCHOR_TOP_LEFT, .offset = { x, y },
                             .zIndex = (int16_t)z, .capture = RC_CAPTURE_PASSTHROUGH };
}

static void label(const char *text, uint16_t font, uint16_t size, RC_Color color) {
    rcTextC(text, .font = font, .color = color, .size = size, .select = RC_SELECT_NONE);
}

/* The small caps over every figure. */
static void caption(const char *text, RC_Color color) {
    rcTextC(text, .color = color, .size = 12, .letterSpacing = 1, .select = RC_SELECT_NONE);
}

/* -- vector sprites ------------------------------------------------------------ */

/* The neon look is three strokes of one path, wide and faint under narrow and bright. */
static void neon_stroke(RC_BoundingBox b, const RC_IconPoint *pts, int n, float box, float width,
                        RC_Color c) {
    rcIconDrawPolyline(b, pts, n, box, width * 3.6f, true, rcWithAlpha(c, 0.133f));
    rcIconDrawPolyline(b, pts, n, box, width * 2.0f, true, rcWithAlpha(c, 0.33f));
    rcIconDrawPolyline(b, pts, n, box, width, true, c);
}

/* A hazard is a crystal shard: five faceted points inside the cell, so the run's
   hit box is the cell's and the drawing is ours. */
static const RC_IconPoint SHARD_UP[5] = {
    { 2.0f, 24.0f }, { 7.0f, 10.0f }, { 12.0f, 1.0f }, { 17.0f, 10.0f }, { 22.0f, 24.0f }
};
static const RC_IconPoint SHARD_DOWN[5] = {
    { 2.0f, 0.0f }, { 22.0f, 0.0f }, { 17.0f, 14.0f }, { 12.0f, 23.0f }, { 7.0f, 14.0f }
};

/* userData is the shard. A dark core under a wash of the course's light, one facet
   catching the light, and a lit edge. */
static void draw_shard(RC_BoundingBox b, RC_Color c, const void *data) {
    const RC_IconPoint *s = (const RC_IconPoint *)data;
    const bool up = s[0].y > s[1].y;
    const RC_IconPoint facet[3] = { s[up ? 1 : 4], s[up ? 2 : 3], { 12.0f, up ? 16.0f : 8.0f } };

    rcIconDrawFilledPolygon(b, s, 5, 24.0f, rcWithAlpha(rcHex(C_HAZARD), 0.9f));
    rcIconDrawFilledPolygon(b, s, 5, 24.0f, rcWithAlpha(c, 0.275f));
    rcIconDrawFilledPolygon(b, facet, 3, 24.0f, rcWithAlpha(RC_WHITE, 0.275f));
    neon_stroke(b, s, 5, 24.0f, 0.9f, c);
}

/* An air orb is a four-point sparkle in a soft halo. */
static void draw_orb(RC_BoundingBox b, RC_Color c, const void *data) {
    static const RC_IconPoint STAR[8] = {
        { 12.0f, 1.5f }, { 14.6f, 9.4f }, { 22.5f, 12.0f }, { 14.6f, 14.6f },
        { 12.0f, 22.5f }, { 9.4f, 14.6f }, { 1.5f, 12.0f }, { 9.4f, 9.4f },
    };

    (void)data;
    rcIconDrawFilledCircle(b, 12.0f, 12.0f, 10.5f, 24.0f, rcWithAlpha(c, 0.18f));
    rcIconDrawFilledPolygon(b, STAR, 8, 24.0f, c);
    rcIconDrawFilledCircle(b, 12.0f, 12.0f, 2.6f, 24.0f, RC_WHITE);
}

/* A launch pad is two chevrons on the floor of its cell. */
static void draw_pad(RC_BoundingBox b, RC_Color c, const void *data) {
    static const RC_IconPoint LOW[6] = {
        { 3.0f, 24.0f }, { 3.0f, 21.0f }, { 12.0f, 15.0f }, { 21.0f, 21.0f }, { 21.0f, 24.0f },
        { 12.0f, 18.0f },
    };
    static const RC_IconPoint HIGH[6] = {
        { 5.0f, 17.0f }, { 5.0f, 14.5f }, { 12.0f, 9.5f }, { 19.0f, 14.5f }, { 19.0f, 17.0f },
        { 12.0f, 12.0f },
    };

    (void)data;
    rcIconDrawFilledPolygon(b, LOW, 6, 24.0f, c);
    rcIconDrawFilledPolygon(b, HIGH, 6, 24.0f, rcWithAlpha(c, 0.59f));
}

static void draw_coin(RC_BoundingBox b, RC_Color c, const void *data) {
    (void)data;
    rcIconDrawFilledCircle(b, 12.0f, 12.0f, 10.0f, 24.0f, c);
    rcIconDrawCircleStroke(b, 12.0f, 12.0f, 7.0f, 24.0f, 1.3f, mix(c, RC_BLACK, 0.32f));
    rcIconDrawFilledCircle(b, 12.0f, 12.0f, 3.0f, 24.0f, mix(c, RC_WHITE, 0.6f));
}

/* The coin before any is taken, dim: it says at a glance that there are coins to take. */
static void draw_coin_slot(RC_BoundingBox b, RC_Color c, const void *data) {
    (void)data;
    rcIconDrawFilledCircle(b, 12.0f, 12.0f, 10.0f, 24.0f, rcWithAlpha(c, 0.22f));
    rcIconDrawCircleStroke(b, 12.0f, 12.0f, 7.0f, 24.0f, 1.3f, rcWithAlpha(c, 0.47f));
    rcIconDrawFilledCircle(b, 12.0f, 12.0f, 3.0f, 24.0f, rcWithAlpha(c, 0.47f));
}

static void draw_diamond(RC_BoundingBox b, RC_Color c, const void *data) {
    static const RC_IconPoint GEM[4] = {
        { 12.0f, 2.0f }, { 22.0f, 12.0f }, { 12.0f, 22.0f }, { 2.0f, 12.0f },
    };

    (void)data;
    rcIconDrawFilledPolygon(b, GEM, 4, 24.0f, rcWithAlpha(c, 0.353f));
    neon_stroke(b, GEM, 4, 24.0f, 1.0f, c);
}

/* Centered in its box, not nudged right as a lone play glyph in a circle is: it leads
   the PLAY pill's word, and the pill pads both ends alike. */
static void draw_play(RC_BoundingBox b, RC_Color c, const void *data) {
    static const RC_IconPoint TRI[3] = { { 5.0f, 4.0f }, { 19.0f, 12.0f }, { 5.0f, 20.0f } };

    (void)data;
    rcIconDrawFilledPolygon(b, TRI, 3, 24.0f, c);
}

/* A point of the orb's face, turned with it, in its 64-unit box. */
static void face_dot(RC_BoundingBox b, float dx, float dy, float r, float cs, float sn,
                     RC_Color c) {
    rcIconDrawFilledCircle(b, 32.0f + dx * cs - dy * sn, 32.0f + dx * sn + dy * cs, r, 64.0f, c);
}

/* The hero: a glowing orb that rolls. Its face turns with the roll, and its shine
   stays where the light is, so the turn reads as a ball and not a spinning card. */
static void draw_hero(RC_BoundingBox b, RC_Color c, const void *data) {
    const PlPose *pose = (const PlPose *)data;
    const float cs = pl_cos(pose->turn);
    const float sn = pl_sin(pose->turn);

    rcIconDrawFilledCircle(b, 32.0f, 32.0f, 29.0f, 64.0f, rcWithAlpha(c, 0.157f));
    rcIconDrawFilledCircle(b, 32.0f, 32.0f, 23.0f, 64.0f, c);
    rcIconDrawCircleStroke(b, 32.0f, 32.0f, 22.0f, 64.0f, 2.0f, mix(c, RC_WHITE, 0.45f));
    switch (pose->face) {
    case 1:     /* a crescent */
        face_dot(b, 3.0f, -2.0f, 11.0f, cs, sn, pose->eye);
        face_dot(b, 8.0f, -5.0f, 9.0f, cs, sn, c);
        break;
    case 2:     /* one eye */
        face_dot(b, 2.0f, -2.0f, 7.5f, cs, sn, pose->eye);
        face_dot(b, 4.0f, -4.0f, 2.6f, cs, sn, RC_WHITE);
        break;
    default:    /* two eyes */
        face_dot(b, -7.0f, -3.0f, 4.0f, cs, sn, pose->eye);
        face_dot(b, 7.0f, -3.0f, 4.0f, cs, sn, pose->eye);
        face_dot(b, -6.0f, -4.5f, 1.4f, cs, sn, RC_WHITE);
        face_dot(b, 8.0f, -4.5f, 1.4f, cs, sn, RC_WHITE);
        break;
    }
    rcIconDrawFilledEllipse(b, 24.0f, 22.0f, 7.0f, 4.5f, 64.0f, rcWithAlpha(RC_WHITE, 0.353f));
}

static void set_pose(AppState *st, int slot, int skin, int32_t turn) {
    st->pose[slot].turn = turn;
    st->pose[slot].ink  = rcHex(0x05060f);
    st->pose[slot].eye  = rcHex(SKINS[skin].eye);
    st->pose[slot].face = SKINS[skin].face;
}

static void coin_icon(float size, bool found) {
    if (found)
        rcIconEmit(size, rcHex(C_COIN), draw_coin, NULL);
    else
        rcIconEmit(size, rcHex(C_COIN), draw_coin_slot, NULL);
}

/* -- the scene, back to front -------------------------------------------------- */

/* The sky's haze: soft bands of the course's light that brighten on the beat. */
static void scene_haze(const PlView *v, const PlPalette *p) {
    for (int i = 0; i < 4; i++) {
        const float h = v->groundY * (0.06f + 0.03f * (float)i);
        const float y = v->groundY * (0.20f + 0.20f * (float)i) + v->shakeY;
        const float a = (10.0f + 6.0f * (float)i + p->pulse * 24.0f) / 255.0f;
        const RC_Color haze = rcWithAlpha(p->tile, a), clear = rcWithAlpha(p->tile, 0);

        rcBox(.wType = RC_PX(v->stageW + 32.0f), .hType = RC_PX(h),
              .gradient = { .from = clear, .to = haze, .dir = "v" },
              .floating = at(-16.0f, y - h, Z_TILES)) {}
        rcBox(.wType = RC_PX(v->stageW + 32.0f), .hType = RC_PX(h),
              .gradient = { .from = haze, .to = clear, .dir = "v" },
              .floating = at(-16.0f, y, Z_TILES)) {}
    }
}

/* The sun rides a beat: it swells on the attack and throws one ring outward. In a run
   it stands to the right, low enough to clear the HUD at its largest. On a menu it
   stands dim behind the wordmark, which then never has to read on its bright face. */
static void scene_sun(const PlWorld *w, const PlView *v, const PlPalette *p, bool live) {
    const float base = 216.0f * v->s;
    const float largest = base + 16.0f * v->s;
    const float size = base + p->pulse * 16.0f * v->s;
    const float phase = pl_beat_phase(w);
    const float ring = base * (1.0f + phase * 0.9f);
    float cx = v->stageW * 0.5f;
    float cy = v->groundY * 0.30f;

    if (live) {
        cx = v->stageW * 0.86f;
        if (cx > v->stageW - largest * 0.5f - PL_EDGE)
            cx = v->stageW - largest * 0.5f - PL_EDGE;
        if (cy < PL_HUD_H + largest * 0.5f)
            cy = PL_HUD_H + largest * 0.5f;
    }
    cx += v->shakeX;
    cy += v->shakeY;

    rcBox(.borderRadius = "all-full",
          .border = { .color = rcWithAlpha(p->line, (1.0f - phase) * 110.0f / 255.0f),
                      .width = 2 },
          .wType = RC_PX(ring), .hType = RC_PX(ring),
          .floating = at(cx - ring * 0.5f, cy - ring * 0.5f, Z_SUN)) {}
    rcBox(.borderRadius = "all-full", .wType = RC_PX(size), .hType = RC_PX(size),
          .gradient = { .from = rcWithAlpha(mix(p->accent, RC_WHITE, 0.3f), live ? 1.0f : 0.33f),
                        .to = rcWithAlpha(p->accent, live ? 0.1f : 0.03f), .dir = "v" },
          .shadow = { .color = rcWithAlpha(p->accent, live ? 0.275f : 0.094f),
                      .blur = 44.0f * v->s },
          .floating = at(cx - size * 0.5f, cy - size * 0.5f, Z_SUN)) {}
}

/* One band of skyline columns. Each column's height is its own and holds still; only its
   cap line takes the beat. A silent city whose heights jump reads as a glitch, where a city
   whose roofs glow in time reads as music. */
static void scene_columns(const PlView *v, const PlPalette *p, int z, float widthPx,
                          float divisor, float low, float high, RC_Color color, uint32_t salt) {
    const float width = widthPx * v->s;
    const float scroll = v->camX * v->s / divisor;
    const float skyH = v->groundY;

    for (int col = (int)(scroll / width); (float)col * width - scroll < v->stageW; col++) {
        const uint32_t seed = (uint32_t)col;
        const float rest = low + (high - low) * (float)pl_scatter(seed, salt, 100u) / 100.0f;
        const float h = skyH * rest;

        rcColumn(.bg = color, .borderRadius = "t-sm", .wType = RC_PX(width - 4.0f),
                 .hType = RC_PX(h),
                 .floating = at((float)col * width - scroll + v->shakeX,
                                skyH - h + v->shakeY, z)) {
            rcBox(.bg = rcWithAlpha(p->line, (90.0f + p->pulse * 150.0f) / 255.0f), .w = "grow",
                  .hType = RC_PX((3.0f + p->pulse * 2.0f) * v->s)) {}
        }
    }
}

/* Motes of light rise through the scene, and the clock keeps them moving when the
   hero stops. That clock runs for as long as a course is up, so their travel is worked
   in double and wrapped there: a float of it moves in whole-pixel steps by the third
   day of a session, and its int conversion overflows by the sixth month. */
static void scene_motes(const PlWorld *w, const PlView *v, const PlPalette *p) {
    for (uint32_t i = 0; i < 18u; i++) {
        const float span = v->stageW + 40.0f;
        const double drift = (double)w->clock * (0.4 + (double)pl_scatter(i, 3u, 80u) / 100.0);
        const double rise = (double)w->clock * (0.2 + (double)pl_scatter(i, 5u, 40u) / 100.0);
        const double x0 = (double)pl_scatter(i, 0xC2B2AE35u, 4096u) - drift * v->s;
        const double y0 = (double)pl_scatter(i, 0x27D4EB2Fu, 4096u) - rise * v->s;
        const int sx = (int)fmod(x0, (double)(int)span);
        const int sy = (int)fmod(y0, (double)(int)v->groundY);
        const float size = (2.0f + (float)pl_scatter(i, 7u, 4u)) * v->s;
        const float x = (float)(sx < 0 ? sx + (int)span : sx);
        const float y = (float)(sy < 0 ? sy + (int)v->groundY : sy);

        rcBox(.bg = rcWithAlpha(p->line, (70.0f + p->pulse * 110.0f) / 255.0f),
              .borderRadius = "all-full", .wType = RC_PX(size), .hType = RC_PX(size),
              .floating = at(x, y, Z_MOTES)) {}
    }
}

/* The ground: a band in the course's dark color, lines of light that close up toward
   the horizon, dashes that run past at two speeds, and a line of light across the top
   that fades toward both edges of the window. */
static void scene_ground(const PlView *v, const PlPalette *p) {
    const float bandH = v->stageH - v->groundY + 16.0f;
    const float glow = 8.0f + p->pulse * 14.0f;
    const float thin = v->s < 1.0f ? 1.0f : v->s;

    rcBox(.wType = RC_PX(v->stageW + 32.0f), .hType = RC_PX(bandH),
          .gradient = { .from = p->groundTop, .to = p->groundBot, .dir = "v" },
          .floating = at(-16.0f, v->groundY + v->shakeY, Z_GROUND)) {}
    for (int k = 1; k <= 5; k++) {
        const float f = (float)k / 5.0f;
        const float y = v->groundY + (bandH - 16.0f) * f * f + v->shakeY;

        rcBox(.bg = rcWithAlpha(p->line, (float)(70 - k * 10) / 255.0f), .wType = RC_PX(v->stageW),
              .hType = RC_PX(thin), .floating = at(0.0f, y, Z_GROUND)) {}
    }
    for (int lane = 0; lane < 2; lane++) {
        const float f = lane == 0 ? 0.2f : 0.55f;
        const float y = v->groundY + (bandH - 16.0f) * f * f + v->shakeY;
        const float gap = (lane == 0 ? 120.0f : 180.0f) * v->s;
        const float dash = gap * 0.35f;
        const float scroll = v->camX * v->s * (lane == 0 ? 0.6f : 1.0f);

        for (int col = (int)(scroll / gap); (float)col * gap - scroll < v->stageW; col++) {
            rcBox(.bg = rcWithAlpha(p->line, (lane == 0 ? 90.0f : 120.0f) / 255.0f),
                  .borderRadius = "all-full", .wType = RC_PX(dash),
                  .hType = RC_PX(thin * (lane == 0 ? 2.0f : 3.0f)),
                  .floating = at((float)col * gap - scroll + v->shakeX, y, Z_GROUND)) {}
        }
    }
    rcBox(.wType = RC_PX(v->stageW * 0.5f), .hType = RC_PX(3.0f * v->s),
          .gradient = { .from = rcWithAlpha(p->line, 0), .to = p->line, .dir = "h" },
          .shadow = { .color = rcWithAlpha(p->accent, 0.43f), .blur = glow },
          .floating = at(0.0f, v->groundY - 1.0f + v->shakeY, Z_GROUND + 1)) {}
    rcBox(.wType = RC_PX(v->stageW * 0.5f), .hType = RC_PX(3.0f * v->s),
          .gradient = { .from = p->line, .to = rcWithAlpha(p->line, 0), .dir = "h" },
          .shadow = { .color = rcWithAlpha(p->accent, 0.43f), .blur = glow },
          .floating = at(v->stageW * 0.5f, v->groundY - 1.0f + v->shakeY, Z_GROUND + 1)) {}
}

/* A square vector sprite on the world grid. */
static void cell_sprite(const PlView *v, int32_t col, int32_t row, RC_Color c,
                        RC_CustomDrawCallback draw, const void *data) {
    const float d = (float)PL_CELL * v->s;
    const float x = view_x(v, (float)(col * PL_CELL));
    const float y = view_y(v, (float)((row + 1) * PL_CELL));

    rcBox(.wType = RC_PX(d), .hType = RC_PX(d), .floating = at(x, y, Z_OBJECT)) {
        rcIconEmit(d, c, draw, data);
    }
}

/* A run of blocks in one row, as one box: tinted glass over the dark, neon-edged. */
static void block_run(const PlView *v, const PlPalette *p, int32_t col, int32_t row, int32_t len) {
    const float x = view_x(v, (float)(col * PL_CELL));
    const float y = view_y(v, (float)((row + 1) * PL_CELL));
    const float d = (float)PL_CELL * v->s;

    rcBox(.borderRadius = "all-md",
          .border = { .color = mix(p->accent, p->line, 0.3f), .width = 2 },
          .wType = RC_PX((float)len * d), .hType = RC_PX(d),
          .gradient = { .from = mix(p->accent, rcHex(C_HAZARD), 0.62f),
                        .to = mix(p->accent, rcHex(C_HAZARD), 0.9f), .dir = "v" },
          .shadow = { .color = rcWithAlpha(p->accent, 0.298f), .blur = 10.0f * v->s },
          .floating = at(x, y, Z_OBJECT)) {}
}

/* The course in view. Only the visible columns are walked, so a long course costs
   no more per frame than a short one. Behind a menu (not `live`) the demo run draws no
   coins: a still coin at the pills' height read as one lying under them. */
static void scene_course(const PlWorld *w, const PlView *v, const PlPalette *p, bool live) {
    int32_t col0 = (int32_t)(v->camX / (float)PL_CELL) - 2;
    int32_t col1 = (int32_t)((v->camX + v->visible) / (float)PL_CELL) + 2;

    if (col0 < 0) col0 = 0;
    if (col1 > w->cols - 1) col1 = w->cols - 1;
    for (int32_t row = 0; row < PL_ROWS; row++) {
        for (int32_t col = col0; col <= col1; col++) {
            const uint8_t cell = w->cell[col][row];
            const bool used = (cell & PL_CELL_USED) != 0;

            switch (cell & (uint8_t)~PL_CELL_USED) {
            case PL_CELL_BLOCK: {
                int32_t len = 1;
                while (col + len <= col1 && w->cell[col + len][row] == PL_CELL_BLOCK)
                    len++;
                block_run(v, p, col, row, len);
                col += len - 1;
                break;
            }
            case PL_CELL_SPIKE:
                cell_sprite(v, col, row, p->line, draw_shard, SHARD_UP);
                break;
            case PL_CELL_SPIKE_DOWN:
                cell_sprite(v, col, row, p->line, draw_shard, SHARD_DOWN);
                break;
            case PL_CELL_ORB:
                cell_sprite(v, col, row, used ? rcWithAlpha(rcHex(C_ORB), 0.275f) : rcHex(C_ORB),
                            draw_orb, NULL);
                break;
            case PL_CELL_PAD:
                cell_sprite(v, col, row, rcHex(C_PAD), draw_pad, NULL);
                break;
            case PL_CELL_COIN:
                if (!used && live)
                    cell_sprite(v, col, row, rcHex(C_COIN), draw_coin, NULL);
                break;
            default:
                break;
            }
        }
    }
}

/* The way out: two posts of light and the air between them. A runner with a visible
   finish is a level; without one, a treadmill. */
static void scene_gate(const PlWorld *w, const PlView *v, const PlPalette *p) {
    const float x = view_x(v, (float)w->goalX);
    const float wide = 96.0f * v->s;

    if (x > v->stageW + wide || x + wide < -wide)
        return;
    rcBox(.wType = RC_PX(wide), .hType = RC_PX(v->groundY),
          .gradient = { .from = rcWithAlpha(p->line, 0), .to = rcWithAlpha(p->line, 0.47f),
                        .dir = "h" },
          .floating = at(x - wide, v->shakeY, Z_OBJECT)) {}
    rcBox(.bg = p->line, .borderRadius = "all-full", .wType = RC_PX(8.0f * v->s),
          .hType = RC_PX(v->groundY),
          .shadow = { .color = p->accent, .blur = 24.0f * v->s },
          .floating = at(x, v->shakeY, Z_OBJECT + 1)) {}
}

/* The diamond where a practice run will come back to. */
static void scene_checkpoint(const PlWorld *w, const PlView *v, const PlPalette *p) {
    if (!w->practice || !w->hasCheckpoint)
        return;
    {
        const float d = 28.0f * v->s;
        const float hx = (float)(w->checkpoint.scrollX + PL_HERO_X);
        const float height = (float)(w->checkpoint.y256 / PL_SUB) + 44.0f;

        rcBox(.wType = RC_PX(d), .hType = RC_PX(d),
              .floating = at(view_x(v, hx) - d * 0.5f, view_y(v, height) - d * 0.5f, Z_OBJECT)) {
            rcIconEmit(d, p->accent, draw_diamond, NULL);
        }
    }
}

static RC_Color particle_color(const PlParticle *part, const PlPalette *p, RC_Color hero) {
    switch (part->kind) {
    case PL_PART_TRAIL:
        return p->line;
    case PL_PART_BURST:
        return part->tone == 0 ? hero : part->tone == 1 ? RC_WHITE : p->accent;
    case PL_PART_SPARK:
        return part->tone == 0 ? rcHex(C_ORB) : RC_WHITE;
    case PL_PART_CONFETTI:
        return part->tone == 0 ? p->accent : part->tone == 1 ? rcHex(C_PAD) : rcHex(C_COIN);
    default:
        return p->line;
    }
}

/* Behind a menu (not `live`) no sparks: the demo still takes the coins it does not draw,
   and a burst from nothing would give them away. */
static void scene_particles(const PlWorld *w, const PlView *v, const PlPalette *p, RC_Color hero,
                            bool live) {
    for (int i = 0; i < PL_MAX_PARTICLES; i++) {
        const PlParticle *part = &w->part[i];

        if (part->ttl <= 0 || (!live && part->kind == PL_PART_SPARK))
            continue;
        {
            const float life = (float)part->ttl / (float)part->life;
            const bool trail = part->kind == PL_PART_TRAIL;
            const float size = (float)part->size * v->s * (trail ? life : 1.0f);
            const float x = view_x(v, (float)(part->x256 / PL_SUB)) - size * 0.5f;
            const float y = view_y(v, (float)(part->y256 / PL_SUB)) - size * 0.5f;

            if (x + size < 0.0f || x > v->stageW)
                continue;
            rcBox(.bg = rcWithAlpha(particle_color(part, p, hero),
                                life * (trail ? 90.0f : 235.0f) / 255.0f),
                  .borderRadius = "all-full", .wType = RC_PX(size), .hType = RC_PX(size),
                  .floating = at(x, y, trail ? Z_TRAIL : Z_PARTS)) {}
        }
    }
}

static void scene_hero(AppState *st, const PlView *v, RC_Color hero) {
    const PlWorld *w = &st->world;
    const float d = 64.0f * v->s;
    const float cx = view_x(v, (float)(w->scrollX + PL_HERO_X));
    const float cy = view_y(v, (float)(w->y256 / PL_SUB) + (float)PL_HERO_SIZE * 0.5f);

    if (w->phase == PL_DEAD || (w->phase == PL_WON && w->phaseTicks >= PL_WIN_FLY_TICKS))
        return;
    set_pose(st, POSE_HERO, st->skin, w->spin);
    rcBox(.wType = RC_PX(d), .hType = RC_PX(d),
          .floating = at(cx - d * 0.5f, cy - d * 0.5f, Z_HERO)) {
        rcIconEmit(d, hero, draw_hero, &st->pose[POSE_HERO]);
    }
}

static void scene_flash(const PlWorld *w, const PlView *v) {
    if (w->phase != PL_DEAD || w->phaseTicks >= 12)
        return;
    rcBox(.bg = rcWithAlpha(RC_WHITE, (float)((12 - w->phaseTicks) * 6) / 255.0f),
          .wType = RC_PX(v->stageW), .hType = RC_PX(v->stageH),
          .floating = at(0.0f, 0.0f, Z_FLASH)) {}
}

/* -- the titlebar -------------------------------------------------------------- */

/* The band's parts, in window px. */
#define PL_BAND_GAP   12
#define PL_LOGO_W     24
#define PL_NAME_W    108
#define PL_COURSE_W  100
#define PL_WAVE_STEP   5    /* one bar and its gap */

/* The course's waveform, played across the titlebar: bars behind the hero are lit,
   the ones around it kick on the beat, and a tiny copy of the hero runs along the
   line, jumping when it jumps. It reads the game's clock and nothing else, so it
   stops with the game: when the window sleeps, no frame is drawn for it at all. The
   caller gives the width, so the bars never depend on last frame's layout, and they
   grow to fill it exactly: a .gap is whole layout px, which a fractional zoom rounds. */
static void title_wave(AppState *st, const PlPalette *p, RC_Color hero, float width) {
    const PlWorld *w = &st->world;
    const int count = width >= PL_WAVE_STEP ? (int)width / PL_WAVE_STEP : 1;
    const float head = pl_progress(w) * (float)count;

    set_pose(st, POSE_TITLEBAR, st->skin, w->spin);
    rcRow(.align = "cc", .w = "grow", .h = "grow") {
        rcRow(.gap = 2, .align = "cl", .h = "grow", .wType = RC_PX(count * PL_WAVE_STEP - 2)) {
            for (int i = 0; i < count; i++) {
                const float sample = (float)w->wave[i * PL_WAVE_BARS / count] / 255.0f;
                const float away = (float)i - head;
                const float near = away > 12.0f || away < -12.0f
                                 ? 0.0f : 1.0f - (away < 0.0f ? -away : away) / 12.0f;
                const float h = 3.0f + sample * 15.0f + near * p->pulse * 12.0f;
                const RC_Color lit = mix(p->accent, RC_WHITE, near * p->pulse * 0.7f);
                const float dim = (44.0f + near * 60.0f) / 255.0f;

                rcBox(.bg = away <= 0.0f ? lit : rcWithAlpha(RC_WHITE, dim),
                      .borderRadius = "all-full", .w = "grow", .hType = RC_PX(h)) {}
            }
            if (w->phase != PL_DEAD) {
                /* A jump's apex reaches the band's top, and nothing goes past it. */
                const float rise = (float)(w->y256 / PL_SUB) * 0.11f;
                const float hop = rise > 12.0f ? 12.0f : rise;

                rcBox(.wType = RC_PX(18), .hType = RC_PX(18),
                      .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                                    .element = RC_ANCHOR_TOP_LEFT,
                                    .offset = { head * (float)PL_WAVE_STEP - 9.0f, 13.0f - hop },
                                    .zIndex = Z_TITLEBAR + 1, .capture = RC_CAPTURE_PASSTHROUGH }) {
                    rcIconEmit(18, hero, draw_hero, &st->pose[POSE_TITLEBAR]);
                }
            }
        }
    }
}

/* CHROME, NOT CONTENT: the drag strip the user grabs is pinned at .titlebar.height,
   so a band that scaled with the content zoom would stop matching it. rcUnzoomed()
   counter-scales by 1/zoom. The band floats over the scene, so no sprite that
   rises past the top of the Stage can cover it, and an empty box holds its place.
   Every part but the wave has a width known before it is declared, the cluster's from
   rcWindowControlsWidth(), so the wave's share comes straight from the window and the
   controls stay on it through any resize. The cluster emits nothing on web and phones,
   where its width reads 0 and the wave takes that room too. The cluster's slabs meet
   the window's edge; without them the band's last part keeps the logo's gap from it,
   the gap the budget below already holds for the cluster. */
static void topbar(AppState *st, const PlPalette *p, RC_Insets safe, float windowW, RC_Color hero) {
    const bool roomy = windowW >= 760.0f;
    const RC_Color band = mix(p->skyTop, RC_BLACK, 0.45f);
    const float endGap = rcWindowControlsWidth() > 0.0f ? 0.0f : (float)PL_BAND_GAP;
    const float parts = safe.left + safe.right + PL_BAND_GAP + PL_LOGO_W + rcWindowControlsWidth()
                      + (roomy ? PL_NAME_W + PL_COURSE_W + PL_BAND_GAP * 4 : PL_BAND_GAP * 2);

    set_pose(st, POSE_LOGO, st->skin, 0);
    rcUnzoomed() {
        rcBox(.w = "grow", .hType = RC_PX(PL_TOPBAR_H)) {
            rcRow(.id = RC_ID_WINDOW_DRAG, .bg = band, .gap = PL_BAND_GAP,
                  .pl = (float)(PL_BAND_GAP + safe.left), .pr = safe.right + endGap,
                  .align = "cl", .wType = RC_PX(windowW), .hType = RC_PX(PL_TOPBAR_H),
                  .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                                .element = RC_ANCHOR_TOP_LEFT, .zIndex = Z_TITLEBAR,
                                .capture = RC_CAPTURE_ON }) {
                rcIconEmit(PL_LOGO_W, hero, draw_hero, &st->pose[POSE_LOGO]);
                if (roomy) {
                    rcBox(.wType = RC_PX(PL_NAME_W)) {
                        label("PULSE DASH", st->display, 16, RC_WHITE);
                    }
                }
                title_wave(st, p, hero, windowW - parts);
                if (roomy) {
                    rcBox(.align = "cr", .wType = RC_PX(PL_COURSE_W)) {
                        label(PL_LEVELS[st->world.level].name, 0, 14, p->muted);
                    }
                }
                rcWindowControls();
            }
        }
    }
}

/* -- shared chrome ------------------------------------------------------------- */

/* A round button of the course's color, lit by its own glow, that sinks a little
   when pressed. rcClicked on an id is what makes any box a button, hand cursor
   included. Its faces are mixed from its fill, the color its caller hands in, and
   no class spells a mix, so it reads its own states here; its glow takes the fill
   too and follows the same states. */
static bool round_button(const char *id, float size, RC_Color fill, RC_CustomDrawCallback draw,
                         const void *data, float iconSize, RC_Color iconColor) {
    const bool hover = rcShowsHover(id);
    const bool down = rcShowsActive(id);
    const RC_Color face = down ? mix(fill, RC_BLACK, 0.14f)
                               : hover ? mix(fill, RC_WHITE, 0.16f) : fill;

    rcBox(.id = id, .align = "cc", .borderRadius = "all-full",
          .wType = RC_PX(size), .hType = RC_PX(size),
          .gradient = { .from = mix(face, RC_WHITE, 0.18f), .to = face, .dir = "v" },
          .shadow = { .color = rcWithAlpha(fill, down ? 0.235f : 0.47f),
                      .blur = down ? 8.0f : 22.0f }) {
        rcIconEmit(iconSize, iconColor, draw, data);
    }
    return rcClicked(id);
}

/* A menu pill: an icon and a word. The main one is filled with the course's accent
   and glows. The others take the course's dark panel, as the level card does: a coin
   of the demo passing behind one must not show through its word. */
static bool pill_button(const char *id, const char *text, float h, bool primary, RC_Color fill,
                        RC_CustomDrawCallback draw, const void *data, RC_Color iconColor,
                        uint16_t font) {
    const bool hover = rcShowsHover(id);
    const bool down = rcShowsActive(id);
    const RC_Color ink = primary ? rcHex(C_HAZARD) : RC_WHITE;
    const RC_Color lit = down ? mix(fill, RC_BLACK, 0.14f)
                              : hover ? mix(fill, RC_WHITE, 0.16f) : fill;

    rcRow(.id = id, .bg = primary ? RC_UNSET_COLOR : rcWithAlpha(lit, 0.94f),
          .gap = (float)(h * 0.22f), .px = (float)(h * (primary ? 0.62f : 0.42f)),
          .align = "cc", .borderRadius = "all-full",
          .border = { .color = rcWithAlpha(RC_WHITE, (primary ? 90.0f : 70.0f) / 255.0f),
                      .width = 1 },
          .hType = RC_PX(h),
          .gradient = { .from = primary ? mix(lit, RC_WHITE, 0.2f) : RC_UNSET_COLOR,
                        .to = primary ? lit : RC_UNSET_COLOR, .dir = "v" },
          .shadow = { .color = primary ? rcWithAlpha(fill, (down ? 70.0f : 130.0f) / 255.0f)
                                       : rcWithAlpha(RC_BLACK, 0.235f),
                      .blur = primary ? 26.0f : 12.0f }) {
        rcIconEmit(h * 0.42f, primary ? ink : iconColor, draw, data);
        label(text, font, (uint16_t)(h * 0.36f), ink);
    }
    return rcClicked(id);
}

/* The logo: the face over a hard shadow of itself, the arcade way. */
static void logo_word(const char *word, uint16_t font, uint16_t size, RC_Color color,
                      RC_Color shade) {
    const float depth = (float)size * 0.06f;

    rcBox(.pl = (float)depth) {
        label(word, font, size, shade);
        rcBox(.floating = at(0.0f, -depth, Z_UI_TOP)) {
            label(word, font, size, color);
        }
    }
}

/* A key and what it does, for a player with a keyboard. */
static void key_hint(const char *key, const char *what, const PlPalette *p) {
    rcRow(.gap = 8, .align = "cl") {
        rcBox(.bg = rcWithAlpha(RC_WHITE, 0.1f), .px = 8, .py = 2, .borderRadius = "all-md",
              .border = { .color = rcWithAlpha(RC_WHITE, 0.275f), .width = 1 }) {
            rcTextC(key, .color = RC_WHITE, .size = 12, .select = RC_SELECT_NONE);
        }
        rcTextC(what, .color = p->muted, .size = 14, .select = RC_SELECT_NONE);
    }
}

/* Name, bar, figure: the same row in the level card and in the pause panel. */
static void progress_row(const char *name, const char *id, int32_t pct, char *buf, int cap,
                         const PlPalette *p) {
    pl_fmt(buf, cap, "", (uint32_t)pct, "%");
    rcRow(.gap = 12, .align = "cl", .w = "grow") {
        rcBox(.wType = RC_PX(72)) {
            caption(name, p->muted);
        }
        rcBox(.align = "cl", .w = "grow") {
            rcProgress(id, (float)pct / 100.0f);
        }
        rcBox(.align = "cr", .wType = RC_PX(44)) {
            rcTextC(buf, .color = RC_WHITE, .size = 14, .select = RC_SELECT_NONE);
        }
    }
}

static void stat_row(const char *name, const char *value, const PlPalette *p) {
    rcRow(.align = "cl", .w = "grow") {
        rcTextC(name, .color = p->muted, .size = 14, .select = RC_SELECT_NONE);
        rcSpacer() {}
        rcTextC(value, .color = RC_WHITE, .size = 14, .select = RC_SELECT_NONE);
    }
}

/* -- moving between screens ---------------------------------------------------- */

/* The menus run the selected course behind themselves with the demo player. */
static void show_demo(AppState *st) {
    pl_world_load(&st->world, st->selected, false);
    st->tickAcc = 0.0f;
    st->menuIdle = 0.0f;
}

static void go_levels(AppState *st) {
    st->screen = SCREEN_LEVELS;
    st->paused = false;
    st->wonOpen = false;
    show_demo(st);
}

static void pick_level(AppState *st, int level) {
    st->selected = (level + PL_LEVEL_COUNT) % PL_LEVEL_COUNT;
    show_demo(st);
}

static void begin_attempt(AppState *st) {
    st->record[st->selected].attempts++;
    st->recordDirty = true;
    st->newBest = false;
    st->pressedThisRun = false;
    st->jumpQueued = false;
    st->jumpHeld = false;
}

static void start_run(AppState *st) {
    pl_world_load(&st->world, st->selected, st->practice);
    st->screen = SCREEN_PLAY;
    st->paused = false;
    st->wonOpen = false;
    st->idleDeaths = 0;
    st->tickAcc = 0.0f;
    begin_attempt(st);
}

/* The next attempt: a practice run comes back to its checkpoint. */
static void retry_run(AppState *st) {
    pl_world_restart(&st->world);
    begin_attempt(st);
}

/* Restart means the top of the course, in practice too. A checkpoint is where a death
   returns to, and this is the way out of one the player has outgrown. */
static void restart_run(AppState *st) {
    st->world.hasCheckpoint = false;
    retry_run(st);
}

void platformer_show_title(AppState *st) {
    st->screen = SCREEN_TITLE;
    show_demo(st);
}

void platformer_pause(AppState *st) {
    if (st->screen == SCREEN_PLAY && st->world.phase != PL_WON)
        st->paused = true;
}

/* Draw continuously only while something is moving. A paused game, a settled panel
   and a menu left alone all sleep at about zero CPU until the next input arrives,
   and the titlebar's animation stops with them. */
bool platformer_wants_frames(const AppState *st) {
    if (st->screen != SCREEN_PLAY)
        return st->menuIdle < PL_MENU_IDLE_SECONDS;
    if (st->paused)
        return false;
    if (st->wonOpen)
        return st->world.phaseTicks < PL_WIN_TICKS + 120;
    return true;
}

/* -- the HUD ------------------------------------------------------------------- */

static void scene_hud(AppState *st, const PlView *v, const PlPalette *p, RC_Insets safe) {
    const PlWorld *w = &st->world;
    /* The narrowest stage gives the track's width to the score, and counts coins alone. */
    const bool tiny = v->stageW < 360.0f;
    const float trackW = v->stageW * (tiny ? 0.16f : v->stageW < 640.0f ? 0.26f : 0.30f);

    if (tiny)
        pl_fmt(st->coinStr, (int)sizeof st->coinStr, "", (uint32_t)w->coins, "");

    rcRow(.gap = 12, .pt = 12, .pl = (float)(PL_EDGE + safe.left),
          .pr = (float)(PL_EDGE + safe.right), .align = "tc", .wType = RC_PX(v->stageW),
          .floating = at(0.0f, 0.0f, Z_HUD)) {
        rcColumn(.w = "grow") {
            caption(w->practice ? "PRACTICE" : "SCORE", p->muted);
            label(w->scoreStr, st->display, 24, RC_WHITE);
        }
        rcRow(.gap = 8, .pt = 6, .align = "cc") {
            rcBox(.wType = RC_PX(trackW)) {
                rcProgress("RunProgress", pl_progress(w));
            }
            rcBox(.align = "cl", .wType = RC_PX(44)) {
                label(w->pctStr, st->display, 16, RC_WHITE);
            }
        }
        rcRow(.gap = 8, .align = "cr", .w = "grow") {
            coin_icon(20.0f, w->coins > 0);
            rcTextC(tiny ? st->coinStr : w->coinStr, .font = st->display, .color = RC_WHITE,
                    .size = 16, .wrap = "n", .select = RC_SELECT_NONE);
            rcBox(.pl = 4) {
                /* A clicked button keeps focus, and the panel gives focus back as it
                   closes, so Space, the jump, would press Pause again. A blur here,
                   before the panel opens, leaves it nothing to give back. */
                if (rcButton("PauseBtn", "", .leadingIcon = rcIconPause)) {
                    st->paused = true;
                    rcSetFocus(NULL);
                }
            }
        }
    }
}

/* The stamp a death leaves: how far, against the record, and what happens next. */
static void scene_death(AppState *st, const PlView *v, const PlPalette *p, bool finger) {
    const PlWorld *w = &st->world;
    const PlRecord *r = &st->record[st->selected];

    if (w->phase != PL_DEAD || w->phaseTicks < 6)
        return;
    rcColumn(.gap = 4, .align = "cc", .wType = RC_PX(v->stageW), .hType = RC_PX(v->groundY),
             .floating = at(0.0f, 0.0f, Z_UI)) {
        logo_word(w->pctStr, st->display, 56, RC_WHITE, rcWithAlpha(RC_BLACK, 0.59f));
        if (st->newBest) {
            rcBox(.bg = p->accent, .px = 12, .py = 4, .borderRadius = "all-full") {
                label("NEW BEST", st->display, 16, rcHex(C_HAZARD));
            }
        } else {
            pl_fmt(st->bestStr, (int)sizeof st->bestStr, "Best ",
                   (uint32_t)(w->practice ? r->practicePct : r->bestPct), "%");
            label(st->bestStr, st->display, 16, rcWithAlpha(RC_WHITE, 0.847f));
        }
        if (w->phaseTicks >= PL_DEATH_SKIP) {
            rcTextC(finger ? "Tap to go again" : "Press SPACE to go again", .color = p->muted,
                    .size = 14, .select = RC_SELECT_NONE);
        }
    }
}

/* -- the title screen ---------------------------------------------------------- */

static void screen_title(AppState *st, const PlView *v, const PlPalette *p, bool finger) {
    const bool narrow = v->stageW < 640.0f;
    const bool low = v->groundY < 320.0f;
    const uint16_t logoSize = low || narrow ? 56 : 96;
    const float playH = low ? 52.0f : narrow ? 60.0f : 68.0f;
    const float sideH = low ? 40.0f : 46.0f;
    int stars = 0, starTotal = 0, coins = 0, coinTotal = 0, attempts = 0;
    char outOf[8];

    for (int i = 0; i < PL_LEVEL_COUNT; i++) {
        stars += st->record[i].bestPct >= 100 ? PL_LEVELS[i].stars : 0;
        starTotal += PL_LEVELS[i].stars;
        attempts += st->record[i].attempts;
        coins += st->record[i].coins;
        coinTotal += st->courseCoins[i];
    }
    pl_fmt(outOf, (int)sizeof outOf, " / ", (uint32_t)starTotal, "");
    pl_fmt(st->statBuf[0], (int)sizeof st->statBuf[0], "", (uint32_t)stars, outOf);
    pl_fmt_coins(st->statBuf[1], (int)sizeof st->statBuf[1], (uint32_t)coins, (uint32_t)coinTotal);
    pl_fmt(st->statBuf[2], (int)sizeof st->statBuf[2], "", (uint32_t)attempts,
           attempts == 1 ? " attempt" : " attempts");

    /* A phone centers the menu above the runner's jump, which would pass behind its pills. */
    const float jump = (float)PL_JUMP_V * (float)PL_JUMP_V
                       / (float)(2 * PL_GRAVITY * PL_SUB) * v->s;

    rcColumn(.gap = (float)(low ? 8 : 16), .align = "cc", .wType = RC_PX(v->stageW),
             .hType = RC_PX(narrow && !low ? v->groundY - jump : v->groundY),
             .floating = at(0.0f, 0.0f, Z_UI)) {
        if (narrow) {
            rcColumn(.align = "tc") {
                logo_word("PULSE", st->display, logoSize, RC_WHITE, rcWithAlpha(RC_BLACK, 0.667f));
                logo_word("DASH", st->display, logoSize, p->accent, rcWithAlpha(RC_BLACK, 0.667f));
            }
        } else {
            rcRow(.gap = (float)(logoSize / 4), .align = "cc") {
                logo_word("PULSE", st->display, logoSize, RC_WHITE, rcWithAlpha(RC_BLACK, 0.667f));
                logo_word("DASH", st->display, logoSize, p->accent, rcWithAlpha(RC_BLACK, 0.667f));
            }
        }
        if (!low) {
            rcBox(.bg = rcWithAlpha(RC_BLACK, 0.376f), .px = 16, .py = 4,
                  .borderRadius = "all-full") {
                rcTextL("One button. Every beat.", .color = RC_WHITE, .size = 16,
                        .select = RC_SELECT_NONE);
            }
        }
        rcColumn(.gap = 12, .pt = (float)(low ? 0 : 12), .align = "tc") {
            if (pill_button("TitlePlay", "PLAY", playH, true, p->accent, draw_play, NULL,
                            RC_WHITE, st->display))
                start_run(st);
            rcRow(.gap = 12, .align = "cc") {
                if (pill_button("TitleLevels", "COURSES", sideH, false, p->panel, rcDrawIconList,
                                NULL, RC_WHITE, st->display))
                    go_levels(st);
                set_pose(st, POSE_TILE, st->skin, 0);
                if (pill_button("TitleStyle", "STYLE", sideH, false, p->panel, draw_hero,
                                &st->pose[POSE_TILE], rcHex(SKINS[st->skin].body), st->display))
                    st->skinsOpen = true;
            }
        }
    }
    rcRow(.gap = 24, .align = "cc", .wType = RC_PX(v->stageW),
          .hType = RC_PX(v->stageH - v->groundY), .floating = at(0.0f, v->groundY, Z_UI)) {
        rcRow(.gap = 8, .align = "cl") {
            rcIconStarFilled(16.0f, rcHex(C_COIN));
            rcTextC(st->statBuf[0], .color = RC_WHITE, .size = 14, .select = RC_SELECT_NONE);
        }
        rcRow(.gap = 8, .align = "cl") {
            coin_icon(16.0f, true);
            rcTextC(st->statBuf[1], .color = RC_WHITE, .size = 14, .select = RC_SELECT_NONE);
        }
        rcTextC(st->statBuf[2], .color = p->muted, .size = 14, .select = RC_SELECT_NONE);
        if (!finger && v->stageW >= 860.0f) {
            rcBox(.bg = rcWithAlpha(RC_WHITE, 0.157f), .wType = RC_PX(1), .hType = RC_PX(20)) {}
            key_hint("SPACE", "Play", p);
            key_hint("L", "Courses", p);
            key_hint("S", "Style", p);
        }
    }

    /* A pill above may already have left this screen in the same frame: its keys then act
       on nothing, or a click and a key would start two runs or open the style panel late. */
    if (st->skinsOpen || st->screen != SCREEN_TITLE)
        return;
    if (rcKeyPressed(RC_KEY_SPACE) || rcKeyPressed(RC_KEY_ENTER))
        start_run(st);
    else if (rcKeyPressed(RC_KEY_L))
        go_levels(st);
    else if (rcKeyPressed(RC_KEY_S))
        st->skinsOpen = true;
}

/* -- the level select ---------------------------------------------------------- */

static void level_card(AppState *st, const PlPalette *p, float width, bool low) {
    const PlWorld *w = &st->world;
    const PlLevelInfo *info = &PL_LEVELS[st->selected];
    const PlRecord *r = &st->record[st->selected];

    rcColumn(.bg = rcWithAlpha(p->panel, 0.94f), .gap = (float)(low ? 8 : 12),
             .p = (float)(low ? 12 : 24), .borderRadius = "all-2xl",
             .border = { .color = rcWithAlpha(p->accent, 0.47f), .width = 1 },
             .wType = RC_PX(width),
             .shadow = { .color = rcWithAlpha(RC_BLACK, 0.47f), .y = 16.0f, .blur = 40.0f }) {
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            rcBox(.bg = p->accent, .px = 8, .py = 2, .borderRadius = "all-full") {
                rcTextC(info->difficulty, .color = rcHex(C_HAZARD), .size = 12,
                        .letterSpacing = 1, .select = RC_SELECT_NONE);
            }
            rcTextC(info->speed == PL_SPEED_FAST ? "150 BPM" : "113 BPM", .color = p->muted,
                    .size = 12, .select = RC_SELECT_NONE);
            rcSpacer() {}
            /* The stars this course pays, hollow until they are won. */
            for (int i = 0; i < info->stars; i++) {
                if (r->bestPct >= 100)
                    rcIconStarFilled(18.0f, rcHex(C_COIN));
                else
                    rcIconStar(18.0f, rcHex(C_COIN));
            }
        }
        label(info->name, st->display, (uint16_t)(low ? 24 : width < 480.0f ? 28 : 40), RC_WHITE);
        if (!low) {
            rcTextC(info->blurb, .color = p->muted, .size = 14, .select = RC_SELECT_NONE);
            rcRow(.gap = 2, .align = "bc", .w = "grow", .hType = RC_PX(44)) {
                const int bars = (int)((width - 48.0f) / 5.0f);

                for (int i = 0; i < bars; i++) {
                    const float h = 4.0f + (float)w->wave[i * PL_WAVE_BARS / bars] / 255.0f * 40.0f;
                    const bool done = i * 100 < r->bestPct * bars;

                    rcBox(.bg = done ? p->accent : rcWithAlpha(RC_WHITE, 0.235f),
                          .borderRadius = "all-full", .wType = RC_PX(3), .hType = RC_PX(h)) {}
                }
            }
        }
        progress_row("NORMAL", "CardNormal", r->bestPct, st->statBuf[0],
                     (int)sizeof st->statBuf[0], p);
        progress_row("PRACTICE", "CardPractice", r->practicePct, st->statBuf[1],
                     (int)sizeof st->statBuf[1], p);
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            coin_icon(18.0f, r->coins > 0);
            pl_fmt_coins(st->coinStr, (int)sizeof st->coinStr, r->coins,
                         (uint32_t)st->courseCoins[st->selected]);
            rcTextC(st->coinStr, .color = p->muted, .size = 14, .select = RC_SELECT_NONE);
            rcSpacer() {}
            pl_fmt(st->bestStr, (int)sizeof st->bestStr, "Best score ", (uint32_t)r->bestScore, "");
            rcTextC(st->bestStr, .color = p->muted, .size = 14, .select = RC_SELECT_NONE);
        }
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            /* A clicked switch keeps focus and would take Space, which starts the run. */
            if (rcToggle("CardPracticeToggle", "Practice mode", &st->practice,
                         .className = "text-[14px] text-white"))
                rcSetFocus(NULL);
            rcSpacer() {}
            if (rcButton("CardPlay", "Play", .className = "ink", .variant = RC_BTN_PRIMARY,
                         .leadingIcon = rcIconPlay))
                start_run(st);
        }
    }
}

static void screen_levels(AppState *st, const PlView *v, const PlPalette *p, bool finger) {
    const bool narrow = v->stageW < 620.0f;
    const bool low = v->stageH < 480.0f;
    const float arrow = narrow ? 44.0f : 56.0f;
    const float dotHit = finger ? 44.0f : 24.0f;    /* the dot is small; its target is not */
    float cardW = v->stageW - (narrow ? 24.0f : 2.0f * (arrow + 32.0f));

    if (cardW > 620.0f)
        cardW = 620.0f;
    rcColumn(.gap = (float)(low ? 8 : 16), .pt = 8, .align = "cc", .wType = RC_PX(v->stageW),
             .hType = RC_PX(v->stageH), .floating = at(0.0f, 0.0f, Z_UI)) {
        rcRow(.gap = 16, .align = "cc") {
            if (!narrow && round_button("LevelPrev", arrow, p->accent, rcDrawIconChevronLeft, NULL,
                                        28.0f, rcHex(C_HAZARD)))
                pick_level(st, st->selected - 1);
            level_card(st, p, cardW, low);
            if (!narrow && round_button("LevelNext", arrow, p->accent, rcDrawIconChevronRight, NULL,
                                        28.0f, rcHex(C_HAZARD)))
                pick_level(st, st->selected + 1);
        }
        rcRow(.gap = 12, .align = "cc") {
            if (narrow && round_button("LevelPrev", arrow, p->accent, rcDrawIconChevronLeft, NULL,
                                       24.0f, rcHex(C_HAZARD)))
                pick_level(st, st->selected - 1);
            rcRow(.align = "cc") {
                for (int i = 0; i < PL_LEVEL_COUNT; i++) {
                    const bool on = i == st->selected;
                    /* The dot lights while the pointer is on its target, the box around it,
                       and a class reads only its own box's state. */
                    const uint8_t rest = rcShowsHover(DOT_IDS[i]) ? 170 : 90;

                    rcBox(.id = DOT_IDS[i], .align = "cc", .wType = RC_PX(dotHit),
                          .hType = RC_PX(dotHit)) {
                        rcBox(.bg = on ? RC_WHITE : rcWithAlpha(RC_WHITE, rest / 255.0f),
                              .borderRadius = "all-full", .wType = RC_PX(on ? 14 : 10),
                              .hType = RC_PX(on ? 14 : 10)) {}
                    }
                    if (rcClicked(DOT_IDS[i]))
                        pick_level(st, i);
                }
            }
            if (narrow && round_button("LevelNext", arrow, p->accent, rcDrawIconChevronRight, NULL,
                                       24.0f, rcHex(C_HAZARD)))
                pick_level(st, st->selected + 1);
        }
    }
    rcBox(.floating = at(PL_EDGE, 12.0f, Z_UI_TOP)) {
        if (rcButton("LevelsBack", "Back", .leadingIcon = rcIconArrowLeft))
            platformer_show_title(st);
    }

    if (st->screen != SCREEN_LEVELS)    /* Play or Back above already left this screen */
        return;
    if (rcKeyPressed(RC_KEY_LEFT))
        pick_level(st, st->selected - 1);
    else if (rcKeyPressed(RC_KEY_RIGHT))
        pick_level(st, st->selected + 1);
    else if (rcKeyPressed(RC_KEY_ENTER) || rcKeyPressed(RC_KEY_SPACE))
        start_run(st);
    else if (rcKeyPressed(RC_KEY_ESCAPE))
        platformer_show_title(st);
}

/* -- the dialogs --------------------------------------------------------------- */

/* A dialog focuses its first stop on the frame it opens, and a focused control takes
   Space (a button Enter too). So in that frame a panel focuses the button those keys
   mean here instead; platformer_layout clears the flag once no panel is up. */
static void focus_default(AppState *st, const char *id) {
    if (!st->panelFocused) {
        st->panelFocused = true;
        rcSetFocus(id);
    }
}

/* A narrow stage's panel is the stage less 64 px, 296 at the least window. The pause panel's
   buttons need 348 px with their icons, so a narrow panel's buttons carry their words alone. */
static void modal_pause(AppState *st, const PlView *v, const PlPalette *p) {
    const PlRecord *r = &st->record[st->selected];
    const bool narrow = v->stageW < 440.0f;
    const float width = narrow ? v->stageW - 64.0f : 376.0f;

    if (!rcBeginModal("PauseModal", &st->paused, .noBackdropDismiss = true))
        return;
    focus_default(st, "PauseResume");   /* the first stop is the Practice switch */
    rcColumn(.gap = 12, .wType = RC_PX(width)) {
        label("PAUSED", st->display, 28, RC_WHITE);
        rcTextC(PL_LEVELS[st->selected].name, .color = p->muted, .size = 14,
                .select = RC_SELECT_NONE);
        progress_row("NORMAL", "PauseNormal", r->bestPct, st->statBuf[0],
                     (int)sizeof st->statBuf[0], p);
        progress_row("PRACTICE", "PausePractice", r->practicePct, st->statBuf[1],
                     (int)sizeof st->statBuf[1], p);
        if (rcToggle("PausePracticeToggle", "Practice mode", &st->practice,
                     .className = "text-[14px] text-white")) {
            rcSetFocus(NULL);   /* a clicked switch keeps focus, and Space means Resume */
            st->world.practice = st->practice;
            st->world.hasCheckpoint = false;
            st->world.checkpointBar = st->world.scrollX / (PL_BAR_CELLS * PL_CELL);
            if (!st->practice)
                retry_run(st);
        }
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            if (rcButton("PauseLevels", "Courses", .leadingIcon = narrow ? NULL : rcIconList))
                go_levels(st);
            if (rcButton("PauseRestart", "Restart",
                         .leadingIcon = narrow ? NULL : rcIconRotateCw)) {
                restart_run(st);
                st->paused = false;
            }
            rcSpacer() {}
            if (rcButton("PauseResume", "Resume", .className = "ink", .variant = RC_BTN_PRIMARY,
                         .leadingIcon = narrow ? NULL : rcIconPlay))
                st->paused = false;
        }
    }
    rcEndModal();

    /* A button above already acted and closed the panel: R in the same frame would restart
       on top of it and count a second attempt. */
    if (!st->paused)
        return;
    if (rcKeyPressed(RC_KEY_R)) {
        restart_run(st);
        st->paused = false;
    } else if (rcKeyPressed(RC_KEY_SPACE) || rcKeyPressed(RC_KEY_ENTER)) {
        st->paused = false;
    }
}

static void modal_won(AppState *st, const PlView *v, const PlPalette *p) {
    const PlWorld *w = &st->world;
    const PlLevelInfo *info = &PL_LEVELS[st->selected];
    const bool narrow = v->stageW < 440.0f;   /* as modal_pause's */
    const float width = narrow ? v->stageW - 64.0f : 376.0f;
    const bool lastLevel = st->selected == PL_LEVEL_COUNT - 1;

    if (!rcBeginModal("WonModal", &st->wonOpen, .noBackdropDismiss = true))
        return;
    focus_default(st, lastLevel ? "WonReplay" : "WonNext");   /* the first stop is Courses */
    pl_fmt(st->statBuf[0], (int)sizeof st->statBuf[0], "", (uint32_t)w->attempt, "");
    pl_fmt(st->statBuf[2], (int)sizeof st->statBuf[2], "", (uint32_t)w->jumps, "");
    rcColumn(.gap = 12, .align = "tc", .wType = RC_PX(width)) {
        label(w->practice ? "PRACTICE DONE" : "COURSE COMPLETE", st->display, 28, p->accent);
        rcTextC(info->name, .color = p->muted, .size = 14, .select = RC_SELECT_NONE);
        /* Only what this win paid: an empty star would read as a poor rating. */
        if (!w->practice) {
            rcRow(.gap = 8, .align = "cc") {
                for (int i = 0; i < info->stars; i++)
                    rcIconStarFilled(32.0f, rcHex(C_COIN));
            }
        }
        rcColumn(.gap = 8, .w = "grow") {
            stat_row("Score", w->scoreStr, p);
            stat_row("Attempts", st->statBuf[0], p);
            stat_row("Jumps", st->statBuf[2], p);
            stat_row("Time", w->timeStr, p);
            stat_row("Coins", w->coinStr, p);
        }
        if (st->newBest) {
            rcBox(.bg = p->accent, .px = 12, .py = 4, .borderRadius = "all-full") {
                label("NEW BEST", st->display, 16, rcHex(C_HAZARD));
            }
        }
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            if (rcButton("WonLevels", "Courses", .leadingIcon = narrow ? NULL : rcIconList))
                go_levels(st);
            if (rcButton("WonReplay", "Replay", .leadingIcon = narrow ? NULL : rcIconRotateCw))
                start_run(st);
            rcSpacer() {}
            if (rcButton("WonNext", "Next", .disabled = lastLevel, .className = "ink",
                         .variant = RC_BTN_PRIMARY)) {
                st->selected++;
                start_run(st);
            }
        }
    }
    rcEndModal();

    /* A button above already acted and closed the panel. Next and a key in one frame would
       otherwise step twice, from the second course to a fourth that does not exist. */
    if (!st->wonOpen)
        return;
    if (rcKeyPressed(RC_KEY_R)) {
        start_run(st);
    } else if ((rcKeyPressed(RC_KEY_SPACE) || rcKeyPressed(RC_KEY_ENTER)) && !lastLevel) {
        st->selected++;
        start_run(st);
    }
}

static void modal_skins(AppState *st, const PlPalette *p) {
    if (!rcBeginModal("SkinsModal", &st->skinsOpen))
        return;
    rcColumn(.gap = 12, .wType = RC_PX(304)) {     /* three tiles and their two gaps */
        label("PICK A STYLE", st->display, 24, RC_WHITE);
        rcRow(.gap = 8, .w = "grow", .className = "flex-wrap") {
            for (int i = 0; i < PL_SKIN_COUNT; i++) {
                const bool on = i == st->skin;

                set_pose(st, POSE_SKIN0 + i, i, 0);
                rcColumn(.id = SKIN_IDS[i], .gap = 4, .py = 8, .align = "tc", .wType = RC_PX(96),
                         .className = on ? "tile border-primary bg-white/12" : "tile") {
                    rcIconEmit(56.0f, rcHex(SKINS[i].body), draw_hero, &st->pose[POSE_SKIN0 + i]);
                    rcTextC(SKINS[i].name, .color = on ? RC_WHITE : p->muted, .size = 14,
                            .select = RC_SELECT_NONE);
                }
                if (rcClicked(SKIN_IDS[i])) {
                    st->skin = i;
                    st->recordDirty = true;
                }
            }
        }
        rcRow(.align = "cr", .w = "grow") {
            if (rcButton("SkinsDone", "Done", .className = "ink", .variant = RC_BTN_PRIMARY))
                st->skinsOpen = false;
        }
    }
    rcEndModal();
}

/* -- input during a run -------------------------------------------------------- */

/* A press anywhere on the playfield is the whole game. It reads rcPressed, the
   press edge, because a jump's latency is the point and it destroys nothing. */
static void read_play_input(AppState *st) {
    const bool overPause = rcIsHovered("PauseBtn");
    const bool keyDown = rcKeyDown(RC_KEY_SPACE) || rcKeyDown(RC_KEY_UP) || rcKeyDown(RC_KEY_W);

    if ((rcPressed("Stage") && !overPause) || rcKeyPressed(RC_KEY_SPACE)
        || rcKeyPressed(RC_KEY_UP) || rcKeyPressed(RC_KEY_W)) {
        st->jumpQueued = true;
        st->jumpHeld = true;
        st->pressedThisRun = true;
        st->idleDeaths = 0;
    } else if (!keyDown && !rcPointerDown(RC_POINTER_LEFT)) {
        st->jumpHeld = false;
    }
    /* Polling a whole playfield would make the pointer a hand over every pixel of
       it, which a game surface should not be. An explicit set wins over the poll. */
    if (!overPause)
        rcSetCursor(RC_CURSOR_DEFAULT);
    if (rcKeyPressed(RC_KEY_ESCAPE) || rcKeyPressed(RC_KEY_P))
        st->paused = true;
    else if (rcKeyPressed(RC_KEY_R))
        restart_run(st);
}

/* -- the four-function contract ------------------------------------------------ */

void platformer_seed(AppState *st, unsigned seed) {
    (void)seed;     /* every course is authored, so a run has no randomness to seed */
    *st = RC_LIT(AppState){ 0 };
    st->seeded = true;
    st->screen = SCREEN_PLAY;
    /* The looks that repeat, named once at start-up, never per frame: the dark ink an
       accent button writes in (C_HAZARD), and a style tile, which lights under the
       pointer and takes the course's accent as its edge once picked. */
    rcDefineClass("ink", "text-[#06061a]");
    rcDefineClass("tile", "rounded-lg border-2 border-white/10 bg-white/3 hover:bg-white/6");
    for (int i = 0; i < PL_LEVEL_COUNT; i++)
        st->courseCoins[i] = pl_course_coins(i);
    pl_world_load(&st->world, 0, false);
}

/* What a run does when it ends, on the tick it ends. */
static void run_ended(AppState *st, PlPhase phase) {
    const PlWorld *w = &st->world;
    PlRecord *r = &st->record[st->selected];
    const int32_t pct = phase == PL_WON ? 100 : pl_percent(w);

    st->recordDirty = true;
    if (w->practice) {
        st->newBest = pct > r->practicePct;
        if (st->newBest)
            r->practicePct = pct;
    } else {
        st->newBest = pct > r->bestPct;
        if (st->newBest)
            r->bestPct = pct;
        if (phase == PL_WON) {
            if (w->coins > r->coins)
                r->coins = w->coins;
            if (w->score > r->bestScore) {
                r->bestScore = w->score;
                st->newBest = true;
            }
        }
    }
    if (phase == PL_DEAD)
        st->idleDeaths = st->pressedThisRun ? 0 : st->idleDeaths + 1;
}

/* One fixed tick of the run the player is in. */
static void tick_run(AppState *st) {
    PlWorld *w = &st->world;
    const PlPhase before = w->phase;
    const bool press = st->jumpQueued;

    st->jumpQueued = false;
    pl_world_step(w, st->jumpHeld, press);
    if (before == PL_RUN && w->phase != PL_RUN)
        run_ended(st, w->phase);
    if (w->phase == PL_DEAD) {
        const bool skip = press && w->phaseTicks >= PL_DEATH_SKIP;

        if (skip || w->phaseTicks >= PL_DEATH_TICKS) {
            /* Two attempts in a row with no press at all: nobody is playing, so
               stop and let the window sleep instead of dying for ever. */
            if (st->idleDeaths >= 2) {
                st->paused = true;
                st->idleDeaths = 0;
            }
            retry_run(st);
        }
    } else if (w->phase == PL_WON && w->phaseTicks == PL_WIN_TICKS) {
        st->wonOpen = true;
    }
}

/* One fixed tick of the demo behind a menu. */
static void tick_demo(AppState *st) {
    PlWorld *w = &st->world;

    pl_world_step(w, false, w->phase == PL_RUN && pl_bot_wants_press(w));
    if (w->phaseTicks >= 40 && w->phase != PL_RUN) {
        if (w->phase == PL_WON)
            pl_world_load(w, st->selected, false);
        else
            pl_world_restart(w);
    }
}

/* The sim runs at PL_TICK_HZ however fast the display draws: every frame banks its
   dt and spends it in whole ticks, so a 144 Hz monitor plays at the speed a 60 Hz
   one does, and a frame with no tick due keeps its queued press for the next. A
   frame of exactly PL_TICK_DT, the scripted run's dt, is one tick. The bank is
   capped, so a stall comes back as a pause rather than a sprint. */
#define PL_TICK_DT   (1.0f / PL_TICK_HZ)
#define PL_MAX_BANK  0.25f

void platformer_update(AppState *st, const AppCtx *ctx) {
    if (ctx->dt <= 0.0f)                    /* dt <= 0 is a STRICT no-op */
        return;
    /* A paused run banks nothing, or Resume would lurch forward by the whole bank. Nor does
       it keep a press or a hold: read_play_input, which lets go of a hold, does not run under
       the panel, and a key held as focus left is let go in another window, so the first
       tick after Resume would jump on its own. */
    if (st->screen == SCREEN_PLAY && st->paused) {
        st->tickAcc = 0.0f;
        st->jumpQueued = false;
        st->jumpHeld = false;
        return;
    }
    /* Only input that acts on a menu restarts its demo: a passing pointer, a focus change or
       a repaint finds a parked menu still, and a sleeping menu banks no time. */
    RC_KeyEvent key;
    if (rcPointerPressed(RC_POINTER_LEFT) || rcWheelDelta().y != 0.0f || rcKeyEvent(0, &key))
        st->menuIdle = 0.0f;
    else if (st->screen != SCREEN_PLAY && st->menuIdle >= PL_MENU_IDLE_SECONDS)
        return;
    st->menuIdle += ctx->dt;
    st->tickAcc += ctx->dt;
    if (st->tickAcc > PL_MAX_BANK)
        st->tickAcc = PL_MAX_BANK;
    while (st->tickAcc >= PL_TICK_DT) {
        if (st->screen != SCREEN_PLAY)
            tick_demo(st);
        else if (!st->paused)
            tick_run(st);
        st->tickAcc -= PL_TICK_DT;
    }
}

void platformer_layout(AppState *st, const AppCtx *ctx) {
    const PlPalette p = platformer_palette(&st->world);
    const RC_Color hero = rcHex(SKINS[st->skin].body);
    /* A game is full-bleed: Root spends only the TOP safe band, and the Stage runs
       under the side and bottom ones while the chrome insets itself instead. */
    const RC_Insets safe = app_safe(ctx);
    const float zoom = ctx->zoom > 0.0f ? ctx->zoom : 1.0f;
    const float stageW = app_view_w(ctx);
    /* The topbar holds PL_TOPBAR_H window px under any zoom, which is 1/zoom as many
       layout px. */
    const float stageH = app_view_h(ctx) - safe.top - (float)PL_TOPBAR_H / zoom;
    const bool live = st->screen == SCREEN_PLAY;
    const PlView v = view_for(&st->world, stageW, stageH, live);
    const bool finger = ctx->view.coarsePointer;
    const PlScreen openedOn = st->screen;

    rcSetStyle(platformer_style(&p));
    rcColumn(.id = "Root", .bg = p.skyTop, .pt = safe.top, .w = "grow", .h = "grow") {
        topbar(st, &p, ctx->safe, stageW * zoom, hero);   /* window px: the bar is unzoomed */
        rcBox(.id = "Stage", .w = "grow", .h = "grow",
              .gradient = { .from = p.skyTop, .to = p.skyBot, .dir = "v" }) {
            scene_haze(&v, &p);
            scene_sun(&st->world, &v, &p, live);
            scene_columns(&v, &p, Z_FAR, 44.0f, 6.0f, 0.18f, 0.62f,
                          mix(p.pillar, p.skyBot, 0.45f), 0x51ED270Bu);
            scene_columns(&v, &p, Z_NEAR, 76.0f, 3.0f, 0.10f, 0.38f,
                          mix(p.pillar, p.groundBot, 0.55f), 0x7FEB352Du);
            scene_motes(&st->world, &v, &p);
            scene_ground(&v, &p);
            scene_course(&st->world, &v, &p, live);
            scene_gate(&st->world, &v, &p);
            scene_checkpoint(&st->world, &v, &p);
            scene_particles(&st->world, &v, &p, hero, live);
            scene_hero(st, &v, hero);
            if (live)
                scene_flash(&st->world, &v);
            switch (st->screen) {
            case SCREEN_TITLE:
                screen_title(st, &v, &p, finger);
                break;
            case SCREEN_LEVELS:
                screen_levels(st, &v, &p, finger);
                break;
            case SCREEN_PLAY:
                scene_hud(st, &v, &p, safe);
                scene_death(st, &v, &p, finger);
                break;
            }
        }
    }

    if (st->screen == SCREEN_PLAY && st->paused)
        modal_pause(st, &v, &p);
    else if (st->screen == SCREEN_PLAY && st->wonOpen)
        modal_won(st, &v, &p);
    else if (openedOn == SCREEN_PLAY && st->screen == SCREEN_PLAY && st->world.phase != PL_WON)
        read_play_input(st);
    /* Esc closed the panel, which leaves the run: back to the level list. */
    if (st->screen == SCREEN_PLAY && st->world.phase == PL_WON && !st->wonOpen
        && st->world.phaseTicks > PL_WIN_TICKS)
        go_levels(st);
    if (st->screen == SCREEN_TITLE && st->skinsOpen)
        modal_skins(st, &p);
    if (!st->paused && !st->wonOpen)    /* the next panel to open focuses its button again */
        st->panelFocused = false;
}

void platformer_bench_step(AppState *st, const AppInputSink *in, int frame) {
    (void)st;   /* every action this app has is synthetic input */
    /* The scripted scenario: press three times on the empty runway, then hold still,
       so a double-rendered frame is identical. The hold needs the hero GROUNDED with
       no press queued, and a jump lasts 32 ticks, so they all land well before it.
       The middle press finds the hero in the air and changes nothing, on purpose. */
    if (!in || frame >= PLATFORMER_BENCH_WARMUP)
        return;                                        /* the HOLD */

    if (frame == 4 || frame == 20 || frame == 40) {
        in->move(in->ctx, 640.0f, 360.0f);             /* anywhere on the playfield */
        in->button(in->ctx, APP_MBTN_LEFT, true);      /* press edge -> queue a jump */
    } else if (frame == 5 || frame == 21 || frame == 41) {
        /* The Stage reads the press edge, so this release fires nothing - and it
           stays exactly for that reason: dropping it would leave the left button
           held down for the rest of the run. */
        in->button(in->ctx, APP_MBTN_LEFT, false);
    } else if (frame == PLATFORMER_BENCH_WARMUP - 2) {
        in->move(in->ctx, -100.0f, -100.0f);           /* park OFF-canvas before the hold */
    }
}
