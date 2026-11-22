#ifndef RC_ICON_CAPTIONS_H
#define RC_ICON_CAPTIONS_H

#include "rc_icons_common.h"

/*
    Generated from captions.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconCaptions(RC_BoundingBox bounds,
                                      RC_Color color,
                                      const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundedRectStroke(bounds, 3.0f, 5.0f, 18.0f, 14.0f,
                                2.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 7.0f, 15.0f, 11.0f, 15.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 15.0f, 15.0f, 17.0f, 15.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 7.0f, 11.0f, 9.0f, 11.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 13.0f, 11.0f, 17.0f, 11.0f, viewBox, stroke, color);
}

static inline void rcIconCaptions(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconCaptions, NULL);
}

#endif /* RC_ICON_CAPTIONS_H */
