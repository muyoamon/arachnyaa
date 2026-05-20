#include "kernel/cap.h"
#include "sys/cap.h"
#include "kernel/kobj.h"
#include "kernel/revnode.h"
#include "kernel/error.h"
#include "kernel/spinlock.h"
#include "mm/kheap.h"
#include "process/process.h"
#include "process/scheduler.h"
#include <stdatomic.h>
#include <stdint.h>
#include "lib/string.h"

static const uint32_t cap_invalid_index = 0xFFFFFFFFu;
static uint32_t cap_next_gen(void);

static int cap_alloc_slot(cap_table_t *ct, uint32_t *out_idx) {
  spin_lock(&ct->lock);
  uint32_t i = ct->free_head;
  if (i == cap_invalid_index) {
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
  if (e->obj) {
    kobj_put(e->obj);
  }
  if (e->rnode) {
    revnode_put(e->rnode);
  }
  e->obj = NULL;
  e->rnode = NULL;
  e->rights.bits = 0;
  e->rights.flags = 0;
  e->rights.off = 0;
  e->rights.len = 0;
  e->type = 0;
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

static inline cap_handle_t cap_make_handle(uint32_t idx, uint32_t gen, uint8_t type) {
  return ((uint64_t)type << 56) | ((uint64_t)gen << 32) | idx;
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
  if (!child) {
    return KERR_NOMEM;
  }

  // install new entry;
  uint32_t idx;
  err = cap_alloc_slot(&p->caps, &idx);
  if (err) {
    revnode_put(child);
    return err;
  }
  cap_entry_t *e = &p->caps.slots[idx];
  e->obj = p_entry->obj;
  kobj_get(e->obj);
  e->rights = r;
  e->rnode = child;
  e->type = p_entry->type;
  e->gen = cap_next_gen();
  
  *out_h = cap_make_handle(idx, e->gen, e->type);
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

static int cap_validate_mut(process_t *p, cap_handle_t h, uint32_t need_bits,
                            uint32_t *out_idx) {
  const cap_entry_t *entry = NULL;
  int err = cap_validate(p, h, 0, need_bits, &entry);
  if (err) {
    return err;
  }
  *out_idx = (uint32_t)(h & 0xFFFFFFFFu);
  return 0;
}

void cap_table_init(cap_table_t *ct, uint32_t capacity) {
  memset(ct, 0, sizeof(*ct));
  ct->slots = kcalloc(capacity, sizeof(cap_entry_t));
  ct->cap_count = capacity;
  
  ct->free_head = 0;
  for (uint32_t i = 0; i < capacity; i++) {
    cap_entry_t *e = &ct->slots[i];
    e->obj = NULL;
    e->gen = 1;
    uint32_t next = (i + 1 < capacity) ? (i + 1) : cap_invalid_index;
    *(uint32_t*)&e->obj = next;
  }
}

void cap_table_destroy(cap_table_t *ct) {
  if (!ct || !ct->slots) {
    return;
  }
  for (uint32_t i = 0; i < ct->cap_count; i++) {
    cap_entry_t *e = &ct->slots[i];
    if (!e->obj || !e->rnode) {
      continue;
    }
    kobj_put(e->obj);
    revnode_put(e->rnode);
  }
  kfree(ct->slots);
  memset(ct, 0, sizeof(*ct));
}

static uint32_t cap_next_gen(void) {
  static atomic_uint g;
  uint32_t v = atomic_fetch_add(&g, 1) + 1;
  return v & 0x00FFFFFFu;
}

static revnode_t *revnode_root_new(void) {
  revnode_t *r = kzalloc(sizeof(*r));
  if (!r) {
    return NULL;
  }
  r->refcnt = 1;
  r->lock.locked = 0;
  r->parent = NULL;
  r->first_child = NULL;
  r->next_sibling = NULL;
  atomic_store(&r->revoked, false);
  return r;
}

cap_handle_t kcap_install_root(process_t *p, kobj_t *obj, cap_rights_t rights) {
  
  cap_table_t *ct = &p->caps;
  uint32_t idx;
  if (cap_alloc_slot(ct, &idx) < 0) 
    return 0; // invalid handle

  cap_entry_t *e = &ct->slots[idx];
  revnode_t *root = revnode_root_new();
  if (!root) {
    spin_lock(&ct->lock);
    *(uint32_t *)&e->obj = ct->free_head;
    ct->free_head = idx;
    spin_unlock(&ct->lock);
    return 0;
  }

  e->obj = obj;
  kobj_get(obj);

  e->rights = rights;
  e->rnode = root;
  e->type = obj->type;
  e->gen = cap_next_gen();

  // Encode into 64-bit handle
  cap_handle_t h = cap_make_handle(idx, e->gen, e->type);
  return h;
}

int sys_cap_close(cap_handle_t h) {
  process_t *p = scheduler_get_current()->proc;
  uint32_t idx = 0;
  int err = cap_validate_mut(p, h, 0, &idx);
  if (err) {
    return err;
  }

  cap_table_t *ct = &p->caps;
  spin_lock(&ct->lock);
  cap_entry_t *e = &ct->slots[idx];
  if (!e->obj || e->gen != (uint32_t)((h >> 32) & 0x00FFFFFFu)) {
    spin_unlock(&ct->lock);
    return KERR_STALE;
  }
  cap_free_slot(ct, idx);
  spin_unlock(&ct->lock);
  return 0;
}

int kcap_transfer(struct process *src, struct process *dst, cap_handle_t handle,
                  uint32_t rights_bits) {
  const cap_entry_t *src_entry = NULL;
  int err = cap_validate(src, handle, 0, rights_bits, &src_entry);
  if (err) {
    return err;
  }
  revnode_t *child = revnode_create(src_entry->rnode);
  if (!child) {
    return KERR_NOMEM;
  }

  uint32_t idx = 0;
  err = cap_alloc_slot(&dst->caps, &idx);
  if (err) {
    revnode_put(child);
    return err;
  }

  cap_entry_t *e = &dst->caps.slots[idx];
  e->obj = src_entry->obj;
  kobj_get(e->obj);
  e->rights = src_entry->rights;
  e->rights.bits = rights_bits;
  e->rnode = child;
  e->gen = cap_next_gen();
  e->type = src_entry->type;
  return 0;
}

process_t *process_find_by_pid(pid_t pid) {
  FOR_EACH_PROC(proc) {
    if (proc->pid == pid) {
      return proc;
    }
  }
  return NULL;
}

int sys_cap_transfer(cap_handle_t dst_cap, cap_sys_arg_t *arg) {
  if (!arg) {
    return KERR_INVAL;
  }
  const cap_entry_t *dst_entry;

  process_t *src = scheduler_get_current()->proc;
  
  if (cap_validate(src, dst_cap, KOBJ_PROC, R_PROC_TRANSFER, &dst_entry)) {
    return KERR_INVAL;
  }

  process_t *dst = dst_entry->obj->payload;
  if (!dst) {
    return KERR_NOTFOUND;
  }
  return kcap_transfer(src, dst, arg->handle, arg->rights_bits);
}
