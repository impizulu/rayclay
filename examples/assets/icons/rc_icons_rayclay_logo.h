#ifndef RC_ICON_RAY_CLAY_LOGO_H
#define RC_ICON_RAY_CLAY_LOGO_H

#include "rc_icons_common.h"

/*
    Generated from rayclay-logo.svg. The layout pass owns placement; the library
    draws the icon through the rc_gfx seam during RayClay's custom render pass. Source viewBox: 731.429x731.429.
*/
static inline void rcDrawIconRayClayLogo(RC_BoundingBox bounds,
                                         RC_Color color,
                                         const void *userData) {
    (void)userData;
    (void)color;
    const float viewBox = 731.429f;

    static const RC_IconPoint path0[] = {
        { 183.714f, 374.714f },
        { 81.714f, 316.714f },
        { 195.714f, 442.714f }
    };
    const RC_Color c0 = { 95, 107, 176, 255 };
    rcIconDrawFilledPolygon(bounds, path0,
                            3,
                            viewBox, c0);

    static const RC_IconPoint path1[] = {
        { 547.714f, 374.714f },
        { 649.714f, 316.714f },
        { 535.714f, 442.714f }
    };
    const RC_Color c1 = { 95, 107, 176, 255 };
    rcIconDrawFilledPolygon(bounds, path1,
                            3,
                            viewBox, c1);

    const RC_Color c2 = { 95, 107, 176, 255 };
    rcIconDrawFilledEllipse(bounds, 365.714f, 394.714f, 200.0f, 185.0f, viewBox, c2);

    const RC_Color c3 = { 71, 82, 143, 255 };
    rcIconDrawFilledEllipse(bounds, 365.714f, 474.714f, 16.0f, 11.0f, viewBox, c3);

    const RC_Color c4 = { 255, 255, 255, 255 };
    rcIconDrawFilledEllipse(bounds, 293.714f, 434.714f, 38.0f, 44.0f, viewBox, c4);

    const RC_Color c5 = { 255, 255, 255, 255 };
    rcIconDrawFilledEllipse(bounds, 443.714f, 434.714f, 38.0f, 44.0f, viewBox, c5);

    const RC_Color c6 = { 35, 38, 74, 255 };
    rcIconDrawFilledCircle(bounds, 307.714f, 444.714f, 17.0f, viewBox, c6);

    const RC_Color c7 = { 35, 38, 74, 255 };
    rcIconDrawFilledCircle(bounds, 457.714f, 444.714f, 17.0f, viewBox, c7);

    const RC_Color c8 = { 255, 255, 255, 255 };
    rcIconDrawFilledCircle(bounds, 301.714f, 436.714f, 6.0f, viewBox, c8);

    const RC_Color c9 = { 255, 255, 255, 255 };
    rcIconDrawFilledCircle(bounds, 451.714f, 436.714f, 6.0f, viewBox, c9);

    static const RC_IconPoint path2[] = {
        { 255.714f, 366.714f },
        { 259.972f, 364.113f },
        { 264.246f, 361.808f },
        { 268.535f, 359.8f },
        { 272.839f, 358.089f },
        { 277.16f, 356.675f },
        { 281.496f, 355.558f },
        { 285.847f, 354.738f },
        { 290.214f, 354.214f },
        { 294.597f, 353.988f },
        { 298.996f, 354.058f },
        { 303.41f, 354.425f },
        { 307.839f, 355.089f },
        { 312.285f, 356.05f },
        { 316.746f, 357.308f },
        { 321.222f, 358.863f },
        { 325.714f, 360.714f }
    };
    const RC_Color c10 = { 58, 67, 115, 255 };
    rcIconDrawPolyline(bounds, path2,
                       17,
                       viewBox, 13.0f, false, c10);

    static const RC_IconPoint path3[] = {
        { 415.714f, 382.714f },
        { 420.206f, 381.558f },
        { 424.683f, 380.589f },
        { 429.144f, 379.808f },
        { 433.589f, 379.214f },
        { 438.019f, 378.808f },
        { 442.433f, 378.589f },
        { 446.831f, 378.558f },
        { 451.214f, 378.714f },
        { 455.581f, 379.058f },
        { 459.933f, 379.589f },
        { 464.269f, 380.308f },
        { 468.589f, 381.214f },
        { 472.894f, 382.308f },
        { 477.183f, 383.589f },
        { 481.456f, 385.058f },
        { 485.714f, 386.714f }
    };
    const RC_Color c11 = { 58, 67, 115, 255 };
    rcIconDrawPolyline(bounds, path3,
                       17,
                       viewBox, 13.0f, false, c11);

    static const RC_IconPoint path4[] = {
        { 248.714f, 509.714f },
        { 263.402f, 524.285f },
        { 278.214f, 536.746f },
        { 293.152f, 547.097f },
        { 308.214f, 555.339f },
        { 323.402f, 561.472f },
        { 338.714f, 565.496f },
        { 354.152f, 567.41f },
        { 369.714f, 567.214f },
        { 385.402f, 564.91f },
        { 401.214f, 560.496f },
        { 417.152f, 553.972f },
        { 433.214f, 545.339f },
        { 449.402f, 534.597f },
        { 465.714f, 521.746f },
        { 482.152f, 506.785f },
        { 498.714f, 489.714f },
        { 482.152f, 499.167f },
        { 465.714f, 507.527f },
        { 449.402f, 514.792f },
        { 433.214f, 520.964f },
        { 417.152f, 526.042f },
        { 401.214f, 530.027f },
        { 385.402f, 532.917f },
        { 369.714f, 534.714f },
        { 354.152f, 535.417f },
        { 338.714f, 535.027f },
        { 323.402f, 533.542f },
        { 308.214f, 530.964f },
        { 293.152f, 527.292f },
        { 278.214f, 522.527f },
        { 263.402f, 516.667f }
    };
    const RC_Color c12 = { 51, 48, 92, 255 };
    rcIconDrawFilledPolygon(bounds, path4,
                            32,
                            viewBox, c12);

    static const RC_IconPoint path5[] = {
        { 415.374f, 554.424f },
        { 406.074f, 551.514f },
        { 395.624f, 549.584f },
        { 384.534f, 548.754f },
        { 373.304f, 549.034f },
        { 362.464f, 550.414f },
        { 352.534f, 552.854f },
        { 343.964f, 556.214f },
        { 337.154f, 560.344f },
        { 333.214f, 564.274f },
        { 334.874f, 564.684f },
        { 338.714f, 565.494f },
        { 342.564f, 566.174f },
        { 346.414f, 566.714f },
        { 350.284f, 567.124f },
        { 354.154f, 567.414f },
        { 358.034f, 567.554f },
        { 361.914f, 567.574f },
        { 365.814f, 567.464f },
        { 369.714f, 567.214f },
        { 373.624f, 566.834f },
        { 377.544f, 566.324f },
        { 381.464f, 565.684f },
        { 385.404f, 564.914f },
        { 389.344f, 564.004f },
        { 393.294f, 562.964f },
        { 397.254f, 561.794f },
        { 401.214f, 560.494f },
        { 405.184f, 559.064f },
        { 409.164f, 557.494f },
        { 413.154f, 555.804f },
        { 415.764f, 554.604f }
    };
    const RC_Color c13 = { 232, 115, 94, 255 };
    rcIconDrawFilledPolygon(bounds, path5,
                            32,
                            viewBox, c13);

    static const RC_IconPoint path6[] = {
        { 431.714f, 512.714f },
        { 463.714f, 508.714f },
        { 451.714f, 546.714f }
    };
    const RC_Color c14 = { 255, 255, 255, 255 };
    rcIconDrawFilledPolygon(bounds, path6,
                            3,
                            viewBox, c14);

    const RC_Color c15 = { 232, 115, 94, 115 };
    rcIconDrawFilledEllipse(bounds, 215.714f, 492.714f, 24.0f, 15.0f, viewBox, c15);

    const RC_Color c16 = { 232, 115, 94, 115 };
    rcIconDrawFilledEllipse(bounds, 519.714f, 476.714f, 24.0f, 15.0f, viewBox, c16);

    static const RC_IconPoint path7[] = {
        { 183.714f, 339.714f },
        { 177.844f, 334.096f },
        { 171.535f, 327.439f },
        { 164.862f, 319.853f },
        { 157.902f, 311.449f },
        { 150.731f, 302.335f },
        { 143.425f, 292.623f },
        { 136.061f, 282.421f },
        { 128.714f, 271.839f },
        { 121.461f, 260.988f },
        { 114.378f, 249.978f },
        { 107.541f, 238.918f },
        { 101.027f, 227.917f },
        { 94.911f, 217.087f },
        { 89.269f, 206.537f },
        { 84.178f, 196.376f },
        { 79.714f, 186.714f },
        { 96.198f, 181.445f },
        { 113.988f, 176.9f },
        { 132.844f, 173.102f },
        { 152.527f, 170.074f },
        { 172.795f, 167.836f },
        { 193.41f, 166.412f },
        { 214.129f, 165.822f },
        { 234.714f, 166.089f },
        { 254.924f, 167.236f },
        { 274.519f, 169.283f },
        { 293.258f, 172.253f },
        { 310.902f, 176.167f },
        { 327.209f, 181.049f },
        { 341.941f, 186.919f },
        { 354.856f, 193.8f },
        { 365.714f, 201.714f },
        { 365.714f, 364.714f },
        { 351.644f, 366.023f },
        { 337.933f, 366.949f },
        { 324.581f, 367.492f },
        { 311.589f, 367.652f },
        { 298.956f, 367.429f },
        { 286.683f, 366.824f },
        { 274.769f, 365.835f },
        { 263.214f, 364.464f },
        { 252.019f, 362.71f },
        { 241.183f, 360.574f },
        { 230.706f, 358.054f },
        { 220.589f, 355.152f },
        { 210.831f, 351.867f },
        { 201.433f, 348.199f },
        { 192.394f, 344.148f }
    };
    const RC_Color c17 = { 242, 137, 75, 255 };
    rcIconDrawFilledPolygon(bounds, path7,
                            49,
                            viewBox, c17);

    static const RC_IconPoint path8[] = {
        { 547.714f, 339.714f },
        { 553.584f, 334.096f },
        { 559.894f, 327.439f },
        { 566.567f, 319.853f },
        { 573.527f, 311.449f },
        { 580.698f, 302.335f },
        { 588.003f, 292.623f },
        { 595.368f, 282.421f },
        { 602.714f, 271.839f },
        { 609.967f, 260.988f },
        { 617.05f, 249.978f },
        { 623.887f, 238.918f },
        { 630.402f, 227.917f },
        { 636.518f, 217.087f },
        { 642.16f, 206.537f },
        { 647.25f, 196.376f },
        { 651.714f, 186.714f },
        { 635.231f, 181.445f },
        { 617.441f, 176.9f },
        { 598.584f, 173.102f },
        { 578.902f, 170.074f },
        { 558.633f, 167.836f },
        { 538.019f, 166.412f },
        { 517.299f, 165.822f },
        { 496.714f, 166.089f },
        { 476.504f, 167.236f },
        { 456.91f, 169.283f },
        { 438.17f, 172.253f },
        { 420.527f, 176.167f },
        { 404.219f, 181.049f },
        { 389.488f, 186.919f },
        { 376.573f, 193.8f },
        { 365.714f, 201.714f },
        { 365.714f, 364.714f },
        { 379.785f, 366.023f },
        { 393.496f, 366.949f },
        { 406.847f, 367.492f },
        { 419.839f, 367.652f },
        { 432.472f, 367.429f },
        { 444.746f, 366.824f },
        { 456.66f, 365.835f },
        { 468.214f, 364.464f },
        { 479.41f, 362.71f },
        { 490.246f, 360.574f },
        { 500.722f, 358.054f },
        { 510.839f, 355.152f },
        { 520.597f, 351.867f },
        { 529.996f, 348.199f },
        { 539.035f, 344.148f }
    };
    const RC_Color c18 = { 246, 234, 209, 255 };
    rcIconDrawFilledPolygon(bounds, path8,
                            49,
                            viewBox, c18);

    static const RC_IconPoint path9[] = {
        { 245.714f, 242.714f },
        { 265.714f, 280.714f },
        { 245.714f, 318.714f },
        { 225.714f, 280.714f }
    };
    const RC_Color c19 = { 246, 234, 209, 255 };
    rcIconDrawFilledPolygon(bounds, path9,
                            4,
                            viewBox, c19);

    static const RC_IconPoint path10[] = {
        { 485.714f, 242.714f },
        { 505.714f, 280.714f },
        { 485.714f, 318.714f },
        { 465.714f, 280.714f }
    };
    const RC_Color c20 = { 242, 137, 75, 255 };
    rcIconDrawFilledPolygon(bounds, path10,
                            4,
                            viewBox, c20);

    static const RC_IconPoint path11[] = {
        { 183.714f, 339.714f },
        { 206.464f, 345.925f },
        { 229.214f, 351.308f },
        { 251.964f, 355.863f },
        { 274.714f, 359.589f },
        { 297.464f, 362.488f },
        { 320.214f, 364.558f },
        { 342.964f, 365.8f },
        { 365.714f, 366.214f },
        { 388.464f, 365.8f },
        { 411.214f, 364.558f },
        { 433.964f, 362.488f },
        { 456.714f, 359.589f },
        { 479.464f, 355.863f },
        { 502.214f, 351.308f },
        { 524.964f, 345.925f },
        { 547.714f, 339.714f }
    };
    const RC_Color c21 = { 71, 82, 143, 255 };
    rcIconDrawPolyline(bounds, path11,
                       17,
                       viewBox, 18.0f, false, c21);

    const RC_Color c22 = { 246, 234, 209, 255 };
    rcIconDrawFilledCircle(bounds, 73.714f, 178.714f, 27.0f, viewBox, c22);

    const RC_Color c23 = { 246, 234, 209, 255 };
    rcIconDrawFilledCircle(bounds, 657.714f, 178.714f, 27.0f, viewBox, c23);

    const RC_Color c24 = { 201, 119, 63, 255 };
    rcIconDrawRoundLine(bounds, 61.714f, 186.714f, 85.714f, 186.714f, viewBox, 5.0f, c24);

    const RC_Color c25 = { 201, 119, 63, 255 };
    rcIconDrawRoundLine(bounds, 645.714f, 186.714f, 669.714f, 186.714f, viewBox, 5.0f, c25);
}

static inline void rcIconRayClayLogo(float size) {
    RC_Color baked = { 0, 0, 0, 255 };   /* unused; the artwork bakes its own colors */
    rcIconEmit(size, baked, rcDrawIconRayClayLogo, NULL);
}

#endif /* RC_ICON_RAY_CLAY_LOGO_H */
