#ifndef RC_ICON_CARET_RIGHT_H
#define RC_ICON_CARET_RIGHT_H

#include "rc_icons_common.h"

/*
    Generated from caret-right.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 16x16.
*/
static inline void rcDrawIconCaretRight(RC_BoundingBox bounds,
                                        RC_Color color,
                                        const void *userData) {
    (void)userData;
    const float viewBox = 16.0f;

    static const RC_IconPoint path0[] = {
        { 6.0f, 3.0f },
        { 11.0f, 8.0f },
        { 6.0f, 13.0f }
    };
    rcIconDrawFilledPolygon(bounds, path0,
                            3,
                            viewBox, color);
}

static inline void rcIconCaretRight(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconCaretRight, NULL);
}

#endif /* RC_ICON_CARET_RIGHT_H */
