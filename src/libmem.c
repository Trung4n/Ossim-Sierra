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

#define PAGING_PAGE_COUNT(start, end) (PAGING_PAGE_ALIGNSZ(end) - PAGING_PAGE_ALIGNSZ(start)) / PAGING_PAGESZ

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt) {
  struct vm_rg_struct *prev = NULL;
  struct vm_rg_struct *curr = mm->mmap->vm_freerg_list;

  // Invalid region: start must < end
  if (rg_elmt->rg_start >= rg_elmt->rg_end)
      return -1;
  // If list has dummy head (zero-sized), replace with new region
  else if(curr && curr->rg_start == curr->rg_end){ 
    mm->mmap->vm_freerg_list = rg_elmt;
    return 0;
  }
  // Traverse list to find insert pos
  while (curr != NULL && curr->rg_start < rg_elmt->rg_start) {
      prev = curr;  
      curr = curr->rg_next;
  }
  // Merge with prev if adjacent
  if (prev && prev->rg_end == rg_elmt->rg_start) {
      prev->rg_end = rg_elmt->rg_end;
      // Also merge with curr if adjacent to new merged region
      if (curr && curr->rg_start == prev->rg_end) {
          prev->rg_end = curr->rg_end;
          prev->rg_next = curr->rg_next;
          free(curr);
      }
      free(rg_elmt);
      return 0;
  }
  // Merge with curr if adjacent at head
  if (curr && rg_elmt->rg_end == curr->rg_start) {
      curr->rg_start = rg_elmt->rg_start;
      free(rg_elmt);
      return 0;
  }
  // Insert rg_elmt between prev and curr
  rg_elmt->rg_next = curr;
  if (prev == NULL) {
      mm->mmap->vm_freerg_list = rg_elmt;
  } else {
      prev->rg_next = rg_elmt;
  }

  return 0;
}

/*delist_vm_freerg_list - remove specify rg from freerg_list
 *@mm: memory region
 *@rg_elmt: specify region
 *
 */
int delist_vm_freerg_list(struct mm_struct **mm, struct vm_rg_struct *rg_elmt){
  struct vm_rg_struct *rg_node = (*mm)->mmap->vm_freerg_list;
  struct vm_rg_struct *prev = NULL; 

  // Case: rm head node
  if(rg_node != NULL && rg_node->rg_start == rg_elmt->rg_start && rg_node->rg_end == rg_elmt->rg_end){
    (*mm)->mmap->vm_freerg_list = rg_node->rg_next;
    free(rg_node);
    return 0;
  }

  // Search for exact match node
  while(rg_node != NULL && (rg_node->rg_start != rg_elmt->rg_start || rg_node->rg_end != rg_elmt->rg_end)){
    prev = rg_node;
    rg_node = rg_node->rg_next;
  }

  // Not found
  if(rg_node == NULL) 
    return -1;

  // Unlink node
  prev->rg_next = rg_node->rg_next;
  free(rg_node);

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid){
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr){
  pthread_mutex_lock(&mmvm_lock); // sync vm alloc
  /*Allocate at the toproof */
  struct vm_rg_struct rgnode;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if(cur_vma == NULL){ // invalid VMA
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  // Try alloc from freelist (fit algo inside get_free_vmrg_area)
  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0){
    // Commit alloc info to symtbl
    cur_vma->vm_mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    cur_vma->vm_mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
    *alloc_addr = rgnode.rg_start;

#ifdef DEBUG
    printf("===== PHYSICAL MEMORY AFTER ALLOCATION =====\n");
    printf("PID=%d - Region=%d - Address=%08x - Size=%d byte\n", caller->pid, rgid, *alloc_addr, size);
#ifdef PAGETBL_DUMP
    print_pgtbl(caller, 0, -1);
#endif
#endif
    pthread_mutex_unlock(&mmvm_lock);
    return 0; // alloc success
  }

  // Fallback: no fitting free region, try sbrk-style growth
  int inc_sz = PAGING_PAGE_ALIGNSZ(size);
  int old_sbrk = cur_vma->sbrk;

  // Prepare syscall args to grow VMA limit
  struct sc_regs regs;
  regs.a1 = SYSMEM_INC_OP;
  regs.a2 = (uint32_t) vmaid;
  regs.a3 = (uint32_t) inc_sz;
  
  // SYSCALL 17 - ask kernel to grow VMA
  if(syscall(caller, 17, &regs) < 0){
    pthread_mutex_unlock(&mmvm_lock);
    return -1; // fail to inc limit
  }

  // Commit alloc range
  cur_vma->vm_mm->symrgtbl[rgid].rg_start = old_sbrk;
  cur_vma->vm_mm->symrgtbl[rgid].rg_end = old_sbrk + size;
  *alloc_addr = old_sbrk;

  // Any leftover after alloc => push to freelist
  if(cur_vma->sbrk - cur_vma->vm_mm->symrgtbl[rgid].rg_end > 0){
    struct vm_rg_struct * free_rg = malloc(sizeof(struct vm_rg_struct));
    free_rg->rg_start = cur_vma->vm_mm->symrgtbl[rgid].rg_end;
    free_rg->rg_end = cur_vma->sbrk;
    enlist_vm_freerg_list(cur_vma->vm_mm, free_rg);
  }
  
#ifdef DEBUG
  printf("===== PHYSICAL MEMORY AFTER ALLOCATION =====\n");
  printf("PID=%d - Region=%d - Address=%08x - Size=%d byte\n", caller->pid, rgid, *alloc_addr, size);
#ifdef PAGETBL_DUMP
  print_pgtbl(caller, 0, -1);
#endif
#endif
  
  pthread_mutex_unlock(&mmvm_lock);
  return 0;

}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid){
  
  if(rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return -1; // invalid region id

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  // Fetch region info from symtbl
  struct vm_rg_struct * rgnode = malloc(sizeof(struct vm_rg_struct));
  rgnode->rg_start = cur_vma->vm_mm->symrgtbl[rgid].rg_start;
  rgnode->rg_end = cur_vma->vm_mm->symrgtbl[rgid].rg_end;
  
  // Push freed mem to freelist if valid
  if(rgnode->rg_end - rgnode->rg_start > 0)
    enlist_vm_freerg_list(caller->mm, rgnode);
  else
    free(rgnode); // skip garbage

  // Clean up symtbl entry
  cur_vma->vm_mm->symrgtbl[rgid].rg_start = cur_vma->vm_mm->symrgtbl[rgid].rg_end = 0;

#ifdef DEBUG
  printf("===== PHYSICAL MEMORY AFTER DEALLOCATION =====\n");
  printf("PID=%d - Region=%d\n", caller->pid, rgid);
#ifdef PAGETBL_DUMP
  print_pgtbl(caller, 0, -1);
#endif
#endif

  return 0;
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index){
  /* TODO Implement allocation on vm area 0 */
  int addr;

  /* By default using vmaid = 0 */
  return __alloc(proc, 0, reg_index, size, &addr);
}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int libfree(struct pcb_t *proc, uint32_t reg_index){
  /* TODO Implement free region */

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
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller){
  uint32_t pte = mm->pgd[pgn];

  if (!PAGING_PAGE_PRESENT(pte)){ /* Page is not online, make it actively living */
    int vicpgn, swpfpn; 
    int vicfpn;
    uint32_t vicpte;

    // Pick victim + alloc swpfpn
    if(MEMPHY_get_freefp(caller->active_mswp, &swpfpn) < 0 || find_victim_page(caller->mm, &vicpgn) < 0)
      return -1;
    
    // Victim info
    vicpte = mm->pgd[vicpgn];
    vicfpn = PAGING_PTE_FPN(vicpte);
    struct sc_regs regs;
    regs.a1 = SYSMEM_SWP_OP; 
    regs.a2 = (uint32_t) vicfpn;
    regs.a3 = (uint32_t) swpfpn;

    // Swap out victim: RAM -> SWAP
    if(syscall(caller, 17, &regs) < 0)
      return -1;

    // Bring in requested page: SWAP -> RAM
    if(__swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn) < 0)
      return -1;

    // Mark victim as swapped
    pte_set_swap(&mm->pgd[vicpgn], 0, swpfpn); 

    // Set new page table entry
    pte_set_fpn(&mm->pgd[pgn], vicfpn);

    // FIFO: add new pgn to FIFO queue
    enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
  }
  *fpn = PAGING_FPN(mm->pgd[pgn]); // out: RAM fpn

  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller){
  int pgn = PAGING_PGN(addr);                   // Page num
  int off = PAGING_OFFST(addr);                 // Offset
  int fpn;                                      // Frame num

  // Ensure page present, swap in if needed
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  // Calc phys addr = frame base + offset
  int phyaddr = fpn * PAGING_ADDR_FPN_LOBIT + off;
  // MEMPHY_read(caller->mram, phyaddr, data);

  // Setup syscall regs for IO READ.
  struct sc_regs regs;
  regs.a1 = (uint32_t) SYSMEM_IO_READ;
  regs.a2 = (uint32_t) phyaddr;
  // regs.a3 = (uint32_t) data;

  // SYSCALL 17: write byte at phys addr
  if(syscall(caller, 17, &regs) < 0)
    return -1;

  // Update data
  *data = (BYTE) regs.a3;

  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller){
  int pgn = PAGING_PGN(addr);               
  int off = PAGING_OFFST(addr);     
  int fpn;                               
  
  if (pg_getpage(mm, pgn, &fpn, caller) != 0){
    return -1;
  }

  int phyaddr = fpn * PAGING_PAGESZ + off;

  // Setup syscall regs for IO WRITE.
  struct sc_regs regs;
  regs.a1 = (uint32_t) SYSMEM_IO_WRITE;
  regs.a2 = (uint32_t) phyaddr;
  regs.a3 = (uint32_t) value;

  // SYSCALL 17: write byte at phys addr
  if(syscall(caller, 17, &regs) < 0)
    return -1;

  // Output read value
  // value = (BYTE) regs.a3;
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
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data){
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;
  
  pg_getval(caller->mm, currg->rg_start + offset, data, caller);

  return 0;
}

/*libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    uint32_t offset,    // Source address = [source] + [offset]
    uint32_t* destination)
{
  BYTE data;
  int val = __read(proc, 0, source, offset, &data);

  /* TODO update result of reading action*/
  *destination = (uint32_t) data;
#ifdef DEBUG
  printf("===== PHYSICAL MEMORY AFTER READING =====\n");
#endif
#ifdef IODUMP
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return val;
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
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    uint32_t offset)
{
  int val =  __write(proc, 0, destination, offset, data);
#ifdef DEBUG
  printf("===== PHYSICAL MEMORY AFTER WRITING =====\n");
#endif
#ifdef IODUMP
  printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return val;
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
int find_victim_page(struct mm_struct *mm, int *retpgn){
  struct pgn_t *prev = NULL;
  struct pgn_t *curr = mm->fifo_pgn;
  if(curr == NULL) // Empty FIFO
    return -1; 

  if (curr->pg_next == NULL) {  // Only 1 node
    *retpgn = curr->pgn; // Victim = head
    mm->fifo_pgn = NULL;
    free(curr);
    return 0;
  }

  // Traverse to tail
  while (curr->pg_next != NULL) {
    prev = curr;
    curr = curr->pg_next;
  }

  *retpgn = curr->pgn; // Tail = victim
  prev->pg_next = NULL;
  free(curr);

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

  /* Probe unintialized newrg */
  newrg->rg_start = newrg->rg_end = 0;

  while(rgit != NULL){
    if(rgit->rg_end - rgit->rg_start >= size){
      // Fit found: assign region
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;

      long long rg_end = rgit->rg_end;
      delist_vm_freerg_list(&caller->mm, rgit); // Remove from free list

      // Split remainder, if any
      if(rg_end - newrg->rg_end > 0){
        struct vm_rg_struct *free_rg = malloc(sizeof(struct vm_rg_struct));
        free_rg->rg_start = newrg->rg_end;
        free_rg->rg_end = rg_end;
        enlist_vm_freerg_list(caller->mm, free_rg);
      }
      return 0;
    }
    rgit = rgit->rg_next;
  }

  return -1; // No fit region found
}

//#endif
