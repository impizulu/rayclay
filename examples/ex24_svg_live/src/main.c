/*
    main.c - SVG Live: the three ways to put vector art on screen, side by side

    RayClay draws the same artwork three ways. They are pixel-identical, so the
    choice is about your build, not about how it looks:

      rcSvg(path, size, color)       point at the .svg in the layout, like
                                     <img src>. Nothing to load or free. DEFAULT.
      rcSvgHandle(svg, size, color)  you called rcLoadSvg / rcLoadSvgFromMemory,
                                     so the lifetime is yours. For markup with
                                     no file behind it.
      rcIcon<Name>(size, color)      converted to C at build time. Nothing to
                                     ship, no parser to run.

    Shapes are converted to icon ops, not rasterised: paths, line, polyline,
    polygon, rect, circle and ellipse work; gradients, <text>, filters and
    embedded images are skipped with a warning - export a PNG and use
    rcLoadImage for those.

    Build target: rayclay_ex24_svg_live
*/
#include "app/app.h"

int main(void)
{
    static AppState state;
    int rc;

    rcSetStyle(svg_style());
    /* Opens on a line artwork, not on the RayClay mark: a line icon resolves
       currentColor, so the tint swatches visibly do something from the first
       frame. The mark is one click away and says so when it is picked. */
    state.sel  = ART_OPENING;
    state.tint = 2;
    state.size = 180.0f;
    load_selected(&state);   /* the handle route parses once, here - not per frame */

    RC_AppOptions opts = {
        .width = 1180, .height = 760, .title = "RayClay SVG Live",
        .clearColor = rcGetStyle().background,
        .fontSizes = FONT_SIZES, .fontCount = F_COUNT,
        .scratchArenaBytes = 8192,          /* backs every rcFormat in a frame */
        .nativeFrame = true, .titlebarHeight = BAR_H,
        .layoutCallback = layout, .userData = &state,
        .titlebar = { .custom = true },
        .renderMode = RC_RENDER_ON_DEMAND,  /* parks between clicks           */
    };
    rc = rcRunApp(&opts);
    rcUnloadSvg(&state.svg);                    /* ours to free; rcSvg's cache is not */
    return rc;
}
