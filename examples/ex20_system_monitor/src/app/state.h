/*
    state.h - everything the dashboard remembers, and the tables behind its pickers.
    One AppState, created once by main() and handed to every callback. The numbers
    arrive through device.h, host.h and fleet.h; the panels only read them.
*/
#ifndef APP_STATE_H
#define APP_STATE_H

#include "rayclay.h"
#include "app/theme.h"
#include "app/device.h"
#include "app/host.h"
#include "app/fleet.h"

/* The refresh picker: seconds between samples, where 0 is Grafana's "Off": the
   dashboard stops sampling and the window parks until the user acts. */
enum { REFRESH_COUNT = 5 };
static const char *const REFRESH_LABEL[REFRESH_COUNT] = { "Off", "1s", "5s", "10s", "30s" };
static const int         REFRESH_SEC[REFRESH_COUNT]   = { 0, 1, 5, 10, 30 };

/* The time-range picker. A range is a window onto history the app already holds, so
   the longest range at the fastest refresh is what sizes HISTORY. */
enum { RANGE_COUNT = 4 };
static const char *const RANGE_LABEL[RANGE_COUNT] = {
    "Last 1 minute", "Last 5 minutes", "Last 15 minutes", "Last 30 minutes"
};
static const int RANGE_SEC[RANGE_COUNT] = { 60, 300, 900, 1800 };

/* The time zones the axes can print: the user's own first, as a browser's time is the
   default, then fixed offsets from UTC. Local's offset is read at every sample
   (follow_wall_clock in app.h). */
enum { ZONE_LOCAL = 0, ZONE_COUNT = 7 };
static const char *const ZONE_LABEL[ZONE_COUNT] = {
    "Local", "UTC", "UTC-8", "UTC-5", "UTC+1", "UTC+5:30", "UTC+9"
};
static const int ZONE_MINUTES[ZONE_COUNT] = { 0, 0, -480, -300, 60, 330, 540 };

/* Template variables, the dropdown row. Host picks the simulated server every row below
   This device reads, Cores narrows the two CPU panels, State narrows the process table. */
enum { HOST_WEB = 0, HOST_DB, HOST_CACHE, HOST_COUNT, STATE_FILTERS = 4 };
static const char *const HOST_LABEL[HOST_COUNT] = { "web-01", "db-01", "cache-01" };
static const SysProfile HOST_PROFILE[HOST_COUNT] = {
    { "web-01", "www-data", { "nginx", "node-api", "envoy", "fluent-bit" }, 3,
      0.85f, 16.0f, 10.4f, 3200.0f, 5800.0f, 180.0f, 640.0f, 58.0f, 21 },
    { "db-01", "postgres", { "postgres", "pgbouncer", "wal-g", "pg_exporter" }, -1,
      1.15f, 64.0f, 47.0f, 1800.0f, 2400.0f, 8200.0f, 6400.0f, 83.0f, 63 },
    { "cache-01", "redis", { "redis-server", "redis-sentinel", "memcached", "twemproxy" }, -1,
      0.45f, 32.0f, 19.0f, 5200.0f, 5100.0f, 40.0f, 220.0f, 34.0f, 9 },
};
static const char *const CORE_LABEL[SYS_CORES + 1] = {
    "All", "cpu0", "cpu1", "cpu2", "cpu3", "cpu4", "cpu5", "cpu6", "cpu7"
};
static const char *const STATE_LABEL[STATE_FILTERS] = { "All", "running", "sleeping", "stopped" };

/* The pages the tabs switch. Each scrolls in a container of its own, so the page
   you come back to is where you left it. */
typedef enum Page { PAGE_RESOURCES = 0, PAGE_SERVICES, PAGE_COUNT } Page;

/* Grafana rows: a title you click to fold the panels under it away. */
typedef enum DashRow {
    ROW_DEVICE = 0, ROW_OVERVIEW, ROW_CPU, ROW_MEMNET, ROW_PROCS, ROW_LIVE, ROW_FLEET, ROW_COUNT
} DashRow;

/* The time series. Each keeps its own legend isolation; the crosshair spans them. */
typedef enum ChartId {
    CHART_CPU = 0, CHART_MEM, CHART_NET, CHART_DISK, CHART_LAT, CHART_COUNT
} ChartId;

/* Sortable columns of the process table. */
typedef enum SortKey {
    SORT_NAME = 0, SORT_PID, SORT_USER, SORT_CPU, SORT_MEM, SORT_STATE, SORT_COUNT
} SortKey;

/* Panels that can leave for a native window of their own. */
typedef enum Panel { PANEL_CHART = 0, PANEL_CORES, PANEL_TABLE, PANEL_COUNT } Panel;

/* What draw_gauge needs. It runs when the frame is drawn, after layout returned, so
   this lives in AppState rather than on a stack. */
typedef struct GaugeDraw {
    float fraction;
    float warn, crit;
    RC_Color track;
} GaugeDraw;

enum { GAUGE_CPU = 0, GAUGE_MEM, GAUGE_DISK, GAUGE_FLEET, GAUGE_COUNT };

typedef struct AppState AppState;

/* Where a panel's window was when it last drew, replayed into the next open. */
typedef struct PanelGeometry {
    bool valid;
    int  x, y, w, h;
} PanelGeometry;

/* One slot per detachable panel. The window pointer is the state machine: NULL means
   docked. The slot is also the window's userData, which the library borrows for the
   record's whole life, so it lives here. */
typedef struct PanelSlot {
    AppState     *st;
    Panel         panel;
    RC_Window    *window;
    bool          pinRequested;  /* what we asked; pinned is what the OS reports     */
    /* On X11 a topmost query is a server round trip, so support is asked once and
       the pin read back at most once a second. */
    unsigned char pinAsked;      /* rcWindowTopmostSupported: 0 not yet, 1 no, 2 yes */
    bool          pinned;
    double        pinReadUntil;
    double        pinReadAt;
    unsigned char skips;         /* half-rate phase for a background panel           */
    PanelGeometry geom;
    SortKey       titledBy;      /* what the native title states; SORT_COUNT = none  */
    bool          titledDesc;
} PanelSlot;

/* One host: its model and its history, oldest first, newest last. Plain arrays rather
   than ring buffers because rcChart and rcSparkline take a contiguous span. `order`
   is an index permutation: the model is never reordered, so pids stay put. */
typedef struct HostView {
    SysHost host;
    int     order[SYS_PROCS];
    float   core[SYS_CORES][HISTORY];
    float   cpu[HISTORY];        /* mean of the cores, percent          */
    float   userShare[HISTORY];  /* how the load splits at each sample  */
    float   sysShare[HISTORY];
    float   memUsed[HISTORY];    /* GiB                                 */
    float   memCache[HISTORY];
    float   swap[HISTORY];
    float   rx[HISTORY];         /* KB/s                                */
    float   tx[HISTORY];
    float   rd[HISTORY];
    float   wr[HISTORY];
    float   load1[HISTORY];
} HostView;

struct AppState {
    HostView view[HOST_COUNT];
    /* Each slot's time as minutes since a UTC midnight: the last one before the oldest
       slot, which roll_day moves on a day at a time so the floats stay small. A chart's
       ticks land on multiples of its step, so this unit puts them on round clock
       minutes. Shared by every host because one sample reads them all. */
    float    at[HISTORY];
    double   clockBase;          /* add rcAppTime for seconds since that midnight */
    double   dayUnix;            /* that midnight, as rcUnixTimeSeconds counts    */

    Device   device;             /* the device this app runs on, real */
    int      samples;
    double   lastSampleAt;
    bool     refreshNow;
    bool     primed;

    /* The controls, each an index into its table above. */
    Page     tab;
    int      host, cores, stateFilter, range, refresh, zone;
    int      localMinutes;       /* the user's offset from UTC        */
    RC_Image aurora, heat;       /* what art.h paints                 */
    bool     auroraPainted;
    int      heatSamples, heatHost, heatFirst, heatLast; /* what the heatmap shows */
    bool     starred;
    bool     rowFolded[ROW_COUNT];
    float    pageY[PAGE_COUNT];  /* each page's scroll offset        */
    int      isolate[CHART_COUNT]; /* legend click: 0 = all, n = entry n-1 only */

    /* The crosshair, shared across the charts: the chart under the pointer, or -1,
       and the time it is pointing at. */
    int      crossChart;
    float    crossAt;
    /* Drag across a chart to zoom every panel to that span, as Grafana does. */
    int      brushChart;         /* the chart being dragged on, or -1 */
    float    brushFrom, brushTo;
    bool     zoomed;
    float    zoomFrom, zoomTo;
    bool     tickSeconds;        /* the x axis being drawn steps under a minute */

    GaugeDraw gauge[GAUGE_COUNT];

    /* The process table. */
    int      page;
    SortKey  sortKey;
    bool     sortDesc;
    char     procQuery[32];

    /* The command palette. */
    bool     paletteOpen;
    char     paletteQuery[48];
    int      paletteSel;

    /* The Services page: the fleet, its clock and its filters, 0 meaning All. */
    Fleet    fleet;
    double   fleetAt;
    double   bandAt;             /* when the band last moved: its axis stops here */
    int      svcRegion, svcTier, svcGroup, svcHealth;
    bool     jumpToCards;
    const char *regionItems[FLEET_REGIONS + 1];
    const char *groupItems[FLEET_GROUPS + 1];

    /* The collapsible titlebar. */
    bool     barOpen;
    float    barH;
    bool     animating;

    /* rcChildWindowsSupported() answers whether this target can open a second window
       at all; `singleSurface` remembers a desktop that refused one anyway. */
    RC_App  *app;
    PanelSlot slot[PANEL_COUNT];
    bool     singleSurface;
    RC_WindowError lastOpenError;
};

/* The width being laid out: a window dragged narrow takes the same branch as a phone,
   and under layout zoom this is the window divided by the zoom. */
static inline float content_width(void) {
    RC_Viewport vp = rcViewport();

    return vp.width - vp.safe.left - vp.safe.right;
}

/* A control's height class: @p fine under a mouse. NULL keeps the control's own height,
   which under a finger is the 44 px a thumb needs. */
static inline const char *control_h(const char *fine) {
    return rcPointerIsCoarse() ? NULL : fine;
}

/* The local-time minutes a slot's axis value prints as, wrapped into the day. */
static inline int clock_seconds(double minutes, int zoneMinutes) {
    int secs = (int)(minutes * 60.0 + 0.5) + zoneMinutes * 60;

    secs %= 86400;
    return secs < 0 ? secs + 86400 : secs;
}

/* The picked zone's offset from UTC, in minutes. */
static inline int zone_minutes(const AppState *st) {
    return st->zone == ZONE_LOCAL ? st->localMinutes : ZONE_MINUTES[st->zone];
}

/* True if needle occurs in hay, ignoring ASCII case: the table's filter and the
   palette's. An empty needle matches everything. */
static inline bool text_has(const char *hay, const char *needle) {
    for (; ; hay++) {
        const char *h = hay, *n = needle;

        while (*n) {
            char a = (*h >= 'A' && *h <= 'Z') ? (char)(*h + 32) : *h;
            char b = (*n >= 'A' && *n <= 'Z') ? (char)(*n + 32) : *n;

            if (a != b) break;
            h++;
            n++;
        }
        if (!*n) return true;
        if (!*hay) return false;
    }
}

#endif /* APP_STATE_H */
