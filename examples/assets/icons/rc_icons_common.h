#ifndef RC_ICONS_COMMON_H
#define RC_ICONS_COMMON_H

#include "../rayclay.h"

/* ===========================================================================
   Shared icon scaffolding

   Procedural icons are RayClay custom elements: rcIcon<Name>(size, color) emits a
   fixed-size element whose payload points at a draw callback. During the render
   pass the callback receives the element's screen bounds and draws the icon
   with the four stroke helpers below.

   The helpers are DECLARED here (extern "C") and IMPLEMENTED in the library
   using the same draw seam as the bundled icons. Crucially, every
   helper works in the icon's own viewBox coordinate space and takes the screen
   `bounds` + `viewBoxSize`; the library does the scaling. That keeps the
   per-icon headers (rc_icons_<name>.h) free of any renderer type - no Vector2,
   no Color - so they stay header-only and compile as both C and C++.
   =========================================================================== */

/* A point in an icon's viewBox coordinate space. */
typedef struct RC_IconPoint {
    float x;
    float y;
} RC_IconPoint;

/* Written as !(size >= 1.0f), not (size < 1.0f): NaN compares false either
   way, and the plain form handed it straight through to CLAY_SIZING_FIXED. A NaN
   that reaches here lands on the floor. Finiteness itself is refused before the
   clamp, in the library (rci_core_icon_size_usable), because this header
   compiles inside app TUs and cannot pull in <math.h>. */
static inline float RCI_IconClampSize(float size) {
    return !(size >= 1.0f) ? 1.0f : size;
}

/* Max points in one procedural-icon stroke path: a fixed cap so a draw needs no
   heap allocation. Single-sourced here (not in rc_icons.c) so a runtime SVG->icon
   converter shares the exact same ceiling instead of mirroring the literal 128. */
enum { RC_ICON_MAX_PTS = 128 };

#ifdef __cplusplus
extern "C" {
#endif

/* Stroke + fill draw helpers - all geometry is in viewBox units; `bounds` is the
   element's screen rectangle and `viewBoxSize` the icon's design size (e.g.
   24.0). `strokeWidth` is also in viewBox units. Colour is a RC_Color.

     Polyline         - a (optionally closed) stroked path through `points`.
     RoundLine        - a single round-capped segment from (x0,y0) to (x1,y1).
     CircleStroke     - a stroked circle centred at (cx,cy) with `radius`.
     RoundedRectStroke- a stroked rounded rectangle at (x,y,width,height,radius).
     FilledPolygon    - a solid (convex or concave) polygon through `points`;
                        used by multi-colour, filled icons such as the logo.
   Stroke helpers draw a CENTRED stroke (SVG semantics), matching how a path's
   strokeWidth straddles its centreline.

   `points` paths are capped at 128 points; points past the cap are not drawn.
   FilledPolygon memoises its triangulation. The key includes an FNV-1a hash of
   the point array's BYTES, not just its address, so re-filling one buffer in
   place is SAFE: two different shapes at one address hash apart and miss, which
   costs a re-triangulation and never replays the wrong geometry. A stable
   address still helps you HIT the memo - a static const table, which is what
   the generated icon headers emit - but it is a performance property, not a
   correctness requirement.
*/
void rcIconDrawPolyline(RC_BoundingBox bounds,
                          const RC_IconPoint *points, int pointCount,
                          float viewBoxSize, float strokeWidth,
                          bool closed, RC_Color color);

void rcIconDrawFilledPolygon(RC_BoundingBox bounds,
                               const RC_IconPoint *points, int pointCount,
                               float viewBoxSize, RC_Color color);

/* A solid disc / solid (axis-aligned) ellipse, in viewBox units. */
void rcIconDrawFilledCircle(RC_BoundingBox bounds,
                              float cx, float cy, float radius,
                              float viewBoxSize, RC_Color color);

void rcIconDrawFilledEllipse(RC_BoundingBox bounds,
                               float cx, float cy, float rx, float ry,
                               float viewBoxSize, RC_Color color);

void rcIconDrawRoundLine(RC_BoundingBox bounds,
                           float x0, float y0, float x1, float y1,
                           float viewBoxSize, float strokeWidth,
                           RC_Color color);

void rcIconDrawCircleStroke(RC_BoundingBox bounds,
                              float cx, float cy, float radius,
                              float viewBoxSize, float strokeWidth,
                              RC_Color color);

void rcIconDrawRoundedRectStroke(RC_BoundingBox bounds,
                                   float x, float y, float width, float height,
                                   float radius, float viewBoxSize,
                                   float strokeWidth, RC_Color color);

#ifdef __cplusplus
}
#endif

/* ---------------------------------------------------------------------------
   Per-frame payload pool

   The CUSTOM payload must live until the layout pass has produced render
   commands and rcRender() has consumed them. A small static ring buffer avoids
   a heap allocation per icon while still allowing many differently coloured
   instances of the same icon in one frame.
   --------------------------------------------------------------------------- */
/* The pool must hold every icon kept live in a SINGLE frame, from the layout
   pass until rcRender consumes the payloads.

   It is one library-owned, chunked, grow-on-demand allocator: one cursor, one
   capacity, one meaning, in both shipping modes. Blocks are allocated as a frame
   needs them and are never moved or freed mid-run. That invariant is the point:
   Clay holds these pointers from the layout pass until rcRender consumes them,
   so a reallocated flat array would dangle every pointer already handed out this
   frame. A block list cannot.

   NOTE RC_ICON_POOL_CAPACITY is the allocation granularity - the size of one
   block - not a ceiling on icons per frame. The pool adds blocks as needed, so
   raising it trades a larger first allocation for fewer later ones. It must
   resolve to the same value in every translation unit that emits icons. */
#ifndef RC_ICON_POOL_CAPACITY
    #define RC_ICON_POOL_CAPACITY 256
#endif

/* All three live in the library (the first two in rc_api.c, the size gate in
   rc_core.c beside rcUnzoomedScale, so every closure that links this header's
   other seams has it). Declared here rather than via rc_internal.h: this header
   also compiles inside app TUs (C and C++), where the internal header is
   unavailable. rci_core_icon_size_usable is false for a NaN or inf size: it has
   warned once per process and the icon must not be declared. */
#ifdef __cplusplus
extern "C" {
#endif
unsigned int       rci_api_icon_frame_stamp_get(void);
RC_CustomDrawData *rci_api_icon_next_draw_data(RC_CustomDrawCallback draw,
                                               RC_Color color,
                                               const void *userData);
bool               rci_core_icon_size_usable(float size);
#ifdef __cplusplus
}
#endif

/* Kept as the header-side name every icon entry point already calls, so this
   change is invisible to callers - it is now a one-line forward into the
   library rather than a ring of its own. */
static inline RC_CustomDrawData *RCI_IconNextDrawData(
    RC_CustomDrawCallback draw,
    RC_Color color,
    const void *userData) {
    return rci_api_icon_next_draw_data(draw, color, userData);
}

/* The partial designated initializers below are idiomatic C (unnamed members
   zero) and silent under every C compiler, but g++/clang++ -Wextra flag each
   one with -Wmissing-field-initializers inside this header-inline function -
   14 warnings landing in every C++ consumer's build, fatal under -Werror.
   The values ARE zero-initialized in C++ too, so the warning is noise here;
   suppress it for this one function on the compilers that emit it. */
#if defined(__cplusplus) && defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
static inline void rcIconEmit(float size,
                                RC_Color color,
                                RC_CustomDrawCallback draw,
                                const void *userData) {
    /* UNZOOMED SCOPE: an icon box is an absolute px square, so inside a
       scope held at constant physical size it must counter-scale like every
       other pixel quantity - otherwise a chip stops growing and its glyph does
       not, and the glyph overflows the chip. Exactly 1.0 outside a scope, so
       this costs a compare on every other icon in the tree.
       WARN: before the clamp, never after. The clamp defines the legal drawn size,
       so clamping first would let the scale carry the result back out of range. */
    size *= rcUnzoomedScale();
    /* Refused, not clamped. The old `size < 1.0f` floor let NaN and inf through
       to CLAY_SIZING_FIXED; measured on this tree's Clay (which collapses a
       non-finite FIXED axis to 0, test_clay_sizing_nonfinite) that was a 0x0
       icon and nothing in the log - one `width / count` with count == 0 read
       as a broken SVG. Same rule as RC_PX (rci_core_sizing_from_typed): warn
       once, declare nothing. Tested AFTER the scale so a product that
       overflowed there is caught too. */
    if (!rci_core_icon_size_usable(size)) {
        return;
    }
    size = RCI_IconClampSize(size);
    RC_CustomDrawData *drawData = RCI_IconNextDrawData(draw, color, userData);

    CLAY_AUTO_ID({
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size) },
        },
        .custom = { .customData = drawData },
    }) {}
}
#if defined(__cplusplus) && defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif /* RC_ICONS_COMMON_H */
