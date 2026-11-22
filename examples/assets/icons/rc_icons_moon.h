#ifndef RC_ICON_MOON_H
#define RC_ICON_MOON_H

#include "rc_icons_common.h"

/*
    Generated from moon.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconMoon(RC_BoundingBox bounds,
                                  RC_Color color,
                                  const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 20.985f, 12.486f },
        { 20.85f, 13.628f },
        { 20.57f, 14.744f },
        { 20.15f, 15.815f },
        { 19.597f, 16.824f },
        { 18.92f, 17.753f },
        { 18.129f, 18.589f },
        { 17.239f, 19.318f },
        { 16.263f, 19.926f },
        { 15.217f, 20.406f },
        { 14.119f, 20.747f },
        { 12.986f, 20.947f },
        { 11.837f, 20.999f },
        { 10.69f, 20.905f },
        { 9.565f, 20.666f },
        { 8.48f, 20.285f },
        { 7.452f, 19.768f },
        { 6.498f, 19.125f },
        { 5.634f, 18.365f },
        { 4.875f, 17.501f },
        { 4.231f, 16.548f },
        { 3.715f, 15.52f },
        { 3.333f, 14.435f },
        { 3.094f, 13.31f },
        { 3.0f, 12.163f },
        { 3.052f, 11.014f },
        { 3.251f, 9.881f },
        { 3.593f, 8.783f },
        { 4.072f, 7.737f },
        { 4.681f, 6.761f },
        { 5.409f, 5.87f },
        { 6.245f, 5.08f },
        { 7.174f, 4.403f },
        { 8.183f, 3.849f },
        { 9.254f, 3.429f },
        { 10.37f, 3.149f },
        { 11.512f, 3.014f },
        { 11.586f, 3.016f },
        { 11.654f, 3.028f },
        { 11.718f, 3.051f },
        { 11.776f, 3.082f },
        { 11.828f, 3.121f },
        { 11.874f, 3.168f },
        { 11.913f, 3.221f },
        { 11.945f, 3.279f },
        { 11.971f, 3.341f },
        { 11.988f, 3.406f },
        { 11.998f, 3.474f },
        { 11.999f, 3.544f },
        { 11.991f, 3.614f },
        { 11.975f, 3.683f },
        { 11.949f, 3.751f },
        { 11.914f, 3.817f },
        { 11.55f, 4.493f },
        { 11.275f, 5.209f },
        { 11.093f, 5.955f },
        { 11.008f, 6.718f },
        { 11.022f, 7.485f },
        { 11.133f, 8.244f },
        { 11.34f, 8.983f },
        { 11.64f, 9.69f },
        { 12.028f, 10.352f },
        { 12.497f, 10.96f },
        { 13.039f, 11.502f },
        { 13.647f, 11.971f },
        { 14.309f, 12.359f },
        { 15.016f, 12.659f },
        { 15.755f, 12.866f },
        { 16.514f, 12.977f },
        { 17.281f, 12.991f },
        { 18.044f, 12.906f },
        { 18.79f, 12.724f },
        { 19.506f, 12.449f },
        { 20.182f, 12.085f },
        { 20.248f, 12.05f },
        { 20.316f, 12.024f },
        { 20.386f, 12.007f },
        { 20.456f, 12.0f },
        { 20.525f, 12.001f },
        { 20.593f, 12.011f },
        { 20.659f, 12.028f },
        { 20.721f, 12.053f },
        { 20.779f, 12.085f },
        { 20.831f, 12.124f },
        { 20.878f, 12.17f },
        { 20.917f, 12.222f },
        { 20.949f, 12.28f },
        { 20.971f, 12.344f },
        { 20.983f, 12.412f },
        { 20.985f, 12.486f }
    };
    rcIconDrawPolyline(bounds, path0,
                       90,
                       viewBox, stroke, false, color);
}

static inline void rcIconMoon(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconMoon, NULL);
}

#endif /* RC_ICON_MOON_H */
