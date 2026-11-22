#ifndef RC_ICON_MUSIC_H
#define RC_ICON_MUSIC_H

#include "rc_icons_common.h"

/*
    Generated from music.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconMusic(RC_BoundingBox bounds,
                                   RC_Color color,
                                   const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 9.0f, 18.0f },
        { 9.0f, 5.0f },
        { 21.0f, 3.0f },
        { 21.0f, 16.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       4,
                       viewBox, stroke, false, color);

    rcIconDrawCircleStroke(bounds, 6.0f, 18.0f, 3.0f, viewBox, stroke, color);
    rcIconDrawCircleStroke(bounds, 18.0f, 16.0f, 3.0f, viewBox, stroke, color);
}

static inline void rcIconMusic(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconMusic, NULL);
}

#endif /* RC_ICON_MUSIC_H */
