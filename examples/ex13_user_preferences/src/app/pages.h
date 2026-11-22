/*
    pages.h - one function per entry in the settings rail

    Each page is a list of calls to pane.h and nothing else: read one, and you have read
    them all. Every setting here is read by something else in the app: the note behind
    the dialog (workspace.h), the style of the whole window (app_style) or the About
    page's update check.
*/
#ifndef APP_PAGES_H
#define APP_PAGES_H

#include "rayclay.h"
#include "app/accent.h"
#include "app/pane.h"
#include "app/state.h"
#include "app/workspace.h"

#include "icons/rc_icons_contrast_filled.h"
#include "icons/rc_icons_file_text.h"
#include "icons/rc_icons_gallery_vertical_end.h"
#include "icons/rc_icons_hash.h"
#include "icons/rc_icons_info.h"
#include "icons/rc_icons_plus.h"
#include "icons/rc_icons_rayclay_logo.h"
#include "icons/rc_icons_rotate_cw.h"
#include "icons/rc_icons_search.h"
#include "icons/rc_icons_settings.h"
#include "icons/rc_icons_user.h"
#include "icons/rc_icons_x.h"

static const char *const DATE_ITEMS[DATE_COUNT] = {
    "2026-11-06", "Nov 6, 2026", "6 Nov 2026"
};
static const char *const PROPS_ITEMS[PROPS_COUNT] = { "Visible", "Hidden" };
static const char *const SCHEME_ITEMS[SCHEME_COUNT] = { "Dark", "Light", "Adapt to system" };

/* The Zoom level list, and the factor each one means. */
static const char *const ZOOM_ITEMS[] = {
    "75%", "80%", "90%", "100%", "110%", "125%", "150%", "175%", "200%"
};
static const float ZOOM_FACTOR[] = { 0.75f, 0.8f, 0.9f, 1.0f, 1.1f, 1.25f, 1.5f, 1.75f, 2.0f };
enum { ZOOM_COUNT = (int)(sizeof ZOOM_FACTOR / sizeof ZOOM_FACTOR[0]) };

static inline int zoom_nearest(float zoom) {
    int   best = 0, i;
    float gap  = 100.0f;

    for (i = 0; i < ZOOM_COUNT; i++) {
        float d = ZOOM_FACTOR[i] > zoom ? ZOOM_FACTOR[i] - zoom : zoom - ZOOM_FACTOR[i];
        if (d < gap) { gap = d; best = i; }
    }
    return best;
}

/* While a check or a download runs, the button is Stop (rcUpdateCancel). */
static inline void update_button(RC_App *app, Updater *up) {
    if (updater_busy(up)) {
        if (rcButton("update_check", "Stop", .className = control_class()))
            updater_stop(up);
    } else if (rcButton("update_check", "Check for updates", .className = primary_class(),
                        .variant = RC_BTN_PRIMARY)) {
        updater_check(app, up);
    }
}

static inline void automatic_updates_row(RC_App *app, AppState *st) {
    pane_toggle(app, "auto_check", "Automatic updates",
                "Check for a new version each time the app starts.",
                &st->set.autoCheck);
}

/* The settings file's path from the app's own folders on: the part before them names the
   user's account, which a shared or recorded screen should not show. settings_path() ends
   every path with those two folders and the file, so their length is known, and the path's
   own separators stay. Copy path copies the whole path. */
static inline const char *path_shown(RC_App *app, const char *path) {
    int tail = (int)sizeof("/" SETTINGS_ORG "/" SETTINGS_APP "/" SETTINGS_FILE) - 1;
    int n    = rcStringFromCStr(path).length;

    if (!path[0]) return "This platform has no app folder.";
    return n > tail ? rcFormat(rcAppArena(app), "...%s", path + n - tail).chars : path;
}

static inline void page_general(RC_App *app, AppState *st) {
    Settings *set = &st->set;
    RC_Style  s   = rcGetStyle();
    Updater  *up  = &st->update;

    pane_card() {
        pane_row() {
            rcColumn(.gap = 2, .w = "grow", .className = "items-stretch") {
                rcText(rcFormat(rcAppArena(app), "Version %s", RUNNING_VERSION), .color = s.text,
                       .size = T_NAME, .select = RC_SELECT_NONE);
                rcText(update_status(app, up), .color = update_status_color(up), .size = T_HINT,
                       .select = RC_SELECT_NONE);
            }
            update_button(app, up);
        }
        pane_rule();
        automatic_updates_row(app, st);
    }
    update_banner(app, up);
    pane_section("Startup");
    pane_card() {
        pane_toggle(app, "open_at_start", "Open settings at startup",
                    "Start on Settings. Turn it off to start on your notes.",
                    &set->openAtStart);
        pane_rule();
        pane_toggle(app, "reopen_last", "Reopen the last note",
                    "Pick up where you left off instead of on Welcome.", &set->reopenLast);
    }
    pane_section("Dates");
    pane_card() {
        pane_select("date_format", "Date format", "How a note's modified date reads.",
                    &set->dateFormat, DATE_ITEMS, DATE_COUNT);
    }
    pane_section("Your settings");
    pane_card() {
        pane_row() {
            rcColumn(.gap = 2, .w = "grow", .className = "items-stretch") {
                rcTextL("Settings file", .color = s.text, .size = T_NAME, .select = RC_SELECT_NONE);
                rcTextC(path_shown(app, st->settingsPath), .color = s.textMuted, .size = T_HINT,
                        .wordBreak = "b");
            }
            if (rcButton("copy_path", st->copiedPath ? "Copied" : "Copy path",
                         .disabled = !st->settingsPath[0], .className = control_class())) {
                rcClipboardSet(st->settingsPath);
                st->copiedPath = true;
            }
        }
        pane_rule();
        if (pane_button("reset_all", "Restore defaults",
                        "Every option, hotkey and color goes back. Your account stays.",
                        "Restore...", RC_BTN_DEFAULT))
            st->confirmReset = true;
    }
}

/* A live sample of a note. A fixed height, as the page must not shift under a slider that
   changes the text in it; what does not fit fades out at the foot, above the border. The fade
   is a float at z 2, under a combo's list; in the dialog the library counts that z from the
   panel up. */
static inline void page_preview(RC_App *app, AppState *st) {
    rcColumn(.p = 16, .overflow = "hidden", .w = "grow", .hType = RC_PX(224.0f),
             .className = "card bg-background border") {
        note_view(app, st, &NOTES[1], false, 4);
        rcBox(.px = 16, .w = "grow", .hType = RC_PX(32.0f),
              .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_BOTTOM_LEFT,
                            .element = RC_ANCHOR_BOTTOM_LEFT, .offset = { 0.0f, -1.0f },
                            .zIndex = 2, .capture = RC_CAPTURE_PASSTHROUGH }) {
            rcBox(.w = "grow", .h = "grow",
                  .className = "bg-linear-to-b from-background/0 to-background") {}
        }
    }
}

static inline void page_reader(RC_App *app, AppState *st) {
    Settings *set = &st->set;
    RC_Arena *a   = rcAppArena(app);

    page_preview(app, st);
    pane_section("Layout");
    pane_card() {
        if (line_width_applies()) {
            pane_toggle(app, "readable", "Readable line length",
                        "Keep long lines from running the width of the window.", &set->readable);
            pane_rule();
            pane_slider("line_width", "Maximum line width", "The widest a line of a note may run.",
                        &set->lineWidth, 480.0f, 960.0f, 40.0f,
                        rcFormat(a, "%d px", (int)set->lineWidth), !set->readable);
            pane_rule();
        }
        pane_toggle(app, "inline_title", "Inline title", "Show the note's title at its head.",
                    &set->inlineTitle);
        pane_rule();
        pane_select("properties", "Properties in document",
                    "Show the modified date and tags above the text.", &set->properties,
                    PROPS_ITEMS, PROPS_COUNT);
    }
    pane_section("Text");
    pane_card() {
        pane_slider("line_height", "Line height", "The space a line of text takes up.",
                    &set->lineHeight, 1.2f, 2.0f, 0.1f, rcFormat(a, "%.1f", set->lineHeight),
                    false);
        pane_rule();
        pane_toggle(app, "accent_headings", "Accent headings", "Draw headings in the accent color.",
                    &set->accentHeadings);
        pane_rule();
        pane_toggle(app, "dim_done", "Dim finished tasks", "Fade a task once its box is checked.",
                    &set->dimDone);
    }
}

static inline void page_appearance(RC_App *app, AppState *st) {
    Settings *set = &st->set;
    RC_Arena *a   = rcAppArena(app);
    int       zoomPick = zoom_nearest(rcWindowZoom(rcAppMainWindow(app)));

    page_preview(app, st);
    pane_section("Theme");
    pane_card() {
        pane_select("scheme", "Base color scheme", "Dark, light, or whatever the system says.",
                    &set->scheme, SCHEME_ITEMS, SCHEME_COUNT);
        pane_rule();
        pane_accent(app, st, "Accent color", "Toggles, links, selection and primary buttons.");
        pane_rule();
        pane_slider("radius", "Corner radius", "How round every card and control is.",
                    &set->radius, 0.0f, 16.0f, 2.0f, rcFormat(a, "%d px", (int)set->radius), false);
    }
    pane_section("Font");
    pane_card() {
        pane_slider("font_size", "Font size", "The size of the text in a note.", &set->fontStop,
                    0.0f, (float)(FONT_STOP_COUNT - 1), 1.0f,
                    rcFormat(a, "%d px", FONT_STOPS[(int)set->fontStop]), false);
    }
    pane_section("Interface");
    pane_card() {
        if (pane_select("zoom", "Zoom level", "The size of everything in the window.", &zoomPick,
                        ZOOM_ITEMS, ZOOM_COUNT))
            rcWindowSetZoom(rcAppMainWindow(app), ZOOM_FACTOR[zoomPick]);
        pane_rule();
        pane_toggle(app, "ribbon", "Show ribbon",
                    "The strip of one-click buttons by the file list.", &set->ribbon);
        pane_rule();
        pane_toggle(app, "status_bar", "Show status bar", "The line of counts under a note.",
                    &set->statusBar);
        pane_rule();
        pane_toggle(app, "compact_files", "Compact file list", "Fit more notes on the screen.",
                    &set->compactFiles);
    }
}

static inline char lower(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

static inline bool contains_nocase(const char *text, const char *needle) {
    if (!needle[0]) return true;
    for (; *text; text++) {
        const char *t = text, *n = needle;
        for (; *t && *n && lower(*t) == lower(*n); t++, n++) {}
        if (!*n) return true;
    }
    return false;
}

/* The chord as a keycap, or the word Blank where a command has none. */
static inline void hotkey_pill(RC_App *app, Chord chord) {
    RC_Style s = rcGetStyle();

    if (chord.key == RC_KEY_NONE) {
        rcTextL("Blank", .color = s.textMuted, .size = T_HINT, .select = RC_SELECT_NONE);
    } else {
        rcBox(.px = 8, .py = 4, .className = "item bg-surface-alt border") {
            rcText(chord_text(rcAppArena(app), chord), .className = "text-nowrap", .color = s.text,
                   .size = T_CHIP, .select = RC_SELECT_NONE);
        }
    }
}

/* An action slot holds its place while empty, so the pills line up down the page. */
static inline void hotkey_slot(void) {
    rcBox(.wType = RC_PX(icon_button_size()), .hType = RC_PX(icon_button_size())) {}
}

static inline void capture_begin(AppState *st, int command) {
    Chord none = { RC_KEY_NONE, false, false, false };

    st->capture.open     = true;
    st->capture.command  = command;
    st->capture.pending  = none;
    st->capture.conflict = -1;
    st->capture.refusal  = NULL;
    rcSetFocus(NULL);
}

static inline void hotkey_row(RC_App *app, AppState *st, int command) {
    RC_Arena *a       = rcAppArena(app);
    Chord     chord   = st->set.hotkeys[command];
    Chord     none    = { RC_KEY_NONE, false, false, false };
    bool      changed = !chord_equal(chord, COMMANDS[command].chord);
    bool      narrow  = pane_is_narrow();

    pane_row_stacking(narrow) {
        pane_words(COMMANDS[command].name, COMMANDS[command].hint, NULL);
        /* Beside the words an empty action holds its place, so the pills line up down the
           page. Under the words the pill leads, with the actions at the far end. */
        rcRow(.gap = 4, .align = "cl", .wType = narrow ? RC_GROW : RC_FIT) {
            if (narrow) {
                hotkey_pill(app, chord);
                rcSpacer() {}
            }
            if (changed) {
                if (icon_button(rcFormat(a, "hk_reset_%d", command).chars, rcIconRotateCw,
                                "Restore default"))
                    st->set.hotkeys[command] = COMMANDS[command].chord;
            } else if (!narrow) {
                hotkey_slot();
            }
            if (!narrow) hotkey_pill(app, chord);
            if (chord.key != RC_KEY_NONE) {
                if (icon_button(rcFormat(a, "hk_clear_%d", command).chars, rcIconX,
                                "Remove hotkey"))
                    st->set.hotkeys[command] = none;
            } else if (!narrow) {
                hotkey_slot();
            }
            if (icon_button(rcFormat(a, "hk_set_%d", command).chars, rcIconPlus, "Set hotkey"))
                capture_begin(st, command);
        }
    }
}

static inline void page_hotkeys(RC_App *app, AppState *st) {
    RC_Style s      = rcGetStyle();
    bool     finger = rcPointerIsCoarse();
    int      shown  = 0, i;

    /* The field's clear x empties the filter inside the call, so the list below sees it
       this frame. */
    rcRow(.gap = 12, .align = "cl", .w = "grow") {
        rcBox(.w = "grow") {
            rcTextInput("hotkey_filter", st->filter, FILTER_CAP,
                        .className = finger ? "h-11" : "h-8", .placeholder = "Filter commands",
                        .leadingIcon = rcIconSearch, .clearButton = true);
        }
        if (rcButton("hotkeys_reset", "Restore defaults", .className = control_class()))
            settings_defaults_hotkeys(&st->set);
    }
    if (rcPointerIsCoarse())
        rcTextL("Hotkeys work with a hardware keyboard.", .color = s.textMuted, .size = T_HINT);
    pane_card() {
        for (i = 0; i < CMD_COUNT; i++) {
            if (!contains_nocase(COMMANDS[i].name, st->filter)) continue;
            if (i == CMD_READABLE && !line_width_applies()) continue;
            if (shown++) pane_rule();
            hotkey_row(app, st, i);
        }
        if (!shown) {
            rcColumn(.gap = 12, .py = 32, .align = "cc", .w = "grow") {
                rcText(rcFormat(rcAppArena(app), "No command matches \"%s\".", st->filter),
                       .color = s.textMuted, .size = T_BODY);
                if (rcButton("hotkeys_unfilter", "Clear filter", .className = control_class()))
                    st->filter[0] = '\0';
            }
        }
    }
}

/* The verdicts are live and never block: an account holds what was typed, and says so. */
static inline bool email_ok(const char *s) {
    const char *at = NULL, *p;

    for (p = s; *p; p++)
        if (*p == '@') { if (at) return false; at = p; }
    if (!at || at == s || !at[1]) return false;
    for (p = at + 1; *p; p++)
        if (*p == '.' && p > at + 1 && p[1]) return true;
    return false;
}

static inline bool username_ok(const char *s) {
    for (; *s; s++)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') ||
              *s == '_'))
            return false;
    return true;
}

static inline bool phone_ok(const char *s) {
    for (; *s; s++)
        if (!((*s >= '0' && *s <= '9') || *s == ' ' || *s == '+' || *s == '-')) return false;
    return true;
}

/* Characters, not bytes: every byte that does not continue a UTF-8 sequence. */
static inline int char_count(const char *s) {
    int n = 0;

    for (; *s; s++)
        if (((unsigned char)*s & 0xC0u) != 0x80u) n++;
    return n;
}

/* EACH FIELD NAMES ITS KEYBOARD. .inputKind is what a phone raises when the field takes
   focus: Email's has the @ key, Username's no automatic capitals, Phone's the phone pad
   with its +, Website's the / and the .com key. The soft keyboard rises and falls by
   itself; the layout's foot grows to its height (app.h), and the library scrolls the
   focused caret clear of it. */
static inline void page_account(RC_App *app, AppState *st) {
    Settings *set  = &st->set;
    RC_Style  s    = rcGetStyle();
    int       bio  = char_count(set->bio);
    bool      web  = !set->website[0] || str_starts(set->website, "https://") ||
                     str_starts(set->website, "http://");

    pane_card() {
        rcRow(.gap = 16, .px = 8, .py = 12, .align = "cl", .w = "grow") {
            profile_avatar(app, set->name, 56.0f, T_TITLE);
            rcColumn(.gap = 2, .w = "grow", .className = "items-stretch") {
                rcTextC(set->name[0] ? set->name : "No name yet",
                        .font = rcFontSemibold((float)T_TITLE), .color = s.text);
                rcTextC(set->email[0] ? set->email : "No email yet", .color = s.textMuted,
                        .size = T_HINT);
            }
        }
    }
    pane_section("Profile");
    pane_card() {
        pane_text("acct_name", "Display name", "Shown at the foot of the file list.", set->name,
                  FIELD_CAP, "Your name", RC_INPUT_TEXT,
                  set->name[0] ? NULL : "A display name is required.");
        pane_rule();
        pane_text("acct_email", "Email", "Where receipts and sign-in codes go.", set->email,
                  FIELD_CAP, "you@example.com", RC_INPUT_EMAIL,
                  email_ok(set->email) ? NULL : "Enter an address such as you@example.com.");
        pane_rule();
        pane_text("acct_user", "Username", "Letters, digits and underscores.", set->username,
                  FIELD_CAP, "your_name", RC_INPUT_USERNAME,
                  username_ok(set->username) ? NULL : "Only letters, digits and underscores.");
        pane_rule();
        pane_text("acct_phone", "Phone", "In case we need to verify a sign-in.", set->phone,
                  FIELD_CAP, "+1 202 555 0100", RC_INPUT_TEL,
                  phone_ok(set->phone) ? NULL : "Digits, spaces, + and - only.");
        pane_rule();
        pane_text("acct_web", "Website", "Linked from your profile.", set->website, FIELD_CAP,
                  "https://example.com", RC_INPUT_URL,
                  web ? NULL : "Start the address with https://.");
    }
    pane_section("About you");
    pane_card() {
        rcColumn(.gap = 8, .px = 8, .py = 12, .w = "grow", .className = "items-stretch") {
            rcTextArea("acct_bio", set->bio, BIO_CAP, .placeholder = "A line or two about you",
                       .rows = 3);
            rcText(rcFormat(rcAppArena(app), "%d of %d characters", bio, BIO_MAX),
                   .color = bio > BIO_MAX ? s.danger : s.textMuted, .size = T_HINT,
                   .select = RC_SELECT_NONE);
        }
    }
}

static inline void page_plugins(RC_App *app, AppState *st) {
    Settings *set = &st->set;
    RC_Style  s   = rcGetStyle();
    int       on  = set->dailyNotes + set->outline + set->randomNote + set->readingTime +
                    set->wordCount;
    /* A plugin that adds to a part of the window that is turned off says what to turn on. */
    const char *no_ribbon = "Turn on Show ribbon, under Appearance, to see its button.";
    const char *no_bar    = "Turn on Show status bar, under Appearance, to see its count.";

    rcBox(.pl = 16, .w = "grow") {
        rcText(rcFormat(rcAppArena(app), "%d of 5 enabled", on), .color = s.textMuted,
               .size = T_HINT, .select = RC_SELECT_NONE);
    }
    pane_card() {
        pane_toggle(app, "plugin_daily", "Daily notes",
                    set->ribbon ? "A ribbon button that opens the newest note in Journal."
                                : no_ribbon, &set->dailyNotes);
        pane_rule();
        pane_toggle(app, "plugin_outline", "Outline",
                    "List a note's headings, and jump to one with a click.",
                    &set->outline);
        pane_rule();
        pane_toggle(app, "plugin_random", "Random note",
                    set->ribbon ? "A ribbon button that opens any note you do not have up."
                                : no_ribbon, &set->randomNote);
        pane_rule();
        pane_toggle(app, "plugin_time", "Reading time",
                    set->statusBar ? "Estimate minutes to read in the status bar, at 200 words "
                                     "a minute." : no_bar, &set->readingTime);
        pane_rule();
        pane_toggle(app, "plugin_words", "Word count",
                    set->statusBar ? "Count a note's words and characters in the status bar."
                                   : no_bar, &set->wordCount);
    }
}

/* One field of rcUpdateResult, under its own name: what Developer options is for. */
static inline void update_field(const char *name, RC_String value) {
    RC_Style s = rcGetStyle();

    rcRow(.gap = 12, .py = 4, .align = "tl", .w = "grow") {
        rcBox(.wType = RC_PX(84.0f)) {
            rcTextC(name, .color = s.textMuted, .size = T_HINT, .select = RC_SELECT_NONE);
        }
        rcBox(.w = "grow", .className = "items-stretch") {
            rcText(value, .color = s.text, .size = T_HINT, .wordBreak = "b");
        }
    }
}

static inline void update_readout(RC_App *app, const Updater *up) {
    RC_Arena       *a = rcAppArena(app);
    RC_UpdateResult u = up->u;

    update_field(".state", rcStringFromCStr(UPDATE_STATE_NAME[u.state]));
    update_field(".action", rcStringFromCStr(UPDATE_ACTION_NAME[u.action]));
    update_field(".version", rcStringFromCStr(u.version && u.version[0] ? u.version : "-"));
    update_field(".required", rcStringFromCStr(u.required ? "true" : "false"));
    update_field(".notes", rcStringFromCStr(u.notes && u.notes[0] ? u.notes : "-"));
    update_field(".url", rcStringFromCStr(u.url && u.url[0] ? u.url : "-"));
    update_field(".error", rcStringFromCStr(UPDATE_ERROR_NAME[u.error]));
    update_field(".message", rcStringFromCStr(u.message ? u.message : "-"));
    if (u.state == RC_UPDATE_DOWNLOADING)
        update_field(".received", rcFormat(a, "%d of %d KB", (int)(u.received / 1024u),
                                           (int)(u.size / 1024u)));
}

/* What the check asks and with which key, the states serve.py can show on purpose, and
   the library's answer read back field by field. */
static inline void developer_options(RC_App *app, AppState *st) {
    RC_Style s   = rcGetStyle();
    Palette  pal = app_palette();
    Updater *up  = &st->update;

    pane_rule();
    pane_text("update_url", "Manifest address", "Plain http is accepted to this machine only.",
              up->url, (int)sizeof up->url, DEV_MANIFEST, RC_INPUT_URL, NULL);
    pane_rule();
    pane_text("update_key", "Public key", "Example only: paste the key serve.py printed.",
              up->key, (int)sizeof up->key, "No key: prompt-only", RC_INPUT_TEXT, NULL);
    pane_rule();
    pane_select("update_knob", "Simulate", "Ask serve.py for each state on purpose.", &up->knob,
                KNOB_LABEL, KNOB_COUNT);
    pane_rule();
    rcColumn(.gap = 8, .px = 8, .py = 12, .w = "grow", .className = "items-stretch") {
        rcTextL("python3 examples/ex13_user_preferences/serve.py --build <this program>",
                .color = s.textMuted, .size = T_HINT, .wordBreak = "b");
        rcTextC(up->keyed ? "Signed: the manifest must verify under the key and name " APP_ID "."
                          : "Prompt-only: no signature is checked, so a changed manifest still "
                            "reads as an offer.",
                .color = up->keyed ? pal.goodInk : pal.warnInk, .size = T_HINT);
    }
    pane_rule();
    rcColumn(.px = 8, .py = 8, .w = "grow") {
        update_readout(app, up);
    }
}

static inline void page_about(RC_App *app, AppState *st) {
    RC_Style s  = rcGetStyle();
    Updater *up = &st->update;

    pane_card() {
        pane_row_stacking(pane_is_narrow()) {
            rcRow(.gap = 16, .align = "cl", .w = "grow") {
                rcIconRayClayLogo(48.0f);
                rcColumn(.gap = 2, .w = "grow", .className = "items-stretch") {
                    rcTextL("RayClay Preferences", .font = rcFontSemibold((float)T_HEAD),
                            .color = s.text);
                    rcText(rcFormat(rcAppArena(app), "Version %s", RUNNING_VERSION),
                           .color = s.textMuted, .size = T_HINT);
                    rcText(update_status(app, up), .color = update_status_color(up),
                           .size = T_HINT);
                    if (up->note[0]) rcTextC(up->note, .color = s.textMuted, .size = T_HINT);
                }
            }
            update_button(app, up);
        }
    }
    update_banner(app, up);
    pane_card() {
        automatic_updates_row(app, st);
        pane_rule();
        if (pane_button("rayclay_site", "Made with RayClay " RC_VERSION,
                        "A C framework for desktop, mobile, and the web.", "Open website",
                        RC_BTN_DEFAULT))
            rcOpenUrl("https://github.com/impizulu/rayclay");
    }
    pane_section("Developer");
    pane_card() {
        pane_toggle(app, "developer", "Developer options",
                    "Point the check at serve.py and show each of its answers.", &st->developer);
        if (st->developer) developer_options(app, st);
    }
}

typedef struct {
    const char     *id;
    const char     *label;
    const char     *line;        /* what the page holds: the one-column root shows it */
    RC_IconCallback icon;
    void          (*draw)(RC_App *app, AppState *st);
} Page;

static const Page PAGES[PAGE_COUNT] = {
    { "cat_general", "General", "Updates, startup and your settings file",
      rcIconSettings, page_general },
    { "cat_reader", "Reader", "Line height, headings and what a note shows",
      rcIconFileText, page_reader },
    { "cat_appearance", "Appearance", "Theme, accent color, font and zoom",
      rcIconContrastFilled, page_appearance },
    { "cat_hotkeys", "Hotkeys", "The chords of a hardware keyboard",
      rcIconHash, page_hotkeys },
    { "cat_account", "Account", "Your name, email and profile",
      rcIconUser, page_account },
    { "cat_plugins", "Core plugins", "Daily notes, Outline, Word count and more",
      rcIconGalleryVerticalEnd, page_plugins },
    { "cat_about", "About", "Version, updates and developer options",
      rcIconInfo, page_about },
};

#endif /* APP_PAGES_H */
