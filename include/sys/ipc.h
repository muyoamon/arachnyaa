#ifndef ARACHNYAA_SYS_IPC_H_
#define ARACHNYAA_SYS_IPC_H_

#include "kernel/cap.h"
#include "uapi/syscalls.h"

int sys_call(cap_handle_t handle, const sys_ipc_msg_t *msg, sys_ipc_msg_t *out);

int sys_recv(cap_handle_t endpoint_handle, sys_ipc_msg_t *out);

int sys_reply(sys_ipc_msg_t *reply);

#endif // ARACHNYAA_SYS_IPC_H_
