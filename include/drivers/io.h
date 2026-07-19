#ifndef _ARACHNYAA_IO_H
#define _ARACHNYAA_IO_H

#include <stdint.h>

// Writes a byte to an I/O port.
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Reads a byte from an I/O port.
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Read `count` 16-bit words from an I/O port into buf (REP INSW).
static inline void insw(uint16_t port, void *buf, uint32_t count) {
    asm volatile ("cld; rep insw"
                  : "+D"(buf), "+c"(count) : "d"(port) : "memory");
}

// Write `count` 16-bit words from buf to an I/O port (REP OUTSW).
static inline void outsw(uint16_t port, const void *buf, uint32_t count) {
    asm volatile ("cld; rep outsw"
                  : "+S"(buf), "+c"(count) : "d"(port) : "memory");
}

// create small I/O delay
void io_wait(void);



#endif // _ARACHNYAA_IO_H
