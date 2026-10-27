#ifdef __ANDROID__

#include "android_support.h"

#include <SDL2/SDL.h>
#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <dlfcn.h>
#include <unwind.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#define TAG "SaturnStudio"

char android_data_dir[4096] = "";

static char status_path[4400] = "";
static char crash_path[4400] = "";
static char last_status[128] = "";

static void mkdir_p(const char *path) {
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = 0; mkdir(tmp, 0775); *p = '/'; }
    }
    mkdir(tmp, 0775);
}

static int dir_writable(const char *path) {
    char probe[4400];
    snprintf(probe, sizeof(probe), "%s/.probe", path);
    FILE *f = fopen(probe, "w");
    if (!f) return 0;
    fclose(f);
    remove(probe);
    return 1;
}

void android_init_paths(void) {
    const char *internal = SDL_AndroidGetInternalStoragePath();
    char cfg[4400];
    char dir[4096] = "";

    // Java (ImportActivity) decides where the data lives and writes it here.
    if (internal) {
        snprintf(cfg, sizeof(cfg), "%s/datadir.txt", internal);
        FILE *f = fopen(cfg, "r");
        if (f) {
            if (fgets(dir, sizeof(dir), f)) {
                size_t n = strlen(dir);
                while (n && (dir[n - 1] == '\n' || dir[n - 1] == '\r' || dir[n - 1] == ' ')) dir[--n] = 0;
            }
            fclose(f);
        }
    }
    if (dir[0]) mkdir_p(dir);
    if (!dir[0] || !dir_writable(dir)) {
        const char *ext = SDL_AndroidGetExternalStoragePath();
        snprintf(dir, sizeof(dir), "%s", ext ? ext : (internal ? internal : "/data/local/tmp"));
        mkdir_p(dir);
    }
    size_t n = strlen(dir);
    while (n > 1 && dir[n - 1] == '/') dir[--n] = 0;
    snprintf(android_data_dir, sizeof(android_data_dir), "%s", dir);

    if (chdir(android_data_dir) != 0)
        __android_log_print(ANDROID_LOG_ERROR, TAG, "chdir(%s) failed: %s", android_data_dir, strerror(errno));

    remove("ready.flag");
    snprintf(status_path, sizeof(status_path), "%s/extract_status.txt", android_data_dir);
    snprintf(crash_path, sizeof(crash_path), "%s/crash.txt", android_data_dir);
    __android_log_print(ANDROID_LOG_INFO, TAG, "data dir: %s", android_data_dir);
}

void android_write_text_file(const char *name, const char *text) {
    char path[4400];
    snprintf(path, sizeof(path), "%s/%s", android_data_dir, name);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs(text, f);
    fclose(f);
}

void android_breadcrumb(const char *msg) {
    snprintf(last_status, sizeof(last_status), "%s", msg ? msg : "");
}

void android_set_status(float progress, const char *msg) {
    if (!status_path[0]) return;
    FILE *f = fopen(status_path, "w");
    if (!f) return;
    fprintf(f, "%.3f\n%s\n", progress, msg ? msg : "");
    fclose(f);
}

struct bt_state { FILE *f; int n; };

static _Unwind_Reason_Code bt_cb(struct _Unwind_Context *ctx, void *arg) {
    struct bt_state *st = (struct bt_state *)arg;
    uintptr_t pc = _Unwind_GetIP(ctx);
    if (!pc) return _URC_END_OF_STACK;
    Dl_info info;
    if (dladdr((void *)pc, &info)) {
        fprintf(st->f, "#%02d pc %p  %s (%s+%lu)\n", st->n, (void *)(pc - (uintptr_t)info.dli_fbase),
                info.dli_fname ? info.dli_fname : "?", info.dli_sname ? info.dli_sname : "?",
                info.dli_saddr ? (unsigned long)(pc - (uintptr_t)info.dli_saddr) : 0UL);
    } else {
        fprintf(st->f, "#%02d pc %p\n", st->n, (void *)pc);
    }
    return (++st->n >= 40) ? _URC_END_OF_STACK : _URC_NO_REASON;
}

static void crash_handler(int sig, siginfo_t *info, void *ctx) {
    (void)ctx;
    FILE *f = crash_path[0] ? fopen(crash_path, "w") : NULL;
    if (f) {
        fprintf(f, "Saturn Studio crashed.\nsignal %d, fault address %p\nlast status: %s\n\nbacktrace:\n",
                sig, info ? info->si_addr : NULL, last_status);
        struct bt_state st = { f, 0 };
        _Unwind_Backtrace(bt_cb, &st);
        fprintf(f, "\nSend this file (and stderr.log / latest.log) when reporting the problem.\n");
        fclose(f);
    }
    __android_log_print(ANDROID_LOG_FATAL, TAG, "crash: signal %d addr %p", sig, info ? info->si_addr : NULL);
    signal(sig, SIG_DFL);
    raise(sig);
}

void android_debug_start(void) {
    char path[4400];
    snprintf(path, sizeof(path), "%s/stderr.log", android_data_dir);
    freopen(path, "w", stderr);
    setvbuf(stderr, NULL, _IOLBF, 0);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    int sigs[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT };
    for (unsigned i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++) sigaction(sigs[i], &sa, NULL);
}

/* ---------- JNI: file picker + toast ---------- */

static pthread_mutex_t pick_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pick_cond = PTHREAD_COND_INITIALIZER;
static int pick_done = 0;
static char pick_result[4096];

JNIEXPORT void JNICALL Java_com_saturnstudio_SaturnActivity_nativeOnFilePicked(JNIEnv *env, jclass cls, jstring path) {
    (void)cls;
    pthread_mutex_lock(&pick_mutex);
    pick_result[0] = 0;
    if (path) {
        const char *c = (*env)->GetStringUTFChars(env, path, NULL);
        if (c) { snprintf(pick_result, sizeof(pick_result), "%s", c); (*env)->ReleaseStringUTFChars(env, path, c); }
    }
    pick_done = 1;
    pthread_cond_broadcast(&pick_cond);
    pthread_mutex_unlock(&pick_mutex);
}

int android_pick_file(const char *title, const char *mime, char *out, int out_len) {
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity) return 0;
    jclass cls = (*env)->GetObjectClass(env, activity);
    jmethodID mid = (*env)->GetStaticMethodID(env, cls, "pickFile", "(Ljava/lang/String;Ljava/lang/String;)V");
    if (!mid) { (*env)->ExceptionClear(env); return 0; }

    pthread_mutex_lock(&pick_mutex);
    pick_done = 0;
    pthread_mutex_unlock(&pick_mutex);

    jstring jt = (*env)->NewStringUTF(env, title ? title : "");
    jstring jm = (*env)->NewStringUTF(env, mime ? mime : "*/*");
    (*env)->CallStaticVoidMethod(env, cls, mid, jt, jm);
    (*env)->DeleteLocalRef(env, jt);
    (*env)->DeleteLocalRef(env, jm);
    (*env)->DeleteLocalRef(env, cls);
    (*env)->DeleteLocalRef(env, activity);

    pthread_mutex_lock(&pick_mutex);
    while (!pick_done) pthread_cond_wait(&pick_cond, &pick_mutex);
    snprintf(out, out_len, "%s", pick_result);
    pthread_mutex_unlock(&pick_mutex);
    return out[0] != 0;
}

void android_toast(const char *msg) {
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!env || !activity) return;
    jclass cls = (*env)->GetObjectClass(env, activity);
    jmethodID mid = (*env)->GetStaticMethodID(env, cls, "toast", "(Ljava/lang/String;)V");
    if (mid) {
        jstring js = (*env)->NewStringUTF(env, msg);
        (*env)->CallStaticVoidMethod(env, cls, mid, js);
        (*env)->DeleteLocalRef(env, js);
    } else (*env)->ExceptionClear(env);
    (*env)->DeleteLocalRef(env, cls);
    (*env)->DeleteLocalRef(env, activity);
}

#endif
