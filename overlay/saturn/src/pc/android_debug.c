// Android debugging helpers for Saturn 64 Mobile.
//  - Redirects stdout/stderr to logcat AND to "saturn_log.txt" (the previous run is kept as "saturn_log_prev.txt").
//  - Installs a crash handler that writes a backtrace to "crash.txt" (and logcat).
// The Java side (ImportActivity -> "Save debug log") bundles these files so they can be shared.
#ifdef __ANDROID__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <android/log.h>
#include <SDL2/SDL.h>
#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unwind.h>

// Folder where Saturn keeps everything (dynos, res, config, saves, logs). The Java side writes its path to
// "<internal files>/datadir.txt" (normally /storage/emulated/0/com.saturn64). Falls back to the internal folder.
char android_data_dir[1024] = { 0 };

void android_init_paths(void) {
    const char *internal = SDL_AndroidGetInternalStoragePath();
    char path[1024] = { 0 };
    if (internal) {
        char cfg[1200];
        snprintf(cfg, sizeof(cfg), "%s/datadir.txt", internal);
        FILE *f = fopen(cfg, "r");
        if (f) {
            if (fgets(path, sizeof(path), f)) {
                size_t n = strlen(path);
                while (n > 0 && (path[n - 1] == '\n' || path[n - 1] == '\r' || path[n - 1] == ' ')) path[--n] = 0;
            }
            fclose(f);
        }
    }
    if (path[0] && chdir(path) == 0) {
        strncpy(android_data_dir, path, sizeof(android_data_dir) - 1);
    } else if (internal) {
        chdir(internal);
        strncpy(android_data_dir, internal, sizeof(android_data_dir) - 1);
    }
}

// Status shown by the Java loading overlay (SaturnActivity): "<percent>\n<message>\n". percent < 0 hides it.
void android_set_status(int pct, const char *msg) {
    FILE *f = fopen("extract_status.txt", "w");
    if (f) {
        fprintf(f, "%d\n%s\n", pct, msg ? msg : "");
        fclose(f);
    }
}

static FILE *g_log = NULL;
static int g_pipe[2];

static void *log_thread(void *arg) {
    (void)arg;
    char line[1024];
    size_t len = 0;
    char chunk[512];
    ssize_t n;
    while ((n = read(g_pipe[0], chunk, sizeof(chunk))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            char ch = chunk[i];
            if (ch == '\n' || len >= sizeof(line) - 1) {
                line[len] = 0;
                __android_log_write(ANDROID_LOG_INFO, "Saturn", line);
                if (g_log) { fputs(line, g_log); fputc('\n', g_log); fflush(g_log); }
                len = 0;
                if (ch != '\n') line[len++] = ch;
            } else {
                line[len++] = ch;
            }
        }
    }
    return NULL;
}

struct bt_state { void **cur; void **end; };

static _Unwind_Reason_Code unwind_cb(struct _Unwind_Context *ctx, void *arg) {
    struct bt_state *st = (struct bt_state *)arg;
    uintptr_t pc = _Unwind_GetIP(ctx);
    if (pc) {
        if (st->cur == st->end) return _URC_END_OF_STACK;
        *st->cur++ = (void *)pc;
    }
    return _URC_NO_REASON;
}

static void emit(FILE *f, const char *text) {
    __android_log_write(ANDROID_LOG_ERROR, "SaturnCrash", text);
    if (f) { fputs(text, f); fputc('\n', f); }
    if (g_log) { fputs(text, g_log); fputc('\n', g_log); fflush(g_log); }
}

static void crash_handler(int sig, siginfo_t *info, void *ucontext) {
    (void)ucontext;
    FILE *f = fopen("crash.txt", "w");
    char line[512];
    snprintf(line, sizeof(line), "CRASH: signal %d (%s), code %d, fault address %p",
             sig, strsignal(sig), info ? info->si_code : 0, info ? info->si_addr : NULL);
    emit(f, line);

    void *frames[64];
    struct bt_state st = { frames, frames + 64 };
    _Unwind_Backtrace(unwind_cb, &st);
    int n = (int)(st.cur - frames);
    for (int i = 0; i < n; i++) {
        Dl_info di;
        if (dladdr(frames[i], &di) && di.dli_fname) {
            uintptr_t pc = (uintptr_t)frames[i];
            snprintf(line, sizeof(line), "#%02d pc 0x%lx  %s  (%s+%ld)", i,
                     (unsigned long)(pc - (uintptr_t)di.dli_fbase), di.dli_fname,
                     di.dli_sname ? di.dli_sname : "?",
                     di.dli_saddr ? (long)(pc - (uintptr_t)di.dli_saddr) : 0L);
        } else {
            snprintf(line, sizeof(line), "#%02d pc %p", i, frames[i]);
        }
        emit(f, line);
    }
    if (f) fclose(f);
    // SA_RESETHAND restored the default action; returning lets the process die normally.
}

void android_debug_start(void) {
    rename("saturn_log.txt", "saturn_log_prev.txt");
    g_log = fopen("saturn_log.txt", "w");

    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    if (pipe(g_pipe) == 0) {
        dup2(g_pipe[1], STDOUT_FILENO);
        dup2(g_pipe[1], STDERR_FILENO);
        pthread_t t;
        if (pthread_create(&t, NULL, log_thread, NULL) == 0) pthread_detach(t);
    }

    static char altstack[64 * 1024];
    stack_t ss;
    ss.ss_sp = altstack;
    ss.ss_size = sizeof(altstack);
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    int sigs[] = { SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL };
    for (unsigned i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++) sigaction(sigs[i], &sa, NULL);

    printf("Saturn Android debug log started\n");
}

#endif
