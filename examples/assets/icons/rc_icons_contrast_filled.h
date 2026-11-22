#ifndef RC_ICON_CONTRAST_FILLED_H
#define RC_ICON_CONTRAST_FILLED_H

#include "rc_icons_common.h"

/*
    Generated from contrast-filled.svg (Lucide). The layout pass owns placement;
    the library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconContrastFilled(RC_BoundingBox bounds,
                                            RC_Color color,
                                            const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawCircleStroke(bounds, 12.0f, 12.0f, 10.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 12.0f, 18.0f },
        { 12.783f, 17.949f },
        { 13.553f, 17.796f },
        { 14.296f, 17.543f },
        { 15.0f, 17.196f },
        { 15.653f, 16.76f },
        { 16.243f, 16.243f },
        { 16.76f, 15.653f },
        { 17.196f, 15.0f },
        { 17.543f, 14.296f },
        { 17.796f, 13.553f },
        { 17.949f, 12.783f },
        { 18.0f, 12.0f },
        { 17.949f, 11.217f },
        { 17.796f, 10.447f },
        { 17.543f, 9.704f },
        { 17.196f, 9.0f },
        { 16.76f, 8.347f },
        { 16.243f, 7.757f },
        { 15.653f, 7.24f },
        { 15.0f, 6.804f },
        { 14.296f, 6.457f },
        { 13.553f, 6.204f },
        { 12.783f, 6.051f },
        { 12.0f, 6.0f }
    };
    rcIconDrawFilledPolygon(bounds, path0,
                            25,
                            viewBox, color);

    static const RC_IconPoint path1[] = {
        { 12.0f, 18.0f },
        { 12.783f, 17.949f },
        { 13.553f, 17.796f },
        { 14.296f, 17.543f },
        { 15.0f, 17.196f },
        { 15.653f, 16.76f },
        { 16.243f, 16.243f },
        { 16.76f, 15.653f },
        { 17.196f, 15.0f },
        { 17.543f, 14.296f },
        { 17.796f, 13.553f },
        { 17.949f, 12.783f },
        { 18.0f, 12.0f },
        { 17.949f, 11.217f },
        { 17.796f, 10.447f },
        { 17.543f, 9.704f },
        { 17.196f, 9.0f },
        { 16.76f, 8.347f },
        { 16.243f, 7.757f },
        { 15.653f, 7.24f },
        { 15.0f, 6.804f },
        { 14.296f, 6.457f },
        { 13.553f, 6.204f },
        { 12.783f, 6.051f },
        { 12.0f, 6.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       25,
                       viewBox, stroke, true, color);
}

static inline void rcIconContrastFilled(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconContrastFilled, NULL);
}

#endif /* RC_ICON_CONTRAST_FILLED_H */
