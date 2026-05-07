#include "sys/proc.h"
#include "boot/multiboot.h"
#include "kernel/elf_loader.h"
#include "kernel/error.h"
#include "mm/vmm.h"
#include "process/process.h"
#include "process/scheduler.h"


int sys_proc_spawn(sys_proc_arg_t *args, pid_t *pid,
                   cap_handle_t *cap) {
  // v1 only support boot module flag for now.
  if (!args) {
    return KERR_INVAL;
  }

  if (args->flags & SYS_PROG_F_BOOTMODULE) {

    multiboot_module_t mod;
    multiboot_info_t *boot_mb_info = multiboot_get_info();
    if (!multiboot_find_module(boot_mb_info, args->module_name, &mod)) {
      return KERR_NOTFOUND;
    }

    vmm_map(mod.mod_start, mod.mod_start,
            (mod.mod_end - mod.mod_start - 1) / PAGE_SIZE + 1, PTE_PRESENT);

    elf_image_t img = {
        .bytes = (void *)(uintptr_t)mod.mod_start,
        .size = mod.mod_end - mod.mod_start,
    };

    process_t *child = process_spawn_from_elf(&img, args->argv0);

    if (!child) {
      return KERR_UNKNOWN;
    }

    scheduler_add(child->main);
    if (pid != NULL) {
      *pid = child->pid;
    }
    // cap initialization is not implemented in v1
    (void)cap;
    return KERR_OK;
  } else {
    // v1 only support boot module mode
    return KERR_UNSUPPORTED;
  }

  return KERR_OK;
}
