void* kern_page_malloc() {
    // Your Code here
    // Suggested: 6 LoCs
    if (allocated_page < INIT_MAX_PAGES) {
    	uint64 addr = kernel_page_available[0];
	    ++ allocated_page;
	    kernel_page_available[0] = kernel_page_available[INIT_MAX_PAGES - allocated_page];
	    return (void*) addr;
    }
    return NULL;
}

void kern_page_free(void* ptr) {
    if (/* Your condition here */ (uint64) ptr < (uint64) kernel_page_initialized
        ) {
        printk("[kfree] invalid kernel free function: %lx\n", ptr);
        return;
    }
    // Suggested: 2 LoCs
    kernel_page_available[INIT_MAX_PAGES - allocated_page] = (uint64) ptr;
    -- allocated_page;
}

// malloc a page
void *mm_kalloc(){
    page_t *valid_page = _buddy_alloc_page(0);
    if(valid_page == NULL) {
        BUG_FMT("Page pool is empty, no valid page.%s", "\n");
        return NULL;
    }
    uint64 idx = _buddy_get_page_idx(valid_page);
    return (void*)(bd_meta.data_head + PGSIZE * idx);
}

void mm_kfree(void* ptr){
    if((paddr_t)ptr > bd_meta.data_tail || (paddr_t)ptr < bd_meta.data_head)
        BUG_FMT("Address 0x%lx cannot be freed. Valid range is [0x%lx <-> 0x%lx]",
                ptr, bd_meta.data_head, bd_meta.data_tail);
    uint64 area_idx = _buddy_get_area_idx(ptr);
    buddy_free_page(_buddy_idx_get_page(area_idx));
}
