/*
    app.h - the frame: the title band, the three ways settings show, the hotkeys

    The settings are a dialog while the window has room for one, and rcBeginModal gives
    a dialog Esc, a scrim and a stack. Where it does not, the same pages fill the window,
    under the same ids: only the frame changes, and it is read from the space and never
    from the OS.
*/
#ifndef APP_APP_H
#define APP_APP_H

#include "rayclay.h"
#include "app/pages.h"
#include "app/state.h"
#include "app/theme.h"
#include "app/workspace.h"
#include "platform/platform.h"

#include "icons/rc_icons_arrow_left.h"
#include "icons/rc_icons_chevron_right.h"
#include "icons/rc_icons_circle_check.h"
#include "icons/rc_icons_panel_left.h"

/* Seconds a change must stand before it goes to disk: a slider's drag is one save. */
#define SAVE_DELAY 0.4

static inline bool bytes_equal(const void *a, const void *b, int size) {
    const unsigned char *x = (const unsigned char *)a, *y = (const unsigned char *)b;
    int                  i;

    for (i = 0; i < size; i++)
        if (x[i] != y[i]) return false;
    return true;
}

/* NO CONTROL SAYS IT CHANGED ANYTHING: the struct is compared with the copy on disk, so a
   new setting saves itself. A frame is asked for, as the window otherwise parks. */
static inline void settings_autosave(RC_App *app, AppState *st) {
    int size = (int)sizeof st->set;

    if (bytes_equal(&st->set, &st->saved, size)) return;
    if (!bytes_equal(&st->set, &st->settled, size)) {
        st->settled   = st->set;
        st->settledAt = rcAppTime(app);
    }
    if (rcAppTime(app) - st->settledAt >= SAVE_DELAY) {
        st->saveStatus = settings_save(&st->set);
        st->saved      = st->set;
        st->everSaved  = true;
    } else {
        rcWindowRequestFrameAfter(rcAppMainWindow(app), SAVE_DELAY);
    }
}

/* Backgrounding and quitting are the last moments an app is told anything. */
static inline void settings_flush(AppState *st) {
    if (bytes_equal(&st->set, &st->saved, (int)sizeof st->set)) return;
    settings_save(&st->set);
    st->saved = st->set;
}

/* Settings' hotkey fires under its Restore confirmation too (only the hotkey dialog holds
   the keys), so closing Settings closes that: left set, it would greet the next open. */
static inline void command_run(RC_App *app, AppState *st, int command) {
    Settings *set = &st->set;
    float     top = (float)(FONT_STOP_COUNT - 1);

    switch (command) {
    case CMD_SETTINGS:   st->settingsOpen = !st->settingsOpen; st->confirmReset = false; break;
    case CMD_SIDEBAR:    st->filesOpen = !st->filesOpen; break;
    case CMD_NEXT_NOTE:  tabs_step(st, +1); break;
    case CMD_PREV_NOTE:  tabs_step(st, -1); break;
    case CMD_FONT_UP:    if (set->fontStop < top) set->fontStop += 1.0f; break;
    case CMD_FONT_DOWN:  if (set->fontStop > 0.0f) set->fontStop -= 1.0f; break;
    case CMD_THEME:      set->scheme = style_is_dark() ? SCHEME_LIGHT : SCHEME_DARK; break;
    case CMD_READABLE:   set->readable = !set->readable; break;
    case CMD_STATUS_BAR: set->statusBar = !set->statusBar; break;
    case CMD_OUTLINE:    set->outline = !set->outline; break;
    case CMD_UPDATE:     updater_check(app, &st->update); break;
    default:             break;
    }
}

/* While the hotkey dialog is up, the next chord belongs to it and fires nothing. */
static inline void capture_update(AppState *st) {
    HotkeyCapture *c = &st->capture;
    Chord          chord = chord_pressed();
    int            i;

    if (chord.key == RC_KEY_NONE) return;
    c->pending  = chord;
    c->conflict = -1;
    c->refusal  = chord_refusal(chord);
    if (c->refusal) return;
    for (i = 0; i < CMD_COUNT; i++)
        if (i != c->command && chord_equal(st->set.hotkeys[i], chord)) c->conflict = i;
    if (c->conflict >= 0) return;
    st->set.hotkeys[c->command] = chord;
    c->open = false;
}

static inline void hotkeys_dispatch(RC_App *app, AppState *st) {
    int i;

    for (i = 0; i < CMD_COUNT; i++)
        if (chord_fired(st->set.hotkeys[i])) command_run(app, st, i);
}

/* The one-time work the window has to be open for: the saved zoom, the check the user
   asked to run at every start. The zoom is read once more so a key's change is kept. */
static inline void first_frame(RC_App *app, AppState *st) {
    RC_Window *win = rcAppMainWindow(app);

    if (st->set.zoom != 1.0f) rcWindowSetZoom(win, st->set.zoom);
    st->seenZoom = rcWindowZoom(win);
    if (st->set.autoCheck) updater_check(app, &st->update);
    st->started = true;
}

static inline void update(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    float     zoom;
    bool      dark;

    /* The update check is read once, here, before anything uses it. */
    updater_read(&st->update);
    if (!st->started) first_frame(app, st);
    /* The settings' look, the style and the corners' class names, installed before the
       hotkeys below read it and the layout draws in it. */
    dark = st->set.scheme == SCHEME_DARK ||
           (st->set.scheme == SCHEME_SYSTEM && rcPreferredColorScheme() != RC_COLOR_SCHEME_LIGHT);
    rcSetStyle(app_style(dark, rcHex(st->set.accent), st->set.radius));
    app_corners(st->set.radius);
    rcWindowSetClearColor(rcAppMainWindow(app), rcGetStyle().background);
    if (st->dismiss) { st->dismiss = false; rcSetFocus(NULL); }

    zoom = rcWindowZoom(rcAppMainWindow(app));
    if (zoom != st->seenZoom) {
        st->seenZoom = zoom;
        st->set.zoom = zoom;
    }

    if (st->capture.open) capture_update(st);
    else                  hotkeys_dispatch(app, st);
    if (rcKeyPressedOn("update_url", RC_KEY_ENTER) || rcKeyPressedOn("update_key", RC_KEY_ENTER))
        updater_check(app, &st->update);

    /* An Escape that a dialog, an open list or a focused field took reads false, so this
       is the Escape nothing else wanted: back up one level of the settings that fill the
       window. With them shut it goes unread, and Android's Back stays the system's. */
    if (st->settingsOpen && rcKeyPressed(RC_KEY_ESCAPE)) {
        if (st->arm == ARM_STACK && st->pageOpen) st->pageOpen = false;
        else                                      st->settingsOpen = false;
    }
    settings_autosave(app, st);
}

/* Replacing a hotkey needs a dialog of its own: Esc must close this, not the settings
   under it, and rcBeginModal's stack is what says so. */
static inline void hotkey_dialog(RC_App *app, AppState *st) {
    RC_Style       s   = rcGetStyle();
    Palette        pal = app_palette();
    RC_Arena      *a   = rcAppArena(app);
    HotkeyCapture *c   = &st->capture;
    float          w   = rcViewport().width - 72.0f;
    Chord          held = { RC_KEY_NONE, rcModDown(RC_MOD_PRIMARY), rcModDown(RC_MOD_SHIFT),
                            rcModDown(RC_MOD_ALT) };

    if (!rcBeginModal("hotkey_dialog", &c->open)) return;
    rcColumn(.gap = 12, .wType = RC_PX(w < 380.0f ? w : 380.0f), .className = "items-stretch") {
        rcTextL("Set hotkey", .font = rcFontSemibold((float)T_TITLE), .color = s.text);
        rcTextC(COMMANDS[c->command].name, .color = s.textMuted, .size = T_BODY);
        rcBox(.py = 24, .align = "cc", .w = "grow",
              .className = "item bg-background border border-primary") {
            if (c->pending.key != RC_KEY_NONE)
                rcText(chord_text(a, c->pending), .color = s.text, .size = T_HEAD);
            else if (held.primary || held.shift || held.alt)
                rcText(chord_text(a, held), .color = s.text, .size = T_HEAD);
            else
                rcTextL("Press the keys together", .color = s.textMuted, .size = T_HEAD);
        }
        if (c->refusal)
            rcTextC(c->refusal, .color = pal.warnInk, .size = T_HINT);
        else if (c->conflict >= 0)
            rcText(rcFormat(a, "Already used by %s. Replace takes it away from that command.",
                            COMMANDS[c->conflict].name),
                   .color = pal.warnInk, .size = T_HINT);
        else
            rcTextL("Esc cancels.", .color = s.textMuted, .size = T_HINT);
        rcRow(.gap = 8, .align = "cr", .w = "grow") {
            if (rcButton("hotkey_cancel", "Cancel", .className = control_class()))
                c->open = false;
            if (c->conflict >= 0 && rcButton("hotkey_replace", "Replace",
                                             .className = primary_class(),
                                             .variant = RC_BTN_PRIMARY)) {
                Chord none = { RC_KEY_NONE, false, false, false };
                st->set.hotkeys[c->conflict] = none;
                st->set.hotkeys[c->command]  = c->pending;
                c->open = false;
            }
        }
    }
    rcEndModal();
}

static inline void reset_dialog(RC_App *app, AppState *st) {
    RC_Style s = rcGetStyle();
    float    w = rcViewport().width - 72.0f;

    if (!rcBeginModal("reset_dialog", &st->confirmReset)) return;
    rcColumn(.gap = 12, .wType = RC_PX(w < 380.0f ? w : 380.0f), .className = "items-stretch") {
        rcTextL("Restore defaults?", .font = rcFontSemibold((float)T_TITLE), .color = s.text);
        rcTextL("Every option, hotkey and color goes back to how it shipped. Your account "
                "details stay.", .color = s.textMuted, .size = T_BODY);
        rcRow(.gap = 8, .pt = 4, .align = "cr", .w = "grow") {
            if (rcButton("reset_cancel", "Cancel", .className = control_class()))
                st->confirmReset = false;
            if (rcButton("reset_confirm", "Restore", .className = control_class(),
                         .variant = RC_BTN_DANGER)) {
                settings_defaults_options(&st->set);
                rcWindowSetZoom(rcAppMainWindow(app), st->set.zoom);
                st->confirmReset = false;
            }
        }
    }
    rcEndModal();
}

static inline void rail_group(const char *title, float top, float left) {
    rcBox(.pt = top, .pb = 4, .pl = left, .w = "grow") {
        rcTextC(title, .color = rcGetStyle().textMuted, .size = T_HINT, .select = RC_SELECT_NONE);
    }
}

static inline void rail_item(AppState *st, int page) {
    RC_Style    s  = rcGetStyle();
    const Page *p  = &PAGES[page];
    bool        on = st->page == page;

    if (rcClicked(p->id)) {
        st->page     = page;
        st->pageOpen = true;
    }
    rcRow(.id = p->id, .focusable = true, .gap = 10, .px = 10,
          .py = (float)(rcPointerIsCoarse() ? 12 : 7), .align = "cl", .w = "grow",
          .className = on ? "item selected" : "item lit") {
        p->icon(16.0f, on ? s.text : s.textMuted);
        rcBox(.w = "grow") {
            rcTextC(p->label, .color = s.text, .size = T_BODY, .select = RC_SELECT_NONE);
        }
    }
}

/* The autosave's answer, so a change never leaves anyone wondering. */
static inline void rail_foot(const AppState *st, float left) {
    RC_Style s   = rcGetStyle();
    Palette  pal = app_palette();

    rcRow(.gap = 8, .pt = 8, .pl = left, .align = "cl", .w = "grow") {
        if (st->everSaved && st->saveStatus != RC_FILE_OK) {
            rcTextL("Could not save settings", .color = s.danger, .size = T_CHIP,
                    .select = RC_SELECT_NONE);
        } else if (st->everSaved) {
            rcIconCircleCheck(14.0f, pal.goodInk);
            rcTextL("All changes saved", .color = s.textMuted, .size = T_CHIP,
                    .select = RC_SELECT_NONE);
        } else {
            rcTextL("Changes save as you make them", .color = s.textMuted, .size = T_CHIP,
                    .select = RC_SELECT_NONE);
        }
    }
}

/* A scrolling column clips what it holds, a focus ring too, and a ring reaches 4 px past
   its row (2 px wide, 2 px out): the padding is the ring's room. */
static inline void rail(AppState *st) {
    int i;

    rcColumn(.gap = 2, .h = "grow", .wType = RC_PX((float)RAIL_W)) {
        rcColumn(.id = "rail", .gap = 2, .pb = 4, .px = 4, .scroll = "v", .w = "grow",
                 .h = "grow") {
            rail_group("Options", 4, 10);
            for (i = PAGE_GENERAL; i < PAGE_PLUGINS; i++) rail_item(st, i);
            rail_group("Application", 16, 10);
            for (i = PAGE_PLUGINS; i < PAGE_COUNT; i++) rail_item(st, i);
        }
        rcScrollbar("rail");
        rail_foot(st, 14);
    }
}

/* One column wide the rail is the whole screen, so it reads as a phone's Settings root:
   you at the top, then each page as a row saying what it holds. */
static inline void root_row(AppState *st, int page) {
    RC_Style    s = rcGetStyle();
    const Page *p = &PAGES[page];

    if (rcClicked(p->id)) {
        st->page     = page;
        st->pageOpen = true;
    }
    rcRow(.id = p->id, .focusable = true, .gap = 12, .px = 8, .py = 12, .align = "cl",
          .w = "grow", .className = "item lit-row") {
        p->icon(20.0f, s.textMuted);
        rcColumn(.gap = 2, .w = "grow", .className = "items-stretch") {
            rcTextC(p->label, .color = s.text, .size = T_NAME, .select = RC_SELECT_NONE);
            rcTextC(p->line, .color = s.textMuted, .size = T_HINT, .select = RC_SELECT_NONE);
        }
        rcIconChevronRight(14.0f, s.textMuted);
    }
}

static inline void root_account(RC_App *app, AppState *st) {
    RC_Style    s  = rcGetStyle();
    const char *id = PAGES[PAGE_ACCOUNT].id;

    if (rcClicked(id)) {
        st->page     = PAGE_ACCOUNT;
        st->pageOpen = true;
    }
    rail_group(PAGES[PAGE_ACCOUNT].label, 4, 16);
    pane_card() {
        rcRow(.id = id, .focusable = true, .gap = 12, .px = 8, .py = 12, .align = "cl",
              .w = "grow", .className = "item lit-row") {
            profile_avatar(app, st->set.name, 48.0f, T_HEAD);
            rcColumn(.gap = 2, .w = "grow", .className = "items-stretch") {
                rcTextC(st->set.name[0] ? st->set.name : "No name yet",
                        .font = rcFontSemibold((float)T_HEAD), .color = s.text,
                        .select = RC_SELECT_NONE);
                rcTextC(st->set.email[0] ? st->set.email : "No email yet", .color = s.textMuted,
                        .size = T_HINT, .select = RC_SELECT_NONE);
            }
            rcIconChevronRight(14.0f, s.textMuted);
        }
    }
}

static inline void root_group(AppState *st, const char *title, int first, int end) {
    int i;

    rail_group(title, 8, 16);
    pane_card() {
        for (i = first; i < end; i++) {
            if (i == PAGE_ACCOUNT) continue;
            if (i > first) pane_rule();
            root_row(st, i);
        }
    }
}

static inline void rail_root(RC_App *app, AppState *st) {
    rcColumn(.gap = 8, .p = 8, .w = "grow", .h = "grow") {
        rcColumn(.id = "rail", .gap = 8, .scroll = "v", .w = "grow", .h = "grow") {
            root_account(app, st);
            root_group(st, "Options", PAGE_GENERAL, PAGE_PLUGINS);
            root_group(st, "Application", PAGE_PLUGINS, PAGE_COUNT);
            rail_foot(st, 16);
        }
        rcScrollbar("rail");
    }
}

/* The page's title and, in the dialog, its way out. */
static inline void pane_header(AppState *st, bool closes) {
    rcRow(.pt = 16, .pb = 8, .pl = 24, .pr = 12, .align = "cl", .w = "grow") {
        rcBox(.w = "grow") {
            rcTextC(PAGES[st->page].label, .font = rcFontSemibold((float)T_TITLE),
                    .color = rcGetStyle().text, .select = RC_SELECT_NONE);
        }
        if (closes && icon_button("settings_close", rcIconX, "Close")) st->settingsOpen = false;
    }
}

/* The page, in the one scrolling column everything is measured against, @p width wide. A
   page starts at its top: the scroll call comes before rcScrollbar, as rcScrollbar asks.
   It starts with no control focused either, unless the focus is on the rail's row that
   opened it: Tab then goes on from that row. One column wide there is no header above it,
   so the page keeps its own distance from the band. */
static inline void pane_body(RC_App *app, AppState *st, float width) {
    float side = rcViewport().width < (float)SPLIT_MIN_W ? 16.0f : 24.0f;

    pane_width = width;
    if (st->shownPage != st->page) {
        rcScrollToTop(PANE_ID);
        if (!rcIsFocused(PAGES[st->page].id)) rcSetFocus(NULL);
        st->shownPage  = st->page;
        st->copiedPath = false;
    }
    rcColumn(.id = PANE_ID, .gap = 12, .pt = (float)(st->arm == ARM_STACK ? 12 : 4), .pb = 24,
             .pl = side, .pr = side, .scroll = "v", .w = "grow", .h = "grow") {
        PAGES[st->page].draw(app, st);
    }
    rcScrollbar(PANE_ID);
}

/* Both dialogs a page can open, inside the settings dialog's scope so that they stack
   above it, and Esc closes the top one first. */
static inline void settings_popups(RC_App *app, AppState *st) {
    hotkey_dialog(app, st);
    reset_dialog(app, st);
}

/* THE PANEL TAKES ITS SIZE FROM ITS BODY, so the body is told the size it may have: about
   three quarters of a wide window, which leaves the note showing around it, and the window
   less room for the title band, or for a keyboard on the panel's far side, as the panel
   centers on the window. The panel's padding is the rail's alone (p-0), so the page, a
   sheet in the page's color, runs to the panel's edge and holds its cards. */
static inline void settings_dialog(RC_App *app, AppState *st) {
    RC_Viewport vp    = rcViewport();
    float       bar   = (float)BAR_H / rcWindowZoom(rcAppMainWindow(app));
    float       kb    = vp.keyboard.top > vp.keyboard.bottom ? vp.keyboard.top : vp.keyboard.bottom;
    float       cover = (bar > kb ? bar : kb) + 8.0f;
    float       room  = vp.width - 32.0f;
    float       w     = vp.width * 0.78f;
    float       h     = vp.height - 2.0f * cover;

    if (w < (float)DIALOG_FULL_W) w = (float)DIALOG_FULL_W;
    if (w > room) w = room;
    if (w > (float)DIALOG_MAX_W) w = (float)DIALOG_MAX_W;
    if (!rcBeginModal("settings", &st->settingsOpen, .className = "p-0")) return;
    /* Opened by the startup option, nobody has reached for it yet, so nothing wears a ring. */
    if (!st->settingsSeen) rcSetFocus(NULL);
    st->settingsSeen = true;
    rcRow(.wType = RC_PX(w), .hType = RC_PX(h < (float)DIALOG_MAX_H ? h : (float)DIALOG_MAX_H)) {
        rcColumn(.p = (float)RAIL_PAD, .h = "grow") {
            rail(st);
        }
        rcColumn(.w = "grow", .h = "grow", .className = "sheet bg-background") {
            pane_header(st, true);
            pane_body(app, st, w - (float)(RAIL_W + 2 * RAIL_PAD));
        }
    }
    settings_popups(app, st);
    rcEndModal();
}

/* The tabs: a click picks one, its X closes it. .noDrag on the row keeps a tab's click
   where the band around it drags the window. A tab and its X are named by the note they
   hold, not by their place: closing a tab moves the later ones left, and a name that
   moved would hand the focus to the next note's X. */
static inline void band_tabs(RC_App *app, AppState *st) {
    RC_Style  s = rcGetStyle();
    RC_Arena *a = rcAppArena(app);
    /* The band ignores the zoom, so its room is in window pixels. A tab that does not fit
       at a width that names its note is left out, the current one never: "+N" says how many
       are, and steps through them. */
    float     zoom  = rcWindowZoom(rcAppMainWindow(app));
    float     room  = rcViewport().width * zoom - (float)BAND_CHROME_W;
    float     least = (float)TAB_MIN_W + icon_button_size();
    int       fit   = (int)(room / (least + 2.0f));
    int       closing = -1, i;

    if (fit < 1) fit = 1;
    if (fit > st->tabCount) fit = st->tabCount;
    if (st->tabFirst > st->tabCount - fit) st->tabFirst = st->tabCount - fit;
    if (st->activeTab >= 0 && st->activeTab < st->tabFirst) st->tabFirst = st->activeTab;
    if (st->activeTab >= st->tabFirst + fit) st->tabFirst = st->activeTab - fit + 1;

    rcRow(.gap = 2, .pt = 6, .pl = 8, .align = "bl", .noDrag = true, .h = "grow") {
        for (i = st->tabFirst; i < st->tabFirst + fit; i++) {
            const char *id = rcFormat(a, "tab_%d", st->tabs[i]).chars;
            bool        on = i == st->activeTab;

            if (rcClicked(id)) {
                st->activeTab    = i;
                st->set.lastNote = st->tabs[i];
            }
            rcRow(.id = id, .focusable = true, .gap = 8, .pl = 12, .pr = 4, .align = "cl",
                  .overflow = "hidden", .h = "grow", .wMin = RC_VAL(least), .wMax = RC_VAL(200.0f),
                  .className = on ? "tab bg-background" : "tab lit") {
                rcBox(.overflow = "hidden") {
                    rcTextC(NOTES[st->tabs[i]].title, .color = on ? s.text : s.textMuted,
                            .size = T_HINT, .wrap = "e", .select = RC_SELECT_NONE);
                }
                if (icon_button(rcFormat(a, "tab_close_%d", st->tabs[i]).chars, rcIconX,
                                "Close tab"))
                    closing = i;
            }
        }
        if (st->tabCount > fit) {
            if (rcClicked("tab_more")) tabs_step(st, +1);
            rcBox(.id = "tab_more", .focusable = true, .px = 8, .align = "cc", .h = "grow",
                  .tooltip = "Next tab", .className = "tab lit") {
                rcText(rcFormat(a, "+%d", st->tabCount - fit), .color = s.textMuted, .size = T_HINT,
                       .select = RC_SELECT_NONE);
            }
        }
    }
    if (closing >= 0) tabs_close(st, closing);
}

/* The title band. Its whole width drags the window on a desktop; on a phone it is the
   screen's header, on the top inset, and the window controls draw nothing. It holds its
   size at any zoom, as a title bar does: the OS's drag strip is in window pixels. */
static inline void band(RC_App *app, AppState *st, bool workspace_up) {
    RC_Style    s    = rcGetStyle();
    RC_Viewport vp   = rcViewport();
    RC_Insets   safe = rcGetSafeAreaInsets();   /* window px: the unzoomed band's unit */
    bool        full = st->settingsOpen && !workspace_up;
    bool        back = full && st->arm == ARM_STACK && st->pageOpen;

    rcUnzoomed() {
        rcRow(.id = RC_ID_WINDOW_DRAG, .bg = s.chrome, .pt = safe.top,
              .pl = safe.left, .pr = safe.right, .align = "cl",
              .w = "grow", .hType = RC_PX((float)BAR_H + safe.top)) {
            rcRow(.gap = 4, .pl = 8, .align = "cl", .noDrag = true) {
                if (back && icon_button("settings_back", rcIconArrowLeft, "All settings"))
                    st->pageOpen = false;
                if (full && !back &&
                    icon_button("settings_close", rcIconArrowLeft, "Close settings"))
                    st->settingsOpen = false;
                if (!full && icon_button("band_files", rcIconPanelLeft, "Toggle file list"))
                    st->filesOpen = !st->filesOpen;
            }
            if (full) {
                rcBox(.pl = 8, .w = "grow") {
                    rcTextC(back ? PAGES[st->page].label : "Settings",
                            .font = rcFontSemibold((float)T_HEAD), .color = s.text,
                            .select = RC_SELECT_NONE);
                }
            } else {
                if (vp.width >= (float)SPLIT_MIN_W) band_tabs(app, st);
                rcSpacer() {}
                rcBox(.pr = 4, .noDrag = true) {
                    if (icon_button("band_settings", rcIconSettings, "Settings")) {
                        st->settingsOpen = true;
                        st->pageOpen     = false;
                    }
                }
            }
            /* Band-tall and flush to the corner, so the OS rounds the close slab with the
               window. It emits nothing on the web or on a phone. */
            rcWindowControls();
        }
    }
}

static inline void layout(RC_App *app, void *userData) {
    AppState   *st = (AppState *)userData;
    RC_Style    s  = rcGetStyle();
    RC_Viewport vp = rcViewport();
    float       cover = vp.keyboard.bottom > vp.safe.bottom ? vp.keyboard.bottom : vp.safe.bottom;
    bool        dialog, workspace_up;

    st->arm = vp.width >= (float)DIALOG_MIN_W && vp.height >= (float)DIALOG_MIN_H ? ARM_DIALOG
            : vp.width >= (float)SPLIT_MIN_W ? ARM_SPLIT : ARM_STACK;
    dialog       = st->settingsOpen && st->arm == ARM_DIALOG;
    workspace_up = !st->settingsOpen || dialog;

    rcColumn(.id = "root", .bg = s.background, .w = "grow", .h = "grow") {
        band(app, st, workspace_up);
        if (workspace_up) {
            workspace(app, st);
        } else if (st->arm == ARM_SPLIT) {
            rcRow(.gap = RAIL_GAP, .pt = 8, .pb = 8, .pl = 8, .w = "grow", .h = "grow") {
                rail(st);
                rcColumn(.w = "grow", .h = "grow") {
                    pane_header(st, false);
                    pane_body(app, st, vp.width - (float)(8 + RAIL_W + RAIL_GAP));
                }
            }
        } else if (st->arm == ARM_STACK) {
            rcColumn(.pl = vp.safe.left, .pr = vp.safe.right, .w = "grow",
                     .h = "grow") {
                if (st->pageOpen) {
                    pane_body(app, st, vp.width - vp.safe.left - vp.safe.right);
                } else {
                    rail_root(app, st);
                }
            }
        } else {
            rcSpacer() {}
        }
        /* The foot is as tall as whatever covers the window's bottom edge, the home
           indicator or the keyboard, so the column above it ends where the keys begin. */
        rcBox(.id = "foot", .w = "grow", .hType = RC_PX(cover)) {}
    }
    if (dialog)
        settings_dialog(app, st);
    else if (st->settingsOpen)
        settings_popups(app, st);
}

#endif /* APP_APP_H */
