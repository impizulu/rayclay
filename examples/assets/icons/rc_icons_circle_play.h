#ifndef RC_ICON_CIRCLE_PLAY_H
#define RC_ICON_CIRCLE_PLAY_H

#include "rc_icons_common.h"

/*
    Generated from circle-play.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconCirclePlay(RC_BoundingBox bounds,
                                        RC_Color color,
                                        const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 9.0f, 9.003f },
        { 9.007f, 8.879f },
        { 9.03f, 8.756f },
        { 9.068f, 8.638f },
        { 9.12f, 8.525f },
        { 9.186f, 8.419f },
        { 9.264f, 8.323f },
        { 9.354f, 8.236f },
        { 9.454f, 8.162f },
        { 9.562f, 8.101f },
        { 9.677f, 8.053f },
        { 9.798f, 8.021f },
        { 9.921f, 8.003f },
        { 10.045f, 8.001f },
        { 10.169f, 8.014f },
        { 10.29f, 8.043f },
        { 10.407f, 8.086f },
        { 10.517f, 8.144f },
        { 15.514f, 11.141f },
        { 15.62f, 11.214f },
        { 15.716f, 11.3f },
        { 15.801f, 11.398f },
        { 15.871f, 11.506f },
        { 15.928f, 11.622f },
        { 15.969f, 11.745f },
        { 15.994f, 11.871f },
        { 16.002f, 12.0f },
        { 15.994f, 12.129f },
        { 15.969f, 12.255f },
        { 15.928f, 12.378f },
        { 15.871f, 12.494f },
        { 15.801f, 12.602f },
        { 15.716f, 12.7f },
        { 15.62f, 12.786f },
        { 15.514f, 12.859f },
        { 10.517f, 15.856f },
        { 10.407f, 15.914f },
        { 10.29f, 15.957f },
        { 10.169f, 15.986f },
        { 10.045f, 15.999f },
        { 9.92f, 15.997f },
        { 9.797f, 15.979f },
        { 9.677f, 15.946f },
        { 9.562f, 15.899f },
        { 9.454f, 15.838f },
        { 9.354f, 15.763f },
        { 9.264f, 15.677f },
        { 9.186f, 15.58f },
        { 9.12f, 15.474f },
        { 9.068f, 15.361f },
        { 9.03f, 15.243f },
        { 9.007f, 15.12f },
        { 9.0f, 14.996f }
    };
    rcIconDrawPolyline(bounds, path0,
                       53,
                       viewBox, stroke, true, color);

    rcIconDrawCircleStroke(bounds, 12.0f, 12.0f, 10.0f, viewBox, stroke, color);
}

static inline void rcIconCirclePlay(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconCirclePlay, NULL);
}

#endif /* RC_ICON_CIRCLE_PLAY_H */
