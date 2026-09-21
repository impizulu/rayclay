# How to structure a RayClay app

RayClay ships as one header, and an app built on it wants about the same amount of structure: a
little, chosen once, that does not get in the way later. This page is the shape the examples use;
`examples/ex23_issue_inbox` is the copyable version of it, and every `exNN` from 20 up has the same
three directories.

```
myapp/
  CMakeLists.txt              about 15 lines
  src/
    main.c                    the ONLY .c file. Options, then run.
    app/
      app.h                   your state, and the layout that renders it
      theme.h                 colours, spacing, type scale
    platform/
      platform.h              the OS seam. It ships empty, and that is the point.
```

Four files. `examples/ex24_svg_live` is exactly this, under a thousand lines all in, of which
`main.c` is about fifty and `platform.h` is a comment. `examples/ex22_docs_reader` is the same
shape one size up, about twice the size, with one component and one pure-data file pulled out
beside `app.h`. (Those are file
lines, comments included, which is what `wc -l` over the example's `src/` reports.)

## The rule: one `.c`, many `.h`, no pairs

**`main.c` is the only translation unit.** Everything else is a header it includes, and every
function in those headers is `static inline`.

That is not C convention and it is deliberate. A `.h`/`.c` pair is core C practice, and it is the
single thing that most reliably confuses a developer arriving from JavaScript: you declare a
function in one file, define it in another, type the filename twice, and get a link error if you
forget. None of that teaches anything about your app. With one translation unit there are no
forward declarations, no `extern`, no link order, and no header that can drift from its
implementation, because there is no implementation file to drift from.

It is also what RayClay itself does. The library you are calling is one amalgamated header.

**Why `static inline` and not plain `static`:** an unused `static` function is a
`-Wunused-function` error under a `-Werror` build, and a header must not force everyone who
includes it to call everything in it. `static inline` has no such rule. That is the whole reason;
there is no performance claim here.

**Does the split cost anything?** No, and it is measured rather than assumed - an A/B of the same
example compiled both ways, same compiler, same flags, changing only how the source is divided into
headers. `ex22` came out **byte-identical** (36,680 bytes, `.text` 13,114, the same 50 defined
symbols); `ex24` differed by **4 bytes**, which traces to one colour table moving into `theme.h` and
so being declared earlier in the translation unit - same tokens, one byte of `.rodata` padding,
slightly shorter addressing. Both render identically. **The transferable result is the direction,
not the byte counts**: your own compiler will not produce those exact sizes, and it will produce the
same program either way, because one translation unit goes in and one comes out.

**When does a header genuinely have to become a pair?** Exactly one thing forces it: a name that
must be visible in a *second* translation unit. Not file size, not tidiness, not convention. So the
honest policy is to keep the number of translation units at one, rather than to fight the headers
that more of them would require. If you only ever have one `.c`, that day never comes.

The bundled examples follow exactly that line, and the one group that departs from it shows what
the rule is FOR. An `exNN` example is a single translation unit, so its helpers are plain statics in
headers and there is no second TU to keep definitions out of. The six benchmark apps under
`examples/bench/` carry a genuine `.h`/`.c` pair instead - because a benchmark harness links them as
objects, which is a real second translation unit and therefore the one condition above.

## What goes where

| file | holds | rule of thumb |
|---|---|---|
| `main.c` | `RC_AppOptions`, arena size, callbacks, `rcRunApp` | if you are editing this more than once a month, something is in the wrong place |
| `app/app.h` | your state struct, and the layout that draws it | this is your `App.jsx` |
| `app/theme.h` | values that are a *choice*: palette, spacing, type ramp | nothing here calls RayClay and nothing here holds state |
| `platform/platform.h` | things one OS does differently | should stay empty; see below |

**Colours in `theme.h` must be `#define`, never `static const`.** `rcRgb` and `rcRgba` expand to
compound literals, so a file-scope `static const RC_Color X = rcRgb(...)` is not a constant
expression in C99. A brace-initialised `RC_Color` (`{ 226, 232, 240, 255 }`) is fine, because that
is an ordinary aggregate initialiser.

## `platform/` ships empty, and that is the most useful thing in the tree

A portable app usually needs a layer for the things operating systems disagree about. In a RayClay
app there is almost nothing left in it, because the library has already absorbed the window, the
event loop, input, fonts, the clipboard, content scale, safe-area insets, the soft keyboard and IME
caret, process CPU and memory telemetry, a monotonic clock, and zoom.

Measured across this repository's five full demo apps (`ex20`-`ex24`):

```
OS-conditional directives   0     (_WIN32 / __APPLE__ / __ANDROID__ / __linux__ / __EMSCRIPTEN__)
system includes             0
```

Not "few". None. (Those apps do contain 24 `#ifndef` directives, so a bare `grep '#if'` will
find them - 21 include guards, one per header, plus three guards around a build-injected default path.
None of them branches on an operating system, which is the thing being counted here.) Across the entire example suite exactly one file needs an operating-system branch,
and it is the SVG converter, the one example intended for a desktop, which needs precisely two
operations: **list a directory**
and **create one**.

So the file exists to tell you what it is for, and what it is *not* for:

- **A mobile layout versus a desktop layout must never go here.** That is the one mistake worth
  naming twice. See the next section.
- **If `platform/` grows, that is a bug report for RayClay**, not a feature of your app. Every line
  in it is a portability difference the library failed to absorb.

The shape of a real entry, when you do need one, is one function and one `#if`, bodies inline. A
seam this small does not earn a translation unit.

## Branch on the space, never on the operating system

There is one `if` that every app on this structure has, and it is not `#ifdef`:

```c
RC_Viewport v = rcViewport();

if (v.breakpoint < RC_BP_MD) {
    one_pane(st, v);
} else {
    rail_list_detail(st, v);
}
```

`RC_Breakpoint` uses Tailwind's names and Tailwind's numbers (`RC_BP_SM` 640, `RC_BP_MD` 768,
`RC_BP_LG` 1024, `RC_BP_XL` 1280, `RC_BP_2XL` 1536), so a developer who knows `md:` already knows
this.

**`#ifdef __ANDROID__` in a layout is always a bug**, and the reason is measurable rather than
stylistic. An OS test gets three ordinary cases wrong: a desktop window dragged narrow is
phone-shaped, a tablet in landscape is desktop-shaped, and a touchscreen laptop is a perfectly good
desktop with a coarse pointer. A space test gets all three right. It is also the only version you
can actually test, because resizing a window takes a second and testing the other kind needs that
device in your hand.

Two more values on the same struct, for the same reason:

- **`v.safe`** is the safe-area inset already converted into layout units, ready to spend on a
  padding field. Spend it **once, at your root**: it is a property of the window, not of a widget.
- **`rcPointerIsCoarse()`** is CSS's `@media (pointer: coarse)`. Size hit targets from it rather
  than from the OS: a finger needs the same target on Android and on a Windows touchscreen laptop.
  **On Android and iOS it is `true` from the first frame**, so a phone sizes for a finger without a
  warm-up. **Everywhere else, desktop and the web included, it starts `false` and latches `true` on
  the first real touch contact**, and never latches back: a device that resized every target as you
  alternated finger and mouse would reflow the page under your hand. Treat a `true` answer as
  reliable and a `false` one as "no finger seen yet".

**Take these numbers from `rcViewport()` and not from `rcGetWindowDimensions()` divided by zoom.**
That division is correct under `RC_ZOOM_LAYOUT` and wrong under `RC_ZOOM_OPTICAL`, where the layout
is left at full size and the rendered surface is magnified instead. `rcViewport()` reports what the
layout engine was actually given, so it needs no knowledge of the mode.

## Portrait is the contract; landscape is a presentation

The branch above decides *how* the panes are arranged. It must never decide *whether* something
exists. A phone is held upright for almost every minute of its life, so the narrow arm is the app
the user actually gets, and the rule that follows from that is simple to state and easy to break:

**Everything the wide arm shows or lets the user do must be present in the narrow arm as well.**
Scrolling is allowed; omission is not. Rotating to landscape may rearrange the same set of things
(two panes beside each other, a wider table) and may never be the only place a control or a datum
appears.

The ways this gets broken are all reasonable-looking on a desktop, which is why each one is named:

| the wide arm has | the narrow arm did | the narrow arm should |
|---|---|---|
| a sidebar with labelled tabs | icons only, labels on hover | icon **and** a label, or a segment strip with text |
| a status line of live numbers | drop it "to save the row" | the same text, smaller, on a second row |
| a table with six sortable columns | keep two columns; sort by clicking a hidden header | a chip per column above the table, or the dropped columns as a muted second line in each row |
| a pane of eight per-core gauges | skip the pane below a width | draw it under the chart inside a scrolling column |
| a rail of section-jump buttons | withhold the rail | the same buttons as a wrapping "Contents" block at the top of the page |
| a fixed `380px` column | fall off the right edge | one scrolling column carrying every section in order |
| a `480px` dialog | a dialog wider than the screen | `"grow"` to the viewport minus the safe insets, labels wrapping |

Three mechanics make the narrow arm hold together:

- **The root scrolls.** A column that carries more than the viewport's height needs `.scroll = "v"`
  or its foot is unreachable, and a phone in landscape is only 393 units tall, so the wide arm
  needs it too: an app can be complete in portrait and still lose its last pane in landscape.
- **Nothing FIXED is wider than the phone.** The layout engine does not shrink a fixed-width child
  to fit; it lets it run off the edge silently. `"grow"` with a maximum, or a wrapping row, is the
  spelling that survives 393 units.
- **Text is never cut mid-word.** A title next to a trailing chip wraps to a second line under the
  narrow arm (let the row's height be `"fit"`); a code sample scrolls sideways inside its own box
  rather than clipping.

One bool per idea, still: `compact` means "the window is narrow", and nothing else. The moment it
also means "the phone sheet is open" or "a row is selected", the app opens on the wrong screen.

Test the narrow arm without a phone: make a desktop window 393x852 and 852x393 and look at both;
the layout only ever sees `rcViewport()`, so the phone shows the same thing. What the desktop window
cannot show is the safe area (zero on desktop; 46 and 24 units on a Pixel 8a, more on a notched
iPhone) and the soft keyboard, so the last look is on the device, and it is a look, never a frame
count: a build proves nothing about layout.

## When a file gets too big

One rule, and it is the same one a React project follows:

**Split a screen out of `app.h` into its own header beside it, and include it. Do not create a
folder until you have enough of them that the folder is the thing that helps.**

```
src/app/
  app.h            root layout, state, and the breakpoint branch
  screen_inbox.h   one screen
  screen_detail.h  one screen
```

A directory holding one file is a directory you have to open in order to discover it holds one
file. `src/app/components/` is a fine thing to add on the day you have four components; it is noise
on the day you have none.

**Do not add a `src/core/`.** The instinct is right and the timing is wrong: pure, portable state
and rules are a good thing to separate, but at template size it is a folder holding one file that
imports nothing, and the split is easy to make later precisely because nothing depends on where
those functions live.

## Four questions this structure gets asked

**"The titlebar is desktop-only. Does `titlebar.h` go in `platform/`?"**
**No. It goes in `app/`, and this is the clearest case there is.** A titlebar is not a portability
difference, it is a UI component that draws nothing on some targets. `rcTitlebar`,
`rcWindowControls` and `rcWindowControlButton` **emit nothing on web and mobile, and the
`RC_ID_WINDOW_*` ids are inert there** - the same source compiles and runs everywhere with no
`#ifdef`. The library already absorbed the difference, so putting it in `platform/` would move a
portable file into the folder reserved for things that are not portable. That is the mistake
`platform/` exists to prevent. The same reasoning covers anything else RayClay makes inert rather
than unavailable: it is a UI decision, and UI lives in `app/`.

**"Should `platform/` split per OS - `platform_win32.c`, `platform_macos.c`, `platform_android.c`?"**
**Not yet, and the reason is a number.** Across this repository's five full demo apps the platform
seam contains **zero lines**. Splitting an empty seam into three files gives you three empty files;
splitting it into five gives you five. File count should follow line count. When you do have
content, one function and one `#if` inside `platform.h` carries it; the day a body is long enough
that the `#if` ladder is hard to read, split *that function* out, not the whole layer. A per-OS file
that exists before there is per-OS code teaches a reader that this project has a large platform
layer, which is the opposite of true.

**"Do I need `app.h` and `app.c` both?"**
**No, and this is the pair the rule is really about.** With one translation unit `app.h` holds the
state and the layout and there is nothing left for an `app.c` to contain. If you write both you have
re-introduced exactly the declare-here-define-there split that the single-`.c` rule removes, for no
benefit: nothing else links against your app.

**"When do I get `components/`?"**
**When you have enough components that the folder is the thing that helps - around four.** The step
before it is a file beside `app.h`, and the test for taking that step is not size. It is whether
there is something in `app.h` you would want to change *without reading the rest of it*.
`examples/ex22_docs_reader` is the worked case: about 1,300 lines, and exactly one file came out
(`app/blocks.h`, which maps a document block to RayClay). Its two other candidates, the contents
panel and the status bar, stayed in `app.h` because they are about the app's own panes and you
cannot sensibly change one without reading the layout around it.

**"My app has its own seam - a data source I would swap per OS. Does THAT go in `platform/`?"**
**No, and `examples/ex20_system_monitor` is the worked case.** Its `src/app/host.h` opens by calling
itself *"the seam you replace to make ex20 a real system monitor"*, which sounds like the
definition of a platform file. It is in `app/`, and the reason is the same test as everywhere else on
this page: **count the OS branches in the file you actually ship.** That file has zero, and zero
system includes - its only `#` conditional is its own include guard - because the shipped collector
is a deterministic simulation, chosen so the example's screenshots match on every target.

The day you replace that simulation with a real one reading `/proc`, `sysctl` or Windows performance
counters, the file genuinely becomes per-OS and **that** is the day it moves to `platform/`. Which
makes it the most useful thing in the tree for understanding the boundary: one file, portable today,
and you can see exactly what would have to change about it to earn a place in the other folder.

**The general rule, and it is the one to remember:** `platform/` is decided by what a file CONTAINS,
never by what it is FOR. A file that is *about* the operating system but branches on none of it is
an ordinary part of your app.

## Coming from React or Next

| this structure | the JS equivalent | where the analogy breaks |
|---|---|---|
| `src/main.c` | `main.jsx` / `index.js` | it sets window options too, which the browser does for you |
| `src/app/app.h` | `App.jsx` | it holds the state struct as well, because there is no `useState` |
| `src/app/theme.h` | `theme.js`, design tokens | values only, and colours must be `#define` |
| `src/platform/platform.h` | *(nothing)* | the browser is your platform layer; here it is usually empty |
| `md:` in a class string, or `rcViewport().breakpoint` | a `md:` class prefix | the class form is Tailwind's, at Tailwind's values; the value is for the branches a class cannot express - which pane to declare at all |
| the layout function | a component's `return (...)` | it runs **every frame**, so it is `UI = f(state)` with no diffing to reason about |

The honest difference: there is no component instance and no lifecycle. Your layout function is
called every frame and describes the window as it should look right now. That is simpler than it
sounds, and [Getting started](getting-started.md) walks it end to end.
