/*
    inspector_log.h - a fixed-capacity in-memory log ring for ex12.

    One interleaved stream of two sources, ordered by a monotonic sequence number
    rather than a wall clock so the demo behaves the same everywhere: [APP] lines
    the app writes about its own events, and [RAY] the library's own RC_LOG_*
    diagnostics, delivered by rcSetLogSink.
*/

#ifndef RC_INSPECTOR_LOG_H
#define RC_INSPECTOR_LOG_H

/* Fixed capacity, so the log is a flat member of AppState; oldest line drops. */
#define INSP_LOG_CAP   256
#define INSP_LOG_MSG   128   /* per-line body; a longer message is truncated */

typedef enum {
    INSP_SRC_APP = 0,        /* the app narrating its own events */
    INSP_SRC_RAY = 1         /* RayClay, via rcSetLogSink       */
} InspLogSource;

/* Level mirrors RC_LogLevel (INFO=0 / WARNING=1 / ERROR=2), so a [RAY] line keeps
   the library's own severity. */
typedef struct {
    unsigned char source;    /* InspLogSource */
    unsigned char level;     /* RC_LogLevel   */
    long          seq;       /* monotonic order == a synthetic timestamp */
    char          msg[INSP_LOG_MSG];
} InspLogLine;

typedef struct {
    InspLogLine line[INSP_LOG_CAP];
    long        total;       /* lines ever pushed (drives seq + the ring head) */
} InspectorLog;

/* Copy at most cap-1 bytes and always null-terminate. THE RING OWNS ITS BYTES: the
   source is usually arena memory from rcFormat, valid only for the rest of THIS
   frame, and a log line outlives the frame it was written in. */
static inline void insp_copy(char *dst, int cap, const char *src, int len)
{
    int n = 0;
    if (len > cap - 1)
        len = cap - 1;
    while (n < len && src[n]) {
        dst[n] = src[n];
        n++;
    }
    dst[n] = '\0';
}

/* Push one line from a (chars, length) pair - the shape rcFormat hands back. */
static inline void insp_log_push_len(InspectorLog *log, InspLogSource source,
                                     int level, const char *chars, int len)
{
    InspLogLine *l = &log->line[(int)(log->total % INSP_LOG_CAP)];
    l->source = (unsigned char)source;
    l->level  = (unsigned char)level;
    l->seq    = log->total;
    insp_copy(l->msg, INSP_LOG_MSG, chars, len);
    log->total++;
}

/* Convenience for the null-terminated C string rcSetLogSink hands over; it bounds
   the length itself, so a rogue caller cannot over-read. */
static inline void insp_log_push(InspectorLog *log, InspLogSource source,
                                 int level, const char *cstr)
{
    int len = 0;
    while (len < INSP_LOG_MSG && cstr[len])
        len++;
    insp_log_push_len(log, source, level, cstr, len);
}

/* Live lines in the ring (<= capacity). */
static inline int insp_log_count(const InspectorLog *log)
{
    return log->total < INSP_LOG_CAP ? (int)log->total : INSP_LOG_CAP;
}

/* The i-th live line, 0 == oldest still held. */
static inline const InspLogLine *insp_log_at(const InspectorLog *log, int i)
{
    long first = log->total < INSP_LOG_CAP ? 0 : log->total - INSP_LOG_CAP;
    return &log->line[(int)((first + i) % INSP_LOG_CAP)];
}

#endif /* RC_INSPECTOR_LOG_H */
