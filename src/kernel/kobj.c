


#include "kernel/kobj.h"
#include "mm/kheap.h"
#include <stdatomic.h>
#include <string.h>

#define KOBJ_DEFAULT_SIZE 1024

static kobj_t* kobj_arr;
static size_t kobj_arr_size;

static kobj_t *kobj_arr_get_next_free(void) {
  static size_t idx = 0;
  for (;kobj_arr[idx].type != 0; idx++) {
    if (idx >= kobj_arr_size) {
      idx = 0;
      continue;
    }
  }
  return &kobj_arr[idx];
}


void kobj_init(void) {
  kobj_arr = kmalloc(KOBJ_DEFAULT_SIZE * sizeof(kobj_t));
  memset(kobj_arr, 0, KOBJ_DEFAULT_SIZE * sizeof(kobj_t));
  kobj_arr_size = KOBJ_DEFAULT_SIZE;
}

kobj_t *kobj_create(void) {
  kobj_t *kobj = kobj_arr_get_next_free();
  
  return kobj;
}




