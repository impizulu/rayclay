/*
    platform.h - the OS seam, and it is empty.

    A RayClay app branches on the SPACE IT HAS, never on the operating system:
    rcViewport() and rcPointerIsCoarse() are how you ask, so a desktop window
    dragged narrow is phone-shaped without an #ifdef anywhere near the layout.

    The soft keyboard is the one piece of phone behaviour this app drives
    itself, and it drives it from one source too: rcSetSoftKeyboardVisible()
    and rcViewport().keyboard are inert and zero on the desktop and the web.

    rcSetImeCaretRect() is the one worth knowing about, because "desktop" is
    not the right unit for it: it is a no-op under GLFW, the desktop default,
    and a REAL call under SDL3, which is a supported desktop backend as well as
    the only mobile and web one. On a desktop SDL3 build it places the IME
    candidate window, which is what a CJK user needs and the reason to call it
    from the same source as everything else. Call it unconditionally either way.
*/
#ifndef APP_PLATFORM_H
#define APP_PLATFORM_H

#endif /* APP_PLATFORM_H */
