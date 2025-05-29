#include "drivers/keyboard.h"
#include "mm/multiboot.h"
#include "mm/pmm.h"
#include "time.h"
#include <stdint.h>
#include <tty.h>

// --- External Functions (Prototypes - should be in proper headers) ---
// extern void gdt_install(void);
extern void idt_install(void);

// --- Multiboot Info (Simplified for now) ---
// We should parse the mb_info_addr later for memory maps etc.

// --- kprint (Simple helper for interrupts.c or other modules) ---
// This allows other modules to print without depending on the full tty.h
void kprint(const char *str) { tty_writestring(str); }
void kprint_hex(uint32_t n) { tty_write_hex(n); }
void kprint_char(char c) { tty_putc(c); }

extern uintptr_t _kernel_start;
extern uintptr_t _kernel_end;

// --- The C Kernel Entry Point ---
void kmain(uint32_t magic, uint32_t mb_info_addr) {
  // 1. Initialize TTY first, so we can see output!
  tty_initialize();

  multiboot_info_t *mb_info = (multiboot_info_t *)(uintptr_t)mb_info_addr;

  // 2. Print a welcome message
  uint8_t normal_color = vga_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  uint8_t title_color = vga_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
  uint8_t ok_color = vga_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  uint8_t fail_color = vga_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

  tty_set_color(title_color);
  tty_writestring("--- Arachnyaa OS Booting ---  (Toronto, ON - 2025-05-23 "
                  "11:07 AM EDT)\n");
  tty_set_color(normal_color);

  // 3. Check Multiboot Magic Number
  tty_writestring("Checking Multiboot magic... ");
  if (magic != 0x2BADB002) {
    tty_set_color(fail_color);
    tty_writestring("[FAIL]\n");
    tty_writestring("Invalid Multiboot magic number. get: ");
    tty_write_hex(magic);
    tty_writestring("\nhalting.\n");
    for (;;) {
      asm volatile("cli; hlt");
    }
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

  // --- Initialize PMM ---
  pmm_init(mb_info, _kernel_start, _kernel_end);

  // 6. Test an interrupt (uncomment ONE to test)
  tty_writestring("Testing Interrupts...\t");
  asm volatile("sti");
  tty_set_color(ok_color);
  tty_writestring("[Enabled]\n"); // If we get here without a test, it's ok.
  tty_set_color(normal_color);

  tty_writestring("Enabling Keyboard...\t");
  keyboard_init();
  tty_set_color(ok_color);
  tty_writestring("[Enabled]\n");
  tty_set_color(normal_color);

  // 7. Welcome and Halt
  tty_writestring("\nWelcome to Arachnyaa!\n");
  tty_writestring("Total memory: ");
  tty_write_dec(pmm_get_total_memory_bytes() / (1024 * 1024));
  tty_writestring(" MB\n");
  tty_writestring("Free memory:  ");
  tty_write_dec(pmm_get_free_memory_bytes() / (1024 * 1024));
  tty_writestring(" MB\n");

  // Test PMM
  void *frame1 = pmm_alloc_frame();
  tty_writestring("Allocated frame 1:");
  tty_write_hex((uintptr_t)frame1);
  tty_putc('\n');
  void *frame2 = pmm_alloc_frame();
  tty_writestring("Allocated frame 2:");
  tty_write_hex((uintptr_t)frame2);
  tty_putc('\n');
  tty_writestring("Free memory after 2 allocs: ");
  tty_write_dec(pmm_get_free_memory_bytes() / (1024 * 1024));
  tty_writestring(" MB\n");
  pmm_free_frame(frame1);
  tty_writestring("Freed frame 1. Free memory: ");
  tty_write_dec(pmm_get_free_memory_bytes() / (1024 * 1024));
  tty_writestring(" MB\n");
  //
  tty_writestring("System initialized.\n");

  for (;;) {
    asm volatile("hlt"); // Halt until the next interrupt (if any)
  }
}
