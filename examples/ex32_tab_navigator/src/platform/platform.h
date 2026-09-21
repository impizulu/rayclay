/*
    platform.h - the OS seam, and it is empty.

    A RayClay app branches on the SPACE IT HAS, never on the operating system:
    rcViewport() and rcPointerIsCoarse() are how you ask, so a desktop window
    dragged narrow is phone-shaped without an #ifdef anywhere near the layout.

    Everything a phone app needs that a desktop app does not - the safe area,
    the orientation lock, the back key, finger-sized targets - comes from
    rcViewport().safe, RC_AppOptions.orientation, RC_KEY_ESCAPE and
    rcPointerIsCoarse(), in one source.
*/
#ifndef APP_PLATFORM_H
#define APP_PLATFORM_H

#endif /* APP_PLATFORM_H */
