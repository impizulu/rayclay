/*
    board.h - the boards, lists and cards the app holds, in ordinary C++ values.

    A board keeps its lists and its cards in two flat vectors, each sorted by
    `pos`, a number that says where an item sits among its neighbors. Moving a card
    gives it a pos between the two cards it lands between and touches nothing else,
    which is also how the wire carries it: one PATCH per move, however long the
    list is. Nothing here draws. main.cpp includes the containers these headers use.
*/
#ifndef KANBAN_BOARD_H
#define KANBAN_BOARD_H

#include "rayclay.h"

/* A day is a count from Dec 31 2025, and 0 means no date. The sample boards below are
   written as if today were day 310, Friday Nov 6 2026; main.cpp reads the real today from
   the clock and moves every sample date by the difference (shift_days), so a card due
   tomorrow in the sample is due tomorrow on any day the app runs. */
constexpr int SAMPLE_DAY = 310;
constexpr int LAST_DAY = 27028;     /* Dec 31 2099: the calendar stops here */
constexpr int LAST_MONTH = 887;     /* the same month, counted in months from Jan 2026 */

/* The wire carries dates as RFC 3339, so a day converts through Unix seconds. */
constexpr double DAY_ONE_UNIX = 1767225600.0;   /* Jan 1 2026, 00:00 UTC */

static inline int day_of_unix(double seconds) {
    return static_cast<int>((seconds - DAY_ONE_UNIX) / 86400.0) + 1;
}

static inline double unix_of_day(int day) {
    return DAY_ONE_UNIX + 86400.0 * (day - 1);
}

static const int   MONTH_DAYS[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static const char *const MONTH_NAME[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

struct Date {
    int year, month, day;           /* month 0 to 11, day from 1 */
};

/* February has 29 days in a year divisible by 4, except a century not divisible by 400. */
static inline int month_days(int year, int month) {
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return month == 1 && leap ? 29 : MONTH_DAYS[month];
}

static inline Date date_of(int n) {
    Date d = { 2026, 0, n };
    while (d.day > month_days(d.year, d.month)) {
        d.day -= month_days(d.year, d.month);
        if (++d.month == 12) {
            d.month = 0;
            d.year++;
        }
    }
    return d;
}

/* The day number of the first of a month, counted in months from Jan 2026. */
static inline int first_of_month(int months) {
    int n = 1;
    for (int m = 0; m < months; m++)
        n += month_days(2026 + m / 12, m % 12);
    return n;
}

static inline int weekday_of(int n) {
    return (n + 3) % 7;             /* day 1, Jan 1 2026, is a Thursday; Sunday is 0 */
}

/* Six people work on every board; a card's `members` is a bit per person, and a comment's
   `author` is an index into MEMBERS. */
constexpr int MEMBER_COUNT = 6;
enum { MAYA = 1, DIEGO = 2, PRIYA = 4, TOM = 8, ANA = 16, KENJI = 32 };
enum { BY_MAYA, BY_DIEGO, BY_PRIYA, BY_TOM, BY_ANA, BY_KENJI };

struct Member {
    const char *name, *initials;
    RC_Color    color;
};

static const Member MEMBERS[MEMBER_COUNT] = {
    { "Maya Chen",   "MC", rcHex(0x0052CC) },
    { "Diego Ramos", "DR", rcHex(0x00875A) },
    { "Priya Nair",  "PN", rcHex(0x8B3FB0) },
    { "Tom Becker",  "TB", rcHex(0xC9372C) },
    { "Ana Silva",   "AS", rcHex(0xB65C02) },
    { "Kenji Ito",   "KI", rcHex(0x206A83) },
};

/* The app's owner: "my cards" and a new comment both mean this person. */
constexpr int ME = BY_MAYA;

/* Six label colors, Trello's: dark ink reads on all of them. A card's `labels` is
   a bit per color, and each board names the colors its own way. */
constexpr int LABEL_COUNT = 6;
enum { GREEN = 1, YELLOW = 2, ORANGE = 4, RED = 8, PURPLE = 16, BLUE = 32 };

static const RC_Color LABEL_COLOR[LABEL_COUNT] = {
    rcHex(0x4BCE97), rcHex(0xF5CD47), rcHex(0xFEA362),
    rcHex(0xF87168), rcHex(0x9F8FEF), rcHex(0x579DFF),
};

/* Linear's five priorities, drawn as signal bars; urgent is the red one. */
constexpr int PRIORITY_COUNT = 5;
static const char *const PRIORITY_NAME[PRIORITY_COUNT] = { "No priority", "Low", "Medium",
                                                           "High", "Urgent" };

/* A checklist item's key names its row on screen, as a card's id names its card: a new item
   takes the next one, a copy keeps it, so a row's focus stays with its item when one above
   it goes. It lives in the app only and never crosses the wire. */
static inline int next_check_key() {
    static int last = 0;
    return ++last;
}

struct CheckItem {
    std::string text;
    bool        done = false;
    int         key = next_check_key();
};

/* Where the item with @p key sits in a checklist, or -1 when it has gone. */
static inline int item_at(const std::vector<CheckItem> &items, int key) {
    for (size_t i = 0; i < items.size(); i++)
        if (items[i].key == key)
            return static_cast<int>(i);
    return -1;
}

struct Comment {
    int         author = ME;
    std::string text;
    int         day = 0;
};

struct Card {
    int         id = 0;         /* never reused; below 0 while a create is in flight */
    int         list = 0;
    double      pos = 0.0;
    std::string title;
    std::string description;
    unsigned    labels = 0;
    unsigned    members = 0;
    int         due = 0;
    bool        dueDone = false;
    int         priority = 0;
    int         cover = -1;     /* an index into LABEL_COLOR, or -1 for none */
    std::vector<CheckItem> checklist;
    std::vector<Comment>   comments;
    std::string meta;           /* a JSON object only the server reads: sync.h keeps it */
    long long   version = 0;    /* the server's version at its last change; 0 = local */
};

struct List {
    int         id = 0;
    double      pos = 0.0;
    std::string title;
    long long   version = 0;
};

struct Board {
    std::string name;
    const char *prefix = "";    /* the "AT" in a card's AT-12 */
    RC_Color    from, to;       /* the background's gradient */
    bool        starred = false;
    const char *labelName[LABEL_COUNT] = {};
    std::vector<List> lists;
    std::vector<Card> cards;
    int         nextCard = 1;
    int         nextList = 1;
};

/* ---- Finding and placing ---- */

static inline Card *find_card(Board &b, int id) {
    for (Card &c : b.cards)
        if (c.id == id)
            return &c;
    return nullptr;
}

static inline List *find_list(Board &b, int id) {
    for (List &l : b.lists)
        if (l.id == id)
            return &l;
    return nullptr;
}

static inline int cards_in(const Board &b, int list) {
    int n = 0;
    for (const Card &c : b.cards)
        n += c.list == list;
    return n;
}

/* Insert keeping the vector sorted by pos: the one place a card enters a board. */
static inline Card &place_card(Board &b, Card card) {
    auto at = b.cards.begin();
    while (at != b.cards.end() && at->pos <= card.pos)
        ++at;
    return *b.cards.insert(at, std::move(card));
}

static inline List &place_list(Board &b, List list) {
    auto at = b.lists.begin();
    while (at != b.lists.end() && at->pos <= list.pos)
        ++at;
    return *b.lists.insert(at, std::move(list));
}

static inline void remove_card(Board &b, int id) {
    for (auto at = b.cards.begin(); at != b.cards.end(); ++at) {
        if (at->id == id) {
            b.cards.erase(at);
            return;
        }
    }
}

static inline void remove_list(Board &b, int id) {
    for (auto at = b.lists.begin(); at != b.lists.end(); ++at) {
        if (at->id == id) {
            b.lists.erase(at);
            return;
        }
    }
}

/* The pos a card takes at `slot` in a list, counting that list's cards without
   the card being moved: halfway between its new neighbors, or one step past the
   last. @p fits, when given, says whether that pos lies strictly between them: each
   move into one gap halves it, so about fifty moves between the same two cards use up
   a double, and a pos equal to a neighbor's sorts the card to the wrong side of it. */
static inline double pos_for_slot(const Board &b, int list, int slot, int moving,
                                  bool *fits = nullptr) {
    const double STEP = 1024.0;
    double before = 0.0;
    int    k = 0;
    for (const Card &c : b.cards) {
        if (c.list != list || c.id == moving)
            continue;
        if (k == slot) {
            const double mid = (before + c.pos) / 2.0;
            if (fits)
                *fits = mid > before && mid < c.pos;
            return mid;
        }
        before = c.pos;
        k++;
    }
    if (fits)
        *fits = before + STEP > before;
    return before + STEP;
}

/* ---- The sample boards ---- */

struct SeedCard {
    int         list;
    const char *title;
    unsigned    labels, members;
    int         due;
    int         priority;
    int         cover;
    const char *description;
};

static inline Card &seed_card(Board &b, const SeedCard &s) {
    Card c;
    c.id          = b.nextCard++;
    c.list        = s.list;
    c.pos         = pos_for_slot(b, s.list, cards_in(b, s.list), 0);
    c.title       = s.title;
    c.description = s.description;
    c.labels      = s.labels;
    c.members     = s.members;
    c.due         = s.due;
    c.priority    = s.priority;
    c.cover       = s.cover;
    return place_card(b, std::move(c));
}

static inline void seed_lists(Board &b, std::initializer_list<const char *> titles) {
    for (const char *t : titles) {
        List l;
        l.id    = b.nextList++;
        l.pos   = 1024.0 * l.id;
        l.title = t;
        b.lists.push_back(std::move(l));
    }
}

/* A release at a software company. serve.py starts from this same board, so the
   sample and the server read alike; keep the two lists in step. */
static inline Board atlas_board() {
    Board b;
    b.name    = "Atlas launch";
    b.prefix  = "AT";
    b.from    = rcHex(0x0B4FA8);
    b.to      = rcHex(0x6E3FC4);
    b.starred = true;
    const char *const names[LABEL_COUNT] = { "Feature", "Polish", "Design", "Bug", "Backend",
                                             "Docs" };
    for (int i = 0; i < LABEL_COUNT; i++)
        b.labelName[i] = names[i];
    seed_lists(b, { "Backlog", "In progress", "In review", "Shipped" });

    seed_card(b, { 1, "Single sign-on for enterprise workspaces", GREEN | PURPLE, PRIYA, 330, 2, -1,
                   "SAML first, then OIDC. Acme and Northwind are waiting on this before "
                   "they renew." }).checklist = { { "Pick the SAML library", true },
                                                  { "Admin settings page" },
                                                  { "Test with Okta and Entra" } };
    seed_card(b, { 1, "Rate limit the public API", PURPLE, TOM, 328, 2, -1,
                   "100 requests a minute per token, with a Retry-After header." });
    seed_card(b, { 1, "Translate onboarding into Spanish and German", BLUE, ANA, 0, 1, -1,
                   "" })
        .comments = { { BY_ANA, "The Spanish draft is back from the agency.", 306 } };
    seed_card(b, { 1, "Migration guide for v1 customers", BLUE, MAYA, 323, 3, -1,
                   "Cover the API key change and the new webhook format." });
    seed_card(b, { 1, "Dashboard is slow on accounts with 10k projects", RED, KENJI, 0, 3, -1,
                   "p95 is 4.2 s. The project count query is doing a full scan." });

    Card &pricing = seed_card(b, { 2, "Redesign the pricing page", YELLOW | ORANGE, MAYA | ANA, 319,
                                   3, 4,
                                   "Three plans, annual toggle, comparison table below the "
                                   "fold. Legal has signed off on the wording." });
    pricing.checklist = { { "Wireframes", true }, { "Copy review", true },
                          { "Comparison table", true }, { "Annual toggle" },
                          { "Launch checklist" } };
    pricing.comments  = { { BY_ANA, "Do we still show the Starter plan on mobile?", 307 },
                          { BY_MAYA, "Yes, but collapsed. Mock is in the design file.", 307 },
                          { BY_TOM, "Stripe price ids are ready when you are.", 308 } };
    seed_card(b, { 2, "Stop duplicate invoice emails", RED, DIEGO, 309, 4, -1,
                   "Webhook retries send a second email. Make the send idempotent on the "
                   "invoice id." })
        .comments = { { BY_DIEGO, "Reproduced on staging.", 308 },
                      { BY_PRIYA, "Support has 14 tickets on this.", 309 } };
    seed_card(b, { 2, "Usage-based billing: metering service", GREEN | PURPLE, PRIYA | TOM, 322, 3,
                   -1, "Events into Kafka, hourly rollups, one line on the invoice." })
        .checklist = { { "Event schema", true }, { "Kafka topic", true }, { "Hourly rollup job" },
                       { "Invoice line" }, { "Backfill" }, { "Alerting" } };

    seed_card(b, { 3, "Mobile nav drawer for small screens", YELLOW, KENJI, 311, 2, -1,
                   "Waiting on one last review pass." });
    seed_card(b, { 3, "Export the audit log to CSV", GREEN, TOM, 314, 1, -1,
                   "Admins asked for this in the last three customer calls." });
    seed_card(b, { 3, "Landing page copy, second pass", BLUE | ORANGE, ANA | MAYA, 0, 1, 0,
                   "Shorter hero line. Keep the customer quotes." })
        .comments = { { BY_MAYA, "Much better. Two small notes in the doc.", 305 } };

    Card &dark = seed_card(b, { 4, "Dark mode for the dashboard", GREEN | ORANGE, KENJI, 306, 2, -1,
                                "Follows the system setting, with an override in the profile." });
    dark.dueDone = true;
    Card &hooks = seed_card(b, { 4, "Retry failed Stripe webhooks", PURPLE, DIEGO, 304, 3,
                                 -1, "Five tries over an hour, then a page to the on-call." });
    hooks.dueDone = true;
    Card &press = seed_card(b, { 4, "Press kit and launch blog post", BLUE, ANA, 303, 1, 2, "" });
    press.dueDone   = true;
    press.checklist = { { "Logos and screenshots", true }, { "Founder quote", true },
                        { "Post goes live", true } };
    return b;
}

/* A marketing site, in warm colors. */
static inline Board website_board() {
    Board b;
    b.name   = "Website refresh";
    b.prefix = "WEB";
    b.from   = rcHex(0xC2410C);
    b.to     = rcHex(0xBE185D);
    const char *const names[LABEL_COUNT] = { "Evergreen", "Campaign", "Illustration", "Blocked",
                                             "SEO", "Video" };
    for (int i = 0; i < LABEL_COUNT; i++)
        b.labelName[i] = names[i];
    seed_lists(b, { "Ideas", "Writing", "Design", "Live" });

    seed_card(b, { 1, "Customer story: Northwind Logistics", GREEN, ANA, 334, 1, 5,
                   "They cut onboarding from two weeks to two days." });
    seed_card(b, { 1, "Comparison page against spreadsheets", PURPLE | GREEN, MAYA, 0, 0, -1, "" });
    seed_card(b, { 2, "Rewrite the home page hero", ORANGE, ANA | MAYA, 315, 3, -1,
                   "One sentence, no jargon, and say what it costs." })
        .checklist = { { "Three headline options", true }, { "Pick one with sales" } };
    seed_card(b, { 2, "FAQ for the new pricing", YELLOW | BLUE, ANA, 320, 2, -1, "" });
    seed_card(b, { 2, "Security page for procurement teams", GREEN, TOM, 312, 2, -1,
                   "SOC 2, data residency, and who to email." });
    seed_card(b, { 3, "Illustrations for the three plans", YELLOW, KENJI, 318, 2, 1, "" });
    seed_card(b, { 3, "Footer redesign", YELLOW, KENJI, 0, 0, -1, "" });
    seed_card(b, { 3, "Launch video storyboard", BLUE | RED, MAYA, 308, 3, -1,
                   "Blocked until the voice-over script is approved." });
    Card &blog = seed_card(b, { 4, "Move the blog to the new template", GREEN, DIEGO, 300, 1, -1,
                                "" });
    blog.dueDone = true;
    return b;
}

/* A home project, to show the same app away from software. */
static inline Board kitchen_board() {
    Board b;
    b.name   = "Kitchen remodel";
    b.prefix = "KIT";
    b.from   = rcHex(0x0F766E);
    b.to     = rcHex(0x4D7C0F);
    const char *const names[LABEL_COUNT] = { "Cabinets", "Lighting", "Plumbing", "Over budget",
                                             "Electrical", "Paint" };
    for (int i = 0; i < LABEL_COUNT; i++)
        b.labelName[i] = names[i];
    seed_lists(b, { "To decide", "To order", "Scheduled", "Done" });

    seed_card(b, { 1, "Backsplash: zellige or plain subway tile?", GREEN, MAYA | ANA, 325, 1, -1,
                   "Zellige is $38 a square foot against $9." });
    seed_card(b, { 1, "Paint color for the ceiling", BLUE, ANA, 0, 0, -1, "" });
    seed_card(b, { 2, "Quartz countertop, 42 sq ft", RED, MAYA, 316, 3, -1,
                   "Quote came in $600 over. Ask about the remnant slab." });
    seed_card(b, { 2, "Under-cabinet lights", YELLOW, TOM, 320, 1, -1, "" });
    seed_card(b, { 3, "Plumber moves the sink line", ORANGE, DIEGO, 313, 3, 2,
                   "Water is off from 8 to noon." })
        .checklist = { { "Clear under the sink", true }, { "Tell the neighbors" } };
    seed_card(b, { 3, "Electrician adds two circuits", PURPLE, TOM, 317, 2, -1, "" });
    Card &demo = seed_card(b, { 4, "Demolition", GREEN, DIEGO | TOM, 305, 2, -1, "" });
    demo.dueDone = true;
    Card &cab = seed_card(b, { 4, "Order the base cabinets", GREEN, MAYA, 290, 2, 0, "" });
    cab.dueDone = true;
    return b;
}

/* Move every date on a board by @p days: each one keeps its distance from today. */
static inline void shift_days(Board &b, int days) {
    for (Card &c : b.cards) {
        if (c.due)
            c.due += days;
        for (Comment &note : c.comments)
            note.day += days;
    }
}

#endif /* KANBAN_BOARD_H */
