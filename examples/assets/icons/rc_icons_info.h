#ifndef RC_ICON_INFO_H
#define RC_ICON_INFO_H

#include "rc_icons_common.h"

/*
    Generated from info.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconInfo(RC_BoundingBox bounds,
                                  RC_Color color,
                                  const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawCircleStroke(bounds, 12.0f, 12.0f, 10.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 12.0f, 16.0f, 12.0f, 12.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 12.0f, 8.0f, 12.01f, 8.0f, viewBox, stroke, color);
}

static inline void rcIconInfo(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconInfo, NULL);
}

#endif /* RC_ICON_INFO_H */
