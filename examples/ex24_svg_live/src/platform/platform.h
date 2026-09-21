/*
    platform.h - the OS seam, and it is empty.

    Where a portable app puts what one operating system does differently. In a
    RayClay app that is nearly always nothing: the library absorbs the window,
    the event loop, input, fonts, the clipboard, content scale, safe-area
    insets, the soft keyboard, a monotonic clock and zoom.

    A LAYOUT IS NOT AN ENTRY HERE. Branch on the space you have, never on the
    operating system - rcViewport() and rcPointerIsCoarse() are how you ask.
    Real entries look like: open a URL, find a writable config directory, a
    native file dialog.
*/
#ifndef APP_PLATFORM_H
#define APP_PLATFORM_H

/* Deliberately empty. */

#endif /* APP_PLATFORM_H */
