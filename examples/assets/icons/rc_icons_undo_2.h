#ifndef RC_ICON_UNDO_2_H
#define RC_ICON_UNDO_2_H

#include "rc_icons_common.h"

/*
    Generated from undo-2.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconUndo2(RC_BoundingBox bounds,
                                   RC_Color color,
                                   const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 9.0f, 14.0f },
        { 4.0f, 9.0f },
        { 9.0f, 4.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       3,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 4.0f, 9.0f },
        { 14.5f, 9.0f },
        { 15.218f, 9.047f },
        { 15.924f, 9.187f },
        { 16.605f, 9.419f },
        { 17.25f, 9.737f },
        { 17.848f, 10.137f },
        { 18.389f, 10.611f },
        { 18.863f, 11.152f },
        { 19.263f, 11.75f },
        { 19.581f, 12.395f },
        { 19.813f, 13.076f },
        { 19.953f, 13.782f },
        { 20.0f, 14.5f },
        { 19.953f, 15.218f },
        { 19.813f, 15.924f },
        { 19.581f, 16.605f },
        { 19.263f, 17.25f },
        { 18.863f, 17.848f },
        { 18.389f, 18.389f },
        { 17.848f, 18.863f },
        { 17.25f, 19.263f },
        { 16.605f, 19.581f },
        { 15.924f, 19.813f },
        { 15.218f, 19.953f },
        { 14.5f, 20.0f },
        { 11.0f, 20.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       27,
                       viewBox, stroke, false, color);
}

static inline void rcIconUndo2(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconUndo2, NULL);
}

#endif /* RC_ICON_UNDO_2_H */
