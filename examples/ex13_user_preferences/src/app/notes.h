/*
    notes.h - the host app's content: a small vault of notes, as plain data

    The settings dialog is the example; these notes are what its settings act on, so a
    change of font size, line width, accent or date format has something real to change.
*/
#ifndef APP_NOTES_H
#define APP_NOTES_H

#include "rayclay.h"
#include "app/settings.h"

typedef enum { L_H2 = 0, L_P, L_BULLET, L_TASK, L_DONE, L_QUOTE } LineKind;

typedef struct {
    LineKind    kind;
    const char *text;
} NoteLine;

typedef enum { FOLDER_PROJECTS = 0, FOLDER_JOURNAL, FOLDER_REFERENCE, FOLDER_COUNT } Folder;

static const char *const FOLDER_NAME[FOLDER_COUNT] = { "Projects", "Journal", "Reference" };
#define NO_FOLDER (-1)

typedef struct {
    const char     *title;
    int             folder;
    int             year, month, day;
    const char     *tags;
    const NoteLine *lines;
    int             lineCount;
} Note;

#define LINES(table) table, (int)(sizeof table / sizeof table[0])

static const NoteLine NOTE_WELCOME[] = {
    { L_P, "This vault is a working copy of a field engineer's notes. It is here so the "
           "settings have something to change: change one, and this note follows." },
    { L_H2, "Where things are" },
    { L_BULLET, "Pick a note in the file list to open it." },
    { L_BULLET, "The gear at the top right opens Settings, and so does its hotkey." },
    { L_BULLET, "Everything you change is saved as you change it." },
    { L_H2, "Try this" },
    { L_TASK, "Pick a new accent color under Appearance." },
    { L_TASK, "Rebind Toggle file list under Hotkeys, then press your new chord." },
    { L_TASK, "Turn on the Outline core plugin and click a heading." },
    { L_DONE, "Open this note." },
};

static const NoteLine NOTE_BRIDGE[] = {
    { L_P, "Seismic retrofit of the 1962 Harbor Bridge approach spans. Twelve bents, "
           "two contractors, and one closure window the port will not move." },
    { L_QUOTE, "Night closures only between March and June. The port's container schedule "
               "wins every argument." },
    { L_H2, "Scope" },
    { L_BULLET, "Replace the rocker bearings on bents 4 to 9 with isolation bearings." },
    { L_BULLET, "Jacket the six tallest columns in steel, grouted to the footing." },
    { L_BULLET, "Add shear keys at the two expansion joints over Pier Road." },
    { L_H2, "Milestones" },
    { L_DONE, "Geotechnical borings at bents 6 and 7." },
    { L_DONE, "Bearing shop drawings approved." },
    { L_TASK, "Traffic control plan signed by the port authority." },
    { L_TASK, "First night closure, bent 4." },
    { L_H2, "Open questions" },
    { L_P, "Grout supply is quoted at nine weeks. If that slips past January the column "
           "jackets move into the summer window, and the night rate goes up by a fifth." },
    { L_P, "Ask Tomas whether the 1962 as-builts show the footing dowels at bent 8. The "
           "borings suggest the drawings and the ground disagree there." },
    { L_H2, "Budget" },
    { L_BULLET, "Bearings and jacking: $1.84 million." },
    { L_BULLET, "Column jackets: $2.36 million." },
    { L_BULLET, "Shear keys and joints: $0.62 million." },
    { L_BULLET, "Night closures and traffic control: $0.78 million." },
    { L_P, "That is $5.60 million against a $6.10 million allowance, which leaves $0.50 "
           "million of contingency for whatever bent 8 turns out to be." },
};

static const NoteLine NOTE_ROADMAP[] = {
    { L_P, "What the structures team commits to before the end of the year." },
    { L_H2, "October" },
    { L_DONE, "Inspection reports for the Pier Road culverts." },
    { L_DONE, "Hire a second load-rating engineer." },
    { L_H2, "November" },
    { L_TASK, "Load ratings for the eleven county bridges." },
    { L_TASK, "Move the inspection photos off the shared drive." },
    { L_H2, "December" },
    { L_TASK, "Capital plan draft to the county board by the 12th." },
    { L_TASK, "Winter inspection schedule." },
};

static const NoteLine NOTE_NOV5[] = {
    { L_P, "Walked bent 7 with Tomas at low tide. The concrete at the waterline is better "
           "than the 2019 report made it sound; spalling is local and the bars are clean." },
    { L_H2, "To do" },
    { L_DONE, "Send the bent 7 photos to the geotechnical team." },
    { L_TASK, "Call the grout supplier about the January date." },
    { L_QUOTE, "Tomas: the drawings say eight dowels. I will believe it when I see eight." },
};

static const NoteLine NOTE_NOV4[] = {
    { L_P, "Design review with the port. They accepted the traffic control concept but "
           "want a second detour sign at Pier Road and Fifth." },
    { L_H2, "Decisions" },
    { L_BULLET, "Closures start at 9:30 p.m., not 10." },
    { L_BULLET, "Weekly call moves to Tuesday mornings." },
};

static const NoteLine NOTE_READING[] = {
    { L_P, "Things worth reading twice." },
    { L_H2, "Bridges" },
    { L_DONE, "Seismic Retrofitting Manual for Highway Structures, part one." },
    { L_TASK, "Part two, on earthquake-resisting systems." },
    { L_H2, "Everything else" },
    { L_TASK, "The Control of Nature, the Atchafalaya chapter." },
    { L_TASK, "Why Buildings Fall Down." },
};

static const NoteLine NOTE_ESPRESSO[] = {
    { L_P, "One dose, three ratios, written down so nobody has to ask again." },
    { L_H2, "The house grinder" },
    { L_BULLET, "Ristretto: 18 g in, 27 g out, 22 seconds." },
    { L_BULLET, "Normale: 18 g in, 36 g out, 28 seconds." },
    { L_BULLET, "Lungo: 18 g in, 54 g out, 34 seconds." },
    { L_QUOTE, "Change one thing at a time, and change the grind before the dose." },
};

static const Note NOTES[] = {
    { "Welcome", NO_FOLDER, 2026, 11, 2, "start", LINES(NOTE_WELCOME) },
    { "Harbor Bridge retrofit", FOLDER_PROJECTS, 2026, 10, 28, "bridge seismic port",
      LINES(NOTE_BRIDGE) },
    { "Q4 roadmap", FOLDER_PROJECTS, 2026, 10, 19, "planning", LINES(NOTE_ROADMAP) },
    { "2026-11-05", FOLDER_JOURNAL, 2026, 11, 5, "field", LINES(NOTE_NOV5) },
    { "2026-11-04", FOLDER_JOURNAL, 2026, 11, 4, "meeting port", LINES(NOTE_NOV4) },
    { "Reading list", FOLDER_REFERENCE, 2026, 9, 14, "books", LINES(NOTE_READING) },
    { "Espresso ratios", FOLDER_REFERENCE, 2026, 8, 3, "coffee", LINES(NOTE_ESPRESSO) },
};
enum { NOTE_COUNT = (int)(sizeof NOTES / sizeof NOTES[0]), NOTE_NEWEST_JOURNAL = 3 };

/* The note's modified date, as the Date format setting says. */
static inline RC_String note_date(RC_Arena *a, const Note *n, int format) {
    static const char *const MONTH[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    if (format == DATE_US) return rcFormat(a, "%s %d, %d", MONTH[n->month - 1], n->day, n->year);
    if (format == DATE_EU) return rcFormat(a, "%d %s %d", n->day, MONTH[n->month - 1], n->year);
    return rcFormat(a, "%04d-%02d-%02d", n->year, n->month, n->day);
}

/* What the Word count core plugin reads: a word is a run of non-spaces. */
static inline void note_count(const Note *n, int *words, int *chars) {
    int i;

    *words = *chars = 0;
    for (i = 0; i < n->lineCount; i++) {
        const char *p = n->lines[i].text;
        bool        inWord = false;
        for (; *p; p++) {
            bool space = *p == ' ';
            if (((unsigned char)*p & 0xC0u) != 0x80u) (*chars)++;
            if (!space && !inWord) (*words)++;
            inWord = !space;
        }
    }
}

#endif /* APP_NOTES_H */
