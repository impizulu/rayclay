#ifndef RC_ICON_MONITOR_H
#define RC_ICON_MONITOR_H

#include "rc_icons_common.h"

/*
    Generated from monitor.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconMonitor(RC_BoundingBox bounds,
                                     RC_Color color,
                                     const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundedRectStroke(bounds, 2.0f, 3.0f, 20.0f, 14.0f,
                                2.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 8.0f, 21.0f, 16.0f, 21.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 12.0f, 17.0f, 12.0f, 21.0f, viewBox, stroke, color);
}

static inline void rcIconMonitor(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconMonitor, NULL);
}

#endif /* RC_ICON_MONITOR_H */
