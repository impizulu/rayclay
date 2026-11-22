#ifndef RC_ICON_CALENDAR_H
#define RC_ICON_CALENDAR_H

#include "rc_icons_common.h"

/*
    Generated from calendar.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconCalendar(RC_BoundingBox bounds,
                                      RC_Color color,
                                      const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 8.0f, 2.0f, 8.0f, 6.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 16.0f, 2.0f, 16.0f, 6.0f, viewBox, stroke, color);
    rcIconDrawRoundedRectStroke(bounds, 3.0f, 4.0f, 18.0f, 18.0f,
                                2.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 3.0f, 10.0f, 21.0f, 10.0f, viewBox, stroke, color);
}

static inline void rcIconCalendar(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconCalendar, NULL);
}

#endif /* RC_ICON_CALENDAR_H */
