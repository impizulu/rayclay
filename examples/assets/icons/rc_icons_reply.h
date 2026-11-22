#ifndef RC_ICON_REPLY_H
#define RC_ICON_REPLY_H

#include "rc_icons_common.h"

/*
    Generated from reply.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconReply(RC_BoundingBox bounds,
                                   RC_Color color,
                                   const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 20.0f, 18.0f },
        { 20.0f, 16.0f },
        { 19.966f, 15.478f },
        { 19.864f, 14.965f },
        { 19.696f, 14.469f },
        { 19.464f, 14.0f },
        { 19.173f, 13.565f },
        { 18.828f, 13.172f },
        { 18.435f, 12.827f },
        { 18.0f, 12.536f },
        { 17.531f, 12.304f },
        { 17.035f, 12.136f },
        { 16.522f, 12.034f },
        { 16.0f, 12.0f },
        { 4.0f, 12.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       15,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 9.0f, 17.0f },
        { 4.0f, 12.0f },
        { 9.0f, 7.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       3,
                       viewBox, stroke, false, color);
}

static inline void rcIconReply(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconReply, NULL);
}

#endif /* RC_ICON_REPLY_H */
