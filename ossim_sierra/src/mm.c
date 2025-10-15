// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  


int init_pte(uint32_t *pte, int pre, int fpn, int drt, int swp, int swptyp,
             int swpoff) {
  if (pre != 0) {
    if (swp == 0) {             // Non swap ~ page online
      if (fpn == 0) return -1;  // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
    } else {  // page swapped
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }
  }

  return 0;
}

int pte_set_swap(uint32_t *pte, int swptyp, int swpoff) {
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

  return 0;
}

int pte_set_fpn(uint32_t *pte, int fpn) {
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

  return 0;
}

/*
 * vmap_page_range - map a range of page at aligned address
 * @caller    : caller process
 * @addr      : start virtual address (page-aligned)
 * @pgnum     : number of pages to map
 * @frames    : list of physical frames allocated for these pages
 * @ret_rg    : output struct to store the virtual region boundaries mapped
 * Returns 0 on success, -1 on failure.
 */
int vmap_page_range(struct pcb_t *caller,
                    int addr,
                    int pgnum, 
                    struct framephy_struct *frames,
                    struct vm_rg_struct *ret_rg)
{
    int pgn_base = PAGING_PGN(addr); 
    int pgit; 
    struct framephy_struct *current_frame = frames;
    uint32_t *pgd = NULL; 

    if (ret_rg == NULL || caller == NULL || caller->mm == NULL || caller->mm->pgd == NULL || pgnum <= 0 || frames == NULL) {
         fprintf(stderr, "vmap_page_range: Invalid parameters (caller=%p, mm=%p, pgd=%p, pgnum=%d, frames=%p).\n",
                (void*)caller, caller ? (void*)caller->mm : NULL, caller && caller->mm ? (void*)caller->mm->pgd : NULL, pgnum, (void*)frames);
         return -1;
    }
    pgd = caller->mm->pgd; 

    ret_rg->rg_start = addr;
    ret_rg->rg_end = addr + pgnum * PAGING_PAGESZ;

    for (pgit = 0; pgit < pgnum; pgit++) 
    {
        if (current_frame == NULL) {
            fprintf(stderr, "vmap_page_range: Error - Not enough frames provided (%d required, list exhausted at page %d of loop)\n", pgnum, pgit);
            return -1; 
        }

        int current_pgn = pgn_base + pgit; 

        if (current_pgn >= PAGING_MAX_PGN) {
             fprintf(stderr, "vmap_page_range: Error - Calculated Page Number %d exceeds PAGING_MAX_PGN %d\n", current_pgn, PAGING_MAX_PGN);
             return -1;
        }

        int fpn = current_frame->fpn;  
        uint32_t *pte = &pgd[current_pgn]; 

        if (pte_set_fpn(pte, fpn) != 0) { 
           fprintf(stderr, "vmap_page_range: Error setting PTE for PGN %d\n", current_pgn);
           return -1;
        }

        if (enlist_pgn_node(&caller->mm->fifo_pgn, current_pgn) != 0) {
           fprintf(stderr, "vmap_page_range: Error enlisting PGN %d to FIFO list\n", current_pgn);
           *pte = 0; 
           return -1; 
        }

        // Move to the next frame in the list
        current_frame = current_frame->fp_next;
    }

    return 0; // Success
}


int alloc_pages_range(struct pcb_t *caller, int req_pgnum,
                      struct framephy_struct **frm_lst) {
  int pgit, fpn;
  struct framephy_struct *newfp_str = NULL;

  for (pgit = 0; pgit < req_pgnum; pgit++) {
    if (MEMPHY_get_freefp(caller->mram, &fpn) == 0) {  
      newfp_str = malloc(sizeof(struct framephy_struct));
      if (newfp_str == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return -1;
      }
      newfp_str->fpn = fpn;
      newfp_str->fp_next = *frm_lst;
      *frm_lst = newfp_str;  
    } else {
      return -1;  
    }
  }

  return 0;
}


int vm_map_ram(struct pcb_t *caller, int astart, int aend, int mapstart,
               int incpgnum, struct vm_rg_struct *ret_rg) {
  struct framephy_struct *frm_lst = NULL;

  /* Allocate frames for the requested pages */
  int ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst);
  if (ret_alloc < 0) {
    return -1; 
  }

  /* Map the allocated frames to the virtual address range */
  vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);

  return 0;
}

int __swap_cp_page(struct memphy_struct *mpsrc, int srcfpn,
                   struct memphy_struct *mpdst, int dstfpn) {
  int cellidx;
  int addrsrc, addrdst;
  for (cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++) {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }

  return 0;
}

int init_mm(struct mm_struct *mm, struct pcb_t *caller) {
  struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));
  if (vma0 == NULL) {
    fprintf(stderr, "Error: Memory allocation failed for VMA\n");
    return -1;
  }

  mm->pgd = malloc(PAGING_MAX_PGN * sizeof(uint32_t));
  if (mm->pgd == NULL) {
    fprintf(stderr, "Error: Memory allocation failed for page table\n");
    free(vma0);
    return -1;
  }

  memset(mm->pgd, 0,
         PAGING_MAX_PGN * sizeof(uint32_t));  

  vma0->vm_id = 0;
  vma0->vm_start = 0;
  vma0->vm_end = vma0->vm_start;
  vma0->sbrk = vma0->vm_start;

  struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end);
  enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);

  vma0->vm_mm = mm;
  vma0->vm_next = NULL;

  mm->mmap = vma0;
  mm->fifo_pgn = NULL;

  return 0;
}

struct vm_rg_struct *init_vm_rg(int rg_start, int rg_end) {
  struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));
  if (newrg == NULL) return NULL;

  newrg->rg_start = rg_start;
  newrg->rg_end = rg_end;
  newrg->rg_next = NULL;
  return newrg;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist,
                      struct vm_rg_struct *rgnode) {
  if (rgnode == NULL) return -1;

  rgnode->rg_next = *rglist;
  *rglist = rgnode;
  return 0;
}

int enlist_pgn_node(struct pgn_t **pgnlist, int pgn) {
  struct pgn_t *newpgn = malloc(sizeof(struct pgn_t));
  if (newpgn == NULL) return -1;

  newpgn->pgn = pgn;
  newpgn->pg_next = *pgnlist;
  *pgnlist = newpgn;
  return 0;
}

int print_list_fp(struct framephy_struct *ifp)
{
  struct framephy_struct *fp = ifp;

  printf("print_list_fp: ");
  if (fp == NULL) { printf("NULL list\n"); return -1;}
  printf("\n");
  while (fp != NULL)
  {
    printf("fp[%d]\n", fp->fpn);
    fp = fp->fp_next;
  }
  printf("\n");
  return 0;
}

int print_list_rg(struct vm_rg_struct *irg)
{
  struct vm_rg_struct *rg = irg;

  printf("print_list_rg: ");
  if (rg == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (rg != NULL)
  {
    printf("rg[%ld->%ld]\n", rg->rg_start, rg->rg_end);
    rg = rg->rg_next;
  }
  printf("\n");
  return 0;
}

int print_list_vma(struct vm_area_struct *ivma)
{
  struct vm_area_struct *vma = ivma;

  printf("print_list_vma: ");
  if (vma == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (vma != NULL)
  {
    printf("va[%ld->%ld]\n", vma->vm_start, vma->vm_end);
    vma = vma->vm_next;
  }
  printf("\n");
  return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
  printf("print_list_pgn: ");
  if (ip == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (ip != NULL)
  {
    printf("va[%d]-\n", ip->pgn);
    ip = ip->pg_next;
  }
  printf("n");
  return 0;
}

int print_pgtbl(struct pcb_t *caller, uint32_t start, uint32_t end)
{
  int pgn_start, pgn_end;
  int pgit;
  uint32_t pte;
  if (end == -1)
  {
    pgn_start = 0;
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, 0);
    end = cur_vma->vm_end;
  }
  pgn_start = PAGING_PGN(start);
  pgn_end = PAGING_PGN(end);

  printf("print_pgtbl: %d - %d", start, end);
  if (caller == NULL) { printf("NULL caller\n"); return -1;}
  printf("\n");

  for (pgit = pgn_start; pgit < pgn_end; pgit++)
  {
    printf("%08ld: %08x\n", pgit * sizeof(uint32_t), caller->mm->pgd[pgit]);
  }

  printf("print_pgtbl: %d - %d\n", start, end);
  if (caller == NULL || caller->mm == NULL || caller->mm->pgd == NULL) {
      printf("Error: print_pgtbl - NULL caller, mm, or pgd\n");
      return -1;
  }

  for (pgit = pgn_start; pgit < pgn_end; pgit++)
  {
    pte = caller->mm->pgd[pgit];

  }

  for (pgit = pgn_start; pgit < pgn_end; pgit++)
  {
    pte = caller->mm->pgd[pgit];

    if (PAGING_PAGE_PRESENT(pte) && !GETVAL(pte, PAGING_PTE_SWAPPED_MASK, 30) ) {
        int fpn = PAGING_PTE_FPN(pte);
        printf("Page Number: %d -> Frame Number: %d\n", pgit, fpn);
    }
  }
   printf("================================================================\n");
  return 0;
}
