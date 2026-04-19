//
// Created by Ramon on 3/06/2025.
//

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <uv.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include "file_watcher.h"

#ifndef _MSC_VER
#define _strdup strdup
#endif

#define MAX_WATCHERS 4096
#define COALESCE_INTERVAL_MS 200

uv_loop_t *loop;
uv_fs_event_t *watchers[MAX_WATCHERS];
int watcher_count = 0;

typedef struct PendingEvent {
    char path[PATH_MAX];
    struct PendingEvent *next;
} PendingEvent;

typedef struct {
    char *path;
} file_task_t;

PendingEvent *pending_head = NULL;
uv_timer_t flush_timer;

static void (*file_change_handler)(const char *path) = NULL;

void on_fs_event(const uv_fs_event_t *handle, const char *filename, int events, int status);

void close_cb(uv_handle_t *handle) {
    free(handle->data);
    free(handle);
}

int is_hidden(const char *name) {
    return name[0] == '.';
}

int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

int is_directory(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR;
}

int is_already_watched(const char *path) {
    for (int i = 0; i < watcher_count; ++i) {
        if (strcmp(path, watchers[i]->data) == 0) return 1;
    }
    return 0;
}

void add_watcher(const char *path) {
    if (watcher_count >= MAX_WATCHERS || is_already_watched(path)) return;

    uv_fs_event_t *fs_event = malloc(sizeof(uv_fs_event_t));
    fs_event->data = _strdup(path);

    if (uv_fs_event_init(loop, fs_event) < 0 ||
        uv_fs_event_start(fs_event, (uv_fs_event_cb) &on_fs_event, path, 0) < 0) {
        free(fs_event->data);
        free(fs_event);
        return;
        }

    watchers[watcher_count++] = fs_event;
    printf("Watching directory: %s\n", path);
}

void remove_watcher(const char *path) {
    int index = -1;
    for (int i = 0; i < watcher_count; ++i) {
        if (strcmp(path, watchers[i]->data) == 0) {
            index = i;
            break;
        }
    }

    if (index == -1) return;

    uv_fs_event_t *h = watchers[index];
    if (!uv_is_closing((uv_handle_t *) h)) {
        uv_close((uv_handle_t *) h, close_cb);
        printf("Stopped watching directory: %s\n", path);
    }

    for (int j = index; j < watcher_count - 1; ++j) {
        watchers[j] = watchers[j + 1];
    }
    watcher_count--;
}

void safe_strcpy(char *dest, size_t dest_size, const char *src) {
#if defined(_MSC_VER)
    strncpy_s(dest, dest_size, src, _TRUNCATE);
#else
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
#endif
}

void do_file_task(uv_work_t *req) {
    const file_task_t *task = req->data;
    if (file_change_handler) {
        file_change_handler(task->path);
    } else {
        fprintf(stderr, "File change handler not set for path: %s\n", task->path);
    }
}

void after_file_task(uv_work_t *req, int status) {
    file_task_t *task = req->data;
    free(task->path);
    free(task);
    free(req);
}

void add_pending_event(const char *full_path) {
    for (const PendingEvent *e = pending_head; e; e = e->next) {
        if (strcmp(e->path, full_path) == 0) return;
    }

    PendingEvent *e = malloc(sizeof(PendingEvent));
    safe_strcpy(e->path, sizeof(e->path), full_path);
    e->next = pending_head;
    pending_head = e;
}

void flush_pending_events(const uv_timer_t *handle) {
    PendingEvent *e = pending_head;
    while (e) {
        if (file_exists(e->path)) {
            uv_work_t *req = malloc(sizeof(uv_work_t));
            file_task_t *task = malloc(sizeof(file_task_t));

            task->path = _strdup(e->path);
            req->data = task;

            if (uv_queue_work(loop, req, &do_file_task, &after_file_task) != 0) {
                free(task->path);
                free(task);
                free(req);
            }
        }
        PendingEvent *next = e->next;
        free(e);
        e = next;
    }
    pending_head = NULL;
}

void on_fs_event(const uv_fs_event_t *handle, const char *filename, const int events, const int status) {
    if (status == UV_EPERM) {
        remove_watcher(handle->data);
        return;
    }

    if (status < 0) {
        fprintf(stderr, "FS error: %s\n", uv_strerror(status));
        remove_watcher(handle->data);
        return;
    }

    if (!filename || is_hidden(filename)) return;

    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", (char *) handle->data, filename);

    if (!is_directory(full_path)) {
        add_pending_event(full_path);
        uv_timer_start(&flush_timer, (uv_timer_cb) &flush_pending_events, COALESCE_INTERVAL_MS, 0);
    }

    if (events & UV_RENAME) {
        if (is_directory(full_path)) {
            if (!is_already_watched(full_path)) {
                add_watcher(full_path);
            }
        } else {
            // Might have been deleted
            if (!is_directory(full_path)) {
                remove_watcher(full_path);
            }
        }
    }
}

void scan_root_and_subdirs(const char *root) {
    add_watcher(root);

    uv_fs_t req;
    if (uv_fs_scandir(loop, &req, root, 0, NULL) < 0) {
        uv_fs_req_cleanup(&req);
        return;
    }

    uv_dirent_t dent;
    while (uv_fs_scandir_next(&req, &dent) != UV_EOF) {
        if (is_hidden(dent.name)) continue;
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", root, dent.name);
        if (dent.type == UV_DIRENT_DIR && is_directory(full_path)) {
            add_watcher(full_path);
        }
    }

    uv_fs_req_cleanup(&req);
}

void file_watcher_start(const char *root, void(*callback)(const char *path)) {
    file_change_handler = callback;
    loop = uv_default_loop();
    uv_timer_init(loop, &flush_timer);
    scan_root_and_subdirs(root);
    uv_run(loop, UV_RUN_DEFAULT);
}
