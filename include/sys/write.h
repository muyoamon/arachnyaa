#ifndef ARACHNYAA_SYS_WRITE_H_
#define ARACHNYAA_SYS_WRITE_H_

#include "kernel/cap.h"
#include "lib/stddef.h"
int sys_write(cap_handle_t handle, const void *user_buf, size_t len);

#endif // ARACHNYAA_SYS_WRITE_H_
