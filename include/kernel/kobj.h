#ifndef ARACHNYAA_KERNEL_KOBJ_H_
#define ARACHNYAA_KERNEL_KOBJ_H_

#include "kernel/protocol.h"
#include <lib/stddef.h>
#include <stdatomic.h>
#include <stdint.h>

struct process;
struct kobj;

typedef struct {
  struct kobj *obj;
  uint32_t rights;
} kobj_open_result_t;

typedef struct kobj_ops {
  void (*release)(struct kobj *obj);
  int (*open)(struct kobj *handler, struct process *caller, const char *path,
              uint32_t flags, kobj_open_result_t *out);
  int (*write)(struct kobj *obj, const void *buf, size_t len,
               size_t *out_len);
} kobj_ops_t;

typedef enum {
  KOBJ_VMOBJ = 1,
  KOBJ_ENDPOINT,
  KOBJ_ASPACE,
  KOBJ_TASK,          // process
  KOBJ_THREAD,
  KOBJ_TUNNEL,
  KOBJ_FUTEX,
  KOBJ_DEVICE,
  KOBJ_IOSTREAM,
  KOBJ_LOGSINK,
} kobj_type_t;

typedef struct kobj {
  kobj_type_t type;
  atomic_uint refcnt;
  uint32_t supported_ops;
  const kobj_ops_t *ops;
  void *payload;
} kobj_t;


kobj_t *kobj_create(void);

void kobj_get(kobj_t *kobj);

void kobj_put(kobj_t *kobj);

void kobj_init(void);


#endif // ARACHNYAA_KERNEL_KOBJ_H_
