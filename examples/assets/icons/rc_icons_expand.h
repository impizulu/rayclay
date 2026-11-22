#ifndef RC_ICON_EXPAND_H
#define RC_ICON_EXPAND_H

#include "rc_icons_common.h"

/*
    Generated from expand.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconExpand(RC_BoundingBox bounds,
                                    RC_Color color,
                                    const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 15.0f, 15.0f, 21.0f, 21.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 15.0f, 9.0f, 21.0f, 3.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 21.0f, 16.0f },
        { 21.0f, 21.0f },
        { 16.0f, 21.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       3,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 21.0f, 8.0f },
        { 21.0f, 3.0f },
        { 16.0f, 3.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       3,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path2[] = {
        { 3.0f, 16.0f },
        { 3.0f, 21.0f },
        { 8.0f, 21.0f }
    };
    rcIconDrawPolyline(bounds, path2,
                       3,
                       viewBox, stroke, false, color);

    rcIconDrawRoundLine(bounds, 3.0f, 21.0f, 9.0f, 15.0f, viewBox, stroke, color);

    static const RC_IconPoint path3[] = {
        { 3.0f, 8.0f },
        { 3.0f, 3.0f },
        { 8.0f, 3.0f }
    };
    rcIconDrawPolyline(bounds, path3,
                       3,
                       viewBox, stroke, false, color);

    rcIconDrawRoundLine(bounds, 9.0f, 9.0f, 3.0f, 3.0f, viewBox, stroke, color);
}

static inline void rcIconExpand(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconExpand, NULL);
}

#endif /* RC_ICON_EXPAND_H */
