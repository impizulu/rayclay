/*  main.c - RayClay 2000s example: an Aqua / Winamp-style media player.

    Glossy vertical gradients, soft drop shadows, saturated aqua chrome and
    pill-rounded transport buttons. Shows a custom draggable titlebar, the
    rcSlider / rcProgress / rcScrollbar widgets, compiled-in icons, on-demand
    frames driven by a real clock, and safe-area insets. The seek bar advances
    by itself while playing, so the loop is visible.

    Zero-asset: the bundled Latin-1 font is baked at runtime and the glyphs are
    icon headers. One source, desktop and web.

    Build target: rayclay_ex03_2000s_gui  (web preset -> gui2000s.html)
*/

#include "rayclay.h"

#include "icons/rc_icons_rayclay_logo.h"

/* Transport glyphs, compiled in as headers - no files are loaded. */
#include "icons/rc_icons_pause.h"
#include "icons/rc_icons_play.h"
#include "icons/rc_icons_skip_back.h"
#include "icons/rc_icons_skip_forward.h"
#include "icons/rc_icons_square.h"

/* Font ladder baked from the bundled face at these sizes. */
typedef enum { F_SMALL = 0, F_BODY, F_TITLE, F_COUNT } AppFont;

#define TRACK_COUNT 8

/* SECONDS, not frames: long enough to outlast a drag's inter-frame gaps, short
   enough that playback resumes on release. A frame count would be four times
   shorter on a 240 Hz display than on a 60 Hz one. */
#define SEEK_SCRUB_HOLD 0.18

typedef struct {
    int   track;      /* index of the current track                 */
    bool  playing;    /* transport state (Play toggles it)          */
    float pos;        /* seek position 0..100 (advances while live) */
    float vol;        /* volume 0..100                              */
    double scrub;     /* seek-scrub guard: pauses auto-advance while > 0, in seconds */
    double last;      /* rcAppTime at the previous update, for the true delta        */
} AppState;

/* One playlist entry. secs mirrors the printed duration, so the seek readout
   tracks whichever track is selected. */
typedef struct {
    const char *title;
    const char *dur;
    int         secs;
} Track;

static const Track g_tracks[TRACK_COUNT] = {
    { "Silica Dreams",        "3:45", 225 },
    { "Aqua Interlude",       "2:58", 178 },
    { "Bondi Blue",           "4:12", 252 },
    { "Brushed Metal",        "3:20", 200 },
    { "Glass Reflections",    "5:04", 304 },
    { "Pinstripe Sunrise",    "3:37", 217 },
    { "Candybar Skyline",     "2:44", 164 },
    { "Frutiger Aero",        "4:29", 269 },
};

/* Eight art gradients, one per track. Packed hex rather than RC_Color: rcRgb
   expands to a compound literal, which C99 forbids as a file-scope initialiser;
   rcHex turns them back at the call site. */
static const uint32_t g_art_from[TRACK_COUNT] = {
    0x22d3ee, 0x38bdf8, 0x60a5fa, 0x7dd3fc,
    0x2dd4bf, 0x818cf8, 0x0ea5e9, 0xa5b4fc,
};
static const uint32_t g_art_to[TRACK_COUNT] = {
    0x3730a3, 0x1d4ed8, 0x4338ca, 0x0369a1,
    0x0f766e, 0x312e81, 0x075985, 0x3b0764,
};

/* Derived once at startup and kept at file scope ON PURPOSE. rcTextC stores the
   POINTER, not a copy, and reads it when the frame is drawn - two initials in a
   local char[3] inside the layout function give a blank tile and no warning.
   Anything but a literal must outlive the frame: rcFormat's arena, or storage. */
static char g_initials[TRACK_COUNT][3];

static void track_initials(const char *title, char out[3])
{
    int  n = 0;
    bool wordStart = true;

    for (; *title && n < 2; title++) {
        if (*title == ' ') {
            wordStart = true;
            continue;
        }
        if (wordStart) {
            out[n++]  = *title;
            wordStart = false;
        }
    }
    out[n] = '\0';
}

/* Stable per-row ids for the playlist (rcClicked needs a unique id per row). */
static const char *const g_row_ids[TRACK_COUNT] = {
    "tk0", "tk1", "tk2", "tk3", "tk4", "tk5", "tk6", "tk7",
};

/* The Aqua gloss: light sky at the top, deep blue at the base. RC_LIT is the one
   spelling of a compound literal that compiles as both C99 and C++20. */
static RC_Gradient aqua_gloss(void) {
    return RC_LIT(RC_Gradient){ .from = RC_SKY_400, .to = RC_BLUE_700, .dir = "v" };
}

static RC_Shadow soft_shadow(void) {
    return RC_LIT(RC_Shadow){ .color = rcAlpha(RC_BLACK, 110), .y = 3.0f, .blur = 8.0f };
}

/* Transport button: a glossy pill around an icon. rcClicked turns the styled box
   into a button (and gives it the pointer cursor); rcIsHovered brightens it. */
static bool transport_btn(const char *id, RC_IconCallback icon, bool primary) {
    RC_Gradient g = aqua_gloss();
    if (rcIsHovered(id)) {
        g.from = RC_SKY_300;   /* lift the gloss on hover */
        g.to   = RC_BLUE_600;
    }
    /* The play button reads a touch larger. RC_PX(expr) is the typed form and
       takes a value computed this frame; the string form (.h = "40px") is parsed. */
    rcBox(.id = id, .align = "cc", .borderRadius = "all-full",
          .border = { .color = rcAlpha(RC_WHITE, 90), .width = "1px" }, .h = "40px",
          .wType = RC_PX(primary ? 58 : 46), .gradient = g, .shadow = soft_shadow()) {
        icon(primary ? 22.0f : 18.0f, RC_WHITE);
    }
    return rcClicked(id);
}

/* One 12px Aqua gel, tagged with a window-control id so the runner performs the
   OS action. Hand-rolled rather than rcWindowControlButton: the era is circles. */
static void aqua_light(const char *winId, unsigned rgb) {
    rcBox(.id = winId, .bg = rcHex(rgb), .borderRadius = "all-full",
          .border = { .color = rcAlpha(RC_BLACK, 70), .width = "1px" }, .w = "12px",
          .h = "12px") {}
}

static void titlebar(void) {
    /* Chrome, not content: RC_AppOptions.titlebarHeight freezes the OS drag strip
       in physical px, so a band that grew with the zoom would stop matching it. */
    rcUnzoomed() {
        rcRow(.id = RC_ID_WINDOW_DRAG, .gap = 8, .px = 10, .align = "cl", .w = "grow",
              .h = "26px", .gradient = aqua_gloss()) {
            /* A window to close, minimise or zoom exists only where the platform
               draws windows; on web and mobile these ids do nothing, so the gels
               would be dead chrome. rcChildWindowsSupported answers that. */
            if (rcChildWindowsSupported()) {
                aqua_light(RC_ID_WINDOW_CLOSE,    0xff5f57);
                aqua_light(RC_ID_WINDOW_MINIMIZE, 0xfebc2e);
                aqua_light(RC_ID_WINDOW_MAXIMIZE, 0x28c840);
            }
            rcIconRayClayLogo(16.0f);
            rcTextL("RayClay Player", .font = F_BODY, .color = RC_WHITE);
            rcBox(.w = "grow") {}
        }
    }
}

static void now_playing(const AppState *st) {
    const Track *t = &g_tracks[st->track];

    rcRow(.gap = 14, .align = "cl", .w = "grow") {
        /* Art colours and letters both come from the track that is playing. */
        rcBox(.id = "album", .align = "cc", .borderRadius = "all-lg",
              .border = { .color = rcAlpha(RC_WHITE, 70), .width = "1px" }, .w = "72px",
              .h = "72px",
              .gradient = { .from = rcHex(g_art_from[st->track]),
                            .to = rcHex(g_art_to[st->track]), .dir = "d" },
              .shadow = soft_shadow()) {
            rcTextC(g_initials[st->track], .font = F_TITLE,
                     .color = rcAlpha(RC_WHITE, 225));
        }
        rcColumn(.gap = 4) {
            rcTextC(t->title, .font = F_TITLE, .color = RC_WHITE);
            rcTextL("RayClay Sound System", .font = F_SMALL, .color = RC_SKY_200);
            rcRow(.gap = 6, .align = "cl") {
                rcBox(.bg = st->playing ? rcAlpha(RC_SKY_500, 90)
                                         : rcAlpha(RC_SLATE_500, 90),
                       .px = 8, .py = 2, .borderRadius = "all-full") {
                    /* A badge is one line: .wrap = "n" stops it breaking after "Now". */
                    rcTextC(st->playing ? "Now Playing" : "Paused", .font = F_SMALL,
                             .color = RC_WHITE, .wrap = "n");
                }
            }
        }
    }
}

static void playlist_row(int i, bool current) {
    const char *id = g_row_ids[i];
    RC_Color bg = current ? rcAlpha(RC_SKY_500, 150)
                            : (rcIsHovered(id) ? rcAlpha(RC_SKY_400, 70)
                                                : RC_TRANSPARENT);
    rcRow(.id = id, .bg = bg, .gap = 8, .px = 10, .align = "cl",
          .borderRadius = "all-md", .w = "grow", .h = "30px") {
        rcTextC(current ? ">" : " ", .font = F_SMALL, .color = RC_WHITE);
        rcBox(.overflow = "hidden", .w = "grow") {
            rcTextC(g_tracks[i].title, .font = F_BODY,
                     .color = current ? RC_WHITE : RC_SKY_100, .wrap = "n");
        }
        rcTextC(g_tracks[i].dur, .font = F_SMALL, .color = RC_SKY_200);
    }
}

static void update(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    /* The transport runs on the CLOCK, not on frames: a fixed step per frame would
       make the track a quarter as long on a 240 Hz panel as on a 60 Hz one.
       rcAppTime is monotonic, so the delta between two readings is real seconds. */
    double now = rcAppTime(app);
    double dt  = (st->last > 0.0 && now > st->last) ? now - st->last : 0.0;

    st->last = now;
    if (dt > 0.25)            /* a long park is not elapsed playback */
        dt = 0.25;
    if (st->scrub > 0.0) {
        st->scrub -= dt;
        if (st->scrub < 0.0)
            st->scrub = 0.0;
    }
    /* Advance the seek bar while playing and wrap at the end. The position is a
       PERCENTAGE, so the per-second step comes from the track's own length and
       the readout beside it stays true. Held off while the user scrubs. */
    if (st->playing && st->scrub == 0.0) {
        st->pos += (float)(dt * 100.0 / (double)g_tracks[st->track].secs);
        if (st->pos >= 100.0f) {
            st->pos = 0.0f;
            st->track = (st->track + 1) % TRACK_COUNT;   /* auto-advance */
        }
    }
    /* The transport moves with no input at all, and RayClay draws only when
       something asks it to - so while the track plays (or the post-drag hold is
       still running) ask for one more frame. Paused, the window parks at ~0 CPU. */
    if (st->playing || st->scrub > 0.0)
        rcWindowRequestFrame(rcAppMainWindow(app));
}

static void layout(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;

    /* Dark player body so the aqua gloss and white highlights pop. */
    RC_Color body   = rcColor("#0b1a2e");
    RC_Color panel  = rcAlpha(RC_SLATE_900, 235);
    RC_Color glass  = rcAlpha(RC_WHITE, 22);

    /* SAFE AREA. A phone draws edge to edge, under the status bar and the home
       indicator, and nothing moves your content out of the way. rcViewport().safe
       gives the margins ALREADY IN LAYOUT UNITS - spend them once, at the root.
       All zero on desktop unless RAYCLAY_SAFE_INSETS stands a phone's bands in. */
    RC_Insets safe = rcViewport().safe;

    rcColumn(.id = "Root", .bg = body, .pt = (uint16_t)(safe.top),
             .pb = (uint16_t)(safe.bottom), .pl = (uint16_t)(safe.left),
             .pr = (uint16_t)(safe.right), .w = "grow", .h = "grow") {
        titlebar();

        /* The page scrolls, so a short viewport reaches the playlist instead of
           squeezing it. A "grow" child of a scrolling column takes its own content
           height when the page overflows and the leftover height when it does not. */
        rcColumn(.id = "page", .gap = 14, .p = 16, .scroll = "v", .w = "grow",
                 .h = "grow") {
            now_playing(st);

            /* Seek: the scrubber, the elapsed fill under it, elapsed / remaining. */
            rcColumn(.id = "seekpanel", .bg = glass, .gap = 6, .p = 10,
                     .borderRadius = "all-lg",
                     .border = { .color = rcAlpha(RC_WHITE, 40), .width = "1px" },
                     .w = "grow", .shadow = soft_shadow()) {
                if (rcSlider("seek", &st->pos, 0.0f, 100.0f))
                    st->scrub = SEEK_SCRUB_HOLD;   /* user is dragging - back off */
                rcProgress("elapsed", st->pos / 100.0f);
                rcRow(.align = "cl", .w = "grow") {
                    int total = g_tracks[st->track].secs;
                    int cur   = (int)(st->pos * (float)total / 100.0f);
                    int left  = total - cur;
                    RC_String time = rcFormat(rcAppArena(app),
                                                 "%d:%02d / %d:%02d",
                                                 cur / 60, cur % 60,
                                                 total / 60, total % 60);
                    RC_String rem  = rcFormat(rcAppArena(app), "-%d:%02d",
                                                 left / 60, left % 60);
                    rcText(time, .font = F_SMALL, .color = RC_SKY_100);
                    rcBox(.w = "grow") {}
                    rcText(rem, .font = F_SMALL, .color = RC_SKY_200);
                }
            }

            rcRow(.gap = 10, .align = "cc", .w = "grow") {
                /* Reset the position on every track change: it is a percentage of
                   the CURRENT track, so carrying it across a skip lands you partway
                   into a song you just started and the elapsed readout lies. */
                if (transport_btn("t_prev", rcIconSkipBack, false)) {
                    st->track = (st->track + TRACK_COUNT - 1) % TRACK_COUNT;
                    st->pos   = 0.0f;
                }
                if (transport_btn("t_play", st->playing ? rcIconPause : rcIconPlay, true))
                    st->playing = !st->playing;
                if (transport_btn("t_stop", rcIconSquare, false)) {
                    st->playing = false;
                    st->pos = 0.0f;
                }
                if (transport_btn("t_next", rcIconSkipForward, false)) {
                    st->track = (st->track + 1) % TRACK_COUNT;
                    st->pos   = 0.0f;
                }
            }

            rcRow(.gap = 10, .align = "cl", .w = "grow") {
                rcTextL("Volume", .font = F_SMALL, .color = RC_SKY_100);
                rcBox(.w = "grow") { rcSlider("vol", &st->vol, 0.0f, 100.0f); }
                RC_String vpct = rcFormat(rcAppArena(app), "%.0f%%", st->vol);
                rcText(vpct, .font = F_SMALL, .color = RC_SKY_200);
            }

            /* Not a scroll container of its own: the page scrolls, and a nested
               scroller under a finger would swallow a drag it cannot use. */
            rcColumn(.id = "tracks", .bg = panel, .gap = 2, .p = 8,
                     .borderRadius = "all-lg",
                     .border = { .color = rcAlpha(RC_WHITE, 30), .width = "1px" },
                     .w = "grow", .h = "grow", .shadow = soft_shadow()) {
                rcRow(.pb = 4, .px = 10, .align = "cl", .w = "grow") {
                    rcBox(.w = "grow") {
                        rcTextL("Playlist", .font = F_BODY, .color = RC_WHITE);
                    }
                    RC_String n = rcFormat(rcAppArena(app), "%d tracks",
                                              TRACK_COUNT);
                    rcText(n, .font = F_SMALL, .color = RC_SKY_200);
                }
                for (int i = 0; i < TRACK_COUNT; i++)
                    playlist_row(i, i == st->track);
            }
        }
    }

    /* Playlist row clicks: applied AFTER the column closes (no in-flight edits). */
    for (int i = 0; i < TRACK_COUNT; i++) {
        if (rcClicked(g_row_ids[i])) {
            st->track = i;
            st->pos = 0.0f;
        }
    }

    /* Floating, declared in-layout; layers itself above "page". */
    rcScrollbar("page");
}

int main(void) {
    AppState state = {
        .track   = 0,
        .playing = true,
        .pos     = 24.0f,
        .vol     = 70.0f,
        .scrub   = 0,
    };

    for (int i = 0; i < TRACK_COUNT; i++)
        track_initials(g_tracks[i].title, g_initials[i]);

    static const float fontSizes[F_COUNT] = {
        [F_SMALL] = 12.0f,
        [F_BODY]  = 15.0f,
        [F_TITLE] = 18.0f,
    };

    /* The widgets this player borrows - both sliders, the elapsed bar and the
       scrollbar's drag colour - paint in RC_Style.primary, so it is aqua here. */
    RC_Style aqua     = rcStyleDark();
    aqua.primary      = RC_SKY_500;
    aqua.primaryHover = RC_SKY_400;
    rcSetStyle(aqua);

    RC_AppOptions opts = {
        .width          = 420,
        .height         = 620,
        .title          = "RayClay Player",
        .clearColor     = rcColor("#0b1a2e"),
        .fontSizes      = fontSizes,
        .fontCount      = F_COUNT,
        .scratchArenaBytes = 4096,   /* backs rcFormat (time / volume / count) */
        .nativeFrame    = true,
        .titlebarHeight = 26,
        .updateCallback       = update,
        .layoutCallback       = layout,
        .userData       = &state,
        .titlebar       = { .custom = true },   /* we draw the Aqua bar ourselves */
    };

    return rcRunApp(&opts);
}
