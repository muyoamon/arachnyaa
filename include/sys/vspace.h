#ifndef ARACHNYAA_SYS_VSPACE_H_
#define ARACHNYAA_SYS_VSPACE_H_

#include "kernel/cap.h"
#include "mm/vmm.h"
#include <lib/stddef.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * Payload for KOBJ_VMOBJ capabilities.
 * Wraps a set of physical pages that can be mapped into address spaces.
 */
typedef struct {
  size_t num_pages;
  bool phys_fixed;   /* true = wraps existing physical memory, do not free */
  union {
    uintptr_t phys_base;     /* used when phys_fixed */
    uintptr_t *phys_frames;  /* array of num_pages frame addresses when !phys_fixed */
  };
} kobj_vmobj_t;

/* Page allocation flags passed to sys_page_alloc. */
enum {
  SYS_PAGE_F_FIXED = 1u << 0,  /* wrap existing physical range (caller provides phys_addr) */
};

typedef struct {
  cap_handle_t vspace_cap;
  uintptr_t virt_addr;
  cap_handle_t page_cap;
  uint32_t prot_flags;
} sys_vspace_map_args_t;

cap_handle_t sys_vspace_create(void);
cap_handle_t sys_vspace_self(void);
cap_handle_t sys_page_alloc(size_t num_pages, uint32_t flags, uintptr_t phys_addr);
int sys_vspace_map(const sys_vspace_map_args_t *args);
int sys_vspace_unmap(cap_handle_t vspace_cap, uintptr_t virt_addr, size_t num_pages);

#endif // ARACHNYAA_SYS_VSPACE_H_
