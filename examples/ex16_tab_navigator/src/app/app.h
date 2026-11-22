/*
    app.h - the app around the navigator: which screen a route shows, the two shells, the frame

    A bottom tab bar under one stack of screens. Past WIDE_W the same screens lay out as a rail, a
    list and a pane: the same ids, texts and state, so that arm is a presentation and never a
    second app.
*/
#ifndef APP_APP_H
#define APP_APP_H

#include "rayclay.h"
#include "app/screens.h"

/* The one callback the navigator asks for: draw this route. */
static void draw_screen(RC_App *app, void *user, const NavScreen *sc) {
    AppState *st = (AppState *)user;

    if (sc->pane) {
        discover_pane(app, st, sc);
        return;
    }
    /* No `default:` on purpose: with -Wswitch the compiler names every switch a new route was
       forgotten in. */
    switch ((RouteKind)sc->route->kind) {
    case ROUTE_ROOT:
        switch ((Tab)sc->tab) {
        case TAB_HOME:    home_root(app, st, sc); break;
        case TAB_SEARCH:  explore_root(app, st, sc); break;
        case TAB_ALERTS:  activity_root(app, st, sc); break;
        case TAB_PROFILE: profile_root(app, st, sc); break;
        case TAB_COUNT:   break;
        }
        break;
    case ROUTE_POST:         post_screen(app, st, sc, sc->route->item); break;
    case ROUTE_PERSON:       person_screen(app, st, sc, (PersonId)sc->route->item); break;
    case ROUTE_TOPIC:        topic_screen(app, st, sc, (TopicId)sc->route->item); break;
    case ROUTE_SETTINGS:     settings_screen(app, st, sc); break;
    case ROUTE_EDIT_PROFILE: edit_profile_screen(app, st, sc); break;
    }
}

/* The four tabs, as this frame sees them: a count on Activity, a dot on Home while a refresh would
   find something and you are looking elsewhere. The lit Home tab, at its top, checks for posts. */
static void tab_item(RC_App *app, AppState *st, Tab t, bool inRail) {
    static const RC_IconCallback ICON_OFF[TAB_COUNT] = {
        rcIconHouse, rcIconCompass, rcIconBell, rcIconUser,
    };
    static const RC_IconCallback ICON_ON[TAB_COUNT] = {
        rcIconHouseFilled, rcIconCompassFilled, rcIconBellSolid, rcIconUserFilled,
    };
    const NavTab tab = {
        TAB_ID[t], ICON_OFF[t], ICON_ON[t], t == TAB_ALERTS ? unread_count(st) : 0,
        t == TAB_HOME && st->nav.tab != TAB_HOME && st->incoming < N(INCOMING),
    };

    if (nav_tab_item(app, &st->nav, &tab, (int)t, inRail) && t == TAB_HOME)
        start_refresh(st);
}

/* The one place the brand's gradient fills: the action a social app most wants you to take. The
   whole cell is the target and the Tab stop, rounded as a tab's cell, so its focus ring is too. */
static void compose_button(AppState *st, bool inRail) {
    rcBox(.id = "tab_compose", .focusable = true, .align = "cc", .borderRadius = "all-12",
          .w = "grow", .hType = inRail ? RC_PX(52) : RC_GROW) {
        rcBox(.align = "cc", .borderRadius = "all-14", .wType = RC_PX(52), .hType = RC_PX(38),
              .gradient = { .from = PALETTE.brandFrom, .to = PALETTE.brandTo, .dir = "d" }) {
            rcIconPlus(24.0f, RC_WHITE);
        }
    }
    if (rcClicked("tab_compose"))
        open_sheet(st, SHEET_COMPOSE);
}

/* THE PHONE SHELL: one stage over one bar, and each spends the safe area it sits on. The bar steps
   aside while a keyboard is up, as a phone's does, and the reply field takes its room. The bar's
   4 px around its cells, which stay centered where they were, are the room a tab's focus ring
   needs: it draws 4 px outside the cell, and the window's bottom and side edges would cut it. */
static void phone_shell(RC_App *app, AppState *st, RC_Viewport vp) {
    RC_Style s = rcGetStyle();

    nav_stage(app, &st->nav, draw_screen, st, vp.safe.top);
    if (vp.keyboard.bottom <= 0.0f) {
        hairline();
        /* The height rounds the inset as the padding does, so the tabs keep BAR_H less 8. */
        rcRow(.id = "tabbar", .bg = s.background, .pt = 4, .pb = (float)(vp.safe.bottom + 4.0f),
              .px = 4, .align = "cc", .w = "grow",
              .hType = RC_PX(BAR_H + (int)(vp.safe.bottom + 0.5f))) {
            tab_item(app, st, TAB_HOME, false);
            tab_item(app, st, TAB_SEARCH, false);
            compose_button(st, false);
            tab_item(app, st, TAB_ALERTS, false);
            tab_item(app, st, TAB_PROFILE, false);
        }
    }
}

/* THE WIDE ARM: the bar's items as a rail, the tab's root as a list, its stack in the pane beside
   it. With nothing open the pane is discover_pane, so the window is full at first sight without
   showing the feed's first post twice. */
static void wide_shell(RC_App *app, AppState *st, RC_Viewport vp) {
    const int listW = vp.width >= (float)ROOMY_W ? LIST_W_ROOMY : LIST_W;

    rcRow(.w = "grow", .h = "grow") {
        rcColumn(.id = "rail", .gap = 2, .pt = 12, .px = 8, .align = "tc", .h = "grow",
                 .wType = RC_PX(RAIL_W), .className = "bg-background border-r border-border") {
            rcBox(.align = "cc", .borderRadius = "all-10", .wType = RC_PX(36), .hType = RC_PX(36),
                  .gradient = { .from = PALETTE.brandFrom, .to = PALETTE.brandTo, .dir = "d" }) {
                rcTextL("P", .font = rcFontSemibold(22), .color = RC_WHITE,
                        .select = RC_SELECT_NONE);
            }
            rcBox(.h = "8") {}
            compose_button(st, true);
            rcBox(.h = "8") {}
            tab_item(app, st, TAB_HOME, true);
            tab_item(app, st, TAB_SEARCH, true);
            tab_item(app, st, TAB_ALERTS, true);
            tab_item(app, st, TAB_PROFILE, true);
        }
        rcColumn(.h = "grow", .wType = RC_PX(listW), .className = "border-r border-border") {
            nav_root_screen(app, &st->nav, draw_screen, st);
        }
        rcColumn(.w = "grow", .h = "grow") {
            nav_stage(app, &st->nav, draw_screen, st, 0.0f);
        }
    }
}

/* THE BACK KEY. RC_KEY_ESCAPE is the desktop Escape and, on Android, the back button: the
   navigator's rule; at the first tab's root it goes unread, and Android's Back leaves the app. AN
   OPEN SHEET AND A FOCUSED TEXT FIELD COME FIRST: each takes that Escape itself, the sheet to close
   and the field to blur, and rcKeyPressed reads false for it, so one press never also goes back. */
static void read_back_key(AppState *st) {
    /* Read only where it does something: at Home's list Android's Back stays the system's. */
    if (nav_can_back(&st->nav) && rcKeyPressed(RC_KEY_ESCAPE))
        nav_back(&st->nav);
}

/* Home's date line, from the device's clock and calendar at every frame: one clock read and one
   calendar conversion, so a phone app resumed the next day, or a window left open past midnight,
   shows the new date at its next frame with no wake-up of its own. With no clock reading (0) or no
   answer from the system, the date stays as it was, app_seed's at first. */
static void read_today(AppState *st) {
    const double unixNow = rcUnixTimeSeconds();
    RC_LocalTime local;

    if (unixNow > 0.0 && rcLocalTime(unixNow, &local)) {
        st->todayWeekday = local.weekday;
        st->todayMonth   = local.month;
        st->todayDay     = local.day;
    }
}

static void update(RC_App *app, void *userData) {
    AppState *st = (AppState *)userData;
    int       i;

    read_today(st);
    if (st->shown && !st->picturesReady)
        load_pictures(rcAppArena(app), st);
    st->lastFrame = st->now;
    st->now = rcAppTime(app);
    nav_update(&st->nav, st->now);
    step_sheet(st);
    if (st->refreshing && st->now - st->refreshStart >= REFRESH_SECONDS)
        finish_refresh(st);
    /* The pull's band springs shut by the clock, on whichever tab is up: left open where no screen
       draws it, it would ask for frames for good and keep every card's press from showing. A drag
       ending on a card is not a tap on it for that one frame, so the mark is cleared here too. */
    st->pullWasDrag = false;
    if (!st->pulling && st->pull > 0.0f) {
        const float dt = (float)(st->now - st->lastFrame);

        st->pull -= st->pull * (dt * 14.0f > 1.0f ? 1.0f : dt * 14.0f);
        if (st->pull < 1.0f)
            st->pull = 0.0f;
    }
    /* A toast in use waits, as a web snackbar does: the time since the last frame, spent with Undo
       focused or under the pointer, or under a sheet, whose scrim puts Undo out of reach, is added
       back to its clock, which runs on from there once you let go. Letting go of it, by a key or
       the pointer, draws a frame, as a sheet's slide away does, so the whole gap was spent as the
       last frame left it (toastHeld, read at the end of layout). */
    if (st->toastHeld)
        st->toastUntil += st->now - st->lastFrame;
    if (st->toastText[0] && st->now >= st->toastUntil)
        st->toastText[0] = '\0';

    read_back_key(st);
    if (rcModDown(RC_MOD_PRIMARY)) {
        if (rcKeyPressed(RC_KEY_N))
            open_sheet(st, SHEET_COMPOSE);
        if (rcKeyPressed(RC_KEY_R)) {
            st->nav.tab = TAB_HOME;
            start_refresh(st);
        }
        for (i = 0; i < TAB_COUNT; i++)
            if (rcKeyPressed((RC_Key)(RC_KEY_1 + i)))
                st->nav.tab = i;
    }
}

/* What moves on its own. RayClay draws only when something happens, so each of these asks for its
   next frame until it settles, and the app sleeps again. */
static bool is_animating(const AppState *st) {
    return nav_is_moving(&st->nav) || st->refreshing ||
           (st->sheet != SHEET_NONE && !st->sheetDragging && st->sheetT != st->sheetTo) ||
           (st->pull > 0.0f && !st->pulling) ||
           (st->popPost >= 0 && st->now - st->popStart < POP_SECONDS);
}

static void layout(RC_App *app, void *userData) {
    AppState    *st = (AppState *)userData;
    RC_Window   *win = rcAppMainWindow(app);
    RC_Viewport  vp;
    RC_Style     s;

    /* THE WINDOW FIRST, as a web page paints before its images arrive: the first frame lays out
       every picture's box at its size, each avatar in its person's color, and asks for the
       second, whose update loads the pictures (docs/getting-started.md). */
    if (!st->picturesReady) {
        st->shown = true;
        rcWindowRequestFrame(win);
    }

    /* The theme is a setting, applied at the top of every frame, and the clear color has to follow
       it: a frame the runner holds back shows nothing else. A change of the system's scheme draws
       a frame by itself, so System follows it here with no event handler. */
    st->dark = st->theme == THEME_DARK ||
               (st->theme == THEME_SYSTEM && rcPreferredColorScheme() == RC_COLOR_SCHEME_DARK);
    rcSetStyle(app_style(st->dark));
    s = rcGetStyle();
    rcWindowSetClearColor(win, s.background);

    /* BRANCH ON THE SPACE, NEVER ON THE PLATFORM. rcViewport() reports layout units, so a desktop
       window dragged to 393 wide gets the phone shell for the same reason a phone does. */
    vp = rcViewport();
    st->nav.wide = vp.width >= (float)WIDE_W;
    st->nav.low = vp.height < (float)LOW_H;
    st->nav.instant = st->reduceMotion;

    /* THE SAFE AREA IS SPENT ONCE, AND BY WHOEVER SITS ON IT. The phone's bands take the top and
       the bottom for themselves, so their surfaces reach the glass; the rail and panes take all
       four here. */
    rcColumn(.id = "root", .bg = s.background, .pt = (float)(st->nav.wide ? vp.safe.top : 0.0f),
             .pb = (float)(st->nav.wide ? vp.safe.bottom : 0.0f), .pl = vp.safe.left,
             .pr = vp.safe.right, .w = "grow", .h = "grow") {
        if (st->nav.wide)
            wide_shell(app, st, vp);
        else
            phone_shell(app, st, vp);
        if (st->sheet != SHEET_NONE)
            sheet_view(app, st);
        if (st->toastText[0])
            toast_view(st);
    }

    /* A pull is let go in refresh_band, which only Home's root draws. One let go on another tab,
       switched to mid-pull, ends here and springs back: held, it would blank every
       card's press. */
    if (st->pulling && !rcPointerDown(RC_POINTER_LEFT))
        st->pulling = false;
    /* Read after the toast is declared. A held toast asks for no wake-up: its clock has stopped,
       and letting go of it draws a frame by itself. */
    st->toastHeld = st->toastText[0] && st->toastUndo != UNDO_NONE &&
                    (rcIsFocused("toast_undo") || rcShowsHover("toast_undo") ||
                     st->sheet != SHEET_NONE);
    if (is_animating(st))
        rcWindowRequestFrame(win);
    else if (st->toastText[0] && !st->toastHeld)
        rcWindowRequestFrameAfter(win, st->toastUntil - st->now);
}

#endif /* APP_APP_H */
