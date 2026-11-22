/*
    parts.h - the small pieces every screen is built from

    Each is a plain function that declares elements. A tappable part returns true on the frame it
    is tapped, as rcButton does, and leaves what that means to the screen that called it.

    WHY THESE ARE NOT rcButton. A plain text button in this app is rcButton: Cancel, Post, Done. The
    parts here are what it does not draw: icons with counts, pills inside a larger finger target
    and tabs with an underline. rcClicked on a styled box is the same button, with the pointer
    cursor and the same press rule, and the box is free to look like anything. .focusable makes it
    the same button to the keyboard: a Tab stop that Enter and Space press, with the focus ring.
*/
#ifndef APP_PARTS_H
#define APP_PARTS_H

#include "rayclay.h"
#include "app/state.h"

#include "icons/rc_icons_arrow_up_down.h"
#include "icons/rc_icons_bell.h"
#include "icons/rc_icons_bell_solid.h"
#include "icons/rc_icons_ban.h"
#include "icons/rc_icons_bookmark.h"
#include "icons/rc_icons_bookmark_filled.h"
#include "icons/rc_icons_check.h"
#include "icons/rc_icons_chevron_right.h"
#include "icons/rc_icons_compass.h"
#include "icons/rc_icons_compass_filled.h"
#include "icons/rc_icons_ellipsis.h"
#include "icons/rc_icons_heart.h"
#include "icons/rc_icons_heart_filled.h"
#include "icons/rc_icons_house.h"
#include "icons/rc_icons_house_filled.h"
#include "icons/rc_icons_inbox.h"
#include "icons/rc_icons_link.h"
#include "icons/rc_icons_message_circle.h"
#include "icons/rc_icons_plus.h"
#include "icons/rc_icons_repeat.h"
#include "icons/rc_icons_search.h"
#include "icons/rc_icons_send_solid.h"
#include "icons/rc_icons_settings.h"
#include "icons/rc_icons_share.h"
#include "icons/rc_icons_user.h"
#include "icons/rc_icons_user_filled.h"
#include "icons/rc_icons_user_plus.h"
#include "icons/rc_icons_users.h"
#include "icons/rc_icons_volume_x.h"
#include "icons/rc_icons_x.h"

/* A border rather than a filled box: a 1 px border is one device pixel at any zoom, where a 1 px
   fill lands on one or two by its position. */
static void hairline(void) {
    rcBox(.w = "grow", .hType = RC_PX(1), .className = "border-t border-border") {}
}

/* nav-press (navigator.h) as a color, in its numbers, for the two places a class cannot reach: a
   pill drawn inside a larger finger target, which a class would light by the pill's own hover
   and not the target's (CSS's group-hover, which RayClay leaves out), and a press over a row's
   tint, as an .overlay, where a class fill would take the tint's place. rcShowsActive is the
   class's active: and, unlike rcPointerDown, asks for no frame while a finger rests on the row. */
static RC_Color press_wash(const char *id) {
    if (rcShowsActive(id))
        return rcWithAlpha(rcGetStyle().text, 0.1f);
    return rcShowsHover(id) ? rcWithAlpha(rcGetStyle().text, 0.05f) : RC_UNSET_COLOR;
}

/* The band between posts: a step down from white, or a step up from true black. */
static void feed_gap(const AppState *st) {
    RC_Style s = rcGetStyle();

    rcBox(.bg = st->dark ? s.surface : s.surfaceAlt, .w = "grow", .hType = RC_PX(FEED_GAP)) {}
}

/* A face in a circle. The person's color stands behind it, so a picture that did not load is still
   somebody's own dot. */
static void avatar(const AppState *st, PersonId who, int size) {
    rcBox(.bg = PERSON[who].from, .borderRadius = "all-full", .wType = RC_PX(size),
          .hType = RC_PX(size), .image = &st->face[who], .imageFit = RC_IMAGE_COVER) {}
}

/* A round icon button: the glyph is drawn at `size`, and the finger's target is always TARGET.
   One that is not `live`, as Send with nothing to send, does nothing and is no Tab stop, as a
   disabled button is not. */
static bool icon_button(const char *id, RC_IconCallback icon, float size, RC_Color ink, bool live) {
    rcBox(.id = id, .focusable = live, .align = "cc", .borderRadius = "all-full",
          .wType = RC_PX(TARGET), .hType = RC_PX(TARGET), .className = "nav-press") {
        icon(size, ink);
    }
    return rcClicked(id) && live;
}

/* A pill that looks 32 tall inside a 44 px target, the way Follow and Edit look on every phone.
   The label takes the page's own ground on the fill, which reads on the accent in both themes.
   The target is the Tab stop; rounded as the pill is, its focus ring takes the pill's shape. */
static bool pill_button(const char *id, const char *label, bool filled) {
    RC_Style       s    = rcGetStyle();
    const RC_Color fill = rcShowsHover(id) ? s.primaryHover : s.primary;

    rcBox(.id = id, .focusable = true, .align = "cc", .borderRadius = "all-full",
          .hType = RC_PX(TARGET)) {
        rcBox(.bg = filled ? fill : press_wash(id), .px = 14, .align = "cc",
              .borderRadius = "all-full",
              .border = { .color = filled ? fill : s.border, .width = 1 }, .hType = RC_PX(32)) {
            rcTextC(label, .font = rcFontSemibold(13), .color = filled ? s.background : s.text,
                    .wrap = "n", .select = RC_SELECT_NONE);
        }
    }
    return rcClicked(id);
}

/* The app's own strip of tabs: equal cells, a short accent bar under the one that is on. Each cell
   is a Tab stop unless `focusable` is false, for a copy of a strip whose cells already are. The
   strip's 4 px at either end are the room the end cells' focus rings need inside a list's clip. */
static bool underline_tabs(RC_Arena *m, const char *prefix, const char *const *labels, int count,
                           int *current, bool focusable) {
    RC_Style s = rcGetStyle();
    bool     changed = false;
    int      i;

    rcRow(.bg = s.background, .px = 4, .w = "grow", .hType = RC_PX(TARGET)) {
        for (i = 0; i < count; i++) {
            const char *id = rcFormat(m, "%s_%d", prefix, i).chars;
            const bool  on = *current == i;

            rcColumn(.id = id, .focusable = focusable, .align = "cc", .w = "grow", .h = "grow",
                     .className = "nav-press") {
                rcBox(.align = "cc", .w = "grow", .h = "grow") {
                    rcTextC(labels[i], .font = rcFontSemibold(15),
                            .color = on ? s.text : s.textMuted, .select = RC_SELECT_NONE);
                }
                rcBox(.bg = on ? s.primary : RC_UNSET_COLOR, .borderRadius = "all-full",
                      .wType = RC_PX(44), .hType = RC_PX(3)) {}
            }
            if (rcClicked(id) && !on) {
                *current = i;
                changed = true;
            }
        }
    }
    hairline();
    return changed;
}

/* A filter chip: ink on tone at rest, reversed when chosen, a shape change and not only a hue.
   Its target is the Tab stop, rounded as pill_button's is. */
static bool chip(const char *id, const char *label, bool on) {
    RC_Style s = rcGetStyle();

    rcBox(.id = id, .focusable = true, .align = "cc", .borderRadius = "all-full",
          .hType = RC_PX(TARGET), .wMin = RC_VAL(TARGET)) {
        rcBox(.bg = on ? s.text : s.surfaceAlt, .px = 14, .align = "cc", .borderRadius = "all-full",
              .hType = RC_PX(32), .overlay = press_wash(id)) {
            rcTextC(label, .font = rcFontSemibold(13), .color = on ? s.background : s.text,
                    .wrap = "n", .select = RC_SELECT_NONE);
        }
    }
    return rcClicked(id);
}

/* The phone's segmented control: a lifted thumb on a sunken track that takes the whole row, tall
   enough that each cell inside the track's 2 px is still a finger's target. */
static bool segmented(RC_Arena *m, const char *prefix, const char *const *labels, int count,
                      int *current) {
    RC_Style s = rcGetStyle();
    bool     changed = false;
    int      i;

    rcRow(.bg = s.surfaceAlt, .gap = 2, .p = 2, .borderRadius = "all-10", .w = "grow",
          .hType = RC_PX(TARGET + 4)) {
        for (i = 0; i < count; i++) {
            const char *id = rcFormat(m, "%s_%d", prefix, i).chars;
            const bool  on = *current == i;

            rcBox(.id = id, .focusable = true, .align = "cc", .borderRadius = "all-8",
                  .w = "grow", .h = "grow",
                  .shadow = { .color = on ? rcWithAlpha(RC_BLACK, 0.157f) : RC_UNSET_COLOR,
                              .y = 1.0f, .blur = 3.0f },
                  .className = on ? "bg-background" : "nav-press") {
                rcTextC(labels[i], .font = rcFontSemibold(13), .color = on ? s.text : s.textMuted,
                        .select = RC_SELECT_NONE);
            }
            if (rcClicked(id) && !on) {
                *current = i;
                changed = true;
            }
        }
    }
    return changed;
}

/* Why a list is empty, never a bare "nothing here". */
static void empty_state(RC_IconCallback icon, const char *title, const char *hint) {
    RC_Style s = rcGetStyle();

    rcColumn(.gap = 8, .px = 32, .py = 48, .align = "tc", .w = "grow") {
        icon(36.0f, s.textMuted);
        rcBox(.h = "4") {}
        rcTextC(title, .font = rcFontSemibold(17), .color = s.text, .textAlign = "c");
        rcTextC(hint, .color = s.textMuted, .size = 15, .lineHeight = 21, .textAlign = "c");
    }
}

static void section_label(const char *text) {
    rcBox(.pt = 20, .pb = 8, .px = GUTTER, .w = "grow") {
        rcTextC(text, .font = rcFontSemibold(17), .color = rcGetStyle().text,
                .select = RC_SELECT_NONE);
    }
}

/* The phone's activity indicator: eight spokes, drawn through the icon seam. While a pull is under
   way the spokes light one by one with the pointer; while the refresh runs a dark spoke chases
   round, by the clock. */
static void draw_spinner(RC_BoundingBox b, RC_Color color, const void *user) {
    static const float DIR[8][2] = {
        { 0.0f, -1.0f }, { 0.707f, -0.707f }, { 1.0f, 0.0f }, { 0.707f, 0.707f },
        { 0.0f, 1.0f }, { -0.707f, 0.707f }, { -1.0f, 0.0f }, { -0.707f, -0.707f },
    };
    const AppState *st   = (const AppState *)user;
    const int       head = (int)(st->now * 12.0) % 8;
    int             i;

    for (i = 0; i < 8; i++) {
        float alpha;

        if (st->refreshing)
            alpha = 40.0f + 215.0f * (float)((i + 8 - head) % 8) / 7.0f;
        else
            alpha = i < st->spinnerSpokes ? 200.0f : 0.0f;
        if (alpha > 0.0f)
            rcIconDrawRoundLine(b, 12.0f + DIR[i][0] * 5.0f, 12.0f + DIR[i][1] * 5.0f,
                                12.0f + DIR[i][0] * 10.0f, 12.0f + DIR[i][1] * 10.0f, 24.0f, 2.2f,
                                rcWithAlpha(color, alpha / 255.0f));
    }
}

#endif /* APP_PARTS_H */
