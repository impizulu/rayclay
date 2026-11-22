#ifndef RC_ICON_ZOOM_OUT_H
#define RC_ICON_ZOOM_OUT_H

#include "rc_icons_common.h"

/*
    Generated from zoom-out.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconZoomOut(RC_BoundingBox bounds,
                                     RC_Color color,
                                     const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawCircleStroke(bounds, 11.0f, 11.0f, 8.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 21.0f, 21.0f, 16.65f, 16.65f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 8.0f, 11.0f, 14.0f, 11.0f, viewBox, stroke, color);
}

static inline void rcIconZoomOut(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconZoomOut, NULL);
}

#endif /* RC_ICON_ZOOM_OUT_H */
