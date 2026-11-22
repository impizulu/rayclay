#ifndef RC_ICON_PLUS_H
#define RC_ICON_PLUS_H

#include "rc_icons_common.h"

/*
    Generated from plus.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconPlus(RC_BoundingBox bounds,
                                  RC_Color color,
                                  const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 5.0f, 12.0f, 19.0f, 12.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 12.0f, 5.0f, 12.0f, 19.0f, viewBox, stroke, color);
}

static inline void rcIconPlus(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconPlus, NULL);
}

#endif /* RC_ICON_PLUS_H */
