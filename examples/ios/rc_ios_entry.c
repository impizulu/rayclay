/* iOS example launcher. Application sources keep their ordinary C entry point:
   the build renames it to rci_ios_example_main and this shim owns SDL_main.
   SDL's iOS port runs SDL_main synchronously on the UIKit main thread once the
   scene connects, and its pump re-enters the run loop, so the blocking rcRunApp
   loop runs unchanged.

   The working directory is the bundle root when the example runs. An
   example opens its files by the repo-relative literal it uses on the desktop
   ("examples/assets/fonts/Lato-Regular.ttf"); the build ships those files
   inside the bundle under the same relative directory (the example target's
   RC_MOBILE_ASSETS property lists them), so the literal resolves and the
   example carries no iOS code. SDL enters the bundle before SDL_main and goes
   on if it cannot; the shim enters it again and stops the run if it cannot. */
#undef main
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <errno.h>
#include <mach/mach.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#include "rayclay.h"

extern int rci_ios_example_main(void);

/* RAYCLAY_TRACE_CPU=<seconds>: what the process spends, read from inside it.
   A helper thread wakes every <seconds> (0.1 to 3600) and prints one line of
   what the whole process used since its previous line: user plus system CPU,
   the main thread's share, the wakeups the kernel charged it (interrupt and
   platform-idle, the two counts Activity Monitor reports), context switches
   and system calls. It exists because an outside reader changes what it reads
   on iOS: attaching xctrace resumes a suspended app. The helper adds one wakeup
   and a few system calls per line; unset, the knob costs one getenv. */
typedef struct {
    double wall;            /* CLOCK_MONOTONIC seconds */
    double cpu;             /* user + system seconds, every thread */
    double main_cpu;        /* the main thread's user + system seconds */
    uint64_t wakeups;       /* interrupt wakeups */
    uint64_t idle_wakeups;  /* platform idle wakeups */
    uint32_t csw;           /* context switches; the kernel counts in 32 bits */
    uint32_t syscalls;      /* mach + unix system calls; likewise */
} RCI_IosCpuSample;

static thread_act_t rci_ios_main_thread;
static double rci_ios_trace_period;

static double rci_ios_seconds(time_value_t t) {
    return (double)t.seconds + (double)t.microseconds / 1e6;
}

static void rci_ios_cpu_sample(RCI_IosCpuSample *s) {
    struct timespec now;
    struct rusage usage;
    thread_basic_info_data_t thread;
    mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
    task_power_info_data_t power;
    mach_msg_type_number_t power_count = TASK_POWER_INFO_COUNT;
    task_events_info_data_t events;
    mach_msg_type_number_t events_count = TASK_EVENTS_INFO_COUNT;

    memset(s, 0, sizeof(*s));
    clock_gettime(CLOCK_MONOTONIC, &now);
    s->wall = (double)now.tv_sec + (double)now.tv_nsec / 1e9;
    if (getrusage(RUSAGE_SELF, &usage) == 0)
        s->cpu = (double)usage.ru_utime.tv_sec + (double)usage.ru_utime.tv_usec / 1e6 +
                 (double)usage.ru_stime.tv_sec + (double)usage.ru_stime.tv_usec / 1e6;
    if (thread_info(rci_ios_main_thread, THREAD_BASIC_INFO, (thread_info_t)&thread,
                    &count) == KERN_SUCCESS)
        s->main_cpu = rci_ios_seconds(thread.user_time) + rci_ios_seconds(thread.system_time);
    if (task_info(mach_task_self(), TASK_POWER_INFO, (task_info_t)&power,
                  &power_count) == KERN_SUCCESS) {
        s->wakeups = power.task_interrupt_wakeups;
        s->idle_wakeups = power.task_platform_idle_wakeups;
    }
    if (task_info(mach_task_self(), TASK_EVENTS_INFO, (task_info_t)&events,
                  &events_count) == KERN_SUCCESS) {
        s->csw = (uint32_t)events.csw;
        s->syscalls = (uint32_t)events.syscalls_mach + (uint32_t)events.syscalls_unix;
    }
}

static void *rci_ios_cpu_trace(void *arg) {
    RCI_IosCpuSample last, next;
    struct timespec period;
    unsigned line = 0;

    (void)arg;
    period.tv_sec = (time_t)rci_ios_trace_period;
    period.tv_nsec = (long)((rci_ios_trace_period - (double)period.tv_sec) * 1e9);
    rci_ios_cpu_sample(&last);
    for (;;) {
        double dt;

        nanosleep(&period, NULL);
        rci_ios_cpu_sample(&next);
        dt = next.wall - last.wall;
        SDL_Log("rayclay cpu-trace n=%u dt=%.3f cpu_ms=%.3f main_ms=%.3f wakeups=%llu "
                "idle_wakeups=%llu csw=%u syscalls=%u", ++line, dt,
                (next.cpu - last.cpu) * 1e3, (next.main_cpu - last.main_cpu) * 1e3,
                (unsigned long long)(next.wakeups - last.wakeups),
                (unsigned long long)(next.idle_wakeups - last.idle_wakeups),
                (unsigned)(next.csw - last.csw), (unsigned)(next.syscalls - last.syscalls));
        last = next;
    }
    return NULL;
}

/* Main thread only: the main thread is the one sampled for main_ms. */
static void rci_ios_cpu_trace_start(void) {
    const char *value = getenv("RAYCLAY_TRACE_CPU");
    char *end = NULL;
    double period;
    pthread_attr_t attr;
    pthread_t thread;

    if (!value || !*value)
        return;
    period = strtod(value, &end);
    if (end == value || *end != '\0' || !(period >= 0.1 && period <= 3600.0)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "RAYCLAY_TRACE_CPU=%s is not 0.1 to 3600 seconds; ignored", value);
        return;
    }
    rci_ios_main_thread = pthread_mach_thread_np(pthread_self());
    rci_ios_trace_period = period;
    if (pthread_attr_init(&attr) != 0)
        return;
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&thread, &attr, rci_ios_cpu_trace, NULL) == 0)
        SDL_Log("rayclay cpu-trace period=%.3f", period);
    pthread_attr_destroy(&attr);
}

/* SDL_GetBasePath() is the bundle's resource directory, which on the flat
   iOS layout is the bundle itself. A failure here is a broken bundle, not a
   condition to run through: the app would draw with no assets and the run
   would read as a layout defect. */
static bool rci_ios_enter_bundle(void) {
    const char *root = SDL_GetBasePath();

    if (!root || chdir(root) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "rayclay bundle root unavailable: %s",
                     root ? strerror(errno) : "SDL_GetBasePath failed");
        return false;
    }
    SDL_Log("rayclay bundle=%s", root);
    return true;
}

static void rci_ios_log(RC_LogLevel level, const char *message, void *user) {
    SDL_LogPriority priority = level == RC_LOG_ERROR ? SDL_LOG_PRIORITY_ERROR :
                               level == RC_LOG_WARNING ? SDL_LOG_PRIORITY_WARN : SDL_LOG_PRIORITY_INFO;
    (void)user;
    SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, priority, "rayclay level=%d %s", (int)level, message);
}

int main(int argc, char **argv) {
    int rc;

    (void)argc;
    (void)argv;
    SDL_Log("rayclay app=%s", RCI_IOS_APP_NAME);
    if (!rci_ios_enter_bundle())
        exit(1);
    rcSetLogSink(rci_ios_log, NULL);
    rci_ios_cpu_trace_start();
    rc = rci_ios_example_main();
    /* SDL never leaves the UIKit run loop after SDL_main returns, so a
       frame-budgeted run would otherwise sit live and blank forever. */
    exit(rc);
}
