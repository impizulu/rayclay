/*
    workspace.h - the app behind the dialog: a file list, tabs and a note

    It is here so the settings have something to change, and it is small on purpose:
    each thing it draws reads one setting. Close the dialog and it is a working reader.
*/
#ifndef APP_WORKSPACE_H
#define APP_WORKSPACE_H

#include "rayclay.h"
#include "app/pane.h"
#include "app/state.h"

#include "icons/rc_icons_check.h"
#include "icons/rc_icons_clock.h"
#include "icons/rc_icons_repeat.h"
#include "icons/rc_icons_chevron_down.h"
#include "icons/rc_icons_chevron_right.h"
#include "icons/rc_icons_file_text.h"
#include "icons/rc_icons_house.h"
#include "icons/rc_icons_moon.h"
#include "icons/rc_icons_panel_right.h"
#include "icons/rc_icons_sun.h"

static inline int note_font_size(const Settings *set) {
    return FONT_STOPS[(int)set->fontStop];
}

/* A run of note text, at the note's size and leading. The column gives it a width to
   wrap at. */
static inline void note_text(const Settings *set, const char *text, RC_Color color) {
    int size = note_font_size(set);

    rcColumn(.w = "grow", .className = "items-stretch") {
        rcTextC(text, .color = color, .size = (uint16_t)size,
                .lineHeight = (uint16_t)((float)size * set->lineHeight + 0.5f));
    }
}

static inline void note_task_box(bool done) {
    Palette pal = app_palette();

    /* A box this small rounds far less than a card (tick, theme.h), or at the largest radius a
       checkbox is a circle and reads as a radio. */
    rcBox(.align = "cc", .wType = RC_PX(16.0f), .hType = RC_PX(16.0f),
          .className = done ? "tick bg-primary border border-primary"
                            : "tick border border-muted-foreground") {
        if (done) rcIconCheck(12.0f, pal.accentInk);
    }
}

/* The air above a line: items of one list sit close, and a heading stands apart. */
static inline float note_line_air(const NoteLine *line, const NoteLine *before) {
    bool list   = line->kind == L_BULLET || line->kind == L_TASK || line->kind == L_DONE;
    bool listed = before && (before->kind == L_BULLET || before->kind == L_TASK ||
                             before->kind == L_DONE);

    if (line->kind == L_H2) return 24.0f;
    return list && listed ? 4.0f : 12.0f;
}

/* @p line_id names a heading for the outline to scroll to; NULL leaves it anonymous. */
static inline void note_line(const Settings *set, const NoteLine *line, float air,
                             const char *line_id) {
    RC_Style s      = rcGetStyle();
    int      size   = note_font_size(set);
    float    pitch  = (float)size * set->lineHeight;
    float    lift   = (pitch - 16.0f) * 0.5f;
    float    dotTop = (pitch - 5.0f) * 0.5f;

    switch (line->kind) {
    case L_H2:
        rcBox(.id = line_id, .pt = air, .w = "grow") {
            rcTextC(line->text, .font = rcFontSemibold((float)T_NOTE_H2),
                    .color = set->accentHeadings ? s.primary : s.text);
        }
        break;
    case L_BULLET:
        rcRow(.gap = 12, .pt = air, .pl = 4, .w = "grow") {
            rcBox(.pt = dotTop) {
                rcBox(.bg = s.textMuted, .borderRadius = "all-full", .wType = RC_PX(5.0f),
                      .hType = RC_PX(5.0f)) {}
            }
            note_text(set, line->text, s.text);
        }
        break;
    case L_TASK:
    case L_DONE:
        rcRow(.gap = 8, .pt = air, .w = "grow") {
            rcBox(.pt = lift) {
                note_task_box(line->kind == L_DONE);
            }
            note_text(set, line->text,
                      line->kind == L_DONE && set->dimDone ? s.textMuted : s.text);
        }
        break;
    case L_QUOTE:
        rcBox(.pt = air, .w = "grow") {
            rcRow(.gap = 12, .pr = 12, .py = 8, .w = "grow", .className = "item bg-primary/10") {
                rcBox(.h = "grow", .wType = RC_PX(3.0f), .className = "item bg-primary") {}
                note_text(set, line->text, s.text);
            }
        }
        break;
    default:
        rcBox(.pt = air, .w = "grow") {
            note_text(set, line->text, s.text);
        }
        break;
    }
}

/* A note's tags as chips. rcFormat, since a chip's text is a piece of a longer string. */
static inline void note_tags(RC_App *app, const char *tags) {
    RC_Style s = rcGetStyle();
    RC_Arena *a = rcAppArena(app);

    while (*tags) {
        int n = 0;
        while (tags[n] && tags[n] != ' ') n++;
        rcBox(.px = 8, .py = 2, .className = "rounded-full bg-primary/16") {
            rcText(rcFormat(a, "#%.*s", n, tags), .color = s.primary, .size = T_CHIP,
                   .select = RC_SELECT_NONE);
        }
        tags += n;
        if (*tags) tags++;
    }
}

/* The note itself, drawn the same in a tab and in the settings' preview. @p anchors gives
   the headings their ids; @p max_lines cuts a preview short, 0 draws them all. */
static inline void note_view(RC_App *app, AppState *st, const Note *n, bool anchors,
                             int max_lines) {
    const Settings *set = &st->set;
    RC_Style        s   = rcGetStyle();
    RC_Arena       *a   = rcAppArena(app);
    int             end = max_lines && max_lines < n->lineCount ? max_lines : n->lineCount;
    int             i;

    if (set->inlineTitle)
        rcColumn(.w = "grow", .className = "items-stretch") {
            rcTextC(n->title, .font = rcFontSemibold((float)T_NOTE_TITLE), .color = s.text);
        }
    if (set->properties == PROPS_VISIBLE)
        rcRow(.gap = 8, .pt = 8, .align = "cl", .w = "grow", .className = "flex-wrap") {
            rcText(rcFormat(a, "Modified %s", note_date(a, n, set->dateFormat).chars),
                   .color = s.textMuted, .size = T_HINT, .select = RC_SELECT_NONE);
            note_tags(app, n->tags);
        }
    for (i = 0; i < end; i++)
        note_line(set, &n->lines[i], note_line_air(&n->lines[i], i ? &n->lines[i - 1] : NULL),
                  anchors && n->lines[i].kind == L_H2 ? rcFormat(a, "line_%d", i).chars : NULL);
}

/* Whether a note can run wider than the shortest line width the setting offers. One column
   wide it cannot, so Readable line length has nothing to do there and does not show. */
static inline bool line_width_applies(void) {
    return rcViewport().width >= (float)SPLIT_MIN_W;
}

static inline void tabs_open(AppState *st, int note) {
    int i;

    st->set.lastNote = note;
    for (i = 0; i < st->tabCount; i++) {
        if (st->tabs[i] == note) {
            st->activeTab = i;
            return;
        }
    }
    /* Full: the new note takes the current tab, as a browser's single-tab link does. */
    if (st->tabCount == MAX_TABS) {
        st->tabs[st->activeTab] = note;
        return;
    }
    st->tabs[st->tabCount] = note;
    st->activeTab = st->tabCount++;
}

static inline void tabs_close(AppState *st, int tab) {
    int i;

    /* A tab closed left of the current one moves it one place left: it stays current. */
    if (tab < st->activeTab) st->activeTab--;
    for (i = tab; i < st->tabCount - 1; i++)
        st->tabs[i] = st->tabs[i + 1];
    st->tabCount--;
    if (st->activeTab >= st->tabCount) st->activeTab = st->tabCount - 1;
    if (st->activeTab >= 0) st->set.lastNote = st->tabs[st->activeTab];
}

static inline void tabs_step(AppState *st, int dir) {
    if (st->tabCount < 2) return;
    st->activeTab = (st->activeTab + dir + st->tabCount) % st->tabCount;
    st->set.lastNote = st->tabs[st->activeTab];
}

/* A list row's air: a finger needs more than a mouse, and Compact file list takes some back. */
static inline float file_row_py(const Settings *set) {
    if (rcPointerIsCoarse()) return set->compactFiles ? 9.0f : 14.0f;
    return set->compactFiles ? 3.0f : 6.0f;
}

/* One file: a row indented under its folder, lit when its tab is the current one. */
static inline void file_row(RC_App *app, AppState *st, int note, float indent) {
    RC_Style    s  = rcGetStyle();
    const char *id = rcFormat(rcAppArena(app), "file_%d", note).chars;
    bool        on = st->tabCount > 0 && st->tabs[st->activeTab] == note;

    if (rcClicked(id)) {
        tabs_open(st, note);
        if (rcViewport().width < (float)SPLIT_MIN_W) st->filesOpen = false;
    }
    rcRow(.id = id, .focusable = true, .pl = indent, .pr = 8, .py = file_row_py(&st->set),
          .align = "cl", .overflow = "hidden", .w = "grow",
          .className = on ? "item shrink-0 selected" : "item shrink-0 lit") {
        rcTextC(NOTES[note].title, .color = on ? s.text : s.textMuted, .size = T_BODY, .wrap = "e",
                .select = RC_SELECT_NONE);
    }
}

static inline void folder_row(RC_App *app, AppState *st, int folder) {
    RC_Style    s  = rcGetStyle();
    const char *id = rcFormat(rcAppArena(app), "folder_%d", folder).chars;

    if (rcClicked(id)) st->folderOpen[folder] = !st->folderOpen[folder];
    rcRow(.id = id, .focusable = true, .gap = 4, .pl = 4, .pr = 8, .py = file_row_py(&st->set),
          .align = "cl", .w = "grow", .className = "item lit") {
        if (st->folderOpen[folder]) rcIconChevronDown(14.0f, s.textMuted);
        else                        rcIconChevronRight(14.0f, s.textMuted);
        rcTextC(FOLDER_NAME[folder], .color = s.textMuted, .size = T_BODY,
                .select = RC_SELECT_NONE);
    }
}

/* The bytes of the character at @p s: a name's initial may be more than one. */
static inline int char_bytes(const char *s) {
    int n = 1;

    if (!*s) return 0;
    while (((unsigned char)s[n] & 0xC0u) == 0x80u) n++;
    return n;
}

/* The user's initials on the accent: the Account page's name, live. */
static inline void profile_avatar(RC_App *app, const char *name, float diameter, int size) {
    Palette     pal = app_palette();
    const char *first = name, *last = "", *p;

    for (p = name; *p; p++)
        if (*p == ' ' && p[1] && p[1] != ' ') last = p + 1;
    rcBox(.bg = rcGetStyle().primary, .align = "cc", .borderRadius = "all-full",
          .wType = RC_PX(diameter), .hType = RC_PX(diameter)) {
        rcText(rcFormat(rcAppArena(app), "%.*s%.*s", char_bytes(first), first, char_bytes(last),
                        last),
               .color = pal.accentInk, .size = (uint16_t)size, .select = RC_SELECT_NONE);
    }
}

/* Any note but the ones already up, or the next along when all of them are. */
static inline int note_random(const AppState *st) {
    /* Tenths of a second since 1970 are past INT_MAX, so they are counted in 64 bits, and a
       clock set before 1970 counts below zero. */
    int64_t tenths = (int64_t)(rcUnixTimeSeconds() * 10.0);
    int     start  = (int)((tenths % NOTE_COUNT + NOTE_COUNT) % NOTE_COUNT);
    int     i, t;

    for (i = 0; i < NOTE_COUNT; i++) {
        int  note = (start + i) % NOTE_COUNT;
        bool up   = false;
        for (t = 0; t < st->tabCount; t++)
            if (st->tabs[t] == note) up = true;
        if (!up) return note;
    }
    return start;
}

/* The one-click actions. The theme icon shows what a click turns it into. */
static inline void ribbon_buttons(AppState *st) {
    if (icon_button("ribbon_home", rcIconHouse, "Open Welcome"))
        tabs_open(st, 0);
    if (icon_button("ribbon_theme", style_is_dark() ? rcIconSun : rcIconMoon,
                    "Switch light and dark"))
        st->set.scheme = style_is_dark() ? SCHEME_LIGHT : SCHEME_DARK;
    if (icon_button("ribbon_outline", rcIconPanelRight, "Toggle outline"))
        st->set.outline = !st->set.outline;
    /* Two core plugins live here: turning one on adds its button. */
    if (st->set.dailyNotes &&
        icon_button("ribbon_daily", rcIconClock, "Open the newest note in Journal"))
        tabs_open(st, NOTE_NEWEST_JOURNAL);
    if (st->set.randomNote && icon_button("ribbon_random", rcIconRepeat, "Open a random note"))
        tabs_open(st, note_random(st));
}

static inline void file_list(RC_App *app, AppState *st, bool whole_window) {
    RC_Style s = rcGetStyle();
    int      f, n;

    rcColumn(.bg = s.surface, .h = "grow",
             .wType = whole_window ? RC_GROW : RC_PX((float)FILES_W)) {
        rcBox(.pt = 16, .pb = 4, .pl = 16, .w = "grow") {
            rcTextL("Field Notes", .font = rcFontSemibold((float)T_HEAD), .color = s.text,
                    .select = RC_SELECT_NONE);
        }
        /* With no room for a strip beside the list, the ribbon runs across its head. */
        if (whole_window && st->set.ribbon)
            rcRow(.gap = 4, .pt = 4, .pb = 4, .pl = (float)(rcPointerIsCoarse() ? 2 : 10),
                  .align = "cl", .w = "grow") {
                ribbon_buttons(st);
            }
        /* The list scrolls, so it clips: its padding is the room a row's focus ring needs,
           which reaches 4 px past the row (2 px wide, 2 px out). */
        rcColumn(.id = "files", .gap = 2, .px = 8, .py = 4, .scroll = "v", .w = "grow",
                 .h = "grow") {
            for (f = 0; f < FOLDER_COUNT; f++) {
                folder_row(app, st, f);
                if (!st->folderOpen[f]) continue;
                for (n = 0; n < NOTE_COUNT; n++)
                    if (NOTES[n].folder == f) file_row(app, st, n, 24);
            }
            for (n = 0; n < NOTE_COUNT; n++)
                if (NOTES[n].folder == NO_FOLDER) file_row(app, st, n, 8);
        }
        rcScrollbar("files");
        /* The foot is the account: click it and Settings opens on that page. */
        if (rcClicked("files_account")) {
            st->settingsOpen = true;
            st->pageOpen     = true;
            st->page         = PAGE_ACCOUNT;
        }
        rcBox(.bg = s.border, .w = "grow", .hType = RC_PX(1.0f)) {}
        /* Set in from the list's edges, as the rows above are, so its focus ring shows whole. */
        rcBox(.p = 4, .w = "grow") {
            rcRow(.id = "files_account", .focusable = true, .gap = 8, .px = 8,
                  .py = (float)(rcPointerIsCoarse() ? 10 : 4), .align = "cl", .w = "grow",
                  .className = "item lit") {
                profile_avatar(app, st->set.name, 24.0f, T_CHIP);
                rcBox(.overflow = "hidden", .w = "grow") {
                    rcTextC(st->set.name[0] ? st->set.name : "No name yet", .color = s.text,
                            .size = T_BODY, .wrap = "e", .select = RC_SELECT_NONE);
                }
            }
        }
    }
}

static inline void ribbon(AppState *st) {
    rcColumn(.bg = rcGetStyle().chrome, .gap = 4, .py = 8, .align = "tc", .h = "grow",
             .wType = RC_PX((float)RIBBON_W)) {
        ribbon_buttons(st);
    }
}

static inline void status_bar(RC_App *app, const AppState *st, const Note *n) {
    RC_Style  s = rcGetStyle();
    RC_Arena *a = rcAppArena(app);
    int       words, chars;

    note_count(n, &words, &chars);
    rcRow(.bg = s.chrome, .gap = 16, .px = 16, .align = "cr", .w = "grow",
          .hType = RC_PX((float)STATUS_H)) {
        if (st->set.readingTime)
            rcText(rcFormat(a, "%d min read", words / 200 + 1), .color = s.textMuted,
                   .size = T_CHIP, .select = RC_SELECT_NONE);
        if (st->set.wordCount) {
            rcText(rcFormat(a, "%s words", rcFormatInt(a, words, ",").chars), .color = s.textMuted,
                   .size = T_CHIP, .select = RC_SELECT_NONE);
            rcText(rcFormat(a, "%s characters", rcFormatInt(a, chars, ",").chars),
                   .color = s.textMuted, .size = T_CHIP, .select = RC_SELECT_NONE);
        }
        rcText(note_date(a, n, st->set.dateFormat), .color = s.textMuted, .size = T_CHIP,
               .select = RC_SELECT_NONE);
    }
}

/* Beside the note when the window has room for both; otherwise a card at the note's head,
   in the same scrolling column, so a heading's click scrolls it the same way. */
static inline void outline_panel(RC_App *app, const Note *n, bool beside) {
    RC_Style  s  = rcGetStyle();
    RC_Arena *a  = rcAppArena(app);
    float     py = rcPointerIsCoarse() ? 12.0f : 4.0f;
    int       i;

    rcColumn(.gap = 2, .p = 12,
             .wType = beside ? RC_PX((float)OUTLINE_W) : RC_GROW,
             .hType = beside ? RC_GROW : RC_FIT,
             .className = beside ? "bg-surface" : "card bg-surface") {
        rcBox(.pb = 8, .pl = 8) {
            rcTextL("Outline", .font = rcFontSemibold((float)T_HEAD), .color = s.text,
                    .select = RC_SELECT_NONE);
        }
        for (i = 0; i < n->lineCount; i++) {
            const char *id;
            bool        hot;

            if (n->lines[i].kind != L_H2) continue;
            id  = rcFormat(a, "outline_%d", i).chars;
            hot = rcShowsHover(id);   /* the run's ink: a text run has no state of its own */
            if (rcClicked(id))
                rcScrollIntoView("note_scroll", rcFormat(a, "line_%d", i).chars, RC_SCROLL_START);
            rcRow(.id = id, .focusable = true, .px = 8, .py = py, .w = "grow",
                  .className = "item lit") {
                rcTextC(n->lines[i].text, .color = hot ? s.text : s.textMuted, .size = T_HINT,
                        .select = RC_SELECT_NONE);
            }
        }
    }
}

/* The tab area: the current note with its outline and status bar, or the empty state. */
static inline void note_pane(RC_App *app, AppState *st) {
    RC_Style    s    = rcGetStyle();
    RC_Viewport vp   = rcViewport();
    const Note *n    = st->tabCount > 0 ? &NOTES[st->tabs[st->activeTab]] : NULL;
    float       side = vp.width < (float)SPLIT_MIN_W ? 16.0f : 32.0f;
    bool        beside = vp.width >= (float)DIALOG_MIN_W;

    rcColumn(.bg = s.background, .w = "grow", .h = "grow") {
        if (!n) {
            rcColumn(.gap = 12, .align = "cc", .w = "grow", .h = "grow") {
                rcTextL("No note is open", .font = rcFontSemibold((float)T_TITLE), .color = s.text);
                rcTextL("Pick one from the file list, or start with Welcome.",
                        .color = s.textMuted, .size = T_BODY);
                if (rcButton("open_welcome", "Open Welcome", .className = primary_class(),
                             .variant = RC_BTN_PRIMARY))
                    tabs_open(st, 0);
            }
        } else {
            rcRow(.w = "grow", .h = "grow") {
                rcColumn(.id = "note_scroll", .pt = 32, .pb = 48, .pl = side, .pr = side,
                         .align = "tc", .scroll = "v", .w = "grow", .h = "grow") {
                    /* A readable measure caps the column; without one an unset .wMax is none. */
                    rcColumn(.w = "grow", .wMax = st->set.readable ? RC_VAL(st->set.lineWidth)
                                                                   : RC_UNSET_VAL) {
                        if (st->set.outline && !beside) {
                            rcBox(.pb = 24, .w = "grow") {
                                outline_panel(app, n, false);
                            }
                        }
                        note_view(app, st, n, true, 0);
                    }
                }
                if (st->set.outline && beside) outline_panel(app, n, true);
            }
            rcScrollbar("note_scroll");
            if (st->set.statusBar) status_bar(app, st, n);
        }
    }
}

static inline void workspace(RC_App *app, AppState *st) {
    bool one = rcViewport().width < (float)SPLIT_MIN_W;

    rcRow(.w = "grow", .h = "grow") {
        if (st->set.ribbon && !one) ribbon(st);
        if (st->filesOpen) file_list(app, st, one);
        if (!one || !st->filesOpen) note_pane(app, st);
    }
}

#endif /* APP_WORKSPACE_H */
