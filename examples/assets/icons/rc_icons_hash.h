#ifndef RC_ICON_HASH_H
#define RC_ICON_HASH_H

#include "rc_icons_common.h"

/*
    Generated from hash.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconHash(RC_BoundingBox bounds,
                                  RC_Color color,
                                  const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 4.0f, 9.0f, 20.0f, 9.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 4.0f, 15.0f, 20.0f, 15.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 10.0f, 3.0f, 8.0f, 21.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 16.0f, 3.0f, 14.0f, 21.0f, viewBox, stroke, color);
}

static inline void rcIconHash(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconHash, NULL);
}

#endif /* RC_ICON_HASH_H */
