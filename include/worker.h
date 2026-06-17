#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>

typedef struct work_node {
    void *item;
    struct work_node *next;
} work_node_t;

typedef struct {
    work_node_t *head;
    work_node_t *tail;
    unsigned int count;
    int done;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
} work_queue_t;

typedef struct {
    work_queue_t *queue;
    unsigned int processed;
    unsigned int errors;
    pthread_mutex_t stats_lock;
} worker_context_t;

typedef struct {
    pthread_t *threads;
    int count;
    worker_context_t ctx;
} worker_pool_t;

int work_queue_init(work_queue_t *queue);
int work_queue_push(work_queue_t *queue, void *item);
void *work_queue_pop(work_queue_t *queue);
void work_queue_finish(work_queue_t *queue);
void work_queue_destroy(work_queue_t *queue);
int worker_pool_start(worker_pool_t *pool, work_queue_t *queue, int count);
void worker_pool_stop(worker_pool_t *pool);

#endif
