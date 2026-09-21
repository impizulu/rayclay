/* ============================================================================
 *  ex23 - Issue Inbox: the rail / list / detail shape, over 100,000 issues.
 *
 *  Type in the filter and the whole list re-selects; scroll it and about thirty
 *  elements are ever declared. Four ideas carry it:
 *
 *    1. rcVirtualList declares the VIEWPORT, not the dataset, so drawing costs
 *       the same at 100 rows and at 100,000.
 *    2. Filtering permutes an index array. No record is copied or reordered, so
 *       the rail, the list and the detail pane cannot disagree.
 *    3. The scan runs on a CHANGE, not a frame. The rail prints its rebuild
 *       counter, and a window drag leaves it where it was.
 *    4. Ordering it is counting, not comparing: the fixtures lay the issues down
 *       along a timeline, so newest-first is the array itself.
 *
 *  One TU, desktop and web, no assets and no #ifdef.
 *  Build target: rayclay_ex23_issue_inbox
 * ========================================================================= */
#include "app/app.h"

int main(void)
{
    static AppState state;
    float sizes[F_COUNT];

    sizes[F_MICRO] = 11.0f; sizes[F_SMALL] = 13.0f; sizes[F_BODY] = 15.0f; sizes[F_TITLE] = 21.0f;
    /* The app's palette, built in theme.h and installed before the first frame:
       every widget RayClay draws reads it back from here. */
    rcSetStyle(inbox_style());
    seed(&state, 0x1B0C51EDu);
    state.railOpen = true;
    state.selIssue = state.selRow = -1;
    rebuild(&state);

    RC_AppOptions opts = {
        .width = 1280, .height = 800, .title = "RayClay Issue Inbox",
        .clearColor = rcGetStyle().background,
        .fontSizes = sizes, .fontCount = F_COUNT,
        .scratchArenaBytes = 32768,          /* backs every rcFormat in a frame */
        .nativeFrame = true, .titlebarHeight = BAR_H,
        .updateCallback = update, .layoutCallback = layout, .userData = &state,
        .titlebar = { .custom = true },      /* the band above IS the titlebar  */
        .renderMode = RC_RENDER_ON_DEMAND,   /* parks between keystrokes        */
    };
    return rcRunApp(&opts);
}
