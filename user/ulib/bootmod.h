#ifndef ULIB_BOOTMOD_H_
#define ULIB_BOOTMOD_H_

#include <stdint.h>

#define BOOT_MOD_TABLE_MAGIC  0x4D4F4442u  /* 'B','D','O','M' */
#define BOOT_MOD_MAX          16
#define BOOT_MOD_NAME_MAX     32

typedef struct {
  uint32_t phys_start;
  uint32_t phys_end;
  char     name[BOOT_MOD_NAME_MAX];
} boot_mod_entry_t;

typedef struct {
  uint32_t        magic;
  uint32_t        mod_count;
  boot_mod_entry_t mods[BOOT_MOD_MAX];
} boot_mod_table_t;

#endif /* ULIB_BOOTMOD_H_ */
