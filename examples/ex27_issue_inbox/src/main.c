/* ============================================================================
 *  ex27 - Issue Inbox: GitHub Issues, keyboard-first, over 100,000 issues.
 *
 *  Hold j. The highlight flies down a list of 100,000 rows and the issue page
 *  beside it keeps up, frame by frame, because neither cost depends on the size
 *  of the dataset. Five ideas carry it:
 *
 *    1. rcVirtualList declares the VIEWPORT, not the dataset, so drawing costs
 *       the same at 100 rows and at 100,000.
 *    2. A row is 28 bytes of indices and no string. Its title is composed from
 *       word tables where it is drawn, so the fixture allocates nothing.
 *    3. Filtering permutes an index array. No record moves, so the list, the
 *       counts and the issue page cannot disagree about what passed.
 *    4. The O(issues) scan runs on a CHANGE, never on a frame, so scrolling and
 *       resizing never repeat it.
 *    5. The search field IS the filter, written out as text. Typing parses it
 *       and every click elsewhere writes it back, so the two never disagree.
 *
 *  One translation unit, for desktop, web and phones, with no assets and no
 *  socket. The order is the one you would read it in: the data, the query, the
 *  operations, the small pieces, then each page, update and layout.
 *  Keys: j k x c / e r p and Enter, as on GitHub and Linear; ? lists them all.
 *  Build target: rayclay_ex27_issue_inbox
 * ========================================================================= */
#include "rayclay.h"

#include "icons/rc_icons_arrow_left.h"
#include "icons/rc_icons_bell.h"
#include "icons/rc_icons_chart_column.h"
#include "icons/rc_icons_check.h"
#include "icons/rc_icons_chevron_down.h"
#include "icons/rc_icons_chevron_up.h"
#include "icons/rc_icons_circle_check.h"
#include "icons/rc_icons_circle_dot.h"
#include "icons/rc_icons_flag.h"
#include "icons/rc_icons_message_square.h"
#include "icons/rc_icons_moon.h"
#include "icons/rc_icons_panel_right.h"
#include "icons/rc_icons_rayclay_logo.h"
#include "icons/rc_icons_search.h"
#include "icons/rc_icons_settings.h"
#include "icons/rc_icons_star.h"
#include "icons/rc_icons_star_filled.h"
#include "icons/rc_icons_sun.h"
#include "icons/rc_icons_tag.h"
#include "icons/rc_icons_user.h"
#include "icons/rc_icons_x.h"

/* ---- the look: GitHub's Primer colors, in both themes ---------------------- */

/* The class bodies are the looks app_classes() names in each theme: hover as the wash under
   the pointer, navOn as a toggled tool's fill, the keyboard's row in its tint, GitHub's
   green button at rest, hovered and pressed, and the same green on a milestone's bar. A
   class names its color literally, so each theme spells its own (#rrggbbaa where the color
   is translucent). */
typedef struct {
    RC_Color    canvas, subtle, border, divider, fg, muted, accent, hover, navOn;
    RC_Color    open, closed, go;
    RC_Color    tab, star, bubble;
    const char *wash, *toggled, *highlight, *goButton, *goBar;
    bool        dark;
} Palette;

static const Palette LIGHT = {
    .canvas    = { 255, 255, 255, 255 },
    .subtle    = { 246, 248, 250, 255 },
    .border    = { 209, 217, 224, 255 },
    .divider   = { 222, 228, 233, 255 },
    .fg        = {  31,  35,  40, 255 },
    .muted     = {  89,  99, 110, 255 },
    .accent    = {   9, 105, 218, 255 },
    .hover     = { 129, 139, 152,  26 },
    .navOn     = { 129, 139, 152,  48 },
    .open      = {  26, 127,  55, 255 },
    .closed    = { 130,  80, 223, 255 },
    .go        = {  31, 136,  61, 255 },
    .tab       = { 253, 140, 115, 255 },
    .star      = { 218, 165,  32, 255 },
    .bubble    = {  31,  35,  40, 240 },
    .wash      = "hover:bg-[#818b981a]",
    .toggled   = "bg-[#818b9830]",
    .highlight = "bg-[#ddf4ff]",
    .goButton  = "bg-[#1f883d] hover:bg-[#1c8139] active:bg-[#186e30]",
    .goBar     = "accent-[#1f883d]",
    .dark      = false,
};

static const Palette DARK = {
    .canvas    = {  13,  17,  23, 255 },
    .subtle    = {  22,  27,  34, 255 },
    .border    = {  61,  68,  77, 255 },
    .divider   = {  38,  44,  54, 255 },
    .fg        = { 240, 246, 252, 255 },
    .muted     = { 145, 152, 161, 255 },
    .accent    = {  68, 147, 248, 255 },
    .hover     = { 101, 108, 118,  40 },
    .navOn     = { 101, 108, 118,  70 },
    .open      = {  63, 185,  80, 255 },
    .closed    = { 171, 125, 248, 255 },
    .go        = {  35, 134,  54, 255 },
    .tab       = { 247, 129, 102, 255 },
    .star      = { 227, 179,  65, 255 },
    .bubble    = {  48,  54,  61, 245 },
    .wash      = "hover:bg-[#656c7628]",
    .toggled   = "bg-[#656c7646]",
    .highlight = "bg-[#122640]",
    .goButton  = "bg-[#238636] hover:bg-[#29903b] active:bg-[#237a32]",
    .goBar     = "accent-[#238636]",
    .dark      = true,
};

static Palette app_palette(bool dark) {
    return dark ? DARK : LIGHT;
}

/* Every library control reads RC_Style, derived here from the palette, so a
   field or a menu can never drift from the panes around it. */
static RC_Style app_style(bool dark) {
    const Palette p = app_palette(dark);
    RC_Style      s = dark ? rcStyleDark() : rcStyleLight();

    s.background   = p.canvas;
    s.surface      = p.canvas;
    s.surfaceAlt   = p.subtle;
    s.chrome       = p.subtle;
    s.border       = p.border;
    s.text         = p.fg;
    s.textMuted    = p.muted;
    s.primary      = p.accent;      /* focus and selection; the green button is a class */
    s.primaryHover = p.accent;
    s.success      = p.open;
    s.radius       = 8.0f;          /* the library's medium corner is then 6 px, Primer's */
    return s;
}

/* The looks the panes repeat, named again each time a theme is installed (in main() and in
   update()) from that theme's class bodies above. A state prefix reads the box's own state,
   so a row lights under the pointer with no rcShowsHover branch; a run of text has no state
   of its own and reads rcShowsHover for its color, as a glyph does. "go" is GitHub's green
   call to action on an RC_BTN_PRIMARY button, where the variant alone would fill with this
   app's blue. Its dark fill gets the variant's white ink; disabled it fades whole, which over
   the canvas comes close to GitHub's disabled green in either theme. "go-bar" fills a
   progress bar with that green. */
static void app_classes(const Palette *p) {
    rcDefineClass("wash", p->wash);
    rcDefineClass("toggled", p->toggled);
    rcDefineClass("highlight", p->highlight);
    rcDefineClass("go", p->goButton);
    rcDefineClass("go-bar", p->goBar);
}

/* The live palette, set with the style between frames: in main() before the
   first, and in update() when the theme flips. */
static Palette ui;

/* Geometry that is a choice. COMPACT_W sits above a phone on its side, so a
   phone keeps the one arm in both orientations and turning it changes nothing
   but the arrangement. CTRL_H is what a 14 px label makes of a menu button, so
   the fields and buttons beside one take the same number: h-8 in a class, as
   HIT_MIN is h-11. BAND_TABS_W is what the band needs to hold the tabs and still
   keep the window's own controls, and BAR_LINE_W what the list's header needs to
   hold everything on one line. */
enum { BAR_H = 56, TABS_H = 44, CTRL_H = 32, HIT_MIN = 44 };
enum { ROW_H = 62, ROW_H_TALL = 104, ROW_WIDE_W = 700, ROW_MID_W = 500 };
enum { SPLIT_MIN = 1100, COMPACT_W = 860, SHORT_H = 500, BAND_TABS_W = 760, BAR_LINE_W = 860 };
enum { SIDEBAR_W = 216, SIDEBAR_AT = 760 };   /* below that the thread keeps its width */

/* ---- the data -------------------------------------------------------------- */

/* The fixture is the tracker of a fictional code editor. SPARE is room in FRONT
   of it for the issues one session files: the newest issue lives at the lowest
   index, so "newest first" stays the array's own order. */
enum { ISSUES = 100000, SPARE = 64, QCAP = 192, WORDS = 6, WEEKS = 26 };
enum { TITLE_CAP = 120, BODY_CAP = 2000, TALK_CAP = 65536, ACTS = 1024 };
enum { NONE = -1, ANY = -2, CHOSE_ANY = -3 };   /* nobody; no filter; a menu chose "any" */
enum { COMMENT_BINS = 64, PRIO_BINS = 5 };

enum { L_BUG, L_ENHANCEMENT, L_DOCS, L_PERFORMANCE, L_QUESTION, L_GOOD_FIRST, L_HELP_WANTED,
       L_P0, L_P1, L_P2, L_P3, L_EDITOR, L_SEARCH, L_SYNC, L_TERMINAL, L_RENDERER, L_EXTENSIONS,
       LABELS };

typedef struct { const char *name, *about; RC_Color color; } Label;

/* GitHub's own label swatches. A chip fills with its color and writes in black
   or white, whichever reads, so a repository can pick any color it likes. */
static const Label LABEL[LABELS] = {
    { "bug",              "Something is not working",            { 215,  58,  74, 255 } },
    { "enhancement",      "New feature or request",              { 162, 238, 239, 255 } },
    { "documentation",    "Improvements or additions to docs",   {   0, 117, 202, 255 } },
    { "performance",      "Slower or heavier than it should be", {  83,  25, 231, 255 } },
    { "question",         "Further information is requested",    { 216, 118, 227, 255 } },
    { "good first issue", "Good for newcomers",                  { 112,  87, 255, 255 } },
    { "help wanted",      "Extra attention is needed",           {   0, 134, 114, 255 } },
    { "P0",               "Drop everything",                     { 182,   2,   5, 255 } },
    { "P1",               "Fix this release",                    { 217,  63,  11, 255 } },
    { "P2",               "Fix soon",                            { 251, 202,   4, 255 } },
    { "P3",               "Fix when someone can",                { 194, 224, 198, 255 } },
    { "editor",           "Text buffer, cursors and folding",    { 197, 222, 245, 255 } },
    { "search",           "Find, replace and the index",         { 191, 212, 242, 255 } },
    { "sync",             "Settings and workspace sync",         { 212, 197, 249, 255 } },
    { "terminal",         "The integrated terminal",             { 254, 242, 192, 255 } },
    { "renderer",         "Layout, text and the GPU",            { 249, 208, 196, 255 } },
    { "extensions",       "The extension host and its API",      { 194, 224, 198, 255 } },
};

/* What a title is about, and the label it files under. */
typedef struct { const char *name; int area; } Subject;

static const Subject SUBJECT[] = {
    { "command palette", L_EDITOR },     { "file tree", L_EDITOR },
    { "search index", L_SEARCH },        { "minimap", L_RENDERER },
    { "diff viewer", L_EDITOR },         { "terminal panel", L_TERMINAL },
    { "tab bar", L_RENDERER },           { "settings sync", L_SYNC },
    { "language server", L_EXTENSIONS }, { "git gutter", L_EDITOR },
    { "find and replace bar", L_SEARCH }, { "autosave timer", L_SYNC },
    { "extension host", L_EXTENSIONS },  { "theme loader", L_RENDERER },
    { "split editor", L_EDITOR },        { "breadcrumb bar", L_RENDERER },
    { "status bar", L_RENDERER },        { "inline completion", L_EXTENSIONS },
    { "merge editor", L_EDITOR },        { "keyboard router", L_EDITOR },
    { "clipboard bridge", L_SYNC },      { "font renderer", L_RENDERER },
    { "notebook view", L_EDITOR },       { "remote workspace", L_SYNC },
    { "shell integration", L_TERMINAL }, { "search results pane", L_SEARCH },
};
static const char *const PROBLEM[] = {
    "crashes", "flickers", "freezes", "loses focus", "renders blank", "leaks memory",
    "drops keystrokes", "ignores the theme", "shows stale results", "jumps to the wrong line",
    "misses updates", "hangs for several seconds", "duplicates entries",
    "resets its scroll position", "stutters", "steals the selection" };
static const char *const CONTEXT[] = {
    "on window resize", "after waking from sleep", "when a file is renamed",
    "with more than 10k files", "on a 4K display", "during IME composition",
    "after switching workspaces", "in split view", "on first launch", "when offline",
    "under Wayland", "with a right-to-left locale", "while indexing", "after undo",
    "with a custom keymap", "on Windows 11" };
enum { CTX_WAYLAND = 10, CTX_WINDOWS = 15 };   /* the two that name a platform */
static const char *const REQUEST[] = {
    "should remember its last position", "needs a keyboard shortcut",
    "should support multiple selections", "needs a compact mode",
    "should follow the system theme", "should work offline", "needs a better empty state",
    "should show its progress", "needs a setting to turn it off",
    "should support drag and drop", "should keep its state across restarts",
    "needs an accessible name for screen readers" };
/* Each word table's length, as the compiler counts it. */
enum { SUBJECTS = (int)(sizeof SUBJECT / sizeof SUBJECT[0]),
       PROBLEMS = (int)(sizeof PROBLEM / sizeof PROBLEM[0]),
       CONTEXTS = (int)(sizeof CONTEXT / sizeof CONTEXT[0]),
       REQUESTS = (int)(sizeof REQUEST / sizeof REQUEST[0]) };

enum { ME = 0, PEOPLE = 15 };
static const char *const HANDLE[PEOPLE] = {
    "samreyes", "amara-okafor", "devonlee", "hiro-tanaka", "irisvdb", "jonas-weber", "leahcohen",
    "mateo-rivas", "nadiak", "omarfaruk", "priyanair", "quinnmoss", "rafael-silva", "sana-mir",
    "theodupont" };
static const char *const INITIALS[PEOPLE] = {
    "SR", "AO", "DL", "HT", "IV", "JW", "LC", "MR", "NK", "OF", "PN", "QM", "RS", "SM", "TD" };

typedef struct { const char *name, *due, *about; } Milestone;

enum { MILESTONES = 6 };
static const Milestone MILESTONE[MILESTONES] = {
    { "v2.8.4", "Due by November 20, 2026",
      "Patch release: the crash reports from the 2.8 rollout." },
    { "v2.9", "Due by December 12, 2026",
      "Startup time, the new file tree and fractional scaling under Wayland." },
    { "v3.0", "Due by March 6, 2027",
      "Extension API 3, remote workspaces and the settings redesign." },
    { "Backlog", "No due date", "Accepted work that has no release yet." },
    { "v2.8.3", "Closed on October 2, 2026", "Patch release: sync and autosave fixes." },
    { "v2.8", "Closed on July 17, 2026", "The new renderer, split editors and the merge editor." },
};

/* What people say. It reads as work rather than filler because the page exists
   to show a real thread, and some of it is Markdown, because the page renders it. */
static const char *const REMARK[] = {
    "Reproduced on a clean profile. Same trace every time.",
    "Bisected to the layout pass: the frame before it is fine.",
    "Only when the window is under 900 px wide, which is why CI missed it.",
    "Not the parser: the same input through the CLI is correct.",
    "Attached a capture. The second frame is where it diverges.",
    "Same root cause as #48213, I think. Worth closing together.",
    "Cannot reproduce on `2.8.3`. Which build were you on?",
    "Confirmed on Windows too, so it is not the compositor.",
    "Toggling the pane settles it, if anyone needs to ship around this today.",
    "This worked before the cache landed, so `git bisect` should find it quickly.",
    "Have a fix locally. Writing the test that fails without it first.",
    "Narrowed it to one branch: the guard in `layout_pass` checks the wrong span.",
    "@priyanair could you take a look? You know this code best.",
    "**Confirmed.** Moving this into the next milestone.",
    "The profiler puts 38 ms in `measure_text`, all of it on the first frame.",
    "A smaller repro:\n```\neditor --safe-mode big.json\nresize window to 640x480\n```",
    "Steps that bring it back:\n- Close every tab\n- Run **Reload Window**\n- Open the file",
    "> Only when the window is under 900 px wide\n\nSame here, on a 13 inch laptop.",
    "PR is up. It needs one more review before it can go in.",
    "Thanks for the report. Tagging this for triage on Thursday.",
    "Checklist for the fix:\n- [x] Failing test\n- [x] Patch\n- [ ] Release note",
    "Happens with every extension disabled, so it is ours.",
    "@devonlee this one is yours if you want it; it overlaps with your scheduler work.",
    "Measured it: **41 ms** at p95, against a 16 ms budget." };
enum { REMARKS = (int)(sizeof REMARK / sizeof REMARK[0]) };
static const char *const PLATFORM[] = { "macOS 15.1", "Windows 11", "Fedora 44", "Ubuntu 24.04" };

/* A ROW IS 28 BYTES AND NOT ONE STRING. labels0, assignee0 and milestone0 keep
   the issue as it was triaged, so the timeline's history stays true after a
   session changes the live fields. */
typedef struct {
    uint32_t num, labels, labels0;
    uint16_t age, comments;                  /* hours since it was opened; its thread */
    uint8_t  subject, problem, context, kind, who, open, flags, mine;
    int8_t   assignee, assignee0, milestone, milestone0;
} Issue;
typedef char IssueIs28Bytes[sizeof(Issue) == 28 ? 1 : -1];   /* the compiler holds it to that */
enum { F_MENTION = 1, F_SUBSCRIBED = 2, F_PICKED = 4, F_CLOSED0 = 8, F_CLOSED_NOW = 16 };

/* Sorts, in menu order, with the qualifier each writes into the query. */
enum { S_NEWEST, S_OLDEST, S_PRIORITY, S_DISCUSSED, SORTS };
static const char *const SORT[SORTS]      = { "Newest", "Oldest", "Priority", "Comments" };
static const char *const SORT_QUAL[SORTS] = { "created-desc", "created-asc", "priority",
                                              "comments-desc" };

/* THE QUERY, parsed. An empty facet means all of it. */
typedef struct {
    bool     closed, mentions, subscribed;
    int      sort, author, assignee, milestone;
    uint32_t labels;
    char     words[QCAP];
} Filter;

/* What one rebuild counts, for the tabs, the menus and every page. */
typedef struct {
    int perState[2], openAll, labelOpen[LABELS], labelHere[LABELS];
    int msOpen[MILESTONES], msClosed[MILESTONES], reporter[PEOPLE];
    int weekOpened[WEEKS], weekClosed[WEEKS];
} Counts;

/* One thing the user did this session. The timeline replays these after the
   issue's own history; a comment's words are at talk[text]. */
enum { ACT_COMMENT, ACT_CLOSED, ACT_REOPENED, ACT_LABELED, ACT_UNLABELED, ACT_ASSIGNED,
       ACT_UNASSIGNED, ACT_MILESTONED, ACT_UNMILESTONED };
typedef struct { int issue, kind, arg, text; double at; } Activity;

typedef struct { char title[TITLE_CAP], body[BODY_CAP]; } Filed;

enum { PAGE_ISSUES, PAGE_MILESTONES, PAGE_LABELS, PAGE_PULSE, PAGES };

/* The whole app in one struct. `sel` is the permutation: filtering reorders
   32-bit indices, never records. */
typedef struct {
    Issue    issue[SPARE + ISSUES];
    int32_t  sel[SPARE + ISSUES];        /* the indices that pass, in list order      */
    int32_t  order[SPARE + ISSUES];      /* where the counting sort stably lays them  */
    Filed    filed[SPARE];
    Activity act[ACTS];
    char     talk[TALK_CAP];
    int      first, nextNum, nact, talkUsed;
    int      count, picked;
    Counts   n;
    Filter   f;
    char     query[QCAP], labelQuery[32];
    char     reply[BODY_CAP];
    int      page, selRow, selIssue, lastRow, expanded, sortWas;
    float    headH;
    float    splitAt;
    bool     dirty, keepSel, stayAt, reveal, pageTop, cursorShown, detailOpen, split, preview;
    bool     dark, flipTheme, helpOpen, starred, watching, focusQuery, focusReply;
    bool     labelsOpen, showClear;
    bool     seeded, shown;  /* the issues exist; the window's first frame is drawn */
    /* Holding j or k: which way, since when, and the rows owed so far. */
    int      flyDir;
    double   flyStart, flyLast, flyOwed;
    bool     flying;
    /* The New issue dialog. */
    bool     composing, composeFresh, createMore, draftPreview;
    char     draftTitle[TITLE_CAP], draftBody[BODY_CAP];
    int      draftKind, draftPrio, draftWho, draftMilestone;
    const char *whoItems[PEOPLE + 1], *milestoneItems[MILESTONES + 1];
    /* Where a Markdown link's id comes from: unique on the page, this frame. */
    int      mdLinks;
    /* The Pulse chart, oldest week first. */
    float    chartOpened[WEEKS], chartClosed[WEEKS];
    char     weekText[WEEKS][12];
    const char *weekLabel[WEEKS];
} AppState;

/* ---- small string jobs: rayclay.h pulls in no libc ------------------------- */

static int cat(char *b, int cap, int at, const char *s) {
    while (*s && at < cap - 1) b[at++] = *s++;
    if (cap > 0) b[at] = '\0';
    return at;
}

static char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }

/* `needle` is folded once per rebuild, not once per row: the difference between
   a filter that keeps up with typing and one that does not. */
static bool has(const char *hay, const char *needle) {
    int i, j;

    if (!needle[0]) return true;
    for (i = 0; hay[i]; i++) {
        for (j = 0; needle[j] && lower(hay[i + j]) == needle[j]; j++) ;
        if (!needle[j]) return true;
    }
    return false;
}

/* Case-blind equality of the C string `a` against the first `n` bytes of `b`. A
   dash in `b` matches a space, which is how a query spells `good-first-issue`. */
static bool same(const char *a, const char *b, int n) {
    int i;

    for (i = 0; i < n; i++) {
        const char c = b[i] == '-' && a[i] == ' ' ? ' ' : lower(b[i]);
        if (!a[i] || lower(a[i]) != c) return false;
    }
    return a[n] == '\0';
}

static bool blank(const char *s) {
    while (*s == ' ' || *s == '\n') s++;
    return !*s;
}

/* ---- the fixture ----------------------------------------------------------- */

/* One seed makes the same 100,000 issues on every machine. It returns the high
   bits: an LCG's low bits have a short period, and `% n` on them repeats. */
static uint32_t nxt(uint32_t *s) {
    return (*s = *s * 1664525u + 1013904223u) >> 16;
}

/* THE BACKLOG IS A TIMELINE. The walk goes backwards from now a drawn gap at a
   time, so age only ever grows along the array and newest-first costs no sort.
   Intake comes in waves around each release, which is what gives the Pulse chart
   a shape, and everything else follows from the age: a queue that is worked
   closes its old issues. */
static void seed(AppState *st) {
    uint32_t s = 0x1B0C5285u;
    unsigned tenths = 0;
    int      i;

    st->first   = SPARE;
    st->nextNum = ISSUES + 1001;
    for (i = 0; i < ISSUES; i++) {
        Issue   *it = &st->issue[SPARE + i];
        const unsigned phase = tenths / 1680u % 13u;
        unsigned r, days, heat;
        int      ms = NONE;

        tenths += nxt(&s) % (3u + (phase < 6u ? phase : 12u - phase));
        it->num     = (uint32_t)(ISSUES - i) + 1000u;
        it->age     = (uint16_t)(6u + tenths / 10u);
        days        = it->age / 24u;
        it->subject = (uint8_t)(nxt(&s) % (unsigned)SUBJECTS);
        it->problem = (uint8_t)(nxt(&s) % 240u);
        it->context = (uint8_t)(nxt(&s) % (unsigned)CONTEXTS);
        /* Skewed, as a real tracker is: a few people file most of it, and one issue
           in a hundred is yours, so "Your issues" is a list. */
        r = nxt(&s) % 1000u;
        it->who     = (uint8_t)(r < 10u ? ME : 1u + r * r * (PEOPLE - 1u) / 1000000u);

        r = nxt(&s) % 100u;
        it->kind   = (uint8_t)(r < 58u ? L_BUG : r < 78u ? L_ENHANCEMENT : r < 86u ? L_DOCS :
                               r < 93u ? L_PERFORMANCE : L_QUESTION);
        it->labels = (1u << it->kind) | (1u << SUBJECT[it->subject].area);
        /* Skewed, as a real backlog is: a uniform draw puts 25,000 fires in it. */
        r = nxt(&s) % 100u;
        if (it->kind == L_BUG || it->kind == L_PERFORMANCE || r < 40u)
            it->labels |= 1u << (r < 3u ? L_P0 : r < 15u ? L_P1 : r < 55u ? L_P2 : L_P3);
        r = nxt(&s) % 100u;
        if (r < 4u)      it->labels |= 1u << L_GOOD_FIRST;
        else if (r < 7u) it->labels |= 1u << L_HELP_WANTED;

        r = nxt(&s) % 100u;
        if (days < 30u)       ms = r < 45u ? 0 : r < 60u ? 1 : NONE;
        else if (days < 120u) ms = r < 30u ? 1 : r < 40u ? 2 : r < 46u ? 3 : NONE;
        else if (days < 300u) ms = r < 2u ? 4 : r < 5u ? 3 : NONE;
        else if (days < 540u) ms = r < 5u ? 5 : r < 8u ? 3 : NONE;
        else if (r < 3u)      ms = 3;
        it->milestone = (int8_t)ms;

        /* A release's own issues are closed first, which is what a milestone's
           progress bar is for, and a backlog is what is still waiting. */
        r = ms == 0 ? 18u : ms == 1 ? 45u : ms == 2 ? 85u : ms == 3 ? 70u : ms >= 4 ? 0u :
            days < 2u ? 96u : days < 7u ? 87u : days < 30u ? 60u : days < 120u ? 32u : 11u;
        it->open  = (uint8_t)(nxt(&s) % 100u < r);
        it->flags = (uint8_t)((nxt(&s) % 100u < 6u ? F_MENTION : 0) |
                              (nxt(&s) % 100u < 5u ? F_SUBSCRIBED : 0) |
                              (it->open ? 0 : F_CLOSED0));

        /* Attention follows severity loosely: a count that is a function of the
           priority would make "most commented" a second copy of "priority". */
        heat = nxt(&s) % 18u + days / 240u;
        if (it->labels & (1u << L_P0)) heat += 12u;
        if (nxt(&s) % 9u == 0u) heat += 14u;
        it->comments = (uint16_t)(heat > COMMENT_BINS - 2u ? COMMENT_BINS - 2u : heat);

        r = nxt(&s) % 100u;
        it->assignee = (int8_t)(r < 9u ? ME : r < 42u ? NONE : 1 + (int)(nxt(&s) % 14u));

        it->labels0    = it->labels;
        it->assignee0  = it->assignee;
        it->milestone0 = it->milestone;
    }
}

/* The title, composed where it is needed from the word tables. */
static void title_into(const AppState *st, int i, char *b, int cap) {
    const Issue *it = &st->issue[i];
    const char  *s  = SUBJECT[it->subject].name;
    const char  *part[5] = { s, " ", NULL, NULL, NULL };
    int          k, at = 0;

    if (i < SPARE) {
        cat(b, cap, 0, st->filed[i].title);
        return;
    }
    switch (it->kind) {
    case L_ENHANCEMENT:
        part[2] = REQUEST[it->problem % REQUESTS];
        break;
    case L_DOCS:
        part[0] = "Document the ";  part[1] = s;  part[2] = " settings";
        break;
    case L_QUESTION:
        part[0] = "How do I configure the ";  part[1] = s;  part[2] = "?";
        break;
    case L_PERFORMANCE:
        part[2] = "is slow ";  part[3] = CONTEXT[it->context];
        break;
    default:
        part[2] = PROBLEM[it->problem % PROBLEMS];
        part[3] = " ";
        part[4] = CONTEXT[it->context];
        break;
    }
    for (k = 0; k < 5 && part[k]; k++) at = cat(b, cap, at, part[k]);
    if (b[0] >= 'a' && b[0] <= 'z') b[0] = (char)(b[0] - 32);
}

static const char *title_of(RC_Arena *m, const AppState *st, int i) {
    char *b = (char *)rcArenaAlloc(m, TITLE_CAP);

    if (!b) return "";
    title_into(st, i, b, TITLE_CAP);
    return b;
}

/* The issue's first comment, Markdown, derived from its record. */
static RC_String body_of(RC_Arena *m, const AppState *st, int i) {
    const Issue *it   = &st->issue[i];
    const char  *what = SUBJECT[it->subject].name;
    const char  *when = CONTEXT[it->context];
    /* A title that names a platform keeps its report on that platform. */
    const char  *os   = it->context == CTX_WINDOWS ? "Windows 11" :
                        it->context == CTX_WAYLAND ? "Fedora 44" : PLATFORM[it->num % 4u];

    if (i < SPARE) return rcStringFromCStr(st->filed[i].body);
    switch (it->kind) {
    case L_ENHANCEMENT:
        return rcFormat(m,
            "### The problem\nToday the %s %s now and then, and it gets in the way every day.\n\n"
            "### Proposal\n- [x] Agree on the behavior\n- [ ] Add it behind a setting\n"
            "- [ ] Turn it on by default in the next release\n\n"
            "Related to #%u.", what, PROBLEM[it->problem % PROBLEMS],
            it->num > 2000u ? it->num - 977u : 1001u);
    case L_DOCS:
        return rcFormat(m,
            "The settings of the **%s** are missing from the handbook, and search finds only a "
            "forum thread from last year.\n\n- [ ] List every setting\n- [ ] Add an example\n"
            "- [ ] Link it from `editor --help`", what);
    case L_QUESTION:
        return rcFormat(m,
            "I would like the %s to behave differently %s, and I cannot find the setting.\n\n"
            "Running **2.8.%u** on %s.", what, when, it->num % 4u, os);
    case L_PERFORMANCE:
        return rcFormat(m,
            "The %s takes far too long %s.\n\n```\nframe 1204  41.2 ms  layout_pass\n"
            "frame 1205  38.7 ms  measure_text\nframe 1206  39.1 ms  layout_pass\n```\n"
            "The budget is 16 ms. Running **2.8.%u** on %s.", what, when, it->num % 4u, os);
    default:
        return rcFormat(m,
            "### What happened\nThe %s %s %s. It comes back every time on a clean profile.\n\n"
            "### Steps to reproduce\n- Start with `editor --safe-mode`\n"
            "- Open any large workspace\n- Keep using the %s for a minute\n\n"
            "### Expected\nIt keeps working and nothing is lost.\n\n"
            "**Version:** 2.8.%u on %s", what, PROBLEM[it->problem % PROBLEMS], when,
            what, it->num % 4u, os);
    }
}

/* When the fixture closed an issue: within four months of its opening, so what a
   week closes tracks what the weeks before it opened. */
static unsigned fixture_closed_ago(const Issue *it) {
    const unsigned life = 24u * (1u + it->num % 120u);

    return (unsigned)it->age > life ? (unsigned)it->age - life : (unsigned)it->age / 2u;
}

/* A close this session is now, on the row and in Pulse, as the timeline says. */
static unsigned closed_ago(const Issue *it) {
    return it->flags & F_CLOSED_NOW ? 0u : fixture_closed_ago(it);
}

/* ONE REPLY, DERIVED RATHER THAN STORED: a thread is a pure function of the issue
   number and the reply's index, so it is stable across frames and sorts without
   a byte of state. Strides coprime with each table visit every entry before
   repeating, and replies fan across the issue's life, so none is older than the
   issue and none comes after its closing. */
typedef struct { int who; const char *text; unsigned hoursAgo; } Reply;

static Reply reply_of(const Issue *it, int k) {
    const unsigned seedv = it->num * 2654435761u;
    const unsigned last  = it->flags & F_CLOSED0 ? fixture_closed_ago(it) : 0u;
    Reply r;

    r.who  = 1 + (int)(((seedv >> 7) + (unsigned)k * 5u) % (unsigned)(PEOPLE - 1));
    r.text = REMARK[((seedv >> 13) + (unsigned)k) % (unsigned)REMARKS];
    r.hoursAgo = last + ((unsigned)it->age - last) * (unsigned)(it->comments - k)
                        / ((unsigned)it->comments + 1u);
    return r;
}

/* ---- the query: `is:issue state:open label:bug author:priyanair crash` ----- */

static void filter_reset(Filter *f) {
    f->closed = f->mentions = f->subscribed = false;
    f->sort = S_NEWEST;
    f->author = f->assignee = f->milestone = ANY;
    f->labels = 0;
    f->words[0] = '\0';
}

static bool filter_is_default(const Filter *f) {
    return !f->closed && !f->mentions && !f->subscribed && f->sort == S_NEWEST &&
           f->author == ANY && f->assignee == ANY && f->milestone == ANY && !f->labels &&
           !f->words[0];
}

/* The first person whose handle starts with these `n` bytes, so a half-typed
   `author:pr` already filters. An unknown name changes nothing. */
static int person_named(const char *s, int n) {
    int i, j;

    if (same("@me", s, n)) return ME;
    for (i = 0; i < PEOPLE && n > 0; i++) {
        for (j = 0; j < n && HANDLE[i][j] && HANDLE[i][j] == lower(s[j]); j++) ;
        if (j == n) return i;
    }
    return ANY;
}

/* A qualifier this does not know is IGNORED rather than refused, so a half-typed
   `is:clo` leaves the list alone until the word is complete. */
static void parse_query(const char *q, Filter *f) {
    int at = 0, w = 0, i, n, k;

    filter_reset(f);
    while (q[at]) {
        const char *t = q + at, *v;
        int         colon = -1;

        for (n = 0; t[n] && t[n] != ' '; n++) if (t[n] == ':' && colon < 0) colon = n;
        at += n;
        while (q[at] == ' ') at++;
        if (n == 0) continue;
        if (colon < 0) {
            if (w && w < QCAP - 1) f->words[w++] = ' ';
            for (i = 0; i < n && w < QCAP - 1; i++) f->words[w++] = lower(t[i]);
            f->words[w] = '\0';
            continue;
        }
        v = t + colon + 1;
        k = n - colon - 1;
        if (same("is", t, colon) || same("state", t, colon)) {
            if (same("open", v, k))       f->closed = false;
            if (same("closed", v, k))     f->closed = true;
            if (same("subscribed", v, k)) f->subscribed = true;
        } else if (same("no", t, colon)) {
            if (same("assignee", v, k))  f->assignee = NONE;
            if (same("milestone", v, k)) f->milestone = NONE;
        } else if (same("mentions", t, colon)) {
            f->mentions = same("@me", v, k);
        } else if (same("author", t, colon)) {
            f->author = person_named(v, k);
        } else if (same("assignee", t, colon)) {
            f->assignee = person_named(v, k);
        } else if (same("milestone", t, colon)) {
            for (i = 0; i < MILESTONES; i++) if (same(MILESTONE[i].name, v, k)) f->milestone = i;
        } else if (same("label", t, colon)) {
            for (i = 0; i < LABELS; i++) if (same(LABEL[i].name, v, k)) f->labels |= 1u << i;
        } else if (same("sort", t, colon)) {
            for (i = 0; i < SORTS; i++) if (same(SORT_QUAL[i], v, k)) f->sort = i;
        }
    }
}

/* The inverse, in a fixed order so a round trip is stable. A label name with a
   space is written with dashes. The trailing space is where the caret lands. */
static void compose_query(const Filter *f, char *out, int cap) {
    int at = cat(out, cap, 0, f->closed ? "is:issue state:closed" : "is:issue state:open");
    int i;

    if (f->subscribed) at = cat(out, cap, at, " is:subscribed");
    if (f->mentions)   at = cat(out, cap, at, " mentions:@me");
    if (f->author >= 0) {
        at = cat(out, cap, at, " author:");
        at = cat(out, cap, at, f->author == ME ? "@me" : HANDLE[f->author]);
    }
    if (f->assignee == NONE) at = cat(out, cap, at, " no:assignee");
    if (f->assignee >= 0) {
        at = cat(out, cap, at, " assignee:");
        at = cat(out, cap, at, f->assignee == ME ? "@me" : HANDLE[f->assignee]);
    }
    if (f->milestone == NONE) at = cat(out, cap, at, " no:milestone");
    if (f->milestone >= 0) {
        at = cat(out, cap, at, " milestone:");
        at = cat(out, cap, at, MILESTONE[f->milestone].name);
    }
    for (i = 0; i < LABELS; i++) {
        int k;

        if (!(f->labels >> i & 1u)) continue;
        at = cat(out, cap, at, " label:");
        for (k = 0; LABEL[i].name[k] && at < cap - 1; k++)
            out[at++] = LABEL[i].name[k] == ' ' ? '-' : LABEL[i].name[k];
    }
    if (f->sort != S_NEWEST) {
        at = cat(out, cap, at, " sort:");
        at = cat(out, cap, at, SORT_QUAL[f->sort]);
    }
    if (f->words[0]) {
        at = cat(out, cap, at, " ");
        at = cat(out, cap, at, f->words);
    }
    cat(out, cap, at, " ");
}

/* Free text is ANDed word by word, and a word matches the title, the author or,
   written as digits with or without the #, the issue number. */
static bool word_matches(const Issue *it, const char *title, const char *w) {
    unsigned n = 0;
    int      i = w[0] == '#' ? 1 : 0;

    if (has(title, w) || has(HANDLE[it->who], w)) return true;
    if (!w[i]) return false;
    /* Past nine digits a number is no issue's, and would wrap onto one's. */
    for (; w[i]; i++) {
        if (w[i] < '0' || w[i] > '9' || n > 99999999u) return false;
        n = n * 10u + (unsigned)(w[i] - '0');
    }
    return n == it->num;
}

/* ---- the one O(issues) pass ------------------------------------------------ */

static int priority_of(const Issue *it) {
    int i;

    for (i = 0; i < 4; i++) if (it->labels >> (L_P0 + i) & 1u) return i;
    return 4;
}

static int thread_of(const Issue *it) { return (int)it->comments + (int)it->mine; }

/* Both keys are small and bounded, so two of the four sorts are stable counting
   passes with no comparison function, and stability is what keeps newest-first
   as the tie-break inside each. The other two follow from the array's order. */
static int sort_key(const Issue *it, int sort) {
    int c;

    if (sort == S_PRIORITY) return priority_of(it);
    c = thread_of(it);
    return COMMENT_BINS - 1 - (c < COMMENT_BINS ? c : COMMENT_BINS - 1);
}

static void sort_rows(AppState *st) {
    int start[COMMENT_BINS];
    const int sort = st->f.sort;
    const int bins = sort == S_PRIORITY ? PRIO_BINS : COMMENT_BINS;
    int i, j, at, n;

    if (sort == S_NEWEST) return;
    if (sort == S_OLDEST) {
        for (i = 0, j = st->count - 1; i < j; i++, j--) {
            const int32_t t = st->sel[i];
            st->sel[i] = st->sel[j];
            st->sel[j] = t;
        }
        return;
    }
    for (i = 0; i < bins; i++) start[i] = 0;
    for (i = 0; i < st->count; i++) start[sort_key(&st->issue[st->sel[i]], sort)]++;
    for (i = 0, at = 0; i < bins; i++) { n = start[i]; start[i] = at; at += n; }
    for (i = 0; i < st->count; i++) {
        const int k = sort_key(&st->issue[st->sel[i]], sort);
        st->order[start[k]++] = st->sel[i];
    }
    for (i = 0; i < st->count; i++) st->sel[i] = st->order[i];
}

/* What every page reads, whatever the query says. */
static void tally(Counts *n, const Issue *it) {
    int l;

    n->openAll += it->open;
    for (l = 0; l < LABELS; l++) if (it->open && it->labels >> l & 1u) n->labelOpen[l]++;
    if (it->milestone >= 0) {
        if (it->open) n->msOpen[it->milestone]++;
        else          n->msClosed[it->milestone]++;
    }
    if (it->age / 168 < WEEKS) {
        n->weekOpened[it->age / 168]++;
        n->reporter[it->who]++;
    }
    if (!it->open && closed_ago(it) / 168u < WEEKS) n->weekClosed[closed_ago(it) / 168u]++;
}

static bool filter_passes(const Filter *f, const Issue *it) {
    if (f->mentions && !(it->flags & F_MENTION)) return false;
    if (f->subscribed && !(it->flags & F_SUBSCRIBED)) return false;
    if (f->author != ANY && f->author != it->who) return false;
    if (f->assignee != ANY && f->assignee != it->assignee) return false;
    if (f->milestone != ANY && f->milestone != it->milestone) return false;
    return (it->labels & f->labels) == f->labels;
}

/* A different issue starts at its top, with its older comments folded. */
static void issue_changed(AppState *st) {
    st->expanded = NONE;
    st->preview  = false;
    st->pageTop  = true;
}

/* It carries the selection across BY IDENTITY, so the row you were reading keeps
   its highlight if it survives the new filter. keepSel leaves the page on an
   issue that just left the list; stayAt moves to the row that took its place. */
static void rebuild(AppState *st) {
    static Counts zero;           /* static storage: all counts 0 */
    const Filter *f = &st->f;
    Counts       *n = &st->n;
    char          words[QCAP], t[TITLE_CAP];
    const char   *word[WORDS];
    const int     was = st->selIssue;
    int           nw = 0, i, k;

    *n = zero;
    /* The free text is split ONCE here, not once per row. */
    cat(words, QCAP, 0, f->words);
    for (i = 0; words[i] && nw < WORDS; ) {
        word[nw++] = words + i;
        while (words[i] && words[i] != ' ') i++;
        if (words[i]) words[i++] = '\0';
    }

    st->count = st->picked = 0;
    for (i = st->first; i < SPARE + ISSUES; i++) {
        Issue *it = &st->issue[i];
        bool   pass, listed;

        tally(n, it);
        pass = filter_passes(f, it);
        if (pass && nw) {
            title_into(st, i, t, TITLE_CAP);
            for (k = 0; k < nw && word_matches(it, t, word[k]); k++) ;
            pass = k == nw;
        }
        if (pass) n->perState[it->open ? 0 : 1]++;
        listed = pass && (it->open != 0) != f->closed;
        if (listed) {
            st->sel[st->count++] = (int32_t)i;
            for (k = 0; k < LABELS; k++) if (it->labels >> k & 1u) n->labelHere[k]++;
            if (it->flags & F_PICKED) st->picked++;
        } else {
            it->flags &= (uint8_t)~F_PICKED;
        }
    }
    sort_rows(st);
    for (i = 0; i < WEEKS; i++) {
        st->chartOpened[i] = (float)n->weekOpened[WEEKS - 1 - i];
        st->chartClosed[i] = (float)n->weekClosed[WEEKS - 1 - i];
    }

    /* A new order starts at its top, as GitHub's does: the row you were on has landed
       somewhere thousands of rows down, and following it would look like no sort. */
    if (f->sort != st->sortWas) {
        st->sortWas  = f->sort;
        st->selIssue = NONE;
    }

    /* AFTER the sort: selRow is a position in the list. */
    if (st->selRow >= 0) st->lastRow = st->selRow;
    st->selRow = -1;
    for (i = 0; st->selIssue >= 0 && i < st->count; i++)
        if (st->sel[i] == st->selIssue) { st->selRow = i; break; }
    if (st->selRow < 0 && !st->keepSel) {
        const int row = !st->stayAt ? 0 : st->lastRow < st->count ? st->lastRow : st->count - 1;
        st->selRow   = st->count ? row : -1;
        st->selIssue = st->count ? (int)st->sel[row] : -1;
    }
    if (st->selIssue != was) issue_changed(st);
    st->dirty = st->keepSel = st->stayAt = false;
    st->reveal = true;
}

/* A click that changes the filter writes the query back into the field. Typing
   goes the other way. */
static void filter_changed(AppState *st) {
    compose_query(&st->f, st->query, QCAP);
    st->dirty = true;
}

static void clear_filters(AppState *st) {
    filter_reset(&st->f);
    filter_changed(st);
}

/* ---- the operations --------------------------------------------------------- */

/* The log is a ring: past ACTS entries the oldest event leaves its timeline,
   and the issue itself keeps what the event did. The count only says where the
   ring starts, so it is held under two laps: a bulk edit logs a row per picked
   issue, and an int counting every one would overflow into a negative index. */
static void log_act(AppState *st, int issue, int kind, int arg, int text, double now) {
    Activity *a = &st->act[st->nact++ % ACTS];

    if (st->nact >= 2 * ACTS) st->nact -= ACTS;
    a->issue = issue;
    a->kind  = kind;
    a->arg   = arg;
    a->text  = text;
    a->at    = now;
}

static void set_open(AppState *st, int i, bool open, double now) {
    Issue *it = &st->issue[i];

    if ((it->open != 0) == open) return;
    it->open  = (uint8_t)open;
    it->flags = (uint8_t)(open ? it->flags & ~F_CLOSED_NOW : it->flags | F_CLOSED_NOW);
    st->dirty = true;
    log_act(st, i, open ? ACT_REOPENED : ACT_CLOSED, 0, 0, now);
}

static void set_label(AppState *st, int i, int label, bool on, double now) {
    Issue *it = &st->issue[i];

    if ((it->labels >> label & 1u) == (unsigned)on) return;
    it->labels ^= 1u << label;
    st->dirty   = true;
    log_act(st, i, on ? ACT_LABELED : ACT_UNLABELED, label, 0, now);
}

static void set_assignee(AppState *st, int i, int who, double now) {
    Issue    *it  = &st->issue[i];
    const int was = it->assignee;

    if (was == who) return;
    it->assignee = (int8_t)who;
    st->dirty    = true;
    log_act(st, i, who == NONE ? ACT_UNASSIGNED : ACT_ASSIGNED, who == NONE ? was : who, 0, now);
}

static void set_milestone(AppState *st, int i, int ms, double now) {
    Issue    *it  = &st->issue[i];
    const int was = it->milestone;

    if (was == ms) return;
    it->milestone = (int8_t)ms;
    st->dirty     = true;
    log_act(st, i, ms == NONE ? ACT_UNMILESTONED : ACT_MILESTONED, ms == NONE ? was : ms, 0, now);
}

/* The comment's words go in a store that wraps, as the log does: a store that
   only filled would end commenting for the session. A comment whose words are
   written over leaves its timeline like the log's oldest, and the issue keeps
   its count. */
static bool can_comment(const AppState *st) {
    return !blank(st->reply);
}

static void add_comment(AppState *st, int i, double now) {
    Issue *it = &st->issue[i];
    int    n, k;

    if (!can_comment(st)) return;
    if (st->talkUsed + BODY_CAP > TALK_CAP) st->talkUsed = 0;
    n = cat(st->talk + st->talkUsed, BODY_CAP, 0, st->reply);
    for (k = 0; k < st->nact && k < ACTS; k++)
        if (st->act[k].kind == ACT_COMMENT && st->act[k].text >= st->talkUsed &&
            st->act[k].text <= st->talkUsed + n)
            st->act[k].issue = NONE;
    log_act(st, i, ACT_COMMENT, 0, st->talkUsed, now);
    st->talkUsed += n + 1;
    if (it->mine < 255) it->mine++;
    it->flags |= F_SUBSCRIBED;
    st->reply[0] = '\0';
    st->preview  = false;
    st->dirty    = true;
    st->keepSel  = true;
}

/* FILING AN ISSUE is the one insert, and it costs what a keystroke costs: the
   record goes in front of the live range, its words in filed[], and the ordinary
   rebuild does the rest. */
static void file_issue(AppState *st) {
    Issue *it;

    if (st->first == 0 || blank(st->draftTitle)) return;
    st->first--;
    it = &st->issue[st->first];
    it->num   = (uint32_t)st->nextNum++;
    it->age   = 0;
    it->comments = 0;
    it->mine  = 0;
    it->who   = ME;
    it->open  = 1;
    it->flags = F_SUBSCRIBED;
    it->kind  = (uint8_t)st->draftKind;
    it->labels = 1u << st->draftKind;
    if (st->draftPrio > 0) it->labels |= 1u << (L_P0 + st->draftPrio - 1);
    it->assignee  = (int8_t)(st->draftWho - 1);
    it->milestone = (int8_t)(st->draftMilestone - 1);
    it->labels0    = it->labels;
    it->assignee0  = it->assignee;
    it->milestone0 = it->milestone;
    cat(st->filed[st->first].title, TITLE_CAP, 0, st->draftTitle);
    cat(st->filed[st->first].body, BODY_CAP, 0,
        blank(st->draftBody) ? "No description provided." : st->draftBody);

    st->draftTitle[0] = st->draftBody[0] = '\0';
    st->draftPreview  = false;
    st->selIssue   = st->first;
    st->detailOpen = true;
    st->page       = PAGE_ISSUES;
    if (st->createMore) st->composeFresh = true;
    else                st->composing = false;
    clear_filters(st);
}

/* The panes. Compared against rcViewport(), the space the layout actually has,
   so a desktop window dragged narrow gets the compact arm for the same reason a
   phone does. */
static float usable_width(void) {
    RC_Viewport v = rcViewport();

    return v.width - v.safe.left - v.safe.right;
}

static bool is_compact(void) { return usable_width() < (float)COMPACT_W; }

/* The bottom safe band, where a page spends it itself: compact, a page runs to the
   bottom edge, under a phone's home indicator, and pads the END of its content, as
   a web page does. Spent on the root, it would stop short above an empty strip. */
static float page_foot(void) { return is_compact() ? rcViewport().safe.bottom : 0.0f; }

static bool two_panes(const AppState *st) {
    return st->split && st->page == PAGE_ISSUES && usable_width() >= (float)SPLIT_MIN;
}

/* The list's rows, from the same numbers layout() places the pane with, so they
   are right on the frame the pane moves rather than one frame later. */
static float rows_width(const AppState *st) {
    const float w = usable_width();

    if (is_compact())    return w;
    return (two_panes(st) ? w * st->splitAt : w) - 2.0f * 16.0f;
}

/* THE ROW HAS THREE SHAPES AND THE LIST'S OWN WIDTH PICKS ONE. The same things
   are in the row in all three: a wrapped title in a fixed-pitch virtual row would
   draw over the row beneath, so narrower rows move the labels instead. */
enum { ROW_WIDE, ROW_MID, ROW_NARROW };

static int row_shape(const AppState *st) {
    const float w = rows_width(st);

    return w >= (float)ROW_WIDE_W ? ROW_WIDE : w >= (float)ROW_MID_W ? ROW_MID : ROW_NARROW;
}

/* One reader, so the pane that draws the rows and the scroll that follows the
   keyboard cannot disagree about the pitch. */
static int row_pitch(const AppState *st) {
    return row_shape(st) == ROW_NARROW ? ROW_H_TALL : ROW_H;
}

/* Keep the keyboard selection on screen. In a virtual list that is not cosmetic:
   only the visible rows are declared at all. Minimum movement, so a row already
   visible never lurches the list. It needs the list's last layout, so it keeps
   asking until the list exists. */
static void reveal_selected(AppState *st) {
    RC_ScrollInfo si   = rcGetScrollInfo("list");
    RC_Box        view = rcGetElementBox("list");
    const float   pitch = (float)row_pitch(st);
    float         top, bottom, foot;

    if (!si.found || view.height <= 0.0f) return;
    st->reveal = false;
    if (st->selRow < 0) return;
    foot   = page_foot();
    top    = (float)st->selRow * pitch;
    bottom = top + pitch;
    if (top < si.offsetY)
        rcScrollBy("list", 0.0f, top - si.offsetY);
    else if (bottom > si.offsetY + view.height - foot)
        rcScrollBy("list", 0.0f, bottom - (si.offsetY + view.height - foot));
}

/* The list's keys take the keyboard back from a button or a menu a click left
   focused, so the Enter after them opens the issue instead of clicking that control. */
static void move(AppState *st, int step) {
    int n;

    rcSetFocus(NULL);
    if (!st->count) return;
    n = st->selRow >= 0 ? st->selRow + step : st->lastRow + (step > 0 ? step - 1 : step);
    st->selRow   = n < 0 ? 0 : n > st->count - 1 ? st->count - 1 : n;
    st->selIssue = (int)st->sel[st->selRow];
    st->cursorShown = st->reveal = true;
    issue_changed(st);
}

static void open_row(AppState *st, int row) {
    st->selRow     = row;
    st->selIssue   = (int)st->sel[row];
    st->detailOpen = st->cursorShown = true;
    issue_changed(st);
}

/* A #123 in a comment: the page goes there, in the list or not. */
static void open_number(AppState *st, unsigned num) {
    int i, k;

    if (num > 1000u && num <= (unsigned)ISSUES + 1000u)
        i = SPARE + ISSUES + 1000 - (int)num;
    else
        for (i = SPARE - 1; i >= st->first && st->issue[i].num != num; i--) ;
    if (i < st->first || i >= SPARE + ISSUES) return;
    st->selIssue   = i;
    st->selRow     = -1;
    st->detailOpen = true;
    for (k = 0; k < st->count; k++) if (st->sel[k] == i) { st->selRow = k; break; }
    st->reveal = true;
    issue_changed(st);
}

static void toggle_pick(AppState *st, int row) {
    Issue *it = &st->issue[st->sel[row]];

    it->flags ^= F_PICKED;
    st->picked += it->flags & F_PICKED ? 1 : -1;
}

static void pick_all(AppState *st, bool on) {
    int i;

    for (i = 0; i < st->count; i++) {
        Issue *it = &st->issue[st->sel[i]];
        if (on) it->flags |= F_PICKED;
        else    it->flags &= (uint8_t)~F_PICKED;
    }
    st->picked = on ? st->count : 0;
}

/* The labels any picked row has, for the bulk menu's check marks. It walks the
   list, and only while that menu is open. */
static uint32_t picked_labels(const AppState *st) {
    uint32_t any = 0u;
    int      k;

    for (k = 0; k < st->count; k++) {
        const Issue *it = &st->issue[st->sel[k]];

        if (it->flags & F_PICKED) any |= it->labels;
    }
    return any;
}

/* A bulk click means one thing on every picked row: the label goes on wherever it
   is missing, and comes off everywhere only when no picked row lacks it. */
static void label_picked(AppState *st, int label, double now) {
    bool lacking = false;
    int  k;

    for (k = 0; k < st->count && !lacking; k++) {
        const Issue *it = &st->issue[st->sel[k]];

        lacking = (it->flags & F_PICKED) && !(it->labels >> label & 1u);
    }
    for (k = 0; k < st->count; k++)
        if (st->issue[st->sel[k]].flags & F_PICKED) set_label(st, st->sel[k], label, lacking, now);
}

/* Holding j or k flies. The first press moves one row at once; after a beat the
   rate climbs with the square of the time held, so a short hold still reads row
   by row and a long one crosses thousands. rcKeyDown asks for the next frame by
   itself, so this runs under RC_RENDER_ON_DEMAND with no timer. */
static void fly(RC_App *app, AppState *st, bool keysFree) {
    const bool   down = keysFree && (rcKeyDown(RC_KEY_J) || rcKeyDown(RC_KEY_DOWN));
    const bool   up   = keysFree && (rcKeyDown(RC_KEY_K) || rcKeyDown(RC_KEY_UP));
    const int    dir  = down ? 1 : up ? -1 : 0;
    const double now  = rcAppTime(app);
    double       held;
    int          rows;

    if (dir != st->flyDir) {
        st->flyDir   = dir;
        st->flyStart = now;
        st->flyOwed  = 0.0;
        st->flying   = false;
        if (dir) move(st, dir);
    } else if (dir) {
        held = now - st->flyStart - 0.28;
        if (held > 0.0) {
            const double rate = 18.0 + 90.0 * held * held;
            st->flying   = true;
            st->flyOwed += (rate < 1800.0 ? rate : 1800.0) * (now - st->flyLast);
            rows = (int)st->flyOwed;
            st->flyOwed -= rows;
            if (rows) move(st, dir * rows);
        }
    }
    st->flyLast = now;
}

/* ---- small pieces used by every page ----------------------------------------- */

/* A control's height: CTRL_H, or a finger's row under a coarse pointer. */
static float control_h(void) {
    return rcPointerIsCoarse() ? (float)HIT_MIN : (float)CTRL_H;
}

/* The semibold face at a size, or the default face where it is not asked for. */
static uint16_t weight(bool semibold, float size) {
    return semibold ? rcFontSemibold(size) : 0;
}

static void hrule(void) {
    rcBox(.bg = ui.border, .w = "grow", .hType = RC_PX(1)) {}
}

/* "3 days ago", or "just now" inside the first hour. */
static RC_String ago(RC_Arena *m, unsigned hours) {
    unsigned    v;
    const char *word;

    if (hours == 0u) return rcStringFromCStr("just now");
    if (hours < 24u)        { v = hours;         word = "hour";  }
    else if (hours < 730u)  { v = hours / 24u;   word = "day";   }
    else if (hours < 8760u) { v = hours / 730u;  word = "month"; }
    else                    { v = hours / 8760u; word = "year";  }
    return rcFormat(m, "%u %s%s ago", v, word, v == 1u ? "" : "s");
}

/* A label: GitHub's filled pill on light. On dark a solid pale fill would glare,
   so it tints instead, the fill at low alpha and the name lightened to read. */
static void label_chip(int l) {
    const RC_Color c    = LABEL[l].color;
    const float    luma = (c.r * 299.0f + c.g * 587.0f + c.b * 114.0f) / 1000.0f;
    const RC_Color tint = rcRgb((int)(c.r + (255.0f - c.r) * 0.45f),
                                (int)(c.g + (255.0f - c.g) * 0.45f),
                                (int)(c.b + (255.0f - c.b) * 0.45f));
    const RC_Color ink  = ui.dark ? tint : luma > 140.0f ? rcRgb(31, 35, 40) : RC_WHITE;

    rcBox(.bg = ui.dark ? rcWithAlpha(c, 0.18f) : c, .px = 8, .align = "cc",
          .borderRadius = "all-full", .hType = RC_PX(20)) {
        rcTextC(LABEL[l].name, .font = rcFontSemibold(12.0f), .color = ink, .wrap = "n",
                .select = RC_SELECT_NONE);
    }
}

/* At most `most` chips, then how many more: a fixed-pitch row has no second line
   to give them. */
static void label_chips(RC_Arena *m, uint32_t labels, int most) {
    int l, shown = 0, more = 0;

    for (l = 0; l < LABELS; l++) {
        if (!(labels >> l & 1u)) continue;
        if (shown < most) { label_chip(l); shown++; }
        else              more++;
    }
    if (more)
        rcText(rcFormat(m, "+%d", more), .color = ui.muted, .size = 12, .wrap = "n",
               .select = RC_SELECT_NONE);
}

/* A person with no photo: their initials on a disc of their own color, the same
   everywhere in the app, and at this size one letter is what fits. */
static void avatar(int who, float size) {
    static const RC_Color INK[PEOPLE] = {
        {  47, 129,  87, 255 }, { 176,  72, 146, 255 }, {  52, 108, 190, 255 },
        { 196, 103,  32, 255 }, { 123,  84, 192, 255 }, {  28, 130, 145, 255 },
        { 186,  66,  66, 255 }, {  98, 124,  36, 255 }, { 139,  94,  60, 255 },
        {  76,  80, 176, 255 }, { 194,  46,  98, 255 }, { 109,  40, 217, 255 },
        {  88, 104, 128, 255 }, { 134, 100,  12, 255 }, { 128,  58, 112, 255 } };
    const RC_String letters = { .length = size < 24.0f ? 1 : 2, .chars = INITIALS[who] };

    rcBox(.bg = INK[who], .align = "cc", .borderRadius = "all-full",
          .wType = RC_PX(size), .hType = RC_PX(size)) {
        rcText(letters, .font = rcFontSemibold(size < 24.0f ? 11.0f : size < 36.0f ? 12.0f : 14.0f),
               .color = RC_WHITE, .wrap = "n", .select = RC_SELECT_NONE);
    }
}

/* A neutral outlined pill, for "Public" and "Author". */
static void outline_badge(const char *text) {
    rcBox(.px = 8, .align = "cc", .borderRadius = "all-full",
          .border = { .color = ui.border, .width = 1 }, .hType = RC_PX(20)) {
        rcTextC(text, .color = ui.muted, .size = 12, .wrap = "n", .select = RC_SELECT_NONE);
    }
}

/* A square icon target with a tooltip, for the header's and the page's tools. */
static bool icon_button(const char *id, RC_IconCallback icon, const char *tip, bool on, bool off) {
    const float side = control_h();

    rcBox(.id = id, .align = "cc", .borderRadius = "all-md", .disabled = off,
          .wType = RC_PX(side), .hType = RC_PX(side), .tooltip = tip,
          .className = on ? "toggled" : "wash") {
        icon(16.0f, on ? ui.fg : ui.muted);
    }
    return rcClicked(id);
}

/* A control's class for the pointer in use. An h-* is exact on every pointer, so
   a button, a combo or a field asks for its finger height (h-11, HIT_MIN) as it
   asks for its mouse one. */
static const char *by_pointer(const char *mouse, const char *finger) {
    return rcPointerIsCoarse() ? finger : mouse;
}

/* A field's or a button's class: a control's height (CTRL_H), or a finger's row
   under a coarse pointer, as control_h() is. */
static const char *control_class(void) {
    return by_pointer("h-8", "h-11");
}

/* A link in the page's flow. It has no chrome and so no padding, which puts its
   text on the same left edge as the text above and below it. */
static bool text_link(const char *id, RC_IconCallback icon, const char *text) {
    rcRow(.id = id, .gap = 8, .align = "cl", .hType = RC_PX(control_h())) {
        if (icon) icon(16.0f, ui.accent);
        rcTextC(text, .color = rcShowsHover(id) ? ui.fg : ui.accent, .wrap = "n",
                .select = RC_SELECT_NONE);
    }
    return rcClicked(id);
}

/* The state glyphs in their own colors, for .leadingIcon, whose callback paints
   in the label's color otherwise. */
static void icon_close(float size, RC_Color c) { (void)c; rcIconCircleCheck(size, ui.closed); }
static void icon_reopen(float size, RC_Color c) { (void)c; rcIconCircleDot(size, ui.open); }
static void icon_starred(float size, RC_Color c) { (void)c; rcIconStarFilled(size, ui.star); }
static void icon_me(float size, RC_Color c) { (void)c; (void)size; avatar(ME, 20.0f); }

/* A key cap. */
static void kbd(const char *key) {
    rcBox(.bg = ui.subtle, .px = 6, .align = "cc", .borderRadius = "all-sm",
          .border = { .color = ui.border, .width = 1 }, .hType = RC_PX(22), .wMin = RC_VAL(22)) {
        rcTextC(key, .color = ui.fg, .size = 12, .wrap = "n", .select = RC_SELECT_NONE);
    }
}

/* ---- the repository header ---------------------------------------------------
   RC_WINDOW_FRAME_NONE means the runner draws nothing and this band IS the titlebar:
   RC_ID_WINDOW_DRAG moves the window and an element with .noDrag opts back out.
   Chrome rather than content: RC_TitlebarOptions.height freezes the OS drag
   strip in unzoomed px, so rcUnzoomed() keeps the band on it at any zoom, and what
   the band holds goes by its own width: the layout's, times the zoom. */

static void go_page(AppState *st, int page) {
    st->page = page;
    st->detailOpen = false;
}

/* With the pane gone the list has the whole window, not the page you were on. */
static void toggle_split(AppState *st) {
    st->split = !st->split;
    st->detailOpen = false;
}

static void user_menu(AppState *st) {
    if (rcBeginMenu("m_user", "", .caret = true, .leadingIcon = icon_me)) {
        if (rcMenuItem("Issues you opened")) {
            clear_filters(st);
            st->f.author = ME;
            filter_changed(st);
            go_page(st, PAGE_ISSUES);
        }
        if (rcMenuItem("Assigned to you")) {
            clear_filters(st);
            st->f.assignee = ME;
            filter_changed(st);
            go_page(st, PAGE_ISSUES);
        }
        if (rcMenuItem(st->watching ? "Stop watching this repository" : "Watch this repository"))
            st->watching = !st->watching;
        if (rcMenuItem("Keyboard shortcuts")) st->helpOpen = true;
        if (rcMenuItem(ui.dark ? "Switch to light theme" : "Switch to dark theme"))
            st->flipTheme = true;
        rcEndMenu();
    }
}

static void tab(RC_Arena *m, AppState *st, int page, bool compact) {
    static const char *const ID[PAGES]     = { "t_issues", "t_miles", "t_labels", "t_pulse" };
    static const char *const NAME[PAGES]   = { "Issues", "Milestones", "Labels", "Pulse" };
    static const RC_IconCallback ICON[PAGES] = { rcIconCircleDot, rcIconFlag, rcIconTag,
                                                 rcIconChartColumn };
    const bool  on   = st->page == page;
    const bool  over = rcShowsHover(ID[page]);
    const char *counter = NULL;
    int         open = 0, i;

    if (page == PAGE_ISSUES && st->seeded)
        counter = st->n.openAll >= 1000 ? rcFormat(m, "%dk", (st->n.openAll + 500) / 1000).chars
                                        : rcFormatInt(m, st->n.openAll, NULL).chars;
    if (page == PAGE_LABELS) counter = rcFormat(m, "%d", LABELS).chars;
    if (page == PAGE_MILESTONES && st->seeded) {
        for (i = 0; i < MILESTONES; i++) open += st->n.msOpen[i] > 0;
        counter = rcFormat(m, "%d", open).chars;
    }
    /* At 320 px the four tabs fit by 6 px sides, so the strip has nothing to slide. */
    rcColumn(.id = ID[page], .align = "bc", .h = "grow") {
        rcBox(.align = "cc", .h = "grow") {
            /* The pill lights while the pointer is anywhere on the band-tall tab: the look
               of one box from another's state, which a class does not reach. */
            rcRow(.bg = over ? ui.hover : RC_UNSET_COLOR, .gap = 8,
                  .px = (float)(rcViewport().width < 360.0f ? 6 : 8), .align = "cc",
                  .borderRadius = "all-md", .hType = RC_PX(32)) {
                if (!compact) ICON[page](16.0f, ui.muted);
                rcTextC(NAME[page], .font = weight(on, 14.0f), .color = ui.fg,
                        .wrap = "n", .select = RC_SELECT_NONE);
                /* A narrow bar keeps the one count a reader looks for. */
                if (counter && (page == PAGE_ISSUES || rcViewport().width >= 400.0f))
                    rcBox(.bg = ui.navOn, .px = 6, .align = "cc", .borderRadius = "all-full",
                          .hType = RC_PX(20)) {
                        rcTextC(counter, .color = ui.fg, .size = 12, .wrap = "n",
                                .select = RC_SELECT_NONE);
                    }
            }
        }
        rcBox(.bg = on ? ui.tab : RC_UNSET_COLOR, .borderRadius = "all-full", .w = "grow",
              .hType = RC_PX(2)) {}
    }
    if (rcClicked(ID[page])) go_page(st, page);
}

/* The tabs are one row in the band on a short, wide window, and their own under it. */
static void tabs(RC_Arena *m, AppState *st, bool compact) {
    int i;

    for (i = 0; i < PAGES; i++) tab(m, st, i, compact);
}

static void titlebar(RC_Arena *m, AppState *st, bool tabsInBand, float bandW) {
    /* Where the band is short of room it keeps the repository's name and the star,
       and the user menu carries the rest. */
    const bool slim = bandW < (float)COMPACT_W || tabsInBand;
    /* On a phone the band paints under the status bar and a side cutout and pads its
       content in, in window px: the unzoomed band's unit. Where no window controls
       hold the right corner, the user menu keeps the margin the mark keeps on the left. */
    const RC_Insets safe   = rcGetSafeAreaInsets();
    const float     endPad = rcWindowControlsWidth() > 0.0f ? 0.0f : 16.0f;

    rcUnzoomed() {
        rcRow(.id = RC_ID_WINDOW_DRAG, .bg = ui.subtle, .gap = 12, .pt = safe.top,
              .pl = 16.0f + safe.left, .pr = endPad + safe.right, .align = "cl",
              .w = "grow", .hType = RC_PX(BAR_H + safe.top)) {
            /* The narrowest window gives the mark's width to its close button. */
            if (bandW >= 360.0f) rcIconRayClayLogo(24.0f);
            rcRow(.gap = 4, .align = "cl") {
                if (!slim) {
                    rcTextL("tessera", .color = ui.muted, .size = 16, .wrap = "n");
                    rcTextL("/", .color = ui.muted, .size = 16, .wrap = "n");
                }
                rcTextL("editor", .font = rcFontSemibold(16.0f), .color = ui.fg, .wrap = "n");
            }
            if (!slim) outline_badge("Public");
            if (tabsInBand)
                rcRow(.gap = 4, .align = "cl", .noDrag = true, .h = "grow") {
                    tabs(m, st, true);
                }
            rcSpacer() {}
            rcRow(.gap = 8, .align = "cl", .noDrag = true) {
                /* The counts are ours to move: starring here is a real toggle. */
                if (slim) {
                    if (icon_button("star", st->starred ? icon_starred : rcIconStar,
                                    "Star this repository", false, false))
                        st->starred = !st->starred;
                } else {
                    if (rcButton("watch",
                                 rcFormat(m, "%s  %s", st->watching ? "Watching" : "Watch",
                                          rcFormatInt(m, 128 + st->watching, NULL).chars).chars,
                                 .className = control_class(), .leadingIcon = rcIconBell))
                        st->watching = !st->watching;
                    if (rcButton("star",
                                 rcFormat(m, "%s  %s", st->starred ? "Starred" : "Star",
                                          rcFormatInt(m, 3214 + st->starred, NULL).chars).chars,
                                 .className = control_class(),
                                 .leadingIcon = st->starred ? icon_starred : rcIconStar))
                        st->starred = !st->starred;
                    if (icon_button("theme", ui.dark ? rcIconSun : rcIconMoon,
                                    ui.dark ? "Switch to the light theme"
                                            : "Switch to the dark theme", false, false))
                        st->flipTheme = true;
                }
                user_menu(st);
            }
            /* Band-tall and flush to the corner, so the OS rounds the close slab
               with the window. It emits nothing on the web or on a phone. */
            rcWindowControls();
        }
    }
}

/* ---- Markdown: the subset an issue thread uses -------------------------------
   Headings, fenced code, lists, task lists, quotes, **bold**, `code`, @mentions
   and #123 links. Like GitHub's comments, a newline is a line break.
   Lines of plain text are one wrapped run. A line with inline styling is a
   flex-wrap row of word runs: a wrapping row has one gap for both axes, so the
   gap is the word space, and the line height plus the gap is the line pitch. */

enum { MD_LINE = 22, MD_WORD_GAP = 4 };

static bool md_styled(const char *s, int n) {
    int i;

    for (i = 0; i < n; i++)
        if (s[i] == '`' || s[i] == '@' || s[i] == '#' || (s[i] == '*' && s[i + 1] == '*'))
            return true;
    return false;
}

static RC_String slice(const char *s, int n) {
    RC_String out = { .length = n, .chars = s };
    return out;
}

/* One run of a word, in the style the scan is in. */
static void md_run(RC_Arena *m, AppState *st, const char *s, int n, bool bold, RC_Color ink) {
    if (n > 0 && s[0] == '@') {
        rcText(slice(s, n), .font = rcFontSemibold(14.0f), .color = ui.fg,
               .lineHeight = MD_LINE - MD_WORD_GAP, .wrap = "n");
    } else if (n > 1 && s[0] == '#' && s[1] >= '0' && s[1] <= '9') {
        const char *id = rcFormat(m, "md%d", st->mdLinks++).chars;
        unsigned    num = 0;
        int         d;

        /* Held past nine digits, where it is no issue's: wrapped, it would open one. */
        for (d = 1; d < n && s[d] >= '0' && s[d] <= '9'; d++)
            num = num > 99999999u ? ~0u : num * 10u + (unsigned)(s[d] - '0');
        rcBox(.id = id) {
            rcText(slice(s, d), .color = ui.accent, .lineHeight = MD_LINE - MD_WORD_GAP,
                   .wrap = "n", .select = RC_SELECT_NONE);
        }
        if (rcClicked(id)) open_number(st, num);
        if (d < n)
            rcText(slice(s + d, n - d), .color = ink, .lineHeight = MD_LINE - MD_WORD_GAP,
                   .wrap = "n");
    } else if (n > 0) {
        rcText(slice(s, n), .font = weight(bold, 14.0f), .color = ink,
               .lineHeight = MD_LINE - MD_WORD_GAP, .wrap = "n");
    }
}

/* One line's inline text. **bold** spans words, so its state lives in the scan;
   a `code` span stays whole. */
static void md_inline(RC_Arena *m, AppState *st, const char *s, int n, RC_Color ink) {
    bool bold = false;
    int  at = 0;

    if (!md_styled(s, n)) {
        rcText(slice(s, n), .color = ink, .lineHeight = MD_LINE);
        return;
    }
    rcRow(.gap = MD_WORD_GAP, .align = "cl", .w = "grow", .hMin = RC_VAL(MD_LINE),
          .className = "flex-wrap") {
        while (at < n) {
            bool code = false;
            int  end = at;

            while (end < n && (code || s[end] != ' ')) {
                if (s[end] == '`') code = !code;
                end++;
            }
            rcRow(.align = "cl") {
                while (at < end) {
                    int run = at;

                    if (s[at] == '*' && at + 1 < end && s[at + 1] == '*') {
                        bold = !bold;
                        at  += 2;
                        continue;
                    }
                    if (s[at] == '`') {
                        for (run = at + 1; run < end && s[run] != '`'; run++) ;
                        rcBox(.bg = ui.navOn, .px = 4, .align = "cc", .borderRadius = "all-sm") {
                            rcText(slice(s + at + 1, run - at - 1), .color = ink, .size = 13,
                                   .lineHeight = MD_LINE - MD_WORD_GAP, .wrap = "n");
                        }
                        at = run < end ? run + 1 : end;
                        continue;
                    }
                    while (run < end && s[run] != '`' &&
                           !(s[run] == '*' && run + 1 < end && s[run + 1] == '*'))
                        run++;
                    md_run(m, st, s + at, run - at, bold, ink);
                    at = run;
                }
            }
            while (at < n && s[at] == ' ') at++;
        }
    }
}

static int line_length(const char *s) {
    int n = 0;

    while (s[n] && s[n] != '\n') n++;
    return n;
}

static bool starts_with(const char *s, int n, const char *prefix) {
    int i;

    for (i = 0; prefix[i]; i++) if (i >= n || s[i] != prefix[i]) return false;
    return true;
}

static void markdown(RC_Arena *m, AppState *st, const char *s) {
    rcColumn(.gap = 6, .w = "grow") {
        int at = 0;

        while (s[at]) {
            const char *l = s + at;
            const int   n = line_length(l);

            at += n + (l[n] ? 1 : 0);
            if (n == 0) {
                /* A blank line opens the gap a paragraph break reads as. */
                rcBox(.hType = RC_PX(4)) {}
                continue;
            }
            if (starts_with(l, n, "```")) {
                const int start = at;
                int       end   = at;

                while (s[at] && !starts_with(s + at, line_length(s + at), "```")) {
                    at += line_length(s + at) + (s[at + line_length(s + at)] ? 1 : 0);
                    end = at;
                }
                if (s[at]) at += line_length(s + at) + (s[at + line_length(s + at)] ? 1 : 0);
                rcBox(.bg = ui.subtle, .p = 12, .overflow = "hidden", .borderRadius = "all-md",
                      .w = "grow") {
                    rcText(slice(s + start, end - start - (end > start ? 1 : 0)), .color = ui.fg,
                           .size = 13, .lineHeight = 20, .wrap = "l");
                }
            } else if (starts_with(l, n, "### ") || starts_with(l, n, "## ")) {
                const int skip = l[2] == '#' ? 4 : 3;
                rcText(slice(l + skip, n - skip), .font = rcFontSemibold(16.0f), .color = ui.fg);
            } else if (starts_with(l, n, "# ")) {
                rcText(slice(l + 2, n - 2), .font = rcFontSemibold(20.0f), .color = ui.fg);
            } else if (starts_with(l, n, "- [ ] ") || starts_with(l, n, "- [x] ")) {
                const bool done = l[3] == 'x';
                rcRow(.gap = 8, .w = "grow") {
                    rcBox(.pt = 4) {
                        rcBox(.bg = done ? ui.accent : ui.canvas, .align = "cc",
                              .borderRadius = "all-sm",
                              .border = { .color = done ? ui.accent : ui.muted, .width = 1 },
                              .w = "14px", .h = "14px") {
                            if (done) rcIconCheck(12.0f, RC_WHITE);
                        }
                    }
                    rcBox(.w = "grow") { md_inline(m, st, l + 6, n - 6, ui.fg); }
                }
            } else if (starts_with(l, n, "- ")) {
                rcRow(.gap = 8, .w = "grow") {
                    rcBox(.pt = 8, .pl = 6) {
                        rcBox(.bg = ui.fg, .borderRadius = "all-full", .w = "5px", .h = "5px") {}
                    }
                    rcBox(.w = "grow") { md_inline(m, st, l + 2, n - 2, ui.fg); }
                }
            } else if (starts_with(l, n, "> ")) {
                rcRow(.gap = 12, .w = "grow") {
                    rcBox(.bg = ui.border, .borderRadius = "all-full", .h = "grow",
                          .wType = RC_PX(3)) {}
                    rcBox(.w = "grow") { md_inline(m, st, l + 2, n - 2, ui.muted); }
                }
            } else {
                md_inline(m, st, l, n, ui.fg);
            }
        }
    }
}

/* ---- the issue page ----------------------------------------------------------- */

/* The vertical line that joins a thread into one timeline. Everything hangs off
   the same x: a comment box's left edge plus 16, the gutter being the avatar's
   column. */
static void connector(float gutter) {
    rcRow(.w = "grow", .hType = RC_PX(16)) {
        rcBox(.wType = RC_PX(gutter + 20.0f)) {}
        rcBox(.bg = ui.border, .h = "grow", .wType = RC_PX(2)) {}
    }
}

/* A comment box's gray header. The avatar out in the gutter centers on it, and
   the box's 1 px border stands above it, so the avatar's cell is HEAD_H + 2. */
enum { HEAD_H = 40 };

/* One comment: who and when in a gray header, then what they said, the avatar
   out in the gutter as GitHub draws it, or in the header where there is no
   gutter. */
static void comment_box(RC_Arena *m, AppState *st, int who, RC_String when, const char *body,
                        bool byAuthor, float gutter) {
    rcRow(.gap = 16, .w = "grow") {
        if (gutter > 0.0f)
            rcBox(.align = "cc", .hType = RC_PX(HEAD_H + 2)) { avatar(who, 32.0f); }
        rcColumn(.borderRadius = "all-md", .border = { .color = ui.border, .width = 1 },
                 .w = "grow") {
            rcRow(.bg = ui.subtle, .gap = 8, .px = 16, .align = "cl", .borderRadius = "t-md",
                  .w = "grow", .hType = RC_PX(HEAD_H)) {
                if (gutter <= 0.0f) avatar(who, 20.0f);
                rcTextC(HANDLE[who], .font = rcFontSemibold(14.0f), .color = ui.fg, .wrap = "n");
                rcText(rcFormat(m, "commented %s", when.chars), .color = ui.muted, .wrap = "n");
                rcSpacer() {}
                if (byAuthor) outline_badge("Author");
            }
            hrule();
            rcBox(.p = 16, .w = "grow") { markdown(m, st, body); }
        }
    }
}

/* One event on the line: its glyph on a disc that interrupts the line, then who
   did what, with any labels in the sentence. The row wraps on a narrow page. */
static void event_row(RC_Arena *m, float gutter, RC_IconCallback icon, RC_Color ink, int who,
                      const char *what, uint32_t labels, RC_String when) {
    const float left = gutter + 5.0f;

    rcRow(.gap = 8, .pl = left, .align = "cl", .w = "grow") {
        rcBox(.bg = ui.subtle, .align = "cc", .borderRadius = "all-full",
              .border = { .color = ui.border, .width = 1 }, .w = "32px", .h = "32px") {
            icon(16.0f, ink);
        }
        rcRow(.gap = 4, .align = "cl", .w = "grow", .className = "flex-wrap") {
            avatar(who, 20.0f);
            rcTextC(HANDLE[who], .font = rcFontSemibold(14.0f), .color = ui.fg, .wrap = "n");
            rcTextC(what, .color = ui.muted);
            label_chips(m, labels, LABELS);
            rcText(when, .color = ui.muted, .wrap = "n");
        }
    }
}

/* How long ago something this session did was: it updates on the next frame any
   input brings, which is as often as anything else on this page changes. */
static RC_String since(RC_Arena *m, double at, double now) {
    const int minutes = (int)((now - at) / 60.0);

    if (minutes < 1) return rcStringFromCStr("just now");
    return rcFormat(m, "%d minute%s ago", minutes, minutes == 1 ? "" : "s");
}

/* What this session did to the issue, oldest first, after its own history. */
static void session_events(RC_Arena *m, AppState *st, int i, float gutter, double now) {
    const Issue *it = &st->issue[i];
    const int    from = st->nact > ACTS ? st->nact - ACTS : 0;
    int          k;

    for (k = from; k < st->nact; k++) {
        const Activity *a = &st->act[k % ACTS];
        RC_String       when;

        if (a->issue != i) continue;
        when = since(m, a->at, now);
        connector(gutter);
        switch (a->kind) {
        case ACT_COMMENT:
            comment_box(m, st, ME, when, st->talk + a->text, it->who == ME, gutter);
            break;
        case ACT_CLOSED:
            event_row(m, gutter, rcIconCircleCheck, ui.closed, ME, "closed this", 0, when);
            break;
        case ACT_REOPENED:
            event_row(m, gutter, rcIconCircleDot, ui.open, ME, "reopened this", 0, when);
            break;
        case ACT_LABELED:
            event_row(m, gutter, rcIconTag, ui.muted, ME, "added", 1u << a->arg, when);
            break;
        case ACT_UNLABELED:
            event_row(m, gutter, rcIconTag, ui.muted, ME, "removed", 1u << a->arg, when);
            break;
        case ACT_ASSIGNED:
            event_row(m, gutter, rcIconUser, ui.muted, ME,
                      rcFormat(m, "assigned %s", HANDLE[a->arg]).chars, 0, when);
            break;
        case ACT_UNASSIGNED:
            event_row(m, gutter, rcIconUser, ui.muted, ME,
                      rcFormat(m, "unassigned %s", HANDLE[a->arg]).chars, 0, when);
            break;
        case ACT_MILESTONED:
            event_row(m, gutter, rcIconFlag, ui.muted, ME,
                      rcFormat(m, "added this to %s", MILESTONE[a->arg].name).chars, 0, when);
            break;
        default:
            event_row(m, gutter, rcIconFlag, ui.muted, ME,
                      rcFormat(m, "removed this from %s", MILESTONE[a->arg].name).chars, 0, when);
            break;
        }
    }
}

/* The box a comment is written in, Write and Preview over one buffer. */
static void composer(RC_Arena *m, AppState *st, int i, float gutter, double now) {
    const Issue *it   = &st->issue[i];
    const bool   open = it->open != 0;
    const bool   text = !blank(st->reply);
    /* Under a finger the tabs are a finger's row, and the header grows to hold them. */
    const float  headH = rcPointerIsCoarse() ? (float)(HIT_MIN + 8) : (float)HEAD_H;

    rcRow(.gap = 16, .w = "grow") {
        if (gutter > 0.0f)
            rcBox(.align = "cc", .hType = RC_PX(headH + 2.0f)) { avatar(ME, 32.0f); }
        rcColumn(.borderRadius = "all-md", .border = { .color = ui.border, .width = 1 },
                 .w = "grow") {
            rcRow(.bg = ui.subtle, .gap = 4, .px = 8, .align = "cl", .borderRadius = "t-md",
                  .w = "grow", .hType = RC_PX(headH)) {
                if (rcButton("w_write", "Write", .className = by_pointer("h-7", "h-11"),
                             .variant = st->preview ? RC_BTN_GHOST : RC_BTN_DEFAULT))
                    st->preview = false;
                if (rcButton("w_preview", "Preview", .className = by_pointer("h-7", "h-11"),
                             .variant = st->preview ? RC_BTN_DEFAULT : RC_BTN_GHOST))
                    st->preview = true;
            }
            hrule();
            rcColumn(.gap = 12, .p = 12, .w = "grow") {
                if (st->preview) {
                    rcColumn(.w = "grow", .hMin = RC_VAL(96)) {
                        if (text) markdown(m, st, st->reply);
                        else rcTextL("Nothing to preview.", .color = ui.muted);
                    }
                } else {
                    rcTextArea("reply", st->reply, BODY_CAP,
                               .placeholder = "Leave a comment. Markdown is supported.", .rows = 4);
                    /* After the field: see shortcuts(). */
                    if (st->focusReply) {
                        rcSetFocus("reply");
                        st->focusReply = false;
                    }
                }
                rcRow(.gap = 8, .align = "cr", .w = "grow", .className = "flex-wrap") {
                    if (rcButton("act", open ? (text ? "Close with comment" : "Close issue")
                                             : (text ? "Comment and reopen" : "Reopen issue"),
                                 .className = control_class(),
                                 .leadingIcon = open ? icon_close : icon_reopen)) {
                        if (text) add_comment(st, i, now);
                        set_open(st, i, !open, now);
                        st->keepSel = true;
                    }
                    if (rcButton("comment", "Comment", .disabled = !can_comment(st),
                                 .className = by_pointer("go h-8", "go h-11"),
                                 .variant = RC_BTN_PRIMARY))
                        add_comment(st, i, now);
                }
            }
        }
    }
}

/* The fold: a long thread shows its opening and its last few, since those decide
   what happens next, and what it folds is counted and one click away. */
enum { REPLIES_SHOWN = 6 };

static void timeline(RC_Arena *m, AppState *st, int i, float gutter, double now) {
    Issue    *it    = &st->issue[i];
    const int total = (int)it->comments;
    const int first = st->expanded == i || total <= REPLIES_SHOWN ? 0 : total - REPLIES_SHOWN;
    const int who   = it->who;
    int       k;

    rcColumn(.w = "grow") {
        comment_box(m, st, who, ago(m, it->age), body_of(m, st, i).chars, true, gutter);
        if (i >= SPARE) {
            const unsigned c = (unsigned)it->comments;
            const unsigned triage = (unsigned)it->age * (2u * c + 1u) / (2u * c + 2u);
            const int      triager = 1 + (int)((it->num * 31u) % (unsigned)(PEOPLE - 1));

            connector(gutter);
            event_row(m, gutter, rcIconTag, ui.muted, triager, "added", it->labels0,
                      ago(m, triage));
            if (it->assignee0 >= 0) {
                connector(gutter);
                event_row(m, gutter, rcIconUser, ui.muted, triager,
                          rcFormat(m, "assigned %s", HANDLE[it->assignee0]).chars, 0,
                          ago(m, triage));
            }
            if (it->milestone0 >= 0) {
                connector(gutter);
                event_row(m, gutter, rcIconFlag, ui.muted, triager,
                          rcFormat(m, "added this to %s", MILESTONE[it->milestone0].name).chars,
                          0, ago(m, triage));
            }
            if (first > 0) {
                const char *id = "more";

                connector(gutter);
                rcRow(.gap = 12, .align = "cl", .w = "grow") {
                    rcBox(.bg = ui.divider, .w = "grow", .hType = RC_PX(1)) {}
                    if (rcButton(id, rcFormat(m, "Show %d earlier comment%s", first,
                                              first == 1 ? "" : "s").chars,
                                 .className = by_pointer("h-7", "h-11")))
                        st->expanded = i;
                    rcBox(.bg = ui.divider, .w = "grow", .hType = RC_PX(1)) {}
                }
            }
            for (k = first; k < total; k++) {
                const Reply r = reply_of(it, k);

                connector(gutter);
                comment_box(m, st, r.who, ago(m, r.hoursAgo), r.text, r.who == who, gutter);
            }
            if (it->flags & F_CLOSED0) {
                connector(gutter);
                event_row(m, gutter, rcIconCircleCheck, ui.closed, triager,
                          "closed this as completed", 0, ago(m, fixture_closed_ago(it)));
            }
        }
        session_events(m, st, i, gutter, now);
        connector(gutter);
        composer(m, st, i, gutter, now);
    }
}

/* A heading in the sidebar and the gear beside it that edits its value. */
static void side_head(const char *text) {
    rcTextC(text, .font = rcFontSemibold(12.0f), .color = ui.muted, .wrap = "n");
    rcSpacer() {}
}

/* The label rows of a menu, shared by the filter bar, the sidebar and the bulk
   bar. They stay open, because a label is multi-select: pick several, then click
   away. It returns the label clicked, or NONE. */
static int label_rows(RC_Arena *m, const char *prefix, uint32_t on, const int *counts) {
    const bool finger = rcPointerIsCoarse();
    int        l, hit = NONE;

    rcColumn(.gap = 2, .wType = RC_PX(264)) {
        for (l = 0; l < LABELS; l++) {
            const char *id = rcFormat(m, "%s%d", prefix, l).chars;

            rcRow(.id = id, .gap = 8, .px = 8, .align = "cl", .borderRadius = "all-md", .w = "grow",
                  .hType = RC_PX(finger ? (float)HIT_MIN : 30.0f), .className = "wash") {
                rcBox(.align = "cc", .w = "16px") {
                    if (on >> l & 1u) rcIconCheck(16.0f, ui.accent);
                }
                rcBox(.bg = LABEL[l].color, .borderRadius = "all-full", .w = "12px", .h = "12px") {}
                rcTextC(LABEL[l].name, .color = ui.fg, .wrap = "n", .select = RC_SELECT_NONE);
                rcSpacer() {}
                if (counts)
                    rcText(rcFormatInt(m, counts[l], NULL), .color = ui.muted, .size = 12,
                           .wrap = "n", .select = RC_SELECT_NONE);
            }
            if (rcClicked(id)) hit = l;
        }
    }
    return hit;
}

/* Single-select menus, closing as they choose. Each returns the choice, or ANY
   while nothing was chosen; `none` names the empty choice, or NULL for none. */
static int person_items(const char *any, const char *none, int current) {
    int i, hit = ANY;

    if (any && rcMenuItem(any, .leadingIcon = current == ANY ? rcIconCheck : NULL)) hit = CHOSE_ANY;
    if (none && rcMenuItem(none, .leadingIcon = current == NONE ? rcIconCheck : NULL)) hit = NONE;
    for (i = 0; i < PEOPLE; i++)
        if (rcMenuItem(HANDLE[i], .leadingIcon = current == i ? rcIconCheck : NULL)) hit = i;
    return hit;
}

static int milestone_items(const char *any, const char *none, int current) {
    int i, hit = ANY;

    if (any && rcMenuItem(any, .leadingIcon = current == ANY ? rcIconCheck : NULL)) hit = CHOSE_ANY;
    if (none && rcMenuItem(none, .leadingIcon = current == NONE ? rcIconCheck : NULL)) hit = NONE;
    for (i = 0; i < MILESTONES; i++)
        if (rcMenuItem(MILESTONE[i].name, .leadingIcon = current == i ? rcIconCheck : NULL))
            hit = i;
    return hit;
}

/* Who has it, how it is labeled, where it is going, who is in it, and whether
   you hear about it. Each gear really edits, and the timeline records the edit. */
static void sidebar(RC_Arena *m, AppState *st, int i, bool beside, double now) {
    Issue    *it = &st->issue[i];
    bool      seen[PEOPLE] = { false };
    const int total = (int)it->comments;
    int       k, c, l, people = 0;

    seen[it->who] = true;
    for (k = total > REPLIES_SHOWN ? total - REPLIES_SHOWN : 0; k < total; k++)
        seen[reply_of(it, k).who] = true;
    if (it->mine) seen[ME] = true;
    for (k = 0; k < PEOPLE; k++) people += seen[k];

    rcColumn(.gap = 12, .wType = beside ? RC_PX(SIDEBAR_W) : RC_GROW) {
        rcColumn(.gap = 8, .w = "grow") {
            rcRow(.align = "cl", .w = "grow") {
                side_head("Assignees");
                if (rcBeginMenu("g_who", "", .className = by_pointer("h-6", "h-11"),
                                .variant = RC_BTN_GHOST, .leadingIcon = rcIconSettings)) {
                    c = person_items(NULL, "No one", it->assignee);
                    if (c != ANY) set_assignee(st, i, c, now);
                    rcEndMenu();
                }
            }
            if (it->assignee >= 0) {
                rcRow(.gap = 8, .align = "cl") {
                    avatar(it->assignee, 20.0f);
                    rcTextC(HANDLE[it->assignee], .color = ui.fg, .wrap = "n");
                }
            } else {
                if (text_link("g_self", NULL, "No one, assign yourself"))
                    set_assignee(st, i, ME, now);
            }
        }
        hrule();
        rcColumn(.gap = 8, .w = "grow") {
            rcRow(.align = "cl", .w = "grow") {
                side_head("Labels");
                if (rcBeginMenu("g_lab", "", .className = by_pointer("h-6", "h-11"),
                                .variant = RC_BTN_GHOST, .leadingIcon = rcIconSettings)) {
                    l = label_rows(m, "gl", it->labels, NULL);
                    if (l != NONE) set_label(st, i, l, !(it->labels >> l & 1u), now);
                    rcEndMenu();
                }
            }
            if (it->labels) {
                rcRow(.gap = 4, .w = "grow", .className = "flex-wrap") {
                    label_chips(m, it->labels, LABELS);
                }
            } else {
                rcTextL("None yet", .color = ui.muted);
            }
        }
        hrule();
        rcColumn(.gap = 8, .w = "grow") {
            rcRow(.align = "cl", .w = "grow") {
                side_head("Milestone");
                if (rcBeginMenu("g_ms", "", .className = by_pointer("h-6", "h-11"),
                                .variant = RC_BTN_GHOST, .leadingIcon = rcIconSettings)) {
                    c = milestone_items(NULL, "No milestone", it->milestone);
                    if (c != ANY) set_milestone(st, i, c, now);
                    rcEndMenu();
                }
            }
            if (it->milestone >= 0) {
                const int done = st->n.msClosed[it->milestone];
                const int all  = done + st->n.msOpen[it->milestone];

                rcRow(.gap = 8, .align = "cl", .w = "grow") {
                    rcIconFlag(14.0f, ui.muted);
                    rcTextC(MILESTONE[it->milestone].name, .color = ui.fg, .wrap = "n");
                }
                rcProgress("g_prog", all ? (float)done / (float)all : 0.0f, .className = "go-bar");
            } else {
                rcTextL("No milestone", .color = ui.muted);
            }
        }
        hrule();
        rcColumn(.gap = 8, .w = "grow") {
            rcText(rcFormat(m, "%d participant%s", people, people == 1 ? "" : "s"),
                   .font = rcFontSemibold(12.0f), .color = ui.muted);
            rcRow(.gap = 4, .w = "grow", .className = "flex-wrap") {
                for (k = 0; k < PEOPLE; k++) if (seen[k]) avatar(k, 24.0f);
            }
        }
        hrule();
        rcColumn(.gap = 8, .w = "grow") {
            const bool on = (it->flags & F_SUBSCRIBED) != 0;

            rcTextL("Notifications", .font = rcFontSemibold(12.0f), .color = ui.muted);
            if (rcButton("g_sub", on ? "Unsubscribe" : "Subscribe", .className = control_class(),
                         .leadingIcon = rcIconBell)) {
                it->flags ^= F_SUBSCRIBED;
                st->dirty   = true;
                st->keepSel = true;
            }
            rcTextC(on ? "You are receiving notifications from this thread."
                       : "You are not receiving notifications from this thread.",
                    .color = ui.muted, .size = 12);
        }
    }
}

/* Do not `return` out of a container body: the brace body is a macro-generated
   loop, so a return skips its close. The empty state is an else branch. */
static void issue_page(RC_Arena *m, AppState *st, float width, bool onePane, double now) {
    const bool  compact = is_compact();
    const float inner   = width - (compact ? 32.0f : 48.0f);
    const bool  beside  = inner >= (float)SIDEBAR_AT;
    /* The avatar gutter goes by the thread's own width, so a tight pane's comments keep theirs. */
    const float thread  = beside ? inner - (float)SIDEBAR_W - 24.0f : inner;
    const float gutter  = thread >= 520.0f ? 48.0f : 0.0f;

    rcColumn(.id = "detail", .bg = ui.canvas, .gap = 16, .pt = (float)(compact ? 8 : 16),
             .pb = (float)(24.0f + page_foot()), .px = (float)(compact ? 16 : 24),
             .scroll = "v", .w = "grow", .h = "grow") {
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            if (onePane && text_link("back", rcIconArrowLeft, "Issues")) {
                /* A view change, not a selection change: the list comes back on
                   the row you were reading. */
                st->detailOpen = false;
                st->reveal     = true;
            }
            rcSpacer() {}
            if (st->selRow >= 0)
                rcText(rcFormat(m, "%s of %s", rcFormatInt(m, st->selRow + 1, NULL).chars,
                                rcFormatInt(m, st->count, NULL).chars),
                       .color = ui.muted, .size = 12, .wrap = "n");
            if (icon_button("prev", rcIconChevronUp, "Previous issue (k)", false,
                            !st->count || st->selRow == 0))
                move(st, -1);
            if (icon_button("next", rcIconChevronDown, "Next issue (j)", false,
                            !st->count || st->selRow == st->count - 1))
                move(st, 1);
        }
        if (st->selIssue < 0) {
            rcColumn(.gap = 8, .pt = 48, .align = "tc", .w = "grow") {
                rcIconCircleDot(24.0f, ui.muted);
                rcTextC(st->count ? "No issue selected" : "No issue to show",
                        .font = rcFontSemibold(16.0f), .color = ui.fg);
                rcTextC(st->count ? "Pick one from the list to read its thread."
                                  : "The list is empty, so there is no thread to read.",
                        .color = ui.muted);
            }
        } else {
            const int    i  = st->selIssue;
            const Issue *it = &st->issue[i];

            rcColumn(.gap = 8, .w = "grow") {
                rcTextC(title_of(m, st, i), .font = rcFontSemibold(24.0f), .color = ui.fg);
                rcRow(.gap = 8, .align = "cl", .w = "grow", .className = "flex-wrap") {
                    /* The state badge: the color AND the word AND the glyph. */
                    rcRow(.bg = it->open ? ui.go : ui.closed, .gap = 4, .px = 12, .align = "cc",
                          .borderRadius = "all-full", .hType = RC_PX(32)) {
                        if (it->open) rcIconCircleDot(16.0f, RC_WHITE);
                        else          rcIconCircleCheck(16.0f, RC_WHITE);
                        rcTextC(it->open ? "Open" : "Closed", .font = rcFontSemibold(14.0f),
                                .color = RC_WHITE, .wrap = "n", .select = RC_SELECT_NONE);
                    }
                    rcTextC(HANDLE[it->who], .font = rcFontSemibold(14.0f), .color = ui.fg,
                            .wrap = "n");
                    rcText(rcFormat(m, "opened #%u %s, %d comment%s", it->num,
                                    ago(m, it->age).chars, thread_of(it),
                                    thread_of(it) == 1 ? "" : "s"),
                           .color = ui.muted);
                }
            }
            hrule();
            if (beside) {
                rcRow(.gap = 24, .w = "grow") {
                    timeline(m, st, i, gutter, now);
                    sidebar(m, st, i, true, now);
                }
            } else {
                timeline(m, st, i, gutter, now);
                hrule();
                sidebar(m, st, i, false, now);
            }
        }
    }
}

/* ---- the list ------------------------------------------------------------------ */

/* The assignee and the size of the thread, at a row's right end in every shape.
   The count holds its width with no thread, so the avatars keep one column. */
static void row_tail(RC_Arena *m, const Issue *it) {
    rcRow(.gap = 12, .align = "cl", .hType = RC_PX(20)) {
        if (it->assignee >= 0) avatar(it->assignee, 20.0f);
        rcRow(.gap = 4, .align = "cl", .wType = RC_PX(40)) {
            if (thread_of(it)) {
                rcIconMessageSquare(14.0f, ui.muted);
                rcText(rcFormat(m, "%d", thread_of(it)), .color = ui.muted, .size = 12, .wrap = "n",
                       .select = RC_SELECT_NONE);
            }
        }
    }
}

static void row_meta(RC_Arena *m, const Issue *it) {
    RC_String meta = it->open
        ? rcFormat(m, "#%u opened %s by %s", it->num, ago(m, it->age).chars, HANDLE[it->who])
        : rcFormat(m, "#%u by %s was closed %s", it->num, HANDLE[it->who],
                   ago(m, closed_ago(it)).chars);

    rcBox(.overflow = "hidden") {
        rcText(meta, .color = ui.muted, .size = 12, .wrap = "e", .select = RC_SELECT_NONE);
    }
    if (it->milestone >= 0) {
        rcRow(.gap = 4, .align = "cl") {
            rcIconFlag(12.0f, ui.muted);
            rcTextC(MILESTONE[it->milestone].name, .color = ui.muted, .size = 12, .wrap = "n",
                    .select = RC_SELECT_NONE);
        }
    }
}

/* ONE ROW, in the order a row is read: the state, the title, the labels, who
   opened it and when, with the assignee and the size of the thread at the right.
   The checkbox is polled inside the row, so its click is its own (rcClicked's
   NESTED CLICKABLES). Only the visible rows ever reach this function. */
static void issue_row(RC_Arena *m, AppState *st, const RC_VirtualRow *row, int shape, int pitch,
                      bool showCursor, bool picks) {
    const int    i     = st->sel[row->index];
    const Issue *it    = &st->issue[i];
    const char  *id    = rcFormat(m, "r%u", it->num).chars;
    const bool   here  = row->index == st->selRow && showCursor;
    const bool   over  = rcShowsHover(id);      /* the title's color: a run has no state */
    const bool   picked = (it->flags & F_PICKED) != 0;
    /* The title's line, which the box and the state glyph beside it stand on. */
    const uint16_t line = (uint16_t)(shape == ROW_WIDE ? 22 : 20);

    rcColumn(.id = id, .w = "grow", .hType = RC_PX(pitch),
             .className = here ? "highlight" : "wash") {
        rcRow(.w = "grow", .h = "grow") {
            /* The cursor gets a second channel: a tinted fill is a hue difference,
               which glare and grayscale lose; the 3 px rule is a position. */
            rcBox(.bg = here ? ui.accent : RC_UNSET_COLOR, .h = "grow", .wType = RC_PX(3)) {}
            rcRow(.gap = 12, .pl = 9, .pr = 16, .align = "cl", .w = "grow", .h = "grow",
                  .className = "min-w-0") {
                /* The block centers in the pitch, and inside it the glyph keeps to the
                   title's first line, as GitHub's does, however many lines follow. */
                rcRow(.gap = 12, .align = "tl", .w = "grow", .className = "min-w-0") {
                    rcRow(.gap = 12, .align = "cl", .hType = RC_PX(line)) {
                        if (picks) {
                            bool v = picked;
                            if (rcCheckbox(rcFormat(m, "c%u", it->num).chars, "", &v))
                                toggle_pick(st, row->index);
                        }
                        if (it->open) rcIconCircleDot(16.0f, ui.open);
                        else          rcIconCircleCheck(16.0f, ui.closed);
                    }
                    /* min-w-0, as on the web: the column shrinks below its title's ellipsis. */
                    rcColumn(.gap = 4, .w = "grow", .className = "min-w-0") {
                        /* Top-aligned under a title that may wrap: the tail stays on line 1. */
                        rcRow(.gap = 8, .align = shape == ROW_NARROW ? "tl" : "cl", .w = "grow") {
                            rcBox(.overflow = "hidden") {
                                rcTextC(title_of(m, st, i),
                                        .font = rcFontSemibold(shape == ROW_WIDE ? 16.0f : 14.0f),
                                        .color = over ? ui.accent : ui.fg, .lineHeight = line,
                                        .wrap = shape == ROW_NARROW ? NULL : "e",
                                        .select = RC_SELECT_NONE,
                                        .lineClamp = (uint8_t)(shape == ROW_NARROW ? 2 : 0));
                            }
                            if (shape == ROW_WIDE) {
                                label_chips(m, it->labels, 3);
                            } else {
                                rcSpacer() {}
                                row_tail(m, it);
                            }
                        }
                        if (shape == ROW_NARROW)
                            rcRow(.gap = 4, .align = "cl", .overflow = "hidden", .w = "grow") {
                                label_chips(m, it->labels, 3);
                            }
                        rcRow(.gap = 8, .align = "cl", .overflow = "hidden", .w = "grow") {
                            row_meta(m, it);
                            /* The meta is what a reader wants, so the chips give way to it. */
                            if (shape == ROW_MID)
                                label_chips(m, it->labels, it->milestone >= 0 ? 1 : 2);
                        }
                    }
                }
                if (shape == ROW_WIDE) row_tail(m, it);
            }
        }
        rcBox(.bg = ui.divider, .w = "grow", .hType = RC_PX(1)) {}
    }
    if (rcClicked(id)) open_row(st, row->index);
}

static void scope_menu(AppState *st) {
    if (rcBeginMenu("m_scope", "Filters", .caret = true)) {
        const bool me = st->f.assignee == ME, mine = st->f.author == ME;

        if (rcMenuItem("Open issues",
                       .leadingIcon = filter_is_default(&st->f) ? rcIconCheck : NULL))
            clear_filters(st);
        if (rcMenuItem("Your issues", .leadingIcon = mine ? rcIconCheck : NULL)) {
            clear_filters(st);
            st->f.author = ME;
            filter_changed(st);
        }
        if (rcMenuItem("Assigned to you", .leadingIcon = me ? rcIconCheck : NULL)) {
            clear_filters(st);
            st->f.assignee = ME;
            filter_changed(st);
        }
        if (rcMenuItem("Mentioning you", .leadingIcon = st->f.mentions ? rcIconCheck : NULL)) {
            clear_filters(st);
            st->f.mentions = true;
            filter_changed(st);
        }
        if (rcMenuItem("Subscribed", .leadingIcon = st->f.subscribed ? rcIconCheck : NULL)) {
            clear_filters(st);
            st->f.subscribed = true;
            filter_changed(st);
        }
        rcEndMenu();
    }
}

/* The field and the green button, as tall as each other under either pointer. A click
   on the field's clear x empties the query as an edit does, so one branch parses both. */
static void search_row(AppState *st, bool compact) {
    rcRow(.gap = 8, .px = compact ? 12.0f : 0.0f, .align = "cl", .w = "grow") {
        if (!compact) scope_menu(st);
        rcBox(.w = "grow") {
            if (rcTextInput("q", st->query, QCAP, .className = control_class(),
                            .placeholder = "Search all issues", .leadingIcon = rcIconSearch,
                            .clearButton = true)) {
                parse_query(st->query, &st->f);
                st->dirty = true;
            }
            /* After the field: see shortcuts(). */
            if (st->focusQuery) {
                rcSetFocus("q");
                st->focusQuery = false;
            }
        }
        if (rcButton("new", "New issue", .className = by_pointer("go h-8", "go h-11"),
                     .variant = RC_BTN_PRIMARY)) {
            st->composing = st->composeFresh = true;
        }
    }
}

/* The line that undoes a query: a list left filtered yesterday must not read as
   an empty tracker. */
static void clear_row(AppState *st, bool compact) {
    const bool over = rcShowsHover("clear");

    rcRow(.px = (float)(compact ? 12 : 0), .w = "grow") {
        rcRow(.id = "clear", .gap = 8, .align = "cl", .hType = RC_PX(24)) {
            rcBox(.bg = over ? ui.accent : ui.muted, .align = "cc", .borderRadius = "all-sm",
                  .w = "16px", .h = "16px") {
                rcIconX(12.0f, ui.canvas);
            }
            rcTextL("Clear current search query, filters, and sorts",
                    .color = over ? ui.accent : ui.muted, .select = RC_SELECT_NONE);
        }
        if (rcClicked("clear")) clear_filters(st);
    }
}

/* Open and Closed: a count and a state, the current one in full ink. Choosing one
   rewrites state: in the query. It has no padding of its own, so on the compact
   arm its glyph starts on the line the field, the menus and the rows start on. */
static void state_tab(RC_Arena *m, AppState *st, bool closed) {
    const char *id = closed ? "closed" : "open";
    const bool  on = st->f.closed == closed;
    const RC_Color c = on || rcShowsHover(id) ? ui.fg : ui.muted;

    rcRow(.id = id, .gap = 6, .align = "cl", .hType = RC_PX(control_h())) {
        if (closed) rcIconCheck(16.0f, c);
        else        rcIconCircleDot(16.0f, c);
        rcText(rcFormatInt(m, st->n.perState[closed ? 1 : 0], NULL),
               .font = weight(on, 14.0f), .color = c, .wrap = "n",
               .select = RC_SELECT_NONE);
        rcTextC(closed ? "Closed" : "Open", .font = weight(on, 14.0f), .color = c,
                .wrap = "n", .select = RC_SELECT_NONE);
    }
    if (rcClicked(id) && !on) {
        st->f.closed = closed;
        filter_changed(st);
    }
}

/* A filter menu says what it holds, so a narrowed list explains itself. Its label
   comes from the frame arena, which outlives the frame's draw. */
static void filter_menus(RC_Arena *m, AppState *st) {
    Filter *f = &st->f;
    int     c, i, labels = 0;

    for (i = 0; i < LABELS; i++) labels += f->labels >> i & 1u;
    if (rcBeginMenu("m_author", f->author >= 0 ? HANDLE[f->author] : "Author", .caret = true)) {
        c = person_items("Anyone", NULL, f->author);
        if (c != ANY) { f->author = c == CHOSE_ANY ? ANY : c; filter_changed(st); }
        rcEndMenu();
    }
    /* Labels are multi-select: this popup's rows toggle rather than close it. */
    if (rcBeginMenu("m_label", labels ? rcFormat(m, "Labels %d", labels).chars : "Labels",
                    .caret = true)) {
        st->labelsOpen = true;
        c = label_rows(m, "ml", f->labels, st->n.labelHere);
        if (c != NONE) { f->labels ^= 1u << c; filter_changed(st); }
        rcEndMenu();
    }
    if (rcBeginMenu("m_ms", f->milestone >= 0 ? MILESTONE[f->milestone].name :
                            f->milestone == NONE ? "No milestone" : "Milestone", .caret = true)) {
        c = milestone_items("Any milestone", "No milestone", f->milestone);
        if (c != ANY) { f->milestone = c == CHOSE_ANY ? ANY : c; filter_changed(st); }
        rcEndMenu();
    }
    if (rcBeginMenu("m_who", f->assignee >= 0 ? HANDLE[f->assignee] :
                             f->assignee == NONE ? "No assignee" : "Assignee", .caret = true)) {
        c = person_items("Anyone", "Assigned to nobody", f->assignee);
        if (c != ANY) { f->assignee = c == CHOSE_ANY ? ANY : c; filter_changed(st); }
        rcEndMenu();
    }
    if (rcBeginMenu("m_sort", SORT[f->sort], .caret = true)) {
        for (i = 0; i < SORTS; i++)
            if (rcMenuItem(SORT[i], .leadingIcon = f->sort == i ? rcIconCheck : NULL)) {
                f->sort = i;
                filter_changed(st);
            }
        rcEndMenu();
    }
}

/* With rows picked the bar acts on them instead. One list-wide pass, on a click. */
static void bulk_actions(RC_Arena *m, AppState *st, double now) {
    int k, c;

    if (rcButton("b_close", st->f.closed ? "Reopen" : "Close", .className = control_class(),
                 .leadingIcon = st->f.closed ? icon_reopen : icon_close)) {
        for (k = 0; k < st->count; k++)
            if (st->issue[st->sel[k]].flags & F_PICKED) set_open(st, st->sel[k], st->f.closed, now);
        st->stayAt = true;
    }
    if (rcBeginMenu("b_label", "Label", .caret = true)) {
        c = label_rows(m, "bl", picked_labels(st), NULL);
        if (c != NONE) label_picked(st, c, now);
        rcEndMenu();
    }
    if (rcBeginMenu("b_who", "Assign", .caret = true)) {
        c = person_items(NULL, "No one", ANY);
        if (c != ANY)
            for (k = 0; k < st->count; k++)
                if (st->issue[st->sel[k]].flags & F_PICKED) set_assignee(st, st->sel[k], c, now);
        rcEndMenu();
    }
    if (rcBeginMenu("b_ms", "Milestone", .caret = true)) {
        c = milestone_items(NULL, "No milestone", ANY);
        if (c != ANY)
            for (k = 0; k < st->count; k++)
                if (st->issue[st->sel[k]].flags & F_PICKED) set_milestone(st, st->sel[k], c, now);
        rcEndMenu();
    }
}

static void header_actions(RC_Arena *m, AppState *st, double now) {
    if (st->picked) {
        bulk_actions(m, st, now);
    } else {
        filter_menus(m, st);
    }
}

/* The compact arm: Open and Closed, then the menus, on one row that slides sideways
   as a phone's filter chips do. A finger swipes it; a mouse drags the row's own bar,
   so the menus never stack three rows deep above the first issue. Under a mouse the
   bar takes its own 12 px under the menus while the row overflows, below the row's
   bottom padding of 6 px, which keeps the thumb off the menus. */
static void compact_header(RC_Arena *m, AppState *st, bool shortWindow) {
    const char *menus = "overflow-x-auto";

    rcRow(.id = "bar1", .gap = 8, .pb = (float)(shortWindow ? 6 : 0), .align = "cl", .w = "grow",
          .className = shortWindow ? menus : "") {
        rcRow(.gap = 16, .align = "cl") {
            state_tab(m, st, false);
            state_tab(m, st, true);
        }
        if (shortWindow) {
            scope_menu(st);
            filter_menus(m, st);
        }
    }
    if (!shortWindow)
        rcRow(.id = "bar2", .gap = 8, .pb = 6, .align = "cl", .w = "grow", .className = menus) {
            scope_menu(st);
            filter_menus(m, st);
        }
    rcScrollbar(shortWindow ? "bar1" : "bar2");
}

/* The framed arm: the pick-all box, then Open and Closed or the count of picks, then
   the menus on the same line where they fit and on their own where they do not.
   The picks swap one line's contents and never its height, and the bar holds the
   height it had before the first tick, so the rows stay under the pointer. */
static void list_header(RC_Arena *m, AppState *st, bool compact, bool shortWindow, double now) {
    const bool oneLine = rows_width(st) >= (float)BAR_LINE_W;
    bool       all     = st->count > 0 && st->picked == st->count;

    if (!st->picked) st->headH = rcGetElementBox("lhead").height;
    /* The one class among the radii: a .borderRadius is a literal, and this one
       comes and goes with the frame. */
    rcColumn(.id = "lhead", .bg = ui.subtle, .gap = 4, .px = 12, .py = (float)(compact ? 4 : 8),
             .w = "grow", .hMin = st->picked && !compact ? RC_VAL(st->headH) : RC_UNSET_VAL,
             .className = compact ? "" : "rounded-t-md") {
        if (compact) {
            compact_header(m, st, shortWindow);
        } else {
            rcRow(.gap = 8, .align = "cl", .w = "grow", .hType = RC_PX(control_h())) {
                if (rcCheckbox("pick_all", "", &all)) pick_all(st, all);
                if (st->picked) {
                    rcText(rcFormat(m, "%s selected", rcFormatInt(m, st->picked, NULL).chars),
                           .font = rcFontSemibold(14.0f), .color = ui.fg, .wrap = "n");
                } else {
                    /* The bar's 8 px gap and this 4 set Open's glyph 12 px after the
                       pick-all box, where the rows set theirs after their own boxes. */
                    rcRow(.gap = 20, .pl = 4, .align = "cl") {
                        state_tab(m, st, false);
                        state_tab(m, st, true);
                    }
                }
                if (oneLine) {
                    rcSpacer() {}
                    header_actions(m, st, now);
                }
            }
            if (!oneLine)
                rcRow(.gap = 4, .align = "cl", .w = "grow", .className = "flex-wrap") {
                    header_actions(m, st, now);
                }
        }
    }
}

/* THE BUBBLE: while j or k is held, where you are, in numbers, because the list is
   moving too fast to read. The cursor rides the end it is flying toward, so the
   bubble takes the other end, and it floats over the rows, not in their scroll box. */
static void fly_bubble(RC_Arena *m, const AppState *st) {
    const Issue *it = st->selIssue >= 0 ? &st->issue[st->selIssue] : NULL;
    const bool   down = st->flyDir > 0;

    if (st->flying && it) {
        rcRow(.bg = ui.bubble, .gap = 8, .px = 16, .align = "cc", .borderRadius = "all-full",
              .hType = RC_PX(36),
              .floating = { .to = RC_ATTACH_PARENT,
                            .parent = down ? RC_ANCHOR_TOP_CENTER : RC_ANCHOR_BOTTOM_CENTER,
                            .element = down ? RC_ANCHOR_TOP_CENTER : RC_ANCHOR_BOTTOM_CENTER,
                            .offset = { 0.0f, down ? 16.0f : -16.0f },
                            .zIndex = 5, .capture = RC_CAPTURE_PASSTHROUGH }) {
            rcText(rcFormat(m, "%s / %s", rcFormatInt(m, st->selRow + 1, NULL).chars,
                            rcFormatInt(m, st->count, NULL).chars),
                   .font = rcFontSemibold(14.0f), .color = RC_WHITE, .wrap = "n",
                   .select = RC_SELECT_NONE);
            rcText(ago(m, it->age), .color = rcRgb(200, 208, 216), .wrap = "n",
                   .select = RC_SELECT_NONE);
        }
    }
}

static void list_rows(RC_Arena *m, AppState *st, bool compact) {
    const int  shape  = row_shape(st);
    const int  pitch  = row_pitch(st);
    const bool cursor = two_panes(st) || st->cursorShown;
    /* The bulk bar needs the width of a framed list, and a finger has no x key. */
    const bool picks  = !compact && !rcPointerIsCoarse();

    rcColumn(.id = "list", .pb = (float)page_foot(), .scroll = "v", .w = "grow", .h = "grow") {
        if (!st->count)
            rcColumn(.gap = 8, .p = 48, .align = "tc", .w = "grow") {
                rcIconCircleDot(24.0f, ui.muted);
                rcTextL("No results matched your search.", .font = rcFontSemibold(20.0f),
                        .color = ui.fg);
                rcTextL("Clear the query to see every issue again.", .color = ui.muted);
            }
        /* The pitch is read once, outside the loop: rcVirtualList sizes its
           spacers from it, and every row must really be that tall. */
        rcVirtualList(row, "list", st->count, (float)pitch) {
            issue_row(m, st, &row, shape, pitch, cursor, picks);
        }
    }
}

/* GitHub closes its list with a tip, and this one says the example's point. */
static void pro_tip(RC_Arena *m, const AppState *st) {
    rcRow(.gap = 4, .align = "cc", .w = "grow", .hType = RC_PX(16)) {
        rcTextL("ProTip!", .font = rcFontSemibold(12.0f), .color = ui.muted, .wrap = "n");
        rcText(rcFormat(m, "Hold j to fly through all %s issues.",
                        rcFormatInt(m, SPARE + ISSUES - st->first, NULL).chars),
               .color = ui.muted, .size = 12, .wrap = "n");
    }
}

static void list_pane(RC_Arena *m, AppState *st, double now) {
    const bool compact     = is_compact();
    const bool shortWindow = rcViewport().height < (float)SHORT_H;

    rcColumn(.bg = ui.canvas, .gap = 8, .pt = compact ? 8.0f : 16.0f,
             .pb = compact ? 0.0f : 8.0f, .px = compact ? 0.0f : 16.0f, .w = "grow",
             .h = "grow") {
        search_row(st, compact);
        if (st->showClear) clear_row(st, compact);
        /* Framed on a wide window, edge to edge on a compact one: a border takes
           room, so a compact frame has none and the list reaches the window's edge. */
        rcColumn(.borderRadius = "all-md",
                 .border = { .color = ui.border, .width = compact ? 0.0f : 1.0f },
                 .w = "grow", .h = "grow") {
            if (compact) hrule();
            list_header(m, st, compact, shortWindow, now);
            hrule();
            rcColumn(.w = "grow", .h = "grow") {
                list_rows(m, st, compact);
                fly_bubble(m, st);
            }
        }
        if (!compact && !shortWindow) pro_tip(m, st);
    }
}

/* ---- Milestones, Labels, Pulse: the repository's other tabs --------------------- */

static void go_filtered(AppState *st) {
    filter_changed(st);
    go_page(st, PAGE_ISSUES);
}

static void milestones_page(RC_Arena *m, AppState *st, bool compact) {
    int i, active = 0;

    for (i = 0; i < MILESTONES; i++) active += st->n.msOpen[i] > 0;
    rcColumn(.id = "pagebody", .bg = ui.canvas, .pt = 16, .pb = (float)(24.0f + page_foot()),
             .px = (float)(compact ? 12 : 24), .scroll = "v", .w = "grow", .h = "grow") {
        rcColumn(.borderRadius = "all-md", .border = { .color = ui.border, .width = 1 },
                 .w = "grow") {
            rcRow(.bg = ui.subtle, .gap = 8, .px = 16, .align = "cl", .borderRadius = "t-md",
                  .w = "grow", .hType = RC_PX(48)) {
                rcIconFlag(16.0f, ui.fg);
                rcText(rcFormat(m, "%d Open, %d Closed", active, MILESTONES - active),
                       .font = rcFontSemibold(14.0f), .color = ui.fg, .wrap = "n");
            }
            for (i = 0; i < MILESTONES; i++) {
                const char *id    = rcFormat(m, "ms%d", i).chars;
                const int   open  = st->n.msOpen[i], done = st->n.msClosed[i];
                const int   all   = open + done;
                const int   pct   = all ? done * 100 / all : 0;
                const bool  over  = rcShowsHover(id);   /* the name's color */

                hrule();
                rcRow(.id = id, .gap = 24, .p = 16, .align = "cl", .w = "grow",
                      .className = "wash flex-wrap") {
                    rcColumn(.gap = 4, .w = "grow", .wMin = RC_VAL(240)) {
                        rcTextC(MILESTONE[i].name, .font = rcFontSemibold(20.0f),
                                .color = over ? ui.accent : ui.fg, .select = RC_SELECT_NONE);
                        rcRow(.gap = 4, .align = "cl") {
                            rcTextC(MILESTONE[i].due, .color = ui.muted, .size = 12, .wrap = "n",
                                    .select = RC_SELECT_NONE);
                        }
                        rcTextC(MILESTONE[i].about, .color = ui.fg, .select = RC_SELECT_NONE);
                    }
                    rcColumn(.gap = 8, .wType = RC_PX(320), .wMin = RC_VAL(240)) {
                        rcProgress(rcFormat(m, "mp%d", i).chars, (float)pct / 100.0f,
                                   .className = "go-bar");
                        rcRow(.gap = 16, .align = "cl", .w = "grow") {
                            rcText(rcFormat(m, "%d%% complete", pct),
                                   .font = rcFontSemibold(12.0f), .color = ui.fg, .wrap = "n");
                            rcText(rcFormat(m, "%s open", rcFormatInt(m, open, NULL).chars),
                                   .color = ui.muted, .size = 12, .wrap = "n");
                            rcText(rcFormat(m, "%s closed", rcFormatInt(m, done, NULL).chars),
                                   .color = ui.muted, .size = 12, .wrap = "n");
                        }
                    }
                }
                if (rcClicked(id)) {
                    clear_filters(st);
                    st->f.milestone = i;
                    go_filtered(st);
                }
            }
        }
    }
}

static void labels_page(RC_Arena *m, AppState *st, bool compact) {
    char needle[32];
    int  i, k, shown = 0;

    rcColumn(.id = "pagebody", .bg = ui.canvas, .gap = 16, .pt = 16,
             .pb = 24.0f + page_foot(), .px = compact ? 12.0f : 24.0f,
             .scroll = "v", .w = "grow", .h = "grow") {
        rcBox(.w = "grow", .wMax = RC_VAL(420)) {
            rcTextInput("labelq", st->labelQuery, 32, .className = control_class(),
                        .placeholder = "Search all labels", .leadingIcon = rcIconSearch,
                        .clearButton = true);
        }
        /* Matched after the field, so the count and the rows see this frame's edit, a click
           on its clear x included. */
        for (k = 0; st->labelQuery[k] && k < 31; k++) needle[k] = lower(st->labelQuery[k]);
        needle[k] = '\0';
        for (i = 0; i < LABELS; i++) shown += has(LABEL[i].name, needle);
        rcColumn(.borderRadius = "all-md", .border = { .color = ui.border, .width = 1 },
                 .w = "grow") {
            rcRow(.bg = ui.subtle, .px = 16, .align = "cl", .borderRadius = "t-md", .w = "grow",
                  .hType = RC_PX(48)) {
                rcText(rcFormat(m, "%d label%s", shown, shown == 1 ? "" : "s"),
                       .font = rcFontSemibold(14.0f), .color = ui.fg, .wrap = "n");
            }
            if (!shown) {
                hrule();
                rcColumn(.gap = 8, .p = 32, .align = "tc", .w = "grow") {
                    rcIconTag(24.0f, ui.muted);
                    rcTextL("No labels match your search.", .font = rcFontSemibold(16.0f),
                            .color = ui.fg);
                }
            }
            for (i = 0; i < LABELS; i++) {
                const char *id = rcFormat(m, "lb%d", i).chars;

                if (!has(LABEL[i].name, needle)) continue;
                hrule();
                rcRow(.id = id, .gap = 16, .px = 16, .py = 12, .align = "cl", .w = "grow",
                      .className = "wash") {
                    rcBox(.wType = RC_PX(compact ? 120.0f : 200.0f)) { label_chip(i); }
                    if (!compact)
                        rcBox(.w = "grow") {
                            rcTextC(LABEL[i].about, .color = ui.muted, .select = RC_SELECT_NONE);
                        }
                    else
                        rcSpacer() {}
                    rcText(rcFormat(m, "%s open", rcFormatInt(m, st->n.labelOpen[i], NULL).chars),
                           .color = ui.muted, .wrap = "n", .select = RC_SELECT_NONE);
                }
                if (rcClicked(id)) {
                    clear_filters(st);
                    st->f.labels = 1u << i;
                    go_filtered(st);
                }
            }
        }
    }
}

/* A number with its caption, on a panel. */
static void stat_tile(RC_Arena *m, const char *caption, int value, RC_Color ink) {
    rcColumn(.gap = 4, .p = 16, .borderRadius = "all-md",
             .border = { .color = ui.border, .width = 1 }, .w = "grow") {
        rcTextC(caption, .color = ui.muted, .size = 12, .wrap = "n");
        rcText(rcFormatInt(m, value, NULL), .font = rcFontSemibold(24.0f), .color = ink,
               .wrap = "n");
    }
}

static void pulse_page(RC_Arena *m, AppState *st, bool compact) {
    const Counts *n = &st->n;
    RC_Series     series[2];
    int order[PEOPLE], i, j, top, opened = 0, closed = 0;

    series[0] = RC_LIT(RC_Series){ .y = st->chartOpened, .count = WEEKS, .kind = RC_SERIES_AREA,
                                   .color = ui.go, .label = "Opened" };
    series[1] = RC_LIT(RC_Series){ .y = st->chartClosed, .count = WEEKS, .kind = RC_SERIES_LINE,
                                   .color = ui.closed, .label = "Closed" };

    for (i = 0; i < WEEKS; i++) { opened += n->weekOpened[i]; closed += n->weekClosed[i]; }
    /* A few rows of a small table: selection order, drawn straight from the count. */
    for (i = 0; i < PEOPLE; i++) order[i] = i;
    for (i = 0; i < PEOPLE - 1; i++)
        for (j = i + 1; j < PEOPLE; j++)
            if (n->reporter[order[j]] > n->reporter[order[i]]) {
                const int t = order[i];
                order[i] = order[j];
                order[j] = t;
            }
    top = n->reporter[order[0]] > 0 ? n->reporter[order[0]] : 1;

    rcColumn(.id = "pagebody", .bg = ui.canvas, .gap = 16, .pt = 16,
             .pb = (float)(24.0f + page_foot()), .px = (float)(compact ? 12 : 24),
             .scroll = "v", .w = "grow", .h = "grow") {
        rcColumn(.gap = 4, .w = "grow") {
            rcTextL("Pulse", .font = rcFontSemibold(24.0f), .color = ui.fg);
            rcTextL("Issue activity over the last 26 weeks", .color = ui.muted);
        }
        /* The tiles hold no query: every count is the whole repository's. Four across,
           or two by two where four would not fit. */
        rcColumn(.gap = 12, .w = "grow") {
            rcRow(.gap = 12, .w = "grow") {
                stat_tile(m, "Open now", n->openAll, ui.open);
                stat_tile(m, "Opened, 26 weeks", opened, ui.fg);
                if (!compact) {
                    stat_tile(m, "Closed, 26 weeks", closed, ui.closed);
                    stat_tile(m, "Opened this week", n->weekOpened[0], ui.fg);
                }
            }
            if (compact)
                rcRow(.gap = 12, .w = "grow") {
                    stat_tile(m, "Closed, 26 weeks", closed, ui.closed);
                    stat_tile(m, "Opened this week", n->weekOpened[0], ui.fg);
                }
        }
        rcColumn(.gap = 12, .p = 16, .borderRadius = "all-md",
                 .border = { .color = ui.border, .width = 1 }, .w = "grow") {
            rcTextL("Issues per week", .font = rcFontSemibold(14.0f), .color = ui.fg);
            rcBox(.w = "grow", .hType = RC_PX(compact ? 220.0f : 300.0f)) {
                rcChart("weeks", series, 2, .className = "text-[12px]",
                        .x = { .ticks = 7, .labels = st->weekLabel },
                        .y = { .grid = true }, .legend = true, .tooltip = true);
            }
        }
        rcColumn(.gap = 8, .p = 16, .borderRadius = "all-md",
                 .border = { .color = ui.border, .width = 1 }, .w = "grow") {
            rcTextL("Top reporters", .font = rcFontSemibold(14.0f), .color = ui.fg);
            for (i = 0; i < 6; i++) {
                const int who = order[i];

                rcRow(.gap = 12, .align = "cl", .w = "grow") {
                    avatar(who, 24.0f);
                    rcBox(.wType = RC_PX(compact ? 104.0f : 140.0f)) {
                        rcTextC(HANDLE[who], .color = ui.fg, .wrap = "n");
                    }
                    rcProgress(rcFormat(m, "tp%d", i).chars,
                               (float)n->reporter[who] / (float)top, .className = "go-bar");
                    rcBox(.align = "cr", .wType = RC_PX(48)) {
                        rcText(rcFormatInt(m, n->reporter[who], NULL), .color = ui.muted,
                               .size = 12, .wrap = "n");
                    }
                }
            }
        }
    }
}

/* ---- the dialogs ----------------------------------------------------------------- */

/* rcBeginModal's panel holds its height inside the window and scrolls a taller
   form itself, but a body wider than the window is cut at its edge, so the body
   is held to the usable width: 20 px of panel padding and a 16 px margin a side. */
static float dialog_width(float most) {
    const float wide = usable_width() - 2.0f * (20.0f + 16.0f);

    return wide < most ? wide : most;
}

static void pick_field(const char *caption, const char *id, int *value, const char *const *items,
                       int count) {
    rcColumn(.gap = 4, .w = "grow", .wMin = RC_VAL(140)) {
        rcTextC(caption, .font = rcFontSemibold(12.0f), .color = ui.fg);
        rcCombo(id, value, items, count, .className = control_class());
    }
}

static void new_issue_dialog(RC_Arena *m, AppState *st) {
    static const char *const KIND[] = { "bug", "enhancement", "documentation", "performance",
                                        "question" };
    static const char *const PRIO[] = { "No priority", "P0", "P1", "P2", "P3" };
    const bool full = st->first == 0;

    if (rcBeginModal("compose", &st->composing)) {
        rcColumn(.gap = 12, .wType = RC_PX(dialog_width(640.0f))) {
            rcColumn(.gap = 2, .w = "grow") {
                rcTextL("Create new issue", .font = rcFontSemibold(20.0f), .color = ui.fg);
                rcTextL("in tessera/editor", .color = ui.muted, .size = 12);
            }
            rcColumn(.gap = 4, .w = "grow") {
                rcTextL("Add a title", .font = rcFontSemibold(12.0f), .color = ui.fg);
                rcBox(.w = "grow") {
                    rcTextInput("ntitle", st->draftTitle, TITLE_CAP, .className = control_class(),
                                .placeholder = "Title", .clearButton = true);
                }
                if (st->composeFresh) {
                    rcSetFocus("ntitle");
                    st->composeFresh = false;
                }
            }
            rcColumn(.gap = 4, .w = "grow") {
                rcRow(.gap = 4, .align = "cl", .w = "grow") {
                    rcTextL("Add a description", .font = rcFontSemibold(12.0f), .color = ui.fg);
                    rcSpacer() {}
                    if (rcButton("d_write", "Write", .className = by_pointer("h-6", "h-11"),
                                 .variant = st->draftPreview ? RC_BTN_GHOST : RC_BTN_DEFAULT))
                        st->draftPreview = false;
                    if (rcButton("d_preview", "Preview", .className = by_pointer("h-6", "h-11"),
                                 .variant = st->draftPreview ? RC_BTN_DEFAULT : RC_BTN_GHOST))
                        st->draftPreview = true;
                }
                if (st->draftPreview) {
                    rcColumn(.p = 12, .borderRadius = "all-md",
                             .border = { .color = ui.border, .width = 1 }, .w = "grow",
                             .hMin = RC_VAL(132)) {
                        if (!blank(st->draftBody)) markdown(m, st, st->draftBody);
                        else rcTextL("Nothing to preview.", .color = ui.muted);
                    }
                } else {
                    rcTextArea("nbody", st->draftBody, BODY_CAP,
                               .placeholder = "Markdown is supported", .rows = 6);
                }
            }
            rcRow(.gap = 12, .w = "grow", .className = "flex-wrap") {
                pick_field("Type", "d_kind", &st->draftKind, KIND,
                           (int)(sizeof KIND / sizeof KIND[0]));
                pick_field("Priority", "d_prio", &st->draftPrio, PRIO,
                           (int)(sizeof PRIO / sizeof PRIO[0]));
                pick_field("Assignee", "d_who", &st->draftWho, st->whoItems, PEOPLE + 1);
                pick_field("Milestone", "d_ms", &st->draftMilestone, st->milestoneItems,
                           MILESTONES + 1);
            }
            hrule();
            rcRow(.gap = 8, .align = "cl", .w = "grow", .className = "flex-wrap") {
                if (full) rcTextL("This session has filed all it can.", .color = ui.muted);
                else      rcCheckbox("d_more", "Create more", &st->createMore);
                rcSpacer() {}
                if (rcButton("d_cancel", "Cancel", .className = control_class()))
                    st->composing = false;
                if (rcButton("d_create", "Create", .disabled = full || blank(st->draftTitle),
                             .className = by_pointer("go h-8", "go h-11"),
                             .variant = RC_BTN_PRIMARY))
                    file_issue(st);
            }
        }
        rcEndModal();
    }
}

static void help_row(const char *key, const char *what) {
    rcRow(.gap = 12, .align = "cl", .w = "grow") {
        rcBox(.wType = RC_PX(112)) { kbd(key); }
        rcTextC(what, .color = ui.fg);
    }
}

/* Every key the app answers, and this dialog is the only list of them. */
static void help_dialog(AppState *st) {
    if (rcBeginModal("help", &st->helpOpen)) {
        rcColumn(.gap = 8, .wType = RC_PX(dialog_width(420.0f))) {
            rcTextL("Keyboard shortcuts", .font = rcFontSemibold(20.0f), .color = ui.fg);
            hrule();
            help_row("j or Down", "Next issue; hold to fly");
            help_row("k or Up", "Previous issue; hold to fly");
            help_row("PgUp, PgDn", "A screenful of the list");
            help_row("Home, End", "The first or the last issue");
            help_row("Enter or o", "Open the issue");
            help_row("u", "Back to the list");
            help_row("Esc", "Back, then clear the picks, then the filters");
            help_row("x", "Select the issue");
            help_row("e", "Close or reopen the issue");
            help_row("r", "Reply to the issue");
            help_row("Cmd/Ctrl+Enter", "Send the comment or file the issue");
            help_row("c", "Create an issue");
            help_row("/", "Search");
            help_row("p", "Show or hide the issue pane");
            help_row("?", "This list");
        }
        rcEndModal();
    }
}

/* ---- the frame ---------------------------------------------------------------------- */

/* A key that opens a field types nothing into it: its keydown's default is
   prevented, as a page's handler calls preventDefault (rcKeyEventConsume). A
   field a key focuses takes the frame's typed text otherwise, as a browser types
   a key into the input its keydown handler focused. */
static void consume_key(RC_Key key) {
    RC_KeyEvent e;

    for (int i = 0; rcKeyEvent(i, &e); i++)
        if (e.key == key)
            rcKeyEventConsume(i);
}

/* The keys, each with no modifier and only while no field is taking them: Cmd+C
   is a copy, and a j typed in the search field is a j. A key that opens a field
   asks here and is granted in layout, right after the field is declared, and is
   consumed (consume_key), so its own / or r or c is not typed into the field it
   opens. */
static void shortcuts(AppState *st, bool keysFree, double now) {
    const int i = st->selIssue;

    if (!keysFree) return;
    if (rcKeyPressed(RC_KEY_SLASH)) {
        if (rcModDown(RC_MOD_SHIFT)) {
            st->helpOpen = true;
        } else {
            go_page(st, PAGE_ISSUES);
            st->focusQuery = true;
            consume_key(RC_KEY_SLASH);
        }
    }
    if (rcKeyPressed(RC_KEY_C)) {
        st->composing = st->composeFresh = true;
        consume_key(RC_KEY_C);
    }
    if (st->page != PAGE_ISSUES) {
        if (rcKeyPressed(RC_KEY_ESCAPE)) go_page(st, PAGE_ISSUES);
        return;
    }
    if (rcKeyPressed(RC_KEY_X) && st->selRow >= 0 && !is_compact()) toggle_pick(st, st->selRow);
    if (rcKeyPressed(RC_KEY_E) && i >= 0) {
        set_open(st, i, !st->issue[i].open, now);
        st->stayAt = true;
    }
    if (rcKeyPressed(RC_KEY_R) && i >= 0 && (two_panes(st) || st->detailOpen)) {
        st->preview    = false;
        st->focusReply = true;
        consume_key(RC_KEY_R);
    }
    if (rcKeyPressed(RC_KEY_P)) toggle_split(st);
    /* A screenful, or the ends, where j and k would take a long hold. */
    if (two_panes(st) || !st->detailOpen) {
        const int page = (int)(rcGetElementBox("list").height / (float)row_pitch(st));

        if (rcKeyPressed(RC_KEY_PAGE_DOWN)) move(st, page > 1 ? page : 1);
        if (rcKeyPressed(RC_KEY_PAGE_UP))   move(st, page > 1 ? -page : -1);
        if (rcKeyPressed(RC_KEY_HOME))      move(st, -st->count);
        if (rcKeyPressed(RC_KEY_END))       move(st, st->count);
    }
    if ((rcKeyPressed(RC_KEY_ENTER) || rcKeyPressed(RC_KEY_O)) && st->selRow >= 0)
        open_row(st, st->selRow);
    if (rcKeyPressed(RC_KEY_U) && st->detailOpen) {
        st->detailOpen = false;
        st->reveal = true;
    }
    /* Esc backs out one level: a pane, then the picks, then the query. An open menu, a dialog
       and a focused field each take the first Esc. It is read only where it backs out, so
       with nothing left Android's Back stays the system's and leaves the app. */
    const bool pane = st->detailOpen && !two_panes(st);
    if ((pane || st->picked || !filter_is_default(&st->f)) && rcKeyPressed(RC_KEY_ESCAPE)) {
        if (pane)            { st->detailOpen = false; st->reveal = true; }
        else if (st->picked) pick_all(st, false);
        else                 clear_filters(st);
    }
}

static void update(RC_App *app, void *userData) {
    AppState    *st   = (AppState *)userData;
    const double now  = rcAppTime(app);
    const bool   typing = rcIsFocused("q") || rcIsFocused("reply") || rcIsFocused("labelq") ||
                          rcIsFocused("ntitle") || rcIsFocused("nbody");
    const bool   dialog = st->composing || st->helpOpen;
    const bool   chord  = rcModDown(RC_MOD_PRIMARY) || rcModDown(RC_MOD_CTRL) ||
                          rcModDown(RC_MOD_ALT) || rcModDown(RC_MOD_SUPER);
    const bool   keysFree = !typing && !dialog && !chord;

    /* THE WINDOW FIRST, as a web page paints before its fetch answers: the first frame shows
       the chrome, and this, on the next, makes the 100,000 issues (where a real app reads its
       file or database), so the backlog never holds the window back. The rebuild below runs
       on the same frame, so no frame shows a list or a count that is not the real one. */
    if (!st->seeded && st->shown) {
        seed(st);
        st->seeded = true;
        st->dirty  = true;
    }
    /* The theme flips HERE, before the layout, so no frame is drawn half in one
       palette. */
    if (st->flipTheme) {
        st->dark      = !st->dark;
        st->flipTheme = false;
        ui = app_palette(st->dark);
        rcSetStyle(app_style(st->dark));
        app_classes(&ui);
        rcWindowSetClearColor(rcAppMainWindow(app), ui.canvas);
    }
    /* Cmd+Enter submits, on the web and on Windows as Ctrl+Enter: a text area keeps
       that chord free for exactly this. */
    if (rcKeyPressed(RC_KEY_ENTER) && rcModDown(RC_MOD_PRIMARY)) {
        if (rcIsFocused("reply") && st->selIssue >= 0) add_comment(st, st->selIssue, now);
        else if (st->composing && !blank(st->draftTitle)) file_issue(st);
    }
    fly(app, st, keysFree && st->page == PAGE_ISSUES);
    shortcuts(st, keysFree, now);
    /* A rebuild changes the list's length, and a scroll call acts on the list as it
       was last laid out, so the reveal waits one frame for the new layout. */
    if (st->dirty)       rebuild(st);
    else if (st->reveal) reveal_selected(st);
    /* The clear row moves everything under it, so it waits while the Labels menu is
       open: the next pick must land where the last one did. */
    if (!st->labelsOpen) st->showClear = !filter_is_default(&st->f);
    if (st->pageTop) {
        if (rcGetScrollInfo("detail").found) rcScrollToTop("detail");
        st->pageTop = false;
    }
}

static void layout(RC_App *app, void *userData) {
    AppState     *st   = (AppState *)userData;
    RC_Arena     *m    = rcAppArena(app);
    RC_Viewport   view = rcViewport();
    const double  now  = rcAppTime(app);
    const bool    compact  = is_compact();
    const float   width    = usable_width();
    /* The band is unzoomed chrome, so both of its tests read window px. */
    const float   zoom     = rcWindowZoom(rcAppMainWindow(app));
    const float   bandW    = width * zoom;
    const bool    tabsBand = view.height * zoom < (float)SHORT_H && bandW >= (float)BAND_TABS_W;
    /* Assigned inside the root, so the scrollbars below read what was built. */
    bool showList = false, showIssue = false;

    st->mdLinks = 0;
    /* Open only while a frame declares that menu: the bulk bar that replaces it once a row is
       picked, and the other pages, declare none, and a mark left set would freeze the clear row. */
    st->labelsOpen = false;
    /* The bands run to the screen's edges and pad their own content by the safe
       insets; the pages keep the side insets in a column of their own. */
    rcColumn(.id = "root", .bg = ui.canvas, .pb = (float)(compact ? 0.0f : view.safe.bottom),
             .w = "grow", .h = "grow") {
        titlebar(m, st, tabsBand, bandW);
        if (!tabsBand) {
            rcRow(.id = "tabs", .bg = ui.subtle, .gap = 4,
                  .pl = (float)(compact ? 8 : 16) + view.safe.left,
                  .pr = (float)(compact ? 8 : 16) + view.safe.right,
                  .align = "cl", .scroll = "h", .w = "grow", .hType = RC_PX(TABS_H)) {
                tabs(m, st, compact);
                rcSpacer() {}
                if (st->page == PAGE_ISSUES && !compact && width >= (float)SPLIT_MIN &&
                    icon_button("split", rcIconPanelRight, "Show or hide the issue pane (p)",
                                st->split, false))
                    toggle_split(st);
            }
            rcScrollbar("tabs");   /* a strip wider than a phone shows that it slides */
        }
        hrule();
        rcColumn(.pl = view.safe.left, .pr = view.safe.right, .w = "grow", .h = "grow") {
            if (!st->seeded) {
                rcBox(.w = "grow", .h = "grow") {}
            } else if (st->page == PAGE_MILESTONES) {
                milestones_page(m, st, compact);
            } else if (st->page == PAGE_LABELS) {
                labels_page(m, st, compact);
            } else if (st->page == PAGE_PULSE) {
                pulse_page(m, st, compact);
            } else if (two_panes(st)) {
                showList = showIssue = true;
                /* The list keeps the room its rows need, and the page takes the rest. */
                if (rcBeginSplitPane("panes", RC_SPLIT_ROW, &st->splitAt, .minFraction = 0.34f,
                                     .maxFraction = 0.66f, .handleThickness = 2.0f)) {
                    list_pane(m, st, now);
                    rcSplitHandle();
                    issue_page(m, st, width * (1.0f - st->splitAt) - 2.0f, false, now);
                    rcEndSplitPane();
                }
            } else if (st->detailOpen) {
                showIssue = true;
                issue_page(m, st, width, true, now);
            } else {
                showList = true;
                list_pane(m, st, now);
            }
        }
    }
    new_issue_dialog(m, st);
    help_dialog(st);

    /* A scroll container gets no bar unless you ask for one. Declared outside the
       root so they float over it, and each carries the condition its pane carries:
       the panes take turns, and a call naming a container this frame did not build
       is what the library cannot tell from a misspelled id. */
    if (st->page != PAGE_ISSUES) rcScrollbar("pagebody");
    if (showList)  rcScrollbar("list");
    if (showIssue) rcScrollbar("detail");

    /* update() runs before layout(), so a control changed here is serviced on the
       NEXT frame, and this app is RC_RENDER_ON_DEMAND, so ask for it. One request
       at the end covers every site, the issues' first frame among them. */
    st->shown = true;
    if (!st->seeded || st->dirty || (st->reveal && showList) || st->flipTheme ||
        (!st->labelsOpen && st->showClear != !filter_is_default(&st->f)))
        rcWindowRequestFrame(rcAppMainWindow(app));
}

int main(void) {
    /* Slot 0 is the size every library control draws its label in: 14, Primer's. */
    static const float BODY_SIZE[1] = { 14.0f };
    static AppState state;
    int i;

    /* The default query and the theme before the first frame; the data after it (update). */
    state.selIssue = state.selRow = NONE;
    state.expanded = NONE;
    state.split    = true;
    state.splitAt  = 0.46f;
    state.draftWho = state.draftMilestone = 0;
    state.whoItems[0] = "No one";
    for (i = 0; i < PEOPLE; i++) state.whoItems[i + 1] = HANDLE[i];
    state.milestoneItems[0] = "No milestone";
    for (i = 0; i < MILESTONES; i++) state.milestoneItems[i + 1] = MILESTONE[i].name;
    for (i = 0; i < WEEKS - 1; i++) {
        const size_t at = rcFormatIntTo(state.weekText[i], 12, WEEKS - 1 - i, "");

        cat(state.weekText[i], 12, (int)at, "w ago");
        state.weekLabel[i] = state.weekText[i];
    }
    state.weekLabel[WEEKS - 1] = "This week";
    clear_filters(&state);
    ui = app_palette(false);
    rcSetStyle(app_style(false));
    app_classes(&ui);

    RC_AppOptions opts = {
        .width = 1280, .height = 820, .title = "RayClay Issue Inbox",
        .clearColor = ui.canvas,
        .fontSizes = BODY_SIZE, .fontCount = 1,
        .scratchArenaBytes = 512 * 1024,     /* backs every rcFormat in a frame  */
        .frame = RC_WINDOW_FRAME_NONE,
        .updateCallback = update, .layoutCallback = layout, .userData = &state,
        .titlebar = { .height = BAR_H },     /* the band above IS the titlebar   */
        .renderMode = RC_RENDER_ON_DEMAND,   /* parks between keystrokes         */
    };
    return rcRunApp(&opts);
}
