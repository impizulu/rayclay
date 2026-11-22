/*
    samples.h - the sample drive's pictures, painted in code

    A file browser sells itself with its previews, and a phone or a browser has no file
    of ours to show. So each picture on the sample drive is encoded here as a 32-bit BMP
    and decoded with rcLoadImageFromMemory, the call for image bytes you already hold,
    beside rcLoadImage for a path.

    A 32-bit BMP is the format the decoder takes that can be written with no compressor
    and no libc: a 54-byte header, then bottom-up BGRA rows, which are 4-byte aligned so
    BMP's row padding never applies.
*/
#ifndef APP_SAMPLES_H
#define APP_SAMPLES_H

#include "rayclay.h"

/* The largest picture is 480 x 320, so a phone's 3x screen shows it scaled up by about two,
   not six, and a desktop's preview scales it down. */
enum { SAMPLE_CAP = 54 + 480 * 320 * 4 };

/* The drive's pictures: what sample_tree.h names and the table below paints. */
enum {
    PIC_DAWN, PIC_ALPINE, PIC_DESERT, PIC_MORNING, PIC_PIER, PIC_TAGUS, PIC_LOFTS, PIC_ALFAMA,
    PIC_MIRADOURO, PIC_PALETTE, PIC_STUDY, PIC_SCREEN, PIC_MARK_BLUE, PIC_MARK_MONO, PIC_LOCKUP,
    PIC_LOGO_V1, PIC_FIGURE, SAMPLE_COUNT
};

typedef struct { int r, g, b; } Rgb;

typedef enum {
    PAINT_LAND, PAINT_SEA, PAINT_TOWN, PAINT_MARK, PAINT_SWATCHES, PAINT_SCREEN, PAINT_FIGURE
} Paint;

/* One picture: the pixels painted, the size its Info reads (a camera's, a screen's), and
   six colors, whose roles each paint_ function below names. x, y and r place its sun or
   its mark. alt is that kind's other form. */
typedef struct {
    Paint paint;
    int   w, h, fileW, fileH;
    Rgb   a, b, c, d, e, f;
    int   x, y, r;
    bool  alt;
} Scene;

static const Scene SCENES[SAMPLE_COUNT] = {
    /* PIC_DAWN */
    { PAINT_SEA, 480, 320, 6000, 4000,
      { 250, 170, 140 }, { 255, 226, 196 }, { 255, 246, 220 },
      { 178, 132, 170 }, { 232, 168, 150 }, {  70,  60, 110 }, 332, 150, 40, false },
    /* PIC_ALPINE */
    { PAINT_LAND, 480, 320, 6720, 4480,
      {  70, 136, 214 }, { 196, 226, 250 }, { 255, 252, 236 },
      { 132, 168, 196 }, {  46, 110,  98 }, {  24,  70,  62 }, 120,  80, 30, true },
    /* PIC_DESERT */
    { PAINT_LAND, 480, 320, 6000, 4000,
      {  44,  50, 118 }, { 236, 122, 104 }, { 255, 214, 124 },
      { 154,  84,  94 }, {  98,  50,  72 }, {  52,  28,  44 }, 240, 192, 52, false },
    /* PIC_MORNING */
    { PAINT_LAND, 480, 320, 5472, 3648,
      { 255, 196, 112 }, { 255, 238, 200 }, { 255, 252, 230 },
      { 226, 164,  96 }, { 170, 106,  62 }, {  94,  58,  38 }, 150, 130, 48, false },
    /* PIC_PIER */
    { PAINT_SEA, 480, 320, 5472, 3648,
      { 120, 138, 158 }, { 214, 220, 224 }, { 240, 242, 238 },
      {  52,  58,  68 }, { 150, 165, 178 }, {  58,  76,  96 }, 380,  84, 26, true },
    /* PIC_TAGUS */
    { PAINT_SEA, 480, 320, 6000, 4000,
      {  86, 150, 214 }, { 214, 232, 246 }, { 255, 250, 230 },
      { 150, 170, 190 }, {  60, 130, 170 }, {  16,  60,  96 }, 120,  96, 26, false },
    /* PIC_LOFTS */
    { PAINT_TOWN, 480, 320, 6000, 4000,
      {  44,  50, 118 }, { 236, 122, 104 }, { 255, 214, 124 },
      {  88,  60, 100 }, {  52,  34,  70 }, { 120,  60,  70 },   0,   0,  0, false },
    /* PIC_ALFAMA */
    { PAINT_TOWN, 480, 320, 6000, 4000,
      { 148,  64, 132 }, { 248, 148, 112 }, {  74,  56,  82 },
      { 150,  90, 120 }, { 226, 202, 182 }, { 176,  84,  56 },  20,   0,  0, false },
    /* PIC_MIRADOURO */
    { PAINT_TOWN, 480, 320, 6000, 4000,
      { 130, 190, 240 }, { 240, 236, 220 }, {  70,  80, 100 },
      { 200, 205, 210 }, { 244, 232, 214 }, { 200,  98,  62 },  40,   0,  0, false },
    /* PIC_PALETTE: five bars over a white card, a color each. */
    { PAINT_SWATCHES, 480, 320, 2400, 1600,
      {  20,  74,  92 }, {  44, 122, 134 }, {  96, 170, 160 },
      { 168, 214, 196 }, { 226, 240, 226 }, { 255, 255, 255 },   0,   0,  0, false },
    /* PIC_STUDY: six pebbles on sand. */
    { PAINT_SWATCHES, 480, 320, 3000, 2000,
      {  60, 134, 150 }, { 120, 186, 170 }, { 176, 216, 186 },
      {  92, 150, 196 }, { 206, 170, 120 }, { 240, 232, 216 },   0,   0,  0, true },
    /* PIC_SCREEN */
    { PAINT_SCREEN, 480, 320, 2880, 1920,
      {  64,  84, 160 }, { 214, 120, 150 }, {   0,  99, 225 },
      { 236, 236, 238 }, { 244, 244, 247 }, { 228, 228, 232 },   0,   0,  0, false },
    /* PIC_MARK_BLUE */
    { PAINT_MARK, 320, 320, 2048, 2048,
      { 255, 255, 255 }, {  24,  86, 148 }, {  96, 170, 160 },
      {   0,   0,   0 }, {   0,   0,   0 }, {   0,   0,   0 }, 160, 160, 112, false },
    /* PIC_MARK_MONO */
    { PAINT_MARK, 320, 320, 512, 512,
      { 255, 255, 255 }, {  29,  29,  31 }, {  99,  99, 102 },
      {   0,   0,   0 }, {   0,   0,   0 }, {   0,   0,   0 }, 160, 160, 112, false },
    /* PIC_LOCKUP: the mark and its name on a sign's blue. */
    { PAINT_MARK, 480, 320, 4800, 3200,
      {  20,  62, 110 }, { 255, 255, 255 }, { 150, 210, 200 },
      { 255, 255, 255 }, { 150, 210, 200 }, {   0,   0,   0 }, 120, 160,  76, true },
    /* PIC_LOGO_V1: the draft the client turned down. */
    { PAINT_MARK, 320, 320, 1024, 1024,
      { 250, 240, 222 }, { 178,  66,  48 }, { 228, 150,  72 },
      {   0,   0,   0 }, {   0,   0,   0 }, {   0,   0,   0 }, 160, 160, 104, false },
    /* PIC_FIGURE */
    { PAINT_FIGURE, 480, 320, 2400, 1600,
      { 255, 255, 255 }, { 240, 130,  20 }, {  40, 110, 220 },
      { 228, 228, 232 }, {  60,  60,  66 }, {   0,   0,   0 },   0,   0,  0, false },
};

static inline void bmp_u32(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)( v        & 0xFFu);
    p[1] = (unsigned char)((v >>  8) & 0xFFu);
    p[2] = (unsigned char)((v >> 16) & 0xFFu);
    p[3] = (unsigned char)((v >> 24) & 0xFFu);
}

/* a + (b - a) * t / den, per channel. */
static inline Rgb mix(Rgb a, Rgb b, int t, int den) {
    Rgb c;

    c.r = a.r + (b.r - a.r) * t / den;
    c.g = a.g + (b.g - a.g) * t / den;
    c.b = a.b + (b.b - a.b) * t / den;
    return c;
}

static inline int clamp16(int v) { return v < 0 ? 0 : v > 16 ? 16 : v; }

/* Lay a disc over @p under, with an edge softened across one pixel. */
static inline Rgb over_disc(Rgb under, Rgb color, int x, int y, int cx, int cy, int radius) {
    int d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
    int r2 = radius * radius, band = 2 * radius;

    if (d2 <= r2 - band) return color;
    if (d2 >= r2 + band) return under;
    return mix(under, color, r2 + band - d2, 2 * band);
}

/* A sky from a to b, with a sun in c and a soft halo around it. */
static inline Rgb paint_sky(const Scene *s, int x, int y, int horizon) {
    Rgb c = mix(s->a, s->b, y < horizon ? y : horizon, horizon);

    if (s->r) {
        int d2 = (x - s->x) * (x - s->x) + (y - s->y) * (y - s->y);
        int r2 = s->r * s->r, glow = 4 * r2;

        if (d2 < glow) c = mix(c, s->c, glow - (d2 > r2 ? d2 : r2), 2 * (glow - r2));
        c = over_disc(c, s->c, x, y, s->x, s->y, s->r);
    }
    return c;
}

/* The row a ridge reaches at column x, in sixteenths of a pixel so its edge can be softened:
   a saw of peaks, or a chain of parabolic hills. phase shifts the pattern so two ridges do
   not line up. */
static inline int ridge_at(bool peaks, int x, int base, int amp, int phase) {
    if (peaks) {
        int k = (x + phase) % 160, d = k < 80 ? k : 160 - k;

        return 16 * base - 16 * amp * d / 80;
    }
    {
        int k = (x + phase) % 240, d = k - 120;

        return 16 * (base - amp) + 16 * amp * d * d / 14400;
    }
}

/* Land: a sky, d for the far ridge, e for the near one, darkening to f at the bottom edge.
   alt makes the ridges sharp peaks. */
static inline Rgb paint_land(const Scene *s, int x, int y) {
    const int horizon = 200;
    int       farY  = ridge_at(s->alt, x, horizon, 60, 34);
    int       nearY = ridge_at(s->alt, x, horizon + 48, 44, 122);
    int       farIn = 16 * y + 16 - farY, nearIn = 16 * y + 16 - nearY;
    Rgb       c = paint_sky(s, x, y, horizon);

    /* Each ridge covers a pixel by how far its edge reaches into it: sixteenths. */
    if (farIn >= 16)     c = s->d;
    else if (farIn > 0)  c = mix(c, s->d, farIn, 16);
    if (nearIn >= 16)    c = mix(s->e, s->f, y - nearY / 16, s->h - nearY / 16);
    else if (nearIn > 0) c = mix(c, s->e, nearIn, 16);
    return c;
}

/* Water: a sky, a far shore in d, a sea from e to f with the sun's path on it in c. alt
   drops the shore and runs a pier in d out from the left. */
static inline Rgb paint_sea(const Scene *s, int x, int y) {
    const int horizon = 190;
    Rgb       c = paint_sky(s, x, y, horizon);

    if (y >= horizon) {
        int depth = y - horizon, reach = s->r / 3 + depth / 3 + (depth * 13) % 11;
        int off   = x > s->x ? x - s->x : s->x - x;

        c = mix(s->e, s->f, depth, s->h - horizon);
        if (off < reach && (depth / 3) % 2 == 0) c = mix(c, s->c, 2, 5);
        if (y % 9 == 0 && (x + y * 7) % 72 < 40) c = mix(c, s->b, 1, 5);
    }
    if (s->alt) {
        if (x < 300 && y >= 172 && y < 182) c = s->d;
        if (x < 300 && y >= 182 && y < 262 - x / 8 && x % 44 < 6) c = s->d;
    } else {
        int k   = (x + 40) % 220, d = k < 110 ? k : 220 - k;
        int top = horizon - 3 - d / 9;

        if (y >= top && y < horizon) c = s->d;
    }
    return c;
}

/* A town: a sky, a far row of roofs in d, a near row of walls in e under pitched roofs in f,
   with a window grid in c. x slides the street along, so two towns differ. */
static inline Rgb paint_town(const Scene *s, int x, int y) {
    static const int HEIGHT[8] = { 96, 132, 84, 150, 110, 70, 140, 100 };
    const int        cell = 56;
    int              px   = x + s->x;
    int              farTop = s->h - 70 - HEIGHT[(px + 28) / cell % 8];
    int              col  = px / cell, wx = px % cell;
    int              top  = s->h - HEIGHT[(col + 3) % 8];
    int              off  = wx > cell / 2 ? wx - cell / 2 : cell / 2 - wx;
    Rgb              c    = mix(s->a, s->b, y, s->h);

    if (y >= farTop) c = s->d;
    if (y >= top - 22 + off * 22 / (cell / 2) && y < top) c = s->f;
    if (y >= top) {
        int wy = y - top;

        c = s->e;
        if (wx >= 8 && wx < 48 && wy >= 12 && (wx - 8) % 14 < 8 && (wy - 12) % 20 < 11
            && (col * 7 + (wy - 12) / 20 * 3 + (wx - 8) / 14 * 5) % 3)
            c = s->c;
    }
    return c;
}

/* A logo: a disc on a plain ground a, its upper part in b, the part below a scalloped wave
   in c. alt sets the name to its right, as a sign does: two bars in d and e. */
static inline Rgb paint_mark(const Scene *s, int x, int y) {
    Rgb c = s->a;

    if (s->alt && x >= s->x + s->r + 28) {
        if (x < s->w - 40 && y >= s->y - 30 && y < s->y - 4) c = s->d;
        if (x < s->w - 96 && y >= s->y + 10 && y < s->y + 22) c = s->e;
    }
    {
        /* How far below the wave's edge this row is, in sixteenths of a pixel: its two edges
           are softened like a ridge's. */
        int k    = (x - s->x + 8 * s->r) % s->r, d = k - s->r / 2;
        int wave = 16 * (s->y - s->r / 12) + 32 * d * d / (3 * s->r);
        int in   = 16 * y - wave, gap = 2 * s->r;
        Rgb ink  = in < gap / 2 ? mix(s->b, s->a, clamp16(in + 8), 16)
                                : mix(s->a, s->c, clamp16(in - gap + 8), 16);

        c = over_disc(c, ink, x, y, s->x, s->y, s->r);
    }
    return c;
}

/* A palette: five bars in a to e over a card in f, each with its label's line; alt is six
   round samples of those colors lying on a ground in f. */
static inline Rgb paint_swatches(const Scene *s, int x, int y) {
    const Rgb inks[6] = { s->a, s->b, s->c, s->d, s->e, mix(s->a, s->e, 1, 2) };
    Rgb       c = s->f;

    if (s->alt) {
        int col = x * 3 / s->w, row = y * 2 / s->h;

        return over_disc(c, inks[row * 3 + col], x, y, s->w * (2 * col + 1) / 6,
                         s->h * (2 * row + 1) / 4, 62);
    }
    {
        int col = x * 5 / s->w, wx = x - col * s->w / 5;

        if (y < 248) return inks[col];
        if (wx >= 14 && wx < 66 && y >= 268 && y < 276) return RC_LIT(Rgb){ 90, 90, 96 };
        if (wx >= 14 && wx < 48 && y >= 284 && y < 290) return RC_LIT(Rgb){ 170, 170, 176 };
    }
    return c;
}

/* A screen capture: a desktop from a to b, one open window (bar d, sidebar f, zebra e, its
   selected row in c) and a dock. */
static inline Rgb paint_screen(const Scene *s, int x, int y) {
    static const Rgb DOCK[6] = { { 40, 110, 220 }, { 240, 130, 20 }, { 52, 199, 89 },
                                 { 175, 82, 222 }, { 255, 69, 58 }, { 20, 150, 160 } };
    const Rgb        white = { 255, 255, 255 }, gray = { 176, 176, 182 };
    Rgb              c = mix(s->a, s->b, y, s->h);

    if (x >= 120 && x < 350 && y >= 284 && y < 312) {
        c = mix(c, white, 3, 5);
        if (x >= 136 && y >= 289 && y < 307 && (x - 136) % 36 < 18)
            c = DOCK[(x - 136) / 36 % 6];
    }
    if (x < 56 || x >= 424 || y < 28 || y >= 268) return c;
    if (y < 50) return s->d;
    if (x < 150) {
        c = s->f;
        if (y >= 58 && (y - 58) % 18 < 7 && x >= 66 && x < 134) c = gray;
        return c;
    }
    {
        int row = (y - 50) / 18;

        c = row == 3 ? s->c : row % 2 ? s->e : white;
        if ((y - 50) % 18 >= 6 && (y - 50) % 18 < 12 && x >= 162 && x < 230 + (row * 37) % 120)
            c = row == 3 ? white : gray;
    }
    return c;
}

/* A thesis figure: a plot on a ground in a, grid in d, axes in e, a trend in b, sites in c. */
static inline Rgb paint_figure(const Scene *s, int x, int y) {
    static const short SITES[14][2] = {
        { 84, 248 }, { 112, 206 }, { 138, 232 }, { 170, 188 }, { 196, 214 }, { 226, 160 },
        { 254, 184 }, { 282, 132 }, { 306, 158 }, { 334, 116 }, { 362, 138 }, { 390, 82 },
        { 414, 104 }, { 438, 58 },
    };
    Rgb c = s->a;
    int i;

    if (x >= 48 && x < 456 && y >= 24 && y < 280) {
        if ((x - 48) % 68 == 0 || (y - 24) % 64 == 0) c = s->d;
        {
            int line = 256 - (x - 48) * 196 / 408;

            if (y >= line - 1 && y <= line + 1) c = s->b;
        }
        for (i = 0; i < 14; i++) c = over_disc(c, s->c, x, y, SITES[i][0], SITES[i][1], 6);
    }
    if ((x >= 46 && x < 48 && y >= 24 && y < 282) || (y >= 280 && y < 282 && x >= 46 && x < 456))
        c = s->e;
    return c;
}

static inline Rgb paint_pixel(const Scene *s, int x, int y) {
    switch (s->paint) {
    case PAINT_SEA:      return paint_sea(s, x, y);
    case PAINT_TOWN:     return paint_town(s, x, y);
    case PAINT_MARK:     return paint_mark(s, x, y);
    case PAINT_SWATCHES: return paint_swatches(s, x, y);
    case PAINT_SCREEN:   return paint_screen(s, x, y);
    case PAINT_FIGURE:   return paint_figure(s, x, y);
    default:             return paint_land(s, x, y);
    }
}

/* Encode scene @p s into @p dst; returns the byte length, or 0 when @p cap is too small,
   which the caller treats as a failed load. */
static inline int sample_bmp(const Scene *s, unsigned char *dst, int cap) {
    const int total = 54 + s->w * s->h * 4;
    int       k, x, r;

    if (!dst || cap < total)
        return 0;
    for (k = 0; k < 54; k++)
        dst[k] = 0;
    dst[0] = 'B';
    dst[1] = 'M';
    bmp_u32(dst + 2,  (unsigned)total);             /* file size        */
    bmp_u32(dst + 10, 54u);                         /* pixel offset     */
    bmp_u32(dst + 14, 40u);                         /* info header size */
    bmp_u32(dst + 18, (unsigned)s->w);
    bmp_u32(dst + 22, (unsigned)s->h);              /* + = bottom-up    */
    dst[26] = 1;                                    /* planes           */
    dst[28] = 32;                                   /* bits per pixel   */
    bmp_u32(dst + 34, (unsigned)(s->w * s->h * 4)); /* image size       */

    for (r = 0; r < s->h; r++) {
        for (x = 0; x < s->w; x++) {
            unsigned char *o = dst + 54 + (r * s->w + x) * 4;
            Rgb            c = paint_pixel(s, x, s->h - 1 - r); /* file row r is image row h-1-r */

            o[0] = (unsigned char)c.b;
            o[1] = (unsigned char)c.g;
            o[2] = (unsigned char)c.r;
            o[3] = 255;
        }
    }
    return total;
}

#endif /* APP_SAMPLES_H */
