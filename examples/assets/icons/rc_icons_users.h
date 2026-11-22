#ifndef RC_ICON_USERS_H
#define RC_ICON_USERS_H

#include "rc_icons_common.h"

/*
    Generated from users.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconUsers(RC_BoundingBox bounds,
                                   RC_Color color,
                                   const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 16.0f, 21.0f },
        { 16.0f, 19.0f },
        { 15.966f, 18.478f },
        { 15.864f, 17.965f },
        { 15.696f, 17.469f },
        { 15.464f, 17.0f },
        { 15.173f, 16.565f },
        { 14.828f, 16.172f },
        { 14.435f, 15.827f },
        { 14.0f, 15.536f },
        { 13.531f, 15.304f },
        { 13.035f, 15.136f },
        { 12.522f, 15.034f },
        { 12.0f, 15.0f },
        { 6.0f, 15.0f },
        { 5.478f, 15.034f },
        { 4.965f, 15.136f },
        { 4.469f, 15.304f },
        { 4.0f, 15.536f },
        { 3.565f, 15.827f },
        { 3.172f, 16.172f },
        { 2.827f, 16.565f },
        { 2.536f, 17.0f },
        { 2.304f, 17.469f },
        { 2.136f, 17.965f },
        { 2.034f, 18.478f },
        { 2.0f, 19.0f },
        { 2.0f, 21.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       28,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 16.0f, 3.128f },
        { 16.477f, 3.284f },
        { 16.93f, 3.498f },
        { 17.353f, 3.768f },
        { 17.739f, 4.088f },
        { 18.081f, 4.454f },
        { 18.376f, 4.86f },
        { 18.617f, 5.3f },
        { 18.801f, 5.766f },
        { 18.926f, 6.252f },
        { 18.988f, 6.749f },
        { 18.988f, 7.251f },
        { 18.926f, 7.748f },
        { 18.801f, 8.234f },
        { 18.617f, 8.7f },
        { 18.376f, 9.14f },
        { 18.081f, 9.546f },
        { 17.739f, 9.912f },
        { 17.353f, 10.232f },
        { 16.93f, 10.502f },
        { 16.477f, 10.716f },
        { 16.0f, 10.872f }
    };
    rcIconDrawPolyline(bounds, path1,
                       22,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path2[] = {
        { 22.0f, 21.0f },
        { 22.0f, 19.0f },
        { 21.971f, 18.522f },
        { 21.885f, 18.051f },
        { 21.744f, 17.594f },
        { 21.548f, 17.157f },
        { 21.302f, 16.746f },
        { 21.009f, 16.368f },
        { 20.673f, 16.027f },
        { 20.298f, 15.729f },
        { 19.89f, 15.478f },
        { 19.456f, 15.277f },
        { 19.0f, 15.13f }
    };
    rcIconDrawPolyline(bounds, path2,
                       13,
                       viewBox, stroke, false, color);

    rcIconDrawCircleStroke(bounds, 9.0f, 7.0f, 4.0f, viewBox, stroke, color);
}

static inline void rcIconUsers(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconUsers, NULL);
}

#endif /* RC_ICON_USERS_H */
