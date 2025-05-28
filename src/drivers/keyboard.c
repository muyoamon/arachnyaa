// arachnyaa/src/drivers/keyboard.c
#include <drivers/keyboard.h>
#include <tty.h>          // For tty_putc
#include <io.h>  // For inb

#define KEYBOARD_DATA_PORT 0x60

// Very simple US QWERTY scancode map (make codes only)
// This is highly simplified and doesn't handle shift, caps, etc.
// Index is the scancode, value is the char.
static const char scancode_to_ascii_map[128] = {
    0,  0/*ESC*/, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b'/*Backspace*/,
    0/*Tab*/, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n'/*Enter*/,
    0/*LCtrl*/, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0/*LShift*/, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0/*RShift*/,
    0/**(KP)*/, 0/*LAlt*/, ' '/*Space*/, /* ... more keys ... */
};


void keyboard_handle_interrupt(void) {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // For now, we only care about "make" codes (key presses)
    // Make codes are generally < 0x80. Break codes are make_code + 0x80.
    if (scancode < 0x80) {
        if (scancode < sizeof(scancode_to_ascii_map) && scancode_to_ascii_map[scancode] != 0) {
            char c = scancode_to_ascii_map[scancode];
            tty_putc(c);
        } else {
            // Unknown scancode, print it for debugging
            // tty_writestring("[SC:0x");
            // tty_write_hex(scancode);
            // tty_putc(']');
        }
    }
    // We are not handling break codes (key releases) yet.
}
