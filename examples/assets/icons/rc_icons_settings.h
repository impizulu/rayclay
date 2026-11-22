#ifndef RC_ICON_SETTINGS_H
#define RC_ICON_SETTINGS_H

#include "rc_icons_common.h"

/*
    Generated from settings.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 32x32.
*/
static inline void rcDrawIconSettings(RC_BoundingBox bounds,
                                      RC_Color color,
                                      const void *userData) {
    (void)userData;
    const float viewBox = 32.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 13.0f, 2.0f },
        { 13.0f, 6.0f },
        { 11.0f, 7.0f },
        { 8.0f, 4.0f },
        { 4.0f, 8.0f },
        { 7.0f, 11.0f },
        { 6.0f, 13.0f },
        { 2.0f, 13.0f },
        { 2.0f, 19.0f },
        { 6.0f, 19.0f },
        { 7.0f, 21.0f },
        { 4.0f, 24.0f },
        { 8.0f, 28.0f },
        { 11.0f, 25.0f },
        { 13.0f, 26.0f },
        { 13.0f, 30.0f },
        { 19.0f, 30.0f },
        { 19.0f, 26.0f },
        { 21.0f, 25.0f },
        { 24.0f, 28.0f },
        { 28.0f, 24.0f },
        { 25.0f, 21.0f },
        { 26.0f, 19.0f },
        { 30.0f, 19.0f },
        { 30.0f, 13.0f },
        { 26.0f, 13.0f },
        { 25.0f, 11.0f },
        { 28.0f, 8.0f },
        { 24.0f, 4.0f },
        { 21.0f, 7.0f },
        { 19.0f, 6.0f },
        { 19.0f, 2.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       32,
                       viewBox, stroke, true, color);

    rcIconDrawCircleStroke(bounds, 16.0f, 16.0f, 4.0f, viewBox, stroke, color);
}

static inline void rcIconSettings(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconSettings, NULL);
}

#endif /* RC_ICON_SETTINGS_H */
