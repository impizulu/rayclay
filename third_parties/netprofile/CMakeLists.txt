cmake_minimum_required(VERSION 3.21)

if(CMAKE_SCRIPT_MODE_FILE AND DEFINED RC_NP_PATCH_CURL)
    function(rc_np_patch number file from to)
        set(path "${RC_NP_PATCH_CURL}/${file}")
        get_property(restored GLOBAL PROPERTY rc_np_patched)
        if(NOT file IN_LIST restored)
            if(EXISTS "${path}.rayclay-orig")
                file(COPY_FILE "${path}.rayclay-orig" "${path}")
            else()
                file(COPY_FILE "${path}" "${path}.rayclay-orig")
            endif()
            set_property(GLOBAL APPEND PROPERTY rc_np_patched "${file}")
        endif()
        file(READ "${path}" text)
        string(FIND "${text}" "${from}" first)
        string(FIND "${text}" "${from}" last REVERSE)
        if(first EQUAL -1 OR NOT first EQUAL last)
            message(FATAL_ERROR "rayclay_netprofile: RayClay patch ${number} does not match ${file} exactly once: "
                                "this curl tree is not the pinned tarball's")
        endif()
        string(REPLACE "${from}" "${to}" text "${text}")
        file(WRITE "${path}" "${text}")
    endfunction()
    rc_np_patch(1 lib/conncache.c [==[  bool healthy = TRUE;

  DEBUGASSERT(!data->conn);
  if(cpool_conn_can_be_closed(conn) && cpool_conn_too_old(data, conn, pnow))
    return FALSE;
  if((curlx_ptimediff_ms(pnow, &conn->created) - conn->lastchecked_ms) < 1000)
    return TRUE;
]==] [==[  bool healthy = TRUE;
  bool poll_input;

  DEBUGASSERT(!data->conn);
  if(cpool_conn_can_be_closed(conn) && cpool_conn_too_old(data, conn, pnow))
    return FALSE;

  /* RayClay patch 1, upstream ef6b17cf12d4:
   * An idle, non-multiplexed connection must not be reused when it has
   * input pending (RFC 9112, section 9.2), so that is polled on every
   * reuse. The once a second limit only applies to the other checks. */
  poll_input = cpool_conn_can_be_closed(conn) &&
    !Curl_conn_is_multiplex(conn, FIRSTSOCKET) &&
    !conn->scheme->run->connection_is_dead;
  if(!poll_input &&
     (curlx_ptimediff_ms(pnow, &conn->created) - conn->lastchecked_ms) < 1000)
    return TRUE;
]==])
    rc_np_patch(1 lib/conncache.c [==[    if(healthy && input_pending && cpool_conn_can_be_closed(conn) &&
       !Curl_conn_is_multiplex(conn, FIRSTSOCKET)) {]==] [==[    if(healthy && input_pending && poll_input) {]==])
    rc_np_patch(2 lib/transfer.c [==[    return CURLE_OK;

  if(conn->bits.reuse &&]==] [==[    return CURLE_OK;

  /* RayClay patch 2: only a GET or a HEAD is ever sent again. The server
     may have read and acted on any other request before its connection
     ended, and only the application knows whether it may run twice. */
  if(conn->scheme->protocol & PROTO_FAMILY_HTTP) {
    const char *method;
    Curl_HttpReq httpreq;

    Curl_http_method(data, &method, &httpreq);
    if(strcmp(method, "GET") && strcmp(method, "HEAD"))
      return CURLE_OK;
  }

  if(conn->bits.reuse &&]==])
    rc_np_patch(2 lib/url.c [==[  if((!data->set.reuse_fresh || data->state.followlocation) &&
     !data->set.connect_only) {]==] [==[  /* RayClay patch 2: a retry (Curl_retry_request) takes a new connection,
     which it never retries, as it retries only a reused one: one resend. */
  if((!data->set.reuse_fresh || data->state.followlocation) &&
     !data->set.connect_only && !data->state.retrycount) {]==])
    rc_np_patch(3 lib/vtls/mbedtls.c [==[  size_t send_blocked_len;
]==] [==[  size_t send_blocked_len;
  CURLcode last_io_result; /* RayClay patch 3: the BIO's last send or recv */
]==])
    rc_np_patch(3 lib/vtls/mbedtls.c [==[  CURL_TRC_CF(data, cf, "mbedtls_bio_cf_out_write(len=%zu) -> %d, %zu",
              blen, (int)result, nwritten);
]==] [==[  CURL_TRC_CF(data, cf, "mbedtls_bio_cf_out_write(len=%zu) -> %d, %zu",
              blen, (int)result, nwritten);
  /* RayClay patch 3 */
  ((struct mbed_ssl_backend_data *)
   ((struct ssl_connect_data *)cf->ctx)->backend)->last_io_result = result;
]==])
    rc_np_patch(3 lib/vtls/mbedtls.c [==[  CURL_TRC_CF(data, cf, "mbedtls_bio_cf_in_read(len=%zu) -> %d, %zu",
              blen, (int)result, nread);
]==] [==[  CURL_TRC_CF(data, cf, "mbedtls_bio_cf_in_read(len=%zu) -> %d, %zu",
              blen, (int)result, nread);
  /* RayClay patch 3 */
  ((struct mbed_ssl_backend_data *)
   ((struct ssl_connect_data *)cf->ctx)->backend)->last_io_result = result;
]==])
    rc_np_patch(3 lib/vtls/mbedtls.c [==[    return CURLE_PEER_FAILED_VERIFICATION;
  }
  else if(ret) {]==] [==[    return CURLE_PEER_FAILED_VERIFICATION;
  }
  else if(ret == MBEDTLS_ERR_SSL_CONN_EOF ||
          (backend->last_io_result && backend->last_io_result != CURLE_AGAIN)) {
    /* RayClay patch 3: the connection closed or failed under the handshake,
       the transport's failure, as the same event is after it, not TLS's */
    failf(data, "the connection %s during the TLS handshake",
          ret == MBEDTLS_ERR_SSL_CONN_EOF ? "closed" : "failed");
    return ret == MBEDTLS_ERR_SSL_CONN_EOF ? CURLE_RECV_ERROR :
           backend->last_io_result;
  }
  else if(ret) {]==])
    message(STATUS "rayclay_netprofile: curl carries RayClay patches 1, 2 and 3")
    return()
endif()

if(CMAKE_SCRIPT_MODE_FILE)
    execute_process(COMMAND ${RC_NP_CHECK_NM} -g --defined-only -P ${RC_NP_CHECK_MERGED}
                    RESULT_VARIABLE _rc_np_rc OUTPUT_VARIABLE _rc_np_out ERROR_VARIABLE _rc_np_err)
    set(_rc_np_elf "unreadable")
    if(EXISTS ${RC_NP_CHECK_MERGED})
        file(READ ${RC_NP_CHECK_MERGED} _rc_np_hex LIMIT 20 HEX)
        if(_rc_np_hex MATCHES "^7f454c46(....)........................(....)")
            set(_rc_np_elf "${CMAKE_MATCH_1}:${CMAKE_MATCH_2}")
        endif()
    endif()
    if(NOT _rc_np_rc EQUAL 0 OR _rc_np_out STREQUAL "")
        file(REMOVE ${RC_NP_CHECK_ARCHIVE})
        message(FATAL_ERROR "rayclay_netprofile: '${RC_NP_CHECK_NM}' failed (${_rc_np_rc}) or listed nothing on "
                            "${RC_NP_CHECK_MERGED}: ${_rc_np_err}")
    endif()
    if(_rc_np_out MATCHES "(^|\n)mbedtls_ct_zero ")
        file(REMOVE ${RC_NP_CHECK_ARCHIVE})
        message(FATAL_ERROR "rayclay_netprofile: the merged object defines mbedtls_ct_zero, TF-PSA-Crypto's "
                            "generic-C constant-time object: the upstreams were built without the constant-time "
                            "assembly the admission probe read (CVE-2025-66442)")
    endif()
    if(NOT _rc_np_elf STREQUAL RC_NP_CHECK_ELF)
        file(REMOVE ${RC_NP_CHECK_ARCHIVE})
        message(FATAL_ERROR "rayclay_netprofile: the merged object's ELF header (${_rc_np_elf}) is not the "
                            "admission probe's (${RC_NP_CHECK_ELF})")
    endif()
    execute_process(COMMAND ${RC_NP_CHECK_NM} -g --defined-only -P ${RC_NP_CHECK_ARCHIVE}
                    RESULT_VARIABLE _rc_np_rc OUTPUT_VARIABLE _rc_np_out ERROR_VARIABLE _rc_np_err)
    if(NOT _rc_np_rc EQUAL 0)
        file(REMOVE ${RC_NP_CHECK_ARCHIVE})
        message(FATAL_ERROR "rayclay_netprofile: '${RC_NP_CHECK_NM}' failed (${_rc_np_rc}) on "
                            "${RC_NP_CHECK_ARCHIVE}: ${_rc_np_err}")
    endif()
    string(REPLACE "\n" ";" _rc_np_lines "${_rc_np_out}")
    set(_rc_np_names "")
    foreach(_rc_np_line IN LISTS _rc_np_lines)
        if(_rc_np_line STREQUAL "" OR _rc_np_line MATCHES ":$")
            continue()
        endif()
        string(REGEX MATCH "^[^ ]+" _rc_np_name "${_rc_np_line}")
        list(APPEND _rc_np_names "${_rc_np_name}")
    endforeach()
    file(STRINGS ${RC_NP_CHECK_NPH} _rc_np_def REGEX "^#define[ \t]+RCI_NET_PROFILE_ANCHOR[ \t]+")
    string(REGEX REPLACE "^#define[ \t]+RCI_NET_PROFILE_ANCHOR[ \t]+([A-Za-z0-9_]+).*" "\\1" _rc_np_anchor
           "${_rc_np_def}")
    list(LENGTH _rc_np_names _rc_np_count)
    if(_rc_np_anchor STREQUAL "" OR NOT _rc_np_count EQUAL 1 OR NOT _rc_np_names STREQUAL _rc_np_anchor)
        if(_rc_np_count GREATER 5)
            list(SUBLIST _rc_np_names 0 5 _rc_np_names)
            list(APPEND _rc_np_names "...")
        endif()
        file(REMOVE ${RC_NP_CHECK_ARCHIVE})
        message(FATAL_ERROR "rayclay_netprofile: the archive must define exactly one global, "
                            "'${_rc_np_anchor}'; it defines ${_rc_np_count}: ${_rc_np_names}")
    endif()
    message(STATUS "rayclay_netprofile: the archive defines exactly one global, ${_rc_np_anchor}")
    return()
endif()

project(rayclay_netprofile LANGUAGES C)
set_property(GLOBAL PROPERTY rayclay_netprofile_added TRUE)
find_package(Threads REQUIRED)
if(NOT DEFINED RC_NETPROFILE_TARBALLS)
    set(RC_NETPROFILE_TARBALLS "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
include(ExternalProject)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/rc_net_profile.h _rc_np_def
     REGEX "^#define[ \t]+RCI_NET_PROFILE_ANCHOR[ \t]+")
string(REGEX REPLACE "^#define[ \t]+RCI_NET_PROFILE_ANCHOR[ \t]+([A-Za-z0-9_]+).*" "\\1" RC_NP_ANCHOR
       "${_rc_np_def}")

foreach(_rc_np_tool CMAKE_LINKER CMAKE_OBJCOPY CMAKE_AR CMAKE_NM)
    if(NOT ${_rc_np_tool})
        if(PROJECT_IS_TOP_LEVEL)
            message(FATAL_ERROR "rayclay_netprofile: ${_rc_np_tool} not found; the network profile cannot be built")
        endif()
        message(WARNING "rayclay_netprofile: ${_rc_np_tool} not found, so the network profile is not built and "
                        "rcFetch, rcWebSocket and rcUpdateCheck programs will not link (undefined reference to "
                        "'${RC_NP_ANCHOR}')")
        return()
    endif()
endforeach()

set(NP ${CMAKE_CURRENT_BINARY_DIR})
set(NPP ${NP}/prefix)
set(NP_PSA_CFG ${CMAKE_CURRENT_SOURCE_DIR}/rc_net_profile_psa.h)
set(_rc_np_brace "{[^}]*(,|\\.\\.)[^}]*}")
set(_rc_np_src "[#$|]")
if(CMAKE_GENERATOR STREQUAL "Unix Makefiles")
    set(_rc_np_src "[#$|:]")
endif()
set(_rc_np_path "")
string(REGEX MATCH "[]['()&|<>`$*?:]|${_rc_np_brace}" _rc_np_c "${NP}")
if(NOT _rc_np_c STREQUAL "")
    set(_rc_np_path "the build directory '${NP}' holds '${_rc_np_c}'")
else()
    string(REGEX MATCH "${_rc_np_src}|${_rc_np_brace}" _rc_np_c "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT _rc_np_c STREQUAL "")
        set(_rc_np_path "the source directory '${CMAKE_CURRENT_SOURCE_DIR}' holds '${_rc_np_c}'")
    endif()
endif()
if(NOT _rc_np_path STREQUAL "")
    string(CONCAT _rc_np_text "${_rc_np_path}, which the build of curl, Mbed TLS and zlib cannot take: build "
           "from a path without it")
    if(PROJECT_IS_TOP_LEVEL)
        message(FATAL_ERROR "rayclay_netprofile: the network profile cannot be built: ${_rc_np_text}. A program "
                            "that calls none of rcFetch, rcWebSocket and rcUpdateCheck needs no profile: "
                            "compile it with -DRC_NO_NET to compile networking out (RayClay's CMake: "
                            "-DRC_NO_NET=ON).")
    endif()
    message(STATUS "rayclay_netprofile: not built: ${_rc_np_text}. Programs that call none of rcFetch, "
                   "rcWebSocket and rcUpdateCheck build; one that calls any of them fails at link (undefined "
                   "reference to '${RC_NP_ANCHOR}'). RC_NO_NET=ON compiles networking out.")
    return()
endif()
set(NP_MBEDTLS mbedtls-4.1.1)
set(NP_MBEDTLS_SHA256 3359a349e23db3d5536fcee032ae7b2ecbfc08972fab643089b5cbf2a375c98c)
set(NP_CFLAGS -fPIC -fvisibility=hidden -ffunction-sections -fdata-sections -fno-common "-ffile-prefix-map=${NP}=.")
include(CheckCCompilerFlag)
list(APPEND NP_CFLAGS -fstack-protector-strong)
check_c_compiler_flag(-fstack-clash-protection RC_NP_STACK_CLASH)
if(RC_NP_STACK_CLASH)
    list(APPEND NP_CFLAGS -fstack-clash-protection)
endif()
set(NP_OPT "-Os -DNDEBUG -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2")
set(NP_DEF_CFG "-DTF_PSA_CRYPTO_USER_CONFIG_FILE=\"${NP_PSA_CFG}\"")
function(rc_np_c_flags _out)
    set(_l ${ARGN})
    list(TRANSFORM _l REPLACE "([^A-Za-z0-9_./=:,+@%-])" "\\\\\\1")
    list(JOIN _l " " _s)
    set(${_out} "${_s}" PARENT_SCOPE)
endfunction()

get_property(_rc_np_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_rc_np_multi)
    set(NP_ARGS -DCMAKE_CONFIGURATION_TYPES=MinSizeRel)
else()
    set(NP_ARGS -DCMAKE_BUILD_TYPE=MinSizeRel)
endif()
list(APPEND NP_ARGS -DCMAKE_INSTALL_PREFIX=${NPP} -DCMAKE_INSTALL_LIBDIR=lib
    "-DCMAKE_C_FLAGS_MINSIZEREL=${NP_OPT}"
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF -DBUILD_SHARED_LIBS=OFF
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER})
foreach(_rc_np_var CMAKE_TOOLCHAIN_FILE ANDROID_ABI ANDROID_PLATFORM CMAKE_C_COMPILER_TARGET CMAKE_SYSROOT
        CMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN)
    if(NOT "${${_rc_np_var}}" STREQUAL "")
        list(APPEND NP_ARGS -D${_rc_np_var}=${${_rc_np_var}})
    endif()
endforeach()
if(CMAKE_CROSSCOMPILING AND "${CMAKE_TOOLCHAIN_FILE}" STREQUAL "")
    list(APPEND NP_ARGS -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME} -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR})
endif()

set(NP_ADMITTED "linux|x86_64|GNU|x86_64-64|-|product" "linux|x86_64|Clang|x86_64-64|-|product"
    "android|aarch64|Clang|aarch64-64|19|product")
string(CONCAT NP_QUALIFIED "RayClay qualified the Linux profile with GCC 15.3 and Clang 21.1, and measured GCC 12 "
       "to 14 and Clang 18 to 20 the same way; another GCC or Clang version is admitted by this probe and the "
       "build's check of its objects alone")
string(CONCAT NP_QUALIFIED_ANDROID "RayClay qualified the Android profile with the NDK r28c's Clang 19.0.1 "
       "(this probe, the build's check of its objects and HTTPS on a Pixel 8a); a later NDK Clang is admitted by "
       "the probe and that check alone")
set(NP_ADMIT ${NP}/admit)
set(NP_MBEDTLS_TARBALL ${RC_NETPROFILE_TARBALLS}/${NP_MBEDTLS}.tar.bz2)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${NP_PSA_CFG} ${NP_MBEDTLS_TARBALL})
if(NOT EXISTS ${NP_MBEDTLS_TARBALL})
    message(FATAL_ERROR "rayclay_netprofile: ${NP_MBEDTLS_TARBALL} is missing")
endif()
file(SHA256 ${NP_MBEDTLS_TARBALL} _rc_np_sha)
if(NOT _rc_np_sha STREQUAL NP_MBEDTLS_SHA256)
    message(FATAL_ERROR "rayclay_netprofile: ${NP_MBEDTLS_TARBALL} hashes to ${_rc_np_sha}, not its pin "
                        "${NP_MBEDTLS_SHA256}")
endif()
set(_rc_np_tf ${NP_ADMIT}/${NP_MBEDTLS}/tf-psa-crypto)
set(_rc_np_stamp "")
if(EXISTS ${NP_ADMIT}/headers.sha256)
    file(READ ${NP_ADMIT}/headers.sha256 _rc_np_stamp)
endif()
if(NOT _rc_np_stamp STREQUAL _rc_np_sha)
    file(REMOVE_RECURSE ${NP_ADMIT}/${NP_MBEDTLS})
    file(ARCHIVE_EXTRACT INPUT ${NP_MBEDTLS_TARBALL} DESTINATION ${NP_ADMIT} PATTERNS
         ${NP_MBEDTLS}/tf-psa-crypto/include ${NP_MBEDTLS}/tf-psa-crypto/drivers/builtin/include
         ${NP_MBEDTLS}/tf-psa-crypto/core ${NP_MBEDTLS}/tf-psa-crypto/utilities)
    file(WRITE ${NP_ADMIT}/headers.sha256 "${_rc_np_sha}")
endif()
string(CONCAT _rc_np_inc "\"${_rc_np_tf}/include\" \"${_rc_np_tf}/drivers/builtin/include\" "
       "\"${_rc_np_tf}/core\" \"${_rc_np_tf}/utilities\"")
file(CONFIGURE OUTPUT ${NP_ADMIT}/proj/CMakeLists.txt @ONLY CONTENT [=[
cmake_minimum_required(VERSION 3.21)
project(rayclay_np_admit LANGUAGES C)
add_library(rayclay_np_admit STATIC probe.c)
target_include_directories(rayclay_np_admit PRIVATE @_rc_np_inc@)
set_target_properties(rayclay_np_admit PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/effective-$<CONFIG>.txt CONTENT "$<TARGET_FILE:rayclay_np_admit>
${CMAKE_SYSTEM_NAME}|${CMAKE_SYSTEM_PROCESSOR}|${CMAKE_C_COMPILER_ID}|${CMAKE_C_COMPILER_VERSION}|\
${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_MINSIZEREL}
")
]=])
file(CONFIGURE OUTPUT ${NP_ADMIT}/proj/probe.c @ONLY CONTENT [=[
#include "tf_psa_crypto_config.c"
#include "constant_time_internal.h"
#define RCI_NET_PROFILE_ADMIT_S(x) #x
#define RCI_NET_PROFILE_ADMIT_X(x) RCI_NET_PROFILE_ADMIT_S(x)
#if defined(__ANDROID__)
#define RCI_NET_PROFILE_ADMIT_OS "android"
#elif defined(__linux__)
#define RCI_NET_PROFILE_ADMIT_OS "linux"
#else
#define RCI_NET_PROFILE_ADMIT_OS "other"
#endif
#if defined(__x86_64__)
#define RCI_NET_PROFILE_ADMIT_ARCH "x86_64"
#elif defined(__aarch64__)
#define RCI_NET_PROFILE_ADMIT_ARCH "aarch64"
#elif defined(__i386__)
#define RCI_NET_PROFILE_ADMIT_ARCH "i386"
#elif defined(__arm__)
#define RCI_NET_PROFILE_ADMIT_ARCH "arm"
#else
#define RCI_NET_PROFILE_ADMIT_ARCH "other"
#endif
#if defined(__clang__)
#define RCI_NET_PROFILE_ADMIT_CC "Clang|" RCI_NET_PROFILE_ADMIT_X(__clang_major__) "." \
    RCI_NET_PROFILE_ADMIT_X(__clang_minor__) "." RCI_NET_PROFILE_ADMIT_X(__clang_patchlevel__)
#elif defined(__GNUC__)
#define RCI_NET_PROFILE_ADMIT_CC "GNU|" RCI_NET_PROFILE_ADMIT_X(__GNUC__) "." \
    RCI_NET_PROFILE_ADMIT_X(__GNUC_MINOR__) "." RCI_NET_PROFILE_ADMIT_X(__GNUC_PATCHLEVEL__)
#else
#define RCI_NET_PROFILE_ADMIT_CC "other|0"
#endif
#if !defined(MBEDTLS_HAVE_ASM)
#define RCI_NET_PROFILE_ADMIT_CT "no-asm"
#elif defined(MBEDTLS_CT_X86_64_ASM) && defined(MBEDTLS_CT_SIZE_64)
#define RCI_NET_PROFILE_ADMIT_CT "x86_64-64"
#elif defined(MBEDTLS_CT_AARCH64_ASM) && defined(MBEDTLS_CT_SIZE_64)
#define RCI_NET_PROFILE_ADMIT_CT "aarch64-64"
#elif defined(MBEDTLS_CT_X86_ASM) && defined(MBEDTLS_CT_SIZE_32)
#define RCI_NET_PROFILE_ADMIT_CT "x86-32"
#elif defined(MBEDTLS_CT_ARM_ASM) && defined(MBEDTLS_CT_SIZE_32)
#define RCI_NET_PROFILE_ADMIT_CT "arm-32"
#else
#define RCI_NET_PROFILE_ADMIT_CT "generic-c"
#endif
const char rci_net_profile_admit_info[] = "RCNPADMIT<" RCI_NET_PROFILE_ADMIT_OS "|" RCI_NET_PROFILE_ADMIT_ARCH
    "|" RCI_NET_PROFILE_ADMIT_CC "|" RCI_NET_PROFILE_ADMIT_CT "|" TF_PSA_CRYPTO_VERSION_STRING ">";
]=])
function(rc_np_admit_probe _n _cflags _out _mach _eff)
    set(_b ${NP_ADMIT}/build)
    set(_args -G ${CMAKE_GENERATOR} ${NP_ARGS})
    if(CMAKE_MAKE_PROGRAM)
        list(APPEND _args -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM})
    endif()
    string(SHA256 _key "${_args}")
    set(_was "")
    if(EXISTS ${_b}/rayclay-np-args.sha256)
        file(READ ${_b}/rayclay-np-args.sha256 _was)
    endif()
    if(NOT _was STREQUAL _key)
        file(REMOVE_RECURSE ${_b})
    endif()
    file(REMOVE ${_b}/effective-MinSizeRel.txt)
    execute_process(COMMAND ${CMAKE_COMMAND} ${_args} "-DCMAKE_C_FLAGS=${_cflags}" -S ${NP_ADMIT}/proj -B ${_b}
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _log ERROR_VARIABLE _log)
    if(_rc EQUAL 0)
        file(WRITE ${_b}/rayclay-np-args.sha256 "${_key}")
        execute_process(COMMAND ${CMAKE_COMMAND} --build ${_b} --config MinSizeRel --clean-first --verbose
                        RESULT_VARIABLE _rc OUTPUT_VARIABLE _blog ERROR_VARIABLE _blog)
        string(APPEND _log "${_blog}")
    endif()
    file(WRITE ${NP_ADMIT}/probe-${_n}.log "${_log}")
    set(_tuple "")
    set(_m "unreadable")
    set(_e "")
    set(_lib "")
    if(_rc EQUAL 0 AND EXISTS ${_b}/effective-MinSizeRel.txt)
        file(READ ${_b}/effective-MinSizeRel.txt _text)
        string(REGEX REPLACE "\n.*" "" _lib "${_text}")
        string(REGEX REPLACE "^[^\n]*\n([^\n]*).*" "\\1" _e "${_text}")
    endif()
    if(NOT _lib STREQUAL "" AND EXISTS ${_lib})
        file(STRINGS ${_lib} _info REGEX "RCNPADMIT<[^>]*>")
        if(_info MATCHES "RCNPADMIT<([^>]*)>")
            set(_tuple "${CMAKE_MATCH_1}")
        endif()
        file(READ ${_lib} _hex HEX)
        string(FIND "${_hex}" "7f454c46" _at)
        if(_at GREATER_EQUAL 0)
            math(EXPR _odd "${_at} % 2")
            math(EXPR _id "${_at} + 8")
            math(EXPR _mc "${_at} + 36")
            if(_odd EQUAL 0)
                string(SUBSTRING "${_hex}" ${_id} 4 _ident)
                string(SUBSTRING "${_hex}" ${_mc} 4 _machine)
                set(_m "${_ident}:${_machine}")
            endif()
        endif()
    endif()
    set(${_out} "${_tuple}" PARENT_SCOPE)
    set(${_mach} "${_m}" PARENT_SCOPE)
    set(${_eff} "${_e}" PARENT_SCOPE)
endfunction()
rc_np_c_flags(_rc_np_c ${NP_CFLAGS} ${NP_DEF_CFG})
rc_np_admit_probe(1 "${_rc_np_c}" _rc_np_tuple _rc_np_elf _rc_np_eff)
set(_rc_np_elf_arch "other")
foreach(_rc_np_pair "0201:3e00=x86_64" "0201:b700=aarch64" "0101:0300=i386" "0101:2800=arm")
    string(REPLACE "=" ";" _rc_np_pair "${_rc_np_pair}")
    list(GET _rc_np_pair 0 _rc_np_key)
    if(_rc_np_elf STREQUAL _rc_np_key)
        list(GET _rc_np_pair 1 _rc_np_elf_arch)
    endif()
endforeach()
if(_rc_np_elf_arch STREQUAL "x86_64")
    list(APPEND NP_CFLAGS -fcf-protection=full)
elseif(_rc_np_elf_arch STREQUAL "aarch64")
    list(APPEND NP_CFLAGS -mbranch-protection=standard)
endif()
rc_np_c_flags(NP_C ${NP_CFLAGS})
rc_np_c_flags(NP_C_CFG ${NP_CFLAGS} ${NP_DEF_CFG})
set(_rc_np_tuple2 "")
set(_rc_np_elf2 "")
set(_rc_np_eff2 "")
if(NOT _rc_np_tuple STREQUAL "")
    rc_np_admit_probe(2 "${NP_C_CFG}" _rc_np_tuple2 _rc_np_elf2 _rc_np_eff2)
endif()
string(REPLACE "|" ";" _rc_np_f "${_rc_np_tuple}|-|-|-|-|-|-")
list(GET _rc_np_f 0 _rc_np_os)
list(GET _rc_np_f 1 _rc_np_arch)
list(GET _rc_np_f 2 _rc_np_cc)
list(GET _rc_np_f 3 _rc_np_ver)
list(GET _rc_np_f 4 _rc_np_ct)
set(_rc_np_here "${CMAKE_SYSTEM_NAME}|${CMAKE_SYSTEM_PROCESSOR}|${CMAKE_C_COMPILER_ID}|${CMAKE_C_COMPILER_VERSION}")
set(_rc_np_there "")
set(_rc_np_flags "")
if(_rc_np_eff2 MATCHES "^([^|]*\\|[^|]*\\|[^|]*\\|[^|]*)\\|(.*)$")
    set(_rc_np_there "${CMAKE_MATCH_1}")
    string(STRIP "${CMAKE_MATCH_2}" _rc_np_flags)
    rc_np_c_flags(_rc_np_bin "${NP}")
    string(REPLACE "${_rc_np_bin}" "<build>" _rc_np_flags "${_rc_np_flags}")
endif()
string(TOLOWER "${CMAKE_SYSTEM_NAME}" _rc_np_sys)
if(NOT _rc_np_sys MATCHES "^(linux|android)$")
    set(_rc_np_sys other)
endif()
set(_rc_np_proc "${CMAKE_SYSTEM_PROCESSOR}")
if(_rc_np_proc MATCHES "^(x86_64|AMD64|amd64|x64)$")
    set(_rc_np_proc x86_64)
elseif(_rc_np_proc MATCHES "^(aarch64|arm64|ARM64)$")
    set(_rc_np_proc aarch64)
elseif(_rc_np_proc MATCHES "^(i[3-6]86|x86)$")
    set(_rc_np_proc i386)
elseif(_rc_np_proc MATCHES "^(arm|armv[0-9].*)$")
    set(_rc_np_proc arm)
else()
    set(_rc_np_proc other)
endif()
set(_rc_np_use "")
set(_rc_np_why "")
if(NOT "${CMAKE_C_COMPILER_ARG1}" STREQUAL "")
    string(CONCAT _rc_np_why "the C compiler is named with arguments "
           "('${CMAKE_C_COMPILER}${CMAKE_C_COMPILER_ARG1}'), which the upstream builds would not get: give them in "
           "a toolchain file")
elseif(_rc_np_tuple STREQUAL "")
    string(CONCAT _rc_np_why "the admission probe does not configure or compile with this compiler and "
           "configuration (its output: ${NP_ADMIT}/probe-1.log)")
elseif(NOT _rc_np_there STREQUAL _rc_np_here)
    string(CONCAT _rc_np_why "the upstream builds would configure for '${_rc_np_there}', this project for "
           "'${_rc_np_here}' (system|processor|compiler|version)")
elseif(NOT CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$")
    string(CONCAT _rc_np_why "the compiler ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION} is not one RayClay "
           "admits (GCC or Clang)")
elseif(NOT _rc_np_os STREQUAL _rc_np_sys)
    set(_rc_np_why "CMAKE_SYSTEM_NAME is ${CMAKE_SYSTEM_NAME}, but the compiler builds for ${_rc_np_os}")
elseif(NOT _rc_np_proc STREQUAL _rc_np_arch)
    string(CONCAT _rc_np_why "CMAKE_SYSTEM_PROCESSOR is ${CMAKE_SYSTEM_PROCESSOR}, but the compiler builds for "
           "${_rc_np_arch}")
elseif(_rc_np_os STREQUAL "other")
    set(_rc_np_why "the compiler builds for neither Linux nor Android")
elseif(NOT _rc_np_elf_arch STREQUAL _rc_np_arch)
    set(_rc_np_why "the probe object's ELF header (${_rc_np_elf}) is not the ${_rc_np_arch} its macros report")
elseif(NOT CMAKE_C_COMPILER_ID STREQUAL _rc_np_cc OR NOT CMAKE_C_COMPILER_VERSION STREQUAL _rc_np_ver)
    string(CONCAT _rc_np_why "CMake detected ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}, but "
           "${_rc_np_cc} ${_rc_np_ver} compiled the probe")
elseif(NOT _rc_np_tuple2 STREQUAL _rc_np_tuple OR NOT _rc_np_elf2 STREQUAL _rc_np_elf)
    string(CONCAT _rc_np_why "the final flags change the probe's answer to '${_rc_np_tuple2}', ELF "
           "${_rc_np_elf2} (its output: ${NP_ADMIT}/probe-2.log)")
elseif(_rc_np_ct STREQUAL "no-asm")
    string(CONCAT _rc_np_why "MBEDTLS_HAVE_ASM is off in the effective configuration, so TF-PSA-Crypto's "
           "constant-time primitives are plain C, which CVE-2025-66442 says a compiler can break")
else()
    if(_rc_np_ct STREQUAL "generic-c")
        set(_rc_np_why "TF-PSA-Crypto takes no constant-time assembly branch for this compiler and target")
    else()
        set(_rc_np_why "RayClay has not qualified the profile for this compiler and target")
    endif()
    foreach(_rc_np_row IN LISTS NP_ADMITTED)
        string(REPLACE "|" ";" _rc_np_row "${_rc_np_row}")
        list(GET _rc_np_row 0 _rc_np_a_os)
        list(GET _rc_np_row 1 _rc_np_a_arch)
        list(GET _rc_np_row 2 _rc_np_a_cc)
        list(GET _rc_np_row 3 _rc_np_a_ct)
        list(GET _rc_np_row 4 _rc_np_a_floor)
        if(_rc_np_os STREQUAL _rc_np_a_os AND _rc_np_arch STREQUAL _rc_np_a_arch AND _rc_np_cc STREQUAL _rc_np_a_cc
           AND _rc_np_ct STREQUAL _rc_np_a_ct)
            if(NOT _rc_np_a_floor STREQUAL "-" AND _rc_np_ver VERSION_LESS _rc_np_a_floor)
                string(CONCAT _rc_np_why "${_rc_np_cc} ${_rc_np_ver} is older than the oldest qualified for "
                       "this target (${_rc_np_a_floor})")
            else()
                list(GET _rc_np_row 5 _rc_np_use)
                set(_rc_np_why "")
            endif()
        endif()
    endforeach()
endif()
if(_rc_np_tuple STREQUAL "")
    set(_rc_np_said "an unidentified target")
else()
    set(_rc_np_said "${_rc_np_os}|${_rc_np_arch}|${_rc_np_cc} ${_rc_np_ver}|${_rc_np_ct}")
endif()
string(APPEND _rc_np_said " (ELF ${_rc_np_elf}; declared ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR})")
if(_rc_np_flags STREQUAL "")
    set(_rc_np_flagtext "")
else()
    set(_rc_np_flagtext " The upstreams' C flags: ${_rc_np_flags}.")
endif()
if(NOT _rc_np_why STREQUAL "")
    set(_rc_np_list "")
    foreach(_rc_np_row IN LISTS NP_ADMITTED)
        string(REPLACE "|" ";" _rc_np_row "${_rc_np_row}")
        list(GET _rc_np_row 0 _rc_np_a_os)
        list(GET _rc_np_row 1 _rc_np_a_arch)
        list(GET _rc_np_row 2 _rc_np_a_cc)
        list(GET _rc_np_row 4 _rc_np_a_floor)
        list(GET _rc_np_row 5 _rc_np_a_use)
        if(_rc_np_a_floor STREQUAL "-")
            set(_rc_np_a_floor "of any version")
        else()
            set(_rc_np_a_floor ">= ${_rc_np_a_floor}")
        endif()
        string(CONCAT _rc_np_item "${_rc_np_a_os} ${_rc_np_a_arch} with ${_rc_np_a_cc} ${_rc_np_a_floor} "
               "(${_rc_np_a_use})")
        list(APPEND _rc_np_list "${_rc_np_item}")
    endforeach()
    string(REPLACE ";" ", " _rc_np_list "${_rc_np_list}")
    string(CONCAT _rc_np_text "the network profile is not admitted for ${_rc_np_said}: ${_rc_np_why}. It "
           "needs Mbed TLS's constant-time assembly (the upstream mitigation for advisory CVE-2025-66442) and "
           "RayClay's own qualification, so it is built only for: ${_rc_np_list}")
    if(PROJECT_IS_TOP_LEVEL)
        message(FATAL_ERROR "rayclay_netprofile: ${_rc_np_text}. Build it with an admitted compiler and target. "
                            "A program that calls none of rcFetch, rcWebSocket and rcUpdateCheck needs no "
                            "profile: compile it with -DRC_NO_NET to compile networking out (RayClay's CMake: "
                            "-DRC_NO_NET=ON).${_rc_np_flagtext}")
    endif()
    message(STATUS "rayclay_netprofile: not built: ${_rc_np_text}. Programs that call none of rcFetch, "
                   "rcWebSocket and rcUpdateCheck build; one that calls any of them fails at link (undefined "
                   "reference to '${RC_NP_ANCHOR}'). RC_NO_NET=ON compiles networking out.${_rc_np_flagtext}")
    return()
endif()
if(_rc_np_use STREQUAL "product" AND _rc_np_os STREQUAL "android")
    set(_rc_np_use "product. ${NP_QUALIFIED_ANDROID}")
elseif(_rc_np_use STREQUAL "product")
    set(_rc_np_use "product. ${NP_QUALIFIED}")
endif()
message(STATUS "rayclay_netprofile: admitted ${_rc_np_said}, ${_rc_np_use}.${_rc_np_flagtext}")

set(NP_EP_STEPS "")
if(_rc_np_multi)
    set(NP_EP_STEPS BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config MinSizeRel
        INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config MinSizeRel --target install)
endif()
ExternalProject_Add(rayclay_np_zlib PREFIX ${NP}/ep-zlib-1.3.2
    URL ${RC_NETPROFILE_TARBALLS}/zlib-1.3.2.tar.xz
    URL_HASH SHA256=d7a0654783a4da529d1bb793b7ad9c3318020af77667bcae35f95d0e42a792f3
    CMAKE_ARGS ${NP_ARGS} "-DCMAKE_C_FLAGS=${NP_C}"
               -DZLIB_BUILD_TESTING=OFF -DZLIB_BUILD_SHARED=OFF -DZLIB_BUILD_STATIC=ON
    ${NP_EP_STEPS}
    BUILD_BYPRODUCTS ${NPP}/lib/libz.a)
ExternalProject_Add(rayclay_np_mbedtls PREFIX ${NP}/ep-${NP_MBEDTLS}
    URL ${NP_MBEDTLS_TARBALL}
    URL_HASH SHA256=${NP_MBEDTLS_SHA256}
    DEPENDS rayclay_np_zlib
    CMAKE_ARGS ${NP_ARGS} "-DCMAKE_C_FLAGS=${NP_C}" -Wno-deprecated
               -DENABLE_PROGRAMS=OFF -DENABLE_TESTING=OFF -DMBEDTLS_FATAL_WARNINGS=OFF
               -DUSE_SHARED_MBEDTLS_LIBRARY=OFF -DUSE_STATIC_MBEDTLS_LIBRARY=ON
               -DGEN_FILES=OFF
               -DTF_PSA_CRYPTO_USER_CONFIG_FILE=${NP_PSA_CFG}
    ${NP_EP_STEPS}
    BUILD_BYPRODUCTS ${NPP}/lib/libmbedtls.a ${NPP}/lib/libmbedx509.a ${NPP}/lib/libtfpsacrypto.a)
ExternalProject_Add(rayclay_np_curl PREFIX ${NP}/ep-curl-8.23.0
    URL ${RC_NETPROFILE_TARBALLS}/curl-8.23.0.tar.xz
    URL_HASH SHA256=8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5
    PATCH_COMMAND ${CMAKE_COMMAND} -DRC_NP_PATCH_CURL=<SOURCE_DIR> -P ${CMAKE_CURRENT_LIST_FILE}
    DEPENDS rayclay_np_zlib rayclay_np_mbedtls
    CMAKE_ARGS ${NP_ARGS} "-DCMAKE_C_FLAGS=${NP_C_CFG}"
        -DBUILD_STATIC_LIBS=ON -DBUILD_CURL_EXE=OFF -DBUILD_TESTING=OFF
        -DBUILD_LIBCURL_DOCS=OFF -DBUILD_MISC_DOCS=OFF -DENABLE_CURL_MANUAL=OFF -DCURL_USE_PKGCONFIG=OFF
        -DCURL_ENABLE_EXPORT_TARGET=ON -DCURL_USE_MBEDTLS=ON -DCURL_USE_OPENSSL=OFF -DENABLE_ARES=OFF
        -DENABLE_THREADED_RESOLVER=ON -DCURL_ZLIB=ON -DCURL_BROTLI=OFF -DCURL_ZSTD=OFF -DUSE_NGHTTP2=OFF
        -DUSE_NGTCP2=OFF -DUSE_QUICHE=OFF -DCURL_USE_LIBPSL=OFF -DUSE_LIBIDN2=OFF -DCURL_USE_LIBSSH2=OFF
        -DCURL_USE_LIBSSH=OFF -DCURL_USE_GSSAPI=OFF -DCURL_CA_BUNDLE=none -DCURL_CA_PATH=none
        -DCURL_DISABLE_DICT=ON -DCURL_DISABLE_FILE=ON -DCURL_DISABLE_FTP=ON -DCURL_DISABLE_GOPHER=ON
        -DCURL_DISABLE_IMAP=ON -DCURL_DISABLE_LDAP=ON -DCURL_DISABLE_LDAPS=ON -DCURL_DISABLE_MQTT=ON
        -DCURL_DISABLE_POP3=ON -DCURL_DISABLE_RTSP=ON -DCURL_DISABLE_SMTP=ON -DCURL_DISABLE_TELNET=ON
        -DCURL_DISABLE_TFTP=ON -DCURL_DISABLE_IPFS=ON -DCURL_DISABLE_COOKIES=ON -DCURL_DISABLE_ALTSVC=ON
        -DCURL_DISABLE_HSTS=ON -DCURL_DISABLE_AWS=ON -DCURL_DISABLE_DOH=ON -DCURL_DISABLE_MIME=ON
        -DCURL_DISABLE_FORM_API=ON -DCURL_DISABLE_GETOPTIONS=ON -DCURL_DISABLE_DIGEST_AUTH=ON
        -DCURL_DISABLE_KERBEROS_AUTH=ON -DCURL_DISABLE_NEGOTIATE_AUTH=ON -DCURL_DISABLE_SHA512_256=ON
        -DCURL_DISABLE_PROGRESS_METER=ON -DCURL_DISABLE_BINDLOCAL=ON -DCURL_DISABLE_VERBOSE_STRINGS=ON
        -DCURL_DISABLE_HEADERS_API=ON -DCURL_DISABLE_WEBSOCKETS=OFF -DCURL_DISABLE_PROXY=OFF
        -DCURL_DISABLE_NETRC=OFF
        -DZLIB_INCLUDE_DIR=${NPP}/include -DZLIB_LIBRARY=${NPP}/lib/libz.a
        -DMBEDTLS_INCLUDE_DIR=${NPP}/include -DMBEDTLS_LIBRARY=${NPP}/lib/libmbedtls.a
        -DMBEDX509_LIBRARY=${NPP}/lib/libmbedx509.a -DMBEDCRYPTO_LIBRARY=${NPP}/lib/libtfpsacrypto.a
    ${NP_EP_STEPS}
    BUILD_BYPRODUCTS ${NPP}/lib/libcurl.a)
ExternalProject_Add_StepDependencies(rayclay_np_mbedtls configure ${NP_PSA_CFG})
ExternalProject_Add_StepDependencies(rayclay_np_curl configure ${NP_PSA_CFG})
ExternalProject_Add_StepDependencies(rayclay_np_curl patch ${CMAKE_CURRENT_LIST_FILE})

add_library(rayclay_np_bridge OBJECT rc_net_profile.c)
target_include_directories(rayclay_np_bridge SYSTEM PRIVATE ${NPP}/include)
target_compile_options(rayclay_np_bridge PRIVATE ${NP_CFLAGS} -fno-lto
                       -ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.)
target_compile_definitions(rayclay_np_bridge PRIVATE "TF_PSA_CRYPTO_USER_CONFIG_FILE=\"${NP_PSA_CFG}\"")
set_target_properties(rayclay_np_bridge PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON
                      INTERPROCEDURAL_OPTIMIZATION OFF)
add_dependencies(rayclay_np_bridge rayclay_np_curl)

set(NP_LIBS ${NPP}/lib/libcurl.a ${NPP}/lib/libmbedtls.a ${NPP}/lib/libmbedx509.a ${NPP}/lib/libtfpsacrypto.a
    ${NPP}/lib/libz.a)
set(NP_KEEP "rci_net_profile_link_rayclay_netprofile_*")
add_custom_command(OUTPUT ${NP}/librayclay_netprofile.a
    COMMAND ${CMAKE_COMMAND} -E rm -f netprofile-merged.o netprofile-local.o librayclay_netprofile.a
    COMMAND ${CMAKE_LINKER} -r -d -o netprofile-merged.o $<TARGET_OBJECTS:rayclay_np_bridge>
            --start-group ${NP_LIBS} --end-group
    COMMAND ${CMAKE_OBJCOPY} --wildcard --keep-global-symbol=${NP_KEEP} netprofile-merged.o netprofile-local.o
    COMMAND ${CMAKE_OBJCOPY} --strip-unneeded --wildcard --keep-symbol=${NP_KEEP} netprofile-local.o
    COMMAND ${CMAKE_AR} rcsD librayclay_netprofile.a netprofile-local.o
    COMMAND ${CMAKE_COMMAND} -DRC_NP_CHECK_NM=${CMAKE_NM} -DRC_NP_CHECK_ARCHIVE=${NP}/librayclay_netprofile.a
            -DRC_NP_CHECK_MERGED=${NP}/netprofile-merged.o -DRC_NP_CHECK_ELF=${_rc_np_elf}
            -DRC_NP_CHECK_NPH=${CMAKE_CURRENT_SOURCE_DIR}/rc_net_profile.h -P ${CMAKE_CURRENT_LIST_FILE}
    DEPENDS rayclay_np_bridge $<TARGET_OBJECTS:rayclay_np_bridge> ${NP_LIBS}
            ${CMAKE_CURRENT_SOURCE_DIR}/rc_net_profile.h ${CMAKE_CURRENT_LIST_FILE}
    WORKING_DIRECTORY ${NP}
    COMMENT "rayclay_netprofile: link, localize and check the network profile"
    VERBATIM)
add_custom_target(rayclay_netprofile_build ALL DEPENDS ${NP}/librayclay_netprofile.a)
add_library(rayclay_netprofile INTERFACE)
target_link_libraries(rayclay_netprofile INTERFACE ${NP}/librayclay_netprofile.a Threads::Threads)
add_dependencies(rayclay_netprofile rayclay_netprofile_build)
