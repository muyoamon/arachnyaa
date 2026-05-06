typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef unsigned long size_t;

enum {
  SYS_EXIT = 0,
  SYS_NS_BIND = 1,
  SYS_OPEN = 2,
  SYS_WRITE = 3,
  SYS_CAP_CLOSE = 4,
};

enum {
  KOP_OPEN = 1u << 0,
  KOP_WRITE = 1u << 3,
};

enum {
  KOBJ_ENDPOINT = 2,
};

#define BOOTSTRAP_LOG_HANDLER \
  (((uint64_t)KOBJ_ENDPOINT << 56) | ((uint64_t)1 << 32) | 0u)

static inline uint64_t syscall2(uint32_t nr, uint32_t a0, uint32_t a1) {
  uint32_t lo;
  uint32_t hi;
  asm volatile(
      "int $0x80"
      : "=a"(lo), "=D"(hi)
      : "a"(nr), "b"(a0), "c"(a1)
      : "edx", "esi", "memory");
  return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t syscall3(uint32_t nr, uint32_t a0, uint32_t a1,
                                uint32_t a2) {
  uint32_t lo;
  uint32_t hi;
  asm volatile(
      "int $0x80"
      : "=a"(lo), "=D"(hi)
      : "a"(nr), "b"(a0), "c"(a1), "d"(a2)
      : "esi", "memory");
  return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t syscall4(uint32_t nr, uint32_t a0, uint32_t a1,
                                uint32_t a2, uint32_t a3) {
  uint32_t lo;
  uint32_t hi;
  asm volatile(
      "int $0x80"
      : "=a"(lo), "=D"(hi)
      : "a"(nr), "b"(a0), "c"(a1), "d"(a2), "S"(a3)
      : "memory");
  return ((uint64_t)hi << 32) | lo;
}

static inline void sys_exit(int code) {
  asm volatile(
      "int $0x80"
      :
      : "a"(SYS_EXIT), "b"(code)
      : "ecx", "edx", "esi", "edi", "memory");
}

static inline void sys_ns_bind(const char *protocol, uint64_t handler,
                               uint32_t declared_ops) {
  syscall4(SYS_NS_BIND, (uint32_t)protocol, (uint32_t)handler,
           (uint32_t)(handler >> 32), declared_ops);
}

static inline uint64_t sys_open(const char *name, uint32_t flags) {
  return syscall2(SYS_OPEN, (uint32_t)name, flags);
}

static inline int sys_write(uint64_t handle, const void *buf, size_t len) {
  return (int)syscall4(SYS_WRITE, (uint32_t)handle, (uint32_t)(handle >> 32),
                       (uint32_t)buf, (uint32_t)len);
}

static inline void sys_cap_close(uint64_t handle) {
  syscall2(SYS_CAP_CLOSE, (uint32_t)handle, (uint32_t)(handle >> 32));
}

void _start(void) {
  static const char protocol[] = "log";
  static const char resource[] = "log:stdout";
  static const char msg[] = "Hello from initd via namespace\n";

  sys_ns_bind(protocol, BOOTSTRAP_LOG_HANDLER, KOP_OPEN);
  uint64_t handle = sys_open(resource, 0);
  if (handle != 0) {
    sys_write(handle, msg, sizeof(msg) - 1);
    sys_cap_close(handle);
  }
  sys_exit(0);
}
