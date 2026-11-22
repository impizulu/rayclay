/*
    gallery_backend.h - the library's model: photos, places, albums and where each
    photo sits on the wall.

    Ninety-six photographs from thirteen trips, built from one table at seed time, so
    their dates, camera data and albums all agree and the app needs no files. Three
    things here are worth reading on their own:

      gallery_filter        one call narrows the library by scope, album and search, and
                            recomputes the dated runs and every display string from it
      gal_wall_flow         a justified grid: rows of photos at their own aspect, scaled
                            so each row meets both window edges, as Google Photos does
                            (gal_wall_grid is the phone's: squares, as iOS Photos does)
      gal_edit_apply        crop, turn and tone: the pixel work behind the viewer's editor

    Plain C with no clock, no rand() and no I/O; only its struct literals use RayClay,
    through RC_LIT. Usage, in exactly one TU, after rayclay.h:
        #define GALLERY_BACKEND_IMPLEMENTATION
        #include "gallery_backend.h"
*/
#ifndef GALLERY_BACKEND_H
#define GALLERY_BACKEND_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define GAL_PHOTO_COUNT 96
#define GAL_SHOOT_COUNT 13
#define GAL_ALBUM_COUNT 6
#define GAL_MEMORY_COUNT 5
#define GAL_YEAR_MAX    4

#define GAL_TINY_EDGE   24     /* the blurred stand-in every tile opens with    */
#define GAL_THUMB_EDGE  320    /* a tile's picture at its smallest; it doubles  */
#define GAL_FULL_EDGE   1280   /* up to the viewer's: the same shot, sharper    */

#define GAL_PICTURE_VERSION "v8"   /* bump when photo_gen.h changes what it draws */
#define GAL_CAPTION_CAP 200
#define GAL_SEARCH_CAP  32

/* An en dash and a middle dot, written so a following letter cannot join the escape. */
#define GAL_NDASH "\xe2\x80\x93"
#define GAL_DOT   "\xc2\xb7"

typedef enum {
    GAL_SCOPE_LIBRARY = 0,
    GAL_SCOPE_FAVORITES,
    GAL_SCOPE_RECENTS,
    GAL_SCOPE_DELETED,
    GAL_SCOPE_ALBUM
} GalScope;

typedef enum { GAL_CROP_ORIGINAL = 0, GAL_CROP_SQUARE, GAL_CROP_4_3, GAL_CROP_3_2, GAL_CROP_16_9,
              GAL_CROP_COUNT } GalCrop;

/* What the editor changes. Zeroed is the photograph as shot. */
typedef struct {
    int   turns;        /* quarter turns clockwise, 0 to 3  */
    int   crop;         /* GalCrop                          */
    float exposure;     /* -1 to 1                          */
    float contrast;     /* -1 to 1                          */
    float saturation;   /* -1 to 1                          */
    float warmth;       /* -1 to 1                          */
    float vignette;     /*  0 to 1                          */
} GalEdit;

typedef struct {
    char     title[28];
    char     place[32];
    char     date[16];          /* "Aug 12, 2026"              */
    char     when[32];          /* "Aug 12, 2026 at 7:41 AM"   */
    char     file[16];          /* "IMG_4127.HEIC"             */
    char     camera[20];
    char     lens[32];          /* "Main Camera, 24 mm f/1.78" */
    char     specs[40];         /* "24 MP, 6000 x 4000, 8.4 MB" */
    char     iso[10], focal[10], ev[8], fstop[8], shutter[10];
    char     caption[GAL_CAPTION_CAP];
    int      shoot, spot, year;
    float    sourceAspect;      /* width / height as shot      */
    float    aspect;            /* width / height as edited    */
    uint32_t albums;            /* bit per album               */
    unsigned seed;
    bool     favorite, deleted;
    GalEdit  edit;
} GalPhoto;

/* One unbroken run of the wall from one trip: a card in Months, a label in All Photos. */
typedef struct {
    char label[20];             /* "August 2026"                */
    char place[32];             /* "Reykjavik, Iceland"         */
    char range[32];             /* "Aug 9 - 14, 2026"           */
    char count[16];             /* "10 Photos"                  */
    int  first, n, cover, year;
} GalRun;

typedef struct {
    char label[8];              /* "2026"                       */
    char places[56];            /* "Kyoto, Reykjavik & 3 more"  */
    char count[16];
    int  firstRun, runs, firstPhoto, n;
} GalYear;

typedef struct {
    char count[8];
    int  n, cover;
} GalAlbum;

/* A place on the wall, in layout px from the content's top left. */
typedef struct { float x, y, w, h; } GalRect;

/* A wall state per photo. A folded photo flew into its month's cover and draws nothing. */
enum { GAL_WALL_ABSENT = 0, GAL_WALL_SHOWN, GAL_WALL_FOLDED };

typedef struct {
    GalRect photo[GAL_PHOTO_COUNT];
    uint8_t state[GAL_PHOTO_COUNT];
    GalRect head[GAL_PHOTO_COUNT];   /* a year's heading, or a month's card, by run or year */
    int     headCount;
    float   height;                  /* the content's height */
} GalWall;

typedef struct {
    GalPhoto photo[GAL_PHOTO_COUNT];
    int      visible[GAL_PHOTO_COUNT];
    int      visibleCount;
    int      keep;                   /* always listed, or -1: the open photo, while it is open */
    GalRun   run[GAL_SHOOT_COUNT];
    int      runCount;
    GalYear  year[GAL_YEAR_MAX];
    int      yearCount;
    GalAlbum album[GAL_ALBUM_COUNT];
    int      favorites, recents, deleted, memoryCover[GAL_MEMORY_COUNT];
    int      favoritesCover, recentsCover;
    char     countStr[16];           /* "96 Photos" / "3 Results" */
    char     favoritesStr[8], recentsStr[8], deletedStr[8], libraryStr[8];
    char     range[32];              /* "Jan 10, 2025 - Oct 24, 2026" */
} GalStore;

/* A memory is a story told over one trip's photos, line by line; the film (shoot -1) is the
   same story told by a video through rcMediaOpen. */
#define GAL_STORY_LINES 4
typedef struct {
    const char *title, *subtitle;
    int         shoot;
    const char *lines[GAL_STORY_LINES];
} GalMemory;

#ifdef GALLERY_BACKEND_IMPLEMENTATION

#include "photo_gen.h"

/* The film's captions: each language is one <track>, timed against the playhead. */
typedef struct { double start, end; } GalCue;
#define GAL_FILM_LINES  4
#define GAL_FILM_TRACKS 4

static const GalCue GAL_FILM_CUES[GAL_FILM_LINES] = {
    { 0.4, 2.9 }, { 3.2, 5.9 }, { 6.2, 8.9 }, { 9.2, 11.6 },
};
/* UTF-8 by octal escape, which stops at three digits: a hex escape would take the "a" of
   "Fran\303\247ais" into itself. */
static const char *const GAL_FILM_LANG[GAL_FILM_TRACKS] = { "en", "fr", "es", "de" };
static const char *const GAL_FILM_TRACK_NAME[GAL_FILM_TRACKS] = {
    "English", "Fran\303\247ais", "Espa\303\261ol", "Deutsch",
};
static const char *const GAL_FILM_TEXT[GAL_FILM_TRACKS][GAL_FILM_LINES] = {
    { "Before dawn, the harbor is still.", "A first line of light finds the water.",
      "The sun lifts clear of the sea.", "And the day begins." },
    { "Avant l'aube, le port est immobile.", "Un premier rayon de lumi\303\250re touche l'eau.",
      "Le soleil se l\303\250ve sur la mer.", "Et le jour commence." },
    { "Antes del alba, el puerto est\303\241 en calma.", "Un primer rayo de luz toca el agua.",
      "El sol se alza sobre el mar.", "Y comienza el d\303\255a." },
    { "Vor dem Morgengrauen ruht der Hafen.", "Ein erstes Licht f\303\244llt aufs Wasser.",
      "Die Sonne steigt aus dem Meer.", "Und der Tag beginnt." },
};

static const GalMemory GAL_MEMORIES[GAL_MEMORY_COUNT] = {
    { "Harbor at Dawn", "Film " GAL_DOT " 0:12", -1, { 0, 0, 0, 0 } },
    { "Ring Road", "Iceland " GAL_DOT " August 2026", 1,
      { "We drove south with the windows down.", "Skogafoss was loud enough to feel in your chest.",
        "The black sand at Reynisfjara still held the rain.",
        "Nobody wanted to be the first to turn back." } },
    { "Lavender Week", "Provence " GAL_DOT " June 2026", 2,
      { "The fields start where the road ends.", "By six the whole plateau turns violet.",
        "We ate dinner on the hood of the car.", "The smell stayed in our clothes for days." } },
    { "Northern Lights", "Tromso " GAL_DOT " February 2026", 4,
      { "Four nights of cloud, then this.", "It started as a pale arc over the fjord.",
        "Then the whole sky began to move.", "We stayed out until the batteries died." } },
    { "Dolomites", "Italy " GAL_DOT " July 2025", 7,
      { "First light hits the Tre Cime at five.", "The lake was so still it looked poured.",
        "We walked until the path ran out.", "Then we walked back the long way." } },
};

static const char *const GAL_ALBUM_NAMES[GAL_ALBUM_COUNT] = {
    "Iceland", "Provence", "Mountains", "Flowers", "Night Skies", "City Lights",
};

/* The scene an album's cover shows where it has one: what its name promises, and no two alike,
   so Flowers opens on a bloom and Iceland on its falls while Mountains keeps the peaks. */
static const int GAL_ALBUM_SCENE[GAL_ALBUM_COUNT] = {
    PG_FALLS, PG_FIELD, PG_ALPINE, PG_FLOWER, PG_AURORA, PG_SKYLINE,
};

static const char *const GAL_CROP_NAMES[GAL_CROP_COUNT] = {
    "Original", "Square", "4:3", "3:2", "16:9",
};

/* The film's two files: rcMediaOpen reads a relative path from the working directory. Run
   from the repository root or the build folder; the web build preloads them under
   /assets/media/ and says so with -DSTORY_DIR. */
#ifndef STORY_DIR
#define STORY_DIR "examples/assets/media/"
#endif

/* The app's own sources name no libc call: the backend lends them memset. */
static void gal_zero(void *p, size_t size) { memset(p, 0, size); }

static void gal_copy(char *dst, const char *src, int cap) {
    int i = 0;
    for (; src && src[i] && i < cap - 1; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

static void gal_put(char *dst, int *pos, int cap, const char *src) {
    for (int i = 0; src[i] && *pos < cap - 1; i++)
        dst[(*pos)++] = src[i];
    dst[*pos] = '\0';
}

static void gal_put_int(char *dst, int *pos, int cap, int v) {
    char tmp[12];
    int  n = 0;
    if (v < 0) {
        gal_put(dst, pos, cap, "-");
        v = -v;
    }
    do {
        tmp[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < 11);
    while (n > 0 && *pos < cap - 1)
        dst[(*pos)++] = tmp[--n];
    dst[*pos] = '\0';
}

static char gal_lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

static bool gal_contains(const char *hay, const char *needle) {
    if (!needle || !needle[0])
        return true;
    for (int i = 0; hay[i]; i++) {
        int j = 0;
        while (needle[j] && hay[i + j] && gal_lower(hay[i + j]) == gal_lower(needle[j]))
            j++;
        if (!needle[j])
            return true;
    }
    return false;
}

static void gal_count(char *dst, int cap, int n, const char *one, const char *many) {
    int pos = 0;
    gal_put_int(dst, &pos, cap, n);
    gal_put(dst, &pos, cap, n == 1 ? one : many);
}

/* ---- the catalog ---- */

enum {
    L_ALP_MORNING, L_ALP_GLOW, L_ALP_ICE,
    L_FOG_BLUE, L_FOG_GOLD, L_FOG_AUTUMN,
    L_COAST_SUNSET, L_COAST_DAWN, L_COAST_DAY,
    L_BEACH,
    L_DUNE_NOON, L_DUNE_DUSK,
    L_LAVENDER, L_TULIP,
    L_CITY_BLUE, L_CITY_DUSK,
    L_AURORA_GREEN, L_AURORA_VIOLET,
    L_BLOOM_PINK, L_BLOOM_SUN, L_BLOOM_WHITE, L_BLOOM_RED,
    L_LIGHTS_WARM, L_LIGHTS_CITY,
    L_FALLS,
    L_COFFEE,
    L_COUNT
};

static const PgStyle GAL_LOOKS[L_COUNT] = {
    /* zenith, mid, horizon, sun, cloudLit, cloudDark,
       massLit, massShade, haze, extra, extra2, sunX, sunY, power, cloud, snow */
    [L_ALP_MORNING] = {
        0x2a6db8, 0x7ab0de, 0xf0e6d2, 0xfff2d0, 0xffffff, 0xb7c6d8,
        0x8f8d8a, 0x414a56, 0xa7c2dc, 0x2e6a8e, 0x23402e, 0.72f, 0.12f, 0.55f, 0.56f, 1.0f },
    [L_ALP_GLOW] = {
        0x2c3a6e, 0xc0738a, 0xffc58a, 0xffcf9a, 0xffc8a8, 0x6a5a7c,
        0xe8926a, 0x56405e, 0xc98e96, 0x5a5a86, 0x2a2a3a, 0.25f, 0.34f, 0.65f, 0.58f, 0.7f },
    [L_ALP_ICE] = {
        0x4f7ea6, 0xa9c3d2, 0xece6da, 0xffffff, 0xeef2f4, 0x8b9aa4,
        0x6a7076, 0x1c2630, 0xb0c6d4, 0x5a7a8a, 0x2f4030, 0.50f, 0.08f, 0.45f, 0.47f, 1.0f },
    [L_FOG_BLUE] = {
        0xa8bdc9, 0xc6d5dc, 0xdfe8ea, 0xffffff, 0xeef3f4, 0xb4c3ca,
        0x2e5a4a, 0x0f2a24, 0xcfdde2, 0x000000, 0x000000, 0.60f, 0.25f, 0.35f, 1.0f, 0.0f },
    [L_FOG_GOLD] = {
        0x8da6b8, 0xe8c79a, 0xfbe2b2, 0xfff0c0, 0xffffff, 0xcfc4b0,
        0x3a5a32, 0x12261a, 0xf0d9ac, 0x000000, 0x000000, 0.35f, 0.34f, 0.75f, 1.0f, 0.0f },
    [L_FOG_AUTUMN] = {
        0x9cb7c9, 0xd3dbd6, 0xece6d4, 0xffeecb, 0xffffff, 0xcfd0c8,
        0xc8682a, 0x5e2616, 0xe0d4c0, 0xd8a838, 0x34482c, 0.70f, 0.20f, 0.40f, 1.0f, 0.0f },
    [L_COAST_SUNSET] = {
        0x28386e, 0xd8657c, 0xffb35e, 0xffc878, 0xffb08a, 0x5c4a78,
        0x2a2438, 0x1a1626, 0x8c5a78, 0x6a4f7c, 0x15203e, 0.62f, 0.47f, 0.85f, 0.55f, 0.0f },
    [L_COAST_DAWN] = {
        0x5f7fc0, 0xe5a8ba, 0xffe2c4, 0xfff0d8, 0xffe0d4, 0x8b86ac,
        0x3a4060, 0x2c3450, 0xa99ab8, 0x8a90b8, 0x28395e, 0.38f, 0.50f, 0.70f, 0.60f, 0.0f },
    [L_COAST_DAY] = {
        0x1c6cc2, 0x67b0e8, 0xe9f4fb, 0xffffff, 0xffffff, 0xbcccdc,
        0x4c5a48, 0x3c4a3c, 0xa9c6da, 0x2a7fb8, 0x0b3f68, 0.70f, 0.10f, 0.25f, 0.58f, 0.0f },
    [L_BEACH] = {
        0x1f78cf, 0x6fbcef, 0xeaf6fb, 0xffffff, 0xffffff, 0xc5d4e0,
        0x1fc4c0, 0xd9c08e, 0xa9d4ea, 0x0f5fa8, 0x9fe8d8, 0.80f, 0.08f, 0.30f, 0.60f, 0.0f },
    [L_DUNE_NOON] = {
        0x1a5cae, 0x5ba0de, 0xeadcbe, 0xffffff, 0xffffff, 0xcfd6dc,
        0xf2b05e, 0x7c3a1c, 0xe8cfa6, 0x000000, 0x000000, 0.80f, 0.06f, 0.30f, 1.0f, 0.0f },
    [L_DUNE_DUSK] = {
        0x3a4a86, 0xd98a7a, 0xffd09a, 0xffb070, 0xffffff, 0xcfd6dc,
        0xf08a48, 0x4c2230, 0xe0a080, 0x000000, 0x000000, 0.20f, 0.33f, 0.60f, 1.0f, 0.0f },
    [L_LAVENDER] = {
        0x3f67b0, 0xa3b4e0, 0xffdcb8, 0xffe0b0, 0xffe6d0, 0x8b7fa8,
        0xb48ae8, 0x4a2f7c, 0xc9b4d8, 0x2f4a30, 0x000000, 0.70f, 0.36f, 0.60f, 0.57f, 0.0f },
    [L_TULIP] = {
        0x2f7ad0, 0x88c0ee, 0xeef6f8, 0xffffff, 0xffffff, 0xc3d0dc,
        0xe8344a, 0x7c1830, 0xcfe0d8, 0x3a6a30, 0xff8a2a, 0.20f, 0.10f, 0.30f, 0.58f, 0.0f },
    [L_CITY_BLUE] = {
        0x14254e, 0x33508e, 0xe08a5a, 0xffb080, 0xffffff, 0xffffff,
        0x1c2a44, 0x0b1424, 0x445a86, 0xffcf7a, 0xffe9c0, 0.75f, 0.60f, 0.50f, 1.0f, 0.0f },
    [L_CITY_DUSK] = {
        0x3a3a78, 0xc46a7e, 0xffb366, 0xffc888, 0xffb09a, 0x4a3a66,
        0x2a2a44, 0x12121f, 0x8a6080, 0xffd68a, 0xfff0d0, 0.30f, 0.58f, 0.70f, 0.60f, 0.0f },
    [L_AURORA_GREEN] = {
        0x050b1c, 0x0a1830, 0x12263e, 0x000000, 0x000000, 0x000000,
        0x3af0a0, 0x060a12, 0x000000, 0x6a4ad8, 0x000000, 0.50f, 0.50f, 0.00f, 1.0f, 0.0f },
    [L_AURORA_VIOLET] = {
        0x060a20, 0x101438, 0x1a1c48, 0x000000, 0x000000, 0x000000,
        0x6af0c8, 0x07081a, 0x000000, 0xc050e0, 0x000000, 0.50f, 0.50f, 0.00f, 1.0f, 0.0f },
    [L_BLOOM_PINK] = {
        0x2f4a2a, 0x5f7f45, 0x1f3420, 0x000000, 0x000000, 0x000000,
        0xffb7cf, 0xc2185b, 0x000000, 0xe0a020, 0x6a4a10, 0.50f, 0.50f, 0.00f, 1.0f, 0.0f },
    [L_BLOOM_SUN] = {
        0x3a6a8a, 0x7fb0c8, 0x2c5a3a, 0x000000, 0x000000, 0x000000,
        0xffe26a, 0xe08a10, 0x000000, 0x5a3a10, 0xb07a20, 0.46f, 0.52f, 0.00f, 1.0f, 0.0f },
    [L_BLOOM_WHITE] = {
        0x20382a, 0x4a6a4a, 0x16281e, 0x000000, 0x000000, 0x000000,
        0xffffff, 0xd9d4e4, 0x000000, 0xffcf30, 0xd08a10, 0.54f, 0.48f, 0.00f, 1.0f, 0.0f },
    [L_BLOOM_RED] = {
        0x6d8f4a, 0xa8c070, 0x3f5a2a, 0x000000, 0x000000, 0x000000,
        0xff5a3a, 0x9c1010, 0x000000, 0x20140c, 0x4a3a10, 0.50f, 0.50f, 0.00f, 1.0f, 0.0f },
    [L_LIGHTS_WARM] = {
        0x0c0a10, 0x000000, 0x1a1210, 0x000000, 0x000000, 0x000000,
        0xffb648, 0x000000, 0x000000, 0xff5a3a, 0xffe0a0, 0.50f, 0.50f, 0.00f, 1.0f, 0.0f },
    [L_LIGHTS_CITY] = {
        0x080c18, 0x000000, 0x101828, 0x000000, 0x000000, 0x000000,
        0x5ab0ff, 0x000000, 0x000000, 0xff6a8a, 0xffcc66, 0.50f, 0.50f, 0.00f, 1.0f, 0.0f },
    [L_FALLS] = {
        0xa8bcc8, 0xc8d4da, 0xe4eaec, 0xffffff, 0xffffff, 0xc0ccd2,
        0x66625c, 0x202226, 0xe4ecec, 0x2a5a60, 0x4e5a38, 0.50f, 0.02f, 0.20f, 1.0f, 0.0f },
    [L_COFFEE] = {
        0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
        0xb5794a, 0x5c3620, 0x000000, 0x3c2212, 0xb5793f, 0.50f, 0.50f, 0.00f, 1.0f, 0.0f },
};

typedef struct {
    const char *name;
    int         scene, look;
    bool        lake;
    const char *caption;
} GalSpot;

typedef struct {
    const char *city, *region;
    int         year, month, dayFrom, dayTo, count, spots;
    GalSpot     spot[4];
} GalShoot;

/* Newest first, as a library reads, so a run is a contiguous span of the wall. */
static const GalShoot GAL_SHOOTS[GAL_SHOOT_COUNT] = {
    { "Kyoto", "Japan", 2026, 10, 18, 24, 8, 4,
      { { "Arashiyama", PG_FOREST, L_FOG_GOLD, false,
         "The bamboo gave way to cedar and the fog stayed." },
        { "Eikando", PG_FOREST, L_FOG_AUTUMN, false, "Peak color, three days early." },
        { "Kamo River", PG_SKYLINE, L_CITY_DUSK, false, "" },
        { "Pontocho", PG_BOKEH, L_LIGHTS_WARM, false, "" } } },
    { "Reykjavik", "Iceland", 2026, 8, 9, 14, 10, 4,
      { { "Jokulsarlon", PG_ALPINE, L_ALP_ICE, true, "Icebergs the size of houses, drifting out." },
        { "Reynisfjara", PG_COAST, L_COAST_DAWN, false,
         "Black sand and a sky that would not settle." },
        { "Skogafoss", PG_FALLS, L_FALLS, false, "Loud enough to feel in your chest." },
        { "Vatnajokull", PG_ALPINE, L_ALP_GLOW, false, "" } } },
    { "Provence", "France", 2026, 6, 21, 27, 8, 4,
      { { "Valensole", PG_FIELD, L_LAVENDER, false, "The plateau turns violet by six." },
        { "Sault", PG_FIELD, L_LAVENDER, false, "" },
        { "Puimoisson", PG_FLOWER, L_BLOOM_SUN, false,
         "Sunflowers over the fence, facing away from us." },
        { "Sainte-Croix", PG_ALPINE, L_ALP_MORNING, true, "" } } },
    { "Portland", "Oregon", 2026, 5, 2, 30, 6, 4,
      { { "Back Garden", PG_FLOWER, L_BLOOM_PINK, false, "The dahlias finally opened." },
        { "Back Garden", PG_FLOWER, L_BLOOM_WHITE, false, "" },
        { "Stumptown", PG_COFFEE, L_COFFEE, false, "Saturday." },
        { "Mt. Hood", PG_ALPINE, L_ALP_MORNING, false, "" } } },
    { "Tromso", "Norway", 2026, 2, 8, 13, 8, 3,
      { { "Ersfjordbotn", PG_AURORA, L_AURORA_GREEN, false, "Four nights of cloud, then this." },
        { "Sommaroy", PG_AURORA, L_AURORA_VIOLET, false, "" },
        { "Lyngen Alps", PG_ALPINE, L_ALP_ICE, false, "" } } },
    { "Lisbon", "Portugal", 2025, 11, 3, 8, 6, 4,
      { { "Alfama", PG_SKYLINE, L_CITY_DUSK, false, "Every street here ends in a view." },
        { "Miradouro", PG_SKYLINE, L_CITY_BLUE, false, "" },
        { "Bairro Alto", PG_BOKEH, L_LIGHTS_WARM, false, "" },
        { "Manteigaria", PG_COFFEE, L_COFFEE, false, "Second breakfast." } } },
    { "Big Sur", "California", 2025, 9, 12, 15, 8, 4,
      { { "Bixby Creek", PG_COAST, L_COAST_SUNSET, false,
         "We got there with four minutes of light left." },
        { "McWay Cove", PG_BEACH, L_BEACH, false, "" },
        { "Pfeiffer", PG_FOREST, L_FOG_BLUE, false, "Redwoods in the marine layer." },
        { "Garrapata", PG_COAST, L_COAST_DAWN, false, "" } } },
    { "Dolomites", "Italy", 2025, 7, 5, 12, 8, 4,
      { { "Tre Cime", PG_ALPINE, L_ALP_GLOW, false, "First light hits the peaks at five." },
        { "Lago di Braies", PG_ALPINE, L_ALP_MORNING, true, "So still it looked poured." },
        { "Seceda", PG_ALPINE, L_ALP_MORNING, false, "" },
        { "Val Gardena", PG_FOREST, L_FOG_BLUE, false, "" } } },
    { "Merzouga", "Morocco", 2025, 4, 17, 20, 6, 2,
      { { "Erg Chebbi", PG_DUNES, L_DUNE_NOON, false, "The wind rewrites this every night." },
        { "Erg Chebbi", PG_DUNES, L_DUNE_DUSK, false, "" } } },
    { "New York", "New York", 2025, 1, 10, 13, 6, 3,
      { { "Brooklyn Bridge Park", PG_SKYLINE, L_CITY_BLUE, false,
         "Blue hour lasts eleven minutes in January." },
        { "Hudson Yards", PG_SKYLINE, L_CITY_DUSK, false, "" },
        { "Times Square", PG_BOKEH, L_LIGHTS_CITY, false, "" } } },
    { "Stowe", "Vermont", 2024, 10, 4, 8, 7, 2,
      { { "Smugglers Notch", PG_FOREST, L_FOG_AUTUMN, false, "The whole valley, one week a year." },
        { "Sterling Pond", PG_ALPINE, L_ALP_MORNING, true, "" } } },
    { "Maui", "Hawaii", 2024, 7, 20, 28, 8, 3,
      { { "Kaanapali", PG_BEACH, L_BEACH, false, "Water this color should not be legal." },
        { "Wailea", PG_BEACH, L_BEACH, false, "" },
        { "Lahaina", PG_COAST, L_COAST_SUNSET, false, "" } } },
    { "Lisse", "Netherlands", 2024, 4, 19, 22, 7, 3,
      { { "Keukenhof", PG_FIELD, L_TULIP, false, "Seven million bulbs and a very good bicycle." },
        { "Keukenhof", PG_FLOWER, L_BLOOM_RED, false, "" },
        { "Bollenstreek", PG_FIELD, L_TULIP, false, "" } } },
};

static const char *const GAL_MONTHS[12] = {
    "January", "February", "March", "April", "May", "June", "July", "August", "September",
    "October", "November", "December",
};

static void gal_month_short(char *dst, int *pos, int cap, int month) {
    char three[4] = { GAL_MONTHS[month - 1][0], GAL_MONTHS[month - 1][1],
                     GAL_MONTHS[month - 1][2], 0 };
    gal_put(dst, pos, cap, three);
}

/* "Aug 9 - 14, 2026": both ends in one month, the American way round. */
static void gal_shoot_range(char *dst, int cap, const GalShoot *s) {
    int pos = 0;
    gal_month_short(dst, &pos, cap, s->month);
    gal_put(dst, &pos, cap, " ");
    gal_put_int(dst, &pos, cap, s->dayFrom);
    gal_put(dst, &pos, cap, " " GAL_NDASH " ");
    gal_put_int(dst, &pos, cap, s->dayTo);
    gal_put(dst, &pos, cap, ", ");
    gal_put_int(dst, &pos, cap, s->year);
}

/* Which album a photo belongs in follows from where and what it is. */
static uint32_t gal_albums_for(int shoot, int scene) {
    uint32_t mask = 0u;
    if (shoot == 1) mask |= 1u << 0;
    if (shoot == 2) mask |= 1u << 1;
    if (scene == PG_ALPINE) mask |= 1u << 2;
    if (scene == PG_FLOWER || scene == PG_FIELD) mask |= 1u << 3;
    if (scene == PG_AURORA) mask |= 1u << 4;
    if (scene == PG_SKYLINE || scene == PG_BOKEH) mask |= 1u << 5;
    return mask;
}

/* Which of a trip's places its photo @p k was taken at: a walk that never stays put, as a real
   day moves about. Where the trip has the places for it, the next one has another look than
   this one and is not the place before it either, so three photos side by side on the wall
   are three pictures; a hash picks among the places that leaves. */
static int gal_spot_of(int shootIndex, int k, int spots) {
    const GalSpot *sp = GAL_SHOOTS[shootIndex].spot;
    int s = 0, before = -1;
    for (int i = 1; i <= k && spots > 1; i++) {
        int pick[4] = { 0 }, count = 0, best = 0;
        for (int c = 0; c < spots; c++) {
            const int rank = (sp[c].look != sp[s].look ? 2 : 0) + (c != before ? 1 : 0);
            if (c == s)
                continue;
            if (rank > best) {
                best = rank;
                count = 0;
            }
            if (rank == best)
                pick[count++] = c;
        }
        before = s;
        s = pick[pg_hash(i, shootIndex, 1442u) % (uint32_t)count];
    }
    return s;
}

/* Fills in one photo. Everything a camera would record follows from the photo's index, so
   its record stays the same on every run. */
static void gal_seed_photo(GalPhoto *p, int index, int shootIndex, int k, int fileNumber) {
    const GalShoot *sh = &GAL_SHOOTS[shootIndex];
    const int       spot = gal_spot_of(shootIndex, k, sh->spots);
    const GalSpot  *sp = &sh->spot[spot];
    unsigned        seed = (unsigned)index * 7919u + 13u;
    uint32_t        h = pg_hash(index, shootIndex, seed);

    gal_copy(p->title, sp->name, sizeof p->title);
    int pos = 0;
    gal_put(p->place, &pos, sizeof p->place, sh->city);
    gal_put(p->place, &pos, sizeof p->place, ", ");
    gal_put(p->place, &pos, sizeof p->place, sh->region);

    int day = sh->dayTo;
    if (sh->count > 1)
        day -= k * (sh->dayTo - sh->dayFrom) / (sh->count - 1);
    pos = 0;
    gal_month_short(p->date, &pos, sizeof p->date, sh->month);
    gal_put(p->date, &pos, sizeof p->date, " ");
    gal_put_int(p->date, &pos, sizeof p->date, day);
    gal_put(p->date, &pos, sizeof p->date, ", ");
    gal_put_int(p->date, &pos, sizeof p->date, sh->year);

    int hour = 6 + (int)(h % 13u), minute = (int)((h >> 8) % 60u);
    pos = 0;
    gal_put(p->when, &pos, sizeof p->when, p->date);
    gal_put(p->when, &pos, sizeof p->when, " at ");
    gal_put_int(p->when, &pos, sizeof p->when, hour > 12 ? hour - 12 : hour);
    gal_put(p->when, &pos, sizeof p->when, minute < 10 ? ":0" : ":");
    gal_put_int(p->when, &pos, sizeof p->when, minute);
    gal_put(p->when, &pos, sizeof p->when, hour >= 12 ? " PM" : " AM");

    pos = 0;
    gal_put(p->file, &pos, sizeof p->file, "IMG_");
    gal_put_int(p->file, &pos, sizeof p->file, fileNumber);
    gal_put(p->file, &pos, sizeof p->file, ".HEIC");

    /* Falls, blooms and cups stand tall or square; a landscape mostly lies down. */
    static const float WIDE[5] = { 1.5f, 1.5f, 1.3333f, 1.7778f, 0.6667f };
    static const float TALL[4] = { 0.75f, 0.6667f, 1.0f, 0.75f };
    bool upright = sp->scene == PG_FALLS || sp->scene == PG_FLOWER || sp->scene == PG_COFFEE;
    p->sourceAspect = upright ? TALL[h % 4u] : WIDE[(h >> 3) % 5u];
    p->aspect = p->sourceAspect;

    int longEdge = sh->year >= 2025 ? 6048 : 4032;
    int w = p->sourceAspect >= 1.0f ? longEdge : (int)((float)longEdge * p->sourceAspect);
    int ph = p->sourceAspect >= 1.0f ? (int)((float)longEdge / p->sourceAspect) : longEdge;
    int mp = (w / 100) * (ph / 100) / 100;
    int tenths = mp * 3 + (int)((h >> 5) % 18u);

    gal_copy(p->camera, sh->year >= 2025 ? "iPhone 16 Pro" : "iPhone 15 Pro", sizeof p->camera);
    int lens = (int)((h >> 12) % 4u);
    static const char *const LENS[4] = {
        "Main Camera, 24 mm f/1.78", "Main Camera, 24 mm f/1.78",
        "Ultra Wide Camera, 13 mm f/2.2", "Telephoto Camera, 120 mm f/2.8",
    };
    static const char *const FOCAL[4] = { "24 mm", "24 mm", "13 mm", "120 mm" };
    static const char *const STOP[4] = { "f/1.78", "f/1.78", "f/2.2", "f/2.8" };
    static const int ISOS[6] = { 50, 64, 80, 125, 200, 640 };
    static const char *const SHUTTER[5] = { "1/2000 s", "1/640 s", "1/250 s", "1/120 s", "1/30 s" };
    static const char *const EV[3] = { "0 ev", "-0.3 ev", "+0.3 ev" };
    gal_copy(p->lens, LENS[lens], sizeof p->lens);
    gal_copy(p->focal, FOCAL[lens], sizeof p->focal);
    gal_copy(p->fstop, STOP[lens], sizeof p->fstop);
    pos = 0;
    gal_put(p->iso, &pos, sizeof p->iso, "ISO ");
    gal_put_int(p->iso, &pos, sizeof p->iso, ISOS[(h >> 15) % 6u]);
    gal_copy(p->shutter, SHUTTER[(h >> 18) % 5u], sizeof p->shutter);
    gal_copy(p->ev, EV[(h >> 21) % 3u], sizeof p->ev);

    pos = 0;
    gal_put_int(p->specs, &pos, sizeof p->specs, mp);
    gal_put(p->specs, &pos, sizeof p->specs, " MP " GAL_DOT " ");
    gal_put_int(p->specs, &pos, sizeof p->specs, w);
    gal_put(p->specs, &pos, sizeof p->specs, " x ");
    gal_put_int(p->specs, &pos, sizeof p->specs, ph);
    gal_put(p->specs, &pos, sizeof p->specs, " " GAL_DOT " ");
    gal_put_int(p->specs, &pos, sizeof p->specs, tenths / 10);
    gal_put(p->specs, &pos, sizeof p->specs, ".");
    gal_put_int(p->specs, &pos, sizeof p->specs, tenths % 10);
    gal_put(p->specs, &pos, sizeof p->specs, " MB");

    /* A place's caption goes on the first photo taken there. */
    bool first = true;
    for (int j = 0; j < k; j++)
        first = first && gal_spot_of(shootIndex, j, sh->spots) != spot;
    if (first)
        gal_copy(p->caption, sp->caption, sizeof p->caption);
    p->shoot = shootIndex;
    p->spot = spot;
    p->year = sh->year;
    p->seed = seed;
    p->albums = gal_albums_for(shootIndex, sp->scene);
    p->favorite = (h >> 24) % 11u == 0u || (k == 0 && shootIndex % 2 == 0);
}

/* The shot behind a photo: its spot's look, with the sun, the exposure and the white balance
   moved a little, and the place faced one way or the other, so two frames from one spot are
   two pictures, not one. */
static void gal_photo_shot(const GalPhoto *p, PgShot *shot) {
    const GalSpot *sp = &GAL_SHOOTS[p->shoot].spot[p->spot];
    shot->scene = (PgScene)sp->scene;
    shot->look = GAL_LOOKS[sp->look];
    shot->seed = p->seed;
    shot->lake = sp->lake;
    shot->flip = pg_rand((int)p->seed, 4, 5u) > 0.5f;
    shot->gain = 0.96f + pg_rand((int)p->seed, 1, 5u) * 0.10f;
    shot->warm = (pg_rand((int)p->seed, 5, 5u) - 0.5f) * 0.12f;
    if (sp->scene != PG_FLOWER) {
        shot->look.sunX += (pg_rand((int)p->seed, 2, 5u) - 0.5f) * 0.26f;
        shot->look.sunY += (pg_rand((int)p->seed, 3, 5u) - 0.5f) * 0.06f;
    } else {
        shot->look.sunX += (pg_rand((int)p->seed, 2, 5u) - 0.5f) * 0.16f;
        shot->look.sunY += (pg_rand((int)p->seed, 3, 5u) - 0.5f) * 0.12f;
    }
}

/* How many pixels to render a photo at, its long side @p edge. */
static void gal_photo_size(const GalPhoto *p, int edge, int *w, int *h) {
    if (p->sourceAspect >= 1.0f) {
        *w = edge;
        *h = (int)((float)edge / p->sourceAspect + 0.5f);
    } else {
        *h = edge;
        *w = (int)((float)edge * p->sourceAspect + 0.5f);
    }
}

/* Width over height once an edit's crop and turns are applied. */
static float gal_edit_aspect(float sourceAspect, const GalEdit *e) {
    static const float RATIO[GAL_CROP_COUNT] = { 0.0f, 1.0f, 4.0f / 3.0f, 3.0f / 2.0f,
                                                16.0f / 9.0f };
    float a = (e->turns & 1) ? 1.0f / sourceAspect : sourceAspect;
    if (e->crop <= GAL_CROP_ORIGINAL || e->crop >= GAL_CROP_COUNT)
        return a;
    return a >= 1.0f ? RATIO[e->crop] : 1.0f / RATIO[e->crop];
}

static bool gal_edit_is_identity(const GalEdit *e) {
    return e->turns == 0 && e->crop == GAL_CROP_ORIGINAL && e->exposure == 0.0f
        && e->contrast == 0.0f && e->saturation == 0.0f && e->warmth == 0.0f && e->vignette == 0.0f;
}

/* The editor's pixel work. One lookup table per channel carries exposure, contrast and
   warmth, so a slider drag over a megapixel stays well inside a frame; saturation and the
   vignette are per pixel. @p dst holds (w * h * 4) bytes; the result's size comes back in
   *outW and *outH. */
static void gal_edit_apply(const unsigned char *src, int sw, int sh, const GalEdit *e,
                           unsigned char *dst, int *outW, int *outH) {
    int rw = (e->turns & 1) ? sh : sw, rh = (e->turns & 1) ? sw : sh;
    int cw = rw, ch = rh;
    float want = gal_edit_aspect((float)sw / (float)sh, e);
    if (e->crop > GAL_CROP_ORIGINAL) {
        if ((float)rw / (float)rh > want)
            cw = (int)((float)rh * want + 0.5f);
        else
            ch = (int)((float)rw / want + 0.5f);
    }
    int ox = (rw - cw) / 2, oy = (rh - ch) / 2;
    *outW = cw;
    *outH = ch;

    unsigned char lut[3][256];
    float gain = 1.0f + e->exposure * (e->exposure > 0.0f ? 0.9f : 0.55f);
    float slope = 1.0f + e->contrast * (e->contrast > 0.0f ? 0.8f : 0.55f);
    const float tint[3] = { 1.0f + e->warmth * 0.14f, 1.0f, 1.0f - e->warmth * 0.16f };
    for (int c = 0; c < 3; c++) {
        for (int i = 0; i < 256; i++) {
            float v = (float)i / 255.0f * gain * tint[c];
            v = 0.5f + (v - 0.5f) * slope;
            lut[c][i] = (unsigned char)(pg_clamp(v) * 255.0f + 0.5f);
        }
    }
    int sat = (int)((1.0f + e->saturation) * 256.0f);
    int vig = (int)(e->vignette * 256.0f);
    float cx = (float)cw * 0.5f, cy = (float)ch * 0.5f;
    float norm = 1.0f / (cx * cx + cy * cy);

    for (int y = 0; y < ch; y++) {
        int ry = y + oy;
        unsigned char *o = dst + (size_t)y * (size_t)cw * 4u;
        for (int x = 0; x < cw; x++, o += 4) {
            int rx = x + ox, sx, sy;
            switch (e->turns & 3) {
            case 1:  sx = ry;          sy = sh - 1 - rx; break;
            case 2:  sx = sw - 1 - rx; sy = sh - 1 - ry; break;
            case 3:  sx = sw - 1 - ry; sy = rx;          break;
            default: sx = rx;          sy = ry;          break;
            }
            const unsigned char *p = src + ((size_t)sy * (size_t)sw + (size_t)sx) * 4u;
            int r = lut[0][p[0]], g = lut[1][p[1]], b = lut[2][p[2]];
            int luma = (r * 77 + g * 150 + b * 29) >> 8;
            r = luma + (((r - luma) * sat) >> 8);
            g = luma + (((g - luma) * sat) >> 8);
            b = luma + (((b - luma) * sat) >> 8);
            if (vig > 0) {
                float dx = (float)x - cx, dy = (float)y - cy;
                int   k = 256 - (int)(pg_smooth(0.20f, 1.0f,
                                                (dx * dx + dy * dy) * norm) * (float)vig * 0.86f);
                r = (r * k) >> 8;
                g = (g * k) >> 8;
                b = (b * k) >> 8;
            }
            o[0] = (unsigned char)(r < 0 ? 0 : r > 255 ? 255 : r);
            o[1] = (unsigned char)(g < 0 ? 0 : g > 255 ? 255 : g);
            o[2] = (unsigned char)(b < 0 ? 0 : b > 255 ? 255 : b);
            o[3] = 255;
        }
    }
}

/* ---- narrowing the library ---- */

static bool gal_passes(const GalStore *s, int i, int scope, int album, const char *search) {
    const GalPhoto *p = &s->photo[i];
    if (i == s->keep)
        return true;
    if (scope == GAL_SCOPE_DELETED)
        return p->deleted && gal_contains(p->title, search);
    if (p->deleted)
        return false;
    if (scope == GAL_SCOPE_FAVORITES && !p->favorite)
        return false;
    if (scope == GAL_SCOPE_RECENTS && p->shoot > 1)
        return false;
    if (scope == GAL_SCOPE_ALBUM && !(p->albums & (1u << album)))
        return false;
    if (!search || !search[0])
        return true;
    if (gal_contains(p->title, search) || gal_contains(p->place, search)
        || gal_contains(p->date, search) || gal_contains(p->caption, search))
        return true;
    for (int a = 0; a < GAL_ALBUM_COUNT; a++)
        if ((p->albums & (1u << a)) && gal_contains(GAL_ALBUM_NAMES[a], search))
            return true;
    return false;
}

/* A run's key photo: its first favorite, else its first. */
static int gal_cover(const GalStore *s, int first, int n) {
    for (int k = 0; k < n; k++)
        if (s->photo[s->visible[first + k]].favorite)
            return s->visible[first + k];
    return n > 0 ? s->visible[first] : -1;
}

/* Recomputes everything that depends on what is visible: the list itself, its runs and
   years, the counts in the sidebar and the toolbar's title strings. */
static void gallery_filter(GalStore *s, int scope, int album, const char *search) {
    s->visibleCount = 0;
    s->runCount = 0;
    s->yearCount = 0;
    for (int i = 0; i < GAL_PHOTO_COUNT; i++) {
        if (!gal_passes(s, i, scope, album, search))
            continue;
        GalRun *r = s->runCount > 0 ? &s->run[s->runCount - 1] : NULL;
        if (!r || s->photo[s->visible[r->first]].shoot != s->photo[i].shoot) {
            const GalShoot *sh = &GAL_SHOOTS[s->photo[i].shoot];
            r = &s->run[s->runCount++];
            r->first = s->visibleCount;
            r->n = 0;
            r->year = sh->year;
            int pos = 0;
            gal_put(r->label, &pos, sizeof r->label, GAL_MONTHS[sh->month - 1]);
            gal_put(r->label, &pos, sizeof r->label, " ");
            gal_put_int(r->label, &pos, sizeof r->label, sh->year);
            gal_copy(r->place, s->photo[i].place, sizeof r->place);
            gal_shoot_range(r->range, sizeof r->range, sh);
        }
        r->n++;
        s->visible[s->visibleCount++] = i;
    }
    for (int k = 0; k < s->runCount; k++) {
        GalRun *r = &s->run[k];
        r->cover = gal_cover(s, r->first, r->n);
        gal_count(r->count, sizeof r->count, r->n, " Photo", " Photos");

        GalYear *y = s->yearCount > 0 ? &s->year[s->yearCount - 1] : NULL;
        if (!y || s->photo[s->visible[y->firstPhoto]].year != r->year) {
            y = &s->year[s->yearCount++];
            y->firstRun = k;
            y->runs = 0;
            y->firstPhoto = r->first;
            y->n = 0;
            int pos = 0;
            gal_put_int(y->label, &pos, sizeof y->label, r->year);
        }
        y->runs++;
        y->n += r->n;
    }
    for (int k = 0; k < s->yearCount; k++) {
        GalYear *y = &s->year[k];
        int pos = 0;
        for (int r = 0; r < y->runs && r < 2; r++) {
            const int first = s->visible[s->run[y->firstRun + r].first];
            const GalShoot *sh = &GAL_SHOOTS[s->photo[first].shoot];
            if (r > 0)
                gal_put(y->places, &pos, sizeof y->places, y->runs == 2 ? " & " : ", ");
            gal_put(y->places, &pos, sizeof y->places, sh->city);
        }
        if (y->runs > 2) {
            gal_put(y->places, &pos, sizeof y->places, " & ");
            gal_put_int(y->places, &pos, sizeof y->places, y->runs - 2);
            gal_put(y->places, &pos, sizeof y->places, " more");
        }
        if (y->runs < 1)
            y->places[0] = '\0';
        gal_count(y->count, sizeof y->count, y->n, " Photo", " Photos");
    }
    bool narrowed = search && search[0];
    gal_count(s->countStr, sizeof s->countStr, s->visibleCount,
              narrowed ? " Result" : " Photo", narrowed ? " Results" : " Photos");
    s->range[0] = '\0';
    if (s->runCount == 1) {
        gal_copy(s->range, s->run[0].range, sizeof s->range);
    } else if (s->runCount > 1) {
        int pos = 0;
        gal_put(s->range, &pos, sizeof s->range, s->photo[s->visible[s->visibleCount - 1]].date);
        gal_put(s->range, &pos, sizeof s->range, " " GAL_NDASH " ");
        gal_put(s->range, &pos, sizeof s->range, s->photo[s->visible[0]].date);
    }

    /* The sidebar's counts describe the whole library, so a search does not change them. A
       cover is a photo of the album's own scene before any other, a favorite before the rest,
       and never a picture an earlier album already stands behind: a row of identical covers
       reads as one album. */
    int lib = 0;
    s->favorites = s->recents = s->deleted = 0;
    s->favoritesCover = s->recentsCover = -1;
    for (int a = 0; a < GAL_ALBUM_COUNT; a++) {
        s->album[a].n = 0;
        s->album[a].cover = -1;
    }
    for (int i = 0; i < GAL_PHOTO_COUNT; i++) {
        const GalPhoto *p = &s->photo[i];
        if (p->deleted) {
            s->deleted++;
            continue;
        }
        lib++;
        if (p->favorite && s->favorites++ == 0)
            s->favoritesCover = i;
        if (p->shoot <= 1) {
            s->recents++;
            if (s->recentsCover < 0 && i != s->favoritesCover)
                s->recentsCover = i;
        }
    }
    if (s->recentsCover < 0 && s->recents > 0)
        s->recentsCover = s->favoritesCover;
    for (int a = 0; a < GAL_ALBUM_COUNT; a++) {
        int best = -1, rank = 5;             /* 0 to 3 free, 4 already a cover */
        for (int i = 0; i < GAL_PHOTO_COUNT; i++) {
            const GalPhoto *p = &s->photo[i];
            if (p->deleted || !(p->albums & (1u << a)))
                continue;
            s->album[a].n++;
            bool taken = i == s->favoritesCover || i == s->recentsCover;
            for (int b = 0; b < a; b++)
                taken = taken || s->album[b].cover == i;
            const bool own = GAL_SHOOTS[p->shoot].spot[p->spot].scene == GAL_ALBUM_SCENE[a];
            int r = taken ? 4 : (own ? 0 : 2) + (p->favorite ? 0 : 1);
            if (r < rank) {
                rank = r;
                best = i;
            }
        }
        s->album[a].cover = best;
    }
    for (int a = 0; a < GAL_ALBUM_COUNT; a++)
        gal_count(s->album[a].count, sizeof s->album[a].count, s->album[a].n, "", "");
    gal_count(s->libraryStr, sizeof s->libraryStr, lib, "", "");
    gal_count(s->favoritesStr, sizeof s->favoritesStr, s->favorites, "", "");
    gal_count(s->recentsStr, sizeof s->recentsStr, s->recents, "", "");
    gal_count(s->deletedStr, sizeof s->deletedStr, s->deleted, "", "");
}

/* Builds the catalog, in one pass down the shoots table. */
static void gallery_backend_seed(GalStore *s) {
    gal_zero(s, sizeof *s);
    s->keep = -1;
    int index = 0, fileNumber = 9412;
    for (int sh = 0; sh < GAL_SHOOT_COUNT; sh++) {
        for (int k = 0; k < GAL_SHOOTS[sh].count && index < GAL_PHOTO_COUNT; k++, index++,
             fileNumber -= 37)
            gal_seed_photo(&s->photo[index], index, sh, k, fileNumber);
    }
    for (int m = 0; m < GAL_MEMORY_COUNT; m++) {
        int shoot = GAL_MEMORIES[m].shoot < 0 ? 6 : GAL_MEMORIES[m].shoot;
        for (int i = 0; i < GAL_PHOTO_COUNT; i++) {
            if (s->photo[i].shoot == shoot) {
                s->memoryCover[m] = i;
                break;
            }
        }
    }
    gallery_filter(s, GAL_SCOPE_LIBRARY, 0, "");
}

/* ---- the wall ---- */

static void gal_wall_reset(GalWall *w) {
    for (int i = 0; i < GAL_PHOTO_COUNT; i++)
        w->state[i] = GAL_WALL_ABSENT;
    w->headCount = 0;
    w->height = 0.0f;
}

/* Lays the photos k0..k1-1 of visible[] out as one row at height @p h. The widths are their
   aspects scaled and rounded, with the rounding error carried along, so a filled row ends on
   the wall's right edge to the pixel. */
static void gal_row(GalWall *w, const GalStore *s, int k0, int k1, float x, float y, float h,
                    float gap,
                    float right, bool fill) {
    float exact = x;
    for (int k = k0; k < k1; k++) {
        int   i = s->visible[k];
        float next = exact + s->photo[i].aspect * h;
        float pw = (float)(int)(next + 0.5f) - (float)(int)(exact + 0.5f);
        if (fill && k == k1 - 1)
            pw = right - x;
        w->photo[i] = RC_LIT(GalRect){ x, y, pw, h };
        w->state[i] = GAL_WALL_SHOWN;
        x += pw + gap;
        exact = next + gap;
    }
}

/* A justified run: greedy rows, each taking photos until it would pass the wall's width,
   then scaled so it meets the edge exactly. A last row too short to stretch keeps the
   target height and ends early, as the last row of a library does. */
static float gal_flow(GalWall *w, const GalStore *s, int k0, int k1, float W, float rowH,
                      float gap, float y) {
    int   start = k0;
    float sum = 0.0f;
    for (int k = k0; k < k1; k++) {
        float a = s->photo[s->visible[k]].aspect;
        float now = (sum + a) * rowH + gap * (float)(k - start);
        if (now >= W && k > start) {
            float before = sum * rowH + gap * (float)(k - start - 1);
            int   end = (W - before < now - W) ? k : k + 1;
            float used = (end == k) ? sum : sum + a;
            float h = (W - gap * (float)(end - start - 1)) / used;
            h = (float)(int)(h + 0.5f);
            gal_row(w, s, start, end, 0.0f, y, h, gap, W, true);
            y += h + gap;
            start = end;
            sum = (end == k) ? a : 0.0f;
        } else {
            sum += a;
        }
    }
    if (start < k1) {
        float used = sum * rowH + gap * (float)(k1 - start - 1);
        bool  stretch = used > W * 0.8f;
        float h = stretch ? (float)(int)((W - gap * (float)(k1 - start - 1)) / sum + 0.5f) : rowH;
        gal_row(w, s, start, k1, 0.0f, y, h, gap, W, stretch);
        y += h + gap;
    }
    return y;
}

/* The row height at which this scope's photos about fill @p room: n photos of mean aspect a
   cover n a h^2, less what a justified wall loses to its last row. */
static float gal_fill_row_h(const GalStore *s, float W, float room) {
    float sum = 0.0f;
    for (int k = 0; k < s->visibleCount; k++)
        sum += s->photo[s->visible[k]].aspect;
    return sum > 0.0f ? pg_sqrt(W * room * 0.84f / sum) : 0.0f;
}

/* True when the wall's first row reaches its right edge at this height: gal_flow stretches a
   row once it passes 80 percent of the width. */
static bool gal_wall_fills_row(const GalStore *s, float W, float rowH) {
    float sum = 0.0f;
    for (int k = 0; k < s->visibleCount; k++)
        sum += s->photo[s->visible[k]].aspect;
    return s->visibleCount > 0 && sum * rowH + 2.0f * (float)(s->visibleCount - 1) > W * 0.8f;
}

/* All Photos: the whole wall as one justified flow, edge to edge under the toolbar. */
static void gal_wall_flow(GalWall *w, const GalStore *s, float W, float rowH, float top,
                          float bottom) {
    gal_wall_reset(w);
    w->height = gal_flow(w, s, 0, s->visibleCount, W, rowH, 2.0f, top) + bottom;
}

/* All Photos on a phone: square cells @p cols to a row with 2 px between them, each photo cut
   to cover its own, as iOS Photos lays a library out. A justified row there would be a strip
   of slivers. A column's edges are rounded from its exact place, so the division's remainder
   is spread a pixel at a time and the grid meets both edges. */
static void gal_wall_grid(GalWall *w, const GalStore *s, float W, int cols, float top,
                          float bottom) {
    const float gap = 2.0f, pitch = (W + gap) / (float)cols;
    const float cell = (float)(int)(pitch - gap + 0.5f);
    gal_wall_reset(w);
    for (int k = 0; k < s->visibleCount; k++) {
        const int   c = k % cols, i = s->visible[k];
        const float x = (float)(int)((float)c * pitch + 0.5f);
        const float right = (float)(int)((float)(c + 1) * pitch + 0.5f) - gap;
        w->photo[i] = RC_LIT(GalRect){ x, top + (float)(k / cols) * (cell + gap), right - x, cell };
        w->state[i] = GAL_WALL_SHOWN;
    }
    w->height = top + (float)((s->visibleCount + cols - 1) / cols) * (cell + gap) + bottom;
}

/* Years: every photo, tiny, under its year's heading. The tile size is whatever fills the
   window, so the overview never strands a few rows at the top of an empty page. */
static void gal_wall_years(GalWall *w, const GalStore *s, float W, float viewH, float top,
                           float headH,
                           float bottom) {
    gal_wall_reset(w);
    float room = viewH - top - bottom - headH * (float)s->yearCount;
    if (room < 200.0f)
        room = 200.0f;
    float rowH = gal_fill_row_h(s, W, room);
    rowH = rowH < 40.0f ? 40.0f : rowH > 150.0f ? 150.0f : (float)(int)rowH;
    float y = top;
    for (int k = 0; k < s->yearCount; k++) {
        const GalYear *yr = &s->year[k];
        w->head[k] = RC_LIT(GalRect){ 16.0f, y, W - 32.0f, headH };
        y += headH;
        y = gal_flow(w, s, yr->firstPhoto, yr->firstPhoto + yr->n, W, rowH, 1.0f, y);
    }
    w->headCount = s->yearCount;
    w->height = y + bottom;
}

/* Months: one card a trip. A run's other photos are folded behind its cover at the card's
   place, so a zoom out flies them all into it. */
static void gal_wall_months(GalWall *w, const GalStore *s, float W, float top, float bottom) {
    gal_wall_reset(w);
    const float pad = 12.0f, gap = 12.0f;
    int cols = (int)((W - 2.0f * pad + gap) / (300.0f + gap));
    cols = cols < 1 ? 1 : cols > 4 ? 4 : cols;
    float cw = (float)(int)((W - 2.0f * pad - gap * (float)(cols - 1)) / (float)cols);
    float ch = (float)(int)(cw * 0.66f);
    float y = top;
    for (int r = 0; r < s->runCount; r++) {
        int   c = r % cols;
        GalRect card = { pad + (float)c * (cw + gap), y, cw, ch };
        const GalRun *run = &s->run[r];
        w->head[r] = card;
        for (int k = 0; k < run->n; k++) {
            int i = s->visible[run->first + k];
            w->photo[i] = card;
            w->state[i] = i == run->cover ? GAL_WALL_SHOWN : GAL_WALL_FOLDED;
        }
        if (c == cols - 1)
            y += ch + gap;
    }
    w->headCount = s->runCount;
    w->height = (s->runCount % cols ? y + ch + gap : y) + bottom;
}

#endif /* GALLERY_BACKEND_IMPLEMENTATION */
#endif /* GALLERY_BACKEND_H */
