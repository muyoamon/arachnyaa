#include "arch/x86/tss.h"
#include "drivers/keyboard.h"
#include "mm/layout.h"
#include "mm/multiboot.h"
#include "mm/pmm.h"
#include "kernel/time.h"
#include <mm/kheap.h>
#include <mm/paging.h>
#include <stdint.h>
#include <lib/string.h>
#include <drivers/tty.h>

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

void switch_to_user(uintptr_t user_entry, uintptr_t user_stack);

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
  vmm_init();

  // --- Initialize Kernel Heap ---
  // Allocate, for example, 4MB for the initial heap
  // This should be done AFTER PMM is initialized.
// #define KHEAP_INITIAL_SIZE (1024 * PMM_PAGE_SIZE) // 4MiB
  size_t kheap_size = kheap_end - kheap_base;
  tty_writestring("PMM: Allocating initial kernel heap space (");
  tty_write_dec(kheap_size / 1024);
  tty_writestring(" KiB)...\n");

// #define KHEAP_VADDR (0xC0400000)
  for (size_t i = 0; i < 1024; i++) {
    vmm_map((uintptr_t)kheap_base + (i * PMM_PAGE_SIZE), (uintptr_t)pmm_alloc_frame(), 1,
            PTE_PRESENT | PTE_WRITABLE);
  }

  uintptr_t initial_heap_page = (uintptr_t)kheap_base;
  if (initial_heap_page) {
    tty_writestring("Kernel heap initial page at: 0x");
    tty_write_hex(initial_heap_page);
    tty_putc('\n');
    kheap_init((uintptr_t)initial_heap_page, kheap_size);
    tty_writestring("Kernel heap initialized.\n");
  } else {
    tty_writestring("Failed to allocate initial page for kernel heap!\n");
  }
  // --- End Kernel Heap Init---

  // --- Remap PMM bitmap and reference counter ---
  
  uint16_t *dynamic_pmm_ref_count = kmalloc(pmm_total_pages * 2);
  memcpy(dynamic_pmm_ref_count, pmm_ref_count, PMM_MAX_PAGES);
  pmm_ref_count = dynamic_pmm_ref_count;
  
  uint8_t *old_pmm_bitmap = pmm_bitmap;
  pmm_bitmap = kmalloc((pmm_total_pages + 7) / 8);
  pmm_parse_mmap(mb_info, (uintptr_t)&_kernel_start,
                 ((uintptr_t)&_kernel_end) - 0xC0000000 + 0x100000,
                 pmm_total_pages);
  memcpy(pmm_bitmap, old_pmm_bitmap, PMM_MAX_PAGES / 8);

  // update used memory for inital kernel heap;
  pmm_set_used_memory_bytes(pmm_get_total_memory_bytes() - pmm_get_free_memory_bytes() + kheap_size);



  // --- Unmap Identity Map ---
  // unmap identity map
  vmm_clear_identity_map();

  // --- Initialize tss ---
  tty_writestring("Initializing Tss...\t");
  tss_init();
  tty_set_color(ok_color);
  tty_writestring("[OK]\n");
  tty_set_color(normal_color);

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

  tty_writestring("System initialized.\n");

  // testing userland
  // uintptr_t userstack = 0x8FFF0000;
  // vmm_map(userstack, // random memory for testing
  //         (uintptr_t)pmm_alloc_frame(), 1,
  //         PTE_PRESENT | PTE_WRITABLE | PTE_USER);
  //
  // uintptr_t user_entry = 0x8FFE0000;
  // vmm_map(user_entry, (uintptr_t)pmm_alloc_frame(), 1,
  //         PTE_USER | PTE_WRITABLE | PTE_PRESENT);
  // unsigned char user_prog[] = {
  //     0xb8, 0x01, 0x00, 0x00, 0x00, 0xbb, 'U',  0x00, 0x00, 0x00, 0xCD,
  //     0x80, 0xB8, 0x00, 0x00, 0x00, 0x00, 0xCD, 0x80, 0xEB, 0xFE,
  // };
  // memcpy((void *)user_entry, user_prog, sizeof(user_prog));
  // uintptr_t kernel_stack = (uintptr_t)pmm_alloc_frame();
  // uintptr_t kernel_stack_2 = (uintptr_t)pmm_alloc_frame();
  // vmm_map(0xC0FFE000, kernel_stack, 1, PTE_PRESENT | PTE_WRITABLE);
  // vmm_map(0xC0FFF000, kernel_stack_2, 1, PTE_WRITABLE | PTE_PRESENT);
  // tss_set_kernel_stack(0xC0FFE000 + PMM_PAGE_SIZE);
  // switch_to_user(user_entry, userstack);
  //
  for (;;) {
    asm volatile("hlt"); // Halt until the next interrupt (if any)
  }
}

void switch_to_user(uintptr_t user_entry, uintptr_t user_stack) {

  asm volatile("cli\n"
               "mov %0, %%ax\n"
               "mov %%ax, %%ds\n"
               "mov %%ax, %%es\n"
               "mov %%ax, %%fs\n"
               "mov %%ax, %%gs\n"

               "pushl %0\n"
               "pushl %1\n"
               "sti\n"
               "pushfl\n"
               "popl %%eax\n"
               "orl $0x200, %%eax\n"
               "pushl %%eax\n"
               "pushl %2\n"
               "pushl %3\n"
               "iret\n"
               :
               : "i"(0x23), "r"(user_stack), "i"(0x1B), "r"(user_entry)
               : "eax", "memory");
}
