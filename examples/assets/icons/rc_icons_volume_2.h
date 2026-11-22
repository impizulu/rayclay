#ifndef RC_ICON_VOLUME_2_H
#define RC_ICON_VOLUME_2_H

#include "rc_icons_common.h"

/*
    Generated from volume-2.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconVolume2(RC_BoundingBox bounds,
                                     RC_Color color,
                                     const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 11.0f, 4.702f },
        { 10.994f, 4.61f },
        { 10.976f, 4.52f },
        { 10.946f, 4.433f },
        { 10.905f, 4.35f },
        { 10.854f, 4.273f },
        { 10.793f, 4.204f },
        { 10.724f, 4.143f },
        { 10.647f, 4.092f },
        { 10.565f, 4.052f },
        { 10.477f, 4.022f },
        { 10.387f, 4.004f },
        { 10.295f, 3.998f },
        { 10.203f, 4.004f },
        { 10.113f, 4.022f },
        { 10.026f, 4.052f },
        { 9.943f, 4.092f },
        { 9.866f, 4.143f },
        { 9.797f, 4.204f },
        { 6.413f, 7.587f },
        { 6.295f, 7.693f },
        { 6.166f, 7.784f },
        { 6.028f, 7.861f },
        { 5.882f, 7.922f },
        { 5.73f, 7.965f },
        { 5.574f, 7.992f },
        { 5.416f, 8.0f },
        { 3.0f, 8.0f },
        { 2.869f, 8.009f },
        { 2.741f, 8.034f },
        { 2.617f, 8.076f },
        { 2.5f, 8.134f },
        { 2.391f, 8.207f },
        { 2.293f, 8.293f },
        { 2.207f, 8.391f },
        { 2.134f, 8.5f },
        { 2.076f, 8.617f },
        { 2.034f, 8.741f },
        { 2.009f, 8.869f },
        { 2.0f, 9.0f },
        { 2.0f, 15.0f },
        { 2.009f, 15.131f },
        { 2.034f, 15.259f },
        { 2.076f, 15.383f },
        { 2.134f, 15.5f },
        { 2.207f, 15.609f },
        { 2.293f, 15.707f },
        { 2.391f, 15.793f },
        { 2.5f, 15.866f },
        { 2.617f, 15.924f },
        { 2.741f, 15.966f },
        { 2.869f, 15.991f },
        { 3.0f, 16.0f },
        { 5.416f, 16.0f },
        { 5.574f, 16.008f },
        { 5.73f, 16.035f },
        { 5.882f, 16.078f },
        { 6.028f, 16.139f },
        { 6.166f, 16.216f },
        { 6.295f, 16.307f },
        { 6.413f, 16.413f },
        { 9.796f, 19.797f },
        { 9.862f, 19.855f },
        { 9.934f, 19.904f },
        { 10.011f, 19.944f },
        { 10.094f, 19.975f },
        { 10.179f, 19.994f },
        { 10.266f, 20.003f },
        { 10.353f, 20.002f },
        { 10.44f, 19.989f },
        { 10.524f, 19.966f },
        { 10.605f, 19.932f },
        { 10.681f, 19.889f },
        { 10.751f, 19.837f },
        { 10.814f, 19.776f },
        { 10.869f, 19.708f },
        { 10.915f, 19.634f },
        { 10.952f, 19.555f },
        { 10.979f, 19.471f },
        { 10.995f, 19.385f },
        { 11.0f, 19.298f }
    };
    rcIconDrawPolyline(bounds, path0,
                       81,
                       viewBox, stroke, true, color);

    static const RC_IconPoint path1[] = {
        { 16.0f, 9.0f },
        { 16.352f, 9.538f },
        { 16.632f, 10.117f },
        { 16.835f, 10.727f },
        { 16.959f, 11.358f },
        { 17.0f, 12.0f },
        { 16.959f, 12.642f },
        { 16.835f, 13.273f },
        { 16.632f, 13.883f },
        { 16.352f, 14.462f },
        { 16.0f, 15.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       11,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path2[] = {
        { 19.364f, 18.364f },
        { 20.085f, 17.551f },
        { 20.702f, 16.656f },
        { 21.207f, 15.694f },
        { 21.593f, 14.678f },
        { 21.853f, 13.622f },
        { 21.984f, 12.543f },
        { 21.984f, 11.457f },
        { 21.853f, 10.378f },
        { 21.593f, 9.322f },
        { 21.207f, 8.306f },
        { 20.702f, 7.344f },
        { 20.085f, 6.449f },
        { 19.364f, 5.636f }
    };
    rcIconDrawPolyline(bounds, path2,
                       14,
                       viewBox, stroke, false, color);
}

static inline void rcIconVolume2(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconVolume2, NULL);
}

#endif /* RC_ICON_VOLUME_2_H */
