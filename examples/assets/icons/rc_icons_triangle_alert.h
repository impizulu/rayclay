#ifndef RC_ICON_TRIANGLE_ALERT_H
#define RC_ICON_TRIANGLE_ALERT_H

#include "rc_icons_common.h"

/*
    Generated from triangle-alert.svg (Lucide). The layout pass owns placement;
    the library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconTriangleAlert(RC_BoundingBox bounds,
                                           RC_Color color,
                                           const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 21.73f, 18.0f },
        { 13.73f, 4.0f },
        { 13.595f, 3.792f },
        { 13.434f, 3.603f },
        { 13.252f, 3.434f },
        { 13.05f, 3.29f },
        { 12.832f, 3.172f },
        { 12.601f, 3.082f },
        { 12.36f, 3.021f },
        { 12.114f, 2.99f },
        { 11.866f, 2.99f },
        { 11.62f, 3.021f },
        { 11.379f, 3.082f },
        { 11.148f, 3.172f },
        { 10.93f, 3.29f },
        { 10.728f, 3.434f },
        { 10.546f, 3.603f },
        { 10.385f, 3.792f },
        { 10.25f, 4.0f },
        { 2.25f, 18.0f },
        { 2.14f, 18.221f },
        { 2.058f, 18.455f },
        { 2.005f, 18.696f },
        { 1.983f, 18.943f },
        { 1.991f, 19.19f },
        { 2.03f, 19.434f },
        { 2.098f, 19.672f },
        { 2.195f, 19.899f },
        { 2.32f, 20.113f },
        { 2.47f, 20.309f },
        { 2.643f, 20.486f },
        { 2.837f, 20.64f },
        { 3.048f, 20.769f },
        { 3.273f, 20.87f },
        { 3.51f, 20.943f },
        { 3.753f, 20.987f },
        { 4.0f, 21.0f },
        { 20.0f, 21.0f },
        { 20.261f, 20.983f },
        { 20.517f, 20.931f },
        { 20.765f, 20.847f },
        { 20.999f, 20.731f },
        { 21.217f, 20.586f },
        { 21.413f, 20.413f },
        { 21.585f, 20.217f },
        { 21.731f, 19.999f },
        { 21.846f, 19.765f },
        { 21.93f, 19.517f },
        { 21.981f, 19.26f },
        { 21.998f, 18.999f },
        { 21.981f, 18.739f },
        { 21.93f, 18.482f },
        { 21.846f, 18.235f },
        { 21.73f, 18.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       54,
                       viewBox, stroke, false, color);

    rcIconDrawRoundLine(bounds, 12.0f, 9.0f, 12.0f, 13.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 12.0f, 17.0f, 12.01f, 17.0f, viewBox, stroke, color);
}

static inline void rcIconTriangleAlert(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconTriangleAlert, NULL);
}

#endif /* RC_ICON_TRIANGLE_ALERT_H */
