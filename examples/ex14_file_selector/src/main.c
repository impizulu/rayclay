/*
    main.c - RayClay File Selector: an open panel the app draws itself

    A component to copy: file_selector(), in src/app/file_selector.h, is the Open
    panel of a well-made desktop app - a sidebar of places, Back, Forward and a path
    bar, folders then files with their icons, kinds, sizes and dates, a click on a
    column to sort, a search and a file-type list, a preview of the picked picture,
    the keyboard, and a readable row for a folder that cannot be listed - drawn by
    RayClay from two file calls:

      rcListDirectory(path, callback, user)   the names in a folder, once
      rcPathInfo(path, &info)                 each one's size and date

    It is declared in the layout and returns true on the frame a file is picked; the
    app keeps its state. It is for the files an APP keeps, and on a phone and the web
    those live in rcAppDataPath, where no system dialog looks. For the user's own
    documents, use the system's dialog: ex15_file_explorer shows rcFileDialogOpen.

    The viewer in src/app/app.h opens it as a centered panel on a desktop and as the
    whole screen on a phone, chosen by the room there is, and shows what was picked.
    src/app/samples.h fills the app's folder on the first run.

    Build target: rayclay_ex14_file_selector
*/
#include "app/app.h"

int main(void) {
    static AppState state;   /* the selector's listing is 350 KB: static, not on the stack */
    /* Slot 0 is every bundled control's: rcButton and rcBeginMenu have no size of their own,
       so this is how they read at the size of the rows beside them. */
    static const float sizes[] = { (float)FS_PX_TEXT };

    /* Light first, as a system's open panel is; the toolbar's sun and moon switch it. */
    rcSetStyle(app_style(false));
    RC_AppOptions opts = {
        .width = 1280, .height = 820, .title = "RayClay File Selector",
        .clearColor = rcGetStyle().background,
        .fontSizes = sizes, .fontCount = 1,
        /* Backs every rcFormat in a frame: a visible row formats its id, its size,
           its date and its kind, and the first run formats a ledger. */
        .scratchArenaBytes = 128 * 1024,
        .updateCallback = update, .layoutCallback = layout, .userData = &state,
        .renderMode = RC_RENDER_ON_DEMAND,   /* parks between inputs */
    };
    return rcRunApp(&opts);
}
