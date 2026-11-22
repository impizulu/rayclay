#ifndef RC_ICON_ARROW_UP_DOWN_H
#define RC_ICON_ARROW_UP_DOWN_H

#include "rc_icons_common.h"

/*
    Generated from arrow-up-down.svg (Lucide). The layout pass owns placement;
    the library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconArrowUpDown(RC_BoundingBox bounds,
                                         RC_Color color,
                                         const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 21.0f, 16.0f },
        { 17.0f, 20.0f },
        { 13.0f, 16.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       3,
                       viewBox, stroke, false, color);

    rcIconDrawRoundLine(bounds, 17.0f, 20.0f, 17.0f, 4.0f, viewBox, stroke, color);

    static const RC_IconPoint path1[] = {
        { 3.0f, 8.0f },
        { 7.0f, 4.0f },
        { 11.0f, 8.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       3,
                       viewBox, stroke, false, color);

    rcIconDrawRoundLine(bounds, 7.0f, 4.0f, 7.0f, 20.0f, viewBox, stroke, color);
}

static inline void rcIconArrowUpDown(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconArrowUpDown, NULL);
}

#endif /* RC_ICON_ARROW_UP_DOWN_H */
