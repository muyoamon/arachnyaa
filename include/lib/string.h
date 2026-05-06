#ifndef ARACHNYAA_STRING_H_
#define ARACHNYAA_STRING_H_

#include <lib/stddef.h>

void *memset(void* s, int c, size_t n);

void *memcpy(void *dest, const void *src, size_t n);

size_t strlen(const char* str);

int strcmp(const char* s1, const char* s2);

void *mempcpy(void *restrict dst, const void *restrict src, size_t n);

char *stpcpy(char *restrict dst, const char *restrict src);
char *strcpy(char *restrict dst, const char *restrict src);
char *strcat(char *restrict dst, const char *restrict src);

#endif // ARACHNYAA_STRING_H_
