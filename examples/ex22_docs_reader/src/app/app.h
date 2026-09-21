/*
    app.h - the whole UI: state, and the layout that renders it.

    ONE header, no .c beside it: main.c is the only translation unit, so there is
    nothing to link and no forward declarations. A visual CHOICE is in theme.h,
    the document in document.h, the block renderer in blocks.h.

    THE LAYOUT HAS TWO ARMS AND THEY ARE NOT THE SAME SHAPE. Wide: a contents
    rail beside the article, both on screen. Narrow: ONE pane at a time, a list
    and then the article it opens, with a back control - which is why the narrow
    arm is a navigation state (AppState.view) and not a squeezed copy.
*/
#ifndef APP_APP_H
#define APP_APP_H

#include "rayclay.h"
#include "app/theme.h"
#include "app/document.h"
#include "app/blocks.h"
#include "platform/platform.h"

#include "icons/rc_icons_chevron_right.h"
#include "icons/rc_icons_arrow_left.h"
#include "icons/rc_icons_search.h"

/* The narrow arm's pane. The wide arm shows both and never reads it, so it is a
   state the LAYOUT consults rather than a mode the app is in. */
typedef enum {
    READER_VIEW_CONTENTS = 0,   /* the list. Where a phone opens.            */
    READER_VIEW_ARTICLE         /* the document itself                       */
} ReaderView;

typedef struct {
    ReaderFonts   fonts;
    bool          ready;
    bool          sawFontError; /* set by the log sink during registration     */
    bool          dark;         /* false = paper, which is what a reader is    */
    bool          themeReady;   /* has any palette reached rcSetStyle yet      */
    bool          themeInStyle; /* which arm the style layer is holding        */
    int           step;         /* index into type_steps                       */
    ReaderView    view;
    char          search[48];   /* the search field's buffer, ours by contract */
    const char   *pending;      /* a section to reach once the article exists  */
    const char   *active;       /* the section on screen, marked in the contents */
    bool          restore;      /* put the article back where the reader left it */
    float         pageOffsetY;  /* where that was                              */
    float         progress;     /* how far through the document, 0..1          */
    unsigned long framesDrawn;  /* the render witness - see main()             */
} AppState;

static AppState state;

/* Counts PRESENTED frames: the runner skips this for an idle frame, which is
   what makes the count in main() a render witness. */
static inline void frame_end(RC_App *app, void *userData)
{
    AppState *st = (AppState *)userData;

    (void)app;
    st->framesDrawn++;
}

/* THE LEVEL IS THE VERDICT, NOT THE WORDING. Within a font call an ERROR means
   the load was REFUSED and you hold the default slot; a WARNING means the face
   loaded with something worth knowing. The sentences are open-ended. */
static inline void reader_log(RC_LogLevel level, const char *msg, void *user)
{
    (void)msg;
    (void)user;
    if (level == RC_LOG_ERROR)
        state.sawFontError = true;
}

/* ONE FILE PER WEIGHT: point both weights at one file and both calls succeed,
   return different ids, and render identical outlines. */
static inline void reader_load_fonts(AppState *st)
{
    ReaderFonts *f = &st->fonts;

    st->sawFontError = false;
    f->body = rcRegisterFont("Lato", RC_WEIGHT_REGULAR, FONT_BODY_TTF, BAKE_BODY);

    /* A 0 return cannot be the test: 0 is both the default slot and every
       failure, so the log sink is the oracle. */
    if (st->sawFontError) {
        f->custom = false;
        f->h1 = f->h2 = f->body = f->bold = f->small = f->code = 0;
        f->note = "bundled face - launch from the repository root to load Lato";
        return;
    }

    f->custom = true;

    rcRegisterFont("Lato", RC_WEIGHT_BOLD,    FONT_BOLD_TTF, BAKE_H1);
    rcRegisterFont("Lato", RC_WEIGHT_BOLD,    FONT_BOLD_TTF, BAKE_H2);
    rcRegisterFont("Lato", RC_WEIGHT_BOLD,    FONT_BOLD_TTF, BAKE_BODY);
    rcRegisterFont("Lato", RC_WEIGHT_REGULAR, FONT_BODY_TTF, BAKE_SMALL);
    rcRegisterFont("Mono", RC_WEIGHT_REGULAR, FONT_MONO_TTF, BAKE_CODE);

    /* Registration knows about FILES; everything after it asks for a (family,
       weight, size). Resolved once here rather than per text run. */
    f->h1    = rcFont("Lato", RC_WEIGHT_BOLD,    BAKE_H1);
    f->h2    = rcFont("Lato", RC_WEIGHT_BOLD,    BAKE_H2);
    f->bold  = rcFont("Lato", RC_WEIGHT_BOLD,    BAKE_BODY);
    f->body  = rcFont("Lato", RC_WEIGHT_REGULAR, BAKE_BODY);
    f->small = rcFont("Lato", RC_WEIGHT_REGULAR, BAKE_SMALL);
    f->code  = rcFont("Mono", RC_WEIGHT_REGULAR, BAKE_CODE);
    f->note  = "Lato + Adwaita Mono - 6 of 16 font slots";
}

static inline const ReaderPalette *reader_palette(const AppState *st)
{
    return st->dark ? &reader_dark_palette : &reader_paper_palette;
}

/* Every library widget reads RC_Style, so the palette has to reach it or the
   search field stays dark on a paper page. Called before any element is
   declared, because rcSetStyle takes effect at the NEXT widget. */
static inline void reader_apply_theme(AppState *st)
{
    const ReaderPalette *pal = reader_palette(st);
    RC_Style             s   = st->dark ? rcStyleDark() : rcStyleLight();

    s.background   = pal->paper;
    s.surface      = pal->raised;
    /* The track under rcProgress and rcScrollbar: the hairline colour, so an
       empty bar reads as the rule it sits on. */
    s.surfaceAlt   = pal->border;
    s.chrome       = pal->paperAlt;
    s.text         = pal->ink;
    s.textMuted    = pal->inkMuted;
    s.primary      = pal->accent;
    s.primaryHover = pal->accentHover;
    s.border       = pal->border;
    s.radius       = 6.0f;
    rcSetStyle(s);
    st->themeReady   = true;
    st->themeInStyle = st->dark;
}

/* One run of text in one face at one size: the same rcMeasureText the layout
   engine wraps with, asked directly. RC_LIT keeps the options literal compiling
   as C99 AND as C++. */
static inline float reader_run_px(uint16_t fontId, uint16_t size,
                                  const char *chars, int32_t length)
{
    RC_TextElementConfig cfg = rcBuildTextConfig(RC_LIT(RC_TextOptions){
                                   ._reserved = 0, .font = fontId, .size = size });
    RC_StringSlice       slice;
    RC_Dimensions        dim;

    slice.chars     = chars;
    slice.baseChars = chars;
    slice.length    = length;

    dim = rcMeasureText(slice, &cfg, NULL);
    return dim.width > 0.0f ? dim.width : 0.0f;
}

static inline float reader_text_px(uint16_t fontId, uint16_t size, RC_String text)
{
    return reader_run_px(fontId, size, text.chars, text.length);
}

/* The average advance in the body face - a proportional face has no single
   character width, which is why the readout says "~". It takes the SIZE because
   a bigger step must buy a wider column to hold the same count. */
static inline float reader_avg_char_px(uint16_t fontId, uint16_t size)
{
    static const char sample[] = "abcdefghijklmnopqrstuvwxyz etaoinshrdlu";
    const int         n        = (int)(sizeof sample - 1);   /* NUL not measured */
    float             avg      = reader_text_px(fontId, size, rcStringFromCStr(sample)) / (float)n;

    return avg > 0.0f ? avg : 0.0f;
}

/* The widest LINE of any listing, plus the panel's padding and hairlines: the
   width a code block needs to show every line whole. Measured in the face and at
   the step it is drawn in, so it follows the text-size control. */
static inline float reader_code_px(const ReaderFonts *f, const TypeStep *sz)
{
    float widest = 0.0f;
    int   i;

    for (i = 0; i < DOC_BLOCK_COUNT; i++) {
        const char *p = doc_blocks[i].text;

        if (doc_blocks[i].kind != DOC_CODE)
            continue;
        while (*p) {
            const char *nl = p;
            float       w;

            while (*nl && *nl != '\n')
                nl++;
            w = reader_run_px(f->code, sz->code, p, (int32_t)(nl - p));
            if (w > widest)
                widest = w;
            p = *nl ? nl + 1 : nl;
        }
    }
    return widest > 0.0f ? widest + 2.0f * (float)CODE_PAD_PX + 2.0f : 0.0f;
}

static inline int reader_measure_chars(float columnPx, float avgCharPx)
{
    if (avgCharPx <= 0.0f)
        return 0;
    return (int)(columnPx / avgCharPx);
}

static inline RC_Color reader_measure_color(const ReaderPalette *pal, int chars)
{
    if (chars >= 45 && chars <= 75)
        return pal->accent;               /* comfortable                     */
    if (chars > 0)
        return pal->warn;                 /* readable, but outside the range */
    return pal->inkMuted;
}

/* ASCII case folding by hand: tolower() is locale-dependent, which a search
   over an English document does not want. */
static inline char reader_fold(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

static inline bool reader_icontains(const char *hay, const char *needle)
{
    const char *h;

    for (h = hay; *h; h++) {
        const char *a = h;
        const char *b = needle;
        while (*b && reader_fold(*a) == reader_fold(*b)) {
            a++;
            b++;
        }
        if (!*b)
            return true;
    }
    return false;
}

/* A section matches on its TITLE or any of its BODY, because "arena" is a word
   a reader remembers from the prose. A section owns the blocks that follow it. */
static inline bool reader_section_matches(int index, const char *query)
{
    int i;

    if (!query || query[0] == '\0')
        return true;
    if (reader_icontains(doc_blocks[index].text, query))
        return true;
    for (i = index + 1; i < DOC_BLOCK_COUNT && doc_blocks[i].kind != DOC_H2; i++) {
        if (reader_icontains(doc_blocks[i].text, query))
            return true;
    }
    return false;
}

/* What one chip will measure, so the toolbar decides whether its controls fit
   by MEASURING them rather than by picking a device width. */
static inline float reader_chip_px(const AppState *st, const char *label, uint16_t textPx)
{
    return reader_text_px(st->fonts.bold, textPx, rcStringFromCStr(label)) + 2.0f * CHIP_PAD_X;
}

/* One toolbar chip: a pill that carries its state in its fill. Polling rcClicked
   is what makes it a BUTTON to the library, and what hints the pointer cursor. */
static inline bool reader_chip(const AppState *st, const ReaderPalette *pal, const char *id,
                               const char *label, uint16_t textPx, bool active, bool segmented)
{
    bool     hover = rcIsHovered(id);
    RC_Color fill  = active ? pal->accent : (hover ? pal->hover : RC_TRANSPARENT);
    RC_Color ink   = active ? pal->accentInk : pal->ink;

    /* SEGMENTED means "exactly one of these is chosen", and colour alone is the
       channel a colour-blind reader, a dimmed screen and a photographed frame
       all lose first - so the chosen chip also takes a ring, in `ink` so it
       reads against the accent fill in both themes. `segmented` is its own
       argument because an ACTION chip passes active=true for the fill alone.
       The ring is declared in both states, transparent when unchosen, so widths
       never move under the pointer. */
    RC_Color ring = (segmented && active) ? pal->ink : RC_TRANSPARENT;

    rcRow(.id = id, .bg = fill, .pl = CHIP_PAD_X, .pr = CHIP_PAD_X, .align = "cc",
          .borderRadius = "all-full", .border = { .color = ring, .width = "all-1" },
          .h = "fit", .hMin = CHIP_H) {
        rcTextC(label, .font = st->fonts.bold, .color = ink, .size = textPx);
    }
    return rcClicked(id);
}

/* The text steps, shown as the same letter at the sizes they select: a control
   that spells its options out costs three times the width and says less. */
static inline void reader_size_group(AppState *st, const ReaderPalette *pal)
{
    static const char *const sizeIds[TYPE_STEP_COUNT] = { "size0", "size1", "size2" };
    int i;

    rcRow(.id = "sizegrp", .bg = pal->paperAlt, .gap = 2, .p = 3, .align = "cc",
          .borderRadius = "all-full", .h = "fit", .tooltip = "Text size") {
        for (i = 0; i < TYPE_STEP_COUNT; i++) {
            if (reader_chip(st, pal, sizeIds[i], "A", size_chip_px[i], st->step == i, true))
                st->step = i;
        }
    }
}

static inline void reader_theme_group(AppState *st, const ReaderPalette *pal)
{
    rcRow(.id = "themegrp", .bg = pal->paperAlt, .gap = 2, .p = 3, .align = "cc",
          .borderRadius = "all-full", .h = "fit", .tooltip = "Theme") {
        if (reader_chip(st, pal, "th_paper", "Paper", CHIP_TEXT_PX, !st->dark, true))
            st->dark = false;
        if (reader_chip(st, pal, "th_dark", "Dark", CHIP_TEXT_PX, st->dark, true))
            st->dark = true;
    }
}

/* The search field. rcTextInput draws its own themed box - fill, border, focus
   ring - and takes no artwork, so the glyph sits beside it rather than being
   faked on top. Returns true on a frame the query changed. */
static inline bool reader_search(AppState *st, const ReaderPalette *pal, float widthPx)
{
    bool changed = false;

    rcRow(.gap = 8, .align = "cc", .h = "fit",
          .wType = widthPx > 0.0f ? RC_PX(widthPx) : RC_GROW) {
        rcIconSearch(15.0f, pal->inkMuted);
        changed = rcTextInput("search", st->search, sizeof st->search,
                              .placeholder = "Search sections", .font = st->fonts.small);
    }
    return changed;
}

/* One contents entry. EXACTLY ONE is the section being read and only that one
   takes the accent; the marker rail is DRAWN ON EVERY ENTRY and merely
   transparent otherwise, so the list never shifts sideways as the reader passes
   a heading. A hand-built row rather than rcButton, which has no height to set
   while a finger needs TOC_ROW_MIN. rcClicked keeps the pointer cursor. */
static inline bool reader_toc_entry(AppState *st, const ReaderPalette *pal, const char *id,
                                    const char *label, uint16_t textPx, bool current)
{
    rcRow(.id = id, .bg = rcIsHovered(id) ? pal->hover : RC_TRANSPARENT, .gap = 6,
          .pl = 6, .pr = 10, .align = "cl", .borderRadius = "all-md", .w = "grow", .h = "fit",
          .hMin = TOC_ROW_MIN) {
        rcBox(.bg = current ? pal->accent : RC_TRANSPARENT, .borderRadius = "all-full",
              .w = "3", .hType = RC_PX(18.0f)) {}
        rcIconChevronRight(16.0f, pal->accent);
        rcBox(.w = "grow", .h = "fit") {
            rcTextC(label, .font = st->fonts.body,
                    .color = current ? pal->accent : pal->ink, .size = textPx);
        }
    }
    return rcClicked(id);
}

/* The contents list: the label and one entry per MATCHING section. The SAME list
   in every arm, so a section appears in both places or in neither. */
static inline void reader_toc_buttons(RC_App *app, AppState *st, const ReaderPalette *pal,
                                      const TypeStep *sz)
{
    int       i, total = 0, shown = 0;
    RC_String label;

    for (i = 0; i < DOC_BLOCK_COUNT; i++) {
        if (doc_blocks[i].kind != DOC_H2)
            continue;
        total++;
        if (reader_section_matches(i, st->search))
            shown++;
    }

    /* The label doubles as the result count: a filtered list that does not say
       it is filtered is a list with sections missing from it. */
    label = st->search[0] == '\0'
          ? rcStringFromCStr("CONTENTS")
          : rcFormat(rcAppArena(app), "%d OF %d SECTIONS", shown, total);

    rcBox(.pb = 6, .w = "grow", .h = "fit") {
        rcText(label, .font = st->fonts.small, .color = pal->inkMuted,
               .size = 11, .letterSpacing = 1);
    }
    for (i = 0; i < DOC_BLOCK_COUNT; i++) {
        const char *btnId;
        if (doc_blocks[i].kind != DOC_H2 || !reader_section_matches(i, st->search))
            continue;
        /* The entry needs an id of its OWN. The document id belongs to the
           HEADING, and declaring it here too is a duplicate: only the first
           element keeps the id, so the seek would find this entry rather than
           the section it is aimed at. */
        btnId = rcFormat(rcAppArena(app), "toc_%s", doc_blocks[i].id).chars;
        if (reader_toc_entry(st, pal, btnId, doc_blocks[i].text, sz->small,
                             doc_blocks[i].id == st->active)) {
            /* RECORDED, not acted on: neither the rail nor the pane knows
               whether the article is on screen yet. reader_seek owns the
               scroll. */
            st->pending = doc_blocks[i].id;
            st->restore = false;
            st->view    = READER_VIEW_ARTICLE;
            rcWindowRequestFrame(rcAppMainWindow(app));
        }
    }
    if (shown == 0) {
        rcBox(.pt = 4, .pb = 8, .pl = 6, .w = "grow", .h = "fit") {
            rcTextL("No section matches that search.", .font = st->fonts.body,
                    .color = pal->inkMuted, .size = sz->small);
        }
    }
}

/* The wide arm: a side rail that scrolls on its own, beside the page. */
static inline void reader_toc(RC_App *app, AppState *st, const ReaderPalette *pal,
                              const TypeStep *sz)
{
    rcColumn(.id = "toc", .bg = pal->paperAlt, .gap = 2, .p = 16, .scroll = "v",
             .h = "grow", .wType = RC_PX(TOC_W_PX)) {
        reader_toc_buttons(app, st, pal, sz);
    }
    rcScrollbar("toc");
    /* The rule between rail and page is its own element: RC_Border carries one
       all-sides width, so a bordered rail would box itself in on three edges. */
    rcBox(.bg = pal->border, .w = "1", .h = "grow") {}
}

/* The narrow arm's FIRST pane: the document's title page and its contents. The
   title and the standfirst are here because a pane that opens on a bare list of
   links does not say what document it is the contents OF. Everything before the
   first paragraph is the title page; nothing counts blocks. */
static inline void reader_pane(RC_App *app, AppState *st, const ReaderPalette *pal,
                               const TypeStep *sz, float columnPx)
{
    int i;

    rcColumn(.id = "pane", .scroll = "v", .w = "grow", .h = "grow") {
        /* The SAME measure the article gets: a contents list is reading
           matter too. */
        rcRow(.w = "grow", .h = "fit") {
            rcBox(.w = "grow", .h = "fit") {}
            rcColumn(.id = "panecol", .pt = 20, .pb = 56, .h = "fit",
                     .wType = RC_PX(columnPx)) {
                for (i = 0; i < DOC_BLOCK_COUNT && doc_blocks[i].kind != DOC_PARA; i++)
                    reader_block(app, &doc_blocks[i], &st->fonts, pal, sz, columnPx);

                rcRow(.pb = 18, .w = "grow", .h = "fit") {
                    if (reader_chip(st, pal, "resume",
                                    st->progress > 0.01f ? "Continue reading" : "Start reading",
                                    CHIP_TEXT_PX, true, false)) {
                        st->view    = READER_VIEW_ARTICLE;
                        st->pending = NULL;
                        rcWindowRequestFrame(rcAppMainWindow(app));
                    }
                }
                rcBox(.bg = pal->border, .w = "grow", .h = "1") {}
                rcBox(.pt = 16, .w = "grow", .h = "fit") {
                    rcColumn(.gap = 2, .w = "grow", .h = "fit") {
                        reader_toc_buttons(app, st, pal, sz);
                    }
                }
            }
            rcBox(.w = "grow", .h = "fit") {}
        }
    }
    rcScrollbar("pane");
}

/* The way back out of the article on the narrow arm. Labelled rather than a bare
   chevron: "back" has to survive being the only navigation in the window. */
static inline bool reader_back(const AppState *st, const ReaderPalette *pal)
{
    bool hover = rcIsHovered("back");

    rcRow(.id = "back", .bg = hover ? pal->hover : pal->paperAlt, .gap = 6, .pl = 8,
          .pr = 12, .align = "cc", .borderRadius = "all-full", .h = "fit",
          .hMin = CHIP_H) {
        rcIconArrowLeft(15.0f, pal->ink);
        rcTextL("Contents", .font = st->fonts.bold, .color = pal->ink, .size = CHIP_TEXT_PX);
    }
    return rcClicked("back");
}

/* The toolbar: who we are on the left, then search, text size and theme. It
   STACKS onto two rows when its controls stop fitting, and the decision is
   MEASURED - so the breakpoint follows the ladder, the labels and the text step
   actually installed. */
static inline void reader_header(RC_App *app, AppState *st, const ReaderPalette *pal,
                                 float availPx, bool article, bool showToc)
{
    const char *brand   = "RayClay Reader";
    /* Read off size_chip_px, because a budget that disagrees with what is drawn
       stacks the toolbar on the wrong frame. */
    float       sizePx  = 6.0f + 2.0f * (float)(TYPE_STEP_COUNT - 1);
    float       themePx = 6.0f + 2.0f
                        + reader_chip_px(st, "Paper", CHIP_TEXT_PX)
                        + reader_chip_px(st, "Dark", CHIP_TEXT_PX);
    float       brandPx = reader_text_px(st->fonts.bold, 15, rcStringFromCStr(brand));
    RC_String   pct     = rcFormat(rcAppArena(app), "%d%%", (int)(st->progress * 100.0f + 0.5f));
    /* 23 is the search icon and its gap; the three 12s are the row's gaps. */
    float       need;
    bool        stacked;
    int         i;

    for (i = 0; i < TYPE_STEP_COUNT; i++)
        sizePx += reader_chip_px(st, "A", size_chip_px[i]);
    need    = 2.0f * HEADER_PAD_X + brandPx + 12.0f + 23.0f + SEARCH_ONE_ROW_PX
            + 12.0f + sizePx + 12.0f + themePx;
    stacked = need > availPx;

    rcColumn(.id = "header", .bg = pal->paper, .gap = 10, .pt = 10, .pb = 8,
             .pl = HEADER_PAD_X, .pr = HEADER_PAD_X, .w = "grow", .h = "fit") {
        rcRow(.gap = 12, .align = "cl", .w = "grow", .h = "fit") {
            if (article && !showToc) {
                if (reader_back(st, pal)) {
                    st->view = READER_VIEW_CONTENTS;
                    rcWindowRequestFrame(rcAppMainWindow(app));
                }
            } else {
                rcBox(.w = "fit", .h = "fit") {
                    rcTextC(brand, .font = st->fonts.bold, .color = pal->ink, .size = 15);
                }
            }
            rcBox(.w = "grow", .h = "1") {}
            if (!stacked) {
                /* Typing is a request to see the results, which on the
                   one-pane arm are in the other pane. */
                if (reader_search(st, pal, SEARCH_ONE_ROW_PX) && !showToc)
                    st->view = READER_VIEW_CONTENTS;
                reader_size_group(st, pal);
            }
            reader_theme_group(st, pal);
        }
        if (stacked) {
            /* A field narrower than a section title cannot show what it
               matched, so it takes the second row whole. */
            rcRow(.gap = 12, .align = "cc", .w = "grow", .h = "fit") {
                if (reader_search(st, pal, 0.0f) && !showToc)
                    st->view = READER_VIEW_CONTENTS;
                reader_size_group(st, pal);
            }
        }
        /* Progress, read off the page container's own scroll offset. */
        rcRow(.gap = 10, .align = "cc", .w = "grow", .h = "fit") {
            rcBox(.w = "grow", .h = "fit") {
                rcProgress("progress", st->progress);
            }
            rcBox(.w = "fit", .h = "fit") {
                rcText(pct, .font = st->fonts.small, .color = pal->inkMuted, .size = 12);
            }
        }
    }
    rcBox(.bg = pal->border, .w = "grow", .h = "1") {}
}

/* The measure readout and the range it is judged against: one function, because
   the bar has two shapes below and both draw exactly these. */
static inline void reader_status_measure(AppState *st, const ReaderPalette *pal,
                                         RC_String measure, int chars)
{
    rcBox(.w = "fit", .h = "fit") {
        rcText(measure, .font = st->fonts.small, .color = reader_measure_color(pal, chars),
               .size = 12);
    }
    rcBox(.w = "fit", .h = "fit") {
        rcTextL("45-75 is comfortable", .font = st->fonts.small,
                .color = pal->inkMuted, .size = 12);
    }
}

static inline void reader_status(RC_App *app, AppState *st, const ReaderPalette *pal,
                                 float availPx, float columnPx, float avgCharPx)
{
    int       chars   = reader_measure_chars(columnPx, avgCharPx);
    /* rcFormat allocates in the frame arena - the lifetime a status line wants. */
    RC_String measure = rcFormat(rcAppArena(app), "measure ~%d chars", chars);
    RC_Color  noteInk = st->fonts.custom ? pal->inkMuted : pal->warn;
    /* Whether the three labels fit side by side, MEASURED rather than guessed:
       the bar's padding (32) and its three gaps (48) plus the three runs in the
       face and size they are drawn in. Four children and three gaps - a growing
       spacer sits between the note and the pair - so budgeting two gaps wraps
       the note inside a bar the arithmetic said fitted. */
    float     need    = 80.0f
                      + reader_text_px(st->fonts.small, 12, rcStringFromCStr(st->fonts.note))
                      + reader_text_px(st->fonts.small, 12, measure)
                      + reader_text_px(st->fonts.small, 12, rcStringFromCStr("45-75 is comfortable"));

    rcBox(.bg = pal->border, .w = "grow", .h = "1") {}
    if (need > availPx) {
        /* Two lines: nothing wraps mid-caption and nothing is dropped. */
        rcColumn(.id = "status", .bg = pal->paperAlt, .gap = 4, .pt = 7, .pb = 7, .pl = 16,
                 .pr = 16, .w = "grow", .h = "fit") {
            rcTextC(st->fonts.note, .font = st->fonts.small, .color = noteInk, .size = 12);
            rcRow(.gap = 16, .align = "c", .w = "grow", .h = "fit") {
                reader_status_measure(st, pal, measure, chars);
            }
        }
    } else {
        /* A floor rather than a fixed height, so the bar still grows if a face
           makes the labels taller than it. */
        rcRow(.id = "status", .bg = pal->paperAlt, .gap = 16, .pl = 16, .pr = 16,
              .align = "c", .w = "grow", .h = "fit", .hMin = 30) {
            rcBox(.w = "fit", .h = "fit") {
                rcTextC(st->fonts.note, .font = st->fonts.small, .color = noteInk, .size = 12);
            }
            rcBox(.w = "grow", .h = "1") {}
            reader_status_measure(st, pal, measure, chars);
        }
    }
}

/* THE SCROLL MUST BE WRITTEN BEFORE THE CONTAINER IS DECLARED. A clip container
   takes its child offset when it is OPENED: a scroll written after that moves
   the scrollbar thumb while the text stays put, and on an on-demand runner no
   frame is coming to correct it.
   IT ALSO HAS TO SETTLE RATHER THAN FIRE ONCE, because every box it reads
   describes the PREVIOUS frame and the document is still settling when the
   article has just appeared. */
static inline bool reader_seek_settled(RC_App *app, RC_ScrollInfo page, float target)
{
    float delta = target - page.offsetY;

    if (delta > -0.5f && delta < 0.5f)
        return true;
    rcScrollBy("page", 0.0f, delta);
    rcWindowRequestFrame(rcAppMainWindow(app));
    /* A target past the end of the document is reached when the container runs
       out of travel, not when the arithmetic says so. */
    return delta > 0.0f && page.offsetY >= page.maxOffsetY - 0.5f;
}

/* The two reasons this app moves the page itself: a section picked out of the
   contents, and the position the one-pane arm owes back - a clip container that
   is not declared for a frame loses its offset. */
static inline void reader_seek(RC_App *app, AppState *st)
{
    RC_ScrollInfo page = rcGetScrollInfo("page");

    if (!page.found) {
        /* The article was not on screen last frame, so there is no container to
           write to yet. It exists by the end of this one. */
        if (st->pending || st->restore)
            rcWindowRequestFrame(rcAppMainWindow(app));
        return;
    }
    if (st->pending) {
        RC_Box sec = rcGetElementBox(st->pending);
        RC_Box box = rcGetElementBox("page");

        /* Both boxes are in the same space, so their difference is how far
           below the container's top the section sits; rcScrollBy is
           positive-down. A box never laid out has no height: wait for one. */
        if (!sec.found || !box.found || sec.height <= 0.0f) {
            rcWindowRequestFrame(rcAppMainWindow(app));
            return;
        }
        if (reader_seek_settled(app, page, page.offsetY + (sec.y - box.y)))
            st->pending = NULL;
        st->restore = false;
    } else if (st->restore) {
        if (reader_seek_settled(app, page, st->pageOffsetY))
            st->restore = false;
    }
}

static inline void layout(RC_App *app, void *userData)
{
    AppState            *st = (AppState *)userData;
    const ReaderPalette *pal;
    const TypeStep      *sz;
    RC_Viewport          view;
    RC_ScrollInfo        page;
    bool                 showToc, article;
    float                columnPx, contentPx, panePx, avgCharPx, idealPx, availPx;
    int                  i;

    /* Fonts need the atlas, which exists once the app is running. */
    if (!st->ready) {
        st->ready = true;
        st->step  = TYPE_STEP_DEFAULT;
        reader_load_fonts(st);
    }

    /* Where the theme bool becomes a style; comparing against what the style
       layer last received keeps rcSetStyle off the per-frame path. */
    if (!st->themeReady || st->themeInStyle != st->dark)
        reader_apply_theme(st);

    /* THE SECOND LINE A THEME SWITCH NEEDS, and deliberately NOT inside the
       branch above: the window's clear colour is a snapshot taken at create, so
       without this the old ground shows wherever the layout does not cover the
       window. Unconditional, because rcWindowSetClearColor is change-gated and a
       guard keyed to the theme BOOL would read a style that is one line old. */
    rcWindowSetClearColor(rcAppMainWindow(app), rcGetStyle().background);

    pal  = reader_palette(st);
    sz   = &type_steps[st->step];
    view = rcViewport();

    /* SAFE AREA. A phone draws the window edge to edge, under the status bar and
       the home indicator, and nothing moves content out of the way for you. Ask
       for the margins and spend them ONCE, at the root below. rcViewport().safe
       hands them over ALREADY IN LAYOUT UNITS, and they are {0,0,0,0} on desktop
       unless RAYCLAY_SAFE_INSETS stands a phone's bands in. */
    RC_Insets safe = view.safe;

    /* Both the column and the breakpoint are derived from the TYPE, not from a
       device width: the column is TARGET_CHARS of the body face measure, and the
       rail folds away when keeping it would take the column below that.
       Measured against the width that SURVIVES THE INSETS, since the root spends
       safe.left/right below and both terms come from rcViewport(). */
    availPx   = view.width - (safe.left + safe.right);
    avgCharPx = reader_avg_char_px(st->fonts.body, sz->body);
    idealPx   = avgCharPx > 0.0f ? avgCharPx * TARGET_CHARS : 640.0f;

    showToc  = availPx >= TOC_MIN_PX;
    /* What the article can take: the window less the rail, its rule, the gutter
       that holds the article beside it, and the page's own breathing room. */
    panePx   = availPx - (showToc ? TOC_W_PX + 1.0f + PAGE_GUTTER_PX : 0.0f) - 48.0f;
    columnPx = panePx > idealPx ? idealPx : panePx;
    if (columnPx < 220.0f)
        columnPx = 220.0f;

    /* PROSE STOPS AT THE MEASURE; A LISTING DOES NOT HAVE TO. Wide, the content
       is anchored beside the rail and the slack falls to its right, so the code
       panel spends it - up to the widest line it holds. The one-pane arm centres
       the content and has no slack to give, so there a listing pans instead. */
    contentPx = columnPx;
    if (showToc) {
        float codePx = reader_code_px(&st->fonts, sz);

        if (codePx > contentPx)
            contentPx = codePx > panePx ? panePx : codePx;
    }

    /* The wide arm always has the article; the narrow arm has whichever pane
       the reader is in. */
    article = showToc || st->view == READER_VIEW_ARTICLE;

    /* Progress and the article's place, both read off the container the library
       already maintains. rcGetScrollInfo reports the PREVIOUS frame, so a scroll
       landing on the last frame of a gesture would leave the bar a notch behind:
       ask for one more frame when the figure has actually moved, which settles
       because the next frame reads the same offset. NOT while a restore is
       outstanding, when the container reports the zero it was re-created with. */
    page = rcGetScrollInfo("page");
    if (page.found && !st->restore) {
        float frac = page.maxOffsetY > 0.0f ? page.offsetY / page.maxOffsetY : 1.0f;
        if (frac > 1.0f)
            frac = 1.0f;
        st->pageOffsetY = page.offsetY;
        if (frac != st->progress) {
            st->progress = frac;
            rcWindowRequestFrame(rcAppMainWindow(app));
        }
    }
    /* WHICH SECTION THE READER IS IN: a contents that cannot say where you are
       is a list of links rather than a map. The current section is the last
       heading whose top has reached the upper part of the page, and section ONE
       is the answer until a heading overtakes it - so the mark is there on the
       opening frame. Boxes describe the PREVIOUS frame, which is the frame the
       reader was looking at, so the mark asks for one only when it MOVES. Not
       computed when the article is absent: the contents pane then keeps the last
       section marked, which is where "Continue reading" goes. */
    if (article) {
        RC_Box      box = rcGetElementBox("page");
        const char *now = NULL;

        for (i = 0; i < DOC_BLOCK_COUNT && !now; i++)
            if (doc_blocks[i].kind == DOC_H2)
                now = doc_blocks[i].id;

        if (box.found && box.height > 0.0f) {
            float band = box.y + box.height * 0.4f;
            for (i = 0; i < DOC_BLOCK_COUNT; i++) {
                RC_Box sec;

                if (doc_blocks[i].kind != DOC_H2)
                    continue;
                sec = rcGetElementBox(doc_blocks[i].id);
                if (sec.found && sec.height > 0.0f && sec.y <= band)
                    now = doc_blocks[i].id;
            }
        }
        /* Pointer equality, not strcmp: every id is a string literal in
           document.h, so a section has one address for the life of the run. */
        if (now != st->active) {
            st->active = now;
            rcWindowRequestFrame(rcAppMainWindow(app));
        }
    }

    if (!article)
        st->restore = true;   /* the page is about to stop existing: see below */

    rcColumn(.id = "root", .bg = pal->paper, .pt = (uint16_t)(safe.top),
             .pb = (uint16_t)(safe.bottom), .pl = (uint16_t)(safe.left),
             .pr = (uint16_t)(safe.right), .w = "grow", .h = "grow") {
        reader_header(app, st, pal, availPx, article, showToc);
        rcRow(.id = "body", .w = "grow", .h = "grow") {
            if (showToc)
                reader_toc(app, st, pal, sz);

            if (!article) {
                reader_pane(app, st, pal, sz, columnPx);
            } else {
                reader_seek(app, st);
                /* Placed by two growing spacers rather than by a margin. */
                rcRow(.id = "page", .scroll = "v", .w = "grow", .h = "grow") {
                    /* The gutter beside the rail is CAPPED so the content sits
                       next to the rail and the slack falls on the right, as a
                       docs site does. No rail below TOC_MIN_PX, so the cap lifts
                       (0 = unset) and the content centres instead. */
                    rcBox(.w = "grow", .h = "fit",
                          .wMax = showToc ? PAGE_GUTTER_PX : 0.0f) {}
                    /* The one place a device band is the honest input, so the
                       one place spelled in responsive classes: vertical rhythm
                       is a judgement about screen size and nothing measures it.
                       Everything else sizes off the TYPE, which a class string
                       cannot ask a font about. */
                    rcColumn(.id = "col", .h = "fit",
                             .wType = RC_PX(contentPx),
                             .className = "pt-5 sm:pt-7 lg:pt-10 "
                                          "pb-12 sm:pb-16") {
                        for (i = 0; i < DOC_BLOCK_COUNT; i++)
                            reader_block(app, &doc_blocks[i], &st->fonts, pal, sz,
                                         columnPx);
                    }
                    rcBox(.w = "grow", .h = "fit") {}
                }
                rcScrollbar("page");
            }
        }
        reader_status(app, st, pal, availPx, columnPx, avgCharPx);
    }
}

#endif /* APP_APP_H */
