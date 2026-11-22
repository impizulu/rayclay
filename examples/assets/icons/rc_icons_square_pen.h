#ifndef RC_ICON_SQUARE_PEN_H
#define RC_ICON_SQUARE_PEN_H

#include "rc_icons_common.h"

/*
    Generated from square-pen.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconSquarePen(RC_BoundingBox bounds,
                                       RC_Color color,
                                       const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 12.0f, 3.0f },
        { 5.0f, 3.0f },
        { 4.739f, 3.017f },
        { 4.482f, 3.068f },
        { 4.235f, 3.152f },
        { 4.0f, 3.268f },
        { 3.782f, 3.413f },
        { 3.586f, 3.586f },
        { 3.413f, 3.782f },
        { 3.268f, 4.0f },
        { 3.152f, 4.235f },
        { 3.068f, 4.482f },
        { 3.017f, 4.739f },
        { 3.0f, 5.0f },
        { 3.0f, 19.0f },
        { 3.017f, 19.261f },
        { 3.068f, 19.518f },
        { 3.152f, 19.765f },
        { 3.268f, 20.0f },
        { 3.413f, 20.218f },
        { 3.586f, 20.414f },
        { 3.782f, 20.587f },
        { 4.0f, 20.732f },
        { 4.235f, 20.848f },
        { 4.482f, 20.932f },
        { 4.739f, 20.983f },
        { 5.0f, 21.0f },
        { 19.0f, 21.0f },
        { 19.261f, 20.983f },
        { 19.518f, 20.932f },
        { 19.765f, 20.848f },
        { 20.0f, 20.732f },
        { 20.218f, 20.587f },
        { 20.414f, 20.414f },
        { 20.587f, 20.218f },
        { 20.732f, 20.0f },
        { 20.848f, 19.765f },
        { 20.932f, 19.518f },
        { 20.983f, 19.261f },
        { 21.0f, 19.0f },
        { 21.0f, 12.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       41,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 18.375f, 2.625f },
        { 18.584f, 2.442f },
        { 18.814f, 2.288f },
        { 19.063f, 2.165f },
        { 19.326f, 2.076f },
        { 19.598f, 2.022f },
        { 19.875f, 2.004f },
        { 20.152f, 2.022f },
        { 20.424f, 2.076f },
        { 20.687f, 2.165f },
        { 20.936f, 2.288f },
        { 21.166f, 2.442f },
        { 21.375f, 2.625f },
        { 21.558f, 2.834f },
        { 21.712f, 3.064f },
        { 21.835f, 3.313f },
        { 21.924f, 3.576f },
        { 21.978f, 3.848f },
        { 21.996f, 4.125f },
        { 21.978f, 4.402f },
        { 21.924f, 4.674f },
        { 21.835f, 4.937f },
        { 21.712f, 5.186f },
        { 21.558f, 5.416f },
        { 21.375f, 5.625f },
        { 12.362f, 14.639f },
        { 12.174f, 14.804f },
        { 11.967f, 14.945f },
        { 11.744f, 15.059f },
        { 11.509f, 15.144f },
        { 8.636f, 15.984f },
        { 8.575f, 15.998f },
        { 8.512f, 16.004f },
        { 8.449f, 16.002f },
        { 8.387f, 15.992f },
        { 8.327f, 15.975f },
        { 8.269f, 15.95f },
        { 8.215f, 15.918f },
        { 8.165f, 15.879f },
        { 8.121f, 15.835f },
        { 8.082f, 15.785f },
        { 8.05f, 15.731f },
        { 8.025f, 15.673f },
        { 8.008f, 15.613f },
        { 7.998f, 15.551f },
        { 7.996f, 15.488f },
        { 8.002f, 15.425f },
        { 8.016f, 15.364f },
        { 8.856f, 12.491f },
        { 8.941f, 12.256f },
        { 9.055f, 12.033f },
        { 9.196f, 11.826f },
        { 9.362f, 11.639f }
    };
    rcIconDrawPolyline(bounds, path1,
                       53,
                       viewBox, stroke, true, color);
}

static inline void rcIconSquarePen(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconSquarePen, NULL);
}

#endif /* RC_ICON_SQUARE_PEN_H */
