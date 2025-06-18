// arachnyaa/lib/src/string.c
// implementation of string.h
//
#include <stdint.h>
#include <lib/string.h>

void *memset(void *s, int c, size_t n) {
  uint8_t *p = (uint8_t *)s;

  // write bytes until we reach word alignment
  while (n > 0 && ((uintptr_t)p & (sizeof(uintptr_t) - 1))) {
    *p++ = (uint8_t)c;
    n--;
  }
  
  // prepare word-sized pattern
  uintptr_t word = 0;
  const size_t word_size = sizeof(word);
  for (size_t i=0; i < word_size; i++) {
    word |= ((uint8_t)c << (8*i));
  }

  // write word-sized chunks
  uintptr_t *wp = (uintptr_t*)p;
  while (n >= word_size) {
    *wp++ = word;
    n -= word_size;
  }

  // handle remaining bytes
  p = (uint8_t*)wp;
  while (n > 0) {
    *p++ = (uint8_t)c;
    n--;
  }

  return s;
}
