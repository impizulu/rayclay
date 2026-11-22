#ifndef RC_ICON_CROSSHAIR_H
#define RC_ICON_CROSSHAIR_H

#include "rc_icons_common.h"

/*
    Generated from crosshair.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconCrosshair(RC_BoundingBox bounds,
                                       RC_Color color,
                                       const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawCircleStroke(bounds, 12.0f, 12.0f, 10.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 22.0f, 12.0f, 18.0f, 12.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 6.0f, 12.0f, 2.0f, 12.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 12.0f, 6.0f, 12.0f, 2.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 12.0f, 22.0f, 12.0f, 18.0f, viewBox, stroke, color);
}

static inline void rcIconCrosshair(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconCrosshair, NULL);
}

#endif /* RC_ICON_CROSSHAIR_H */
