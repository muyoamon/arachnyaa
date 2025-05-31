// arachnyaa/src/arch/x86/tss.c
#include <arch/x86/tss.h>
#include <string.h> // For memset
// You'll need access to your GDT entries if you modify them from C.
// For now, we assume boot.s creates the GDT with a placeholder TSS base/limit.
// We will update the GDT entry for the TSS from here.

// Our single TSS structure
static tss_entry_t tss_entry;

// Assembly function to load the Task Register
extern void tss_flush(uint16_t tss_selector); // To be defined in tss_asm.s

// --- Helper: Access GDT directly (Simplified) ---
// This is a simplified way; a proper gdt_set_gate function is better.
// Assumes gdt_start is accessible (e.g., via an extern from boot.s, or pass its address)
extern uint8_t gdt_start[]; // Assuming gdt_start label is visible or passed

// GDT entry structure (matching boot.s)
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_struct_t;


void tss_init(void) {
    uintptr_t tss_base = (uintptr_t)&tss_entry;
    uint16_t tss_limit = sizeof(tss_entry_t) - 1;

    // Assuming GDT is already loaded by boot.s but we need to update the TSS entry in it.
    // GDT Selector for TSS is 0x28 (5th entry after NULL, KCode, KData, UCode, UData)
    gdt_entry_struct_t* tss_gdt_entry = (gdt_entry_struct_t*)&gdt_start[5 * sizeof(gdt_entry_struct_t)];

    // Set base and limit for TSS descriptor in GDT
    tss_gdt_entry->limit_low = tss_limit & 0xFFFF;
    tss_gdt_entry->base_low = tss_base & 0xFFFF;
    tss_gdt_entry->base_middle = (tss_base >> 16) & 0xFF;
    tss_gdt_entry->base_high = (tss_base >> 24) & 0xFF;
    // Access (0x89) and Granularity (0x00 for TSS limit in bytes) set in boot.s
    tss_gdt_entry->access = 0x89; // P=1, DPL=0, S=0 (System), Type=9 (32-bit TSS Available)
    tss_gdt_entry->granularity = 0x00; // G=0 (limit is bytes), D/B=0, L=0, AVL=0 | top nibble of limit (all 0 for 0x67)


    // Initialize the TSS structure itself
    // memset(&tss_entry, 0, sizeof(tss_entry_t)); // Not strictly needed if fields are set

    tss_entry.ss0 = 0x10; // Kernel Data Segment selector
    // esp0 will be set when a process is created or on first switch to user.
    // For now, we can set it to the top of the initial kernel stack.
    // This needs to be a valid stack that can be used when an interrupt
    // from user mode occurs.
    // extern uint8_t kernel_stack_top[]; // Assuming this symbol from boot.s
    // tss_entry.esp0 = (uint32_t)kernel_stack_top;
    // For a more robust system, each task/process would have its own kernel stack,
    // and esp0 would be updated on context switches.
    // Let's set it to a placeholder or a known safe kernel stack top.
    // For now, we might not have a readily available symbol for the *current* stack top.
    // We'll set this more properly when we switch to a user task.
    // For now, just ensure ss0 is correct. esp0 can be set before first user jump.
    tss_entry.esp0 = 0; // Will be set before first jump to user mode


    // No I/O permission bitmap for now
    tss_entry.iomap_base = sizeof(tss_entry_t);

    // Load the Task Register (LTR) with our TSS selector (0x28)
    tss_flush(0x28);
}

// This function will be called by code that prepares a jump to user mode,
// or by the scheduler when switching to a task that will run in user mode.
void tss_set_kernel_stack(uint32_t stack_ptr) { // stack_ptr is ESP0
    tss_entry.esp0 = stack_ptr;
}
