#include <stdio.h> 
#include "cpu.h"
#include "mem.h"  
#include "mm.h"    
#include "syscall.h"
#include "libmem.h"


int calc(struct pcb_t *proc);
#ifndef MM_PAGING 
int alloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index);
int free_data(struct pcb_t *proc, uint32_t reg_index);
int read(struct pcb_t *proc, uint32_t source, uint32_t offset, uint32_t destination);
int write(struct pcb_t *proc, BYTE data, uint32_t destination, uint32_t offset);
#endif




int calc(struct pcb_t *proc)
{
    return ((unsigned long)proc & 0UL);
}



#ifndef MM_PAGING
int alloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
    addr_t addr = alloc_mem(size, proc); 
    if (addr == 0) { return 1; }
    else { proc->regs[reg_index] = addr; return 0; }
}

int free_data(struct pcb_t *proc, uint32_t reg_index)
{
    return free_mem(proc->regs[reg_index], proc); 
}

int read(struct pcb_t *proc, uint32_t source, uint32_t offset, uint32_t destination)
{
    BYTE data;
    if (read_mem(proc->regs[source] + offset, proc, &data) == 0) {
        proc->regs[destination] = data; return 0;
    } else { return 1; }
}

int write(struct pcb_t *proc, BYTE data, uint32_t destination, uint32_t offset)
{
    return write_mem(proc->regs[destination] + offset, proc, data);
}
#endif 


int run(struct pcb_t *proc)
{
    if (proc == NULL || proc->code == NULL || proc->pc >= proc->code->size)
    {
        fprintf(stderr, "run: Invalid process state or PC out of bounds (PID: %d, PC: %d, Size: %d)\n",
                proc ? proc->pid : -1, proc ? proc->pc : -1, proc && proc->code ? proc->code->size : -1);
        return 1; 
    }

    struct inst_t ins = proc->code->text[proc->pc]; 
    proc->pc++; 
    int stat = 1; 

#ifdef MM_PAGING
    switch (ins.opcode)
    {
        case CALC:
            stat = calc(proc); 
            break;

        case ALLOC:
            printf("===== PHYSICAL MEMORY AFTER ALLOCATION =====\n");
        
            stat = liballoc(proc, ins.arg_0, ins.arg_1);
            if (stat == 0) {
                addr_t allocated_addr = 0; 
                if (proc->mm && ins.arg_1 < PAGING_MAX_SYMTBL_SZ && proc->mm->symrgtbl[ins.arg_1].rg_end > proc->mm->symrgtbl[ins.arg_1].rg_start) {
                     allocated_addr = proc->mm->symrgtbl[ins.arg_1].rg_start;
                } else {
                    fprintf(stderr, "Warning: Could not retrieve valid address for allocated region %d PID %d after alloc\n", ins.arg_1, proc->pid);
                }
                printf("PID=%d - Region=%d - Address=%08lx - Size=%d byte\n",
                       proc->pid, ins.arg_1, (unsigned long)allocated_addr, ins.arg_0);
                #ifdef PAGETBL_DUMP
                print_pgtbl(proc, 0, -1); 
                #else
              
                printf("================================================================\n");
                #endif
            } else {
               
                 printf("ALLOCATION FAILED for PID=%d Region=%d Size=%d\n", proc->pid, ins.arg_1, ins.arg_0);
                 printf("================================================================\n");
            }
            break; 

        case FREE:
            printf("===== PHYSICAL MEMORY AFTER DEALLOCATION =====\n");

            printf("PID=%d - Region=%d\n", proc->pid, ins.arg_0);

            stat = libfree(proc, ins.arg_0);

            #ifdef PAGETBL_DUMP
            print_pgtbl(proc, 0, -1);
            #else

            printf("================================================================\n");
            #endif
            break; 

        case READ:

            printf("===== PHYSICAL MEMORY AFTER READING =====\n");

            stat = libread(proc, ins.arg_0, ins.arg_1, &ins.arg_2);

            break; 

        case WRITE:
            printf("===== PHYSICAL MEMORY AFTER WRITING =====\n");
            stat = libwrite(proc, (BYTE)ins.arg_0, ins.arg_1, ins.arg_2); 
            break; 

        case SYSCALL:
            stat = libsyscall(proc, ins.arg_0, ins.arg_1, ins.arg_2, ins.arg_3);
            break; 

        default:
    
            fprintf(stderr, "Error: Unknown opcode %d encountered by PID %d at PC %d\n", ins.opcode, proc->pid, proc->pc -1);
            stat = 1; 

    }

#else 
   
    switch (ins.opcode)
    {
        case CALC: stat = calc(proc); break;
        case ALLOC: stat = alloc(proc, ins.arg_0, ins.arg_1); break;
        case FREE: stat = free_data(proc, ins.arg_0); break;
        case READ: stat = read(proc, ins.arg_0, ins.arg_1, ins.arg_2); break;
        case WRITE: stat = write(proc, (BYTE)ins.arg_0, ins.arg_1, ins.arg_2); break;
        case SYSCALL: stat = libsyscall(proc, ins.arg_0, ins.arg_1, ins.arg_2, ins.arg_3); break;
        default:
            fprintf(stderr, "Error: Unknown opcode %d encountered by PID %d at PC %d\n", ins.opcode, proc->pid, proc->pc -1);
            stat = 1;
    }
#endif 

    return stat; 
}
