#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if(q==NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        if(q->size>=MAX_QUEUE_SIZE) return;
        q->proc[q->size]=proc;
        q->size++;
}

struct pcb_t * dequeue(struct queue_t * q) {
        if (empty(q)) return NULL;
        //! Tìm proc cos prio min
        int minprioidx=0;
        for(int i=1;i<q->size;i++){
                if(q->proc[i]->prio<q->proc[minprioidx]->prio) minprioidx=i;
        }
        //! Lưu process cần trả về
        struct pcb_t *minprioiproc=q->proc[minprioidx];
        //! Dịch chuyển các process phía sau lên 
        for(int i=minprioidx;i<q->size-1;i++) {
                q->proc[i]=q->proc[i+1];
        }
        q->size--;
        return minprioiproc;
}

