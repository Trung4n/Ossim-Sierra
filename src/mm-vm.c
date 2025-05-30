// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid){
    struct vm_area_struct *pvma = mm->mmap;

    if (mm->mmap == NULL)
        return NULL;

    int vmait = pvma->vm_id;

    while (vmait < vmaid){
      if (pvma == NULL)
        return NULL;

      pvma = pvma->vm_next;
      vmait = pvma->vm_id;
    }

    return pvma;
}

int __mm_swap_page(struct pcb_t *caller, int vicfpn , int swpfpn){
    __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
    return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz){
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  if(cur_vma == NULL) return NULL;
  struct vm_rg_struct * newrg = malloc(sizeof(struct vm_rg_struct));

  /* Align sbrk upward to nearest alignedsz boundary */
  newrg->rg_start = (cur_vma->sbrk + alignedsz - 1) / alignedsz * alignedsz;
  newrg->rg_end = cur_vma->sbrk + size;

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend){
  struct vm_area_struct *vma = caller->mm->mmap;
  while(vma != NULL){
    if(vma->vm_id == vmaid){  // Skip the VMA that matches the current ID (caller’s VMA)
      vma = vma->vm_next;
      continue;
    } 
    else if((vmastart > vma->vm_start && vmastart < vma->vm_end) || 
            (vmaend > vma->vm_start && vmaend < vma->vm_end) ||
            (vmastart < vma->vm_start && vmaend > vma->vm_end)){
      return -1;
    }
  }
  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz){
  struct vm_rg_struct * newrg = malloc(sizeof(struct vm_rg_struct));
  int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
  int incnumpage =  inc_amt / PAGING_PAGESZ;
  struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if(cur_vma == NULL) 
    return -1;

  int old_end = cur_vma->vm_end;

  // Check if the new region overlaps with any existing VMA
  if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0){
    return -1; // Overlap detected, abort allocation
  }

  // Expand VMA limit and program break (sbrk)
  cur_vma->vm_end += inc_sz;
  cur_vma->sbrk += inc_sz;

  // Request physical memory mapping for the extended region
  if (vm_map_ram(caller, area->rg_start, area->rg_end, old_end, incnumpage , newrg) < 0){  
    /* Roll back */
    cur_vma->vm_end -= inc_sz;
    cur_vma->sbrk -= inc_sz;    
    return -1; /* Map the memory to MEMRAM */
  }
    
  return 0;
}

// #endif
