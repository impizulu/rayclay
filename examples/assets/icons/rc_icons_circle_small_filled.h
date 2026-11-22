#ifndef RC_ICON_CIRCLE_SMALL_FILLED_H
#define RC_ICON_CIRCLE_SMALL_FILLED_H

#include "rc_icons_common.h"

/*
    Generated from circle-small-filled.svg (Lucide). The layout pass owns
    placement; the library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconCircleSmallFilled(RC_BoundingBox bounds,
                                               RC_Color color,
                                               const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawFilledCircle(bounds, 12.0f, 12.0f, 6.0f, viewBox, color);
    rcIconDrawCircleStroke(bounds, 12.0f, 12.0f, 6.0f, viewBox, stroke, color);
}

static inline void rcIconCircleSmallFilled(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconCircleSmallFilled, NULL);
}

#endif /* RC_ICON_CIRCLE_SMALL_FILLED_H */
