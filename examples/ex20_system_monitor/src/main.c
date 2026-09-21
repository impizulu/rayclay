/* ex20 - System Monitor
 *
 * A live resource monitor: a branded titlebar you fold with the primary
 * modifier + T, four stat cards, a rolling load chart, a per-core strip and a
 * sortable 128-row process table. One source, desktop and web, no assets.
 *
 * Shows a custom titlebar (.titlebar.custom, rcWindowControlButton,
 * RC_ID_WINDOW_DRAG / RC_ID_WINDOW_NODRAG), an app-defined element macro over
 * rcBeginComponent / rcEndComponent, on-demand pacing with
 * rcWindowRequestFrameAfter, and detachable panels via rcAppOpenWindow.
 * This process's CPU and memory are real; the rest comes from app/host.h.
 *
 * Build: cmake --build build --target rayclay_ex20_system_monitor
 */
#include "app/app.h"

int main(void)
{
    static AppState state;
    float fontSizes[F_COUNT];
    int i;

    fontSizes[F_MICRO] = (float)SZ_MICRO;
    fontSizes[F_SMALL] = (float)SZ_SMALL;
    fontSizes[F_BODY]  = (float)SZ_BODY;
    fontSizes[F_STAT]  = (float)SZ_STAT;

    rcSetStyle(sys_style());

    sysmon_host_init(&state.host, 0x53595300u);
    for (i = 0; i < SYS_PROCS; i++)
        state.order[i] = i;
    state.sortKey  = SORT_CPU;
    state.sortDesc = true;
    state.barOpen  = true;
    state.barH     = (float)BAR_OPEN_H;
    /* Each slot is a floating window's userData for the record's whole life,
       so it is set up once, here, and never on a stack. */
    for (i = 0; i < PANEL_COUNT; i++) {
        state.slot[i].st    = &state;
        state.slot[i].panel = (Panel)i;
    }

    RC_AppOptions opts = {
        .width  = 1240,
        .height = 820,
        .title  = "RayClay System Monitor",
        .clearColor = rcGetStyle().background,
        .fontSizes = fontSizes,
        .fontCount = F_COUNT,
        /* Backs every rcFormat in a frame. A scrolling page formats the cells
           of all 128 rows, which is the frame that sizes this. */
        .scratchArenaBytes = 16384,
        /* A scrolling page declares every row, well past the 2,048 the layout
           arena starts at. Sized so no frame has to grow it and be re-run. */
        .startLayoutElements = 4096,
        .nativeFrame    = true,
        .titlebarHeight = BAR_OPEN_H,
        .updateCallback  = update,
        .layoutCallback  = layout,
        .userData  = &state,
        .titlebar       = { .custom = true },  /* the band above IS the titlebar */
        /* The default, spelled out because it is the point of this app: the
           window parks between samples and update() asks for the next wake. */
        .renderMode = RC_RENDER_ON_DEMAND,
    };

    return rcRunApp(&opts);
}
