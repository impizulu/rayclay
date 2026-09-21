/*
    gallery_app.h - the gallery app's types and prototypes.

    Declarations only: no RayClay layout macros and no system includes, so this is
    clean to include from C and from C++.

    The app is a photo gallery and viewer - a searchable, tag-filtered,
    clipped-scroll thumbnail wall in dated sections, with a tile-size control, and
    a detail pane carrying the photograph, its metadata, an editable caption
    (rcTextArea) and a details modal. Side by side where both panes fit; one pane
    at a time on a narrow window.
*/
#ifndef GALLERY_APP_H
#define GALLERY_APP_H

#include "bench_app.h"          /* the shared AppCtx / AppInputSink / AppMode contract */
#include "gallery_backend.h"    /* GalStore + the procedural-BMP asset generator       */

/* The scripted scenario's version. Bump when its rendered output changes. */
#define GALLERY_BENCH_VERSION 6

/* Font ladder. F_HERO is the display face, spent on the details modal's heading:
   text that large has to stay crisp at 2x zoom, and this app is the example. */
typedef enum { F_SMALL = 0, F_BODY, F_MD, F_TITLE, F_HERO, F_COUNT } GalleryFont;

/* App state - a flat, memset-able POD, so gallery_seed can memset then set. The
   decoded RC_Image handles live here rather than in the RayClay-free backend, and
   are copied from a one-time process cache, so a re-seed never re-decodes. */
typedef struct {
    GalStore  store;                       /* the backend model, BY VALUE (seed zeroes it) */
    RC_Image  images[GAL_IMG_COUNT];       /* decoded handles (copied from the process cache) */
    int       selected;                    /* selected image index, 0..GAL_IMG_COUNT-1     */
    int       tagFilter;                   /* index into GAL_TAG_LABELS; 0 = everything    */
    int       zoomStep;                    /* tile size; 0 = the largest tiles              */
    char      search[32];                  /* grid search box (rcTextInput); filters titles */
    char      caption[GAL_CAPTION_CAP];    /* the editable multiline caption (rcTextArea)   */
    int       captionOf;                   /* image the caption buffer holds (-1 = unsynced)  */
    bool      modalInfo;                   /* the photo-details modal (closed by default)     */
    bool      darkMode;                    /* theme toggle                                    */
    bool      detailOpen;                  /* one-pane arm: the photo view is open (the grid
                                              is home; a tap opens it, the back control
                                              closes it; ignored while both panes fit)     */
    bool      seeded;                      /* demo lazy-init guard                            */
} AppState;

/* The four-function contract, plus the demo-only overlay. No RC_App and no window
   handle; the input seam is the shared AppInputSink. */
void gallery_seed  (AppState *st, unsigned seed);            /* memset, then build and load the images */
void gallery_update(AppState *st, const AppCtx *ctx);        /* static content: a no-op */
void gallery_layout(AppState *st, const AppCtx *ctx);        /* the whole UI */
void gallery_demo_chrome(AppState *st, const AppCtx *ctx);   /* demo-only overlay */
void gallery_bench_step(AppState *st, const AppInputSink *in, int frame);

/* The app's own theme: the preset re-surfaced in neutral grey, with blue kept for
   selection and action, so the photographs carry the colour. The demo runner needs
   it too, for the window's clear colour. */
RC_Style gallery_style(bool dark);

#endif /* GALLERY_APP_H */
