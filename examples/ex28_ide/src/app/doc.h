/*
    doc.h - one open file: a flat byte buffer, a line table and an undo log

    The simplest editor buffer that stays fast: the text is one contiguous block, an edit moves
    the bytes after it and rebuilds the line table. On the files people keep in a repository
    that is well under a millisecond, and every read in the app is a plain array read.

    NOTHING HERE TOUCHES THE DISK. A Doc holds the bytes it was given; the one call that writes
    a file is rcSaveFile in doc_write (workspace.h), reached only from an explicit Save.
*/
#ifndef APP_DOC_H
#define APP_DOC_H

#include "rayclay.h"
#include "app/syntax.h"
#include "app/text.h"

/* DOC_BYTES is the most a buffer grows to: every offset in it is an int, so an edit past it is
   refused. LOG_BYTES is what the undo log keeps, its oldest steps going first. */
enum {
    NAME_BYTES = 96, PATH_BYTES = 512, MSG_BYTES = 96, MAX_STEPS = 4096, DOC_BYTES = 64 << 20,
    LOG_BYTES = 64 << 20
};

/* Severity is an index into the Problems panel's icons and the status bar's counts. */
enum { SEV_ERROR, SEV_WARNING, SEV_INFO };

typedef struct {
    int     line, col;
    uint8_t severity;
    char    msg[MSG_BYTES];
} Problem;

enum { SYM_FUNCTION, SYM_TYPE, SYM_HEADING, SYM_KEY };

typedef struct {
    int     line, endLine;  /* the span it covers, for the breadcrumb */
    int     name, nameLen;  /* byte offset of its name in the text    */
    uint8_t kind;
} Symbol;

/* What a step did, so typing merges into one undo and a paste stays its own. */
enum { EDIT_OTHER, EDIT_TYPING };

typedef struct {
    int     at, oldLen, newLen; /* `oldLen` bytes at `at` became `newLen` bytes           */
    size_t  bytes;              /* where the old bytes, then the new, start in the log    */
    int     caret, anchor;      /* the selection before the edit, restored by an undo     */
    uint8_t kind;
} Step;

typedef struct {
    char     name[NAME_BYTES];  /* the tab's words                                    */
    char     path[PATH_BYTES];  /* "" for a buffer no file stands behind              */
    char     scrollId[16];      /* the code view's id: each file keeps its own place  */
    int      lang;
    bool     unreadable;        /* binary or too large: a notice, not an editor       */
    bool     memoryOnly;        /* no file of the user's: Save hands them the bytes   */
    bool     imported;          /* its path is a private copy, released on close      */
    char     notice[MSG_BYTES];
    double   diskTime;          /* the file's time when read or last saved            */

    RC_Arena textMem, lineMem, stateMem, stepMem, logMem, symMem, probMem;
    char    *text;
    int      len;
    int     *line;              /* the byte offset each line starts at                */
    uint8_t *lineState;         /* the tokenizer state each line starts in            */
    int      lines;
    int      longest;           /* the widest line, in characters                     */
    bool     crlf;              /* the file's first line ending, kept on new lines    */
    bool     tabs;              /* a line is indented with a tab: the status bar says */

    int      caret, anchor;     /* byte offsets; equal when nothing is selected       */
    float    wantX;             /* where Up and Down aim, kept across short lines     */
    float    scrollX, scrollY;  /* the place it was left at                           */

    Step    *steps;
    int      top, count;        /* steps below top are done, from top to count undone */
    int      savedTop;          /* top when last saved: at it, the file is clean      */
    size_t   logLen;

    Symbol  *sym;
    int      nSym;
    Problem *prob;
    int      nProb, errors, warnings;
    unsigned rev;               /* bumped by every edit, so caches can tell           */
    bool     stale;             /* symbols and problems are behind the text           */
    bool     atLimit;           /* an edit was refused at DOC_BYTES: Output says so   */

    RC_Svg  *svg;               /* the live preview of an .svg, parsed from the text  */
    bool     svgFailed;
} Doc;

static inline bool doc_dirty(const Doc *d) { return d->top != d->savedTop; }

/* The bytes of line `ln`, its line ending left out. */
static inline int doc_line_len(const Doc *d, int ln) {
    int start = d->line[ln];
    int end   = ln + 1 < d->lines ? d->line[ln + 1] - 1 : d->len;

    if (end > start && d->text[end - 1] == '\r')
        end--;
    return end - start;
}

/* The line holding byte `at`. */
static inline int doc_line_of(const Doc *d, int at) {
    int lo = 0, hi = d->lines - 1;

    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;

        if (d->line[mid] <= at)
            lo = mid;
        else
            hi = mid - 1;
    }
    return lo;
}

/* Rebuild the line table and each line's start state: one pass over the text. */
static inline void doc_reindex(Doc *d) {
    uint8_t   state = ST_NORMAL;
    bool      track = lang_has_state(d->lang);
    TokenList none  = { 0 };
    int       lines = 1, i, ln = 0, chars = 0, tabs = 0;

    for (i = 0; i < d->len; i++)
        lines += d->text[i] == '\n';
    d->line      = (int *)block_reserve(&d->lineMem, (size_t)lines * sizeof(int));
    d->lineState = (uint8_t *)block_reserve(&d->stateMem, (size_t)lines);
    d->lines     = lines;
    d->longest   = 0;
    if (!d->line || !d->lineState) {
        d->lines = 0;
        return;
    }
    d->line[0] = 0;
    for (i = 0; i <= d->len; i++) {
        if (i == d->len || d->text[i] == '\n') {
            int start = d->line[ln];

            if (chars + tabs * 3 > d->longest)
                d->longest = chars + tabs * 3;
            d->lineState[ln] = state;
            if (track)
                tokenize_line(d->lang, d->text + start, i - start, &state, &none);
            chars = tabs = 0;
            if (i < d->len)
                d->line[++ln] = i + 1;
        } else if (!is_tail_byte(d->text[i])) {
            chars++;
            tabs += d->text[i] == '\t';
        }
    }
}

/* Replace bytes: the one place the text changes. */
static inline bool doc_splice(Doc *d, int at, int oldLen, const char *bytes, int newLen) {
    int   tail = d->len - at - oldLen;
    char *text = (char *)block_reserve(&d->textMem, (size_t)(d->len - oldLen + newLen) + 1);

    if (!text)
        return false;
    d->text = text;
    move_bytes(text + at + newLen, text + at + oldLen, tail);
    if (newLen)
        move_bytes(text + at, bytes, newLen);
    d->len += newLen - oldLen;
    text[d->len] = '\0';
    d->rev++;
    d->stale = true;
    doc_reindex(d);
    return true;
}

/* Room in the undo log for a step of `need` bytes. Past MAX_STEPS steps or LOG_BYTES bytes the
   oldest half goes, and its bytes with it, so a long session's log stays this size. There is no
   way back past what went, and a save point among it is lost. The redo branch is already cut. */
static inline void doc_forget(Doc *d, size_t need) {
    int    drop = 0, i;
    size_t base;

    if (d->count < MAX_STEPS && d->logLen + need <= LOG_BYTES)
        return;
    while (drop < d->count && (d->count - drop > MAX_STEPS / 2
                               || d->logLen - d->steps[drop].bytes + need > LOG_BYTES / 2))
        drop++;
    base = drop < d->count ? d->steps[drop].bytes : d->logLen;
    for (i = drop; i < d->count; i++) {
        d->steps[i - drop] = d->steps[i];
        d->steps[i - drop].bytes -= base;
    }
    move_bytes((char *)d->logMem.buffer, (const char *)d->logMem.buffer + base,
               (int)(d->logLen - base));
    d->logLen  -= base;
    d->count   -= drop;
    d->top     -= drop;
    d->savedTop = d->savedTop >= drop ? d->savedTop - drop : -1;
}

/* An edit the user can undo. The log holds each step's old and new bytes in a row, so an undo
   is one splice and new typing past the top of the log only has to extend the last step.
   An edit past DOC_BYTES, or one memory cannot hold, leaves the text and the caret as they
   were and returns false, so a caller places nothing after an edit that did not happen. */
static inline bool doc_edit(Doc *d, int at, int oldLen, const char *bytes, int newLen, int kind) {
    size_t size = (size_t)(d->len - oldLen) + (size_t)newLen;
    Step  *prev;
    char  *log, *text;
    int    i;

    if (size > (size_t)DOC_BYTES) {
        d->atLimit = true;
        return false;
    }
    /* The text's room first: once the step is in the log, the splice cannot fail. */
    text = (char *)block_reserve(&d->textMem, size + 1);
    if (!text)
        return false;
    d->text = text;
    if (d->count > d->top) {            /* a new edit ends the redo branch */
        if (d->savedTop > d->top)       /* and with it, the saved state on that branch */
            d->savedTop = -1;
        d->logLen = d->steps[d->top].bytes;
        d->count  = d->top;
    }
    log = (char *)block_reserve(&d->logMem, d->logLen + (size_t)oldLen + (size_t)newLen);
    if (!log)
        return false;
    prev = d->top > 0 ? &d->steps[d->top - 1] : NULL;
    if (kind == EDIT_TYPING && prev && prev->kind == EDIT_TYPING && oldLen == 0
        && prev->at + prev->newLen == at && d->savedTop != d->top
        && d->logLen + (size_t)newLen <= LOG_BYTES) {
        prev->newLen += newLen;
    } else {
        Step *steps;

        doc_forget(d, (size_t)oldLen + (size_t)newLen);
        steps = (Step *)block_reserve(&d->stepMem, (size_t)(d->count + 1) * sizeof(Step));
        if (!steps)
            return false;
        d->steps = steps;
        steps[d->count] = RC_LIT(Step){ at, oldLen, newLen, d->logLen, d->caret, d->anchor,
                                        (uint8_t)kind };
        d->top = ++d->count;
        for (i = 0; i < oldLen; i++)
            log[d->logLen + (size_t)i] = d->text[at + i];
        d->logLen += (size_t)oldLen;
    }
    for (i = 0; i < newLen; i++)
        log[d->logLen + (size_t)i] = bytes[i];
    d->logLen += (size_t)newLen;
    doc_splice(d, at, oldLen, bytes, newLen);
    d->caret = d->anchor = at + newLen;
    return true;
}

static inline void doc_undo(Doc *d) {
    const Step *s;
    const char *log;

    if (d->top == 0)
        return;
    s   = &d->steps[--d->top];
    log = (const char *)d->logMem.buffer + s->bytes;
    doc_splice(d, s->at, s->newLen, log, s->oldLen);
    d->caret  = s->caret;
    d->anchor = s->anchor;
}

static inline void doc_redo(Doc *d) {
    const Step *s;
    const char *log;

    if (d->top == d->count)
        return;
    s   = &d->steps[d->top++];
    log = (const char *)d->logMem.buffer + s->bytes + s->oldLen;
    doc_splice(d, s->at, s->oldLen, log, s->newLen);
    d->caret = d->anchor = s->at + s->newLen;
}

static inline int sel_start(const Doc *d) { return d->caret < d->anchor ? d->caret : d->anchor; }
static inline int sel_end(const Doc *d) { return d->caret > d->anchor ? d->caret : d->anchor; }
static inline bool has_sel(const Doc *d) { return d->caret != d->anchor; }

/* Typing and pasting: what is selected gives way. */
static inline void doc_insert(Doc *d, const char *bytes, int n, int kind) {
    int at = sel_start(d);

    doc_edit(d, at, sel_end(d) - at, bytes, n, has_sel(d) ? EDIT_OTHER : kind);
}

static inline void doc_set_text(Doc *d, const char *bytes, int n) {
    int i;

    d->text = (char *)block_reserve(&d->textMem, (size_t)n + 1);
    if (!d->text)
        return;
    move_bytes(d->text, bytes, n);
    d->text[n] = '\0';
    d->len     = n;
    d->crlf    = false;
    d->tabs    = n > 0 && bytes[0] == '\t';
    for (i = 0; i + 1 < n; i++) {
        if (bytes[i] == '\n') {
            d->tabs = d->tabs || bytes[i + 1] == '\t';
            d->crlf = d->crlf || (i > 0 && bytes[i - 1] == '\r');
        }
    }
    d->stale = true;
    d->rev++;
    doc_reindex(d);
}

/* Give the memory back when a tab closes. */
static inline void doc_release(Doc *d) {
    rcArenaFree(&d->textMem);
    rcArenaFree(&d->lineMem);
    rcArenaFree(&d->stateMem);
    rcArenaFree(&d->stepMem);
    rcArenaFree(&d->logMem);
    rcArenaFree(&d->symMem);
    rcArenaFree(&d->probMem);
    rcUnloadSvg(&d->svg);
}

#endif /* APP_DOC_H */
