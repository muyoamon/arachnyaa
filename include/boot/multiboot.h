// arachnyaa/include/boot/multiboot.h
#ifndef ARACHNYAA_BOOT_MULTIBOOT_H
#define ARACHNYAA_BOOT_MULTIBOOT_H

#include <stdbool.h>
#include <stdint.h>

#define MULTIBOOT_INFO_MEMORY           0x00000001 // Is there a full memory map?
#define MULTIBOOT_INFO_MEM_MAP          0x00000040 // Is mmap_addr and mmap_length valid?
#define MULTIBOOT_INFO_MODS             (0x1 << 3)

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

typedef struct {
  uint32_t mod_start;
  uint32_t mod_end;
  uint32_t cmdline;
  uint32_t pad;
} multiboot_module_t;

bool multiboot_find_module(multiboot_info_t* info, const char* name, multiboot_module_t *mod);

multiboot_info_t *multiboot_get_info(void);

void multiboot_set_info(multiboot_info_t*);

/**
 * @brief Map bootinfo at BOOT_INFO_BASE
 *
 */
void multiboot_map_bootinfo(void);

void multiboot_unmap_bootinfo(void);

// Type field for mmap_entry
#define MULTIBOOT_MEMORY_AVAILABLE        1
#define MULTIBOOT_MEMORY_RESERVED         2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS              4
#define MULTIBOOT_MEMORY_BADRAM           5

#endif // MULTIBOOT_H
