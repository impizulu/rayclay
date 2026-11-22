#ifndef RC_ICON_REDO_2_H
#define RC_ICON_REDO_2_H

#include "rc_icons_common.h"

/*
    Generated from redo-2.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconRedo2(RC_BoundingBox bounds,
                                   RC_Color color,
                                   const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 15.0f, 14.0f },
        { 20.0f, 9.0f },
        { 15.0f, 4.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       3,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 20.0f, 9.0f },
        { 9.5f, 9.0f },
        { 8.782f, 9.047f },
        { 8.076f, 9.187f },
        { 7.395f, 9.419f },
        { 6.75f, 9.737f },
        { 6.152f, 10.137f },
        { 5.611f, 10.611f },
        { 5.137f, 11.152f },
        { 4.737f, 11.75f },
        { 4.419f, 12.395f },
        { 4.187f, 13.076f },
        { 4.047f, 13.782f },
        { 4.0f, 14.5f },
        { 4.047f, 15.218f },
        { 4.187f, 15.924f },
        { 4.419f, 16.605f },
        { 4.737f, 17.25f },
        { 5.137f, 17.848f },
        { 5.611f, 18.389f },
        { 6.152f, 18.863f },
        { 6.75f, 19.263f },
        { 7.395f, 19.581f },
        { 8.076f, 19.813f },
        { 8.782f, 19.953f },
        { 9.5f, 20.0f },
        { 13.0f, 20.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       27,
                       viewBox, stroke, false, color);
}

static inline void rcIconRedo2(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconRedo2, NULL);
}

#endif /* RC_ICON_REDO_2_H */
