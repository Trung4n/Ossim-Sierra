/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "common.h"
#include "syscall.h"
#include "stdio.h"
#include "libmem.h"
#include "queue.h"
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int __sys_killall(struct pcb_t *caller, struct sc_regs* regs){
    char proc_name[100];
    uint32_t data;
    uint32_t memrg = regs->a1;
    int i = 0;
    /* Read target process name from user-space memory */
    do{
        if(libread(caller, memrg, i, &data) < 0){
            printf("Error reading from memory region %d\n", memrg);
            return -1;
        }
        proc_name[i] = (char) data;
        i++;
    }while(data != -1 && i < sizeof(proc_name) - 1);
    proc_name[i - 1] = '\0';  // Terminate proc name string

    printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);

    i = 0;
    /* === Traverse running list to kill procs with matching name === */
    while (i < caller->running_list->size) {
        struct pcb_t *target_proc = caller->running_list->proc[i];
        
        // Skip null proc or self
        if (caller->running_list->proc[i] == NULL || target_proc->pid == caller->pid) {
            i++;
            continue;
        }

        // Match name by checking if path contains proc_name
        if (strstr(target_proc->path, proc_name) != NULL) {
            pthread_mutex_lock(&lock);
#ifdef DEBUG
            printf("Terminating process PID: %d PRIO: %d\n", target_proc->pid, target_proc->prio);
#endif      
            // Mark process as terminated by setting PC to EOF
            peek_at_id(caller->running_list, target_proc->pid);
            target_proc->pc = target_proc->code->size;  // Force exit
            pthread_mutex_unlock(&lock);
            continue; // Stay at index i since current slot is shifted
        } 
        i++;
    }
    
#ifdef MLQ_SCHED
    /* === Traverse MLQ ready queues to kill procs with matching name === */
    for(int q = 0; q < MAX_PRIO; q++){
        i = 0;
        while(i < caller->mlq_ready_queue[q].size) {
            struct pcb_t *target_proc = caller->mlq_ready_queue[q].proc[i];
            
            if (target_proc == NULL || target_proc->pid == caller->pid) {
                i++;
                continue;
            }
            if (strstr(target_proc->path, proc_name) != NULL) {
                pthread_mutex_lock(&lock);
#ifdef DEBUG
                printf("Terminating process PID: %d PRIO: %d\n", target_proc->pid, target_proc->prio);
#endif      
                peek_at_index(&caller->mlq_ready_queue[q], i); // Dealloc PCB
                free(target_proc);
                pthread_mutex_unlock(&lock);
                continue;
            }
            i++;
        }
    }
#endif

    return 0; 
}
