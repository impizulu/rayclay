#ifndef RC_ICON_EYE_OFF_H
#define RC_ICON_EYE_OFF_H

#include "rc_icons_common.h"

/*
    Generated from eye-off.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconEyeOff(RC_BoundingBox bounds,
                                    RC_Color color,
                                    const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 10.733f, 5.076f },
        { 12.125f, 5.001f },
        { 13.515f, 5.107f },
        { 14.88f, 5.392f },
        { 16.196f, 5.852f },
        { 17.442f, 6.478f },
        { 18.596f, 7.26f },
        { 19.639f, 8.185f },
        { 20.554f, 9.237f },
        { 21.324f, 10.399f },
        { 21.938f, 11.651f },
        { 21.973f, 11.764f },
        { 21.993f, 11.881f },
        { 22.001f, 11.999f },
        { 21.993f, 12.117f },
        { 21.973f, 12.234f },
        { 21.938f, 12.347f },
        { 21.641f, 13.005f },
        { 21.3f, 13.641f },
        { 20.917f, 14.252f },
        { 20.494f, 14.837f }
    };
    rcIconDrawPolyline(bounds, path0,
                       21,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 14.084f, 14.158f },
        { 13.788f, 14.409f },
        { 13.462f, 14.62f },
        { 13.112f, 14.787f },
        { 12.743f, 14.907f },
        { 12.361f, 14.978f },
        { 11.974f, 15.0f },
        { 11.587f, 14.972f },
        { 11.207f, 14.893f },
        { 10.84f, 14.767f },
        { 10.492f, 14.594f },
        { 10.17f, 14.378f },
        { 9.879f, 14.121f },
        { 9.622f, 13.83f },
        { 9.406f, 13.508f },
        { 9.233f, 13.16f },
        { 9.107f, 12.793f },
        { 9.028f, 12.413f },
        { 9.0f, 12.026f },
        { 9.022f, 11.639f },
        { 9.093f, 11.257f },
        { 9.213f, 10.888f },
        { 9.38f, 10.538f },
        { 9.591f, 10.212f },
        { 9.842f, 9.916f }
    };
    rcIconDrawPolyline(bounds, path1,
                       25,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path2[] = {
        { 17.479f, 17.499f },
        { 16.308f, 18.099f },
        { 15.073f, 18.551f },
        { 13.791f, 18.85f },
        { 12.483f, 18.989f },
        { 11.168f, 18.968f },
        { 9.865f, 18.786f },
        { 8.594f, 18.446f },
        { 7.374f, 17.953f },
        { 6.223f, 17.316f },
        { 5.159f, 16.542f },
        { 4.197f, 15.644f },
        { 3.352f, 14.636f },
        { 2.637f, 13.531f },
        { 2.062f, 12.348f },
        { 2.027f, 12.235f },
        { 2.007f, 12.118f },
        { 1.999f, 12.0f },
        { 2.007f, 11.882f },
        { 2.027f, 11.765f },
        { 2.062f, 11.652f },
        { 2.67f, 10.41f },
        { 3.433f, 9.257f },
        { 4.337f, 8.211f },
        { 5.367f, 7.29f },
        { 6.508f, 6.509f }
    };
    rcIconDrawPolyline(bounds, path2,
                       26,
                       viewBox, stroke, false, color);

    rcIconDrawRoundLine(bounds, 2.0f, 2.0f, 22.0f, 22.0f, viewBox, stroke, color);
}

static inline void rcIconEyeOff(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconEyeOff, NULL);
}

#endif /* RC_ICON_EYE_OFF_H */
