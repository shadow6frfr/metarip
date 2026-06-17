#ifndef SCANNER_H
#define SCANNER_H

#include "worker.h"

size_t scan_directory(const char *path, work_queue_t *queue);

#endif
