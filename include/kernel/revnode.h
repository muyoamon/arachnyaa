#ifndef ARACHNYAA_KERNEL_REVNODE_H_
#define ARACHNYAA_KERNEL_REVNODE_H_


#include "kernel/spinlock.h"
#include <stdatomic.h>
#include <stdbool.h>

typedef struct revnode {
  atomic_uint refcnt;
  spinlock_t lock;
  struct revnode *parent;
  struct revnode *first_child;
  struct revnode *next_sibling;
  atomic_bool revoked;
} revnode_t;

revnode_t *revnode_create(revnode_t *parent);

void revnode_put(revnode_t *node);

bool revnode_is_descendant(revnode_t *child, revnode_t *parent);

#endif // ARACHNYAA_KERNEL_REVNODE_H_
