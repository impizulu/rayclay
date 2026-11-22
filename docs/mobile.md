# Mobile: Android and iOS

## Project setup

**Android.** Copy `third_parties/SDL3/android-project`, then RayClay's `mobile/android` over it
once: its manifest is SDL's with `org.rayclay.RCActivity` as the activity and
`android:glEsVersion="0x00030000"`, its `mipmap-*` folders are the RayClay launcher icon (put
yours there), and `RCActivity.java` and `RayClayMedia.java` land in `java/org/rayclay/`, beside
SDL's Java in `java/org/libsdl/app/`; an app with its own manifest copies only `app/src/main/res`
and `app/src/main/java`. `app/jni/CMakeLists.txt` becomes yours:
`add_subdirectory(<RayClay> rayclay)`, then `add_library(main SHARED ...)` linked to `rayclay`.
Recopy both Java folders at each RayClay update (a mismatched `SDLActivity.java` stops the app at
launch), or have `app/build.gradle` copy them each build (then delete the project's
`java/org/libsdl` and `java/org/rayclay`):

```groovy
def rayclayDir = rootProject.file("vendor/rayclay")   // where you unpacked RayClay
def rayclayJava = layout.buildDirectory.dir("generated/rayclay/java")
tasks.register("copyRayClayJava", Sync) {
    from("$rayclayDir/mobile/android/app/src/main/java")
    from("$rayclayDir/third_parties/SDL3/android-project/app/src/main/java/org/libsdl/app") {
        include("*.java"); into("org/libsdl/app")
    }
    into(rayclayJava)
}
android.sourceSets.main.java.srcDir(rayclayJava)
tasks.named("preBuild") { dependsOn("copyRayClayJava") }
```

Build with JDK 17 or 21: `sh gradlew -PBUILD_WITH_CMAKE assembleDebug`. Under R8, add
`third_parties/SDL3/android-project/app/proguard-rules.pro` and keep both `org.rayclay` classes
whole. A relative path also reads the APK's `assets/` (`rcMediaOpen` needs it uncompressed).
`adb logcat --pid=$(adb shell pidof -s <package>)` shows the app's log. To set a `RAYCLAY_*`
variable, an `RCActivity` subclass calls `android.system.Os.setenv` before `super.onCreate`.

**iOS.** Configure (CMake 3.24+) with `cmake -G Xcode -DCMAKE_SYSTEM_NAME=iOS`
`-DCMAKE_OSX_DEPLOYMENT_TARGET=17.0`, build `add_executable(myapp MACOSX_BUNDLE ...)`, set its
`MACOSX_BUNDLE_GUI_IDENTIFIER`, `XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER` (same id),
`MACOSX_BUNDLE_BUNDLE_NAME`, `MACOSX_BUNDLE_BUNDLE_VERSION` and
`MACOSX_BUNDLE_SHORT_VERSION_STRING`, call `rc_ios_app(myapp)` and sign in Xcode or, with the
team's account there, pass `-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE=Automatic`
`-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=<team>` too, then
`cmake --build <build> -- -allowProvisioningUpdates`. `rc_ios_app` fills
`mobile/ios/Info.plist.in` and adds RayClay's icon; off iOS it does nothing.
`rc_ios_app(myapp ICONS <dir>)` packages PNGs named like the template's. A target's own
`MACOSX_BUNDLE_INFO_PLIST` stays: start it from the template, whose `UILaunchScreen` stops
letterboxing. App Store builds add `PrivacyInfo.xcprivacy` declaring the file timestamp and system
boot time API categories. Relative paths read the bundle. On an iPhone 15, Metal, the default, uses
under half the in-use memory of OpenGL ES 3 (`-DRC_GFX_BACKEND=gl`).

**Both.** Phones take the launcher icon from the app package, never `.iconBytes`. SDL calls
a phone's `main(int argc, char *argv[])`; include `<SDL3/SDL_main.h>`:

<!-- compile: c -->
```c
#include "rayclay.h"
#include <math.h>
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif
#if defined(__ANDROID__) || (defined(__APPLE__) && TARGET_OS_IPHONE)
#include <SDL3/SDL_main.h>
#endif

static void layout(RC_App *app, void *user) {
    RC_Viewport v = rcViewport();

    (void)app; (void)user;
    rcColumn(.id = "root", .pt = ceilf(v.safe.top), .pb = ceilf(v.safe.bottom),
             .pl = ceilf(v.safe.left), .pr = ceilf(v.safe.right), .w = "grow", .h = "grow") {
        rcTextL("My app");
    }
}

int main(int argc, char *argv[]) {
    RC_AppOptions opts = { .layoutCallback = layout };

    (void)argc; (void)argv;
    return rcRunApp(&opts);
}
```

## Screen and lifecycle

- **Orientation.** `.orientation` applies once the window opens, so also declare a lock where the
  phone reads it at launch, or the app starts as held and turns: Android's
  `android:screenOrientation="portrait"` or `"userLandscape"`; iOS's
  `rc_ios_app(myapp ORIENTATION portrait)` or `landscape`.
- **Safe area.** Spend `rcViewport().safe` once, at the root; a scrolling root spends the bottom
  inset at its content's end, like `env(safe-area-inset-bottom)`.
- **Status bar.** Its icons, and Android's navigation bar's, follow the colors under them; over a
  picture, choose them with `.statusBar` or `rcSetStatusBar()`. Content scrolled under the clock
  shows: float a page-colored box `rcViewport().safe.top` tall on the root. On iOS leave
  `UIViewControllerBasedStatusBarAppearance` out of Info.plist.
- **Keyboard.** A focused field in a `.scroll = "v"` container scrolls clear of the keyboard;
  elsewhere the view pans to the caret until the field blurs, unless the root pads by
  `rcViewport().keyboard.bottom`. Leave the manifest's soft input mode unset: `RCActivity` sets
  `adjustResize`. An input method's text shows on iOS once committed, on Android as typed text.
- **Finger sizes.** Under a coarse pointer, buttons, menu items, combos and their rows, fields and
  the rows of checkboxes, radios and toggles are at least 44 px tall. A `min-h-*` in the control's
  `.className` (`.itemClassName` for a combo's rows) replaces that floor; an `h-*` is exact. On a
  checkbox, radio or toggle `min-h-*` sizes the row and `h-*` or `size-*` the box.
- **Drags.** A finger dragging in a `.scroll` box pans it, canceling the press; a handle you drag
  yourself keeps the finger with `.captureTouch = RC_TOUCH_AXIS_X` (`_Y`, `_BOTH`).
- **Back.** Android's Back arrives as `RC_KEY_ESCAPE`: it closes an open dialog, combo or menu or
  blurs a focused field, else goes to an app reading Escape that frame, else backgrounds the app.
  Read Escape only where Back does something; `rcKeyPressedOn(id, RC_KEY_ESCAPE)` owns Back only
  while that stop holds focus.
- **Background.** Save on `RC_APP_EVENT_SUSPENDED` (`rcSetAppEventHandler`):
  `RC_APP_EVENT_TERMINATING` is not guaranteed.
- **Storage.** `rcAppDataPath` is backed up (iCloud, Android's Auto Backup); keep re-downloadable
  media in `rcAppCachePath`, which neither backs up and either may empty.
- **Sound on iOS.** The audio session is Playback while audible media plays, else Ambient; to set
  your own category, call `rcAudioSession(RC_AUDIO_SESSION_APP)` before the first `rcMediaPlay`.
- **Deep links.** Declare the scheme (an intent filter on the activity, `CFBundleURLTypes` on iOS)
  and poll `rcFileDialogPoll(RC_FILE_DIALOG_OPENED_BY_OS)` each frame: its `paths` hold the URL as
  the system gave it, at launch or later (on iOS, a file URL's path).
- **Networking.** On Android declare `android.permission.INTERNET` and build `arm64-v8a` alone (NDK
  r28+, `minSdk` 21+). Plain `http://` fails `SECURITY` unless Android's network security config or
  iOS's App Transport Security allows its host; that config also trusts a named test CA. On iOS a
  private address or `.local` host needs the Local Network permission
  (`NSLocalNetworkUsageDescription`), or its requests fail.

## Platform code beside RayClay

Put platform code (a share sheet, a store) in a shim of plain C calls; its callback stores the
answer, then calls `rcAppWake()`.

<!-- shim: usage.c -->
<!-- compile: c -->
```c
bool app_share_text(const char *utf8);   /* share.h: true when the sheet was asked for */
int app_share_take_result(void);         /* -1 nothing new; 0 closed; 1 shared (iOS reports it) */

static const char *share_status = "";

static void update(RC_App *app, void *user) {
    int r = app_share_take_result();      /* each answer once, on the main thread */

    (void)app; (void)user;
    if (r >= 0)
        share_status = r == 1 ? "Shared" : "Closed";
}

static void layout(RC_App *app, void *user) {
    (void)app; (void)user;
    rcColumn(.id = "root", .className = "gap-2 p-3") {
        if (rcButton("share", "Share", .variant = RC_BTN_PRIMARY)
            && !app_share_text("Made with RayClay"))
            share_status = "Sharing is not available here";
        rcTextC(share_status);            /* static strings: they outlive the frame */
    }
}
```

<!-- shim: share.c -->
<!-- compile: c -->
```c
/* share.c - a system share sheet beside RayClay. Every platform builds this file; iOS adds
   share_ios.m. The shared code calls only these two. */
#include "rayclay.h"
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

bool app_share_text(const char *utf8);
int app_share_take_result(void);

#if defined(__ANDROID__)
#include <jni.h>
#include <string.h>
#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_system.h>

static SDL_AtomicInt share_busy;     /* 1 from a request until its answer */
static SDL_AtomicInt share_result;   /* 0 nothing new; 1 closed; 2 shared */

bool app_share_text(const char *utf8) {
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();      /* this thread's: never keep it */
    jobject activity = NULL;
    jclass cls = NULL;
    jbyteArray bytes = NULL;
    jmethodID share = NULL;
    bool asked = false;

    if (!env || !utf8 || !SDL_CompareAndSwapAtomicInt(&share_busy, 0, 1))
        return false;                                     /* one sheet at a time */
    activity = (jobject)SDL_GetAndroidActivity();
    if (activity)
        cls = (*env)->GetObjectClass(env, activity);
    if (cls)
        share = (*env)->GetMethodID(env, cls, "shareText", "([B)Z");
    if (share)
        bytes = (*env)->NewByteArray(env, (jsize)strlen(utf8));
    if (bytes) {
        (*env)->SetByteArrayRegion(env, bytes, 0, (jsize)strlen(utf8), (const jbyte *)utf8);
        if (!(*env)->ExceptionCheck(env))
            asked = (*env)->CallBooleanMethod(env, activity, share, bytes) == JNI_TRUE;
    }
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        asked = false;
    }
    if (bytes)
        (*env)->DeleteLocalRef(env, bytes);
    if (cls)
        (*env)->DeleteLocalRef(env, cls);
    if (activity)
        (*env)->DeleteLocalRef(env, activity);
    if (!asked)
        SDL_SetAtomicInt(&share_busy, 0);
    return asked;
}

/* MyActivity.nativeShareDone, on Android's UI thread (rename it for your package). */
JNIEXPORT void JNICALL Java_com_example_myapp_MyActivity_nativeShareDone(JNIEnv *env, jclass cls,
                                                                         jboolean shared) {
    (void)env; (void)cls;
    SDL_SetAtomicInt(&share_result, shared ? 2 : 1);      /* the answer first, */
    SDL_SetAtomicInt(&share_busy, 0);
    rcAppWake();                                          /* then the wake */
}

int app_share_take_result(void) {
    return SDL_SetAtomicInt(&share_result, 0) - 1;
}
#elif defined(__APPLE__) && TARGET_OS_IPHONE
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

void *app_uikit_window(void);

/* RayClay's UIWindow, or NULL: on the main thread, while its window is open. */
void *app_uikit_window(void) {
    SDL_Window **windows;
    void *uiwindow = NULL;
    int count = 0;

    if (!SDL_IsMainThread())
        return NULL;
    windows = SDL_GetWindows(&count);
    if (!windows)
        return NULL;
    if (count == 1) {                                     /* none, or several: never guess */
        SDL_PropertiesID props = SDL_GetWindowProperties(windows[0]);
        if (props)
            uiwindow = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
    }
    SDL_free(windows);
    return uiwindow;                                      /* borrowed: never release it */
}
#else
bool app_share_text(const char *utf8) {                   /* no sheet here: refuse */
    (void)utf8;
    return false;
}

int app_share_take_result(void) {
    return -1;
}
#endif
```

The manifest names this subclass, not `RCActivity`
(`<activity android:name="com.example.myapp.MyActivity" ...>`):

<!-- shim: MyActivity.java -->
```java
package com.example.myapp;

import android.content.ActivityNotFoundException;
import android.content.Intent;

import org.rayclay.RCActivity;

import java.nio.charset.StandardCharsets;

public class MyActivity extends RCActivity {
    /* SDL counts its request codes up from 0; RCActivity takes 0x5200 to 0x52FF. */
    private static final int SHARE_REQUEST = 0x7300;

    /* Read and written on the UI thread only: a chooser is up; the activity is resumed. */
    private boolean sharing;
    private boolean resumed;

    private static native void nativeShareDone(boolean shared);

    /* share.c calls this on SDL's thread: hand the work to the UI thread and return. */
    public boolean shareText(byte[] utf8) {
        final String text = new String(utf8, StandardCharsets.UTF_8);
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (!resumed || isFinishing() || isDestroyed()) {   /* checked as it runs */
                    nativeShareDone(false);
                    return;
                }
                Intent send = new Intent(Intent.ACTION_SEND)
                        .setType("text/plain")
                        .putExtra(Intent.EXTRA_TEXT, text);
                try {
                    startActivityForResult(Intent.createChooser(send, null), SHARE_REQUEST);
                    sharing = true;
                } catch (ActivityNotFoundException e) {
                    nativeShareDone(false);
                }
            }
        });
        return true;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == SHARE_REQUEST && sharing) {
            sharing = false;
            nativeShareDone(false);   /* Android does not say whether the user shared */
        }
    }

    /* A result arrives before onResume, so a chooser unanswered here was left behind. */
    @Override
    protected void onResume() {
        super.onResume();
        resumed = true;
        if (sharing) {
            finishActivity(SHARE_REQUEST);
            sharing = false;
            nativeShareDone(false);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        resumed = false;
    }

    @Override
    protected void onDestroy() {
        if (sharing && isFinishing()) {
            finishActivity(SHARE_REQUEST);
            sharing = false;
            nativeShareDone(false);
        }
        super.onDestroy();
    }
}
```

<!-- shim: proguard-rules.pro -->
```
-keep class org.rayclay.RCActivity { *; }
-keep class org.rayclay.RayClayMedia { *; }
-keep class com.example.myapp.MyActivity {
    public boolean shareText(byte[]);
    private static native void nativeShareDone(boolean);
}
```

<!-- shim: share_ios.m -->
```objc
/* share_ios.m - the share sheet on iOS, built with ARC (-fobjc-arc). Only iOS builds this file. */
#import <UIKit/UIKit.h>
#include <SDL3/SDL_atomic.h>
#include "rayclay.h"

void *app_uikit_window(void);   /* share.c */
bool app_share_text(const char *utf8);
int app_share_take_result(void);

static SDL_AtomicInt share_result;          /* 0 nothing new; 1 closed; 2 shared */
static UIActivityViewController *share_up;  /* the sheet that is up; main thread only */

bool app_share_text(const char *utf8) {
    UIWindow *window;
    UIViewController *top;
    NSString *text;
    UIActivityViewController *sheet;
    __weak UIActivityViewController *asked;

    if (![NSThread isMainThread])
        return false;
    if (share_up && share_up.presentingViewController)
        return false;
    if (share_up) {                                                /* gone without an answer: */
        share_up = nil;                                            /* settle it as closed, */
        SDL_SetAtomicInt(&share_result, 1);
        rcAppWake();                                               /* then wake */
    }
    window = (__bridge UIWindow *)app_uikit_window();
    top = window.rootViewController;
    text = utf8 ? [NSString stringWithUTF8String:utf8] : nil;
    if (!top || !text)
        return false;
    if (UIApplication.sharedApplication.applicationState != UIApplicationStateActive ||
        (window.windowScene &&
         window.windowScene.activationState != UISceneActivationStateForegroundActive))
        return false;                                              /* not in front: ask later */
    while (top.presentedViewController)
        top = top.presentedViewController;
    if (!top.viewIfLoaded.window || top.isBeingPresented || top.isBeingDismissed)
        return false;
    sheet = [[UIActivityViewController alloc] initWithActivityItems:@[text]
                                              applicationActivities:nil];
    asked = sheet;
    sheet.completionWithItemsHandler = ^(UIActivityType type, BOOL completed,
                                         NSArray *items, NSError *error) {
        (void)type; (void)items; (void)error;
        if (!asked || share_up != asked)
            return;
        share_up = nil;
        SDL_SetAtomicInt(&share_result, completed ? 2 : 1);       /* the answer first, */
        rcAppWake();                                               /* then the wake */
    };
    sheet.popoverPresentationController.sourceView = top.view;    /* an iPad's popover */
    sheet.popoverPresentationController.sourceRect =
        CGRectMake(CGRectGetMidX(top.view.bounds), CGRectGetMidY(top.view.bounds), 0, 0);
    share_up = sheet;
    [top presentViewController:sheet animated:YES completion:nil];
    return true;
}

int app_share_take_result(void) {
    return SDL_SetAtomicInt(&share_result, 0) - 1;
}
```
