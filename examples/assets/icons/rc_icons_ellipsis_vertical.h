#ifndef RC_ICON_ELLIPSIS_VERTICAL_H
#define RC_ICON_ELLIPSIS_VERTICAL_H

#include "rc_icons_common.h"

/*
    Generated from ellipsis-vertical.svg (Lucide). The layout pass owns
    placement; the library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconEllipsisVertical(RC_BoundingBox bounds,
                                              RC_Color color,
                                              const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawCircleStroke(bounds, 12.0f, 12.0f, 1.0f, viewBox, stroke, color);
    rcIconDrawCircleStroke(bounds, 12.0f, 5.0f, 1.0f, viewBox, stroke, color);
    rcIconDrawCircleStroke(bounds, 12.0f, 19.0f, 1.0f, viewBox, stroke, color);
}

static inline void rcIconEllipsisVertical(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconEllipsisVertical, NULL);
}

#endif /* RC_ICON_ELLIPSIS_VERTICAL_H */
