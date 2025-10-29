#ifndef ARACHNYAA_STRING_H_
#define ARACHNYAA_STRING_H_

#include <lib/stddef.h>

void *memset(void* s, int c, size_t n);

void *memcpy(void *dest, const void *src, size_t n);

size_t strlen(const char* str);

#endif // ARACHNYAA_STRING_H_
