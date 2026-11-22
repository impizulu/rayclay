#ifndef RC_ICON_IMAGE_H
#define RC_ICON_IMAGE_H

#include "rc_icons_common.h"

/*
    Generated from image.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconImage(RC_BoundingBox bounds,
                                   RC_Color color,
                                   const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundedRectStroke(bounds, 3.0f, 3.0f, 18.0f, 18.0f,
                                2.0f, viewBox, stroke, color);
    rcIconDrawCircleStroke(bounds, 9.0f, 9.0f, 2.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 21.0f, 15.0f },
        { 17.914f, 11.914f },
        { 17.717f, 11.742f },
        { 17.5f, 11.596f },
        { 17.265f, 11.481f },
        { 17.018f, 11.397f },
        { 16.761f, 11.346f },
        { 16.5f, 11.328f },
        { 16.239f, 11.346f },
        { 15.982f, 11.397f },
        { 15.735f, 11.481f },
        { 15.5f, 11.596f },
        { 15.283f, 11.742f },
        { 15.086f, 11.914f },
        { 6.0f, 21.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       15,
                       viewBox, stroke, false, color);
}

static inline void rcIconImage(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconImage, NULL);
}

#endif /* RC_ICON_IMAGE_H */
