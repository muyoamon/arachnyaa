#include "fb.h"
#include "../ulib/syscall.h"

static volatile uint16_t *vga_buf = (volatile uint16_t *)(uintptr_t)FB_VADDR;

void fb_init(void) {
    /* Allocate physical page at VGA_PHYS and map it to FB_VADDR */
    cap_handle_t page_cap = sys_page_alloc(1, SYS_PAGE_F_FIXED, (uintptr_t)VGA_PHYS);
    cap_handle_t vspace_cap = sys_vspace_self();

    sys_vspace_map_args_t map_args = {
        .vspace_cap = vspace_cap,
        .virt_addr  = (uintptr_t)FB_VADDR,
        .page_cap   = page_cap,
        .prot_flags = VMM_PROT_READ | VMM_PROT_WRITE,
    };
    sys_vspace_map(&map_args);
}

void fb_put(uint8_t x, uint8_t y, char c, uint8_t attr) {
    if (x >= FB_COLS || y >= FB_ROWS) return;
    uint32_t idx = (uint32_t)y * FB_COLS + (uint32_t)x;
    vga_buf[idx] = (uint16_t)((uint16_t)attr << 8) | (uint8_t)c;
}

void fb_clear(uint8_t attr) {
    uint16_t blank = (uint16_t)((uint16_t)attr << 8) | (uint8_t)' ';
    for (uint32_t i = 0; i < (uint32_t)(FB_ROWS * FB_COLS); i++)
        vga_buf[i] = blank;
}

void fb_blit(const uint16_t cells[FB_ROWS][FB_COLS]) {
    for (uint32_t r = 0; r < (uint32_t)FB_ROWS; r++)
        for (uint32_t c = 0; c < (uint32_t)FB_COLS; c++)
            vga_buf[r * FB_COLS + c] = cells[r][c];
}
