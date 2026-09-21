# CSS and Tailwind, translated to RayClay

> **This is the coverage reference: every Tailwind core utility family COUNTED, and every absent
> group named.** It draws the boundary and tells you which system a gap belongs to. It is not a
> per-family lookup table: 299 families are counted here and about half are answered as a group
> rather than by name, so if you came to look up one utility and do not find it spelled out, the
> category row and the systems table below are the answer, not an omission.
> For the twenty-odd fields you actually reach for while writing a screen, read
> **[`for-web-developers.md` > CSS to RayClay: the per-field map](for-web-developers.md#css--rayclay-the-per-field-map)**
> first. That page teaches the deviations. This one draws the boundary.

## The one-sentence answer

**RayClay is CSS-familiar, not CSS-compatible.** The map covers 299 property families. The number
that are a complete match for their CSS counterpart's expressive range is **zero**. Not a small
number: zero, at family level and at the level of individual values. Measured the other way round,
against Tailwind's spellings rather than CSS's range, **237 of the 247 rows a class parser can actually
reach resolve today, and the other 10 are DECIDED rather than missing** - each carries its reason in
the table under "The distance to 1:1", so every row in that population has an answer. Measured by
running every spelling through the engine, one process per spelling per surface, most recently in
the 2026-09-19 census that the provenance note at the foot of this page pins, so the two agree.
Over the whole map that is 20.0%.
**A version stamp older than the header you hold is not automatically stale, and this one is not:**
the class parser has not changed since that run, so every figure on this page still describes the
engine you have. Re-derive them if it does - the producers are named under "Where these numbers
come from".
RayClay has **two `className` surfaces**: one on `RC_ComponentOptions` for boxes and one on
`RC_TextOptions` for text runs. They are separate parsers with separate vocabularies, and a row
is reachable when either one owns it. Twenty-five families resolve on every row they have.

**THIS FILE GRADES RayClay's OWN API, NOT THE VENDORED CLAY LAYER UNDER IT**, and the single
header you are given carries both. So a row can read "not supported" here while the matching Clay
field is visible in that same header. `Clay_LayoutConfig` and `Clay_ElementDeclaration` are the
engine's own surface: they carry fields RayClay does not expose a spelling for, and they move in
both directions without the RayClay API moving with them. So a Clay field you can see is not
evidence that you can ask for it. What you can write is this file and the typed structs in
`rayclay.h`, and nothing else.

That is the useful headline, not a disappointing one. It tells you the right way to use this
library: bring your CSS intuition for *layout shape and naming*, and check the specific field
before you rely on its full CSS range. A page that claimed compatibility would cost you an hour
per surprise.

## How to read a status

**There are two axes, and they answer two different questions.** The first is the survey's
*status*: *can you REACH this from RayClay at all*, through a typed C field on
`RC_ComponentOptions` or `RC_TextOptions`, or through the class string? The second is the *class*
axis: *does the Tailwind spelling itself resolve when you write it in `.className`?* A row can be
`partial` on the first (`.p = 16` reaches padding) and `refused` on the second (`p-[5.5px]` is not
a token the engine takes), or reachable only because of the second (`justify-between` has no typed
letter at all). The page reports both and never merges them. Three families sit on the first axis
only by way of the class string: `justify-content`'s space distribution, the per-side border
widths and `divide`; on a typed-fields-only reading they would move the family totals to 101
`partial` and 9 `escape-hatch`.

### The status axis: reachable, by any route

Every family carries exactly one of five verdicts. There is no `full`.

| Status | Families | What it means for you |
|---|---:|---|
| `partial` | 105 | The concept exists with the name you expect, over a narrower syntax or range. |
| `different-model` | 47 | The need is met, through a structurally different API. Expect to rewrite, not to translate. |
| `escape-hatch` | 6 | Not in the `rc` DSL. Reachable by setting the underlying Clay field on a `defaults` declaration. |
| `none` | 109 | A coherent feature RayClay does not expose. |
| `n/a` | 32 | A document or cascade concept that does not apply to an immediate-mode library at all. |

**FOUR `none` ROWS RE-GRADED AGAINST THAT LEGEND: TWO MOVE TO `n/a`, TWO STAY `none`.**
The test is the legend above and nothing else: `none` means *coherent here and absent*, `n/a` means
*does not apply at all*.

| row | grade | why |
|---|---|---|
| `float` | **-> `n/a`** | CSS float exists so inline content WRAPS AROUND a box. There is no text flow here for content to wrap around, so the concept has nothing to attach to. |
| `columns` | **-> `n/a`** | Multi-column breaks a continuous document flow across columns and reflows between them. Immediate mode has no such flow to break. |
| `object-position` | **stays `none`** | RayClay HAS images. Placing a picture's content inside its own box is an ordinary, coherent thing to want here and we simply do not expose it. That is the definition of `none`. |
| `visibility` | **stays `none`** | `visibility: hidden` means *occupy the space, skip the paint*. That is perfectly meaningful in an immediate-mode layout - it is a feature we do not offer, not a concept that fails to apply. |

**WHY THIS MATTERS BEYOND TWO CELLS:** `n/a` and `none` are not two shades of "no". `n/a` says the
question is malformed here; `none` is a BACKLOG ENTRY. Grading a real gap as `n/a` hides work, and
grading an inapplicable concept as `none` invents work and depresses the headline number. Both
errors were available in these four rows and one of each was present.

**NO HEADLINE PERCENTAGE IS RESTATED HERE ON PURPOSE.** The counts in the table above this note are
the file's own and were not re-derived here; a number quoted from a re-grade rather than
recomputed from the rows is exactly the kind of figure that goes stale unnoticed. The
regrade re-derives it.


**`partial` plus `different-model` plus `escape-hatch` is 158 of 299, or 52.8%.** Reachable is
the weaker of the two questions: a family moves from `escape-hatch` to `partial` the day a name you
would guess reaches it, and the total does not move at all. Read the total as a navigation aid and
nothing more. It is not a conformance score: the denominator includes Tailwind's
own mechanics and several summary rows, and a `different-model` entry can carry substantial
behavioural caveats. Nobody should quote it as "RayClay supports half of CSS".

### The status you must not misread

Under those 299 families sit **1,187 concrete value rows**, individual spellings such as
`justify-between` or `p-[5.5px]`. Their verdicts:

| Row verdict | Rows | Share |
|---|---:|---:|
| `none` | 743 | 62.6% |
| `partial` | 239 | 20.1% |
| `n/a` | 134 | 11.3% |
| `different-model` | 67 | 5.6% |
| `escape-hatch` | 2 | 0.2% |
| `exact` | 2 | 0.2% |

**These row counts, like the family counts above, are the file's own and do not yet carry the
four re-grades above.** When `float` and `columns` are regraded to `n/a`, **nine value rows move
with them**: `none` 743 -> 734 and `n/a` 134 -> 143. **The reachable headline below does not move at
all**, because neither `none` nor `n/a` is in the reachable set - which is the useful thing to know
before deciding whether to wait for the regrade.

**Every row carries a verdict.** There is no `unknown` column, and there is no reduced
denominator to argue about: **310 of 1,187 rows, 26.1%, name something you can REACH in RayClay.**
Reachable is not implemented-to-parity: a `partial` row means the concept is there under a narrower
syntax or range, and a `different-model` row means you will rewrite rather than translate. The
remaining 74% is the honest size of the gap and this page keeps it visible on purpose.
The other 743 are features it does not expose, and 134 are not RayClay questions at all - a Tailwind
build mechanic, or a value the CSS parser itself rejects.

### The class axis: does the Tailwind spelling itself resolve?

The map file grades every value row on this axis. A row is `n/a` here when the map graded it
not a class question (a CSS-only value, a build mechanic, a mechanism RayClay does not model), so the
continuity denominator is the **1,012 graded class-applicable rows**. That denominator is a
policy, not a count of utilities: 116 of the 175 `n/a` rows still carry a real Tailwind spelling
(`box-border`, `break-after-page`, the `z-[1.5]` value forms, the transform utilities), and 1,127
rows carry one in all. Both figures are reported; neither is hidden inside the other.

| Row verdict (class axis) | Rows | Share of 1,012 | What the verdict means, and only that |
|---|---:|---:|---|
| `token`: the Tailwind spelling resolves through `.className` | 145 | 14.3% | semantically graded exact class-token rows at the pinned map |
| `alias`: a class token exists under a different spelling | 1 | 0.1% | one graded alias |
| `typed`: no class token; a typed C field expresses it | 167 | 16.5% | some broad non-class route exists; NOT "one parser handler away" |
| `unsupported`: neither route reaches it | 699 | 69.1% | graded class-unsupported; NOT a proved capability gap |

**146 of 1,012 graded class-applicable rows, 14.4%, resolve by class in the PINNED grade above**
(13.0% of the 1,127 rows with a real spelling). **That grade is frozen at the revision the map was
last re-derived against and the engine has moved a long way past it** - the live measurement below reads
243. Twenty-five families resolve on every row they have and thirty-one more on some of them; the
lists are under the category table. **The pinned file grades every typography row `unsupported` on
this axis; the live engine does not.** `RC_TextOptions` carries its own `className`, so a text run
takes `text-2xl text-slate-50 leading-tight tracking-wide` directly, and 48 typography rows resolve
that way - every one of them invisible to a count that only asks the box parser. The file's grade is
pinned; every table below is live.

> **The graded number and the live number are two different measurements, and this file quotes
> both.** The semantic grade is pinned at the revision the map was last re-derived against. The live
> measurement runs every row's spelling through the engine at the current tree - one process per
> string, because the engine's bad-token warning dedupes and caps at 64 per process, and once per
> surface. In the 2026-09-19 census the live measurement counts **243** rows accepting: on the box surface
> **117 `TOKEN`** (parsed and the declaration changed), **78 `SILENT`** (accepted without a warning
> and no visible change: an identity token such as `flex` or `flex-nowrap`, an effect not carried
> in the declaration, or a responsive variant at a band it does not apply under), **932 `REFUSED`**
> and **60 `NO-UTILITY`** (placeholder cells); and on the text surface **48 `TOKEN`** against
> **35 `REFUSED`**. A `SILENT` is an acceptance, not a semantic conformance,
> which is why the live count is never merged with the graded one. **237** of the 243 sit inside
> the 247 rows a class parser can reach, which is the number quoted at the top of this page. The
> six that do not are the five responsive prefixes - counted under Tailwind's mechanics rather than
> as utilities of their own - and `gap-4` listed against `display: grid`: that token parses, and grid
> still does not exist, so counting it would tell you something false. Rows where the pinned grade and
> the live engine disagree are a regrade worklist rather than a score: where the two
> part, the live reading is the one every table below this point carries.

### The text surface

`RC_TextOptions.className` takes the same grammar as `rcBox`'s, applied to one text run:

```c
rcTextL("Revenue", .className = "text-2xl text-slate-50 leading-tight tracking-wide");
```

An explicit field still wins, which is CSS's own rule that an inline style beats a class:
`.className = "text-2xl", .size = 30` is 30 px. Relative units resolve once, after the whole string
is read, so `text-2xl leading-tight` and `leading-tight text-2xl` mean the same thing.

| Family | Spellings that resolve | Rows |
|---|---|---:|
| `font-size` | `text-xs` .. `text-9xl`, `text-base` | 13 |
| `line-height` | `leading-none` .. `leading-loose` (a ratio), `leading-<n>` (n x 0.25rem, so `leading-6` is 24 px) | 7 |
| `color` | `text-<palette>`, with `/opacity`; `text-transparent` (the run vanishes); `text-inherit`, `text-current` (back to the theme colour) | 7 |
| `letter-spacing` | `tracking-tighter`, `-tight`, `-normal`, `-wide`, `-wider`, `-widest` | 6 |
| `text-align` | `text-left`, `text-center`, `text-right`, and the logical pair `text-start` / `text-end` (resolved for the left-to-right mode RayClay lays out in, so start is left and end is right) | 5 |
| `white-space` | `whitespace-normal`, `-nowrap`, `-pre`, `-pre-wrap`, `-break-spaces`, `-pre-line` | 6 |
| `text-wrap` | `text-wrap`, `text-nowrap`, `text-balance`, `text-pretty` (the last two apply `wrap`, CSS's own fallback) | 4 |

The two negative tracking rungs are class-only: `RC_TextOptions.letterSpacing` is a `uint16_t`, so
`tracking-tight` and `tracking-tighter` go straight to the engine's signed field (em x the resolved
size, rounded half away from zero) and a non-zero typed `.letterSpacing` still wins over them.

**Two ways a text utility does not land, and they behave differently.** A spelling whose family the
text parser owns but whose value it cannot represent **warns**: `text-justify`
has no justified layout behind it; and the whole `font-*` family warns, because a RayClay font is a
slot id rather than a family-and-weight, so the parser claims `font-` precisely to refuse it rather
than let it look accepted. A spelling no text family owns at all is **ignored in silence**, because
one class string is meant to be safe to hand to a box and to the text inside it: writing
`uppercase`, `underline`, `line-clamp-3` or `indent-4` on a text run does nothing and says nothing.

The file grades **49** typography rows as the text surface. **48 of them resolve at the live tree**,
and the one that does not is `text-justify`. The families
that carry no text field at all - weight, stretch, decoration, transform, clamping, hyphenation,
word-breaking, indentation, numeric variants - are graded `engine-capability`, because they are not
one parser handler away.

**The two axes cross.** `justify-between` / `-around` / `-evenly`, the per-side and per-axis border
widths and `divide-x` / `divide-y` resolve by class and have no typed spelling at all; if you are
writing typed fields alone, treat them as absent. Conversely `.p = 16` is typed and `p-[5.5px]` is
refused, so a `partial` typed row is not a promise about the class string.

**That number is deliberately less flattering than the family figure above it**, and the gap between
them is the most useful thing on this page. A family reads `partial` if *any* of its spellings
lands; a row reads `partial` only if *that* spelling does. `interactivity.cursor` is one family and
one `partial`; underneath it, twenty-three of thirty-seven rows are reachable (six with the name
you expect, seventeen through a different model) and fourteen are not. Count
families to find your way around. Count rows to size the work.

**Read `none` as "not exposed", never as "impossible".** Roughly a dozen systems account for most of
the 743: there is no cascade to inherit from, no transform stage, no grid, no writing-mode-relative
side, no `calc()`, and no physical length unit. One system missing takes out every row that needed
it, which is why the row count falls so much faster than the family count.

## Two ways to write a style, and which one wins

Every family below is graded on what you can **write**, and there are
two places to write it. Both are on the same options struct, and the rule between them is the rule
you already know from the web.

```c
rcBox(.p = 6,                           /* the C field WINS: padding is 6, not 16 */
      .className = "flex-col gap-4 p-4 rounded-lg bg-slate-800") {
}
```

**The class string is the stylesheet. The typed C fields are the inline style, and they win.**
A class string is parsed once and cached; the C fields are folded over the result.

**A C zero is UNSET, not zero.** This is the one place the analogy breaks, and it is worth reading
twice. `.p` and `.gap` are both `uint16_t`, so a struct that does not mention them leaves them at
their zero-initialised value - which is exactly what "the caller said nothing" looks like. The
engine therefore cannot tell `gap: 0` from `gap: unspecified`. **If you need an explicit zero,
spell it in the class string** (`gap-0`, `p-0`, `rounded-none`): that is a token the parser saw, so
it is applied rather than skipped.

### Naming a class, and its limits

`rcDefineClass` is `@apply` with a table instead of a build step:

```c
rcDefineClass("card", "p-4 gap-2 rounded-lg bg-slate-800");
rcDefineClass("panel", "card divide-y");      /* a class may name earlier classes */
rcBox(.className = "card flex-col") { ... }
```

The name is **copied**; the utility string is **borrowed**, so pass a literal or something that
outlives the app. It returns `false` and warns once on a malformed name, on a name a built-in
utility already owns (`rcDefineClass("p-4", ...)` is refused), and on a full table. **A name the
TEXT surface owns is refused too** (`text-*`, `leading-*`, `tracking-*`, `whitespace-*` and
`font-*`): the text parser expands definitions before it dispatches, so a definition that shadowed
one of those would silently replace that utility for every text run in the process. Defined classes
resolve **before** utilities regardless of where they sit in the string, which is Tailwind's
components-layer order: `"card p-8"` gives you the card with `p-8`, not with `p-4`.

Five compile-time caps bound the whole engine, and none of them allocates:

| Cap | Value | What happens at the edge |
|---|---:|---|
| `RC_CLASS_DEFINE_MAX` | 32 | the 33rd `rcDefineClass` is refused and warns |
| `RC_CLASS_CACHE_MAX` | 44 | 44 slots as 11 four-way sets, PER SURFACE: element classes and text classes each get a table. A full set evicts the string resolved longest ago |
| `RC_CLASS_KEY_MAX` | 96 | a longer string still works, it is re-parsed every frame |
| `RC_CLASS_NAME_MAX` | 24 | a longer class name is refused |
| `RC_CLASS_NEST_MAX` | 4 | deeper nesting stops and warns |

### The grammar boundary, measured

The parser accepts Tailwind v4's canonical spelling on the quarter lattice and **nothing wider**,
deliberately: a parser that accepted more would silently take classes a browser rejects. The one
deliberate exception is CSS's own border-width keywords, `border-thin` / `-medium` / `-thick`, which
Tailwind does not spell and RayClay takes.

| You write | Result |
|---|---|
| `p-4` · `p-4.5` · `p-0.25` | 16 · 18 · 1 px at the default root |
| `p-04` · `p-.5` · `p-4.50` · `p-1e2` · `p-+4` | refused, one warning each, the rest of the string still applies |
| `bg-white/50` · `bg-black/25` · `bg-slate-800/50` | the opacity modifier works on **every** colour spelling |
| `bg-[#ff000080]/50` | alpha 64 - the modifier **scales the colour's own alpha**, it does not replace it |
| `bg-white/50px` · `bg-white/101` | refused: the modifier is unitless and 0..100 |
| `bg-[rgb(30,41,59)]` · `bg-[rgb(30_41_59)]` | accepted - the comma form, and the underscore form, where `_` is the space |
| `bg-[rgb(30 41 59)]` | **refused**: tokens split on whitespace before the colour parser sees them; write the underscore |
| `bg-[typo]` | the base fill is left alone and the parser warns; it never falls back to a guess |
| `w-md` · `w-3xs` · `w-7xl` | the container scale on the width: `3xs` through `7xl` are 16 through 80 rem, fixed |
| `w-svw` · `w-lvw` · `w-dvw` · `h-svh` · `h-lvh` · `h-dvh` | the screen on that axis; the alias for the other axis (`w-svh`) is refused |
| `border-s` · `border-e` · `border-bs` · `border-be` | logical sides, lowered to left, right, top and bottom |
| `border-thin` · `border-medium` · `border-thick` | 1, 3 and 5 px |
| `border` · `border-2` · `border-t` | a WIDTH. The colour comes from the active preset's `border`, the same hairline the library's own widgets draw, so it follows the preset into light and dark. `border-<colour>` sets it explicitly and wins wherever both appear, because a bare `border` never writes the colour channel |
| `z-10` · `-z-10` · `z-[3]` · `z-auto` | accepted on floating AND in-flow elements; a typed non-zero `.floating.zIndex` wins. In flow the range is `int8_t` and a value outside `-128..127` warns once and saturates. `z-auto` and `z-0` resolve to 0 and never warn. Ordinary in-flow siblings tile and never overlap, so z only reorders a subtree that overflows its box |
| `aspect-3/2` · `aspect-[3/2]` · `aspect-[1.5]` | a ratio; the bare decimal `aspect-1.5` is refused, as Tailwind refuses it. **A ratio fills in the axis you left auto; it may make a pinned box SHORTER in a squeeze but never TALLER than you pinned it** - `aspect-[3/2] h-[100px]` on a 300-wide box is 150x100. Size both axes and it does nothing, silently. **`aspect-*` with a max-height on a `fit` or `grow` axis resolves the ratio and IGNORES the maximum** - `aspect-6/1 max-h-[40px]` at 300 wide draws 300x50, not 300x40. Measured, and declined for v0.9 on cost rather than left pending: a working prototype prices every compressing parent about 1%, against a 0.5% budget |
| `place-content-*` · `place-items-*` | aliases of `justify-*` and `items-*`, same vocabulary, same reach |
| `rounded-s` · `rounded-e` · `rounded-ss` · `rounded-se` · `rounded-es` · `rounded-ee` | CSS logical corners. For horizontal-tb/ltr: `s` is left, `e` is right, the block axis is top/bottom, so `rounded-ss` is the TOP-LEFT corner |
| `pbs-*` · `pbe-*` | logical block padding: `pbs-` is padding-top, `pbe-` is padding-bottom |
| `min-w-md` · `max-w-2xl` | the container scale on the width clamps (`max-w-2xl` is 42 rem = 672 px) |
| `h-md` · `h-7xl` · `size-lg` · `size-3xs` · `min-h-md` · `max-h-md` | **refused, and deliberately**: the container scale is a WIDTH family and reaches `w-`, `min-w-` and `max-w-` alone, because that is what Tailwind v4.3.3 generates - it emits nothing for `max-h-md` |
| `shrink-0` · `shrink` · `flex-none` · `flex-initial` · `flex-auto` · `flex-1` · `grow` · `grow-0` | the flex-shrink family. Two caveats below |
| `items-stretch` · `place-items-stretch` | the cross axis STRETCH: a child with no size on that axis fills the parent's inner extent, as CSS `align-items: stretch` |
| `justify-stretch` · `justify-normal` | start on the main axis, which is what CSS does with `stretch` on a flex line; accepted, not a distribution |
| `self-auto` · `self-start` · `self-center` · `self-end` · `self-stretch` | a per-child override of the parent's cross-axis alignment (`self-auto` inherits); `self-baseline` is refused: there is no baseline |

An unrecognised token never voids the rest of the string: recognised utilities still apply, and you
get one warning naming the token that failed.

**Two caveats on the flex family, and both are places this is NOT 1:1.**

**`flex-auto` behaves as `flex-1`.** CSS's `flex-auto` is `1 1 auto`, which distributes free space
*after* each item's content width is reserved; `flex-1` is `1 1 0%`, which ignores content and
splits the space evenly. RayClay's growth measures as the second. Measured: a 600 px row over one
400 px-content and one 40 px-content growing child reads **400 / 200**, where `flex-auto`'s own
answer would be 480 / 120. The token is accepted as an alias of `flex-1`; do not read it as CSS's
`flex-auto`.

**`grow`, `shrink` and the `flex-*` shorthands set BOTH axes.** In CSS these name the parent's main
axis, but a class rule is resolved with no parent in hand, so RayClay applies it to both. On the
cross axis a growing child fills the parent's inner extent even under `items-start`. When that
matters, spell the axis you mean with `w-`, `h-` or `size-` instead.

### Alignment is the one place the two surfaces differ in reach

The typed `.align` field is **3x3**: `top`/`centre`/`bottom` on one axis, `left`/`centre`/`right` on
the other, and that is its whole vocabulary. The class string reaches further on the **main** axis
only: `justify-between`, `justify-around` and `justify-evenly` lower onto real space distribution,
on a row and on a column alike. On the cross axis the typed field is start / centre / end, while the
class string adds `items-stretch` (a child with no cross-axis size fills the parent) and the
per-child `self-*` family, which has no typed spelling at all. `place-content-*` / `place-items-*`
are the same vocabularies under Tailwind's shorthand names. So if you want `space-between`, a
stretched child or one child aligned differently from its siblings, write it as a class - there is
no letter for any of them in `.align`.

## `p-5` is not five pixels

The single most expensive assumption a Tailwind developer brings across.

A Tailwind utility token is a **theme reference**, not a value. Under the default
`--spacing: 0.25rem`, `p-5` is `1.25rem`, which is 20 CSS pixels at a 16px root. RayClay's `.p` is
a logical-pixel count. So:

| You want | CSS | Tailwind | RayClay |
|---|---|---|---|
| 5 physical px | `padding: 5px` | `p-[5px]` | `.p = 5` |
| the look of `p-5` | `padding: 1.25rem` | `p-5` | `.p = 20` |
| 5.5 px | `padding: 5.5px` | `p-[5.5px]` | no spelling |
| 5% | `padding: 5%` | `p-[5%]` | no spelling |

**Translate the resolved value, never the digit in the class name.** This holds in both directions
and for every theme-backed family, not only spacing.

### "Tailwind's numeric scale" is at least seven scales

The digit after the dash does not mean the same thing across families, and assuming it does is the
way to be wrong by a factor of four while feeling precise:

| Family | What `N` means | `4` resolves to |
|---|---|---|
| `p-*` `m-*` `gap-*` `w-*` and the rest of spacing | `N x 0.25rem` | **16 px** |
| `border-*` `divide-*` `outline-*` `outline-offset-*` `ring-*` `inset-ring-*` `ring-offset-*` `decoration-*` `underline-offset-*` | **raw pixels** | **4 px** |
| `opacity-*` | percent | 4% |
| `z-*` | unitless | 4 |
| `tracking-*` | em, and it can be **negative** | not a `4` at all |

**Nine families emit raw pixels from a bare integer** and do not read `--spacing`:
`border-width`, `divide-width`, `outline-width`, `outline-offset`, `ring-width`, `inset-ring`,
`ring-offset`, `text-decoration-thickness` (`decoration-2` is 2 px, not 0.5 rem) and
`text-underline-offset` (`underline-offset-4` is 4 px, not 1 rem). The last two are the easiest to
miss because they are typography rather than borders.

**This page documents Tailwind v4, and v4's spacing scale is a RULE rather than a table.** If you
carry a v3 intuition that the scale is a fixed set of named keys and that `p-13` does not exist,
drop it here: v4 generates the whole ladder from one number.

v4 replaced v3's fixed named map with ONE scalar: `--spacing: 0.25rem`, and every bare numeric
spacing utility is a MULTIPLE of it. The gate is a predicate, not a table -
`num >= 0 && num % 0.25 === 0 && String(Number(v)) === String(v)` - so **`p-13` IS a valid v4 class**
(`calc(var(--spacing) * 13)` = 3.25rem = 52 px at a 16 px root), and so is `gap-4.5`, which v3
rejected. **There is no upper bound in the predicate.** The four fractional keys v3 shipped
(`0.5 1.5 2.5 3.5`) were never special: they are simply four points on the quarter lattice.

`px` in that family still means literally one CSS pixel rather than a scale step - that part was
always true, and it is one of three unrelated meanings the token carries in v4 (one pixel in
`p-px`, the inline AXIS in `px-4`, and the unit inside `p-[5px]`).

`0.25rem` is `4 px` at CSS's default 16 px root, which is where the spacing column above comes from.

### What `.p` accepts

`.p` and its siblings `.px` `.py` `.pt` `.pb` `.pl` `.pr` `.gap` are `uint16_t` logical pixels.
Three consequences worth knowing before you design around them:

- **Integer only.** `.p = 5.5f` is a narrowing conversion that happens before RayClay sees it.
  There is no fractional padding to lose; the value simply arrives as `5`.
- **Unchecked range.** `.p = -4` stores `65532`. A negative padding is not rejected, it wraps.
- **Zero is the unset sentinel.** `.p = 12, .pl = 0` leaves the left padding at 12. The CSS idiom
  `padding: 12px; padding-left: 0` has no direct expression. Spell the three sides you want
  instead: `.pt = 12, .pb = 12, .pr = 12`.

Unit strings and percentages do not type-check for these fields: `.p = "5px"` is a compile error,
which is the failure mode you want.

## Name the Tailwind version, always

RayClay tracks **two different Tailwind majors in two different places**, and a reader who diffs
against the wrong doc page will file a bug that is not there.

| What | Tracks | Where |
|---|---|---|
| `.borderRadius` rungs | **Tailwind v4** | `none 0 · xs 2 · sm 4 · md 6 · lg 8 · xl 12 · 2xl 16 · 3xl 24 · 4xl 32 · full 9999`, byte-exact against v4's ladder |
| the colour palette | **Tailwind v3** | the `RC_SLATE_500`-style constants in `rayclay.h` carry v3's sRGB values |

The radii look shifted by one name against v3 and are not: **v3 has no `rounded-xs` at all** and puts
2 px on `sm`, so a v3 reading makes RayClay's correct ladder appear to be off by one rung. It is an
easy mistake to make and the ladder above is the one to check against.

### Two caveats on the palette, because a palette is the first thing an evaluator diffs

RayClay ships **22 of Tailwind's 26** named families, 11 shades each (50-950).

- **It is not the complete current palette.** Tailwind v4.2 added `taupe`, `mauve`, `mist` and
  `olive`; RayClay ships none of them. Define them yourself if you need them - a shade is a plain
  `RC_Color` constant and the pattern is on the card.
- **The same name is not the same value as current Tailwind.** v4 re-based the palette onto `oklch`,
  so `RC_SLATE_500` carries v3's sRGB value and will not byte-match today's `slate-500`, most
  visibly on a wide-gamut (P3) display. That is a pinned-provenance decision rather than a defect:
  the constants are stable and do not shift under you when Tailwind ships a new version.

`v3.tailwindcss.com` and `tailwindcss.com` disagree on the small end of that ladder, so **cite the
version whenever you quote a Tailwind value**, in a bug report or in your own notes.

The radius ladder is **complete against v4**: `none` (0), `3xl` (24) and v4-only `4xl` (32)
sit beside the seven byte-exact steps. `full` stays RayClay's 9999 px pill
rather than v4's `calc(infinity * 1px)`, which has no meaning in a float corner radius.

## Two limits of the layout engine - one scheduled, one permanent

Most of what is missing is work nobody has done. These two are different: they are properties of
the layout engine RayClay is built on, so no amount of DSL work removes them. **They are not the
same KIND of limit.** One was costed and scheduled; the other was costed and declined. Both
were measured on 2026-09-18.

1. **`flex-row-reverse` and `flex-col-reverse` are not something you can write.** RayClay has no
   spelling for a reversed row or column, so reverse your data before you iterate it. This is a
   missing spelling rather than a missing capability, which is the cheaper of the two to close.
   **THE ESCAPE HATCH REACHES IT, AND THAT IS NEW.** Setting the underlying field on a `defaults`
   declaration is the documented hatch, and RayClay's own layout now ASKS THE AXIS rather than
   comparing against one direction - so a reverse row is treated as a row. That matters for four
   things a developer can see: `divide-x`/`divide-y` and `gap-x`/`gap-y` land on the right axis, and
   the "flex-wrap is inert on a column" warning fires only on a genuine column.
   **The enumerators and the hatch are the engine's; an `rc` spelling and a class token are not
   built yet** - verified by grepping the class parser and the public header, both of which return
   nothing for a reverse form.
   What this costs, and it is the part worth keeping: adding ENUMERATORS to the existing full-width
   field is free on every ABI, which is why this shape shipped and a separate flag did not.

   **THE ZERO COST BELONGS TO ONE SPELLING ONLY, and it is worth recording because the obvious
   alternative was the expensive one.** Adding ENUMERATORS to the existing full-width field is free
   on every ABI. Adding a SEPARATE reverse FLAG beside it is not: measured 2026-09-18, a new 1-byte
   field appended to `Clay_LayoutConfig` is free on LP64 (it lands in an existing 2-byte tail hole,
   `Clay_LayoutElement` 288 -> 288) but costs **+8 bytes per element** on the wide-enum path, where
   that struct is exactly filled at 60 with no holes - the same +8 that got point 2 declined. The
   wide-enum figure is gcc with `CLAY_PACKED_ENUM` forced to a plain `enum` to isolate the width
   variable; it is a PREVIEW of the Microsoft ABI rather than a measurement of it, and the MSVC leg
   settles it.
2. **A percentage size and a min/max clamp cannot coexist on one axis. This one IS permanent for
   v0.9, by a deliberate decision rather than by absence of effort.**
   `Clay_SizingAxis.size` unions `Clay_SizingMinMax` with `float percent`: they occupy the same
   storage, so one has to lose. **`w-1/2 min-w-64` has no expression at any layer**, and it is the
   one Tailwind pairing this page cannot translate at all. RayClay refuses the clamp and warns once
   rather than reinterpreting your percentage as a minimum, so you find out at the log rather than
   from a mis-sized panel. Choose one: a percentage, or a clamped fixed size.

   **WHY IT WAS DECLINED, so nobody reopens it as an oversight.** Un-unioning it was measured on
   LP64 gcc: `Clay_SizingAxis` 12 -> 16 bytes, `Clay_Sizing` 24 -> 32, `Clay_LayoutConfig` 44 -> 52,
   and `Clay_LayoutElement` **+8 bytes** - about **+1.1% on the 699 B/element arena, paid by every
   element whether or not the app ever writes a percentage with a floor.** RayClay's stated
   philosophy is that an app must run indefinitely with stable memory, and a permanent per-element
   cost on a published contract to buy one pairing is the wrong trade. There is no cheaper spelling:
   the pairing needs percent AND min AND max live at once - three floats where the union has room
   for two - so it is 12 bytes before alignment takes it to 16 whichever way it is arranged.

   Every other `min-w-*` / `max-w-*` pairing resolves - see `.wMin` / `.wMax` in the per-field map -
   including an INVERTED one, which is point 3.
3. **An INVERTED pair resolves to the MINIMUM, which is the CSS answer, and it warns.**
   `min-w-[200px] max-w-[100px]` gives **200**, exactly as a browser does: CSS 2.1 section 10.7 has
   the minimum override the maximum, and RayClay raises the max to meet the min before the layout
   engine sees the pair. The log says so in those words - *".wMin 200 exceeds .wMax 100; the MINIMUM
   wins, as in CSS"*. **The typed fields and the class string agree**, because both routes reach the
   same clamp. Measured on a box asking for 150, against a sane control that resolves to 150 - so the
   200 is the clamp acting and not a token that failed to parse.
   **The warning is LATCHED**, like most of RayClay's: two inverted declarations produce one line, so
   seeing a single warning does not mean a single mistake. An inverted pair is almost always an error
   in the source, and here you find out from the log rather than from a mis-sized panel.

## Coverage by category

> **Eight families look like gaps and are not, and the page says which so nobody re-files them.**
> Three are FINAL `none` by design: absolute physical units (`in`/`pt`/`cm`/`mm`/`pc`/`Q`), because
> no screen carries a physical-DPI contract and the web has none either; blend modes past the
> hardwired multiply, which cost real per-pixel GPU time and are vanishingly rare in application UI;
> and the long tail of the cursor set. Crosshair, wait/progress and the resize shapes are
> `different-model` rather than `none` - they belong with the panel work and will land named.
> Two more read as gaps but are shaped by the immediate-mode model, and both sit inside the
> `partial` Flexbox and Grid families rather than in a weaker grade. **Reverse flex directions have
> no Clay spelling at all** - `Clay_LayoutDirection` is left-to-right or top-to-bottom and nothing
> else - but your loop controls child emission order, so reversing one is a one-line change in your
> own code. **Per-axis `gap` collapses to one scalar**, because the underlying child gap is a single
> value by construction; spell the axis you need and pad the other.
>
> **`space-between`/`around`/`evenly` and per-side border widths are reachable** through the class
> string, and both are graded `partial` above. **Negative letter-spacing is reachable from the class string and
> not from the typed field**: `RC_TextOptions.letterSpacing` is a `uint16_t`, so `.letterSpacing`
> cannot spell a negative value, but `tracking-tight` and `tracking-tighter` carry their em product
> SIGNED past that field into the engine's own signed one, and a non-zero typed `.letterSpacing`
> still wins over them. **Fractional letter-spacing is `none` on both routes**: the class product is
> em x the resolved size, rounded to whole pixels.
>
> **The four logical-size families carry a caveat the physical fields cannot discharge.**
> `inline-size` and `block-size` are writing-mode relative; `.wMin` / `.hMin` are physical. They
> coincide only in `horizontal-tb`, so treat a logical-size utility as translated rather than
> equivalent.

All 299 families, by Tailwind's own grouping. The five status columns are the survey's status at
family level; the last two are the class axis at row level, **measured in the 2026-09-19 census** by running
every spelling through the engine. "Rows with a utility" excludes the rows with no Tailwind utility
to try, and "resolve by class" counts a row that resolves on EITHER `className` surface: a typography
row lands on `RC_TextOptions`, and a count that only asked the box parser reads that whole category
as zero. Read a row as a shape, not a score.

| Category | Families | `partial` | `different-model` | `escape-hatch` | `none` | `n/a` | rows with a utility | resolve by class |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Typography | 34 | 8 | 1 | 0 | 19 | 6 | 164 | 48 |
| Spacing | 30 | 17 | 10 | 0 | 1 | 2 | 19 | 11 |
| Tailwind mechanics | 28 | 12 | 13 | 1 | 0 | 2 | 8 | 5 |
| Flexbox and Grid | 25 | 13 | 0 | 0 | 12 | 0 | 159 | 55 |
| CSS value types | 21 | 11 | 4 | 0 | 1 | 5 | 20 | 6 |
| Interactivity | 21 | 4 | 6 | 0 | 9 | 2 | 126 | 0 |
| Layout | 21 | 5 | 4 | 0 | 4 | 8 | 129 | 20 |
| Filters | 20 | 0 | 0 | 0 | 20 | 0 | 62 | 0 |
| Borders | 17 | 6 | 5 | 0 | 6 | 0 | 60 | 38 |
| Effects | 17 | 3 | 0 | 0 | 14 | 0 | 107 | 9 |
| Backgrounds | 16 | 4 | 1 | 0 | 11 | 0 | 52 | 14 |
| Sizing | 16 | 16 | 0 | 0 | 0 | 0 | 46 | 37 |
| Transforms | 12 | 1 | 1 | 0 | 5 | 5 | 93 | 0 |
| Transitions and Animation | 9 | 0 | 0 | 5 | 3 | 1 | 46 | 0 |
| Tables | 5 | 1 | 2 | 0 | 2 | 0 | 14 | 0 |
| SVG | 4 | 4 | 0 | 0 | 0 | 0 | 18 | 0 |
| Accessibility | 3 | 0 | 0 | 0 | 2 | 1 | 4 | 0 |
| **Total** | **299** | **105** | **47** | **6** | **109** | **32** | **1127** | **243** |

**Strongest:** sizing (37 of 46 rows), borders (38 of 60), the theme-like half of Tailwind's
mechanics (5 of 8) and spacing (11 of 19). Those are the areas where your habits transfer with the
fewest surprises.

**Thinnest, and every one of them reads zero:** filters, transforms, transitions and animation,
tables, SVG, accessibility, and the pseudo-class half of interactivity. Effects reads 9 of 107 -
`shadow-*`, `ring-*` and `outline-*` all land on the one `RC_Shadow`, so the family resolves where
CSS would compose. The Grid half of Flexbox and Grid is still absent; what resolves there is flex.

**Every utility row resolves in these 25 families:** `backgrounds.background-color`, `backgrounds.background-image`, `backgrounds.linear-gradient`, `borders.border-radius`, `borders.divide`, `borders.outline-width`, `flexbox-and-grid.flex-wrap`, `layout.aspect-ratio`, `layout.overflow`, `layout.overflow-x-y`, `sizing.container-scale`, `sizing.max-height`, `sizing.min-max`, `sizing.size`, `sizing.viewport-units`, `spacing.margin-inline`, `spacing.padding-inline-block`, `spacing.padding-side`, `spacing.space-x`, `typography.color`, `typography.font-size`, `typography.letter-spacing`, `typography.line-height`, `typography.text-wrap`, `typography.white-space`

**Some rows resolve in these 31:** `backgrounds.gradient-color-stops`, `backgrounds.gradient-interpolation`, `borders.border-width`, `borders.divide-width`, `borders.ring-width`, `css-value-types.color-syntax`, `css-value-types.integer`, `css-value-types.length-units`, `effects.box-shadow`, `flexbox-and-grid.align-content`, `flexbox-and-grid.align-items`, `flexbox-and-grid.align-self`, `flexbox-and-grid.display-grid`, `flexbox-and-grid.flex`, `flexbox-and-grid.flex-direction`, `flexbox-and-grid.flex-grow`, `flexbox-and-grid.flex-shrink`, `flexbox-and-grid.gap`, `flexbox-and-grid.justify-content`, `flexbox-and-grid.place-content`, `flexbox-and-grid.place-items`, `layout.display`, `layout.z-index`, `sizing.height`, `sizing.max-width`, `sizing.min-height`, `sizing.min-width`, `sizing.width`, `spacing.padding`, `tailwind-mechanics.variants-breakpoints`, `typography.text-align`

## The distance to 1:1

The goal is a one-to-one map from Tailwind's utilities to RayClay. The map file carries a
per-row verdict on the class axis, so this is arithmetic rather than an estimate. Of the 1,187
value rows, **175 were graded not a class question**, which leaves the **1,012 graded
class-applicable rows**:

| | Rows | Share of 1,012 | What the label is evidence of |
|---|---:|---:|---|
| resolves by class today (`token` + `alias`) | 146 | 14.4% | the engine takes the spelling; nothing to do |
| some non-class route exists, no token yet (`typed`) | 167 | 16.5% | a route of SOME kind: a typed field, a text API, a global call, a widget, an SVG source |
| no class route today (`unsupported`) | 699 | 69.1% | partitioned by `route_kind`, below |

**Every row carries a `route_kind`: the one thing that stands between it and a class spelling.**
That is what makes the next line arithmetic rather than an estimate.

| `route_kind` | rows | what it means |
|---|---:|---|
| **`component-class`** | **198** | the field exists on the element today; 189 of them resolve, and a class handler is all that is missing from the rest |
| `text-class-surface` | 49 | the text surface: 48 of them resolve through `RC_TextOptions.className`; the one that does not is `text-justify`, a value that parser owns and refuses |
| `engine-capability` | 434 | no field at all: transforms, filters, background tiling, blend, most value types, and every typography property with no text field |
| `conceptual-n/a` | 184 | no meaning in an immediate-mode C UI, or a Tailwind build mechanic |
| `structural-widget` | 143 | another layout model: grid, tables, float, multi-column, positioned offsets |
| `global-action` | 119 | app or runtime behaviour, not a property of an element |
| `clay-defaults` | 42 | the layout engine's transition model owns the timing |
| `svg-source` | 18 | an attribute of the SVG document, not of the element that draws it |

- **The population a class parser can reach is 247 rows. 237 RESOLVE and the other 10 are DECIDED -
  none of them is an open gap.** That is 198 box rows of which 189 resolve, plus the 49 text rows, of
  which 48 resolve. `text-start` and `text-end` resolve to the same alignment as `text-left` and
  `text-right`, since RayClay lays out horizontal-tb / ltr.

  **THE TEN, EACH WITH ITS REASON. Read this before filing any of them as missing:**

  | Row | Why it does not resolve |
  |---|---|
  | `justify-items-start` · `-end` · `-center` | **Grid-only in CSS, and there is no grid.** In a flex container the property is inert, so a token that resolved would be a lie about what happened. `items-*` is the axis you want |
  | `shadow-inner` | **No inset model.** `RC_Shadow` is one outer shadow; an inset shadow is a different paint op, not a parameter of this one |
  | `text-justify` | **No justified layout.** The text layer breaks lines; it does not distribute the slack inside them. The parser owns the `text-` family, so this one WARNS rather than being ignored |
  | `bg-blend-multiply` | **No blend state in the renderer.** Every fill is source-over; a blend mode needs a pipeline the paint stage does not have |
  | `slate-500` (v3) · `slate-500` (v4) | **Not utilities.** These two rows name a palette VALUE, not a class you can write. The utility that carries the colour is `bg-slate-500`, and it resolves. They stay in the map because the map records the palette's provenance |
  | `spacing.spacing-scale` default | **No Tailwind spelling to try.** The row is the scale itself, not a utility; it is `NO-UTILITY` on both surfaces |
  | `border-[thin]` | **A CSS keyword inside an arbitrary-value bracket.** Measured refused; the bracket form carries a length and `border-[3px]` resolves. The confusing part, worth knowing: **`border-thin` without brackets DOES resolve** - CSS's named widths are a RayClay extension (`thin`/`medium`/`thick` = 1/3/5 px), listed in the extensions table above |

  **`@platform` IS RayClay's NAME FOR THE CSS `@media` PARALLEL, AND IT IS KEYED TO CAPABILITY AND
  VIEWPORT - NEVER TO THE OPERATING SYSTEM.** Width, orientation, pointer and truthful user
  preferences are the model; "is this Android" is not a question it can ask.

  **PART OF THAT MODEL SHIPS TODAY AND THE REST IS NOT IMPLEMENTED. Do not build on the unshipped
  half.** What ships: `sm:` `md:` `lg:` `xl:` `2xl:` work on both className surfaces at Tailwind's
  own values, `rcViewport().breakpoint` is the same band in C, and the pointer question is answered
  by `rcPointerIsCoarse()` or `rcViewport().coarsePointer`. What does NOT yet exist: a bracketed
  class condition for utility-only changes, and a typed query or block for structural ones. Neither
  is in the tree, so this page records them as the shape and not as a feature.

  **WHAT IS REFUSED, and it is a decision rather than a gap** (2026-09-16): an OS-KEYED prefix. That
  refusal is not this page's opinion - the class parser rejects those prefixes, and `rayclay.h`
  states the refusal beside the variant list. There is no class spelling for the pointer either:
  `pointer-coarse:` is refused along with every OS-keyed prefix. A browser has no OS-keyed media
  query: it has
  `pointer: coarse`, `hover: none`, `prefers-color-scheme` and width
  breakpoints, and it deliberately refuses to let a stylesheet ask which vendor it is running on,
  because the question a layout actually has is about the INPUT and the VIEWPORT. An OS-keyed
  variant would also be the one construct here that cannot be tested by resizing a window.
- **The text surface is measured, not predicted.** Every row graded `text-class-surface` was run
  through the engine at the current tree, one process per spelling per surface; the seven families
  that hold carry 48 resolving rows between them - font-size 13, color 7, line-height 7, white-space
  6, letter-spacing 6, text-align 5 and text-wrap 4.
- **The 434 `engine-capability` rows are not a backlog of 434 tickets.** They collapse into the
  systems in the table below, each one design decision: no retained paint stage (every filter, mask,
  blend and transform), no shaped-text layer, no grid, no cascade, no `calc()`, no physical length
  unit. One absent system takes out every row that needed it.
- **Judging a row by its CATEGORY is wrong, and the file was graded one level deeper than that.**
  `RC_ComponentOptions` carries a gradient and a shadow, so `bg-linear-to-l` and `ring-2` sit in
  `component-class` while their whole categories look unreachable - and reading those two structs
  takes rows back off the list: the gradient is two stops, so `via-*` and every colour-space
  interpolation variant is out; the shadow is one shadow with no inset flag, so `ring-offset-*` and
  `inset-ring-4` are out. Ask at the field, not at the family.
- **Rows are not utilities.** Ten rows are spelled as patterns (`border-t-*`, `rounded-tl-*`); the
  file's expansion manifest enumerates them over their own family's scale into **74 concrete
  utilities, and all 74 resolve**. No utility-weighted percentage is quoted here
  because the other 1,177 rows have no enumerated multiplicity yet.
- **1:1 therefore means: every row either resolves by class, or carries its reason.** Not every row
  resolving. The reasons are in the file, per row, and summarised in the systems table below.

## What is missing, grouped by system

Counting the 109 `none` families as 109 pieces of work overstates the gap by roughly tenfold. They
collapse into a handful of systems, each of which is one design decision rather than a backlog.

| System | Families | What it covers |
|---|---:|---|
| Retained paint and effects | 42 | Every filter and backdrop-filter, the whole mask family, blend modes, true subtree `opacity`, `text-shadow`, `rotate` / `scale` / `skew` / `transform-origin`, `animation` and `@keyframes`. All of it needs a retained paint path that does not exist. |
| Shaped text and OpenType | 19 | `text-decoration` in all four of its properties, `font-feature-settings`, `font-variant-numeric`, `font-stretch`, `font-style`, `text-transform`, `text-indent`, `text-overflow`, `line-clamp`, `hyphens`, `word-break`, `overflow-wrap`, `word-spacing`, `tab-size`. |
| Grid and richer flex | 12 | Grid entirely, plus `flex-basis`, `order`, the weighted shrink and grow factors (`shrink-2`, `grow-2`: the engine's model is boolean, so a weight is refused rather than silently lowered), and the per-child alignment properties `justify-self` and `place-self`. |
| Richer paint | 11 | Radial and conic gradients, gradient stop positions and interpolation, and the `background-*` positioning family: `repeat`, `size`, `position`, `clip`, `origin`, `attachment`. |
| Scrolling and scrollbars | 7 | The entire scroll-snap family, `scroll-margin`, `scroll-padding`, `scrollbar-width` and `scrollbar-gutter`. The scrollbar is a fixed 8px overlay that reserves no gutter. |
| Outlines and border styles | 6 | `border-style` and `divide-style` (a border is a solid line), `outline` and its offset and style, and `ring-offset`. |
| Accessibility | 2 | There is no accessibility tree and no screen-reader surface, so `sr-only` has nothing to preserve. This is worth knowing before you choose RayClay for a product with an accessibility requirement. |
| Long tail | 10 | Singletons across layout, tables, interactivity and CSS value types: `margin`, `float`, `columns`, `visibility`, `object-position`, `border-collapse`, `border-spacing`, `touch-action`, `field-sizing`, pseudo-elements. Responsive variants and the spacing and container scales are not in this list: all three ship as `partial`, and the two paragraphs below are the contract. |

The Families column sums to exactly the 109, so the partition is checkable rather than descriptive.

Three of those deserve a direct answer rather than a table row:

**`margin` is `none` and that is deliberate.** Use `.gap` on the parent, or `rcMargin` for a
one-off spacer. A gap composes and cannot collapse; CSS margin collapsing is a rule most developers
have to learn twice.

**`user-select` is the one family where a Tailwind utility has an EXACT counterpart, and there are
two of them.** Text selection is on by default, as it is in a browser: `select-auto` and
`select-none` mean precisely what they mean in CSS, which is why this family grades `partial`
rather than `none` and why two of its rows are the only `exact` rows on the page.

| Tailwind | RayClay |
|---|---|
| `select-auto` | `.select = RC_SELECT_AUTO` - the default, so every call already has it |
| `select-none` | `.select = RC_SELECT_NONE` - chrome: buttons, menus, tabs, the title bar |
| `select-text` | `.select = RC_SELECT_TEXT` - on a text run same as `AUTO`; on a BUTTON the spelling that selects |
| `select-all` | no counterpart |

**A bare `.select = true` is `select-text` and a bare `.select = false` is `select-auto`**, because
`RC_SELECT_TEXT` is `1` and `RC_SELECT_AUTO` is `0`. The enum is ordered that way on purpose so the
spelling a consumer reaches for first is the one that reads correctly; see api-notes for why `false`
means "this widget's default" rather than "off". `rcSelectable(bool)` is the explicit one-argument
form, and it resolves to `TEXT` or to `NONE` - never to `AUTO` - so it can turn a text run off where
a bare `false` cannot. **Two limits, both measured rather than inferred, because a translation table
is the wrong place to learn them the hard way:**

- **There is no SCOPE.** CSS's `user-select: text` inside a `user-select: none` subtree re-enables
  selection for that subtree. RayClay has no container-level `.select`, so nothing inherits and
  nothing overrides. **On a TEXT RUN `RC_SELECT_TEXT` and `RC_SELECT_AUTO` do the same thing** - the
  only behavioural reading of the mode there is "is it `RC_SELECT_NONE`" - so spell `TEXT` to
  document intent, not to override anything. **On a BUTTON they differ, and that is the one place
  `TEXT` is load-bearing:** a button label's unset `.select` resolves to `NONE`, so `TEXT` (or a
  bare `true`) is what makes a label selectable.
- **A selection spans RUNS, not just lines.** A drag wraps onto the next visual line of the same text
  element and carries on into the next element, so a paragraph selects as a paragraph and a column of
  labels selects as a column; `RC_SELECT_NONE` chrome is skipped. Primary modifier + A takes every run
  in the window. Copy (primary modifier + C) takes up to 4,095 bytes and stops on a codepoint boundary.

**The class axis is `unsupported` for all four, and that is measured rather than assumed**:
`select-auto`, `select-text`, `select-none` and `select-all` are each REFUSED by the class parser,
against a `p-4` control that resolves in the same run. This is a typed-field route only.

**Responsive variants ship, and they are the only variants.** `sm:` `md:` `lg:` `xl:` `2xl:`
before any utility in a class string, on a box or a text run, at Tailwind's own values (640 / 768 /
1024 / 1280 / 1536) and mobile-first as Tailwind's `min-width` queries are: `md:p-4` applies while
the window's *layout* width is at least 768. They apply after the base utilities in rank order,
whatever order they were written in - the order Tailwind's stylesheet produces - so `md:p-4 p-2`
and `p-2 md:p-4` are both 16 at `md`. One prefix per token; a stacked prefix, a prefix outside the
five (`hover:`, `dark:`, `max-md:`, `pointer-coarse:`, `ios:`) or a variant on an `rcDefineClass`
name warns once and is skipped, while a definition's body may carry variants. State variants have
no cascade to select in and stay C (`rcIsHovered`), and nothing is keyed on the operating system by
design. The same band is a value in C, `rcViewport().breakpoint`, for a branch a string cannot
express; both read the *layout* width, never the window width, and the idioms are in
[`for-web-developers.md` > Breakpoints](for-web-developers.md#breakpoints-measure-the-layout-width-not-the-window-width).

**THE WORKED CASE IS `examples/ex22_docs_reader`, AND WHICH VALUES IT SPELLS THIS WAY IS THE LESSON.**
Its article column carries `pt-5 sm:pt-7 lg:pt-10 pb-12 sm:pb-16` - reading rhythm that steps with
the size of the screen - and **nothing else in that app uses a prefix**, because nothing else in it
is a judgement about the screen. The reading column is `TARGET_CHARS` of the body face as that face
actually measures, and the contents rail folds when keeping it would take the column under that:
those are questions about the TYPE, and a class string cannot ask a font how wide its characters
are. Reach for a variant when the honest input is a device band, and stay in C when it is your
content. There is a third case that looks like the first and is not: the same app's header padding
is a plain constant, because its own C arithmetic adds it into the budget that decides whether the
toolbar stacks - **a number your code reads cannot live only in a class string.**

Measured rather than asserted, and you can repeat it in ten lines: put two boxes side by side, one
with `pt-5 sm:pt-7 lg:pt-10` and one with a plain `pt-7`, give each a fixed-height child, and read
both back with `rcGetElementBox` at three window widths. The prefixed box resolves to 20 px of top
padding at a layout width of 500, 28 at 700 and 40 at 1200; the unprefixed one holds 28 at all
three, which is what separates "the prefix did nothing" from "the token never parsed"; and
`3xl:pt-99` logs one `unparsable class variant` line and leaves the base token applied.
The tables above are measured in the 2026-09-19 census, which is after this landed, so the five prefixes are counted in them. The pinned SEMANTIC grade for the family is still `different-model`, which is the grade's own lag and not a second measurement.

## The escape hatch, and the one thing it does not promise

Six families are marked `escape-hatch`. `RC_ElementDeclaration` is a pass-through typedef of
Clay's own declaration struct, and `rcBeginComponent(options, defaults)` forwards the whole thing.
**The fold never writes a field the `rc` DSL has no spelling for**, so those fields arrive at the
layout engine exactly as your `defaults` left them. The `transition` struct appears nowhere in the
fold, which is precisely why the `transition-*` families are reachable without a DSL spelling.

**`minMax` and `aspectRatio` are NOT on that list, and do not reach for the escape hatch to get
them.** Both have first-class spellings - the `.wMin` / `.wMax` / `.hMin` / `.hMax` and
`.aspectRatio` fields of `RC_ComponentOptions` - and the fold writes them. Spell them as fields. That covers the nine
sizing families and `aspect-ratio` - ten of the families this section would otherwise be about.

**And the clamps are the ONE PLACE the composition warning below does not apply**, which is worth
knowing before you reach past them. The fold merges them PER SIDE rather than assigning the pair:
it writes the minimum only when a minimum was spelled and the
maximum only when a maximum was spelled, so an
unspelled side genuinely leaves whatever was there. `0` means unset on the typed fields, which is why `.wMin = 0` and
`.wMax = 0` spell nothing. The class string is the other way round - presence there is lexical - so
`min-w-0` and `min-w-auto` DO write a zero floor, and clear one an earlier token or a `defaults`
declaration set. A zero CEILING has no spelling on either route: `max-w-0` is refused, because a
maximum of 0 already means unbounded. A FIXED axis is the exception inside the exception: it carries its size in BOTH
min and max, so the fold clamps the VALUE and keeps the axis exact rather than writing one side and
silently turning a 100 px box into a range.

**What it does not promise is composition, and this is the sharp edge.** Where the DSL *does* spell a
property, it assigns the whole underlying struct rather than merging into it. Sizing is the case that
will bite you, because it is the group this page recommends:

```c
RC_ElementDeclaration card = { .layout.sizing.width = CLAY_SIZING_GROW(0, 200) };  /* max 200 */

rcComponent(card, .id = "a")                {}   /* 200 - the clamp holds   */
rcComponent(card, .id = "b", .w = "grow")   {}   /* 800 - the clamp is gone */
rcComponent(card, .id = "c", .wType = RC_GROW) {} /* 800 - likewise         */
```

Measured, in an 800 px parent, with an unclamped `grow` in the same run reading 800: arms `b` and `c`
land exactly on the unclamped width. `.w` and `.wType` both replace the entire `Clay_SizingAxis`, and
a `Clay_SizingAxis` carries `minMax` inside it, so the clamp goes with the rest. Nothing warns.
`.scroll` and `.overflow` assign the clip config the same way, so a `defaults`-carried
`.clip.childOffset` does not survive either.

**So the rule is: a clamp survives on an axis you leave unspelled, and is discarded on an axis you
spell.** Spell the size on the `defaults` too, or leave the axis out of the options entirely. Those
are the two shapes that work.

**One thing the fold gets right, and it is worth knowing which half you are in.** Every
*string*-valued property tests whether you spelled it: `.align`, `.scroll`, `.overflow` and
`.borderRadius` check for a non-empty first byte, and `.w` / `.h` check for a non-null, non-empty
pointer. Every *numeric* one tests truthiness instead, so it cannot tell a spelled zero from an
omitted field. That is the whole of the `.p = 12, .pl = 0` problem above, stated structurally.

**`escape-hatch` describes the type system, not the behaviour.** Five of the six are the
`transition` family, and RayClay's default runner lays out on demand rather than every frame. A
transition advances only on a frame that actually runs a layout, so under the default it starts and
then freezes mid-curve. The field is reachable; the animation is not. Treat those five as `none`
when you are planning, and read the `transition` row of the per-field map before you spend time on
them.

The sixth is `arbitrary-properties`, Tailwind's own escape hatch, which has no `rc` spelling by
design: a declaration field is where you go instead.

**`divide` is not an escape hatch, and it carries a trap worth knowing:** `divide-<colour>` and
`divide-x` / `divide-y` are ordinary class utilities graded `partial` above. The trap is that
**RayClay has one border colour per element and the divider shares it**, so `divide-<colour>` and
`border-<colour>` write the same channel and the last one in the string wins, rather than the two
composing the way Tailwind's separate custom properties do.

**Where that one colour comes from when the string never names one.** A width alone (`border`,
`border-2`, `divide-y`) sets no colour, so the resolver fills it from the active preset's `border`.
Precedence, highest first: a typed `.border = { .color = ... }` on the element, then
`border-<colour>` or `divide-<colour>` from the class, then the preset. The typed field outranks
the class default because it is the only one of the three that can spell a DELIBERATELY
transparent edge (`.color = rcAlpha(x, 0)` reserves the space without drawing a line); the class
grammar has no spelling for that, so inside a class string there is no ambiguity to resolve.

## Could RayClay read a `.css` file directly?

A fair question to ask of a page like this, and the honest answer is **a useful subset, without the
cascade, and it would not be CSS**. This section draws that boundary so nobody starts at the wrong end.

**The map is not a 1:1 translation and cannot become one.** Zero families are a complete match, 109
are `none`, and three of the gaps are properties of the layout engine rather than unfinished work.
A file that parsed successfully would still lay out differently from a browser, which is a worse
outcome than refusing it: a silent visual difference is the hardest kind of bug to attribute.

**The selector engine is the structural blocker, and it is not an effort problem.** A CSS rule is a
*query against a retained tree*: `.card > p:nth-child(2)` has to find elements, know their parents,
know their sibling index, and re-answer as the tree changes. RayClay is immediate mode. There is no
retained tree to query - the element tree exists for the duration of one layout pass and is rebuilt
from your C on the next frame. Matching selectors would mean building and keeping the structure the
library exists to avoid, and paying for it every frame. The cascade sits on top of that: specificity,
source order, inheritance and `!important` are all rules about *which of several matching rules wins*,
and there are no several matching rules without a matcher.

**Footprint says the same thing from the other direction.** The nearest in-house comparison is
RayClay's own SVG parser: **57,429 bytes of `.text`** against **751,835 bytes** for the whole
library - **about 7.6% of RayClay to parse 21 attributes** with no selectors, no cascade and no
inheritance. That figure flatters CSS, because some of it is arc and curve geometry a stylesheet has
no use for; it also *under*-states the target, because CSS's value grammar is far wider than those
21 attributes and none of the selector or cascade machinery is in it at all. Either way the order of
magnitude is clear, and footprint is a first-class tenet here.

> **Both figures, and the ratio in the box below, re-measured 2026-09-17.**
> `cmake -DCMAKE_BUILD_TYPE=Release -DRC_BUILD_TESTS=OFF -DRC_BUILD_EXAMPLES=OFF`, gcc 15.3.1,
> x86-64 Linux, GLFW host unless the row says otherwise. The numerator is `.text` of
> the SVG parser's object file; the denominator is `.text` summed over every object in
> `librayclay.a` (`size -t`).
> **THE BASIS IS THE FIGURE.** An earlier run of this paragraph read 10% from the same numerator
> over a smaller denominator, and neither number said how the denominator was taken - which is the
> whole reason the ratio moved without the library doing so.

The denominator above is RayClay's *own* library, not a shipped binary: a binary carries the window
host's code too, and that is much the larger number.

> **A size figure names the window host.** Two hosts are selectable (`RC_WINDOW_BACKEND`: GLFW is
> the desktop default, SDL3 the option and the mobile host). Measured in the same pass as the
> figures above - two full Release builds from one revision, differing only in that flag - the library's
> own `.text` is **751,835 bytes on GLFW and 770,820 on SDL3, a difference of 18,985 bytes or
> 2.53%**, with SDL3 the larger. A delivered BINARY differs by far more than that, because it
> carries the host itself. So say which host a figure was taken on, and whether it is the library or
> the delivered binary, because those answers move independently.
>
> **AND THE PART WORTH KEEPING WHEN THE ABSOLUTES ROT:** an earlier run of this paragraph, on an
> unrecorded basis, read **2.61%** for the same comparison against absolutes about 200 KB smaller.
> The absolutes did not survive a change of basis and **the ratio did, to within 0.08 points.**
> That is this page's own rule about directions and magnitudes, demonstrated on its own numbers.

**What is genuinely reachable is a flat utility-class table, and that is a different and better
thing.** A class string is a set of tokens: no selectors, no specificity, no inheritance, no document
order. Resolving one is a lookup, and a lookup needs none of the machinery above. It is also what the
majority of real Tailwind usage already is, which is why the question feels close to answered.

So the honest shape of the answer:

| Ask | Verdict |
|---|---|
| Read a stylesheet and render it like a browser | **No**, and not with more effort - it needs a retained paint and layout path RayClay does not have |
| Match CSS selectors against the element tree | **No** - there is no retained tree to match against |
| Honour the cascade, specificity and inheritance | **No** - these are rules for arbitrating between matched selectors |
| Resolve a flat set of utility tokens to element options | **Yes**, and cheaply |
| Accept a *subset* of declarations from a flat rule block | **Possible**, bounded by this page's own coverage: no `grid`, no reverse-direction SPELLING, no `w-1/2 min-w-64`, and no filter, mask, blend or transform family |

The last row is the one worth thinking hardest about, because it is the one that *looks* like a win.
A parser that silently drops every declaration this page marks `none` would accept a real stylesheet
and render something that is not what the author wrote, with no error and no diff. If that route is
ever taken, the parser should **refuse** what it cannot honour rather than skip it.

## Where these numbers come from

> **The two axes have two different pins, and conflating them is the mistake this note exists to
> prevent.** The TYPED axis - can RayClay reach this at all, by any route - is the map file's
> `status`, 299 families and 1,187 value rows, every row carrying a verdict. **That file is pinned
> at RayClay 0.9.8, verified 2026-09-11, and the library has not stood still since.** So every
> typed figure on this page is a FLOOR, not a current reading - a row graded `none` may have been
> served since, and none has been re-graded downward. Treat any typed number here as a lower
> bound rather than as today's reading. The CLASS axis - does
> the Tailwind spelling itself resolve - is not read from the file at all: **every spelling is run
> through the engine, one process per token, and the verdict is what the engine did.** That census
> was last re-run on **2026-09-19**, one process per spelling because the
> engine's bad-token warning dedupes within a process, and with a known-good token in the same run
> as a control - so a column of refusals cannot be a broken harness reading as a result. Every
> number on this page is computed from one of those two sources, so the tables agree with each
> other.
> **A verdict is invalidated by a feature landing, not by a version number moving.** The engine runs
> ahead of the pinned file, and the rows where they part are called out where they matter; where a row decides
> something you are building, check the field itself rather than this page.

The map behind this page covers **all 184 property-family pages in the Tailwind core-utility
sidebar**, with zero omissions, and expands to 299 entries because it separates logical from
physical subfamilies and records CSS value-system and Tailwind mechanics rules alongside the
properties.

It is not a catalogue of the whole CSS platform. There are 629 non-internal CSS properties; Tailwind
exposes a deliberately chosen subset, and Tailwind's arbitrary-property escape can spell others
without those becoming named utility families.

Two boundaries on what this page asserts:

- **The RayClay column is a verdict about a specific build.** A status is invalidated by a feature
  landing, not by a version number moving. Where a row matters to a decision you are making, check
  the field itself.
- **Coverage is complete; agreement with a browser is not.** Family verdicts are 299 of 299 and
  value rows are 1,187 of 1,187, so no figure on this page rests on a reduced denominator any more.
  What a complete verdict does *not* give you is pixel agreement: a `partial` row means the spelling
  exists and lands in range, never that RayClay and Blink render it identically.
