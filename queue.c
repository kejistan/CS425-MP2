#include <assert.h>
#include <stdlib.h>

#include "queue.h"

queue_t *queue_init(void)
{
	return calloc(sizeof(queue_t), 1);
}

void queue_free(queue_t *queue)
{
	message_t *current = dequeue(queue);;
	while (current) {
		message_free(current);
		current = dequeue(queue);
	}

	assert(queue->size == 0);

	free(queue);
}

int queue_empty(queue_t *queue)
{
	return !queue->head;
}

void enqueue(queue_t *queue, message_t *message)
{
	message_t *current = queue->head;

	assert(!message->next);

	++queue->size;

	if (!current) {
		queue->head = message;
		return;
	}

	while (current->next) {
		current = current->next;
	}

	current->next = message;
}

message_t *dequeue(queue_t *queue)
{
	message_t *current = queue->head;
	if (current) {
		queue->head = current->next;
		--queue->size;
	}

	current->next = NULL;
	return current;
}
