/*
    blocks.h - one document block, rendered.

    Maps a DocKind to RayClay elements: every heading, rule, list marker and code
    panel in the reader is decided here. The palette and the type step arrive by
    pointer, so a frame cannot be drawn half in the old theme.

    PROSE IS HELD TO THE MEASURE; A LISTING IS NOT. Every text block is given
    measurePx, the width TARGET_CHARS of the body face comes to; the code panel
    takes the whole column, because a wrapped line of C is a lie about the
    program and a cropped one reads as a rendering fault.
*/
#ifndef APP_BLOCKS_H
#define APP_BLOCKS_H

#include "rayclay.h"
#include "app/document.h"
#include "app/theme.h"

/* The resolved face ids for one type ramp; filled once at startup by app.h. */
typedef struct {
    uint16_t    h1, h2, body, bold, small, code;
    bool        custom;      /* false => every id above is the bundled face */
    const char *note;        /* what the status bar says about that         */
} ReaderFonts;

/* Under a listing still wider than its panel - a phone, or the largest text
   step - say so, and say how to reach the rest: the panel pans, but nothing
   DRAWS that, since rcScrollbar is vertical-only. The library is the authority
   on whether it overflows, so the caption appears exactly when there is
   something to pan to. That reading describes the PREVIOUS frame, so ask for one
   more when the container is not found yet. */
static inline void reader_code_caption(RC_App *app, const DocBlock *b, const ReaderFonts *f,
                                       const ReaderPalette *pal, const TypeStep *sz)
{
    RC_ScrollInfo pan;

    if (!b->id)
        return;
    pan = rcGetScrollInfo(b->id);
    if (!pan.found) {
        rcWindowRequestFrame(rcAppMainWindow(app));
        return;
    }
    if (pan.maxOffsetX <= 0.0f)
        return;
    rcBox(.pt = 6, .pb = 10, .w = "grow", .h = "fit") {
        rcTextL("Continues to the right: drag or scroll it sideways", .font = f->small,
                .color = pal->inkMuted, .size = sz->small);
    }
}

static inline void reader_block(RC_App *app, const DocBlock *b, const ReaderFonts *f,
                                const ReaderPalette *pal, const TypeStep *sz, float measurePx)
{
    RC_Size measure = measurePx > 0.0f ? RC_PX(measurePx) : RC_GROW;

    switch (b->kind) {
    case DOC_H1:
        rcBox(.pt = 4, .pb = 12, .h = "fit", .wType = measure) {
            rcTextC(b->text, .font = f->h1, .color = pal->ink, .size = sz->h1);
        }
        break;

    case DOC_H2:
        rcBox(.id = b->id, .pt = 26, .pb = 6, .h = "fit", .wType = measure) {
            rcTextC(b->text, .font = f->h2, .color = pal->accent, .size = sz->h2);
        }
        break;

    /* The one block that draws the bold weight. ONLY REGISTER A FACE YOU DRAW:
       each costs a slot out of sixteen, and rcUnloadFont buys the SLOT back but
       not the atlas room it baked into. */
    case DOC_LEAD:
        rcBox(.pb = 16, .h = "fit", .wType = measure) {
            rcTextC(b->text, .font = f->bold, .color = pal->ink,
                    .size = sz->body, .lineHeight = sz->bodyLine);
        }
        break;

    case DOC_PARA:
        rcBox(.pb = 14, .h = "fit", .wType = measure) {
            rcTextC(b->text, .font = f->body, .color = pal->ink,
                    .size = sz->body, .lineHeight = sz->bodyLine);
        }
        break;

    case DOC_BULLET:
        /* A hanging indent, so a wrapped line aligns under the first WORD. The
           marker is DRAWN, not typed: the bundled face bakes Latin-1 only, so a
           U+2022 would resolve to the missing-glyph substitute on screen with
           nothing in the log. */
        rcRow(.gap = 10, .pb = 9, .h = "fit", .wType = measure) {
            rcBox(.align = "cc", .w = "14", .hType = RC_PX((float)sz->bodyLine)) {
                rcBox(.bg = pal->accent, .borderRadius = "all-full", .w = "5px",
                      .h = "5px") {}
            }
            rcBox(.w = "grow", .h = "fit") {
                rcTextC(b->text, .font = f->body, .color = pal->ink,
                        .size = sz->body, .lineHeight = sz->bodyLine);
            }
        }
        break;

    case DOC_CODE:
        /* .wrap = "l" keeps every newline the listing contains and invents none.
           .scroll = "h" is what makes that survivable where the panel is still
           too narrow, and it also makes this a clip container on BOTH axes -
           which is why the block draws no shadow.
           The hairline keeps the panel a panel on paper. The .id is what lets
           reader_code_caption ask whether this block overflows. */
        rcBox(.id = b->id, .bg = pal->codeBg, .p = CODE_PAD_PX, .scroll = "h",
              .borderRadius = "all-md", .border = { .color = pal->border, .width = "1" },
              .w = "grow", .h = "fit") {
            rcTextC(b->text, .font = f->code, .color = pal->codeInk,
                    .size = sz->code, .lineHeight = sz->codeLine, .wrap = "l");
        }
        reader_code_caption(app, b, f, pal, sz);
        break;

    case DOC_NOTE:
        rcRow(.gap = 12, .pt = 2, .pb = 14, .h = "fit", .wType = measure) {
            rcBox(.bg = pal->accent, .borderRadius = "all-sm", .w = "3",
                  .h = "grow") {}
            rcBox(.w = "grow", .h = "fit") {
                rcTextC(b->text, .font = f->body, .color = pal->inkMuted,
                        .size = sz->small, .lineHeight = sz->noteLine);
            }
        }
        break;
    }
}

#endif /* APP_BLOCKS_H */
