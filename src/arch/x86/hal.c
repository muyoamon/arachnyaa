// hal.c - Hardware Abstraction Layer for Arachnyaa (x86)

#include <time.h>
#include <stddef.h>
#include <stdint.h>
#include <io.h>




// --- IO ---
void io_wait() {
  outb(0x80, 0);
}

// --- VGA Text Mode ---

volatile uint16_t* vga_buffer = (uint16_t*)0xB8000;
const int VGA_WIDTH = 80;
const int VGA_HEIGHT = 25;
int tty_row = 0;
int tty_col = 0;
uint8_t tty_color = 0x0F; // White on black

// Function to create a VGA character
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

// Simple internal print char (doesn't handle scrolling yet)
void hal_tty_putc(char c) {
    if (c == '\n') {
        tty_col = 0;
        tty_row++;
    } else {
        const size_t index = tty_row * VGA_WIDTH + tty_col;
        vga_buffer[index] = vga_entry(c, tty_color);
        tty_col++;
    }

    // Wrap around / scroll (very basic)
    if (tty_col >= VGA_WIDTH) {
        tty_col = 0;
        tty_row++;
    }
    if (tty_row >= VGA_HEIGHT) {
        // Clear screen for now (TODO: implement scrolling)
        for(int i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
            vga_buffer[i] = vga_entry(' ', tty_color);
        }
        tty_row = 0;
        tty_col = 0;
    }
    // TODO: Update cursor position via ports 0x3D4/0x3D5
}

// Simple print string
void hal_tty_print(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        hal_tty_putc(str[i]);
    }
}

// Simple hex print
void hal_tty_print_hex(uint32_t n) {
    char buf[9];
    char hex[] = "0123456789ABCDEF";
    buf[8] = '\0';
    for (int i = 7; i >= 0; --i) {
        buf[i] = hex[n & 0xF];
        n >>= 4;
    }
    hal_tty_print("0x");
    hal_tty_print(buf);
}


// --- PIC (Programmable Interrupt Controller) ---
// We'll need these later for hardware interrupts (keyboard, timer)
// For now, they can be stubs or implemented but unused.

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI   0x20

#define ICW1_INIT 0x10 // base for Initialization
#define ICW1_ICW4 0x01 // ICW4 (not) needed
#define ICW4_8086 0x01 // 8086/80 (MCS-80/85) mode

void pic_remap(int offset1, int offset2) {
  // uint8_t master_mask, slave_mask;
  //
  // // save current mask 
  // master_mask = inb(PIC1_DATA);
  // io_wait();
  // slave_mask = inb(PIC2_DATA);
  // io_wait();
  //
  outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
  io_wait();
  outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);
  io_wait();

  outb(PIC1_DATA, offset1);
  io_wait();
  outb(PIC2_DATA, offset2);

  outb(PIC1_DATA, 4);
  io_wait();
  outb(PIC2_DATA, 2);
  io_wait();

  outb(PIC1_DATA, ICW4_8086);
  io_wait();
  outb(PIC2_DATA, ICW4_8086);
  io_wait();

  outb(PIC1_DATA, 0xFF);
  io_wait();
  outb(PIC2_DATA, 0xFF);
  io_wait();
}

void pic_send_eoi(uint8_t irq) {
    if(irq >= 8) outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

void pic_mask_irq(uint8_t irq) {
  uint16_t port;
  uint8_t val;

  if (irq < 8) {
    port = PIC1_DATA;
  } else {
    port = PIC2_DATA;
    irq -= 8;
  }
  val = inb(port) | (1 << irq);
  outb(port, val);
  io_wait();
}

void pic_unmask_irq(uint8_t irq) {
  uint16_t port;
  uint8_t val;
  if (irq < 8) {
    port = PIC1_DATA;
  } else {
    port = PIC2_DATA;
    irq -= 8;
  }
  val = inb(port) & ~(1 << irq);
  outb(port, val);
  io_wait();
}




// --- PIT ---
#define PIT_BASE_FREQ 1193180
#define PIT_CHANNEL0_DATA 0x40
#define PIT_COMMAND_REG 0x43

#define PIT_CMD_BYTE 0x36


// initalize PIT system
void timer_init_system(uint32_t frequency) {

  if (frequency == 0) return; // prevent divide by zero
  if (frequency > PIT_BASE_FREQ) frequency = PIT_BASE_FREQ;

  uint32_t divisor_u32 = PIT_BASE_FREQ / frequency;

  uint16_t divisor = (uint16_t) (divisor_u32 > 65535 ? 0 : divisor_u32);

  // send command byte 
  outb(PIT_COMMAND_REG, PIT_CMD_BYTE);

  // send the divisor
  uint8_t low_byte = (uint8_t)(divisor & 0xFF);
  uint8_t high_byte = (uint8_t)((divisor >> 8) & 0xFF);
  outb(PIT_CHANNEL0_DATA, low_byte);
  outb(PIT_CHANNEL0_DATA, high_byte);
}


// --- HAL Initialization ---
void hal_initialize() {
    // Clear the screen
     for(int i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
        vga_buffer[i] = vga_entry(' ', tty_color);
     }
     tty_row = 0;
     tty_col = 0;

     // PIC remapping would go here...
}
