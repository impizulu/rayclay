/*
    sample_tree.h - the drive the app opens on, as a table

    One person's home folder, written as an outline: a row's depth says where it
    hangs, and a name that ends in '/' is a folder. Nothing here touches a disk, so
    the first run is the same on every machine and reads no file of the user's.
    Sizes and dates are fixed, and the budget's and the invoice's rows add up.
*/
#ifndef APP_SAMPLE_TREE_H
#define APP_SAMPLE_TREE_H

#include "rayclay.h"
#include "app/samples.h"

/* The Finder's seven tag colors, in its order. 0 means no tag. */
enum { TAG_NONE = 0, TAG_RED, TAG_ORANGE, TAG_YELLOW, TAG_GREEN, TAG_BLUE, TAG_PURPLE, TAG_GRAY,
       TAG_END };

enum { KB = 1000 };
#define GB(x) ((uint64_t)((x) * 1000000000.0))

/* What a preview shows for a document. A sample has no bytes on disk, so its text is here. */
static const char BODY_BRIEF[] =
    "Harbor Cafe: Creative Brief\n\n"
    "Harbor Cafe has served the Eastside pier since 1998. The new owners want the brand to "
    "feel as warm as the room without losing the regulars who still ask for the blue mugs.\n\n"
    "Goals: a mark that works on a cup sleeve and a storefront, a menu that reads in low "
    "light, and one voice across print and web.\n\n"
    "Deliverables: logo suite, color and type system, menu templates, signage.\n\n"
    "Budget: $18,400. Launch: January 12.";

static const char BODY_GUIDE[] =
    "Harbor Cafe: Brand Guidelines v3\n\n"
    "1. The mark. Keep one mug height of clear space on every side. Never set it on a photo "
    "without the tide band behind it.\n\n"
    "2. Color. Harbor Blue leads. Sea Glass and Rope support. Buoy Red is for a single "
    "accent per layout.\n\n"
    "3. Type. Headlines in the display cut, set tight. Menus at no less than 11 point.";

static const char BODY_NOTES[] =
    "# Kickoff notes, Oct 6\n\n"
    "- Dana wants the blue mugs in the first shoot\n"
    "- Sign vendor needs vector art by Nov 14\n"
    "- Menu has 41 items; cut to 28 before layout\n"
    "- Photos: Sunday Oct 18, 7am for the light\n\n"
    "Next: moodboard review on Tuesday.";

/* A line holding tabs draws as a row of cells: names on the left, numbers on the right. */
static const char BODY_BUDGET[] =
    "Item\tBudget\tSpent\n"
    "Logo suite\t$6,500\t$6,500\n"
    "Photography\t$4,200\t$3,850\n"
    "Menu templates\t$2,900\t$1,450\n"
    "Signage\t$3,800\t$0\n"
    "Print proofs\t$1,000\t$312\n"
    "Total\t$18,400\t$12,112";

static const char BODY_INVOICE[] =
    "Invoice 1042\n"
    "Northwind Studio, for Harbor Cafe, 14 Eastside Pier\n"
    "October 31, 2026. Due in 30 days.\n\n"
    "Work\tAmount\n"
    "Logo suite, final payment\t$3,250\n"
    "Menu templates, first half\t$1,450\n"
    "Print proofs\t$312\n"
    "Total due\t$5,012\n\n"
    "Thank you. Dana has the blue mugs on order.";

static const char BODY_TODO[] =
    "This week\n\n"
    "[x] Send invoice 1042 to Harbor Cafe\n"
    "[ ] Export menu proofs for Dana\n"
    "[ ] Book the sign vendor call\n"
    "[ ] Renew the lease paperwork\n"
    "[ ] Back up the Lisbon photos";

static const char BODY_RESUME[] =
    "Maya Chen\nBrand and Product Designer\n\n"
    "Eleven years shaping identities and interfaces for small businesses and startups.\n\n"
    "Northwind Studio, Lead Designer, 2021 to now.\n"
    "Fieldnote, Senior Designer, 2017 to 2021.\n"
    "Parallel Print, Designer, 2015 to 2017.";

static const char BODY_READING[] =
    "# Reading list\n\n"
    "- Thinking with Type, Ellen Lupton\n"
    "- The Elements of Typographic Style, Robert Bringhurst\n"
    "- Grid Systems, Josef Muller-Brockmann\n"
    "- A Pattern Language, Christopher Alexander";

static const char BODY_README[] =
    "# Trailhead\n\n"
    "Offline trail maps for day hikers. One C source tree, every platform.\n\n"
    "    cmake -B build\n"
    "    cmake --build build\n"
    "    ./build/trailhead\n\n"
    "Tile data lives in assets/tiles; see docs/tiles.md for the format.";

static const char BODY_MAIN[] =
    "#include \"rayclay.h\"\n\n"
    "static void layout(RC_App *app, void *user)\n"
    "{\n"
    "    rcColumn(.w = \"grow\", .h = \"grow\") {\n"
    "        map_view(app, user);\n"
    "        trail_list(app, user);\n"
    "    }\n"
    "}\n\n"
    "int main(void)\n"
    "{\n"
    "    RC_AppOptions opts = { .title = \"Trailhead\", .layoutCallback = layout };\n"
    "    return rcRunApp(&opts);\n"
    "}";

/* One row of the outline. scene names a picture in samples.h, or -1. */
typedef struct {
    int         depth;
    const char *name;
    const char *when;
    uint64_t    size;
    int         tag;
    int         scene;
    const char *body;
} SampleRow;

static const SampleRow SAMPLE_ROWS[] = {
    { 0, "Applications/", "2026-09-14T10:02:00Z", 0, 0, -1, NULL },
    { 1, "Atlas Maps.app", "2026-09-02T08:11:00Z", 188500 * KB, 0, -1, NULL },
    { 1, "Beacon Mail.app", "2026-08-21T15:47:00Z", 96700 * KB, 0, -1, NULL },
    { 1, "Cadence Calendar.app", "2026-07-30T11:20:00Z", 54600 * KB, 0, -1, NULL },
    { 1, "Inkwell.app", "2026-09-12T09:05:00Z", 412100 * KB, 0, -1, NULL },
    { 1, "Prism Photo.app", "2026-08-03T18:33:00Z", GB(1.24), 0, -1, NULL },
    { 1, "Tempo Music.app", "2026-06-17T13:02:00Z", 231200 * KB, 0, -1, NULL },
    { 1, "Vault Passwords.app", "2026-09-14T10:02:00Z", 72300 * KB, 0, -1, NULL },

    { 0, "Desktop/", "2026-11-02T09:41:00Z", 0, 0, -1, NULL },
    { 1, "Invoice 1042 - Harbor Cafe.pdf", "2026-10-31T16:05:00Z", 184 * KB, TAG_GREEN, -1,
      BODY_INVOICE },
    { 1, "Screenshot 2026-11-02 at 9.41 AM.png", "2026-11-02T09:41:00Z", 2500 * KB, 0, PIC_SCREEN,
      NULL },
    { 1, "To Do.txt", "2026-11-03T08:12:00Z", 212, TAG_RED, -1, BODY_TODO },

    { 0, "Documents/", "2026-11-03T08:40:00Z", 0, 0, -1, NULL },
    { 1, "Family Recipes.docx", "2026-03-22T19:15:00Z", 2500 * KB, 0, -1, NULL },
    { 1, "Harbor Cafe Rebrand/", "2026-11-03T08:40:00Z", 0, TAG_ORANGE, -1, NULL },
    { 2, "Brand Guidelines v3.pdf", "2026-10-28T14:18:00Z", 24200 * KB, TAG_ORANGE, -1,
      BODY_GUIDE },
    { 2, "Budget.xlsx", "2026-10-30T17:22:00Z", 84 * KB, 0, -1, BODY_BUDGET },
    { 2, "Creative Brief.pdf", "2026-10-06T10:30:00Z", 1800 * KB, 0, -1, BODY_BRIEF },
    { 2, "Kickoff Notes.md", "2026-10-06T16:02:00Z", 4 * KB, 0, -1, BODY_NOTES },
    { 2, "Logos/", "2026-10-24T11:45:00Z", 0, 0, -1, NULL },
    { 3, "harbor-mark-blue.png", "2026-10-24T11:45:00Z", 318 * KB, TAG_BLUE, PIC_MARK_BLUE,
      NULL },
    { 3, "harbor-mark-mono.svg", "2026-10-24T11:40:00Z", 12 * KB, 0, PIC_MARK_MONO, NULL },
    { 3, "harbor-sign-lockup.png", "2026-10-22T15:09:00Z", 742 * KB, 0, PIC_LOCKUP, NULL },
    { 2, "Menu Redesign.pptx", "2026-11-01T12:51:00Z", 42200 * KB, 0, -1, NULL },
    { 2, "Moodboard/", "2026-10-21T16:12:00Z", 0, 0, -1, NULL },
    { 3, "Dawn over the harbor.jpg", "2026-10-18T07:14:00Z", 3100 * KB, TAG_BLUE, PIC_DAWN, NULL },
    { 3, "Morning light.jpg", "2026-10-18T07:32:00Z", 2800 * KB, 0, PIC_MORNING, NULL },
    { 3, "Net lofts at dusk.jpg", "2026-10-17T18:05:00Z", 3400 * KB, 0, PIC_LOFTS, NULL },
    { 3, "Old pier.jpg", "2026-10-18T07:51:00Z", 2600 * KB, 0, PIC_PIER, NULL },
    { 3, "Sea glass palette.png", "2026-10-21T16:12:00Z", 980 * KB, TAG_GREEN, PIC_PALETTE, NULL },
    { 2, "Photoshoot Schedule.xlsx", "2026-10-14T09:27:00Z", 38 * KB, 0, -1, NULL },
    { 1, "Home Inventory.xlsx", "2026-01-11T20:44:00Z", 142 * KB, 0, -1, NULL },
    { 1, "Lease Agreement.pdf", "2026-02-27T11:03:00Z", 3300 * KB, TAG_RED, -1, NULL },
    { 1, "Reading List.md", "2026-09-19T21:30:00Z", 1 * KB, TAG_PURPLE, -1, BODY_READING },
    { 1, "Resume - Maya Chen.pdf", "2026-10-02T13:16:00Z", 212 * KB, TAG_GREEN, -1,
      BODY_RESUME },
    { 1, "Taxes 2025/", "2026-04-09T18:20:00Z", 0, 0, -1, NULL },
    { 2, "1099-INT Seaboard Credit Union.pdf", "2026-02-03T09:00:00Z", 96 * KB, 0, -1, NULL },
    { 2, "Deductions.xlsx", "2026-04-02T20:15:00Z", 58 * KB, 0, -1, NULL },
    { 2, "Receipts/", "2026-04-02T20:01:00Z", 0, 0, -1, NULL },
    { 3, "Desk lamp.pdf", "2026-03-11T12:00:00Z", 44 * KB, 0, -1, NULL },
    { 3, "Home office chair.pdf", "2026-03-11T12:02:00Z", 51 * KB, 0, -1, NULL },
    { 3, "Laptop stand.pdf", "2026-03-11T12:04:00Z", 39 * KB, 0, -1, NULL },
    { 2, "Return Summary.pdf", "2026-04-09T18:20:00Z", 310 * KB, TAG_GRAY, -1, NULL },
    { 2, "W-2 2025.pdf", "2026-01-29T08:45:00Z", 88 * KB, 0, -1, NULL },
    { 1, "Thesis/", "2026-05-18T22:08:00Z", 0, TAG_YELLOW, -1, NULL },
    { 2, "Chapter 1 - Introduction.docx", "2026-03-02T17:30:00Z", 88 * KB, 0, -1, NULL },
    { 2, "Chapter 2 - Methods.docx", "2026-03-29T19:12:00Z", 132 * KB, 0, -1, NULL },
    { 2, "Chapter 10 - Conclusion.docx", "2026-05-18T22:08:00Z", 74 * KB, 0, -1, NULL },
    { 2, "Figures/", "2026-04-20T14:40:00Z", 0, 0, -1, NULL },
    { 3, "Survey sites.png", "2026-04-20T14:40:00Z", 620 * KB, 0, PIC_FIGURE, NULL },
    { 2, "References.bib", "2026-05-11T10:25:00Z", 22 * KB, 0, -1, NULL },

    { 0, "Downloads/", "2026-11-01T18:30:00Z", 0, 0, -1, NULL },
    { 1, "Inkwell-4.2.dmg", "2026-09-12T08:58:00Z", 186300 * KB, 0, -1, NULL },
    { 1, "meeting-recording.m4a", "2026-10-27T15:02:00Z", 38300 * KB, 0, -1, NULL },
    { 1, "photos-export.zip", "2026-10-11T22:14:00Z", GB(1.42), 0, -1, NULL },
    { 1, "rayclay-1.0.30.tar.gz", "2026-11-01T18:30:00Z", 8900 * KB, 0, -1, NULL },
    { 1, "seaboard-statement-oct.pdf", "2026-11-01T09:12:00Z", 412 * KB, 0, -1, NULL },
    { 1, "wallpaper-alpine.jpg", "2026-08-30T12:47:00Z", 5100 * KB, 0, PIC_ALPINE, NULL },

    { 0, "Movies/", "2026-10-12T17:44:00Z", 0, 0, -1, NULL },
    { 1, "Lisbon Trip.mov", "2026-06-02T20:10:00Z", GB(1.9), TAG_PURPLE, -1, NULL },
    { 1, "Screen Recording 2026-10-12.mov", "2026-10-12T17:44:00Z", 412100 * KB, 0, -1, NULL },

    { 0, "Music/", "2026-09-27T10:05:00Z", 0, 0, -1, NULL },
    { 1, "Demo Mixdown.mp3", "2026-09-27T10:05:00Z", 7700 * KB, 0, -1, NULL },
    { 1, "Field Recordings/", "2026-08-16T09:30:00Z", 0, 0, -1, NULL },
    { 2, "Harbor Bells.m4a", "2026-08-16T09:30:00Z", 6600 * KB, 0, -1, NULL },
    { 2, "Night Ferry.m4a", "2026-07-04T23:18:00Z", 9900 * KB, 0, -1, NULL },
    { 2, "Rain on Tin Roof.wav", "2026-05-29T06:42:00Z", 48400 * KB, 0, -1, NULL },

    { 0, "Pictures/", "2026-10-18T09:15:00Z", 0, 0, -1, NULL },
    { 1, "Alpine Lake.jpg", "2026-08-30T12:50:00Z", 5100 * KB, 0, PIC_ALPINE, NULL },
    { 1, "Desert Dusk.jpg", "2026-07-12T19:03:00Z", 4300 * KB, TAG_YELLOW, PIC_DESERT, NULL },
    { 1, "Lisbon 2026/", "2026-06-02T19:40:00Z", 0, TAG_PURPLE, -1, NULL },
    { 2, "Alfama at dusk.jpg", "2026-05-27T18:58:00Z", 4300 * KB, 0, PIC_ALFAMA, NULL },
    { 2, "Miradouro morning.jpg", "2026-05-28T07:21:00Z", 3300 * KB, 0, PIC_MIRADOURO, NULL },
    { 2, "Tagus crossing.jpg", "2026-05-29T16:37:00Z", 4300 * KB, 0, PIC_TAGUS, NULL },
    { 1, "Sea Glass Study.png", "2026-10-18T09:15:00Z", 1100 * KB, 0, PIC_STUDY, NULL },

    { 0, "Projects/", "2026-11-03T07:58:00Z", 0, 0, -1, NULL },
    { 1, "trailhead/", "2026-11-03T07:58:00Z", 0, TAG_BLUE, -1, NULL },
    { 2, ".gitignore", "2026-08-01T10:00:00Z", 64, 0, -1, NULL },
    { 2, "CMakeLists.txt", "2026-10-29T21:10:00Z", 2 * KB, 0, -1, NULL },
    { 2, "README.md", "2026-10-29T21:44:00Z", 3 * KB, 0, -1, BODY_README },
    { 2, "src/", "2026-11-03T07:58:00Z", 0, 0, -1, NULL },
    { 3, "main.c", "2026-11-03T07:58:00Z", 6 * KB, 0, -1, BODY_MAIN },
    { 3, "map_view.c", "2026-11-02T22:31:00Z", 18 * KB, 0, -1, NULL },
    { 3, "trail_list.c", "2026-11-01T20:12:00Z", 9 * KB, 0, -1, NULL },
};

/* What the Trash already holds on a first run, so Put Back and Empty have something to do.
   seed() in browser.h says which folder each came from. */
static const SampleRow SAMPLE_TRASH[] = {
    { 0, "Old Logo Drafts/", "2026-09-30T16:20:00Z", 0, 0, -1, NULL },
    { 1, "harbor-logo-v1.png", "2026-09-30T16:18:00Z", 402 * KB, 0, PIC_LOGO_V1, NULL },
    { 0, "Untitled 3.txt", "2026-10-09T11:37:00Z", 86, 0, -1, NULL },
};

#endif /* APP_SAMPLE_TREE_H */
