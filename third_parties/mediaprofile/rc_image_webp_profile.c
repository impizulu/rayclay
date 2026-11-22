#include "rc_image_webp_profile.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "src/webp/decode.h"
#include "src/webp/demux.h"
#include "src/dec/vp8i_dec.h"

#define RCI_IMAGE_WEBP_PROFILE_STR_(x) #x
#define RCI_IMAGE_WEBP_PROFILE_STR(x)  RCI_IMAGE_WEBP_PROFILE_STR_(x)
#define RCI_IMAGE_WEBP_PROFILE_VERSION                                                           \
    "libwebp/" RCI_IMAGE_WEBP_PROFILE_STR(DEC_MAJ_VERSION) "."                                    \
    RCI_IMAGE_WEBP_PROFILE_STR(DEC_MIN_VERSION) "." RCI_IMAGE_WEBP_PROFILE_STR(DEC_REV_VERSION)

typedef struct RCI_ImageWebpProfileDecoder {
    const unsigned char *bytes;
    size_t               len;
    int                  width, height;
    int                  frames;
    int                  next;
    int                  ended;
    WebPAnimDecoder     *anim;
} RCI_ImageWebpProfileDecoder;

static void rci_image_webp_profile_why(char *why, const char *text) {
    size_t n = strlen(text);

    n = n < RCI_IMAGE_WEBP_WHY - 1 ? n : RCI_IMAGE_WEBP_WHY - 1;
    memcpy(why, text, n);
    why[n] = '\0';
}

static const char *rci_image_webp_profile_status(VP8StatusCode st) {
    switch (st) {
    case VP8_STATUS_OUT_OF_MEMORY:       return "out of memory";
    case VP8_STATUS_NOT_ENOUGH_DATA:     return "truncated image data";
    case VP8_STATUS_UNSUPPORTED_FEATURE: return "a WebP feature libwebp does not decode";
    case VP8_STATUS_BITSTREAM_ERROR:     return "damaged image data";
    default:                             return "libwebp refused the image";
    }
}

static void *rci_image_webp_profile_open(const unsigned char *bytes, size_t len, int *width, int *height, int *frames,
                                         int *plays, char *why) {
    WebPBitstreamFeatures        f;
    RCI_ImageWebpProfileDecoder *d;
    VP8StatusCode                st = WebPGetFeatures(bytes, len, &f);

    if (st != VP8_STATUS_OK) {
        rci_image_webp_profile_why(why, rci_image_webp_profile_status(st));
        return NULL;
    }
    d = (RCI_ImageWebpProfileDecoder *)calloc(1, sizeof *d);
    if (!d) {
        rci_image_webp_profile_why(why, "out of memory");
        return NULL;
    }
    d->bytes  = bytes;
    d->len    = len;
    d->width  = f.width;
    d->height = f.height;
    d->frames = 1;
    *plays    = 1;
    if (f.has_animation) {
        WebPAnimDecoderOptions o;
        WebPAnimInfo           info;
        WebPData               data;

        data.bytes = bytes;
        data.size  = len;
        if (!WebPAnimDecoderOptionsInit(&o)) {
            free(d);
            rci_image_webp_profile_why(why, "libwebp refused its own options");
            return NULL;
        }
        o.color_mode  = MODE_RGBA;
        o.use_threads = 0;
        d->anim = WebPAnimDecoderNew(&data, &o);
        if (!d->anim || !WebPAnimDecoderGetInfo(d->anim, &info) || info.frame_count < 1 ||
            info.frame_count > INT_MAX || info.canvas_width > INT_MAX || info.canvas_height > INT_MAX) {
            if (d->anim)
                WebPAnimDecoderDelete(d->anim);
            free(d);
            rci_image_webp_profile_why(why, "an animation libwebp's demuxer refuses (damaged, or out of memory)");
            return NULL;
        }
        d->width  = (int)info.canvas_width;
        d->height = (int)info.canvas_height;
        d->frames = (int)info.frame_count;
        *plays    = (int)info.loop_count;
    }
    *width  = d->width;
    *height = d->height;
    *frames = d->frames;
    return d;
}

static int rci_image_webp_profile_frame(void *decoder, unsigned char *canvas, int *ms, char *why) {
    RCI_ImageWebpProfileDecoder *d   = (RCI_ImageWebpProfileDecoder *)decoder;
    const size_t                 row = (size_t)d->width * 4u;
    uint8_t                     *px;
    int                          ended;

    if (!d->anim) {
        WebPDecoderConfig c;
        VP8StatusCode     st;

        if (!WebPInitDecoderConfig(&c)) {
            rci_image_webp_profile_why(why, "libwebp refused its own options");
            return 0;
        }
        c.options.use_threads        = 0;
        c.output.colorspace          = MODE_RGBA;
        c.output.is_external_memory  = 1;
        c.output.u.RGBA.rgba         = canvas;
        c.output.u.RGBA.stride       = (int)row;
        c.output.u.RGBA.size         = row * (size_t)d->height;
        st = WebPDecode(d->bytes, d->len, &c);
        if (st != VP8_STATUS_OK) {
            rci_image_webp_profile_why(why, rci_image_webp_profile_status(st));
            return 0;
        }
        *ms = 0;
        return 1;
    }
    if (d->next == d->frames) {
        WebPAnimDecoderReset(d->anim);
        d->next  = 0;
        d->ended = 0;
    }
    if (!WebPAnimDecoderGetNext(d->anim, &px, &ended)) {
        rci_image_webp_profile_why(why, "damaged frame data");
        return 0;
    }
    memcpy(canvas, px, row * (size_t)d->height);

    *ms      = ended > d->ended ? ended - d->ended : 0;
    d->ended = ended;
    d->next++;
    return 1;
}

static void rci_image_webp_profile_close(void *decoder) {
    RCI_ImageWebpProfileDecoder *d = (RCI_ImageWebpProfileDecoder *)decoder;

    if (d) {
        if (d->anim)
            WebPAnimDecoderDelete(d->anim);
        free(d);
    }
}

const struct RCI_ImageWebpCodec RCI_IMAGE_WEBP_ANCHOR = {
    rci_image_webp_profile_open, rci_image_webp_profile_frame, rci_image_webp_profile_close,
    RCI_IMAGE_WEBP_PROFILE_VERSION
};
