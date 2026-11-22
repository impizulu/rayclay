#ifndef RC_ICON_ARCHIVE_H
#define RC_ICON_ARCHIVE_H

#include "rc_icons_common.h"

/*
    Generated from archive.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconArchive(RC_BoundingBox bounds,
                                     RC_Color color,
                                     const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundedRectStroke(bounds, 2.0f, 3.0f, 20.0f, 5.0f,
                                1.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 4.0f, 8.0f },
        { 4.0f, 19.0f },
        { 4.017f, 19.261f },
        { 4.068f, 19.518f },
        { 4.152f, 19.765f },
        { 4.268f, 20.0f },
        { 4.413f, 20.218f },
        { 4.586f, 20.414f },
        { 4.782f, 20.587f },
        { 5.0f, 20.732f },
        { 5.235f, 20.848f },
        { 5.482f, 20.932f },
        { 5.739f, 20.983f },
        { 6.0f, 21.0f },
        { 18.0f, 21.0f },
        { 18.261f, 20.983f },
        { 18.518f, 20.932f },
        { 18.765f, 20.848f },
        { 19.0f, 20.732f },
        { 19.218f, 20.587f },
        { 19.414f, 20.414f },
        { 19.587f, 20.218f },
        { 19.732f, 20.0f },
        { 19.848f, 19.765f },
        { 19.932f, 19.518f },
        { 19.983f, 19.261f },
        { 20.0f, 19.0f },
        { 20.0f, 8.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       28,
                       viewBox, stroke, false, color);

    rcIconDrawRoundLine(bounds, 10.0f, 12.0f, 14.0f, 12.0f, viewBox, stroke, color);
}

static inline void rcIconArchive(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconArchive, NULL);
}

#endif /* RC_ICON_ARCHIVE_H */
