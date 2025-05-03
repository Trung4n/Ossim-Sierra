#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        // Enqueue a new PCB into ready queue [q]
        
        if (q != NULL && proc != NULL && q->size < MAX_QUEUE_SIZE){
                // Avoid duplicate enqueues based on PID
                for(int i = 0; i < q->size; i++){
                        if(proc->pid == q->proc[i]->pid){
                                // printf("------------ Enqueue duplicate process %d ------------\n", proc->pid);
                                return; // Dup detected => skip enqueue
                        }
                }
                // Insert proc at tail of queue
                q->proc[q->size] = proc;
                q->size++;
        }
}

struct pcb_t * dequeue(struct queue_t * q) {
        // Dequeue the PCB with highest priority (lowest prio value) from [q]

        struct pcb_t * process = NULL;
        if(q != NULL){
                int idx = 0;
                unsigned long prio = MAX_PRIO;
                // Find the proc with min prio (highest priority)
                for(int i = 0; i < q->size; i++){
                        if(process == NULL || prio > q->proc[i]->prio){
                                process = q->proc[i];
                                prio = q->proc[i]->prio;
                                idx = i;
                        }            
                }
                if (process != NULL){
                        peek_at_index(q, idx);
                }    
        }
	return process;
}

void peek_at_id(struct queue_t *q, uint32_t pid){
        // Remove PCB with matching PID from queue [q]

        if(q != NULL && !empty(q)){
                for(int i = 0; i < q->size; i++){
                        if(q->proc[i]->pid == pid){
                                // Found => remove at index i
                                peek_at_index(q, i);
                                return;   
                        }
                }
        }
}

void peek_at_index(struct queue_t *q, int index){
        // Remove PCB at given idx in queue [q]

        if(q != NULL && !empty(q)){
                // Shift procs left from idx
                for(int j = index; j < q->size - 1; j++){
                        q->proc[j] = q->proc[j + 1];
                }

                // Clear last slot & dec q size
                q->proc[q->size - 1] = NULL;
                q->size--;
        }
}
