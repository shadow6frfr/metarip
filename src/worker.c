#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include "scanner.h"
#include "worker.h"

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define fsync_safe(fd) _commit(fd)
#define O_BINARY_MODE O_BINARY
#else
#define fsync_safe(fd) fsync(fd)
#define O_BINARY_MODE 0
#endif

static uint32_t crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320u : 0);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

static int read_full(int fd, uint8_t *buf, size_t len) {
    if (len == 0) return 0;
    size_t remaining = len;
    uint8_t *ptr = buf;
    while (remaining > 0) {
        ssize_t nread = read(fd, ptr, remaining);
        if (nread < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (nread == 0) return -1; // Unexpected EOF
        remaining -= (size_t)nread;
        ptr += nread;
    }
    return 0;
}

static int write_full(int fd, const uint8_t *buf, size_t len) {
    size_t remaining = len;
    const uint8_t *ptr = buf;
    while (remaining > 0) {
        ssize_t written = write(fd, ptr, remaining);
        if (written < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (written == 0) return -1;
        remaining -= (size_t)written;
        ptr += written;
    }
    return 0;
}

static int strip_jpeg(uint8_t *data, size_t size) {
    size_t pos = 2;
    if (size < 4 || data[0] != 0xFF || data[1] != 0xD8) return -1;
    while (pos + 4 <= size) {
        if (data[pos] != 0xFF) return -1;
        uint8_t marker = data[pos + 1];
        if (marker == 0xDA) break;
        if ((marker >= 0xD0 && marker <= 0xD9) || marker == 0x01) {
            pos += 2;
            continue;
        }
        uint16_t length = (data[pos + 2] << 8) | data[pos + 3];
        if (length < 2) return -1;
        if (size - pos < 2 || length > size - pos - 2) return -1; // Fixed structural underflow
        if (marker == 0xE1 || marker == 0xED) {
            memset(data + pos + 4, 0, length - 2);
        }
        pos += 2 + length;
    }
    return 0;
}

static int strip_png(uint8_t *data, size_t size) {
    const uint8_t pngsig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (size < 8 || memcmp(data, pngsig, 8) != 0) return -1;
    size_t pos = 8;
    while (pos + 12 <= size) {
        uint32_t chunk_len = (data[pos] << 24) | (data[pos + 1] << 16) |
                             (data[pos + 2] << 8) | data[pos + 3];
        if (chunk_len > size - pos - 12) return -1; // Fixed calculation safety
        size_t next_pos = pos + 12 + chunk_len;
        if (next_pos < pos || next_pos > size) return -1;
        const char type[5] = {data[pos + 4], data[pos + 5], data[pos + 6], data[pos + 7], '\0'};
        if (strcmp(type, "tEXt") == 0 || strcmp(type, "iTXt") == 0 || 
            strcmp(type, "zTXt") == 0 || strcmp(type, "iCCP") == 0) {
            memset(data + pos + 8, 0, chunk_len);
            uint32_t crc = crc32(data + pos + 4, 4 + chunk_len);
            data[pos + 8 + chunk_len] = (crc >> 24) & 0xFF;
            data[pos + 9 + chunk_len] = (crc >> 16) & 0xFF;
            data[pos + 10 + chunk_len] = (crc >> 8) & 0xFF;
            data[pos + 11 + chunk_len] = crc & 0xFF;
        }
        pos = next_pos;
    }
    return 0;
}

static void redact_pdf_key(uint8_t *data, size_t size, const char *key) {
    size_t keylen = strlen(key);
    const char *replacement = "/Null";
    for (size_t pos = 0; pos + keylen <= size; ++pos) {
        if (memcmp(data + pos, key, keylen) != 0) continue;
        if (pos > 0 && !isspace((unsigned char)data[pos - 1]) && data[pos - 1] != '/') continue;
        memcpy(data + pos, replacement, 5);
        if (keylen > 5) memset(data + pos + 5, ' ', keylen - 5);
    }
}

static int strip_pdf(uint8_t *data, size_t size) {
    if (size < 5 || memcmp(data, "%PDF-", 5) != 0) return -1;
    redact_pdf_key(data, size, "/Info");
    redact_pdf_key(data, size, "/Metadata");
    return 0;
}

static int strip_mp3(uint8_t *data, size_t size) {
    if (size < 10 || memcmp(data, "ID3", 3) != 0) return 0;
    uint32_t tag_size = (data[6] & 0x7F) << 21 | (data[7] & 0x7F) << 14 | 
                        (data[8] & 0x7F) << 7 | (data[9] & 0x7F);
    if (tag_size > size - 10) return -1;
    memset(data, 0, 10 + tag_size);
    return 0;
}

static int strip_wav(uint8_t *data, size_t size) {
    if (size < 12 || memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0) return -1;
    size_t pos = 12;
    while (pos + 8 <= size) {
        uint32_t chunk_len = (data[pos + 4]) | (data[pos + 5] << 8) | 
                             (data[pos + 6] << 16) | (data[pos + 7] << 24);
        if (chunk_len > size - pos - 8) return -1;
        size_t padding = (chunk_len & 1) ? 1 : 0;
        size_t next_pos = pos + 8 + chunk_len + padding;
        if (next_pos < pos || next_pos > size) return -1;
        if (memcmp(data + pos, "LIST", 4) == 0) {
            memset(data + pos, ' ', 8);
            size_t fill = (chunk_len < size - pos - 8) ? chunk_len : size - pos - 8;
            memset(data + pos + 8, ' ', fill);
        }
        pos = next_pos;
    }
    return 0;
}

static int process_buffer(const char *path, uint8_t *data, size_t size) {
    const char *ext = strrchr(path, '.');
    if (!ext) return -1;
    if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) return strip_jpeg(data, size);
    if (strcasecmp(ext, ".png") == 0) return strip_png(data, size);
    if (strcasecmp(ext, ".pdf") == 0) return strip_pdf(data, size);
    if (strcasecmp(ext, ".mp3") == 0) return strip_mp3(data, size);
    if (strcasecmp(ext, ".wav") == 0) return strip_wav(data, size);
    return -1;
}

static int atomic_write_file(const char *path, const uint8_t *data, size_t size) {
    char tmp_path[PATH_MAX + 8];
    if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path) >= (int)sizeof(tmp_path)) {
        return -1;
    }

    // Fixed: Open temporary target file instead of truncating in-place
    int fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY_MODE, 0666);
    if (fd < 0) {
        fprintf(stderr, "[ERROR] cannot open temporary file '%s': %s\n", tmp_path, strerror(errno));
        return -1;
    }

    if (write_full(fd, data, size) != 0) {
        fprintf(stderr, "[ERROR] cannot write temporary file '%s': %s\n", tmp_path, strerror(errno));
        close(fd);
        unlink(tmp_path);
        return -1;
    }

    if (fsync_safe(fd) != 0) {
        fprintf(stderr, "[ERROR] fsync failed for '%s': %s\n", tmp_path, strerror(errno));
        close(fd);
        unlink(tmp_path);
        return -1;
    }
    close(fd);

#ifdef _WIN32
    if (MoveFileExA(tmp_path, path, MOVEFILE_REPLACE_EXISTING) == 0) {
        fprintf(stderr, "[ERROR] failed to atomically replace file '%s'\n", path);
        unlink(tmp_path);
        return -1;
    }
#else
    if (rename(tmp_path, path) != 0) {
        fprintf(stderr, "[ERROR] failed to rename '%s' to '%s': %s\n", tmp_path, path, strerror(errno));
        unlink(tmp_path);
        return -1;
    }
#endif
    return 0;
}

static void *worker_thread(void *arg) {
    worker_context_t *ctx = arg;
    while (1) {
        char *path = work_queue_pop(ctx->queue);
        if (!path) break;

        int fd = open(path, O_RDONLY | O_BINARY_MODE);
        if (fd < 0) {
            fprintf(stderr, "[ERROR] cannot open '%s': %s\n", path, strerror(errno));
            pthread_mutex_lock(&ctx->stats_lock);
            ctx->errors++;
            pthread_mutex_unlock(&ctx->stats_lock);
            free(path);
            continue;
        }

        struct stat sb;
        if (fstat(fd, &sb) != 0 || sb.st_size == 0) {
            close(fd);
            pthread_mutex_lock(&ctx->stats_lock);
            if (sb.st_size == 0) ctx->processed++;
            else ctx->errors++;
            pthread_mutex_unlock(&ctx->stats_lock);
            free(path);
            continue;
        }

        if (sb.st_size > 1073741824) {
            fprintf(stderr, "[WARN] skipping oversized file '%s' (%ld bytes)\n", path, sb.st_size);
            close(fd);
            pthread_mutex_lock(&ctx->stats_lock);
            ctx->processed++;
            pthread_mutex_unlock(&ctx->stats_lock);
            free(path);
            continue;
        }

        uint8_t *data = malloc(sb.st_size);
        if (!data) {
            fprintf(stderr, "[ERROR] malloc failed for '%s': %s\n", path, strerror(errno));
            close(fd);
            pthread_mutex_lock(&ctx->stats_lock);
            ctx->errors++;
            pthread_mutex_unlock(&ctx->stats_lock);
            free(path);
            continue;
        }

        int failed = 0;
        if (read_full(fd, data, sb.st_size) != 0) {
            fprintf(stderr, "[ERROR] read failed for '%s': %s\n", path, strerror(errno));
            failed = 1;
        } else {
            int rc = process_buffer(path, data, sb.st_size);
            if (rc != 0) {
                fprintf(stderr, "[ERROR] failed to scrub '%s'\n", path);
                failed = 1;
            } else if (atomic_write_file(path, data, sb.st_size) != 0) {
                failed = 1;
            }
        }

        close(fd);
        free(data);
        pthread_mutex_lock(&ctx->stats_lock);
        ctx->processed++;
        if (failed) ctx->errors++;
        pthread_mutex_unlock(&ctx->stats_lock);
        free(path);
    }
    return NULL;
}

int worker_pool_start(worker_pool_t *pool, work_queue_t *queue, int count) {
    pool->threads = calloc(count, sizeof(pthread_t));
    if (!pool->threads) return -1;
    pool->count = count;
    pool->ctx.queue = queue;
    pool->ctx.processed = 0;
    pool->ctx.errors = 0;
    if (pthread_mutex_init(&pool->ctx.stats_lock, NULL) != 0) {
        free(pool->threads);
        return -1;
    }

    for (int i = 0; i < count; ++i) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, &pool->ctx) != 0) {
            // Fixed: Wake threads and terminate before trying to join them to avoid permanent deadlock
            work_queue_finish(queue);
            for (int j = 0; j < i; ++j) {
                pthread_join(pool->threads[j], NULL);
            }
            free(pool->threads);
            pthread_mutex_destroy(&pool->ctx.stats_lock);
            return -1;
        }
    }
    return 0;
}

void worker_pool_stop(worker_pool_t *pool) {
    for (int i = 0; i < pool->count; ++i) {
        pthread_join(pool->threads[i], NULL);
    }
    free(pool->threads);
    pthread_mutex_destroy(&pool->ctx.stats_lock);
}