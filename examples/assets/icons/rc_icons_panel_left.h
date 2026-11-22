#ifndef RC_ICON_PANEL_LEFT_H
#define RC_ICON_PANEL_LEFT_H

#include "rc_icons_common.h"

/*
    Generated from panel-left.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconPanelLeft(RC_BoundingBox bounds,
                                       RC_Color color,
                                       const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundedRectStroke(bounds, 3.0f, 3.0f, 18.0f, 18.0f,
                                2.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 9.0f, 3.0f, 9.0f, 21.0f, viewBox, stroke, color);
}

static inline void rcIconPanelLeft(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconPanelLeft, NULL);
}

#endif /* RC_ICON_PANEL_LEFT_H */
