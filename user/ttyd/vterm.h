#ifndef TTYD_VTERM_H_
#define TTYD_VTERM_H_

#include <stdint.h>
#include <stdbool.h>
#include "kb.h"

#define VT_COLS     80
#define VT_ROWS     25
#define VT_LINE_MAX 256

/* ANSI escape parser states */
#define VTS_NORMAL 0
#define VTS_ESC    1  /* received ESC, waiting for '[' */
#define VTS_CSI    2  /* received ESC '[', collecting params */

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
    /* ANSI escape state */
    uint8_t  esc_state;
    uint16_t csi_params[4];
    uint8_t  csi_nparam;
} vterm_t;

void     vterm_init(vterm_t *vt, uint8_t attr);
void     vterm_putchar(vterm_t *vt, char c);
void     vterm_input(vterm_t *vt, keyevent_t key);
uint32_t vterm_consume_line(vterm_t *vt, char *buf, uint32_t maxlen);

#endif /* TTYD_VTERM_H_ */
