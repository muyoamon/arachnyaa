#include <lib/string.h>
void *memcpy(void *dest, const void *src, size_t n) {
  if (n==0 || dest == src) {
    return dest;
  }

  asm volatile (
    "cld\n"
    "rep movsb"
    :
    : "S"(src), "D"(dest), "c"(n)
    : "memory", "cc"
  );
  return dest;
}
