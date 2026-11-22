#ifndef RC_ICON_FILM_H
#define RC_ICON_FILM_H

#include "rc_icons_common.h"

/*
    Generated from film.svg (Lucide). The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 24x24.
*/
static inline void rcDrawIconFilm(RC_BoundingBox bounds,
                                  RC_Color color,
                                  const void *userData) {
    (void)userData;
    const float viewBox = 24.0f;
    const float stroke  = 2.0f;

    rcIconDrawRoundedRectStroke(bounds, 3.0f, 3.0f, 18.0f, 18.0f,
                                2.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 7.0f, 3.0f, 7.0f, 21.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 3.0f, 7.5f, 7.0f, 7.5f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 3.0f, 12.0f, 21.0f, 12.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 3.0f, 16.5f, 7.0f, 16.5f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 17.0f, 3.0f, 17.0f, 21.0f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 17.0f, 7.5f, 21.0f, 7.5f, viewBox, stroke, color);
    rcIconDrawRoundLine(bounds, 17.0f, 16.5f, 21.0f, 16.5f, viewBox, stroke, color);
}

static inline void rcIconFilm(float size, RC_Color color) {
    rcIconEmit(size, color, rcDrawIconFilm, NULL);
}

#endif /* RC_ICON_FILM_H */
