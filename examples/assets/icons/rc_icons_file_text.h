#ifndef RC_ICON_FILE_TEXT_H
#define RC_ICON_FILE_TEXT_H

#include "rc_icons_common.h"

/*
    Generated from file-text.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconFileText(RC_BoundingBox bounds,
                                      RC_Color color,
                                      const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 6.0f, 22.0f },
        { 5.739f, 21.983f },
        { 5.482f, 21.932f },
        { 5.235f, 21.848f },
        { 5.0f, 21.732f },
        { 4.782f, 21.587f },
        { 4.586f, 21.414f },
        { 4.413f, 21.218f },
        { 4.268f, 21.0f },
        { 4.152f, 20.765f },
        { 4.068f, 20.518f },
        { 4.017f, 20.261f },
        { 4.0f, 20.0f },
        { 4.0f, 4.0f },
        { 4.017f, 3.739f },
        { 4.068f, 3.482f },
        { 4.152f, 3.235f },
        { 4.268f, 3.0f },
        { 4.413f, 2.782f },
        { 4.586f, 2.586f },
        { 4.782f, 2.413f },
        { 5.0f, 2.268f },
        { 5.235f, 2.152f },
        { 5.482f, 2.068f },
        { 5.739f, 2.017f },
        { 6.0f, 2.0f },
        { 14.0f, 2.0f },
        { 14.27f, 2.015f },
        { 14.536f, 2.06f },
        { 14.796f, 2.135f },
        { 15.046f, 2.238f },
        { 15.282f, 2.369f },
        { 15.503f, 2.526f },
        { 15.704f, 2.706f },
        { 19.292f, 6.294f },
        { 19.473f, 6.495f },
        { 19.63f, 6.716f },
        { 19.761f, 6.953f },
        { 19.865f, 7.203f },
        { 19.94f, 7.463f },
        { 19.985f, 7.73f },
        { 20.0f, 8.0f },
        { 20.0f, 20.0f },
        { 19.983f, 20.261f },
        { 19.932f, 20.518f },
        { 19.848f, 20.765f },
        { 19.732f, 21.0f },
        { 19.587f, 21.218f },
        { 19.414f, 21.414f },
        { 19.218f, 21.587f },
        { 19.0f, 21.732f },
        { 18.765f, 21.848f },
        { 18.518f, 21.932f },
        { 18.261f, 21.983f },
        { 18.0f, 22.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       55,
                       viewBox, stroke, true, color);

    static const RC_IconPoint path1[] = {
        { 14.0f, 2.0f },
        { 14.0f, 7.0f },
        { 14.009f, 7.131f },
        { 14.034f, 7.259f },
        { 14.076f, 7.383f },
        { 14.134f, 7.5f },
        { 14.207f, 7.609f },
        { 14.293f, 7.707f },
        { 14.391f, 7.793f },
        { 14.5f, 7.866f },
        { 14.617f, 7.924f },
        { 14.741f, 7.966f },
        { 14.869f, 7.991f },
        { 15.0f, 8.0f },
        { 20.0f, 8.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       15,
                       viewBox, stroke, false, color);

    rcIconDrawRoundLine(bounds, 10.0f, 9.0f, 8.0f, 9.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 16.0f, 13.0f, 8.0f, 13.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 16.0f, 17.0f, 8.0f, 17.0f, viewBox, stroke, color);
}

static inline void rcIconFileText(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconFileText, NULL);
}

#endif /* RC_ICON_FILE_TEXT_H */
