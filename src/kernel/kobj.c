#include "kernel/kobj.h"
#include "mm/kheap.h"
#include <stdatomic.h>
#include <lib/string.h>

#define KOBJ_DEFAULT_SIZE 1024

static kobj_t* kobj_arr;
static size_t kobj_arr_size;

static kobj_t *kobj_arr_get_next_free(void) {
  static size_t idx = 0;
  size_t start = idx;
  for (;;) {
    if (kobj_arr[idx].type == 0) {
      return &kobj_arr[idx];
    }
    idx = (idx + 1) % kobj_arr_size;
    if (idx == start) {
      return NULL;
    }
  }
}


void kobj_init(void) {
  kobj_arr = kmalloc(KOBJ_DEFAULT_SIZE * sizeof(kobj_t));
  memset(kobj_arr, 0, KOBJ_DEFAULT_SIZE * sizeof(kobj_t));
  kobj_arr_size = KOBJ_DEFAULT_SIZE;
}

kobj_t *kobj_create(void) {
  kobj_t *kobj = kobj_arr_get_next_free();
  if (!kobj) {
    return NULL;
  }
  memset(kobj, 0, sizeof(*kobj));
  atomic_store(&kobj->refcnt, 1);
  return kobj;
}

void kobj_get(kobj_t *kobj) {
  if (!kobj) {
    return;
  }
  atomic_fetch_add(&kobj->refcnt, 1);
}

void kobj_put(kobj_t *kobj) {
  if (!kobj) {
    return;
  }
  if (atomic_fetch_sub(&kobj->refcnt, 1) != 1) {
    return;
  }
  if (kobj->ops && kobj->ops->release) {
    kobj->ops->release(kobj);
  }
  memset(kobj, 0, sizeof(*kobj));
}



