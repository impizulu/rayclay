#ifndef RC_ICON_COPY_H
#define RC_ICON_COPY_H

#include "rc_icons_common.h"

/*
    Generated from copy.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconCopy(RC_BoundingBox bounds,
                                  RC_Color color,
                                  const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundedRectStroke(bounds, 8.0f, 8.0f, 14.0f, 14.0f,
                                2.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 4.0f, 16.0f },
        { 3.796f, 15.99f },
        { 3.598f, 15.959f },
        { 3.407f, 15.91f },
        { 3.223f, 15.842f },
        { 3.049f, 15.758f },
        { 2.884f, 15.657f },
        { 2.73f, 15.542f },
        { 2.587f, 15.412f },
        { 2.458f, 15.27f },
        { 2.343f, 15.116f },
        { 2.242f, 14.951f },
        { 2.158f, 14.777f },
        { 2.09f, 14.593f },
        { 2.041f, 14.402f },
        { 2.01f, 14.204f },
        { 2.0f, 14.0f },
        { 2.0f, 4.0f },
        { 2.01f, 3.796f },
        { 2.041f, 3.598f },
        { 2.09f, 3.407f },
        { 2.158f, 3.223f },
        { 2.242f, 3.049f },
        { 2.343f, 2.884f },
        { 2.458f, 2.73f },
        { 2.587f, 2.587f },
        { 2.73f, 2.458f },
        { 2.884f, 2.343f },
        { 3.049f, 2.242f },
        { 3.223f, 2.158f },
        { 3.407f, 2.09f },
        { 3.598f, 2.041f },
        { 3.796f, 2.01f },
        { 4.0f, 2.0f },
        { 14.0f, 2.0f },
        { 14.204f, 2.01f },
        { 14.402f, 2.041f },
        { 14.593f, 2.09f },
        { 14.777f, 2.158f },
        { 14.951f, 2.242f },
        { 15.116f, 2.343f },
        { 15.27f, 2.458f },
        { 15.412f, 2.587f },
        { 15.542f, 2.73f },
        { 15.657f, 2.884f },
        { 15.758f, 3.049f },
        { 15.842f, 3.223f },
        { 15.91f, 3.407f },
        { 15.959f, 3.598f },
        { 15.99f, 3.796f },
        { 16.0f, 4.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       51,
                       viewBox, stroke, false, color);
}

static inline void rcIconCopy(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconCopy, NULL);
}

#endif /* RC_ICON_COPY_H */
