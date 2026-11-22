#ifndef RC_ICON_SQUARE_FUNCTION_H
#define RC_ICON_SQUARE_FUNCTION_H

#include "rc_icons_common.h"

/*
    Generated from square-function.svg (Lucide). The layout pass owns placement;
    the library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconSquareFunction(RC_BoundingBox bounds,
                                            RC_Color color,
                                            const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundedRectStroke(bounds, 3.0f, 3.0f, 18.0f, 18.0f,
                                2.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 9.0f, 17.0f },
        { 9.361f, 16.988f },
        { 9.695f, 16.954f },
        { 10.001f, 16.896f },
        { 10.281f, 16.816f },
        { 10.536f, 16.713f },
        { 10.765f, 16.589f },
        { 10.969f, 16.443f },
        { 11.15f, 16.275f },
        { 11.307f, 16.086f },
        { 11.441f, 15.877f },
        { 11.553f, 15.647f },
        { 11.644f, 15.397f },
        { 11.713f, 15.127f },
        { 11.762f, 14.837f },
        { 11.791f, 14.528f },
        { 11.8f, 14.2f },
        { 11.8f, 10.0f },
        { 11.812f, 9.633f },
        { 11.847f, 9.285f },
        { 11.907f, 8.955f },
        { 11.991f, 8.645f },
        { 12.099f, 8.358f },
        { 12.232f, 8.093f },
        { 12.391f, 7.852f },
        { 12.575f, 7.638f },
        { 12.785f, 7.45f },
        { 13.021f, 7.29f },
        { 13.283f, 7.16f },
        { 13.572f, 7.061f },
        { 13.888f, 6.994f },
        { 14.231f, 6.961f },
        { 14.602f, 6.962f },
        { 15.0f, 7.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       34,
                       viewBox, stroke, false, color);

    rcIconDrawRoundLine(bounds, 9.0f, 11.2f, 14.7f, 11.2f, viewBox, stroke, color);
}

static inline void rcIconSquareFunction(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconSquareFunction, NULL);
}

#endif /* RC_ICON_SQUARE_FUNCTION_H */
