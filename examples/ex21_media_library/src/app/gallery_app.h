/*
    gallery_app.h - the photo library's state and its four-function contract

    Declarations only: no RayClay layout macros and no system includes, so this is clean to
    include from C and from C++. The app is four functions over one flat AppState, which
    is what lets a window run it and a headless benchmark run the same code.
*/
#ifndef GALLERY_APP_H
#define GALLERY_APP_H

#include "app_ctx.h"            /* AppCtx / AppInputSink / AppMode: the seam */
#include "gallery_backend.h"    /* GalStore and the wall's layouts           */

/* The scripted scenario's version. Bump when its rendered output changes. */
#define GALLERY_BENCH_VERSION 19

/* The toolbar's height in window px, which main.c hands the window as .titlebar.height. It is
   fixed chrome: the wall's bar, the viewer's and the player's are each declared inside
   rcUnzoomed() at this height, so they stay on the strip the system drags by at any zoom. */
#define GALLERY_BAND_H 52

/* The toolbar's segmented control: three densities of one wall. */
typedef enum { GAL_VIEW_YEARS = 0, GAL_VIEW_MONTHS, GAL_VIEW_ALL, GAL_VIEW_COUNT } GalView;

/* What the main pane shows. */
typedef enum { GAL_TAB_LIBRARY = 0, GAL_TAB_MEMORIES, GAL_TAB_ALBUMS, GAL_TAB_SEARCH } GalTab;

enum { GAL_ZOOM_STEPS = 5, GAL_ZOOM_DEFAULT = 2, GAL_FULL_SLOTS = 3 };

typedef enum { GAL_EDIT_ADJUST = 0, GAL_EDIT_CROP } GalEditTab;

/* The lightbox: a photo opened from the wall. */
typedef struct {
    bool       open;         /* drawn at all                                       */
    bool       closing;      /* flying back to its tile                            */
    float      fade;         /* 0 is the wall, 1 is the black viewer               */
    GalRect    rect;         /* where the picture is now, eased toward its target  */
    float      filmPos;      /* the filmstrip's center, eased toward the selection */
    bool       easing;       /* either still has some way to go                    */
    bool       editing;
    GalEditTab editTab;
    GalEdit    work;         /* the edit in progress, committed by Done            */
    bool       workDirty;
    bool       comparing;    /* Hold to compare is down                            */
} GalViewer;

/* The full-bleed player. A film is a video through rcMediaOpen; a story or a slideshow is
   the same chrome over photographs with its own clock. */
typedef struct {
    bool     open;
    int      memory;         /* into GAL_MEMORIES; -1 is a slideshow of the wall     */
    RC_Media clip;           /* the film's media, 0 for a story                      */
    double   time;           /* a story's playhead, seconds                          */
    float    rate;
    bool     playing;
    bool     captions;
    bool     transcript;     /* the list of lines beside the picture                 */
    bool     awake;          /* what rcSetKeepScreenOn was last told                 */
    int      track;          /* the film's caption language                          */
    bool     trackPicked;    /* rcPreferredLocales is asked once, on the first film  */
    float    scrub;          /* the seek slider's value                              */
    int      list[GAL_PHOTO_COUNT];
    int      listCount;
} GalPlayer;

/* App state: a flat POD, zeroed by gallery_seed. */
typedef struct {
    GalStore  store;                 /* the catalog, by value                         */
    GalWall   wall;                  /* where every photo sits at the current density */
    GalRect   from[GAL_PHOTO_COUNT]; /* where each stood when a zoom began, in the view */
    uint8_t   fromState[GAL_PHOTO_COUNT];
    float     morph;                 /* the zoom's progress, 1 at rest                */
    uint32_t  wallKey;               /* the inputs wall was built from                */
    int       wallW, wallH;          /* the wall's size at the last build             */
    float     wallScroll;            /* the reader's place, read back every frame     */
    float     scrollTarget;          /* where a zoom lands the scroll                 */
    bool      scrollPending;         /* it waits a frame, for the new content range   */
    int       anchor;                /* the photo a zoom holds in place, or -1        */
    int       yearsFrom;             /* the photo held on the way into Years, or -1   */
    bool      wallToTop;             /* a new scope or search opens at its top        */

    GalTab    tab;
    GalView   view;
    int       zoomStep;              /* All Photos' row height, 0 is the largest      */
    int       scope;                 /* GalScope                                      */
    int       album;
    char      search[GAL_SEARCH_CAP];
    bool      searchOpen;            /* the field replaces the title where room is short */
    bool      focusSearch;           /* a key asked for the field: focus it next frame */
    bool      filterDirty;           /* the store changed: refilter next frame        */
    bool      selecting;             /* tiles toggle instead of opening               */
    uint8_t   picked[GAL_PHOTO_COUNT];
    int       pickedCount;
    bool      confirmDelete;

    int       selected;              /* the open photo; the seeded one at rest        */
    char      caption[GAL_CAPTION_CAP]; /* the open photo's caption being edited      */
    int       captionOf;             /* whose it is (-1 = not loaded yet)             */
    bool      modalInfo;             /* Info: docked where it fits, else a modal      */
    bool      darkMode;

    GalViewer viewer;
    GalPlayer player;

    /* What the picture jobs still owe: a tile asks by stamping its frame here. */
    uint32_t  frame;
    uint32_t  wanted[GAL_PHOTO_COUNT];
    uint16_t  wantEdge[GAL_PHOTO_COUNT]; /* the picture's long edge its place calls for */
    uint16_t  sharp[GAL_PHOTO_COUNT]; /* the long edge it has, 0 for the blurred one  */
    float     dt;                    /* this frame's true delta, from gallery_update  */
    float     density;               /* the display's scale, from main.c; 0 draws at 1 */
    bool      working;               /* a picture job ran this frame                  */
    int       ambientOf;             /* which photo tints the sidebar                 */

    /* Element ids: a tile, its check, its filmstrip thumbnail. */
    char      tileId[GAL_PHOTO_COUNT][8];
    char      checkId[GAL_PHOTO_COUNT][8];
    char      filmId[GAL_PHOTO_COUNT][8];
    char      cardId[GAL_PHOTO_COUNT][8]; /* a month card's scrim                    */
    char      lineId[GAL_PHOTO_COUNT][8]; /* a transcript row                        */

    bool      seeded;
} AppState;

/* The four-function contract. No RC_App and no window handle; input comes through the
   shared AppInputSink. */
void gallery_seed  (AppState *st, unsigned seed);       /* zero, build the catalog,
    first pictures */
void gallery_update(AppState *st,
                    const AppCtx *ctx);   /* picture jobs and every animation clock  */
void gallery_layout(AppState *st, const AppCtx *ctx);   /* the whole UI */
void gallery_bench_step(AppState *st, const AppInputSink *in, int frame);

/* The app's style, which main.c also needs for the window's clear color. */
RC_Style gallery_style(bool dark);

/* True while something moves with no input behind it: a picture still rendering, a zoom in
   flight, a playing story. An on-demand window has to be asked for those frames. */
bool gallery_wants_frame(const AppState *st);

#endif /* GALLERY_APP_H */
