#include "printf.h"
#include <stdarg.h>
#include <stdint.h>

static size_t emit_str(char *buf, size_t pos, size_t size, const char *s) {
  while (*s && pos + 1 < size) buf[pos++] = *s++;
  return pos;
}

static size_t emit_uint(char *buf, size_t pos, size_t size,
                        uint32_t val, int base, int width, char pad) {
  char tmp[16];
  int  ndig = 0;
  const char *digits = "0123456789abcdef";

  if (val == 0) {
    tmp[ndig++] = '0';
  } else {
    while (val) {
      tmp[ndig++] = digits[val % (uint32_t)base];
      val /= (uint32_t)base;
    }
  }

  while (width > ndig && pos + 1 < size) { buf[pos++] = pad; width--; }
  while (ndig-- && pos + 1 < size) buf[pos++] = tmp[ndig];
  return pos;
}

static size_t emit_int(char *buf, size_t pos, size_t size,
                       int32_t val, int width, char pad) {
  if (val < 0 && pos + 1 < size) {
    buf[pos++] = '-';
    val = -val;
    width--;
  }
  return emit_uint(buf, pos, size, (uint32_t)val, 10, width, pad);
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
  if (!size) return 0;

  va_list ap;
  va_start(ap, fmt);

  size_t pos = 0;
  while (*fmt && pos + 1 < size) {
    if (*fmt != '%') {
      buf[pos++] = *fmt++;
      continue;
    }
    fmt++;

    char pad = ' ';
    int  width = 0;
    if (*fmt == '0') { pad = '0'; fmt++; }
    while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');

    switch (*fmt) {
    case 's': pos = emit_str(buf, pos, size, va_arg(ap, const char *)); break;
    case 'd': pos = emit_int(buf, pos, size, va_arg(ap, int), width, pad); break;
    case 'u': pos = emit_uint(buf, pos, size, va_arg(ap, unsigned int), 10, width, pad); break;
    case 'x': pos = emit_uint(buf, pos, size, va_arg(ap, unsigned int), 16, width, pad); break;
    case 'c':
      if (pos + 1 < size) buf[pos++] = (char)va_arg(ap, int);
      break;
    case '%':
      if (pos + 1 < size) buf[pos++] = '%';
      break;
    default: break;
    }
    fmt++;
  }

  buf[pos] = '\0';
  va_end(ap);
  return (int)pos;
}
