/*
    text.h - bytes, the one heap an example has, and the fuzzy match

    RayClay apps name no libc call, so the handful of byte loops an editor needs live here.
    A compiler recognizes these loops, so they cost what memmove and memcmp cost.
*/
#ifndef APP_TEXT_H
#define APP_TEXT_H

#include "rayclay.h"

static inline int str_len(const char *s) {
    int n = 0;

    while (s[n])
        n++;
    return n;
}

static inline void move_bytes(char *dst, const char *src, int n) {
    int i;

    if (dst < src)
        for (i = 0; i < n; i++)
            dst[i] = src[i];
    else
        for (i = n - 1; i >= 0; i--)
            dst[i] = src[i];
}

static inline char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }

static inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

static inline bool is_word(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || is_digit(c) || c == '_';
}

static inline bool is_blank(char c) { return c == ' ' || c == '\t'; }

static inline bool same_bytes(const char *a, const char *b, int n) {
    int i;

    for (i = 0; i < n; i++)
        if (a[i] != b[i])
            return false;
    return true;
}

static inline bool same_text(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

/* Folder-style order: case-blind, so "Makefile" sits among the m's. */
static inline int compare_names(const char *a, const char *b) {
    while (*a && lower(*a) == lower(*b)) {
        a++;
        b++;
    }
    return (int)(unsigned char)lower(*a) - (int)(unsigned char)lower(*b);
}

/* A path's last name. A dialog answers in the system's separator, the walk writes '/'. */
static inline const char *base_name(const char *path) {
    const char *name = path;

    for (; *path; path++)
        if (*path == '/' || *path == '\\')
            name = path + 1;
    return name;
}

static inline bool ends_with(const char *s, const char *tail) {
    int ns = str_len(s), nt = str_len(tail), i;

    if (nt > ns)
        return false;
    for (i = 0; i < nt; i++)
        if (lower(s[ns - nt + i]) != tail[i])
            return false;
    return true;
}

/* UTF-8: the caret steps over whole code points, never into one. */
static inline bool is_tail_byte(char c) { return ((unsigned char)c & 0xC0) == 0x80; }

/* RC_Arena is the allocator an example has, and only the bytes rcArenaAlloc hands out are
   the app's: a sanitizer build poisons the rest. So a block is a new arena claimed whole at
   once, and its claim, currOffset, is how much of it is usable. NULL when there is no memory. */
static inline char *block_new(RC_Arena *mem, size_t size) {
    *mem = rcArenaInit(size);
    return (char *)rcArenaAlloc(mem, size);
}

/* A growable block: moved to a larger one when it fills. */
static inline void *block_reserve(RC_Arena *mem, size_t need) {
    RC_Arena bigger;
    char    *to;
    size_t   i;

    if (mem->buffer && need <= mem->currOffset)
        return mem->buffer;
    to = block_new(&bigger, need + need / 2 + 256);
    if (!to) {
        rcArenaFree(&bigger);
        return NULL;
    }
    for (i = 0; i < mem->currOffset; i++)
        to[i] = (char)mem->buffer[i];
    rcArenaFree(mem);
    *mem = bigger;
    return to;
}

/* The quick-open match: every query character in order, case-blind, the score rewarding a
   word start and a run. `hits`, when given, receives the matched byte offsets to highlight.
   Returns the score, or -1 for no match. */
enum { FUZZY_MAX_HITS = 32 };

static inline int fuzzy_match(const char *query, const char *text, int *hits, int *nHits) {
    int score = 0, run = 0, n = 0, i = 0;

    for (; *query; query++) {
        char want = lower(*query);

        while (text[i] && lower(text[i]) != want) {
            i++;
            run = 0;
        }
        if (!text[i])
            return -1;
        if (i == 0 || !is_word(text[i - 1]))
            score += 12;
        else if (text[i - 1] >= 'a' && text[i] >= 'A' && text[i] <= 'Z')
            score += 8;
        score += 2 + run * 4;
        run++;
        if (hits && n < FUZZY_MAX_HITS)
            hits[n++] = i;
        i++;
    }
    if (nHits)
        *nHits = n;
    return score;
}

#endif /* APP_TEXT_H */
