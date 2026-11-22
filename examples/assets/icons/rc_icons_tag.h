#ifndef RC_ICON_TAG_H
#define RC_ICON_TAG_H

#include "rc_icons_common.h"

/*
    Generated from tag.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconTag(RC_BoundingBox bounds,
                                 RC_Color color,
                                 const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 12.586f, 2.586f },
        { 12.389f, 2.413f },
        { 12.172f, 2.268f },
        { 11.937f, 2.152f },
        { 11.69f, 2.068f },
        { 11.433f, 2.017f },
        { 11.172f, 2.0f },
        { 4.0f, 2.0f },
        { 3.739f, 2.017f },
        { 3.482f, 2.068f },
        { 3.235f, 2.152f },
        { 3.0f, 2.268f },
        { 2.782f, 2.413f },
        { 2.586f, 2.586f },
        { 2.413f, 2.782f },
        { 2.268f, 3.0f },
        { 2.152f, 3.235f },
        { 2.068f, 3.482f },
        { 2.017f, 3.739f },
        { 2.0f, 4.0f },
        { 2.0f, 11.172f },
        { 2.017f, 11.433f },
        { 2.068f, 11.69f },
        { 2.152f, 11.937f },
        { 2.268f, 12.172f },
        { 2.413f, 12.389f },
        { 2.586f, 12.586f },
        { 11.29f, 21.29f },
        { 11.528f, 21.498f },
        { 11.791f, 21.673f },
        { 12.075f, 21.812f },
        { 12.375f, 21.913f },
        { 12.685f, 21.975f },
        { 13.0f, 21.995f },
        { 13.315f, 21.975f },
        { 13.625f, 21.913f },
        { 13.925f, 21.812f },
        { 14.209f, 21.673f },
        { 14.472f, 21.498f },
        { 14.71f, 21.29f },
        { 21.29f, 14.71f },
        { 21.498f, 14.472f },
        { 21.673f, 14.209f },
        { 21.812f, 13.925f },
        { 21.913f, 13.625f },
        { 21.975f, 13.315f },
        { 21.995f, 13.0f },
        { 21.975f, 12.685f },
        { 21.913f, 12.375f },
        { 21.812f, 12.075f },
        { 21.673f, 11.791f },
        { 21.498f, 11.528f },
        { 21.29f, 11.29f }
    };
    rcIconDrawPolyline(bounds, path0,
                       53,
                       viewBox, stroke, true, color);

    rcIconDrawFilledCircle(bounds, 7.5f, 7.5f, 0.5f, viewBox, color);
    rcIconDrawCircleStroke(bounds, 7.5f, 7.5f, 0.5f, viewBox, stroke, color);
}

static inline void rcIconTag(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconTag, NULL);
}

#endif /* RC_ICON_TAG_H */
