#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "scanner.h"
#include "worker.h"

int work_queue_init(work_queue_t *queue) {
    queue->head = queue->tail = NULL;
    queue->count = 0;
    queue->done = 0;
    if (pthread_mutex_init(&queue->lock, NULL) != 0) return -1;
    if (pthread_cond_init(&queue->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&queue->lock);
        return -1;
    }
    return 0;
}

int work_queue_push(work_queue_t *queue, void *item) {
    work_node_t *node = malloc(sizeof(*node));
    if (!node) return -1;
    node->item = item;
    node->next = NULL;

    pthread_mutex_lock(&queue->lock);
    if (queue->tail) queue->tail->next = node;
    else queue->head = node;
    queue->tail = node;
    queue->count++;
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->lock);
    return 0;
}

void *work_queue_pop(work_queue_t *queue) {
    pthread_mutex_lock(&queue->lock);
    while (!queue->done && !queue->head) {
        pthread_cond_wait(&queue->not_empty, &queue->lock);
    }
    if (!queue->head) {
        pthread_mutex_unlock(&queue->lock);
        return NULL;
    }

    work_node_t *node = queue->head;
    queue->head = node->next;
    if (!queue->head) queue->tail = NULL;
    queue->count--;
    pthread_mutex_unlock(&queue->lock);

    void *item = node->item;
    free(node);
    return item;
}

void work_queue_finish(work_queue_t *queue) {
    pthread_mutex_lock(&queue->lock);
    queue->done = 1;
    pthread_cond_broadcast(&queue->not_empty);
    pthread_mutex_unlock(&queue->lock);
}

void work_queue_destroy(work_queue_t *queue) {
    while (queue->head) {
        work_node_t *node = queue->head;
        queue->head = node->next;
        free(node->item);
        free(node);
    }
    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->not_empty);
}
