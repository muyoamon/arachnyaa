#include "mm/kheap.h"
#include <kernel/revnode.h>
#include <lib/string.h>
#include <stdatomic.h>


/**
 * @brief Get allocated next child.
 *
 * @param[in] parent Pointer to parent.
 * @return allocated next child.
 */
static inline revnode_t *_revnode_get_next_child(revnode_t *parent) {
  revnode_t *r = kzalloc(sizeof(*r));
  if (!r) {
    return NULL;
  }
  spin_lock(&parent->lock);
  r->next_sibling = parent->first_child;
  parent->first_child = r;
  spin_unlock(&parent->lock);
  return r;
}

revnode_t *revnode_create(revnode_t *parent) {
  
  revnode_t *r = _revnode_get_next_child(parent);
  if (r == NULL) {
    return NULL;
  }

  r->refcnt = 1;
  r->lock.locked = 0;
  r->parent = parent;
  r->first_child = NULL;
  atomic_store(&r->revoked, false);

  // bump parent refcnt;
  atomic_fetch_add(&parent->refcnt, 1);

  return r;
}

void revnode_put(revnode_t *node) {
  if (!node) {
    return;
  }
  if (atomic_fetch_sub(&node->refcnt, 1) != 1) {
    return;
  }

  revnode_t *parent = node->parent;
  if (parent) {
    spin_lock(&parent->lock);
    revnode_t **cursor = &parent->first_child;
    while (*cursor) {
      if (*cursor == node) {
        *cursor = node->next_sibling;
        break;
      }
      cursor = &(*cursor)->next_sibling;
    }
    spin_unlock(&parent->lock);
    revnode_put(parent);
  }
  kfree(node);
}

bool revnode_is_descendant(revnode_t *child, revnode_t *parent) {
  for (revnode_t *cur = child; cur != NULL; cur = cur->parent) {
    if (cur == parent) {
      return true;
    }
  }
  return false;
}
