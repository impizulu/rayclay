#ifndef RC_ICON_CHECK_CHECK_SOLID_H
#define RC_ICON_CHECK_CHECK_SOLID_H

#include "rc_icons_common.h"

/*
    Generated from check-check-solid.svg (Lucide). The layout pass owns
    placement; the library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconCheckCheckSolid(RC_BoundingBox bounds,
                                             RC_Color color,
                                             const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;

    static const RC_IconPoint path0[] = {
        { 1.33f, 13.17f },
        { 6.54f, 18.38f },
        { 15.71f, 8.13f },
        { 14.29f, 6.87f },
        { 6.46f, 15.62f },
        { 2.67f, 11.83f }
    };
    rcIconDrawFilledPolygon(bounds, path0,
                            6,
                            viewBox, color);

    static const RC_IconPoint path1[] = {
        { 9.93f, 16.57f },
        { 11.73f, 18.37f },
        { 21.2f, 8.15f },
        { 19.8f, 6.85f },
        { 11.67f, 15.63f },
        { 11.27f, 15.23f }
    };
    rcIconDrawFilledPolygon(bounds, path1,
                            6,
                            viewBox, color);
}

static inline void rcIconCheckCheckSolid(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconCheckCheckSolid, NULL);
}

#endif /* RC_ICON_CHECK_CHECK_SOLID_H */
