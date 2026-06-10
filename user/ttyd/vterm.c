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

/* Dispatch a completed CSI sequence: ESC [ params... cmd */
static void vterm_csi(vterm_t *vt, char cmd) {
    uint16_t p0 = vt->csi_params[0];
    uint16_t p1 = vt->csi_params[1];
    uint16_t blank = (uint16_t)((uint16_t)vt->attr << 8) | (uint8_t)' ';

    switch (cmd) {
    case 'J':  /* erase display */
        if (p0 == 2) {
            for (uint32_t r = 0; r < (uint32_t)VT_ROWS; r++)
                for (uint32_t c = 0; c < (uint32_t)VT_COLS; c++)
                    vt->cells[r][c] = blank;
        }
        break;
    case 'H':  /* cursor position: ESC[row;colH (1-indexed; 0 treated as 1) */
    case 'f':
        vt->cy = (uint8_t)(p0 > 0 ? p0 - 1 : 0);
        vt->cx = (uint8_t)(p1 > 0 ? p1 - 1 : 0);
        if (vt->cy >= VT_ROWS) vt->cy = VT_ROWS - 1;
        if (vt->cx >= VT_COLS) vt->cx = VT_COLS - 1;
        break;
    case 'K':  /* erase line */
        if (p0 == 0) {
            for (uint8_t c = vt->cx; c < VT_COLS; c++)
                vt->cells[vt->cy][c] = blank;
        } else if (p0 == 1) {
            for (uint8_t c = 0; c <= vt->cx; c++)
                vt->cells[vt->cy][c] = blank;
        } else if (p0 == 2) {
            for (uint8_t c = 0; c < VT_COLS; c++)
                vt->cells[vt->cy][c] = blank;
        }
        break;
    default:
        break;
    }
}

static void vterm_putchar_normal(vterm_t *vt, char c) {
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

void vterm_putchar(vterm_t *vt, char c) {
    switch (vt->esc_state) {
    case VTS_ESC:
        if (c == '[') {
            vt->esc_state = VTS_CSI;
            vt->csi_params[0] = vt->csi_params[1] =
            vt->csi_params[2] = vt->csi_params[3] = 0;
            vt->csi_nparam = 0;
        } else {
            vt->esc_state = VTS_NORMAL;
            vterm_putchar_normal(vt, c);
        }
        return;
    case VTS_CSI:
        if (c >= '0' && c <= '9') {
            uint8_t i = vt->csi_nparam < 3 ? vt->csi_nparam : 3u;
            vt->csi_params[i] = (uint16_t)(vt->csi_params[i] * 10u + (uint8_t)(c - '0'));
        } else if (c == ';') {
            if (vt->csi_nparam < 3) vt->csi_nparam++;
        } else {
            vt->esc_state = VTS_NORMAL;
            vterm_csi(vt, c);
        }
        return;
    default: /* VTS_NORMAL */
        if (c == '\033') {
            vt->esc_state = VTS_ESC;
            return;
        }
        vterm_putchar_normal(vt, c);
        break;
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
