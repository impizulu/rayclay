#ifndef RC_ICON_BOOK_OPEN_H
#define RC_ICON_BOOK_OPEN_H

#include "rc_icons_common.h"

/*
    Generated from book-open.svg (Lucide). The layout pass owns placement; the
    library draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconBookOpen(RC_BoundingBox bounds,
                                      RC_Color color,
                                      const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundLine(bounds, 12.0f, 7.0f, 12.0f, 21.0f, viewBox, stroke, color);

    static const RC_IconPoint path0[] = {
        { 3.0f, 18.0f },
        { 2.869f, 17.991f },
        { 2.741f, 17.966f },
        { 2.617f, 17.924f },
        { 2.5f, 17.866f },
        { 2.391f, 17.793f },
        { 2.293f, 17.707f },
        { 2.207f, 17.609f },
        { 2.134f, 17.5f },
        { 2.076f, 17.383f },
        { 2.034f, 17.259f },
        { 2.009f, 17.131f },
        { 2.0f, 17.0f },
        { 2.0f, 4.0f },
        { 2.009f, 3.869f },
        { 2.034f, 3.741f },
        { 2.076f, 3.617f },
        { 2.134f, 3.5f },
        { 2.207f, 3.391f },
        { 2.293f, 3.293f },
        { 2.391f, 3.207f },
        { 2.5f, 3.134f },
        { 2.617f, 3.076f },
        { 2.741f, 3.034f },
        { 2.869f, 3.009f },
        { 3.0f, 3.0f },
        { 8.0f, 3.0f },
        { 8.522f, 3.034f },
        { 9.035f, 3.136f },
        { 9.531f, 3.304f },
        { 10.0f, 3.536f },
        { 10.435f, 3.827f },
        { 10.828f, 4.172f },
        { 11.173f, 4.565f },
        { 11.464f, 5.0f },
        { 11.696f, 5.469f },
        { 11.864f, 5.965f },
        { 11.966f, 6.478f },
        { 12.0f, 7.0f },
        { 12.034f, 6.478f },
        { 12.136f, 5.965f },
        { 12.304f, 5.469f },
        { 12.536f, 5.0f },
        { 12.827f, 4.565f },
        { 13.172f, 4.172f },
        { 13.565f, 3.827f },
        { 14.0f, 3.536f },
        { 14.469f, 3.304f },
        { 14.965f, 3.136f },
        { 15.478f, 3.034f },
        { 16.0f, 3.0f },
        { 21.0f, 3.0f },
        { 21.131f, 3.009f },
        { 21.259f, 3.034f },
        { 21.383f, 3.076f },
        { 21.5f, 3.134f },
        { 21.609f, 3.207f },
        { 21.707f, 3.293f },
        { 21.793f, 3.391f },
        { 21.866f, 3.5f },
        { 21.924f, 3.617f },
        { 21.966f, 3.741f },
        { 21.991f, 3.869f },
        { 22.0f, 4.0f },
        { 22.0f, 17.0f },
        { 21.991f, 17.131f },
        { 21.966f, 17.259f },
        { 21.924f, 17.383f },
        { 21.866f, 17.5f },
        { 21.793f, 17.609f },
        { 21.707f, 17.707f },
        { 21.609f, 17.793f },
        { 21.5f, 17.866f },
        { 21.383f, 17.924f },
        { 21.259f, 17.966f },
        { 21.131f, 17.991f },
        { 21.0f, 18.0f },
        { 15.0f, 18.0f },
        { 14.608f, 18.026f },
        { 14.224f, 18.102f },
        { 13.852f, 18.228f },
        { 13.5f, 18.402f },
        { 13.174f, 18.62f },
        { 12.879f, 18.879f },
        { 12.62f, 19.174f },
        { 12.402f, 19.5f },
        { 12.228f, 19.852f },
        { 12.102f, 20.224f },
        { 12.026f, 20.608f },
        { 12.0f, 21.0f },
        { 11.974f, 20.608f },
        { 11.898f, 20.224f },
        { 11.772f, 19.852f },
        { 11.598f, 19.5f },
        { 11.38f, 19.174f },
        { 11.121f, 18.879f },
        { 10.826f, 18.62f },
        { 10.5f, 18.402f },
        { 10.148f, 18.228f },
        { 9.776f, 18.102f },
        { 9.392f, 18.026f },
        { 9.0f, 18.0f }
    };
    rcIconDrawPolyline(bounds, path0,
                       102,
                       viewBox, stroke, true, color);
}

static inline void rcIconBookOpen(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconBookOpen, NULL);
}

#endif /* RC_ICON_BOOK_OPEN_H */
