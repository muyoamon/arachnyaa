#include "sys/vspace.h"
#include "kernel/cap.h"
#include "kernel/error.h"
#include "kernel/kobj.h"
#include "kernel/mm.h"
#include "mm/kheap.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "process/process.h"
#include "process/scheduler.h"

/* ── KOBJ_ASPACE ─────────────────────────────────────────────────────────── */

static void aspace_release(kobj_t *obj) {
  if (!obj || !obj->payload) return;
  as_put((as_t *)obj->payload);
  obj->payload = NULL;
}

static const kobj_ops_t aspace_ops = { .release = aspace_release };

static cap_handle_t install_aspace_cap(process_t *proc, as_t *as) {
  as_get(as);  /* cap holds a reference */
  kobj_t *obj = kobj_create();
  if (!obj) {
    as_put(as);
    return 0;
  }
  obj->type = KOBJ_ASPACE;
  obj->ops = &aspace_ops;
  obj->payload = as;
  cap_rights_t rights = { .bits = R_AS_MAP | R_AS_UNMAP };
  cap_handle_t h = kcap_install_root(proc, obj, rights);
  kobj_put(obj);
  return h;
}

cap_handle_t sys_vspace_create(void) {
  process_t *proc = scheduler_get_current()->proc;
  as_t *as = as_create();
  if (!as) return 0;
  cap_handle_t h = install_aspace_cap(proc, as);
  /* as_create gave refcnt=1; install_aspace_cap called as_get (+1=2);
     drop our local reference so only the cap holds it. */
  as_put(as);
  return h;
}

cap_handle_t sys_vspace_self(void) {
  process_t *proc = scheduler_get_current()->proc;
  if (!proc->mm) return 0;
  return install_aspace_cap(proc, proc->mm);
}

/* ── KOBJ_VMOBJ ──────────────────────────────────────────────────────────── */

static void vmobj_release(kobj_t *obj) {
  if (!obj || !obj->payload) return;
  kobj_vmobj_t *vm = (kobj_vmobj_t *)obj->payload;
  if (!vm->phys_fixed && vm->phys_frames) {
    for (size_t i = 0; i < vm->num_pages; i++) {
      if (vm->phys_frames[i])
        pmm_free_frame((void *)vm->phys_frames[i]);
    }
    kfree(vm->phys_frames);
  }
  kfree(vm);
  obj->payload = NULL;
}

static const kobj_ops_t vmobj_ops = { .release = vmobj_release };

cap_handle_t sys_page_alloc(size_t num_pages, uint32_t flags, uintptr_t phys_addr) {
  if (num_pages == 0) return 0;

  process_t *proc = scheduler_get_current()->proc;

  kobj_vmobj_t *vm = kzalloc(sizeof(*vm));
  if (!vm) return 0;
  vm->num_pages = num_pages;

  if (flags & SYS_PAGE_F_FIXED) {
    vm->phys_fixed = true;
    vm->phys_base = phys_addr;
  } else {
    vm->phys_fixed = false;
    vm->phys_frames = kcalloc(num_pages, sizeof(uintptr_t));
    if (!vm->phys_frames) {
      kfree(vm);
      return 0;
    }
    for (size_t i = 0; i < num_pages; i++) {
      void *frame = pmm_alloc_frame();
      if (!frame) {
        for (size_t j = 0; j < i; j++) pmm_free_frame((void *)vm->phys_frames[j]);
        kfree(vm->phys_frames);
        kfree(vm);
        return 0;
      }
      vm->phys_frames[i] = (uintptr_t)frame;
    }
  }

  kobj_t *obj = kobj_create();
  if (!obj) {
    if (!vm->phys_fixed && vm->phys_frames) {
      for (size_t i = 0; i < num_pages; i++)
        if (vm->phys_frames[i]) pmm_free_frame((void *)vm->phys_frames[i]);
      kfree(vm->phys_frames);
    }
    kfree(vm);
    return 0;
  }

  obj->type = KOBJ_VMOBJ;
  obj->ops = &vmobj_ops;
  obj->payload = vm;

  cap_rights_t rights = { .bits = R_VM_READ | R_VM_WRITE | R_VM_EXEC | R_VM_MAP | R_VM_DERIVE };
  cap_handle_t h = kcap_install_root(proc, obj, rights);
  kobj_put(obj);
  return h;
}

/* ── Mapping / Unmapping ─────────────────────────────────────────────────── */

static uint64_t prot_to_pte(uint32_t prot) {
  uint64_t flags = PTE_PRESENT | PTE_USER;
  if (prot & VMM_PROT_WRITE) flags |= PTE_WRITABLE;
  return flags;
}

int sys_vspace_map(const sys_vspace_map_args_t *args) {
  if (!args) return KERR_INVAL;
  process_t *proc = scheduler_get_current()->proc;

  const cap_entry_t *as_e = cap_resolve(proc, args->vspace_cap, R_AS_MAP);
  if (!as_e || as_e->obj->type != KOBJ_ASPACE) return KERR_INVAL;
  as_t *as = (as_t *)as_e->obj->payload;

  const cap_entry_t *vm_e = cap_resolve(proc, args->page_cap, R_VM_MAP);
  if (!vm_e || vm_e->obj->type != KOBJ_VMOBJ) return KERR_INVAL;
  kobj_vmobj_t *vm = (kobj_vmobj_t *)vm_e->obj->payload;

  uint64_t pte = prot_to_pte(args->prot_flags);

  for (size_t i = 0; i < vm->num_pages; i++) {
    uintptr_t phys = vm->phys_fixed
                       ? (vm->phys_base + i * PAGE_SIZE)
                       : vm->phys_frames[i];
    vmm_map_user((uintptr_t)as->ptable, args->virt_addr + i * PAGE_SIZE, phys, pte);
  }
  return KERR_OK;
}

int sys_vspace_unmap(cap_handle_t vspace_cap, uintptr_t virt_addr, size_t num_pages) {
  process_t *proc = scheduler_get_current()->proc;

  const cap_entry_t *as_e = cap_resolve(proc, vspace_cap, R_AS_UNMAP);
  if (!as_e || as_e->obj->type != KOBJ_ASPACE) return KERR_INVAL;
  as_t *as = (as_t *)as_e->obj->payload;

  kerror_t err = vmm_unmap_user_range((uintptr_t)as->ptable,
                                      virt_addr, num_pages * PAGE_SIZE);
  return err;
}
