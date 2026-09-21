/* ============================================================================
 *  ex22 - RayClay Reader: an offline documentation reader.
 *
 *  Demonstrates a custom font family registered as a ladder (rcRegisterFont +
 *  rcFont, one file per weight), rcMeasureText driving both the reading column
 *  and the toolbar's own breakpoint, a two-arm responsive layout, a custom
 *  window icon, and rcSetLogSink for font diagnostics.
 *
 *  Run it from the repository root: the fonts and the icon are read from
 *  examples/assets/. Launched elsewhere the registrations fail, RayClay keeps
 *  drawing in the bundled face, and the status bar says so.
 * ========================================================================= */
#include "app/app.h"

int main(void)
{
    RC_AppOptions opts = {0};
    int rc;

    opts.width    = 1180;
    opts.height   = 820;
    opts.title    = "RayClay Reader";
    opts.iconPath = RC_EX22_ICON;
    opts.layoutCallback = layout;
    opts.userData = &state;
    opts.frameEndCallback = frame_end;

    /* Backs rcFormat, which the status bar uses for the measure readout. This
       is the one option where 0 means "off" rather than "a sensible default":
       leave it unset and rcFormat returns the visible "<set scratchArenaBytes>"
       placeholder instead of failing. */
    opts.scratchArenaBytes = 4096;

    /* Installed BEFORE the app starts: the font registrations happen on the
       first frame and their diagnostics are the only failure signal there is. */
    rcSetLogSink(reader_log, NULL);

    rc = rcRunApp(&opts);

    /* THE RENDER WITNESS IS THE EXIT CODE. This app installs a log sink, so the
       runner's "rendered N frames" line never reaches stdout, and the example
       is pure RC_ with no way to print one of its own.
       "> 0" rather than "== maxFrames": frameEndCallback counts PRESENTED
       frames, and the window system's first map presents one the main loop
       never counted. */
    if (rc == 0 && state.framesDrawn == 0)
        rc = 3;

    return rc;
}
