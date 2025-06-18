#include <mm/layout.h>
#include <arch/x86/defs.h>

void* kheap_base = (void*)KHEAP_BASE;
void* kheap_end = (void*)(KHEAP_BASE + KHEAP_SIZE);

void* kernel_stack_region_base = (void*)KSTACK_BASE;
void* kernel_stack_region_top = (void*)(KSTACK_BASE + KSTACK_SIZE);

void* kernel_base = (void*)KERNEL_BASE;
void* kernel_top = (void*)(KERNEL_BASE + 0x400000);
