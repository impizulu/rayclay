/*  main.cpp - RayClay ex06: a kanban board written in C++.

    A three-lane task board (To do / Doing / Done) whose cards carry a label and
    a reference and move between lanes, with the board's state in ordinary C++
    containers. Below the md breakpoint the three lanes become one behind a
    segmented switcher.

    RayClay is C99; your application does not have to be. The header is wrapped
    in extern "C" and the element DSL is the same text in both languages. Four
    differences bite, and all four are visible in this file:

      1. DESIGNATED INITIALISERS FOLLOW DECLARATION ORDER in C++20, and g++
         hard-errors where clang++ only warns. Every rcRow / rcColumn / rcBox and
         the RC_AppOptions below list their fields in rayclay.h's order.
      2. RAYCLAY BORROWS YOUR STRINGS UNTIL THE FRAME IS DRAWN, which is after
         your layout callback returns. A std::string in a container is fine to
         hand over, provided the container is not mutated while the frame is in
         flight. This board records a click as a PENDING change and applies it at
         the top of the NEXT layout, before anything is declared.
      3. A CAPTURE-LESS LAMBDA IS A C FUNCTION POINTER: it converts to
         RC_LayoutCallback, and userData carries `this` across.
      4. ARRAY DESIGNATORS ARE C ONLY: the font ladder below is positional.

    Linking the rayclay target adds -std=c++20 and -Wno-missing-field-initializers
    to your C++ TUs. Build target: rayclay_ex06_cpp_kanban
*/

#include "rayclay.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

enum AppFont { F_SMALL = 0, F_BODY, F_TITLE, F_COUNT };

/* Slot 0 is also the size every library widget draws its own text at. */
static const float FONT_SIZES[F_COUNT] = { 14.0f, 16.0f, 20.0f };

constexpr int LANE_COUNT = 3;
constexpr int CARD_CAP   = 64;                  /* a board, not a database */

static const char *const LANE_TITLE[LANE_COUNT] = { "To do", "Doing", "Done" };

/* What moving a card FORWARD from each lane is called; the last lane has no
   forward, it has Remove. A verb beats an arrow: it says where the card goes. */
static const char *const LANE_FORWARD[LANE_COUNT] = { "Start", "Finish", nullptr };

/* The label a card carries - one per card, and the board's only colour code. */
enum Label { L_BUILD = 0, L_DOCS, L_DESIGN, L_BUG, L_COUNT };
static const char *const LABEL_TEXT[L_COUNT] = { "build", "docs", "design", "bug" };
static const RC_Color    LABEL_INK[L_COUNT]  = { RC_SKY_700, RC_VIOLET_600,
                                                 RC_AMBER_600, RC_ROSE_600 };

struct Card {
    std::string title;
    int         label = L_BUILD;
    int         ref   = 0;      /* the "RC-7" the card shows; never reused */
};

/* A click on a card records ONE of these; the board applies it at the top of the
   next layout (rule 2 above). `delta` is -1 / +1 for a lane move, 0 for delete. */
struct PendingMove {
    int lane  = -1;
    int index = -1;
    int delta = 0;
    bool empty() const { return lane < 0; }
};

class Board {
public:
    Board();
    void layout(RC_App *app);

private:
    std::array<std::vector<Card>, LANE_COUNT> lanes;
    PendingMove pending;
    bool        pendingAdd = false;             /* the composer's text is waiting  */
    char        input[64]  = {};                /* the composer buffer, owned by us */
    int         shownLane  = 0;                 /* the one-lane arm's active lane   */
    int         addLane    = 0;                 /* the lane whose composer is open  */
    int         pendingLane = -1;               /* a click asking the composer to move */
    int         totalCards = 0;
    int         nextRef    = 1;

    void apply_pending();
    void lane_column(RC_App *app, int lane, bool grow);
    void card(RC_App *app, int lane, int index);
    void lane_switcher(RC_App *app);
};

Board::Board() {
    lanes[0] = { { "Write the C++ example",           L_DOCS   },
                 { "Read rayclay.h top to bottom",    L_DOCS   },
                 { "Try a std::string title",         L_BUILD  },
                 { "Pick a lane accent colour",       L_DESIGN } };
    lanes[1] = { { "Move a card with Start and Back", L_DESIGN },
                 { "Check the one-lane arm at 768",   L_DESIGN },
                 { "Wrap a long title in a lane",     L_BUG    } };
    lanes[2] = { { "Build with -Wextra -Werror",      L_BUILD  },
                 { "Keep designators in order",       L_BUILD  },
                 { "Hand the layout a lambda",        L_DOCS   } };
    for (auto &lane : lanes)
        for (auto &c : lane) {
            c.ref = nextRef++;
            totalCards++;
        }
}

/* Mutations happen HERE, before any element of the new frame is declared, so no
   pointer the previous frame borrowed is still in use (rule 2). push_back may
   reallocate the whole lane, which is why the composer defers too. */
void Board::apply_pending() {
    if (pendingLane >= 0) {
        addLane = pendingLane;
        pendingLane = -1;
    }
    if (pendingAdd) {
        pendingAdd = false;
        if (input[0] && totalCards < CARD_CAP) {
            /* A new card takes the next label, so the board keeps its colour. */
            lanes[addLane].push_back({ std::string(input), nextRef % L_COUNT, nextRef });
            nextRef++;
            totalCards++;
        }
        input[0] = '\0';
    }
    if (pending.empty())
        return;
    std::vector<Card> &from = lanes[pending.lane];
    if (pending.index >= 0 && pending.index < static_cast<int>(from.size())) {
        Card moved = std::move(from[pending.index]);
        from.erase(from.begin() + pending.index);
        int to = pending.lane + pending.delta;
        if (pending.delta != 0 && to >= 0 && to < LANE_COUNT)
            lanes[to].push_back(std::move(moved));
        else
            totalCards--;                       /* delete, or a move off the board */
    }
    pending = PendingMove{};
}

/* One card: its label, its reference, the title, then the move verbs. The ids are
   formatted into the frame arena; `.chars` is the C string. */
void Board::card(RC_App *app, int lane, int index) {
    RC_Arena *mem = rcAppArena(app);
    RC_Style  s   = rcGetStyle();
    const char *card_id = rcFormat(mem, "l%d_%d", lane, index).chars;
    const char *back_id = rcFormat(mem, "l%d_%d_back", lane, index).chars;
    const char *fwd_id  = rcFormat(mem, "l%d_%d_fwd",  lane, index).chars;
    const Card &c    = lanes[lane][index];
    const bool  done = lane == LANE_COUNT - 1;
    RC_String   ref  = rcFormat(mem, "RC-%d", c.ref);
    RC_Color    ink  = LABEL_INK[c.label];

    rcColumn(.id = card_id, .bg = s.surface, .gap = 8, .p = 12,
             .borderRadius = "all-md", .w = "grow",
             .shadow = { rcAlpha(RC_BLACK, 18), 0, 1, 3, 0 }) {
        rcRow(.gap = 6, .align = "cl", .w = "grow") {
            rcBox(.bg = rcAlpha(ink, 38), .px = 8, .py = 2, .borderRadius = "all-full") {
                rcTextC(LABEL_TEXT[c.label], .font = F_SMALL, .color = ink, .wrap = "n");
            }
            rcBox(.w = "grow") {}
            rcText(ref, .font = F_SMALL, .color = s.textMuted, .wrap = "n");
        }
        /* c.title outlives the frame: it lives in the lane vector, unmutated. */
        rcTextC(c.title.c_str(), .font = F_BODY, .color = done ? s.textMuted : s.text);
        rcRow(.gap = 4, .align = "cr", .w = "grow") {
            if (lane > 0 && rcButton(back_id, "Back", RC_BTN_GHOST))
                pending = { lane, index, -1 };
            if (!done && rcButton(fwd_id, LANE_FORWARD[lane], RC_BTN_DEFAULT))
                pending = { lane, index, +1 };
            /* DANGER, not GHOST. In the Done lane this sits beside "Back", and the
               variant is the only thing telling "put it back" apart from "destroy it". */
            if (done && rcButton(fwd_id, "Remove", RC_BTN_DANGER))
                pending = { lane, index, 0 };
        }
    }
}

/* One lane: a header with the live count, then its cards in a scroll column.
   `grow` is true on the three-across arm and false when the lane is alone. */
void Board::lane_column(RC_App *app, int lane, bool grow) {
    RC_Arena *mem = rcAppArena(app);
    RC_Style  s   = rcGetStyle();
    const std::vector<Card> &cards = lanes[lane];
    const char *scroll_id = rcFormat(mem, "lane%d", lane).chars;
    const char *add_id    = rcFormat(mem, "laneadd%d", lane).chars;
    /* Changes whenever a card moves, so it belongs in the arena, not in a member. */
    RC_String count = rcFormat(mem, "%zu", cards.size());

    /* One accent per lane: neutral, then primary while live, then success. */
    const RC_Color accent[LANE_COUNT] = { s.textMuted, s.primary, s.success };

    rcColumn(.bg = s.surfaceAlt, .gap = 10, .p = 12, .borderRadius = "all-lg",
             .w = grow ? "grow" : "100%", .h = "grow") {
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            rcBox(.bg = accent[lane], .borderRadius = "all-full", .w = "10px", .h = "10px") {}
            rcTextC(LANE_TITLE[lane], .font = F_TITLE, .color = s.text);
            rcBox(.bg = s.surface, .px = 8, .py = 2, .borderRadius = "all-full") {
                rcText(count, .font = F_SMALL, .color = s.textMuted);
            }
        }
        rcColumn(.id = scroll_id, .gap = 8, .scroll = "v", .w = "grow", .h = "grow") {
            for (int i = 0; i < static_cast<int>(cards.size()); i++)
                card(app, lane, i);
            if (cards.empty()) {
                rcBox(.align = "cc", .w = "grow", .h = "80px") {
                    rcTextL("No cards here", .font = F_SMALL, .color = s.textMuted);
                }
            }
            /* THE COMPOSER LIVES AT THE FOOT OF THE LANE IT FILLS. One buffer and
               one id move between lanes, so there is one caret on the board. */
            if (lane == addLane) {
                rcColumn(.gap = 6, .w = "grow") {
                    rcBox(.w = "grow") {
                        rcTextInput("new", input, sizeof input,
                                    .placeholder = "Card title");
                    }
                    rcRow(.gap = 6, .align = "cr", .w = "grow") {
                        if (rcButton("add", "Add card", RC_BTN_PRIMARY)
                            || (rcIsFocused("new") && rcKeyPressed(RC_KEY_ENTER)))
                            pendingAdd = true;
                    }
                }
            } else {
                rcBox(.id = add_id,
                      .bg = rcIsHovered(add_id) ? s.surface : RC_TRANSPARENT,
                      .px = 10, .py = 8, .borderRadius = "all-md", .w = "grow") {
                    rcTextL("+ Add a card", .font = F_SMALL, .color = s.textMuted);
                }
                if (rcClicked(add_id)) {
                    pendingLane = lane;
                    rcSetFocus("new");
                }
            }
        }
        /* Declared in the same scope as the container it scrolls, so it layers with it. */
        rcScrollbar(scroll_id);
    }
}

/* The one-lane arm's switcher: three segment buttons, the active one filled. */
void Board::lane_switcher(RC_App *app) {
    RC_Arena *mem = rcAppArena(app);
    rcRow(.gap = 6, .w = "grow") {
        for (int i = 0; i < LANE_COUNT; i++) {
            const char *id = rcFormat(mem, "seg%d", i).chars;
            if (rcButton(id, LANE_TITLE[i], i == shownLane ? RC_BTN_PRIMARY : RC_BTN_DEFAULT))
                shownLane = i;
        }
    }
}

void Board::layout(RC_App *app) {
    apply_pending();                            /* rule 2: mutate, THEN declare */

    RC_Arena   *mem  = rcAppArena(app);
    RC_Style    s    = rcGetStyle();
    RC_Viewport view = rcViewport();
    const bool  wide = view.breakpoint >= RC_BP_MD;
    /* A count, not a frame counter: the app still parks between clicks. */
    RC_String total = rcFormat(mem, "%d cards", totalCards);

    rcColumn(.id = "root", .bg = s.background, .gap = 12,
             .pt = static_cast<uint16_t>(16 + view.safe.top),
             .pb = static_cast<uint16_t>(16 + view.safe.bottom),
             .pl = static_cast<uint16_t>(16 + view.safe.left),
             .pr = static_cast<uint16_t>(16 + view.safe.right),
             .w = "grow", .h = "grow") {

        rcRow(.gap = 10, .align = "cl", .w = "grow") {
            rcTextL("Board", .font = F_TITLE, .color = RC_WHITE);
            rcBox(.bg = rcAlpha(RC_WHITE, 45), .px = 8, .py = 2,
                  .borderRadius = "all-full") {
                rcText(total, .font = F_SMALL, .color = rcAlpha(RC_WHITE, 220));
            }
        }

        if (wide) {
            rcRow(.gap = 12, .w = "grow", .h = "grow") {
                for (int i = 0; i < LANE_COUNT; i++)
                    lane_column(app, i, true);
            }
        } else {
            lane_switcher(app);
            lane_column(app, shownLane, false);
        }
    }

    /* A deferred change is only correct if a frame actually follows, and nothing
       else wakes an on-demand app once the pointer stops moving. */
    if (pendingAdd || pendingLane >= 0 || !pending.empty())
        rcWindowRequestFrame(rcAppMainWindow(app));
}

int main() {
    Board board;

    /* A board's own ground, light lists on it - a copy-modify of a plain struct. */
    RC_Style style = rcStyleLight();
    style.background = RC_SKY_700;
    style.surfaceAlt = RC_SLATE_100;
    style.primary    = RC_BLUE_600;
    style.radius     = 8.0f;
    rcSetStyle(style);

    /* Brace form, fields in declaration order (rule 1). The callback is a
       capture-less lambda and userData brings the Board back as `this` (rule 3). */
    RC_AppOptions opts{
        .width             = 960,
        .height            = 640,
        .title             = "Kanban - RayClay from C++",
        .fontSizes         = FONT_SIZES,
        .fontCount         = F_COUNT,
        .scratchArenaBytes = 8192,
        .layoutCallback    = [](RC_App *app, void *user) {
            static_cast<Board *>(user)->layout(app);
        },
        .userData          = &board,
    };
    return rcRunApp(&opts);
}
