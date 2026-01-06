#include "kernel/cap.h"
#include "kernel/kobj.h"
#include "kernel/revnode.h"
#include "kernel/error.h"
#include "kernel/spinlock.h"
#include "process/process.h"
#include "process/scheduler.h"
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

static int cap_alloc_slot(cap_table_t *ct, uint32_t *out_idx) {
  spin_lock(&ct->lock);
  uint32_t i = ct->free_head;
  if (i == 0xFFFFFFFF) {
    spin_unlock(&ct->lock);
    return KERR_NOMEM;
  }
  cap_entry_t *e = &ct->slots[i];
  ct->free_head = *(uint32_t *)&e->obj;
  memset(e, 0, sizeof(*e));
  spin_unlock(&ct->lock);
  *out_idx = i;
  return 0;
}

static void cap_free_slot(cap_table_t *ct, uint32_t idx) {
  cap_entry_t *e = &ct->slots[idx];
  e->gen++; // invalidate stale handles
  e->obj = NULL;
  // push to free list
  *(uint32_t *)&e->obj = ct->free_head;
  ct->free_head = idx;
}

static int cap_validate(process_t *p, cap_handle_t h, kobj_type_t want_type,
                        uint32_t need_bits, const cap_entry_t **out) {
  uint32_t idx = (uint32_t)(h & 0xFFFFFFFFu);
  uint32_t gen = (uint32_t)((h >> 32) & 0x00FFFFFFu);
  uint32_t typ = (uint32_t)((h >> 56) & 0xFFu);
  if (idx >= p->caps.cap_count)
    return KERR_INVAL;

  const cap_entry_t *e = &p->caps.slots[idx];
  if (e->obj == NULL)
    return KERR_INVAL;
  if (e->gen != gen)
    return KERR_STALE;
  if (typ != e->type)
    return KERR_INVAL;
  if (want_type && e->type != want_type)
    return KERR_PERM;
  if ((e->rights.bits & need_bits) != need_bits)
    return KERR_ACCESS;
  if (e->rnode && atomic_load(&e->rnode->revoked))
    return KERR_ACCESS;
  *out = e;
  return 0;
}

int kcap_derive(process_t *p, cap_handle_t parent_h, uint32_t bits,
                uint64_t off, uint64_t len, uint32_t flags,
                cap_handle_t *out_h) {
  const cap_entry_t *p_entry;
  int err = cap_validate(p, parent_h, 0, 0, &p_entry);
  if (err)
    return err;

  if ((bits & p_entry->rights.bits) != bits)
    return KERR_ACCESS;
  cap_rights_t r = {.bits = bits, .flags = flags};
  if (flags & CAPF_HAS_RANGE) {
    if (!(p_entry->rights.flags & CAPF_HAS_RANGE)) {
      // parent unlimited range -> ok
    } else {
      uint64_t pend = p_entry->rights.off + p_entry->rights.len;
      uint64_t cend = off + len;
      if (off < p_entry->rights.off || cend > pend)
        return KERR_RANGE;
    }
    r.off = off;
    r.len = len;
  }

  // require explicit derivation right for some types 
  if (p_entry->type == KOBJ_VMOBJ && !(p_entry->rights.bits & R_VM_DERIVE))
    return KERR_ACCESS;

  // create child revnode
  revnode_t *child = revnode_create(p_entry->rnode);

  // install new entry;
  uint32_t idx;
  err = cap_alloc_slot(&p->caps, &idx);
  if (err) {
    revnode_put(child);
    return err;
  }
  cap_entry_t *e = &p->caps.slots[idx];
  e->obj = p_entry->obj;
  e->rights = r;
  e->rnode = child;
  e->type = p_entry->type;
  e->gen = p_entry->gen + 1;
  
  *out_h = ((uint64_t)e->type << 56) | ((uint64_t)e->gen << 32) | idx;
  return 0;
}

int kcap_revoke(process_t *p, cap_handle_t h) {
  const cap_entry_t *e;
  int err = cap_validate(p, h, 0, 0, &e);
  if (err) return err;

  // mark subtree revoked once.
  bool expected=false;
  if (!atomic_compare_exchange_strong(&e->rnode->revoked, &expected, true))
    return 0; // already revoked

  FOR_EACH_PROC(other) {
    cap_table_t *ct = &other->caps;
    spin_lock(&ct->lock);
    for (uint32_t i = 0; i < ct->cap_count; i++) {
      cap_entry_t *ce = &ct->slots[i];
      if (!ce->obj || !ce->rnode) continue;
      if (revnode_is_descendant(ce->rnode, e->rnode)) {
        // drop reference
        kobj_put(ce->obj);
        revnode_put(ce->rnode);
        cap_free_slot(ct, i);
      }
    }
    spin_unlock(&ct->lock);
  } 
  return 0;
}

cap_handle_t sys_cap_derive(cap_sys_arg_t *arg) {
  process_t *p = scheduler_get_current()->proc;
  cap_handle_t out_h;
  if (kcap_derive(p, arg->handle, arg->rights_bits, arg->off, arg->len, arg->flags, &out_h)) {
    return 0u;
  }
  return out_h;
}

int sys_cap_revoke(cap_handle_t h) {
  process_t *p = scheduler_get_current()->proc;
  return kcap_revoke(p, h);
}

const cap_entry_t *cap_resolve(struct process *p, cap_handle_t h, uint32_t rights) {
  const cap_entry_t *entry = NULL;
  int err = cap_validate(p, h, 0, rights, &entry);
  if (err) {
    return NULL;
  }
  return entry;
}

