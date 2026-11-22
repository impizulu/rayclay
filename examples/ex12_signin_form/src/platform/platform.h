/*
    platform.h - the OS seam, and it holds one answer

    A RayClay app branches on the SPACE IT HAS, never on the operating system:
    rcViewport() says whether the form is a card or the screen, on any system.
    What is here is the one thing that is not about space.
*/
#ifndef APP_PLATFORM_H
#define APP_PLATFORM_H

#include "rayclay.h"

/* SIGN IN WITH APPLE IS OFFERED WHERE THE SYSTEM OFFERS IT: macOS and iOS,
   where __APPLE__ is defined. There the app hands the request to the system
   (AuthenticationServices), which shows Apple's own sheet with Face ID or Touch
   ID and the account the device is signed in to, and App Store Review Guideline
   4.8 asks an app that offers Google sign-in to offer an equivalent private
   one, which this is. Apple has a web flow for other systems, but there it is a
   browser detour to an account most of their users do not have. */
#if defined(__APPLE__)
#define PLATFORM_SIGN_IN_WITH_APPLE 1
#else
#define PLATFORM_SIGN_IN_WITH_APPLE 0
#endif

#endif /* APP_PLATFORM_H */
