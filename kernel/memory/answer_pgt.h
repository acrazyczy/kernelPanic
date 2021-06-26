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
	for (int level = 3;level > 0;-- level) {
		pte_t *pte = pagetable + PX(level, va);
		if (*pte & PTE_V) pagetable = (pagetable_t) PTE2PA(*pte);
		else {
			if (!alloc || (pagetable = (pde_t*) mm_kalloc()) == 0) return 0;
			memset(pagetable, 0, PGSIZE);
			*pte = PA2PTE(pagetable) | PTE_V;
		}
	}
	return pagetable + PX(0, va);
}

int pt_map_pages(pagetable_t pagetable, vaddr_t va, paddr_t pa, uint64 size, int perm){
	// Suggested: 11 LoCs
	vaddr_t current = PGROUNDDOWN(va), last = PGROUNDDOWN(va + size - 1);
	for (pte_t *pte;;) {
		if ((pte = pt_query(pagetable, current, 1)) == NULL) return -1;
		if (*pte & PTE_V) BUG("remap");
		*pte = PA2PTE(pa) | perm | PTE_V;
		if (current == last) break;
		current += PGSIZE, pa += PGSIZE;
	}
	return 0; // Do not modify
}

paddr_t pt_query_address(pagetable_t pagetable, vaddr_t va){
	// Suggested: 3 LoCs
	pte_t *pte = pt_query(pagetable, va, 0);
	if (pte == NULL || (*pte & PTE_V) == 0) {
		FAIL_FMT("no mapping exists for 0x%lx", va);
		return NULL;
	}
	if ((*pte & PTE_U) == 0) {
		FAIL("access denied");
		return NULL;
	}
	return /* Return value here */ PTE2PA(*pte);
}

int pt_unmap_addrs(pagetable_t pagetable, vaddr_t va){
	// Suggested: 2 LoCs
	pte_t *pte = pt_query(pagetable, va, 0);
	if (pte == NULL || (*pte & PTE_V) == 0) BUG_FMT("no mapping exists for 0x%lx", va);
	if (PTE_FLAGS(*pte) == PTE_V) BUG_FMT("0x%lx is not a leaf", va);
	*pte = 0;
	return 0; // Do not modify
}

int pt_map_addrs(pagetable_t pagetable, vaddr_t va, paddr_t pa, int perm){
	// Suggested: 2 LoCs
	pte_t *pte = pt_query(pagetable, va, 1);
	if (pte == NULL) return -1;
	*pte = PA2PTE(pa) | perm | PTE_V;
	return 0; // Do not modify
}

void pt_unmap_pages(pagetable_t pagetable, vaddr_t va, uint npages, bool do_free) {
	pte_t *pte;

	for (vaddr_t current = va;current < va + npages * PGSIZE;current += PGSIZE) {
		if ((pte = pt_query(pagetable, current, 0)) == 0 || (*pte & PTE_V) == 0)
			FAIL_FMT("page at 0x%lx not found", current);
		else {
			if (PTE_FLAGS(*pte) == PTE_V) BUG_FMT("0x%lx is not a leaf", current);
			if (do_free) mm_kfree((void *) PTE2PA(*pte));
			*pte = 0;
		}
	}
}

uint64 pt_dealloc_pages(pagetable_t pagetable, uint64 oldsz, uint64 newsz) {
	if (newsz >= oldsz) return oldsz;
	if (PGROUNDUP(newsz) < PGROUNDUP(oldsz)) {
		uint npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
		pt_unmap_pages(pagetable, PGROUNDUP(newsz), npages, 1);
	}
	return newsz;
}

uint64 pt_alloc_pages(pagetable_t pagetable, uint64 oldsz, uint64 newsz) {
	char *mem;

	if (oldsz > newsz) return oldsz;

	for (uint64 current = oldsz = PGROUNDUP(oldsz);current < newsz;current += PGSIZE) {
		mem = mm_kalloc();
		if (mem == NULL) {
			pt_dealloc_pages(pagetable, current, oldsz);
			return 0;
		}
		memset(mem, 0, PGSIZE);
		if (pt_map_pages(pagetable, current, (uint64) mem, PGSIZE, PTE_W | PTE_X | PTE_R | PTE_U) != 0) {
			mm_kfree(mem);
			pt_dealloc_pages(pagetable, current, oldsz);
			return 0;
		}
	}
	return newsz;
}

void pt_free(pagetable_t pagetable) {
	for (int i = 0;i < PGSIZE / sizeof(pte_t);++ i) {
		pte_t pte = pagetable[i];
		if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0) {
			paddr_t child = PTE2PA(pte);
			pt_free((pagetable_t) child);
			pagetable[i] = 0;
		} else if (pte & PTE_V) BUG("leaf");
	}
	mm_kfree(pagetable);
}

void pt_free_pagetable(pagetable_t pagetable, uint64 sz) {
	if (sz > 0) pt_unmap_pages(pagetable, 0, PGROUNDUP(sz) / PGSIZE, true);
	pt_free(pagetable);
}

void pt_clear_page(pagetable_t pagetable, vaddr_t va) {
	pte_t *pte = pt_query(pagetable, va, 0);
	if (pte == NULL) BUG_FMT("not a valid page to clear: 0x%lx", va);
	*pte &= ~PTE_U;
}