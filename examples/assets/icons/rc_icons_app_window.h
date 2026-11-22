#ifndef RC_ICON_APP_WINDOW_H
#define RC_ICON_APP_WINDOW_H

#include "rc_icons_common.h"

/*
    Generated from app-window.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconAppWindow(RC_BoundingBox bounds,
                                       RC_Color color,
                                       const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundedRectStroke(bounds, 2.0f, 4.0f, 20.0f, 16.0f,
                                2.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 10.0f, 4.0f, 10.0f, 8.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 2.0f, 8.0f, 22.0f, 8.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 6.0f, 4.0f, 6.0f, 8.0f, viewBox, stroke, color);
}

static inline void rcIconAppWindow(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconAppWindow, NULL);
}

#endif /* RC_ICON_APP_WINDOW_H */
