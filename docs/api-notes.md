# RayClay API notes: the measurements behind the cheatsheet

`docs/cheatsheet.md` is a **quick reference**, in the shape of the
[raylib cheatsheet](https://www.raylib.com/cheatsheet/cheatsheet.html) that inspired it: one line per
entry, a short trailing comment, and nothing you have to read twice. This page is where the long-form
material lives (the measurements, the traps, the "why is this number what it is"), so the card can stay
a card.

**Everything here was measured, not reasoned.** Where a figure is quoted, the box, the counter, the build
type and the date are named with it, because a magnitude that cannot say where it came from is not
transferable. Directions transfer between machines; magnitudes never do.

**This page does not define the API.** Every public function, type, field and constant is named on the
cheatsheet, and that is checked automatically. If a symbol appears only here, that is a bug in the card.

---

## Memory: how much of your app is actually RayClay?

*Referenced from the cheatsheet's build-knob section.*

▸▸ **Before you touch any dial below: most of your app's memory is not RayClay, and you cannot tell
without a control.** Run a bare window + GL context with *your* creation parameters, built against
the same windowing library your app links, and subtract it. The
absolute floor is a property of the GL driver, the compositor and the swapchain far more than of
any toolkit, measured on two machines with two different counters:
  macOS M3 Pro, phys_footprint (unified memory, so GPU is *inside* the number),
  800x600, drawing nothing, three runs of medians-of-3 (bands are the observed spread).
  **These rows were taken with a GLFW control against a GLFW-hosted build**, which is the desktop
  default. Treat the shape as durable and re-measure the absolute values on your own target:
      bare window, no GL context                     ~20 MiB
      + GL 4.1 core, MSAA 0, depth/stencil 0        55-63 MiB
      + MSAA 4 (RayClay's default)                 116-124 MiB   <- +53 to 61 MiB
      + RayClay itself                             126-130 MiB   <- +5 to 14 MiB
  Fedora + discrete NVIDIA, cgroup v2 non-reclaimable, VRAM read separately:
      bare GL 12.24 MiB host / 16 MiB VRAM;  RayClay 14.51 / 19.
      bare GL is 84% of RayClay on both sides of the memory boundary.
**The direction transfers, the magnitude never does:** ~90% and 84% are the same finding in two
  memory architectures, not a disagreement. Quote neither as "RayClay's footprint"; quote the
  *delta* you measured on the box you measured it on.
**The biggest line item on macOS is the 4x MSAA framebuffer, and it is a knob, not a cost of using
  RayClay.** `RC_GFX_MSAA_SAMPLES` defaults to 4; on this box that framebuffer is worth more than the
  GL context and roughly five times everything RayClay itself allocates. If memory matters more than
  edge quality on your target, that is the first dial to turn, and it is the same dial on every
  platform, so measure it before you go looking for allocations.
**Four rows instead of two, because two subtractions can go wrong in opposite directions.** Run the
  control *without* MSAA against a RayClay build that defaults to 4x and the library is charged for
  the framebuffer: "RayClay's marginal cost is 56-65 MiB", almost exactly the MSAA framebuffer. Set
  MSAA 4 but leave a default 24-bit depth + 8-bit stencil in, which RayClay explicitly reclaims
  (it never samples either), and the control comes out *higher* than the subject, making the library
  look like it *saved* 32 MiB. Both numbers are the control's fault.
  **So: a control is not "the same kind of program".** It is the same *creation parameters*, all of
  them: sample count, depth bits, stencil bits, profile, scale-to-monitor. Copy them from the
  source rather than choosing them, and if a subtraction comes out negative or suspiciously large,
  suspect the control before you believe the finding.
**Do the arithmetic on that Fedora line, and do it twice, because it answers two different
  questions and mixing them is a 6x error:**
    "what does the process hold?"    19 MiB device + 14.51 MiB host. More than half of it is
                                     invisible to the host counter most people quote, so on a
                                     discrete GPU you need two readings or you have none.
    "what does RayClay cost?"        19 − 16 = ~3 MiB marginal device, because a bare GL window
                                     holds 16 MiB before any toolkit exists.
  **"RayClay uses 19 MiB of VRAM" is the wrong sentence**: it is the total, and ~84% of it is
  the GL context. It is the same "quote the *delta*" rule as the line above, on the device side.
  *(The total and the control have both been measured independently on the same class of box:
  19.004 MiB device against 14.653 MiB host. Both that number and the ~3 MiB are correct and they
  answer different questions. Quote the total and the control together, or the total will be read
  as RayClay's device cost.)*
  **And the ~3 MiB is now decomposed,** by counting what RayClay *asks* the GPU for at link time
  (`--wrap` on sg_make_image/sg_make_buffer, which is GNU-ld only, so quote it as "GPU bytes requested",
  never as VRAM: it is demand, not residency, and it does not exist on macOS or Windows):
      images   1,048,580 B   the 1024x1024 R8 glyph atlas + a 1x1 white RGBA8
      buffers  2,162,688 B   packet vertex + index buffers
      TOTAL    3,211,268 B = 3.063 MiB
  **Two instruments sharing no mechanism agree:** 19.004 total minus the ~16 a bare GL window holds
  gives ~3, and the link-time count that never touches the device gives 3.063. Believe it more than
  you would believe either alone.
  **So "the atlas is RayClay's GPU cost" is measurably wrong**: the packet *buffers* are 2.06 of the
  3.06 MiB, larger than the atlas. ⇒ ~84% of the 19 MiB is driver context, swapchain, shader
  compiler and allocator rounding: real, but not RayClay's to shrink.
  **The sharp edge nothing host-side guards:** a pixel-format change R8 -> RGBA8 would 4x the
  atlas's demand with *zero* host-heap movement, so a measurement of peak *heap* alone stays flat
  through it, and so does a count of live pool entries, because *alive-counts* do not move when
  bytes do. Only a count of requested bytes sees it.
**And the spread is an architecture property too.** The same experiment moved 21.5 MiB run-to-run
  under ri_phys_footprint and 0.02 MiB under the Linux cgroup counter: ~400x. Report your spread
  beside your number, or you cannot tell an effect from noise: a 341 KB binary saving is invisible
  inside a 21 MiB spread, and "no effect" is then a statement about your instrument, not the code.
**Reserved is not resident, and the gap is easy to miss.** A bare window + GL
program sits at 20 MiB after glBufferData(16 MiB, NULL), and jumps to 36 MiB the moment it writes
48 bytes into that buffer. Allocation is free; **the first touch commits the whole thing**. So a pool's
declared capacity is an honest *upper bound* on what it costs you, never a reading of what it costs.
**First-ever launch is not steady state.** On a cold GL shader cache the driver compiles the shaders at
startup and the cost lands in your process: measured on NVIDIA by an external benchmarking team,
+12.7 MiB for a window that only calls glClear and +50.0 MiB for RayClay, both vanishing once the
cache is warm. If your harness hands each run a fresh HOME/XDG_CACHE_HOME for isolation, you are
measuring shader compilation, not your app. Driver-specific: verify before porting the numbers.

## rcRegisterFont / rcFont

`rcRegisterFont` bakes a font and indexes it by (family, weight, size); size rounds to the nearest
pixel. **The dedup key is that triple, not the path.** Re-registering the same
(family, weight, size) from the same path is idempotent: it reuses the baked face and costs no
slot. A *different* path under that triple re-bakes and swaps the mapping, releasing the face the
row stopped naming. The same file under a different family, weight or size is not a repeat at all:
it is a fresh bake and a fresh slot.
`rcFont` resolves a registered (family, weight, size) back to its fontId, rounding size the same way.

**One file per weight. The `weight` argument labels the file you give it; it does not select a
weight inside it.** Point two weights at one file and both bake that file's outlines:

```c
rcRegisterFont("Roboto", RC_WEIGHT_REGULAR, "Roboto-VariableFont_wght.ttf", 28);
rcRegisterFont("Roboto", RC_WEIGHT_BOLD,    "Roboto-VariableFont_wght.ttf", 28);  // NOT bold
```

Both calls succeed and return *different* ids, `rcFont`(..., `RC_WEIGHT_BOLD`, ...) resolves the second,
and your bold text is Regular.
**This is not a variable-font problem**: handing a *static* Regular file to `RC_WEIGHT_BOLD` does exactly
the same thing. The rule is one *file* per weight, whatever its flavour.

**It tells you:** registering one path under a second weight logs a warning naming both
weights ("`rcRegisterFont`: '<path>' is already registered at weight 400 and is now also registered at
weight 700 …"). It fires **once per weight**, not per call (a 2-weight x 5-size ladder logs it once),
and the face still loads, so this is a warning, not a refusal.
Measured 2026-08-09 on Roboto, Inter, Open Sans and Nunito: the two variable rows are identical to
the ink pixel, while the same test on the *static* files separates cleanly (Roboto 1481 px regular vs
2118 bold).
**The fix is the file, not the call:** use the per-weight statics. A Google Fonts download puts them
in the `static/` subfolder while the variable file sits at the top level, so the file you reach for
first is the one that cannot work.

**A path that FAILED is remembered, so retrying the same call is not a retry.** The row is kept, the
family resolves to the default face, and an identical later call returns `0` again without reopening
the file and without a second warning: the "cannot read" line from the first attempt is the only
notice you get. That matters when the file might appear later, as with an asset download or a
corrected working directory. Repeating the call will not pick it up. Register it from a **different
path string**, which takes the swap path and re-opens.

## The font ladder (RC_AppOptions.fontPath / fontSizes / fontCount)

Declarative setup is the usual way in, and what every bundled example does: declare the ladder on
`RC_AppOptions` and the runner bakes it before the first frame.

```
.fontPath + .fontSizes + .fontCount     // bake ONE face at each listed pixel size
```

**The i-th size becomes fontId i: the load-order index, not the size value.** Name the slots and pass
the name:

```c
enum { F_SMALL, F_BODY, F_H1, F_COUNT };  static const float sizes[F_COUNT] = { 13, 16, 28 };
.fontSizes = sizes, .fontCount = F_COUNT   =>   rcTextL("Title", .font = F_H1)   // NOT .font = 28
```

.fontPath = NULL *with* .fontSizes bakes the bundled face at each of those sizes: a crisp ladder
with zero asset files.
.fontPath without .fontSizes/.fontCount is ignored (warns once) and you get the bundled face:
your font silently never loads.

Neither .fontPath nor .fontSizes set gives you the bundled Latin-1 Roboto subset at one default size
as fontId 0, so text always renders out of the box.
A bake that fails mid-ladder does not consume its slot, which **shifts every later id**; the runner
warns loudly, once per drifted size, and the line names the **size, the slot it landed in and the
index it was expected at** - `rcAppCreate: font 18.0px baked to slot 2, expected 3; later .font ids will be off`.
It does not name the file, so grep the log for `expected`, not for your font's path.

Two tables of 16 sit behind this, and it is worth knowing which one you are filling. `RC_MAX_FONTS`
counts BAKES: one per registration, keyed by the file and the size, with no de-duplication, so two
families naming the same file at the same size still cost two. A second table of 16 holds the
(family, weight, size) names `rcFont` resolves. A registration takes one row of each, so a
4-weight x 5-size ladder exhausts both. Over-cap registration is diagnosed (ERROR/WARN) at load. Plan your weight x
size matrix.

## rcProcessMemoryBytes / rcProcessPeakMemoryBytes / rcProcessCpuPercent

`rcProcessMemoryBytes` returns this process's resident memory: RSS (Linux), working set (Windows),
Mach resident size (macOS), WASM heap (web). 0 means the platform offers no reading. Coarse and
cheap: a dashboard sample, *not* an allocator-grade profile.

`rcProcessPeakMemoryBytes` returns the *high-water mark* of that same figure over the whole life of
the process: `PeakWorkingSetSize` (Windows), `VmHWM` (Linux), Mach `resident_size_max` (macOS). It is
read from the kernel's own counter rather than sampled, because a peak that matters is usually brief
(an atlas bake, a resize, one oversized frame) and a sampler slower than the spike simply misses it.
There is no reset, deliberately: the OS counters have none, and emulating one would make "peak" mean
something different from what every platform tool reports for the same process. On web the WASM heap
only grows, so it equals `rcProcessMemoryBytes`.

Validated against the platform's own instrument rather than asserted: on Linux it agreed with
`/usr/bin/time -v` to the **byte** on a 200 MiB allocate-touch-free probe. One surprise worth knowing
before you report it as a bug - **`VmHWM` can read *lower* later**. Linux reports
`max(stored hiwater, current RSS)` and refreshes the stored value lazily, so a read taken while the
peak is still resident can exceed the one that survives it. `/usr/bin/time` shares the limitation
exactly and reported the same lower number, so RayClay is neither better nor worse than the tool you
would check it against.

`rcProcessCpuPercent` returns CPU since the *previous* call, as a % of one core (like `top`, so >100
means multiple cores busy).
The first call returns 0 (no interval yet); web returns -1, because a browser tab has no OS
process view; `rcProcessMemoryBytes` still works there.
**A third reading exists and it is easy to misdiagnose: -1 on the desktop, from the very first call.**
That means the implementation was compiled in strict-ANSI mode, so `CLOCK_MONOTONIC` was never
declared and the monotonic clock fell through to its unavailable branch. The usual cause is a
directory-scope `set(CMAKE_C_EXTENSIONS OFF)` in your own `CMakeLists.txt`, which is inherited by
RayClay's target through `add_subdirectory`/`FetchContent`. Put your standard settings on your own
target instead; see [getting-started.md](getting-started.md) ▸ Build. `rcProcessMemoryBytes` is
unaffected either way.
Sample at a steady ~1 Hz: OS accounting is 1-10 ms granular, so per-frame calls read noise and
back-to-back calls (<1 ms) re-return the previous reading.
**Call it from exactly one place.** The reading is process-wide and single-sampler: the interval it
  reports is the gap since the previous call *from anywhere*, so two callers polling on their own
  schedules do not each get a series. They interleave, shorten each other's intervals and both read
  low, with nothing in the output saying so. A debug overlay plus a telemetry tick is the shape that
  does it. There is deliberately no reset, because a reset would only help a second sampler and the
  answer to a second sampler is to have one instead. It is not thread-safe either: the previous
  sample is shared state with no lock, so concurrent calls can tear it. Sample once, fan the value out.
**0 is a reading, not a sentinel.** The first call returns 0 because there is no interval yet, and
  so does any interval in which the process used no measurable CPU - the right answer for an
  on-demand app that parked. Only **-1** means no reading is available.


## rcMachineMemoryBytes / rcMachineCpuPercent / rcUnixTimeSeconds

The machine-level half of live telemetry, each with the platform oracle its contract names.

`rcMachineMemoryBytes(&total, &available)` reports whole-machine RAM. `total` is what each OS
reports: Linux `MemTotal` (usable RAM, which excludes what the kernel and firmware reserve, so it is
a little under the installed size), Windows `ullTotalPhys`, macOS `hw.memsize` (the installed size).
`available` is an estimate: Linux `MemAvailable`, Windows `ullAvailPhys`, and on macOS RayClay's OWN
free-plus-inactive figure, because macOS publishes no such number (it is not Activity Monitor's
"Memory Used" complement; a native oracle now grades it - see the matrix below). It
returns `false` and writes 0 to both when the platform offers no reading (the web), when only one
figure is present, or when a figure does not fit `size_t` (a 32-bit build on a bigger box); it never
guesses or narrows. Either pointer may be NULL. Sample coarsely (about 1 Hz). Oracles for the total,
measured exact on all three desktops: `free -b`, `sysctl hw.memsize`, `Get-CimInstance
Win32_OperatingSystem`.

`rcMachineCpuPercent` is whole-machine busy time since the *previous* call, all cores = 100
(`/proc/stat` with busy = everything but idle and iowait, `host_statistics`, `GetSystemTimes`). The
first call has no interval and returns 0; where there is no reading it returns -1.

**`-1` IS NOT WEB-ONLY: ON ANDROID IT IS PERMANENT.** An Android app cannot read `/proc/stat` -
SELinux denies it to the `untrusted_app` domain - so the whole-machine figure is unavailable **by
platform policy**, and no future RayClay version can supply it. Measured on a physical Pixel 8a on
2026-09-19, with a control in the same app proving that process *can* read `/proc/meminfo` and
`/proc/self/*`. **Do not paper over it with a per-app or per-core number**: that returns a plausible
figure answering a different question, which is worse than an honest `-1`. The other machine
readings on this page do work on Android. **iOS is not the same case: it returns a whole-machine
figure rather than Android's permanent `-1`.** A physical iPhone 15 plots a live series and names
its source `real`. That is AVAILABILITY, not accuracy - there is no iOS oracle in the recipe below
and no comparison has been run against one - so the matrix still reads unverified. Treat the iOS
number as **present and unvalidated**, which is not the same as absent, and not the same as macOS's
figure either: that one has a recipe and this one does not. Design the card so `-1` reads as "not
available here", not as a value.

`rcMachineCpuPercent` is process-wide and single-sampler like `rcProcessCpuPercent`, and not
thread-safe; back-to-back calls (< 1 ms) re-return the previous reading, and a counter that did not
advance re-baselines and returns it. On Windows `GetSystemTimes` covers only the calling thread's
processor group, so a machine with more than 64 logical processors (more than one group) gets -1
rather than one group presented as the machine. Oracles: `mpstat 1 1`, `top -l
1`, `typeperf "\Processor(_Total)\% Processor Time" -sc 1` (`wmic` is gone on Windows 11 25H2).

`rcUnixTimeSeconds` is civil time: seconds since 1970-01-01T00:00:00Z from the real-time clock,
fractional where the platform provides it. It is a wall clock, not a timer: it moves whenever the
system clock is set, stepped by NTP or corrected after a suspend, and it ignores the time-zone
setting because UTC has no zone. For intervals, deadlines and animation use `rcAppTime`, which is
finite and never decreases. Returns 0 where the platform offers no reading. Oracle: `date -u +%s` at
the same instant, within one second.

### Which of these is MEASURED where, and which is not

A reading that is "supported" and a reading that has been **graded against the platform's own
tool** are different claims, and only the second one is worth anything to you. This matrix says
which is which, per platform, in three words and nothing softer:

- **measured** - RayClay's number was compared in-process against a native oracle, on that OS, and
  the arm carries a control that FAILS when the number is wrong.
- **unavailable-by-contract** - the OS will not give it to us and no version of RayClay can. The
  API says so with a documented sentinel; it is not a gap to be filled later.
- **unverified** - plausible, probably fine, NOT measured on that platform. Treat it as untested.

| Reading | Linux | macOS | Windows | Android | iOS | Web |
|---|---|---|---|---|---|---|
| `rcMachineMemoryBytes` total | measured (exact) | measured (exact) | measured (exact) | measured (dated) | unverified | unavailable-by-contract (`false`, 0/0) |
| `rcMachineMemoryBytes` available | measured | measured | measured | measured (dated) | unverified | unavailable-by-contract (`false`, 0/0) |
| `rcMachineCpuPercent` | measured | measured | measured | **unavailable-by-contract (`-1`)** | unverified | unavailable-by-contract (`-1`) |
| `rcUnixTimeSeconds` vs libc | measured | measured | measured | measured (dated) | unverified | unverified |
| `rcUnixTimeSeconds` TZ-invariance | measured | measured | measured | unverified | unverified | unverified |
| `rcUnixTimeSeconds` across a wall-clock STEP | measured | **unverified** | **unverified** | unverified | unverified | unverified |

"measured (dated)" is the Android column, and it means exactly that: read off a physical Pixel 8a
on 2026-09-19 against `/proc/meminfo` and `date -u`, at the precision the on-screen
card prints (0.1 GiB), and NOT re-run for this release. Its permanent `-1` for whole-machine CPU
has a matched control in the same frame - five readings rendered real while that one rendered
"no reading" - which is why the other cells in that column mean something.

The desktop row is a run, not a recollection: one telemetry probe executed on all three desktops
from the same source, at one revision, on 2026-09-20. Linux 18 checks / 0 failed; macOS 20 / 0,
twice; Windows 19 / 0, twice.

**Each "measured" cell carries the wrong answer it can reject**, because an arm that only ever sees
the right number cannot tell you it works:

- macOS available: the probe re-derives free + inactive from `host_statistics64` either side of the
  call and brackets the reading, then checks that the **free-pages-only** figure - the natural wrong
  reading of the word "available" - falls OUTSIDE that bracket. Measured: 3,588,833,280 against a
  window of 8,960,213,647 to 9,733,307,761. A free-only implementation fails this arm.
- Windows available: the oracle is `GetPerformanceInfo`, a different kernel surface from the
  `GlobalMemoryStatusEx` the implementation reads, and the rejected neighbour is **commit
  headroom** (`CommitLimit - CommitTotal`), which is what Task Manager shows and what a reader
  reaches for. Measured: 10,781,884,416 against a window of 9,610,687,939 to 10,286,099,005.
- Both totals are EXACT, not bracketed, and against an independent source: `sysctl hw.memsize`
  = 19,327,352,832 on the mac, `GetPerformanceInfo.PhysicalTotal` = 16,885,276,672 on Windows,
  which also equals `Get-CimInstance Win32_ComputerSystem.TotalPhysicalMemory` in the same receipt.

**Reconciling the macOS number with `vm_stat` by hand: add the speculative row.** RayClay's macOS
`available` is `(free_count + inactive_count) x page size` from Mach, and **Mach's `free_count`
includes speculative pages while the `vm_stat` COMMAND subtracts them and prints them separately.**
So the hand check is *Pages free + Pages speculative + Pages inactive*. Measured 2026-09-20 on an
18 GiB Apple-silicon mac, 16 KiB pages: RayClay reported 570,481 pages against 571,379 and 572,403
from `vm_stat` taken either side of the call - a shortfall of 898 to 1,922 pages, which is the
probe's own resident footprint (17,760,256 B = 1,084 pages, from `/usr/bin/time -l`). Comparing
against *Pages free + Pages inactive* alone leaves you 44,879 pages - 735 MB - short, and that
difference is the whole speculative cache, not an error.

**The one honestly open desktop cell is the wall-clock STEP.** The gate that proves the civil clock
survives a discontinuity injects the step with a glibc `LD_PRELOAD` shim, so it runs on Linux and is
an explicit expected-skip on macOS and Windows. Those two platforms are graded
for clock SOURCE behaviour (monotonic vs wall, TZ invariance, agreement with libc and with
`date -u +%s`) and are **unverified for the step itself**. A green Apple or Windows suite is not
evidence about it, and this page will not pretend otherwise.

## rcUnloadFont

**The slot table is not one-way.** `RC_MAX_FONTS` is 16 including the bundled default, so without
this a font browser would hit `table full` and every later load would silently render in the default
face. `rcUnloadFont` gives the slot back.

Returns false, changing *nothing*, for: id 0, an id never loaded, one already unloaded, and a call
made while the glyph sheet is **frozen**. id 0 is refused deliberately - it is both the default face
and `rcLoadFont`'s failure return.

**The frozen window is wider than "during a draw pass", which is the trap.** The sheet freezes on a
frame's *first* text draw and thaws only when the *next* frame begins, so a `frameEndCallback`, and
anything after your last frame, is refused too. **Unload between frames, and branch on the return
rather than assuming it took.**

**The slot buys back at once; atlas bytes come back on demand.** Unloading marks the sheet stale
rather than repacking, so atlas usage does not fall at the moment you unload - if you are watching
for an effect, measure slots, not bytes. The reclaim happens at the next load that cannot pack.

**The freed slot comes back with the SAME id number**, and that is the sharp edge: unload id 1, load
another face, and you get id 1 again - a different face wearing the number you still hold. Nothing
errors. ⇒ **Treat an id as dead the moment you unload it**, exactly as you would a freed pointer. It
also drops any `rcRegisterFont` rows that resolved to that id, so `rcFont` stops finding them.

### 0 is a valid id, so `if (!id)` is the wrong check

The id is an **ordinal**, not a size, so the first sized load after init legitimately returns 0: 12 px
first returns 0; 16 px first returns 0 and 12 px then returns 1. 0 is also the default slot every
failure degrades to. **Every failure logs, so detect a failed load from the log (`rcSetLogSink`),
never from the return value.**

### Branch on the LEVEL, not on the wording

Across the whole font path an **ERROR means refused** - you were handed the default slot - and a
**WARNING means the face loaded** and something about it is worth knowing. The message is only the
cause, and that list is open-ended. The prefixes are a family that all begin `rc_font`, `rcFont` or
`rcRegisterFont`, and the set is not closed, so **match the family and branch on the level; never
string-equal a remembered list.**

```
ERROR - refused, you hold the default slot
  rc_font_load: cannot read '<path>'     the path did not resolve. NOTE THE DIFFERENT PREFIX:
                                         a sink matching only "rc_font:" misses this entirely
  rc_font: table full (16)               out of slots. rcLoadFont does NOT de-duplicate by path,
                                         so a call in a loop exhausts the table - load once at
                                         startup. rcRegisterFont dedups, but only on the exact
                                         (family, weight, size) key. rcUnloadFont frees one
  rc_font: atlas full packing @ Npx      THE FACE IS DROPPED, not degraded. Too big for the
                                         sheet even after the internal retry. Ask for a smaller
                                         size, or build with a larger RC_FONT_ATLAS_W / _H
  rc_font: bad font data                 damaged, truncated, or not a font. Re-download it
  rc_font: unsupported font (...)        valid structure this build cannot open. Three causes,
                                         all named in the message: a COLLECTION (.ttc/.otc),
                                         CFF2 outlines, or a symbol-only cmap. A collection is
                                         the likely one - stock CJK faces often ship as .ttc.
                                         Supply a single-face .ttf or .otf

WARNING - the face LOADED and is usable
  rc_font: N glyph(s) skipped ...        minus N glyphs judged unsafe; they bake blank at their
                                         true advance, so your layout does not shift
  rc_font: ... sharpened at NxN instead  baked lower than you asked. Text is softer, not
                                         missing. Load fewer or smaller faces, or lower
                                         RC_AppOptions.fontOversample
  rc_font: symbol-encoded face (...)     its ASCII lives in the private use area, so letters
                                         come out as symbols. Expected for a dingbat face
```

⇒ **Do not read every refusal as a damaged file, and do not read a WARNING as a failure.** A refusal
of the face itself is the exception; the common refusals are the path, the slot table and the atlas.

**`CFF2` is not the same as "variable", and the difference decides whether your font works.** A
variable font built on `glyf` loads and draws normally; only the `CFF2` flavour cannot be read at
all. ⇒ Check the table, never the filename.

**Trusted fonts only.** RayClay bounds the offset table, table directory, cmap, the `glyf` outlines
and the `CFF ` container, but the Type-2 charstring program a CFF font executes is still unbounded.
Bundle the fonts you ship; never hand this a file a user supplied.

## rcUnloadImage

**The one lifecycle rule**, and skipping it *breaks the app* rather than merely fattening it:
`rcLoadImage` **decodes and uploads on every call**; it does not cache by path. Assign a second `RC_Image`
over a live one and the first texture is unreachable forever. **Free, then load.**
**There is a ceiling of 128 live images.** Measured: reloading without freeing succeeds 126 times,
then *every* further `rcLoadImage` fails for the life of the process and .handle comes back NULL. The
same 200-frame run that frees first: 0 failures. Exhaustion **logs once** naming the cause
and the remedy, so you get a sentence rather than a mystery NULL. Host memory leaks alongside it
(~2.3 MB/load), but the pool is what actually ends the app.
**Do not plan around 128 growing.** A slot costs 84 bytes of host bookkeeping and no GPU memory, so
a larger pool would be cheap, and it would not help, because the *textures* are what accumulate: a
bigger pool buys a leaking app a later wall, not a working one. 128 is generous for a fixed art set,
and any app that *streams* images (a folder browser, a feed, a map tiler) must free as it goes no
matter what the cap is.
**Where to free: wherever your own code decides a picture is finished with** - leaving a screen,
evicting a cache, swapping art - **including inside the layout callback that just drew it.**
`if (rcClicked("close")) rcUnloadImage(&pic);` is safe and is the natural thing to write, because
layout is where you poll `rcClicked`.

Your handle is cleared the moment you call, so nothing you declare after the unload can reference
it, and the picture stops drawing from that point in the frame. The bytes themselves are released
at the frame boundary, once the frame you were building has been drawn - so an element declared
EARLIER in the same frame keeps its artwork until the frame reaches the screen, as it must. You
never sequence any of this yourself. ex10's IMAGE section drives it live.
**At shutdown, do nothing**: `rcRunApp`'s teardown releases every GPU resource for you, so there is no
cleanup to write and no shutdown hook to want. Calling `rcUnloadImage` after `rcRunApp` returns is a
deliberate, safe no-op (it detects the backend is down, frees the CPU-side wrapper, and cannot
double-free). Freeing *during* the run is about the ceiling, never about exit.

## rcWindowFrameCounts

.declared is the elements your layoutCallback declared this frame, before culling; .drawCommands is how
many of them survived culling and reached the renderer.

**The first number to reach for** when a frame feels expensive, and unlike `rcAppPerfFrame` it needs no
build knob; it is always compiled in. A large ratio is the signal, and the cure is to declare less
(`rcVirtualList`), not to draw faster. Measured: 400 filled 4px boxes stacked in a column at 800x600
read 402 declared -> 151 drawn, so only what fits the viewport survives; the ratio climbs with the
list length while the visible cost stays flat.

**The drawn count depends on your geometry**, so compare it against your own earlier runs rather than
against any number quoted here. .declared is the stable half: it counts what your layoutCallback asked for
and does not move when the window does.

**Culling happens in two places, and the second one surprises people.** The first is per *element*: a box
whose bounding box is entirely outside the viewport emits nothing. The second is *inside* a wrapped-text
element, at *line* granularity: the emission loop stops at the first line whose running y has passed the
viewport bottom, and every later line of that same paragraph is dropped with it.
  ⇒ **One long paragraph can contribute fewer draw commands than it has lines**, and how many depends on
  the window height. It is not clipped-at-draw-time; the commands are never generated.
  **It keys on the viewport, not on the containing box**, so a tall paragraph inside a scroll container
  is unaffected once you scroll it into view; the test is where the line lands *on screen*.
  The line that crosses the edge *is* emitted; the stop applies to the ones after it. Both sites are
  governed by the same culling switch, so both go away together if it is turned off.
  **This is why a drawn count is not a line count.** If you are using .drawCommands to sanity-check that
  a body of text rendered, it will under-report by design, and resizing the window will change it.

**An element with nothing to fill emits no draw command.** The same 400 boxes with no .bg read 402
declared -> 0 drawn. A low drawn count is not evidence of a problem: a layout scaffold of bare
`rcRow`/`rcColumn` is *supposed* to cost nothing.

Both fields are 0 before the first drawn frame, and for a NULL app. What each ratio means, with a
table, is in getting-started ▸ "The first number to look at".

**Read it between frames**: in `RC_AppOptions.frameEndCallback`, or anywhere outside the layout pass.
It is snapshotted where the render walk begins, so a read inside layoutCallback/updateCallback answers for the
*previous* drawn frame. That is deliberate: correct numbers for an earlier frame rather than torn
numbers for this one.

## rcAppDestroy

  **One PRIMARY window per process, and the guard tests the WINDOW.** `rcAppCreate` returns NULL
    whenever a primary window is already open, whether an earlier `rcAppCreate` opened it or a bare
    `rcInitWindow` did. A caller holding no `RC_App` at all is refused just the same. It logs at
    ERROR, and the message names the call you actually want for a second desktop window:
    `rcAppCreate: a primary window is already open. Close its app with rcAppDestroy(app), or use`
    `rcCloseWindow() for rcInitWindow. For additional desktop windows, use rcAppOpenWindow on the`
    `existing app.`
    The refusal runs both ways, in different shapes: `rcInitWindow` while a window is open logs
    `rcInitWindow: a window already exists` at WARNING and returns, and it returns `void`, so that log
    line is the caller's only signal. The open window is untouched either way: a refused
    `rcInitWindow(640, 480, ...)` against a live 320 x 240 window leaves it 320 x 240.
    **Close it with the verb that matches how it was opened.** `rcAppDestroy(app)` for a window
    `rcAppCreate` opened; `rcCloseWindow()` for one `rcInitWindow` opened. They do not substitute for
    each other. `rcAppDestroy(NULL)` returns immediately and tears down nothing, so it leaves a bare
    `rcInitWindow` window open and the next `rcAppCreate` still refused.
    `rcCloseWindow()` on a window an `RC_App` owns does clear the guard and lets the next
    `rcAppCreate` through, at the cost of the first `RC_App`, which becomes unreachable: **the whole
    layout arena leaks, plus the `RC_App` record itself**, against 0 bytes for
    `create -> rcAppDestroy -> create`. The arena dominates, and its size is yours to choose rather
    than a constant to memorise: it is sized from `startLayoutElements`, 2,048 by default, which is
    **1,501,376 bytes** in the build this page was written against - a figure to calibrate against,
    never to hard-code. Derive your own with `Clay_MinMemorySize()` after
    `Clay_SetMaxElementCount(n)` instead of scaling that figure - the arena is 64-byte aligned, so an
    affine estimate under-predicts by up to about 1 kB off a multiple of 64, and the per-element cost
    differs by ABI (the Microsoft C dialect has no narrow enum before C23). It also grows: a frame
    that needs more elements re-sizes the arena up to `maxLayoutElements`, 65,536 by default, so a
    leaked arena is as large as the busiest frame that app ever drew.
    Sequential windows *do* work: create -> `rcAppDestroy` -> create, and
    `rcInitWindow` -> `rcCloseWindow` -> `rcAppCreate`.
    The limit is RayClay's own; the layout engine underneath does support simultaneous contexts.
    Lifting it means moving a hundred-odd pieces of per-window state behind an explicit context, so
    treat one live `RC_App` as a standing constraint rather than a check waiting to be lifted:
    `rcAppCreate` refuses outright rather than half-working.
    **Want a second view today?** Dock it in the same window: `rcBeginSplitPane` (a resizable
    sidebar/detail split) or a non-modal `rcBeginModal` panel (a floating inspector that leaves
    the app live behind it). Both ship, and are demoed in ex10/ex12.
  **`rcRunFrame` does not park while a window can draw**: it polls, draws one frame per eligible
    window and returns, so the on-demand idle does not apply to a hand-rolled
    `while (rcRunFrame(app)) {}`. That spins flat out: measured 115.45 vs 0.20 CPU-s over the same
    idle window: 577x. The park lives in `rcRunApp`'s loop, not in the frame call. The one case it
    does wait is when NO window is eligible to draw (a minimized primary without
    `renderWhileMinimized`): then it parks for up to 250 ms per call. Child windows opened with
    `rcAppOpenWindow` are still admitted through the on-demand scheduler under this loop, so
    `RC_RENDER_ON_DEMAND` applies to them and only the primary is yours to pace. Prefer `rcRunApp`
    unless you must own the loop; if you must, block or pace it
    yourself. **Pacing is yours; the two bounds are not.** `maxFrames`/`RAYCLAY_MAX_FRAMES` and
    `maxSeconds`/`RAYCLAY_MAX_SECONDS` are both honoured here: `rcRunFrame` tests the deadline on
    entry and returns false, the same edge `rcRunApp`'s loop tests it on.

## Several windows, one app

A second window is a second SURFACE of the same app: same arena, same style, same `userData`. Every
property that belongs to a surface is an `rcWindow*` call taking an `RC_Window *`, never a global.

**Ask before you offer the affordance.** `rcChildWindowsSupported()` answers for the platform;
mobile and web return false and `rcAppOpenWindow` there returns `NULL` without attempting anything.
Capability is not availability - a `true` does not promise the next open succeeds.

```c
static void panel_layout(RC_Window *w, void *user) {
    AppState *st = user;                        /* shared, and BORROWED */
    rcColumn(.w = "grow", .h = "grow", .className = "p-4 gap-2") {
        rcTextL("Detached panel", .color = rcGetStyle().text);
    }
}

RC_WindowOptions o = {
    .title = "Inspector", .width = 360, .height = 480,
    .placement   = RC_WINDOW_PLACE_AT,          /* open where the user dropped it */
    .x = dropX, .y = dropY,                     /* signed; a left-hand monitor is negative */
    .topmost     = true,                        /* pinned from the first frame */
    .focusPolicy = RC_WINDOW_FOCUS_NONE,        /* a palette must not steal the caret */
    .taskbar     = RC_WINDOW_TASKBAR_SKIP,      /* a tool panel, not a second app */
    .owner       = rcAppMainWindow(app),        /* a CHILD: stacks and minimises with its parent */
    .updateCallback = panel_update,             /* optional: run before the layout */
    .layoutCallback = panel_layout,             /* REQUIRED on every window */
    .stateCallback  = panel_state,              /* optional: focus, minimise, close */
    .userData       = st,
};
RC_Window *panel = rcAppOpenWindow(app, &o);
```

`layoutCallback` is the only required one. `updateCallback` runs before it each frame,
`stateCallback` reports focus, minimise, maximise and close, and `frameEndCallback` runs after the
draw; all three are optional and all four receive the window and your `userData`.

**A window is BORN floating, pinned, placed and taskbar-skipped - it is not made so afterwards.**
Those four are open-time options. There are setters for some of them, but a window that opens in the
wrong place and corrects itself is a visible jump.

**`.owner` is the whole difference between a panel and a second application.** With an owner the
window stacks above its parent and minimises with it; without one it is an independent top-level
window. It does NOT by itself keep the panel out of the taskbar - that is `.taskbar`, a separate
opt-in. `rcWindowOwnerSupported()` reports it, and on Wayland the answer depends on the host (SDL3
parents a toplevel, GLFW cannot). **On Wayland treat `.owner` as STACKING ONLY**: the protocol
promises stacking and says nothing about minimising, so do not build a workflow on a panel vanishing
and returning with its owner. Destroying a window that owns others leaves them as ordinary
top-level windows.

### A non-NULL return is a RECORD, not a ready window

`rcAppOpenWindow` returns a handle whose `rcWindowState(w)` you must read:

| Called from | State on return |
|---|---|
| inside a callback, a frame or a pump | `RC_WINDOW_PENDING` - the runner creates it once the stack unwinds |
| outside one | creation is attempted first, so `RC_WINDOW_READY` may already be true |

`RC_WINDOW_FAILED` carries a reason in `rcWindowError(w)`: `RC_WINDOW_ERROR_HOST`,
`RC_WINDOW_ERROR_GRAPHICS`, `RC_WINDOW_ERROR_LAYOUT`, `RC_WINDOW_ERROR_OUT_OF_MEMORY`,
`RC_WINDOW_ERROR_INVALID_ARGUMENT`. `RC_WINDOW_CLOSED` means the user closed it. Never draw to a
handle you have not seen reach `RC_WINDOW_READY`.

`rcAppCurrentWindow(app)` names the window whose callback is running, and `NULL` outside one.

### The lifetime rule, which is the one to get right

```c
if (rcWindowRelease(panel))
    panel = NULL;                     /* do this, every time */
```

The runner owns the window; you own the POINTER. `rcWindowRelease` accepts only a TERMINAL record -
one that has reached `RC_WINDOW_CLOSED` or `RC_WINDOW_FAILED` - and returns false for a live,
pending or closing one, so the `if` above is the whole protocol. A handle you keep after a true is a
dangling pointer. `rcWindowRequestClose(w)` asks politely and the window closes next frame.
**Never release `rcAppMainWindow(app)`**: the primary window is borrowed and lives until the app is
destroyed.

### The verbs, and the fact that they are all requests

`rcWindowSetPosition` / `rcWindowSetSize` / `rcWindowSetTitle`, `rcWindowMinimize` /
`rcWindowMaximize` / `rcWindowRestore`, `rcWindowRaise`, `rcWindowSetTopmost`. Every one ASKS the
window system. Read the answer back with `rcWindowPosition`, `rcWindowDimensions`,
`rcWindowIsMinimized`, `rcWindowIsMaximized`, `rcWindowIsFocused`, `rcWindowIsTopmost` - and gate the
control itself on `rcWindowPositionSupported`, `rcWindowTopmostSupported` (or the process-wide
`rcTopmostSupported()`) and `rcWindowOwnerSupported`. There is no taskbar equivalent: ask for
`.taskbar` at open and read the capability table below.

**Those gates answer FALSE until the window reaches `RC_WINDOW_READY`**, because they read the
native handle - so a gate computed once at startup, or from a `PENDING` record, hides your control
forever on a desktop that supports it. Read them from the window's own layout, or ask the main
window. `rcChildWindowsSupported()` is the exception and answers before `rcAppCreate`.

**`rcWindowIsFocused`, `rcWindowIsMinimized` and `rcWindowIsMaximized` answer FALSE on Android, iOS
and the web** - always, because geometry is not the app's to touch there. `false` means no reading
was taken, not that the window is unfocused. `rcWindowDimensions` is unaffected: a canvas has a size.

**`rcWindowRaise` does not promise to un-minimise.** Call `rcWindowRestore(w)` first if the window
may be minimised. `rcWindowSetTitle` sets the NATIVE title; an app drawing its own titlebar paints
its own string and must set both.

**Drive a pin control from the window, never from a `bool` you kept** - a desktop may refuse, and a
remembered flag then lies to the user.

```c
/* Drawn in the FLOATING panel's own layout, so `w` is that window. */
if (rcWindowTopmostSupported(w)) {                 /* should this control exist at all? */
    bool pinned = rcWindowIsTopmost(w);            /* NATIVE REALITY, not your last request */
    if (rcButton("pin", pinned ? "Unpin" : "Pin", RC_BTN_DEFAULT))
        rcWindowSetTopmost(w, !pinned);
} else {
    rcTextL("This desktop cannot pin a window on top.",
            .font = F_SMALL, .color = rcGetStyle().textMuted);
}
```

### What each platform can and cannot do

| | Windows | macOS | Linux X11 | Linux Wayland | Android, iOS, web |
|---|---|---|---|---|---|
| open a child | yes | yes | yes | yes | `NULL`, no attempt |
| stay on top (`rcWindowSetTopmost`) | yes | yes | if the WM supports it; read it back | refused: no always-on-top protocol | inert, reads false |
| raise + focus (`rcWindowRaise`) | yes | yes | yes | asked, and the compositor decides | refused |
| read focus/min/max state back | yes | yes | yes | yes | false always: no reading, not "no" |
| rename (`rcWindowSetTitle`) | yes | yes | yes | yes | refused |
| give it an `.owner` | yes | yes | yes | **host-dependent**: SDL3 yes, GLFW no | false, one surface |
| place at a coordinate | yes | yes | yes | refused: clients cannot position themselves | refused |
| open pinned / at a coordinate | yes | yes | WM-dependent | refused, warned once | not applicable |
| open without taking focus | yes | yes | yes | yes | not applicable |
| keep out of the taskbar | yes | not applicable: the Dock lists the app | yes, if the window manager honours it | refused, warned once | not applicable |
| own titlebar, zoom, clear colour, render mode | yes | yes | yes | yes | not applicable |

Wayland is the one row that depends on which host you built against, and the refusals above are the
protocol's, not RayClay's. Worked call sites: `ex20_system_monitor` and the `notes` bench app.

**One SDL3-only fact that changes what you write** (`-DRC_WINDOW_BACKEND=SDL3`): `rcRunApp` owns the
SDL event queue, and the first SDL event RayClay does not own ends the run - it returns 1 and logs
one `RAYCLAY[ERROR]` naming the split loop. Keep your own SDL work beside RayClay by driving
`rcRunFrame` yourself and servicing your events between frames. If you called `SDL_Init` before
`rcAppCreate`, RayClay's Wayland decoration hint is dormant and it warns once; set the hint yourself
or create the app first.

## rcCloseWindow, and how an app actually quits

**`rcCloseWindow` is the deferral-safe close you can call from inside a callback**, and that is the
role worth knowing: it shuts down fonts, text, clipboard, the icon pool, widgets, intents and input,
tears down the render backend, then destroys the window. It is also `rcInitWindow`'s symmetric
partner for an L1 host. **It is not how an `rcRunApp` application quits itself** - a Quit button
calls `rcAppRequestClose(app)`, which ends the run after the current frame.

It is also what clears the one-window guard for that path. While the window is open `rcAppCreate`
returns NULL, and a second `rcInitWindow` logs `rcInitWindow: a window already exists` and opens
nothing. The condition tests the WINDOW, not whether an `RC_App` exists, so a program that opened
its window with a bare `rcInitWindow` is refused a later `rcAppCreate` until it calls this. See
**rcAppDestroy** above for the full pairing and for why `rcAppDestroy(NULL)` will not do instead.

**It is not how an `rcRunApp` application quits itself.** A Quit button, a menu item, a close
chip: all of them call `rcAppRequestClose(app)`, which sets a flag the runner reads at the top of
its next iteration, so the current frame finishes drawing and the loop then exits normally.

```c
if (rcButton("quit", "Quit", RC_BTN_GHOST))
    rcAppRequestClose(app);          /* the quit verb */
```

**Calling `rcCloseWindow` from a callback is safe, and it always defers.** While a callback, a frame
or a pump is on the stack, native teardown waits until everything unwinds: the frame you are in
completes, your layout callback still runs for the rest of it, and the window is destroyed after.
Called outside any frame it is synchronous. There is no frame kind that tears down mid-frame.

The deferral is uniform, so the only thing to design for is that the rest of the current frame
still runs: do not free the state your layout callback reads in the same breath as the close.
⇒ **Reach for `rcAppRequestClose` and this whole question disappears.**

`rcCloseWindow` is idempotent, so a split-loop host that calls it explicitly *and* lets the runner
reach its own teardown is doing the ordinary thing, not double-freeing.

## Colour builders are compound literals

`rcRgb` / `rcRgba` and the built-in RC_* palette expand to a compound literal, which is not a constant
expression. `rcHex` and `rcColor` are *functions*, also not constant expressions, so the same rule
below applies to all four spellings, for a different reason.

**Name a file-scope token with #define, not `static const`.** A file-scope
`static const RC_Color X = rcRgb(...)` (or `= RC_SLATE_800`) is rejected under
-std=c99/c11 -pedantic-errors. It compiles as a GNU/clang extension; do not rely on it.

```c
#define BRAND_500  rcRgb(99, 102, 241)
```

## rcWindowSchedStats: reading the idle scheduler while it runs

On-demand rendering is RayClay's headline claim, and a claim needs an instrument. `rcWindowSchedStats`
returns a snapshot of the runner's frame-admission counters, so an app can plot what that claim is
about: how often it woke, and how often that wake did nothing.

```c
RC_SchedStats s = rcWindowSchedStats(rcAppMainWindow(app));
float wasted = s.waits ? 100.0f * (float)s.spurious / (float)s.waits : 0.0f;
```

**The counters are cumulative.** Chart the *delta* between samples, not the value: the raw figure only
  ramps, and a wake loop is a *rate* that spikes, not a total that grows.

`deadlineWakes` counts the admissions whose reason was a due deadline (an `rcWindowRequestFrameAfter` or a
transition coming due), so `admitted - deadlineWakes` is the share of drawn frames that input or a
request caused.

`requests` counts the requests that actually **added** a reason - one that was not already set. Asking
for a frame twice before the next pump books one, not two, because the second finds the reason
standing. So `requests` is the count of distinct things that made this frame necessary, never the
number of times you called: a widget that requests on every mouse move cannot inflate it.

**The denominator is `waits`, never `admitted`.** spurious/admitted compares two different
  populations and reads alarmingly high on a perfectly healthy idle app: one that parks a great
  deal and draws rarely is doing exactly what it is supposed to.

**A per-second rate needs a clock, and the clock is `rcAppTime`**, *not* an accumulation of
  `rcWindowFrameTime`, which is exponentially smoothed and discards any interval of 1 s or more, so on
  demand it does not lose precision, it stops tracking. Measured over 6 s: summing it while woken
  about once a second gives 0.610 s against 6.006 s of wall clock; the same measurement under
  continuous rendering reads 1.000. → **rcAppTime** below. Or prefer the *ratio* above, which is
  dimensionless and needs no clock at all. `examples/ex12_rayclay_inspector` labels its two series
  `spur/frame` and `adm/frame`, so the unit is on screen and not merely in its source.

### The three zeros that lie

**`RC_FRAME_EXPOSE` usually reads 0, and a non-zero value is real rather than impossible.** The
  normal refresh path does not raise it: during an OS modal resize the window system's damage
  callback repaints SYNCHRONOUSLY, drawing the frame itself because the main loop is not running to
  admit one, so nothing is attributed to EXPOSE. Two FALLBACK paths do raise it - a draw attempted
  re-entrantly while another frame owns the loop, and a refresh callback that arrives when the
  window cannot draw - and both are rare enough that most runs never see either. So read a 0 as
  "neither fallback fired", and read a non-zero as a genuine count of times one did, not as a
  reporting artefact. That distinction matters because "nothing can report this" and "this never
  happened" print the same digit.

**Every ADMISSION counter is 0 under a manual `rcRunFrame` loop, and `requests` is not.** The
  scheduler belongs to `rcRunApp`; a host driving its own loop has nothing to admit and nothing to
  park, so `admitted`, `waits` and `spurious` stay at zero. That zero is *not* "my app never sleeps";
  it is "this app has no scheduler". `requests` still counts the first raise of each reason, because
  raising a reason is something your code does and does not need a scheduler to happen - so a
  non-zero `requests` beside a zero `admitted` is the normal reading here, not a contradiction.

**`waits` is 0 under `RC_RENDER_CONTINUOUS`**, because the loop never parks. So a scheduler panel in a
  continuous app is a structural "not applicable", not a clean bill of health. Measured on ex23 idle
  for 3 s, Linux/Xvfb/llvmpipe: continuous 0 waits and 0 spurious, on demand 1 wait.
  Frame counts from that run are a property of one GPU and one 3-second window and would not
  reproduce elsewhere; the zeros are the finding.

### Reading `spurious` and `refreshRepaints`

Both are `RC_SchedStats` counters, read through `rcWindowSchedStats`.
`spurious` counts parks that ended with **no reason pending and no deadline due**, so it reads
directly as "parks that woke for nothing", which is the presentation-echo signature you are looking
for. A park armed with a timeout that then fires *on* that timeout is real work you asked for, and it
is **not** charged here: a deadline is a *time* rather than a reason bit, and the scheduler asks
whether one is due before booking the wake as waste.

`refreshRepaints` counts **window-refresh callback ENTRIES**: how often the OS *asked* for a
synchronous repaint outside the admit path, not frames drawn. The counter is incremented before the
frame is attempted, and a callback that arrives while a frame is already in flight is dropped by the
re-entrancy guard without painting; nothing decrements. Measured on macOS: **+40 across a single
window zoom, against about two frames actually completed**. Never read it as a frame count.

Those entries are a different population from the parks the other fields describe, so treat it as
context beside `spurious` rather than something to subtract from it.

**`RC_FRAME_REASON_COUNT` is an array length, not a reason.** Bound `byReason[]` with `<`, never
  `<=`, and read the length from the enum rather than writing the number down; that is the whole
  point of it having a name. `rcFrameReasonName` turns an index into a label, so a loop over the
  array needs no table of your own and picks up any reason added later.

## Leaving a container body early

**Leaving a container body early.** The brace body is a macro-generated loop, not a plain block, so
   C's jump keywords do not mean what they look like. Measured on gcc and clang, 0 warnings, on
   both public runners (`rcRunApp` and the `rcRunFrame` split loop, which share one frame path):
     continue   safe      ends the element body cleanly. No diagnostic.
     break      safe, but not what you mean: it ends the *element* body, not your enclosing loop,
                and the loop keeps going. Measured: `break` at i==2 of 5 still ran i=3 and i=4.
                The element is closed for you, and RayClay logs one warning that names the fix.
     goto out   unsupported. Do not write them. Both leave the whole `for` without running its
     return out increment, so the close is skipped and the element stack is left unbalanced.
                The cost, measured: the layout engine drains what you left open, so you get a completed frame
                that is the *wrong* frame: every element after the jump is missing, every frame (a
                5-iteration loop declared 3), and a sibling declared after the escape point can
                vanish entirely. "Unsupported" is the contract, not a promise about what happens
                next: do not rely on whatever you happen to observe.
   **TWO DIAGNOSTICS SIT HERE AND THEY LATCH DIFFERENTLY, which matters the moment you try to
     reproduce one.** Neither floods your log. The *escape* warning - the one this section is about,
     fired when a body is left early - latches **once per PROCESS**, so a second window and a second
     `rcAppCreate` both stay **silent**. The layout-engine errors below it (`Innermost element left
     open` and its family) latch **per WINDOW**, so a second window really does report the same
     fault again. If you opened a second window to make the escape warning reappear and saw nothing,
     that is this, not a fixed bug.
     The warning scrolls away while the broken UI stays, so never go looking for a repeating line
     to confirm this; there isn't one. It usually names the culprit: "Innermost element left open:
     <your-element-id>", but only when that element
     has a string .id and the message fits the frame's diagnostic buffer; otherwise you get the bare
     sentence by design, so absence is not a second bug. Give looped containers real string ids.
     Fail CI on any RAYCLAY[WARNING] across the whole run, never on a tail.
   **The fix** for all three: put the loop outside the element, or set a flag inside the body and
      test it after. Never jump out of the braces.
   On the web the warning *does* reach you: RayClay logs to stderr and the bundled shell routes it to
     console.error, so it shows up in devtools. You just have to have devtools open. See web-build.md.

## .align: the "<Y><X>" code

.align: "<Y><X>", e.g. "tc" = top + centre.  Grammar: Y is one of t|c|b, X is one of l|c|r.
  **A half-wrong code half-works, loudly:** "lc" puts 'l' in the Y slot, so the parser warns
    (`RAYCLAY[WARNING]: RayClay DSL: unparsable align token "lc"; unrecognised bytes keep the
    default, recognised ones still apply`) and **then lays out anyway** using the valid half.

**That rule is not special to `.align`: six fields take a short code and all six behave this way.**
  `.align` and `rcBeginTable`'s column align are three bytes; `.wrap`, `.textAlign`, `.scroll` and
  `RC_Gradient.dir` are two. In every one of them **resolution is per byte**: a byte the grammar does
  not know falls back to that slot's default, and every byte it does know still applies. So
  `.align = "Qc"` still centres horizontally and only the vertical half falls back. **A trailing byte
  outside the grammar warns without changing the result**: `.wrap = "no"` warns and still wraps as
  `"n"`. Five of the six print one line, and it states the rule rather than the symptom:

      RAYCLAY[WARNING]: RayClay DSL: unparsable <kind> token "<token>"; unrecognised bytes keep the default, recognised ones still apply

  where `<kind>` is `text wrap`, `text align`, `align`, `scroll` or `gradient direction`. The table's
  column align prints its own sentence naming the grammar it wanted, and says the same thing about
  the axes: `unrecognised bytes keep their axis default (top / left)`. **The axes do not fall
  together**, so a table column spelled `"crX"` is still centred and right-aligned.
  **One thing stops you seeing it.** The line is emitted **once per distinct (field, token) pair for
  the life of the process**, so a typo you repeat is reported once rather than every frame. And the
  compiler will not cover for you: `.wrap = "no"` fits a `char[2]` exactly and legally, with no
  terminator (C99 6.7.8p14), and the diagnostic for that spelling lives in `-Wextra`, not `-Wall`.

**The trap: some of these string fields are fixed arrays, so they take a literal and NOT a variable.**
  The two shapes look identical at the call site and behave differently. `char[N]` fields are
  initialised from a string *literal*; a `const char *` field is assigned a *pointer*, so only the
  second accepts a runtime value:

      .overflow = "hidden"                   /* ok: initialising a char[8] from a literal   */
      .overflow = cover ? "hidden" : "";     /* ERROR: initialization of 'char' from 'char *' */
      .w        = wide  ? "grow"   : "fit";  /* ok: .w is a const char *, so a value is fine  */

  The fixed-array fields are `.align`, `.scroll`, `.overflow` and `.borderRadius` on an element,
  `.width` on a border, `.dir` on a gradient, `.wrap` and `.textAlign` on text, and a table
  column's own `.align`. Everything else string-shaped
  (`.w`, `.h`, `.className`, `.id`) is a pointer and takes any expression. To choose one of these at
  runtime, branch on the whole call rather than on the designator, or pick a value that is correct in
  both cases: `.overflow = "hidden"` costs nothing on content that already fits.

**The trap: which letter is justify-content depends on the container's direction.**
  These letters are not bound to fixed CSS properties. justify-content is always the *main* axis, and
  the main axis is X in a row but Y in a column, so the two letters *swap roles* between them:
                     justify-content (distributes the children as a group)   align-items (each child alone)
    `rcRow`                    X, the 2nd letter                                  Y, the 1st letter
    `rcColumn` / `rcBox`         Y, the 1st letter                                  X, the 2nd letter
       (`rcBox` is top-to-bottom by default, so it behaves as a column here)
  Measured, 1280-wide container holding two 100px-wide children:
    `rcRow`    .align="tc"  -> first child x = 540 = (1280-200)/2   the pair is centred as a group
    `rcColumn` .align="tc"  -> first child x = 590 = (1280-100)/2   each child is centred on its own
  It reads correctly for "cc" whichever way you assume, which is exactly why this bites late: a
    developer writing .align="tc" on an `rcColumn` expecting a spread row gets individually-centred
    children and no warning. If you want a group distributed along a row, you want `rcRow`.

  The TYPED `.align` is start | centre | end on the main axis. The CLASS surface carries the
    distributions: `justify-between`, `justify-around` and `justify-evenly` all resolve through
    `.className` (driven against the engine, not read off a table). With the typed field alone the
    substitute is an `rcSeparator` between the items, which grows to fill.

## Sizing strings (.w / .h)

.w / .h is the CSS-like sizing string, the primary inline-CSS form used throughout examples/:
  ""/NULL/"fit"/"auto" => FIT (shrink to content)   "grow" => GROW (fill the parent)
  "100" or "100px" => fixed px   "50%" => % of parent   "50vw" / "50vh" => % of the viewport w/h
  "8rem" => fixed px, 8 x the root font size (16 by default; `rcSetRootFontSize` moves it). `rem` is
  a LENGTH, so it resolves down the same path as `px` and is counter-scaled inside `rcUnzoomed()`
  the same way.
  **Units are case-sensitive** here while CSS is not: "50vw" parses, "50VW" is rejected (and warns).
  Decimals and "1e2" are fine; a hex / negative / over-100% / unknown-unit token warns once and falls
  back to FIT, and a childless FIT box is 0-wide, so a bad unit makes the element vanish (watch the
  log). "%" is parent-relative; a "%" inside a FIT parent has no definite basis and likewise collapses
  to 0. Its exact per-axis basis vs CSS (the main axis reserves sibling gaps, the cross axis does not,
  and a "fit" parent collapses the child) is measured out in for-web-developers.md ▸ "% is parent-relative".

## rcTextC

  e.g.  `rcTextL("Title", .font = rcFont("Roboto", RC_WEIGHT_BOLD, 22), .color = s.text);`
  Lifetime: `rcText`/`rcTextC` do **not** copy; the layout engine keeps your pointer until the frame is drawn, so a loop-scoped
  stack buffer is a use-after-scope; use `rcFormat` / a longer-lived buffer (`rcTextL` literals are static).
  Text covers ASCII (32–126) + Latin-1 (160–255) by default, an *engine* codepoint window, not the loaded font's
  own coverage: a codepoint outside it (smart quotes, €, CJK) renders '?' even from a custom font that contains
  it; control/DEL chars render nothing.
  **That substitution reports itself, ONCE PER DISTINCT CODEPOINT.** Each one past the cap emits an
  `RC_LOG_WARNING` naming that codepoint and the live cap, so `?` on screen has something to search for -
  and fixing the em dash then still seeing `?` gives you a *second* line naming the arrow, rather than
  silence you would reasonably read as a failed fix. **The reporting is bounded, not the drawing:**
  after 8 distinct codepoints one further line says so and the rest are substituted silently, so a
  document pasted from the web cannot turn this into a log flood.
  **What it still cannot do, and it is the one open ask here:** an app cannot query at run time whether
  the string it just drew was substituted. The report is for the console, not for the program.
  The window is a build-time default, not a hard limit: `-DRC_FONT_LAST_CODEPOINT=N` widens it, and the
  three conditions that must travel with it (bundled font has no glyphs past Latin-1 · cost is linear in the
  *cap* not in use · atlas area is a separate ceiling) are spelled out at `rcRegisterFont`. Not a CJK switch.

## rcScrollBy: the sign convention

POSITIVE-DOWN and POSITIVE-RIGHT, like the DOM's element.scrollBy, and like `RC_ScrollInfo`'s
.offsetY/.offsetX readback, so the two *compose*: `rcScrollBy`(id, 0, 160) raises .offsetY by 160.
Clamped at both ends.
**Both wheel readers are exceptions, and in different ways.** They report the raw wheel in notches
rather than pixels, so scale them. `rcScrollDeltaY` is positive-UP, the opposite of `rcScrollBy`'s
positive-DOWN, so negate it. **`rcScrollDeltaX`'s sign is UNSPECIFIED**: it is not normalised, and
no direction has been measured across platforms, so none is promised. The magnitude is trustworthy
and the direction is not. If your app scrolls horizontally, test the sign on each target you ship to
and apply your own factor; do not calibrate it on one machine and assume it holds on the next. Do
not assume either reader shares `rcScrollBy`'s signs.

## Element ids are 32-bit hashes

Ids are 32-bit hashes of the *string*, so a formatted-index id can collide once ~15k of them coexist
in one frame, and the threshold depends on the *prefix*: "item%d" collides at 20,000 while "Row %d" is
clean through 65,536 (measured 2026-08-08).

Virtualizing is what keeps this from mattering in practice: only the visible window is declared, so
a 1,000,000-row list has ~20 live ids and the *data* index is safe at any size. And it is not silent
if you do hit it: the warning names the id.

## rcScrollbar

**Call it inside your layout callback**: a requirement, not a style note. The bar is a floating
  element declared where you call it, so the layout must still be open
**Layering is automatic**: the bar sits just above the content it scrolls and *below* a modal scrim,
  so an open dialog covers and dims it along with the rest of the background. Declare it in the
  *same scope* as its container: called inside a modal/popup scope it lifts above *that* panel instead
**The bar is a floating element at `zIndex` 1.** A floating element of your own left at the default
  `zIndex` 0 (a floating action button over a list, say) is painted *under* the thumb wherever the
  two overlap, whatever the declaration order; give yours `.zIndex = 2` or more
Drive the scroll *before* you declare the bar: `rcScrollbar` samples the offset as it declares
  itself, so an `rcScrollToBottom` placed after it leaves the thumb a frame stale
The thumb is placed from the container's *previous* frame, so it lags one frame on a content-size
  change (the ordinary immediate-mode trade). Auto-hides while the content fits
**CALL IT UNCONDITIONALLY - THE LIBRARY ALREADY KNOWS ABOUT FINGERS.** Where `rcPointerIsCoarse()`
  is true, `rcScrollbar` draws **no track at all**: a thinner translucent thumb appears while the
  container's offset is actually moving and fades out about half a second after it stops, which is
  exactly what a browser on a phone does. On those devices the thumb is a **readout until you hold
  it**: press and hold it for half a second and it widens and takes the drag, so a long list can be
  driven by the bar instead of swiped. A tap or a pan across it still passes straight through to the
  content, because nothing is reserved until the hold completes - so the bar can never swallow a tap
  meant for what is behind it.
  **Do not write `if (!rcPointerIsCoarse()) rcScrollbar("list");`** - a guard in your code freezes one
  answer into your app, and the answer belongs to the library. Asking on the POINTER rather than on the
  OS is still right, and a native mobile build is coarse from the FIRST frame rather than correcting
  itself on the second - the library asks the same question you would have. The affordance argument
  is real and it is a DESKTOP argument: there, a visible bar still beats an invisible one, and that
  is the arm you still get.
  **On the WEB, design the layout so the scrollbar thumb still works.** That is correct whether or
  not a given browser delivers a finger pan, where assuming the pan is not. The pan owns the **deepest eligible
  container under the contact, per axis**, after an 8 px threshold, and *eligible* means you
  authored that axis scrollable **and** the content exceeds the view: `overflow: hidden` or
  `overflow: clip` without `.scroll` never pans, whatever its content size
**A pan does not fire a click**, so a list of buttons stays draggable. An existing slider, splitter,
  scrollbar or editor grab keeps the contact instead, as does any widget target hit-tested on the
  previous frame; and where two DIFFERENT nested containers own the two axes, the gesture locks to
  one axis, chosen by the larger delta. One container that scrolls both pans diagonally
**A FINGER FLINGS, AND THERE IS NO SCROLL CHAINING**: a release throws the container and damps to
  a stop, with the velocity smoothed over the last moment so a slow drag ending in a flick throws at
  the flick's speed, and a parked finger throws nothing. The decay is folded analytically, so the
  same flick carries the same distance at 60 Hz and at 120 Hz. A pan that reaches the end of
  its container stops there rather than moving the parent. **The mouse wheel does chain:** a notch
  the container under the pointer cannot take - a horizontal scroller under a vertical notch, or a
  nested list already at its end - goes to the nearest ancestor that can, so a code block that
  scrolls sideways does not hold the page still under a vertical wheel. Measured on ex22: ten
  notches over a horizontal code block move the page exactly as ten over plain prose do
**The bar itself is vertical only**: it sits at the container's right edge and drives the y scroll,
  and horizontal overflow gets no bar. A horizontal wheel, a trackpad gesture and a finger pan all
  move horizontal content, so what is missing is the visible affordance, which you draw yourself.
  But do not answer a too-wide element with `.scroll = "h"` on the strength of the pan: a `.scroll`
  container clips on **both** axes regardless of which one scrolls, so it also cuts a drop shadow
  off the top and bottom of its own row, everywhere

## A scroll container that goes away comes back at the top: `.scrollOffset`

A container's scroll position lives only while the container is declared: the registry retires an
entry not declared for two frames, so a tab shell that drops a page and brings it back lands the
reader at the top. `RC_ComponentOptions.scrollOffset` (an `RC_Vec2`, layout units) is the restore:
read `rcGetScrollInfo(id).offsetY` while the page is live, keep it in your state, and hand it back as
`.scrollOffset = { 0, saved }` when the page returns. It is applied once, on the frame the container
is declared with no live record (its first declaration, or a return from absence), one frame later
than that declaration because the range does not exist until the layout has run, and clamped to the
range; it is never applied while the container is live, so it cannot fight the user's own scrolling.
Zero is the origin, which is also "unset".

**Two consequences that the "one frame later" above causes and that cost you a bug each if you
work them out afterwards.** Both are about the single frame the container comes back on:

```c
/* Called every frame, for whichever list is live. */
static void keep_list_scroll(RC_App *app, MyTab *tab, const char *id)
{
    if (tab->justReturned) {
        /* 1. DO NOT READ BACK ON THE RETURN FRAME. The saved offset is applied at
              the FOLLOWING frame's open, so this frame still reads 0 - and storing
              that 0 would overwrite the very value being restored. */
        /* 2. NOTHING ASKS FOR THAT FOLLOWING FRAME. Under the default on-demand
              runner the app can park before it ever happens, leaving the list at
              the top with the correct offset saved and never applied. */
        rcWindowRequestFrame(rcAppMainWindow(app));
        tab->justReturned = false;
        return;
    }
    RC_ScrollInfo si = rcGetScrollInfo(id);
    if (si.found)
        tab->savedY = si.offsetY;              /* keep it while the list is live */
}

/* ... and on the declaration that brings the list home: */
rcColumn(.id = "list", .scroll = "v", .w = "grow", .h = "grow",
         .scrollOffset = { 0, tab->savedY }) { /* rows */ }
```

The second one is the quieter of the two, because an app that renders continuously will never see
it: the extra frame arrives anyway. Park the runner and the same code restores nothing.

`examples/ex32_tab_navigator` is the worked case if you have that checkout.

## Long lists: declaring is what costs, not showing

Long lists: declare only what's visible
  **Why:** layout charges per *declared* element, not per visible one (**about 1,300 Ir each, every
  frame** - see the provenance below), and culling cannot help, because an element must be sized and
  positioned before anyone knows it is offscreen. So one row costs (elements in it) x that, and a
  1,000-row list pays it a thousand times to show fifteen rows.
  **Measured across five of the bench apps:** halving the viewport drops render commands 35-58% and
  moves declared elements by exactly zero in all five of them. Culling is working perfectly and it is not
  the cost. Showing less of a list never makes a list cheaper; declaring less of it does. A scroll
  container is the shape where this hides: it looks bounded on screen and is unbounded in the data
  behind it, so the cost is linear in *your* dataset with no visible symptom until it is large.
  `rcVirtualList` declares only the visible window plus overscan, padded by two spacers, so cost goes
  *flat* in row count. On a 3-element row: 100 rows **397,835 -> 78,637 Ir/frame (5.1x)**, 1,000 rows
  **3,895,411 -> 78,603 (49.6x)**. Scale that by *your* elements-per-row rather than quoting it.
  It is also a RAM lever: the element arena never shrinks, so 5,000 declared rows pin it at
  10.97 MiB where the virtualized list stays on the 1.43 MiB floor.

  **Where those numbers come from, because this page's own rule says a magnitude must say.**
  **Ir** is *instructions read*, callgrind's instruction count - a COUNT from a simulator, not a
  time, which is why it is the one kind of magnitude that carries between machines of the same
  instruction set. Re-measured **2026-09-17** against the current vendored Clay fork with a
  standalone probe built `gcc -std=c11 -O2`, scrolled to the MIDDLE of the list so neither spacer is
  degenerate (starting at the top would flatter virtualization). The equivalence oracle is that both
  arms emit **exactly 49 render commands at every row count**, so the difference is declaration and
  layout work and nothing else; Ir/frame is a slope over 50 and 300 frames, which cancels start-up
  and warm-up. Per declared element it is 1,321.71 Ir at 100 rows and 1,298.04 at 1,000.

  **The figures moved about 11% CHEAPER since the first run, and the re-measurement carried the
  control that makes that attributable.** The 2026-07-24 numbers were 446,057 / 4,375,713 full and
  1,482 / 1,458 per element. Rebuilding the FROZEN July engine with today's compiler reproduces the
  published figure **to the digit**, so the toolchain contributes zero and the whole difference is
  the engine. The saving is **-160 Ir per declared element** and it is the same number at 301 and at
  3,001 declared elements, a tenfold scale change - which is the check that says the shape is
  understood rather than merely observed. **The box is still not recorded, and that control is why
  it does not sink the figure**: it reproduced an eight-week-old count exactly while the machine was
  otherwise loaded. A time would have moved; a count did not. What must be recorded is the toolchain.

## rcVirtualList: the three rules

**Three rules:**
  1. The id must name the *enclosing scroll container*.
     **And give that container a height if it is itself inside another scroll container.** A scrolling
       parent is unbounded along its scroll axis (that is what scrolling means), so a "grow" child of
       one resolves against its own content instead, and the spacers *are* that content: the viewport
       this helper samples becomes the window it declared last frame, and the two feed each other.
       The sampled viewport is clamped to the layout and one line naming the list is
       logged, so the symptom is a stuck screenful of rows, not a hang. "grow" is otherwise the
       normal, correct idiom, including at the root, exactly as above. Measured at 1,000,000 rows:
       grow-at-root, grow-in-fit-parent, grow-in-fixed-parent and fixed px all declare a bounded
       window every frame; scroll-inside-scroll is the *one* shape that runs away.
  2. Every row must really be rowHeight tall: the spacers are computed from it. Uniform rows only
     ; a wrong pitch skews the scrollbar.
  3. Key each row by its *data* index, never by its position in the window. Keying by position
     rebuilds the hashmap working set on every scroll step: it gives back most of the win and makes
     hover/click jump between rows.
  Never `break` out of the *loop* (see above); `continue` is fine.
  The 32-bit id hash can collide once enough sequential ids coexist, and how many depends on the
  *prefix* ("item%d" collides at 20,000, "Row %d" and "e%d" stay clean through 65,536; see "Element
  ids are 32-bit hashes" above). A virtualised list declares only ~20 ids at a time, which is why it
  is the only shape that works at that scale at all.

## Three properties need an `.id`: .shadow, .gradient and .tooltip

**`.shadow`, `.gradient` and `.tooltip` are keyed by the element's id string, so an element that
declares one without an `.id` loses that property entirely.** The element still lays out and draws;
it is simply flat, or has no tooltip. Nothing else about the frame changes, which is what makes it
easy to stare past. A gradient written as a class string (`"bg-linear-to-r from-cyan-500
to-blue-500"`) materialises into the same `.gradient` and inherits the requirement, and the
warning names the typed field - `RC: .gradient requires an .id on the element; gradient ignored` -
even when your source never spelled `.gradient`. A shadow, ring or outline written as a class
string (`"shadow-lg"`, `"ring-2"`, `"outline"`) materialises into the same `.shadow` the same way:
it needs the `.id`, and it needs a visible fill to anchor to. A class `bg-*` counts: the anchor
test reads the folded declaration, so `.shadow` beside `.className = "bg-slate-800"` draws. Both
warnings name `.shadow`.

```c
rcColumn(.bg = RC_INDIGO_500, .shadow = { rcAlpha(RC_BLACK, 40), 0, 2, 6, 0 }) { }  /* no shadow */
rcColumn(.id = "appbar", .bg = RC_INDIGO_500,
         .shadow = { rcAlpha(RC_BLACK, 40), 0, 2, 6, 0 }) { }                       /* shadow */
```

**You are told, and the message names the property:**

```
RC: .shadow requires an .id on the element; shadow ignored
RC: .tooltip requires an .id on the element; tooltip ignored ("Save the file")
```

**A tooltip is identified by its own text; a shadow and a gradient cannot be.** `.tooltip` carries a
string, so it is deduplicated on that text and you get **one line per distinct offender** - which is
the whole point, because "some of my tooltips do not appear" is otherwise an afternoon of bisecting.
`.shadow` and `.gradient` have no text of their own, so there is nothing to tell two offenders
apart: they are deduplicated on the property name and you get **one line per property per run**.
Either way the count is bounded, never silenced.

**The fix is to name the element**, and the id only has to be unique among the ids alive in that
frame - `rcFormat(mem, "card_%d", i).chars` is the usual shape for a list. This is separate from the
cap below: a named element can still lose its gradient by being the 65th one in a frame.

## RC_CHART_MAX_SERIES

`-DRC_CHART_MAX_SERIES=N`, default 16, legal range **1..16**.

**It is a public, readable constant**: it lives in `rayclay.h`, so you can design against it
rather than only set it. That is what a public cap is for:

```c
RC_Series series[RC_CHART_MAX_SERIES];      /* size an array against it */
#if RC_CHART_MAX_SERIES < 4
#  error "this dashboard needs four series"  /* or refuse at compile time */
#endif
```

**Define it for every translation unit.** This is the `[both]` class: your code and the library
each read it, and setting it on one side only gives you an array sized to one cap while the library
clamps at another: **a silently truncated plot, not a build error.** Put it in your build's compile
definitions.

**Both ends of the range are refused, and the floor is the one that matters.** Above 16 the
auto-colour palette wraps and two series get the same rgba, which no reader can attribute. At 0 the
payload's series array becomes zero-length while the library still writes a whole series through
`&series[0]`, and gcc and clang accept that as an extension and diagnose **nothing**
(measured: zero warnings at `-Wall -Wextra -Warray-bounds=2`). The `#error` is what makes the illegal
value impossible rather than merely discouraged.

Cost is `.bss` only and linear: **1,488 B per series**, so 16 costs 22,320 B more than 1
(15 x 1,488). Re-derive it rather than trust the figure: compile the chart module at two values of
`RC_CHART_MAX_SERIES` and take the `size -A` `.bss` delta - at 8 and 16 it reads 44,960 and
56,864, and 11,904 / 8 is the per-series number. Tune it for *readability*, not size: past ~12
categorical hues nobody can tell two lines apart, and a caller with more than that should be
setting `.color` explicitly.
`-DRC_NO_UI_HELPERS` removes this knob along with `rcChart` itself: it configures the chart API and
has no meaning without it.

## Chart hover: .tooltip, .hoverGuide, .hoverMarkers

.tooltip = `RC_CHART_TOOLTIP_NEAREST` opts a chart into a hover readout: the x value plus *every*
series' value at the hovered datum. Zero-init is NONE, so existing charts are unchanged. Prefer it
over hand-rolling: the chart owns its plot transform.

**On a multi-series chart, setting .tooltip alone is rarely what you want.** The panel lists the
numbers but cannot say which series each belongs to. The three-field recipe is what you actually
want, and .tooltip deliberately does not switch the others on for you:

```c
.tooltip = RC_CHART_TOOLTIP_NEAREST, .hoverGuide = true, .hoverMarkers = true
```

ex10's chart panel ships exactly that, with a checkbox for each boolean so you can turn them back
off and see what each was contributing; the comparison is the point.

**Which datum is hovered is decided by mark geometry**, not by an option:

```
LINE / AREA    a curve is continuous, so every x has a reading => the datum nearest the
               pointer's x. UNCHANGED.
BAR / SCATTER  a discrete mark has a real rect or disc and there is nothing to read between
               marks => THE POINTER MUST BE OVER THE MARK. No mark under the pointer means NO
               readout.
```

A chart *mixing* the two stays continuous: the curve is readable at any x, even where no bar is.
Marks get a few px of grace at *both* vertical ends, so a near-zero bar (drawn 1px tall) and a
*negative* bar stay reachable.
A bar chart does *not* fire a readout anywhere inside the plot: not above a short bar, not in
the gap between two. So you do not need a hit test of your own around it.
This is not configurable: a readout for a datum the user did not point at would be a bug rather
than a style choice.

.tooltipPlace decides *where* that readout sits, a *different* field from .tooltip (the trigger):

```
RC_TOOLTIP_PLACE_CURSOR (0, default)  follows the pointer, flipping leftward past the plot's
                                      horizontal middle; the panel is then clamped into the
                                      VISIBLE VIEW, not the plot (neither mode confines it to the plot)
RC_TOOLTIP_PLACE_CORNER               parks it in the TOP corner opposite the pointer's half:
                                      stays clear of the data under the pointer, the better
                                      default for dense plots
RC_TOOLTIP_PLACE_FIXED                pins it at .tooltipAnchor (any of the nine RC_Anchor
                                      points; 0 => TOP_LEFT)
```

.tooltipOffset is the gap from the anchor, {0} => 12x12, a *distance*, not a direction: the sign is
applied for you, away from whichever edge the panel is flipping off, so one value reads the same in
all four quadrants. **The fallback is PER COMPONENT, not all-or-nothing**: `{24, 0}` means 24 x 12,
because a zero component asks for that axis's default gap rather than a flush edge. Negative and
non-finite components take the default the same way. Use a small non-zero value such as `1` to tuck
a panel tight against an edge.

.hoverGuide / .hoverMarkers (bool) draw the hover *in the plot* instead of only in the
floating panel. **Both** are *independent* of .tooltip: a guide with .tooltip = NONE is a legitimate
choice (crosshair, no panel).

```
.hoverGuide    a vertical rule at the hovered datum's x, drawn UNDER the series so it aids
               reading rather than obscuring it. Vertical ONLY: with a second y axis a
               horizontal rule would have to pick one and be wrong for the other.
.hoverMarkers  one dot per LINE/AREA/SCATTER series at the hovered x, in THAT SERIES' OWN
               COLOUR, with a halo so it stays visible where a same-colour line runs through
               it. BARS ARE SKIPPED: a hovered bar already shows which datum it is.
```

This is what makes a *multi-series* readout legible: the panel lists the numbers but cannot say
which series each belongs to; a colour-matched dot can.

**Cost:** with the pointer parked off the plot the frame is byte-identical whether both are off or
both are on, so an unhovered chart pays nothing for either.
While hovering, the guide is +1 line and the markers +2 circles per series (the dot and its halo).

## The bundled titlebar (Flat Slab)

Under nativeFrame the runner draws the *bundled* bar for you (title + min/max/close); set
`RC_AppOptions.titlebar.custom` to own the band instead.

A Flat Slab is a square-cornered, full-band-height rectangle carrying your glyph, invisible at rest
and filling with its accent on hover: close reaches a near-solid red, minimize and maximize a faint
wash. No radius, no border, no glow, no press bounce.
**Desktop-only**: on web and mobile these draw nothing and the ids are inert.

**Fixed chrome:** content zoom never resizes or moves the bundled band. It stays pinned to the
window's top edge at a constant on-screen size while the content magnifies or reflows beneath,
exactly like a browser's own titlebar. The band is sized by the *visible window*, never by your
content, so the controls stay on-screen at any zoom and any window size.
Opt the band into zooming with `RC_TitlebarOptions.zoomWithContent`.
**That is the bundled band only.** A `titlebar.custom` band is ordinary content you drew, so it zooms
unless you say otherwise; wrap it in `rcUnzoomed()`, below. The two knobs are inverses: the bundled
band is out by default and `zoomWithContent` opts it *in*; a custom band is in by default and
`rcUnzoomed()` opts it *out*.

### nativeFrame on Wayland: one decoration policy per process

On Linux under Wayland, `nativeFrame` decides more than one window's border. The window host
loads the system's decoration plugin (libdecor, and through it GTK and its image loaders) during
its **global** initialisation, before any window exists, and it consults that choice exactly once
per process. So RayClay reads the **primary window's** `RC_AppOptions.nativeFrame` and, when it is
true, tells the host not to bring the plugin up at all: an app that draws its own title bar never
needs it, and the dependency it would drag in is where an app can abort before its first frame on
some distributions (a Fedora 43 GTK plugin aborted when `bwrap` was off `PATH`). When it is false,
the plugin loads as normal and the OS draws your frame.

Three consequences follow, and the first is the one to design around:

- **Every window in the app should agree.** A child opened with the other `nativeFrame` keeps its
  own border flag - the same mix is fine on Windows, macOS and X11, where the window system draws
  the frame - but on Wayland the plugin decision was already made. A decorated child under a
  native-frame primary has no plugin to draw its frame (GLFW falls back to a blank caption strip
  with no title or buttons; the SDL3 host draws no frame at all); a native-frame child under a
  decorated primary draws its own bar inside a hidden plugin frame. RayClay logs one
  `RAYCLAY[WARNING]` per such open, naming which of the two you got. It does not refuse the window.
- **Sequential apps in one process inherit the first decorated one - on the GLFW host.** After an
  app with `nativeFrame` false has run on Wayland, GLFW stays initialised until the process exits (its
  teardown with the plugin loaded is the exit crash the skip exists to avoid), so a later
  `rcAppCreate` with `nativeFrame` true keeps its own title bar but cannot unload the plugin;
  RayClay says so once. The other order is fine: a native-frame app tears the host down fully and
  the next app's choice is honoured. The SDL3 host quits its video subsystem on every shutdown, so
  there both orders work and nothing pins.
- **`RAYCLAY_WAYLAND_LIBDECOR=1` in the environment forces the plugin back on** for a
  native-frame app, without a rebuild, for a compositor where running without it turns out worse.
  It is the only knob: RayClay's own setting outranks the host's equivalent environment variable,
  and it reads the resolved value back and warns if the host disagrees. A
  program that initialises SDL video itself before `rcAppCreate` owns the policy instead - see the
  SDL3 host notes under *Several windows, one app*.

## rcAppTime: the clock to animate from

`double rcAppTime(const RC_App *app)`: monotonic seconds since RayClay brought the windowing
system up. It is the only wall clock the API exposes, and it is what a fade, a spinner, a
countdown or a debounce should be driven from.

```c
static void layout(RC_App *app, void *userData) {
    float t = (float)rcAppTime(app);
    rcBox(.id = "pulse", .bg = rcAlpha(RC_SKY_400, (int)(128 + 127 * sinf(t * 3.0f)))) {}
    rcWindowRequestFrameAfter(rcAppMainWindow(app), 1.0f / 60.0f);   /* on demand, ask for the next frame */
}
```

**Only differences are meaningful.** The origin is windowing start-up, not a date, not
process start, and there is nothing you may persist or transmit. Take two readings and subtract.

**And the origin is not even uniform across two apps in one process.** After
`rcAppDestroy`, a second `rcAppCreate` restarts the clock near zero on Windows, macOS, X11 and
the web, but *continues* from the first app's clock on Wayland once an app there used the OS frame
(`nativeFrame` false), because RayClay then keeps the window host up for the rest of the process
(see *nativeFrame on Wayland* above). ⇒ **Never compare a reading from one app against a reading
from another.**

**Do not build elapsed time by summing `rcWindowFrameTime`: it is not a shortfall, it is a
stop.** That value is an exponential moving average and it *discards* any interval of 1 s or more
outright so one modal-resize stall cannot tank the readout. Under the default on-demand mode an
app woken about once a second therefore never updates the average at all. Measured, 6 s of wall
clock, in two render modes:

```
on-demand, woken ~1/s      9 frames    summed 0.610s   real 6.006s   ratio 0.102
continuous  (CONTROL)   8069 frames    summed 5.999s   real 6.000s   ratio 1.000
```

The control tracks wall clock exactly, so the shortfall is the render mode and not the
arithmetic. ⇒ **On demand, summing frame times under-reports by about ten times.**

**Under `RAYCLAY_FIXED_DT` this clock is frame-indexed, which is the point.** It advances by
exactly the pinned delta per frame rather than reading the real clock, so a benchmark replays
identically. That is also precisely why pairing `RAYCLAY_FIXED_DT` with
`RAYCLAY_RENDER_MODE=ondemand` is refused: time would advance only when a frame is drawn, an
idle park would produce no frames, and a deadline expressed in that time could never come due.

**Why it takes the app handle:** it is the proof that the windowing system is up. A `(void)` form would be callable before that, and the
underlying clock answers differently on desktop and on the web at that moment, which is exactly
the divergence RayClay exists to not have.

## rcWindowClearColor / rcWindowSetClearColor

```c
RC_Color rcWindowClearColor(const RC_Window *window);            /* ALWAYS opaque; opaque black for a NULL app */
void     rcWindowSetClearColor(RC_Window *window, RC_Color color);
```

**The clear colour is the window behind your UI.** It is painted every frame before anything you
draw, and it is the only thing visible on a frame RayClay holds back while it grows its layout
arena. The initial value is `RC_AppOptions.clearColor`.

**Why the setter exists, and it is not symmetry.** `rcSetStyle` changes every colour your UI draws
with, but it cannot reach the window behind it: the clear colour was resolved once, at creation. A
dark/light toggle that does not also call this leaves the *old* theme's background showing wherever
your layout does not cover the window:

```c
rcSetStyle(dark ? rcStyleDark() : rcStyleLight());
rcWindowSetClearColor(rcAppMainWindow(app), rcGetStyle().background);   /* without this the window stays on the old theme */
```

**The alpha is an input sentinel, never an output.** Alpha 0 (to the option or to the setter) means
"use the active style's background". Every other alpha is *discarded*: the window's framebuffer is
opaque and RayClay never presents a see-through window. So `rcWindowClearColor` always reports an opaque
colour, and the colour it reports is exactly the colour painted.

⇒ Round-tripping the getter's value through the setter is a no-op; zeroing its alpha first is a
*reset* to the theme, not a no-op.

**The sentinel resolves once and then freezes.** It reads the theme installed *at the moment of the
call*, a snapshot, not a live link. A later `rcSetStyle` does not move an already-resolved colour,
which is exactly why the setter exists.

**Calling it every frame is safe.** Setting the colour already in force returns immediately, so a
layout callback can set it unconditionally without tracking whether it changed, and an on-demand
app still parks.

**It requests its own frame** when the colour actually changes. That matters for one caller in
particular: a `frameEndCallback` runs *after* the clear and after the buffer swap, so a colour set
there reaches no frame at all. From an update or layout callback the change lands in the frame
already in flight, because the clear is issued after your layout runs.

The getter returns *opaque black* for a NULL app rather than a zeroed colour, whose alpha 0 would
read back as the sentinel and make the documented round-trip lie.

## rcWindowTitlebarHeight / rcWindowSetTitlebarHeight

```c
int  rcWindowTitlebarHeight(const RC_Window *window);      /* PHYSICAL px; 0 for a NULL app */
void rcWindowSetTitlebarHeight(RC_Window *window, int height);
```

**The caption height and the bar you draw are two different things, and on Windows that gap is a
defect the user can feel.** The height is a number you hand the OS; the bar is pixels you draw. They
are set from one number at window creation and then drift apart the moment your bar changes height:
on Windows a bar that folds leaves a vacated strip still swallowing clicks that should reach your UI,
and a bar that grows gets extra pixels the user cannot drag by. (macOS, X11 and Wayland re-read your
drag box every frame, so they follow a folding bar on their own; see below.)

Call the setter whenever your bar's height changes and the two stay in step:

```c
if (app) rcWindowSetTitlebarHeight(rcAppMainWindow(app), (int)state->barHeight);
```

**Calling it every frame is the intended use.** A value equal to the current one returns immediately,
so a layout callback can set it unconditionally without tracking whether it changed.

**It requests its own frame.** An on-demand app is parked until something asks for a redraw, and a
bar that just changed height has to be redrawn to match the strip that already moved. The setter
calls `rcWindowRequestFrame` for you; you do not add one, and the OS region and the pixels cannot
disagree while waiting for unrelated input.

**On the bundled bar it moves the drawn band too.** Under `nativeFrame` with the bundled titlebar
(`titlebar.custom` unset), the setter also updates the band's drawn height, because the runner owns
those pixels and drawing one height while the OS hit-tests another is the same defect inverted. With
`titlebar.custom` the band is your own layout and the setter leaves it alone; you are already
drawing whatever you drew.

**On Windows, a very short strip is not draggable at all, and that is the OS, not RayClay.**
Measured 2026-08-21 against the window manager itself (`WM_NCHITTEST`). Folding `ex20`'s bar from
48 px to 5 px and back:

```text
open      y0-y7 HTTOP    y8-y47 HTCAPTION    y48+ HTCLIENT
folded    y0-y7 HTTOP    y8+    HTCLIENT
reopened  y0-y7 HTTOP    y8-y47 HTCAPTION    y48+ HTCLIENT
```

The caption ends at exactly the value you set, vanishes when you set 5, and comes back. So the
setter does reach the OS. But the top `SM_CYSIZEFRAME + SM_CXPADDEDBORDER` pixels of a resizable
window are the **resize border**, and `HTTOP` outranks `HTCAPTION`. That sum was 4 + 4 = 8 on the
measured machine at 96 DPI; both metrics scale with DPI, so it is larger on a high-DPI display.

So a strip at or below that sum keeps zero draggable pixels on Windows, and because the sum is a
function of DPI and theme there is no fixed pixel value that is safe everywhere. If your folded
state still needs to be draggable, keep a strip comfortably taller than a resize border rather
than a hairline, and treat "a few pixels are still draggable" as something to check on the target
machine rather than assume. Other platforms do not reserve the same band, so this is one of the
few places where an identical `titlebarHeight` gives you genuinely different behaviour.

`height <= 0` means "no draggable strip". The getter reports the resolved caption height in
*physical* px: the units the OS strip is in, not zoom-scaled layout px.

**It does not move the window's minimum height.** That is resolved once at creation from the *open*
bar, deliberately: a folded bar must not license the window to shrink below what the open bar needs.

**Desktop only**, and only Win32 actually reads the value. macOS, X11 and Wayland drive the drag from
the box you draw with `RC_ID_WINDOW_DRAG` and re-read it from your layout every frame, so they follow
a folding bar already. Calling it there is harmless and keeps one source portable: write the call
once and every desktop behaves.

## rcUnzoomed

`rcUnzoomed() { ... }` holds a subtree at a constant on-screen size whatever the content zoom is:
the desktop answer to "this part is chrome, not content". Ctrl +/-/0 and Ctrl+wheel zoom your content
like a browser page; a browser's own toolbar does not grow with it, and neither should a custom
titlebar, a HUD or a status strip.

```c
rcUnzoomed() {
    rcRow(.id = RC_ID_WINDOW_DRAG, .h = "46px", ...) { ... }
}
```

**A custom titlebar needs this, and it is why the scope exists.** `RC_AppOptions.titlebarHeight`
freezes the strip the OS lets you drag in *physical* px at window creation. A band that grows with
the zoom desyncs from it, so the bar you *see* stops matching the bar you can *grab*. Measured on a
46px band with `.titlebarHeight = 46`, reading
`rcGetElementBox(RC_ID_WINDOW_DRAG).height × rcWindowZoom(rcAppMainWindow(app))`, the band's physical height, which
must stay 46:

| content zoom | 0.50 | 0.75 | 1.00 | 1.25 | 1.50 | 2.00 |
|---|---|---|---|---|---|---|
| plain band | 23.0 | 34.5 | 46.0 | 57.5 | 69.0 | 92.0 |
| inside `rcUnzoomed()` | 46.0 | 46.0 | 46.0 | 46.0 | 46.0 | 46.0 |

At 2× the drawn band is exactly twice the configured height; at 0.5×, half. Measured on Linux/Xvfb with
both sizing spellings (`.h = "46px"` and `.hType = RC_PX(46)`); they are different parse paths and
behave identically.

**What is scaled:** fixed sizes (`.w`/`.h` `"48px"`, `RC_PX`), padding, gaps, border widths, corner
radii, floating offsets, text size / lineHeight / letterSpacing, and icon + `rcSvg` boxes.
**What is not, because it is already right:** `"grow"`/`"fit"` (parent- and content-driven), `"%"`
(parent-relative), and `vw`/`vh` (viewport-relative, and the layout viewport is itself window ÷ zoom).
Scaling any of them would double-apply.
⇒ **You do not multiply anything yourself.** Write the px you want on screen.

**The band's height must be the same number you passed to `.titlebarHeight`.** Get it wrong and the
desync is a small *constant* at every zoom rather than a proportional one (2 px for a `"48px"` band
against `.titlebarHeight = 46`), which is much harder to spot.
**On Windows this matters most**, because there the configured height *is* the drag region, so the bar
you see stops being the region that drags; on macOS, X11 and Wayland the drag follows the box you draw
and stays in step by itself.
**A debug build checks it for you** and warns once, naming both numbers. It waits for the zoom to
settle, so a transient during a zoom step will not cry wolf, and it is compiled out under `NDEBUG`:
a Release artifact carries none of it.

Scopes nest, and nesting does not compound: an inner scope is still 1÷zoom, not 1÷zoom².
**`break`, `continue` and a normal exit all close the scope.** The macro is a *double* `for` - the
same shape `rcComponent` uses - so the closing runs on every way out of the block, and the two
behave alike. Measured on all five paths with a counter and `gcc -std=c99`: normal 0, `continue` 0,
`break` 0, a nested `break` 0, and a `break` from inside a surrounding loop 0.
`rcUnzoomedScale()` returns exactly 1.0 outside any scope,
so it is always safe to multiply by; it exists for pixel maths RayClay does not own, such as your own
`RC_CustomDrawCallback`. Everything RayClay lays out inside the scope is already scaled; multiplying
again double-applies.

## Loading an SVG at runtime

`rcSvg` draws an .svg *by path*, straight in your layout. There is no generator step, no compiled
icon header and no build dependency:

```c
rcSvg("assets/logo.svg", 48.0f, s.text);            /* that is the whole thing */
```

Nothing in your app struct, nothing to load, nothing to free. The first frame that names a path
parses it, every later frame reuses that parse, and the library frees it at `rcCloseWindow`.

`rcLoadSvg` / `rcLoadSvgFromMemory` parse into an opaque `RC_Svg` handle that *you* own, and
`rcSvgHandle` draws from one. That is the route to reach for when the markup has no file behind
it, or when you want to free on your own schedule:

```c
RC_Svg *logo = rcLoadSvgFromMemory(LOGO_SVG, (int)(sizeof LOGO_SVG - 1));
...
rcSvgHandle(logo, 48.0f, s.text);                   /* per frame          */
...
rcUnloadSvg(&logo);                                 /* NULLs your pointer */
```

**`rcUnloadSvg` frees on the same terms as `rcUnloadImage`: call it whenever you are finished with
the artwork, the layout callback included.** Your pointer is cleared immediately and the document
is released at the frame boundary, so an `rcSvgHandle` already drawn this frame keeps its artwork
until the frame reaches the screen. There is no ordering for you to get right.

**Colour is a per-call argument, not a property of the artwork.** The same SVG can be drawn at two
  sizes in two tints in one frame, and re-tinted every frame, with no reload. A *zero-alpha* colour
  means "unset" and resolves to the active style's text colour, so a mono icon stays visible on
  either theme rather than becoming transparent. Artwork with its own baked colours ignores the
  tint: the colour is then part of the drawing, exactly as it is for a generated icon.

**Both draw calls end in `rcIconEmit`'s (size, color)**: `rcSvg`(path, size, color) and
  `rcSvgHandle`(svg, size, color), precisely as a generated icon header does. Moving between any
  two routes is a one-line change, which is what makes the choice below a question about your
  *build* rather than a rewrite.

**Which route**: four, and the first is the one to reach for. All four end in the same icon ops and
the same drawing code, with the single exception recorded after the list.
examples/ex24_svg_live draws three of them at once on one artwork.

  1. `rcSvg`("path.svg", size, color): **the default**. No app state, no load, no unload.

  2. `rcLoadSvgFromMemory` + `rcSvgHandle`: the markup is a static const char[] in your source, or
     a string your program built this frame. One executable, no converter, no asset files.
  3. `rcLoadSvg` + `rcSvgHandle`. Own the lifetime: free on your own schedule, or swap which handle
     is drawn from frame to frame.
  4. A generated icon header (ex11_rayclay_icon_converter). The parser stays out of the
     binary entirely, there is no parse at startup, and the icon *cannot* fail to load.

**The one artwork shape where routes 1-3 and route 4 disagree.** The runtime parser classifies the
  WHOLE document and picks a single builder for it, and its monochrome builder reads only the stroke;
  the converter reads each shape's own fill and stroke whichever generator it picks. They part on
  exactly one class: no shape anywhere bakes a concrete colour, at least one shape carries a FILL
  paint, and at least one other shape carries no paint at all. For that document the parser puts every
  shape on the monochrome path, so the filled shape is emitted as an outline of its own silhouette
  while the generated header for the same file fills it. Neither route logs anything. A document whose
  paints are all STROKES draws identically both ways, so the trigger is a FILL sitting beside an
  unpainted shape, not merely a mixture. No artwork RayClay ships is in that class - every shape in
  every bundled `.svg` carries a paint. **The repair is in the artwork, not the call:** give every
  shape a paint and all four routes agree again.

**Any route that names a path**, `rcSvg`("...") or `rcLoadSvg`("..."), **costs you the
  single-executable property, and nothing warns you:** the artwork becomes a file on the end user's
  disk that has to ship, install and stay put. That is the trade route 1 makes for its
  convenience, and for most apps it is the right one. When it is not (a single binary you can
  email), route 2 buys the property back for the price of a string literal, with no converter and
  no build step. The two are one line apart, so this is a decision you can defer.

**What route 4 still buys over route 2**, all small but real: 86,080 B of parser stays out (measured
on ex24, Release + -DRC_SIZE_OPT=ON, stripped; ex24 *calls* `rcLoadSvg` directly, and the saving for
an app that reaches the parser only through `rcSvg`'s cache may differ); the artwork is
compact point arrays rather than XML text; there is no 21-41 µs per-icon parse at startup; and a
baked icon cannot fail to load.

### How rcSvg's cache behaves

**The key is the path's bytes**, not the pointer, and the cache keeps its own copy. So building a
  path into a scratch buffer every frame works exactly as well as a string literal, and two call
  sites naming the same file share *one* parse; they do not each get their own.

**A failure is cached too**, and that half is what makes the shape safe at all. A missing or
  unparseable path is opened *once* and logged *once*; every later frame draws nothing, in silence.
  Without it a typo'd path would re-open a missing file sixty times a second and bury the very
  log line it was writing. Measured on ex24 with a deliberately wrong working directory: 60
  frames, *two* call sites naming the same missing file, *one* "cannot open" line.
  **The message names the function you called** (`rcSvg`, `rcLoadSvg` or `rcLoadSvgFromMemory`), even
  though all three reach one loader, so a log line always points at code you actually wrote.

**A path that fails is remembered as failed for the process.** Fixing the file on disk while the
  app is running will not bring the artwork back; the miss is cached exactly as a hit is. Restart,
  or take route 3 and control the loads yourself.

## RC_SVG_CACHE_MAX

How many *distinct* paths `rcSvg` will hold at once. Default 64, and it is **non-evicting**: past the
limit the call warns once and draws nothing, rather than silently re-parsing every frame. The cache
key is the path's bytes, so `"a.svg"` and `"./a.svg"` are two entries.

⇒ **An app cycling through hundreds of SVG files wants `rcLoadSvg` handles and its own lifetime
policy**, not a bigger number. The cache is for the fixed set of artwork an app ships with.

**RayClay does not rasterise SVG** - shapes become icon ops. `path`, `line`, `polyline`, `polygon`,
`rect`, `circle` and `ellipse` are supported. Gradients, `<text>`, filters, `<image>` and `<use>` are
skipped with a warning naming the family. A partial result is normal. For artwork needing any of
them, export a PNG and use `rcLoadImage` - that is a picture, so it will not re-tint.

### The two ways an icon fails SILENTLY

Both are common in real exports, and neither logs anything.

**1. A shape with no paint at all.** RayClay's root `fill` starts at `none` where the SVG spec's
initial value is black, so a bare `<path d="..."/>` with no `fill` is drawn as a hairline outline of
its own silhouette - about 0.05 px in a 960-unit viewBox at 24 px. **The icon is effectively blank.**
Material Symbols and Font Awesome ship exactly this markup and colour it with CSS. **The
one-attribute repair is `fill="currentColor"` on the root `<svg>`.** Sets that name a paint work as
drawn: Bootstrap Icons use `fill="currentColor"`, Lucide and Feather use `fill="none"` with
`stroke="currentColor"`.

**2. A `<style>` stylesheet.** There is no selector engine and `class` is not a name the parser
knows, so it fails silently. This is the likeliest way a real export loses all its colour:

```
<style>.st0{fill:#231F20;}</style><path class="st0" d="..."/>     <- fill is lost, no warning
<path fill="#231F20" d="..."/>                                    <- what to export instead
```

Illustrator's "Style Elements" setting produces the first form. Export with presentation attributes
or inline `style=""`. A shape left with neither fill nor stroke is dropped rather than drawn
untinted, so the symptom is a *missing* shape.

`<defs>`, `<clipPath>`, `<mask>`, `<pattern>`, `<marker>`, `<symbol>`, the two gradients and
`<filter>` declare reusable content and draw nothing where they sit, nested shapes included - exactly
as a browser treats them. What they cannot do is APPLY: a `<clipPath>` or `<mask>` does not clip your
art, and a `<use>`/`<symbol>` reference is not resolved. Both warn and say to flatten in your editor.

### Attributes the parser ignores: the ones that draw your art WRONG

An unsupported *element* disappears and you go looking for it. An unsupported *attribute* leaves the
element on screen drawn wrong, so the app looks like it has a layout bug. Each logs one warning:

```
attribute                        what you drew          what RayClay draws
---------------------------------------------------------------------------------------
transform                        art placed and scaled  art at its RAW viewBox coordinates
display="none" / visibility      a hidden layer         THE HIDDEN LAYER, DRAWN
opacity / fill- / stroke-        a blend                fully opaque
fill-rule / clip-rule            an evenodd hole        the hole FILLED SOLID
stroke-dasharray                 dashes                 one solid stroke
```

One warning per family per document, and on the `rcSvg` route once per distinct path spelling for
the life of the process - editing the file while the app runs will not produce a second one.
`stroke-linecap` and `stroke-linejoin` are ignored and deliberately do NOT warn: they appear on
almost every healthy icon, and a diagnostic that fires on healthy input teaches you to ignore it.

**The fix for a transform is in the asset.** Flatten it in your editor, then reframe the viewBox
around the art as drawn (Inkscape "Resize page to drawing"; in Figma, frame the art and export the
frame). **The rule underneath: RayClay maps your viewBox onto the box.** It does not compute the
drawing's bounding box, so art sitting high inside its viewBox draws high inside your element, and
`.align` cannot correct it - `.align` places the box, and the box is already where you asked.

**The complete list of attributes whose value the parser reads** is 21. Anything else is ignored,
whether or not it warns:

```
cx  cy  d  fill  height  points  r  rx  ry  stroke  stroke-width  style
viewbox  viewBox  width  x  x1  x2  y  y1  y2
```

`fill`, `stroke` and `stroke-width` are read from an inline `style=""` first and only then from the
matching presentation attribute, exactly as CSS says.

### Lengths, percentages and units

A bare number is user units, which is what an icon export gives you. A percentage resolves against
its own reference rather than one shared basis:

| a percentage on | resolves against |
|---|---|
| `x` `x1` `x2` `width` `rx` `cx` | the viewport **width** |
| `y` `y1` `y2` `height` `ry` `cy` | the viewport **height** |
| `r` and `stroke-width` | the normalised **diagonal**, `sqrt((w*w + h*h) / 2)` |

Every route resolves a percentage the same way, so an icon baked ahead of time matches the same file
drawn through `rcSvg`. With no viewBox to resolve against, the bare number is kept and you are told.

`px` is dropped without a word - 1 px *is* 1 user unit. `pt`, `pc`, `mm`, `cm`, `in`, `em`, `ex` and
`rem` are also read as their bare number, so a stroke asked for in points comes out narrower than you
meant, and a file carrying one says so once. Convert the number yourself:

```
<path stroke-width="2pt" .../>     <- drawn as 2 user units, and reported
<path stroke-width="2.667" .../>   <- what to write instead
```

### Limits and failures

**`RC_ICON_MAX_PTS` (128) caps the vertices in one flattened path.** Each cubic or quadratic segment
flattens to 16 points; an arc flattens at 7.5 degrees per step, minimum 4, so a 90-degree arc costs
12 points. ⇒ **more than 8 curve segments in one OPEN subpath will be truncated** - countable in your
editor in a way "128 points" is not. A closed subpath gets one further, because the duplicate closing
point is dropped. Truncation warns and names the remedy; splitting is not offered, because splitting
a fill seams its triangulation and splitting a stroke gaps it.

A path command the parser cannot read logs `malformed path; skipped incomplete tail` once per
document: it skips to the next command letter and carries on, so you get a *partial* path, not a
dropped file. Minified output from svgo, Figma and Illustrator is safe - both arc-flag spellings
parse, and a flag that is neither `0` nor `1` is refused rather than coerced.

**A failed load returns NULL and is not fatal.** The reason is logged and `rcSvgHandle(NULL, ...)`
draws nothing, so a missing asset degrades to a gap in the UI. Check the handle if the art is
load-bearing. On the `rcSvg` route there is no handle to check: the failure is cached and logged
once instead.

**Trusted input only**, the same posture `rcLoadFont` documents: the pools are bounded and overflow
warns rather than grows, but the document inside them is not.

`RC_NO_SVG` compiles the parser out and **all five public SVG symbols stay**, so a consumer needs no
`#ifdef`. `rcLoadSvg`, `rcLoadSvgFromMemory` and `rcSvg` warn by name; `rcSvgHandle` draws nothing
and `rcUnloadSvg` still NULLs your pointer. With section-GC on, an app naming none of the five
already dead-strips the parser, so the knob only shows a saving on an app that draws an SVG.
  → docs/cheatsheet.md ▸ build-time configuration

## RC_ICON_POOL_CAPACITY

The pool holds every icon payload kept live in one frame, from the layout pass until `rcRender`
consumes it. It is one process-wide, chunked, grow-on-demand allocator owned by the
library: blocks are added as a frame needs them and are never moved or freed mid-run.

**So there is no per-frame icon ceiling to budget against.** An icon-dense frame grows the pool
instead of failing, and `RC_ICON_POOL_CAPACITY` (256) is the *growth step* (the block size), not a
hard limit. Raising it trades a little idle memory for fewer allocations on a very icon-heavy
frame; leaving it alone is the right default.

**The invariant that matters**, if you are wondering why it is a block list rather than one array:
the layout engine holds these pointers from the layout pass until `rcRender` runs, so a realloc'd
flat array would dangle every pointer already handed out *this* frame. A block list cannot.

**The count is per process, not per source file.** A #define in your own file
cannot raise the pool that `rcSvg` emits from, because that is the library's TU. One name, one
value, one meaning, in both the split build and the amalgamated header.

**AND THE EDGE THE PARAGRAPHS ABOVE DO NOT COVER: what a frame looks like when the pool cannot serve
  it.** "Grow on demand" is true, but growth happens **between** frames by design - the cursor keeps
  counting past capacity all frame so that at the next boundary the pool knows what the frame really
  needed. So a frame CAN outrun the pool, and when it does:

- **The icons past that point do not draw, and nothing else moves.** They keep their layout box, so
  the page does not reflow and no other icon is affected - each slot belongs to one icon or to none.
- **The next frame is correct.** The pool has grown by then and serves all of them, which is why
  this shows up as a one-frame gap on the first icon-heavy frame and never again.
- **You get told, once per process, and the three cases are deliberately different messages**
  because the remedy is different. *Outgrew the pool* names `RC_ICON_POOL_CAPACITY` and the current
  value: define it past your peak icons per frame, before the `#include`, to avoid the first-frame
  gap. *At the ceiling* means the pool cannot grow any further - 64 blocks of
  `RC_ICON_POOL_CAPACITY`, so 16,384 icons at the default, and raising the knob moves it; but a
  single frame emitting that many is a runaway layout, not a sizing problem. *Could not allocate*
  is an allocation failure, and it says so plainly, because a diagnostic that cannot tell "too many
  icons" from "no memory" sends you to size a pool that was never the problem.

The block count is internal and is not a public constant; the warning prints it, which is the right
  place for a number you can only react to at run time.

## clip slots

**There are only 100 clip slots per frame, and a cell that never scrolls still takes one.**
  Every element declaring .clip / .overflow / .scroll consumes a slot whether or not it is
  scrollable, so a *grid* of clipped cells hits the ceiling long before anything scrolls: 8 clipped
  cells per row is ~12 rows. Raising maxLayoutElements does *not* raise this; the slot array is a
  fixed 100.
**And the ceiling counts slots HELD, not elements on screen**, which is why it can refuse while you
  are looking at far fewer than 100 clipped boxes. A slot outlives the container that opened it, and
  when it is reclaimed is the host's business, so swapping between two pages of 60 clipped elements
  can want both sets held at once. Count turnover, not the visible cells, or the limit will look
  wrong; `rcClipSlotCounts().rejected` is the reading that settles it.
Reported by a team shipping on RayClay: their table clipped one box per cell and simply
  stopped working past ~12 rows. Past the limit an element *still lays out and clips* correctly;
  what it loses is the scroll offset *outright*, not merely across frames: it is discarded in the
  very frame it is set. Measured with two cells declaring the *same* offset: cell 99 (slotted)
  moved -10.00, cell 100 (past the ceiling) moved 0.00. The two failures compound, so you cannot
  route around one with the other: rcScrollTo* is ignored *and* the offset you set by hand never
  applies.
**The symptom you will actually see**, measured with 120 clipped cells against a baseline of 80:
  the 120th element has a real box (`rcGetElementBox` reports found, 60x12, it renders), but
  `rcGetScrollInfo` reports .found = false for it. So an element that is plainly on screen reads as
  "not a scroll container", and `rcScrollBy` / `rcScrollToBottom` against its id do nothing.
You get told: RayClay logs one warning naming clipping and the remedy (once per `RC_App`, not
  per frame).
**The fix:** clip the *container*, not each cell. A row of plain cells inside one clipped scroll body
  costs one slot.
**For the per-cell overflow itself, there is no ellipsis:** `RC_TextOptions` carries no
  `text-overflow` field, so a cell cannot be truncated with a trailing "…" for you. What you have is
  `.wrap` (`""` lets a long cell wrap to more lines instead of overflowing) or shortening the string
  yourself before you draw it.

## rcClipSlotCounts

**You do not have to discover the ceiling above by hitting it.** `rcClipSlotCounts()` returns an
  `RC_ClipSlotCounts` describing the pool right now, and it is **unconditional** - unlike
  `rcAppPerfFrame` it is not behind `RC_PERF_COUNTERS`, so a shipping build can assert on it without
  a second build configuration. It is zeroed rather than faulting before the first layout, so a
  status pane may poll it freely.

| field | what it means |
|---|---|
| `capacity` | slots the pool holds; fixed for the context |
| `stored` | slots held now. A slot outlives the container that opened it, and when it is reclaimed is the host's business, so do not read a fixed one-frame lag into it |
| `attempted` | what this pass asked of the pool. **Use it with `rejected`, never alone**: it counts slots on the seated path and *elements* above the ceiling, so it errs high there, and a pass can be refused while `attempted` is below `capacity` because the pool may be full of slots whose containers have just left |
| `rejected` | **the alarm.** Non-zero means scroll offsets are being discarded right now, on every frame, whatever the other four read |
| `highWaterMark` | peak demand since init, never reset. Greater than `capacity` means something was turned away at some point - which the once-per-context warning cannot tell you at frame 10,000 |

**Why a census and not just the warning:** the overflow error is latched **once per context**, so
  frames 2..n say nothing. A developer who arrives after frame 1 sees a list that has simply stopped
  scrolling, with a clean log. `rejected` and `highWaterMark` are what make that state readable at
  any moment. The external report behind this was a session list that stopped scrolling at 48
  entries and cost an afternoon.

**Watch one thing when you read it:** opening the layout debug inspector **raises these numbers by
  its own chrome** (measured: +2 slots). That is deliberate and is not an artefact to subtract - the
  inspector takes real slots and can itself cause refusals near a full ceiling, so a census that hid
  them would conceal a defect the inspector created.

`examples/ex12_rayclay_inspector` is the worked case: a `clips` row on its PERFORMANCE panel showing
  `stored`/`capacity` with an `rcProgress` bar, and one caption line under it carrying the peak. Two
  things there are worth copying. **The caption is drawn in every state, healthy or not** - a line
  that appears only on a fault reflows the panel at the moment you are reading it, and a reader who
  has never seen it does not know the app watches for this. And **it never claims "none ever"**:
  `rejected` is a statement about this pass, and `highWaterMark <= capacity` does not prove nothing
  was turned away, for the reason in the `attempted` row above. Only the reverse holds.

## RC_String.isStaticallyAllocated

**A public, writable field that silently renders stale text.** `RC_String` is a transparent alias for
  the layout engine's own string type, so this field is reachable from your code and is part of the
  public surface. Compiled against the public header at -std=c99 -pedantic-errors -Wall -Wextra,
  this is clean C, and wrong:

```c
RC_String s = rcStringFromCStr(p);
s.isStaticallyAllocated = true;      /* renders stale text forever */
```

**The mechanism.** With the flag *set*, the measure cache keys on the *address*
  (`hash += (uintptr_t)text->chars`) plus the length; with it clear, it keys on the *contents*.
  Separately, the scratch arena is a bump allocator (`rcArenaAlloc` advances currOffset) that the
  runner resets once per frame (`rcArenaReset(&app->scratchArena)`).
  ⇒ The Nth `rcFormat` of frame N+1 lands at the *exact* address of the Nth of frame N. Same address,
  same length, flag set ⇒ cache *hit* ⇒ the previous frame's measurement, forever.

```
static=0  f1("AAA")=24.0  f2("WWW")=60.0   correct
static=1  f1("AAA")=24.0  f2("WWW")=24.0   STALE - and zero errors logged
static=1  f1("AAA")=24.0  f2("WWWW")=80.0  correct (a LENGTH change breaks the key)
```

**It bites hardest where it looks safest:** every changing number of *constant width*. "45%"->"92%",
  "2019"->"2020", "1.23"->"9.87" all keep their length, so the key never changes and the old width
  is served with no warning and no error.
**Storage duration is not content stability**, and this is the part that catches people. A
  `static char[]` that you *rewrite* does *not* qualify, even though `static` is right there in the
  declaration. A `static char[N][M]` ring rewritten every frame is the shape that fools people: the
  storage lives forever, the *content* does not. It is
  also why no automatic "is this address stable?" check is possible: such a check passes a stack
  buffer, a refilled malloc, a caller ring and that static char[] alike.
**The rule:** set it only when the **bytes at that address never change** for the life of the program:
  a string literal, or a table you fill once and never touch. Leave it clear for anything from
  `rcFormat`, any buffer you rewrite, and anything you are unsure about. Clear is always correct;
  it costs a content hash.
`rcTextL` literals set it for you, correctly. You never need to set it by hand.

## RC_String.length

**Bytes** (not codepoints, not characters) and **signed** (`int32_t`). RayClay's text is UTF-8, so
a `.length` you compute yourself can cut in the middle of a character: dropping N from it drops N
*bytes*, which outside ASCII is not N characters. Both producers fill it in correctly for you:
`rcStringFromCStr` stores the `strlen` of what you passed, `rcFormat` the formatted byte count.

**It is the whole bound, and the only one.** RayClay does not copy your bytes and is never told
your buffer's size (the three-field `RC_String` is the entire contract), so a `.length` *longer* than
the bytes you own is read past them. An over-long *positive* length is undecidable here, and it stays
your bug.

**A negative length is treated as length 0.** It is clamped as the string enters the layout engine,
so none of your bytes are read and no text is drawn: a safety net for a subtraction that came out
backwards, not an idiom for hiding text.

**"The text disappears" is the whole promise**, not "the element does". Exactly as for `""`, the
element is still declared and still consumes an element slot.

You only reach this field by writing it yourself. `rcTextL`, `rcTextC` and every `rcFormat` result
already carry a correct length; `rcText` forwards the `RC_String` you built verbatim.

## rcClicked and the pointer cursor

Polling `rcClicked` (or `rcPressed`) also gives that element the web's clickable-hand: while hovered,
the frame's cursor defaults to `RC_CURSOR_POINTER`. Both predicates share one hit-scan, so the hint
costs nothing extra and appears whichever edge you chose.

Opt out app-wide with `RC_AppOptions.autoCursorsDisabled`, or per-element by calling
`rcSetCursor`(`RC_CURSOR_DEFAULT`) after the poll, e.g. a click-to-dismiss backdrop.
RayClay auto-sets the expected shape per component; autoCursorsDisabled turns those defaults off.

The corollary is the one that bites: an element that reads `rcIsHovered` plus a raw
`rcPointerPressed` has polled neither predicate, so it gets no pointer hint. Use `rcClicked`, or name
the cursor yourself. `docs/widgets.md` has the worked case.

What it shows instead depends on what is inside it. An empty box shows the arrow. A box containing
ordinary **selectable text** shows the **I-beam**, because selectable text carries a weak cursor hint
of its own - this is CSS `cursor: auto` resolving over text, and a browser does exactly the same with
a `<div onclick>` that never set `cursor: pointer`. The hint is weak by construction, so the moment
you poll `rcClicked` the pointer wins it back; it only surfaces when nothing claimed the cursor at
all. Chrome is unaffected either way: a run marked `RC_SELECT_NONE` - every button label, menu item,
tab and title - emits no hint, so it can never become an I-beam.

**A CLICKABLE INSIDE A CLICKABLE: GUARD THE OUTER POLL, AND ORDER DECIDES IT.** When the pointer is
over the inner control it is over the outer one too, so both polls see it and the LATER poll takes
the press. A card that polls `rcClicked` for itself therefore swallows the click of a Clear or Close
drawn inside it: the inner control still highlights on hover and simply never fires. **The library
names the collision for you** - both ids, once per ordered pair - so read the log before you go
looking at hit boxes, because the hover fill painting over a dead control is what makes this read as
a paint bug. Ask the cheap hover question first, so `&&` short-circuits the outer poll away on exactly
the frames the inner one needs it gone.

```c
if (rcClicked("row_clear")) {          /* the inner control is polled FIRST */
    clear_row();
} else if (!rcIsHovered("row_clear") && rcClicked("row")) {
    open_row();                        /* rcClicked("row") first here would eat the clear */
}
```

**THE GUARD COSTS ONE GESTURE, AND IT IS WORTH KNOWING WHICH.** A press that STARTS on the card and
ENDS over the inner control fires nothing at all. On the release frame the pointer is over the
inner control, so `&&` removes the only poll that could still have matched that press, and the card
click a user expects after a few pixels of drag is dropped. Nothing leaks: an unconsumed press is
discarded on the next frame, so no stray click arrives later. The trade is usually right, because
the alternative is a Clear button that never works at all. **Note the asymmetry: the collision is
reported and this is not.** Once the guard is in, there is no collision left to name, so a card that
"sometimes does not open" is on you to recognise rather than hunt.

## rcModDown and RC_MOD_PRIMARY

Use `RC_MOD_PRIMARY` for app shortcuts (Cmd on a macOS desktop build; Ctrl **with Alt up**
everywhere else), so they land on the right key with no per-platform branch. The Alt clause is
deliberate and it is the part that surprises people: Windows AltGr presents as Ctrl+Alt, so a held
Alt makes `RC_MOD_PRIMARY` read false rather than firing a shortcut under someone typing an
accented character. `RC_MOD_CTRL` stays a raw physical poll of the Ctrl keys, so the two
legitimately disagree on the same frame. Use PRIMARY for accelerators and CTRL only where you mean
the physical key, such as word-jump arrows.

**On the web the pick cannot be made at compile time**, because one binary serves every visitor's
OS, so `RC_MOD_PRIMARY` accepts **either Cmd or Ctrl-with-Alt-up** there. A macOS visitor in a
browser gets Cmd, which is what every other page on their machine has taught them; a Windows or
Linux visitor still gets Ctrl, and AltGr still cannot fire a shortcut. Only the DESKTOP builds pick
one at compile time (`__APPLE__`). So the same source needs no per-platform branch on any target.

## rcPointerReleased

The drag shape (latch on the press, track while down, commit on release):

```c
static bool dragging;  static float x0, x1;
RC_Vec2 p = rcPointer();
if (rcPointerPressed(RC_POINTER_LEFT) && rcIsHovered("plot")) { dragging = true;  x0 = x1 = p.x; }
else if (dragging && rcPointerDown(RC_POINTER_LEFT))          { x1 = p.x; }         // live edge
else if (dragging && rcPointerReleased(RC_POINTER_LEFT))      { dragging = false; } // commit
```

**A position is only half a mapping.** To turn a pointer x into a value you also need the rect you are
  mapping *into*; that is `rcGetElementBox` / `rcChartPlotRect`, immediately below. A gesture that is purely
  *relative* (a drag-scrub, dragging a card, a custom divider) needs only the delta between two `rcPointer`()
  reads and no rect at all.
**A held drag keeps tracking past the window edge, and an idle pointer outside it reads as a sentinel.**
  While any pointer button is down, `rcPointer`() reports the real cursor even outside the window, and on
  the web even outside the canvas, so a slider, a scrollbar thumb or a brush follows the pointer across the
  rest of the page. Coordinates there sit outside the element box, negative or past its width, so the clamp
  is your decision: `rcSlider` and `rcScrollbar` clamp for you, a hand-rolled brush does not.
With **no** button down and the cursor outside the window, the position is replaced by a large negative
  sentinel, `-1e6` before the zoom and pan transform, so nothing reads as hovered and no tooltip dwells.
  **That is why the sample above commits `x1` and never reads `p` on the release frame.** The button can
  also drop without a real release (on the web, the page losing focus clears it), and the release edge then
  arrives on a frame whose position is that sentinel. Track the value on the down frames, gated by
  `rcIsHovered` and your own `dragging` latch, and the commit is already in hand.
On-demand builds: a gesture is pointer motion, so frames come for free while the mouse moves. But if you
  *animate* the commit (an eased zoom), that is your own state changing: call `rcWindowRequestFrame`(app) per
  step or it will not draw.

## RC_Box

  `RC_Box` is { float x, y, width, height; bool found; }, in content space, so it compares directly
  against `rcPointer`().
  **One frame behind by construction**: layout runs *after* your callback, so this reports the last
    completed layout: exact while the scene is static (the normal case for a gesture, which spans
    many frames anyway) and one frame stale through a resize.
  **Guard on the extent, not on .found:**   if (b.found && b.width > 0.0f)
    `.found` answers "does this id exist?", **not** "is the rect ready". On an element's *first* frame
    you get found = TRUE with an all-zero rect. The layout engine registers an element when it *opens* and fills
    its box only when layout *ends*, so `if (b.found)` alone divides by zero.
    Use `.found` to catch a *typo'd id*; use the extent to catch the *first frame*.
  `rcChartPlotRect` returns the axes' inner region, excluding the tick-label gutters, the x-axis
    strip and the legend/title row. That is deliberately **not** `rcGetElementBox`(chartId): the chart
    sizes its plot inside the box you gave it (the y gutter grows with the widest tick label, a
    legend takes a header row), so mapping against the *outer* box is wrong by however much chrome
    the chart chose. The plot's own id is internal and hash-falls-back for long ids, so you cannot
    ask for it by name. Same one-frame settle, same .found contract.

## Build knobs: where to define them

*Where* you define a knob decides whether it does anything, and getting it wrong is **silent**.

**The rule: define a knob for every translation unit**, in your build system's compile definitions
rather than as a `#define` above one `#include`. Most knobs are read only in the TU the implementation
is compiled in, a few are read in yours as well, and two are read in *both*; defining it everywhere is
correct for all of them and needs no per-knob lookup.
**Three knobs are the exception** and the cheatsheet tags them `[CONSUMER-VIEW]`:
`RC_NO_UI_HELPERS`, `RC_NO_STYLE`, `RC_NO_COLOR_PALETTE`. They trim the *declarations* and must **not**
reach the implementation, which needs the header in full.
**[both]** marks the knobs read on both sides: `RC_CHART_MAX_SERIES` and `RC_DEBUG_TOOLS`. Setting
one of those on a single side is the worst case in this whole section: no `#error`, no link error,
just your code and the library working from different numbers.

```
single header (rayclay.h alone)     the SAME TU that defines RAYCLAY_IMPLEMENTATION, above the #include
CMake (add_subdirectory/FetchContent)   -D on the RAYCLAY target, not on yours
```

**The instinct to avoid** is the stb one: `#define RC_FONT_ATLAS_W 2048` at the top of your own main.c
and then `#include "rayclay.h"`. That TU gets **declarations only**, so the define is read by nothing.
Run-verified both ways on the shipped single header: with RAYCLAY_IMPLEMENTATION a bad value stops the
build with an #error; without it, the same bad value compiles clean and the atlas stays 1024.

**Only three knobs are refused at build time, and the refusal is narrower than it looks.** The usual
advice ("a build knob is silent unless you define it where the implementation is compiled") is true
for most and fatal for three. The three are exactly `RC_NO_COLOR_PALETTE`, `RC_NO_STYLE` and
`RC_NO_UI_HELPERS`.

```
CONSUMER-VIEW    must NOT reach the implementation: these trim the DECLARATIONS, and the
                 implementation needs them in full. The AMALGAMATED header refuses it with #error.
both             read on BOTH sides; one-sided values diverge SILENTLY (RC_CHART_MAX_SERIES,
                 RC_DEBUG_TOOLS)
untagged         read where the implementation is compiled; a consumer-only define does nothing
```

**The `#error` is single-header only.** The single-header build emits those three arms inside
`#if defined(RAYCLAY_IMPLEMENTATION)`, and a multi-TU CMake build from the RayClay source tree never
defines that macro, so in a source build the same mistake is silent again. **Follow the rule; do not
rely on the diagnostic to catch you.**

So a project-wide -DRC_NO_COLOR_PALETTE (via CMAKE_C_FLAGS, or target_compile_definitions on the
library target) **fails the build** with "cannot be combined with RAYCLAY_IMPLEMENTATION in one translation
unit". Define a consumer-view knob on **your own** target only.

And do not reach for the consumer-view three to save space: they trim your view of the header, not
the library. With section-GC on (which the shipped CMakeLists.txt enables unconditionally), code you
never call is already collected, so the size win is ~0. The size levers are all in the implementation
class. (Classified by compiling both kinds of translation unit against the shipped header,
2026-08-04.)

**Test a knob by value, not by existence; then ask which TU is reading it.** Measured on the shipped
header, 2026-08-08, compiling one consumer TU and one RAYCLAY_IMPLEMENTATION TU:

```
knob              your own TU sees    the implementation TU sees
RC_DEBUG_TOOLS    #define'd to 0      0
RC_GFX_DIGEST     undefined           0
RC_GFX_PACKET     undefined           1   <- the packet renderer is the DEFAULT
```

⇒ `#ifdef RC_DEBUG_TOOLS` is true in a stock build, so a guard written that way reports the inspector
*on* when it is off. Write `#if RC_DEBUG_TOOLS`. The other two are defaulted where the library itself
is compiled, so they answer only in that TU.
The RC_NO_* family is the opposite again: never defined by the library, so `#ifdef` is the correct
test there.

## Selecting the alternate renderer

**The spelling depends on which RayClay you build, and one of the two is silently ignored.** If you use
the single-header drop (the normal case, and the only one if you vendored rayclay.h plus its
CMakeLists.txt), `cmake -DRC_GFX_PACKET=0` does **nothing**. It is an unread cache variable there: the
`if(DEFINED RC_GFX_PACKET)` block lives in the full source tree's root CMakeLists.txt, which is
not what the drop ships.

```
# works everywhere, including the single-header drop
cmake -S . -B build -DCMAKE_C_FLAGS="-DRC_GFX_PACKET=0"

what you build        -DRC_GFX_PACKET=0    -DRC_GFX_PACKET=2        -DCMAKE_C_FLAGS="-D..=0"
the drop              ignored, packet      ignored, packet          alternate
the full source tree  alternate            configure FATAL_ERROR    alternate
```

(Measured 2026-08-09 against the single-header drop, entered the supported way with add_subdirectory, and
against the full source tree.)

**The typo hazard is still live for a drop consumer:** `#if 2` is true, so -DRC_GFX_PACKET=2 in
CMAKE_C_FLAGS compiles the packet arm (the very renderer you were escaping), and no configure step
objects. Do not trust the flag you passed; read the arm back out of the built library:

```sh
nm librayclay.a | grep -c rci_gfx_sokol_pkt_      # 0 = alternate, NON-ZERO = packet
```

**Take the zero-vs-non-zero, never the magnitude.** The same property counted 4 on a drop build and 2
on a modular development build, and a third nm scope once read 15. A counter's name is not its unit;
only its being zero means anything here. This has already caught a real consumer: a guard asserting
that the experimental renderer was **not** enabled fired, because the flag they passed had been
silently ignored.

## RC_GFX_MSAA_SAMPLES

**The largest safe memory dial** in the library: the multisampled framebuffer is allocated at full
window size for *every* sample.

**You do not have to rebuild to compare arms.** `RAYCLAY_MSAA=1|2|4` overrides the compiled default
for *one* process, so an A/B holds the binary constant and the arm becomes a property of the *run*:

```
RAYCLAY_MSAA=1 RAYCLAY_GFX_STATS=1 ./myapp     # 1 is how "off" is spelled
RAYCLAY_MSAA=4 RAYCLAY_GFX_STATS=1 ./myapp
```

**It takes 1, 2 and 4 and refuses everything else, warning and falling back to the build default**,
including 8, which the macro will accept. 8 was measured coming back as 4 on an M3, and a knob whose
value the driver silently replaces is how a measurement lies.
`RAYCLAY_GFX_STATS=1` prints one line per window (the resolved *request*, what the framebuffer
actually reported, and the backing-store size), so a figure can be attributed to an arm instead of
guessed at:

```
rc_gfx stats: msaa_requested=4 msaa_framebuffer=4 fb=1600x1200
```

`fb` **is device pixels**, deliberately with no DPR column: the logical viewport here is zoom-divided,
so fb/logical is contentScale x zoom and equals the DPR only at zoom 1.0. The count reads `n/a`,
never 0, when it could not be read: 0 is the dummy/web backend's null, and a null printed as a
number is how a blank column becomes a false claim. Both knobs are opt-in and silent otherwise: a
shipped app must not print diagnostics at its user.
Measured on macOS, where these regions are actually accounted:

```
ex00_hello      4x = 380.1 MB   vs  1x = 327.0 MB   physical footprint
bare GL window  4x = 192.8 MB   vs  0x =  90.5 MB
```

**Whether your tooling can see this depends on where the buffer lives**. There are three regimes,
  and "Linux under-reports it" is true of only one of them:

```
discrete GPU:   the buffer is in VRAM, so process RSS never sees it. A flat RSS here means
                your measurement cannot see the cost, NOT that the cost is absent.
unified memory (macOS): accounted to the process; the figures above.
software raster (llvmpipe / Xvfb / most headless CI): the framebuffer is ordinary system
                RAM and shows in RSS in full. MEASURED: a blank 800x600 window,
                4x = 117.8 MiB vs 1x = 87.3 MiB peak RSS, -30.45 MiB, 3 reps, spread <=24 kB.
```

  ⇒ If you gate footprint in CI you will *see* this dial move; if you profile on a workstation GPU
    you may not. Say which you measured on.

Unlike the vertex pool below it trades *quality*, not correctness: at 1x nothing is dropped,
curves and diagonals are simply harder-edged.
**How to decide whether you want it:** MSAA does nothing for axis-aligned rectangles or for text
  (text is sampled from an already-antialiased atlas). That is most of a typical UI. It earns
  its cost on *rounded corners*, chart lines, scatter and pie. Two measured end points:

```
a window that draws no geometry  -> 4x vs 1x is ZERO differing pixels. You pay the full
                                    buffer for nothing; turn it down.
a line chart + rounded cards     -> a real difference (see the measuring note below).
```

**Measuring the visual side:** on an *animating* app a pixel diff cannot answer the question; the
  frame-to-frame churn swamps it (control, same config twice: 37,655 differing px; treatment
  4x vs 1x: 44,537, an 18% separation, i.e. noise). **Count distinct colours** instead: antialiasing
  works by adding intermediate blend colours, and that quantity barely moves as the animation
  advances (same control: spread of 15 colours; 4x vs 1x: a gap of 391, 26x the control).
**The count you ask for is not the count you necessarily get.** GL/EGL read a sample count as a
  *minimum*, so a request of 1 is commonly satisfied with 2 or 4 and the memory you meant to save is
  still spent. RayClay reads the count back off the default framebuffer and **warns once** when
  it disagrees with the request, so *silence* is the confirmation, and a footprint A/B only means
  something if *neither* arm warned. Check the log (`rcSetLogSink`) before trusting either number.
A build being md5-distinct proves the *define* took, never that the driver *agreed*.

**All of that is the desktop story. Web asks a weaker question and gets a weaker answer.**
  On web the request reaches WebGL as the *boolean* `antialias` context attribute, which the spec
  lets a GPU ignore, so there is no negotiated count to compare
  against, and the desktop equality test above is not compiled into a wasm build at all.
  RayClay asks the one question that *is* answerable there ("did we get any?") and warns
  once if the browser refused outright.

```
desktop  "requested 4x MSAA, the framebuffer reports 1x"   <- a COUNT disagreed
web      "this browser refused the antialias request"      <- there was NO antialiasing at all
```

  ⇒ **On web, silence means "some antialiasing happened", not "you got four samples."** A browser that
    hands you 2 where you asked for 4 is silent, correctly: the count on web is the browser's choice
    and RayClay reporting it as a disagreement would fire for your users and stay quiet for you.
  **What to do with that:** on web, treat the sample count as a hint you cannot hold the platform to.
    Do not build a design whose readability depends on 4x edges; the fallback is single-sampled,
    which is legible but harder-edged on rounded corners, chart lines and icon strokes. Text is
    unaffected on every platform (it is sampled from an already-antialiased atlas).
  **How often a browser actually refuses is unmeasured.** The warning is proven to fire and proven to
    stay quiet, but the population that would trigger it in the field (low-end and mobile GPUs,
    software rasterisers, drivers that drop MSAA on a large canvas) has not been surveyed.
  The two wordings are exclusive per target: a Linux Release `librayclay.a` carries only the count
    sentence, a wasm `librayclay.a` only the refusal sentence, and each is absent from the other.

## RC_SIZE_OPT

(CMake build option, not a header define; it configures the build, so it is invisible if you vendor
rayclay.h and compile it yourself) aggressive size posture: LTO + section GC + static stb_truetype;
the inspector is not part of what this option buys you, because `RC_DEBUG_TOOLS` is already 0 for
every build. `RC_SIZE_OPT` deliberately does not re-assert it, which is what leaves
`-DRC_SIZE_OPT=ON` together with `-DRC_DEBUG_TOOLS=1` a legitimate configuration rather than a
contradiction.

**The biggest size lever is not a RayClay knob at all: it is your own compiler flags.**
  -ffunction-sections -fdata-sections   (compile)      MSVC: /Gy /Gw
  -Wl,--gc-sections                     (link)         MSVC: /OPT:REF
  Measured on a minimal single-header consumer app, linux/gcc -O2 -DNDEBUG, stripped:
    ordinary flags            923,968 B
    + the three flags above   669,784 B     = 254,184 B saved (~27%)
    + -DRC_DEBUG_TOOLS=0      620,472 B     = 303,496 B saved (~33%), zero code changed
  **Read the third row as your starting point, not as a saving to claim:** `RC_DEBUG_TOOLS` defaults
    to 0, so you are handed it. The gc-sections row is the one still yours to pull, and it is the
    biggest single lever.
  Reproduce the *saving*, not the absolute: your total depends on what else you link. Two independent
    trees measured the gc-sections saving 32 bytes apart (254,184 vs 254,216) and the `RC_DEBUG_TOOLS` delta
    at exactly 49,312 B on both, so the deltas are the trustworthy half, and the deltas *survive* the
    default flip, because a delta does not care which side of it you start on.
  **Why the single-header shape creates this**, and why it is easy to lose by accident: the amalgam is *one*
    translation unit compiled with *your* flags. Verified on the shipped rayclay.h: without those flags the
    whole implementation lands in 3 .text sections; with them, 1,136. The linker can only discard whole
    sections, so at 3 it cannot drop a single function you never call.
  RayClay's own CMake sets all three, so if you build RayClay through it, you already have them.
    A consumer who just drops rayclay.h into their own build gets *none* of it, which
    is exactly the case that never sees RayClay's build system.

## RC_ZoomOptions

Zero-init = zoom on, 25%–500%, the Chrome ladder on the keys, a 10% continuous wheel, layout
reflow, and pan off.

**The keys walk a ladder of stops, not a fixed multiplier.** The bundled table is Chrome's:

```
25 33 50 67 75 80 90 100 110 125 150 175 200 250 300 400 500  (%)
```

so Ctrl+'+' from 100% gives 110, 125, 150, 175, 200 (round numbers a user recognises).

.ladder + .ladderCount supply your *own* stops (ascending, positive, finite, >= 2 entries):

```c
.zoom = { .ladder = (float[]){ 0.5f, 1.0f, 2.0f, 4.0f }, .ladderCount = 4 }
```

**Not copied**: it must outlive the app; a static or literal array is the intended shape. An invalid
table warns once and falls back to the bundled ladder rather than zooming unpredictably.
minZoom/maxZoom always win: stops outside the range are unreachable.

.step **is the opt-out**, not "the keyboard step". Set it
(e.g. 1.05f for a drawing tool) and the keyboard goes *continuous* with the ladder off for that app.
Unset or <= 1 means use the ladder. **.wheelStep falls back to .step when you leave it 0**, so
setting .step also changes the wheel; give .wheelStep a value of its own to keep the two apart. The
wheel is always continuous either way: it never consults the ladder. The two compose: a ladder keypress from a wheel-zoomed 137% goes to
the next stop beyond it.

.pan = true turns on drag-to-pan: hold .bindPan (`RC_KEY_NONE` = Space) and drag with the left button
to move a magnified view, Figma-style. **Optical only**: `RC_ZOOM_LAYOUT` reflows into the window, so
there is nothing outside it to reach and the flag does nothing there.
**Off by default on purpose:** a browser has no pan, and out of the box a RayClay app is a browser.
Turn it on for canvas-shaped apps (maps, diagrams, image work), not for ordinary UI.
Space is a *content* key. Panning is suppressed while an `rcTextInput` holds focus, so a space typed
into a field can never drag the view. Rebind if your app holds Space for something else.

**Four independent per-gesture kills**, so you can drop one route and keep the rest:
.inDisabled · .outDisabled · .resetDisabled · .wheelDisabled; e.g. wheelDisabled alone leaves
Ctrl+wheel to your handler while the keyboard steps still zoom.

bindZoom* and `RC_AppOptions.debugToggleKey` take `RC_Key` values (.bindZoomIn = `RC_KEY_I`,
.debugToggleKey = `RC_KEY_F12`). For bindZoom*, `RC_KEY_NONE` (0) means the built-in default (=/-/0)
and the *keypad twin* is always also accepted. For debugToggleKey, `RC_KEY_NONE` (0) means the overlay
toggle is *disabled*: there is no default debug key.
**The overlay is compiled out by default** (`RC_DEBUG_TOOLS`=0), so binding a key is not
enough on its own: build with -DRC_DEBUG_TOOLS=1 or the key warns once and does nothing.
`RC_Key` is an enum (an int in C), so a raw backend keycode like the host's own F12 constant compiles and
silently binds the *wrong* key. Always name the RC_KEY_* constant.

### RAYCLAY_ZOOM: start at a zoom without rebuilding

`RAYCLAY_ZOOM=<float>` replaces the starting zoom for one run, so a single binary can be sampled at
several zooms. It is read *before* the first layout, so frame 1 is already at the requested zoom: a
zoom that landed on frame 2 would make every first-frame reading a lie.

```
RAYCLAY_ZOOM=1.5 ./myapp      # opens at 150%
```

Precedence and behaviour match RAYCLAY_RENDER_MODE: the environment overrides the app's seed, but
never overrides a *refusal*: an app with .disabled set is not forced into zoom.
**Clamped to .minZoom/.maxZoom and it says so**, naming the requested value and the applied one.
A value that does not parse cleanly, or is not positive, is *ignored* rather than guessed at, with one
warning, so a typo turns into a sentence in the log instead of a degenerate layout that reads as a
RayClay bug.

## RC_TitlebarOptions

Zero-init = the default bar, controls right, a chrome-coloured band, and *fixed* chrome that never
zooms. .title is copied at `rcAppCreate`, so a temporary buffer is fine. .background with 0 alpha
means the theme's chrome colour, and `.titleColor` with 0 alpha means the theme's text colour.
`.hideTitle` drops the title text, `.hideMinimize` and `.hideMaximize` trim those two buttons (close is
always shown), and `.iconSize` is the control glyph size in px, 0 for the 16 px default.

**`.chipWidth` sizes the control slabs; 0 is the bundled 46 px.** The slab HEIGHT always follows the
band, because a slab that is not the band's height either leaves a dead strip or overdraws its border -
so this is a width knob, not a size knob. A smaller slab is a smaller hit target: 46x38 is ~1748 px^2
and 28x28 is 784, which the library does not clamp, because shrinking the controls is exactly the
presentation decision a custom titlebar exists to make.
**`.height` at or below 0 means UNSET, not folded.** The band falls back to
`RC_AppOptions.titlebarHeight`, and to the 38 px default only if that is unset too, so zero-initialising
this struct gives you the normal bar with all three controls, not a chromeless window. A negative value
behaves exactly like 0; the test is `> 0`.
Folding at *runtime* is a different mechanism with a different contract: `rcWindowSetTitlebarHeight(rcAppMainWindow(app), 0)`
does fold the bundled band, and because that band is what draws minimize, maximize and close, under
`.nativeFrame` (where there is no OS chrome to fall back on), it leaves **no close button and no drag
region**, only the resize edges. That is a legitimate way to build a chromeless window and a nasty
surprise if you only meant to shrink the bar. **Fold to a small positive height, or draw your own
`RC_ID_WINDOW_*` controls.**
Only the *bundled* bar behaves this way. With `.titlebar.custom` the band is your own layout, so
`rcWindowSetTitlebarHeight(rcAppMainWindow(app), 0)` moves the OS strip and nothing of yours disappears, which is why
`ex20` can fold to a rail safely.
And a *positive* height that is very small is a separate trap on Windows: the resize border outranks
the caption, so a strip thinner than `SM_CYSIZEFRAME + SM_CXPADDEDBORDER` keeps no draggable pixel.
See `rcWindowTitlebarHeight / rcWindowSetTitlebarHeight` above.

**Per-button glyphs.** `.minimize`, `.maximize` and `.close` each take an `RC_TitlebarButtonIcons`:
`normal`, `hover` and `press`, all of them `RC_IconCallback`: `void (*)(float size, RC_Color color)`.
Three rules, and they compose: a NULL state falls back to `normal`; an **all-NULL set keeps the
bundled Flat Slab glyph**, so you override only the buttons you mean to; and the bar chooses the
colour it hands you (the style's text colour, lifted toward white as the hover fill strengthens), so
**a glyph must draw with the `color` argument and never pick its own**, or it goes invisible against
the filled slab.

That signature is exactly what a generated icon header exposes, so an icon you converted drops onto a
window control with no adapter. `examples/ex11_rayclay_icon_converter` does it to its own window:
minimize sets `normal` + `hover` (a chevron that gains a rail under the pointer), maximize sets
`normal` alone (so its hover and press reuse that one glyph), and **close is deliberately left
bundled.** Minimize and maximize are safe to restyle; the control a user reaches for when an app
misbehaves is the one to leave conventional.

.allowControlsClip deliberately accepts a window that can be sized smaller than its own controls.
While the library is drawing those controls it floors `RC_AppOptions.minWidth` so they cannot be
clipped, and warns once if it had to raise your value. It does not apply to `.custom`, which draws no
controls of its own. See `RC_AppOptions` in depth ▸ minWidth / minHeight.

## RC_TextOptions

.wrap chooses how a run breaks: `""` words (the default) · `"n"` none · `"l"` newlines-only.
**It governs automatic wrapping only.** An explicit `\n` inside your string starts a new line under
**all three values**, `"n"` included. `"n"` and `"l"` both switch off the width-driven breaking that
`""` does; neither of them suppresses a newline you typed.
`.wrap` and `.textAlign` are short-code fields, so spelling one out (`.wrap = "no"`) warns once and
  still applies the first letter. The rule and its diagnostic cover all six short-code fields:
  [.align: the Y-then-X code](#align-the-yx-code)

**If you arrive from CSS, that is the opposite of what you expect.** `white-space: nowrap` collapses
  a newline into a space, so a string you meant to draw on one line comes out N lines tall, a *third*
  of the width the same text takes on one line, and it pushes whatever sits below it down the column.
  Nothing warns.

Measured on Linux, Release, with one text element in an 800x600 window. The top group sizes
a `fit` box, so only a newline can break it; the bottom group forces a 60 px box that the text
cannot fit, so only *width* can break it:

```
--- a FIT box: nothing forces a width break, so any break here is the NEWLINE ---
"alphabravocharlie"      .wrap="n"    box 108x16    1 draw command    <- control: one line
"alpha\nbravo\ncharlie"  .wrap=""     box  41x48    3 draw commands
"alpha\nbravo\ncharlie"  .wrap="l"    box  41x48    3 draw commands
"alpha\nbravo\ncharlie"  .wrap="n"    box  41x48    3 draw commands   <- "n" did NOT stop it

--- a 60px box, spaces not newlines: so any break here is the WIDTH ---
"alpha bravo charlie"    .wrap=""     box  60x48    3 draw commands   <- control: it wraps
"alpha bravo charlie"    .wrap="l"    box  60x16    1 draw command    <- "l" DID stop it
"alpha bravo charlie"    .wrap="n"    box  60x16    1 draw command    <- "n" DID stop it
```

  ⇒ Both directions are measured, and they are the whole contract: `"n"` suppresses the break the
  *layout* would invent, and never the break *you* typed.
  Each row reads two values, and they agree: `rcGetElementBox` for the box the layout gave it, and
  `rcWindowFrameCounts().drawCommands` for the lines that actually reached the renderer. (They *can*
  disagree: a line past the viewport bottom keeps its box and emits no command, so a run where they
  diverge means the window was too small, not that wrapping behaved differently.)
  Note what the bottom group costs you: at `"n"` the *box* stays 60 px while the run needs 108, so
  the text is wider than the element holding it. What that looks like is then the ancestors'
  `.overflow`: visible by default, cut inside a `"hidden"`/`"scroll"` container, and gone entirely
  past the layout cull edge, which is the button-label case below. Budget the width or clip on
  purpose; do not leave it to chance.

The rule this leaves you with is simple: **if you want one line, put one line in the string.**
  `.wrap` cannot rescue a string that already contains a break. Where the text comes from your data
  rather than a literal, strip or replace the newlines before you draw it.
`"n"` on a button label can push it past the layout cull edge, where it *vanishes* rather than
  overflowing. Keep labels short.
**For text whose line structure you typed** (a block of code, a log line), reach for `"l"`. A break
  invented by the layout would be a lie about that text, while a break you typed is the content, and
  `"l"` (newlines) is the value that says exactly that. The table above shows `"n"` measuring the
  same here; `"l"` is the one whose *name* matches the intent, so it is the one to write down.

.lineHeight sets the *line box* in px. It is CSS `line-height`, **not** CSS `margin`: it *replaces* the
line's default height instead of adding to it, and the glyph run is centred in the box that results.
0 does **not** mean a typographic metric: the default is the text's *pixel size* (`.size`, or the font
slot's loaded size when `.size` is 0). 16 px text with `.lineHeight` unset gives a 16 px line box, not
the ~19 px a browser's `normal` would give you. Set `.lineHeight` explicitly for body copy.

Measured with 20 px text, so the default line box is 20:

|lines|`.lineHeight`|total height|
|---|---|---|
|1|0|20|
|1|40|**40**|
|3|0|60|
|3|40|120|

**The single-line row shows this directly.** One line has nothing to be spaced *from*, so a field that
*added* space would have to measure 20 there; it measures 40. And three lines at 40 give 3 x 40 = 120, not
60 + 2 x 40.
**A value below the natural height tightens the lines** and will eventually crowd them, because it
replaces rather than pads. Leading is `.lineHeight` minus the pixel size, and it may be negative.
⇒ Pick it as you would pick CSS `line-height`: the px line box you want, comfortably above the font
size for body copy. The examples run 16 px text at 25-27. Because the extra space is split above and
below, that centring is what you align against when a text block sits beside an icon or a control.

.letterSpacing adds N px after each character, including the last. For n glyphs the element **box**
grows by n x spacing and the **ink** by (n-1) x spacing, so the box carries one trailing gap the ink
does not fill. That is not a rounding error and it is not ours to fix: it is what CSS does, and it is
why centred and right-aligned text lands where a browser puts it rather than half a gap away.
0 means the font's natural advance.

.textAlign ("l"/"c"/"r") aligns lines *within* the block width, and is effective only for **multi-line**
(wrapped) text. A single line shrink-wraps to its own width, so align is a no-op there; centre one
line via the parent's .align.

.textAlign aligns **lines inside** the text block; the *container*'s .align is a two-letter "<Y><X>"
code positioning **children**. Different axis, different field. Do not reach for one meaning the other.

## RC_Float

Floating placement: .to (the attach target, `RC_AttachTo`), .toId (the target element's id, required
when .to = `RC_ATTACH_ELEMENT`), parent/element anchors, offset, zIndex, capture, and .clip
(`RC_FloatClip`: `RC_CLIP_NONE`, the default, escapes the target's clipping; `RC_CLIP_TO_PARENT`
inherits it, and only under `RC_ATTACH_PARENT` or `RC_ATTACH_ELEMENT`).

**Declare your floating elements in ascending `zIndex` where you can.** Each floating element is its
own tree root, and the roots are ordered by a stable insertion sort before anything is emitted. That
sort is linear on input already in ascending order or all-equal, which is the ordinary case and costs
nothing. Declared in *descending* z it is quadratic, and the two curves cross at about **128 roots**:
below roughly 64 overlays the ordering is free, and by 1,024 reverse-declared roots the sort costs
about ten times what the ordered arm does. So it is declaration *order* that matters and not overlay
count alone, and past ~128 overlays expect the sort to cost about as much as the rest of the frame.
Almost no application reaches that; a host generating overlays from data, in whatever order the data
arrives, is the one that does.

**A floating element does not inherit its target's clipping by default, and `.clip` is the switch.**
A dropdown or tooltip opened inside a scroll panel draws *over* whatever lies outside that panel,
and keeps drawing there as the panel scrolls. That is the default because escaping the container is
the whole point of a menu, and every floating widget here relies on it. When the float is
*decoration* of its target rather than an escape from it - a badge on a card, an axis label, a
caption pinned to a panel - set `.floating.clip = RC_CLIP_TO_PARENT` and it is cut by the same
rectangle that cuts the target, scrolling out of view with it instead of painting over the titlebar.
`RC_CLIP_NONE` is the default and the escaping behaviour. One precondition: only `RC_ATTACH_PARENT`
and `RC_ATTACH_ELEMENT` resolve a rectangle to inherit, so under `RC_ATTACH_ROOT` the field does
nothing and is not a defect when it appears to be ignored: the root never clips.

**When `.toId` names an element not declared this frame, the anchor becomes a zero-size box at the
window origin**, never its last position, and not for one frame only: it stays that way for as long
as the target is missing. Dismissing the row a tooltip or dropdown is pinned to is exactly this shape.

**"The origin" is not where it lands.** Your `.element` anchor still subtracts the float's own
extent and `.offset` is still added, so the result is usually *negative*: off-screen, culled, drawing
nothing. Worked example, the bundled `rcScrollbar`: 8 px wide, `.parent` and `.element` both
`RC_ANCHOR_TOP_RIGHT`, `.offset = {-3, +3}` lands at **(-11, +3)**: x = 0 − 8 − 3, y = 0 + 3, because
TOP_RIGHT subtracts the width in x and nothing in y. **The symptom is a disappearance**, not a widget
stranded in the corner. (The bar itself self-heals: once the scroll container is gone
`rcGetScrollInfo` stops finding it too, so the next frame declares no bar at all.)

**The warning this logs names a field you do not have.** It reports a `.parentId` (the layout
engine's name for what you spell `.toId`) and says the element "is laid out at the origin and is not
clipped", which is the imprecision above. It is logged **once per app**, so later recurrences are
silent: do not read one line as one frame. Declaring the anchor *after* the float in the same frame is
fine and is *not* this error; the message says so itself.

Widgets that declare the anchor and the float in one call (`rcBeginMenu`) cannot reach this. Only a
`.toId` naming an element declared elsewhere in your tree can.

## Text selection: `.select` and `RC_SelectMode`

**Selection is ON by default.** `.select` is zero (`RC_SELECT_AUTO`) on every call you have already
written, so every label, heading and table cell can be dragged across and copied without asking for
it. That is the browser's posture, deliberately: a design system opts CHROME out rather than opting
content in, and the library already does that for you - buttons, menu items, combo values, tabs and
the titlebar title are `RC_SELECT_NONE` from inside the library, so you never spell it for them.

| you write | on a text run | on a button label |
|---|---|---|
| nothing | selectable | **not** selectable - chrome |
| `.select = true` | selectable | selectable |
| `.select = false` | selectable (the default) | not selectable (the default) |
| `.select = rcSelectable(true)` | selectable | selectable |
| `.select = rcSelectable(false)` | not selectable | not selectable |
| `.select = RC_SELECT_TEXT` | selectable | selectable |
| `.select = RC_SELECT_NONE` | not selectable | not selectable |

**A bare `true` does what it reads as**, because the ordering is a contract: `AUTO = 0`,
`TEXT = 1`, `NONE = 2`, so `.select = true` converts to `RC_SELECT_TEXT`.

**A bare `false` means "no opinion", not "off".** `false` is `0` is `RC_SELECT_AUTO` is the field's
unset state - C cannot distinguish an explicit `false` from a field you never named - and `AUTO`
resolves to each widget's own default, which is why the `false` row above differs between the two
columns. **To force a text run off, name it:** `RC_SELECT_NONE` or `rcSelectable(false)`.

`.select` lives on `RC_TextOptions` and on `RC_ButtonOptions`, and it needs no `.id`: it rides on
the text run itself, which is why the library's own unnamed labels can carry it.

**ON A BUTTON, `AUTO` AND `TEXT` ARE NOT THE SAME, AND THAT IS THE ONE PLACE THEY DIFFER.** A button
label is chrome, so `rcButton` maps an unset `.select` (`AUTO`) to `RC_SELECT_NONE` before it draws
the label - a deliberate default, and what every browser UA sheet does. To let a button's label be
selected - a "Copy" button whose label is the value, say - opt in with `.select = true`, and
`rcSelectable(true)` and `RC_SELECT_TEXT` are the same thing spelled longer. **On an ordinary text
run there is no such mapping and `AUTO` and `TEXT` resolve the same**, because only
`RC_SELECT_NONE` is consulted.

**Copy is the primary modifier plus C** - Cmd+C on macOS, Ctrl+C elsewhere - and it goes through
`rcClipboardSet`, so a backend you installed with `rcSetClipboardImpl` receives it and a web build
reaches `navigator.clipboard`.

**DOUBLE-CLICK SELECTS A WORD**, as a browser does, with the same 400 ms / 4 px gesture the text
fields use - one number, shared, so the two cannot drift. A double click on whitespace selects the
run of whitespace rather than nothing, and a caret just past the end of a word belongs to that word
rather than the gap after it.

**A TRIPLE CLICK SELECTS THE WHOLE ELEMENT** - every wrapped line of the text element under the
pointer, and nothing in the element next to it. The subject is deliberately the ELEMENT and not the
visual row: a row changes when the window is resized, so the same gesture would select different
text at different sizes, while an element's text does not change at all. Copy then yields the
element's text including the space a wrap consumed, which is in neither line's slice. A **fourth**
click starts a fresh single click, exactly as it does in a text field.

**HOLDING THE BUTTON AFTER A DOUBLE OR TRIPLE CLICK KEEPS WHAT THE GESTURE SELECTED**, and dragging
on from one does not extend it - the same choice `rcTextInput` makes. A drag that has to cross
components therefore starts from a single click.

**SELECTION IS PER WINDOW.** Each window owns its own drag, so a second window (`rcAppOpenWindow`, a
popped-out panel) can neither extend nor cancel a selection made in the first, and a highlight band
never appears in the window that does not own it. Copy - both the chord and `rcCopySelection` - is
serviced by the owning window, so a chord struck in a window holding no selection copies nothing,
which is what a browser does. The rule to hold on to: **a selection belongs to exactly one window,
and no other window can read, extend, cancel or paint it.**

**`rcSelectAll()` acts on the window whose UI callback you are inside.** It resolves through the
bound input context, which the app runner switches around each window's callback along with that
window's interact, widget and Clay state - so a select-all button on a popped-out panel selects that
panel's text, not a sibling's. Called from outside any window callback it acts on whichever window
was bound last.

**A SELECTION CROSSES COMPONENTS, and Ctrl+A selects the page.** A drag that leaves the element it
started in keeps going: every run between the anchor and the pointer is selected - the first from
the anchor to its end, the last from its start to the pointer, and everything between it whole -
exactly as a drag across several `<div>`s behaves in a browser. Wrapped lines inside a run are
covered too. **Ctrl+A** (Cmd+A on macOS) selects every selectable run in the
window, and `rcSelectAll()` is the same verb for a device with no keyboard. Inside a focused text
field the chord keeps its field-local meaning, as it does in a browser.

**WHAT THE COPY GIVES YOU ACROSS SEVERAL RUNS.** Runs are joined in walk order. Two runs on the same
visual line - a row of labels - join with a **space**; runs on different lines join with a
**newline**. The separator is decided from the laid-out boxes, not from the widget tree, so it
matches what you see. Whitespace a *wrap* consumed inside one run is preserved, because each run is
copied as one contiguous range rather than line by line.

`RC_SELECT_NONE` runs - buttons, menus, tabs, the titlebar - are chrome and are skipped by a drag
and by select-all alike, so crossing a row of buttons does not drag their labels into your
clipboard.

### FOUR LIMITS, and they are the difference between this and a browser

1. **There is NO SCOPE, so `RC_SELECT_TEXT` overrides nothing.** CSS lets `user-select: text` inside
   a `user-select: none` subtree re-enable selection for that subtree. RayClay has no
   container-level `.select` and one behavioural reading of the mode - "is it `RC_SELECT_NONE`" - so
   `TEXT` and `AUTO` behave identically. Spell `TEXT` where it documents intent; do not build on it
   re-enabling anything. **This is also why `rcSelectAll()` takes no scope**: the window is the only
   unit the library can name.
2. **Copy stops at 4,095 bytes**, on a codepoint boundary, so a very long selection is truncated
   rather than mangled - and the library says so, warning once with the byte counts when it happens.
   **Select-all over a long document reaches it routinely**, which is why the warning matters.
3. **A selection is renumbered if the SET of selectable runs changes mid-drag.** Run identity is
   positional - it counts selectable runs in walk order - so a scene that adds or removes one while
   the button is held renumbers every run after it, and the selection appears to jump or die. A
   stable scene is the common case; this is the price of not needing a per-element handle Clay does
   not expose.

The first and third are design limits with no diagnostic to give, which is why they are written
down here; the second reports itself. One asymmetry is worth knowing: a press will not start a
selection on text a clip container has hidden, but **select-all deliberately sweeps clipped runs
too**, so a Ctrl+A inside a long scrolled list copies content the user cannot see. That is what a
browser does with `overflow:hidden`.

### What a selection LOOKS like, and the one way to make it invisible

**Selected text gets a highlight band by default - you do not opt in.** It is the theme's
`primary` at alpha 160, square-cornered, painted immediately *under* the glyphs so the text keeps
its own colour rather than inverting the way a browser does.

**THE ONE WAY TO MAKE IT INVISIBLE: set a pale `RC_Style.primary`.** The band inherits your accent,
which is deliberate - it is what a design system wants and what CSS `::selection` allows - but a
near-white accent on a white surface produces a selection nobody can see. Nothing warns you,
because a pale accent is a legitimate choice everywhere else it is used.

**Text on the band stays above WCAG AA** - 6.07:1 on light and 9.32:1 on dark - which matters
because the band tints *under* the glyphs rather than inverting them.

## Copying a selection: `rcCopySelection` and `rcHasSelection`

```c
RC_API bool rcCopySelection(void);   /* copy the live selection; false if nothing is selected */
RC_API bool rcHasSelection(void);    /* live, or being dismissed by a press still down          */
```

**These exist because of mobile, and without them selection is a feature the user can start and
cannot finish there.** The built-in accelerators are primary+C to copy and primary+A to select all -
Cmd on macOS, Ctrl elsewhere - and a touch device can produce neither. `rcSelectAll` and
`rcCopySelection` are those two chords as verbs, so an app can offer its own affordance.

RayClay ships the **verb and the arming gesture, but not the menu.** There is no built-in
long-press MENU, and that is a decision rather than an omission: the right affordance differs per
platform and per app - an iOS share sheet, an Android contextual action bar, a toolbar button, a
keyboard-first desktop app that wants none of them - and a built-in menu would be wrong somewhere.

**HOW A SELECTION BEGINS DEPENDS ON THE POINTER, AND ON A TOUCHSCREEN IT IS A LONG PRESS.** With a
fine pointer a press starts a selection and a drag extends it, which is the desktop convention. With
a coarse pointer the press only arms a candidate: a selection begins after the finger has been held
for about half a second **without travelling** further than the same small threshold the scroll
gesture uses. Move past it and the candidate is dead for that gesture and does not come back if the
finger returns, so **a scroll over text can never end in a selection**. Lift early and it was a tap,
which selects nothing. Once the long press has fired, dragging extends the selection as it does
anywhere else. The half second is a fixed RayClay value, near iOS's own default; Android's default
is shorter and is a user-tunable accessibility setting, so a selection can arm slightly sooner here
than in a native Android app.

**Static text and a focused `rcTextInput` do not share a gesture.** Both refuse a coarse drag, but
neither one's behaviour tells you the other's - see the field's own in [widgets.md](widgets.md).

**Both bind to an ordinary `rcButton`**, which is the pair every touch app needs:

```c
if (rcButton("selall", "Select all"))
    rcSelectAll();
```

Select-all creates a selection and takes effect on the next render walk.

**A COPY BUTTON IS THE PLAIN `rcButton` IDIOM, and it is the whole of the mobile copy path:**

```c
if (rcButton("copy", "Copy", RC_BTN_PRIMARY))
    rcCopySelection();
```

A press DISMISSES a selection rather than ending it: the highlight stops at once, but the span stays
readable until the pointer comes up - which is exactly when `rcButton` fires. So the release edge
names the selection the user was looking at.

**Gate the button's VARIANT on `rcHasSelection()`, never its DECLARATION.**
`if (rcHasSelection() && rcButton(...))` removes the control from the layout mid-gesture, so it
disappears under the user's finger.

**`rcCopySelection` schedules; it does not copy synchronously.** The selected bytes are only
addressable during the frame's render walk - they live in the text command's own slice and nothing
else in the library holds them - so the clipboard write lands later in the same frame. The return
value tells you whether there was a selection to copy, not whether the clipboard write succeeded;
for that, read it back with `rcClipboardRequest` / `rcClipboardPoll`.

It returns **false rather than latching** when nothing is selected. That matters: a request parked
with no selection would fire against whatever got selected next, copying text the user never asked
for. The latch is also dropped whenever the selection is cleared, for the same reason.

On desktop this is the same verb the chord fires, so there is one copy path rather than two - a fix
to one is a fix to both.

## RC_Style

Read by value with `rcGetStyle`(). Idiom: `RC_Style s = rcGetStyle();` then `.bg = s.surface`,
`.color = s.textMuted`.

Three of the four accent pairs take a _600 base and a _500 hover, one step lighter:
`primary`/`primaryHover` are INDIGO, `success`/`successHover` EMERALD, `warning`/`warningHover`
AMBER. So s.success is **not** `RC_EMERALD_500`; that constant is s.successHover. **`danger` /
`dangerHover` is the exception and changes hue rather than shade**: `danger` is `RC_ROSE_600` and
`dangerHover` is `RC_RED_600`, in both presets. The surfaces are `background`
(the window), `surface` (panels and cards), `surfaceAlt` (a nested or inset panel, one step off
`surface`) and `chrome` (the titlebar band).

**A mark drawn ON an accent has no name in `RC_Style`, and four library marks default to white:**
`rcButton`'s PRIMARY and DANGER labels, `rcCheckbox`'s tick and `rcToggle`'s knob. A theme whose
`primary` is light wants a dark mark instead. **`rcButton` can now be told:**
`rcButton("ok", "Save", RC_BTN_PRIMARY, .color = s.text)` - the label colour is one of the widget's
**two** options, the other being `.select`, and leaving `.color` unset keeps today's white. `rcCheckbox` and `rcToggle` still have no options
channel, so for those two draw your own box. Every other widget reads an accent against a SURFACE
and is unaffected.

**`s.success` and `s.warning` are yours alone - no built-in widget reads either**, and what an
application does with them is write coloured text. Check the pairing before you spend one. Both
presets ship the SAME four accents and flip only the neutrals, so a value picked to carry white on a
FILL is being asked to do the opposite job when you write it on a surface. Measured as TEXT
(WCAG 2.1; AA wants 4.5:1 at normal size, 3:1 at 18.66 px or 14 px bold):

| written on | `danger` | `success` | `warning` | `primary` |
|---|---|---|---|---|
| dark `background` | 4.30 | 5.35 | 6.33 | 3.21 |
| dark `surface` | 3.80 | 4.74 | 5.60 | 2.84 |
| dark `surfaceAlt` | 3.11 | 3.88 | 4.59 | 2.33 |
| light `background` | 4.29 | 3.44 | 2.91 | 5.74 |
| light `surface` | 4.70 | 3.77 | 3.19 | 6.29 |
| light `surfaceAlt` | 4.49 | 3.60 | 3.05 | 6.01 |

Nine of those twenty-four reach 4.5:1. As a FILL under white the same colours are doing the job
they were picked for - white on `s.danger` is 4.70:1 and on `s.primary` 6.29:1 - which is why the
gap is easy to miss. **If you write a severity as a SENTENCE, give the ink its own value per theme
and leave the preset's for the fill; one value cannot be both.** `s.danger` is the sharp case:
ROSE_600 as text on a dark panel is 3.80:1, ROSE_500 fixes that at 4.83:1 but then carries white at
only 3.67:1. `examples/ex12_rayclay_inspector` splits both `warning` and `danger` this way and
writes the numbers beside the code.

**`s.radius` is a setting; `s.padding` and `s.gap` are advisory tokens for *your* layout.**
Every library widget derives its corners from `s.radius`: the sm / md / xl tiers are **0.5x / 0.75x /
1.5x** of it and a pill is full, so one field restyles `rcButton`, `rcTextInput`, the chart frames and
the rest together. **Zero draws everything square, pills and radios included**, which makes a
zero-initialised `RC_Style` square - the CSS rule that no border-radius is no rounding. A negative or
non-finite value is read as 0 and warns once. Both presets set **8**, which is the 4 / 6 / 12 / pill
the widgets draw, so installing a preset changes nothing you can see.

```c
RC_Style s = rcGetStyle();
s.radius = 0.0f;  rcSetStyle(s);      // every widget is square from the next one you declare
s.radius = 14.0f; rcSetStyle(s);      // and softer everywhere, in one line
```

**`rcSetStyle` takes effect at the NEXT widget you declare, which can be later in the same frame** -
not at a frame boundary. Each widget compares the radius it last derived under against the live one,
so a call made part-way through a frame leaves everything already declared at the old radius and
draws the rest at the new one. Call it before you build a frame unless a half-and-half frame is what
you want.

`padding` and `gap` are the ones nothing in the library reads. Use them the way you would a CSS
variable:

```c
rcRow(.gap = s.gap, .p = s.padding)   // your own containers re-theme in one line
```

The *colours*, like the radius, **are** consumed by the built-in widgets.

## One-line behaviours worth knowing before you hit them

Each of these is a single observable behaviour of one call, verified against the implementation
rather than against its header comment. They live here rather than on the reference card because the
card gives each symbol one line, and each of these needs a sentence.

**Fonts and text.**

- `rcFont` returns 0 when no registered (family, weight, size-to-nearest-pixel) matches - and 0 is
  slot 0, so an unmatched lookup silently renders in the default face rather than failing. It warns
  only on the first miss in the process.
- `rcSetRootFontSize` refuses a non-positive or non-finite value and warns once instead of clamping
  it, so a bad number leaves the root at the size it already had (16 px unless a host changed it)
  rather than silently substituting one.
- A `RC_NO_BUNDLED_FONT` build has no default face: unless you load one (`RC_AppOptions.fontPath`
  **together with** `fontSizes` and `fontCount`, or a runtime `rcLoadFont`), no glyph resolves and
  the build draws no text. `fontPath` alone is ignored with its own warning and leaves you textless.
- `RC_FONT_LAST_CODEPOINT` is range-checked at build time rather than clamped: a value below 160 or
  above 0x10FFFF stops the build with a #error naming the rule (159 and 0x110000 fail; 160, the
  default 255, and 0x10FFFF compile), because the glyph table is direct-indexed from 32 and the
  Latin-1 supplement begins at 160, and 0x10FFFF is the last Unicode code point.

**The app and its frame.**

- `rcWindowFrameTime` and `rcAppFPS` both return 0.0f while no frame gap has been recorded - before
  the first drawn frame, or on a frame whose gap was rejected as below FLT_MIN or a second or more
  (a modal-resize stall). **Test for zero before computing a rate**: `1.0f / rcWindowFrameTime(app)`
  yields +inf.
- `rcBeginUnzoomed()` / `rcEndUnzoomed()` are the manual push and pop for a scope that opens in one
  helper and closes in another. Prefer `rcUnzoomed()`, which pops on every way out including
  `break`. A surplus `rcEndUnzoomed()` saturates at zero rather than going negative.
- `rcWindowDimensions` returns {0, 0} for NULL and for any window not in `RC_WINDOW_READY` - pending,
  failed or closing. Test for zero before dividing by it.
- `rcBeginComponent` applies the `.id` itself, because Clay stores element ids outside
  `RC_ElementDeclaration`, so a declaration you build with `rcParseComponentOptions` alone carries
  no id.

**Widgets.**

- `rcCheckbox` returns true only on the frame the click flipped *value, not on every frame the box
  is ticked, so read *value for the current state and the return value only for the change edge.
- `rcToggle` returns `true` only on the frame `*value` flipped, not while the switch is on, so
  read `*value` for the current state.
- `rcProgress` takes a fraction, not a percentage: the value is clamped to 0..1 with no warning, and
  the fill grows left to right across the track, so passing 75 silently draws a completely full
  bar while a negative value or NaN draws an empty one.
- Every `rcRadio` in one group shares a single `*selected` and takes a distinct `index` (and its
  own `id`): the option is on when `*selected == index`, and clicking it sets `*selected = index`
  and returns true on that frame.
- `rcCombo` copies nothing: the pointers in `items` go straight to the text elements, so every label
  string must stay valid until the frame is drawn (string literals and app-owned buffers are fine,
  but a label formatted into the frame arena or into a loop-scoped stack buffer is read after it
  dies), while the `items` array itself is only read during the `rcCombo` call.
- `rcBeginSplitPane` writes through *fraction in place, both as you drag the handle and as a clamp
  into [minFraction, maxFraction] (default 0.05 to 0.95) on every frame, and it keeps no internal
  copy, so the float must live in state that outlives the frame (a static or a field of your app
  struct) or the drag is lost the moment the frame ends.
- `rcBeginTable` caps colCount at 16 and silently drops every column past the sixteenth after
  logging one warning per process that names both the count you passed and the cap of 16; the
  limit is a fixed 16-slot per-table array with no build flag to raise it and no public constant
  to read, so clamp generated column arrays to 16 yourself.

**The clipboard.**

- A token from `rcClipboardRequest` is never 0 (the counter skips 0 on wrap, and a token is returned
  even when the installed RC_ClipboardImpl has no request callback), so store 0 to mean "no read
  in flight" - that is exactly how the bundled text field tracks its own paste.
- Pass NULL as `rcClipboardDeliver`'s text to answer a read with a denial or "no text": the read
  counts as answered and the bundled text field's Ctrl+V gives up at once instead of running out
  its 120-frame wait, but your own code cannot see the denial because `rcClipboardPoll` returns NULL
  alike for a pending read, a denied read and one already collected.
- An RC_ClipboardToken of 0 is the "no request" sentinel that `rcClipboardDeliver` and
  `rcClipboardPoll` always reject, but `rcClipboardRequest` never returns 0 - every call hands back a
  non-zero token (the counter skips 0 on wrap), even when the installed backend's request is NULL
  and the read will therefore never be answered, so testing the return value for 0 is dead code.

**Icons.**

- On a web (emscripten) build the bundled titlebar glyphs `rcIconTitlebarMinimize`,
  `rcIconTitlebarMaximize` and `rcIconTitlebarClose` are not declared at all, because
  `rayclay.h` declares them only under `#ifndef __EMSCRIPTEN__`, so naming one
  is a compile error ("use of undeclared identifier"), whereas `rcTitlebar`, `rcWindowControls` and
  `rcWindowControlButton` compile everywhere and simply emit nothing on web and mobile.
- `rcIconRayClayLogo` takes (size) alone because the logo artwork bakes its own 26 fixed colours and
  its drawer ignores any colour passed to it, so the logo can never be tinted and will not compile
  where an RC_IconCallback, void (*)(float size, RC_Color color), is required, which is the type
  `rcWindowControlButton` and every RC_TitlebarButtonIcons field expect.
- `rcIconEmit` only declares the element and stores your userData pointer as-is, so the draw
  callback does not run until `rcRender` at the end of the same frame: a buffer whose scope ends
  when your layout function returns is already dead by then, while frame-arena bytes from `rcFormat`
  (reset only at the top of the next frame) are still valid.

**Strings and input.**

- `rcStrCopy`(dst, src, dstsize) takes the FULL size of the destination buffer (sizeof dst), not the
  room left after a terminator: it copies at most dstsize - 1 bytes, always writes the NUL itself,
  and truncates a longer source rather than overflowing, so passing sizeof dst - 1 silently costs
  you one character and it is a no-op only when dst is NULL or dstsize is 0.
- `rcGetKeyboardInsets`() fills only the bottom field, because a phone keyboard rises from the
  bottom edge; top, right and left are always 0, and all four are 0 while no soft keyboard is up
  and on every desktop and web build.

**Build knobs.**

- Defining `RC_NO_COLOR_PALETTE` for your own translation unit also defines `RC_NO_STYLE`
  (`rayclay.h` defines it for you), because the built-in presets are composed from Tailwind
  constants, so your code loses the `RC_Style` type and the `rcStyleDark`, `rcStyleLight`,
  `rcGetStyle`, `rcSetStyle` and `rcGetStylePtr` declarations and must use its own colours, while
  the library's own widgets keep drawing from the dark preset because this knob trims only your
  view and must never reach the implementation.
- The default packet renderer (`RC_GFX_PACKET`=1) refuses the whole frame when it needs more than
  `RC_GFX_PACKET_MAX_VERTICES` (98304) vertices, leaving a cleared window and logging one error
  ending 'Raise `RC_GFX_PACKET_MAX_VERTICES`.', whereas the alternate sokol_gl renderer
  (`RC_GFX_PACKET`=0) keeps the frame and drops only the excess geometry, logging one warning that
  names the full pool and `RC_SGL_MAX_VERTICES` rather than dropping it silently.
- Leave `RC_GFX_PACKET_MAX_INDICES` derived: three indices per vertex is the worst ratio the packet
  renderer can produce and the annulus emitter reaches it exactly (a border ring or shadow feather
  writes 6*npts indices for 2*npts vertices, while a centroid fan only approaches it with 3*npts
  indices for npts+1 vertices), so 3x the vertex cap bounds every path and the knob to raise is
  `RC_GFX_PACKET_MAX_VERTICES` (98304 by default).

**Charts, tables and styling.**

- **`rcChart` and `rcSparkline` GROW to fill their parent and have no size of their own, while
  `rcBox` / `rcColumn` / `rcRow` all default to FIT.** A chart declared straight into a plain
  container therefore resolves to zero height and draws nothing, with no warning. Wrap it in a box
  that carries a real height.
- The `y`/`x` arrays and label strings you pass `rcChart` and `rcSparkline` are **borrowed**: keep
  them alive until `rcRender()` has run. Only the `RC_Series` descriptors themselves are copied, so
  a stack-local descriptor array is fine.
- Two different 16s: 16 series per chart (`RC_CHART_MAX_SERIES`, tunable down) and 16 charts per
  FRAME (not tunable). The frame pool counts `rcChart` and `rcSparkline` CALLS before any culling,
  so a chart scrolled out of view still consumes a slot.
- In a table the virtual-list pitch is (row height + 2 x `cellPadding`), not the row height.
- Past `RC_GRADIENT_MAX` / `RC_SHADOW_MAX` (both 64 distinct ids per frame, neither tunable) the
  surplus elements fall back to flat `.bg` or draw no shadow, and the warning fires once per
  PROCESS, not per frame - so a grid of 80 styled cards logs twice on frame 0 and then draws wrong
  in silence.
- `.borderRadius` is a char array (as are `.align`, `.scroll`, `.overflow`), so a scalar such as
  `.borderRadius = 6` COMPILES with only a `-Wmissing-braces` warning and is dropped at runtime.
  Write the string: `"-md"` or `"6px"`.
- `rcDefineClass` COPIES the name and BORROWS the utilities string - that pointer is read on every
  resolve until `rcResetClasses()` or a redefinition - so pass a literal, never a stack buffer or a
  frame arena.
- A zeroed `RC_ElementDeclaration` is a valid `rcComponent` base, but it lays children LEFT TO RIGHT
  where `rcBox` lays them top to bottom. `.className = "flex-col"` is the whole fix.

**Input, pointer and zoom.**

- A touch tap arrives as `RC_POINTER_LEFT`, so one `rcPointerPressed(RC_POINTER_LEFT)` path serves a
  click and a tap alike - there is no separate touch API to write.
- `rcPointer()` never reads (0,0) as "no pointer yet": a window opened under the cursor reports a
  real position on frame one, and an idle cursor outside reads a large negative sentinel. Gate on
  `rcIsHovered` or `RC_Box.found`, never on zero.
- `rcPointerIsCoarse` LATCHES: off native mobile it reads `false` until a real touch arrives, then
  stays `true` for the process. A `true` is reliable; a `false` only means "no finger seen yet".
- `rcPressed` is one fire per physical press and does not auto-repeat. A hold-to-repeat control - a
  scrollbar arrow that keeps stepping while held - is `rcPointerDown` + `rcIsHovered` + your timer.
- Read `rcViewport()` for the size Clay was actually given. `rcGetWindowDimensions()` divided by
  zoom reproduces only `RC_ZOOM_LAYOUT`, and is wrong under `RC_ZOOM_OPTICAL` where the layout size
  is `max(window, window / zoom)` because the surface is magnified rather than reflowed.
- Never multiply `rcGetWindowDimensions` by `rcGetContentScale` to get device pixels - the
  framebuffer size already IS the device-pixel count, so multiplying applies the factor twice. It
  looks correct on Win32 and X11, where the ratio is 1.0 at every DPI.
- `rcAppZoomLadder` returns NULL with `*count = 0` when the app set `.step` for continuous zoom.

**Frames, modals and the log.**

- RayClay redraws only when something happens - input, resize, focus, or your own request. If your
  app changes state RayClay cannot see (a timer, a socket, a worker result), call
  `rcWindowRequestFrame` or nothing repaints.
- `rcWindowRequestFrameAfter` holds ONE outstanding deadline and it is earliest-wins: arming 5 s
  while a 1 s wake is pending discards the 5 s request, so re-arm the later schedule each time you
  are woken.
- `RC_MODALITY_NON_MODAL` alone does not give you a panel you can work behind: with no scrim,
  "outside" is anywhere in the app, so the first click both does what the user wanted and closes the
  panel. A stay-open panel is that field plus `.noBackdropDismiss = true`.
- `rcSetLogSink`'s sink is called synchronously on the thread that logged, and REPLACES the default
  rather than adding to it.
- An empty `rcFormat` result with NO log warning means a NULL arena or NULL format - usually an
  arena fetched before `rcAppCreate`, or from a callback handed a different one. The unconfigured
  and arena-full cases both warn.
- `RC_APP_EVENT_CONTEXT_RESTORED` is declared but never emitted on any platform: after
  `CONTEXT_LOST`, rebuild your GPU resources immediately rather than waiting for a restore.
- A clipboard READ is mirrored into a fixed buffer and truncates past 4,095 bytes, warning with both
  sizes. Writes are uncapped, but the bundled `rcTextInput` / `rcTextArea` Ctrl+C copies at most
  1,024 bytes.

**Build knobs whose failure is quiet.**

- Past any of the three packet arenas (`RC_GFX_PACKET_MAX_VERTICES` / `_MAX_INDICES` / `_MAX_SPANS`)
  the frame is refused WHOLE - RayClay draws nothing rather than a clipped scene - so an unexplained
  blank window is the signature. The error names the arena and the knob to raise.
- `RC_SGL_MAX_VERTICES` is the alternate renderer only, and it is the one cap that fails quietly:
  past it the excess geometry is DISCARDED, it warns once per process and still exits 0, so lowering
  it buys memory with a permanently clipped UI.
- Raising `RC_FONT_LAST_CODEPOINT` alone changes nothing visible - the bundled face is a Latin-1
  subset, so supply a face with the coverage via `.fontPath` - and the glyph table is direct-indexed
  and duplicated across all 16 font slots, so each added codepoint costs ~456 B of `.bss` whether
  you load a face that uses it or not.

## RC_AppOptions in depth

frameEndCallback fires only for a frame that actually *drew*: under the default on-demand
  scheduling an idle frame does not call it, so "per frame" there means per *drawn* frame. It is the
  supported read point for `rcAppPerfFrame`(); see getting-started ▸ "Measuring what a frame costs"
It fires *more* often than maxFrames, so it is not a frame counter: live-resize repaints draw
  outside the main loop and call it too (measured, budget 90 -> 91 calls at rest, more while a
  window is dragged, none under -DRC_NO_LIVE_RESIZE). For "how many frames did this run render",
  read the runner's "rendered N of N budgeted frames" teardown line instead
### maxFrames / maxSeconds

Two bounded-run knobs that are **not** interchangeable. maxFrames forces *continuous* rendering;
maxSeconds is the **only** bound that leaves on-demand scheduling intact. See `RC_RenderMode`.

### iconBytes / iconLength / iconPath

Three `RC_AppOptions` fields. The icon a taskbar, dock or window list shows. Leave all three zeroed and you get RayClay's bundled
icon, so an app that configures nothing still looks like an app. Bytes (an encoded PNG/JPG/BMP you
embedded) beat iconPath, and are the robust form: an embedded icon cannot go missing when the
binary is moved. `iconLength` is how many bytes `iconBytes` points at - the encoded file's size, not
a pixel count - and both must be set together for either to be read.

**This is the one option whose effect is not the same on every target.** Five behaviours, not one:

```
Windows        the taskbar and title-bar icon
Linux/X11      the window icon
macOS          the DOCK icon: a macOS window has no icon of its own, so the dock is what a user sees
Linux/Wayland  NOTHING. There is no protocol to set an icon at runtime; the compositor matches your
               app_id to an INSTALLED .desktop file, so on Wayland the icon is a PACKAGING job.
               Setting the field anyway is harmless and stays right on every other desktop.
Web            NOTHING. The page owns the favicon; it is HTML, not something a canvas can claim.
```

⇒ Ship the .desktop file if Wayland matters to you. Nothing in this API can substitute for it.

Under `RC_NO_IMAGE` the icon is dropped on **Windows and X11 only**: macOS hands the bytes to the OS,
which decodes them itself, so that leg keeps working. A partial outcome, not an all-or-nothing one.

**Footprint:** the bundled default is 15,833 bytes of embedded image data, 78% of it the 256px size the
macOS dock wants at Retina. That is the image payload, **not** the whole cost: as hex text in
`rayclay.h` the same icon is ~103 KB of source you compile, and header text, download size,
compile time, binary size and RSS are not proxies for one another. -DRC_NO_DEFAULT_APP_ICON drops
it, which is worth doing if you always set your own, and for a Wayland-only or web build, where those bytes
can never do anything.
It does **not** drop the PNG decoder with it: .iconBytes/.iconPath still need one, so stb_image stays
linked. If your app never calls rcLoadImage*, `RC_NO_IMAGE` saves ~3.7x more; measured A/B and the
section-GC caveat that both numbers depend on are under "Build knobs" ▸ `RC_NO_DEFAULT_APP_ICON`.

### fontPath / fontSizes / fontCount

The font ladder, baked before the first frame. See getting-started ▸ "Fonts" for the
fontId-is-the-load-order-index rule and the fontPath-without-fontSizes footgun.

### fontOversample  (1-8)

*Replaces* the automatic policy on *both* axes, so the 36px oversampling cliff never fires: 2 roughly
halves text error; it costs ~2x the sheet per glyph at ordinary sizes (auto *already* oversamples 2x
horizontally, so you are only adding the vertical axis) but the full 4x out at the ceiling, where
auto has dropped oversampling entirely, so it *halves* your zoom ceiling. Measure your own font
ladder first. Too high is retried at a lower oversample; only a genuinely full atlas drops to the
default font.

### startLayoutElements / maxLayoutElements

The layout arena, counted in *elements*. start (0 => 2048, ~1.4 MiB) is the initial capacity; it
doubles on demand and *never* shrinks again, so max (0 => 65536) is the safety ceiling: at the cap the
runner drops that frame's overflow and logs one error instead of doubling until OOM. Lower start
(e.g. 512) for a memory-tight or embedded build; raise it to skip warm-up growth for a large UI.
Exceeding start costs a few warm-up frames of clean background, never a half-drawn frame.

**Size it for ~2x your per-frame element count if your ids churn**, and a scrolling `rcVirtualList`
churns by design. The layout engine retires a generation at the *end* of the following frame, so two
generations are briefly resident: a 24-row churning list occupies ~50 slots (root + anchor + 2x24),
not 25. Undersizing is not a bug; the arena just grows over a few background frames.

### titlebarHeight

The caption height for a nativeFrame window. **On Windows it is the drag region**; on macOS, X11 and
Wayland it sets the window's minimum height instead, and the drag follows the box you draw. It **must**
match the height you actually draw, or on Windows the region that drags is not the bar you see. The
bundled bar keeps the two in step for you; set this only when titlebar.custom draws a band at a
non-default height. 0 = 38, what `rcTitlebar` draws. **It is not ignored without `nativeFrame`**:
the height is resolved unconditionally and becomes the OS caption strip and the window's minimum
height, so a borderless app that sets it still moves its own drag region. Only the bundled band's
drawn height and the mismatch warning are gated on `nativeFrame`.

**It is frozen in physical px at window creation, so a custom band must not zoom.** Matching the
height is necessary and not sufficient: a band you draw yourself is ordinary content and grows with
the content zoom, so the two agree at 100% and nowhere else. Wrap it in `rcUnzoomed()`: measured
above, at 2× an unwrapped 46px band draws 92 physical px against a configured 46.
The element must also be tagged `RC_ID_WINDOW_DRAG`. RayClay moves the window from that element, so
without it RayClay contributes no drag at all, and **what you then see depends on the platform**: on
Windows, X11 and Wayland the window is undecorated and cannot be moved by its title bar, while macOS
keeps its own native title bar and still drags. One source, different behaviour per platform, which is
exactly why the debug build says so. If an app deliberately has no drag region, call
`rcWindowSetTitlebarHeight(rcAppMainWindow(app), 0)` to say so.

### minWidth / minHeight

The window cannot be resized below this, and a smaller requested size is clamped up at startup.
0 = **derived from the title bar**: a window may shrink to its own chrome and no further (the control
cluster's width by the caption band's height), and this applies *whether or not* nativeFrame is set.

It tracks what the band *actually* draws: 150x38 with the default 3 slabs, 58x38 with minimize+maximize
hidden, and the height follows titlebar.height (80 => x80). Those are *derived*, not constants: hide
a chip or set titlebar.height and the floor moves, so never hard-code them. Ignored on web.

Setting either field *replaces* the library default on **that axis** only: minWidth=900 with minHeight=0
still gets the derived height floor.

Your minWidth is itself *floored* when RayClay draws the window controls, nativeFrame with the
bundled bar: a value small enough to clip them off the band is raised, with one warning naming the
override. Opt out deliberately with titlebar.allowControlsClip. Two other modes honour a tiny
minWidth exactly as written and say nothing: an OS-decorated window, where the chrome is the OS's,
and titlebar.custom, where the band you draw owns its own controls. (In that floored mode the library warns
once per axis (twice for a too-small width *and* height) and names allowControlsClip as the way
out; the opt-out and an already-clearing value are both silent.
The floor it raises *to* is derived from the band, not a constant: 12 + 46*nbtn wide by the band
height, so 150x38 at the default three slabs. Do not hard-code it; change the band and it moves.)

### renderWhileMinimized

false (default) skips layout+render while the desktop window is minimized (iconified): the loop
parks on events at ~0 CPU (no swap = no vsync spin) and redraws the instant it is restored. Set true
for a game/sim/dashboard that must keep updating while hidden. No effect on web (hidden tabs are
browser-throttled).

## RC_GFX_TEXT_CACHE

Off by default. `1` caches text-run quads so a run that has not changed replays instead of re-shaping.
`RC_GFX_TEXT_CACHE_KB` sizes it (default 64, range 4–4096). The default holds one screen of text with
headroom and deliberately refuses to hold two; that is the sizing rule, and it is the one to reason
with. The quad *count* is derived from an internal struct's size and moves when that struct does, so it
is not a number to design against. Both knobs are `#error`-checked, so a typo stops the build rather
than compiling something else.

**It is not pixel-exact against the uncached renderer, and that is a property, not a bug.** The key
excludes position (which is exactly what makes a *moved* run a hit, and the whole prize), so a hit
replays `(abs − origin) + origin`. Measured over 15,680 coordinates (6 strings × 4 sizes × 7 origins):

```
(0,0) (20,60) (3840,2160) (-20000,-20000) (±1e6,±1e6)   ->  0 coordinates differ
(-512,-33)                                              ->  205 differ, worst 6.104e-05 logical px
```

The mechanism is arithmetic rather than statistical. Recording and replaying at the **same** origin is
exact at every origin measured, including a 4K deep scroll: the subtraction is exact whenever origin and
coordinate are within a factor of two (Sterbenz). The divergence needs the origin to have **moved**, which
is precisely the case the key exists to create. It is then bounded by **one float ULP of the origin's
magnitude** (`ULP(x) = 2^(floor(log2 x) − 23)` for float32), so it is **not a fixed figure: it doubles at
every power of two**, and it is constant *within* a binade:

```
|origin|      64      128      256      512     1024     2048     4096     8192    16384
bound   7.63e-06 1.53e-05 3.05e-05 6.10e-05 1.22e-04 2.44e-04 4.88e-04 9.77e-04 1.95e-03
```

Sub-pixel by a factor of ≥1000 at any realistic scroll depth, and that, not the number being small and
fixed, is what makes the divergence acceptable. Quote the bound with the origin it belongs to, or quote
the rule.

**The bound is tight, not conservative**: it is an attained maximum, not a safety margin. At 64 px,
128 px and 512 px the ratio of worst observed error to ULP is **1.000**: the worst case reaches a full
ULP rather than approaching it. Reading an observed error as if it were the bound is easy and wrong in
a specific way: **half an ULP at one origin has the same digits as a full ULP one binade below**
(`½·ULP(2048) == ULP(1024)` exactly), so a figure quoted without saying *which quantity it is* can be
read two ways that both look right.

**Spell the [512,1024) figure `6.104e-05`, never `6.1e-05`.** The attainable maximum there is exactly
`0x1p-14 = 6.103515625e-05`, and the rounded-down spelling sits 3.5e-08 *below* it: an assertion written
to the short form rejects correct output.

**No key can fix it.** Putting position back into the key destroys the hit rate; generating stored
quads at a zero pen makes hit and miss agree with each other while both still differ from the uncached
renderer.

**A pixel comparison that draws each scene once cannot see it.** The hit path runs only when a run is
drawn a second time, so a single-draw comparison exercises the *miss* path alone: passing with this knob
on says nothing about hits. The figures above come from measuring the record-and-replay round trip
directly rather than inferring it from a passing comparison.

⇒ **When to turn it on:** text-heavy scenes that redraw often. **When not to:** anything doing pixel
comparison against an uncached build, and anything where content routinely sits just off the top-left.

