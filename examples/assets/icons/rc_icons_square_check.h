#ifndef RC_ICON_SQUARE_CHECK_H
#define RC_ICON_SQUARE_CHECK_H

#include "rc_icons_common.h"

/*
    Generated from square-check.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconSquareCheck(RC_BoundingBox bounds,
                                         RC_Color color,
                                         const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundedRectStroke(bounds, 3.0f, 3.0f, 18.0f, 18.0f,
                                2.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 9.0f, 12.0f },
        { 11.0f, 14.0f },
        { 15.0f, 10.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       3,
                       viewBox, stroke, false, color);
}

static inline void rcIconSquareCheck(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconSquareCheck, NULL);
}

#endif /* RC_ICON_SQUARE_CHECK_H */
