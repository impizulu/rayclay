#ifndef RC_ICON_FLAG_H
#define RC_ICON_FLAG_H

#include "rc_icons_common.h"

/*
    Generated from flag.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconFlag(RC_BoundingBox bounds,
                                  RC_Color color,
                                  const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 4.0f, 22.0f },
        { 4.0f, 4.0f },
        { 4.007f, 3.884f },
        { 4.027f, 3.77f },
        { 4.06f, 3.659f },
        { 4.106f, 3.553f },
        { 4.163f, 3.452f },
        { 4.232f, 3.359f },
        { 4.312f, 3.275f },
        { 4.4f, 3.2f },
        { 5.046f, 2.778f },
        { 5.741f, 2.442f },
        { 6.473f, 2.198f },
        { 7.23f, 2.05f },
        { 8.0f, 2.0f },
        { 8.551f, 2.022f },
        { 9.081f, 2.086f },
        { 9.591f, 2.185f },
        { 10.083f, 2.312f },
        { 10.56f, 2.464f },
        { 11.023f, 2.633f },
        { 11.475f, 2.813f },
        { 11.917f, 3.0f },
        { 12.351f, 3.187f },
        { 12.779f, 3.367f },
        { 13.203f, 3.536f },
        { 13.625f, 3.688f },
        { 14.047f, 3.815f },
        { 14.471f, 3.914f },
        { 14.899f, 3.978f },
        { 15.333f, 4.0f },
        { 15.579f, 3.997f },
        { 15.818f, 3.987f },
        { 16.05f, 3.972f },
        { 16.275f, 3.95f },
        { 16.492f, 3.922f },
        { 16.702f, 3.888f },
        { 16.904f, 3.847f },
        { 17.1f, 3.8f },
        { 17.288f, 3.747f },
        { 17.469f, 3.688f },
        { 17.642f, 3.622f },
        { 17.808f, 3.55f },
        { 17.967f, 3.472f },
        { 18.119f, 3.388f },
        { 18.263f, 3.297f },
        { 18.4f, 3.2f },
        { 18.509f, 3.129f },
        { 18.626f, 3.072f },
        { 18.75f, 3.032f },
        { 18.878f, 3.007f },
        { 19.008f, 3.0f },
        { 19.138f, 3.01f },
        { 19.265f, 3.036f },
        { 19.388f, 3.078f },
        { 19.504f, 3.137f },
        { 19.612f, 3.209f },
        { 19.71f, 3.296f },
        { 19.795f, 3.394f },
        { 19.867f, 3.502f },
        { 19.925f, 3.619f },
        { 19.966f, 3.742f },
        { 19.992f, 3.87f },
        { 20.0f, 4.0f },
        { 20.0f, 14.0f },
        { 19.993f, 14.116f },
        { 19.973f, 14.23f },
        { 19.94f, 14.341f },
        { 19.894f, 14.447f },
        { 19.837f, 14.548f },
        { 19.768f, 14.641f },
        { 19.688f, 14.725f },
        { 19.6f, 14.8f },
        { 18.954f, 15.222f },
        { 18.259f, 15.558f },
        { 17.527f, 15.802f },
        { 16.77f, 15.95f },
        { 16.0f, 16.0f },
        { 15.449f, 15.978f },
        { 14.918f, 15.914f },
        { 14.405f, 15.815f },
        { 13.906f, 15.688f },
        { 13.419f, 15.536f },
        { 12.941f, 15.367f },
        { 12.469f, 15.187f },
        { 12.0f, 15.0f },
        { 11.531f, 14.813f },
        { 11.059f, 14.633f },
        { 10.581f, 14.464f },
        { 10.094f, 14.312f },
        { 9.595f, 14.185f },
        { 9.082f, 14.086f },
        { 8.551f, 14.022f },
        { 8.0f, 14.0f },
        { 7.272f, 14.044f },
        { 6.555f, 14.177f },
        { 5.859f, 14.395f },
        { 5.195f, 14.696f },
        { 4.572f, 15.076f },
        { 4.0f, 15.528f }
    };
    rcIconDrawPolyline(bounds, path0,
                       100,
                       viewBox, stroke, false, color);
}

static inline void rcIconFlag(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconFlag, NULL);
}

#endif /* RC_ICON_FLAG_H */
