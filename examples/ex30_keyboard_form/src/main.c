/*
 *  ex30 - RayClay Sign-up   |   the soft keyboard, end to end, one TU
 *
 *  A phone sign-up: eight fields under three section heads, a country list and
 *  two switches, in one scrolling column between a header on the status bar and
 *  an action bar on the home indicator. Focus a field and the app raises the
 *  soft keyboard, the column shrinks to the space above it, the field scrolls
 *  into that space, and a Previous / Next / Done bar takes the action bar's
 *  place on the keyboard's top edge. It opens at 393x852 and is locked to
 *  portrait on a device; a wider window shows the same form as a 560 px card.
 *
 *  Shows: rcTextInput, rcTextArea, rcCombo, rcCheckbox, rcToggle, rcScrollBy,
 *  rcSetSoftKeyboardVisible, rcSetImeCaretRect, rcViewport().keyboard / .safe,
 *  rcSetAppEventHandler.
 *
 *  Build target: rayclay_ex30_keyboard_form
 */
#include "app/app.h"

/* Backgrounding is the one moment a phone app is told anything. A form left
   with the keyboard up should not come back with it up, so this asks for the
   caret to be dropped and update() does it on the next frame: nothing may draw
   from here. */
static void on_app_event(RC_AppEvent event, void *user)
{
    if (event == RC_APP_EVENT_SUSPENDED) ((AppState *)user)->dismiss = true;
}

int main(void)
{
    static AppState state;
    float sizes[F_COUNT];

    sizes[F_BODY]  = 15.0f; sizes[F_MICRO] = 11.0f; sizes[F_SMALL] = 13.0f;
    sizes[F_INPUT] = 17.0f; sizes[F_TITLE] = 22.0f;
    /* A LIGHT FORM, which is what a sign-up is on both phone platforms: the page
       is paper, the fields are the only thing with weight on it, and the one
       saturated colour in the frame is the button you are meant to press. The
       preset already carries the three signal colours this form needs. */
    rcSetStyle(rcStyleLight());
    state.focused = -1;
    rcSetAppEventHandler(on_app_event, &state);

    RC_AppOptions opts = {
        .width = 393, .height = 852, .title = "RayClay Sign-up",
        .clearColor = rcGetStyle().background,
        .fontSizes = sizes, .fontCount = F_COUNT,
        .scratchArenaBytes = 32768,          /* backs every rcFormat in a frame */
        .updateCallback = update, .layoutCallback = layout, .userData = &state,
        .renderMode = RC_RENDER_ON_DEMAND,   /* parks between taps and blinks  */
        /* A phone app that is portrait-first says so; on a device this can only
           NARROW the set the Info.plist / manifest allows. */
        .orientation = RC_ORIENTATION_PORTRAIT,
    };
    return rcRunApp(&opts);
}
