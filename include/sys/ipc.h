#ifndef ARACHNYAA_SYS_IPC_H_
#define ARACHNYAA_SYS_IPC_H_

#include "kernel/cap.h"
#include "uapi/syscalls.h"
#include <stdint.h>

int sys_call(cap_handle_t handle, const sys_ipc_msg_t *msg, sys_ipc_msg_t *out);

int sys_recv(cap_handle_t endpoint_handle, sys_ipc_msg_t *out);

int sys_reply(sys_ipc_msg_t *reply);

uint32_t sys_defer_call(void);

int sys_reply_to(uint32_t token, const sys_ipc_msg_t *msg);

#endif // ARACHNYAA_SYS_IPC_H_
