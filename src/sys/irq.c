#include "sys/irq.h"
#include "kernel/cap.h"
#include "kernel/error.h"
#include "kernel/ipc.h"
#include "kernel/kobj.h"
#include "kernel/spinlock.h"
#include "mm/kheap.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "process/thread.h"

typedef struct {
  uint8_t irq_num;
  spinlock_t lock;
  thread_t *waiting_thread;
  struct kobj *notify_ep;  /* endpoint to post notifications to */
} kobj_irq_t;

/* One slot per hardware IRQ line (IRQs 0–15 mapped to interrupts 32–47). */
static kobj_irq_t *irq_slots[16];

static void irq_kobj_release(kobj_t *obj) {
  if (!obj || !obj->payload) return;
  kobj_irq_t *slot = (kobj_irq_t *)obj->payload;
  if (slot->irq_num < 16) irq_slots[slot->irq_num] = NULL;
  if (slot->notify_ep) kobj_put(slot->notify_ep);
  kfree(slot);
  obj->payload = NULL;
}

static const kobj_ops_t irq_kobj_ops = { .release = irq_kobj_release };

cap_handle_t sys_irq_claim(uint32_t irq_num) {
  if (irq_num >= 16) return 0;
  if (irq_slots[irq_num]) return 0;  /* already claimed */

  process_t *proc = scheduler_get_current()->proc;

  kobj_irq_t *slot = kzalloc(sizeof(*slot));
  if (!slot) return 0;
  slot->irq_num = (uint8_t)irq_num;

  kobj_t *obj = kobj_create();
  if (!obj) {
    kfree(slot);
    return 0;
  }

  obj->type = KOBJ_IRQ;
  obj->ops = &irq_kobj_ops;
  obj->payload = slot;

  cap_rights_t rights = { .bits = R_IRQ_WAIT };
  cap_handle_t h = kcap_install_root(proc, obj, rights);
  kobj_put(obj);
  if (!h) return 0;

  irq_slots[irq_num] = slot;
  return h;
}

int sys_irq_wait(cap_handle_t irq_cap) {
  process_t *proc = scheduler_get_current()->proc;
  const cap_entry_t *e = cap_resolve(proc, irq_cap, R_IRQ_WAIT);
  if (!e) return KERR_INVAL;
  if (e->obj->type != KOBJ_IRQ) return KERR_PERM;

  kobj_irq_t *slot = (kobj_irq_t *)e->obj->payload;
  if (!slot) return KERR_INVAL;

  thread_t *current = scheduler_get_current();
  spin_lock(&slot->lock);
  slot->waiting_thread = current;
  spin_unlock(&slot->lock);

  current->state = T_BLOCKED;
  scheduler_reschedule();

  return KERR_OK;
}

void irq_cap_notify_data(uint32_t irq_num, uint8_t data) {
  if (irq_num >= 16) return;
  kobj_irq_t *slot = irq_slots[irq_num];
  if (!slot) return;

  spin_lock(&slot->lock);
  thread_t *waiter = slot->waiting_thread;
  slot->waiting_thread = NULL;
  struct kobj *notify_ep = slot->notify_ep;
  spin_unlock(&slot->lock);

  if (waiter) { waiter->state = T_READY; scheduler_add(waiter); }

  if (notify_ep && notify_ep->type == KOBJ_ENDPOINT) {
    kobj_endpoint_t *ep = (kobj_endpoint_t *)notify_ep->payload;
    spin_lock(&ep->lock);
    ep->pending_notify_mask |= (1u << irq_num);
    ep->notify_data[irq_num] = data;
    thread_t *ws = ep->waiting_server;
    if (ws) { ep->waiting_server = NULL; ws->state = T_READY; scheduler_add(ws); }
    spin_unlock(&ep->lock);
  }
}

void irq_cap_notify(uint32_t irq_num) { irq_cap_notify_data(irq_num, 0); }

int sys_irq_notify(cap_handle_t irq_cap, cap_handle_t ep_cap) {
  process_t *proc = scheduler_get_current()->proc;
  const cap_entry_t *ie = cap_resolve(proc, irq_cap, R_IRQ_WAIT);
  if (!ie || ie->obj->type != KOBJ_IRQ) return KERR_INVAL;
  kobj_irq_t *slot = (kobj_irq_t *)ie->obj->payload;
  if (!slot) return KERR_INVAL;

  const cap_entry_t *ee = cap_resolve(proc, ep_cap, 0);
  if (!ee || ee->obj->type != KOBJ_ENDPOINT) return KERR_INVAL;

  kobj_get(ee->obj);
  spin_lock(&slot->lock);
  if (slot->notify_ep) kobj_put(slot->notify_ep);
  slot->notify_ep = ee->obj;
  spin_unlock(&slot->lock);
  return KERR_OK;
}

bool irq_has_notify_ep(uint32_t irq_num) {
  if (irq_num >= 16) return false;
  kobj_irq_t *slot = irq_slots[irq_num];
  return slot && slot->notify_ep != NULL;
}
