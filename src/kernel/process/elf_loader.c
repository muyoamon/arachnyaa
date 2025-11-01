#include "kernel/elf_loader.h"
#include "arch/x86/defs.h"
#include "format/elf32.h"
#include "kernel/error.h"
#include "kernel/mm.h"
#include "mm/vmm.h"
#include <lib/stddef.h>
#include <lib/string.h>
#include <stdbool.h>
#include <stdint.h>

static bool valid_elf32_header(const Elf32_Ehdr *eh, size_t size) {
  if (size < sizeof(*eh))
    return false;
  if (!(eh->e_ident[0] == ELFMAG0 && eh->e_ident[1] == ELFMAG1 &&
        eh->e_ident[2] == ELFMAG2 && eh->e_ident[3] == ELFMAG3))
    return false;
  if (eh->e_ident[4] != ELFCLASS32)
    return false;
  if (eh->e_ident[5] != ELFDATA2LSB)
    return false;
  if (eh->e_version != EV_CURRENT)
    return false;
  if (!(eh->e_type == ET_EXEC || eh->e_type == ET_DYN))
    return false;
  if (eh->e_machine != EM_386)
    return false;
  if (eh->e_phentsize != sizeof(Elf32_Phdr))
    return false;
  if ((size_t)eh->e_phoff + (size_t)eh->e_phnum * sizeof(Elf32_Phdr) > size)
    return false;
  return true;
}

static inline uintptr_t page_floor(uintptr_t x, uintptr_t pg) {
  return x & ~(pg - 1);
}
static inline uintptr_t page_ceil(uintptr_t x, uintptr_t pg) {
  return (x + pg - 1) & ~(pg - 1);
}

static uintptr_t choose_dyn_base(void) { return USER_ENTRY_BASE; }

kerror_t elf32_load_image(const elf_image_t *img, mm_t *as,
                          elf_load_result_t *out) {
  if (!img || !img->bytes || img->size < sizeof(Elf32_Ehdr) || !as || !out)
    return KERR_INVAL;
  const Elf32_Ehdr *eh = (const Elf32_Ehdr *)img->bytes;
  if (!valid_elf32_header(eh, img->size))
    return KERR_INVAL;

  const Elf32_Phdr *phdrs =
      (const Elf32_Phdr *)((const uint8_t *)img->bytes + eh->e_phoff);

  // Compute load base for ET_DYN;
  uintptr_t base = (eh->e_type == ET_DYN) ? choose_dyn_base() : 0;

  uintptr_t lo = (uintptr_t)-1, hi = 0;

  for (uint16_t i = 0; i < eh->e_phnum; i++) {
    const Elf32_Phdr *ph = &phdrs[i];
    if (ph->p_type != PT_LOAD)
      continue;
    if (ph->p_memsz == 0)
      continue;

    uintptr_t seg_va = base + ph->p_vaddr;
    uintptr_t seg_off = ph->p_offset;
    size_t file_sz = ph->p_filesz;
    size_t mem_sz = ph->p_memsz;
    uintptr_t align = ph->p_align ? ph->p_align : 0x1000;

    uintptr_t map_begin = page_floor(seg_va, 0x1000);
    uintptr_t map_end = page_ceil(seg_va + mem_sz, 0x1000);
    size_t map_len = map_end - map_begin;

    vmm_prot_t prot = 0;
    if (ph->p_flags & PF_R)
      prot |= VMM_PROT_READ;
    if (ph->p_flags & PF_W)
      prot |= VMM_PROT_WRITE;
    // if (ph->p_flags & PF_X) prot |= VMM_PROT_EXEC;

    vmm_flags_t vmm_flags = VMM_MAP_ANON | VMM_AUTO;
    kerror_t vmm_err =
        mm_map(as, map_begin, map_len, vmm_flags, prot | VMM_PROT_READ | VMM_PROT_USER, NULL);
    if (vmm_err)
      return vmm_err;

    if (file_sz) {
      if (seg_off + file_sz > img->size)
        return KERR_INVAL; // corrupt file
      // copy file to memory.
      kerror_t err = mm_memcpy(as, seg_va, ((uintptr_t)img->bytes + seg_off),
                               ph->p_filesz);
      if (err)
        return err;
    }

    // zero bss data
    if (mem_sz > file_sz) {
      uintptr_t bss_start = seg_va + file_sz;
      size_t bss_len = mem_sz - file_sz;
      kerror_t err = mm_zero(as, bss_start, bss_len);
      if (err)
        return err;
    }

    if (map_begin < lo)
      lo = map_begin;
    if (map_end > hi)
      hi = map_end;
    (void)align;
  }
  out->entry_va = base + eh->e_entry;
  out->lo_va = (lo == (uintptr_t)-1) ? 0 : lo;
  out->hi_va = hi;
  return 0;
}

kerror_t elf_setup_user_stack(mm_t *as, uintptr_t stack_top, const char *argv0,
                              uintptr_t *out_user_sp) {
  if (!as || !out_user_sp)
    return KERR_INVAL;
  if (!argv0)
    argv0 = "prog";
  kerror_t err = 0;
  /* Layout
   * [string]
   * [align]
   * argv[1]
   * argv[0]
   * argc
   * */
  mm_map(as, stack_top - USER_STACK_SIZE, USER_STACK_SIZE,
         VMM_AUTO | VMM_MAP_ANON | VMM_GUARD_BELOW,
         VMM_PROT_READ | VMM_PROT_WRITE | VMM_PROT_USER, NULL);
  size_t len = strlen(argv0) + 1;
  uintptr_t sp = stack_top;

  sp -= len;
  uintptr_t user_str = sp;
  err = mm_memcpy(as, user_str, (uintptr_t)argv0, len);
  if (err)
    return err;

  // 4 bytes align
  sp &= ~0x3u;

  sp -= sizeof(uint32_t);
  uint32_t zero = 0;
  err = mm_memcpy(as, sp, (uintptr_t)&zero, sizeof(uint32_t));
  if (err)
    return err;

  sp -= sizeof(uint32_t);
  uint32_t ptr0 = (uint32_t)user_str;
  err = mm_memcpy(as, sp, (uintptr_t)&ptr0, sizeof(uint32_t));
  if (err)
    return err;

  sp -= sizeof(uint32_t);
  uint32_t argc = 1;
  err = mm_memcpy(as, sp, (uintptr_t)&argc, sizeof(uint32_t));
  if (err)
    return err;

  *out_user_sp = sp;
  return 0;
}
