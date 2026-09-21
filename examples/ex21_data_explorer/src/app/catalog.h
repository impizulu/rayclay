/* ============================================================================
 *  catalog.h - the dataset seam: swap this file, keep the whole app
 * ============================================================================
 *  Everything above reads an array of CatPlanet and knows nothing about where
 *  the rows came from. Replace catalog_init with a CSV reader, a SQLite query
 *  or an HTTP fetch and not one line of the UI changes.
 *
 *  The shipped catalogue is SYNTHETIC and deterministic, because examples ship
 *  no asset files and a browser tab has no filesystem: one seed gives every
 *  target byte-identical rows, so "the same picture everywhere" is checkable
 *  with a screenshot. The distributions are shaped after the real exoplanet
 *  record - the Kepler spikes, the radius valley near 1.8 Earth radii, hot
 *  Jupiters under ten days. The SHAPES are real; not one row is. Do not cite
 *  it.
 *
 *  No libc: every skewed distribution is drawn by walking a geometric ladder
 *  rather than calling pow or log. Fixed-size storage, no allocation - the
 *  catalogue is a plain value type the caller owns. Include rayclay.h first.
 * ========================================================================= */

#ifndef APP_CATALOG_H
#define APP_CATALOG_H

enum {
    CAT_COUNT      = 12000,  /* rows. One line to raise; see main.c's status bar */
    CAT_YEAR_FIRST = 1992,   /* 51 Pegasi b's era: the first confirmed worlds    */
    CAT_YEAR_LAST  = 2026,
    CAT_YEARS      = CAT_YEAR_LAST - CAT_YEAR_FIRST + 1
};

/** How the planet was found. Ordered by how many rows each accounts for, so a
    legend built by walking this enum reads largest-first. */
typedef enum CatMethod {
    CAT_TRANSIT = 0,
    CAT_RADIAL,
    CAT_MICROLENS,
    CAT_IMAGING,
    CAT_METHODS
} CatMethod;

/** One row. 24 bytes, so the catalogue is a 288 KiB static object: small
    enough to hold by value, large enough that scanning it every frame should be
    a choice you make on purpose. */
typedef struct CatPlanet {
    float    period;    /* orbital period, days                                */
    float    radius;    /* planet radius, Earth radii                          */
    float    mass;      /* planet mass, Earth masses                           */
    float    distance;  /* distance from Earth, parsecs                        */
    uint16_t year;      /* year of discovery                                   */
    uint16_t teq;       /* equilibrium temperature, kelvin                     */
    uint16_t host;      /* host star's number within its survey                */
    uint8_t  method;    /* CatMethod                                           */
    uint8_t  survey;    /* index into CAT_SURVEY                               */
} CatPlanet;

typedef struct Catalog {
    CatPlanet row[CAT_COUNT];
} Catalog;

static const char *const CAT_METHOD_NAME[CAT_METHODS] = {
    "Transit", "Radial velocity", "Microlensing", "Direct imaging"
};

/* Short forms for the table column, where "Radial velocity" does not fit. */
static const char *const CAT_METHOD_ABBR[CAT_METHODS] = {
    "Transit", "RV", "Lensing", "Imaging"
};

enum { CAT_SURVEYS = 9 };
static const char *const CAT_SURVEY[CAT_SURVEYS] = {
    "Kepler", "TOI", "K2", "WASP", "HAT-P", "HD", "GJ", "OGLE", "HR"
};

/* Planets are lettered from b in order of discovery around their star; the
   star itself is a. */
static const char *const CAT_PLANET_LETTER[4] = { "b", "c", "d", "e" };

/** The planet's designation, in parts, e.g. "Kepler" + 1842 + "b".

    Deliberately NOT assembled into a string here: RayClay keeps the pointer you
    hand it until the frame is drawn, so a name built into a local buffer is
    dangling by then - silently, as a blank column. The call site formats these
    three parts into the frame arena, which is the lifetime it needs. */
static const char *catalog_letter(const CatPlanet *p)
{
    return CAT_PLANET_LETTER[p->host & 3u];
}

/** xorshift32 (Marsaglia 2003). The requirement is REPRODUCIBILITY, not
    statistical quality: 32-bit shifts and xors are exactly defined, so every
    target draws the same catalogue. */
static uint32_t cat_rnd(uint32_t *s)
{
    uint32_t x = *s;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

/** Uniform in [0, 1). 24 bits is a float's mantissa; more would add bits the
    result cannot represent. */
static float cat_rnd01(uint32_t *s)
{
    return (float)(cat_rnd(s) >> 8) * (1.0f / 16777216.0f);
}

/** Uniform in [lo, hi). */
static float cat_range(uint32_t *s, float lo, float hi)
{
    return lo + (hi - lo) * cat_rnd01(s);
}

/** Uniform integer in [0, n). */
static int cat_below(uint32_t *s, int n)
{
    return (int)(cat_rnd(s) % (uint32_t)n);
}

/** Pick an index from `n` integer weights, proportionally. A cumulative walk,
    so each distribution stays readable as the literal list of counts it is. */
static int cat_pick(uint32_t *s, const uint16_t *weight, int n)
{
    uint32_t total = 0, r;
    int      i;

    for (i = 0; i < n; i++)
        total += weight[i];
    if (total == 0)
        return 0;

    r = cat_rnd(s) % total;
    for (i = 0; i < n - 1; i++) {
        if (r < weight[i])
            return i;
        r -= weight[i];
    }
    return n - 1;
}

/* Discoveries per year, shaped after the real record: a trickle through the
   1990s, the two Kepler releases, then a TESS-era plateau. Sampling weights,
   so only their RATIOS matter. */
static const uint16_t CAT_YEAR_WEIGHT[CAT_YEARS] = {
    2,   0,   1,   1,   6,   1,   5,  13,  16,  12,   /* 1992-2001 */
    31,  25,  25,  37,  30,  53,  60,  88, 106, 138,  /* 2002-2011 */
    138, 152, 875, 138,1500, 380, 300, 205, 400, 350, /* 2012-2021 */
    320, 300, 280, 260, 200                           /* 2022-2026 */
};

/* Method mix by era: which technique was finding planets when. Rows are
   <2000 / 2000-2008 / 2009-2017 / 2018+; columns follow CatMethod. */
static const uint16_t CAT_METHOD_ERA[4][CAT_METHODS] = {
    {  4, 90,  3,  3 },
    { 24, 70,  3,  3 },
    { 88,  9,  2,  1 },
    { 84, 12,  3,  1 }
};

/** Which size class a planet falls in. The valley is its own class so it can
    be given a low weight: the gap at ~1.8 Earth radii is the most recognisable
    feature of the radius histogram, and it is real. */
typedef enum CatSize {
    CAT_ROCKY = 0,
    CAT_VALLEY,
    CAT_SUB_NEPTUNE,
    CAT_NEPTUNE,
    CAT_GIANT,
    CAT_SIZES
} CatSize;

static const uint16_t CAT_SIZE_BY_METHOD[CAT_METHODS][CAT_SIZES] = {
    { 22,  4, 42, 18, 14 },   /* transit: sensitive to small close-in planets */
    {  4,  2, 14, 20, 60 },   /* radial velocity: mass-biased, so giants      */
    { 15,  5, 25, 25, 30 },   /* microlensing: the least biased of the four   */
    {  0,  0,  0,  0,100 }    /* imaging: only young, hot, wide-orbit giants  */
};

static const float CAT_SIZE_LO[CAT_SIZES] = { 0.4f, 1.6f, 2.0f, 4.0f,  8.0f };
static const float CAT_SIZE_HI[CAT_SIZES] = { 1.6f, 2.0f, 4.0f, 8.0f, 16.0f };

/** Mass from radius: a four-segment fit standing in for the literature's
    broken power law. The +/-18% scatter keeps the mass column from being a pure
    function of the radius column, which real measurements are not. */
static float cat_mass_for(uint32_t *s, float r)
{
    float m;

    if (r <= 1.6f)
        m = 0.9f * r * r * r;
    else if (r <= 4.0f)
        m = 3.7f + 5.5f * (r - 1.6f);
    else if (r <= 8.0f)
        m = 16.9f + 8.0f * (r - 4.0f);
    else
        m = 48.9f + 42.0f * (r - 8.0f);

    return m * cat_range(s, 0.82f, 1.18f);
}

/** Draw the orbit, and the temperature that follows from it. Both come off ONE
    geometric ladder, which is what lets this file do astrophysics with no libm:
    Teq goes as the inverse cube root of the period, so P *= 1.06 always pairs
    with Teq *= 1.06^(-1/3) = 0.98086. */
static void cat_draw_orbit(uint32_t *s, int steps, float *period, uint16_t *teq)
{
    /* Anchors at step 0: a 0.4-day orbit around a Sun-like star sits at about
       0.011 AU, which puts its dayside near 2700 K. */
    float p = 0.4f;
    float t = 2700.0f;
    int   k;

    for (k = 0; k < steps; k++) {
        p *= 1.06f;
        t *= 0.98086f;
    }

    /* Land ANYWHERE between this rung and the next, not on it. An integer step
       count quantises the result: ~130 planets share a period to the digit,
       which draws as vertical stripes in the scatter and a stack of identical
       cells in the table. One multiply and the artefact goes away. */
    p *= cat_range(s, 1.0f, 1.06f);

    /* Host stars are not all Sun-like, so the temperature spreads either side
       of the ladder even at a fixed period. */
    t *= cat_range(s, 0.78f, 1.26f);
    if (t < 30.0f)
        t = 30.0f;

    *period = p;
    *teq    = (uint16_t)t;
}

/** Uniform integer in [0, n), biased toward the FAR end: the larger of two
    draws. Stars within reach grow as the cube of the distance, so most
    detections sit near the survey's limit; a flat draw over a log-spaced ladder
    does the opposite and piles identical "6 pc" rows at the top of a distance
    sort. */
static int cat_below_far(uint32_t *s, int n)
{
    int a = cat_below(s, n);
    int b = cat_below(s, n);

    return a > b ? a : b;
}

/** How far out each method can see, as a step count on a 1.055 ladder from
    1.3 pc. Radial velocity and imaging need bright nearby stars; transit
    surveys stare a few hundred parsecs out; microlensing looks through the
    galactic bulge. */
static const int CAT_DIST_LO[CAT_METHODS] = { 60,  0,  120, 0  };
static const int CAT_DIST_HI[CAT_METHODS] = { 138, 78, 163, 68 };

/** Which surveys plausibly own a detection, by method and era. */
static int cat_survey_for(uint32_t *s, int method, int year)
{
    if (method == CAT_RADIAL)
        return cat_below(s, 2) ? 5 : 6;               /* HD | GJ              */
    if (method == CAT_MICROLENS)
        return 7;                                     /* OGLE                 */
    if (method == CAT_IMAGING)
        return 8;                                     /* HR                   */

    if (year >= 2018)
        return cat_below(s, 4) ? 1 : 3;               /* TOI | WASP           */
    if (year >= 2014)
        return cat_below(s, 3) ? 2 : 0;               /* K2 | Kepler          */
    if (year >= 2009)
        return cat_below(s, 5) ? 0 : 4;               /* Kepler | HAT-P       */
    return cat_below(s, 2) ? 3 : 4;                   /* WASP | HAT-P         */
}

/** Fill `c` with a reproducible catalogue. The same seed gives the same rows
    on every platform - see the file header for why that is load-bearing. */
static void catalog_init(Catalog *c, uint32_t seed)
{
    uint32_t s = seed ? seed : 1u;
    int      i;

    for (i = 0; i < CAT_COUNT; i++) {
        CatPlanet *p = &c->row[i];
        int year   = CAT_YEAR_FIRST + cat_pick(&s, CAT_YEAR_WEIGHT, CAT_YEARS);
        int era    = year < 2000 ? 0 : year < 2009 ? 1 : year < 2018 ? 2 : 3;
        int method = cat_pick(&s, CAT_METHOD_ERA[era], CAT_METHODS);
        int size   = cat_pick(&s, CAT_SIZE_BY_METHOD[method], CAT_SIZES);
        int steps, k;
        float d;

        /* Period, by method: transit surveys need repeats inside one campaign
           and imaging can only resolve a planet far from its star. */
        switch (method) {
        case CAT_TRANSIT:
            steps = cat_below(&s, 91);
            /* A hot Jupiter is a giant that transits every few days - the
               first kind of planet anyone found, and still the easiest. */
            if (size == CAT_GIANT && cat_below(&s, 4) != 0)
                steps = cat_below(&s, 41);
            break;
        case CAT_RADIAL:    steps = 10 + cat_below(&s, 141); break;
        case CAT_MICROLENS: steps = 110 + cat_below(&s, 66); break;
        default:            steps = 180 + cat_below(&s, 56); break;
        }
        cat_draw_orbit(&s, steps, &p->period, &p->teq);

        p->radius = cat_range(&s, CAT_SIZE_LO[size], CAT_SIZE_HI[size]);
        p->mass   = cat_mass_for(&s, p->radius);

        d     = 1.3f;
        steps = CAT_DIST_LO[method] +
                cat_below_far(&s, CAT_DIST_HI[method] - CAT_DIST_LO[method] + 1);
        for (k = 0; k < steps; k++)
            d *= 1.055f;
        p->distance = d * cat_range(&s, 1.0f, 1.055f);   /* off the rung */

        p->year   = (uint16_t)year;
        p->method = (uint8_t)method;
        p->survey = (uint8_t)cat_survey_for(&s, method, year);
        p->host   = (uint16_t)(1 + cat_below(&s, 9999));
    }
}

#endif /* APP_CATALOG_H */
