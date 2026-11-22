#ifndef RC_ICON_USER_PLUS_H
#define RC_ICON_USER_PLUS_H

#include "rc_icons_common.h"

/*
    Generated from user-plus.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconUserPlus(RC_BoundingBox bounds,
                                      RC_Color color,
                                      const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 16.0f, 21.0f },
        { 16.0f, 19.0f },
        { 15.966f, 18.478f },
        { 15.864f, 17.965f },
        { 15.696f, 17.469f },
        { 15.464f, 17.0f },
        { 15.173f, 16.565f },
        { 14.828f, 16.172f },
        { 14.435f, 15.827f },
        { 14.0f, 15.536f },
        { 13.531f, 15.304f },
        { 13.035f, 15.136f },
        { 12.522f, 15.034f },
        { 12.0f, 15.0f },
        { 6.0f, 15.0f },
        { 5.478f, 15.034f },
        { 4.965f, 15.136f },
        { 4.469f, 15.304f },
        { 4.0f, 15.536f },
        { 3.565f, 15.827f },
        { 3.172f, 16.172f },
        { 2.827f, 16.565f },
        { 2.536f, 17.0f },
        { 2.304f, 17.469f },
        { 2.136f, 17.965f },
        { 2.034f, 18.478f },
        { 2.0f, 19.0f },
        { 2.0f, 21.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       28,
                       viewBox, stroke, false, color);

    rcIconDrawCircleStroke(bounds, 9.0f, 7.0f, 4.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 19.0f, 8.0f, 19.0f, 14.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 22.0f, 11.0f, 16.0f, 11.0f, viewBox, stroke, color);
}

static inline void rcIconUserPlus(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconUserPlus, NULL);
}

#endif /* RC_ICON_USER_PLUS_H */
