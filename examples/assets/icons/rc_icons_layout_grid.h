#ifndef RC_ICON_LAYOUT_GRID_H
#define RC_ICON_LAYOUT_GRID_H

#include "rc_icons_common.h"

/*
    Generated from layout-grid.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconLayoutGrid(RC_BoundingBox bounds,
                                        RC_Color color,
                                        const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundedRectStroke(bounds, 3.0f, 3.0f, 7.0f, 7.0f,
                                1.0f, viewBox, stroke, color);
    rcIconDrawRoundedRectStroke(bounds, 14.0f, 3.0f, 7.0f, 7.0f,
                                1.0f, viewBox, stroke, color);
    rcIconDrawRoundedRectStroke(bounds, 14.0f, 14.0f, 7.0f, 7.0f,
                                1.0f, viewBox, stroke, color);
    rcIconDrawRoundedRectStroke(bounds, 3.0f, 14.0f, 7.0f, 7.0f,
                                1.0f, viewBox, stroke, color);
}

static inline void rcIconLayoutGrid(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconLayoutGrid, NULL);
}

#endif /* RC_ICON_LAYOUT_GRID_H */
