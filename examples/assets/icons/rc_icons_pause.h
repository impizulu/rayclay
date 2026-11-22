#ifndef RC_ICON_PAUSE_H
#define RC_ICON_PAUSE_H

#include "rc_icons_common.h"

/*
    Generated from pause.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconPause(RC_BoundingBox bounds,
                                   RC_Color color,
                                   const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundedRectStroke(bounds, 14.0f, 3.0f, 5.0f, 18.0f,
                                1.0f, viewBox, stroke, color);
    rcIconDrawRoundedRectStroke(bounds, 5.0f, 3.0f, 5.0f, 18.0f,
                                1.0f, viewBox, stroke, color);
}

static inline void rcIconPause(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconPause, NULL);
}

#endif /* RC_ICON_PAUSE_H */
