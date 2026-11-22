#ifndef RC_ICON_MINIMIZE_H
#define RC_ICON_MINIMIZE_H

#include "rc_icons_common.h"

/*
    Generated from minimize.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconMinimize(RC_BoundingBox bounds,
                                      RC_Color color,
                                      const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 8.0f, 3.0f },
        { 8.0f, 6.0f },
        { 7.983f, 6.261f },
        { 7.932f, 6.518f },
        { 7.848f, 6.765f },
        { 7.732f, 7.0f },
        { 7.587f, 7.218f },
        { 7.414f, 7.414f },
        { 7.218f, 7.587f },
        { 7.0f, 7.732f },
        { 6.765f, 7.848f },
        { 6.518f, 7.932f },
        { 6.261f, 7.983f },
        { 6.0f, 8.0f },
        { 3.0f, 8.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       15,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 21.0f, 8.0f },
        { 18.0f, 8.0f },
        { 17.739f, 7.983f },
        { 17.482f, 7.932f },
        { 17.235f, 7.848f },
        { 17.0f, 7.732f },
        { 16.782f, 7.587f },
        { 16.586f, 7.414f },
        { 16.413f, 7.218f },
        { 16.268f, 7.0f },
        { 16.152f, 6.765f },
        { 16.068f, 6.518f },
        { 16.017f, 6.261f },
        { 16.0f, 6.0f },
        { 16.0f, 3.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       15,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path2[] = {
        { 3.0f, 16.0f },
        { 6.0f, 16.0f },
        { 6.261f, 16.017f },
        { 6.518f, 16.068f },
        { 6.765f, 16.152f },
        { 7.0f, 16.268f },
        { 7.218f, 16.413f },
        { 7.414f, 16.586f },
        { 7.587f, 16.782f },
        { 7.732f, 17.0f },
        { 7.848f, 17.235f },
        { 7.932f, 17.482f },
        { 7.983f, 17.739f },
        { 8.0f, 18.0f },
        { 8.0f, 21.0f }
    };
    rcIconDrawPolyline(bounds, path2,
                       15,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path3[] = {
        { 16.0f, 21.0f },
        { 16.0f, 18.0f },
        { 16.017f, 17.739f },
        { 16.068f, 17.482f },
        { 16.152f, 17.235f },
        { 16.268f, 17.0f },
        { 16.413f, 16.782f },
        { 16.586f, 16.586f },
        { 16.782f, 16.413f },
        { 17.0f, 16.268f },
        { 17.235f, 16.152f },
        { 17.482f, 16.068f },
        { 17.739f, 16.017f },
        { 18.0f, 16.0f },
        { 21.0f, 16.0f }
    };
    rcIconDrawPolyline(bounds, path3,
                       15,
                       viewBox, stroke, false, color);
}

static inline void rcIconMinimize(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconMinimize, NULL);
}

#endif /* RC_ICON_MINIMIZE_H */
