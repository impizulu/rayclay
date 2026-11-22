#ifndef RC_ICON_CHEVRON_DOWN_H
#define RC_ICON_CHEVRON_DOWN_H

#include "rc_icons_common.h"

/*
    Generated from chevron-down.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconChevronDown(RC_BoundingBox bounds,
                                         RC_Color color,
                                         const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 6.0f, 9.0f },
        { 12.0f, 15.0f },
        { 18.0f, 9.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       3,
                       viewBox, stroke, false, color);
}

static inline void rcIconChevronDown(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconChevronDown, NULL);
}

#endif /* RC_ICON_CHEVRON_DOWN_H */
