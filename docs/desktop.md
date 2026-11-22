# Desktop: Linux, macOS and Windows

## Add RayClay to your build

```cmake
cmake_minimum_required(VERSION 3.21)
project(myapp C)
add_subdirectory(rayclay)                      # the RayClay folder
add_executable(myapp main.c)
target_link_libraries(myapp PRIVATE rayclay)
```

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/myapp                                  # Visual Studio: build\Release\myapp.exe
```

You need CMake 3.21 (3.24 on macOS with the SDL3 host) and a C11 compiler; your code needs only
C99.

- `-DRC_WINDOW_BACKEND=SDL3`: SDL3, not GLFW. GLFW lacks an input method on Wayland and
  press-and-hold accents on macOS, and on X11 composes in the field only on-the-spot (ibus).
- `-DRC_GFX_BACKEND=gl`: OpenGL, not Direct3D 11 (Windows) or Metal (macOS).
- `-DRC_NO_NET=ON`: networking compiled out; its calls answer UNAVAILABLE.

A compile-time knob ([api.md](api.md)) goes on RayClay's target, so the library sees
it too: `target_compile_definitions(rayclay PUBLIC RC_NO_DEFAULT_APP_ICON)`. `RC_NO_UI_HELPERS`,
`RC_NO_STYLE` and `RC_NO_COLOR_PALETTE` trim only your code's view: put them on `myapp` alone.

To compile the implementation yourself, link `rayclay_impl_deps`, not `rayclay`, and add one C
file (`C_EXTENSIONS ON`; under Metal, a `.m` without `-fobjc-arc`) holding only:

```c
#define RAYCLAY_IMPLEMENTATION
#include "rayclay.h"
```

C++ needs C++20, which linking RayClay sets when `project()` names CXX, and designated
initializers in declaration order.

## Several windows, the titlebar and the icon

<!-- compile: c -->
```c
static RC_Window *panel;

static void panel_layout(RC_Window *w, void *user) { (void)w; (void)user; rcTextL("Panel"); }

static void layout(RC_App *app, void *user) {
    (void)user;
    if (panel && rcWindowRelease(panel))       /* true once it has closed or failed */
        panel = NULL;
    if (!panel && rcChildWindowsSupported() && rcButton("detach", "Detach")) {
        RC_WindowOptions o = { .title = "Panel", .owner = rcAppMainWindow(app),
                               .layoutCallback = panel_layout };
        panel = rcAppOpenWindow(app, &o);
    }
}
```

`rcAppOpenWindow` returns a record: read `rcWindowState`. The user's close (the X, Alt+F4, Cmd+Q,
the titlebar's close chip, a Windows log-off) asks first, unlike `rcAppRequestClose(app)` and a stop
signal: the app's event handler hears `RC_APP_EVENT_CLOSE_REQUESTED`, and
`rcWindowCancelClose(rcAppMainWindow(app))` keeps the app open while you ask "Save changes?"; a child's own close asks its
`stateCallback` with `RC_WINDOW_CLOSING`. Wayland refuses window placement, always-on-top and
taskbar hiding; give every window there the same kind of `.frame` (the OS's, or RayClay's).

`RC_AppOptions.frame = RC_WINDOW_FRAME_TITLEBAR` swaps the system frame (a Mac's traffic lights
too) for RayClay's titlebar, its controls at the right unless `titlebar.controlsLeft`.
`titlebar.className` styles the band and its title, which follows `rcWindowSetTitle` unless
`titlebar.title` is set. With `RC_WINDOW_FRAME_NONE` you draw the band: `titlebar.height` tall
inside `rcUnzoomed()`, tagged `.id = RC_ID_WINDOW_DRAG`, holding `rcWindowControls()`, with
`.noDrag` on your widgets in it. `iconBytes` or
`iconPath` sets the taskbar, window or Dock icon; Wayland desktops mostly use the installed
`.desktop` file's.

## What each system needs

- Linux: build with `pkg-config xorg-dev libwayland-dev libxkbcommon-dev libgl1-mesa-dev`
  (Debian/Ubuntu) on the oldest distribution you support, for glibc. Users need X11 and OpenGL
  libraries (a console program too, unless built `-DRC_GUI=OFF`), CA certificates for https,
  `libasound.so.2` for sound and media, and a desktop portal or zenity for file dialogs.
- macOS: Xcode command line tools; users need the macOS `CMAKE_OSX_DEPLOYMENT_TARGET` names or
  later (unset, the build Mac's own).
- Windows: MSVC or MinGW-w64; `rcFetch` and `rcWebSocket` need Windows 8.1.

Linux networking builds only for x86-64 with GCC or Clang. When the configure prints
`rayclay_netprofile: not built`, a program calling `rcFetch`, `rcWebSocket` or `rcUpdateCheck` needs
`RC_NO_NET` on its target, set under `if(NOT TARGET rayclay_netprofile)`. From macOS 15.4
a program with no Info.plist is refused plain `http://` to a public or `.localhost` name
(`localhost` passes): use https, or embed an Info.plist allowing the host. `rcMediaOpen` plays
through the system player on macOS and Windows 8+ (H.264/AAC MP4, MP3, M4A, WAV; a Windows N edition needs
its Media Feature Pack). Linux plays files, not URLs: WAV, MP3, FLAC, Ogg Vorbis and WebM (8-bit
VP8 or VP9 with Opus). For a video every desktop plays, open the MP4 with `.alt` naming its WebM.

A relative path reads the working directory, then the executable's folder or a Mac bundle's
`Contents/Resources`. To ship a Windows app without its console, set `WIN32_EXECUTABLE ON` on
`myapp`, and with MSVC link `/ENTRY:mainCRTStartup`. There, and in a Mac app opened from Finder,
stderr reaches no one: keep diagnostics with `rcSetLogSink`. From Windows 10 1903, linking
`rayclay` makes the process code page UTF-8, so `fopen` and `argv` take RayClay's paths; a MinGW
app embedding its own manifest opts out with target property `RC_NO_UTF8_MANIFEST`. On macOS, a C
app whose plugins link against it, that calls `dlsym` on itself or that passes its own
`-exported_symbols_list` sets target property `ENABLE_EXPORTS ON`.

`rcUpdateCheck` reads a manifest you host and sign with `rc-update-publish.py` (beside ex13).
`RC_UpdateResult.action` says what `rcUpdateApply` does: INSTALL (only with `.publicKey`) swaps in
a signed `exe` or Mac `app-zip` where the program can replace itself, or stages a Windows
`installer` for `rcUpdateRelaunch` to start at exit; OPEN sends anything else (a `page`, as for an
MSI) to the browser. Store, package-manager, elevated and App Management installs get a `page`.

## A program with no GUI

Include `rayclay.h`, configure with `-DRC_GUI=OFF` and link what you use; no window host is built.
It runs on Linux, macOS, Windows and the web, and builds for Android and iOS.

| Link | Holds |
|---|---|
| `rayclay::json` | Base, JSON |
| `rayclay::net` | Base, networking |

Without CMake, define `RAYCLAY_NO_GUI` beside `RAYCLAY_IMPLEMENTATION`. On Android a relative path
reads the working directory only, not `assets/`, and your loop lends networking the process's
`JavaVM *` (never a `JNIEnv *`) with `rcFetchAttach(.javaVm = vm)` on the thread that calls
`rcFetch`, before its first request.

Wait for an answer with `rcFetchWait`, and call `rcFetchShutdown()` before the program ends:
`while (rcFetchResult(t).state == RC_FETCH_PENDING && rcFetchWait(10) >= 0) {}`. On the web it
returns -1 at once: return to the browser instead.

## Headless and CI runs

```bash
RAYCLAY_MAX_FRAMES=3 ./build/myapp     # 3 frames, then exit 0
RAYCLAY_MAX_SECONDS=3 ./build/myapp    # 3 s, still rendering on demand
RAYCLAY_WINDOW_PLATFORM=x11 RAYCLAY_MAX_SECONDS=3 xvfb-run -a ./build/myapp   # Linux CI
RAYCLAY_RENDER_MODE=continuous ./build/myapp    # draw every frame
RAYCLAY_FIXED_DT=0.016 ./build/myapp            # rcAppTime advances per frame, deterministic
RAYCLAY_SAFE_INSETS=44,0,34,0 ./build/myapp     # top,right,bottom,left, to try a phone's insets
```

A test reads `rcGetElementBox` and posts input with OS tools (`xdotool`).

With no display `rcRunApp` returns 1. RayClay logs mistakes and runs on, so fail a run
on its warnings (the window system's excepted):

```bash
./build/myapp 2>&1 | grep -v 'rc_window (host):' | grep -q 'RAYCLAY\[\(WARNING\|ERROR\)\]' && exit 1
```
