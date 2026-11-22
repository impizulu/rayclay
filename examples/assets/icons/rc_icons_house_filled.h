#ifndef RC_ICON_HOUSE_FILLED_H
#define RC_ICON_HOUSE_FILLED_H

#include "rc_icons_common.h"

/*
    Generated from house-filled.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconHouseFilled(RC_BoundingBox bounds,
                                         RC_Color color,
                                         const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 3.0f, 10.0f },
        { 3.015f, 9.752f },
        { 3.061f, 9.508f },
        { 3.137f, 9.272f },
        { 3.242f, 9.047f },
        { 3.373f, 8.836f },
        { 3.53f, 8.644f },
        { 3.709f, 8.472f },
        { 10.709f, 2.473f },
        { 10.914f, 2.321f },
        { 11.136f, 2.197f },
        { 11.373f, 2.101f },
        { 11.62f, 2.037f },
        { 11.873f, 2.005f },
        { 12.127f, 2.005f },
        { 12.38f, 2.037f },
        { 12.627f, 2.101f },
        { 12.864f, 2.197f },
        { 13.086f, 2.321f },
        { 13.291f, 2.473f },
        { 20.291f, 8.472f },
        { 20.47f, 8.644f },
        { 20.627f, 8.836f },
        { 20.758f, 9.047f },
        { 20.863f, 9.272f },
        { 20.939f, 9.508f },
        { 20.985f, 9.752f },
        { 21.0f, 10.0f },
        { 21.0f, 19.0f },
        { 20.983f, 19.261f },
        { 20.932f, 19.518f },
        { 20.848f, 19.765f },
        { 20.732f, 20.0f },
        { 20.587f, 20.218f },
        { 20.414f, 20.414f },
        { 20.218f, 20.587f },
        { 20.0f, 20.732f },
        { 19.765f, 20.848f },
        { 19.518f, 20.932f },
        { 19.261f, 20.983f },
        { 19.0f, 21.0f },
        { 5.0f, 21.0f },
        { 4.739f, 20.983f },
        { 4.482f, 20.932f },
        { 4.235f, 20.848f },
        { 4.0f, 20.732f },
        { 3.782f, 20.587f },
        { 3.586f, 20.414f },
        { 3.413f, 20.218f },
        { 3.268f, 20.0f },
        { 3.152f, 19.765f },
        { 3.068f, 19.518f },
        { 3.017f, 19.261f },
        { 3.0f, 19.0f }
    };
    rcIconDrawFilledPolygon(bounds, path0,
                            54,
                            viewBox, color);

    static const RC_IconPoint path1[] = {
        { 3.0f, 10.0f },
        { 3.015f, 9.752f },
        { 3.061f, 9.508f },
        { 3.137f, 9.272f },
        { 3.242f, 9.047f },
        { 3.373f, 8.836f },
        { 3.53f, 8.644f },
        { 3.709f, 8.472f },
        { 10.709f, 2.473f },
        { 10.914f, 2.321f },
        { 11.136f, 2.197f },
        { 11.373f, 2.101f },
        { 11.62f, 2.037f },
        { 11.873f, 2.005f },
        { 12.127f, 2.005f },
        { 12.38f, 2.037f },
        { 12.627f, 2.101f },
        { 12.864f, 2.197f },
        { 13.086f, 2.321f },
        { 13.291f, 2.473f },
        { 20.291f, 8.472f },
        { 20.47f, 8.644f },
        { 20.627f, 8.836f },
        { 20.758f, 9.047f },
        { 20.863f, 9.272f },
        { 20.939f, 9.508f },
        { 20.985f, 9.752f },
        { 21.0f, 10.0f },
        { 21.0f, 19.0f },
        { 20.983f, 19.261f },
        { 20.932f, 19.518f },
        { 20.848f, 19.765f },
        { 20.732f, 20.0f },
        { 20.587f, 20.218f },
        { 20.414f, 20.414f },
        { 20.218f, 20.587f },
        { 20.0f, 20.732f },
        { 19.765f, 20.848f },
        { 19.518f, 20.932f },
        { 19.261f, 20.983f },
        { 19.0f, 21.0f },
        { 5.0f, 21.0f },
        { 4.739f, 20.983f },
        { 4.482f, 20.932f },
        { 4.235f, 20.848f },
        { 4.0f, 20.732f },
        { 3.782f, 20.587f },
        { 3.586f, 20.414f },
        { 3.413f, 20.218f },
        { 3.268f, 20.0f },
        { 3.152f, 19.765f },
        { 3.068f, 19.518f },
        { 3.017f, 19.261f },
        { 3.0f, 19.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       54,
                       viewBox, stroke, true, color);
}

static inline void rcIconHouseFilled(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconHouseFilled, NULL);
}

#endif /* RC_ICON_HOUSE_FILLED_H */
