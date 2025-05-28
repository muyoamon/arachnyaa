#include "time.h"
#include <tty.h>
#include <stdint.h>

// --- External Functions (Prototypes - should be in proper headers) ---
//extern void gdt_install(void);
extern void idt_install(void);

// --- Multiboot Info (Simplified for now) ---
// We should parse the mb_info_addr later for memory maps etc.

// --- kprint (Simple helper for interrupts.c or other modules) ---
// This allows other modules to print without depending on the full tty.h
void kprint(const char *str) {
    tty_writestring(str);
}
void kprint_hex(uint32_t n) {
    tty_write_hex(n);
}
void kprint_char(char c) {
    tty_putc(c);
}


// --- The C Kernel Entry Point ---
void kmain(uint32_t magic, uint32_t mb_info_addr) {
    // 1. Initialize TTY first, so we can see output!
    tty_initialize();

    // 2. Print a welcome message
    uint8_t normal_color = vga_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    uint8_t title_color = vga_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    uint8_t ok_color = vga_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    uint8_t fail_color = vga_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    tty_set_color(title_color);
    tty_writestring("--- Arachnyaa OS Booting ---  (Toronto, ON - 2025-05-23 11:07 AM EDT)\n");
    tty_set_color(normal_color);

    // 3. Check Multiboot Magic Number
    tty_writestring("Checking Multiboot magic... ");
    if (magic != 0x1BADB002) {
        tty_set_color(fail_color);
        tty_writestring("[FAIL]\n");
        tty_writestring("Invalid Multiboot magic number. get: ");
        tty_write_hex(magic);
        tty_writestring("\nhalting.\n");
        for (;;) { asm volatile ("cli; hlt"); }
    }
    tty_set_color(ok_color);
    tty_writestring("[OK]\n");
    tty_set_color(normal_color);
    tty_writestring("Multiboot Info at: ");
    tty_write_hex(mb_info_addr);
    tty_putc('\n');
    
    // 5. Initialize IDT
    tty_writestring("Initializing IDT...         ");
    idt_install();
    tty_set_color(ok_color);
    tty_writestring("[OK]\n");
    tty_set_color(normal_color);

    tty_writestring("Initializing PIT (100Hz)... ");
    timer_init_system(100);
    tty_set_color(ok_color);
    tty_writestring("[OK]\n");
    tty_set_color(normal_color);
  
  
  

    // 6. Test an interrupt (uncomment ONE to test)
    tty_writestring("Testing Interrupts... ");
    asm volatile ("sti"); // Test Overflow Exception
    tty_set_color(ok_color);
    tty_writestring("[Enabled]\n"); // If we get here without a test, it's ok.
    tty_set_color(normal_color);


    // 7. Welcome and Halt
    tty_writestring("\nWelcome to Arachnyaa!\n");
    tty_writestring("System initialized. Waiting for interrupts.\n");

    // Enable interrupts IF you have set up PIC & IRQ handlers.
    // For now, we keep them disabled.
    // asm volatile ("sti");

    // 8. Infinite Halt Loop
    for (;;) {
        asm volatile ("hlt"); // Halt until the next interrupt (if any)
    }
}
