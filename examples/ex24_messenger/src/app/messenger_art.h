/*
    messenger_art.h - everything the messenger draws instead of shipping: its stickers,
    the photos in its threads, the wallpaper and the bubble tails.

    Each piece is one RC_CustomDrawCallback over rcIconEmit, so it scales like an icon,
    looks the same on every platform and costs no asset. Stickers stand where color emoji
    would be: a system emoji font differs from host to host, and these do not. Include it
    from one translation unit: the shapes are static tables.
*/
#ifndef MESSENGER_ART_H
#define MESSENGER_ART_H

#include "rayclay.h"

/* The sticker set. The first six are the quick reactions, in the order they appear. */
typedef enum {
    ART_THUMB = 0, ART_HEART, ART_JOY, ART_WOW, ART_SAD, ART_FIRE,
    ART_GRIN, ART_LOVE, ART_WINK, ART_COOL, ART_ANGRY, ART_TADA,
    ART_STICKER_COUNT
} MsgSticker;

enum { ART_PHOTO_COUNT = 8 };

/* The index a callback draws, as stable storage its userData can point at: a draw
   callback runs after the layout returns, so its payload must outlive the frame. */
static const uint8_t ART_INDEX[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

/* One sky per photo, top and bottom: the box behind the scene paints it as a gradient. */
static const uint32_t ART_SKY[ART_PHOTO_COUNT][2] = {
    { 0x7fb2e5, 0xf7d9b0 }, { 0x9bd0e8, 0xe6f2ee }, { 0xf59e6c, 0xfde2a8 }, { 0x101a3a, 0x3a2f62 },
    { 0xc9e4d8, 0xeef4e2 }, { 0xf9c784, 0xfde9c9 }, { 0x07111f, 0x123a4a }, { 0x8ecbf0, 0xdff1fb },
};

static const RC_Color ART_INK     = { 102.0f, 69.0f, 0.0f, 255.0f };
static const RC_Color ART_SKIN    = { 255.0f, 204.0f, 77.0f, 255.0f };
static const RC_Color ART_RED     = { 221.0f, 46.0f, 68.0f, 255.0f };
static const RC_Color ART_TEAR    = { 93.0f, 173.0f, 236.0f, 255.0f };
static const RC_Color ART_WHITE   = { 255.0f, 255.0f, 255.0f, 255.0f };
static const RC_Color ART_ORANGE  = { 244.0f, 144.0f, 12.0f, 255.0f };
static const RC_Color ART_YELLOW  = { 255.0f, 220.0f, 93.0f, 255.0f };
static const RC_Color ART_BLUE    = { 85.0f, 172.0f, 238.0f, 255.0f };
static const RC_Color ART_GREEN   = { 119.0f, 178.0f, 85.0f, 255.0f };
static const RC_Color ART_PURPLE  = { 170.0f, 142.0f, 214.0f, 255.0f };
static const RC_Color ART_ANGER   = { 244.0f, 120.0f, 80.0f, 255.0f };

/* A heart at any size, one outline, so a translucent ink shows no overlap. Its points are built
   on the stack: the polygon memo keys on their bytes, so that costs no second triangulation. */
static inline void art_heart(RC_BoundingBox b, float cx, float cy, float s, float vb, RC_Color c) {
    static const RC_IconPoint unit[32] = {
        { 0.000f, -0.215f }, { 0.008f, -0.262f }, { 0.059f, -0.383f }, { 0.180f, -0.525f },
        { 0.371f, -0.625f }, { 0.604f, -0.637f }, { 0.828f, -0.547f }, { 0.991f, -0.374f },
        { 1.050f, -0.153f }, { 0.991f, 0.078f }, { 0.828f, 0.299f }, { 0.604f, 0.502f },
        { 0.371f, 0.690f }, { 0.180f, 0.864f }, { 0.059f, 1.011f }, { 0.008f, 1.112f },
        { 0.000f, 1.149f }, { -0.008f, 1.112f }, { -0.059f, 1.011f }, { -0.180f, 0.864f },
        { -0.371f, 0.690f }, { -0.604f, 0.502f }, { -0.828f, 0.299f }, { -0.991f, 0.078f },
        { -1.050f, -0.153f }, { -0.991f, -0.374f }, { -0.828f, -0.547f }, { -0.604f, -0.637f },
        { -0.371f, -0.625f }, { -0.180f, -0.525f }, { -0.059f, -0.383f }, { -0.008f, -0.262f },
    };
    RC_IconPoint at[32];

    for (int i = 0; i < 32; i++) {
        at[i].x = cx + s * unit[i].x;
        at[i].y = cy + s * unit[i].y;
    }
    rcIconDrawFilledPolygon(b, at, 32, vb, c);
}

static inline void art_eye(RC_BoundingBox b, float cx, float cy, float ry) {
    rcIconDrawFilledEllipse(b, cx, cy, 2.2f, ry, 36.0f, ART_INK);
}

/* A mouth open in a smile: the dark half disc, its top teeth, and nothing more. */
static inline void art_open_smile(RC_BoundingBox b) {
    static const RC_IconPoint mouth[] = {
        { 9.00f, 20.00f }, { 27.00f, 20.00f }, { 26.92f, 21.28f }, { 26.69f, 22.54f },
        { 26.31f, 23.75f }, { 25.79f, 24.90f }, { 25.14f, 25.97f }, { 24.36f, 26.93f },
        { 23.48f, 27.77f }, { 22.50f, 28.49f }, { 21.44f, 29.05f }, { 20.33f, 29.47f },
        { 19.17f, 29.72f }, { 18.00f, 29.80f }, { 16.83f, 29.72f }, { 15.67f, 29.47f },
        { 14.56f, 29.05f }, { 13.50f, 28.49f }, { 12.52f, 27.77f }, { 11.64f, 26.93f },
        { 10.86f, 25.97f }, { 10.21f, 24.90f }, { 9.69f, 23.75f }, { 9.31f, 22.54f },
        { 9.08f, 21.28f },
    };
    static const RC_IconPoint teeth[] = {
        { 10.2f, 20.0f }, { 25.8f, 20.0f }, { 25.4f, 22.4f }, { 10.6f, 22.4f },
    };

    rcIconDrawFilledPolygon(b, mouth, 25, 36.0f, ART_INK);
    rcIconDrawFilledPolygon(b, teeth, 4, 36.0f, ART_WHITE);
}

static inline void art_smile_line(RC_BoundingBox b) {
    static const RC_IconPoint line[] = {
        { 10.0f, 21.5f }, { 12.4f, 24.4f }, { 15.0f, 26.0f }, { 18.0f, 26.6f },
        { 21.0f, 26.0f }, { 23.6f, 24.4f }, { 26.0f, 21.5f },
    };

    rcIconDrawPolyline(b, line, 7, 36.0f, 2.0f, false, ART_INK);
}

static inline void art_frown_line(RC_BoundingBox b) {
    static const RC_IconPoint line[] = {
        { 12.0f, 28.0f }, { 14.4f, 25.8f }, { 18.0f, 24.8f }, { 21.6f, 25.8f }, { 24.0f, 28.0f },
    };

    rcIconDrawPolyline(b, line, 5, 36.0f, 2.0f, false, ART_INK);
}

/* One sticker; userData points at its MsgSticker in ART_INDEX. */
static void art_draw_sticker(RC_BoundingBox b, RC_Color c, const void *ud) {
    static const RC_IconPoint thumbHand[] = {
        { 11.5f, 16.0f }, { 16.5f, 8.0f }, { 18.0f, 3.5f }, { 21.0f, 3.5f }, { 23.0f, 6.5f },
        { 22.0f, 13.5f }, { 30.5f, 13.5f }, { 33.0f, 16.5f }, { 30.5f, 30.0f }, { 27.5f, 32.5f },
        { 11.5f, 32.5f },
    };
    static const RC_IconPoint thumbCuff[] = {
        { 3.5f, 15.5f }, { 10.0f, 15.5f }, { 10.0f, 33.0f }, { 3.5f, 33.0f },
    };
    static const RC_IconPoint flameOuter[] = {
        { 18.5f, 1.5f }, { 23.5f, 9.0f }, { 28.5f, 14.0f }, { 30.5f, 21.0f }, { 28.0f, 29.0f },
        { 21.0f, 34.5f }, { 14.0f, 34.5f }, { 7.5f, 29.0f }, { 5.5f, 21.0f }, { 8.0f, 14.0f },
        { 11.0f, 18.0f }, { 13.0f, 10.0f },
    };
    static const RC_IconPoint flameInner[] = {
        { 18.5f, 15.0f }, { 22.5f, 20.5f }, { 24.0f, 26.0f }, { 21.5f, 32.0f }, { 18.0f, 34.0f },
        { 14.5f, 32.0f }, { 12.0f, 26.0f }, { 14.0f, 21.5f },
    };
    static const RC_IconPoint shades[] = {
        { 6.0f, 12.0f }, { 30.0f, 12.0f }, { 29.0f, 18.5f }, { 25.0f, 20.0f }, { 20.5f, 18.5f },
        { 18.0f, 15.5f }, { 15.5f, 18.5f }, { 11.0f, 20.0f }, { 7.0f, 18.5f },
    };
    static const RC_IconPoint wink[] = {
        { 20.5f, 14.5f }, { 24.0f, 12.0f }, { 27.5f, 14.5f },
    };
    static const RC_IconPoint squintL[] = { { 9.0f, 14.5f }, { 12.5f, 11.0f }, { 16.0f, 14.5f } };
    static const RC_IconPoint squintR[] = { { 20.0f, 14.5f }, { 23.5f, 11.0f }, { 27.0f, 14.5f } };
    static const RC_IconPoint cone[] = {
        { 3.0f, 33.0f }, { 10.5f, 13.0f }, { 23.0f, 25.5f },
    };
    /* A tear of joy: its point at the eye's outer corner, its drop out on the cheek. An upright
       oval on the face's rim reads as an earmuff. */
    static const RC_IconPoint tearL[] = {
        { 9.0f, 15.5f }, { 8.2f, 22.4f }, { 7.3f, 24.3f }, { 5.4f, 25.2f }, { 3.3f, 24.7f },
        { 2.0f, 23.1f }, { 2.0f, 21.0f }, { 3.2f, 19.3f },
    };
    static const RC_IconPoint tearR[] = {
        { 27.0f, 15.5f }, { 32.8f, 19.3f }, { 34.0f, 21.0f }, { 34.0f, 23.1f }, { 32.7f, 24.7f },
        { 30.6f, 25.2f }, { 28.7f, 24.3f }, { 27.8f, 22.4f },
    };
    const int which = *(const uint8_t *)ud;

    (void)c;
    switch (which) {
    case ART_THUMB:
        rcIconDrawFilledPolygon(b, thumbCuff, 4, 36.0f, ART_BLUE);
        rcIconDrawFilledPolygon(b, thumbHand, 11, 36.0f, ART_YELLOW);
        return;
    case ART_HEART:
        art_heart(b, 18.0f, 14.0f, 15.5f, 36.0f, ART_RED);
        return;
    case ART_FIRE:
        rcIconDrawFilledPolygon(b, flameOuter, 12, 36.0f, ART_ORANGE);
        rcIconDrawFilledPolygon(b, flameInner, 8, 36.0f, ART_YELLOW);
        return;
    case ART_TADA:
        rcIconDrawFilledPolygon(b, cone, 3, 36.0f, ART_ORANGE);
        rcIconDrawFilledCircle(b, 24.0f, 7.0f, 2.2f, 36.0f, ART_RED);
        rcIconDrawFilledCircle(b, 30.5f, 15.0f, 2.0f, 36.0f, ART_BLUE);
        rcIconDrawFilledCircle(b, 16.5f, 5.0f, 1.8f, 36.0f, ART_GREEN);
        rcIconDrawFilledCircle(b, 31.0f, 26.0f, 1.8f, 36.0f, ART_PURPLE);
        rcIconDrawRoundLine(b, 17.0f, 13.5f, 21.0f, 9.5f, 36.0f, 2.0f, ART_PURPLE);
        rcIconDrawRoundLine(b, 22.5f, 19.0f, 27.5f, 16.5f, 36.0f, 2.0f, ART_GREEN);
        return;
    default:
        break;
    }
    rcIconDrawFilledCircle(b, 18.0f, 18.0f, 17.0f, 36.0f, which == ART_ANGRY ? ART_ANGER
                           : ART_SKIN);
    switch (which) {
    case ART_JOY:
        rcIconDrawPolyline(b, squintL, 3, 36.0f, 2.2f, false, ART_INK);
        rcIconDrawPolyline(b, squintR, 3, 36.0f, 2.2f, false, ART_INK);
        art_open_smile(b);
        rcIconDrawFilledPolygon(b, tearL, 8, 36.0f, ART_TEAR);
        rcIconDrawFilledPolygon(b, tearR, 8, 36.0f, ART_TEAR);
        break;
    case ART_WOW:
        art_eye(b, 12.0f, 13.0f, 3.4f);
        art_eye(b, 24.0f, 13.0f, 3.4f);
        rcIconDrawFilledEllipse(b, 18.0f, 25.5f, 4.2f, 5.2f, 36.0f, ART_INK);
        break;
    case ART_SAD:
        art_eye(b, 12.0f, 14.0f, 3.0f);
        art_eye(b, 24.0f, 14.0f, 3.0f);
        art_frown_line(b);
        rcIconDrawFilledEllipse(b, 26.5f, 21.5f, 2.2f, 3.2f, 36.0f, ART_TEAR);
        break;
    case ART_GRIN:
        art_eye(b, 12.0f, 13.0f, 3.2f);
        art_eye(b, 24.0f, 13.0f, 3.2f);
        art_open_smile(b);
        break;
    case ART_LOVE:
        art_heart(b, 11.5f, 12.5f, 5.0f, 36.0f, ART_RED);
        art_heart(b, 24.5f, 12.5f, 5.0f, 36.0f, ART_RED);
        art_open_smile(b);
        break;
    case ART_WINK:
        art_eye(b, 12.0f, 13.5f, 3.2f);
        rcIconDrawPolyline(b, wink, 3, 36.0f, 2.2f, false, ART_INK);
        art_smile_line(b);
        break;
    case ART_COOL:
        rcIconDrawFilledPolygon(b, shades, 9, 36.0f, ART_INK);
        art_smile_line(b);
        break;
    default: /* ART_ANGRY */
        rcIconDrawRoundLine(b, 7.5f, 9.5f, 15.0f, 13.5f, 36.0f, 2.4f, ART_INK);
        rcIconDrawRoundLine(b, 28.5f, 9.5f, 21.0f, 13.5f, 36.0f, 2.4f, ART_INK);
        art_eye(b, 12.5f, 16.5f, 2.4f);
        art_eye(b, 23.5f, 16.5f, 2.4f);
        art_frown_line(b);
        break;
    }
}

/* A filled polygon in the photos' 100-unit box, in a color named once at the call. */
static inline void art_fill(RC_BoundingBox b, const RC_IconPoint *p, int n, uint32_t rgb) {
    rcIconDrawFilledPolygon(b, p, n, 100.0f, rcHex(rgb));
}

static inline void art_disc(RC_BoundingBox b, float x, float y, float r, uint32_t rgb) {
    rcIconDrawFilledCircle(b, x, y, r, 100.0f, rcHex(rgb));
}

#define ART_LEN(a) ((int)(sizeof a / sizeof a[0]))

/* The eight photos, painted over the sky their box already has. A bubble shows the top
   three quarters of the square (messenger_app.c's photo), so nothing important sits low. */
static void art_draw_photo(RC_BoundingBox b, RC_Color c, const void *ud) {
    /* 0: the ridge, snow on the far range. */
    static const RC_IconPoint farRange[] = {
        { 0.0f, 46.0f }, { 14.0f, 30.0f }, { 24.0f, 38.0f }, { 42.0f, 16.0f }, { 58.0f, 36.0f },
        { 72.0f, 24.0f }, { 100.0f, 44.0f }, { 100.0f, 100.0f }, { 0.0f, 100.0f },
    };
    static const RC_IconPoint snow[] = {
        { 42.0f, 16.0f }, { 50.0f, 26.0f }, { 45.0f, 24.0f }, { 42.0f, 28.0f }, { 38.0f, 24.0f },
        { 34.0f, 26.0f },
    };
    static const RC_IconPoint midRange[] = {
        { 0.0f, 58.0f }, { 22.0f, 44.0f }, { 44.0f, 56.0f }, { 66.0f, 42.0f }, { 100.0f, 60.0f },
        { 100.0f, 100.0f }, { 0.0f, 100.0f },
    };
    static const RC_IconPoint nearRidge[] = {
        { 0.0f, 68.0f }, { 30.0f, 62.0f }, { 58.0f, 72.0f }, { 100.0f, 64.0f }, { 100.0f, 100.0f },
        { 0.0f, 100.0f },
    };
    /* 1: the lake under its hills. */
    static const RC_IconPoint hills[] = {
        { 0.0f, 38.0f }, { 20.0f, 26.0f }, { 42.0f, 36.0f }, { 66.0f, 22.0f }, { 100.0f, 36.0f },
        { 100.0f, 46.0f }, { 0.0f, 46.0f },
    };
    static const RC_IconPoint water[] = {
        { 0.0f, 46.0f }, { 100.0f, 46.0f }, { 100.0f, 100.0f }, { 0.0f, 100.0f },
    };
    static const RC_IconPoint reflection[] = {
        { 0.0f, 46.0f }, { 100.0f, 46.0f }, { 100.0f, 54.0f }, { 66.0f, 62.0f }, { 42.0f, 54.0f },
        { 20.0f, 60.0f }, { 0.0f, 52.0f },
    };
    /* 2: the beach at dusk. */
    static const RC_IconPoint sea[] = {
        { 0.0f, 44.0f }, { 100.0f, 44.0f }, { 100.0f, 66.0f }, { 0.0f, 66.0f },
    };
    static const RC_IconPoint sand[] = {
        { 0.0f, 60.0f }, { 40.0f, 58.0f }, { 100.0f, 64.0f }, { 100.0f, 100.0f }, { 0.0f, 100.0f },
    };
    static const RC_IconPoint palm[] = {
        { 13.0f, 66.0f }, { 16.0f, 40.0f }, { 20.0f, 26.0f }, { 22.0f, 27.0f }, { 19.0f, 40.0f },
        { 17.0f, 66.0f },
    };
    static const RC_IconPoint frondL[] = {
        { 21.0f, 26.0f }, { 10.0f, 19.0f }, { 2.0f, 27.0f }, { 11.0f, 24.0f },
    };
    static const RC_IconPoint frondR[] = {
        { 21.0f, 26.0f }, { 33.0f, 20.0f }, { 40.0f, 30.0f }, { 30.0f, 26.0f },
    };
    /* 3: the city at night. */
    static const RC_IconPoint towerA[] = {
        { 6.0f, 34.0f }, { 22.0f, 34.0f }, { 22.0f, 100.0f }, { 6.0f, 100.0f },
    };
    static const RC_IconPoint towerB[] = {
        { 26.0f, 18.0f }, { 40.0f, 18.0f }, { 40.0f, 100.0f }, { 26.0f, 100.0f },
    };
    static const RC_IconPoint towerC[] = {
        { 46.0f, 40.0f }, { 66.0f, 40.0f }, { 66.0f, 100.0f }, { 46.0f, 100.0f },
    };
    static const RC_IconPoint towerD[] = {
        { 72.0f, 26.0f }, { 92.0f, 26.0f }, { 92.0f, 100.0f }, { 72.0f, 100.0f },
    };
    /* 4: the forest, three layers of pines. */
    static const RC_IconPoint pinesFar[] = {
        { 0.0f, 50.0f }, { 8.0f, 30.0f }, { 16.0f, 50.0f }, { 26.0f, 26.0f }, { 36.0f, 50.0f },
        { 48.0f, 28.0f }, { 60.0f, 50.0f }, { 70.0f, 24.0f }, { 82.0f, 50.0f }, { 92.0f, 30.0f },
        { 100.0f, 50.0f }, { 100.0f, 100.0f }, { 0.0f, 100.0f },
    };
    static const RC_IconPoint pinesMid[] = {
        { 0.0f, 62.0f }, { 12.0f, 36.0f }, { 24.0f, 62.0f }, { 38.0f, 32.0f }, { 52.0f, 62.0f },
        { 66.0f, 34.0f }, { 80.0f, 62.0f }, { 92.0f, 40.0f }, { 100.0f, 58.0f }, { 100.0f, 100.0f },
        { 0.0f, 100.0f },
    };
    static const RC_IconPoint pinesNear[] = {
        { 0.0f, 74.0f }, { 18.0f, 48.0f }, { 36.0f, 74.0f }, { 56.0f, 44.0f }, { 76.0f, 74.0f },
        { 90.0f, 52.0f }, { 100.0f, 68.0f }, { 100.0f, 100.0f }, { 0.0f, 100.0f },
    };
    /* 5: the dunes. */
    static const RC_IconPoint duneFar[] = {
        { 0.0f, 44.0f }, { 22.0f, 34.0f }, { 48.0f, 42.0f }, { 74.0f, 30.0f }, { 100.0f, 40.0f },
        { 100.0f, 100.0f }, { 0.0f, 100.0f },
    };
    static const RC_IconPoint duneMid[] = {
        { 0.0f, 58.0f }, { 30.0f, 44.0f }, { 60.0f, 58.0f }, { 100.0f, 48.0f }, { 100.0f, 100.0f },
        { 0.0f, 100.0f },
    };
    static const RC_IconPoint duneNear[] = {
        { 0.0f, 72.0f }, { 40.0f, 58.0f }, { 74.0f, 70.0f }, { 100.0f, 62.0f }, { 100.0f, 100.0f },
        { 0.0f, 100.0f },
    };
    /* 6: the aurora over a black range. */
    static const RC_IconPoint auroraA[] = {
        { 0.0f, 26.0f }, { 30.0f, 8.0f }, { 62.0f, 22.0f }, { 100.0f, 6.0f }, { 100.0f, 26.0f },
        { 62.0f, 40.0f }, { 30.0f, 28.0f }, { 0.0f, 44.0f },
    };
    static const RC_IconPoint auroraB[] = {
        { 0.0f, 36.0f }, { 34.0f, 22.0f }, { 66.0f, 34.0f }, { 100.0f, 20.0f }, { 100.0f, 30.0f },
        { 66.0f, 44.0f }, { 34.0f, 32.0f }, { 0.0f, 46.0f },
    };
    static const RC_IconPoint blackRange[] = {
        { 0.0f, 62.0f }, { 18.0f, 48.0f }, { 34.0f, 60.0f }, { 56.0f, 44.0f }, { 80.0f, 62.0f },
        { 100.0f, 52.0f }, { 100.0f, 100.0f }, { 0.0f, 100.0f },
    };
    /* 7: the field with one tree. */
    static const RC_IconPoint field[] = {
        { 0.0f, 50.0f }, { 100.0f, 44.0f }, { 100.0f, 100.0f }, { 0.0f, 100.0f },
    };
    static const RC_IconPoint furrowA[] = {
        { 0.0f, 62.0f }, { 58.0f, 47.0f }, { 62.0f, 47.0f }, { 0.0f, 72.0f },
    };
    static const RC_IconPoint furrowB[] = {
        { 42.0f, 100.0f }, { 66.0f, 47.0f }, { 70.0f, 46.0f }, { 70.0f, 100.0f },
    };
    static const RC_IconPoint trunk[] = {
        { 77.0f, 32.0f }, { 80.0f, 32.0f }, { 80.0f, 48.0f }, { 77.0f, 48.0f },
    };
    const int which = *(const uint8_t *)ud;

    (void)c;
    switch (which) {
    case 0:
        art_disc(b, 72.0f, 14.0f, 6.0f, 0xfff3c4);
        art_fill(b, farRange, ART_LEN(farRange), 0x7d93b5);
        art_fill(b, snow, ART_LEN(snow), 0xf4f7fb);
        art_fill(b, midRange, ART_LEN(midRange), 0x4d6a7d);
        art_fill(b, nearRidge, ART_LEN(nearRidge), 0x2f5140);
        break;
    case 1:
        art_fill(b, water, ART_LEN(water), 0x5da5c0);
        art_fill(b, hills, ART_LEN(hills), 0x3f7f5f);
        art_fill(b, reflection, ART_LEN(reflection), 0x3b7f8a);
        art_disc(b, 30.0f, 68.0f, 1.2f, 0xe6f2ee);
        art_disc(b, 64.0f, 72.0f, 1.0f, 0xe6f2ee);
        break;
    case 2:
        art_disc(b, 60.0f, 42.0f, 12.0f, 0xfff0c2);
        art_fill(b, sea, ART_LEN(sea), 0xd9745a);
        art_fill(b, sand, ART_LEN(sand), 0xe8c79b);
        art_fill(b, palm, ART_LEN(palm), 0x3d2c24);
        art_fill(b, frondL, ART_LEN(frondL), 0x2c3f2a);
        art_fill(b, frondR, ART_LEN(frondR), 0x2c3f2a);
        break;
    case 3:
        art_disc(b, 82.0f, 10.0f, 4.0f, 0xf4f1d0);
        art_disc(b, 12.0f, 12.0f, 0.6f, 0xffffff);
        art_disc(b, 48.0f, 8.0f, 0.5f, 0xffffff);
        art_disc(b, 60.0f, 20.0f, 0.6f, 0xffffff);
        art_fill(b, towerA, ART_LEN(towerA), 0x1b2448);
        art_fill(b, towerB, ART_LEN(towerB), 0x232d58);
        art_fill(b, towerC, ART_LEN(towerC), 0x18203f);
        art_fill(b, towerD, ART_LEN(towerD), 0x202a52);
        for (int k = 0; k < 18; k++) {
            /* A fixed scatter of lit windows, walked by a step coprime with the grid. */
            const float x = 9.0f + (float)((k * 37) % 80);
            const float y = 42.0f + (float)((k * 13) % 30);
            const RC_IconPoint win[] = {
                { x, y }, { x + 2.0f, y }, { x + 2.0f, y + 3.0f }, { x, y + 3.0f },
            };
            art_fill(b, win, 4, k % 3 ? 0xffd66b : 0xfff2b0);
        }
        break;
    case 4:
        art_fill(b, pinesFar, ART_LEN(pinesFar), 0x8fb8a0);
        art_fill(b, pinesMid, ART_LEN(pinesMid), 0x4f8567);
        art_fill(b, pinesNear, ART_LEN(pinesNear), 0x25543f);
        break;
    case 5:
        art_disc(b, 22.0f, 20.0f, 8.0f, 0xfffaf0);
        art_fill(b, duneFar, ART_LEN(duneFar), 0xe9a55f);
        art_fill(b, duneMid, ART_LEN(duneMid), 0xd9853f);
        art_fill(b, duneNear, ART_LEN(duneNear), 0xb9652a);
        break;
    case 6:
        art_disc(b, 16.0f, 8.0f, 0.5f, 0xffffff);
        art_disc(b, 52.0f, 6.0f, 0.5f, 0xffffff);
        art_disc(b, 86.0f, 14.0f, 0.6f, 0xffffff);
        art_fill(b, auroraA, ART_LEN(auroraA), 0x2fbf8f);
        art_fill(b, auroraB, ART_LEN(auroraB), 0x7be0b0);
        art_fill(b, blackRange, ART_LEN(blackRange), 0x0a1420);
        break;
    default:
        art_disc(b, 26.0f, 16.0f, 6.0f, 0xffffff);
        art_disc(b, 34.0f, 16.0f, 7.0f, 0xffffff);
        art_disc(b, 42.0f, 17.0f, 5.0f, 0xffffff);
        art_fill(b, field, ART_LEN(field), 0x9dbb5a);
        art_fill(b, furrowA, ART_LEN(furrowA), 0x86a648);
        art_fill(b, furrowB, ART_LEN(furrowB), 0x86a648);
        art_fill(b, trunk, ART_LEN(trunk), 0x5a4230);
        art_disc(b, 78.5f, 28.0f, 9.0f, 0x3f7f46);
        break;
    }
}

/* The wallpaper: a repeating tile of faint shapes in the one color it is given. Each tile draws
   in a 220 px box of its own, so a tile keeps its 220 px however large the pane is, and every
   other row of tiles starts half a tile in, so the repeat does not line up into a grid. Every
   tile hands the library the same points: its polygon memo keys on their bytes, so the whole
   wallpaper is three shapes cut once, at any pane size. Filled shapes on purpose: each is one
   draw call, where a stroked outline is many, and this runs every frame. */
static void art_draw_wallpaper(RC_BoundingBox b, RC_Color c, const void *ud) {
    static const RC_IconPoint star[] = {
        { 0.0f, -11.0f }, { 3.2f, -3.4f }, { 11.0f, -3.4f }, { 4.6f, 1.5f }, { 6.8f, 9.3f },
        { 0.0f, 4.6f }, { -6.8f, 9.3f }, { -4.6f, 1.5f }, { -11.0f, -3.4f }, { -3.2f, -3.4f },
    };
    RC_IconPoint big[10], small[10];   /* the second star is smaller and stands on its point */
    int row = 0;

    (void)ud;
    for (int k = 0; k < 10; k++) {
        big[k].x = star[k].x + 40.0f;
        big[k].y = star[k].y + 140.0f;
        small[k].x = 170.0f + 0.7f * star[k].x;
        small[k].y = 180.0f - 0.7f * star[k].y;
    }
    for (float ty = 0.0f; ty < b.width; ty += 220.0f, row++) {
        for (float tx = row % 2 ? -110.0f : 0.0f; tx < b.width; tx += 220.0f) {
            const RC_BoundingBox t = { b.x + tx, b.y + ty, 220.0f, 220.0f };

            rcIconDrawFilledCircle(t, 148.0f, 22.0f, 4.0f, 220.0f, c);
            rcIconDrawFilledCircle(t, 92.0f, 186.0f, 3.5f, 220.0f, c);
            art_heart(t, 100.0f, 88.0f, 9.0f, 220.0f, c);
            rcIconDrawFilledPolygon(t, big, 10, 220.0f, c);
            rcIconDrawFilledPolygon(t, small, 10, 220.0f, c);
        }
    }
}

/* The bubble tails: the sliver that says which side spoke, on the first bubble of a run.
   The box is square, so the tail fills its inner edge and the rest stays empty; the
   bubble's matching corner is squared to meet it. */
static void art_draw_tail_in(RC_BoundingBox b, RC_Color c, const void *ud) {
    static const RC_IconPoint tail[] = {
        { 13.0f, 0.0f }, { 7.8f, 0.0f }, { 7.0f, 0.35f }, { 6.5f, 1.0f },
        { 6.4f, 1.8f }, { 6.53f, 2.57f }, { 13.0f, 11.2f },
    };

    (void)ud;
    rcIconDrawFilledPolygon(b, tail, 7, 13.0f, c);
}

static void art_draw_tail_out(RC_BoundingBox b, RC_Color c, const void *ud) {
    static const RC_IconPoint tail[] = {
        { 0.0f, 0.0f }, { 0.0f, 11.2f }, { 6.47f, 2.57f }, { 6.6f, 1.8f },
        { 6.5f, 1.0f }, { 6.0f, 0.35f }, { 5.2f, 0.0f },
    };

    (void)ud;
    rcIconDrawFilledPolygon(b, tail, 7, 13.0f, c);
}

/* The typing dots. userData points at the state's phase, a number the app advances on
   its own clock, so the dots freeze when the app does. Each dot rides its own part of
   the wave and rests between. */
static void art_draw_typing(RC_BoundingBox b, RC_Color c, const void *ud) {
    const float phase = *(const float *)ud;

    for (int k = 0; k < 3; k++) {
        float t = phase - (float)k * 0.16f;

        /* The fraction of the cycle: a later dot's place is behind the phase, so it is
           negative for the first third of a cycle, and truncation alone would leave it so. */
        t -= (float)(int)t;
        if (t < 0.0f)
            t += 1.0f;
        /* A triangle over the first half of the cycle: up, down, then still. */
        const float lift = t < 0.25f ? t * 4.0f : (t < 0.5f ? (0.5f - t) * 4.0f : 0.0f);

        rcIconDrawFilledCircle(b, 6.0f + (float)k * 8.0f, 12.0f - lift * 3.5f, 2.6f, 28.0f, c);
    }
}

#endif /* MESSENGER_ART_H */
