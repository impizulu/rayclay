#ifndef RC_ICON_IMAGES_H
#define RC_ICON_IMAGES_H

#include "rc_icons_common.h"

/*
    Generated from images.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconImages(RC_BoundingBox bounds,
                                    RC_Color color,
                                    const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 22.0f, 11.0f },
        { 20.704f, 9.704f },
        { 20.487f, 9.51f },
        { 20.247f, 9.344f },
        { 19.99f, 9.208f },
        { 19.718f, 9.104f },
        { 19.435f, 9.034f },
        { 19.146f, 8.999f },
        { 18.854f, 8.999f },
        { 18.565f, 9.034f },
        { 18.282f, 9.104f },
        { 18.01f, 9.208f },
        { 17.753f, 9.344f },
        { 17.513f, 9.51f },
        { 17.296f, 9.704f },
        { 11.0f, 16.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       16,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 4.0f, 8.0f },
        { 3.739f, 8.017f },
        { 3.482f, 8.068f },
        { 3.235f, 8.152f },
        { 3.0f, 8.268f },
        { 2.782f, 8.413f },
        { 2.586f, 8.586f },
        { 2.413f, 8.782f },
        { 2.268f, 9.0f },
        { 2.152f, 9.235f },
        { 2.068f, 9.482f },
        { 2.017f, 9.739f },
        { 2.0f, 10.0f },
        { 2.0f, 20.0f },
        { 2.017f, 20.261f },
        { 2.068f, 20.518f },
        { 2.152f, 20.765f },
        { 2.268f, 21.0f },
        { 2.413f, 21.218f },
        { 2.586f, 21.414f },
        { 2.782f, 21.587f },
        { 3.0f, 21.732f },
        { 3.235f, 21.848f },
        { 3.482f, 21.932f },
        { 3.739f, 21.983f },
        { 4.0f, 22.0f },
        { 14.0f, 22.0f },
        { 14.261f, 21.983f },
        { 14.518f, 21.932f },
        { 14.765f, 21.848f },
        { 15.0f, 21.732f },
        { 15.218f, 21.587f },
        { 15.414f, 21.414f },
        { 15.587f, 21.218f },
        { 15.732f, 21.0f },
        { 15.848f, 20.765f },
        { 15.932f, 20.518f },
        { 15.983f, 20.261f },
        { 16.0f, 20.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       39,
                       viewBox, stroke, false, color);

    rcIconDrawFilledCircle(bounds, 13.0f, 7.0f, 1.0f, viewBox, color);
    rcIconDrawCircleStroke(bounds, 13.0f, 7.0f, 1.0f, viewBox, stroke, color);
    rcIconDrawRoundedRectStroke(bounds, 8.0f, 2.0f, 14.0f, 14.0f,
                                2.0f, viewBox, stroke, color);
}

static inline void rcIconImages(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconImages, NULL);
}

#endif /* RC_ICON_IMAGES_H */
