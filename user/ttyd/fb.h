#ifndef TTYD_FB_H_
#define TTYD_FB_H_

#include <stdint.h>

#define FB_COLS 80
#define FB_ROWS 25
#define FB_VADDR  0x00200000u  /* virtual address where VGA is mapped */
#define VGA_PHYS  0x000B8000u  /* physical address of VGA text buffer */

/* VGA colour codes */
#define VGA_BLACK    0
#define VGA_BLUE     1
#define VGA_GREEN    2
#define VGA_CYAN     3
#define VGA_RED      4
#define VGA_MAGENTA  5
#define VGA_BROWN    6
#define VGA_LGRAY    7
#define VGA_DGRAY    8
#define VGA_LBLUE    9
#define VGA_LGREEN   10
#define VGA_LCYAN    11
#define VGA_LRED     12
#define VGA_LMAGENTA 13
#define VGA_YELLOW   14
#define VGA_WHITE    15

static inline uint8_t fb_attr(uint8_t fg, uint8_t bg) {
    return (uint8_t)((bg << 4) | (fg & 0xFu));
}

void fb_init(void);
void fb_put(uint8_t x, uint8_t y, char c, uint8_t attr);
void fb_clear(uint8_t attr);
void fb_blit(const uint16_t cells[FB_ROWS][FB_COLS]);
void fb_set_cursor(uint8_t x, uint8_t y);

#endif /* TTYD_FB_H_ */
