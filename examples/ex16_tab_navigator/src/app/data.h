/*
    data.h - the demo's content: people, topics, articles, posts, stories

    Everything here is constant and hand-written, so the app opens the same on every machine
    and opens no socket. A real client fills the same shapes from its server. Which post you
    liked, who you follow and what you typed live in AppState (state.h), never here.
*/
#ifndef APP_DATA_H
#define APP_DATA_H

#include "rayclay.h"

#define N(table) ((int)(sizeof (table) / sizeof (table)[0]))

/* The pictures, real photographs and drawn faces (examples/assets/photos/LICENSE.md). rcLoadImage
   reads a relative path from the working directory, so run the desktop build from the repository
   root. A phone's launcher starts the app where its packaged copies are, at this same path. */
#define PICTURE_DIR "examples/assets/photos/"

typedef enum {
    PHOTO_ALPS, PHOTO_AURORA, PHOTO_BOOKS, PHOTO_BREAD, PHOTO_COAST, PHOTO_FERRY, PHOTO_FOREST,
    PHOTO_ROAD, PHOTO_TEA, PHOTO_TRAM, PHOTO_TYPE, PHOTO_WHALE, PHOTO_COUNT, NO_PHOTO = -1
} PhotoId;

static const char *const PHOTO_FILE[PHOTO_COUNT] = {
    PICTURE_DIR "alps.jpg", PICTURE_DIR "aurora.jpg", PICTURE_DIR "books.jpg",
    PICTURE_DIR "bread.jpg", PICTURE_DIR "coast.jpg", PICTURE_DIR "ferry.jpg",
    PICTURE_DIR "forest.jpg", PICTURE_DIR "road.jpg", PICTURE_DIR "tea.jpg",
    PICTURE_DIR "tram.jpg", PICTURE_DIR "type.jpg", PICTURE_DIR "whale.jpg",
};

typedef enum {
    P_ME, P_MAYA, P_JONAS, P_PRIYA, P_THEO, P_HANA, P_DIEGO, P_LENA, P_AMARA, P_SAM, P_INES,
    P_OWEN, P_ZOFIA, P_KOFI, P_ELISE, PERSON_COUNT
} PersonId;

typedef struct {
    const char *name, *handle, *bio, *place;
    RC_Color    from, to;       /* the profile banner's gradient */
    int         followers, following;
    bool        followed;       /* at launch */
} Person;

/* In PersonId order. A face is faces/<handle>.png. P_ME's name and bio seed what Edit profile
   changes (AppState.myName). */
static const Person PERSON[PERSON_COUNT] = {
    /* P_ME */ { "Alex Rivera", "alexrivera",
      "Reading on trains. Writing it down so I remember.",
      "Lisbon", { 124, 58, 237, 255 }, { 236, 72, 153, 255 }, 214, 0, false },
    /* P_MAYA */ { "Maya Okafor", "mayareads",
      "Urban planner. Streets, trams and the people on them.",
      "Rotterdam", { 14, 165, 233, 255 }, { 79, 70, 229, 255 }, 18420, 612, true },
    /* P_JONAS */ { "Jonas Lindqvist", "jonasl",
      "Marine biologist. Field notes from the Baltic. Bad sailor.",
      "Turku", { 13, 148, 136, 255 }, { 8, 145, 178, 255 }, 9310, 288, true },
    /* P_PRIYA */ { "Priya Raman", "priyaraman",
      "Science editor. Asking what the data actually says.",
      "Bengaluru", { 244, 63, 94, 255 }, { 168, 85, 247, 255 }, 64200, 1040, true },
    /* P_THEO */ { "Theo Marchetti", "theomakes",
      "Product designer in Turin. Type nerd, poster printer.",
      "Turin", { 245, 158, 11, 255 }, { 239, 68, 68, 255 }, 12750, 430, false },
    /* P_HANA */ { "Hana Sato", "hanasato",
      "Ceramics, tea and slow mornings.",
      "Kanazawa", { 217, 119, 6, 255 }, { 190, 18, 60, 255 }, 7680, 155, true },
    /* P_DIEGO */ { "Diego Fuentes", "diegoclimbs",
      "Mountain guide. Patagonia half the year, the Alps the rest.",
      "Chamonix", { 22, 163, 74, 255 }, { 13, 148, 136, 255 }, 23100, 377, false },
    /* P_LENA */ { "Lena Novak", "lenanovak",
      "Data journalist. Charts are arguments.",
      "Prague", { 37, 99, 235, 255 }, { 124, 58, 237, 255 }, 41800, 902, true },
    /* P_AMARA */ { "Amara Diallo", "amaracooks",
      "Food writer between Dakar and Lisbon.",
      "Lisbon", { 234, 88, 12, 255 }, { 219, 39, 119, 255 }, 30500, 740, false },
    /* P_SAM */ { "Sam Whitaker", "samw",
      "Battery engineer. Yes, I will explain the chemistry.",
      "Austin", { 71, 85, 105, 255 }, { 37, 99, 235, 255 }, 15900, 266, true },
    /* P_INES */ { "Ines Carvalho", "inesc",
      "Architect. Restoring old houses, one argument at a time.",
      "Porto", { 192, 38, 211, 255 }, { 225, 29, 72, 255 }, 5240, 318, false },
    /* P_OWEN */ { "Owen Hartley", "owenh",
      "Writes about software people actually have to use.",
      "Leeds", { 8, 145, 178, 255 }, { 22, 163, 74, 255 }, 27600, 511, false },
    /* P_ZOFIA */ { "Zofia Wrona", "zofiareads",
      "Book critic. Fifty-two novels a year, on purpose.",
      "Krakow", { 147, 51, 234, 255 }, { 37, 99, 235, 255 }, 38900, 684, true },
    /* P_KOFI */ { "Kofi Mensah", "kofiskies",
      "Astronomy teacher. Clear skies.",
      "Accra", { 30, 64, 175, 255 }, { 13, 148, 136, 255 }, 21300, 196, false },
    /* P_ELISE */ { "Elise Moreau", "elisepedals",
      "Cycling across Europe, slowly. Day count in every post.",
      "On the road", { 225, 29, 72, 255 }, { 245, 158, 11, 255 }, 11200, 349, true },
};

typedef enum {
    T_CITIES, T_CLIMATE, T_DESIGN, T_SPACE, T_FOOD, T_BOOKS, T_SCIENCE, T_OUTDOORS, TOPIC_COUNT
} TopicId;

typedef struct {
    const char *name, *about;
    PhotoId     photo;
} Topic;

/* In TopicId order. */
static const Topic TOPIC[TOPIC_COUNT] = {
    { "Cities", "Streets, transit and how places change.", PHOTO_TRAM },
    { "Climate", "What the measurements say this year.", PHOTO_FOREST },
    { "Design", "Type, tools and things made with care.", PHOTO_TYPE },
    { "Space", "The night sky, from the ground up.", PHOTO_AURORA },
    { "Food", "Recipes, kitchens and the people in them.", PHOTO_BREAD },
    { "Books", "What people are reading, and why.", PHOTO_BOOKS },
    { "Science", "Studies read carefully, and field notes.", PHOTO_WHALE },
    { "Outdoors", "Mountains, coasts and long days out.", PHOTO_ALPS },
};

/* What people are talking about, on Explore. A row opens its topic. */
typedef struct {
    const char *label;
    TopicId     topic;
    int         posts;
} Trend;

static const Trend TREND[] = {
    { "Night trains", T_CITIES, 18400 },
    { "Dark-sky towns", T_SPACE, 12100 },
    { "Gulf of Finland", T_CLIMATE, 9200 },
    { "Slow bread", T_FOOD, 6300 },
    { "Letterpress", T_DESIGN, 4700 },
};

typedef struct {
    PersonId    author;
    TopicId     topic;
    PhotoId     photo;
    const char *text;
    int         minutes;      /* how long ago */
    int         likes, reposts, replies;
} PostSeed;

/* The feed at launch, newest first. replies is exact: it is how many a post's page shows. */
static const PostSeed SEED[] = {
    { P_DIEGO, T_OUTDOORS, PHOTO_ALPS,
      "First light on the ridge this morning. Minus nine, no wind, nobody else up here. Worth the "
      "3 a.m. alarm.", 12, 842, 64, 4 },
    { P_PRIYA, T_SCIENCE, NO_PHOTO,
      "Reminder when you read a headline about a new study: find the sample size first. It takes "
      "ten seconds and saves you a week of being wrong.", 38, 2310, 540, 5 },
    { P_MAYA, T_CITIES, PHOTO_TRAM,
      "The trams at shift change are the only time this city looks finished.", 52, 1130, 58, 2 },
    { P_HANA, T_FOOD, PHOTO_TEA,
      "Two years of tea lessons came down to this: a cast-iron pot, roasted leaves and ten quiet "
      "minutes.", 75, 1540, 97, 4 },
    { P_LENA, T_SCIENCE, NO_PHOTO,
      "A chart is an argument. If the y-axis starts at 40, ask what it is arguing.",
      110, 3120, 812, 6 },
    { P_JONAS, T_SCIENCE, PHOTO_WHALE,
      "Forty minutes out of the harbor and this one came up beside the boat. Nobody remembered the "
      "sample bottles.", 140, 978, 301, 5 },
    { P_ELISE, T_OUTDOORS, PHOTO_COAST,
      "Day 41. Pedaled into Porto with a headwind and a flat tire. The ocean made up for both.",
      190, 1205, 76, 3 },
    { P_ZOFIA, T_BOOKS, PHOTO_BOOKS,
      "Fifty-two novels a year sounds like a stunt. It is actually a scheduling trick, and it made "
      "me a kinder reader.", 225, 1902, 264, 5 },
    { P_OWEN, T_DESIGN, NO_PHOTO,
      "Every settings screen should answer one question: what will change if I touch this? Most of "
      "them don't.", 270, 870, 150, 3 },
    { P_KOFI, T_SPACE, PHOTO_AURORA,
      "Took my students north for the weekend. Nobody looked at a phone for two hours. Not one.",
      330, 2688, 233, 5 },
    { P_AMARA, T_FOOD, PHOTO_BREAD,
      "The best bakery in my neighborhood almost closed over a lease clause, not over the bread. "
      "This is what slow food costs.", 400, 734, 120, 4 },
    { P_SAM, T_SCIENCE, NO_PHOTO,
      "People ask how grid batteries fail. Rarely with fire. Mostly it is a slow loss of capacity "
      "nobody budgeted for.", 470, 561, 143, 2 },
    { P_INES, T_CITIES, NO_PHOTO,
      "Spent the afternoon on the harbor wall measuring window reveals for a house that predates "
      "the harbor wall.", 560, 418, 31, 2 },
    { P_THEO, T_DESIGN, PHOTO_TYPE,
      "Sorting metal type for the club's winter show. Two colors, one weekend, zero regrets.",
      640, 688, 49, 2 },
    { P_JONAS, T_CLIMATE, PHOTO_FOREST,
      "Back on land and up in the survey plane. The spruce along the shore are browning from the "
      "tips, further north than last year.", 720, 506, 130, 3 },
    { P_ME, T_BOOKS, NO_PHOTO,
      "Started keeping a paper list of everything I read this year. Thirty-one so far, and I "
      "remember the ones I wrote a line about.", 1100, 42, 3, 2 },
    { P_PRIYA, T_CLIMATE, NO_PHOTO,
      "Attribution science has gotten fast enough that the study now lands before the floodwater "
      "recedes. That changes what a newsroom can say on day two.", 1390, 1455, 402, 3 },
    { P_AMARA, T_FOOD, NO_PHOTO,
      "Salt your onions earlier than you think. That is the whole post.", 1700, 4210, 930, 6 },
    { P_ELISE, T_OUTDOORS, PHOTO_ROAD,
      "Someone told me the Spanish interior was empty. It is just that everything in it is far "
      "apart.", 1900, 972, 61, 2 },
    { P_ME, T_CITIES, PHOTO_FERRY,
      "Took the slow ferry instead of the bridge this morning. Twelve extra minutes and two very "
      "good dogs.", 2100, 58, 4, 3 },
    { P_ZOFIA, T_BOOKS, NO_PHOTO,
      "A rule that has never failed me: if the first chapter has a funeral, the book will be very "
      "good or very bad. Never fine.", 2400, 1388, 176, 4 },
    { P_OWEN, T_DESIGN, NO_PHOTO,
      "The fastest way to find out what a feature is for is to watch someone try to turn it off.",
      2700, 1760, 420, 3 },
    { P_INES, T_CITIES, NO_PHOTO,
      "Old houses don't have straight walls. They have opinions.", 3000, 2044, 190, 2 },
    { P_ME, T_DESIGN, NO_PHOTO,
      "Hot take: a good back button is the most important feature in any app.", 3900, 31, 2, 1 },
};

/* What a refresh finds, a few at a time, oldest of the batch last. */
static const PostSeed INCOMING[] = {
    { P_LENA, T_CITIES, NO_PHOTO,
      "New numbers on who actually rides the night buses. It is not who the budget assumes.",
      0, 120, 18, 1 },
    { P_KOFI, T_SPACE, PHOTO_AURORA,
      "Clear tonight. Jupiter rises around nine. Go outside.", 2, 240, 15, 1 },
    { P_THEO, T_DESIGN, NO_PHOTO,
      "Kerning is a conversation between two letters that don't want to talk.", 4, 84, 6, 0 },
    { P_ELISE, T_OUTDOORS, PHOTO_COAST,
      "Day 43. Rest day. The bike is in the shop and I am in the sea.", 5, 190, 7, 1 },
    { P_PRIYA, T_SCIENCE, NO_PHOTO,
      "A preprint is not a paper, and a press release is not a preprint.", 7, 410, 60, 2 },
    { P_AMARA, T_FOOD, NO_PHOTO,
      "Tonight: rice, beans and the last of the good tomatoes.", 9, 96, 3, 0 },
};
enum { REFRESH_BATCH = 3 };

/* A post's page shows post.replies of these, walked so no two posts open on the same list. */
static const char *const REPLY_LINE[] = {
    "This is the most useful thing I have read all week.",
    "Saving this for the weekend.",
    "I would love to hear how this looks a year from now.",
    "We saw the same thing where I live, though the numbers were smaller.",
    "Thank you for writing this. Sending it to my sister.",
    "The second point is the one people always miss.",
    "I disagreed until the last line. Now I need to think about it.",
    "Do you have a link to the source for that?",
    "Reading this on the train, which feels right.",
    "Now I know what to bring up at dinner.",
    "Can confirm. Learned this the hard way a few years ago.",
    "This deserves a longer piece. Please write it.",
};

/* Who the row over the feed rings: they have posted since you last looked. */
static const PersonId RINGED[] = {
    P_MAYA, P_ELISE, P_KOFI, P_DIEGO, P_JONAS, P_INES, P_THEO, P_HANA,
};
enum { RINGED_COUNT = N(RINGED) };

/* What Activity's rows say, one per kind. */
typedef enum { ACT_LIKE, ACT_REPLY, ACT_REPOST, ACT_FOLLOW, ACT_MENTION, ACT_KINDS } ActivityKind;

static const char *const ACT_TEXT[ACT_KINDS] = {
    "liked your post", "replied to you", "reposted you", "followed you", "mentioned you",
};

/* The words of Home's date line. app_seed sets a Thursday, November 5; update (app/app.h) puts
   in the device's own date. */
static const char *const DAY_NAME[7] = {
    "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY",
};
static const char *const MONTH_NAME[12] = {
    "JANUARY", "FEBRUARY", "MARCH",     "APRIL",   "MAY",      "JUNE",
    "JULY",    "AUGUST",   "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER",
};

#endif /* APP_DATA_H */
