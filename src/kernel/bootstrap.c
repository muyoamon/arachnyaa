#include "kernel/cap.h"
#include "kernel/ipc.h"
#include "kernel/kobj.h"
#include "kernel/protocol.h"
#include "mm/kheap.h"
#include "process/process.h"
#include "arch/x86/defs.h"
#include "sys/vspace.h"
#include "boot/multiboot.h"
#include <lib/string.h>

static void endpoint_release(kobj_t *obj) {
  if (!obj || !obj->payload) {
    return;
  }

  ipc_endpoint_destroy((kobj_endpoint_t *)obj->payload);
  kfree(obj->payload);
  obj->payload = NULL;
}

static kobj_t *bootstrap_endpoint_create(process_t *owner) {
  static const kobj_ops_t endpoint_ops = {
      .release = endpoint_release,
  };

  kobj_t *endpoint = kobj_create();
  if (!endpoint) {
    return NULL;
  }

  kobj_endpoint_t *payload = kzalloc(sizeof(*payload));
  if (!payload) {
    kobj_put(endpoint);
    return NULL;
  }

  if (ipc_endpoint_init(payload, owner) != KERR_OK) {
    kfree(payload);
    kobj_put(endpoint);
    return NULL;
  }

  endpoint->type = KOBJ_ENDPOINT;
  endpoint->supported_ops = KOP_OPEN | KOP_CALL | KOP_WRITE | KOP_CLOSE | KOP_READ | KOP_EXEC;
  endpoint->ops = &endpoint_ops;
  endpoint->payload = payload;
  return endpoint;
}

cap_handle_t process_install_bootstrap_log_handler(process_t *proc) {
  if (!proc) {
    return 0;
  }

  kobj_t *endpoint = bootstrap_endpoint_create(proc);
  if (!endpoint) {
    return 0;
  }

  cap_rights_t rights = {
      .bits = CAP_RIGHT_BIND_PROTOCOL | R_EP_BIND | R_EP_CALL | R_EP_REPLY,
      .flags = 0,
      .off = 0,
      .len = 0,
  };

  cap_handle_t h = kcap_install_root(proc, endpoint, rights);
  kobj_put(endpoint);
  return h;
}

/* Layout of the compact module table written at physical 0x0. Must match
   boot_mod_table_t in user/ulib/bootmod.h exactly. */
#define _BOOT_MOD_MAGIC     0x4D4F4442u
#define _BOOT_MOD_MAX       16
#define _BOOT_MOD_NAME_MAX  32

typedef struct {
  uint32_t phys_start;
  uint32_t phys_end;
  char     name[_BOOT_MOD_NAME_MAX];
} _boot_mod_entry_t;

typedef struct {
  uint32_t          magic;
  uint32_t          mod_count;
  _boot_mod_entry_t mods[_BOOT_MOD_MAX];
} _boot_mod_table_t;

static void bm_vmobj_release(kobj_t *obj) {
  if (!obj || !obj->payload) return;
  /* phys_fixed: do not free physical pages; they are boot-time memory */
  kfree(obj->payload);
  obj->payload = NULL;
}

static const kobj_ops_t bm_vmobj_ops = { .release = bm_vmobj_release };

cap_handle_t process_install_boot_manifest_cap(process_t *proc,
                                               multiboot_info_t *mb_info) {
  if (!proc) return 0;

  /* Serialize a compact module table to physical 0x0 (= BOOT_INFO_BASE).
     The module array and cmdline strings are in low memory (< 1 MB) and
     accessible via BOOT_INFO_BASE.  The multiboot_info struct itself may
     be above 1 MB, but its fields mods_addr/mods_count point into low mem. */
  _boot_mod_table_t *tbl = (_boot_mod_table_t *)(uintptr_t)BOOT_INFO_BASE;
  memset(tbl, 0, sizeof(*tbl));
  tbl->magic = _BOOT_MOD_MAGIC;

  if (mb_info && (mb_info->flags & MULTIBOOT_INFO_MODS) &&
      mb_info->mods_count > 0 && mb_info->mods_addr < BOOT_INFO_SIZE) {
    uint32_t n = mb_info->mods_count;
    if (n > _BOOT_MOD_MAX) n = _BOOT_MOD_MAX;
    tbl->mod_count = n;

    const multiboot_module_t *mods =
        (const multiboot_module_t *)((uintptr_t)mb_info->mods_addr +
                                     BOOT_INFO_BASE);
    for (uint32_t i = 0; i < n; i++) {
      tbl->mods[i].phys_start = mods[i].mod_start;
      tbl->mods[i].phys_end   = mods[i].mod_end;
      if (mods[i].cmdline && mods[i].cmdline < BOOT_INFO_SIZE) {
        const char *cmd =
            (const char *)((uintptr_t)mods[i].cmdline + BOOT_INFO_BASE);
        size_t j = 0;
        while (j < _BOOT_MOD_NAME_MAX - 1 && cmd[j]) {
          tbl->mods[i].name[j] = cmd[j];
          j++;
        }
        tbl->mods[i].name[j] = '\0';
      }
    }
  }

  kobj_vmobj_t *vm = kzalloc(sizeof(*vm));
  if (!vm) return 0;
  vm->num_pages  = BOOT_INFO_SIZE / PAGE_SIZE;
  vm->phys_fixed = true;
  vm->phys_base  = 0;

  kobj_t *obj = kobj_create();
  if (!obj) {
    kfree(vm);
    return 0;
  }
  obj->type    = KOBJ_VMOBJ;
  obj->ops     = &bm_vmobj_ops;
  obj->payload = vm;

  cap_rights_t rights = { .bits = R_VM_READ | R_VM_MAP };
  cap_handle_t h = kcap_install_root(proc, obj, rights);
  kobj_put(obj);
  return h;
}
