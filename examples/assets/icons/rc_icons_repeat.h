#ifndef RC_ICON_REPEAT_H
#define RC_ICON_REPEAT_H

#include "rc_icons_common.h"

/*
    Generated from repeat.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconRepeat(RC_BoundingBox bounds,
                                    RC_Color color,
                                    const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 17.0f, 2.0f },
        { 21.0f, 6.0f },
        { 17.0f, 10.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       3,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 3.0f, 11.0f },
        { 3.0f, 10.0f },
        { 3.034f, 9.478f },
        { 3.136f, 8.965f },
        { 3.304f, 8.469f },
        { 3.536f, 8.0f },
        { 3.827f, 7.565f },
        { 4.172f, 7.172f },
        { 4.565f, 6.827f },
        { 5.0f, 6.536f },
        { 5.469f, 6.304f },
        { 5.965f, 6.136f },
        { 6.478f, 6.034f },
        { 7.0f, 6.0f },
        { 21.0f, 6.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       15,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path2[] = {
        { 7.0f, 22.0f },
        { 3.0f, 18.0f },
        { 7.0f, 14.0f }
    };
    rcIconDrawPolyline(bounds, path2,
                       3,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path3[] = {
        { 21.0f, 13.0f },
        { 21.0f, 14.0f },
        { 20.966f, 14.522f },
        { 20.864f, 15.035f },
        { 20.696f, 15.531f },
        { 20.464f, 16.0f },
        { 20.173f, 16.435f },
        { 19.828f, 16.828f },
        { 19.435f, 17.173f },
        { 19.0f, 17.464f },
        { 18.531f, 17.696f },
        { 18.035f, 17.864f },
        { 17.522f, 17.966f },
        { 17.0f, 18.0f },
        { 3.0f, 18.0f }
    };
    rcIconDrawPolyline(bounds, path3,
                       15,
                       viewBox, stroke, false, color);
}

static inline void rcIconRepeat(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconRepeat, NULL);
}

#endif /* RC_ICON_REPEAT_H */
