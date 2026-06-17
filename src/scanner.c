#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include "scanner.h"
#include "worker.h"

static const char *const extensions[] = {".jpg", ".jpeg", ".png", ".pdf", ".mp3", ".wav", NULL};

static int has_supported_extension(const char *name) {
    for (const char *const *ext = extensions; *ext; ++ext) {
        size_t len = strlen(*ext);
        size_t nlen = strlen(name);
        if (nlen >= len && strcasecmp(name + nlen - len, *ext) == 0) {
            return 1;
        }
    }
    return 0;
}

static int enqueue_path(work_queue_t *queue, const char *path) {
    char *copy = strdup(path);
    if (!copy) return -1;
    if (work_queue_push(queue, copy) != 0) {
        free(copy);
        return -1;
    }
    return 0;
}

size_t scan_directory(const char *path, work_queue_t *queue) {
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "warn: cannot open directory '%s': %s\n", path, strerror(errno));
        return 0;
    }

    size_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char child[PATH_MAX];
        int ret = snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        if (ret < 0 || ret >= (int)sizeof(child)) continue;

        struct stat sb;
        // Fixed: Use lstat to ignore symlink processing loops completely
        if (lstat(child, &sb) != 0) continue;

        if (S_ISDIR(sb.st_mode)) {
            count += scan_directory(child, queue);
        } else if (S_ISREG(sb.st_mode) && has_supported_extension(entry->d_name)) {
            if (enqueue_path(queue, child) == 0) count++;
        }
    }

    closedir(dir);
    return count;
}