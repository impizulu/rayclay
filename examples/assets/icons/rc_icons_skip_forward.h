#ifndef RC_ICON_SKIP_FORWARD_H
#define RC_ICON_SKIP_FORWARD_H

#include "rc_icons_common.h"

/*
    Generated from skip-forward.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconSkipForward(RC_BoundingBox bounds,
                                         RC_Color color,
                                         const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 21.0f, 4.0f, 21.0f, 20.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 6.029f, 4.285f },
        { 5.809f, 4.171f },
        { 5.576f, 4.085f },
        { 5.334f, 4.028f },
        { 5.087f, 4.002f },
        { 4.839f, 4.006f },
        { 4.593f, 4.042f },
        { 4.354f, 4.107f },
        { 4.124f, 4.202f },
        { 3.908f, 4.324f },
        { 3.709f, 4.472f },
        { 3.53f, 4.644f },
        { 3.373f, 4.836f },
        { 3.242f, 5.047f },
        { 3.137f, 5.272f },
        { 3.061f, 5.508f },
        { 3.015f, 5.752f },
        { 3.0f, 6.0f },
        { 3.0f, 18.0f },
        { 3.015f, 18.248f },
        { 3.061f, 18.492f },
        { 3.137f, 18.728f },
        { 3.242f, 18.953f },
        { 3.373f, 19.164f },
        { 3.53f, 19.356f },
        { 3.709f, 19.528f },
        { 3.908f, 19.676f },
        { 4.124f, 19.798f },
        { 4.354f, 19.893f },
        { 4.593f, 19.958f },
        { 4.839f, 19.994f },
        { 5.087f, 19.998f },
        { 5.334f, 19.972f },
        { 5.576f, 19.915f },
        { 5.809f, 19.829f },
        { 6.029f, 19.715f },
        { 16.026f, 13.717f },
        { 16.238f, 13.571f },
        { 16.43f, 13.399f },
        { 16.598f, 13.203f },
        { 16.739f, 12.988f },
        { 16.852f, 12.756f },
        { 16.934f, 12.512f },
        { 16.983f, 12.259f },
        { 17.0f, 12.002f },
        { 16.984f, 11.745f },
        { 16.935f, 11.492f },
        { 16.853f, 11.247f },
        { 16.741f, 11.015f },
        { 16.6f, 10.8f },
        { 16.432f, 10.604f },
        { 16.241f, 10.431f },
        { 16.029f, 10.285f }
    };
    rcIconDrawPolyline(bounds, path0,
                       (int)(sizeof(path0) / sizeof(path0[0])),
                       viewBox, stroke, true, color);
}

static inline void rcIconSkipForward(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconSkipForward, NULL);
}

#endif /* RC_ICON_SKIP_FORWARD_H */
