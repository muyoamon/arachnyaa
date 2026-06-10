#include "arch/cpu.h"
#include "arch/x86/defs.h"
#include "arch/x86/tss.h"
#include "boot/multiboot.h"
#include "drivers/keyboard.h"
#include "kernel/elf_loader.h"
#include "kernel/mm.h"
#include "kernel/kobj.h"
#include "kernel/time.h"
#include "kernel/user.h"
#include "mm/kstack.h"
#include "mm/layout.h"
#include "mm/pmm.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "process/thread.h"
#include <drivers/tty.h>
#include <lib/string.h>
#include <mm/kheap.h>
#include <mm/vmm.h>
#include <stdint.h>

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
extern cap_handle_t process_install_bootstrap_log_handler(process_t *proc);
extern cap_handle_t process_install_boot_manifest_cap(process_t *proc,
                                                      multiboot_info_t *mb_info);

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
      arch_cpu_idle();
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
    vmm_map((uintptr_t)kheap_base + (i * PMM_PAGE_SIZE),
            (uintptr_t)pmm_alloc_frame(), 1,
            PTE_PRESENT | PTE_WRITABLE | PTE_GLOBAL);
  }

  uintptr_t initial_heap_page = (uintptr_t)kheap_base;
  if (initial_heap_page) {
    tty_writestring("Kernel heap initial page at: 0x");
    tty_write_hex(initial_heap_page);
    tty_putc('\n');
    kheap_init(initial_heap_page);
    tty_writestring("Kernel heap initialized.\n");
    kobj_init();
    tty_writestring("Kernel object store initialized.\n");
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
  pmm_set_used_memory_bytes(pmm_get_total_memory_bytes() -
                            pmm_get_free_memory_bytes() + kheap_size);

  // --- Unmap Identity Map ---
  // unmap identity map
  vmm_clear_identity_map();

  // --- Initialize tss ---
  tty_writestring("Initializing Tss...\t");
  tss_init();
  tty_set_color(ok_color);
  tty_writestring("[OK]\n");
  tty_set_color(normal_color);

  // tty_writestring("Testing Interrupts...\t");
  // arch_local_irq_enable();
  // tty_set_color(ok_color);
  // tty_writestring("[Enabled]\n");
  // tty_set_color(normal_color);

  tty_writestring("Enabling Keyboard...\t");
  keyboard_init();
  tty_set_color(ok_color);
  tty_writestring("[Enabled]\n");
  tty_set_color(normal_color);

  // Setting kernel stack
  tty_writestring("Setting Kernel Stack...\n");
  kstack_init();

  // 7. Welcome and Halt
  tty_writestring("\nWelcome to Arachnyaa!\n\n");
  tty_writestring("Total memory: ");
  tty_write_dec(pmm_get_total_memory_bytes() / (1024 * 1024));
  tty_writestring(" MB\n");
  tty_writestring("Free memory:  ");
  tty_write_dec(pmm_get_free_memory_bytes() / (1024 * 1024));
  tty_writestring(" MB\n");

  tty_writestring("System initialized.\n");

  tty_writestring("Initializing scheduler...\n");
  arch_local_irq_disable();
  scheduler_init(20);


  // set multiboot info so that kernel don't have to link against kmain local
  multiboot_set_info(mb_info);
  

  multiboot_module_t initd;
  multiboot_map_bootinfo();
  if (multiboot_find_module(mb_info, "initd", &initd)) {
    tty_writestring("initd module found! Attempt to load binary...\n");

    // map initd image identitily
    vmm_map(initd.mod_start, initd.mod_start, (initd.mod_end - initd.mod_start - 1) / PAGE_SIZE + 1 , PTE_PRESENT);

    elf_image_t img = {.bytes = (void *)(uintptr_t)(initd.mod_start),
                       .size = initd.mod_end - initd.mod_start};

    arch_local_irq_disable();
    process_t *p = process_spawn_from_elf(&img, NULL);
    if (p) {
      process_install_bootstrap_log_handler(p);
      process_install_boot_manifest_cap(p, mb_info);
      scheduler_add(p->main);
    }
  }

  scheduler_reschedule();
  // never return;

  arch_cpu_idle();
}
