/*
 * RCActivity - RayClay's Android activity: SDL's own SDLActivity, which runs
 * the app, plus the file dialogs (rcFileDialogOpen, rcFileDialogSave), the
 * system bars (RC_AppOptions.statusBar, rcSetStatusBar) and the launch screen
 * held until RayClay's first frame.
 *
 * TO USE IT, add this file to the app beside SDL's Java, as
 * app/src/main/java/org/rayclay/RCActivity.java, and name it in
 * AndroidManifest.xml as the launchable activity in place of SDLActivity:
 *
 *     <activity android:name="org.rayclay.RCActivity" ...>
 *
 * Everything else is SDLActivity's, unchanged, but for the launch screen
 * (THE LAUNCH SCREEN below), the display's rotation (RCSurface, THE ROTATION),
 * the watch that stops sounds in the background, the URL the system opened
 * the app with, a soft input mode where the manifest names none and a keyboard
 * another app left up (RCSurface's insets). Without it every
 * rcFileDialogOpen and rcFileDialogSave is answered UNSUPPORTED, and RayClay's
 * log says why, once; and sounds (rcPlaySound) play on while the app is in
 * the background. Only RayClay's C side calls rcFilesShow and
 * rcFilesClose, so a build that shrinks code (R8 or ProGuard) must keep the
 * class whole:
 *
 *     -keep class org.rayclay.RCActivity { *; }
 *
 * WHAT IT DOES. RayClay's C side asks it, on SDL's thread, to start the
 * system's document picker (rcFilesShow): ACTION_OPEN_DOCUMENT to open,
 * ACTION_OPEN_DOCUMENT_TREE for a folder, ACTION_CREATE_DOCUMENT to save. It starts the picker from that thread, as
 * SDL's own picker does, so a device with no picker is answered UNSUPPORTED
 * at once. The result arrives in onActivityResult, on the UI thread. A cancel is answered
 * from there; a choice on a worker thread of its own: each opened document is
 * copied into the app's cache below rayclay-files/, because a content:// URI
 * is no path that fopen reads, and a save's bytes are written into the
 * created document by the C side. Every answer goes back through
 * nativeFilesAnswer. A folder is imported by the C side, on the worker,
 * through nativeFilesImport: it asks rcFilesList for each folder's children
 * and rcFilesOpen for each document it copies, and answers itself. The tree's
 * grant is the picker's, for this activity's life; no persistable permission
 * is ever taken. Names cross as UTF-8 bytes, never as Java strings, and
 * the C side creates each copy from those bytes, so a name with a character
 * past the BMP (an emoji) reaches the app as the file's own.
 *
 * ONE REQUEST AT A TIME, in static state, and one activity sees it through:
 * when Android re-creates this activity in a running process, SDL ends the
 * process instead (RayClay leaves SDL_HINT_ANDROID_ALLOW_RECREATE_ACTIVITY
 * off; SDLActivity.onCreate), and a request still up ends with it. A
 * start for a result lands in this activity's own task even with SDL's
 * launchMode, singleInstance (AOSP ActivityStarter: computeTargetTask), so
 * a launcher tap, which clears the task down to this activity, finishes the
 * picker, and that answers CANCELED. A result always arrives before
 * onResume, so a request still unanswered when the activity resumes was
 * left behind all the same: it is answered CANCELED and its picker
 * finished, so the one dialog at a time never locks the app out.
 *
 * The copies' folder is emptied once per process, at the first onCreate, so
 * copies a run never released go at the next start.
 *
 * THE STATUS BAR. rcStatusBar is called on SDL's thread and applies the style
 * on the UI thread: the bar shown or hidden, and its icons dark (the
 * light-status-bar appearance) or light. AUTO's icons follow what the app
 * draws under the bar: the C side reads that band each frame and calls again
 * with its own AUTO_LIGHT or AUTO_DARK when the answer changes. AUTO itself
 * arrives as the window opens, before a frame says what is there, and
 * whenever a picture or custom drawing under the bar hides the band; then the
 * icons follow the system's appearance, read again at a configuration change
 * (the activity takes uiMode changes itself and is not re-created). All of
 * that is edge to edge. Below Android 15, or for an app that targets less
 * than 35, the window is laid out below the bar, which shows the theme's
 * statusBarColor. An opaque one gives every style the icons that read on it
 * instead (darkIcons): an app's LIGHT_ICONS and DARK_ICONS name its band under
 * the bar, and no band is under it there. One that is not opaque (a theme may
 * name a transparent one) keeps the style, as edge to edge does.
 *
 * THE NAVIGATION BAR has a band of its own (the light-navigation-bar appearance,
 * API 26). Under every style (HIDDEN hides the status bar only) the C side reads
 * the band under the navigation bar as it reads the status bar's and calls
 * rcNavigationBar with its own AUTO_LIGHT or AUTO_DARK when that answer changes.
 * While a picture hides the band it calls it with an app's LIGHT_ICONS or
 * DARK_ICONS, or AUTO: then the system's appearance. Below Android 15 the
 * navigation bar, too, shows the theme's navigationBarColor (black unless the
 * app's theme says otherwise): an opaque one takes the icons that read on it,
 * as the status bar's does, so dark icons never land on a black bar, and one
 * that is not opaque keeps the style.
 * Those answers move the icons only: the status bar stays shown or hidden as
 * the last rcStatusBar left it, so one the user swiped into view under HIDDEN
 * stays until the app asks again. Edge to edge, onCreate clears the system's
 * forced light navigation appearance once, while the app's theme names no
 * navigation-bar color (clearForcedNavigationBar).
 *
 * THE LAUNCH SCREEN. The system takes it down when the activity's window
 * reports its first draw, and SDL's surface stays black until RayClay
 * presents its first frame. So that report waits until the C side calls
 * rcFirstFrame, after its first frame is presented (an empty one too), or
 * LAUNCH_CAP_MS pass (LaunchHold). From Android 8 (API 26) the window draws at
 * once and only its report waits: at that draw SDL's surface view asks
 * RCSurface for a redraw (SurfaceHolder.Callback2.surfaceRedrawNeededAsync),
 * and the system shows the window, launch screen gone, when RCSurface says the
 * redraw is done, which it says at the first frame. The window's own first
 * draw so runs while the C side starts instead of after its first present (a
 * Pixel 8a, Android 17: shown 4 ms after the first present, was 20). Below 26
 * the activity holds its draws themselves. From Android 14 the system waits 1 s
 * at most for that report (its SurfaceSyncGroup timeout, logged as "Failed to
 * receive transaction ready in 1000ms"), so a first frame later than that finds
 * the window already shown: the surface view cuts through the window only once
 * its redraw is done, so until the frame the window shows its theme's
 * background (the launch screen's color), not black, and the launch screen
 * goes at that second, not with the frame (a Pixel 8a, Android 17: shown at
 * 1.11 s for a first frame at 1.56 s).
 *
 * THE EARLY START. SDL starts its thread, and so the C side, when the surface
 * is ready; onPostCreate starts it instead, once the activity (a subclass too)
 * is created (SDLActivity.startSDLThreadEarly, entry 15 of the SDL3 tree's
 * RAYCLAY-PATCHES.txt), with the display's size standing for the surface's. SDL makes the
 * window and its GL context with no surface, so the C side's start (the
 * renderer's shaders, compiled cold after an install or a cleared cache, the
 * fonts, the app's first layout) runs while the UI thread lays the window out,
 * and its first frame is drawn when the surface comes.
 *
 * THE ROTATION. SDL's surface learns that the display turned from an
 * accelerometer listener, on the UI thread, while the activity is resumed:
 * at SENSOR_DELAY_GAME it wakes that thread 50-60 times a second for as long
 * as the app is in front, idle or not (a Pixel 8a, Android 17: 60 wakeups a
 * second, 2.7-2.9% of a core, nearly all an idle app spends). The readings
 * themselves go nowhere: SDL keeps them for Android_JNI_GetAccelerometerValues,
 * which nothing calls, and SDL_OpenSensor reads the sensors through the NDK on
 * its own. RCSurface takes SDL's surface without that listener: the same
 * rotation reaches SDL (onNativeRotationChanged, its display orientation)
 * from the display manager's change events instead, which come only when a
 * display changes, so an idle app's UI thread sleeps.
 */
package org.rayclay;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.database.Cursor;
import android.graphics.Color;
import android.hardware.Sensor;
import android.hardware.display.DisplayManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.webkit.MimeTypeMap;

import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;

import java.io.Closeable;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public class RCActivity extends SDLActivity {
    /* What the C side asks for (its RCI_FilesKind). */
    private static final int KIND_FILES = 1, KIND_FOLDER = 2, KIND_SAVE = 3;

    /* An rcFilesList row's kind (the C side's RCI_FILES_ENTRY_*). */
    private static final byte ENTRY_FILE = 1, ENTRY_FOLDER = 2, ENTRY_OTHER = 3;

    /* DocumentsContract.Document.FLAG_VIRTUAL_DOCUMENT (API 24): no bytes of its own. */
    private static final int FLAG_VIRTUAL = 1 << 9;

    /* rcFilesShow's answer. */
    private static final int SHOWN = 0, NO_PICKER = 1, BUSY = 2, NOT_SHOWN = 3;

    /* nativeFilesAnswer's `what`. */
    private static final int DONE = 0, CANCELED = 1, FAILED_READ = 2, FAILED_LIMIT = 3,
                             FAILED_EMPTY = 4, FAILED_WRITE = 5;

    /* The worker gave up for rcFilesClose and answers nothing; nativeFilesWrite's
       answer for the same. */
    private static final int STOPPED = -1;

    /* The copies' folder below the cache; the C side names the same one. */
    private static final String FOLDER = "rayclay-files";
    private static final String OLD = FOLDER + "-old-";

    /* Each request's own request code, these 256 in turn, so a late result
       from a picker left behind never answers a newer request. */
    private static final int REQUEST_CODES = 0x5200;

    /* The longest file name the cache's file system takes, in bytes. */
    private static final int NAME_MAX = 255;

    /* RC_StatusBar, as the C side's RC_STATUS_BAR_*, then AUTO's icons as the
       runner read them from the band under the bar (its RCI_STATUS_BAR_AUTO_LIGHT
       and _DARK); AUTO itself when it reads none. */
    private static final int BAR_AUTO = 0, BAR_LIGHT_ICONS = 1, BAR_DARK_ICONS = 2, BAR_HIDDEN = 3,
                             BAR_AUTO_LIGHT = 4, BAR_AUTO_DARK = 5;

    /* The style last asked for, for a configuration change; -1 = never asked. */
    private static volatile int statusBar = -1;

    /* The navigation bar's icons as the C side read them (BAR_AUTO_LIGHT,
       BAR_AUTO_DARK), an app's BAR_LIGHT_ICONS or BAR_DARK_ICONS where a picture
       hides the band, or BAR_AUTO for the system's. */
    private static volatile int navigationBar = BAR_AUTO;

    /* The longest the launch screen waits for RayClay's first frame, so an app
       that presents none (a long start, a surface that never comes) is shown
       late, never held for good. From Android 14 the system's own 1 s comes
       first for the held report (THE LAUNCH SCREEN); this cap still ends the
       pre-draw hold below Android 8 and releases what is held. */
    private static final long LAUNCH_CAP_MS = 4000;

    private static final String TAG = "RayClay";

    private static final Object lock = new Object();
    private static Request request;   /* from rcFilesShow to its answer, or rcFilesClose */
    private static int sequence;
    private static boolean emptied;

    private static final class Request {
        final int token, kind, code, maxFiles;
        final long maxBytes;
        final String dir;       /* an open's folder for copies, ending in '/'; null for a save */
        boolean answered;       /* the picker gave its result (under lock) */
        boolean importing;      /* a folder's import runs: it answers itself (under lock) */
        volatile boolean stop;  /* rcFilesClose: give up, delete the copies, answer nothing */
        final CancellationSignal signal = new CancellationSignal();   /* a folder's IO */
        Uri tree;                 /* a folder: the granted tree, its root document, */
        String rootId;            /* and the resolver its import reads through */
        ContentResolver resolver;

        Request(int token, int kind, int code, int maxFiles, long maxBytes, String dir) {
            this.token = token;
            this.kind = kind;
            this.code = code;
            this.maxFiles = maxFiles;
            this.maxBytes = maxBytes;
            this.dir = dir;
        }
    }

    /* The C side registers these at its first request. */
    private static native boolean nativeFilesAnswer(int token, int what, byte[][] paths);
    private static native int nativeFilesCreate(byte[] path);
    private static native int nativeFilesWrite(int token, int fd);
    private static native void nativeFilesImport(int token, Object request, byte[] name);

    /* RayClay's sounds: their output stops while the
       app is in the background, every voice held where it is, and starts again
       when it comes back. The C side registers nativeSoundHidden and calls
       soundWatch at its first play, so an app that plays no sound makes no
       call; soundWatch reports where the app is then, under the lock onStart
       and onStop take, so a first play while hidden is held as well. */
    private static final Object soundLock = new Object();
    private static boolean soundWatched, soundHidden;
    private static native void nativeSoundHidden(boolean hidden);

    private static void soundWatch() {
        synchronized (soundLock) {
            soundWatched = true;
            nativeSoundHidden(soundHidden);
        }
    }

    private static void soundShown(boolean shown) {
        synchronized (soundLock) {
            soundHidden = !shown;
            if (soundWatched)
                nativeSoundHidden(soundHidden);
        }
    }

    /* THE URL THE SYSTEM OPENED THE APP WITH (a deep link, a sign-in callback), whole:
       the C side's queue answers it to rcFileDialogPoll(RC_FILE_DIALOG_OPENED_BY_OS).
       SDL's own copy, sent while super.onCreate runs, has the URL's path alone, and
       the C side ignores it. A launch comes before the C side can take anything, so
       it waits here until the C side registers nativeOpened and calls openedWatch. */
    private static final Object openedLock = new Object();
    private static final ArrayList<byte[]> openedWaiting = new ArrayList<byte[]>();
    private static boolean openedWatched;
    private static native void nativeOpened(byte[] url);

    private static void openedWatch() {
        synchronized (openedLock) {
            openedWatched = true;
            for (byte[] url : openedWaiting)
                nativeOpened(url);
            openedWaiting.clear();
        }
    }

    private static void opened(Intent intent) {
        String url = intent == null ? null : intent.getDataString();

        if (url == null)
            return;
        byte[] bytes = url.getBytes(StandardCharsets.UTF_8);
        synchronized (openedLock) {
            if (openedWatched)
                nativeOpened(bytes);
            else
                openedWaiting.add(bytes);
        }
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        opened(intent);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        emptyCopies();
        super.onCreate(savedInstanceState);
        clearForcedNavigationBar(this);
        keyboardReportsItsCover();
        /* A task the system restored, or the user reopened from recents, carries
           the intent that started it, already delivered. */
        Intent launch = getIntent();
        if (savedInstanceState == null && launch != null &&
                (launch.getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) == 0)
            opened(launch);
        /* SDL's thread has not started (super.onCreate has just cleared it), so
           a first frame is always to come; a broken library never draws one. */
        View content = findViewById(android.R.id.content);
        if (!mBrokenLibraries && content != null)
            new LaunchHold(content);
    }

    /* THE EARLY START: SDL's thread starts once the activity is created, after
       onCreate (a subclass's too) and onStart, instead of when the surface is
       ready (SDL's way): the C side makes its window, GL context, shaders,
       fonts and first layout while the UI thread lays the window out and makes
       the surface, and draws its first frame when the surface comes. */
    @Override
    protected void onPostCreate(Bundle savedInstanceState) {
        super.onPostCreate(savedInstanceState);
        startSDLThreadEarly();
    }

    /* The window keeps its place under a soft keyboard and hears its cover through
       the insets, which RayClay's own lift and pan answer. Left to the platform's
       default (SDL's manifest names no mode), Android would pick a pan of its own,
       a second mover of the same caret. A mode the manifest names stays. The
       constant is deprecated from 30 and still the mode's name. */
    @SuppressWarnings("deprecation")
    private void keyboardReportsItsCover() {
        Window window = getWindow();
        int adjust = WindowManager.LayoutParams.SOFT_INPUT_MASK_ADJUST;

        if (window != null && (window.getAttributes().softInputMode & adjust) ==
                WindowManager.LayoutParams.SOFT_INPUT_ADJUST_UNSPECIFIED)
            window.setSoftInputMode(window.getAttributes().softInputMode |
                                    WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
    }

    /* Holds the launch screen until the C side's first frame is on screen or
       LAUNCH_CAP_MS pass (THE LAUNCH SCREEN), one per activity: SDL ends the
       process rather than create a second. From API 26 it holds the window's
       draw REPORTS, each Runnable SDL's surface view hands RCSurface at a draw
       (hold), and release runs them; the system calls hold on the UI thread and
       takes a report's Runnable from any thread. Below 26 a pre-draw listener
       that answers false cancels the draws themselves and ViewRootImpl tries
       again next frame; the listener removes itself on the UI thread, as the
       view tree is no other thread's. Either way SDL's surface is created at
       the window's first layout, so the C side draws its first frame under
       the launch screen. */
    private static final class LaunchHold implements ViewTreeObserver.OnPreDrawListener, Runnable {
        private static volatile LaunchHold current;
        private final View content;   /* the view whose draws are held, below 26 only */
        private final Handler handler = new Handler(Looper.getMainLooper());
        private final long start = SystemClock.uptimeMillis();
        private final ArrayList<Runnable> reports = new ArrayList<Runnable>();   /* under this */
        private boolean shown;   /* the first frame is up, or the cap passed (under this) */

        LaunchHold(View content) {
            this.content = Build.VERSION.SDK_INT < 26 ? content : null;
            current = this;
            if (this.content != null)
                this.content.getViewTreeObserver().addOnPreDrawListener(this);
            handler.postDelayed(this, LAUNCH_CAP_MS);
        }

        /* A window draw's report, held until release; at once when none is
           held (a later draw, or no hold at all). */
        static void hold(Runnable report) {
            LaunchHold h = current;
            if (h != null) {
                synchronized (h) {
                    if (!h.shown) {
                        h.reports.add(report);
                        return;
                    }
                }
            }
            report.run();
        }

        /* The first frame is on screen (byFrame) or the cap passed: every held
           report runs, and the next pre-draw draws. Once. */
        void release(boolean byFrame) {
            Runnable[] held;
            synchronized (this) {
                if (shown)
                    return;
                shown = true;
                held = reports.toArray(new Runnable[0]);
                reports.clear();
            }
            handler.removeCallbacks(this);
            if (byFrame)
                Log.v(TAG, "launch screen held " + (SystemClock.uptimeMillis() - start)
                           + " ms, released by the first frame");
            else
                Log.w(TAG, "launch screen released by its " + LAUNCH_CAP_MS + " ms cap: no frame "
                           + "was on screen by then");
            for (Runnable report : held)
                report.run();
        }

        @Override
        public boolean onPreDraw() {
            synchronized (this) {
                if (!shown)
                    return false;
            }
            content.getViewTreeObserver().removeOnPreDrawListener(this);
            return true;
        }

        /* The cap. */
        @Override
        public void run() {
            release(false);
        }
    }

    /* Called by the C side, on SDL's thread, once its first frame is on
       screen: the window is shown, launch screen gone, at once from API 26
       and at the next frame below it. */
    static void rcFirstFrame() {
        LaunchHold h = LaunchHold.current;
        if (h != null)
            h.release(true);
    }

    /* Started and stopped as the app enters and leaves the foreground, as
       RayClayMedia's players pause (onActivityStopped). */
    @Override
    protected void onStart() {
        super.onStart();
        soundShown(true);
    }

    @Override
    protected void onStop() {
        soundShown(false);
        super.onStop();
    }

    /* SDL's surface, its rotation from the display manager (THE ROTATION). */
    @Override
    protected SDLSurface createSDLSurface(Context context) {
        return new RCSurface(context);
    }

    /* SDL turns its accelerometer listener on at every resume and off at every
       pause, on the UI thread, through enableSensor: here the display
       listener takes its place (SDL's own, should the system give no display
       manager), and the rotation is read again at each resume, so a turn
       while paused reaches SDL as SDL's first reading did. */
    static final class RCSurface extends SDLSurface implements DisplayManager.DisplayListener,
            SurfaceHolder.Callback2 {
        private final DisplayManager displays;
        private final Handler handler = new Handler(Looper.getMainLooper());
        private boolean listening;

        RCSurface(Context context) {
            super(context);
            displays = (DisplayManager) context.getSystemService(Context.DISPLAY_SERVICE);
        }

        @Override
        protected void enableSensor(int sensortype, boolean enabled) {
            if (sensortype != Sensor.TYPE_ACCELEROMETER || displays == null) {
                super.enableSensor(sensortype, enabled);
                return;
            }
            if (enabled == listening)
                return;
            listening = enabled;
            if (enabled) {
                displays.registerDisplayListener(this, handler);
                rotated();
            } else {
                displays.unregisterDisplayListener(this);
            }
        }

        /* THE LAUNCH SCREEN: the window's draw reports when this redraw is
           done, from API 26 (the asynchronous form, which the system calls in
           place of the one below). */
        @Override
        public void surfaceRedrawNeeded(SurfaceHolder holder) {
        }

        @Override
        public void surfaceRedrawNeededAsync(SurfaceHolder holder, Runnable drawingFinished) {
            LaunchHold.hold(drawingFinished);
        }

        /* A KEYBOARD THE APP DID NOT RAISE IS NOT ITS OWN. SDL takes any keyboard
           the insets show for the app's, and shows it again at the next surface
           change (Android_RestoreScreenKeyboard) with its editor focused. An app
           opened from another app's search field gets that app's keyboard in its
           first insets, so SDL focused its editor and the system kept the keyboard
           up over an app with no field focused. Only SDL's editor raises the
           app's keyboard and holds the focus while it is up, so with it not
           focused SDL hears the keyboard hidden, and the system takes the other
           app's keyboard down as for any window with no editor focused. */
        @Override
        public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
            WindowInsets seen = insets;

            if (Build.VERSION.SDK_INT >= 30 && insets.isVisible(WindowInsets.Type.ime()) &&
                    (SDLActivity.mTextEdit == null || !SDLActivity.mTextEdit.isFocused()))
                seen = new WindowInsets.Builder(insets).setVisible(WindowInsets.Type.ime(), false).build();
            super.onApplyWindowInsets(v, seen);
            return insets;
        }

        @Override
        public void onDisplayAdded(int displayId) {
        }

        @Override
        public void onDisplayRemoved(int displayId) {
        }

        @Override
        public void onDisplayChanged(int displayId) {
            rotated();
        }

        /* What SDL's accelerometer handler does with a reading: the rotation,
           passed down when it moved. */
        private static void rotated() {
            int rotation = SDLActivity.getCurrentRotation();
            if (rotation != SDLActivity.mCurrentRotation) {
                SDLActivity.mCurrentRotation = rotation;
                SDLActivity.onNativeRotationChanged(rotation);
            }
        }
    }

    /* The system's appearance moved: AUTO's icons follow it while the C side
       has not resolved AUTO from a frame, on either bar. The icons only: the
       status bar stays shown or hidden as it is. */
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        int style = statusBar;
        if (style == BAR_AUTO || (style >= 0 && navigationBar == BAR_AUTO))
            applyIcons(this, style);
    }

    /* rcSetStatusBar's Android half, from SDL's thread: the style is applied
       on the UI thread, the icons and then the status bar shown or hidden. */
    static void rcStatusBar(final Activity activity, final int style) {
        statusBar = style;
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                applyIcons(activity, style);
                applyShown(activity, style);
            }
        });
    }

    /* The navigation bar's icons as the C side read them, from SDL's thread:
       applied on the UI thread with the status bar's style. The icons only,
       so a status bar the user swiped into view under HIDDEN is not hidden
       again each time the band under the navigation bar changes. */
    static void rcNavigationBar(final Activity activity, final int icons) {
        navigationBar = icons;
        final int style = statusBar;
        if (style < 0)
            return;
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                applyIcons(activity, style);
            }
        });
    }

    /* Edge to edge (Android 15 and later), while the app names no
       navigation-bar color, a window whose background is a light color gets
       the system's forced light navigation appearance (DecorView.
       setWindowBackground sets the hidden APPEARANCE_FORCE_LIGHT_NAVIGATION_BARS
       and moves that background into the bar's color), which overrides the
       appearance applyIcons sets and tints a three-button bar's scrim white
       over every app, a dark one too. Naming the transparent color the bar
       already shows clears it for good (PhoneWindow.setNavigationBarColor marks
       the color specified, so setWindowBackground never sets it again): the
       icons and the scrim then follow applyIcons. Once, after super.onCreate
       (the decor and its window background are in place by then), and only
       while the theme names no color of its own (namesNavigationBarColor):
       an app's own color stays, and the system sets no forced appearance over
       it. A subclass naming one in code does so after super.onCreate, and its
       call wins. Below Android 15 the bar keeps the theme's color,
       untouched (getNavigationBarColor answers it). The navigation-bar color
       calls are deprecated from 35 and still the only route to that clear. */
    @SuppressWarnings("deprecation")
    private static void clearForcedNavigationBar(Activity activity) {
        Window window = activity.getWindow();
        if (Build.VERSION.SDK_INT < 35 || window == null)
            return;
        if (Color.alpha(window.getNavigationBarColor()) == 0 && !namesNavigationBarColor(activity))
            window.setNavigationBarColor(Color.TRANSPARENT);
    }

    /* Whether the activity's theme names a navigation-bar color, as PhoneWindow
       tells it (generateLayout): a color other than the platform's own default,
       navigation_bar_default (a transparent gray, #00808080, on Android 17).
       Should a later Android drop that name, a transparent color counts as
       none, as that default is one. The attribute is deprecated from 35, and
       PhoneWindow still reads it. */
    @SuppressWarnings("deprecation")
    private static boolean namesNavigationBarColor(Activity activity) {
        Resources res = activity.getResources();
        int id = res.getIdentifier("navigation_bar_default", "color", "android");
        TypedArray a = activity.getTheme().obtainStyledAttributes(
            new int[] { android.R.attr.navigationBarColor });
        try {
            if (id != 0) {
                int unnamed = res.getColor(id, null);
                return a.getColor(0, unnamed) != unnamed;
            }
            return a.hasValue(0) && Color.alpha(a.getColor(0, 0)) != 0;
        } finally {
            a.recycle();
        }
    }

    /* The icons of both bars for @p style. Dark icons are the light-status-bar
       appearance (API 23; 30 through the insets controller); below 23 the
       icons are always light. The navigation bar's are the light-navigation-bar
       appearance (API 26), light below: the band under it as the C side read it
       (navigationBar). The flags below 30 are the only route there, deprecated
       or not. */
    @SuppressWarnings("deprecation")
    private static void applyIcons(Activity activity, int style) {
        Window window = activity.getWindow();
        if (window == null)
            return;
        boolean dark = darkIcons(activity, window.getStatusBarColor(), style);
        boolean navDark = darkIcons(activity, window.getNavigationBarColor(), navigationBar);

        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController c = window.getInsetsController();
            if (c == null)
                return;
            int light = WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS;
            int lightNav = WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS;
            c.setSystemBarsAppearance((dark ? light : 0) | (navDark ? lightNav : 0),
                                      light | lightNav);
            return;
        }
        View decor = window.getDecorView();
        int flags = decor.getSystemUiVisibility();
        if (Build.VERSION.SDK_INT >= 23)
            flags = dark ? flags | View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR
                         : flags & ~View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
        if (Build.VERSION.SDK_INT >= 26)
            flags = navDark ? flags | View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR
                            : flags & ~View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
        decor.setSystemUiVisibility(flags);
    }

    /* The status bar hidden under HIDDEN, else shown: rcStatusBar's alone, so
       only the app's own call moves it. */
    @SuppressWarnings("deprecation")
    private static void applyShown(Activity activity, int style) {
        Window window = activity.getWindow();
        boolean hidden = style == BAR_HIDDEN;
        if (window == null)
            return;
        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController c = window.getInsetsController();
            if (c == null)
                return;
            if (hidden)
                c.hide(WindowInsets.Type.statusBars());
            else
                c.show(WindowInsets.Type.statusBars());
            return;
        }
        View decor = window.getDecorView();
        int flags = decor.getSystemUiVisibility();
        flags = hidden ? flags | View.SYSTEM_UI_FLAG_FULLSCREEN
                       : flags & ~View.SYSTEM_UI_FLAG_FULLSCREEN;
        decor.setSystemUiVisibility(flags);
        if (hidden)
            window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        else
            window.clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
    }

    /* Whether a bar's icons are dark, on a bar of color @p bar. An opaque bar
       color (below Android 15 the window is laid out between the bars, which
       keep the theme's statusBarColor and navigationBarColor) takes the icons
       that read on it, by the C side's rule: dark above a luminance of 0.3,
       whatever the style, since no band of the app's is under the bar. A
       transparent bar (edge to edge, where Android 15 makes it so) shows the
       band: an app's LIGHT_ICONS or DARK_ICONS stands, and AUTO takes the
       band's icons, or the system's appearance while the C side reads no
       band. */
    private static boolean darkIcons(Activity activity, int bar, int style) {
        if (Color.alpha(bar) == 0xff)
            return luminance(bar) > 0.3;
        if (style == BAR_LIGHT_ICONS || style == BAR_DARK_ICONS || style == BAR_HIDDEN)
            return style == BAR_DARK_ICONS;
        if (style != BAR_AUTO)
            return style == BAR_AUTO_DARK;
        return (activity.getResources().getConfiguration().uiMode
                & Configuration.UI_MODE_NIGHT_MASK) != Configuration.UI_MODE_NIGHT_YES;
    }

    /* WCAG's relative luminance of an opaque color, as the C side computes it. */
    private static double luminance(int color) {
        int[] channel = { Color.red(color), Color.green(color), Color.blue(color) };
        double[] weight = { 0.2126, 0.7152, 0.0722 };
        double sum = 0;
        for (int i = 0; i < 3; i++) {
            double v = channel[i] / 255.0;
            sum += weight[i] * (v <= 0.04045 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4));
        }
        return sum;
    }

    /* A request still unanswered here was left behind: the safety net. */
    @Override
    protected void onResume() {
        super.onResume();
        Request left;
        synchronized (lock) {
            left = request;
            if (left == null || left.answered)
                return;
            request = null;
        }
        finishPicker(left.code);
        nativeFilesAnswer(left.token, CANCELED, null);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        final Request r;
        synchronized (lock) {
            r = request;
            if (r == null || r.code != requestCode || r.answered)
                return;
            r.answered = true;
            if (resultCode != RESULT_OK)
                request = null;
            else if (r.kind == KIND_FOLDER)
                r.importing = true;
        }
        if (resultCode != RESULT_OK) {
            nativeFilesAnswer(r.token, CANCELED, null);
            return;
        }
        final ContentResolver resolver = getApplicationContext().getContentResolver();
        if (r.kind == KIND_FOLDER) {
            final Uri tree = data != null ? data.getData() : null;
            new Thread(new Runnable() {
                @Override
                public void run() {
                    importTree(r, tree, resolver);
                }
            }, "RCFiles").start();
            return;
        }
        final List<Uri> uris = chosen(data);
        new Thread(new Runnable() {
            @Override
            public void run() {
                work(r, uris, resolver);
            }
        }, "RCFiles").start();
    }

    /* Called by the C side, on SDL's thread: start the picker for one request.
       accept is the canonical extension list (".png,.jpg"; "" for any file),
       name a save's suggested name, dir an open's folder for its copies. */
    public int rcFilesShow(int token, int kind, byte[] accept, byte[] name, byte[] dir,
                           int maxFiles, long maxBytes) {
        Intent intent;
        if (kind == KIND_SAVE) {
            String title = new String(name, StandardCharsets.UTF_8);
            int dot = title.lastIndexOf('.');
            String type = dot > 0 ? mime(title.substring(dot + 1)) : null;
            intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType(type != null ? type : "application/octet-stream");
            intent.putExtra(Intent.EXTRA_TITLE, title);
        } else if (kind == KIND_FOLDER) {
            intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        } else {
            intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, kind == KIND_FILES);
            types(intent, new String(accept, StandardCharsets.UTF_8));
        }
        Request r;
        synchronized (lock) {
            /* An import still running was answered or closed: the C side asks
               again only then, and the import reads its own request. */
            if (request != null && !request.importing)
                return BUSY;
            r = new Request(token, kind, REQUEST_CODES + (sequence++ & 0xff), maxFiles, maxBytes,
                            dir != null ? new String(dir, StandardCharsets.UTF_8) : null);
            request = r;
        }
        try {
            startActivityForResult(intent, r.code);
            return SHOWN;
        } catch (RuntimeException e) {
            synchronized (lock) {
                if (request == r)
                    request = null;
            }
            return e instanceof ActivityNotFoundException ? NO_PICKER : NOT_SHOWN;
        }
    }

    /* Called by the C side, on SDL's thread: request `token` is not wanted any
       more (teardown). Its picker is finished, or its worker stops. */
    public void rcFilesClose(int token) {
        Request r;
        boolean picking;
        synchronized (lock) {
            r = request;
            if (r == null || r.token != token)
                return;
            request = null;
            r.stop = true;
            picking = !r.answered;
        }
        r.signal.cancel();   /* a folder's query or open in progress gives up */
        if (picking)
            finishPicker(r.code);
    }

    private void finishPicker(int code) {
        try {
            finishActivity(code);
        } catch (RuntimeException e) {
            /* already gone */
        }
    }

    /* The accept list as the picker's MIME types. An extension the system has
       no type for shows every file, rather than hiding the ones it names. */
    private static void types(Intent intent, String accept) {
        List<String> mimes = new ArrayList<String>();
        for (String entry : accept.isEmpty() ? new String[0] : accept.split(",")) {
            String mime = mime(entry.substring(1));
            if (mime == null) {
                mimes.clear();
                break;
            }
            if (!mimes.contains(mime))
                mimes.add(mime);
        }
        intent.setType(mimes.size() == 1 ? mimes.get(0) : "*/*");
        if (mimes.size() > 1)
            intent.putExtra(Intent.EXTRA_MIME_TYPES, mimes.toArray(new String[0]));
    }

    private static String mime(String extension) {
        MimeTypeMap map = MimeTypeMap.getSingleton();
        return map.getMimeTypeFromExtension(extension.toLowerCase(Locale.ROOT));
    }

    private static List<Uri> chosen(Intent data) {
        List<Uri> uris = new ArrayList<Uri>();
        ClipData clip = data != null ? data.getClipData() : null;
        if (clip != null) {
            for (int i = 0; i < clip.getItemCount(); i++) {
                Uri uri = clip.getItemAt(i).getUri();
                if (uri != null)
                    uris.add(uri);
            }
        }
        if (uris.isEmpty() && data != null && data.getData() != null)
            uris.add(data.getData());
        return uris;
    }

    /* The worker: copy or write, then answer, unless rcFilesClose came first. */
    private static void work(Request r, List<Uri> uris, ContentResolver resolver) {
        byte[][] paths = null;
        int what;
        if (uris.isEmpty())
            what = FAILED_EMPTY;
        else if (r.kind == KIND_SAVE)
            what = save(r, uris.get(0), resolver);
        else if (uris.size() > r.maxFiles)
            what = FAILED_LIMIT;
        else {
            paths = new byte[uris.size()][];
            what = copy(r, uris, resolver, paths);
        }
        boolean stopped;
        synchronized (lock) {
            stopped = request != r;
            if (!stopped)
                request = null;
        }
        if (what != DONE || stopped) {
            if (r.dir != null)
                delete(new File(r.dir));
            if (stopped || what == STOPPED)
                return;
            paths = null;
        }
        /* Refused (the app is closing): no release will come, so the copies go now. */
        if (!nativeFilesAnswer(r.token, what, paths) && r.dir != null)
            delete(new File(r.dir));
    }

    /* A folder: the C side imports the granted tree and answers; this thread
       answers only when there is no tree to import. */
    private static void importTree(Request r, Uri tree, ContentResolver resolver) {
        byte[] name = null;
        int what = DONE;
        if (tree == null)
            what = FAILED_EMPTY;
        else {
            try {
                r.rootId = DocumentsContract.getTreeDocumentId(tree);
                r.tree = tree;
                r.resolver = resolver;
                name = displayName(resolver,
                                   DocumentsContract.buildDocumentUriUsingTree(tree, r.rootId),
                                   "folder");
            } catch (RuntimeException e) {
                what = FAILED_READ;
            }
        }
        if (what == DONE && !r.stop)
            nativeFilesImport(r.token, r, name);
        boolean stopped;
        synchronized (lock) {
            stopped = request != r;
            if (!stopped)
                request = null;
        }
        if (what != DONE && !stopped)
            nativeFilesAnswer(r.token, what, null);
    }

    /* Called by the C side, on the import's worker: the children of one folder
       of the tree ("" its root), at most max + 1 of them, each row its kind,
       its plain name, a 0 byte and its document ID, all UTF-8; null when the
       provider will not list it, or the import stopped. */
    private static byte[][] rcFilesList(Object request, byte[] document, int max) {
        Request r = (Request) request;
        Cursor c = null;
        if (r.stop)
            return null;
        try {
            String id = document.length == 0 ? r.rootId
                                             : new String(document, StandardCharsets.UTF_8);
            String[] columns = { DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                                 DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                                 DocumentsContract.Document.COLUMN_MIME_TYPE,
                                 DocumentsContract.Document.COLUMN_FLAGS };
            c = r.resolver.query(DocumentsContract.buildChildDocumentsUriUsingTree(r.tree, id),
                                 columns, null, null, null, r.signal);
            if (c == null)
                return null;
            List<byte[]> rows = new ArrayList<byte[]>();
            while (rows.size() <= max && c.moveToNext()) {
                if (r.stop || c.isNull(0))
                    return null;
                String mime = c.isNull(2) ? null : c.getString(2);
                int flags = c.isNull(3) ? 0 : c.getInt(3);
                byte kind = DocumentsContract.Document.MIME_TYPE_DIR.equals(mime) ? ENTRY_FOLDER
                          : (flags & FLAG_VIRTUAL) != 0 ? ENTRY_OTHER : ENTRY_FILE;
                byte[] name = plain(c.isNull(1) ? null : c.getString(1), "file");
                byte[] child = c.getString(0).getBytes(StandardCharsets.UTF_8);
                byte[] row = new byte[2 + name.length + child.length];
                row[0] = kind;
                System.arraycopy(name, 0, row, 1, name.length);
                System.arraycopy(child, 0, row, 2 + name.length, child.length);
                rows.add(row);
            }
            return rows.toArray(new byte[0][]);
        } catch (RuntimeException e) {
            return null;   /* the provider's error, a lost grant, or the cancel */
        } finally {
            close(c);
        }
    }

    /* Called by the C side, on the import's worker: one document of the tree
       open for reading, its file descriptor now the caller's; -1 when it is
       gone, unreadable, no longer a document, or the import stopped. */
    private static int rcFilesOpen(Object request, byte[] document) {
        Request r = (Request) request;
        if (r.stop)
            return -1;
        try {
            Uri uri = DocumentsContract.buildDocumentUriUsingTree(
                r.tree, new String(document, StandardCharsets.UTF_8));
            ParcelFileDescriptor pfd = r.resolver.openFileDescriptor(uri, "r", r.signal);
            return pfd != null ? pfd.detachFd() : -1;
        } catch (IOException | RuntimeException e) {
            return -1;
        }
    }

    /* Each document into <dir><k>/<name>, created by the C side from the
       path's bytes. Every byte counts toward maxBytes. */
    private static int copy(Request r, List<Uri> uris, ContentResolver resolver, byte[][] paths) {
        byte[] buffer = new byte[64 * 1024];
        long total = 0;
        for (int k = 0; k < uris.size(); k++) {
            if (r.stop)
                return STOPPED;
            File folder = new File(r.dir + k);
            if (!folder.mkdirs() && !folder.isDirectory())
                return FAILED_READ;
            byte[] prefix = (r.dir + k + "/").getBytes(StandardCharsets.UTF_8);
            byte[] name = name(resolver, uris.get(k));
            byte[] path = new byte[prefix.length + name.length];
            System.arraycopy(prefix, 0, path, 0, prefix.length);
            System.arraycopy(name, 0, path, prefix.length, name.length);
            int fd = nativeFilesCreate(path);
            if (fd < 0)
                return FAILED_READ;
            OutputStream out = new ParcelFileDescriptor.AutoCloseOutputStream(
                ParcelFileDescriptor.adoptFd(fd));
            InputStream in = null;
            try {
                in = resolver.openInputStream(uris.get(k));
                if (in == null)
                    return FAILED_READ;
                for (int n; (n = in.read(buffer)) != -1; ) {
                    if (r.stop)
                        return STOPPED;
                    total += n;
                    if (total > r.maxBytes)
                        return FAILED_LIMIT;
                    out.write(buffer, 0, n);
                }
                out.close();   /* a failed close is a failed copy */
            } catch (IOException | RuntimeException e) {
                return FAILED_READ;
            } finally {
                close(in);
                close(out);
            }
            paths[k] = path;
        }
        return DONE;
    }

    /* The C side writes the bytes into the chosen document. The picker answers
       the document it made, or one that exists when the user chose to replace
       it, so a save that does not finish deletes the document again only when
       it was empty before; one that held bytes is left as the write left it. */
    private static int save(Request r, Uri uri, ContentResolver resolver) {
        boolean empty = size(resolver, uri) == 0;
        ParcelFileDescriptor pfd = null;
        int what = FAILED_WRITE;
        try {
            pfd = resolver.openFileDescriptor(uri, "wt");
            if (pfd != null) {
                int rc = nativeFilesWrite(r.token, pfd.getFd());
                what = rc == 0 ? DONE : rc == STOPPED ? STOPPED : FAILED_WRITE;
                pfd.close();   /* a pipe's reader says here whether it took every byte */
                pfd = null;
            }
        } catch (IOException | RuntimeException e) {
            if (what == DONE)
                what = FAILED_WRITE;
        } finally {
            close(pfd);
        }
        if (what != DONE && empty) {
            try {
                DocumentsContract.deleteDocument(resolver, uri);
            } catch (IOException | RuntimeException e) {
                /* left as it is */
            }
        }
        return what;
    }

    /* The document's size in bytes, or -1 when its provider does not say. */
    private static long size(ContentResolver resolver, Uri uri) {
        Cursor c = null;
        try {
            String[] column = { OpenableColumns.SIZE };
            c = resolver.query(uri, column, null, null, null);
            return c != null && c.moveToFirst() && !c.isNull(0) ? c.getLong(0) : -1;
        } catch (RuntimeException e) {
            return -1;
        } finally {
            close(c);
        }
    }

    /* The document's name as a plain file name, in UTF-8 (plain). */
    private static byte[] name(ContentResolver resolver, Uri uri) {
        return displayName(resolver, uri, "file");
    }

    private static byte[] displayName(ContentResolver resolver, Uri uri, String fallback) {
        String shown = null;
        Cursor c = null;
        try {
            String[] column = { OpenableColumns.DISPLAY_NAME };
            c = resolver.query(uri, column, null, null, null);
            if (c != null && c.moveToFirst() && !c.isNull(0))
                shown = c.getString(0);
        } catch (RuntimeException e) {
            shown = null;
        } finally {
            close(c);
        }
        return plain(shown, fallback);
    }

    /* A provider's name as a plain file name, in UTF-8. DISPLAY_NAME is the
       provider's word and "might not necessarily be the file name": '/', NUL
       and a lone surrogate become '_', "", "." and ".." read `fallback`, and a
       name past NAME_MAX bytes loses the end of its stem, not its extension. */
    private static byte[] plain(String shown, String fallback) {
        StringBuilder plain = new StringBuilder();
        for (int i = 0; shown != null && i < shown.length(); i++) {
            char ch = shown.charAt(i);
            if (Character.isHighSurrogate(ch) && i + 1 < shown.length() &&
                    Character.isLowSurrogate(shown.charAt(i + 1)))
                plain.append(ch).append(shown.charAt(++i));
            else
                plain.append(ch == '/' || ch == '\0' || Character.isSurrogate(ch) ? '_' : ch);
        }
        String s = plain.toString();
        if (s.isEmpty() || s.equals(".") || s.equals(".."))
            s = fallback;
        byte[] bytes = s.getBytes(StandardCharsets.UTF_8);
        if (bytes.length <= NAME_MAX)
            return bytes;
        int dot = s.lastIndexOf('.');
        String ext = dot > 0 && s.length() - dot <= 16 ? s.substring(dot) : "";
        int room = NAME_MAX - ext.getBytes(StandardCharsets.UTF_8).length;
        StringBuilder stem = new StringBuilder();
        for (int i = 0, used = 0; i < s.length() - ext.length(); ) {
            int cp = s.codePointAt(i);
            int n = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
            if (used + n > room)
                break;
            stem.appendCodePoint(cp);
            used += n;
            i += Character.charCount(cp);
        }
        return (stem.length() > 0 ? stem + ext : fallback + ext).getBytes(StandardCharsets.UTF_8);
    }

    /* Copies left by an earlier process: the folder is moved aside at once, so
       nothing new lands among them, and swept on a thread of its own. */
    private void emptyCopies() {
        synchronized (lock) {
            if (emptied)
                return;
            emptied = true;
        }
        final File cache = getCacheDir();
        if (cache == null)
            return;
        File root = new File(cache, FOLDER);
        if (root.exists() && !root.renameTo(new File(cache, OLD + System.nanoTime())))
            delete(root);
        final List<File> old = new ArrayList<File>();
        File[] all = cache.listFiles();
        for (File f : all != null ? all : new File[0])
            if (f.getName().startsWith(OLD))
                old.add(f);
        if (old.isEmpty())
            return;
        new Thread(new Runnable() {
            @Override
            public void run() {
                for (File f : old)
                    delete(f);
            }
        }, "RCFilesSweep").start();
    }

    /* Delete f and, for a folder, everything in it; a link is removed, never
       followed. */
    private static void delete(File f) {
        try {
            if (OsConstants.S_ISDIR(Os.lstat(f.getPath()).st_mode)) {
                File[] children = f.listFiles();
                if (children != null)
                    for (File child : children)
                        delete(child);
            }
        } catch (ErrnoException e) {
            return;   /* gone already */
        }
        f.delete();
    }

    private static void close(Closeable c) {
        if (c == null)
            return;
        try {
            c.close();
        } catch (IOException e) {
            /* the answer is settled */
        }
    }
}
