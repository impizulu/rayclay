#ifndef RC_ICON_SUN_H
#define RC_ICON_SUN_H

#include "rc_icons_common.h"

/*
    Generated from sun.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconSun(RC_BoundingBox bounds,
                                 RC_Color color,
                                 const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawCircleStroke(bounds, 12.0f, 12.0f, 4.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 12.0f, 2.0f, 12.0f, 4.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 12.0f, 20.0f, 12.0f, 22.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 4.93f, 4.93f, 6.34f, 6.34f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 17.66f, 17.66f, 19.07f, 19.07f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 2.0f, 12.0f, 4.0f, 12.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 20.0f, 12.0f, 22.0f, 12.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 6.34f, 17.66f, 4.93f, 19.07f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 19.07f, 4.93f, 17.66f, 6.34f, viewBox, stroke, color);
}

static inline void rcIconSun(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconSun, NULL);
}

#endif /* RC_ICON_SUN_H */
