#ifndef RC_ICON_INBOX_H
#define RC_ICON_INBOX_H

#include "rc_icons_common.h"

/*
    Generated from inbox.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconInbox(RC_BoundingBox bounds,
                                   RC_Color color,
                                   const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 22.0f, 12.0f },
        { 16.0f, 12.0f },
        { 14.0f, 15.0f },
        { 10.0f, 15.0f },
        { 8.0f, 12.0f },
        { 2.0f, 12.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       6,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 5.45f, 5.11f },
        { 2.0f, 12.0f },
        { 2.0f, 18.0f },
        { 2.017f, 18.261f },
        { 2.068f, 18.518f },
        { 2.152f, 18.765f },
        { 2.268f, 19.0f },
        { 2.413f, 19.218f },
        { 2.586f, 19.414f },
        { 2.782f, 19.587f },
        { 3.0f, 19.732f },
        { 3.235f, 19.848f },
        { 3.482f, 19.932f },
        { 3.739f, 19.983f },
        { 4.0f, 20.0f },
        { 20.0f, 20.0f },
        { 20.261f, 19.983f },
        { 20.518f, 19.932f },
        { 20.765f, 19.848f },
        { 21.0f, 19.732f },
        { 21.218f, 19.587f },
        { 21.414f, 19.414f },
        { 21.587f, 19.218f },
        { 21.732f, 19.0f },
        { 21.848f, 18.765f },
        { 21.932f, 18.518f },
        { 21.983f, 18.261f },
        { 22.0f, 18.0f },
        { 22.0f, 12.0f },
        { 18.55f, 5.11f },
        { 18.427f, 4.897f },
        { 18.279f, 4.7f },
        { 18.107f, 4.523f },
        { 17.916f, 4.368f },
        { 17.706f, 4.239f },
        { 17.483f, 4.136f },
        { 17.248f, 4.061f },
        { 17.006f, 4.015f },
        { 16.76f, 4.0f },
        { 7.24f, 4.0f },
        { 6.994f, 4.015f },
        { 6.752f, 4.061f },
        { 6.517f, 4.136f },
        { 6.294f, 4.239f },
        { 6.084f, 4.368f },
        { 5.893f, 4.523f },
        { 5.721f, 4.7f },
        { 5.573f, 4.897f }
    };
    rcIconDrawPolyline(bounds, path1,
                       48,
                       viewBox, stroke, true, color);
}

static inline void rcIconInbox(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconInbox, NULL);
}

#endif /* RC_ICON_INBOX_H */
