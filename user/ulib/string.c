#include "string.h"
#include <stdint.h>

void *memcpy(void *dst, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  while (n--) *d++ = *s++;
  return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  if (d < s || d >= s + n) {
    while (n--) *d++ = *s++;
  } else {
    d += n; s += n;
    while (n--) *--d = *--s;
  }
  return dst;
}

void *memset(void *s, int c, size_t n) {
  uint8_t *p = (uint8_t *)s;
  while (n--) *p++ = (uint8_t)c;
  return s;
}

int memcmp(const void *a, const void *b, size_t n) {
  const uint8_t *p = (const uint8_t *)a;
  const uint8_t *q = (const uint8_t *)b;
  while (n--) {
    if (*p != *q) return (int)*p - (int)*q;
    p++; q++;
  }
  return 0;
}

size_t strlen(const char *s) {
  size_t n = 0;
  while (*s++) n++;
  return n;
}

int strcmp(const char *a, const char *b) {
  const unsigned char *p = (const unsigned char *)a;
  const unsigned char *q = (const unsigned char *)b;
  while (*p && *p == *q) { p++; q++; }
  return (int)*p - (int)*q;
}

int strncmp(const char *a, const char *b, size_t n) {
  const unsigned char *p = (const unsigned char *)a;
  const unsigned char *q = (const unsigned char *)b;
  while (n) {
    if (*p != *q) return (int)*p - (int)*q;
    if (*p == '\0') return 0;
    p++; q++; n--;
  }
  return 0;
}

char *strcpy(char *dst, const char *src) {
  char *d = dst;
  while ((*d++ = *src++)) {}
  return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
  char *d = dst;
  while (n && (*d++ = *src++)) n--;
  while (n--) *d++ = '\0';
  return dst;
}
