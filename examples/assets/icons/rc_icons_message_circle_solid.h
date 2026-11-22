#ifndef RC_ICON_MESSAGE_CIRCLE_SOLID_H
#define RC_ICON_MESSAGE_CIRCLE_SOLID_H

#include "rc_icons_common.h"

/*
    Generated from message-circle-solid.svg (Lucide). The layout pass owns
    placement; the library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconMessageCircleSolid(RC_BoundingBox bounds,
                                                RC_Color color,
                                                const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;

    static const RC_IconPoint path0[] = {
        { 2.992f, 16.342f },
        { 3.066f, 16.568f },
        { 3.113f, 16.8f },
        { 3.132f, 17.037f },
        { 3.123f, 17.274f },
        { 3.086f, 17.509f },
        { 2.021f, 20.799f },
        { 2.003f, 20.925f },
        { 2.002f, 21.052f },
        { 2.016f, 21.178f },
        { 2.047f, 21.301f },
        { 2.093f, 21.42f },
        { 2.153f, 21.531f },
        { 2.227f, 21.634f },
        { 2.314f, 21.727f },
        { 2.411f, 21.808f },
        { 2.518f, 21.877f },
        { 2.633f, 21.931f },
        { 2.754f, 21.97f },
        { 2.879f, 21.993f },
        { 3.006f, 22.0f },
        { 3.132f, 21.992f },
        { 3.257f, 21.967f },
        { 6.67f, 20.969f },
        { 6.891f, 20.938f },
        { 7.114f, 20.932f },
        { 7.337f, 20.95f },
        { 7.556f, 20.994f },
        { 7.769f, 21.061f },
        { 8.981f, 21.534f },
        { 10.244f, 21.845f },
        { 11.537f, 21.99f },
        { 12.838f, 21.965f },
        { 14.125f, 21.772f },
        { 15.375f, 21.414f },
        { 16.569f, 20.896f },
        { 17.685f, 20.227f },
        { 18.705f, 19.42f },
        { 19.612f, 18.486f },
        { 20.389f, 17.443f },
        { 21.025f, 16.308f },
        { 21.508f, 15.1f },
        { 21.83f, 13.839f },
        { 21.985f, 12.548f },
        { 21.972f, 11.247f },
        { 21.79f, 9.959f },
        { 21.442f, 8.705f },
        { 20.934f, 7.507f },
        { 20.275f, 6.385f },
        { 19.476f, 5.358f },
        { 18.55f, 4.444f },
        { 17.514f, 3.658f },
        { 16.384f, 3.012f },
        { 15.18f, 2.519f },
        { 13.922f, 2.187f },
        { 12.632f, 2.02f },
        { 11.331f, 2.023f },
        { 10.041f, 2.194f },
        { 8.785f, 2.531f },
        { 7.583f, 3.029f },
        { 6.455f, 3.678f },
        { 5.422f, 4.469f },
        { 4.5f, 5.387f },
        { 3.705f, 6.416f },
        { 3.05f, 7.541f },
        { 2.547f, 8.74f },
        { 2.203f, 9.995f },
        { 2.026f, 11.284f },
        { 2.017f, 12.585f },
        { 2.178f, 13.876f },
        { 2.505f, 15.136f },
        { 2.992f, 16.342f }
    };
    rcIconDrawFilledPolygon(bounds, path0,
                            72,
                            viewBox, color);
}

static inline void rcIconMessageCircleSolid(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconMessageCircleSolid, NULL);
}

#endif /* RC_ICON_MESSAGE_CIRCLE_SOLID_H */
