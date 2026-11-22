#ifndef RC_ICON_SEPARATOR_HORIZONTAL_H
#define RC_ICON_SEPARATOR_HORIZONTAL_H

#include "rc_icons_common.h"

/*
    Generated from separator-horizontal.svg (Lucide). The layout pass owns
    placement; the library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconSeparatorHorizontal(RC_BoundingBox bounds,
                                                 RC_Color color,
                                                 const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 16.0f, 16.0f },
        { 12.0f, 20.0f },
        { 8.0f, 16.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       3,
                       viewBox, stroke, false, color);

    rcIconDrawRoundLine(bounds, 3.0f, 12.0f, 21.0f, 12.0f, viewBox, stroke, color);

    static const RC_IconPoint path1[] = {
        { 8.0f, 8.0f },
        { 12.0f, 4.0f },
        { 16.0f, 8.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       3,
                       viewBox, stroke, false, color);
}

static inline void rcIconSeparatorHorizontal(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconSeparatorHorizontal, NULL);
}

#endif /* RC_ICON_SEPARATOR_HORIZONTAL_H */
