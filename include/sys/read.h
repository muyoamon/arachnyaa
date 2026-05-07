#ifndef ARACHNYAA_SYS_READ_H_
#define ARACHNYAA_SYS_READ_H_


#include "kernel/cap.h"
int sys_read(cap_handle_t handle, void* buf, size_t nbytes);

#endif // ARACHNYAA_SYS_READ_H_
