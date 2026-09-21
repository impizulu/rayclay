/*
    notes_backend.h - the notes app's model: a note/post store, a frozen text
    corpus, a seeded PRNG and a fixed-step clock.

    Pure C99, no RayClay dependency, deterministic under a seed: no wall clock,
    no rand(), no I/O at frame time - it takes a plain dt. A note's body is an
    array of paragraph strings, so the GUI wraps each paragraph as its own block
    and the breaks are guaranteed. This header owns note_memzero, which keeps the
    GUI free of <system> includes.

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

#define NOTE_MAX_NOTES        32
#define NOTE_MAX_TAGS          4
#define NOTE_TITLE_CAP        80
#define NOTE_TAG_CAP          16
#define NOTE_EPOCH_SECONDS 32400   /* 09:00:00 - a frozen origin, no wall clock */

typedef enum { NOTE_DRAFT = 0, NOTE_PUBLISHED } NoteStatus;

/* A body = an array of paragraph strings (guaranteed breaks, deterministic wrap). */
typedef struct {
    const char *const *paras;
    int                nParas;
} NoteBody;

typedef struct {
    char        title[NOTE_TITLE_CAP];   /* editable (overwritten in place)         */
    uint16_t    titleLen;
    const char *snippet;                 /* sidebar one-line preview (corpus const*) */
    NoteBody    body;                    /* the note's paragraphs                    */
    char        tags[NOTE_MAX_TAGS][NOTE_TAG_CAP];  /* editable, via note_set_tags   */
    uint8_t     tagCount;
    uint8_t     status;                  /* NoteStatus                              */
    uint16_t    wordCount;               /* precomputed at seed                     */
    uint16_t    readMins;                /* ceil(wordCount / 200), min 1            */
    char        date[8];                 /* precomputed "Mon DD" - ALWAYS 6 glyphs  */
    char        meta[28];                /* precomputed "842 words \xc2\xb7 5 min read" */
} Note;

typedef struct {
    Note     notes[NOTE_MAX_NOTES];
    int32_t  count;
    int32_t  selected;         /* open-note index (-1 = none)                */
    int32_t  publishedCount;   /* cached header/tab stat                      */
    uint32_t nowEpoch;         /* sim clock, seconds since the frozen origin  */
    float    accum;            /* dt integrator -> whole-second ticks         */
    uint32_t rng;              /* xorshift32 state (seed-time ONLY)           */
} NoteStore;

/* -- queries (const, pure, per-frame; always available) ---------------------- */
static inline int         note_count(const NoteStore *s) { return s->count; }
static inline const Note *note_at(const NoteStore *s, int i) {
    return (i >= 0 && i < s->count) ? &s->notes[i] : NULL;
}
static inline int note_published_count(const NoteStore *s) { return s->publishedCount; }

#ifdef NOTES_BACKEND_IMPLEMENTATION

#include <string.h>

/* Declared and defined only under IMPLEMENTATION, so a TU that includes this header
   for the types alone never sees a static prototype it does not define. */
NOTEDEF void note_memzero(void *p, size_t n);
NOTEDEF void note_store_seed(NoteStore *s, unsigned seed);
NOTEDEF void note_store_step(NoteStore *s, float dt);   /* dt <= 0 => no-op (freeze) */
NOTEDEF void note_set_title(NoteStore *s, int i, const char *text, int len);
NOTEDEF void note_set_tags(NoteStore *s, int i, const char *csv);
NOTEDEF void note_tags_csv(const Note *n, char *out, int cap);
NOTEDEF void note_publish(NoteStore *s, int i);
NOTEDEF int  note_create(NoteStore *s);
NOTEDEF bool note_matches(const Note *n, const char *query);
NOTEDEF void note_elide(const char *src, char *out, int cap, int maxChars);
NOTEDEF int  note_body_text(const Note *n, char *out, int cap);

NOTEDEF void note_memzero(void *p, size_t n) { memset(p, 0, n); }

/* xorshift32 - deterministic, used ONLY at seed time (never on a measured frame). */
static uint32_t note__rng(NoteStore *s) {
    uint32_t x = s->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return (s->rng = x);
}

/* The frozen corpus: immutable, ASCII/Latin-1 only. */
static const char *const NB__BODY_LONG[] = {
    "RayClay renders an entire application into a single canvas, so the thing you "
    "ship and the thing you benchmark are one and the same source. This note is the "
    "text-heavy member of the standardised suite: its job is to push the glyph path.",
    "Immediate mode means the layout is rebuilt every frame. That sounds expensive, "
    "but the cost is dominated by measuring and shaping the text you can actually "
    "see - a long article like this one, wrapped to a fixed editorial column.",
    "Because the column width is fixed, the wrap is deterministic: the same words "
    "break onto the same lines on every machine, so a benchmark can subtract two "
    "identical frames and read a clean per-frame instruction count.",
    "The typography ladder matters here. A real editor mixes a display title, a "
    "reading body, the occasional subheading, and small metadata - each a different "
    "baked size, which is exactly the atlas coverage a text app should exercise.",
    "None of this reads a wall clock. Dates are stamped from a frozen epoch, word "
    "counts are precomputed once at seed time, and the reading estimate is a fixed "
    "width string, so no formatted value can change length and reflow the page.",
    "Editing is not confined to single-line fields. The body below is a live "
    "multi-line editor over a wrapped buffer - the one capability a note taking app "
    "really wants - so the write tab is a genuine text area, not a static preview.",
    "Everything else is here today: a sidebar of notes, a write and a preview tab, a "
    "publish dialog, a details table, and a body that scrolls. It looks like a real "
    "tool because, under the immediate-mode hood, it is one.",
};
static const char *const NB__BODY_MED[] = {
    "A shorter draft. Notes in the sidebar carry different bodies so selecting one "
    "forces a genuine re-measure of a new block of text, not a cached repaint.",
    "The benchmark selects the long article; the demo lets you browse them all. "
    "Same code path, only the clock, the seed, and the input source differ.",
    "Publishing flips a draft to a post: the status chip changes and the preview "
    "tab renders the byline, the body, a details table, and the tag chips.",
};
static const char *const NB__BODY_SHORT[] = {
    "A quick capture - the kind of note that is two sentences and a tag.",
    "It still exercises the wrap, just less of it.",
};

typedef struct { const char *const *paras; int n; } NB__Doc;
#define NB__DOC(arr) { (arr), (int)(sizeof(arr) / sizeof((arr)[0])) }
static const NB__Doc NB__DOCS[] = {
    NB__DOC(NB__BODY_LONG), NB__DOC(NB__BODY_MED), NB__DOC(NB__BODY_SHORT),
};

static const char *const NB__TITLES[] = {
    "Designing a benchmark that ships",
    "Immediate mode, in practice",
    "Why the wrap must be deterministic",
    "A typography ladder for editors",
    "Notes on a frozen clock",
    "The single-line editing wall",
    "One source, two modes",
    "Reading time, precomputed",
    "A sidebar that scales",
    "Publishing a draft",
    "Tags and taxonomy",
    "Weekend backlog",
    "Meeting summary",
    "Grocery list",
};
static const char *const NB__SNIPPETS[] = {
    "The thing you ship is the thing you benchmark.",
    "The layout is rebuilt every frame - and that is fine.",
    "Same words, same line breaks, every machine.",
    "Title, body, subhead, metadata - four baked sizes.",
    "Dates from an epoch; nothing reads the wall clock.",
    "Single-line fields today; a text area is the gap.",
    "Demo and bench are the same code path.",
    "A fixed-width string can never reflow the page.",
    "Different bodies force a real re-measure.",
    "A draft becomes a post in one dialog.",
    "Comma-separated, lowercased, deduped.",
    "Half-finished thoughts, saved for later.",
    "Action items from the standup.",
    "Milk, bread, coffee, the usual.",
};
static const char *const NB__TAGS[] = {
    "rayclay", "gui", "perf", "design", "notes", "draft", "c", "web",
};
static const char *const NB__MONTHS[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

static int note__uint_str(unsigned v, char *out, int cap) {   /* right-anchored, no libc */
    char tmp[12]; int n = 0;
    do { tmp[n++] = (char)('0' + v % 10u); v /= 10u; } while (v && n < 11);
    if (n >= cap) n = cap - 1;
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = '\0';
    return n;
}

static void note__set_date(Note *nt, int month0, int day) {
    /* "Mon DD" - fixed 6 glyphs (a leading space pads a 1-digit day). */
    const char *m = NB__MONTHS[month0 % 12];
    nt->date[0] = m[0]; nt->date[1] = m[1]; nt->date[2] = m[2];
    nt->date[3] = ' ';
    nt->date[4] = (char)('0' + (day / 10) % 10);
    nt->date[5] = (char)('0' + day % 10);
    nt->date[6] = '\0';
}

static void note__set_meta(Note *nt) {
    /* "N words \xc2\xb7 M min read" - Latin-1 middot; lengths vary with the number,
       but a note's meta is FIXED at seed and never re-formatted, so it never reflows. */
    char *o = nt->meta; int cap = (int)sizeof nt->meta, k = 0;
    k += note__uint_str(nt->wordCount, o + k, cap - k);
    const char *suffix = " words \xc2\xb7 ";
    for (int i = 0; suffix[i] && k < cap - 1; i++) o[k++] = suffix[i];
    k += note__uint_str(nt->readMins, o + k, cap - k);
    const char *tail = " min read";
    for (int i = 0; tail[i] && k < cap - 1; i++) o[k++] = tail[i];
    o[k] = '\0';
}

static void note__count_words(Note *nt) {
    unsigned words = 0;
    for (int p = 0; p < nt->body.nParas; p++) {
        bool in = false;
        for (const char *c = nt->body.paras[p]; *c; c++) {
            bool sp = (*c == ' ');
            if (!sp && !in) { words++; in = true; }
            else if (sp) in = false;
        }
    }
    nt->wordCount = (uint16_t)words;
    nt->readMins  = (uint16_t)(words ? (words + 199u) / 200u : 1u);   /* ceil, min 1 */
}

NOTEDEF void note_store_seed(NoteStore *s, unsigned seed) {
    note_memzero(s, sizeof *s);
    s->rng      = seed ? seed : 0x1234567u;
    s->nowEpoch = NOTE_EPOCH_SECONDS;
    s->selected = 0;

    const int nTitles = (int)(sizeof NB__TITLES / sizeof *NB__TITLES);
    const int nTags   = (int)(sizeof NB__TAGS   / sizeof *NB__TAGS);
    s->count = nTitles;
    if (s->count > NOTE_MAX_NOTES) s->count = NOTE_MAX_NOTES;

    for (int i = 0; i < s->count; i++) {
        Note *nt = &s->notes[i];
        /* title copied into the note's own fixed slot (editable) */
        size_t tl = strlen(NB__TITLES[i]);
        if (tl >= NOTE_TITLE_CAP) tl = NOTE_TITLE_CAP - 1;
        memcpy(nt->title, NB__TITLES[i], tl);
        nt->title[tl] = '\0';
        nt->titleLen  = (uint16_t)tl;
        nt->snippet   = NB__SNIPPETS[i];
        /* note 0 (initially open) + note 2 (the scripted selection) get the long body */
        int doc = (i == 0 || i == 2) ? 0 : (i % 3 == 1 ? 1 : (i % 3 == 2 ? 2 : 1));
        nt->body.paras  = NB__DOCS[doc].paras;
        nt->body.nParas = NB__DOCS[doc].n;
        nt->tagCount = (uint8_t)(2u + note__rng(s) % 3u);
        for (int t = 0; t < nt->tagCount; t++) {
            const char *src = NB__TAGS[(i * 3 + t) % nTags];
            size_t      tg  = strlen(src);
            if (tg >= NOTE_TAG_CAP) tg = NOTE_TAG_CAP - 1;
            memcpy(nt->tags[t], src, tg);
            nt->tags[t][tg] = '\0';
        }
        nt->status = (uint8_t)((i % 3 == 0) ? NOTE_PUBLISHED : NOTE_DRAFT);
        if (nt->status == NOTE_PUBLISHED) s->publishedCount++;
        note__count_words(nt);
        note__set_meta(nt);
        note__set_date(nt, (5 + i) % 12, 1 + (i * 3) % 28);
    }
}

NOTEDEF void note_store_step(NoteStore *s, float dt) {
    if (dt <= 0.0f)                 /* the freeze: a strict no-op */
        return;
    s->accum += dt;
    while (s->accum >= 1.0f) {      /* advance whole sim-seconds (autosave cadence etc.) */
        s->accum -= 1.0f;
        s->nowEpoch++;
    }
}

NOTEDEF void note_set_title(NoteStore *s, int i, const char *text, int len) {
    if (i < 0 || i >= s->count)
        return;
    if (len < 0) len = 0;
    if (len >= NOTE_TITLE_CAP) len = NOTE_TITLE_CAP - 1;
    memcpy(s->notes[i].title, text, (size_t)len);
    s->notes[i].title[len] = '\0';
    s->notes[i].titleLen   = (uint16_t)len;
}

/* Split a comma-separated list into the note's tag slots. Empty entries are
   skipped and anything past NOTE_MAX_TAGS is dropped, so a run-on line cannot
   grow the note. */
NOTEDEF void note_set_tags(NoteStore *s, int i, const char *csv) {
    Note *nt;
    int   n = 0;

    if (i < 0 || i >= s->count)
        return;
    nt = &s->notes[i];
    for (int k = 0; k < NOTE_MAX_TAGS; k++)
        nt->tags[k][0] = '\0';
    while (csv && *csv && n < NOTE_MAX_TAGS) {
        int len = 0, keep;

        while (*csv == ' ' || *csv == ',')
            csv++;
        if (!*csv)
            break;
        while (csv[len] && csv[len] != ',')
            len++;
        keep = len;
        while (keep > 0 && csv[keep - 1] == ' ')
            keep--;
        if (keep > NOTE_TAG_CAP - 1)
            keep = NOTE_TAG_CAP - 1;
        if (keep > 0) {
            memcpy(nt->tags[n], csv, (size_t)keep);
            nt->tags[n][keep] = '\0';
            n++;
        }
        csv += len;
    }
    nt->tagCount = (uint8_t)n;
}

/* The note's tags as one editable line, which is the shape the Write tab edits. */
NOTEDEF void note_tags_csv(const Note *n, char *out, int cap) {
    int k = 0;
    if (!out || cap <= 0)
        return;
    for (int t = 0; n && t < n->tagCount; t++) {
        if (t && k < cap - 2) { out[k++] = ','; out[k++] = ' '; }
        for (const char *c = n->tags[t]; *c && k < cap - 1; c++)
            out[k++] = *c;
    }
    out[k] = '\0';
}

NOTEDEF void note_publish(NoteStore *s, int i) {
    if (i < 0 || i >= s->count || s->notes[i].status == NOTE_PUBLISHED)
        return;
    s->notes[i].status = NOTE_PUBLISHED;
    s->publishedCount++;
}

/* Append an empty draft and return its index, or -1 when the notebook is full. */
NOTEDEF int note_create(NoteStore *s) {
    static const char FRESH[] = "New note";
    Note *nt;
    int   i = s->count;

    if (i >= NOTE_MAX_NOTES)
        return -1;
    nt = &s->notes[i];
    note_memzero(nt, sizeof *nt);
    memcpy(nt->title, FRESH, sizeof FRESH);
    nt->titleLen = (uint16_t)(sizeof FRESH - 1);
    nt->snippet  = "Nothing here yet.";
    nt->status   = NOTE_DRAFT;
    note__count_words(nt);              /* an empty body: 0 words, 1 min */
    note__set_meta(nt);
    note__set_date(nt, (5 + i) % 12, 1 + (i * 3) % 28);
    s->count = i + 1;
    return i;
}

/* Case-insensitive substring match over the title and the snippet. An empty
   query matches everything, so an untouched search box filters nothing. */
NOTEDEF bool note_matches(const Note *n, const char *query) {
    if (!query || !query[0])
        return true;
    for (int f = 0; f < 2; f++) {
        const char *hay = f ? n->snippet : n->title;
        for (; hay && *hay; hay++) {
            const char *a = hay, *b = query;
            while (*b && ((*a | 32) == (*b | 32))) { a++; b++; }
            if (!*b)
                return true;
        }
    }
    return false;
}

/* Copy `src` into `out`, capped at `maxChars`; a string that did not fit ends in
   "..." so a clipped label reads as clipped rather than as a typo. */
NOTEDEF void note_elide(const char *src, char *out, int cap, int maxChars) {
    int len = 0, keep;

    if (!out || cap <= 0)
        return;
    out[0] = '\0';
    if (!src)
        return;
    if (maxChars > cap - 1)
        maxChars = cap - 1;
    while (src[len])
        len++;
    if (len <= maxChars) {
        memcpy(out, src, (size_t)len);
        out[len] = '\0';
        return;
    }
    keep = maxChars > 3 ? maxChars - 3 : 0;
    while (keep > 0 && src[keep - 1] == ' ')
        keep--;
    memcpy(out, src, (size_t)keep);
    for (int i = 0; i < 3; i++)
        out[keep + i] = '.';
    out[keep + 3] = '\0';
}

/* Flatten a note's paragraph body into one editable buffer, paragraphs joined by a
   blank line, so the Write-tab text area shows the note verbatim. Deterministic (no
   wall clock); always NUL-terminates within cap. Returns the byte length written. */
NOTEDEF int note_body_text(const Note *n, char *out, int cap) {
    int k = 0;
    if (!out || cap <= 0)
        return 0;
    for (int p = 0; n && p < n->body.nParas; p++) {
        if (p)                          /* a blank line separates paragraphs */
            for (int i = 0; i < 2 && k < cap - 1; i++)
                out[k++] = '\n';
        for (const char *c = n->body.paras[p]; *c && k < cap - 1; c++)
            out[k++] = *c;
    }
    out[k] = '\0';
    return k;
}

#endif /* NOTES_BACKEND_IMPLEMENTATION */
#endif /* NOTES_BACKEND_H */
