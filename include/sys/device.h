#ifndef ARACHNYAA_SYS_DEVICE_H_
#define ARACHNYAA_SYS_DEVICE_H_

#include <stdint.h>

uint32_t sys_io_in(uint32_t port);

int sys_io_out(uint32_t port, uint32_t val);

uint32_t sys_io_insw(uint32_t port, void *buf, uint32_t count);

int sys_io_outsw(uint32_t port, const void *buf, uint32_t count);

#endif // ARACHNYAA_SYS_DEVICE_H_
