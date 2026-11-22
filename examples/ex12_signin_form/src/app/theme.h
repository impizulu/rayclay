/*
    theme.h - this app's design tokens

    Every value the UI reads that is a CHOICE rather than a mechanism. Nothing
    here holds state.

    THE PAGE TAKES ITS COLORS FROM THE LOGO: a warm wash from the hat, a cool one
    from the face, and the face's indigo for the one filled button and the links.
    The card is the only bright thing on the page, so the eye lands on the form.
*/
#ifndef APP_THEME_H
#define APP_THEME_H

#include "rayclay.h"

/* The type scale, in CSS pixels. A control's own text is the library's body
   size; labels, links, notes and the title are set here. */
enum { TEXT_TITLE = 24, TEXT_LABEL = 14, TEXT_NOTE = 13 };

/* ONE HEIGHT FOR EVERY FIELD AND BUTTON: 44, Apple's default for a Sign in with
   Apple button and the smallest finger target any platform guideline allows,
   so a mouse and a finger see the same form. A field or a button takes it as the
   "control" class (app_define_classes, below); CONTROL_H sizes what floats over
   a field. */
enum { CONTROL_H = 44, HIT_MIN = 44 };

/* One spacing scale. A label, its field and its message read as ONE field only
   if the gap to the next field is clearly bigger than the gaps inside. */
enum { LABEL_GAP = 8, FIELD_GAP = 16, SECTION_GAP = 24 };

/* THE TWO ARMS. FORM_W is a sign-in form's measure: wide enough for an address,
   narrow enough to read as one column. A viewport that fits the card and air
   around it gets the card. A narrower one, a phone included, makes the form
   the screen, and so does one shorter than CARD_MIN_VH, Material's compact
   height (a phone on its side), where the card's air would cost a field. Both
   arms hold the form to CARD_W. The air at the card's sides, 76 px each, leaves
   the corner button clear of it: the button is CONTROL_H wide and stands 24 px
   in from the edge, which leaves 8 px between it and the card. */
enum { FORM_W = 352, CARD_PAD = 32, CARD_W = FORM_W + 2 * CARD_PAD };
enum { PAGE_AIR = 24, PAGE_SIDE = 24, CARD_MIN_VW = CARD_W + 2 * 76, CARD_MIN_VH = 480 };

/* The two themes. */
static inline RC_Style app_style(bool dark) {
    RC_Style s = dark ? rcStyleDark() : rcStyleLight();

    if (dark) {
        s.background   = rcHex(0x101117);
        s.surface      = rcHex(0x191A22);
        s.surfaceAlt   = rcHex(0x22232C);
        s.text         = RC_ZINC_50;
        s.textMuted    = RC_ZINC_400;      /* 7:1 on the card */
        s.primary      = rcHex(0x8B93E8);  /* 7.4:1 under a black label */
        s.primaryHover = rcHex(0xA0A7EE);
        s.danger       = RC_RED_400;
        s.dangerHover  = RC_RED_300;
        s.success      = RC_EMERALD_400;
        s.warning      = RC_AMBER_400;
        s.border       = rcHex(0x373844);
    } else {
        s.background   = rcHex(0xF3EFEA);
        s.surface      = RC_WHITE;
        s.surfaceAlt   = RC_WHITE;
        s.text         = RC_ZINC_900;
        s.textMuted    = RC_ZINC_600;      /* 7.7:1 on white, 5.5:1 in the card's shadow */
        s.primary      = rcHex(0x4F56B0);  /* 6.4:1 under a white label */
        s.primaryHover = rcHex(0x454C9E);
        s.danger       = RC_RED_600;
        s.dangerHover  = RC_RED_700;
        s.success      = RC_EMERALD_700;   /* 5.5:1 on white */
        s.warning      = RC_AMBER_700;     /* 5.0:1 on white */
        s.border       = RC_ZINC_300;
    }
    s.chrome = s.background;
    return s;
}

/* What RC_Style has no field for: the wash on the page behind the card, and the
   card's shadow. */
typedef struct {
    RC_Gradient pageWash;
    RC_Shadow   cardShadow;
} Palette;

static inline Palette app_palette(bool dark) {
    Palette p;

    p.pageWash.dir = "d";
    p.cardShadow   = RC_LIT(RC_Shadow){ .y = 8.0f, .blur = 32.0f };
    if (dark) {
        p.pageWash.from    = rcHex(0x1D1C33);
        p.pageWash.to      = rcHex(0x0D0E13);
        p.cardShadow.color = rcWithAlpha(RC_BLACK, 0.4f);
    } else {
        p.pageWash.from    = rcHex(0xFBF2E4);
        p.pageWash.to      = rcHex(0xE4E6F6);
        p.cardShadow.color = rcWithAlpha(RC_BLACK, 0.08f);
    }
    return p;
}

/* THE NAMED LOOKS, each a class defined once at start-up, before the first frame.
   "control" is CONTROL_H, h-11. The sign-in buttons wear a brand's colors, one set
   for a light page and one for a dark page: Google's fill, edge and label from its
   branding rules, Apple's black or white from its own, each label in the medium
   weight (font-medium), as the filled button's is. A hover is this app's
   choice, written as the solid color it comes to (Google's: 8% of the label over
   the fill; Apple's: 85% of the fill over the card), and a press darkens the hover
   to 85%, as a filled button's own press does. */
static inline void app_define_classes(void) {
    rcDefineClass("control", "h-11");
    rcDefineClass("google-on-light", "bg-white border-[#747775] text-[#1F1F1F] font-medium "
                                     "hover:bg-[#EDEDED] active:bg-[#C9C9C9] disabled:opacity-50");
    rcDefineClass("google-on-dark", "bg-[#131314] border-[#8E918F] text-[#E3E3E3] font-medium "
                                    "hover:bg-[#242425] active:bg-[#1F1F1F] disabled:opacity-50");
    rcDefineClass("apple-on-light", "bg-black text-white font-medium "
                                    "hover:bg-[#262626] active:bg-[#202020]");
    rcDefineClass("apple-on-dark", "bg-white text-black font-medium "
                                   "hover:bg-[#DDDDDE] active:bg-[#BCBCBD]");
}

#endif /* APP_THEME_H */
