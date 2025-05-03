
#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"

#define MAX_QUEUE_SIZE 10

struct queue_t {
	struct pcb_t * proc[MAX_QUEUE_SIZE];
	int size;
};

void enqueue(struct queue_t * q, struct pcb_t * proc);

struct pcb_t * dequeue(struct queue_t * q);

void peek_at_id(struct queue_t *q, uint32_t pid);

void peek_at_index(struct queue_t *q, int index);

int empty(struct queue_t * q);

#endif

