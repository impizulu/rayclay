/*
    palette.h - the quick-open palette: Ctrl/Cmd+P, the app's moment of speed

    A plain query fuzzy-finds a file by name, ">" a command, "@" a symbol of this file, ":" a
    line. The list is rebuilt on the frame the query changes, over everything in the folder,
    with no index and no thread: the arrays are short enough that a keystroke costs less than
    the frame it arrives in.
*/
#ifndef APP_PALETTE_H
#define APP_PALETTE_H

#include "rayclay.h"
#include "app/commands.h"

/* Insert into the best-first list, keeping only the top MAX_PICKS. */
static inline void add_pick(QuickOpen *p, Pick pick) {
    int i;

    if (p->n == MAX_PICKS && pick.score <= p->picks[MAX_PICKS - 1].score)
        return;
    i = p->n < MAX_PICKS ? p->n++ : MAX_PICKS - 1;
    while (i > 0 && p->picks[i - 1].score < pick.score) {
        p->picks[i] = p->picks[i - 1];
        i--;
    }
    p->picks[i] = pick;
}

static inline void pick_files(AppState *st, const char *q) {
    QuickOpen *p  = &st->pal;
    Workspace *ws = &st->ws;
    int        i;

    for (i = 0; i < ws->count; i++) {
        const Node *n = &ws->node[i];
        Pick        pick = { .kind = PICK_FILE, .index = i };
        int         score;

        if (n->dir)
            continue;
        score = fuzzy_match(q, n->name, pick.hits, &pick.nHits);
        if (score >= 0) {
            pick.score = score + 100;       /* a hit in the name beats one in the path */
        } else {
            score = fuzzy_match(q, n->path, NULL, NULL);
            pick.nHits = 0;
            pick.score = score;
        }
        if (pick.score < 0)
            continue;
        /* Open files come first, and short names beat long ones on a tie. */
        pick.score = pick.score * 8 + (tab_of(st, n->path) >= 0 ? 400 : 0) - str_len(n->name) / 4;
        add_pick(p, pick);
    }
}

static inline void pick_commands(AppState *st, const char *q) {
    int i;

    for (i = 0; i < CMD_COUNT; i++) {
        Pick pick = { .kind = PICK_COMMAND, .index = i };
        int  score;

        if (!COMMANDS[i].title || !command_enabled(st, (Command)i))
            continue;
        score = fuzzy_match(q, COMMANDS[i].title, pick.hits, &pick.nHits);
        if (score >= 0) {
            pick.score = score * 8 - i;
            add_pick(&st->pal, pick);
        }
    }
}

/* Symbols stay in file order until a query ranks them. */
static inline void pick_symbols(AppState *st, const char *q) {
    Doc *d = active_doc(st);
    int  i;

    if (!d)
        return;
    for (i = 0; i < d->nSym; i++) {
        const Symbol *s = &d->sym[i];
        Pick          pick = { .kind = PICK_SYMBOL, .index = i };
        char          name[80];
        int           n = s->nameLen < 79 ? s->nameLen : 79, score;

        move_bytes(name, d->text + s->name, n);
        name[n] = '\0';
        score = fuzzy_match(q, name, pick.hits, &pick.nHits);
        if (score >= 0) {
            pick.score = q[0] ? score * 8 : -i;
            add_pick(&st->pal, pick);
        }
    }
}

/* Nine digits already name a line past any file's last, where go_to lands; a tenth could
   overflow the int. */
static inline int parse_int(const char *s) {
    int v = 0;

    while (is_digit(*s) && v < 100000000)
        v = v * 10 + (*s++ - '0');
    return v;
}

static inline void palette_refresh(AppState *st) {
    QuickOpen  *p = &st->pal;
    const char *q = p->query;
    const Doc  *d = active_doc(st);
    unsigned    rev = d ? d->rev : 0u;

    /* A symbol pick is an index into the front tab's outline. Another tab in front, or an edit
       under the open palette (a drop, a paste's answer), moves that outline: list again. */
    if (same_text(p->seen, p->query) && p->seenDoc == st->active && p->seenRev == rev)
        return;
    rcStrCopy(p->seen, p->query, sizeof p->seen);
    p->seenDoc = st->active;
    p->seenRev = rev;
    p->n   = 0;
    p->sel = 0;
    p->scrollToPick = true;
    if (q[0] == '>')
        pick_commands(st, q + 1);
    else if (q[0] == '@')
        pick_symbols(st, q + 1);
    else if (q[0] != ':')
        pick_files(st, q);
}

static inline void palette_close(AppState *st) {
    st->pal.open  = false;
    st->pal.query[0] = '\0';
    st->wantCaret = !rcPointerIsCoarse();
}

/* Do what the chosen row says. */
static inline void palette_accept(AppState *st, int index) {
    QuickOpen *p = &st->pal;
    Doc       *d = active_doc(st);

    if (p->query[0] == ':') {
        if (d && is_digit(p->query[1]))
            go_to(st, d, parse_int(p->query + 1) - 1, 0);
        palette_close(st);
        return;
    }
    if (index >= 0 && index < p->n) {
        const Pick *pick = &p->picks[index];
        int         kind = pick->kind, what = pick->index;

        palette_close(st);
        if (kind == PICK_COMMAND) {
            post_command(st, (Command)what);
        } else if (kind == PICK_SYMBOL && d) {
            go_to(st, d, d->sym[what].line, 0);
        } else if (kind == PICK_FILE) {
            /* Posted: a search's rows, declared this frame, may draw from the file's bytes. */
            st->openNode = what;
            st->openLine = -1;
            post_command(st, CMD_OPEN_NODE);
        }
    } else {
        palette_close(st);
    }
}

/* A name with the query's letters picked out, the way the eye checks a fuzzy match. */
static inline void highlighted_name(const char *text, const int *hits, int nHits, RC_Color normal,
                                    RC_Color strong, int size) {
    int at = 0, h = 0, n = str_len(text);

    while (at < n) {
        bool on  = h < nHits && hits[h] == at;
        int  end = at + 1;

        if (on) {
            for (h++; end < n && h < nHits && hits[h] == end; h++)
                end++;
        } else {
            while (end < n && (h >= nHits || hits[h] != end))
                end++;
        }
        {
            RC_String run = { .isStaticallyAllocated = false, .length = end - at,
                              .chars = text + at };

            rcText(run, .color = on ? strong : normal, .size = (uint16_t)size, .wrap = "n",
                   .select = RC_SELECT_NONE);
        }
        at = end;
    }
}

static inline void file_mark_box(const char *name, float size) {
    FileMark m = file_mark(name);

    rcBox(.align = "cc", .wType = RC_PX(size), .hType = RC_PX(size)) {
        rcTextC(m.mark, .color = m.color, .size = SZ_SMALL, .wrap = "n", .select = RC_SELECT_NONE);
    }
}

/* A row's id names what it picks, not its place: the list re-sorts as the query grows, and
   hover and press stay with the item. */
static inline const char *pick_id(AppState *st, const Pick *pk) {
    return rcFormat(st->arena, "pal%d_%d", pk->kind, pk->index).chars;
}

static inline void palette_row(AppState *st, int i, const char *id) {
    const Palette *t  = theme();
    QuickOpen     *p  = &st->pal;
    const Pick    *pk = &p->picks[i];
    bool           on = i == p->sel;
    Doc           *d  = active_doc(st);

    /* The chosen row's typed fill beats the class's hover; any other row shows the hover. */
    rcRow(.id = id, .bg = on ? t->pick : RC_UNSET_COLOR, .gap = 8, .px = 12, .align = "cl",
          .overflow = "hidden", .w = "grow", .hType = RC_PX(st->compact ? HIT : 26),
          .className = "hover:bg-surface-alt") {
        if (pk->kind == PICK_FILE) {
            const Node *n = &st->ws.node[pk->index];

            file_mark_box(n->name, 16.0f);
            rcRow(.align = "cl") {
                highlighted_name(n->name, pk->hits, pk->nHits, t->bright, t->info, SZ_UI);
            }
            rcTextC(n->path, .color = t->muted, .size = SZ_SMALL, .wrap = "e",
                    .select = RC_SELECT_NONE);
        } else if (pk->kind == PICK_COMMAND) {
            rcRow(.align = "cl", .w = "grow") {
                highlighted_name(COMMANDS[pk->index].title, pk->hits, pk->nHits, t->bright, t->info,
                                 SZ_UI);
            }
            rcTextC(command_keys(st->arena, (Command)pk->index), .color = t->muted,
                    .size = SZ_SMALL, .wrap = "n", .select = RC_SELECT_NONE);
        } else if (d) {
            const Symbol *s = &d->sym[pk->index];
            RC_String     name = { .isStaticallyAllocated = false, .length = s->nameLen,
                                   .chars = d->text + s->name };

            rcTextL("f", .font = st->mono, .color = t->syntax[TOK_CONTROL], .size = SZ_UI,
                    .select = RC_SELECT_NONE);
            rcText(name, .color = t->bright, .size = SZ_UI, .wrap = "n", .select = RC_SELECT_NONE);
            rcSpacer() {}
            rcText(rcFormat(st->arena, "Ln %d", s->line + 1), .color = t->muted, .size = SZ_SMALL,
                   .wrap = "n", .select = RC_SELECT_NONE);
        }
    }
}

/* The palette floats at the top center, above everything but a modal, like its leader's. Its
   field keeps the focus: a press on a row takes it for a moment, so it is asked for per frame. */
static inline void palette_view(AppState *st, float width) {
    const Palette *t = theme();
    QuickOpen     *p = &st->pal;
    int            i;
    int            accept = -1;

    if (!p->open)
        return;
    palette_refresh(st);
    if (rcKeyPressed(RC_KEY_ESCAPE) || rcKeyPressedOn("pal_input", RC_KEY_ESCAPE)
        || (rcPointerPressed(RC_POINTER_LEFT) && !rcIsHovered("palette"))) {
        palette_close(st);
        return;
    }
    if (key_fires(RC_KEY_DOWN) && p->n) {
        p->sel = (p->sel + 1) % p->n;
        p->scrollToPick = true;
    }
    if (key_fires(RC_KEY_UP) && p->n) {
        p->sel = (p->sel + p->n - 1) % p->n;
        p->scrollToPick = true;
    }
    if (rcKeyPressed(RC_KEY_ENTER))
        accept = p->sel;
    rcSetFocus("pal_input");
    rcColumn(.id = "palette", .bg = t->widget, .borderRadius = "all-md",
             .border = { .color = t->edge, .width = 1 }, .wType = RC_PX(width),
             .shadow = { .color = rcWithAlpha(RC_BLACK, 0.43f), .y = 8, .blur = 24 },
             .floating = { .to = RC_ATTACH_ROOT, .parent = RC_ANCHOR_TOP_CENTER,
                           .element = RC_ANCHOR_TOP_CENTER, .offset = { 0.0f, 6.0f },
                           .zIndex = 2000 }) {
        rcColumn(.p = 6, .w = "grow") {
            rcBox(.w = "grow") {
                /* The picks follow the query at the next frame's top: ask for it. */
                if (rcTextInput("pal_input", p->query, (int)sizeof p->query,
                                .className = control_h(st, "h-[30px]"),
                                .placeholder =
                                    "Search files by name. > commands, @ symbols, : line",
                                .clearButton = true))
                    rcWindowRequestFrame(rcAppMainWindow(st->app));
            }
        }
        if (p->query[0] == ':') {
            Doc *d = active_doc(st);

            rcRow(.px = 12, .align = "cl", .w = "grow", .hType = RC_PX(30)) {
                rcText(!d ? rcFormat(st->arena, "Open a file first")
                       : is_digit(p->query[1])
                       ? rcFormat(st->arena, "Go to line %d of %d, then press Enter",
                                  parse_int(p->query + 1), d->lines)
                       : rcFormat(st->arena, "Current line: %d. Type a line from 1 to %d.",
                                  doc_line_of(d, d->caret) + 1, d->lines),
                       .color = t->text, .size = SZ_UI, .wrap = "n", .select = RC_SELECT_NONE);
            }
        } else if (!p->n) {
            rcRow(.px = 12, .align = "cl", .w = "grow", .hType = RC_PX(30)) {
                rcTextL("No matching results", .color = t->muted, .size = SZ_UI,
                        .select = RC_SELECT_NONE);
            }
        } else {
            rcColumn(.id = "pal_list", .py = 4, .scroll = "v", .w = "grow",
                     .hMax = RC_VAL(st->compact ? 8 * HIT : 13 * 26)) {
                for (i = 0; i < p->n; i++) {
                    const char *id = pick_id(st, &p->picks[i]);

                    palette_row(st, i, id);
                    if (rcClicked(id))
                        accept = i;
                }
            }
            if (p->scrollToPick) {
                rcScrollIntoView("pal_list", pick_id(st, &p->picks[p->sel]), RC_SCROLL_NEAREST);
                p->scrollToPick = false;
            }
            rcScrollbar("pal_list");
        }
    }
    if (accept >= 0)
        palette_accept(st, accept);
}

#endif /* APP_PALETTE_H */
