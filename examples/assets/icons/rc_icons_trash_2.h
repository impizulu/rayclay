#ifndef RC_ICON_TRASH_2_H
#define RC_ICON_TRASH_2_H

#include "rc_icons_common.h"

/*
    Generated from trash-2.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconTrash2(RC_BoundingBox bounds,
                                    RC_Color color,
                                    const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 10.0f, 11.0f, 10.0f, 17.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 14.0f, 11.0f, 14.0f, 17.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 19.0f, 6.0f },
        { 19.0f, 20.0f },
        { 18.983f, 20.261f },
        { 18.932f, 20.518f },
        { 18.848f, 20.765f },
        { 18.732f, 21.0f },
        { 18.587f, 21.218f },
        { 18.414f, 21.414f },
        { 18.218f, 21.587f },
        { 18.0f, 21.732f },
        { 17.765f, 21.848f },
        { 17.518f, 21.932f },
        { 17.261f, 21.983f },
        { 17.0f, 22.0f },
        { 7.0f, 22.0f },
        { 6.739f, 21.983f },
        { 6.482f, 21.932f },
        { 6.235f, 21.848f },
        { 6.0f, 21.732f },
        { 5.782f, 21.587f },
        { 5.586f, 21.414f },
        { 5.413f, 21.218f },
        { 5.268f, 21.0f },
        { 5.152f, 20.765f },
        { 5.068f, 20.518f },
        { 5.017f, 20.261f },
        { 5.0f, 20.0f },
        { 5.0f, 6.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       28,
                       viewBox, stroke, false, color);

    rcIconDrawRoundLine(bounds, 3.0f, 6.0f, 21.0f, 6.0f, viewBox, stroke, color);

    static const RC_IconPoint path1[] = {
        { 8.0f, 6.0f },
        { 8.0f, 4.0f },
        { 8.017f, 3.739f },
        { 8.068f, 3.482f },
        { 8.152f, 3.235f },
        { 8.268f, 3.0f },
        { 8.413f, 2.782f },
        { 8.586f, 2.586f },
        { 8.782f, 2.413f },
        { 9.0f, 2.268f },
        { 9.235f, 2.152f },
        { 9.482f, 2.068f },
        { 9.739f, 2.017f },
        { 10.0f, 2.0f },
        { 14.0f, 2.0f },
        { 14.261f, 2.017f },
        { 14.518f, 2.068f },
        { 14.765f, 2.152f },
        { 15.0f, 2.268f },
        { 15.218f, 2.413f },
        { 15.414f, 2.586f },
        { 15.587f, 2.782f },
        { 15.732f, 3.0f },
        { 15.848f, 3.235f },
        { 15.932f, 3.482f },
        { 15.983f, 3.739f },
        { 16.0f, 4.0f },
        { 16.0f, 6.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       28,
                       viewBox, stroke, false, color);
}

static inline void rcIconTrash2(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconTrash2, NULL);
}

#endif /* RC_ICON_TRASH_2_H */
