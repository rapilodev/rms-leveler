#ifndef FILE_LOG_H
#define FILE_LOG_H

#include <stdio.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define LOG_BUF_SIZE 4096

typedef struct {
    char buffer[LOG_BUF_SIZE];
    atomic_size_t head;
    atomic_size_t tail;
    bool can_log;
    pthread_t thread;
    atomic_bool run_thread;
    char log_dir[256];
    char log_id[64];
} FileLogger;

// Worker must be static to avoid linker conflicts
static void* logger_background_worker(void* arg) {
    FileLogger* l = (FileLogger*)arg;
    while (atomic_load(&l->run_thread)) {
        size_t h = atomic_load(&l->head);
        size_t t = atomic_load(&l->tail);

        if (h != t) {
            time_t now = time(NULL);
            struct tm lt;
            localtime_r(&now, &lt);

            char date_str[11];
            char time_str[20];
            strftime(date_str, sizeof(date_str), "%Y-%m-%d", &lt);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &lt);

            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s-monitor-%s.log",
                     l->log_dir, date_str, l->log_id);

            FILE* f = fopen(full_path, "a");
            if (f) {
                fprintf(f, "%s\t", time_str);
                while (t != h) {
                    char c = l->buffer[t];
                    fputc(c, f);
                    t = (t + 1) % LOG_BUF_SIZE;
                    if (c == '\n') break;
                }
                atomic_store(&l->tail, t);
                fclose(f);
            } else {
                atomic_store(&l->tail, h);
            }
        }
        usleep(200000);
    }
    return NULL;
}

// Added static inline to prevent "implicit declaration" and "multiple definition"
static inline void file_log(FileLogger* l, double left, double right) {
    if (!l || !l->can_log) return;

    char msg[64];
    int len = snprintf(msg, sizeof(msg), "%2.3f\t%2.3f\n", left, right);
    if (len < 0) return;

    for (int i = 0; i < len; i++) {
        size_t h = atomic_load_explicit(&l->head, memory_order_relaxed);
        size_t next = (h + 1) % LOG_BUF_SIZE;
        if (next != atomic_load_explicit(&l->tail, memory_order_acquire)) {
            l->buffer[h] = msg[i];
            atomic_store_explicit(&l->head, next, memory_order_release);
        }
    }
}

static inline void file_logger_init(FileLogger* l, const char* dir, const char* id) {
    if (!l) return;
    struct stat info;

    // Fixed strncpy warnings by ensuring buffer-1 length and manual null termination
    l->log_dir[sizeof(l->log_dir) - 1] = '\0';
    strncpy(l->log_dir, dir, sizeof(l->log_dir) - 1);

    l->log_id[sizeof(l->log_id) - 1] = '\0';
    strncpy(l->log_id, id, sizeof(l->log_id) - 1);

    if (stat(dir, &info) == 0 && S_ISDIR(info.st_mode) && access(dir, W_OK) == 0) {
        l->can_log = true;
        l->head = 0;
        l->tail = 0;
        atomic_store(&l->run_thread, true);
        pthread_create(&l->thread, NULL, logger_background_worker, l);
    } else {
        l->can_log = false;
        fprintf(stderr, "FileLogger: %s inaccessible. Logging disabled.\n", dir);
    }
}

static inline void file_logger_cleanup(FileLogger* l) {
    if (l && l->can_log) {
        atomic_store(&l->run_thread, false);
        pthread_join(l->thread, NULL);
        l->can_log = false;
    }
}

void print_log(const char* LOG_ID, double l, double r) {
    time_t now;
    time(&now);
    struct tm *localTime = localtime(&now);
    char formattedTime[20];
    strftime(formattedTime, sizeof(formattedTime), "%Y-%m-%d %H:%M:%S",
            localTime);
    fprintf(stderr, "%s %s\t%2.3f\t%2.3f\n", formattedTime, LOG_ID, l, r);
}

#endif
