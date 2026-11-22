/*
    platform.h - the OS seam: one word.

    The library absorbs the window, the event loop, input, fonts, the clipboard,
    content scale, safe-area insets, the soft keyboard, process telemetry and a
    monotonic clock, so an ordinary app has nothing left to branch on. This one
    says one thing differently per system: the name of the key RC_MOD_PRIMARY
    reads, so a shortcut hint matches the keyboard.

    WHAT DOES NOT BELONG HERE: a mobile-versus-desktop layout. A layout branches
    on the SPACE IT HAS, never on the operating system - a desktop window dragged
    narrow is phone-shaped and a tablet in landscape is desktop-shaped.
    rcViewport() and rcPointerIsCoarse() are how you ask.
*/
#ifndef APP_PLATFORM_H
#define APP_PLATFORM_H

#if defined(__APPLE__)
#define PRIMARY_KEY "Cmd"
#else
#define PRIMARY_KEY "Ctrl"
#endif

#endif /* APP_PLATFORM_H */
