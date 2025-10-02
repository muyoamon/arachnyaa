#ifndef _ARACHNYAA_TTY_H
#define _ARACHNYAA_TTY_H

#include <lib/stddef.h>
#include <stdint.h>

#define TAB_HORIZONTAL_SPACE  4
#define TAB_VERTICAL_SPACE    3

// Initialize the TTY system (clears screen, sets cursor).
void tty_initialize(void);

// Set the current text color.
void tty_set_color(uint8_t color);

// Get the current text color.
uint8_t tty_get_color(void);

// Put a single character on the screen. Handles newlines, scrolling, etc.
void tty_putc(char c);

// Write a block of data to the TTY.
void tty_write(const char* data, size_t size);

// Write a null-terminated string to the TTY.
void tty_writestring(const char* data);

// Write an unsigned 32-bit integer as hexadecimal.
void tty_write_hex(uint32_t n);

// Write an unsigned 32-bit integer as decimal.
void tty_write_dec(uint32_t n);

// VGA colors
#define VGA_COLOR_BLACK         0
#define VGA_COLOR_BLUE          1
#define VGA_COLOR_GREEN         2
#define VGA_COLOR_CYAN          3
#define VGA_COLOR_RED           4
#define VGA_COLOR_MAGENTA       5
#define VGA_COLOR_BROWN         6
#define VGA_COLOR_LIGHT_GREY    7
#define VGA_COLOR_DARK_GREY     8
#define VGA_COLOR_LIGHT_BLUE    9
#define VGA_COLOR_LIGHT_GREEN   10
#define VGA_COLOR_LIGHT_CYAN    11
#define VGA_COLOR_LIGHT_RED     12
#define VGA_COLOR_LIGHT_MAGENTA 13
#define VGA_COLOR_LIGHT_BROWN   14
#define VGA_COLOR_WHITE         15

// Helper to create a color byte
static inline uint8_t vga_color(uint8_t fg, uint8_t bg) {
    return fg | bg << 4;
}

#endif // _ARACHNYAA_TTY_H

