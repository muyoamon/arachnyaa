#ifndef ARACHNYAA_UAPI_SYSCALLS_H_
#define ARACHNYAA_UAPI_SYSCALLS_H_

#include <stdint.h>

typedef int32_t sysret_t;
typedef uint32_t cap_t;
typedef uint32_t objtype_t;

enum {
  SYS_cap_create = 1,
  SYS_cap_dup,
  SYS_cap_move,
  SYS_cap_revoke,
  SYS_vm_map,
  SYS_vm_unmap,
  SYS_frame_alloc,
  SYS_thread_create,
  SYS_thread_start,
  SYS_thread_yield,
  SYS_ipc_call,
  SYS_ipc_send,
  SYS_ipc_recv,
  SYS_time_mono_ns,
  SYS_futex_wait,
  SYS_futex_wake,
  SYS_srvc_create,
  SYS_srvc_load,
  SYS_srvc_close,
};

enum {
  OBJ_Task = 1,
  OBJ_Thread,
  OBJ_Endpoint,
  OBJ_VmSpace,
  OBJ_Frame,
  OBJ_Timer,
};

typedef struct {
  uint32_t op;
  uint32_t a0, a1, a2;
} sys_args_t;

typedef struct {
  uint32_t opcode;
  uint32_t flags;
  uint32_t num_bytes;
  uint32_t num_handles;
  uint64_t object_id;
  uint8_t data[256];
  uint64_t handles[4];
} sys_ipc_msg_t;

typedef struct {
  uint32_t allowed_ops;
} sys_open_reply_t;

enum {
  VM_R = 1 << 0,
  VM_W = 1 << 1,
  VM_X = 1 << 3,
  VM_G = 1 << 4,
};

#endif // ARACHNYAA_UAPI_SYSCALLS_H_
