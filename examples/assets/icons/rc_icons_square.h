#ifndef RC_ICON_SQUARE_H
#define RC_ICON_SQUARE_H

#include "rc_icons_common.h"

/*
    Generated from square.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconSquare(RC_BoundingBox bounds,
                                    RC_Color color,
                                    const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundedRectStroke(bounds, 3.0f, 3.0f, 18.0f, 18.0f,
                                2.0f, viewBox, stroke, color);
}

static inline void rcIconSquare(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconSquare, NULL);
}

#endif /* RC_ICON_SQUARE_H */
