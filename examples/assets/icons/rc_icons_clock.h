#ifndef RC_ICON_CLOCK_H
#define RC_ICON_CLOCK_H

#include "rc_icons_common.h"

/*
    Generated from clock.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconClock(RC_BoundingBox bounds,
                                   RC_Color color,
                                   const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawCircleStroke(bounds, 12.0f, 12.0f, 10.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 12.0f, 6.0f },
        { 12.0f, 12.0f },
        { 16.0f, 14.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       3,
                       viewBox, stroke, false, color);
}

static inline void rcIconClock(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconClock, NULL);
}

#endif /* RC_ICON_CLOCK_H */
