#ifndef ARACHNYAA_SYS_NAMESPACE_H_
#define ARACHNYAA_SYS_NAMESPACE_H_

#include "kernel/cap.h"
#include <stdint.h>

int sys_ns_bind(const char *protocol, cap_handle_t handler, uint32_t declared_ops);
cap_handle_t sys_open(const char *name, uint32_t flags);

#endif // ARACHNYAA_SYS_NAMESPACE_H_
