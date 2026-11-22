#ifndef RC_ICON_STAR_H
#define RC_ICON_STAR_H

#include "rc_icons_common.h"

/*
    Generated from star.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconStar(RC_BoundingBox bounds,
                                  RC_Color color,
                                  const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 11.525f, 2.295f },
        { 12.475f, 2.295f },
        { 14.785f, 6.974f },
        { 14.912f, 7.196f },
        { 15.065f, 7.402f },
        { 15.242f, 7.588f },
        { 15.44f, 7.75f },
        { 15.656f, 7.888f },
        { 15.887f, 7.999f },
        { 16.129f, 8.082f },
        { 16.38f, 8.134f },
        { 21.546f, 8.89f },
        { 21.84f, 9.794f },
        { 18.104f, 13.432f },
        { 17.931f, 13.622f },
        { 17.783f, 13.831f },
        { 17.661f, 14.057f },
        { 17.567f, 14.296f },
        { 17.502f, 14.544f },
        { 17.468f, 14.798f },
        { 17.465f, 15.055f },
        { 17.493f, 15.31f },
        { 18.375f, 20.45f },
        { 17.604f, 21.01f },
        { 12.986f, 18.582f },
        { 12.752f, 18.477f },
        { 12.508f, 18.4f },
        { 12.255f, 18.354f },
        { 11.999f, 18.339f },
        { 11.744f, 18.354f },
        { 11.491f, 18.4f },
        { 11.247f, 18.477f },
        { 11.013f, 18.582f },
        { 6.396f, 21.01f },
        { 5.626f, 20.45f },
        { 6.507f, 15.311f },
        { 6.535f, 15.056f },
        { 6.532f, 14.799f },
        { 6.498f, 14.545f },
        { 6.434f, 14.296f },
        { 6.34f, 14.057f },
        { 6.217f, 13.831f },
        { 6.069f, 13.622f },
        { 5.896f, 13.432f },
        { 2.16f, 9.795f },
        { 2.454f, 8.889f },
        { 7.619f, 8.134f },
        { 7.87f, 8.082f },
        { 8.113f, 8.0f },
        { 8.344f, 7.889f },
        { 8.561f, 7.751f },
        { 8.759f, 7.588f },
        { 8.936f, 7.402f },
        { 9.089f, 7.197f },
        { 9.216f, 6.974f }
    };
    rcIconDrawPolyline(bounds, path0,
                       55,
                       viewBox, stroke, true, color);
}

static inline void rcIconStar(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconStar, NULL);
}

#endif /* RC_ICON_STAR_H */
