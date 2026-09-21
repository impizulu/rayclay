/*  demo_image.h - the gallery's in-memory fallback for the one demo PNG.

    rcLoadImage resolves against the process working directory, so the file only
    loads when the gallery runs from the repository root. The IMAGE section
    therefore shows both entry points and falls back to the second when the
    first cannot find the file:

        rcLoadImage(path)                 - decode a file
        rcLoadImageFromMemory(bytes, len) - decode bytes you already hold

    That is the honest production pattern too: an app that must not depend on
    its working directory ships its pixels inside the binary. A 32-bit BMP is
    the format the decoder accepts that can be written with no compressor and
    no libc.  */

#ifndef RC_EX10_DEMO_IMAGE_H
#define RC_EX10_DEMO_IMAGE_H

/* 96x96 keeps the encoded card under 37 KB, so it fits a stack buffer in the
   one-time seed path. 32-bit rows are 4-byte aligned, so BMP's padding never
   applies. */
enum {
    DEMO_CARD_W   = 96,
    DEMO_CARD_H   = 96,
    DEMO_CARD_CAP = 54 + DEMO_CARD_W * DEMO_CARD_H * 4
};

static void demo__u32le(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)( v        & 0xFFu);
    p[1] = (unsigned char)((v >>  8) & 0xFFu);
    p[2] = (unsigned char)((v >> 16) & 0xFFu);
    p[3] = (unsigned char)((v >> 24) & 0xFFu);
}

static void demo__u16le(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)( v       & 0xFFu);
    p[1] = (unsigned char)((v >> 8) & 0xFFu);
}

/* Encode the fallback card into `dst`, returning the byte length written or 0 if
   `cap` is too small - the caller treats 0 as a failed load, so neither path can
   hand rcLoadImageFromMemory a short buffer. */
static int demo_card_bmp(unsigned char *dst, int cap) {
    const int w = DEMO_CARD_W, h = DEMO_CARD_H;
    const int pixels = w * h * 4;
    const int total  = 54 + pixels;
    int r, x, k;

    if (!dst || cap < total)
        return 0;

    for (k = 0; k < 54; k++)
        dst[k] = 0;

    /* BITMAPFILEHEADER (14 bytes), then BITMAPINFOHEADER (40); every field the
       decoder does not need stays zero. */
    dst[0] = 'B';
    dst[1] = 'M';
    demo__u32le(dst + 2,  (unsigned)total);    /* bfSize                     */
    demo__u32le(dst + 10, 54u);                /* bfOffBits                  */
    demo__u32le(dst + 14, 40u);                /* biSize                     */
    demo__u32le(dst + 18, (unsigned)w);        /* biWidth                    */
    demo__u32le(dst + 22, (unsigned)h);        /* biHeight, + = bottom-up    */
    demo__u16le(dst + 26, 1u);                 /* biPlanes                   */
    demo__u16le(dst + 28, 32u);                /* biBitCount                 */
    demo__u32le(dst + 30, 0u);                 /* biCompression = BI_RGB     */
    demo__u32le(dst + 34, (unsigned)pixels);   /* biSizeImage                */

    /* Bottom-up BGRA rows: file row r is image row h-1-r. */
    for (r = 0; r < h; r++) {
        const int y = h - 1 - r;
        for (x = 0; x < w; x++) {
            unsigned char *o = dst + 54 + (r * w + x) * 4;
            int edge   = (x < 3 || y < 3 || x >= w - 3 || y >= h - 3);
            /* A 45-degree band whose period divides none of the scale factors
               this section draws at, so the filtering modes look different. */
            int band   = (((x + y) / 7) & 1);
            int red    = 40  + (x * 150) / w;
            int green  = 90  + (y * 120) / h;
            int blue   = 200 - (x * 90)  / w;

            /* The bands differ in LUMINANCE, not just hue: the tinted copy in
               the section would flatten a purely chromatic pattern. */
            if (band) {
                red   = (red   * 45) / 100;
                green = (green * 45) / 100;
                blue  = (blue  * 45) / 100;
            }
            if (edge)
                red = green = blue = 235;

            o[0] = (unsigned char)blue;    /* B */
            o[1] = (unsigned char)green;   /* G */
            o[2] = (unsigned char)red;     /* R */
            o[3] = 0xFFu;                  /* A - opaque */
        }
    }
    return total;
}

#endif /* RC_EX10_DEMO_IMAGE_H */
