# The files each example opens by a relative path at run time (RC_MOBILE_ASSETS).
# CMakeLists.txt bundles them into the example's iOS app at that same path.

set_property(TARGET rayclay_ex06_widgets_gallery PROPERTY RC_MOBILE_ASSETS
    examples/assets/logos/rayclay-logo-1024.png
    examples/assets/fonts/AdwaitaMono-Regular.ttf examples/assets/fonts/AdwaitaMono-OFL.txt)
set_property(TARGET rayclay_ex15_file_explorer PROPERTY RC_MOBILE_ASSETS
    examples/assets/fonts/AdwaitaMono-Regular.ttf examples/assets/fonts/AdwaitaMono-OFL.txt)
set_property(TARGET rayclay_ex29_dash_game PROPERTY RC_MOBILE_ASSETS
    examples/assets/fonts/RussoOne-Regular.ttf examples/assets/fonts/RussoOne-OFL.txt)
set_property(TARGET rayclay_ex01_1980s_gui PROPERTY RC_MOBILE_ASSETS
    examples/assets/fonts/Jersey10-Grid.ttf examples/assets/fonts/Jersey10-OFL.txt)
set_property(TARGET rayclay_ex11_util_rc_inspector PROPERTY RC_MOBILE_ASSETS
    examples/assets/fonts/AdwaitaMono-Regular.ttf examples/assets/fonts/AdwaitaMono-OFL.txt)
set_property(TARGET rayclay_ex22_markdown_notes PROPERTY RC_MOBILE_ASSETS
    examples/assets/logos/rayclay-logo-1024.png
    examples/assets/fonts/Lato-Regular.ttf examples/assets/fonts/Lato-Bold.ttf
    examples/assets/fonts/Lato-Italic.ttf examples/assets/fonts/AdwaitaMono-Regular.ttf
    examples/assets/fonts/Lato-OFL.txt examples/assets/fonts/AdwaitaMono-OFL.txt)
set_property(TARGET rayclay_ex28_ide PROPERTY RC_MOBILE_ASSETS
    examples/assets/icons/rayclay-logo.svg
    examples/assets/fonts/AdwaitaMono-Regular.ttf examples/assets/fonts/AdwaitaMono-OFL.txt)
set_property(TARGET rayclay_ex21_media_library PROPERTY RC_MOBILE_ASSETS
    examples/assets/media/harbor.mp4 examples/assets/media/harbor.webm)
set_property(TARGET rayclay_ex12_signin_form PROPERTY RC_MOBILE_ASSETS
    examples/assets/logos/google-g.svg examples/assets/logos/BRAND-MARKS.md)
set(_rc_photo_assets examples/assets/photos/LICENSE.md)
foreach(_photo alps aurora books bread coast ferry forest road tea tram type whale)
    list(APPEND _rc_photo_assets "examples/assets/photos/${_photo}.jpg")
endforeach()
foreach(_face alexrivera amaracooks diegoclimbs elisepedals hanasato inesc jonasl kofiskies
              lenanovak mayareads owenh priyaraman samw theomakes zofiareads)
    list(APPEND _rc_photo_assets "examples/assets/photos/faces/${_face}.png")
endforeach()
set_property(TARGET rayclay_ex16_tab_navigator PROPERTY RC_MOBILE_ASSETS "${_rc_photo_assets}")
