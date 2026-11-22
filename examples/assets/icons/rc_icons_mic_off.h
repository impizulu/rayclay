#ifndef RC_ICON_MIC_OFF_H
#define RC_ICON_MIC_OFF_H

#include "rc_icons_common.h"

/*
    Generated from mic-off.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconMicOff(RC_BoundingBox bounds,
                                    RC_Color color,
                                    const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 12.0f, 19.0f, 12.0f, 22.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 15.0f, 9.34f },
        { 15.0f, 5.0f },
        { 14.974f, 4.62f },
        { 14.899f, 4.246f },
        { 14.778f, 3.885f },
        { 14.612f, 3.542f },
        { 14.403f, 3.223f },
        { 14.156f, 2.932f },
        { 13.875f, 2.676f },
        { 13.563f, 2.457f },
        { 13.225f, 2.28f },
        { 12.868f, 2.146f },
        { 12.497f, 2.06f },
        { 12.118f, 2.02f },
        { 11.737f, 2.03f },
        { 11.36f, 2.087f },
        { 10.994f, 2.192f },
        { 10.644f, 2.342f },
        { 10.315f, 2.536f },
        { 10.014f, 2.769f },
        { 9.745f, 3.039f },
        { 9.513f, 3.341f },
        { 9.32f, 3.67f }
    };
    rcIconDrawPolyline(bounds, path0,
                       23,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 16.95f, 16.95f },
        { 16.3f, 17.524f },
        { 15.583f, 18.014f },
        { 14.812f, 18.411f },
        { 13.998f, 18.709f },
        { 13.152f, 18.905f },
        { 12.289f, 18.995f },
        { 11.422f, 18.977f },
        { 10.563f, 18.852f },
        { 9.727f, 18.621f },
        { 8.926f, 18.289f },
        { 8.171f, 17.861f },
        { 7.476f, 17.342f },
        { 6.85f, 16.741f },
        { 6.303f, 16.068f },
        { 5.844f, 15.332f },
        { 5.479f, 14.545f },
        { 5.214f, 13.719f },
        { 5.054f, 12.866f },
        { 5.0f, 12.0f },
        { 5.0f, 10.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       21,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path2[] = {
        { 18.89f, 13.23f },
        { 18.938f, 12.925f },
        { 18.972f, 12.617f },
        { 18.993f, 12.309f },
        { 19.0f, 12.0f },
        { 19.0f, 10.0f }
    };
    rcIconDrawPolyline(bounds, path2,
                       6,
                       viewBox, stroke, false, color);

    rcIconDrawRoundLine(bounds, 2.0f, 2.0f, 22.0f, 22.0f, viewBox, stroke, color);

    static const RC_IconPoint path3[] = {
        { 9.0f, 9.0f },
        { 9.0f, 12.0f },
        { 9.026f, 12.391f },
        { 9.103f, 12.776f },
        { 9.229f, 13.147f },
        { 9.403f, 13.499f },
        { 9.621f, 13.825f },
        { 9.879f, 14.119f },
        { 10.174f, 14.378f },
        { 10.501f, 14.596f },
        { 10.852f, 14.769f },
        { 11.224f, 14.895f },
        { 11.608f, 14.972f },
        { 12.0f, 14.997f },
        { 12.391f, 14.972f },
        { 12.776f, 14.895f },
        { 13.147f, 14.769f },
        { 13.499f, 14.596f },
        { 13.825f, 14.378f },
        { 14.12f, 14.12f }
    };
    rcIconDrawPolyline(bounds, path3,
                       20,
                       viewBox, stroke, false, color);
}

static inline void rcIconMicOff(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconMicOff, NULL);
}

#endif /* RC_ICON_MIC_OFF_H */
