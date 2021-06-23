#include "process.h"
#include "lock.h"
#include "pagetable.h"
#include "memlayout.h"
#include "trap.c"
#include "elf.h"

extern const char binary_putc_start;
extern char trampoline[];
cpu_t CPUs[NCPU];
thread_t *running[NCPU];
struct list_head sched_list[NCPU];
process_t procs[NPROC];
thread_t thrs[NTHR];
struct lock pidlock, tidlock, schedlock;

int _pid, _tid;

static void load_segment(pagetable_t pagetable, vaddr_t va, const char *bin, uint64 offset, uint64 sz);

// 将ELF文件映射到给定页表的地址空间，返回pc的数值
// 关于 ELF 文件，请参考：https://docs.oracle.com/cd/E19683-01/816-1386/chapter6-83432/index.html
static uint64 load_binary(pagetable_t *target_page_table, const char *bin){
	struct elf_file *elf;
    int i;
    uint64 seg_sz, p_vaddr, seg_map_sz, current_sz = 0;
	elf = elf_parse_file(bin);
	
	/* load each segment in the elf binary */
	for (i = 0; i < elf->header.e_phnum; ++i) {
		if (elf->p_headers[i].p_type == PT_LOAD) {
            // 根据 ELF 文件格式做段映射
            // 从ELF中获得这一段的段大小
            seg_sz = elf->p_headers[i].p_memsz;
            // 对应段的在内存中的虚拟地址
            p_vaddr = elf->p_headers[i].p_vaddr;
            // 对映射大小做页对齐
			seg_map_sz = ROUNDUP(seg_sz + p_vaddr, PGSIZE) - PGROUNDDOWN(p_vaddr);
            // 接下来代码的期望目的：将程序代码映射/复制到对应的内存空间
            // 一种可能的实现如下：
            /* 
             * 在 target_page_table 中分配一块大小
             * 通过 memcpy 将某一段复制进入这一块空间
             * 页表映射修改
             */
            if (seg_sz < elf -> p_headers[i].p_filesz || p_vaddr % PGSIZE != 0) goto bad;
            uint64 new_sz = pt_alloc_pages(target_page_table, current_sz, p_vaddr + seg_sz);
            if (new_sz == 0) goto bad;
            current_sz = new_sz;
            load_segment(target_page_table, p_vaddr, bin, elf -> p_headers[i].p_offset, elf -> p_headers[i].p_filesz);
		}
	}
	/* PC: the entry point */
	return elf->header.e_entry;
	bad:
}

static void load_segment(pagetable_t pagetable, vaddr_t va, const char *bin, uint64 offset, uint64 sz) {
	for (uint64 i = 0;i < sz;i += PGSIZE) {
		paddr_t pa = pt_query_address(pagetable, va + i);
		if (pa == NULL) BUG_FMT("loadseg: invalid address 0x%lx", va + i);
		uint64 load_sz = sz - i < PGSIZE ? sz - i : PGSIZE;
		memcpy(pa, bin + offset + i, load_sz);
	}
}

static int alloc_pid() {
	int pid;
	acquire(&pidlock);
	pid = _pid ++;
	release(&pidlock);
	return pid;
}

/* 分配一个进程，需要至少完成以下目标：
 * 
 * 分配一个主线程
 * 创建一张进程页表
 * 分配pid、tid
 * 设置初始化线程上下文
 * 设置初始化线程返回地址寄存器ra，栈寄存器sp
 * 
 * 这个函数传入参数为一个二进制的代码和一个线程指针(具体传入规则可以自己修改)
 * 此外程序首次进入用户态之前，应该设置好trap处理向量为usertrap（或者你自定义的）
 */
process_t *alloc_proc(const char* bin, thread_t *thr){
	process_t *p = NULL; // to do: alloc p
	acquire(&p -> lock);
	p -> process_state = RUNNABLE;
	p -> pid = alloc_pid();
	init_list_head(&p -> thread_list);
	if ((p -> trapframes = mm_kalloc()) == NULL) {
		free_proc(p);
		release(&p -> lock);
		return NULL;
	}
	if ((p -> pagetable = proc_pagetable(p)) == NULL) {
		free_proc(p);
		release(&p -> lock);
		return NULL;
	}
    thr = alloc_thr(bin);
    if (thr == NULL) {
    	free_proc(p);
    	release(&p -> lock);
    	return NULL;
    }
    thr -> process = p;
    list_add(&thr -> process_list_thread_node, &p -> thread_list);
	release(&thr -> lock);
    return p;
}

static int alloc_tid() {
	int tid;
	acquire(&tidlock);
	tid = _tid ++;
	release(&tidlock);
	return tid;
}

static void load_thread_ret() {
	release(&mythread() -> lock);
	usertrapret();
}

thread_t *alloc_thr(const char *bin) {
	thread_t *t = NULL; // to do: alloc t
	acquire(&t -> lock);
	t -> thread_state = RUNNABLE;
	t -> tid = alloc_tid();
	init_list_head(&t -> process_list_thread_node);
	init_list_head(&t -> sched_list_thread_node);
	memset(&t -> context, 0, sizeof(t -> context));
	t -> context.ra = (uint64) load_thread_ret;
	t -> context.sp = t -> kstack + PGSIZE;
	return t;
}

pagetable_t proc_pagetable(process_t *p) {
	pagetable_t pagetable;
	if ((pagetable = (pagetable_t) mm_kalloc()) == NULL) return NULL;
	memset(pagetable, 0, PGSIZE);
	if ((~pt_map_pages(pagetable, TRAMPOLINE, PGSIZE, (uint64) trampoline, PTE_R | PTE_X)) == 0) {
		pt_free_pagetable(pagetable, 0);
		return NULL;
	}
	if ((~pt_map_pages(pagetable, TRAPFRAME, PGSIZE, (uint64) p -> trapframes, PTE_R | PTE_W)) == 0) {
		pt_unmap_pages(pagetable, TRAMPOLINE, 1, 0);
		pt_free_pagetable(pagetable, 0);
		return NULL;
	}
	return pagetable;
}

void proc_freepagetable(pagetable_t pagetable, uint64 sz) {
	pt_unmap_pages(pagetable, TRAMPOLINE, 1, 0);
	pt_unmap_pages(pagetable, TRAPFRAME, 1, 0);
	pt_free_pagetable(pagetable, sz);
}

void free_proc(process_t *p) {
	if (p -> trapframes) mm_kfree(p -> trapframes);
	p -> trapframes = NULL;
	if (p -> pagetable) proc_freepagetable(p -> pagetable, p -> sz);
	p -> pagetable = NULL;
	p -> sz = p -> pid = p -> killed = p -> xstate = 0;
	p -> process_state = UNUSED;
}

bool load_thread(file_type_t type){
    if(type == PUTC){
        thread_t *t = NULL;
        process_t *p = alloc_proc(&binary_putc_start, t);
        if(!t) return false;
        sched_enqueue(t);
    } else {
        BUG("Not supported");
    }

}

// sched_enqueue和sched_dequeue的主要任务是加入一个任务到队列中和删除一个任务
// 这两个函数的展示了如何使用list.h中可的函数（加入、删除、判断空、取元素）
// 具体可以参考：Stackoverflow上的回答
// https://stackoverflow.com/questions/15832301/understanding-container-of-macro-in-the-linux-kernel
void sched_enqueue(thread_t *target_thread){
    if(target_thread->thread_state == RUNNING) BUG("Running Thread cannot be scheduled.");
    list_add(&target_thread->sched_list_thread_node, &(sched_list[cpuid()]));
}

thread_t *sched_dequeue(){
    if(list_empty(&(sched_list[cpuid()]))) BUG("Scheduler List is empty");
    thread_t *head = container_of(&(sched_list[cpuid()]), thread_t, sched_list_thread_node);
    list_del(&head->sched_list_thread_node);
    return head;
}

bool sched_empty(){
    return list_empty(&(sched_list[cpuid()]));
}

// 开始运行某个特定的函数
void thread_run(thread_t *target){
	thread_t *t;
	cpu_t *c = mycpu();
	uint cid = (c - CPUs) / sizeof(cpu_t);
	running[cid] = NULL;

	intr_on();

	acquire(&t -> lock);
	ASSERT_EQ(t -> thread_state, RUNNABLE, "unrunnable thread found in sched_list");
	t -> thread_state = RUNNING;
	acquire(&t -> process -> lock);
	if (t -> process -> running_threads ++)
		t -> process -> process_state = RUNNING;
	release(&t -> process -> lock);
	sched_dequeue();
	running[cid] = t;

	swtch(&c -> context, &t -> context);

	running[cid] = NULL;
	release(&t -> lock);
}

// sched_start函数启动调度，按照调度的队列开始运行。
void sched_start(){
    while(1){
        if(sched_empty()) BUG("Scheduler list empty, no app loaded");
        thread_t *next = sched_dequeue();
        thread_run(next);
    }
}

void sched_init(){
    // 初始化调度队列锁
    lock_init(&schedlock);
    // 初始化队列头
    init_list_head(&(sched_list[cpuid()]));
}

void proc_init(){
    // 初始化pid、tid锁
    lock_init(&pidlock);
    lock_init(&tidlock);
    // 接下来代码期望的目的：映射第一个用户线程并且插入调度队列
    if(!load_thread(PUTC)) BUG("Load failed");
    // to do
}

cpu_t *mycpu() {return CPUs + cpuid();}

thread_t *mythread() {
	push_off();
	int id = cpuid();
	thread_t *t = running[id];
	pop_off();
	return t;
}

void yield() {
	thread_t *t = mythread();
	acquire(&t -> lock);
	t -> thread_state = RUNNABLE;
	acquire(&t -> process -> lock);
	if (-- t -> process -> running_threads == 0)
		t -> process -> process_state = list_empty(&t -> process -> thread_list) ? UNUSED : RUNNABLE;
	release(&t -> process -> lock);
	sched_enqueue(t);
	sched();
	release(&t -> lock);
}

void sched() {
	int intena;
	thread_t *t = mythread();

	if (!holding_lock(&t -> lock)) BUG("sched p -> lock");
	if (mycpu() -> noff != 1) BUG("sched locks");
	if (t -> thread_state == RUNNABLE) BUG("sched running");
	if (intr_get()) BUG("sched interruptible");

	intena = mycpu() -> intena;
	swtch(&t -> context, &mycpu() -> context);
	mycpu() -> intena = intena;
}