/*
    main.c - RayClay Sign-in: a sign-in form to copy into your own app

    The page an account-based app opens on: Continue with Google, Continue with
    Apple where the system offers it, an "or", email and password, a forgotten
    password, Sign in, and the way to a new account. The form is one call,
    signin_form() in src/app/signin.h, that returns what the user sent this frame,
    and signin_switch() sets the link between its pages wherever the app wants it.
    It shows a request running, a refusal, and a confirmation when the app says so.
    This file is the app around it: the card, the mascot, and a pretend server that
    answers after a moment, so every state can be seen with nothing on a network.
    Each answer's comment says where a real app's flow goes instead.

    Try it: any address with the password "rayclay" asks for a one-time code, 123456
    signs in, any other password or code is refused, and ada@example.com is taken if
    you sign up. With a server named (src/app/server.h; dev-server.py names one), a
    password sign-in is real: a bearer token, expiry, cancel and sign out.

    On a desktop the form is a centered card. On a phone, or in a window too
    narrow for the card, the form is the screen, inside the safe area, and the
    focused field scrolls clear of the soft keyboard. The theme follows the
    system's light or dark appearance until the button in the corner changes it.

    Shows: rcButton (a look in .className with hover:, active: and disabled:
    states, .leadingIcon, .disabled, full width by items-stretch), rcDefineClass,
    rcTextInput (RC_INPUT_EMAIL, RC_INPUT_PASSWORD, RC_INPUT_SHOWN_PASSWORD,
    RC_INPUT_ONE_TIME_CODE, .clearButton), a float over a field, rcSvg, rcIconEmit,
    rcSetFocus, .gradient, .shadow, rcAppTime with rcWindowRequestFrame,
    rcViewport().safe and .keyboard, rcPreferredColorScheme.

    Build target: rayclay_ex12_signin_form
*/
#include "app/signin.h"
#include "app/mascot.h"
#include "app/server.h"
#include "icons/rc_icons_moon.h"
#include "icons/rc_icons_sun.h"

#define DEMO_ADDRESS  "ada@example.com"
#define DEMO_PASSWORD "rayclay"
#define DEMO_CODE     "123456"
#define DEMO_LATENCY  1.1       /* seconds, about a real server's answer */
#define SHAKE_SECONDS 0.45
#define MASCOT_READ_SECONDS 1.5

typedef struct {
    SigninForm  form;
    Mascot      mascot;
    double      lastFrame;
    double      readUntil;                /* the mascot reads the field till then */
    int         typedChars;               /* how many characters the fields hold */
    double      answerAt;                 /* when the pretend server answers    */
    double      shakeAt;                  /* when a refusal began; 0 once done  */
    const char *signedInVia;              /* NULL until signed in               */
    char        who[SIGNIN_EMAIL_CAP];    /* the address of the account         */
    char        initial[5];               /* its first letter, for the avatar   */
    bool        isNewAccount;
    bool        dark;
    bool        schemeAsked;
    Server      server;                   /* a real one, when one is named (server.h) */
} AppState;

static bool same_text(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static void welcome(AppState *st, const char *address, const char *via) {
    size_t bytes = 1;

    rcStrCopy(st->who, address, sizeof st->who);
    /* The first letter whole, since UTF-8 spends up to four bytes on one: the bytes
       after a letter's first all read 10xxxxxx. Only an ASCII one is made a capital. */
    while (bytes < sizeof st->initial - 1 && (address[bytes] & 0xC0) == 0x80)
        bytes++;
    rcStrCopy(st->initial, address, bytes + 1);
    if (address[0] >= 'a' && address[0] <= 'z')
        st->initial[0] = (char)(address[0] - 'a' + 'A');
    st->signedInVia = via;
    signin_forget_password(&st->form);
}

static void refuse(RC_App *app, AppState *st, const char *why) {
    signin_refuse(&st->form, why);
    st->shakeAt = rcAppTime(app);
}

/* THE PRETEND SERVER'S ANSWER. A real app gets here from rcFetch's reply or an
   identity provider's redirect, and clears form.busy the same way. */
static void answer_request(RC_App *app, AppState *st) {
    SigninForm  *f = &st->form;
    SigninChoice asked = f->busy;

    f->busy = SIGNIN_NONE;
    switch (asked) {
    case SIGNIN_EMAIL:
        /* POST form.email and form.password to your server over HTTPS (rcFetch).
           A good password opens the second step, and the server sends the code. */
        if (same_text(f->password, DEMO_PASSWORD))
            signin_set_page(app, f, SIGNIN_PAGE_CODE);
        else
            refuse(app, st, "That email and password don't match. Try again, or reset "
                            "your password.");
        break;
    case SIGNIN_CODE:
        /* POST form.code the same way, and open the app's first screen on its answer. */
        if (same_text(f->code, DEMO_CODE))
            welcome(st, f->email, "Signed in with your password and a code");
        else
            refuse(app, st, "That code isn't right. Check it and try again, or send a new one.");
        break;
    case SIGNIN_RESEND:
        signin_confirm(f, "A new code is on its way.");
        rcSetFocus(SIGNIN_ID_CODE);
        break;
    case SIGNIN_CREATE:
        /* POST the new account to your server, the same way. */
        if (same_text(f->email, DEMO_ADDRESS)) {
            refuse(app, st, "An account with this address already exists. Sign in instead.");
        } else {
            st->isNewAccount = true;
            welcome(st, f->email, "New account, signed in");
        }
        break;
    case SIGNIN_RESET:
        /* Your server emails the reset link, and says the same whether or not the
           address has an account, so the form reveals nothing. */
        signin_confirm(f, "If that address has an account, a reset link is on its way.");
        break;
    case SIGNIN_GOOGLE:
        /* Google's OAuth flow: Google's sign-in SDK through a platform shim on
           Android and iOS (docs/mobile.md), or rcOpenUrl on Google's authorization
           URL, with a redirect back, on a desktop and the web. */
        welcome(st, "ada.lovelace@example.com", "Signed in with Google");
        break;
    case SIGNIN_APPLE:
        /* AuthenticationServices' Apple ID credential
           (ASAuthorizationAppleIDProvider) through a platform shim. */
        welcome(st, "ada@privaterelay.appleid.com", "Signed in with Apple");
        break;
    case SIGNIN_NONE:
        break;
    }
    /* The form is declared after this, but the mascot's next pose wants a frame. */
    rcWindowRequestFrame(rcAppMainWindow(app));
}

/* THE REAL SERVER'S ANSWERS (server.h): a token asks who it belongs to, and that answer signs
   in. A refusal, a failed request or an expired session puts the form back with the reason. */
static void real_answers(RC_App *app, AppState *st, double now) {
    Server     *sv = &st->server;
    const char *refusal;
    ServerCall  kind = server_answer(sv, rcAppArena(app), now, &refusal);

    if (kind == CALL_SIGN_IN && !refusal) {
        server_ask(sv, rcAppArena(app), CALL_ME, "GET", "/me", NULL);
    } else if (kind == CALL_ME && !refusal) {
        st->form.busy = SIGNIN_NONE;
        welcome(st, st->form.email, sv->via);
    } else if (refusal) {
        st->form.busy = SIGNIN_NONE;
        refuse(app, st, refusal);
    }
    if (sv->token[0] && now >= sv->until) {   /* the session the server granted is over */
        sv->token[0]    = '\0';
        st->signedInVia = NULL;
        signin_set_page(app, &st->form, SIGNIN_PAGE_SIGN_IN);
        refuse(app, st, "Your session ended. Sign in again.");
    } else if (sv->token[0]) {
        rcWindowRequestFrameAfter(rcAppMainWindow(app), sv->until - now);
    }
}

/* WHERE THE MASCOT LOOKS: up and away while the server thinks, and shut eyes for a
   password, with one open if it is shown. A field being typed in gets read along
   its length, and for a moment after the last key; a finger's field gets read for
   as long as it is focused. Otherwise the eyes follow the pointer, and with the
   pointer away it is the logo as drawn. */
static Mascot mascot_pose(const AppState *st, double now) {
    const SigninForm *f = &st->form;
    Mascot            pose = MASCOT_REST;
    RC_Box            box = rcGetElementBox("mascot");
    bool              reading = rcPointerIsCoarse() || now < st->readUntil;
    bool              code = rcIsFocused(SIGNIN_ID_CODE);

    if (st->signedInVia) {
        reading = false;
    } else if (rcIsFocused(SIGNIN_ID_PASSWORD)) {
        pose.gazeX = -0.3f;
        pose.gazeY = 0.8f;
        pose.lidLeft = 1.0f;
        pose.lidRight = f->showPassword ? 0.0f : 1.0f;
        return pose;
    }
    if (f->busy != SIGNIN_NONE) {
        pose.gazeX = 0.6f;
        pose.gazeY = -0.9f;
    } else if (reading && (code || rcIsFocused(SIGNIN_ID_EMAIL) || rcIsFocused(SIGNIN_ID_NAME))) {
        int n = signin_length(code ? f->code : rcIsFocused(SIGNIN_ID_EMAIL) ? f->email : f->name);
        int end = code ? SIGNIN_CODE_LEN : 30;

        pose.gazeX = -0.9f + 1.8f * (float)(n > end ? end : n) / (float)end;
        pose.gazeY = 0.9f;
    } else if (!rcPointerIsCoarse() && rcIsHovered("page") && box.found && box.width > 0.0f) {
        RC_Vec2 p = rcPointer();

        pose.gazeX = mascot_gaze(p.x - (box.x + box.width * 0.5f));
        pose.gazeY = mascot_gaze(p.y - (box.y + box.height * 0.5f));
    }
    return pose;
}

/* The keys typed since the last frame keep the mascot reading a little longer. */
static void note_typing(RC_App *app, AppState *st, double now) {
    const SigninForm *f = &st->form;
    int typed = signin_length(f->name) + signin_length(f->email) + signin_length(f->code);

    if (typed != st->typedChars) {
        st->typedChars = typed;
        st->readUntil  = now + MASCOT_READ_SECONDS;
        rcWindowRequestFrameAfter(rcAppMainWindow(app), MASCOT_READ_SECONDS);
    }
}

/* A REFUSAL SHAKES ITS HEAD, as a login window does: the offset dies away over a
   few keyframes, and each frame of it asks for the next. */
static int shake_offset(RC_App *app, AppState *st) {
    static const int KEYS[] = { 0, -8, 8, -6, 6, -3, 3, 0 };
    const int last = (int)(sizeof KEYS / sizeof KEYS[0]) - 1;
    double    t;
    int       k;

    if (st->shakeAt <= 0.0)
        return 0;
    t = (rcAppTime(app) - st->shakeAt) / SHAKE_SECONDS * last;
    if (t >= last) {
        st->shakeAt = 0.0;
        return 0;
    }
    k = (int)t;
    rcWindowRequestFrame(rcAppMainWindow(app));
    return KEYS[k] + (int)((double)(KEYS[k + 1] - KEYS[k]) * (t - k));
}

/* The title and what it means. An address gets a line of its own, the one run that
   may break anywhere: a long one must fit, and a sentence must keep its words.
   A PHONE ON ITS SIDE SPENDS ITS HEIGHT ON THE FIELDS: in a viewport wide enough for
   the card and too short for it, the mascot goes, and so does a line that only greets.
   The line an address follows stays, since it says what the address is. */
static void card_header(const Mascot *mascot, const char *title, const char *subtitle,
                        const char *address) {
    RC_Style    s = rcGetStyle();
    RC_Viewport vp = rcViewport();
    bool        sideways = vp.width >= (float)CARD_MIN_VW && vp.height < (float)CARD_MIN_VH;

    rcColumn(.gap = 16, .align = "tc", .w = "grow") {
        if (!sideways)
            mascot_draw(mascot);
        rcColumn(.gap = 4, .align = "tc", .w = "grow") {
            rcTextC(title, .font = rcFontSemibold(TEXT_TITLE), .color = s.text);
            if (!sideways || address)
                rcTextC(subtitle, .color = s.textMuted, .size = TEXT_LABEL, .textAlign = "c");
            if (address)
                rcTextC(address, .font = rcFontSemibold(TEXT_LABEL), .color = s.text,
                        .textAlign = "c", .wordBreak = "b");
        }
    }
}

/* THE SCREEN BEHIND THE FORM: in a real app, the app. Here, proof it got there, and
   the way back. */
static void account_summary(RC_App *app, AppState *st) {
    RC_Style s = rcGetStyle();

    rcColumn(.gap = SECTION_GAP, .w = "grow", .className = "items-stretch") {
        rcRow(.bg = rcWithAlpha(s.primary, 0.08f), .gap = 12, .p = 12, .align = "cl",
              .borderRadius = "all-lg", .w = "grow") {
            rcBox(.bg = s.primary, .align = "cc", .borderRadius = "all-full",
                  .w = "40", .h = "40") {
                rcTextC(st->initial, .font = rcFontSemibold(16.0f),
                        .color = signin_ink_on(s.primary), .select = RC_SELECT_NONE);
            }
            rcColumn(.gap = 4, .w = "grow") {
                rcTextC(st->who, .font = rcFontSemibold(TEXT_LABEL),
                        .color = s.text, .wordBreak = "b");
                rcTextC(st->signedInVia, .color = s.textMuted, .size = TEXT_NOTE);
            }
        }
        if (rcButton("sign_out", "Sign out", .className = "control")) {
            server_sign_out(&st->server, rcAppArena(app));
            st->signedInVia   = NULL;
            st->isNewAccount  = false;
            st->form.name[0]  = '\0';
            signin_set_page(app, &st->form, SIGNIN_PAGE_SIGN_IN);
        }
    }
}

/* What the form sent starts a request: the form waits and the button spins until
   the answer. */
static void start_request(RC_App *app, AppState *st, SigninChoice choice) {
    if (choice == SIGNIN_NONE)
        return;
    st->form.busy = choice;
    if (st->server.url && choice == SIGNIN_EMAIL) {
        if (!server_sign_in(&st->server, rcAppArena(app), st->form.email, st->form.password)) {
            st->form.busy = SIGNIN_NONE;
            refuse(app, st, "That email or password is too long to send.");
        }
        rcWindowRequestFrame(rcAppMainWindow(app));
        return;
    }
    st->answerAt  = rcAppTime(app) + DEMO_LATENCY;
    /* The form above this was drawn idle: draw it waiting. */
    rcWindowRequestFrame(rcAppMainWindow(app));
}

/* The form, the mascot and the title that goes with its page. */
static void card_body(RC_App *app, AppState *st) {
    switch (st->form.page) {
    case SIGNIN_PAGE_SIGN_IN:
        card_header(&st->mascot, "Sign in to RayClay",
                    "Welcome back. Enter your details to continue.", NULL);
        break;
    case SIGNIN_PAGE_SIGN_UP:
        card_header(&st->mascot, "Create your account",
                    "Start building with RayClay in a minute.", NULL);
        break;
    case SIGNIN_PAGE_RESET:
        card_header(&st->mascot, "Reset your password",
                    "Enter your email and we will send you a reset link.", NULL);
        break;
    case SIGNIN_PAGE_CODE:
        card_header(&st->mascot, "Check your email", "We sent a 6-digit code to",
                    st->form.email);
        break;
    }
    start_request(app, st, signin_form(app, &st->form));
}

/* What the demo wants at this step, and a click that types it in: the way through
   for someone who only came to look. */
static void demo_tip(AppState *st) {
    RC_Style    s = rcGetStyle();
    SigninForm *f = &st->form;
    bool        code = f->page == SIGNIN_PAGE_CODE;

    if (st->server.url && !code) {
        rcRow(.gap = 4, .align = "cc", .w = "grow", .className = "flex-wrap") {
            rcTextC(st->server.call ? "Asking the server..." : "Signing in at a real server.",
                    .color = s.textMuted, .size = TEXT_LABEL, .textAlign = "c");
            if (st->server.call && signin_link("cancel_call", "Cancel", false)) {
                server_cancel(&st->server);
                f->busy = SIGNIN_NONE;
            }
        }
    }
    rcRow(.gap = 4, .align = "cc", .w = "grow", .className = "flex-wrap") {
        rcTextC(code ? "Demo code: " DEMO_CODE "."
                     : "Demo: any address, password \"" DEMO_PASSWORD "\".",
                .color = s.textMuted, .size = TEXT_LABEL, .textAlign = "c");
        if (signin_link("demo_fill", "Fill it in", f->busy != SIGNIN_NONE)) {
            if (code) {
                rcStrCopy(f->code, DEMO_CODE, sizeof f->code);
                rcSetFocus(SIGNIN_ID_CODE);
            } else {
                rcStrCopy(f->email, DEMO_ADDRESS, sizeof f->email);
                rcStrCopy(f->password, DEMO_PASSWORD, sizeof f->password);
                rcSetFocus(SIGNIN_ID_PASSWORD);
            }
        }
    }
}

/* THE USER'S APPEARANCE, as a page reads prefers-color-scheme. No stated
   preference keeps the light page a sign-in page usually is. */
static void follow_scheme(AppState *st) {
    st->dark = rcPreferredColorScheme() == RC_COLOR_SCHEME_DARK;
}

static void on_app_event(RC_AppEvent event, void *userData) {
    if (event == RC_APP_EVENT_COLOR_SCHEME_CHANGED)
        follow_scheme((AppState *)userData);
}

/* The corner button: the page's own choice of theme, until the system next changes. It
   stands 24 px in from the right: under a mouse the page's scrollbar lies over that edge,
   in a 12 px lane, and the button keeps 12 px of air from it, as it does from the top. */
static void theme_button(AppState *st, RC_Viewport vp) {
    rcBox(.floating = { .to = RC_ATTACH_ROOT, .parent = RC_ANCHOR_TOP_RIGHT,
                        .element = RC_ANCHOR_TOP_RIGHT,
                        .offset = { -(vp.safe.right + 24.0f), vp.safe.top + 12.0f } },
          .tooltip = st->dark ? "Switch to light theme" : "Switch to dark theme") {
        if (rcButton("theme", "", .className = "control",
                     .leadingIcon = st->dark ? rcIconSun : rcIconMoon))
            st->dark = !st->dark;
    }
}

static void layout(RC_App *app, void *userData) {
    AppState   *st = (AppState *)userData;
    RC_Viewport vp = rcViewport();
    RC_Style    s;
    Palette     pal;
    RC_Shadow   noShadow = { .color = RC_UNSET_COLOR };
    RC_Gradient noWash = { .from = RC_UNSET_COLOR, .to = RC_UNSET_COLOR, .dir = "" };
    double      now = rcAppTime(app);
    bool        card;
    float       side, bottom;
    int         shake;

    /* Asked on the first frame, not in main: an SDL3 build learns the
       appearance only once the app and its window exist. */
    if (!st->schemeAsked) {
        st->schemeAsked = true;
        st->mascot = MASCOT_REST;
        follow_scheme(st);
    }
    rcSetStyle(app_style(st->dark));
    s    = rcGetStyle();
    pal  = app_palette(st->dark);
    card = vp.width >= (float)CARD_MIN_VW && vp.height >= (float)CARD_MIN_VH;
    side = card ? (float)PAGE_AIR : (float)PAGE_SIDE;
    /* THE PAGE SPENDS THE SAFE AREA, and the bottom is whichever covers more,
       the home indicator or the keyboard: a focused field scrolls into what is
       left, as the scroll root's own end. */
    bottom = vp.keyboard.bottom > vp.safe.bottom ? vp.keyboard.bottom : vp.safe.bottom;
    rcWindowSetClearColor(rcAppMainWindow(app), card ? s.background : s.surface);

    if (st->server.call || st->server.token[0])
        real_answers(app, st, now);
    else if (st->form.busy != SIGNIN_NONE && now >= st->answerAt)
        answer_request(app, st);
    note_typing(app, st, now);
    if (mascot_ease(&st->mascot, mascot_pose(st, now), (float)(now - st->lastFrame)))
        rcWindowRequestFrame(rcAppMainWindow(app));
    st->lastFrame = now;
    shake = shake_offset(app, st);

    /* "Cc" centers the column in the page while it fits, and lets it scroll from
       its top once it does not: CSS's safe center. The two side paddings trade the
       shake between them, so the page never changes width. */
    rcColumn(.id = "page", .bg = card ? s.background : s.surface,
             .pt = (float)(vp.safe.top + (float)PAGE_AIR),
             .pb = (float)(bottom + (float)PAGE_AIR),
             .pl = (float)(vp.safe.left + side + (float)shake),
             .pr = (float)(vp.safe.right + side - (float)shake),
             .align = "Cc", .scroll = "v", .w = "grow", .h = "grow",
             .gradient = card ? pal.pageWash : noWash) {
        rcColumn(.gap = 16, .w = "grow", .wMax = RC_VAL(CARD_W), .className = "items-stretch") {
            /* THE CARD IS ALWAYS DECLARED and only its colors and padding change:
               on the narrow arm the form is the screen. */
            rcColumn(.id = "card", .bg = card ? s.surface : RC_UNSET_COLOR,
                     .gap = (float)(card ? 0 : SECTION_GAP), .borderRadius = "all-xl",
                     .border = { .color = card ? rcWithAlpha(s.text, 0.08f) : RC_UNSET_COLOR,
                                 .width = 1 },
                     .w = "grow", .shadow = card ? pal.cardShadow : noShadow,
                     .className = "items-stretch") {
                rcColumn(.gap = SECTION_GAP, .p = (float)(card ? CARD_PAD : 0), .w = "grow",
                         .className = "items-stretch") {
                    if (st->signedInVia) {
                        if (st->isNewAccount)
                            card_header(&st->mascot, "Welcome to RayClay",
                                        "You are signed in as", st->form.name);
                        else
                            card_header(&st->mascot, "Welcome back", "You are signed in.", NULL);
                        account_summary(app, st);
                    } else {
                        card_body(app, st);
                    }
                }
                if (!st->signedInVia) {
                    if (card)
                        rcBox(.bg = rcWithAlpha(s.border, 0.6f), .w = "grow", .h = "1") {}
                    rcRow(.bg = card ? rcWithAlpha(s.primary, 0.05f) : RC_UNSET_COLOR,
                          .py = (float)(card ? 16 : 0), .align = "cc",
                          .borderRadius = "b-xl", .w = "grow") {
                        signin_switch(app, &st->form);
                    }
                }
            }
            if (!st->signedInVia && (st->form.page == SIGNIN_PAGE_SIGN_IN ||
                                     st->form.page == SIGNIN_PAGE_CODE))
                demo_tip(st);
        }
    }
    rcScrollbar("page");
    /* The form scrolls under the clock, where its text would show: a strip of the narrow
       page's color covers it. The card's wash has no flat color a strip could match. */
    if (!card && vp.safe.top > 0.0f)
        rcBox(.bg = s.surface, .w = "100vw", .hType = RC_PX(vp.safe.top),
              .floating = { .to = RC_ATTACH_ROOT }) {}
    theme_button(st, vp);

    /* Escape steps back to the sign-in page. It is read only where it does that, so on the
       sign-in page itself Android's Back stays the system's and leaves the app. */
    if (!st->signedInVia && st->form.page != SIGNIN_PAGE_SIGN_IN && st->form.busy == SIGNIN_NONE
        && rcKeyPressed(RC_KEY_ESCAPE))
        signin_set_page(app, &st->form, SIGNIN_PAGE_SIGN_IN);
}

int main(void) {
    static AppState state;

    rcSetStyle(app_style(false));
    app_define_classes();
    server_init(&state.server);
    rcSetAppEventHandler(on_app_event, &state);   /* COLOR_SCHEME_CHANGED */

    RC_AppOptions opts = {
        .width = 1040, .height = 820, .title = "RayClay Sign-in",
        .clearColor = rcGetStyle().background,
        .layoutCallback = layout, .userData = &state,
        .renderMode = RC_RENDER_ON_DEMAND,   /* parks between keystrokes    */
    };
    return rcRunApp(&opts);
}
