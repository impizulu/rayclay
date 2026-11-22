/*
    pane.h - the settings row: the part of this app to copy into yours

    A settings page is a column of cards, a card is a column of rows, and every row is
    the same thing: a name and a line of explanation on the left, one control on the
    right. A row is one call:

        pane_section("Display");
        pane_card() {
            pane_toggle(app, "inline_title", "Inline title", "Show the title in the note",
                        &set->inlineTitle);
            pane_rule();
            pane_select("properties", "Properties", "...", &set->properties, ITEMS, 2);
        }

    The controls write straight into your struct and return true on the frame they
    change it. In a narrow pane each row moves its control under the words by itself.
*/
#ifndef APP_PANE_H
#define APP_PANE_H

#include "rayclay.h"
#include "app/theme.h"

/* The scrolling column every page draws into. */
#define PANE_ID "pane"

/* THE FRAME SETS THE PANE'S WIDTH BEFORE A PAGE DRAWS, and that width decides when rows
   stack. The pane's own box is the last layout's, and on the frame a page is pushed there
   is none: a row reading it would draw wide for that one frame. */
static float pane_width;

static inline bool pane_is_narrow(void) {
    return pane_width < (float)ROW_STACK_W;
}

/* A heading above a card, in line with the text in the card. RayClay has no margin: the air
   above it is its own padding. */
static inline void pane_section(const char *title) {
    rcBox(.pt = 12, .pl = 16, .w = "grow") {
        rcTextC(title, .font = rcFontSemibold((float)T_HEAD), .color = rcGetStyle().text,
                .select = RC_SELECT_NONE);
    }
}

/* A card's padding and a row's add up to the 16 px that text sits in from the card's edge.
   The row holds half of it, so a lit row keeps an edge clear of the card's corners. A card is
   a step from the page toward a surface: the surface at 72% over the page. */
#define pane_card() rcColumn(.px = 8, .py = 4, .w = "grow", .className = "card bg-surface/72")

/* The hairline between two rows of a card, as wide as their text. */
static inline void pane_rule(void) {
    rcBox(.px = 8, .w = "grow") {
        rcBox(.bg = rcGetStyle().border, .w = "grow", .hType = RC_PX(1)) {}
    }
}

/* The row's left half. @p error, when set, takes the place of the line. */
static inline void pane_words(const char *name, const char *line, const char *error) {
    RC_Style s = rcGetStyle();

    rcColumn(.gap = 2, .w = "grow", .className = "items-stretch") {
        rcTextC(name, .color = s.text, .size = T_NAME, .select = RC_SELECT_NONE);
        if (error)
            rcTextC(error, .color = s.danger, .size = T_HINT, .select = RC_SELECT_NONE);
        else if (line && line[0])
            rcTextC(line, .color = s.textMuted, .size = T_HINT, .select = RC_SELECT_NONE);
    }
}

/* A row that keeps its control beside the words at every width: a button. */
#define pane_row() \
    rcRow(.gap = 16, .px = 8, .py = 12, .align = "cl", .w = "grow", .hMin = RC_VAL(56.0f))

/* A row whose control takes the row's width in a narrow pane. */
#define pane_row_stacking(narrow)                                                              \
    rcBox(.gap = (float)((narrow) ? 8 : 16), .px = 8, .py = 12, .w = "grow",                \
          .hMin = RC_VAL(56.0f),                                                               \
          .className = (narrow) ? "flex-col items-stretch" : "flex-row items-center")

/* The whole row flips the switch and lights up to say so, which a finger on a phone needs:
   a press goes to the innermost polled element, so a press on the switch itself flips it
   once. The switch is the keyboard's stop, so the row is not one too. */
static inline bool pane_toggle(RC_App *app, const char *id, const char *name, const char *line,
                               bool *value) {
    const char *row = rcFormat(rcAppArena(app), "row_%s", id).chars;
    bool        before = *value;

    rcRow(.id = row, .gap = 16, .px = 8, .py = 12, .align = "cl", .w = "grow",
          .hMin = RC_VAL(56.0f), .className = "item lit-row") {
        pane_words(name, line, NULL);
        rcToggle(id, "", value);
    }
    if (rcClicked(row)) *value = !*value;
    return *value != before;
}

static inline bool pane_select(const char *id, const char *name, const char *line, int *selected,
                               const char *const *items, int count) {
    bool narrow  = pane_is_narrow();
    bool changed = false;

    pane_row_stacking(narrow) {
        pane_words(name, line, NULL);
        rcBox(.wType = narrow ? RC_GROW : RC_PX((float)SELECT_W)) {
            changed = rcCombo(id, selected, items, count, .className = control_class());
        }
    }
    return changed;
}

/* @p value_text is the number as the user reads it ("16 px"), which a slider never shows. */
static inline bool pane_slider(const char *id, const char *name, const char *line, float *value,
                               float min, float max, float step, RC_String value_text,
                               bool disabled) {
    bool narrow  = pane_is_narrow();
    bool changed = false;

    pane_row_stacking(narrow) {
        pane_words(name, line, NULL);
        rcRow(.gap = 12, .align = "cr", .wType = narrow ? RC_GROW : RC_FIT) {
            rcBox(.wType = narrow ? RC_GROW : RC_PX((float)SLIDER_W)) {
                changed = rcSlider(id, value, min, max, .disabled = disabled, .step = step);
            }
            rcBox(.align = "cr", .wType = RC_PX((float)SLIDER_VALUE_W)) {
                rcText(value_text, .color = rcGetStyle().textMuted, .size = T_BODY,
                       .select = RC_SELECT_NONE);
            }
        }
    }
    return changed;
}

static inline bool pane_button(const char *id, const char *name, const char *line,
                               const char *label, RC_ButtonVariant variant) {
    bool clicked = false;

    pane_row() {
        pane_words(name, line, NULL);
        clicked = rcButton(id, label, .className = control_class(), .variant = variant);
    }
    return clicked;
}

/* A text field in a row. @p error is the verdict on what it holds, NULL when it is fine. */
static inline bool pane_text(const char *id, const char *name, const char *line, char *buffer,
                             int capacity, const char *placeholder, RC_InputKind kind,
                             const char *error) {
    bool narrow  = pane_is_narrow();
    bool changed = false;

    pane_row_stacking(narrow) {
        pane_words(name, line, error);
        rcBox(.wType = narrow ? RC_GROW : RC_PX((float)FIELD_W)) {
            changed = rcTextInput(id, buffer, capacity, .className = control_class(),
                                  .placeholder = placeholder, .inputKind = kind,
                                  .clearButton = true);
        }
    }
    return changed;
}

/* A square ghost button holding one glyph. A box polled with rcClicked is a button to the
   pointer; .focusable makes it one to the keyboard too: a Tab stop that Enter and Space click,
   with the focus ring around it. Every such box in this app declares it but a settings row,
   whose switch is its stop. The box lights by its class; the glyph is drawn in a color and has
   no state of its own, so it asks rcShowsHover. */
static inline float icon_button_size(void) {
    return rcPointerIsCoarse() ? 44.0f : 28.0f;
}

static inline bool icon_button(const char *id, RC_IconCallback icon, const char *tip) {
    RC_Style s   = rcGetStyle();
    bool     hit = rcClicked(id);
    float    box = icon_button_size();

    rcBox(.id = id, .focusable = true, .align = "cc", .wType = RC_PX(box), .hType = RC_PX(box),
          .tooltip = tip, .className = "item lit") {
        icon(16.0f, rcShowsHover(id) ? s.text : s.textMuted);
    }
    return hit;
}

#endif /* APP_PANE_H */
