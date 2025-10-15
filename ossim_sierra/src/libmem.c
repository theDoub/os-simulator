/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c 
 */

 #include "string.h"
 #include "mm.h"
 #include "syscall.h"
 #include "libmem.h"
 #include <stdlib.h>
 #include <stdio.h>
 #include <pthread.h>
 
 static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;
 
 /*enlist_vm_freerg_list - add new rg to freerg_list
  *@mm: memory region
  *@rg_elmt: new region
  *
  */
 
 int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
 {
   struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;
 
   if (rg_elmt->rg_start >= rg_elmt->rg_end)
     return -1;
 
   if (rg_node != NULL)
     rg_elmt->rg_next = rg_node;
 
   /* Enlist the new region */
   mm->mmap->vm_freerg_list = rg_elmt;
 
   return 0;
 }
 
 /*get_symrg_byid - get mem region by region ID
  *@mm: memory region
  *@rgid: region ID act as symbol index of variable
  *
  */
 struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
 {
   if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
     return NULL;
 
   return &mm->symrgtbl[rgid];
 }
 

/*__alloc - allocate a region memory
 *@caller: caller process
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: pointer to store the address of allocated memory region
 * Returns 0 on success, -1 on failure.
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
    struct vm_rg_struct rgnode; 
    int ret = -1; 

    if (caller == NULL || caller->mm == NULL || alloc_addr == NULL || size <= 0 ||
        rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) {
        fprintf(stderr, "__alloc: Invalid parameters.\n");
        return -1;
    }

    pthread_mutex_lock(&mmvm_lock); 
    if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
    {
        caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
        caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end; 

        *alloc_addr = rgnode.rg_start;
        ret = 0; 
        
    }
    else
    {
        int inc_sz = PAGING_PAGE_ALIGNSZ(size);
        struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

        if (cur_vma != NULL) {
            int old_sbrk = cur_vma->sbrk; 

            struct sc_regs regs;
            regs.a1 = SYSMEM_INC_OP; 
            regs.a2 = vmaid;        
            regs.a3 = inc_sz;       
            int syscall_ret = libsyscall(caller, 17, regs.a1, regs.a2, regs.a3);

            if (syscall_ret == 0) {
                caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
                caller->mm->symrgtbl[rgid].rg_end = old_sbrk + inc_sz;

                *alloc_addr = old_sbrk; 
                ret = 0; 
            } else {
            }
        } else {
        }
    }

    pthread_mutex_unlock(&mmvm_lock); 
    return ret; 
}



 
 
 /*__free - remove a region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@rgid: memory region ID (used to identify variable in symbole table)
  *@size: allocated size
  *
  */
 int __free(struct pcb_t *caller, int vmaid, int rgid) {
   // Kiểm tra hợp lệ của rgid
   if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) {
     return -1;  // rgid không hợp lệ
   }
 
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
   if (!cur_vma) {
     return -1;  
   }
 
   struct vm_rg_struct *rg_elmt = &caller->mm->symrgtbl[rgid];  
   if (!rg_elmt) {
     return -1;  
   }
 
   struct vm_rg_struct *rg_free = malloc(sizeof(struct vm_rg_struct));
   rg_free->rg_start = rg_elmt->rg_start;
   rg_free->rg_end = rg_elmt->rg_end;
   rg_free->rg_next = cur_vma->vm_freerg_list;
   cur_vma->vm_freerg_list = rg_free;
 
   rg_elmt->rg_start = 0;
   rg_elmt->rg_end = 0;
   
   int start_pgn = rg_elmt->rg_start / PAGING_PAGESZ;
   int end_pgn = rg_elmt->rg_end / PAGING_PAGESZ;
   for (int pgn = start_pgn; pgn < end_pgn; pgn++) {
       pte_set_swap(&caller->mm->pgd[pgn], 0, 0); 
   }
   return 0;  
 }
 
 /*liballoc - PAGING-based allocate a region memory
  *@proc:  Process executing the instruction
  *@size: allocated size
  *@reg_index: memory region ID (used to identify variable in symbole table)
  */
 int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
 {
   int addr;
   /* By default using vmaid = 0 */
   return __alloc(proc, 0, reg_index, size, &addr);
 }
 
 /*libfree - PAGING-based free a region memory
  *@proc: Process executing the instruction
  *@size: allocated size
  *@reg_index: memory region ID (used to identify variable in symbole table)
  */
 
 int libfree(struct pcb_t *proc, uint32_t reg_index)
 {
   /* By default using vmaid = 0 */
   return __free(proc, 0, reg_index);
 }
 
 /*pg_getpage - get the page in ram
  *@mm: memory region
  *@pagenum: PGN
  *@framenum: return FPN
  *@caller: caller
  *
  */
 

int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller) {
  uint32_t pte = mm->pgd[pgn];

  if (!PAGING_PAGE_PRESENT(pte)) { // Page fault!
      int vicpgn = -1;
      int swpfpn = -1;
      int find_victim_ret;
      int get_free_ret;

      find_victim_ret = find_victim_page(caller->mm, &vicpgn);

      if (find_victim_ret != 0 || vicpgn < 0) {
          return -1;
      }

      get_free_ret = MEMPHY_get_freefp(caller->active_mswp, &swpfpn);

      if (get_free_ret != 0 || swpfpn < 0) {
          return -1;
      }

      int vicfpn = PAGING_FPN(caller->mm->pgd[vicpgn]);


      int tgtfpn = PAGING_PTE_SWP(pte);

      struct sc_regs regs;

      regs.a1 = SYSMEM_SWP_OP;
      regs.a2 = vicfpn;  
      regs.a3 = swpfpn;  
      if (__sys_memmap(caller, &regs) != 0) {
           MEMPHY_put_freefp(caller->active_mswp, swpfpn); 
           return -1; 
      }

      regs.a1 = SYSMEM_SWP_OP;
      regs.a2 = tgtfpn;  
      regs.a3 = vicfpn;  
       if (__sys_memmap(caller, &regs) != 0) {
            MEMPHY_put_freefp(caller->active_mswp, swpfpn);
            return -1; 
      }

      MEMPHY_put_freefp(caller->active_mswp, swpfpn);
      pte_set_swap(&mm->pgd[vicpgn], caller->active_mswp_id ,tgtfpn); 
      pte_set_fpn(&mm->pgd[pgn], vicfpn);    

      enlist_pgn_node(&caller->mm->fifo_pgn, pgn); 

  } 

  *fpn = PAGING_FPN(mm->pgd[pgn]);
  return 0; // Success
}
 
 /*pg_getval - read value at given offset
  *@mm: memory region
  *@addr: virtual address to acess
  *@value: value
  *
  */
 int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
 {
     int pgn = PAGING_PGN(addr);  
     int fpn;
     if (pg_getpage(mm, pgn, &fpn, caller) != 0) {
         return -1;  
     }
 
     int off = PAGING_OFFST(addr);  
     int phyaddr = (fpn * PAGING_PAGESZ) + off; 
     struct sc_regs regs;
     regs.a1 = SYSMEM_IO_READ;  
     regs.a2 = phyaddr;  
     regs.a3 = (uint64_t)data;  
     __sys_memmap(caller, &regs);  
     *data = (BYTE)regs.a3; 
 
     return 0;  
 }
 
 /*pg_setval - write value to given offset
  *@mm: memory region
  *@addr: virtual address to acess
  *@value: value
  *
  */
 int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
 {
   int pgn = PAGING_PGN(addr);
   int off = PAGING_OFFST(addr); 
   int fpn;
 
   if (pg_getpage(mm, pgn, &fpn, caller) != 0)
     return -1; 
 
   int phyaddr = (fpn * PAGING_PAGESZ) + off;  
   struct sc_regs regs;
   regs.a1 = SYSMEM_IO_WRITE;   
   regs.a2 = phyaddr;          
   regs.a3 = value;             
 
   __sys_memmap(caller, &regs);
 
   return 0;
 }
 
 /*__read - read value in region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@offset: offset to acess in memory region
  *@rgid: memory region ID (used to identify variable in symbole table)
  *@size: allocated size
  *
  */
 int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
 {
   struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
 
   if (currg == NULL || cur_vma == NULL)
     return -1;
 
   pg_getval(caller->mm, currg->rg_start + offset, data, caller);
 
   return 0;
 }
 
 /*libread - PAGING-based read a region memory */
 int libread(
  struct pcb_t *proc,   
  uint32_t source,      
  uint32_t offset,       
  uint32_t* destination) 
{
BYTE data = 0; 
int val = __read(proc, 0, source, offset, &data); 
if (val == 0) {
  *destination = (uint32_t)data; 
#ifdef IODUMP
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1);
#endif
  MEMPHY_dump(proc->mram);
#endif
  return 0; 
} else {
#ifdef IODUMP
  fprintf(stderr, "Error: libread failed for region=%d offset=%d (error code %d)\n", source, offset, val);
#ifdef PAGETBL_DUMP
   print_pgtbl(proc, 0, -1);
#endif
   MEMPHY_dump(proc->mram);
#endif
   *destination = (uint32_t)-1; 
   return val; 
}
}
 
 /*__write - write a region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@offset: offset to acess in memory region
  *@rgid: memory region ID (used to identify variable in symbole table)
  *@size: allocated size
  *
  */
 int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
 {
   struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
 
   if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
     return -1;
 
   pg_setval(caller->mm, currg->rg_start + offset, value, caller);
 
   return 0;
 }
 
 /*libwrite - PAGING-based write a region memory */
 int libwrite(
     struct pcb_t *proc,   
     BYTE data,           
     uint32_t destination,
     uint32_t offset)
 {
 #ifdef IODUMP
   printf("write region=%d offset=%d value=%d\n", destination, offset, data);
 #ifdef PAGETBL_DUMP
   print_pgtbl(proc, 0, -1); 
 #endif
   MEMPHY_dump(proc->mram);
 #endif
 
   return __write(proc, 0, destination, offset, data);
 }
 
 /*free_pcb_memphy - collect all memphy of pcb
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@incpgnum: number of page
  */
 int free_pcb_memph(struct pcb_t *caller)
 {
   int pagenum, fpn;
   uint32_t pte;
 
 
   for(pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
   {
     pte= caller->mm->pgd[pagenum];
 
     if (!PAGING_PAGE_PRESENT(pte))
     {
       fpn = PAGING_PTE_FPN(pte);
       MEMPHY_put_freefp(caller->mram, fpn);
     } else {
       fpn = PAGING_PTE_SWP(pte);
       MEMPHY_put_freefp(caller->active_mswp, fpn);    
     }
   }
 
   return 0;
 }
 
 
 /*find_victim_page - find victim page
  *@caller: caller
  *@pgn: return page number
  *
  */
 int find_victim_page(struct mm_struct *mm, int *retpgn) {
  if (!mm->fifo_pgn) return -1;

  // Remove the HEAD (oldest page)
  struct pgn_t *victim = mm->fifo_pgn;
  *retpgn = victim->pgn;
  mm->fifo_pgn = victim->pg_next;
  free(victim);

  return 0;
}
 
 /*get_free_vmrg_area - get a free vm region
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@size: allocated size
  *
  */
 int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
 {
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
 
   struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;
   struct vm_rg_struct *prev = NULL;
 
   if (rgit == NULL)
     return -1;
 
   /* Probe unintialized newrg */
   newrg->rg_start = newrg->rg_end = -1;
   while (rgit != NULL) {
     if ((rgit->rg_end - rgit->rg_start) >= size) {  
         newrg->rg_start = rgit->rg_start;  
         newrg->rg_end = rgit->rg_start + size;  
         rgit->rg_start += size;  
         if (rgit->rg_start == rgit->rg_end) {
           if (prev == NULL) {  
               cur_vma->vm_freerg_list = rgit->rg_next;
           } else {
               prev->rg_next = rgit->rg_next;
           }
           free(rgit);
       }
         return 0;  
     }
     prev = rgit;
     rgit = rgit->rg_next;  
   }
 
   return -1;  
 }
 //#endif
