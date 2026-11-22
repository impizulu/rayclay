#ifndef RC_ICON_PRESENTATION_H
#define RC_ICON_PRESENTATION_H

#include "rc_icons_common.h"

/*
    Generated from presentation.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconPresentation(RC_BoundingBox bounds,
                                          RC_Color color,
                                          const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 2.0f, 3.0f, 22.0f, 3.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 21.0f, 3.0f },
        { 21.0f, 14.0f },
        { 20.983f, 14.261f },
        { 20.932f, 14.518f },
        { 20.848f, 14.765f },
        { 20.732f, 15.0f },
        { 20.587f, 15.218f },
        { 20.414f, 15.414f },
        { 20.218f, 15.587f },
        { 20.0f, 15.732f },
        { 19.765f, 15.848f },
        { 19.518f, 15.932f },
        { 19.261f, 15.983f },
        { 19.0f, 16.0f },
        { 5.0f, 16.0f },
        { 4.739f, 15.983f },
        { 4.482f, 15.932f },
        { 4.235f, 15.848f },
        { 4.0f, 15.732f },
        { 3.782f, 15.587f },
        { 3.586f, 15.414f },
        { 3.413f, 15.218f },
        { 3.268f, 15.0f },
        { 3.152f, 14.765f },
        { 3.068f, 14.518f },
        { 3.017f, 14.261f },
        { 3.0f, 14.0f },
        { 3.0f, 3.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       28,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 7.0f, 21.0f },
        { 12.0f, 16.0f },
        { 17.0f, 21.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       3,
                       viewBox, stroke, false, color);
}

static inline void rcIconPresentation(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconPresentation, NULL);
}

#endif /* RC_ICON_PRESENTATION_H */
