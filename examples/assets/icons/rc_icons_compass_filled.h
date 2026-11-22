#ifndef RC_ICON_COMPASS_FILLED_H
#define RC_ICON_COMPASS_FILLED_H

#include "rc_icons_common.h"

/*
    Generated from compass-filled.svg (Lucide). The layout pass owns placement;
    the library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconCompassFilled(RC_BoundingBox bounds,
                                           RC_Color color,
                                           const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 16.24f, 7.76f },
        { 14.436f, 13.171f },
        { 14.35f, 13.386f },
        { 14.24f, 13.59f },
        { 14.107f, 13.78f },
        { 13.953f, 13.953f },
        { 13.78f, 14.107f },
        { 13.59f, 14.24f },
        { 13.386f, 14.35f },
        { 13.171f, 14.436f },
        { 7.76f, 16.24f },
        { 9.564f, 10.829f },
        { 9.65f, 10.614f },
        { 9.76f, 10.41f },
        { 9.893f, 10.22f },
        { 10.047f, 10.047f },
        { 10.22f, 9.893f },
        { 10.41f, 9.76f },
        { 10.614f, 9.65f },
        { 10.829f, 9.564f }
    };
    rcIconDrawFilledPolygon(bounds, path0,
                            20,
                            viewBox, color);

    static const RC_IconPoint path1[] = {
        { 16.24f, 7.76f },
        { 14.436f, 13.171f },
        { 14.35f, 13.386f },
        { 14.24f, 13.59f },
        { 14.107f, 13.78f },
        { 13.953f, 13.953f },
        { 13.78f, 14.107f },
        { 13.59f, 14.24f },
        { 13.386f, 14.35f },
        { 13.171f, 14.436f },
        { 7.76f, 16.24f },
        { 9.564f, 10.829f },
        { 9.65f, 10.614f },
        { 9.76f, 10.41f },
        { 9.893f, 10.22f },
        { 10.047f, 10.047f },
        { 10.22f, 9.893f },
        { 10.41f, 9.76f },
        { 10.614f, 9.65f },
        { 10.829f, 9.564f }
    };
    rcIconDrawPolyline(bounds, path1,
                       20,
                       viewBox, stroke, true, color);

    rcIconDrawCircleStroke(bounds, 12.0f, 12.0f, 10.0f, viewBox, stroke, color);
}

static inline void rcIconCompassFilled(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconCompassFilled, NULL);
}

#endif /* RC_ICON_COMPASS_FILLED_H */
