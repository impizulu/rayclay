/*
 * RayClayMedia - RayClay's media player on Android: what rcMediaOpen and its
 * family play through. Its C half is in
 * rayclay.h.
 *
 * TO USE IT, add this file to the app beside RCActivity.java, as
 * app/src/main/java/org/rayclay/RayClayMedia.java. Without it every
 * rcMediaOpen reads FAILED / UNAVAILABLE, and RayClay's log says why, once.
 * Only RayClay's C side calls it, so a build that shrinks code (R8 or
 * ProGuard) must keep the class whole:
 *
 *     -keep class org.rayclay.RayClayMedia { *; }
 *
 * ONE THREAD OWNS EVERY PLAYER. MediaPlayer "is not thread-safe. Creation of
 * and all access to player instances should be on the same thread. If
 * registering callbacks, the thread must have a Looper" (MediaPlayer.java).
 * So one HandlerThread, started at the first open and ended by shutdown,
 * creates, drives and releases every MediaPlayer. Each call from the C side
 * (the app's main thread) is posted to it and returns at once, and every
 * MediaPlayer call goes through mp(), which refuses, counts and throws when
 * it is reached from any other thread. Its listeners, the frame listener of
 * the SurfaceTexture a video draws into and the audio-focus listener all run
 * on that thread's Looper and report through one native method, nativeReport,
 * which the C side registers. MAIN only posts and reads what was reported.
 *
 * WHAT IT REPORTS, each from its own listener and never inferred: prepared
 * (READY, with the duration and the video's size), completion (ENDED), an
 * error (FAILED: the C side's RC_MediaError, the platform's what and extra),
 * buffering's start and end (WAITING), a seek's completion (SEEKED, with the C
 * side's number for it), a frame in the SurfaceTexture (FRAME), a new video
 * size (SIZE), and where the playhead is (TIME: a media time, the
 * System.nanoTime it was at and the clock's rate, which the C side
 * extrapolates; refreshed four times a second while a media plays). With an
 * error listener a failure never reaches the completion listener.
 *
 * SEEKS run one at a time. A seek asked for while one runs replaces any that
 * waits and starts when the running one completes, so a drag that asks for
 * many lands on its last, and every SEEKED names a seek that finished. Exact
 * from API 26 (SEEK_CLOSEST); below it the previous sync frame. A paused
 * player shows a frame: a SurfaceTexture lent to a prepared player that is not
 * playing gets the frame where it stands, by a seek to there.
 *
 * A PAUSED EXACT SEEK SHOWS THE FRAME THAT HOLDS ITS TIME, as currentTime does
 * on a page. SEEK_CLOSEST renders from the first frame at or after the
 * millisecond it is given (AOSP: GenericSource makes it every track's
 * resume-at time, NuPlayerDecoder drops what comes before), so a target inside
 * a frame showed the next one. A paused seek is given instead the millisecond
 * at or before the start of the frame that holds its target, read off the
 * container by a MediaExtractor of the media's own on a thread of the media's
 * own ("RayClayMedia frames"), never the owner: the extractor parses the
 * container when it opens (a content:// source may read the network) and
 * reads every sample over binder. Both are made at the media's first paused
 * seek past 0 and end when it plays, closes or stays FRAMES_IDLE_MS without
 * another paused seek, so only a media being scrubbed while paused holds them.
 * The seek waits for that answer at most FRAME_WAIT_MS; a play meanwhile
 * issues it at its own millisecond. SEEK_CLOSEST moves the sound and the
 * playhead there too, so a play after it starts there: at most REWIND_MAX_US
 * before the target, inside the frame on screen (or under a millisecond
 * before it). Every other seek takes its own millisecond, as before: one while
 * playing; one further into its frame than REWIND_MAX_US (a frame held long: a
 * still, a screen recording); one past the video's last frame (sound that
 * outlasts it); one whose answer outlasts FRAME_WAIT_MS; every seek of a URL's
 * media, which gets no extractor.
 *
 * .alt: given a second source, MediaCodecList is asked first whether this
 * device decodes every track of the first (MediaExtractor reads their formats,
 * findDecoderForFormat answers: profile, level, size); when it does not, the
 * second plays. That question runs on a short thread of its own, never the
 * owner's: MediaExtractor reads the container, over the network for a URL,
 * and the owner must keep driving every other media meanwhile. A first source
 * whose tracks cannot be read is played as it is, and an error before it is
 * prepared moves on to the second, once. An .alt equal to the source is none.
 *
 * AUDIO FOCUS: an audible play (not muted, volume above 0) takes the focus
 * (AUDIOFOCUS_GAIN; an AudioFocusRequest from API 26). When the platform
 * refuses it (a call in progress), the media does not start and reads PAUSED
 * with BLOCKED, as Android's focus guidance asks. A loss, transient or not,
 * pauses every playing media, which reads PAUSED, and the next play asks for
 * the focus again; a can-duck loss lowers the volume to a fifth until the
 * focus returns (from API 26 the platform ducks by itself). The focus goes
 * back when nothing audible plays.
 *
 * THE FOREGROUND: when an activity of the app stops, every playing media
 * pauses (PAUSED), as the phone's own players do, and stays paused on return:
 * playback in the background is not in this release.
 *
 * HEADPHONES: when the sound is about to move to the speaker (headphones
 * unplugged, a Bluetooth headset gone: ACTION_AUDIO_BECOMING_NOISY), every
 * playing audible media (not muted, volume above 0) pauses (PAUSED), as
 * Android's media guide asks; a muted or silent one plays on: it sends nothing
 * to the speaker. The receiver is on the application context from the owner
 * thread's start to shutdown, so nothing hears the broadcast after it.
 *
 * Rate needs API 23 (PlaybackParams, the pitch kept); below it the media plays
 * at 1 and the C side's log says so once. Sources: a path, an http(s),
 * content:// or android.resource:// URI, or, for a relative name the working
 * directory lacks, the APK's asset of that name, stored uncompressed.
 */
package org.rayclay;

import android.app.Activity;
import android.app.Application;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.AssetFileDescriptor;
import android.graphics.SurfaceTexture;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.media.MediaCodecList;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.media.MediaPlayer;
import android.media.MediaTimestamp;
import android.media.PlaybackParams;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.view.Surface;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;

public final class RayClayMedia implements MediaPlayer.OnPreparedListener,
        MediaPlayer.OnCompletionListener, MediaPlayer.OnErrorListener, MediaPlayer.OnInfoListener,
        MediaPlayer.OnSeekCompleteListener, MediaPlayer.OnVideoSizeChangedListener,
        SurfaceTexture.OnFrameAvailableListener {
    /* nativeReport's `what`: the C side's RCI_MEDIA_JNI_* (in rayclay.h). PAUSED's `a` is
       1 for a play the platform refused (BLOCKED), 2 for a pause of every media (HALT). */
    static final int READY = 1, FAILED = 2, ENDED = 3, PAUSED = 4, WAITING = 5, SEEKED = 6,
                     FRAME = 7, SIZE = 8, TIME = 9, NOTE = 10;

    /* FAILED's `a`: RC_MediaError. */
    static final int UNAVAILABLE = 1, UNSUPPORTED = 2, NOT_FOUND = 3, DECODE = 4, NETWORK = 5;

    /* MediaPlayer's error extras (its MEDIA_ERROR_*), and its info codes. */
    private static final int ERROR_IO = -1004, ERROR_UNSUPPORTED = -1010,
                             ERROR_TIMED_OUT = -110, ERROR_SERVER_DIED = 100,
                             BUFFERING_START = 701, BUFFERING_END = 702;

    private static final long TICK_MS = 250;   /* TIME's refresh while a media plays */

    /* Registered by the C side (RegisterNatives); `detail` is UTF-8 or null. */
    private static native void nativeReport(int token, int what, int a, int b, long c, long d,
                                            byte[] detail);

    /* Build.VERSION.SDK_INT as the rate and seek guards read it: the device rows lower it
       to force each guard's older path. */
    static volatile int sdk = Build.VERSION.SDK_INT;

    /* MediaPlayer calls reached off the owner thread: mp() counts each, then throws. */
    static volatile int offThread;

    private static final Object lock = new Object();
    private static HandlerThread thread;   /* the owner, from the first open to shutdown */
    private static Handler handler;        /* its; under lock */

    /* The owner thread's. */
    private static final ArrayList<RayClayMedia> live = new ArrayList<RayClayMedia>();
    private static Context app;
    private static AudioManager audio;
    private static Object focusRequest;    /* an AudioFocusRequest, from API 26 */
    private static AudioManager.OnAudioFocusChangeListener focusListener;
    private static boolean focusHeld, ducked, ticking;
    private static volatile boolean watching;   /* the activity lifecycle callbacks are in */

    /* The headphones' receiver (the class comment), and MAIN's record of the context it is
       registered on, from the owner thread's start to shutdown; null while none runs. */
    private static final BroadcastReceiver noisy = new Noisy();
    private static Context noisyOn;

    private final int token;
    private final String src, alt;
    private final Handler owner;

    /* The owner thread's. */
    private MediaPlayer mp;
    private String source;                 /* what mp holds: src or alt */
    private boolean prepared, started, released;
    private float volume = 1f, rate = 1f;
    private boolean muted, loop;
    private int seekGen;                   /* the running seek's number */
    private boolean seekRunning, seekWaiting;
    private double seekNext;
    private int seekNextGen;
    private SurfaceTexture texture;
    private Surface surface;
    private int width, height;
    private Frames frames;                 /* its frames thread (the class comment), or null */
    private boolean framesNone;            /* none for this media: a URL, no video, unreadable */
    private boolean seekHeld;              /* the running seek waits for its frame's start */
    private double heldSeconds;
    private int heldGen;                   /* the held seek's number: another's answer is dropped */
    private long seekMs = -1;              /* the last SEEK_CLOSEST's millisecond */
    private double seekAsked;              /* and the target it was given for */
    private boolean rateNoted;             /* the rate's NOTE is said once a media */

    private RayClayMedia(int token, String src, String alt, Handler owner) {
        this.token = token;
        this.src = src;
        /* An .alt the same as the source is none: a failed source would move on to itself
           without end (load and onError switch while what failed is the source). */
        this.alt = alt != null && !alt.equals(src) ? alt : null;
        this.owner = owner;
    }

    /* ---- the C side's calls: MAIN; each posts to the owner and returns ---- */

    /* Open media `token`: `src` and `alt` (null: none) as UTF-8. */
    static RayClayMedia open(Activity activity, int token, byte[] src, byte[] alt) {
        final Context context = activity.getApplicationContext();
        final Application application = activity.getApplication();
        Handler h;
        synchronized (lock) {
            if (thread == null) {
                thread = new HandlerThread("RayClayMedia");
                thread.start();
                handler = new Handler(thread.getLooper());
            }
            h = handler;
        }
        final RayClayMedia m = new RayClayMedia(token, new String(src, StandardCharsets.UTF_8),
                alt != null ? new String(alt, StandardCharsets.UTF_8) : null, h);
        if (!watching && application != null) {
            watching = true;
            application.registerActivityLifecycleCallbacks(new Lifecycle());
        }
        if (noisyOn == null) {
            /* A protected broadcast, which only the system sends: Android 14 asks no
               RECEIVER_EXPORTED or RECEIVER_NOT_EXPORTED for it. */
            context.registerReceiver(noisy,
                                     new IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY));
            noisyOn = context;
        }
        h.post(new Runnable() {
            public void run() {
                app = context;
                live.add(m);
                if (m.alt != null)
                    m.choose();
                else
                    m.load(m.src);
            }
        });
        return m;
    }

    void play(final boolean play) {
        owner.post(new Runnable() {
            public void run() {
                doPlay(play);
            }
        });
    }

    void seek(final double seconds, final int gen) {
        owner.post(new Runnable() {
            public void run() {
                doSeek(seconds, gen);
            }
        });
    }

    void set(final float volume, final boolean muted, final float rate, final boolean loop) {
        owner.post(new Runnable() {
            public void run() {
                doSet(volume, muted, rate, loop);
            }
        });
    }

    /* The SurfaceTexture the GUI lends its frames to, or null. */
    void lend(final SurfaceTexture texture) {
        owner.post(new Runnable() {
            public void run() {
                doLend(texture);
            }
        });
    }

    void close() {
        owner.post(new Runnable() {
            public void run() {
                release();
                live.remove(RayClayMedia.this);
                focus();
            }
        });
    }

    /* After every media is closed: end the owner thread, waiting at most `millis`.
       True when it ended. */
    static boolean shutdown(long millis) {
        HandlerThread t;
        Handler h;
        synchronized (lock) {
            t = thread;
            h = handler;
            thread = null;
            handler = null;
        }
        if (noisyOn != null) {
            noisyOn.unregisterReceiver(noisy);
            noisyOn = null;
        }
        if (t == null)
            return true;
        h.post(new Runnable() {
            public void run() {
                for (RayClayMedia m : live)
                    m.release();
                live.clear();
                focus();
                /* The next thread starts afresh: the request names this one's Handler, and
                   a tick still queued here never runs. */
                focusRequest = null;
                ticking = false;
            }
        });
        t.quitSafely();
        try {
            t.join(millis);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
        return !t.isAlive();
    }

    /* ---- the owner thread ---- */

    /* The player, on its owner thread only: anywhere else is RayClay's bug, refused loudly. */
    private MediaPlayer mp() {
        if (Looper.myLooper() != owner.getLooper()) {
            offThread++;
            throw new IllegalStateException("RayClayMedia: a MediaPlayer call off its owner thread");
        }
        return mp;
    }

    private void report(int what, int a, int b, long c, long d, String detail) {
        if (!released)
            nativeReport(token, what, a, b, c, d,
                         detail != null ? detail.getBytes(StandardCharsets.UTF_8) : null);
    }

    private void fail(int error, String detail) {
        report(FAILED, error, 0, 0, 0, detail);
        release();
    }

    /* Load the source to play, src unless no decoder here plays one of its tracks, decided on
       a thread of its own (the class comment's .alt) and loaded back on the owner. A media
       closed meanwhile loads nothing. */
    private void choose() {
        new Thread(new Runnable() {
            public void run() {
                final String format = undecodable(src);
                owner.post(new Runnable() {
                    public void run() {
                        if (released)
                            return;
                        if (format != null)
                            report(NOTE, 0, 0, 0, 0, name(src) + " is " + format + ", which no "
                                   + "decoder on this device plays: playing .alt, " + name(alt));
                        load(format != null ? alt : src);
                    }
                });
            }
        }, "RayClayMedia .alt").start();
    }

    /* The format of `s`'s first track MediaCodecList finds no decoder for here, or null: all
       decode, or its tracks cannot be read (the player's own answer then decides). Any
       thread: it reads only `app`, written before the thread that calls it started. */
    private static String undecodable(String s) {
        MediaExtractor x = new MediaExtractor();
        try {
            source(x, s);
            MediaCodecList codecs = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
            for (int i = 0; i < x.getTrackCount(); i++) {
                MediaFormat f = x.getTrackFormat(i);
                String mime = f.getString(MediaFormat.KEY_MIME);
                if (mime == null || !(mime.startsWith("video/") || mime.startsWith("audio/")))
                    continue;
                /* API 21's findDecoderForFormat refuses any format that names a frame rate. */
                if (Build.VERSION.SDK_INT == 21)
                    f.setString(MediaFormat.KEY_FRAME_RATE, null);
                if (codecs.findDecoderForFormat(f) == null)
                    return describe(f);
            }
        } catch (IOException | RuntimeException e) {
            /* unreadable here */
        } finally {
            x.release();
        }
        return null;
    }

    private static String describe(MediaFormat f) {
        StringBuilder s = new StringBuilder(f.getString(MediaFormat.KEY_MIME));
        if (f.containsKey(MediaFormat.KEY_PROFILE))
            s.append(" profile ").append(f.getInteger(MediaFormat.KEY_PROFILE));
        if (f.containsKey(MediaFormat.KEY_WIDTH) && f.containsKey(MediaFormat.KEY_HEIGHT))
            s.append(' ').append(f.getInteger(MediaFormat.KEY_WIDTH)).append('x')
             .append(f.getInteger(MediaFormat.KEY_HEIGHT));
        return s.toString();
    }

    /* A source's last path segment, for messages. */
    private static String name(String s) {
        int slash = s.lastIndexOf('/');
        return slash >= 0 && slash + 1 < s.length() ? s.substring(slash + 1) : s;
    }

    private static boolean isUri(String s) {
        return s.contains("://");
    }

    private static void source(MediaPlayer p, String s) throws IOException {
        if (s.startsWith("content://") || s.startsWith("android.resource://")) {
            p.setDataSource(app, Uri.parse(s));
        } else if (isUri(s) || s.startsWith("/")) {
            p.setDataSource(s);
        } else {
            AssetFileDescriptor fd = app.getAssets().openFd(s);
            try {
                p.setDataSource(fd.getFileDescriptor(), fd.getStartOffset(), fd.getLength());
            } finally {
                fd.close();
            }
        }
    }

    /* A file:// URI through its Uri, which decodes it (%20), as MediaPlayer.setDataSource does. */
    private static void source(MediaExtractor x, String s) throws IOException {
        if (s.startsWith("content://") || s.startsWith("android.resource://")
                || s.startsWith("file://")) {
            x.setDataSource(app, Uri.parse(s), null);
        } else if (isUri(s) || s.startsWith("/")) {
            x.setDataSource(s);
        } else {
            AssetFileDescriptor fd = app.getAssets().openFd(s);
            try {
                x.setDataSource(fd.getFileDescriptor(), fd.getStartOffset(), fd.getLength());
            } finally {
                fd.close();
            }
        }
    }

    private static AudioAttributes attributes() {
        return new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA)
                .setContentType(AudioAttributes.CONTENT_TYPE_MOVIE).build();
    }

    /* A new player on `s`, prepared asynchronously; its events arrive on this Looper. What
       refuses it here, prepareAsync's own synchronous half included (it throws an IOException
       for a source it cannot open, a FIFO for one), moves on to .alt or reads FAILED: thrown
       on this thread, it would end the app. */
    private void load(String s) {
        source = s;
        mp = new MediaPlayer();
        MediaPlayer p = mp();
        p.setOnPreparedListener(this);
        p.setOnCompletionListener(this);
        p.setOnErrorListener(this);
        p.setOnInfoListener(this);
        p.setOnSeekCompleteListener(this);
        p.setOnVideoSizeChangedListener(this);
        p.setAudioAttributes(attributes());
        int error;
        String why;
        try {
            source(p, s);
            if (surface != null)
                p.setSurface(surface);
            p.prepareAsync();
            return;
        } catch (FileNotFoundException e) {
            error = NOT_FOUND;
            why = name(s) + ": no such file, nor an APK asset stored uncompressed";
        } catch (IOException | RuntimeException e) {
            error = isUri(s) ? NETWORK : UNSUPPORTED;
            why = name(s) + ": " + e;
        }
        if (alt != null && s.equals(src)) {
            report(NOTE, 0, 0, 0, 0, why + ": playing .alt, " + name(alt));
            mp().release();
            mp = null;
            load(alt);
        } else {
            fail(error, why);
        }
    }

    private void release() {
        if (mp != null) {
            mp().release();
            mp = null;
        }
        seekHeld = false;
        stopFrames();
        if (texture != null)
            texture.setOnFrameAvailableListener(null);
        if (surface != null)
            surface.release();
        texture = null;
        surface = null;
        released = true;
        prepared = started = false;
    }

    private void doPlay(boolean play) {
        if (!prepared || released)
            return;
        if (play) {
            started = true;
            if (seekHeld) {   /* a seek still waiting for its frame's start: from its own ms */
                seekHeld = false;
                closest(Math.round(heldSeconds * 1000.0), heldSeconds);
            }
            stopFrames();     /* only a paused seek reads frames */
            focus();
            if (blocked()) {
                time();
                return;
            }
            mp().start();
            speed();
            tick();
        } else if (started) {
            started = false;
            mp().pause();
            focus();
        }
        time();
    }

    private void doSeek(double seconds, int gen) {
        if (!prepared || released)
            return;
        if (seekRunning) {
            seekWaiting = true;
            seekNext = seconds;
            seekNextGen = gen;
            return;
        }
        startSeek(seconds, gen);
    }

    private void startSeek(double seconds, int gen) {
        long ms = Math.round(seconds * 1000.0);
        seekRunning = true;
        seekGen = gen;
        if (sdk < 26) {
            seekMs = -1;
            mp().seekTo((int) Math.min(ms, Integer.MAX_VALUE));
            return;
        }
        if (!started && ms > 0 && holdForFrame(seconds))
            return;
        closest(ms, seconds);
    }

    /* SEEK_CLOSEST to `ms` for a seek asked to `asked` seconds: doLend's seek to where the player
       stands takes the target back, as the position it reads is that frame's start, or before. */
    private void closest(long ms, double asked) {
        seekMs = ms;
        seekAsked = asked;
        mp().seekTo(ms, MediaPlayer.SEEK_CLOSEST);
    }

    /* A paused seek moves the sound and the playhead back to the millisecond it gives: never
       further than this, so a target deeper into a frame held longer (a still, a screen
       recording) keeps its own millisecond. 100 ms is a frame at 10 fps, the slowest rate AOSP's
       NuPlayerRenderer smooths as video (its kDefaultVideoFrameIntervalUs). */
    private static final long REWIND_MAX_US = 100000;

    /* How long a paused seek waits for the frames thread (its extractor's open, then the scan)
       before it goes on at its own millisecond. */
    private static final long FRAME_WAIT_MS = 500;

    /* How long the frames thread and its extractor outlive the last paused seek: a scrub keeps
       them, a media left paused gives them back (measured on a Pixel 8a: a 2 h 1080p MP4's
       extractor holds ~4.8 MB in media.extractor and 7 descriptors in the app). */
    private static final long FRAMES_IDLE_MS = 10000;

    private final Runnable framesIdle = new Runnable() {
        public void run() {
            if (!seekHeld)
                stopFrames();
        }
    };

    /* H.264's and HEVC's reorder depth is at most 16: no sample follows, in decode order, more
       than 16 that come after it in time, so once 17 past the target have been read, none
       left holds the target nor starts the frame after it. */
    private static final int REORDER_MAX = 16;

    /* frameStartMs's answer for a media whose frames it cannot read (nor ever will). */
    private static final long NO_FRAMES = -2;

    /* Hold a paused seek while the frames thread works out the millisecond to give it; false
       when the media has none (the class comment). Its answer, or FRAME_WAIT_MS without one,
       issues the seek (framed). */
    private boolean holdForFrame(double seconds) {
        if (framesNone)
            return false;
        if (frames == null) {
            String s = source;
            if (isUri(s) && !s.startsWith("file://") && !s.startsWith("content://")
                    && !s.startsWith("android.resource://")) {
                framesNone = true;   /* a URL: its scan would read the network */
                return false;
            }
            if (width <= 0 || height <= 0)
                return false;        /* no video known yet */
            frames = new Frames(s);
        }
        final Frames f = frames;
        final int gen = ++heldGen;
        owner.removeCallbacks(framesIdle);
        final long us = Math.round(seconds * 1e6);
        seekHeld = true;
        heldSeconds = seconds;
        f.wanted = gen;
        f.on.post(new Runnable() {
            public void run() {
                final long ms = f.frameStartMs(us, gen);
                owner.post(new Runnable() {
                    public void run() {
                        framed(gen, ms);
                    }
                });
            }
        });
        owner.postDelayed(new Runnable() {
            public void run() {
                framed(gen, -1);
            }
        }, FRAME_WAIT_MS);
        return true;
    }

    /* The frames thread's answer for held seek `gen`, or the wait's end (-1): the held seek goes,
       at that millisecond or its own. NO_FRAMES ends the frames thread for good. */
    private void framed(int gen, long ms) {
        if (released)
            return;
        if (ms == NO_FRAMES) {
            framesNone = true;
            stopFrames();
        }
        if (!seekHeld || gen != heldGen)
            return;
        seekHeld = false;
        if (frames != null)
            frames.wanted = 0;
        closest(ms >= 0 ? ms : Math.round(heldSeconds * 1000.0), heldSeconds);
        if (frames != null)
            owner.postDelayed(framesIdle, FRAMES_IDLE_MS);
    }

    private void stopFrames() {
        owner.removeCallbacks(framesIdle);
        if (frames != null) {
            frames.stop();
            frames = null;
        }
    }

    /* A media's frames thread and what only it touches: a MediaExtractor on the video track,
       opened at the first scan, and that track's duration (us), the last frame's end. */
    private static final class Frames {
        final String source;
        final HandlerThread thread = new HandlerThread("RayClayMedia frames");
        final Handler on;
        volatile int wanted;   /* the held seek's number, 0 none: a scan for another stops */
        private MediaExtractor x;
        private long end = -1;
        private boolean tried;

        Frames(String source) {
            this.source = source;
            thread.start();
            on = new Handler(thread.getLooper());
        }

        /* The owner: end the thread, which releases the extractor first. */
        void stop() {
            wanted = 0;
            on.post(new Runnable() {
                public void run() {
                    if (x != null) {
                        x.release();
                        x = null;
                    }
                }
            });
            thread.quitSafely();
        }

        /* The millisecond to give SEEK_CLOSEST for a paused seek to `us` to show the video frame
           that holds it, at or before that frame's start and after the frame before's; -1 to
           take the target's own (the class comment's other seeks, or held seek `gen` went on
           without it); NO_FRAMES. */
        long frameStartMs(long us, int gen) {
            if (wanted != gen)
                return -1;   /* stopped, or the seek went on, before this scan began: no open */
            if (!tried) {
                tried = true;
                x = openVideo();
            }
            if (x == null)
                return NO_FRAMES;
            long start = -1, before = -1, next = Long.MAX_VALUE;   /* a negative time: never */
            int after = 0, read = 0;
            try {
                x.seekTo(us, MediaExtractor.SEEK_TO_PREVIOUS_SYNC);
                while (x.getSampleTrackIndex() >= 0) {   /* below 0: every sample was read */
                    if ((read++ & 15) == 0 && wanted != gen)
                        return -1;   /* the seek went on without it */
                    long t = x.getSampleTime();
                    if (t > us) {
                        next = Math.min(next, t);
                        if (++after > REORDER_MAX)
                            break;
                    } else if (t > start) {
                        before = start;
                        start = t;
                    } else if (t > before && t < start) {
                        before = t;
                    }
                    x.advance();
                }
            } catch (RuntimeException e) {
                return -1;   /* unreadable here */
            }
            long last = next != Long.MAX_VALUE ? next : end;   /* the last frame ends the track */
            long ms = start / 1000;
            if (start < 0 || us >= last || us - ms * 1000 > REWIND_MAX_US || before >= ms * 1000)
                return -1;
            return ms;
        }

        /* An extractor on the source's video track, with `end` its duration; null for no video
           track or a source it cannot read. */
        private MediaExtractor openVideo() {
            MediaExtractor e = new MediaExtractor();
            try {
                source(e, source);
                for (int i = 0; i < e.getTrackCount(); i++) {
                    MediaFormat f = e.getTrackFormat(i);
                    String mime = f.getString(MediaFormat.KEY_MIME);
                    if (mime != null && mime.startsWith("video/")) {
                        e.selectTrack(i);
                        if (f.containsKey(MediaFormat.KEY_DURATION))
                            end = f.getLong(MediaFormat.KEY_DURATION);
                        return e;
                    }
                }
            } catch (IOException | RuntimeException ex) {
                /* unreadable: every seek takes its own millisecond */
            }
            e.release();
            return null;
        }
    }

    private void doSet(float volume, boolean muted, float rate, boolean loop) {
        boolean faster = rate != this.rate;
        this.volume = volume;
        this.muted = muted;
        this.rate = rate;
        this.loop = loop;
        if (!prepared || released)
            return;
        loudness();
        mp().setLooping(loop);
        if (faster)
            speed();
        focus();
        if (started && blocked()) {   /* made audible while the focus is refused */
            mp().pause();
            time();
        }
    }

    /* An audible media plays only while the focus is held: when the platform refused it, the
       media is not started (the caller pauses one that was) and reads PAUSED / BLOCKED. */
    private boolean blocked() {
        if (focusHeld || muted || volume <= 0f)
            return false;
        started = false;
        report(PAUSED, 1, 0, 0, 0, null);
        return true;
    }

    private void loudness() {
        float v = muted ? 0f : ducked ? volume * 0.2f : volume;
        mp().setVolume(v, v);
    }

    /* The rate, pitch kept, on a playing player only: a PlaybackParams speed above 0 starts a
       paused one. */
    private void speed() {
        if (!started)
            return;
        if (sdk < 23) {
            if (rate != 1f && !rateNoted) {
                rateNoted = true;
                report(NOTE, 1, 0, 0, 0, "rcMediaSetRate needs Android 6.0 (API 23); playing at 1");
            }
            return;
        }
        try {
            PlaybackParams params = mp().getPlaybackParams();
            mp().setPlaybackParams(params.setSpeed(rate).setPitch(1f));
        } catch (IllegalStateException | IllegalArgumentException e) {
            report(NOTE, 1, 0, 0, 0, "rcMediaSetRate: the platform refused " + rate + ": " + e);
        }
        time();
    }

    private void doLend(SurfaceTexture t) {
        if (t == texture || released)
            return;
        if (texture != null)
            texture.setOnFrameAvailableListener(null);
        Surface old = surface;
        texture = t;
        surface = t != null ? new Surface(t) : null;
        if (t != null)
            t.setOnFrameAvailableListener(this, owner);
        if (mp != null)
            mp().setSurface(surface);
        if (old != null)
            old.release();
        if (prepared && !started && surface != null && !seekRunning) {
            long at = mp().getCurrentPosition();   /* the frame where it stands */
            startSeek(at == seekMs ? seekAsked : at / 1000.0, seekGen);
        }
    }

    /* Where the playhead is, for the C side to extrapolate: MediaPlayer's own anchor from
       API 23 while it plays, else its position now. */
    @SuppressWarnings("deprecation")
    private void time() {
        if (mp == null || !prepared)
            return;
        MediaTimestamp ts = started && Build.VERSION.SDK_INT >= 23 ? mp().getTimestamp() : null;
        if (ts != null) {
            long at = Build.VERSION.SDK_INT >= 29 ? ts.getAnchorSystemNanoTime()
                                                  : ts.getAnchorSytemNanoTime();
            report(TIME, Math.round(ts.getMediaClockRate() * 1000f), 0, ts.getAnchorMediaTimeUs(), at,
                   null);
        } else {
            report(TIME, started ? Math.round(rate * 1000f) : 0, 0,
                   mp().getCurrentPosition() * 1000L, System.nanoTime(), null);
        }
    }

    /* TIME again every TICK_MS while anything plays. */
    private static void tick() {
        if (ticking)
            return;
        ticking = true;
        Handler h;
        synchronized (lock) {
            h = handler;
        }
        if (h == null || !h.postDelayed(new Runnable() {
                public void run() {
                    boolean any = false;
                    ticking = false;
                    for (RayClayMedia m : live) {
                        if (m.started) {
                            m.time();
                            any = true;
                        }
                    }
                    if (any)
                        tick();
                }
            }, TICK_MS))
            ticking = false;
    }

    /* Hold the focus while something audible plays; give it back when nothing does. */
    @SuppressWarnings("deprecation")
    private static void focus() {
        boolean want = false;
        for (RayClayMedia m : live)
            want |= m.started && !m.muted && m.volume > 0f;
        if (want == focusHeld || app == null)
            return;
        if (audio == null)
            audio = (AudioManager) app.getSystemService(Context.AUDIO_SERVICE);
        if (focusListener == null) {
            focusListener = new AudioManager.OnAudioFocusChangeListener() {
                public void onAudioFocusChange(int change) {
                    focusChanged(change);
                }
            };
        }
        if (Build.VERSION.SDK_INT >= 26 && focusRequest == null) {
            Handler h;
            synchronized (lock) {
                h = handler;
            }
            focusRequest = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                    .setAudioAttributes(attributes()).setOnAudioFocusChangeListener(focusListener, h)
                    .build();
        }
        if (want) {
            int got = Build.VERSION.SDK_INT >= 26
                    ? audio.requestAudioFocus((AudioFocusRequest) focusRequest)
                    : audio.requestAudioFocus(focusListener, AudioManager.STREAM_MUSIC,
                                              AudioManager.AUDIOFOCUS_GAIN);
            focusHeld = got == AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
        } else {
            if (Build.VERSION.SDK_INT >= 26)
                audio.abandonAudioFocusRequest((AudioFocusRequest) focusRequest);
            else
                audio.abandonAudioFocus(focusListener);
            focusHeld = false;
        }
    }

    /* The legacy listener runs on the main Looper: everything here is the owner's. */
    private static void focusChanged(final int change) {
        Handler h;
        synchronized (lock) {
            h = handler;
        }
        if (h != null && Looper.myLooper() != h.getLooper()) {
            h.post(new Runnable() {
                public void run() {
                    focusChanged(change);
                }
            });
            return;
        }
        if (change == AudioManager.AUDIOFOCUS_LOSS || change == AudioManager.AUDIOFOCUS_LOSS_TRANSIENT) {
            /* Not held from here: the next audible play asks again, which a call refuses. */
            focusHeld = false;
            pauseAll(false);
        } else if (change == AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK || change > 0) {
            ducked = change < 0;
            for (RayClayMedia m : live) {
                if (m.prepared && !m.released)
                    m.loudness();
            }
        }
    }

    /* Every playing media paused by the platform, or with `audibleOnly` (the output is gone)
       every audible one (as focus() counts it): each reads PAUSED. Every other media hears it
       too (with `audibleOnly` one still preparing, whatever its volume, as neither reaches this
       side before it is prepared): the C side's core drops the pending play of one whose READY
       it has not applied, any one's for a pause of every media (HALT), an audible one's for the
       output. */
    private static void pauseAll(boolean audibleOnly) {
        for (RayClayMedia m : live) {
            boolean audible = !m.muted && m.volume > 0f;
            if (m.started && m.prepared && !m.released && (audible || !audibleOnly)) {
                m.started = false;
                m.mp().pause();
                m.report(PAUSED, 0, 0, 0, 0, null);
                m.time();
            } else if (!m.released && (!audibleOnly || !m.prepared)) {
                m.report(PAUSED, audibleOnly ? 0 : 2, 0, 0, 0, null);
            }
        }
        focus();
    }

    /* pauseAll, posted to the owner thread from any other (the platform's callbacks come on the
       app's main Looper); nothing once it has shut down. */
    private static void pauseAllSoon(final boolean audibleOnly) {
        Handler h;
        synchronized (lock) {
            h = handler;
        }
        if (h != null) {
            h.post(new Runnable() {
                public void run() {
                    pauseAll(audibleOnly);
                }
            });
        }
    }

    /* The app's activities stop when it leaves the foreground. */
    private static final class Lifecycle implements Application.ActivityLifecycleCallbacks {
        public void onActivityStopped(Activity activity) {
            pauseAllSoon(false);
        }

        public void onActivityCreated(Activity activity, Bundle state) { }
        public void onActivityStarted(Activity activity) { }
        public void onActivityResumed(Activity activity) { }
        public void onActivityPaused(Activity activity) { }
        public void onActivitySaveInstanceState(Activity activity, Bundle state) { }
        public void onActivityDestroyed(Activity activity) { }
    }

    /* The sound is about to move to the speaker (the class comment's HEADPHONES). */
    private static final class Noisy extends BroadcastReceiver {
        public void onReceive(Context context, Intent intent) {
            if (AudioManager.ACTION_AUDIO_BECOMING_NOISY.equals(intent.getAction()))
                pauseAllSoon(true);
        }
    }

    /* ---- MediaPlayer's and SurfaceTexture's listeners: the owner thread ---- */

    public void onPrepared(MediaPlayer p) {
        if (p != mp)
            return;
        prepared = true;
        width = mp().getVideoWidth();
        height = mp().getVideoHeight();
        int duration = mp().getDuration();
        loudness();
        mp().setLooping(loop);
        report(READY, width, height, duration, 0, null);
        time();
        if (surface != null && !started)
            startSeek(0.0, seekGen);   /* frame 0, before anything is asked for */
    }

    public void onCompletion(MediaPlayer p) {
        if (p != mp || !started)
            return;
        started = false;
        report(ENDED, 0, 0, 0, 0, null);
        time();
        focus();
    }

    public boolean onError(MediaPlayer p, int what, int extra) {
        if (p != mp)
            return true;
        if (!prepared && alt != null && source.equals(src)) {
            report(NOTE, 0, 0, 0, 0, name(src) + ": MediaPlayer error " + what + "/" + extra
                   + " before it was ready: playing .alt, " + name(alt));
            mp().release();
            mp = null;
            load(alt);
            return true;
        }
        /* A missing file failed at setDataSource (load): what reaches here was found. */
        int error = what == ERROR_SERVER_DIED ? UNAVAILABLE
                  : extra == ERROR_UNSUPPORTED ? UNSUPPORTED
                  : (extra == ERROR_IO || extra == ERROR_TIMED_OUT) && isUri(source) ? NETWORK
                  : DECODE;
        report(FAILED, error, what, extra, 0, name(source) + ": MediaPlayer error " + what + "/" + extra);
        boolean was = started;
        release();
        if (was)
            focus();
        return true;
    }

    public boolean onInfo(MediaPlayer p, int what, int extra) {
        if (p == mp && (what == BUFFERING_START || what == BUFFERING_END))
            report(WAITING, what == BUFFERING_START ? 1 : 0, 0, 0, 0, null);
        return false;
    }

    public void onSeekComplete(MediaPlayer p) {
        if (p != mp)
            return;
        seekRunning = false;
        if (seekWaiting) {
            seekWaiting = false;
            startSeek(seekNext, seekNextGen);
            return;
        }
        time();
        report(SEEKED, seekGen, 0, 0, 0, null);
    }

    public void onVideoSizeChanged(MediaPlayer p, int w, int h) {
        if (p != mp || w <= 0 || h <= 0 || (w == width && h == height))
            return;
        width = w;
        height = h;
        if (prepared)
            report(SIZE, w, h, 0, 0, null);
    }

    public void onFrameAvailable(SurfaceTexture t) {
        if (t == texture)
            report(FRAME, 0, 0, System.nanoTime(), 0, null);
    }
}
