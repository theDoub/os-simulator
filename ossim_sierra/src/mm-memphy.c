// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory physical module mm/mm-memphy.c
 */

 #include "mm.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 
 /*
  *  MEMPHY_mv_csr - move MEMPHY cursor
  *  @mp: memphy struct
  *  @offset: offset
  */
 int MEMPHY_mv_csr(struct memphy_struct *mp, int offset)
 {
    int numstep = 0;
 
    mp->cursor = 0;
    while (numstep < offset && numstep < mp->maxsz)
    {
       /* Traverse sequentially */
       mp->cursor = (mp->cursor + 1) % mp->maxsz;
       numstep++;
    }
 
    return 0;
 }
 
 /*
  *  MEMPHY_seq_read - read MEMPHY device sequentially
  *  @mp: memphy struct
  *  @addr: address
  *  @value: obtained value
  */
 int MEMPHY_seq_read(struct memphy_struct *mp, int addr, BYTE *value)
 {
    if (mp == NULL)
       return -1;
 
    if (!mp->rdmflg)
       return -1; /* Not compatible mode for sequential read */
 
    MEMPHY_mv_csr(mp, addr);
    *value = (BYTE)mp->storage[addr];
 
    return 0;
 }
 
 /*
  *  MEMPHY_read - read MEMPHY device
  *  @mp: memphy struct
  *  @addr: address
  *  @value: obtained value
  */
 int MEMPHY_read(struct memphy_struct *mp, int addr, BYTE *value)
 {
    if (mp == NULL || mp->maxsz <= 0 || mp->storage == NULL)
       return -1;
   
   if (addr < 0 || addr >= mp->maxsz)
      return -1;
 
    if (mp->rdmflg)
       *value = mp->storage[addr];
    else /* Sequential access device */
       return MEMPHY_seq_read(mp, addr, value);
 
    return 0;
 }
 
 /*
  *  MEMPHY_seq_write - write MEMPHY device sequentially
  *  @mp: memphy struct
  *  @addr: address
  *  @data: written data
  */
 int MEMPHY_seq_write(struct memphy_struct *mp, int addr, BYTE value)
 {
    if (mp == NULL)
       return -1;
 
    if (!mp->rdmflg)
       return -1; /* Not compatible mode for sequential write */
 
    MEMPHY_mv_csr(mp, addr);
    mp->storage[addr] = value;
 
    return 0;
 }
 
 /*
  *  MEMPHY_write - write MEMPHY device
  *  @mp: memphy struct
  *  @addr: address
  *  @data: written data
  */
 int MEMPHY_write(struct memphy_struct *mp, int addr, BYTE data)
 {
    if (mp == NULL || mp->maxsz <= 0 || mp->storage == NULL)
       return -1;
      
   if (addr < 0 || addr >= mp->maxsz)
      return -1;
 
    if (mp->rdmflg)
       mp->storage[addr] = data;
    else /* Sequential access device */
       return MEMPHY_seq_write(mp, addr, data);
 
    return 0;
 }
 
 /*
  *  MEMPHY_format - format MEMPHY device
  *  @mp: memphy struct
  *  @pagesz: page size
  */
 int MEMPHY_format(struct memphy_struct *mp, int pagesz)
 {
    int numfp = mp->maxsz / pagesz;
    struct framephy_struct *newfst, *fst;
    int iter = 0;
 
    if (numfp <= 0)
       return -1;
 
    /* Initialize head of free framephy list */
    fst = malloc(sizeof(struct framephy_struct));
    fst->fpn = iter;
    mp->free_fp_list = fst;
 
    /* Fill in the rest of the free frame list */
    for (iter = 1; iter < numfp; iter++)
    {
       newfst = malloc(sizeof(struct framephy_struct));
       newfst->fpn = iter;
       newfst->fp_next = NULL;
       fst->fp_next = newfst;
       fst = newfst;
    }
 
    return 0;
 }
 
 /*
  *  MEMPHY_get_freefp - get a free frame from MEMPHY
  *  @mp: memphy struct
  *  @retfpn: returned frame page number
  */
 int MEMPHY_get_freefp(struct memphy_struct *mp, int *retfpn)
 {
   if(mp == NULL || retfpn == NULL)
      return -1;
    struct framephy_struct *fp = mp->free_fp_list;
 
    if (fp == NULL || mp->maxsz <= 0)
       return -1;
 
    *retfpn = fp->fpn;
    mp->free_fp_list = fp->fp_next;
 
    /* Free the used frame node */
    free(fp);
 
    return 0;
 }
 
 /*
  *  MEMPHY_put_freefp - return a frame to the free list
  *  @mp: memphy struct
  *  @fpn: frame page number
  */
 int MEMPHY_put_freefp(struct memphy_struct *mp, int fpn)
 {
    struct framephy_struct *fp = mp->free_fp_list;
    struct framephy_struct *newnode = malloc(sizeof(struct framephy_struct));
 
    /* Create a new node with the given frame page number */
    newnode->fpn = fpn;
    newnode->fp_next = fp;
    mp->free_fp_list = newnode;
 
    return 0;
 }
 
 /*
  *  MEMPHY_dump - dump MEMPHY content
  *  @mp: memphy struct
  */
 int MEMPHY_dump(struct memphy_struct *mp)
 {
    printf("PHYSICAL MEMORY DUMP:\n");
    for (int i = 0; i < mp->maxsz; i++)
    {
       if (mp->storage[i] != 0)
          printf("BYTE %08X: %d\n", i, mp->storage[i]);
          

    }
    printf("PHYSICAL MEMORY DUMP:\n");
    printf("================================================================\n");
    return 0;
 }
 
 /*
  *  init_memphy - initialize MEMPHY struct
  *  @mp: memphy struct
  *  @max_size: maximum size of memory
  *  @randomflg: random access flag
  */
 int init_memphy(struct memphy_struct *mp, int max_size, int randomflg)
 {
    mp->storage = (BYTE *)malloc(max_size * sizeof(BYTE));
    mp->maxsz = max_size;
    memset(mp->storage, 0, max_size * sizeof(BYTE));
 
    MEMPHY_format(mp, PAGING_PAGESZ);
 
    mp->rdmflg = (randomflg != 0) ? 1 : 0;
 
    if (!mp->rdmflg) /* Not random access device, then it is sequential */
       mp->cursor = 0;
 
    return 0;
 }
 
 // #endif
