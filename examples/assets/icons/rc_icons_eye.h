#ifndef RC_ICON_EYE_H
#define RC_ICON_EYE_H

#include "rc_icons_common.h"

/*
    Generated from eye.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconEye(RC_BoundingBox bounds,
                                 RC_Color color,
                                 const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 2.062f, 12.348f },
        { 2.027f, 12.235f },
        { 2.007f, 12.118f },
        { 1.999f, 12.0f },
        { 2.007f, 11.882f },
        { 2.027f, 11.765f },
        { 2.062f, 11.652f },
        { 2.646f, 10.453f },
        { 3.374f, 9.335f },
        { 4.235f, 8.316f },
        { 5.216f, 7.412f },
        { 6.301f, 6.636f },
        { 7.474f, 6.0f },
        { 8.716f, 5.514f },
        { 10.009f, 5.187f },
        { 11.333f, 5.021f },
        { 12.667f, 5.021f },
        { 13.991f, 5.187f },
        { 15.284f, 5.514f },
        { 16.526f, 6.0f },
        { 17.699f, 6.636f },
        { 18.784f, 7.412f },
        { 19.765f, 8.316f },
        { 20.626f, 9.335f },
        { 21.354f, 10.453f },
        { 21.938f, 11.652f },
        { 21.973f, 11.765f },
        { 21.993f, 11.882f },
        { 22.001f, 12.0f },
        { 21.993f, 12.118f },
        { 21.973f, 12.235f },
        { 21.938f, 12.348f },
        { 21.354f, 13.547f },
        { 20.626f, 14.665f },
        { 19.765f, 15.684f },
        { 18.784f, 16.588f },
        { 17.699f, 17.364f },
        { 16.526f, 18.0f },
        { 15.284f, 18.486f },
        { 13.991f, 18.813f },
        { 12.667f, 18.979f },
        { 11.333f, 18.979f },
        { 10.009f, 18.813f },
        { 8.716f, 18.486f },
        { 7.474f, 18.0f },
        { 6.301f, 17.364f },
        { 5.216f, 16.588f },
        { 4.235f, 15.684f },
        { 3.374f, 14.665f },
        { 2.646f, 13.547f },
        { 2.062f, 12.348f }
    };
    rcIconDrawPolyline(bounds, path0,
                       51,
                       viewBox, stroke, false, color);

    rcIconDrawCircleStroke(bounds, 12.0f, 12.0f, 3.0f, viewBox, stroke, color);
}

static inline void rcIconEye(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconEye, NULL);
}

#endif /* RC_ICON_EYE_H */
