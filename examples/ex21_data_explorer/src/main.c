/* ============================================================================
 *  ex21 - Data Explorer
 * ============================================================================
 *  An analytics workbench over a 12,000-row exoplanet catalogue: filter it from
 *  the rail, and a year chart, a radius histogram, a period/radius scatter, a
 *  summary and a sortable table all follow.
 *
 *  Demonstrates: one dataset behind many views through an index array; derived
 *  data cached and rebuilt only when a control moves; a sampled scatter; a
 *  virtualized 12,000-row table; charts linked to the table both ways; and one
 *  layout that reflows from desktop to phone with no platform branch.
 *
 *  Build target rayclay_ex21_data_explorer, or the web target `explorer`.
 *  Zero-asset: bundled font and procedural icons, nothing to install.
 * ========================================================================= */

#include "app/app.h"

int main(void)
{
    static AppState state;
    float fontSizes[F_COUNT];

    fontSizes[F_MICRO] = 11.0f;
    fontSizes[F_SMALL] = 12.5f;
    fontSizes[F_HEAD]  = 17.0f;
    fontSizes[F_STAT]  = 26.0f;

    /* Installed BEFORE anything reads it, including .clearColor below: the
       whole app's colour is this one call. */
    rcSetStyle(explorer_style());

    catalog_init(&state.cat, 0x45585021u);
    view_init(&state.view, &state.cat);
    filters_reset(&state.filter, state.view.distanceMax);
    /* Nearest first: unlike "newest first" it opens on a screen where every
       column varies. */
    state.sortKey  = K_DISTANCE;
    state.sortDesc = false;
    state.selected = -1;
    view_rebuild(&state.view, &state.cat, &state.filter, state.sortKey,
                  state.sortDesc);

    RC_AppOptions opts = {
        .width  = 1320,
        .height = 860,
        .title  = "RayClay Data Explorer",
        .clearColor = rcGetStyle().background,
        .fontSizes = fontSizes,
        .fontCount = F_COUNT,
        /* Backs every rcFormat in a frame: ~20 virtualized rows of formatted
           cells, plus the rail, the detail card and the status bar. */
        .scratchArenaBytes = 32768,
        /* The bundled titlebar rather than a custom one: this app is here to
           show what you get for free. */
        .nativeFrame = true,
        .layoutCallback  = layout,
        .userData  = &state,
        /* The default, spelled out because it is what an analytics tool should
           do: nothing animates, so the window parks between interactions. */
        .renderMode = RC_RENDER_ON_DEMAND
    };

    return rcRunApp(&opts);
}
