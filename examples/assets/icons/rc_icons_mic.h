#ifndef RC_ICON_MIC_H
#define RC_ICON_MIC_H

#include "rc_icons_common.h"

/*
    Generated from mic.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconMic(RC_BoundingBox bounds,
                                 RC_Color color,
                                 const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 12.0f, 19.0f, 12.0f, 22.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 19.0f, 10.0f },
        { 19.0f, 12.0f },
        { 18.94f, 12.914f },
        { 18.761f, 13.812f },
        { 18.467f, 14.679f },
        { 18.062f, 15.5f },
        { 17.553f, 16.261f },
        { 16.95f, 16.95f },
        { 16.261f, 17.553f },
        { 15.5f, 18.062f },
        { 14.679f, 18.467f },
        { 13.812f, 18.761f },
        { 12.914f, 18.94f },
        { 12.0f, 19.0f },
        { 11.086f, 18.94f },
        { 10.188f, 18.761f },
        { 9.321f, 18.467f },
        { 8.5f, 18.062f },
        { 7.739f, 17.553f },
        { 7.05f, 16.95f },
        { 6.447f, 16.261f },
        { 5.938f, 15.5f },
        { 5.533f, 14.679f },
        { 5.239f, 13.812f },
        { 5.06f, 12.914f },
        { 5.0f, 12.0f },
        { 5.0f, 10.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       27,
                       viewBox, stroke, false, color);

    rcIconDrawRoundedRectStroke(bounds, 9.0f, 2.0f, 6.0f, 13.0f,
                                3.0f, viewBox, stroke, color);
}

static inline void rcIconMic(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconMic, NULL);
}

#endif /* RC_ICON_MIC_H */
