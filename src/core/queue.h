#ifndef QUEUE_H_
#define QUEUE_H_

#include "core/sge.h"


typedef struct sge_queue sge_queue;

sge_queue* create_queue(size_t size);
void destroy_queue(sge_queue* queue);
int enqueue(sge_queue* q, void* data);
int dequeue(sge_queue* q, void** ud);

#endif
