#include <stdint.h>

typedef unsigned long size_t;
typedef uint64_t cap_handle_t;

enum {
  SYS_EXIT = 0,
  SYS_NS_BIND = 0x1,
  SYS_OPEN = 0x2,
  SYS_WRITE = 0x3,
  SYS_CAP_CLOSE = 0x4,
  SYS_PUTC = 0x5,
  SYS_CALL = 0x6,
  SYS_REPLY = 0x7,
  SYS_RECV = 0x8,
  SYS_SPAWN = 0x9,
};

static inline uint64_t syscall1(uint32_t nr, uint32_t a0) {
  uint32_t lo;
  uint32_t hi;
  asm volatile("int $0x80"
               : "=a"(lo), "=D"(hi)
               : "a"(nr), "b"(a0)
               : "ecx", "edx", "esi", "memory");
  return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t syscall2(uint32_t nr, uint32_t a0, uint32_t a1) {
  uint32_t lo;
  uint32_t hi;
  asm volatile("int $0x80"
               : "=a"(lo), "=D"(hi)
               : "a"(nr), "b"(a0), "c"(a1)
               : "edx", "esi", "memory");
  return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t syscall4(uint32_t nr, uint32_t a0, uint32_t a1,
                                uint32_t a2, uint32_t a3) {
  uint32_t lo;
  uint32_t hi;
  asm volatile("int $0x80"
               : "=a"(lo), "=D"(hi)
               : "a"(nr), "b"(a0), "c"(a1), "d"(a2), "S"(a3)
               : "memory");
  return ((uint64_t)hi << 32) | lo;
}

static inline void sys_exit(int code) { return (void)syscall1(SYS_EXIT, code); }

static inline uint64_t sys_open(const char *name, uint32_t flags) {
  return syscall2(SYS_OPEN, (uint32_t)name, flags);
}

static inline int sys_write(uint64_t handle, const void *buf, size_t len) {
  return (int)syscall4(SYS_WRITE, (uint32_t)handle, (uint32_t)(handle >> 32),
                       (uint32_t)buf, (uint32_t)len);
}

static inline int sys_cap_close(uint64_t handle) {
  return (int)syscall2(SYS_CAP_CLOSE, (uint32_t)handle,
                       (uint32_t)(handle >> 32));
}

static inline void dbgprint(const char* str) {
  int i = 0;
  for (;;) {
    if (str[i] == '\0') {
      return;
    }
    syscall1(SYS_PUTC, str[i]);
    i++;
  }
}

void _start(void) {
  static const char path[] = "log:stdout";
  static const char msg[] = "Hello from log-client via IPC\n";

  dbgprint("Hello from log-client via putc\n");

  dbgprint("[LOGCLIENT] calling sys_open\n");
  cap_handle_t h = sys_open(path, 0);
  if (h == 0) {
    sys_exit(1);
  }

  dbgprint("[LOGCLIENT] calling sys_write\n");
  int rc = sys_write(h, msg, sizeof(msg) - 1);
  (void)rc;

  sys_cap_close(h);
  sys_exit(0);
}
