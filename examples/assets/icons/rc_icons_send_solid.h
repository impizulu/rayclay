#ifndef RC_ICON_SEND_SOLID_H
#define RC_ICON_SEND_SOLID_H

#include "rc_icons_common.h"

/*
    Generated from send-solid.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconSendSolid(RC_BoundingBox bounds,
                                       RC_Color color,
                                       const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;

    static const RC_IconPoint path0[] = {
        { 3.0f, 20.5f },
        { 22.0f, 12.0f },
        { 3.0f, 3.5f },
        { 3.0f, 10.2f },
        { 15.5f, 12.0f },
        { 3.0f, 13.8f }
    };
    rcIconDrawFilledPolygon(bounds, path0,
                            6,
                            viewBox, color);
}

static inline void rcIconSendSolid(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconSendSolid, NULL);
}

#endif /* RC_ICON_SEND_SOLID_H */
