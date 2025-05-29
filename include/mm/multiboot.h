// arachnyaa/include/pmm/multiboot.h
#ifndef ARACHNYAA_MM_MULTIBOOT_H
#define ARACHNYAA_MM_MULTIBOOT_H

#include <stdint.h>

#define MULTIBOOT_INFO_MEMORY           0x00000001 // Is there a full memory map?
#define MULTIBOOT_INFO_MEM_MAP          0x00000040 // Is mmap_addr and mmap_length valid?

typedef struct multiboot_info {
    uint32_t flags;         // Flags to indicate which fields are valid
    uint32_t mem_lower;     // Amount of lower memory (in KiB)
    uint32_t mem_upper;     // Amount of upper memory (in KiB)
    uint32_t boot_device;   // BIOS boot device
    uint32_t cmdline;       // Kernel command line
    uint32_t mods_count;    // Number of modules loaded
    uint32_t mods_addr;     // Address of the first module structure
    // ELF section header table (if flags[5] is set)
    struct {
        uint32_t num;
        uint32_t size;
        uint32_t addr;
        uint32_t shndx;
    } elf_sec;
    uint32_t mmap_length;   // Length of memory map
    uint32_t mmap_addr;     // Address of memory map
    // ... other fields ...
} __attribute__((packed)) multiboot_info_t;

typedef struct multiboot_mmap_entry {
    uint32_t size;          // Size of this entry structure itself (not the region)
    uint64_t addr;          // Starting address of the memory region
    uint64_t len;           // Length of the memory region in bytes
    uint32_t type;          // Type of region (1 = Available RAM, others = reserved/ACPI/etc.)
} __attribute__((packed)) multiboot_mmap_entry_t;

// Type field for mmap_entry
#define MULTIBOOT_MEMORY_AVAILABLE        1
#define MULTIBOOT_MEMORY_RESERVED         2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS              4
#define MULTIBOOT_MEMORY_BADRAM           5

#endif // MULTIBOOT_H
