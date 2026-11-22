/*
    main.c - RayClay "2000s" example: an Aqua-era jukebox (Mac OS X, 2003-2007)

    The glossy desktop of the iPod years: brushed metal, gel pills, pinstripes, an
    LCD with the song's progress, a source list, a Genre / Artist / Album browser, and
    Cover Flow with reflections over a striped song table.

    It teaches what to do when the look needs pixels no element gives you: compute
    them and hand them to rcImageUpdate. The metal, the pinstripes, the covers and
    the turning flow are all drawn here with no asset, and the flow is redrawn only
    while it moves. It also shows gels from two-stop gradients, any box as a button
    through rcClicked that Tab reaches through .focusable, rcVirtualList for the rows, a
    table that drops columns rather than scroll sideways, on-demand frames on a real
    clock, and safe-area insets.

    There are no audio files here, so the clock, not a decoder, moves the song.

    Build target: rayclay_ex03_2000s_gui  (web preset -> gui2000s.html)
*/

#include "rayclay.h"
#include "icons/rc_icons_chevron_down.h"
#include "icons/rc_icons_chevron_up.h"
#include "icons/rc_icons_list.h"
#include "icons/rc_icons_music.h"
#include "icons/rc_icons_pause.h"
#include "icons/rc_icons_play.h"
#include "icons/rc_icons_search.h"
#include "icons/rc_icons_settings.h"
#include "icons/rc_icons_skip_back.h"
#include "icons/rc_icons_skip_forward.h"
#include "icons/rc_icons_volume_2.h"

enum { ART = 256, METAL_W = 640, METAL_H = 96, PIN_H = 1024, SIDEBAR_W = 196, SPEAKER_W = 22 };
/* CELL_PAD pads every cell of the song table, its head's too, so the two line up. */
enum { COL_NAME, COL_TIME, COL_ARTIST, COL_ALBUM, COL_GENRE, COL_COUNT, CELL_PAD = 4 };
enum { SRC_MUSIC, SRC_RECENT, SRC_TOP, SRC_ROAD, SRC_NIGHT, SOURCE_COUNT };
/* NEW to NIGHT are the playlists a song is in, one bit each. */
enum { T_BROWSE, T_FLOW, TOGGLE_COUNT, NEW = 1, TOP = 2, ROAD = 4, NIGHT = 8 };
/* The era's colors that more than one place draws in, as 0xRRGGBB: the ink on a gel, on
   the LCD and on a list's head, the metal under its texture, and Aqua's deep blue. A
   head's sheen and the pale-blue alternate row are classes, "head" and "stripe". */
enum { INK = 0x1c1c1c, LCD_INK = 0x1f231c, HEAD_INK = 0x303030, METAL = 0xc4c4c4, AQUA = 0x2f63c1 };

#define COVER_SHARE 0.74f      /* a cover's side as a share of the flow's height */
#define REFLECTION  0.34f      /* how much of a cover shows mirrored in the floor */

typedef struct { uint8_t r, g, b, a; } Px;
typedef struct { const char *name, *artist, *genre; int year; uint32_t from, to; } Album;
typedef struct { const char *title; int secs, album, lists; } Track;

/* By genre, then artist: the browser lists each column in this order. */
static const Album ALBUMS[] = {
    { "Paper Lanterns",       "Hollow Coast",         "Alternative", 2006, 0x2b1055, 0xff8c42 },
    { "Lucid Water",          "Glass Harbor",         "Electronic",  2004, 0x0b3c8c, 0x7dd3fc },
    { "Afterglow Engine",     "Nova Drift",           "Electronic",  2007, 0x4a044e, 0xf472b6 },
    { "Jaguar Nights",        "Mira Solen",           "Jazz",        2002, 0x3b1d0a, 0xfbbf24 },
    { "Tenor for Two",        "Oskar Lindqvist Trio", "Jazz",        2003, 0x0c1a3a, 0x93c5fd },
    { "Candy Apple",          "Junie Vale",           "Pop",         2006, 0x7f1d1d, 0xfca5a5 },
    { "Clear Plastic Summer", "The Translucents",     "Pop",         2003, 0x115e59, 0x99f6e4 }, };
/* In album order, as a library is: the flow walks albums in the order songs list them. */
static const Track TRACKS[] = {
    { "Ember Tide", 214, 0, ROAD | NEW }, { "Paper Lanterns", 251, 0, ROAD | TOP | NEW },
    { "Salt Road", 198, 0, ROAD | NEW }, { "Aqua Interlude", 178, 1, NIGHT },
    { "Glass Reflections", 304, 1, TOP | NIGHT }, { "Undertow", 241, 1, 0 },
    { "Shallows", 189, 1, NIGHT }, { "Afterglow", 256, 2, ROAD | TOP | NEW },
    { "Coil Whine", 171, 2, NIGHT | NEW }, { "Pinstripe Sunrise", 217, 3, NIGHT },
    { "Frutiger Aero", 269, 3, TOP | NIGHT }, { "Spotted Moon", 402, 3, NIGHT },
    { "Velvet Rope", 244, 3, 0 }, { "Brass Ashtray", 233, 4, NIGHT },
    { "Ballad in F", 389, 4, TOP | NIGHT }, { "Candybar Skyline", 164, 5, ROAD | TOP },
    { "Sugar Glass", 201, 5, ROAD | NEW }, { "Red Delicious", 183, 5, ROAD | NEW },
    { "Gumdrop Freeway", 176, 5, ROAD | NEW }, { "Sour Cherry", 195, 5, NEW },
    { "Bondi Blue", 252, 6, ROAD | TOP }, { "Tangerine Tuesday", 197, 6, ROAD }, };
enum { ALBUM_N = sizeof ALBUMS / sizeof *ALBUMS, TRACK_N = sizeof TRACKS / sizeof *TRACKS };

static const char *const SOURCES[] = { "Music", "Recently Added", "Top 25 Most Played",
                                       "Road Trip", "Late Night" };
/* iTunes' source icons: a note for the library, a gear for a smart playlist, a list for a
   playlist of your own. */
static const RC_IconCallback SOURCE_ICONS[] = { rcIconMusic, rcIconSettings, rcIconSettings,
                                                rcIconList, rcIconList };
static const char *const TOGGLES[TOGGLE_COUNT] = { "Browse", "Cover Flow" };

/* A box laid over the middle of its parent, which still takes the clicks. */
static const RC_Float CENTERED = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_CENTER,
                                   .element = RC_ANCHOR_CENTER, .capture = RC_CAPTURE_PASSTHROUGH };

typedef struct {
    int         track, sel, source, sort; /* track plays; sel is the blue row; sort: cols[] index */
    bool        playing, sortDown, on[TOGGLE_COUNT];
    float       pos, flowAt;             /* pos: seek 0..100; flowAt eases to flowTo */
    double      last;                    /* the clock's last reading, which only grows */
    const char *pick[3];                 /* the browser's Genre, Artist, Album; NULL: All */
    char        query[32];
    /* Rebuilt each frame from the source and the search: the table's songs, the
       albums the browser may list, and the albums the flow shows. */
    int         shown[TRACK_N], shownCount, secs, flowIds[ALBUM_N], flowCount, flowTo;
    unsigned    avail, flowMask;         /* a bit per album */
} AppState;

/* What the window has room for. panes: how many of Browse and Cover Flow fit over the
   list; up: which of the two show, an on pane that fits; rowH and pickH: a song's row and
   a browser pick's, a finger's 44 px under a coarse pointer. */
typedef struct { bool compact, slim, coarse, up[TOGGLE_COUNT]; int panes; float rowH, pickH; } Fit;

static RC_Image g_metal, g_pin, g_flow, g_now;
static Px       g_coverPx[ALBUM_N][ART * ART];

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float absf(float v) { return v < 0.0f ? -v : v; }
/* strcmp's order: 0 for the same text (any text); the A to Z the catalog's ASCII sorts by. */
static int lexcmp(const char *a, const char *b) { while (*a && *a == *b) a++, b++; return *a - *b; }
static char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c; }

/* A parabola for the sine, near enough for a pattern or a turn: x folded
   into -pi..pi by whole turns, then sin x ~ (4/pi) x - (4/pi^2) x |x|. */
static float sine(float x) {
    x -= 6.2832f * (float)(int)((x + (x < 0.0f ? -3.1416f : 3.1416f)) / 6.2832f);
    return x * (1.2732f - 0.4053f * absf(x));
}

static RC_Gradient grad(uint32_t a, uint32_t b) {
    return RC_LIT(RC_Gradient){ rcHex(a), rcHex(b), "v" };
}

static uint8_t mix(uint32_t a, uint32_t b, int shift, float t) {
    return (uint8_t)((float)(a >> shift & 255) * (1.0f - t) + (float)(b >> shift & 255) * t);
}

/* A cover is flat, as printed sleeve art is: the flow's floor and its dimming give the depth.
   The metal is a random walk per row, so its streaks run along the grain and a wide
   window only lengthens them. The pinstripes are a sidebar wide and fit by COVER,
   which draws them at one to one, so the stripes stay even at any height. */
static void bake_textures(void) {
    static Px metal[METAL_H * METAL_W], pin[PIN_H * SIDEBAR_W];
    uint32_t  seed = 7;
    float     walk = 0.0f;

    for (int i = 0; i < ART * ART * ALBUM_N; i++) {
        int   n = i / (ART * ART), p = i % (ART * ART);
        float u = (float)(p % ART) / (float)ART, v = (float)(p / ART) / (float)ART;
        float r = (u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f);   /* distance squared */
        /* Each album its own art: waves, rings or bands, in its own two colors. */
        float k = (float)n, w = n % 3 == 0 ? u * (5.0f + k) + 3.0f * sine(v * 6.0f + k)
                            : n % 3 == 1 ? r * (40.0f + 9.0f * k) + 2.0f * u
                                         : (u + v) * (5.0f + k) + 0.7f * sine(u * 11.0f);
        float t = 0.5f + 0.5f * sine(w);
        uint32_t a = ALBUMS[n].from, b = ALBUMS[n].to;
        g_coverPx[n][p] = RC_LIT(Px){ mix(a, b, 16, t), mix(a, b, 8, t), mix(a, b, 0, t), 255 };
    }
    for (int i = 0; i < METAL_H * METAL_W; i++) {
        seed = seed * 1103515245u + 12345u;
        walk = walk * 0.99f + ((float)(seed >> 16 & 0x7fff) / 32768.0f - 0.5f) * 1.6f;
        uint8_t g = (uint8_t)(226.0f - 46.0f * (float)(i / METAL_W) / (float)METAL_H + walk);
        metal[i] = RC_LIT(Px){ g, g, (uint8_t)(g + 2), 255 };
    }
    for (int i = 0; i < PIN_H * SIDEBAR_W; i++)
        pin[i] = RC_LIT(Px){ 255, 255, 255, (uint8_t)(i / SIDEBAR_W % 4 < 2 ? 70 : 0) };
    rcImageUpdate(&g_metal, metal, METAL_W, METAL_H, 0, RC_PIXEL_RGBA8);
    rcImageUpdate(&g_pin, pin, SIDEBAR_W, PIN_H, 0, RC_PIXEL_RGBA8);
}

/* A label on chrome or in a cell: one line, cut with an ellipsis where a box clips
   it, and not selectable, as a desktop list's text is not. */
static void label(const char *s, uint16_t size, uint32_t color) {
    rcTextC(s, .color = rcHex(color), .size = size, .wrap = "e", .select = RC_SELECT_NONE);
}

/* Where a cover sits, d places from the center: the two neighbors swing out past
   the center cover, 0.92 of a side each, and the rest stack up 0.28 behind them. */
static float cover_x(float d, float side) { return (clampf(d, -1, 1) * 0.64f + d * 0.28f) * side; }

/* One cover turned on its vertical axis, painted over what is in the frame and
   standing on its reflection. For each column on screen this solves the perspective
   for the place on the cover that column shows, then walks the column down. A turned
   cover also stands further back, by `far`, so it fits under the center one. The
   reflection starts dark, past a thin gap, so a dark cover's bottom edge still shows. */
static void draw_cover(Px *fb, int fw, int fh, const Px *art, float d, float side) {
    float near = clampf(d, -1.0f, 1.0f), hw = side * 0.5f, cam = side * 2.3f;
    float sa = sine(near * 1.16f), ca = sine(near * 1.16f + 1.5708f);
    float far = cam + absf(near) * side * 0.7f;
    float cx = (float)fw * 0.5f + cover_x(d, side), cy = (float)fh * 0.42f;
    float x0 = cx - hw * ca * cam / (far - hw * sa), x1 = cx + hw * ca * cam / (far + hw * sa);
    float dim = clampf(1.0f - 0.3f * absf(near) - 0.05f * (absf(d) - absf(near)), 0.3f, 1.0f);

    for (int x = (int)clampf(x0, 0.0f, (float)fw); x < fw && (float)x < x1; x++) {
        float sx = (float)x + 0.5f - cx, s = sx * far / (ca * cam - sx * sa);
        float half = hw * cam / (far + s * sa), top = cy - half, bot = cy + half;
        float end = bot + 2.0f * half * REFLECTION, gap = side * 0.012f;
        int   tx = (int)clampf((s + hw) / side * (float)ART, 0.0f, ART - 1);

        for (int y = (int)clampf(top, 0.0f, (float)fh); y < fh && (float)y < end; y++) {
            /* A cover's top and bottom edges are both blended by coverage, so neither steps. */
            float fy = (float)y, v = (fy + 0.5f - top) / (2.0f * half);
            float a = v < 1.0f ? clampf(fy + 1.0f - top, 0.0f, 1.0f) * clampf(bot - fy, 0.0f, 1.0f)
                    : fy < bot + gap ? 0.0f : 0.25f * (1.0f - (v - 1.0f) / REFLECTION);
            int ty = (int)clampf((v < 1.0f ? v : 2.0f - v) * (float)ART, 0.0f, ART - 1);
            Px src = art[ty * ART + tx], *dst = &fb[y * fw + x];
            dst->r = (uint8_t)((float)src.r * dim * a + (float)dst->r * (1.0f - a));
            dst->g = (uint8_t)((float)src.g * dim * a + (float)dst->g * (1.0f - a));
            dst->b = (uint8_t)((float)src.b * dim * a + (float)dst->b * (1.0f - a));
        }
    }
}

/* The flow is one picture at the box's device pixels, shrunk evenly when the box
   outgrows it, so the box stretches it back at its own aspect. It is redrawn only
   when its turn, its size or its albums changed. A size is painted once it has
   rested (sw, sh, t: the size seen last and when it came): a window being resized
   changes it every frame, and a repaint is a million pixels and a new texture. Until
   then the last picture is held, and as its covers are sized by its height and
   centered, the box shows it scaled by its height and centered: cut at the sides in
   a relatively narrower box (COVER), black beside it in a wider one (CONTAIN). The
   repaint then only sharpens it. Returns how the box fits the picture. */
static RC_ImageFit render_flow(RC_App *app, const AppState *st, RC_Box box) {
    enum { FLOW_W = 2048, FLOW_H = 720 };
    static Px fb[FLOW_H * FLOW_W];
    static struct { float at; int w, h, sw, sh; unsigned mask; double t; } drawn = { .at = -1.0f };
    float s = rcWindowContentScale(rcAppCurrentWindow(app)) * rcWindowZoom(rcAppMainWindow(app));
    float f = clampf((float)FLOW_W / (box.width * s), 0.0f, 1.0f);   /* a 0 box: inf, so 1 */

    s *= clampf((float)FLOW_H / (box.height * s), 0.0f, f);
    int w = (int)(box.width * s), h = (int)(box.height * s);
    if (w != drawn.sw || h != drawn.sh) drawn.sw = w, drawn.sh = h, drawn.t = rcAppTime(app);
    /* A tenth of a second: longer than the gap between two steps of a drag, shorter
       than a soft picture is noticed. The timer brings the frame that repaints; a
       turn in the meantime repaints at once, at the last painted size. */
    float wait = 0.1f - (float)(rcAppTime(app) - drawn.t);
    RC_ImageFit fit = !drawn.w || (w == drawn.w && h == drawn.h) || wait <= 0.0f ? RC_IMAGE_FILL
                    : w * drawn.h < h * drawn.w ? RC_IMAGE_COVER : RC_IMAGE_CONTAIN;
    if (fit) rcWindowRequestFrameAfter(rcAppMainWindow(app), wait), w = drawn.w, h = drawn.h;
    if (w < 1 || h < 1 || (drawn.at == st->flowAt && drawn.w == w && drawn.h == h
                           && drawn.mask == st->flowMask)) return fit;
    drawn.at = st->flowAt, drawn.w = w, drawn.h = h, drawn.mask = st->flowMask;
    for (int i = 0; i < w * h; i++) fb[i] = RC_LIT(Px){ 0, 0, 0, 255 };
    /* Painter's order: the two ends first, the center cover last. */
    for (int lo = 0, hi = st->flowCount - 1; lo <= hi;) {
        int k = st->flowAt - (float)lo > (float)hi - st->flowAt ? lo : hi;
        if (k == lo) lo++; else hi--;
        draw_cover(fb, w, h, g_coverPx[st->flowIds[k]], (float)k - st->flowAt,
                   (float)h * COVER_SHARE);
    }
    rcImageUpdate(&g_flow, fb, w, h, 0, RC_PIXEL_RGBX8);
    return fit;
}

/* Whether s is in hay, ignoring the case of ASCII letters, as a search field does. */
static bool contains(const char *hay, const char *s) {
    for (size_t k = 0;; hay++, k = 0) {
        while (s[k] && hay[k] && lower(hay[k]) == lower(s[k])) k++;
        if (!s[k] || !*hay) return !s[k];
    }
}

/* The browser's three columns over an album: 0 genre, 1 artist, 2 name. */
static const char *album_field(const Album *a, int c) {
    return c == 0 ? a->genre : c == 1 ? a->artist : a->name;
}

/* An album stays while it matches the browser's picks in the columns before c. */
static bool passes_picks(const AppState *st, const Album *a, int c) {
    for (int i = 0; i < c; i++)
        if (st->pick[i] && lexcmp(st->pick[i], album_field(a, i))) return false;
    return true;
}

static const char *song_field(int i, int c) {
    const Album *a = &ALBUMS[TRACKS[i].album];
    return !c ? TRACKS[i].title : album_field(a, c == COL_GENRE ? 0 : c == COL_ARTIST ? 1 : 2);
}

/* Songs a and b's order in the sorted column: Time by length, the rest A to Z. */
static int song_cmp(const AppState *st, int a, int b) {
    int c = st->sort - 1, d = c == COL_TIME ? TRACKS[a].secs - TRACKS[b].secs
                                            : lexcmp(song_field(a, c), song_field(b, c));
    return st->sortDown ? -d : d;
}

/* The source and the search choose what every pane may show; the browser's picks
   then narrow the table, and the flow shows the albums the table holds. */
static void filter_songs(AppState *st) {
    st->shownCount = st->secs = st->flowCount = st->flowTo = 0, st->avail = st->flowMask = 0;
    for (int i = 0; i < TRACK_N; i++) {
        const Track *t = &TRACKS[i];
        const Album *a = &ALBUMS[t->album];
        if ((st->source != SRC_MUSIC && !(t->lists >> (st->source - 1) & 1))
            || !(contains(t->title, st->query) || contains(a->artist, st->query)
                 || contains(a->name, st->query) || contains(a->genre, st->query))) continue;
        st->avail |= 1u << t->album;
        if (!passes_picks(st, a, 3)) continue;
        int at = st->shownCount++;   /* in sorted place; equals keep the library's order */
        for (; st->sort && at > 0 && song_cmp(st, i, st->shown[at - 1]) < 0; at--)
            st->shown[at] = st->shown[at - 1];
        st->shown[at] = i;
        st->secs += t->secs;
        if (!(st->flowMask >> t->album & 1)) st->flowIds[st->flowCount++] = t->album;
        st->flowMask |= 1u << t->album;
    }
    /* The flow turns to the selection's album, decided afresh each frame, so a source
       that lacks it shows its first album and the way back finds the selection again. */
    for (int k = 0; k < st->flowCount; k++)
        if (st->flowIds[k] == TRACKS[st->sel].album) st->flowTo = k;
}

static void play_track(AppState *st, int i) {
    st->track = st->sel = i, st->pos = 0.0f, st->playing = true;
}

/* The song dir places from `from` in the list on screen, wrapping: Next, Previous,
   a song's end and the arrow keys all walk what the person sees. */
static int neighbor(const AppState *st, int from, int dir) {
    int at = 0, n = st->shownCount;

    for (int i = 0; i < n; i++) at = st->shown[i] == from ? i : at;
    return n ? st->shown[(at + dir + n) % n] : from;
}

/* A gel: a body that brightens toward the bottom under a white gloss across its top
   half, the look that gave Aqua its name. The content floats over the gloss, which is
   a half height narrower than the body, so its round ends stay inside a round gel. It
   darkens while pressed and the pointer is on it: rcShowsActive is that look and asks for
   no frame, where rcPointerDown would draw every frame Play is held. It is a Tab stop
   (.focusable), so Return or Space on it reads as rcClicked. A pointer's click (a click
   count above 0) leaves no focus on it, so Space and Return stay the player's keys. */
static bool gel(const char *id, RC_IconCallback icon, const char *t, bool on, float w, float h) {
    bool down = rcShowsActive(id) && rcIsHovered(id);

    rcBox(.id = id, .focusable = true, .pt = 1, .align = "tc", .borderRadius = "all-full",
          .wType = RC_PX(w), .hType = RC_PX(h), .gradient = on ? grad(0x1c58c8, 0x6fb0f8)
                                  : grad(down ? 0x989898 : 0xc2c2c2, down ? 0xd0d0d0 : 0xffffff),
          .shadow = { .color = rcWithAlpha(RC_BLACK, 0.275f), .y = 1, .blur = 2 },
          .className = "border border-[#5a5a5a]") {
        rcBox(.borderRadius = "all-full", .wType = RC_PX(w - h * 0.5f), .hType = RC_PX(h * 0.48f),
              .gradient = { .from = rcWithAlpha(RC_WHITE, on ? 0.51f : down ? 0.353f : 0.882f),
                            .to = rcWithAlpha(RC_WHITE, on ? 0.118f : 0.275f), .dir = "v" }) {}
        rcRow(.floating = CENTERED) {
            if (icon) icon(h * 0.5f, rcHex(on ? 0xffffff : INK));
            else label(t, 12, on ? 0xffffff : INK);
        }
    }
    if (rcClicked(id) && rcClickCount() > 0) rcSetFocus(NULL);
    return rcClicked(id);
}

/* The LCD: song, artist and album, and the song's progress between two times: the
   era's groove and its marker, an rcSlider styled by its parts' classes. */
static void lcd(RC_App *app, AppState *st) {
    RC_Arena    *mem = rcAppArena(app);
    const Track *t   = &TRACKS[st->track];
    /* The times read last frame's position; the slider below moves it, and its write
       draws the frame that reads the new one, so the times follow the marker. */
    int cur = (int)(st->pos * (float)t->secs / 100.0f), left = t->secs - cur;
    rcColumn(.px = 14, .py = 4, .align = "tc", .borderRadius = "8", .w = "grow",
             .wMax = RC_VAL(560), .gradient = grad(0xd9dec8, 0xf4f5e4),
             .className = "border border-[#6f7468]") {
        rcTextC(t->title, .font = rcFontSemibold(13), .color = rcHex(LCD_INK), .wrap = "n");
        label(rcFormat(mem, "%s - %s", song_field(st->track, COL_ARTIST),
                       song_field(st->track, COL_ALBUM)).chars, 12, 0x4b5145);
        rcRow(.gap = 8, .align = "cc", .w = "grow") {
            label(rcFormat(mem, "%d:%02d", cur / 60, cur % 60).chars, 11, LCD_INK);
            /* A press on the groove jumps the song there and a drag scrubs it, as on
               the iPod-era LCD: the era's 7 px groove and its 9 px marker, no fill. */
            rcSlider("seek", &st->pos, 0.0f, 99.9f,
                     .className = "h-[7px] bg-white/60 border border-[#5b6052]",
                     .thumbClassName = "size-[9px] bg-[#1f231c]",
                     .fillClassName = "bg-transparent");
            label(rcFormat(mem, "-%d:%02d", left / 60, left % 60).chars, 11, LCD_INK);
        }
    }
}

/* The unified toolbar: one sheet of metal under the title strip and the controls,
   painted up under a phone's status bar (the content is padded in). Wide, equal
   side clusters keep the LCD dead center. Compact, the source list, which has no
   room, becomes a menu beside the transport, and the LCD takes a row of its own
   under the controls; slim, a phone upright, each of the three gets a row. */
static void toolbar(RC_App *app, AppState *st, const Fit *fit, RC_Insets safe) {
    float d = fit->coarse ? 52.0f : 38.0f;   /* Play's size; its neighbors are 8 smaller */

    rcColumn(.bg = rcHex(METAL), .pt = safe.top, .w = "grow", .image = &g_metal,
             .className = "border-b border-[#6f6f6f]") {
        /* Chrome, not content: RC_TitlebarOptions.height freezes the OS drag strip
           in unzoomed px, so a band that grew with the zoom would stop matching it. */
        rcUnzoomed() {
            /* The bundled window controls, band-tall and flush in the corner so the OS
               rounds the close slab with the window; a phone or a browser tab draws none. */
            rcRow(.id = RC_ID_WINDOW_DRAG, .align = "cr", .w = "grow", .h = "24px") {
                rcBox(.floating = CENTERED) { label("RayClay Player", 13, 0x2a2a2a); }
                rcWindowControls();
            }
        }
        rcRow(.gap = 10, .pb = 8, .px = 14, .align = "cc", .w = "grow",
              .className = fit->slim ? "flex-col" : "flex-row") {
            rcRow(.gap = 6, .align = "cl", .w = fit->compact ? "grow" : "240px") {
                if (gel("t_prev", rcIconSkipBack, NULL, false, d - 8, d - 8))
                    play_track(st, neighbor(st, st->track, -1));
                if (gel("t_play", st->playing ? rcIconPause : rcIconPlay, NULL, false, d, d))
                    st->playing = !st->playing;
                if (gel("t_next", rcIconSkipForward, NULL, false, d - 8, d - 8))
                    play_track(st, neighbor(st, st->track, 1));
                rcSpacer() {}
                if (fit->compact)
                    rcBox(.w = "180px") {
                        rcCombo("source", &st->source, SOURCES, SOURCE_COUNT, .className = "popup");
                    }
            }
            if (!fit->compact || fit->slim) rcBox(.align = "cc", .w = "grow") { lcd(app, st); }
            rcRow(.align = "cr", .w = fit->slim ? "grow" : "240px") {
                /* Aqua's cancel button is the field's own clear x, there while it holds text. */
                rcTextInput("search", st->query, sizeof st->query, .className = "rounded-full",
                            .placeholder = "Search", .leadingIcon = rcIconSearch,
                            .clearButton = true);
            }
        }
        if (fit->compact && !fit->slim) rcBox(.pb = 8, .align = "cc", .w = "grow") { lcd(app, st); }
    }
}

/* A row of a pick list, the source list's and the browser's: Aqua blue when on, and a
   shade darker under the pointer when not. Like a gel, it is a Tab stop that a pointer's
   click leaves unfocused. Its focus ring is drawn inside the row (ring-inset), where the
   list's edge would clip a ring drawn outside. */
static bool pick_row(const char *id, const char *text, RC_IconCallback icon, bool on, float h) {
    RC_Gradient sheen = on ? grad(0x6f9fe6, AQUA) : RC_LIT(RC_Gradient){ .dir = "v" };

    rcRow(.id = id, .focusable = true, .gap = 7, .pl = (float)(icon ? 20 : 8), .align = "cl",
          .overflow = "hidden", .w = "grow", .hType = RC_PX(h), .gradient = sheen,
          .className = "ring-inset hover:bg-black/7") {
        if (icon) icon(14.0f, on ? RC_WHITE : rcHex(0x4a6da3));
        label(text, 13, on ? 0xffffff : 0x1b1b1b);
    }
    if (rcClicked(id) && rcClickCount() > 0) rcSetFocus(NULL);
    return rcClicked(id);
}

/* The list's foot shows the playing song's cover, as iTunes' artwork pane did, where
   the window is tall enough to hold it under the sources. Where a window is too short
   for the sources themselves, a phone on its side, the list scrolls: its rows are no
   floor for the window's middle, so they never push the status bar off its foot. */
static void source_list(AppState *st, const Fit *fit) {
    static int cur = -1;   /* the album g_now holds */
    int        album = TRACKS[st->track].album;
    if (cur != album) rcImageUpdate(&g_now, g_coverPx[cur = album], ART, ART, 0, RC_PIXEL_RGBA8);
    rcColumn(.id = "sources", .bg = rcHex(0xd9e0ea), .gap = 1, .pt = 8, .scroll = "v",
             .h = "grow", .wType = RC_PX(SIDEBAR_W), .image = &g_pin,
             .imageFit = RC_IMAGE_COVER, .className = "border-r border-[#8f99a6]") {
        for (int i = 0; i < SOURCE_COUNT; i++) {
            if (i == SRC_MUSIC || i == SRC_RECENT)
                rcBox(.pt = (float)(i ? 12 : 2), .pb = 3, .pl = 10)
                    label(i ? "PLAYLISTS" : "LIBRARY", 11, 0x55637a);
            if (pick_row(SOURCES[i], SOURCES[i], SOURCE_ICONS[i], st->source == i, fit->rowH + 2))
                st->source = i;
        }
        rcSpacer() {}
        if (rcViewport().height >= 660.0f)
            rcBox(.p = 8, .w = "grow", .className = "border-t border-[#8f99a6]")
                rcBox(.w = "grow", .aspectRatio = 1.0f, .image = &g_now) {}   /* a square */
    }
    rcScrollbar("sources");
}

/* One column of the browser, listing what the source, the search and the picks to
   its left allow. A pick clears the picks after it. Row -1 is All. */
static void browser_column(RC_App *app, AppState *st, const Fit *fit, int c) {
    static const char *const heads[3] = { "Genre", "Artist", "Album" };
    RC_Arena   *mem = rcAppArena(app);
    const char *seen[ALBUM_N], *id = rcFormat(mem, "col%d", c).chars;
    int         n = 0;

    for (int i = 0; i < ALBUM_N; i++)
        if ((st->avail >> i & 1) && passes_picks(st, &ALBUMS[i], c)
            && (!n || lexcmp(seen[n - 1], album_field(&ALBUMS[i], c))))
            seen[n++] = album_field(&ALBUMS[i], c);
    rcColumn(.w = "grow", .h = "grow", .className = "border-r border-[#a8a8a8]") {
        rcRow(.pl = 8, .align = "cl", .w = "grow", .h = "20px", .className = "head")
            label(heads[c], 12, HEAD_INK);
        /* Under a mouse rcScrollbar's bar takes its own 12 px of the list while it overflows,
           so it never covers the blue of a pick; under a finger a thin thumb shows only
           while the list moves. */
        rcColumn(.id = id, .bg = RC_WHITE, .scroll = "v", .w = "grow", .h = "grow") {
            for (int i = -1; i < n; i++) {
                const char *text = i < 0 ? rcFormat(mem, "All (%d %s%s)", n, heads[c],
                                                    n == 1 ? "" : "s").chars : seen[i];
                bool on = i < 0 ? !st->pick[c] : st->pick[c] && !lexcmp(st->pick[c], seen[i]);
                /* Keyed by the value, not its place: a pick or the search shifts the places. */
                if (pick_row(rcFormat(mem, "%d_%s", c, i < 0 ? "*" : seen[i]).chars, text, NULL,
                             on, fit->pickH))
                    for (int k = c; k < 3; k++) st->pick[k] = k == c && i >= 0 ? seen[i] : NULL;
            }
        }
        rcScrollbar(id);
    }
}

/* Cover Flow: a click on a side cover brings it round and picks its first song into
   *pick, which layout selects once the song table is declared. */
static void flow_pane(RC_App *app, AppState *st, float h, int *pick) {
    RC_Box       box = rcGetElementBox("flowimg");   /* last frame's; 0 at first */
    const Album *a   = &ALBUMS[st->flowIds[st->flowTo]];

    RC_ImageFit fit = render_flow(app, st, box);
    rcColumn(.bg = RC_BLACK, .pb = 8, .align = "tc", .w = "grow", .hType = RC_PX(h)) {
        rcBox(.id = "flowimg", .w = "grow", .h = "grow", .image = &g_flow, .imageFit = fit) {}
        rcTextC(a->name, .font = rcFontSemibold(14), .color = RC_WHITE, .wrap = "n");
        label(rcFormat(rcAppArena(app), "%s - %d", a->artist, a->year).chars, 12, 0xa8a8ae);
    }
    if (rcClicked("flowimg")) {
        float x = rcPointer().x - box.x - box.width * 0.5f, side = box.height * COVER_SHARE;
        int   best = 0;
        for (int k = 1; k < st->flowCount; k++) if (absf(cover_x((float)k - st->flowAt, side) - x)
                < absf(cover_x((float)best - st->flowAt, side) - x)) best = k;
        for (int i = st->shownCount - 1; i >= 0; i--)
            if (TRACKS[st->shown[i]].album == st->flowIds[best]) *pick = st->shown[i];
    }
}

static void song_row(RC_App *app, AppState *st, const Fit *fit, int i, int stripe) {
    RC_Arena   *mem = rcAppArena(app);
    const char *id  = rcFormat(mem, "tk%d", i).chars;
    const char *dur = rcFormat(mem, "%d:%02d", TRACKS[i].secs / 60, TRACKS[i].secs % 60).chars;
    bool        sel = i == st->sel;
    RC_Size     h   = RC_PX(fit->rowH - 2.0f * CELL_PAD);   /* a row is its cells and their pads */

    rcTableRow(.id = id,
               .className = sel ? "bg-[#3875d7]" : stripe ? "stripe hover:bg-[#d5e2f8]"
                                                          : "bg-white hover:bg-[#d5e2f8]");
    rcBox(.align = "cc", .w = "grow", .hType = h)
        if (i == st->track) rcIconVolume2(13.0f, sel ? RC_WHITE : rcHex(AQUA));
    /* Every column takes a cell, so rows line up: a hidden one's shows nothing. */
    for (int c = 0; c < COL_COUNT; c++) {
        rcTableNext();
        rcBox(.align = c == COL_TIME ? "cr" : "cl", .w = "grow", .hType = h)
            label(c == COL_TIME ? dur : song_field(i, c), 13, sel ? 0xffffff : 0x141414);
    }
    /* A click selects, a double click plays, as iTunes does, and a finger's tap plays. */
    if (rcClicked(id)) {
        if (fit->coarse || rcClickCount() == 2) play_track(st, i);
        st->sel = i;
    }
}

static void song_table(RC_App *app, AppState *st, const Fit *fit) {
    /* The stripes run to the foot of the pane, the signature of the look: the list
       declares at least as many rows as fit. Last frame's height; 0 at first. */
    int fill = (int)(rcGetElementBox("songs").height / fit->rowH);
    /* The table never scrolls sideways: Name keeps 180 px and the others hide by .priority,
       Genre first, and its head says how many hid. Under a mouse the scrollbar takes its own
       12 px past the last column, in the head too. */
    RC_TableColumn cols[COL_COUNT + 1] = {
        { .wType = RC_PX(SPEAKER_W) },
        { .header = "Name", .align = "cl", .id = "h_name", .wMin = RC_VAL(180) },
        { .header = "Time", .align = "cr", .w = "56px", .id = "h_time", .priority = 1 },
        { .header = "Artist", .align = "cl", .w = "170px", .id = "h_artist", .priority = 2 },
        { .header = "Album", .align = "cl", .w = "190px", .id = "h_album", .priority = 3 },
        { .header = "Genre", .align = "cl", .w = "110px", .id = "h_genre", .priority = 4 }, };

    if (rcBeginTable("songs", cols, COL_COUNT + 1, .className = fill % 2 ? "stripe" : "bg-white",
                     .headerClassName = "head select-none divide-x divide-[#b4b4b4] text-[12px]"
                                        " text-[#303030]", .cellPadding = RC_VAL(CELL_PAD))) {
        /* The list lays out as many rows as its pane held on the frame before, and .overscan
           more past each edge. On the frame the pane grows - Browse or Cover Flow closing,
           the window drawn taller - the room it gains past a row or two would stand bare for
           that frame, so the overscan is the window's height in rows (64 at most, the most a
           list takes): the pane never grows by more than the window is tall. */
        rcVirtualList(r, "songs", st->shownCount > fill ? st->shownCount : fill, fit->rowH,
                      .overscan = (int)clampf(rcViewport().height / fit->rowH, 1.0f, 64.0f)) {
            if (r.index < st->shownCount) song_row(app, st, fit, st->shown[r.index], r.index % 2);
            else {
                rcTableRow(.className = r.index % 2 ? "stripe" : "bg-white");
                rcBox(.hType = RC_PX(fit->rowH - 2.0f * CELL_PAD)) {}
            }
        }
        rcEndTable();
    }
    /* A header sorts by its column, a second click the other way, as iTunes' do; the sorted
       one shows which way. The library's order returns with a relaunch. */
    for (int c = 1; c <= COL_COUNT; c++)   /* cols[0] is the speaker's */
        if (rcClicked(cols[c].id)) st->sortDown = st->sort == c && !st->sortDown, st->sort = c;
    /* The arrow stands on the far side of a right-aligned heading, clear of its text, and
       passes the pointer on: a click on it sorts, as on the rest of the header. */
    RC_Anchor side = st->sort == COL_TIME + 1 ? RC_ANCHOR_CENTER_LEFT : RC_ANCHOR_CENTER_RIGHT;
    const uint64_t hidden = rcTableHiddenColumns("songs");
    if (st->sort && !(hidden >> st->sort & 1))
        rcBox(.px = 6, .floating = { .to = RC_ATTACH_ELEMENT, .toId = cols[st->sort].id,
                                     .parent = side, .element = side,
                                     .capture = RC_CAPTURE_PASSTHROUGH })
            (st->sortDown ? rcIconChevronDown : rcIconChevronUp)(10.0f, rcHex(0x303030));
    int n = 0;
    for (uint64_t hid = hidden; hid; hid >>= 1) n += (int)(hid & 1);
    /* A hidden column stays sortable: the tip names it under its header's id, and a tap sorts. */
    if (n) rcRow(.gap = 6, .pr = st->sort == 1 ? 22.0f : 7.0f,   /* clear of Name's arrow */
                 .floating = { .to = RC_ATTACH_ELEMENT, .toId = "h_name",
                               .parent = RC_ANCHOR_CENTER_RIGHT,
                               .element = RC_ANCHOR_CENTER_RIGHT }) {
        label(rcFormat(rcAppArena(app), "+%d column%s:", n, n == 1 ? "" : "s").chars, 11, 0x2c4a78);
        for (int c = 2; c <= COL_COUNT; c++)
            if (hidden >> c & 1) rcRow(.id = cols[c].id, .gap = 2, .align = "cc") {
                label(cols[c].header, 11, st->sort == c ? 0x141414 : 0x2c4a78);
                if (st->sort == c)
                    (st->sortDown ? rcIconChevronDown : rcIconChevronUp)(9.0f, rcHex(0x141414));
            }
    }
    rcScrollbar("songs");
}

/* Brings the selected row into view by the least move. The list declares only the rows
   on screen, so a row out of view has no box for rcScrollIntoView to find: rcScrollToRow
   places it by its index in the list instead. */
static void reveal_selection(const AppState *st) {
    for (int i = 0; i < st->shownCount; i++)
        if (st->shown[i] == st->sel) rcScrollToRow("songs", i, RC_SCROLL_NEAREST);
}

/* The view toggles and the iTunes count: "1 song, 3.6 minutes, 3.4 MB", or the library's
   "22 songs, 1.4 hours, 82.0 MB". */
static void status_bar(RC_App *app, AppState *st, const Fit *fit, float safeBottom) {
    bool     hours = st->secs >= 3600, brief = rcViewport().width < 360.0f;   /* the count alone */
    /* The count sits centered under the LCD, as in iTunes: the middle of the bar above its
       safe area. Slim, it stays in the row, at the right, clear of the toggles. */
    RC_Float at    = CENTERED;

    at.offset.y = -0.5f * safeBottom;
    if (fit->slim) at = RC_LIT(RC_Float){ .to = RC_ATTACH_NONE };

    rcRow(.bg = rcHex(METAL), .gap = 6, .pb = (float)safeBottom, .px = 10, .align = "cl",
          .w = "grow", .hType = RC_PX(32.0f + safeBottom), .image = &g_metal,
          .className = "border-t border-[#7d7d7d]") {
        /* In a short or slim window the two panes take turns: turning one on turns the other off.
           Where neither fits, a gel would switch nothing, so none is drawn. */
        for (int i = 0; i < TOGGLE_COUNT && fit->panes; i++)
            if (gel(TOGGLES[i], NULL, TOGGLES[i], fit->up[i], i ? 84.0f : 64.0f, 22.0f)) {
                st->on[i] = !fit->up[i];
                if (st->on[i] && fit->panes == 1) st->on[!i] = false;
            }
        rcBox(.align = "cr", .w = fit->slim ? "grow" : "fit", .floating = at,
              .className = "min-w-0")
            label(!st->shownCount ? "No items"
                  : brief ? rcFormat(rcAppArena(app), "%d song%s", st->shownCount,
                                     st->shownCount == 1 ? "" : "s").chars
                  : rcFormat(rcAppArena(app), "%d song%s, %.1f %s, %.1f MB", st->shownCount,
                             st->shownCount == 1 ? "" : "s", st->secs / (hours ? 3600.0 : 60.0),
                             hours ? "hours" : "minutes", st->secs * 0.016).chars, 12, 0x262626);
    }
}

static void layout(RC_App *app, void *userData) {
    AppState  *st  = (AppState *)userData;
    double     now = rcAppTime(app);
    /* Frames come a second apart while a song plays, so a longer gap is the app not
       running - asleep, suspended - and that is not playback. The readings stay double
       and only their difference is a float: a float holding the clock itself steps
       1/64 s after 36 hours and 1 s after 97 days, and the turn and the song would
       move in those steps. */
    float      dt  = st->last > 0.0 ? clampf((float)(now - st->last), 0.0f, 2.0f) : 0.0f;
    float      gap = (float)st->flowTo - st->flowAt, secs = (float)TRACKS[st->track].secs;

    /* THE CLOCK runs first, so everything below reads this frame's song and flow. */
    st->last = now;
    /* The position is a percentage: the per-second step comes from the song's length. */
    if (st->playing) st->pos += dt * 100.0f / secs;
    if (st->pos >= 100.0f) play_track(st, neighbor(st, st->track, 1));
    /* The flow eases to its target on the clock (frames for it are asked at the end). */
    st->flowAt = absf(gap) < 0.002f ? (float)st->flowTo
                                    : st->flowAt + gap * clampf(dt * 10.0f, 0.0f, 1.0f);
    /* While a song plays the readout is what moves, and it counts whole seconds: ask
       for the frame that shows the next one and park until then. */
    float at = st->pos * secs / 100.0f;
    if (st->playing) rcWindowRequestFrameAfter(rcAppMainWindow(app), (float)(int)at + 1.0f - at);

    AppState    was  = *st;
    RC_Viewport vp   = rcViewport();
    /* rcViewport().safe is already in layout units; spend each side once: top in the
       toolbar, bottom in the status bar, the sides on the root. The root is the window's
       metal, so a phone's side cutout shows a strip of the chrome, not a bare white one. */
    float       room = vp.width - vp.safe.left - vp.safe.right;
    /* Compact is a phone's width, or a desktop window zoomed as narrow: the source list
       becomes a menu. Slim is a phone upright, too narrow for Search beside that menu. A
       short window, or a slim one, has room for one pane over the list, not both, so the
       songs keep most of it, and a compact or finger-driven one that is shorter still, a
       phone on its side, for the list alone. */
    bool        compact = room < 853.0f, coarse = rcPointerIsCoarse();
    int         panes = (compact || coarse) && vp.height < 450.0f ? 0
                      : vp.height < 680.0f || room < 640.0f ? 1 : 2;
    Fit         fit  = { .compact = compact, .slim = room < 640.0f, .coarse = coarse,
                         .up = { panes && st->on[T_BROWSE] && !(panes == 1 && st->on[T_FLOW]),
                                 panes && st->on[T_FLOW] }, .panes = panes,
                         .rowH = coarse ? 44.0f : 22.0f, .pickH = coarse ? 44.0f : 20.0f };
    /* A key's or a cover's pick becomes the selection after the song table is declared: the
       table keeps the old row lit this frame, and the next shows the new one lit and in view,
       as rcScrollToRow moves the list on the frame after its call. */
    int         picked = -1;
    if (!g_metal.handle) bake_textures();
    /* The leader's keys, unless Search is typing: Space plays and pauses, Return plays
       the selection, Up and Down move it (and the flow turns with it). */
    if (!rcIsFocused("search")) {
        if (rcKeyPressed(RC_KEY_SPACE)) st->playing = !st->playing;
        if (rcKeyPressed(RC_KEY_ENTER)) play_track(st, st->sel);
        if (rcKeyPressed(RC_KEY_DOWN) || rcKeyPressed(RC_KEY_UP))
            picked = neighbor(st, st->sel, rcKeyPressed(RC_KEY_DOWN) ? 1 : -1);
    }
    rcColumn(.bg = rcHex(METAL), .pl = vp.safe.left, .pr = vp.safe.right,
             .w = "grow", .h = "grow") {
        toolbar(app, st, &fit, vp.safe);
        /* Read after the toolbar, so a keystroke in Search filters this frame. */
        filter_songs(st);
        /* flowAt counts places in the albums shown, so another set of albums would start
           the turn from an unrelated cover: a new set opens on its target instead. */
        if (st->flowMask != was.flowMask) st->flowAt = (float)st->flowTo;
        rcRow(.w = "grow", .h = "grow") {
            if (!fit.compact) source_list(st, &fit);
            /* min-w-0: the song table's columns are no floor for the pane, so the table fits
               the width beside the source list and drops columns rather than run past it. */
            rcColumn(.w = "grow", .h = "grow", .className = "min-w-0") {
                /* A head, five whole rows (three of a finger's) and the rule under them. */
                if (fit.up[T_BROWSE])
                    rcRow(.w = "grow", .hType = RC_PX(21.0f + (coarse ? 3.0f : 5.0f) * fit.pickH),
                          .className = "border-b border-[#9a9a9a]") {
                        for (int c = 0; c < 3; c++) browser_column(app, st, &fit, c);
                    }
                if (fit.up[T_FLOW] && st->flowCount)
                    flow_pane(app, st, clampf(vp.height * 0.38f, 150.0f, 360.0f), &picked);
                song_table(app, st, &fit);
                if (picked >= 0) st->sel = picked;
            }
        }
        status_bar(app, st, &fit, vp.safe.bottom);
    }
    /* A new selection brings its row into view; filter_songs turns the flow to it. */
    if (was.sel != st->sel) reveal_selection(st);
    /* The flow asks for frames while it is short of its target. Asked here, after
       filter_songs set this frame's target, so a new selection never leaves it unturned. */
    if (st->flowAt != (float)st->flowTo) rcWindowRequestFrame(rcAppMainWindow(app));
    /* Another source or search may hold none of the browser's picks: start from All. */
    if (was.source != st->source || lexcmp(was.query, st->query))
        st->pick[0] = st->pick[1] = st->pick[2] = NULL;
    /* A click changes state part-way through the frame that declares it, and on
       demand nothing else draws the next one, so ask whenever the state moved. */
    if (was.sel != st->sel || was.track != st->track || was.source != st->source
        || was.playing != st->playing || was.on[0] != st->on[0] || was.on[1] != st->on[1]
        || was.pick[0] != st->pick[0] || was.pick[1] != st->pick[1] || was.pick[2] != st->pick[2]
        || was.sort != st->sort || was.sortDown != st->sortDown)
        rcWindowRequestFrame(rcAppMainWindow(app));
}

int main(void) {
    static AppState state = { .track = 10, .sel = 10, .playing = true, .on = { true, true },
                              .pos = 24.0f };
    static const float sizes[] = { 13.0f };   /* slot 0 is the search field's size */

    rcSetStyle(rcStyleLight());   /* the era is light: the stock field and scrollbars follow */
    rcDefineClass("stripe", "bg-[#edf3fe]");   /* the songs' pale-blue alternate row */
    /* A list's head, the browser columns' and the song table's: a sheen over a rule. */
    rcDefineClass("head", "bg-linear-to-b from-[#fbfbfb] to-[#d9d9d9] border-b border-[#9a9a9a]");
    /* The compact source menu is Aqua's pop-up button, a gel lozenge as the transport's are,
       held down while its list is open. No height or text size: it stays as tall as the
       search pill and a finger's row. */
    rcDefineClass("popup", "rounded-full border-[#5a5a5a] bg-linear-to-b from-white to-[#d4d4d4] "
                           "open:from-[#d0d0d0] open:to-[#989898]");
    RC_AppOptions opts = {
        .width = 1280, .height = 820, .title = "RayClay Player", .clearColor = rcHex(METAL),
        /* The arena backs rcFormat's text. */
        .fontSizes = sizes, .fontCount = 1, .scratchArenaBytes = 32768,
        .frame = RC_WINDOW_FRAME_NONE, .layoutCallback = layout, .userData = &state,
        .titlebar = { .height = 24 },
    };

    return rcRunApp(&opts);
}
