/*
    hotkeys.h - the commands, their chords, and the keys a chord can use

    A chord is a key and its modifiers. RC_MOD_PRIMARY is Cmd on Apple and Ctrl
    elsewhere, so one saved chord means the same thing on every machine.

    A CHORD IS SAVED BY NAME ("Primary+Shift+K"), not by its RC_KEY value (which
    rayclay.h fixes, so either would load): a settings file a person reads and edits.
*/
#ifndef APP_HOTKEYS_H
#define APP_HOTKEYS_H

#include "rayclay.h"
#include "platform/platform.h"

typedef struct {
    RC_Key key;      /* RC_KEY_NONE = no hotkey */
    bool   primary;
    bool   shift;
    bool   alt;
} Chord;

typedef enum {
    CMD_SETTINGS = 0, CMD_SIDEBAR, CMD_NEXT_NOTE, CMD_PREV_NOTE, CMD_FONT_UP, CMD_FONT_DOWN,
    CMD_THEME, CMD_READABLE, CMD_STATUS_BAR, CMD_OUTLINE, CMD_UPDATE, CMD_COUNT
} Command;

typedef struct {
    const char *store;   /* its key in settings.json */
    const char *name;
    const char *hint;
    Chord       chord;   /* its default */
} CommandInfo;

static const CommandInfo COMMANDS[CMD_COUNT] = {
    { "open_settings", "Open settings", "Show or close Settings",
      { RC_KEY_COMMA, true, false, false } },
    { "toggle_sidebar", "Toggle file list", "Show or hide the list of notes",
      { RC_KEY_B, true, false, false } },
    { "next_note", "Next tab", "Move to the tab on the right",
      { RC_KEY_RIGHT, true, false, true } },
    { "previous_note", "Previous tab", "Move to the tab on the left",
      { RC_KEY_LEFT, true, false, true } },
    { "font_larger", "Increase font size", "One step up the Font size slider",
      { RC_KEY_PERIOD, true, true, false } },
    { "font_smaller", "Decrease font size", "One step down the Font size slider",
      { RC_KEY_COMMA, true, true, false } },
    { "toggle_theme", "Switch light and dark", "Flip the base color scheme",
      { RC_KEY_L, true, true, false } },
    { "toggle_readable", "Toggle readable line length", "Limit notes to the maximum width",
      { RC_KEY_NONE, false, false, false } },
    { "toggle_status_bar", "Toggle status bar", "Show or hide the bar under a note",
      { RC_KEY_NONE, false, false, false } },
    { "toggle_outline", "Toggle outline", "Turn the Outline core plugin on or off",
      { RC_KEY_O, true, true, false } },
    { "check_updates", "Check for updates", "Ask the update server for a newer build",
      { RC_KEY_NONE, false, false, false } },
};

/* Every key a hotkey may use, and its name both on screen and in settings.json. */
static const struct {
    RC_Key      key;
    const char *name;
} KEY_NAMES[] = {
    { RC_KEY_A, "A" }, { RC_KEY_B, "B" }, { RC_KEY_C, "C" }, { RC_KEY_D, "D" }, { RC_KEY_E, "E" },
    { RC_KEY_F, "F" }, { RC_KEY_G, "G" }, { RC_KEY_H, "H" }, { RC_KEY_I, "I" }, { RC_KEY_J, "J" },
    { RC_KEY_K, "K" }, { RC_KEY_L, "L" }, { RC_KEY_M, "M" }, { RC_KEY_N, "N" }, { RC_KEY_O, "O" },
    { RC_KEY_P, "P" }, { RC_KEY_Q, "Q" }, { RC_KEY_R, "R" }, { RC_KEY_S, "S" }, { RC_KEY_T, "T" },
    { RC_KEY_U, "U" }, { RC_KEY_V, "V" }, { RC_KEY_W, "W" }, { RC_KEY_X, "X" }, { RC_KEY_Y, "Y" },
    { RC_KEY_Z, "Z" }, { RC_KEY_0, "0" }, { RC_KEY_1, "1" }, { RC_KEY_2, "2" }, { RC_KEY_3, "3" },
    { RC_KEY_4, "4" }, { RC_KEY_5, "5" }, { RC_KEY_6, "6" }, { RC_KEY_7, "7" }, { RC_KEY_8, "8" },
    { RC_KEY_9, "9" }, { RC_KEY_F1, "F1" }, { RC_KEY_F2, "F2" }, { RC_KEY_F3, "F3" },
    { RC_KEY_F4, "F4" }, { RC_KEY_F5, "F5" }, { RC_KEY_F6, "F6" }, { RC_KEY_F7, "F7" },
    { RC_KEY_F8, "F8" }, { RC_KEY_F9, "F9" }, { RC_KEY_F10, "F10" }, { RC_KEY_F11, "F11" },
    { RC_KEY_F12, "F12" }, { RC_KEY_LEFT, "Left" }, { RC_KEY_RIGHT, "Right" },
    { RC_KEY_UP, "Up" }, { RC_KEY_DOWN, "Down" }, { RC_KEY_HOME, "Home" }, { RC_KEY_END, "End" },
    { RC_KEY_PAGE_UP, "PageUp" }, { RC_KEY_PAGE_DOWN, "PageDown" }, { RC_KEY_INSERT, "Insert" },
    { RC_KEY_DELETE, "Delete" }, { RC_KEY_BACKSPACE, "Backspace" }, { RC_KEY_TAB, "Tab" },
    { RC_KEY_ENTER, "Enter" }, { RC_KEY_SPACE, "Space" }, { RC_KEY_COMMA, "," },
    { RC_KEY_PERIOD, "." }, { RC_KEY_SLASH, "/" }, { RC_KEY_SEMICOLON, ";" },
    { RC_KEY_APOSTROPHE, "'" }, { RC_KEY_MINUS, "-" }, { RC_KEY_EQUAL, "=" },
    { RC_KEY_LEFT_BRACKET, "[" }, { RC_KEY_RIGHT_BRACKET, "]" }, { RC_KEY_BACKSLASH, "\\" },
    { RC_KEY_GRAVE, "`" },
};
enum { KEY_NAME_COUNT = (int)(sizeof KEY_NAMES / sizeof KEY_NAMES[0]) };

static inline bool str_eq(const char *a, const char *b) {
    for (; *a && *a == *b; a++, b++) {}
    return *a == *b;
}

static inline bool str_starts(const char *s, const char *prefix) {
    for (; *prefix; s++, prefix++)
        if (*s != *prefix) return false;
    return true;
}

static inline const char *key_name(RC_Key key) {
    int i;

    for (i = 0; i < KEY_NAME_COUNT; i++)
        if (KEY_NAMES[i].key == key) return KEY_NAMES[i].name;
    return "";
}

static inline bool chord_equal(Chord a, Chord b) {
    if (a.key != b.key) return false;
    return a.key == RC_KEY_NONE ||
           (a.primary == b.primary && a.shift == b.shift && a.alt == b.alt);
}

/* What the row and the capture dialog print: "Ctrl + Shift + K". */
static inline RC_String chord_text(RC_Arena *a, Chord c) {
    return rcFormat(a, "%s%s%s%s", c.primary ? PLATFORM_PRIMARY " + " : "",
                    c.shift ? "Shift + " : "", c.alt ? PLATFORM_ALT " + " : "", key_name(c.key));
}

static inline void str_add(char *dst, size_t cap, const char *src) {
    size_t n = (size_t)rcStringFromCStr(dst).length;

    rcStrCopy(dst + n, src, cap - n);
}

/* What settings.json holds: "Primary+Shift+K", or "" for no hotkey. */
static inline void chord_store(char *out, size_t cap, Chord c) {
    out[0] = '\0';
    if (c.key == RC_KEY_NONE) return;
    if (c.primary) str_add(out, cap, "Primary+");
    if (c.shift)   str_add(out, cap, "Shift+");
    if (c.alt)     str_add(out, cap, "Alt+");
    str_add(out, cap, key_name(c.key));
}

static inline Chord chord_parse(const char *text) {
    Chord c = { RC_KEY_NONE, false, false, false };
    int   i;

    if (str_starts(text, "Primary+")) { c.primary = true; text += 8; }
    if (str_starts(text, "Shift+"))   { c.shift = true;   text += 6; }
    if (str_starts(text, "Alt+"))     { c.alt = true;     text += 4; }
    for (i = 0; i < KEY_NAME_COUNT; i++)
        if (str_eq(KEY_NAMES[i].name, text)) c.key = KEY_NAMES[i].key;
    return c;
}

/* Exact modifiers: Ctrl+B must not also fire on Ctrl+Shift+B. */
static inline bool chord_fired(Chord c) {
    return c.key != RC_KEY_NONE && rcKeyPressed(c.key) && rcModDown(RC_MOD_PRIMARY) == c.primary &&
           rcModDown(RC_MOD_SHIFT) == c.shift && rcModDown(RC_MOD_ALT) == c.alt;
}

/* The chord pressed this frame, key RC_KEY_NONE when none was: the capture dialog's one
   read. Modifiers alone are not in KEY_NAMES, so holding Ctrl waits for the key. */
static inline Chord chord_pressed(void) {
    Chord c = { RC_KEY_NONE, false, false, false };
    int   i;

    for (i = 0; i < KEY_NAME_COUNT; i++)
        if (rcKeyPressed(KEY_NAMES[i].key)) c.key = KEY_NAMES[i].key;
    c.primary = rcModDown(RC_MOD_PRIMARY);
    c.shift   = rcModDown(RC_MOD_SHIFT);
    c.alt     = rcModDown(RC_MOD_ALT);
    return c;
}

/* Why this chord cannot be a hotkey, or NULL. A bare key would fire while typing, and a
   text field and the window's zoom own a few chords of their own. */
static inline const char *chord_refusal(Chord c) {
    bool function = c.key >= RC_KEY_F1 && c.key <= RC_KEY_F24;

    if (!c.primary && !c.alt && !function)
        return "Add " PLATFORM_PRIMARY " or " PLATFORM_ALT ": a plain key would fire while typing.";
    if (c.primary && !c.shift && !c.alt) {
        if (c.key == RC_KEY_EQUAL || c.key == RC_KEY_MINUS || c.key == RC_KEY_0)
            return "The window keeps this one for its zoom.";
        if (c.key == RC_KEY_A || c.key == RC_KEY_C || c.key == RC_KEY_V || c.key == RC_KEY_X ||
            c.key == RC_KEY_Z)
            return "Text fields keep this one for editing.";
    }
    return NULL;
}

#endif /* APP_HOTKEYS_H */
