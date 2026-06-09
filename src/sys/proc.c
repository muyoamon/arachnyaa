#include "sys/proc.h"
#include "boot/multiboot.h"
#include "kernel/cap.h"
#include "kernel/elf_loader.h"
#include "kernel/error.h"
#include "kernel/kobj.h"
#include "kernel/mm.h"
#include "mm/vmm.h"
#include "process/process.h"
#include "process/scheduler.h"
#include <lib/string.h>

static void _release_proc_cap(struct kobj *obj) {
  process_t *proc = (process_t *)obj->payload;
  if (!proc) return;
  if (proc->main)
    scheduler_remove(proc->main->tid);
  process_free(proc);
}

static kobj_ops_t _process_ops = {
  .release = _release_proc_cap
};

static cap_handle_t _install_proc_cap(process_t *parent, process_t *child) {
  kobj_t *obj = kobj_create();
  if (!obj) return 0;
  obj->type = KOBJ_PROC;
  obj->payload = child;
  obj->ops = &_process_ops;
  cap_rights_t rights = {
    .bits = R_PROC_SIGNAL | R_PROC_CTRL | R_PROC_INSP | R_PROC_WAIT | R_PROC_TRANSFER,
  };
  cap_handle_t h = kcap_install_root(parent, obj, rights);
  kobj_put(obj);
  return h;
}

static void _transfer_caps_to_child(process_t *parent, process_t *child,
                                     const sys_proc_arg_t *args) {
  if (args->endpoint != 0) {
    const cap_entry_t *e = cap_resolve(parent, args->endpoint, 0);
    if (e) kcap_transfer(parent, child, args->endpoint, e->rights.bits, NULL);
  }
  for (int i = 0; i < 3; i++) {
    if (args->stdio[i] == 0) continue;
    const cap_entry_t *e = cap_resolve(parent, args->stdio[i], 0);
    if (e) kcap_transfer(parent, child, args->stdio[i], e->rights.bits, NULL);
  }
}

int sys_proc_spawn(sys_proc_arg_t *args, pid_t *pid, cap_handle_t *cap) {
  if (!args) {
    return KERR_INVAL;
  }

  process_t *parent = scheduler_get_current()->proc;
  process_t *child = NULL;

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

    child = process_spawn_from_elf(&img, args->argv0);
    if (!child) return KERR_UNKNOWN;

  } else if (args->flags & SYS_PROG_F_VSPACE) {
    if (!args->entry || !args->vspace) return KERR_INVAL;

    const cap_entry_t *as_e = cap_resolve(parent, args->vspace, R_AS_MAP);
    if (!as_e || as_e->obj->type != KOBJ_ASPACE) return KERR_INVAL;
    as_t *target_as = (as_t *)as_e->obj->payload;

    child = process_spawn_from_vspace(target_as, args->entry,
                                      args->user_sp, args->argv0);
    if (!child) return KERR_UNKNOWN;

  } else {
    return KERR_UNSUPPORTED;
  }

  _transfer_caps_to_child(parent, child, args);
  scheduler_add(child->main);

  if (pid) *pid = child->pid;

  cap_handle_t h = _install_proc_cap(parent, child);
  if (cap) *cap = h;

  return KERR_OK;
}

int sys_proc_wait(cap_handle_t proc_cap) {
  process_t *caller = scheduler_get_current()->proc;
  const cap_entry_t *e = cap_resolve(caller, proc_cap, R_PROC_WAIT);
  if (!e) return -KERR_INVAL;
  if (e->obj->type != KOBJ_PROC) return -KERR_PERM;

  process_t *target = (process_t *)e->obj->payload;
  if (!target) return -KERR_NOTFOUND;

  /* Process already exited (no main thread). */
  if (!target->main) return target->exit_code;

  thread_t *current = scheduler_get_current();
  target->waiting_thread = current;
  current->state = T_BLOCKED;
  scheduler_reschedule();

  return target->exit_code;
}
