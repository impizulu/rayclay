/* The umbrella precedes this header's own guard so an icon header compiles
   when it is included first or alone: rayclay.h includes the bundled icons,
   whose headers re-enter this one, and the guard must still be open then for
   them to see the declarations below. "rayclay.h" by name, not "../../../rayclay.h":
   a release ships this file beside the generated icons, in
   examples/assets/icons/, where only the include path reaches rayclay.h. */
#include "rayclay.h"

#ifndef RC_ICONS_COMMON_H
#define RC_ICONS_COMMON_H

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
   way, and the plain form handed it straight through to claySizingFixed. A NaN
   that reaches here lands on the floor. Finiteness itself is refused before the
   clamp, in the library (rci_core_icon_size_usable), because this header
   compiles inside app TUs and cannot pull in <math.h>. */
static inline float rci_icons_clamp_size(float size) {
    return !(size >= 1.0f) ? 1.0f : size;
}

/* The most points of a FILLED icon outline (rcIconDrawFilledPolygon) that its
   ear clip cuts, from fixed scratch with no heap allocation, once a shape and
   scale; a longer outline is cut into quads on each draw instead and fills
   whole. A STROKED path (rcIconDrawPolyline) takes any number: it is drawn in
   windows of this many points. Single-sourced here (not in the library) so a
   runtime SVG->icon converter shares the exact same ceiling instead of
   mirroring the literal 256. */
enum { RC_ICON_MAX_PTS = 256 };

#ifdef __cplusplus
extern "C" {
#endif

/* Stroke + fill draw helpers - all geometry is in viewBox units; `bounds` is the
   element's screen rectangle and `viewBoxSize` the icon's design size (e.g.
   24.0). `strokeWidth` is also in viewBox units. Color is a RC_Color.

     Polyline         - a (optionally closed) stroked path through `points`.
     RoundLine        - a single round-capped segment from (x0,y0) to (x1,y1).
     CircleStroke     - a stroked circle centered at (cx,cy) with `radius`.
     RoundedRectStroke- a stroked rounded rectangle at (x,y,width,height,radius).
     FilledPolygon    - a solid polygon through `points`, ONE outline filled
                        by the NONZERO rule, as canvas fill() fills a path:
                        a point is inside where the outline winds round it
                        any number of times. So an outline may cross itself
                        (a bowtie, a star drawn in one stroke), repeat a
                        point, or walk round a hole and back over a bridge,
                        which leaves the hole open when it is wound the
                        other way. For the evenodd rule, or holes as outlines
                        of their own, load the shape as an SVG path (rcSvg,
                        rcLoadSvg), which fills by the path's fill-rule.
   Stroke helpers draw a CENTERED stroke (SVG semantics), matching how a path's
   strokeWidth straddles its centerline.

   A FilledPolygon outline of any number of points fills whole. A simple one
   (no two of its edges but neighbors share a point) of up to RC_ICON_MAX_PTS
   (256) points is cut by an ear clip, once (below); any other is cut into
   quads between level lines on each draw, which costs a sort of its edges and
   grows with its crossings. One whose cut passes its budget of 65,536 steps
   (one that crosses itself very often, or has more edges that are not level
   than that), or that finds no memory for its scratch, fills as a simple
   outline would (its first 256 points), and the first such warns; that is
   learned once a shape and scale, as a clip is. A Polyline takes any number of
   points. FilledPolygon memoizes its triangulation. The key includes a hash of the
   point array's BYTES, not just its address, so re-filling one buffer in
   place is SAFE: two different shapes at one address hash apart and miss, which
   costs a re-triangulation and never replays the wrong geometry. A stable
   address still helps you HIT the memo - a static const table, which is what
   the generated icon headers emit - but it is a performance property, not a
   correctness requirement. The key is the shape and its scale (bounds.width /
   viewBoxSize), not where it is drawn: one point table drawn in many places, or
   moving, is triangulated once, so place a repeated shape by its `bounds`
   rather than by adding an offset to a copy of its points.
   A translucent Polyline that turns or closes is drawn as ONE image, as a
   browser paints a stroke: where its segments overlap, on the inner side of
   each turn, the color is laid down once, not darkened. Two calls are two
   images, as two SVG elements are. Past 64 such images in a frame the rest
   blend piece by piece, and that is logged once.
*/
RC_API void rcIconDrawPolyline(RC_BoundingBox bounds,
                                 const RC_IconPoint *points, int pointCount,
                                 float viewBoxSize, float strokeWidth,
                                 bool closed, RC_Color color);

RC_API void rcIconDrawFilledPolygon(RC_BoundingBox bounds,
                                      const RC_IconPoint *points, int pointCount,
                                      float viewBoxSize, RC_Color color);

/* A solid disc / solid (axis-aligned) ellipse, in viewBox units. */
RC_API void rcIconDrawFilledCircle(RC_BoundingBox bounds,
                                     float cx, float cy, float radius,
                                     float viewBoxSize, RC_Color color);

RC_API void rcIconDrawFilledEllipse(RC_BoundingBox bounds,
                                      float cx, float cy, float rx, float ry,
                                      float viewBoxSize, RC_Color color);

RC_API void rcIconDrawRoundLine(RC_BoundingBox bounds,
                                  float x0, float y0, float x1, float y1,
                                  float viewBoxSize, float strokeWidth,
                                  RC_Color color);

RC_API void rcIconDrawCircleStroke(RC_BoundingBox bounds,
                                     float cx, float cy, float radius,
                                     float viewBoxSize, float strokeWidth,
                                     RC_Color color);

RC_API void rcIconDrawRoundedRectStroke(RC_BoundingBox bounds,
                                          float x, float y, float width, float height,
                                          float radius, float viewBoxSize,
                                          float strokeWidth, RC_Color color);

#ifdef __cplusplus
}
#endif

/* ---------------------------------------------------------------------------
   Per-frame payload pool

   The CUSTOM payload must live until the layout pass has produced render
   commands and the renderer has consumed them. A library-owned pool of fixed
   blocks avoids a heap allocation per icon while still allowing many
   differently colored instances of the same icon in one frame.
   --------------------------------------------------------------------------- */
/* The pool must hold every icon kept live in a SINGLE frame, from the layout
   pass until the renderer consumes the payloads.

   It is one library-owned, chunked, grow-on-demand allocator: one cursor, one
   capacity, one meaning, in both shipping modes. A block is allocated in the
   frame that needs it, at the icon that first passes the blocks already held,
   and is never moved or freed mid-run. That invariant is the point:
   Clay holds these pointers from the layout pass until the renderer consumes them,
   so a reallocated flat array would dangle every pointer already handed out this
   frame. A block list cannot.

   NOTE RC_ICON_POOL_CAPACITY is the allocation granularity - the size of one
   block - not a ceiling on icons per frame. The pool adds blocks as needed, so
   raising it trades a larger first allocation for fewer later ones; either
   way every icon under the ceiling (64 blocks) draws in the frame
   that emits it. It must resolve to the same value in every translation unit
   that emits icons. */
#ifndef RC_ICON_POOL_CAPACITY
    #define RC_ICON_POOL_CAPACITY 256
#endif

/* NOT API - the calls rcIconEmit and rcCustomDraw (inline, below) make: RC_API
   because they compile into your translation unit; call those two instead.
   Both live in the library (the pool, and the size gate
   beside rcUnzoomedScale, so every closure that links this header's other seams
   has it). Declared here rather than via an internal header: this header also
   compiles inside app TUs (C and C++), where the internal header is
   unavailable. rci_api_icon_next_draw_data hands out the next pool slot, NULL
   past the pool's ceiling. rci_core_icon_size_usable is false for a NaN or inf
   size: it has warned once per process and the icon must not be declared. */
#ifdef __cplusplus
extern "C" {
#endif
RC_API RC_CustomDrawData *rci_api_icon_next_draw_data(RC_CustomDrawCallback draw,
                                                      RC_Color color,
                                                      const void *userData);
RC_API bool               rci_core_icon_size_usable(float size);
#ifdef __cplusplus
}
#endif

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
       to claySizingFixed; measured on this tree's Clay (which reads a non-finite
       FIXED axis as no size, and an icon has no content to size it;
       test_clay_sizing_nonfinite) that was a 0x0 icon and nothing in the log -
       one `width / count` with count == 0 read
       as a broken SVG. Same rule as RC_PX (rci_core_sizing_from_typed): warn
       once, declare nothing. Tested AFTER the scale so a product that
       overflowed there is caught too. */
    if (!rci_core_icon_size_usable(size)) {
        return;
    }
    size = rci_icons_clamp_size(size);
    RC_CustomDrawData *drawData = rci_api_icon_next_draw_data(draw, color, userData);

    /* clayAutoId names __COUNTER__: see RCI_COUNTER_PUSH in rayclay.h. */
    RCI_COUNTER_PUSH
    clayAutoId({
        .layout = {
            .sizing = { claySizingFixed(size), claySizingFixed(size) },
        },
        .custom = { .customData = drawData },
    }) {}
    RCI_COUNTER_POP
}
/**
 * Draw custom content that FILLS ITS PARENT BOX: a plot, a canvas, a meter -
 * any shape rcIconEmit's square cannot hold. The element grows on both axes,
 * so the parent's .w, .h or .className size it:
 *
 *     rcBox(.id = "plot", .w = "grow", .h = "grow") {
 *         rcCustomDraw(draw_plot, &state, RC_SLATE_300);
 *     }
 *
 * @p draw runs in the draw pass with the element's bounds (RC_CustomDrawCallback
 * says what it may do). The payload comes from the per-frame pool rcIconEmit
 * draws from, so nothing is allocated per frame; past the pool's ceiling the
 * element keeps its box and draws nothing, as an icon does.
 *
 * @param draw     the drawer; NULL draws nothing
 * @param userData handed to @p draw; it must stay valid until the frame is drawn
 * @param color    handed to @p draw; zero alpha means the style's text color
 */
static inline void rcCustomDraw(RC_CustomDrawCallback draw, const void *userData,
                                RC_Color color) {
    RC_CustomDrawData *drawData = rci_api_icon_next_draw_data(draw, color, userData);

    /* clayAutoId names __COUNTER__: see RCI_COUNTER_PUSH in rayclay.h. */
    RCI_COUNTER_PUSH
    clayAutoId({
        .layout = {
            .sizing = { claySizingGrow(0), claySizingGrow(0) },
        },
        .custom = { .customData = drawData },
    }) {}
    RCI_COUNTER_POP
}
#if defined(__cplusplus) && defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif /* RC_ICONS_COMMON_H */
