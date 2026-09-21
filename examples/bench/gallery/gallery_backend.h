/*
    gallery_backend.h - the gallery's model and its asset generator.

    Twelve "photographs", each generated in memory as a real 32-bit BMP, plus the
    metadata, dated sections, tag vocabulary, search filter and the display strings
    the UI draws. Pure C99, no RayClay dependency, no clock, no rand(), no I/O.

    32-bit BGRA is deliberate: w*4 is always 4-byte aligned, so BMP row padding is
    structurally impossible, and alpha is 255 everywhere, so the decode is opaque.

    Usage (define the implementation in exactly one TU):
        #define GALLERY_BACKEND_IMPLEMENTATION
        #include "gallery_backend.h"

    It also owns gallery_memzero, so the UI stays free of system includes. Decoded
    image handles are a RayClay type and live in the app, not here.
*/
#ifndef GALLERY_BACKEND_H
#define GALLERY_BACKEND_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef GALDEF
#define GALDEF static
#endif

#define GAL_IMG_COUNT    12               /* twelve photographs */
/* The source resolution of a "photograph", and a LOOK decision before a size one:
   the viewer shows a preview up to 380 px across. Every scene builder derives its
   features from w and h, so this adds pixels to a composition, never detail. */
#define GAL_IMG_MAXW     288
#define GAL_IMG_MAXH     288
#define GAL_BMP_CAP      (54 + GAL_IMG_MAXW * 4 * GAL_IMG_MAXH)  /* worst-case encoded size, 331830 */
#define GAL_TITLE_CAP    24
#define GAL_DIM_CAP      16               /* "288 x 192"              */
#define GAL_TAG_CAP      16               /* one tag label            */
#define GAL_DATE_CAP     16               /* "24 Aug 2026"            */
#define GAL_COUNT_CAP    16               /* "12 photos" / "3 results" */
#define GAL_CAPTION_CAP  257              /* 256-byte paste cap + NUL  */
#define GAL_SECT_COUNT   4                /* dated sections, newest first */
#define GAL_SECT_CAP     20               /* "September 2026"         */

/* Per-image record. All strings are precomputed fixed-width at seed. */
typedef struct {
    char     title[GAL_TITLE_CAP];        /* "Sunset Ridge"                          */
    char     dim[GAL_DIM_CAP];            /* precomputed "288 x 192"                 */
    char     tag[GAL_TAG_CAP];            /* one filter/display tag                  */
    char     date[GAL_DATE_CAP];          /* precomputed capture date "24 Aug 2026"  */
    char     caption[GAL_CAPTION_CAP];    /* default caption (the editable seed text) */
    int32_t  w, h;                        /* image pixel dimensions                  */
    uint8_t  pattern;                     /* which scene builder draws this image     */
    uint8_t  sect;                        /* the dated section this photo belongs to  */
    uint32_t hueA, hueB, hueC;            /* sky/zenith, horizon light, subject mass  */
} GalImage;

/* One dated run of the library. Both strings are built HERE, never in the UI.
   `first` indexes visible[], so a section is a contiguous span of it - which holds
   because the photo table is ordered by section. */
typedef struct {
    char     label[GAL_SECT_CAP];         /* "August 2026"                             */
    char     countStr[GAL_COUNT_CAP];     /* "4 photos" / "1 photo"                    */
    int32_t  first;                       /* index into visible[] of its first photo   */
    int32_t  count;                       /* photos of it passing the search (0 = gone) */
} GalSection;

/* The whole gallery model - a flat, memset-able POD (the app embeds it by value). */
typedef struct {
    GalImage img[GAL_IMG_COUNT];
    int32_t  visible[GAL_IMG_COUNT];      /* compacted indices matching the search   */
    int32_t  visibleCount;
    char     countStr[GAL_COUNT_CAP];     /* "12 photos" / "3 results" (recomputed on filter) */
    GalSection sect[GAL_SECT_COUNT];      /* the dated runs of visible[] (ditto)     */
} GalStore;

/* The non-inline API is declared AND defined only under the IMPLEMENTATION macro,
   so a TU that includes this header without defining it never sees a
   static-declared-but-undefined prototype. */
#ifdef GALLERY_BACKEND_IMPLEMENTATION

/* memzero for the pure-RC_ GUI TU (keeps <string.h> out of the GUI). */
GALDEF void gallery_memzero(void *p, size_t n);
/* Build the store: metadata + precomputed strings. Deterministic; `seed` is accepted
   for contract symmetry but the curated content is fixed. */
GALDEF void gallery_backend_seed(GalStore *s, unsigned seed);
/* Encode image `i` as a 32-bit BGRA BMP into `dst`. Returns the byte length, or 0 on
   any bounds/argument failure (the caller treats 0 / a NULL decode as "degrade to a
   placeholder", never a crash). */
GALDEF size_t gallery_encode_bmp(const GalStore *s, int i, unsigned char *dst, size_t cap);
/* Recompute the visible-index list for a search string and a tag (both matched
   case-insensitively, both empty meaning "everything"), the "n photos / n results"
   count string, and the span and count of every dated section. */
GALDEF void gallery_filter(GalStore *s, const char *search, const char *tag);
/* Copy image `i`'s caption into `dst` (bounded). Lets the pure-RC_ GUI TU load
   a caption without a libc string call. Out-of-range `i` writes an empty string. */
GALDEF void gallery_load_caption(const GalStore *s, int i, char *dst, int cap);
/* The other half, and the app is wrong without it: one edit buffer is shared by every
   photo, so whatever the user typed must go back to image `i` BEFORE the buffer is
   reloaded for another one. Out-of-range `i` is a no-op. */
GALDEF void gallery_store_caption(GalStore *s, int i, const char *src);

GALDEF void gallery_memzero(void *p, size_t n) {
    unsigned char *b = (unsigned char *)p;
    for (size_t i = 0; i < n; i++) b[i] = 0;
}

/* Bounded ASCII copy (dst always NUL-terminated); no libc. */
static void gal__str_copy(char *dst, const char *src, int cap) {
    int i = 0;
    if (cap <= 0) return;
    for (; src && src[i] && i < cap - 1; i++) dst[i] = src[i];
    dst[i] = '\0';
}

/* Append an unsigned int as decimal to dst[*pos], bounded. */
static void gal__put_uint(char *dst, int *pos, int cap, unsigned v) {
    char tmp[12];
    int n = 0;
    do { tmp[n++] = (char)('0' + v % 10u); v /= 10u; } while (v && n < 11);
    while (n > 0 && *pos < cap - 1) dst[(*pos)++] = tmp[--n];
    dst[*pos] = '\0';
}

/* "<n> photos" / "1 photo" into a fixed buffer - the toolbar count and every
   section count come through here, so the singular is spelled in ONE place. */
static void gal__count_str(char *dst, int cap, int n, const char *one, const char *many) {
    const char *word = (n == 1) ? one : many;
    int pos = 0;
    gal__put_uint(dst, &pos, cap, (unsigned)(n < 0 ? 0 : n));
    for (int k = 0; word[k] && pos < cap - 1; k++) dst[pos++] = word[k];
    dst[pos] = '\0';
}

/* "288 x 192" into a fixed buffer. */
static void gal__dim_str(char *dst, int cap, int w, int h) {
    int pos = 0;
    gal__put_uint(dst, &pos, cap, (unsigned)(w < 0 ? 0 : w));
    if (pos < cap - 3) { dst[pos++] = ' '; dst[pos++] = 'x'; dst[pos++] = ' '; dst[pos] = '\0'; }
    gal__put_uint(dst, &pos, cap, (unsigned)(h < 0 ? 0 : h));
}

static char gal__lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

/* Case-insensitive: does `hay` contain `needle`? Empty needle matches. No libc. */
static bool gal__contains_ci(const char *hay, const char *needle) {
    if (!needle || !needle[0]) return true;
    for (int i = 0; hay[i]; i++) {
        int j = 0;
        while (needle[j] && hay[i + j] &&
               gal__lower(hay[i + j]) == gal__lower(needle[j])) j++;
        if (!needle[j]) return true;
    }
    return false;
}

static void gal__u16le(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)(v & 0xFFu);
    p[1] = (unsigned char)((v >> 8) & 0xFFu);
}
static void gal__u32le(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)(v & 0xFFu);
    p[1] = (unsigned char)((v >> 8) & 0xFFu);
    p[2] = (unsigned char)((v >> 16) & 0xFFu);
    p[3] = (unsigned char)((v >> 24) & 0xFFu);
}

/* Blend two BGRA endpoints by t in [0,255] (per-channel, alpha forced opaque). */
static uint32_t gal__lerp_bgra(uint32_t a, uint32_t b, int t) {
    if (t < 0) t = 0; else if (t > 255) t = 255;
    uint32_t out = 0xFFu << 24;                       /* opaque alpha */
    for (int sh = 0; sh <= 16; sh += 8) {
        int ca = (int)((a >> sh) & 0xFFu);
        int cb = (int)((b >> sh) & 0xFFu);
        int c  = ca + (cb - ca) * t / 255;
        out |= (uint32_t)(c & 0xFF) << sh;
    }
    return out;
}

/* Move c toward white by k (0..255). Sun glow, lit windows, water specular. */
static uint32_t gal__lighten(uint32_t c, int k) {
    if (k <= 0) return c;
    if (k > 255) k = 255;
    uint32_t out = 0xFFu << 24;
    for (int sh = 0; sh <= 16; sh += 8) {
        int v = (int)((c >> sh) & 0xFFu);
        out |= (uint32_t)((v + (255 - v) * k / 255) & 0xFF) << sh;
    }
    return out;
}

/* Move c toward black by k (0 = unchanged, 255 = black). Vignette and shadow. */
static uint32_t gal__darken(uint32_t c, int k) {
    if (k <= 0) return c;
    if (k > 255) k = 255;
    uint32_t out = 0xFFu << 24;
    for (int sh = 0; sh <= 16; sh += 8) {
        int v = (int)((c >> sh) & 0xFFu);
        out |= (uint32_t)((v * (255 - k) / 255) & 0xFF) << sh;
    }
    return out;
}

/* A deterministic 0..255 hash of two lattice coordinates. */
static int gal__hash(int a, int b) {
    uint32_t h = (uint32_t)a * 374761393u + (uint32_t)b * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (int)((h ^ (h >> 16)) & 0xFFu);
}

/* Smooth 1-D value noise across `span`, sampled on `cells` lattice points. The
   smoothstep keeps a ridge rounded; raw linear interpolation looks like a saw. */
static int gal__noise1(int x, int span, int cells, int seed) {
    if (span  < 1) span  = 1;
    if (cells < 1) cells = 1;
    int pos = x * cells * 255 / span;
    int i   = pos / 255;
    int f   = pos % 255;
    int a   = gal__hash(i, seed);
    int b   = gal__hash(i + 1, seed);
    f = (int)(((long)f * f * (765 - 2 * f)) / 65025);         /* smoothstep, 0..255 */
    return a + (b - a) * f / 255;
}

/* THE FIVE SCENE BUILDERS. A rectangle reads as a photograph when it carries a
   horizon, a light source, a value structure AND detail at the pixel scale. Miss
   the last and it reads as a gradient however good the first three are, which is
   why every scene ends in gal__grain and why the boundaries below are hard edges.
   Integer arithmetic only, and it runs once at load, never in a frame. */

enum {
    GAL_SCENE_RIDGES = 0,    /* sun over two haze-separated ridge lines */
    GAL_SCENE_WATER,         /* sky above, banded reflection below      */
    GAL_SCENE_CITY,          /* night skyline, lit windows              */
    GAL_SCENE_SUBJECT,       /* one soft subject against a bokeh wash   */
    GAL_SCENE_FOREST,        /* trunks receding into mist               */
    GAL_SCENE_N
};

/* Sky shared by every outdoor scene: hueA at the zenith warming to hueB at the
   horizon, with a sun whose glow falls off as the square of the distance. */
static uint32_t gal__sky(const GalImage *m, int x, int y, int w, int horizon) {
    int span = horizon > 1 ? horizon - 1 : 1;
    int t    = y * 255 / span;
    uint32_t c = gal__lerp_bgra(m->hueA, m->hueB, t > 255 ? 255 : t);

    /* THE SUN IS A DISC WITH A CORE, NOT A HAZE: a wide linear falloff paints a
       soft white oval across a third of the frame, which reads as a lens smudge.
       A small radius with a quadratic falloff has a bright middle instead.
       AND IT MOVES BETWEEN FRAMES, hashed off the scene's own colours: a corpus
       whose light sits in the same spot in every sky reads as one picture
       recoloured, which is exactly the thing this generator has to avoid. */
    int place = gal__hash((int)(m->hueA & 0xFFu), (int)(m->hueC & 0xFFu));
    int sunX  = w * (12 + place * 76 / 255) / 100;
    int sunY  = horizon * (20 + gal__hash(place, 5) * 55 / 255) / 100;
    int dx = x - sunX, dy = (y - sunY) * 2;                   /* squash: a low sun flares wide */
    int d2 = dx * dx + dy * dy;
    int rad = w * w / 130; if (rad < 1) rad = 1;
    if (d2 < rad) {
        int k = 255 - d2 * 255 / rad;
        c = gal__lighten(c, k * k / 255);
    }
    return c;
}

/* A few units of luminance of per-pixel noise. Every scene ends here: nothing else
   in them carries detail at the pixel scale, and a frame with none reads as a
   render however well composed it is. */
static uint32_t gal__grain(uint32_t c, int x, int y, int amt) {
    int k = (gal__hash(x * 7 + y * 131, x - y) - 128) * amt / 128;
    return k >= 0 ? gal__lighten(c, k) : gal__darken(c, -k);
}

static uint32_t gal__scene_ridges(const GalImage *m, int x, int y, int w, int h) {
    int horizon = h * 62 / 100;
    /* Two noise octaves per ridge: the coarse one is the landform, the fine one is
       the relief on it. One octave alone draws a smooth curve, and a smooth curve
       is the tell that a mountain was computed. */
    int far  = horizon - gal__noise1(x, w, 4, 11) * h / 900
                       - gal__noise1(x, w, 15, 17) * h / 2600;
    int near = horizon + h * 14 / 100 - gal__noise1(x, w, 7, 29) * h / 700
                                      - gal__noise1(x, w, 23, 37) * h / 2200;

    if (y < far - 1) return gal__sky(m, x, y, w, horizon);
    /* A LIT RIM where the mass meets the sky. A ridge shot against the light has a
       bright edge a pixel or two deep, and that edge is what stops the boundary
       reading as a blend. */
    if (y < far + 1) return gal__lighten(gal__lerp_bgra(m->hueC, m->hueB, 150), 70);
    /* Distance haze: the far ridge keeps some sky in it, the near one almost none. */
    if (y < near)     return gal__lerp_bgra(m->hueC, m->hueB, 96 - (y - far) * 60 / (h > 1 ? h : 1));
    if (y < near + 1) return gal__lighten(gal__lerp_bgra(m->hueC, m->hueB, 60), 40);
    return gal__darken(m->hueC, 40 + (y - near) * 90 / (h > 1 ? h : 1));
}

static uint32_t gal__scene_water(const GalImage *m, int x, int y, int w, int h) {
    int horizon = h * 48 / 100;
    if (y < horizon - 1) return gal__sky(m, x, y, w, horizon);
    /* The far shore: one dark line, and the water below it stops being sky. */
    if (y < horizon + 1) return gal__darken(gal__sky(m, x, horizon - 1, w, horizon), 90);

    /* Mirror the sky downward, then break it into ripples that grow coarser and
       brighter with distance from the horizon. */
    int depth = y - horizon;
    int mir   = horizon - depth * 88 / 100;
    if (mir < 0) mir = 0;
    uint32_t c = gal__sky(m, x, mir, w, horizon);
    c = gal__darken(c, 28 + depth * 40 / (h > 1 ? h : 1));

    int cells = 8 + depth * 40 / (h > 1 ? h : 1);
    int band  = gal__noise1(x + depth * 3, w, cells, 53) - 128;   /* ripple, signed */
    int lit   = band * (48 + depth * 70 / (h > 1 ? h : 1)) / 128;
    return lit > 0 ? gal__lighten(c, lit) : gal__darken(c, -lit);
}

static uint32_t gal__scene_city(const GalImage *m, int x, int y, int w, int h) {
    int horizon = h * 72 / 100;
    if (y >= horizon) return gal__darken(m->hueC, 120);       /* foreground street mass */

    int bw   = w / 9; if (bw < 3) bw = 3;                     /* one tower per 1/9 width */
    int bx   = x / bw;
    int top  = horizon - (h * 22 / 100 + gal__hash(bx, 7) * h * 44 / 100 / 255);
    if (y < top) return gal__sky(m, x, y, w, horizon);

    /* A tower: dark mass, windows lit on a stable per-cell hash. The lattice is a
       FRACTION of the frame, so a larger source carries the same windows at more
       pixels each rather than more windows. */
    uint32_t c = gal__darken(m->hueC, 150 - gal__hash(bx, 3) / 6);
    int cw = w / 24; if (cw < 4) cw = 4;
    int ch = cw * 5 / 4;
    int wx = (x - bx * bw) % cw, wy = (horizon - y) % ch;
    if (wx < cw / 2 && wy < ch * 3 / 5 && gal__hash(bx * 31 + x / cw, y / ch) > 150)
        c = gal__lighten(c, 150);
    return c;
}

static uint32_t gal__scene_subject(const GalImage *m, int x, int y, int w, int h) {
    /* Background: a vertical wash carrying out-of-focus highlights (bokeh discs). */
    uint32_t c = gal__lerp_bgra(m->hueA, m->hueB, y * 255 / (h > 1 ? h - 1 : 1));
    for (int k = 0; k < 5; k++) {
        int ox = gal__hash(k, 17) * w / 255;
        int oy = gal__hash(k, 23) * h / 255;
        int dx = x - ox, dy = y - oy;
        int r  = w * 11 / 100; if (r < 2) r = 2;
        int d2 = dx * dx + dy * dy;
        if (d2 < r * r) c = gal__lighten(c, 70 - d2 * 70 / (r * r));
    }

    /* SUBJECT: an off-centre mass on the thirds. Two things make it a subject
       rather than a blob - a NEARLY HARD EDGE, because a lens in focus resolves
       one, and a light direction, so the mass has a lit side and a shaded one. */
    int cx = w * 38 / 100, cy = h * 56 / 100;
    int dx = x - cx, dy = y - cy;
    int rr = (w < h ? w : h) * 34 / 100; if (rr < 2) rr = 2;
    int d  = dx * dx + dy * dy;
    if (d < rr * rr) {
        int edge = 255 - d * 255 / (rr * rr);
        int lit  = 128 - (dx + dy) * 110 / rr;                /* lit from upper left */
        if (lit < 0) lit = 0; else if (lit > 255) lit = 255;
        uint32_t sub = gal__lerp_bgra(gal__darken(m->hueC, 120),
                                      gal__lighten(m->hueC, 70), lit);
        c = gal__lerp_bgra(c, sub, edge > 26 ? 255 : edge * 255 / 26);
    }
    /* Vignette: every lens has one, and it is what stops a render looking flat. */
    int vx = (x - w / 2) * 255 / (w > 1 ? w : 1);
    int vy = (y - h / 2) * 255 / (h > 1 ? h : 1);
    return gal__darken(c, (vx * vx + vy * vy) / 260);
}

static uint32_t gal__scene_forest(const GalImage *m, int x, int y, int w, int h) {
    /* Mist: brightest at the middle distance, which is what separates the trunks. */
    uint32_t c = gal__lerp_bgra(m->hueA, m->hueB, y * 255 / (h > 1 ? h - 1 : 1));
    if (y > h * 78 / 100)
        c = gal__darken(m->hueC, 60 + (y - h * 78 / 100) * 120 / (h > 1 ? h : 1));

    /* Trunks: six lattice positions, nearer ones wider and darker. The width is a
       FRACTION of the frame, and a deliberately small one - trunks wide enough to
       touch stop reading as a forest and start reading as a test pattern. */
    for (int k = 0; k < 6; k++) {
        int tx  = gal__hash(k, 41) * w / 255;
        int wd  = w / (gal__hash(k, 59) > 170 ? 30 : 60);
        if (wd < 1) wd = 1;
        int dep = gal__hash(k, 71);                           /* 0 far .. 255 near */
        if (x >= tx - wd && x <= tx + wd && y < h * 92 / 100) {
            int shade = 60 + dep * 120 / 255;
            int lit   = (x - (tx - wd)) * 255 / (2 * wd + 1); /* light from the left */
            c = gal__darken(m->hueC, shade - lit / 6);
        }
    }
    /* Canopy: the top of a forest frame is leaves, not open sky, and without it the
       trunks read as free-standing posts. */
    if (y < h * 28 / 100)
        c = gal__lerp_bgra(gal__darken(m->hueC, 30), c, y * 255 / (h * 28 / 100));
    return c;
}

/* BGRA at pixel (x,y) of a w*h image. Every divisor is floored to >= 1, so a 1 px
   image cannot divide by zero. */
static uint32_t gal__pixel(const GalImage *m, int x, int y, int w, int h) {
    uint32_t c;
    switch (m->pattern % GAL_SCENE_N) {
    case GAL_SCENE_WATER:   c = gal__scene_water(m, x, y, w, h);   break;
    case GAL_SCENE_CITY:    c = gal__scene_city(m, x, y, w, h);    break;
    case GAL_SCENE_SUBJECT: c = gal__scene_subject(m, x, y, w, h); break;
    case GAL_SCENE_FOREST:  c = gal__scene_forest(m, x, y, w, h);  break;
    default:                c = gal__scene_ridges(m, x, y, w, h);  break;
    }
    return gal__grain(c, x, y, 11);
}

GALDEF size_t gallery_encode_bmp(const GalStore *s, int i, unsigned char *dst, size_t cap) {
    if (!s || !dst || i < 0 || i >= GAL_IMG_COUNT) return 0;
    const GalImage *m = &s->img[i];
    int w = (int)m->w, h = (int)m->h;
    if (w <= 0 || h <= 0 || w > GAL_IMG_MAXW || h > GAL_IMG_MAXH) return 0;

    size_t pixels = (size_t)w * (size_t)h * 4u;               /* 32-bit: no row padding, ever */
    size_t total  = 54u + pixels;
    if (total > cap) return 0;

    for (size_t k = 0; k < 54; k++) dst[k] = 0;               /* zero both headers first */

    /* BITMAPFILEHEADER (14 bytes) */
    dst[0] = 'B'; dst[1] = 'M';
    gal__u32le(dst + 2,  (unsigned)total);                    /* bfSize                 */
    gal__u32le(dst + 10, 54u);                                /* bfOffBits              */
    /* BITMAPINFOHEADER (40 bytes) */
    gal__u32le(dst + 14, 40u);                                /* biSize                 */
    gal__u32le(dst + 18, (unsigned)w);                        /* biWidth                */
    gal__u32le(dst + 22, (unsigned)h);                        /* biHeight (+ = bottom-up) */
    gal__u16le(dst + 26, 1u);                                 /* biPlanes               */
    gal__u16le(dst + 28, 32u);                                /* biBitCount             */
    gal__u32le(dst + 30, 0u);                                 /* biCompression = BI_RGB */
    gal__u32le(dst + 34, (unsigned)pixels);                   /* biSizeImage            */
    /* 38..53 (ppm x/y, clrUsed, clrImportant) stay zero. */

    /* Pixels: bottom-up BGRA rows. File row r (0 = bottom) is image y = h-1-r. */
    unsigned char *px = dst + 54;
    for (int r = 0; r < h; r++) {
        int y = h - 1 - r;
        for (int x = 0; x < w; x++) {
            uint32_t c = gal__pixel(m, x, y, w, h);
            unsigned char *o = px + ((size_t)r * (size_t)w + (size_t)x) * 4u;
            o[0] = (unsigned char)(c & 0xFFu);                /* B */
            o[1] = (unsigned char)((c >> 8) & 0xFFu);         /* G */
            o[2] = (unsigned char)((c >> 16) & 0xFFu);        /* R */
            o[3] = 0xFFu;                                     /* A - opaque */
        }
    }
    return total;
}

/* Curated content, indexed by image. Kept fixed so the app is byte-deterministic. */
typedef struct {
    const char *title, *tag, *date, *caption;
    int w, h;
    uint8_t  pat;                         /* GAL_SCENE_*                           */
    uint8_t  sect;                        /* the dated section, 0 = the newest     */
    uint32_t a, b, c;                     /* zenith, horizon light, subject mass   */
} GalSeed;

/* The section headings, newest first. A library reads newest-to-oldest, so the
   table below is ordered the same way and `sect` never decreases down it: that is
   what lets a section be a contiguous span of visible[] (see GalSection). */
static const char *const GAL__SECT_LABELS[GAL_SECT_COUNT] = {
    "August 2026", "July 2026", "June 2026", "May 2026",
};

/* The filter vocabulary the toolbar offers, label and query side by side. Entry 0
   is the empty query, so "everything" needs no special case anywhere. The labels
   are also what a tag pill in the viewer reads, one letter of case apart. */
#define GAL_TAG_COUNT 6
static const char *const GAL_TAG_LABELS[GAL_TAG_COUNT] = {
    "All photos", "Landscape", "Nature", "Night", "Macro", "Abstract"
};
static const char *const GAL_TAG_QUERIES[GAL_TAG_COUNT] = {
    "", "landscape", "nature", "night", "macro", "abstract"
};

/* Colour literals are 0xAARRGGBB. The three roles are physical, and swapping the
   first two is what makes a scene look wrong rather than merely recoloured:
     zenith  - the sky straight up, the DEEPER and cooler colour;
     horizon - the sky at the skyline, LIGHTER and warmer, because the light comes
               from there and a sky that darkens downward reads as a painting;
     mass    - the lit colour of the land, tower or subject before shading. */
static const GalSeed GAL__SEEDS[GAL_IMG_COUNT] = {
    /* title           tag          date           caption                               w    h  scene             s  zenith      horizon     mass       */
    { "Sunset Ridge", "landscape", "24 Aug 2026", "Golden light over the far ridge.",  288, 192, GAL_SCENE_RIDGES,  0, 0xFF3A4A80u, 0xFFFFA050u, 0xFF2A2018u },
    { "Ocean Drift",  "landscape", "21 Aug 2026", "",                                  288, 192, GAL_SCENE_WATER,   0, 0xFF4A6A9Au, 0xFFFFC878u, 0xFF1A2838u },
    { "City Lights",  "night",     "16 Aug 2026", "Downtown after the rain.",          288, 192, GAL_SCENE_CITY,    0, 0xFF101828u, 0xFF4A3A5Au, 0xFF0A0A10u },
    { "Forest Path",  "nature",    "9 Aug 2026",  "",                                  192, 288, GAL_SCENE_FOREST,  0, 0xFF2A4A30u, 0xFFD0E8D8u, 0xFF1A2A1Au },
    { "Desert Dune",  "landscape", "30 Jul 2026", "Endless ridgelines of sand.",       288, 192, GAL_SCENE_RIDGES,  1, 0xFF5A8AC8u, 0xFFF0D8A8u, 0xFFC89A5Au },
    { "Aurora Sky",   "night",     "22 Jul 2026", "",                                  288, 192, GAL_SCENE_RIDGES,  1, 0xFF102040u, 0xFF40E080u, 0xFF0A1018u },
    { "Still Water",  "nature",    "14 Jul 2026", "A quiet morning reflection.",       240, 240, GAL_SCENE_WATER,   1, 0xFF6A8AB0u, 0xFFF0E0C0u, 0xFF201810u },
    { "Autumn Leaf",  "macro",     "26 Jun 2026", "",                                  240, 240, GAL_SCENE_SUBJECT, 2, 0xFF3A2A18u, 0xFF6A4A28u, 0xFFE07020u },
    { "Blue Hour",    "landscape", "19 Jun 2026", "The last blue before dark.",        288, 192, GAL_SCENE_CITY,    2, 0xFF182848u, 0xFFD08050u, 0xFF101828u },
    { "Marble",       "abstract",  "8 Jun 2026",  "",                                  240, 240, GAL_SCENE_WATER,   2, 0xFF505050u, 0xFFA8A8A8u, 0xFFF0F0F0u },
    { "Neon Grid",    "abstract",  "23 May 2026", "Synth grid, endless horizon.",      288, 192, GAL_SCENE_CITY,    3, 0xFF200838u, 0xFFC030A0u, 0xFF100818u },
    { "Soft Focus",   "macro",     "12 May 2026", "",                                  192, 288, GAL_SCENE_SUBJECT, 3, 0xFF204058u, 0xFF80A8C0u, 0xFFF0C070u },
};

GALDEF void gallery_backend_seed(GalStore *s, unsigned seed) {
    (void)seed;                                               /* content is curated + fixed */
    gallery_memzero(s, sizeof *s);
    for (int i = 0; i < GAL_IMG_COUNT; i++) {
        const GalSeed *g = &GAL__SEEDS[i];
        GalImage *m = &s->img[i];
        gal__str_copy(m->title, g->title, GAL_TITLE_CAP);
        gal__str_copy(m->tag, g->tag, GAL_TAG_CAP);
        gal__str_copy(m->date, g->date, GAL_DATE_CAP);
        gal__str_copy(m->caption, g->caption, GAL_CAPTION_CAP);
        m->w = g->w; m->h = g->h; m->pattern = g->pat;
        m->sect = (uint8_t)(g->sect % GAL_SECT_COUNT);
        m->hueA = g->a; m->hueB = g->b; m->hueC = g->c;
        gal__dim_str(m->dim, GAL_DIM_CAP, g->w, g->h);
    }
    for (int k = 0; k < GAL_SECT_COUNT; k++)
        gal__str_copy(s->sect[k].label, GAL__SECT_LABELS[k], GAL_SECT_CAP);
    gallery_filter(s, "", "");                                /* all visible initially */
}

GALDEF void gallery_filter(GalStore *s, const char *search, const char *tag) {
    s->visibleCount = 0;
    for (int k = 0; k < GAL_SECT_COUNT; k++) {
        s->sect[k].first = 0;
        s->sect[k].count = 0;
    }
    for (int i = 0; i < GAL_IMG_COUNT; i++) {
        if (!gal__contains_ci(s->img[i].title, search)) continue;
        if (!gal__contains_ci(s->img[i].tag, tag)) continue;
        /* The photo table is ordered by section, so a section's matches land as one
           run of visible[]: record where the run starts, then just count it. */
        GalSection *sec = &s->sect[s->img[i].sect % GAL_SECT_COUNT];
        if (sec->count == 0) sec->first = s->visibleCount;
        sec->count++;
        s->visible[s->visibleCount++] = i;
    }
    for (int k = 0; k < GAL_SECT_COUNT; k++)
        gal__count_str(s->sect[k].countStr, GAL_COUNT_CAP, s->sect[k].count,
                       " photo", " photos");
    /* "<n> photos" when nothing is narrowing the wall, "<n> results" when
       something is. */
    bool filtering = (search && search[0]) || (tag && tag[0]);
    gal__count_str(s->countStr, GAL_COUNT_CAP, s->visibleCount,
                   filtering ? " result" : " photo",
                   filtering ? " results" : " photos");
}

GALDEF void gallery_load_caption(const GalStore *s, int i, char *dst, int cap) {
    if (!dst || cap <= 0) return;
    if (!s || i < 0 || i >= GAL_IMG_COUNT) { dst[0] = '\0'; return; }
    gal__str_copy(dst, s->img[i].caption, cap);
}

GALDEF void gallery_store_caption(GalStore *s, int i, const char *src) {
    if (!s || !src || i < 0 || i >= GAL_IMG_COUNT) return;
    gal__str_copy(s->img[i].caption, src, GAL_CAPTION_CAP);
}

#endif /* GALLERY_BACKEND_IMPLEMENTATION */
#endif /* GALLERY_BACKEND_H */
