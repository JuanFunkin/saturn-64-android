#ifndef ANDROID_SUPPORT_H
#define ANDROID_SUPPORT_H

// Android glue for Saturn Studio: data folder, status/progress file, crash report, JNI helpers.

#ifdef __cplusplus
extern "C" {
#endif

// Absolute path of the data folder (/storage/emulated/0/com.saturnstudio, or the private fallback). No trailing '/'.
extern char android_data_dir[4096];

void android_init_paths(void);                         // reads <internal>/datadir.txt, mkdir -p, chdir()
void android_debug_start(void);                        // stderr -> stderr.log, crash handler -> crash.txt
void android_set_status(float progress, const char *msg); // writes extract_status.txt (progress < 0 = error)
void android_breadcrumb(const char *msg);               // remembered for crash.txt only
void android_write_text_file(const char *name, const char *text); // relative to the data folder

// Opens the system file picker (blocks the calling thread until the user picks / cancels).
// The picked file is copied into <data>/imports/ and its path written to out. Returns 1 on success.
int  android_pick_file(const char *title, const char *mime, char *out, int out_len);
void android_toast(const char *msg);

#ifdef __cplusplus
}
#endif
#endif
