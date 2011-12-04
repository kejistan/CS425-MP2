#ifndef QUEUE_H
#define QUEUE_H

#include "mp2.h"

#define kQueueSize 20

typedef struct queue
{
	message_t *head;
	size_t size;
} queue_t;

queue_t *queue_init(void);
void queue_free(queue_t *queue);
int queue_empty(queue_t *queue);
void enqueue(queue_t *queue, message_t *message);
message_t *dequeue(queue_t *queue);

#endif // QUEUE_H
