#ifndef TTYD_VTERM_H_
#define TTYD_VTERM_H_

#include <stdint.h>
#include <stdbool.h>
#include "kb.h"

#define VT_COLS     80
#define VT_ROWS     25
#define VT_LINE_MAX 256

typedef struct {
    uint16_t cells[VT_ROWS][VT_COLS];
    uint8_t  cx;
    uint8_t  cy;
    uint8_t  attr;
    char     line_buf[VT_LINE_MAX];
    uint32_t line_len;
    bool     line_ready;
    uint32_t defer_token;      /* valid when has_pending_read */
    bool     has_pending_read;
} vterm_t;

void     vterm_init(vterm_t *vt, uint8_t attr);
void     vterm_putchar(vterm_t *vt, char c);
void     vterm_input(vterm_t *vt, keyevent_t key);
uint32_t vterm_consume_line(vterm_t *vt, char *buf, uint32_t maxlen);

#endif /* TTYD_VTERM_H_ */
