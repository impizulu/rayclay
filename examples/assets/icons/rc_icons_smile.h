#ifndef RC_ICON_SMILE_H
#define RC_ICON_SMILE_H

#include "rc_icons_common.h"

/*
    Generated from smile.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconSmile(RC_BoundingBox bounds,
                                   RC_Color color,
                                   const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawCircleStroke(bounds, 12.0f, 12.0f, 10.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 8.0f, 14.0f },
        { 8.017f, 14.022f },
        { 8.069f, 14.086f },
        { 8.155f, 14.185f },
        { 8.273f, 14.312f },
        { 8.424f, 14.464f },
        { 8.606f, 14.633f },
        { 8.819f, 14.813f },
        { 9.062f, 15.0f },
        { 9.335f, 15.187f },
        { 9.636f, 15.367f },
        { 9.964f, 15.536f },
        { 10.32f, 15.688f },
        { 10.703f, 15.815f },
        { 11.11f, 15.914f },
        { 11.543f, 15.978f },
        { 12.0f, 16.0f },
        { 12.457f, 15.978f },
        { 12.89f, 15.914f },
        { 13.297f, 15.815f },
        { 13.68f, 15.688f },
        { 14.036f, 15.536f },
        { 14.364f, 15.367f },
        { 14.665f, 15.187f },
        { 14.938f, 15.0f },
        { 15.181f, 14.813f },
        { 15.394f, 14.633f },
        { 15.576f, 14.464f },
        { 15.727f, 14.312f },
        { 15.845f, 14.185f },
        { 15.931f, 14.086f },
        { 15.983f, 14.022f },
        { 16.0f, 14.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       33,
                       viewBox, stroke, false, color);

    rcIconDrawRoundLine(bounds, 9.0f, 9.0f, 9.01f, 9.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 15.0f, 9.0f, 15.01f, 9.0f, viewBox, stroke, color);
}

static inline void rcIconSmile(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconSmile, NULL);
}

#endif /* RC_ICON_SMILE_H */
