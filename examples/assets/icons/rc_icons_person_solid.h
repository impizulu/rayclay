#ifndef RC_ICON_PERSON_SOLID_H
#define RC_ICON_PERSON_SOLID_H

#include "rc_icons_common.h"

/*
    Generated from person-solid.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconPersonSolid(RC_BoundingBox bounds,
                                         RC_Color color,
                                         const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;

    rcIconDrawFilledCircle(bounds, 12.0f, 9.2f, 4.3f, viewBox, color);

    static const RC_IconPoint path0[] = {
        { 18.67f, 21.98f },
        { 17.31f, 22.76f },
        { 15.11f, 23.59f },
        { 13.57f, 23.9f },
        { 12.0f, 24.0f },
        { 10.43f, 23.9f },
        { 8.14f, 23.36f },
        { 6.69f, 22.76f },
        { 5.33f, 21.98f },
        { 3.86f, 20.8f },
        { 4.55f, 19.3f },
        { 5.18f, 18.39f },
        { 6.33f, 17.18f },
        { 7.22f, 16.52f },
        { 9.24f, 15.55f },
        { 10.88f, 15.17f },
        { 12.56f, 15.12f },
        { 14.23f, 15.39f },
        { 15.8f, 15.97f },
        { 17.67f, 17.18f },
        { 18.47f, 17.96f },
        { 19.45f, 19.3f },
        { 20.14f, 20.8f }
    };
    rcIconDrawFilledPolygon(bounds, path0,
                            23,
                            viewBox, color);
}

static inline void rcIconPersonSolid(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconPersonSolid, NULL);
}

#endif /* RC_ICON_PERSON_SOLID_H */
