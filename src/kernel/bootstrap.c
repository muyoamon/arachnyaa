#include "kernel/cap.h"
#include "kernel/ipc.h"
#include "kernel/kobj.h"
#include "kernel/protocol.h"
#include "mm/kheap.h"
#include "process/process.h"

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
  endpoint->supported_ops = KOP_OPEN | KOP_WRITE | KOP_CLOSE | KOP_READ;
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
