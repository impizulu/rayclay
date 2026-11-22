/*
    main.c - RayClay Files: a Finder for one source tree

    A file browser in the shape of the Finder: icon, list and column views over a real
    folder tree, a sidebar, a path bar and Quick Look. It teaches the calls behind a real
    file manager, with no thread of your own:

      rcListDirectory, rcPathInfo        list a folder, as the user opens it
      rcMakeDirectory, rcRemovePath      change one, only when the user says so
      rcFileDialogOpen, rcFileDialogPoll ask for a folder, from a click
      rcDroppedFile, rcIsDragOver        take a folder dropped on the window
      rcLoadImage, rcLoadImageFromMemory show a picture the moment it is chosen
      rcVirtualList                      draw four thousand rows like forty
      rcBeginContextMenu, rcBeginModal   the right-click menu and Quick Look

    It opens on a sample drive that lives in memory (src/app/sample_tree.h), so the first
    run reads nothing of the user's and looks the same everywhere. "This Computer" and
    "Open Folder..." in the sidebar are the real thing, read-only until the user acts.

    The model and its commands are in src/app/tree.h and browser.h; views.h, preview.h and
    chrome.h draw them, and app.h is the frame. One source for the desktop, the web and the
    phones, with no #if in it.
    Build target: rayclay_ex15_file_explorer
*/
#include "app/app.h"

int main(void) {
    static AppState state;   /* a pool of 4,096 nodes: static, not on the stack */
    static const float body[1] = { (float)TEXT_BODY };

    ui = app_palette();
    rcSetStyle(app_style());
    app_classes();
    RC_AppOptions opts = {
        .width = 1280, .height = 820, .title = "Files",
        .clearColor = rcGetStyle().background,
        /* Slot 0 is what menus, fields and tooltips read: the Finder's body size. */
        .fontSizes = body, .fontCount = 1,
        /* Backs every rcFormat in a frame: a visible row formats its id and its columns. */
        .scratchArenaBytes = 256 * 1024,
        .frame = RC_WINDOW_FRAME_NONE,
        .layoutCallback = layout, .userData = &state,
        .titlebar = { .height = BAR_H },
        .renderMode = RC_RENDER_ON_DEMAND,  /* parks between inputs and answers */
    };
    return rcRunApp(&opts);
}
