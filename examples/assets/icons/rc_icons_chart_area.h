#ifndef RC_ICON_CHART_AREA_H
#define RC_ICON_CHART_AREA_H

#include "rc_icons_common.h"

/*
    Generated from chart-area.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconChartArea(RC_BoundingBox bounds,
                                       RC_Color color,
                                       const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 3.0f, 3.0f },
        { 3.0f, 19.0f },
        { 3.017f, 19.261f },
        { 3.068f, 19.518f },
        { 3.152f, 19.765f },
        { 3.268f, 20.0f },
        { 3.413f, 20.218f },
        { 3.586f, 20.414f },
        { 3.782f, 20.587f },
        { 4.0f, 20.732f },
        { 4.235f, 20.848f },
        { 4.482f, 20.932f },
        { 4.739f, 20.983f },
        { 5.0f, 21.0f },
        { 21.0f, 21.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       15,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 7.0f, 11.207f },
        { 7.004f, 11.142f },
        { 7.017f, 11.078f },
        { 7.038f, 11.016f },
        { 7.067f, 10.957f },
        { 7.103f, 10.903f },
        { 7.146f, 10.854f },
        { 9.146f, 8.854f },
        { 9.191f, 8.814f },
        { 9.241f, 8.779f },
        { 9.294f, 8.751f },
        { 9.351f, 8.73f },
        { 9.41f, 8.715f },
        { 9.47f, 8.708f },
        { 9.53f, 8.708f },
        { 9.59f, 8.715f },
        { 9.649f, 8.73f },
        { 9.706f, 8.751f },
        { 9.759f, 8.779f },
        { 9.809f, 8.814f },
        { 9.854f, 8.854f },
        { 13.146f, 12.146f },
        { 13.191f, 12.186f },
        { 13.241f, 12.221f },
        { 13.294f, 12.249f },
        { 13.351f, 12.27f },
        { 13.41f, 12.285f },
        { 13.47f, 12.292f },
        { 13.53f, 12.292f },
        { 13.59f, 12.285f },
        { 13.649f, 12.27f },
        { 13.706f, 12.249f },
        { 13.759f, 12.221f },
        { 13.809f, 12.186f },
        { 13.854f, 12.146f },
        { 18.146f, 7.854f },
        { 18.192f, 7.813f },
        { 18.244f, 7.778f },
        { 18.299f, 7.749f },
        { 18.357f, 7.728f },
        { 18.417f, 7.714f },
        { 18.479f, 7.708f },
        { 18.541f, 7.709f },
        { 18.602f, 7.718f },
        { 18.662f, 7.734f },
        { 18.719f, 7.758f },
        { 18.773f, 7.788f },
        { 18.823f, 7.825f },
        { 18.868f, 7.868f },
        { 18.907f, 7.916f },
        { 18.94f, 7.969f },
        { 18.966f, 8.025f },
        { 18.985f, 8.084f },
        { 18.996f, 8.145f },
        { 19.0f, 8.207f },
        { 19.0f, 16.0f },
        { 18.991f, 16.131f },
        { 18.966f, 16.259f },
        { 18.924f, 16.383f },
        { 18.866f, 16.5f },
        { 18.793f, 16.609f },
        { 18.707f, 16.707f },
        { 18.609f, 16.793f },
        { 18.5f, 16.866f },
        { 18.383f, 16.924f },
        { 18.259f, 16.966f },
        { 18.131f, 16.991f },
        { 18.0f, 17.0f },
        { 8.0f, 17.0f },
        { 7.869f, 16.991f },
        { 7.741f, 16.966f },
        { 7.617f, 16.924f },
        { 7.5f, 16.866f },
        { 7.391f, 16.793f },
        { 7.293f, 16.707f },
        { 7.207f, 16.609f },
        { 7.134f, 16.5f },
        { 7.076f, 16.383f },
        { 7.034f, 16.259f },
        { 7.009f, 16.131f },
        { 7.0f, 16.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       81,
                       viewBox, stroke, true, color);
}

static inline void rcIconChartArea(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconChartArea, NULL);
}

#endif /* RC_ICON_CHART_AREA_H */
