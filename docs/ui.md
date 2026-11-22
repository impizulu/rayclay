# Building the UI

The UI is element macros with designated options in their struct's order; [api.md](api.md) lists
every call and field.

## Layout

`rcRow` lays children left to right, `rcColumn` and `rcBox` top to bottom. A box fits its content
until sized; its children sit at its start, unstretched (`items-stretch` stretches them).

- `.w` and `.h`: `"fit"` (default), `"grow"`, `"120"` or `"120px"`, `"50%"` of the parent,
  `"50vw"`, `"8rem"`. Clamps (`.wMin`...) take `RC_VAL(n)` px and bound any axis, `%` too.
- `.align` is two letters, vertical then horizontal: `t` `c` `b`, then `l` `c` `r`. A capital
  `C`, `B` or `R` is CSS's `safe` form, for a scrolling centered pane.
- `.gap` and padding (`.p`, `.px`...) are px. A set size includes border and padding, as Tailwind's
  `border-box`; a fit box grows by both.
- `rcSpacer() {}` grows, pushing later siblings to the far end; one on each side centers a
  child in a row (`mx-auto` centers only in a column).
- `flex-wrap` (a class only) wraps a row once its width is bounded: `"grow"`, `%`, px,
  `max-w-*` or a stretching parent. Its lines pack at the top, where CSS stretches them;
  `content-stretch` is refused: use `content-between` or a fit height. A `flex-wrap-reverse`
  row scrolling vertically opens on its last line, where CSS opens on the first:
  `rcScrollToBottom(id)` shows the first.
- A row's child shrinks no smaller than its content's minimum width, as in CSS: for a title to
  truncate, give each box between the row and the text `min-w-0`. An overfull row with no `%`
  child shrinks its widest child first, where CSS shrinks all proportionally: `shrink-0` holds one.
  A `shrink-0` child also holds an unstretched fit-width row around it at least its own width,
  where CSS lets the child overflow: `overflow-hidden` on the row lets it shrink.
- A column's child keeps its content's height: a pane that should shrink takes `min-h-0`, or
  `overflow-y-auto` to scroll.
- `.floating` with its `.to` set is `position: absolute`. A scroll pane cuts it at its edge: give a
  popup `.clip = RC_CLIP_NONE` and a `.zIndex`. A float declared in another counts its `.zIndex`
  from that one, so give sibling windows their own. It takes the pointer unless
  `.capture = RC_CAPTURE_PASSTHROUGH`. An in-flow `z-*` orders a box only among its siblings: a
  menu rising over the next card must float. A float attached to a scroll pane itself stays put
  and adds no range (CSS's scrolls): attach it to a child.
- Branch on space, never on the OS: `md:` classes and `rcViewport().breakpoint` read the layout's
  width, not the window's.

<!-- compile: c -->
```c
static void inbox(RC_App *app, const char *const *subjects, int count, char *query, int *opened) {
    rcColumn(.w = "grow", .h = "grow") {
        rcRow(.align = "cl", .w = "grow", .className = "gap-2 p-3 bg-surface") {
            rcTextL("Inbox", .className = "text-[22px]");
            rcSpacer() {}
            rcTextInput("search", query, 64, .placeholder = "Search", .clearButton = true);
        }
        rcColumn(.id = "list", .scroll = "v", .w = "grow", .h = "grow") {
            rcVirtualList(row, "list", count, 40) {
                RC_String id = rcFormat(rcAppArena(app), "mail-%d", row.index);
                rcRow(.id = id.chars, .px = 16, .align = "cl", .overflow = "hidden",
                      .w = "grow", .h = "40") {
                    rcTextC(subjects[row.index], .wrap = "e");
                }
                if (rcClicked(id.chars)) *opened = row.index;
            }
        }
        rcScrollbar("list");
    }
}
```

Other differences from CSS:

- In a row without a fixed height, a stretched `aspect-*` box with no width does not stretch to
  the row: fix the row's height or give the box a width.
- A `flex-1` `aspect-*` box whose height comes from its row narrows to keep the ratio, where CSS
  fills the row: drop the ratio.
- In an overfull row, a fixed-height `aspect-*` box with a `min-w-*` shrinks as far as that minimum
  and its height follows the ratio, where CSS keeps the height: add `shrink-0`.
- An overfull row may push a child below its minimum width: give it `shrink-0`.
- Wrapping content can push a stretched `aspect-*` box past its fixed-height row: fix the content's
  height.
- Stretched `aspect-*` boxes in `min-h-*` `max-h-*` rows keep wide content's width: fix the row's
  height.
- Content under a clip cannot grow an `aspect-*` box: add `min-h-*` or drop the clip.
- `-z-*` never paints behind its parent's background: make it the parent's sibling or use `.shadow`.
- When a float extending a scroll pane's range resizes, the range follows one frame late.
- A pane scrolling sideways over clips nested past 64 deep can scroll short: flatten them.
- A squeezed `items-baseline` row lets descenders overflow: give it `shrink-0` or a `min-h-*`.
- An `items-baseline` row's height ignores stretch or growth in a child: fix those heights.
- With a raised `RC_AppOptions.maxLayoutElements`, one parent drops children past 65,535: use
  `rcVirtualList`.
- A tab is one space wide, with no tab stops: use boxes for columns.

## Classes, color and themes

A box's, run's or control's look lives in its `.className`: Tailwind v4 utilities and numbers
(`p-4` is 16 px). Typed fields carry data and behavior and beat a class, but a C zero is unset:
`.p = 0` leaves `p-4`, so write `p-0`; a ternary unsets with `RC_UNSET_VAL` or `RC_UNSET_COLOR`.
The string must live through the frame.

A token takes one responsive prefix, `sm:` to `2xl:`, mobile first, and one state prefix: `hover:`,
`active:`, `focus:`, `focus-visible:`, `disabled:`, and a control's `checked:` and `open:`. On a
control they read its own state, label included (`hover:text-white`); a box's `focus:` needs
`.focusable`. A state token beats one without; a class fill gets no implied hover or press shade. A
text run has no state: color it from `rcShowsHover(id)`.

`accent-<color>` colors a control's mark: a checked box's fill, a radio's dot, a toggle's on track,
a slider's fill. A slider's knob takes `.thumbClassName` (`size-[9px] bg-zinc-800`) and its fill
`.fillClassName` (`bg-transparent` draws none). A color may name an `RC_Style` role in kebab case,
following `rcSetStyle`: `bg-surface`, `bg-primary/20`, `text-muted-foreground` (`text` is
`foreground`, `textMuted` `muted-foreground`).

`rcDefineClass("card", "p-4 gap-2")` is `@apply` for a repeated look, defined at start-up or on a
theme switch, with a literal body. A `.className` over 95 bytes warns: name it. A class color, an
accent aside, stands on a disabled control, so write its `disabled:` look; a disabled
`RC_BTN_PRIMARY` or `RC_BTN_DANGER` fades whole, class fill included. A ring or outline takes the
theme's border color, and `ring-inset` draws the ring and the focus ring inside the border. An
`outline-*` set when focused replaces a control's focus ring and a plain text field's focus border
(`outline-none` hides them); a `ring-*` does not.

`.transition = "background-color .18s ease-out, color 90ms"` eases a box's fields as CSS's shorthand
does; keep it set every frame. Its classes: `transition`, `transition-colors`, `duration-200`,
`delay-100` and `ease-out`; leave them unprefixed. `.opacity` fades a box and everything in it as
one image, as CSS does.

Colors are `RC_Color`: `rcColor("#1e293b")` or the palette (`RC_SLATE_800`, `RC_TRANSPARENT`).

`RC_Style` holds the role colors and the `radius` every control rounds with; there is no `dark:`.
Install `rcStyleDark()` (default), `rcStyleLight()` or your own with `rcSetStyle` before the frame's
first element. To follow the system, read `rcPreferredColorScheme()` in the layout.

## Text, fonts and images

Text utilities (`text-*`, `leading-*`, `tracking-*`, `line-clamp-*`) go on the run, not its box.
`select-*` (CSS `user-select`) goes on a run, a widget or a box, whose runs take it.

- `RC_AppOptions.fontSizes` and `.fontCount` load font slot i at `fontSizes[i]` from `.fontPath`,
  else the bundled face; `rcLoadFont` adds one once the window is open. `rcRegisterFont` and
  `rcFont(family, weight, size)` load and find one by family, one static .ttf or CFF .otf per
  weight.
- Each face and size takes one of 16 slots; past them a new size draws soft and warns once, so
  never animate `.size`.
- The bundled face covers Western European text; color emoji come from the system. For other
  scripts load a font: any loaded font supplies a glyph a run's font lacks.
- Spaces and newlines stay as typed (CSS `white-space: pre-wrap`); a long URL needs
  `.wordBreak = "w"` (`break-words`). `.wrap = "n"` keeps a run on one line, and `"e"` (`truncate`)
  ends it in an ellipsis inside a box that clips. `.textAlign` aligns wrapped lines; center one line
  with the parent's `.align`.

`rcLoadImage` reads PNG, JPG, BMP, GIF and WebP (both animate) up to 50 megapixels (GIF 8.8,
animated WebP 6.6) and 16384 px a side; a NULL `.handle` means it failed. Load once, draw with
`.image = &img` on a sized box (`.imageFit`, `.imageTint`), free with `rcUnloadImage`. A frame draws
up to 1,024 images.

`rcSvg("icons/star.svg", 20, color)` loads on first use and caches by path; `color` paints
`currentColor`. Gradients, text, filters, images and `<style>` sheets are skipped. Prefer
trusted SVG. Parsing truncates past 512 ops, 28,672 points or 64 levels.

## Controls and input

A control takes an id, positional arguments, then options. Its `.className`
beats the look `rcButton`'s `.variant` picks and styles the part CSS reaches (a checkbox's box and
label, a toggle's track); `icon-6` sets an icon's size. `font-semibold` (`-medium`, `-bold` too)
draws a label in the bundled Roboto Semibold; with your own font, use `font-[<id>]`.

<!-- compile: c -->
```c
static bool save_bar(bool *autosave, bool dirty) {
    rcToggle("autosave", "Autosave", autosave, .className = "accent-emerald-600");
    return rcButton("save", "Save", .disabled = !dirty,
                    .className = "bg-emerald-600 hover:bg-emerald-500 active:bg-emerald-700",
                    .variant = RC_BTN_PRIMARY);
}
```

A control returns true the frame its value changes or it is clicked. Every control takes
`.disabled`; on a box it disables everything inside.

- `rcClicked` makes any box with an `.id` a button.
- A `Begin` call (menu, context menu, modal, table) returns true while open or ready; call its
  `End` only then.
- `rcBeginModal(id, &open, .placement = RC_MODAL_BOTTOM)` is a bottom sheet; `.offset` slides
  it (a rise, a drag to dismiss, a slide out under `.exiting`).
- A context menu opens on a right-click or a long press.
- A table fills its parent and keeps its header out of the scroll; `rcTableRow(.id = rowId)` makes
  a row clickable. A column's `.priority` hides it, highest first, when the parent is narrow;
  `rcTableHiddenColumns(id)` says which. A GROW column's `.wMin = RC_VAL(160)` keeps it that wide
  while lesser columns hide.
- A box's `.tooltip = "text"` shows on mouse dwell, never on touch.
- `rcKeyPressed(RC_KEY_S) && rcModDown(RC_MOD_PRIMARY)` is Cmd+S on Apple, Ctrl+S elsewhere and
  either on the web. An Esc that closed a popup or blurred a field reads false. A non-modal panel
  with `.noBackdropDismiss` closes on neither Esc nor a press outside.
- Tab walks the controls, fields and `.focusable` boxes; `rcSetFocus(id)` moves focus. A focused
  control answers its HTML analog's keys, which then read false in `rcKeyPressed`;
  `rcKeyPressedOn(id, key)` still reads them for that stop.
- In an `items-baseline` row a text field aligns by its bottom edge or icon, not its text: use
  `items-center`.

## Scrolling, lists, charts and zoom

The window never scrolls: put long content in a box with `.scroll = "v"`, `"h"` or `"b"`, an `.id`
and a bounded size. Call `rcScrollbar(id)` after the box, on phones too, and any scroll call
(`rcScrollToBottom`, `rcScrollBy`) before it. A finger's fling outlives a scroll call; to end
it, declare `.overflow = "hidden"` instead of `.scroll` for one frame. Under a mouse, or under
every pointer once it takes a `.className` (`bg-*` its track, `accent-<color>` its thumb), the bar
takes a 12 px band inside its edge: do not pad for it. A window scrolls 100 boxes at most, table
bodies and open modals included; more only clip; RayClay warns once. Use `overflow-hidden` to
only clip, `rcVirtualList` for long lists.

`rcVirtualList(row, id, count, rowHeight)` declares only the visible rows. It sits directly in the
scroll box it names, and every row is exactly `rowHeight` tall with an id from its data.
`rcScrollToRow(id, i, align)` reveals a row the list has not declared.

`rcChart` fills its parent unless its `.className` sizes it (`w-*`, `h-*`); that class styles its
card and its text (`font-[<id>]`, `text-<size>`). Its arrays are borrowed until the frame is drawn.
For a hover readout, set `.tooltip = true` and label every series. Its numbers follow the user's
locale on every OS; pin one with `.locale = "de-CH"`.

Desktop zoom (Ctrl or Cmd with +, -, 0 or a wheel) reflows the layout, breakpoints included.
Wrap chrome that keeps its size in `rcUnzoomed() { }`; turn zoom off with
`RC_AppOptions.zoom = { .disabled = true }`.

## CSS to RayClay

| CSS | Field | Class |
|---|---|---|
| `flex-direction` | `rcRow`, `rcColumn` | `flex-row`, `flex-col` |
| `justify-content`, `align-items` | `.align` | `justify-between`, `items-center` |
| `flex: 1` | `.w = "grow"` | `flex-1` (both axes) |
| `min-width`, `max-width` | `.wMin`, `.wMax` | `min-w-0`, `max-w-md` |
| `linear-gradient` | `.gradient` | `bg-linear-to-r from-sky-500 to-indigo-500` |
| `box-shadow` | `.shadow` | `shadow-md`, `ring-2` |
| `outline` | | `outline-2` |
| `opacity` | `.opacity = RC_VAL(0.5)` | `opacity-50` |
