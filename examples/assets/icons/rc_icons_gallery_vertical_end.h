#ifndef RC_ICON_GALLERY_VERTICAL_END_H
#define RC_ICON_GALLERY_VERTICAL_END_H

#include "rc_icons_common.h"

/*
    Generated from gallery-vertical-end.svg (Lucide). The layout pass owns
    placement; the library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconGalleryVerticalEnd(RC_BoundingBox bounds,
                                                RC_Color color,
                                                const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 7.0f, 2.0f, 17.0f, 2.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 5.0f, 6.0f, 19.0f, 6.0f, viewBox, stroke, color);
    rcIconDrawRoundedRectStroke(bounds, 3.0f, 10.0f, 18.0f, 12.0f,
                                2.0f, viewBox, stroke, color);
}

static inline void rcIconGalleryVerticalEnd(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconGalleryVerticalEnd, NULL);
}

#endif /* RC_ICON_GALLERY_VERTICAL_END_H */
