#ifndef RC_SVG2ICON_H
#define RC_SVG2ICON_H

/*
 * rc_svg2icon.h - convert simple stroke/filled SVG icons (Lucide-style strokes,
 * or hand-authored multi-colour artwork) into RayClay icon ops, then either
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

/* M_PI is not part of ISO C99; provide a fallback for strict -std=c99 builds. */
#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

/* Public op IR. */

typedef enum { RC_SVG_ROUND_LINE, RC_SVG_POLYLINE, RC_SVG_CIRCLE_STROKE,
               RC_SVG_RRECT_STROKE, RC_SVG_FILLED_POLY, RC_SVG_FILLED_CIRCLE,
               RC_SVG_FILLED_ELLIPSE } RcSvgOpKind;

typedef struct {
    RcSvgOpKind kind;
    float p[5];              /* ROUND_LINE:x0,y0,x1,y1 . CIRCLE_STROKE/FILLED_CIRCLE:cx,cy,r .
                               RRECT_STROKE:x,y,w,h,rx . FILLED_ELLIPSE:cx,cy,rx,ry */
    int   pointOff, pointCount;   /* slice into RcSvgIcon.points (POLYLINE / FILLED_POLY) */
    float stroke;            /* stroke width in viewBox units (stroke ops) */
    bool  closed;            /* POLYLINE only */
    bool  baked;             /* true -> draw with .color; false -> runtime mono color */
    RC_Color color;        /* baked colour when .baked */
} RcSvgOp;

#define RC_SVG_MAX_OPS 512
#define RC_SVG_MAX_POINTS 8192
#define RC_SVG_MAX_WARN 16

/* The runtime draws at most this many points per path; a flattened path above it
   has its TAIL vertices dropped at DRAW time. Single-sourced from the library's
   RC_ICON_MAX_PTS (rayclay/icons/rc_icons_common.h, pulled in via rayclay.h) so
   the converter's GENERATE-time warning can never drift from the runtime cap. */
#define RC_SVG__RUNTIME_MAX_PTS RC_ICON_MAX_PTS

typedef struct {
    float viewW, viewH;      /* viewBox size; shapes normalised so origin is 0,0 */
    RcSvgOp ops[RC_SVG_MAX_OPS]; int opCount;
    RC_IconPoint points[RC_SVG_MAX_POINTS]; int pointCount;
    bool colored;            /* any baked fill/stroke -> rcIcon(size) form on emit */
    bool ok; char err[160];
    const char *warn[RC_SVG_MAX_WARN]; int warnCount;   /* static-string warnings */
} RcSvgIcon;

/* Internal geometry / parse scratch. */

#define RC_SVG__MAX_SHAPES 512
#define RC_SVG__MAX_SHAPE_POINTS 4096
#define RC_SVG__MAX_ATTRS 32
#define RC_SVG__MAX_DEPTH 64

typedef struct { float x, y; } RcSvgPoint;

/* A resolved paint: NONE (no paint), CURRENT (the runtime colour argument), or a
   baked (r,g,b,a) colour. */
typedef enum { RC_PAINT_NONE, RC_PAINT_CURRENT, RC_PAINT_BAKED } RcSvgPaintKind;
typedef struct {
    RcSvgPaintKind kind;
    RC_Color color;        /* valid when kind == RC_PAINT_BAKED */
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
} RcSvgShape;

/* Parse-wide mutable state, kept off the stack to bound recursion cheaply. */
typedef struct {
    RcSvgShape  shapes[RC_SVG__MAX_SHAPES];  int shapeCount;
    RcSvgPoint  pts[RC_SVG__MAX_SHAPE_POINTS]; int ptCount;
    int   curveSteps;
    float arcDeg;
    RcSvgIcon  *icon;        /* for warning collection */
    bool  overflow;          /* set once any pool is exhausted */
} RcSvgParse;

/* Local helpers that keep the converter half free of the RayClay runtime. */

/* rcStrCopy's contract, spelled locally: the converter half must link against
   nothing but libm, and two lines of C99 buy that back. */
static void rc_svg__str_copy(char *dst, const char *src, size_t cap) {
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

static void rc_svg__warn(RcSvgIcon *icon, const char *msg) {
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

static bool rc_svg__is_cmd_char(char c) {
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
static bool rc_svg__scan_float(const char *s, int len, int *i, float *out) {
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
static bool rc_svg__scan_flag(const char *s, int len, int *i, bool *out) {
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
static int rc_svg__comment_end(const char *s, int len, int from) {
    for (int j = from; j + 2 < len; j++) {
        if (s[j] == '-' && s[j + 1] == '-' && s[j + 2] == '>') {
            return j + 3;
        }
    }
    return -1;
}

/* Skip whitespace and commas (SVG treats commas as separators). */
static void rc_svg__skip_sep(const char *s, int len, int *i) {
    while (*i < len && (isspace((unsigned char)s[*i]) || s[*i] == ',')) {
        (*i)++;
    }
}

/* Bezier / arc flattening. */

static bool rc_svg__nearly_same(RcSvgPoint a, RcSvgPoint b) {
    const float eps = 1e-6f;
    return fabsf(a.x - b.x) <= eps && fabsf(a.y - b.y) <= eps;
}

static RcSvgPoint rc_svg__cubic(RcSvgPoint p0, RcSvgPoint p1,
                                RcSvgPoint p2, RcSvgPoint p3, float t) {
    float mt = 1.0f - t;
    RcSvgPoint r;
    r.x = mt * mt * mt * p0.x + 3.0f * mt * mt * t * p1.x
        + 3.0f * mt * t * t * p2.x + t * t * t * p3.x;
    r.y = mt * mt * mt * p0.y + 3.0f * mt * mt * t * p1.y
        + 3.0f * mt * t * t * p2.y + t * t * t * p3.y;
    return r;
}

static RcSvgPoint rc_svg__quad(RcSvgPoint p0, RcSvgPoint p1, RcSvgPoint p2, float t) {
    float mt = 1.0f - t;
    RcSvgPoint r;
    r.x = mt * mt * p0.x + 2.0f * mt * t * p1.x + t * t * p2.x;
    r.y = mt * mt * p0.y + 2.0f * mt * t * p1.y + t * t * p2.y;
    return r;
}

static float rc_svg__angle_between(float ux, float uy, float vx, float vy) {
    float dot = ux * vx + uy * vy;
    float det = ux * vy - uy * vx;
    return atan2f(det, dot);
}

/* Append one point to the shared shape-point pool; flags overflow if full. */
static bool rc_svg__push_pt(RcSvgParse *ps, RcSvgPoint p) {
    if (ps->ptCount >= RC_SVG__MAX_SHAPE_POINTS) {
        if (!ps->overflow) {
            rc_svg__warn(ps->icon, "shape-point budget exhausted; geometry truncated");
        }
        ps->overflow = true;
        return false;
    }
    ps->pts[ps->ptCount].x = p.x;
    ps->pts[ps->ptCount].y = p.y;
    ps->ptCount++;
    return true;
}

/* Flatten one SVG elliptical arc into points EXCLUDING start, appending them to
   the shape-point pool. Endpoint->centre parametrisation, radii correction,
   arcDeg cap, >=4 steps, last point snapped to the exact end. */
static void rc_svg__arc(RcSvgParse *ps, RcSvgPoint start, float rx, float ry,
                        float rotDeg, bool largeArc, bool sweep, RcSvgPoint end,
                        float maxSegDeg) {
    if (rc_svg__nearly_same(start, end)) {
        return;
    }
    /* Load-bearing: every arc input must be finite before the maths below runs.
       A radius of 1e30 makes rx*rx overflow to +inf, inf-inf is NaN, and
       converting that NaN with (int)ceilf is undefined behaviour. */
    if (!isfinite(end.x) || !isfinite(end.y)) {
        rc_svg__warn(ps->icon, "path arc has a non-finite endpoint - segment skipped");
        return;   /* deliberately does NOT push `end`: that is the poison itself */
    }
    if (!isfinite(start.x) || !isfinite(start.y) ||
        !isfinite(rx) || !isfinite(ry) || !isfinite(rotDeg)) {
        /* SVG's own degenerate-arc rule is "draw a straight line to the
           endpoint" (it says so for a zero radius); a non-finite parameter is
           outside the spec entirely, so borrowing that rule keeps the subpath
           connected instead of silently losing a segment. */
        rc_svg__warn(ps->icon, "path arc has a non-finite radius or start - drawn as a line");
        rc_svg__push_pt(ps, end);
        return;
    }
    rx = fabsf(rx);
    ry = fabsf(ry);
    if (rx == 0.0f || ry == 0.0f) {
        rc_svg__push_pt(ps, end);
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
        rc_svg__push_pt(ps, end);
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

    float theta1 = rc_svg__angle_between(1.0f, 0.0f, v1x, v1y);
    float delta = rc_svg__angle_between(v1x, v1y, v2x, v2y);

    if (!sweep && delta > 0.0f) {
        delta -= 2.0f * (float)M_PI;
    } else if (sweep && delta < 0.0f) {
        delta += 2.0f * (float)M_PI;
    }

    /* Finite inputs are not enough - the intermediate maths can still overflow
       (finite radii with a huge start point do it). This guard is what makes the
       (int) conversion below provably safe. */
    if (!isfinite(cx) || !isfinite(cy) || !isfinite(theta1) || !isfinite(delta)) {
        rc_svg__warn(ps->icon, "path arc geometry overflowed - drawn as a line to its endpoint");
        rc_svg__push_pt(ps, end);   /* `end` was proven finite at entry */
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
        rc_svg__push_pt(ps, p);
    }
}

/* Colour parsing. */

static int rc_svg__hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

static bool rc_svg__hex2(const char *s, int *out) {
    int hi = rc_svg__hexval(s[0]);
    int lo = rc_svg__hexval(s[1]);
    if (hi < 0 || lo < 0) {
        return false;
    }
    *out = hi * 16 + lo;
    return true;
}

typedef struct { const char *name; unsigned char r, g, b, a; } RcSvgNamedColor;

/* Named-colour table. */
static const RcSvgNamedColor rc_svg__named[] = {
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
static int rc_svg__lower_trim(const char *v, int len, char *buf, int cap) {
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
static RcSvgPaint rc_svg__parse_color(const char *v, int len) {
    RcSvgPaint p;
    p.kind = RC_PAINT_NONE;
    p.color = RC_LIT(RC_Color){ 0, 0, 0, 0 };
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
    rc_svg__lower_trim(v, len, low, (int)sizeof(low));
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
            int r1 = rc_svg__hexval(h[0]), g1 = rc_svg__hexval(h[1]), b1 = rc_svg__hexval(h[2]);
            if (r1 < 0 || g1 < 0 || b1 < 0) return p;
            r = r1 * 16 + r1; g = g1 * 16 + g1; bl = b1 * 16 + b1; al = 255;
        } else if (hn == 4) {
            int r1 = rc_svg__hexval(h[0]), g1 = rc_svg__hexval(h[1]);
            int b1 = rc_svg__hexval(h[2]), a1 = rc_svg__hexval(h[3]);
            if (r1 < 0 || g1 < 0 || b1 < 0 || a1 < 0) return p;
            r = r1 * 16 + r1; g = g1 * 16 + g1; bl = b1 * 16 + b1; al = a1 * 16 + a1;
        } else if (hn == 6) {
            if (!rc_svg__hex2(h, &r) || !rc_svg__hex2(h + 2, &g) || !rc_svg__hex2(h + 4, &bl)) return p;
            al = 255;
        } else if (hn == 8) {
            if (!rc_svg__hex2(h, &r) || !rc_svg__hex2(h + 2, &g) ||
                !rc_svg__hex2(h + 4, &bl) || !rc_svg__hex2(h + 6, &al)) return p;
        } else {
            return p;
        }
        p.kind = RC_PAINT_BAKED;
        p.color = RC_LIT(RC_Color){ (float)r, (float)g, (float)bl, (float)al };
        return p;
    }

    for (size_t i = 0; i < sizeof(rc_svg__named) / sizeof(rc_svg__named[0]); i++) {
        if (strcmp(low, rc_svg__named[i].name) == 0) {
            p.kind = RC_PAINT_BAKED;
            p.color = RC_LIT(RC_Color){ (float)rc_svg__named[i].r, (float)rc_svg__named[i].g,
                                    (float)rc_svg__named[i].b, (float)rc_svg__named[i].a };
            return p;
        }
    }
    return p;   /* unrecognised: no paint */
}

/* Minimal XML tokeniser: attribute reads only, enough for icon SVGs. */

typedef struct {
    char  name[32];
    const char *val;         /* points into the source buffer (not NUL-terminated) */
    int   valLen;
} RcSvgAttr;

typedef struct {
    char  tag[32];
    RcSvgAttr attrs[RC_SVG__MAX_ATTRS];
    int   attrCount;
    bool  selfClose;
    bool  isClose;           /* </tag> */
} RcSvgElem;

/* Read an attribute value's raw text by name (no CSS cascade). Returns NULL if
   the attribute is absent. */
static const char *rc_svg__attr(const RcSvgElem *e, const char *name, int *lenOut) {
    for (int i = 0; i < e->attrCount; i++) {
        if (strcmp(e->attrs[i].name, name) == 0) {
            if (lenOut) *lenOut = e->attrs[i].valLen;
            return e->attrs[i].val;
        }
    }
    return NULL;
}

/* Parse a float attribute: the first number in the value, else the default. */
static float rc_svg__attr_number(const RcSvgElem *e, const char *name, float def) {
    int len = 0;
    const char *v = rc_svg__attr(e, name, &len);
    if (!v) {
        return def;
    }
    /* Search for the first float anywhere in the value (matches FLOAT_RE.search). */
    int i = 0;
    while (i < len) {
        int save = i;
        float out;
        if (rc_svg__scan_float(v, len, &i, &out)) {
            return out;
        }
        i = save + 1;
    }
    return def;
}

/* Read a CSS property honouring the cascade: inline style="" beats the
   presentation attribute. Writes the value slice to valOut/lenOut; returns
   true if present (in either place). */
static bool rc_svg__css_property(const RcSvgElem *e, const char *name,
                                 const char **valOut, int *lenOut) {
    int styleLen = 0;
    const char *style = rc_svg__attr(e, "style", &styleLen);
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
    const char *pres = rc_svg__attr(e, name, lenOut);
    if (pres) {
        *valOut = pres;
        return true;
    }
    return false;
}

/* Which viewport dimension a percentage resolves against. SVG gives each
   attribute its own reference rather than one shared basis: x-family lengths
   against the width, y-family against the height, and a radius or a stroke
   width against the normalised diagonal, because neither belongs to one axis. */
typedef enum {
    RC_SVG_REF_W = 0,
    RC_SVG_REF_H,
    RC_SVG_REF_DIAG
} RcSvgRef;

static float rc_svg__ref_length(const RcSvgIcon *icon, RcSvgRef ref) {
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
/* One static instance so rc_svg__warn de-dups it by address: a file with twenty
   `pt` strokes says this once. */
static const char *const rc_svg__unit_suffix_msg =
    "a length carries a unit suffix this converter does not convert "
    "(pt pc mm cm in em ex rem); the bare number was kept as user units - "
    "write the converted number instead";

/* True when v[i..] is exactly one of the SVG 1.1 unit identifiers this converter
   does not convert, ASCII case-folded because CSS reads units without regard to
   case (`13pt`, `2EM`, `1.5rem`). The identifier must END there: `13ptx` is not
   a unit and stays silent, and so do `px` (harmless: 1 px IS 1 user unit) and a
   lone letter. No ctype call - a locale must not decide what a unit is. */
static bool rc_svg__unit_suffix_unconverted(const char *v, int len, int i) {
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

static float rc_svg__resolve_length(const char *v, int len, RcSvgIcon *icon,
                                    RcSvgRef ref, float def) {
    int i = 0;

    while (i < len) {
        int save = i;
        float out;
        if (rc_svg__scan_float(v, len, &i, &out)) {
            while (i < len && (v[i] == ' ' || v[i] == '\t')) {
                i++;
            }
            if (i < len && v[i] == '%') {
                out = out / 100.0f * rc_svg__ref_length(icon, ref);
            } else if (rc_svg__unit_suffix_unconverted(v, len, i)) {
                rc_svg__warn(icon, rc_svg__unit_suffix_msg);
            }
            return out;
        }
        i = save + 1;
    }
    return def;
}

static float rc_svg__attr_length(const RcSvgParse *ps, const RcSvgElem *e,
                                 const char *name, float def, RcSvgRef ref) {
    int len = 0;
    const char *v = rc_svg__attr(e, name, &len);

    if (!v) {
        return def;
    }
    return rc_svg__resolve_length(v, len, ps ? ps->icon : NULL, ref, def);
}

static float rc_svg__stroke_width(const RcSvgElem *e, float inherited,
                                  RcSvgIcon *icon) {
    const char *v; int len;
    if (!rc_svg__css_property(e, "stroke-width", &v, &len)) {
        return inherited;
    }
    return rc_svg__resolve_length(v, len, icon, RC_SVG_REF_DIAG, inherited);
}

/* Resolve fill/stroke honouring the cascade + inheritance; root default = NONE. */
static RcSvgPaint rc_svg__read_paint(const RcSvgElem *e, const char *attr,
                                     RcSvgPaint inherited, RcSvgIcon *icon) {
    const char *v; int len;
    if (!rc_svg__css_property(e, attr, &v, &len)) {
        return inherited;   /* not specified -> inherit */
    }
    RcSvgPaint p = rc_svg__parse_color(v, len);
    if (p.kind == RC_PAINT_NONE) {
        /* Warn on an explicit-but-unsupported colour (rgb()/hsl()/unknown name)
           so the author sees why the paint vanished. */
        char low[16];
        rc_svg__lower_trim(v, len, low, (int)sizeof(low));
        if (low[0] != '\0' && strcmp(low, "none") != 0) {
            if (strcmp(attr, "fill") == 0) {
                rc_svg__warn(icon, "unrecognised fill colour; shape paint dropped (use hex #rrggbb or a named colour)");
            } else {
                rc_svg__warn(icon, "unrecognised stroke colour; shape paint dropped (use hex #rrggbb or a named colour)");
            }
        }
    }
    return p;
}

/* Element scanning. */

static void rc_svg__strip_ns(const char *raw, int len, char *out, int cap) {
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

/* Parse the tag + attributes of a single '<...>' element token starting at s[*i]
   (which must point just past '<'). Advances *i past the closing '>'. Returns
   false only on a hard scan failure (unterminated tag). */
static bool rc_svg__scan_element(const char *s, int len, int *i, RcSvgElem *e) {
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
    rc_svg__strip_ns(s + nameStart, p - nameStart, e->tag, (int)sizeof(e->tag));

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
        rc_svg__strip_ns(s + aStart, aLen, aname, (int)sizeof(aname));

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
        if (aname[0] != '\0' && e->attrCount < RC_SVG__MAX_ATTRS) {
            RcSvgAttr *at = &e->attrs[e->attrCount++];
            rc_svg__str_copy(at->name, aname, sizeof(at->name));
            at->val = val;
            at->valLen = vlen;
        }
    }
    return false;   /* unterminated */
}

/* viewBox / points-attr / path-data. */

static void rc_svg__parse_viewbox(const RcSvgElem *root, float *vx, float *vy,
                                  float *vw, float *vh) {
    int len = 0;
    const char *raw = rc_svg__attr(root, "viewBox", &len);
    if (!raw) {
        raw = rc_svg__attr(root, "viewbox", &len);
    }
    if (raw) {
        float vals[4];
        int n = 0, i = 0;
        while (i < len && n < 4) {
            rc_svg__skip_sep(raw, len, &i);
            if (i >= len) {
                break;
            }
            float out;
            int save = i;
            if (rc_svg__scan_float(raw, len, &i, &out)) {
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
    float width = rc_svg__attr_number(root, "width", 24.0f);
    float height = rc_svg__attr_number(root, "height", width);
    *vx = 0.0f; *vy = 0.0f; *vw = width; *vh = height;
}

/* Push a raw point (no flatten) onto the shape-point pool. */
static bool rc_svg__push_xy(RcSvgParse *ps, float x, float y) {
    RcSvgPoint p = { x, y };
    return rc_svg__push_pt(ps, p);
}

/* Parse a <polyline>/<polygon> "points" attribute into the shape-point pool.
   Returns the count pushed; a trailing odd coordinate is dropped. */
static int rc_svg__parse_points_attr(RcSvgParse *ps, const char *v, int len, int *offOut) {
    float nums[2];
    int have = 0;
    int start = ps->ptCount;
    *offOut = start;
    int i = 0;
    int pushed = 0;
    while (i < len) {
        rc_svg__skip_sep(v, len, &i);
        if (i >= len) {
            break;
        }
        float out;
        int save = i;
        if (!rc_svg__scan_float(v, len, &i, &out)) {
            i = save + 1;
            continue;
        }
        nums[have++] = out;
        if (have == 2) {
            if (!rc_svg__push_xy(ps, nums[0], nums[1])) {
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
static void rc_svg__parse_path_data(RcSvgParse *ps, const char *d, int len,
                                    float strokeWidth, RcSvgPaint fill, RcSvgPaint stroke);

/* Emit a PATH shape for a completed subpath [start, ps->ptCount). Applies the
   flush_subpath rules: <2 points -> drop; closed + duplicate endpoint -> drop it. */
static void rc_svg__flush_subpath(RcSvgParse *ps, int start, bool closed,
                                  float strokeWidth, RcSvgPaint fill, RcSvgPaint stroke) {
    int count = ps->ptCount - start;
    if (count < 2) {
        ps->ptCount = start;   /* discard a degenerate subpath's points */
        return;
    }
    if (closed && count >= 2 &&
        rc_svg__nearly_same(ps->pts[start], ps->pts[start + count - 1])) {
        ps->ptCount--;   /* drop the duplicate closing endpoint */
        count--;
    }
    if (ps->shapeCount >= RC_SVG__MAX_SHAPES) {
        if (!ps->overflow) {
            rc_svg__warn(ps->icon, "shape budget exhausted; remaining shapes skipped");
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
}

static void rc_svg__parse_path_data(RcSvgParse *ps, const char *d, int len,
                                    float strokeWidth, RcSvgPaint fill, RcSvgPaint stroke) {
    int i = 0;
    char command = 0;
    char prevCommand = 0;
    RcSvgPoint current = { 0.0f, 0.0f };
    RcSvgPoint startPt = { 0.0f, 0.0f };
    int subStart = ps->ptCount;
    bool haveSub = false;
    RcSvgPoint lastCubic = { 0.0f, 0.0f }; bool haveCubic = false;
    RcSvgPoint lastQuad = { 0.0f, 0.0f };  bool haveQuad = false;

    /* Local: read the next float token, honouring separators. Returns false at a
       command boundary or end of input. */
    #define RC_SVG_READ_FLOAT(dst) \
        do { \
            rc_svg__skip_sep(d, len, &i); \
            if (i >= len || rc_svg__is_cmd_char(d[i])) { goto malformed; } \
            int _save = i; \
            if (!rc_svg__scan_float(d, len, &i, &(dst))) { i = _save; goto malformed; } \
        } while (0)

    /* Local: read an arc flag. A flag is one character, so there is no index to
       restore on failure, and no separate command-boundary test is needed - a
       command character is never '0' or '1'. */
    #define RC_SVG_READ_FLAG(dst) \
        do { \
            rc_svg__skip_sep(d, len, &i); \
            if (!rc_svg__scan_flag(d, len, &i, &(dst))) { goto malformed; } \
        } while (0)

    for (;;) {
        rc_svg__skip_sep(d, len, &i);
        if (i >= len) {
            break;
        }
        if (rc_svg__is_cmd_char(d[i])) {
            command = d[i];
            i++;
        } else if (command == 0) {
            rc_svg__warn(ps->icon, "path data starts with numbers before a command; skipped remainder");
            break;
        }

        char upper = (char)toupper((unsigned char)command);
        bool relative = islower((unsigned char)command) != 0;

        if (upper == 'Z') {
            if (haveSub) {
                rc_svg__flush_subpath(ps, subStart, true, strokeWidth, fill, stroke);
            }
            subStart = ps->ptCount;
            haveSub = false;
            current = startPt;
            haveCubic = false;
            haveQuad = false;
            prevCommand = command;
            command = 0;
            continue;
        }

        if (upper != 'M' && upper != 'L' && upper != 'H' && upper != 'V' &&
            upper != 'C' && upper != 'S' && upper != 'Q' && upper != 'T' && upper != 'A') {
            rc_svg__warn(ps->icon, "unsupported path command; skipped");
            break;
        }

        bool firstMoveto = (upper == 'M');
        char curUpper = upper;   /* mutated: M's repeated pairs become L */

        while (i < len) {
            rc_svg__skip_sep(d, len, &i);
            if (i >= len || rc_svg__is_cmd_char(d[i])) {
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
                        rc_svg__flush_subpath(ps, subStart, false, strokeWidth, fill, stroke);
                    }
                    subStart = ps->ptCount;
                    if (!rc_svg__push_pt(ps, pt)) { goto done; }
                    haveSub = true;
                    current = pt;
                    startPt = pt;
                    firstMoveto = false;
                    curUpper = 'L';   /* implicit L for repeated coords */
                } else {
                    if (!rc_svg__push_pt(ps, pt)) { goto done; }
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
                if (!rc_svg__push_pt(ps, end)) { goto done; }
                current = end;
                haveCubic = false; haveQuad = false;
            } else if (curUpper == 'H') {
                float x;
                RC_SVG_READ_FLOAT(x);
                RcSvgPoint end = relative ? RC_LIT(RcSvgPoint){ current.x + x, current.y }
                                          : RC_LIT(RcSvgPoint){ x, current.y };
                if (!rc_svg__push_pt(ps, end)) { goto done; }
                current = end;
                haveCubic = false; haveQuad = false;
            } else if (curUpper == 'V') {
                float y;
                RC_SVG_READ_FLOAT(y);
                RcSvgPoint end = relative ? RC_LIT(RcSvgPoint){ current.x, current.y + y }
                                          : RC_LIT(RcSvgPoint){ current.x, y };
                if (!rc_svg__push_pt(ps, end)) { goto done; }
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
                    RcSvgPoint pp = rc_svg__cubic(current, c1, c2, end, (float)step / (float)ps->curveSteps);
                    if (!rc_svg__push_pt(ps, pp)) { goto done; }
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
                    RcSvgPoint pp = rc_svg__cubic(current, c1, c2, end, (float)step / (float)ps->curveSteps);
                    if (!rc_svg__push_pt(ps, pp)) { goto done; }
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
                    RcSvgPoint pp = rc_svg__quad(current, c, end, (float)step / (float)ps->curveSteps);
                    if (!rc_svg__push_pt(ps, pp)) { goto done; }
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
                    RcSvgPoint pp = rc_svg__quad(current, c, end, (float)step / (float)ps->curveSteps);
                    if (!rc_svg__push_pt(ps, pp)) { goto done; }
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
                rc_svg__arc(ps, current, rx, ry, rot, la, sw, end, ps->arcDeg);
                current = end;
                haveCubic = false; haveQuad = false;
            }
            prevCommand = command;
            continue;

        malformed:
            rc_svg__warn(ps->icon, "malformed path; skipped incomplete tail");
            /* Load-bearing: skip to the next command character. The float scan
               RESTORES the index when it fails, so without this the outer loop
               re-enters on the same byte forever - any byte that cannot start a
               number does it, including the lead byte of a UTF-8 character.
               Skipping rather than abandoning the path keeps the valid geometry
               that follows the bad token. */
            while (i < len && !rc_svg__is_cmd_char(d[i])) {
                i++;
            }
            goto after_params;
        }

    after_params:
        /* After an explicit M, subsequent repeated pairs are L (same relativity).
           (curUpper already flipped to L; keep `command` as M for prevCommand,
           then normalise the token so a following bare number continues as L.) */
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
        rc_svg__flush_subpath(ps, subStart, false, strokeWidth, fill, stroke);
    }

    #undef RC_SVG_READ_FLOAT
    #undef RC_SVG_READ_FLAG
}

/* Shape collection: recursive, bounded depth. */

static void rc_svg__add_shape(RcSvgParse *ps, RcSvgShape sh) {
    if (ps->shapeCount >= RC_SVG__MAX_SHAPES) {
        if (!ps->overflow) {
            rc_svg__warn(ps->icon, "shape budget exhausted; remaining shapes skipped");
        }
        ps->overflow = true;
        return;
    }
    ps->shapes[ps->shapeCount++] = sh;
}

/* Recursively collect drawable shapes under `s[*i]`, having just consumed the
   opening tag of `parent`. Iterates sibling elements until the matching close
   tag (or EOF). `depth` bounds recursion. */
static void rc_svg__collect(RcSvgParse *ps, const char *s, int len, int *i,
                            const RcSvgElem *parent, float inhStrokeW,
                            RcSvgPaint inhFill, RcSvgPaint inhStroke, int depth);

/* The ATTRIBUTE half of the unsupported-feature report, and it exists because an
   unsupported ELEMENT disappears, which an author notices, while an unsupported
   ATTRIBUTE changes the picture in SILENCE. A <g transform> draws its whole
   subtree at raw coordinates, an opacity draws fully opaque, a clip-path draws
   the clipped shape whole. Each of those is a plausible-looking icon that is
   wrong, and nothing else in this file would say so.

   The lookup is exact-name strcmp, NOT substring, and that is load-bearing:
   gradientTransform and patternTransform are real SVG attributes on families
   that already report on their own, so a substring test would fire on them.

   stroke-linecap and stroke-linejoin are ignored too and deliberately do NOT
   report: they appear on most ordinary artwork, and a diagnostic that fires on
   healthy input teaches people to stop reading the channel.

   rc_svg__warn de-dups on the static string pointer, so each message here is
   reported once per document however many elements carry the attribute. */
static void rc_svg__report_ignored_attrs(RcSvgIcon *icon, const RcSvgElem *e) {
    static const char *const transform_msg =
        "transform is ignored - shapes draw at their raw coordinates; bake it into the coordinates";
    static const char *const opacity_msg =
        "opacity is ignored - shapes draw fully opaque; carry the alpha in an #rrggbbaa fill instead";
    static const char *const clip_msg =
        "clip-path is ignored - the clipped shape draws whole; resolve the clip into explicit geometry";
    static const char *const hidden_msg =
        "display/visibility are ignored - a hidden element still draws; delete it instead";
    static const char *const fillrule_msg =
        "fill-rule/clip-rule are ignored - an evenodd hole fills solid";
    static const char *const dash_msg =
        "stroke-dasharray is ignored - the stroke draws solid";
    static const struct {
        const char *name;
        const char *const *msg;
    } table[] = {
        { "transform",        &transform_msg },
        { "clip-path",        &clip_msg      },
        { "opacity",          &opacity_msg   },
        { "fill-opacity",     &opacity_msg   },
        { "stroke-opacity",   &opacity_msg   },
        { "display",          &hidden_msg    },
        { "visibility",       &hidden_msg    },
        { "fill-rule",        &fillrule_msg  },
        { "clip-rule",        &fillrule_msg  },
        { "stroke-dasharray", &dash_msg      },
    };
    int i;

    for (i = 0; i < (int)(sizeof table / sizeof table[0]); i++) {
        if (rc_svg__attr(e, table[i].name, NULL)) {
            rc_svg__warn(icon, *table[i].msg);
        }
    }
}

/* Handle one element: read its paints, emit its own shape(s), recurse for
   containers. `e` is already scanned; `s[*i]` sits just past its '>'. */
static void rc_svg__handle_element(RcSvgParse *ps, const char *s, int len, int *i,
                                   const RcSvgElem *e, float inhStrokeW,
                                   RcSvgPaint inhFill, RcSvgPaint inhStroke, int depth) {
    float strokeWidth = rc_svg__stroke_width(e, inhStrokeW, ps->icon);
    RcSvgPaint fill = rc_svg__read_paint(e, "fill", inhFill, ps->icon);
    RcSvgPaint stroke = rc_svg__read_paint(e, "stroke", inhStroke, ps->icon);

    rc_svg__report_ignored_attrs(ps->icon, e);

    const char *tag = e->tag;

    if (strcmp(tag, "path") == 0) {
        int dLen = 0;
        const char *d = rc_svg__attr(e, "d", &dLen);
        if (d && dLen > 0) {
            rc_svg__parse_path_data(ps, d, dLen, strokeWidth, fill, stroke);
        }
    } else if (strcmp(tag, "line") == 0) {
        RcSvgShape sh; memset(&sh, 0, sizeof(sh));
        sh.kind = RC_SHAPE_PATH;
        int off = ps->ptCount;
        bool ok1 = rc_svg__push_xy(ps, rc_svg__attr_length(ps, e, "x1", 0.0f, RC_SVG_REF_W),
                                 rc_svg__attr_length(ps, e, "y1", 0.0f, RC_SVG_REF_H));
        bool ok2 = rc_svg__push_xy(ps, rc_svg__attr_length(ps, e, "x2", 0.0f, RC_SVG_REF_W),
                                 rc_svg__attr_length(ps, e, "y2", 0.0f, RC_SVG_REF_H));
        if (ok1 && ok2) {
            sh.ptOff = off; sh.ptCount = 2; sh.closed = false;
            sh.strokeWidth = strokeWidth; sh.fill = fill; sh.stroke = stroke;
            rc_svg__add_shape(ps, sh);
        }
    } else if (strcmp(tag, "polyline") == 0 || strcmp(tag, "polygon") == 0) {
        int pLen = 0;
        const char *pts = rc_svg__attr(e, "points", &pLen);
        if (pts && pLen > 0) {
            int off = 0;
            int n = rc_svg__parse_points_attr(ps, pts, pLen, &off);
            if (n >= 2) {
                RcSvgShape sh; memset(&sh, 0, sizeof(sh));
                sh.kind = RC_SHAPE_PATH;
                sh.ptOff = off; sh.ptCount = n;
                sh.closed = (strcmp(tag, "polygon") == 0);
                sh.strokeWidth = strokeWidth; sh.fill = fill; sh.stroke = stroke;
                rc_svg__add_shape(ps, sh);
            } else {
                ps->ptCount = off;   /* roll back a too-short points list */
            }
        }
    } else if (strcmp(tag, "rect") == 0) {
        float x = rc_svg__attr_length(ps, e, "x", 0.0f, RC_SVG_REF_W);
        float y = rc_svg__attr_length(ps, e, "y", 0.0f, RC_SVG_REF_H);
        float w = rc_svg__attr_length(ps, e, "width", 0.0f, RC_SVG_REF_W);
        float h = rc_svg__attr_length(ps, e, "height", 0.0f, RC_SVG_REF_H);
        float rx = rc_svg__attr_length(ps, e, "rx", 0.0f, RC_SVG_REF_W);
        float ry = rc_svg__attr_length(ps, e, "ry", rx, RC_SVG_REF_H);
        if (w > 0.0f && h > 0.0f) {
            RcSvgShape sh; memset(&sh, 0, sizeof(sh));
            sh.kind = RC_SHAPE_RECT;
            sh.x = x; sh.y = y; sh.w = w; sh.h = h; sh.rx = rx; sh.ry = ry;
            sh.strokeWidth = strokeWidth; sh.fill = fill; sh.stroke = stroke;
            rc_svg__add_shape(ps, sh);
        }
    } else if (strcmp(tag, "circle") == 0) {
        float cx = rc_svg__attr_length(ps, e, "cx", 0.0f, RC_SVG_REF_W);
        float cy = rc_svg__attr_length(ps, e, "cy", 0.0f, RC_SVG_REF_H);
        float r = rc_svg__attr_length(ps, e, "r", 0.0f, RC_SVG_REF_DIAG);
        if (r > 0.0f) {
            RcSvgShape sh; memset(&sh, 0, sizeof(sh));
            sh.kind = RC_SHAPE_CIRCLE;
            sh.cx = cx; sh.cy = cy; sh.r = r;
            sh.strokeWidth = strokeWidth; sh.fill = fill; sh.stroke = stroke;
            rc_svg__add_shape(ps, sh);
        }
    } else if (strcmp(tag, "ellipse") == 0) {
        float cx = rc_svg__attr_length(ps, e, "cx", 0.0f, RC_SVG_REF_W);
        float cy = rc_svg__attr_length(ps, e, "cy", 0.0f, RC_SVG_REF_H);
        float rx = rc_svg__attr_length(ps, e, "rx", 0.0f, RC_SVG_REF_W);
        float ry = rc_svg__attr_length(ps, e, "ry", 0.0f, RC_SVG_REF_H);
        if (rx > 0.0f && ry > 0.0f) {
            RcSvgShape sh; memset(&sh, 0, sizeof(sh));
            sh.kind = RC_SHAPE_ELLIPSE;
            sh.cx = cx; sh.cy = cy; sh.rx = rx; sh.ry = ry;
            sh.strokeWidth = strokeWidth; sh.fill = fill; sh.stroke = stroke;
            rc_svg__add_shape(ps, sh);
        }
    } else if (strcmp(tag, "svg") == 0 || strcmp(tag, "g") == 0 ||
               strcmp(tag, "defs") == 0 || strcmp(tag, "title") == 0 ||
               strcmp(tag, "desc") == 0 || strcmp(tag, "style") == 0) {
        /* Pass-through container: shapes come from its children. */
    } else {
        rc_svg__warn(ps->icon, "unsupported SVG element skipped");
    }

    /* Recurse into children (containers and, harmlessly, any element that has
       nested content). Self-closing tags have no children. */
    if (!e->selfClose) {
        rc_svg__collect(ps, s, len, i, e, strokeWidth, fill, stroke, depth + 1);
    }
}

static void rc_svg__collect(RcSvgParse *ps, const char *s, int len, int *i,
                            const RcSvgElem *parent, float inhStrokeW,
                            RcSvgPaint inhFill, RcSvgPaint inhStroke, int depth) {
    (void)parent;
    if (depth > RC_SVG__MAX_DEPTH) {
        rc_svg__warn(ps->icon, "element nesting too deep; subtree skipped");
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
                int end = rc_svg__comment_end(s, len, *i);
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
        if (!rc_svg__scan_element(s, len, i, &e)) {
            /* Unterminated tag: bail out of this subtree gracefully. */
            *i = len;
            return;
        }
        if (e.isClose) {
            return;   /* matching close of our parent (or a stray close) */
        }
        rc_svg__handle_element(ps, s, len, i, &e, inhStrokeW, inhFill, inhStroke, depth);
    }
}

/* Op building. */

/* 64-sample circle/ellipse ring used for non-circular ellipses, appended to
   icon->points. Returns the point offset. */
static int rc_svg__emit_circle_ring(RcSvgIcon *out, float cx, float cy,
                                    float rx, float ry, int count) {
    int off = out->pointCount;
    for (int k = 0; k < count; k++) {
        if (out->pointCount >= RC_SVG_MAX_POINTS) {
            rc_svg__warn(out, "icon point budget exhausted; geometry truncated");
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
static int rc_svg__copy_points(RcSvgIcon *out, const RcSvgParse *ps,
                               int srcOff, int srcCount, int *countOut) {
    int off = out->pointCount;
    int n = 0;
    for (int k = 0; k < srcCount; k++) {
        if (out->pointCount >= RC_SVG_MAX_POINTS) {
            rc_svg__warn(out, "icon point budget exhausted; geometry truncated");
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

static bool rc_svg__push_op(RcSvgIcon *out, RcSvgOp op) {
    if (out->opCount >= RC_SVG_MAX_OPS) {
        rc_svg__warn(out, "op budget exhausted; remaining ops skipped");
        return false;
    }
    /* Every emitted path passes through here, and only point-based ops (POLYLINE /
       FILLED_POLY) carry a non-zero pointCount - so this one check catches a path
       the runtime would silently truncate, on every code path, before it is baked
       into a header. Warn (do not split): splitting a fill would seam its
       triangulation and splitting a stroke would gap it; the honest fix is a
       simpler SVG. */
    if (op.pointCount > RC_SVG__RUNTIME_MAX_PTS) {
        rc_svg__warn(out, "a path exceeds the runtime point cap; its tail "
                          "vertices will be dropped when drawn - simplify the SVG");
    }
    out->ops[out->opCount++] = op;
    return true;
}

static void rc_svg__rect_corners(RcSvgIcon *out, const RcSvgShape *sh, int *offOut, int *countOut) {
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
            rc_svg__warn(out, "icon point budget exhausted; geometry truncated");
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

static bool rc_svg__paint_is_baked(RcSvgPaint p) {
    return p.kind == RC_PAINT_BAKED;
}

static bool rc_svg__paint_is_paint(RcSvgPaint p) {
    return p.kind == RC_PAINT_BAKED || p.kind == RC_PAINT_CURRENT;
}

/* A currentColor paint in a coloured icon bakes as black. */
static RC_Color rc_svg__baked_color(RcSvgPaint p) {
    if (p.kind == RC_PAINT_BAKED) {
        return p.color;
    }
    return RC_LIT(RC_Color){ 0, 0, 0, 255 };   /* currentColor -> black */
}

/* Coloured-emit shape grouping. Each SOURCE SHAPE is separated by a blank line,
   but a shape can produce two ops (fill THEN stroke) that must NOT be split.
   RcSvgOp/RcSvgIcon are a fixed shared contract, so the fill/stroke->shape
   boundary is recorded here instead: index i is true iff op i is the FIRST op of
   a new source shape. Only the coloured path uses it. */
static bool rc_svg__op_shape_start[RC_SVG_MAX_OPS];

/* True for the op kinds that actually draw an edge. The shared `stroke` constant
   is computed over these alone: a fill-only shape carries a strokeWidth no op
   ever reads, and counting it breaks the constant for a value nothing uses. */
static bool rc_svg__op_strokes(RcSvgOpKind k) {
    return k == RC_SVG_ROUND_LINE || k == RC_SVG_POLYLINE
        || k == RC_SVG_CIRCLE_STROKE || k == RC_SVG_RRECT_STROKE;
}

/* MONO op building, baked=false. Mirrors generate_header's plan.
   FILL AND STROKE ARE INDEPENDENT, and a shape carrying both emits both, fill
   first - painter's order, matching the coloured builder. Reading only the
   stroke turns a solid disc into a ring of its own outline, which at icon sizes
   is a sub-pixel smear rather than a shape: RayClay's own monochrome logo came
   out with its four filled circles drawn as 2.0-unit rings in a 731-unit
   viewBox, 0.06 device pixels at a 22 px titlebar.
   On this path a paint can only be the currentColor sentinel - a shape baking a
   concrete colour routes to rc_svg__build_colored instead - so a fill draws with
   the caller's `color` and the (float size, RC_Color color) signature holds.
   A shape with NEITHER paint still strokes, which is the long-standing default
   for artwork that relies on a stylesheet this parser does not resolve. */
static void rc_svg__build_mono(RcSvgIcon *out, const RcSvgParse *ps) {
    for (int si = 0; si < ps->shapeCount; si++) {
        const RcSvgShape *sh = &ps->shapes[si];
        const bool fills   = sh->fill.kind   != RC_PAINT_NONE;
        const bool strokes = sh->stroke.kind != RC_PAINT_NONE || !fills;
        RcSvgOp op;

        if (sh->kind == RC_SHAPE_RECT) {
            if (fills) {
                int off, count;
                if (sh->rx > 0.0f) {
                    rc_svg__warn(out, "filled rect corner radius ignored (no filled rounded-rect helper)");
                }
                rc_svg__rect_corners(out, sh, &off, &count);
                memset(&op, 0, sizeof(op));
                op.kind = RC_SVG_FILLED_POLY;
                op.pointOff = off; op.pointCount = count;
                rc_svg__push_op(out, op);
            }
            if (strokes) {
                memset(&op, 0, sizeof(op));
                if (sh->rx > 0.0f) {
                    if (sh->ry > 0.0f && fabsf(sh->rx - sh->ry) > 1e-6f) {
                        rc_svg__warn(out, "rect rx != ry; using rx for the rounded stroke");
                    }
                    op.kind = RC_SVG_RRECT_STROKE;
                    op.p[0] = sh->x; op.p[1] = sh->y; op.p[2] = sh->w; op.p[3] = sh->h; op.p[4] = sh->rx;
                    op.stroke = sh->strokeWidth;
                    rc_svg__push_op(out, op);
                } else {
                    int off, count;
                    rc_svg__rect_corners(out, sh, &off, &count);
                    op.kind = RC_SVG_POLYLINE;
                    op.pointOff = off; op.pointCount = count;
                    op.closed = true; op.stroke = sh->strokeWidth;
                    rc_svg__push_op(out, op);
                }
            }
        } else if (sh->kind == RC_SHAPE_CIRCLE) {
            if (fills) {
                memset(&op, 0, sizeof(op));
                op.kind = RC_SVG_FILLED_CIRCLE;
                op.p[0] = sh->cx; op.p[1] = sh->cy; op.p[2] = sh->r;
                rc_svg__push_op(out, op);
            }
            if (strokes) {
                memset(&op, 0, sizeof(op));
                op.kind = RC_SVG_CIRCLE_STROKE;
                op.p[0] = sh->cx; op.p[1] = sh->cy; op.p[2] = sh->r;
                op.stroke = sh->strokeWidth;
                rc_svg__push_op(out, op);
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
                rc_svg__push_op(out, op);
            }
            if (strokes) {
                memset(&op, 0, sizeof(op));
                if (roundEnough) {
                    op.kind = RC_SVG_CIRCLE_STROKE;
                    op.p[0] = sh->cx; op.p[1] = sh->cy; op.p[2] = sh->rx;
                    op.stroke = sh->strokeWidth;
                    rc_svg__push_op(out, op);
                } else {
                    rc_svg__warn(out, "non-circular ellipse flattened to a polyline (no ellipse helper)");
                    int off = rc_svg__emit_circle_ring(out, sh->cx, sh->cy, sh->rx, sh->ry, 64);
                    op.kind = RC_SVG_POLYLINE;
                    op.pointOff = off; op.pointCount = out->pointCount - off;
                    op.closed = true; op.stroke = sh->strokeWidth;
                    rc_svg__push_op(out, op);
                }
            }
        } else { /* RC_SHAPE_PATH */
            if (fills) {
                int count;
                int off = rc_svg__copy_points(out, ps, sh->ptOff, sh->ptCount, &count);
                memset(&op, 0, sizeof(op));
                op.kind = RC_SVG_FILLED_POLY;
                op.pointOff = off; op.pointCount = count;
                rc_svg__push_op(out, op);
            }
            if (strokes) {
                memset(&op, 0, sizeof(op));
                if (sh->ptCount == 2 && !sh->closed) {
                    op.kind = RC_SVG_ROUND_LINE;
                    op.p[0] = ps->pts[sh->ptOff].x; op.p[1] = ps->pts[sh->ptOff].y;
                    op.p[2] = ps->pts[sh->ptOff + 1].x; op.p[3] = ps->pts[sh->ptOff + 1].y;
                    op.stroke = sh->strokeWidth;
                    rc_svg__push_op(out, op);
                } else {
                    int count;
                    int off = rc_svg__copy_points(out, ps, sh->ptOff, sh->ptCount, &count);
                    op.kind = RC_SVG_POLYLINE;
                    op.pointOff = off; op.pointCount = count;
                    op.closed = sh->closed; op.stroke = sh->strokeWidth;
                    rc_svg__push_op(out, op);
                }
            }
        }
    }
}

/* COLOURED op building: FILL first then STROKE per shape, baked=true. */
static void rc_svg__build_colored(RcSvgIcon *out, RcSvgParse *ps) {
    bool usesColor = false;
    memset(rc_svg__op_shape_start, 0, sizeof(rc_svg__op_shape_start));

    for (int si = 0; si < ps->shapeCount; si++) {
        const RcSvgShape *sh = &ps->shapes[si];
        bool hasFill = rc_svg__paint_is_paint(sh->fill);
        bool hasStroke = rc_svg__paint_is_paint(sh->stroke);
        if (!hasFill && !hasStroke) {
            continue;
        }
        int shapeFirstOp = out->opCount;   /* first op this shape emits */

        /* ---- FILL (drawn first / underneath) ---- */
        if (hasFill) {
            RcSvgPaint fp = sh->fill;
            /* currentColor stays runtime (baked=false -> emits the `color` arg,
               draw() uses monoColor); its .color is the documented {0,0,0,255}
               fallback. An explicit colour is baked. */
            bool fbaked = (fp.kind == RC_PAINT_BAKED);
            if (fp.kind == RC_PAINT_CURRENT) usesColor = true;
            RC_Color fc = rc_svg__baked_color(fp);
            if (sh->kind == RC_SHAPE_CIRCLE) {
                RcSvgOp op; memset(&op, 0, sizeof(op));
                op.kind = RC_SVG_FILLED_CIRCLE; op.baked = fbaked; op.color = fc;
                op.p[0] = sh->cx; op.p[1] = sh->cy; op.p[2] = sh->r;
                rc_svg__push_op(out, op);
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
                rc_svg__push_op(out, op);
            } else if (sh->kind == RC_SHAPE_RECT) {
                if (sh->rx > 0.0f) {
                    rc_svg__warn(out, "filled rounded rect drawn square-cornered (no filled rounded-rect helper)");
                }
                int off, count;
                rc_svg__rect_corners(out, sh, &off, &count);
                RcSvgOp op; memset(&op, 0, sizeof(op));
                op.kind = RC_SVG_FILLED_POLY; op.baked = fbaked; op.color = fc;
                op.pointOff = off; op.pointCount = count;
                rc_svg__push_op(out, op);
            } else { /* PATH */
                if (sh->ptCount >= 3) {
                    int count;
                    int off = rc_svg__copy_points(out, ps, sh->ptOff, sh->ptCount, &count);
                    RcSvgOp op; memset(&op, 0, sizeof(op));
                    op.kind = RC_SVG_FILLED_POLY; op.baked = fbaked; op.color = fc;
                    op.pointOff = off; op.pointCount = count;
                    rc_svg__push_op(out, op);
                } else if (!hasStroke) {
                    rc_svg__warn(out, "degenerate filled path (<3 points) skipped");
                }
            }
        }

        /* ---- STROKE (drawn over the fill) ---- */
        if (hasStroke && sh->strokeWidth > 0.0f) {
            RcSvgPaint spnt = sh->stroke;
            bool sbaked = (spnt.kind == RC_PAINT_BAKED);
            if (spnt.kind == RC_PAINT_CURRENT) usesColor = true;
            RC_Color sc = rc_svg__baked_color(spnt);
            if (sh->kind == RC_SHAPE_CIRCLE) {
                RcSvgOp op; memset(&op, 0, sizeof(op));
                op.kind = RC_SVG_CIRCLE_STROKE; op.baked = sbaked; op.color = sc;
                op.p[0] = sh->cx; op.p[1] = sh->cy; op.p[2] = sh->r;
                op.stroke = sh->strokeWidth;
                rc_svg__push_op(out, op);
            } else if (sh->kind == RC_SHAPE_ELLIPSE) {
                if (fabsf(sh->rx - sh->ry) <= 1e-6f) {
                    RcSvgOp op; memset(&op, 0, sizeof(op));
                    op.kind = RC_SVG_CIRCLE_STROKE; op.baked = sbaked; op.color = sc;
                    op.p[0] = sh->cx; op.p[1] = sh->cy; op.p[2] = sh->rx;
                    op.stroke = sh->strokeWidth;
                    rc_svg__push_op(out, op);
                } else {
                    int off = rc_svg__emit_circle_ring(out, sh->cx, sh->cy, sh->rx, sh->ry, 64);
                    RcSvgOp op; memset(&op, 0, sizeof(op));
                    op.kind = RC_SVG_POLYLINE; op.baked = sbaked; op.color = sc;
                    op.pointOff = off; op.pointCount = out->pointCount - off;
                    op.closed = true; op.stroke = sh->strokeWidth;
                    rc_svg__push_op(out, op);
                }
            } else if (sh->kind == RC_SHAPE_RECT) {
                if (sh->rx > 0.0f) {
                    RcSvgOp op; memset(&op, 0, sizeof(op));
                    op.kind = RC_SVG_RRECT_STROKE; op.baked = sbaked; op.color = sc;
                    op.p[0] = sh->x; op.p[1] = sh->y; op.p[2] = sh->w; op.p[3] = sh->h; op.p[4] = sh->rx;
                    op.stroke = sh->strokeWidth;
                    rc_svg__push_op(out, op);
                } else {
                    int off, count;
                    rc_svg__rect_corners(out, sh, &off, &count);
                    RcSvgOp op; memset(&op, 0, sizeof(op));
                    op.kind = RC_SVG_POLYLINE; op.baked = sbaked; op.color = sc;
                    op.pointOff = off; op.pointCount = count;
                    op.closed = true; op.stroke = sh->strokeWidth;
                    rc_svg__push_op(out, op);
                }
            } else { /* PATH */
                if (sh->ptCount == 2 && !sh->closed) {
                    RcSvgOp op; memset(&op, 0, sizeof(op));
                    op.kind = RC_SVG_ROUND_LINE; op.baked = sbaked; op.color = sc;
                    op.p[0] = ps->pts[sh->ptOff].x; op.p[1] = ps->pts[sh->ptOff].y;
                    op.p[2] = ps->pts[sh->ptOff + 1].x; op.p[3] = ps->pts[sh->ptOff + 1].y;
                    op.stroke = sh->strokeWidth;
                    rc_svg__push_op(out, op);
                } else {
                    int count;
                    int off = rc_svg__copy_points(out, ps, sh->ptOff, sh->ptCount, &count);
                    RcSvgOp op; memset(&op, 0, sizeof(op));
                    op.kind = RC_SVG_POLYLINE; op.baked = sbaked; op.color = sc;
                    op.pointOff = off; op.pointCount = count;
                    op.closed = sh->closed; op.stroke = sh->strokeWidth;
                    rc_svg__push_op(out, op);
                }
            }
        }

        /* If this shape emitted at least one op, its first op begins a new group
           (the emitter inserts a blank line before each group after the first). */
        if (out->opCount > shapeFirstOp && shapeFirstOp < RC_SVG_MAX_OPS) {
            rc_svg__op_shape_start[shapeFirstOp] = true;
        }
    }

    if (usesColor) {
        rc_svg__warn(out, "currentColor in a multi-colour icon renders as black (the colour argument is dropped); use an explicit colour");
    }
}

/* Public: parse. `svg` is read as exactly `len` bytes and does NOT need a NUL
   terminator - every scan in this file is length-bounded, so an mmap'd slice or
   a socket buffer is a legitimate input. */

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
                int end = rc_svg__comment_end(svg, len, i);
                if (end < 0) { i = len; break; }
                i = end;
            } else {
                while (i < len && svg[i] != '>') i++;
                if (i < len) i++;
            }
            continue;
        }
        i++;   /* skip '<' */
        if (!rc_svg__scan_element(svg, len, &i, &root)) {
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
    rc_svg__parse_viewbox(&root, &vx, &vy, &vw, &vh);
    out->viewW = vw;
    out->viewH = vh;

    float rootStrokeW = rc_svg__stroke_width(&root, 2.0f, out);
    RcSvgPaint none = { RC_PAINT_NONE, { 0, 0, 0, 0 } };
    RcSvgPaint rootFill = rc_svg__read_paint(&root, "fill", none, out);
    RcSvgPaint rootStroke = rc_svg__read_paint(&root, "stroke", none, out);

    /* The root <svg>/<g>/... is itself a container: its own shapes (rare) plus
       its children. Handle the root element (which may itself be a shape if the
       file's outermost tag is e.g. a lone <path>, though normally it's <svg>). */
    const char *tag = root.tag;
    bool rootIsContainer = (strcmp(tag, "svg") == 0 || strcmp(tag, "g") == 0 ||
                            strcmp(tag, "defs") == 0 || strcmp(tag, "title") == 0 ||
                            strcmp(tag, "desc") == 0 || strcmp(tag, "style") == 0);
    /* The ROOT's own ignored attributes. rc_svg__handle_element reports these for
       every other element, and the container path below never goes through it, so
       without this a root-level transform is silent. That is the likeliest place
       for one to be: scaling the whole artwork by putting a transform on the outer
       <svg> is what a design tool exports, which makes the root the case this
       reporting most needs to cover rather than the one it can skip. */
    if (rootIsContainer) {
        rc_svg__report_ignored_attrs(out, &root);
    }

    if (!rootIsContainer) {
        /* Treat the root itself as a drawable element (still recurse for kids). */
        rc_svg__handle_element(&ps, svg, len, &i, &root, rootStrokeW, rootFill, rootStroke, 0);
    } else if (!root.selfClose) {
        rc_svg__collect(&ps, svg, len, &i, &root, rootStrokeW, rootFill, rootStroke, 1);
    }

    /* Normalise: translate every shape so the viewBox origin is 0,0. */
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
        rc_svg__warn(out, "non-square viewBox; generated icon uses max dimension for scaling");
    }

    /* colored? -> any shape bakes a concrete fill OR stroke colour. */
    bool colored = false;
    for (int s = 0; s < ps.shapeCount; s++) {
        if (rc_svg__paint_is_baked(ps.shapes[s].fill) ||
            rc_svg__paint_is_baked(ps.shapes[s].stroke)) {
            colored = true;
            break;
        }
    }
    out->colored = colored;

    if (colored) {
        rc_svg__build_colored(out, &ps);
    } else {
        rc_svg__build_mono(out, &ps);
    }

    if (ps.shapeCount == 0) {
        rc_svg__warn(out, "no supported drawable stroke shapes found");
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
            rcIconDrawFilledPolygon(bounds, pts, op->pointCount, viewBox, color);
            break;
        case RC_SVG_FILLED_CIRCLE:
            rcIconDrawFilledCircle(bounds, op->p[0], op->p[1], op->p[2], viewBox, color);
            break;
        case RC_SVG_FILLED_ELLIPSE:
            rcIconDrawFilledEllipse(bounds, op->p[0], op->p[1], op->p[2], op->p[3],
                                      viewBox, color);
            break;
        }
    }
}
#endif /* RC_SVG2ICON_PREVIEW */

/* Emit helpers: float / name formatting. */

/* Render a float: round to `precision`, strip trailing zeros/dot, map -0 -> 0,
   always end "N.0f". */
static void rc_svg__fmt_float(float value, int precision, char *buf, int cap) {
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

static void rc_svg__w_str(RcSvgWriter *w, const char *s) {
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

static void rc_svg__w_float(RcSvgWriter *w, float value, int precision) {
    char b[48];
    rc_svg__fmt_float(value, precision, b, (int)sizeof(b));
    rc_svg__w_str(w, b);
}

static void rc_svg__w_int(RcSvgWriter *w, int value) {
    char b[16];
    snprintf(b, sizeof(b), "%d", value);
    rc_svg__w_str(w, b);
}

/* PascalCase a name: split on non-alnum, uppercase each part's first letter,
   preserve the rest. Prefix "Icon" if it would start with a digit / be empty. */
static void rc_svg__pascal(const char *name, char *out, int cap) {
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
        rc_svg__str_copy(out, tmp, (size_t)cap);
    }
}

/* Split into words across separators, camel/Pascal humps, and digit boundaries,
   join with '_', uppercase, then wrap as RC_ICON_<...>_H. */
static void rc_svg__guard(const char *name, char *out, int cap) {
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
        snprintf(tmp, sizeof(tmp), "ICON_%s", words);
        strncpy(words, tmp, sizeof(words) - 1);
        words[sizeof(words) - 1] = '\0';
    }
    snprintf(out, (size_t)cap, "RC_ICON_%s_H", words);
}

/* Render a viewBox dimension for the doc comment. */
static void rc_svg__fmt_viewbox_dim(float value, char *buf, int cap) {
    snprintf(buf, (size_t)cap, "%g", (double)value);
}

/* Emit: doc comment. */

static void rc_svg__emit_doc_comment(RcSvgWriter *w, const char *svgName,
                                     float vw, float vh) {
    const int DOC_WRAP_WIDTH = 81;
    char wtag[32], htag[32];
    rc_svg__fmt_viewbox_dim(vw, wtag, (int)sizeof(wtag));
    rc_svg__fmt_viewbox_dim(vh, htag, (int)sizeof(htag));

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

    /* Tokenise prose on spaces. */
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

    rc_svg__w_str(w, "/*\n");
    rc_svg__w_str(w, first);
    rc_svg__w_str(w, "\n");
    /* Only when a word did not fit: prose short enough for one line must not
       trail an indent-only second line into the reader's own source tree. */
    if (taken < wordCount) {
        rc_svg__w_str(w, second);
        rc_svg__w_str(w, "\n");
    }
    rc_svg__w_str(w, "*/\n");
}

/* Emit: point array, shared by mono + coloured. */

static void rc_svg__emit_point_array(RcSvgWriter *w, const RcSvgIcon *icon,
                                     const RcSvgOp *op, int index, int precision) {
    char head[64];
    snprintf(head, sizeof(head), "    static const RC_IconPoint path%d[] = {\n", index);
    rc_svg__w_str(w, head);
    const RC_IconPoint *pts = icon->points + op->pointOff;
    for (int k = 0; k < op->pointCount; k++) {
        rc_svg__w_str(w, "        { ");
        rc_svg__w_float(w, pts[k].x, precision);
        rc_svg__w_str(w, ", ");
        rc_svg__w_float(w, pts[k].y, precision);
        rc_svg__w_str(w, " }");
        if (k + 1 < op->pointCount) {
            rc_svg__w_str(w, ",");
        }
        rc_svg__w_str(w, "\n");
    }
    rc_svg__w_str(w, "    };\n");
}

/* Continuation padding: N spaces (to align wrapped call arguments). */
static void rc_svg__w_pad(RcSvgWriter *w, int n) {
    for (int k = 0; k < n && !w->overflow; k++) {
        rc_svg__w_str(w, " ");
    }
}

/* Public: the function name rc_svg2icon_emit will give an icon generated under
   `name`, so a caller can show it before anything is written. Same transform. */
static inline void rc_svg2icon_symbol(const char *name, char *out, int cap) {
    char pascal[128];
    if (!out || cap <= 0) {
        return;
    }
    rc_svg__pascal(name ? name : "Icon", pascal, (int)sizeof(pascal));
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
    rc_svg__pascal(pascalName ? pascalName : "Icon", pascal, (int)sizeof(pascal));
    char guard[160];
    rc_svg__guard(pascalName ? pascalName : "Icon", guard, (int)sizeof(guard));

    float viewBoxSize = icon->viewW > icon->viewH ? icon->viewW : icon->viewH;

    char drawFn[160];
    snprintf(drawFn, sizeof(drawFn), "rcDrawIcon%s", pascal);
    int sigCont = (int)strlen("static inline void ") + (int)strlen(drawFn) + 1; /* '(' */

    /* Header preamble. */
    rc_svg__w_str(&w, "#ifndef ");
    rc_svg__w_str(&w, guard);
    rc_svg__w_str(&w, "\n#define ");
    rc_svg__w_str(&w, guard);
    rc_svg__w_str(&w, "\n\n#include \"rc_icons_common.h\"\n\n");

    rc_svg__emit_doc_comment(&w, svgName ? svgName : "icon.svg",
                             icon->viewW, icon->viewH);

    /* Draw-fn signature. */
    rc_svg__w_str(&w, "static inline void ");
    rc_svg__w_str(&w, drawFn);
    rc_svg__w_str(&w, "(RC_BoundingBox bounds,\n");
    rc_svg__w_pad(&w, sigCont);
    rc_svg__w_str(&w, "RC_Color color,\n");
    rc_svg__w_pad(&w, sigCont);
    rc_svg__w_str(&w, "const void *userData) {\n");
    rc_svg__w_str(&w, "    (void)userData;\n");

    if (icon->colored) {
        /* ---- Coloured form (rcIcon(size)) ---- */
        bool usesColor = false;
        for (int oi = 0; oi < icon->opCount; oi++) {
            if (!icon->ops[oi].baked) {   /* a slipped-in currentColor op */
                usesColor = true;
                break;
            }
        }
        if (!usesColor) {
            rc_svg__w_str(&w, "    (void)color;\n");
        }
        rc_svg__w_str(&w, "    const float viewBox = ");
        rc_svg__w_float(&w, viewBoxSize, precision);
        rc_svg__w_str(&w, ";\n\n");

        int polyIndex = 0;
        int colIndex = 0;

        for (int oi = 0; oi < icon->opCount; oi++) {
            const RcSvgOp *op = &icon->ops[oi];

            /* Each SOURCE SHAPE is separated by a blank line; a shape's fill +
               stroke ops stay together. rc_svg__op_shape_start[oi] marks the
               first op of each shape. */
            if (oi > 0 && rc_svg__op_shape_start[oi]) {
                rc_svg__w_str(&w, "\n");
            }

            /* Emit any point array first. */
            bool hasArray = (op->kind == RC_SVG_POLYLINE || op->kind == RC_SVG_FILLED_POLY);
            char arr[16];
            if (hasArray) {
                snprintf(arr, sizeof(arr), "path%d", polyIndex);
                rc_svg__emit_point_array(&w, icon, op, polyIndex, precision);
                polyIndex++;
            }

            /* Colour expression. */
            char cexpr[32];
            if (op->baked) {
                snprintf(cexpr, sizeof(cexpr), "c%d", colIndex);
                rc_svg__w_str(&w, "    const RC_Color ");
                rc_svg__w_str(&w, cexpr);
                rc_svg__w_str(&w, " = { ");
                rc_svg__w_int(&w, (int)op->color.r);
                rc_svg__w_str(&w, ", ");
                rc_svg__w_int(&w, (int)op->color.g);
                rc_svg__w_str(&w, ", ");
                rc_svg__w_int(&w, (int)op->color.b);
                rc_svg__w_str(&w, ", ");
                rc_svg__w_int(&w, (int)op->color.a);
                rc_svg__w_str(&w, " };\n");
                colIndex++;
            } else {
                snprintf(cexpr, sizeof(cexpr), "color");
            }

            switch (op->kind) {
            case RC_SVG_FILLED_POLY:
                rc_svg__w_str(&w, "    rcIconDrawFilledPolygon(bounds, ");
                rc_svg__w_str(&w, arr);
                rc_svg__w_str(&w, ",\n");
                rc_svg__w_pad(&w, (int)strlen("    rcIconDrawFilledPolygon("));
                rc_svg__w_str(&w, "(int)(sizeof(");
                rc_svg__w_str(&w, arr);
                rc_svg__w_str(&w, ") / sizeof(");
                rc_svg__w_str(&w, arr);
                rc_svg__w_str(&w, "[0])),\n");
                rc_svg__w_pad(&w, (int)strlen("    rcIconDrawFilledPolygon("));
                rc_svg__w_str(&w, "viewBox, ");
                rc_svg__w_str(&w, cexpr);
                rc_svg__w_str(&w, ");\n");
                break;
            case RC_SVG_POLYLINE:
                rc_svg__w_str(&w, "    rcIconDrawPolyline(bounds, ");
                rc_svg__w_str(&w, arr);
                rc_svg__w_str(&w, ",\n");
                rc_svg__w_pad(&w, (int)strlen("    rcIconDrawPolyline("));
                rc_svg__w_str(&w, "(int)(sizeof(");
                rc_svg__w_str(&w, arr);
                rc_svg__w_str(&w, ") / sizeof(");
                rc_svg__w_str(&w, arr);
                rc_svg__w_str(&w, "[0])),\n");
                rc_svg__w_pad(&w, (int)strlen("    rcIconDrawPolyline("));
                rc_svg__w_str(&w, "viewBox, ");
                rc_svg__w_float(&w, op->stroke, precision);
                rc_svg__w_str(&w, op->closed ? ", true, " : ", false, ");
                rc_svg__w_str(&w, cexpr);
                rc_svg__w_str(&w, ");\n");
                break;
            case RC_SVG_FILLED_CIRCLE:
                rc_svg__w_str(&w, "    rcIconDrawFilledCircle(bounds, ");
                rc_svg__w_float(&w, op->p[0], precision);
                rc_svg__w_str(&w, ", ");
                rc_svg__w_float(&w, op->p[1], precision);
                rc_svg__w_str(&w, ", ");
                rc_svg__w_float(&w, op->p[2], precision);
                rc_svg__w_str(&w, ", viewBox, ");
                rc_svg__w_str(&w, cexpr);
                rc_svg__w_str(&w, ");\n");
                break;
            case RC_SVG_FILLED_ELLIPSE:
                rc_svg__w_str(&w, "    rcIconDrawFilledEllipse(bounds, ");
                rc_svg__w_float(&w, op->p[0], precision);
                rc_svg__w_str(&w, ", ");
                rc_svg__w_float(&w, op->p[1], precision);
                rc_svg__w_str(&w, ", ");
                rc_svg__w_float(&w, op->p[2], precision);
                rc_svg__w_str(&w, ", ");
                rc_svg__w_float(&w, op->p[3], precision);
                rc_svg__w_str(&w, ", viewBox, ");
                rc_svg__w_str(&w, cexpr);
                rc_svg__w_str(&w, ");\n");
                break;
            case RC_SVG_CIRCLE_STROKE:
                rc_svg__w_str(&w, "    rcIconDrawCircleStroke(bounds, ");
                rc_svg__w_float(&w, op->p[0], precision);
                rc_svg__w_str(&w, ", ");
                rc_svg__w_float(&w, op->p[1], precision);
                rc_svg__w_str(&w, ", ");
                rc_svg__w_float(&w, op->p[2], precision);
                rc_svg__w_str(&w, ", viewBox, ");
                rc_svg__w_float(&w, op->stroke, precision);
                rc_svg__w_str(&w, ", ");
                rc_svg__w_str(&w, cexpr);
                rc_svg__w_str(&w, ");\n");
                break;
            case RC_SVG_RRECT_STROKE:
                rc_svg__w_str(&w, "    rcIconDrawRoundedRectStroke(bounds, ");
                rc_svg__w_float(&w, op->p[0], precision);
                rc_svg__w_str(&w, ", ");
                rc_svg__w_float(&w, op->p[1], precision);
                rc_svg__w_str(&w, ", ");
                rc_svg__w_float(&w, op->p[2], precision);
                rc_svg__w_str(&w, ", ");
                rc_svg__w_float(&w, op->p[3], precision);
                rc_svg__w_str(&w, ",\n");
                rc_svg__w_pad(&w, (int)strlen("    rcIconDrawRoundedRectStroke("));
                rc_svg__w_float(&w, op->p[4], precision);
                rc_svg__w_str(&w, ", viewBox, ");
                rc_svg__w_float(&w, op->stroke, precision);
                rc_svg__w_str(&w, ", ");
                rc_svg__w_str(&w, cexpr);
                rc_svg__w_str(&w, ");\n");
                break;
            case RC_SVG_ROUND_LINE:
                rc_svg__w_str(&w, "    rcIconDrawRoundLine(bounds, ");
                rc_svg__w_float(&w, op->p[0], precision);
                rc_svg__w_str(&w, ", ");
                rc_svg__w_float(&w, op->p[1], precision);
                rc_svg__w_str(&w, ", ");
                rc_svg__w_float(&w, op->p[2], precision);
                rc_svg__w_str(&w, ", ");
                rc_svg__w_float(&w, op->p[3], precision);
                rc_svg__w_str(&w, ", viewBox, ");
                rc_svg__w_float(&w, op->stroke, precision);
                rc_svg__w_str(&w, ", ");
                rc_svg__w_str(&w, cexpr);
                rc_svg__w_str(&w, ");\n");
                break;
            }
        }

        rc_svg__w_str(&w, "}\n\n");
        rc_svg__w_str(&w, "static inline void rcIcon");
        rc_svg__w_str(&w, pascal);
        rc_svg__w_str(&w, "(float size) {\n");
        rc_svg__w_str(&w, "    RC_Color baked = { 0, 0, 0, 255 };   /* unused; the artwork bakes its own colours */\n");
        rc_svg__w_str(&w, "    rcIconEmit(size, baked, ");
        rc_svg__w_str(&w, drawFn);
        rc_svg__w_str(&w, ", NULL);\n}\n\n");
    } else {
        /* ---- Mono form (rcIcon(size, color)) ---- */
        if (icon->opCount == 0) {
            rc_svg__w_str(&w, "    (void)bounds; (void)color;\n");
        } else {
            /* Uniform stroke? All STROKING op widths equal, rounded to
               `precision`. A fill op carries no width, so including one would
               compare a zero against real widths and drop the shared constant
               for a value no emitted call ever reads. */
            bool uniform = false;
            int  firstStroke = -1;
            char firstW[48];
            for (int oi = 0; oi < icon->opCount; oi++) {
                if (!rc_svg__op_strokes(icon->ops[oi].kind)) {
                    continue;
                }
                if (firstStroke < 0) {
                    firstStroke = oi;
                    uniform = true;
                    rc_svg__fmt_float(icon->ops[oi].stroke, precision, firstW, (int)sizeof(firstW));
                    continue;
                }
                {
                    char thisW[48];
                    rc_svg__fmt_float(icon->ops[oi].stroke, precision, thisW, (int)sizeof(thisW));
                    if (strcmp(firstW, thisW) != 0) {
                        uniform = false;
                        break;
                    }
                }
            }

            /* Aligned decl block: "viewBox" is the widest name (7 chars). */
            rc_svg__w_str(&w, "    const float viewBox = ");
            rc_svg__w_float(&w, viewBoxSize, precision);
            rc_svg__w_str(&w, ";\n");
            if (uniform) {
                rc_svg__w_str(&w, "    const float stroke  = ");
                rc_svg__w_float(&w, icon->ops[firstStroke].stroke, precision);
                rc_svg__w_str(&w, ";\n");
            }
            rc_svg__w_str(&w, "\n");

            /* Op emission with the polyline blank-line rule. */
            int polyIndex = 0;
            int prevKind = -1;
            for (int oi = 0; oi < icon->opCount; oi++) {
                const RcSvgOp *op = &icon->ops[oi];
                bool isBlock = (op->kind == RC_SVG_POLYLINE || op->kind == RC_SVG_FILLED_POLY);
                bool prevBlock = (prevKind == RC_SVG_POLYLINE || prevKind == RC_SVG_FILLED_POLY);
                if (oi > 0 && (isBlock || prevBlock)) {
                    rc_svg__w_str(&w, "\n");
                }

                switch (op->kind) {
                case RC_SVG_ROUND_LINE:
                    rc_svg__w_str(&w, "    rcIconDrawRoundLine(bounds, ");
                    rc_svg__w_float(&w, op->p[0], precision);
                    rc_svg__w_str(&w, ", ");
                    rc_svg__w_float(&w, op->p[1], precision);
                    rc_svg__w_str(&w, ", ");
                    rc_svg__w_float(&w, op->p[2], precision);
                    rc_svg__w_str(&w, ", ");
                    rc_svg__w_float(&w, op->p[3], precision);
                    rc_svg__w_str(&w, ", viewBox, ");
                    if (uniform) {
                        rc_svg__w_str(&w, "stroke");
                    } else {
                        rc_svg__w_float(&w, op->stroke, precision);
                    }
                    rc_svg__w_str(&w, ", color);\n");
                    break;
                case RC_SVG_CIRCLE_STROKE:
                    rc_svg__w_str(&w, "    rcIconDrawCircleStroke(bounds, ");
                    rc_svg__w_float(&w, op->p[0], precision);
                    rc_svg__w_str(&w, ", ");
                    rc_svg__w_float(&w, op->p[1], precision);
                    rc_svg__w_str(&w, ", ");
                    rc_svg__w_float(&w, op->p[2], precision);
                    rc_svg__w_str(&w, ", viewBox, ");
                    if (uniform) {
                        rc_svg__w_str(&w, "stroke");
                    } else {
                        rc_svg__w_float(&w, op->stroke, precision);
                    }
                    rc_svg__w_str(&w, ", color);\n");
                    break;
                case RC_SVG_RRECT_STROKE:
                    rc_svg__w_str(&w, "    rcIconDrawRoundedRectStroke(bounds, ");
                    rc_svg__w_float(&w, op->p[0], precision);
                    rc_svg__w_str(&w, ", ");
                    rc_svg__w_float(&w, op->p[1], precision);
                    rc_svg__w_str(&w, ", ");
                    rc_svg__w_float(&w, op->p[2], precision);
                    rc_svg__w_str(&w, ", ");
                    rc_svg__w_float(&w, op->p[3], precision);
                    rc_svg__w_str(&w, ",\n");
                    rc_svg__w_pad(&w, (int)strlen("    rcIconDrawRoundedRectStroke("));
                    rc_svg__w_float(&w, op->p[4], precision);
                    rc_svg__w_str(&w, ", viewBox, ");
                    if (uniform) {
                        rc_svg__w_str(&w, "stroke");
                    } else {
                        rc_svg__w_float(&w, op->stroke, precision);
                    }
                    rc_svg__w_str(&w, ", color);\n");
                    break;
                case RC_SVG_POLYLINE: {
                    char arr[16];
                    snprintf(arr, sizeof(arr), "path%d", polyIndex);
                    rc_svg__emit_point_array(&w, icon, op, polyIndex, precision);
                    polyIndex++;
                    rc_svg__w_str(&w, "    rcIconDrawPolyline(bounds, ");
                    rc_svg__w_str(&w, arr);
                    rc_svg__w_str(&w, ",\n");
                    rc_svg__w_pad(&w, (int)strlen("    rcIconDrawPolyline("));
                    rc_svg__w_str(&w, "(int)(sizeof(");
                    rc_svg__w_str(&w, arr);
                    rc_svg__w_str(&w, ") / sizeof(");
                    rc_svg__w_str(&w, arr);
                    rc_svg__w_str(&w, "[0])),\n");
                    rc_svg__w_pad(&w, (int)strlen("    rcIconDrawPolyline("));
                    rc_svg__w_str(&w, "viewBox, ");
                    if (uniform) {
                        rc_svg__w_str(&w, "stroke");
                    } else {
                        rc_svg__w_float(&w, op->stroke, precision);
                    }
                    rc_svg__w_str(&w, op->closed ? ", true, color);\n" : ", false, color);\n");
                    break;
                }
                case RC_SVG_FILLED_CIRCLE:
                    rc_svg__w_str(&w, "    rcIconDrawFilledCircle(bounds, ");
                    rc_svg__w_float(&w, op->p[0], precision);
                    rc_svg__w_str(&w, ", ");
                    rc_svg__w_float(&w, op->p[1], precision);
                    rc_svg__w_str(&w, ", ");
                    rc_svg__w_float(&w, op->p[2], precision);
                    rc_svg__w_str(&w, ", viewBox, color);\n");
                    break;
                case RC_SVG_FILLED_ELLIPSE:
                    rc_svg__w_str(&w, "    rcIconDrawFilledEllipse(bounds, ");
                    rc_svg__w_float(&w, op->p[0], precision);
                    rc_svg__w_str(&w, ", ");
                    rc_svg__w_float(&w, op->p[1], precision);
                    rc_svg__w_str(&w, ", ");
                    rc_svg__w_float(&w, op->p[2], precision);
                    rc_svg__w_str(&w, ", ");
                    rc_svg__w_float(&w, op->p[3], precision);
                    rc_svg__w_str(&w, ", viewBox, color);\n");
                    break;
                case RC_SVG_FILLED_POLY: {
                    char arr[16];
                    snprintf(arr, sizeof(arr), "path%d", polyIndex);
                    rc_svg__emit_point_array(&w, icon, op, polyIndex, precision);
                    polyIndex++;
                    rc_svg__w_str(&w, "    rcIconDrawFilledPolygon(bounds, ");
                    rc_svg__w_str(&w, arr);
                    rc_svg__w_str(&w, ",\n");
                    rc_svg__w_pad(&w, (int)strlen("    rcIconDrawFilledPolygon("));
                    rc_svg__w_str(&w, "(int)(sizeof(");
                    rc_svg__w_str(&w, arr);
                    rc_svg__w_str(&w, ") / sizeof(");
                    rc_svg__w_str(&w, arr);
                    rc_svg__w_str(&w, "[0])),\n");
                    rc_svg__w_pad(&w, (int)strlen("    rcIconDrawFilledPolygon("));
                    rc_svg__w_str(&w, "viewBox, color);\n");
                    break;
                }
                default:
                    break;
                }
                prevKind = (int)op->kind;
            }
        }

        rc_svg__w_str(&w, "}\n\n");
        rc_svg__w_str(&w, "static inline void rcIcon");
        rc_svg__w_str(&w, pascal);
        rc_svg__w_str(&w, "(float size, RC_Color color) {\n");
        rc_svg__w_str(&w, "    rcIconEmit(size, color, ");
        rc_svg__w_str(&w, drawFn);
        rc_svg__w_str(&w, ", NULL);\n}\n\n");
    }

    rc_svg__w_str(&w, "#endif /* ");
    rc_svg__w_str(&w, guard);
    rc_svg__w_str(&w, " */\n");

    if (w.overflow) {
        return -1;
    }
    return w.len;
}

#endif /* RC_SVG2ICON_H */
