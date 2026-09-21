/*
    platform.h - the OS seam, and in a RayClay app it is empty.

    A layout branches on the SPACE IT HAS, never on the operating system:
    rcViewport() and rcPointerIsCoarse() are how you ask, because a desktop
    window dragged narrow is phone-shaped and a touchscreen laptop is a fine
    desktop with a coarse pointer. What belongs here is the handful of things
    RayClay does not absorb - opening a URL, a native file dialog, a writable
    config directory.
*/
#ifndef APP_PLATFORM_H
#define APP_PLATFORM_H

#endif /* APP_PLATFORM_H */
