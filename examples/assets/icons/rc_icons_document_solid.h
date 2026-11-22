#ifndef RC_ICON_DOCUMENT_SOLID_H
#define RC_ICON_DOCUMENT_SOLID_H

#include "rc_icons_common.h"

/*
    Generated from document-solid.svg (Lucide). The layout pass owns placement;
    the library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconDocumentSolid(RC_BoundingBox bounds,
                                           RC_Color color,
                                           const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;

    static const RC_IconPoint path0[] = {
        { 6.0f, 2.0f },
        { 13.5f, 2.0f },
        { 13.5f, 7.0f },
        { 13.513f, 7.196f },
        { 13.551f, 7.388f },
        { 13.614f, 7.574f },
        { 13.701f, 7.75f },
        { 13.81f, 7.913f },
        { 13.939f, 8.061f },
        { 14.087f, 8.19f },
        { 14.25f, 8.299f },
        { 14.426f, 8.386f },
        { 14.612f, 8.449f },
        { 14.804f, 8.487f },
        { 15.0f, 8.5f },
        { 20.0f, 8.5f },
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
        { 18.0f, 22.0f },
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
        { 5.739f, 2.017f }
    };
    rcIconDrawFilledPolygon(bounds, path0,
                            54,
                            viewBox, color);

    static const RC_IconPoint path1[] = {
        { 15.0f, 2.4f },
        { 19.6f, 7.0f },
        { 15.0f, 7.0f }
    };
    rcIconDrawFilledPolygon(bounds, path1,
                            3,
                            viewBox, color);
}

static inline void rcIconDocumentSolid(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconDocumentSolid, NULL);
}

#endif /* RC_ICON_DOCUMENT_SOLID_H */
