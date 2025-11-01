#include "boot/multiboot.h"
#include "arch/x86/defs.h"
#include "mm/vmm.h"
#include <lib/string.h>
#include <stdint.h>

#ifndef PHYS_TO_VIRT 
#define PHYS_TO_VIRT(p) ((uintptr_t)(p) + (0xC0000000 - 0x100000))
#endif

bool multiboot_find_module(multiboot_info_t *info, const char *name, multiboot_module_t *mod) {
  if (!info || !name || !mod) return false;

  if (!(info->flags & MULTIBOOT_INFO_MODS) || info->mods_count == 0) return false;

  const multiboot_module_t *mods = (const multiboot_module_t *)(info->mods_addr + BOOT_INFO_BASE);
  for (uint32_t i = 0; i < info->mods_count; i++) {
    const char *cmd = (const char*)(mods[i].cmdline + BOOT_INFO_BASE);
    if (cmd && strcmp(cmd, name) == 0) {
      mod->mod_start = PHYS_TO_VIRT(mods[i].mod_start);
      mod->mod_end = PHYS_TO_VIRT(mods[i].mod_end);
      mod->cmdline = (uintptr_t)cmd;
      return true;
    }
  }
  return false;
}


void multiboot_map_bootinfo(void) {
  vmm_map(BOOT_INFO_BASE, 0x0, BOOT_INFO_SIZE/PAGE_SIZE, PTE_PRESENT);
}

void multiboot_unmap_bootinfo(void) {
  for (uintptr_t i = BOOT_INFO_BASE; i < BOOT_INFO_BASE + BOOT_INFO_SIZE; i += PAGE_SIZE)
    vmm_unmap(i);
}
