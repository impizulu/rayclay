function(_rc_ios_narrow_plist target orient template out_var)
    set(_orient_all [=[
    <key>UISupportedInterfaceOrientations</key>
    <array>
        <string>UIInterfaceOrientationPortrait</string>
        <string>UIInterfaceOrientationLandscapeLeft</string>
        <string>UIInterfaceOrientationLandscapeRight</string>
    </array>
]=])
    set(_orient_portrait [=[
    <key>UISupportedInterfaceOrientations</key>
    <array>
        <string>UIInterfaceOrientationPortrait</string>
    </array>
]=])
    set(_orient_landscape [=[
    <key>UISupportedInterfaceOrientations</key>
    <array>
        <string>UIInterfaceOrientationLandscapeLeft</string>
        <string>UIInterfaceOrientationLandscapeRight</string>
    </array>
]=])
    file(READ "${template}" _text)
    string(FIND "${_text}" "${_orient_all}" _at)
    string(FIND "${_text}" "${_orient_all}" _last REVERSE)
    if(_at LESS 0 OR NOT _at EQUAL _last)
        message(FATAL_ERROR "rc_ios_app(${target}): ${template} does not list the three "
                            "orientations once, in the shape ORIENTATION ${orient} narrows")
    endif()
    string(REPLACE "${_orient_all}" "${_orient_${orient}}" _text "${_text}")
    set(_out "${CMAKE_CURRENT_BINARY_DIR}/rc_ios_app/${target}/Info.plist.in")
    set(_old "")
    if(EXISTS "${_out}")
        file(READ "${_out}" _old)
    endif()
    if(NOT _old STREQUAL _text)
        file(WRITE "${_out}" "${_text}")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${template}")
    set(${out_var} "${_out}" PARENT_SCOPE)
endfunction()

function(rc_ios_app target)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        return()
    endif()
    cmake_parse_arguments(PARSE_ARGV 1 _rc_ios "" "ICONS;ORIENTATION" "")
    if(_rc_ios_UNPARSED_ARGUMENTS OR _rc_ios_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "rc_ios_app(${target}): expected rc_ios_app(<target> [ICONS <dir>] "
                            "[ORIENTATION portrait|landscape|any])")
    endif()
    set(_rc_ios_orient "${_rc_ios_ORIENTATION}")
    if(_rc_ios_orient STREQUAL "")
        set(_rc_ios_orient any)
    endif()
    if(NOT _rc_ios_orient MATCHES "^(any|portrait|landscape)$")
        message(FATAL_ERROR "rc_ios_app(${target}): ORIENTATION takes portrait, landscape or any, "
                            "not '${_rc_ios_orient}'")
    endif()
    if(NOT DEFINED _rc_ios_ICONS)
        set(_rc_ios_ICONS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/appicon")
    endif()
    get_filename_component(_rc_ios_ICONS "${_rc_ios_ICONS}" ABSOLUTE)
    file(GLOB _rc_ios_icons "${_rc_ios_ICONS}/*.png")
    if(NOT _rc_ios_icons)
        message(FATAL_ERROR "rc_ios_app(${target}): no icon PNG in ${_rc_ios_ICONS}")
    endif()
    get_target_property(_rc_ios_plist ${target} MACOSX_BUNDLE_INFO_PLIST)
    if(_rc_ios_plist AND NOT _rc_ios_orient STREQUAL "any")
        message(FATAL_ERROR "rc_ios_app(${target}): ORIENTATION ${_rc_ios_orient} narrows the "
                            "template's plist, but the target names its own (${_rc_ios_plist}): list "
                            "the orientations in its UISupportedInterfaceOrientations instead")
    endif()
    if(NOT _rc_ios_plist)
        set(_rc_ios_plist "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Info.plist.in")
        if(NOT _rc_ios_orient STREQUAL "any")
            _rc_ios_narrow_plist(${target} ${_rc_ios_orient} "${_rc_ios_plist}" _rc_ios_plist)
        endif()
        set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${_rc_ios_plist}")
    endif()
    set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_COMBINE_HIDPI_IMAGES "NO")
    target_sources(${target} PRIVATE ${_rc_ios_icons})
    set_source_files_properties(${_rc_ios_icons} TARGET_DIRECTORY ${target}
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endfunction()
