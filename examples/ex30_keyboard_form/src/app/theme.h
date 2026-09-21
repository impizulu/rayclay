/*
    theme.h - this app's design tokens

    Every value the UI reads that is a CHOICE rather than a mechanism. Nothing
    here calls RayClay and nothing here holds state.

    There is almost no palette on purpose: every surface, text and accent colour
    comes from rcGetStyle() at the moment it is drawn, so one rcSetStyle() in
    main.c restyles the whole form.

    A COLOUR MUST BE A #define, NEVER A static const. rcRgb / rcRgba expand to
    compound literals, which are not constant expressions at file scope in C99.
*/
#ifndef APP_THEME_H
#define APP_THEME_H

#include "rayclay.h"

/* The type ramp: five of the sixteen slots RayClay offers.
   SLOT 0 IS THE BODY, and that is a contract rather than a preference: the
   bundled widgets draw their text in slot 0, so an app that puts its 11 px micro
   face there gets 11 px buttons. F_INPUT is what the fields type in, and a
   field's box takes its height from it. */
enum { F_BODY = 0, F_MICRO, F_SMALL, F_INPUT, F_TITLE, F_COUNT };

/* The two bands. The header sits on the top safe inset and the footer on the
   bottom one or on the keyboard, whichever is taller; each adds its inset to its
   own height, so the root spends nothing. */
enum { HEADER_H = 56, DONE_BAR_H = 44, PROGRESS_H = 3 };

/* Finger sizes. 44 is Apple's floor and Material's is 48. An input's own box is
   29 px at 17 px type, under either, which is why the ROW is the target. */
enum { FIELD_MIN_H = 44, HIT_MIN = 44 };

/* One spacing scale, and FIELD_GAP is deliberately far larger than LABEL_GAP: a
   label, its field and its hint only read as ONE field if the gap to the NEXT
   field is clearly bigger than the gaps inside this one. AIR is what the scroll
   keeper leaves between a focused field and the edge of the visible column. */
enum { PAD = 16, LABEL_GAP = 4, FIELD_GAP = 26, AIR = 12 };

/* A reading measure. Below FORM_CARD_W the form is the screen, as a phone
   sign-up is; at or above it the same fields become a centred card. Same ids,
   same fields, same order on both arms - only the FRAME changes. */
enum { FORM_MAX_W = 560, FORM_CARD_W = 720 };

/* BIO_CAP is the 240 the counter under the text area reports against, plus NUL. */
enum { FIELD_CAP = 64, BIO_CAP = 241 };

/* The tint of the safe-area bands, so the insets they spend are visible. A
   shipping app would leave both bands the plain surface colour. */
#define BAND_ALPHA 38   /* 0.15 * 255 */

/* What a hint turns when its field is right, and the header's counter when
   nothing required is outstanding. The theme's success colour is a BUTTON
   colour; as 13 px of text on a near-white page it wants the shade down. */
#define VALID_COLOR RC_EMERALD_700

#endif /* APP_THEME_H */
