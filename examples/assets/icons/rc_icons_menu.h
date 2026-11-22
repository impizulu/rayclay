#ifndef RC_ICON_MENU_H
#define RC_ICON_MENU_H

#include "rc_icons_common.h"

/*
    Generated from menu.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconMenu(RC_BoundingBox bounds,
                                  RC_Color color,
                                  const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 4.0f, 12.0f, 20.0f, 12.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 4.0f, 18.0f, 20.0f, 18.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 4.0f, 6.0f, 20.0f, 6.0f, viewBox, stroke, color);
}

static inline void rcIconMenu(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconMenu, NULL);
}

#endif /* RC_ICON_MENU_H */
