#ifndef RC_ICON_LINK_H
#define RC_ICON_LINK_H

#include "rc_icons_common.h"

/*
    Generated from link.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconLink(RC_BoundingBox bounds,
                                  RC_Color color,
                                  const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 10.0f, 13.0f },
        { 10.396f, 13.467f },
        { 10.846f, 13.881f },
        { 11.343f, 14.238f },
        { 11.879f, 14.531f },
        { 12.448f, 14.757f },
        { 13.04f, 14.911f },
        { 13.647f, 14.992f },
        { 14.258f, 14.999f },
        { 14.866f, 14.93f },
        { 15.461f, 14.788f },
        { 16.035f, 14.574f },
        { 16.578f, 14.292f },
        { 17.082f, 13.945f },
        { 17.54f, 13.54f },
        { 20.54f, 10.54f },
        { 20.958f, 10.047f },
        { 21.309f, 9.503f },
        { 21.588f, 8.919f },
        { 21.788f, 8.304f },
        { 21.907f, 7.669f },
        { 21.943f, 7.023f },
        { 21.896f, 6.378f },
        { 21.766f, 5.744f },
        { 21.555f, 5.133f },
        { 21.266f, 4.554f },
        { 20.906f, 4.017f },
        { 20.479f, 3.531f },
        { 19.993f, 3.104f },
        { 19.456f, 2.744f },
        { 18.877f, 2.455f },
        { 18.266f, 2.244f },
        { 17.632f, 2.114f },
        { 16.987f, 2.067f },
        { 16.341f, 2.103f },
        { 15.706f, 2.222f },
        { 15.091f, 2.422f },
        { 14.507f, 2.701f },
        { 13.963f, 3.052f },
        { 13.47f, 3.47f },
        { 11.75f, 5.18f }
    };
    rcIconDrawPolyline(bounds, path0,
                       41,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 14.0f, 11.0f },
        { 13.604f, 10.533f },
        { 13.154f, 10.119f },
        { 12.657f, 9.762f },
        { 12.121f, 9.469f },
        { 11.552f, 9.243f },
        { 10.96f, 9.089f },
        { 10.353f, 9.008f },
        { 9.742f, 9.001f },
        { 9.134f, 9.07f },
        { 8.539f, 9.212f },
        { 7.965f, 9.426f },
        { 7.422f, 9.708f },
        { 6.918f, 10.055f },
        { 6.46f, 10.46f },
        { 3.46f, 13.46f },
        { 3.042f, 13.953f },
        { 2.691f, 14.497f },
        { 2.412f, 15.081f },
        { 2.212f, 15.696f },
        { 2.093f, 16.331f },
        { 2.057f, 16.977f },
        { 2.104f, 17.622f },
        { 2.234f, 18.256f },
        { 2.445f, 18.867f },
        { 2.734f, 19.446f },
        { 3.094f, 19.983f },
        { 3.521f, 20.469f },
        { 4.007f, 20.896f },
        { 4.544f, 21.256f },
        { 5.123f, 21.545f },
        { 5.734f, 21.756f },
        { 6.368f, 21.886f },
        { 7.013f, 21.933f },
        { 7.659f, 21.897f },
        { 8.294f, 21.778f },
        { 8.909f, 21.578f },
        { 9.493f, 21.299f },
        { 10.037f, 20.948f },
        { 10.53f, 20.53f },
        { 12.24f, 18.82f }
    };
    rcIconDrawPolyline(bounds, path1,
                       41,
                       viewBox, stroke, false, color);
}

static inline void rcIconLink(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconLink, NULL);
}

#endif /* RC_ICON_LINK_H */
