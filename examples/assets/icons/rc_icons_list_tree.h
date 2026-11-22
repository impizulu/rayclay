#ifndef RC_ICON_LIST_TREE_H
#define RC_ICON_LIST_TREE_H

#include "rc_icons_common.h"

/*
    Generated from list-tree.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconListTree(RC_BoundingBox bounds,
                                      RC_Color color,
                                      const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 21.0f, 12.0f, 13.0f, 12.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 21.0f, 6.0f, 8.0f, 6.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 21.0f, 18.0f, 13.0f, 18.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 3.0f, 6.0f },
        { 3.0f, 10.0f },
        { 3.01f, 10.204f },
        { 3.041f, 10.402f },
        { 3.09f, 10.593f },
        { 3.158f, 10.777f },
        { 3.242f, 10.951f },
        { 3.343f, 11.116f },
        { 3.458f, 11.27f },
        { 3.587f, 11.412f },
        { 3.73f, 11.542f },
        { 3.884f, 11.657f },
        { 4.049f, 11.758f },
        { 4.223f, 11.842f },
        { 4.407f, 11.91f },
        { 4.598f, 11.959f },
        { 4.796f, 11.99f },
        { 5.0f, 12.0f },
        { 8.0f, 12.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       19,
                       viewBox, stroke, false, color);

    static const RC_IconPoint path1[] = {
        { 3.0f, 10.0f },
        { 3.0f, 16.0f },
        { 3.01f, 16.204f },
        { 3.041f, 16.402f },
        { 3.09f, 16.593f },
        { 3.158f, 16.777f },
        { 3.242f, 16.951f },
        { 3.343f, 17.116f },
        { 3.458f, 17.27f },
        { 3.587f, 17.413f },
        { 3.73f, 17.542f },
        { 3.884f, 17.657f },
        { 4.049f, 17.758f },
        { 4.223f, 17.842f },
        { 4.407f, 17.91f },
        { 4.598f, 17.959f },
        { 4.796f, 17.99f },
        { 5.0f, 18.0f },
        { 8.0f, 18.0f }
    };
    rcIconDrawPolyline(bounds, path1,
                       19,
                       viewBox, stroke, false, color);
}

static inline void rcIconListTree(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconListTree, NULL);
}

#endif /* RC_ICON_LIST_TREE_H */
