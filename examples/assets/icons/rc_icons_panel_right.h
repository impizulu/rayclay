#ifndef RC_ICON_PANEL_RIGHT_H
#define RC_ICON_PANEL_RIGHT_H

#include "rc_icons_common.h"

/*
    Generated from panel-right.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconPanelRight(RC_BoundingBox bounds,
                                        RC_Color color,
                                        const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundedRectStroke(bounds, 3.0f, 3.0f, 18.0f, 18.0f,
                                2.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 15.0f, 3.0f, 15.0f, 21.0f, viewBox, stroke, color);
}

static inline void rcIconPanelRight(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconPanelRight, NULL);
}

#endif /* RC_ICON_PANEL_RIGHT_H */
