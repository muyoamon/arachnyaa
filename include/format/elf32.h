#ifndef ARACHNYAA_FORMAT_ELF32_H_
#define ARACHNYAA_FORMAT_ELF32_H_

#include <stdint.h>
#define EI_NIDENT 16
#define ELFMAG0   0x7F
#define ELFMAG1   'E'
#define ELFMAG2   'L'
#define ELFMAG3   'F'

#define ELFCLASS32 1 
#define ELFDATA2LSB 1 
#define EV_CURRENT 1 
#define ET_EXEC 2 
#define ET_DYN  3 
#define EM_386  3 

#define PT_NULL 0 
#define PT_LOAD 1 
#define PF_X    0x1 
#define PF_W    0x2
#define PF_R    0x4

typedef struct {
  unsigned char e_ident[EI_NIDENT];
  uint16_t e_type;
  uint16_t e_machine;
  uint32_t e_version;
  uint32_t e_entry;
  uint32_t e_phoff;
  uint32_t e_shoff;
  uint32_t e_flags;
  uint16_t e_ehsize;
  uint16_t e_phentsize;
  uint16_t e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;
  uint16_t e_shstrndx;
} Elf32_Ehdr;

typedef struct {
  uint32_t p_type;
  uint32_t p_offset;
  uint32_t p_vaddr;
  uint32_t p_paddr;
  uint32_t p_filesz;
  uint32_t p_memsz;
  uint32_t p_align;
  uint32_t p_flags;
} Elf32_Phdr;

#endif // ARACHNYAA_FORMAT_ELF32_H_
