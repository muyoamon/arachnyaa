#include "kernel/cap.h"
#include "kernel/ipc.h"
#include "kernel/kobj.h"
#include "kernel/protocol.h"
#include "mm/kheap.h"
#include "process/process.h"
#include "arch/x86/defs.h"
#include "sys/vspace.h"

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
  endpoint->supported_ops = KOP_OPEN | KOP_CALL | KOP_WRITE | KOP_CLOSE | KOP_READ;
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

static void bm_vmobj_release(kobj_t *obj) {
  if (!obj || !obj->payload) return;
  /* phys_fixed: do not free physical pages; they are boot-time memory */
  kfree(obj->payload);
  obj->payload = NULL;
}

static const kobj_ops_t bm_vmobj_ops = { .release = bm_vmobj_release };

cap_handle_t process_install_boot_manifest_cap(process_t *proc,
                                               uint32_t mb_info_phys) {
  if (!proc) return 0;

  /* Write the physical offset of multiboot_info_t at virtual address
   * BOOT_INFO_BASE (= physical 0x0) so initd can locate it after mapping. */
  *(volatile uint32_t *)(uintptr_t)BOOT_INFO_BASE = mb_info_phys;

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
