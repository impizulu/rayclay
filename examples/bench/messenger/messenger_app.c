/*
    messenger_app.c - the messenger's UI.

    A desktop chat client: a conversation list with presence, unread weight and
    live search, a bubble transcript with delivery state, an anchored composer, a
    contact profile, and two modals. Three panes where they fit, one pane at a
    time below that; every branch reads the viewport, never the platform.

    Pure RC_ API - RayClay types only, no system includes (any raw-memory need
    goes through messenger_backend.h). The UI formats no strings: the backend
    hands back every timestamp, badge and state label ready to draw.

    Build: cmake --build build --target rayclay_bench_messenger
*/
#define MESSENGER_BACKEND_IMPLEMENTATION
#include "messenger_app.h"

#include "icons/rc_icons_panel_left.h"
#include "icons/rc_icons_panel_right.h"
#include "icons/rc_icons_arrow_left.h"
#include "icons/rc_icons_bell.h"

/* The scripted scenario's length: warmup frames, then a strict hold. */
#define MSG_BENCH_WARMUP 64

/* THE THREE FIXED COLUMNS, named once. They are spelled here and nowhere else,
   because the breakpoints below and the bubble cap in messenger_layout are all
   derived from them - change a pane's width in one place and everything that
   depends on it follows. */
#define MSG_RAIL_W        64.0f
#define MSG_LIST_W       300.0f
#define MSG_DRAWER_PANE_W 260.0f

/* THE TWO BREAKPOINTS, DERIVED FROM THIS APP'S OWN PANES rather than from a table
   of device widths. A thread needs about 332 px of content to stay legible, so:
   below rail + list + 332 the list and the thread take TURNS, and below that plus
   the drawer the profile takes the thread's place as a full-width sheet rather
   than being withheld. Compared against app_view_w(), so a desktop window dragged
   narrow takes the compact arm for the same reason a phone does. */
#define MSG_ONE_PANE_W (MSG_RAIL_W + MSG_LIST_W + 332.0f)
#define MSG_DRAWER_W   (MSG_ONE_PANE_W + MSG_DRAWER_PANE_W)

/* THE BUBBLE CAP, as a fraction of the thread's content width. The side a bubble
   is pinned to is what says who spoke, and a line wide enough to span the pane
   leaves the grow spacer nothing to occupy, so the fill colour is the only cue
   left. 0.80 keeps a fifth of the pane open on the free side. */
#define MSG_BUBBLE_FRAC 0.80f

/* "PAST THE END", for the thread's opening scroll. Any value beyond the real
   travel does: .scrollOffset is clamped to the content extent when it lands, so
   this asks for the bottom without the app ever measuring the backlog. Finite on
   purpose - a non-finite offset is dropped. */
#define MSG_THREAD_END 1.0e6f

/* THE MODALS' WIDTH. rcBeginModal centres its panel on the whole viewport and adds
   20 px of padding and a 1 px border a side - 42 px the body never sees. So the
   body takes whatever the viewport leaves after the safe bands, that chrome and a
   margin, capped at the desktop literal. */
#define MSG_MODAL_CHROME  42.0f
#define MSG_MODAL_MARGIN  16.0f
#define MSG_SETTINGS_W   400.0f
#define MSG_ATTACH_W     380.0f

/* THE DEMO STATUS BAR. Every view fills the foot of the screen edge to edge - a
   list row carries its badge at the right edge, a bubble reaches it, the composer
   spans it - so a floating readout has nowhere to sit that is not over content.
   The demo arm gives it a status bar of its own under the body, the way a desktop
   client does, and the fps chip docks at its right. Demo mode only. */
#define MSG_HUD_STRIP_H   32.0f
#define MSG_HUD_MARGIN     4.0f

static float modal_body_w(float cap, float avail) {
    if (avail < 0.0f)
        avail = 0.0f;               /* a sub-74 px viewport: never a negative RC_PX */
    return avail < cap ? avail : cap;
}

/* Stable ids for the rows: every interactive element needs one. */
static const char *const CONV_IDS[MSG_MAX_CONVERSATIONS] = {
    "conv00", "conv01", "conv02", "conv03", "conv04", "conv05", "conv06", "conv07",
    "conv08", "conv09", "conv10", "conv11", "conv12", "conv13", "conv14", "conv15",
};

/* Stable ids for the attach modal's picker tiles. */
static const char *const ATTACH_IDS[18] = {
    "att00", "att01", "att02", "att03", "att04", "att05",
    "att06", "att07", "att08", "att09", "att10", "att11",
    "att12", "att13", "att14", "att15", "att16", "att17",
};

/* The account avatar's tint: the one avatar that wears the brand. */
#define MSG_ME_ACCENT 0x0d9488u

/* Tile tints for the picker and the shared-media grid: zero-asset, and no emoji
   glyph, which the bundled face does not carry. */
static const uint32_t TILE_TINTS[] = {
    0x6366f1, 0x10b981, 0xf59e0b, 0xec4899, 0x8b5cf6, 0x06b6d4,
    0xef4444, 0x84cc16, 0x3b82f6, 0xf97316, 0x14b8a6, 0xa855f7,
};

/* THE SHELL: the preset re-surfaced in charcoal (dark) or warm neutral (light),
   with a teal accent. Everything else is the preset's, so a reader still
   recognises RC_Style.

   TWO TEALS, AND THEY ARE NOT INTERCHANGEABLE. `.primary` is a FILL under WHITE
   text, so it has to be dark enough to carry it: teal-700 is 5.47:1, where the
   brand teal-500 is 2.49:1 and fails AA at any size. The brand teal survives as a
   LINE and a TINT (MsgPalette.accent) because the fill shade is too dark for that
   role on a dark panel. One colour cannot be both.
   Exported because the demo runner needs it for the window's clear colour before
   the first layout runs. */
RC_Style messenger_theme(bool dark) {
    RC_Style s = dark ? rcStyleDark() : rcStyleLight();
    if (dark) {
        s.background   = rcHex(0x14171a);
        s.surface      = rcHex(0x1b1f23);
        s.surfaceAlt   = rcHex(0x262c32);
        s.chrome       = rcHex(0x101316);
        s.text         = rcHex(0xe7e5e0);
        s.textMuted    = rcHex(0x9aa0a6);
        s.border       = rcHex(0x2e353c);
        s.primary      = rcHex(0x0f766e);
        s.primaryHover = rcHex(0x115e59);
    } else {
        s.background   = rcHex(0xf6f3ee);
        s.surface      = rcHex(0xfffdfa);
        s.surfaceAlt   = rcHex(0xeae4db);
        s.chrome       = rcHex(0xfffdfa);
        s.text         = rcHex(0x24201b);
        s.textMuted    = rcHex(0x6f6862);
        s.border       = rcHex(0xdfd7cb);
        s.primary      = rcHex(0x0f766e);
        s.primaryHover = rcHex(0x115e59);
    }
    return s;
}

/* The colours RC_Style has no name for, because they are this app's vocabulary and
   not a general UI's: whose message a bubble is, whether it arrived, and whether a
   contact is there. KEY: THE OWN-MESSAGE FILL IS NOT THE BRAND ACCENT. A bubble
   painted in the primary colour competes with every button on screen, and the eye
   stops reading "mine" and starts reading "action". */
typedef struct {
    RC_Color ownBg;     /* own-message fill - a blue of its own, never s.primary */
    RC_Color ownText;
    RC_Color ownMeta;   /* timestamp + delivery mark inside an own bubble        */
    RC_Color failBg;    /* a failed own message keeps the shape, loses the blue  */
    RC_Color failText;
    RC_Color rose;      /* failure wherever it is a mark rather than a fill      */
    RC_Color online;
    RC_Color away;      /* also the "queued" mark: amber reads as waiting, not lost */
    RC_Color offline;
    RC_Color accent;    /* the BRAND teal as a line/tint, never as a fill under white */
} MsgPalette;

static MsgPalette msg_palette(bool dark) {
    if (dark)
        return (MsgPalette){
            .ownBg = rcHex(0x2f4f7a), .ownText = rcHex(0xf2f6fb),
            .ownMeta = rcAlpha(RC_WHITE, 175),
            .failBg = rcHex(0x4a2230), .failText = rcHex(0xffe4e9),
            .rose = rcHex(0xfb7185),
            .online = rcHex(0x22c55e), .away = rcHex(0xf59e0b),
            .offline = rcHex(0x6b7280),
            .accent = rcHex(0x14b8a6),   /* 5.67:1 on the active rail surface */
        };
    return (MsgPalette){
        .ownBg = rcHex(0xd7e6f7), .ownText = rcHex(0x17334f),
        .ownMeta = rcHex(0x5b7690),
        .failBg = rcHex(0xfbe1e6), .failText = rcHex(0x7a1d2e),
        .rose = rcHex(0xe11d48),
        .online = rcHex(0x16a34a), .away = rcHex(0xd97706),
        .offline = rcHex(0x9ca3af),
        .accent = rcHex(0x0f766e),       /* 4.33:1 on the light active surface  */
    };
}

/* Your own presence: the dropdown's three entries are the three states in
   order, so its index IS the presence. */
static uint8_t me_presence(int statusCombo) {
    if (statusCombo == 1) return MSG_PRESENCE_AWAY;
    if (statusCombo == 2) return MSG_PRESENCE_OFFLINE;
    return MSG_PRESENCE_ONLINE;
}

static RC_Color presence_color(const MsgPalette *pal, uint8_t presence) {
    if (presence == MSG_PRESENCE_ONLINE)
        return pal->online;
    return presence == MSG_PRESENCE_AWAY ? pal->away : pal->offline;
}

/* A round, procedural avatar: a tinted circle with the contact's initials. */
static void avatar_circle(const char *initials, uint32_t accent, float size,
                          const MsgPalette *pal, int presence) {
    rcBox(.bg = rcHex(accent), .align = "cc", .borderRadius = "all-full",
          .wType = RC_PX(size), .hType = RC_PX(size)) {
        rcTextC(initials, .font = F_SMALL, .color = RC_WHITE);
        /* The pip is DECORATION of the avatar, so RC_CLIP_TO_PARENT makes it scroll
           out with the row it belongs to instead of painting over the search field.
           A ring in the surrounding fill keeps a green pip legible on a green
           avatar. */
        if (presence >= 0)
            rcBox(.bg = presence_color(pal, (uint8_t)presence),
                  .borderRadius = "all-full",
                  .border = { .color = rcGetStyle().surface, .width = "all-2" },
                  .wType = RC_PX(size * 0.30f), .hType = RC_PX(size * 0.30f),
                  .floating = { .to = RC_ATTACH_PARENT,
                                .parent = RC_ANCHOR_BOTTOM_RIGHT,
                                .element = RC_ANCHOR_BOTTOM_RIGHT,
                                .capture = RC_CAPTURE_PASSTHROUGH,
                                .clip = RC_CLIP_TO_PARENT }) {}
    }
}

/* A nav-rail icon button. The active tint is pal->accent, not s.primary: an icon
   is a mark on a surface, and the fill shade is too dark for that. */
static bool nav_button(const char *id, RC_IconCallback icon, bool active,
                       const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    rcBox(.id = id,
          .bg = active ? s.surfaceAlt : (rcIsHovered(id) ? s.surface : RC_TRANSPARENT),
          .align = "cc", .borderRadius = "all-lg", .w = "44px", .h = "44px") {
        icon(20.0f, active ? pal->accent : s.textMuted);
    }
    return rcClicked(id);
}

/* One conversation-header action. Polling rcClicked marks the whole box
   clickable, and the pointer cursor comes with that - no cursor call needed. */
static bool hdr_button(const char *id, RC_IconCallback icon, bool on,
                       const MsgPalette *pal, const char *tip) {
    RC_Style s = rcGetStyle();
    rcBox(.id = id, .bg = rcIsHovered(id) ? s.surfaceAlt : RC_TRANSPARENT,
          .align = "cc", .borderRadius = "all-md", .w = "40px", .h = "40px",
          .tooltip = tip) {
        icon(18.0f, on ? pal->accent : s.textMuted);
    }
    return rcClicked(id);
}

/* A sidebar conversation row; the whole row is the click target. */
static bool conv_row(const char *id, const MsgConversation *c, bool active,
                     const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    if (!c)                               /* out-of-range query returns NULL; row is inert */
        return false;
    /* UNREAD IS A WEIGHT, NOT JUST A BADGE: the ink is read before the number. */
    const bool unread = (c->unread > 0);
    rcRow(.id = id,
          .bg = active ? s.surfaceAlt : (rcIsHovered(id) ? s.surface : RC_TRANSPARENT),
          .gap = 10, .pl = 5, .pr = 8, .align = "cl", .borderRadius = "all-md",
          .w = "grow", .h = "62px") {
        /* SELECTION IS A BACKGROUND PLUS A MARK, never a background alone: a tint
           is one channel, and it is the channel a colour-blind user and a dimmed
           screen lose first. The bar holds the row's left gutter in both states,
           transparent when unselected, so nothing reflows as the selection moves. */
        rcBox(.bg = active ? pal->accent : RC_TRANSPARENT, .borderRadius = "all-full",
              .w = "3px", .h = "34px") {}
        avatar_circle(c->initials, c->accent, 42.0f, pal, (int)c->presence);
        rcColumn(.gap = 3, .w = "grow") {
            rcRow(.gap = 6, .align = "cl", .w = "grow") {
                rcBox(.overflow = "hidden", .w = "grow") {
                    rcTextC(c->name, .font = F_BODY, .color = s.text, .wrap = "n");
                }
                rcTextC(c->lastTs, .font = F_SMALL, .color = s.textMuted);
            }
            rcRow(.gap = 6, .align = "cl", .w = "grow") {
                rcBox(.overflow = "hidden", .w = "grow") {
                    rcTextC(c->preview, .font = F_SMALL,
                            .color = unread ? s.text : s.textMuted, .wrap = "n");
                }
                if (c->badge[0]) {
                    rcBox(.bg = s.primary, .px = 7, .py = 1, .align = "cc",
                           .borderRadius = "all-full") {
                        rcTextC(c->badge, .font = F_SMALL, .color = RC_WHITE);
                    }
                }
            }
        }
    }
    return rcClicked(id);
}

/* One message bubble: outgoing right, incoming left. A grow spacer on the free
   side pins it; maxW caps how wide it may run (0 = uncapped). */
static void bubble(const MsgMessage *m, const MsgPalette *pal, float maxW,
                   bool receipts) {
    if (!m)                               /* out-of-range query returns NULL; bubble is inert */
        return;
    RC_Style s = rcGetStyle();
    bool out = (m->dir == MSG_DIR_OUTGOING);
    bool bad = out && (m->deliv == MSG_DELIV_FAILED);
    RC_Color fill = !out ? s.surfaceAlt : (bad ? pal->failBg  : pal->ownBg);
    RC_Color ink  = !out ? s.text       : (bad ? pal->failText : pal->ownText);
    RC_Color meta = !out ? s.textMuted  : (bad ? pal->rose     : pal->ownMeta);
    /* THE DELIVERY MARK ANSWERS "DID IT LEAVE?", so it exists only on messages the
       user sent, and only while the answer is interesting: a thread that spells
       "sent" on every line has said nothing. Failure takes rose, queued amber.
       The receipts setting turns the mark off entirely. */
    const bool showMark = out && receipts && m->deliv != MSG_DELIV_SENT;
    RC_Color mark = meta;
    if (m->deliv == MSG_DELIV_FAILED)
        mark = pal->rose;
    else if (m->deliv == MSG_DELIV_OFFLINE)
        mark = pal->away;
    /* Content-sized, so a one-word reply is a pill and not a half-width block. */
    rcRow(.gap = 0, .align = "cl", .w = "grow") {
        if (out)
            rcBox(.w = "grow") {}
        rcRow(.bg = fill, .gap = 8, .px = 12, .py = 7, .align = "cr",
              .borderRadius = "all-lg", .wMax = maxW) {
            rcTextC(m->text, .font = F_BODY, .color = ink);
            rcTextC(m->ts, .font = F_SMALL, .color = meta);
            if (showMark)
                rcTextC(msg_delivery_label(m->deliv), .font = F_SMALL, .color = mark);
        }
        if (!out)
            rcBox(.w = "grow") {}
    }
}

/* A "typing" bubble on the app's own frame tick, never a wall clock. */
static void typing_indicator(uint64_t tick) {
    RC_Style s = rcGetStyle();
    static const char *const DOTS[4] = { "", ".", "..", "..." };
    rcRow(.gap = 8, .align = "tl", .w = "grow") {
        rcRow(.bg = s.surfaceAlt, .gap = 1, .px = 12, .py = 8, .align = "cl",
              .borderRadius = "all-lg") {
            rcTextL("typing", .font = F_SMALL, .color = s.textMuted);
            rcTextC(DOTS[(tick / 20) % 4], .font = F_SMALL, .color = s.textMuted);
        }
        rcBox(.w = "grow") {}
    }
}

/* The custom titlebar; RC_ID_WINDOW_DRAG makes the band draggable. */
static void msg_topbar(AppState *st, bool onePane, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    /* CHROME, NOT CONTENT: the drag strip is pinned at .titlebarHeight, so a band
       that scaled with the content zoom would stop matching it. rcUnzoomed()
       counter-scales the row it prefixes; at zoom 1 it changes nothing. */
    rcUnzoomed()
    rcRow(.bg = s.chrome, .gap = 10, .px = 14, .align = "cl", .w = "grow", .h = "52px") {
        /* Only the brand and the empty stretch are the drag handle: a widget inside
           RC_ID_WINDOW_DRAG loses its click to the window move. */
        rcRow(.id = RC_ID_WINDOW_DRAG, .gap = 10, .align = "cl", .w = "grow",
              .h = "grow") {
            rcBox(.bg = s.primary, .align = "cc", .borderRadius = "all-md", .w = "26px",
                  .h = "26px") {
                rcTextL("R", .font = F_BODY, .color = RC_WHITE);
            }
            /* Narrow, the width belongs to the conversation, so the name shortens. */
            if (onePane)
                rcTextL("Messenger", .font = F_HEAD, .color = s.text);
            else
                rcTextL("RayClay Messenger", .font = F_HEAD, .color = s.text);
        }
        /* No rail narrow, so the account lives in the titlebar, which costs
           no width at all. */
        if (onePane) {
            rcBox(.id = "top_me", .align = "cc", .tooltip = "You - settings") {
                avatar_circle("ME", MSG_ME_ACCENT, 32.0f, pal,
                              (int)me_presence(st->statusCombo));
            }
            if (rcClicked("top_me")) {
                st->modalSettings = true;
                st->modalAttach   = false;
            }
            /* Two teal controls at a 10 px gap read as one; this separates them. */
            rcBox(.bg = s.border, .w = "1px", .h = "24px") {}
        }
        rcRow(.gap = 8, .align = "cl") {
            if (!onePane)
                rcTextC(st->darkMode ? "Dark" : "Light", .font = F_SMALL,
                        .color = s.textMuted);
            rcToggle("tg_theme", &st->darkMode);
        }
        rcWindowControls();
    }
}

/* The nav rail, WIDE ARM ONLY: a permanent column is an easy trade beside a list
   and a thread, and a bad one on a phone, so the narrow arm hands its destinations
   to the topbar and the thread's back control.
   Its ground is s.background, DEEPER than the list beside it - two icons at
   opposite ends of a column only read as one rail when the rail has a ground of
   its own.

   TRAP: .border.width is a fixed char[] holding ONE all-sides width ("1",
   "all-1"). A per-side spelling like "0 1px 0 0" fits the buffer, parses to
   nothing and draws NOTHING, silently. Per-side edges go through the class route
   instead, where a bare `border-r` is a 1 px default and .border.color still
   supplies the colour. */
static void msg_navrail(AppState *st, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    /* The rule on the right comes from the "border-r" class, which carries BOTH the
       width and the colour. A .border colour with no .width is dead - the whole
       border block is skipped unless a width is spelled - so writing one here would
       look deliberate and do nothing. */
    rcColumn(.bg = s.background, .gap = 6, .py = 12, .align = "tc",
             .h = "grow", .wType = RC_PX(MSG_RAIL_W), .className = "border-r") {
        /* Collapse or expand the list; the button reads active while it shows. */
        if (nav_button("nav_toggle", rcIconPanelLeft, !st->sidebarCollapsed, pal))
            st->sidebarCollapsed = !st->sidebarCollapsed;
        rcBox(.w = "grow", .h = "grow") {}
        /* The account avatar opens settings, so the rail carries nothing inert. */
        rcBox(.id = "nav_me", .align = "cc", .tooltip = "You - settings") {
            avatar_circle("ME", MSG_ME_ACCENT, 40.0f, pal,
                          (int)me_presence(st->statusCombo));
        }
        if (rcClicked("nav_me")) {
            st->modalSettings = true;
            st->modalAttach   = false;
        }
    }
}

/* The conversation list. Alone on the screen it IS the screen, so it grows. */
static void msg_sidebar(AppState *st, bool onePane, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    const char *total = msg_unread_badge(&st->store);
    rcColumn(.bg = s.surface, .gap = 8, .p = 10, .h = "grow",
             .wType = onePane ? RC_GROW : RC_PX(MSG_LIST_W)) {
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            rcTextL("Chats", .font = F_TITLE, .color = s.text);
            /* An empty string means no chip, never a chip reading zero. */
            if (total[0])
                rcBox(.bg = s.primary, .px = 8, .py = 1, .align = "cc",
                      .borderRadius = "all-full") {
                    rcTextC(total, .font = F_SMALL, .color = RC_WHITE);
                }
        }
        rcTextInput("search", st->search, sizeof st->search, .placeholder = "Search conversations");
        rcColumn(.id = "SideScroll", .gap = 2, .pr = 12, .scroll = "v", .w = "grow",
                 .h = "grow") {
            int n = msg_conversation_count(&st->store);
            int shown = 0;
            for (int i = 0; i < n; i++) {
                const MsgConversation *c = msg_conversation_at(&st->store, i);
                if (c && !msg_name_matches(c->name, st->search))
                    continue;                 /* the search box filters the list live */
                shown++;
                if (conv_row(CONV_IDS[i], c, i == st->openConv, pal)) {
                    st->openConv = i;
                    msg_mark_read(&st->store, i);
                    /* One pane at a time: picking a chat IS the navigation. It
                       lands on the THREAD, because an info sheet left open from
                       the previous chat is a level deeper than the row just
                       tapped. */
                    if (onePane) {
                        st->sidebarCollapsed = true;
                        st->infoOpen         = false;
                    }
                }
            }
            if (!shown)
                rcTextC("No conversations match.", .font = F_SMALL, .color = s.textMuted);
        }
    }
}

static void msg_thread(AppState *st, bool onePane, float bubbleMax,
                       const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    const MsgConversation *c = msg_conversation_at(&st->store, st->openConv);
    if (!c)
        return;                     /* no open conversation: render nothing (defensive) */

    rcColumn(.bg = s.background, .w = "grow", .h = "grow") {
        /* The header: the contact, and the actions for this conversation. */
        rcRow(.bg = s.chrome, .gap = 4, .px = 10, .align = "cl", .w = "grow",
              .h = "60px") {
            /* THE WAY BACK, and the only control the narrow arm adds: the list and
               the thread take turns there. The wide arm needs none - the list is on
               screen beside the thread. */
            if (onePane) {
                rcBox(.id = "thr_back",
                      .bg = rcIsHovered("thr_back") ? s.surfaceAlt : RC_TRANSPARENT,
                      .align = "cc", .borderRadius = "all-md", .w = "40px", .h = "44px",
                      .tooltip = "Back to chats") {
                    rcIconArrowLeft(20.0f, s.text);
                }
                if (rcClicked("thr_back")) {
                    st->sidebarCollapsed = false;   /* the list is the pane again */
                    st->infoOpen         = false;
                }
            }
            /* The profile chip is only as wide as the contact's name: a hover
               highlight stretched across an empty half-header reads as a button
               the size of the room. The actions take the right edge, where every
               chat client a general consumer has used already puts them. */
            rcRow(.id = "hdr_profile", .bg = st->infoOpen ? s.surfaceAlt
                       : (rcIsHovered("hdr_profile") ? s.surface : RC_TRANSPARENT),
                  .gap = 10, .px = 6, .py = 5, .align = "cl", .borderRadius = "all-md",
                  .tooltip = "View profile") {
                avatar_circle(c->initials, c->accent, 40.0f, pal, (int)c->presence);
                rcColumn(.gap = 1) {
                    rcTextC(c->name, .font = F_HEAD, .color = s.text, .wrap = "n");
                    rcRow(.gap = 5, .align = "cl") {
                        rcBox(.bg = presence_color(pal, c->presence),
                              .borderRadius = "all-full", .w = "8px", .h = "8px") {}
                        rcTextC(msg_presence_label(c->presence), .font = F_SMALL,
                                .color = s.textMuted);
                    }
                }
            }
            if (rcClicked("hdr_profile"))
                st->infoOpen = !st->infoOpen;
            rcBox(.w = "grow") {}
            /* Muting is the volume at zero: the bell and the settings slider
               are two views of one number. */
            const bool loud = (st->notifVolume > 0.0f);
            if (hdr_button("thr_mute", rcIconBell, loud, pal,
                           loud ? "Mute notifications" : "Unmute notifications"))
                st->notifVolume = loud ? 0.0f : 0.7f;
            if (hdr_button("thr_info", rcIconPanelRight, st->infoOpen, pal,
                           "Contact profile"))
                st->infoOpen = !st->infoOpen;
        }
        /* A CHAT OPENS ON ITS NEWEST LINE, which no reader should have to scroll
           down to find. Asking for an offset past the end gets it without the app
           ever measuring the backlog - it could not measure it here anyway, since
           a container's extent is still zero on the frame it is declared.
           .scrollOffset is taken only when the container is FRESH and is ignored
           while it is live, so this opens the thread at the bottom and then never
           fights the user's own scrolling. */
        rcColumn(.id = "ThreadScroll", .gap = 8, .p = 16, .scroll = "v", .w = "grow",
                 .h = "grow", .scrollOffset = { 0.0f, MSG_THREAD_END }) {
            /* A SHORT THREAD SITS AT THE BOTTOM, beside the composer, not stranded
               at the top of an empty pane. This spacer takes the leftover height and
               collapses to nothing once the backlog is taller than the pane, so the
               scroll range is untouched. */
            rcBox(.w = "grow", .h = "grow") {}
            int n = msg_thread_count(&st->store, st->openConv);
            for (int i = 0; i < n; i++)
                bubble(msg_thread_at(&st->store, st->openConv, i), pal, bubbleMax,
                       st->readReceipts);
            /* Only the open chat types, so the app does not claim they all do. */
            if (st->openConv == 0)
                typing_indicator(st->tick);
        }
        /* ANCHORED: a sibling of the thread, not a row inside it. */
        rcBox(.bg = s.border, .w = "grow", .h = "1px") {}
        /* The thread's worst delivery state, and somewhere to retry it from. */
        uint8_t worst = msg_worst_delivery(&st->store, st->openConv);
        if (worst != MSG_DELIV_SENT) {
            bool failed = (worst == MSG_DELIV_FAILED);
            rcRow(.bg = failed ? pal->failBg : s.surfaceAlt, .gap = 8, .px = 12, .py = 8,
                  .align = "cl", .w = "grow") {
                rcBox(.bg = failed ? pal->rose : pal->away, .borderRadius = "all-full",
                      .w = "8px", .h = "8px") {}
                rcTextC(failed ? "A message did not send."
                               : "Queued - waiting for a connection.",
                        .font = F_SMALL, .color = failed ? pal->failText : s.text);
                rcBox(.w = "grow") {}
                if (failed) {
                    rcBox(.id = "btn_retry",
                          .bg = rcIsHovered("btn_retry") ? pal->rose : RC_TRANSPARENT,
                          .px = 10, .py = 3, .align = "cc", .borderRadius = "all-full",
                          .border = { .color = pal->rose, .width = "all-1" }) {
                        rcTextL("Retry", .font = F_SMALL,
                                .color = rcIsHovered("btn_retry") ? RC_WHITE : pal->rose);
                    }
                    if (rcClicked("btn_retry"))
                        msg_retry_failed(&st->store, st->openConv);
                }
            }
        }
        rcRow(.bg = s.surface, .gap = 8, .p = 10, .align = "cl", .w = "grow") {
            rcBox(.id = "btn_attach",
                  .bg = rcIsHovered("btn_attach") ? s.surfaceAlt : RC_TRANSPARENT,
                  .align = "cc", .borderRadius = "all-md", .w = "40px", .h = "40px",
                  .tooltip = "Attach a photo") {
                rcTextL("+", .font = F_TITLE, .color = s.textMuted);
            }
            if (rcClicked("btn_attach")) {
                st->modalAttach   = true;
                st->modalSettings = false;
            }
            rcBox(.w = "grow") {
                rcTextInput("composer", st->composer, sizeof st->composer,
                             .placeholder = "Message");
            }
            if (rcButton("btn_send", "Send", RC_BTN_PRIMARY) && st->composer[0]) {
                int len = 0;
                while (st->composer[len])
                    len++;
                msg_send_text(&st->store, st->openConv, st->composer, len);
                st->composer[0] = '\0';
            }
        }
    }
}

/* The contact's profile. TWO SHAPES, ONE STATE: st->infoOpen says the user asked
   for it and the width decides how it is shown - a drawer beside the thread where
   it fits, or a full-width sheet with a back row where it does not. The body is
   shared; only the shell differs, spelled out twice because .scroll is a fixed
   char[] and cannot take a ternary. */
static void info_body(AppState *st, const MsgConversation *c, bool sheet,
                      const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    avatar_circle(c->initials, c->accent, 72.0f, pal, (int)c->presence);
    rcTextC(c->name, .font = F_HEAD, .color = s.text);
    rcRow(.gap = 5, .align = "cc") {
        rcBox(.bg = presence_color(pal, c->presence), .borderRadius = "all-full",
              .w = "8px", .h = "8px") {}
        rcTextC(msg_presence_label(c->presence), .font = F_SMALL, .color = s.textMuted);
    }
    rcBox(.bg = s.border, .w = "grow", .h = "1px") {}
    rcRow(.align = "cl", .w = "grow") {
        rcTextL("Shared media", .font = F_SMALL, .color = s.textMuted);
    }
    rcColumn(.gap = 6, .w = "grow") {
        for (int r = 0; r < 3; r++) {
            rcRow(.gap = 6, .w = "grow") {
                for (int col = 0; col < 3; col++) {
                    /* In a sheet the column is the screen, so the tile takes its
                       width as its height - a short wide tile is a stripe. */
                    if (sheet)
                        rcBox(.bg = rcHex(TILE_TINTS[(r * 3 + col) % 12]),
                              .borderRadius = "all-md", .w = "grow", .aspectRatio = 1.0f) {}
                    else
                        rcBox(.bg = rcHex(TILE_TINTS[(r * 3 + col) % 12]),
                              .borderRadius = "all-md", .w = "grow", .h = "52px") {}
                }
            }
        }
    }
    /* The same setting the settings modal carries, on the chat it applies to. */
    rcRow(.gap = 8, .align = "cl", .w = "grow") {
        rcTextL("Delivery receipts", .font = F_SMALL, .color = s.text);
        rcBox(.w = "grow") {}
        rcToggle("tg_receipts_info", &st->readReceipts);
    }
}

static void msg_info_pane(AppState *st, bool sheet, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    const MsgConversation *c = msg_conversation_at(&st->store, st->openConv);
    if (!c)
        return;                     /* no open conversation: render nothing (defensive) */
    if (!sheet) {
        rcColumn(.bg = s.surface, .gap = 12, .p = 16, .align = "tc", .h = "grow",
                 .wType = RC_PX(MSG_DRAWER_PANE_W)) {
            info_body(st, c, false, pal);
        }
        return;
    }
    rcColumn(.bg = s.surface, .w = "grow", .h = "grow") {
        /* THE ONLY CONTROL THE SHEET ADDS: the way back to the thread it replaced,
           44 px tall for a finger. It sits ABOVE the scrolling body, not in it, so
           it cannot scroll out of reach. The drawer needs none - the header button
           that opened it is still on screen. */
        rcBox(.pt = 16, .px = 16, .w = "grow") {
            rcRow(.id = "info_back",
                  .bg = rcIsHovered("info_back") ? s.surfaceAlt : RC_TRANSPARENT,
                  .gap = 8, .px = 6, .align = "cl", .borderRadius = "all-md", .w = "grow",
                  .h = "44px") {
                rcIconArrowLeft(18.0f, s.textMuted);
                rcTextL("Back to chat", .font = F_BODY, .color = s.textMuted);
            }
        }
        if (rcClicked("info_back"))
            st->infoOpen = false;
        rcColumn(.id = "InfoScroll", .gap = 12, .p = 16, .align = "tc", .scroll = "v",
                 .w = "grow", .h = "grow") {
            info_body(st, c, true, pal);
        }
    }
}

/* The modals, placed OUTSIDE the root so their scrim covers the whole window.
   bodyAvail is what the viewport leaves after the safe bands, the panel chrome and
   the margins; each body takes the smaller of that and its desktop width. */
static void msg_modals(AppState *st, float bodyAvail, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();

    if (rcBeginModal("modal_attach", &st->modalAttach)) {
        static const char SEND_ORIG[]  = "Photo - original quality";
        static const char SEND_SMALL[] = "Photo";
        rcColumn(.bg = s.surface, .gap = 12, .p = 18, .borderRadius = "all-xl",
                 .wType = RC_PX(modal_body_w(MSG_ATTACH_W, bodyAvail))) {
            rcTextL("Attach", .font = F_TITLE, .color = s.text);
            /* The line under the title reports what Send will actually do. */
            rcTextC(st->attachOriginal ? "1 photo, at its original size"
                                       : "1 photo, reduced for sending",
                    .font = F_SMALL, .color = s.textMuted);
            /* The tiles ARE the picker: one is always chosen and the ring says
               which. Two element heads, because .border.width takes a literal. */
            rcColumn(.gap = 6, .w = "grow") {
                for (int r = 0; r < 3; r++) {
                    rcRow(.gap = 6, .w = "grow") {
                        for (int col = 0; col < 6; col++) {
                            int k = r * 6 + col;
                            const char *id = ATTACH_IDS[k];
                            if (k == st->attachPick)
                                rcBox(.id = id, .bg = rcHex(TILE_TINTS[k % 12]),
                                      .borderRadius = "all-md",
                                      .border = { .color = s.primary, .width = "all-3" }, .w = "grow", .h = "40px") {}
                            else
                                rcBox(.id = id, .bg = rcHex(TILE_TINTS[k % 12]),
                                      .borderRadius = "all-md", .w = "grow", .h = "40px",
                                      .overlay = rcIsHovered(id) ? rcAlpha(RC_WHITE, 45)
                                                                 : RC_TRANSPARENT) {}
                            if (rcClicked(id))
                                st->attachPick = k;
                        }
                    }
                }
            }
            rcCheckbox("cb_origq", "Send at original quality", &st->attachOriginal);
            rcRow(.gap = 8) {
                if (rcButton("btn_attach_send", "Send", RC_BTN_PRIMARY)) {
                    const char *text = st->attachOriginal ? SEND_ORIG : SEND_SMALL;
                    int len = st->attachOriginal ? (int)sizeof SEND_ORIG - 1
                                                 : (int)sizeof SEND_SMALL - 1;
                    msg_send_text(&st->store, st->openConv, text, len);
                    st->modalAttach = false;
                }
                if (rcButton("btn_attach_cancel", "Cancel", RC_BTN_DEFAULT))
                    st->modalAttach = false;
            }
        }
        rcEndModal();
    }

    if (rcBeginModal("modal_settings", &st->modalSettings)) {
        /* Its index is the presence the rail and the topbar draw (me_presence). */
        static const char *const statuses[] = { "Online", "Away", "Offline" };
        rcColumn(.bg = s.surface, .gap = 14, .p = 18, .borderRadius = "all-xl",
                 .wType = RC_PX(modal_body_w(MSG_SETTINGS_W, bodyAvail))) {
            rcTextL("Settings", .font = F_TITLE, .color = s.text);
            rcRow(.align = "cl", .w = "grow") {
                rcTextL("Delivery receipts", .font = F_BODY, .color = s.text);
                rcBox(.w = "grow") {}
                rcToggle("tg_receipts", &st->readReceipts);
            }
            rcRow(.gap = 12, .align = "cl", .w = "grow") {
                rcBox(.w = "150px") {
                    rcTextL("Notification volume", .font = F_BODY, .color = s.text);
                }
                rcBox(.w = "grow") { rcSlider("sl_vol", &st->notifVolume, 0.0f, 1.0f); }
                rcBox(.align = "cr", .w = "44px") {
                    rcTextC(msg_volume_label(st->notifVolume), .font = F_SMALL,
                            .color = s.textMuted);
                }
            }
            rcRow(.gap = 12, .align = "cl", .w = "grow") {
                rcBox(.w = "150px") {
                    rcTextL("Status", .font = F_BODY, .color = s.text);
                }
                rcBox(.w = "grow") { rcCombo("cb_status", &st->statusCombo, statuses, 3); }
                rcBox(.bg = presence_color(pal, me_presence(st->statusCombo)),
                      .borderRadius = "all-full", .w = "10px", .h = "10px") {}
            }
            rcRow(.gap = 8) {
                if (rcButton("btn_settings_done", "Done", RC_BTN_PRIMARY))
                    st->modalSettings = false;
            }
        }
        rcEndModal();
    }
}

void messenger_seed(AppState *st, unsigned seed) {
    msg_memzero(st, sizeof *st);        /* B2: zero all (incl. padding) THEN set fields */
    msg_store_seed(&st->store, seed);
    st->openConv       = 0;
    st->darkMode       = true;
    st->readReceipts   = true;
    st->attachOriginal = true;
    st->attachPick     = 0;             /* the picker always has one photo chosen */
    st->notifVolume    = 0.7f;
    st->statusCombo    = 0;             /* online */
    st->seeded         = true;
    msg_mark_read(&st->store, st->openConv);
}

void messenger_update(AppState *st, const AppCtx *ctx) {
    msg_store_step(&st->store, ctx->dt);   /* dt <= 0 (freeze) => a no-op */
    if (ctx->dt > 0.0f)
        st->tick++;                        /* the app clock; pinned at freeze */
}

void messenger_layout(AppState *st, const AppCtx *ctx) {
    rcSetStyle(messenger_theme(st->darkMode));
    RC_Style s = rcGetStyle();
    const MsgPalette pal = msg_palette(st->darkMode);

    /* Branch on the SPACE: a window dragged narrow is a phone's problem too. */
    const float viewW      = app_view_w(ctx);
    const bool  onePane    = viewW < MSG_ONE_PANE_W;
    const bool  drawerFits = viewW >= MSG_DRAWER_W;
    /* One bool of state, two shapes: a drawer beside the thread, or a sheet. */
    const bool  infoSheet  = st->infoOpen && !drawerFits;
    /* The demo's status bar: a readout on a hand-tuned offset clears the content
       at one size and lands on it at every other. */
    const bool  hudStrip   = ctx->mode == APP_DEMO;

    /* Safe area in layout units - a phone's status bar, cutout and home indicator.
       Zero on desktop, so Root's padding is a no-op there. */
    RC_Insets safe = app_safe(ctx);
    const float modalAvail = viewW - safe.left - safe.right - MSG_MODAL_CHROME
                           - 2.0f * MSG_MODAL_MARGIN;
    /* The widest a bubble may draw, derived from what the THREAD actually gets:
       the pane is at its narrowest not on a phone but on a desktop window just
       above the one-pane line, where the fixed columns still take their share and
       only the thread gives way. Spend those same named widths, then
       ThreadScroll's own 16 px gutters. */
    float threadW = viewW - safe.left - safe.right - 2.0f * 16.0f;
    if (!onePane) {
        threadW -= MSG_RAIL_W;
        if (!st->sidebarCollapsed)      threadW -= MSG_LIST_W;
        if (st->infoOpen && drawerFits) threadW -= MSG_DRAWER_PANE_W;
    }
    const float bubbleMax = threadW > 0.0f ? threadW * MSG_BUBBLE_FRAC : 0.0f;

    rcColumn(.id = "Root", .bg = s.background, .pt = (uint16_t)safe.top,
             .pb = (uint16_t)safe.bottom, .pl = (uint16_t)safe.left,
             .pr = (uint16_t)safe.right, .w = "grow", .h = "grow") {
        msg_topbar(st, onePane, &pal);
        rcRow(.id = "Body", .w = "grow", .h = "grow") {
            /* THE RAIL IS A WIDE-ARM ORGAN: a permanent column is permanent, and
               on a phone it costs a seventh of the screen on every screen. Narrow,
               the topbar and the thread header carry its destinations. */
            if (!onePane)
                msg_navrail(st, &pal);
            if (!st->sidebarCollapsed)
                msg_sidebar(st, onePane, &pal);
            /* Wide: list AND thread. One pane: whichever the user is on - the
               thread is what the collapsed sidebar reveals. Where the drawer
               cannot sit beside the thread the profile takes the thread's slot as
               a sheet, rather than being withheld and leaving its button dead. */
            if (!onePane || st->sidebarCollapsed) {
                if (infoSheet)
                    msg_info_pane(st, true, &pal);
                else
                    msg_thread(st, onePane, bubbleMax, &pal);
            }
            if (st->infoOpen && drawerFits)
                msg_info_pane(st, false, &pal);
        }
        /* A status bar: a connection state from the outbox, and room for the chip. */
        if (hudStrip) {
            rcBox(.bg = s.border, .w = "grow", .h = "1px") {}
            rcRow(.id = "hud_strip", .bg = s.chrome, .gap = 6, .px = 12, .align = "cl",
                  .w = "grow", .hType = RC_PX(MSG_HUD_STRIP_H)) {
                const bool offline =
                    msg_worst_delivery(&st->store, st->openConv) == MSG_DELIV_OFFLINE;
                rcBox(.bg = offline ? pal.away : pal.online, .borderRadius = "all-full",
                      .w = "8px", .h = "8px") {}
                rcTextC(offline ? "Reconnecting" : "Connected", .font = F_SMALL,
                        .color = s.textMuted);
            }
        }
    }
    msg_modals(st, modalAvail, &pal);   /* modals sit outside Root (full-window scrim) */

    /* EACH BAR CARRIES THE CONDITION ITS PANE CARRIES. rcScrollbar draws the bar
       for a container laid out THIS frame; name one this frame did not build and
       the call is dropped with a warning. These three are the Body block above read
       back, arm for arm - change one there and change it here. */
    const bool threadPane = (!onePane || st->sidebarCollapsed);
    if (!st->sidebarCollapsed) rcScrollbar("SideScroll");
    if (threadPane && !infoSheet) rcScrollbar("ThreadScroll");
    if (threadPane && infoSheet) rcScrollbar("InfoScroll");
}

void messenger_demo_chrome(AppState *st, const AppCtx *ctx) {
    if (ctx->mode != APP_DEMO || !ctx->arena)
        return;
    /* Floating, so it never reflows the UI, and PASSTHROUGH, so clicks fall
       through it. */
    RC_String hud = rcFormat(ctx->arena, "%.0f fps \xc2\xb7 %d chats \xc2\xb7 %d unread",
                                ctx->fps,
                                msg_conversation_count(&st->store),
                                msg_unread_total(&st->store));
    const RC_Insets safe = app_safe(ctx);
    rcBox(.id = "demo_hud", .bg = rcAlpha(RC_BLACK, 150), .px = 10, .py = 5,
           .borderRadius = "all-full",
           .floating = { .to = RC_ATTACH_ROOT, .parent = RC_ANCHOR_BOTTOM_RIGHT,
                         .element = RC_ANCHOR_BOTTOM_RIGHT,
                         .offset = { -16.0f - safe.right,
                                     -(safe.bottom + MSG_HUD_MARGIN) },
                         .capture = RC_CAPTURE_PASSTHROUGH }) {
        rcText(hud, .font = F_SMALL, .color = RC_WHITE);
    }
}

void messenger_bench_step(AppState *st, const AppInputSink *in, int frame) {
    /* The scripted scenario. Every action goes through the input sink, so the real
       hit-test, focus and caret paths run; only the incoming message is a direct
       backend call. At MSG_BENCH_WARMUP the app holds, a strict no-op.
       The caret blink and tooltip dwell run on real time, not the injected dt, so
       the held frame must carry no focused input and no hovered tooltip - hence the
       blur and off-canvas park below. */
    if (!in || frame >= MSG_BENCH_WARMUP)
        return;                                        /* the HOLD */

    if (frame == 0) {
        in->move(in->ctx, 700.0f, 300.0f);             /* park the pointer over the thread so the wheel targets it */
    } else if (frame < 12) {
        in->wheel(in->ctx, 0.0f, -3.0f);               /* scroll the thread across the backlog */
    } else if (frame == 18) {
        in->move(in->ctx, 700.0f, 690.0f);             /* focus the composer: press ... */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == 19) {
        in->button(in->ctx, APP_MBTN_LEFT, false);      /* ... release (a real click edge) */
    } else if (frame >= 22 && frame < 46) {
        static const char TYPED[] = "great work on the trend!";  /* 24 ASCII chars, one/frame */
        in->text(in->ctx, (unsigned int)(unsigned char)TYPED[frame - 22]);
    } else if (frame == 47) {
        msg_inject_incoming(&st->store, 0);            /* the seeded incoming (backend event, not input) */
    } else if (frame == MSG_BENCH_WARMUP - 2) {
        in->move(in->ctx, -100.0f, -100.0f);           /* blur the composer + park off-canvas: press ... */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == MSG_BENCH_WARMUP - 1) {
        in->button(in->ctx, APP_MBTN_LEFT, false);      /* ... release; pointer stays off-canvas into the hold */
    }
}
