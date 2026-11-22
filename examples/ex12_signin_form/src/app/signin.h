/*
    signin.h - a sign-in form to drop into an app: one call for the form and one
    for the switch link, once a frame

        static SigninForm form;                 the app keeps the form's state
        switch (signin_form(app, &form)) {      inside your layout, where it sits
        case SIGNIN_EMAIL:                      send form.email and form.password
            form.busy = SIGNIN_EMAIL;           the form waits, its button spins
            break;
        case SIGNIN_GOOGLE:                     start Google sign-in
            ...
        }
        signin_switch(app, &form);              "Don't have an account?", where it sits

    The form is a column as wide as the box you declare it in and as tall as
    its content. It returns what the user sent THIS frame, and SIGNIN_NONE on
    every other one. It owns its pages (sign in, sign up, reset password, and the
    one-time code your app opens after a good password) and the moves between them,
    and asks your app only for answers: while form.busy names a request the form
    is inert, and when the answer comes you clear busy and, if the answer is no,
    call signin_refuse. Copy this file with theme.h's tokens and named looks (its
    app_define_classes, called once at start-up) and platform.h's one answer:
    every theme color comes from rcGetStyle() as it is drawn.

    THE ORDER IS THE ONE PEOPLE KNOW FROM EVERY SIGN-IN PAGE: the one-tap
    providers first, an "or", the email and the password, with the way back from
    a forgotten password on the password's own label row, then the primary action,
    and the way to an account for someone who has none.

    A FIELD IS WRONG ONLY ONCE THE USER IS DONE WITH IT: an address when they
    leave it, an empty field only when they press the button. From then on the
    message follows the typing, so it clears the moment the field is right.
*/
#ifndef APP_SIGNIN_H
#define APP_SIGNIN_H

#include "rayclay.h"
#include "icons/rc_icons_circle_alert.h"
#include "icons/rc_icons_circle_check.h"
#include "icons/rc_icons_eye.h"
#include "icons/rc_icons_eye_off.h"
#include "app/theme.h"
#include "platform/platform.h"

/* Where the two brand marks are read at run time. A desktop finds the folder
   beside the program, and the phones package it under the same path; the web
   build sets its own. */
#ifndef SIGNIN_LOGO_DIR
#define SIGNIN_LOGO_DIR "examples/assets/logos/"
#endif

enum {
    SIGNIN_NAME_CAP     = 128,
    SIGNIN_EMAIL_CAP    = 256,
    SIGNIN_PASSWORD_CAP = 128,
    SIGNIN_CODE_CAP     = 16,     /* room for a paste like "123 456" */
    SIGNIN_BANNER_CAP   = 320,
    SIGNIN_PASSWORD_MIN = 8,
    SIGNIN_CODE_LEN     = 6
};

typedef enum {
    SIGNIN_PAGE_SIGN_IN = 0,
    SIGNIN_PAGE_SIGN_UP,
    SIGNIN_PAGE_RESET,
    SIGNIN_PAGE_CODE      /* the app opens this one, after a good password    */
} SigninPage;

typedef enum {
    SIGNIN_NONE = 0,
    SIGNIN_EMAIL,         /* form.email and form.password, both filled in     */
    SIGNIN_CREATE,        /* form.name, form.email and a long enough password */
    SIGNIN_RESET,         /* form.email, for the reset link                   */
    SIGNIN_CODE,          /* form.code, all six digits                        */
    SIGNIN_RESEND,        /* send another code                                */
    SIGNIN_GOOGLE,
    SIGNIN_APPLE          /* only where platform.h offers it                  */
} SigninChoice;

typedef struct {
    SigninPage   page;
    char         name[SIGNIN_NAME_CAP];
    char         email[SIGNIN_EMAIL_CAP];
    char         password[SIGNIN_PASSWORD_CAP];
    char         code[SIGNIN_CODE_CAP];
    SigninChoice busy;           /* set by the app: the request now running     */
    char         banner[SIGNIN_BANNER_CAP];   /* the last answer, "" for none   */
    bool         bannerIsError;
    bool         showPassword;
    bool         submitted;      /* the button was pressed: every message shows */
    bool         emailLeft;      /* the address was left: its message may show  */
    bool         emailFocused;   /* as of the last frame, to see it being left  */
    bool         focused;        /* the page's first field has had its focus    */
} SigninForm;

/* The fields' ids: Enter and rcSetFocus name them. Every id here starts signin_,
   so none collides with the app's own. */
#define SIGNIN_ID_NAME     "signin_name"
#define SIGNIN_ID_EMAIL    "signin_email"
#define SIGNIN_ID_PASSWORD "signin_password"
#define SIGNIN_ID_CODE     "signin_code"

/* THE SHAPE OF AN ADDRESS, as a browser checks an <input type=email> before it
   sends a form: text, one @, then a domain with a dot inside it. Whether the
   account exists is the server's answer, never the form's. */
static inline bool signin_email_ok(const char *s) {
    const char *at = NULL, *p;

    for (p = s; *p; p++) {
        if (*p == ' ') return false;
        if (*p == '@') { if (at) return false; at = p; }
    }
    if (!at || at == s) return false;
    for (p = at + 1; *p; p++)
        if (*p == '.' && p > at + 1 && p[1]) return true;
    return false;
}

static inline int signin_length(const char *s) {
    int n = 0;

    while (s[n])
        n++;
    return n;
}

/* The message under each field, or NULL. Literals, because rcText borrows its
   string until the frame is drawn. */
static inline const char *signin_name_error(const SigninForm *f) {
    return f->submitted && !f->name[0] ? "Enter your name" : NULL;
}

static inline const char *signin_email_error(const SigninForm *f) {
    if (!f->email[0])
        return f->submitted ? "Enter your email address" : NULL;
    if ((f->submitted || f->emailLeft) && !signin_email_ok(f->email))
        return "Enter an address like name@example.com";
    return NULL;
}

static inline const char *signin_password_error(const SigninForm *f) {
    if (!f->submitted)
        return NULL;
    if (!f->password[0])
        return "Enter your password";
    if (f->page == SIGNIN_PAGE_SIGN_UP && signin_length(f->password) < SIGNIN_PASSWORD_MIN)
        return "Use at least 8 characters";
    return NULL;
}

static inline const char *signin_code_error(const SigninForm *f) {
    return f->submitted && signin_length(f->code) < SIGNIN_CODE_LEN ? "Enter all 6 digits" : NULL;
}

/* A password is not the app's to keep: once it is sent, every byte goes. */
static inline void signin_forget_password(SigninForm *f) {
    size_t i;

    for (i = 0; i < sizeof f->password; i++)
        f->password[i] = '\0';
}

/* The server's no, in its own words, and the caret back where the fix goes. */
static inline void signin_refuse(SigninForm *f, const char *why) {
    rcStrCopy(f->banner, why, sizeof f->banner);
    f->bannerIsError = true;
    if (f->page == SIGNIN_PAGE_SIGN_UP) {
        rcSetFocus(SIGNIN_ID_EMAIL);
    } else if (f->page == SIGNIN_PAGE_CODE) {
        f->code[0] = '\0';
        rcSetFocus(SIGNIN_ID_CODE);
    } else {
        signin_forget_password(f);
        rcSetFocus(SIGNIN_ID_PASSWORD);
    }
}

/* The server's yes, where it has more to say than opening the next screen. */
static inline void signin_confirm(SigninForm *f, const char *what) {
    rcStrCopy(f->banner, what, sizeof f->banner);
    f->bannerIsError = false;
}

/* Another page starts clean, but keeps the address: nobody types it twice. This
   frame already drew the old page, so it asks for the next one. */
static inline void signin_set_page(RC_App *app, SigninForm *f, SigninPage page) {
    f->page         = page;
    f->banner[0]    = '\0';
    f->submitted    = false;
    f->emailLeft    = false;
    f->emailFocused = false;
    f->focused      = false;
    f->showPassword = false;
    f->code[0]      = '\0';
    signin_forget_password(f);
    rcWindowRequestFrame(rcAppCurrentWindow(app));
}

/* THE MARKS ARE THE BRANDS' OWN FILES (examples/assets/logos/BRAND-MARKS.md),
   each a button's leading icon. The G keeps Google's four colors.
   APPLE'S LOGO IS NOT SHIPPED: Apple's artwork comes with a license that does
   not let RayClay redistribute it. Download it from Apple's Sign in with Apple
   resources, save it as examples/assets/logos/apple-logo.svg as BRAND-MARKS.md
   describes, and define SIGNIN_APPLE_LOGO 1; until then the Apple button shows
   its label alone. The logo takes the label's color, white on the black button
   and black on the white one. */
#ifndef SIGNIN_APPLE_LOGO
#define SIGNIN_APPLE_LOGO 0
#endif
static inline void signin_google_mark(float size, RC_Color color) {
    rcSvg(SIGNIN_LOGO_DIR "google-g.svg", size, color);
}

#if PLATFORM_SIGN_IN_WITH_APPLE && SIGNIN_APPLE_LOGO
static inline void signin_apple_mark(float size, RC_Color color) {
    rcSvg(SIGNIN_LOGO_DIR "apple-logo.svg", size, color);
}
#define SIGNIN_APPLE_ICON signin_apple_mark
#else
#define SIGNIN_APPLE_ICON NULL   /* no icon: the label alone, centered */
#endif

/* THE SPINNER: twelve spokes, the newest darkest. It is the one glyph drawn here,
   because it is an animation rather than a picture. An icon callback carries no
   data of its own, so signin_form leaves the frame's step in signin_spin_step. */
static int signin_spin_step;

static inline void signin_spinner_glyph(RC_BoundingBox b, RC_Color c, const void *u) {
    static const RC_IconPoint DIR[12] = {
        {  0.0f,   -1.0f   }, {  0.5f,   -0.866f }, {  0.866f, -0.5f   }, {  1.0f,    0.0f   },
        {  0.866f,  0.5f   }, {  0.5f,    0.866f }, {  0.0f,    1.0f   }, { -0.5f,    0.866f },
        { -0.866f,  0.5f   }, { -1.0f,    0.0f   }, { -0.866f, -0.5f   }, { -0.5f,   -0.866f }
    };
    int step = *(const int *)u, k;

    for (k = 0; k < 12; k++) {
        int age = (step - k + 12) % 12;

        rcIconDrawRoundLine(b, 12.0f + 5.0f * DIR[k].x, 12.0f + 5.0f * DIR[k].y,
                            12.0f + 10.0f * DIR[k].x, 12.0f + 10.0f * DIR[k].y, 24.0f, 2.25f,
                            rcWithAlpha(c, (uint8_t)(255 - 18 * age) / 255.0f));
    }
}

static inline void signin_spinner(float size, RC_Color color) {
    rcIconEmit(size, color, signin_spinner_glyph, &signin_spin_step);
}

static inline bool signin_is_light(RC_Color c) {
    return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b > 140.0f;
}

/* THE INK ON A FILL: black on a light one and white on a dark one, by the
   fill's luminance, as RC_BTN_PRIMARY picks its own label, so any accent the
   app installs reads. A button names it in its class, text-black or
   text-white. */
static inline RC_Color signin_ink_on(RC_Color fill) {
    return signin_is_light(fill) ? RC_BLACK : RC_WHITE;
}

/* A LINK IS TEXT THAT ANSWERS A CLICK. A box with an id is a button by the
   library's own rule (rcClicked, which gives it the pointer cursor too). That box
   floats over the run, so a finger gets a finger's height while the row keeps one
   line's: the text stays beside the label or the sentence it belongs to. */
static inline bool signin_link(const char *id, const char *label, bool off) {
    RC_Style s = rcGetStyle();
    RC_Color ink = off ? s.textMuted : !rcShowsHover(id) ? s.primary
                 : rcShowsActive(id) ? s.text : s.primaryHover;

    rcBox(.align = "cl") {
        rcTextC(label, .font = rcFontSemibold(TEXT_LABEL), .color = ink,
                .select = RC_SELECT_NONE);
        rcBox(.id = id, .disabled = off, .wType = RC_GROW,
              .hType = rcPointerIsCoarse() ? RC_PX(HIT_MIN) : RC_GROW,
              .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_CENTER,
                            .element = RC_ANCHOR_CENTER }) {}
    }
    return rcClicked(id);
}

/* A LABEL FOCUSES ITS FIELD, as <label for=...> does. */
static inline void signin_label(const char *id, const char *text, const char *field) {
    rcBox(.id = id) {
        rcTextC(text, .font = rcFontSemibold(TEXT_LABEL), .color = rcGetStyle().text,
                .select = RC_SELECT_NONE);
    }
    if (rcClicked(id))
        rcSetFocus(field);
}

static inline void signin_message(const char *message) {
    RC_Style s = rcGetStyle();

    if (!message)
        return;
    rcRow(.gap = 4, .align = "cl", .w = "grow") {
        rcIconCircleAlert(14.0f, s.danger);
        rcTextC(message, .color = s.danger, .size = TEXT_NOTE);
    }
}

/* The last answer, at the top of the form, red for a refusal, green for a yes. */
static inline void signin_banner(const SigninForm *f) {
    RC_Style s = rcGetStyle();
    RC_Color tone = f->bannerIsError ? s.danger : s.success;

    if (!f->banner[0])
        return;
    rcRow(.bg = rcWithAlpha(tone, 0.1f), .gap = 8, .p = 12, .align = "tl",
          .borderRadius = "all-md", .border = { .color = rcWithAlpha(tone, 0.35f), .width = 1 },
          .w = "grow") {
        if (f->bannerIsError)
            rcIconCircleAlert(20.0f, tone);
        else
            rcIconCircleCheck(20.0f, tone);
        rcBox(.w = "grow") {
            rcTextC(f->banner, .color = s.text, .size = TEXT_LABEL, .lineHeight = 20);
        }
    }
}

/* The "or" between the providers and the fields: a hairline each side. */
static inline void signin_divider(void) {
    RC_Style s = rcGetStyle();

    rcRow(.gap = 12, .align = "cc", .w = "grow") {
        rcBox(.bg = s.border, .w = "grow", .h = "1") {}
        rcTextL("or", .color = s.textMuted, .size = TEXT_NOTE,
                .select = RC_SELECT_NONE);
        rcBox(.bg = s.border, .w = "grow", .h = "1") {}
    }
}

/* A FIELD IS RINGED WHILE IT HAS THE CARET: the box around it casts a soft band in
   the accent, as a web form's focus halo is. */
static inline RC_Shadow signin_field_ring(const char *id, bool bad) {
    const RC_Style *s = rcGetStylePtr();

    if (!rcIsFocused(id))
        return RC_LIT(RC_Shadow){ .color = RC_UNSET_COLOR };
    return RC_LIT(RC_Shadow){ .color = rcWithAlpha(bad ? s->danger : s->primary, 0.25f),
                              .spread = 3.0f };
}

/* A field draws its edge in the style's border color, its focus line, caret and
   selection in the primary and its placeholder in the muted color. So a wrong field is
   the same field under a style lent for that one call, and a hint reads lighter than
   the words around it. A class would reach the edge alone: it names no placeholder,
   caret or selection color, and a border color it names stays under focus with the
   primary's ring outside it, where a wrong field's focus line turns red. */
static inline RC_Style signin_lent_for(RC_Style s, bool bad) {
    s.textMuted = rcWithAlpha(s.textMuted, 0.85f);
    if (bad) {
        s.border  = s.danger;
        s.primary = s.danger;
    }
    return s;
}

/* The email and the name: a label, a field, a message. A field that holds text ends in
   an x that empties it (.clearButton), and a press on the x leaves the focus where it is. */
static inline void signin_text_field(const SigninForm *form, const char *labelId,
                                     const char *label, const char *id, char *buf, int cap,
                                     const char *hint, RC_InputKind kind, const char *error) {
    RC_Style s = rcGetStyle();
    bool     off = form->busy != SIGNIN_NONE;

    rcColumn(.gap = LABEL_GAP, .w = "grow", .className = "items-stretch") {
        signin_label(labelId, label, id);
        rcBox(.bg = s.surface, .borderRadius = "all-md", .w = "grow",
              .shadow = signin_field_ring(id, error != NULL)) {
            rcSetStyle(signin_lent_for(s, error != NULL));
            rcTextInput(id, buf, cap, .disabled = off, .className = "control",
                        .placeholder = hint, .inputKind = kind, .clearButton = true);
            rcSetStyle(s);
        }
        signin_message(error);
    }
}

/* HOW STRONG A NEW PASSWORD IS, 0 to 4: a point once anything is typed and, from the
   minimum on, one for going well past it, one for mixed case, one for a digit or a
   symbol. A nudge for the person typing, not a rule: only the minimum is refused. */
static inline int signin_password_score(const char *p) {
    bool lower = false, upper = false, other = false;
    int  n = 0;

    for (; p[n]; n++) {
        char c = p[n];

        if (c >= 'a' && c <= 'z')      lower = true;
        else if (c >= 'A' && c <= 'Z') upper = true;
        else                           other = true;
    }
    if (n < SIGNIN_PASSWORD_MIN)
        return n ? 1 : 0;
    return 1 + (n >= 12) + (lower && upper) + other;
}

static inline RC_Color signin_score_color(int score) {
    const RC_Style *s = rcGetStylePtr();

    return score <= 1 ? s->danger : score == 2 ? s->warning : s->success;
}

/* The word on the label row. */
static inline void signin_strength_word(const char *password) {
    static const char *const WORDS[5] = { "", "Weak", "Fair", "Good", "Strong" };
    int score = signin_password_score(password);

    rcTextC(WORDS[score], .font = rcFontSemibold(TEXT_NOTE), .color = signin_score_color(score),
            .select = RC_SELECT_NONE);
}

/* Four segments under the field, that fill and warm up as the password gets better. */
static inline void signin_strength_bar(const char *password) {
    int      score = signin_password_score(password), k;
    RC_Color fill = signin_score_color(score);
    RC_Color empty = rcWithAlpha(rcGetStylePtr()->border, 0.7f);

    rcRow(.gap = 4, .w = "grow") {
        for (k = 0; k < 4; k++)
            rcBox(.bg = k < score ? fill : empty, .borderRadius = "all-full",
                  .w = "grow", .h = "4") {}
    }
}

/* THE PASSWORD: its label row holds what belongs to it, and the eye sits inside
   the field, where people look for it: at the right edge, in place of the clear x
   the other fields end in, as Windows' own password box trades its x for an eye.
   Show turns the field's kind, as a page turns its input's type to text: the
   keyboard stays a password's. The eye asks for the focus back, because a press
   on it took it from the field. */
static inline void signin_password_field(RC_App *app, SigninForm *form, const char *error) {
    RC_Style s = rcGetStyle();
    bool     off = form->busy != SIGNIN_NONE;
    bool     signUp = form->page == SIGNIN_PAGE_SIGN_UP;

    rcColumn(.gap = LABEL_GAP, .w = "grow", .className = "items-stretch") {
        rcRow(.align = "cl", .w = "grow") {
            signin_label("signin_password_label", "Password", SIGNIN_ID_PASSWORD);
            rcSpacer() {}
            if (signUp) {
                signin_strength_word(form->password);
            } else if (signin_link("signin_forgot", "Forgot password?", off)) {
                signin_set_page(app, form, SIGNIN_PAGE_RESET);
            }
        }
        rcBox(.bg = s.surface, .borderRadius = "all-md", .w = "grow",
              .shadow = signin_field_ring(SIGNIN_ID_PASSWORD, error != NULL)) {
            rcSetStyle(signin_lent_for(s, error != NULL));
            rcTextInput(SIGNIN_ID_PASSWORD, form->password, (int)sizeof form->password,
                        .disabled = off, .className = "control pr-12",
                        .placeholder = signUp ? "At least 8 characters" : NULL,
                        .inputKind = form->showPassword ? RC_INPUT_SHOWN_PASSWORD
                                                        : RC_INPUT_PASSWORD);
            rcSetStyle(s);
            rcBox(.id = "signin_eye", .align = "cc", .disabled = off,
                  .wType = RC_PX(CONTROL_H), .hType = RC_PX(CONTROL_H),
                  .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_CENTER_RIGHT,
                                .element = RC_ANCHOR_CENTER_RIGHT },
                  .tooltip = form->showPassword ? "Hide password" : "Show password") {
                RC_Color c = !rcShowsHover("signin_eye") ? s.textMuted
                           : rcShowsActive("signin_eye") ? s.primary : s.text;

                if (form->showPassword)
                    rcIconEyeOff(20.0f, c);
                else
                    rcIconEye(20.0f, c);
            }
            if (rcClicked("signin_eye")) {
                form->showPassword = !form->showPassword;
                if (!rcPointerIsCoarse())
                    rcSetFocus(SIGNIN_ID_PASSWORD);
            }
        }
        if (signUp)
            signin_strength_bar(form->password);
        signin_message(error);
    }
}

/* THE ONE-TIME CODE. RC_INPUT_ONE_TIME_CODE raises the digit pad, and iOS and a
   phone's browser offer a code that just arrived above the keyboard. The field keeps
   digits only, whatever was pasted, and the sixth digit sends the form: true on that
   frame, so nobody has to find a button for it. */
static inline bool signin_code_field(SigninForm *form, const char *error) {
    RC_Style s = rcGetStyle();
    bool     off = form->busy != SIGNIN_NONE;
    bool     complete = false;

    rcColumn(.gap = LABEL_GAP, .w = "grow", .className = "items-stretch") {
        signin_label("signin_code_label", "Verification code", SIGNIN_ID_CODE);
        rcBox(.bg = s.surface, .borderRadius = "all-md", .w = "grow",
              .shadow = signin_field_ring(SIGNIN_ID_CODE, error != NULL)) {
            rcSetStyle(signin_lent_for(s, error != NULL));
            if (rcTextInput(SIGNIN_ID_CODE, form->code, (int)sizeof form->code,
                            .disabled = off, .className = "control",
                            .placeholder = "6-digit code",
                            .inputKind = RC_INPUT_ONE_TIME_CODE, .clearButton = true)) {
                int from, to = 0;

                for (from = 0; form->code[from] && to < SIGNIN_CODE_LEN; from++)
                    if (form->code[from] >= '0' && form->code[from] <= '9')
                        form->code[to++] = form->code[from];
                form->code[to] = '\0';
                complete = to == SIGNIN_CODE_LEN;
            }
            rcSetStyle(s);
        }
        signin_message(error);
    }
    return complete;
}

static inline const char *signin_button_label(const SigninForm *form, bool running) {
    switch (form->page) {
    case SIGNIN_PAGE_SIGN_UP: return running ? "Creating account..." : "Create account";
    case SIGNIN_PAGE_RESET:   return running ? "Sending..."          : "Send reset link";
    case SIGNIN_PAGE_CODE:
        if (running)
            return form->busy == SIGNIN_RESEND ? "Sending a new code..." : "Verifying...";
        return "Verify";
    case SIGNIN_PAGE_SIGN_IN: break;
    }
    return running ? "Signing in..." : "Sign in";
}

/* The filled button, with the spinner while its own request runs. Disabled while any
   request runs, it fades whole, as a loading button does: Tab passes it and a click
   does nothing until the answer comes. */
static inline bool signin_primary(const SigninForm *form) {
    bool light = signin_is_light(rcGetStylePtr()->primary);
    bool busy = form->busy != SIGNIN_NONE;
    bool mine = busy && form->busy != SIGNIN_GOOGLE && form->busy != SIGNIN_APPLE;

    return rcButton("signin_submit", signin_button_label(form, mine), .disabled = busy,
                    .className = light ? "control text-black font-medium"
                                       : "control text-white font-medium",
                    .variant = RC_BTN_PRIMARY, .leadingIcon = mine ? signin_spinner : NULL);
}

/* GOOGLE'S BUTTON TAKES ITS FILL, ITS EDGE AND ITS LABEL FROM GOOGLE'S BRANDING RULES,
   one look for a light page and one for a dark page (theme.h's app_define_classes):
   a default button, whose hairline the look colors, with the hover and the press each
   look names. Disabled while any request runs, it fades whole, its own included, as
   the filled buttons do. */
static inline bool signin_google_button(const SigninForm *form) {
    bool light = signin_is_light(rcGetStylePtr()->surface);
    bool running = form->busy == SIGNIN_GOOGLE;

    return rcButton("signin_google", running ? "Connecting to Google..." : "Continue with Google",
                    .disabled = form->busy != SIGNIN_NONE,
                    .className = light ? "control google-on-light" : "control google-on-dark",
                    .leadingIcon = running ? signin_spinner : signin_google_mark);
}

/* APPLE'S BUTTON is black on a light page and white on a dark one, as its rules
   allow (theme.h's app_define_classes), over a filled button: a class fill stands
   in for the variant's, so disabled while any request runs it fades whole, its own
   included, fill and all. */
#if PLATFORM_SIGN_IN_WITH_APPLE
static inline bool signin_apple_button(const SigninForm *form) {
    bool light = signin_is_light(rcGetStylePtr()->surface);
    bool running = form->busy == SIGNIN_APPLE;

    return rcButton("signin_apple", running ? "Connecting to Apple..." : "Continue with Apple",
                    .disabled = form->busy != SIGNIN_NONE,
                    .className = light ? "control apple-on-light" : "control apple-on-dark",
                    .variant = RC_BTN_PRIMARY,
                    .leadingIcon = running ? signin_spinner : SIGNIN_APPLE_ICON);
}
#endif

static inline SigninChoice signin_providers(const SigninForm *form) {
    SigninChoice choice = SIGNIN_NONE;

    rcColumn(.gap = 12, .w = "grow", .className = "items-stretch") {
        if (signin_google_button(form))
            choice = SIGNIN_GOOGLE;
#if PLATFORM_SIGN_IN_WITH_APPLE
        if (signin_apple_button(form))
            choice = SIGNIN_APPLE;
#endif
    }
    return choice;
}

/* BUTTON OR ENTER: send it, or say what is wrong and put the caret there. */
static inline SigninChoice signin_submit(RC_App *app, SigninForm *f) {
    const char  *bad = NULL;
    SigninChoice send = f->page == SIGNIN_PAGE_SIGN_UP ? SIGNIN_CREATE
                      : f->page == SIGNIN_PAGE_RESET   ? SIGNIN_RESET
                      : f->page == SIGNIN_PAGE_CODE    ? SIGNIN_CODE : SIGNIN_EMAIL;

    f->submitted = true;
    f->banner[0] = '\0';
    if (f->page == SIGNIN_PAGE_CODE) {
        bad = signin_code_error(f) ? SIGNIN_ID_CODE : NULL;
    } else {
        if (f->page != SIGNIN_PAGE_RESET && signin_password_error(f))
            bad = SIGNIN_ID_PASSWORD;
        if (signin_email_error(f))
            bad = SIGNIN_ID_EMAIL;
        if (f->page == SIGNIN_PAGE_SIGN_UP && signin_name_error(f))
            bad = SIGNIN_ID_NAME;
    }
    if (bad) {
        rcSetFocus(bad);
        /* The messages are declared above the button, so this frame drew
           none: ask for the next one rather than wait for an input. */
        rcWindowRequestFrame(rcAppCurrentWindow(app));
        return SIGNIN_NONE;
    }
    f->submitted = false;     /* sent: the next attempt starts clean */
    rcSetFocus(NULL);         /* and a phone puts its keyboard away  */
    return send;
}

/* The page's fields, and its button's two labels. True when a field has sent the
   form by itself. */
static inline bool signin_fields(RC_App *app, SigninForm *form) {
    bool sent = false;

    rcColumn(.gap = FIELD_GAP, .w = "grow", .className = "items-stretch") {
        if (form->page == SIGNIN_PAGE_CODE) {
            sent = signin_code_field(form, signin_code_error(form));
        } else {
            if (form->page == SIGNIN_PAGE_SIGN_UP)
                signin_text_field(form, "signin_name_label", "Name", SIGNIN_ID_NAME, form->name,
                                  (int)sizeof form->name, "Ada Lovelace", RC_INPUT_TEXT,
                                  signin_name_error(form));
            signin_text_field(form, "signin_email_label", "Email", SIGNIN_ID_EMAIL, form->email,
                              (int)sizeof form->email, "name@example.com", RC_INPUT_EMAIL,
                              signin_email_error(form));
            if (form->page != SIGNIN_PAGE_RESET)
                signin_password_field(app, form, signin_password_error(form));
        }
    }
    return sent;
}

/* THE FORM. Every button spans the column: a button is as wide as its label,
   and items-stretch sizes it to the column, as CSS's align-items: stretch
   does. A field already spans it. */
static inline SigninChoice signin_form(RC_App *app, SigninForm *form) {
    SigninChoice choice = SIGNIN_NONE;
    bool         providers = form->page == SIGNIN_PAGE_SIGN_IN || form->page == SIGNIN_PAGE_SIGN_UP;
    bool         off = form->busy != SIGNIN_NONE;
    bool         escaped = rcKeyPressedOn(SIGNIN_ID_EMAIL, RC_KEY_ESCAPE);
    bool         send;

    /* The caret starts in the page's first field, as a page's autofocus does. A
       phone waits for a tap, since this would raise its keyboard over the form,
       except for a code: there the keyboard is what offers the code that arrived. */
    if (!form->focused) {
        form->focused = true;
        if (form->page == SIGNIN_PAGE_CODE)
            rcSetFocus(SIGNIN_ID_CODE);
        else if (!rcPointerIsCoarse())
            rcSetFocus(form->page == SIGNIN_PAGE_SIGN_UP ? SIGNIN_ID_NAME : SIGNIN_ID_EMAIL);
    }
    /* An address is judged when it is left, not on its first letter, and only once
       the pointer is up: its message moves the form, so the press that took the focus
       lands first, on the layout it was aimed at. Esc is a step back, not an answer,
       so it is not judged. */
    if (rcIsFocused(SIGNIN_ID_EMAIL)) {
        form->emailFocused = !escaped;
    } else if (form->emailFocused && !rcPointerDown(RC_POINTER_LEFT)) {
        if (form->email[0] && !escaped)
            form->emailLeft = true;
        form->emailFocused = false;
    }

    if (off) {
        /* Twelfths of a second since the start, in 64 bits: an int overflows in 5.6 years. */
        signin_spin_step = (int)((int64_t)(rcAppTime(app) * 12.0) % 12);
        rcWindowRequestFrameAfter(rcAppCurrentWindow(app), 1.0 / 12.0);
    }

    rcColumn(.gap = SECTION_GAP, .w = "grow", .className = "items-stretch") {
        signin_banner(form);
        if (providers) {
            choice = signin_providers(form);
            signin_divider();
        }
        send = signin_fields(app, form);
        /* Enter in any field is the button, as it submits a page's form, except that
           a good address with its password still to come only moves on to it. */
        if (rcKeyPressedOn(SIGNIN_ID_EMAIL, RC_KEY_ENTER) && form->page != SIGNIN_PAGE_RESET &&
            signin_email_ok(form->email) && !form->password[0])
            rcSetFocus(SIGNIN_ID_PASSWORD);
        else if (rcKeyPressedOn(SIGNIN_ID_NAME, RC_KEY_ENTER) ||
                 rcKeyPressedOn(SIGNIN_ID_EMAIL, RC_KEY_ENTER) ||
                 rcKeyPressedOn(SIGNIN_ID_PASSWORD, RC_KEY_ENTER) ||
                 rcKeyPressedOn(SIGNIN_ID_CODE, RC_KEY_ENTER))
            send = true;
        if (signin_primary(form))
            send = true;
        if (send && !off)
            choice = signin_submit(app, form);
        if (form->page == SIGNIN_PAGE_CODE) {
            rcRow(.gap = 4, .align = "cc", .w = "grow") {
                rcTextL("Didn't get it?", .color = rcGetStyle().textMuted, .size = TEXT_LABEL);
                if (signin_link("signin_resend", "Send a new code", off))
                    choice = SIGNIN_RESEND;
            }
        }
    }
    return choice;
}

/* THE WAY TO THE OTHER PAGE, for the app to set where it wants: "Don't have an
   account?" under the form. */
static inline void signin_switch(RC_App *app, SigninForm *form) {
    RC_Style    s = rcGetStyle();
    bool        off = form->busy != SIGNIN_NONE;
    SigninPage  next = form->page == SIGNIN_PAGE_SIGN_IN ? SIGNIN_PAGE_SIGN_UP
                                                         : SIGNIN_PAGE_SIGN_IN;
    const char *lead = form->page == SIGNIN_PAGE_SIGN_IN ? "Don't have an account?"
                     : form->page == SIGNIN_PAGE_SIGN_UP ? "Already have an account?"
                     : form->page == SIGNIN_PAGE_CODE    ? "Wrong account?"
                                                         : "Remembered it?";
    const char *link = form->page == SIGNIN_PAGE_SIGN_IN ? "Sign up" : "Sign in";

    rcRow(.gap = 4, .align = "cc", .w = "grow") {
        rcTextC(lead, .color = s.textMuted, .size = TEXT_LABEL);
        if (signin_link("signin_switch", link, off))
            signin_set_page(app, form, next);
    }
}

#endif /* APP_SIGNIN_H */
