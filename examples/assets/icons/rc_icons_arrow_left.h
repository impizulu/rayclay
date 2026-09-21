#ifndef RC_ICON_ARROW_LEFT_H
#define RC_ICON_ARROW_LEFT_H

#include "rc_icons_common.h"

/*
    Generated from arrow-left.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconArrowLeft(RC_BoundingBox bounds,
                                       RC_Color color,
                                       const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 12.0f, 19.0f },
        { 5.0f, 12.0f },
        { 12.0f, 5.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       (int)(sizeof(path0) / sizeof(path0[0])),
                       viewBox, stroke, false, color);

    rcIconDrawRoundLine(bounds, 19.0f, 12.0f, 5.0f, 12.0f, viewBox, stroke, color);
}

static inline void rcIconArrowLeft(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconArrowLeft, NULL);
}

#endif /* RC_ICON_ARROW_LEFT_H */
