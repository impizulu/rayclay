#ifndef RC_ICON_SEND_H
#define RC_ICON_SEND_H

#include "rc_icons_common.h"

/*
    Generated from send.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconSend(RC_BoundingBox bounds,
                                  RC_Color color,
                                  const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 14.536f, 21.686f },
        { 14.564f, 21.744f },
        { 14.598f, 21.797f },
        { 14.639f, 21.846f },
        { 14.687f, 21.889f },
        { 14.739f, 21.926f },
        { 14.795f, 21.956f },
        { 14.855f, 21.978f },
        { 14.917f, 21.993f },
        { 14.981f, 21.999f },
        { 15.045f, 21.998f },
        { 15.108f, 21.988f },
        { 15.169f, 21.97f },
        { 15.228f, 21.945f },
        { 15.283f, 21.912f },
        { 15.333f, 21.873f },
        { 15.378f, 21.827f },
        { 15.417f, 21.776f },
        { 15.448f, 21.721f },
        { 15.473f, 21.662f },
        { 21.973f, 2.662f },
        { 21.99f, 2.602f },
        { 22.0f, 2.541f },
        { 22.002f, 2.478f },
        { 21.996f, 2.416f },
        { 21.983f, 2.355f },
        { 21.962f, 2.297f },
        { 21.933f, 2.241f },
        { 21.898f, 2.19f },
        { 21.857f, 2.143f },
        { 21.81f, 2.102f },
        { 21.759f, 2.067f },
        { 21.703f, 2.038f },
        { 21.645f, 2.017f },
        { 21.584f, 2.004f },
        { 21.522f, 1.998f },
        { 21.459f, 2.0f },
        { 21.398f, 2.01f },
        { 21.338f, 2.027f },
        { 2.338f, 8.527f },
        { 2.279f, 8.552f },
        { 2.224f, 8.583f },
        { 2.173f, 8.622f },
        { 2.127f, 8.667f },
        { 2.088f, 8.717f },
        { 2.055f, 8.772f },
        { 2.03f, 8.831f },
        { 2.012f, 8.892f },
        { 2.002f, 8.955f },
        { 2.001f, 9.019f },
        { 2.007f, 9.083f },
        { 2.022f, 9.145f },
        { 2.044f, 9.205f },
        { 2.074f, 9.261f },
        { 2.111f, 9.313f },
        { 2.154f, 9.361f },
        { 2.203f, 9.402f },
        { 2.256f, 9.436f },
        { 2.314f, 9.464f },
        { 10.244f, 12.644f },
        { 10.453f, 12.742f },
        { 10.649f, 12.863f },
        { 10.83f, 13.006f },
        { 10.993f, 13.169f },
        { 11.136f, 13.35f },
        { 11.258f, 13.545f },
        { 11.356f, 13.754f }
    };
    rcIconDrawPolyline(bounds, path0,
                       67,
                       viewBox, stroke, true, color);

    rcIconDrawRoundLine(bounds, 21.854f, 2.147f, 10.914f, 13.086f, viewBox, stroke, color);
}

static inline void rcIconSend(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconSend, NULL);
}

#endif /* RC_ICON_SEND_H */
