#ifndef TTYD_KB_H_
#define TTYD_KB_H_

#include <stdint.h>

typedef struct {
    char ch; /* ch==0 means non-printable or modifier */
} keyevent_t;

void kb_init(void);
keyevent_t kb_translate(uint8_t scancode);

#endif /* TTYD_KB_H_ */
