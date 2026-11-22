/*
    platform.h - the OS seam, and it holds three answers.

    A RayClay app branches on the SPACE IT HAS, never on the operating system: the
    selector's sheet and panel, and its table and list, come from rcViewport() and
    its own width. What is here is what space cannot say: whether the sidebar may
    list the working folder, what the Open shortcut is called, and what the panel's
    Browse button calls the system's dialog.
*/
#ifndef APP_PLATFORM_H
#define APP_PLATFORM_H

#include "rayclay.h"

/* THE WORKING FOLDER IS A PLACE ON A DESKTOP ONLY. A desktop program runs where
   its user started it - a terminal's folder, a shortcut's "Start in" - so the
   sidebar lists it, as the path ".": rcListDirectory reads relative paths from
   there, and RayClay has no call that names that directory. A phone app's working
   directory is the root of a file system its sandbox may not read, and a web
   page's is the root of its own in-memory one, so there the selector stays in
   rcAppDataPath, the folder the app owns, which samples.h fills on the first run.
   iOS is told by clang's own __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__:
   <TargetConditionals.h> spells it TARGET_OS_IPHONE, but an example includes no
   system header. */
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__) \
    || defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
#define PLATFORM_HAS_WORKING_FOLDER 0
#else
#define PLATFORM_HAS_WORKING_FOLDER 1
#endif

/* What the panel's Browse calls the system's dialog. A phone is no computer: its picker
   browses files, as the iOS Files picker does. The web says "your computer", as nothing
   here tells a phone's browser from a desktop's. */
#if defined(__ANDROID__) || defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
#define PLATFORM_BROWSE_LABEL "Browse files..."
#else
#define PLATFORM_BROWSE_LABEL "Browse your computer..."
#endif

/* The Open shortcut as its keys are labeled. RC_MOD_PRIMARY is the key itself. */
#if defined(__APPLE__)
#define PLATFORM_OPEN_SHORTCUT "Cmd+O"
#else
#define PLATFORM_OPEN_SHORTCUT "Ctrl+O"
#endif

#endif /* APP_PLATFORM_H */
