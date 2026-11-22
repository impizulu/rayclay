/*
    notes_backend.h - the notebook: notes as Markdown text, and everything a note's
    text implies (its title, preview, tags, word count, task progress, backlinks).

    Pure C99 with no RayClay in it, and deterministic: it reads no clock, calls no rand()
    and does no I/O. Time is handed in: the runner sets the wall clock and the user's zone
    (note_store_set_clock), and a run with none, the benchmark, advances a fixed demo
    moment by a plain dt, so its "24 minutes ago" reads the same on every run. A time is
    seconds since 1970 UTC throughout. The text is the only source of truth: a note has no
    separate title field, as in Bear its first line is the title, and every derived
    field is recomputed on an edit and never per frame.

    Single implementation, in exactly one TU:
        #define NOTES_BACKEND_IMPLEMENTATION
        #include "notes_backend.h"
*/
#ifndef NOTES_BACKEND_H
#define NOTES_BACKEND_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef NOTEDEF
#define NOTEDEF static
#endif

#define NOTE_MAX_NOTES    40
#define NOTE_BODY_CAP     16384  /* a note's Markdown, NUL included */
#define NOTE_TITLE_CAP    96
#define NOTE_SNIPPET_CAP  360
#define NOTE_MAX_TAGS     6
#define NOTE_TAG_CAP      32
#define NOTE_MAX_TAG_ROWS 40

/* The demo "now", 09:00 UTC on Nov 6, 2026: the seed dates the sample notes back from it,
   and a run with no wall clock counts every age from it. */
#define NOTE_MINUTE       60u
#define NOTE_HOUR         3600u
#define NOTE_DAY          86400u
#define NOTE_EPOCH        (20763u * NOTE_DAY + 9u * NOTE_HOUR)

/* Where the note list is looking. NOTE_SEC_TAG also reads the tag path beside it. */
typedef enum {
    NOTE_SEC_ALL = 0,
    NOTE_SEC_UNTAGGED,
    NOTE_SEC_TODO,
    NOTE_SEC_TODAY,
    NOTE_SEC_ARCHIVE,
    NOTE_SEC_TRASH,
    NOTE_SEC_TAG
} NoteSection;

typedef struct {
    bool     used;                 /* a free slot is reused by note_create            */
    bool     pinned;
    bool     archived;
    bool     trashed;
    uint32_t modified;             /* the last edit, seconds since 1970 UTC           */
    char     rowId[16];            /* the list row's element id: "note" and any int   */
    /* Everything below the body is derived from it (note_derive). */
    char     body[NOTE_BODY_CAP];
    char     title[NOTE_TITLE_CAP];
    char     snippet[NOTE_SNIPPET_CAP];
    char     tags[NOTE_MAX_TAGS][NOTE_TAG_CAP];
    int      tagCount;
    int      words;
    int      chars;
    int      readMins;
    int      tasksDone;
    int      tasksTotal;
    char     ageText[24];          /* "24 minutes ago", refreshed once a minute       */
} Note;

/* One row of the sidebar's tag tree. "work/ops" appears under "work", which exists
   as a row even when no note names it alone, because nesting is how Bear files. */
typedef struct {
    char path[NOTE_TAG_CAP];
    char id[NOTE_TAG_CAP + 4];  /* "tag:" and the path, a key no re-sort moves */
    char chevronId[NOTE_TAG_CAP + 5];
    int  depth;
    int  count;
    bool hasKids;
    bool collapsed;
} NoteTagRow;

typedef struct {
    Note       notes[NOTE_MAX_NOTES];
    int        count;              /* the high-water slot; a slot may be free         */
    NoteTagRow tagRows[NOTE_MAX_TAG_ROWS];
    int        tagRowCount;
    uint32_t   now;                /* the clock, seconds since 1970 UTC               */
    int32_t    utcOffset;          /* the user's zone: local minus UTC, seconds       */
    uint32_t   dayStart;           /* midnight of today by the user's clock           */
    bool       wallClock;          /* the runner sets now; dt then moves no clock     */
    float      accum;              /* dt integrator -> whole-second ticks             */
    uint32_t   ageStamp;           /* the minute the age labels were last written     */
    uint32_t   revision;           /* counts changes the user made, for the saver     */
} NoteStore;

/* -- queries, available to a TU that includes this header for the types alone ---- */
static inline Note *note_at(NoteStore *s, int i) {
    return (i >= 0 && i < s->count && s->notes[i].used) ? &s->notes[i] : NULL;
}

#ifdef NOTES_BACKEND_IMPLEMENTATION

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "notes_corpus.h"

/* Declared and defined only under IMPLEMENTATION, so a TU that includes this header
   for the types alone never sees a static prototype it does not define. */
NOTEDEF void note_memzero(void *p, size_t n);
NOTEDEF void note_store_seed(NoteStore *s);
NOTEDEF void note_store_step(NoteStore *s, float dt);
NOTEDEF void note_store_set_clock(NoteStore *s, uint32_t now, int32_t utcOffset);
NOTEDEF void note_derive(NoteStore *s, Note *n);
NOTEDEF void note_touch(NoteStore *s, Note *n);
NOTEDEF size_t note_store_write(const NoteStore *s, char *out, size_t cap);
NOTEDEF bool note_store_read(NoteStore *s, const char *text);
NOTEDEF void note_tags_rebuild(NoteStore *s);
NOTEDEF int  note_create(NoteStore *s, const char *body);
NOTEDEF int  note_duplicate(NoteStore *s, int i);
NOTEDEF void note_destroy(NoteStore *s, int i);
NOTEDEF void note_append(NoteStore *s, Note *n, const char *text);
NOTEDEF int  note_find_title(NoteStore *s, const char *text, int len);
NOTEDEF int  note_backlinks(NoteStore *s, int i, int *out, int max);
NOTEDEF bool note_has_tag(const Note *n, const char *path);
NOTEDEF bool note_in_section(const Note *n, NoteSection sec, const char *tag,
                             uint32_t dayStart);
NOTEDEF int  note_section_count(NoteStore *s, NoteSection sec, const char *tag);
NOTEDEF bool note_matches(const Note *n, const char *query);
NOTEDEF bool note_text_contains(const char *s, int len, const char *query);
NOTEDEF int  note_sorted(NoteStore *s, NoteSection sec, const char *tag, const char *query,
                         int *out);

NOTEDEF void note_memzero(void *p, size_t n) { memset(p, 0, n); }

static char note_fold(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

static bool note_is_space(char c) { return c == ' ' || c == '\t' || c == '\n'; }

static bool note_is_tag_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '-' || c == '/';
}

static const char *note_line_end(const char *p) {
    while (*p && *p != '\n')
        p++;
    return p;
}

static bool note_starts(const char *p, const char *prefix) {
    return strncmp(p, prefix, strlen(prefix)) == 0;
}

/* The next line of `body`, or NULL at the end of the text. */
static const char *note_next_line(const char *eol) {
    return *eol ? eol + 1 : NULL;
}

static void note_append_char(char *out, int cap, int *k, char c) {
    if (*k < cap - 1)
        out[(*k)++] = c;
}

/* out[0..k) less a UTF-8 character the cap cut in two, so a title or a preview ends before
   that character rather than on half of it (a file name made from the title included). */
static int note_whole(const char *out, int k) {
    int start = k;

    while (start > 0 && ((unsigned char)out[start - 1] & 0xc0u) == 0x80u)
        start--;
    if (start > 0) {
        unsigned char lead = (unsigned char)out[start - 1];
        int need = lead >= 0xf0u ? 4 : (lead >= 0xe0u ? 3 : (lead >= 0xc0u ? 2 : 1));

        if (k - (start - 1) < need)
            return start - 1;
    }
    return k;
}

/* Copy a line's words without their Markdown: what a preview line reads as. A link
   keeps its text and drops its address; a tag keeps its sign, as Bear's list does. */
static void note_plain(const char *s, const char *e, char *out, int cap, int *k) {
    for (const char *p = s; p < e; p++) {
        if (p[0] == ']' && p + 1 < e && p[1] == '(') {
            while (p < e && *p != ')')
                p++;
            continue;
        }
        if (*p == '*' || *p == '`' || *p == '[' || *p == ']' || *p == '|' ||
            (p[0] == '=' && p + 1 < e && p[1] == '='))
            continue;
        if (*p == '=' && p > s && p[-1] == '=')
            continue;
        if (*p == '<') {    /* inline HTML: skip a tag, keep its text */
            const char *q = p + 1;
            while (q < e && *q != '>' && !note_is_space(*q))
                q++;
            if (q < e && *q == '>') {
                p = q;
                continue;
            }
        }
        note_append_char(out, cap, k, *p);
    }
}

/* The line's content past its block marker: "## ", "> ", "- [ ] ", "1. ". */
static const char *note_skip_marker(const char *p, const char *e) {
    while (p < e && (*p == ' ' || *p == '\t'))
        p++;
    while (p < e && *p == '#')
        p++;
    if (p < e && *p == '>') {
        p++;
        while (p < e && *p == ' ')
            p++;
        if (p < e && note_starts(p, "[!")) {
            while (p < e && *p != ']')
                p++;
            if (p < e)
                p++;
        }
    }
    if (e - p >= 2 && (p[0] == '-' || p[0] == '*' || p[0] == '+') && p[1] == ' ') {
        p += 2;
    } else {
        const char *d = p;   /* an ordered item's number, of up to nine digits */

        while (d < e && d - p < 9 && *d >= '0' && *d <= '9')
            d++;
        if (d > p && e - d >= 2 && d[0] == '.' && d[1] == ' ')
            p = d + 2;
    }
    if (e - p >= 4 && p[0] == '[' && p[2] == ']' && p[3] == ' ')
        p += 4;
    while (p < e && *p == ' ')
        p++;
    return p;
}

/* A table is no preview: its cells read as a run of disconnected words. */
static bool note_is_table_line(const char *s, const char *e) {
    while (s < e && *s == ' ')
        s++;
    return s < e && *s == '|';
}

/* Midnight of the store's today by the user's clock, as a time. */
static uint32_t note_day_start(const NoteStore *s) {
    int64_t local = (int64_t)s->now + s->utcOffset;

    return (uint32_t)(local - local % (int64_t)NOTE_DAY - s->utcOffset);
}

/* "24 minutes ago", "3 hours ago" today, "Yesterday", or the day's date by the user's
   calendar, so a session left running, or a notebook kept on the device, ages its notes. */
static void note_age(const NoteStore *s, uint32_t modified, char *out, size_t cap) {
    static const char *const MONTHS[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    uint32_t ago = s->now > modified ? s->now - modified : 0u;

    if (ago < NOTE_MINUTE) {
        snprintf(out, cap, "Just now");
    } else if (ago < NOTE_HOUR) {
        unsigned m = (unsigned)(ago / NOTE_MINUTE);
        snprintf(out, cap, "%u minute%s ago", m, m == 1u ? "" : "s");
    } else if (modified >= s->dayStart) {
        unsigned h = (unsigned)(ago / NOTE_HOUR);
        snprintf(out, cap, "%u hour%s ago", h, h == 1u ? "" : "s");
    } else if (modified >= s->dayStart - NOTE_DAY) {
        snprintf(out, cap, "Yesterday");
    } else if (ago > 365u * NOTE_DAY) {
        snprintf(out, cap, "Over a year ago");
    } else {
        /* The month and day of a day number: Howard Hinnant's civil_from_days, which
           counts in 400-year eras of 146097 days with each year starting on March 1. */
        int64_t z   = ((int64_t)modified + s->utcOffset) / (int64_t)NOTE_DAY + 719468;
        int64_t doe = z % 146097;
        int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        int64_t mp  = (5 * doy + 2) / 153;

        snprintf(out, cap, "%s %d", MONTHS[(mp + 2) % 12], (int)(doy - (153 * mp + 2) / 5 + 1));
    }
}

/* Add one tag from the body, or skip it when the note already has it or is full. */
static void note_add_tag(Note *n, const char *s, int len) {
    if (n->tagCount >= NOTE_MAX_TAGS || len <= 0 || len >= NOTE_TAG_CAP)
        return;
    for (int t = 0; t < n->tagCount; t++)
        if ((int)strlen(n->tags[t]) == len && strncmp(n->tags[t], s, (size_t)len) == 0)
            return;
    memcpy(n->tags[n->tagCount], s, (size_t)len);
    n->tags[n->tagCount][len] = '\0';
    n->tagCount++;
}

static void note_scan_line(Note *n, const char *s, const char *e) {
    const char *m = s;

    while (m < e && *m == ' ')
        m++;
    if (e - m >= 6 && (m[0] == '-' || m[0] == '*' || m[0] == '+') && m[1] == ' ' &&
        m[2] == '[' && m[4] == ']') {
        n->tasksTotal++;
        if (m[3] == 'x' || m[3] == 'X')
            n->tasksDone++;
    }
    for (const char *p = s; p < e; p++) {
        if (*p == '#' && (p == s || note_is_space(p[-1])) && p + 1 < e &&
            ((p[1] >= 'a' && p[1] <= 'z') || (p[1] >= 'A' && p[1] <= 'Z'))) {
            const char *q = p + 1;
            while (q < e && note_is_tag_char(*q))
                q++;
            while (q > p + 1 && q[-1] == '/')   /* a trailing slash is punctuation */
                q--;
            note_add_tag(n, p + 1, (int)(q - p - 1));
        }
    }
}

/* WORDS ARE THE TEXT, NOT ITS MARKUP. A run of non-space bytes is a word once it holds
   a letter, a digit or a UTF-8 byte, so a table's pipes and a divider are not words, and
   a line is read past its block marker (note_skip_marker), so ticking "- [ ]" into
   "- [x]" leaves the count where it was. */
static int note_count_words(const char *s, const char *e) {
    int  words   = 0;
    bool counted = false;   /* the run under way is already a word */

    for (; s < e; s++) {
        unsigned char c = (unsigned char)*s;

        if (note_is_space(*s)) {
            counted = false;
        } else if (!counted && (c >= 0x80u || (c >= '0' && c <= '9') ||
                                (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) {
            counted = true;
            words++;
        }
    }
    return words;
}

/* Recompute what the text implies. The title is the first non-blank line, the preview
   the prose after it. A fence's lines count as words but carry no tags and no tasks:
   a C comment is not a tag, and "#include" is not one either. */
NOTEDEF void note_derive(NoteStore *s, Note *n) {
    const char *p       = n->body;
    int         tk      = 0;
    int         sk      = 0;
    bool        inFence = false;
    bool        title   = false;

    n->tagCount   = 0;
    n->tasksDone  = 0;
    n->tasksTotal = 0;
    n->words      = 0;
    n->chars      = 0;
    n->title[0]   = '\0';
    n->snippet[0] = '\0';

    for (const char *c = n->body; *c; c++)
        if (((unsigned char)*c & 0xc0u) != 0x80u)   /* count a UTF-8 lead byte once */
            n->chars++;

    while (p) {
        const char *e = note_line_end(p);
        const char *fence = p;
        bool        blank = true;

        for (const char *c = p; c < e; c++)
            if (!note_is_space(*c))
                blank = false;
        while (*fence == ' ')   /* a fence may be indented, as the page draws one */
            fence++;

        if (note_starts(fence, "```")) {
            inFence = !inFence;
        } else if (inFence) {
            n->words += note_count_words(p, e);
        } else if (!blank) {
            n->words += note_count_words(note_skip_marker(p, e), e);
            note_scan_line(n, p, e);
            if (!title) {
                note_plain(note_skip_marker(p, e), e, n->title, NOTE_TITLE_CAP, &tk);
                tk = note_whole(n->title, tk);
                n->title[tk] = '\0';
                title = true;
            } else if (sk < NOTE_SNIPPET_CAP - 4 && !note_is_table_line(p, e)) {
                /* A line that ends a sentence reads on into the next one; a list item
                   does not, so it gets a separator instead of running together. */
                if (sk && n->snippet[sk - 1] != '.') {
                    note_append_char(n->snippet, NOTE_SNIPPET_CAP, &sk, ' ');
                    note_append_char(n->snippet, NOTE_SNIPPET_CAP, &sk, '\xc2');
                    note_append_char(n->snippet, NOTE_SNIPPET_CAP, &sk, '\xb7');
                }
                if (sk)
                    note_append_char(n->snippet, NOTE_SNIPPET_CAP, &sk, ' ');
                note_plain(note_skip_marker(p, e), e, n->snippet, NOTE_SNIPPET_CAP, &sk);
                sk = note_whole(n->snippet, sk);
                n->snippet[sk] = '\0';
            }
        }
        p = note_next_line(e);
    }
    n->readMins = n->words ? (n->words + 199) / 200 : 1;
    if (!n->title[0])
        snprintf(n->title, sizeof n->title, "Untitled");
    if (!n->snippet[0])
        snprintf(n->snippet, sizeof n->snippet, "No additional text");
    note_age(s, n->modified, n->ageText, sizeof n->ageText);
}

/* An edit: the text changed, so everything it implies did, and the note is newest. */
NOTEDEF void note_touch(NoteStore *s, Note *n) {
    n->modified = s->now;
    note_derive(s, n);
    note_tags_rebuild(s);
    s->revision++;
}

static void note_add_tag_row(NoteStore *s, const char *path, int len) {
    NoteTagRow *row;

    for (int r = 0; r < s->tagRowCount; r++)
        if ((int)strlen(s->tagRows[r].path) == len &&
            strncmp(s->tagRows[r].path, path, (size_t)len) == 0)
            return;
    if (s->tagRowCount >= NOTE_MAX_TAG_ROWS)
        return;
    row = &s->tagRows[s->tagRowCount++];
    memset(row, 0, sizeof *row);
    memcpy(row->path, path, (size_t)len);
    row->path[len] = '\0';
}

/* Rebuild the tag tree from the live notes, in path order so a child follows its
   parent. A row's id is its path, so its hover and focus stay on it when a new tag
   sorts in above, and its collapsed state is carried over by name: typing a new tag
   must not spring open a branch the user closed. */
NOTEDEF void note_tags_rebuild(NoteStore *s) {
    static NoteTagRow previous[NOTE_MAX_TAG_ROWS];
    int               previousCount = s->tagRowCount;

    memcpy(previous, s->tagRows, sizeof previous);
    s->tagRowCount = 0;
    for (int i = 0; i < s->count; i++) {
        const Note *n = &s->notes[i];

        if (!n->used || n->trashed || n->archived)
            continue;
        for (int t = 0; t < n->tagCount; t++) {
            const char *path = n->tags[t];
            int         len  = (int)strlen(path);

            for (int k = 1; k < len; k++)   /* every ancestor is a row too */
                if (path[k] == '/')
                    note_add_tag_row(s, path, k);
            note_add_tag_row(s, path, len);
        }
    }
    for (int a = 1; a < s->tagRowCount; a++) {   /* insertion sort: tens of rows */
        NoteTagRow key = s->tagRows[a];
        int        b   = a - 1;

        while (b >= 0 && strcmp(s->tagRows[b].path, key.path) > 0) {
            s->tagRows[b + 1] = s->tagRows[b];
            b--;
        }
        s->tagRows[b + 1] = key;
    }
    for (int r = 0; r < s->tagRowCount; r++) {
        NoteTagRow *row = &s->tagRows[r];
        size_t      len = strlen(row->path);

        /* path is shorter than NOTE_TAG_CAP, so each id fits its prefix, path and NUL. */
        memcpy(row->id, "tag:", 4);
        memcpy(row->id + 4, row->path, len + 1);
        memcpy(row->chevronId, "tagc:", 5);
        memcpy(row->chevronId + 5, row->path, len + 1);
        row->depth = 0;
        for (size_t k = 0; k < len; k++)
            if (row->path[k] == '/')
                row->depth++;
        row->hasKids = r + 1 < s->tagRowCount &&
                       strncmp(s->tagRows[r + 1].path, row->path, len) == 0 &&
                       s->tagRows[r + 1].path[len] == '/';
        row->count = 0;
        for (int i = 0; i < s->count; i++) {
            const Note *n = &s->notes[i];

            if (n->used && !n->trashed && !n->archived && note_has_tag(n, row->path))
                row->count++;
        }
        for (int o = 0; o < previousCount; o++)
            if (strcmp(previous[o].path, row->path) == 0)
                row->collapsed = previous[o].collapsed;
    }
}

/* Join a corpus note and date it `age` seconds before the store's now. */
static int note_seed_one(NoteStore *s, const char *const *parts, int partCount, uint32_t age) {
    Note *n = &s->notes[s->count];
    int   k = 0;

    n->used = true;
    snprintf(n->rowId, sizeof n->rowId, "note%d", s->count);
    for (int p = 0; p < partCount; p++) {
        size_t len = strlen(parts[p]);

        if (k + (int)len >= NOTE_BODY_CAP)
            break;
        memcpy(n->body + k, parts[p], len);
        k += (int)len;
    }
    n->body[k]  = '\0';
    n->modified = s->now - age;
    note_derive(s, n);
    return s->count++;
}

#define NOTE_SEED(s, arr, age) \
    note_seed_one((s), (arr), (int)(sizeof(arr) / sizeof((arr)[0])), (age))

NOTEDEF void note_store_seed(NoteStore *s) {
    note_memzero(s, sizeof *s);
    s->now      = NOTE_EPOCH;
    s->dayStart = note_day_start(s);

    /* Order is identity: note 2 is the note the app opens on and the bench freezes on. */
    NOTE_SEED(s, NB_MOVING,     6u * NOTE_HOUR);
    NOTE_SEED(s, NB_SOURDOUGH,  3u * NOTE_DAY);
    NOTE_SEED(s, NB_KYOTO,      24u * NOTE_MINUTE);
    NOTE_SEED(s, NB_DOCS,       26u * NOTE_HOUR);
    NOTE_SEED(s, NB_RING,       3u * NOTE_HOUR);
    NOTE_SEED(s, NB_ROADMAP,    2u * NOTE_HOUR);
    NOTE_SEED(s, NB_BOOKS,      5u * NOTE_DAY);
    NOTE_SEED(s, NB_BUDGET,     50u * NOTE_MINUTE);
    NOTE_SEED(s, NB_RUNBOOK,    8u * NOTE_DAY);
    NOTE_SEED(s, NB_BALCONY,    12u * NOTE_DAY);
    NOTE_SEED(s, NB_CHEATSHEET, 2u * NOTE_DAY);
    NOTE_SEED(s, NB_INTERVIEW,  4u * NOTE_DAY);
    NOTE_SEED(s, NB_CONFIG,     19u * NOTE_DAY);
    NOTE_SEED(s, NB_GROCERY,    5u * NOTE_MINUTE);
    NOTE_SEED(s, NB_CAR,        40u * NOTE_DAY);
    NOTE_SEED(s, NB_KEYS,       7u * NOTE_DAY);

    s->notes[2].pinned   = true;
    s->notes[3].pinned   = true;
    s->notes[14].archived = true;
    s->notes[15].trashed  = true;
    note_tags_rebuild(s);
}

/* The notebook as one text file: a version line, then a header line per note, its length
   saying where the next one starts, so a note can hold any text at all. A note keeps the
   time of its last edit, so its age is right however long the file was put away. */
#define NOTE_FILE_VERSION "@notebook 2\n"

NOTEDEF size_t note_store_write(const NoteStore *s, char *out, size_t cap) {
    size_t k = sizeof NOTE_FILE_VERSION - 1;

    if (cap <= k)
        return 0;
    memcpy(out, NOTE_FILE_VERSION, k);
    for (int i = 0; i < s->count; i++) {
        const Note *n = &s->notes[i];
        size_t      len;
        int         head;

        if (!n->used)
            continue;
        len  = strlen(n->body);
        head = snprintf(out + k, cap - k, "@note %d %d %d %u %lu\n", n->pinned ? 1 : 0,
                        n->archived ? 1 : 0, n->trashed ? 1 : 0, (unsigned)n->modified,
                        (unsigned long)len);
        if (head < 0 || k + (size_t)head + len + 1 >= cap)
            break;
        k += (size_t)head;
        memcpy(out + k, n->body, len);
        k += len;
        out[k++] = '\n';
    }
    return k;
}

/* Replace the notebook with what note_store_write wrote, or refuse and leave it: a
   damaged file must not cost the user the notes on screen. A file with no version line
   holds each note's age at its save rather than a time; the age is counted back from
   the moment the file is read, the best reading it allows, and the next save writes times. */
NOTEDEF bool note_store_read(NoteStore *s, const char *text) {
    static NoteStore fresh;
    const char      *p     = text;
    const bool       times = note_starts(p, NOTE_FILE_VERSION);

    note_memzero(&fresh, sizeof fresh);
    fresh.now       = s->now;
    fresh.utcOffset = s->utcOffset;
    fresh.dayStart  = s->dayStart;
    fresh.wallClock = s->wallClock;
    if (times)
        p += sizeof NOTE_FILE_VERSION - 1;
    while (*p && fresh.count < NOTE_MAX_NOTES) {
        int           pinned, archived, trashed, k;
        unsigned      stamp;
        unsigned long len, field[5];
        const char   *body = strchr(p, '\n');
        const char   *q;
        char         *end;
        Note         *n    = &fresh.notes[fresh.count];

        /* The header line's five unsigned fields, each bounded by the line. */
        if (!body || !note_starts(p, "@note "))
            return false;
        q = p + 6;   /* only now known to be inside the text */
        for (k = 0; k < 5; k++) {
            while (*q == ' ')
                q++;
            if (*q < '0' || *q > '9')
                return false;
            field[k] = strtoul(q, &end, 10);
            q = end;
        }
        pinned   = (int)field[0];
        archived = (int)field[1];
        trashed  = (int)field[2];
        stamp    = (unsigned)field[3];
        len      = field[4];
        if (len >= NOTE_BODY_CAP || strlen(body + 1) < len)
            return false;
        n->used     = true;
        n->pinned   = pinned != 0;
        n->archived = archived != 0;
        n->trashed  = trashed != 0;
        /* An age longer than the clock has run reads as the oldest time, not wrapped round
           to the newest. */
        if (times)
            n->modified = stamp;
        else
            n->modified = stamp < fresh.now ? fresh.now - stamp : 0u;
        snprintf(n->rowId, sizeof n->rowId, "note%d", fresh.count);
        memcpy(n->body, body + 1, len);
        note_derive(&fresh, n);
        fresh.count++;
        p = body + 1 + len;
        if (*p == '\n')
            p++;
    }
    note_tags_rebuild(&fresh);
    *s = fresh;
    return true;
}

/* Rewrite the age labels and where today began once a minute, which is the most often
   any of them can change. */
static void note_store_tick(NoteStore *s) {
    if (s->now / NOTE_MINUTE == s->ageStamp)
        return;
    s->ageStamp = s->now / NOTE_MINUTE;
    s->dayStart = note_day_start(s);
    for (int i = 0; i < s->count; i++)
        if (s->notes[i].used)
            note_age(s, s->notes[i].modified, s->notes[i].ageText, sizeof s->notes[i].ageText);
}

/* Advance a store with no wall clock by dt. dt <= 0 is the benchmark's freeze: a strict
   no-op. */
NOTEDEF void note_store_step(NoteStore *s, float dt) {
    if (dt <= 0.0f || s->wallClock)
        return;
    s->accum += dt;
    while (s->accum >= 1.0f) {
        s->accum -= 1.0f;
        s->now++;
    }
    note_store_tick(s);
}

/* The wall clock and the user's zone, read by the runner each frame. The first reading
   carries the sample notes along, so each keeps the age the seed gave it. */
NOTEDEF void note_store_set_clock(NoteStore *s, uint32_t now, int32_t utcOffset) {
    if (!s->wallClock || utcOffset != s->utcOffset)
        s->ageStamp = 0;   /* relabel at once: no real minute is 0 */
    if (!s->wallClock)
        for (int i = 0; i < s->count; i++)
            s->notes[i].modified += now - s->now;   /* unsigned, so exact either way */
    s->wallClock = true;
    s->now       = now;
    s->utcOffset = utcOffset;
    note_store_tick(s);
}

/* A new note in a free slot, or -1 when the notebook is full. */
NOTEDEF int note_create(NoteStore *s, const char *body) {
    int i = 0;

    while (i < s->count && s->notes[i].used)
        i++;
    if (i >= NOTE_MAX_NOTES)
        return -1;
    memset(&s->notes[i], 0, sizeof s->notes[i]);
    s->notes[i].used = true;
    snprintf(s->notes[i].rowId, sizeof s->notes[i].rowId, "note%d", i);
    if (body)
        snprintf(s->notes[i].body, NOTE_BODY_CAP, "%s", body);
    if (i == s->count)
        s->count++;
    note_touch(s, &s->notes[i]);
    return i;
}

NOTEDEF int note_duplicate(NoteStore *s, int i) {
    int fresh = note_at(s, i) ? note_create(s, s->notes[i].body) : -1;
    return fresh;
}

NOTEDEF void note_destroy(NoteStore *s, int i) {
    if (!note_at(s, i))
        return;
    s->notes[i].used = false;
    note_tags_rebuild(s);
    s->revision++;
}

/* Add a block after the note's last line, keeping one blank line between them. */
NOTEDEF void note_append(NoteStore *s, Note *n, const char *text) {
    size_t have = strlen(n->body);

    if (have && n->body[have - 1] != '\n')
        snprintf(n->body + have, NOTE_BODY_CAP - have, "\n\n%s", text);
    else if (have)
        snprintf(n->body + have, NOTE_BODY_CAP - have, "\n%s", text);
    else
        snprintf(n->body, NOTE_BODY_CAP, "%s", text);
    note_touch(s, n);
}

/* The first line of the note a dead wiki link creates (notes_create_titled): "# " and as
   much of the link's text as that line holds. Returns its length; @p line holds
   NOTE_TITLE_CAP + 8 bytes, room for the two newlines and the NUL after it. */
static int note_link_line(const char *text, int len, char *line) {
    int k = 0;

    line[k++] = '#';
    line[k++] = ' ';
    for (int i = 0; i < len && k < NOTE_TITLE_CAP + 5; i++)
        line[k++] = text[i];
    return k;
}

/* The live note a wiki link resolves to, case folded, or -1. The link's text is read as
   note_derive reads the line notes_create_titled writes for it, so the note a click on a
   dead link creates is the note the link finds afterwards: compared raw, a link with markup,
   spaces at its ends or more text than a title holds stays dead, and each click adds a note. */
NOTEDEF int note_find_title(NoteStore *s, const char *text, int len) {
    char line[NOTE_TITLE_CAP + 8];
    char want[NOTE_TITLE_CAP];
    int  k = note_link_line(text, len, line);
    int  t = 0;

    note_plain(note_skip_marker(line, line + k), line + k, want, NOTE_TITLE_CAP, &t);
    t = note_whole(want, t);
    want[t] = '\0';
    if (!t)
        snprintf(want, sizeof want, "Untitled");
    for (int i = 0; i < s->count; i++) {
        const Note *n = &s->notes[i];
        int         c = 0;

        if (!n->used || n->trashed)
            continue;
        while (n->title[c] && note_fold(n->title[c]) == note_fold(want[c]))
            c++;
        if (!n->title[c] && !want[c])
            return i;
    }
    return -1;
}

/* The notes that link here with [[title]], for the status bar and the page's foot. */
NOTEDEF int note_backlinks(NoteStore *s, int i, int *out, int max) {
    const Note *self = note_at(s, i);
    int         found = 0;
    int         titleLen;

    if (!self)
        return 0;
    titleLen = (int)strlen(self->title);
    for (int o = 0; o < s->count && found < max; o++) {
        const Note *n = &s->notes[o];

        if (o == i || !n->used || n->trashed)
            continue;
        for (const char *p = strstr(n->body, "[["); p; p = strstr(p + 2, "[[")) {
            int k = 0;

            while (k < titleLen && p[2 + k] &&
                   note_fold(p[2 + k]) == note_fold(self->title[k]))
                k++;
            if (k == titleLen && strncmp(p + 2 + k, "]]", 2) == 0) {
                out[found++] = o;
                break;
            }
        }
    }
    return found;
}

/* Exactly this tag, or one nested under it. */
NOTEDEF bool note_has_tag(const Note *n, const char *path) {
    size_t len = strlen(path);

    for (int t = 0; t < n->tagCount; t++)
        if (strncmp(n->tags[t], path, len) == 0 &&
            (n->tags[t][len] == '\0' || n->tags[t][len] == '/'))
            return true;
    return false;
}

NOTEDEF bool note_in_section(const Note *n, NoteSection sec, const char *tag,
                             uint32_t dayStart) {
    if (!n->used)
        return false;
    if (sec == NOTE_SEC_TRASH)
        return n->trashed;
    if (n->trashed)
        return false;
    if (sec == NOTE_SEC_ARCHIVE)
        return n->archived;
    if (n->archived)
        return false;
    switch (sec) {
    case NOTE_SEC_UNTAGGED: return n->tagCount == 0;
    case NOTE_SEC_TODO:     return n->tasksDone < n->tasksTotal;
    case NOTE_SEC_TODAY:    return n->modified >= dayStart;
    case NOTE_SEC_TAG:      return note_has_tag(n, tag);
    default:                return true;
    }
}

NOTEDEF int note_section_count(NoteStore *s, NoteSection sec, const char *tag) {
    int count = 0;

    for (int i = 0; i < s->count; i++)
        if (note_in_section(&s->notes[i], sec, tag, s->dayStart))
            count++;
    return count;
}

/* Case-insensitive search of a slice, for a section of the reader's contents. */
NOTEDEF bool note_text_contains(const char *s, int len, const char *query) {
    int qlen = (int)strlen(query);

    for (int i = 0; i + qlen <= len; i++) {
        int k = 0;

        while (k < qlen && note_fold(s[i + k]) == note_fold(query[k]))
            k++;
        if (k == qlen)
            return true;
    }
    return false;
}

/* An empty query matches everything, so an untouched search box filters nothing. */
NOTEDEF bool note_matches(const Note *n, const char *query) {
    if (!query || !query[0])
        return true;
    return note_text_contains(n->body, (int)strlen(n->body), query);
}

/* The list's rows: pinned first, then the newest edit, as Bear sorts. */
NOTEDEF int note_sorted(NoteStore *s, NoteSection sec, const char *tag, const char *query,
                        int *out) {
    int n = 0;

    for (int i = 0; i < s->count; i++) {
        const Note *note = &s->notes[i];
        int         at;

        if (!note_in_section(note, sec, tag, s->dayStart) || !note_matches(note, query))
            continue;
        at = n++;
        while (at > 0) {
            const Note *prev = &s->notes[out[at - 1]];

            if (prev->pinned != note->pinned ? prev->pinned : prev->modified >= note->modified)
                break;
            out[at] = out[at - 1];
            at--;
        }
        out[at] = i;
    }
    return n;
}

#endif /* NOTES_BACKEND_IMPLEMENTATION */
#endif /* NOTES_BACKEND_H */
