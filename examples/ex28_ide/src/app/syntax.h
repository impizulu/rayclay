/*
    syntax.h - one line of source in, colored tokens out

    An editor only colors what is on screen, so a tokenizer takes a single line. What a line
    cannot know by itself (is this inside a block comment, a Markdown fence?) is the `state`
    byte: the state a line ends in is the state the next line starts in, and doc_reindex keeps
    one per line, so the view can tokenize any line of any file at once.
*/
#ifndef APP_SYNTAX_H
#define APP_SYNTAX_H

#include "rayclay.h"
#include "app/text.h"
#include "app/theme.h"

enum { LANG_PLAIN, LANG_C, LANG_MARKDOWN, LANG_JSON, LANG_XML, LANG_SCRIPT, LANG_COUNT };

/* The state a line starts in. */
enum { ST_NORMAL, ST_COMMENT, ST_FENCE };

typedef struct {
    int     start, end;
    uint8_t kind;
} Token;

typedef struct {
    Token *out;     /* NULL: only the state is wanted, as when a whole file is indexed */
    int    cap, n;
} TokenList;

/* Adjacent tokens of one kind merge, so a line of plain punctuation is one text run. */
static inline void tok_push(TokenList *l, int start, int end, int kind) {
    if (!l->out || end <= start)
        return;
    if (l->n && (l->out[l->n - 1].kind == kind || l->n == l->cap)) {
        l->out[l->n - 1].end = end;
        return;
    }
    l->out[l->n].start = start;
    l->out[l->n].end   = end;
    l->out[l->n].kind  = (uint8_t)kind;
    l->n++;
}

static inline bool word_is(const char *s, int n, const char *word) {
    int i;

    for (i = 0; i < n; i++)
        if (s[i] != word[i] || !word[i])
            return false;
    return !word[n];
}

static inline bool word_in(const char *s, int n, const char *const *list, int count) {
    int i;

    for (i = 0; i < count; i++)
        if (list[i][0] == s[0] && word_is(s, n, list[i]))
            return true;
    return false;
}

static const char *const C_CONTROL[] = {
    "if", "else", "for", "while", "do", "switch", "case", "default", "break", "continue",
    "return", "goto",
};

static const char *const C_KEYWORD[] = {
    "auto", "bool", "char", "class", "const", "constexpr", "double", "enum", "extern", "false",
    "float", "inline", "int", "long", "namespace", "nullptr", "register", "restrict", "short",
    "signed", "sizeof", "static", "struct", "template", "this", "true", "typedef", "typename",
    "union", "unsigned", "using", "void", "volatile", "NULL",
};

#define COUNT_OF(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* Dark+ with semantic colors: control flow purple, storage blue, calls yellow, types teal,
   constants bright blue, every other name light blue. A C file reads by shape at a glance. */
static inline int c_word_kind(const char *s, int i, int j, int n) {
    const char *w  = s + i;
    int         wn = j - i, k = j, caps = 1, a;

    if (word_in(w, wn, C_CONTROL, COUNT_OF(C_CONTROL)))
        return TOK_CONTROL;
    if (word_in(w, wn, C_KEYWORD, COUNT_OF(C_KEYWORD)))
        return TOK_KEYWORD;
    while (k < n && is_blank(s[k]))
        k++;
    if (k < n && s[k] == '(')
        return TOK_FUNCTION;
    for (a = 0; a < wn; a++)
        if (w[a] >= 'a' && w[a] <= 'z')
            caps = 0;
    if (caps && wn > 1)
        return TOK_CONSTANT;
    if ((w[0] >= 'A' && w[0] <= 'Z') || (wn > 2 && w[wn - 2] == '_' && w[wn - 1] == 't'))
        return TOK_TYPE;
    return TOK_VARIABLE;
}

/* Past a quoted run, with its escapes. An unclosed one ends with the line. */
static inline int skip_quoted(const char *s, int i, int n) {
    char q = s[i++];

    while (i < n && s[i] != q)
        i += s[i] == '\\' ? 2 : 1;
    return i < n ? i + 1 : n;
}

static inline void tokenize_c(const char *s, int n, uint8_t *state, TokenList *l) {
    int  i = 0, first = 0;
    bool directive;

    while (first < n && is_blank(s[first]))
        first++;
    directive = first < n && s[first] == '#';
    while (i < n) {
        char c = s[i];
        int  j = i;

        if (*state == ST_COMMENT) {
            while (j < n && !(s[j] == '*' && j + 1 < n && s[j + 1] == '/'))
                j++;
            if (j < n) {
                j += 2;
                *state = ST_NORMAL;
            }
            tok_push(l, i, j, TOK_COMMENT);
        } else if (c == '/' && i + 1 < n && s[i + 1] == '/') {
            j = n;
            tok_push(l, i, j, TOK_COMMENT);
        } else if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            *state = ST_COMMENT;
            j = i + 2;
            tok_push(l, i, j, TOK_COMMENT);
        } else if (c == '"' || c == '\'') {
            j = skip_quoted(s, i, n);
            tok_push(l, i, j, TOK_STRING);
        } else if (directive && i == first) {
            j = i + 1;
            while (j < n && is_blank(s[j]))
                j++;
            while (j < n && is_word(s[j]))
                j++;
            tok_push(l, i, j, TOK_PREPROC);
        } else if (is_digit(c) || (c == '.' && i + 1 < n && is_digit(s[i + 1]))) {
            while (j < n && (is_word(s[j]) || s[j] == '.'))
                j++;
            tok_push(l, i, j, TOK_NUMBER);
        } else if (is_word(c)) {
            while (j < n && is_word(s[j]))
                j++;
            if (l->out)
                tok_push(l, i, j, c_word_kind(s, i, j, n));
        } else if (c == '<' && directive && first + 1 < n && s[first + 1] == 'i') {
            while (j < n && s[j] != '>')
                j++;
            j = j < n ? j + 1 : n;
            tok_push(l, i, j, TOK_STRING);
        } else {
            j = i + 1;
            tok_push(l, i, j, TOK_PLAIN);
        }
        i = j;
    }
}

static inline bool starts_with(const char *s, int n, const char *lit) {
    int i;

    for (i = 0; lit[i]; i++)
        if (i >= n || s[i] != lit[i])
            return false;
    return true;
}

/* A Markdown line: block shape first, then the inline runs that matter for reading. */
static inline void tokenize_md(const char *s, int n, uint8_t *state, TokenList *l) {
    int i = 0, close = -1;

    if (starts_with(s, n, "```")) {
        *state = *state == ST_FENCE ? ST_NORMAL : ST_FENCE;
        tok_push(l, 0, n, TOK_COMMENT);
        return;
    }
    if (*state == ST_FENCE) {
        tok_push(l, 0, n, TOK_STRING);
        return;
    }
    if (n && s[0] == '#') {
        tok_push(l, 0, n, TOK_HEADING);
        return;
    }
    if (n && s[0] == '>') {
        tok_push(l, 0, n, TOK_COMMENT);
        return;
    }
    while (i < n && is_blank(s[i]))
        i++;
    if (i + 1 < n && (s[i] == '-' || s[i] == '*') && s[i + 1] == ' ') {
        tok_push(l, 0, i + 1, TOK_KEYWORD);
        i += 1;
    } else {
        tok_push(l, 0, i, TOK_PLAIN);
    }
    while (i < n) {
        int j = i + 1;

        if (s[i] == '`') {
            while (j < n && s[j] != '`')
                j++;
            j = j < n ? j + 1 : n;
            tok_push(l, i, j, TOK_STRING);
        } else if (s[i] == '*' && i + 1 < n && s[i + 1] == '*') {
            j = i + 2;
            while (j + 1 < n && !(s[j] == '*' && s[j + 1] == '*'))
                j++;
            j = j + 1 < n ? j + 2 : n;
            tok_push(l, i, j, TOK_CONSTANT);
        } else if (s[i] == '[') {
            /* Every '[' before a ']' finds that same ']', so it is looked for once and kept:
               a search per '[' would read the rest of the line again each time, which is
               quadratic in a line of many '['. */
            if (close < i)
                for (close = i; close < n && s[close] != ']'; close++) {}
            if (close + 1 < n && s[close + 1] == '(') {
                j = close + 1;
                while (j < n && s[j] != ')')
                    j++;
                j = j < n ? j + 1 : n;
                tok_push(l, i, close + 1, TOK_FUNCTION);
                tok_push(l, close + 1, j, TOK_LINK);
            } else {
                tok_push(l, i, j, TOK_PLAIN);
            }
        } else {
            tok_push(l, i, j, TOK_PLAIN);
        }
        i = j;
    }
}

static inline void tokenize_json(const char *s, int n, TokenList *l) {
    int i = 0;

    while (i < n) {
        char c = s[i];
        int  j = i + 1;

        if (c == '"') {
            int k;

            j = skip_quoted(s, i, n);
            k = j;
            while (k < n && is_blank(s[k]))
                k++;
            tok_push(l, i, j, k < n && s[k] == ':' ? TOK_ATTR : TOK_STRING);
        } else if (is_digit(c) || c == '-') {
            while (j < n && (is_digit(s[j]) || s[j] == '.' || s[j] == 'e' || s[j] == 'E'
                             || s[j] == '+' || s[j] == '-'))
                j++;
            tok_push(l, i, j, TOK_NUMBER);
        } else if (is_word(c)) {
            while (j < n && is_word(s[j]))
                j++;
            tok_push(l, i, j, TOK_KEYWORD);
        } else {
            tok_push(l, i, j, TOK_PLAIN);
        }
        i = j;
    }
}

/* XML and SVG: tags and attribute names and their quoted values. */
static inline void tokenize_xml(const char *s, int n, uint8_t *state, TokenList *l) {
    int  i = 0;
    bool inTag = false;

    while (i < n) {
        int j = i + 1;

        if (*state == ST_COMMENT) {
            j = i;
            while (j < n && !starts_with(s + j, n - j, "-->"))
                j++;
            if (j < n) {
                j += 3;
                *state = ST_NORMAL;
            }
            tok_push(l, i, j, TOK_COMMENT);
        } else if (starts_with(s + i, n - i, "<!--")) {
            *state = ST_COMMENT;
            j = i + 4;
            tok_push(l, i, j, TOK_COMMENT);
        } else if (s[i] == '<') {
            inTag = true;
            while (j < n && s[j] != ' ' && s[j] != '>' && s[j] != '/')
                j++;
            tok_push(l, i, j, TOK_TAG);
        } else if (s[i] == '>' || (s[i] == '/' && i + 1 < n && s[i + 1] == '>')) {
            inTag = false;
            j = s[i] == '/' ? i + 2 : i + 1;
            tok_push(l, i, j, TOK_TAG);
        } else if (s[i] == '"' || s[i] == '\'') {
            j = skip_quoted(s, i, n);
            tok_push(l, i, j, TOK_STRING);
        } else if (is_word(s[i]) && (inTag || i > 0)) {
            while (j < n && (is_word(s[j]) || s[j] == '-' || s[j] == ':'))
                j++;
            tok_push(l, i, j, j < n && s[j] == '=' ? TOK_ATTR : TOK_PLAIN);
        } else {
            tok_push(l, i, j, TOK_PLAIN);
        }
        i = j;
    }
}

static const char *const SCRIPT_WORDS[] = {
    "if", "else", "elif", "fi", "then", "for", "while", "do", "done", "case", "esac", "return",
    "function", "def", "class", "import", "from", "in", "not", "and", "or", "set", "endif",
    "foreach", "endforeach", "None", "True", "False",
};

/* Shell, Python, CMake, YAML: the same three shapes, enough to read them by. */
static inline void tokenize_script(const char *s, int n, TokenList *l) {
    int i = 0;

    while (i < n) {
        char c = s[i];
        int  j = i + 1;

        if (c == '#') {
            j = n;
            tok_push(l, i, j, TOK_COMMENT);
        } else if (c == '"' || c == '\'') {
            j = skip_quoted(s, i, n);
            tok_push(l, i, j, TOK_STRING);
        } else if (is_digit(c)) {
            while (j < n && (is_word(s[j]) || s[j] == '.'))
                j++;
            tok_push(l, i, j, TOK_NUMBER);
        } else if (is_word(c)) {
            while (j < n && is_word(s[j]))
                j++;
            tok_push(l, i, j, word_in(s + i, j - i, SCRIPT_WORDS, COUNT_OF(SCRIPT_WORDS))
                              ? TOK_CONTROL : TOK_PLAIN);
        } else {
            tok_push(l, i, j, TOK_PLAIN);
        }
        i = j;
    }
}

/* Tokens for one line, contiguous from 0 to n. `state` goes in as the line's start state and
   comes out as the next line's. With l->out NULL only the state is computed. */
static inline void tokenize_line(int lang, const char *s, int n, uint8_t *state, TokenList *l) {
    switch (lang) {
    case LANG_C:       tokenize_c(s, n, state, l);   break;
    case LANG_MARKDOWN: tokenize_md(s, n, state, l); break;
    case LANG_XML:     tokenize_xml(s, n, state, l); break;
    case LANG_JSON:    tokenize_json(s, n, l);       break;
    case LANG_SCRIPT:  tokenize_script(s, n, l);     break;
    default:           tok_push(l, 0, n, TOK_PLAIN); break;
    }
}

static inline bool lang_has_state(int lang) {
    return lang == LANG_C || lang == LANG_MARKDOWN || lang == LANG_XML;
}

static inline int lang_for(const char *name) {
    static const struct { const char *ext; int lang; } TABLE[] = {
        { ".c", LANG_C }, { ".h", LANG_C }, { ".cpp", LANG_C }, { ".hpp", LANG_C },
        { ".cc", LANG_C }, { ".mm", LANG_C }, { ".m", LANG_C },
        { ".md", LANG_MARKDOWN }, { ".json", LANG_JSON }, { ".svg", LANG_XML },
        { ".xml", LANG_XML }, { ".html", LANG_XML }, { ".plist", LANG_XML },
        { ".sh", LANG_SCRIPT }, { ".py", LANG_SCRIPT }, { ".cmake", LANG_SCRIPT },
        { "cmakelists.txt", LANG_SCRIPT }, { ".yml", LANG_SCRIPT }, { ".yaml", LANG_SCRIPT },
    };
    int i;

    for (i = 0; i < COUNT_OF(TABLE); i++)
        if (ends_with(name, TABLE[i].ext))
            return TABLE[i].lang;
    return LANG_PLAIN;
}

/* The status bar's words for a file. */
static inline const char *lang_label(const char *name) {
    if (ends_with(name, ".c") || ends_with(name, ".h"))
        return "C";
    if (ends_with(name, ".cpp") || ends_with(name, ".hpp") || ends_with(name, ".cc"))
        return "C++";
    if (ends_with(name, ".md"))
        return "Markdown";
    if (ends_with(name, ".json"))
        return "JSON";
    if (ends_with(name, ".svg"))
        return "SVG";
    if (ends_with(name, ".sh"))
        return "Shell Script";
    if (ends_with(name, ".py"))
        return "Python";
    if (ends_with(name, ".cmake") || ends_with(name, "cmakelists.txt"))
        return "CMake";
    if (lang_for(name) == LANG_XML)
        return "XML";
    return "Plain Text";
}

/* The file-type mark in the tree and on a tab: a glyph or two and a color, as icon themes do. */
typedef struct {
    const char *mark;
    RC_Color    color;
} FileMark;

static inline FileMark file_mark(const char *name) {
    static const RC_Color BLUE = { 81, 154, 186, 255 }, PURPLE = { 160, 116, 196, 255 };
    static const RC_Color YELLOW = { 203, 203, 65, 255 }, ORANGE = { 227, 121, 51, 255 };
    static const RC_Color GREEN = { 141, 193, 73, 255 }, GRAY = { 150, 150, 150, 255 };
    FileMark m = { "\xc2\xb7", GRAY };

    if (ends_with(name, ".c") || ends_with(name, ".cc") || ends_with(name, ".cpp")) {
        m.mark = "C";
        m.color = BLUE;
    } else if (ends_with(name, ".h") || ends_with(name, ".hpp")) {
        m.mark = "h";
        m.color = PURPLE;
    } else if (ends_with(name, ".json")) {
        m.mark = "{}";
        m.color = YELLOW;
    } else if (ends_with(name, ".md")) {
        m.mark = "M";
        m.color = BLUE;
    } else if (ends_with(name, ".svg") || ends_with(name, ".xml") || ends_with(name, ".html")) {
        m.mark = "<>";
        m.color = ORANGE;
    } else if (ends_with(name, ".py") || ends_with(name, ".sh")) {
        m.mark = "$";
        m.color = GREEN;
    }
    return m;
}

#endif /* APP_SYNTAX_H */
