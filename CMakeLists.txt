# ==============================================================================
#  RayClay - one command: cmake -B build && cmake --build build --parallel 4
#
#  Builds the selected vendored window host, then librayclay as a static library from a
#  generated two-line implementation TU over the single rayclay.h. Consume via
#  add_subdirectory() and link rayclay::rayclay. On a checkout that carries
#  examples/ (the 'examples' branch), the examples build too.
# ==============================================================================
cmake_minimum_required(VERSION 3.21)

# Refuse an in-source build - it scatters CMakeCache.txt / CMakeFiles/ and the
# host binary tree through the source. Checked before project() so the configure
# aborts before writing the cache; the REALPATH compare also catches `cmake -B .`
# and a symlinked root.
get_filename_component(_rc_src_real "${CMAKE_SOURCE_DIR}" REALPATH)
get_filename_component(_rc_bin_real "${CMAKE_BINARY_DIR}" REALPATH)
if(_rc_src_real STREQUAL _rc_bin_real)
    message(FATAL_ERROR
        "In-source builds are not allowed. Build out-of-source instead, e.g.\n"
        "    cmake -B build && cmake --build build --parallel 4\n"
        "Remove the stray CMakeCache.txt + CMakeFiles/ this attempt just created first.")
endif()

# rayclay.h is the single source of truth for the version; parse it so a release
# never needs to edit this file.
foreach(_part MAJOR MINOR PATCH)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/rayclay.h" _rc_line
         REGEX "^#define[ \t]+RC_VERSION_${_part}[ \t]+[0-9]+")
    if(NOT _rc_line)
        message(FATAL_ERROR "rayclay.h: '#define RC_VERSION_${_part} <n>' not found")
    endif()
    string(REGEX MATCH "[0-9]+" _rc_ver_${_part} "${_rc_line}")
endforeach()

project(rayclay
    VERSION ${_rc_ver_MAJOR}.${_rc_ver_MINOR}.${_rc_ver_PATCH}
    LANGUAGES C)
message(STATUS "RayClay ${PROJECT_VERSION}")

set(RC_IOS OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(RC_IOS ON)
endif()
set(RC_DESKTOP_TARGETS_DEFAULT ${PROJECT_IS_TOP_LEVEL})
if(ANDROID OR RC_IOS)
    set(RC_DESKTOP_TARGETS_DEFAULT OFF)
endif()

# Internal build selection, matching the modular library. No runtime selector
# or host types are exposed in the public declarations.
if(ANDROID OR RC_IOS OR EMSCRIPTEN)
    set(RC_WINDOW_BACKEND_DEFAULT SDL3)
else()
    set(RC_WINDOW_BACKEND_DEFAULT GLFW)
endif()
set(RC_WINDOW_BACKEND "${RC_WINDOW_BACKEND_DEFAULT}" CACHE STRING
    "Window host: GLFW (desktop default) or SDL3 (desktop option; mobile and web)")
set_property(CACHE RC_WINDOW_BACKEND PROPERTY STRINGS GLFW SDL3)
if(NOT RC_WINDOW_BACKEND MATCHES "^(GLFW|SDL3)$")
    message(FATAL_ERROR "Unknown RC_WINDOW_BACKEND: ${RC_WINDOW_BACKEND} (GLFW or SDL3)")
endif()
if((ANDROID OR RC_IOS OR EMSCRIPTEN) AND NOT RC_WINDOW_BACKEND STREQUAL "SDL3")
    message(FATAL_ERROR "Android, iOS and the web require RC_WINDOW_BACKEND=SDL3")
endif()
message(STATUS "rayclay: window host ${RC_WINDOW_BACKEND}")

set(OpenGL_GL_PREFERENCE GLVND)
if(PROJECT_IS_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Editors resolve includes - system headers included - from a compile database,
# not from this file. Without one an editor falls back to guessing and marks
# every #include as unresolvable while the build itself is perfectly fine. The
# presets set this too, but only some of them, and not the plain
# `cmake -S . -B build` path. Top-level only, for the same reason as the
# build-type default above: a consumer embedding RayClay owns that decision for
# their own tree, and a library forcing a database on them would overreach.
if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# RC_SIZE_OPT - the size-first build posture, opt-in and OFF by default.
#
# WHY IT IS HERE AND NOT ONLY IN THE FLAGS BELOW. Section-GC is unconditional a
# few lines down because a single-object library is useless without it. This
# option is what remains once that is already on: link-time optimisation across
# the whole tree. Measured on the shipped header, gcc 15.3.1, Release, x86-64, on
# the do-nothing app: 833,536 -> 783,248 bytes stripped, a saving of 50,288 (6.0%).
#
# STBTT_STATIC IS NOT PART OF THIS OPTION. rayclay.h defines it for its own
# bundled copy of stb_truetype, so EVERY consumer of rayclay.h gets that
# internal linkage whether or not they opt in - which is
# the point, because the single header is the primary shipping artifact and is
# the build that would otherwise pay full price. Measured on a -O3 -DNDEBUG
# implementation object: .text 678,259 -> 643,891,
# a saving of 34,368 B, with global stbtt_ symbols 59 -> 0. THE SYMBOL COUNT IS
# THE ORACLE, not the bytes: a byte delta alone could mean a define never
# reached the compiler, while 59 -> 0 proves the LINKAGE changed.
# The `add_compile_definitions(STBTT_STATIC)` below is therefore now REDUNDANT
# for the shipped header rather than load-bearing. It is kept because the header
# guards with `!defined(STBTT_STATIC)`, so a consumer predefining it costs
# nothing and eats no -Wmacro-redefined, and because removing a define is a
# consumer-visible change that buys zero bytes.
# WARN: the SIZE OF THE WIN DEPENDS ON THE CONSUMER'S LINK, so quote the pair
# and never one number - a consumer who uses THIS build file section-GCs
# unconditionally a few lines down and has therefore already discarded most of
# the unreachable stb, so their saving is small; a consumer who drops rayclay.h
# into a build with no section-GC is the one who sees the full amount.
#
# It is applied DIRECTORY-WIDE and BEFORE the host add_subdirectory(), deliberately:
# most of the bytes are in the vendored window library, not in rayclay's own
# object, and a posture set after the subdirectory would miss them entirely.
#
# LTO is PROBED, never assumed. check_ipo_supported answers for this compiler
# and generator, and a toolchain without it gets the rest of the posture plus a
# line saying so - an option that silently delivers less than it promises is
# worse than one that refuses, because it is believed.
#
# MSVC already links /OPT:REF,ICF and compiles /Gy, which is why the GNU/Clang
# section flags are skipped there. STBTT_STATIC and LTO are not applied on that
# toolchain, and this says so rather than no-op an option a developer set.
option(RC_SIZE_OPT "Aggressive binary-size build posture (LTO + section GC + static stb_truetype)" OFF)

if(RC_SIZE_OPT AND NOT MSVC)
    add_compile_options(-ffunction-sections -fdata-sections)
    add_compile_definitions(STBTT_STATIC)
    if(APPLE)
        add_link_options(-Wl,-dead_strip)
    else()
        add_link_options(-Wl,--gc-sections)
    endif()
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _rc_ipo_ok OUTPUT _rc_ipo_msg)
    if(_rc_ipo_ok)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    else()
        message(STATUS "RC_SIZE_OPT: LTO unavailable (${_rc_ipo_msg}); section GC + static stb only.")
    endif()
elseif(RC_SIZE_OPT AND MSVC)
    message(STATUS "RC_SIZE_OPT: MSVC already section-GCs by default; STBTT_STATIC "
                   "and LTO are not applied on this toolchain.")
endif()

# OpenGL is the desktop renderer. On web it is WebGL2 through sokol's GLES3
# backend, supplied by the browser, so nothing is looked up there.
if(NOT EMSCRIPTEN AND NOT ANDROID AND NOT RC_IOS)
    find_package(OpenGL REQUIRED)
endif()

# Both dependency trees stay external to the header. Only the selected host
# is built. SDL3's emscripten port owns the web canvas, with no windowing shim.
# The sources under SDL3/ are upstream release-3.4.16 with nothing edited, so
# every file there compares byte-for-byte against that release. What was left
# out is whole directories this build never reads - upstream's own test/ and
# examples/, its docs and its IDE project files. They are built here static
# except on Android, with unused subsystems forced OFF, so a
# consumer pays for windowing, input and the GL context and nothing else.
#
# A parent application may already own an SDL3 build. Its subsystem and linkage
# choices are not ours to override in the shared cache, so an existing
# SDL3::SDL3 target wins and nothing below runs. RC_SDL_SOURCE_DIR is the middle
# ground for a consumer who wants THEIR SDL3 source tree built with THESE knobs.
if(RC_WINDOW_BACKEND STREQUAL "SDL3" AND NOT TARGET SDL3::SDL3)
    set(RC_SDL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SDL3"
        CACHE PATH "SDL3 sources (defaults to the shipped copy under SDL3/)")
    if(NOT EXISTS "${RC_SDL_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "RC_SDL_SOURCE_DIR='${RC_SDL_SOURCE_DIR}' has no CMakeLists.txt. "
            "RayClay ships its window host under SDL3/ beside this file; either "
            "restore that directory from a fresh checkout, or point this at an "
            "intact SDL3 source tree.")
    endif()
    if(ANDROID)
        set(SDL_SHARED ON CACHE BOOL "" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    else()
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
    endif()
    # Everything RayClay does not call is OFF, and FORCED so a stale cache entry
    # from an upstream default cannot quietly re-enable a subsystem. This is
    # already the minimal configuration: re-measuring with every remaining knob
    # explicitly OFF produces a byte-identical binary, so nothing here
    # is optional tuning. (The names are built by interpolation, so a grep for a
    # literal `set(SDL_AUDIO` finds nothing - read the loop.)
    foreach(_sdl_unused TESTS TEST_LIBRARY EXAMPLES INSTALL AUDIO JOYSTICK
                        HAPTIC HIDAPI SENSOR CAMERA RENDER GPU POWER DIALOG TRAY)
        set(SDL_${_sdl_unused} OFF CACHE BOOL "" FORCE)
    endforeach()
    # RayClay does not inhibit the screensaver - that is a fullscreen-video and
    # game concern, not a GUI toolkit's. Saying so removes libXScrnSaver from
    # the packages a Linux consumer must install: SDL treats a requested-but-
    # unfindable X11 feature as a HARD configure error, not a warning.
    set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
    add_subdirectory("${RC_SDL_SOURCE_DIR}" sdl EXCLUDE_FROM_ALL)
    # SDL's EGL headers require C11; change only the targets created here.
    foreach(_sdl_target SDL3-shared SDL3-static SDL_uclibc)
        if(TARGET ${_sdl_target})
            set_target_properties(${_sdl_target} PROPERTIES C_STANDARD 11)
        endif()
    endforeach()
endif()

# GLFW includes the vendored nobar Wayland move/resize patch. An existing
# parent target wins, just as it does for SDL3.
if(RC_WINDOW_BACKEND STREQUAL "GLFW" AND NOT TARGET glfw)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(glfw glfw EXCLUDE_FROM_ALL)
    set(RC_GLFW_IS_VENDORED_PATCHED ON)
endif()

# The library IS the header; generate the one implementation TU so no consumer
# ever writes it by hand.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/rayclay_impl.c"
     "#define RAYCLAY_IMPLEMENTATION\n#include \"rayclay.h\"\n")
add_library(rayclay STATIC "${CMAKE_CURRENT_BINARY_DIR}/rayclay_impl.c")
add_library(rayclay::rayclay ALIAS rayclay)
target_include_directories(rayclay PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# The bundled default application icon is inert on the web (the page owns the
# favicon and the SDL host compiles its icon call out), yet its four PNG arrays
# stayed alive through section GC in every page. Measured 2026-09-13:
# hello.wasm 793,546 -> 777,613 B.
if(EMSCRIPTEN)
    target_compile_definitions(rayclay PRIVATE RC_NO_DEFAULT_APP_ICON)
endif()

# The implementation TU needs C11 (with the toolchain's default extensions);
# consumers including rayclay.h need only C99. /utf-8: the sources hold UTF-8
# string literals MSVC would otherwise decode in the system codepage.
set_target_properties(rayclay PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
if(ANDROID)
    set_target_properties(rayclay PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
if(MSVC)
    target_compile_options(rayclay PRIVATE /utf-8)
endif()

# C++ CONSUMERS GET THE DIALECT FROM US, and rayclay.h's own #error block
# PROMISES they do: "A CMake consumer gets the dialect automatically ... and
# never sees this" - the line below is what keeps that promise true.
# clay.h needs C99, C++20 or MSVC;
# g++ and clang++ both still default to gnu++17, so a consumer who adds a C++
# target and links rayclay::rayclay would hit clay's #error naming a vendored
# dependency they never asked for.
# GUARDED on the C++ compiler being loaded: this project() is LANGUAGES C, and
# requesting a CXX feature with no CXX compiler is a configure error. A consumer
# whose top level is C-only and who adds C++ later gets no dialect from us and
# is not left guessing - the header's #error names the flag.
if(CMAKE_CXX_COMPILER_LOADED)
    target_compile_features(rayclay INTERFACE cxx_std_20)
endif()

# THE SAME C++ CONSUMER GETS -Wno-missing-field-initializers FROM US, and it is
# INTERFACE because the consumer's own translation units are where it fires.
# Every element macro (rcBox, rcRow, rcColumn ...) expands to a designated
# initialiser of RC_ComponentOptions that names the fields you wrote and
# value-initialises the rest; those zeros are the DSL's contract, not an
# omission. In C neither gcc nor clang warns about it. In C++ g++ warns under
# -Wextra, and clang does so from 18 (-Wmissing-designated-field-
# initializers, a subgroup of the same flag, llvm/llvm-project#56628), so a C++
# consumer building at -Wextra -Werror takes a HARD ERROR from rayclay.h at its
# first rcBox without it. Measured 2026-09-15 on g++ 15.3.1 and clang++ 21.1.8:
# both red, both green with this one flag. It has to be INTERFACE: the same flag
# set PRIVATE on this library's own object never reaches the TU that expands
# the macro.
# Scoped to C++ so a C consumer's warning set is untouched, and to the families
# that spell the flag: cl.exe rejects any -W<name> as D8021. No
# CMAKE_CXX_COMPILER_LOADED guard: a generator expression on a language that is
# never enabled is simply never true. Keep this line and the guarded feature
# above together - removing either alone changes what a C-only consumer builds.
target_compile_options(rayclay INTERFACE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wno-missing-field-initializers>)

# Section-GC, on by default: the library is ONE object file, so without it every
# consumer binary would carry the whole framework. MSVC Release already links
# with /OPT:REF,ICF.
if(NOT MSVC)
    target_compile_options(rayclay PRIVATE -ffunction-sections -fdata-sections)
    if(APPLE)
        target_link_options(rayclay INTERFACE -Wl,-dead_strip)
    else()
        target_link_options(rayclay INTERFACE -Wl,--gc-sections)
    endif()
endif()

# A dependency-only interface target: the platform libraries the single header's
# implementation links against, WITHOUT the implementation itself. The library
# links it PUBLIC, so consumers inherit them. It is also the target to link if
# you compile your own copy of the implementation - defining
# RAYCLAY_IMPLEMENTATION in one of your own translation units rather than
# linking librayclay - because you then still need the libraries the window and
# titlebar backends resolve against. Section-GC drops that code on ELF but not
# on PE, so on Windows those libraries are genuinely needed at link time.
add_library(rayclay_deps INTERFACE)

if(RC_WINDOW_BACKEND STREQUAL "SDL3")
    target_compile_definitions(rayclay PRIVATE RC_WINDOW_BACKEND_SDL3=1)
    target_link_libraries(rayclay_deps INTERFACE SDL3::SDL3)
else()
    target_link_libraries(rayclay_deps INTERFACE glfw)
endif()

# --------------------------------------------------------------------------
#  THE OPTIONAL SEAM ENTRIES (the RCI_WINDOW_HAS_* window capabilities)
# --------------------------------------------------------------------------
#  KEEP EVERY ONE OF THESE, BECAUSE THE CONSEQUENCE OF DROPPING ONE IS
#  INVISIBLE. Each macro guards BOTH a seam helper and its only caller, and
#  rayclay.h defaults it to 0. So an absent definition here does not fail to
#  configure, does not fail to build and does not fail to link - it silently
#  compiles the feature OUT of the library you get, and nothing says so.
#
#  What each one switches on, enumerated in full so it can be read rather than
#  inferred from the code below:
#    * rcWindowSetSize             resizing the window from the application
#    * the Wayland libdecor hint   the guard against a decoration-plugin abort
#    * rcGetKeyboardInsets         the mobile soft-keyboard insets
#    * the iOS rotation background the surfaces UIKit shows mid-rotation
#  Every one of them is INERT unless its define is set here.

if(NOT EMSCRIPTEN AND NOT ANDROID AND NOT RC_IOS)
    # rcWindowSetSize: a desktop window is the app's to resize, a phone screen
    # and a page's canvas are not. Both desktop seams define it, so this keys on
    # the PLATFORM, not on the backend.
    target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_SET_SIZE=1)

    # rcWindowRaise: bringing a window to the front and giving it the keyboard
    # only means something where windows are stacked and one of them holds
    # focus. Both desktop seams have the call (glfwFocusWindow, SDL_RaiseWindow),
    # so this keys on the PLATFORM, not on the backend.
    target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_RAISE=1)

    # rcWindowSetTitle: the NATIVE window title - what the task switcher, the
    # taskbar and the window list read. A phone surface has no name a user sees,
    # and on the web the title belongs to the DOCUMENT rather than to a window.
    # Both desktop seams have the call (glfwSetWindowTitle, SDL_SetWindowTitle),
    # so this keys on the PLATFORM, not on the backend.
    target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_SET_TITLE=1)

    # The Wayland libdecor hint: an app that draws its own titlebar must not make
    # the host bring up a decoration plugin, because on Wayland that plugin can
    # abort the process before any window appears. LINUX ONLY - both desktop
    # seams honour it there; Windows and macOS have no libdecor to speak to.
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_OWN_DECORATIONS_HINT=1)
    endif()
endif()

if(RC_WINDOW_BACKEND STREQUAL "SDL3")
    if(RC_IOS)
        # The surfaces UIKit shows around the frame during a rotation; only the
        # iOS host has such surfaces, so only it compiles the call.
        target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_SET_BACKGROUND=1)
    endif()
    if(RC_IOS OR ANDROID)
        # rcGetKeyboardInsets: only a host with a soft keyboard defines it.
        target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_KEYBOARD_INSETS=1)
        # The library raises and lowers the keyboard with editor focus.
        target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_SOFT_KEYBOARD=1)
    endif()
endif()

if(ANDROID)
    target_link_libraries(rayclay_deps INTERFACE GLESv3 m log ${CMAKE_DL_LIBS})
elseif(RC_IOS)
    target_link_libraries(rayclay_deps INTERFACE "-framework OpenGLES")
elseif(NOT EMSCRIPTEN)
    target_link_libraries(rayclay_deps INTERFACE OpenGL::GL)

    if(WIN32)
        # The titlebar backend names these with #pragma comment(lib,...), which
        # only MSVC honours; listing them keeps MinGW/clang links working.
        target_link_libraries(rayclay_deps INTERFACE comctl32 dwmapi shell32 user32 gdi32)
    endif()

    if(APPLE)
        # nobar's Cocoa backend, compiled into the single header's titlebar
        # implementation, talks to the Objective-C runtime directly. Named here
        # rather than inherited from the window host's own frameworks, because a
        # dependency a library only borrows from its neighbour breaks the day
        # the neighbour is swapped.
        target_link_libraries(rayclay_deps INTERFACE "-framework AppKit" "-framework Foundation")
    endif()

    if(UNIX AND NOT APPLE)
        # Mirror the selected host's display backends into nobar. A parent
        # host with unspecified flags keeps both, with a visible warning.
        if(RC_WINDOW_BACKEND STREQUAL "GLFW")
            set(_rc_x11_option GLFW_BUILD_X11)
            set(_rc_wayland_option GLFW_BUILD_WAYLAND)
        else()
            set(_rc_x11_option SDL_X11)
            set(_rc_wayland_option SDL_WAYLAND)
        endif()
        set(_rc_host_x11 ON)
        set(_rc_host_wayland ON)
        foreach(_backend x11 wayland)
            if(DEFINED ${_rc_${_backend}_option})
                set(_rc_host_${_backend} ${${_rc_${_backend}_option}})
            else()
                message(WARNING "${_rc_${_backend}_option} is unresolved; keeping its titlebar glue")
            endif()
        endforeach()
        if(RC_WINDOW_BACKEND STREQUAL "GLFW")
            target_compile_definitions(rayclay PRIVATE
                RC_GLFW_X11=$<BOOL:${_rc_host_x11}>
                RC_GLFW_WAYLAND=$<BOOL:${_rc_host_wayland}>)
            # WAYLAND DRAG AND RESIZE NEED RAYCLAY'S PATCHED GLFW, and the block
            # above that adds it says whether you got it. glfwWaylandStartWindowMove
            # and its resize twin are a RayClay patch: they are in NO released GLFW
            # and in no GLFW header. Defining it against a stock GLFW is a LINK
            # ERROR naming two symbols you will not find anywhere.
            #
            # THERE IS NO HIT-TEST FALLBACK ON THE GLFW SEAM, so do not expect the
            # titlebar to degrade into a draggable window. `rci_window_set_hit_test`
            # there is `(void)w; (void)cb; (void)user; return false;`, and GLFW
            # exposes no hit-test API that could make it do otherwise. The SDL3
            # backend IS different - SDL_SetWindowHitTest exists, so that seam does
            # degrade to a compositor-dragged window. GLFW does not.
            #
            # THE ACTUAL CONSEQUENCE, because you will hit it and no gate can see it:
            # a custom-titlebar app on Wayland, built against an unpatched GLFW you
            # brought yourself, has NO move path at all - not a degraded one. nobar
            # reports no MOVE capability and the seam's fallback is a stub, so the
            # window cannot be dragged by its own title bar. Two real remedies:
            # build against the vendored GLFW (which carries the patch), or set
            # nativeFrame so the compositor draws a frame it will move.
            if(RC_GLFW_IS_VENDORED_PATCHED)
                target_compile_definitions(rayclay PRIVATE RC_GLFW_WAYLAND_MOVE=1)
                message(STATUS "rayclay: Wayland window move/resize ON (RayClay's patched GLFW)")
            else()
                message(STATUS "rayclay: Wayland window move/resize OFF - an existing `glfw` "
                               "target was found, and RayClay's Wayland move patch is not in "
                               "any released GLFW. A custom titlebar then has NO drag path on "
                               "Wayland - there is no fallback. Remove your glfw target to use "
                               "the shipped one, or set nativeFrame so the compositor draws a "
                               "frame it will move.")
            endif()
        endif()
        if(_rc_host_x11)
            find_package(X11 REQUIRED)
            target_link_libraries(rayclay_deps INTERFACE ${X11_LIBRARIES})
        else()
            target_compile_definitions(rayclay PRIVATE NTB_NO_X11)
        endif()
        target_link_libraries(rayclay_deps INTERFACE m ${CMAKE_DL_LIBS})
    endif()
endif()

target_link_libraries(rayclay PUBLIC rayclay_deps)

# Implementation-only compile definitions: they matter only when compiling the
# single header's implementation, so they stay PRIVATE - a consumer that merely
# includes rayclay.h never sees them. If you compile the implementation
# yourself, set them on that target too, so it is built exactly as the library.
if(NOT EMSCRIPTEN)
    if(MINGW)
        # msvcrt printf has no %zu; select MinGW's ISO-conforming stdio instead.
        target_compile_definitions(rayclay PRIVATE __USE_MINGW_ANSI_STDIO=1)
    endif()
endif()

# ------------------------------------------------------------------------------
#  Examples - present only on the 'examples' branch. On 'main' the sentinel below
#  is absent and this block is inert, so one CMakeLists serves both branches.
#
#  The sentinel is a source file rather than the directory, deliberately. A
#  directory test asks whether anything landed under examples/, which is weaker
#  than asking whether the sources this block enumerates are actually present.
# ------------------------------------------------------------------------------
option(RC_BUILD_EXAMPLES "Build the RayClay examples" ${RC_DESKTOP_TARGETS_DEFAULT})

if(RC_BUILD_EXAMPLES AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/ex00_hello/hello.c)
    add_executable(rayclay_ex00_hello                   examples/ex00_hello/hello.c)
    add_executable(rayclay_ex01_1980s_gui               examples/ex01_1980s_gui/main.c)
    add_executable(rayclay_ex02_1990s_gui               examples/ex02_1990s_gui/main.c)
    add_executable(rayclay_ex03_2000s_gui               examples/ex03_2000s_gui/main.c)
    add_executable(rayclay_ex04_2010s_gui               examples/ex04_2010s_gui/main.c)
    add_executable(rayclay_ex05_2020s_gui               examples/ex05_2020s_gui/main.c)
    # The one C++ example. It needs a C++ compiler the C examples do not, so it is
    # added only when one exists: a C-only toolchain still builds every other
    # example, and the ONE list below gains this target only when it was made.
    # The cxx_std_20 INTERFACE guard above ran before CXX was enabled here, so the
    # dialect is pinned on the target itself; the -Wno-missing-field-initializers
    # INTERFACE option is a generator expression and reaches it at generate time.
    include(CheckLanguage)
    check_language(CXX)
    if(CMAKE_CXX_COMPILER)
        enable_language(CXX)
        add_executable(rayclay_ex06_cpp_kanban           examples/ex06_cpp_kanban/main.cpp)
        set_target_properties(rayclay_ex06_cpp_kanban PROPERTIES
            CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    endif()
    add_executable(rayclay_ex10_rayclay_widgets_gallery examples/ex10_rayclay_widgets_gallery/main.c)
    # ex11 is the one example with no web arm: it SCANS A DIRECTORY and WRITES .h
    # files, which a browser page cannot do, so it has no web target.
    if(NOT EMSCRIPTEN)
        add_executable(rayclay_ex11_rayclay_icon_converter  examples/ex11_rayclay_icon_converter/main.c)
    endif()
    add_executable(rayclay_ex12_rayclay_inspector       examples/ex12_rayclay_inspector/main.c)
    # The ex20-ex29 band: fully functional demo apps, single-source like the
    # decade lineup rather than split like the bench suite (nothing benchmarks
    # them, so there is no reason for a second TU).
    add_executable(rayclay_ex20_system_monitor          examples/ex20_system_monitor/src/main.c)
    add_executable(rayclay_ex21_data_explorer           examples/ex21_data_explorer/src/main.c)
    # ex22 loads three real typefaces at runtime from examples/assets/fonts/.
    # It needs NO injected defines: its source defaults RC_EX22_FONT_DIR and
    # RC_EX22_ICON to repo-root-relative paths, which is exactly this tree's
    # layout, so it runs from the repo root like ex10 does.
    add_executable(rayclay_ex22_docs_reader             examples/ex22_docs_reader/src/main.c)
    # ex23 virtualizes 100,000 rows from a generated dataset: single source, no
    # assets, no local header, so it needs nothing beyond the target.
    add_executable(rayclay_ex23_issue_inbox             examples/ex23_issue_inbox/src/main.c)
    # ex24 loads an .svg at RUNTIME and draws the same artwork beside its
    # generated-header twin. Its artwork is examples/assets/icons/, shipped
    # beside it, so it needs no injected define.
    add_executable(rayclay_ex24_svg_live                examples/ex24_svg_live/src/main.c)
    # The ex30+ band: examples INTENDED for a phone (portrait-first, 393x852).
    # They are included because they are the worked examples for building a
    # mobile application with RayClay.
    #
    # DECLARED OFF THE WEB, the same way ex11 is and for a reason of the same
    # kind: both are marked NO_WEB, so neither has a page, and a target
    # the web arm builds but rc_web_example() never names would emit a bare
    # <target>.js with no shell and no card in the hub - a build artifact nothing
    # can open. ex11 has no page because a browser has no filesystem; these two
    # have none because a browser is not a phone. A page for them is a DESIGN
    # decision (a portrait viewport in a desktop tab), not a consequence of this
    # export, so it stays unmade here.
    #
    # They need no injected define and no asset: one translation unit each, over
    # headers beside it under src/app/ and src/platform/, which the foreach below
    # puts on the include path from the target's own first source. ex32's
    # "icons/..." headers resolve through examples/assets, and the shared
    # header they themselves include by a bare name sits beside them in that
    # same directory, so it resolves too.
    if(NOT EMSCRIPTEN)
        add_executable(rayclay_ex30_keyboard_form       examples/ex30_keyboard_form/src/main.c)
        add_executable(rayclay_ex32_tab_navigator       examples/ex32_tab_navigator/src/main.c)
    endif()
    # The bench suite: production-shaped multi-file apps (GUI TU + thin runner).
    add_executable(rayclay_bench_messenger
                   examples/bench/messenger/main.c
                   examples/bench/messenger/messenger_app.c)
    add_executable(rayclay_bench_notes
                   examples/bench/notes/main.c
                   examples/bench/notes/notes_app.c)
    add_executable(rayclay_bench_trader
                   examples/bench/trader/main.c
                   examples/bench/trader/trader_app.c)
    add_executable(rayclay_bench_platformer
                   examples/bench/platformer/main.c
                   examples/bench/platformer/platformer_app.c)
    add_executable(rayclay_bench_gallery
                   examples/bench/gallery/main.c
                   examples/bench/gallery/gallery_app.c)
    add_executable(rayclay_bench_opsdash
                   examples/bench/opsdash/main.c
                   examples/bench/opsdash/opsdash_app.c)

    if(NOT EMSCRIPTEN)
        # Repo-relative on purpose: an absolute path would bake the build machine's
        # home directory into the binary. Run ex10 from the repo root.
        target_compile_definitions(rayclay_ex10_rayclay_widgets_gallery PRIVATE
            RC_DEMO_LOGO="examples/assets/logos/rayclay-logo-1024.png")

        # The converter uses portable C string fns; silence MSVC's "use strcpy_s".
        target_compile_definitions(rayclay_ex11_rayclay_icon_converter PRIVATE
            $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)
    else()
        # ----------------------------------------------------------------------
        #  The web arm. THE SAME SOURCES - no example has a web variant; what
        #  differs is the link, the page shell, and how runtime assets reach the
        #  program (a browser has no filesystem, so they are preloaded into a
        #  virtual /assets root at the SAME paths the desktop arm reads).
        # ----------------------------------------------------------------------
        set(RC_DEMO_PNG   "${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/logos/rayclay-logo-1024.png")
        set(RC_EX22_FONTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/fonts")
        set(RC_WEB_SHELL  "${CMAKE_CURRENT_SOURCE_DIR}/examples/web/shell.html")

        # THE PAGE NAME IS SET AT BUILD TIME AND MUST NEVER BE A COPY-TIME RENAME:
        # emcc bakes the wasm filename into the generated .js, so renaming the
        # output afterwards leaves <name>.js fetching the OLD .wasm and the page
        # dies with a blank canvas and no error.
        function(rc_web_example target out)
            set_target_properties(${target} PROPERTIES OUTPUT_NAME ${out} SUFFIX ".html")
            # GROWABLE_ARRAYBUFFERS=0: emcc 6.0.2 turned it on by default alongside
            # ALLOW_MEMORY_GROWTH, backing heap views with a RESIZABLE ArrayBuffer,
            # which Chrome's WebGL rejects ("texSubImage2D: ... must not be
            # resizable") - the glyph-atlas upload then faults and every page
            # renders blank.
            # SDL3 arrives through the rayclay target's PUBLIC link (the vendored
            # tree compiled for wasm) and needs no windowing flag here.
            target_link_options(${target} PRIVATE
                -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3
                -sALLOW_MEMORY_GROWTH=1 -sGROWABLE_ARRAYBUFFERS=0 -sSTACK_SIZE=8MB
                -sENVIRONMENT=web -sMALLOC=emmalloc
                --shell-file "${RC_WEB_SHELL}")
            # emcc bakes the shell into <out>.html at LINK time and CMake does not
            # track it as an input; relink when it changes.
            set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${RC_WEB_SHELL}")
        endfunction()

        rc_web_example(rayclay_ex05_2020s_gui               dashboard)
        if(TARGET rayclay_ex06_cpp_kanban)
            rc_web_example(rayclay_ex06_cpp_kanban           kanban)
        endif()
        rc_web_example(rayclay_ex00_hello                   hello)
        rc_web_example(rayclay_ex01_1980s_gui               gui1980s)
        rc_web_example(rayclay_ex02_1990s_gui               gui1990s)
        rc_web_example(rayclay_ex03_2000s_gui               gui2000s)
        rc_web_example(rayclay_ex04_2010s_gui               gui2010s)
        rc_web_example(rayclay_ex10_rayclay_widgets_gallery gallery)
        rc_web_example(rayclay_ex12_rayclay_inspector       inspector)
        rc_web_example(rayclay_ex20_system_monitor          sysmon)
        rc_web_example(rayclay_ex21_data_explorer           explorer)
        rc_web_example(rayclay_ex22_docs_reader             reader)
        rc_web_example(rayclay_ex23_issue_inbox             inbox)
        rc_web_example(rayclay_ex24_svg_live                svg)
        rc_web_example(rayclay_bench_messenger              messenger)
        rc_web_example(rayclay_bench_notes                  notes)
        rc_web_example(rayclay_bench_trader                 trader)
        rc_web_example(rayclay_bench_platformer             platformer)
        rc_web_example(rayclay_bench_gallery                photos)
        rc_web_example(rayclay_bench_opsdash                opsdash)

        # Load-bearing: `SHELL:` is required, not decoration. CMake de-duplicates repeated
        # identical link options, so N bare `--preload-file` tokens collapse to ONE
        # while all N paths survive - measured on the generated link.txt: 4
        # requested, 1 flag emitted, 4 paths emitted. The three orphaned paths then
        # reach emcc as bare arguments and it reports them as missing INPUT FILES,
        # which reads like an absent asset and is not. `SHELL:` is CMake's
        # documented escape and keeps each flag adjacent to its value.
        target_compile_definitions(rayclay_ex10_rayclay_widgets_gallery PRIVATE
            RC_DEMO_LOGO="/assets/rayclay-logo-1024.png")
        target_link_options(rayclay_ex10_rayclay_widgets_gallery PRIVATE
            "SHELL:--preload-file ${RC_DEMO_PNG}@/assets/rayclay-logo-1024.png")

        # The trailing slash matters: the source concatenates a filename onto it.
        # RC_EX22_ICON IS NULL HERE AND THE PNG IS NOT PRELOADED: an application icon
        # has no effect on the web (the page owns the favicon; the SDL host compiles
        # its icon call out), so the 52,071-byte logo was downloaded by every visitor
        # and ignored. Measured 2026-09-13: reader.data 111,655 -> 59,584 B. NULL,
        # not undefined, because theme.h's fallback is the desktop path, absent in
        # MEMFS, and a named-but-unreadable icon warns on every page load.
        target_compile_definitions(rayclay_ex22_docs_reader PRIVATE
            RC_EX22_FONT_DIR="/assets/fonts/"
            RC_EX22_ICON=NULL)
        target_link_options(rayclay_ex22_docs_reader PRIVATE
            "SHELL:--preload-file ${RC_EX22_FONTS}/Lato-Regular.ttf@/assets/fonts/Lato-Regular.ttf"
            "SHELL:--preload-file ${RC_EX22_FONTS}/Lato-Bold.ttf@/assets/fonts/Lato-Bold.ttf"
            "SHELL:--preload-file ${RC_EX22_FONTS}/AdwaitaMono-Regular.ttf@/assets/fonts/AdwaitaMono-Regular.ttf")

        # ex24 opens its .svg sources at RUN time, so the web arm must ship them.
        # Deliberately not a directory preload: examples/assets/icons/ also holds
        # the GENERATED .h headers and a licence, none of which ex24 opens, and a
        # .data bundle is downloaded by every visitor.
        # rayclay-logo is FIRST because it is what ex24 OPENS ON - the default
        # artwork, not an icon in the grid. Omitting it leaves the web arm's BY
        # PATH and HANDLE panes empty on first run and shows the amber "that path
        # did not open" state, while desktop is fine because it resolves the
        # literal from the repo. The page still LOADS fine (it opens on
        # chart-column, which is packaged), so nothing looks wrong until the
        # developer clicks the first card - and the app then advises "launch from
        # the repository root", which cannot apply in a browser and reads as the
        # developer's own mistake.
        # This list must name EVERY SVG the example can open. A name absent here
        # is simply not packaged, and the example then reports a file it cannot
        # find - which reads as the developer's own mistake, not a packaging gap.
        set(RC_EX24_SVGS rayclay-logo
                         chart-column expand folder maximize minimize minus
                         panel-left panel-right settings shrink x)
        foreach(_svg IN LISTS RC_EX24_SVGS)
            target_link_options(rayclay_ex24_svg_live PRIVATE
                "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/icons/${_svg}.svg@/assets/icons/${_svg}.svg")
        endforeach()
        target_compile_definitions(rayclay_ex24_svg_live PRIVATE
            RC_EX24_SVG_DIR="/assets/icons/")

        # Stage the demo hub as the build's index.html so a local build is the same
        # tree a deploy would publish. A COPY, never a rename of a generated page -
        # see the rc_web_example() note above.
        add_custom_target(rayclay_web_hub ALL
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${CMAKE_CURRENT_SOURCE_DIR}/examples/web/hub.html
                    ${CMAKE_CURRENT_BINARY_DIR}/index.html
            COMMENT "web: staging the demo hub as index.html")
    endif()

    # ONE list, so the web arm cannot drift from the desktop one by a target.
    # ex11 and the ex30+ band are appended rather than listed because they exist
    # only off the web - the same guard their add_executable() calls sit behind,
    # so a target can never be linked on an arm that did not declare it.
    set(_rc_examples
        rayclay_ex00_hello rayclay_ex01_1980s_gui rayclay_ex02_1990s_gui
        rayclay_ex03_2000s_gui rayclay_ex04_2010s_gui rayclay_ex05_2020s_gui
        rayclay_ex10_rayclay_widgets_gallery
        rayclay_ex12_rayclay_inspector rayclay_ex20_system_monitor
        rayclay_ex21_data_explorer rayclay_ex22_docs_reader
        rayclay_ex23_issue_inbox rayclay_ex24_svg_live
        rayclay_bench_messenger rayclay_bench_notes rayclay_bench_trader
        rayclay_bench_platformer rayclay_bench_gallery
        rayclay_bench_opsdash)
    if(NOT EMSCRIPTEN)
        list(APPEND _rc_examples rayclay_ex11_rayclay_icon_converter
                                 rayclay_ex30_keyboard_form
                                 rayclay_ex32_tab_navigator)
    endif()
    if(TARGET rayclay_ex06_cpp_kanban)
        list(APPEND _rc_examples rayclay_ex06_cpp_kanban)
    endif()

    foreach(_ex IN LISTS _rc_examples)
        target_link_libraries(${_ex} PRIVATE rayclay)
        # examples/bench holds the bench suite's shared contract (bench_app.h);
        # examples/assets resolves the "icons/..." includes.
        target_include_directories(${_ex} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/bench
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/assets)
        # The rule is the one the desktop examples already follow, stated the same
        # way: an example laid out as a small application keeps its sources under
        # examples/<name>/src and includes them ROOT-RELATIVE, so that root must
        # be on its path. Derived from the target's own first source, so a
        # conversion needs no edit in EITHER file and the two cannot drift.
        get_target_property(_rc_ex_sources ${_ex} SOURCES)
        list(GET _rc_ex_sources 0 _rc_ex_first_source)
        get_filename_component(_rc_ex_src_dir "${_rc_ex_first_source}" DIRECTORY)
        target_include_directories(${_ex} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/${_rc_ex_src_dir})
        target_compile_options(${_ex} PRIVATE $<$<C_COMPILER_ID:MSVC>:/utf-8>)
    endforeach()
else()
    # ------------------------------------------------------------------------------
    #  SAY SO. A branch without examples is a legitimate state - the published
    #  `main` carries the library alone and the demos live on the `examples`
    #  branch - but until this else() existed the build simply omitted them in
    #  silence. A contributor ran the first command the docs give them,
    #  `cmake --preset desktop && cmake --build --preset desktop`, got exit 0,
    #  and had NOTHING RUNNABLE: zero executables and a preset whose own
    #  displayName promises "library + examples".
    #  A silently degraded build is the worst of the three possible outcomes -
    #  worse than a hard failure, which at least tells you - so this converts it
    #  into one actionable sentence. It costs nothing on the branches that do
    #  carry examples, because the if() arm is unchanged.
    # ------------------------------------------------------------------------------
    if(RC_BUILD_EXAMPLES)
        message(STATUS
            "RayClay: no examples on this branch - building the library only. "
            "The demos live on the 'examples' branch: "
            "git clone --branch examples <this repo>")
    endif()
endif()

# ------------------------------------------------------------------------------
#  Tests, if a test/ directory is ever present beside this file. Neither
#  published branch carries one, so - exactly like the examples block above -
#  the EXISTS check leaves this inert and one CMakeLists serves every checkout.
# ------------------------------------------------------------------------------
option(RC_BUILD_TESTS "Build the RayClay differential tests" ${RC_DESKTOP_TARGETS_DEFAULT})

if(RC_BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt
   AND NOT EMSCRIPTEN AND NOT ANDROID AND NOT RC_IOS)
    enable_testing()
    add_subdirectory(test)
endif()
