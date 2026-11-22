#ifndef RC_ICON_ROTATE_CW_H
#define RC_ICON_ROTATE_CW_H

#include "rc_icons_common.h"

/*
    Generated from rotate-cw.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconRotateCw(RC_BoundingBox bounds,
                                      RC_Color color,
                                      const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 21.0f, 12.0f },
        { 20.923f, 13.175f },
        { 20.693f, 14.329f },
        { 20.315f, 15.444f },
        { 19.794f, 16.5f },
        { 19.14f, 17.479f },
        { 18.364f, 18.364f },
        { 17.479f, 19.14f },
        { 16.5f, 19.794f },
        { 15.444f, 20.315f },
        { 14.329f, 20.693f },
        { 13.175f, 20.923f },
        { 12.0f, 21.0f },
        { 10.825f, 20.923f },
        { 9.671f, 20.693f },
        { 8.556f, 20.315f },
        { 7.5f, 19.794f },
        { 6.521f, 19.14f },
        { 5.636f, 18.364f },
        { 4.86f, 17.479f },
        { 4.206f, 16.5f },
        { 3.685f, 15.444f },
        { 3.307f, 14.329f },
        { 3.077f, 13.175f },
        { 3.0f, 12.0f },
        { 3.077f, 10.825f },
        { 3.307f, 9.671f },
        { 3.685f, 8.556f },
        { 4.206f, 7.5f },
        { 4.86f, 6.521f },
        { 5.636f, 5.636f },
        { 6.521f, 4.86f },
        { 7.5f, 4.206f },
        { 8.556f, 3.685f },
        { 9.671f, 3.307f },
        { 10.825f, 3.077f },
        { 12.0f, 3.0f },
        { 12.471f, 3.012f },
        { 12.939f, 3.046f },
        { 13.403f, 3.104f },
        { 13.862f, 3.183f },
        { 14.315f, 3.285f },
        { 14.763f, 3.408f },
        { 15.203f, 3.552f },
        { 15.636f, 3.718f },
        { 16.061f, 3.903f },
        { 16.476f, 4.108f },
        { 16.882f, 4.333f },
        { 17.278f, 4.578f },
        { 17.662f, 4.841f },
        { 18.034f, 5.123f },
        { 18.394f, 5.422f },
        { 18.74f, 5.74f },
        { 21.0f, 8.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       54,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 21.0f, 3.0f },
        { 21.0f, 8.0f },
        { 16.0f, 8.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       3,
                       viewBox, stroke, false, color);
}

static inline void rcIconRotateCw(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconRotateCw, NULL);
}

#endif /* RC_ICON_ROTATE_CW_H */
