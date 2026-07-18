#ifndef ARACHNYAA_KERNEL_KOBJ_H_
#define ARACHNYAA_KERNEL_KOBJ_H_

#include "kernel/protocol.h"
#include <lib/stddef.h>
#include <stdatomic.h>
#include <stdint.h>

struct process;
struct kobj;
struct thread;

typedef struct {
  struct kobj *obj;
  uint32_t rights;
} kobj_open_result_t;

/*
 * Kernel object operations are lifecycle-only.
 * Protocol verbs like OPEN/WRITE are routed through IPC, not direct callbacks.
 */
typedef struct kobj_ops {
  void (*release)(struct kobj *obj);
} kobj_ops_t;

typedef enum {
  KOBJ_NONE = 0,
  KOBJ_VMOBJ = 1,
  KOBJ_ENDPOINT,
  KOBJ_REMOTE,
  KOBJ_ASPACE,
  KOBJ_PROC,
  KOBJ_THREAD,
  KOBJ_TUNNEL,
  KOBJ_FUTEX,
  KOBJ_DEVICE,
  KOBJ_IOSTREAM,
  KOBJ_IRQ,
  KOBJ_PIPE,
} kobj_type_t;

/*
 * Generic kernel object header.
 *
 * supported_ops is the kernel-known operation bitset that may be attempted on
 * this object. Per-handle rights are enforced separately by the capability
 * table.
 *
 * payload type depends on `type`:
 * - KOBJ_ENDPOINT -> kobj_endpoint_t *
 * - KOBJ_REMOTE   -> kobj_remote_t *
 * - other types   -> type-specific payload or NULL
 */
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
