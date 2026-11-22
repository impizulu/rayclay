#!/usr/bin/env python3
"""make-harbor.py - draw the film ex21_media_library plays: a harbor at dawn.

Every frame is computed here (numpy), and the sound is two ffmpeg sources: a
held chord and a filtered noise for the sea. Nothing is sampled or recorded,
so the files are as free to copy as this script. Writes, beside it:

    harbor.mp4    H.264 Main + AAC: what Apple's AVPlayer and every browser play
    harbor.webm   VP9 + Opus: the second source (the app's .alt), for a player without H.264

Usage: python3 make-harbor.py      (needs numpy and an ffmpeg with libx264,
                                     libvpx-vp9 and libopus)
"""
import os
import subprocess
import numpy as np

W, H, FPS, SECONDS = 360, 640, 24, 12.0
HORIZON = 430
HERE = os.path.dirname(os.path.abspath(__file__))

# The sky's top and horizon colors at four moments of the dawn (0..1).
KEYS = np.array([0.0, 0.35, 0.6, 1.0])
TOP = np.array([[8, 10, 35], [40, 35, 95], [70, 110, 190], [90, 160, 235]], float)
LOW = np.array([[25, 25, 70], [230, 120, 90], [250, 190, 140], [180, 220, 250]], float)

rng = np.random.default_rng(29)
STARS = np.column_stack([rng.integers(0, W, 70), rng.integers(0, HORIZON - 120, 70),
                         rng.uniform(0, 6.3, 70)])
ys, xs = np.mgrid[0:H, 0:W].astype(float)
# A headland on the left, darker than any sky behind it.
HILL = HORIZON - 46 * np.exp(-((xs[0] - 50) / 80) ** 2) - 18 * np.exp(-((xs[0] - 150) / 45) ** 2)


def mix(keys, p):
    return np.array([np.interp(p, KEYS, keys[:, c]) for c in range(3)])


def sky_at(row, top, low):
    f = np.clip(row / HORIZON, 0.0, 1.0)[..., None] ** 1.5
    return top * (1 - f) + low * f


def frame(t):
    p = t / SECONDS
    top, low = mix(TOP, p), mix(LOW, p)
    img = sky_at(ys, top, low)

    rise = 1 - (1 - min(max(p - 0.12, 0.0) / 0.7, 1.0)) ** 2   # from 12 %, easing to a stop at 82 %
    sx, sy = W / 2, HORIZON + 50 - rise * 300
    d = np.hypot(xs - sx, ys - sy)
    glow = np.exp(-(d / 110) ** 2)[..., None] * np.array([255, 160, 100]) * 0.55
    disc = (d < 34)[..., None] * np.array([255, 236, 196])
    sky = ys < HORIZON
    img = np.where(sky[..., None], np.maximum(img + glow, disc), img)

    night = max(0.0, 1 - p * 3)
    for x, y, ph in STARS:
        img[int(y), int(x)] += 200 * night * (0.6 + 0.4 * np.sin(t * 3 + ph))

    hill = (ys >= HILL[None, :]) & sky
    img[hill] = np.array([14, 16, 34]) + (low - [14, 16, 34]) * 0.12

    sea = ~sky
    mirror = np.clip(2 * HORIZON - ys, 0, HORIZON - 1)
    water = sky_at(mirror, top, low) * 0.5 + np.array([5, 15, 35])
    ripple = 0.5 + 0.5 * np.sin(ys * 0.45 - t * 5 + np.sin(xs * 0.07))
    spread = 18 + np.maximum(ys - HORIZON, 0) * 0.45
    path = np.exp(-((xs - sx) / spread) ** 2) * ripple * (sy < HORIZON)
    water = water + path[..., None] * np.array([255, 200, 140]) * 0.7
    img = np.where(sea[..., None], water, img)
    return np.clip(img, 0, 255).astype(np.uint8)


def encode(name, vcodec, acodec, frames):
    # The sound is made inside ffmpeg: a held A major chord that breathes, and
    # brown noise under a low-pass for the sea, faded in and out.
    audio = ("aevalsrc=0.07*sin(2*PI*110*t)+0.05*sin(2*PI*164.81*t)+0.04*sin(2*PI*220*t)"
             "+0.03*sin(2*PI*277.18*t):s=48000:d={d},tremolo=f=0.25:d=0.3[chord];"
             "anoisesrc=color=brown:amplitude=0.18:d={d}:r=48000:seed=29,lowpass=f=500,"
             "tremolo=f=0.12:d=0.7[sea];[chord][sea]amix=inputs=2:normalize=0,"
             "afade=t=in:d=1.5,afade=t=out:st={out}:d=2,aformat=channel_layouts=stereo[a]"
             ).format(d=SECONDS, out=SECONDS - 2)
    cmd = (["ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
            "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", "%dx%d" % (W, H), "-r", str(FPS), "-i", "-",
            "-filter_complex", audio, "-map", "0:v", "-map", "[a]"]
           + vcodec + acodec + ["-t", str(SECONDS),
                                # no random segment id or encoder stamp: a rerun writes the same bytes
                                "-fflags", "+bitexact", "-flags:v", "+bitexact", "-flags:a", "+bitexact",
                                os.path.join(HERE, name)])
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    for f in frames:
        proc.stdin.write(f.tobytes())
    proc.stdin.close()
    if proc.wait() != 0:
        raise SystemExit("ffmpeg failed for " + name)


def main():
    frames = [frame(i / FPS) for i in range(int(SECONDS * FPS))]
    encode("harbor.mp4", ["-c:v", "libx264", "-threads:v", "1", "-profile:v", "main", "-pix_fmt", "yuv420p",
                           "-crf", "30",
                           "-preset", "slow", "-movflags", "+faststart"], ["-c:a", "aac", "-b:a", "64k"],
           frames)
    encode("harbor.webm", ["-c:v", "libvpx-vp9", "-crf", "40", "-b:v", "0", "-row-mt", "1",
                            "-pix_fmt", "yuv420p"], ["-c:a", "libopus", "-b:a", "48k"], frames)


if __name__ == "__main__":
    main()
