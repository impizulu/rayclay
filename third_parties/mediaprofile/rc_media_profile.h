#ifndef RC_MEDIA_PROFILE_H
#define RC_MEDIA_PROFILE_H

#include <stddef.h>

#define RCI_MEDIA_PROFILE_ANCHOR rci_media_profile_link_rayclay_mediaprofile_43cc4f0fa706

#define RCI_MEDIA_PROFILE_WHY 96

#define RCI_MEDIA_PROFILE_DAMAGED    1
#define RCI_MEDIA_PROFILE_UNPLAYABLE 2
#define RCI_MEDIA_PROFILE_NO_MEMORY  3

struct RCI_MediaProfileCodec {

    void  *(*open)(void *file, int *channels, int *rate, unsigned long long *frames, int *refusal, char *why);

    size_t (*read)(void *stream, float *out, size_t frames);

    int    (*seek)(void *stream, unsigned long long frame);
    void   (*close)(void *stream);
};

struct RCI_MediaProfileWebmInfo {
    int                  video;
    int                  width, height;
    int                  audio;
    int                  channels, rate;
    const unsigned char *audio_private;
    size_t               audio_private_size;
    unsigned long long   duration_ns;
    unsigned long long   codec_delay_ns, seek_preroll_ns;
};

struct RCI_MediaProfileWebm {

    void *(*open)(void *file, struct RCI_MediaProfileWebmInfo *info, int *refusal, char *why);

    int   (*read)(void *demux, int *track, const unsigned char **data, size_t *size, unsigned long long *ns,
                  int *key);

    int   (*seek)(void *demux, unsigned long long ns);
    void  (*close)(void *demux);
};

struct RCI_MediaProfileVideo {

    void *(*open)(int vp9, int threads, int *refusal, char *why);

    int   (*decode)(void *dec, const unsigned char *data, size_t size, const unsigned char **plane, int *stride,
                    int *width, int *height, int *matrix, int *full_range, char *why);
    void  (*close)(void *dec);
};

struct RCI_MediaProfileOpus {

    void *(*open)(const unsigned char *head, size_t size, int *channels, int *preskip, int *refusal, char *why);

    int   (*decode)(void *dec, const unsigned char *data, size_t size, float *pcm, int max);
    void  (*close)(void *dec);
};

struct RCI_MediaProfile {
    struct RCI_MediaProfileCodec mp3;
    struct RCI_MediaProfileCodec flac;
    struct RCI_MediaProfileCodec vorbis;
    struct RCI_MediaProfileWebm  webm;
    struct RCI_MediaProfileVideo vpx;
    struct RCI_MediaProfileOpus  opus;

    const char *profile;
};

#endif
