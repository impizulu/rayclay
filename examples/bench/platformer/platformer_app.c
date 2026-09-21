/*
    platformer_app.c - the runner's screen, drawn with nothing but the public API.

    Shows: a FLOATING SCENE (every sprite is a floating child of one Stage box,
    placed at an integer pixel offset and marked RC_CAPTURE_PASSTHROUGH so a tap
    falls through to the playfield beneath it), explicit z bands, a custom
    titlebar, and one design space mapped onto whatever window the user gives.

    Every sprite is an rcBox rect: no image, no asset, no custom draw callback.
    Run it with the rayclay_bench_platformer target.
*/
#define PLATFORMER_BACKEND_IMPLEMENTATION
#include "platformer_app.h"

/* Warmup frames before the scripted scenario holds still. */
#define PLATFORMER_BENCH_WARMUP 128

/* Explicit z per layer, so the interactive controls are always topmost for the
   hit test no matter what order the sprites are emitted in. */
#define Z_SKY        5
#define Z_FAR       10
#define Z_MID       20
#define Z_TERRAIN   30
#define Z_BEDROCK   32
#define Z_ROCK      34
#define Z_TUFT      36
#define Z_SHADOW    38
#define Z_COIN      40
#define Z_HERO      50
#define Z_PARTICLE  60
#define Z_FORE      70
#define Z_HUD       90
#define Z_JUMP     100

/* The scene palette. The game has its own twilight look rather than the RC_Style
   chrome theme, which still dresses the titlebar and the HUD text. */
#define C_SKY_TOP    0x0f1836
#define C_SKY_BOT    0x5c3b6e
#define C_GRASS      0x46b45f
#define C_GRASS_DK   0x2c7a3f
#define C_DIRT       0x5a3f2a
#define C_SOIL       0x503a2a
#define C_ROCK       0x33251c
#define C_BEDROCK    0x221811
#define C_PEBBLE     0x6a5140
#define C_FORE       0x160f0b
#define C_COIN       0xfacc15
#define C_COIN_HI    0xfde68a
#define C_HERO_BODY  0x22d3ee
#define C_HERO_HEAD  0xfef3c7
#define C_HERO_LEG   0x0e7490
#define C_DUST       0xcbb89a
#define C_GOAL_POLE  0xe2e8f0
#define C_GOAL_FLAG  0xf43f5e

/* The chrome's geometry, in Stage px and never scaled. The JUMP button is
   thumb-sized for a finger and a smaller, quieter affordance for a mouse, which
   has SPACE and the whole playfield to click on instead. */
#define PL_JUMP_D      116
#define PL_JUMP_D_FINE  72
#define PL_JUMP_INSET   28
#define PL_HINT_INSET   28
#define PL_HINT_GAP     12

/* A hint centred at the foot runs under the JUMP button once the Stage is
   narrower than this, so below it the hint lifts above the button instead. Both
   phones in portrait are under the line; every landscape arm is over it. */
#define PL_HINT_W      190
#define PL_HINT_CENTRED_MIN_W  (PL_HINT_W + 2 * (PL_JUMP_INSET + PL_JUMP_D + PL_HINT_GAP))

/* Ground detail: the deepest stratum's thickness, and the world-x spacing of the
   scattered rocks and of the foreground ridge. */
#define PL_BEDROCK_H       56
#define PL_PEBBLE_STRIDE   74
#define PL_FORE_STRIDE    112

/* The world is authored in PL_DESIGN_W x PL_DESIGN_H and mapped onto the live
   Stage by ONE uniform scale. A phone held sideways is SHORTER than the design, so
   the scene scales down to fit; anything taller keeps scale 1 and spends the extra
   height on SKY, by pinning the design space to the FOOT of the Stage - growing
   the ground instead would make the dullest part of the picture the biggest.
   Sprites map their EDGES on the way out, so what tiles still tiles at any scale. */
typedef struct {
    float s;        /* design px -> Stage px; 1 at the design height and above */
    int   sw;       /* the visible width in DESIGN units (the cull bound)      */
    int   oy;       /* Stage px of sky above design y 0                        */
    int   stageW;   /* the Stage's own size, in Stage px                       */
    int   stageH;
} PlView;

/* A design-space LENGTH in Stage px, and a design-space Y on the Stage. */
static int view_len(const PlView *v, int design) { return (int)((float)design * v->s); }
static int view_y  (const PlView *v, int design) { return view_len(v, design) + v->oy; }

typedef struct { int x, y, w, h; } PlRect;

static PlRect view_rect(const PlView *v, int x, int y, int w, int h) {
    PlRect r;
    r.x = view_len(v, x);
    r.y = view_y(v, y);
    r.w = view_len(v, x + w) - r.x;
    r.h = view_y(v, y + h) - r.y;
    return r;
}

/* A decorative scene sprite: attached to the Stage at an integer pixel offset, on
   the given z band, PASSTHROUGH so it never eats the tap. */
static RC_Float scene_float(int sx, int sy, int z) {
    return RC_LIT(RC_Float){ .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                             .element = RC_ANCHOR_TOP_LEFT, .offset = { (float)sx, (float)sy },
                             .zIndex = (int16_t)z, .capture = RC_CAPTURE_PASSTHROUGH };
}

/* The sky: one moon at 1/16 the scroll speed over a field of stars at 1/8. The
   slowest things on screen, and the reason the top of the stage is a place. */
static void scene_sky(const PlWorld *w, const PlView *v) {
    int mx = pl_parallax_x(w->moon.x, w->scrollX, 1, 16);

    if (pl_visible(mx, w->moon.w, 120, v->sw)) {
        PlRect r = view_rect(v, mx, 58, w->moon.w, w->moon.h);
        rcBox(.bg = rcAlpha(rcHex(w->moon.tint), 225), .borderRadius = "all-full",
              .wType = RC_PX(r.w), .hType = RC_PX(r.h),
              .floating = scene_float(r.x, r.y, Z_SKY)) {}
    }
    for (int i = 0; i < w->starCount; i++) {
        int sx = pl_parallax_x(w->star[i].x, w->scrollX, 1, 8);
        if (!pl_visible(sx, w->star[i].w, 20, v->sw))
            continue;
        PlRect r = view_rect(v, sx, pl_star_y(i), w->star[i].w, w->star[i].h);
        /* a star under a pixel after the scale is no star at all, so the floor is
           1: the sky thins on a phone instead of emptying */
        if (r.w < 1) r.w = 1;
        if (r.h < 1) r.h = 1;
        rcBox(.bg = rcHex(w->star[i].tint), .borderRadius = "all-full",
              .wType = RC_PX(r.w), .hType = RC_PX(r.h),
              .floating = scene_float(r.x, r.y, Z_SKY)) {}
    }
}

/* The finish: a pole and a pennant at the end of the authored course. A runner
   with no visible goal is a treadmill. Past it the flat tail carries on, which is
   why the goal is drawn rather than enforced. */
static void scene_goal(const PlWorld *w, const PlView *v) {
    int sx = pl_ground_x(w, PL_LEVEL_LEN);

    if (!pl_visible(sx, 60, 80, v->sw))
        return;
    {
        int top = pl_ground_top(w, PL_LEVEL_LEN) - 150;
        PlRect pole = view_rect(v, sx, top, 6, 150);
        PlRect flag = view_rect(v, sx + 6, top + 8, 54, 34);

        rcBox(.bg = rcHex(C_GOAL_POLE), .wType = RC_PX(pole.w), .hType = RC_PX(pole.h),
              .floating = scene_float(pole.x, pole.y, Z_TUFT)) {}
        rcBox(.bg = rcHex(C_GOAL_FLAG), .align = "cc", .borderRadius = "r-md",
              .wType = RC_PX(flag.w), .hType = RC_PX(flag.h),
              .floating = scene_float(flag.x, flag.y, Z_TUFT)) {
            rcTextL("END", .font = F_SMALL, .color = RC_WHITE);
        }
    }
}

/* Far parallax: low hills that drift at 1/4 the scroll speed. */
static void scene_parallax_far(const PlWorld *w, const PlView *v) {
    for (int i = 0; i < w->hillCount; i++) {
        int sx = pl_parallax_x(w->hill[i].x, w->scrollX, 1, 4);
        if (!pl_visible(sx, w->hill[i].w, 460, v->sw))
            continue;
        int sy = PL_GROUND_LO - w->hill[i].h;      /* rise from the horizon line */
        PlRect r = view_rect(v, sx, sy, w->hill[i].w, w->hill[i].h);
        /* a domed top is what makes a rectangle read as a hill rather than a wall */
        rcBox(.bg = rcHex(w->hill[i].tint), .borderRadius = "t-full",
              .wType = RC_PX(r.w), .hType = RC_PX(r.h),
              .floating = scene_float(r.x, r.y, Z_FAR)) {}
    }
}

/* Mid parallax: building silhouettes that drift at 1/2 the scroll speed. */
static void scene_parallax_mid(const PlWorld *w, const PlView *v) {
    for (int i = 0; i < w->bldgCount; i++) {
        int sx = pl_parallax_x(w->bldg[i].x, w->scrollX, 1, 2);
        if (!pl_visible(sx, w->bldg[i].w, 160, v->sw))
            continue;
        int sy = PL_GROUND_LO - w->bldg[i].h;
        PlRect r = view_rect(v, sx, sy, w->bldg[i].w, w->bldg[i].h);
        rcBox(.bg = rcHex(w->bldg[i].tint), .borderRadius = "all-sm",
              .wType = RC_PX(r.w), .hType = RC_PX(r.h),
              .floating = scene_float(r.x, r.y, Z_MID)) {}
    }
}

/* The terrain heightfield: each visible segment is a column of STRATA under its
   grass cap - turf, topsoil, subsoil, then rock to the foot of the window. The
   bands are the point. One flat brown from the grass line down is the largest
   shape on the screen and the one that says nothing at all. */
static void scene_terrain(const PlWorld *w, const PlView *v) {
    for (int i = 0; i < w->segCount; i++) {
        bool tail = (i == w->segCount - 1);
        int x0 = pl_ground_x(w, w->seg[i].startX);
        /* the last segment runs on to the window edge: the course ends at
           PL_LEVEL_LEN and the run does not, so the ground must not either */
        int x1 = tail ? v->sw + 8 : pl_ground_x(w, w->seg[i].endX);
        if (x1 < -8 || x0 > v->sw + 8)
            continue;
        if (x0 < -8)        x0 = -8;
        if (x1 > v->sw + 8) x1 = v->sw + 8;
        int wpx = x1 - x0;
        if (wpx < 1)
            continue;
        PlRect r = view_rect(v, x0, w->seg[i].topY, wpx, 0);
        rcBox(.bg = rcHex(C_ROCK), .wType = RC_PX(r.w), .hType = RC_PX(v->stageH - r.y),
              .floating = scene_float(r.x, r.y, Z_TERRAIN)) {
            rcBox(.bg = rcHex(C_GRASS),    .w = "grow", .hType = RC_PX(view_len(v, 6))) {}
            rcBox(.bg = rcHex(C_GRASS_DK), .w = "grow", .hType = RC_PX(view_len(v, 3))) {}
            rcBox(.bg = rcHex(C_DIRT),     .w = "grow", .hType = RC_PX(view_len(v, 26))) {}
            rcBox(.bg = rcHex(C_SOIL),     .w = "grow", .hType = RC_PX(view_len(v, 44))) {}
        }
    }
}

/* The deepest stratum, pinned across the foot of the Stage. Ground with no
   visible bottom is a wall. */
static void scene_bedrock(const PlView *v) {
    int h = view_len(v, PL_BEDROCK_H);

    if (h < 1)
        return;
    rcBox(.bg = rcHex(C_BEDROCK), .wType = RC_PX(v->stageW), .hType = RC_PX(h),
          .floating = scene_float(0, v->stageH - h, Z_BEDROCK)) {}
}

/* Rocks embedded in the ground, following the terrain's contour so the strata
   read as one solid body rather than as bands painted on a backdrop. Their world
   x comes off the index, so the ground is detailed for the whole run at no
   per-rock memory - only the visible span of indices is walked. */
static void scene_rocks(const PlWorld *w, const PlView *v) {
    int32_t i0 = (w->scrollX - 200) / PL_PEBBLE_STRIDE;
    int32_t i1 = (w->scrollX + v->sw + 200) / PL_PEBBLE_STRIDE;

    if (i0 < 0) i0 = 0;
    for (int32_t i = i0; i <= i1; i++) {
        int32_t wx = i * PL_PEBBLE_STRIDE + (int32_t)pl_scatter((uint32_t)i, 0x2545F491u, 76u);
        int sx = pl_ground_x(w, wx);
        int sw_ = 9 + (int)pl_scatter((uint32_t)i, 0x85EBCA6Bu, 18u);
        int sh  = 5 + (int)pl_scatter((uint32_t)i, 0x27D4EB2Fu, 8u);
        int sy  = pl_ground_top(w, wx) + 44 + (int)pl_scatter((uint32_t)i, 0xC2B2AE35u, 62u);
        if (!pl_visible(sx, sw_, 40, v->sw) || sy + sh > PL_DESIGN_H - 6)
            continue;
        {
            PlRect r = view_rect(v, sx, sy, sw_, sh);
            if (r.w < 1) r.w = 1;
            if (r.h < 1) r.h = 1;
            rcBox(.bg = rcAlpha(rcHex(C_PEBBLE), (unsigned char)(70 + (i & 3) * 26)),
                  .borderRadius = "all-full", .wType = RC_PX(r.w), .hType = RC_PX(r.h),
                  .floating = scene_float(r.x, r.y, Z_ROCK)) {}
        }
    }
}

/* A near ridge along the foot of the Stage, running at 3/2 the ground's speed and
   drawn over everything but the controls. A side-scroller reads as depth because
   its layers move at DIFFERENT speeds, and this is the only one faster than the
   hero. */
static void scene_foreground(const PlWorld *w, const PlView *v) {
    int32_t lead = (int32_t)((int64_t)w->scrollX * 3 / 2);
    int32_t i0 = (lead - 400) / PL_FORE_STRIDE;
    int32_t i1 = (lead + v->sw + 400) / PL_FORE_STRIDE;

    if (i0 < 0) i0 = 0;
    for (int32_t i = i0; i <= i1; i++) {
        int32_t wx = i * PL_FORE_STRIDE + (int32_t)pl_scatter((uint32_t)i, 0x165667B1u, 96u);
        int sx = pl_parallax_x(wx, w->scrollX, 3, 2);
        int sw_ = 108 + (int)pl_scatter((uint32_t)i, 0x9E3779B1u, 146u);
        int sh  = 28 + (int)pl_scatter((uint32_t)i, 0xD6E8FEB8u, 56u);
        if (!pl_visible(sx, sw_, 240, v->sw))
            continue;
        {
            PlRect r = view_rect(v, sx, PL_DESIGN_H - sh, sw_, sh);
            if (r.w < 1) r.w = 1;
            if (r.h < 1) r.h = 1;
            rcBox(.bg = rcHex(C_FORE), .borderRadius = "t-3xl", .wType = RC_PX(r.w),
                  .hType = RC_PX(r.h), .floating = scene_float(r.x, r.y, Z_FORE)) {}
        }
    }
}

/* Foreground grass tufts standing on the terrain (full scroll speed). */
static void scene_tufts(const PlWorld *w, const PlView *v) {
    for (int i = 0; i < w->tuftCount; i++) {
        int sx = pl_ground_x(w, w->tuft[i].x);
        if (!pl_visible(sx, w->tuft[i].w, 40, v->sw))
            continue;
        int sy = pl_ground_top(w, w->tuft[i].x) - w->tuft[i].h;
        PlRect r = view_rect(v, sx, sy, w->tuft[i].w, w->tuft[i].h);
        rcBox(.bg = rcHex(w->tuft[i].tint), .borderRadius = "all-sm",
              .wType = RC_PX(r.w), .hType = RC_PX(r.h),
              .floating = scene_float(r.x, r.y, Z_TUFT)) {}
    }
}

/* Uncollected coins: a gold disc with a light pip. */
static void scene_coins(const PlWorld *w, const PlView *v) {
    for (int i = 0; i < w->coinCount; i++) {
        if (w->coin[i].collected)
            continue;
        int sx = pl_ground_x(w, w->coin[i].x) - PL_COIN_R;
        int sy = w->coin[i].y - PL_COIN_R;
        if (!pl_visible(sx, PL_COIN_R * 2, 40, v->sw))
            continue;
        PlRect r = view_rect(v, sx, sy, PL_COIN_R * 2, PL_COIN_R * 2);
        int pip = view_len(v, 8);
        rcBox(.bg = rcHex(C_COIN), .align = "cc", .borderRadius = "all-full",
              .wType = RC_PX(r.w), .hType = RC_PX(r.h),
              .floating = scene_float(r.x, r.y, Z_COIN)) {
            rcBox(.bg = rcHex(C_COIN_HI), .borderRadius = "all-full", .wType = RC_PX(pip),
                  .hType = RC_PX(pip)) {}
        }
    }
}

/* The hero's contact shadow: a dark ellipse ON the terrain under him, tightening
   and darkening as he lands. It is what tells a reader how high he is and where
   he will come down, which a silhouette against a flat sky cannot. */
static void scene_hero_shadow(const PlWorld *w, const PlView *v) {
    int footY  = w->heroFoot256 >> 8;
    int ground = pl_ground_top(w, pl_hero_world_x(w));
    int air    = ground - footY;                    /* 0 on the ground, grows upward */
    int wide;

    if (air < 0)   air = 0;
    if (air > 140) air = 140;
    wide = PL_HERO_W - (air * 10 / 140);
    {
        PlRect r = view_rect(v, PL_HERO_SCREEN_X - wide / 2, ground - 3, wide, 7);

        if (r.w < 1) r.w = 1;
        if (r.h < 1) r.h = 1;
        rcBox(.bg = rcAlpha(RC_BLACK, (unsigned char)(120 - air * 80 / 140)),
              .borderRadius = "all-full", .wType = RC_PX(r.w), .hType = RC_PX(r.h),
              .floating = scene_float(r.x, r.y, Z_SHADOW)) {}
    }
}

/* The hero: head, body and two legs whose heights alternate by the run phase
   while grounded. The parts sit on the box's FLOOR because at a fractional scale
   the truncated parts sum to a px or two less than the truncated box, and a
   centred stack would hover that much above the ground. */
static void scene_hero(const PlWorld *w, const PlView *v) {
    int footY = w->heroFoot256 >> 8;
    int sx    = PL_HERO_SCREEN_X - PL_HERO_W / 2;
    int sy    = footY - PL_HERO_H;
    int phase = (int)(w->step % PL_RUN_PHASES);
    bool run  = w->grounded;
    int legA  = run ? (phase < PL_RUN_PHASES / 2 ? 12 : 5) : 8;
    int legB  = run ? (phase < PL_RUN_PHASES / 2 ? 5 : 12) : 8;
    PlRect r  = view_rect(v, sx, sy, PL_HERO_W, PL_HERO_H);
    rcBox(.align = "bc", .wType = RC_PX(r.w), .hType = RC_PX(r.h),
          .floating = scene_float(r.x, r.y, Z_HERO)) {
        rcColumn(.gap = (uint16_t)view_len(v, 1), .align = "cc") {
            rcBox(.bg = rcHex(C_HERO_HEAD), .borderRadius = "all-full",
                  .wType = RC_PX(view_len(v, 12)), .hType = RC_PX(view_len(v, 12))) {}
            rcBox(.bg = rcHex(C_HERO_BODY), .borderRadius = "all-md",
                  .wType = RC_PX(view_len(v, 20)), .hType = RC_PX(view_len(v, 14))) {}
            rcRow(.gap = (uint16_t)view_len(v, 4), .align = "bc",
                  .hType = RC_PX(view_len(v, 12))) {
                rcBox(.bg = rcHex(C_HERO_LEG), .borderRadius = "all-sm",
                      .wType = RC_PX(view_len(v, 6)), .hType = RC_PX(view_len(v, legA))) {}
                rcBox(.bg = rcHex(C_HERO_LEG), .borderRadius = "all-sm",
                      .wType = RC_PX(view_len(v, 6)), .hType = RC_PX(view_len(v, legB))) {}
            }
        }
    }
}

/* Dust kicked up while running: a fading disc drifting back off-screen. */
static void scene_particles(const PlWorld *w, const PlView *v) {
    for (int i = 0; i < PL_MAX_PARTICLES; i++) {
        if (w->part[i].ttl <= 0)
            continue;
        int sx = w->part[i].x256 / PL_SUB;
        int sy = w->part[i].y256 / PL_SUB;
        if (!pl_visible(sx, 8, 20, v->sw))
            continue;
        int sz = 3 + w->part[i].ttl / 6;
        PlRect r = view_rect(v, sx, sy, sz, sz);
        rcBox(.bg = rcAlpha(rcHex(C_DUST), (unsigned char)(w->part[i].ttl * 10)),
              .borderRadius = "all-full", .wType = RC_PX(r.w), .hType = RC_PX(r.h),
              .floating = scene_float(r.x, r.y, Z_PARTICLE)) {}
    }
}

/* The score / distance / best HUD, top-left and PASSTHROUGH. The row aligns TOP,
   so the three captions share one line while the big COINS figure hangs below
   its own; centring them would push the tallest column's caption up alone. The
   values come from the backend's fixed-width buffers. */
static void scene_hud(const PlWorld *w, RC_Insets safe) {
    rcBox(.px = 16, .py = 10,
           .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                         .element = RC_ANCHOR_TOP_LEFT, .offset = { 4.0f + safe.left, 4 },
                         .zIndex = Z_HUD, .capture = RC_CAPTURE_PASSTHROUGH }) {
        rcRow(.gap = 20, .align = "tl") {
            rcColumn(.gap = 0) {
                rcTextL("COINS", .font = F_SMALL, .color = rcAlpha(RC_WHITE, 200));
                rcTextC(w->scoreStr, .font = F_HERO, .color = rcHex(C_COIN));
            }
            rcColumn(.gap = 0) {
                rcTextL("DIST", .font = F_SMALL, .color = rcAlpha(RC_WHITE, 200));
                rcTextC(w->distStr, .font = F_TITLE, .color = RC_WHITE);
            }
            rcColumn(.gap = 0) {
                rcTextL("BEST", .font = F_SMALL, .color = rcAlpha(RC_WHITE, 200));
                rcTextC(w->bestStr, .font = F_TITLE, .color = rcAlpha(RC_WHITE, 220));
            }
        }
    }
}

/* The on-screen JUMP control, inset by the safe bands below and beside it so a
   thumb is never asked to press the home indicator. A mouse has SPACE and the
   whole playfield, so there the button shrinks and steps back. */
static void scene_jump_button(RC_Insets safe, bool finger) {
    const int d = finger ? PL_JUMP_D : PL_JUMP_D_FINE;
    bool hov = rcIsHovered("JumpBtn");
    unsigned char a = finger ? (hov ? 235 : 180) : (hov ? 215 : 120);
    rcBox(.id = "JumpBtn", .bg = rcAlpha(rcHex(C_HERO_BODY), a),
          .align = "cc", .borderRadius = "all-full", .wType = RC_PX(d),
          .hType = RC_PX(d),
          .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_BOTTOM_RIGHT,
                         .element = RC_ANCHOR_BOTTOM_RIGHT,
                         .offset = { -PL_JUMP_INSET - safe.right, -PL_JUMP_INSET - safe.bottom },
                         .zIndex = Z_JUMP, .capture = RC_CAPTURE_ON }) {
        /* The cast is not decoration: a ternary over two enumerators is a
           non-constant expression, and narrowing one into the uint16_t slot is
           an ERROR for clang++ in C++20 where g++ accepts it silently. */
        rcTextL("JUMP", .font = (uint16_t)(finger ? F_TITLE : F_MD),
                .color = RC_WHITE);
    }
}

/* A faint how-to-play hint, bottom-centre and PASSTHROUGH. It names the input the
   pointer class actually has, and where the Stage is too narrow to share the foot
   with the JUMP button it lifts above it instead. */
static void scene_hint(const PlWorld *w, int stageW, RC_Insets safe, bool finger) {
    const bool cramped = stageW < PL_HINT_CENTRED_MIN_W;
    /* Solid until the first jump, then gone over PL_HINT_TTL ticks: an
       instruction already followed is clutter, and one that vanishes on the frame
       it is followed reads as a glitch. */
    int alpha = w->hintTtl < 0 ? 255 : w->hintTtl * 255 / PL_HINT_TTL;

    if (alpha <= 0)
        return;
    const float lift   = cramped ? (float)(PL_JUMP_INSET + PL_JUMP_D + PL_HINT_GAP)
                                 : (float)PL_HINT_INSET;
    rcBox(.bg = rcAlpha(RC_BLACK, (unsigned char)(120 * alpha / 255)), .px = 12, .py = 6,
          .borderRadius = "all-full",
          .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_BOTTOM_CENTER,
                         .element = RC_ANCHOR_BOTTOM_CENTER,
                         .offset = { 0, -lift - safe.bottom },
                         .zIndex = Z_HUD, .capture = RC_CAPTURE_PASSTHROUGH }) {
        rcTextC(finger ? "Tap anywhere or press JUMP to leap"
                       : "Press SPACE or click anywhere to leap",
                 .font = F_SMALL,
                 .color = rcAlpha(RC_WHITE, (unsigned char)(205 * alpha / 255)));
    }
}

/* The custom titlebar. Its content is inset by the side bands, which is what lets
   Root spend none of its own and the sky run under them. */
static void platformer_topbar(RC_Insets safe) {
    RC_Style s = rcGetStyle();
    /* CHROME, NOT CONTENT: the drag strip the user grabs is pinned at
       .titlebarHeight, so a band that scaled with the content zoom would stop
       matching it. rcUnzoomed() counter-scales by 1/zoom and takes the row as its
       single statement; at zoom 1 it changes nothing. */
    rcUnzoomed()
    rcRow(.id = RC_ID_WINDOW_DRAG, .bg = s.chrome, .gap = 10,
          .pl = (uint16_t)(14 + safe.left), .pr = (uint16_t)(14 + safe.right), .align = "cl",
          .w = "grow", .hType = RC_PX(PL_TOPBAR_H)) {
        rcBox(.bg = s.primary, .align = "cc", .borderRadius = "all-md", .w = "26px",
              .h = "26px") {
            rcTextL("R", .font = F_BODY, .color = RC_WHITE);
        }
        rcTextL("RayClay Runner", .font = F_MD, .color = s.text);
        rcBox(.w = "grow") {}
        rcWindowControls();
    }
}

void platformer_seed(AppState *st, unsigned seed) {
    pl_memzero(st, sizeof *st);
    pl_world_seed(&st->world, seed);
    st->jumpQueued = false;
    st->seeded     = true;
}

void platformer_update(AppState *st, const AppCtx *ctx) {
    if (ctx->dt <= 0.0f)                    /* dt <= 0 is a STRICT no-op */
        return;
    if (st->jumpQueued)
        pl_world_jump(&st->world);          /* grounded-gated inside; idempotent airborne */
    st->jumpQueued = false;
    pl_world_step(&st->world);              /* one fixed tick: scroll, physics, coins, dust, HUD */
}

void platformer_layout(AppState *st, const AppCtx *ctx) {
    rcSetStyle(rcStyleDark());
    RC_Style s = rcGetStyle();
    const PlWorld *w = &st->world;

    /* A game is full-bleed: Root spends only the TOP safe band, and the Stage
       runs under the side and bottom ones while the HUD, the button and the hint
       inset themselves by those instead. */
    RC_Insets safe = app_safe(ctx);
    const int stageW = (int)app_view_w(ctx);
    const int stageH = (int)(app_view_h(ctx) - safe.top) - PL_TOPBAR_H;
    const bool finger = ctx->view.coarsePointer;

    /* Read off AppCtx and NEVER off the live window, so a headless run at a fixed
       size takes the same branch as a real window that size. */
    PlView v = { .s = 1.0f, .sw = PL_DESIGN_W, .oy = 0,
                 .stageW = PL_DESIGN_W, .stageH = PL_DESIGN_H };
    if (ctx->mode == APP_DEMO && stageW > 0 && stageH > 0) {
        if (stageH < PL_DESIGN_H)
            v.s = (float)stageH / (float)PL_DESIGN_H;
        v.stageW = stageW;
        v.stageH = stageH;
        v.oy = stageH - view_len(&v, PL_DESIGN_H);   /* extra height becomes sky */
        v.sw = (int)((float)stageW / v.s);
        if (v.sw < PL_DESIGN_W) v.sw = PL_DESIGN_W;
    }

    rcColumn(.id = "Root", .bg = s.background, .pt = (uint16_t)safe.top, .w = "grow",
             .h = "grow") {
        platformer_topbar(safe);
        /* Stage: the play area and the twilight sky gradient, which needs the
           stable id. It is also the tap-to-jump target; the scene layers attach
           to it as floating children, back to front. */
        rcBox(.id = "Stage", .w = "grow", .h = "grow",
               .gradient = { .from = rcHex(C_SKY_TOP), .to = rcHex(C_SKY_BOT), .dir = "v" }) {
            scene_sky(w, &v);
            scene_parallax_far(w, &v);
            scene_parallax_mid(w, &v);
            scene_terrain(w, &v);
            scene_bedrock(&v);
            scene_rocks(w, &v);
            scene_tufts(w, &v);
            scene_goal(w, &v);
            scene_hero_shadow(w, &v);
            scene_coins(w, &v);
            scene_hero(w, &v);
            scene_particles(w, &v);
            scene_foreground(w, &v);
            scene_hud(w, safe);
            scene_hint(w, stageW, safe, finger);
            scene_jump_button(safe, finger);
        }
    }

    /* Read the tap AFTER the scene is declared, unioning three inputs on
       DELIBERATELY DIFFERENT EDGES. rcPressed is the PRESS edge, right here and
       only here: a jump is one non-destructive action whose latency is the whole
       point, on a button that captures its own taps. rcClicked is the RELEASE edge
       and the default; the Stage is the whole playfield, where an eager activation
       would fire on any press with no way to slide off and take it back.
       OR-accumulated, so a jump queued between ticks can never be lost. */
    if (rcPressed("JumpBtn") || rcClicked("Stage") ||
        rcKeyPressed(RC_KEY_SPACE) || rcKeyPressed(RC_KEY_UP) || rcKeyPressed(RC_KEY_W))
        st->jumpQueued = true;

    /* Polling a whole playfield for taps makes the pointer a clickable hand over
       every pixel of it, which a game surface should not be. An explicit set wins
       over the poll's default, so the JUMP button keeps its own pointer. */
    if (!rcIsHovered("JumpBtn"))
        rcSetCursor(RC_CURSOR_DEFAULT);
}

void platformer_demo_chrome(AppState *st, const AppCtx *ctx) {
    if (ctx->mode != APP_DEMO || !ctx->arena)
        return;
    /* A floating perf readout, PASSTHROUGH so it never blocks the JUMP button or
       the titlebar, and inset by the safe bands like the rest of the overlay. */
    RC_Insets safe = app_safe(ctx);
    RC_String hud = rcFormat(ctx->arena, "%.0f fps \xc2\xb7 %d coins \xc2\xb7 %d px",
                                ctx->dt > 0.0f ? 1.0f / ctx->dt : 0.0f,
                                st->world.score, st->world.scrollX);
    rcBox(.id = "demo_hud", .bg = rcAlpha(RC_BLACK, 150), .px = 10, .py = 5,
           .borderRadius = "all-full",
           .floating = { .to = RC_ATTACH_ROOT, .parent = RC_ANCHOR_BOTTOM_LEFT,
                         .element = RC_ANCHOR_BOTTOM_LEFT,
                         .offset = { 16.0f + safe.left, -16.0f - safe.bottom },
                         .zIndex = Z_JUMP + 10, .capture = RC_CAPTURE_PASSTHROUGH }) {
        rcText(hud, .font = F_SMALL, .color = RC_WHITE);
    }
}

void platformer_bench_step(AppState *st, const AppInputSink *in, int frame) {
    (void)st;   /* every action this app has is synthetic input */
    /* The scripted scenario: jump three times, then hold still, so a
       double-rendered frame is identical. The hold needs the hero GROUNDED with
       no jump queued, and an arc lasts about 50 ticks, so the jumps all land well
       before it. The coordinates aim at the centre of the JUMP button as a
       1280x720 window places it; re-aim them if that corner's geometry moves. */
    if (!in || frame >= PLATFORMER_BENCH_WARMUP)
        return;                                        /* the HOLD */

    if (frame == 4 || frame == 20 || frame == 40) {
        in->move(in->ctx, 1216.0f, 656.0f);            /* over the JUMP button */
        in->button(in->ctx, APP_MBTN_LEFT, true);      /* press edge -> queue a jump */
    } else if (frame == 5 || frame == 21 || frame == 41) {
        /* The button reads the press edge, so this release fires nothing - and it
           stays exactly for that reason: dropping it would leave the left button
           held down for the rest of the run. */
        in->button(in->ctx, APP_MBTN_LEFT, false);
    } else if (frame == PLATFORMER_BENCH_WARMUP - 2) {
        in->move(in->ctx, -100.0f, -100.0f);           /* park OFF-canvas before the hold */
    }
}
