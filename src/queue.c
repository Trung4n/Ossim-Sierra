#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        /* TODO: put a new process to queue [q] */
        if (q->size >= MAX_QUEUE_SIZE) return;
        int i = q->size - 1;
        while (i >= 0 && q->proc[i]->prio > proc->prio) {
                q->proc[i + 1] = q->proc[i];
                i--;
        }
        q->proc[i + 1] = proc;
        q->size++;
}

struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
	if (empty(q)) return NULL;
        struct pcb_t *proc = q->proc[0];
        for (int i = 1; i < q->size; i++) {
                q->proc[i - 1] = q->proc[i];
        }
        q->size--;
        return proc;
}

