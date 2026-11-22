#ifndef RC_ICON_GIT_PULL_REQUEST_H
#define RC_ICON_GIT_PULL_REQUEST_H

#include "rc_icons_common.h"

/*
    Generated from git-pull-request.svg (Lucide). The layout pass owns placement;
    the library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconGitPullRequest(RC_BoundingBox bounds,
                                            RC_Color color,
                                            const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawCircleStroke(bounds, 18.0f, 18.0f, 3.0f, viewBox, stroke, color);
    rcIconDrawCircleStroke(bounds, 6.0f, 6.0f, 3.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 13.0f, 6.0f },
        { 16.0f, 6.0f },
        { 16.261f, 6.017f },
        { 16.518f, 6.068f },
        { 16.765f, 6.152f },
        { 17.0f, 6.268f },
        { 17.218f, 6.413f },
        { 17.414f, 6.586f },
        { 17.587f, 6.782f },
        { 17.732f, 7.0f },
        { 17.848f, 7.235f },
        { 17.932f, 7.482f },
        { 17.983f, 7.739f },
        { 18.0f, 8.0f },
        { 18.0f, 15.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       15,
                       viewBox, stroke, false, color);

    rcIconDrawRoundLine(bounds, 6.0f, 9.0f, 6.0f, 21.0f, viewBox, stroke, color);
}

static inline void rcIconGitPullRequest(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconGitPullRequest, NULL);
}

#endif /* RC_ICON_GIT_PULL_REQUEST_H */
