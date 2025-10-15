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
#include "string.h"
#include <stdlib.h>


int __sys_killall(struct pcb_t *caller, struct sc_regs* regs)
{
    char proc_name[100];
    uint32_t data;
    uint32_t memrg = regs->a1;

    int i = 0;
    data = 0;
    while (data != -1 && i < 99) { // Add limit
        libread(caller, memrg, i, &data);
        proc_name[i] = (char)data;
        if (data == -1 || data == 0) {
            proc_name[i] = '\0';
            break;
        }
        i++;
    }
    proc_name[i] = '\0';
    printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);

    for (int lvl = 0; lvl < MAX_PRIO; lvl++) {
        struct queue_t *queue = &caller->mlq_ready_queue[lvl]; 
        if (!queue || empty(queue)) continue;

        int idx = 0;
        while (idx < queue->size) {
            struct pcb_t *proc = queue->proc[idx];
            if (proc && strcmp(proc->path, proc_name) == 0) {
                printf("Terminated process PID %d with name \"%s\"\n", proc->pid, proc->path);

                free(proc->code);
#ifdef MM_PAGING
                if (proc->mm) free(proc->mm);
#endif
                for (int k = idx; k < queue->size - 1; k++) {
                    queue->proc[k] = queue->proc[k + 1];
                }
                queue->proc[queue->size - 1] = NULL;
                queue->size--;
            } else {
                idx++;
            }
        }
    }

    struct queue_t *run_queue = caller->running_list;
    if (run_queue && !empty(run_queue)) {
        int idx = 0;
        while (idx < run_queue->size) {
            struct pcb_t *proc = run_queue->proc[idx];
            if (proc && strcmp(proc->path, proc_name) == 0) {
                printf("Terminated running process PID %d with name \"%s\"\n", proc->pid, proc->path);

                free(proc->code);
#ifdef MM_PAGING
                if (proc->mm) free(proc->mm);
#endif
                for (int k = idx; k < run_queue->size - 1; k++) {
                    run_queue->proc[k] = run_queue->proc[k + 1];
                }
                run_queue->proc[run_queue->size - 1] = NULL;
                run_queue->size--;
            } else {
                idx++;
            }
        }
    }

    return 0; 
}

