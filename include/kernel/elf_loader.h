#ifndef ARACHNYAA_KERNEL_ELF_LOADER_H_
#define ARACHNYAA_KERNEL_ELF_LOADER_H_

#include "kernel/error.h"
#include "kernel/mm.h"
#include <lib/stddef.h>
#include <stdint.h>
typedef struct {
  const void *bytes; // ELF file in memory
  size_t size;
} elf_image_t;

typedef struct {
  uintptr_t entry_va;
  uintptr_t lo_va;
  uintptr_t hi_va;
} elf_load_result_t;

kerror_t elf32_load_image(const elf_image_t *img, mm_t *as,
                      elf_load_result_t *out);

kerror_t elf_setup_user_stack(mm_t *as, uintptr_t stack_top,
                          const char *argv0, uintptr_t *out_user_sp);

#endif // ARACHNYAA_KERNEL_ELF_LOADER_H_
