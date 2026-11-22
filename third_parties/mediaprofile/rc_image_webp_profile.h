#ifndef RC_IMAGE_WEBP_PROFILE_H
#define RC_IMAGE_WEBP_PROFILE_H

#include <stddef.h>

#define RCI_IMAGE_WEBP_ANCHOR rci_image_webp_link_rayclay_mediaprofile_43cc4f0fa706

#define RCI_IMAGE_WEBP_WHY 96

struct RCI_ImageWebpCodec {

    void *(*open)(const unsigned char *bytes, size_t len, int *width, int *height, int *frames, int *plays,
                  char *why);

    int   (*frame)(void *decoder, unsigned char *canvas, int *ms, char *why);
    void  (*close)(void *decoder);

    const char *name;
};

extern const struct RCI_ImageWebpCodec RCI_IMAGE_WEBP_ANCHOR;

#endif
