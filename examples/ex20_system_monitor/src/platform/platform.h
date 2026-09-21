/* platform.h - the OS seam, and in a RayClay app it is normally empty.
 *
 * The library absorbs the window, the event loop, input, fonts, the clipboard,
 * content scale, safe-area insets, the soft keyboard, process telemetry and a
 * monotonic clock, so an ordinary app has nothing left to branch on. This one
 * adds nothing here.
 *
 * WHAT DOES NOT BELONG HERE: a mobile-versus-desktop layout. A layout branches
 * on the SPACE IT HAS, never on the operating system - a desktop window dragged
 * narrow is phone-shaped and a tablet in landscape is desktop-shaped.
 * rcViewport() and rcPointerIsCoarse() are how you ask. What does belong:
 * opening a URL, listing a directory, a native file dialog.
 */
#ifndef APP_PLATFORM_H
#define APP_PLATFORM_H

#endif /* APP_PLATFORM_H */
