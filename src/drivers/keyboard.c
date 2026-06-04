// arachnyaa/src/drivers/keyboard.c
#include <drivers/keyboard.h>
#include <stdint.h>
#include <drivers/tty.h>          // For tty_putc
#include <drivers/io.h>  // For inb


// Modifier key states
static bool shift_pressed = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;
static bool caps_lock_on = false;

// Scancode maps (US QWERTY layout)
// Index is the scancode. 0 for non-printable or unhandled.
static const char scancode_map_nomod[128] = {
    0, SCANCODE_ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0/*LCtrl*/, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0/*LShift*/, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0/*RShift*/,
    0/**(KP)*/, 0/*LAlt*/, ' ', 0/*CapsLock*/, /* F1-F10 */0,0,0,0,0,0,0,0,0,0,
    /* NumLock, ScrollLock, KP7-9, KP-, KP4-6, KP+, KP1-3, KP0, KP. */
    // ... (extend as needed)
};

static const char scancode_map_shift[128] = {
    0, SCANCODE_ESC, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0/*LCtrl*/, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0/*LShift*/, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0/*RShift*/,
    0/**(KP)*/, 0/*LAlt*/, ' ', 0/*CapsLock*/, /* ... */
    // ... (extend as needed)
};

static bool scancode_map_isalpha[128] = {0};

void keyboard_init(void) {
  shift_pressed = false;
  ctrl_pressed = false;
  alt_pressed = false;
  caps_lock_on = false;
  scancode_map_isalpha[0x1E] = true; // A
  scancode_map_isalpha[0x30] = true; // B
  scancode_map_isalpha[0x2E] = true; // C
  scancode_map_isalpha[0x20] = true; // D
  scancode_map_isalpha[0x12] = true; // E
  scancode_map_isalpha[0x21] = true; // F
  scancode_map_isalpha[0x22] = true; // G
  scancode_map_isalpha[0x23] = true; // H
  scancode_map_isalpha[0x17] = true; // I
  scancode_map_isalpha[0x24] = true; // J
  scancode_map_isalpha[0x25] = true; // K
  scancode_map_isalpha[0x26] = true; // L
  scancode_map_isalpha[0x32] = true; // M
  scancode_map_isalpha[0x31] = true; // N
  scancode_map_isalpha[0x18] = true; // O
  scancode_map_isalpha[0x19] = true; // P
  scancode_map_isalpha[0x10] = true; // Q
  scancode_map_isalpha[0x13] = true; // R
  scancode_map_isalpha[0x1F] = true; // S
  scancode_map_isalpha[0x14] = true; // T
  scancode_map_isalpha[0x16] = true; // U
  scancode_map_isalpha[0x2F] = true; // V
  scancode_map_isalpha[0x11] = true; // W
  scancode_map_isalpha[0x2D] = true; // X
  scancode_map_isalpha[0x15] = true; // Y
  scancode_map_isalpha[0x2C] = true; // Z
}

void keyboard_handle_scancode(uint8_t scancode) {
  bool is_pressed = !(scancode & SCANCODE_RELEASE_BIT);
  uint8_t make_code = scancode & ~SCANCODE_RELEASE_BIT;

  switch (make_code) {
    case SCANCODE_LSHIFT:
    case SCANCODE_RSHIFT:
      shift_pressed = is_pressed;
      break;
    case SCANCODE_LCTRL:
      ctrl_pressed = is_pressed;
      break;
    case SCANCODE_LALT:
      alt_pressed = is_pressed;
      break;
    case SCANCODE_CAPSLOCK:
      if (is_pressed) {
        caps_lock_on = !caps_lock_on;
        // Send LED update command to PS/2 keyboard controller
        while (inb(0x64) & 0x02);
        outb(0x60, 0xED);
        while (inb(0x64) & 0x02);
        outb(0x60, caps_lock_on ? 0x04 : 0x00);
      }
      break;
    default:
      if (is_pressed) {
        char c = 0;
        bool effective_shift = shift_pressed;
        
        if (scancode_map_isalpha[make_code]) {
          effective_shift ^= caps_lock_on;
        }

        if (effective_shift) {
          if (make_code < sizeof(scancode_map_shift)) {
            c = scancode_map_shift[make_code];
          }
        } else {
          if (make_code < sizeof(scancode_map_nomod)) {
            c = scancode_map_nomod[make_code];
          }
        }

        if (c != 0) {
          if (!ctrl_pressed && !alt_pressed) {
            tty_putc(c);
          } else {
            if (ctrl_pressed) tty_putc('^');
            if (alt_pressed) tty_putc('~');
            tty_putc(c);
          }
        } else {
          // unhandled scancode
        }
      } else {
        // key release
      }
      break;
  }
}
