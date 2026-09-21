/*
    hello.c - the smallest RayClay program. Start here.

    rcRunApp is the entry point every RayClay app uses, and this is it with
    nothing configured: NULL asks for every default and opens a window on the
    built-in welcome canvas. To make it yours, fill an RC_AppOptions (see
    rayclay.h) and pass that instead - never something else.

    The same source runs on every desktop and compiles to the web
    (cmake --preset web). Containing none of your code, it is also the fastest
    way to tell a broken toolchain from a broken layout.

    Next: ex01 onward, one GUI per decade; ex10 is the full widget gallery.
    Build target: rayclay_ex00_hello
*/
#include "rayclay.h"

int main(void) { return rcRunApp(NULL); }
