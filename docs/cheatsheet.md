# RayClay cheatsheet

> **v0.9.20 quick reference card**: every public symbol, one line each.
> Long-form material is **[`api-notes.md`](api-notes.md)**; the CSS and Tailwind inventories are
> **[`for-web-developers.md`](for-web-developers.md)** and **[`css-tailwind-map.md`](css-tailwind-map.md)**.

A pure-C99 immediate-mode GUI library: one source builds a desktop window, a browser (WASM), an
Android app or an iOS app. `#include "rayclay.h"`, link `librayclay`. Three layers: **L1** core
renderer, **L2** UI helpers and element DSL, **L3** app-loop runner.

```c
#include "rayclay.h"

int main(void) { return rcRunApp(NULL); } // the whole app, in two lines
```

## module: core, L1 renderer (always available)

```c
// Window + frame
RC_Dimensions   rcGetWindowDimensions(void);        // Window size in logical px
float           rcGetContentScale(void);            // Framebuffer/logical pixel ratio
RC_String       rcStringFromCStr(const char *cstr); // Wrap a C string as an RC_String, no copy

// L1 window primitives; an rcRunApp application must never call rcInitWindow
void            rcInitWindow(int width, int height, const char *title); // Open a window and GL context
void            rcCloseWindow(void);                                    // Tear down the window rcInitWindow opened

// L1 renderer path; under rcRunApp or rcRunFrame the runner calls both
void            rcSyncLayoutDimensions(void);                           // Push the layout size into Clay, before each layout
void            rcRender(RC_RenderCommandArray commands);               // Turn one frame of render commands into draw calls

// Diagnostics
void            rcSetLogSink(RC_LogCallback sink, void *user);          // Route warnings and errors to a callback
size_t          rcProcessMemoryBytes(void);                             // This process's resident memory
size_t          rcProcessPeakMemoryBytes(void);                         // Peak resident memory over the process's life
float           rcProcessCpuPercent(void);                              // CPU since the previous call, as % of one core
bool            rcMachineMemoryBytes(size_t *total, size_t *available); // Whole-machine RAM, total and available
float           rcMachineCpuPercent(void);                              // Whole-machine busy percent since the previous call
double          rcUnixTimeSeconds(void);                                // Wall-clock UTC seconds since the epoch

// Fonts, images and SVG
uint16_t        rcLoadFont(const char *path, float size);                                               // Bake a TTF/OTF and return its fontId
bool            rcUnloadFont(uint16_t font);                                                            // Release a fontId slot for reuse
uint16_t        rcRegisterFont(const char *family, RC_FontWeight weight, const char *path, float size); // Bake and index under (family, weight, size)
uint16_t        rcFont(const char *family, RC_FontWeight weight, float size);                           // Resolve (family, weight, size) to a fontId
RC_Dimensions   rcMeasureText(RC_StringSlice text, RC_TextElementConfig *config, void *userData);       // UTF-8 text-measure callback for the layout engine

RC_Image        rcLoadImage(const char *path);                              // Load PNG/JPG/BMP into a GPU texture
RC_Image        rcLoadImageFromMemory(const unsigned char *bytes, int len); // Same from encoded bytes in memory, not raw pixels
void            rcUnloadImage(RC_Image *img);                               // Free the texture and zero the handle
void            rcSvg(const char *path, float size, RC_Color color);        // Draw an .svg file by path
RC_Svg         *rcLoadSvg(const char *path);                                // Parse an .svg into a handle you own
RC_Svg         *rcLoadSvgFromMemory(const char *bytes, int len);            // Same from SVG text in memory
void            rcSvgHandle(const RC_Svg *svg, float size, RC_Color color); // Draw a loaded SVG handle
void            rcUnloadSvg(RC_Svg **svg);                                  // Free a parsed SVG and NULL your pointer

// Per-frame arena + formatting
RC_Arena        rcArenaInit(size_t size);                           // Create a bump allocator for per-frame allocations
void           *rcArenaAlloc(RC_Arena *arena, size_t size);         // Allocate size bytes from the arena
void            rcArenaReset(RC_Arena *arena);                      // Reclaim every allocation at once, once per frame
void            rcArenaFree(RC_Arena *arena);                       // Release the arena's backing buffer
RC_String       rcFormat(RC_Arena *arena, const char *format, ...); // printf into arena memory
```

## module: app: L3 runner (optional; `RC_NO_APP_RUNNER` to drop)

```c
// Entry point
int        rcRunApp(const RC_AppOptions *options); // Run the whole app loop

// Split-loop host only - rcRunApp is the supported shape and needs none of these
RC_App    *rcAppCreate(const RC_AppOptions *options); // Open window and backends
bool       rcRunFrame(RC_App *app);                   // Pump and draw one frame per eligible window
void       rcAppDestroy(RC_App *app);                 // Normal teardown, then free the RC_App

// Accessors
RC_Arena      *rcAppArena(RC_App *app);                // Per-frame scratch arena that rcFormat allocates from
void           rcAppRequestClose(RC_App *app);         // Exit the runner after the current frame
bool           rcAppIsDebugEnabled(const RC_App *app); // Whether the layout inspector is on
float          rcAppFPS(const RC_App *app);            // Reciprocal of rcWindowFrameTime. On-demand it is the WAKE rate: low is not slow
double         rcAppTime(const RC_App *app);           // Monotonic seconds since start-up
const float   *rcAppZoomLadder(const RC_App *app, uint16_t *count); // Borrowed table of stops the zoom keys walk

// NOTE: a property of a surface is an rcWindow* call, under module: windows
```

## module: chrome, titlebar, rem units and always-on-top (always available)

```c
// Kept under RC_NO_APP_RUNNER: an L1 host still draws the band and tags the ids
void       rcTitlebar(const RC_TitlebarOptions *options); // Draw the bundled titlebar here
rcWindowControlButton(control, icon, iconSize, ...);      // One control as a Flat Slab; append .width/.height to size the slab
rcWindowControls(...);                                    // Standard minimize/maximize/close cluster; append .iconSize/.width/.height
bool       rcIsWindowMaximized(void);                     // True while the window is maximised

void       rcSetRootFontSize(float px); // Set the root font size for rem units (default 16)
float      rcRootFontSize(void);        // The current root font size in px
bool       rcSetWindowTopmost(bool on); // Pin above other apps; false where the desktop cannot pin
bool       rcIsWindowTopmost(void);     // Reads the window system, so a refusal reads false
bool       rcTopmostSupported(void);    // Can this desktop pin a window at all
```

## module: windows, extra native windows on the desktop (L3)

```c
// Opening, lifecycle, frame scheduling and scratch
bool            rcChildWindowsSupported(void);                                 // Can this platform open a child window at all
RC_Window      *rcAppOpenWindow(RC_App *app, const RC_WindowOptions *options); // Open an additional native window
RC_Window      *rcAppMainWindow(RC_App *app);                                  // The app's primary window
RC_Window      *rcAppCurrentWindow(RC_App *app);                               // Whose callback is running, NULL outside one
RC_App         *rcWindowApp(const RC_Window *window);                          // The app that owns this window
RC_WindowState  rcWindowState(const RC_Window *window);                        // RC_WINDOW_PENDING, READY, FAILED, CLOSING, CLOSED
RC_WindowError  rcWindowError(const RC_Window *window);                        // Why window creation failed
void            rcWindowRequestClose(RC_Window *window);                       // Idempotent request to close this window
bool            rcWindowRelease(RC_Window *window);                            // Release a terminal child window record

void      rcWindowRequestFrame(RC_Window *window);                         // Admit one frame at the next pump
void      rcWindowRequestFrameAfter(RC_Window *window, double seconds);    // Admit a frame after this many seconds
void      rcWindowSetContinuousRendering(RC_Window *window, bool enabled); // Per-window override of the render mode
RC_Arena *rcWindowArena(RC_Window *window);                                // This window's scratch arena, reset each frame

// Surface, colour, topmost, zoom and titlebar
RC_Dimensions  rcWindowDimensions(const RC_Window *window);              // This window's logical size in px
bool           rcWindowSetSize(RC_Window *window, int width, int height); // Request a new logical size for this window
float          rcWindowContentScale(const RC_Window *window);            // Device pixel ratio, excluding app zoom
bool           rcWindowSetPosition(RC_Window *window, int x, int y);     // Request a screen position; false on Wayland
bool           rcWindowPosition(const RC_Window *window, int *x, int *y); // Read it back; outputs untouched on refusal
bool           rcWindowPositionSupported(const RC_Window *window);       // Can this session place a window at all
bool           rcWindowOwnerSupported(const RC_Window *window);          // Can this session give a window an owner
bool           rcWindowMinimize(RC_Window *window);                      // Ask the window manager to minimise
bool           rcWindowMaximize(RC_Window *window);                      // Ask the window manager to maximise
bool           rcWindowRestore(RC_Window *window);                       // Undo either; un-minimising also takes focus
bool           rcWindowIsMinimized(const RC_Window *window);             // Native reality, not the last request
bool           rcWindowIsMaximized(const RC_Window *window);             // Native reality, not the last request
bool           rcWindowIsFocused(const RC_Window *window);               // Does the host give this window focus
bool           rcWindowRaise(RC_Window *window);                         // Front + keyboard; rcWindowRestore first if minimised
bool           rcWindowSetTitle(RC_Window *window, const char *title);   // Native title: what alt-tab and the taskbar show
RC_Color       rcWindowClearColor(const RC_Window *window);              // Resolved background behind this window's layout
void           rcWindowSetClearColor(RC_Window *window, RC_Color color); // Alpha 0 selects the style background
bool           rcWindowSetTopmost(RC_Window *window, bool on);           // Pin this window; false where the desktop cannot pin
bool           rcWindowIsTopmost(const RC_Window *window);               // What the window system actually reports
bool           rcWindowTopmostSupported(const RC_Window *window);        // Per-window form of rcTopmostSupported

float        rcWindowZoom(const RC_Window *window);                    // This window's current zoom factor
void         rcWindowSetZoom(RC_Window *window, float zoom);           // Clamped to this window's own zoom ladder
RC_ZoomMode  rcWindowZoomMode(const RC_Window *window);                // RC_ZOOM_LAYOUT or RC_ZOOM_OPTICAL for this surface
void         rcWindowSetZoomMode(RC_Window *window, RC_ZoomMode mode); // Switch reflow versus magnify per window
int          rcWindowTitlebarHeight(const RC_Window *window);          // Bundled titlebar caption height, in PHYSICAL px
void         rcWindowSetTitlebarHeight(RC_Window *window, int height); // Set this window's caption height in physical px

// Per-window counters
float           rcWindowFrameTime(const RC_Window *window);   // Smoothed seconds between this window's frames
RC_FrameCounts  rcWindowFrameCounts(const RC_Window *window); // Declared versus drawn, per window
RC_SchedStats   rcWindowSchedStats(const RC_Window *window);  // Why this window woke, per window
```

## module: layout, L2 element DSL (`RC_NO_UI_HELPERS` hides these declarations)

```c
// Containers and sizing
rcBox(...) { ... }         // Generic container, top-to-bottom by default
rcColumn(...) { ... }      // Explicit vertical container
rcRow(...) { ... }         // Horizontal container
rcSeparator(...) { ... }   // Spacer that grows to fill the parent
rcMargin(...) { ... }      // Fit-sized spacer
rcComponent(defaults, ...) // Pairs Begin and End around your own defaults record

RC_FIT             // Shrink to content
RC_GROW            // Grow to fill the parent
RC_PX(px)          // Fixed px pixels
RC_PCT(pct)        // pct percent of the parent box
RC_VW(vw)          // vw percent of the viewport width
RC_VH(vh)          // vh percent of the viewport height
float aspectRatio; // Width over height, as CSS aspect-ratio

// Compose your own helpers, and utility classes
RC_ElementDeclaration rcParseComponentOptions(RC_ComponentOptions options, RC_ElementDeclaration base); // Build one element declaration from flat RC_ options
void                  rcBeginComponent(RC_ComponentOptions options, RC_ElementDeclaration defaults);    // Open and configure one element
void                  rcEndComponent(void);                                                             // Close the element rcBeginComponent opened

bool rcDefineClass(const char *name, const char *utilities); // Register a reusable utility class by name
void rcResetClasses(void);                                   // Forget every class and drop the resolved cache

// Constant on-screen size inside a zoomed view
     rcUnzoomed() { ... }    // Block: everything inside keeps constant on-screen size
void rcBeginUnzoomed(void);  // The scope's push, when a block will not fit
void rcEndUnzoomed(void);    // Pop the unzoomed scope
float rcUnzoomedScale(void); // 1/zoom inside a scope, exactly 1.0 outside

// Utility classes for .className; an explicit field wins, as inline style beats class
flex  flex-row  flex-col  flex-wrap  flex-wrap-reverse  flex-nowrap  flex-1  flex-auto  flex-initial  flex-none
grow  grow-0  shrink  shrink-0  items-start|center|end|stretch  self-auto|start|center|end|stretch
justify-start|center|end|between|around|evenly|normal|stretch  content-*  place-content-*  place-items-*
p-4  px-3  py-2  pt-1  ps-1  p-[5px]  gap-4  gap-x-2  gap-y-2  space-x-2  mx-auto
w-full|auto|fit|px|1/2|screen|dvw  w-xs..w-7xl  h-full|fit|screen|dvh|svh|lvh  size-full|fit|px  min-w-0  max-w-[480px]
aspect-square|video|3/2|[1.5]  overflow-auto|hidden|scroll|clip|visible  overflow-x-*  overflow-y-*  z-10  -z-10  z-[9999]
border  border-2  border-t-2  border-x-4  rounded-lg  rounded-full  rounded-tl-xl  divide-x  divide-y  ring-2  outline-4
bg-slate-800  bg-blue-500/75  bg-[#1e293b]  bg-[rgb(30_41_59)]  bg-transparent  bg-none
bg-linear-to-r  from-cyan-500  to-blue-500 // One RC_Gradient: two stops, no via-*
shadow-2xs|xs|sm|md|lg|xl|2xl|none         // One RC_Shadow; where CSS composes layers, the last token wins
sm:  md:  lg:  xl:  2xl:                   // Mobile-first at Tailwind's widths; one prefix per token

// Utility classes for RC_TextOptions.className; the vocabularies do not overlap
text-xs..text-9xl  text-slate-50  text-blue-600/50  text-inherit  text-current  text-transparent
text-left|center|right|start|end  text-wrap|nowrap|balance|pretty  leading-none..loose  leading-6
whitespace-normal|nowrap|pre|pre-wrap|pre-line|break-spaces  tracking-tighter|tight|normal|wide|wider|widest
```

## module: text, L2 (`RC_NO_UI_HELPERS` hides these declarations)

```c
// Text elements
rcTextL(literal, ...) // Text from a compile-time string literal
rcText(string, ...)   // Text from an RC_String
rcTextC(cstr, ...)    // Text from a C string
rcSelectable(on)      // A bool as .select: true -> RC_SELECT_TEXT, false -> RC_SELECT_NONE
// Selection is ON by default: a drag wraps lines AND crosses components; chrome
// (RC_SELECT_NONE) is skipped, double-click a word, triple the element, primary+A the window.

RC_TextElementConfig rcBuildTextConfig(RC_TextOptions options); // Build one text-run config from flat options
```

## module: widgets, L2 (`RC_NO_UI_HELPERS` hides these declarations)

```c
// Controls and text input
bool rcButton(const char *id, const char *label, RC_ButtonVariant variant, ...);  // True on the frame it is clicked; .color sets the label
bool rcCheckbox(const char *id, const char *label, bool *value);                  // Toggles *value on click; true on the frame it changed
bool rcToggle(const char *id, bool *value);                                       // On/off switch bound to *value; true on the frame it changed
bool rcSlider(const char *id, float *value, float min, float max);                // Clamped to [min,max]; true on a frame the value changes
void rcProgress(const char *id, float fraction);                                  // Determinate progress bar
bool rcRadio(const char *id, const char *label, int *selected, int index);        // Radio option in a mutually-exclusive group
bool rcCombo(const char *id, int *selected, const char *const *items, int count); // Dropdown that sets *selected from items

bool rcTextInput(const char *id, char *buf, int capacity, ...);   // Single-line text field over your char buffer
bool rcTextArea(const char *id, char *buf, int capacity, ...);    // Multiline text field over your char buffer

// Scrolling, virtual lists and menus
void rcScrollbar(const char *containerId);                                         // Draggable bar for a clip container
void rcScrollBy(const char *containerId, float deltaX, float deltaY);              // Scroll a container by a delta
void rcScrollToTop(const char *containerId);                                       // Jump a clip container to the top
void rcScrollToBottom(const char *containerId);                                    // Jump a clip container to the bottom
RC_ScrollInfo rcGetScrollInfo(const char *containerId);                            // Read offset and travel, DOM positive-down
bool rcIsScrolledToBottom(const char *containerId);                                // True at or past a container's scroll bottom
rcVirtualList(var, containerId, count, rowHeight) { ... }                          // Loop over only the visible rows
RC_VirtualRow rcVirtualBegin(const char *containerId, int count, float rowHeight); // Opens the run, emits the leading spacer
bool rcVirtualNext(RC_VirtualRow *row);                                            // Advance a row; emits the trailing spacer, false at the end - never break out

bool rcBeginMenu(const char *id, const char *label);           // Menu button plus its floating item column
void rcEndMenu(void);                                          // Close the open menu
bool rcBeginContextMenu(const char *id, const char *targetId); // Right-click or long-press menu on targetId
void rcEndContextMenu(void);                                   // Close the open context menu
bool rcMenuItem(const char *label);                            // A row in the open menu

// Popups
rcBeginModal(id, open, ...);                                              // Scrim plus centered panel; append .noBackdropDismiss / .modality
void rcEndModal(void);                                                    // Close the panel body
bool rcIsModalOpen(void);                                                 // True while a modal runs

// Charts, split panes and tables
void rcChart(const char *id, const RC_Series *series, int seriesCount, RC_ChartOptions options); // Multi-series plot, redeclared every frame
void rcSparkline(const char *id, const float *values, int count, RC_SparklineOptions options);   // Inline strip with no axes

bool rcBeginSplitPane(const char *id, RC_SplitAxis axis, float *fraction, RC_SplitOptions opts); // Two-pane split sized by *fraction. FALSE = nothing opened (nesting limit), skip the two below
void rcSplitHandle(void);                                                                        // Emit the divider between the two panes
void rcEndSplitPane(void);                                                                       // Close the split container

bool rcBeginTable(const char *id, const RC_TableColumn *cols, int colCount, RC_TableOptions opts); // Column-declared data grid
void rcTableRow(void);                                                                             // Close the open row and start the next
void rcTableRowId(const char *id);                                                                 // Name the open row so it can be hovered and clicked
void rcTableNext(void);                                                                            // Move to the next column's cell
void rcEndTable(void);                                                                             // Close the table
```

## module: interaction, element reads + small helpers (always available)

```c
// Element reads
bool          rcIsHovered(const char *id);          // True while the pointer is over that element
bool          rcClicked(const char *id);            // Release edge: press, then release over it
bool          rcPressed(const char *id);            // Press edge: eager, for nudge or step
bool          rcIsFocused(const char *id);          // True while that text input holds keyboard focus
void          rcSetFocus(const char *id);           // Move keyboard focus to that text input
void          rcSetCursor(RC_Cursor cursor);        // This frame's cursor shape, last writer wins
RC_Box        rcGetElementBox(const char *id);      // Where that element landed, in content space
RC_Box        rcChartPlotRect(const char *chartId); // A chart's plot area, not its outer box

// Pointer, wheel and keyboard
RC_Vec2       rcPointer(void);                            // Pointer position, in content space
bool          rcPointerDown(RC_PointerButton button);     // Level: true while the button is held
bool          rcPointerPressed(RC_PointerButton button);  // Edge that starts a drag or gesture
bool          rcPointerReleased(RC_PointerButton button); // Edge that commits the gesture
float         rcScrollDeltaY(void);                       // Wheel lines, positive up
float         rcScrollDeltaX(void);                       // Horizontal wheel delta, in lines

bool          rcKeyDown(RC_Key key);     // True while the key is held
bool          rcKeyPressed(RC_Key key);  // The frame the key goes down
bool          rcKeyReleased(RC_Key key); // True on the frame the key rises
bool          rcModDown(RC_Mod mod);     // RC_MOD_PRIMARY is Cmd on macOS, else Ctrl with Alt up

// Clipboard
void               rcClipboardSet(const char *text);         // Write text to the system clipboard
RC_ClipboardToken  rcClipboardRequest(void);                 // Start a clipboard read and return its token
const char        *rcClipboardPoll(RC_ClipboardToken token); // Collect once: text, or NULL if pending
const char        *rcClipboardGet(void);                     // Sync read; NULL on web's default async backend
bool               rcCopySelection(void);                    // Latch a copy of the live selection. TRUE = one EXISTED, not that bytes moved
bool               rcHasSelection(void);                     // True through the press that dismisses a selection, so a release-edge Copy sees it
void               rcSelectAll(void);                        // Select every selectable run; the touch-reachable select-all (primary+A needs keys)

// Clipboard backend seam; an app never calls these, a HOST integrating RayClay does
void               rcClipboardDeliver(RC_ClipboardToken token, const char *utf8); // Backend answers a pending read
void               rcSetClipboardImpl(const RC_ClipboardImpl *impl);              // Install a clipboard backend

// Frame-scheduler counters; these four need L3, rcAppPerfFrame also RC_PERF_COUNTERS=1
RC_SchedStats        rcWindowSchedStats(const RC_Window *window); // Cumulative frame-scheduler counters
const RC_PerfFrame  *rcAppPerfFrame(const RC_App *app);           // The last drawn frame's cost
const char          *rcFrameReasonName(RC_FrameReason reason);    // Label for a byReason[] index
RC_FrameReason  RC_FRAME_INITIAL RC_FRAME_INPUT RC_FRAME_WINDOW RC_FRAME_EXPOSE RC_FRAME_RESOURCE
                RC_FRAME_APP RC_FRAME_DEADLINE RC_FRAME_INTERNAL RC_FRAME_REASON_COUNT

// Colour and strings

RC_ClipSlotCounts rcClipSlotCounts(void); // Poll the clip-slot pool; zeroed before the first layout
RC_ClipSlotCounts  // .capacity .stored .attempted .rejected .highWaterMark; rejected > 0 = offsets lost now

RC_Color    rcAlpha(RC_Color color, uint8_t alpha);                // Same colour, alpha 0-255 (BYTES)
RC_Color    rcAlphaF(RC_Color color, float alpha);                 // Same colour, alpha 0.0-1.0, clamped
RC_Color    rcRgb(r, g, b) / rcRgba(r, g, b, a);                   // Channel builders, 0-255
RC_Color    rcHex(uint32_t rgb);                                   // An opaque colour from 0xRRGGBB
RC_Color    rcColor(const char *css);                              // CSS string; rgba() alpha is 0-1
void        rcStrCopy(char *dst, const char *src, size_t dstsize); // Always-NUL-terminating string copy
```

## module: platform integration

```c
// Platform hooks and types
void        rcSetAppEventHandler(RC_AppEventCallback handler, void *user);   // Lifecycle events; both mobile hosts, plus CONTENT_SCALE_CHANGED under SDL3 anywhere
RC_Insets   rcGetSafeAreaInsets(void);                                       // Notch insets, logical px; native only, zero on web; RAYCLAY_SAFE_INSETS on desktop
RC_Viewport rcViewport(void);                                                // The space Clay lays out in, plus breakpoint; every target
bool        rcPointerIsCoarse(void);                                         // True when the primary pointer is a finger; every target
void        rcSetImeCaretRect(float x, float y, float width, float height);  // Place the IME candidate window; SDL3 host, no-op under GLFW
void        rcSetSoftKeyboardVisible(bool visible);                          // Raise or dismiss the on-screen keyboard; Android and iOS only, no-op elsewhere
RC_Insets   rcGetKeyboardInsets(void);                                       // The region the soft keyboard covers; zero on desktop and web

RC_Viewport   // width, height, RC_Insets safe, breakpoint, coarsePointer, RC_Insets keyboard
RC_Breakpoint // RC_BP_BASE <640, RC_BP_SM >=640, RC_BP_MD >=768, RC_BP_LG >=1024, RC_BP_XL >=1280, RC_BP_2XL >=1536
```

## module: theme, application-wide style (`RC_NO_STYLE` hides these declarations)

```c
RC_Style        rcStyleDark(void);          // Ready-made dark theme preset
RC_Style        rcStyleLight(void);         // Ready-made light theme preset
RC_Style        rcGetStyle(void);           // Active theme by value, dark until set
void            rcSetStyle(RC_Style style); // Install a theme app-wide
const RC_Style *rcGetStylePtr(void);        // Borrowed pointer to active theme, no copy
```

## module: icons (procedural, zero-asset), L2 (`RC_NO_UI_HELPERS` hides these declarations)

```c
// Bundled artwork
void rcIconTitlebarMinimize(float size, RC_Color color); // The bundled minimize glyph, desktop only
void rcIconTitlebarMaximize(float size, RC_Color color); // The bundled maximize glyph, desktop only
void rcIconTitlebarClose(float size, RC_Color color);    // The bundled close glyph, desktop only
void rcIconRayClayLogo(float size);                      // The packaged logo; NOT in rayclay.h - include the icon header by the examples

// Your own artwork
RC_IconPoint    // One point in viewBox space, scaled to element bounds
RC_ICON_MAX_PTS // Max points in one icon path (128)

void rcIconEmit(float size, RC_Color color, RC_CustomDrawCallback draw, const void *userData); // Emits a size x size icon element

void rcIconDrawPolyline(RC_BoundingBox bounds, const RC_IconPoint *points, int pointCount, float viewBoxSize, float strokeWidth, bool closed, RC_Color color);            // Stroked path with round joins and caps
void rcIconDrawFilledPolygon(RC_BoundingBox bounds, const RC_IconPoint *points, int pointCount, float viewBoxSize, RC_Color color);                                       // Solid fill, convex or concave
void rcIconDrawFilledCircle(RC_BoundingBox bounds, float cx, float cy, float radius, float viewBoxSize, RC_Color color);                                                  // Solid disc
void rcIconDrawFilledEllipse(RC_BoundingBox bounds, float cx, float cy, float rx, float ry, float viewBoxSize, RC_Color color);                                           // Solid axis-aligned ellipse, never rotated
void rcIconDrawRoundLine(RC_BoundingBox bounds, float x0, float y0, float x1, float y1, float viewBoxSize, float strokeWidth, RC_Color color);                            // One round-capped segment, (x0,y0) to (x1,y1)
void rcIconDrawCircleStroke(RC_BoundingBox bounds, float cx, float cy, float radius, float viewBoxSize, float strokeWidth, RC_Color color);                               // Stroked circle
void rcIconDrawRoundedRectStroke(RC_BoundingBox bounds, float x, float y, float width, float height, float radius, float viewBoxSize, float strokeWidth, RC_Color color); // Stroked rounded rect

rcSvg("assets/logo.svg", 96.0f, s.text);                                  // 1. Default: no handle, no app state, no unload
static const char LOGO_SVG[] = "<svg viewBox='0 0 24 24' ...>";           // 2. One executable: embed the markup, no converter
RC_Svg *logo = rcLoadSvgFromMemory(LOGO_SVG, (int)(sizeof LOGO_SVG - 1)); // 2. Parse it once, at startup
rcSvgHandle(logo, 96.0f, s.text);                                         // 2. Draw from the handle, every frame
RC_Svg *owned = rcLoadSvg("assets/logo.svg");                             // 3. Own the lifetime: free on your own schedule
rcIconRayClayLogo(24.0f);                                                 // 4. Generated header, included separately: no parser, nothing to load
```

---

## structures

```c
// Value types
RC_Color              // RGBA float members, 0..255 per channel
RC_String             // length + chars, NOT NUL-terminated
RC_Dimensions         // width + height, in pixels
RC_Vec2               // x + y, in pixels
RC_BoundingBox        // Geometry handed to RC_CustomDrawCallback
RC_StringSlice        // length + chars + baseChars: one measured run
RC_TextElementConfig  // Resolved text styling, from rcBuildTextConfig
RC_ElementDeclaration // One element's full config (rcBeginComponent)
RC_RenderCommandArray // One frame of render commands, consumed by rcRender

// App, window and chrome configuration
RC_App                 // Opaque app and runner handle
RC_Window              // Opaque handle for one native window
RC_WindowState         // RC_WINDOW_PENDING, then READY, FAILED, CLOSING, CLOSED
RC_WindowError         // RC_WINDOW_ERROR_NONE, then INVALID_ARGUMENT, OUT_OF_MEMORY, HOST, GRAPHICS, LAYOUT
RC_WindowOptions       // One extra window's settings
RC_WindowPlacement     // RC_WINDOW_PLACE_DEFAULT (host places it), then AT (.x/.y, clamped on-screen)
RC_WindowFocusPolicy   // RC_WINDOW_FOCUS_DEFAULT (takes focus), then NONE (tool palettes)
RC_WindowTaskbar       // RC_WINDOW_TASKBAR_DEFAULT (listed), then SKIP (out of the taskbar and window list)
RC_AppOptions          // The whole app in one struct
RC_ZoomOptions         // Zoom and pan config (RC_AppOptions.zoom)
RC_TitlebarOptions     // Titlebar band config (RC_AppOptions.titlebar)
RC_ClipboardImpl       // Pluggable clipboard: set copies, request asks
RC_ClipboardToken      // Names one clipboard read
RC_Insets              // Safe-area insets, logical px: top right bottom left
RC_TitlebarButtonIcons // normal/hover/press RC_IconCallback overrides

// Element, text and modal options
RC_ComponentOptions // Every field behind rcBox, rcRow and rcColumn
RC_TextOptions      // className, font, color, size, lineHeight, letterSpacing, wrap, textAlign, select
RC_TextInputOptions // placeholder, font, password, rows, multiline
RC_ModalOptions     // modality plus noBackdropDismiss
RC_Modality         // RC_MODALITY_MODAL (scrim) or RC_MODALITY_NON_MODAL (live)

// Widget and chart options
RC_ButtonOptions        // rcButton: variant (the 3rd argument) plus color, the label colour, and select
RC_SplitOptions         // rcBeginSplitPane: minFraction, maxFraction, handleThickness
RC_TableColumn          // One column: header, align, width - .w ("48px"/"25%"/"grow") or .wType; zero-init = GROW
RC_TableOptions         // rcBeginTable cell padding
RC_WindowControlOptions // Window-control slab: iconSize, width, height; 0 = bundled default
RC_OptFloat / RC_VAL(x) // {value, set} - unset versus zero, for a field where 0 counts. RC_VAL(x) sets both
RC_LIT(T)               // Compound-literal spelling that compiles as C AND C++

**`RC_LIT(T)` - one spelling that compiles in both languages.** A compound literal is `(T){...}` in
C and `T{...}` in C++, so a header shared by both cannot write either one directly. `RC_LIT` picks
the right form: `RC_LIT(RC_Color){255, 0, 0, 255}`. This is why the supported claim is *"a C++
project can consume RayClay's public C API"* and not *"RayClay's C code is C++"*: the public header
and the DSL are written to compile in both, while a shipped C example is C and stays C - MSVC
refuses its bare `(T){...}` with `error C4576`, and g++ and clang++ take it only as an extension.

**It is NOT addressable in C++, and that is not a wart you can work around in place.** `&RC_LIT(P){1,2}`
is legal C - a compound literal is an lvalue with automatic storage duration - and ill-formed in
C++, where `P{1,2}` is a prvalue temporary. If you need the address, name a variable first. This is
the one direction where the C spelling is strictly more capable, which is why `RC_LIT` is a spelling
aid rather than a drop-in for every compound literal you already have.

RC_Series           // One dataset: y, x, count, kind, color, label, thickness, points. axis 0 = y, 1 = y2
RC_Axis             // One axis: min, max, label, ticks, grid, hide. Both 0 = auto-fit, else pins the range
RC_ChartOptions     // rcChart: x, y, y2 axes, legend, fontSize, tooltip, hover cues
RC_SparklineOptions // rcSparkline: kind, color, min, max, thickness

// Layout queries, decoration, geometry and resources
RC_VirtualRow // rcVirtualList loop variable: index, first, last, found
RC_Box        // rcGetElementBox / rcChartPlotRect result, content space
RC_ScrollInfo // {offsetX, offsetY, maxOffsetX, maxOffsetY, found} - travel = content - viewport, >= 0

RC_Border   // Colour plus one all-sides width ("1", "1px", "all-1"); per side: .className "border-b-2"
RC_Gradient // Two-stop gradient (from, to, dir)
RC_Shadow   // Drop shadow: color, x, y, blur, spread
RC_Float    // Lift out of flow and anchor to a target
RC_Size     // {mode, value} - RC_FIT, RC_GROW, RC_PX, RC_PCT, RC_VW, RC_VH. Zero-init means UNSET, not zero

RC_Image          // Opaque image handle: handle, width, height
RC_Svg            // Opaque parsed-SVG handle
RC_Arena          // Bump allocator: buffer, bufferLength, currOffset
RC_CustomDrawData // Custom-element payload: magic, draw, userData, color
RC_Style          // Semantic theme colours plus radius, padding, gap
```

## callbacks

```c
// App and extra-window callbacks
typedef void (*RC_UpdateCallback)(RC_App *app, void *userData);                                     // Per-frame update hook
typedef void (*RC_LayoutCallback)(RC_App *app, void *userData);                                     // Per-frame layout hook
typedef void (*RC_FrameEndCallback)(RC_App *app, void *userData);                                   // Runs last on a DRAWN frame
typedef void (*RC_CustomDrawCallback)(RC_BoundingBox bounds, RC_Color color, const void *userData); // Draw callback for a CUSTOM element
typedef void (*RC_IconCallback)(float size, RC_Color color);                                        // Draw one glyph at size, in the given colour
typedef void (*RC_AppEventCallback)(RC_AppEvent event, void *user);                                 // App lifecycle observer
typedef void (*RC_LogCallback)(RC_LogLevel level, const char *msg, void *user);                     // Log sink for rcSetLogSink

typedef void (*RC_WindowUpdateCallback)(RC_Window *window, void *userData);                      // Per-frame update hook for one extra window
typedef void (*RC_WindowLayoutCallback)(RC_Window *window, void *userData);                      // Per-frame layout hook for one extra window
typedef void (*RC_WindowFrameEndCallback)(RC_Window *window, void *userData);                    // Runs last on a drawn frame of that window
typedef void (*RC_WindowStateCallback)(RC_Window *window, RC_WindowState state, void *userData); // READY, FAILED and CLOSED, never before open returns
```

## enumerations

```c
// Styling, sizing, panes and charts
RC_FontWeight    // RC_WEIGHT_THIN(100) EXTRALIGHT LIGHT REGULAR MEDIUM SEMIBOLD BOLD EXTRABOLD BLACK(900)
RC_ButtonVariant // RC_BTN_DEFAULT RC_BTN_PRIMARY RC_BTN_DANGER RC_BTN_GHOST
RC_SelectMode    // RC_SELECT_AUTO(0) selectable, a button reads it as NONE; RC_SELECT_NONE never; RC_SELECT_TEXT always
RC_SizeMode      // RC_SIZE_UNSET RC_SIZE_FIT RC_SIZE_GROW RC_SIZE_FIXED RC_SIZE_PERCENT RC_SIZE_VW RC_SIZE_VH
RC_SplitAxis     // RC_SPLIT_ROW(0) side by side, RC_SPLIT_COLUMN stacked

RC_SeriesKind        // RC_SERIES_LINE(0) RC_SERIES_BAR RC_SERIES_AREA RC_SERIES_SCATTER
RC_ChartTooltip      // RC_CHART_TOOLTIP_NONE(0) RC_CHART_TOOLTIP_NEAREST on hover
RC_ChartTooltipPlace // RC_TOOLTIP_PLACE_CURSOR(0) RC_TOOLTIP_PLACE_CORNER RC_TOOLTIP_PLACE_FIXED
RC_AttachTo          // RC_ATTACH_NONE(0) RC_ATTACH_PARENT RC_ATTACH_ELEMENT RC_ATTACH_ROOT
RC_Anchor            // RC_ANCHOR_TOP_LEFT TOP_CENTER TOP_RIGHT CENTER_LEFT CENTER CENTER_RIGHT BOTTOM_LEFT BOTTOM_CENTER BOTTOM_RIGHT
RC_Capture           // RC_CAPTURE_DEFAULT RC_CAPTURE_ON eat pointer events, RC_CAPTURE_PASSTHROUGH does not
RC_FloatClip         // RC_CLIP_NONE(0) escapes the target's clipping, RC_CLIP_TO_PARENT shares it

// Window, input and app lifecycle
RC_WindowControl // RC_WINCTL_MINIMIZE RC_WINCTL_MAXIMIZE RC_WINCTL_CLOSE
RC_ZoomMode      // RC_ZOOM_LAYOUT(0) reflows the UI, RC_ZOOM_OPTICAL magnifies it
RC_RenderMode    // RC_RENDER_ON_DEMAND(0) draws on events, RC_RENDER_CONTINUOUS every vsync
RC_Cursor        // RC_CURSOR_DEFAULT/POINTER/TEXT/GRAB/GRABBING/NOT_ALLOWED, set by rcSetCursor
RC_Key           // 119 RC_KEY_* names: A..Z 0..9 F1..F24 RC_KEY_KP_* RC_KEY_NONE(0) RC_KEY_COUNT
RC_Mod           // RC_MOD_PRIMARY(Cmd on macOS, else Ctrl with Alt up) RC_MOD_SHIFT RC_MOD_ALT RC_MOD_CTRL RC_MOD_SUPER
RC_PointerButton // RC_POINTER_LEFT(0) RC_POINTER_RIGHT(1) RC_POINTER_MIDDLE(2)
RC_AppEvent      // RC_APP_EVENT_CREATED/SUSPENDED/RESUMED/CONTEXT_LOST/CONTEXT_RESTORED/CONTENT_SCALE_CHANGED/TERMINATING
RC_Orientation   // RC_ORIENTATION_ANY(0) RC_ORIENTATION_PORTRAIT RC_ORIENTATION_LANDSCAPE, a phone lock
RC_LogLevel      // RC_LOG_INFO(0) RC_LOG_WARNING(1) RC_LOG_ERROR(2), values ABI-stable
```

## titlebar control ids

```c
// Tagged by rcTitlebar and rcWindowControls; desktop only
RC_ID_WINDOW_MINIMIZE // "RC_Window_Minimize", click to minimize
RC_ID_WINDOW_MAXIMIZE // "RC_Window_Maximize", click to maximize or restore
RC_ID_WINDOW_CLOSE    // "RC_Window_Close", click to close the window
RC_ID_WINDOW_DRAG     // "RC_Window_Drag", a press anywhere starts an OS window move
RC_ID_WINDOW_NODRAG   // "RC_Window_NoDrag", subtree opts out of the drag
```

## constants

```c
// Version and the custom-draw sentinel
RC_VERSION_MAJOR // integer major version
RC_VERSION_MINOR // integer minor version
RC_VERSION_PATCH // integer patch version
RC_VERSION       // version string, always "MAJOR.MINOR.PATCH"

RC_CUSTOM_DRAW_MAGIC // 0x52434457u ('RCDW') in RC_CustomDrawData.magic, checked by rcRender
```

## colors: Tailwind-style palette and your own (`RC_NO_COLOR_PALETTE` hides these declarations)

```c
// Palette: RC_<FAMILY>_<shade>, shades 50 100 200 300 400 500 600 700 800 900 950
RC_BLACK   RC_WHITE   RC_TRANSPARENT                                                            // Named, outside the 50-950 ramps
RC_SLATE_*  RC_GRAY_*   RC_ZINC_*    RC_NEUTRAL_* RC_STONE_*   RC_RED_*     RC_ORANGE_*         // 22 of Tailwind's 26 families
RC_AMBER_*  RC_YELLOW_* RC_LIME_*    RC_GREEN_*   RC_EMERALD_* RC_TEAL_*    RC_CYAN_*           // 11 shades each, greys first
RC_SKY_*    RC_BLUE_*   RC_INDIGO_*  RC_VIOLET_*  RC_PURPLE_*  RC_FUCHSIA_* RC_PINK_* RC_ROSE_* // Tailwind v3 sRGB, not v4 oklch

// Your own palette
#define BRAND_500  rcRgb(99, 102, 241)                   // Expands in place, no call
#define BRAND_700  rcRgb(67, 56, 202)                    // File-scope tokens use #define
#define BRAND_A20  rcRgba(99, 102, 241, 51)              // Fourth channel is alpha, 0-255
#define ACCENT_400 rcHex(0x22d3ee)                       // Hex to colour, no string parse
#define ACCENT_500 rcColor("#06b6d4")                    // CSS string, parsed on every use
RC_Color accent = BRAND_500;                             // In a function, an ordinary initialiser
static const RC_Color SLATE_800 = { 30, 41, 59, 255 };   // The only constant expression here

rcColumn(.bg = BRAND_700, .gap = 8, .p = 16) {                    // A token is just an RC_Color
    rcBox(.bg = BRAND_500, .borderRadius = "-md", .h = "40px");   // "-md" rounds all four corners
    rcBox(.bg = ACCENT_400, .borderRadius = "-md", .h = "40px");  // Any token, built-in or your own
}
```

## build-time configuration

```text
// Consumer-view trims [CONSUMER-VIEW]
RC_NO_UI_HELPERS       // Hide L2 from this TU: element DSL, widgets, rcIconEmit, rcIconDraw*
RC_NO_STYLE            // Hide the RC_Style theme layer from this TU, keep the palette
RC_NO_COLOR_PALETTE    // Hide the Tailwind palette from this TU; implies RC_NO_STYLE, built from it

// NOTE: a [CONSUMER-VIEW] knob trims YOUR target only; every knob below needs EVERY TU
RC_NO_APP_RUNNER       // Drop L3: the app runner, rcRunApp and rcRunFrame
RC_NO_IMAGE            // Stub rcLoadImage*, drop stb_image from the build
RC_NO_SVG              // Compile out the SVG parser
RC_NO_BUNDLED_FONT     // Drop the bundled Roboto Latin-1 subset
RC_NO_DEFAULT_APP_ICON // Drop the bundled taskbar and dock icon (15,833 B)
RC_NO_LIVE_RESIZE      // Drop the repaint hook for the OS resize loop

// Renderer, memory and fixed caps
RC_GFX_PACKET        // 1 (default) packet renderer, 0 the alternate one
RC_GFX_TEXT_CACHE    // 0 (default) or 1: cache text-run quads, not pixel-exact
RC_GFX_TEXT_CACHE_KB // Text-cache size in KB, default 64, range 4..4096
RC_FLATNESS_TOL      // Max physical px between drawn and true curve (0.25)
RC_GFX_MSAA_SAMPLES  // Samples 1, 2 or 4 (default 4)

RC_DEFAULT_START_LAYOUT_ELEMENTS // Default for RC_AppOptions.startLayoutElements (2048, ~1.4 MiB)
RC_DEFAULT_MAX_LAYOUT_ELEMENTS   // Default for RC_AppOptions.maxLayoutElements (65536)
RC_GFX_PACKET_MAX_VERTICES       // Vertex arena, 98304 slots x 16 B
RC_GFX_PACKET_MAX_INDICES        // Index arena, 3x the vertex count
RC_GFX_PACKET_MAX_SPANS          // Draw-state spans per frame, 2048 x 24 B, host only
RC_SGL_MAX_VERTICES              // Alternate renderer's vertex pool (256*1024)
RC_SGL_MAX_COMMANDS              // Alternate renderer only: command pool (16*1024)

RC_ICON_POOL_CAPACITY  // Growth step of the icon pool (256); a hard ceiling without a runner
RC_SVG_CACHE_MAX       // Distinct rcSvg() paths held at once (64)
RC_FONT_ATLAS_W / _H   // Glyph-atlas edge in px (1024)
RC_MAX_FONTS           // Font-ladder slots (16), one per family, weight and size
RC_FONT_LAST_CODEPOINT // Highest codepoint the glyph table covers (255); past it text draws '?', warns once each
RC_CHART_MAX_SERIES    // Series per chart (16)
RC_GRADIENT_MAX        // Distinct .gradient elements per frame (64)
RC_SHADOW_MAX          // Distinct .shadow elements per frame (64)
RC_CLASS_DEFINE_MAX    // Names rcDefineClass holds (32)
RC_CLASS_CACHE_MAX     // Resolved-string cache slots (44) in eleven four-way sets, PER SURFACE
RC_CLASS_KEY_MAX       // Bytes of one cached .className (96)
RC_CLASS_NAME_MAX      // Bytes of a class name including the NUL (24)
RC_CLASS_NEST_MAX      // How deep a class may name other classes (4)

// Instruments
RC_EDIT_BLINK_TIMEOUT // Seconds a caret blinks before it settles (10.0)
RC_GFX_DIGEST         // 1 = warn once if the app animates without requesting frames
RC_PERF_COUNTERS      // 1 = unlock rcAppPerfFrame(app), off by default
RC_PERF_PHASES        // 1 = time the 8 startup phases, one report at exit
RC_DEBUG_TOOLS        // Layout inspector out (0, default) or in (1)  [both]

// Linkage and CMake options
RC_BUILD_SHARED       // Define when COMPILING RayClay shared: RC_API is dllexport
RC_USE_SHARED         // Define when CONSUMING RayClay shared: RC_API is dllimport

// NOTE: RC_SIZE_OPT is a CMake cache option, not a #define; the link flags hide the Win console
-DRC_SIZE_OPT=ON                          // Size posture: LTO, section GC, static stb_truetype

/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup  // MSVC: keeps a plain int main() linking
-mwindows                                 // MinGW and clang: the same in one flag
```

---

*This card mirrors the style of the
[raylib cheatsheet](https://www.raylib.com/cheatsheet/cheatsheet.html).*
