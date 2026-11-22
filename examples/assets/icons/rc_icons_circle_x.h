#ifndef RC_ICON_CIRCLE_X_H
#define RC_ICON_CIRCLE_X_H

#include "rc_icons_common.h"

/*
    Generated from circle-x.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconCircleX(RC_BoundingBox bounds,
                                     RC_Color color,
                                     const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawCircleStroke(bounds, 12.0f, 12.0f, 10.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 15.0f, 9.0f, 9.0f, 15.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 9.0f, 9.0f, 15.0f, 15.0f, viewBox, stroke, color);
}

static inline void rcIconCircleX(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconCircleX, NULL);
}

#endif /* RC_ICON_CIRCLE_X_H */
