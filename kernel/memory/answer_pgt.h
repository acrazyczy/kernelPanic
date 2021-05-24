void enable_paging() {
    // TODO: Homework 2: Enable paging
    // After initializing the page table, write to register SATP register for kernel registers.
    // Flush the TLB to invalidate the existing TLB Entries
    // Suggested: 2 LoCs
	w_satp(MAKE_SATP(kernel_pagetable));
    flush_tlb();
}

// Return the address of the PTE in page table *pagetable*
// The Risc-v Sv48 scheme has four levels of page table.
// For VA:
//   48...63 zero
//   39...47 -- 9  bits of level-3 index
//   30...38 -- 9  bits of level-2 index
//   21...29 -- 9  bits of level-1 index
//   12...20 -- 9  bits of level-0 index
//   0...11  -- 12 bits of byte offset within the page
// Return the last-level page table entry.
static pte_t* pt_query(pagetable_t pagetable, vaddr_t va, int alloc){
    if(va >= MAXVA) BUG_FMT("get va[0x%lx] >= MAXVA[0x%lx]", va, MAXVA);
    // Suggested: 18 LoCs
    pte_t *pte = NULL;
    for (int i = 39;i >= 12;i -= 9) {
	    if (pte == NULL) pte = pagetable;
	    else {
		    if (!(*pte & PTE_V)) {
			    pagetable_t new_pagetable = mm_kalloc();
			    memset(new_pagetable, 0, BD_LEAF_SIZE);
			    *pte = ((uint64) new_pagetable >> 12) << 10 | PTE_V;
		    }
	        pte = (pte_t*) ((*pte >> 10 & (((uint64) 1 << 44) - 1)) << 12);
        }
    	pte += (va >> i) & ((1 << 9) - 1);
    	if (!(*pte & PTE_V) && !alloc) break;
    }
    return /* Return value here */ pte;
}

int pt_map_pages(pagetable_t pagetable, vaddr_t va, paddr_t pa, uint64 size, int perm){
    // Suggested: 11 LoCs
    uint cnt = size / PGSIZE;
    cnt += cnt * PGSIZE < size;
    for (uint64 i = 0;i < cnt;++ i)
	    pt_map_addrs(pagetable, va + PGSIZE * i, pa + PGSIZE * i, perm);
    return 0; // Do not modify
}

paddr_t pt_query_address(pagetable_t pagetable, vaddr_t va){
    // Suggested: 3 LoCs
    pte_t *pte = pt_query(pagetable, va, 0);
    if (pte == NULL) BUG_FMT("no mapping exists for 0x%lx", va);
    return /* Return value here */ (*pte >> 10 & (((uint64)1 << 44) - 1)) << 12 | (va & ((1 << 12) - 1));
}

int pt_unmap_addrs(pagetable_t pagetable, vaddr_t va){
    // Suggested: 2 LoCs
    pte_t *pte = pt_query(pagetable, va, 0);
    if (pte == NULL) BUG_FMT("no mapping exists for 0x%lx", va);
    else *pte = 0;
    return 0; // Do not modify
}

int pt_map_addrs(pagetable_t pagetable, vaddr_t va, paddr_t pa, int perm){
    // Suggested: 2 LoCs
    pte_t *pte = pt_query(pagetable, va, 1);
    *pte = (pa >> 12) << 10 | perm | PTE_V;
    return 0; // Do not modify
}
