#ifndef RC_ICON_CODE_H
#define RC_ICON_CODE_H

#include "rc_icons_common.h"

/*
    Generated from code.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconCode(RC_BoundingBox bounds,
                                  RC_Color color,
                                  const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 16.0f, 18.0f },
        { 22.0f, 12.0f },
        { 16.0f, 6.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       3,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 8.0f, 6.0f },
        { 2.0f, 12.0f },
        { 8.0f, 18.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       3,
                       viewBox, stroke, false, color);
}

static inline void rcIconCode(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconCode, NULL);
}

#endif /* RC_ICON_CODE_H */
