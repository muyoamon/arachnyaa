#include "vterm.h"
#include "../ulib/string.h"
#include <stdint.h>
#include <stdbool.h>

/* Scroll the vterm up by one row */
static void vterm_scroll(vterm_t *vt) {
    /* Move rows 1..VT_ROWS-1 up to 0..VT_ROWS-2 */
    memmove(&vt->cells[0][0], &vt->cells[1][0],
            (size_t)(VT_ROWS - 1) * VT_COLS * sizeof(uint16_t));
    /* Clear last row */
    uint16_t blank = (uint16_t)((uint16_t)vt->attr << 8) | (uint8_t)' ';
    for (uint32_t c = 0; c < (uint32_t)VT_COLS; c++)
        vt->cells[VT_ROWS - 1][c] = blank;
    vt->cy = VT_ROWS - 1;
}

void vterm_init(vterm_t *vt, uint8_t attr) {
    memset(vt, 0, sizeof(*vt));
    vt->attr = attr;
    uint16_t blank = (uint16_t)((uint16_t)attr << 8) | (uint8_t)' ';
    for (uint32_t r = 0; r < (uint32_t)VT_ROWS; r++)
        for (uint32_t c = 0; c < (uint32_t)VT_COLS; c++)
            vt->cells[r][c] = blank;
}

void vterm_putchar(vterm_t *vt, char c) {
    if (c == '\n') {
        vt->cx = 0;
        vt->cy++;
        if (vt->cy >= VT_ROWS)
            vterm_scroll(vt);
    } else if (c == '\b') {
        if (vt->cx > 0) {
            vt->cx--;
            vt->cells[vt->cy][vt->cx] =
                (uint16_t)((uint16_t)vt->attr << 8) | (uint8_t)' ';
        }
    } else {
        vt->cells[vt->cy][vt->cx] =
            (uint16_t)((uint16_t)vt->attr << 8) | (uint8_t)c;
        vt->cx++;
        if (vt->cx >= VT_COLS) {
            vt->cx = 0;
            vt->cy++;
            if (vt->cy >= VT_ROWS)
                vterm_scroll(vt);
        }
    }
}

void vterm_input(vterm_t *vt, keyevent_t key) {
    char ch = key.ch;
    if (ch == 0)
        return;

    if (ch == '\b') {
        if (vt->line_len > 0) {
            vt->line_len--;
            vterm_putchar(vt, '\b');
        }
    } else if (ch == '\n') {
        vterm_putchar(vt, '\n');
        if (vt->line_len < VT_LINE_MAX)
            vt->line_buf[vt->line_len++] = '\n';
        vt->line_ready = true;
    } else {
        /* printable */
        if (vt->line_len < VT_LINE_MAX - 1) {
            vterm_putchar(vt, ch);
            vt->line_buf[vt->line_len] = ch;
            vt->line_len++;
        }
    }
}

uint32_t vterm_consume_line(vterm_t *vt, char *buf, uint32_t maxlen) {
    uint32_t n = vt->line_len;
    if (n > maxlen) n = maxlen;
    memcpy(buf, vt->line_buf, n);
    vt->line_len   = 0;
    vt->line_ready = false;
    return n;
}
