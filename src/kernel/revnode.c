#include "mm/kheap.h"
#include <kernel/revnode.h>
#include <stdatomic.h>


/**
 * @brief Get allocated next child.
 *
 * @param[in] parent Pointer to parent.
 * @return allocated next child.
 */
static inline revnode_t *_revnode_get_next_child(revnode_t *parent) {
  revnode_t *r = parent->first_child;
  while (r != NULL) {
    r = r->next_sibling;
  }
  r = kzalloc(sizeof(*r));
  return r;
}

revnode_t *revnode_create(revnode_t *parent) {
  
  revnode_t *r = _revnode_get_next_child(parent);
  if (r == NULL) {
    return NULL;
  }

  r->refcnt = 1;
  r->parent = parent;
  r->first_child = NULL;
  r->next_sibling = NULL;
  atomic_store(&r->revoked, false);

  // bump parent refcnt;
  atomic_fetch_add(&parent->refcnt, 1);

  return r;
}

void revnode_put(revnode_t *node) {
  // TODO:
  (void)node;
}

bool revnode_is_descendant(revnode_t *child, revnode_t *parent) {
  // TODO:
  (void)child;
  (void)parent;
  return true;
}

