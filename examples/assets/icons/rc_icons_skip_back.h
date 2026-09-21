#ifndef RC_ICON_SKIP_BACK_H
#define RC_ICON_SKIP_BACK_H

#include "rc_icons_common.h"

/*
    Generated from skip-back.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconSkipBack(RC_BoundingBox bounds,
                                      RC_Color color,
                                      const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 17.971f, 4.285f },
        { 18.191f, 4.171f },
        { 18.424f, 4.085f },
        { 18.666f, 4.028f },
        { 18.913f, 4.002f },
        { 19.161f, 4.006f },
        { 19.407f, 4.042f },
        { 19.646f, 4.107f },
        { 19.876f, 4.202f },
        { 20.092f, 4.324f },
        { 20.291f, 4.472f },
        { 20.47f, 4.644f },
        { 20.627f, 4.836f },
        { 20.758f, 5.047f },
        { 20.863f, 5.272f },
        { 20.939f, 5.508f },
        { 20.985f, 5.752f },
        { 21.0f, 6.0f },
        { 21.0f, 18.0f },
        { 20.985f, 18.248f },
        { 20.939f, 18.492f },
        { 20.863f, 18.728f },
        { 20.758f, 18.953f },
        { 20.627f, 19.164f },
        { 20.47f, 19.356f },
        { 20.291f, 19.528f },
        { 20.092f, 19.676f },
        { 19.876f, 19.798f },
        { 19.646f, 19.893f },
        { 19.407f, 19.958f },
        { 19.161f, 19.994f },
        { 18.913f, 19.998f },
        { 18.666f, 19.972f },
        { 18.424f, 19.915f },
        { 18.191f, 19.829f },
        { 17.971f, 19.715f },
        { 7.974f, 13.717f },
        { 7.762f, 13.571f },
        { 7.57f, 13.399f },
        { 7.402f, 13.203f },
        { 7.261f, 12.988f },
        { 7.148f, 12.756f },
        { 7.066f, 12.512f },
        { 7.017f, 12.259f },
        { 7.0f, 12.002f },
        { 7.016f, 11.745f },
        { 7.065f, 11.492f },
        { 7.147f, 11.247f },
        { 7.259f, 11.015f },
        { 7.4f, 10.8f },
        { 7.568f, 10.604f },
        { 7.759f, 10.431f },
        { 7.971f, 10.285f }
    };
    rcIconDrawPolyline(bounds, path0,
                       (int)(sizeof(path0) / sizeof(path0[0])),
                       viewBox, stroke, true, color);

    rcIconDrawRoundLine(bounds, 3.0f, 20.0f, 3.0f, 4.0f, viewBox, stroke, color);
}

static inline void rcIconSkipBack(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconSkipBack, NULL);
}

#endif /* RC_ICON_SKIP_BACK_H */
