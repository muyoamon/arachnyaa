#ifndef ARACHNYAA_KERNEL_BOOTINFO_H_
#define ARACHNYAA_KERNEL_BOOTINFO_H_

#include <stddef.h>
#include <pmm/pmm.h>
#include <stdint.h>

#define BOOT_INFO_MAX_MEM_REGIONS 16

typedef struct {
  uint32_t magic_cookie;
  const char* bootloader_name;
  const char* kernel_cmdline;

  size_t num_memory_regions;
  memory_map_entry_t memory_map[BOOT_INFO_MAX_MEM_REGIONS];

  // pointers to important info 
  uintptr_t initrd_start;
  uintptr_t initrd_end;

  uintptr_t framebuffer_addr;
  uint32_t framebuffer_width;
  uint32_t framebuffer_height;
  uint32_t frambuffer_pitch;
  uint8_t framebuffer_bpp;

} kernel_boot_info_t;

#endif // ARACHNYAA_KERNEL_BOOTINFO_H_
