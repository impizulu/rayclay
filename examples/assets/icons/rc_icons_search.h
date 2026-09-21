#ifndef RC_ICON_SEARCH_H
#define RC_ICON_SEARCH_H

#include "rc_icons_common.h"

/*
    Generated from search.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconSearch(RC_BoundingBox bounds,
                                    RC_Color color,
                                    const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 21.0f, 21.0f, 16.66f, 16.66f, viewBox, stroke, color);
    rcIconDrawCircleStroke(bounds, 11.0f, 11.0f, 8.0f, viewBox, stroke, color);
}

static inline void rcIconSearch(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconSearch, NULL);
}

#endif /* RC_ICON_SEARCH_H */
