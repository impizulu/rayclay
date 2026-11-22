#ifndef RC_ICON_RULER_H
#define RC_ICON_RULER_H

#include "rc_icons_common.h"

/*
    Generated from ruler.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconRuler(RC_BoundingBox bounds,
                                   RC_Color color,
                                   const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 21.3f, 15.3f },
        { 21.493f, 15.517f },
        { 21.658f, 15.756f },
        { 21.794f, 16.013f },
        { 21.897f, 16.284f },
        { 21.966f, 16.566f },
        { 22.001f, 16.855f },
        { 22.001f, 17.145f },
        { 21.966f, 17.434f },
        { 21.897f, 17.716f },
        { 21.794f, 17.987f },
        { 21.658f, 18.244f },
        { 21.493f, 18.483f },
        { 21.3f, 18.7f },
        { 18.7f, 21.3f },
        { 18.483f, 21.493f },
        { 18.244f, 21.658f },
        { 17.987f, 21.794f },
        { 17.716f, 21.897f },
        { 17.434f, 21.966f },
        { 17.145f, 22.001f },
        { 16.855f, 22.001f },
        { 16.566f, 21.966f },
        { 16.284f, 21.897f },
        { 16.013f, 21.794f },
        { 15.756f, 21.658f },
        { 15.517f, 21.493f },
        { 15.3f, 21.3f },
        { 2.7f, 8.7f },
        { 2.493f, 8.463f },
        { 2.319f, 8.202f },
        { 2.181f, 7.92f },
        { 2.08f, 7.622f },
        { 2.019f, 7.314f },
        { 1.998f, 7.0f },
        { 2.019f, 6.686f },
        { 2.08f, 6.378f },
        { 2.181f, 6.08f },
        { 2.319f, 5.798f },
        { 2.493f, 5.537f },
        { 2.7f, 5.3f },
        { 5.3f, 2.7f },
        { 5.537f, 2.493f },
        { 5.798f, 2.319f },
        { 6.08f, 2.181f },
        { 6.378f, 2.08f },
        { 6.686f, 2.019f },
        { 7.0f, 1.998f },
        { 7.314f, 2.019f },
        { 7.622f, 2.08f },
        { 7.92f, 2.181f },
        { 8.202f, 2.319f },
        { 8.463f, 2.493f },
        { 8.7f, 2.7f }
    };
    rcIconDrawPolyline(bounds, path0,
                       54,
                       viewBox, stroke, true, color);

    rcIconDrawRoundLine(bounds, 14.5f, 12.5f, 16.5f, 10.5f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 11.5f, 9.5f, 13.5f, 7.5f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 8.5f, 6.5f, 10.5f, 4.5f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 17.5f, 15.5f, 19.5f, 13.5f, viewBox, stroke, color);
}

static inline void rcIconRuler(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconRuler, NULL);
}

#endif /* RC_ICON_RULER_H */
