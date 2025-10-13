#ifndef ARACHNYAA_ARCH_X86_DEFS_H_
#define ARACHNYAA_ARCH_X86_DEFS_H_

#define PAGE_SIZE 4096

//
// Memory Layout
//


#define KSTACK_ARENA_BASE 0xD0000000
#define KSTACK_ARENA_SIZE 0x01000000 // 16 MB

#define KSTACK_DEFAULT_SIZE (2*PAGE_SIZE)

#define KHEAP_BASE      0xC0400000
#define KHEAP_SIZE      0x00400000 // 4MB

#define KERNEL_BASE     0xC0000000

#define USER_TOP        0xBFDFF000
#define USER_STACK_TOP  (USER_TOP & ~0xF)
#define USER_STACK_SIZE (1<<20)

#define USER_HEAP_BASE  0x09000000

#define USER_ENTRY_BASE 0x08048000







#define USER_CS         0x1B      // ring3 code selector 
#define USER_DS         0x23      // ring3 data selector

#endif // ARACHNYAA_ARCH_X86_DEFS_H_
