# Project 3
## Problem 1
The binary code of `putc` is included in `binary_include.S` and then loaded by `load_binary()` from `binary_putc_start`.
## Problem 2
Context switching in user mode need to store all registers, change the pagetable and switch the privilege mode.

Context switching in kernel mode only need to store the saved registers.