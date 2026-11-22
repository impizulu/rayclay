#ifndef RC_ICON_CARET_DOWN_H
#define RC_ICON_CARET_DOWN_H

#include "rc_icons_common.h"

/*
    Generated from caret-down.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 16x16.
*/
static inline void rcDrawIconCaretDown(RC_BoundingBox bounds,
                                       RC_Color color,
                                       const void *userData) {
    (void)userData;
    const float viewBox = 16.0f;

    static const RC_IconPoint path0[] = {
        { 3.0f, 6.0f },
        { 13.0f, 6.0f },
        { 8.0f, 11.0f }
    };
    rcIconDrawFilledPolygon(bounds, path0,
                            3,
                            viewBox, color);
}

static inline void rcIconCaretDown(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconCaretDown, NULL);
}

#endif /* RC_ICON_CARET_DOWN_H */
