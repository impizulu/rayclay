/*
    main.c - Perch: the tab navigator, a component to copy, in a phone-first social reader

    app/navigator.h is the component: tabs with badges, a stack of screens under each tab, a real
    push with the old screen drifting and dimming beneath the new one, a swipe back from the left
    edge, large titles that collapse into the top band, and a rail when the window is wide. It knows
    nothing about posts. Copy that one file, give it a callback that draws a route, and you have a
    phone app's navigation.

    Everything else here is the demo around it: Perch, a small social reader that opens at 393x852
    and is locked to portrait on a device. Every control works, nothing touches the network, and
    every run opens the same but for Home's date line, the device's own date. Beyond the navigator
    it shows pull to refresh under a mouse (a finger taps the lit Home tab), sheets that rise from
    the bottom edge and follow a finger down, a toast with Undo, and safe areas spent once, by
    whoever sits on them. Run the desktop build from the repository root, where it finds its
    pictures (app/data.h).

    Read app/navigator.h first, then app/app.h for how an app hosts it. app/state.h is the data
    model and every rule that changes it, app/screens.h what each route shows, app/parts.h the small
    pieces, and app/data.h the demo's content.

    Shows: floats and .offset for a slide, .overlay for a dim, rcIsHovered and rcPointer for a swipe
    and a pull, rcBeginModal as a bottom sheet, rcVirtualList, rcGetScrollInfo and .scrollOffset to
    keep a place, rcTextInput and rcTextArea, rcToggle, rcViewport's safe and keyboard insets,
    rcLoadImage, rcClipboardSet, rcSetStyle at run time, rcPreferredColorScheme and rcLocalTime.

    Build target: rayclay_ex16_tab_navigator
*/
#include "app/app.h"

int main(void) {
    static AppState state;

    app_seed(&state);
    /* The toast's Undo (app/screens.h): nav-press's hover and press, in the page's ground. */
    rcDefineClass("toast-press", "hover:bg-background/5 active:bg-background/10");
    /* A sheet's panel (app/screens.h): no hairline, a shadow cast up, and no padding at the sides,
       so its rows run edge to edge and pad themselves. */
    rcDefineClass("sheet", "w-full max-w-[520px] px-0 py-2 gap-0 border-0 "
                           "shadow-[0_-4px_24px_#00000046]");

    RC_AppOptions opts = {
        .width = 393, .height = 852, .title = "Perch",
        .clearColor = app_style(false).background,
        .scratchArenaBytes = 128 * 1024,     /* backs every rcFormat in a frame */
        .updateCallback = update, .layoutCallback = layout, .userData = &state,
        .renderMode = RC_RENDER_ON_DEMAND,   /* parks between taps */
        /* A phone app that is portrait-first says so. iOS narrows the Info.plist set to it; Android
           applies it when the window opens, so the APK's manifest declares the same lock
           (ORIENTATION portrait where the build adds this example). */
        .orientation = RC_ORIENTATION_PORTRAIT,
    };
    return rcRunApp(&opts);
}
