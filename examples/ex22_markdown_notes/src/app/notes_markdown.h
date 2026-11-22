/*
    notes_markdown.h - Markdown rendered by walking the text and declaring elements.

    THERE IS NO DOCUMENT TREE. An immediate-mode UI redraws the whole window from your
    state every frame, so the renderer is one pass over the note's text: each line is
    classified and declared on the spot, and an edit is visible on the very next frame
    with nothing to invalidate. Every text run borrows its bytes straight from the
    note's body, so a frame copies no text at all (rcText does not copy, and the body
    outlives the frame).

    A PARAGRAPH WITH NO MARKUP IS ONE WRAPPED RUN, which is the cheapest thing the
    library can draw. A text run has one face, so a paragraph that mixes faces breaks
    its own lines with rcTextWidth and declares each line as an `items-baseline` row of
    runs, so bold, italic, code chips and tag pills share a baseline. The page pays for
    that only where it is used, and a line holds one run per change of style rather
    than one per word: a run keeps its spaces exact, and each new run starts on a whole
    pixel, so a row of single words spaces unevenly.

    Included by notes_app.c, the one TU that needs it.
*/
#ifndef NOTES_MARKDOWN_H
#define NOTES_MARKDOWN_H

#include "notes_app.h"
#include "notes_theme.h"

#include "icons/rc_icons_check.h"
#include "icons/rc_icons_circle_alert.h"
#include "icons/rc_icons_copy.h"
#include "icons/rc_icons_info.h"
#include "icons/rc_icons_triangle_alert.h"

/* What a click in the page asks of the app (notes_app.c). */
static void notes_open(AppState *st, int index);
static void notes_show_tag(AppState *st, const char *path, int length);
static void notes_create_titled(AppState *st, const char *title, int length);

/* One rendering of one note: the Preview, the Split's right pane, the reader and an
   extra window each hold their own, so their element ids never collide. */
typedef struct {
    AppState            *st;
    const NotesPalette  *t;
    const NotesTypeStep *rs;
    RC_Arena            *arena;
    Note                *note;
    const char          *prefix;   /* the id namespace of this rendering          */
    float                width;    /* the column's width, for a table's columns   */
    bool                 readOnly; /* tasks do not tick, links do not navigate    */
    int                  nextId;
    int                  nextHeading;
} MdPage;

/* A run's look. Passed by pointer through the inline pass. */
typedef struct {
    uint16_t font;
    uint16_t size;
    uint16_t line;
    RC_Color color;
    float    width;      /* the line's width, where the app breaks the line itself */
} MdRun;

/* -- the inline pass: one line into flat segments ---------------------------------- */

enum {
    MD_BOLD   = 1 << 0,
    MD_ITALIC = 1 << 1,
    MD_MARK   = 1 << 2,
    MD_CODE   = 1 << 3,
    MD_KBD    = 1 << 4,
    MD_LINK   = 1 << 5,
    MD_TAG    = 1 << 6,
    MD_BREAK  = 1 << 7
};

/* A word with its trailing space, or an unbreakable span such as a code chip. `glued`
   means no space follows it, so it must stay on one line with the next segment: the
   period after a bold word is not allowed to wrap onto a line of its own. */
typedef struct {
    const char *s;
    int         len;
    int         style;
    int         link;
    bool        glued;
} MdSeg;

typedef struct {
    const char *target;
    int         len;
    bool        wiki;
} MdLink;

#define MD_MAX_SEGS  512
#define MD_MAX_LINKS 16

typedef struct {
    MdSeg  seg[MD_MAX_SEGS];
    int    count;
    MdLink link[MD_MAX_LINKS];
    int    linkCount;
    bool   full;     /* a run had no room: the line draws as plain text (md_inline) */
} MdLine;

static RC_String md_slice(const char *s, int len) {
    RC_String r = rcStringFromCStr("");

    r.chars  = s;
    r.length = len;
    return r;
}

static bool md_at(const char *p, const char *e, const char *token) {
    while (*token) {
        if (p >= e || *p != *token)
            return false;
        p++;
        token++;
    }
    return true;
}

static const char *md_find(const char *p, const char *e, const char *token) {
    for (; p < e; p++)
        if (md_at(p, e, token))
            return p;
    return NULL;
}

static bool md_is_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool md_is_digit(char c) { return c >= '0' && c <= '9'; }

static bool md_is_word(char c) { return md_is_letter(c) || md_is_digit(c) || c == '_'; }

static void md_add_words(MdLine *l, const char *s, const char *e, int style, int link) {
    while (s < e && l->count < MD_MAX_SEGS) {
        const char *w = s;
        MdSeg      *g = &l->seg[l->count++];

        while (w < e && *w != ' ')
            w++;
        while (w < e && *w == ' ')
            w++;
        g->s     = s;
        g->len   = (int)(w - s);
        g->style = style;
        g->link  = link;
        g->glued = w[-1] != ' ';
        s = w;
    }
    if (s < e)
        l->full = true;
}

static void md_add_span(MdLine *l, const char *s, const char *e, int style) {
    if (l->count < MD_MAX_SEGS) {
        MdSeg *g = &l->seg[l->count++];

        g->s     = s;
        g->len   = (int)(e - s);
        g->style = style;
        g->link  = -1;
        g->glued = true;
    } else {
        l->full = true;
    }
}

static int md_add_link(MdLine *l, const char *target, int len, bool wiki) {
    if (l->linkCount >= MD_MAX_LINKS)
        return -1;
    l->link[l->linkCount].target = target;
    l->link[l->linkCount].len    = len;
    l->link[l->linkCount].wiki   = wiki;
    return l->linkCount++;
}

/* The inline HTML subset, one entry per tag. A flag toggles for the text inside; a
   code or kbd span is taken whole. */
typedef struct {
    const char *open;
    const char *close;
    int         style;
    bool        span;
} MdHtmlTag;

static const MdHtmlTag MD_HTML[] = {
    { "<b>",      "</b>",      MD_BOLD,   false },
    { "<strong>", "</strong>", MD_BOLD,   false },
    { "<i>",      "</i>",      MD_ITALIC, false },
    { "<em>",     "</em>",     MD_ITALIC, false },
    { "<mark>",   "</mark>",   MD_MARK,   false },
    { "<code>",   "</code>",   MD_CODE,   true  },
    { "<kbd>",    "</kbd>",    MD_KBD,    true  },
};

/* A word boundary before p: where a link or an underscore may begin. */
static bool md_boundary(const char *p, const char *s) {
    return p == s || p[-1] == ' ' || p[-1] == '(';
}

/* Where a tag may begin, exactly as note_derive reads one: at the start of the note's
   text or after white space. A pill the note's own tag list does not hold would filter
   the list down to nothing when clicked. */
static bool md_tag_start(const char *p, const char *text) {
    return p == text || p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\n';
}

/* One line, or one slice of a line, of the note whose text starts at `text`. */
static void md_parse(MdLine *l, const char *text, const char *s, const char *e) {
    const char *p     = s;
    const char *run   = s;
    int         style = 0;

    l->count     = 0;
    l->linkCount = 0;
    l->full      = false;
    while (p < e) {
        const char *close;
        const char *next = NULL;
        bool        html = false;

        if (*p == '\\' && p + 1 < e) {
            md_add_words(l, run, p, style, -1);
            run = p + 1;
            p  += 2;
            continue;
        }
        if (*p == '`' && (close = md_find(p + 1, e, "`")) != NULL) {
            md_add_words(l, run, p, style, -1);
            md_add_span(l, p + 1, close, style | MD_CODE);
            next = close + 1;
        } else if (md_at(p, e, "**") &&
                   ((style & MD_BOLD) || md_find(p + 2, e, "**"))) {
            md_add_words(l, run, p, style, -1);
            style ^= MD_BOLD;
            next = p + 2;
        } else if (*p == '*' && ((style & MD_ITALIC) ||
                                 (p + 1 < e && p[1] != ' ' && md_find(p + 1, e, "*")))) {
            md_add_words(l, run, p, style, -1);
            style ^= MD_ITALIC;
            next = p + 1;
        } else if (*p == '_' &&
                   (style & MD_ITALIC ? (p + 1 == e || !md_is_word(p[1]))
                                      : (md_boundary(p, s) && md_find(p + 1, e, "_")))) {
            md_add_words(l, run, p, style, -1);
            style ^= MD_ITALIC;
            next = p + 1;
        } else if (md_at(p, e, "==") && ((style & MD_MARK) || md_find(p + 2, e, "=="))) {
            md_add_words(l, run, p, style, -1);
            style ^= MD_MARK;
            next = p + 2;
        } else if (md_at(p, e, "[[") && (close = md_find(p + 2, e, "]]")) != NULL) {
            int link = md_add_link(l, p + 2, (int)(close - p - 2), true);

            md_add_words(l, run, p, style, -1);
            md_add_words(l, p + 2, close, style | MD_LINK, link);
            next = close + 2;
        } else if (*p == '[' && (close = md_find(p + 1, e, "](")) != NULL &&
                   md_find(close + 2, e, ")")) {
            const char *urlEnd = md_find(close + 2, e, ")");
            int         link   = md_add_link(l, close + 2, (int)(urlEnd - close - 2), false);

            md_add_words(l, run, p, style, -1);
            md_add_words(l, p + 1, close, style | MD_LINK, link);
            next = urlEnd + 1;
        } else if (*p == '<') {
            if (md_at(p, e, "<br>") || md_at(p, e, "<br/>")) {
                md_add_words(l, run, p, style, -1);
                md_add_span(l, p, p, MD_BREAK);
                l->seg[l->count - 1].glued = false;
                next = p + (p[3] == '/' ? 5 : 4);
                html = true;
            }
            for (size_t t = 0; !html && t < sizeof MD_HTML / sizeof MD_HTML[0]; t++) {
                const MdHtmlTag *tag     = &MD_HTML[t];
                size_t           openLen = strlen(tag->open);

                if (md_at(p, e, tag->open)) {
                    close = md_find(p + openLen, e, tag->close);
                    if (close) {
                        md_add_words(l, run, p, style, -1);
                        if (tag->span) {
                            md_add_span(l, p + openLen, close, style | tag->style);
                            next = close + strlen(tag->close);
                        } else {
                            style |= tag->style;
                            next = p + openLen;
                        }
                        html = true;
                    }
                } else if (md_at(p, e, tag->close) && (style & tag->style)) {
                    /* Only a tag that is open closes: a stray one stays text. */
                    md_add_words(l, run, p, style, -1);
                    style &= ~tag->style;
                    next = p + strlen(tag->close);
                    html = true;
                }
            }
        } else if (*p == '#' && md_tag_start(p, text) && p + 1 < e && md_is_letter(p[1])) {
            close = p + 1;
            while (close < e && (md_is_word(*close) || *close == '/' || *close == '-'))
                close++;
            while (close[-1] == '/')   /* a trailing slash is punctuation, not the tag's */
                close--;
            md_add_words(l, run, p, style, -1);
            md_add_span(l, p, close, MD_TAG);
            next = close;
        } else if (md_boundary(p, s) && (md_at(p, e, "https://") || md_at(p, e, "http://"))) {
            int link;

            close = p;
            while (close < e && *close != ' ' && *close != ')')
                close++;
            link = md_add_link(l, p, (int)(close - p), false);
            md_add_words(l, run, p, style, -1);
            md_add_words(l, p, close, style | MD_LINK, link);
            next = close;
        }
        if (next) {
            p   = next;
            run = next;
        } else {
            p++;
        }
    }
    md_add_words(l, run, p, style, -1);
}

/* -- the inline pass: segments into elements --------------------------------------- */

static const char *md_next_id(MdPage *m) {
    return rcFormat(m->arena, "%s.%d", m->prefix, m->nextId++).chars;
}

/* Where a click in the page goes. A wiki link to a title nobody wrote creates that
   note, which is how an Obsidian vault grows. */
static void md_follow(MdPage *m, const MdLink *link) {
    if (link->wiki) {
        int found = note_find_title(&m->st->store, link->target, link->len);

        if (found >= 0)
            notes_open(m->st, found);
        else
            notes_create_titled(m->st, link->target, link->len);
    } else {
        rcStrCopy(m->st->requestUrl, rcFormat(m->arena, "%.*s", link->len, link->target).chars,
                  sizeof m->st->requestUrl);
    }
}

/* A chip or a link wider than the whole line has no break to take: .wrap "e" under a box
   that clips at the line's width ends it in an ellipsis, and draws any shorter run whole.
   A read-only page lights neither under the pointer: a hover look promises a click, and
   that page takes none. */
static void md_seg(MdPage *m, const MdLine *l, const MdSeg *g, const MdRun *base) {
    const NotesPalette *t     = m->t;
    uint16_t            font  = base->font;
    RC_Color            color = base->color;
    /* The line's width clips the run; a run with no width set has no cap. */
    const RC_OptFloat clip  = base->width > 0.0f ? RC_VAL(base->width) : RC_UNSET_VAL;

    if (g->style & MD_BOLD)
        font = m->st->faces.bold;
    else if (g->style & MD_ITALIC)
        font = m->st->faces.italic;

    if (g->style & MD_CODE) {
        rcBox(.bg = t->codeFill, .px = 5, .py = 1, .overflow = "hidden", .borderRadius = "all-sm",
              .wMax = clip) {
            rcText(md_slice(g->s, g->len), .font = m->st->faces.mono, .color = t->codeInk,
                   .size = m->rs->code, .wrap = "e");
        }
    } else if (g->style & MD_KBD) {
        rcBox(.bg = t->paper, .px = 6, .py = 1, .overflow = "hidden", .borderRadius = "all-sm",
              .border = { .color = t->rule, .width = 1 }, .wMax = clip) {
            rcText(md_slice(g->s, g->len), .font = m->st->faces.mono, .color = t->inkSoft,
                   .size = m->rs->code, .wrap = "e");
        }
    } else if (g->style & MD_TAG) {
        const char *id = md_next_id(m);

        rcBox(.id = id, .focusable = !m->readOnly, .px = 8, .py = 1, .overflow = "hidden",
              .borderRadius = "all-full", .wMax = clip,
              .className = m->readOnly ? "tag-pill" : "tag-pill hover:bg-surface-alt") {
            rcText(md_slice(g->s, g->len), .color = t->inkSoft, .size = m->rs->code, .wrap = "e");
        }
        if (!m->readOnly && rcClicked(id))
            notes_show_tag(m->st, g->s + 1, g->len - 1);
    } else if (g->style & MD_LINK) {
        const char   *id     = md_next_id(m);
        const MdLink *link   = g->link >= 0 ? &l->link[g->link] : NULL;
        bool          dead   = link && link->wiki &&
                               note_find_title(&m->st->store, link->target, link->len) < 0;
        RC_Color      linkInk = dead ? t->inkMuted : t->link;

        rcBox(.id = id, .focusable = !m->readOnly && link, .overflow = "hidden", .wMax = clip) {
            rcText(md_slice(g->s, g->len), .font = font,
                   .color = !m->readOnly && rcShowsHover(id) ? t->accent : linkInk,
                   .size = base->size, .lineHeight = base->line, .wrap = "e");
        }
        if (!m->readOnly && link && rcClicked(id))
            md_follow(m, link);
    } else if (g->style & MD_MARK) {
        rcBox(.bg = t->markFill, .overflow = "hidden", .wMax = clip) {
            rcText(md_slice(g->s, g->len), .font = font, .color = t->ink, .size = base->size,
                   .lineHeight = base->line, .wrap = "e");
        }
    } else {
        rcText(md_slice(g->s, g->len), .font = font, .color = color, .size = base->size,
               .lineHeight = base->line, .wrap = "l");
    }
}

static bool md_is_plain(const char *s, const char *e) {
    for (const char *p = s; p < e; p++) {
        if (*p == '*' || *p == '`' || *p == '[' || *p == '<' || *p == '#' || *p == '\\' ||
            *p == '_' || (p[0] == '=' && p + 1 < e && p[1] == '=') ||
            (p[0] == 'h' && md_at(p, e, "http")))
            return false;
    }
    return true;
}

/* The face a segment draws in, and the width it takes in a line. */
static uint16_t md_seg_font(const MdPage *m, const MdSeg *g, const MdRun *base) {
    if (g->style & (MD_CODE | MD_KBD))
        return m->st->faces.mono;
    if (g->style & MD_TAG)
        return 0;
    if (g->style & MD_BOLD)
        return m->st->faces.bold;
    if (g->style & MD_ITALIC)
        return m->st->faces.italic;
    return base->font;
}

static float md_seg_width(const MdPage *m, const MdSeg *g, const MdRun *base) {
    bool  chip = (g->style & (MD_CODE | MD_KBD | MD_TAG)) != 0;
    float w    = rcTextWidth(md_seg_font(m, g, base), chip ? m->rs->code : base->size, g->s,
                             g->len);

    if (g->style & MD_CODE) return w + 10.0f;
    if (g->style & MD_KBD)  return w + 12.0f;
    if (g->style & MD_TAG)  return w + 16.0f;
    return w;
}

/* One line's segments, drawn as few runs as possible: neighbors in the same style that
   are neighbors in the text too become one run. Space at the line's end is dropped, as
   md_inline's fill leaves it out, so the lone space after a chip or a bold word, a run
   of its own, is not drawn there either. */
static void md_line_runs(MdPage *m, const MdLine *l, int from, int to, const MdRun *base) {
    for (int i = from; i < to;) {
        MdSeg run = l->seg[i++];

        while (i < to && !(run.style & (MD_CODE | MD_KBD | MD_TAG)) &&
               l->seg[i].style == run.style && l->seg[i].link == run.link &&
               run.s + run.len == l->seg[i].s) {
            run.len += l->seg[i].len;
            i++;
        }
        if (i == to) {
            while (run.len > 0 && run.s[run.len - 1] == ' ')
                run.len--;
            if (run.len == 0)
                break;
        }
        md_seg(m, l, &run, base);
    }
}

/* One line of source as a block of text, in `align`: 'l', 'c' or 'r'. */
static void md_inline(MdPage *m, const char *s, const char *e, const MdRun *base, char align) {
    static MdLine l;     /* one line at a time, and too large for the stack */
    const char   *justify = align == 'r' ? "items-baseline justify-end"
                          : (align == 'c' ? "items-baseline justify-center" : "items-baseline");
    const bool    plain   = md_is_plain(s, e);
    float         space;
    int           i = 0;

    if (!plain)
        md_parse(&l, m->note->body, s, e);
    /* A line with more runs than MD_MAX_SEGS (a long paragraph pasted as one line) draws
       whole as plain text, its markup showing, rather than cut off at the last run that fit. */
    if (plain || l.full) {
        rcBox(.w = "grow",
              .className = align == 'r' ? "items-end" : (align == 'c' ? "items-center" : "")) {
            rcText(md_slice(s, (int)(e - s)),
                   .className = align == 'r' ? "text-right" : (align == 'c' ? "text-center" : ""),
                   .font = base->font, .color = base->color, .size = base->size,
                   .lineHeight = base->line);
        }
        return;
    }
    space = rcTextWidth(base->font, base->size, " ", 1);
    rcColumn(.w = "grow") {
        while (i < l.count) {
            int   from = i;
            float x    = 0.0f;

            /* Fill the line, a glued run at a time, without counting the space a line
               would end on. Only a run that ends in one is let off it: the text's last
               word and a chip end in none, and crediting them a space overfills the row,
               which the row then takes out of a chip, cutting it to an ellipsis. The
               first run always goes in, however wide. */
            while (i < l.count && !(l.seg[i].style & MD_BREAK)) {
                int   j    = i;
                float w    = 0.0f;
                float tail = 0.0f;

                for (;;) {
                    const MdSeg *g = &l.seg[j];

                    w += md_seg_width(m, g, base);
                    if (!g->glued || j + 1 == l.count || (l.seg[j + 1].style & MD_BREAK)) {
                        tail = g->len > 0 && g->s[g->len - 1] == ' ' ? space : 0.0f;
                        break;
                    }
                    j++;
                }
                if (x > 0.0f && x + w - tail > base->width)
                    break;
                x += w;
                i  = j + 1;
            }
            rcRow(.w = "grow", .className = justify) {
                md_line_runs(m, &l, from, i, base);
            }
            if (i < l.count && (l.seg[i].style & MD_BREAK))
                i++;
        }
    }
}

/* -- code fences ------------------------------------------------------------------- */

typedef enum { LANG_NONE = 0, LANG_C, LANG_JS, LANG_PY, LANG_SH, LANG_JSON } MdLang;

static bool md_word_in(const char *w, int n, const char *const *list, int count) {
    for (int i = 0; i < count; i++)
        if ((int)strlen(list[i]) == n && strncmp(list[i], w, (size_t)n) == 0)
            return true;
    return false;
}

static MdLang md_lang(const char *s, int n) {
    static const char *const C_NAMES[]  = { "c", "h", "cpp", "c++", "cc" };
    static const char *const JS_NAMES[] = { "js", "ts", "javascript", "typescript" };
    static const char *const PY_NAMES[] = { "py", "python" };
    static const char *const SH_NAMES[] = { "sh", "bash", "shell", "zsh" };

    if (md_word_in(s, n, C_NAMES, 5))  return LANG_C;
    if (md_word_in(s, n, JS_NAMES, 4)) return LANG_JS;
    if (md_word_in(s, n, PY_NAMES, 2)) return LANG_PY;
    if (md_word_in(s, n, SH_NAMES, 4)) return LANG_SH;
    if (n == 4 && strncmp(s, "json", 4) == 0) return LANG_JSON;
    return LANG_NONE;
}

static bool md_is_keyword(MdLang lang, const char *w, int n) {
    static const char *const C_WORDS[] = {
        "if", "else", "for", "while", "do", "return", "switch", "case", "break", "continue",
        "static", "const", "struct", "typedef", "enum", "union", "void", "int", "float",
        "double", "char", "bool", "unsigned", "sizeof", "true", "false", "NULL", "inline",
    };
    static const char *const JS_WORDS[] = {
        "if", "else", "for", "while", "return", "function", "const", "let", "var", "new",
        "class", "import", "from", "export", "await", "async", "true", "false", "null",
    };
    static const char *const PY_WORDS[] = {
        "if", "elif", "else", "for", "while", "return", "def", "class", "import", "from",
        "as", "with", "try", "except", "True", "False", "None", "and", "or", "not", "in",
    };
    static const char *const SH_WORDS[] = {
        "if", "then", "else", "fi", "for", "do", "done", "while", "case", "esac", "export",
        "echo", "cd", "git", "docker", "kubectl", "sudo",
    };
    static const char *const JSON_WORDS[] = { "true", "false", "null" };

#define MD_IN(list) md_word_in(w, n, list, (int)(sizeof(list) / sizeof((list)[0])))
    switch (lang) {
    case LANG_C:    return MD_IN(C_WORDS);
    case LANG_JS:   return MD_IN(JS_WORDS);
    case LANG_PY:   return MD_IN(PY_WORDS);
    case LANG_SH:   return MD_IN(SH_WORDS);
    case LANG_JSON: return MD_IN(JSON_WORDS);
    default:        return false;
    }
#undef MD_IN
}

static void md_token(MdPage *m, const char *s, const char *e, RC_Color color) {
    if (e > s)
        rcText(md_slice(s, (int)(e - s)), .font = m->st->faces.mono, .color = color,
               .size = m->rs->code, .lineHeight = m->rs->codeLine, .wrap = "l");
}

/* One highlighted line: a row of runs. *inComment carries a block comment across
   lines, which is the only state a line-at-a-time lexer needs. */
static void md_code_line(MdPage *m, MdLang lang, const char *s, const char *e, bool *inComment) {
    const NotesPalette *t = m->t;
    const char         *p = s;

    if (s == e) {
        rcBox(.hType = RC_PX((float)m->rs->codeLine)) {}
        return;
    }
    rcRow() {
        const char *plain = p;

        while (p < e) {
            const char *q = p;
            RC_Color    c = t->codeInk;

            if (*inComment) {
                const char *end = md_find(p, e, "*/");

                q = end ? end + 2 : e;
                *inComment = end == NULL;
                c = t->codeComment;
            } else if (lang != LANG_NONE && lang != LANG_JSON &&
                       (((lang == LANG_C || lang == LANG_JS) && md_at(p, e, "//")) ||
                        ((lang == LANG_PY || lang == LANG_SH) && *p == '#'))) {
                q = e;
                c = t->codeComment;
            } else if ((lang == LANG_C || lang == LANG_JS) && md_at(p, e, "/*")) {
                const char *end = md_find(p + 2, e, "*/");

                q = end ? end + 2 : e;
                *inComment = end == NULL;
                c = t->codeComment;
            } else if (lang == LANG_C && *p == '#' && p == s) {
                q = p + 1;
                while (q < e && md_is_word(*q))
                    q++;
                c = t->codeKeyword;
            } else if (lang != LANG_NONE && (*p == '"' || *p == '\'')) {
                char quote = *p;

                q = p + 1;
                while (q < e && *q != quote) {
                    if (*q == '\\' && q + 1 < e)
                        q++;
                    q++;
                }
                q = q < e ? q + 1 : e;
                c = (lang == LANG_JSON && q < e && *q == ':') ? t->codeType : t->codeString;
            } else if (lang != LANG_NONE && md_is_digit(*p) && (p == s || !md_is_word(p[-1]))) {
                while (q < e && (md_is_word(*q) || *q == '.'))
                    q++;
                c = t->codeNumber;
            } else if (lang != LANG_NONE && md_is_letter(*p) && (p == s || !md_is_word(p[-1]))) {
                while (q < e && md_is_word(*q))
                    q++;
                if (md_is_keyword(lang, p, (int)(q - p)))
                    c = t->codeKeyword;
                else if (q < e && *q == '(')
                    c = t->codeFunc;
                else if (lang == LANG_C &&
                         (*p <= 'Z' || (q - p > 2 && q[-2] == '_' && q[-1] == 't')))
                    c = t->codeType;
            } else {
                p++;
                continue;
            }
            md_token(m, plain, p, t->codeInk);
            md_token(m, p, q, c);
            p     = q;
            plain = q;
        }
        md_token(m, plain, p, t->codeInk);
    }
}

/* A fence. The panel is a column so its header never scrolls; the lines scroll inside
   it, because a wrapped line of C is a lie about the program. */
static const char *md_code_block(MdPage *m, const char *fence, const char *fenceEnd) {
    const NotesPalette *t         = m->t;
    const char         *langStart = fence + 3;
    const char         *body      = *fenceEnd ? fenceEnd + 1 : fenceEnd;
    const char         *end       = body;
    const char         *after;
    MdLang              lang      = md_lang(langStart, (int)(fenceEnd - langStart));
    const char         *copyId    = md_next_id(m);
    const char         *scrollId  = md_next_id(m);
    bool                copied    = m->st->copiedBlock == body && m->st->copiedLeft > 0.0f;
    bool                inComment = false;

    /* The block ends at the next line that starts with a fence, indented or not: the way
       md_render opens one, and the way md_outline and note_derive count them. */
    for (;;) {
        after = end;
        while (*after == ' ')
            after++;
        if (!*end || md_at(after, after + 3, "```"))
            break;
        while (*end && *end != '\n')
            end++;
        if (*end)
            end++;
    }
    while (*after && *after != '\n')
        after++;

    rcColumn(.pb = (float)m->rs->body, .w = "grow") {
        rcColumn(.bg = t->codeFill, .overflow = "hidden", .borderRadius = "all-lg",
                 .border = { .color = t->rule, .width = 1 }, .w = "grow") {
            rcRow(.gap = 8, .pt = 6, .pl = 14, .pr = 8, .align = "cl", .w = "grow") {
                rcText(md_slice(langStart, (int)(fenceEnd - langStart)), .color = t->inkMuted,
                       .size = 13);
                rcSpacer() {}
                if (rcButton(copyId, copied ? "Copied" : "Copy",
                             .className = rcPointerIsCoarse() ? "h-11 ink-soft" : "h-7 ink-soft",
                             .variant = RC_BTN_GHOST,
                             .leadingIcon = copied ? rcIconCheck : rcIconCopy)) {
                    int len = (int)(end - body);

                    m->st->requestCopy = rcFormat(m->arena, "%.*s", len, body).chars;
                    m->st->copiedBlock = body;
                    m->st->copiedLeft  = 1.5f;
                }
            }
            rcColumn(.id = scrollId, .pt = 2, .pb = 12, .pl = 14, .pr = 14, .scroll = "h",
                     .w = "grow") {
                const char *p = body;

                while (p < end) {
                    const char *e = p;

                    while (e < end && *e != '\n')
                        e++;
                    md_code_line(m, lang, p, e, &inComment);
                    p = e + 1;
                }
            }
            rcScrollbar(scrollId);
        }
    }
    return after;
}

/* -- tables ------------------------------------------------------------------------ */

#define MD_MAX_COLS 8

/* Split one "| a | b |" row into trimmed cells. Returns the count. */
static int md_cells(const char *s, const char *e, const char **start, const char **stop) {
    int n = 0;

    while (s < e && *s == ' ')
        s++;
    if (s < e && *s == '|')
        s++;
    while (s < e && n < MD_MAX_COLS) {
        const char *c = s;

        while (c < e && *c != '|')
            c++;
        const char *a = s, *b = c;

        while (a < b && *a == ' ')
            a++;
        while (b > a && b[-1] == ' ')
            b--;
        if (c == e && a == b)
            break;
        start[n] = a;
        stop[n]  = b;
        n++;
        s = c < e ? c + 1 : c;
    }
    return n;
}

static bool md_is_table_rule(const char *s, const char *e) {
    bool dash = false, pipe = false;

    for (const char *p = s; p < e; p++) {
        if (*p == '-')      dash = true;
        else if (*p == '|') pipe = true;
        else if (*p != ':' && *p != ' ') return false;
    }
    return dash && pipe;
}

/* What a cell asks for: all of it on one line, and its longest piece that cannot break.
   Read off the parsed runs, so `**$6,000**` counts as the bold text it draws. */
static void md_cell_widths(MdPage *m, const char *s, const char *e, const MdRun *base, float *whole,
                           float *piece) {
    static MdLine l;     /* one cell at a time, and too large for the stack */
    float         space = rcTextWidth(base->font, base->size, " ", 1);
    float         glued = 0.0f;

    *whole = 0.0f;
    *piece = 0.0f;
    md_parse(&l, m->note->body, s, e);
    for (int i = 0; i < l.count; i++) {
        float w = md_seg_width(m, &l.seg[i], base);

        *whole += w;
        glued  += w;
        if (!l.seg[i].glued) {
            if (glued - space > *piece)
                *piece = glued - space;
            glued = 0.0f;
        }
    }
    if (glued > *piece)
        *piece = glued;
}

/* A table: rcBeginTable with no header text, so the first row is drawn like any other
   and can carry inline markup. No column is ever narrower than its longest word, and
   the table never scrolls sideways, so a window too narrow for every column drops some
   and says how many. RC_TableColumn.priority counts a column's fixed width, .wMin or
   header, but drops in an order fixed ahead, and this one depends on the cells (the
   widest prose first), so the table measures each column's longest word itself and
   builds its cols[] from the columns that stay. Returns the first line after it. */
static const char *md_table(MdPage *m, const char *head, const char *headEnd, const char *rule,
                            const char *ruleEnd) {
    const NotesPalette *t = m->t;
    const float         pad = 25.0f;   /* a cell's 12 px each side, and a pixel of rounding */
    const float         room = m->width - 2.0f;   /* inside the table's border */
    const char         *starts[MD_MAX_COLS], *stops[MD_MAX_COLS];
    char                align[MD_MAX_COLS];
    float               whole[MD_MAX_COLS], least[MD_MAX_COLS];
    RC_TableColumn      cols[MD_MAX_COLS];
    int                 shown[MD_MAX_COLS];
    const char         *afterTable = *ruleEnd ? ruleEnd + 1 : ruleEnd;
    const char         *id         = md_next_id(m);
    int                 colCount   = md_cells(head, headEnd, starts, stops);
    int                 shownCount = 0;
    float               used       = 0.0f;
    float               wanted     = 0.0f;
    float               total      = 0.0f;
    bool                keep[MD_MAX_COLS];

    memset(cols, 0, sizeof cols);
    for (int c = 0; c < colCount; c++) {
        align[c] = 'l';
        whole[c] = pad;
        least[c] = pad;
        keep[c]  = true;
    }
    {
        const char *rs[MD_MAX_COLS], *re[MD_MAX_COLS];
        int         n = md_cells(rule, ruleEnd, rs, re);

        for (int c = 0; c < n && c < colCount; c++) {
            bool left  = re[c] > rs[c] && rs[c][0] == ':';
            bool right = re[c] > rs[c] && re[c][-1] == ':';

            align[c] = left && right ? 'c' : (right ? 'r' : 'l');
        }
    }
    /* Measure the whole table first: a column's width cannot depend on a row below it
       unless that row has been seen. */
    for (const char *line = head; line && *line;) {
        const char *e = line;
        int         n;

        while (*e && *e != '\n')
            e++;
        if (line != head && line != rule) {
            const char *bar = line;

            while (*bar == ' ')   /* a row may be indented, as the head may */
                bar++;
            if (*bar != '|')
                break;
            afterTable = *e ? e + 1 : e;
        }
        if (line != rule) {
            MdRun run = { line == head ? m->st->faces.bold : (uint16_t)0, m->rs->body,
                          m->rs->bodyLine, t->ink, 0.0f };

            n = md_cells(line, e, starts, stops);
            for (int c = 0; c < n && c < colCount; c++) {
                float w, word;

                md_cell_widths(m, starts[c], stops[c], &run, &w, &word);
                if (w + pad > whole[c])
                    whole[c] = w + pad;
                if (word + pad > least[c])
                    least[c] = word + pad;
            }
        }
        line = *e ? e + 1 : NULL;
    }
    for (int c = 0; c < colCount; c++)
        used += least[c];
    /* Too narrow for every column's longest word. The first column names the row and a
       right-aligned one holds the numbers, so the widest of the others goes first: it is
       the prose, which reads worst squeezed and frees the most room. */
    while (used > room) {
        int drop = -1;

        for (int c = colCount - 1; c > 0; c--)
            if (keep[c] && (drop < 0 || (align[c] != 'r' &&
                                         (align[drop] == 'r' || whole[c] > whole[drop]))))
                drop = c;
        if (drop < 0)
            break;
        keep[drop] = false;
        used -= least[drop];
    }
    /* Every kept column has its longest word; the room left goes first to the columns
       that would otherwise wrap, and what still remains is shared by size. */
    for (int c = 0; c < colCount; c++) {
        if (keep[c]) {
            shown[shownCount++] = c;
            wanted += whole[c] - least[c];
            total  += whole[c];
        }
    }
    for (int k = 0; k < shownCount; k++) {
        int   c     = shown[k];
        float spare = room - used;
        float share = wanted > 0.0f ? (whole[c] - least[c]) / wanted : 0.0f;
        float width = least[c] + (spare < wanted ? spare * share : whole[c] - least[c]);

        if (spare > wanted)
            width += (spare - wanted) * whole[c] / total;
        cols[k].wType    = RC_PX(width < room ? width : room);
        cols[k].align    = "tl";
    }

    if (shownCount < colCount) {
        int dropped = colCount - shownCount;

        rcRow(.pb = 4, .align = "cr", .w = "grow") {
            rcText(rcFormat(m->arena, "+%d column%s", dropped, dropped == 1 ? "" : "s"),
                   .color = t->inkMuted, .size = 13);
        }
    }
    rcColumn(.pb = (float)m->rs->body, .w = "grow") {
        rcBox(.overflow = "hidden", .borderRadius = "all-md",
              .border = { .color = t->rule, .width = 1 }, .w = "grow") {
            if (rcBeginTable(id, cols, shownCount, .cellPadding = RC_VAL(0.0f))) {
                int rowIndex = 0;

                for (const char *line = head; line < afterTable;) {
                    const char *e = line;

                    while (*e && *e != '\n')
                        e++;
                    if (line != rule) {
                        int   n    = md_cells(line, e, starts, stops);
                        bool  top  = rowIndex == 0;
                        MdRun run  = { top ? m->st->faces.bold : (uint16_t)0, m->rs->body,
                                       m->rs->bodyLine, t->ink, 0.0f };

                        rcTableRow();
                        for (int k = 0; k < shownCount; k++) {
                            int      c    = shown[k];
                            RC_Color fill = top ? t->tableHead
                                                : (rowIndex % 2 == 0 ? t->tableZebra : t->paper);

                            run.width = cols[k].wType.value - 24.0f;

                            if (k)
                                rcTableNext();
                            rcBox(.bg = fill, .pt = 6, .pb = 6, .pl = 12, .pr = 12, .w = "grow",
                                  .h = "grow") {
                                if (c < n)
                                    md_inline(m, starts[c], stops[c], &run, align[c]);
                            }
                        }
                        rowIndex++;
                    }
                    line = *e ? e + 1 : e;
                    if (!*e)
                        break;
                }
                rcEndTable();
            }
        }
    }
    return afterTable;
}

/* -- blocks ------------------------------------------------------------------------ */

static const char *md_line_end(const char *p) {
    while (*p && *p != '\n')
        p++;
    return p;
}

static bool md_is_blank(const char *s, const char *e) {
    for (; s < e; s++)
        if (*s != ' ' && *s != '\t')
            return false;
    return true;
}

static const char *md_heading_id(MdPage *m, int ordinal) {
    return rcFormat(m->arena, "%s.h%d", m->prefix, ordinal).chars;
}

static void md_heading(MdPage *m, int level, const char *s, const char *e, bool first) {
    const NotesTypeStep *rs   = m->rs;
    uint16_t size = level == 1 ? rs->h1 : (level == 2 ? rs->h2 : (level == 3 ? rs->h3 : rs->body));
    MdRun    run  = { m->st->faces.bold, size, (uint16_t)(size * 1.25f + 0.5f), m->t->ink,
                      m->width };
    float    top  = first ? 0.0f : (float)(level <= 2 ? rs->body * 2 : rs->body + 4);
    float    foot = (float)(level == 1 ? rs->body : rs->body / 2);

    rcBox(.id = md_heading_id(m, m->nextHeading++), .pt = top, .pb = foot, .w = "grow") {
        md_inline(m, s, e, &run, 'l');
    }
}

/* A task's box is the library's checkbox, and ticking it rewrites one byte of the
   note's own text: the Markdown stays the single source of truth, so the list's
   task count, the Todo count and an open Write pane all follow on the next frame. */
static void md_task(MdPage *m, char *mark, const char *s, const char *e, int indent) {
    const NotesPalette *t = m->t;
    bool     done = *mark == 'x' || *mark == 'X';   /* as note_derive counts them */
    bool     value = done;
    MdRun    run  = { 0, m->rs->body, m->rs->bodyLine, done ? t->inkMuted : t->ink,
                      m->width - (float)indent - 28.0f };
    uint16_t line = m->rs->bodyLine;

    rcRow(.gap = 8, .pb = 4, .pl = (float)indent, .w = "grow") {
        rcBox(.align = "cc", .hType = RC_PX((float)line)) {
            if (rcCheckbox(md_next_id(m), "", &value, .disabled = m->readOnly)) {
                *mark = value ? 'x' : ' ';
                note_touch(&m->st->store, m->note);
            }
        }
        rcBox(.w = "grow") {
            md_inline(m, s, e, &run, 'l');
        }
    }
}

/* A list item. The marker's slot is 18 px, or the number's own width once "10." outgrows
   it, in whole pixels; the text's lines are narrower by the slot, the 6 px gap and the 2 px
   of padding before them. */
static void md_bullet(MdPage *m, const char *s, const char *e, int indent, const char *number,
                      int numberLen) {
    const NotesPalette *t    = m->t;
    float               wide = number ? rcTextWidth(0, m->rs->body, number, numberLen) : 0.0f;
    float               slot = wide > 18.0f ? (float)(int)(wide + 1.0f) : 18.0f;
    MdRun               run  = { 0, m->rs->body, m->rs->bodyLine, t->ink,
                                 m->width - (float)indent - slot - 8.0f };
    float               line = (float)m->rs->bodyLine;

    rcRow(.gap = 6, .pb = 4, .pl = (float)indent, .w = "grow") {
        rcBox(.align = "cr", .wType = RC_PX(slot), .hType = RC_PX(line)) {
            if (number) {
                rcText(md_slice(number, numberLen), .color = t->link, .size = m->rs->body);
            } else {
                rcBox(.bg = t->accent, .borderRadius = "all-full", .w = "6px", .h = "6px") {}
            }
        }
        rcBox(.pl = 2, .w = "grow") {
            md_inline(m, s, e, &run, 'l');
        }
    }
}

/* A callout's kind in any case: Obsidian reads [!NOTE] and [!note] alike. */
static bool md_kind_is(const char *s, int n, const char *kind) {
    for (int i = 0; i < n; i++)
        if ((s[i] | 0x20) != kind[i])
            return false;
    return kind[n] == '\0';
}

/* A quote, or with a [!kind] first line, a callout as Bear and Obsidian draw one. Its
   lines are 34 px narrower than the page: the 16 and 14 px of padding and the 4 px bar,
   since a border takes layout space. Returns the first line after it. */
static const char *md_quote(MdPage *m, const char *s) {
    const NotesPalette *t = m->t;
    const char         *p = s + 1;
    const char         *e = md_line_end(s);
    RC_Color            bar = t->rule, fill = RC_UNSET_COLOR;
    RC_IconCallback     icon = NULL;
    const char         *titleStart = NULL, *titleEnd = NULL;
    const char         *rest = s;
    MdRun               run = { 0, m->rs->body, m->rs->bodyLine, t->inkSoft, m->width - 34.0f };

    while (p < e && *p == ' ')
        p++;
    if (md_at(p, e, "[!")) {
        const char *close = md_find(p, e, "]");

        if (close) {
            int kindLen = (int)(close - p - 2);

            if (md_kind_is(p + 2, kindLen, "note")) {
                bar = t->noteBar; fill = t->noteFill; icon = rcIconInfo;
            } else if (md_kind_is(p + 2, kindLen, "tip")) {
                bar = t->tipBar; fill = t->tipFill; icon = rcIconInfo;
            } else if (md_kind_is(p + 2, kindLen, "warning")) {
                bar = t->warnBar; fill = t->warnFill; icon = rcIconCircleAlert;
            } else if (md_kind_is(p + 2, kindLen, "danger")) {
                bar = t->dangerBar; fill = t->dangerFill; icon = rcIconTriangleAlert;
            }
            if (icon) {
                titleStart = close + 1;
                while (titleStart < e && *titleStart == ' ')
                    titleStart++;
                titleEnd = e;
                rest     = *e ? e + 1 : e;
                run.color = t->ink;
            }
        }
    }

    rcColumn(.pb = (float)m->rs->body, .w = "grow") {
        rcColumn(.bg = fill, .gap = 4, .pt = (float)(icon ? 12 : 2),
                 .pb = (float)(icon ? 12 : 2), .pl = 16, .pr = 14,
                 .border = { .color = bar }, .w = "grow",
                 .className = icon ? "border-l-4 rounded-r-md" : "border-l-4") {
            if (icon) {
                /* Beside the icon, which is body-sized, and the row's 8 px gap. */
                MdRun head = { m->st->faces.bold, m->rs->body, m->rs->bodyLine, t->ink,
                               m->width - 42.0f - (float)m->rs->body };

                rcRow(.gap = 8, .align = "cl", .w = "grow") {
                    icon((float)m->rs->body, bar);
                    md_inline(m, titleStart, titleEnd, &head, 'l');
                }
            }
            for (p = rest; *p == '>';) {
                const char *le = md_line_end(p);
                const char *q  = p + 1;

                while (q < le && *q == ' ')
                    q++;
                md_inline(m, q, le, &run, 'l');
                p = *le ? le + 1 : le;
            }
        }
    }
    return p;
}

/* The whole note. One pass over its lines; see the file header. */
static void md_render(MdPage *m) {
    char *p     = m->note->body;
    bool  first = true;

    while (*p) {
        char *e = p;
        char *s = p;
        int   indent = 0;

        while (*e && *e != '\n')
            e++;
        if (md_is_blank(p, e)) {
            p = *e ? e + 1 : e;
            continue;
        }
        while (s < e && *s == ' ') {
            s++;
            indent++;
        }

        if (md_at(s, e, "```")) {
            p = (char *)md_code_block(m, s, e);
            if (*p == '\n')
                p++;
        } else if (*s == '>') {
            p = (char *)md_quote(m, s);
        } else if (*e == '\n' && *s != '-' && *s != '*' && md_find(s, e, "|") &&
                   md_is_table_rule(e + 1, md_line_end(e + 1))) {
            p = (char *)md_table(m, s, e, e + 1, md_line_end(e + 1));
        } else {
            char *t = s;
            int   level = 0;
            int   digits = 0;   /* of an ordered item's number: "1." and "12." alike */
            bool  gapped, listItem = false;

            /* Six levels, as HTML has: a seventh # makes the line a paragraph. */
            while (t < e && *t == '#' && level < 6) {
                t++;
                level++;
            }
            while (digits < 9 && s + digits < e && md_is_digit(s[digits]))
                digits++;
            gapped = *e == '\0' || e[1] == '\n' || e[1] == '\0';

            if (level > 0 && t < e && *t == ' ') {
                md_heading(m, level, t + 1, e, first);
            } else if (e - s >= 3 && (md_at(s, e, "---") || md_at(s, e, "***")) &&
                       md_is_blank(s + 3, e)) {
                rcBox(.pt = 8, .pb = (float)(m->rs->body + 8), .w = "grow") {
                    rcBox(.bg = m->t->rule, .w = "grow", .h = "1px") {}
                }
            } else if (e - s >= 6 && (s[0] == '-' || s[0] == '*' || s[0] == '+') && s[1] == ' ' &&
                       s[2] == '[' && s[4] == ']' && s[5] == ' ') {
                md_task(m, s + 3, s + 6, e, indent * 10);
                listItem = true;
            } else if (e - s >= 2 && (s[0] == '-' || s[0] == '*' || s[0] == '+') && s[1] == ' ') {
                md_bullet(m, s + 2, e, indent * 10, NULL, 0);
                listItem = true;
            } else if (digits > 0 && e - s >= digits + 2 && s[digits] == '.' &&
                       s[digits + 1] == ' ') {
                md_bullet(m, s + digits + 2, e, indent * 10, s, digits + 1);
                listItem = true;
            } else {
                MdRun run = { 0, m->rs->body, m->rs->bodyLine, m->t->ink, m->width };

                rcBox(.pb = (float)(gapped ? m->rs->body : 2), .w = "grow") {
                    md_inline(m, s, e, &run, 'l');
                }
            }
            /* A list ends with a small gap of its own before the block after it. */
            if (listItem && gapped) {
                rcBox(.h = "10px") {}
            }
            p = *e ? e + 1 : e;
        }
        first = false;
    }
}

/* The headings of a note, in the order md_render numbers them, skipping fences. The
   contents list and the scroll spy read this, not the page. The first non-blank line
   outside a fence is the note's title, as note_derive reads it, so a heading there
   opens no section and the sections start at firstSection. */
static void md_outline(AppState *st, const char *body) {
    const char *p         = body;
    bool        inFence   = false;
    bool        seenTitle = false;

    st->headingCount = 0;
    st->firstSection = 0;
    while (*p && st->headingCount < NOTES_MAX_HEADINGS) {
        const char *e = md_line_end(p);

        const char *t = p;

        while (t < e && *t == ' ')
            t++;
        if (md_at(t, e, "```")) {
            inFence = !inFence;
        } else if (!inFence && !md_is_blank(t, e)) {
            int level = 0;

            while (t < e && *t == '#' && level < 6) {
                t++;
                level++;
            }
            if (level > 0 && t < e && *t == ' ') {
                NoteHeading *h = &st->headings[st->headingCount++];

                h->level  = level;
                h->offset = (int)(t + 1 - body);
                h->length = (int)(e - t - 1);
                if (!seenTitle)
                    st->firstSection = 1;
            }
            seenTitle = true;
        }
        p = *e ? e + 1 : e;
    }
}

#endif /* NOTES_MARKDOWN_H */
