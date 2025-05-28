#ifndef ARACHNYAA_KEYBOARD_H
#define ARACHNYAA_KEYBOARD_H

#include <stdint.h>

#include <stdbool.h> // For bool type

// Scancode Set 1 - Make codes (Key Press)
#define SCANCODE_ESC      0x01
#define SCANCODE_1        0x02
#define SCANCODE_2        0x03
// ... (add more numbers and symbols as needed) ...
#define SCANCODE_0        0x0B
#define SCANCODE_MINUS    0x0C
#define SCANCODE_EQUALS   0x0D
#define SCANCODE_BACKSPACE 0x0E

#define SCANCODE_Q        0x10
#define SCANCODE_W        0x11
#define SCANCODE_E        0x12
#define SCANCODE_R        0x13
#define SCANCODE_T        0x14
#define SCANCODE_Y        0x15
#define SCANCODE_U        0x16
#define SCANCODE_I        0x17
#define SCANCODE_O        0x18
#define SCANCODE_P        0x19
#define SCANCODE_LBRACKET 0x1A
#define SCANCODE_RBRACKET 0x1B
#define SCANCODE_ENTER    0x1C
#define SCANCODE_LCTRL    0x1D // Left Control

#define SCANCODE_A        0x1E
#define SCANCODE_S        0x1F
#define SCANCODE_D        0x20
#define SCANCODE_F        0x21
#define SCANCODE_G        0x22
#define SCANCODE_H        0x23
#define SCANCODE_J        0x24
#define SCANCODE_K        0x25
#define SCANCODE_L        0x26
#define SCANCODE_SEMICOLON 0x27
#define SCANCODE_APOSTROPHE 0x28
#define SCANCODE_GRAVE    0x29 // ` (backtick)
#define SCANCODE_LSHIFT   0x2A // Left Shift
#define SCANCODE_BACKSLASH 0x2B

#define SCANCODE_Z        0x2C
#define SCANCODE_X        0x2D
#define SCANCODE_C        0x2E
#define SCANCODE_V        0x2F
#define SCANCODE_B        0x30
#define SCANCODE_N        0x31
#define SCANCODE_M        0x32
#define SCANCODE_COMMA    0x33
#define SCANCODE_PERIOD   0x34
#define SCANCODE_SLASH    0x35
#define SCANCODE_RSHIFT   0x36 // Right Shift
#define SCANCODE_LALT     0x38 // Left Alt
#define SCANCODE_SPACE    0x39
#define SCANCODE_CAPSLOCK 0x3A

// Break code bit
#define SCANCODE_RELEASE_BIT 0x80

// Call this from your IRQ1 C handler
void keyboard_handle_interrupt(void);

// initializing keyboard state
void keyboard_init(void);

#endif  // ARACHNYAA_KEYBOARD_H
