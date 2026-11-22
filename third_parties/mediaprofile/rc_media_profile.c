#include "rc_media_profile.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DRMP3_API              static
#define DRFLAC_API             static
#define DR_MP3_NO_STDIO
#define DR_MP3_ONLY_MP3
#define DR_FLAC_NO_STDIO
#define DR_MP3_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#include <dr_mp3.h>
#include <dr_flac.h>

#define stb_vorbis_get_info                      rci_media_profile_stbv_get_info
#define stb_vorbis_get_comment                   rci_media_profile_stbv_get_comment
#define stb_vorbis_get_error                     rci_media_profile_stbv_get_error
#define stb_vorbis_close                         rci_media_profile_stbv_close
#define stb_vorbis_get_sample_offset             rci_media_profile_stbv_get_sample_offset
#define stb_vorbis_get_file_offset               rci_media_profile_stbv_get_file_offset
#define stb_vorbis_open_memory                   rci_media_profile_stbv_open_memory
#define stb_vorbis_open_filename                 rci_media_profile_stbv_open_filename
#define stb_vorbis_open_file                     rci_media_profile_stbv_open_file
#define stb_vorbis_open_file_section             rci_media_profile_stbv_open_file_section
#define stb_vorbis_seek_frame                    rci_media_profile_stbv_seek_frame
#define stb_vorbis_seek                          rci_media_profile_stbv_seek
#define stb_vorbis_seek_start                    rci_media_profile_stbv_seek_start
#define stb_vorbis_stream_length_in_samples      rci_media_profile_stbv_stream_length_in_samples
#define stb_vorbis_stream_length_in_seconds      rci_media_profile_stbv_stream_length_in_seconds
#define stb_vorbis_get_frame_float               rci_media_profile_stbv_get_frame_float
#define stb_vorbis_get_samples_float_interleaved rci_media_profile_stbv_get_samples_float_interleaved
#define stb_vorbis_get_samples_float             rci_media_profile_stbv_get_samples_float
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_INTEGER_CONVERSION
#define STB_VORBIS_MAX_CHANNELS 8
#include <stb_vorbis.c>

#include "rc_mp_nestegg_names.h"
#include "rc_mp_opus_names.h"
#include <nestegg/nestegg.h>
#include <opus_multistream.h>
#include "rc_mp_vpx_names.h"
#include <vpx/vp8dx.h>
#include <vpx/vpx_decoder.h>
#include <vpx_version.h>

#define RCI_MEDIA_PROFILE_CHANNELS_MAX 8
#define RCI_MEDIA_PROFILE_RATE_MAX     384000
#define RCI_MEDIA_PROFILE_MP3_POINTS   256
#define RCI_MEDIA_PROFILE_ARENA_MIN    (256 * 1024)
#define RCI_MEDIA_PROFILE_ARENA_MAX    (4 * 1024 * 1024)
#define RCI_MEDIA_PROFILE_COMMENTS_MAX (16 * 1024 * 1024)
#define RCI_MEDIA_PROFILE_FRAME_MAX    (16 * 1024 * 1024)
#define RCI_MEDIA_PROFILE_NO_TRACK     UINT_MAX
#define RCI_MEDIA_PROFILE_THREADS_MAX  8

#define RCI_MEDIA_PROFILE_STR(x)  RCI_MEDIA_PROFILE_STR_(x)
#define RCI_MEDIA_PROFILE_STR_(x) #x

static void rci_media_profile_why(char *why, const char *text) {
    size_t n = strlen(text);

    n = n < RCI_MEDIA_PROFILE_WHY - 1 ? n : RCI_MEDIA_PROFILE_WHY - 1;
    memcpy(why, text, n);
    why[n] = '\0';
}

static void rci_media_profile_refuse(int *refusal, int kind, char *why, const char *text) {
    rci_media_profile_why(why, text);
    *refusal = kind;
}

static int rci_media_profile_shape(int channels, unsigned long rate, int *refusal, char *why) {
    if (channels < 1 || channels > RCI_MEDIA_PROFILE_CHANNELS_MAX) {
        rci_media_profile_refuse(refusal, channels < 1 ? RCI_MEDIA_PROFILE_DAMAGED : RCI_MEDIA_PROFILE_UNPLAYABLE, why,
                                 channels < 1 ? "a stream with no channels" : "more than 8 channels: not decoded");
        return 0;
    }
    if (rate < 1 || rate > RCI_MEDIA_PROFILE_RATE_MAX) {
        rci_media_profile_refuse(refusal, rate < 1 ? RCI_MEDIA_PROFILE_DAMAGED : RCI_MEDIA_PROFILE_UNPLAYABLE, why,
                                 "a sample rate outside 1 to 384,000 frames a second");
        return 0;
    }
    return 1;
}

static size_t rci_media_profile_read(void *user, void *out, size_t bytes) {
    return fread(out, 1, bytes, (FILE *)user);
}

static int rci_media_profile_seek(void *user, int offset, int origin) {
    return fseek((FILE *)user, (long)offset, origin == 0 ? SEEK_SET : origin == 1 ? SEEK_CUR : SEEK_END) == 0;
}

static int rci_media_profile_tell(void *user, long long *at) {
    long p = ftell((FILE *)user);

    if (p < 0)
        return 0;
    *at = (long long)p;
    return 1;
}

typedef struct RCI_MediaProfileMp3 {
    drmp3            mp3;
    drmp3_seek_point points[RCI_MEDIA_PROFILE_MP3_POINTS];
} RCI_MediaProfileMp3;

static drmp3_bool32 rci_media_profile_mp3_seek(void *user, int offset, drmp3_seek_origin origin) {
    return (drmp3_bool32)rci_media_profile_seek(user, offset, origin == DRMP3_SEEK_SET ? 0
                                                              : origin == DRMP3_SEEK_CUR ? 1 : 2);
}

static drmp3_bool32 rci_media_profile_mp3_tell(void *user, drmp3_int64 *at) {
    long long p = 0;

    if (!rci_media_profile_tell(user, &p))
        return DRMP3_FALSE;
    *at = (drmp3_int64)p;
    return DRMP3_TRUE;
}

static void *rci_media_profile_mp3_open(void *file, int *channels, int *rate, unsigned long long *frames,
                                        int *refusal, char *why) {
    RCI_MediaProfileMp3 *s = (RCI_MediaProfileMp3 *)calloc(1, sizeof *s);
    drmp3_uint32         points = RCI_MEDIA_PROFILE_MP3_POINTS;
    drmp3_uint64         length;

    if (!s) {
        rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_NO_MEMORY, why, "out of memory for an MP3 decoder");
        return NULL;
    }
    if (!drmp3_init(&s->mp3, rci_media_profile_read, rci_media_profile_mp3_seek, rci_media_profile_mp3_tell,
                    NULL, file, NULL)) {
        free(s);
        rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_DAMAGED, why, "not an MP3 stream dr_mp3 decodes");
        return NULL;
    }
    if (!rci_media_profile_shape((int)s->mp3.channels, s->mp3.sampleRate, refusal, why)) {
        drmp3_uninit(&s->mp3);
        free(s);
        return NULL;
    }

    length = drmp3_get_pcm_frame_count(&s->mp3);
    if (length && drmp3_calculate_seek_points(&s->mp3, &points, s->points) && points)
        drmp3_bind_seek_table(&s->mp3, points, s->points);
    if (!length || !drmp3_seek_to_pcm_frame(&s->mp3, 0)) {
        drmp3_uninit(&s->mp3);
        free(s);
        rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_DAMAGED, why, "an MP3 stream with no frame dr_mp3 decodes");
        return NULL;
    }
    *channels = (int)s->mp3.channels;
    *rate     = (int)s->mp3.sampleRate;
    *frames   = (unsigned long long)length;
    return s;
}

static size_t rci_media_profile_mp3_read(void *stream, float *out, size_t frames) {
    return (size_t)drmp3_read_pcm_frames_f32(&((RCI_MediaProfileMp3 *)stream)->mp3, (drmp3_uint64)frames, out);
}

static int rci_media_profile_mp3_seek_to(void *stream, unsigned long long frame) {
    return drmp3_seek_to_pcm_frame(&((RCI_MediaProfileMp3 *)stream)->mp3, (drmp3_uint64)frame) ? 1 : 0;
}

static void rci_media_profile_mp3_close(void *stream) {
    drmp3_uninit(&((RCI_MediaProfileMp3 *)stream)->mp3);
    free(stream);
}

static drflac_bool32 rci_media_profile_flac_seek(void *user, int offset, drflac_seek_origin origin) {
    return (drflac_bool32)rci_media_profile_seek(user, offset, origin == DRFLAC_SEEK_SET ? 0
                                                               : origin == DRFLAC_SEEK_CUR ? 1 : 2);
}

static drflac_bool32 rci_media_profile_flac_tell(void *user, drflac_int64 *at) {
    long long p = 0;

    if (!rci_media_profile_tell(user, &p))
        return DRFLAC_FALSE;
    *at = (drflac_int64)p;
    return DRFLAC_TRUE;
}

static unsigned long long rci_media_profile_ogg_length(FILE *file) {
    const long          at   = ftell(file);
    const size_t        most = 65536;
    unsigned char       head[18], *tail = (unsigned char *)malloc(most);
    unsigned long long  length = 0;
    long                end;
    size_t              n = 0;

    if (at < 0 || !tail) {
        free(tail);
        return 0;
    }
    if (fseek(file, 0, SEEK_SET) == 0 && fread(head, 1, sizeof head, file) == sizeof head &&
        memcmp(head, "OggS", 4) == 0 && fseek(file, 0, SEEK_END) == 0 && (end = ftell(file)) >= 0 &&
        fseek(file, end > (long)most ? end - (long)most : 0, SEEK_SET) == 0)
        n = fread(tail, 1, most, file);
    for (size_t i = n >= 27 ? n - 27 + 1 : 0; i-- > 0;) {
        unsigned long long granule = 0;

        if (memcmp(tail + i, "OggS", 4) != 0 || tail[i + 4] != 0 || memcmp(tail + i + 14, head + 14, 4) != 0)
            continue;
        for (int k = 7; k >= 0; k--)
            granule = granule << 8 | tail[i + 6 + (size_t)k];
        if (granule != ~0ULL) {
            length = granule;
            break;
        }
    }
    free(tail);
    if (fseek(file, at, SEEK_SET) != 0)
        return 0;
    return length;
}

static void *rci_media_profile_flac_open(void *file, int *channels, int *rate, unsigned long long *frames,
                                         int *refusal, char *why) {
    drflac *f = drflac_open(rci_media_profile_read, rci_media_profile_flac_seek, rci_media_profile_flac_tell,
                            file, NULL);

    if (!f) {
        rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_DAMAGED, why, "not a FLAC stream dr_flac decodes");
        return NULL;
    }
    if (!rci_media_profile_shape((int)f->channels, f->sampleRate, refusal, why)) {
        drflac_close(f);
        return NULL;
    }
    *channels = (int)f->channels;
    *rate     = (int)f->sampleRate;
    *frames   = (unsigned long long)f->totalPCMFrameCount;

    if (!*frames && f->container == drflac_container_ogg)
        *frames = rci_media_profile_ogg_length((FILE *)file);
    return f;
}

static size_t rci_media_profile_flac_read(void *stream, float *out, size_t frames) {
    return (size_t)drflac_read_pcm_frames_f32((drflac *)stream, (drflac_uint64)frames, out);
}

static int rci_media_profile_flac_seek_to(void *stream, unsigned long long frame) {
    return drflac_seek_to_pcm_frame((drflac *)stream, (drflac_uint64)frame) ? 1 : 0;
}

static void rci_media_profile_flac_close(void *stream) {
    drflac_close((drflac *)stream);
}

typedef struct RCI_MediaProfileVorbis {
    stb_vorbis *v;
    int         channels;
    char       *arena;
} RCI_MediaProfileVorbis;

typedef struct RCI_MediaProfileOgg {
    FILE         *file;
    unsigned char seg[255];
    int           segs, next;
    size_t        left;
    int           last;
    int           bad;
} RCI_MediaProfileOgg;

static int rci_media_profile_ogg_page(RCI_MediaProfileOgg *o, int *flags) {
    unsigned char h[27];

    if (fread(h, 1, sizeof h, o->file) != sizeof h || memcmp(h, "OggS", 4) != 0 || h[4] != 0 || h[26] == 0 ||
        fread(o->seg, 1, (size_t)h[26], o->file) != (size_t)h[26])
        return 0;
    *flags  = h[5];
    o->segs = h[26];
    o->next = 0;
    o->left = 0;
    return 1;
}

static size_t rci_media_profile_ogg_read(RCI_MediaProfileOgg *o, unsigned char *out, size_t n) {
    unsigned char skip[255];
    size_t        done = 0;

    while (done < n && !o->bad) {
        size_t step;
        int    flags = 0;

        if (!o->left) {
            if (o->last)
                break;
            if (o->next == o->segs && (!rci_media_profile_ogg_page(o, &flags) || !(flags & 1))) {
                o->bad = 1;
                break;
            }
            o->left = o->seg[o->next++];
            o->last = o->left < 255;
            continue;
        }
        step = n - done < o->left ? n - done : o->left;
        if (fread(out ? out + done : skip, 1, step, o->file) != step) {
            o->bad = 1;
            break;
        }
        done    += step;
        o->left -= step;
    }
    return done;
}

static int rci_media_profile_ogg_u32(RCI_MediaProfileOgg *o, unsigned long *v) {
    unsigned char b[4];

    if (rci_media_profile_ogg_read(o, b, 4) != 4)
        return 0;
    *v = (unsigned long)b[0] | (unsigned long)b[1] << 8 | (unsigned long)b[2] << 16 | (unsigned long)b[3] << 24;
    return 1;
}

static size_t rci_media_profile_stbv_size(size_t bytes) {
    return (bytes + 7) & ~(size_t)7;
}

static int rci_media_profile_vorbis_comments(FILE *file, long start, size_t *need, int *refusal, char *why) {
    static const unsigned char kind[7] = { 3, 'v', 'o', 'r', 'b', 'i', 's' };
    RCI_MediaProfileOgg        o;
    unsigned char              head[7];
    unsigned long              vendor, count, i;
    int                        flags = 0, ok = 0;
    size_t                     total = 0;

    memset(&o, 0, sizeof o);
    o.file = file;
    if (rci_media_profile_ogg_page(&o, &flags) && o.segs == 1 && o.seg[0] == 30 && fseek(file, 30, SEEK_CUR) == 0 &&
        rci_media_profile_ogg_page(&o, &flags) && rci_media_profile_ogg_read(&o, head, 7) == 7 &&
        memcmp(head, kind, 7) == 0 && rci_media_profile_ogg_u32(&o, &vendor) &&
        vendor < RCI_MEDIA_PROFILE_COMMENTS_MAX && rci_media_profile_ogg_read(&o, NULL, vendor) == vendor &&
        rci_media_profile_ogg_u32(&o, &count) && count <= RCI_MEDIA_PROFILE_COMMENTS_MAX / sizeof(char *)) {
        total = rci_media_profile_stbv_size(vendor + 1) + (count ? rci_media_profile_stbv_size(count * sizeof(char *))
                                                                 : 0);
        for (i = 0; i < count && total <= RCI_MEDIA_PROFILE_COMMENTS_MAX; i++) {
            unsigned long len;

            if (!rci_media_profile_ogg_u32(&o, &len) || len >= RCI_MEDIA_PROFILE_COMMENTS_MAX ||
                rci_media_profile_ogg_read(&o, NULL, len) != len)
                break;
            total += rci_media_profile_stbv_size(len + 1);
        }
        ok = i == count && total <= RCI_MEDIA_PROFILE_COMMENTS_MAX;
    }
    if (fseek(file, start, SEEK_SET) != 0)
        ok = 0;
    if (!ok) {
        if (total > RCI_MEDIA_PROFILE_COMMENTS_MAX)
            rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_UNPLAYABLE, why,
                                     "an Ogg Vorbis stream whose comments take more than 16 MB");
        else
            rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_DAMAGED, why,
                                     "an Ogg Vorbis stream whose header pages are damaged");
        return 0;
    }
    *need = total;
    return 1;
}

static void *rci_media_profile_vorbis_open(void *file, int *channels, int *rate, unsigned long long *frames,
                                           int *refusal, char *why) {
    RCI_MediaProfileVorbis *s       = (RCI_MediaProfileVorbis *)calloc(1, sizeof *s);
    const long              start   = ftell((FILE *)file);
    size_t                  budget  = RCI_MEDIA_PROFILE_ARENA_MIN, comments = 0;
    int                     error   = 0;
    stb_vorbis_info         info;
    unsigned int            length;

    if (!s || start < 0) {
        free(s);
        rci_media_profile_refuse(refusal, s ? RCI_MEDIA_PROFILE_DAMAGED : RCI_MEDIA_PROFILE_NO_MEMORY, why,
                                 s ? "an Ogg file whose position cannot be read"
                                   : "out of memory for an Ogg Vorbis decoder");
        return NULL;
    }
    if (!rci_media_profile_vorbis_comments((FILE *)file, start, &comments, refusal, why)) {
        free(s);
        return NULL;
    }
    for (;;) {
        stb_vorbis_alloc arena;

        if (!(s->arena = (char *)malloc(comments + budget))) {
            free(s);
            rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_NO_MEMORY, why,
                                     "out of memory for an Ogg Vorbis decoder's arena");
            return NULL;
        }
        arena.alloc_buffer                 = s->arena;
        arena.alloc_buffer_length_in_bytes = (int)(comments + budget);
        s->v = stb_vorbis_open_file((FILE *)file, 0, &error, &arena);
        if (s->v || error != VORBIS_outofmem || budget >= RCI_MEDIA_PROFILE_ARENA_MAX ||
            fseek((FILE *)file, start, SEEK_SET) != 0)
            break;
        free(s->arena);
        s->arena = NULL;
        budget *= 4;
    }
    if (!s->v) {
        free(s->arena);
        free(s);
        if (error == VORBIS_outofmem || error == VORBIS_too_many_channels)
            rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_UNPLAYABLE, why,
                                     error == VORBIS_outofmem ? "an Ogg Vorbis stream whose setup needs more than 4 MB"
                                                              : "more than 8 channels: not decoded");
        else
            rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_DAMAGED, why,
                                     "not an Ogg Vorbis stream stb_vorbis decodes");
        return NULL;
    }
    info = stb_vorbis_get_info(s->v);
    if (!rci_media_profile_shape(info.channels, info.sample_rate, refusal, why)) {
        stb_vorbis_close(s->v);
        free(s->arena);
        free(s);
        return NULL;
    }
    length      = stb_vorbis_stream_length_in_samples(s->v);
    s->channels = info.channels;
    *channels   = info.channels;
    *rate       = (int)info.sample_rate;
    *frames     = length == 0xFFFFFFFFu ? 0u : (unsigned long long)length;

    if (!stb_vorbis_seek_start(s->v)) {
        stb_vorbis_close(s->v);
        free(s->arena);
        free(s);
        rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_DAMAGED, why,
                                 "an Ogg Vorbis stream stb_vorbis cannot rewind");
        return NULL;
    }
    return s;
}

static size_t rci_media_profile_vorbis_read(void *stream, float *out, size_t frames) {
    RCI_MediaProfileVorbis *s    = (RCI_MediaProfileVorbis *)stream;
    const size_t            most = (size_t)(INT_MAX / s->channels);
    int                     n;

    frames = frames < most ? frames : most;
    n      = stb_vorbis_get_samples_float_interleaved(s->v, s->channels, out, (int)frames * s->channels);
    return n > 0 ? (size_t)n : 0;
}

static int rci_media_profile_vorbis_seek_to(void *stream, unsigned long long frame) {
    RCI_MediaProfileVorbis *s = (RCI_MediaProfileVorbis *)stream;

    if (frame >= UINT_MAX)
        return 0;
    return stb_vorbis_seek(s->v, (unsigned int)frame) ? 1 : 0;
}

static void rci_media_profile_vorbis_close(void *stream) {
    RCI_MediaProfileVorbis *s = (RCI_MediaProfileVorbis *)stream;

    stb_vorbis_close(s->v);
    free(s->arena);
    free(s);
}

typedef struct RCI_MediaProfileWebmDemux {
    FILE               *file;
    nestegg            *ne;
    unsigned int        track[2];
    unsigned long long  step[2];
    int                 opus;
    nestegg_packet     *packet;
    unsigned int        frame, frames;
    int                 kind, key;
    unsigned long long  ns;
    int                 need_key;
    unsigned char      *head;
} RCI_MediaProfileWebmDemux;

static int64_t rci_media_profile_webm_io_read(void *buffer, size_t length, void *user) {
    size_t n;

    if (length > RCI_MEDIA_PROFILE_FRAME_MAX)
        return -1;
    n = fread(buffer, 1, length, (FILE *)user);
    return n ? (int64_t)n : ferror((FILE *)user) ? -1 : 0;
}

static int rci_media_profile_webm_io_seek(int64_t offset, int whence, void *user) {
    if ((int64_t)(long)offset != offset)
        return -1;
    return fseek((FILE *)user, (long)offset, whence == NESTEGG_SEEK_SET ? SEEK_SET
                                             : whence == NESTEGG_SEEK_CUR ? SEEK_CUR : SEEK_END) == 0 ? 0 : -1;
}

static int64_t rci_media_profile_webm_io_tell(void *user) {
    return (int64_t)ftell((FILE *)user);
}

static void rci_media_profile_webm_drop(RCI_MediaProfileWebmDemux *d) {
    if (d->packet)
        nestegg_free_packet(d->packet);
    d->packet = NULL;
    d->frame  = d->frames = 0;
}

static int rci_media_profile_webm_init(RCI_MediaProfileWebmDemux *d) {
    nestegg_io io;

    rci_media_profile_webm_drop(d);
    if (d->ne)
        nestegg_destroy(d->ne);
    d->ne       = NULL;
    io.read     = rci_media_profile_webm_io_read;
    io.seek     = rci_media_profile_webm_io_seek;
    io.tell     = rci_media_profile_webm_io_tell;
    io.userdata = d->file;
    if (fseek(d->file, 0, SEEK_SET) != 0 || nestegg_init(&d->ne, io, NULL, -1) != 0) {
        d->ne = NULL;
        return 0;
    }
    return 1;
}

static unsigned long long rci_media_profile_opus_toc_ns(const unsigned char *p, size_t n) {
    static const unsigned short frames[32] = { 480, 960, 1920, 2880, 480, 960, 1920, 2880, 480, 960, 1920, 2880,
                                               480, 960, 480, 960, 120, 240, 480, 960, 120, 240, 480, 960,
                                               120, 240, 480, 960, 120, 240, 480, 960 };
    unsigned int count;

    if (n < 1)
        return 0;
    count = (p[0] & 3) == 0 ? 1 : (p[0] & 3) < 3 ? 2 : n > 1 ? p[1] & 63u : 0;
    return (unsigned long long)count * frames[p[0] >> 3] * 62500 / 3;
}

static void rci_media_profile_webm_close(void *demux) {
    RCI_MediaProfileWebmDemux *d = (RCI_MediaProfileWebmDemux *)demux;

    rci_media_profile_webm_drop(d);
    if (d->ne)
        nestegg_destroy(d->ne);
    free(d->head);
    free(d);
}

static void *rci_media_profile_webm_open(void *file, struct RCI_MediaProfileWebmInfo *info, int *refusal,
                                         char *why) {
    RCI_MediaProfileWebmDemux *d = (RCI_MediaProfileWebmDemux *)calloc(1, sizeof *d);
    unsigned int               tracks = 0, t;
    uint64_t                   v;

    memset(info, 0, sizeof *info);
    if (!d) {
        rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_NO_MEMORY, why, "out of memory for a WebM demuxer");
        return NULL;
    }
    d->file     = (FILE *)file;
    d->track[0] = d->track[1] = RCI_MEDIA_PROFILE_NO_TRACK;
    if (!rci_media_profile_webm_init(d) || nestegg_track_count(d->ne, &tracks) != 0) {
        rci_media_profile_webm_close(d);
        rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_DAMAGED, why, "not a WebM file nestegg reads");
        return NULL;
    }
    for (t = 0; t < tracks; t++) {
        const int type = nestegg_track_type(d->ne, t);
        const int k    = type == NESTEGG_TRACK_VIDEO ? 0 : type == NESTEGG_TRACK_AUDIO ? 1 : -1;

        if (k >= 0 && d->track[k] == RCI_MEDIA_PROFILE_NO_TRACK)
            d->track[k] = t;
    }
    if (d->track[0] != RCI_MEDIA_PROFILE_NO_TRACK) {
        const int            codec = nestegg_track_codec_id(d->ne, d->track[0]);
        nestegg_video_params p;

        info->video = nestegg_track_encoding(d->ne, d->track[0]) != NESTEGG_ENCODING_COMPRESSION ? 3
                    : codec == NESTEGG_CODEC_VP8 ? 1 : codec == NESTEGG_CODEC_VP9 ? 2 : 3;
        if (nestegg_track_video_params(d->ne, d->track[0], &p) != 0 ||
            (info->video < 3 && (p.width == 0 || p.width > 65536 || p.height == 0 || p.height > 65536))) {
            rci_media_profile_webm_close(d);
            rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_DAMAGED, why,
                                     "a WebM video track whose size is not 1 to 65,536 a side");
            return NULL;
        }
        info->width  = info->video < 3 ? (int)p.width : 0;
        info->height = info->video < 3 ? (int)p.height : 0;
    }
    if (d->track[1] != RCI_MEDIA_PROFILE_NO_TRACK) {
        const int            codec = nestegg_track_codec_id(d->ne, d->track[1]);
        nestegg_audio_params a;
        unsigned char       *head = NULL;
        size_t               size = 0;

        info->audio = nestegg_track_encoding(d->ne, d->track[1]) != NESTEGG_ENCODING_COMPRESSION ? 3
                    : codec == NESTEGG_CODEC_OPUS ? 1 : codec == NESTEGG_CODEC_VORBIS ? 2 : 3;
        if (nestegg_track_audio_params(d->ne, d->track[1], &a) != 0) {
            rci_media_profile_webm_close(d);
            rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_DAMAGED, why,
                                     "a WebM audio track without its parameters");
            return NULL;
        }
        info->channels        = a.channels <= 255 ? (int)a.channels : 0;
        info->rate            = a.rate >= 1 && a.rate <= RCI_MEDIA_PROFILE_RATE_MAX ? (int)(a.rate + 0.5) : 0;
        info->codec_delay_ns  = a.codec_delay;
        info->seek_preroll_ns = a.seek_preroll;
        d->opus               = info->audio == 1;
        if (d->opus && nestegg_track_codec_data(d->ne, d->track[1], 0, &head, &size) == 0 && size) {
            if (!(d->head = (unsigned char *)malloc(size))) {
                rci_media_profile_webm_close(d);
                rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_NO_MEMORY, why,
                                         "out of memory for a WebM file's Opus head");
                return NULL;
            }
            memcpy(d->head, head, size);
            info->audio_private      = d->head;
            info->audio_private_size = size;
        }
    }
    if (!info->video && !info->audio) {
        rci_media_profile_webm_close(d);
        rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_UNPLAYABLE, why,
                                 "a WebM file with neither a video nor an audio track");
        return NULL;
    }
    for (t = 0; t < 2; t++)
        if (d->track[t] != RCI_MEDIA_PROFILE_NO_TRACK && nestegg_track_default_duration(d->ne, d->track[t], &v) == 0)
            d->step[t] = v;
    if (nestegg_duration(d->ne, &v) == 0)
        info->duration_ns = v;
    return d;
}

static int rci_media_profile_webm_packet(RCI_MediaProfileWebmDemux *d) {
    for (;;) {
        unsigned int track = 0, frames = 0;
        uint64_t     ns    = 0;
        int          r;

        rci_media_profile_webm_drop(d);
        if (!d->ne)
            return -1;
        if ((r = nestegg_read_packet(d->ne, &d->packet)) != 1)
            return r < 0 ? -1 : 0;
        if (nestegg_packet_track(d->packet, &track) != 0 || nestegg_packet_tstamp(d->packet, &ns) != 0 ||
            nestegg_packet_count(d->packet, &frames) != 0)
            return -1;
        if (track != d->track[0] && track != d->track[1])
            continue;
        d->kind = track == d->track[1];
        d->key  = nestegg_packet_has_keyframe(d->packet) != NESTEGG_PACKET_HAS_KEYFRAME_FALSE;
        if (d->kind == 0 && d->need_key && !d->key)
            continue;
        d->need_key = d->need_key && d->kind;
        d->ns       = ns;
        d->frames   = frames;
        if (frames)
            return 1;
    }
}

static int rci_media_profile_webm_read(void *demux, int *track, const unsigned char **data, size_t *size,
                                       unsigned long long *ns, int *key) {
    RCI_MediaProfileWebmDemux *d = (RCI_MediaProfileWebmDemux *)demux;
    unsigned char             *p = NULL;
    size_t                     n = 0;
    int                        r;

    if (d->frame == d->frames && (r = rci_media_profile_webm_packet(d)) != 1)
        return r;
    if (nestegg_packet_data(d->packet, d->frame++, &p, &n) != 0)
        return -1;
    *track = d->kind;
    *data  = p;
    *size  = n;
    *ns    = d->ns;
    *key   = d->key;
    d->ns += d->kind && d->opus ? rci_media_profile_opus_toc_ns(p, n) : d->step[d->kind];
    return 1;
}

static int rci_media_profile_webm_scan(RCI_MediaProfileWebmDemux *d, int k, unsigned long long ns) {
    unsigned long long at = 0, i;

    if (!rci_media_profile_webm_init(d))
        return 0;
    for (i = 0;; i++) {
        nestegg_packet *p     = NULL;
        unsigned int    track = 0;
        uint64_t        time  = 0;
        int             stop;

        if (nestegg_read_packet(d->ne, &p) != 1)
            break;
        stop = nestegg_packet_track(p, &track) != 0 || nestegg_packet_tstamp(p, &time) != 0 ||
               (track == d->track[k] && time > ns);
        if (!stop && track == d->track[k] &&
            (k || nestegg_packet_has_keyframe(p) != NESTEGG_PACKET_HAS_KEYFRAME_FALSE))
            at = i;
        nestegg_free_packet(p);
        if (stop)
            break;
    }
    if (!rci_media_profile_webm_init(d))
        return 0;
    for (i = 0; i < at; i++) {
        nestegg_packet *p = NULL;

        if (nestegg_read_packet(d->ne, &p) != 1)
            return 0;
        nestegg_free_packet(p);
    }
    return 1;
}

static int rci_media_profile_webm_seek(void *demux, unsigned long long ns) {
    RCI_MediaProfileWebmDemux *d = (RCI_MediaProfileWebmDemux *)demux;
    const int                  k = d->track[0] == RCI_MEDIA_PROFILE_NO_TRACK;

    rci_media_profile_webm_drop(d);
    d->need_key = !k;
    if (d->ne && nestegg_track_seek(d->ne, d->track[k], ns) == 0)
        return 1;
    return rci_media_profile_webm_scan(d, k, ns);
}

static void *rci_media_profile_opus_open(const unsigned char *head, size_t size, int *channels, int *preskip,
                                         int *refusal, char *why) {
    unsigned char  map[RCI_MEDIA_PROFILE_CHANNELS_MAX] = { 0, 1 };
    int            n, streams = 1, coupled, error = OPUS_OK, ok, i;
    OpusMSDecoder *dec;

    if (!head || size < 19 || memcmp(head, "OpusHead", 8) != 0 || (head[8] & 0xF0) != 0 || head[9] == 0) {
        rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_DAMAGED, why, "not an OpusHead (RFC 7845) of version 0");
        return NULL;
    }
    n = head[9];
    if (head[18] > 1 || n > RCI_MEDIA_PROFILE_CHANNELS_MAX) {
        rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_UNPLAYABLE, why,
                                 "an Opus stream of more than 8 channels or another mapping family than 0 and 1");
        return NULL;
    }
    coupled = n - 1;
    ok      = head[18] == 0 ? n <= 2 : size >= 21 + (size_t)n;
    if (ok && head[18] == 1) {
        streams = head[19];
        coupled = head[20];
        memcpy(map, head + 21, (size_t)n);
    }
    for (i = 0; ok && i < n; i++)
        ok = map[i] == 255 || map[i] < streams + coupled;
    if (!ok || streams < 1 || streams > n || coupled > streams) {
        rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_DAMAGED, why, "an OpusHead whose channel mapping is "
                                 "damaged");
        return NULL;
    }
    dec = opus_multistream_decoder_create(48000, n, streams, coupled, map, &error);
    if (dec && (head[16] | head[17]) != 0) {
        const int gain = (int)((unsigned int)(head[16] | head[17] << 8) ^ 0x8000u) - 0x8000;

        if (opus_multistream_decoder_ctl(dec, OPUS_SET_GAIN_REQUEST, (opus_int32)gain) != OPUS_OK) {
            opus_multistream_decoder_destroy(dec);
            dec   = NULL;
            error = OPUS_BAD_ARG;
        }
    }
    if (!dec) {
        rci_media_profile_refuse(refusal, error == OPUS_ALLOC_FAIL ? RCI_MEDIA_PROFILE_NO_MEMORY
                                                                   : RCI_MEDIA_PROFILE_DAMAGED, why,
                                 error == OPUS_ALLOC_FAIL ? "out of memory for an Opus decoder"
                                                          : "an OpusHead libopus refuses");
        return NULL;
    }
    *channels = n;
    *preskip  = head[10] | head[11] << 8;
    return dec;
}

static int rci_media_profile_opus_decode(void *dec, const unsigned char *data, size_t size, float *pcm, int max) {
    int n;

    if (data && (size == 0 || size > 0x7FFFFFFF))
        return -1;
    n = opus_multistream_decode_float((OpusMSDecoder *)dec, data, data ? (opus_int32)size : 0, pcm,
                                      data ? max : max - max % 120, 0);
    return n < 0 ? -1 : n;
}

static void rci_media_profile_opus_close(void *dec) {
    opus_multistream_decoder_destroy((OpusMSDecoder *)dec);
}

typedef struct RCI_MediaProfileVpx {
    vpx_codec_ctx_t ctx;
    int             vp9;
} RCI_MediaProfileVpx;

#define RCI_MEDIA_PROFILE_VPX_SIDE_MAX 8192

static void *rci_media_profile_vpx_open(int vp9, int threads, int *refusal, char *why) {
    RCI_MediaProfileVpx *s = (RCI_MediaProfileVpx *)calloc(1, sizeof *s);
    vpx_codec_dec_cfg_t  cfg;
    vpx_codec_err_t      res;

    if (!s) {
        rci_media_profile_refuse(refusal, RCI_MEDIA_PROFILE_NO_MEMORY, why, "out of memory for a VP8 or VP9 decoder");
        return NULL;
    }
    memset(&cfg, 0, sizeof cfg);
    cfg.threads = (unsigned int)(threads < 1 ? 1 : threads > RCI_MEDIA_PROFILE_THREADS_MAX
                                                 ? RCI_MEDIA_PROFILE_THREADS_MAX : threads);
    res = vpx_codec_dec_init(&s->ctx, vp9 ? vpx_codec_vp9_dx() : vpx_codec_vp8_dx(), &cfg, 0);
    if (res != VPX_CODEC_OK) {
        free(s);
        rci_media_profile_refuse(refusal, res == VPX_CODEC_MEM_ERROR ? RCI_MEDIA_PROFILE_NO_MEMORY
                                                                     : RCI_MEDIA_PROFILE_UNPLAYABLE,
                                 why, vpx_codec_err_to_string(res));
        return NULL;
    }

    if (vp9 && cfg.threads > 1)
        vpx_codec_control(&s->ctx, VP9D_SET_ROW_MT, 1);
    s->vp9 = vp9;
    return s;
}

static int rci_media_profile_vpx_decode(void *dec, const unsigned char *data, size_t size,
                                        const unsigned char **plane, int *stride, int *width, int *height,
                                        int *matrix, int *full_range, char *why) {
    RCI_MediaProfileVpx *s    = (RCI_MediaProfileVpx *)dec;
    vpx_codec_iter_t     iter = NULL;
    vpx_image_t         *img, *shown = NULL;
    vpx_codec_err_t      res;

    if (!s->vp9 && data && size && (unsigned int)size == size) {
        vpx_codec_stream_info_t si;

        memset(&si, 0, sizeof si);
        si.sz = sizeof si;
        if (vpx_codec_peek_stream_info(vpx_codec_vp8_dx(), data, (unsigned int)size, &si) == VPX_CODEC_OK &&
            si.is_kf && (si.w > RCI_MEDIA_PROFILE_VPX_SIDE_MAX || si.h > RCI_MEDIA_PROFILE_VPX_SIDE_MAX)) {
            rci_media_profile_why(why, "a VP8 picture wider or taller than 8,192 pixels");
            return -2;
        }
    }
    res = (unsigned int)size != size ? VPX_CODEC_CORRUPT_FRAME
                                     : vpx_codec_decode(&s->ctx, data, (unsigned int)size, NULL, 0);
    if (res != VPX_CODEC_OK) {

        if (res == VPX_CODEC_UNSUP_BITSTREAM && s->vp9 && size && (data[0] & 0xD0) == 0x90) {
            rci_media_profile_why(why, "a VP9 stream of 10 or 12 bits (profile 2 or 3): only 8 bits are played");
            return -2;
        }
        return -1;
    }

    while ((img = vpx_codec_get_frame(&s->ctx, &iter)))
        shown = img;
    if (!shown)
        return 0;
    if (shown->fmt != VPX_IMG_FMT_I420 && shown->fmt != VPX_IMG_FMT_YV12) {
        rci_media_profile_why(why, shown->fmt == VPX_IMG_FMT_I444 ? "a VP9 stream in 4:4:4: only 4:2:0 is played"
                                   : shown->fmt == VPX_IMG_FMT_I422 ? "a VP9 stream in 4:2:2: only 4:2:0 is played"
                                   : shown->fmt == VPX_IMG_FMT_I440 ? "a VP9 stream in 4:4:0: only 4:2:0 is played"
                                   : "a VP8 or VP9 picture format other than 8-bit 4:2:0");
        return -2;
    }
    for (int i = 0; i < 3; i++) {
        plane[i]  = shown->planes[i];
        stride[i] = shown->stride[i];
    }
    *width      = (int)shown->d_w;
    *height     = (int)shown->d_h;

    *matrix     = shown->cs == VPX_CS_BT_601 || shown->cs == VPX_CS_SMPTE_170 ? 0
                : shown->cs == VPX_CS_BT_2020 ? 2
                : shown->cs == VPX_CS_UNKNOWN || shown->cs == VPX_CS_RESERVED ? shown->d_h >= 720 : 1;
    *full_range = shown->range == VPX_CR_FULL_RANGE;
    return 1;
}

static void rci_media_profile_vpx_close(void *dec) {
    vpx_codec_destroy(&((RCI_MediaProfileVpx *)dec)->ctx);
    free(dec);
}

const struct RCI_MediaProfile RCI_MEDIA_PROFILE_ANCHOR = {
    { rci_media_profile_mp3_open, rci_media_profile_mp3_read, rci_media_profile_mp3_seek_to,
      rci_media_profile_mp3_close },
    { rci_media_profile_flac_open, rci_media_profile_flac_read, rci_media_profile_flac_seek_to,
      rci_media_profile_flac_close },
    { rci_media_profile_vorbis_open, rci_media_profile_vorbis_read, rci_media_profile_vorbis_seek_to,
      rci_media_profile_vorbis_close },
    { rci_media_profile_webm_open, rci_media_profile_webm_read, rci_media_profile_webm_seek,
      rci_media_profile_webm_close },
    { rci_media_profile_vpx_open, rci_media_profile_vpx_decode, rci_media_profile_vpx_close },
    { rci_media_profile_opus_open, rci_media_profile_opus_decode, rci_media_profile_opus_close },
    "dr_mp3/" DRMP3_VERSION_STRING " dr_flac/" DRFLAC_VERSION_STRING " stb_vorbis/1.22 nestegg/767aab25013a"
    " libvpx/" RCI_MEDIA_PROFILE_STR(VERSION_MAJOR) "." RCI_MEDIA_PROFILE_STR(VERSION_MINOR) "."
    RCI_MEDIA_PROFILE_STR(VERSION_PATCH) " libopus/1.6.1"
};
