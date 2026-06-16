#include "kernel/cap.h"
#include "kernel/error.h"
#include "kernel/kobj.h"
#include "kernel/spinlock.h"
#include "mm/kheap.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "process/thread.h"
#include <kernel/ipc.h>

static uint32_t next_call_id() {
  static atomic_uint g;
  uint32_t v = atomic_fetch_add(&g, 1) + 1;
  return v & 0x00FFFFFFu;
}

/* Endpoint lifecycle */
int ipc_endpoint_init(kobj_endpoint_t *ep, struct process *owner_proc) {
  if (!ep || !owner_proc) {
    return KERR_INVAL;
  }

  ep->lock.locked = 0;
  ep->owner_proc = owner_proc;
  ep->queue_head = NULL;
  ep->queue_tail = NULL;
  ep->waiting_server = NULL;
  return KERR_OK;
}

void ipc_endpoint_destroy(kobj_endpoint_t *ep) {
  if (!ep) {
    return;
  }
  uint32_t flags;
  spin_lock_irqsave(&ep->lock, &flags);
  ipc_call_t *call = ep->queue_head;
  thread_t *waiting_server = ep->waiting_server;
  ep->queue_head = NULL;
  ep->queue_tail = NULL;
  ep->waiting_server = NULL;
  spin_unlock_irqrestore(&ep->lock, flags);

  if (waiting_server && waiting_server->state == T_BLOCKED) {
    waiting_server->state = T_READY;
    scheduler_add(waiting_server);
  }

  while (call) {
    ipc_call_t *next = call->next;
    call->state = IPC_CALL_CANCELLED;
    if (call->client_thread && call->client_thread->state == T_BLOCKED) {
      call->client_thread->state = T_READY;
      scheduler_add(call->client_thread);
    }
    ipc_call_destroy(call);
    call = next;
  }
}

/* Client path */
kerror_t ipc_call_enqueue(struct kobj *endpoint, struct thread *client,
                          uint64_t server_object_id, const ipc_kmsg_t *request,
                          ipc_call_t **out_call) {
  if (!endpoint || !client || !request || !out_call) {
    return KERR_INVAL;
  }
  if (endpoint->type != KOBJ_ENDPOINT) {
    return KERR_PERM;
  }

  kobj_endpoint_t *ep = endpoint->payload;
  if (!ep) {
    return KERR_INVAL;
  }

  ipc_call_t *call = ipc_call_create();
  if (!call) {
    return KERR_NOMEM;
  }

  call->client_thread = client;
  call->client_proc = client->proc;
  call->server_proc = ep->owner_proc;
  call->endpoint = endpoint;
  call->server_object_id = server_object_id;
  call->request = *request;
  
  call->request.object_id = server_object_id;

  kobj_get(endpoint);

  uint32_t flags;
  spin_lock_irqsave(&ep->lock, &flags);

  if (ep->queue_tail) {
    ep->queue_tail->next = call;
  } else {
    ep->queue_head = call;
  }
  ep->queue_tail = call;

  /* Set client blocked before unlock so ipc_reply_finish can't miss it. */
  client->state = T_BLOCKED;

  thread_t *server_to_wake = NULL;
  if (ep->waiting_server) {
    server_to_wake = ep->waiting_server;
    server_to_wake->state = T_READY;
    ep->waiting_server = NULL;
  }

  spin_unlock_irqrestore(&ep->lock, flags);

  /* Add to run queue outside the spinlock (scheduler_add uses crit_enter/exit). */
  if (server_to_wake)
    scheduler_add(server_to_wake);

  *out_call = call;
  return KERR_OK;
}

/* Server path */
kerror_t ipc_recv_next(struct kobj *endpoint, struct thread *server,
                       ipc_call_t **out_call) {
  if (!endpoint || !server || !out_call) {
    return KERR_INVAL;
  }
  if (endpoint->type != KOBJ_ENDPOINT) {
    return KERR_PERM;
  }

  kobj_endpoint_t *ep = endpoint->payload;
  if (!ep) {
    return KERR_INVAL;
  }

  uint32_t flags;
  spin_lock_irqsave(&ep->lock, &flags);

  if (ep->pending_notify_mask) {
    uint32_t bit = ep->pending_notify_mask & (-(int32_t)ep->pending_notify_mask);
    uint32_t irq_num = (uint32_t)__builtin_ctz(ep->pending_notify_mask);
    uint8_t head = ep->notify_head[irq_num];
    uint8_t data = ep->notify_ring[irq_num][head];
    ep->notify_head[irq_num] = (uint8_t)((head + 1u) % NOTIFY_RING_SIZE);
    if (ep->notify_head[irq_num] == ep->notify_tail[irq_num])
      ep->pending_notify_mask &= ~bit; /* clear mask only when ring is empty */
    spin_unlock_irqrestore(&ep->lock, flags);

    ipc_call_t *ncall = ipc_call_create();
    if (!ncall) return KERR_NOMEM;
    ncall->is_notification = true;
    ncall->state = IPC_CALL_ACTIVE;
    ncall->server_thread = server;
    ncall->server_proc = server->proc;
    ncall->request.opcode = IPC_OP_NOTIFY;
    ncall->request.object_id = irq_num;
    ncall->request.data[0] = data;
    ncall->request.num_bytes = 1;
    server->active_call = ncall;
    *out_call = ncall;
    return KERR_OK;
  }

  ipc_call_t *call = ep->queue_head;
  if (!call) {
    ep->waiting_server = server;
    /* Set blocked before unlock so ipc_call_enqueue can't set T_READY and
       then have us overwrite it with T_BLOCKED after the unlock. */
    server->state = T_BLOCKED;
    spin_unlock_irqrestore(&ep->lock, flags);

    *out_call = NULL;
    return KERR_NOTFOUND;
  }

  ep->queue_head = call->next;
  if (!ep->queue_head) {
    ep->queue_tail = NULL;
  }
  call->next = NULL;
  call->state = IPC_CALL_ACTIVE;
  call->server_thread = server;
  call->server_proc = server->proc;
  server->active_call = call;

  spin_unlock_irqrestore(&ep->lock, flags);

  *out_call = call;
  return KERR_OK;
}

/* Reply path */
kerror_t ipc_reply_finish(ipc_call_t *call, const ipc_kmsg_t *reply) {
  if (!call || !reply) {
    return KERR_INVAL;
  }
  if (call->state != IPC_CALL_ACTIVE) {
    return KERR_BUSY;
  }

  call->reply = *reply;
  call->state = IPC_CALL_REPLIED;

  if (call->client_thread && call->client_thread->state == T_BLOCKED) {
    call->client_thread->state = T_READY;
    scheduler_add(call->client_thread);
  } else if (!call->client_thread) {
    /* Notification call: no client, free immediately after clearing server state */
    if (call->server_thread) call->server_thread->active_call = NULL;
    ipc_call_destroy(call);
    return KERR_OK;
  }

  if (call->server_thread) {
    call->server_thread->active_call = NULL;
  }

  return KERR_OK;
}

/* Call object lifecycle */
ipc_call_t *ipc_call_create(void) {
  ipc_call_t *call = (ipc_call_t *)kzalloc(sizeof(*call));
  if (!call) {
    return NULL;
  }

  call->state = IPC_CALL_QUEUED;
  call->next = NULL;
  call->call_id = next_call_id();
  return call;
}

void ipc_call_destroy(ipc_call_t *call) {
  /* v1: data is inline */
  if (!call) {
    return;
  }
  if (call->endpoint) {
    kobj_put(call->endpoint);
  }
  kfree(call);
}

/* Call Wrapper*/
kerror_t ipc_call(struct kobj *endpoint, struct thread *client,
                  uint64_t server_obj_id, const ipc_kmsg_t *req,
                  ipc_kmsg_t *reply) {
  if (!reply) {
    return KERR_INVAL;
  }

  ipc_call_t *call = NULL;
  kerror_t err = ipc_call_enqueue(endpoint, client, server_obj_id, req, &call);
  if (err) {
    return err;
  }

  scheduler_reschedule();

  if (call->state == IPC_CALL_CANCELLED) {
    ipc_call_destroy(call);
    return KERR_INTERRUPTED;
  }

  if (call->state != IPC_CALL_REPLIED) {
    ipc_call_destroy(call);
    return KERR_INTERRUPTED;
  }
  *reply = call->reply;
  ipc_call_destroy(call);
  return KERR_OK;
}

static void _release_remote (struct kobj *obj) {
  if (!obj || !obj->payload) {
    return;
  }

  kobj_remote_t *payload = obj->payload;

  kobj_put(payload->endpoint);
  kfree(payload);
  obj->payload = NULL;
}

cap_handle_t ipc_install_remote_handle(struct process *proc, kobj_t *endpoint,
                                       uint64_t object_id,
                                       uint32_t allowed_ops) {
  if (!proc || !endpoint || endpoint->type != KOBJ_ENDPOINT || object_id == 0 ||
      allowed_ops == 0) {
    return 0;
  }

  kobj_t *obj = kobj_create();
  if (!obj) {
    return 0;
  }

  kobj_remote_t *payload = kzalloc(sizeof(kobj_remote_t));
  if (!payload) {
    kobj_put(obj);
    return 0;
  }

  kobj_get(endpoint);

  obj->type = KOBJ_REMOTE;

  payload->endpoint = endpoint;
  payload->server_object_id = object_id;
  payload->allowed_ops = allowed_ops;

  obj->payload = payload;
  
  static const kobj_ops_t ops = {
    .release = _release_remote
  };

  obj->ops = &ops;
  obj->supported_ops = allowed_ops;

  cap_rights_t rights  = {
    .bits = allowed_ops,
    .flags = 0,
    .len = 0,
    .off = 0
  };

  
  cap_handle_t h = kcap_install_root(proc, obj, rights);
  if (!h) {
    kobj_put(obj);
    return 0;
  }

  kobj_put(obj);
  return h;
}
