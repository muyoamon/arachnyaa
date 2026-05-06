#include "drivers/tty.h"
#include "kernel/cap.h"
#include "kernel/error.h"
#include "kernel/kobj.h"
#include "kernel/protocol.h"
#include "mm/kheap.h"
#include "process/process.h"
#include "lib/string.h"

typedef struct {
  uint32_t reserved;
} log_endpoint_payload_t;

static void payload_release(kobj_t *obj) {
  if (obj->payload) {
    kfree(obj->payload);
    obj->payload = NULL;
  }
}

static int log_sink_write(kobj_t *obj, const void *buf, size_t len,
                          size_t *out_len) {
  (void)obj;
  tty_write((const char *)buf, len);
  if (out_len) {
    *out_len = len;
  }
  return 0;
}

static int log_handler_open(kobj_t *handler, struct process *caller,
                            const char *path, uint32_t flags,
                            kobj_open_result_t *out) {
  (void)handler;
  (void)caller;
  (void)flags;
  if (strcmp(path, "stdout") != 0 && strcmp(path, "console") != 0 &&
      strcmp(path, "") != 0) {
    return KERR_NOTFOUND;
  }

  static const kobj_ops_t log_sink_ops = {
      .release = NULL,
      .open = NULL,
      .write = log_sink_write,
  };

  kobj_t *sink = kobj_create();
  if (!sink) {
    return KERR_NOMEM;
  }

  sink->type = KOBJ_LOGSINK;
  sink->supported_ops = KOP_WRITE | KOP_CLOSE;
  sink->ops = &log_sink_ops;
  sink->payload = NULL;

  out->obj = sink;
  out->rights = KOP_WRITE | KOP_CLOSE;
  return 0;
}

static kobj_t *bootstrap_log_handler_create(void) {
  static const kobj_ops_t log_handler_ops = {
      .release = payload_release,
      .open = log_handler_open,
      .write = NULL,
  };

  kobj_t *handler = kobj_create();
  if (!handler) {
    return NULL;
  }

  log_endpoint_payload_t *payload = kzalloc(sizeof(*payload));
  if (!payload) {
    kobj_put(handler);
    return NULL;
  }

  handler->type = KOBJ_ENDPOINT;
  handler->supported_ops = KOP_OPEN;
  handler->ops = &log_handler_ops;
  handler->payload = payload;
  return handler;
}

cap_handle_t process_install_bootstrap_log_handler(process_t *proc) {
  kobj_t *handler = bootstrap_log_handler_create();
  if (!handler) {
    return 0;
  }

  cap_rights_t rights = {
      .bits = CAP_RIGHT_BIND_PROTOCOL | KOP_OPEN,
      .flags = 0,
      .off = 0,
      .len = 0,
  };
  cap_handle_t h = kcap_install_root(proc, handler, rights);
  kobj_put(handler);
  return h;
}
