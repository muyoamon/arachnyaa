#include <stddef.h>
#include <stdint.h>

typedef unsigned long size_t;
typedef int32_t pid_t;
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

enum {
  KOP_OPEN = 1u << 0,
  KOP_CALL = 1u << 1,
  KOP_READ = 1u << 2,
  KOP_WRITE = 1u << 3,
  KOP_MAP = 1u << 4,
  KOP_CLOSE = 1u << 5,
};

enum {
  IPC_OP_OPEN = 1,
  IPC_OP_WRITE = 2,
  IPC_OP_READ = 3,
  IPC_OP_CLOSE = 4,
};

enum {
  SYS_PROG_F_BOOTMODULE = 1 << 13,
};

enum {
  KOBJ_ENDPOINT = 2,
};

#define BOOTSTRAP_LOG_HANDLER                                                  \
  (((uint64_t)KOBJ_ENDPOINT << 56) | ((uint64_t)1 << 32) | 0u)

typedef struct {
  uint32_t opcode;
  uint32_t flags;
  uint32_t num_bytes;
  uint32_t num_handles;
  uint64_t object_id;
  uint8_t data[256];
  uint64_t handles[4];
} sys_ipc_msg_t;

typedef struct {
  uint32_t allowed_ops;
} sys_open_reply_t;

typedef struct {
  cap_handle_t vspace;
  uintptr_t entry;
  uintptr_t user_sp;
  cap_handle_t handle_table;
  cap_handle_t endpoint;
  uint32_t flags;
  uint32_t priority;
  const char *argv0;
  union {
    const char *module_name;
    void *payload;
  };
} sys_proc_arg_t;

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

static inline uint64_t syscall3(uint32_t nr, uint32_t a0, uint32_t a1,
                                uint32_t a2) {
  uint32_t lo;
  uint32_t hi;
  asm volatile("int $0x80"
               : "=a"(lo), "=D"(hi)
               : "a"(nr), "b"(a0), "c"(a1), "d"(a2)
               : "esi", "memory");
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

static inline void sys_exit(int code) {
  asm volatile("int $0x80"
               :
               : "a"(SYS_EXIT), "b"(code)
               : "ecx", "edx", "esi", "edi", "memory");
}

static inline void sys_putc(char c) { syscall1(SYS_PUTC, (uint32_t)c); }

static inline int sys_ns_bind(const char *protocol, uint64_t handler,
                              uint32_t declared_ops) {
  return (int)syscall4(SYS_NS_BIND, (uint32_t)protocol, (uint32_t)handler,
                       (uint32_t)(handler >> 32), declared_ops);
}

static inline int sys_recv(uint64_t endpoint, sys_ipc_msg_t *out) {
  return (int)syscall3(SYS_RECV, (uint32_t)endpoint, (uint32_t)(endpoint >> 32),
                       (uint32_t)out);
}

static inline int sys_reply(sys_ipc_msg_t *reply) {
  return (int)syscall1(SYS_REPLY, (uint32_t)reply);
}

static inline int sys_spawn(sys_proc_arg_t *arg, pid_t *pid, uint64_t *cap) {
  return (int)syscall3(SYS_SPAWN, (uint32_t)arg, (uint32_t)pid, (uint32_t)cap);
}

static void *memcpy_local(void *dst, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  while (n--) {
    *d++ = *s++;
  }
  return dst;
}

static void memset_local(void *dst, int value, size_t n) {
  uint8_t *d = (uint8_t *)dst;
  while (n--) {
    *d++ = (uint8_t)value;
  }
}

static int streq_bytes(const char *buf, size_t len, const char *lit) {
  size_t i = 0;
  while (lit[i] != '\0') {
    if (i >= len || buf[i] != lit[i]) {
      return 0;
    }
    i++;
  }
  return i == len;
}

static void puts_raw(const char *buf, size_t len) {
  for (size_t i = 0; i < len; i++) {
    sys_putc(buf[i]);
  }
}

static void bind_log_protocol(void) {
  static const char protocol[] = "log";
  sys_ns_bind(protocol, BOOTSTRAP_LOG_HANDLER,
              KOP_OPEN | KOP_WRITE | KOP_CLOSE | KOP_READ);
}

static void spawn_log_client(void) {
  sys_proc_arg_t arg;
  pid_t pid = 0;

  memset_local(&arg, 0, sizeof(arg));
  arg.flags = SYS_PROG_F_BOOTMODULE;
  arg.module_name = "log-client";
  arg.argv0 = "log-client";

  sys_spawn(&arg, &pid, 0);
  (void)pid;
}

static void handle_open(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  sys_open_reply_t open_reply;

  memset_local(&reply, 0, sizeof(reply));
  memset_local(&open_reply, 0, sizeof(open_reply));

  if (streq_bytes((const char *)req->data, req->num_bytes, "stdout") ||
      streq_bytes((const char *)req->data, req->num_bytes, "console")) {
    reply.object_id = 1;
    open_reply.allowed_ops = KOP_WRITE | KOP_CLOSE | KOP_READ;
    reply.num_bytes = sizeof(open_reply);
    memcpy_local(reply.data, &open_reply, sizeof(open_reply));
  } else {
    reply.object_id = 0;
    reply.num_bytes = 0;
  }

  sys_reply(&reply);
}

static void handle_write(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  uint32_t written = 0;

  memset_local(&reply, 0, sizeof(reply));

  if (req->object_id == 1) {
    puts_raw((const char *)req->data, req->num_bytes);
    written = req->num_bytes;
  }

  reply.num_bytes = sizeof(written);
  memcpy_local(reply.data, &written, sizeof(written));
  sys_reply(&reply);
}

static void handle_read(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;

  // for debuging purpose, reading a constant string.
  const char str[] = "Hello from reading on initd.\n";
  
  size_t nbyte_to_read = *(size_t *)req->data;
  reply.num_bytes = sizeof(str) > nbyte_to_read ? nbyte_to_read : sizeof(str);

  memcpy_local(reply.data, str, reply.num_bytes);
  sys_reply(&reply);
}

static void handle_unknown(void) {
  sys_ipc_msg_t reply;
  uint32_t rc = 0;

  memset_local(&reply, 0, sizeof(reply));
  reply.num_bytes = sizeof(rc);
  memcpy_local(reply.data, &rc, sizeof(rc));
  sys_reply(&reply);
}

static void write(const char* str) {
  int i = 0;
  for (;;) {
    if (str[i] == '\0') {
      break;
    }
    sys_putc(str[i]);
    i++;
  }
}

static void handle_close(const sys_ipc_msg_t* req) {
  // mock implementation: printing stuff
  write("Received close request!\n");
  (void)req;

  sys_ipc_msg_t reply = {0};
  sys_reply(&reply);
}

static void server_loop(void) {
  write("Starting initd server!\n");
  for (;;) {
    sys_ipc_msg_t req;
    int err;
  
    memset_local(&req, 0, sizeof(req));
    err = sys_recv(BOOTSTRAP_LOG_HANDLER, &req);
    if (err != 0) {
      continue;
    }

    switch (req.opcode) {
    case IPC_OP_OPEN:
      handle_open(&req);
      break;
    case IPC_OP_WRITE:
      handle_write(&req);
      break;
    case IPC_OP_READ:
      handle_read(&req);
      break;
    case IPC_OP_CLOSE:
      handle_close(&req);
      break;
    default:
      handle_unknown();
      break;
    }
  }
}

void _start(void) {
  bind_log_protocol();
  spawn_log_client();
  server_loop();
  sys_exit(0);
}
