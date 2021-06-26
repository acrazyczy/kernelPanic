//
// Created by Wenxin Zheng on 2021/1/31.
//

#ifndef ACMOS_SPR21_PAGETABLE_H
#define ACMOS_SPR21_PAGETABLE_H

#include <riscv.h>
#include "mm.h"

extern pagetable_t kernel_pagetable;


void pt_init();
pte_t* _pt_query(pagetable_t pagetable, vaddr_t va);

void pt_kern_vmmap();
int pt_map_pages(pagetable_t pagetable, vaddr_t va, paddr_t pa, uint64 size, int perm);


int pt_unmap_addrs(pagetable_t pagetable, vaddr_t va);
int pt_map_addrs(pagetable_t pagetable, vaddr_t va, paddr_t pa, int perm);
void enable_paging();
paddr_t pt_query_address(pagetable_t pagetable, vaddr_t va);
void pt_unmap_pages(pagetable_t pagetable, vaddr_t va, uint npages, bool do_free);
uint64 pt_dealloc_pages(pagetable_t pagetable, uint64 oldsz, uint64 newsz);
vaddr_t pt_alloc_pages(pagetable_t pagetable, vaddr_t oldva, vaddr_t newva);
void pt_free(pagetable_t pagetable);
void pt_free_pagetable(pagetable_t pagetable, uint64 sz);
void pt_clear_page(pagetable_t pagetable, vaddr_t va);

#endif  // ACMOS_SPR21_PAGETABLE_H
