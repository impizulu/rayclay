/*
    issues.h - the backlog itself: the dataset, and the logic that scans it.

    THE MODEL LAYER. Every function below makes ZERO RayClay calls and names ZERO
    RayClay types; anything that draws, or that touches AppState, is in app.h.
    The one include is for TYPES: rayclay.h brings <stdint.h> and <stdbool.h>
    with it, so nothing here needs a system header of its own.

    A ROW IS 20 BYTES OF INDICES AND NOT ONE STRING. The title is composed where
    it is drawn, out of two shared word tables, so 100,000 issues cost one flat
    array and no allocation at all. title_of() is that composition.
*/
#ifndef APP_ISSUES_H
#define APP_ISSUES_H

#include "rayclay.h"

/* The dataset's own dimensions: geometry lives in theme.h, this is the model.
   AREAS sizes AREA[] below, and theme.h's AREA_INK carries one hue per entry in
   the same order. */
enum { ISSUES = 100000, QCAP = 48, VIEWS = 4, AREAS = 8 };
enum { V_INBOX = 0, V_MINE, V_MENTION, V_CLOSED };

/* THE BACKLOG IS A TIMELINE, NOT A BAG OF NUMBERS. seed() walks BACKWARDS from
   now, a drawn gap at a time, so the 100,000 span about three years of intake;
   a FIXED gap is the tell of a generated fixture. Two things fall out and both
   are load-bearing: the age is MONOTONE in the array index, so "newest first"
   costs no sort; and everything else is derived FROM the age. */
enum { INTAKE_GAP = 6 };          /* 0.0 to 0.5 h between two reports         */
enum { COMMENT_BINS = 64 };       /* comments are 0..63, so sort_key can bin  */

/* The one sort control's options, in the order it offers them. Adding one means
   teaching sort_key a new key. */
enum { S_NEWEST = 0, S_OLDEST, S_PRIORITY, S_DISCUSSED, SORTS };
static const char *const SORT[SORTS] = { "Newest first", "Oldest first",
                                         "Priority", "Most discussed" };

static const char *const VIEW[VIEWS] = { "Inbox", "Assigned to me", "Mentions", "Closed" };
static const char *const PRIO[4]     = { "P0", "P1", "P2", "P3" };
static const char *const ACTION[] = {
    "Crash", "Regression", "Flicker", "Memory leak", "Deadlock", "Race condition", "Wrong colour",
    "Missing focus ring", "Slow scroll", "Stale cache", "Off-by-one", "Dropped keystroke",
    "Broken tab order", "Truncated label", "Silent failure", "Timeout", "Layout jump",
    "Lost selection", "Stuck spinner", "Wrong sort order", "Blank state", "Frozen pointer" };
static const char *const OBJECT[] = {
    "the sidebar", "the command palette", "the search index", "the virtual list",
    "the settings sheet", "the export dialog", "the markdown parser", "the sync queue",
    "the offline cache", "the notification tray", "the keyboard router", "the theme loader",
    "the plugin host", "the undo stack", "the clipboard bridge", "the file watcher",
    "the diff viewer", "the autosave timer", "the login flow", "the emoji picker",
    "the thread view", "the draft store", "the table renderer", "the update checker" };
static const char *const AREA[AREAS] = { "Editor", "Sync", "Renderer", "Platform", "Search",
                                         "Mobile", "Desktop", "Web" };
static const char *const WHO[]  = { "amara", "devon", "hiro", "iris", "jonas", "leah", "mateo",
                                    "nadia", "omar", "priya", "quinn", "rafael", "sana", "theo" };
/* What people SAY on an issue: the sort of line that reads as work rather than
   as filler, because the pane exists to show a real thread. */
static const char *const REMARK[] = {
    "Reproduced on a clean profile. Same trace every time.",
    "Bisected to the layout pass - the frame before it is fine.",
    "Only when the window is under 900 px wide, which is why CI missed it.",
    "Not the parser: the same input through the CLI is correct.",
    "Attached a capture. The second frame is where it diverges.",
    "Same root cause as the sidebar one, I think. Worth closing together.",
    "Cannot reproduce on this build. Which revision were you on?",
    "Confirmed on Windows too, so it is not the compositor.",
    "Toggling the pane settles it, if anyone needs to ship around this today.",
    "This is a regression - it worked before the cache landed.",
    "Have a fix locally. Writing the test that fails without it first.",
    "Narrowed it to one branch. The guard is checking the wrong span." };

#define N(a) ((int)(sizeof (a) / sizeof (a)[0]))

typedef struct { uint32_t num; uint16_t verb, noun, area, age, comments;
                 uint8_t prio, who, open, flags; } Issue;

/* ONE COMMENT OF A THREAD, DERIVED RATHER THAN STORED: a thread is a pure
   function of the issue number and the reply's index, so it is stable across
   frames, sorts and runs without a byte of state. */
typedef struct { const char *who, *text; unsigned hoursAgo; } Reply;

static inline Reply reply_of(const Issue *it, int k)
{
    unsigned seed = it->num * 2654435761u;
    Reply    r;

    /* WALK the tables from a per-issue starting point rather than re-hashing per
       reply: a stride coprime with the table length visits every entry before
       repeating, so six visible replies are always six different people saying
       six different things. 5 against 14 names, 1 against 12 remarks. */
    r.who  = WHO[((seed >> 7) + (unsigned)k * 5u) % (unsigned)N(WHO)];
    r.text = REMARK[((seed >> 13) + (unsigned)k) % (unsigned)N(REMARK)];
    /* Newest LAST, as a thread reads: they fan forward from just after the issue
       was opened, so no reply is ever older than the issue itself. */
    r.hoursAgo = it->comments > 0
               ? (unsigned)it->age * (unsigned)(it->comments - k) / (unsigned)it->comments
               : 0u;
    return r;
}
enum { F_MINE = 1, F_MENTION = 2 };

/* rayclay.h pulls in no libc, so the two string jobs printf would otherwise do
   are done by hand. Element ids are not one of them: rcFormat's result is
   NUL-terminated, so `.id = rcFormat(m, "r%d", i).chars` is the idiom. */
static inline int cat(char *b, int cap, int at, const char *s)
{
    while (*s && at < cap - 1) b[at++] = *s++;
    if (cap > 0) b[at] = '\0';
    return at;
}
/* Grouped in threes: 61893 -> "61,893". printf has no portable grouping flag. */
static inline int cat_grouped(char *b, int cap, int at, unsigned v)
{
    char t[16];
    int  n = 0;
    do {
        if (n && n % 4 == 3) t[n++] = ',';
        t[n++] = (char)('0' + v % 10u);
    } while ((v /= 10u) && n < (int)sizeof t);
    while (n > 0 && at < cap - 1) b[at++] = t[--n];
    if (cap > 0) b[at] = '\0';
    return at;
}

/* An age, ready to draw. `unit` is the list row's single letter, `word` the
   detail pane's noun, left SINGULAR because rcFormat has no plural rule. */
typedef struct { unsigned value; const char *unit, *word; } Age;

static inline Age age_of(unsigned hours)
{
    Age a;
    if (hours < 24u)        { a.value = hours;         a.unit = "h"; a.word = "hour"; }
    else if (hours < 336u)  { a.value = hours / 24u;   a.unit = "d"; a.word = "day";  }
    else if (hours < 8760u) { a.value = hours / 168u;  a.unit = "w"; a.word = "week"; }
    else                    { a.value = hours / 8760u; a.unit = "y"; a.word = "year"; }
    return a;
}

/* The bucket an issue falls in under a given sort. Both keys are SMALL and
   bounded, which is what lets app.h order 100,000 rows in two counting passes.
   Bucket 0 is drawn first, so "most discussed" counts DOWN from the cap. */
static inline int sort_key(const Issue *it, int sort)
{
    if (sort == S_PRIORITY) return (int)it->prio;
    return COMMENT_BINS - 1 - (it->comments < COMMENT_BINS ? (int)it->comments
                                                           : COMMENT_BINS - 1);
}
static inline int sort_bins(int sort) { return sort == S_PRIORITY ? 4 : COMMENT_BINS; }

static inline char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }

/* `needle` is pre-folded once per rebuild, not once per row - the difference
   between a filter that keeps up with typing and one that does not. */
static inline bool has(const char *hay, const char *needle)
{
    int i, j;
    if (!needle[0]) return true;
    for (i = 0; hay[i]; i++) {
        for (j = 0; needle[j] && lower(hay[i + j]) == needle[j]; j++) ;
        if (!needle[j]) return true;
    }
    return false;
}
static inline void title_of(const Issue *it, char *b, int cap)
{
    cat(b, cap, cat(b, cap, cat(b, cap, 0, ACTION[it->verb]), " in "), OBJECT[it->noun]);
}

/* One seed -> the same 100,000 issues on every machine. It returns the HIGH 16
   bits: an LCG's low bits have a short period, so `% n` on one visibly repeats
   and the list fills with adjacent duplicate titles. */
static inline uint32_t nxt(uint32_t *s)
{
    return (*s = *s * 1664525u + 1013904223u) >> 16;
}

static inline bool in_view(const Issue *it, int v)
{
    if (v == V_CLOSED)  return !it->open;
    if (v == V_MINE)    return it->open && (it->flags & F_MINE);
    if (v == V_MENTION) return it->open && (it->flags & F_MENTION);
    return it->open != 0;
}

#endif /* APP_ISSUES_H */
