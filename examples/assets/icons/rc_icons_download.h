#ifndef RC_ICON_DOWNLOAD_H
#define RC_ICON_DOWNLOAD_H

#include "rc_icons_common.h"

/*
    Generated from download.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconDownload(RC_BoundingBox bounds,
                                      RC_Color color,
                                      const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 12.0f, 15.0f, 12.0f, 3.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 21.0f, 15.0f },
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
        { 3.0f, 19.0f },
        { 3.0f, 15.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       28,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 7.0f, 10.0f },
        { 12.0f, 15.0f },
        { 17.0f, 10.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       3,
                       viewBox, stroke, false, color);
}

static inline void rcIconDownload(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconDownload, NULL);
}

#endif /* RC_ICON_DOWNLOAD_H */
