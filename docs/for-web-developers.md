# Coming from the web: React / Tailwind → RayClay

RayClay is a C GUI library, but its authoring model is built for you: a developer who thinks in
components, props, state, and utility classes. This page maps what you already know onto RayClay and
flags the one place where C rules genuinely differ from JavaScript.

The one-sentence model: **RayClay is immediate-mode; your layout function is `UI = f(state)`, called
whenever the UI needs to redraw, with the reconciler removed.** There is no virtual DOM, no hooks, no
cascade. You mutate state directly and describe the whole UI in one pass; nothing diffs, everything
lays out.

**"Whenever it needs to redraw" is literal, and it is the one thing to internalise before you write
an app.** RayClay defaults to `RC_RENDER_ON_DEMAND`: it draws when something actually
happened and otherwise parks at ~0 CPU, exactly like a native toolkit, and unlike a `while (true)`
game loop. It already wakes itself for everything it can see (input, resize, focus, DPR changes). For
state it *cannot* see (a socket, a worker thread, a timer, an animation you drive yourself), you must
say so: `rcWindowRequestFrame(rcAppMainWindow(app))` for "redraw now", `rcWindowRequestFrameAfter(rcAppMainWindow(app), 0.1)` for "redraw in
100 ms". **This is the browser's `requestAnimationFrame` bargain: nothing animates for free.** A game
or simulation that genuinely must run every frame sets `.renderMode = RC_RENDER_CONTINUOUS` instead.

## What maps to what

| You know | RayClay | The difference that matters |
|---|---|---|
| JSX / HTML nesting | nested `rcBox` / `rcRow` / `rcColumn` braces | a C-preprocessor DSL, no DOM behind it |
| a component | an ordinary C function | no instance/object: just a function that emits elements |
| props | function arguments | C value/pointer lifetime rules apply (see below) |
| `useState` / app state | a long-lived `struct` you own | no hook storage; you hold the state |
| a controlled input | a pointer to a field in your state | `rcTextInput("name", state->name, sizeof state->name)` |
| conditional rendering | an ordinary `if` | re-evaluated on every redraw, like a re-render |
| `useEffect` firing a re-render | `rcWindowRequestFrame(rcAppMainWindow(app))` | state RayClay cannot see must ask for the redraw |
| `.map()` over a list | an ordinary `for` | stable ids still required (see *Lists*) |
| React `key` | the element / widget **id** | the id also carries hover / scroll / focus state |
| `onClick` | `rcClicked(id)` | the same release edge the DOM has: press, slide off, release **cancels** |
| `onMouseDown` / `onPointerDown` | `rcPressed(id)` | eager, and like the DOM's it **cannot be cancelled**; one fire per press, no auto-repeat |
| inline `style={{...}}` | designated option fields `.bg`, `.p`, `.gap` | a documented subset, no cascade, and they WIN over a class string |
| `className="..."` | `.className = "..."` | a real utility string on the same element, parsed once and cached |
| `@apply` / a component class | `rcDefineClass("card", "p-4 gap-2")` | a runtime table, not a build step; 32 names |
| `theme.extend.colors` | your own `RC_Color` constants | one name per shade, plain C |
| `@media (min-width: 768px)` | `md:` in the class string, or `rcViewport().breakpoint >= RC_BP_MD` in C | the five Tailwind prefixes at Tailwind's values, mobile-first; both forms read the *layout* width, see [Breakpoints](#breakpoints-measure-the-layout-width-not-the-window-width) |
| `env(safe-area-inset-*)` | `rcViewport().safe` | **NOT the same idea on web, and this is the row most likely to bite you.** `.safe` is **always `{0,0,0,0}` on web** - a phone browser and an installed PWA included. Nothing in a web build reads `env(safe-area-inset-*)`; the only backends that ever report a safe area are Android, UIKit and Cocoa, so a nonzero inset means a **native** build. On web the notch is your *page's* problem, because the shell is the only layer that can see `env()` at all. Where the mapping does hold, `.safe` is **already converted into layout units** - spend it at your root with no arithmetic; `rcGetSafeAreaInsets()` is the raw window-pixel form, and do not divide it by `rcWindowZoom` yourself, for the same reason the row above gives |

Backend pointers (windows, GL, fonts) are fully hidden. The pointers you *do* see are deliberate: they
make **state ownership** explicit without allocation or hidden retained objects:

```c
rcCheckbox("enabled", "Enabled", &state->enabled);
rcSlider("volume", &state->volume, 0.0f, 1.0f);
rcTextInput("name", state->name, sizeof state->name);
```

That `&state->field` is RayClay's controlled-component model: you own the storage, RayClay reads and
writes it. No `useState`, no setter, no re-render bookkeeping.

## Utility classes: `.className`

The thing you reach for first has the name you expect. `RC_ComponentOptions` carries a
`const char *className`, and it takes a space-separated Tailwind v4 utility string:

```c
rcBox(.className = "flex-col gap-4 p-4 rounded-lg bg-slate-800 border border-slate-700") {
    rcTextL("Hello");
}
```

### Text takes its own class string

`class=` works on any element in a browser, and it works on a text run here too. `RC_TextOptions`
carries its own `className`, with the same grammar:

```c
rcTextL("Revenue", .className = "text-2xl text-slate-50 leading-tight tracking-wide");
```

| Family | Spellings |
|---|---|
| size | `text-xs` .. `text-9xl`, `text-base` |
| colour | `text-<palette>`, with `/opacity`; `text-transparent` (the run vanishes); `text-inherit`, `text-current` (back to the theme colour) |
| align | `text-left`, `text-center`, `text-right` |
| wrapping | `text-wrap`, `text-nowrap`, `text-balance`, `text-pretty`, `whitespace-normal`, `whitespace-nowrap`, `whitespace-pre`, `whitespace-pre-wrap`, `whitespace-break-spaces`, `whitespace-pre-line` |
| leading | `leading-none` .. `leading-loose` (a ratio of the resolved size), `leading-<n>` (n x 0.25rem, so `leading-6` is 24 px) |
| tracking | `tracking-tighter`, `tracking-tight`, `tracking-normal`, `tracking-wide`, `tracking-wider`, `tracking-widest` |

Relative units resolve once, after the whole string is read, so `text-2xl leading-tight` and
`leading-tight text-2xl` mean the same thing. `rcDefineClass` names expand here too, but not into a layer of their own: on text a definition
substitutes its body **where it appears** and a later token wins, so `"title text-sm"` ends at
`text-sm` while `"text-sm title"` ends at whatever `title` sets. On a box the order never matters.

**A size class sets a size; it does not bake one.** The slot's baked pixel size is what the atlas
holds, and RayClay draws any other size by scaling it - the same route `.size` takes. For
a size you care about, list it in `RC_AppOptions.fontSizes` and select it by slot (`.font = F_H1`);
use `text-*` for the sizes you just want to be about right.

`tracking-tight` and `tracking-tighter` are class-only: `.letterSpacing` is unsigned, so the two
negative rungs go straight to the engine's signed field, and a non-zero `.letterSpacing` still wins.
`text-balance` and `text-pretty` apply `wrap`, which is the fallback CSS itself names for them.

**Two ways a text utility does not land, and they are deliberately different.** A spelling this
parser owns but cannot represent **warns**: `text-justify` has no justified layout behind it;
and the whole `font-*` family is claimed in order to be refused out loud, `font-bold` included,
because `.font` is a slot id from `rcLoadFont`'s registration order and there is no font-weight
surface for a class to reach. A spelling no text family owns at all is **ignored in silence** -
`uppercase`, `underline`, `line-clamp-3`, `indent-4` do nothing and say nothing - because one class
string is meant to be safe to hand to a box *and* to the text inside it. That silence is the price
of `.className = s` on both.

**The class string is the stylesheet; the typed fields are the inline style, and the typed fields
win.** Same precedence you have in a browser, and the same reason to use both: a class for the
shape you repeat, a field for the one value this element does differently.

```c
rcBox(.bg = rcColor("#0f172a"), .className = "p-4 gap-2 bg-slate-800") { ... }
/*        the C field's darker background wins, and the class brings padding 16, gap 8 */
```

### The one rule that is not the browser's: a C zero means UNSET

C structs zero-initialise, so a field you did not mention and a field you set to `0` hold the same
bytes. The fold cannot tell them apart, so **a numeric field left at zero is read as "you said
nothing"** and the class string's value stands.

```c
rcBox(.p = 0, .className = "p-4") { ... }        /* padding 16: 0 is UNSET, not zero */
rcBox(.className = "p-4 p-0") { ... }            /* padding 0: the string spelled it, so it applies */
rcBox(.w = "grow", .className = "w-64") { ... }  /* grow: .w is a STRING, and "" is its unset */
```

The rule to carry: **if you need an explicit zero, spell it in the class string.** The numeric
fields (`.p`, `.pt` and the rest of the padding shorthands, `.gap`) test truthiness and cannot see a
deliberate zero. The string fields (`.w`, `.h`, `.align`, `.overflow`, `.borderRadius`) test for a
non-empty first byte, so they can - and this is why `.px = 0` cannot undo a
`.p` on the same element. The class string is the answer to both.

### `rcDefineClass` is `@apply`, without the build step

```c
rcDefineClass("card",  "p-4 gap-2 rounded-lg bg-slate-800 border border-slate-700");
rcDefineClass("panel", "card divide-y");           /* classes may name earlier classes */

rcBox(.className = "card flex-col") { ... }
```

Call it once at startup. The name is **copied**; the utility string is **borrowed**, so pass a
string literal. It returns `false` and warns once if the name is malformed, is one a built-in
utility already owns (`"p-4"` is refused), or if the 32-name table is full. `rcResetClasses()`
forgets every definition and drops the cache - useful in tests and for a theme swap, not needed in
ordinary use.

Defined classes resolve **before** utilities no matter where they appear in the string, which is
Tailwind's components-layer order: `"card p-8"` gives you the card with `p-8`.

### What the vocabulary covers

Layout and box: `flex-row` / `flex-col`, `items-*`, `justify-*` (including `justify-between`,
`justify-around` and `justify-evenly`), `gap-*` / `gap-x-*` / `gap-y-*` / `space-x-*` / `space-y-*`,
`p-*` and every side and axis (`px`, `py`, `pt`, `pb`, `pl`, `pr`, `ps`, `pe`), `w-*` / `h-*` /
`size-*` including fractions such as `w-1/2`, `min-w-*` / `max-w-*` / `min-h-*` / `max-h-*`,
`overflow-*` per axis, `aspect-*`, `rounded-*` down to a single corner, `bg-*` over 22 of
Tailwind's 26 colour families at **v3** sRGB values (v4.2's taupe, mauve, mist and olive are
absent), plus `bg-white` / `bg-black` / `bg-transparent` and the arbitrary form, background
gradients as `bg-linear-to-{t,tr,r,br,b,bl,l,tl}` (or the v3 spelling `bg-gradient-to-*`) with
`from-<colour>` / `to-<colour>` stops and `bg-none` to remove one (two stops only - `via-*` warns;
no angle, radial or conic form), `border` and
`border-<side>-<n>` per side and axis, and `divide-x` / `divide-y`. The radius scale below is
v4's, so the two borrowings pin different Tailwind versions on purpose.

Effects: `shadow` and `shadow-2xs|xs|sm|md|lg|xl|2xl` / `shadow-none`, `ring` and `ring-0|1|2|3|4|8`,
`outline` and `outline-0|1|2|4|8`, all through the element's one `.shadow`, plus `mx-auto`. **Read
the two deviations before you reach for them.** (1) An element has ONE shadow slot, so `shadow-*`,
`ring-*` and `outline-*` are one family in three spellings and **the last token wins whole** -
`shadow-lg ring-2` draws the ring only, where CSS composes all three into `box-shadow`. (2) A
ring or outline is coloured with the **active preset's border colour** (`slate-700` dark,
`slate-300` light), resolved when the element is declared, because RayClay has no `currentColor`
to inherit; it follows the corner radius and moves no layout, exactly as `box-shadow: 0 0 0 Npx`
does. The shadow rungs are the FIRST layer of Tailwind v4's `box-shadow` (black at 0.05 / 0.1 /
0.25 alpha, e.g. `shadow-lg` is `0 10px 15px -3px`), so `sm`, `md`, `lg` and `xl` drop their
second, tighter layer; `2xs`, `xs` and `2xl` land exactly. `shadow-inner` is refused (no inset
model), and so are `ring-<colour>`, `ring-offset-*`, `ring-inset`, `shadow-<colour>` and
`outline-<style>`; each warns once. A class effect inherits the typed `.shadow`'s two
requirements whole, with its warnings: the element needs an `.id` and a visible fill (`bg-*` or a
gradient) to anchor to. `mx-auto` is the block-centring idiom and lowers to `self-center`: it
centres the child on its parent's CROSS axis, so it works inside a **column** (the case you write
it for); inside a **row** it centres the child on the row's cross axis, i.e. vertically (CSS's
`my-auto`), because it is `self-center` under another name - CSS's `mx-auto` in a row absorbs
main-axis space, which self-alignment cannot express. Every other margin (`m-*`, `my-*`, `mt-*`,
a numeric `mx-*`) warns.
`justify-items-*` is refused on purpose: it is a grid property, inert in a flex container, and
every RayClay box is one.

Rows also wrap: `flex-wrap` / `flex-wrap-reverse` / `flex-nowrap`, with
`content-start|center|end|between|around|evenly|normal` distributing the resulting LINES across the
cross axis. The worked example is the applied-filter chip row in `examples/ex21_data_explorer`: one
`rcRow(.gap = 6, .className = "flex-wrap")` holds the heading, the chips and the Clear action, one
line at a desktop width and as many as a phone needs. **One habit to unlearn, because it reads as a broken feature.** A RayClay box defaults
to fit-content on its main axis, which is CSS `max-content` - the sum of its children on one line -
so `flex-wrap` alone changes nothing until a width, a percent, a `grow` or a `max-w-*` makes the
container narrower than that sum. A browser does the same thing to `width: fit-content`; you rarely
meet it there because a block-level `<div>` is `width: auto` and already constrained by its parent.
`content-stretch` is refused rather than ignored, since every child's cross size is final before the
lines are placed. And a wrapping container has **one** gap, not two: the value that separates
children within a line is the same one that separates the lines. So on a wrapping container a
per-axis gap is refused - warned once, never silently halved - whenever the two axes could
disagree: `gap-x-4 gap-y-2`, or either one on its own. `gap-4` is the spelling that works. And
`flex-wrap` on a column (`flex-col`) is inert: only a row breaks its children into lines. A browser
given `flex-direction: column; flex-wrap: wrap` and a fixed height starts a second column beside
the first; RayClay lays the column out unwrapped and warns once (`flex-wrap is inert on a column`),
because the token is legal and the direction is what makes it do nothing.

**The only variants are the five responsive prefixes** - `sm:` `md:` `lg:` `xl:` `2xl:`, at
Tailwind's own values (640 / 768 / 1024 / 1280 / 1536) and mobile-first, exactly as Tailwind's
`min-width` media queries: `md:p-4` applies while the window's *layout* width is at least 768.
They apply in rank order after the base utilities, whatever order you wrote them in (`md:p-4 p-2`
and `p-2 md:p-4` are both 16 at `md` and both 8 below it; `md:p-4 lg:p-6` is 24 at `lg`), which is
the order Tailwind's stylesheet produces. One prefix per token; `md:lg:p-4` warns once and is
skipped. They work on a text run's `className` too (`text-sm md:text-lg`), and inside the body of
an `rcDefineClass` definition - but not on the defined name (`md:card` is refused; put the variant
in the body). No `hover:`, no `focus:`, no `dark:`, no `max-md:` and nothing keyed on the operating
system (`ios:`, `android:`): a state prefix is a selector, and there is no cascade to select in,
so those are ordinary C - `if (rcIsHovered(id))` - and a layout branches on the *space* and the
*pointer*, never the OS (see `RC_Viewport`). Transforms, filters and the grid families have no
class spelling either; the [coverage map](css-tailwind-map.md) is the complete inventory and grades
every family.

### The grammar is Tailwind's, exactly, and nothing wider

`p-4`, `p-4.5` and `p-0.25` parse; `p-04`, `p-.5`, `p-4.50` and `p-1e2` do not, because a browser
rejects them too. The opacity modifier works on every colour spelling (`bg-white/50`,
`bg-slate-800/50`, `bg-[#fff]/50`) and **scales the colour's own alpha** rather than replacing it;
it is unitless, so `bg-white/50px` is refused. Arbitrary values take the comma form,
`bg-[rgb(30,41,59)]`. Tokens split on whitespace before the colour parser runs, so the literal
`bg-[rgb(30 41 59)]` is refused; Tailwind's own escape applies, and write the space as an
underscore: `bg-[rgb(30_41_59)]` is converted inside the brackets and resolves.

An unrecognised token warns once and is skipped; the rest of the string still applies, and a bad
colour leaves the existing fill rather than guessing.

### Cost

A class string is parsed once and cached on its address, so passing the same literal every frame
costs a pointer compare. Nothing allocates: every table is fixed at compile time and lives in
`.bss`. Strings
longer than 95 bytes still work, they are simply re-parsed each frame rather than cached.

## The one gotcha: string & pointer lifetimes

This is the single place where C bites a JS developer. **`rcText` and `rcTextC` do not copy the string;
the layout engine keeps your pointer until the frame is drawn.** The most natural JS move becomes a
use-after-scope bug in C:

```c
// BROKEN: buf dies when the function returns, before the frame draws.
static void row(int n) {
    char buf[32];
    snprintf(buf, sizeof buf, "%d items", n);
    rcTextC(buf);            // RayClay retains &buf[0] - dangling by draw time
}
```

Three safe patterns:

```c
rcTextL("Static label");                              // (1) a literal: always alive
rcTextC(state->name);                                 // (2) a char* buffer YOU own that outlives the frame
rcText(rcFormat(rcAppArena(app), "%d items", n));     // (3) the per-frame arena (needs scratchArenaBytes)
```

(`rcTextC` takes a `const char *`; `rcText` takes an `rcFormat`-style `RC_String`; `rcTextL` takes a
compile-time literal. None of the three copies; pattern (3)'s string lives until the arena resets next
frame.)

The full lifetime table, worth internalising once:

| What you pass | Who owns it | Rule |
|---|---|---|
| a string literal (`rcTextL`, ids) | the binary | always valid |
| a dynamic string (`rcText` / `rcTextC`) | **you** | must outlive the frame: *not* a function-local buffer |
| a scalar (`.p`, `.bg`, `&state->volume`) | you / by value | copied or read in place; no lifetime concern |
| an `rcFormat(...)` string | the frame arena | valid for this frame only; re-format next frame |
| a loaded font / image handle | RayClay | keep the id; the resource is retained for you |
| callback `userData` | you | the pointer you pass is handed back unchanged |

There is no runtime canary for the dangling-string case; treat the rule above as the contract.

**Know what it looks like when you get it wrong, because it does not look like a lifetime bug.** It
compiles clean under `-Wall -Wextra -Werror`, it does not crash, and sanitizers often stay quiet -
the frame is still alive and the bytes have merely been reused by the next call. What you see is
*wrong pixels*, so the first place you look is the renderer or your own layout. The signature is a
run of elements that should differ and do not:

```c
// BROKEN: `initial` is gone by the time the frame is drawn.
// Symptom: every avatar in the list paints the SAME letter.
for (int i = 0; i < n; i++) {
    char initial[2] = { name[i][0], '\0' };
    rcTextC(initial, .font = F_SMALL, .color = s.text);   // borrowed, not copied
}

// FIXED, two ways. Pick by whether the text is a known set or genuinely dynamic.
static const char *const LETTER[26] = { "A","B","C", /* ... */ "Z" };
rcTextC(LETTER[name[i][0] - 'a'], .font = F_SMALL, .color = s.text);   // literals: free

rcText(rcFormat(arena, "%c", name[i][0]), .font = F_SMALL, .color = s.text); // arena: this frame
```

The same shape catches a helper that builds a string and returns: the buffer dies with the helper,
not with the frame. If a label is dynamic, it belongs in the frame arena or in storage you own.

### The same rule one step further: don't free your model *during* the layout

The table above is about a pointer that dies too early. The other half is a pointer you kill yourself,
and it is easy to reach because **a click handler runs inside your layout callback**:

```c
// BROKEN: the row is freed while the rest of THIS layout still borrows from it.
if (rcClicked("delete")) {
    note_store_remove(state, i);      // frees state->notes[i].title ...
}
rcTextC(state->notes[i].title);       // ... which a later element still reads
```

`rcClicked` and friends report an edge *while the layout is being built*, so anything you free or
reallocate there can still be read by elements declared after it, including strings the engine
retained earlier in the same frame. The failure is a use-after-free, not a blank label, and it will
not reproduce every run.

**Defer the mutation instead.** `updateCallback` runs before `layoutCallback` on each frame, so a flag
set during the layout is acted on next frame with nothing borrowed:

```c
static void update(RC_App *app, void *user) {
    App *s = user;
    if (s->pendingDelete >= 0) {      // acted on OUTSIDE any live layout
        note_store_remove(s, s->pendingDelete);
        s->pendingDelete = -1;
    }
}
// ... and in the layout, only record the intent:
if (rcClicked("delete")) s->pendingDelete = i;
```

This is the same discipline as an immediate-mode UI in any language (record intent while drawing,
apply it between frames), and it is why `RC_AppOptions` gives you an `updateCallback` at all.

## Alignment: the `.align` matrix

`.align` positions the *children* of a box in two characters: **vertical then horizontal.**

```
        left ("l")   center ("c")   right ("r")
top     "tl"         "tc"           "tr"
center  "cl"         "cc"           "cr"
bottom  "bl"         "bc"           "br"
```

So `.align = "cc"` centres both ways; `.align = "tr"` pins children to the top-right. A single axis is
fine (`.align = "c"` sets only the vertical), but the two-letter form is clearest. One caveat: `.align`
centres a *single line of text* only if the text element fills the box; otherwise a one-line label
shrink-wraps to its own width, so centre it via the **parent's** `.align`.

## Sizing: strings vs. typed

`.w` / `.h` accept CSS-like strings (`"grow"`, `"fit"`, `"200px"`, `"50%"`, `"50vw"`) that read like
Tailwind and cost exactly the same as the typed forms for static literals (measured 1:1). Reach for the
typed constants (`RC_PX(x)`, `RC_GROW`) only when the value is **computed every frame** in a hot loop.
One footnote: a decimal-percent string like `"12.5%"` parses ~1.8× slower than a whole number, still
tens of nanoseconds, so it only matters in a hot per-frame path. Rule of thumb: **strings for the values
you type by hand, typed constants for values you calculate.**

### `%` is parent-relative, and its basis is not CSS's

`"50%"` (and `RC_PCT(50)`) resolves against the **parent**, like CSS, but the basis differs in three
ways that each produce a correct-looking layout you did not ask for. Measured by rendering a 400 px row
and counting pixels, 2026-08-09:

| you write | CSS gives you | RayClay gives you |
|---|---|---|
| two `50%` siblings, `.gap = 10` | 200 + 200 → **overflows** by the gap | **195 + 10 + 195**: they *fit* |
| one `50%` child, no gap | 200 | 200 |
| a `50%` child of a **`"fit"`** parent | 200 (basis is the content box) | **0: the box vanishes** |

- **On the main axis, a percent is of what's left after the gaps are reserved.** Two 50 % siblings share
  the row instead of overflowing it. This is usually what you wanted; it is not what CSS does.
- **On the cross axis, gaps are not involved**: a clean `50%` of a 400 px parent is 200 px.
- **A percent inside a shrink-to-fit parent collapses to `0`, silently.** A `"fit"` parent sizes itself
  *from* its children, so a child asking for a percentage *of its parent* is circular and resolves to
  nothing. **Nothing is logged**; the element simply is not there, which looks exactly like a mistake in
  your own layout code. **If a box disappears, check whether its parent is `"fit"`** (that includes the
  default: `.w` unset **is** `fit`). Give the parent a definite width (`"grow"`, `"400px"`, a `%` of its
  own definite parent) or size the child in `px` / `vw`.

**`vw` / `vh` sidestep all of this**, because they resolve against the window rather than the parent,
so they work at any depth, including inside a `"fit"` box.

## Lists: stable ids are your `key`

Every interactive element needs an id, and, exactly like a React `key`, it must be **stable across
frames**, because the id is what carries hover / scroll / focus / selection state. Give each row an id
backed by memory that lives as long as the row: a `static` table of literals for a small fixed list, or
an id array you fill once for a long one (see the next section). A scratch buffer rebuilt every frame
still *hashes* correctly, so layout and clicks work, but the layout engine also keeps the id string for its debug
inspector, so scratch ids show up garbled there. Until an indexed-id shorthand lands (the equivalent of
`key={i}`), a small list is just a literal table:

```c
static const char *ROW_IDS[] = { "row0", "row1", "row2", "row3" };
for (int i = 0; i < n; i++)
    rcRow(.id = ROW_IDS[i]) { /* ... */ }
```

## Long lists: render only what's visible

Immediate mode rebuilds every element every frame, and layout charges per **declared** element, not per
*visible* one. So a 5,000-row list lays out 5,000 rows to show fifteen, and culling cannot rescue
you: an element has to be sized and positioned before anything knows it is offscreen, which is why
culling happens at draw time, too late to matter. One such list costs several times an entire
240-widget screen.

The fix is the one you already know from `react-window`: render only the visible slice and reserve the
rest of the scroll height. RayClay ships it as a loop macro. `rcVirtualList` works out the visible
window from last frame's scroll position, adds overscan rows so a fast fling has no gap, and emits the
two spacer boxes that hold the total content height constant, so the scrollbar and the scroll position
behave exactly as if every row were there.

```c
enum { ROW_COUNT = 5000, ROW_H = 28 };

typedef struct {
    const char *labels[ROW_COUNT];   /* your row data (must outlive the frame) */
    int         selected;
} ListState;

static void virtual_list(RC_App *app, ListState *st) {
    rcColumn(.id = "list", .scroll = "v", .w = "grow", .h = "grow") {
        rcVirtualList(row, "list", ROW_COUNT, ROW_H) {
            const char *id = rcFormat(rcAppArena(app), "row%d", row.index).chars;  /* key by the DATA index */
            rcRow(.id = id,
                  .bg = (row.index == st->selected) ? rcGetStyle().primary
                                                    : rcGetStyle().surface,
                  .p = 6, .w = "grow", .hType = RC_PX(ROW_H)) {
                rcTextC(st->labels[row.index]);
            }
            if (rcClicked(id)) st->selected = row.index;
        }
    }
    rcScrollbar("list");   /* a mouse gets a bar; a finger gets the browser's fading thumb */
}
```

That is the whole thing: no manual first/last arithmetic, no spacer bookkeeping, and no table of
pre-baked row ids. **The id must stay alive until the frame is DRAWN**, not merely until the call
returns: the hash is taken as the element opens, but the string itself is not copied, so format it
into the frame arena with `rcFormat(rcAppArena(app), ...)` rather than into a local buffer - which is
why the function above takes the `RC_App *` as well as its own state.

> **Why the id is keyed on the data index and not on the visible slot**, and the one ceiling worth
> knowing. Element ids are 32-bit hashes of the **string**, so formatted-index ids can collide once enough
> of them coexist in a single frame; how many is enough depends on the prefix you picked, which is not
> something you can reason about from your own code. Measured 2026-08-08: `"item%d"` is clean at 15,000
> siblings and collides at 20,000; `"Row %d"` and `"e%d"` stay clean through 65,536.
>
> **Virtualizing is what makes this a footnote rather than a hazard.** `rcVirtualList` declares only the
> visible window plus overscan, so a 1,000,000-row list still has about twenty live ids; the data index
> above is safe at any list size. The collision regime needs a *non*-virtualized list of 15,000+ siblings,
> and reaching it at all takes an explicit `RC_AppOptions.startLayoutElements` (the arena grows from 2,048).
> If you do hit it, it is not silent: the warning names the exact id.

> **Sizing note: a scrolling virtual list churns ids by design, so budget ~2× its element count.**
> the layout engine retires a generation of elements at the *end of the following* frame, so while you scroll, two
> generations are briefly resident: a 24-row list occupies about **50** slots (root + anchor + 2×24),
> not 25. If you set `RC_AppOptions.startLayoutElements` yourself, size it for roughly twice your
> per-frame element count. Undersizing is not a bug and never shows a half-drawn frame; the arena just
> doubles over a few background frames while you scroll.

> **The one shape to avoid: a scroll container inside another scroll container.** Nesting
> `overflow-y: auto` is unremarkable on the web, so this is the trap most likely to find you here. A
> scrolling parent is unbounded along its scroll axis, so the `"grow"` above resolves against its own
> content instead, and the spacers *are* that content, which is what the helper reads back as a viewport.
> Give the inner container a real height (`.h = "400px"`, or a parent that bounds it) whenever something
> above it scrolls. RayClay clamps the sampled viewport to the layout and logs one line naming
> the list, so the symptom is a stuck screenful plus a diagnostic rather than a hang. `"grow"` is correct
> everywhere else, including at the root exactly as written above.

Measured through the real DSL, the cost stops depending on how long the list is:

| rows | declared in full | `rcVirtualList` |
|---:|---:|---:|
| 100 | 397,835 Ir/frame | 78,637 (5.1×) |
| 1,000 | 3,895,411 | 78,603 (**49.6×**) |

Those are a **3-element row**, re-measured 2026-09-17; `Ir` is callgrind's instruction count, and
[api-notes.md](api-notes.md#long-lists-declaring-is-what-costs-not-showing) carries the method and
the control. Layout charges about **1,300 instructions per element you declare**, so scale the left
column by your own elements-per-row rather than quoting these: a richer row moves the absolute cost
and the ratio together. The right column barely moves at all, which is the point.

It is a memory lever too: one full-list frame permanently ratchets the element arena up (10.97 MiB at
5,000 rows), where the virtualized list holds the 1.43 MiB floor.

Three rules, and the third is the one that bites:

- **The id must name the enclosing scroll container**, and the macro sits directly inside it.
- **Every row must really be `rowHeight` tall.** The spacers are computed from that number, so a wrong
  pitch skews the scrollbar. Uniform rows only; a variable-height list still needs per-row
  measured offsets, which RayClay has no built-in for yet.
- **Key each row by its data index** (`row.index`), never by its position in the window. Keying by
  position retires and rebuilds the whole hashmap working set on every scroll step; it gives back most
  of the win *and* makes hover and selection jump between rows as you scroll.

**Do not `break` out of the `rcVirtualList` body.** The trailing spacer is emitted by the loop's *final* step, so
breaking skips it and the content ends up short by the rows you never declared; the scrollbar stops
matching the list. `continue` is fine. (This is a separate hazard from a `break` inside an `rcBox` body, which RayClay
closes for you; that one is safe.)

Virtualizing is also what keeps the id-collision ceiling in the note above a footnote: about twenty
live ids, at any list size.

## What transfers from CSS / Tailwind, and what doesn't

RayClay is **CSS-familiar and Tailwind-inspired, not Tailwind-compatible.** The honest boundary:

- **Strong:** nested flex layout, wrapping rows included; fixed / parent-`%` / `vw`·`vh` sizing; hex / `rgb()` / `rgba()` /
  named colours; a Tailwind-*named* palette plus your own tokens; themes; built-in widgets; a
  desktop-first runner.
- **Partial / different semantics:** `%` over 100 and intrinsic sizing; positioning / overflow;
  gradients; single-line text alignment; the radius scale; logical properties (padding, border,
  radius and text alignment all have their `s`/`e` and `bs`/`be` spellings; margin and inset do
  not, because neither property exists).
- **Absent by design or not yet:** the cascade; selectors / specificity; `hover:` / `focus:` state
  variants; `dark:`; `max-*:` and container queries; grid; margin; full CSS Color / text shaping.
  **The responsive variants ship**: `sm:` `md:` `lg:` `xl:` `2xl:` in any class string, at
  Tailwind's own values, and `rcViewport().breakpoint` is the same band in C (`RC_BP_SM` 640,
  `MD` 768, `LG` 1024, `XL` 1280, `2XL` 1536).

The per-field map is below.
When you reach for something in the "absent" column, the answer is usually an ordinary `if` in C: a
`hover:` is `if (rcIsHovered(id))`, and a breakpoint is `md:` or `rcViewport().breakpoint`.

**For the complete inventory rather than this hand-picked list**, including which Tailwind families
have no expression at all and which have simply not been checked, see
[CSS and Tailwind, translated to RayClay](css-tailwind-map.md). It covers all 184 Tailwind
core-utility pages and states the two limits that no future work removes.

## CSS → RayClay: the per-field map


RayClay's DSL is CSS-*like*, not CSS: a familiar subset with a few deliberate deviations. This is the
one-glance map (each field's fine print is inline with its module below):

| CSS | RayClay | Deviations |
|---|---|---|
| flexbox layout | `rcRow`/`rcColumn`/`rcBox`, `.align "<Y><X>"` | a flexbox subset; no grid, no float |
| `justify-content` | the **main-axis** letter of `.align`: **X (2nd) in a row, Y (1st) in a column** | the two letters swap roles by direction; see "module: layout". the typed `.align` is start/centre/end, and **`justify-between`/`-around`/`-evenly` resolve through `.className`** |
| `align-items` | the **cross-axis** letter of `.align`: Y (1st) in a row, X (2nd) in a column | same swap; `"cc"` reads correctly either way, which is why the flip bites late |
| `width`/`height`, `%`, `vw`/`vh`, `auto` | `.w`/`.h` strings `fit`/`auto`/`grow`/`N`/`Npx`/`N%`/`Nvw`/`Nvh` | `%` must be 0–100 and is **not** clamped: `150%` warns once and falls back to the default sizing (FIT), exactly as a bad unit does |
| `padding`/`gap`/`margin` | `.p` · `.px`/`.py` · `.pt`/`.pb`/`.pl`/`.pr` · `.gap`; `rcMargin`/`rcSeparator`; **`mx-auto` through `.className`** | px scalars only; `mx-auto` is the one margin spelling, and it is cross-axis centring (`self-center`): horizontal in a column, VERTICAL in a row |
| `color`, hex, `rgb()`/`rgba()`, names | `rcColor("…")` · `rcRgb`/`rcRgba` · `rcHex` · `rcAlpha` | hex 3/4/6/8 + `rgb()/rgba()` + 20 names + `transparent`; **no `hsl()`**, not the full 148-name set. **Watch the alpha unit: `rcAlpha` takes 0-255, where CSS `rgba()` takes 0-1.** `rcAlpha(c, 0.5f)` compiles clean, truncates to 0 and renders fully transparent; write `rcAlphaF(c, 0.5f)` for the fraction. Inside `rcColor("rgba(...)")` the alpha is CSS's 0-1, as you would expect |
| `border` width | `.border` (`RC_Border`) `"1px"` / `"all-2px"`; **per side: `.className`** `"border-b-2"` | **The two spellings are not equal and the struct one is the lesser.** `.border.width` is all-sides in the `rc` DSL, so `"b-1"` there is not a per-side spelling - it warns and draws nothing. **Per-side widths are reachable through the class grammar**, which is the route to prefer: `border-t` · `border-b-2` · `border-x-4` · `border-l-[3px]`, plus the logical `border-s`/`-e`/`-bs`/`-be`, and a bare `border-<side>` is 1 px (CSS's own default). MEASURED, eight arms headless: `border-b-2` gives `t0 r0 b2 l0`, `border-x-4` gives `r4 l4`, `border-l-[3px]` gives `l3`. The struct route stays the right one for a uniform border and for a `defaults` declaration. `RC_Border` itself has exactly two members, `.color` and `.width`. For a rule BETWEEN children, which CSS has no single property for, use `divide-x` on a row or `divide-y` on a column: each resolves against the direction the element ends up with, so `divide-y` on a row writes nothing, and both take their colour from the same `.border.color` (or a `border-<colour>` token) that the sides use. See the note under this table |
| `border-radius` | `.borderRadius` `"{side}-{size}"`, e.g. `"all-8"`, `"tl-6px"`, `"all-lg"`, `"-md"` | side ∈ `all` (or empty)/`t`/`b`/`l`/`r`/`tl`/`tr`/`bl`/`br`; size ∈ a number, `Npx`, **or** `none`/`xs`/`sm`/`md`/`lg`/`xl`/`2xl`/`3xl`/`4xl`/`full`. Empty side = all four (`"-md"` == `"all-md"`, the Tailwind shape). **A bare number = all four corners** (`"8"`/`"8px"` == `"all-8"`, like `.w`); a bare *keyword* still needs the dash (`"md"` warns → use `"-md"`). The keywords are **Tailwind v4's** radius scale, in px: `none` 0, `xs` 2, `sm` 4, `md` 6, `lg` 8, `xl` 12, `2xl` 16, `3xl` 24, `4xl` 32, `full` 9999 (a pill). **v4 renamed that scale, so one step in a v3 habit misses by 2x**: v3's `rounded-sm` is 2px and lands on 4px here, and the value a v3 user wants is `xs`. `md`/`lg`/`xl`/`2xl` are the same in both versions. `"all-none"` and `"all-0"` are equivalent spellings of "no radius", and both differ from an *unknown* token, which warns and leaves whatever radius the base declaration carried. **The palette deliberately pins v3 instead** (see the colour row), so the two halves of the Tailwind borrowing do not agree on a version and neither one governs the other |
| `background-color` | `.bg`: an `RC_Color` field on `rcBox`/`rcRow`/`rcColumn` (and `rcSeparator`/`rcMargin`), e.g. `.bg = rcColor("#1e293b")` | a struct field, not a property, and it takes an `RC_Color` rather than a CSS string; the string form is `rcColor("…")`. Unset, or any colour with alpha 0, fills nothing, exactly like `transparent`. There is no `background` shorthand. A `.gradient` on the same element **replaces** the flat fill. The `background-image` slot is `.image` (a `const RC_Image *` from `rcLoadImage`): it draws behind the children like a CSS background, but it always **stretches to the element box**; there is no `repeat`/`position`/`size`. On such an element the flat `.bg` is still painted behind the picture *and* the same colour is multiplied into it as a **tint** (no `.bg` = untinted) |
| `background: linear-gradient(...)` | `.gradient` (`RC_Gradient`, 2-stop, dir `v`/`h`/`d`/`u`); **or `.className`** `"bg-linear-to-r from-cyan-500 to-blue-500"` (eight directions, the typed field wins whole) | needs the element's `.id` either way; at most **`RC_GRADIENT_MAX`** (64) distinct elements per frame may carry one. Past the cap the element falls back to its flat `.bg` and the warning fires **once per process**, so a grid of 80 styled cards logs on frame 0 and then draws 16 of them wrong in silence. The constant is public so you can design against it; it is not tunable |
| `background` on `html` / `body` | `RC_AppOptions.clearColor` at create, `rcWindowSetClearColor(rcAppMainWindow(app), c)` live | the **window surface behind your whole UI**: a different thing from `.bg`, and no element field reaches it. **Always opaque**: alpha 0 is an input *sentinel* meaning "the active style's background *at the moment of the call*", and every other alpha is discarded; RayClay never presents a see-through window. It is a **snapshot, not a live link**, so `rcSetStyle` does not move it and a dark/light toggle must call the setter too. It is also the only thing on screen for a frame RayClay holds back while it grows its layout arena; on that frame the clear colour *is* your app → [api-notes](api-notes.md) ▸ `rcWindowClearColor` |
| `<title>` / `document.title` | `RC_AppOptions.title` | one field, both platforms: the OS window title on desktop AND the browser tab on web. You do not set it in your shell HTML. An EMPTY title is deliberately skipped rather than published, so a page that names itself in its own shell keeps that name instead of being overwritten |
| `box-shadow` | `.shadow` (`RC_Shadow`); **or `.className`** `"shadow-lg"`, `"ring-2"`, `"outline"` (one slot, last token wins; the typed field wins whole) | single-layer, linear falloff; needs `.id` + a fill (a class `bg-*` counts); at most **`RC_SHADOW_MAX`** (64) per frame, and past the cap the shadow is simply not drawn, warned once per PROCESS. The missing-anchor warning has a different grain - once per `.id`, printing it - so a design system styling four unanchored classes hears about all four. The class rungs are v4's FIRST layer; a ring/outline is the preset's border colour |
| `opacity` (element/subtree) | `.overlay` (a subtree tint), else per-colour alpha | **no true opacity.** `.overlay = rcRgba(0,0,0,120)` tints a whole subtree (mixed over the children), which covers dim/scrim/wash, but it composites a colour ON TOP rather than making the subtree translucent, and it is square-cornered. For a single colour use 8-digit hex, `rcAlpha` (0-255) or `rcAlphaF` (0.0-1.0) |
| `transition` / `@keyframes` | - | **not in v1.0**: poll state and set values yourself. Your editor **will** offer you `.transition` on `RC_ElementDeclaration`; it is **visible but unsupported**. It freezes mid-curve under the default on-demand runner (a transition advances only on a frame that actually lays out, and nothing asks for the next one); the slot pool is a fixed **200 for the whole app**, which `maxLayoutElements` does not raise; and once your own declaration fills the layout arena to capacity, elements mid-*exit* stop animating for that frame (they resume when the scene fits again). **An arena grow discards progress, by design**: a frame that outgrows the layout arena's current element capacity (which starts at `startLayoutElements` and doubles) re-creates the layout context, so an in-flight `.transition` snaps to its target on the first presented frame, an element whose enter is triggered on its first parent frame replays that enter, and an element mid-*exit* is dropped. A `.scroll` / `.overflow` container carrying an `.id` does keep its position across that grow. **And the cost is bought by the declaration, not by the motion**: any frame that declares a `.transition` anywhere runs Clay's whole layout pass **twice** rather than once, animating or not, because the branch tests only whether the transition slot pool is non-empty. A settled screen that declares one pays a second full layout every frame for as long as that element is on screen. The toll stops on the first frame after the element is withdrawn, with one exception: an element declaring `.transition.exit.setFinalState` survives the prune and keeps the second pass for its exit duration |
| `:hover` / `:active` | poll `rcIsHovered` / `rcClicked` and branch | no declarative pseudo-states; the theme ships a `…Hover` token for each of the four accents (`primary`, `danger`, `success`, `warning`) for the ternary idiom |
| `font-family`/`weight`/`size` | `rcRegisterFont` + `rcFont` | exact-size match, no synthetic bold; text is ASCII + Latin-1 |
| `line-height` / Tailwind `leading-*` | `RC_TextOptions.lineHeight` (px) | **the default is NOT the browser's.** It sets the LINE BOX and REPLACES the line's default height rather than adding to it, and the glyph run is centred in the result. Unset does not mean a typographic metric: it means the text's own pixel size, so 16 px text gives a **16 px** line box where a browser's `normal` gives you about **19**. Translating `leading-6` (24 px at a 16 px root) is `.lineHeight = 24`, but translating *nothing* is not `normal` - **set it explicitly for body copy** or your type ships visibly tighter than the design → [api-notes](api-notes.md) ▸ `RC_TextOptions` |
| `overflow` | `.overflow`: `"visible"` (default) · `"hidden"`/`"clip"` · `"scroll"`/`"auto"` | maps directly. Per-*axis* scroll is `.scroll` (`"v"`/`"h"`/`"b"`), and `.scroll` wins if you set both |
| `position: absolute` / `fixed` | `.floating` (`RC_Float`): `.to` + `.toId`, `.parent`/`.element` anchor points, `.offset` | anchor-based, not coordinate-based: you pin an anchor ON the target to an anchor ON yourself, then nudge. No `top`/`left` offsets from the viewport |
| `z-index` | `.floating.zIndex` - one `int16_t` for both cases: the full range on a floating element, saturated into Clay's `int8_t` in normal flow | higher paints later, ties keep declaration order, and the whole subtree moves with it. **In normal flow it only matters for a subtree that OVERFLOWS its own box** under a non-clipping parent: Clay tiles ordinary siblings, so they never overlap and `z-10` on one correctly changes nothing. Out of `-128..127` it warns once and saturates rather than wrapping. **A menu inside a card rising over the NEXT card needs a floating element**: CSS's initial `z-index: auto` does not establish a stacking context, every element here does, so in normal flow the menu stays confined to its card. The hit walk follows paint order, so the box you lift also takes the wheel |
| `white-space` / wrapping | `RC_TextOptions.wrap`: `""` words (default) · `"n"` none · `"l"` newlines-only | text only, and **this is the row where CSS habits mislead.** `"n"` and `"l"` suppress the break the *layout* would invent, never the break *you typed*; `white-space: nowrap` does both, collapsing a newline to a space. Measured both directions: in a box too narrow to hold it, `"alpha bravo charlie"` is 3 lines at `""` and **1 line at `"n"` and at `"l"`**; but `"alpha\nbravo\ncharlie"` is **3 lines under all three values**, `"n"` included. **If you want one line, put one line in the string.** `"n"` on a button label can push it past the layout cull edge and it VANISHES; keep labels short → [api-notes](api-notes.md) ▸ `RC_TextOptions` |
| `text-overflow: ellipsis` | - | **no ellipsis.** Clip with `.overflow = "hidden"`, or shorten the string yourself |
| `min-width`/`max-width` on an element | `.wMin` / `.wMax` / `.hMin` / `.hMax` | floats in px, `0` = unset. Do not confuse them with `RC_AppOptions.minWidth`/`minHeight`, which are the **WINDOW** minimum. They are applied **after** `.w`/`.wType` resolves, which is what makes `w-auto min-w-64` expressible: a sizing token replaces the whole axis, so a clamp written before it would be the very thing that token discards. **A PERCENT AXIS CANNOT CARRY ONE** - percent and the clamp share one union in the layout engine, so RayClay refuses and warns once rather than reinterpreting your percentage as a minimum. That rules out `w-full` with a clamp, because `full` IS a percent (100%); reach for `w-auto` or `grow` when you need a floor. `min > max` warns and **resolves to the MINIMUM**: RayClay raises the max to the min before the layout engine sees the pair, so you get the CSS answer `max(min, min(max, w))` rather than losing your minimum. **Clamps are NOT counter-scaled inside `rcUnzoomed()`, unlike a fixed-px `.w`, which is** - so in an unzoomed scope at zoom 2 a `.w` of `"100"` resolves to 50 layout px while a `.wMin` of `100` stays 100. Multiply a px clamp by `rcUnzoomedScale()` yourself if you want it to hold a constant on-screen size |
| `aspect-ratio` | `.aspectRatio` (a `float`: width divided by height) | `0` = unset, so `16.0f / 9.0f` is spelled directly. **It FILLS IN the axis you left auto and rewrites neither pin**: size one axis and the engine derives the other; size BOTH and it does nothing, silently. A ratio may make a pinned box **SHORTER** where the parent squeezes it, but **never TALLER than you pinned it** - `aspect-[3/2] h-[100px]` on a 300-wide box is 150x100, not 200x133. **Not honoured yet:** a ratio combined with a max-height on a `fit` or `grow` axis. Setting it on a `defaults` declaration still works and is still honoured when you leave the field unset - see the note under this table |
| `cursor` | `rcSetCursor(RC_CURSOR_*)`, last-writer-wins per frame | RayClay already auto-sets the expected shape per widget; `RC_AppOptions.autoCursorsDisabled` turns those defaults off |
| `letter-spacing` / Tailwind `tracking-*` | `RC_TextOptions.letterSpacing` | px |
| `text-align` | `RC_TextOptions.textAlign` (`l`/`c`/`r`), **or the class**: `text-left` · `text-center` · `text-right` · `text-start` · `text-end` | Both spellings work and the class one is the Tailwind habit. `text-start`/`text-end` are the logical names and resolve for the horizontal-tb / ltr mode RayClay lays out in, so `start` is LEFT and `end` is RIGHT - the same reading `border-s`/`border-e` take. **`text-justify` is refused**, and warns. These belong to the TEXT `className` (`RC_TextOptions`), not the box one; the two parsers have separate vocabularies. Not to be confused with the container's `.align`, which is the two-letter `"<Y><X>"` form and means something else. Multi-line blocks only; centre a single line via the parent's `.align` |

### The escape hatch behind the clamp and `aspect-ratio` rows: fields `rc` never writes reach Clay untouched

`RC_ElementDeclaration` is a pass-through typedef of Clay's own `Clay_ElementDeclaration`, and
`rcBeginComponent(options, defaults)` forwards the whole struct.

**The contract is one sentence, and it is worth stating exactly because a great deal rests on it:
the option fold overwrites only what you actually spelled.** An axis you leave unset keeps whatever
the `defaults` declaration carried; an `.aspectRatio` you leave at `0` keeps the ratio your defaults
supplied. Both rows above have their own `rc` spelling, so you rarely need this - but the
guarantee is what lets you build a component library where a shared `defaults` carries the house
rules and each call site overrides only its own.

**Two things follow from it that are easy to get wrong.** First, `0` means *unspelled* for these
numeric fields, so you cannot use the fold to force a ratio or a clamp back to zero; spell the
result you want on the defaults instead. Second, a *sizing token* is not a field but a whole axis:
`.w = "grow"` replaces `layout.sizing.width` entire, which is precisely why the clamps are applied
afterwards rather than merged into it.

Reaching past the DSL still works for anything it has no spelling for:

```c
RC_ElementDeclaration d = {
    .layout      = { .layoutDirection = CLAY_TOP_TO_BOTTOM },   /* a column, the way rcBox stacks */
    .aspectRatio = { 16.0f / 9.0f },
};
d.layout.sizing.width.size.minMax = RC_LIT(Clay_SizingMinMax){ .min = 120.0f, .max = 480.0f };

rcBeginComponent(RC_LIT(RC_ComponentOptions){ .h = "grow" }, d);
    /* your children here */
rcEndComponent();
```

Every name in that declaration is one you are meant to write: `RC_` is RayClay's public surface, and
Clay's own `Clay_`/`CLAY_` types and constants are re-exported unchanged and appear in public
signatures. **A name beginning `rci_` or `RCI_` is not**; those are internal, a few are exported only
so the `rc` macros can reference them, and they can change in any release. You do not need one here.

`rcBeginComponent` takes the options and the defaults as **two separate arguments** on purpose:
folding both into one designated-initialiser list warns under `-Winitializer-overrides` at every
call site that overrides a default. `ex20` builds its whole `card(...)` macro on this pair.

**This is the raw Clay layer, deliberately.** It is not the `rc` string DSL, so it is not covered by
that DSL's validation, and a future RayClay spelling for either field may supersede it. Reach for it when you need the capability, not as a matter of course; `ex20` uses the
same `rcBeginComponent`/`rcEndComponent` pair for its custom element macro if you want a worked
example.

**The general rule, which is worth more than either row:** whether RayClay can express something
is a question about **what you can pass in**, not about which fields RayClay's own code happens to
set. A pass-through struct hands you every field of the type it wraps, including ones the library
never writes itself.

### Breakpoints: measure the *layout* width, not the window width

**`rcGetWindowDimensions()` is not your `100vw`.** It reports the real OS window size and the zoom
factor deliberately does **not** scale it, but the default zoom mode (`RC_ZOOM_LAYOUT`) reflows the UI
by laying it out into `window / zoom`. So on a 1400px window at 200% zoom your UI is really being laid
out into **700** logical px (tablet width, where a sidebar should collapse) while
`rcGetWindowDimensions().width` still says 1400 and your breakpoint never fires. Zoom is **on by
default**, so this is not an edge case; it is every user who has pressed `Ctrl` `-`.

Both breakpoint idioms read the *layout* width for you, so neither needs the divide. The class
form is Tailwind's, mobile-first, at Tailwind's values:

```c
/* base: a column with tight padding; from md (>= 768 layout px): a row, roomier.
   Order in the string does not matter - variants apply after the base utilities,
   in rank order, as Tailwind's stylesheet does. */
rcBox(.className = "flex-col p-2 gap-2 md:flex-row md:p-4 lg:gap-6") {
    rcTextL("Title", .className = "text-lg md:text-2xl");
    ...
}
```

and the C form is the same band as a value, for the branches a class string cannot express (which
pane to declare at all, how many columns to build):

```c
RC_Viewport v = rcViewport();                 /* width/height in LAYOUT units */
if (v.breakpoint >= RC_BP_MD) { /* sidebar + content */ } else { /* one pane */ }
```

`RC_BP_SM` is 640, `MD` 768, `LG` 1024, `XL` 1280, `2XL` 1536 - the same five numbers behind
`sm:` .. `2xl:`, derived from the same width, so the two forms can never disagree. `v.width` is the
`100vw` of the space Clay lays out in, if you need the number itself.

**The band follows the layout width, not the runner.** The L3 app runner sets it at each window's
frame top, which is exact when several windows sit in different bands. Without it the band is
**derived** from the width you handed Clay, by the same ladder `rcViewport().breakpoint` reports, so
`md:p-4` resolves identically either way. A browser's media queries do not depend on who owns the
frame loop, and neither do these.

**The cache is keyed per band, and two live bands can cost a re-parse per frame.** A class string
with a variant is cached per band and re-resolves when the band changes; one without a variant is
band-free and never does. Two windows sitting in *different* bands therefore hold two entries for
the same literal in the same four-way cache set, so a scene with several variant strings in one set
and several bands live at once re-parses them every frame rather than once per crossing (measured
2026-09-16: 3 strings in one set x 2 bands = 6 parses per frame, indefinitely; the variant-free
control is 0). Nothing allocates and nothing draws wrong - it is parse work, and the multi-window
case is the one that pays it.

What they do for you, if you ever have to reproduce it by hand: `RC_ZOOM_LAYOUT` (the default)
reflows into `window / zoom`; `RC_ZOOM_OPTICAL` magnifies a fixed surface instead, and only grows
the logical space when zoomed *out* below 1.0 - so the layout width is `window / zoom` in the first
mode, `max(window, window / zoom)` in the second. Dividing by the zoom unconditionally is right in
one mode and wrong in the other; read `rcViewport()` instead.

**Sizing does not need this either.** `"50vw"` / `"100%"` / `"grow"` are resolved inside the layout
pass and are already correct in both zoom modes.

## Putting it together: a small app

Everything above in one ~50-line program: an `AppState` struct, a reusable component function,
controlled widgets, a conditional block, a list with stable ids, and your own colour tokens, driven by
`rcRunApp`. It compiles as-is on desktop and web, with no asset files:

```c
#include "rayclay.h"

/* project colour tokens: theme.extend.colors, RayClay-style.
   IN C, use #define rather than `static const RC_Color`: rcRgb expands to a
   compound literal, which is not a constant expression, so a file-scope
   static initialiser is rejected under -std=c99/c11 -pedantic-errors.
   (In C++ the same macro expands to a braced initialiser, which IS valid at
   file scope, so a C++ app may use either form.) */
#define BRAND   rcRgb(99, 102, 241)
#define SURFACE rcRgb(30, 41, 59)

typedef struct {
    bool  shuffle;
    float volume;                 /* 0..1 */
    int   selected;               /* index into TRACKS[] */
} AppState;

static const char *TRACKS[]    = { "Intro", "Nightfall", "Signals", "Afterglow" };
static const char *TRACK_IDS[] = { "t0", "t1", "t2", "t3" };
enum { TRACK_COUNT = 4 };

/* a reusable component: one list row, highlighted when selected */
static void track_row(AppState *st, int i) {
    RC_Style s = rcGetStyle();
    bool active = (st->selected == i);
    rcRow(.id = TRACK_IDS[i], .bg = active ? BRAND : SURFACE, .p = 10,
          .borderRadius = "-md", .w = "grow") {
        rcTextC(TRACKS[i], .color = active ? RC_WHITE : s.text);   // runtime string → rcTextC
    }
    if (rcClicked(TRACK_IDS[i]))
        st->selected = i;
}

static void layout(RC_App *app, void *user) {
    (void)app;
    AppState *st = (AppState *)user;
    RC_Style s = rcGetStyle();

    rcColumn(.bg = s.background, .gap = 12, .p = 20, .w = "grow", .h = "grow") {
        rcTextL("Now playing", .color = s.text, .size = 22);

        for (int i = 0; i < TRACK_COUNT; i++)          /* a list, stable ids */
            track_row(st, i);

        rcCheckbox("shuffle", "Shuffle", &st->shuffle);   /* controlled widgets */
        rcSlider("vol", &st->volume, 0.0f, 1.0f);

        if (st->shuffle)                               /* conditional block */
            rcTextL("Shuffle is on", .color = BRAND);
    }
}

int main(void) {
    AppState st = { .volume = 0.6f };
    RC_AppOptions opts = { .title = "Player", .layoutCallback = layout, .userData = &st };
    return rcRunApp(&opts);
}
```

Notice what is *not* here: no `malloc`, no destructor, no re-render bookkeeping. `st` lives on the stack
in `main`; every widget reads and writes it directly; the whole UI is rebuilt each frame from that one
struct. That is the entire RayClay model.

