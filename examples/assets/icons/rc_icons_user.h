#ifndef RC_ICON_USER_H
#define RC_ICON_USER_H

#include "rc_icons_common.h"

/*
    Generated from user.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconUser(RC_BoundingBox bounds,
                                  RC_Color color,
                                  const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 19.0f, 21.0f },
        { 19.0f, 19.0f },
        { 18.966f, 18.478f },
        { 18.864f, 17.965f },
        { 18.696f, 17.469f },
        { 18.464f, 17.0f },
        { 18.173f, 16.565f },
        { 17.828f, 16.172f },
        { 17.435f, 15.827f },
        { 17.0f, 15.536f },
        { 16.531f, 15.304f },
        { 16.035f, 15.136f },
        { 15.522f, 15.034f },
        { 15.0f, 15.0f },
        { 9.0f, 15.0f },
        { 8.478f, 15.034f },
        { 7.965f, 15.136f },
        { 7.469f, 15.304f },
        { 7.0f, 15.536f },
        { 6.565f, 15.827f },
        { 6.172f, 16.172f },
        { 5.827f, 16.565f },
        { 5.536f, 17.0f },
        { 5.304f, 17.469f },
        { 5.136f, 17.965f },
        { 5.034f, 18.478f },
        { 5.0f, 19.0f },
        { 5.0f, 21.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       (int)(sizeof(path0) / sizeof(path0[0])),
                       viewBox, stroke, false, color);

    rcIconDrawCircleStroke(bounds, 12.0f, 7.0f, 4.0f, viewBox, stroke, color);
}

static inline void rcIconUser(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconUser, NULL);
}

#endif /* RC_ICON_USER_H */
