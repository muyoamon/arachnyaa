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

size_t strlen(const char* str) {
  const char* s = str;

  // align pointer to 4 bytes boundary.
  while ((uintptr_t)s % sizeof(size_t) != 0) {
    if (*s == '\0') return s - str;
    s++;
  }

  const size_t *w = (const size_t *)s;

  const size_t one_mask = 0x01010101UL;
  const size_t high_bits = 0x80808080UL;

  for (;; w++) {
    size_t v = *w;
    if (((v - one_mask) & ~v & high_bits) != 0) {
      // Found a zero bytes
      s = (const char *)w;
      while (*s != '\0')
        s++;
      return s - str;
    }
  }
}

int strcmp(const char *s1, const char *s2) {
  const unsigned char *p1 = (const unsigned char*)s1;
  const unsigned char *p2 = (const unsigned char*)s2;

  typedef uintptr_t word;
  const size_t align_mask = sizeof(word) - 1;

  while (((uintptr_t)p1 & align_mask) && (uintptr_t)p2 & align_mask) {
    if (*p1 != *p2) return *p1 - *p2;
    if (*p1 == '\0') return 0;
    p1++;
    p2++;
  }

  const word *w1 = (const word *)p1;
  const word *w2 = (const word *)p2;

  // compare word by word
  while (1) {
    if (*w1 != *w2) {
      // fall back to byte.
      p1 = (const unsigned char *)w1;
      p2 = (const unsigned char *)w2;
      while (*p1 == *p2) {
        if (*p1 == '\0') return 0;
        p1++;
        p2++;
      }
      return *p1 - *p2;
    }

    const size_t one_mask = 0x01010101UL;
    const size_t high_bits = 0x80808080UL;
    
    word zero_mask = (*w1 - one_mask) & ~*w1 & high_bits;
    if (zero_mask) return 0;

    w1++;
    w2++;
  }

}
