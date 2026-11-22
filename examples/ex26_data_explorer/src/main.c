/*
    main.c - RayClay Data Explorer: an analytics explorer over 12,000 orders

    In the Datadog and Metabase mold: a query bar with removable filter chips, facets
    with live counts, KPIs that double as the metric picker, a trend you can click or
    drag to filter, a top list you can click to filter, and a table that always fits
    its width.

    Demonstrates: one dataset behind many views through an index array; one scan that
    answers every panel at once; a table that drops its least important columns and
    says "+N columns" instead of scrolling sideways; a virtualized rcBeginTable with
    sortable headers and a selectable row; a chart brush built from three public reads;
    rcBeginSplitPane between chart and table; exporting with rcFileDialogSave and
    rcClipboardSet; and one layout that reflows from desktop to phone with no platform
    branch.

    Keys: / focuses the query, Enter turns region:west into a chip, Backspace on an
    empty query takes off the last chip, Up and Down walk the rows with the pointer on
    the table or an order open, Esc closes.

    Zero-asset: bundled font and procedural icons, nothing to install.
    Build target: rayclay_ex26_data_explorer
*/

#include "app/app.h"

int main(void) {
    static AppState state;
    /* Slot 0 is what the library's own widgets draw with, so it sets the app's body
       size. Every other size is written where it is used. */
    static float bodySize = 14.0f;
    RC_Style style = app_style();
    int d, i;

    rcSetStyle(style);
    state.pal = app_palette();
    app_classes();

    orders_init(&state.book, 0x52494447u);
    state.view.labels = rcArenaInit(4096);
    filter_reset(&state);
    state.groupDim    = DIM_CATEGORY;
    state.metric      = M_REVENUE;
    state.sortCol     = C_DATE;
    state.sortDesc    = true;
    state.selected    = -1;
    state.split       = 0.5f;
    state.rangeBefore = RANGE_DEFAULT;
    for (i = 0; i < C_COUNT; i++)
        state.importance[i] = COLUMN_IMPORTANCE[i];
    for (d = 0; d < DIM_COUNT + 1; d++)
        state.sectionOpen[d] = d <= DIM_STATUS || d == DIM_COUNT;
    set_range(&state, RANGE_DEFAULT);

    RC_AppOptions opts = {
        .width  = 1280,
        .height = 820,
        .title  = "Ridgeline Orders",
        .clearColor = style.background,
        .fontSizes = &bodySize,
        .fontCount = 1,
        /* Backs every rcFormat in a frame: about 20 virtualized rows of formatted cells,
           the rail's counts and the chart's ticks. */
        .scratchArenaBytes = 65536,
        .frame = RC_WINDOW_FRAME_NONE,
        .layoutCallback = layout,
        .userData = &state,
        .titlebar = { .height = BAND_H }, /* the app's top band is the titlebar */
    };

    return rcRunApp(&opts);
}
