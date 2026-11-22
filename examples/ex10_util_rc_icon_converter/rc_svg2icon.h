#ifndef RC_SVG2ICON_H
#define RC_SVG2ICON_H

/*
 * rc_svg2icon.h - convert simple stroke/filled SVG icons (Lucide-style strokes,
 * or hand-authored multi-color artwork) into RayClay icon ops, then either
 * preview them (rc_svg2icon_draw) or emit a header-only RayClay icon
 * (rc_svg2icon_emit) you can call like any bundled icon.
 *
 * Single translation unit: every function is static. Malformed input never
 * crashes - every budget is bounded, an exhausted one warns, and a bad tag or
 * path is skipped with a warning and a partial result returned.
 *
 * Parse + emit link against nothing but libm:
 *     cc -std=c99 -I<rayclay dir> -I<this dir> my_gen.c -o my_gen -lm
 * Drawing the result needs RayClay: #define RC_SVG2ICON_PREVIEW first.
 */

#include "rayclay.h"   /* pulls in the icon scaffolding: RC_IconPoint, rcIconDraw*, RC_Color/BoundingBox */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <float.h>     /* FLT_MAX: an empty box (rc_svg_fill_spans) */

/* M_PI is not part of ISO C99; provide a fallback for strict -std=c99 builds. */
#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

/* Public op IR. */

typedef enum { RC_SVG_ROUND_LINE, RC_SVG_POLYLINE, RC_SVG_CIRCLE_STROKE,
               RC_SVG_RRECT_STROKE, RC_SVG_FILLED_POLY, RC_SVG_FILLED_CIRCLE,
               RC_SVG_FILLED_ELLIPSE, RC_SVG_FILLED_QUADS } RcSvgOpKind;

typedef struct {
    RcSvgOpKind kind;
    float p[5];              /* ROUND_LINE:x0,y0,x1,y1 . CIRCLE_STROKE/FILLED_CIRCLE:cx,cy,r .
                               RRECT_STROKE:x,y,w,h,rx . FILLED_ELLIPSE:cx,cy,rx,ry */
    int   pointOff, pointCount;   /* slice into RcSvgIcon.points (POLYLINE / FILLED_POLY /
                                     FILLED_QUADS: four points a quad) */
    float stroke;            /* stroke width in viewBox units (stroke ops) */
    bool  closed;            /* POLYLINE only */
    bool  baked;             /* true -> draw with .color; false -> runtime mono color */
    RC_Color color;        /* baked color when .baked */
    bool  shapeStart;        /* colored only: the first op of a source shape */
} RcSvgOp;

#define RC_SVG_MAX_OPS 512
#define RC_SVG_MAX_POINTS 8192
#define RC_SVG_MAX_WARN 16

/* The most points of a fill the runtime's ear clip takes, once a shape and scale; a
   flattened fill above it still draws whole, cut into quads by the nonzero rule on EVERY
   draw (a stroke takes any length). Single-sourced from the library's RC_ICON_MAX_PTS
   (rc_icons_common.h, pulled in via rayclay.h) so the converter's GENERATE-time note can
   never drift from the runtime's. */
#define RC_SVG_RUNTIME_MAX_PTS RC_ICON_MAX_PTS

typedef struct {
    float viewW, viewH;      /* viewBox size; shapes normalized so origin is 0,0 */
    RcSvgOp ops[RC_SVG_MAX_OPS]; int opCount;
    RC_IconPoint points[RC_SVG_MAX_POINTS]; int pointCount;
    bool colored;            /* any baked fill/stroke -> rcIcon(size) form on emit */
    bool ok; char err[160];
    const char *warn[RC_SVG_MAX_WARN]; int warnCount;   /* static-string warnings */
} RcSvgIcon;

/* Internal geometry / parse scratch. */

#define RC_SVG_MAX_SHAPES 512
#define RC_SVG_MAX_SHAPE_POINTS 4096
#define RC_SVG_MAX_ATTRS 32
#define RC_SVG_MAX_DEPTH 64

typedef struct { float x, y; } RcSvgPoint;

/* An SVG transform: x' = a x + c y + e, y' = b x + d y + f (SVG's matrix()). */
typedef struct {
    float a, b, c, d, e, f;
} RcSvgMatrix;

/* A resolved paint: NONE (no paint), CURRENT (the runtime color argument), or a
   baked (r,g,b,a) color.
   A NONE paint has three origins, and they are not drawn alike: nothing in the
   cascade named it (an UNPAINTED shape, drawn as its outline), the author wrote
   "none" (`stated`), or the author named a color this parser cannot read
   (`unread`: the paint was lost, not withheld). Both bits inherit with the paint. */
typedef enum { RC_PAINT_NONE, RC_PAINT_CURRENT, RC_PAINT_BAKED } RcSvgPaintKind;
typedef struct {
    RcSvgPaintKind kind;
    RC_Color color;        /* valid when kind == RC_PAINT_BAKED */
    bool stated;           /* NONE because the author wrote "none" */
    bool unread;           /* NONE because the color named is unrecognized */
} RcSvgPaint;

typedef enum { RC_SHAPE_PATH, RC_SHAPE_RECT, RC_SHAPE_CIRCLE, RC_SHAPE_ELLIPSE } RcSvgShapeKind;

typedef struct {
    RcSvgShapeKind kind;
    /* PATH: a flattened polyline stored in the shared shape-point pool. */
    int   ptOff, ptCount;
    bool  closed;
    /* RECT: x,y,w,h,rx,ry .  CIRCLE: cx,cy,r .  ELLIPSE: cx,cy,rx,ry */
    float x, y, w, h, rx, ry;   /* rect */
    float cx, cy, r;            /* circle / ellipse re-use cx,cy + (r | rx,ry) */
    float strokeWidth;
    RcSvgPaint fill, stroke;
    int   elem;                 /* its source element; a <path>'s subpaths share one */
    bool  evenodd;              /* PATH: fill-rule evenodd (else SVG's initial nonzero) */
    bool  mitered;              /* RECT: square, its stroke's corners mitered */
    bool  initialFill;          /* fill named nowhere, given SVG's initial black */
} RcSvgShape;

/* Parse-wide mutable state, kept off the stack to bound recursion cheaply. */
typedef struct {
    RcSvgShape  shapes[RC_SVG_MAX_SHAPES];  int shapeCount;
    RcSvgPoint  pts[RC_SVG_MAX_SHAPE_POINTS]; int ptCount;
    int   curveSteps;
    float arcDeg;
    RcSvgIcon  *icon;        /* for warning collection */
    int   elem;              /* elements handled so far: the one being read */
    bool  evenodd;           /* the fill-rule the element being read uses or hands down */
    bool  joinMiters;        /* its stroke-linejoin miters (rc_svg_join_miters) */
    bool  limitMiters;       /* its stroke-miterlimit lets a right angle miter */
    bool  overflow;          /* set once any pool is exhausted */
    bool  sawStyle;          /* a <style> sheet may paint what the attributes do not */
    /* The element being read's user space in the root's (its own transform and
       every ancestor's), and whether it is anything but the identity:
       rc_svg_push_pt places every point through it, as rcLoadSvg does. */
    RcSvgMatrix ctm;
    bool  transformed;
} RcSvgParse;

/* Local helpers that keep the converter half free of the RayClay runtime. */

/* rcStrCopy's contract, spelled locally: the converter half must link against
   nothing but libm, and two lines of C99 buy that back. */
static void rc_svg_str_copy(char *dst, const char *src, size_t cap) {
    size_t n;
    if (!dst || cap == 0) {
        return;
    }
    for (n = 0; n + 1 < cap && src && src[n] != '\0'; n++) {
        dst[n] = src[n];
    }
    dst[n] = '\0';
}

/* Warnings: static strings only, bounded. */

static void rc_svg_warn(RcSvgIcon *icon, const char *msg) {
    if (!icon || icon->warnCount >= RC_SVG_MAX_WARN) {
        return;
    }
    /* De-dup identical static-string pointers so a repeated shape kind (e.g.
       many non-circular ellipses) does not swamp the small warning ring. */
    for (int i = 0; i < icon->warnCount; i++) {
        if (icon->warn[i] == msg) {
            return;
        }
    }
    icon->warn[icon->warnCount++] = msg;
}

/* Number / token scanning. */

static bool rc_svg_is_cmd_char(char c) {
    switch (c) {
    case 'A': case 'a': case 'C': case 'c': case 'H': case 'h':
    case 'L': case 'l': case 'M': case 'm': case 'Q': case 'q':
    case 'S': case 's': case 'T': case 't': case 'V': case 'v':
    case 'Z': case 'z':
        return true;
    default:
        return false;
    }
}

/* Scan a float (with optional sign/exponent) starting at s[*i]; advance *i past
   it. Returns true and writes *out on success. Grammar:
   [-+]?((\d+\.\d*)|(\.\d+)|(\d+))([eE][-+]?\d+)? */
static bool rc_svg_scan_float(const char *s, int len, int *i, float *out) {
    int j = *i;
    int start = j;
    if (j < len && (s[j] == '+' || s[j] == '-')) {
        j++;
    }
    int digitsBefore = 0;
    while (j < len && isdigit((unsigned char)s[j])) {
        j++; digitsBefore++;
    }
    int digitsAfter = 0;
    if (j < len && s[j] == '.') {
        j++;
        while (j < len && isdigit((unsigned char)s[j])) {
            j++; digitsAfter++;
        }
    }
    if (digitsBefore == 0 && digitsAfter == 0) {
        return false;   /* no mantissa digits: not a number */
    }
    /* Exponent (only consumed when well-formed, matching the regex). */
    if (j < len && (s[j] == 'e' || s[j] == 'E')) {
        int k = j + 1;
        if (k < len && (s[k] == '+' || s[k] == '-')) {
            k++;
        }
        int expDigits = 0;
        while (k < len && isdigit((unsigned char)s[k])) {
            k++; expDigits++;
        }
        if (expDigits > 0) {
            j = k;
        }
    }
    /* Copy to a NUL-terminated scratch for strtod. */
    char buf[64];
    int n = j - start;
    if (n <= 0 || n >= (int)sizeof(buf)) {
        return false;
    }
    memcpy(buf, s + start, (size_t)n);
    buf[n] = '\0';
    *out = (float)strtod(buf, NULL);
    *i = j;
    return true;
}

/* Scan an SVG path arc FLAG. The grammar defines a flag as a single character,
   "0" or "1" - not a number. Reading one with the float scanner both accepts
   what the grammar forbids and rejects the compact spelling browsers accept:
   "A5 5 0 1150 50" is largeArc = 1, sweep = 1, endpoint (50,50). */
static bool rc_svg_scan_flag(const char *s, int len, int *i, bool *out) {
    if (*i >= len || (s[*i] != '0' && s[*i] != '1')) {
        return false;
    }
    *out = (s[*i] == '1');
    (*i)++;
    return true;
}

/* Index just past the "-->" that closes an XML comment beginning at or after
   `from`, or -1 when no terminator lies inside `len`. Length-bounded rather than
   NUL-bounded, because parse takes a LENGTH: the buffer may be an mmap'd slice
   or bytes off a socket and need not carry a terminator. */
static int rc_svg_comment_end(const char *s, int len, int from) {
    for (int j = from; j + 2 < len; j++) {
        if (s[j] == '-' && s[j + 1] == '-' && s[j + 2] == '>') {
            return j + 3;
        }
    }
    return -1;
}

/* Skip whitespace and commas (SVG treats commas as separators). */
static void rc_svg_skip_sep(const char *s, int len, int *i) {
    while (*i < len && (isspace((unsigned char)s[*i]) || s[*i] == ',')) {
        (*i)++;
    }
}

/* Bezier / arc flattening. */

static bool rc_svg_nearly_same(RcSvgPoint a, RcSvgPoint b) {
    const float eps = 1e-6f;
    return fabsf(a.x - b.x) <= eps && fabsf(a.y - b.y) <= eps;
}

static RcSvgPoint rc_svg_cubic(RcSvgPoint p0, RcSvgPoint p1,
                               RcSvgPoint p2, RcSvgPoint p3, float t) {
    float mt = 1.0f - t;
    RcSvgPoint r;
    r.x = mt * mt * mt * p0.x + 3.0f * mt * mt * t * p1.x
        + 3.0f * mt * t * t * p2.x + t * t * t * p3.x;
    r.y = mt * mt * mt * p0.y + 3.0f * mt * mt * t * p1.y
        + 3.0f * mt * t * t * p2.y + t * t * t * p3.y;
    return r;
}

static RcSvgPoint rc_svg_quad(RcSvgPoint p0, RcSvgPoint p1, RcSvgPoint p2, float t) {
    float mt = 1.0f - t;
    RcSvgPoint r;
    r.x = mt * mt * p0.x + 2.0f * mt * t * p1.x + t * t * p2.x;
    r.y = mt * mt * p0.y + 2.0f * mt * t * p1.y + t * t * p2.y;
    return r;
}

static float rc_svg_angle_between(float ux, float uy, float vx, float vy) {
    float dot = ux * vx + uy * vy;
    float det = ux * vy - uy * vx;
    return atan2f(det, dot);
}

/* Append one point to the shared shape-point pool, placed by the element's
   transform (a point is read in its element's user space); flags overflow if
   full. */
static bool rc_svg_push_pt(RcSvgParse *ps, RcSvgPoint p) {
    if (ps->ptCount >= RC_SVG_MAX_SHAPE_POINTS) {
        if (!ps->overflow) {
            rc_svg_warn(ps->icon, "shape-point budget exhausted; geometry truncated");
        }
        ps->overflow = true;
        return false;
    }
    if (ps->transformed) {
        const RcSvgMatrix *m = &ps->ctm;
        const float        x = p.x;

        p.x = m->a * x + m->c * p.y + m->e;
        p.y = m->b * x + m->d * p.y + m->f;
    }
    ps->pts[ps->ptCount].x = p.x;
    ps->pts[ps->ptCount].y = p.y;
    ps->ptCount++;
    return true;
}

/* Flatten one SVG elliptical arc into points EXCLUDING start, appending them to
   the shape-point pool. Endpoint->center parametrization, radii correction,
   arcDeg cap, >=4 steps, last point snapped to the exact end. */
static void rc_svg_arc(RcSvgParse *ps, RcSvgPoint start, float rx, float ry,
                       float rotDeg, bool largeArc, bool sweep, RcSvgPoint end,
                       float maxSegDeg) {
    if (rc_svg_nearly_same(start, end)) {
        return;
    }
    /* Load-bearing: every arc input must be finite before the math below runs.
       A radius of 1e30 makes rx*rx overflow to +inf, inf-inf is NaN, and
       converting that NaN with (int)ceilf is undefined behavior. */
    if (!isfinite(end.x) || !isfinite(end.y)) {
        rc_svg_warn(ps->icon, "path arc has a non-finite endpoint - segment skipped");
        return;   /* deliberately does NOT push `end`: that is the poison itself */
    }
    if (!isfinite(start.x) || !isfinite(start.y) ||
        !isfinite(rx) || !isfinite(ry) || !isfinite(rotDeg)) {
        /* SVG's own degenerate-arc rule is "draw a straight line to the
           endpoint" (it says so for a zero radius); a non-finite parameter is
           outside the spec entirely, so borrowing that rule keeps the subpath
           connected instead of silently losing a segment. */
        rc_svg_warn(ps->icon, "path arc has a non-finite radius or start - drawn as a line");
        rc_svg_push_pt(ps, end);
        return;
    }
    rx = fabsf(rx);
    ry = fabsf(ry);
    if (rx == 0.0f || ry == 0.0f) {
        rc_svg_push_pt(ps, end);
        return;
    }

    float phi = (float)(fmod((double)rotDeg, 360.0) * M_PI / 180.0);
    float cosPhi = cosf(phi);
    float sinPhi = sinf(phi);

    float dx2 = (start.x - end.x) / 2.0f;
    float dy2 = (start.y - end.y) / 2.0f;

    float x1p = cosPhi * dx2 + sinPhi * dy2;
    float y1p = -sinPhi * dx2 + cosPhi * dy2;

    float lam = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
    if (lam > 1.0f) {
        float scale = sqrtf(lam);
        rx *= scale;
        ry *= scale;
    }

    float rx2 = rx * rx;
    float ry2 = ry * ry;
    float x1p2 = x1p * x1p;
    float y1p2 = y1p * y1p;

    float denom = rx2 * y1p2 + ry2 * x1p2;
    if (denom == 0.0f) {
        rc_svg_push_pt(ps, end);
        return;
    }

    /* largeArc and sweep are bool BY TYPE: no out-of-range float can reach a
       conversion here. */
    float sign = (largeArc == sweep) ? -1.0f : 1.0f;
    float factorSq = (rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2) / denom;
    if (factorSq < 0.0f) {
        factorSq = 0.0f;
    }
    float factor = sign * sqrtf(factorSq);

    float cxp = factor * (rx * y1p / ry);
    float cyp = factor * (-ry * x1p / rx);

    float cx = cosPhi * cxp - sinPhi * cyp + (start.x + end.x) / 2.0f;
    float cy = sinPhi * cxp + cosPhi * cyp + (start.y + end.y) / 2.0f;

    float v1x = (x1p - cxp) / rx, v1y = (y1p - cyp) / ry;
    float v2x = (-x1p - cxp) / rx, v2y = (-y1p - cyp) / ry;

    float theta1 = rc_svg_angle_between(1.0f, 0.0f, v1x, v1y);
    float delta = rc_svg_angle_between(v1x, v1y, v2x, v2y);

    if (!sweep && delta > 0.0f) {
        delta -= 2.0f * (float)M_PI;
    } else if (sweep && delta < 0.0f) {
        delta += 2.0f * (float)M_PI;
    }

    /* Finite inputs are not enough - the intermediate math can still overflow
       (finite radii with a huge start point do it). This guard is what makes the
       (int) conversion below provably safe. */
    if (!isfinite(cx) || !isfinite(cy) || !isfinite(theta1) || !isfinite(delta)) {
        rc_svg_warn(ps->icon, "path arc geometry overflowed - drawn as a line to its endpoint");
        rc_svg_push_pt(ps, end);   /* `end` was proven finite at entry */
        return;
    }

    float maxSeg = maxSegDeg > 1.0f ? maxSegDeg : 1.0f;
    float deltaDeg = fabsf(delta) * 180.0f / (float)M_PI;
    int steps = (int)ceilf(deltaDeg / maxSeg);
    if (steps < 4) {
        steps = 4;
    }

    for (int k = 1; k <= steps; k++) {
        float t = (float)k / (float)steps;
        float theta = theta1 + delta * t;
        RcSvgPoint p;
        if (k == steps) {
            /* Snap the final sample to the exact command endpoint. */
            p = end;
        } else {
            p.x = cx + cosPhi * rx * cosf(theta) - sinPhi * ry * sinf(theta);
            p.y = cy + sinPhi * rx * cosf(theta) + cosPhi * ry * sinf(theta);
        }
        rc_svg_push_pt(ps, p);
    }
}

/* Color parsing. */

static int rc_svg_hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

static bool rc_svg_hex2(const char *s, int *out) {
    int hi = rc_svg_hexval(s[0]);
    int lo = rc_svg_hexval(s[1]);
    if (hi < 0 || lo < 0) {
        return false;
    }
    *out = hi * 16 + lo;
    return true;
}

typedef struct { const char *name; unsigned char r, g, b, a; } RcSvgNamedColor;

/* Named-color table. */
static const RcSvgNamedColor rc_svg_named[] = {
    { "black", 0, 0, 0, 255 },       { "white", 255, 255, 255, 255 },
    { "red", 255, 0, 0, 255 },       { "green", 0, 128, 0, 255 },
    { "lime", 0, 255, 0, 255 },      { "blue", 0, 0, 255, 255 },
    { "navy", 0, 0, 128, 255 },      { "teal", 0, 128, 128, 255 },
    { "aqua", 0, 255, 255, 255 },    { "cyan", 0, 255, 255, 255 },
    { "yellow", 255, 255, 0, 255 },  { "gold", 255, 215, 0, 255 },
    { "orange", 255, 165, 0, 255 },  { "brown", 165, 42, 42, 255 },
    { "purple", 128, 0, 128, 255 },  { "magenta", 255, 0, 255, 255 },
    { "fuchsia", 255, 0, 255, 255 }, { "pink", 255, 192, 203, 255 },
    { "olive", 128, 128, 0, 255 },   { "maroon", 128, 0, 0, 255 },
    { "silver", 192, 192, 192, 255 },{ "gray", 128, 128, 128, 255 },
    { "grey", 128, 128, 128, 255 },  { "transparent", 0, 0, 0, 0 },
};

/* Lowercase-copy a trimmed token into buf; returns the trimmed length. */
static int rc_svg_lower_trim(const char *v, int len, char *buf, int cap) {
    int a = 0, b = len;
    while (a < b && isspace((unsigned char)v[a])) a++;
    while (b > a && isspace((unsigned char)v[b - 1])) b--;
    int n = b - a;
    if (n >= cap) {
        n = cap - 1;
    }
    for (int i = 0; i < n; i++) {
        buf[i] = (char)tolower((unsigned char)v[a + i]);
    }
    buf[n] = '\0';
    return n;
}

/* Resolve a fill/stroke value (len bytes) to a paint. NULL/empty/"none"/
   unparseable -> NONE; currentColor (any case) -> CURRENT; hex/named -> BAKED. */
static RcSvgPaint rc_svg_parse_color(const char *v, int len) {
    RcSvgPaint p;
    p.kind = RC_PAINT_NONE;
    p.color = RC_LIT(RC_Color){ 0, 0, 0, 0 };
    p.stated = false;
    p.unread = false;
    if (!v) {
        return p;
    }

    /* Trim (preserving case for the hex body) + a lowercase copy for matching. */
    int a = 0, b = len;
    while (a < b && isspace((unsigned char)v[a])) a++;
    while (b > a && isspace((unsigned char)v[b - 1])) b--;
    const char *t = v + a;
    int tn = b - a;
    if (tn <= 0) {
        return p;
    }

    char low[64];
    rc_svg_lower_trim(v, len, low, (int)sizeof(low));
    if (strcmp(low, "none") == 0) {
        return p;
    }
    if (strcmp(low, "currentcolor") == 0) {
        p.kind = RC_PAINT_CURRENT;
        return p;
    }

    if (t[0] == '#') {
        const char *h = t + 1;
        int hn = tn - 1;
        int r, g, bl, al;
        if (hn == 3) {
            int r1 = rc_svg_hexval(h[0]), g1 = rc_svg_hexval(h[1]), b1 = rc_svg_hexval(h[2]);
            if (r1 < 0 || g1 < 0 || b1 < 0) return p;
            r = r1 * 16 + r1; g = g1 * 16 + g1; bl = b1 * 16 + b1; al = 255;
        } else if (hn == 4) {
            int r1 = rc_svg_hexval(h[0]), g1 = rc_svg_hexval(h[1]);
            int b1 = rc_svg_hexval(h[2]), a1 = rc_svg_hexval(h[3]);
            if (r1 < 0 || g1 < 0 || b1 < 0 || a1 < 0) return p;
            r = r1 * 16 + r1; g = g1 * 16 + g1; bl = b1 * 16 + b1; al = a1 * 16 + a1;
        } else if (hn == 6) {
            if (!rc_svg_hex2(h, &r) || !rc_svg_hex2(h + 2, &g) || !rc_svg_hex2(h + 4, &bl)) return p;
            al = 255;
        } else if (hn == 8) {
            if (!rc_svg_hex2(h, &r) || !rc_svg_hex2(h + 2, &g) ||
                !rc_svg_hex2(h + 4, &bl) || !rc_svg_hex2(h + 6, &al)) return p;
        } else {
            return p;
        }
        p.kind = RC_PAINT_BAKED;
        p.color = rcRgb(r, g, bl, al / 255.0f);
        return p;
    }

    for (size_t i = 0; i < sizeof(rc_svg_named) / sizeof(rc_svg_named[0]); i++) {
        if (strcmp(low, rc_svg_named[i].name) == 0) {
            p.kind = RC_PAINT_BAKED;
            p.color = rcRgb(rc_svg_named[i].r, rc_svg_named[i].g,
                             rc_svg_named[i].b, rc_svg_named[i].a / 255.0f);
            return p;
        }
    }
    return p;   /* unrecognized: no paint */
}

/* Minimal XML tokenizer: attribute reads only, enough for icon SVGs. */

typedef struct {
    char  name[32];
    const char *val;         /* points into the source buffer (not NUL-terminated) */
    int   valLen;
} RcSvgAttr;

typedef struct {
    char  tag[32];
    RcSvgAttr attrs[RC_SVG_MAX_ATTRS];
    int   attrCount;
    bool  selfClose;
    bool  isClose;           /* </tag> */
    bool  editorData;        /* an editor/metadata namespace prefix (see scan) */
} RcSvgElem;

/* Read an attribute value's raw text by name (no CSS cascade). Returns NULL if
   the attribute is absent. */
static const char *rc_svg_attr(const RcSvgElem *e, const char *name, int *lenOut) {
    for (int i = 0; i < e->attrCount; i++) {
        if (strcmp(e->attrs[i].name, name) == 0) {
            if (lenOut) *lenOut = e->attrs[i].valLen;
            return e->attrs[i].val;
        }
    }
    return NULL;
}

/* Parse a float attribute: the first number in the value, else the default. */
static float rc_svg_attr_number(const RcSvgElem *e, const char *name, float def) {
    int len = 0;
    const char *v = rc_svg_attr(e, name, &len);
    if (!v) {
        return def;
    }
    /* Search for the first float anywhere in the value (matches FLOAT_RE.search). */
    int i = 0;
    while (i < len) {
        int save = i;
        float out;
        if (rc_svg_scan_float(v, len, &i, &out)) {
            return out;
        }
        i = save + 1;
    }
    return def;
}

/* Read a CSS property honoring the cascade: inline style="" beats the
   presentation attribute. Writes the value slice to valOut/lenOut; returns
   true if present (in either place). */
static bool rc_svg_css_property(const RcSvgElem *e, const char *name,
                                const char **valOut, int *lenOut) {
    int styleLen = 0;
    const char *style = rc_svg_attr(e, "style", &styleLen);
    if (style) {
        int i = 0;
        while (i < styleLen) {
            int declStart = i;
            while (i < styleLen && style[i] != ';') {
                i++;
            }
            int declEnd = i;   /* exclusive */
            if (i < styleLen) {
                i++;   /* skip ';' */
            }
            /* Split on the first ':'. */
            int colon = -1;
            for (int j = declStart; j < declEnd; j++) {
                if (style[j] == ':') { colon = j; break; }
            }
            if (colon < 0) {
                continue;
            }
            /* Trim the key. */
            int ka = declStart, kb = colon;
            while (ka < kb && isspace((unsigned char)style[ka])) ka++;
            while (kb > ka && isspace((unsigned char)style[kb - 1])) kb--;
            int klen = kb - ka;
            if (klen == (int)strlen(name) && strncmp(style + ka, name, (size_t)klen) == 0) {
                int va = colon + 1, vb = declEnd;
                while (va < vb && isspace((unsigned char)style[va])) va++;
                while (vb > va && isspace((unsigned char)style[vb - 1])) vb--;
                *valOut = style + va;
                *lenOut = vb - va;
                return true;
            }
        }
    }
    const char *pres = rc_svg_attr(e, name, lenOut);
    if (pres) {
        *valOut = pres;
        return true;
    }
    return false;
}

/* Which viewport dimension a percentage resolves against. SVG gives each
   attribute its own reference rather than one shared basis: x-family lengths
   against the width, y-family against the height, and a radius or a stroke
   width against the normalized diagonal, because neither belongs to one axis. */
typedef enum {
    RC_SVG_REF_W = 0,
    RC_SVG_REF_H,
    RC_SVG_REF_DIAG
} RcSvgRef;

static float rc_svg_ref_length(const RcSvgIcon *icon, RcSvgRef ref) {
    float w = icon ? icon->viewW : 0.0f;
    float h = icon ? icon->viewH : 0.0f;

    if (ref == RC_SVG_REF_W) {
        return w;
    }
    if (ref == RC_SVG_REF_H) {
        return h;
    }
    return sqrtf((w * w + h * h) / 2.0f);
}

/* The first float in `v`, resolved as a percentage when a '%' follows it.
   A bare number is user units and passes through unchanged, which is what an
   icon export gives you; any OTHER suffix (pt, em, mm) is ignored. */
/* One static instance so rc_svg_warn de-dups it by address: a file with twenty
   `pt` strokes says this once. */
static const char *const rc_svg_unit_suffix_msg =
    "a length carries a unit suffix this converter does not convert "
    "(pt pc mm cm in em ex rem); the bare number was kept as user units - "
    "write the converted number instead";

/* True when v[i..] is exactly one of the SVG 1.1 unit identifiers this converter
   does not convert, ASCII case-folded because CSS reads units without regard to
   case (`13pt`, `2EM`, `1.5rem`). The identifier must END there: `13ptx` is not
   a unit and stays silent, and so do `px` (harmless: 1 px IS 1 user unit) and a
   lone letter. No ctype call - a locale must not decide what a unit is. */
static bool rc_svg_unit_suffix_unconverted(const char *v, int len, int i) {
    static const char *const units[] = { "pt", "pc", "mm", "cm", "in", "em", "ex", "rem" };
    char u[4];
    int n = 0;

    while (i + n < len && n < 4) {
        char c = v[i + n];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c + ('a' - 'A'));
        } else if (c < 'a' || c > 'z') {
            break;
        }
        u[n++] = c;
    }
    if (n < 2 || n > 3) {
        return false;
    }
    for (size_t k = 0; k < sizeof(units) / sizeof(units[0]); k++) {
        if ((int)strlen(units[k]) == n && memcmp(units[k], u, (size_t)n) == 0) {
            return true;
        }
    }
    return false;
}

static float rc_svg_resolve_length(const char *v, int len, RcSvgIcon *icon,
                                   RcSvgRef ref, float def) {
    int i = 0;

    while (i < len) {
        int save = i;
        float out;
        if (rc_svg_scan_float(v, len, &i, &out)) {
            while (i < len && (v[i] == ' ' || v[i] == '\t')) {
                i++;
            }
            if (i < len && v[i] == '%') {
                out = out / 100.0f * rc_svg_ref_length(icon, ref);
            } else if (rc_svg_unit_suffix_unconverted(v, len, i)) {
                rc_svg_warn(icon, rc_svg_unit_suffix_msg);
            }
            return out;
        }
        i = save + 1;
    }
    return def;
}

static float rc_svg_attr_length(const RcSvgParse *ps, const RcSvgElem *e,
                                const char *name, float def, RcSvgRef ref) {
    int len = 0;
    const char *v = rc_svg_attr(e, name, &len);

    if (!v) {
        return def;
    }
    return rc_svg_resolve_length(v, len, ps ? ps->icon : NULL, ref, def);
}

static float rc_svg_stroke_width(const RcSvgElem *e, float inherited,
                                 RcSvgIcon *icon) {
    const char *v; int len;
    if (!rc_svg_css_property(e, "stroke-width", &v, &len)) {
        return inherited;
    }
    return rc_svg_resolve_length(v, len, icon, RC_SVG_REF_DIAG, inherited);
}

/* Resolve fill/stroke honoring the cascade + inheritance; root default = NONE. */
static RcSvgPaint rc_svg_read_paint(const RcSvgElem *e, const char *attr,
                                    RcSvgPaint inherited, RcSvgIcon *icon) {
    const char *v; int len;
    if (!rc_svg_css_property(e, attr, &v, &len)) {
        return inherited;   /* not specified -> inherit */
    }
    RcSvgPaint p = rc_svg_parse_color(v, len);
    if (p.kind == RC_PAINT_NONE) {
        /* Warn on an explicit-but-unsupported color (rgb()/hsl()/unknown name)
           so the author sees why the paint vanished. */
        char low[16];
        rc_svg_lower_trim(v, len, low, (int)sizeof(low));
        p.stated = (strcmp(low, "none") == 0);
        if (low[0] != '\0' && !p.stated) {
            p.unread = true;
            if (strcmp(attr, "fill") == 0) {
                rc_svg_warn(icon, "unrecognized fill color; shape paint dropped (use hex #rrggbb or a named color)");
            } else {
                rc_svg_warn(icon, "unrecognized stroke color; shape paint dropped (use hex #rrggbb or a named color)");
            }
        }
    }
    return p;
}

/* Is this element's fill-rule evenodd? The property inherits, so an element
   that does not state it, or states anything but the two rules ("inherit", a
   typo), keeps its parent's, as rcLoadSvg does. */
static bool rc_svg_fill_rule(const RcSvgElem *e, bool inherited) {
    const char *v;
    int         len;
    char        low[16];

    if (!rc_svg_css_property(e, "fill-rule", &v, &len)) {
        return inherited;
    }
    rc_svg_lower_trim(v, len, low, (int)sizeof(low));
    return strcmp(low, "evenodd") == 0 || (inherited && strcmp(low, "nonzero") != 0);
}

/* Does this element's stroke-linejoin miter? miter-clip and arcs join two
   straight segments as miter does; round and bevel do not. The property
   inherits, and a value SVG does not define is dropped, so the parent's
   stands. Only a square rect reads it: rcIconDrawPolyline has round joins
   alone. rcLoadSvg reads it the same way. */
static bool rc_svg_join_miters(const RcSvgElem *e, bool inherited) {
    const char *v;
    int         len;
    char        low[16];

    if (!rc_svg_css_property(e, "stroke-linejoin", &v, &len)) {
        return inherited;
    }
    rc_svg_lower_trim(v, len, low, (int)sizeof(low));
    if (!strcmp(low, "miter") || !strcmp(low, "miter-clip") || !strcmp(low, "arcs")) {
        return true;
    }
    if (!strcmp(low, "round") || !strcmp(low, "bevel")) {
        return false;
    }
    return inherited;
}

/* Does this element's stroke-miterlimit let a right angle miter (a limit of at
   least sqrt(2))? It inherits; a value that is not one plain number of at
   least 1 is dropped, so the parent's stands, as in rcLoadSvg. */
static bool rc_svg_limit_miters(const RcSvgElem *e, bool inherited) {
    const char *v;
    int         len, a = 0;
    float       limit;

    if (!rc_svg_css_property(e, "stroke-miterlimit", &v, &len)) {
        return inherited;
    }
    while (a < len && isspace((unsigned char)v[a])) {
        a++;
    }
    while (len > a && isspace((unsigned char)v[len - 1])) {
        len--;
    }
    if (!rc_svg_scan_float(v, len, &a, &limit) || a != len || !(limit >= 1.0f)) {
        return inherited;
    }
    return limit >= 1.41421356f;
}

/* Element scanning. */

static void rc_svg_strip_ns(const char *raw, int len, char *out, int cap) {
    /* Drop an XML namespace prefix ("svg:rect" -> "rect", "{ns}rect" -> "rect"). */
    int start = 0;
    for (int i = 0; i < len; i++) {
        if (raw[i] == ':' || raw[i] == '}') {
            start = i + 1;
        }
    }
    int n = len - start;
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(out, raw + start, (size_t)n);
    out[n] = '\0';
}

/* Does a RAW tag carry the prefix of an editor or metadata namespace? Inkscape
   writes <sodipodi:namedview> (with <inkscape:page>/<inkscape:grid> inside) into
   every file it saves, and the RDF license block nests rdf:, cc: and dc:. None of
   it is artwork, and a browser draws none of it. Decided before
   rc_svg_strip_ns, which would leave "namedview" to read as an unknown SVG
   element. The list is these conventional prefixes and nothing wider: a prefix
   names a namespace only through its xmlns declaration, which is not read, so
   an unlisted prefix is stripped like any other ("svg:" resolves as SVG). */
static bool rc_svg_prefix_is_editor_data(const char *raw, int len) {
    static const char *const prefixes[] = { "sodipodi", "inkscape", "rdf", "cc", "dc" };
    int colon = 0;
    while (colon < len && raw[colon] != ':') {
        colon++;
    }
    if (colon == 0 || colon >= len) {
        return false;
    }
    for (size_t k = 0; k < sizeof prefixes / sizeof prefixes[0]; k++) {
        if (strlen(prefixes[k]) == (size_t)colon &&
            strncmp(raw, prefixes[k], (size_t)colon) == 0) {
            return true;
        }
    }
    return false;
}

/* Parse the tag + attributes of a single '<...>' element token starting at s[*i]
   (which must point just past '<'). Advances *i past the closing '>'. Returns
   false only on a hard scan failure (unterminated tag). */
static bool rc_svg_scan_element(const char *s, int len, int *i, RcSvgElem *e) {
    memset(e, 0, sizeof(*e));

    int p = *i;
    if (p < len && s[p] == '/') {
        e->isClose = true;
        p++;
    }
    /* Tag name. */
    int nameStart = p;
    while (p < len && !isspace((unsigned char)s[p]) && s[p] != '>' && s[p] != '/') {
        p++;
    }
    e->editorData = rc_svg_prefix_is_editor_data(s + nameStart, p - nameStart);
    rc_svg_strip_ns(s + nameStart, p - nameStart, e->tag, (int)sizeof(e->tag));

    /* Attributes. */
    while (p < len) {
        while (p < len && isspace((unsigned char)s[p])) {
            p++;
        }
        if (p >= len) {
            return false;
        }
        if (s[p] == '>') {
            p++;
            *i = p;
            return true;
        }
        if (s[p] == '/') {
            e->selfClose = true;
            p++;
            /* Expect '>' next. */
            while (p < len && isspace((unsigned char)s[p])) p++;
            if (p < len && s[p] == '>') {
                p++;
            }
            *i = p;
            return true;
        }
        /* Attribute name. */
        int aStart = p;
        while (p < len && s[p] != '=' && !isspace((unsigned char)s[p]) && s[p] != '>' && s[p] != '/') {
            p++;
        }
        int aLen = p - aStart;
        char aname[32];
        rc_svg_strip_ns(s + aStart, aLen, aname, (int)sizeof(aname));

        while (p < len && isspace((unsigned char)s[p])) {
            p++;
        }
        const char *val = NULL;
        int vlen = 0;
        if (p < len && s[p] == '=') {
            p++;
            while (p < len && isspace((unsigned char)s[p])) {
                p++;
            }
            if (p < len && (s[p] == '"' || s[p] == '\'')) {
                char q = s[p];
                p++;
                int vStart = p;
                while (p < len && s[p] != q) {
                    p++;
                }
                val = s + vStart;
                vlen = p - vStart;
                if (p < len) {
                    p++;   /* skip closing quote */
                }
            } else {
                /* Unquoted value (tolerate). */
                int vStart = p;
                while (p < len && !isspace((unsigned char)s[p]) && s[p] != '>' && s[p] != '/') {
                    p++;
                }
                val = s + vStart;
                vlen = p - vStart;
            }
        }
        if (aname[0] != '\0' && e->attrCount < RC_SVG_MAX_ATTRS) {
            RcSvgAttr *at = &e->attrs[e->attrCount++];
            rc_svg_str_copy(at->name, aname, sizeof(at->name));
            at->val = val;
            at->valLen = vlen;
        }
    }
    return false;   /* unterminated */
}

/* viewBox / points-attr / path-data. */

static void rc_svg_parse_viewbox(const RcSvgElem *root, float *vx, float *vy,
                                 float *vw, float *vh) {
    int len = 0;
    const char *raw = rc_svg_attr(root, "viewBox", &len);
    if (!raw) {
        raw = rc_svg_attr(root, "viewbox", &len);
    }
    if (raw) {
        float vals[4];
        int n = 0, i = 0;
        while (i < len && n < 4) {
            rc_svg_skip_sep(raw, len, &i);
            if (i >= len) {
                break;
            }
            float out;
            int save = i;
            if (rc_svg_scan_float(raw, len, &i, &out)) {
                vals[n++] = out;
            } else {
                i = save + 1;
            }
        }
        if (n == 4) {
            *vx = vals[0]; *vy = vals[1]; *vw = vals[2]; *vh = vals[3];
            return;
        }
    }
    float width = rc_svg_attr_number(root, "width", 24.0f);
    float height = rc_svg_attr_number(root, "height", width);
    *vx = 0.0f; *vy = 0.0f; *vw = width; *vh = height;
}

/* Push a raw point (no flatten) onto the shape-point pool. */
static bool rc_svg_push_xy(RcSvgParse *ps, float x, float y) {
    RcSvgPoint p = { x, y };
    return rc_svg_push_pt(ps, p);
}

/* Parse a <polyline>/<polygon> "points" attribute into the shape-point pool.
   Returns the count pushed; a trailing odd coordinate is dropped. */
static int rc_svg_parse_points_attr(RcSvgParse *ps, const char *v, int len, int *offOut) {
    float nums[2];
    int have = 0;
    int start = ps->ptCount;
    *offOut = start;
    int i = 0;
    int pushed = 0;
    while (i < len) {
        rc_svg_skip_sep(v, len, &i);
        if (i >= len) {
            break;
        }
        float out;
        int save = i;
        if (!rc_svg_scan_float(v, len, &i, &out)) {
            i = save + 1;
            continue;
        }
        nums[have++] = out;
        if (have == 2) {
            if (!rc_svg_push_xy(ps, nums[0], nums[1])) {
                break;
            }
            pushed++;
            have = 0;
        }
    }
    return pushed;
}

/* Flatten SVG path data `d` into the shape-point pool as one or more subpaths,
   emitting a PATH shape per subpath. Ports parse_path_data + flush_subpath. */
static void rc_svg_parse_path_data(RcSvgParse *ps, const char *d, int len,
                                   float strokeWidth, RcSvgPaint fill, RcSvgPaint stroke);

/* Emit a PATH shape for a completed subpath [start, ps->ptCount). Applies the
   flush_subpath rules: <2 points -> drop; closed + duplicate endpoint -> drop it. */
static void rc_svg_flush_subpath(RcSvgParse *ps, int start, bool closed,
                                 float strokeWidth, RcSvgPaint fill, RcSvgPaint stroke) {
    int count = ps->ptCount - start;
    if (count < 2) {
        ps->ptCount = start;   /* discard a degenerate subpath's points */
        return;
    }
    if (closed && count >= 2 &&
        rc_svg_nearly_same(ps->pts[start], ps->pts[start + count - 1])) {
        ps->ptCount--;   /* drop the duplicate closing endpoint */
        count--;
    }
    if (ps->shapeCount >= RC_SVG_MAX_SHAPES) {
        if (!ps->overflow) {
            rc_svg_warn(ps->icon, "shape budget exhausted; remaining shapes skipped");
        }
        ps->overflow = true;
        return;
    }
    RcSvgShape *sh = &ps->shapes[ps->shapeCount++];
    memset(sh, 0, sizeof(*sh));
    sh->kind = RC_SHAPE_PATH;
    sh->ptOff = start;
    sh->ptCount = count;
    sh->closed = closed;
    sh->strokeWidth = strokeWidth;
    sh->fill = fill;
    sh->stroke = stroke;
    sh->elem = ps->elem;
    sh->evenodd = ps->evenodd;
}

static void rc_svg_parse_path_data(RcSvgParse *ps, const char *d, int len,
                                   float strokeWidth, RcSvgPaint fill, RcSvgPaint stroke) {
    int i = 0;
    char command = 0;
    char prevCommand = 0;
    RcSvgPoint current = { 0.0f, 0.0f };
    RcSvgPoint startPt = { 0.0f, 0.0f };
    int subStart = ps->ptCount;
    bool haveSub = false;
    bool reopenAtStart = false;   /* the last command was Z */
    RcSvgPoint lastCubic = { 0.0f, 0.0f }; bool haveCubic = false;
    RcSvgPoint lastQuad = { 0.0f, 0.0f };  bool haveQuad = false;

    /* Local: read the next float token, honoring separators. Returns false at a
       command boundary or end of input. */
    #define RC_SVG_READ_FLOAT(dst) \
        do { \
            rc_svg_skip_sep(d, len, &i); \
            if (i >= len || rc_svg_is_cmd_char(d[i])) { goto malformed; } \
            int _save = i; \
            if (!rc_svg_scan_float(d, len, &i, &(dst))) { i = _save; goto malformed; } \
        } while (0)

    /* Local: read an arc flag. A flag is one character, so there is no index to
       restore on failure, and no separate command-boundary test is needed - a
       command character is never '0' or '1'. */
    #define RC_SVG_READ_FLAG(dst) \
        do { \
            rc_svg_skip_sep(d, len, &i); \
            if (!rc_svg_scan_flag(d, len, &i, &(dst))) { goto malformed; } \
        } while (0)

    for (;;) {
        rc_svg_skip_sep(d, len, &i);
        if (i >= len) {
            break;
        }
        if (rc_svg_is_cmd_char(d[i])) {
            command = d[i];
            i++;
        } else if (command == 0) {
            rc_svg_warn(ps->icon, "path data starts with numbers before a command; skipped remainder");
            break;
        }

        char upper = (char)toupper((unsigned char)command);
        bool relative = islower((unsigned char)command) != 0;

        if (upper == 'Z') {
            if (haveSub) {
                rc_svg_flush_subpath(ps, subStart, true, strokeWidth, fill, stroke);
            }
            subStart = ps->ptCount;
            haveSub = false;
            reopenAtStart = true;
            current = startPt;
            haveCubic = false;
            haveQuad = false;
            prevCommand = command;
            command = 0;
            continue;
        }

        if (upper != 'M' && upper != 'L' && upper != 'H' && upper != 'V' &&
            upper != 'C' && upper != 'S' && upper != 'Q' && upper != 'T' && upper != 'A') {
            rc_svg_warn(ps->icon, "unsupported path command; skipped");
            break;
        }

        /* SVG 1.1 section 8.3.3: a drawing command right after Z starts a new
           subpath at the closed one's initial point, which `current` holds. */
        if (reopenAtStart && upper != 'M') {
            subStart = ps->ptCount;
            if (!rc_svg_push_pt(ps, current)) { goto done; }
            haveSub = true;
        }
        reopenAtStart = false;

        bool firstMoveto = (upper == 'M');
        char curUpper = upper;   /* mutated: M's repeated pairs become L */

        while (i < len) {
            rc_svg_skip_sep(d, len, &i);
            if (i >= len || rc_svg_is_cmd_char(d[i])) {
                break;
            }
            if (curUpper == 'M') {
                float x, y;
                RC_SVG_READ_FLOAT(x);
                RC_SVG_READ_FLOAT(y);
                RcSvgPoint pt = relative ? RC_LIT(RcSvgPoint){ current.x + x, current.y + y }
                                         : RC_LIT(RcSvgPoint){ x, y };
                if (firstMoveto) {
                    if (haveSub) {
                        rc_svg_flush_subpath(ps, subStart, false, strokeWidth, fill, stroke);
                    }
                    subStart = ps->ptCount;
                    if (!rc_svg_push_pt(ps, pt)) { goto done; }
                    haveSub = true;
                    current = pt;
                    startPt = pt;
                    firstMoveto = false;
                    curUpper = 'L';   /* implicit L for repeated coords */
                } else {
                    if (!rc_svg_push_pt(ps, pt)) { goto done; }
                    current = pt;
                }
                haveCubic = false;
                haveQuad = false;
            } else if (curUpper == 'L') {
                float x, y;
                RC_SVG_READ_FLOAT(x);
                RC_SVG_READ_FLOAT(y);
                RcSvgPoint end = relative ? RC_LIT(RcSvgPoint){ current.x + x, current.y + y }
                                          : RC_LIT(RcSvgPoint){ x, y };
                if (!rc_svg_push_pt(ps, end)) { goto done; }
                current = end;
                haveCubic = false; haveQuad = false;
            } else if (curUpper == 'H') {
                float x;
                RC_SVG_READ_FLOAT(x);
                RcSvgPoint end = relative ? RC_LIT(RcSvgPoint){ current.x + x, current.y }
                                          : RC_LIT(RcSvgPoint){ x, current.y };
                if (!rc_svg_push_pt(ps, end)) { goto done; }
                current = end;
                haveCubic = false; haveQuad = false;
            } else if (curUpper == 'V') {
                float y;
                RC_SVG_READ_FLOAT(y);
                RcSvgPoint end = relative ? RC_LIT(RcSvgPoint){ current.x, current.y + y }
                                          : RC_LIT(RcSvgPoint){ current.x, y };
                if (!rc_svg_push_pt(ps, end)) { goto done; }
                current = end;
                haveCubic = false; haveQuad = false;
            } else if (curUpper == 'C') {
                float x1, y1, x2, y2, x, y;
                RC_SVG_READ_FLOAT(x1); RC_SVG_READ_FLOAT(y1);
                RC_SVG_READ_FLOAT(x2); RC_SVG_READ_FLOAT(y2);
                RC_SVG_READ_FLOAT(x);  RC_SVG_READ_FLOAT(y);
                RcSvgPoint c1 = relative ? RC_LIT(RcSvgPoint){ current.x + x1, current.y + y1 }
                                         : RC_LIT(RcSvgPoint){ x1, y1 };
                RcSvgPoint c2 = relative ? RC_LIT(RcSvgPoint){ current.x + x2, current.y + y2 }
                                         : RC_LIT(RcSvgPoint){ x2, y2 };
                RcSvgPoint end = relative ? RC_LIT(RcSvgPoint){ current.x + x, current.y + y }
                                          : RC_LIT(RcSvgPoint){ x, y };
                for (int step = 1; step <= ps->curveSteps; step++) {
                    RcSvgPoint pp = rc_svg_cubic(current, c1, c2, end, (float)step / (float)ps->curveSteps);
                    if (!rc_svg_push_pt(ps, pp)) { goto done; }
                }
                current = end;
                lastCubic = c2; haveCubic = true; haveQuad = false;
            } else if (curUpper == 'S') {
                float x2, y2, x, y;
                RC_SVG_READ_FLOAT(x2); RC_SVG_READ_FLOAT(y2);
                RC_SVG_READ_FLOAT(x);  RC_SVG_READ_FLOAT(y);
                RcSvgPoint c1;
                char pu = prevCommand ? (char)toupper((unsigned char)prevCommand) : 0;
                if ((pu == 'C' || pu == 'S') && haveCubic) {
                    c1 = RC_LIT(RcSvgPoint){ 2.0f * current.x - lastCubic.x, 2.0f * current.y - lastCubic.y };
                } else {
                    c1 = current;
                }
                RcSvgPoint c2 = relative ? RC_LIT(RcSvgPoint){ current.x + x2, current.y + y2 }
                                         : RC_LIT(RcSvgPoint){ x2, y2 };
                RcSvgPoint end = relative ? RC_LIT(RcSvgPoint){ current.x + x, current.y + y }
                                          : RC_LIT(RcSvgPoint){ x, y };
                for (int step = 1; step <= ps->curveSteps; step++) {
                    RcSvgPoint pp = rc_svg_cubic(current, c1, c2, end, (float)step / (float)ps->curveSteps);
                    if (!rc_svg_push_pt(ps, pp)) { goto done; }
                }
                current = end;
                lastCubic = c2; haveCubic = true; haveQuad = false;
            } else if (curUpper == 'Q') {
                float x1, y1, x, y;
                RC_SVG_READ_FLOAT(x1); RC_SVG_READ_FLOAT(y1);
                RC_SVG_READ_FLOAT(x);  RC_SVG_READ_FLOAT(y);
                RcSvgPoint c = relative ? RC_LIT(RcSvgPoint){ current.x + x1, current.y + y1 }
                                        : RC_LIT(RcSvgPoint){ x1, y1 };
                RcSvgPoint end = relative ? RC_LIT(RcSvgPoint){ current.x + x, current.y + y }
                                          : RC_LIT(RcSvgPoint){ x, y };
                for (int step = 1; step <= ps->curveSteps; step++) {
                    RcSvgPoint pp = rc_svg_quad(current, c, end, (float)step / (float)ps->curveSteps);
                    if (!rc_svg_push_pt(ps, pp)) { goto done; }
                }
                current = end;
                lastQuad = c; haveQuad = true; haveCubic = false;
            } else if (curUpper == 'T') {
                float x, y;
                RC_SVG_READ_FLOAT(x); RC_SVG_READ_FLOAT(y);
                RcSvgPoint c;
                char pu = prevCommand ? (char)toupper((unsigned char)prevCommand) : 0;
                if ((pu == 'Q' || pu == 'T') && haveQuad) {
                    c = RC_LIT(RcSvgPoint){ 2.0f * current.x - lastQuad.x, 2.0f * current.y - lastQuad.y };
                } else {
                    c = current;
                }
                RcSvgPoint end = relative ? RC_LIT(RcSvgPoint){ current.x + x, current.y + y }
                                          : RC_LIT(RcSvgPoint){ x, y };
                for (int step = 1; step <= ps->curveSteps; step++) {
                    RcSvgPoint pp = rc_svg_quad(current, c, end, (float)step / (float)ps->curveSteps);
                    if (!rc_svg_push_pt(ps, pp)) { goto done; }
                }
                current = end;
                lastQuad = c; haveQuad = true; haveCubic = false;
            } else if (curUpper == 'A') {
                float rx, ry, rot, x, y;
                bool la, sw;
                RC_SVG_READ_FLOAT(rx); RC_SVG_READ_FLOAT(ry); RC_SVG_READ_FLOAT(rot);
                RC_SVG_READ_FLAG(la);  RC_SVG_READ_FLAG(sw);
                RC_SVG_READ_FLOAT(x);  RC_SVG_READ_FLOAT(y);
                RcSvgPoint end = relative ? RC_LIT(RcSvgPoint){ current.x + x, current.y + y }
                                          : RC_LIT(RcSvgPoint){ x, y };
                rc_svg_arc(ps, current, rx, ry, rot, la, sw, end, ps->arcDeg);
                current = end;
                haveCubic = false; haveQuad = false;
            }
            prevCommand = command;
            continue;

        malformed:
            rc_svg_warn(ps->icon, "malformed path; skipped incomplete tail");
            /* Load-bearing: skip to the next command character. The float scan
               RESTORES the index when it fails, so without this the outer loop
               re-enters on the same byte forever - any byte that cannot start a
               number does it, including the lead byte of a UTF-8 character.
               Skipping rather than abandoning the path keeps the valid geometry
               that follows the bad token. */
            while (i < len && !rc_svg_is_cmd_char(d[i])) {
                i++;
            }
            goto after_params;
        }

    after_params:
        /* After an explicit M, subsequent repeated pairs are L (same relativity).
           (curUpper already flipped to L; keep `command` as M for prevCommand,
           then normalize the token so a following bare number continues as L.) */
        if (command != 0 && (command == 'M' || command == 'm')) {
            command = islower((unsigned char)command) ? 'l' : 'L';
        }
        /* Reaching here via `malformed` does NOT stop the whole path: parsing
           resumes at the next command character and every point accumulated
           before the bad token is kept. The result is a PARTIAL path, not a
           dropped one. */
        continue;
    }

done:
    if (haveSub) {
        rc_svg_flush_subpath(ps, subStart, false, strokeWidth, fill, stroke);
    }

    #undef RC_SVG_READ_FLOAT
    #undef RC_SVG_READ_FLAG
}

/* Shape collection: recursive, bounded depth. */

/* One static instance, compared by ADDRESS: a skipped editor subtree hands back
   every warning it raised but this one (rc_svg_handle_element). */
static const char *const rc_svg_deep_msg = "element nesting too deep; subtree skipped";

/* ---- Transforms: the transform attribute, as rcLoadSvg and a
   browser apply it. A copy of the library's code, kept in step by hand until
   this converter parses with the library. ---- */

static const RcSvgMatrix rc_svg_identity = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };

/* The transform that applies q first, then p. */
static RcSvgMatrix rc_svg_matrix_mul(RcSvgMatrix p, RcSvgMatrix q) {
    RcSvgMatrix r;

    r.a = p.a * q.a + p.c * q.b;
    r.b = p.b * q.a + p.d * q.b;
    r.c = p.a * q.c + p.c * q.d;
    r.d = p.b * q.c + p.d * q.d;
    r.e = p.a * q.e + p.c * q.f + p.e;
    r.f = p.b * q.e + p.d * q.f + p.f;
    return r;
}

static const char *const rc_svg_transform_bad_msg =
    "a transform that does not parse is ignored, as a browser ignores it; "
    "write it as matrix, translate, scale, rotate, skewX or skewY with plain numbers";

/* Parse a transform list, the left one outermost, angles in degrees. Any
   mistake voids the whole list, as in Chrome. True with *out set on success. */
static bool rc_svg_parse_transform(const char *v, int len, RcSvgMatrix *out) {
    RcSvgMatrix m = rc_svg_identity;
    int         i = 0;

    for (;;) {
        char  name[12];   /* the longest name, "translate", and a NUL */
        float n[6];
        int   count = 0, nameLen = 0;
        RcSvgMatrix t = rc_svg_identity;

        rc_svg_skip_sep(v, len, &i);
        if (i >= len) {
            break;
        }
        while (i < len && isalpha((unsigned char)v[i])) {
            if (nameLen + 1 >= (int)sizeof(name)) {
                return false;
            }
            name[nameLen++] = v[i++];
        }
        name[nameLen] = '\0';
        while (i < len && isspace((unsigned char)v[i])) {
            i++;
        }
        if (nameLen == 0 || i >= len || v[i] != '(') {
            return false;
        }
        for (i++;;) {
            rc_svg_skip_sep(v, len, &i);
            if (i < len && v[i] == ')') {
                i++;
                break;
            }
            if (count == 6 || !rc_svg_scan_float(v, len, &i, &n[count]) ||
                !isfinite(n[count])) {
                return false;
            }
            count++;
        }
        if (strcmp(name, "matrix") == 0 && count == 6) {
            t.a = n[0]; t.b = n[1]; t.c = n[2]; t.d = n[3]; t.e = n[4]; t.f = n[5];
        } else if (strcmp(name, "translate") == 0 && (count == 1 || count == 2)) {
            t.e = n[0];
            t.f = count == 2 ? n[1] : 0.0f;
        } else if (strcmp(name, "scale") == 0 && (count == 1 || count == 2)) {
            t.a = n[0];
            t.d = count == 2 ? n[1] : n[0];
        } else if (strcmp(name, "rotate") == 0 && (count == 1 || count == 3)) {
            const double r = fmod((double)n[0], 360.0) * M_PI / 180.0;
            const float  cs = (float)cos(r), sn = (float)sin(r);

            t.a = cs; t.b = sn; t.c = -sn; t.d = cs;
            if (count == 3) {
                t.e = n[1] - cs * n[1] + sn * n[2];
                t.f = n[2] - sn * n[1] - cs * n[2];
            }
        } else if (strcmp(name, "skewX") == 0 && count == 1) {
            t.c = (float)tan(fmod((double)n[0], 360.0) * M_PI / 180.0);
        } else if (strcmp(name, "skewY") == 0 && count == 1) {
            t.b = (float)tan(fmod((double)n[0], 360.0) * M_PI / 180.0);
        } else {
            return false;
        }
        m = rc_svg_matrix_mul(m, t);
    }
    if (!isfinite(m.a) || !isfinite(m.b) || !isfinite(m.c) || !isfinite(m.d) ||
        !isfinite(m.e) || !isfinite(m.f)) {
        return false;
    }
    *out = m;
    return true;
}

/* Compose an element's transform attribute into the parse's; a list that does
   not parse leaves it as it was, and warns. */
static void rc_svg_apply_transform(RcSvgParse *ps, const RcSvgElem *e) {
    int         len = 0;
    const char *v = rc_svg_attr(e, "transform", &len);
    RcSvgMatrix t;

    if (!v) {
        return;
    }
    if (!rc_svg_parse_transform(v, len, &t)) {
        rc_svg_warn(ps->icon, rc_svg_transform_bad_msg);
        return;
    }
    ps->ctm = rc_svg_matrix_mul(ps->transformed ? ps->ctm : rc_svg_identity, t);
    ps->transformed = memcmp(&ps->ctm, &rc_svg_identity, sizeof(ps->ctm)) != 0;
}

/* How much a transform scales a stroke's width: its area scale's root. */
static float rc_svg_matrix_width_scale(const RcSvgMatrix *m) {
    return sqrtf(fabsf(m->a * m->d - m->b * m->c));
}

/* Does a transform keep circles round: an even scale with a turn or a mirror? */
static bool rc_svg_matrix_is_similar(const RcSvgMatrix *m) {
    const float x2 = m->a * m->a + m->b * m->b, y2 = m->c * m->c + m->d * m->d;
    const float tol = 1e-5f * (x2 + y2);

    return fabsf(x2 - y2) <= tol && fabsf(m->a * m->c + m->b * m->d) <= tol;
}

static void rc_svg_add_shape(RcSvgParse *ps, RcSvgShape sh);

/* Push a rounded rectangle as the closed path SVG 2 says a <rect> is (a circle
   or ellipse is one whose radii are half its sides), clockwise from its top,
   every point placed by the transform, and flush it as one PATH shape. */
static void rc_svg_push_round_rect_path(RcSvgParse *ps, float x, float y, float w, float h,
                                        float rx, float ry, float strokeW,
                                        RcSvgPaint fill, RcSvgPaint stroke) {
    const int start = ps->ptCount;

    if (!(rx > 0.0f) || !(ry > 0.0f)) {
        rx = ry = 0.0f;
    }
    rx = fminf(rx, 0.5f * w);
    ry = fminf(ry, 0.5f * h);

    const RcSvgPoint from[4] = { { x + rx, y }, { x + w, y + ry },
                                 { x + w - rx, y + h }, { x, y + h - ry } };
    const RcSvgPoint to[4]   = { { x + w - rx, y }, { x + w, y + h - ry },
                                 { x + rx, y + h }, { x, y + ry } };
    RcSvgPoint       last = from[0];

    rc_svg_push_pt(ps, from[0]);
    for (int k = 0; k < 4; k++) {
        const RcSvgPoint next = from[(k + 1) % 4];

        if (!rc_svg_nearly_same(to[k], last)) {
            rc_svg_push_pt(ps, to[k]);
        }
        if (rx > 0.0f) {
            rc_svg_arc(ps, to[k], rx, ry, 0.0f, false, true, next, ps->arcDeg);
        } else if (!rc_svg_nearly_same(next, to[k])) {
            rc_svg_push_pt(ps, next);
        }
        last = next;
    }
    rc_svg_flush_subpath(ps, start, true, strokeW, fill, stroke);
}

/* Add a rect, circle or ellipse read in its element's user space, placed by the
   transform: the primitive kept under an axis-aligned scale and move (a circle
   scaled unevenly an ellipse; a circle under a turn or mirror with an even
   scale a circle), else the closed path it equals. */
static void rc_svg_add_basic_shape(RcSvgParse *ps, RcSvgShape sh) {
    const RcSvgMatrix *m = &ps->ctm;

    if (!ps->transformed) {
        rc_svg_add_shape(ps, sh);
        return;
    }
    if (m->b == 0.0f && m->c == 0.0f && m->a != 0.0f && m->d != 0.0f) {
        const float sx = fabsf(m->a), sy = fabsf(m->d);

        if (sh.kind == RC_SHAPE_RECT) {
            const float x0 = m->a * sh.x + m->e, x1 = m->a * (sh.x + sh.w) + m->e;
            const float y0 = m->d * sh.y + m->f, y1 = m->d * (sh.y + sh.h) + m->f;

            sh.x = fminf(x0, x1);
            sh.y = fminf(y0, y1);
            sh.w *= sx;
            sh.h *= sy;
            sh.rx *= sx;
            sh.ry *= sy;
        } else {
            sh.cx = m->a * sh.cx + m->e;
            sh.cy = m->d * sh.cy + m->f;
            if (sh.kind == RC_SHAPE_ELLIPSE) {
                sh.rx *= sx;
                sh.ry *= sy;
            } else if (sx == sy) {
                sh.r *= sx;
            } else {
                sh.kind = RC_SHAPE_ELLIPSE;
                sh.rx = sx * sh.r;
                sh.ry = sy * sh.r;
            }
        }
        rc_svg_add_shape(ps, sh);
        return;
    }
    if (sh.kind == RC_SHAPE_CIRCLE && rc_svg_matrix_is_similar(m)) {
        const float cx = sh.cx;

        sh.cx = m->a * cx + m->c * sh.cy + m->e;
        sh.cy = m->b * cx + m->d * sh.cy + m->f;
        sh.r *= rc_svg_matrix_width_scale(m);
        rc_svg_add_shape(ps, sh);
        return;
    }
    if (sh.kind == RC_SHAPE_RECT) {
        rc_svg_push_round_rect_path(ps, sh.x, sh.y, sh.w, sh.h, sh.rx, sh.ry,
                                    sh.strokeWidth, sh.fill, sh.stroke);
    } else {
        const float rx = sh.kind == RC_SHAPE_CIRCLE ? sh.r : sh.rx;
        const float ry = sh.kind == RC_SHAPE_CIRCLE ? sh.r : sh.ry;

        rc_svg_push_round_rect_path(ps, sh.cx - rx, sh.cy - ry, 2.0f * rx, 2.0f * ry, rx, ry,
                                    sh.strokeWidth, sh.fill, sh.stroke);
    }
}

static void rc_svg_add_shape(RcSvgParse *ps, RcSvgShape sh) {
    if (ps->shapeCount >= RC_SVG_MAX_SHAPES) {
        if (!ps->overflow) {
            rc_svg_warn(ps->icon, "shape budget exhausted; remaining shapes skipped");
        }
        ps->overflow = true;
        return;
    }
    ps->shapes[ps->shapeCount++] = sh;
}

/* Recursively collect drawable shapes under `s[*i]`, having just consumed the
   opening tag of `parent`. Iterates sibling elements until the matching close
   tag (or EOF). `depth` bounds recursion. */
static void rc_svg_collect(RcSvgParse *ps, const char *s, int len, int *i,
                           const RcSvgElem *parent, float inhStrokeW,
                           RcSvgPaint inhFill, RcSvgPaint inhStroke, int depth);

/* The ATTRIBUTE half of the unsupported-feature report, and it exists because an
   unsupported ELEMENT disappears, which an author notices, while an unsupported
   ATTRIBUTE changes the picture in SILENCE. An opacity draws fully opaque, a
   clip-path draws the clipped shape whole (a transform is applied:
   rc_svg_apply_transform). Each of those is a plausible-looking icon that is
   wrong, and nothing else in this file would say so.

   The lookup is exact-name strcmp, NOT substring, and that is load-bearing:
   gradientTransform and patternTransform are real SVG attributes on families
   that already report on their own, so a substring test would fire on them.

   stroke-linecap and stroke-linejoin are ignored too and deliberately do NOT
   report: they appear on most ordinary artwork, and a diagnostic that fires on
   healthy input teaches people to stop reading the channel.

   rc_svg_warn de-dups on the static string pointer, so each message here is
   reported once per document however many elements carry the attribute. */
static void rc_svg_report_ignored_attrs(RcSvgIcon *icon, const RcSvgElem *e) {
    static const char *const opacity_msg =
        "opacity is ignored - shapes draw fully opaque; carry the alpha in an #rrggbbaa fill instead";
    static const char *const clip_msg =
        "clip-path is ignored - the clipped shape draws whole; resolve the clip into explicit geometry";
    static const char *const hidden_msg =
        "display/visibility are ignored - a hidden element still draws; delete it instead";
    static const char *const cliprule_msg =
        "clip-rule is ignored - the clipped shape draws whole; "
        "resolve the clip into explicit geometry";
    static const char *const dash_msg =
        "stroke-dasharray is ignored - the stroke draws solid";
    static const struct {
        const char *name;
        const char *const *msg;
    } table[] = {
        { "clip-path",        &clip_msg      },
        { "opacity",          &opacity_msg   },
        { "fill-opacity",     &opacity_msg   },
        { "stroke-opacity",   &opacity_msg   },
        { "display",          &hidden_msg    },
        { "visibility",       &hidden_msg    },
        { "clip-rule",        &cliprule_msg  },
        { "stroke-dasharray", &dash_msg      },
    };
    int i;

    for (i = 0; i < (int)(sizeof table / sizeof table[0]); i++) {
        if (rc_svg_attr(e, table[i].name, NULL)) {
            rc_svg_warn(icon, *table[i].msg);
        }
    }
}

/* Is this tag a definition container? Its children DECLARE content for a
   later reference (<use>, fill="url(#...)", clip-path) and a browser draws
   none of them where they sit; rcLoadSvg discards them the same way.
   These nine are the set that can carry
   drawable children; Figma writes a bare <clipPath> with no <defs> round it. */
static bool rc_svg_tag_is_definition(const char *tag) {
    static const char *const defn[] = {
        "defs", "clipPath", "mask", "pattern", "marker",
        "symbol", "linearGradient", "radialGradient", "filter"
    };
    size_t k;
    for (k = 0; k < sizeof defn / sizeof defn[0]; k++)
        if (strcmp(tag, defn[k]) == 0)
            return true;
    return false;
}

/* Handle one element: read its paints, emit its own shape(s), recurse for
   containers. `e` is already scanned; `s[*i]` sits just past its '>'. */
static void rc_svg_handle_element(RcSvgParse *ps, const char *s, int len, int *i,
                                  const RcSvgElem *e, float inhStrokeW,
                                  RcSvgPaint inhFill, RcSvgPaint inhStroke, int depth) {
    /* EDITOR DATA AND <metadata> ARE NOT ARTWORK: the element and its WHOLE
       subtree draw nothing and warn nothing. No attribute of the element is
       read. A warning anywhere in there would fire on healthy files - every file
       Inkscape saves (its <inkscape:grid> even carries an `opacity`), and the
       <sfw> or XMP elements Illustrator nests in <metadata> - and the one generic
       "unsupported element" line it cost would also de-dup away the report of a
       genuinely dropped element later in the file. The subtree is still WALKED
       so the parse index passes it; the shapes, points and overflow latch it
       collected are handed back, and so is every warning it raised but one:
       nesting too deep, whose damage reaches past the subtree (the parse index
       loses its place, and the artwork after it can be lost). The one fact it
       leaves behind is sawStyle: a <style> in there still counts as a sheet, so
       no shape is dropped as invisible (rc_svg2icon_parse). */
    if (e->editorData || strcmp(e->tag, "metadata") == 0) {
        if (!e->selfClose) {
            const int  savedShapes   = ps->shapeCount;
            const int  savedPts      = ps->ptCount;
            const bool savedOverflow = ps->overflow;
            const int  savedWarns    = ps->icon ? ps->icon->warnCount : 0;

            rc_svg_collect(ps, s, len, i, e, inhStrokeW, inhFill, inhStroke, depth + 1);

            ps->shapeCount = savedShapes;
            ps->ptCount    = savedPts;
            ps->overflow   = savedOverflow;
            if (ps->icon) {
                bool deep = false;

                for (int w = savedWarns; w < ps->icon->warnCount; w++) {
                    deep = deep || ps->icon->warn[w] == rc_svg_deep_msg;
                }
                ps->icon->warnCount = savedWarns;   /* the ring only appends */
                if (deep) {
                    rc_svg_warn(ps->icon, rc_svg_deep_msg);
                }
            }
        }
        return;
    }

    float strokeWidth = rc_svg_stroke_width(e, inhStrokeW, ps->icon);
    RcSvgPaint fill = rc_svg_read_paint(e, "fill", inhFill, ps->icon);
    RcSvgPaint stroke = rc_svg_read_paint(e, "stroke", inhStroke, ps->icon);
    /* The transform in force goes down through ps and is put back when the
       element is done. A stroke scales with it; the children inherit the width
       as written. An uneven stretch or a skew draws the mean width, and says so. */
    const RcSvgMatrix inhCtm = ps->ctm;
    const bool inhTransformed = ps->transformed;
    /* The fill-rule goes down the same way: this element's shapes and its
       children read it from ps, and it is put back when the element is done. */
    const bool inhEvenodd = ps->evenodd;
    const bool inhJoin = ps->joinMiters, inhLimit = ps->limitMiters;
    ps->evenodd = rc_svg_fill_rule(e, inhEvenodd);
    ps->joinMiters = rc_svg_join_miters(e, inhJoin);
    ps->limitMiters = rc_svg_limit_miters(e, inhLimit);
    ps->elem++;
    rc_svg_apply_transform(ps, e);
    float drawnStrokeW = strokeWidth;
    if (ps->transformed) {
        drawnStrokeW = strokeWidth * rc_svg_matrix_width_scale(&ps->ctm);
        if (stroke.kind != RC_PAINT_NONE && strokeWidth > 0.0f &&
            !rc_svg_matrix_is_similar(&ps->ctm)) {
            rc_svg_warn(ps->icon, "a transform that stretches unevenly or skews draws a "
                                   "stroke at one width, where a browser varies it with "
                                   "direction");
        }
    }

    rc_svg_report_ignored_attrs(ps->icon, e);

    const char *tag = e->tag;

    if (strcmp(tag, "path") == 0) {
        int dLen = 0;
        const char *d = rc_svg_attr(e, "d", &dLen);
        if (d && dLen > 0) {
            rc_svg_parse_path_data(ps, d, dLen, drawnStrokeW, fill, stroke);
        }
    } else if (strcmp(tag, "line") == 0) {
        RcSvgShape sh; memset(&sh, 0, sizeof(sh));
        sh.kind = RC_SHAPE_PATH;
        int off = ps->ptCount;
        bool ok1 = rc_svg_push_xy(ps, rc_svg_attr_length(ps, e, "x1", 0.0f, RC_SVG_REF_W),
                                 rc_svg_attr_length(ps, e, "y1", 0.0f, RC_SVG_REF_H));
        bool ok2 = rc_svg_push_xy(ps, rc_svg_attr_length(ps, e, "x2", 0.0f, RC_SVG_REF_W),
                                 rc_svg_attr_length(ps, e, "y2", 0.0f, RC_SVG_REF_H));
        if (ok1 && ok2) {
            sh.ptOff = off; sh.ptCount = 2; sh.closed = false;
            sh.strokeWidth = drawnStrokeW; sh.fill = fill; sh.stroke = stroke;
            sh.elem = ps->elem;
            rc_svg_add_shape(ps, sh);
        }
    } else if (strcmp(tag, "polyline") == 0 || strcmp(tag, "polygon") == 0) {
        int pLen = 0;
        const char *pts = rc_svg_attr(e, "points", &pLen);
        if (pts && pLen > 0) {
            int off = 0;
            int n = rc_svg_parse_points_attr(ps, pts, pLen, &off);
            if (n >= 2) {
                RcSvgShape sh; memset(&sh, 0, sizeof(sh));
                sh.kind = RC_SHAPE_PATH;
                sh.ptOff = off; sh.ptCount = n;
                sh.closed = (strcmp(tag, "polygon") == 0);
                sh.strokeWidth = drawnStrokeW; sh.fill = fill; sh.stroke = stroke;
                sh.elem = ps->elem; sh.evenodd = ps->evenodd;
                rc_svg_add_shape(ps, sh);
            } else {
                ps->ptCount = off;   /* roll back a too-short points list */
            }
        }
    } else if (strcmp(tag, "rect") == 0) {
        float x = rc_svg_attr_length(ps, e, "x", 0.0f, RC_SVG_REF_W);
        float y = rc_svg_attr_length(ps, e, "y", 0.0f, RC_SVG_REF_H);
        float w = rc_svg_attr_length(ps, e, "width", 0.0f, RC_SVG_REF_W);
        float h = rc_svg_attr_length(ps, e, "height", 0.0f, RC_SVG_REF_H);
        float rx = rc_svg_attr_length(ps, e, "rx", 0.0f, RC_SVG_REF_W);
        float ry = rc_svg_attr_length(ps, e, "ry", rx, RC_SVG_REF_H);
        /* `rx: auto` is the ry-from-rx rule the other way round: an ABSENT rx
           takes ry's used length, so a ry-only rect rounds as it does in a
           browser. Keyed on presence, not value - an explicit rx="0" stays square. */
        if (!rc_svg_attr(e, "rx", NULL)) {
            rx = ry;
        }
        /* A corner rounds only by two radii above 0, as a browser draws it: a
           zero radius on one axis makes the corner square. */
        if (!(rx > 0.0f && ry > 0.0f)) {
            rx = ry = 0.0f;
        }
        if (w > 0.0f && h > 0.0f) {
            RcSvgShape sh; memset(&sh, 0, sizeof(sh));
            sh.kind = RC_SHAPE_RECT;
            sh.x = x; sh.y = y; sh.w = w; sh.h = h; sh.rx = rx; sh.ry = ry;
            sh.strokeWidth = drawnStrokeW; sh.fill = fill; sh.stroke = stroke;
            sh.elem = ps->elem;
            /* A square rect strokes as one closed run with no curve, so its
               corners take the join in force: mitered by the helper at a radius
               of 0, or rcIconDrawPolyline's round ones. */
            sh.mitered = rx == 0.0f && ps->joinMiters && ps->limitMiters;
            rc_svg_add_basic_shape(ps, sh);
        }
    } else if (strcmp(tag, "circle") == 0) {
        float cx = rc_svg_attr_length(ps, e, "cx", 0.0f, RC_SVG_REF_W);
        float cy = rc_svg_attr_length(ps, e, "cy", 0.0f, RC_SVG_REF_H);
        float r = rc_svg_attr_length(ps, e, "r", 0.0f, RC_SVG_REF_DIAG);
        if (r > 0.0f) {
            RcSvgShape sh; memset(&sh, 0, sizeof(sh));
            sh.kind = RC_SHAPE_CIRCLE;
            sh.cx = cx; sh.cy = cy; sh.r = r;
            sh.strokeWidth = drawnStrokeW; sh.fill = fill; sh.stroke = stroke;
            sh.elem = ps->elem;
            rc_svg_add_basic_shape(ps, sh);
        }
    } else if (strcmp(tag, "ellipse") == 0) {
        float cx = rc_svg_attr_length(ps, e, "cx", 0.0f, RC_SVG_REF_W);
        float cy = rc_svg_attr_length(ps, e, "cy", 0.0f, RC_SVG_REF_H);
        float rx = rc_svg_attr_length(ps, e, "rx", 0.0f, RC_SVG_REF_W);
        float ry = rc_svg_attr_length(ps, e, "ry", 0.0f, RC_SVG_REF_H);
        if (rx > 0.0f && ry > 0.0f) {
            RcSvgShape sh; memset(&sh, 0, sizeof(sh));
            sh.kind = RC_SHAPE_ELLIPSE;
            sh.cx = cx; sh.cy = cy; sh.rx = rx; sh.ry = ry;
            sh.strokeWidth = drawnStrokeW; sh.fill = fill; sh.stroke = stroke;
            sh.elem = ps->elem;
            rc_svg_add_basic_shape(ps, sh);
        }
    } else if (strcmp(tag, "svg") == 0 || strcmp(tag, "g") == 0 ||
               strcmp(tag, "defs") == 0 || strcmp(tag, "title") == 0 ||
               strcmp(tag, "desc") == 0 || strcmp(tag, "style") == 0) {
        /* Pass-through container: shapes come from its children. */
        if (strcmp(tag, "style") == 0) {
            ps->sawStyle = true;
        }
    } else {
        rc_svg_warn(ps->icon, "unsupported SVG element skipped");
    }

    /* Recurse into children (containers and, harmlessly, any element that has
       nested content). Self-closing tags have no children. A definition
       container's subtree is still walked, so the parse index passes it, and
       what it collected is handed back: the shape and point budgets, and the
       sticky overflow latch, which would otherwise mark the icon overflowed for
       geometry that is not drawn. Its warnings stay, as in rcLoadSvg. */
    if (!e->selfClose) {
        const bool isDefinition  = rc_svg_tag_is_definition(e->tag);
        const int  savedShapes   = ps->shapeCount;
        const int  savedPts      = ps->ptCount;
        const bool savedOverflow = ps->overflow;

        rc_svg_collect(ps, s, len, i, e, strokeWidth, fill, stroke, depth + 1);

        if (isDefinition) {
            ps->shapeCount = savedShapes;
            ps->ptCount    = savedPts;
            ps->overflow   = savedOverflow;
        }
    }
    ps->evenodd = inhEvenodd;
    ps->joinMiters = inhJoin;
    ps->limitMiters = inhLimit;
    ps->ctm = inhCtm;
    ps->transformed = inhTransformed;
}

static void rc_svg_collect(RcSvgParse *ps, const char *s, int len, int *i,
                           const RcSvgElem *parent, float inhStrokeW,
                           RcSvgPaint inhFill, RcSvgPaint inhStroke, int depth) {
    (void)parent;
    if (depth > RC_SVG_MAX_DEPTH) {
        rc_svg_warn(ps->icon, rc_svg_deep_msg);
        return;
    }
    while (*i < len) {
        /* Advance to the next '<'. */
        while (*i < len && s[*i] != '<') {
            (*i)++;
        }
        if (*i >= len) {
            return;
        }
        /* Comment / CDATA / processing-instruction / DOCTYPE: skip to '>'. */
        if (*i + 1 < len && (s[*i + 1] == '!' || s[*i + 1] == '?')) {
            if (*i + 3 < len && strncmp(s + *i, "<!--", 4) == 0) {
                int end = rc_svg_comment_end(s, len, *i);
                if (end < 0) {
                    *i = len;
                    return;
                }
                *i = end;
            } else {
                while (*i < len && s[*i] != '>') {
                    (*i)++;
                }
                if (*i < len) {
                    (*i)++;
                }
            }
            continue;
        }
        (*i)++;   /* skip '<' */
        RcSvgElem e;
        if (!rc_svg_scan_element(s, len, i, &e)) {
            /* Unterminated tag: bail out of this subtree gracefully. */
            *i = len;
            return;
        }
        if (e.isClose) {
            return;   /* matching close of our parent (or a stray close) */
        }
        rc_svg_handle_element(ps, s, len, i, &e, inhStrokeW, inhFill, inhStroke, depth);
    }
}

/* Op building. */

/* 64-sample circle/ellipse ring used for non-circular ellipses, appended to
   icon->points. Returns the point offset. */
static int rc_svg_emit_circle_ring(RcSvgIcon *out, float cx, float cy,
                                   float rx, float ry, int count) {
    int off = out->pointCount;
    for (int k = 0; k < count; k++) {
        if (out->pointCount >= RC_SVG_MAX_POINTS) {
            rc_svg_warn(out, "icon point budget exhausted; geometry truncated");
            break;
        }
        float ang = 2.0f * (float)M_PI * (float)k / (float)count;
        out->points[out->pointCount].x = cx + cosf(ang) * rx;
        out->points[out->pointCount].y = cy + sinf(ang) * ry;
        out->pointCount++;
    }
    return off;
}

/* Copy a shape's flattened points (from the parse pool) into icon->points.
   Returns the destination offset; sets *countOut to how many were copied. */
static int rc_svg_copy_points(RcSvgIcon *out, const RcSvgParse *ps,
                              int srcOff, int srcCount, int *countOut) {
    int off = out->pointCount;
    int n = 0;
    for (int k = 0; k < srcCount; k++) {
        if (out->pointCount >= RC_SVG_MAX_POINTS) {
            rc_svg_warn(out, "icon point budget exhausted; geometry truncated");
            break;
        }
        out->points[out->pointCount].x = ps->pts[srcOff + k].x;
        out->points[out->pointCount].y = ps->pts[srcOff + k].y;
        out->pointCount++;
        n++;
    }
    *countOut = n;
    return off;
}

static bool rc_svg_push_op(RcSvgIcon *out, RcSvgOp op) {
    if (out->opCount >= RC_SVG_MAX_OPS) {
        rc_svg_warn(out, "op budget exhausted; remaining ops skipped");
        return false;
    }
    /* Every emitted path passes through here, and only point-based ops (POLYLINE /
       FILLED_POLY / FILLED_QUADS) carry a non-zero pointCount - so this one check
       catches a FILL OUTLINE the runtime cuts on every draw, on every code path, before
       it is baked into a header (a cut fill's quads are four points each and have no
       cap). It draws whole; the note is its cost, a cut a frame where the ear clip's
       list is cut once and replayed. Only a fill: the runtime strokes a POLYLINE of any
       length (in windows, rcIconDrawPolyline) at no such cost. Note (do not split):
       splitting a fill would seam its triangulation; fewer curve steps, or a simpler
       SVG, is the fix. */
    if (op.kind == RC_SVG_FILLED_POLY && op.pointCount > RC_SVG_RUNTIME_MAX_PTS) {
        rc_svg_warn(out, "a filled path has more points than RayClay's ear clip takes; it "
                          "draws whole, cut on every draw: lower the curve steps or simplify "
                          "the SVG");
    }
    out->ops[out->opCount++] = op;
    return true;
}

static void rc_svg_rect_corners(RcSvgIcon *out, const RcSvgShape *sh, int *offOut, int *countOut) {
    int off = out->pointCount;
    struct { float x, y; } corners[4] = {
        { sh->x, sh->y },
        { sh->x + sh->w, sh->y },
        { sh->x + sh->w, sh->y + sh->h },
        { sh->x, sh->y + sh->h },
    };
    int n = 0;
    for (int k = 0; k < 4; k++) {
        if (out->pointCount >= RC_SVG_MAX_POINTS) {
            rc_svg_warn(out, "icon point budget exhausted; geometry truncated");
            break;
        }
        out->points[out->pointCount].x = corners[k].x;
        out->points[out->pointCount].y = corners[k].y;
        out->pointCount++;
        n++;
    }
    *offOut = off;
    *countOut = n;
}

/* ---- A fill its outlines cannot make one at a time: holes, overlaps,
   crossings. A copy of the cut rcLoadSvg makes, kept
   in step by hand until this converter parses with the library.

   RayClay fills ONE simple outline (rcIconDrawFilledPolygon); SVG fills a path
   by its fill-rule over ALL its subpaths at once: an inner subpath cuts a hole
   (nonzero: wound the other way; evenodd: always), a subpath inside the hole
   fills again, and an outline that crosses itself fills by the same count. So
   an element fills one of two ways: each subpath as its own outline, when that
   is the same picture under either rule (none past the runtime's point cap, no
   two touching, crossing or one inside the other: nearly every icon), else cut
   into quads between level lines, four points each, by the winding number
   (FILLED_QUADS). Both are counted in steps (RC_SVG_MAX_WORK); an element they
   cannot finish fills subpath by subpath, and says so. ---- */
typedef struct {
    float x0, y0, x1, y1;   /* its upper end, then its lower (y grows downward) */
    float xa, xb;           /* where it meets the top and bottom of the band being cut */
    int   dir;              /* the winding it adds: +1 drawn downward, -1 upward, 0 level */
    int   sub;              /* the subpath (shape) it belongs to */
    int   quadRight;        /* the quad it last opened: the edge on its right, */
    int   quadBand;         /* the band that last wrote it, */
    int   quadOff;          /* and its first point in the icon's pool */
} RcSvgEdge;

/* One subpath's edges in rc_svg_fill_edges' list, and the box they lie in. */
typedef struct {
    int   end;              /* one past its last edge; its first is the span before's end */
    float x0, y0, x1, y1;   /* the box; x0 > x1 when the subpath has no edge */
} RcSvgSpan;

/* What a parse's fills may cost, in steps (an edge placed on a band's line, a
   place the band's sort moves an edge, a pair of edges compared), one count a
   document: a star of thousands of crossings must not hang the window that
   dropped it. rcLoadSvg keeps the same bound. */
#define RC_SVG_MAX_WORK (1 << 20)

static const char *const rc_svg_point_budget_msg =
    "icon point budget exhausted; geometry truncated";
static const char *const rc_svg_fill_work_msg =
    "a filled path is too complex to cut by its fill-rule; its subpaths fill one by one";

/* Which side of the line a -> b the point c lies on: 1, -1, or 0 on the line.
   Two products compared, each of two differences taken in double, so three
   points on a line read as on it. */
static int rc_svg_side(float ax, float ay, float bx, float by, float cx, float cy) {
    const double l = ((double)bx - ax) * ((double)cy - ay);
    const double r = ((double)by - ay) * ((double)cx - ax);
    return (l > r) - (l < r);
}

/* Do two edges share a point: cross, touch, or lie along one another? */
static bool rc_svg_edges_meet(const RcSvgEdge *e, const RcSvgEdge *f) {
    const int a = rc_svg_side(e->x0, e->y0, e->x1, e->y1, f->x0, f->y0);
    const int b = rc_svg_side(e->x0, e->y0, e->x1, e->y1, f->x1, f->y1);
    const int c = rc_svg_side(f->x0, f->y0, f->x1, f->y1, e->x0, e->y0);
    const int d = rc_svg_side(f->x0, f->y0, f->x1, f->y1, e->x1, e->y1);

    if (a | b | c | d) {
        return a != b && c != d;
    }
    /* All four ends on one line: they meet when their extents overlap. */
    return fminf(e->x0, e->x1) <= fmaxf(f->x0, f->x1) &&
           fminf(f->x0, f->x1) <= fmaxf(e->x0, e->x1) && e->y0 <= f->y1 && f->y0 <= e->y1;
}

/* The edges of the subpaths [first, end) that can be filled (three points or
   more), each closed back to its first point, upper end first. A repeated
   point makes no edge. Returns the count, or -1 when a coordinate is not
   finite (such a fill is left to its subpaths). */
static int rc_svg_fill_edges(const RcSvgParse *ps, int first, int end, RcSvgEdge *edge) {
    int n = 0;

    for (int s = first; s < end; s++) {
        const RcSvgShape *sh = &ps->shapes[s];
        for (int k = 0; sh->ptCount >= 3 && k < sh->ptCount; k++) {
            const RcSvgPoint a = ps->pts[sh->ptOff + k];
            const RcSvgPoint b = ps->pts[sh->ptOff + (k + 1) % sh->ptCount];
            const bool       up = b.y < a.y;
            RcSvgEdge       *e = &edge[n];

            if (!isfinite(a.x) || !isfinite(a.y)) {
                return -1;
            }
            if (a.x == b.x && a.y == b.y) {
                continue;
            }
            memset(e, 0, sizeof(*e));
            e->x0 = up ? b.x : a.x; e->y0 = up ? b.y : a.y;
            e->x1 = up ? a.x : b.x; e->y1 = up ? a.y : b.y;
            e->dir = up ? -1 : b.y > a.y;
            e->sub = s;
            n++;
        }
    }
    return n;
}

/* Each subpath's edges in the list, and the box they lie in; `first` is the
   element's first subpath (the lowest edge->sub), `count` how many. */
static void rc_svg_fill_spans(const RcSvgEdge *edge, int n, int first,
                              RcSvgSpan *span, int count) {
    int i = 0;

    for (int s = 0; s < count; s++) {
        RcSvgSpan *sp = &span[s];

        sp->x0 = sp->y0 = FLT_MAX;
        sp->x1 = sp->y1 = -FLT_MAX;
        for (; i < n && edge[i].sub == first + s; i++) {
            sp->x0 = fminf(sp->x0, fminf(edge[i].x0, edge[i].x1));
            sp->x1 = fmaxf(sp->x1, fmaxf(edge[i].x0, edge[i].x1));
            sp->y0 = fminf(sp->y0, edge[i].y0);
            sp->y1 = fmaxf(sp->y1, edge[i].y1);
        }
        sp->end = i;
    }
}

/* May every subpath fill on its own, as one simple outline each? Yes when none
   is past the runtime's point cap, none stands inside another, and no two
   edges share a point but an outline's neighbors at their common vertex (a
   touch counts: an outline that returns to its own point may have walked round
   a hole). Outlines are compared only where their boxes overlap. Each pair
   looked at is one step of `work`; with none left the answer is no. */
static bool rc_svg_fills_alone(const RcSvgParse *ps, int first, int end,
                               const RcSvgEdge *edge, const RcSvgSpan *span, int *work) {
    const int count = end - first;

    for (int s = 0; s < count; s++) {
        const RcSvgShape *sh = &ps->shapes[first + s];
        RcSvgPoint        q;
        int               wn = 0;

        if (sh->ptCount < 3) {
            continue;
        }
        if (sh->ptCount > RC_SVG_RUNTIME_MAX_PTS) {
            return false;
        }
        /* The other subpaths' winding round this one's first point, which
           stands for all of it while no two outlines touch (tested next). */
        q = ps->pts[sh->ptOff];
        for (int o = 0; o < count; o++) {
            const int from = o ? span[o - 1].end : 0;

            if (o == s || q.x < span[o].x0 || q.x > span[o].x1 ||
                q.y < span[o].y0 || q.y > span[o].y1) {
                continue;
            }
            *work += span[o].end - from;
            for (int i = from; i < span[o].end; i++) {
                const RcSvgEdge *e = &edge[i];
                if (e->y0 <= q.y && q.y < e->y1 &&
                    rc_svg_side(e->x0, e->y0, e->x1, e->y1, q.x, q.y) > 0) {
                    wn += e->dir;
                }
            }
        }
        if (wn != 0 || *work > RC_SVG_MAX_WORK) {
            return false;
        }
    }
    for (int s = 0, a = 0; s < count; a = span[s++].end) {
        const RcSvgSpan *sp = &span[s];
        const int        b = sp->end;   /* this outline's edges are [a, b) */

        /* Its own edges with one another. */
        for (int i = a; i < b && *work <= RC_SVG_MAX_WORK; i++) {
            const RcSvgEdge *e = &edge[i];

            *work += b - i;
            for (int j = i + 2; j < b; j++) {
                const RcSvgEdge *f = &edge[j];
                if (e->y1 >= f->y0 && f->y1 >= e->y0 && !(i == a && j == b - 1) &&
                    rc_svg_edges_meet(e, f)) {
                    return false;
                }
            }
        }
        /* Its edges with those of each later outline whose box they reach. */
        for (int o = s + 1; o < count && *work <= RC_SVG_MAX_WORK; o++) {
            const RcSvgSpan *op = &span[o];

            if (sp->x1 < op->x0 || op->x1 < sp->x0 || sp->y1 < op->y0 || op->y1 < sp->y0) {
                continue;
            }
            *work += b - a;
            for (int i = a; i < b && *work <= RC_SVG_MAX_WORK; i++) {
                const RcSvgEdge *e = &edge[i];

                if (e->y1 < op->y0 || op->y1 < e->y0 || fmaxf(e->x0, e->x1) < op->x0 ||
                    op->x1 < fminf(e->x0, e->x1)) {
                    continue;
                }
                *work += op->end - span[o - 1].end;
                for (int j = span[o - 1].end; j < op->end; j++) {
                    const RcSvgEdge *f = &edge[j];
                    if (e->y1 >= f->y0 && f->y1 >= e->y0 && rc_svg_edges_meet(e, f)) {
                        return false;
                    }
                }
            }
        }
    }
    return *work <= RC_SVG_MAX_WORK;
}

/* An edge's x on the level line y, which lies within its two ends: an end's
   own x at that end, so edges that share a vertex agree there exactly. */
static float rc_svg_edge_x(const RcSvgEdge *e, float y) {
    if (y <= e->y0) {
        return e->x0;
    }
    if (y >= e->y1) {
        return e->x1;
    }
    return (float)(e->x0 + ((double)e->x1 - e->x0) *
                           (((double)y - e->y0) / ((double)e->y1 - e->y0)));
}

/* qsort's order for the cut: by upper end, top to bottom, then left to right,
   and the rest of the edge after, so the order is TOTAL and every C library's
   qsort leaves the same list. */
static int rc_svg_edge_by_top(const void *a, const void *b) {
    const RcSvgEdge *e = (const RcSvgEdge *)a, *f = (const RcSvgEdge *)b;

    if (e->y0 != f->y0) {
        return e->y0 < f->y0 ? -1 : 1;
    }
    if (e->x0 != f->x0) {
        return e->x0 < f->x0 ? -1 : 1;
    }
    if (e->x1 != f->x1) {
        return e->x1 < f->x1 ? -1 : 1;
    }
    if (e->y1 != f->y1) {
        return e->y1 < f->y1 ? -1 : 1;
    }
    return e->sub != f->sub ? e->sub - f->sub : e->dir - f->dir;
}

/* Where the band [ya, yb] must end for no two of its edges to cross inside it:
   the highest level strictly inside at which two neighbors in the list cross,
   else yb. */
static float rc_svg_band_cut(const RcSvgEdge *edge, const int *act, int na,
                             float ya, float yb) {
    float cut = yb;

    for (int i = 0; i + 1 < na; i++) {
        const RcSvgEdge *p = &edge[act[i]], *q = &edge[act[i + 1]];
        const double     top = (double)q->xa - p->xa;
        const double     bottom = (double)q->xb - p->xb;

        if (top < 0.0 || bottom < 0.0) {
            /* Out of order on both lines, or parallel: the level falls outside
               the band, or is not a number, and the test below refuses it. */
            const float yc = (float)(ya + ((double)yb - ya) * (top / (top - bottom)));
            if (yc > ya && yc < cut) {
                cut = yc;
            }
        }
    }
    return cut;
}

/* Put the band's edges in their order inside it, by where each stands at the
   band's middle (xa + xb): an insertion sort, each place an edge moves
   counted; past `room` it stops. Returns the moves made. */
static int rc_svg_band_sort(const RcSvgEdge *edge, int *act, int na, int room) {
    int moved = 0;

    for (int i = 1; i < na && moved <= room; i++) {
        const int        k = act[i];
        const RcSvgEdge *e = &edge[k];
        const double     mid = (double)e->xa + e->xb;
        int              j;

        for (j = i; j > 0; j--) {
            const RcSvgEdge *p = &edge[act[j - 1]];
            const double     pmid = (double)p->xa + p->xb;
            if (pmid < mid || (pmid == mid && p->xa <= e->xa)) {
                break;
            }
            act[j] = act[j - 1];
        }
        act[j] = k;
        moved += i - j;
    }
    return moved;
}

/* Cut the fill of `n` edges into quads appended to the icon's points, four a
   quad: upper left, upper right, lower right, lower left. Level lines through
   every vertex (and every crossing) slice the plane into bands; inside one the
   edges stand in one left-to-right order, and walking them adding each one's
   direction gives every gap's winding number: a gap is filled where it is odd
   (evenodd) or not zero (nonzero). Edges at one place count together, so no
   quad of no width is written. A quad grows downward while the same two edges
   bound it, so quads meet only along level lines: no seam and no double cover.
   Charged against `spent` (RC_SVG_MAX_WORK); returns false when the steps ran
   out before the cut was whole. */
static bool rc_svg_fill_quads(RcSvgIcon *out, RcSvgEdge *edge, int n, bool evenodd,
                              int *act, int *spent) {
    int   na = 0, next = 0, band = 1, work = *spent;
    float ya = 0.0f;

    qsort(edge, (size_t)n, sizeof(*edge), rc_svg_edge_by_top);
    for (;;) {
        float yb, cut;
        int   i, j, w = 0, left = 0;
        bool  in = false;

        /* The edges that cross the band under ya: those still running, and
           those that start on it. With none running, ya skips to the next. */
        for (i = j = 0; i < na; i++) {
            RcSvgEdge *e = &edge[act[i]];
            if (e->y1 > ya) {
                e->xa = e->xb;   /* the last band's bottom line is this one's top */
                act[j++] = act[i];
            }
        }
        na = j;
        if (na == 0) {
            if (next == n) {
                break;
            }
            ya = edge[next].y0;
        }
        /* Merge the edges that start on ya (in order of x) into the list,
           which is in its order on ya, from the back. */
        for (j = next; j < n && edge[j].y0 <= ya; j++) {
            edge[j].xa = edge[j].x0;
        }
        for (int k = j, d = na + j - next - 1, r = na - 1; k > next; d--) {
            if (r >= 0 && edge[act[r]].xa > edge[k - 1].xa) {
                act[d] = act[r--];
            } else {
                act[d] = --k;
            }
        }
        na += j - next;
        next = j;
        /* The band's bottom: the nearest end or start below ya, or sooner,
           the first level at which two of its edges cross. */
        yb = edge[act[0]].y1;
        for (i = 1; i < na; i++) {
            yb = fminf(yb, edge[act[i]].y1);
        }
        if (next < n) {
            yb = fminf(yb, edge[next].y0);
        }
        for (;;) {
            int moved;

            work += na;
            if (work > RC_SVG_MAX_WORK) {
                *spent = work;
                return false;
            }
            for (i = 0; i < na; i++) {
                RcSvgEdge *e = &edge[act[i]];
                e->xb = rc_svg_edge_x(e, yb);
            }
            cut = rc_svg_band_cut(edge, act, na, ya, yb);
            if (cut == yb) {
                moved = rc_svg_band_sort(edge, act, na, RC_SVG_MAX_WORK - work);
                work += moved;
                if (work > RC_SVG_MAX_WORK) {
                    *spent = work;
                    return false;
                }
                if (moved == 0 || (cut = rc_svg_band_cut(edge, act, na, ya, yb)) == yb) {
                    break;
                }
            }
            yb = cut;
        }
        band++;
        for (i = 0; i < na; i = j) {
            const RcSvgEdge *e = &edge[act[i]];
            RcSvgEdge       *l;
            RC_IconPoint    *q;

            for (j = i; j < na && edge[act[j]].xa == e->xa && edge[act[j]].xb == e->xb; j++) {
                w += edge[act[j]].dir;
            }
            if (((evenodd ? w & 1 : w) != 0) == in) {
                continue;
            }
            in = !in;
            if (in) {
                left = act[i];
                continue;
            }
            l = &edge[left];
            if (l->quadBand != band - 1 || l->quadRight != act[i]) {
                if (out->pointCount + 4 > RC_SVG_MAX_POINTS) {
                    rc_svg_warn(out, rc_svg_point_budget_msg);
                    *spent = work;
                    return true;
                }
                l->quadRight = act[i];
                l->quadOff   = out->pointCount;
                out->pointCount += 4;
                q = &out->points[l->quadOff];
                q[0].x = l->xa; q[0].y = ya;
                q[1].x = fmaxf(l->xa, e->xa); q[1].y = ya;
            }
            /* fmaxf: two edges that cross on the band's line can read a float
               out of order there, and the quad's side is then one point. */
            l->quadBand = band;
            q = &out->points[l->quadOff];
            q[2].x = fmaxf(l->xb, e->xb); q[2].y = yb;
            q[3].x = l->xb; q[3].y = yb;
        }
        ya = yb;
    }
    *spent = work;
    return true;
}

/* Settle how the element whose subpaths are the shapes [first, end) fills:
   false when each subpath fills on its own (the caller's FILLED_POLY ops),
   true when its whole fill was appended to the icon's points as quads (none
   when it has no area). An element the steps cannot finish fills subpath by
   subpath, and warns. */
static bool rc_svg_fill_cut(RcSvgIcon *out, const RcSvgParse *ps, int first, int end,
                            int *work) {
    const int  mark = out->pointCount;
    RcSvgEdge *edge;
    RcSvgSpan *span;
    int        cap = 0, n;
    bool       cut;

    for (int s = first; s < end; s++) {
        cap += ps->shapes[s].ptCount;
    }
    /* The edges, one int an edge for the cut's working list, a span a subpath. */
    edge = (RcSvgEdge *)malloc((size_t)cap * (sizeof(*edge) + sizeof(int)) +
                               (size_t)(end - first) * sizeof(*span));
    if (!edge) {
        rc_svg_warn(out, "out of memory reading a filled path's outlines; each fills on its own");
        return false;
    }
    span = (RcSvgSpan *)(void *)((int *)(void *)(edge + cap) + cap);
    n = rc_svg_fill_edges(ps, first, end, edge);
    rc_svg_fill_spans(edge, n, first, span, end - first);
    cut = n > 0 && !rc_svg_fills_alone(ps, first, end, edge, span, work);
    if (cut) {
        int kept = 0;
        for (int i = 0; i < n; i++) {   /* a level edge crosses no band */
            if (edge[i].dir != 0) {
                edge[kept++] = edge[i];
            }
        }
        if (!rc_svg_fill_quads(out, edge, kept, ps->shapes[first].evenodd,
                                (int *)(void *)(edge + cap), work)) {
            rc_svg_warn(out, rc_svg_fill_work_msg);
            out->pointCount = mark;
            cut = false;
        }
    }
    free(edge);
    return cut;
}

/* Push a <path>'s or <polygon>'s whole fill at its first subpath `si`, all of
   it before any subpath is stroked (SVG's paint order, as rcLoadSvg): cut by its
   fill-rule (one FILLED_QUADS op), or a FILLED_POLY a subpath of three points
   or more. Returns one past the element's last subpath. */
static int rc_svg_push_path_fill(RcSvgIcon *out, const RcSvgParse *ps, int si,
                                 bool baked, RC_Color color, int *work) {
    const int elem = ps->shapes[si].elem;
    int       end;
    RcSvgOp   op;

    for (end = si + 1; end < ps->shapeCount && ps->shapes[end].kind == RC_SHAPE_PATH &&
                       ps->shapes[end].elem == elem; end++) {
    }
    memset(&op, 0, sizeof(op));
    op.baked = baked;
    op.color = color;
    op.pointOff = out->pointCount;
    if (rc_svg_fill_cut(out, ps, si, end, work)) {
        op.kind = RC_SVG_FILLED_QUADS;
        op.pointCount = out->pointCount - op.pointOff;
        if (op.pointCount > 0) {
            rc_svg_push_op(out, op);
        }
        return end;
    }
    op.kind = RC_SVG_FILLED_POLY;
    for (int s = si; s < end; s++) {
        if (ps->shapes[s].ptCount >= 3) {
            op.pointOff = rc_svg_copy_points(out, ps, ps->shapes[s].ptOff,
                                              ps->shapes[s].ptCount, &op.pointCount);
            rc_svg_push_op(out, op);
        }
    }
    return end;
}

/* A FILLED ROUNDED RECT IS ITS OWN OUTLINE, SAMPLED, as rcLoadSvg
   samples it: four quarter arcs clockwise from the
   top edge, 90 / arcDeg steps a corner, each radius clamped to half its side.
   A straight edge the clamp shortens to nothing is no edge (a duplicate vertex
   would fail the fill's convex fan), and the step is capped so the outline
   stays under the runtime's per-fill point cap. */
static void rc_svg_rounded_rect_points(RcSvgIcon *out, const RcSvgShape *sh, float arcDeg,
                                       int *offOut, int *countOut) {
    float rx = sh->rx < sh->w * 0.5f ? sh->rx : sh->w * 0.5f;
    float ry = sh->ry < sh->h * 0.5f ? sh->ry : sh->h * 0.5f;
    /* Within 1/10,000 of its side, an edge is the clamp's zero, not art. */
    const bool noTopBottom = sh->w - 2.0f * rx <= sh->w * 1e-4f;
    const bool noSides     = sh->h - 2.0f * ry <= sh->h * 1e-4f;
    if (noTopBottom) {
        rx = sh->w * 0.5f;
    }
    if (noSides) {
        ry = sh->h * 0.5f;
    }

    const float maxSeg = arcDeg > 1.0f ? arcDeg : 1.0f;
    int steps = (int)ceilf(90.0f / maxSeg);
    if (steps < 4) {
        steps = 4;
    }
    if (steps > RC_SVG_RUNTIME_MAX_PTS / 4 - 1) {
        steps = RC_SVG_RUNTIME_MAX_PTS / 4 - 1;
    }

    /* Each corner: its center, the direction its arc leaves (u) and the one it
       arrives along (v), and whether the edge BEFORE it has no length. */
    const struct { float cx, cy, ux, uy, vx, vy; bool joined; } corner[4] = {
        { sh->x + sh->w - rx, sh->y + ry,          0.0f, -1.0f,  1.0f,  0.0f, noTopBottom },
        { sh->x + sh->w - rx, sh->y + sh->h - ry,  1.0f,  0.0f,  0.0f,  1.0f, noSides     },
        { sh->x + rx,         sh->y + sh->h - ry,  0.0f,  1.0f, -1.0f,  0.0f, noTopBottom },
        { sh->x + rx,         sh->y + ry,         -1.0f,  0.0f,  0.0f, -1.0f, noSides     },
    };
    const int off = out->pointCount;
    int n = 0;
    for (int c = 0; c < 4; c++) {
        for (int k = corner[c].joined ? 1 : 0; k <= steps; k++) {
            if (out->pointCount >= RC_SVG_MAX_POINTS) {
                rc_svg_warn(out, rc_svg_point_budget_msg);
                *offOut = off;
                *countOut = n;
                return;
            }
            /* The two ends are exact, so the straight edges stay straight. */
            const float t  = (float)k / (float)steps * 0.5f * (float)M_PI;
            const float ct = k == 0 ? 1.0f : k == steps ? 0.0f : cosf(t);
            const float st = k == 0 ? 0.0f : k == steps ? 1.0f : sinf(t);
            out->points[out->pointCount].x =
                corner[c].cx + rx * (corner[c].ux * ct + corner[c].vx * st);
            out->points[out->pointCount].y =
                corner[c].cy + ry * (corner[c].uy * ct + corner[c].vy * st);
            out->pointCount++;
            n++;
        }
    }
    *offOut = off;
    *countOut = n;
}

/* A rect's outline as its fill draws it: rounded when both radii are above 0,
   else its four corners. The stroke of a rect whose corners are not one circle
   takes it too, so stroke and fill part nowhere (as in rcLoadSvg). */
static void rc_svg_rect_outline(RcSvgIcon *out, const RcSvgShape *sh, float arcDeg,
                                int *offOut, int *countOut) {
    if (sh->rx > 0.0f && sh->ry > 0.0f) {
        rc_svg_rounded_rect_points(out, sh, arcDeg, offOut, countOut);
    } else {
        rc_svg_rect_corners(out, sh, offOut, countOut);
    }
}

/* Does a rect's stroke round its corners by one circle, so that
   rcIconDrawRoundedRectStroke draws it? Both radii above 0 and equal once each
   is clamped to half its side as the fill clamps them: rx="30" alone on a
   20 x 16 rect is 30 on both axes, yet its corners are 10 x 8, which the
   one-radius helper cannot follow (as in rcLoadSvg). */
static bool rc_svg_rrect_one_radius(const RcSvgShape *sh) {
    const float rx = sh->rx < sh->w * 0.5f ? sh->rx : sh->w * 0.5f;
    const float ry = sh->ry < sh->h * 0.5f ? sh->ry : sh->h * 0.5f;
    return sh->rx > 0.0f && sh->ry > 0.0f && fabsf(rx - ry) <= 1e-6f;
}

static bool rc_svg_paint_is_baked(RcSvgPaint p) {
    return p.kind == RC_PAINT_BAKED;
}

static bool rc_svg_paint_is_paint(RcSvgPaint p) {
    return p.kind == RC_PAINT_BAKED || p.kind == RC_PAINT_CURRENT;
}

/* A currentColor paint in a colored icon bakes as black. */
static RC_Color rc_svg_baked_color(RcSvgPaint p) {
    if (p.kind == RC_PAINT_BAKED) {
        return p.color;
    }
    return RC_LIT(RC_Color){ 0, 0, 0, 255 };   /* currentColor -> black */
}

/* True for the op kinds that actually draw an edge. The shared `stroke` constant
   is computed over these alone: a fill-only shape carries a strokeWidth no op
   ever reads, and counting it breaks the constant for a value nothing uses. */
static bool rc_svg_op_strokes(RcSvgOpKind k) {
    return k == RC_SVG_ROUND_LINE || k == RC_SVG_POLYLINE
        || k == RC_SVG_CIRCLE_STROKE || k == RC_SVG_RRECT_STROKE;
}

/* MONO op building, baked=false. Mirrors generate_header's plan.
   FILL AND STROKE ARE INDEPENDENT, and a shape carrying both emits both, fill
   first - painter's order, matching the colored builder. Reading only the
   stroke turns a solid disc into a ring of its own outline, which at icon sizes
   is a sub-pixel smear rather than a shape: a filled circle read as a 2.0-unit
   ring in a 731-unit viewBox is 0.06 device pixels wide at a 22 px
   titlebar.
   On this path a paint can only be the currentColor sentinel - a shape baking a
   concrete color routes to rc_svg_build_colored instead - so a fill draws with
   the caller's `color` and the (float size, RC_Color color) signature holds.
   A shape with NEITHER paint still strokes, which is the long-standing default
   for artwork that relies on a stylesheet this parser does not resolve. That
   shape is UNPAINTED or sits in a <style> document: a fill the author SET to
   none, with no stroke, never reaches here (rc_svg2icon_parse drops it). */
static void rc_svg_build_mono(RcSvgIcon *out, const RcSvgParse *ps) {
    int fillEnd = 0;    /* one past the last subpath whose element's fill is pushed */
    int fillWork = 0;   /* the parse's fill steps (RC_SVG_MAX_WORK) */

    for (int si = 0; si < ps->shapeCount; si++) {
        const RcSvgShape *sh = &ps->shapes[si];
        const bool fills   = sh->fill.kind   != RC_PAINT_NONE;
        const bool strokes = sh->stroke.kind != RC_PAINT_NONE || !fills;
        RcSvgOp op;

        if (sh->kind == RC_SHAPE_RECT) {
            /* Its outline as the runtime fills it: rounded, elliptical where rx
               and ry differ, or square. */
            if (fills) {
                int off, count;
                rc_svg_rect_outline(out, sh, ps->arcDeg, &off, &count);
                memset(&op, 0, sizeof(op));
                op.kind = RC_SVG_FILLED_POLY;
                op.pointOff = off; op.pointCount = count;
                rc_svg_push_op(out, op);
            }
            if (strokes) {
                memset(&op, 0, sizeof(op));
                /* The helper strokes corners of one circle, and miters a square
                   rect at a radius of 0; any other stroke follows the outline. */
                if (rc_svg_rrect_one_radius(sh) || sh->mitered) {
                    op.kind = RC_SVG_RRECT_STROKE;
                    op.p[0] = sh->x; op.p[1] = sh->y; op.p[2] = sh->w; op.p[3] = sh->h; op.p[4] = sh->rx;
                    op.stroke = sh->strokeWidth;
                    rc_svg_push_op(out, op);
                } else {
                    int off, count;
                    rc_svg_rect_outline(out, sh, ps->arcDeg, &off, &count);
                    op.kind = RC_SVG_POLYLINE;
                    op.pointOff = off; op.pointCount = count;
                    op.closed = true; op.stroke = sh->strokeWidth;
                    rc_svg_push_op(out, op);
                }
            }
        } else if (sh->kind == RC_SHAPE_CIRCLE) {
            if (fills) {
                memset(&op, 0, sizeof(op));
                op.kind = RC_SVG_FILLED_CIRCLE;
                op.p[0] = sh->cx; op.p[1] = sh->cy; op.p[2] = sh->r;
                rc_svg_push_op(out, op);
            }
            if (strokes) {
                memset(&op, 0, sizeof(op));
                op.kind = RC_SVG_CIRCLE_STROKE;
                op.p[0] = sh->cx; op.p[1] = sh->cy; op.p[2] = sh->r;
                op.stroke = sh->strokeWidth;
                rc_svg_push_op(out, op);
            }
        } else if (sh->kind == RC_SHAPE_ELLIPSE) {
            const bool roundEnough = fabsf(sh->rx - sh->ry) <= 1e-6f;
            if (fills) {
                memset(&op, 0, sizeof(op));
                if (roundEnough) {
                    op.kind = RC_SVG_FILLED_CIRCLE;
                    op.p[0] = sh->cx; op.p[1] = sh->cy; op.p[2] = sh->rx;
                } else {
                    op.kind = RC_SVG_FILLED_ELLIPSE;
                    op.p[0] = sh->cx; op.p[1] = sh->cy; op.p[2] = sh->rx; op.p[3] = sh->ry;
                }
                rc_svg_push_op(out, op);
            }
            if (strokes) {
                memset(&op, 0, sizeof(op));
                if (roundEnough) {
                    op.kind = RC_SVG_CIRCLE_STROKE;
                    op.p[0] = sh->cx; op.p[1] = sh->cy; op.p[2] = sh->rx;
                    op.stroke = sh->strokeWidth;
                    rc_svg_push_op(out, op);
                } else {
                    rc_svg_warn(out, "non-circular ellipse flattened to a polyline (no ellipse helper)");
                    int off = rc_svg_emit_circle_ring(out, sh->cx, sh->cy, sh->rx, sh->ry, 64);
                    op.kind = RC_SVG_POLYLINE;
                    op.pointOff = off; op.pointCount = out->pointCount - off;
                    op.closed = true; op.stroke = sh->strokeWidth;
                    rc_svg_push_op(out, op);
                }
            }
        } else { /* RC_SHAPE_PATH */
            /* The element's whole fill, by its fill-rule, at its first subpath. */
            if (fills && si >= fillEnd) {
                fillEnd = rc_svg_push_path_fill(out, ps, si, false, sh->fill.color, &fillWork);
            }
            if (strokes) {
                memset(&op, 0, sizeof(op));
                if (sh->ptCount == 2 && !sh->closed) {
                    op.kind = RC_SVG_ROUND_LINE;
                    op.p[0] = ps->pts[sh->ptOff].x; op.p[1] = ps->pts[sh->ptOff].y;
                    op.p[2] = ps->pts[sh->ptOff + 1].x; op.p[3] = ps->pts[sh->ptOff + 1].y;
                    op.stroke = sh->strokeWidth;
                    rc_svg_push_op(out, op);
                } else {
                    int count;
                    int off = rc_svg_copy_points(out, ps, sh->ptOff, sh->ptCount, &count);
                    op.kind = RC_SVG_POLYLINE;
                    op.pointOff = off; op.pointCount = count;
                    op.closed = sh->closed; op.stroke = sh->strokeWidth;
                    rc_svg_push_op(out, op);
                }
            }
        }
    }
}

/* COLORED op building: FILL first then STROKE per shape, baked=true. */
static void rc_svg_build_colored(RcSvgIcon *out, RcSvgParse *ps) {
    bool usesColor = false;
    int  fillEnd = 0;    /* one past the last subpath whose element's fill is pushed */
    int  fillWork = 0;   /* the parse's fill steps (RC_SVG_MAX_WORK) */

    for (int si = 0; si < ps->shapeCount; si++) {
        const RcSvgShape *sh = &ps->shapes[si];
        bool hasFill = rc_svg_paint_is_paint(sh->fill);
        bool hasStroke = rc_svg_paint_is_paint(sh->stroke);
        if (!hasFill && !hasStroke) {
            continue;
        }
        int shapeFirstOp = out->opCount;   /* first op this shape emits */

        /* ---- FILL (drawn first / underneath) ---- */
        if (hasFill) {
            RcSvgPaint fp = sh->fill;
            /* currentColor stays runtime (baked=false -> emits the `color` arg,
               draw() uses monoColor); its .color is the documented {0,0,0,255}
               fallback. An explicit color is baked. */
            bool fbaked = (fp.kind == RC_PAINT_BAKED);
            if (fp.kind == RC_PAINT_CURRENT) usesColor = true;
            RC_Color fc = rc_svg_baked_color(fp);
            if (sh->kind == RC_SHAPE_CIRCLE) {
                RcSvgOp op; memset(&op, 0, sizeof(op));
                op.kind = RC_SVG_FILLED_CIRCLE; op.baked = fbaked; op.color = fc;
                op.p[0] = sh->cx; op.p[1] = sh->cy; op.p[2] = sh->r;
                rc_svg_push_op(out, op);
            } else if (sh->kind == RC_SHAPE_ELLIPSE) {
                RcSvgOp op; memset(&op, 0, sizeof(op));
                op.baked = fbaked; op.color = fc;
                if (fabsf(sh->rx - sh->ry) <= 1e-6f) {
                    op.kind = RC_SVG_FILLED_CIRCLE;
                    op.p[0] = sh->cx; op.p[1] = sh->cy; op.p[2] = sh->rx;
                } else {
                    op.kind = RC_SVG_FILLED_ELLIPSE;
                    op.p[0] = sh->cx; op.p[1] = sh->cy; op.p[2] = sh->rx; op.p[3] = sh->ry;
                }
                rc_svg_push_op(out, op);
            } else if (sh->kind == RC_SHAPE_RECT) {
                /* Elliptical when rx != ry, as in a browser, and the stroke over
                   it (below) follows the same outline. */
                int off, count;
                rc_svg_rect_outline(out, sh, ps->arcDeg, &off, &count);
                RcSvgOp op; memset(&op, 0, sizeof(op));
                op.kind = RC_SVG_FILLED_POLY; op.baked = fbaked; op.color = fc;
                op.pointOff = off; op.pointCount = count;
                rc_svg_push_op(out, op);
            } else { /* PATH */
                /* A <path> is ONE shape however many subpaths it has: its whole
                   fill, by its fill-rule, goes down at the first, before any
                   subpath is stroked, or a subpath's fill would cover the stroke
                   of one drawn before it. */
                if (si >= fillEnd) {
                    fillEnd = rc_svg_push_path_fill(out, ps, si, fbaked, fc, &fillWork);
                }
                if (sh->ptCount < 3 && !hasStroke && !sh->initialFill) {
                    /* Not for SVG's initial fill: an unstroked <line> draws
                       nothing in a browser too, and the author asked for no fill. */
                    rc_svg_warn(out, "degenerate filled path (<3 points) skipped");
                }
            }
        }

        /* ---- STROKE (drawn over the fill) ---- */
        if (hasStroke && sh->strokeWidth > 0.0f) {
            RcSvgPaint spnt = sh->stroke;
            bool sbaked = (spnt.kind == RC_PAINT_BAKED);
            if (spnt.kind == RC_PAINT_CURRENT) usesColor = true;
            RC_Color sc = rc_svg_baked_color(spnt);
            if (sh->kind == RC_SHAPE_CIRCLE) {
                RcSvgOp op; memset(&op, 0, sizeof(op));
                op.kind = RC_SVG_CIRCLE_STROKE; op.baked = sbaked; op.color = sc;
                op.p[0] = sh->cx; op.p[1] = sh->cy; op.p[2] = sh->r;
                op.stroke = sh->strokeWidth;
                rc_svg_push_op(out, op);
            } else if (sh->kind == RC_SHAPE_ELLIPSE) {
                if (fabsf(sh->rx - sh->ry) <= 1e-6f) {
                    RcSvgOp op; memset(&op, 0, sizeof(op));
                    op.kind = RC_SVG_CIRCLE_STROKE; op.baked = sbaked; op.color = sc;
                    op.p[0] = sh->cx; op.p[1] = sh->cy; op.p[2] = sh->rx;
                    op.stroke = sh->strokeWidth;
                    rc_svg_push_op(out, op);
                } else {
                    int off = rc_svg_emit_circle_ring(out, sh->cx, sh->cy, sh->rx, sh->ry, 64);
                    RcSvgOp op; memset(&op, 0, sizeof(op));
                    op.kind = RC_SVG_POLYLINE; op.baked = sbaked; op.color = sc;
                    op.pointOff = off; op.pointCount = out->pointCount - off;
                    op.closed = true; op.stroke = sh->strokeWidth;
                    rc_svg_push_op(out, op);
                }
            } else if (sh->kind == RC_SHAPE_RECT) {
                /* rcIconDrawRoundedRectStroke miters a radius of 0. */
                if (rc_svg_rrect_one_radius(sh) || sh->mitered) {
                    RcSvgOp op; memset(&op, 0, sizeof(op));
                    op.kind = RC_SVG_RRECT_STROKE; op.baked = sbaked; op.color = sc;
                    op.p[0] = sh->x; op.p[1] = sh->y; op.p[2] = sh->w; op.p[3] = sh->h; op.p[4] = sh->rx;
                    op.stroke = sh->strokeWidth;
                    rc_svg_push_op(out, op);
                } else {
                    int off, count;
                    rc_svg_rect_outline(out, sh, ps->arcDeg, &off, &count);
                    RcSvgOp op; memset(&op, 0, sizeof(op));
                    op.kind = RC_SVG_POLYLINE; op.baked = sbaked; op.color = sc;
                    op.pointOff = off; op.pointCount = count;
                    op.closed = true; op.stroke = sh->strokeWidth;
                    rc_svg_push_op(out, op);
                }
            } else { /* PATH */
                if (sh->ptCount == 2 && !sh->closed) {
                    RcSvgOp op; memset(&op, 0, sizeof(op));
                    op.kind = RC_SVG_ROUND_LINE; op.baked = sbaked; op.color = sc;
                    op.p[0] = ps->pts[sh->ptOff].x; op.p[1] = ps->pts[sh->ptOff].y;
                    op.p[2] = ps->pts[sh->ptOff + 1].x; op.p[3] = ps->pts[sh->ptOff + 1].y;
                    op.stroke = sh->strokeWidth;
                    rc_svg_push_op(out, op);
                } else {
                    int count;
                    int off = rc_svg_copy_points(out, ps, sh->ptOff, sh->ptCount, &count);
                    RcSvgOp op; memset(&op, 0, sizeof(op));
                    op.kind = RC_SVG_POLYLINE; op.baked = sbaked; op.color = sc;
                    op.pointOff = off; op.pointCount = count;
                    op.closed = sh->closed; op.stroke = sh->strokeWidth;
                    rc_svg_push_op(out, op);
                }
            }
        }

        /* The emitter puts a blank line before each source shape after the first,
           and a shape's fill and stroke ops stay together. The mark lives in the
           op, so an icon emitted after another file parsed keeps its own groups. */
        if (out->opCount > shapeFirstOp) {
            out->ops[shapeFirstOp].shapeStart = true;
        }
    }

    if (usesColor) {
        rc_svg_warn(out, "currentColor in a multi-color icon renders as black (the color argument is dropped); use an explicit color");
    }
}

/* Public: parse. `svg` is read as exactly `len` bytes and does NOT need a NUL
   terminator - every scan in this file is length-bounded, so an mmap'd slice or
   a socket buffer is a legitimate input. */

/* The outer <svg>'s own transform, as Chrome applies it: a CSS transform of
   its box about the box's center, in the box's px. The box is the root's width
   and height, or the viewBox's size where they are not given; the viewBox maps
   into it as xMidYMid meet, as rcLoadSvg maps it. */
static void rc_svg_root_transform(RcSvgParse *ps, const RcSvgElem *root,
                                  float vx, float vy, float vw, float vh) {
    int         len = 0;
    const char *v = rc_svg_attr(root, "transform", &len);
    RcSvgMatrix t;

    if (!v) {
        return;
    }
    if (!rc_svg_parse_transform(v, len, &t)) {
        rc_svg_warn(ps->icon, rc_svg_transform_bad_msg);
        return;
    }
    const float bw = rc_svg_attr_length(ps, root, "width", vw, RC_SVG_REF_W);
    const float bh = rc_svg_attr_length(ps, root, "height", vh, RC_SVG_REF_H);
    const float k = fminf(bw / vw, bh / vh);
    if (!(k > 0.0f) || !isfinite(k)) {
        return;
    }
    const RcSvgMatrix toBox = { k, 0.0f, 0.0f, k, 0.5f * (bw - k * vw) - k * vx,
                                0.5f * (bh - k * vh) - k * vy };
    const RcSvgMatrix fromBox = { 1.0f / k, 0.0f, 0.0f, 1.0f / k, -toBox.e / k, -toBox.f / k };
    const RcSvgMatrix toCenter = { 1.0f, 0.0f, 0.0f, 1.0f, -0.5f * bw, -0.5f * bh };
    const RcSvgMatrix fromCenter = { 1.0f, 0.0f, 0.0f, 1.0f, 0.5f * bw, 0.5f * bh };

    t = rc_svg_matrix_mul(fromCenter, rc_svg_matrix_mul(t, toCenter));
    ps->ctm = rc_svg_matrix_mul(fromBox, rc_svg_matrix_mul(t, toBox));
    ps->transformed = memcmp(&ps->ctm, &rc_svg_identity, sizeof(ps->ctm)) != 0;
}

static inline bool rc_svg2icon_parse(const char *svg, int len, int curveSteps, float arcDeg, RcSvgIcon *out) {
    if (!out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->ok = false;

    if (!svg || len <= 0) {
        snprintf(out->err, sizeof(out->err), "empty SVG input");
        return false;
    }
    if (curveSteps < 2) {
        curveSteps = 2;
    }
    if (arcDeg <= 0.0f) {
        arcDeg = 7.5f;
    }

    /* Parse state is large, so it lives in one static instance rather than on
       the stack (single-TU, single-threaded tool - not reentrant). */
    static RcSvgParse ps;
    memset(&ps, 0, sizeof(ps));
    ps.curveSteps = curveSteps;
    ps.arcDeg = arcDeg;
    ps.icon = out;

    /* Find the root element to read its viewBox + root paint defaults. */
    int i = 0;
    RcSvgElem root;
    bool haveRoot = false;
    while (i < len) {
        while (i < len && svg[i] != '<') {
            i++;
        }
        if (i >= len) {
            break;
        }
        if (i + 1 < len && (svg[i + 1] == '!' || svg[i + 1] == '?')) {
            if (i + 3 < len && strncmp(svg + i, "<!--", 4) == 0) {
                int end = rc_svg_comment_end(svg, len, i);
                if (end < 0) { i = len; break; }
                i = end;
            } else {
                while (i < len && svg[i] != '>') i++;
                if (i < len) i++;
            }
            continue;
        }
        i++;   /* skip '<' */
        if (!rc_svg_scan_element(svg, len, &i, &root)) {
            snprintf(out->err, sizeof(out->err), "malformed root element");
            return false;
        }
        if (!root.isClose) {
            haveRoot = true;
            break;
        }
    }
    if (!haveRoot) {
        snprintf(out->err, sizeof(out->err), "no root SVG element found");
        return false;
    }

    float vx, vy, vw, vh;
    rc_svg_parse_viewbox(&root, &vx, &vy, &vw, &vh);
    out->viewW = vw;
    out->viewH = vh;

    /* A stroke-width set nowhere in the cascade is SVG's initial 1, as a browser
       and rcLoadSvg draw it. */
    float rootStrokeW = rc_svg_stroke_width(&root, 1.0f, out);
    /* The stroke joins the root hands down (a drawable root reads its own
       again): SVG's initial miter and limit 4 where it states none. */
    ps.joinMiters = rc_svg_join_miters(&root, true);
    ps.limitMiters = rc_svg_limit_miters(&root, true);
    RcSvgPaint none = { RC_PAINT_NONE, { 0, 0, 0, 0 }, false, false };
    RcSvgPaint rootFill = rc_svg_read_paint(&root, "fill", none, out);
    RcSvgPaint rootStroke = rc_svg_read_paint(&root, "stroke", none, out);

    /* The root <svg>/<g>/... is itself a container: its own shapes (rare) plus
       its children. Handle the root element (which may itself be a shape if the
       file's outermost tag is e.g. a lone <path>, though normally it's <svg>). */
    const char *tag = root.tag;
    bool rootIsContainer = (strcmp(tag, "svg") == 0 || strcmp(tag, "g") == 0 ||
                            strcmp(tag, "defs") == 0 || strcmp(tag, "title") == 0 ||
                            strcmp(tag, "desc") == 0 || strcmp(tag, "style") == 0);
    /* The ROOT's own ignored attributes. rc_svg_handle_element reports these for
       every other element, and the container path below never goes through it, so
       without this a root-level transform is silent. That is the likeliest place
       for one to be: scaling the whole artwork by putting a transform on the outer
       <svg> is what a design tool exports, which makes the root the case this
       reporting most needs to cover rather than the one it can skip. */
    if (rootIsContainer) {
        rc_svg_report_ignored_attrs(out, &root);
        rc_svg_root_transform(&ps, &root, vx, vy, vw, vh);
        /* A container root is not handled as an element, so the fill-rule it
           hands down is read here; a drawable root reads its own. */
        ps.evenodd = rc_svg_fill_rule(&root, false);
    }

    if (!rootIsContainer) {
        /* Treat the root itself as a drawable element (still recurse for kids). */
        rc_svg_handle_element(&ps, svg, len, &i, &root, rootStrokeW, rootFill, rootStroke, 0);
    } else if (!root.selfClose) {
        rc_svg_collect(&ps, svg, len, &i, &root, rootStrokeW, rootFill, rootStroke, 1);
    }

    /* Normalize: translate every shape so the viewBox origin is 0,0. */
    if (vx != 0.0f || vy != 0.0f) {
        for (int k = 0; k < ps.ptCount; k++) {
            ps.pts[k].x -= vx;
            ps.pts[k].y -= vy;
        }
        for (int s = 0; s < ps.shapeCount; s++) {
            RcSvgShape *sh = &ps.shapes[s];
            if (sh->kind == RC_SHAPE_RECT) {
                sh->x -= vx; sh->y -= vy;
            } else if (sh->kind == RC_SHAPE_CIRCLE || sh->kind == RC_SHAPE_ELLIPSE) {
                sh->cx -= vx; sh->cy -= vy;
            }
            /* PATH points already shifted in the pool loop above. */
        }
    }

    if (fabsf(vw - vh) > 1e-6f) {
        rc_svg_warn(out, "non-square viewBox; generated icon uses max dimension for scaling");
    }

    /* AN INVISIBLE SHAPE IS NOT AN UNPAINTED ONE, so it leaves before the
       routing below reads paints. A fill the author SET to none, with no stroke
       (SVG's initial stroke is none), draws nothing in a browser. Kept, it
       would reach rc_svg_build_mono in any document with no baked color, and
       that builder strokes a shape with neither paint - so the invisible 24x24
       box in every Tabler and classic Material icon would come out as a frame
       round the viewBox. An unpainted shape stays (nothing named its fill, or
       the color named was unreadable), and so does every shape in a document
       with a <style> sheet, whose selectors this parser does not evaluate and
       may paint the shape after all. RayClay's runtime parser (rcSvg) drops the
       same shapes. */
    if (!ps.sawStyle) {
        int kept = 0;
        for (int s = 0; s < ps.shapeCount; s++) {
            const RcSvgShape *sh = &ps.shapes[s];
            if (sh->fill.stated && sh->stroke.kind == RC_PAINT_NONE &&
                !sh->stroke.unread) {
                continue;
            }
            ps.shapes[kept++] = *sh;
        }
        ps.shapeCount = kept;
    }

    /* SVG'S INITIAL FILL IS BLACK, so a shape whose fill nothing in the cascade
       names is filled, as rcLoadSvg fills it: a
       bare <path d=...> glyph is a filled shape, not its outline. Which black:
       currentColor in a document that bakes no color, so the icon keeps its
       color argument; the literal {0,0,0,255} beside a baked color, as a
       browser draws it. A <style> sheet may paint the shape (its selectors are
       not read), and an unreadable color was lost, not withheld: both keep the
       outline route. */
    if (!ps.sawStyle) {
        bool bakes = false;
        for (int s = 0; s < ps.shapeCount; s++) {
            bakes = bakes || rc_svg_paint_is_baked(ps.shapes[s].fill) ||
                    rc_svg_paint_is_baked(ps.shapes[s].stroke);
        }
        for (int s = 0; s < ps.shapeCount; s++) {
            RcSvgShape *sh = &ps.shapes[s];
            if (sh->fill.kind == RC_PAINT_NONE && !sh->fill.stated && !sh->fill.unread) {
                sh->fill.kind  = bakes ? RC_PAINT_BAKED : RC_PAINT_CURRENT;
                sh->fill.color = RC_LIT(RC_Color){ 0, 0, 0, 255 };
                sh->initialFill = true;
            }
        }
    }

    /* colored? -> any shape bakes a concrete fill OR stroke color. */
    bool colored = false;
    for (int s = 0; s < ps.shapeCount; s++) {
        if (rc_svg_paint_is_baked(ps.shapes[s].fill) ||
            rc_svg_paint_is_baked(ps.shapes[s].stroke)) {
            colored = true;
            break;
        }
    }
    out->colored = colored;

    if (colored) {
        rc_svg_build_colored(out, &ps);
    } else {
        rc_svg_build_mono(out, &ps);
    }

    if (ps.shapeCount == 0) {
        rc_svg_warn(out, "no supported drawable stroke shapes found");
    }

    out->ok = true;
    return true;
}

/* Public: live preview draw - OPT-IN. Define RC_SVG2ICON_PREVIEW before
   including this header to draw an icon you just parsed: it calls the RayClay
   icon helpers and so obliges the caller to link the library. An offline
   generator leaves it undefined and links against nothing but libm. */
#ifdef RC_SVG2ICON_PREVIEW
static inline void rc_svg2icon_draw(const RcSvgIcon *icon, RC_BoundingBox bounds, RC_Color monoColor) {
    if (!icon || !icon->ok) {
        return;
    }
    float viewBox = icon->viewW > icon->viewH ? icon->viewW : icon->viewH;
    if (viewBox <= 0.0f) {
        viewBox = 24.0f;
    }

    for (int oi = 0; oi < icon->opCount; oi++) {
        const RcSvgOp *op = &icon->ops[oi];
        RC_Color color = op->baked ? op->color : monoColor;
        const RC_IconPoint *pts = icon->points + op->pointOff;

        switch (op->kind) {
        case RC_SVG_ROUND_LINE:
            rcIconDrawRoundLine(bounds, op->p[0], op->p[1], op->p[2], op->p[3],
                                  viewBox, op->stroke, color);
            break;
        case RC_SVG_POLYLINE:
            rcIconDrawPolyline(bounds, pts, op->pointCount, viewBox,
                                 op->stroke, op->closed, color);
            break;
        case RC_SVG_CIRCLE_STROKE:
            rcIconDrawCircleStroke(bounds, op->p[0], op->p[1], op->p[2],
                                     viewBox, op->stroke, color);
            break;
        case RC_SVG_RRECT_STROKE:
            rcIconDrawRoundedRectStroke(bounds, op->p[0], op->p[1], op->p[2], op->p[3],
                                          op->p[4], viewBox, op->stroke, color);
            break;
        case RC_SVG_FILLED_POLY:
            /* All of it, as the runtime fills it: a fill past the cap draws whole. */
            rcIconDrawFilledPolygon(bounds, pts, op->pointCount, viewBox, color);
            break;
        case RC_SVG_FILLED_CIRCLE:
            rcIconDrawFilledCircle(bounds, op->p[0], op->p[1], op->p[2], viewBox, color);
            break;
        case RC_SVG_FILLED_ELLIPSE:
            rcIconDrawFilledEllipse(bounds, op->p[0], op->p[1], op->p[2], op->p[3],
                                      viewBox, color);
            break;
        case RC_SVG_FILLED_QUADS:
            /* A fill cut by its fill-rule: each quad a four-point polygon, as the
               emitted header draws it. */
            for (int q = 0; q + 4 <= op->pointCount; q += 4) {
                rcIconDrawFilledPolygon(bounds, pts + q, 4, viewBox, color);
            }
            break;
        }
    }
}
#endif /* RC_SVG2ICON_PREVIEW */

/* Emit helpers: float / name formatting. */

/* Render a float: round to `precision`, strip trailing zeros/dot, map -0 -> 0,
   always end "N.0f". */
static void rc_svg_fmt_float(float value, int precision, char *buf, int cap) {
    if (precision < 0) precision = 0;
    if (precision > 9) precision = 9;

    double v = (double)value;
    double thresh = 0.5 * pow(10.0, -(double)precision);
    if (fabs(v) < thresh) {
        v = 0.0;
    }
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%.*f", precision, v);

    /* Strip trailing zeros, then a trailing dot. */
    int n = (int)strlen(tmp);
    if (strchr(tmp, '.')) {
        while (n > 0 && tmp[n - 1] == '0') {
            n--;
        }
        if (n > 0 && tmp[n - 1] == '.') {
            n--;
        }
    }
    tmp[n] = '\0';

    /* Map "" / "-0" -> "0". */
    if (tmp[0] == '\0' || strcmp(tmp, "-0") == 0) {
        strcpy(tmp, "0");
        n = 1;
    }

    /* Ensure a decimal point (no 'e' in our fixed-notation output). */
    if (!strchr(tmp, '.')) {
        if (n + 2 < (int)sizeof(tmp)) {
            strcat(tmp, ".0");
        }
    }
    snprintf(buf, (size_t)cap, "%sf", tmp);
}

/* Append to a bounded output buffer; latches w->overflow once full (writes
   become no-ops; the caller checks the flag after the emit completes). */
typedef struct { char *buf; int cap; int len; bool overflow; } RcSvgWriter;

static void rc_svg_w_str(RcSvgWriter *w, const char *s) {
    if (w->overflow) {
        return;
    }
    int n = (int)strlen(s);
    if (w->len + n >= w->cap) {
        w->overflow = true;
        return;
    }
    memcpy(w->buf + w->len, s, (size_t)n);
    w->len += n;
    w->buf[w->len] = '\0';
}

static void rc_svg_w_float(RcSvgWriter *w, float value, int precision) {
    char b[48];
    rc_svg_fmt_float(value, precision, b, (int)sizeof(b));
    rc_svg_w_str(w, b);
}

static void rc_svg_w_int(RcSvgWriter *w, int value) {
    char b[16];
    snprintf(b, sizeof(b), "%d", value);
    rc_svg_w_str(w, b);
}

/* PascalCase a name: split on non-alnum, uppercase each part's first letter,
   preserve the rest. Prefix "Icon" if it would start with a digit / be empty. */
static void rc_svg_pascal(const char *name, char *out, int cap) {
    int oi = 0;
    int i = 0;
    int nlen = (int)strlen(name);
    while (i < nlen && oi < cap - 1) {
        /* Skip separators. */
        while (i < nlen && !isalnum((unsigned char)name[i])) {
            i++;
        }
        if (i >= nlen) {
            break;
        }
        /* First char of the part -> upper; rest preserved. */
        out[oi++] = (char)toupper((unsigned char)name[i]);
        i++;
        while (i < nlen && isalnum((unsigned char)name[i]) && oi < cap - 1) {
            out[oi++] = name[i];
            i++;
        }
    }
    out[oi] = '\0';
    if (out[0] == '\0') {
        strncpy(out, "Icon", (size_t)cap - 1);
        out[cap - 1] = '\0';
        return;
    }
    if (isdigit((unsigned char)out[0])) {
        char tmp[128];
        snprintf(tmp, sizeof(tmp), "Icon%.120s", out);
        rc_svg_str_copy(out, tmp, (size_t)cap);
    }
}

/* Split into words across separators, camel/Pascal humps, and digit boundaries,
   join with '_', uppercase, then wrap as RC_ICON_<...>_H. */
static void rc_svg_guard(const char *name, char *out, int cap) {
    char words[64];   /* underscore-joined word buffer */
    int wi = 0;
    int i = 0;
    int nlen = (int)strlen(name);
    bool anyWord = false;

    while (i < nlen) {
        /* Skip separators. */
        while (i < nlen && !isalnum((unsigned char)name[i])) {
            i++;
        }
        if (i >= nlen) {
            break;
        }
        /* Consume one chunk (run of alnum), splitting into words. */
        while (i < nlen && isalnum((unsigned char)name[i])) {
            char c = name[i];
            /* Determine a word starting at i, per the regex alternatives:
               [A-Z]+(?=[A-Z][a-z]) | [A-Z]?[a-z]+ | [A-Z]+ | \d+ */
            int start = i;
            if (isdigit((unsigned char)c)) {
                while (i < nlen && isdigit((unsigned char)name[i])) i++;
            } else if (isupper((unsigned char)c)) {
                /* Look ahead: run of uppercase. */
                int j = i;
                while (j < nlen && isupper((unsigned char)name[j])) j++;
                int upperRun = j - i;
                if (upperRun > 1 && j < nlen && islower((unsigned char)name[j])) {
                    /* [A-Z]+(?=[A-Z][a-z]) : take all but the last upper. */
                    i = j - 1;
                } else if (upperRun == 1) {
                    /* [A-Z]?[a-z]+ : one upper then lowers. */
                    i++;
                    while (i < nlen && islower((unsigned char)name[i])) i++;
                } else {
                    /* [A-Z]+ : the whole uppercase run. */
                    i = j;
                }
            } else { /* lowercase */
                while (i < nlen && islower((unsigned char)name[i])) i++;
            }
            /* Append the word [start, i) (uppercased) with a separating '_'. */
            if (anyWord && wi < (int)sizeof(words) - 1) {
                words[wi++] = '_';
            }
            for (int k = start; k < i && wi < (int)sizeof(words) - 1; k++) {
                words[wi++] = (char)toupper((unsigned char)name[k]);
            }
            anyWord = true;
        }
    }
    words[wi] = '\0';

    if (words[0] == '\0') {
        strncpy(words, "ICON", sizeof(words) - 1);
        words[sizeof(words) - 1] = '\0';
    }
    if (isdigit((unsigned char)words[0])) {
        char tmp[80];
        size_t n = (size_t)snprintf(tmp, sizeof(tmp), "ICON_%s", words);
        if (n > sizeof(words) - 1) n = sizeof(words) - 1;
        memcpy(words, tmp, n);
        words[n] = '\0';
    }
    snprintf(out, (size_t)cap, "RC_ICON_%s_H", words);
}

/* Render a viewBox dimension for the doc comment. */
static void rc_svg_fmt_viewbox_dim(float value, char *buf, int cap) {
    snprintf(buf, (size_t)cap, "%g", (double)value);
}

/* Emit: doc comment. */

static void rc_svg_emit_doc_comment(RcSvgWriter *w, const char *svgName,
                                    float vw, float vh) {
    const int DOC_WRAP_WIDTH = 81;
    char wtag[32], htag[32];
    rc_svg_fmt_viewbox_dim(vw, wtag, (int)sizeof(wtag));
    rc_svg_fmt_viewbox_dim(vh, htag, (int)sizeof(htag));

    char prose[512];
    snprintf(prose, sizeof(prose),
             "Generated from %s. Source viewBox: %sx%s.",
             svgName, wtag, htag);

    /* Greedy-fill the first line to DOC_WRAP_WIDTH (4-space indent included);
       the whole remainder goes on the second line. */
    const char *indent = "    ";
    char first[256];
    snprintf(first, sizeof(first), "%s", indent);
    int firstLen = (int)strlen(first);
    int taken = 0;

    /* Tokenize prose on spaces. */
    char proseCopy[512];
    snprintf(proseCopy, sizeof(proseCopy), "%s", prose);
    char *words[128];
    int wordCount = 0;
    char *tok = strtok(proseCopy, " ");
    while (tok && wordCount < 128) {
        words[wordCount++] = tok;
        tok = strtok(NULL, " ");
    }

    for (int i = 0; i < wordCount; i++) {
        int add = (int)strlen(words[i]) + (taken ? 1 : 0);
        if (firstLen + add <= DOC_WRAP_WIDTH) {
            if (taken && firstLen + 1 < (int)sizeof(first)) {
                strcat(first, " ");
                firstLen++;
            }
            strncat(first, words[i], sizeof(first) - firstLen - 1);
            firstLen += (int)strlen(words[i]);
            taken++;
        } else {
            break;
        }
    }

    char second[512];
    snprintf(second, sizeof(second), "%s", indent);
    for (int i = taken; i < wordCount; i++) {
        if (i > taken) {
            strncat(second, " ", sizeof(second) - strlen(second) - 1);
        }
        strncat(second, words[i], sizeof(second) - strlen(second) - 1);
    }

    rc_svg_w_str(w, "/*\n");
    rc_svg_w_str(w, first);
    rc_svg_w_str(w, "\n");
    /* Only when a word did not fit: prose short enough for one line must not
       trail an indent-only second line into the reader's own source tree. */
    if (taken < wordCount) {
        rc_svg_w_str(w, second);
        rc_svg_w_str(w, "\n");
    }
    rc_svg_w_str(w, "*/\n");
}

/* Emit: point array, shared by mono + colored. It writes op->pointCount rows,
   and each draw call passes that count as an int literal: a C++ TU that
   includes the generated header with -Wold-style-cast finds no cast. */

static void rc_svg_emit_point_array(RcSvgWriter *w, const RcSvgIcon *icon,
                                    const RcSvgOp *op, int index, int precision) {
    char head[64];
    snprintf(head, sizeof(head), "    static const RC_IconPoint path%d[] = {\n", index);
    rc_svg_w_str(w, head);
    const RC_IconPoint *pts = icon->points + op->pointOff;
    for (int k = 0; k < op->pointCount; k++) {
        rc_svg_w_str(w, "        { ");
        rc_svg_w_float(w, pts[k].x, precision);
        rc_svg_w_str(w, ", ");
        rc_svg_w_float(w, pts[k].y, precision);
        rc_svg_w_str(w, " }");
        if (k + 1 < op->pointCount) {
            rc_svg_w_str(w, ",");
        }
        rc_svg_w_str(w, "\n");
    }
    rc_svg_w_str(w, "    };\n");
}

/* Continuation padding: N spaces (to align wrapped call arguments). */
static void rc_svg_w_pad(RcSvgWriter *w, int n) {
    for (int k = 0; k < n && !w->overflow; k++) {
        rc_svg_w_str(w, " ");
    }
}

/* Emit a fill cut into quads (FILLED_QUADS) after its point array `arr`: a
   loop filling each quad as a four-point rcIconDrawFilledPolygon. The count is
   the array's rows, an int literal. */
static void rc_svg_emit_quads_loop(RcSvgWriter *w, const RcSvgOp *op, const char *arr,
                                   const char *cexpr) {
    rc_svg_w_str(w, "    for (int q = 0; q < ");
    rc_svg_w_int(w, op->pointCount);
    rc_svg_w_str(w, "; q += 4)\n");
    rc_svg_w_str(w, "        rcIconDrawFilledPolygon(bounds, ");
    rc_svg_w_str(w, arr);
    rc_svg_w_str(w, " + q, 4, viewBox, ");
    rc_svg_w_str(w, cexpr);
    rc_svg_w_str(w, ");\n");
}

/* Public: the function name rc_svg2icon_emit will give an icon generated under
   `name`, so a caller can show it before anything is written. Same transform. */
static inline void rc_svg2icon_symbol(const char *name, char *out, int cap) {
    char pascal[128];
    if (!out || cap <= 0) {
        return;
    }
    rc_svg_pascal(name ? name : "Icon", pascal, (int)sizeof(pascal));
    /* Bound the name explicitly rather than leaving snprintf to truncate: the
       caller's buffer may be shorter than a Pascal name, and "rcIcon" is 6. */
    snprintf(out, (size_t)cap, "rcIcon%.*s", cap > 7 ? cap - 7 : 0, pascal);
}

/* Public: emit. */

static inline int rc_svg2icon_emit(const RcSvgIcon *icon, const char *pascalName,
                            const char *svgName, int precision, char *out, int cap) {
    if (!icon || !out || cap <= 0) {
        return -1;
    }
    if (precision < 0) precision = 0;
    if (precision > 9) precision = 9;

    RcSvgWriter w = { out, cap, 0, false };
    out[0] = '\0';

    char pascal[128];
    rc_svg_pascal(pascalName ? pascalName : "Icon", pascal, (int)sizeof(pascal));
    char guard[160];
    rc_svg_guard(pascalName ? pascalName : "Icon", guard, (int)sizeof(guard));

    float viewBoxSize = icon->viewW > icon->viewH ? icon->viewW : icon->viewH;

    char drawFn[160];
    snprintf(drawFn, sizeof(drawFn), "rcDrawIcon%s", pascal);
    int sigCont = (int)strlen("static inline void ") + (int)strlen(drawFn) + 1; /* '(' */

    /* Header preamble. */
    rc_svg_w_str(&w, "#ifndef ");
    rc_svg_w_str(&w, guard);
    rc_svg_w_str(&w, "\n#define ");
    rc_svg_w_str(&w, guard);
    rc_svg_w_str(&w, "\n\n#include \"rc_icons_common.h\"\n\n");

    rc_svg_emit_doc_comment(&w, svgName ? svgName : "icon.svg",
                            icon->viewW, icon->viewH);

    /* Draw-fn signature. */
    rc_svg_w_str(&w, "static inline void ");
    rc_svg_w_str(&w, drawFn);
    rc_svg_w_str(&w, "(RC_BoundingBox bounds,\n");
    rc_svg_w_pad(&w, sigCont);
    rc_svg_w_str(&w, "RC_Color color,\n");
    rc_svg_w_pad(&w, sigCont);
    rc_svg_w_str(&w, "const void *userData) {\n");
    rc_svg_w_str(&w, "    (void)userData;\n");

    if (icon->colored) {
        /* ---- Colored form (rcIcon(size)) ---- */
        bool usesColor = false;
        for (int oi = 0; oi < icon->opCount; oi++) {
            if (!icon->ops[oi].baked) {   /* a slipped-in currentColor op */
                usesColor = true;
                break;
            }
        }
        if (!usesColor) {
            rc_svg_w_str(&w, "    (void)color;\n");
        }
        rc_svg_w_str(&w, "    const float viewBox = ");
        rc_svg_w_float(&w, viewBoxSize, precision);
        rc_svg_w_str(&w, ";\n\n");

        int polyIndex = 0;
        int colIndex = 0;

        for (int oi = 0; oi < icon->opCount; oi++) {
            const RcSvgOp *op = &icon->ops[oi];

            /* Each SOURCE SHAPE is separated by a blank line; a shape's fill +
               stroke ops stay together. op->shapeStart marks the first op of each
               shape. */
            if (oi > 0 && op->shapeStart) {
                rc_svg_w_str(&w, "\n");
            }

            /* Emit any point array first. */
            bool hasArray = (op->kind == RC_SVG_POLYLINE || op->kind == RC_SVG_FILLED_POLY ||
                             op->kind == RC_SVG_FILLED_QUADS);
            char arr[16];
            if (hasArray) {
                snprintf(arr, sizeof(arr), "path%d", polyIndex);
                rc_svg_emit_point_array(&w, icon, op, polyIndex, precision);
                polyIndex++;
            }

            /* Color expression. */
            char cexpr[32];
            if (op->baked) {
                snprintf(cexpr, sizeof(cexpr), "c%d", colIndex);
                rc_svg_w_str(&w, "    const RC_Color ");
                rc_svg_w_str(&w, cexpr);
                rc_svg_w_str(&w, " = { ");
                rc_svg_w_int(&w, (int)op->color.r);
                rc_svg_w_str(&w, ", ");
                rc_svg_w_int(&w, (int)op->color.g);
                rc_svg_w_str(&w, ", ");
                rc_svg_w_int(&w, (int)op->color.b);
                rc_svg_w_str(&w, ", ");
                rc_svg_w_int(&w, (int)op->color.a);
                rc_svg_w_str(&w, " };\n");
                colIndex++;
            } else {
                snprintf(cexpr, sizeof(cexpr), "color");
            }

            switch (op->kind) {
            case RC_SVG_FILLED_POLY:
                rc_svg_w_str(&w, "    rcIconDrawFilledPolygon(bounds, ");
                rc_svg_w_str(&w, arr);
                rc_svg_w_str(&w, ",\n");
                rc_svg_w_pad(&w, (int)strlen("    rcIconDrawFilledPolygon("));
                rc_svg_w_int(&w, op->pointCount);
                rc_svg_w_str(&w, ",\n");
                rc_svg_w_pad(&w, (int)strlen("    rcIconDrawFilledPolygon("));
                rc_svg_w_str(&w, "viewBox, ");
                rc_svg_w_str(&w, cexpr);
                rc_svg_w_str(&w, ");\n");
                break;
            case RC_SVG_FILLED_QUADS:
                rc_svg_emit_quads_loop(&w, op, arr, cexpr);
                break;
            case RC_SVG_POLYLINE:
                rc_svg_w_str(&w, "    rcIconDrawPolyline(bounds, ");
                rc_svg_w_str(&w, arr);
                rc_svg_w_str(&w, ",\n");
                rc_svg_w_pad(&w, (int)strlen("    rcIconDrawPolyline("));
                rc_svg_w_int(&w, op->pointCount);
                rc_svg_w_str(&w, ",\n");
                rc_svg_w_pad(&w, (int)strlen("    rcIconDrawPolyline("));
                rc_svg_w_str(&w, "viewBox, ");
                rc_svg_w_float(&w, op->stroke, precision);
                rc_svg_w_str(&w, op->closed ? ", true, " : ", false, ");
                rc_svg_w_str(&w, cexpr);
                rc_svg_w_str(&w, ");\n");
                break;
            case RC_SVG_FILLED_CIRCLE:
                rc_svg_w_str(&w, "    rcIconDrawFilledCircle(bounds, ");
                rc_svg_w_float(&w, op->p[0], precision);
                rc_svg_w_str(&w, ", ");
                rc_svg_w_float(&w, op->p[1], precision);
                rc_svg_w_str(&w, ", ");
                rc_svg_w_float(&w, op->p[2], precision);
                rc_svg_w_str(&w, ", viewBox, ");
                rc_svg_w_str(&w, cexpr);
                rc_svg_w_str(&w, ");\n");
                break;
            case RC_SVG_FILLED_ELLIPSE:
                rc_svg_w_str(&w, "    rcIconDrawFilledEllipse(bounds, ");
                rc_svg_w_float(&w, op->p[0], precision);
                rc_svg_w_str(&w, ", ");
                rc_svg_w_float(&w, op->p[1], precision);
                rc_svg_w_str(&w, ", ");
                rc_svg_w_float(&w, op->p[2], precision);
                rc_svg_w_str(&w, ", ");
                rc_svg_w_float(&w, op->p[3], precision);
                rc_svg_w_str(&w, ", viewBox, ");
                rc_svg_w_str(&w, cexpr);
                rc_svg_w_str(&w, ");\n");
                break;
            case RC_SVG_CIRCLE_STROKE:
                rc_svg_w_str(&w, "    rcIconDrawCircleStroke(bounds, ");
                rc_svg_w_float(&w, op->p[0], precision);
                rc_svg_w_str(&w, ", ");
                rc_svg_w_float(&w, op->p[1], precision);
                rc_svg_w_str(&w, ", ");
                rc_svg_w_float(&w, op->p[2], precision);
                rc_svg_w_str(&w, ", viewBox, ");
                rc_svg_w_float(&w, op->stroke, precision);
                rc_svg_w_str(&w, ", ");
                rc_svg_w_str(&w, cexpr);
                rc_svg_w_str(&w, ");\n");
                break;
            case RC_SVG_RRECT_STROKE:
                rc_svg_w_str(&w, "    rcIconDrawRoundedRectStroke(bounds, ");
                rc_svg_w_float(&w, op->p[0], precision);
                rc_svg_w_str(&w, ", ");
                rc_svg_w_float(&w, op->p[1], precision);
                rc_svg_w_str(&w, ", ");
                rc_svg_w_float(&w, op->p[2], precision);
                rc_svg_w_str(&w, ", ");
                rc_svg_w_float(&w, op->p[3], precision);
                rc_svg_w_str(&w, ",\n");
                rc_svg_w_pad(&w, (int)strlen("    rcIconDrawRoundedRectStroke("));
                rc_svg_w_float(&w, op->p[4], precision);
                rc_svg_w_str(&w, ", viewBox, ");
                rc_svg_w_float(&w, op->stroke, precision);
                rc_svg_w_str(&w, ", ");
                rc_svg_w_str(&w, cexpr);
                rc_svg_w_str(&w, ");\n");
                break;
            case RC_SVG_ROUND_LINE:
                rc_svg_w_str(&w, "    rcIconDrawRoundLine(bounds, ");
                rc_svg_w_float(&w, op->p[0], precision);
                rc_svg_w_str(&w, ", ");
                rc_svg_w_float(&w, op->p[1], precision);
                rc_svg_w_str(&w, ", ");
                rc_svg_w_float(&w, op->p[2], precision);
                rc_svg_w_str(&w, ", ");
                rc_svg_w_float(&w, op->p[3], precision);
                rc_svg_w_str(&w, ", viewBox, ");
                rc_svg_w_float(&w, op->stroke, precision);
                rc_svg_w_str(&w, ", ");
                rc_svg_w_str(&w, cexpr);
                rc_svg_w_str(&w, ");\n");
                break;
            }
        }

        rc_svg_w_str(&w, "}\n\n");
        rc_svg_w_str(&w, "static inline void rcIcon");
        rc_svg_w_str(&w, pascal);
        rc_svg_w_str(&w, "(float size) {\n");
        rc_svg_w_str(&w, "    RC_Color baked = { 0, 0, 0, 255 };   /* unused; the artwork bakes its own colors */\n");
        rc_svg_w_str(&w, "    rcIconEmit(size, baked, ");
        rc_svg_w_str(&w, drawFn);
        rc_svg_w_str(&w, ", NULL);\n}\n\n");
    } else {
        /* ---- Mono form (rcIcon(size, color)) ---- */
        if (icon->opCount == 0) {
            rc_svg_w_str(&w, "    (void)bounds; (void)color;\n");
        } else {
            /* Uniform stroke? All STROKING op widths equal, rounded to
               `precision`. A fill op carries no width, so including one would
               compare a zero against real widths and drop the shared constant
               for a value no emitted call ever reads. */
            bool uniform = false;
            int  firstStroke = -1;
            char firstW[48];
            for (int oi = 0; oi < icon->opCount; oi++) {
                if (!rc_svg_op_strokes(icon->ops[oi].kind)) {
                    continue;
                }
                if (firstStroke < 0) {
                    firstStroke = oi;
                    uniform = true;
                    rc_svg_fmt_float(icon->ops[oi].stroke, precision, firstW, (int)sizeof(firstW));
                    continue;
                }
                {
                    char thisW[48];
                    rc_svg_fmt_float(icon->ops[oi].stroke, precision, thisW, (int)sizeof(thisW));
                    if (strcmp(firstW, thisW) != 0) {
                        uniform = false;
                        break;
                    }
                }
            }

            /* Aligned decl block: "viewBox" is the widest name (7 chars). */
            rc_svg_w_str(&w, "    const float viewBox = ");
            rc_svg_w_float(&w, viewBoxSize, precision);
            rc_svg_w_str(&w, ";\n");
            if (uniform) {
                rc_svg_w_str(&w, "    const float stroke  = ");
                rc_svg_w_float(&w, icon->ops[firstStroke].stroke, precision);
                rc_svg_w_str(&w, ";\n");
            }
            rc_svg_w_str(&w, "\n");

            /* Op emission with the polyline blank-line rule. */
            int polyIndex = 0;
            int prevKind = -1;
            for (int oi = 0; oi < icon->opCount; oi++) {
                const RcSvgOp *op = &icon->ops[oi];
                bool isBlock = (op->kind == RC_SVG_POLYLINE || op->kind == RC_SVG_FILLED_POLY ||
                                op->kind == RC_SVG_FILLED_QUADS);
                bool prevBlock = (prevKind == RC_SVG_POLYLINE || prevKind == RC_SVG_FILLED_POLY ||
                                  prevKind == RC_SVG_FILLED_QUADS);
                if (oi > 0 && (isBlock || prevBlock)) {
                    rc_svg_w_str(&w, "\n");
                }

                switch (op->kind) {
                case RC_SVG_ROUND_LINE:
                    rc_svg_w_str(&w, "    rcIconDrawRoundLine(bounds, ");
                    rc_svg_w_float(&w, op->p[0], precision);
                    rc_svg_w_str(&w, ", ");
                    rc_svg_w_float(&w, op->p[1], precision);
                    rc_svg_w_str(&w, ", ");
                    rc_svg_w_float(&w, op->p[2], precision);
                    rc_svg_w_str(&w, ", ");
                    rc_svg_w_float(&w, op->p[3], precision);
                    rc_svg_w_str(&w, ", viewBox, ");
                    if (uniform) {
                        rc_svg_w_str(&w, "stroke");
                    } else {
                        rc_svg_w_float(&w, op->stroke, precision);
                    }
                    rc_svg_w_str(&w, ", color);\n");
                    break;
                case RC_SVG_CIRCLE_STROKE:
                    rc_svg_w_str(&w, "    rcIconDrawCircleStroke(bounds, ");
                    rc_svg_w_float(&w, op->p[0], precision);
                    rc_svg_w_str(&w, ", ");
                    rc_svg_w_float(&w, op->p[1], precision);
                    rc_svg_w_str(&w, ", ");
                    rc_svg_w_float(&w, op->p[2], precision);
                    rc_svg_w_str(&w, ", viewBox, ");
                    if (uniform) {
                        rc_svg_w_str(&w, "stroke");
                    } else {
                        rc_svg_w_float(&w, op->stroke, precision);
                    }
                    rc_svg_w_str(&w, ", color);\n");
                    break;
                case RC_SVG_RRECT_STROKE:
                    rc_svg_w_str(&w, "    rcIconDrawRoundedRectStroke(bounds, ");
                    rc_svg_w_float(&w, op->p[0], precision);
                    rc_svg_w_str(&w, ", ");
                    rc_svg_w_float(&w, op->p[1], precision);
                    rc_svg_w_str(&w, ", ");
                    rc_svg_w_float(&w, op->p[2], precision);
                    rc_svg_w_str(&w, ", ");
                    rc_svg_w_float(&w, op->p[3], precision);
                    rc_svg_w_str(&w, ",\n");
                    rc_svg_w_pad(&w, (int)strlen("    rcIconDrawRoundedRectStroke("));
                    rc_svg_w_float(&w, op->p[4], precision);
                    rc_svg_w_str(&w, ", viewBox, ");
                    if (uniform) {
                        rc_svg_w_str(&w, "stroke");
                    } else {
                        rc_svg_w_float(&w, op->stroke, precision);
                    }
                    rc_svg_w_str(&w, ", color);\n");
                    break;
                case RC_SVG_POLYLINE: {
                    char arr[16];
                    snprintf(arr, sizeof(arr), "path%d", polyIndex);
                    rc_svg_emit_point_array(&w, icon, op, polyIndex, precision);
                    polyIndex++;
                    rc_svg_w_str(&w, "    rcIconDrawPolyline(bounds, ");
                    rc_svg_w_str(&w, arr);
                    rc_svg_w_str(&w, ",\n");
                    rc_svg_w_pad(&w, (int)strlen("    rcIconDrawPolyline("));
                    rc_svg_w_int(&w, op->pointCount);
                    rc_svg_w_str(&w, ",\n");
                    rc_svg_w_pad(&w, (int)strlen("    rcIconDrawPolyline("));
                    rc_svg_w_str(&w, "viewBox, ");
                    if (uniform) {
                        rc_svg_w_str(&w, "stroke");
                    } else {
                        rc_svg_w_float(&w, op->stroke, precision);
                    }
                    rc_svg_w_str(&w, op->closed ? ", true, color);\n" : ", false, color);\n");
                    break;
                }
                case RC_SVG_FILLED_CIRCLE:
                    rc_svg_w_str(&w, "    rcIconDrawFilledCircle(bounds, ");
                    rc_svg_w_float(&w, op->p[0], precision);
                    rc_svg_w_str(&w, ", ");
                    rc_svg_w_float(&w, op->p[1], precision);
                    rc_svg_w_str(&w, ", ");
                    rc_svg_w_float(&w, op->p[2], precision);
                    rc_svg_w_str(&w, ", viewBox, color);\n");
                    break;
                case RC_SVG_FILLED_ELLIPSE:
                    rc_svg_w_str(&w, "    rcIconDrawFilledEllipse(bounds, ");
                    rc_svg_w_float(&w, op->p[0], precision);
                    rc_svg_w_str(&w, ", ");
                    rc_svg_w_float(&w, op->p[1], precision);
                    rc_svg_w_str(&w, ", ");
                    rc_svg_w_float(&w, op->p[2], precision);
                    rc_svg_w_str(&w, ", ");
                    rc_svg_w_float(&w, op->p[3], precision);
                    rc_svg_w_str(&w, ", viewBox, color);\n");
                    break;
                case RC_SVG_FILLED_POLY: {
                    char arr[16];
                    snprintf(arr, sizeof(arr), "path%d", polyIndex);
                    rc_svg_emit_point_array(&w, icon, op, polyIndex, precision);
                    polyIndex++;
                    rc_svg_w_str(&w, "    rcIconDrawFilledPolygon(bounds, ");
                    rc_svg_w_str(&w, arr);
                    rc_svg_w_str(&w, ",\n");
                    rc_svg_w_pad(&w, (int)strlen("    rcIconDrawFilledPolygon("));
                    rc_svg_w_int(&w, op->pointCount);
                    rc_svg_w_str(&w, ",\n");
                    rc_svg_w_pad(&w, (int)strlen("    rcIconDrawFilledPolygon("));
                    rc_svg_w_str(&w, "viewBox, color);\n");
                    break;
                }
                case RC_SVG_FILLED_QUADS: {
                    char arr[16];
                    snprintf(arr, sizeof(arr), "path%d", polyIndex);
                    rc_svg_emit_point_array(&w, icon, op, polyIndex, precision);
                    polyIndex++;
                    rc_svg_emit_quads_loop(&w, op, arr, "color");
                    break;
                }
                default:
                    break;
                }
                prevKind = (int)op->kind;
            }
        }

        rc_svg_w_str(&w, "}\n\n");
        rc_svg_w_str(&w, "static inline void rcIcon");
        rc_svg_w_str(&w, pascal);
        rc_svg_w_str(&w, "(float size, RC_Color color) {\n");
        rc_svg_w_str(&w, "    rcIconEmit(size, color, ");
        rc_svg_w_str(&w, drawFn);
        rc_svg_w_str(&w, ", NULL);\n}\n\n");
    }

    rc_svg_w_str(&w, "#endif /* ");
    rc_svg_w_str(&w, guard);
    rc_svg_w_str(&w, " */\n");

    if (w.overflow) {
        return -1;
    }
    return w.len;
}

#endif /* RC_SVG2ICON_H */
