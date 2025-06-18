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

// create small I/O delay
void io_wait(void);



#endif // _ARACHNYAA_IO_H
