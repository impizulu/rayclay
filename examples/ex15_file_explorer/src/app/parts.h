/*
    parts.h - the small shapes the screen files share

    Text styles, hairlines, the toolbar's icon button, and the icon of a file. Every text
    run in the browser is chrome, as in the Finder, so a drag on a row never selects its
    letters and Cmd+A selects files, not words.
*/
#ifndef APP_PARTS_H
#define APP_PARTS_H

#include "rayclay.h"
#include "app/theme.h"
#include "app/browser.h"

/* Every glyph is a header generated from an SVG in examples/assets/icons/;
   ex10_util_rc_icon_converter makes headers of the same kind from your own
   SVGs. folder-tile and document-page bake their own colors. */
#include "icons/rc_icons_app_window.h"
#include "icons/rc_icons_archive.h"
#include "icons/rc_icons_arrow_up_down.h"
#include "icons/rc_icons_caret_down.h"
#include "icons/rc_icons_caret_right.h"
#include "icons/rc_icons_check.h"
#include "icons/rc_icons_chevron_down.h"
#include "icons/rc_icons_chevron_left.h"
#include "icons/rc_icons_chevron_right.h"
#include "icons/rc_icons_chevron_up.h"
#include "icons/rc_icons_clock.h"
#include "icons/rc_icons_code.h"
#include "icons/rc_icons_columns_3.h"
#include "icons/rc_icons_document_page.h"
#include "icons/rc_icons_document_solid.h"
#include "icons/rc_icons_download.h"
#include "icons/rc_icons_ellipsis.h"
#include "icons/rc_icons_file_text.h"
#include "icons/rc_icons_film.h"
#include "icons/rc_icons_folder.h"
#include "icons/rc_icons_folder_plus.h"
#include "icons/rc_icons_folder_tile.h"
#include "icons/rc_icons_hard_drive.h"
#include "icons/rc_icons_image.h"
#include "icons/rc_icons_info.h"
#include "icons/rc_icons_layout_grid.h"
#include "icons/rc_icons_list.h"
#include "icons/rc_icons_monitor.h"
#include "icons/rc_icons_music.h"
#include "icons/rc_icons_panel_left.h"
#include "icons/rc_icons_presentation.h"
#include "icons/rc_icons_search.h"
#include "icons/rc_icons_table_2.h"
#include "icons/rc_icons_trash_2.h"
#include "icons/rc_icons_x.h"

/* finger() sizes the targets: a phone, or a touch screen once it has been touched. */
static inline bool finger(void) { return rcPointerIsCoarse(); }

/* A text button's class: compact under a mouse, HIT under a finger. An h-* is exact, so a
   bare "h-7" would take away the 44 px the library gives a button under a coarse pointer. */
static inline const char *button_h(void) { return finger() ? "h-11" : "h-7"; }

static inline void rule(void) { rcBox(.bg = ui.line, .w = "grow", .hType = RC_PX(1)) {} }
static inline void vline(void) { rcBox(.bg = ui.line, .h = "grow", .wType = RC_PX(1)) {} }

/* What a person reads for a node: an app's bundle suffix stays out of sight, as in the Finder.
   The bytes stay the node's, so it lasts as long as the name. */
static inline RC_String shown_name(const Node *n) {
    RC_String s = rcStringFromCStr(n->name);

    if (n->kind == KIND_APP && s.length > 4) s.length -= 4;
    return s;
}

/* One line that ends in an ellipsis where its box does: the box grows to the room left. */
static inline void line_fill(RC_String s, int size, RC_Color color) {
    rcBox(.overflow = "hidden", .w = "grow") {
        rcText(s, .color = color, .size = (uint16_t)size, .wrap = "e", .select = RC_SELECT_NONE);
    }
}

static inline void section_title(const char *s, RC_Color color) {
    rcTextC(s, .font = rcFontSemibold(TEXT_CAPTION), .color = color, .select = RC_SELECT_NONE);
}

/* rcClicked for a box drawn as a button: a Tab stop (.focusable), which Enter and Space
   click. A pointer's click gives the focus back, as a press on a row does, so Space and
   Return stay Quick Look and rename (app.h). A key's click keeps it (rcClickCount reads 0
   for one), and Tab goes on from there. */
static inline bool box_clicked(const char *id) {
    if (!rcClicked(id)) return false;
    if (rcClickCount() > 0) rcSetFocus(NULL);
    return true;
}

/* A toolbar button: a glyph, a hover wash, a tooltip for the mouse. rcClicked makes any
   box a button, pointer cursor included, and .disabled is the same for a box as for a
   control, so a dead Back button reads dead, takes no click, shows no hover and Tab passes
   it. It wears what the toolbar's ghost menus wear: nothing until hovered. */
static inline bool tool_button(const char *id, RC_IconCallback icon, const char *tip, bool on,
                               bool disabled) {
    float side = finger() ? (float)HIT : (float)TOOL_H;

    rcBox(.id = id, .focusable = true, .align = "cc", .borderRadius = "all-md",
          .disabled = disabled, .wType = RC_PX(side), .hType = RC_PX(side), .tooltip = tip,
          .className = on ? "chosen" : "wash-blue") {
        icon(16.0f, ui.ink);
    }
    return box_clicked(id);
}

/* ---- File icons ---- */

typedef struct { RC_IconCallback glyph; RC_Color color; } KindLook;

/* The glyph and the hue of each kind. A list's small icon is a solid sheet in the hue; a
   large one is a white sheet with the glyph on it. */
static inline KindLook kind_look(Kind kind) {
    switch (kind) {
    case KIND_IMAGE:
    case KIND_DRAWING: return RC_LIT(KindLook){ rcIconImage, rcRgb(20, 150, 160) };
    case KIND_PDF:     return RC_LIT(KindLook){ rcIconFileText, rcRgb(224, 62, 54) };
    case KIND_DOC:     return RC_LIT(KindLook){ rcIconFileText, rcRgb(40, 110, 220) };
    case KIND_CODE:    return RC_LIT(KindLook){ rcIconCode, rcRgb(94, 92, 230) };
    case KIND_SHEET:   return RC_LIT(KindLook){ rcIconTable2, rcRgb(40, 160, 85) };
    case KIND_SLIDES:  return RC_LIT(KindLook){ rcIconPresentation, rcRgb(240, 130, 20) };
    case KIND_AUDIO:   return RC_LIT(KindLook){ rcIconMusic, rcRgb(230, 62, 120) };
    case KIND_VIDEO:   return RC_LIT(KindLook){ rcIconFilm, rcRgb(150, 80, 220) };
    case KIND_ARCHIVE: return RC_LIT(KindLook){ rcIconArchive, rcRgb(180, 125, 40) };
    case KIND_DISK:    return RC_LIT(KindLook){ rcIconHardDrive, rcRgb(110, 110, 120) };
    default:           return RC_LIT(KindLook){ rcIconFileText, rcRgb(130, 130, 138) };
    }
}

/* An app's tile: a rounded square in a hue taken from its name, with its initial on it
   once there is room for a letter. A name starts with a whole character when it is ASCII;
   any other first letter leaves the tile plain. */
static inline void app_icon(const Node *n, float size) {
    static const uint32_t FROM[4] = { 0x5B8DEF, 0xF2726F, 0x34C38F, 0xA66CFF };
    static const uint32_t TO[4]   = { 0x2F5FD0, 0xD8433F, 0x1E9C6D, 0x7C3AED };
    int       hue     = (unsigned char)n->name[0] % 4;
    RC_String initial = rcStringFromCStr(n->name);

    initial.length = (unsigned char)n->name[0] < 0x80 ? 1 : 0;
    if (size < 32.0f) {
        rcBox(.borderRadius = "all-sm", .wType = RC_PX(size), .hType = RC_PX(size),
              .gradient = { .from = rcHex(FROM[hue]), .to = rcHex(TO[hue]), .dir = "v" }) {}
    } else {
        rcBox(.align = "cc", .borderRadius = "all-xl", .wType = RC_PX(size), .hType = RC_PX(size),
              .gradient = { .from = rcHex(FROM[hue]), .to = rcHex(TO[hue]), .dir = "v" }) {
            const float em = (float)(size >= 96.0f ? TEXT_INITIAL : TEXT_TITLE);
            rcText(initial, .font = rcFontSemibold(em), .color = RC_WHITE,
                   .select = RC_SELECT_NONE);
        }
    }
}

/* The icon of a node at any size. On a selected row a small sheet turns white, as its hue
   would fight the selection's blue. */
static inline void file_icon(const Node *n, float size, bool selected) {
    KindLook look = kind_look(n->kind);

    if (n->kind == KIND_APP) {
        app_icon(n, size);
    } else if (n->isDir) {
        rcIconFolderTile(size);
    } else if (size < 32.0f) {
        rcIconDocumentSolid(size, selected ? RC_WHITE : look.color);
    } else {
        rcBox(.align = "cc", .wType = RC_PX(size), .hType = RC_PX(size)) {
            rcIconDocumentPage(size);
            rcBox(.floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_CENTER,
                                .element = RC_ANCHOR_CENTER,
                                .offset = { 0.0f, size * 0.06f },
                                .capture = RC_CAPTURE_PASSTHROUGH }) {
                look.glyph(size * 0.40f, look.color);
            }
        }
    }
}

static inline RC_Color tag_color(int tag) {
    switch (tag) {
    case TAG_RED:    return rcRgb(255, 69, 58);
    case TAG_ORANGE: return rcRgb(255, 149, 0);
    case TAG_YELLOW: return rcRgb(255, 204, 0);
    case TAG_GREEN:  return rcRgb(52, 199, 89);
    case TAG_BLUE:   return rcRgb(0, 122, 255);
    case TAG_PURPLE: return rcRgb(175, 82, 222);
    default:         return rcRgb(142, 142, 147);
    }
}

/* A tag's dot. On a selected row a white ring keeps a blue tag apart from the blue. */
static inline void tag_dot(int tag, float side, bool selected) {
    rcBox(.bg = tag_color(tag), .borderRadius = "all-full",
          .border = { .color = selected ? RC_WHITE : RC_UNSET_COLOR, .width = 1 },
          .wType = RC_PX(side), .hType = RC_PX(side)) {}
}

/* ---- Numbers and dates, as the Finder writes them ---- */

/* Decimal units, as a volume's maker and the Finder count them. */
static inline const char *size_text(RC_Arena *a, const Node *n) {
    double v = (double)n->size;

    if (n->isDir) return "--";
    if (v < 1e3)  return rcFormat(a, "%d bytes", (int)n->size).chars;
    if (v < 1e6)  return rcFormat(a, "%.0f KB", v / 1e3).chars;
    if (v < 1e9)  return rcFormat(a, "%.1f MB", v / 1e6).chars;
    return rcFormat(a, "%.2f GB", v / 1e9).chars;
}

/* A date in the form "Oct 28, 2026 at 9:41 AM", in the user's time zone, as a file manager
   shows one. */
static inline const char *date_text(RC_Arena *a, double unixSeconds, bool withTime) {
    static const char *const MONTH[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    RC_LocalTime at;

    if (unixSeconds <= 0.0 || !rcLocalTime(unixSeconds, &at)) return "--";
    if (!withTime) return rcFormat(a, "%s %d, %d", MONTH[at.month - 1], at.day, at.year).chars;
    return rcFormat(a, "%s %d, %d at %d:%02d %s", MONTH[at.month - 1], at.day, at.year,
                    at.hour % 12 ? at.hour % 12 : 12, at.minute, at.hour < 12 ? "AM" : "PM").chars;
}

#endif /* APP_PARTS_H */
