#ifndef RC_ICON_HOUSE_H
#define RC_ICON_HOUSE_H

#include "rc_icons_common.h"

/*
    Generated from house.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconHouse(RC_BoundingBox bounds,
                                   RC_Color color,
                                   const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 15.0f, 21.0f },
        { 15.0f, 13.0f },
        { 14.991f, 12.869f },
        { 14.966f, 12.741f },
        { 14.924f, 12.617f },
        { 14.866f, 12.5f },
        { 14.793f, 12.391f },
        { 14.707f, 12.293f },
        { 14.609f, 12.207f },
        { 14.5f, 12.134f },
        { 14.383f, 12.076f },
        { 14.259f, 12.034f },
        { 14.131f, 12.009f },
        { 14.0f, 12.0f },
        { 10.0f, 12.0f },
        { 9.869f, 12.009f },
        { 9.741f, 12.034f },
        { 9.617f, 12.076f },
        { 9.5f, 12.134f },
        { 9.391f, 12.207f },
        { 9.293f, 12.293f },
        { 9.207f, 12.391f },
        { 9.134f, 12.5f },
        { 9.076f, 12.617f },
        { 9.034f, 12.741f },
        { 9.009f, 12.869f },
        { 9.0f, 13.0f },
        { 9.0f, 21.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       (int)(sizeof(path0) / sizeof(path0[0])),
                       viewBox, stroke, false, color);

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
                       (int)(sizeof(path1) / sizeof(path1[0])),
                       viewBox, stroke, true, color);
}

static inline void rcIconHouse(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconHouse, NULL);
}

#endif /* RC_ICON_HOUSE_H */
