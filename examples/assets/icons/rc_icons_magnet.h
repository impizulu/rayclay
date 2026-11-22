#ifndef RC_ICON_MAGNET_H
#define RC_ICON_MAGNET_H

#include "rc_icons_common.h"

/*
    Generated from magnet.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconMagnet(RC_BoundingBox bounds,
                                    RC_Color color,
                                    const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 12.0f, 15.0f, 16.0f, 19.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 2.352f, 10.648f },
        { 2.248f, 10.767f },
        { 2.161f, 10.898f },
        { 2.091f, 11.039f },
        { 2.04f, 11.188f },
        { 2.009f, 11.343f },
        { 1.999f, 11.5f },
        { 2.009f, 11.657f },
        { 2.04f, 11.812f },
        { 2.091f, 11.961f },
        { 2.161f, 12.102f },
        { 2.248f, 12.233f },
        { 2.352f, 12.352f },
        { 4.648f, 14.648f },
        { 4.767f, 14.752f },
        { 4.898f, 14.839f },
        { 5.039f, 14.909f },
        { 5.188f, 14.96f },
        { 5.343f, 14.991f },
        { 5.5f, 15.001f },
        { 5.657f, 14.991f },
        { 5.812f, 14.96f },
        { 5.961f, 14.909f },
        { 6.102f, 14.839f },
        { 6.233f, 14.752f },
        { 6.352f, 14.648f },
        { 12.381f, 8.619f },
        { 12.59f, 8.436f },
        { 12.82f, 8.282f },
        { 13.069f, 8.159f },
        { 13.332f, 8.07f },
        { 13.604f, 8.016f },
        { 13.881f, 7.998f },
        { 14.158f, 8.016f },
        { 14.43f, 8.07f },
        { 14.693f, 8.159f },
        { 14.942f, 8.282f },
        { 15.172f, 8.436f },
        { 15.381f, 8.619f },
        { 15.564f, 8.828f },
        { 15.718f, 9.058f },
        { 15.841f, 9.307f },
        { 15.93f, 9.57f },
        { 15.984f, 9.842f },
        { 16.002f, 10.119f },
        { 15.984f, 10.396f },
        { 15.93f, 10.668f },
        { 15.841f, 10.931f },
        { 15.718f, 11.18f },
        { 15.564f, 11.41f },
        { 15.381f, 11.619f },
        { 9.352f, 17.648f },
        { 9.248f, 17.767f },
        { 9.161f, 17.898f },
        { 9.091f, 18.039f },
        { 9.04f, 18.188f },
        { 9.009f, 18.343f },
        { 8.999f, 18.5f },
        { 9.009f, 18.657f },
        { 9.04f, 18.812f },
        { 9.091f, 18.961f },
        { 9.161f, 19.102f },
        { 9.248f, 19.233f },
        { 9.352f, 19.352f },
        { 11.648f, 21.648f },
        { 11.767f, 21.752f },
        { 11.898f, 21.839f },
        { 12.039f, 21.909f },
        { 12.188f, 21.96f },
        { 12.343f, 21.991f },
        { 12.5f, 22.001f },
        { 12.657f, 21.991f },
        { 12.812f, 21.96f },
        { 12.961f, 21.909f },
        { 13.102f, 21.839f },
        { 13.233f, 21.752f },
        { 13.352f, 21.648f },
        { 19.717f, 15.281f },
        { 20.388f, 14.516f },
        { 20.953f, 13.67f },
        { 21.403f, 12.757f },
        { 21.73f, 11.794f },
        { 21.928f, 10.796f },
        { 21.995f, 9.781f },
        { 21.928f, 8.766f },
        { 21.729f, 7.768f },
        { 21.402f, 6.804f },
        { 20.952f, 5.892f },
        { 20.387f, 5.046f },
        { 19.716f, 4.281f },
        { 18.951f, 3.61f },
        { 18.105f, 3.045f },
        { 17.192f, 2.595f },
        { 16.229f, 2.268f },
        { 15.231f, 2.07f },
        { 14.216f, 2.003f },
        { 13.201f, 2.07f },
        { 12.203f, 2.269f },
        { 11.239f, 2.596f },
        { 10.327f, 3.046f },
        { 9.481f, 3.611f },
        { 8.716f, 4.282f }
    };
    rcIconDrawPolyline(bounds, path0,
                       102,
                       viewBox, stroke, true, color);

    rcIconDrawRoundLine(bounds, 5.0f, 8.0f, 9.0f, 12.0f, viewBox, stroke, color);
}

static inline void rcIconMagnet(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconMagnet, NULL);
}

#endif /* RC_ICON_MAGNET_H */
