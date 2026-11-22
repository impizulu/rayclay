# RayClay API

> **v1.0.x quick reference card**: every public symbol, one line each.

Guides: [getting-started.md](getting-started.md), [ui.md](ui.md), [desktop.md](desktop.md),
[mobile.md](mobile.md), [web.md](web.md).

## module: app

```c
// App runner
int rcRunApp(const RC_AppOptions *options);                        // Run until it quits
void rcAppRequestClose(RC_App *app);                               // Quit after this frame
RC_Arena *rcAppArena(RC_App *app);                                 // Frame arena
double rcAppTime(const RC_App *app);                               // App clock, seconds
RC_App *rcAppCreate(const RC_AppOptions *options);                 // Open app, own loop
bool rcRunFrame(RC_App *app);                                      // Run one frame
bool rcAppDestroy(RC_App *app);                                    // Close and free app (false: kept)

// Frame requests
void rcWindowRequestFrame(RC_Window *window);                          // Ask for one frame
void rcWindowRequestFrameAfter(RC_Window *window, double seconds);     // Ask for a frame later
void rcAppWake(void);                                                  // Ask for a frame, any thread
void rcWindowSetRenderMode(RC_Window *window, RC_RenderMode mode);     // Draw every vsync, or on demand
RC_RenderMode rcWindowRenderMode(const RC_Window *window);             // Render mode asked for

// Windows
bool rcChildWindowsSupported(void);                                        // Check extra windows work
RC_Window *rcAppOpenWindow(RC_App *app, const RC_WindowOptions *options);  // Open another window
RC_Window *rcAppMainWindow(RC_App *app);                                   // Main window
RC_Window *rcAppCurrentWindow(RC_App *app);                                // Callback's window
RC_App *rcWindowApp(const RC_Window *window);                              // Window's app
RC_WindowState rcWindowState(const RC_Window *window);                     // Window state
RC_WindowError rcWindowError(const RC_Window *window);                     // Why it failed
void rcWindowRequestClose(RC_Window *window);                              // Close window
void rcWindowCancelClose(RC_Window *window);                               // Keep it: answers user's close
bool rcWindowRelease(RC_Window *window);                                   // Free closed window
RC_Arena *rcWindowArena(RC_Window *window);                                // Window's frame arena
RC_Dimensions rcWindowSize(const RC_Window *window);                       // Window size
bool rcWindowSetSize(RC_Window *window, int width, int height);            // Set window size, within the screen
float rcWindowContentScale(const RC_Window *window);                       // Device pixel ratio
bool rcWindowPosition(const RC_Window *window, int *x, int *y);            // Window position
bool rcWindowSetPosition(RC_Window *window, int x, int y);                 // Set position
bool rcWindowPositionSupported(void);                                      // Check windows can be placed
bool rcWindowOwnerSupported(void);                                         // Check windows can have owners
bool rcWindowMinimize(RC_Window *window);                                  // Minimize
bool rcWindowMaximize(RC_Window *window);                                  // Maximize
bool rcWindowRestore(RC_Window *window);                                   // Restore
bool rcWindowIsMinimized(const RC_Window *window);                         // Check minimized
bool rcWindowIsMaximized(const RC_Window *window);                         // Check maximized
bool rcWindowIsFocused(const RC_Window *window);                           // Check focused
bool rcWindowRaise(RC_Window *window);                                     // Raise and focus window
bool rcWindowSetTitle(RC_Window *window, const char *title);               // Set title
RC_Color rcWindowClearColor(const RC_Window *window);                      // Background color
void rcWindowSetClearColor(RC_Window *window, RC_Color color);             // Set background color
bool rcWindowSetTopmost(RC_Window *window, bool on);                       // Pin above other apps
bool rcWindowIsTopmost(const RC_Window *window);                           // Check pinned
bool rcWindowTopmostSupported(void);                                       // Check windows can be pinned
float rcWindowZoom(const RC_Window *window);                               // Zoom factor
void rcWindowSetZoom(RC_Window *window, float zoom);                       // Set zoom factor
RC_ZoomMode rcWindowZoomMode(const RC_Window *window);                     // Zoom mode
void rcWindowSetZoomMode(RC_Window *window, RC_ZoomMode mode);             // Set zoom mode
const float *rcWindowZoomLadder(const RC_Window *window, uint16_t *count); // Zoom key stops
RC_Mirror rcWindowMirror(const RC_Window *window);                         // Mirroring
void rcWindowSetMirror(RC_Window *window, RC_Mirror mirror);               // Mirror picture and input
int rcWindowTitlebarHeight(const RC_Window *window);                       // Titlebar height
void rcWindowSetTitlebarHeight(RC_Window *window, int height);             // Set titlebar height
float rcWindowFrameTime(const RC_Window *window);                          // Last frame's delta, seconds
RC_FrameCounts rcWindowFrameCounts(const RC_Window *window);               // Last frame's counts
RC_ClipSlotCounts rcWindowClipSlotCounts(const RC_Window *window);         // Scroll clip pool use
const RC_PerfFrame *rcWindowPerfFrame(const RC_Window *window);            // Frame's work (RC_PERF_COUNTERS=1)
RC_SchedStats rcWindowSchedStats(const RC_Window *window);                 // Frame scheduler counters
const char *rcFrameReasonName(RC_FrameReason reason);                      // Frame reason's name

// Titlebar
void rcWindowControlButton(RC_WindowControl control, RC_IconCallback icon, ...);  // Draw one window control
void rcWindowControls(...);                               // Draw minimize, maximize, close
float rcWindowControlsWidth(void);                        // Their width (web/phones: 0)
void rcIconTitlebarMinimize(float size, RC_Color color);  // Non-web minimize glyph
void rcIconTitlebarMaximize(float size, RC_Color color);  // Non-web maximize glyph
void rcIconTitlebarClose(float size, RC_Color color);     // Non-web close glyph
```

## module: layout

```c
// Elements: rcBox(.className = "p-4 bg-slate-800") { children }
rcBox(...) { }                                                // Container, top to bottom
rcColumn(...) { }                                             // Vertical container
rcRow(...) { }                                                // Horizontal container
rcSpacer(...) { }                                             // Spacer that grows
rcComponent(defaults, ...) { }                                // Element with your defaults
RC_ElementDeclaration rcParseComponentOptions(RC_ComponentOptions options, RC_ElementDeclaration defaults);  // Options to declaration
void rcBeginComponent(RC_ComponentOptions options, RC_ElementDeclaration defaults);  // Open element
void rcEndComponent(void);                                    // Close element
bool rcDefineClass(const char *name, const char *utilities);  // Define utility class
void rcResetClasses(void);                                    // Forget defined classes
void rcSetRootFontSize(float px);                             // Set rem size
float rcRootFontSize(void);                                   // Rem size

// Sizes: .w = "48px" "50%" "50vw" "2rem" "grow" "fit", or .wType
RC_FIT       // Shrink to content
RC_GROW      // Fill parent
RC_PX(px)    // Fixed pixels
RC_PCT(pct)  // Percent of parent
RC_VW(vw)    // Percent of viewport width
RC_VH(vh)    // Percent of viewport height
RC_VAL(x)    // Set RC_OptFloat
RC_UNSET_VAL // Unset RC_OptFloat
RC_LIT(T)    // Compound literal, C and C++
// NOTE: a C static uses { }, not RC_LIT (RC_PX, rcRgb, RC_SLATE_800)

// Zoom-free subtree
rcUnzoomed() { }              // Keep on-screen size
void rcBeginUnzoomed(void);   // Open scope
void rcEndUnzoomed(void);     // Close scope
float rcUnzoomedScale(void);  // Scale inside scope

// Text
rcTextL(literal, ...);                                                          // Text from string literal
rcText(string, ...);                                                            // Text from RC_String
rcTextC(cstr, ...);                                                             // Text from C string
rcSelectable(on)                                                                // Bool to RC_SelectMode
RC_TextElementConfig rcBuildTextConfig(RC_TextOptions options);                 // Options to text config
float rcTextWidth(uint16_t font, uint16_t size, const char *text, int length);  // Measure text run

// Scrolling and long lists
void rcScrollbar(const char *containerId, ...);                        // Draw scrollbar
void rcScrollBy(const char *containerId, float deltaX, float deltaY);  // Scroll by amount
void rcScrollToTop(const char *containerId);                           // Scroll to top
void rcScrollToBottom(const char *containerId);                        // Scroll to bottom
void rcScrollTo(const char *containerId, float x, float y);            // Scroll to absolute offset
void rcScrollIntoView(const char *containerId, const char *elementId, RC_ScrollAlign align);  // Scroll into view, after layout
void rcScrollToRow(const char *containerId, int index, RC_ScrollAlign align);  // Reveal virtual-list row
RC_ScrollInfo rcGetScrollInfo(const char *containerId);                // Scroll offsets
rcVirtualList(row, containerId, count, rowHeight, ...) { }             // Loop over visible rows
```

## module: widgets

```c
// Controls
bool rcButton(const char *id, const char *label, ...);                           // Button; true when clicked
bool rcCheckbox(const char *id, const char *label, bool *value, ...);            // Checkbox; true when changed
bool rcToggle(const char *id, const char *label, bool *value, ...);  // Switch, its label before it; true when changed
bool rcSlider(const char *id, float *value, float min, float max, ...);          // Slider; true when changed
bool rcRadio(const char *id, const char *label, int *selected, int index, ...);  // Radio option; true when chosen
bool rcCombo(const char *id, int *selected, const char *const *items, int count, ...);  // Dropdown; true when changed
bool rcTextInput(const char *id, char *buf, int capacity, ...);                  // Text field; true when edited
bool rcTextArea(const char *id, char *buf, int capacity, ...);                   // Multiline field; true when edited
void rcProgress(const char *id, float fraction, ...);                            // Draw progress bar

// Menus and modals: Begin returns true while open
bool rcBeginMenu(const char *id, const char *label, ...);       // Menu button
void rcEndMenu(void);                                           // End menu body
bool rcBeginContextMenu(const char *id, const char *targetId, ...);  // Right-click menu on target
void rcEndContextMenu(void);                                    // End context menu body
bool rcMenuItem(const char *label, ...);                        // Menu row; true when chosen
bool rcBeginModal(const char *id, bool *open, ...);             // Modal dialog
void rcEndModal(void);                                          // End modal body
bool rcIsModalOpen(void);                                       // Check modal is open

// Tables, panes and charts
bool rcBeginTable(const char *id, const RC_TableColumn *cols, int colCount, ...);  // Start table
void rcTableRow(...);                         // Next row; .id makes it clickable
void rcTableNext(void);                       // Next cell
uint64_t rcTableHiddenColumns(const char *id);  // Bit i set: column i hidden by .priority
void rcEndTable(void);                        // End table
bool rcBeginSplitPane(const char *id, RC_SplitAxis axis, float *fraction, ...);  // Start two resizable panes
void rcSplitHandle(void);                     // Draw divider
void rcEndSplitPane(void);                    // End panes
void rcChart(const char *id, const RC_Series *series, int seriesCount, ...);  // Draw chart
void rcSparkline(const char *id, const float *values, int count, ...);  // Draw sparkline
RC_Box rcChartPlotRect(const char *chartId);  // Plot area
```

## module: input

```c
// Elements, by id
bool rcClicked(const char *id);          // Check element was clicked
int  rcClickCount(void);                 // Click count (event.detail)
bool rcPressed(const char *id);          // Check element was pressed
bool rcContextClicked(const char *id);   // Check right-click or long press
bool rcIsHovered(const char *id);        // Check pointer is over
bool rcShowsHover(const char *id);       // Check hover look applies
bool rcShowsActive(const char *id);      // Check press look applies
bool rcIsFocused(const char *id);        // Check element has focus
void rcSetFocus(const char *id);         // Move keyboard focus
RC_FieldSelection rcGetFieldSelection(const char *fieldId);            // Focused field's anchor, caret
bool rcSetFieldSelection(const char *fieldId, int anchor, int caret);  // Select its bytes
RC_Box rcGetElementBox(const char *id);  // Last frame's box, 0 at first
bool rcIsDragOver(const char *id);       // Check drag is over
void rcSetCursor(RC_Cursor cursor);      // Set cursor this frame

// Pointer and keyboard
RC_Vec2 rcPointer(void);                          // Pointer position; reading it draws every move
bool rcPointerDown(RC_PointerButton button);      // Check button is held
bool rcPointerPressed(RC_PointerButton button);   // Check button went down
bool rcPointerReleased(RC_PointerButton button);  // Check button went up
bool rcPointerIsCoarse(void);                     // Check pointer is a finger
RC_Vec2 rcWheelDelta(void);                       // Wheel notches: +y down, +x right
bool rcKeyDown(RC_Key key);                       // Check key is held
bool rcKeyPressed(RC_Key key);                    // Check key went down
bool rcKeyPressedOn(const char *id, RC_Key key);  // Check key went down on that focused stop
bool rcKeyReleased(RC_Key key);                   // Check key went up
bool rcModDown(RC_Mod mod);                       // Check modifier is held
bool rcKeyEvent(int index, RC_KeyEvent *event);   // Frame's key presses, in order
void rcKeyEventConsume(int index);                // Stop that press's typing and field action

// Clipboard and selection
void rcClipboardSet(const char *text);                               // Set clipboard text
RC_ClipboardToken rcClipboardRequest(void);                          // Start clipboard read
RC_ClipboardResult rcClipboardPoll(RC_ClipboardToken token);         // Read's status and text
bool rcCopySelection(void);                                          // Copy selected text
bool rcHasSelection(void);                                           // Check text is selected
void rcSelectAll(void);                                              // Select all text
```

## module: graphics

```c
// Fonts
uint16_t rcLoadFont(const char *path, float size);                               // Load font, get its id
uint16_t rcLoadFontFromMemory(const void *bytes, size_t length, float size);     // Same, from memory
uint16_t rcRegisterFont(const char *family, RC_FontWeight weight, const char *path, float size);  // Load font under family
uint16_t rcRegisterFontFromMemory(const char *family, RC_FontWeight weight, const void *bytes, size_t length, float size);  // Same, from memory
uint16_t rcFont(const char *family, RC_FontWeight weight, float size);           // Find registered font
uint16_t rcFontSemibold(float size);                                             // Bundled Roboto SemiBold
bool rcUnloadFont(uint16_t font);                                                // Free font slot
int rcFontMissingGlyphs(uint16_t font, const char *text, int len, uint32_t *firstMissing);  // Count glyphs drawn as boxes

// Images and SVG
RC_Image rcLoadImage(const char *path);                               // Load PNG, JPG, BMP, GIF or WebP
RC_Image rcLoadImageFromMemory(const void *bytes, size_t length);     // Same, from memory
RC_ImagePixels rcLoadImagePixels(const char *path);                   // Decode to RGBA8 you own
RC_ImagePixels rcLoadImagePixelsFromMemory(const void *bytes, size_t length);     // Decode from memory
void rcFreeImagePixels(RC_ImagePixels *pixels);                       // Free decoded pixels
bool rcImageUpdate(RC_Image *img, const void *pixels, int width, int height, int stride, RC_PixelFormat format);  // Replace pixels, as putImageData
void rcUnloadImage(RC_Image *img);                                    // Free image
void rcSvg(const char *path, float size, RC_Color color);             // Draw SVG file
RC_Svg *rcLoadSvg(const char *path);                                  // Parse SVG file
RC_Svg *rcLoadSvgFromMemory(const void *bytes, size_t length);        // Parse SVG text
void rcSvgHandle(const RC_Svg *svg, float size, RC_Color color);      // Draw parsed SVG
void rcUnloadSvg(RC_Svg **svg);                                       // Free parsed SVG

// Icons
void rcIconEmit(float size, RC_Color color, RC_CustomDrawCallback draw, const void *userData);  // Emit drawn icon
void rcCustomDraw(RC_CustomDrawCallback draw, const void *userData, RC_Color color);  // Custom drawing filling its parent
void rcDrawIcon<Name>(RC_BoundingBox bounds, RC_Color color, const void *userData);  // Icon draw callback, e.g. rcDrawIconTitlebarClose
void rcIconDrawPolyline(RC_BoundingBox bounds, const RC_IconPoint *points, int pointCount, float viewBoxSize, float strokeWidth, bool closed, RC_Color color);  // Stroke path
void rcIconDrawFilledPolygon(RC_BoundingBox bounds, const RC_IconPoint *points, int pointCount, float viewBoxSize, RC_Color color);  // Fill one outline, nonzero
void rcIconDrawFilledCircle(RC_BoundingBox bounds, float cx, float cy, float radius, float viewBoxSize, RC_Color color);  // Fill circle
void rcIconDrawFilledEllipse(RC_BoundingBox bounds, float cx, float cy, float rx, float ry, float viewBoxSize, RC_Color color);  // Fill ellipse
void rcIconDrawRoundLine(RC_BoundingBox bounds, float x0, float y0, float x1, float y1, float viewBoxSize, float strokeWidth, RC_Color color);  // Stroke round-capped line
void rcIconDrawCircleStroke(RC_BoundingBox bounds, float cx, float cy, float radius, float viewBoxSize, float strokeWidth, RC_Color color);  // Stroke circle
void rcIconDrawRoundedRectStroke(RC_BoundingBox bounds, float x, float y, float width, float height, float radius, float viewBoxSize, float strokeWidth, RC_Color color);  // Stroke rounded rectangle
void rcIconRayClayLogo(float size);  // Logo; its header ships with the examples

// Color
RC_Color rcRgb(r, g, b);                          // Opaque color, 0-255
RC_Color rcRgb(r, g, b, alpha);                   // Color with alpha, 0-1
RC_Color rcHex(uint32_t rgb);                     // Color from 0xRRGGBB
RC_Color rcColor(const char *css);                // Color from CSS string
RC_Color rcWithAlpha(RC_Color color, float alpha);  // Set alpha, 0-1

// Themes
RC_Style rcStyleDark(void);           // Dark theme
RC_Style rcStyleLight(void);          // Light theme
RC_Style rcGetStyle(void);            // Active theme
void rcSetStyle(RC_Style style);      // Set active theme
const RC_Style *rcGetStylePtr(void);  // Borrow active theme
```

## module: platform

```c
void rcSetAppEventHandler(RC_AppEventCallback handler, void *userData);   // Watch lifecycle events
RC_ColorScheme rcPreferredColorScheme(void);                          // Light or dark preference
int rcPreferredLocales(const char **tags, int max);                   // User's languages; take the first you have
RC_Viewport rcViewport(void);                                         // Viewport and breakpoint
RC_Insets rcGetSafeAreaInsets(void);                                  // Safe area, window px
RC_Insets rcGetKeyboardInsets(void);                                  // Keyboard cover, window px
void rcSetSoftKeyboardVisible(bool visible);                          // Show or hide keyboard
void rcSetImeCaretRect(float x, float y, float width, float height);  // Place IME at caret
bool rcSetStatusBar(RC_StatusBar style);                              // Set phone status bar
bool rcSetKeepScreenOn(bool on);                                      // Keep screen awake; video never does
bool rcOpenUrl(const char *url);                                      // Open http(s) URL, from user action
```

## module: files

```c
RC_FileStatus rcLoadFile(const char *path, void **bytes, size_t *length);         // Read whole file
void rcFreeFile(void *bytes);                                                     // Free rcLoadFile's buffer
RC_FileStatus rcSaveFile(const char *path, const void *bytes, size_t length, unsigned flags);  // Write whole file, atomically
RC_FileStatus rcPathInfo(const char *path, RC_PathInfo *out);                     // File info
RC_FileStatus rcMakeDirectory(const char *path, unsigned flags);                  // Make folder
RC_FileStatus rcRemovePath(const char *path, unsigned flags);                     // Remove file or folder
RC_FileStatus rcListDirectory(const char *path, RC_DirectoryEntryCallback callback, void *userData);  // List folder
const char *rcAppDataPath(const char *org, const char *app);                      // App data folder
RC_SaveCheckpoint rcSaveCheckpoint(void);                                         // Mark saves so far
RC_SaveStatus rcSavePoll(RC_SaveCheckpoint checkpoint);                           // Check they are stored
const char *rcAppCachePath(const char *org, const char *app);                     // App cache folder
int rcDroppedFileCount(void);                                                     // Files dropped this frame
const char *rcDroppedFile(int index);                                             // Dropped file's path
RC_FileDialogToken rcFileDialogOpen(RC_FileDialogKind kind, const char *accept);  // System open dialog: files or folder
RC_FileDialogToken rcFileDialogSave(const char *name, const void *bytes, size_t len);  // System save dialog
RC_FileDialogResult rcFileDialogPoll(RC_FileDialogToken token);                   // Dialog's answer
void rcReleaseFile(const char *path);                                             // Done with path
```

## module: network

```c
// HTTP: start once, read each frame, as fetch()
RC_Fetch rcFetch(const char *url, ...);                                     // Start request
const char *rcFetchCheck(const char *url, ...);                             // Check request, send nothing
RC_FetchResult rcFetchResult(RC_Fetch fetch);                               // Request state
const char *rcFetchHeader(RC_Fetch fetch, const char *name);                // Response header
RC_FetchChunk rcFetchRead(RC_Fetch fetch);                                  // Read streamed bytes
RC_FetchMessages rcFetchMessages(RC_Fetch fetch, RC_FetchFraming framing);  // Read streamed messages
void rcFetchCancel(RC_Fetch fetch);                                         // Cancel request
void rcFetchFree(RC_Fetch fetch);                                           // Release request

// URLs
RC_String rcUrlEncode(RC_Arena *arena, const char *text);               // encodeURIComponent into arena
size_t rcUrlEncodeTo(char *buffer, size_t capacity, const char *text);  // encodeURIComponent into buffer
size_t rcUrlDecodeTo(char *buffer, size_t capacity, const char *text);  // decodeURIComponent into buffer
size_t rcUrlQueryGet(char *buffer, size_t capacity, const char *search, const char *name);  // URLSearchParams get into buffer
bool rcUrlParse(RC_Arena *arena, const char *url, RC_UrlParts *out);    // Parse URL, as new URL()

// WebSockets
RC_WebSocket rcWebSocket(const char *url, ...);                                 // Open socket
RC_WebSocketState rcWebSocketState(RC_WebSocket ws);                            // Socket state
bool rcWebSocketNext(RC_WebSocket ws, RC_WebSocketEvent *event);                // Take next event
bool rcWebSocketSend(RC_WebSocket ws, const char *text);                        // Send text
bool rcWebSocketSendText(RC_WebSocket ws, const char *text, size_t length);     // Send text of a length
bool rcWebSocketSendBinary(RC_WebSocket ws, const void *bytes, size_t length);  // Send binary message
bool rcWebSocketClose(RC_WebSocket ws, int code, const char *reason);           // Start closing
void rcWebSocketFree(RC_WebSocket ws);                                          // Release socket

// No window: your loop hosts the network
void rcFetchAttach(...);             // Host it in your loop
void rcFetchDetach(void);            // Stop hosting
int rcFetchUpdate(void);             // Apply what arrived
int rcFetchWait(double maxSeconds);  // Wait for progress
double rcFetchNextDeadline(void);    // Seconds until update
void rcFetchShutdown(void);          // Cancel and close everything
```

## module: media

```c
// Audio and video, as <video>
RC_Media rcMediaOpen(const char *src, ...);           // Open audio or video
RC_MediaInfo rcMediaRead(RC_Media media);             // Media state
void rcMediaPlay(RC_Media media);                     // Play
void rcMediaPause(RC_Media media);                    // Pause
void rcMediaSeek(RC_Media media, double seconds);     // Exact seek (Android: API 26+)
void rcMediaSetRate(RC_Media media, double rate);     // Set playback rate
void rcMediaSetVolume(RC_Media media, float volume);  // Set volume, 0-1
void rcMediaSetMuted(RC_Media media, bool muted);     // Mute or unmute
void rcMediaSetLoop(RC_Media media, bool loop);       // Loop or not
const RC_Image *rcMediaImage(RC_Media media);         // Video frame to draw
void rcMediaClose(RC_Media media);                    // Close media
void rcAudioSession(RC_AudioSession type);            // Set iOS audio session

// No window: your loop moves media
int rcMediaUpdate(void);                              // Apply player progress
double rcMediaNextDeadline(void);                     // Seconds until update
void rcMediaShutdown(void);                           // Close all media

// Sounds
RC_Sound rcLoadSound(const char *path);                         // Load sound file
RC_Sound rcLoadSoundFromMemory(const void *bytes, size_t len);  // Same, from memory
RC_Voice rcPlaySound(RC_Sound sound, float volume, ...);        // Play sound
RC_MediaInfo rcSoundRead(RC_Sound sound);                       // Sound state
void rcStopSound(RC_Sound sound);                               // Stop all its voices
void rcUnloadSound(RC_Sound sound);                             // Unload sound
RC_MediaInfo rcVoiceRead(RC_Voice voice);                       // Voice state
void rcVoiceStop(RC_Voice voice);                               // Stop one voice
void rcVoiceSetVolume(RC_Voice voice, float volume);            // Set voice volume
```

## module: json

```c
// Read, as response.json(); paths as in JS, "items[0].title"
RC_Json rcJsonParse(const char *text, size_t length);                         // Parse JSON; (size_t)-1 = strlen
void rcJsonFree(RC_Json root);                                                // Free document; max 4,095 live, 64 MiB total
const char *rcJsonError(RC_Json v);                                           // Why parsing failed
RC_JsonErrorKind rcJsonErrorKind(RC_Json v);                                  // Failure kind
RC_JsonType rcJsonType(RC_Json v);                                            // Value's type
RC_Json rcJsonGet(RC_Json v, const char *path);                               // Value at path
int rcJsonCount(RC_Json v);                                                   // Count elements or members
RC_Json rcJsonAt(RC_Json v, int i);                                           // Value at index
RC_String rcJsonKey(RC_Json v, int i);                                        // Member name at index
const char *rcJsonString(RC_Json v, const char *path, const char *fallback);  // String at path
RC_String rcJsonText(RC_Json v, const char *path);                            // Text with its length
double rcJsonNumber(RC_Json v, const char *path, double fallback);            // Number at path
int64_t rcJsonInt64(RC_Json v, const char *path, int64_t fallback);           // Exact integer at path
uint64_t rcJsonUint64(RC_Json v, const char *path, uint64_t fallback);        // Exact unsigned integer
bool rcJsonBool(RC_Json v, const char *path, bool fallback);                  // Bool at path

// Write, as JSON.stringify; key NULL at top level and in arrays
RC_JsonWriter rcJsonWriter(char *buffer, size_t capacity);                     // Write into your buffer
void rcJsonBeginObject(RC_JsonWriter *w, const char *key);                     // Open object
void rcJsonBeginArray(RC_JsonWriter *w, const char *key);                      // Open array
void rcJsonEnd(RC_JsonWriter *w);                                              // Close object or array
void rcJsonWriteString(RC_JsonWriter *w, const char *key, const char *value);  // Write string
void rcJsonWriteText(RC_JsonWriter *w, const char *key, RC_String value);      // Write text of a length
void rcJsonWriteNumber(RC_JsonWriter *w, const char *key, double value);       // Write number
void rcJsonWriteInt64(RC_JsonWriter *w, const char *key, int64_t value);       // Write 64-bit integer
void rcJsonWriteUint64(RC_JsonWriter *w, const char *key, uint64_t value);     // Write unsigned integer
void rcJsonWriteBool(RC_JsonWriter *w, const char *key, bool value);           // Write true or false
void rcJsonWriteNull(RC_JsonWriter *w, const char *key);                       // Write null
void rcJsonWriteValue(RC_JsonWriter *w, const char *key, RC_Json value);       // Write parsed value
const char *rcJsonWriterFinish(RC_JsonWriter *w);                              // Text, NULL on failure
```

## module: update

```c
RC_Update rcUpdateCheck(const char *url, ...);     // Check signed manifest
RC_UpdateResult rcUpdateResult(RC_Update update);  // Check state
bool rcUpdateApply(RC_Update update);              // Install, open or reload
bool rcUpdateRelaunch(RC_Update update);           // Start new build at exit
void rcUpdateCancel(RC_Update update);             // Cancel check or download
void rcUpdateFree(RC_Update update);               // Release check
```

## module: base

```c
// Memory and strings
RC_Arena rcArenaInit(size_t size);                                             // Create arena
void *rcArenaAlloc(RC_Arena *arena, size_t size);                              // Allocate
void rcArenaReset(RC_Arena *arena);                                            // Rewind, keeping the memory
void rcArenaFree(RC_Arena *arena);                                             // Destroy arena
RC_String rcFormat(RC_Arena *arena, const char *format, ...);                  // printf into arena
RC_String rcFormatV(RC_Arena *arena, const char *format, va_list args);        // vprintf into arena
RC_String rcFormatInt(RC_Arena *arena, int64_t value, const char *separator);  // Group digits: 1,234,567
size_t rcFormatIntTo(char *buffer, size_t capacity, int64_t value, const char *separator);  // Group digits into buffer
RC_String rcStringFromCStr(const char *cstr);                                  // Wrap C string, no copy
void rcStrCopy(char *dst, const char *src, size_t dstsize);                    // Copy string, always terminated
RC_PRINTF_FMT(fmt_pos, args_pos)                                               // Check printf-style helper's format

// Logs, time and crypto
void rcSetLogSink(RC_LogCallback sink, void *userData);                         // Send logs to callback
void rcLogWrite(RC_LogLevel level, const char *tag, const char *message);   // Write to platform log
double rcMonotonicSeconds(void);                                            // performance.now() in seconds
double rcUnixTimeSeconds(void);                                             // Seconds since 1970, UTC
bool rcLocalTime(double unixSeconds, RC_LocalTime *out);                    // Local date and time
bool rcTimeParse(const char *text, size_t length, double *unixSeconds);     // Parse RFC 3339 time
size_t rcTimeFormatUtc(char *buffer, size_t capacity, double unixSeconds);  // Format as toISOString()
bool rcRandomBytes(void *buffer, size_t length);                            // Secure random bytes
void rcSha256(const void *data, size_t length, uint8_t digest[32]);         // SHA-256 digest
void rcSha256Begin(RC_Sha256 *sha);                                         // Start streamed digest
void rcSha256Add(RC_Sha256 *sha, const void *data, size_t length);          // Add bytes
void rcSha256End(RC_Sha256 *sha, uint8_t digest[32]);                       // Finish it

// Process and machine
bool rcProcessMemoryBytes(size_t *current, size_t *peak);         // Resident memory and its peak
bool rcProcessCpuPercent(RC_CpuSampler *since, float *percent);   // CPU % of one core since the last call
bool rcMachineMemoryBytes(size_t *total, size_t *available);      // Machine RAM
bool rcMachineCpuPercent(RC_CpuSampler *since, float *percent);   // Machine CPU since sampler's last call
```

## structs

```c
// Values
RC_Color               // r g b a, floats 0-255
RC_Vec2                // x y
RC_Dimensions          // width height
RC_BoundingBox         // x y width height
RC_Box                 // x y width height found
RC_Insets              // top right bottom left
RC_KeyEvent            // key repeat shift ctrl alt super primary consumed
RC_String              // isStaticallyAllocated length chars, not NUL-terminated
RC_Size                // mode value, zero is unset
RC_OptFloat            // set value, written RC_VAL(x)
RC_Arena               // buffer bufferLength currOffset
RC_Image               // handle width height
RC_ImagePixels         // rgba width height
RC_Svg                 // Parsed SVG
RC_IconPoint           // x y, viewBox units
RC_CustomDrawData      // Opaque: custom-draw element's payload
RC_CpuSampler          // CPU reading's interval; zero it to start
RC_ElementDeclaration  // One element's config
RC_TextElementConfig   // One text run's config

// Handles
RC_App RC_Window                                            // App and window handles
RC_Fetch RC_WebSocket RC_Media RC_Sound RC_Voice RC_Update  // Handles, 0 is none
RC_ClipboardToken RC_FileDialogToken                        // Request tokens

// App and windows
RC_AppOptions            // width height title clearColor fontPath fontSizes fontCount fontOversample scratchArenaBytes debugToggleKey maxFrames startLayoutElements
                         // maxLayoutElements frame updateCallback layoutCallback frameEndCallback userData zoom titlebar autoCursorsDisabled minWidth minHeight
                         // renderWhileMinimized renderMode maxSeconds iconBytes iconLength iconPath orientation fontBytes fontLength statusBar
RC_WindowOptions         // title width height minWidth minHeight placement x y topmost focusPolicy taskbar owner frame titlebar clearColor mirror renderMode renderWhileMinimized
                         // autoCursorsDisabled debugToggleKey scratchArenaBytes startLayoutElements maxLayoutElements zoom updateCallback layoutCallback frameEndCallback
                         // stateCallback userData
RC_ZoomOptions           // disabled minZoom maxZoom ladder ladderCount step wheelStep bindZoomIn bindZoomOut bindZoomReset inDisabled outDisabled resetDisabled wheelDisabled mode pan bindPan
RC_TitlebarOptions       // className controlsLeft hideTitle hideMinimize hideMaximize height title minimize maximize close zoomWithContent allowControlsClip
RC_TitlebarButtonIcons   // normal hover press
RC_WindowControlOptions  // className
RC_Viewport              // width height safe breakpoint coarsePointer keyboard
RC_FrameCounts           // declared drawCommands
RC_SchedStats            // admitted waits deadlineWakes spurious requests refreshRepaints byReason
RC_PerfFrame             // RC_PERF_COUNTERS=1: declaredElements renderCommands rects borders shadows texts images vertices clipPushes
RC_ClipSlotCounts        // capacity stored attempted rejected highWaterMark
RC_ClipboardResult       // status text

// Elements and text
RC_ComponentOptions    // id focusable bg gap p pt pb pl pr px py align scroll overflow borderRadius disabled border noDrag captureWheel scrollStickToEnd captureTouch w h wType hType wMin
                       // wMax hMin hMax aspectRatio gradient shadow floating image imageFit imageTint overlay tooltip className scrollOffset opacity transition
RC_Border              // color width
RC_Gradient            // from to dir
RC_Shadow              // color x y blur spread
RC_Float               // to toId parent element offset zIndex capture clip
RC_TextOptions         // className font color size lineHeight letterSpacing wrap textAlign select lineClamp wordBreak
RC_Style               // background surface surfaceAlt chrome text textMuted primary primaryHover danger dangerHover success successHover warning warningHover border radius
RC_ScrollbarOptions    // className
RC_ScrollInfo          // offsetX offsetY maxOffsetX maxOffsetY found
RC_VirtualRow          // index
RC_VirtualListOptions  // overscan

// Widgets
RC_CheckboxOptions RC_ToggleOptions RC_RadioOptions  // disabled className
RC_ButtonOptions                                     // disabled className variant select leadingIcon
RC_SliderOptions                                     // disabled className thumbClassName fillClassName step
RC_ComboOptions                                      // disabled className popupClassName itemClassName placeholder
RC_TextInputOptions                                  // disabled className placeholder rows leadingIcon inputKind readOnly clearButton
RC_FieldSelection                                    // anchor caret found
RC_ProgressOptions                                   // className
RC_MenuOptions                                       // disabled className popupClassName caret variant leadingIcon
RC_ContextMenuOptions                                // className
RC_MenuItemOptions                                   // disabled className leadingIcon
RC_ModalOptions                                      // className noBackdropDismiss modality placement offset exiting
RC_TableColumn                                       // header wType align w id priority wMin
RC_TableOptions                                      // className headerClassName cellPadding
RC_TableRowOptions                                   // id className focusable
RC_SplitOptions                                      // className minFraction maxFraction handleThickness
RC_Series                                            // y x count kind color label thickness axis points colors
RC_Axis                                              // min max label ticks grid hide decimals unit labels format formatUserData readoutFormat
RC_ChartOptions                                      // className x y y2 legend tooltip tooltipPlace tooltipAnchor tooltipOffsetX tooltipOffsetY barWidth locale
RC_SparklineOptions                                  // className kind color min max thickness

// Files, network, media, updates
RC_PathInfo          // size modifiedUnixSeconds isDirectory
RC_FileDialogResult  // status count paths folder
RC_FetchOptions      // method headers json body bodyLength timeoutSeconds maxBytes redirect stream idleSeconds reconnect maxUnreadBytes maxMessageBytes headersTimeoutSeconds
                     // headerPairs
RC_FetchHeaderPair   // name value
RC_FetchResult       // state status ok text length error message received
RC_FetchChunk        // data length done
RC_FetchMessage      // data length event id flags
RC_FetchMessages     // items count framing done cut
RC_FetchHost         // wakeCallback userData javaVm
RC_UrlParts          // href origin protocol host hostname pathname search port
RC_WebSocketOptions  // protocols headers headerPairs connectTimeoutSeconds maxMessageBytes maxUnreadBytes maxBufferedBytes
RC_WebSocketState    // readyState protocol bufferedAmount status error message closeCode closeReason wasClean
RC_WebSocketEvent    // type binary data length code reason wasClean
RC_MediaOptions      // alt autoplay loop muted
RC_MediaInfo         // state error message time duration width height waiting seeking muted loop volume rate
RC_SoundOptions      // loop, for rcPlaySound
RC_UpdateOptions     // version build publicKey app channel platform
RC_UpdateResult      // state action version build notes url required received size error message
RC_Json              // JSON value; fields are RayClay's
RC_JsonWriter        // Writer state; read len, needed
RC_Sha256            // Streamed digest; fields are RayClay's
RC_LocalTime         // year month day hour minute second weekday yearDay utcOffsetSeconds daylightSaving

// Callbacks
typedef void (*RC_UpdateCallback)(RC_App *app, void *userData);                  // Each frame, before layout
typedef void (*RC_LayoutCallback)(RC_App *app, void *userData);                  // Each frame, declares UI
typedef void (*RC_FrameEndCallback)(RC_App *app, void *userData);                // After a drawn frame
typedef void (*RC_WindowUpdateCallback)(RC_Window *window, void *userData);      // Update, extra window
typedef void (*RC_WindowLayoutCallback)(RC_Window *window, void *userData);      // Layout, extra window
typedef void (*RC_WindowFrameEndCallback)(RC_Window *window, void *userData);    // Frame end, extra window
typedef void (*RC_WindowStateCallback)(RC_Window *window, RC_WindowState state, void *userData);  // Window state change
typedef void (*RC_AppEventCallback)(RC_AppEvent event, void *userData);              // Lifecycle event
typedef void (*RC_LogCallback)(RC_LogLevel level, const char *msg, void *userData);  // Receive log message
typedef void (*RC_CustomDrawCallback)(RC_BoundingBox bounds, RC_Color color, const void *userData);  // Custom drawing
typedef void (*RC_IconCallback)(float size, RC_Color color);                     // Draw one glyph
typedef void (*RC_FetchWakeCallback)(void *userData);                                // Wake your loop, any thread
typedef bool (*RC_DirectoryEntryCallback)(const char *name, bool isDirectory, void *userData);  // One folder entry, false stops
```

## enums and constants

```c
// Layout and styling
RC_SizeMode           // RC_SIZE_UNSET/FIT/GROW/FIXED/PERCENT/VW/VH
RC_FontWeight         // RC_WEIGHT_THIN/EXTRALIGHT/LIGHT/REGULAR/MEDIUM/SEMIBOLD/BOLD/EXTRABOLD/BLACK
RC_ButtonVariant      // RC_BTN_DEFAULT/PRIMARY/DANGER/GHOST
RC_ModalPlacement     // RC_MODAL_CENTER/TOP/BOTTOM
RC_SelectMode         // RC_SELECT_AUTO/TEXT/NONE/ALL
RC_ImageFit           // RC_IMAGE_FILL/COVER/CONTAIN/SCALE_DOWN
RC_PixelFormat        // RC_PIXEL_RGBA8/BGRA8/RGBX8/BGRX8
RC_AttachTo           // RC_ATTACH_NONE/PARENT/ELEMENT/ROOT
RC_Anchor             // RC_ANCHOR_TOP_LEFT/TOP_CENTER/TOP_RIGHT/CENTER_LEFT/CENTER/CENTER_RIGHT/BOTTOM_LEFT/BOTTOM_CENTER/BOTTOM_RIGHT
RC_Capture            // RC_CAPTURE_DEFAULT/ON/PASSTHROUGH
RC_TouchAxis          // RC_TOUCH_AXIS_X/Y/BOTH
RC_FloatClip          // RC_CLIP_DEFAULT/TO_PARENT/NONE
RC_ScrollAlign        // RC_SCROLL_NEAREST/START/CENTER/END
RC_SplitAxis          // RC_SPLIT_ROW/COLUMN
RC_InputKind          // RC_INPUT_TEXT/EMAIL/NUMBER/URL/TEL/USERNAME/PASSWORD/SHOWN_PASSWORD/ONE_TIME_CODE/DECIMAL
RC_Modality           // RC_MODALITY_MODAL/NON_MODAL
RC_SeriesKind         // RC_SERIES_LINE/BAR/AREA/SCATTER
RC_ChartTooltipPlace  // RC_CHART_TOOLTIP_PLACE_POINT/CURSOR/CORNER/FIXED
RC_Breakpoint         // RC_BP_BASE/SM/MD/LG/XL/2XL

// Windows, input and lifecycle
RC_RenderMode         // RC_RENDER_ON_DEMAND/CONTINUOUS
RC_ZoomMode           // RC_ZOOM_LAYOUT/OPTICAL
RC_Mirror             // RC_MIRROR_NONE/HORIZONTAL/VERTICAL/BOTH
RC_WindowState        // RC_WINDOW_PENDING/READY/FAILED/CLOSING/CLOSED
RC_WindowError        // RC_WINDOW_ERROR_NONE/INVALID_ARGUMENT/OUT_OF_MEMORY/HOST/GRAPHICS/LAYOUT
RC_WindowPlacement    // RC_WINDOW_PLACE_DEFAULT/AT
RC_WindowFocusPolicy  // RC_WINDOW_FOCUS_DEFAULT/NONE
RC_WindowTaskbar      // RC_WINDOW_TASKBAR_DEFAULT/SKIP
RC_WindowControl      // RC_WINCTL_MINIMIZE/MAXIMIZE/CLOSE
RC_WindowFrame        // RC_WINDOW_FRAME_OS/TITLEBAR/NONE
RC_FrameReason        // RC_FRAME_INITIAL/INPUT/WINDOW/EXPOSE/RESOURCE/APP/DEADLINE/INTERNAL/REASON_COUNT
RC_Cursor             // RC_CURSOR_DEFAULT/POINTER/TEXT/GRAB/GRABBING/NOT_ALLOWED/RESIZE_EW/RESIZE_NS/CROSSHAIR
RC_PointerButton      // RC_POINTER_LEFT/RIGHT/MIDDLE
RC_Mod                // RC_MOD_PRIMARY/SHIFT/ALT/CTRL/SUPER
RC_Key                // RC_KEY_NONE/A..Z/0..9/F1..F24/LEFT/RIGHT/UP/DOWN/HOME/END/PAGE_UP/PAGE_DOWN/INSERT/DELETE/BACKSPACE/TAB/ENTER/ESCAPE/SPACE/LEFT_SHIFT/RIGHT_SHIFT/LEFT_CTRL
                      // RIGHT_CTRL/LEFT_ALT/RIGHT_ALT/LEFT_SUPER/RIGHT_SUPER/CAPS_LOCK/NUM_LOCK/SCROLL_LOCK/PRINT_SCREEN/PAUSE/MENU/APOSTROPHE/COMMA/MINUS/PERIOD/SLASH/SEMICOLON
                      // EQUAL/LEFT_BRACKET/BACKSLASH/RIGHT_BRACKET/GRAVE/KP_0..9/KP_DECIMAL/KP_DIVIDE/KP_MULTIPLY/KP_SUBTRACT/KP_ADD/KP_ENTER/KP_EQUAL/COUNT
RC_AppEvent           // RC_APP_EVENT_SUSPENDED/RESUMED/CONTEXT_LOST/CONTENT_SCALE_CHANGED/TERMINATING/COLOR_SCHEME_CHANGED/CLOSE_REQUESTED
RC_ColorScheme        // RC_COLOR_SCHEME_NO_PREFERENCE/LIGHT/DARK
RC_StatusBar          // RC_STATUS_BAR_AUTO/LIGHT_ICONS/DARK_ICONS/HIDDEN
RC_Orientation        // RC_ORIENTATION_ANY/PORTRAIT/LANDSCAPE
RC_LogLevel           // RC_LOG_INFO/WARNING/ERROR

// Files, network, media, data
RC_FileStatus           // RC_FILE_OK/INVALID/NOT_FOUND/WRONG_TYPE/NOT_EMPTY/DENIED/BUSY/FULL/IO/UNSUPPORTED
RC_FileDialogKind       // RC_FILE_DIALOG_FILE/FILES/FOLDER
RC_FileDialogStatus     // RC_FILE_DIALOG_NONE/PENDING/DONE/CANCELED/REFUSED/FAILED/UNSUPPORTED
RC_SaveStatus           // RC_SAVE_NONE/PENDING/STORED/REFUSED
RC_ClipboardStatus      // RC_CLIPBOARD_NONE/PENDING/DONE
RC_FetchState           // RC_FETCH_NONE/PENDING/DONE/FAILED/CANCELED/OPEN
RC_FetchError           // RC_FETCH_ERROR_NONE/NETWORK/TIMEOUT/SECURITY/TOO_LARGE/INVALID/UNAVAILABLE
RC_FetchRedirect        // RC_FETCH_REDIRECT_FOLLOW/ERROR
RC_FetchFraming         // RC_FETCH_FRAMING_AUTO/SSE/LINES/ENVELOPES/WHOLE
RC_WebSocketReadyState  // RC_WEBSOCKET_NONE/CONNECTING/OPEN/CLOSING/CLOSED
RC_WebSocketError       // RC_WEBSOCKET_ERROR_NONE/NETWORK/TIMEOUT/SECURITY/REFUSED/PROTOCOL/TOO_LARGE/INVALID/UNAVAILABLE
RC_WebSocketEventType   // RC_WEBSOCKET_EVENT_NONE/OPEN/MESSAGE/ERROR/CLOSE
RC_MediaState           // RC_MEDIA_NONE/LOADING/READY/PLAYING/PAUSED/ENDED/FAILED
RC_MediaError           // RC_MEDIA_ERROR_NONE/UNAVAILABLE/UNSUPPORTED/NOT_FOUND/DECODE/NETWORK/BLOCKED
RC_AudioSession         // RC_AUDIO_SESSION_AUTO/PLAYBACK/AMBIENT/APP
RC_UpdateState          // RC_UPDATE_NONE/CHECKING/CURRENT/AVAILABLE/DOWNLOADING/READY/FAILED/CANCELED
RC_UpdateAction         // RC_UPDATE_ACTION_NONE/INSTALL/OPEN/RELOAD
RC_UpdateError          // RC_UPDATE_ERROR_NONE/NETWORK/MANIFEST/SIGNATURE/EXPIRED/HASH/UNSUPPORTED/IO/UNAVAILABLE/INVALID
RC_JsonType             // RC_JSON_MISSING/INVALID/NULL/BOOL/NUMBER/STRING/ARRAY/OBJECT
RC_JsonErrorKind        // RC_JSON_ERROR_NONE/SYNTAX/LIMIT/NO_TEXT/DISABLED

// File flags and tokens
RC_FILE_PRIVATE RC_FILE_PARENTS RC_FILE_RECURSIVE RC_FILE_RELAXED  // Owner only, mkdir -p, rm -r, power-cut unsafe
RC_FILE_DIALOG_OPENED_BY_OS                                        // Deep links, Open With; Windows, Linux: argv

// Titlebar element ids
RC_ID_WINDOW_DRAG                                               // Press drags window
RC_ID_WINDOW_MINIMIZE RC_ID_WINDOW_MAXIMIZE RC_ID_WINDOW_CLOSE  // Window control ids

// Limits
RC_CHART_MAX_SERIES                    // Series per chart: 16
RC_SVG_CACHE_MAX                       // rcSvg paths held: 1,024
RC_ICON_MAX_PTS                        // Fill points ear-clipped once: 256
RC_CLASS_DEFINE_MAX RC_CLASS_NAME_MAX  // Classes: 32, name bytes: 24
RC_CLASS_CACHE_MAX RC_CLASS_KEY_MAX    // Class cache: 44, key bytes: 96
RC_CLASS_NEST_MAX                      // Class nesting depth: 4
RC_FONT_LAST_CODEPOINT                 // Last codepoint of direct glyph table: 255

// Version
RC_VERSION RC_VERSION_SHORT                         // "MAJOR.MINOR.PATCH", "MAJOR.MINOR"
RC_VERSION_MAJOR RC_VERSION_MINOR RC_VERSION_PATCH  // Version numbers
RC_VERSION_COMMIT                                   // Reserved; ""
```

## colors

```c
// Tailwind palette: RC_<FAMILY>_<shade>, shades 50 100 200 ... 900 950
RC_BLACK RC_WHITE RC_TRANSPARENT                                           // Named colors
RC_UNSET_COLOR                                                             // Unset color
RC_SLATE_* RC_GRAY_* RC_ZINC_* RC_NEUTRAL_* RC_STONE_*                     // Grays
RC_MAUVE_* RC_OLIVE_* RC_MIST_* RC_TAUPE_*                                 // Tinted grays
RC_RED_* RC_ORANGE_* RC_AMBER_* RC_YELLOW_* RC_LIME_* RC_GREEN_*
RC_EMERALD_* RC_TEAL_* RC_CYAN_* RC_SKY_* RC_BLUE_* RC_INDIGO_*
RC_VIOLET_* RC_PURPLE_* RC_FUCHSIA_* RC_PINK_* RC_ROSE_*
```

## build options

```text
// NOTE: set a knob on the library and your code alike
RC_NO_UI_HELPERS RC_NO_STYLE RC_NO_COLOR_PALETTE             // Your code only: hide elements and widgets, theme, palette
RC_NO_IMAGE RC_NO_WEBP RC_NO_SVG RC_NO_FILE_DIALOG           // Drop images, WebP, SVG, dialogs
RC_NO_NET RC_NO_JSON RC_NO_UPDATE RC_NO_MEDIA                // Drop module, keep stubs
RC_NO_LINE_BREAK                                             // Break at spaces, newlines only
RC_NO_BUNDLED_FONT RC_NO_COLOR_EMOJI                         // Drop bundled fonts, emoji
RC_NO_DEFAULT_APP_ICON RC_NO_LIVE_RESIZE                     // Drop app icon, resize repaint
RC_SOUND                                                     // 0 compiles sounds out
RC_DEBUG_TOOLS                                               // 1 adds layout inspector
RC_PERF_COUNTERS                                             // 1 adds rcWindowPerfFrame
RC_GFX_MSAA_SAMPLES                                          // Antialias samples: 1, 2 or 4 (default)
RC_FONT_ATLAS_W RC_FONT_ATLAS_H                              // Glyph atlas px, power of two: 2048
RC_GFX_BACKEND_GL RC_GFX_BACKEND_D3D11 RC_GFX_BACKEND_METAL  // Single header: pick renderer
RAYCLAY_IMPLEMENTATION                                       // Single header: one C11/.m file
RAYCLAY_NO_GUI                                               // Single header: base, JSON, network; no rcStringFromCStr, process, machine
-DRC_WINDOW_BACKEND=GLFW|SDL3                                // CMake: desktop window host
-DRC_GFX_BACKEND=gl|d3d11|metal                              // CMake: renderer
-DRC_SIZE_OPT=ON                                             // CMake: smallest binary
-DRC_NO_NET=ON -DRC_NO_WEBP=ON -DRC_SOUND=OFF                // CMake: drop networking, WebP, sounds
```
