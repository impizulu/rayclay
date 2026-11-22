#ifndef RC_ICON_SHRINK_H
#define RC_ICON_SHRINK_H

#include "rc_icons_common.h"

/*
    Generated from shrink.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconShrink(RC_BoundingBox bounds,
                                    RC_Color color,
                                    const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 15.0f, 15.0f, 21.0f, 21.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 15.0f, 15.0f, 15.0f, 19.8f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 15.0f, 15.0f, 19.8f, 15.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 9.0f, 19.8f, 9.0f, 15.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 9.0f, 15.0f, 4.2f, 15.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 9.0f, 15.0f, 3.0f, 21.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 15.0f, 4.2f, 15.0f, 9.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 15.0f, 9.0f, 19.8f, 9.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 15.0f, 9.0f, 21.0f, 3.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 9.0f, 4.2f, 9.0f, 9.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 9.0f, 9.0f, 4.2f, 9.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 9.0f, 9.0f, 3.0f, 3.0f, viewBox, stroke, color);
}

static inline void rcIconShrink(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconShrink, NULL);
}

#endif /* RC_ICON_SHRINK_H */
