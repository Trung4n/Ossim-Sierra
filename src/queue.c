#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        /* TODO: put a new process to queue [q] */
        if (q->size >= MAX_QUEUE_SIZE || q == NULL) return;
        q->proc[q->size] = proc;
        q->size++;
}

struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
	int minPrioId = 0;
        for (int i=1; i<q->size; i++) {
                if (q->proc[i]->prio < q->proc[minPrioId]->prio) minPrioId = i;
        }
        //! Lưu process cần trả về
        struct pcb_t *minprioiproc = q->proc[minPrioId];
        //! Dịch chuyển các process phía sau lên 
        for (int i = minPrioId; i<q->size-1; i++) {
                q->proc[i] = q->proc[i+1];
        }
        q->size--;
        return minprioiproc;
}

