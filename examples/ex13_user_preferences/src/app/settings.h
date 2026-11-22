/*
    settings.h - what the user chose, and the file that remembers it

    One struct holds every preference, the controls write straight into it, and
    settings.json under rcAppDataPath keeps it between runs. A missing or damaged
    file, or a key an older build did not write, simply reads as the default, so
    a new setting needs one line here and nothing else.

    NOTHING SAYS "THIS CHANGED". update() compares the struct with the copy it last
    saved, which is what lets every control in the app stay a one-liner: see
    settings_autosave in app.h.
*/
#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include "rayclay.h"
#include "app/hotkeys.h"

#define SETTINGS_ORG  "RayClay"
#define SETTINGS_APP  "Preferences"
#define SETTINGS_FILE "settings.json"

#define ACCENT_DEFAULT 0x8a5cf5u

typedef enum { SCHEME_DARK = 0, SCHEME_LIGHT, SCHEME_SYSTEM, SCHEME_COUNT } Scheme;
typedef enum { DATE_ISO = 0, DATE_US, DATE_EU, DATE_COUNT } DateFormat;
typedef enum { PROPS_VISIBLE = 0, PROPS_HIDDEN, PROPS_COUNT } PropsMode;

enum { FIELD_CAP = 64, BIO_MAX = 240, BIO_CAP = 4 * BIO_MAX + 1 };

/* The Font size slider's stops. A few real sizes, because each one a note draws is
   baked once and kept: a slider that spent a size per pixel would fill the font table. */
static const int FONT_STOPS[] = { 14, 15, 16, 18, 20 };
enum { FONT_STOP_COUNT = (int)(sizeof FONT_STOPS / sizeof FONT_STOPS[0]) };

typedef struct {
    /* General */
    bool  openAtStart;
    bool  reopenLast;
    int   dateFormat;
    int   lastNote;
    /* Reader */
    bool  readable;
    float lineWidth;
    bool  inlineTitle;
    int   properties;
    bool  accentHeadings;
    bool  dimDone;
    float lineHeight;
    /* Appearance */
    int      scheme;
    uint32_t accent;      /* 0xRRGGBB */
    float    radius;
    float    fontStop;    /* an index into FONT_STOPS, as the slider holds it */
    float    zoom;
    bool     ribbon;
    bool     statusBar;
    bool     compactFiles;
    /* Core plugins */
    bool dailyNotes;
    bool outline;
    bool randomNote;
    bool readingTime;
    bool wordCount;
    /* Account */
    char name[FIELD_CAP];
    char email[FIELD_CAP];
    char username[FIELD_CAP];
    char phone[FIELD_CAP];
    char website[FIELD_CAP];
    char bio[BIO_CAP];
    /* Hotkeys */
    Chord hotkeys[CMD_COUNT];
    /* About */
    bool autoCheck;
} Settings;

static inline void settings_defaults_hotkeys(Settings *set) {
    int i;

    for (i = 0; i < CMD_COUNT; i++)
        set->hotkeys[i] = COMMANDS[i].chord;
}

/* Everything but the account, which is the user's words and not a preference. */
static inline void settings_defaults_options(Settings *set) {
    set->openAtStart    = true;
    set->reopenLast     = true;
    set->dateFormat     = DATE_US;
    set->readable       = true;
    set->lineWidth      = 680.0f;
    set->inlineTitle    = true;
    set->properties     = PROPS_VISIBLE;
    set->accentHeadings = false;
    set->dimDone        = true;
    set->lineHeight     = 1.5f;
    set->scheme         = SCHEME_DARK;
    set->accent         = ACCENT_DEFAULT;
    set->radius         = 8.0f;
    set->fontStop       = 2.0f;
    set->zoom           = 1.0f;
    set->ribbon         = true;
    set->statusBar      = true;
    set->compactFiles   = false;
    set->dailyNotes     = true;
    set->outline        = false;
    set->randomNote     = false;
    set->readingTime    = false;
    set->wordCount      = true;
    set->autoCheck      = false;
    settings_defaults_hotkeys(set);
}

static inline void settings_defaults(Settings *set) {
    settings_defaults_options(set);
    set->lastNote = 0;
    rcStrCopy(set->name, "Maya Okafor", sizeof set->name);
    rcStrCopy(set->email, "maya.okafor@example.com", sizeof set->email);
    rcStrCopy(set->username, "mayao", sizeof set->username);
    rcStrCopy(set->phone, "+1 202 555 0147", sizeof set->phone);
    rcStrCopy(set->website, "https://example.com/maya", sizeof set->website);
    rcStrCopy(set->bio, "Structural engineer. Field notes, old bridges and too much espresso.",
              sizeof set->bio);
}

/* The file's whole path into @p out; false when this platform gave the app no folder. */
static inline bool settings_path(char *out, int cap) {
    const char *dir = rcAppDataPath(SETTINGS_ORG, SETTINGS_APP);
    int         n;

    if (!dir) return false;
    rcStrCopy(out, dir, (size_t)cap);
    n = rcStringFromCStr(out).length;
    rcStrCopy(out + n, SETTINGS_FILE, (size_t)(cap - n));
    return true;
}

/* A clamp on each number read: a hand-edited file must not reach a slider or a table. */
static inline float clamp_float(double v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : (float)v;
}

static inline int clamp_int(double v, int count) {
    return v < 0 ? 0 : v >= count ? count - 1 : (int)v;
}

static inline void settings_load(Settings *set) {
    void        *bytes = NULL;
    char         path[1024];
    size_t       length = 0;
    RC_Json      doc;
    int          i;

    settings_defaults(set);
    if (!settings_path(path, (int)sizeof path)) return;
    if (rcLoadFile(path, &bytes, &length) != RC_FILE_OK) return;
    doc = rcJsonParse((const char *)bytes, length);

    set->openAtStart    = rcJsonBool(doc, "openAtStart", set->openAtStart);
    set->reopenLast     = rcJsonBool(doc, "reopenLast", set->reopenLast);
    set->dateFormat     = clamp_int(rcJsonNumber(doc, "dateFormat", set->dateFormat), DATE_COUNT);
    set->lastNote       = clamp_int(rcJsonNumber(doc, "lastNote", set->lastNote), 1024);
    set->readable       = rcJsonBool(doc, "readable", set->readable);
    set->lineWidth      = clamp_float(rcJsonNumber(doc, "lineWidth", set->lineWidth), 480, 960);
    set->inlineTitle    = rcJsonBool(doc, "inlineTitle", set->inlineTitle);
    set->properties     = clamp_int(rcJsonNumber(doc, "properties", set->properties), PROPS_COUNT);
    set->accentHeadings = rcJsonBool(doc, "accentHeadings", set->accentHeadings);
    set->dimDone        = rcJsonBool(doc, "dimDone", set->dimDone);
    set->lineHeight     = clamp_float(rcJsonNumber(doc, "lineHeight", set->lineHeight), 1.2f, 2.0f);
    set->scheme         = clamp_int(rcJsonNumber(doc, "scheme", set->scheme), SCHEME_COUNT);
    set->accent         = (uint32_t)rcJsonUint64(doc, "accent", set->accent) & 0xffffffu;
    set->radius         = clamp_float(rcJsonNumber(doc, "radius", set->radius), 0, 16);
    set->fontStop       = clamp_float(rcJsonNumber(doc, "fontStop", set->fontStop), 0,
                                      (float)(FONT_STOP_COUNT - 1));
    set->zoom           = clamp_float(rcJsonNumber(doc, "zoom", set->zoom), 0.5f, 3.0f);
    set->ribbon         = rcJsonBool(doc, "ribbon", set->ribbon);
    set->statusBar      = rcJsonBool(doc, "statusBar", set->statusBar);
    set->compactFiles   = rcJsonBool(doc, "compactFiles", set->compactFiles);
    set->dailyNotes     = rcJsonBool(doc, "dailyNotes", set->dailyNotes);
    set->outline        = rcJsonBool(doc, "outline", set->outline);
    set->randomNote     = rcJsonBool(doc, "randomNote", set->randomNote);
    set->readingTime    = rcJsonBool(doc, "readingTime", set->readingTime);
    set->wordCount      = rcJsonBool(doc, "wordCount", set->wordCount);
    set->autoCheck      = rcJsonBool(doc, "autoCheck", set->autoCheck);
    rcStrCopy(set->name, rcJsonString(doc, "name", set->name), sizeof set->name);
    rcStrCopy(set->email, rcJsonString(doc, "email", set->email), sizeof set->email);
    rcStrCopy(set->username, rcJsonString(doc, "username", set->username), sizeof set->username);
    rcStrCopy(set->phone, rcJsonString(doc, "phone", set->phone), sizeof set->phone);
    rcStrCopy(set->website, rcJsonString(doc, "website", set->website), sizeof set->website);
    rcStrCopy(set->bio, rcJsonString(doc, "bio", set->bio), sizeof set->bio);
    /* A command the file does not name keeps its default; "" is a hotkey taken away. */
    for (i = 0; i < CMD_COUNT; i++) {
        const char *chord = rcJsonString(rcJsonGet(doc, "hotkeys"), COMMANDS[i].store, NULL);
        if (chord) set->hotkeys[i] = chord_parse(chord);
    }
    rcJsonFree(doc);
    rcFreeFile(bytes);
}

/* rcSaveFile replaces the file in one step, so a crash leaves the old settings or the
   new, never half of them. */
static inline RC_FileStatus settings_save(const Settings *set) {
    /* Room for the worst a loaded file can leave, under 9 KB: every byte of every text
       field a control character, which JSON writes as a six-byte escape. */
    static char   text[16384];
    char          path[1024];
    char          chord[64];
    RC_JsonWriter w = rcJsonWriter(text, sizeof text);
    const char   *json;
    int           i;

    if (!settings_path(path, (int)sizeof path)) return RC_FILE_UNSUPPORTED;
    rcJsonBeginObject(&w, NULL);
    rcJsonWriteBool(&w, "openAtStart", set->openAtStart);
    rcJsonWriteBool(&w, "reopenLast", set->reopenLast);
    rcJsonWriteNumber(&w, "dateFormat", set->dateFormat);
    rcJsonWriteNumber(&w, "lastNote", set->lastNote);
    rcJsonWriteBool(&w, "readable", set->readable);
    rcJsonWriteNumber(&w, "lineWidth", set->lineWidth);
    rcJsonWriteBool(&w, "inlineTitle", set->inlineTitle);
    rcJsonWriteNumber(&w, "properties", set->properties);
    rcJsonWriteBool(&w, "accentHeadings", set->accentHeadings);
    rcJsonWriteBool(&w, "dimDone", set->dimDone);
    rcJsonWriteNumber(&w, "lineHeight", set->lineHeight);
    rcJsonWriteNumber(&w, "scheme", set->scheme);
    rcJsonWriteUint64(&w, "accent", set->accent);
    rcJsonWriteNumber(&w, "radius", set->radius);
    rcJsonWriteNumber(&w, "fontStop", set->fontStop);
    rcJsonWriteNumber(&w, "zoom", set->zoom);
    rcJsonWriteBool(&w, "ribbon", set->ribbon);
    rcJsonWriteBool(&w, "statusBar", set->statusBar);
    rcJsonWriteBool(&w, "compactFiles", set->compactFiles);
    rcJsonWriteBool(&w, "dailyNotes", set->dailyNotes);
    rcJsonWriteBool(&w, "outline", set->outline);
    rcJsonWriteBool(&w, "randomNote", set->randomNote);
    rcJsonWriteBool(&w, "readingTime", set->readingTime);
    rcJsonWriteBool(&w, "wordCount", set->wordCount);
    rcJsonWriteBool(&w, "autoCheck", set->autoCheck);
    rcJsonWriteString(&w, "name", set->name);
    rcJsonWriteString(&w, "email", set->email);
    rcJsonWriteString(&w, "username", set->username);
    rcJsonWriteString(&w, "phone", set->phone);
    rcJsonWriteString(&w, "website", set->website);
    rcJsonWriteString(&w, "bio", set->bio);
    rcJsonBeginObject(&w, "hotkeys");
    for (i = 0; i < CMD_COUNT; i++) {
        chord_store(chord, sizeof chord, set->hotkeys[i]);
        rcJsonWriteString(&w, COMMANDS[i].store, chord);
    }
    rcJsonEnd(&w);
    rcJsonEnd(&w);
    json = rcJsonWriterFinish(&w);
    if (!json) return RC_FILE_IO;
    return rcSaveFile(path, json, w.len, RC_FILE_PRIVATE);
}

#endif /* APP_SETTINGS_H */
