/*
    app.h - the whole UI: state, the navigation model, and the two shells

    A bottom tab bar, and under each tab a list that pushes a detail with a back
    control. Past WIDE_W the SAME source lays the tabs out as a left rail with the
    list and the detail side by side - same ids, same texts, same state.

    FOUR TABS, FOUR MODELS, AND THE COLOUR SAYS WHICH. Each tab narrows a
    different kind of thing and carries its own chips and its own row shape. The
    accent belongs to the tab you are on, RC_Style.primary included.

    screens.h is included mid-file, because a screen takes AppState *.
*/
#ifndef APP_APP_H
#define APP_APP_H

#include "rayclay.h"
#include "app/theme.h"
#include "platform/platform.h"

#include "icons/rc_icons_house.h"
#include "icons/rc_icons_search.h"
#include "icons/rc_icons_bell.h"
#include "icons/rc_icons_user.h"
#include "icons/rc_icons_arrow_left.h"
#include "icons/rc_icons_chevron_right.h"
#include "icons/rc_icons_x.h"
#include "icons/rc_icons_rayclay_logo.h"

/* The data: deterministic and synthetic, seeded once at startup. A record holds
   INDICES into the word tables, never strings, and the text is composed where it
   is drawn, which is what keeps 300 posts at 2.4 KB. */
enum { POSTS = 300, ITEMS = 120, ALERTS = 40, QUERY_CAP = 32, NAME_CAP = 32 };

static const char *const WHO[] = {
    "Ada", "Grace", "Linus", "Ken", "Dennis", "Margaret", "Barbara", "Alan", "Radia", "Hedy",
    "Tim", "Yukihiro",
};
static const char *const VERB[] = {
    "Shipping", "Measuring", "Rewriting", "Profiling", "Sketching", "Reviewing", "Porting",
    "Benchmarking",
};
/* A post's SUBJECT is a noun for its title, the lede its feed row shows, and a
   paragraph for its body, in one row so none can go missing. A post's body is
   three of these: the one its title names, then two the seed chooses. */
typedef struct {
    const char *noun;
    const char *lede;
    const char *body;
} Subject;

static const Subject SUBJECT[] = {
    { "the tab bar",
      "Four items, and the cell is the target.",
      "The tab bar is the one piece of chrome a phone app never scrolls away. Four items is "
      "the ceiling we set ourselves: past that the labels shrink below what a thumb can read, "
      "and the fifth becomes a More screen nobody opens. Each cell is a quarter of the width, "
      "and the whole cell is the target, not the glyph in it." },
    { "a virtual list",
      "Twelve rows describe three hundred.",
      "A feed of three hundred posts declares about a dozen rows. The list only describes the "
      "viewport, so the cost of scrolling is the cost of the rows that entered it, and the rows "
      "that left are retired by their ids. The one number that has to be exact is the pitch: "
      "every row is precisely that tall, or the arithmetic drifts and a tap lands on the wrong "
      "post." },
    { "the safe area",
      "Spent once, by the band that sits on it.",
      "The status bar and the home indicator sit on top of the window, and nothing moves the "
      "content out from under them. We spend the insets once, by whichever band sits on them: "
      "the header takes the top, the bar takes the bottom, and both paint their surface to the "
      "edge so the system chrome reads as part of the app rather than as a stripe over it." },
    { "one source",
      "Desktop, browser, iOS and Android.",
      "There is one C file for the desktop, the browser, iOS and Android, and it branches on "
      "the space it has rather than on the operating system. A desktop window dragged down to "
      "a phone's width gets the phone shell for the same reason a phone does, which is what "
      "lets a layout be checked on a workstation before a device ever sees it." },
    { "the soft keyboard",
      "Back closes it before it pops.",
      "When a field takes focus on a phone the keyboard covers the bottom third of the screen. "
      "The rule we settled on is that back closes the keyboard before it does anything else, "
      "and a field that scrolls out from under the keyboard is a defect, not a quirk to live "
      "with." },
    { "the scroll offset",
      "It belongs to the tab, not the list.",
      "Each tab keeps its own scroll position while you are somewhere else, so a reader who "
      "leaves Home halfway down the feed and comes back finds the row they left rather than "
      "the top. The offset is held per tab and handed back to the container by its id, which "
      "is what lets Home sit deep in the feed while Search is still at the top of its own." },
    { "a back stack",
      "One stack per tab, and it is kept.",
      "Each tab owns its own stack, and switching tabs never pops it. Home can be halfway "
      "through a post while Search is on its list, and tapping Home shows the post again. "
      "Tapping the tab that is already lit is the one gesture that pops to the root, which is "
      "the rule every platform's guidelines agree on." },
    { "the wide arm",
      "Nine hundred units, then two panes.",
      "Above nine hundred units the same screens sit side by side: the tabs become a rail, the "
      "list keeps its column, and the detail takes what is left. Nothing is added there that "
      "the phone lacks. A tablet is a presentation of the phone app, not a second app with more "
      "in it, and a gate lays both out to prove it." },
    { "an icon set",
      "Stroke paths, checked against the SVG.",
      "Every glyph on the bar is a stroke path from Lucide, generated into a header the build "
      "checks byte for byte against the SVG it came from. The icon is drawn at whatever size "
      "the layout asks for, so the same set serves an 18 pixel chevron in a row and a 40 pixel "
      "placeholder in the empty pane." },
    { "the render loop",
      "It draws on a change and parks.",
      "The app draws when something changed and parks otherwise. A tap, a keystroke or a scroll "
      "wakes it; a frame that would be identical to the last one is never rendered, which is "
      "what keeps a phone's battery out of the conversation. When the app changes its own state "
      "from inside a frame it asks for one more, and that request is the whole scheduling "
      "story." },
};
static const char *const ADJ[] = {
    "Quiet", "Copper", "Nimble", "Amber", "Cedar", "Velvet", "Granite", "Ivory", "Cobalt",
    "Willow", "Saffron", "Slate",
};
static const char *const ITEM_NOUN[] = {
    "Lantern", "Compass", "Kettle", "Journal", "Satchel", "Anchor", "Prism", "Beacon", "Ledger",
    "Tiller",
};
static const char *const CATEGORY[] = { "Tools", "Home", "Travel", "Office", "Outdoor" };
static const char *const TAG[] = {
    "new", "popular", "sale", "handmade", "limited", "classic", "compact", "gift",
};
/* An alert's severity belongs to the KIND of alert, never to a draw: a finished
   backup is never critical and a red main never merely informational. */
enum { SEV_INFO = 0, SEV_WARNING, SEV_CRITICAL };
static const char *const SEVERITY[] = { "Info", "Warning", "Critical" };

typedef struct {
    const char *text;
    uint8_t     severity;
} AlertKind;

static const AlertKind ALERT_KIND[] = {
    { "Build on main went red",          SEV_CRITICAL },
    { "Nightly soak finished",           SEV_INFO     },
    { "Runner disk is 90 percent full",  SEV_WARNING  },
    { "A reviewer approved your change", SEV_INFO     },
    { "Certificate expires in 7 days",   SEV_WARNING  },
    { "Frame time exceeded its budget",  SEV_WARNING  },
    { "New device enrolled: Pixel 8a",   SEV_INFO     },
    { "Backup completed",                SEV_INFO     },
    { "Login from a new location",       SEV_CRITICAL },
    { "Release " RC_VERSION " tagged",   SEV_INFO     },
};
#define N(table) ((int)(sizeof (table) / sizeof (table)[0]))

typedef struct {
    uint8_t  verb, noun, who, tint;
    uint16_t age;        /* minutes; the feed is newest first                  */
    uint16_t seed;       /* the body paragraphs are drawn from this            */
    bool     saved;      /* the Save switch on the detail, and a feed filter    */
    bool     read;       /* set by opening it; the unread mark reads this       */
} Post;

typedef struct {
    uint8_t  adj, noun, cat, tag[3];
    uint16_t grams, addedDays;
} Item;

typedef struct {
    uint8_t  text, severity;
    uint16_t minutesAgo; /* newest first, like the feed                        */
    bool     unread, archived;
} Alert;

/* THE NAVIGATION MODEL. Each tab owns a small stack: `depth` 0 is its list, 1 a
   detail pushed over it, `item` what was pushed. Switching tabs never pops.
   RAYCLAY DROPS A SCROLL CONTAINER'S OFFSET once it stops being declared, and a
   tab shell declares one tab at a time, so every list would come back at the top.
   `listY` holds it and `justBack` marks the one frame it must not be read. */
typedef enum { TAB_HOME = 0, TAB_SEARCH, TAB_ALERTS, TAB_PROFILE, TAB_COUNT } Tab;

/* WHAT EACH TAB IS FILTERING, one enum per navigation model, because "four tabs
   over one list" is what this example must not be. Each has a strip of chips at
   the top of its list that both SHOWS the setting and sets it. */
enum { FEED_ALL = 0, FEED_UNREAD, FEED_SAVED, FEED_FILTERS };
static const char *const FEED_NAME[FEED_FILTERS] = { "All", "Unread", "Saved" };
static const char *const FEED_ID[FEED_FILTERS]   = { "feed_all", "feed_unread", "feed_saved" };
static const char *const BOX_NAME[]              = { "Inbox", "Archived" };
static const char *const BOX_ID[]                = { "alerts_inbox", "alerts_archived" };

typedef struct {
    int     depth;    /* 0 = the list, 1 = a pushed detail                     */
    int     item;     /* the pushed index into this tab's data                 */
    float   listY;    /* the list's scroll offset the last frame it was live   */
    bool    justBack; /* the list was re-declared THIS frame after an absence  */
    bool    listLive; /* the list was declared last frame                      */
} NavStack;

/* Everything the app knows, in one struct main.c owns as a static. `match` and
   `feed` are permutations - indices that pass a filter, in order, so no record
   is ever copied. */
typedef struct {
    Post     post[POSTS];
    Item     item[ITEMS];
    Alert    alert[ALERTS];
    int      match[ITEMS];
    int      matchCount;
    int      feed[POSTS];      /* the Home filter's permutation, like match     */
    int      feedCount;
    int      feedFilter;       /* FEED_ALL | FEED_UNREAD | FEED_SAVED           */
    int      category;         /* 0 = every category, else CATEGORY index + 1   */
    bool     showArchived;     /* the Alerts tab is showing the archived box    */
    char     query[QUERY_CAP];
    char     name[NAME_CAP];
    bool     compactRows;
    int      theme;       /* index into THEME_NAME: 0 dark, 1 light            */
    float    textSize;    /* body px, TEXT_MIN..TEXT_MAX, from the slider      */
    Tab      tab;
    NavStack nav[TAB_COUNT];
} AppState;

static const char *const THEME_NAME[] = { "Dark", "Light" };

/* The four tabs, as tables keyed by Tab so every screen and both shells spell an
   id exactly once, and the same id serves both arms. */
static const char *const     TAB_TITLE[TAB_COUNT] = { "Home", "Search", "Alerts", "Profile" };
static const char *const     TAB_ID[TAB_COUNT]    = { "tab_home", "tab_search", "tab_alerts", "tab_profile" };
static const char *const     LIST_ID[TAB_COUNT]   = { "home_list", "search_list", "alerts_list", "profile_list" };
static const char *const     DETAIL_ID[TAB_COUNT] = { "home_detail", "search_detail", "alerts_detail",
                                                      "profile_detail" };
static const RC_IconCallback TAB_ICON[TAB_COUNT]  = { rcIconHouse, rcIconSearch, rcIconBell, rcIconUser };

/* Numerical Recipes' LCG. The high bits are the good ones, hence the shift. */
static inline uint32_t nxt(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s >> 8;
}

static inline void seed(AppState *st, uint32_t s)
{
    int i;

    /* A TITLE IS A WALK, NOT A DRAW. The steps are coprime with their tables, so
       eight consecutive posts carry eight different verbs and ten carry ten
       different subjects: no screenful of the feed repeats a title, or the lede
       the subject decides. A uniform draw collides on nearly every screen and
       reads as broken placeholder data. */
    for (i = 0; i < POSTS; i++) {
        Post *p = &st->post[i];
        p->verb = (uint8_t)((i * 3) % N(VERB));
        p->noun = (uint8_t)((i * 7) % N(SUBJECT));
        p->who  = (uint8_t)(nxt(&s) % (unsigned)N(WHO));
        p->tint = (uint8_t)(p->who % AVATAR_TINT_COUNT);   /* one author, one colour */
        p->age  = (uint16_t)(3u + (unsigned)i * 43u + nxt(&s) % 40u);
        p->seed = (uint16_t)(nxt(&s) & 0xFFFFu);
        /* The newest few are unread, the rest two thirds read, one in eight
           saved, so the first screen shows every state rather than one. */
        p->read  = i >= 2 && nxt(&s) % 100u < 64u;
        p->saved = nxt(&s) % 100u < 13u;
    }
    /* 12 adjectives x 10 objects = exactly 120 distinct names, walked so BOTH
       WORDS TURN OVER ON EVERY ROW and no pair repeats. A random draw would
       collide and leave the Search count lying about what it filtered. */
    for (i = 0; i < ITEMS; i++) {
        Item *it = &st->item[i];
        it->noun      = (uint8_t)(i % N(ITEM_NOUN));
        it->adj       = (uint8_t)((i / N(ITEM_NOUN) + i) % N(ADJ));
        it->cat       = (uint8_t)(nxt(&s) % (unsigned)N(CATEGORY));
        it->tag[0]    = (uint8_t)(nxt(&s) % (unsigned)N(TAG));
        it->tag[1]    = (uint8_t)((it->tag[0] + 1u + nxt(&s) % 6u) % (unsigned)N(TAG));
        it->tag[2]    = (uint8_t)((it->tag[1] + 1u + nxt(&s) % 5u) % (unsigned)N(TAG));
        it->grams     = (uint16_t)(120u + nxt(&s) % 2400u);
        it->addedDays = (uint16_t)(1u + nxt(&s) % 400u);
    }
    /* THE KIND IS A WALK TOO, and for the same reason: seven is coprime with
       ten, so every run of ten rows carries all ten kinds and the run's own
       index rotates where each one falls. */
    for (i = 0; i < ALERTS; i++) {
        Alert *a = &st->alert[i];
        a->text       = (uint8_t)((i * 7 + i / N(ALERT_KIND)) % N(ALERT_KIND));
        a->severity   = ALERT_KIND[a->text].severity;
        a->minutesAgo = (uint16_t)(12u + (unsigned)i * 95u + nxt(&s) % 30u);
        a->unread     = i < 6 || nxt(&s) % 100u < 20u;              /* the newest are unread */
        a->archived   = i >= 28 && nxt(&s) % 100u < 45u;            /* both boxes have rows  */
    }
}

/* rcText DOES NOT COPY. A string composed into a stack buffer draws blank with
   no log once the buffer is gone, so every runtime string goes through rcFormat
   on the frame arena, which lives exactly as long as the frame. */
static inline RC_String post_title(RC_Arena *m, const Post *p)
{
    return rcFormat(m, "%s %s", VERB[p->verb], SUBJECT[p->noun].noun);
}

static inline RC_String item_name(RC_Arena *m, const Item *it)
{
    return rcFormat(m, "%s %s", ADJ[it->adj], ITEM_NOUN[it->noun]);
}

static inline RC_String age_text(RC_Arena *m, unsigned minutes)
{
    if (minutes < 60u)   return rcFormat(m, "%um", minutes);
    if (minutes < 1440u) return rcFormat(m, "%uh", minutes / 60u);
    return rcFormat(m, "%ud", minutes / 1440u);
}

/* A wall clock the data is dated against, so "Today", "Yesterday" and "N days
   ago" fall out of one subtraction. A real app would read rcUnixTimeSeconds();
   a demo whose pictures must match on every machine cannot. */
enum { CLOCK_MIN = 16 * 60 + 40 };

static inline RC_String stamp_text(RC_Arena *m, unsigned minutesAgo)
{
    const unsigned now = (unsigned)CLOCK_MIN;
    unsigned t;

    if (minutesAgo <= now) {
        t = now - minutesAgo;
        return rcFormat(m, "Today %02u:%02u", t / 60u, t % 60u);
    }
    if (minutesAgo <= now + 1440u) {
        t = now + 1440u - minutesAgo;
        return rcFormat(m, "Yesterday %02u:%02u", t / 60u, t % 60u);
    }
    return rcFormat(m, "%u days ago", (minutesAgo - now) / 1440u + 1u);
}

/* Paragraph n of a post, 0..2. The first is the subject the title names; the
   other two are offsets from it chosen by the post's own seed, and the third's
   offset skips the second's, so all three differ by construction. */
static inline const char *paragraph(const Post *p, int n)
{
    const unsigned count = (unsigned)N(SUBJECT);
    uint32_t       s     = (uint32_t)p->seed * 2654435761u;
    unsigned       ob    = 1u + nxt(&s) % (count - 1u);      /* 1 .. count-1        */
    unsigned       oc    = 1u + nxt(&s) % (count - 2u);      /* 1 .. count-2, then  */
    unsigned       pick  = p->noun;

    if (oc >= ob) oc++;                                       /* ... past ob: 1 .. count-1, != ob */
    if (n == 1) pick = (p->noun + ob) % count;
    if (n == 2) pick = (p->noun + oc) % count;
    return SUBJECT[pick].body;
}

/* THE TAB'S OWN ACCENT, asked of the SETTING rather than inferred from a colour
   the app was handed, because the theme is a Profile control here. */
static inline RC_Color tab_accent(const AppState *st, Tab t)
{
    return st->theme ? TAB_ACCENT_LIGHT[t] : TAB_ACCENT_DARK[t];
}

/* A lighter or darker sibling of a colour. RC_Style wants two shades of its
   accent and a tab's accent here is one shade, so the second is derived. */
static inline float clamp255(float v)
{
    return v < 0.0f ? 0.0f : v > 255.0f ? 255.0f : v;
}

static inline RC_Color shade(RC_Color c, float delta)
{
    RC_Color out = { clamp255(c.r + delta), clamp255(c.g + delta), clamp255(c.b + delta), c.a };
    return out;
}

/* TWO HUES IN THE ALERTS TAB AND NO MORE: danger for critical, the tab's amber
   for a warning, nothing for information. The word is drawn beside the colour
   everywhere this is used, so the scale never rests on the hue alone. */
static inline RC_Color severity_color(const AppState *st, int sev)
{
    if (sev == SEV_CRITICAL) return rcGetStyle().danger;
    if (sev == SEV_WARNING)  return tab_accent(st, TAB_ALERTS);
    return rcGetStyle().textMuted;
}

/* The counts the strips and the badge show, read fresh every frame rather than
   cached into a field that a missed update could leave lying. */
static inline int unread_count(const AppState *st)
{
    int i, n = 0;
    for (i = 0; i < ALERTS; i++)
        if (st->alert[i].unread && !st->alert[i].archived) n++;
    return n;
}

static inline int box_count(const AppState *st, bool archived)
{
    int i, n = 0;
    for (i = 0; i < ALERTS; i++)
        if (st->alert[i].archived == archived) n++;
    return n;
}

static inline int post_count(const AppState *st, int filter)
{
    int i, n = 0;
    if (filter == FEED_ALL) return POSTS;
    for (i = 0; i < POSTS; i++)
        if (filter == FEED_SAVED ? st->post[i].saved : !st->post[i].read) n++;
    return n;
}

/* THE FEED'S FILTER IS A PERMUTATION, rebuilt ON A TAP and never on a frame:
   a row must not vanish from under the reader who just came back from it. The
   chips' counts are live, so the strip says what the next tap would give. */
static inline void rebuild_feed(AppState *st)
{
    int i;

    st->feedCount = 0;
    for (i = 0; i < POSTS; i++) {
        if (st->feedFilter == FEED_UNREAD && st->post[i].read)   continue;
        if (st->feedFilter == FEED_SAVED  && !st->post[i].saved) continue;
        st->feed[st->feedCount++] = i;
    }
}

/* The Search filter: a scan on a CHANGE, never on a frame. */
static inline char lower(char c) { return (char)(c >= 'A' && c <= 'Z' ? c + 32 : c); }

static inline bool has(const char *hay, const char *needle)
{
    int i, j;
    if (!needle[0]) return true;
    for (i = 0; hay[i]; i++) {
        for (j = 0; needle[j] && hay[i + j] && lower(hay[i + j]) == needle[j]; j++) {}
        if (!needle[j]) return true;
    }
    return false;
}

/* "a b" into a fixed buffer, so a query can span both words of a name. A plain
   buffer rather than rcFormat: this runs on a change, not on a frame. */
static inline void cat2(char *dst, int cap, const char *a, const char *b)
{
    int n = 0;
    for (; *a && n < cap - 1; a++) dst[n++] = *a;
    if (n < cap - 1) dst[n++] = ' ';
    for (; *b && n < cap - 1; b++) dst[n++] = *b;
    dst[n] = '\0';
}

static inline void rebuild_matches(AppState *st)
{
    char q[QUERY_CAP], name[48];
    int  i;

    for (i = 0; i < QUERY_CAP - 1 && st->query[i]; i++) q[i] = lower(st->query[i]);
    q[i] = '\0';
    st->matchCount = 0;
    for (i = 0; i < ITEMS; i++) {
        const Item *it = &st->item[i];
        /* TWO NARROWINGS, ONE SCAN: the typed query and the category chip
           compose here, so the count beside the field answers both at once. */
        if (st->category && (int)it->cat != st->category - 1) continue;
        cat2(name, (int)sizeof name, ADJ[it->adj], ITEM_NOUN[it->noun]);
        if (has(name, q)) st->match[st->matchCount++] = i;
    }
}

/* Push, or replace: at depth 1 a second push swaps the item under the same
   header, which is what "Next post" does. The reset is what stops item B opening
   halfway down item A when the pane is already live. */
static inline void push(AppState *st, Tab t, int item)
{
    st->nav[t].depth = 1;
    st->nav[t].item  = item;
    rcScrollToTop(DETAIL_ID[t]);
}

static inline void pop(AppState *st, Tab t)
{
    if (st->nav[t].depth > 0) st->nav[t].depth = 0;
}

/* The bar's rule, iOS's: tapping another tab switches; tapping the one that is
   already lit pops its stack to the root. */
static inline void select_tab(AppState *st, Tab t)
{
    if (st->tab == t) pop(st, t);
    else              st->tab = t;
}

/* THE BACK KEY. RC_KEY_ESCAPE is the desktop Escape and, on Android, the back
   button: pop the stack; at a root that is not the first tab, go to the first
   tab; at the first tab's root let the platform take it. A FOCUSED TEXT FIELD
   COMES FIRST - RayClay does not unfocus on Escape, so the app does. */
static inline void back(AppState *st)
{
    if (rcIsFocused("search_q") || rcIsFocused("profile_name")) {
        rcSetFocus(NULL);
        return;
    }
    if (st->nav[st->tab].depth > 0) pop(st, st->tab);
    else if (st->tab != TAB_HOME)   st->tab = TAB_HOME;
}

static inline void mark_all_read(AppState *st)
{
    int i;
    for (i = 0; i < ALERTS; i++) st->alert[i].unread = false;
}

/* 48 until a finger has been seen, 56 after: rcPointerIsCoarse latches on the
   first real touch, so a false answer means "no finger yet", never "a mouse". */
static inline int hit_size(void)
{
    return rcPointerIsCoarse() ? HIT_COARSE : HIT_FINE;
}

/* Body text at the size the Profile slider chose. RC_TextOptions.size scales the
   F_BODY slot's baked glyphs, so one number moves every paragraph and row title
   without a re-bake. */
static inline uint16_t body_px(const AppState *st)
{
    return (uint16_t)(st->textSize + 0.5f);
}

/* The title the header shows at depth 1: the pushed item's own name. */
static inline RC_String stack_title(RC_Arena *m, const AppState *st, Tab t)
{
    int item = st->nav[t].item;
    switch (t) {
    case TAB_HOME:    return post_title(m, &st->post[item]);
    case TAB_SEARCH:  return item_name(m, &st->item[item]);
    case TAB_ALERTS:  return rcStringFromCStr(ALERT_KIND[st->alert[item].text].text);
    case TAB_PROFILE: return rcStringFromCStr("About");
    case TAB_COUNT:   break;
    }
    return rcStringFromCStr("");
}

/* INK OR PAPER, ASKED OF THE FILL AND NEVER OF THE THEME. The avatar tints and
   the tab accents span half the luminance range, so one fixed label colour is
   unreadable on half of them - white on the yellow tint is 1.8:1. */
static inline RC_Color on_tint(RC_Color c)
{
    return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b >= 140.0f ? RC_ZINC_900 : RC_WHITE;
}

/* A ROW THAT HAS BEEN READ SOFTENS, BUT ITS TITLE STAYS A STEP ABOVE THE LINE
   UNDER IT: dropping it to the muted colour puts it on the same token as its own
   lede and meta, and the row collapses into three identical grey lines. */
static inline RC_Color title_ink(bool strong)
{
    return strong ? rcGetStyle().text : rcAlpha(rcGetStyle().text, 200);
}

/* A coloured disc with an initial in it: the avatar every feed row and the
   Profile card use. */
static inline void avatar(RC_Arena *m, RC_Color tint, char initial, int size)
{
    rcBox(.bg = tint, .align = "cc", .borderRadius = "all-full", .wType = RC_PX(size), .hType = RC_PX(size)) {
        rcText(rcFormat(m, "%c", initial), .font = (uint16_t)(size >= AVATAR ? F_HEAD : F_SMALL),
                .color = on_tint(tint));
    }
}

/* A small pill of text: a tag, a severity, a status. `.wrap = "n"` is load
   bearing, not taste: a pill is a FIT box, and a fit box's width comes from the
   text's longest WORD, so a two-word label wraps inside its own pill. */
static inline void chip(const char *text, RC_Color fg, RC_Color bg)
{
    rcBox(.bg = bg, .px = 9, .py = 3, .borderRadius = "all-full") {
        rcTextC(text, .font = F_MICRO, .color = fg, .wrap = "n");
    }
}

/* THE APP'S OWN FILLED ACTION, and why it is not rcButton: a filled button
   labels itself RC_WHITE over RC_Style.primary, and this app's accent can be
   LIGHT - amber, on the dark theme - which is white on yellow. */
static inline bool action(const char *id, const char *label, RC_Color accent, int height)
{
    rcBox(.id = id, .bg = rcAlpha(accent, rcIsHovered(id) ? 60 : 34), .px = 16, .align = "cc",
          .borderRadius = "all-full", .border = { .color = rcAlpha(accent, 150), .width = "1px" },
          .hType = RC_PX(height)) {
        rcTextC(label, .font = F_BODY, .color = accent, .wrap = "n");
    }
    return rcClicked(id);
}

/* THE MARK DOWN THE LEFT OF A ROW, a SHAPE before it is a colour: a short pill
   says unread, a full-height bar says this is the one the pane beside it has
   open. A read row keeps the column, so every title starts at the same x. */
static inline void row_mark(bool unread, bool open, RC_Color accent)
{
    rcBox(.bg = unread || open ? accent : RC_TRANSPARENT, .borderRadius = "all-full",
          .wType = RC_PX(MARK_W), .hType = open ? RC_GROW : RC_PX(MARK_H)) {}
}

/* ONE CHIP OF A FILTER STRIP, returning its own click. Chosen and not chosen
   differ in FILL and BORDER first and in hue second.
   `grow` picks the SHAPE OF THE STRIP, not a taste: a strip that fits on ONE row
   stays fit-width and reads as a row of tags, while a strip that WRAPS must share
   its width equally or the second row's right edge lands somewhere the first
   row's did not, and six pills of six widths read as a lumpy cloud. */
static inline bool filter_chip(const char *id, RC_String label, bool on, RC_Color accent, bool grow)
{
    RC_Style s = rcGetStyle();

    rcBox(.id = id, .bg = on ? rcAlpha(accent, 38) : RC_TRANSPARENT, .px = 13, .align = "cc",
          .borderRadius = "all-full", .border = { .color = on ? accent : s.border, .width = "1px" },
          .wType = grow ? RC_GROW : RC_FIT, .hType = RC_PX(hit_size() - 10)) {
        rcText(label, .font = F_SMALL, .color = on ? accent : s.textMuted, .wrap = "n");
    }
    return rcClicked(id);
}

/* One attribute line of a detail screen: a muted label column and a value. */
static inline void field(const char *label, RC_String value)
{
    RC_Style s = rcGetStyle();
    rcRow(.gap = 12, .align = "cl", .w = "grow") {
        rcBox(.wType = RC_PX(84)) { rcTextC(label, .font = F_SMALL, .color = s.textMuted); }
        rcText(value, .font = F_BODY, .color = s.text);
    }
}

static inline void rule(void)
{
    rcBox(.bg = rcGetStyle().border, .w = "grow", .hType = RC_PX(1)) {}
}

/* THE MUTED LINE AT THE RIGHT OF A ROOT BAND: what this tab holds that the
   screen below does not already say. A large title with three hundred pixels of
   nothing beside it is the emptiest thing in a phone app, and the cure is a fact
   rather than an invented control - which is why two tabs keep a bare title. */
static inline RC_String header_note(RC_Arena *m, const AppState *st, Tab t)
{
    switch (t) {
    case TAB_HOME:
        return rcFormat(m, "Updated %02u:%02u", (unsigned)CLOCK_MIN / 60u, (unsigned)CLOCK_MIN % 60u);
    case TAB_ALERTS:
        return st->showArchived ? rcStringFromCStr("") : rcStringFromCStr("All caught up");
    case TAB_SEARCH:
    case TAB_PROFILE:
    case TAB_COUNT:
        break;
    }
    return rcStringFromCStr("");
}

/* THE HEADER BAND. `pushed` decides which of its two faces it wears: a tab's
   root shows the large title with its note or its one list action, a pushed
   screen the back arrow and the item's title. Both faces are one function.
   `band` is the part of the safe area this band sits on. Its SURFACE spans the
   full width and its CONTENT is padded in by the insets - a root that padded the
   sides instead leaves the window background showing beside it. */
static inline void header(RC_App *app, AppState *st, Tab t, bool pushed, RC_Insets band)
{
    RC_Style  s    = rcGetStyle();
    RC_Arena *m    = rcAppArena(app);
    int       hit  = hit_size();
    RC_String note = pushed ? rcStringFromCStr("") : header_note(m, st, t);

    rcRow(.bg = s.surface, .gap = 6, .pt = (uint16_t)band.top,
          .pl = (uint16_t)(band.left + (pushed ? 4.0f : (float)PAD)), .pr = (uint16_t)(band.right + 10.0f),
          .align = "cl", .w = "grow", .hType = RC_PX(HEADER_H + (int)band.top)) {
        if (pushed) {
            rcBox(.id = "back", .bg = rcAlpha(s.border, rcIsHovered("back") ? 120 : 0),
                  .align = "cc", .borderRadius = "all-full", .wType = RC_PX(hit), .hType = RC_PX(hit)) {
                rcIconArrowLeft((float)ICON, s.text);
            }
            if (rcClicked("back")) pop(st, t);
        }
        /* The title column grows and the text wraps inside it, so a long post
           title takes a second line rather than running under the action. */
        rcBox(.w = "grow") {
            if (pushed)
                rcText(stack_title(m, st, t), .font = F_HEAD, .color = s.text);
            else
                rcTextC(TAB_TITLE[t], .font = F_TITLE, .color = s.text);
        }
        /* The one action that belongs to a LIST rather than to an item, offered
           only where it means something. HIT_MIN rather than hit_size(), because
           a 56 px target fills a 56 px band edge to edge. */
        if (t == TAB_ALERTS && !pushed && !st->showArchived && unread_count(st) > 0) {
            if (action("alerts_read_all", "Mark all read", tab_accent(st, TAB_ALERTS), HIT_MIN))
                mark_all_read(st);
        } else if (note.length) {
            rcText(note, .font = F_SMALL, .color = s.textMuted, .wrap = "n");
        }
    }
    rule();
}

/* ONE TAB ITEM; the whole cell is the target.
   ONE 3 PX PILL CARRIES BOTH NAVIGATION STATES: the accent says this is the tab
   you are on, the line colour says that tab's stack has a screen open, an empty
   slot says a tab at its root. Neither rests on colour alone.
   The Alerts badge is anchored by its own TOP_LEFT so it clears the pill, and
   capture passthrough keeps it off the cell's tap. */
static inline void tab_item(RC_App *app, AppState *st, Tab t, bool inRail)
{
    RC_Style  s      = rcGetStyle();
    bool      on     = st->tab == t;
    bool      opened = st->nav[t].depth > 0;
    RC_Color  accent = tab_accent(st, t);
    RC_Color  fg     = on ? accent : s.textMuted;
    int       unread = unread_count(st);

    rcColumn(.id = TAB_ID[t], .gap = 4, .align = "cc", .w = "grow",
             .hType = inRail ? RC_PX(72) : RC_GROW) {
        rcBox(.bg = on ? accent : opened ? s.border : RC_TRANSPARENT, .borderRadius = "all-full",
              .wType = RC_PX(PILL_W), .hType = RC_PX(PILL_H)) {}
        rcBox(.align = "cc") {
            TAB_ICON[t]((float)ICON, fg);
            if (t == TAB_ALERTS && unread > 0)
                /* The count sits ON the accent, so its text takes the ground's
                   side: on_tint asks the fill. */
                rcBox(.bg = accent, .px = 5, .align = "cc", .borderRadius = "all-full",
                      .hType = RC_PX(BADGE_H), .wMin = (float)BADGE_H,
                      .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_RIGHT,
                                    .element = RC_ANCHOR_TOP_LEFT, .offset = { -7.0f, 0.0f },
                                    .capture = RC_CAPTURE_PASSTHROUGH }) {
                    rcText(rcFormat(rcAppArena(app), "%d", unread), .font = F_MICRO,
                            .color = on_tint(accent), .wrap = "n");
                }
        }
        rcTextC(TAB_TITLE[t], .font = F_MICRO, .color = fg);
    }
    if (rcClicked(TAB_ID[t])) select_tab(st, t);
}

/* The screens take AppState *, so this include sits here rather than at the
   top: a header included before the struct it draws would not compile. */
#include "app/screens.h"

/* Both shells dispatch the same way. No `default:` on purpose: with -Wswitch the
   compiler then names every switch a fifth tab was forgotten in. */
static inline void list_screen(RC_App *app, AppState *st, Tab t)
{
    switch (t) {
    case TAB_HOME:    home_list(app, st);    break;
    case TAB_SEARCH:  search_list(app, st);  break;
    case TAB_ALERTS:  alerts_list(app, st);  break;
    case TAB_PROFILE: profile_list(app, st); break;
    case TAB_COUNT:   break;
    }
}

static inline void detail_screen(RC_App *app, AppState *st, Tab t)
{
    switch (t) {
    case TAB_HOME:    home_detail(app, st);    break;
    case TAB_SEARCH:  search_detail(app, st);  break;
    case TAB_ALERTS:  alerts_detail(app, st);  break;
    case TAB_PROFILE: profile_detail(app, st); break;
    case TAB_COUNT:   break;
    }
}

/* THE PHONE SHELL: header, one screen, bar, and never a third band. Each band
   spends the safe area it sits on, so the status bar sits on the header's
   surface and the home indicator on the bar's. */
static inline void phone_shell(RC_App *app, AppState *st, RC_Insets safe)
{
    RC_Style s      = rcGetStyle();
    Tab      t      = st->tab;
    bool     pushed = st->nav[t].depth > 0;
    int      i;

    header(app, st, t, pushed, safe);
    rcColumn(.pl = (uint16_t)safe.left, .pr = (uint16_t)safe.right, .w = "grow", .h = "grow") {
        if (pushed) detail_screen(app, st, t);
        else        list_screen(app, st, t);
    }
    rule();
    rcRow(.id = "tabbar", .bg = s.surface, .pb = (uint16_t)safe.bottom, .pl = (uint16_t)safe.left,
          .pr = (uint16_t)safe.right, .w = "grow", .hType = RC_PX(BAR_H + (int)safe.bottom)) {
        for (i = 0; i < TAB_COUNT; i++) tab_item(app, st, (Tab)i, false);
    }
}

/* THE WIDE ARM: the four items become a rail, the list keeps its LIST_W column
   and the detail takes the rest. At depth 0 the pane says what the tab holds
   rather than guessing an item. */
static inline void wide_shell(RC_App *app, AppState *st)
{
    RC_Style  s      = rcGetStyle();
    Tab       t      = st->tab;
    bool      pushed = st->nav[t].depth > 0;
    RC_Insets none   = { 0.0f, 0.0f, 0.0f, 0.0f };   /* the root spent the bands */
    int       i;

    rcRow(.w = "grow", .h = "grow") {
        rcColumn(.id = "rail", .bg = s.surface, .gap = 4, .pt = 12, .h = "grow", .wType = RC_PX(RAIL_W)) {
            for (i = 0; i < TAB_COUNT; i++) tab_item(app, st, (Tab)i, true);
        }
        rcBox(.bg = s.border, .h = "grow", .wType = RC_PX(1)) {}
        rcColumn(.h = "grow", .wType = RC_PX(LIST_W)) {
            header(app, st, t, false, none);
            list_screen(app, st, t);
        }
        rcBox(.bg = s.border, .h = "grow", .wType = RC_PX(1)) {}
        rcColumn(.w = "grow", .h = "grow") {
            if (pushed) {
                header(app, st, t, true, none);
                detail_screen(app, st, t);
            } else {
                empty_pane(app, st, t);
            }
        }
    }
}

/* KEEPING A LIST'S SCROLL ACROSS ITS ABSENCE. The app reads the offset back
   every frame the list is live and hands it to `.scrollOffset` on the
   declaration that brings the list home. Both of the app's remaining jobs are
   about the ONE frame a list comes back on: the offset is applied at the
   FOLLOWING frame's open, so this frame reads zero and reading it back would
   overwrite what is being restored - and nothing asks the on-demand runner for
   that following frame, so we do. */
static inline void keep_list_scroll(RC_App *app, NavStack *nav, const char *id)
{
    if (nav->justBack) {
        rcWindowRequestFrame(rcAppMainWindow(app));
        return;
    }

    RC_ScrollInfo si = rcGetScrollInfo(id);

    if (si.found)
        nav->listY = si.offsetY;
}

static inline void update(RC_App *app, void *userData)
{
    AppState *st = (AppState *)userData;
    (void)app;
    if (rcKeyPressed(RC_KEY_ESCAPE)) back(st);
}

static inline void layout(RC_App *app, void *userData)
{
    AppState   *st = (AppState *)userData;
    RC_Viewport vp;
    RC_Insets   safe;
    RC_Style    s;
    bool        wide, listLive;
    Tab         t = st->tab;
    int         i;

    /* THE THEME GIVES THE SURFACES, THE TAB GIVES THE ACCENT. The theme is a
       Profile setting, applied at the top of every frame, and the clear colour
       has to follow it: a frame the runner holds back shows nothing else.
       Overriding `primary` before installing it carries one accent per navigation
       model into the LIBRARY'S widgets, down to every focus ring. */
    s = st->theme ? rcStyleLight() : rcStyleDark();
    s.primary      = tab_accent(st, st->tab);
    s.primaryHover = shade(s.primary, st->theme ? -24.0f : 24.0f);
    rcSetStyle(s);
    s = rcGetStyle();
    rcWindowSetClearColor(rcAppMainWindow(app), s.background);

    /* BRANCH ON THE SPACE, NEVER ON THE PLATFORM. rcViewport() reports layout
       units, so a desktop window dragged to 393 wide gets the phone shell for the
       same reason a phone does. `safe` is the same insets, in those units. */
    vp   = rcViewport();
    safe = vp.safe;
    wide = vp.width >= (float)WIDE_W;

    /* Which tab's list is on screen this frame, for the scroll keeper. A list
       that was not live last frame is flagged, because the frame it comes back on
       is the one frame its offset must not be read. */
    listLive = wide || st->nav[t].depth == 0;
    for (i = 0; i < TAB_COUNT; i++) {
        bool live = i == (int)t && listLive;
        st->nav[i].justBack = live && !st->nav[i].listLive;
        st->nav[i].listLive = live;
    }

    /* THE SAFE AREA IS SPENT ONCE, AND BY WHOEVER SITS ON IT. On the wide arm
       nothing does, so the root takes all four insets; on the phone shell the
       bands take their own, and a root that padded here would spend them twice. */
    rcColumn(.id = "root", .bg = s.background,
             .pt = (uint16_t)(wide ? safe.top : 0.0f), .pb = (uint16_t)(wide ? safe.bottom : 0.0f),
             .pl = (uint16_t)(wide ? safe.left : 0.0f), .pr = (uint16_t)(wide ? safe.right : 0.0f),
             .w = "grow", .h = "grow") {
        if (wide) wide_shell(app, st);
        else      phone_shell(app, st, safe);
    }

    if (listLive) keep_list_scroll(app, &st->nav[t], LIST_ID[t]);

    /* A scroll container gets no bar unless you ask for one. Declared after the
       container itself, which is the order rayclay.h requires. */
    if (listLive) rcScrollbar(LIST_ID[t]);
    if (st->nav[t].depth > 0) rcScrollbar(DETAIL_ID[t]);
}

#endif /* APP_APP_H */
