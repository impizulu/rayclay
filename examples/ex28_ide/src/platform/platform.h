/*
    platform.h - the OS seam: two answers

    Layout never asks which system it runs on. This app says two things differently per system:
    the name of the key RC_MOD_PRIMARY reads, so a shortcut hint matches the keyboard, and what a
    file dialog's or a drop's path names: the user's own file on a desktop, a private read-only
    copy on the web and phones (rcFileDialogOpen's notes), which Save must not write to.
*/
#ifndef APP_PLATFORM_H
#define APP_PLATFORM_H

#if defined(__APPLE__)
    #define PRIMARY_KEY "Cmd"
#else
    #define PRIMARY_KEY "Ctrl"
#endif

/* iOS is told by clang's own macro: an example includes no system header. */
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__) \
    || defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
    #define HANDED_PATHS_ARE_COPIES 1
#else
    #define HANDED_PATHS_ARE_COPIES 0
#endif

#endif /* APP_PLATFORM_H */
