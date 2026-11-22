#ifndef RC_ICON_BRIEFCASE_H
#define RC_ICON_BRIEFCASE_H

#include "rc_icons_common.h"

/*
    Generated from briefcase.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconBriefcase(RC_BoundingBox bounds,
                                       RC_Color color,
                                       const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 16.0f, 20.0f },
        { 16.0f, 4.0f },
        { 15.983f, 3.739f },
        { 15.932f, 3.482f },
        { 15.848f, 3.235f },
        { 15.732f, 3.0f },
        { 15.587f, 2.782f },
        { 15.414f, 2.586f },
        { 15.218f, 2.413f },
        { 15.0f, 2.268f },
        { 14.765f, 2.152f },
        { 14.518f, 2.068f },
        { 14.261f, 2.017f },
        { 14.0f, 2.0f },
        { 10.0f, 2.0f },
        { 9.739f, 2.017f },
        { 9.482f, 2.068f },
        { 9.235f, 2.152f },
        { 9.0f, 2.268f },
        { 8.782f, 2.413f },
        { 8.586f, 2.586f },
        { 8.413f, 2.782f },
        { 8.268f, 3.0f },
        { 8.152f, 3.235f },
        { 8.068f, 3.482f },
        { 8.017f, 3.739f },
        { 8.0f, 4.0f },
        { 8.0f, 20.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       28,
                       viewBox, stroke, false, color);

    rcIconDrawRoundedRectStroke(bounds, 2.0f, 6.0f, 20.0f, 14.0f,
                                2.0f, viewBox, stroke, color);
}

static inline void rcIconBriefcase(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconBriefcase, NULL);
}

#endif /* RC_ICON_BRIEFCASE_H */
