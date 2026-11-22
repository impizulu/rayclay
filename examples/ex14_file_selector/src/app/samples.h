/*
    samples.h - what the app's own folder starts with

    The selector opens in rcAppDataPath, which a fresh install holds nothing in,
    and an empty selector shows nothing of itself. So the app keeps a small home
    there with rcSaveFile: folders to walk into, pictures and documents for
    the file-type list, a folder nested two deep for the path bar, and one left
    empty for the empty state. Every file is honestly what its name says: the
    pictures are real BMPs and SVGs, painted here, and the rest is real text.
*/
#ifndef APP_SAMPLES_H
#define APP_SAMPLES_H

#include "rayclay.h"
#include "app/file_selector.h"

typedef struct {
    const char *path;   /* below the app's folder, '/' between names */
    const char *text;
} Sample;

static const Sample SAMPLES[] = {
    { "Documents/Q3 report.md",
      "# Q3 report\n\nRevenue grew 14 percent over the quarter, led by renewals in the\n"
      "northern region. Support volume fell after the September release, and the new\n"
      "onboarding flow lifted trial conversion from 21 to 26 percent.\n\n"
      "## Next quarter\n\n- Hire two support engineers\n- Finish the billing migration\n"
      "- Open the Lisbon office in November\n" },
    { "Documents/Meeting notes.txt",
      "Thursday planning\n- Ship the selector on every platform\n- Review the empty state\n"
      "- Maya to send the launch checklist by Friday\n" },
    { "Documents/Reading list.md",
      "# Reading list\n\n"
      "- The Design of Everyday Things\n- A Pattern Language\n- Thinking in Systems\n" },
    { "Documents/Contacts.json",
      "[ { \"name\": \"Maya Okafor\", \"team\": \"Design\", \"city\": \"Lisbon\" },\n"
      "  { \"name\": \"Tomas Lind\", \"team\": \"Support\", \"city\": \"Oslo\" },\n"
      "  { \"name\": \"Priya Raman\", \"team\": \"Finance\", \"city\": \"Toronto\" } ]\n" },
    { "Documents/Packing list.txt",
      "Passport\nChargers\nRain jacket\nWalking shoes\nPaper tickets\n" },
    { "Documents/Recipes/Tomato soup.md",
      "# Tomato soup\n\nSoften an onion in butter, add two cans of tomatoes and a cup of stock,\n"
      "simmer for twenty minutes and blend.\n" },
    { "Documents/Recipes/Sourdough.md",
      "# Sourdough\n\n500 g flour, 350 g water, 100 g starter, 10 g salt. Fold four times over\n"
      "two hours, shape, rest overnight in the fridge, bake at 240 C in a covered pot.\n" },
    { "Pictures/Sunrise.svg",
      "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 64 64\">"
      "<rect width=\"64\" height=\"64\" fill=\"#fbbf24\"/>"
      "<rect width=\"64\" height=\"20\" fill=\"#fb923c\"/>"
      "<circle cx=\"32\" cy=\"38\" r=\"16\" fill=\"#fde68a\"/>"
      "<circle cx=\"32\" cy=\"38\" r=\"11\" fill=\"#fff7ed\"/>"
      "<path d=\"M0 42 L16 34 L30 42 L46 32 L64 42 L64 64 L0 64 Z\" fill=\"#b45309\"/>"
      "<path d=\"M0 52 L22 44 L40 52 L64 46 L64 64 L0 64 Z\" fill=\"#78350f\"/></svg>\n" },
    { "Pictures/Mountains.svg",
      "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 64 64\">"
      "<rect width=\"64\" height=\"64\" fill=\"#bae6fd\"/>"
      "<circle cx=\"48\" cy=\"14\" r=\"6\" fill=\"#fef9c3\"/>"
      "<path d=\"M0 54 L14 30 L26 46 L40 18 L64 54 L64 64 L0 64 Z\" fill=\"#64748b\"/>"
      "<path d=\"M40 18 L47 28.5 L42 27 L38 32 L34 30 Z\" fill=\"#f8fafc\"/>"
      "<path d=\"M14 30 L18 35.3 L14 35 L10 36.9 Z\" fill=\"#f8fafc\"/>"
      "<path d=\"M0 64 L0 52 L18 58 L36 50 L64 60 L64 64 Z\" fill=\"#0f766e\"/></svg>\n" },
    { "Pictures/Lighthouse.svg",
      "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 64 64\">"
      "<rect width=\"64\" height=\"64\" fill=\"#1e3a8a\"/>"
      "<path d=\"M34 14 L64 4 L64 24 Z\" fill=\"#fde68a\"/>"
      "<circle cx=\"12\" cy=\"12\" r=\"1.5\" fill=\"#f8fafc\"/>"
      "<circle cx=\"22\" cy=\"6\" r=\"1\" fill=\"#f8fafc\"/>"
      "<path d=\"M27 50 L29 18 L35 18 L37 50 Z\" fill=\"#f8fafc\"/>"
      "<path d=\"M28 30 L36 30 L36.5 38 L27.5 38 Z\" fill=\"#dc2626\"/>"
      "<rect x=\"28\" y=\"12\" width=\"8\" height=\"6\" fill=\"#fbbf24\"/>"
      "<path d=\"M0 50 L64 46 L64 64 L0 64 Z\" fill=\"#0f172a\"/></svg>\n" },
    { "Pictures/City skyline.svg",
      "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 64 64\">"
      "<rect width=\"64\" height=\"64\" fill=\"#312e81\"/>"
      "<circle cx=\"50\" cy=\"14\" r=\"6\" fill=\"#e0e7ff\"/>"
      "<rect x=\"4\" y=\"32\" width=\"10\" height=\"32\" fill=\"#1e1b4b\"/>"
      "<rect x=\"16\" y=\"20\" width=\"12\" height=\"44\" fill=\"#0f172a\"/>"
      "<rect x=\"30\" y=\"28\" width=\"9\" height=\"36\" fill=\"#1e1b4b\"/>"
      "<rect x=\"41\" y=\"38\" width=\"10\" height=\"26\" fill=\"#0f172a\"/>"
      "<rect x=\"53\" y=\"30\" width=\"8\" height=\"34\" fill=\"#1e1b4b\"/>"
      "<rect x=\"19\" y=\"26\" width=\"2\" height=\"3\" fill=\"#fde68a\"/>"
      "<rect x=\"23\" y=\"34\" width=\"2\" height=\"3\" fill=\"#fde68a\"/>"
      "<rect x=\"33\" y=\"36\" width=\"2\" height=\"3\" fill=\"#fde68a\"/>"
      "<rect x=\"44\" y=\"44\" width=\"2\" height=\"3\" fill=\"#fde68a\"/></svg>\n" },
    { "Pictures/Wallpapers/Tide pools.svg",
      "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 64 64\">"
      "<rect width=\"64\" height=\"64\" fill=\"#0e7490\"/>"
      "<circle cx=\"18\" cy=\"20\" r=\"12\" fill=\"#22d3ee\"/>"
      "<circle cx=\"44\" cy=\"40\" r=\"16\" fill=\"#67e8f9\"/></svg>\n" },
    { "Pictures/Trips/Lisbon notes.md",
      "# Lisbon\n\nTram 28 before nine. Lunch in Alfama. The view from Graca at dusk.\n" },
    { "Music/Road trip.m3u",
      "#EXTM3U\n#EXTINF:214,The Lanterns - Open Highway\nopen-highway.mp3\n"
      "#EXTINF:189,Salt Flats - Mile Marker\nmile-marker.mp3\n" },
    { "Music/Morning focus.m3u",
      "#EXTM3U\n#EXTINF:402,Low Tide - First Light\nfirst-light.mp3\n" },
    { "Music/Dinner party.m3u",
      "#EXTM3U\n#EXTINF:251,Marina Vale - Slow Tuesday\nslow-tuesday.mp3\n"
      "#EXTINF:233,The Lanterns - Porch Song\nporch-song.mp3\n"
      "#EXTINF:305,Low Tide - Long Table\nlong-table.mp3\n" },
    { "Music/Running.m3u",
      "#EXTM3U\n#EXTINF:178,Salt Flats - Second Wind\nsecond-wind.mp3\n" },
    { "Music/Liner notes.txt",
      "Recorded over three weekends in a converted barn, mostly live, mostly in one take.\n" },
    { "Music/Concert tickets.txt",
      "Marina Vale, Coliseu dos Recreios, November 21, doors at 20:00, row F, seats 11 and 12.\n" },
    { "Projects/Garden/plan.md",
      "# Garden\n\nThree raised beds by the fence: tomatoes, beans, herbs.\n" },
    { "Projects/Garden/plants.json",
      "{ \"beds\": [ { \"name\": \"tomatoes\", \"plants\": 6 },\n"
      "            { \"name\": \"beans\", \"plants\": 12 } ] }\n" },
    { "Projects/Website/index.html",
      "<!doctype html>\n<title>Harbor Coffee</title>\n"
      "<link rel=\"stylesheet\" href=\"style.css\">\n<h1>Harbor Coffee</h1>\n"
      "<p>Open from seven, every day.</p>\n<script src=\"app.js\"></script>\n" },
    { "Projects/Website/style.css",
      "body { font: 16px/1.5 serif; margin: 4rem auto; max-width: 40rem; }\n" },
    { "Projects/Website/app.js",
      "document.title = 'Harbor Coffee, open now';\n" },
    { "Projects/Telemetry/main.c",
      "#include \"sensor.h\"\n\nint main(void)\n{\n    return sensor_read() < 0;\n}\n" },
    { "Projects/Telemetry/sensor.h",
      "#ifndef SENSOR_H\n#define SENSOR_H\nint sensor_read(void);\n#endif\n" },
    { "Projects/Telemetry/README.md",
      "# Telemetry\n\nReads one sensor and exits non-zero when it fails.\n" },
    { "Downloads/Train tickets.txt",
      "Lisbon Oriente to Porto Campanha, November 14, 09:05, coach 21, seats 41 and 42.\n" },
    { "Downloads/Lisbon itinerary.md",
      "# Lisbon, four days\n\n1. Alfama and the castle\n2. Belem and the river\n"
      "3. Sintra by train\n4. Markets, then the airport\n" },
    { "Downloads/Router settings.json",
      "{ \"ssid\": \"Harbor-5G\", \"channel\": 36, \"guest\": false }\n" },
    { "Downloads/Boarding pass.txt",
      "TP 1352, Lisbon to Oslo, November 18, gate 24, boarding 06:40, seat 14C.\n" },
    { "Downloads/Exchange rates.csv",
      "currency,per_euro\nUSD,1.08\nGBP,0.86\nNOK,11.72\nCAD,1.49\n" },
};

/* A folder with nothing in it: the selector's empty state. */
static const char *const EMPTY_FOLDERS[] = { "Documents/Archive", "Downloads/Unsorted",
                                             "Pictures/Screenshots" };

/* ---- Pictures, painted in code ---- */

/* A 32-bit BMP is the format the decoder takes that can be written with no
   compressor: a 54-byte header, then bottom-up BGRA rows, which are 4-byte aligned
   so BMP's row padding never applies. */
enum {
    SCENE_W     = 480,            /* the grid a scene is drawn on, whatever its size */
    SCENE_H     = 320,
    HORIZON     = 200,
    PICTURE_CAP = 54 + 960 * 640 * 4
};

typedef struct { int r, g, b; } Rgb;

/* What a scene is named for, drawn over its landscape. */
typedef enum { MARK_BOATS, MARK_SNOW, MARK_SAGUARO, MARK_TREES } SceneMark;

/* One landscape: a sky that fades to the horizon, a sun behind two ridges, a ground that
   darkens toward the bottom edge, and water, where there is any, mirroring it all. */
typedef struct {
    const char *path;
    int         w, h;
    Rgb         skyTop, skyLow, sun, farHill, nearHill, ground;
    int         sunX, sunY, sunR;
    bool        peaks;              /* sharp ridges rather than rolling hills */
    int         waterY;             /* the grid row the water starts at; 0 = none */
    SceneMark   mark;
} Scene;

static const Scene SCENES[] = {
    { "Pictures/Alpine lake.bmp", 960, 640, {  70, 136, 214 }, { 196, 226, 250 },
      { 255, 252, 236 }, { 118, 150, 182 }, {  46, 110,  98 }, {  24,  70,  62 },
      120, 64, 26, true, 206, MARK_SNOW },
    { "Pictures/Desert dusk.bmp", 840, 560, {  44,  50, 118 }, { 236, 122, 104 },
      { 255, 214, 124 }, { 154,  84,  94 }, {  98,  50,  72 }, {  52,  28,  44 },
      300, 168, 40, false, 0, MARK_SAGUARO },
    { "Pictures/Harbor at dawn.bmp", 720, 480, { 250, 170, 140 }, { 255, 226, 196 },
      { 255, 246, 220 }, { 178, 132, 170 }, { 104,  78, 128 }, {  60,  46,  84 },
      332, 136, 36, false, 204, MARK_BOATS },
    { "Pictures/Autumn ridge.bmp", 600, 400, { 236, 160,  84 }, { 252, 220, 150 },
      { 255, 240, 200 }, { 196, 104,  52 }, { 140,  58,  34 }, {  70,  30,  24 },
      380, 104, 30, false, 0, MARK_TREES },
};

static inline void bmp_u32(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)( v        & 0xFFu);
    p[1] = (unsigned char)((v >>  8) & 0xFFu);
    p[2] = (unsigned char)((v >> 16) & 0xFFu);
    p[3] = (unsigned char)((v >> 24) & 0xFFu);
}

/* From a to b by t, 0 to 1, per channel. */
static inline Rgb rgb_mix(Rgb a, Rgb b, float t) {
    Rgb c;

    c.r = a.r + (int)((float)(b.r - a.r) * t);
    c.g = a.g + (int)((float)(b.g - a.g) * t);
    c.b = a.b + (int)((float)(b.b - a.b) * t);
    return c;
}

/* A triangle wave: 0 to 1 and back over `span`, for x from 0 up. */
static inline float wave(float x, float span) {
    float k = x / span;

    k -= (float)(int)k;
    return k < 0.5f ? 2.0f * k : 2.0f - 2.0f * k;
}

/* The row a ridge reaches at column x (y down). Waves of three spans read as a range
   where one reads as a zigzag; smoothed, the same waves roll. */
static inline float ridge_row(const Scene *s, float x, float base, float amp, float phase) {
    float a = wave(x + phase, 190.0f), b = wave(x + 2.1f * phase, 83.0f);
    float c = wave(x + phase, 31.0f);

    if (s->peaks) return base - amp * (0.60f * a + 0.28f * b + 0.12f * c);
    return base - amp * (0.80f * a * a * (3.0f - 2.0f * a) + 0.20f * b * b * (3.0f - 2.0f * b));
}

static inline float far_ridge(const Scene *s, float x) {
    return ridge_row(s, x, (float)HORIZON, s->peaks ? 110.0f : 60.0f, 34.0f);
}

static inline float near_ridge(const Scene *s, float x) {
    return ridge_row(s, x, (float)HORIZON + 48.0f, 44.0f, 122.0f);
}

/* Is (x, y) in the upright capsule from row top to row bottom? */
static inline bool in_capsule(float x, float y, float cx, float top, float bottom, float r) {
    const float dy = y < top ? y - top : y > bottom ? y - bottom : 0.0f;

    return (x - cx) * (x - cx) + dy * dy <= r * r;
}

/* A sailboat with its deck at (u, v) = (0, 0): hull, mast and sail. */
static inline bool boat_at(const Scene *s, float u, float v, Rgb *out) {
    if (v >= 0.0f && v < 12.0f && u > v && u < 80.0f - 0.5f * v) {
        *out = s->ground;
    } else if (u >= 30.0f && u < 32.5f && v >= -66.0f && v < 0.0f) {
        *out = s->ground;
    } else if (v >= -62.0f && v < -5.0f && u >= 34.0f && u < 34.0f + (v + 62.0f) * 0.62f) {
        *out = s->sun;
    } else {
        return false;
    }
    return true;
}

/* A saguaro with its foot at (u, v) = (0, 0), and its two arms. */
static inline bool saguaro_at(float u, float v) {
    return in_capsule(u, v, 0.0f, -112.0f, 8.0f, 9.0f)
        || in_capsule(u, v, -27.0f, -84.0f, -48.0f, 6.0f)
        || in_capsule(u, v, 25.0f, -98.0f, -62.0f, 6.0f)
        || (u >= -27.0f && u < 0.0f && v >= -54.0f && v < -42.0f)
        || (u >= 0.0f && u < 25.0f && v >= -68.0f && v < -56.0f);
}

/* What the scene is named for, where it covers the point: true, its color in out. */
static inline bool scene_mark(const Scene *s, float x, float y, Rgb *out) {
    const int   cell  = (int)((x + 11.0f) / 26.0f);
    const float treeX = ((float)cell + 0.5f) * 26.0f - 11.0f;
    float       foot, top;

    switch (s->mark) {
    case MARK_BOATS:
        return boat_at(s, x - 84.0f, y - 262.0f, out)
            || boat_at(s, (x - 318.0f) / 0.62f, (y - 232.0f) / 0.62f, out);
    case MARK_SAGUARO:
        *out = s->ground;
        return saguaro_at(x - 112.0f, y - 268.0f)
            || saguaro_at((x - 392.0f) / 0.6f, (y - 244.0f) / 0.6f);
    case MARK_TREES:
        foot = near_ridge(s, treeX) + 6.0f;
        top  = foot - 26.0f - 14.0f * wave((float)cell * 37.0f, 61.0f);
        if (y < top || y > foot) return false;
        if ((x > treeX ? x - treeX : treeX - x) > (y - top) * 0.34f) return false;
        *out = cell % 3 ? s->nearHill : s->ground;
        return true;
    case MARK_SNOW:
        break;                          /* it lies on the ridge: scene_land draws it */
    }
    return false;
}

/* The sky and the land, with no water. */
static inline Rgb scene_land(const Scene *s, float x, float y) {
    const float farY  = far_ridge(s, x);
    const float nearY = near_ridge(s, x);
    const float h     = (float)HORIZON;

    /* The land's shades follow the row, not the ridge line, or each column would show. */
    if (y < farY) {
        const float dx = x - (float)s->sunX, dy = y - (float)s->sunY;
        const float d2 = dx * dx + dy * dy, r2 = (float)(s->sunR * s->sunR), glow = 4.0f * r2;
        Rgb         c  = rgb_mix(s->skyTop, s->skyLow, (y < h ? y : h) / h);

        if (d2 <= r2) return s->sun;
        return d2 < glow ? rgb_mix(c, s->sun, (glow - d2) / (2.0f * (glow - r2))) : c;
    }
    if (y < nearY) {
        const Rgb snow = { 244, 248, 252 };

        if (s->mark == MARK_SNOW
            && y < 116.0f + 12.0f * wave(x + 40.0f, 37.0f) + 6.0f * wave(x, 13.0f))
            return snow;
        return rgb_mix(s->farHill, s->nearHill, (y - 90.0f) / 150.0f);
    }
    return rgb_mix(s->nearHill, s->ground, (y - h) / ((float)SCENE_H - h));
}

/* The scene's color at a point on its grid. Water is the land mirrored at its edge, darker
   the nearer it comes, and swaying a little: slowly, over many rows, because a ripple a
   few rows long shears a mirrored ridge into steps no supersample can smooth. */
static inline Rgb scene_color(const Scene *s, float x, float y) {
    const float edge = (float)s->waterY;
    Rgb         c;

    if (scene_mark(s, x, y, &c)) return c;
    if (s->waterY && y > edge) {
        const float below = y - edge;

        c = scene_land(s, x + below * 0.01f * wave(y, 23.0f), edge - below);
        return rgb_mix(c, s->ground, 0.16f + below / 420.0f);
    }
    return scene_land(s, x, y);
}

/* One pixel: four samples averaged, so a ridge's edge stays smooth when the picture
   is drawn larger than it is. */
static inline Rgb scene_pixel(const Scene *s, int x, int y) {
    const float gridX = (float)SCENE_W / (float)s->w, gridY = (float)SCENE_H / (float)s->h;
    Rgb         sum   = { 0, 0, 0 };
    int         k;

    for (k = 0; k < 4; k++) {
        Rgb c = scene_color(s, ((float)x + 0.25f + 0.5f * (float)(k & 1)) * gridX,
                            ((float)y + 0.25f + 0.5f * (float)(k >> 1)) * gridY);

        sum.r += c.r;
        sum.g += c.g;
        sum.b += c.b;
    }
    sum.r /= 4;
    sum.g /= 4;
    sum.b /= 4;
    return sum;
}

/* Is the file at `rel` below root missing? Its whole path goes into path either way. */
static inline bool sample_missing(const char *root, const char *rel, char *path, int cap) {
    RC_PathInfo info;

    return fs_join(path, cap, root, rel) && rcPathInfo(path, &info) != RC_FILE_OK;
}

/* Paint one scene below root. */
static inline void save_scene(const char *root, const Scene *s) {
    static unsigned char bmp[PICTURE_CAP];   /* 2.4 MB: static, so it is not on the stack */
    const int bytes = 54 + s->w * s->h * 4;
    char      path[FS_PATH_CAP];
    int       k, x, r;

    if (!sample_missing(root, s->path, path, (int)sizeof path)) return;
    for (k = 0; k < 54; k++) bmp[k] = 0;
    bmp[0] = 'B';
    bmp[1] = 'M';
    bmp_u32(bmp + 2,  (unsigned)bytes);                       /* file size        */
    bmp_u32(bmp + 10, 54u);                                   /* pixel offset     */
    bmp_u32(bmp + 14, 40u);                                   /* info header size */
    bmp_u32(bmp + 18, (unsigned)s->w);
    bmp_u32(bmp + 22, (unsigned)s->h);                        /* + = bottom-up    */
    bmp[26] = 1;                                              /* planes           */
    bmp[28] = 32;                                             /* bits per pixel   */
    bmp_u32(bmp + 34, (unsigned)(s->w * s->h * 4));           /* image size       */
    for (r = 0; r < s->h; r++) {
        for (x = 0; x < s->w; x++) {
            unsigned char *o = bmp + 54 + (r * s->w + x) * 4;
            Rgb            c = scene_pixel(s, x, s->h - 1 - r);

            o[0] = (unsigned char)c.b;
            o[1] = (unsigned char)c.g;
            o[2] = (unsigned char)c.r;
            o[3] = 255;
        }
    }
    rcSaveFile(path, bmp, (size_t)bytes, RC_FILE_PARENTS);
}

/* ---- A ledger with real rows ---- */

/* Nine months of spending as a CSV of a few hundred rows, so the folder holds a file of
   some size. The numbers repeat by a small fixed recurrence: they read as real
   and every run writes the same bytes. */
static inline void save_ledger(RC_Arena *m, const char *root) {
    static const char *const SPEND[] = { "Groceries", "Rent", "Transit", "Dining", "Utilities" };
    static char csv[24 * 1024];
    char        path[FS_PATH_CAP];
    int         used = 0, month, day, seed = 41;

    if (!sample_missing(root, "Documents/Budget 2026.csv", path, (int)sizeof path)) return;
    for (month = 1; month <= 9; month++) {
        for (day = 1; day <= 28; day++) {
            RC_String row;
            int       i;

            seed = (seed * 57 + 13) % 9973;
            row  = rcFormat(m, "2026-%02d-%02d,%s,%d.%02d\n", month, day, SPEND[seed % 5],
                            8 + seed % 240, seed % 100);
            for (i = 0; i < (int)row.length && used < (int)sizeof csv; i++)
                csv[used++] = row.chars[i];
        }
    }
    rcSaveFile(path, csv, (size_t)used, RC_FILE_PARENTS);
}

/* Write whichever samples are missing. rcSaveFile leaves a whole file or none, so a first
   run cut short finishes on the next, and a user who deletes one gets it back, which is
   what a demo wants. */
static inline void seed_samples(RC_Arena *m, const char *root) {
    char path[FS_PATH_CAP];
    int  i;

    for (i = 0; i < (int)(sizeof SAMPLES / sizeof SAMPLES[0]); i++)
        if (sample_missing(root, SAMPLES[i].path, path, (int)sizeof path))
            rcSaveFile(path, SAMPLES[i].text, (size_t)fs_len(SAMPLES[i].text), RC_FILE_PARENTS);
    for (i = 0; i < (int)(sizeof SCENES / sizeof SCENES[0]); i++)
        save_scene(root, &SCENES[i]);
    save_ledger(m, root);
    for (i = 0; i < (int)(sizeof EMPTY_FOLDERS / sizeof EMPTY_FOLDERS[0]); i++)
        if (fs_join(path, (int)sizeof path, root, EMPTY_FOLDERS[i]))
            rcMakeDirectory(path, RC_FILE_PARENTS);
}

#endif /* APP_SAMPLES_H */
