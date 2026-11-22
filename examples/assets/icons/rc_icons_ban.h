#ifndef RC_ICON_BAN_H
#define RC_ICON_BAN_H

#include "rc_icons_common.h"

/*
    Generated from ban.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconBan(RC_BoundingBox bounds,
                                 RC_Color color,
                                 const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawCircleStroke(bounds, 12.0f, 12.0f, 10.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 4.929f, 4.929f, 19.07f, 19.071f, viewBox, stroke, color);
}

static inline void rcIconBan(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconBan, NULL);
}

#endif /* RC_ICON_BAN_H */
