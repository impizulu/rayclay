#ifndef RC_ICON_FOLDER_SOLID_H
#define RC_ICON_FOLDER_SOLID_H

#include "rc_icons_common.h"

/*
    Generated from folder-solid.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconFolderSolid(RC_BoundingBox bounds,
                                         RC_Color color,
                                         const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;

    static const RC_IconPoint path0[] = {
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
        { 22.0f, 8.0f },
        { 21.983f, 7.739f },
        { 21.932f, 7.482f },
        { 21.848f, 7.235f },
        { 21.732f, 7.0f },
        { 21.587f, 6.782f },
        { 21.414f, 6.586f },
        { 21.218f, 6.413f },
        { 21.0f, 6.268f },
        { 20.765f, 6.152f },
        { 20.518f, 6.068f },
        { 20.261f, 6.017f },
        { 20.0f, 6.0f },
        { 12.1f, 6.0f },
        { 11.851f, 5.987f },
        { 11.605f, 5.943f },
        { 11.367f, 5.869f },
        { 11.14f, 5.765f },
        { 10.928f, 5.635f },
        { 10.733f, 5.478f },
        { 10.56f, 5.299f },
        { 10.41f, 5.1f },
        { 9.6f, 3.9f },
        { 9.452f, 3.703f },
        { 9.28f, 3.525f },
        { 9.088f, 3.37f },
        { 8.878f, 3.239f },
        { 8.654f, 3.136f },
        { 8.419f, 3.061f },
        { 8.176f, 3.015f },
        { 7.93f, 3.0f },
        { 4.0f, 3.0f },
        { 3.739f, 3.017f },
        { 3.482f, 3.068f },
        { 3.235f, 3.152f },
        { 3.0f, 3.268f },
        { 2.782f, 3.413f },
        { 2.586f, 3.586f },
        { 2.413f, 3.782f },
        { 2.268f, 4.0f },
        { 2.152f, 4.235f },
        { 2.068f, 4.482f },
        { 2.017f, 4.739f },
        { 2.0f, 5.0f },
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
        { 4.0f, 20.0f }
    };
    rcIconDrawFilledPolygon(bounds, path0,
                            70,
                            viewBox, color);
}

static inline void rcIconFolderSolid(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconFolderSolid, NULL);
}

#endif /* RC_ICON_FOLDER_SOLID_H */
