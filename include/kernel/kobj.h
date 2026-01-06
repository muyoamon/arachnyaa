#ifndef ARACHNYAA_KERNEL_KOBJ_H_
#define ARACHNYAA_KERNEL_KOBJ_H_

#include <stdatomic.h>

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
  void *ops;
  void *payload;
} kobj_t;


kobj_t *kobj_create(void);

void kobj_put(kobj_t *kobj);

void kobj_init(void);


#endif // ARACHNYAA_KERNEL_KOBJ_H_
