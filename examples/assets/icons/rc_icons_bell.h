#ifndef RC_ICON_BELL_H
#define RC_ICON_BELL_H

#include "rc_icons_common.h"

/*
    Generated from bell.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconBell(RC_BoundingBox bounds,
                                  RC_Color color,
                                  const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 10.268f, 21.0f },
        { 10.413f, 21.218f },
        { 10.586f, 21.414f },
        { 10.783f, 21.587f },
        { 11.0f, 21.732f },
        { 11.235f, 21.848f },
        { 11.482f, 21.932f },
        { 11.739f, 21.983f },
        { 12.0f, 22.0f },
        { 12.261f, 21.983f },
        { 12.518f, 21.932f },
        { 12.765f, 21.848f },
        { 13.0f, 21.732f },
        { 13.217f, 21.587f },
        { 13.414f, 21.414f },
        { 13.587f, 21.218f },
        { 13.732f, 21.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       (int)(sizeof(path0) / sizeof(path0[0])),
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 3.262f, 15.326f },
        { 3.182f, 15.426f },
        { 3.115f, 15.536f },
        { 3.063f, 15.653f },
        { 3.026f, 15.776f },
        { 3.006f, 15.902f },
        { 3.001f, 16.03f },
        { 3.013f, 16.158f },
        { 3.042f, 16.283f },
        { 3.086f, 16.403f },
        { 3.145f, 16.517f },
        { 3.218f, 16.622f },
        { 3.304f, 16.717f },
        { 3.402f, 16.801f },
        { 3.509f, 16.871f },
        { 3.625f, 16.927f },
        { 3.746f, 16.967f },
        { 3.872f, 16.992f },
        { 4.0f, 17.0f },
        { 20.0f, 17.0f },
        { 20.128f, 16.992f },
        { 20.254f, 16.967f },
        { 20.375f, 16.927f },
        { 20.491f, 16.871f },
        { 20.598f, 16.801f },
        { 20.696f, 16.718f },
        { 20.782f, 16.623f },
        { 20.856f, 16.518f },
        { 20.915f, 16.405f },
        { 20.959f, 16.284f },
        { 20.988f, 16.159f },
        { 21.0f, 16.032f },
        { 20.996f, 15.903f },
        { 20.975f, 15.777f },
        { 20.939f, 15.654f },
        { 20.887f, 15.537f },
        { 20.82f, 15.427f },
        { 20.74f, 15.327f },
        { 20.49f, 15.068f },
        { 20.24f, 14.803f },
        { 19.993f, 14.527f },
        { 19.751f, 14.236f },
        { 19.515f, 13.926f },
        { 19.289f, 13.592f },
        { 19.073f, 13.231f },
        { 18.871f, 12.836f },
        { 18.685f, 12.406f },
        { 18.516f, 11.934f },
        { 18.368f, 11.417f },
        { 18.241f, 10.85f },
        { 18.139f, 10.229f },
        { 18.063f, 9.55f },
        { 18.016f, 8.809f },
        { 18.0f, 8.0f },
        { 17.949f, 7.217f },
        { 17.796f, 6.447f },
        { 17.543f, 5.704f },
        { 17.196f, 5.0f },
        { 16.76f, 4.347f },
        { 16.243f, 3.757f },
        { 15.653f, 3.24f },
        { 15.0f, 2.804f },
        { 14.296f, 2.457f },
        { 13.553f, 2.204f },
        { 12.783f, 2.051f },
        { 12.0f, 2.0f },
        { 11.217f, 2.051f },
        { 10.447f, 2.204f },
        { 9.704f, 2.457f },
        { 9.0f, 2.804f },
        { 8.347f, 3.24f },
        { 7.757f, 3.757f },
        { 7.24f, 4.347f },
        { 6.804f, 5.0f },
        { 6.457f, 5.704f },
        { 6.204f, 6.447f },
        { 6.051f, 7.217f },
        { 6.0f, 8.0f },
        { 5.984f, 8.809f },
        { 5.937f, 9.55f },
        { 5.861f, 10.229f },
        { 5.759f, 10.85f },
        { 5.632f, 11.417f },
        { 5.484f, 11.934f },
        { 5.315f, 12.406f },
        { 5.129f, 12.836f },
        { 4.927f, 13.23f },
        { 4.711f, 13.592f },
        { 4.485f, 13.926f },
        { 4.25f, 14.236f },
        { 4.007f, 14.527f },
        { 3.761f, 14.802f },
        { 3.511f, 15.067f },
        { 3.262f, 15.326f }
    };
    rcIconDrawPolyline(bounds, path1,
                       (int)(sizeof(path1) / sizeof(path1[0])),
                       viewBox, stroke, false, color);
}

static inline void rcIconBell(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconBell, NULL);
}

#endif /* RC_ICON_BELL_H */
