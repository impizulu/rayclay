#ifndef RC_ICON_MESSAGE_SQUARE_H
#define RC_ICON_MESSAGE_SQUARE_H

#include "rc_icons_common.h"

/*
    Generated from message-square.svg (Lucide). The layout pass owns placement;
    the library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconMessageSquare(RC_BoundingBox bounds,
                                           RC_Color color,
                                           const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 22.0f, 17.0f },
        { 21.983f, 17.261f },
        { 21.932f, 17.518f },
        { 21.848f, 17.765f },
        { 21.732f, 18.0f },
        { 21.587f, 18.218f },
        { 21.414f, 18.414f },
        { 21.218f, 18.587f },
        { 21.0f, 18.732f },
        { 20.765f, 18.848f },
        { 20.518f, 18.932f },
        { 20.261f, 18.983f },
        { 20.0f, 19.0f },
        { 6.828f, 19.0f },
        { 6.567f, 19.017f },
        { 6.31f, 19.068f },
        { 6.063f, 19.152f },
        { 5.828f, 19.268f },
        { 5.611f, 19.413f },
        { 5.414f, 19.586f },
        { 3.212f, 21.788f },
        { 3.142f, 21.849f },
        { 3.065f, 21.901f },
        { 2.982f, 21.942f },
        { 2.894f, 21.972f },
        { 2.803f, 21.99f },
        { 2.71f, 21.996f },
        { 2.617f, 21.99f },
        { 2.526f, 21.972f },
        { 2.438f, 21.942f },
        { 2.355f, 21.901f },
        { 2.278f, 21.849f },
        { 2.208f, 21.788f },
        { 2.147f, 21.718f },
        { 2.095f, 21.641f },
        { 2.054f, 21.558f },
        { 2.024f, 21.47f },
        { 2.006f, 21.379f },
        { 2.0f, 21.286f },
        { 2.0f, 5.0f },
        { 2.017f, 4.739f },
        { 2.068f, 4.482f },
        { 2.152f, 4.235f },
        { 2.268f, 4.0f },
        { 2.413f, 3.782f },
        { 2.586f, 3.586f },
        { 2.782f, 3.413f },
        { 3.0f, 3.268f },
        { 3.235f, 3.152f },
        { 3.482f, 3.068f },
        { 3.739f, 3.017f },
        { 4.0f, 3.0f },
        { 20.0f, 3.0f },
        { 20.261f, 3.017f },
        { 20.518f, 3.068f },
        { 20.765f, 3.152f },
        { 21.0f, 3.268f },
        { 21.218f, 3.413f },
        { 21.414f, 3.586f },
        { 21.587f, 3.782f },
        { 21.732f, 4.0f },
        { 21.848f, 4.235f },
        { 21.932f, 4.482f },
        { 21.983f, 4.739f },
        { 22.0f, 5.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       65,
                       viewBox, stroke, true, color);
}

static inline void rcIconMessageSquare(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconMessageSquare, NULL);
}

#endif /* RC_ICON_MESSAGE_SQUARE_H */
