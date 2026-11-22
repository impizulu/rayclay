/*
    main.c - RayClay System Monitor: a live monitoring dashboard with Grafana's data colors
    on a night sky of its own. Stat, gauge, status, bar gauge, heatmap, time series and table
    panels sit as glass over a painted backdrop, under a titlebar you fold with the primary
    modifier + B. Its tabs hold two pages: Resources, this device's readings over one
    server's panels, and Services, 48 service cards under a live traffic band. One source,
    desktop, web and phone.

    Shows a custom titlebar (RC_WINDOW_FRAME_NONE, rcWindowControls, .noDrag), an app-defined
    element over rcComponent, a gauge drawn with rcIconEmit, pictures painted with
    rcImageUpdate (app/art.h), on-demand pacing with rcWindowRequestFrameAfter, a crosshair
    shared across charts, and detachable panels via rcAppOpenWindow. The Services page is
    the lesson in what a frame costs: "THE STATIC ISLAND" in app/services.h says how a page
    that moves on every frame can hold 48 cards that cost next to nothing.

    The This device row is real: app/device.h takes the four readings RayClay reports, and
    a reading the platform does not give stays empty. Every other row is a simulated server
    or fleet from app/host.h and app/fleet.h, the seams you replace, and its header says
    simulated.

    Build target: rayclay_ex20_system_monitor
*/
#include "app/app.h"

int main(void) {
    static AppState state;
    /* Slot 0 sizes the text of the library's own controls: combos, fields, buttons
       and tooltips. Every other run states its size where it is drawn. */
    static float controlText = 13.0f;

    rcSetStyle(app_style());
    app_init(&state);

    RC_AppOptions opts = {
        .width  = 1280,
        .height = 820,
        .title  = "RayClay System Monitor",
        .clearColor = rcGetStyle().background,
        .fontSizes = &controlText,
        .fontCount = 1,
        /* Backs every rcFormat in a frame. */
        .scratchArenaBytes = 32768,
        /* Sized so no frame has to grow the layout arena and be re-run. */
        .startLayoutElements = 4096,
        .frame          = RC_WINDOW_FRAME_NONE,
        .updateCallback  = update,
        .layoutCallback  = layout,
        .userData  = &state,
        .titlebar       = { .height = BAR_OPEN_H }, /* the band in app.h is the titlebar */
        /* The default, spelled out because it is the point of this app: the window
           parks between samples and update() asks for the next wake. */
        .renderMode = RC_RENDER_ON_DEMAND,
    };

    return rcRunApp(&opts);
}
