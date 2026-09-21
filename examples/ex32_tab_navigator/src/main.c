/*
 *  ex32 - RayClay Pocket   |   the navigation model of a phone app, one TU
 *
 *  A bottom tab bar with four tabs; under each, a list that pushes a detail with
 *  a back control, with per-tab stacks and per-tab scroll positions that survive
 *  a tab switch. It opens at 393x852 and is locked to portrait on a device. On a
 *  viewport 900 units wide or more the SAME source shows the tabs as a left rail
 *  with the list and the detail side by side - the same ids, texts and state, so
 *  that arm is a presentation, never a second app.
 *
 *  Shows: rcVirtualList, rcTextInput, rcToggle, rcSlider, rcCombo, rcScrollbar,
 *  .scrollOffset, floating badges, rcPointerIsCoarse, rcViewport().safe and a
 *  run-time theme swap through rcSetStyle.
 *
 *  Build target: rayclay_ex32_tab_navigator
 */
#include "app/app.h"

int main(void)
{
    static AppState state;
    float sizes[F_COUNT];

    sizes[F_MICRO] = 11.0f; sizes[F_SMALL] = 13.0f; sizes[F_BODY] = 15.0f;
    sizes[F_HEAD]  = 17.0f; sizes[F_TITLE] = 26.0f;
    rcSetStyle(rcStyleDark());
    seed(&state, 0x9E3779B9u);
    state.textSize = 15.0f;
    rebuild_matches(&state);             /* an empty query, every category: 120 */
    rebuild_feed(&state);                /* the All filter: every post          */

    RC_AppOptions opts = {
        .width = 393, .height = 852, .title = "RayClay Pocket",
        .clearColor = rcGetStyle().background,
        .fontSizes = sizes, .fontCount = F_COUNT,
        .scratchArenaBytes = 32768,          /* backs every rcFormat in a frame */
        .updateCallback = update, .layoutCallback = layout, .userData = &state,
        .renderMode = RC_RENDER_ON_DEMAND,   /* parks between taps            */
        /* A phone app that is portrait-first says so; on a device this can only
           NARROW the set the Info.plist / manifest allows. */
        .orientation = RC_ORIENTATION_PORTRAIT,
    };
    return rcRunApp(&opts);
}
