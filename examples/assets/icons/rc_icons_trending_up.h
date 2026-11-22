#ifndef RC_ICON_TRENDING_UP_H
#define RC_ICON_TRENDING_UP_H

#include "rc_icons_common.h"

/*
    Generated from trending-up.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconTrendingUp(RC_BoundingBox bounds,
                                        RC_Color color,
                                        const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 16.0f, 7.0f },
        { 22.0f, 7.0f },
        { 22.0f, 13.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       3,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 22.0f, 7.0f },
        { 13.5f, 15.5f },
        { 8.5f, 10.5f },
        { 2.0f, 17.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       4,
                       viewBox, stroke, false, color);
}

static inline void rcIconTrendingUp(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconTrendingUp, NULL);
}

#endif /* RC_ICON_TRENDING_UP_H */
