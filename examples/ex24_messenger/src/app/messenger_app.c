/*
    messenger_app.c - the messenger's UI.

    A chat client laid out the way the world's most familiar messenger is: a rail, a chat
    list with search and filter chips, a bubble thread on a wallpaper with ticks and tails,
    reply quotes, reactions and an anchored composer. Group rooms take the shape people know
    from a team chat: a Channels group in the list, and flat rows in the room.

    What to read here:
      - One state, many shapes: every branch reads the space the layout has, never the
        platform, so a window dragged narrow behaves as a phone does.
      - Motion on the app's own clock: a new line opens up over MSG_ENTER_SECONDS from an
        .hMax the app eases, and .scrollStickToEnd follows it, so the thread glides.
      - Ids from data: a row's id is built from its message's id, so hover and context
        menus stay with the line as the thread moves around it.

    Pure RC_ API, RayClay types only. Sockets are not here: see main.c and live_room.h.

    Build: cmake --build build --target rayclay_ex24_messenger
*/
#define MESSENGER_BACKEND_IMPLEMENTATION
#include "messenger_app.h"
#include "messenger_art.h"

#include "icons/rc_icons_arrow_down.h"
#include "icons/rc_icons_arrow_left.h"
#include "icons/rc_icons_arrow_up.h"
#include "icons/rc_icons_ban.h"
#include "icons/rc_icons_check.h"
#include "icons/rc_icons_check_check_solid.h"
#include "icons/rc_icons_chevron_down.h"
#include "icons/rc_icons_chevron_right.h"
#include "icons/rc_icons_circle_alert.h"
#include "icons/rc_icons_clock.h"
#include "icons/rc_icons_contrast_filled.h"
#include "icons/rc_icons_copy.h"
#include "icons/rc_icons_ellipsis_vertical.h"
#include "icons/rc_icons_file_text.h"
#include "icons/rc_icons_hash.h"
#include "icons/rc_icons_image.h"
#include "icons/rc_icons_inbox.h"
#include "icons/rc_icons_info.h"
#include "icons/rc_icons_message_circle_solid.h"
#include "icons/rc_icons_mic.h"
#include "icons/rc_icons_mic_off.h"
#include "icons/rc_icons_paperclip.h"
#include "icons/rc_icons_person_solid.h"
#include "icons/rc_icons_phone.h"
#include "icons/rc_icons_phone_off.h"
#include "icons/rc_icons_pin.h"
#include "icons/rc_icons_reply.h"
#include "icons/rc_icons_rotate_cw.h"
#include "icons/rc_icons_search.h"
#include "icons/rc_icons_send_solid.h"
#include "icons/rc_icons_settings.h"
#include "icons/rc_icons_smile.h"
#include "icons/rc_icons_square_pen.h"
#include "icons/rc_icons_star.h"
#include "icons/rc_icons_star_filled.h"
#include "icons/rc_icons_trash_2.h"
#include "icons/rc_icons_triangle_alert.h"
#include "icons/rc_icons_users.h"
#include "icons/rc_icons_volume_2.h"
#include "icons/rc_icons_volume_x.h"
#include "icons/rc_icons_x.h"

/* The key RC_MOD_PRIMARY answers to, by the name this platform's users know it by. */
#if defined(__APPLE__)
#define PRIMARY_KEY "Cmd"
#else
#define PRIMARY_KEY "Ctrl"
#endif

/* The scripted scenario's length: warmup frames, then a strict hold. */
#define MSG_BENCH_WARMUP 64

/* The fixed columns and bands, named once: the breakpoints and the bubble cap are derived
   from them, so a pane's width changes in one place. */
#define MSG_RAIL_W        60.0f
#define MSG_LIST_W       380.0f
#define MSG_DRAWER_PANE_W 340.0f
#define MSG_BAR_WIDE      40.0f
#define MSG_BAR_NARROW    56.0f
#define MSG_HEAD_H        60.0f
#define MSG_TAB_H         56.0f

/* Below rail + list + a legible chat the two take turns; below that plus the drawer, the
   details take the chat's place as a sheet. */
#define MSG_ONE_PANE_W (MSG_RAIL_W + MSG_LIST_W + 332.0f)
#define MSG_DRAWER_W   (MSG_ONE_PANE_W + MSG_DRAWER_PANE_W)
/* Below this height the list's own title row costs most of a chat row. */
#define MSG_SHORT_H    520.0f
/* A chat in the top band keeps its name and gives its buttons to its menu below this width. */
#define MSG_BAR_ROOMY_W 560.0f

/* The side a bubble is pinned to says who spoke, so a bubble never spans the pane. A narrow
   pane gives the line more of its width, or one-line messages wrap with room to spare. */
#define MSG_THREAD_NARROW_W   640.0f
#define MSG_BUBBLE_FRAC_WIDE   0.65f
#define MSG_BUBBLE_FRAC_NARROW 0.85f
#define MSG_THREAD_PX_WIDE     52.0f
#define MSG_THREAD_PX_NARROW   12.0f
#define MSG_TAIL_W             13.0f

/* A channel's reading column: past this a line is too long to read. */
#define MSG_FLAT_MAX_W 880.0f

/* Any offset past the real travel opens a thread at its newest line: .scrollOffset is
   clamped to the content when it lands, so the app never measures the backlog. */
#define MSG_THREAD_END 1.0e6f

/* rcBeginModal's panel pads its body 20 px a side. A body of a set width too wide for that
   room is cut, not narrowed, so each body takes what the safe width leaves. */
#define MSG_MODAL_CHROME 40.0f
#define MSG_MODAL_MARGIN 16.0f

#define MSG_TOAST_SECONDS 4.5f

static const char *const FILTER_IDS[3]    = { "chip_all", "chip_unread", "chip_groups" };
static const char *const FILTER_LABELS[3] = { "All", "Unread", "Groups" };
static const char *const NAV_TITLES[3]    = { "Chats", "Starred", "Archived" };

/* Each wallpaper's ground, light and dark. A light ground is a step darker than the white
   bubble on it and never the green of your own pale bubble, which stands off a pale wall by
   its hue, as it does off the leading messenger's beige. */
static const uint32_t WALL_TINT[4][2] = {
    { 0xefeae2, 0x0b141a }, { 0xdde8f0, 0x0c1a16 }, { 0xf4ecd0, 0x1a150e }, { 0xf6e7ec, 0x1d1016 },
};
static const char *const WALL_IDS[4] = { "wall0", "wall1", "wall2", "wall3" };

/* A person's color in a group: stable, from their name, and legible on both bubbles. */
static const uint32_t SENDER_INK[8][2] = {
    { 0x1f6fd6, 0x60a5fa }, { 0xa62c71, 0xe26ab6 }, { 0x0f766e, 0x2dd4bf }, { 0xb4541a, 0xffa15a },
    { 0x6b4fbb, 0xa78bfa }, { 0x00838f, 0x4dd0e1 }, { 0xc2185b, 0xff7fb0 }, { 0x5f7d14, 0xb8d44a },
};
static const uint32_t SENDER_TINT[8][2] = {
    { 0x4facfe, 0x00c6fb }, { 0xf2709c, 0xff9472 }, { 0x11998e, 0x38ef7d }, { 0xf7971e, 0xffd200 },
    { 0xa18cd1, 0x764ba2 }, { 0x30cfd0, 0x5a67d8 }, { 0xfa709a, 0xfee140 }, { 0x7f9e2c, 0xc3d86a },
};

/* The shell: the preset re-surfaced in the messenger's green. `.surface` is what the library's
   own widgets fill with (modal panels, combos, popups). The primary is a fill under the white
   label the library picks for a dark fill, so it is the deep green in both themes. */
RC_Style messenger_style(bool dark) {
    RC_Style s = dark ? rcStyleDark() : rcStyleLight();

    if (dark) {
        s.background   = rcHex(0x0b141a);
        s.surface      = rcHex(0x202c33);
        s.surfaceAlt   = rcHex(0x2a3942);
        s.chrome       = rcHex(0x202c33);
        s.text         = rcHex(0xe9edef);
        s.textMuted    = rcHex(0x96a4ad);
        s.border       = rcHex(0x313d45);
        s.primary      = rcHex(0x008069);
        s.primaryHover = rcHex(0x00735e);
    } else {
        s.background   = rcHex(0xffffff);
        s.surface      = rcHex(0xffffff);
        s.surfaceAlt   = rcHex(0xf0f2f5);
        s.chrome       = rcHex(0xf0f2f5);
        s.text         = rcHex(0x111b21);
        s.textMuted    = rcHex(0x54656f);
        s.border       = rcHex(0xd1d7db);
        s.primary      = rcHex(0x008069);
        s.primaryHover = rcHex(0x006e5a);
    }
    s.radius = 8.0f;
    return s;
}

/* The colors RC_Style has no name for, because they are a messenger's vocabulary. */
typedef struct {
    RC_Color bar, barInk, barMuted;   /* the top band                            */
    RC_Color rail, railOn;
    RC_Color header;                  /* pane headers and the composer's bar     */
    RC_Color list, rowHover, rowActive, divider;
    RC_Color wall, doodle;
    RC_Color sheet;                   /* a channel's plain page                  */
    RC_Color bubbleIn, bubbleOut, quote;
    RC_Color meta;                    /* the time inside a bubble                */
    RC_Color icon, accent, badge, badgeInk, chipOn, chipOnInk;
    RC_Color pill, read, error, away, offline, online, panel, panelEdge;
    RC_Color notice, noticeInk, hit, hitNow;
    RC_Color drawer;                  /* the details pane, between its cards     */
    RC_Color me, meInk;               /* your own avatar                         */
    /* The classes, in this theme's colors. A state's look lives in the class, so a fill
       that comes with the pointer is a hover: there, and a box's typed .bg, which beats
       the class, is set only for a state that should keep its own fill under the pointer. */
    const char *search;               /* the list's search field                 */
    const char *field;                /* the composer field                      */
    const char *find;                 /* the in-chat search: field's, plus the x */
    const char *row;                  /* a list row: rowHover, inset focus ring  */
    const char *tile;                 /* a rail icon, a reaction, a sticker      */
    const char *chip;                 /* a filter chip that is not chosen        */
    /* An icon button's wash under the pointer: its own ink at an eighth. */
    const char *iconWash, *barWash, *accentWash, *awayWash;
    /* A "more" menu dressed as an icon button, on a pane and on the band: the same 22 px
       glyph in its ink, in the same round hit_size() square, padded (hit - 22) / 2 a side,
       and the same wash under the pointer and while its menu is open. round-icon, named in
       messenger_seed, is the glyph's size and the circle. */
    const char *menu, *barMenu;
} MsgPalette;

static MsgPalette messenger_palette(bool dark, int wallpaper) {
    /* The list's search and the composer are a finger's 44 px tall under a coarse pointer,
       and the in-chat search keeps its 36. A search field's right padding is its clear
       button's lane: a finger's 44 px square, otherwise a mouse's 24 px and the 6 px it sits
       in by. */
    const bool finger = rcPointerIsCoarse();

    if (dark) {
        MsgPalette p = {
            .bar = rcHex(0x202c33), .barInk = rcHex(0xe9edef), .barMuted = rcHex(0x96a4ad),
            .rail = rcHex(0x202c33), .railOn = rcHex(0x0a332c),
            .header = rcHex(0x202c33),
            .list = rcHex(0x111b21), .rowHover = rcHex(0x202c33), .rowActive = rcHex(0x2a3942),
            .divider = rcHex(0x202c33),
            .wall = rcHex(WALL_TINT[wallpaper][1]), .doodle = rcWithAlpha(RC_WHITE, 0.047f),
            .sheet = rcHex(0x111b21),
            .bubbleIn = rcHex(0x202c33), .bubbleOut = rcHex(0x005c4b),
            .quote = rcWithAlpha(RC_BLACK, 0.25f),
            .meta = rcWithAlpha(rcHex(0xe9edef), 0.75f),
            .icon = rcHex(0xaebac1), .accent = rcHex(0x06cf9c),
            .badge = rcHex(0x00a884), .badgeInk = rcHex(0x111b21),
            .chipOn = rcHex(0x0a332c), .chipOnInk = rcHex(0x06cf9c),
            .pill = rcHex(0x202c33), .read = rcHex(0x53bdeb),
            .error = rcHex(0xfb7185), .away = rcHex(0xfbbf24), .offline = rcHex(0x96a4ad),
            .online = rcHex(0x34d399),
            .panel = rcHex(0x202c33), .panelEdge = rcHex(0x313d45),
            .notice = rcHex(0x182229), .noticeInk = rcHex(0xffd279),
            .hit = rcHex(0x4a4520), .hitNow = rcHex(0x7a6f1c),
            .drawer = rcHex(0x0b141a), .me = rcHex(0x3b4a54), .meInk = rcHex(0xd1d7db),
            .search = finger ? "h-11 rounded-lg px-3 bg-[#202c33] border-[#202c33]"
                             : "h-[38px] rounded-lg px-3 bg-[#202c33] border-[#202c33]",
            .field = finger ? "h-11 rounded-lg px-3 bg-[#2a3942] border-[#2a3942]"
                            : "h-[42px] rounded-lg px-3 bg-[#2a3942] border-[#2a3942]",
            .find = "h-9 rounded-lg px-3 bg-[#2a3942] border-[#2a3942]",
            .row = "hover:bg-[#202c33] ring-inset", .tile = "hover:bg-[#2a3942]",
            .chip = "bg-[#2a3942] hover:bg-[#364147]",
            .iconWash = "hover:bg-[#aebac120]", .barWash = "hover:bg-[#e9edef20]",
            .accentWash = "hover:bg-[#06cf9c20]", .awayWash = "hover:bg-[#fbbf2420]",
            .menu = finger ? "round-icon h-11 px-[11px] text-[#aebac1] "
                             "hover:bg-[#aebac120] open:bg-[#aebac120]"
                           : "round-icon h-10 px-[9px] text-[#aebac1] "
                             "hover:bg-[#aebac120] open:bg-[#aebac120]",
            .barMenu = finger ? "round-icon h-11 px-[11px] text-[#e9edef] "
                                "hover:bg-[#e9edef20] open:bg-[#e9edef20]"
                              : "round-icon h-10 px-[9px] text-[#e9edef] "
                                "hover:bg-[#e9edef20] open:bg-[#e9edef20]",
        };
        return p;
    }
    MsgPalette p = {
        .bar = rcHex(0xf0f2f5), .barInk = rcHex(0x111b21), .barMuted = rcHex(0x54656f),
        .rail = rcHex(0xf0f2f5), .railOn = rcHex(0xd9fdd3),
        .header = rcHex(0xf0f2f5),
        .list = rcHex(0xffffff), .rowHover = rcHex(0xf5f6f6), .rowActive = rcHex(0xf0f2f5),
        .divider = rcHex(0xe9edef),
        .wall = rcHex(WALL_TINT[wallpaper][0]), .doodle = rcWithAlpha(RC_BLACK, 0.047f),
        .sheet = rcHex(0xffffff),
        .bubbleIn = rcHex(0xffffff), .bubbleOut = rcHex(0xd9fdd3),
        .quote = rcWithAlpha(rcHex(0x0b3d2e), 0.055f),
        .meta = rcHex(0x5f6f79),
        .icon = rcHex(0x54656f), .accent = rcHex(0x006e5a),
        .badge = rcHex(0x008069), .badgeInk = RC_WHITE,
        .chipOn = rcHex(0xd9fdd3), .chipOnInk = rcHex(0x00684f),
        .pill = rcHex(0xffffff), .read = rcHex(0x027eb5),
        .error = rcHex(0xdc2626), .away = rcHex(0xd97706), .offline = rcHex(0x8696a0),
        .online = rcHex(0x16a34a),
        .panel = rcHex(0xffffff), .panelEdge = rcHex(0xe9edef),
        .notice = rcHex(0xfff4d6), .noticeInk = rcHex(0x54656f),
        .hit = rcHex(0xfff3b8), .hitNow = rcHex(0xffd84d),
        .drawer = rcHex(0xf0f2f5), .me = rcHex(0x8696a0), .meInk = rcHex(0xffffff),
        .search = finger ? "h-11 rounded-lg px-3 bg-[#f0f2f5] border-[#f0f2f5]"
                         : "h-[38px] rounded-lg px-3 bg-[#f0f2f5] border-[#f0f2f5]",
        .field = finger ? "h-11 rounded-lg px-3 bg-[#ffffff] border-[#ffffff]"
                        : "h-[42px] rounded-lg px-3 bg-[#ffffff] border-[#ffffff]",
        .find = "h-9 rounded-lg px-3 bg-[#ffffff] border-[#ffffff]",
        .row = "hover:bg-[#f5f6f6] ring-inset", .tile = "hover:bg-[#f0f2f5]",
        .chip = "bg-[#f0f2f5] hover:bg-[#e9edef]",
        .iconWash = "hover:bg-[#54656f20]", .barWash = "hover:bg-[#54656f20]",
        .accentWash = "hover:bg-[#006e5a20]", .awayWash = "hover:bg-[#d9770620]",
        .menu = finger ? "round-icon h-11 px-[11px] text-[#54656f] "
                         "hover:bg-[#54656f20] open:bg-[#54656f20]"
                       : "round-icon h-10 px-[9px] text-[#54656f] "
                         "hover:bg-[#54656f20] open:bg-[#54656f20]",
        .barMenu = finger ? "round-icon h-11 px-[11px] text-[#111b21] "
                            "hover:bg-[#54656f20] open:bg-[#54656f20]"
                          : "round-icon h-10 px-[9px] text-[#111b21] "
                            "hover:bg-[#54656f20] open:bg-[#54656f20]",
    };
    return p;
}

/* ---- Small pieces ----------------------------------------------------------------- */

/* A hit target sized for the pointer in use: a finger needs 44 px. */
static float hit_size(void) {
    return rcPointerIsCoarse() ? 44.0f : 40.0f;
}

/* A control's class for the pointer in use. An h-* is exact on every pointer, so a button's
   finger height (h-11, 44 px) has to be asked for as its mouse height is. */
static const char *by_pointer(const char *mouse, const char *finger) {
    return rcPointerIsCoarse() ? finger : mouse;
}

static const char *fmt_id(AppState *st, const char *prefix, uint32_t n) {
    return rcFormat(&st->frame, "%s%u", prefix, n).chars;
}

/* A round icon button, the shape every header action takes, washed in its ink under the
   pointer: @p wash is the palette's class for that ink. Polling rcClicked marks the box
   clickable, and the pointer cursor comes with that; .focusable makes it a Tab stop that Enter
   and Space click, as the menu buttons beside it are. */
static bool icon_button(const char *id, RC_IconCallback icon, RC_Color ink, const char *wash,
                        const char *tip) {
    const float hit = hit_size();

    rcBox(.id = id, .focusable = true, .align = "cc", .borderRadius = "all-full",
          .wType = RC_PX(hit), .hType = RC_PX(hit), .tooltip = tip, .className = wash) {
        icon(22.0f, ink);
    }
    return rcClicked(id);
}

static uint32_t name_hash(const char *name) {
    uint32_t h = 2166136261u;

    for (; name && *name; name++)
        h = (h ^ (unsigned char)*name) * 16777619u;
    return h;
}

static RC_Color sender_ink(const char *name, bool dark) {
    return rcHex(SENDER_INK[name_hash(name) % 8u][dark]);
}

/* A disc in a gradient of its own, with initials: it stands in for a profile photo. */
static void avatar_disc(uint32_t tintA, uint32_t tintB, const char *initials, float size) {
    const float px = size >= 80.0f ? 28.0f : (size >= 44.0f ? 16.0f : 13.0f);

    rcBox(.align = "cc", .borderRadius = "all-full", .wType = RC_PX(size), .hType = RC_PX(size),
          .gradient = { .from = rcHex(tintA), .to = rcHex(tintB), .dir = "d" }) {
        rcTextC(initials, .font = rcFontSemibold(px), .color = RC_WHITE, .select = RC_SELECT_NONE);
    }
}

/* A chat's avatar: initials for a person, the people glyph for a group, a hash for a room. */
static void avatar(const MsgConversation *c, float size, const MsgPalette *pal) {
    if (c->kind == MSG_CONV_DIRECT) {
        avatar_disc(c->tintA, c->tintB, c->initials, size);
    } else if (c->kind == MSG_CONV_GROUP) {
        rcBox(.align = "cc", .borderRadius = "all-full", .wType = RC_PX(size), .hType = RC_PX(size),
              .gradient = { .from = rcHex(c->tintA), .to = rcHex(c->tintB), .dir = "d" }) {
            rcIconUsers(size * 0.52f, RC_WHITE);
        }
    } else {
        rcBox(.bg = pal->rowActive, .align = "cc", .borderRadius = "all-lg",
              .wType = RC_PX(size), .hType = RC_PX(size)) {
            rcIconHash(size * 0.5f, pal->icon);
        }
    }
}

/* The first character of a name, whole, for a sender's square. */
static const char *first_char(AppState *st, const char *name) {
    char c[5] = { '?', 0, 0, 0, 0 };
    int n = 1;

    if (name && name[0]) {
        c[0] = name[0];
        while (n < 4 && ((unsigned char)name[n] & 0xC0u) == 0x80u) {
            c[n] = name[n];
            n++;
        }
        if (n == 1 && c[0] >= 'a' && c[0] <= 'z')
            c[0] = (char)(c[0] - 'a' + 'A');
    }
    return rcFormat(&st->frame, "%s", c).chars;
}

/* Your own avatar, with a pip for the presence you set in Settings. */
static RC_Color presence_color(const MsgPalette *pal, int presence) {
    if (presence == MSG_PRESENCE_ONLINE)
        return pal->online;
    return presence == MSG_PRESENCE_AWAY ? pal->away : pal->offline;
}

static bool me_avatar(const char *id, float size, const AppState *st, RC_Color ring,
                      const MsgPalette *pal) {
    const float hit = size > hit_size() ? size : hit_size();

    rcBox(.id = id, .align = "cc", .wType = RC_PX(hit), .hType = RC_PX(hit),
          .tooltip = "Profile and settings") {
        rcBox(.bg = pal->me, .align = "cc", .borderRadius = "all-full",
              .wType = RC_PX(size), .hType = RC_PX(size)) {
            rcIconPersonSolid(size * 0.92f, pal->meInk);
            rcBox(.bg = presence_color(pal, st->statusCombo), .borderRadius = "all-full",
                  .border = { .color = ring, .width = 2 },
                  .wType = RC_PX(size * 0.32f), .hType = RC_PX(size * 0.32f),
                  .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_BOTTOM_RIGHT,
                                .element = RC_ANCHOR_BOTTOM_RIGHT,
                                .capture = RC_CAPTURE_PASSTHROUGH,
                                .clip = RC_CLIP_TO_PARENT }) {}
        }
    }
    return rcClicked(id);
}

/* The delivery mark of your own line: a clock on its way, one tick sent, two delivered, two
   in the read color once read, a red alert if it failed. The double tick is wider than the
   others, so it draws at 16 px where they draw at 14. */
static void delivery_mark(uint8_t deliv, bool receipts, RC_Color gray, const MsgPalette *pal) {
    if (deliv == MSG_DELIV_SENDING || deliv == MSG_DELIV_QUEUED)
        rcIconClock(14.0f, gray);
    else if (deliv == MSG_DELIV_FAILED)
        rcIconCircleAlert(14.0f, pal->error);
    else if (deliv == MSG_DELIV_SENT)
        rcIconCheck(14.0f, gray);
    else
        rcIconCheckCheckSolid(16.0f, deliv == MSG_DELIV_READ && receipts ? pal->read : gray);
}

static void show_toast(AppState *st, const char *title, const char *words, int conv) {
    rcStrCopy(st->toastTitle, title ? title : "", sizeof st->toastTitle);
    rcStrCopy(st->toast, words, sizeof st->toast);
    st->toastConv = conv;
    st->undoConv  = -1;
    st->toastLeft = MSG_TOAST_SECONDS;
}

static const char *thread_id(AppState *st, int conv) {
    return fmt_id(st, "thread", (uint32_t)conv);
}

static const char *composer_id(AppState *st) {
    return fmt_id(st, "composer", (uint32_t)st->openConv);
}

/* ---- What a click does ------------------------------------------------------------ */

/* Opening a chat is one place: the store learns what is on screen, the unread line stands
   where the reader left off, and every thread-sized popup closes. */
static void open_chat(AppState *st, int i, bool onePane) {
    MsgConversation *c = &st->store.conv[i];

    st->openConv = i;
    st->store.openConv = i;
    st->unreadMarkConv = i;
    st->unreadMarkCount = c->unread;
    st->unreadMarkId = c->unread > 0 && c->unread <= c->count
                     ? c->items[c->count - c->unread].id : 0;
    msg_mark_read(&st->store, i);
    if (st->toastConv == i)
        st->toastLeft = 0.0f;       /* the banner's news is on screen now */
    st->replyTo = 0;
    st->reactFor = 0;
    st->stickerOpen = false;
    st->threadSearchOn = false;
    st->threadSearch[0] = '\0';
    if (onePane) {
        st->sidebarCollapsed = true;
        st->infoOpen = false;
    }
    if (c->kind == MSG_CONV_CHANNEL && st->live.link != LIVE_DEMO && st->live.room != i) {
        st->live.ask = LIVE_ASK_ROOM;
        st->live.askRoom = i;
    }
}

static void jump_to_line(AppState *st, uint32_t id) {
    st->scrollTo = id;
    st->flashId = id;
    st->flashLeft = 1.4f;
}

/* Your draft becomes a line. Called before the thread is declared: a full thread drops its
   oldest line to make room, which would move a row already declared this frame. */
static void send_draft(AppState *st) {
    char *draft = st->drafts[st->openConv];
    const char *start = draft;

    while (*start == ' ')
        start++;
    int len = rcStringFromCStr(start).length;

    while (len > 0 && start[len - 1] == ' ')
        len--;
    if (len <= 0)
        return;
    /* A refused send keeps the draft, so nothing the user typed is lost. */
    if (msg_send(&st->store, st->openConv, MSG_KIND_TEXT, 0, start, len, st->replyTo,
                 st->store.conv[st->openConv].kind == MSG_CONV_CHANNEL ? st->myName : NULL) < 0)
        return;
    draft[0] = '\0';
    st->replyTo = 0;
    if (!rcPointerIsCoarse())
        rcSetFocus(composer_id(st));
    rcScrollToBottom(thread_id(st, st->openConv));
}

/* A sticker, a photo or a call's note: the panel, the dialog and the call that ask for one
   are declared after the thread, and a send can recycle pooled text a line on screen still
   draws (send_draft's note), so the line waits for the top of the next layout. */
static void send_art(AppState *st, MsgKind kind, int art, const char *text) {
    st->sendKind = (int)kind + 1;
    st->sendArt  = art;
    rcStrCopy(st->sendText, text ? text : "", sizeof st->sendText);
}

static void send_pending(AppState *st) {
    const MsgKind kind = (MsgKind)(st->sendKind - 1);

    st->sendKind = 0;
    if (kind == MSG_KIND_EVENT) {     /* a call's end: its note is the line */
        msg_live_line(&st->store, st->openConv, st->sendText, "", st->store.nowClock, 0,
                      MSG_KIND_EVENT, MSG_DIR_INCOMING);
        return;
    }
    msg_send(&st->store, st->openConv, kind, st->sendArt, st->sendText,
             rcStringFromCStr(st->sendText).length, st->replyTo,
             st->store.conv[st->openConv].kind == MSG_CONV_CHANNEL ? st->myName : NULL);
    st->replyTo = 0;
    rcScrollToBottom(thread_id(st, st->openConv));
}

/* The row to show when a chat leaves the list: the one after it, or the one before it if it
   was the last. A channel can be neither deleted nor archived, so there is always one. */
static int next_row(const AppState *st, int leaving) {
    const MsgStore *ms = &st->store;
    int before = C_GENERAL;
    bool past = false;

    for (int o = 0; o < ms->orderCount; o++) {
        const int i = ms->order[o];

        if (i == leaving) {
            past = true;
        } else if (!ms->conv[i].archived && !ms->conv[i].hidden) {
            if (past)
                return i;
            before = i;
        }
    }
    return before;
}

/* Delete or archive: the row goes, and if its thread was on screen that goes too, so nothing
   stays open that the list does not have. The banner offers all of it back. One pane returns
   to the list; the next chat waits there, unread lines and all, until someone opens it. The
   ask comes from inside a pane, mid-declaration, so this runs at the top of the next frame. */
static void set_chat_aside(AppState *st, int i, bool archive, bool onePane) {
    MsgConversation *c = &st->store.conv[i];
    const bool wasOpen = i == st->openConv;
    const int next = next_row(st, i);

    if (archive)
        c->archived = true;
    else
        c->hidden = true;
    show_toast(st, "", archive ? "Chat archived" : "Chat deleted", -1);
    st->undoConv    = i;
    st->undoArchive = archive;
    st->undoOpen    = wasOpen;
    if (!wasOpen)
        return;
    st->infoOpen = false;
    if (onePane) {
        st->openConv = next;
        st->sidebarCollapsed = false;
        st->store.openConv = -1;
    } else {
        open_chat(st, next, false);
    }
}

static void undo_set_aside(AppState *st, bool onePane) {
    MsgConversation *c = &st->store.conv[st->undoConv];

    if (st->undoArchive)
        c->archived = false;
    else
        c->hidden = false;
    if (st->undoOpen)
        open_chat(st, st->undoConv, onePane);
    st->undoConv = -1;
    st->toastLeft = 0.0f;
}

/* The actions on a chat, shared by a row's context menu and the header's menu. */
static void chat_menu_items(AppState *st, int i) {
    MsgConversation *c = &st->store.conv[i];
    const bool unread = c->unread > 0 || c->markedUnread;

    if (c->kind != MSG_CONV_CHANNEL
        && rcMenuItem(c->pinned ? "Unpin chat" : "Pin chat", .leadingIcon = rcIconPin))
        c->pinned = !c->pinned;
    if (rcMenuItem(c->muted ? "Unmute notifications" : "Mute notifications",
                   .leadingIcon = c->muted ? rcIconVolume2 : rcIconVolumeX))
        c->muted = !c->muted;
    if (rcMenuItem(unread ? "Mark as read" : "Mark as unread", .leadingIcon = rcIconCheck)) {
        if (unread)
            msg_mark_read(&st->store, i);
        else
            msg_mark_unread(&st->store, i);
    }
    if (rcMenuItem("Export chat", .disabled = st->exportAsk >= 0, .leadingIcon = rcIconFileText))
        st->exportAsk = i;
    if (c->kind != MSG_CONV_CHANNEL) {
        if (rcMenuItem(c->archived ? "Unarchive chat" : "Archive chat",
                       .leadingIcon = rcIconInbox)) {
            if (c->archived) {
                c->archived = false;
                show_toast(st, "", "Chat moved back to your list", -1);
            } else {
                st->asideConv = i;
                st->asideArchive = true;
            }
        }
        if (rcMenuItem("Delete chat", .className = "text-danger hover:bg-danger/10",
                       .leadingIcon = rcIconTrash2)) {
            st->asideConv = i;
            st->asideArchive = false;
        }
    }
}

/* ---- The band and the rail -------------------------------------------------------- */

/* The word for where the channels' lines come from, for the list's Channels heading. */
static const char *link_word(LiveLink link) {
    static const char *const WORDS[] = {
        "Demo", "Connecting", "Joining", "Live", "Reconnecting", "Closed",
    };

    return WORDS[link];
}

/* The live room's status dot, one color rule for the band's chip and the list's heading. */
static RC_Color link_dot(LiveLink link, RC_Color ink, const MsgPalette *pal) {
    return link == LIVE_OPEN ? pal->online
         : link == LIVE_CLOSED ? pal->error
         : link == LIVE_DEMO ? rcWithAlpha(ink, 0.59f) : pal->away;
}

/* The status of the live room, in the band, in full. */
static void live_chip(AppState *st, RC_Color ink, const MsgPalette *pal) {
    const LiveRoom *lv = &st->live;
    const RC_Color dot = link_dot(lv->link, ink, pal);
    const char *word = "Demo room";

    switch (lv->link) {
    case LIVE_CONNECTING:
        word = lv->tries ? "Reconnecting" : "Connecting";
        break;
    case LIVE_JOINING:
        word = "Joining";
        break;
    case LIVE_OPEN:
        word = rcFormat(&st->frame, "Live in #%s", st->store.conv[lv->room].name).chars;
        break;
    case LIVE_WAITING:
        word = rcFormat(&st->frame, "Reconnecting in %d s", (int)(lv->retryIn + 0.999f)).chars;
        break;
    case LIVE_CLOSED:
        word = "Closed";
        break;
    default:
        break;
    }
    rcRow(.gap = 6, .px = 12, .align = "cl", .overflow = "hidden") {
        rcBox(.bg = dot, .borderRadius = "all-full", .w = "8px", .h = "8px") {}
        rcTextC(word, .color = ink, .size = 12, .wrap = "n");
    }
}

static void start_call(AppState *st) {
    st->modalCall = true;
    st->callTime = 0.0;
    st->callMuted = false;
}

/* An away contact was last seen twenty minutes before the app's now, on the clock its lines
   run on, so the header never names a time still to come. */
static const char *last_seen(AppState *st) {
    const uint32_t seen = st->store.nowClock % 86400u + 86400u - 1200u;

    return rcFormat(&st->frame, "last seen %s at %02u:%02u", seen >= 86400u ? "today" : "yesterday",
                    seen / 3600u % 24u, seen / 60u % 60u).chars;
}

static void toggle_thread_search(AppState *st) {
    st->threadSearchOn = !st->threadSearchOn;
    st->threadSearch[0] = '\0';
    if (st->threadSearchOn)
        rcSetFocus("thread_search");
}

/* The chat's header. It draws in the wide header row and, narrow, inside the top band,
   where it adds the way back and its empty middle becomes the window's drag handle. On a band
   with little room (`roomy` false) the name stays readable and the menu takes the buttons. */
static void chat_header(AppState *st, MsgConversation *c, bool inBar, bool roomy, RC_Color ink,
                        RC_Color muted, const MsgPalette *pal) {
    const RC_Color iconInk = inBar ? pal->barInk : pal->icon;
    const char *wash = inBar ? pal->barWash : pal->iconWash;

    if (inBar && icon_button("thr_back", rcIconArrowLeft, iconInk, wash, "Back to chats")) {
        st->sidebarCollapsed = false;
        st->infoOpen = false;
        st->store.openConv = -1;   /* the list is on screen: new lines count as unread */
    }
    /* The whole identity block opens the details, as a click on the name does in every
       client a general user has met. */
    rcRow(.id = "hdr_profile", .gap = 12, .align = "cl", .overflow = "hidden", .h = "grow",
          .tooltip = c->kind == MSG_CONV_DIRECT ? "Contact info"
                   : (c->kind == MSG_CONV_GROUP ? "Group info" : "Channel details")) {
        if (c->kind != MSG_CONV_CHANNEL)
            avatar(c, 40.0f, pal);
        rcColumn(.gap = 2, .overflow = "hidden") {
            if (c->kind == MSG_CONV_CHANNEL) {
                rcRow(.gap = 4, .align = "cl") {
                    rcIconHash(17.0f, ink);
                    rcTextC(c->name, .font = rcFontSemibold(16), .color = ink, .wrap = "e");
                    rcIconChevronDown(14.0f, muted);
                }
            } else {
                rcTextC(c->name, .font = rcFontSemibold(16), .color = ink, .wrap = "e");
            }
            /* Who is here or how they are: a chat's second line, live while they type. */
            if (c->typing[0])
                rcTextC(c->typing, .color = pal->accent, .size = 13, .wrap = "e");
            else if (c->kind == MSG_CONV_DIRECT)
                rcTextC(c->presence == MSG_PRESENCE_ONLINE ? "online"
                        : (c->presence == MSG_PRESENCE_AWAY ? last_seen(st)
                                                            : "last seen recently"),
                        .color = muted, .size = 13, .wrap = "e");
            else if (c->kind == MSG_CONV_GROUP)
                rcTextC(c->detail, .color = muted, .size = 13, .wrap = "e");
            else
                rcTextC(c->about, .color = muted, .size = 13, .wrap = "e");
        }
    }
    if (rcClicked("hdr_profile"))
        st->infoOpen = !st->infoOpen;
    if (inBar)
        rcBox(.id = RC_ID_WINDOW_DRAG, .w = "grow", .h = "grow") {}
    else
        rcSpacer() {}
    if (roomy && c->kind == MSG_CONV_DIRECT
        && icon_button("thr_call", rcIconPhone, iconInk, wash, "Voice call"))
        start_call(st);
    if (roomy && icon_button("thr_search", rcIconSearch, iconInk, wash, "Search this chat"))
        toggle_thread_search(st);
    if (rcBeginMenu("thr_menu", "", .className = inBar ? pal->barMenu : pal->menu,
                    .variant = RC_BTN_GHOST, .leadingIcon = rcIconEllipsisVertical)) {
        if (!roomy && c->kind == MSG_CONV_DIRECT
            && rcMenuItem("Voice call", .leadingIcon = rcIconPhone))
            start_call(st);
        if (!roomy && rcMenuItem("Search this chat", .leadingIcon = rcIconSearch))
            toggle_thread_search(st);
        if (rcMenuItem(c->kind == MSG_CONV_DIRECT ? "Contact info"
                       : (c->kind == MSG_CONV_GROUP ? "Group info" : "Channel details"),
                       .leadingIcon = rcIconInfo))
            st->infoOpen = true;
        chat_menu_items(st, st->openConv);
        rcEndMenu();
    }
}

static void open_new_chat(AppState *st) {
    st->modalNewChat = true;
    rcSetFocus("new_search");
}

/* The top band. Wide, a slim titlebar with the app's mark; narrow, it is whichever screen's
   app bar is showing, so a phone never stacks two bars. It is chrome: the caller declares it
   unzoomed, as a browser's toolbar stays put while its page zooms, so barW is unzoomed px. */
static void msg_bar(AppState *st, bool onePane, bool listShown, bool infoSheet, float barH,
                    float barW, const MsgPalette *pal) {
    MsgConversation *c = &st->store.conv[st->openConv];

    rcRow(.bg = pal->bar, .gap = 4, .pl = (float)(onePane ? 4 : 14), .align = "cl", .w = "grow",
          .hType = RC_PX(barH)) {
        if (!onePane) {
            rcRow(.id = RC_ID_WINDOW_DRAG, .gap = 8, .align = "cl", .w = "grow", .h = "grow") {
                rcIconMessageCircleSolid(20.0f, pal->accent);
                rcTextL("RayClay Messenger", .font = rcFontSemibold(13), .color = pal->barInk);
            }
            live_chip(st, pal->barMuted, pal);
        } else if (listShown) {
            /* min-w-0 and a clip, as on the web: the title gives way to the buttons and the
               window controls, ending in an ellipsis. */
            rcRow(.id = RC_ID_WINDOW_DRAG, .pl = 12, .align = "cl", .w = "grow", .h = "grow",
                  .className = "min-w-0 overflow-hidden") {
                rcTextC(NAV_TITLES[st->nav], .font = rcFontSemibold(22), .color = pal->barInk,
                        .wrap = "e");
            }
            /* No search button: the list's search field is right under this bar. */
            if (st->nav == NAV_CHATS
                && icon_button("bar_new", rcIconSquarePen, pal->barInk, pal->barWash, "New chat"))
                open_new_chat(st);
        } else if (infoSheet) {
            if (icon_button("info_back", rcIconArrowLeft, pal->barInk, pal->barWash,
                            "Back to chat"))
                st->infoOpen = false;
            rcRow(.id = RC_ID_WINDOW_DRAG, .pl = 8, .align = "cl", .w = "grow", .h = "grow",
                  .className = "min-w-0 overflow-hidden") {
                rcTextC(c->kind == MSG_CONV_DIRECT ? "Contact info"
                        : (c->kind == MSG_CONV_GROUP ? "Group info" : "Channel details"),
                        .font = rcFontSemibold(18), .color = pal->barInk, .wrap = "e");
            }
        } else {
            chat_header(st, c, true, barW >= MSG_BAR_ROOMY_W, pal->barInk, pal->barMuted, pal);
        }
        /* Band-tall and flush to the corner, so the OS rounds the close slab with the
           window rather than leaving a strip beside it; the glyphs take the style's text,
           which is the band's ink in both themes. */
        rcWindowControls();
    }
}

/* One destination on the rail, or on the phone's tab bar with its word under the glyph. */
static bool nav_item(const char *id, RC_IconCallback icon, const char *label, const char *badge,
                     bool on, bool tabs, const MsgPalette *pal) {
    const RC_Color ink = on ? pal->accent : pal->icon;

    if (tabs) {
        rcColumn(.id = id, .gap = 2, .align = "cc", .w = "grow", .h = "grow") {
            rcBox(.bg = on ? pal->chipOn : RC_UNSET_COLOR, .align = "cc",
                  .borderRadius = "all-full", .w = "56px", .h = "30px") {
                icon(22.0f, on ? pal->chipOnInk : pal->icon);
                if (badge && badge[0])
                    rcBox(.bg = pal->badge, .px = 5, .align = "cc", .borderRadius = "all-full",
                          .h = "18px", .wMin = RC_VAL(18.0f),
                          .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_RIGHT,
                                        .element = RC_ANCHOR_CENTER,
                                        .offset = { -8.0f, 3.0f } }) {
                        rcTextC(badge, .color = pal->badgeInk, .size = 11);
                    }
            }
            rcTextC(label, .color = on ? rcGetStyle().text : pal->icon, .size = 12,
                    .select = RC_SELECT_NONE);
        }
    } else {
        /* Where you are keeps its fill under the pointer: the typed .bg beats the class. */
        rcBox(.id = id, .bg = on ? pal->railOn : RC_UNSET_COLOR, .align = "cc",
              .borderRadius = "all-full", .w = "40px", .h = "40px", .tooltip = label,
              .className = pal->tile) {
            icon(22.0f, ink);
            if (badge && badge[0])
                rcBox(.bg = pal->badge, .px = 4, .align = "cc", .borderRadius = "all-full",
                      .h = "16px", .wMin = RC_VAL(16.0f),
                      .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_RIGHT,
                                    .element = RC_ANCHOR_CENTER,
                                    .offset = { -8.0f, 7.0f } }) {
                    rcTextC(badge, .color = pal->badgeInk, .size = 11,
                            .select = RC_SELECT_NONE);
                }
        }
    }
    return rcClicked(id);
}

static void set_nav(AppState *st, MsgNav nav) {
    st->nav = nav;
    st->search[0] = '\0';
}

/* The rail: where you are, then you. */
static void msg_rail(AppState *st, const MsgPalette *pal) {
    rcColumn(.bg = pal->rail, .gap = 8, .pt = 10, .pb = 12, .align = "tc", .h = "grow",
             .wType = RC_PX(MSG_RAIL_W)) {
        if (nav_item("nav_chats", rcIconMessageCircleSolid, "Chats", msg_unread_badge(&st->store),
                     st->nav == NAV_CHATS, false, pal))
            set_nav(st, NAV_CHATS);
        if (nav_item("nav_starred", rcIconStar, "Starred messages", NULL,
                     st->nav == NAV_STARRED, false, pal))
            set_nav(st, NAV_STARRED);
        if (nav_item("nav_archived", rcIconInbox, "Archived", NULL,
                     st->nav == NAV_ARCHIVED, false, pal))
            set_nav(st, NAV_ARCHIVED);
        rcSpacer() {}
        if (nav_item("nav_theme", rcIconContrastFilled,
                     st->darkMode ? "Switch to the light theme" : "Switch to the dark theme",
                     NULL, false, false, pal))
            st->darkMode = !st->darkMode;
        if (nav_item("nav_settings", rcIconSettings, "Settings", NULL, st->modalSettings, false,
                     pal))
            st->modalSettings = true;
        if (me_avatar("nav_me", 32.0f, st, pal->rail, pal))
            st->modalSettings = true;
    }
}

/* The phone's bottom tabs: the rail's destinations, under a thumb. */
static void msg_tabs(AppState *st, const MsgPalette *pal) {
    rcBox(.bg = pal->divider, .w = "grow", .h = "1px") {}
    rcRow(.bg = pal->header, .align = "cc", .w = "grow", .hType = RC_PX(MSG_TAB_H)) {
        if (nav_item("tab_chats", rcIconMessageCircleSolid, "Chats", msg_unread_badge(&st->store),
                     st->nav == NAV_CHATS, true, pal))
            set_nav(st, NAV_CHATS);
        if (nav_item("tab_starred", rcIconStar, "Starred", NULL, st->nav == NAV_STARRED, true, pal))
            set_nav(st, NAV_STARRED);
        if (nav_item("tab_archived", rcIconInbox, "Archived", NULL, st->nav == NAV_ARCHIVED, true,
                     pal))
            set_nav(st, NAV_ARCHIVED);
        if (nav_item("tab_settings", rcIconSettings, "Settings", NULL, st->modalSettings, true,
                     pal))
            st->modalSettings = true;
    }
}

/* ---- The chat list ---------------------------------------------------------------- */

/* Whether the list shows a chat or a channel: where the rail points, and the chips. */
static bool row_listed(const AppState *st, const MsgConversation *c) {
    if (c->archived != (st->nav == NAV_ARCHIVED))
        return false;
    if (st->nav != NAV_CHATS || st->listFilter == FILTER_ALL)
        return true;
    if (st->listFilter == FILTER_UNREAD)
        return c->unread > 0 || c->markedUnread;
    return c->kind == MSG_CONV_GROUP;
}

/* A chat row; the whole row is the click target, and right-click or a long press offers the
   chat's menu. */
static void conv_row(AppState *st, int i, bool onePane, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    const MsgConversation *c = &st->store.conv[i];
    const char *id = fmt_id(st, "conv", (uint32_t)i);
    const bool active = !onePane && i == st->openConv;
    const bool unread = c->unread > 0 || c->markedUnread;
    const MsgMessage *last = msg_last(c);
    const bool mine = last && last->dir == MSG_DIR_OUTGOING && !c->typing[0];
    const char *draft = st->drafts[i];
    const bool hasDraft = draft[0] != '\0' && i != st->openConv;
    const char *preview = "";

    if (last) {
        preview = msg_snippet(last);
        if (last->kind == MSG_KIND_EVENT && last->textLen)
            preview = rcFormat(&st->frame, "%s %s", last->who, last->text).chars;
        else if (c->kind != MSG_CONV_DIRECT && last->dir == MSG_DIR_INCOMING && last->who
                 && last->kind != MSG_KIND_EVENT)
            preview = rcFormat(&st->frame, "%s: %s", last->who, preview).chars;
    }

    rcRow(.id = id, .bg = active ? pal->rowActive : RC_UNSET_COLOR, .gap = 12, .pl = 12,
          .align = "cl", .w = "grow", .h = "72px", .className = pal->row) {
        avatar(c, 48.0f, pal);
        rcColumn(.w = "grow", .h = "grow", .className = "min-w-0") {
            rcColumn(.gap = 3, .pr = 14, .align = "cl", .w = "grow", .h = "grow") {
                rcRow(.gap = 6, .align = "cl", .w = "grow") {
                    rcBox(.overflow = "hidden", .w = "grow", .className = "min-w-0") {
                        rcTextC(c->name, .font = unread ? rcFontSemibold(15) : (uint16_t)0,
                                .color = s.text, .size = 15, .wrap = "e",
                                .select = RC_SELECT_NONE);
                    }
                    rcTextC(c->lastTime, .color = unread && !c->muted ? pal->accent : s.textMuted,
                            .size = 12, .select = RC_SELECT_NONE);
                }
                rcRow(.gap = 4, .align = "cl", .w = "grow", .h = "20px") {
                    if (mine && !hasDraft)
                        delivery_mark(last->deliv, st->store.readReceipts, s.textMuted, pal);
                    rcBox(.overflow = "hidden", .w = "grow", .className = "min-w-0") {
                        if (c->typing[0]) {
                            rcTextC(c->typing, .color = pal->accent, .size = 13, .wrap = "e",
                                    .select = RC_SELECT_NONE);
                        } else if (hasDraft) {
                            rcRow(.gap = 4, .align = "cl") {
                                rcTextL("Draft:", .font = rcFontSemibold(13),
                                        .color = pal->accent);
                                rcTextC(draft, .color = s.textMuted, .size = 13, .wrap = "e",
                                        .select = RC_SELECT_NONE);
                            }
                        } else {
                            rcTextC(preview, .color = s.textMuted, .size = 13, .wrap = "e",
                                    .select = RC_SELECT_NONE);
                        }
                    }
                    if (c->muted)
                        rcIconVolumeX(15.0f, s.textMuted);
                    if (c->pinned)
                        rcIconPin(15.0f, s.textMuted);
                    if (c->badge[0] || c->markedUnread) {
                        rcBox(.bg = c->muted ? s.textMuted : pal->badge, .px = 6, .align = "cc",
                              .borderRadius = "all-full", .h = "20px", .wMin = RC_VAL(20.0f)) {
                            rcTextC(c->badge, .color = c->muted ? RC_WHITE : pal->badgeInk,
                                    .size = 12, .select = RC_SELECT_NONE);
                        }
                    }
                }
            }
            rcBox(.bg = active ? RC_UNSET_COLOR : pal->divider, .w = "grow", .h = "1px") {}
        }
    }
    if (rcClicked(id))
        open_chat(st, i, onePane);
    if (rcBeginContextMenu(fmt_id(st, "conv_menu", (uint32_t)i), id)) {
        chat_menu_items(st, i);
        rcEndContextMenu();
    }
}

/* A channel, in the compact row a team chat gives it: a hash and a name, bold while it has
   lines you have not read. It spans the list as a chat's row does, and its hash starts where
   the avatars do. */
static void channel_row(AppState *st, int i, bool onePane, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    const MsgConversation *c = &st->store.conv[i];
    const char *id = fmt_id(st, "conv", (uint32_t)i);
    const bool active = !onePane && i == st->openConv;
    const bool unread = c->unread > 0 || c->markedUnread;
    const RC_Color ink = active || unread ? s.text : s.textMuted;

    rcRow(.id = id, .bg = active ? pal->rowActive : RC_UNSET_COLOR, .gap = 10, .pl = 12,
          .pr = 14, .align = "cl", .w = "grow",
          .hType = RC_PX(rcPointerIsCoarse() ? 44.0f : 32.0f), .className = pal->row) {
        rcIconHash(15.0f, ink);
        rcBox(.overflow = "hidden", .w = "grow") {
            rcTextC(c->name, .font = unread ? rcFontSemibold(14) : (uint16_t)0, .color = ink,
                    .size = 14, .wrap = "e", .select = RC_SELECT_NONE);
        }
        if (c->muted)
            rcIconVolumeX(14.0f, s.textMuted);
        if (c->badge[0] && !c->muted)
            rcBox(.bg = pal->badge, .px = 6, .align = "cc", .borderRadius = "all-full",
                  .h = "18px", .wMin = RC_VAL(18.0f)) {
                rcTextC(c->badge, .color = pal->badgeInk, .size = 11, .select = RC_SELECT_NONE);
            }
    }
    if (rcClicked(id))
        open_chat(st, i, onePane);
    if (rcBeginContextMenu(fmt_id(st, "conv_menu", (uint32_t)i), id)) {
        chat_menu_items(st, i);
        rcEndContextMenu();
    }
}

/* The small heading over a part of the list. */
static void list_heading(const char *text, RC_Color ink) {
    rcBox(.pt = 12, .pb = 6, .px = 20, .w = "grow") {
        rcTextC(text, .font = rcFontSemibold(13), .color = ink, .select = RC_SELECT_NONE);
    }
}

/* The Channels group: it folds on its heading, and says where its lines come from. Its word
   starts where list_heading's does, with the fold's chevron after it. The chips filter it too,
   and with no channel to show there is no heading. Returns how many it lists. */
static int channels_section(AppState *st, bool onePane, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    const char *id = "channels_head";
    int rows = 0;

    for (int i = 0; i < st->store.convCount; i++)
        if (st->store.conv[i].kind == MSG_CONV_CHANNEL && row_listed(st, &st->store.conv[i]))
            rows++;
    if (!rows)
        return 0;
    rcRow(.id = id, .gap = 6, .pt = 12, .pb = 6, .pl = 20, .pr = 14, .align = "cl", .w = "grow") {
        rcTextL("Channels", .font = rcFontSemibold(13), .color = s.textMuted,
                .select = RC_SELECT_NONE);
        if (st->channelsCollapsed)
            rcIconChevronRight(15.0f, s.textMuted);
        else
            rcIconChevronDown(15.0f, s.textMuted);
        rcSpacer() {}
        rcBox(.bg = link_dot(st->live.link, s.textMuted, pal), .borderRadius = "all-full",
              .w = "6px", .h = "6px") {}
        rcTextC(link_word(st->live.link), .color = s.textMuted, .size = 11,
                .select = RC_SELECT_NONE);
    }
    if (rcClicked(id))
        st->channelsCollapsed = !st->channelsCollapsed;
    if (!st->channelsCollapsed) {
        rcColumn(.gap = 1, .w = "grow") {
            for (int i = 0; i < st->store.convCount; i++)
                if (st->store.conv[i].kind == MSG_CONV_CHANNEL
                    && row_listed(st, &st->store.conv[i]))
                    channel_row(st, i, onePane, pal);
        }
    }
    return rows;
}

/* One found line: whose chat, the line, when. Opening it lands on the line. */
static void found_row(AppState *st, int conv, const MsgMessage *m, bool onePane,
                      const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    const MsgConversation *c = &st->store.conv[conv];
    const char *id = fmt_id(st, "found", m->id);

    rcRow(.id = id, .gap = 12, .pl = 12, .pr = 14, .py = 10, .align = "cl", .w = "grow",
          .className = pal->row) {
        avatar(c, 40.0f, pal);
        rcColumn(.gap = 2, .overflow = "hidden", .w = "grow") {
            rcRow(.gap = 6, .align = "cl", .w = "grow") {
                rcBox(.overflow = "hidden", .w = "grow") {
                    rcTextC(c->name, .font = rcFontSemibold(13), .color = s.text, .wrap = "e",
                            .select = RC_SELECT_NONE);
                }
                rcTextC(m->day ? msg_day_word(&st->store, m->day) : m->ts, .color = s.textMuted,
                        .size = 12, .select = RC_SELECT_NONE);
            }
            rcTextC(msg_snippet(m), .color = s.textMuted, .size = 13, .wrap = "e",
                    .select = RC_SELECT_NONE);
        }
    }
    if (rcClicked(id)) {
        open_chat(st, conv, onePane);
        jump_to_line(st, m->id);
    }
}

/* One contact, from the search or the New chat list. True on a click. */
static bool contact_row(AppState *st, const char *prefix, int person, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    const MsgPerson *who = msg_person(person);
    const char *id = fmt_id(st, prefix, (uint32_t)person);
    char initials[3];

    msg_initials(who->name, initials);
    rcRow(.id = id, .focusable = true, .gap = 12, .px = 12, .py = 8, .align = "cl", .w = "grow",
          .className = pal->row) {
        avatar_disc(who->tintA, who->tintB, rcFormat(&st->frame, "%s", initials).chars, 40.0f);
        rcColumn(.gap = 1, .overflow = "hidden", .w = "grow") {
            rcTextC(who->name, .color = s.text, .size = 15, .select = RC_SELECT_NONE);
            rcTextC(who->about, .color = s.textMuted, .size = 13, .wrap = "e",
                    .select = RC_SELECT_NONE);
        }
    }
    return rcClicked(id);
}

/* A new chat with this person, or back to the one you have. Run after the list is declared:
   it empties the queries that list was reading. */
static void start_chat(AppState *st, int person, bool onePane) {
    const int conv = msg_start_chat(&st->store, person);

    if (conv >= 0)
        open_chat(st, conv, onePane);
    st->newChatQuery[0] = '\0';
    st->search[0] = '\0';
    st->modalNewChat = false;
}

/* Whether the person already has a row of their own among the chats. */
static bool has_chat(const MsgStore *ms, int person) {
    for (int i = 0; i < ms->convCount; i++)
        if (ms->conv[i].person == person && ms->conv[i].kind == MSG_CONV_DIRECT
            && !ms->conv[i].hidden)
            return true;
    return false;
}

/* What the list shows when you have typed in the search: the chats by name, the contacts you
   have no chat with, then the lines that say it. Each heading comes with its first row. */
static int search_results(AppState *st, bool onePane, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    int shown = 0;
    int chats = 0, contacts = 0, lines = 0;
    int pick = -1;

    for (int k = 0; k < st->store.convCount; k++) {
        const MsgConversation *c = &st->store.conv[k];

        if (!c->hidden && msg_text_matches(c->name, st->search)) {
            if (!chats++)
                list_heading("Chats", s.textMuted);
            conv_row(st, k, onePane, pal);
            shown++;
        }
    }
    for (int p = MSG_CONTACT_FIRST; p <= MSG_CONTACT_LAST; p++) {
        if (has_chat(&st->store, p) || !msg_text_matches(msg_person(p)->name, st->search))
            continue;
        if (!contacts++)
            list_heading("Contacts", s.textMuted);
        if (contact_row(st, "found_contact", p, pal))
            pick = p;
        shown++;
    }
    for (int k = 0; k < st->store.convCount; k++) {
        const MsgConversation *c = &st->store.conv[k];

        for (int i = c->count - 1; i >= 0 && !c->hidden; i--) {
            const MsgMessage *m = &c->items[i];

            if (m->textLen && !m->deleted && m->kind != MSG_KIND_EVENT
                && msg_text_matches(m->text, st->search) && shown < 40) {
                if (!lines++)
                    list_heading("Messages", s.textMuted);
                found_row(st, k, m, onePane, pal);
                shown++;
            }
        }
    }
    if (pick >= 0)
        start_chat(st, pick, onePane);
    return shown;
}

/* The starred lines, newest chat first; opening one lands on it. */
static int starred_results(AppState *st, bool onePane, const MsgPalette *pal) {
    int shown = 0;

    for (int o = 0; o < st->store.orderCount; o++) {
        const int k = st->store.order[o];
        const MsgConversation *c = &st->store.conv[k];

        for (int i = c->count - 1; i >= 0; i--) {
            if (c->items[i].starred) {
                found_row(st, k, &c->items[i], onePane, pal);
                shown++;
            }
        }
    }
    return shown;
}

/* An empty list says why, and what to do about it. */
static void list_empty(const char *title, const char *hint, RC_IconCallback icon) {
    RC_Style s = rcGetStyle();

    rcColumn(.gap = 8, .pt = 64, .px = 32, .align = "tc", .w = "grow") {
        icon(40.0f, s.textMuted);
        rcBox(.h = "4px") {}
        rcTextC(title, .font = rcFontSemibold(15), .color = s.text, .select = RC_SELECT_NONE);
        rcTextC(hint, .color = s.textMuted, .size = 13, .textAlign = "c",
                .select = RC_SELECT_NONE);
    }
}

/* The list's search, whose x empties it before the list under it reads the search. A short
   window has dropped the list's title row, so New chat comes here. */
static void list_search(AppState *st, bool withNew, bool onePane, const MsgPalette *pal) {
    /* Beside the rail in a short window, with no title row above, the row starts 10 px down
       as the rail does, so the field and New chat sit level with the rail's first icon. One
       pane has no title row either: the field keeps the same 10 px under the app bar. */
    rcRow(.gap = 4, .pt = (float)(withNew || onePane ? 10 : 4), .pb = 6, .pl = 12,
          .pr = (float)(withNew ? 4 : 12), .align = "cl", .w = "grow") {
        rcTextInput("search", st->search, sizeof st->search, .className = pal->search,
                    .placeholder = "Search or start a new chat", .leadingIcon = rcIconSearch,
                    .clearButton = true);
        if (withNew && icon_button("list_new", rcIconSquarePen, pal->icon, pal->iconWash,
                                   "New chat (" PRIMARY_KEY "+N)"))
            open_new_chat(st);
    }
}

/* The chips really filter, and compose with the search box. Unread carries the total, so the
   count is on screen before it is chosen. */
static void list_chips(AppState *st, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    const char *total = msg_unread_badge(&st->store);

    rcRow(.gap = 8, .pb = 8, .px = 12, .align = "cl", .w = "grow") {
        for (int k = 0; k < 3; k++) {
            const bool on = (int)st->listFilter == k;
            const char *id = FILTER_IDS[k];

            rcRow(.id = id, .bg = on ? pal->chipOn : RC_UNSET_COLOR, .gap = 5, .px = 12,
                  .align = "cc", .borderRadius = "all-full", .h = "30px",
                  .className = pal->chip) {
                rcTextC(FILTER_LABELS[k], .font = on ? rcFontSemibold(13) : (uint16_t)0,
                        .color = on ? pal->chipOnInk : s.textMuted, .size = 13,
                        .select = RC_SELECT_NONE);
                if (k == FILTER_UNREAD && total[0])
                    rcTextC(total, .font = rcFontSemibold(13),
                            .color = on ? pal->chipOnInk : pal->accent, .select = RC_SELECT_NONE);
            }
            if (rcClicked(id))
                st->listFilter = (MsgFilter)k;
        }
    }
}

/* The list pane: whichever destination the rail chose. Alone on the screen it grows. */
static void msg_list(AppState *st, bool onePane, bool shortView, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();

    rcColumn(.bg = pal->list, .h = "grow", .wType = onePane ? RC_GROW : RC_PX(MSG_LIST_W),
             .className = "min-w-0") {
        if (!onePane && !shortView) {
            rcRow(.gap = 4, .pl = 20, .pr = 10, .align = "cl", .w = "grow",
                  .hType = RC_PX(MSG_HEAD_H)) {
                rcBox(.w = "grow") {
                    rcTextC(NAV_TITLES[st->nav], .font = rcFontSemibold(22), .color = s.text,
                            .select = RC_SELECT_NONE);
                }
                if (st->nav == NAV_CHATS) {
                    if (icon_button("list_new", rcIconSquarePen, pal->icon, pal->iconWash,
                                    "New chat (" PRIMARY_KEY "+N)"))
                        open_new_chat(st);
                    if (rcBeginMenu("list_menu", "", .className = pal->menu,
                                    .variant = RC_BTN_GHOST,
                                    .leadingIcon = rcIconEllipsisVertical)) {
                        if (rcMenuItem("Mark all as read", .leadingIcon = rcIconCheck))
                            for (int k = 0; k < st->store.convCount; k++)
                                msg_mark_read(&st->store, k);
                        if (rcMenuItem("Starred messages", .leadingIcon = rcIconStar))
                            set_nav(st, NAV_STARRED);
                        if (rcMenuItem("Archived", .leadingIcon = rcIconInbox))
                            set_nav(st, NAV_ARCHIVED);
                        if (rcMenuItem("Settings", .leadingIcon = rcIconSettings))
                            st->modalSettings = true;
                        rcEndMenu();
                    }
                }
            }
        }
        if (st->nav == NAV_CHATS) {
            list_search(st, shortView && !onePane, onePane, pal);
            if (!st->search[0] && !shortView)
                list_chips(st, pal);
        }
        /* Under a mouse rcScrollbar takes its own 12 px of this column while the rows overflow,
           so no time or badge sits under the bar. A finger's bar takes no track, so a phone's
           rows keep the whole width. */
        rcColumn(.id = "SideScroll", .scroll = "v",
                 .w = "grow", .h = "grow") {
            int shown = 0;

            if (st->nav == NAV_STARRED) {
                shown = starred_results(st, onePane, pal);
                if (!shown)
                    list_empty("No starred messages",
                               "Hover a message and press the star to keep it here.",
                               rcIconStar);
            } else if (st->search[0]) {
                shown = search_results(st, onePane, pal);
                if (!shown)
                    list_empty("Nothing found",
                               "Check the spelling, or search for a word from the chat.",
                               rcIconSearch);
            } else {
                /* The Channels group sits above the chats while it has something to show. */
                int chats = 0;

                if (st->nav == NAV_CHATS)
                    shown = channels_section(st, onePane, pal);
                for (int o = 0; o < st->store.orderCount; o++)
                    if (row_listed(st, &st->store.conv[st->store.order[o]]))
                        chats++;
                if (shown && chats)
                    list_heading("Chats", s.textMuted);
                for (int o = 0; o < st->store.orderCount; o++) {
                    const int i = st->store.order[o];

                    if (!row_listed(st, &st->store.conv[i]))
                        continue;
                    conv_row(st, i, onePane, pal);
                    shown++;
                }
                if (!shown && st->nav == NAV_ARCHIVED)
                    list_empty("Nothing archived",
                               "Right-click a chat and archive it to tidy your list.", rcIconInbox);
                else if (!shown && st->listFilter == FILTER_UNREAD)
                    list_empty("All caught up", "New messages will show up here.", rcIconCheck);
                else if (!shown)
                    list_empty("No groups yet", "Groups you are in will show up here.",
                               rcIconUsers);
            }
        }
    }
}

/* ---- The thread ------------------------------------------------------------------- */

static void photo_art(int art, float w, float h) {
    rcBox(.overflow = "hidden", .wType = RC_PX(w), .hType = RC_PX(h),
          .gradient = { .from = rcHex(ART_SKY[art][0]), .to = rcHex(ART_SKY[art][1]),
                       .dir = "v" }) {
        rcIconEmit(w, RC_WHITE, art_draw_photo, &ART_INDEX[art]);
    }
}

static void sticker(int art, float size) {
    rcIconEmit(size, RC_WHITE, art_draw_sticker, &ART_INDEX[art]);
}

/* The quote above a reply: who it answers and what they said. A click lands on that line. */
static void quote_block(AppState *st, int conv, const MsgMessage *m, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    const MsgMessage *q = msg_find(&st->store, conv, m->replyTo);
    const char *id = fmt_id(st, "q", m->id);

    if (!q)
        return;
    const char *name = q->dir == MSG_DIR_OUTGOING ? "You"
                     : (q->who && q->who[0] ? q->who : st->store.conv[conv].name);
    const RC_Color ink = q->dir == MSG_DIR_OUTGOING ? pal->accent : sender_ink(name, st->darkMode);

    rcRow(.id = id, .bg = pal->quote, .overflow = "hidden", .borderRadius = "all-sm", .w = "grow") {
        rcBox(.bg = ink, .w = "4px", .h = "grow") {}
        rcColumn(.gap = 1, .px = 8, .py = 5, .overflow = "hidden", .w = "grow") {
            rcTextC(name, .font = rcFontSemibold(13), .color = ink, .select = RC_SELECT_NONE);
            rcTextC(msg_snippet(q), .color = s.textMuted, .size = 13, .wrap = "e",
                    .select = RC_SELECT_NONE);
        }
    }
    if (rcClicked(id))
        jump_to_line(st, m->replyTo);
}

/* The chips under a line: each reaction with its count; yours has the accent's edge and takes
   yours off. */
static void reaction_chips(AppState *st, MsgMessage *m, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();

    for (int k = 0; k < MSG_REACTION_COUNT; k++) {
        if (!m->reactions[k])
            continue;
        const char *id = rcFormat(&st->frame, "rx%u_%d", m->id, k).chars;
        const bool mine = (m->reacted & (1u << k)) != 0;

        rcRow(.id = id, .bg = pal->panel, .gap = 3, .pl = 5, .pr = 6, .py = 2,
              .align = "cc", .borderRadius = "all-full",
              .border = { .color = mine ? pal->accent : pal->panelEdge, .width = 1 },
              .tooltip = mine ? "Remove your reaction" : "React with this too") {
            sticker(k, 14.0f);
            if (m->reactions[k] > 1)
                rcText(rcFormat(&st->frame, "%d", m->reactions[k]), .color = s.textMuted,
                       .size = 11, .select = RC_SELECT_NONE);
        }
        if (rcClicked(id))
            msg_toggle_reaction(m, k);
    }
}

/* The small square buttons of the hover toolbar, washed in their ink as icon_button is. Only
   one toolbar is ever up, so their ids are plain literals. */
static bool tool_button(const char *id, RC_IconCallback icon, RC_Color ink, const char *wash,
                        const char *tip) {
    rcBox(.id = id, .focusable = true, .align = "cc", .borderRadius = "all-sm", .w = "28px",
          .h = "28px", .tooltip = tip, .className = wash) {
        icon(16.0f, ink);
    }
    return rcClicked(id);
}

static void reply_to(AppState *st, uint32_t id) {
    st->replyTo = id;
    st->reactFor = 0;
    rcSetFocus(composer_id(st));
}

static void delete_line(AppState *st, MsgMessage *m) {
    m->deleted = true;
    m->starred = false;
    if (st->replyTo == m->id)
        st->replyTo = 0;
}

/* What the pointer finds beside a line: react, reply, star, copy, delete. On the bubble's outer
   side, in the row's flow, so it moves nothing; a narrow pane has no such room, so there it
   floats over the row's corner. A finger gets the same list from a long press. */
static void line_tools(AppState *st, MsgMessage *m, const MsgPalette *pal) {
    rcRow(.bg = pal->panel, .p = 2, .align = "cl", .borderRadius = "all-md",
          .border = { .color = pal->panelEdge, .width = 1 },
          .shadow = { .color = rcWithAlpha(RC_BLACK, 0.1f), .y = 1.0f, .blur = 3.0f }) {
        if (tool_button("t_react", rcIconSmile, pal->icon, pal->iconWash, "Add a reaction"))
            st->reactFor = st->reactFor == m->id ? 0 : m->id;
        if (tool_button("t_reply", rcIconReply, pal->icon, pal->iconWash, "Reply"))
            reply_to(st, m->id);
        if (m->starred ? tool_button("t_star", rcIconStarFilled, pal->away, pal->awayWash, "Unstar")
                       : tool_button("t_star", rcIconStar, pal->icon, pal->iconWash, "Star"))
            m->starred = !m->starred;
        if (m->textLen && tool_button("t_copy", rcIconCopy, pal->icon, pal->iconWash,
                                      "Copy text")) {
            rcClipboardSet(m->text);
            show_toast(st, "", "Copied to the clipboard", -1);
        }
        if (tool_button("t_delete", rcIconTrash2, pal->icon, pal->iconWash, "Delete"))
            delete_line(st, m);
    }
}

/* The same list as words, for a right-click or a long press on the line. */
static void line_menu(AppState *st, MsgMessage *m, const char *rowId) {
    if (rcBeginContextMenu(fmt_id(st, "lm", m->id), rowId)) {
        if (rcMenuItem("Reply", .leadingIcon = rcIconReply))
            reply_to(st, m->id);
        if (rcMenuItem("Add a reaction", .leadingIcon = rcIconSmile))
            st->reactFor = m->id;
        if (rcMenuItem(m->starred ? "Unstar" : "Star", .leadingIcon = rcIconStar))
            m->starred = !m->starred;
        if (m->textLen && rcMenuItem("Copy text", .leadingIcon = rcIconCopy))
            rcClipboardSet(m->text);
        if (rcMenuItem("Delete", .className = "text-danger hover:bg-danger/10",
                       .leadingIcon = rcIconTrash2))
            delete_line(st, m);
        rcEndContextMenu();
    }
}

/* The quick reactions over a line, in a popup that escapes the thread's clip. A press
   anywhere else closes it (close_popups). */
static void reaction_bar(AppState *st, MsgMessage *m, bool mineSide, const MsgPalette *pal) {
    rcRow(.id = "react_bar", .bg = pal->panel, .gap = 2, .p = 4, .align = "cl",
          .borderRadius = "all-full", .border = { .color = pal->panelEdge, .width = 1 },
          .shadow = { .color = rcWithAlpha(RC_BLACK, 0.19f), .y = 2.0f, .blur = 10.0f },
          .floating = { .to = RC_ATTACH_PARENT,
                        .parent = mineSide ? RC_ANCHOR_TOP_RIGHT : RC_ANCHOR_TOP_LEFT,
                        .element = mineSide ? RC_ANCHOR_BOTTOM_RIGHT : RC_ANCHOR_BOTTOM_LEFT,
                        .offset = { 0.0f, -6.0f }, .zIndex = 20, .clip = RC_CLIP_NONE }) {
        for (int k = 0; k < MSG_REACTION_COUNT; k++) {
            const char *id = rcFormat(&st->frame, "pick%d", k).chars;
            const bool mine = (m->reacted & (1u << k)) != 0;

            rcBox(.id = id, .bg = mine ? pal->chipOn : RC_UNSET_COLOR, .align = "cc",
                  .borderRadius = "all-full", .w = "36px", .h = "36px", .className = pal->tile) {
                sticker(k, 26.0f);
            }
            if (rcClicked(id)) {
                msg_toggle_reaction(m, k);
                st->reactFor = 0;
            }
        }
    }
}

/* Time and ticks, the small print inside a bubble. */
static void bubble_meta(const AppState *st, const MsgMessage *m, RC_Color ink,
                        const MsgPalette *pal) {
    rcRow(.gap = 3, .align = "cl") {
        if (m->starred)
            rcIconStarFilled(11.0f, ink);
        rcTextC(m->ts, .color = ink, .size = 11, .select = RC_SELECT_NONE);
        if (m->dir == MSG_DIR_OUTGOING && m->deliv != MSG_DELIV_FAILED && !m->deleted)
            delivery_mark(m->deliv, st->store.readReceipts, ink, pal);
    }
}

static bool has_reactions(const MsgMessage *m) {
    for (int k = 0; k < MSG_REACTION_COUNT; k++)
        if (m->reactions[k])
            return true;
    return false;
}

/* Whether the search in this chat finds the line. */
static bool line_hit(const AppState *st, const MsgMessage *m) {
    return st->threadSearchOn && st->threadSearch[0] && m->textLen && !m->deleted
        && msg_text_matches(m->text, st->threadSearch);
}

/* A new line opens up from nothing, and the thread's stick-to-end follows it down: the .hMax
   its row takes while it arrives, 0 once it is in. The box to ease toward is its own last
   layout; the very first frame has none. */
static float line_open_h(AppState *st, const MsgMessage *m) {
    if (m->age >= MSG_ENTER_SECONDS)
        return 0.0f;
    const RC_Box was = rcGetElementBox(fmt_id(st, "mb", m->id));
    const float t = m->age / MSG_ENTER_SECONDS;
    const float ease = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);

    return was.found && was.height > 0.0f ? was.height * ease : 1.0f;
}

/* One bubble. Yours right in the accent's tint, theirs left; `first` starts a run from one
   side, which is the bubble that carries a tail and, in a group, the name. */
static void bubble(AppState *st, int conv, MsgMessage *m, bool first, float maxW,
                   const MsgPalette *pal) {
    static const char *const CORNERS[3] = {
        "rounded-lg items-stretch", "rounded-lg rounded-tl-none items-stretch",
        "rounded-lg rounded-tr-none items-stretch",
    };
    RC_Style s = rcGetStyle();
    const MsgConversation *c = &st->store.conv[conv];
    const bool out = m->dir == MSG_DIR_OUTGOING;
    const bool group = c->kind != MSG_CONV_DIRECT;
    const bool stickerOnly = m->kind == MSG_KIND_STICKER && !m->deleted;
    const bool flash = m->id == st->flashId && st->flashLeft > 0.0f;
    RC_Color fill = out ? pal->bubbleOut : pal->bubbleIn;
    const char *rowId = fmt_id(st, "m", m->id);
    const bool hovered = !rcPointerIsCoarse() && rcShowsHover(rowId);
    const bool hasReactions = has_reactions(m);
    const float openH = line_open_h(st, m);
    /* A photo sets its bubble's width: its caption wraps under it, at its width. */
    const bool photo = m->kind == MSG_KIND_PHOTO && !m->deleted;
    const float photoW = maxW - 16.0f < 300.0f ? maxW - 16.0f : 300.0f;

    if (line_hit(st, m))
        fill = pal->hit;
    if (flash)
        fill = pal->hitNow;
    /* A short line keeps its time on the same line; a long one gives it a line of its own.
       Measuring beats guessing: the width decides, whatever the window. */
    const float metaW = out ? 52.0f : 34.0f;
    const bool sameLine = m->kind == MSG_KIND_TEXT && !m->deleted
                       && rcTextWidth(0, 14, m->text, m->textLen) + metaW + 24.0f <= maxW;

    rcBox(.id = rowId, .w = "grow",
          .hMax = openH > 0.0f ? RC_VAL(openH) : RC_UNSET_VAL,
          .className = openH > 0.0f ? "overflow-hidden shrink-0" : "") {
        rcRow(.id = fmt_id(st, "mb", m->id), .pt = (float)(first ? 8 : 0),
              .pb = (float)(hasReactions ? 20 : 0), .align = "tl", .w = "grow") {
            if (out) {
                rcSpacer() {}
                if (hovered && !st->threadTight)
                    rcBox(.pr = 8) { line_tools(st, m, pal); }
                if (m->deliv == MSG_DELIV_FAILED) {
                    const char *retry = fmt_id(st, "retry", m->id);

                    rcRow(.id = retry, .gap = 4, .pr = 8, .align = "cc", .h = "32px",
                          .tooltip = "Send it again") {
                        rcIconCircleAlert(18.0f, pal->error);
                        rcTextL("Not sent. Retry", .color = pal->error, .size = 12,
                                .select = RC_SELECT_NONE);
                    }
                    if (rcClicked(retry))
                        msg_retry_failed(&st->store, conv);
                }
            }
            if (!out) {
                if (first && !stickerOnly)
                    rcIconEmit(MSG_TAIL_W, fill, art_draw_tail_in, NULL);
                else
                    rcBox(.wType = RC_PX(MSG_TAIL_W)) {}
            }
            rcColumn(.bg = stickerOnly ? RC_UNSET_COLOR : fill, .gap = 3,
                     .pt = 5, .pb = 5, .pl = 8, .pr = 8,
                     .wMax = maxW > 0.0f ? RC_VAL(photo ? photoW + 16.0f : maxW) : RC_UNSET_VAL,
                     .shadow = { .color = stickerOnly ? RC_UNSET_COLOR
                                                      : rcWithAlpha(RC_BLACK, 0.133f),
                                 .y = 1.0f, .blur = 1.0f },
                     .className = CORNERS[!first || stickerOnly ? 0 : (out ? 2 : 1)]) {
                if (first && group && !out && m->who && !stickerOnly)
                    rcTextC(m->who, .font = rcFontSemibold(13),
                            .color = sender_ink(m->who, st->darkMode), .select = RC_SELECT_NONE);
                if (m->deleted) {
                    rcRow(.gap = 6, .align = "cl") {
                        rcIconBan(14.0f, s.textMuted);
                        rcTextL("You deleted this message", .color = s.textMuted, .size = 13,
                                .select = RC_SELECT_NONE);
                        bubble_meta(st, m, pal->meta, pal);
                    }
                } else if (stickerOnly) {
                    /* A sticker has no bubble, so its time rides its corner in a chip. */
                    rcBox() {
                        sticker(m->art, 96.0f);
                        rcBox(.bg = fill, .px = 6, .py = 1, .borderRadius = "all-full",
                              .floating = { .to = RC_ATTACH_PARENT,
                                            .parent = RC_ANCHOR_BOTTOM_RIGHT,
                                            .element = RC_ANCHOR_BOTTOM_RIGHT }) {
                            bubble_meta(st, m, pal->meta, pal);
                        }
                    }
                } else {
                    if (m->replyTo)
                        quote_block(st, conv, m, pal);
                    if (photo) {
                        const char *open = fmt_id(st, "ph", m->id);

                        rcBox(.id = open, .tooltip = "Open") {
                            photo_art(m->art, photoW, photoW * 0.75f);
                        }
                        if (rcClicked(open)) {
                            st->modalPhoto = true;
                            st->photoShown = m->id;
                            st->photoConv = conv;
                        }
                    }
                    if (sameLine) {
                        rcRow(.gap = 10, .align = "bl", .w = "grow") {
                            rcBox(.w = "grow") { rcTextC(m->text, .color = s.text); }
                            bubble_meta(st, m, pal->meta, pal);
                        }
                    } else {
                        if (m->textLen)
                            rcTextC(m->text, .color = s.text);
                        rcRow(.align = "cr", .w = "grow") { bubble_meta(st, m, pal->meta, pal); }
                    }
                }
                if (hasReactions)
                    rcRow(.gap = 4, .align = "cl",
                          .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_BOTTOM_LEFT,
                                        .element = RC_ANCHOR_TOP_LEFT,
                                        .offset = { 8.0f, -4.0f } }) {
                        reaction_chips(st, m, pal);
                    }
                if (st->reactFor == m->id)
                    reaction_bar(st, m, out, pal);
            }
            if (out) {
                if (first && !stickerOnly)
                    rcIconEmit(MSG_TAIL_W, fill, art_draw_tail_out, NULL);
                else
                    rcBox(.wType = RC_PX(MSG_TAIL_W)) {}
            } else {
                if (hovered && !st->threadTight)
                    rcBox(.pl = 8) { line_tools(st, m, pal); }
                rcSpacer() {}
            }
        }
        if (hovered && st->threadTight)
            rcBox(.floating = { .to = RC_ATTACH_PARENT,
                                .parent = out ? RC_ANCHOR_TOP_LEFT : RC_ANCHOR_TOP_RIGHT,
                                .element = out ? RC_ANCHOR_TOP_LEFT : RC_ANCHOR_TOP_RIGHT,
                                .offset = { 0.0f, -8.0f } }) {
                line_tools(st, m, pal);
            }
    }
    if (!m->deleted)
        line_menu(st, m, rowId);
}

/* A line in a channel, in a team chat's flat form: a square, a bold name and a time, then the
   text; a run from one person within five minutes shares the first one's square. */
static void flat_row(AppState *st, int conv, MsgMessage *m, bool first, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    const bool out = m->dir == MSG_DIR_OUTGOING;
    const char *name = out ? st->myName : (m->who ? m->who : "");
    const char *rowId = fmt_id(st, "m", m->id);
    const bool hot = rcShowsHover(rowId);
    const bool flash = m->id == st->flashId && st->flashLeft > 0.0f;
    const uint32_t tint = name_hash(name) % 8u;
    const RC_Color rest = hot ? pal->rowHover : RC_UNSET_COLOR;
    const RC_Color back = flash ? pal->hitNow : (line_hit(st, m) ? pal->hit : rest);
    const float openH = line_open_h(st, m);

    rcBox(.id = rowId, .bg = back, .w = "grow",
          .hMax = openH > 0.0f ? RC_VAL(openH) : RC_UNSET_VAL,
          .className = openH > 0.0f ? "overflow-hidden shrink-0" : "") {
        rcRow(.id = fmt_id(st, "mb", m->id), .gap = 10, .pt = (float)(first ? 10 : 2), .pb = 2,
              .pl = 20, .pr = 20, .align = "tl", .w = "grow") {
            if (first) {
                rcBox(.align = "cc", .borderRadius = "all-md", .w = "36px", .h = "36px",
                      .gradient = { .from = rcHex(SENDER_TINT[tint][0]),
                                    .to = rcHex(SENDER_TINT[tint][1]), .dir = "d" }) {
                    rcTextC(first_char(st, name), .font = rcFontSemibold(16), .color = RC_WHITE,
                            .select = RC_SELECT_NONE);
                }
            } else {
                rcBox(.w = "36px") {}
            }
            rcColumn(.gap = 2, .overflow = "hidden", .w = "grow", .className = "items-stretch") {
                if (first)
                    rcRow(.gap = 8, .align = "bl", .w = "grow") {
                        rcTextC(name, .font = rcFontSemibold(14), .color = s.text);
                        rcTextC(m->ts, .color = s.textMuted, .size = 12,
                                .select = RC_SELECT_NONE);
                    }
                if (m->replyTo)
                    quote_block(st, conv, m, pal);
                if (m->deleted) {
                    rcRow(.gap = 6, .align = "cl") {
                        rcIconBan(14.0f, s.textMuted);
                        rcTextL("You deleted this message", .color = s.textMuted);
                    }
                } else if (m->kind == MSG_KIND_STICKER) {
                    sticker(m->art, 80.0f);
                } else if (m->kind == MSG_KIND_PHOTO) {
                    const char *open = fmt_id(st, "ph", m->id);

                    rcBox(.id = open, .tooltip = "Open") {
                        photo_art(m->art, 300.0f, 225.0f);
                    }
                    if (rcClicked(open)) {
                        st->modalPhoto = true;
                        st->photoShown = m->id;
                        st->photoConv = conv;
                    }
                    if (m->textLen)
                        rcTextC(m->text, .color = s.text);
                } else {
                    rcTextC(m->text, .color = m->deliv == MSG_DELIV_QUEUED ? s.textMuted : s.text);
                }
                if (out && (m->deliv == MSG_DELIV_QUEUED || m->deliv == MSG_DELIV_SENDING))
                    rcTextL("Sending...", .color = s.textMuted, .size = 12,
                            .select = RC_SELECT_NONE);
                if (out && m->deliv == MSG_DELIV_FAILED) {
                    const char *retry = fmt_id(st, "retry", m->id);

                    rcRow(.id = retry, .gap = 6, .align = "cl", .tooltip = "Send it again") {
                        rcIconCircleAlert(14.0f, pal->error);
                        rcTextL("Not delivered. Send again", .color = pal->error, .size = 12,
                                .select = RC_SELECT_NONE);
                    }
                    if (rcClicked(retry))
                        m->deliv = MSG_DELIV_QUEUED;   /* live_room.h sends it on its next frame */
                }
                if (has_reactions(m))
                    rcRow(.gap = 4, .pt = 2, .align = "cl") { reaction_chips(st, m, pal); }
            }
        }
        if (hot)
            rcBox(.floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_RIGHT,
                                .element = RC_ANCHOR_TOP_RIGHT, .offset = { -16.0f, -8.0f } }) {
                line_tools(st, m, pal);
            }
        if (st->reactFor == m->id)
            reaction_bar(st, m, true, pal);
    }
    if (!m->deleted)
        line_menu(st, m, rowId);
}

/* A centered note in the thread: a day, how many are unread, who came or went. */
static void thread_pill(const char *text, RC_Color back, RC_Color ink) {
    rcRow(.pt = 8, .pb = 4, .align = "cc", .w = "grow") {
        rcBox(.bg = back, .px = 12, .py = 5, .borderRadius = "all-md",
              .shadow = { .color = rcWithAlpha(RC_BLACK, 0.08f), .y = 1.0f, .blur = 1.0f }) {
            rcTextC(text, .color = ink, .size = 12, .select = RC_SELECT_NONE);
        }
    }
}

/* The dots while someone writes: a bubble of their own at the thread's end, where the
   answer will land. */
static void typing_bubble(AppState *st, const MsgConversation *c, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();

    if (c->kind == MSG_CONV_CHANNEL) {
        rcRow(.gap = 6, .pt = 4, .pl = 66, .align = "cl", .w = "grow") {
            rcIconEmit(22.0f, s.textMuted, art_draw_typing, &st->typingPhase);
            rcTextC(c->typing, .color = s.textMuted, .size = 12, .select = RC_SELECT_NONE);
        }
        return;
    }
    rcRow(.pt = 6, .align = "tl", .w = "grow") {
        rcIconEmit(MSG_TAIL_W, pal->bubbleIn, art_draw_tail_in, NULL);
        rcBox(.bg = pal->bubbleIn, .px = 12, .py = 6,
              .shadow = { .color = rcWithAlpha(RC_BLACK, 0.133f), .y = 1.0f, .blur = 1.0f },
              .className = "rounded-lg rounded-tl-none") {
            rcIconEmit(28.0f, s.textMuted, art_draw_typing, &st->typingPhase);
        }
    }
}

/* The lines themselves. A day's pill wherever the day changes, the unread line where you left
   off. Each line is a bubble or, in a channel, a flat row. */
static void thread_lines(AppState *st, MsgConversation *c, float maxW, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    const int conv = st->openConv;
    const bool channel = c->kind == MSG_CONV_CHANNEL;
    int prevDay = -1;
    const MsgMessage *prev = NULL;

    for (int i = 0; i < c->count; i++) {
        MsgMessage *m = &c->items[i];
        bool first = !prev || prev->dir != m->dir || prev->kind == MSG_KIND_EVENT
                  || (c->kind != MSG_CONV_DIRECT && !msg_same_text(prev->who, m->who));

        if (m->day != prevDay) {
            thread_pill(msg_day_word(&st->store, m->day), pal->pill, s.textMuted);
            first = true;
            prevDay = m->day;
        }
        if (channel && prev && m->clock - prev->clock >= 300u)
            first = true;
        if (st->unreadMarkConv == conv && st->unreadMarkId == m->id && st->unreadMarkCount > 0) {
            thread_pill(st->unreadMarkCount == 1 ? "1 unread message"
                        : rcFormat(&st->frame, "%d unread messages", st->unreadMarkCount).chars,
                        pal->pill, pal->accent);
            first = true;
        }
        if (m->kind == MSG_KIND_EVENT) {
            thread_pill(m->textLen ? rcFormat(&st->frame, "%s %s", m->who, m->text).chars : m->who,
                        channel ? pal->rowActive : pal->pill, s.textMuted);
        } else if (channel) {
            flat_row(st, conv, m, first, pal);
        } else {
            bubble(st, conv, m, first, maxW, pal);
        }
        prev = m;
    }
    if (c->typing[0])
        typing_bubble(st, c, pal);
}

/* Search in this chat: it walks the lines that say it, newest first, and the current one
   comes into view and glows. */
static void thread_search_bar(AppState *st, const MsgConversation *c, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    const bool go = rcKeyPressedOn("thread_search", RC_KEY_ENTER);
    uint32_t hits[MSG_MAX_MSGS_PER_CONV];
    int n = 0;
    bool moved = go;

    rcRow(.bg = pal->header, .gap = 6, .px = 12, .py = 8, .align = "cl", .w = "grow") {
        if (rcTextInput("thread_search", st->threadSearch, sizeof st->threadSearch,
                        .className = pal->find, .placeholder = "Search this chat",
                        .leadingIcon = rcIconSearch, .clearButton = true)) {
            st->matchAt = 0;
            moved = true;
        }
        /* Counted after the field, so a letter typed or its x's clear counts on this frame. */
        if (st->threadSearch[0]) {
            for (int i = c->count - 1; i >= 0; i--) {
                const MsgMessage *m = &c->items[i];

                if (m->textLen && !m->deleted && m->kind != MSG_KIND_EVENT
                    && msg_text_matches(m->text, st->threadSearch))
                    hits[n++] = m->id;
            }
        }
        st->matchCount = n;
        rcTextC(n ? rcFormat(&st->frame, "%d of %d", st->matchAt + 1, n).chars
                  : (st->threadSearch[0] ? "No results" : ""),
                .color = s.textMuted, .size = 13, .select = RC_SELECT_NONE);
        if (icon_button("match_older", rcIconArrowUp, pal->icon, pal->iconWash, "Older match")
            && n) {
            st->matchAt = (st->matchAt + 1) % n;
            moved = true;
        } else if (go && n) {
            st->matchAt = (st->matchAt + 1) % n;
        }
        if (icon_button("match_newer", rcIconArrowDown, pal->icon, pal->iconWash, "Newer match")
            && n) {
            st->matchAt = (st->matchAt + n - 1) % n;
            moved = true;
        }
        if (icon_button("match_close", rcIconX, pal->icon, pal->iconWash, "Close the search")) {
            st->threadSearchOn = false;
            st->threadSearch[0] = '\0';
        }
    }
    if (st->matchAt >= n)
        st->matchAt = 0;
    if (moved && n)
        jump_to_line(st, hits[st->matchAt]);
}

/* What a channel says over its lines: demo or live, and the one thing to do about it. This
   is the fold's front door: nothing opens a socket until "Connect to a live room" is pressed. */
static void channel_strip(AppState *st, const MsgConversation *c, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    LiveRoom *lv = &st->live;
    const char *text = NULL;
    const char *word = NULL;
    bool primary = false;
    LiveAsk ask = LIVE_ASK_CONNECT;
    RC_IconCallback icon = rcIconInfo;
    RC_Color ink = s.textMuted;

    switch (lv->link) {
    case LIVE_DEMO:
        if (lv->offline) {
            text = lv->failText[0] ? lv->failText : "Could not reach the chat server.";
            word = "Retry";
            icon = rcIconTriangleAlert;
            ink = pal->away;
        } else {
            text = "Demo room: the others are simulated, and your lines stay here.";
            word = "Connect to a live room";
            primary = true;
        }
        break;
    case LIVE_CONNECTING:
        text = lv->tries ? rcFormat(&st->frame, "Reconnecting, attempt %d...", lv->tries).chars
                         : rcFormat(&st->frame, "Connecting to #%s...", c->name).chars;
        break;
    case LIVE_JOINING:
        text = rcFormat(&st->frame, "Joining #%s...", c->name).chars;
        break;
    /* In words: the server's own reason when it gave one. The code, the cleanness and the
       stack's message are the details pane's Last close, where the socket is laid open. */
    case LIVE_WAITING:
        text = rcFormat(&st->frame, "%s. Reconnecting in %d s.",
                        lv->closeWhy[0] ? lv->closeWhy : "Connection lost",
                        (int)(lv->retryIn + 0.999f)).chars;
        word = "Reconnect now";
        icon = rcIconTriangleAlert;
        ink = pal->away;
        break;
    case LIVE_CLOSED:
        text = lv->closeWhy[0] ? rcFormat(&st->frame, "%s.", lv->closeWhy).chars
                               : "The room closed.";
        word = "Reconnect";
        icon = rcIconCircleAlert;
        ink = pal->error;
        break;
    default:
        return;     /* an open room says nothing: it is simply working */
    }
    rcRow(.bg = pal->rowActive, .gap = 10, .pl = 20, .pr = 12, .py = 6, .align = "cl", .w = "grow",
          .hMin = RC_VAL(44.0f)) {
        icon(16.0f, ink);
        rcBox(.overflow = "hidden", .w = "grow") {
            rcTextC(text, .color = s.text, .size = 13, .select = RC_SELECT_NONE);
        }
        if (word && rcButton("strip_go", word, .className = by_pointer("h-8", "h-11"),
                             .variant = primary ? RC_BTN_PRIMARY : RC_BTN_DEFAULT)) {
            lv->ask = ask;
            lv->askRoom = st->openConv;
        }
    }
}

/* The bar over the composer while you are replying. */
static void reply_bar(AppState *st, int conv, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    const MsgMessage *q = msg_find(&st->store, conv, st->replyTo);

    if (!q) {
        st->replyTo = 0;
        return;
    }
    const char *name = q->dir == MSG_DIR_OUTGOING ? "You"
                     : (q->who && q->who[0] ? q->who : st->store.conv[conv].name);

    rcRow(.bg = pal->header, .gap = 8, .pt = 8, .pl = 16, .pr = 10, .align = "cl", .w = "grow") {
        rcRow(.bg = pal->quote, .overflow = "hidden", .borderRadius = "all-sm", .w = "grow") {
            rcBox(.bg = pal->accent, .w = "4px", .h = "grow") {}
            rcColumn(.gap = 1, .px = 10, .py = 6, .overflow = "hidden", .w = "grow") {
                rcText(rcFormat(&st->frame, "Replying to %s", name),
                       .font = rcFontSemibold(13),
                       .color = pal->accent, .select = RC_SELECT_NONE);
                rcTextC(msg_snippet(q), .color = s.textMuted, .size = 13, .wrap = "e",
                        .select = RC_SELECT_NONE);
            }
        }
        if (icon_button("reply_close", rcIconX, pal->icon, pal->iconWash, "Cancel the reply (Esc)"))
            st->replyTo = 0;
    }
}

/* The stickers, over the composer. A sticker is a line: it sends at once. */
static void sticker_panel(AppState *st, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();

    rcColumn(.id = "sticker_panel", .bg = pal->panel, .gap = 8, .p = 12, .borderRadius = "all-xl",
             .border = { .color = pal->panelEdge, .width = 1 },
             .shadow = { .color = rcWithAlpha(RC_BLACK, 0.19f), .y = 4.0f, .blur = 16.0f },
             .floating = { .to = RC_ATTACH_ELEMENT, .toId = "composer_bar",
                           .parent = RC_ANCHOR_TOP_LEFT, .element = RC_ANCHOR_BOTTOM_LEFT,
                           .offset = { 10.0f, -4.0f }, .zIndex = 20, .clip = RC_CLIP_NONE }) {
        rcTextL("Stickers", .font = rcFontSemibold(13), .color = s.textMuted,
                .select = RC_SELECT_NONE);
        for (int r = 0; r < 2; r++) {
            rcRow(.gap = 4) {
                for (int k = 0; k < 6; k++) {
                    const int art = r * 6 + k;
                    const char *id = rcFormat(&st->frame, "stk%d", art).chars;

                    rcBox(.id = id, .align = "cc", .borderRadius = "all-md", .w = "52px",
                          .h = "52px", .className = pal->tile) {
                        sticker(art, 40.0f);
                    }
                    if (rcClicked(id)) {
                        send_art(st, MSG_KIND_STICKER, art, NULL);
                        st->stickerOpen = false;
                    }
                }
            }
        }
    }
}

/* Anchored under the lines: stickers, attach, the field, send. */
static void composer(AppState *st, const MsgConversation *c, bool canWrite, const MsgPalette *pal) {
    const char *field = composer_id(st);
    char *draft = st->drafts[st->openConv];
    const bool ready = draft[0] != '\0' && canWrite;
    const float hit = hit_size();

    if (st->replyTo)
        reply_bar(st, st->openConv, pal);
    rcRow(.id = "composer_bar", .bg = pal->header, .gap = 2, .px = 8, .py = 8, .align = "cl",
          .w = "grow") {
        if (st->stickerOpen ? icon_button("btn_emoji", rcIconSmile, pal->accent, pal->accentWash,
                                          "Stickers")
                            : icon_button("btn_emoji", rcIconSmile, pal->icon, pal->iconWash,
                                          "Stickers"))
            st->stickerOpen = !st->stickerOpen;
        if (icon_button("btn_attach", rcIconPaperclip, pal->icon, pal->iconWash, "Attach a photo"))
            st->modalAttach = true;
        rcBox(.px = 6, .w = "grow") {
            rcTextInput(field, draft, sizeof st->drafts[0], .disabled = !canWrite,
                        .className = pal->field,
                        .placeholder = c->kind == MSG_CONV_CHANNEL
                                     ? rcFormat(&st->frame, "Message #%s", c->name).chars
                                     : "Type a message");
        }
        /* The send button wakes up with the text: a disc in the theme's primary once there is
           something to send, a quiet glyph before that. A class fill gets no press shade of its
           own, so the disc names one: a step under the hover, as a primary rcButton darkens. */
        rcBox(.id = "btn_send", .focusable = ready, .align = "cc", .borderRadius = "all-full",
              .wType = RC_PX(hit), .hType = RC_PX(hit), .tooltip = "Send",
              .className = ready ? "bg-primary hover:bg-primary-hover active:bg-[#005d4c]" : "") {
            rcIconSendSolid(20.0f, ready ? RC_WHITE : pal->icon);
        }
    }
    if (st->stickerOpen)
        sticker_panel(st, pal);
}

/* The top of every thread: a channel's welcome, as a team chat opens a room, or a note that says
   what this demo is, in the spot where a real client says how a chat is protected. */
static void thread_top(AppState *st, const MsgConversation *c, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();

    if (c->kind == MSG_CONV_CHANNEL) {
        rcColumn(.gap = 6, .pt = 24, .pb = 8, .px = 20, .w = "grow") {
            rcBox(.bg = pal->rowActive, .align = "cc", .borderRadius = "all-lg", .w = "48px",
                  .h = "48px") {
                rcIconHash(24.0f, s.text);
            }
            rcText(rcFormat(&st->frame, "Welcome to #%s", c->name),
                   .font = rcFontSemibold(22), .color = s.text);
            rcText(rcFormat(&st->frame, "This is the start of the #%s channel. %s", c->name,
                            c->about),
                   .color = s.textMuted);
        }
    } else {
        thread_pill(c->count
                        ? "A demo chat: everyone here is simulated, and nothing leaves this device."
                        : "No messages here yet. Say hello!",
                    pal->notice, pal->noticeInk);
    }
}

/* The thread pane. The wallpaper floats at z -1, under all in-flow content and so under this
   pane's own lines; nothing in the flow behind it paints a background, or it would hide. */
static void msg_thread(AppState *st, bool onePane, float paneW, float paneH, float bubbleMax,
                       const MsgPalette *pal) {
    MsgConversation *c = &st->store.conv[st->openConv];
    const bool channel = c->kind == MSG_CONV_CHANNEL;
    const bool canWrite = !channel || st->live.link == LIVE_DEMO
                       || (st->live.link == LIVE_OPEN && st->live.room == st->openConv);
    const char *listId = thread_id(st, st->openConv);
    const RC_ScrollInfo info = rcGetScrollInfo(listId);
    const bool farUp = info.found && info.maxOffsetY - info.offsetY > 160.0f;
    const float sidePad = st->threadTight ? MSG_THREAD_PX_NARROW : MSG_THREAD_PX_WIDE;
    const float padX = channel ? 0.0f : sidePad;
    const char *field = composer_id(st);

    st->threadDrawn = st->openConv;
    /* The send is read before any line is declared (send_draft's note). Enter sends unless
       Settings says otherwise; polling btn_send is what gives the disc its pointer. */
    if (rcClicked("btn_send") || (st->enterSends && rcKeyPressedOn(field, RC_KEY_ENTER)))
        if (canWrite)
            send_draft(st);
    /* A jump waits until the thread has been laid out with the line in it. */
    if (st->scrollTo && info.found) {
        const char *target = fmt_id(st, "m", st->scrollTo);

        if (rcGetElementBox(target).found) {
            rcScrollIntoView(listId, target, RC_SCROLL_CENTER);
            st->scrollTo = 0;
        }
    }
    rcColumn(.bg = channel ? pal->sheet : RC_UNSET_COLOR, .overflow = "hidden", .w = "grow",
             .h = "grow") {
        if (!channel)
            rcBox(.bg = pal->wall, .overflow = "hidden", .w = "grow", .h = "grow",
                  .floating = { .to = RC_ATTACH_PARENT, .zIndex = -1,
                                .capture = RC_CAPTURE_PASSTHROUGH }) {
                rcIconEmit(paneW > paneH ? paneW : paneH, pal->doodle, art_draw_wallpaper, NULL);
            }
        if (!onePane) {
            rcRow(.bg = pal->header, .gap = 4, .pl = 16, .pr = 8, .align = "cl", .w = "grow",
                  .hType = RC_PX(MSG_HEAD_H)) {
                chat_header(st, c, false, true, rcGetStyle().text, rcGetStyle().textMuted, pal);
            }
        }
        if (st->threadSearchOn)
            thread_search_bar(st, c, pal);
        if (channel)
            channel_strip(st, c, pal);
        rcBox(.overflow = "hidden", .w = "grow", .h = "grow") {
            /* A short chat sits at the bottom, by the composer: a capital B is CSS's safe end,
               so it packs low until it fills the pane and then scrolls as usual. */
            rcColumn(.id = listId, .gap = 2, .pt = 8, .pb = 12, .pl = padX, .pr = padX,
                     .align = "Bl", .scroll = "v", .scrollStickToEnd = true, .w = "grow",
                     .h = "grow", .scrollOffset = { 0.0f, MSG_THREAD_END }) {
                thread_top(st, c, pal);
                thread_lines(st, c, bubbleMax, pal);
            }
            if (farUp) {
                rcBox(.id = "jump_down", .bg = pal->panel, .align = "cc",
                      .borderRadius = "all-full",
                      .border = { .color = pal->panelEdge, .width = 1 },
                      .w = "40px", .h = "40px",
                      .shadow = { .color = rcWithAlpha(RC_BLACK, 0.157f), .y = 2.0f, .blur = 8.0f },
                      .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_BOTTOM_RIGHT,
                                    .element = RC_ANCHOR_BOTTOM_RIGHT, .offset = { -20.0f, -14.0f },
                                    .zIndex = 2 },
                      .tooltip = "Jump to the newest") {
                    rcIconArrowDown(20.0f, pal->icon);
                }
                if (rcClicked("jump_down"))
                    rcScrollToBottom(listId);
            }
        }
        rcColumn(.id = "composer_dock", .w = "grow") {
            composer(st, c, canWrite, pal);
        }
    }
}

/* ---- The details ------------------------------------------------------------------ */

/* One label and its value, the value wrapping under itself. */
static void fact(const char *label, const char *value) {
    RC_Style s = rcGetStyle();

    rcRow(.gap = 12, .align = "tl", .w = "grow") {
        rcBox(.w = "96px") { rcTextC(label, .color = s.textMuted, .size = 12); }
        rcBox(.overflow = "hidden", .w = "grow") {
            rcTextC(value, .color = s.text, .size = 12);
        }
    }
}

static void info_card_heading(const char *text) {
    rcTextC(text, .color = rcGetStyle().textMuted, .size = 13, .select = RC_SELECT_NONE);
}

static const char *size_words(AppState *st, uint64_t bytes) {
    if (bytes < 1024u)
        return rcFormat(&st->frame, "%u B", (unsigned)bytes).chars;
    if (bytes < 1024u * 1024u)
        return rcFormat(&st->frame, "%.1f KB", (double)bytes / 1024.0).chars;
    return rcFormat(&st->frame, "%.1f MB", (double)bytes / (1024.0 * 1024.0)).chars;
}

/* The room's socket, laid open: where it points and what it agreed, how the last one closed,
   the send buffer against its cap, the traffic, every event in order. Its Simulate menu asks
   serve.py for each state on purpose, so all of it can be seen. */
static void live_panel(AppState *st, bool open, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    LiveRoom *lv = &st->live;
    const bool demo = lv->link == LIVE_DEMO;

    rcColumn(.bg = pal->list, .gap = 10, .p = 16, .w = "grow") {
        rcRow(.gap = 8, .align = "cl", .w = "grow", .hMin = RC_VAL(32.0f)) {
            rcBox(.w = "grow") { info_card_heading("Live room"); }
            if (rcBeginMenu("simulate", "Simulate", .disabled = !open, .caret = true)) {
                if (rcMenuItem("Drop the connection (no Close)"))
                    lv->ask = LIVE_ASK_DROP;
                if (rcMenuItem("Close the room (4001)"))
                    lv->ask = LIVE_ASK_CLOSE;
                if (rcMenuItem("Restart the server (1012)"))
                    lv->ask = LIVE_ASK_RESTART;
                if (rcMenuItem("Stall the server's reads (10 s)"))
                    lv->ask = LIVE_ASK_STALL;
                if (rcMenuItem("Send 2 MB of bulk data"))
                    lv->ask = LIVE_ASK_BURST;
                rcEndMenu();
            }
        }
        rcTextInput("server", lv->server, sizeof lv->server,
                    .placeholder = lv->relative ? "This page's server" : "ws://127.0.0.1:8789",
                    .clearButton = true);
        rcRow(.gap = 8, .w = "grow") {
            const char *go = demo ? (lv->offline ? "Retry" : "Connect to a live room")
                           : "Reconnect";

            if (rcButton("live_go", go, .variant = demo ? RC_BTN_PRIMARY : RC_BTN_DEFAULT)) {
                lv->ask = LIVE_ASK_CONNECT;
                lv->askRoom = st->openConv;
            }
            if (!demo && rcButton("live_leave", "Back to the demo room"))
                lv->ask = LIVE_ASK_LEAVE;
        }
        if (demo && !lv->offline)
            rcTextL("Run python3 examples/ex24_messenger/serve.py, then connect two windows.",
                    .color = s.textMuted, .size = 12);
        if (demo && !lv->offline) {
            fact("Address", "None: the demo room makes no request.");
        } else {
            fact("Address", lv->url[0] ? lv->url : "-");
            fact("Subprotocol", lv->protocol[0] ? lv->protocol
                                : (lv->link == LIVE_OPEN ? "None agreed" : "-"));
            fact("Upgrade", lv->status ? rcFormat(&st->frame, "HTTP %d", lv->status).chars
                            : (lv->link == LIVE_OPEN ? "Done by the browser" : "-"));
            fact("Last close", lv->closeText[0] ? lv->closeText : "None yet");
        }
        fact("Send buffer", rcFormat(&st->frame, "%s of %s waiting to go out",
                                     size_words(st, lv->buffered),
                                     size_words(st, LIVE_CAP_BUFFERED)).chars);
        rcProgress("live_buffer", (float)((double)lv->buffered / (double)LIVE_CAP_BUFFERED));
        if (lv->peak)
            fact("Peak", size_words(st, lv->peak));
        fact("In", rcFormat(&st->frame, "%u message%s, %s", lv->msgsIn, lv->msgsIn == 1 ? "" : "s",
                            size_words(st, lv->bytesIn)).chars);
        fact("Out", rcFormat(&st->frame, "%u message%s, %s", lv->msgsOut,
                             lv->msgsOut == 1 ? "" : "s", size_words(st, lv->bytesOut)).chars);
        if (lv->burstSent || lv->burstRefused)
            fact("Last burst", rcFormat(&st->frame, "%d of %d sent, %d stopped at the send cap",
                                        lv->burstSent, LIVE_BURST_COUNT, lv->burstRefused).chars);
        if (lv->sunk)
            fact("Server took", rcFormat(&st->frame, "%s of bulk data",
                                         size_words(st, lv->sunk)).chars);
        fact("Caps", rcFormat(&st->frame, "Message %s, unread %s, send buffer %s",
                              size_words(st, LIVE_CAP_MESSAGE), size_words(st, LIVE_CAP_UNREAD),
                              size_words(st, LIVE_CAP_BUFFERED)).chars);
    }
    rcColumn(.bg = pal->list, .gap = 8, .p = 16, .w = "grow") {
        info_card_heading("Events (seconds since launch)");
        if (!lv->eventCount)
            rcTextL("Nothing yet.", .color = s.textMuted, .size = 12);
        for (int k = 0; k < lv->eventCount; k++) {
            const int at = (lv->eventHead - 1 - k + LIVE_EVENT_MAX) % LIVE_EVENT_MAX;
            const LiveEvent *e = &lv->events[at];
            const RC_Color dot = e->tone == LIVE_TONE_OK ? pal->online
                               : (e->tone == LIVE_TONE_WARN ? pal->away : s.textMuted);

            rcRow(.gap = 8, .align = "tl", .w = "grow") {
                rcBox(.pt = 5) {
                    rcBox(.bg = dot, .borderRadius = "all-full", .w = "6px", .h = "6px") {}
                }
                rcBox(.w = "44px") {
                    rcText(rcFormat(&st->frame, "%.1f", e->at), .color = s.textMuted,
                           .size = 12);
                }
                rcBox(.overflow = "hidden", .w = "grow") {
                    rcTextC(e->text, .color = s.text, .size = 12);
                }
            }
        }
    }
}

/* Who is in the room. Live, the server says; in the demo, the group's own list. */
static void member_list(AppState *st, const MsgConversation *c, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    const LiveRoom *lv = &st->live;

    rcColumn(.bg = pal->list, .gap = 10, .p = 16, .w = "grow") {
        if (c->kind == MSG_CONV_CHANNEL && lv->link == LIVE_OPEN && lv->room == st->openConv) {
            info_card_heading(rcFormat(&st->frame, "%d here", lv->people).chars);
            for (int k = 0; k < lv->people; k++) {
                const char *name = lv->who[k].name;
                const uint32_t tint = name_hash(name) % 8u;

                rcRow(.gap = 10, .align = "cl", .w = "grow") {
                    avatar_disc(SENDER_TINT[tint][0], SENDER_TINT[tint][1], first_char(st, name),
                                28.0f);
                    rcTextC(name, .color = s.text);
                }
            }
        } else if (c->kind == MSG_CONV_CHANNEL && lv->link != LIVE_DEMO) {
            info_card_heading("Members");
            rcTextL("Not connected", .color = s.textMuted);
        } else {
            info_card_heading("Members");
            rcTextC(c->detail, .color = s.text);
        }
    }
}

/* The photos this chat has shared, as a grid: each opens the viewer. A chat that has shared
   none has no card: an empty heading is filler, and without it the controls below move up
   into view on a short window. */
static void media_grid(AppState *st, const MsgConversation *c, const MsgPalette *pal) {
    const float side = (MSG_DRAWER_PANE_W - 32.0f - 12.0f) / 3.0f;
    int found = 0;

    for (int i = 0; i < c->count; i++)
        found += c->items[i].kind == MSG_KIND_PHOTO && !c->items[i].deleted;
    if (!found)
        return;
    rcColumn(.bg = pal->list, .gap = 10, .p = 16, .w = "grow") {
        info_card_heading("Photos");
        rcRow(.gap = 6, .w = "grow", .className = "flex-wrap") {
            for (int i = c->count - 1; i >= 0; i--) {
                const MsgMessage *m = &c->items[i];

                if (m->kind == MSG_KIND_PHOTO && !m->deleted) {
                    const char *id = fmt_id(st, "mg", m->id);

                    rcBox(.id = id, .tooltip = "Open") {
                        photo_art(m->art, side, side);
                    }
                    if (rcClicked(id)) {
                        st->modalPhoto = true;
                        st->photoShown = m->id;
                        st->photoConv = st->openConv;
                    }
                }
            }
        }
    }
}

/* The details. Two shapes, one state: st->infoOpen says the user asked for it and the width
   decides how it shows, a drawer beside the chat where it fits, a sheet in the chat's place
   where it does not. */
static void msg_info_pane(AppState *st, bool sheet, bool header, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    MsgConversation *c = &st->store.conv[st->openConv];
    const bool channel = c->kind == MSG_CONV_CHANNEL;
    const bool open = st->live.link == LIVE_OPEN;
    const bool isMuted = c->muted;

    rcColumn(.bg = pal->drawer, .h = "grow", .wType = sheet ? RC_GROW : RC_PX(MSG_DRAWER_PANE_W)) {
        if (header) {
            rcRow(.bg = pal->header, .gap = 12, .pl = 10, .align = "cl", .w = "grow",
                  .hType = RC_PX(MSG_HEAD_H)) {
                if (icon_button("info_close", rcIconX, pal->icon, pal->iconWash, "Close (Esc)"))
                    st->infoOpen = false;
                rcTextC(c->kind == MSG_CONV_DIRECT ? "Contact info"
                        : (c->kind == MSG_CONV_GROUP ? "Group info" : "Channel details"),
                        .font = rcFontSemibold(15), .color = s.text, .select = RC_SELECT_NONE);
            }
        }
        rcColumn(.id = "InfoScroll", .gap = 10, .scroll = "v", .w = "grow", .h = "grow") {
            rcColumn(.bg = pal->list, .gap = 6, .pt = 28, .pb = 22, .px = 16, .align = "tc",
                     .w = "grow") {
                avatar(c, 112.0f, pal);
                rcBox(.h = "8px") {}
                rcTextC(channel ? rcFormat(&st->frame, "#%s", c->name).chars : c->name,
                        .font = rcFontSemibold(22), .color = s.text);
                if (c->kind == MSG_CONV_DIRECT)
                    rcTextC(c->detail, .color = s.textMuted, .size = 15);
                rcTextC(c->typing[0] ? c->typing : c->about, .color = s.textMuted,
                        .textAlign = "c");
            }
            if (c->kind != MSG_CONV_DIRECT)
                member_list(st, c, pal);
            media_grid(st, c, pal);
            if (channel)
                live_panel(st, open, pal);
            rcColumn(.bg = pal->list, .p = 6, .w = "grow") {
                rcRow(.gap = 12, .px = 10, .align = "cl", .w = "grow", .h = "44px") {
                    if (isMuted)
                        rcIconVolumeX(20.0f, pal->icon);
                    else
                        rcIconVolume2(20.0f, pal->icon);
                    rcBox(.w = "grow") { rcTextL("Mute notifications", .color = s.text); }
                    rcToggle("tg_mute", "", &c->muted);
                }
                rcRow(.gap = 12, .px = 10, .align = "cl", .w = "grow", .h = "44px") {
                    rcIconCheckCheckSolid(20.0f, pal->icon);
                    rcBox(.w = "grow") { rcTextL("Read receipts", .color = s.text); }
                    rcToggle("tg_receipts_info", "", &st->store.readReceipts);
                }
                if (!channel) {
                    const char *id = "info_delete";

                    rcRow(.id = id, .gap = 12, .px = 10, .align = "cl", .borderRadius = "all-md",
                          .w = "grow", .h = "44px", .className = pal->row) {
                        rcIconTrash2(20.0f, pal->error);
                        rcTextL("Delete chat", .color = pal->error, .select = RC_SELECT_NONE);
                    }
                    if (rcClicked(id)) {
                        st->asideConv = st->openConv;
                        st->asideArchive = false;
                    }
                }
            }
        }
    }
}

/* ---- The modals ------------------------------------------------------------------- */

static float modal_body_w(float cap, float avail) {
    if (avail < 0.0f)
        avail = 0.0f;               /* a safe width under the chrome: never a negative RC_PX */
    return avail < cap ? avail : cap;
}

/* A row of a settings section: what it is on the left, the control the caller draws after. */
static void setting_label(const char *name, const char *hint) {
    RC_Style s = rcGetStyle();

    rcColumn(.gap = 2, .w = "grow") {
        rcTextC(name, .color = s.text);
        if (hint)
            rcTextC(hint, .color = s.textMuted, .size = 12);
    }
}

static void settings_modal(AppState *st, float bodyAvail, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    static const char *const statuses[] = { "Online", "Away", "Invisible" };
    const bool wasOpen = st->modalSettings;

    if (rcBeginModal("modal_settings", &st->modalSettings)) {
        rcColumn(.gap = 16, .wType = RC_PX(modal_body_w(440.0f, bodyAvail))) {
            rcTextL("Settings", .font = rcFontSemibold(22), .color = s.text);
            rcRow(.gap = 12, .align = "cl", .w = "grow") {
                setting_label("Your name", "How groups and live rooms see you");
                rcBox(.w = "180px") {
                    rcTextInput("my_name", st->myName, sizeof st->myName, .clearButton = true);
                }
            }
            rcRow(.gap = 12, .align = "cl", .w = "grow") {
                setting_label("Status", NULL);
                rcBox(.w = "180px") { rcCombo("cb_status", &st->statusCombo, statuses, 3); }
            }
            rcBox(.bg = pal->divider, .w = "grow", .h = "1px") {}
            rcRow(.gap = 12, .align = "cl", .w = "grow") {
                setting_label("Dark theme", NULL);
                rcToggle("tg_theme", "", &st->darkMode);
            }
            rcRow(.gap = 12, .align = "cl", .w = "grow") {
                setting_label("Wallpaper", "The tint behind every chat");
                for (int k = 0; k < 4; k++) {
                    const char *id = WALL_IDS[k];
                    const bool on = st->wallpaper == k;

                    rcBox(.id = id, .bg = rcHex(WALL_TINT[k][st->darkMode]), .align = "cc",
                          .borderRadius = "all-full",
                          .border = { .color = on ? pal->accent : pal->panelEdge,
                                      .width = 2 },
                          .w = "32px", .h = "32px") {
                        if (on)
                            rcIconCheck(16.0f, pal->accent);
                    }
                    if (rcClicked(id))
                        st->wallpaper = k;
                }
            }
            rcBox(.bg = pal->divider, .w = "grow", .h = "1px") {}
            rcRow(.gap = 12, .align = "cl", .w = "grow") {
                setting_label("Enter sends", "Otherwise only the send button does");
                rcToggle("tg_enter", "", &st->enterSends);
            }
            rcRow(.gap = 12, .align = "cl", .w = "grow") {
                setting_label("Read receipts", "Colored ticks once a message has been read");
                rcToggle("tg_receipts", "", &st->store.readReceipts);
            }
            rcRow(.gap = 12, .align = "cl", .w = "grow") {
                setting_label("Message banners", "A banner when a line lands in another chat");
                rcToggle("tg_notify", "", &st->notifications);
            }
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                rcSpacer() {}
                if (rcButton("btn_settings_done", "Done", .className = by_pointer("h-9", "h-11"),
                             .variant = RC_BTN_PRIMARY))
                    st->modalSettings = false;
            }
        }
        rcEndModal();
    }
    /* The name field writes as you type, so the room learns the new name when the dialog
       closes, whether by Done, Esc or a click on the scrim. */
    if (wasOpen && !st->modalSettings && st->live.link == LIVE_OPEN)
        st->live.ask = LIVE_ASK_RENAME;
}

static void attach_modal(AppState *st, float bodyAvail, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();

    if (rcBeginModal("modal_attach", &st->modalAttach)) {
        const float bodyW = modal_body_w(420.0f, bodyAvail);
        const float side = (bodyW - 18.0f) / 4.0f;

        rcColumn(.gap = 14, .wType = RC_PX(bodyW)) {
            rcText(rcFormat(&st->frame, "Send a photo to %s",
                            st->store.conv[st->openConv].name),
                   .font = rcFontSemibold(18), .color = s.text);
            for (int r = 0; r < 2; r++) {
                rcRow(.gap = 6, .w = "grow") {
                    for (int col = 0; col < 4; col++) {
                        const int k = r * 4 + col;
                        const char *id = rcFormat(&st->frame, "att%d", k).chars;
                        const bool on = k == st->attachPick;

                        rcBox(.id = id, .border = { .color = on ? pal->accent : RC_UNSET_COLOR,
                                                    .width = 3 }) {
                            photo_art(k, side, side);
                        }
                        if (rcClicked(id))
                            st->attachPick = k;
                    }
                }
            }
            rcTextInput("attach_caption", st->attachCaption, sizeof st->attachCaption,
                        .placeholder = "Add a caption", .clearButton = true);
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                rcSpacer() {}
                if (rcButton("btn_attach_cancel", "Cancel", .className = by_pointer("h-9", "h-11")))
                    st->modalAttach = false;
                if (rcButton("btn_attach_send", "Send", .className = by_pointer("h-9", "h-11"),
                             .variant = RC_BTN_PRIMARY)) {
                    send_art(st, MSG_KIND_PHOTO, st->attachPick, st->attachCaption);
                    st->attachCaption[0] = '\0';
                    st->modalAttach = false;
                }
            }
        }
        rcEndModal();
    }
}

static void new_chat_modal(AppState *st, float bodyAvail, float viewH, bool onePane,
                           const MsgPalette *pal) {
    RC_Style s = rcGetStyle();

    if (rcBeginModal("modal_new", &st->modalNewChat)) {
        int pick = -1;

        rcColumn(.gap = 12, .wType = RC_PX(modal_body_w(380.0f, bodyAvail))) {
            rcTextL("New chat", .font = rcFontSemibold(18), .color = s.text);
            rcTextInput("new_search", st->newChatQuery, sizeof st->newChatQuery,
                        .placeholder = "Search contacts", .leadingIcon = rcIconSearch,
                        .clearButton = true);
            /* The panel's own title, field and padding take about 140 px of a short window. */
            rcColumn(.id = "NewScroll", .scroll = "v", .w = "grow",
                     .hMax = RC_VAL(viewH - 180.0f < 360.0f ? viewH - 180.0f : 360.0f)) {
                for (int p = MSG_CONTACT_FIRST; p <= MSG_CONTACT_LAST; p++) {
                    if (msg_text_matches(msg_person(p)->name, st->newChatQuery)
                        && contact_row(st, "contact", p, pal))
                        pick = p;
                }
            }
            rcScrollbar("NewScroll");
            if (pick >= 0)
                start_chat(st, pick, onePane);
        }
        rcEndModal();
    }
}

static void photo_modal(AppState *st, float bodyAvail, float viewH) {
    RC_Style s = rcGetStyle();

    if (rcBeginModal("modal_photo", &st->modalPhoto)) {
        const MsgMessage *m = msg_find(&st->store, st->photoConv, st->photoShown);
        float w = modal_body_w(720.0f, bodyAvail);

        if (w * 0.75f > viewH - 160.0f)
            w = (viewH - 160.0f) / 0.75f;
        if (w < 64.0f)
            w = 64.0f;
        rcColumn(.gap = 12, .wType = RC_PX(w)) {
            if (m) {
                photo_art(m->art, w, w * 0.75f);
                if (m->textLen)
                    rcTextC(m->text, .color = s.text);
            }
            rcRow(.gap = 8, .align = "cl", .w = "grow") {
                if (m)
                    rcTextC(m->day ? msg_day_word(&st->store, m->day) : m->ts, .color = s.textMuted,
                            .size = 13);
                rcSpacer() {}
                if (rcButton("btn_photo_close", "Close", .className = by_pointer("h-9", "h-11")))
                    st->modalPhoto = false;
            }
        }
        rcEndModal();
    }
}

/* A voice call, simulated like the people in it: it rings, connects and counts, and however it
   ends, by the red button, Esc or Back, it leaves its length, or that it was missed, in the
   thread. */
static void call_modal(AppState *st, float bodyAvail, const MsgPalette *pal) {
    RC_Style s = rcGetStyle();
    const MsgConversation *c = &st->store.conv[st->openConv];
    const float talking = (float)(st->callTime - 3.0);
    const bool calling = st->modalCall;

    if (rcBeginModal("modal_call", &st->modalCall, .noBackdropDismiss = true)) {
        rcColumn(.gap = 8, .py = 16, .align = "tc",
                 .wType = RC_PX(modal_body_w(300.0f, bodyAvail))) {
            avatar(c, 112.0f, pal);
            rcBox(.h = "8px") {}
            rcTextC(c->name, .font = rcFontSemibold(22), .color = s.text);
            rcTextC(talking < 0.0f ? "Calling..."
                    : rcFormat(&st->frame, "%02d:%02d", (int)talking / 60, (int)talking % 60).chars,
                    .color = s.textMuted, .size = 15);
            rcRow(.gap = 24, .pt = 20, .align = "cc", .w = "grow") {
                rcBox(.id = "call_mute", .focusable = true,
                      .bg = st->callMuted ? pal->chipOn : pal->rowActive, .align = "cc",
                      .borderRadius = "all-full", .w = "56px", .h = "56px",
                      .tooltip = st->callMuted ? "Unmute" : "Mute") {
                    if (st->callMuted)
                        rcIconMicOff(24.0f, pal->chipOnInk);
                    else
                        rcIconMic(24.0f, pal->icon);
                }
                if (rcClicked("call_mute"))
                    st->callMuted = !st->callMuted;
                rcBox(.id = "call_end", .focusable = true, .bg = pal->error, .align = "cc",
                      .borderRadius = "all-full", .w = "56px", .h = "56px", .tooltip = "Hang up") {
                    rcIconPhoneOff(26.0f, RC_WHITE);
                }
                if (rcClicked("call_end"))
                    st->modalCall = false;
            }
        }
        rcEndModal();
    }
    /* The red button, Esc and Back each clear the flag by the time rcEndModal returns, so the
       note is written once, here. */
    if (calling && !st->modalCall) {
        const char *note = talking < 0.0f ? "Missed voice call"
                         : rcFormat(&st->frame, "Voice call, %02d:%02d",
                                    (int)talking / 60, (int)talking % 60).chars;

        send_art(st, MSG_KIND_EVENT, 0, note);
    }
}

/* The banner: a line landed in another chat, or the answer to the last action. It opens its
   chat on a click. It stands in the top right of the pane on screen (@p pane), under its
   headers, as a desktop's notifications do: the newest lines and the composer, what the user
   is acting on, are at the bottom. A dialog has the user's attention, so one closes it, and
   none comes up behind one. Its z sits under the popups the user opens (20): the quick
   reactions of a line at the top of the thread rise into this corner. */
static void msg_toast(AppState *st, const char *pane, float maxW, bool onePane,
                      const MsgPalette *pal) {
    RC_Style s = rcGetStyle();

    if (st->modalSettings || st->modalAttach || st->modalNewChat || st->modalPhoto
        || st->modalCall)
        st->toastLeft = 0.0f;
    if (st->toastLeft <= 0.0f || !st->toast[0])
        return;
    rcRow(.id = "toast", .bg = pal->panel, .gap = 12, .px = 16, .py = 12, .align = "cl",
          .borderRadius = "all-xl", .border = { .color = pal->panelEdge, .width = 1 },
          .wMax = RC_VAL(maxW < 0.0f ? 0.0f : (maxW < 360.0f ? maxW : 360.0f)),
          .shadow = { .color = rcWithAlpha(RC_BLACK, 0.22f), .y = 6.0f, .blur = 20.0f },
          .floating = { .to = RC_ATTACH_ELEMENT, .toId = pane, .parent = RC_ANCHOR_TOP_RIGHT,
                        .element = RC_ANCHOR_TOP_RIGHT, .offset = { -16.0f, 12.0f },
                        .zIndex = 10 }) {
        if (st->toastConv >= 0)
            avatar(&st->store.conv[st->toastConv], 36.0f, pal);
        rcColumn(.gap = 1, .overflow = "hidden", .w = "grow") {
            if (st->toastTitle[0])
                rcTextC(st->toastTitle, .font = rcFontSemibold(13), .color = s.text, .wrap = "e",
                        .select = RC_SELECT_NONE);
            rcTextC(st->toast, .className = st->toastTitle[0] ? "truncate" : "",
                    .color = st->toastTitle[0] ? s.textMuted : s.text, .size = 13,
                    .select = RC_SELECT_NONE);
        }
        if (st->undoConv >= 0
            && rcButton("toast_undo", "Undo", .className = by_pointer("h-7", "h-11"),
                         .variant = RC_BTN_GHOST))
            undo_set_aside(st, onePane);
    }
    if (st->toastConv >= 0 && rcClicked("toast")) {
        open_chat(st, st->toastConv, onePane);
        st->toastLeft = 0.0f;
    }
}

/* ---- The frame -------------------------------------------------------------------- */

/* A press anywhere but on them closes the two popups the app draws itself; the press on the
   button that opened one is left to that button, so it toggles. */
static void close_popups(AppState *st) {
    if (!rcPointerPressed(RC_POINTER_LEFT))
        return;
    if (st->stickerOpen && !rcIsHovered("sticker_panel") && !rcIsHovered("btn_emoji"))
        st->stickerOpen = false;
    if (st->reactFor && !rcIsHovered("react_bar") && !rcIsHovered("t_react"))
        st->reactFor = 0;
}

/* The shortcuts the leaders have, and Esc: it closes what is on top, one thing a press. A
   focused field takes the first press to blur (rcKeyPressed), and the next one reaches here. */
static void handle_keys(AppState *st, bool onePane, bool threadShown) {
    const bool primary = rcModDown(RC_MOD_PRIMARY);

    if (primary && rcKeyPressed(RC_KEY_K)) {
        set_nav(st, NAV_CHATS);
        st->sidebarCollapsed = false;
        rcSetFocus("search");
    }
    if (primary && rcKeyPressed(RC_KEY_F) && threadShown) {
        st->threadSearchOn = true;
        rcSetFocus("thread_search");
    }
    if (primary && rcKeyPressed(RC_KEY_N))
        open_new_chat(st);
    if (primary && rcKeyPressed(RC_KEY_COMMA))
        st->modalSettings = true;
    if (rcModDown(RC_MOD_ALT) && (rcKeyPressed(RC_KEY_UP) || rcKeyPressed(RC_KEY_DOWN))) {
        const int step = rcKeyPressed(RC_KEY_UP) ? -1 : 1;

        for (int o = 0; o < st->store.orderCount; o++) {
            if (st->store.order[o] == st->openConv) {
                int to = o + step;

                while (to >= 0 && to < st->store.orderCount
                       && !row_listed(st, &st->store.conv[st->store.order[to]]))
                    to += step;
                if (to >= 0 && to < st->store.orderCount)
                    open_chat(st, st->store.order[to], onePane);
                break;
            }
        }
    }
    /* Read only where it closes something: with nothing open Android's Back stays the
       system's and leaves the app. */
    if ((st->reactFor || st->stickerOpen || st->threadSearchOn || st->replyTo || st->infoOpen
         || (onePane && st->sidebarCollapsed))
        && rcKeyPressed(RC_KEY_ESCAPE)) {
        if (st->reactFor)
            st->reactFor = 0;
        else if (st->stickerOpen)
            st->stickerOpen = false;
        else if (st->threadSearchOn) {
            st->threadSearchOn = false;
            st->threadSearch[0] = '\0';
        } else if (st->replyTo) {
            st->replyTo = 0;
            rcSetFocus(composer_id(st));
        } else if (st->infoOpen) {
            st->infoOpen = false;
        } else {
            st->sidebarCollapsed = false;
            st->store.openConv = -1;
        }
    }
}

void messenger_seed(AppState *st, unsigned seed) {
    (void)seed;                      /* the corpus is written out: one seed, one story */
    msg_memzero(st, sizeof *st);
    msg_store_seed(&st->store);
    st->frame.buffer       = st->frameBytes;
    st->frame.bufferLength = sizeof st->frameBytes;
    st->frame.currOffset   = 0;
    st->openConv        = C_MAYA;
    st->store.openConv  = C_MAYA;
    st->store.readReceipts = true;
    st->darkMode        = false;
    st->enterSends      = true;
    st->notifications   = true;
    st->toastConv       = -1;
    st->undoConv        = -1;
    st->unreadMarkConv  = -1;
    st->exportAsk       = -1;
    st->asideConv       = -1;
    rcStrCopy(st->myName, "Sam Rivera", sizeof st->myName);
    /* A look that repeats is named once, at start-up, as Tailwind's @apply names one: every
       "more" menu's 22 px glyph in a circle. The palette's menu classes add their height, ink
       and wash to it, and stay short enough for the class cache to resolve each one once. */
    rcDefineClass("round-icon", "icon-[22px] rounded-full");
    st->seeded = true;
}

void messenger_update(AppState *st, const AppCtx *ctx) {
    const float dt = ctx->dt;

    msg_store_step(&st->store, dt);    /* dt <= 0 (freeze) => a no-op */
    if (dt > 0.0f) {
        /* The dots' cycle is 1 / 0.85 s, so 20 s is 17 whole cycles: the clock turns over
           there instead of counting up for ever, where a float's step grows to a frame's size
           (1/64 s at 36 hours, 1/8 s at twelve days) and the dots stutter, then stop. */
        st->time += dt;
        if (st->time >= 20.0f)          /* a parked window's dt can be hours */
            st->time -= 20.0f * (float)(int)(st->time / 20.0f);
        st->typingPhase = st->time * 0.85f;
        if (st->modalCall)
            st->callTime += dt;
        if (st->flashLeft > 0.0f)
            st->flashLeft -= dt;
        if (st->toastLeft > 0.0f)
            st->toastLeft -= dt;
    }
}

float messenger_wake_in(const AppState *st) {
    const MsgConversation *open = &st->store.conv[st->openConv];
    float next = msg_next_change(&st->store);

    if (st->asideConv >= 0 || st->sendKind)
        return 0.0f;
    /* Dots dance while someone types on screen; the rest of the motion is short. */
    if (open->typing[0] || st->flashLeft > 0.0f)
        return 1.0f / 30.0f;
    if (st->modalCall)
        return 0.25f;
    if (st->toastLeft > 0.0f && (next < 0.0f || st->toastLeft < next))
        next = st->toastLeft;
    return next;
}

void messenger_notice(AppState *st, const char *words) {
    show_toast(st, NULL, words, -1);
}

float messenger_bar_height(const AppCtx *ctx) {
    const RC_Insets safe = app_safe(ctx);

    return app_view_w(ctx) - safe.left - safe.right < MSG_ONE_PANE_W ? MSG_BAR_NARROW
        : MSG_BAR_WIDE;
}

void messenger_layout(AppState *st, const AppCtx *ctx) {
    rcArenaReset(&st->frame);
    rcSetStyle(messenger_style(st->darkMode));

    /* Branch on the space: a window dragged narrow is a phone's problem too. */
    const RC_Insets safe    = app_safe(ctx);
    const float rootW       = app_view_w(ctx) - safe.left - safe.right;
    const float viewH       = app_view_h(ctx);
    const bool onePane      = rootW < MSG_ONE_PANE_W;

    if (st->asideConv >= 0) {
        set_chat_aside(st, st->asideConv, st->asideArchive, onePane);
        st->asideConv = -1;
        msg_order_rebuild(&st->store);
    }
    if (st->sendKind)
        send_pending(st);
    const bool drawerFits   = rootW >= MSG_DRAWER_W;
    const float barH        = onePane ? MSG_BAR_NARROW : MSG_BAR_WIDE;
    const MsgPalette pal    = messenger_palette(st->darkMode, st->wallpaper);
    const bool listShown    = !onePane || !st->sidebarCollapsed;
    const bool chatShown    = !onePane || st->sidebarCollapsed;
    const bool infoSheet    = st->infoOpen && !drawerFits;
    const bool drawer       = st->infoOpen && drawerFits;
    const bool shortView    = viewH < MSG_SHORT_H;
    /* The page ends where a soft keyboard begins, so the composer stays above it. */
    const float bottom      = safe.bottom > ctx->view.keyboard.bottom ? safe.bottom
                                                                      : ctx->view.keyboard.bottom;
    const float modalAvail  = rootW - MSG_MODAL_CHROME - 2.0f * MSG_MODAL_MARGIN;

    /* The widest a bubble may draw, from what the chat actually gets. */
    float paneW = rootW;
    if (!onePane) {
        paneW -= MSG_RAIL_W + MSG_LIST_W + 1.0f;
        if (drawer)
            paneW -= MSG_DRAWER_PANE_W;
    }
    st->threadTight = paneW < MSG_THREAD_NARROW_W;
    const float padX = st->threadTight ? MSG_THREAD_PX_NARROW : MSG_THREAD_PX_WIDE;
    const float frac = st->threadTight ? MSG_BUBBLE_FRAC_NARROW : MSG_BUBBLE_FRAC_WIDE;
    const float threadW = paneW - 2.0f * padX - MSG_TAIL_W;
    const float bubbleMax = threadW > 0.0f ? threadW * frac : 0.0f;

    msg_order_rebuild(&st->store);
    close_popups(st);
    handle_keys(st, onePane, chatShown && !infoSheet);

    /* A line came into another chat: the banner says so, unless that chat is muted or set
       aside. A channel only raises its badge, as in a team chat, and a banner still offering
       Undo stays up. A phone's chat list says so itself: the row moves up and takes a badge. */
    if (st->store.arrivedConv >= 0) {
        const MsgConversation *c = &st->store.conv[st->store.arrivedConv];
        const MsgMessage *m = msg_find(&st->store, st->store.arrivedConv, st->store.arrivedId);
        const bool holdsUndo = st->toastLeft > 0.0f && st->undoConv >= 0;

        if (m && st->notifications && !c->muted && !c->archived && !c->hidden
            && c->kind != MSG_CONV_CHANNEL && !holdsUndo && !(onePane && !st->sidebarCollapsed))
            show_toast(st, c->name, msg_snippet(m), st->store.arrivedConv);
        st->store.arrivedConv = -1;
    }
    /* The open chat is on screen, so whatever arrives there counts as read: the store takes
       it from here. The one-pane list screen has no open chat. */
    if (onePane && !st->sidebarCollapsed)
        st->store.openConv = -1;
    else
        st->store.openConv = st->openConv;

    rcColumn(.id = "Root", .pt = safe.top, .pb = (float)bottom,
             .pl = safe.left, .pr = safe.right, .w = "grow", .h = "grow") {
        rcUnzoomed() {
            msg_bar(st, onePane, listShown, infoSheet && chatShown, barH, rootW * ctx->zoom, &pal);
        }
        rcRow(.id = "Body", .w = "grow", .h = "grow") {
            if (!onePane)
                msg_rail(st, &pal);
            if (listShown)
                msg_list(st, onePane, shortView, &pal);
            if (listShown && chatShown)
                rcBox(.bg = pal.divider, .w = "1px", .h = "grow") {}
            if (chatShown) {
                if (infoSheet)
                    msg_info_pane(st, true, !onePane, &pal);
                else
                    msg_thread(st, onePane, paneW, viewH, bubbleMax, &pal);
            }
            if (drawer)
                msg_info_pane(st, false, true, &pal);
        }
        if (onePane && listShown)
            msg_tabs(st, &pal);
    }
    settings_modal(st, modalAvail, &pal);
    attach_modal(st, modalAvail, &pal);
    new_chat_modal(st, modalAvail, viewH, onePane, &pal);
    photo_modal(st, modalAvail, viewH);
    call_modal(st, modalAvail, &pal);
    const char *toastPane = !chatShown ? "SideScroll"
                          : (infoSheet ? "InfoScroll" : thread_id(st, st->threadDrawn));

    msg_toast(st, toastPane, paneW - 32.0f, onePane, &pal);

    /* Each bar carries the condition its pane carries: rcScrollbar draws a bar for a container
       this frame built, and warns for one it did not. */
    if (listShown)
        rcScrollbar("SideScroll");
    if (chatShown && !infoSheet)
        rcScrollbar(thread_id(st, st->threadDrawn));
    if ((chatShown && infoSheet) || drawer)
        rcScrollbar("InfoScroll");
}

void messenger_bench_step(AppState *st, const AppInputSink *in, int frame) {
    /* The scripted scenario. Every action goes through the input sink, so the real hit-test,
       focus and caret paths run; only the incoming message is a direct backend call. At
       MSG_BENCH_WARMUP the app holds, a strict no-op. The caret blink and tooltip dwell run on
       real time, not the injected dt, so the held frame must carry no focused input and no
       hovered tooltip: hence the blur and off-canvas park below. */
    if (!in || frame >= MSG_BENCH_WARMUP)
        return;

    if (frame == 0) {
        in->move(in->ctx, 800.0f, 300.0f);             /* over the thread: the wheel's target */
    } else if (frame < 12) {
        in->wheel(in->ctx, 0.0f, 3.0f);                /* scroll up the backlog */
    } else if (frame == 18) {
        in->move(in->ctx, 800.0f, 690.0f);             /* focus the composer: press ... */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == 19) {
        in->button(in->ctx, APP_MBTN_LEFT, false);     /* ... release (a real click edge) */
    } else if (frame >= 22 && frame < 46) {
        static const char TYPED[] = "see you there on friday!";  /* 24 ASCII chars, one a frame */
        in->text(in->ctx, (unsigned int)(unsigned char)TYPED[frame - 22]);
    } else if (frame == 47) {
        msg_inject_incoming(&st->store, C_CREW);       /* a line lands in another chat */
    } else if (frame == MSG_BENCH_WARMUP - 2) {
        in->move(in->ctx, -100.0f, -100.0f);           /* blur the composer and park: press ... */
        in->button(in->ctx, APP_MBTN_LEFT, true);
    } else if (frame == MSG_BENCH_WARMUP - 1) {
        in->button(in->ctx, APP_MBTN_LEFT, false);     /* ... release; the pointer stays away */
    }
}
