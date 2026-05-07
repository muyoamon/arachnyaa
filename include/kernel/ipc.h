#ifndef ARACHNYAA_KERNEL_IPC_H_
#define ARACHNYAA_KERNEL_IPC_H_

#include "kernel/cap.h"
#include "kernel/error.h"
#include "kernel/protocol.h"
#include "kernel/spinlock.h"
#include <lib/stddef.h>
#include <stdint.h>

struct process;
struct thread;
struct kobj;

#define IPC_INLINE_BYTES 256
#define IPC_MAX_HANDLES 4

typedef enum {
  IPC_CALL_QUEUED = 0,
  IPC_CALL_ACTIVE,
  IPC_CALL_REPLIED,
  IPC_CALL_CANCELLED,
} ipc_call_state_t;

/*
 * Kernel-owned copied message buffer.
 * The kernel copies request/reply bytes into this structure so it does not
 * depend on userspace memory remaining valid while threads block.
 */
typedef struct {
  uint32_t opcode;
  uint32_t flags;
  uint32_t num_bytes;
  uint32_t num_handles;
  uint64_t object_id;
  uint8_t data[IPC_INLINE_BYTES];
  cap_handle_t handles[IPC_MAX_HANDLES];
} ipc_kmsg_t;

/*
 * One in-flight synchronous call.
 * Owned by the kernel while client/server rendezvous completes.
 */
typedef struct ipc_call {
  uint64_t call_id;
  ipc_call_state_t state;

  struct thread *client_thread;
  struct process *client_proc;

  struct thread *server_thread;
  struct process *server_proc;

  struct kobj *endpoint;

  /*
   * For remote-object operations, the kernel includes the server-owned object
   * id so the server can identify which object the operation targets. For
   * endpoint OPEN requests, this can be zero.
   */
  uint64_t server_object_id;

  ipc_kmsg_t request;
  ipc_kmsg_t reply;

  struct ipc_call *next;
} ipc_call_t;

/*
 * Endpoint wait/queue state.
 * This becomes the payload of KOBJ_ENDPOINT.
 */
typedef struct {
  spinlock_t lock;
  struct process *owner_proc;

  ipc_call_t *queue_head;
  ipc_call_t *queue_tail;

  struct thread *waiting_server;
} kobj_endpoint_t;

/*
 * Kernel-side handle for a userspace-owned remote object.
 * This becomes the payload of KOBJ_REMOTE.
 */
typedef struct {
  struct kobj *endpoint;
  uint64_t server_object_id;
  uint32_t allowed_ops;
} kobj_remote_t;

/* Endpoint lifecycle */
int ipc_endpoint_init(kobj_endpoint_t *ep, struct process *owner_proc);
void ipc_endpoint_destroy(kobj_endpoint_t *ep);

/* Client path */
kerror_t ipc_call_enqueue(struct kobj *endpoint, struct thread *client,
                          uint64_t server_object_id, const ipc_kmsg_t *request,
                          ipc_call_t **out_call);

/* Server path */
kerror_t ipc_recv_next(struct kobj *endpoint, struct thread *server,
                       ipc_call_t **out_call);

/* Reply path */
kerror_t ipc_reply_finish(ipc_call_t *call, const ipc_kmsg_t *reply);

/* Call object lifecycle */
ipc_call_t *ipc_call_create(void);
void ipc_call_destroy(ipc_call_t *call);

/* Wrapper */
kerror_t ipc_call(struct kobj *endpoint, struct thread *client,
                  uint64_t server_obj_id, const ipc_kmsg_t *req,
                  ipc_kmsg_t *reply);

/* Helper */
cap_handle_t ipc_install_remote_handle(struct process *proc, kobj_t *endpoint,
                                       uint64_t object_id,
                                       uint32_t allowed_ops);

#endif // ARACHNYAA_KERNEL_IPC_H_
