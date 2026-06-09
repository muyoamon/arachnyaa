#include "kb.h"
#include <stdint.h>
#include <stdbool.h>

/* Scancode set 1 make/break constants (no kernel headers) */
#define SC_LSHIFT   0x2A
#define SC_RSHIFT   0x36
#define SC_LCTRL    0x1D
#define SC_LALT     0x38
#define SC_CAPSLOCK 0x3A
#define SC_RELEASE  0x80u

/* US QWERTY scancode maps — index is make code (0..127).
 * Row comments show scancode range covered.
 * Total must equal exactly 128 entries.
 */
/* US QWERTY scancode maps — exactly 128 entries each (indices 0x00-0x7F).
 * Entry = printable char or 0 for non-printable.
 */
static const char scancode_map_nomod[128] = {
    /* 0x00-0x0E (15): reserved, ESC, 1-0, -, =, backspace */
    0, 0x1B, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    /* 0x0F-0x1C (14): tab, q-p, [, ], enter */
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    /* 0x1D-0x29 (13): LCtrl, a-l, ;, ', ` */
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    /* 0x2A-0x36 (13): LShift, \, z-m, ,, ., /, RShift */
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    /* 0x37-0x3A (4): KP*, LAlt, space, CapsLock */
    0, 0, ' ', 0,
    /* 0x3B-0x58 (30): F1-F10, NumLock, ScrollLock, KP7-KP. */
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    /* 0x59-0x7F (39): extended/reserved */
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0
};

static const char scancode_map_shift[128] = {
    /* 0x00-0x0E (15) */
    0, 0x1B, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    /* 0x0F-0x1C (14) */
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    /* 0x1D-0x29 (13) */
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    /* 0x2A-0x36 (13) */
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    /* 0x37-0x3A (4) */
    0, 0, ' ', 0,
    /* 0x3B-0x58 (30) */
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
    /* 0x59-0x7F (39) */
    0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0
};

/* Alpha scancode set — caps lock affects these */
static bool scancode_isalpha[128];

/* Modifier state */
static bool shift_pressed;
static bool ctrl_pressed;
static bool caps_lock_on;

void kb_init(void) {
    shift_pressed = false;
    ctrl_pressed  = false;
    caps_lock_on  = false;

    /* Mark alpha keys */
    scancode_isalpha[0x1E] = true; /* A */
    scancode_isalpha[0x30] = true; /* B */
    scancode_isalpha[0x2E] = true; /* C */
    scancode_isalpha[0x20] = true; /* D */
    scancode_isalpha[0x12] = true; /* E */
    scancode_isalpha[0x21] = true; /* F */
    scancode_isalpha[0x22] = true; /* G */
    scancode_isalpha[0x23] = true; /* H */
    scancode_isalpha[0x17] = true; /* I */
    scancode_isalpha[0x24] = true; /* J */
    scancode_isalpha[0x25] = true; /* K */
    scancode_isalpha[0x26] = true; /* L */
    scancode_isalpha[0x32] = true; /* M */
    scancode_isalpha[0x31] = true; /* N */
    scancode_isalpha[0x18] = true; /* O */
    scancode_isalpha[0x19] = true; /* P */
    scancode_isalpha[0x10] = true; /* Q */
    scancode_isalpha[0x13] = true; /* R */
    scancode_isalpha[0x1F] = true; /* S */
    scancode_isalpha[0x14] = true; /* T */
    scancode_isalpha[0x16] = true; /* U */
    scancode_isalpha[0x2F] = true; /* V */
    scancode_isalpha[0x11] = true; /* W */
    scancode_isalpha[0x2D] = true; /* X */
    scancode_isalpha[0x15] = true; /* Y */
    scancode_isalpha[0x2C] = true; /* Z */
}

keyevent_t kb_translate(uint8_t scancode) {
    keyevent_t ev = {.ch = 0};

    bool is_pressed = !(scancode & (uint8_t)SC_RELEASE);
    uint8_t make = scancode & (uint8_t)~SC_RELEASE;

    switch (make) {
    case SC_LSHIFT:
    case SC_RSHIFT:
        shift_pressed = is_pressed;
        return ev;
    case SC_LCTRL:
        ctrl_pressed = is_pressed;
        return ev;
    case SC_LALT:
        /* alt state tracked but not used for char generation */
        return ev;
    case SC_CAPSLOCK:
        if (is_pressed)
            caps_lock_on = !caps_lock_on;
        return ev;
    default:
        break;
    }

    if (!is_pressed)
        return ev; /* key release — no character */

    if (make >= 128)
        return ev;

    bool effective_shift = shift_pressed;
    if (scancode_isalpha[make])
        effective_shift = (bool)(effective_shift ^ caps_lock_on);

    char c;
    if (effective_shift)
        c = scancode_map_shift[make];
    else
        c = scancode_map_nomod[make];

    if (c != 0 && !ctrl_pressed)
        ev.ch = c;

    return ev;
}
