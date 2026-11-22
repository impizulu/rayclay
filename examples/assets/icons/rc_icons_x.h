#ifndef RC_ICON_X_H
#define RC_ICON_X_H

#include "rc_icons_common.h"

/*
    Generated from x.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconX(RC_BoundingBox bounds,
                               RC_Color color,
                               const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 18.0f, 6.0f, 6.0f, 18.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 6.0f, 6.0f, 18.0f, 18.0f, viewBox, stroke, color);
}

static inline void rcIconX(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconX, NULL);
}

#endif /* RC_ICON_X_H */
