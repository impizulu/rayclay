/*
================================================================================
    platform.h - the OS seam, and in a RayClay app it is usually empty
================================================================================
    The library absorbs the window, the event loop, input, fonts, the clipboard,
    content scale, safe-area insets, the soft keyboard, telemetry and zoom, so
    an ordinary app has nothing left to branch on. A genuine entry here is
    something like listing a directory or opening a URL: one function, one
    `#if`, bodies inline, no .c file.

    WHAT DOES NOT BELONG HERE is a mobile-versus-desktop layout. A layout
    branches on the SPACE IT HAS, never on the operating system - a desktop
    window dragged narrow is phone-shaped. rcViewport() and rcPointerIsCoarse()
    are how you ask.
================================================================================
*/
#ifndef APP_PLATFORM_H
#define APP_PLATFORM_H

#endif /* APP_PLATFORM_H */
