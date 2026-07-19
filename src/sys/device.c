#include "sys/device.h"
#include "drivers/io.h"

uint32_t sys_io_in(uint32_t port) {
  return (uint32_t)inb((uint16_t)port);
}

int sys_io_out(uint32_t port, uint32_t val) {
  outb((uint16_t)port, (uint8_t)val);
  return 0;
}

/* Kernel occupies [0xC0000000, 4GB); user buffers must live below it. */
#define USER_LIMIT 0xC0000000u

/* Bulk 16-bit port IO (REP INSW/OUTSW) into/from a user buffer.  Runs with the
 * calling process's address space active, so the user pointer is dereferenced
 * directly — validate it stays within user space first. */
uint32_t sys_io_insw(uint32_t port, void *buf, uint32_t count) {
  uintptr_t b = (uintptr_t)buf;
  if (count > 0x10000u || b >= USER_LIMIT ||
      b + (uintptr_t)count * 2u > USER_LIMIT)
    return (uint32_t)-1;
  insw((uint16_t)port, buf, count);
  return 0;
}

int sys_io_outsw(uint32_t port, const void *buf, uint32_t count) {
  uintptr_t b = (uintptr_t)buf;
  if (count > 0x10000u || b >= USER_LIMIT ||
      b + (uintptr_t)count * 2u > USER_LIMIT)
    return -1;
  outsw((uint16_t)port, buf, count);
  return 0;
}
