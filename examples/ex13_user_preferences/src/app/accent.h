/*
    accent.h - the color picker: swatches, then a square, a hue strip and a hex field

    RayClay has no color control, so this one is built from what it does have: a box
    takes a two-color gradient, and rcPressed, rcPointer and rcGetElementBox turn a drag
    into a value. The square is white-to-hue across and clear-to-black down, as every
    HSV picker is. Whatever it writes lands in Settings.accent, and update() installs
    that at the top of the next frame, so the whole window recolors under the pointer.
*/
#ifndef APP_ACCENT_H
#define APP_ACCENT_H

#include "rayclay.h"
#include "app/pane.h"
#include "app/state.h"

#include "icons/rc_icons_check.h"
#include "icons/rc_icons_chevron_down.h"
#include "icons/rc_icons_chevron_up.h"

/* The swatches. The ids are a table, because an id must outlive the frame. */
static const struct {
    const char *id;
    const char *name;
    uint32_t    rgb;
} ACCENT_SWATCH[] = {
    { "accent_iris",  "Iris",  ACCENT_DEFAULT }, { "accent_blue",  "Blue",  0x3b82f6u },
    { "accent_teal",  "Teal",  0x14b8a6u },      { "accent_green", "Green", 0x22c55eu },
    { "accent_amber", "Amber", 0xf59e0bu },      { "accent_red",   "Red",   0xef4444u },
    { "accent_pink",  "Pink",  0xec4899u },      { "accent_slate", "Slate", 0x94a3b8u },
};
enum { ACCENT_SWATCHES = (int)(sizeof ACCENT_SWATCH / sizeof ACCENT_SWATCH[0]) };

enum { PICK_H = 148, HUE_H = 14, KNOB_D = 16 };

static inline float clamp01(float v) {
    return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}

static inline uint32_t rgb_pack(float r, float g, float b) {
    return (uint32_t)(r * 255.0f + 0.5f) << 16 | (uint32_t)(g * 255.0f + 0.5f) << 8 |
           (uint32_t)(b * 255.0f + 0.5f);
}

static inline uint32_t hsv_to_rgb(float h, float s, float v) {
    float f = h * 6.0f;
    int   i = (int)f;
    float t = f - (float)i;
    float p = v * (1.0f - s), q = v * (1.0f - s * t), u = v * (1.0f - s * (1.0f - t));

    switch (i % 6) {
    case 0:  return rgb_pack(v, u, p);
    case 1:  return rgb_pack(q, v, p);
    case 2:  return rgb_pack(p, v, u);
    case 3:  return rgb_pack(p, q, v);
    case 4:  return rgb_pack(u, p, v);
    default: return rgb_pack(v, p, q);
    }
}

/* Hue is left alone where the color has none, so gray does not snap the strip to red. */
static inline void rgb_to_hsv(uint32_t rgb, float *h, float *s, float *v) {
    float r = (float)(rgb >> 16 & 0xffu) / 255.0f;
    float g = (float)(rgb >> 8 & 0xffu) / 255.0f;
    float b = (float)(rgb & 0xffu) / 255.0f;
    float hi = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float lo = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float d  = hi - lo;

    *v = hi;
    *s = hi > 0.0f ? d / hi : 0.0f;
    if (d <= 0.0f) return;
    if (hi == r)      *h = (g - b) / d;
    else if (hi == g) *h = 2.0f + (b - r) / d;
    else              *h = 4.0f + (r - g) / d;
    *h /= 6.0f;
    if (*h < 0.0f) *h += 1.0f;
}

static inline int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* "#rrggbb" or "rrggbb"; false for anything else, so half a typed color changes nothing. */
static inline bool hex_parse(const char *text, uint32_t *rgb) {
    uint32_t v = 0;
    int      i;

    if (*text == '#') text++;
    for (i = 0; i < 6; i++) {
        int d = hex_digit(text[i]);
        if (d < 0) return false;
        v = v << 4 | (uint32_t)d;
    }
    if (text[6]) return false;
    *rgb = v;
    return true;
}

/* A white ring around the color it stands on, with a shadow to lift it off any color. Its z
   of 2 is under a combo's list; in the dialog the library counts that z from the panel up. */
static inline void picker_knob(RC_Color fill, float x, float y) {
    rcBox(.bg = fill, .borderRadius = "all-full", .border = { .color = RC_WHITE, .width = 2 },
          .wType = RC_PX((float)KNOB_D), .hType = RC_PX((float)KNOB_D),
          .shadow = { .color = rcRgb(0, 0, 0, 160 / 255.0f), .blur = 3.0f },
          .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                        .element = RC_ANCHOR_CENTER, .offset = { x, y },
                        .zIndex = 2, .capture = RC_CAPTURE_PASSTHROUGH }) {}
}

/* The square and the strip. A drag ends where the button comes up, wherever that is:
   the pointer may leave the box and keep steering, as a browser's picker does. A picker
   the last frame did not draw (Esc or a hotkey closed Settings mid-drag) saw no button
   come up, so its drag is over too, and a later press elsewhere does not steer it. The
   pane scrolls, so both boxes keep a finger (.captureTouch) or its drag would pan it. The
   button is read only while a drag runs: a read that answers true asks for another frame,
   and a finger resting anywhere else would keep the window drawing. */
static inline void picker_drag(RC_App *app, AppState *st) {
    AccentPicker *pk = &st->picker;
    RC_Box        sq = rcGetElementBox("accent_square");
    RC_Box        hu = rcGetElementBox("accent_hue");

    if (rcPressed("accent_square")) pk->drag = 1;
    if (rcPressed("accent_hue"))    pk->drag = 2;
    if (pk->drag && (!rcPointerDown(RC_POINTER_LEFT) || !sq.found)) pk->drag = 0;
    RC_Vec2 p = pk->drag ? rcPointer() : RC_LIT(RC_Vec2){ 0 };
    if (pk->drag == 1 && sq.width > 0.0f && sq.height > 0.0f) {
        pk->sat = clamp01((p.x - sq.x) / sq.width);
        pk->val = 1.0f - clamp01((p.y - sq.y) / sq.height);
    } else if (pk->drag == 2 && hu.width > 0.0f) {
        pk->hue = clamp01((p.x - hu.x) / hu.width);
    }
    if (pk->drag) {
        st->set.accent = hsv_to_rgb(pk->hue, pk->sat, pk->val);
        pk->seen = st->set.accent;
        rcStrCopy(pk->hex, rcFormat(rcAppArena(app), "#%06x", st->set.accent).chars, HEX_CAP);
    }
}

/* What the picker opens to: the square, the strip, the hex field. */
static inline void picker_body(RC_App *app, AppState *st) {
    static const uint32_t STOPS[7] = {
        0xff0000u, 0xffff00u, 0x00ff00u, 0x00ffffu, 0x0000ffu, 0xff00ffu, 0xff0000u
    };
    RC_Style      s  = rcGetStyle();
    AccentPicker *pk = &st->picker;
    RC_Box        sq = rcGetElementBox("accent_square");
    RC_Box        hu = rcGetElementBox("accent_hue");
    uint32_t      typed;
    int           i;

    picker_drag(app, st);
    rcColumn(.gap = 12, .pb = 16, .px = 8, .w = "grow") {
        rcBox(.id = "accent_square", .captureTouch = RC_TOUCH_AXIS_BOTH, .w = "grow",
              .hType = RC_PX((float)PICK_H),
              .gradient = { .from = RC_WHITE, .to = rcHex(hsv_to_rgb(pk->hue, 1.0f, 1.0f)),
                            .dir = "h" }, .className = "item") {
            rcBox(.w = "grow", .h = "grow",
                  .gradient = { .from = rcRgb(0, 0, 0, 0), .to = RC_BLACK, .dir = "v" },
                  .className = "item") {}
            picker_knob(rcHex(st->set.accent), pk->sat * sq.width, (1.0f - pk->val) * sq.height);
        }
        /* A box takes two colors, so the rainbow is six boxes, and the end ones round the
           strip's ends. */
        rcBox(.id = "accent_hue", .captureTouch = RC_TOUCH_AXIS_X, .w = "grow",
              .hType = RC_PX((float)HUE_H)) {
            rcRow(.w = "grow", .h = "grow") {
                for (i = 0; i < 6; i++)
                    rcBox(.w = "grow", .h = "grow",
                          .gradient = { .from = rcHex(STOPS[i]), .to = rcHex(STOPS[i + 1]),
                                        .dir = "h" },
                          .className = i == 0 ? "rounded-l-full"
                                     : i == 5 ? "rounded-r-full" : NULL) {}
            }
            picker_knob(rcHex(hsv_to_rgb(pk->hue, 1.0f, 1.0f)), pk->hue * hu.width,
                        (float)HUE_H * 0.5f);
        }
        rcRow(.gap = 12, .align = "cl", .w = "grow") {
            rcTextL("Hex", .color = s.textMuted, .size = T_HINT, .select = RC_SELECT_NONE);
            rcBox(.wType = RC_PX(140.0f)) {
                if (rcTextInput("accent_hex", pk->hex, HEX_CAP, .className = control_class(),
                                .placeholder = "#8a5cf5", .clearButton = true)) {
                    /* The x leaves focus where it was, and picker_sync() refills a field
                       out of focus: emptied, it takes focus so the new color can be typed. */
                    if (!pk->hex[0]) rcSetFocus("accent_hex");
                    else if (hex_parse(pk->hex, &typed)) st->set.accent = typed;
                }
            }
            rcBox(.w = "grow") {
                rcText(rcFormat(rcAppArena(app), "H %d, S %d, V %d", (int)(pk->hue * 360.0f),
                                (int)(pk->sat * 100.0f), (int)(pk->val * 100.0f)),
                       .color = s.textMuted, .size = T_HINT, .select = RC_SELECT_NONE);
            }
        }
    }
}

/* Keep the picker's numbers in step with an accent changed by anything else: a swatch,
   the hex field, Restore defaults. The field keeps what is being typed into it, and once
   it loses focus, half a color goes back to the accent it failed to change. */
static inline void picker_sync(RC_App *app, AppState *st) {
    AccentPicker *pk = &st->picker;
    uint32_t      typed;

    if (pk->seen != st->set.accent || !pk->hex[0]) {
        pk->seen = st->set.accent;
        rgb_to_hsv(st->set.accent, &pk->hue, &pk->sat, &pk->val);
    }
    if (!rcIsFocused("accent_hex") && !(hex_parse(pk->hex, &typed) && typed == st->set.accent))
        rcStrCopy(pk->hex, rcFormat(rcAppArena(app), "#%06x", st->set.accent).chars, HEX_CAP);
}

/* The whole Accent color row: a dot per swatch, then Custom, which opens the picker. A
   finger's dots and Custom need about 520 px beside the words, so under a finger the row
   stacks in a pane narrower than 770, and the dots wrap under the words. */
static inline void pane_accent(RC_App *app, AppState *st, const char *name, const char *line) {
    Palette  pal    = app_palette();
    bool     finger = rcPointerIsCoarse();
    bool     narrow = pane_is_narrow() || (finger && pane_width < 770.0f);
    float    dot    = finger ? 44.0f : 24.0f;
    int      i;

    picker_sync(app, st);
    pane_row_stacking(narrow) {
        pane_words(name, line, NULL);
        rcRow(.gap = 8, .align = "cl", .wType = narrow ? RC_GROW : RC_FIT,
              .className = "flex-wrap") {
            for (i = 0; i < ACCENT_SWATCHES; i++) {
                const char *id = ACCENT_SWATCH[i].id;
                bool        on = st->set.accent == ACCENT_SWATCH[i].rgb;

                if (rcClicked(id)) st->set.accent = ACCENT_SWATCH[i].rgb;
                rcBox(.id = id, .focusable = true, .bg = rcHex(ACCENT_SWATCH[i].rgb),
                      .align = "cc", .borderRadius = "all-full", .wType = RC_PX(dot),
                      .hType = RC_PX(dot), .tooltip = ACCENT_SWATCH[i].name,
                      .className = on ? "border-2 border-foreground"
                                      : "border-2 border-transparent hover:border-foreground") {
                    if (on) rcIconCheck(dot * 0.5f, pal.accentInk);
                }
            }
            if (rcButton("accent_custom", "Custom", .className = control_class(),
                         .leadingIcon = st->picker.open ? rcIconChevronUp : rcIconChevronDown))
                st->picker.open = !st->picker.open;
        }
    }
    if (st->picker.open) picker_body(app, st);
}

#endif /* APP_ACCENT_H */
