#include "kernel/ipc.h"
#include "kernel/cap.h"
#include "kernel/error.h"
#include "kernel/kobj.h"
#include "kernel/protocol.h"
#include "lib/stddef.h"
#include "lib/string.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "process/thread.h"
#include "uapi/syscalls.h"
#include <stdint.h>
#include <sys/ipc.h>

static int ipc_msg_validate(const sys_ipc_msg_t *msg) {
  if (!msg) {
    return KERR_INVAL;
  }
  if (msg->num_bytes > sizeof(msg->data)) {
    return KERR_NOSPACE;
  }
  if (msg->num_handles > (sizeof(msg->handles) / sizeof(msg->handles[0]))) {
    return KERR_NOSPACE;
  }
  return KERR_OK;
}

// copy from user to kmsg
static void kmsg_cpy(const sys_ipc_msg_t *msg, ipc_kmsg_t *kmsg) {
  if (!msg || !kmsg) return;

  kmsg->num_handles = msg->num_handles;
  kmsg->num_bytes = msg->num_bytes;
  kmsg->flags = msg->flags;
  kmsg->opcode = msg->opcode;
  kmsg->object_id = msg->object_id;
  memcpy(&kmsg->handles, &msg->handles, sizeof(kmsg->handles));
  mempcpy(&kmsg->data, &msg->data, sizeof(msg->data));
}

// copy from kmsg to user msg
static void umsg_cpy(const ipc_kmsg_t *kmsg, sys_ipc_msg_t *umsg) {
  if (!kmsg || !umsg) return;

  umsg->num_handles = kmsg->num_handles;
  umsg->num_bytes = kmsg->num_bytes;
  umsg->flags = kmsg->flags;
  umsg->opcode = kmsg->opcode;
  umsg->object_id = kmsg->object_id;
  memcpy(&umsg->handles, &kmsg->handles, sizeof(umsg->handles));
  mempcpy(&umsg->data, &kmsg->data, sizeof(kmsg->data));
}

int sys_call(cap_handle_t handle, const sys_ipc_msg_t *msg, sys_ipc_msg_t *out) {
  int err = ipc_msg_validate(msg);
  if (err) {
    return err;
  }

  ipc_kmsg_t kmsg = {0};
  kmsg_cpy(msg, &kmsg);
  thread_t *thread = scheduler_get_current();
  process_t *proc = thread->proc;
  const cap_entry_t *cap = cap_resolve(proc, handle, R_EP_CALL);
  kobj_t *obj = NULL;
  uint64_t obj_id = 0;
  if (cap) {
    obj = cap->obj;
    if (obj->type != KOBJ_ENDPOINT) {
      return KERR_PERM;
    }
    kobj_get(obj);
  } else {
    cap = cap_resolve(proc, handle, KOP_CALL);
    if (!cap) {
      return KERR_INVAL;
    }

    obj = cap->obj;
    if (obj->type != KOBJ_REMOTE) {
      return KERR_PERM;
    }

    kobj_remote_t *remote = (kobj_remote_t *)obj->payload;
    if (!remote || !remote->endpoint) {
      return KERR_INVAL;
    }

    obj_id = remote->server_object_id;
    obj = remote->endpoint;
    kobj_get(obj);
  }


  ipc_kmsg_t reply = {0};

  kerror_t kerr = ipc_call(obj, thread, obj_id, &kmsg, &reply);
  if (kerr) {
    kobj_put(obj);
    return kerr;
  }

  err = ipc_msg_validate((const sys_ipc_msg_t *)&reply);
  if (err) {
    kobj_put(obj);
    return err;
  }

  if (out != NULL) {
    umsg_cpy(&reply, out);
  }
  kobj_put(obj);
  return KERR_OK;
}

int sys_recv(cap_handle_t endpoint_handle, sys_ipc_msg_t *out) {
  if (!out) {
    return KERR_INVAL;
  }

  thread_t *thread = scheduler_get_current();
  process_t *proc = thread->proc;
  const cap_entry_t *cap = cap_resolve(proc, endpoint_handle, R_EP_CALL);
  if (!cap) {
    return KERR_INVAL;
  }
  kobj_t *obj = cap->obj;
  if (obj->type != KOBJ_ENDPOINT) {
    return KERR_PERM;
  }

  ipc_call_t *call = NULL;
  kerror_t err = KERR_OK;
  do {
    err = ipc_recv_next(obj, thread, &call);
    if (err != KERR_NOTFOUND) {
      break;
    }
    thread->state = T_BLOCKED;
    scheduler_reschedule();
  } while (err == KERR_NOTFOUND);


  if (err != KERR_OK) {
    return err;
  }

  err = ipc_msg_validate((const sys_ipc_msg_t *)&call->request);
  if (err) {
    return err;
  }

  umsg_cpy(&call->request, out);

  return KERR_OK;
}

int sys_reply(sys_ipc_msg_t *reply) {
  int err = ipc_msg_validate(reply);
  if (err) {
    return err;
  }

  thread_t *server_thread = scheduler_get_current();
  ipc_call_t *call = server_thread->active_call;
  if (!call) {
    return KERR_INVAL;
  }

  ipc_kmsg_t kmsg = {0};
  kmsg_cpy(reply, &kmsg);

  /* Transfer capability handles from server's cap table to client's. */
  if (kmsg.num_handles > 0 && call->client_proc) {
    process_t *server = server_thread->proc;
    process_t *client = call->client_proc;
    for (uint32_t i = 0; i < kmsg.num_handles; i++) {
      if (kmsg.handles[i] == 0) continue;
      const cap_entry_t *e = cap_resolve(server, kmsg.handles[i], 0);
      if (!e) {
        kmsg.handles[i] = 0;
        continue;
      }
      cap_handle_t new_h = 0;
      if (kcap_transfer(server, client, kmsg.handles[i], e->rights.bits, &new_h) != 0) {
        kmsg.handles[i] = 0;
      } else {
        kmsg.handles[i] = new_h;
      }
    }
  }

  return ipc_reply_finish(call, &kmsg);
}
