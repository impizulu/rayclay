#ifndef RC_ICON_PLAY_H
#define RC_ICON_PLAY_H

#include "rc_icons_common.h"

/*
    Generated from play.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconPlay(RC_BoundingBox bounds,
                                  RC_Color color,
                                  const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 5.0f, 5.0f },
        { 5.015f, 4.754f },
        { 5.061f, 4.511f },
        { 5.135f, 4.276f },
        { 5.239f, 4.052f },
        { 5.369f, 3.842f },
        { 5.524f, 3.65f },
        { 5.701f, 3.478f },
        { 5.899f, 3.33f },
        { 6.113f, 3.207f },
        { 6.34f, 3.111f },
        { 6.578f, 3.044f },
        { 6.822f, 3.007f },
        { 7.069f, 3.001f },
        { 7.314f, 3.024f },
        { 7.555f, 3.078f },
        { 7.788f, 3.161f },
        { 8.008f, 3.272f },
        { 20.005f, 10.27f },
        { 20.222f, 10.415f },
        { 20.417f, 10.588f },
        { 20.589f, 10.784f },
        { 20.734f, 11.001f },
        { 20.849f, 11.235f },
        { 20.933f, 11.482f },
        { 20.984f, 11.738f },
        { 21.001f, 11.998f },
        { 20.984f, 12.258f },
        { 20.934f, 12.514f },
        { 20.851f, 12.762f },
        { 20.736f, 12.996f },
        { 20.591f, 13.213f },
        { 20.42f, 13.41f },
        { 20.224f, 13.582f },
        { 20.008f, 13.728f },
        { 8.008f, 20.728f },
        { 7.788f, 20.839f },
        { 7.555f, 20.922f },
        { 7.314f, 20.976f },
        { 7.069f, 20.999f },
        { 6.822f, 20.993f },
        { 6.578f, 20.956f },
        { 6.34f, 20.889f },
        { 6.113f, 20.793f },
        { 5.899f, 20.67f },
        { 5.701f, 20.522f },
        { 5.524f, 20.35f },
        { 5.369f, 20.158f },
        { 5.239f, 19.948f },
        { 5.135f, 19.724f },
        { 5.061f, 19.489f },
        { 5.015f, 19.246f },
        { 5.0f, 19.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       (int)(sizeof(path0) / sizeof(path0[0])),
                       viewBox, stroke, true, color);
}

static inline void rcIconPlay(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconPlay, NULL);
}

#endif /* RC_ICON_PLAY_H */
