#ifndef RC_ICON_CHEVRON_RIGHT_H
#define RC_ICON_CHEVRON_RIGHT_H

#include "rc_icons_common.h"

/*
    Generated from chevron-right.svg (Lucide). The layout pass owns placement;
    the library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconChevronRight(RC_BoundingBox bounds,
                                          RC_Color color,
                                          const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 9.0f, 18.0f },
        { 15.0f, 12.0f },
        { 9.0f, 6.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       (int)(sizeof(path0) / sizeof(path0[0])),
                       viewBox, stroke, false, color);
}

static inline void rcIconChevronRight(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconChevronRight, NULL);
}

#endif /* RC_ICON_CHEVRON_RIGHT_H */
