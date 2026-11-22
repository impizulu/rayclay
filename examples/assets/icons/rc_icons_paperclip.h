#ifndef RC_ICON_PAPERCLIP_H
#define RC_ICON_PAPERCLIP_H

#include "rc_icons_common.h"

/*
    Generated from paperclip.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconPaperclip(RC_BoundingBox bounds,
                                       RC_Color color,
                                       const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    static const RC_IconPoint path0[] = {
        { 16.0f, 6.0f },
        { 7.586f, 14.586f },
        { 7.413f, 14.783f },
        { 7.268f, 15.0f },
        { 7.152f, 15.235f },
        { 7.068f, 15.483f },
        { 7.017f, 15.739f },
        { 7.0f, 16.0f },
        { 7.017f, 16.262f },
        { 7.068f, 16.518f },
        { 7.152f, 16.766f },
        { 7.268f, 17.001f },
        { 7.413f, 17.218f },
        { 7.586f, 17.415f },
        { 7.783f, 17.588f },
        { 8.0f, 17.733f },
        { 8.235f, 17.849f },
        { 8.483f, 17.933f },
        { 8.739f, 17.984f },
        { 9.001f, 18.001f },
        { 9.262f, 17.984f },
        { 9.518f, 17.933f },
        { 9.766f, 17.849f },
        { 10.001f, 17.733f },
        { 10.218f, 17.588f },
        { 10.415f, 17.415f },
        { 18.829f, 8.829f },
        { 19.174f, 8.436f },
        { 19.465f, 8.001f },
        { 19.696f, 7.531f },
        { 19.864f, 7.036f },
        { 19.966f, 6.523f },
        { 20.001f, 6.0f },
        { 19.966f, 5.478f },
        { 19.864f, 4.965f },
        { 19.696f, 4.47f },
        { 19.465f, 4.0f },
        { 19.174f, 3.565f },
        { 18.829f, 3.172f },
        { 18.436f, 2.827f },
        { 18.001f, 2.536f },
        { 17.531f, 2.305f },
        { 17.036f, 2.137f },
        { 16.523f, 2.035f },
        { 16.001f, 2.0f },
        { 15.478f, 2.035f },
        { 14.965f, 2.137f },
        { 14.47f, 2.305f },
        { 14.0f, 2.536f },
        { 13.565f, 2.827f },
        { 13.172f, 3.172f },
        { 4.793f, 11.723f },
        { 4.288f, 12.287f },
        { 3.858f, 12.911f },
        { 3.509f, 13.583f },
        { 3.249f, 14.294f },
        { 3.079f, 15.032f },
        { 3.005f, 15.786f },
        { 3.026f, 16.543f },
        { 3.142f, 17.292f },
        { 3.351f, 18.02f },
        { 3.65f, 18.715f },
        { 4.035f, 19.368f },
        { 4.499f, 19.966f },
        { 5.035f, 20.502f },
        { 5.633f, 20.966f },
        { 6.286f, 21.351f },
        { 6.981f, 21.65f },
        { 7.709f, 21.859f },
        { 8.458f, 21.975f },
        { 9.215f, 21.996f },
        { 9.969f, 21.922f },
        { 10.707f, 21.752f },
        { 11.418f, 21.492f },
        { 12.09f, 21.143f },
        { 12.714f, 20.713f },
        { 13.278f, 20.208f },
        { 21.657f, 11.657f }
    };
    rcIconDrawPolyline(bounds, path0,
                       78,
                       viewBox, stroke, false, color);
}

static inline void rcIconPaperclip(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconPaperclip, NULL);
}

#endif /* RC_ICON_PAPERCLIP_H */
