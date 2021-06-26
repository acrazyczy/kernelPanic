//
// Created by Wenxin Zheng on 2021/4/21.
//

#ifndef ACMOS_SPR21_ANSWER_LOCKS_H
#define ACMOS_SPR21_ANSWER_LOCKS_H

#include "process.h"

void push_off()
{
	int old = intr_get();

	intr_off();
	if(mycpu() -> noff == 0)
		mycpu() -> intena = old;
	++ mycpu() -> noff;
}

void pop_off()
{
	cpu_t *c = mycpu();
	if(intr_get())
		BUG("pop_off - interruptible");
	if(c -> noff < 1)
		BUG("pop_off");
	-- c -> noff;
	if(c -> noff == 0 && c -> intena)
		intr_on();
}

int lock_init(struct lock *lock, char *name){
	/* Your code here */
	lock -> locked = 0, lock -> cpuid = -1;
	if(nlock >= MAXLOCKS) BUG("Max lock count reached.");
	locks[nlock ++] = lock;
	lock -> name = name;
	return 0;
}

void acquire(struct lock *lock){
	/* Your code here */
	push_off();
	if (holding_lock(lock)) BUG_FMT("The lock %s is already held.", lock -> name);
	while (__sync_lock_test_and_set(&lock -> locked, 1));
	__sync_synchronize();
	lock -> cpuid = cpuid();
}

// Try to acquire the lock once
// Return 0 if succeed, -1 if failed.
int try_acquire(struct lock *lock){
	/* Your code here */
	if (holding_lock(lock)) BUG_FMT("The lock %s is already held.", lock -> name);
	if (!__sync_lock_test_and_set(&lock -> locked, 1)) {
		__sync_synchronize();
		lock -> cpuid = cpuid();
		return 0;
	} else return -1;
}

void release(struct lock* lock){
	/* Your code here */
	if (holding_lock(lock)) {
		lock -> cpuid = -1;
		__sync_synchronize();
		__sync_lock_release(&lock -> locked);
	} else BUG_FMT("The lock %s isn't held by current processor.", lock -> name);
}

int is_locked(struct lock* lock){
	return lock->locked;
}

// private for spin lock
int holding_lock(struct lock* lock){
	/* Your code here */
	if (lock -> cpuid == cpuid()) return 1;
	return 0;
}

#endif  // ACMOS_SPR21_ANSWER_LOCKS_H