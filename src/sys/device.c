#include "sys/device.h"
#include "drivers/io.h"

uint32_t sys_io_in(uint32_t port) {
  return (uint32_t)inb((uint16_t)port);
}

int sys_io_out(uint32_t port, uint32_t val) {
  outb((uint16_t)port, (uint8_t)val);
  return 0;
}
