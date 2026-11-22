#ifndef RC_ICON_SHARE_H
#define RC_ICON_SHARE_H

#include "rc_icons_common.h"

/*
    Generated from share.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconShare(RC_BoundingBox bounds,
                                   RC_Color color,
                                   const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 12.0f, 2.0f, 12.0f, 15.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 16.0f, 6.0f },
        { 12.0f, 2.0f },
        { 8.0f, 6.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       3,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 4.0f, 12.0f },
        { 4.0f, 20.0f },
        { 4.017f, 20.261f },
        { 4.068f, 20.518f },
        { 4.152f, 20.765f },
        { 4.268f, 21.0f },
        { 4.413f, 21.218f },
        { 4.586f, 21.414f },
        { 4.782f, 21.587f },
        { 5.0f, 21.732f },
        { 5.235f, 21.848f },
        { 5.482f, 21.932f },
        { 5.739f, 21.983f },
        { 6.0f, 22.0f },
        { 18.0f, 22.0f },
        { 18.261f, 21.983f },
        { 18.518f, 21.932f },
        { 18.765f, 21.848f },
        { 19.0f, 21.732f },
        { 19.218f, 21.587f },
        { 19.414f, 21.414f },
        { 19.587f, 21.218f },
        { 19.732f, 21.0f },
        { 19.848f, 20.765f },
        { 19.932f, 20.518f },
        { 19.983f, 20.261f },
        { 20.0f, 20.0f },
        { 20.0f, 12.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       28,
                       viewBox, stroke, false, color);
}

static inline void rcIconShare(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconShare, NULL);
}

#endif /* RC_ICON_SHARE_H */
