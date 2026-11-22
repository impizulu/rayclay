#ifndef RC_ICON_ARROW_DOWN_H
#define RC_ICON_ARROW_DOWN_H

#include "rc_icons_common.h"

/*
    Generated from arrow-down.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconArrowDown(RC_BoundingBox bounds,
                                       RC_Color color,
                                       const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 12.0f, 5.0f, 12.0f, 19.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 19.0f, 12.0f },
        { 12.0f, 19.0f },
        { 5.0f, 12.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       3,
                       viewBox, stroke, false, color);
}

static inline void rcIconArrowDown(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconArrowDown, NULL);
}

#endif /* RC_ICON_ARROW_DOWN_H */
