/*
    editor.h - the code editor: caret, selection, keys, mouse, find and the drawn lines

    RayClay's rcTextArea is a fine text box, but a code editor needs colored runs, a gutter and
    a caret position it can read, so this app owns the document and draws the lines itself, one
    virtual-list row per line and one text run per token.

    THE KEYBOARD STILL BELONGS TO A REAL FIELD. An empty rcTextArea sits exactly at the caret.
    Whatever it receives - typing, dead keys, an input method's commit, a paste, a phone's
    keyboard - lands in its buffer, the editor moves those bytes into the document at the top of
    the next frame and empties it. So the field also draws the blinking caret, shows composing
    text in place and raises the soft keyboard, with no code here. It is the technique browser
    code editors use with a hidden <textarea>. It is declared last, which leaves Tab to the
    editor on a desktop and a phone: Tab at a page's last field is the app's. On the web the
    browser takes that Tab and focus leaves the page, so no keyboard is trapped (rcTextInput's
    notes on Tab).

    Edits only happen at the top of a frame, before a line is declared, because a text run
    borrows the document's bytes until the frame is drawn.
*/
#ifndef APP_EDITOR_H
#define APP_EDITOR_H

#include "rayclay.h"
#include "app/state.h"
#include "app/workspace.h"

#define SINK_ID "editor_keys"
#define GUTTER_ID "gutter"
#define PANE_ID "editor_pane"

enum { CODE_PAD = 8, TAB_COLUMNS = 4, MINI_CELL = 1, SIDE_BAR_H = 12 };

/* What a byte of a line has behind it. */
enum { MARK_NONE, MARK_WORD, MARK_BRACKET, MARK_SELECTION, MARK_MATCH, MARK_CURRENT };

static inline RC_Color mark_color(int mark) {
    const Palette *t = theme();

    switch (mark) {
    case MARK_WORD:      return t->word;
    case MARK_BRACKET:   return t->bracket;
    case MARK_MATCH:     return t->match;
    case MARK_CURRENT:   return t->matchOn;
    case MARK_SELECTION: return t->selection;
    default:             return RC_UNSET_COLOR;
    }
}

/* ---- Positions: a byte offset and the pixel it draws at. ---- */

/* A tab draws as TAB_COLUMNS spaces (code_text); rcTextWidth counts it as one. */
static inline float line_x(const AppState *st, const Doc *d, int ln, int bytes) {
    const char *s = d->text + d->line[ln];
    int         tabs = 0, i;

    for (i = 0; i < bytes; i++)
        tabs += s[i] == '\t';
    return rcTextWidth(st->mono, SZ_CODE, s, bytes)
         + (float)(tabs * (TAB_COLUMNS - 1)) * st->cell;
}

static inline int byte_at_x(const AppState *st, const Doc *d, int ln, float x) {
    const char *s = d->text + d->line[ln];
    int         n = doc_line_len(d, ln), i = 0;
    float       acc = 0.0f;

    while (i < n) {
        int   next = i + 1;
        float w;

        while (next < n && is_tail_byte(s[next]))
            next++;
        w = s[i] == '\t' ? (float)TAB_COLUMNS * st->cell
                         : rcTextWidth(st->mono, SZ_CODE, s + i, next - i);
        if (x < acc + w * 0.5f)
            return i;
        acc += w;
        i = next;
    }
    return n;
}

static inline void set_caret(AppState *st, Doc *d, int at, bool extend) {
    d->caret = at;
    if (!extend)
        d->anchor = at;
    d->wantX = -1.0f;
    st->reveal = REVEAL_NEAREST;
}

/* Jump to a place from outside the editor, and put the keyboard back in it. */
static inline void go_to(AppState *st, Doc *d, int line, int col) {
    int len;

    line = line < 0 ? 0 : line >= d->lines ? d->lines - 1 : line;
    len  = doc_line_len(d, line);
    set_caret(st, d, d->line[line] + (col > len ? len : col), false);
    st->reveal    = REVEAL_CENTER;
    st->wantCaret = true;
}

static inline int step_left(const Doc *d, int at) {
    if (at <= 0)
        return 0;
    at--;
    while (at > 0 && is_tail_byte(d->text[at]))
        at--;
    if (at > 0 && d->text[at] == '\n' && d->text[at - 1] == '\r')
        at--;
    return at;
}

static inline int step_right(const Doc *d, int at) {
    if (at >= d->len)
        return d->len;
    if (d->text[at] == '\r' && d->text[at + 1] == '\n')
        return at + 2;
    at++;
    while (at < d->len && is_tail_byte(d->text[at]))
        at++;
    return at;
}

static inline int word_left(const Doc *d, int at) {
    while (at > 0 && !is_word(d->text[at - 1]))
        at--;
    while (at > 0 && is_word(d->text[at - 1]))
        at--;
    return at;
}

static inline int word_right(const Doc *d, int at) {
    while (at < d->len && !is_word(d->text[at]))
        at++;
    while (at < d->len && is_word(d->text[at]))
        at++;
    return at;
}

static inline void move_vertical(AppState *st, Doc *d, int lines, bool extend) {
    int   ln = doc_line_of(d, d->caret), target = ln + lines;
    float want = d->wantX >= 0.0f ? d->wantX
                                  : line_x(st, d, ln, d->caret - d->line[ln]);

    if (target < 0) {
        set_caret(st, d, 0, extend);
    } else if (target >= d->lines) {
        set_caret(st, d, d->len, extend);
    } else {
        set_caret(st, d, d->line[target] + byte_at_x(st, d, target, want), extend);
        d->wantX = want;
    }
}

/* Home goes to the code first, then to the margin, then back. */
static inline void move_home(AppState *st, Doc *d, bool extend) {
    int ln = doc_line_of(d, d->caret), start = d->line[ln], n = doc_line_len(d, ln), i = 0;

    while (i < n && is_blank(d->text[start + i]))
        i++;
    set_caret(st, d, d->caret == start + i ? start : start + i, extend);
}

static inline int view_lines(const Doc *d) {
    RC_Box box = rcGetElementBox(d->scrollId);

    return box.found && box.height > (float)LINE_H ? (int)(box.height / (float)LINE_H) : 20;
}

/* ---- Edits of whole lines: one undo step each, built in a temporary block. ---- */

static inline int line_end_offset(const Doc *d, int ln) {
    return d->line[ln] + doc_line_len(d, ln);
}

enum { BLOCK_INDENT, BLOCK_OUTDENT, BLOCK_COMMENT };

static inline const char *comment_mark(const Doc *d) {
    return d->lang == LANG_C ? "// " : d->lang == LANG_SCRIPT ? "# " : NULL;
}

/* The selected lines rewritten through one of the three transforms. */
static inline void edit_block(Doc *d, int how) {
    int         first = doc_line_of(d, sel_start(d));
    int         last  = doc_line_of(d, sel_end(d));
    const char *mark  = comment_mark(d);
    int         mn    = mark ? str_len(mark) : 0;
    int         start, end, out = 0, ln, indent = 1 << 30;
    bool        allMarked = mn > 0;
    bool        hadSel    = has_sel(d), edited;
    int         caretLine = doc_line_of(d, d->caret);
    int         caretCol  = d->caret - d->line[caretLine];
    int         caretDelta = 0;
    RC_Arena    tmp;

    if (how == BLOCK_COMMENT && !mark)
        return;
    if (hadSel && last > first && d->line[last] == sel_end(d))
        last--;
    start = d->line[first];
    end   = line_end_offset(d, last);
    if (!block_new(&tmp, (size_t)(end - start) + (size_t)(last - first + 1) * 8 + 8)) {
        rcArenaFree(&tmp);
        return;
    }
    for (ln = first; ln <= last; ln++) {
        const char *s = d->text + d->line[ln];
        int         n = doc_line_len(d, ln), i = 0;

        while (i < n && is_blank(s[i]))
            i++;
        if (i < n) {
            indent = i < indent ? i : indent;
            allMarked = allMarked && (n - i >= mn - 1) && same_bytes(s + i, mark, mn - 1);
        }
    }
    for (ln = first; ln <= last; ln++) {
        const char *s     = d->text + d->line[ln];
        int         n     = doc_line_len(d, ln);
        int         end2  = line_end_offset(d, ln);
        char       *o     = (char *)tmp.buffer;
        int         i     = 0, before = out;

        if (how == BLOCK_INDENT && n) {
            for (i = 0; i < TAB_COLUMNS; i++)
                o[out++] = ' ';
            i = 0;
        } else if (how == BLOCK_OUTDENT) {
            while (i < n && i < TAB_COLUMNS && s[i] == ' ')
                i++;
        } else if (how == BLOCK_COMMENT && allMarked) {
            int at = 0;

            while (at < n && is_blank(s[at]))
                at++;
            if (at < n) {
                int drop = mn - 1 + (at + mn - 1 < n && s[at + mn - 1] == ' ');

                move_bytes(o + out, s, at);
                out += at;
                i = at + drop;
            }
        } else if (how == BLOCK_COMMENT && n) {
            int at = indent < n ? indent : n;

            move_bytes(o + out, s, at);
            out += at;
            move_bytes(o + out, mark, mn);
            out += mn;
            i = at;
        }
        move_bytes(o + out, s + i, n - i);
        out += n - i;
        if (ln == caretLine)
            caretDelta = (out - before) - n;
        if (ln < last) {
            int nl = d->line[ln + 1] - end2;

            move_bytes(o + out, d->text + end2, nl);
            out += nl;
        }
    }
    edited = doc_edit(d, start, end - start, (const char *)tmp.buffer, out, EDIT_OTHER);
    rcArenaFree(&tmp);
    if (!edited)            /* refused: the positions below are the new text's */
        return;
    if (hadSel) {
        d->anchor = start;
        d->caret  = start + out;
    } else {
        int col = caretCol + caretDelta;

        d->caret = d->anchor = d->line[caretLine] + (col < 0 ? 0 : col);
    }
}

/* Alt+Up and Alt+Down: the selected lines trade places with their neighbor. The line ending
   between the two blocks is the file's own bytes, moved with them: a CRLF file stays CRLF. */
static inline void move_lines(AppState *st, Doc *d, int dir) {
    int      first = doc_line_of(d, sel_start(d)), last = doc_line_of(d, sel_end(d));
    int      top, bottom, split, a, b, an, bn, sep, keepCaret, keepAnchor;
    RC_Arena tmp;
    char    *o;

    if (has_sel(d) && last > first && d->line[last] == sel_end(d))
        last--;
    if ((dir < 0 && first == 0) || (dir > 0 && last == d->lines - 1))
        return;
    top    = dir < 0 ? first - 1 : first;
    bottom = dir < 0 ? last : last + 1;
    split  = dir < 0 ? first : last + 1;      /* the second block's first line */
    a  = d->line[top];
    b  = d->line[split];
    an = line_end_offset(d, split - 1) - a;
    bn = line_end_offset(d, bottom) - b;
    sep = b - (a + an);
    o = block_new(&tmp, (size_t)(an + sep + bn));
    if (!o) {
        rcArenaFree(&tmp);
        return;
    }
    move_bytes(o, d->text + b, bn);
    move_bytes(o + bn, d->text + a + an, sep);
    move_bytes(o + bn + sep, d->text + a, an);
    keepCaret  = d->caret + (dir < 0 ? -an - sep : bn + sep);
    keepAnchor = d->anchor + (dir < 0 ? -an - sep : bn + sep);
    if (doc_edit(d, a, an + sep + bn, o, an + sep + bn, EDIT_OTHER)) {
        d->caret  = keepCaret;
        d->anchor = keepAnchor;
    }
    rcArenaFree(&tmp);
    st->reveal = REVEAL_NEAREST;
}

static inline void delete_lines(AppState *st, Doc *d) {
    int first = doc_line_of(d, sel_start(d)), last = doc_line_of(d, sel_end(d)), start, end;

    if (has_sel(d) && last > first && d->line[last] == sel_end(d))
        last--;
    start = d->line[first];
    end   = last + 1 < d->lines ? d->line[last + 1] : d->len;
    if (last + 1 >= d->lines && first > 0)
        start = line_end_offset(d, first - 1);
    doc_edit(d, start, end - start, "", 0, EDIT_OTHER);
    st->reveal = REVEAL_NEAREST;
}

/* The copy goes in under the lines, after a line ending of the file's kind, as Enter's is. */
static inline void duplicate_lines(AppState *st, Doc *d) {
    int      first = doc_line_of(d, sel_start(d)), last = doc_line_of(d, sel_end(d));
    int      start = d->line[first], end = line_end_offset(d, last), n = end - start;
    int      caret = d->caret, anchor = d->anchor, nl = d->crlf ? 2 : 1;
    RC_Arena tmp;
    char    *o = block_new(&tmp, (size_t)(nl + n));

    if (!o) {
        rcArenaFree(&tmp);
        return;
    }
    move_bytes(o, d->crlf ? "\r\n" : "\n", nl);
    move_bytes(o + nl, d->text + start, n);
    if (doc_edit(d, end, 0, o, nl + n, EDIT_OTHER)) {
        d->caret  = caret + nl + n;
        d->anchor = anchor + nl + n;
    }
    rcArenaFree(&tmp);
    st->reveal = REVEAL_NEAREST;
}

/* Copy what is selected or, with nothing selected, the caret's line, as editors do. */
static inline void copy_selection(Doc *d, bool cut) {
    int      ln = doc_line_of(d, d->caret);
    int      a  = has_sel(d) ? sel_start(d) : d->line[ln];
    int      b  = has_sel(d) ? sel_end(d) : ln + 1 < d->lines ? d->line[ln + 1] : d->len;
    RC_Arena tmp;
    char    *o = block_new(&tmp, (size_t)(b - a) + 1);

    if (!o) {
        rcArenaFree(&tmp);
        return;
    }
    move_bytes(o, d->text + a, b - a);
    o[b - a] = '\0';
    rcClipboardSet(o);
    rcArenaFree(&tmp);
    if (cut)
        doc_edit(d, a, b - a, "", 0, EDIT_OTHER);
}

/* ---- Typing. ---- */

/* Enter keeps the line's indent, one deeper after an opening brace. */
static inline void insert_newline(Doc *d) {
    int   ln = doc_line_of(d, sel_start(d)), start = d->line[ln], n = 0, i = 0;
    char  buf[256];
    int   col = sel_start(d) - start;

    if (d->crlf)
        buf[n++] = '\r';
    buf[n++] = '\n';
    while (i < col && is_blank(d->text[start + i]) && n < 240)
        buf[n++] = d->text[start + i++];
    if (col > 0 && d->text[start + col - 1] == '{')
        for (i = 0; i < TAB_COLUMNS; i++)
            buf[n++] = ' ';
    doc_insert(d, buf, n, EDIT_OTHER);
}

/* A closing brace typed on a blank line steps back one level. */
static inline void insert_close_brace(Doc *d) {
    int ln = doc_line_of(d, d->caret), start = d->line[ln], col = d->caret - start, i = 0;

    while (i < col && d->text[start + i] == ' ')
        i++;
    if (!has_sel(d) && i == col && col >= TAB_COLUMNS)
        doc_edit(d, d->caret - TAB_COLUMNS, TAB_COLUMNS, "}", 1, EDIT_OTHER);
    else
        doc_insert(d, "}", 1, EDIT_TYPING);
}

/* Move what the field received into the document, and empty the field. */
static inline void take_typed(AppState *st, Doc *d) {
    const char *s = st->sink;
    int         n = str_len(s);

    st->chordK = false;
    if (st->swallowFrames && (s[0] == 'z' || s[0] == 'Z')) {    /* the chord's own letter */
        s++;
        n--;
        st->swallowFrames = 0;
    }
    if (same_text(s, "\n"))
        insert_newline(d);
    else if (same_text(s, "}") && d->lang == LANG_C)
        insert_close_brace(d);
    else if (n)
        doc_insert(d, s, n, n == 1 ? EDIT_TYPING : EDIT_OTHER);
    st->sink[0] = '\0';
    d->wantX    = -1.0f;
    st->reveal  = REVEAL_NEAREST;
}

/* rcKeyPressed is one edge per press. A held arrow or Backspace repeats as it does in every
   field, at the user's own delay and rate: rcKeyEvent hears the press and each auto-repeat. */
static inline bool key_fires(RC_Key key) {
    RC_KeyEvent e;
    int         i;

    for (i = 0; rcKeyEvent(i, &e); i++)
        if (e.key == key)
            return true;
    return false;
}

/* Backspace in the indent removes a whole level, so the caret stays on the 4-column grid. */
static inline int backspace_target(const Doc *d) {
    int start = d->line[doc_line_of(d, d->caret)], col = d->caret - start, i = 0;

    while (i < col && d->text[start + i] == ' ')
        i++;
    if (col > 0 && i == col)
        return d->caret - ((col - 1) % TAB_COLUMNS + 1);
    return step_left(d, d->caret);
}

/* The keys a field leaves to its app. On a Mac Cmd+arrow means line ends and Option+arrow a
   word; elsewhere Ctrl+arrow a word. Reading the physical keys needs no platform test. */
static inline void editor_keys(AppState *st, Doc *d) {
    bool shift = rcModDown(RC_MOD_SHIFT), alt = rcModDown(RC_MOD_ALT);
    bool ctrl  = rcModDown(RC_MOD_CTRL), cmd = rcModDown(RC_MOD_SUPER);
    bool word  = ctrl || alt;

    if (key_fires(RC_KEY_LEFT)) {
        if (cmd)
            move_home(st, d, shift);
        else if (word)
            set_caret(st, d, word_left(d, d->caret), shift);
        else if (has_sel(d) && !shift)
            set_caret(st, d, sel_start(d), false);
        else
            set_caret(st, d, step_left(d, d->caret), shift);
    }
    if (key_fires(RC_KEY_RIGHT)) {
        if (cmd)
            set_caret(st, d, line_end_offset(d, doc_line_of(d, d->caret)), shift);
        else if (word)
            set_caret(st, d, word_right(d, d->caret), shift);
        else if (has_sel(d) && !shift)
            set_caret(st, d, sel_end(d), false);
        else
            set_caret(st, d, step_right(d, d->caret), shift);
    }
    if (!alt && key_fires(RC_KEY_UP)) {
        if (cmd)
            set_caret(st, d, 0, shift);
        else if (ctrl)
            rcScrollBy(d->scrollId, 0.0f, -(float)LINE_H);
        else
            move_vertical(st, d, -1, shift);
    }
    if (!alt && key_fires(RC_KEY_DOWN)) {
        if (cmd)
            set_caret(st, d, d->len, shift);
        else if (ctrl)
            rcScrollBy(d->scrollId, 0.0f, (float)LINE_H);
        else
            move_vertical(st, d, 1, shift);
    }
    if (!rcModDown(RC_MOD_PRIMARY) && key_fires(RC_KEY_PAGE_UP))
        move_vertical(st, d, -view_lines(d), shift);
    if (!rcModDown(RC_MOD_PRIMARY) && key_fires(RC_KEY_PAGE_DOWN))
        move_vertical(st, d, view_lines(d), shift);
    if (rcKeyPressed(RC_KEY_HOME)) {
        if (rcModDown(RC_MOD_PRIMARY))
            set_caret(st, d, 0, shift);
        else
            move_home(st, d, shift);
    }
    if (rcKeyPressed(RC_KEY_END)) {
        if (rcModDown(RC_MOD_PRIMARY))
            set_caret(st, d, d->len, shift);
        else
            set_caret(st, d, line_end_offset(d, doc_line_of(d, d->caret)), shift);
    }
    if (key_fires(RC_KEY_BACKSPACE)) {
        if (has_sel(d)) {
            doc_insert(d, "", 0, EDIT_OTHER);
        } else {
            int to = word ? word_left(d, d->caret) : backspace_target(d);

            if (to != d->caret)
                doc_edit(d, to, d->caret - to, "", 0, EDIT_OTHER);
        }
        st->reveal = REVEAL_NEAREST;
    }
    if (key_fires(RC_KEY_DELETE)) {
        if (has_sel(d)) {
            doc_insert(d, "", 0, EDIT_OTHER);
        } else {
            int to = word ? word_right(d, d->caret) : step_right(d, d->caret);

            if (to != d->caret)
                doc_edit(d, d->caret, to - d->caret, "", 0, EDIT_OTHER);
        }
        st->reveal = REVEAL_NEAREST;
    }
    /* Where a stop comes before the field (a menu, a filter), Shift+Tab moves focus back to it
       and rcKeyPressed does not read that press: the field's own keydown does (rcKeyPressedOn),
       and the focus is kept here. */
    if ((rcKeyPressed(RC_KEY_TAB) || (shift && rcKeyPressedOn(SINK_ID, RC_KEY_TAB))) && !ctrl) {
        int first = doc_line_of(d, sel_start(d)), last = doc_line_of(d, sel_end(d));

        if (shift) {
            edit_block(d, BLOCK_OUTDENT);
            rcSetFocus(SINK_ID);
        } else if (first != last) {
            edit_block(d, BLOCK_INDENT);
        } else {
            doc_insert(d, "    ", TAB_COLUMNS, EDIT_OTHER);
        }
        st->reveal = REVEAL_NEAREST;
    }
}

/* ---- The mouse. ---- */

static inline int position_at_pointer(const AppState *st, const Doc *d) {
    RC_Box        box = rcGetElementBox(d->scrollId);
    RC_ScrollInfo si  = rcGetScrollInfo(d->scrollId);
    RC_Vec2       p   = rcPointer();
    int           ln  = (int)((p.y - box.y + si.offsetY) / (float)LINE_H);

    if (p.y < box.y && si.offsetY <= 0.0f)
        return 0;
    if (ln >= d->lines)
        return d->len;
    ln = ln < 0 ? 0 : ln;
    return d->line[ln] + byte_at_x(st, d, ln, p.x - box.x + si.offsetX - (float)CODE_PAD);
}

/* A double click takes the word, a triple click the line, and a drag from either keeps
   growing by whole words or lines, as in every editor. */
static inline void select_clicked(Doc *d, int at, int clicks, int *from, int *to) {
    int ln = doc_line_of(d, at);

    *from = *to = at;
    if (clicks == 2) {
        while (*from > 0 && is_word(d->text[*from - 1]))
            (*from)--;
        while (*to < d->len && is_word(d->text[*to]))
            (*to)++;
    } else if (clicks >= 3) {
        *from = d->line[ln];
        *to   = ln + 1 < d->lines ? d->line[ln + 1] : d->len;
    }
}

static inline void editor_mouse(AppState *st, Doc *d) {
    bool over = rcIsHovered(d->scrollId);

    if (over)
        rcSetCursor(RC_CURSOR_TEXT);
    if (rcPointerPressed(RC_POINTER_LEFT) && over) {
        int at = position_at_pointer(st, d);

        /* The press's count in a row, as the web's event.detail: three or more takes the line. */
        st->clicks        = rcClickCount();
        st->dragging      = true;
        st->wantCaret     = true;
        select_clicked(d, at, st->clicks, &st->dragStart, &st->dragEnd);
        if (rcModDown(RC_MOD_SHIFT)) {
            set_caret(st, d, at, true);
        } else {
            d->anchor = st->dragStart;
            set_caret(st, d, st->dragEnd, true);
        }
    } else if (st->dragging && rcPointerDown(RC_POINTER_LEFT)) {
        RC_Box box = rcGetElementBox(d->scrollId);
        int    at  = position_at_pointer(st, d), from, to;

        select_clicked(d, at, st->clicks, &from, &to);
        if (at < st->dragStart) {
            d->anchor = st->dragEnd;
            d->caret  = from;
        } else {
            d->anchor = st->dragStart;
            d->caret  = to > st->dragEnd ? to : st->dragEnd;
        }
        d->wantX = -1.0f;
        /* Held past an edge the view keeps moving under the selection. */
        if (rcPointer().y < box.y)
            rcScrollBy(d->scrollId, 0.0f, -(float)LINE_H);
        else if (rcPointer().y > box.y + box.height)
            rcScrollBy(d->scrollId, 0.0f, (float)LINE_H);
    } else if (st->dragging) {
        st->dragging = false;
    }
    if (rcPointerPressed(RC_POINTER_LEFT) && rcIsHovered(GUTTER_ID)) {
        RC_Box        box = rcGetElementBox(d->scrollId);
        RC_ScrollInfo si  = rcGetScrollInfo(d->scrollId);
        int           ln  = (int)((rcPointer().y - box.y + si.offsetY) / (float)LINE_H);

        if (ln >= 0 && ln < d->lines) {
            d->anchor = d->line[ln];
            set_caret(st, d, ln + 1 < d->lines ? d->line[ln + 1] : d->len, true);
            st->wantCaret = true;
        }
    }
}

/* The pair the caret touches, scanned outward through a bounded stretch of text. */
static inline int bracket_pair(const Doc *d, int at) {
    static const char PAIRS[] = "()[]{}";
    char c = d->text[at];
    int  i, dir, depth = 1;
    char open, close;

    for (i = 0; i < 6; i++)
        if (PAIRS[i] == c)
            break;
    if (i == 6 || at >= d->len)
        return -1;
    dir   = i % 2 ? -1 : 1;
    open  = c;
    close = PAIRS[i + dir];
    for (i = at + dir; i >= 0 && i < d->len && i > at - 20000 && i < at + 20000; i += dir) {
        depth += d->text[i] == open;
        depth -= d->text[i] == close;
        if (!depth)
            return i;
    }
    return -1;
}

/* The word the caret stands in: its other uses on screen get a quiet wash. */
static inline void update_word(AppState *st, const Doc *d) {
    int a = d->caret, b = d->caret;

    st->wordLen = 0;
    if (has_sel(d))
        return;
    while (a > 0 && is_word(d->text[a - 1]))
        a--;
    while (b < d->len && is_word(d->text[b]))
        b++;
    if (b - a >= 2) {
        st->wordAt  = a;
        st->wordLen = b - a;
    }
}

static inline void update_brackets(AppState *st, const Doc *d) {
    int at = d->caret, pair = -1;

    st->bracketA = st->bracketB = -1;
    if (has_sel(d))
        return;
    if (at < d->len)
        pair = bracket_pair(d, at);
    if (pair < 0 && at > 0) {
        at--;
        pair = bracket_pair(d, at);
    }
    if (pair >= 0) {
        st->bracketA = at;
        st->bracketB = pair;
    }
}

/* ---- Find in the file. ---- */

static inline void find_refresh(AppState *st, const Doc *d) {
    Find *f  = &st->find;
    int   qn = str_len(f->query), i;

    if (same_text(f->seen, f->query) && f->seenCase == f->matchCase && f->seenWord == f->wholeWord
        && f->seenRev == d->rev && f->seenDoc == st->active)
        return;
    rcStrCopy(f->seen, f->query, sizeof f->seen);
    f->seenCase = f->matchCase;
    f->seenWord = f->wholeWord;
    f->seenRev  = d->rev;
    f->seenDoc  = st->active;
    f->nHits    = 0;
    f->current  = -1;
    if (!qn)
        return;
    for (i = 0; i + qn <= d->len && f->nHits < MAX_HITS; i++) {
        if (!text_at(d->text + i, f->query, qn, f->matchCase))
            continue;
        if (f->wholeWord && ((i > 0 && is_word(d->text[i - 1]))
                             || (i + qn < d->len && is_word(d->text[i + qn]))))
            continue;
        f->hits[f->nHits++] = i;
        if (f->current < 0 && i >= sel_start(d))
            f->current = f->nHits - 1;
        i += qn - 1;
    }
    if (f->current < 0 && f->nHits)
        f->current = 0;
}

/* Select a hit: the caret lands after it, so the next search starts from there. */
static inline void find_select(AppState *st, Doc *d) {
    Find *f = &st->find;

    if (f->current < 0 || f->current >= f->nHits)
        return;
    d->anchor = f->hits[f->current];
    set_caret(st, d, f->hits[f->current] + str_len(f->query), true);
    st->reveal = REVEAL_CENTER;
}

static inline void find_step(AppState *st, Doc *d, int dir) {
    Find *f = &st->find;

    find_refresh(st, d);
    if (!f->nHits)
        return;
    f->current = (f->current + dir + f->nHits) % f->nHits;
    find_select(st, d);
}

static inline void replace_current(AppState *st, Doc *d) {
    Find *f = &st->find;
    int   qn = str_len(f->query);

    find_refresh(st, d);
    if (f->current < 0)
        return;
    d->anchor = d->caret = f->hits[f->current];
    doc_edit(d, f->hits[f->current], qn, f->with, str_len(f->with), EDIT_OTHER);
    find_refresh(st, d);
    find_select(st, d);
}

/* One buffer rebuilt, so Replace All is one undo however many hits it had. */
static inline void replace_all(AppState *st, Doc *d) {
    Find    *f = &st->find;
    int      qn = str_len(f->query), wn = str_len(f->with), i, at = 0, out = 0, count;
    RC_Arena tmp;
    char    *o;
    bool     replaced;

    find_refresh(st, d);
    count = f->nHits;
    if (!count)
        return;
    o = block_new(&tmp, (size_t)d->len + (size_t)count * (size_t)(wn + 1) + 1);
    if (!o) {
        rcArenaFree(&tmp);
        return;
    }
    for (i = 0; i < count; i++) {
        move_bytes(o + out, d->text + at, f->hits[i] - at);
        out += f->hits[i] - at;
        move_bytes(o + out, f->with, wn);
        out += wn;
        at = f->hits[i] + qn;
    }
    move_bytes(o + out, d->text + at, d->len - at);
    out += d->len - at;
    d->caret = d->anchor = 0;
    replaced = doc_edit(d, 0, d->len, o, out, EDIT_OTHER);
    d->caret = d->anchor = 0;
    rcArenaFree(&tmp);
    if (replaced)
        out_log(st, SEV_INFO, rcFormat(st->arena, "Replaced %d occurrence%s in %s", count,
                                       count == 1 ? "" : "s", d->name).chars);
}

/* ---- Drawing a line. ---- */

/* One run of code. A run borrows the document's bytes: nothing edits them until next frame.
   The string is marked as not static, so its width is cached by its content. */
static inline void code_text(const AppState *st, const char *s, int n, RC_Color color) {
    static const char SPACES[] = "    ";
    int               i = 0;

    while (i < n) {
        int j = i;

        while (j < n && s[j] != '\t')
            j++;
        if (j > i) {
            RC_String run = { .isStaticallyAllocated = false, .length = j - i, .chars = s + i };

            rcText(run, .font = st->mono, .color = color, .size = SZ_CODE, .lineHeight = LINE_H,
                   .wrap = "n", .select = RC_SELECT_NONE);
        }
        if (j < n) {
            RC_String run = { .isStaticallyAllocated = true, .length = TAB_COLUMNS,
                              .chars = SPACES };

            rcText(run, .font = st->mono, .color = color, .size = SZ_CODE, .lineHeight = LINE_H,
                   .wrap = "n", .select = RC_SELECT_NONE);
            j++;
        }
        i = j;
    }
}

/* A token cut where the marks behind it change. */
static inline void code_span(const AppState *st, const char *s, int n, const uint8_t *marks,
                             RC_Color color) {
    int i = 0;

    while (i < n) {
        int j = i + 1;

        while (j < n && marks[j] == marks[i])
            j++;
        if (marks[i] == MARK_NONE) {
            code_text(st, s + i, j - i, color);
        } else {
            rcBox(.bg = mark_color(marks[i]), .hType = RC_PX(LINE_H)) {
                code_text(st, s + i, j - i, color);
            }
        }
        i = j;
    }
}

static inline void fill_marks(AppState *st, const Doc *d, int start, int n) {
    const Find *f = &st->find;
    int         a = sel_start(d), b = sel_end(d), i;

    for (i = 0; i < n; i++)
        st->marks[i] = MARK_NONE;
    /* Whole-word uses only: "line" lights no part of "lineState". */
    for (i = 0; st->wordLen && i + st->wordLen <= n; i++) {
        if (same_bytes(d->text + start + i, d->text + st->wordAt, st->wordLen)
            && !(start + i > 0 && is_word(d->text[start + i - 1]))
            && !is_word(d->text[start + i + st->wordLen])) {
            int k;

            for (k = 0; k < st->wordLen; k++)
                st->marks[i + k] = MARK_WORD;
            i += st->wordLen - 1;
        }
    }
    if (st->bracketA >= start && st->bracketA < start + n)
        st->marks[st->bracketA - start] = MARK_BRACKET;
    if (st->bracketB >= start && st->bracketB < start + n)
        st->marks[st->bracketB - start] = MARK_BRACKET;
    if (a != b)
        for (i = a > start ? a - start : 0; i < n && start + i < b; i++)
            st->marks[i] = MARK_SELECTION;
    if (f->open && f->nHits) {
        int qn = str_len(f->query), lo = 0, hi = f->nHits;

        while (lo < hi) {
            int mid = (lo + hi) / 2;

            if (f->hits[mid] + qn <= start)
                lo = mid + 1;
            else
                hi = mid;
        }
        for (i = lo; i < f->nHits && f->hits[i] < start + n; i++) {
            int k;

            for (k = f->hits[i]; k < f->hits[i] + qn; k++)
                if (k >= start && k < start + n)
                    st->marks[k - start] = i == f->current ? MARK_CURRENT : MARK_MATCH;
        }
    }
}

/* How deep a blank line's guides run: as deep as the shallower of its neighbors. */
static inline int line_indent(const Doc *d, int ln) {
    const char *s = d->text + d->line[ln];
    int         n = doc_line_len(d, ln), i = 0;

    while (i < n && s[i] == ' ')
        i++;
    return i == n ? -1 : i;
}

static inline int guide_levels(const Doc *d, int ln) {
    int indent = line_indent(d, ln), a = ln, b = ln, ia = -1, ib = -1;

    if (indent >= 0)
        return indent / TAB_COLUMNS;
    while (--a >= 0 && ia < 0 && ln - a < 32)
        ia = line_indent(d, a);
    while (++b < d->lines && ib < 0 && b - ln < 32)
        ib = line_indent(d, b);
    indent = ia < ib ? ia : ib;
    return indent > 0 ? indent / TAB_COLUMNS : 0;
}

/* Indent guides are in-flow boxes, each as wide as one level of the run it stands in for. */
static inline void guide_box(const AppState *st, RC_Color bg) {
    rcRow(.bg = bg, .wType = RC_PX((float)TAB_COLUMNS * st->cell), .hType = RC_PX(LINE_H)) {
        rcBox(.bg = theme()->guide, .h = "grow", .wType = RC_PX(1)) {}
    }
}

static inline bool marks_uniform(const uint8_t *marks, int n) {
    int i;

    for (i = 1; i < n; i++)
        if (marks[i] != marks[0])
            return false;
    return true;
}

/* A problem underlines its place in the code, floating on the row's bottom edge. */
static inline void squiggles(const AppState *st, const Doc *d, int ln) {
    const Palette *t = theme();
    int            i;

    for (i = 0; i < d->nProb; i++) {
        const Problem *p = &d->prob[i];

        if (p->line == ln && p->severity != SEV_INFO) {
            int   n = doc_line_len(d, ln), col = p->col > n ? n : p->col;
            float x = line_x(st, d, ln, col) + (float)CODE_PAD;
            float w = line_x(st, d, ln, n) + (float)CODE_PAD - x;

            rcBox(.bg = p->severity == SEV_ERROR ? t->error : t->warning,
                  .wType = RC_PX(w < st->cell ? st->cell : w), .hType = RC_PX(2),
                  .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_BOTTOM_LEFT,
                                .element = RC_ANCHOR_BOTTOM_LEFT, .offset = { x, 0.0f },
                                .capture = RC_CAPTURE_PASSTHROUGH }) {}
        }
    }
}

static inline void code_row(AppState *st, const Doc *d, int ln, float width) {
    const Palette *t = theme();
    bool           cur = ln < d->lines && !has_sel(d) && ln == doc_line_of(d, d->caret);

    rcRow(.bg = cur ? t->curLine : RC_UNSET_COLOR, .pl = CODE_PAD, .align = "cl", .w = "grow",
          .hType = RC_PX(LINE_H), .wMin = RC_VAL(width)) {
        if (ln < d->lines) {
            const char *s = d->text + d->line[ln];
            int         n = doc_line_len(d, ln);
            int         shown = n < MARK_BYTES ? n : MARK_BYTES;
            int         levels = guide_levels(d, ln), lead = 0, tk;
            uint8_t     state = d->lineState[ln];
            Token       toks[192];
            TokenList   l = { toks, 192, 0 };

            fill_marks(st, d, d->line[ln], shown);
            while (lead < levels && (lead + 1) * TAB_COLUMNS <= shown
                   && marks_uniform(st->marks + lead * TAB_COLUMNS, TAB_COLUMNS)
                   && s[lead * TAB_COLUMNS] == ' ') {
                guide_box(st, mark_color(st->marks[lead * TAB_COLUMNS]));
                lead++;
            }
            if (!n)
                for (tk = 0; tk < levels; tk++)
                    guide_box(st, RC_UNSET_COLOR);
            tokenize_line(d->lang, s, shown, &state, &l);
            for (tk = 0; tk < l.n; tk++) {
                int a = toks[tk].start, b = toks[tk].end;

                a = a < lead * TAB_COLUMNS ? lead * TAB_COLUMNS : a;
                if (b > a)
                    code_span(st, s + a, b - a, st->marks + a, t->syntax[toks[tk].kind]);
            }
            /* A selected line ending shows as a stub of selection past the text. */
            if (has_sel(d) && sel_start(d) <= line_end_offset(d, ln)
                && sel_end(d) > line_end_offset(d, ln)) {
                rcBox(.bg = t->selection, .wType = RC_PX(st->cell * 0.6f),
                      .hType = RC_PX(LINE_H)) {}
            }
            squiggles(st, d, ln);
        }
    }
}

/* The numbers are their own column, out of the code's horizontal scroll, drawn from the code
   view's offset. A float inside it carries them, so the column clips the part-lines. */
static inline float gutter_width(const AppState *st, const Doc *d) {
    int digits = 1, n = d->lines;

    while (n >= 10) {
        n /= 10;
        digits++;
    }
    digits = digits < 3 ? 3 : digits;
    return (float)(st->compact ? 8 : 16) + (float)digits * st->cell + 12.0f;
}

static inline RC_Color line_problem_color(const Doc *d, int ln, RC_Color normal) {
    const Palette *t = theme();
    int            i;

    for (i = 0; i < d->nProb; i++) {
        if (d->prob[i].line == ln && d->prob[i].severity == SEV_ERROR)
            return t->error;
        if (d->prob[i].line == ln && d->prob[i].severity == SEV_WARNING)
            normal = t->warning;
    }
    return normal;
}

static inline void gutter_view(AppState *st, const Doc *d, float w, float offsetY) {
    const Palette *t       = theme();
    int            caretLn = doc_line_of(d, d->caret);
    int            first   = (int)(offsetY / (float)LINE_H), i;
    int            count   = view_lines(d) + 2;
    float          shift   = offsetY - (float)first * (float)LINE_H;

    rcColumn(.id = GUTTER_ID, .bg = t->editor, .overflow = "hidden", .h = "grow",
             .wType = RC_PX(w)) {
        rcColumn(.wType = RC_PX(w),
                 .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                               .element = RC_ANCHOR_TOP_LEFT, .offset = { 0.0f, -shift },
                               .capture = RC_CAPTURE_PASSTHROUGH, .clip = RC_CLIP_TO_PARENT }) {
            for (i = first; i < first + count && i < d->lines; i++) {
                RC_Color c = line_problem_color(d, i, i == caretLn ? t->gutterOn : t->gutter);

                rcRow(.bg = i == caretLn && !has_sel(d) ? t->curLine : RC_UNSET_COLOR, .pr = 12,
                      .align = "cr", .wType = RC_PX(w), .hType = RC_PX(LINE_H)) {
                    rcText(rcFormat(st->arena, "%d", i + 1), .font = st->mono, .color = c,
                           .size = SZ_CODE, .lineHeight = LINE_H, .wrap = "n",
                           .select = RC_SELECT_NONE);
                }
            }
        }
    }
}

/* Scroll the caret into view: from outside the frame's declarations, so it lands next frame. */
static inline void apply_reveal(AppState *st, Doc *d) {
    RC_ScrollInfo si;
    RC_Box        box;
    int           ln;
    float         y, x, dx = 0.0f, dy = 0.0f, viewW, viewH;

    si  = rcGetScrollInfo(d->scrollId);
    box = rcGetElementBox(d->scrollId);
    if (!si.found || !box.found) {
        if (st->reveal != REVEAL_NONE)
            rcWindowRequestFrame(rcAppMainWindow(st->app));
        return;
    }
    ln = doc_line_of(d, d->caret);
    y  = (float)ln * (float)LINE_H;
    x  = line_x(st, d, ln, d->caret - d->line[ln]) + (float)CODE_PAD;
    viewW = box.width;
    /* The sideways bar lies over the view's bottom edge, so a line under it is not in view. */
    viewH = si.maxOffsetX > 0.0f ? box.height - (float)SIDE_BAR_H : box.height;
    /* A panel opened or a window shortened must not leave behind a caret that was on show. */
    if (st->viewH != box.height) {
        if (st->reveal == REVEAL_NONE && y >= si.offsetY && y < si.offsetY + st->viewH)
            st->reveal = REVEAL_NEAREST;
        st->viewH = box.height;
    }
    if (st->reveal == REVEAL_NONE)
        return;
    rcWindowRequestFrame(rcAppMainWindow(st->app));
    if (st->reveal == REVEAL_CENTER && (y < si.offsetY || y + (float)LINE_H > si.offsetY + viewH))
        dy = y - si.offsetY - viewH * 0.4f;
    else if (y < si.offsetY)
        dy = y - si.offsetY;
    else if (y + (float)LINE_H > si.offsetY + viewH)
        dy = y + (float)LINE_H - si.offsetY - viewH;
    if (x < si.offsetX + (float)CODE_PAD)
        dx = x - si.offsetX - (float)CODE_PAD * 4.0f;
    else if (x + st->cell * 4.0f > si.offsetX + viewW)
        dx = x + st->cell * 8.0f - si.offsetX - viewW;
    if (dx != 0.0f || dy != 0.0f)
        rcScrollBy(d->scrollId, dx, dy);
    st->reveal = REVEAL_NONE;
}

/* The minimap: each line a row of tiny colored bars, one per token. Short tokens merge into
   the bar before them, so a dense line stays a few boxes. A press or a drag scrolls there. */
static inline void minimap_row(const Doc *d, int ln) {
    const Palette *t = theme();

    rcRow(.align = "tl", .wType = RC_PX(MINIMAP_W), .hType = RC_PX(MINI_LINE)) {
        const char *s = d->text + d->line[ln];
        int         n = doc_line_len(d, ln), tk;
        uint8_t     state = d->lineState[ln];
        Token       toks[96];
        TokenList   l = { toks, 96, 0 };

        n = n > MINIMAP_W ? MINIMAP_W : n;
        tokenize_line(d->lang, s, n, &state, &l);
        for (tk = 0; tk < l.n; tk++) {
            int  a = toks[tk].start, b = toks[tk].end, kind = toks[tk].kind;
            bool blank = true;
            int  i;

            for (i = a; i < b; i++)
                blank = blank && is_blank(s[i]);
            while (tk + 1 < l.n && toks[tk + 1].end - toks[tk + 1].start < 2) {
                tk++;
                b = toks[tk].end;
            }
            rcBox(.bg = blank ? RC_UNSET_COLOR : rcWithAlpha(t->syntax[kind], 0.59f),
                  .wType = RC_PX((float)(b - a) * MINI_CELL), .hType = RC_PX(2)) {}
        }
    }
}

static inline void minimap_view(AppState *st, Doc *d, float offsetY, float maxOffset) {
    const Palette *t      = theme();
    RC_Box         own    = rcGetElementBox("minimap");
    float          height = own.found ? own.height : 600.0f;
    int            rows   = (int)(height / (float)MINI_LINE);
    int            viewN  = view_lines(d);
    float          frac   = maxOffset > 0.0f ? offsetY / maxOffset : 0.0f;
    int            top    = d->lines > rows ? (int)((float)(d->lines - rows) * frac) : 0;
    float          topY   = ((float)(offsetY / (float)LINE_H) - (float)top) * (float)MINI_LINE;
    int            i;

    /* A press goes to the line under it and starts a drag, which lasts until the button lifts,
       off the strip too. The drag reads the map as it stood at the press: the map scrolls with
       the view, and a held pointer chasing it would never come to rest. */
    if (rcPressed("minimap")) {
        st->miniDrag = true;
        st->miniTop  = top;
    } else if (!rcPointerDown(RC_POINTER_LEFT)) {
        st->miniDrag = false;
    }
    if (st->miniDrag && own.found) {
        float y = rcPointer().y - own.y;
        int   target = st->miniTop + (int)(y / (float)MINI_LINE) - viewN / 2;
        float want = (float)(target < 0 ? 0 : target) * (float)LINE_H;

        rcScrollBy(d->scrollId, 0.0f, want - offsetY);
    }
    rcColumn(.id = "minimap", .bg = t->editor, .overflow = "hidden", .h = "grow",
             .wType = RC_PX(MINIMAP_W)) {
        for (i = top; i < top + rows && i < d->lines; i++)
            minimap_row(d, i);
        rcBox(.bg = rcWithAlpha(t->text, rcShowsHover("minimap") ? 0.157f : 0.094f),
              .wType = RC_PX(MINIMAP_W), .hType = RC_PX((float)viewN * (float)MINI_LINE),
              .floating = { .to = RC_ATTACH_PARENT, .parent = RC_ANCHOR_TOP_LEFT,
                            .element = RC_ANCHOR_TOP_LEFT, .offset = { 0.0f, topY },
                            .capture = RC_CAPTURE_PASSTHROUGH, .clip = RC_CLIP_TO_PARENT }) {}
    }
}

/* ---- A field's or a button's height. ---- */

/* Its height class. An h-* is exact and drops the 44 px finger floor (mobile.md), so under a
   finger or on a phone the class is h-11, that floor (HIT); at a desk it is the dense size. */
static inline const char *control_h(const AppState *st, const char *desk) {
    return rcPointerIsCoarse() || st->compact ? "h-11" : desk;
}

/* ---- The find bar. ---- */

static inline bool toggle_chip(const char *id, const char *label, bool on, const char *tip) {
    const Palette *t = theme();

    rcBox(.id = id, .bg = on ? rcWithAlpha(t->accent, 0.353f) : RC_UNSET_COLOR, .align = "cc",
          .borderRadius = "all-sm",
          .border = { .color = on ? t->accent : RC_UNSET_COLOR, .width = 1 },
          .wType = RC_PX(24), .hType = RC_PX(22), .tooltip = tip,
          .className = "hover:bg-surface-alt") {
        rcTextC(label, .color = on ? t->bright : t->text, .size = SZ_SMALL, .wrap = "n",
                .select = RC_SELECT_NONE);
    }
    return rcClicked(id);
}

static inline bool icon_chip(const char *id, RC_IconCallback icon, bool disabled, const char *tip) {
    const Palette *t = theme();

    /* A disabled box is in no other state, so its class shows no hover. */
    rcBox(.id = id, .align = "cc", .borderRadius = "all-sm", .disabled = disabled,
          .wType = RC_PX(24), .hType = RC_PX(24), .tooltip = tip,
          .className = "hover:bg-surface-alt") {
        icon(16.0f, disabled ? t->muted : t->text);
    }
    return rcClicked(id);
}

#endif /* APP_EDITOR_H */
