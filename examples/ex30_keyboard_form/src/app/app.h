/*
    app.h - the whole UI: the form model, the layout, and the keyboard

    Eight fields in three sections, a country list and two switches, in one
    scrolling column between a header and an action bar.

    THREE THINGS THE APP OWNS, not the library: sync_focus() raises and dismisses
    the keyboard on the frame focus changes; layout() lets the footer band grow to
    the keyboard's cover, so the "grow" form column ends at its top edge by
    construction; keep_focused_visible() scrolls the focused row back into view
    whenever the column's height moves.

    FIELD_REQUIRED IS THE ONLY STATEMENT OF WHAT IS REQUIRED, and the verdict, the
    header's count and its progress track all count from it.
*/
#ifndef APP_APP_H
#define APP_APP_H

#include "rayclay.h"
#include "app/theme.h"
#include "platform/platform.h"

/* The form model. The order IS the tab order: Next, Previous, Tab and Enter all
   walk it. The country list and the two switches are controls on the form but
   not FIELDS - they hold no text and take no keyboard focus. */
typedef enum {
    FLD_NAME = 0, FLD_EMAIL, FLD_USERNAME, FLD_PASSWORD, FLD_CONFIRM, FLD_PHONE, FLD_CITY, FLD_BIO,
    FLD_COUNT
} Field;

/* Two ids per field: the INPUT that rcSetFocus names, and the ROW around it,
   which is the tap target. Tables, because an id must outlive the frame. */
static const char *const FIELD_ID[FLD_COUNT] = {
    "name", "email", "username", "password", "confirm", "phone", "city", "bio",
};
static const char *const FIELD_ROW[FLD_COUNT] = {
    "row_name", "row_email", "row_username", "row_password", "row_confirm", "row_phone", "row_city",
    "row_bio",
};
static const char *const FIELD_LABEL[FLD_COUNT] = {
    "Full name", "E-mail", "Username", "Password", "Confirm password", "Phone", "City", "Bio",
};
/* A PLACEHOLDER AND ITS HINT MUST NOT SAY THE SAME WORDS: the placeholder shows
   the shape of an answer, the hint states the rule. */
static const char *const FIELD_PLACEHOLDER[FLD_COUNT] = {
    "Ada Lovelace", "ada@example.com", "ada_l", "Choose a password", "Repeat it",
    "+44 20 7946 0000", "London", "A line or two about you",
};
/* The hint under a field when there is nothing to correct. Bio's is a live
   character count composed at the draw, so its slot is empty here. */
static const char *const FIELD_HINT[FLD_COUNT] = {
    "As it should appear on your profile", "We will send a confirmation here",
    "Letters, digits and underscores", "At least 8 characters",
    "Must match the password above",
    "In case we need to verify a sign-in", "Nearest large town is fine", "",
};
/* WHICH FIELDS AN ACCOUNT CANNOT BE MADE WITHOUT, and the only statement of it
   anywhere in the app: flip an entry and every reader of the fact follows. */
static const bool FIELD_REQUIRED[FLD_COUNT] = {
    true, true, true, true, true, false, false, false,
};

/* THE THREE SECTIONS. A field names its section rather than the section naming
   a range, so a field moved in the enum stays under the right heading. */
typedef enum { GRP_ACCOUNT = 0, GRP_SECURITY, GRP_PROFILE, GRP_COUNT } Group;

static const char *const GROUP_TITLE[GRP_COUNT] = { "Account", "Security", "Profile" };
static const Group FIELD_GROUP[FLD_COUNT] = {
    GRP_ACCOUNT, GRP_ACCOUNT, GRP_ACCOUNT, GRP_SECURITY, GRP_SECURITY,
    GRP_PROFILE, GRP_PROFILE, GRP_PROFILE,
};

static const char *const COUNTRY[] = {
    "United Kingdom", "United States", "Germany", "France", "Japan", "Brazil", "India", "Australia",
};
#define N(table) ((int)(sizeof (table) / sizeof (table)[0]))

/* Everything the app knows, in one struct main.c owns as a static. The text
   lives HERE and not in the layout, which is the whole of the rotation story. */
typedef struct {
    char    text[FLD_COUNT][BIO_CAP];   /* one buffer per field; BIO_CAP is the largest */
    int     country;                    /* index into COUNTRY                            */
    bool    subscribe;
    bool    showPassword;               /* unmask both password fields at once           */
    bool    submitted;                  /* Create was pressed: empty required fields now say so */
    bool    created;                    /* a valid submit happened; cleared by the next edit    */
    bool    dismiss;                    /* the app was backgrounded: drop the caret            */
    int     focused;                    /* which field holds focus, -1 none: ONE index         */
    float   columnH;                    /* the form column's height last frame                 */
    uint8_t settle;                     /* frames until the focused row is scrolled into view   */
} AppState;

/* The short fields stop at FIELD_CAP, so a name cannot run to 240 characters. */
static inline int field_cap(Field f)
{
    return f == FLD_BIO ? BIO_CAP : FIELD_CAP;
}

/* Both together: a masked Confirm beside a visible Password is the one
   arrangement nobody can check by eye. */
static inline bool is_secret(Field f)
{
    return f == FLD_PASSWORD || f == FLD_CONFIRM;
}

/* Validation: live, and it never blocks. A form that greys its button out until
   every rule passes leaves the user guessing which rule. "Required" appears only
   after Create: an untouched empty form is not wrong, it is empty. */
typedef enum { V_HINT = 0, V_OK, V_BAD } Verdict;

static inline bool is_word_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static inline bool email_ok(const char *s)
{
    const char *at = NULL, *p;
    for (p = s; *p; p++)
        if (*p == '@') { if (at) return false; at = p; }
    if (!at || at == s || !at[1]) return false;
    for (p = at + 1; *p; p++)
        if (*p == '.' && p > at + 1 && p[1]) return true;
    return false;
}

static inline bool phone_ok(const char *s)
{
    for (; *s; s++)
        if (!((*s >= '0' && *s <= '9') || *s == ' ' || *s == '+')) return false;
    return true;
}

static inline bool username_ok(const char *s)
{
    for (; *s; s++)
        if (!is_word_char(*s)) return false;
    return true;
}

static inline bool str_eq(const char *a, const char *b)
{
    for (; *a && *a == *b; a++, b++) {}
    return *a == *b;
}

/* The verdict and its line, for one field. `m` is the frame arena, because
   rcText DOES NOT COPY: a stack buffer here would draw blank on return. */
static inline Verdict verdict(RC_Arena *m, const AppState *st, Field f, RC_String *line)
{
    const char *t     = st->text[f];
    bool        empty = t[0] == '\0';
    int         n     = 0;

    *line = rcStringFromCStr(FIELD_HINT[f]);
    switch (f) {
    case FLD_NAME:
        break;
    case FLD_EMAIL:
        if (!empty && !email_ok(t)) { *line = rcStringFromCStr("Enter a valid e-mail"); return V_BAD; }
        break;
    case FLD_USERNAME:
        if (!empty && !username_ok(t)) {
            *line = rcStringFromCStr("Only letters, digits and underscores");
            return V_BAD;
        }
        if (!empty && (t[1] == '\0' || t[2] == '\0')) {
            *line = rcStringFromCStr("At least 3 characters");
            return V_BAD;
        }
        break;
    case FLD_PASSWORD:
        for (n = 0; t[n]; n++) {}
        if (!empty && n < 8) { *line = rcStringFromCStr("Too short: 8 characters minimum"); return V_BAD; }
        if (n >= 8)          { *line = rcStringFromCStr("Long enough"); return V_OK; }
        break;
    case FLD_CONFIRM:
        if (!empty && !str_eq(t, st->text[FLD_PASSWORD])) {
            *line = rcStringFromCStr("Passwords do not match");
            return V_BAD;
        }
        if (!empty) { *line = rcStringFromCStr("Passwords match"); return V_OK; }
        break;
    case FLD_PHONE:
        if (!phone_ok(t)) { *line = rcStringFromCStr("Digits, spaces and + only"); return V_BAD; }
        break;
    case FLD_CITY:
        break;
    case FLD_BIO:
        for (n = 0; t[n]; n++) {}
        *line = rcFormat(m, "%d of %d characters", n, BIO_CAP - 1);
        break;
    case FLD_COUNT:
        return V_HINT;      /* the enum's bound, not a field: nothing to index */
    }
    /* The only place an empty field is called wrong, and FIELD_REQUIRED rather
       than the switch decides: an optional field made required is one `true`. */
    if (empty && st->submitted && FIELD_REQUIRED[f]) {
        *line = rcStringFromCStr("Required");
        return V_BAD;
    }
    return V_HINT;
}

static inline int first_bad(RC_Arena *m, const AppState *st)
{
    RC_String unused;
    int       f;
    for (f = 0; f < FLD_COUNT; f++)
        if (verdict(m, st, (Field)f, &unused) == V_BAD) return f;
    return -1;
}

static inline int required_count(void)
{
    int f, n = 0;
    for (f = 0; f < FLD_COUNT; f++)
        if (FIELD_REQUIRED[f]) n++;
    return n;
}

static inline int group_required(Group g)
{
    int f, n = 0;
    for (f = 0; f < FLD_COUNT; f++)
        if (FIELD_GROUP[f] == g && FIELD_REQUIRED[f]) n++;
    return n;
}

/* The header's count and the width of its track. It counts the REQUIRED set
   only: a figure over every field would say Phone, City and Bio stand between
   the user and an account. */
static inline int required_done(RC_Arena *m, const AppState *st)
{
    RC_String unused;
    int       f, n = 0;
    for (f = 0; f < FLD_COUNT; f++)
        if (FIELD_REQUIRED[f] && st->text[f][0] && verdict(m, st, (Field)f, &unused) != V_BAD) n++;
    return n;
}

/* Clamped, not wrapped: Next at the last field does nothing rather than jumping
   back to the top. From no focus, Tab lands first and Shift+Tab last. */
static inline void focus_step(AppState *st, int dir)
{
    int next = st->focused < 0 ? (dir > 0 ? 0 : FLD_COUNT - 1) : st->focused + dir;

    if (next < 0 || next >= FLD_COUNT || next == st->focused) return;
    rcSetFocus(FIELD_ID[next]);
}

/* ONE index for "which field has focus", read back from the library so it is
   never a second copy that can drift. The frame it CHANGES is the keyboard
   event: raise it for a field, dismiss it for none. */
static inline void sync_focus(AppState *st)
{
    int now = -1, f;

    for (f = 0; f < FLD_COUNT; f++)
        if (rcIsFocused(FIELD_ID[f])) now = f;
    if (now == st->focused) return;
    st->focused = now;
    rcSetSoftKeyboardVisible(now >= 0);
    if (now >= 0) st->settle = 2;
}

/* KEEPING THE FOCUSED FIELD ABOVE THE KEYBOARD, from last frame's boxes - the
   only frame rcGetElementBox can see. It runs whenever focus moves and whenever
   the form column's HEIGHT changes, because the height is the one number every
   cause flows into: the keyboard inset arriving, the bar changing face, a
   rotation. The window is the column's box less AIR at both edges, and a positive
   deltaY raises offsetY as in the DOM. A row already inside moves by zero. */
static inline void keep_focused_visible(RC_App *app, AppState *st)
{
    RC_Box        column = rcGetElementBox("form");
    RC_Box        row;
    RC_ScrollInfo si;
    float         top, bottom, dy = 0.0f;

    if (column.found && column.height > 0.0f && column.height != st->columnH) {
        st->columnH = column.height;
        if (st->focused >= 0) st->settle = 2;
    }
    if (st->focused < 0 || !st->settle) return;
    st->settle = (uint8_t)(st->settle - 1);
    rcWindowRequestFrame(rcAppMainWindow(app));
    if (st->settle) return;

    row = rcGetElementBox(FIELD_ROW[st->focused]);
    si  = rcGetScrollInfo("form");
    if (!row.found || row.height <= 0.0f || !column.found || !si.found) return;
    top    = column.y + (float)AIR;
    bottom = column.y + column.height - (float)AIR;
    if (row.y + row.height > bottom) dy = row.y + row.height - bottom;
    if (row.y - dy < top)            dy = row.y - top;
    if (dy != 0.0f) rcScrollBy("form", 0.0f, dy);
}

/* Create: mark the form submitted so empties say "Required", then focus the
   first field needing attention, or declare success and drop focus. */
static inline void submit(RC_Arena *m, AppState *st)
{
    int bad;

    st->submitted = true;
    bad = first_bad(m, st);
    if (bad >= 0) {
        rcSetFocus(FIELD_ID[bad]);
    } else {
        st->created = true;
        rcSetFocus(NULL);
    }
}

static inline void rule(void)
{
    rcBox(.bg = rcGetStyle().border, .w = "grow", .hType = RC_PX(1)) {}
}

/* A Done-bar control: a plain box rather than rcButton, so it fills the bar's
   full height the way a phone's keyboard accessory bar does.
   rcPressed, NOT rcClicked: a text field blurs on the PRESS edge of any press
   outside it, so this bar is gone by the release edge and a click never lands. */
static inline bool bar_button(const char *id, const char *label, bool accent)
{
    RC_Style s = rcGetStyle();

    rcBox(.id = id, .bg = rcAlpha(s.border, rcIsHovered(id) ? 120 : 0), .px = 14, .align = "cc",
          .borderRadius = "all-md", .hType = RC_PX(HIT_MIN)) {
        rcTextC(label, .font = F_BODY, .color = accent ? s.primary : s.text, .wrap = "n");
    }
    return rcPressed(id);
}

/* ONE FIELD: label, input, hint, in a column that is the tap target, the way an
   HTML <label> wrapping an <input> is. That is also what makes the target
   finger-sized: the input's own box is 29 px at 17 px type, under the 44 a finger
   needs. A tap on an already focused input asks for the keyboard again, for the
   phone whose back button dismissed it while the focus stayed. */
static inline void field_row(RC_App *app, AppState *st, Field f)
{
    RC_Style  s   = rcGetStyle();
    RC_Arena *m   = rcAppArena(app);
    RC_String line;
    Verdict   v   = verdict(m, st, f, &line);
    bool      lit = st->focused == (int)f;
    /* "Nothing changed" is the honest value if the body below is ever skipped. */
    bool      changed = false;

    rcColumn(.id = FIELD_ROW[f], .gap = LABEL_GAP, .w = "grow", .hMin = (float)FIELD_MIN_H) {
        /* Full text colour for the label, muted for the hint: a field whose
           name, placeholder and advice share one colour reads as three lines of
           prose, and its placeholder reads as a typed answer. */
        rcTextC(FIELD_LABEL[f], .font = F_SMALL, .color = lit ? s.primary : s.text);
        if (f == FLD_BIO)
            changed = rcTextArea(FIELD_ID[f], st->text[f], field_cap(f),
                                 .placeholder = FIELD_PLACEHOLDER[f], .font = F_INPUT, .rows = 4);
        else
            changed = rcTextInput(FIELD_ID[f], st->text[f], field_cap(f),
                                  .placeholder = FIELD_PLACEHOLDER[f], .font = F_INPUT,
                                  .password = is_secret(f) && !st->showPassword);
        rcText(line, .font = F_SMALL, .color = v == V_BAD ? s.danger : v == V_OK ? VALID_COLOR : s.textMuted);
    }
    if (changed) st->created = false;
    if (rcClicked(FIELD_ROW[f])) {
        if (!rcIsHovered(FIELD_ID[f])) rcSetFocus(FIELD_ID[f]);
        else                           rcSetSoftKeyboardVisible(true);
    }
    /* AND GIVE THE FIELD ITS I-BEAM BACK. A hovered element's ancestors count as
       hovered, so the rcClicked above hints the hand for the whole row, and the
       frame cursor is last-writer-wins. The hand is RIGHT over the label and the
       hint, so narrow the I-beam onto the field rather than dropping the poll. */
    if (rcIsHovered(FIELD_ID[f]))
        rcSetCursor(RC_CURSOR_TEXT);
}

/* A SECTION HEAD. The caption is COUNTED from FIELD_REQUIRED, so a section that
   says "Optional" is one whose fields all are. RayClay has no margin, so the air
   above a block is that block's own padding. */
static inline void section_head(RC_App *app, Group g)
{
    RC_Style  s   = rcGetStyle();
    RC_Arena *m   = rcAppArena(app);
    int       req = group_required(g);

    rcColumn(.gap = LABEL_GAP, .pt = (uint16_t)AIR, .w = "grow") {
        rcRow(.gap = 8, .align = "cl", .w = "grow") {
            rcBox(.w = "grow") {
                rcTextC(GROUP_TITLE[g], .font = F_BODY, .color = s.text);
            }
            if (req > 0)
                rcText(rcFormat(m, "%d required", req), .font = F_MICRO, .color = s.textMuted);
            else
                rcTextL("Optional", .font = F_MICRO, .color = s.textMuted);
        }
        rule();
    }
}

static inline void section_fields(RC_App *app, AppState *st, Group g)
{
    int f;

    for (f = 0; f < FLD_COUNT; f++)
        if (FIELD_GROUP[f] == g) field_row(app, st, (Field)f);
}

/* THE FORM BODY: three sections, each its own head then its fields then the
   controls that belong with them. The primary action is NOT here - it lives in
   the footer, where it is on screen without scrolling to it. */
static inline void form_body(RC_App *app, AppState *st)
{
    RC_Style s = rcGetStyle();
    /* Whether a switch took the tap itself; each row writes it before testing it. */
    bool     flipped = false;

    rcTextL("Takes a minute. Nothing here is sent anywhere.",
            .font = F_SMALL, .color = s.textMuted);

    section_head(app, GRP_ACCOUNT);
    section_fields(app, st, GRP_ACCOUNT);
    rcColumn(.id = "row_country", .gap = LABEL_GAP, .w = "grow", .hMin = (float)FIELD_MIN_H) {
        rcTextL("Country", .font = F_SMALL, .color = s.text);
        rcCombo("country", &st->country, COUNTRY, N(COUNTRY));
        rcTextL("Where the account is registered", .font = F_SMALL, .color = s.textMuted);
    }

    section_head(app, GRP_SECURITY);
    section_fields(app, st, GRP_SECURITY);
    /* THE WIDGET'S OWN RETURN IS WHAT SAYS "ALREADY HANDLED". The row is
       clickable too, so one tap must be counted once whichever took it. Asking
       instead whether the pointer is over the box looks right and is not: with
       that guard a tap landing ON the box changes nothing while one on the row's
       empty half works, so the control is dead where the user aims it. */
    rcRow(.id = "row_showpw", .gap = 12, .align = "cl", .w = "grow", .hMin = (float)FIELD_MIN_H) {
        flipped = rcCheckbox("showpw", "Show password", &st->showPassword);
    }
    if (!flipped && rcClicked("row_showpw")) st->showPassword = !st->showPassword;

    section_head(app, GRP_PROFILE);
    section_fields(app, st, GRP_PROFILE);

    rcRow(.id = "row_subscribe", .gap = 12, .align = "cl", .w = "grow", .hMin = (float)FIELD_MIN_H) {
        rcColumn(.gap = 2, .w = "grow") {
            rcTextL("Subscribe to updates", .font = F_BODY, .color = s.text);
            rcTextL("Product news, about once a month", .font = F_SMALL, .color = s.textMuted);
        }
        flipped = rcToggle("subscribe", &st->subscribe);
    }
    if (!flipped && rcClicked("row_subscribe")) st->subscribe = !st->subscribe;
}

/* THE HEADER BAND: the top safe inset painted with the band tint, the title row,
   and a track that fills as the required fields are answered. The surface spans
   the window and only the CONTENT is inset by the sides. */
static inline void header(RC_App *app, const AppState *st, RC_Insets safe)
{
    RC_Style  s    = rcGetStyle();
    RC_Arena *m    = rcAppArena(app);
    int       req  = required_count();
    int       done = required_done(m, st);
    bool      all  = done >= req;

    rcColumn(.id = "header", .bg = s.surface, .w = "grow") {
        rcBox(.id = "top_band", .bg = rcAlpha(s.primary, BAND_ALPHA), .w = "grow",
              .hType = RC_PX(safe.top)) {}
        rcRow(.pl = (uint16_t)(safe.left + (float)PAD), .pr = (uint16_t)(safe.right + (float)PAD),
              .align = "cc", .w = "grow", .hType = RC_PX(HEADER_H)) {
            rcRow(.gap = 12, .align = "cl", .w = "grow", .wMax = (float)FORM_MAX_W) {
                rcBox(.w = "grow") {
                    rcTextL("Create your account", .font = F_TITLE, .color = s.text);
                }
                rcText(rcFormat(m, "%d of %d required", done, req),
                       .font = F_SMALL, .color = all ? VALID_COLOR : s.textMuted);
            }
        }
        /* The track is the band's hairline as well as its progress. A percent
           needs a definite basis, which the "grow" track gives it. */
        rcRow(.bg = s.border, .w = "grow", .hType = RC_PX(PROGRESS_H)) {
            rcBox(.bg = all ? VALID_COLOR : s.primary, .h = "grow",
                  .wType = RC_PCT(req > 0 ? (float)done * 100.0f / (float)req : 0.0f)) {}
        }
    }
}

/* THE FOOTER BAND, and it is on screen at every moment. While a field is focused
   it is the keyboard's accessory bar; otherwise it is the form's primary action,
   which on a phone belongs at the bottom of the SCREEN and not at the bottom of a
   column the reader has to travel to. Under either sits a box exactly as tall as
   whatever covers the window's bottom edge - the home indicator, or the keyboard,
   which covers the indicator too. That box IS the keyboard avoidance. */
static inline void footer(RC_App *app, AppState *st, RC_Insets safe, float kb)
{
    RC_Style  s      = rcGetStyle();
    RC_Arena *m      = rcAppArena(app);
    bool      typing = st->focused >= 0 || kb > 0.0f;
    float     cover  = kb > safe.bottom ? kb : safe.bottom;
    int       bad    = st->submitted ? first_bad(m, st) : -1;

    rcColumn(.id = "footer", .bg = s.surface, .w = "grow") {
        rule();
        /* The verdict on the whole form, above BOTH faces of the bar: pressing
           Create raises the keyboard, and must not take its answer away. */
        if (st->created || bad >= 0) {
            rcRow(.pt = 10, .pl = (uint16_t)(safe.left + (float)PAD),
                  .pr = (uint16_t)(safe.right + (float)PAD), .align = "cc", .w = "grow") {
                rcBox(.w = "grow", .wMax = (float)FORM_MAX_W) {
                    if (st->created)
                        rcText(rcFormat(m, "Account created for %s in %s%s.", st->text[FLD_NAME],
                                        COUNTRY[st->country],
                                        st->subscribe ? ", subscribed to updates" : ""),
                               .font = F_SMALL, .color = VALID_COLOR);
                    else
                        rcText(rcFormat(m, "Check %s, and any other field marked in red.",
                                        FIELD_LABEL[bad]),
                               .font = F_SMALL, .color = s.danger);
                }
            }
        }
        if (typing) {
            rcRow(.pl = (uint16_t)(safe.left + 8.0f), .pr = (uint16_t)(safe.right + 8.0f),
                  .align = "cc", .w = "grow", .hType = RC_PX(DONE_BAR_H)) {
                rcRow(.id = "donebar", .gap = 4, .align = "cl", .w = "grow", .wMax = (float)FORM_MAX_W) {
                    if (bar_button("kb_prev", "Previous", false)) focus_step(st, -1);
                    if (bar_button("kb_next", "Next", false))     focus_step(st, +1);
                    rcBox(.align = "cc", .w = "grow") {
                        rcText(rcFormat(m, "keyboard %d", (int)(kb + 0.5f)), .font = F_SMALL, .color = s.textMuted);
                    }
                    if (bar_button("kb_done", "Done", true)) {
                        rcSetSoftKeyboardVisible(false);
                        rcSetFocus(NULL);
                    }
                }
            }
        } else {
            rcRow(.pl = (uint16_t)(safe.left + (float)PAD), .pr = (uint16_t)(safe.right + (float)PAD),
                  .py = 10, .align = "cc", .w = "grow") {
                rcBox(.id = "create", .bg = rcIsHovered("create") ? s.primaryHover : s.primary,
                      .align = "cc", .borderRadius = "all-md", .w = "grow",
                      .hType = RC_PX(48), .wMax = (float)FORM_MAX_W) {
                    rcTextL("Create account", .font = F_BODY, .color = RC_WHITE);
                }
                if (rcClicked("create")) submit(m, st);
            }
        }
        rcBox(.id = "bottom_band", .bg = rcAlpha(s.primary, BAND_ALPHA), .w = "grow",
              .hType = RC_PX(cover)) {}
    }
}

/* Hardware keys: Tab and Shift+Tab walk the fields, Enter in a single-line field
   is Next (Bio is a text area, where Enter is a newline). Escape needs nothing -
   the editor blurs itself, and on Android the back button does the same. */
static inline void update(RC_App *app, void *userData)
{
    AppState *st = (AppState *)userData;
    (void)app;

    if (st->dismiss) { st->dismiss = false; rcSetFocus(NULL); }
    if (rcKeyPressed(RC_KEY_TAB))
        focus_step(st, rcModDown(RC_MOD_SHIFT) ? -1 : +1);
    if (rcKeyPressed(RC_KEY_ENTER) && st->focused >= 0 && st->focused != FLD_BIO)
        focus_step(st, +1);
}

static inline void layout(RC_App *app, void *userData)
{
    AppState   *st = (AppState *)userData;
    RC_Style    s  = rcGetStyle();
    RC_Viewport vp = rcViewport();
    RC_Box      caret;
    bool        card = vp.width >= (float)FORM_CARD_W;

    /* THE SAFE AREA IS SPENT ONCE, AND BY WHOEVER SITS ON IT: the header takes
       the top inset, the footer the bottom one or the keyboard, the root
       nothing. */
    rcColumn(.id = "root", .bg = s.background, .w = "grow", .h = "grow") {
        header(app, st, vp.safe);
        rcColumn(.id = "form", .pt = (uint16_t)(card ? AIR * 2 : 0),
                 .pb = (uint16_t)(card ? AIR * 2 : 0),
                 .pl = (uint16_t)vp.safe.left, .pr = (uint16_t)vp.safe.right,
                 .align = "tc", .scroll = "v", .w = "grow", .h = "grow") {
            /* THE WIDE ARM IS A CARD, the narrow arm is the screen - one
               condition, read from the SPACE and never from the OS. The border is
               always declared and only its COLOUR changes, so nothing moves by a
               pixel at the breakpoint.
               THE DESIGNATORS RUN IN DECLARATION ORDER because C++ requires it
               and C99 does not, and every example here is compiled as both. */
            rcColumn(.bg = card ? s.surface : RC_TRANSPARENT,
                     .gap = FIELD_GAP, .p = (uint16_t)(card ? PAD * 2 : PAD),
                     .borderRadius = "all-xl",
                     .border = { .color = card ? s.border : RC_TRANSPARENT,
                                 .width = "1px" },
                     .w = "grow", .wMax = (float)FORM_MAX_W) {
                form_body(app, st);
            }
        }
        footer(app, st, vp.safe, vp.keyboard.bottom);
    }

    /* After the fields: which holds focus now, the keyboard event if that
       changed, the scroll that keeps it visible, then the caret rect. rcScrollbar
       comes last, after the rcScrollBy on the same container, which is the order
       rayclay.h requires. */
    sync_focus(st);
    keep_focused_visible(app, st);
    if (st->focused >= 0) {
        caret = rcGetElementBox(FIELD_ID[st->focused]);
        if (caret.found && caret.height > 0.0f)
            rcSetImeCaretRect(caret.x, caret.y, caret.width, caret.height);
    }
    rcScrollbar("form");
}

#endif /* APP_APP_H */
