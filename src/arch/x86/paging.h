#ifndef ARACHNYAA_ARCH_X86_PAGING_H_
#define ARACHNYAA_ARCH_X86_PAGING_H_

#include <stdint.h>

#define KERNEL_VIRT_BASE 0xC0000000
#define KERNEL_PHYS_OFFSET (0x100000)

// reserve pdpt:3 pd:510 pt:500-511 for vmm temp virtual addr
#define TEMP_MAPPING_BASE 0xFFDF4000
#define TEMP_MAPPING_TOP  0xFFDFF000

#define KERNEL_PDPT_INDEX 3


void page_fault_handler(uint32_t error_code, uint32_t eip, uint32_t useresp, uint32_t ebp3, uint32_t eax3, uint32_t edi3, uint32_t esi3, uint32_t ebx3);



#endif // ARACHNYAA_ARCH_X86_PAGING_H_
