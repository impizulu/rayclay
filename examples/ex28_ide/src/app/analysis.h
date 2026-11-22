/*
    analysis.h - what the Outline and the Problems panel show, read from the text

    Both are rebuilt when the text changed, once per frame at most. The checks are real and
    simple: a JSON file goes through rcJsonParse, an SVG through rcLoadSvgFromMemory, and C gets
    a bracket balance that knows strings and comments, plus the house style's line rules.
*/
#ifndef APP_ANALYSIS_H
#define APP_ANALYSIS_H

#include "rayclay.h"
#include "app/doc.h"

enum { MAX_SYMBOLS = 1024, MAX_PROBLEMS = 256, LINE_LIMIT = 100, FUNCTION_LIMIT = 60 };

static inline void add_symbol(Doc *d, int line, int endLine, int name, int nameLen, int kind) {
    if (d->nSym < MAX_SYMBOLS)
        d->sym[d->nSym++] = RC_LIT(Symbol){ line, endLine, name, nameLen, (uint8_t)kind };
}

/* A function's name: the word before the first '(' of its head. */
static inline void add_c_function(Doc *d, int head, int end) {
    const char *s = d->text + d->line[head];
    int         n = doc_line_len(d, head), i = 0, j;

    while (i < n && s[i] != '(')
        i++;
    j = i;
    while (j > 0 && is_blank(s[j - 1]))
        j--;
    i = j;
    while (i > 0 && is_word(s[i - 1]))
        i--;
    if (j > i)
        add_symbol(d, head, end, d->line[head] + i, j - i, SYM_FUNCTION);
}

/* The last non-blank byte of a line, or 0 for a blank one. */
static inline char c_last(const Doc *d, int ln, int *at) {
    const char *s = d->text + d->line[ln];
    int         n = doc_line_len(d, ln);

    while (n > 0 && is_blank(s[n - 1]))
        n--;
    *at = n - 1;
    return n ? s[n - 1] : 0;
}

/* C as the examples write it: a head starts at column 0 with a word, and a function's body opens
   with a `{` that ends its head (a parameter list may wrap onto indented lines) or stands alone at
   column 0 under it; the body closes with `}` at column 0. `} Name;` at column 0 closes a type.
   One pass, each line read once: good enough for an outline, and cheap. */
static inline void scan_c_symbols(Doc *d) {
    int ln;

    for (ln = 0; ln < d->lines; ln++) {
        const char *s = d->text + d->line[ln];
        int         n = doc_line_len(d, ln), k = ln, open = -1, at, end;
        char        c;

        if (d->lineState[ln] != ST_NORMAL || !n)
            continue;
        if (s[0] == '}' && n > 3 && s[n - 1] == ';') {
            int i = 1;

            while (i < n && is_blank(s[i]))
                i++;
            if (i < n - 1 && is_word(s[i]))
                add_symbol(d, ln, ln, d->line[ln] + i, n - 1 - i, SYM_TYPE);
            continue;
        }
        if (!is_word(s[0]))
            continue;
        /* The head runs to the line that ends it: `) {` opens a body, `;` or any other `{`
           (a type, an initializer) ends it, as does a line that is not indented under it. */
        for (;;) {
            c = c_last(d, k, &at);
            if (c == '{') {
                while (at > 0 && is_blank(d->text[d->line[k] + at - 1]))
                    at--;
                if (at > 0 && d->text[d->line[k] + at - 1] == ')')
                    open = k;
                break;
            }
            if (c == ')' && k + 1 < d->lines && doc_line_len(d, k + 1) == 1
                && d->text[d->line[k + 1]] == '{') {
                open = k + 1;
                break;
            }
            if (c == ';' || k + 1 >= d->lines || !is_blank(d->text[d->line[k + 1]])
                || !doc_line_len(d, k + 1))
                break;
            k++;
        }
        if (open < 0) {
            ln = k;
            continue;
        }
        for (end = open + 1; end < d->lines - 1 && d->text[d->line[end]] != '}'; end++) {}
        add_c_function(d, ln, end);
        ln = end;
    }
}

static inline void scan_md_symbols(Doc *d) {
    int ln;

    for (ln = 0; ln < d->lines; ln++) {
        const char *s = d->text + d->line[ln];
        int         n = doc_line_len(d, ln), i = 0;

        if (d->lineState[ln] != ST_NORMAL || !n || s[0] != '#')
            continue;
        while (i < n && s[i] == '#')
            i++;
        while (i < n && is_blank(s[i]))
            i++;
        add_symbol(d, ln, ln, d->line[ln] + i, n - i, SYM_HEADING);
    }
}

/* A JSON outline is its shallowest keys: those on the document's first indent. */
static inline void scan_json_symbols(Doc *d) {
    int ln, depth = -1;

    for (ln = 0; ln < d->lines; ln++) {
        const char *s = d->text + d->line[ln];
        int         n = doc_line_len(d, ln), i = 0, j;

        while (i < n && is_blank(s[i]))
            i++;
        if (i >= n || s[i] != '"')
            continue;
        if (depth < 0)
            depth = i;
        if (i != depth)
            continue;
        j = skip_quoted(s, i, n);
        if (j < n && s[j] == ':')
            add_symbol(d, ln, ln, d->line[ln] + i + 1, j - i - 2, SYM_KEY);
    }
}

/* The list's last row says it stopped, rather than a file's problems ending quietly. */
static inline void add_problem(Doc *d, int line, int col, int sev, const char *msg) {
    Problem *p;

    if (d->nProb == MAX_PROBLEMS)
        return;
    if (d->nProb == MAX_PROBLEMS - 1) {
        sev = SEV_INFO;
        msg = "The list stops here: this file has more problems";
    }
    p = &d->prob[d->nProb++];
    p->line = line;
    p->col  = col;
    p->severity = (uint8_t)sev;
    rcStrCopy(p->msg, msg, sizeof p->msg);
    d->errors   += sev == SEV_ERROR;
    d->warnings += sev == SEV_WARNING;
}

/* Brackets must balance, outside strings and comments. An error names the line it opened on. */
static inline void check_c_brackets(Doc *d, RC_Arena *a) {
    static const char OPEN[] = "([{", CLOSE[] = ")]}";
    int  stackLine[64], stackCol[64], depth = 0, ln;
    char stack[64];
    Token toks[256];

    for (ln = 0; ln < d->lines; ln++) {
        const char *s     = d->text + d->line[ln];
        int         n     = doc_line_len(d, ln), t;
        uint8_t     state = d->lineState[ln];
        TokenList   l     = { toks, 256, 0 };

        tokenize_line(d->lang, s, n, &state, &l);
        for (t = 0; t < l.n; t++) {
            int i;

            if (toks[t].kind != TOK_PLAIN)
                continue;
            for (i = toks[t].start; i < toks[t].end; i++) {
                int k;

                for (k = 0; k < 3; k++) {
                    if (s[i] == OPEN[k] && depth < 64) {
                        stack[depth] = OPEN[k];
                        stackLine[depth] = ln;
                        stackCol[depth++] = i;
                    } else if (s[i] == CLOSE[k]) {
                        if (depth && stack[depth - 1] == OPEN[k]) {
                            depth--;
                        } else {
                            add_problem(d, ln, i, SEV_ERROR,
                                        rcFormat(a, "Unmatched '%c'", s[i]).chars);
                            return;
                        }
                    }
                }
            }
        }
    }
    if (depth)
        add_problem(d, stackLine[depth - 1], stackCol[depth - 1], SEV_ERROR,
                    rcFormat(a, "'%c' is never closed: expected before the end of the file",
                             stack[depth - 1]).chars);
}

/* The rules every text file shares, and the ones a code file adds. */
static inline void check_lines(Doc *d, RC_Arena *a) {
    int ln;

    for (ln = 0; ln < d->lines; ln++) {
        const char *s = d->text + d->line[ln];
        int         n = doc_line_len(d, ln), i, chars = 0;
        bool        code = d->lang == LANG_C;

        for (i = 0; i < n; i++)
            chars += !is_tail_byte(s[i]);
        if (code && chars > LINE_LIMIT)
            add_problem(d, ln, LINE_LIMIT, SEV_WARNING,
                        rcFormat(a, "Line is %d columns long (limit %d)", chars, LINE_LIMIT).chars);
        if (n && is_blank(s[n - 1]) && d->lang != LANG_MARKDOWN)
            add_problem(d, ln, n - 1, SEV_WARNING, "Trailing whitespace");
        for (i = 0; code && i < n; i++) {
            if (s[i] == '\t') {
                add_problem(d, ln, i, SEV_WARNING, "Tab character: the style is 4 spaces");
                i = n;
            }
        }
        for (i = 0; i + 4 <= n; i++) {
            if (starts_with(s + i, n - i, "TODO") || starts_with(s + i, n - i, "FIXME")) {
                add_problem(d, ln, i, SEV_INFO,
                            rcFormat(a, "%.*s", n - i > 80 ? 80 : n - i, s + i).chars);
                i = n;
            }
        }
    }
    if (d->len && d->text[d->len - 1] != '\n' && d->lang != LANG_JSON)
        add_problem(d, d->lines - 1, doc_line_len(d, d->lines - 1), SEV_INFO,
                    "No newline at the end of the file");
}

static inline void check_c_functions(Doc *d, RC_Arena *a) {
    int i;

    for (i = 0; i < d->nSym; i++) {
        const Symbol *s = &d->sym[i];
        int           lines = s->endLine - s->line + 1;

        if (s->kind == SYM_FUNCTION && lines > FUNCTION_LIMIT)
            add_problem(d, s->line, 0, SEV_INFO,
                        rcFormat(a, "Function '%.*s' is %d lines long (limit %d)", s->nameLen,
                                 d->text + s->name, lines, FUNCTION_LIMIT).chars);
    }
}

static inline void check_json(Doc *d, RC_Arena *a) {
    RC_Json root = rcJsonParse(d->text, (size_t)d->len);

    if (rcJsonType(root) == RC_JSON_INVALID)
        add_problem(d, 0, 0, SEV_ERROR,
                    rcFormat(a, "Invalid JSON: %s", rcJsonError(root)).chars);
    rcJsonFree(root);
}

/* The preview shows the buffer, so a parse that fails is the file's own error to report. The
   handle is parsed here, once per edit: never per frame. */
static inline void check_svg(Doc *d) {
    rcUnloadSvg(&d->svg);
    d->svg       = rcLoadSvgFromMemory(d->text, d->len);
    d->svgFailed = d->svg == NULL;
    if (d->svgFailed)
        add_problem(d, 0, 0, SEV_ERROR, "This markup does not parse as an SVG");
}

/* Bring the outline and the problems up to the text. */
static inline void doc_analyze(Doc *d, RC_Arena *a) {
    if (!d->stale || d->unreadable)
        return;
    d->stale = false;
    d->sym   = (Symbol *)block_reserve(&d->symMem, MAX_SYMBOLS * sizeof(Symbol));
    d->prob  = (Problem *)block_reserve(&d->probMem, MAX_PROBLEMS * sizeof(Problem));
    d->nSym  = d->nProb = d->errors = d->warnings = 0;
    if (!d->sym || !d->prob)
        return;
    if (d->lang == LANG_C) {
        scan_c_symbols(d);
        check_c_brackets(d, a);
        check_c_functions(d, a);
    } else if (d->lang == LANG_MARKDOWN) {
        scan_md_symbols(d);
    } else if (d->lang == LANG_JSON) {
        scan_json_symbols(d);
        check_json(d, a);
    }
    if (ends_with(d->name, ".svg"))
        check_svg(d);
    check_lines(d, a);
    if (d->nSym == MAX_SYMBOLS)
        add_problem(d, 0, 0, SEV_INFO, "The outline stops here: this file has more symbols");
}

/* The innermost symbol holding a line, for the breadcrumb. */
static inline const Symbol *symbol_at(const Doc *d, int line) {
    const Symbol *best = NULL;
    int           i;

    for (i = 0; i < d->nSym; i++) {
        if (d->sym[i].line > line)
            break;
        if (d->sym[i].kind == SYM_HEADING || line <= d->sym[i].endLine)
            best = &d->sym[i];
    }
    return best;
}

#endif /* APP_ANALYSIS_H */
