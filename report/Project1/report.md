# Project 1
## Problem 1
### (1)
```
_entry      0000000080000000
start       00000000800009a2
timer_init  0000000080000936
main        000000008000001c
```
### (2)
Use `rbreak .` to set breakpoint on each function and use `bt` to display stack trace.
## Problem 2
### (1)
Qemu's -kernel at `0x1000` jumps to `0x80000000`, which is set in `kernel.ld` and which `_entry` is located at.

`_entry` sets up a stack for each CPU and then jumps to `start()`.

`start()` sets M Previous Privilege mode to Supervisor and M Exception Program Counter to main,
initializes the page table,
delegates all interrupts and exceptions to supervisor mode,
sets up to receive timer interrupts in machine mode,
keeps each CPU's hartid in its tp register,
and switches to supervisor mode and jump to `main()`.

Then system kernel is started.
### (2)
Run `qemu-system-riscv64 -machine virt -bios none -kernel $(BUILD_DIR)/kernel.img -m size=1G` and then qemu begins to read the `kernel.img` from `0x0`.
## Problem 3
`printk` in kernel displays characters by directly write them to `UART`.

`printf` in `stdio.h` displays characters by invoking a system call.