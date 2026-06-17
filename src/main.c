#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "scanner.h"
#include "worker.h"

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s <directory>\n"
            "Recursively scrub metadata from images, documents, and audio files.\n",
            prog);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *root = argv[1];
    struct stat sb;
    if (stat(root, &sb) != 0) {
        fprintf(stderr, "error: cannot access '%s': %s\n", root, strerror(errno));
        return EXIT_FAILURE;
    }

    if (!S_ISDIR(sb.st_mode)) {
        fprintf(stderr, "error: '%s' is not a directory\n", root);
        return EXIT_FAILURE;
    }

    work_queue_t queue;
    if (work_queue_init(&queue) != 0) {
        fprintf(stderr, "error: failed to initialize queue\n");
        return EXIT_FAILURE;
    }

    int thread_count = 4;
#ifdef _SC_NPROCESSORS_ONLN
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);
    if (nproc > 0) thread_count = (int)nproc;
#endif
    if (thread_count < 1) thread_count = 4;

    worker_pool_t pool;
    if (worker_pool_start(&pool, &queue, thread_count) != 0) {
        fprintf(stderr, "error: failed to start workers\n");
        work_queue_destroy(&queue);
        return EXIT_FAILURE;
    }

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    size_t queued = scan_directory(root, &queue);
    printf("[INFO] queued %zu supported files for processing\n", queued);

    work_queue_finish(&queue);
    worker_pool_stop(&pool);
    work_queue_destroy(&queue);

    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("[SUMMARY] Files queued: %zu, processed: %u, errors: %u, threads: %d, elapsed: %.3f sec\n",
           queued, pool.ctx.processed, pool.ctx.errors, thread_count, elapsed);

    return EXIT_SUCCESS;
}
