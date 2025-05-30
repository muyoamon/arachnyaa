#ifndef ARACHNYAA_ARCH_X86_PAGING_H_
#define ARACHNYAA_ARCH_X86_PAGING_H_

#include <stdint.h>

#define KERNEL_VIRT_BASE 0xC0000000
#define KERNEL_PHYS_OFFSET (0x100000)


void page_fault_handler(uint32_t error_code);



#endif // ARACHNYAA_ARCH_X86_PAGING_H_
