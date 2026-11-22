#ifndef RC_ICON_BELL_SOLID_H
#define RC_ICON_BELL_SOLID_H

#include "rc_icons_common.h"

/*
    Generated from bell-solid.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconBellSolid(RC_BoundingBox bounds,
                                       RC_Color color,
                                       const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;

    static const RC_IconPoint path0[] = {
        { 4.5f, 17.5f },
        { 6.0f, 16.0f },
        { 6.0f, 11.0f },
        { 6.2f, 9.45f },
        { 6.8f, 8.0f },
        { 7.76f, 6.76f },
        { 9.0f, 5.8f },
        { 10.45f, 5.2f },
        { 12.0f, 5.0f },
        { 13.55f, 5.2f },
        { 15.0f, 5.8f },
        { 16.24f, 6.76f },
        { 17.2f, 8.0f },
        { 17.8f, 9.45f },
        { 18.0f, 11.0f },
        { 18.0f, 16.0f },
        { 19.5f, 17.5f }
    };
    rcIconDrawFilledPolygon(bounds, path0,
                            17,
                            viewBox, color);

    rcIconDrawFilledCircle(bounds, 12.0f, 19.8f, 2.0f, viewBox, color);
    rcIconDrawFilledCircle(bounds, 12.0f, 3.9f, 1.3f, viewBox, color);
}

static inline void rcIconBellSolid(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconBellSolid, NULL);
}

#endif /* RC_ICON_BELL_SOLID_H */
