#include <drivers/tty.h>
#include <drivers/io.h>
#include <stdint.h>
#include <lib/stddef.h>

// --- VGA Constants ---
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY_ADDR 0xC01FF000  // higher-half address
#define VGA_PORT_CMD 0x3D4
#define VGA_PORT_DATA 0x3D5

// --- Module State ---
static size_t tty_row;
static size_t tty_column;
static uint8_t tty_color;
static volatile uint16_t* tty_buffer;

// Helper to create a VGA entry (char + color)
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

// Update the hardware cursor position.
static void tty_update_cursor() {
    uint16_t pos = tty_row * VGA_WIDTH + tty_column;

    outb(VGA_PORT_CMD, 0x0F); // Low byte command
    outb(VGA_PORT_DATA, (uint8_t)(pos & 0xFF));
    outb(VGA_PORT_CMD, 0x0E); // High byte command
    outb(VGA_PORT_DATA, (uint8_t)((pos >> 8) & 0xFF));
}

// Scroll the TTY screen up by one line.
static void tty_scroll() {
    // Move all lines up one row
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index_to = y * VGA_WIDTH + x;
            const size_t index_from = (y + 1) * VGA_WIDTH + x;
            tty_buffer[index_to] = tty_buffer[index_from];
        }
    }

    // Clear the last line
    const size_t last_line_start = (VGA_HEIGHT - 1) * VGA_WIDTH;
    uint16_t blank = vga_entry(' ', tty_color);
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        tty_buffer[last_line_start + x] = blank;
    }

    // Move cursor back to the last line
    tty_row = VGA_HEIGHT - 1;
    tty_column = 0;
}

// Clear the screen and reset position.
void tty_initialize(void) {
    tty_row = 0;
    tty_column = 0;
    tty_color = vga_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    tty_buffer = (uint16_t*) VGA_MEMORY_ADDR;

    uint16_t blank = vga_entry(' ', tty_color);
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        tty_buffer[i] = blank;
    }
    tty_update_cursor();
}

void tty_set_color(uint8_t color) {
    tty_color = color;
}

uint8_t tty_get_color(void) {
    return tty_color;
}

// Put a single character at the current position.
void tty_put_entry_at(char c, uint8_t color, size_t x, size_t y) {
    const size_t index = y * VGA_WIDTH + x;
    tty_buffer[index] = vga_entry(c, color);
}

// Put a character, handling newlines and scrolling.
void tty_putc(char c) {
  
  switch (c) {
    case ('\n'):
      tty_column = 0;
      tty_row++;
      break;
    case ('\b'):
      tty_column--;
      break;
    case ('\t'):
      tty_column += (TAB_HORIZONTAL_SPACE - (tty_column % TAB_HORIZONTAL_SPACE));
      break;
    case ('\v'):
      tty_row += (TAB_VERTICAL_SPACE - (tty_row % TAB_VERTICAL_SPACE));
      break;
    case ('\r'):
      tty_column = 0;
      break;
    default:
      tty_put_entry_at(c, tty_color, tty_column, tty_row);
      tty_column++;
  }
  
  // Wrap to next line if needed
  if (tty_column >= VGA_WIDTH) {
    tty_column = 0;
    tty_row++;
  }

  // Scroll if needed
  if (tty_row >= VGA_HEIGHT) {
    tty_scroll();
  }

  tty_update_cursor();
}

void tty_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        tty_putc(data[i]);
    }
}

void tty_writestring(const char* data) {
    size_t i = 0;
    while (data[i] != '\0') {
        tty_putc(data[i]);
        i++;
    }
}

void tty_write_hex(uint32_t n) {
    char buf[11] = "0x"; // "0x" + 8 hex digits + null
    char hex_chars[] = "0123456789ABCDEF";
    int i = 9; // Start at the end

    if (n == 0) {
        tty_writestring("0x0");
        return;
    }

    while (n > 0 && i >= 2) {
        buf[i--] = hex_chars[n & 0xF];
        n >>= 4;
    }

    tty_writestring(buf + i + 1); // Print only the used part
}


void tty_write_dec(uint32_t n) {
    char buf[11]; // Max 10 digits + null
    int i = 9;
    buf[10] = '\0';

    if (n == 0) {
        tty_putc('0');
        return;
    }

    while (n > 0) {
        buf[i--] = (n % 10) + '0';
        n /= 10;
    }

    tty_writestring(buf + i + 1);
}
