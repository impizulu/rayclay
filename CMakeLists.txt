# RayClay - build with: cmake -B build && cmake --build build --parallel 4
# In your project: add_subdirectory(<this directory>) and link rayclay::rayclay, or link
# rayclay_impl_deps where your own .c defines RAYCLAY_IMPLEMENTATION. docs/desktop.md

cmake_minimum_required(VERSION 3.21)

get_filename_component(_rc_src_real "${CMAKE_CURRENT_SOURCE_DIR}" REALPATH)
get_filename_component(_rc_bin_real "${CMAKE_CURRENT_BINARY_DIR}" REALPATH)
if(_rc_src_real STREQUAL _rc_bin_real)
    message(FATAL_ERROR
        "In-source builds are not allowed. Build out-of-source instead, e.g.\n"
        "    cmake -B build && cmake --build build --parallel 4\n"
        "Remove the stray CMakeCache.txt + CMakeFiles/ this attempt just created first.")
endif()

foreach(_part MAJOR MINOR PATCH)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/rayclay.h" _rc_line
         REGEX "^#define[ \t]+RC_VERSION_${_part}[ \t]+[0-9]+")
    if(NOT _rc_line)
        message(FATAL_ERROR "rayclay.h: '#define RC_VERSION_${_part} <n>' not found")
    endif()
    string(REGEX MATCH "[0-9]+" _rc_ver_${_part} "${_rc_line}")
endforeach()

if(CMAKE_HOST_APPLE AND NOT DEFINED CMAKE_SYSTEM_NAME AND NOT DEFINED CMAKE_TOOLCHAIN_FILE
   AND "$ENV{MACOSX_DEPLOYMENT_TARGET}" STREQUAL "")
    get_property(_rc_osx_dt_help CACHE CMAKE_OSX_DEPLOYMENT_TARGET PROPERTY HELPSTRING)
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET OR ("${CMAKE_OSX_DEPLOYMENT_TARGET}" STREQUAL ""
       AND _rc_osx_dt_help MATCHES "^Minimum OS X version to target"))
        execute_process(COMMAND xcrun --sdk macosx --show-sdk-path OUTPUT_VARIABLE _rc_osx_sdk
                        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(EXISTS "${_rc_osx_sdk}/SDKSettings.json")
            file(READ "${_rc_osx_sdk}/SDKSettings.json" _rc_osx_json)
            string(JSON _rc_osx_min ERROR_VARIABLE _rc_osx_err
                   GET "${_rc_osx_json}" SupportedTargets macosx MinimumDeploymentTarget)
        endif()
        if(NOT _rc_osx_min MATCHES "^[0-9]+(\\.[0-9]+)*$")
            set(_rc_osx_min "12.0")
        elseif(_rc_osx_min VERSION_LESS 11.0)
            set(_rc_osx_min "11.0")
        endif()
        set(CMAKE_OSX_DEPLOYMENT_TARGET "${_rc_osx_min}" CACHE STRING
            "Oldest macOS the programs start on (empty: the build Mac's own version)" FORCE)
    endif()
    foreach(_rc_v _rc_osx_dt_help _rc_osx_sdk _rc_osx_json _rc_osx_min _rc_osx_err)
        unset(${_rc_v})
    endforeach()
endif()

project(rayclay
    VERSION ${_rc_ver_MAJOR}.${_rc_ver_MINOR}.${_rc_ver_PATCH}
    LANGUAGES C)
message(STATUS "RayClay ${PROJECT_VERSION}")

set(RC_IOS OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(RC_IOS ON)
endif()
include(mobile/ios/rc_ios_app.cmake OPTIONAL)
set(RC_DESKTOP_TARGETS_DEFAULT ${PROJECT_IS_TOP_LEVEL})
if(ANDROID OR RC_IOS)
    set(RC_DESKTOP_TARGETS_DEFAULT OFF)
endif()

if(PROJECT_IS_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

option(RC_SIZE_OPT "Aggressive binary-size build posture (LTO + section GC + static stb_truetype)" OFF)

if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/Gw>)
endif()

if(NOT MSVC)
    add_compile_options($<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>>:-O2>)
endif()

if(MINGW)
    set(_rc_section_flags -ffunction-sections)
else()
    set(_rc_section_flags -ffunction-sections -fdata-sections)
endif()

if(RC_SIZE_OPT AND NOT MSVC)
    add_compile_options(${_rc_section_flags})
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
        if(MINGW AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
            add_compile_options(-ffat-lto-objects)
        endif()
    else()
        message(STATUS "RC_SIZE_OPT: LTO unavailable (${_rc_ipo_msg}); section GC + static stb only.")
    endif()
elseif(RC_SIZE_OPT AND MSVC)
    message(STATUS "RC_SIZE_OPT: MSVC already section-GCs by default; STBTT_STATIC "
                   "and LTO are not applied on this toolchain.")
endif()

option(RC_NO_NET
       "Compile out networking (rcFetch, rcWebSocket); on Linux and Android also skip building the network profile"
       OFF)

option(RC_SOUND "Compile in sounds (rcLoadSound, rcPlaySound); OFF compiles them out" ON)

option(RC_NO_WEBP "Compile out WebP decoding; off Apple also skip building libwebp in third_parties/mediaprofile/" OFF)

option(RC_GUI
       "The GUI (rayclay::rayclay): OpenGL, the window host, X11 or Wayland. OFF: rayclay::json and rayclay::net only, with no GUI discovery"
       ON)

if(WIN32)
    set(_rc_utf8_xml [=[<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <activeCodePage xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">UTF-8</activeCodePage>
    </windowsSettings>
  </application>
</assembly>]=])
    if(MSVC)
        set(_rc_utf8_source "${CMAKE_CURRENT_BINARY_DIR}/rayclay-utf8.manifest")
        file(CONFIGURE OUTPUT "${_rc_utf8_source}" @ONLY CONTENT "${_rc_utf8_xml}\n")
    else()
        string(REPLACE "\"" "\"\"" _rc_utf8_rc "${_rc_utf8_xml}")
        string(REPLACE "\n" "\\n\"\n  \"" _rc_utf8_rc "${_rc_utf8_rc}")
        set(_rc_utf8_source "${CMAKE_CURRENT_BINARY_DIR}/rayclay-utf8.rc")
        file(CONFIGURE OUTPUT "${_rc_utf8_source}" @ONLY
                       CONTENT "1 24\nBEGIN\n  \"${_rc_utf8_rc}\\n\"\nEND\n")
    endif()
endif()

if(MINGW AND CMAKE_C_COMPILER_ID STREQUAL "GNU" AND NOT DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _rc_pe_lto OUTPUT _rc_pe_lto_msg LANGUAGES C)
    if(NOT _rc_pe_lto)
        message(STATUS "rayclay: no LTO (${_rc_pe_lto_msg}); a MinGW app links the whole library.")
    endif()
endif()

function(_rc_impl_target tgt)
    set_target_properties(${tgt} PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS ON)
    if(ANDROID)
        set_target_properties(${tgt} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
    if(MSVC)
        target_compile_options(${tgt} PRIVATE /utf-8)
    else()
        target_compile_options(${tgt} PRIVATE ${_rc_section_flags})
        if(APPLE)
            target_link_options(${tgt} INTERFACE -Wl,-dead_strip)
            if(NOT RC_IOS AND NOT CMAKE_GENERATOR STREQUAL "Xcode")
                set(_rc_exe "$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>")
                set(_rc_keeps "$<BOOL:$<TARGET_PROPERTY:ENABLE_EXPORTS>>")
                target_link_options(${tgt} INTERFACE
                    "$<$<AND:${_rc_exe},$<LINK_LANGUAGE:C>,$<NOT:${_rc_keeps}>>:-Wl,-no_exported_symbols>")
            endif()
        else()
            target_link_options(${tgt} INTERFACE -Wl,--gc-sections)
        endif()
    endif()
    if(_rc_pe_lto)
        set_target_properties(${tgt} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
            INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON
            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
        target_compile_options(${tgt} PRIVATE
            $<$<CONFIG:Release,MinSizeRel,RelWithDebInfo>:-ffat-lto-objects>)
        target_link_options(${tgt} INTERFACE
            $<$<CONFIG:Release,MinSizeRel,RelWithDebInfo>:-flto-partition=one>)
    elseif(MINGW AND CMAKE_C_COMPILER_ID STREQUAL "GNU" AND RC_SIZE_OPT AND _rc_ipo_ok)
        target_link_options(${tgt} INTERFACE -flto-partition=one)
    endif()
    if(MINGW)
        target_compile_definitions(${tgt} PRIVATE __USE_MINGW_ANSI_STDIO=1)
    endif()
endfunction()

add_library(rayclay_deps INTERFACE)

if(EMSCRIPTEN)
    set(_rc_net_js "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/rayclay_net.js")
    file(CONFIGURE OUTPUT "${_rc_net_js}" @ONLY CONTENT [==[
addToLibrary({
    $rciNet: {
        live: new Map(),
        text: (p) => {
            var s = '';

            for (var c; (c = HEAPU8[p++]);)
                s += String.fromCharCode(c);
            return s;
        },
        line: (e, text) => {
            var n = Math.min(text.length, e.size);

            for (var i = 0; i < n; i++)
                HEAPU8[e.stage + i] = text.charCodeAt(i);
            return {{{ makeDynCall('ippp', 'e.call.header') }}}(e.req, e.stage, n);
        },
        body: async (e, data) => {
            for (var at = 0, n, room; at < data.length; at += n) {
                n = Math.min(data.length - at, e.size);
                while (e.stream && !(room = {{{ makeDynCall('ppp', 'e.call.room') }}}(e.req, n))) {
                    await new Promise((wake) => e.wake = wake);
                    if (e.ctrl.signal.aborted)
                        throw 0;
                }
                n = e.stream ? Math.min(n, room) : n;
                HEAPU8.set(data.subarray(at, at + n), e.stage);
                if (!{{{ makeDynCall('ippp', 'e.call.append') }}}(e.req, e.stage, n))
                    return false;
            }
            return true;
        },
        end: (e) => {
            if (e.done)
                return false;
            e.done = true;
            rciNet.live.delete(e.req);
            e.ctrl.abort();
            return true;
        },
        run: async (e, request, max) => {
            var response = await fetch(request), reader, chunk;

            e.reading = true;
            for (var [name, value] of response.headers)
                if (!rciNet.line(e, name + ': ' + value))
                    return rciNet.end(e) && {{{ makeDynCall('vp', 'e.call.tooLarge') }}}(e.req);
            if ((!e.stream && !e.head && response.body && response.type == 'basic' &&
                 !response.headers.has('content-encoding') && response.headers.get('content-length') > max) ||
                !{{{ makeDynCall('ipi', 'e.call.open') }}}(e.req, response.status))
                return rciNet.end(e) && {{{ makeDynCall('vp', 'e.call.tooLarge') }}}(e.req);
            reader = response.body && response.body.getReader();
            while (reader && !(chunk = await reader.read()).done)
                if (!await rciNet.body(e, chunk.value))
                    return rciNet.end(e) && {{{ makeDynCall('vp', 'e.call.tooLarge') }}}(e.req);
            return rciNet.end(e) && {{{ makeDynCall('vpi', 'e.call.answered') }}}(e.req, response.status);
        },
    },
    rci_net_web_fetch__deps: ['$rciNet'],
    rci_net_web_fetch__sig: 'vppppppppppii',
    rci_net_web_fetch: (req, calls, stage, size, method, url, headers, body, length, max, refuse, stream) => {
        var [header, open, room, append, answered, tooLarge, unreached, cut, malformed] =
            [0, 1, 2, 3, 4, 5, 6, 7, 8].map((i) => {{{ makeGetValue('calls', 'i * ' + POINTER_SIZE, '*') }}});
        var e = { req, stage, size, stream,
                  call: { header, open, room, append, answered, tooLarge, unreached, cut, malformed },
                  ctrl: new AbortController(), done: false, reading: false, wake: null, head: false };
        var list = [], request;

        for (var i = 0, p; (p = {{{ makeGetValue('headers', 'i * ' + POINTER_SIZE, '*') }}}); i++) {
            var line = rciNet.text(p), colon = line.indexOf(':');

            list.push([line.slice(0, colon), line.slice(colon + 1)]);
        }
        method = rciNet.text(method);
        e.head = method == 'HEAD';   /* no body follows, whatever Content-Length says */
        try {
            request = new Request(rciNet.text(url), {
                method,
                headers: list,
                body: length ? HEAPU8.slice(body, body + length)
                             : ['POST', 'PUT', 'PATCH'].includes(method) ? new Uint8Array(0) : null,
                credentials: 'same-origin',
                cache: 'no-store',
                redirect: refuse ? 'error' : 'follow',
                signal: e.ctrl.signal,
            });
        } catch (error) {
            {{{ makeDynCall('vp', 'malformed') }}}(req);
            return;
        }
        rciNet.live.set(req, e);
        rciNet.run(e, request, max).catch(() => rciNet.end(e) &&
            {{{ makeDynCall('vp', '(e.reading ? cut : unreached)') }}}(req));
    },
    rci_net_web_abort__deps: ['$rciNet'],
    rci_net_web_abort__sig: 'vp',
    rci_net_web_abort: (req) => {
        var e = rciNet.live.get(req);

        e?.ctrl.abort();
        e?.wake?.();
    },
    rci_net_web_wake__deps: ['$rciNet'],
    rci_net_web_wake__sig: 'vp',
    rci_net_web_wake: (req) => rciNet.live.get(req)?.wake?.(),
    $rciWs: {
        live: new Map(),
        ascii: (p) => {
            var s = '';

            for (var c; (c = HEAPU8[p++]);)
                s += String.fromCharCode(c);
            return s;
        },
        /* UTF-8 text as the native arms send it: a default TextDecoder drops
           a leading U+FEFF. */
        utf8: (bytes) => new TextDecoder('utf-8', { ignoreBOM: true }).decode(bytes),
        put: (e, bytes) => {
            var n = Math.min(bytes.length, e.size);

            HEAPU8.set(bytes.subarray(0, n), e.stage);
            return n;
        },
        drop: (e) => {
            rciWs.live.delete(e.s);
            clearTimeout(e.timer);
            e.ws.onopen = e.ws.onmessage = e.ws.onerror = e.ws.onclose = null;
        },
        /* The browser has no event for bufferedAmount falling, so this looks
           again while bytes are unsent: it waits 4, 8, 16 ... 256 ms through
           looks that find nothing sent and starts over at 4 ms after one
           that does, so a peer that stopped reading costs four looks a
           second, not 250. */
        poll: (e) => {
            var done = e.queued - e.ws.bufferedAmount;

            e.timer = 0;
            if (done > e.reported) {
                {{{ makeDynCall('vpp', 'e.call.sent') }}}(e.s, done - e.reported);
                e.reported = done;
                e.wait = 0;
            }
            if (e.reported < e.queued && rciWs.live.has(e.s)) {
                e.wait = Math.min(Math.max(2 * e.wait, 4), 256);
                e.timer = setTimeout(() => rciWs.poll(e), e.wait);
            }
        },
    },
    rci_net_ws_web_open__deps: ['$rciWs'],
    rci_net_ws_web_open__sig: 'vpppppp',
    rci_net_ws_web_open: (s, calls, stage, size, url, protocols) => {
        var [opened, message, sent, closed] =
            [0, 1, 2, 3].map((i) => {{{ makeGetValue('calls', 'i * ' + POINTER_SIZE, '*') }}});
        var e = { s, stage, size, call: { opened, message, sent, closed }, ws: null, queued: 0, reported: 0,
                  timer: 0, wait: 0, opened: false, errored: false };
        var list = [];

        for (var i = 0, p; protocols && (p = {{{ makeGetValue('protocols', 'i * ' + POINTER_SIZE, '*') }}}); i++)
            list.push(rciWs.ascii(p));
        try {
            e.ws = new WebSocket(rciWs.ascii(url), list);
        } catch (error) {
            {{{ makeDynCall('vpiippi', 'closed') }}}(s, 0, 1006, 0, 0, error.name == 'SecurityError' ? 4 : 3);
            return;
        }
        e.ws.binaryType = 'arraybuffer';
        e.ws.onopen = () => {
            e.opened = true;
            {{{ makeDynCall('ipipp', 'e.call.opened') }}}(s, 0, e.stage,
                                                           rciWs.put(e, new TextEncoder().encode(e.ws.protocol)));
        };
        e.ws.onmessage = (event) => {
            var text = typeof event.data == 'string';
            var bytes = text ? new TextEncoder().encode(event.data) : new Uint8Array(event.data);
            var at = 0, n;

            do {
                n = Math.min(bytes.length - at, e.size);
                HEAPU8.set(bytes.subarray(at, at + n), e.stage);
                at += n;
                if ({{{ makeDynCall('ipiippp', 'e.call.message') }}}(s, text ? 0 : 1, at == bytes.length ? 1 : 0,
                                                                     bytes.length, e.stage, n))
                    return;
            } while (at < bytes.length);
        };
        e.ws.onerror = () => e.errored = true;
        e.ws.onclose = (event) => {
            var reason = new TextEncoder().encode(event.reason);

            rciWs.drop(e);
            if (event.wasClean && e.queued > e.reported)
                {{{ makeDynCall('vpp', 'e.call.sent') }}}(s, e.queued - e.reported);
            {{{ makeDynCall('vpiippi', 'e.call.closed') }}}(s, event.wasClean ? 1 : 0, event.code, e.stage,
                                                             rciWs.put(e, reason),
                                                             e.errored ? (e.opened ? 1 : 2) : 0);
        };
        rciWs.live.set(s, e);
    },
    rci_net_ws_web_send__deps: ['$rciWs'],
    rci_net_ws_web_send__sig: 'vpipp',
    rci_net_ws_web_send: (s, binary, bytes, length) => {
        var e = rciWs.live.get(s), data = HEAPU8.slice(bytes, bytes + length);

        if (!e)
            return;
        e.ws.send(binary ? data : rciWs.utf8(data));
        e.queued += length;
        if (!e.timer)
            e.timer = setTimeout(() => rciWs.poll(e), 0);
    },
    rci_net_ws_web_close__deps: ['$rciWs'],
    rci_net_ws_web_close__sig: 'vpipp',
    rci_net_ws_web_close: (s, code, reason, length) => {
        var e = rciWs.live.get(s);

        if (!e)
            return;
        try {
            code ? e.ws.close(code, rciWs.utf8(HEAPU8.slice(reason, reason + length))) : e.ws.close();
        } catch (error) {
            e.ws.close();
        }
    },
    rci_net_ws_web_abort__deps: ['$rciWs'],
    rci_net_ws_web_abort__sig: 'ip',
    rci_net_ws_web_abort: (s) => {
        var e = rciWs.live.get(s);

        if (!e)
            return 0;
        rciWs.drop(e);
        e.ws.close();
        return 1;
    },
});
]==])
    target_link_options(rayclay_deps INTERFACE "--js-library=${_rc_net_js}")
    set_property(TARGET rayclay_deps APPEND PROPERTY INTERFACE_LINK_DEPENDS "${_rc_net_js}")
endif()

get_property(_rc_np_added GLOBAL PROPERTY rayclay_netprofile_added)
if((CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID) AND NOT RC_NO_NET AND NOT TARGET rayclay_netprofile
   AND NOT _rc_np_added)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_parties/netprofile/CMakeLists.txt")
        add_subdirectory(third_parties/netprofile netprofile EXCLUDE_FROM_ALL)
    else()
        file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/rayclay.h" _rc_anchor
             REGEX "^#define[ \t]+RCI_FETCH_PROFILE_ANCHOR[ \t]+")
        string(REGEX REPLACE "^#define[ \t]+RCI_FETCH_PROFILE_ANCHOR[ \t]+([A-Za-z0-9_]+).*" "\\1"
               _rc_anchor "${_rc_anchor}")
        if(_rc_anchor)
            message(WARNING "rayclay: third_parties/netprofile/ is missing, so programs that call rcFetch, "
                            "rcWebSocket or rcUpdateCheck will not link (undefined reference to "
                            "'${_rc_anchor}'). Restore it, or set RC_NO_NET.")
        else()
            message(WARNING "rayclay: rcFetch, rcWebSocket and rcUpdateCheck programs will not link: "
                            "third_parties/netprofile/ is missing. Restore it, or set RC_NO_NET.")
        endif()
    endif()
endif()

foreach(_rc_part BASE JSON NET)
    string(TOLOWER ${_rc_part} _rc_low)
    set(_rc_tu "${CMAKE_CURRENT_BINARY_DIR}/rayclay_${_rc_low}_impl.c")
    file(CONFIGURE OUTPUT "${_rc_tu}" CONTENT
         "#define RAYCLAY_IMPLEMENTATION\n#define RCI_IMPL_PART_${_rc_part} 1\n#include \"rayclay.h\"\n")
    add_library(rayclay_${_rc_low} STATIC "${_rc_tu}")
    _rc_impl_target(rayclay_${_rc_low})
    if(RC_GUI OR NOT PROJECT_IS_TOP_LEVEL)
        set_target_properties(rayclay_${_rc_low} PROPERTIES EXCLUDE_FROM_ALL ON)
    endif()
endforeach()
add_library(rayclay::json ALIAS rayclay_json)
add_library(rayclay::net ALIAS rayclay_net)
target_include_directories(rayclay_base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rayclay_json PUBLIC rayclay_base)
target_link_libraries(rayclay_net PUBLIC rayclay_base)
set_property(TARGET rayclay_base APPEND PROPERTY COMPATIBLE_INTERFACE_STRING RC_BASE_OWNER)
set_property(TARGET rayclay_base PROPERTY INTERFACE_RC_BASE_OWNER rayclay_base)
if(RC_NO_NET)
    target_compile_definitions(rayclay_net PUBLIC RC_NO_NET)
endif()

function(_rc_component_cxx_floors)
    if(CMAKE_CXX_COMPILER_LOADED)
        target_compile_features(rayclay_json INTERFACE cxx_std_20)
        target_compile_features(rayclay_net INTERFACE cxx_std_20)
    endif()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _rc_component_cxx_floors)
target_compile_options(rayclay_json INTERFACE $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wno-missing-field-initializers>)
target_compile_options(rayclay_net INTERFACE $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wno-missing-field-initializers>)

if(ANDROID)
    target_link_libraries(rayclay_base PUBLIC log)
elseif(APPLE)
    target_link_libraries(rayclay_base PUBLIC "-framework CoreFoundation")
endif()
if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
    target_link_libraries(rayclay_json PUBLIC m)
    target_link_libraries(rayclay_net PUBLIC m)
endif()
if(RC_NO_NET)
elseif(UNIX AND NOT APPLE AND NOT ANDROID AND NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)
    target_link_libraries(rayclay_net PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
elseif(APPLE)
    target_link_libraries(rayclay_net PUBLIC "-framework Foundation")
endif()
if((CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID) AND NOT RC_NO_NET AND TARGET rayclay_netprofile)
    target_link_libraries(rayclay_net PUBLIC rayclay_netprofile)
endif()
if(EMSCRIPTEN)
    target_link_options(rayclay_net INTERFACE "--js-library=${_rc_net_js}")
    set_property(TARGET rayclay_net APPEND PROPERTY INTERFACE_LINK_DEPENDS "${_rc_net_js}")
endif()
if(WIN32)
    target_sources(rayclay_base INTERFACE
        "$<$<AND:$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>,$<NOT:$<BOOL:$<TARGET_PROPERTY:RC_NO_UTF8_MANIFEST>>>>:${_rc_utf8_source}>")
endif()

if(NOT RC_GUI)
    if(RC_BUILD_EXAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/ex00_hello/hello.c")
        message(FATAL_ERROR "rayclay: the examples are GUI programs: RC_BUILD_EXAMPLES=ON needs RC_GUI=ON. "
                            "An earlier configure of this build directory may have cached it ON: set "
                            "RC_BUILD_EXAMPLES=OFF, or use a fresh build directory, to build rayclay::json and "
                            "rayclay::net alone.")
    endif()
    if(RC_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")
        message(FATAL_ERROR "rayclay: the differential tests are built on the GUI: RC_BUILD_TESTS=ON needs RC_GUI=ON. "
                            "An earlier configure of this build directory may have cached it ON: set "
                            "RC_BUILD_TESTS=OFF, or use a fresh build directory, to build rayclay::json and "
                            "rayclay::net alone.")
    endif()
    add_library(rayclay INTERFACE)
    add_library(rayclay::rayclay ALIAS rayclay)
    add_library(rayclay_impl_deps INTERFACE)
    target_link_libraries(rayclay INTERFACE rayclay::gui_needs_RC_GUI_ON)
    target_link_libraries(rayclay_impl_deps INTERFACE rayclay::gui_needs_RC_GUI_ON)
    target_link_libraries(rayclay_deps INTERFACE rayclay::gui_needs_RC_GUI_ON)
    message(STATUS "rayclay: RC_GUI=OFF: rayclay::json and rayclay::net, no GUI discovery")
    return()
endif()

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

if(WIN32 OR APPLE)
    if(WIN32)
        set(_rc_gfx_moved d3d11)
        set(_rc_gfx_moved_what "Direct3D 11 became the Windows default")
        set(_rc_gfx_moved_why "no OpenGL context")
    elseif(RC_IOS)
        set(_rc_gfx_moved metal)
        set(_rc_gfx_moved_what "Metal became the iOS default")
        set(_rc_gfx_moved_why "no OpenGL ES context; about half the memory in use")
    else()
        set(_rc_gfx_moved metal)
        set(_rc_gfx_moved_what "Metal became the macOS default")
        set(_rc_gfx_moved_why "no OpenGL context; on Apple silicon, 4x MSAA in tile memory")
    endif()
    get_property(_rc_gfx_cached_type CACHE RC_GFX_BACKEND PROPERTY TYPE)
    if(DEFINED CACHE{CMAKE_CACHEFILE_DIR} AND NOT DEFINED CACHE{RCI_GFX_BACKEND_MOVE_SEEN}
       AND _rc_gfx_cached_type STREQUAL "STRING" AND RC_GFX_BACKEND STREQUAL "gl")
        message(WARNING
            "rayclay: this build tree was configured before ${_rc_gfx_moved_what}, "
            "and it keeps the gl it cached. Configure with "
            "-DRC_GFX_BACKEND=${_rc_gfx_moved} to take the default "
            "(${_rc_gfx_moved_why}), or with "
            "-DRC_GFX_BACKEND=gl to keep OpenGL. This warning is shown once.")
    endif()
    set(RCI_GFX_BACKEND_MOVE_SEEN 1 CACHE INTERNAL
        "rayclay: configured since this platform's default graphics API moved off OpenGL")
    set(RC_GFX_BACKEND_DEFAULT ${_rc_gfx_moved})
else()
    set(RC_GFX_BACKEND_DEFAULT gl)
endif()
set(RC_GFX_BACKEND "${RC_GFX_BACKEND_DEFAULT}" CACHE STRING
    "Graphics API: d3d11 (the Windows default), metal (the macOS and iOS default) or gl (OpenGL: the default elsewhere, the escape hatch on Windows, macOS and iOS)")
set_property(CACHE RC_GFX_BACKEND PROPERTY STRINGS gl d3d11 metal)
if(NOT RC_GFX_BACKEND MATCHES "^(gl|d3d11|metal)$")
    message(FATAL_ERROR "Unknown RC_GFX_BACKEND: ${RC_GFX_BACKEND} (gl, d3d11 or metal)")
endif()
if(RC_GFX_BACKEND STREQUAL "d3d11")
    if(NOT WIN32)
        message(FATAL_ERROR
            "RC_GFX_BACKEND=d3d11 is a Windows build; leave the knob at gl elsewhere")
    endif()
    add_compile_definitions(RC_GFX_BACKEND_D3D11=1)
elseif(RC_GFX_BACKEND STREQUAL "metal")
    if(NOT APPLE)
        message(FATAL_ERROR
            "RC_GFX_BACKEND=metal is an Apple build; elsewhere leave the knob at gl")
    endif()
    add_compile_definitions(RC_GFX_BACKEND_METAL=1)
elseif(WIN32 OR APPLE)
    add_compile_definitions(RC_GFX_BACKEND_GL=1)
endif()
message(STATUS "rayclay: graphics backend ${RC_GFX_BACKEND}")

set(OpenGL_GL_PREFERENCE GLVND)

if(NOT EMSCRIPTEN AND NOT ANDROID AND NOT RC_IOS AND NOT RC_GFX_BACKEND STREQUAL "metal")
    find_package(OpenGL REQUIRED)
endif()

set(_rc_vendored_no_warnings $<$<NOT:$<C_COMPILER_ID:MSVC>>:-w>)

function(_rc_vendored_c_flags out)
    set(_strip "-Werror|-Werror=[^ ]*|-pedantic-errors|[/-]WX")
    if(MSVC)
        string(APPEND _strip "|[/-]W[0-4]|[/-]Wall")
    endif()
    set(_flags " ${CMAKE_C_FLAGS} ")
    set(_prev "")
    while(NOT "${_flags}" STREQUAL "${_prev}")
        set(_prev "${_flags}")
        string(REGEX REPLACE " (${_strip}) " " " _flags "${_flags}")
    endwhile()
    string(STRIP "${_flags}" _flags)
    set(${out} "${_flags}" PARENT_SCOPE)
endfunction()

if(RC_WINDOW_BACKEND STREQUAL "SDL3" AND NOT TARGET SDL3::SDL3)
    set(_rc_sdl_shipped "${CMAKE_CURRENT_SOURCE_DIR}/third_parties/SDL3")
    set(RC_SDL_SOURCE_DIR "${_rc_sdl_shipped}"
        CACHE PATH "SDL3 sources (defaults to the shipped copy under third_parties/SDL3/)")
    get_filename_component(_rc_sdl_parent "${RC_SDL_SOURCE_DIR}" DIRECTORY)
    get_filename_component(_rc_sdl_name "${RC_SDL_SOURCE_DIR}" NAME)
    if(_rc_sdl_parent STREQUAL CMAKE_CURRENT_SOURCE_DIR AND _rc_sdl_name STREQUAL "SDL3")
        set_property(CACHE RC_SDL_SOURCE_DIR PROPERTY VALUE "${_rc_sdl_shipped}")
        message(STATUS "rayclay: RC_SDL_SOURCE_DIR now names third_parties/SDL3/; this build "
                       "folder had cached the shipped copy's earlier place at the root")
    endif()
    if(NOT EXISTS "${RC_SDL_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "RC_SDL_SOURCE_DIR='${RC_SDL_SOURCE_DIR}' has no CMakeLists.txt. "
            "RayClay ships its window host under third_parties/SDL3/ beside this file; either "
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
    foreach(_sdl_unused TESTS TEST_LIBRARY EXAMPLES INSTALL AUDIO JOYSTICK
                        HAPTIC HIDAPI SENSOR CAMERA RENDER GPU POWER DIALOG TRAY)
        set(SDL_${_sdl_unused} OFF CACHE BOOL "" FORCE)
    endforeach()
    if(NOT ANDROID AND NOT RC_IOS AND NOT EMSCRIPTEN)
        foreach(_sdl_desktop_extra IBUS DBUS LIBUDEV KMSDRM OFFSCREEN VULKAN OPENGLES)
            set(SDL_${_sdl_desktop_extra} OFF CACHE BOOL "" FORCE)
        endforeach()
    endif()
    if(RC_IOS)
        if(RC_GFX_BACKEND STREQUAL "metal")
            set(_rc_sdl_ios_gl OFF)
        else()
            set(_rc_sdl_ios_gl ON)
        endif()
        set(SDL_OPENGLES ${_rc_sdl_ios_gl} CACHE BOOL "" FORCE)
        set(SDL_VULKAN ${_rc_sdl_ios_gl} CACHE BOOL "" FORCE)
    endif()
    set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
    set(_rc_consumer_c_flags "${CMAKE_C_FLAGS}")
    _rc_vendored_c_flags(CMAKE_C_FLAGS)
    add_subdirectory("${RC_SDL_SOURCE_DIR}" sdl EXCLUDE_FROM_ALL)
    include(CheckCSourceCompiles)
    check_c_source_compiles(
        "#ifndef __FILE_NAME__\n#error no __FILE_NAME__\n#endif\nint main(void) { return 0; }\n"
        RC_CC_HAS_FILE_NAME)
    set(CMAKE_C_FLAGS "${_rc_consumer_c_flags}")
    set(_rc_sdl_lean SDL_LEAN_AND_MEAN SDL_HAVE_BLIT_N=1 SDL_DISABLE_STB)
    set(_rc_sdl_palette_guard "")
    if(EXISTS "${RC_SDL_SOURCE_DIR}/src/video/SDL_blit.c")
        file(STRINGS "${RC_SDL_SOURCE_DIR}/src/video/SDL_blit.c" _rc_sdl_palette_guard
             REGEX "else if \\(!SDL_ISPIXELFORMAT_INDEXED\\(surface->format\\)\\)")
    endif()
    if(NOT _rc_sdl_palette_guard)
        list(APPEND _rc_sdl_lean SDL_HAVE_BLIT_0=1 SDL_HAVE_BLIT_1=1)
    endif()
    foreach(_sdl_target SDL3-shared SDL3-static SDL_uclibc)
        if(TARGET ${_sdl_target})
            set_target_properties(${_sdl_target} PROPERTIES C_STANDARD 11)
            set_target_properties(${_sdl_target} PROPERTIES
                XCODE_ATTRIBUTE_OTHER_LIBTOOLFLAGS -no_warning_for_no_symbols)
            if(CMAKE_GENERATOR STREQUAL "Xcode")
                set_target_properties(${_sdl_target} PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
            endif()
            target_compile_options(${_sdl_target} PRIVATE ${_rc_vendored_no_warnings})
            if(MINGW)
                target_compile_options(${_sdl_target} PRIVATE -ffunction-sections)
            elseif(NOT MSVC)
                target_compile_options(${_sdl_target} PRIVATE -ffunction-sections -fdata-sections)
            endif()
            if(NOT RC_CC_HAS_FILE_NAME)
                target_compile_definitions(${_sdl_target} PRIVATE "SDL_FILE=\"SDL3\"")
            endif()
            target_compile_definitions(${_sdl_target} PRIVATE ${_rc_sdl_lean})
        endif()
    endforeach()
endif()

if(RC_WINDOW_BACKEND STREQUAL "GLFW" AND NOT TARGET glfw)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    set(_rc_consumer_c_flags "${CMAKE_C_FLAGS}")
    _rc_vendored_c_flags(CMAKE_C_FLAGS)
    add_subdirectory(third_parties/glfw glfw EXCLUDE_FROM_ALL)
    set(CMAKE_C_FLAGS "${_rc_consumer_c_flags}")
    target_compile_options(glfw PRIVATE ${_rc_vendored_no_warnings})
    if(MINGW)
        target_compile_options(glfw PRIVATE -ffunction-sections)
    elseif(NOT MSVC)
        target_compile_options(glfw PRIVATE -ffunction-sections -fdata-sections)
    endif()
    if(APPLE)
        target_compile_definitions(glfw PRIVATE RC_GLFW_COCOA_NSGL_ONLY=1)
    endif()
    set(RC_GLFW_IS_VENDORED_PATCHED ON)
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/rayclay_impl.c"
     "#define RAYCLAY_IMPLEMENTATION\n#include \"rayclay.h\"\n")
add_library(rayclay STATIC "${CMAKE_CURRENT_BINARY_DIR}/rayclay_impl.c")
add_library(rayclay::rayclay ALIAS rayclay)
target_include_directories(rayclay PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(EMSCRIPTEN)
    target_compile_definitions(rayclay PRIVATE RC_NO_DEFAULT_APP_ICON)
endif()

if(RC_GFX_BACKEND STREQUAL "metal")
    target_compile_options(rayclay PRIVATE -x objective-c -fno-objc-arc)
endif()
_rc_impl_target(rayclay)

if(CMAKE_CXX_COMPILER_LOADED)
    target_compile_features(rayclay INTERFACE cxx_std_20)
endif()

target_compile_options(rayclay INTERFACE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wno-missing-field-initializers>)

if(RC_NO_NET)
    target_compile_definitions(rayclay_deps INTERFACE RC_NO_NET)
endif()
if(NOT RC_SOUND)
    target_compile_definitions(rayclay_deps INTERFACE RC_SOUND=0)
endif()
if(RC_NO_WEBP)
    target_compile_definitions(rayclay_deps INTERFACE RC_NO_WEBP)
endif()

if(RC_WINDOW_BACKEND STREQUAL "SDL3")
    target_compile_definitions(rayclay PRIVATE RC_WINDOW_BACKEND_SDL3=1)
    target_link_libraries(rayclay_deps INTERFACE SDL3::SDL3)
else()
    target_link_libraries(rayclay_deps INTERFACE glfw)
endif()

if(RC_GLFW_IS_VENDORED_PATCHED)
    target_compile_definitions(rayclay PRIVATE RC_GLFW_DRAG=1)
endif()
if(RC_GLFW_IS_VENDORED_PATCHED AND NOT WIN32)
    target_compile_definitions(rayclay PRIVATE RC_GLFW_PREEDIT=1)
endif()

target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_FILE_DIALOG=1)

if(NOT EMSCRIPTEN AND NOT ANDROID AND NOT RC_IOS)
    target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_SET_SIZE=1)

    target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_RAISE=1)

    target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_SET_TITLE=1)

    target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_RESTORE_UNFOCUSED=1)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_OWN_DECORATIONS_HINT=1)
    endif()
endif()

if(WIN32 AND NOT RC_WINDOW_BACKEND STREQUAL "SDL3")
    target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_OLE_DROP_TARGET=1)
endif()

if(NOT RC_WINDOW_BACKEND STREQUAL "SDL3" OR (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID))
    target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_IME_SESSION=1)
endif()

if(RC_WINDOW_BACKEND STREQUAL "SDL3")
    if(RC_IOS)
        target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_SET_BACKGROUND=1)
        target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_IME_SHIFT=1)
        target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_KEYBOARD_MOTION=1)
        target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_TEXT_INTERACTION=1)
    endif()
    if(RC_IOS OR ANDROID)
        target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_KEYBOARD_INSETS=1)
        target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_SOFT_KEYBOARD=1)
        target_compile_definitions(rayclay PRIVATE RCI_WINDOW_HAS_STATUS_BAR=1)
    endif()
endif()

if(ANDROID)
    target_link_libraries(rayclay_deps INTERFACE GLESv3 android m log ${CMAKE_DL_LIBS})
elseif(RC_IOS)
    if(RC_GFX_BACKEND STREQUAL "metal")
        target_link_libraries(rayclay_deps INTERFACE "-framework Metal" "-framework QuartzCore"
                              "-framework CoreGraphics")
    else()
        target_link_libraries(rayclay_deps INTERFACE "-framework OpenGLES")
    endif()
    target_link_libraries(rayclay_deps INTERFACE "-framework Foundation"
                          "-Wl,-weak_framework,UniformTypeIdentifiers")
elseif(NOT EMSCRIPTEN)
    if(RC_GFX_BACKEND STREQUAL "d3d11")
        target_link_libraries(rayclay_deps INTERFACE d3d11 dxgi)
    elseif(RC_GFX_BACKEND STREQUAL "metal")
        target_link_libraries(rayclay_deps INTERFACE "-framework Metal" "-framework QuartzCore"
                              "-framework CoreGraphics")
    else()
        target_link_libraries(rayclay_deps INTERFACE OpenGL::GL)
    endif()

    if(WIN32)
        target_link_libraries(rayclay_deps INTERFACE comctl32 dwmapi shell32 user32 gdi32)
    endif()

    if(APPLE)
        target_link_libraries(rayclay_deps INTERFACE "-framework AppKit" "-framework Foundation")
        if(RC_WINDOW_BACKEND STREQUAL "GLFW")
            target_link_libraries(rayclay_deps INTERFACE "-framework IOKit"
                                  "-framework CoreFoundation")
        endif()
    endif()

    if(UNIX AND NOT APPLE)
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
            if(RC_GLFW_IS_VENDORED_PATCHED)
                target_compile_definitions(rayclay PRIVATE RC_GLFW_WAYLAND_MOVE=1)
                message(STATUS "rayclay: Wayland window move/resize ON (RayClay's patched GLFW)")
            else()
                message(STATUS "rayclay: Wayland window move/resize OFF - an existing `glfw` "
                               "target was found, and RayClay's Wayland move patch is not in "
                               "any released GLFW. A custom titlebar then has NO drag path on "
                               "Wayland - there is no fallback. Remove your glfw target to use "
                               "the shipped one, or keep RC_WINDOW_FRAME_OS so the "
                               "compositor draws a frame it will move.")
            endif()
        endif()
        if(_rc_host_x11)
            find_package(X11 REQUIRED)
            target_link_libraries(rayclay_deps INTERFACE ${X11_LIBRARIES})
        else()
            target_compile_definitions(rayclay PRIVATE NTB_NO_X11)
        endif()
        target_link_libraries(rayclay_deps INTERFACE m ${CMAKE_DL_LIBS})
        find_package(Threads REQUIRED)
        target_link_libraries(rayclay_deps INTERFACE Threads::Threads)
    endif()
endif()

if((CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID) AND NOT RC_NO_NET AND TARGET rayclay_netprofile)
    target_link_libraries(rayclay_deps INTERFACE rayclay_netprofile)
endif()

set(RC_MEDIAPROFILE_WEBP OFF)
if(NOT APPLE AND NOT RC_NO_WEBP)
    set(RC_MEDIAPROFILE_WEBP ON)
endif()
if((CMAKE_SYSTEM_NAME STREQUAL "Linux" OR RC_MEDIAPROFILE_WEBP) AND NOT TARGET rayclay_mediaprofile)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_parties/mediaprofile/CMakeLists.txt")
        add_subdirectory(third_parties/mediaprofile mediaprofile)
    else()
        message(WARNING "rayclay: third_parties/mediaprofile/ is missing, so programs that call rcMediaOpen will not link "
                        "(undefined reference to the media profile's anchor), and WebP is compiled out "
                        "(RC_NO_WEBP). Restore it.")
        if(RC_MEDIAPROFILE_WEBP)
            target_compile_definitions(rayclay_deps INTERFACE RC_NO_WEBP)
        endif()
    endif()
endif()
if(TARGET rayclay_mediaprofile)
    target_link_libraries(rayclay_deps INTERFACE rayclay_mediaprofile)
endif()

if(EMSCRIPTEN)
    set(_rc_web_js "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/rayclay_web.js")
    file(CONFIGURE OUTPUT "${_rc_web_js}" @ONLY CONTENT [==[
addToLibrary({
    $rciWebStore__deps: ['$FS', '$IDBFS', '$addRunDependency', '$removeRunDependency',
                         '$withStackSave', '$stringToUTF8OnStack'],
    $rciWebStore__postset: () => addAtPreRun('rciWebStore.load();'),
    $rciWebStore: {
        stores: null,
        held: {},
        notes: {},
        report: 0,
        warned: {},
        tick: 0,
        last: 0,
        asked: 0,
        wake: 0,
        refused: (name, e) => {
            var report = rciWebStore.report;

            if (!e || !report || rciWebStore.warned[name])
                return;
            rciWebStore.warned[name] = 1;
            withStackSave(() => {{{ makeDynCall('vpp', 'report') }}}(
                stringToUTF8OnStack(name), stringToUTF8OnStack(e.name || String(e))));
        },
        begin: (mount) => {
            var run = { at: ++rciWebStore.tick, next: 0 };

            (mount.rcRuns ||= []).push(run);
            return run;
        },
        end: (mount, run, ok) => {
            var asked = rciWebStore.asked, wake = rciWebStore.wake;

            mount.rcRuns = mount.rcRuns.filter((r) => r !== run);
            if (ok === undefined)
                return;
            if (ok && mount.rcDirty && mount.rcDirty < run.at)
                mount.rcDirty = run.next;
            mount.rcRefused = !ok;
            if (!asked)
                return;
            if (rciWebStore.answer(asked) == 2)
                rciWebStore.asked = 0;
            if (wake)
                {{{ makeDynCall('v', 'wake') }}}();
        },
        answer: (at) => {
            var stores = rciWebStore.stores, status = 2, mount;

            if (!at || at > rciWebStore.last)
                return 0;
            if (!stores)
                return 3;
            for (var s in stores) {
                mount = stores[s];
                if (mount && mount.rcDirty && mount.rcDirty < at) {
                    if (mount.rcRefused)
                        return 3;
                    status = 1;
                }
            }
            return status;
        },
        mount: (name) => {
            var stores = rciWebStore.stores, mount, root, setattr;

            stores[name] = 0;
            for (var s in stores)
                if (stores[s] && (s.startsWith(name + '/') || name.startsWith(s + '/')))
                    return null;
            try {
                FS.mkdirTree(name);
                root = FS.mount(IDBFS, { autoPersist: true }, name);
                mount = root.mount;
                setattr = root.node_ops.setattr;
                root.node_ops.setattr = (node, attr) => {
                    setattr(node, attr);
                    if (attr.size !== undefined) {
                        node.mtime = node.ctime = Date.now();
                        node.isModified = true;
                    }
                };
            } catch (e) {
                return null;
            }
            if (mount.idbPersistState !== 0) {
                FS.unmount(name);
                return null;
            }
            stores[name] = mount;
            return mount;
        },
        note: (name, entries, gone, held) => {
            var key = 'rcStoreLeave' + Math.random(), list = [], body = '', size = 0, value;
            var base = (path) => +held[path]?.timestamp, older, kept;
            var narrow = (bytes, chars = '') => {
                for (var i = 0; i < bytes.length; i += 0x8000)
                    chars += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
                return chars;
            };

            for (var path in entries)
                size += entries[path].contents?.length || 0;
            if (size > 0x500000)
                return;
            for (path of Object.keys(entries).sort()) {
                var e = entries[path], c = e.contents;
                var bytes = c ? new Uint8Array(c.buffer, c.byteOffset, c.length) : new Uint8Array();

                list.push([path, +e.timestamp, e.mode, bytes.length, e.link, base(path)]);
                body += narrow(bytes);
            }
            value = JSON.stringify([name, Date.now(), list, gone, gone.map(base)])
                .replace(/[^ -~]/g, (c) => '\\u' + c.charCodeAt(0).toString(16).padStart(4, '0'));
            value = narrow(new TextEncoder().encode(value + '\n')) + body;
            try {
                try {
                    localStorage.setItem(key, value);
                } catch (e) {
                    older = rciWebStore.notes[name] || [];
                    kept = older.map((old) => localStorage.getItem(old));
                    older.forEach((old) => localStorage.removeItem(old));
                    try {
                        localStorage.setItem(key, value);
                        rciWebStore.notes[name] = [];
                    } catch (again) {
                        older.forEach((old, i) => localStorage.setItem(old, kept[i]));
                        throw again;
                    }
                }
                (rciWebStore.notes[name] ||= []).push(key);
            } catch (e) {
            }
        },
        settled: (name, keys) => {
            try {
                keys?.forEach((key) => localStorage.removeItem(key));
            } catch (e) {
            }
            if (keys)
                rciWebStore.notes[name] = rciWebStore.notes[name].filter((key) => !keys.includes(key));
        },
        named: (name) => /^\/rayclay(\/(?!\.\.?(\/|$))[^/]+){1,2}$/.test(name),
        parse: (text) => {
            var at = text.indexOf('\n'), head = JSON.parse(text.slice(0, at)), size = 0;
            var within = (path) => typeof path == 'string' && path.startsWith(head[0] + '/') &&
                                   !/\/\.\.?(\/|$)/.test(path);

            if (at < 0 || !Array.isArray(head) || !rciWebStore.named(head[0]) || !Number.isFinite(head[1]) ||
                !Array.isArray(head[2]) || !Array.isArray(head[3]) || !head[3].every(within))
                return null;
            for (var e of head[2]) {
                if (!Array.isArray(e) || !within(e[0]) || !Number.isFinite(e[1]) || !Number.isInteger(e[3]) ||
                    e[3] < 0 || !(FS.isFile(e[2]) || FS.isDir(e[2]) || FS.isLink(e[2]) && typeof e[4] == 'string'))
                    return null;
                size += e[3];
            }
            return size == text.length - at - 1 ? head : null;
        },
        replay: (mount, notes) => {
            var name = mount.mountpoint, held = rciWebStore.held[name], removed = new Set();
            var clear = (path) => {
                if (FS.isDir(FS.lstat(path).mode))
                    FS.readdir(path).forEach((entry) => entry != '.' && entry != '..' && clear(path + '/' + entry));
                IDBFS.removeLocalEntry(path, (e) => e || removed.add(path));
            };

            for (var [key, head, text] of notes.sort((a, b) => a[1][1] - b[1][1])) {
                var at = text.indexOf('\n') + 1, failed = 0, gone = head[3].map((path, i) => [path, head[4]?.[i]]);

                for (var [path, base] of gone.sort((a, b) => a[0] < b[0] ? 1 : -1)) {
                    var was = !removed.has(path) && held[path];

                    try {
                        if (was && FS.analyzePath(path, true).exists &&
                            (+was.timestamp <= head[1] || +was.timestamp == base))
                            clear(path);
                    } catch (e) {
                        failed = e;
                    }
                }
                for (var [path, time, mode, size, link, base] of head[2]) {
                    var bytes = text.slice(at, at += size), was = !removed.has(path) && held[path], there;

                    if (was && +was.timestamp > time && +was.timestamp != base)
                        continue;
                    try {
                        there = FS.analyzePath(path, true).exists && FS.lstat(path).mode;
                        if (there && (FS.isDir(mode) != FS.isDir(there) || FS.isLink(mode) || FS.isLink(there)))
                            clear(path);
                        if (FS.isLink(mode))
                            FS.symlink(link, path);
                        else
                            IDBFS.storeLocalEntry(path, {
                                timestamp: new Date(time), mode,
                                contents: Uint8Array.from(bytes, (c) => c.charCodeAt(0))
                            }, (e) => {
                                if (e)
                                    throw e;
                            });
                        held[path] = { timestamp: new Date(time) };
                        removed.delete(path);
                    } catch (e) {
                        failed = e;
                    }
                }
                if (!failed)
                    (rciWebStore.notes[name] ||= []).push(key);
            }
            IDBFS.queuePersist(mount);
        },
        mark: (entries, held) => {
            var now = Date.now(), changed = [], stat;

            for (var p in entries) {
                try {
                    stat = FS.lstat(p);
                } catch (e) {
                    continue;
                }
                entries[p].node = stat.ino;
                entries[p].racy = +stat.mtime >= now;
                if (held?.[p] && (held[p].node != stat.ino || held[p].racy))
                    changed.push(p);
            }
            return changed;
        },
        persist: (mount, now) => {
            var done = () => {
                if (mount.idbPersistState === 'again') {
                    rciWebStore.persist(mount);
                } else {
                    mount.idbPersistState = 0;
                    IDBFS.onAutoPersistStateChanged?.(false);
                }
            };

            mount.idbPersistState = 'idb';
            IDBFS.onAutoPersistStateChanged?.(true);
            if (!now || !rciWebStore.write(mount, done))
                IDBFS.syncfs(mount, false, done);
        },
        write: (mount, done) => {
            var name = mount.mountpoint, held = rciWebStore.held[name] || {}, db = IDBFS.dbs[name];
            var below = (path) => IDBKeyRange.bound(path + '/', path + '0', false, true);
            var local, changed, put = new Set(), entries = {}, gone, keys, tx, files;
            var run = rciWebStore.begin(mount);

            try {
                IDBFS.getLocalSet(mount, (e, set) => {
                    if (e)
                        throw e;
                    local = set.entries;
                });
                changed = new Set(rciWebStore.mark(local, held));
                for (let path in local) {
                    let p = path;

                    if (held[p] && +held[p].timestamp == +local[p].timestamp && !changed.has(p))
                        continue;
                    while (p.length > name.length && !put.has(p)) {
                        put.add(p);
                        p = p.slice(0, p.lastIndexOf('/'));
                    }
                }
                gone = Object.keys(held).filter((path) => !local[path]);
                for (let path of put)
                    IDBFS.loadLocalEntry(path, (e, entry) => {
                        if (e)
                            throw e;
                        entries[path] = entry;
                    });
                if (put.size || gone.length)
                    rciWebStore.note(name, entries, gone, held);
                keys = rciWebStore.notes[name]?.slice();
                if (!db) {
                    rciWebStore.end(mount, run);
                    return false;
                }
                tx = db.transaction([IDBFS.DB_STORE_NAME], 'readwrite');
                files = tx.objectStore(IDBFS.DB_STORE_NAME);
                for (let path of put) {
                    files.put(entries[path], path);
                    if (!FS.isDir(entries[path].mode))
                        files.delete(below(path));
                }
                for (let path of gone) {
                    files.delete(path);
                    files.delete(below(path));
                }
            } catch (e) {
                tx?.abort();
                rciWebStore.end(mount, run);
                return false;
            }
            tx.oncomplete = () => {
                rciWebStore.held[name] = local;
                rciWebStore.settled(name, keys);
                rciWebStore.end(mount, run, true);
                done();
            };
            tx.onabort = () => {
                rciWebStore.refused(name, tx.error);
                rciWebStore.end(mount, run, false);
                done();
            };
            tx.commit?.();
            return true;
        },
        flush: (now) => {
            var stores = rciWebStore.stores, state;

            for (var s in stores) {
                state = stores[s] && stores[s].idbPersistState;
                if (state && typeof state != 'string') {
                    clearTimeout(state);
                    rciWebStore.persist(stores[s], now);
                } else if (now && state && state != 'load') {
                    rciWebStore.write(stores[s], () => {});
                }
            }
        },
        load: () => {
            var left = 1, done = () => --left || removeRunDependency('rciWebStore'), list;
            var reconcile = IDBFS.reconcile, truncate = FS.truncate, notes = {};
            var queue = IDBFS.queuePersist, syncfs = IDBFS.syncfs;

            try {
                list = indexedDB.databases();
            } catch (e) {
                return;
            }
            addRunDependency('rciWebStore');
            IDBFS.queuePersist = (mount) => {
                var at;

                if (mount.idbPersistState !== 'load') {
                    at = ++rciWebStore.tick;
                    mount.rcDirty ||= at;
                    mount.rcRuns?.forEach((run) => run.next ||= at);
                }
                queue(mount);
            };
            IDBFS.syncfs = (mount, populate, callback) => {
                var run = !populate && rciWebStore.begin(mount);

                syncfs(mount, populate, (e) => {
                    if (run)
                        rciWebStore.end(mount, run, !e);
                    callback(e);
                });
            };
            IDBFS.reconcile = (src, dst, callback) => {
                var name = (src.db || dst.db).name, store = dst.type == 'remote';
                var keys = rciWebStore.notes[name]?.slice();

                if (store)
                    for (var p of rciWebStore.mark(src.entries, rciWebStore.held[name]))
                        delete dst.entries[p];
                reconcile(src, dst, (e) => {
                    if (!e) {
                        if (store)
                            rciWebStore.settled(name, keys);
                        else
                            rciWebStore.mark(src.entries);
                        rciWebStore.held[name] = src.entries;
                    } else if (store) {
                        rciWebStore.refused(name, e);
                    }
                    callback(e);
                });
            };
            FS.truncate = (path, len) => {
                truncate(path, len);
                path = typeof path == 'string' && FS.lookupPath(path, { follow: true }).node;
                if (path && path.idbfs_mount) {
                    path.isModified = false;
                    IDBFS.queuePersist(path.idbfs_mount);
                }
            };
            addEventListener('visibilitychange', () => document.hidden && rciWebStore.flush());
            Module['rcStoreLeave'] = () => rciWebStore.flush(true);
            addEventListener('pagehide', Module['rcStoreLeave']);
            list.then((dbs) => {
                dbs = dbs.map((db) => db.name);
                try {
                    for (var i = 0; i < localStorage.length; i++) {
                        var key = localStorage.key(i), text, head;

                        if (key.startsWith('rcStoreLeave')) {
                            try {
                                text = localStorage.getItem(key);
                                if ((head = rciWebStore.parse(text))) {
                                    (notes[head[0]] ||= []).push([key, head, text]);
                                    dbs.push(head[0]);
                                }
                            } catch (e) {
                            }
                        }
                    }
                } catch (e) {
                }
                rciWebStore.stores = {};
                for (let name of [...new Set(dbs)].sort()) {
                    let mount = rciWebStore.named(name) && rciWebStore.mount(name);

                    if (!mount)
                        continue;
                    mount.idbPersistState = 'load';
                    left++;
                    IDBFS.syncfs(mount, true, (e) => {
                        try {
                            if (e) {
                                rciWebStore.stores[name] = 0;
                                FS.unmount(name);
                            } else {
                                mount.idbPersistState = 0;
                                if (notes[name])
                                    rciWebStore.replay(mount, notes[name]);
                            }
                        } finally {
                            done();
                        }
                    });
                }
            }).catch(() => rciWebStore.stores = null).finally(done);
        },
    },
    rci_window_sdl_web_store_open__deps: ['$rciWebStore', '$UTF8ToString'],
    rci_window_sdl_web_store_open__sig: 'ipp',
    rci_window_sdl_web_store_open: (name, refused) => {
        var stores = rciWebStore.stores;

        rciWebStore.report ||= refused;
        name = UTF8ToString(name);
        if (!stores)
            return 0;
        if (!(name in stores) && rciWebStore.mount(name)) {
            IDBFS.getDB(name, (e) => {
                if (!e && !rciWebStore.held[name])
                    rciWebStore.held[name] = {};
            });
        }
        return stores[name] ? 1 : 0;
    },
    rci_app_files_web_save_checkpoint__deps: ['$rciWebStore'],
    rci_app_files_web_save_checkpoint__sig: 'dp',
    rci_app_files_web_save_checkpoint: (wake) => {
        var at = rciWebStore.last = ++rciWebStore.tick;

        rciWebStore.wake = wake;
        rciWebStore.asked = rciWebStore.answer(at) != 2 ? at : 0;
        return at;
    },
    rci_app_files_web_save_poll__deps: ['$rciWebStore'],
    rci_app_files_web_save_poll__sig: 'id',
    rci_app_files_web_save_poll: (at) => rciWebStore.answer(at),
    $rciFiles__deps: ['$FS', '$lengthBytesUTF8', '$stringToNewUTF8'],
    $rciFiles: {
        input: null,
        open: (job) => {
            var input = rciFiles.input;

            if (!input) {
                input = rciFiles.input = document.createElement('input');
                input.type = 'file';
                input.style.display = 'none';
                document.body.appendChild(input);
            }
            input.value = '';
            input.multiple = job.kind == 1;
            input.webkitdirectory = job.kind == 2;
            input.accept = job.accept;
            input.oncancel = input.onchange = (e) => {
                var files = e.type == 'change' ? [...input.files] : null;

                input.oncancel = input.onchange = null;
                queueMicrotask(() => rciFiles.copy(job, files));
            };
            try {
                if (input.showPicker)
                    input.showPicker();
                else
                    input.click();
            } catch (e) {
                input.oncancel = input.onchange = null;
                return e.name == 'NotAllowedError' ? 1 : 2;
            }
            return 0;
        },
        copy: async (job, files) => {
            var exts = job.accept && job.accept.split(','), folder = '', what = 1, copied = [];
            var pending = () => {{{ makeDynCall('ii', 'job.pending') }}}(job.token);
            var paths, text, data;

            if (!files || (!files.length && job.kind != 2))
                return rciFiles.answer(job, 0);
            if (job.kind == 2) {
                if (files.some((f) => f.webkitRelativePath.split('/').some((p, i, all) =>
                        all.length < 2 || !p || p == '.' || p == '..')))
                    return rciFiles.answer(job, 5);
                folder = job.dir;
                if (files.length)
                    folder += files[0].webkitRelativePath.split('/')[0] + '/';
                if (exts)
                    files = files.filter((f) => exts.some((e) =>
                        f.name.replace(/[A-Z]/g, (c) => c.toLowerCase()).endsWith(e)));
            }
            paths = files.map((f, k) => job.dir + (job.kind == 2 ? f.webkitRelativePath :
                                                   k + '/' + f.name));
            text = paths.reduce((n, p) => n + lengthBytesUTF8(p) + 1,
                                folder ? lengthBytesUTF8(folder) + 1 : 0);
            if (files.length > job.maxFiles || text > job.maxText ||
                files.reduce((n, f) => n + f.size, 0) > job.maxBytes)
                return rciFiles.answer(job, 4);
            for (var k = 0; k < files.length && pending(); k++) {
                try {
                    data = new Uint8Array(await files[k].arrayBuffer());
                    if (!pending())
                        break;
                    FS.mkdirTree(paths[k].slice(0, paths[k].lastIndexOf('/')));
                    FS.writeFile(paths[k], data, { canOwn: true });
                    copied.push(paths[k]);
                } catch (e) {
                    what = 2;
                }
            }
            try {
                if (folder)
                    FS.mkdirTree(folder);
            } catch (e) {
                return rciFiles.answer(job, 3);
            }
            if (files.length && !copied.length)
                return rciFiles.answer(job, 3);
            rciFiles.answer(job, what, folder ? [folder, ...copied] : copied, copied.length);
        },
        answer: (job, what, list = [], count = 0) => {{{ makeDynCall('viiipi', 'job.answer') }}}(
            job.token, what, job.kind, list.length ? stringToNewUTF8(list.join('\0')) : 0, count),
    },
    rci_files_web_open__deps: ['$rciFiles', '$UTF8ToString'],
    rci_files_web_open__sig: 'iiippiiipp',
    rci_files_web_open: (token, kind, accept, dir, maxFiles, maxBytes, maxText, answer, pending) =>
        rciFiles.open({ token, kind, accept: UTF8ToString(accept), dir: UTF8ToString(dir), maxFiles,
                        maxBytes, maxText, answer, pending }),
    rci_files_web_save__deps: ['$UTF8ToString'],
    rci_files_web_save__sig: 'ippp',
    rci_files_web_save: (name, bytes, len) => {
        var a = document.createElement('a');

        try {
            a.href = URL.createObjectURL(new Blob([HEAPU8.subarray(bytes, bytes + len)]));
        } catch (e) {
            return 0;
        }
        a.download = UTF8ToString(name);
        a.click();
        setTimeout(() => URL.revokeObjectURL(a.href), 4e4);
        return 1;
    },
    rci_gfx_web_element_upload__deps: ['$GL', '$rciMedia'],
    rci_gfx_web_element_upload__sig: 'iiiii',
    rci_gfx_web_element_upload: (tex, element, w, h) => {
        var video = rciMedia.element(element), gl = GL.currentContext.GLctx;

        if (!video || video.readyState < 2 || video.videoWidth != w || video.videoHeight != h)
            return 0;
        gl.bindTexture(gl.TEXTURE_2D, GL.textures[tex]);
        try {
            gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, video);
        } catch (e) {
            return -1;
        }
        return 1;
    },
    rci_open_url_web__deps: ['$UTF8ToString'],
    rci_open_url_web__sig: 'ip',
    rci_open_url_web: (url) => {
        try {
            window.open(UTF8ToString(url), '_blank', 'noopener,noreferrer');
        } catch (e) {
            return 0;
        }
        return 1;
    },
});
]==])
    set(_rc_idbfs_src "${EMSCRIPTEN_ROOT_PATH}/src/lib/libidbfs.js")
    set(_rc_idbfs -lidbfs.js)
    if(EXISTS "${_rc_idbfs_src}")
        configure_file("${_rc_idbfs_src}" "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/rayclay_idbfs.js" COPYONLY)
        set(_rc_idbfs "--js-library=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/rayclay_idbfs.js")
    endif()
    target_link_options(rayclay_deps INTERFACE "${_rc_idbfs}" "--js-library=${_rc_web_js}")
    target_link_options(rayclay_deps INTERFACE
        -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3 -sGROWABLE_ARRAYBUFFERS=0)
    set_property(TARGET rayclay_deps APPEND PROPERTY INTERFACE_LINK_DEPENDS "${_rc_web_js}")
endif()

if(EMSCRIPTEN)
    set(_rc_update_js "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/rayclay_update.js")
    file(CONFIGURE OUTPUT "${_rc_update_js}" @ONLY CONTENT [==[
addToLibrary({
    rci_update_web_reload__sig: 'v',
    rci_update_web_reload: () => location.reload(),
});
]==])
    target_link_options(rayclay_deps INTERFACE "--js-library=${_rc_update_js}")
    set_property(TARGET rayclay_deps APPEND PROPERTY INTERFACE_LINK_DEPENDS "${_rc_update_js}")
endif()

if(EMSCRIPTEN)
    set(_rc_media_js "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/rayclay_media.js")
    file(CONFIGURE OUTPUT "${_rc_media_js}" @ONLY CONTENT [==[
addToLibrary({
    $rciMedia__deps: ['$FS', '$stringToUTF8'],
    $rciMedia: {
        live: new Map(),
        next: 0,
        box: null,
        types: { webm: 'video/webm', mp4: 'video/mp4', m4v: 'video/mp4', mov: 'video/quicktime',
                 mp3: 'audio/mpeg', m4a: 'audio/mp4', aac: 'audio/aac', ogg: 'audio/ogg', oga: 'audio/ogg',
                 opus: 'audio/ogg', wav: 'audio/wav', flac: 'audio/flac' },
        words: ['the browser could not play it', 'the browser stopped loading it',
                'a network error stopped the browser loading it', 'the browser could not decode it',
                'not found, cross-origin without CORS, or a format this browser cannot play'],
        element: (id) => rciMedia.live.get(id)?.el,
        type: (src) => rciMedia.types[/\.(\w+)$/.exec(src.split(/[?#]/)[0])?.[1].toLowerCase()] || '',
        call: (e, what, value = 0, w = 0, h = 0) => {
            if (!e.closed)
                {{{ makeDynCall('vpidii', 'e.call') }}}(e.web, what, value, w, h);
        },
        load: (e, src) => {
            var node = FS.analyzePath(src).object;

            if (e.blob)
                URL.revokeObjectURL(e.blob);
            e.blob = null;
            try {
                if (node && FS.isFile(node.mode))
                    e.blob = URL.createObjectURL(new Blob([FS.readFile(src)], { type: rciMedia.type(src) }));
            } catch (error) {
            }
            e.el.src = e.blob || src;
        },
        fail: (e) => {
            var code = e.el.error?.code || 0, said = e.el.error?.message;
            var words = rciMedia.words[code] || rciMedia.words[0];

            if (said && words.length + said.length + 3 < e.size)
                words += ' (' + said + ')';
            stringToUTF8(words, e.words, e.size);
            rciMedia.call(e, 8, code);
        },
        poll: (e, t) => {
            e.polling = !e.closed && !e.el.paused;
            if (!e.polling)
                return;
            if (e.el.currentTime != t)
                rciMedia.call(e, 2);
            requestAnimationFrame(() => rciMedia.poll(e, e.el.currentTime));
        },
    },
    rci_media_web_open__deps: ['$rciMedia', '$UTF8ToString'],
    rci_media_web_open__sig: 'ipppipp',
    rci_media_web_open: (web, call, words, size, src, alt) => {
        var el = document.createElement('video'), id = ++rciMedia.next, kind;
        var e = { web, call, words, size, el, blob: null, alt: alt ? UTF8ToString(alt) : null, want: false,
                  serial: 0, ready: false, closed: false, polling: false, w: 0, h: 0, waiting: false,
                  waitedAt: 0 };
        var on = (name, run) => el.addEventListener(name, () => e.closed || run());
        var resumed = () => {
            if (e.waiting) {
                e.waiting = false;
                rciMedia.call(e, 4, 0);
            }
        };
        var moved = () => e.waiting && !el.paused && el.currentTime != e.waitedAt && resumed();
        var frame = () => {
            if (!e.closed) {
                moved();
                rciMedia.call(e, 2);
                el.requestVideoFrameCallback(frame);
            }
        };

        src = UTF8ToString(src);
        if (!rciMedia.box) {
            rciMedia.box = document.createElement('div');
            rciMedia.box.style.cssText = 'position:fixed;left:0;top:0;width:1px;height:1px;opacity:0;' +
                                         'pointer-events:none';
            rciMedia.box.setAttribute('aria-hidden', 'true');
            document.body.appendChild(rciMedia.box);
        }
        el.crossOrigin = 'anonymous';
        el.playsInline = true;
        el.preload = 'auto';
        el.disableRemotePlayback = true;
        el.preservesPitch = true;
        el.style.cssText = 'width:1px;height:1px';
        on('loadedmetadata', () => {
            e.ready = true;
            e.alt = null;
            e.w = el.videoWidth;
            e.h = el.videoHeight;
            rciMedia.call(e, 1, el.duration, e.w, e.h);
        });
        on('loadeddata', () => rciMedia.call(e, 2));
        on('seeked', () => el.seeking || rciMedia.call(e, 3));
        on('waiting', () => {
            if (!el.paused && !e.waiting) {
                e.waiting = true;
                e.waitedAt = el.currentTime;
                rciMedia.call(e, 4, 1);
            }
        });
        on('playing', resumed);
        on('timeupdate', moved);
        on('play', () => {
            if (!e.want && !el.paused) {
                e.want = true;
                rciMedia.call(e, 10);
            }
            if (!el.requestVideoFrameCallback && !e.polling)
                rciMedia.poll(e, -1);
        });
        on('pause', () => {
            if (e.want && !el.ended) {
                e.want = false;
                rciMedia.call(e, 5);
            }
        });
        on('ended', () => {
            e.want = false;
            rciMedia.call(e, 6);
        });
        on('resize', () => {
            if (e.ready && (el.videoWidth != e.w || el.videoHeight != e.h)) {
                e.w = el.videoWidth;
                e.h = el.videoHeight;
                rciMedia.call(e, 9, 0, e.w, e.h);
            }
        });
        on('error', () => {
            var next = e.alt;

            e.alt = null;
            next ? rciMedia.load(e, next) : rciMedia.fail(e);
        });
        if (el.requestVideoFrameCallback)
            el.requestVideoFrameCallback(frame);
        rciMedia.box.appendChild(el);
        rciMedia.live.set(id, e);
        kind = rciMedia.type(src);
        if (e.alt && kind && el.canPlayType(kind) === '')
            [src, e.alt] = [e.alt, null];
        rciMedia.load(e, src);
        return id;
    },
    rci_media_web_close__deps: ['$rciMedia'],
    rci_media_web_close__sig: 'vi',
    rci_media_web_close: (id) => {
        var e = rciMedia.live.get(id);

        if (!e)
            return;
        e.closed = true;
        rciMedia.live.delete(id);
        e.el.pause();
        e.el.removeAttribute('src');
        e.el.load();
        e.el.remove();
        if (e.blob)
            URL.revokeObjectURL(e.blob);
        if (!rciMedia.live.size) {
            rciMedia.box.remove();
            rciMedia.box = null;
        }
    },
    rci_media_web_play__deps: ['$rciMedia'],
    rci_media_web_play__sig: 'vii',
    rci_media_web_play: (id, playing) => {
        var e = rciMedia.live.get(id), serial;

        if (!e)
            return;
        e.want = !!playing;
        serial = ++e.serial;
        if (!playing)
            return e.el.pause();
        e.el.play()?.catch((error) => {
            if (!e.closed && serial == e.serial && error?.name == 'NotAllowedError') {
                e.want = false;
                rciMedia.call(e, 7);
            }
        });
    },
    rci_media_web_seek__deps: ['$rciMedia'],
    rci_media_web_seek__sig: 'vid',
    rci_media_web_seek: (id, seconds) => {
        var e = rciMedia.live.get(id);

        if (e)
            e.el.currentTime = seconds;
    },
    rci_media_web_set__deps: ['$rciMedia'],
    rci_media_web_set__sig: 'vididi',
    rci_media_web_set: (id, volume, muted, rate, loop) => {
        var el = rciMedia.live.get(id)?.el;

        if (!el)
            return;
        el.volume = volume;
        el.muted = !!muted;
        el.loop = !!loop;
        if (el.playbackRate != rate) {
            try {
                el.playbackRate = rate;
            } catch (error) {
            }
        }
    },
    rci_media_web_time__deps: ['$rciMedia'],
    rci_media_web_time__sig: 'di',
    rci_media_web_time: (id) => rciMedia.live.get(id)?.el.currentTime || 0,
});
]==])
    target_link_options(rayclay_deps INTERFACE "--js-library=${_rc_media_js}")
    set_property(TARGET rayclay_deps APPEND PROPERTY INTERFACE_LINK_DEPENDS "${_rc_media_js}")
endif()
if(EMSCRIPTEN)
    set(_rc_sound_js "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/rayclay_sound.js")
    file(CONFIGURE OUTPUT "${_rc_sound_js}" @ONLY CONTENT [==[
addToLibrary({
    $rciSound__deps: ['$stringToUTF8', '$UTF8ToString'],
    $rciSound: {
        ctx: null,        /* the AudioContext; false once none can be made */
        buffers: new Map(),
        next: 0,
        slots: [],
        WAIT: 0.5,        /* RCI_SOUND_WEB_WAIT: seconds a play waits for the context to run */
        SLOW: 3,          /* RCI_SOUND_WEB_SLOW: ... in all, once the user has used the page */
        RAMP: 0.005,      /* a gain change's and a stop's ramp, as the C mixer's 5 ms */
        GESTURES: ['keydown', 'mousedown', 'pointerdown', 'pointerup', 'touchend', 'click'],
        make: () => {
            var C = globalThis.AudioContext || globalThis.webkitAudioContext;

            if (rciSound.ctx !== null)
                return rciSound.ctx;
            rciSound.ctx = false;
            if (!C)
                return false;
            try {
                rciSound.ctx = new C();
            } catch (error) {
                return false;
            }
            if (globalThis.addEventListener) {
                for (var name of rciSound.GESTURES)
                    globalThis.addEventListener(name, rciSound.unlock, { capture: true });
            }
            return rciSound.ctx;
        },
        unlock: () => {
            var c = rciSound.ctx;

            if (c && c.state !== 'running' && c.state !== 'closed')
                c.resume().catch(() => {});
        },
        slot: (i) => rciSound.slots[i] || (rciSound.slots[i] = { id: 0, done: 0, blocked: 0 }),
        /* The voice `id` is over on slot `s`: its id is the slot's done. */
        letGo: (s, id, blocked) => {
            if (s.id !== id)
                return;
            clearTimeout(s.timer);
            if (s.src) {
                s.src.onended = null;
                s.src.disconnect();
                s.gain.disconnect();
            }
            s.src = s.gain = s.buffer = null;
            s.pending = false;
            s.stopping = false;
            s.id = 0;
            s.blocked = blocked;
            s.done = id;
        },
        ramp: (param, to) => {
            var t = rciSound.ctx.currentTime;

            param.cancelScheduledValues(t);
            param.setValueAtTime(param.value, t);
            param.linearRampToValueAtTime(to, t + rciSound.RAMP);
        },
        begin: (s) => {
            var ctx = rciSound.ctx, id = s.id;

            s.pending = false;
            s.src = ctx.createBufferSource();
            s.gain = ctx.createGain();
            s.src.buffer = s.buffer;
            s.src.loop = s.loop;
            s.gain.gain.value = s.level;
            s.src.connect(s.gain);
            s.gain.connect(ctx.destination);
            s.src.onended = () => rciSound.letGo(s, id, 0);
            s.t0 = ctx.currentTime;
            s.src.start();
        },
    },
    rci_media_sound_web_js_open__deps: ['$rciSound'],
    rci_media_sound_web_js_open__sig: 'i',
    rci_media_sound_web_js_open: () => {
        var ctx = rciSound.make();

        return ctx ? Math.round(ctx.sampleRate) : 0;
    },
    rci_media_sound_web_js_start__deps: ['$rciSound'],
    rci_media_sound_web_js_start__sig: 'iiiidi',
    rci_media_sound_web_js_start: (slot, id, buffer, level, loop) => {
        var ctx = rciSound.ctx, s = rciSound.slot(slot), p;
        var refuse = () => {
            if (s.id === id && s.pending)
                rciSound.letGo(s, id, 1);
        };
        /* Still not running after WAIT: refused, unless the user has used the page
           (sticky activation, which lets resume() run), when it is an output slow to
           start (a Bluetooth device waking), given SLOW in all first. */
        var late = () => {
            var used = globalThis.navigator && navigator.userActivation &&
                       navigator.userActivation.hasBeenActive;

            if (s.id !== id || !s.pending)
                return;
            if (used && !s.slow) {
                s.slow = true;
                s.timer = setTimeout(late, (rciSound.SLOW - rciSound.WAIT) * 1000);
            } else {
                refuse();
            }
        };

        if (!ctx)
            return 0;
        if (s.id)
            rciSound.letGo(s, s.id, 0);
        s.id = id;
        s.buffer = rciSound.buffers.get(buffer) || null;
        s.level = level;
        s.loop = !!loop;
        if (!s.buffer) {
            rciSound.letGo(s, id, 0);
            return 1;
        }
        if (ctx.state === 'running') {
            rciSound.begin(s);
            return 1;
        }
        /* Never started later by surprise. */
        s.pending = true;
        s.slow = false;
        s.timer = setTimeout(late, rciSound.WAIT * 1000);
        try {
            p = ctx.resume();
        } catch (error) {
            refuse();
            return 1;
        }
        Promise.resolve(p).then(() => {
            if (s.id === id && s.pending && ctx.state === 'running') {
                clearTimeout(s.timer);
                rciSound.begin(s);
            }
        }, refuse);
        return 1;
    },
    rci_media_sound_web_js_gain__deps: ['$rciSound'],
    rci_media_sound_web_js_gain__sig: 'viid',
    rci_media_sound_web_js_gain: (slot, id, level) => {
        var s = rciSound.slot(slot);

        if (s.id !== id || s.stopping)
            return;
        s.level = level;
        if (s.gain)
            rciSound.ramp(s.gain.gain, level);
    },
    rci_media_sound_web_js_stop__deps: ['$rciSound'],
    rci_media_sound_web_js_stop__sig: 'vii',
    rci_media_sound_web_js_stop: (slot, id) => {
        var s = rciSound.slot(slot);

        if (s.id !== id || s.stopping)
            return;
        if (!s.src) {
            rciSound.letGo(s, id, 0);
            return;
        }
        s.stopping = true;
        rciSound.ramp(s.gain.gain, 0);
        try {
            s.src.stop(rciSound.ctx.currentTime + rciSound.RAMP);
        } catch (error) {
            rciSound.letGo(s, id, 0);
        }
    },
    rci_media_sound_web_js_done__deps: ['$rciSound'],
    rci_media_sound_web_js_done__sig: 'ii',
    rci_media_sound_web_js_done: (slot) => rciSound.slots[slot]?.done || 0,
    rci_media_sound_web_js_blocked__deps: ['$rciSound'],
    rci_media_sound_web_js_blocked__sig: 'ii',
    rci_media_sound_web_js_blocked: (slot) => rciSound.slots[slot]?.blocked || 0,
    rci_media_sound_web_js_at__deps: ['$rciSound'],
    rci_media_sound_web_js_at__sig: 'ii',
    rci_media_sound_web_js_at: (slot) => {
        var s = rciSound.slots[slot], b, f;

        if (!s || !s.src)
            return 0;
        b = s.buffer;
        f = Math.floor((rciSound.ctx.currentTime - s.t0) * b.sampleRate);
        f = f > 0 ? f : 0;
        return s.loop ? f % b.length : Math.min(f, b.length);
    },
    rci_media_sound_web_js_renders__deps: ['$rciSound'],
    rci_media_sound_web_js_renders__sig: 'i',
    rci_media_sound_web_js_renders: () => rciSound.ctx ? Math.floor(rciSound.ctx.currentTime * 1000) % 2147483647 : 0,
    rci_media_sound_web_js_buffer__deps: ['$rciSound'],
    rci_media_sound_web_js_buffer__sig: 'ipiii',
    rci_media_sound_web_js_buffer: (samples, frames, channels, rate) => {
        var ctx = rciSound.make(), b, all, id;

        if (!ctx)
            return 0;
        try {
            b = ctx.createBuffer(channels, frames, rate);
        } catch (error) {
            return -1;
        }
        all = new Float32Array(HEAPU8.buffer, samples, frames * channels);
        for (var c = 0; c < channels; c++) {
            var d = b.getChannelData(c);

            for (var i = 0; i < frames; i++)
                d[i] = all[i * channels + c];
        }
        id = ++rciSound.next;
        rciSound.buffers.set(id, b);
        return id;
    },
    rci_media_sound_web_js_decode__deps: ['$rciSound', '$stringToUTF8', '$UTF8ToString'],
    rci_media_sound_web_js_decode__sig: 'vppppiipip',
    rci_media_sound_web_js_decode: (answer, room, wav, words, size, token, bytes, len, path) => {
        var ctx = rciSound.make(), call = (error, text, b) => {
            var id = 0;

            if (b) {
                id = ++rciSound.next;
                rciSound.buffers.set(id, b);
            }
            stringToUTF8(text || '', words, size);
            {{{ makeDynCall('viiiiii', 'answer') }}}(token, error, id, b ? b.length : 0,
                                                    b ? b.numberOfChannels : 0, b ? b.sampleRate : 0);
        };
        /* The browser's words after RayClay's, when both fit. */
        var said = (text, error) => {
            var more = error && error.message ? ' (' + error.message + ')' : '';

            return text.length + more.length < size ? text + more : text;
        };
        var decode = (data) => new Promise((ok, no) => {
            try {
                var p = ctx.decodeAudioData(data, ok, no);

                if (p && p.then)
                    p.then(ok, no);
            } catch (error) {
                no(error);
            }
        });
        var got = (b) => {
            if (b.numberOfChannels > 2)
                call(2, b.numberOfChannels + ' channels: mono and stereo sounds are decoded');
            else if (!b.length)
                call(4, 'a sound with no samples');
            else
                call(0, '', b);
        };
        var bad = (error) => call(4, said('the browser could not decode it', error));

        if (!ctx) {
            call(1, 'this browser has no Web Audio (AudioContext)');
        } else if (bytes) {
            decode(HEAPU8.slice(bytes, bytes + len).buffer).then(got, bad);
        } else {
            fetch(UTF8ToString(path)).then((r) => {
                if (r.ok)
                    return r.arrayBuffer().then((data) => {
                        var u = new Uint8Array(data), at;
                        var tag = String.fromCharCode(...u.subarray(0, 4)) +
                                  String.fromCharCode(...u.subarray(8, 12));

                        /* A WAV is RayClay's reader's, as on every host: its bytes
                           into C's heap, through the pointer room gives. */
                        if (u.length < 12 || !['RIFFWAVE', 'RIFXWAVE', 'RF64WAVE'].includes(tag))
                            return decode(data).then(got, bad);
                        at = {{{ makeDynCall('pi', 'room') }}}(u.length);
                        if (!at)
                            return call(1, 'out of memory for the fetched file');
                        HEAPU8.set(u, at);
                        {{{ makeDynCall('vipi', 'wav') }}}(token, at, u.length);
                    });
                if (r.status === 404 || r.status === 410)
                    call(3, 'not in the page\'s files, and its server has none (' + r.status + ')');
                else
                    call(5, 'the page\'s server answered ' + r.status);
            }, (error) => call(5, said('the browser could not fetch it', error)));
        }
    },
    rci_media_sound_web_js_release__deps: ['$rciSound'],
    rci_media_sound_web_js_release__sig: 'vi',
    rci_media_sound_web_js_release: (buffer) => rciSound.buffers.delete(buffer),
    rci_media_sound_web_js_close__deps: ['$rciSound'],
    rci_media_sound_web_js_close__sig: 'v',
    rci_media_sound_web_js_close: () => {
        for (var s of rciSound.slots) {
            if (s && s.id) {
                if (s.src) {
                    try {
                        s.src.stop();
                    } catch (error) {
                    }
                }
                rciSound.letGo(s, s.id, 0);
            }
        }
    },
});
]==])
    if(RC_SOUND)
        target_link_options(rayclay_deps INTERFACE "--js-library=${_rc_sound_js}")
        set_property(TARGET rayclay_deps APPEND PROPERTY INTERFACE_LINK_DEPENDS "${_rc_sound_js}")
    endif()
endif()

target_link_libraries(rayclay PUBLIC rayclay_deps)

if(WIN32)
    target_sources(rayclay INTERFACE
        "$<$<NOT:$<BOOL:$<TARGET_PROPERTY:RC_NO_UTF8_MANIFEST>>>:${_rc_utf8_source}>")
endif()

add_library(rayclay_impl_deps INTERFACE)
target_include_directories(rayclay_impl_deps INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(rayclay_impl_deps INTERFACE
    $<TARGET_PROPERTY:rayclay,COMPILE_DEFINITIONS>)
target_compile_options(rayclay_impl_deps INTERFACE "$<$<C_COMPILER_ID:MSVC>:/utf-8;/Gw>")
target_compile_features(rayclay_impl_deps INTERFACE c_std_11)
if(CMAKE_CXX_COMPILER_LOADED)
    target_compile_features(rayclay_impl_deps INTERFACE cxx_std_20)
endif()
target_compile_options(rayclay_impl_deps INTERFACE $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wno-missing-field-initializers>)
if(NOT MSVC)
    target_compile_options(rayclay_impl_deps INTERFACE ${_rc_section_flags})
    if(APPLE)
        target_link_options(rayclay_impl_deps INTERFACE -Wl,-dead_strip)
    else()
        target_link_options(rayclay_impl_deps INTERFACE -Wl,--gc-sections)
    endif()
endif()
if(WIN32)
    target_sources(rayclay_impl_deps INTERFACE
        "$<$<NOT:$<BOOL:$<TARGET_PROPERTY:RC_NO_UTF8_MANIFEST>>>:${_rc_utf8_source}>")
endif()
target_link_libraries(rayclay_impl_deps INTERFACE rayclay_deps)

set_property(TARGET rayclay rayclay_impl_deps APPEND PROPERTY COMPATIBLE_INTERFACE_STRING RC_BASE_OWNER)
set_property(TARGET rayclay rayclay_impl_deps PROPERTY INTERFACE_RC_BASE_OWNER rayclay)

option(RC_BUILD_EXAMPLES "Build the RayClay examples" ${RC_DESKTOP_TARGETS_DEFAULT})

if(RC_BUILD_EXAMPLES AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/ex00_hello/hello.c)
    add_executable(rayclay_ex00_hello                   examples/ex00_hello/hello.c)
    add_executable(rayclay_ex01_1980s_gui               examples/ex01_1980s_gui/main.c)
    add_executable(rayclay_ex02_1990s_gui               examples/ex02_1990s_gui/main.c)
    add_executable(rayclay_ex03_2000s_gui               examples/ex03_2000s_gui/main.c)
    add_executable(rayclay_ex04_2010s_gui               examples/ex04_2010s_gui/main.c)
    add_executable(rayclay_ex05_2020s_gui               examples/ex05_2020s_gui/main.c)
    include(CheckLanguage)
    check_language(CXX)
    if(CMAKE_CXX_COMPILER)
        enable_language(CXX)
        add_executable(rayclay_ex25_cpp_kanban           examples/ex25_cpp_kanban/main.cpp)
        set_target_properties(rayclay_ex25_cpp_kanban PROPERTIES
            CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    endif()
    add_executable(rayclay_ex06_widgets_gallery examples/ex06_widgets_gallery/main.c)
    if(NOT EMSCRIPTEN)
        add_executable(rayclay_ex10_util_rc_icon_converter  examples/ex10_util_rc_icon_converter/main.c)
    endif()
    add_executable(rayclay_ex11_util_rc_inspector       examples/ex11_util_rc_inspector/main.c)
    add_executable(rayclay_ex12_signin_form             examples/ex12_signin_form/src/main.c)
    add_executable(rayclay_ex20_system_monitor          examples/ex20_system_monitor/src/main.c)
    add_executable(rayclay_ex26_data_explorer           examples/ex26_data_explorer/src/main.c)
    add_executable(rayclay_ex27_issue_inbox             examples/ex27_issue_inbox/src/main.c)
    add_executable(rayclay_ex28_ide                examples/ex28_ide/src/main.c)
    if((CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID) AND NOT RC_NO_NET AND NOT TARGET rayclay_netprofile
       AND TARGET rayclay_ex25_cpp_kanban)
        set_target_properties(rayclay_ex25_cpp_kanban PROPERTIES EXCLUDE_FROM_ALL ON)
        message(STATUS "rayclay: rayclay_ex25_cpp_kanban is left out of the default build: it calls "
                       "rcFetch, and no network profile was built (the netprofile line above says why)")
    endif()
    add_executable(rayclay_ex15_file_explorer            examples/ex15_file_explorer/src/main.c)
    add_executable(rayclay_ex14_file_selector            examples/ex14_file_selector/src/main.c)
    if(NOT EMSCRIPTEN)
        add_executable(rayclay_ex13_user_preferences       examples/ex13_user_preferences/src/main.c)
        add_executable(rayclay_ex16_tab_navigator       examples/ex16_tab_navigator/src/main.c)
        if((CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID) AND NOT RC_NO_NET AND NOT TARGET rayclay_netprofile)
            set_target_properties(rayclay_ex13_user_preferences PROPERTIES EXCLUDE_FROM_ALL ON)
            message(STATUS "rayclay: rayclay_ex13_user_preferences is left out of the default build: it calls "
                           "rcUpdateCheck, whose manifest is an rcFetch, and no network profile was built "
                           "(the netprofile line above says why)")
        endif()
        if(NOT ANDROID AND NOT RC_IOS)
            message(WARNING
                "rayclay: building examples that are intended for phones: "
                "rayclay_ex13_user_preferences, rayclay_ex16_tab_navigator. They are built here, "
                "not only on the platform they are designed for, so their layout can be checked "
                "without a device. Judge their layout on a phone.")
        else()
            message(WARNING
                "rayclay: building examples that are intended for the desktop: "
                "rayclay_ex10_util_rc_icon_converter. They are built here as compile coverage and "
                "installed on no device: their function needs a desktop filesystem, and there is "
                "no frame for a device witness to see.")
        endif()
    endif()
    add_executable(rayclay_ex24_messenger
                   examples/ex24_messenger/src/main.c
                   examples/ex24_messenger/src/app/messenger_app.c)
    if((CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID) AND NOT RC_NO_NET AND NOT TARGET rayclay_netprofile)
        set_target_properties(rayclay_ex24_messenger PROPERTIES EXCLUDE_FROM_ALL ON)
        message(STATUS "rayclay: rayclay_ex24_messenger is left out of the default build: it calls "
                       "rcWebSocket, and no network profile was built (the netprofile line above says why)")
    endif()
    add_executable(rayclay_ex22_markdown_notes
                   examples/ex22_markdown_notes/src/main.c
                   examples/ex22_markdown_notes/src/app/notes_app.c)
    add_executable(rayclay_ex23_stocks_trader
                   examples/ex23_stocks_trader/src/main.c
                   examples/ex23_stocks_trader/src/app/trader_app.c)
    if((CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID) AND NOT RC_NO_NET AND NOT TARGET rayclay_netprofile)
        set_target_properties(rayclay_ex23_stocks_trader PROPERTIES EXCLUDE_FROM_ALL ON)
        message(STATUS "rayclay: rayclay_ex23_stocks_trader is left out of the default build: it calls "
                       "rcFetch, and no network profile was built (the netprofile line above says why)")
    endif()
    add_executable(rayclay_ex29_dash_game
                   examples/ex29_dash_game/src/main.c
                   examples/ex29_dash_game/src/app/platformer_app.c)
    add_executable(rayclay_ex21_media_library
                   examples/ex21_media_library/src/main.c
                   examples/ex21_media_library/src/app/gallery_app.c)

    if(NOT EMSCRIPTEN)
        target_compile_definitions(rayclay_ex06_widgets_gallery PRIVATE
            RC_DEMO_LOGO="examples/assets/logos/rayclay-logo-1024.png")

        target_compile_definitions(rayclay_ex10_util_rc_icon_converter PRIVATE
            $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)

        if(NOT ANDROID AND NOT RC_IOS)
            file(GLOB_RECURSE _rc_ex_assets CONFIGURE_DEPENDS
                 "${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/*")
            set(_rc_ex_assets_stamp
                "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/rayclay-example-assets.stamp")
            add_custom_command(OUTPUT "${_rc_ex_assets_stamp}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory
                        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
                COMMAND "${CMAKE_COMMAND}" -E copy_directory
                        "${CMAKE_CURRENT_SOURCE_DIR}/examples/assets"
                        "$<TARGET_FILE_DIR:rayclay_ex06_widgets_gallery>/examples/assets"
                COMMAND "${CMAKE_COMMAND}" -E touch "${_rc_ex_assets_stamp}"
                DEPENDS ${_rc_ex_assets}
                COMMENT "examples: copying examples/assets beside the programs"
                VERBATIM)
            add_custom_target(rayclay_example_assets DEPENDS "${_rc_ex_assets_stamp}")
            foreach(_rc_reader rayclay_ex06_widgets_gallery
                               rayclay_ex10_util_rc_icon_converter
                               rayclay_ex12_signin_form
                               rayclay_ex22_markdown_notes rayclay_ex28_ide
                               rayclay_ex21_media_library rayclay_ex15_file_explorer
                               rayclay_ex29_dash_game)
                add_dependencies(${_rc_reader} rayclay_example_assets)
            endforeach()
        endif()
    else()
        set(RC_DEMO_PNG   "${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/logos/rayclay-logo-1024.png")
        set(RC_EX22_FONTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/fonts")
        set(RC_WEB_SHELL  "${CMAKE_CURRENT_SOURCE_DIR}/examples/web/shell.html")

        set(_rc_web_build_id "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/rc_web_build_id.cmake")
        file(CONFIGURE OUTPUT "${_rc_web_build_id}" @ONLY CONTENT [==[
cmake_minimum_required(VERSION 3.21)
get_filename_component(dir "${RC_PAGE}" DIRECTORY)
get_filename_component(name "${RC_PAGE}" NAME_WLE)
set(sums "")
foreach(ext wasm js data)
    if(EXISTS "${dir}/${name}.${ext}")
        file(SHA256 "${dir}/${name}.${ext}" sum)
        string(APPEND sums "${ext} ${sum}\n")
    elseif(NOT ext STREQUAL "data")
        message(FATAL_ERROR "web build id: ${dir}/${name}.${ext} is missing")
    endif()
endforeach()
string(SHA256 id "${sums}")
string(SUBSTRING "${id}" 0 12 id)
set(query "?v=${id}")
file(READ "${RC_PAGE}" html)
file(SIZE "${RC_PAGE}" size)
string(LENGTH "${html}" length)
if(NOT length EQUAL size)
    message(FATAL_ERROR "web build id: read ${length} of the ${size} bytes of ${RC_PAGE}")
endif()
function(rc_web_build_id_swap total old new)
    string(REPLACE "${old}" "" rest "${html}")
    string(LENGTH "${html}" before)
    string(LENGTH "${rest}" after)
    string(LENGTH "${old}" each)
    math(EXPR hits "${${total}} + (${before} - ${after}) / ${each}")
    string(REPLACE "${old}" "${new}" html "${html}")
    set(html "${html}" PARENT_SCOPE)
    set(${total} ${hits} PARENT_SCOPE)
endfunction()
set(vars 0)
rc_web_build_id_swap(vars "rcBuildQuery = \"\"" "rcBuildQuery = \"${query}\"")
rc_web_build_id_swap(vars "rcBuildQuery=\"\"" "rcBuildQuery=\"${query}\"")
set(srcs 0)
rc_web_build_id_swap(srcs "src=\"${name}.js\"" "src=\"${name}.js${query}\"")
rc_web_build_id_swap(srcs "src=${name}.js>" "src=\"${name}.js${query}\">")
rc_web_build_id_swap(srcs "src=${name}.js " "src=\"${name}.js${query}\" ")
if(NOT vars EQUAL 1 OR NOT srcs EQUAL 1)
    message(FATAL_ERROR "web build id: ${RC_PAGE} holds ${vars} rcBuildQuery placeholders and "
        "${srcs} script srcs of ${name}.js; the build id rewrites exactly one of each")
endif()
file(WRITE "${RC_PAGE}" "${html}")
]==])
        set(_rc_web_notices "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/rc_web_notices.cmake")
        file(CONFIGURE OUTPUT "${_rc_web_notices}" @ONLY CONTENT [==[
cmake_minimum_required(VERSION 3.21)
file(GLOB scripts "${RC_PAGES}/*.js")
set(families "")
foreach(script IN LISTS scripts)
    file(READ "${script}" text)
    string(REGEX MATCHALL "filename\"?[ \t\r\n]*:[ \t\r\n]*\"[^\"]*\\.ttf\"" fonts "${text}")
    foreach(font IN LISTS fonts)
        string(REGEX REPLACE "^.*[/\"]([^/\"]+)\\.ttf\"$" "\\1" font "${font}")
        string(REGEX REPLACE "-[^-]*$" "" font "${font}")
        list(APPEND families "${font}")
    endforeach()
endforeach()
list(REMOVE_DUPLICATES families)
list(SORT families)
if(NOT families)
    message(FATAL_ERROR "web notices: no preloaded .ttf is named in ${RC_PAGES}/*.js")
endif()
set(texts "${RC_ASSETS}/icons/LICENSE-Lucide.md")
foreach(family IN LISTS families)
    list(APPEND texts "${RC_ASSETS}/fonts/${family}-OFL.txt")
endforeach()
if(NOT EXISTS "${RC_NOTICES}")
    message(FATAL_ERROR "web notices: ${RC_NOTICES} is missing")
endif()
configure_file("${RC_NOTICES}" "${RC_SITE}/LICENSES.txt" COPYONLY)
set(index "<!DOCTYPE html><meta charset=\"utf-8\"><title>License notices</title><ul>\n")
string(APPEND index
       "<li><a href=\"../LICENSES.txt\">LICENSES.txt</a> (RayClay and its libraries)</li>\n")
foreach(text IN LISTS texts)
    if(NOT EXISTS "${text}")
        message(FATAL_ERROR "web notices: a page needs ${text}, which is missing")
    endif()
    get_filename_component(name "${text}" NAME)
    configure_file("${text}" "${RC_SITE}/licenses/${name}" COPYONLY)
    string(APPEND index "<li><a href=\"${name}\">${name}</a></li>\n")
endforeach()
string(APPEND index "</ul>\n")
set(page "${RC_SITE}/licenses/index.html")
set(old "")
if(EXISTS "${page}")
    file(READ "${page}" old)
endif()
if(NOT old STREQUAL index)
    file(WRITE "${page}" "${index}")
endif()
]==])

        function(rc_web_example target out)
            set_target_properties(${target} PROPERTIES OUTPUT_NAME ${out} SUFFIX ".html")
            target_link_options(${target} PRIVATE
                -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3
                -sALLOW_MEMORY_GROWTH=1 -sGROWABLE_ARRAYBUFFERS=0 -sSTACK_SIZE=8MB
                -sENVIRONMENT=web -sMALLOC=emmalloc
                --shell-file "${RC_WEB_SHELL}"
                "SHELL:--output-eol linux")
            set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS
                "${RC_WEB_SHELL}" "${_rc_web_build_id}")
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" "-DRC_PAGE=$<TARGET_FILE:${target}>"
                        -P "${_rc_web_build_id}"
                VERBATIM)
            set_property(GLOBAL APPEND PROPERTY RC_WEB_PAGES ${target})
        endfunction()

        rc_web_example(rayclay_ex05_2020s_gui               dashboard)
        if(TARGET rayclay_ex25_cpp_kanban)
            rc_web_example(rayclay_ex25_cpp_kanban           kanban)
        endif()
        rc_web_example(rayclay_ex00_hello                   hello)
        rc_web_example(rayclay_ex01_1980s_gui               gui1980s)
        rc_web_example(rayclay_ex02_1990s_gui               gui1990s)
        rc_web_example(rayclay_ex03_2000s_gui               gui2000s)
        rc_web_example(rayclay_ex04_2010s_gui               gui2010s)
        rc_web_example(rayclay_ex06_widgets_gallery gallery)
        rc_web_example(rayclay_ex11_util_rc_inspector       inspector)
        rc_web_example(rayclay_ex12_signin_form             signin)
        rc_web_example(rayclay_ex20_system_monitor          sysmon)
        rc_web_example(rayclay_ex26_data_explorer           explorer)
        rc_web_example(rayclay_ex27_issue_inbox             inbox)
        rc_web_example(rayclay_ex28_ide                svg)
        rc_web_example(rayclay_ex15_file_explorer            files)
        rc_web_example(rayclay_ex14_file_selector            selector)
        rc_web_example(rayclay_ex24_messenger              messenger)
        rc_web_example(rayclay_ex22_markdown_notes                  notes)
        rc_web_example(rayclay_ex23_stocks_trader                 trader)
        rc_web_example(rayclay_ex29_dash_game             platformer)
        rc_web_example(rayclay_ex21_media_library                photos)

        target_compile_definitions(rayclay_ex06_widgets_gallery PRIVATE
            RC_DEMO_LOGO="/assets/rayclay-logo-1024.png"
            RC_DEMO_MONO="/assets/fonts/AdwaitaMono-Regular.ttf")
        target_link_options(rayclay_ex06_widgets_gallery PRIVATE
            "SHELL:--preload-file \"${RC_DEMO_PNG}@/assets/rayclay-logo-1024.png\""
            "SHELL:--preload-file \"${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/fonts/AdwaitaMono-Regular.ttf@/assets/fonts/AdwaitaMono-Regular.ttf\"")
        set_property(TARGET rayclay_ex06_widgets_gallery APPEND PROPERTY LINK_DEPENDS
                     "${RC_DEMO_PNG}"
                     "${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/fonts/AdwaitaMono-Regular.ttf")

        target_compile_definitions(rayclay_ex22_markdown_notes PRIVATE
            RC_EX22_FONT_DIR="/assets/fonts/"
            RC_EX22_ICON=NULL)
        target_link_options(rayclay_ex22_markdown_notes PRIVATE
            "SHELL:--preload-file \"${RC_EX22_FONTS}/Lato-Regular.ttf@/assets/fonts/Lato-Regular.ttf\""
            "SHELL:--preload-file \"${RC_EX22_FONTS}/Lato-Bold.ttf@/assets/fonts/Lato-Bold.ttf\""
            "SHELL:--preload-file \"${RC_EX22_FONTS}/Lato-Italic.ttf@/assets/fonts/Lato-Italic.ttf\""
            "SHELL:--preload-file \"${RC_EX22_FONTS}/AdwaitaMono-Regular.ttf@/assets/fonts/AdwaitaMono-Regular.ttf\"")
        set_property(TARGET rayclay_ex22_markdown_notes APPEND PROPERTY LINK_DEPENDS
                     "${RC_EX22_FONTS}/Lato-Regular.ttf" "${RC_EX22_FONTS}/Lato-Bold.ttf"
                     "${RC_EX22_FONTS}/Lato-Italic.ttf" "${RC_EX22_FONTS}/AdwaitaMono-Regular.ttf")
        set(RC_EX21_MEDIA "${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/media")
        target_compile_definitions(rayclay_ex21_media_library PRIVATE STORY_DIR="/assets/media/")
        target_link_options(rayclay_ex21_media_library PRIVATE
            "SHELL:--preload-file \"${RC_EX21_MEDIA}/harbor.mp4@/assets/media/harbor.mp4\""
            "SHELL:--preload-file \"${RC_EX21_MEDIA}/harbor.webm@/assets/media/harbor.webm\"")
        set_property(TARGET rayclay_ex21_media_library APPEND PROPERTY LINK_DEPENDS
                     "${RC_EX21_MEDIA}/harbor.mp4" "${RC_EX21_MEDIA}/harbor.webm")

        target_compile_definitions(rayclay_ex15_file_explorer PRIVATE RC_EX15_FONT_DIR="/assets/fonts/")
        target_link_options(rayclay_ex15_file_explorer PRIVATE
            "SHELL:--preload-file \"${RC_EX22_FONTS}/AdwaitaMono-Regular.ttf@/assets/fonts/AdwaitaMono-Regular.ttf\"")
        set_property(TARGET rayclay_ex15_file_explorer APPEND PROPERTY LINK_DEPENDS "${RC_EX22_FONTS}/AdwaitaMono-Regular.ttf")
        target_link_options(rayclay_ex29_dash_game PRIVATE
            "SHELL:--preload-file \"${RC_EX22_FONTS}/RussoOne-Regular.ttf@/examples/assets/fonts/RussoOne-Regular.ttf\"")
        set_property(TARGET rayclay_ex29_dash_game APPEND PROPERTY LINK_DEPENDS "${RC_EX22_FONTS}/RussoOne-Regular.ttf")
        target_link_options(rayclay_ex01_1980s_gui PRIVATE
            "SHELL:--preload-file \"${RC_EX22_FONTS}/Jersey10-Grid.ttf@/examples/assets/fonts/Jersey10-Grid.ttf\"")
        set_property(TARGET rayclay_ex01_1980s_gui APPEND PROPERTY LINK_DEPENDS "${RC_EX22_FONTS}/Jersey10-Grid.ttf")
        target_link_options(rayclay_ex11_util_rc_inspector PRIVATE
            "SHELL:--preload-file \"${RC_EX22_FONTS}/AdwaitaMono-Regular.ttf@/examples/assets/fonts/AdwaitaMono-Regular.ttf\"")
        set_property(TARGET rayclay_ex11_util_rc_inspector APPEND PROPERTY LINK_DEPENDS
                     "${RC_EX22_FONTS}/AdwaitaMono-Regular.ttf")
        set(RC_EX28_SVGS rayclay-logo)
        foreach(_svg IN LISTS RC_EX28_SVGS)
            target_link_options(rayclay_ex28_ide PRIVATE
                "SHELL:--preload-file \"${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/icons/${_svg}.svg@/assets/icons/${_svg}.svg\"")
            set_property(TARGET rayclay_ex28_ide APPEND PROPERTY LINK_DEPENDS
                         "${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/icons/${_svg}.svg")
        endforeach()
        target_link_options(rayclay_ex28_ide PRIVATE
            "SHELL:--preload-file \"${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/fonts/AdwaitaMono-Regular.ttf@/assets/fonts/AdwaitaMono-Regular.ttf\"")
        set_property(TARGET rayclay_ex28_ide APPEND PROPERTY LINK_DEPENDS
                     "${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/fonts/AdwaitaMono-Regular.ttf")
        target_compile_definitions(rayclay_ex28_ide PRIVATE
            RC_EX28_ICON_DIR="/assets/icons/"
            RC_EX28_FONT_DIR="/assets/fonts/")
        target_compile_definitions(rayclay_ex12_signin_form PRIVATE
            SIGNIN_LOGO_DIR="/assets/logos/")
        target_link_options(rayclay_ex12_signin_form PRIVATE
            "SHELL:--preload-file \"${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/logos/google-g.svg@/assets/logos/google-g.svg\"")
        set_property(TARGET rayclay_ex12_signin_form APPEND PROPERTY LINK_DEPENDS
                     "${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/logos/google-g.svg")

        add_custom_target(rayclay_web_hub ALL
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${CMAKE_CURRENT_SOURCE_DIR}/examples/web/hub.html
                    ${CMAKE_CURRENT_BINARY_DIR}/index.html
            COMMAND ${CMAKE_COMMAND} -DRC_PAGES=${CMAKE_CURRENT_BINARY_DIR}
                    -DRC_SITE=${CMAKE_CURRENT_BINARY_DIR}
                    -DRC_ASSETS=${CMAKE_CURRENT_SOURCE_DIR}/examples/assets
                    -DRC_NOTICES=${CMAKE_CURRENT_SOURCE_DIR}/third_parties/LICENSES.txt
                    -P ${_rc_web_notices}
            COMMENT "web: staging the demo hub as index.html, with the license notices it links"
            VERBATIM)
        get_property(_rc_web_pages GLOBAL PROPERTY RC_WEB_PAGES)
        add_dependencies(rayclay_web_hub ${_rc_web_pages})
    endif()

    set(_rc_examples
        rayclay_ex00_hello rayclay_ex01_1980s_gui rayclay_ex02_1990s_gui
        rayclay_ex03_2000s_gui rayclay_ex04_2010s_gui rayclay_ex05_2020s_gui
        rayclay_ex06_widgets_gallery
        rayclay_ex11_util_rc_inspector rayclay_ex12_signin_form rayclay_ex20_system_monitor
        rayclay_ex26_data_explorer
        rayclay_ex27_issue_inbox rayclay_ex28_ide
        rayclay_ex14_file_selector rayclay_ex15_file_explorer
        rayclay_ex21_media_library
        rayclay_ex24_messenger rayclay_ex22_markdown_notes rayclay_ex23_stocks_trader
        rayclay_ex29_dash_game)
    if(NOT EMSCRIPTEN)
        list(APPEND _rc_examples rayclay_ex10_util_rc_icon_converter
                                 rayclay_ex13_user_preferences
                                 rayclay_ex16_tab_navigator)
    endif()
    if(TARGET rayclay_ex25_cpp_kanban)
        list(APPEND _rc_examples rayclay_ex25_cpp_kanban)
    endif()
    if(RC_IOS)
        include(${CMAKE_CURRENT_SOURCE_DIR}/examples/ios/rc_example_assets.cmake)
    endif()
    set(_rc_portrait_examples rayclay_ex13_user_preferences rayclay_ex16_tab_navigator)

    foreach(_ex IN LISTS _rc_examples)
        target_link_libraries(${_ex} PRIVATE rayclay)
        target_include_directories(${_ex} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/assets
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/icons)
        get_target_property(_rc_ex_sources ${_ex} SOURCES)
        list(GET _rc_ex_sources 0 _rc_ex_first_source)
        get_filename_component(_rc_ex_src_dir "${_rc_ex_first_source}" DIRECTORY)
        target_include_directories(${_ex} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/${_rc_ex_src_dir})
        target_compile_options(${_ex} PRIVATE $<$<C_COMPILER_ID:MSVC>:/utf-8>)
        if(RC_IOS)
            string(REPLACE "_" "-" _rc_ios_bundle_id "${_ex}")
            if(CMAKE_GENERATOR STREQUAL "Xcode")
                set(_rc_ios_plist_id "$(PRODUCT_BUNDLE_IDENTIFIER)")
            else()
                set(_rc_ios_plist_id "org.rayclay.${_rc_ios_bundle_id}")
            endif()
            set_target_properties(${_ex} PROPERTIES
                MACOSX_BUNDLE ON
                MACOSX_BUNDLE_GUI_IDENTIFIER "${_rc_ios_plist_id}"
                MACOSX_BUNDLE_BUNDLE_NAME "${_ex}"
                MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
                MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
                XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER
                    "org.rayclay.${_rc_ios_bundle_id}$(DEVELOPMENT_TEAM)"
                XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")
            if(_ex IN_LIST _rc_portrait_examples)
                rc_ios_app(${_ex} ORIENTATION portrait)
            else()
                rc_ios_app(${_ex})
            endif()
            target_sources(${_ex} PRIVATE examples/ios/rc_ios_entry.c)
            target_compile_definitions(${_ex} PRIVATE main=rci_ios_example_main
                RCI_IOS_APP_NAME="${_ex}")
            target_compile_options(${_ex} PRIVATE
                "$<$<COMPILE_LANGUAGE:CXX>:-include;${CMAKE_CURRENT_SOURCE_DIR}/examples/ios/rc_ios_main_linkage.h>")
            get_property(_rc_assets TARGET ${_ex} PROPERTY RC_MOBILE_ASSETS)
            foreach(_rc_asset IN LISTS _rc_assets)
                get_filename_component(_rc_asset_dir "${_rc_asset}" DIRECTORY)
                set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/${_rc_asset}" PROPERTIES
                    MACOSX_PACKAGE_LOCATION "Resources/${_rc_asset_dir}")
                target_sources(${_ex} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${_rc_asset}")
            endforeach()
        endif()
    endforeach()
endif()

option(RC_BUILD_TESTS "Build the RayClay differential tests" ${RC_DESKTOP_TARGETS_DEFAULT})

if(RC_BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt
   AND NOT EMSCRIPTEN AND NOT ANDROID AND NOT RC_IOS)
    enable_testing()
    add_subdirectory(test)
endif()
