/*
    ui_player.h - Memories: a story that plays full bleed, its captions on the playhead

    Included once by gallery_app.c. This is where the app teaches media, and it folds in what
    a story player needs from RayClay:

      rcMediaOpen(src, .alt)         open once, on the click; a .webm beside the .mp4
      rcMediaRead(clip)              every frame: state, time, duration, size, muted, rate
      rcMediaImage(clip)             the frame to draw: .image = rcMediaImage(clip)
      rcMediaPlay / rcMediaPause     play() and pause()
      rcMediaSeek(clip, seconds)     the slider and the transcript
      rcMediaSetRate / SetMuted      playbackRate and muted
      rcAudioSession(PLAYBACK)       iOS: play through the silent switch, as a video app does
      rcSetKeepScreenOn(on)          the screen stays awake while it plays
      rcPreferredLocales(tags, n)    the captions start in the user's language, if the film has it

    No callback and no thread: the runner draws a frame when the clip moves on, at the video's
    own rate while it plays, and none while it is paused. Captions are the app's own timed
    lines, shown against .time as a page shows a <track>.

    One player serves three things. "Harbor at Dawn" is a video. Every other memory, and the
    toolbar's Slideshow, is photographs under a slow pan with a clock of their own. Both fill
    the same RC_MediaInfo, so the chapter bar, the captions, the transcript and the controls
    are one piece of code that never asks which it is drawing.

    WHERE THE FILM PLAYS: macOS and iOS (AVPlayer), Android (MediaPlayer), Windows (the Media
    Engine), the web (the browser's <video>) and Linux, whose player takes the .webm. A host
    that can play neither file reads FAILED naming why: the player shows the reason over the
    cover, grays the controls, and the transcript still tells the story.
*/
#ifndef UI_PLAYER_H
#define UI_PLAYER_H

#define PLAYER_SLIDE  4.0f    /* seconds a photograph holds the screen  */
#define PLAYER_FADE   0.22f   /* the share of a slide spent crossfading */
#define PLAYER_PANEL  340

static bool player_is_story(const GalPlayer *p) { return p->clip == 0; }

static double player_story_length(const GalPlayer *p) {
    return (double)p->listCount * PLAYER_SLIDE;
}

/* A story fills the same record a film's rcMediaRead does. */
static RC_MediaInfo player_read(const GalPlayer *p) {
    if (!player_is_story(p))
        return rcMediaRead(p->clip);
    const double length = player_story_length(p);
    RC_MediaInfo m = { .state = p->time >= length ? RC_MEDIA_ENDED
                                : p->playing ? RC_MEDIA_PLAYING : RC_MEDIA_PAUSED };
    m.time = p->time;
    m.duration = length;
    m.rate = p->rate;
    m.volume = 1.0f;
    return m;
}

/* Lines: a film's cue table, a memory's four spans, or one a slide for a slideshow. */
static int player_lines(const GalPlayer *p) {
    if (!player_is_story(p))
        return GAL_FILM_LINES;
    return p->memory >= 0 ? GAL_STORY_LINES : p->listCount;
}

static double player_line_start(const GalPlayer *p, int i) {
    if (!player_is_story(p))
        return GAL_FILM_CUES[i].start;
    return player_story_length(p) * (double)i / (double)player_lines(p);
}

static double player_line_end(const GalPlayer *p, int i) {
    if (!player_is_story(p))
        return GAL_FILM_CUES[i].end;
    return player_story_length(p) * (double)(i + 1) / (double)player_lines(p);
}

static int player_line_at(const GalPlayer *p, double t) {
    for (int i = 0; i < player_lines(p); i++)
        if (t >= player_line_start(p, i) && t < player_line_end(p, i))
            return i;
    return -1;
}

static const char *player_line_text(const AppState *st, int i) {
    const GalPlayer *p = &st->player;
    if (!player_is_story(p))
        return GAL_FILM_TEXT[p->track][i];
    if (p->memory >= 0)
        return GAL_MEMORIES[p->memory].lines[i];
    return st->store.photo[p->list[i]].title;
}

/* How far chapter @p i has played. A chapter runs to the next one's start, so the bar has no
   gaps even where the captions do. */
static float player_chapter_fill(const GalPlayer *p, int i, double t, double duration) {
    double from = i == 0 ? 0.0 : player_line_start(p, i);
    double to = i + 1 < player_lines(p) ? player_line_start(p, i + 1) : duration;
    if (t <= from || to <= from)
        return 0.0f;
    return t >= to ? 1.0f : (float)((t - from) / (to - from));
}

/* A playhead as "0:07". A clip that is still loading has no duration yet: not a number, which
   fails both tests and reads "-:--". */
static const char *player_clock(double seconds) {
    if (!(seconds >= 0.0 && seconds < 360000.0))
        return "-:--";
    return rcFormat(&g_text, "%d:%02d", (int)seconds / 60, (int)seconds % 60).chars;
}

static bool player_lang_is(const char *tag, const char *lang) {
    while (*lang && *tag == *lang) {
        tag++;
        lang++;
    }
    return !*lang && (*tag == '\0' || *tag == '-');
}

/* The first of the user's languages the film has captions in, English when none. */
static int player_track_for_user(void) {
    const char *tags[8];
    int n = rcPreferredLocales(tags, 8);
    for (int i = 0; i < n; i++)
        for (int k = 0; k < GAL_FILM_TRACKS; k++)
            if (player_lang_is(tags[i], GAL_FILM_LANG[k]))
                return k;
    return 0;
}

static void player_begin(AppState *st, int memory) {
    GalPlayer *p = &st->player;
    p->open = true;
    p->memory = memory;
    p->time = 0.0;
    p->rate = 1.0f;
    p->playing = true;
    p->captions = true;
    p->transcript = false;
    p->listCount = 0;
}

static void player_open_slideshow(AppState *st) {
    player_begin(st, -1);
    GalPlayer *p = &st->player;
    const int  from = viewer_index(st);
    /* From the open photo onward when the viewer brought it up, else from the top. */
    const int  first = st->viewer.open && from > 0 ? from : 0;
    for (int k = first; k < st->store.visibleCount; k++)
        p->list[p->listCount++] = st->store.visible[k];
}

static void player_open_memory(AppState *st, int memory) {
    GalPlayer *p = &st->player;
    player_begin(st, memory);
    const int shoot = GAL_MEMORIES[memory].shoot;
    if (shoot >= 0) {
        for (int i = 0; i < GAL_PHOTO_COUNT && p->listCount < 8; i++)
            if (st->store.photo[i].shoot == shoot && !st->store.photo[i].deleted)
                p->list[p->listCount++] = i;
        return;
    }
    /* The film. It opens here, on the click, so a run nobody clicks in opens no media: the
       default run stays deterministic. */
    if (!p->trackPicked) {
        p->track = player_track_for_user();
        p->trackPicked = true;
    }
    rcAudioSession(RC_AUDIO_SESSION_PLAYBACK);
    p->clip = rcMediaOpen(STORY_DIR "harbor.mp4", .alt = STORY_DIR "harbor.webm");
    rcMediaPlay(p->clip);
    p->transcript = true;
    p->listCount = 1;
    p->list[0] = st->store.memoryCover[memory];
}

static void player_close(AppState *st) {
    GalPlayer *p = &st->player;
    if (p->clip) {
        rcMediaClose(p->clip);
        p->clip = 0;
    }
    if (p->awake) {
        rcSetKeepScreenOn(false);
        p->awake = false;
    }
    p->open = false;
}

static void player_toggle(GalPlayer *p, const RC_MediaInfo *m) {
    if (!player_is_story(p)) {
        if (m->state == RC_MEDIA_PLAYING)
            rcMediaPause(p->clip);
        else
            rcMediaPlay(p->clip);    /* from ENDED it starts again from 0 */
        return;
    }
    if (m->state == RC_MEDIA_ENDED)
        p->time = 0.0;
    p->playing = m->state != RC_MEDIA_PLAYING;
}

static void player_seek(GalPlayer *p, double t) {
    if (!player_is_story(p)) {
        rcMediaSeek(p->clip, t);
        return;
    }
    const double length = player_story_length(p);
    p->time = t < 0.0 ? 0.0 : t > length ? length : t;
}

static void player_set_rate(GalPlayer *p, float rate) {
    p->rate = rate;
    if (!player_is_story(p))
        rcMediaSetRate(p->clip, rate);
}

static void player_update(AppState *st, float dt, int *want, int *wants) {
    GalPlayer *p = &st->player;
    if (!p->open || !player_is_story(p))
        return;
    if (p->playing) {
        p->time += (double)(dt * p->rate);
        if (p->time >= player_story_length(p)) {
            p->time = player_story_length(p);
            p->playing = false;
        }
    }
    const int k = (int)(p->time / PLAYER_SLIDE);
    *wants = 0;
    if (k < p->listCount)
        want[(*wants)++] = p->list[k];
    if (k + 1 < p->listCount)
        want[(*wants)++] = p->list[k + 1];
}

static bool player_animating(const AppState *st) {
    return st->player.open && player_is_story(&st->player) && st->player.playing;
}

static bool player_covers(const AppState *st) { return st->player.open; }

/* One photograph under a slow move. The picture is larger than the box and the box clips, so
   a pan and a push are only an offset and a size. @p p runs 0 to 1 over the time it shows. */
static void player_slide(int photo, float aspect, float W, float H, float p, float alpha, int k) {
    const float push = 1.04f + 0.09f * ((k & 1) ? 1.0f - p : p);
    const float cover = W / H > aspect ? W / aspect : H;
    const float h = cover * push, w = h * aspect;
    const float pan = (push - 1.0f) * ((k & 2) ? 0.35f : -0.35f) * (p - 0.5f) * 2.0f;
    const RC_Color tint = rcWithAlpha(RC_WHITE, alpha);
    rcBox(.wType = RC_PX(w), .hType = RC_PX(h),
          .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                        .element = RC_ANCHOR_TOP_LEFT,
                        .offset = { (W - w) * 0.5f + pan * W, (H - h) * 0.5f } },
          .image = gallery_picture(photo), .imageTint = alpha < 1.0f ? tint : RC_UNSET_COLOR) {}
}

/* What fills the screen behind the controls. A film is shown whole at its own shape, in the
   middle, with its own frame scaled up and darkened behind it, the ambient look a video
   site uses to spend a wide window on a portrait clip. */
static void player_picture(AppState *st, const RC_MediaInfo *m, float W, float H) {
    GalPlayer *p = &st->player;
    if (!player_is_story(p)) {
        const RC_Image *frame = rcMediaImage(p->clip);
        const RC_Image *cover = frame ? frame : &g_thumb[p->list[0]];
        const float aspect = m->width > 0
            && m->height > 0 ? (float)m->width / (float)m->height : 0.5625f;
        rcBox(.wType = RC_PX(W), .hType = RC_PX(H),
              .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                            .element = RC_ANCHOR_TOP_LEFT },
              .image = cover, .imageFit = RC_IMAGE_COVER,
              .overlay = rcWithAlpha(RC_BLACK, 0.667f)) {}
        if (frame) {
            float h = H, w = h * aspect;
            if (w > W) {
                w = W;
                h = w / aspect;
            }
            rcBox(.wType = RC_PX(w), .hType = RC_PX(h),
                  .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                                .element = RC_ANCHOR_TOP_LEFT,
                                .offset = { (W - w) * 0.5f, (H - h) * 0.5f } },
                  .image = frame, .imageFit = RC_IMAGE_CONTAIN) {}
        }
        return;
    }
    const float now = (float)(p->time / PLAYER_SLIDE);
    int k = (int)now;
    k = k >= p->listCount ? p->listCount - 1 : k;
    if (k < 0)
        return;
    const float tau = now - (float)k;
    const float span = 1.0f + PLAYER_FADE;
    player_slide(p->list[k], st->store.photo[p->list[k]].aspect, W, H,
                 pg_clamp((tau + PLAYER_FADE) / span),
                 1.0f, k);
    if (tau > 1.0f - PLAYER_FADE && k + 1 < p->listCount)
        player_slide(p->list[k + 1], st->store.photo[p->list[k + 1]].aspect, W, H,
                     pg_clamp((tau - 1.0f + PLAYER_FADE) / span),
                         (tau - 1.0f + PLAYER_FADE) / PLAYER_FADE, k + 1);
}

/* The chapter bar: one segment a line, filled as the playhead passes. A long slideshow gets
   one, since ninety-six segments would be a texture rather than a bar. */
static void player_chapters(const GalPlayer *p, const RC_MediaInfo *m) {
    const int lines = player_lines(p);
    rcRow(.gap = 4, .w = "grow") {
        if (lines > 16) {
            rcBox(.bg = rcWithAlpha(RC_WHITE, 0.314f), .borderRadius = "all-full", .w = "grow",
                  .h = "3px") {
                rcBox(.bg = RC_WHITE, .borderRadius = "all-full", .h = "grow",
                      .wType = RC_PCT((float)(m->time / (m->duration > 0.0 ? m->duration
                                                         : 1.0)) * 100.0f)) {}
            }
        } else {
            for (int i = 0; i < lines; i++) {
                const float fill = player_chapter_fill(p, i, m->time, m->duration);
                rcBox(.bg = rcWithAlpha(RC_WHITE, 0.314f), .borderRadius = "all-full", .w = "grow",
                      .h = "3px") {
                    if (fill > 0.0f)
                        rcBox(.bg = RC_WHITE, .borderRadius = "all-full", .h = "grow",
                              .wType = RC_PCT(fill * 100.0f)) {}
                }
            }
        }
    }
}

/* What the picture says when there is nothing to show yet, or ever. */
static void player_status(const RC_MediaInfo *m, const GalFrame *f) {
    if (m->state == RC_MEDIA_FAILED) {
        rcColumn(.gap = 10, .p = 20, .align = "cc", .borderRadius = "all-xl", .w = "grow",
                 .wMax = RC_VAL(340.0f), .className = "bg-surface/92") {
            rcIconTriangleAlert(28.0f, f->pal.caution);
            ui_strong("This film cannot play here", 15, RC_WHITE);
            rcTextC(m->message ? m->message : "No player on this system.", .color = f->pal.quiet,
                    .size = 13,
                    .textAlign = "c");
            ui_label("Its words are in the transcript.", 13, f->pal.quiet);
        }
    } else if (m->state == RC_MEDIA_LOADING) {
        ui_label("Loading...", 13, RC_WHITE);
    } else if (m->state != RC_MEDIA_PLAYING) {
        rcBox(.bg = rcWithAlpha(RC_BLACK, 0.59f), .align = "cc", .borderRadius = "all-full",
              .w = "72px", .h = "72px") {
            rcIconPlay(30.0f, RC_WHITE);
        }
    }
}

/* The transcript: every line with its time, the one being spoken lit. A click seeks to it. */
static void player_transcript(AppState *st, const RC_MediaInfo *m, const GalFrame *f) {
    GalPlayer *p = &st->player;
    const int  line = player_line_at(p, m->time);
    const bool ready = m->duration > 0.0 && m->state != RC_MEDIA_FAILED;
    rcColumn(.id = "Transcript", .bg = f->pal.panel, .gap = 4, .p = 16, .scroll = "v",
             .w = "grow", .h = "grow") {
        rcRow(.gap = 12, .align = "cl", .w = "grow") {
            ui_strong("Transcript", 15, RC_WHITE);
            rcSpacer() {}
            if (!player_is_story(p))
                rcBox(.wType = RC_PX(120)) {
                    rcCombo("track", &p->track, GAL_FILM_TRACK_NAME, GAL_FILM_TRACKS);
                }
        }
        rcBox(.h = "6px") {}
        for (int i = 0; i < player_lines(p); i++) {
            const char *id = st->lineId[i];
            const bool  on = i == line;
            rcRow(.id = id, .focusable = ready, .gap = 12, .p = 10, .align = "tl",
                  .borderRadius = "all-lg", .w = "grow",
                  .className = on ? "bg-surface-alt" : ready ? "hover:bg-[#222224]" : NULL) {
                rcBox(.w = "36px") {
                    ui_label(player_clock(player_line_start(p, i)), 13,
                             on ? RC_WHITE : f->pal.faint);
                }
                rcTextC(player_line_text(st, i), .color = on ? RC_WHITE : f->pal.quiet,
                        .size = 14,
                        .select = RC_SELECT_NONE);
            }
            if (ready && rcClicked(id)) {
                player_seek(p, player_line_start(p, i));
                if (m->state != RC_MEDIA_PLAYING)
                    player_toggle(p, m);
            }
        }
    }
    rcScrollbar("Transcript");
}

/* Keys, as a video page takes them. A shortcut with Ctrl or Cmd held is the system's. */
static void player_keys(AppState *st, const RC_MediaInfo *m, bool ready) {
    GalPlayer *p = &st->player;
    if (rcKeyPressed(RC_KEY_ESCAPE)) {
        player_close(st);
        return;
    }
    if (rcModDown(RC_MOD_CTRL) || rcModDown(RC_MOD_SUPER))
        return;
    if (rcKeyPressed(RC_KEY_C)) p->captions = !p->captions;
    if (!ready)
        return;
    if (rcKeyPressed(RC_KEY_SPACE)) player_toggle(p, m);
    if (rcKeyPressed(RC_KEY_LEFT)) player_seek(p, m->time - 3.0);
    if (rcKeyPressed(RC_KEY_RIGHT)) player_seek(p, m->time + 3.0);
    if (rcKeyPressed(RC_KEY_M) && !player_is_story(p)) rcMediaSetMuted(p->clip, !m->muted);
}

/* The bar across the top: back, the story's name, sound, captions, the transcript. It is also
   the window's titlebar while the player is up, and fixed chrome as the wall's is: see
   wall_band. */
static void player_top(AppState *st, const RC_MediaInfo *m, const GalFrame *f, bool ready) {
    GalPlayer *p = &st->player;
    const char    *hot = "hover:bg-white/16";
    const float    d = f->coarse ? 44.0f : 36.0f;
    const char    *title = p->memory >= 0 ? GAL_MEMORIES[p->memory].title : "Slideshow";
    const char    *sub = p->memory >= 0 ? GAL_MEMORIES[p->memory].subtitle
                                        : gallery_scope_title(st);
    rcUnzoomed()
    rcRow(.id = RC_ID_WINDOW_DRAG, .gap = 4, .pl = 8, .align = "cl", .w = "grow",
          .hType = RC_PX(GALLERY_BAND_H)) {
        rcBox(.noDrag = true) {
            if (ui_icon("player_close", rcIconChevronLeft, RC_WHITE, hot, d, 22.0f, true, "Back"))
                player_close(st);
        }
        rcColumn(.gap = 1, .pl = 4, .overflow = "hidden", .w = "grow") {
            ui_strong(title, 15, RC_WHITE);
            ui_label(sub, 12, rcWithAlpha(RC_WHITE, 0.804f));
        }
        rcRow(.gap = 2, .pr = 8, .align = "cc", .noDrag = true) {
            if (!player_is_story(p)
                && ui_icon("player_mute", m->muted ? rcIconVolumeX : rcIconVolume2, RC_WHITE,
                           hot, d, 18.0f, ready, m->muted ? "Unmute" : "Mute"))
                rcMediaSetMuted(p->clip, !m->muted);
            if (ui_icon("player_cc", rcIconCaptions,
                        p->captions ? RC_WHITE : rcWithAlpha(RC_WHITE, 0.47f), hot, d, 18.0f,
                        m->state != RC_MEDIA_FAILED, "Captions"))
                p->captions = !p->captions;
            if (ui_icon("player_lines", rcIconList,
                        p->transcript ? RC_WHITE : rcWithAlpha(RC_WHITE, 0.667f), hot, d, 18.0f,
                        true, "Transcript"))
                p->transcript = !p->transcript;
        }
        rcWindowControls();
    }
}

/* The controls and the line being spoken, over the bottom of the picture. */
static void player_bottom(AppState *st, const RC_MediaInfo *m, const GalFrame *f, bool ready) {
    GalPlayer *p = &st->player;
    const char    *hot = "hover:bg-white/16";
    const float    d = f->coarse ? 44.0f : 36.0f;
    const int      line = player_line_at(p, m->time);
    const bool     slow = ready && m->rate < 1.0f;

    if (p->captions && line >= 0 && m->state != RC_MEDIA_FAILED) {
        rcRow(.align = "cc", .w = "grow") {
            rcBox(.bg = rcWithAlpha(RC_BLACK, 0.647f), .px = 14, .py = 7, .borderRadius = "all-lg",
                  .wMax = RC_VAL(620.0f)) {
                rcTextC(player_line_text(st, line), .color = RC_WHITE, .size = 16, .textAlign = "c",
                        .select = RC_SELECT_NONE);
            }
        }
    }
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        const bool playing = m->state == RC_MEDIA_PLAYING;
        if (ui_icon("player_play", playing ? rcIconPause : rcIconPlay, RC_WHITE, hot, d, 18.0f,
                    ready, playing ? "Pause" : "Play"))
            player_toggle(p, m);
        ui_label(player_clock(m->time), 12, RC_WHITE);
        /* A story that cannot play shows its bar disabled. The bar keeps a range of 1 then, as a
           slider needs its max above its min. */
        p->scrub = ready ? (float)m->time : 0.0f;
        rcBox(.w = "grow") {
            if (rcSlider("player_seek", &p->scrub, 0.0f, ready ? (float)m->duration : 1.0f,
                         .disabled = !ready))
                player_seek(p, (double)p->scrub);
        }
        ui_label(player_clock(m->duration), 12, RC_WHITE);
        /* As tall as the icon buttons beside it, which are larger here than in the toolbar. */
        if (rcButton("player_rate", slow ? "0.75x" : "1x", .disabled = !ready,
                     .className = f->coarse ? "h-11 on-dark" : "h-9 on-dark",
                     .variant = RC_BTN_GHOST))
            player_set_rate(p, slow ? 1.0f : 0.75f);
    }
}

static void player_layout(AppState *st, const GalFrame *wallFrame) {
    GalPlayer *p = &st->player;
    if (!p->open)
        return;
    GalFrame frame = *wallFrame;
    frame.style = viewer_style(false);
    const GalFrame *f = &frame;
    rcSetStyle(frame.style);

    RC_MediaInfo m = player_read(p);
    const bool ready = m.duration > 0.0 && m.state != RC_MEDIA_FAILED;
    player_keys(st, &m, ready);
    if (!p->open)                                 /* Esc just closed it */
        return;
    /* Hold the screen awake while it plays, and only then. */
    if ((m.state == RC_MEDIA_PLAYING) != p->awake) {
        p->awake = m.state == RC_MEDIA_PLAYING;
        rcSetKeepScreenOn(p->awake);
    }

    const float roomW = (float)f->viewW - f->safe.left - f->safe.right;
    const float roomH = (float)f->viewH - f->safe.top - f->safe.bottom;
    const bool  wide = roomW >= 760.0f;
    const bool  panel = p->transcript && wide;
    const bool  sheet = p->transcript && !wide;
    const float W = panel ? roomW - PLAYER_PANEL : roomW;
    const float H = sheet ? roomH * 0.55f : roomH;

    rcRow(.id = "Player", .bg = RC_BLACK, .pt = f->safe.top,
          .pb = f->safe.bottom,
          .pl = f->safe.left, .pr = f->safe.right, .wType = RC_PX(f->viewW),
          .hType = RC_PX(f->viewH),
          .floating = { .to = RC_ATTACH_ROOT, .parent = RC_ANCHOR_TOP_LEFT,
                        .element = RC_ANCHOR_TOP_LEFT }) {
        rcColumn(.w = "grow", .h = "grow") {
            rcColumn(.id = "PlayerStage", .overflow = "hidden", .wType = RC_PX(W),
                     .hType = RC_PX(H)) {
                player_picture(st, &m, W, H);
                rcColumn(.wType = RC_PX(W), .hType = RC_PX(H),
                         .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                                       .element = RC_ANCHOR_TOP_LEFT }) {
                    rcColumn(.id = "player_scrim_top", .w = "grow",
                             .gradient = { .from = rcWithAlpha(RC_BLACK, 0.667f),
                                           .to = RC_TRANSPARENT, .dir = "v" }) {
                        rcBox(.w = "grow", .hType = RC_PX(f->bandH)) {}
                        rcBox(.pb = 16, .px = 16, .w = "grow") {
                            player_chapters(p, &m);
                        }
                    }
                    rcColumn(.id = "player_tap", .p = 16, .align = "cc", .w = "grow", .h = "grow") {
                        player_status(&m, f);
                    }
                    if (ready && rcClicked("player_tap"))
                        player_toggle(p, &m);
                    rcColumn(.id = "player_scrim_bottom", .gap = 12, .p = 16, .w = "grow",
                             .gradient = { .from = RC_TRANSPARENT,
                                           .to = rcWithAlpha(RC_BLACK, 0.725f), .dir = "v" }) {
                        player_bottom(st, &m, f, ready);
                    }
                }
            }
            if (sheet) {
                rcBox(.w = "grow", .h = "grow") { player_transcript(st, &m, f); }
            }
        }
        if (panel) {
            rcBox(.bg = f->pal.panel, .pt = (float)f->bandH, .h = "grow",
                  .wType = RC_PX(PLAYER_PANEL)) {
                player_transcript(st, &m, f);
            }
        }
    }
    /* The bar runs the window's whole width, above the picture and the transcript alike, so the
       window's own controls stay in their corner whether the panel is open or not. The picture
       and its controls float inside the player, so they count their z from it: z 3 covers them. */
    rcBox(.wType = RC_PX(roomW),
          .floating = { .to = RC_ATTACH_ROOT, .parent = RC_ANCHOR_TOP_LEFT,
                        .element = RC_ANCHOR_TOP_LEFT, .offset = { f->safe.left, f->safe.top },
                        .zIndex = 3 }) {
        player_top(st, &m, f, ready);
    }
}

#endif /* UI_PLAYER_H */
