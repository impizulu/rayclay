#ifndef RC_ICON_CHEVRON_UP_H
#define RC_ICON_CHEVRON_UP_H

#include "rc_icons_common.h"

/*
    Generated from chevron-up.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconChevronUp(RC_BoundingBox bounds,
                                       RC_Color color,
                                       const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 18.0f, 15.0f },
        { 12.0f, 9.0f },
        { 6.0f, 15.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       3,
                       viewBox, stroke, false, color);
}

static inline void rcIconChevronUp(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconChevronUp, NULL);
}

#endif /* RC_ICON_CHEVRON_UP_H */
