/*
    platform.h - the OS seam, and it holds one answer.

    A RayClay app branches on the SPACE IT HAS, never on the operating system:
    rcViewport() and rcPointerIsCoarse() are how you ask, so a desktop window
    dragged narrow is phone-shaped without an #ifdef anywhere near the layout.

    The soft keyboard follows focus by itself, and rcViewport().keyboard is zero on
    a desktop without an on-screen keyboard. The input method's candidate window
    needs nothing from the app either: a focused rcTextInput or rcTextArea hands the
    platform its caret on every frame.

    What is here is what the keyboard's modifiers are called on this machine.
*/
#ifndef APP_PLATFORM_H
#define APP_PLATFORM_H

#include "rayclay.h"

/* RC_MOD_PRIMARY and RC_MOD_ALT, as this machine's keycaps print them. */
#if defined(__APPLE__)
#define PLATFORM_PRIMARY "Cmd"
#define PLATFORM_ALT     "Option"
#else
#define PLATFORM_PRIMARY "Ctrl"
#define PLATFORM_ALT     "Alt"
#endif

#endif /* APP_PLATFORM_H */
