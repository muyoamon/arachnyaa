#include "process/task.h"
#include "arch/x86/defs.h"
#include "mm/kheap.h"
#include "mm/layout.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "register.h"
#include <stdint.h>

extern void* pdpt;

bool task_context_init(task_t *new_task, void (*entry_point)(void)) {


  // Allocate a kernel stack for this task
  // Let's use 2 pages (8KB) for the kernel stack for now
  new_task->kernel_stack_bottom = (uintptr_t)pmm_alloc_frame();
  if (!new_task->kernel_stack_bottom) {
    kfree(new_task);
    return false;
  }
  void *second_page = pmm_alloc_frame();
  if (!second_page) {
    pmm_free_frame((void *)new_task->kernel_stack_bottom);
    kfree(new_task);
    return false;
  }
  vmm_map((uintptr_t)(kernel_stack_region_base + ((new_task->id * 2) * PAGE_SIZE)),
          new_task->kernel_stack_bottom, 1, PTE_PRESENT | PTE_WRITABLE);
  vmm_map((uintptr_t)(kernel_stack_region_base + ((new_task->id * 2 + 1) * PAGE_SIZE)),
          (uintptr_t)second_page, 1, PTE_PRESENT | PTE_WRITABLE);
  new_task->kernel_stack_top = (uintptr_t)(kernel_stack_region_base + ((new_task->id * 2 + 2) * PAGE_SIZE));
  new_task->kernel_stack_bottom = (uintptr_t)(kernel_stack_region_base + ((new_task->id * 2) * PAGE_SIZE));
  
  // setup initial register state
  new_task->registers = kmalloc(sizeof(registers_t));
  if (!new_task->registers) {
    vmm_unmap(new_task->kernel_stack_bottom);
    vmm_unmap(new_task->kernel_stack_bottom + PAGE_SIZE);
    kfree(new_task);
    return false;
  }
  new_task->registers->cs = 0x08;
  new_task->registers->ds = 0x10;
  new_task->registers->es = 0x10;
  new_task->registers->fs = 0x10;
  new_task->registers->gs = 0x10;
  new_task->registers->ss = 0x10;

  new_task->registers->eip = (uint32_t)entry_point;
  new_task->registers->esp = new_task->kernel_stack_top;

  new_task->registers->eflags = 0x202;

  new_task->page_directory_phys = (uintptr_t)pdpt;


  return true;
}
