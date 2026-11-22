/*
    inspector_log.h - a fixed-capacity in-memory log ring for ex11.

    One interleaved stream of two sources: lines the app writes about its own
    events, and RayClay's own diagnostics, delivered by rcSetLogSink.
*/

#ifndef RC_INSPECTOR_LOG_H
#define RC_INSPECTOR_LOG_H

/* Fixed capacity, so the log is a flat member of AppState; the oldest line drops.
   A power of two, so a slot index stays right when the unsigned count wraps. */
#define INSP_LOG_CAP   256
#define INSP_LOG_MSG   160   /* per-line body; a longer message is truncated */

typedef enum {
    INSP_SRC_APP = 0,        /* the app narrating its own events */
    INSP_SRC_RAY = 1         /* RayClay, via rcSetLogSink       */
} InspLogSource;

/* Level mirrors RC_LogLevel, so a RayClay line keeps the library's own severity. The time is a
   double, as the app clock is: a float steps 1/64 s from 36 hours in, and the console prints
   milliseconds. */
typedef struct {
    unsigned char source;    /* InspLogSource */
    unsigned char level;     /* RC_LogLevel   */
    double        time;      /* seconds on the app clock */
    char          msg[INSP_LOG_MSG];
} InspLogLine;

/* `total` is unsigned so a long run wraps it instead of overflowing; `held` is
   counted on its own, so the wrap never empties the ring. */
typedef struct {
    InspLogLine   line[INSP_LOG_CAP];
    unsigned long total;     /* lines ever pushed (drives the ring head) */
    int           held;      /* live lines, <= capacity */
} InspectorLog;

/* The ring owns its bytes: the source is usually arena memory from rcFormat, valid
   only for this frame, and a log line outlives the frame it was written in. A line too long
   for the slot is cut before the UTF-8 character the cut would split: its stray bytes would
   draw as Latin-1 at the end of the line. */
static inline void insp_log_push(InspectorLog *log, InspLogSource source, int level,
                                 double time, const char *text) {
    InspLogLine *l = &log->line[log->total % INSP_LOG_CAP];
    int n = 0;

    while (n < INSP_LOG_MSG - 1 && text[n]) {
        l->msg[n] = text[n];
        n++;
    }
    /* Back to the split character's lead byte: UTF-8 continues one for at most 3 bytes. */
    for (int k = 0; k < 3 && n > 0 && ((unsigned char)text[n] & 0xC0) == 0x80; k++)
        n--;
    l->msg[n] = '\0';
    l->source = (unsigned char)source;
    l->level  = (unsigned char)level;
    l->time   = time;
    log->total++;
    if (log->held < INSP_LOG_CAP)
        log->held++;
}

static inline void insp_log_clear(InspectorLog *log) {
    log->total = 0;
    log->held  = 0;
}

/* The i-th live line, 0 == oldest still held. The subtraction is unsigned, so it
   lands on the right slot across the wrap. */
static inline const InspLogLine *insp_log_at(const InspectorLog *log, int i) {
    unsigned long first = log->total - (unsigned long)log->held;
    return &log->line[(first + (unsigned long)i) % INSP_LOG_CAP];
}

#endif /* RC_INSPECTOR_LOG_H */
