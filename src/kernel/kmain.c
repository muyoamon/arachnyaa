#include "drivers/keyboard.h"
#include <mm/kheap.h>
#include <mm/paging.h>
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

extern char _kernel_start;
extern char _kernel_end;

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
  pmm_init(mb_info, (uintptr_t)&_kernel_start, (uintptr_t)&_kernel_end);
  
  // --- Initialize paging ---
  paging_init();

  // --- Initialize Kernel Heap ---
  // Allocate, for example, 1MB (256 pages) for the initial heap
  // This should be done AFTER PMM is initialized.
#define KHEAP_INITIAL_SIZE (256 * PMM_PAGE_SIZE) // 1MB
  tty_writestring("PMM: Allocating initial kernel heap space (");
  tty_write_dec(KHEAP_INITIAL_SIZE / 1024);
  tty_writestring(" KiB)...\n");

  // Allocate contiguous pages for the heap if possible, or handle
  // non-contiguous later For simplicity, let's assume PMM can give us a large
  // enough single block or we get one page For a robust heap, you might
  // allocate pages one by one and add them. For now, let's just try to get one
  // page to start.
  uintptr_t initial_heap_page = (uintptr_t)pmm_alloc_frame();
  if (initial_heap_page) {
    tty_writestring("Kernel heap initial page at: 0x");
    tty_write_hex(initial_heap_page);
    tty_putc('\n');
    kheap_init((uintptr_t)initial_heap_page, PMM_PAGE_SIZE);
    tty_writestring("Kernel heap initialized.\n");

    // Test kmalloc
    tty_writestring("KHEAP Test:\n");
    char *test_str = (char *)kmalloc(30);
    if (test_str) {
      // You'll need strcpy or similar
      // For now, let's manually fill:
      const char *msg = "Heap allocation works!";
      int i = 0;
      while (msg[i]) {
        test_str[i] = msg[i];
        i++;
      }
      test_str[i] = '\0';

      tty_writestring("  kmalloc(30) content: ");
      tty_writestring(test_str);
      tty_putc('\n');
      tty_writestring("  Freeing test_str...\n");
      kfree(test_str);

      void *p1 = kmalloc(10);
      void *p2 = kmalloc(100);
      tty_writestring("  Allocated p1: 0x");
      tty_write_hex((uintptr_t)p1);
      tty_writestring(", p2: 0x");
      tty_write_hex((uintptr_t)p2);
      tty_putc('\n');
      kfree(p1);
      kfree(p2);
      tty_writestring("  Heap tests done.\n");

    } else {
      tty_writestring("kmalloc test failed!\n");
    }
  } else {
    tty_writestring("Failed to allocate initial page for kernel heap!\n");
  }
  // --- End Kernel Heap Init & Test ---
  


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
  tty_writestring("\nWelcome to Arachnyaa!\n\n");
  tty_writestring("Total memory: ");
  tty_write_dec(pmm_get_total_memory_bytes() / (1024 * 1024));
  tty_writestring(" MB\n");
  tty_writestring("Free memory:  ");
  tty_write_dec(pmm_get_free_memory_bytes() / (1024 * 1024));
  tty_writestring(" MB\n");

  // Test PMM
  // void *frame1 = pmm_alloc_frame();
  // tty_writestring("Allocated frame 1:");
  // tty_write_hex((uintptr_t)frame1);
  // tty_putc('\n');
  // void *frame2 = pmm_alloc_frame();
  // tty_writestring("Allocated frame 2:");
  // tty_write_hex((uintptr_t)frame2);
  // tty_putc('\n');
  // tty_writestring("Free memory after 2 allocs: ");
  // tty_write_dec(pmm_get_free_memory_bytes() / (1024 * 1024));
  // tty_writestring(" MB\n");
  // pmm_free_frame(frame1);
  // tty_writestring("Freed frame 1. Free memory: ");
  // tty_write_dec(pmm_get_free_memory_bytes() / (1024 * 1024));
  // tty_writestring(" MB\n");
  // //
  // uint32_t* unmapped_ptr = (uint32_t*)0xDEADBEEF;
  // uint32_t test_val = *unmapped_ptr;
  // tty_write_dec(test_val);
  
  tty_writestring("System initialized.\n");

  for (;;) {
    asm volatile("hlt"); // Halt until the next interrupt (if any)
  }
}
