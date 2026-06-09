#ifndef ULIB_SYSCALL_H_
#define ULIB_SYSCALL_H_

#include <stddef.h>
#include <stdint.h>

typedef uint64_t cap_handle_t;
typedef int32_t  pid_t;

/* ---- syscall numbers ---- */
enum {
  SYS_EXIT         = 0x0,
  SYS_NS_BIND      = 0x1,
  SYS_OPEN         = 0x2,
  SYS_WRITE        = 0x3,
  SYS_CAP_CLOSE    = 0x4,
  SYS_PUTC         = 0x5,
  SYS_CALL         = 0x6,
  SYS_REPLY        = 0x7,
  SYS_RECV         = 0x8,
  SYS_SPAWN        = 0x9,
  SYS_READ         = 0xA,
  SYS_CLOSE        = 0xB,
  SYS_EP_CREATE    = 0xC,
  SYS_CAP_RESTRICT = 0xD,
  SYS_IRQ_CLAIM    = 0xE,
  SYS_PROC_WAIT    = 0xF,
  SYS_IRQ_WAIT     = 0x10,
  SYS_IO_IN        = 0x11,
  SYS_IO_OUT       = 0x12,
  SYS_VSPACE_CREATE = 0x13,
  SYS_VSPACE_SELF  = 0x14,
  SYS_PAGE_ALLOC   = 0x15,
  SYS_VSPACE_MAP   = 0x16,
  SYS_VSPACE_UNMAP  = 0x17,
  SYS_IRQ_NOTIFY    = 0x18,
  SYS_DEFER_CALL    = 0x19,
  SYS_REPLY_TO      = 0x1A,
  SYS_BOOTSTRAP_CAP = 0x1B,
};

/* ---- protocol operation rights ---- */
enum {
  KOP_OPEN  = 1u << 0,
  KOP_CALL  = 1u << 1,
  KOP_READ  = 1u << 2,
  KOP_WRITE = 1u << 3,
  KOP_MAP   = 1u << 4,
  KOP_CLOSE = 1u << 5,
  KOP_EXEC  = 1u << 6,
};

/* ---- IPC opcodes ---- */
enum {
  IPC_OP_OPEN  = 1,
  IPC_OP_WRITE = 2,
  IPC_OP_READ  = 3,
  IPC_OP_CLOSE = 4,
  IPC_OP_EXEC   = 5,
  IPC_OP_NOTIFY = 6,
};

/* ---- kobj types ---- */
enum {
  KOBJ_ENDPOINT = 2,
};

/* ---- spawn flags ---- */
enum {
  SYS_PROG_F_BOOTMODULE = 1 << 13,
  SYS_PROG_F_USERMEM    = 1 << 14,
  SYS_PROG_F_VSPACE     = 1 << 15,
};

/* ---- capability rights (for sys_cap_restrict) ---- */
enum {
  R_PROC_WAIT = 1u << 4,
};

/* ---- page alloc flags ---- */
enum {
  SYS_PAGE_F_FIXED = 1u << 0,
};

/* ---- memory protection flags (for sys_vspace_map prot_flags) ---- */
enum {
  VMM_PROT_READ  = 1u << 0,
  VMM_PROT_WRITE = 1u << 1,
  VMM_PROT_EXEC  = 1u << 2,
};

/* ---- IPC message ---- */
typedef struct {
  uint32_t opcode;
  uint32_t flags;
  uint32_t num_bytes;
  uint32_t num_handles;
  uint64_t object_id;
  uint8_t  data[256];
  uint64_t handles[4];
} sys_ipc_msg_t;

typedef struct {
  uint32_t allowed_ops;
} sys_open_reply_t;

/* ---- vspace map args ---- */
typedef struct {
  cap_handle_t vspace_cap;
  uintptr_t    virt_addr;
  cap_handle_t page_cap;
  uint32_t     prot_flags;
} sys_vspace_map_args_t;

/* ---- process spawn args ---- */
typedef struct {
  cap_handle_t vspace;
  uintptr_t    entry;
  uintptr_t    user_sp;
  cap_handle_t handle_table;
  cap_handle_t endpoint;
  cap_handle_t stdio[3];
  uint32_t     flags;
  uint32_t     priority;
  const char  *argv0;
  union {
    const char *module_name;
    struct {
      void  *payload;
      size_t payload_size;
    };
  };
} sys_proc_arg_t;

/* ---- raw syscall trampolines ---- */

static inline uint64_t _sc0(uint32_t nr) {
  uint32_t lo, hi;
  asm volatile("int $0x80"
               : "=a"(lo), "=D"(hi)
               : "a"(nr)
               : "ebx", "ecx", "edx", "esi", "memory");
  return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t _sc1(uint32_t nr, uint32_t a0) {
  uint32_t lo, hi;
  asm volatile("int $0x80"
               : "=a"(lo), "=D"(hi)
               : "a"(nr), "b"(a0)
               : "ecx", "edx", "esi", "memory");
  return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t _sc2(uint32_t nr, uint32_t a0, uint32_t a1) {
  uint32_t lo, hi;
  asm volatile("int $0x80"
               : "=a"(lo), "=D"(hi)
               : "a"(nr), "b"(a0), "c"(a1)
               : "edx", "esi", "memory");
  return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t _sc3(uint32_t nr, uint32_t a0, uint32_t a1, uint32_t a2) {
  uint32_t lo, hi;
  asm volatile("int $0x80"
               : "=a"(lo), "=D"(hi)
               : "a"(nr), "b"(a0), "c"(a1), "d"(a2)
               : "esi", "memory");
  return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t _sc4(uint32_t nr,
                             uint32_t a0, uint32_t a1,
                             uint32_t a2, uint32_t a3) {
  uint32_t lo, hi;
  asm volatile("int $0x80"
               : "=a"(lo), "=D"(hi)
               : "a"(nr), "b"(a0), "c"(a1), "d"(a2), "S"(a3)
               : "memory");
  return ((uint64_t)hi << 32) | lo;
}

/* ---- named syscall wrappers ---- */

__attribute__((noreturn))
static inline void sys_exit(int code) {
  asm volatile("int $0x80"
               :
               : "a"(SYS_EXIT), "b"(code)
               : "ecx", "edx", "esi", "edi", "memory");
  __builtin_unreachable();
}

/* debug-only: direct character to TTY; do not use in production paths */
static inline void sys_putc(char c) {
  _sc1(SYS_PUTC, (uint32_t)(unsigned char)c);
}

static inline uint64_t sys_open(const char *name, uint32_t flags) {
  return _sc2(SYS_OPEN, (uint32_t)(uintptr_t)name, flags);
}

static inline int sys_write(cap_handle_t h, const void *buf, size_t len) {
  return (int)_sc4(SYS_WRITE,
                   (uint32_t)h, (uint32_t)(h >> 32),
                   (uint32_t)(uintptr_t)buf, (uint32_t)len);
}

static inline int sys_read(cap_handle_t h, void *buf, size_t len) {
  return (int)_sc4(SYS_READ,
                   (uint32_t)h, (uint32_t)(h >> 32),
                   (uint32_t)(uintptr_t)buf, (uint32_t)len);
}

static inline int sys_cap_close(cap_handle_t h) {
  return (int)_sc2(SYS_CAP_CLOSE, (uint32_t)h, (uint32_t)(h >> 32));
}

static inline int sys_close(cap_handle_t h) {
  return (int)_sc2(SYS_CLOSE, (uint32_t)h, (uint32_t)(h >> 32));
}

static inline int sys_call(cap_handle_t h,
                           const sys_ipc_msg_t *req, sys_ipc_msg_t *rep) {
  return (int)_sc4(SYS_CALL,
                   (uint32_t)h, (uint32_t)(h >> 32),
                   (uint32_t)(uintptr_t)req, (uint32_t)(uintptr_t)rep);
}

static inline int sys_reply(sys_ipc_msg_t *msg) {
  return (int)_sc1(SYS_REPLY, (uint32_t)(uintptr_t)msg);
}

static inline int sys_recv(cap_handle_t ep, sys_ipc_msg_t *out) {
  return (int)_sc3(SYS_RECV,
                   (uint32_t)ep, (uint32_t)(ep >> 32),
                   (uint32_t)(uintptr_t)out);
}

static inline int sys_ns_bind(const char *proto,
                              cap_handle_t handler, uint32_t ops) {
  return (int)_sc4(SYS_NS_BIND,
                   (uint32_t)(uintptr_t)proto,
                   (uint32_t)handler, (uint32_t)(handler >> 32),
                   ops);
}

static inline int sys_spawn(sys_proc_arg_t *arg, pid_t *pid, cap_handle_t *cap) {
  return (int)_sc3(SYS_SPAWN,
                   (uint32_t)(uintptr_t)arg,
                   (uint32_t)(uintptr_t)pid,
                   (uint32_t)(uintptr_t)cap);
}

static inline cap_handle_t sys_ep_create(void) {
  return _sc0(SYS_EP_CREATE);
}

static inline cap_handle_t sys_cap_restrict(cap_handle_t h, uint32_t new_rights) {
  return _sc3(SYS_CAP_RESTRICT,
              (uint32_t)h, (uint32_t)(h >> 32),
              new_rights);
}

static inline cap_handle_t sys_irq_claim(uint32_t irq) {
  return _sc1(SYS_IRQ_CLAIM, irq);
}

static inline int sys_irq_wait(cap_handle_t irq_cap) {
  return (int)_sc2(SYS_IRQ_WAIT, (uint32_t)irq_cap, (uint32_t)(irq_cap >> 32));
}

static inline int sys_proc_wait(cap_handle_t proc_cap) {
  return (int)_sc2(SYS_PROC_WAIT,
                   (uint32_t)proc_cap, (uint32_t)(proc_cap >> 32));
}

static inline uint32_t sys_io_in(uint32_t port) {
  return (uint32_t)_sc1(SYS_IO_IN, port);
}

static inline void sys_io_out(uint32_t port, uint32_t val) {
  _sc2(SYS_IO_OUT, port, val);
}

static inline cap_handle_t sys_vspace_create(void) {
  return _sc0(SYS_VSPACE_CREATE);
}

static inline cap_handle_t sys_vspace_self(void) {
  return _sc0(SYS_VSPACE_SELF);
}

static inline cap_handle_t sys_page_alloc(size_t num_pages, uint32_t flags,
                                          uintptr_t phys_addr) {
  return _sc3(SYS_PAGE_ALLOC,
              (uint32_t)num_pages, flags, (uint32_t)phys_addr);
}

static inline int sys_vspace_map(const sys_vspace_map_args_t *args) {
  return (int)_sc1(SYS_VSPACE_MAP, (uint32_t)(uintptr_t)args);
}

static inline int sys_vspace_unmap(cap_handle_t vspace,
                                   uintptr_t virt, size_t num_pages) {
  return (int)_sc4(SYS_VSPACE_UNMAP,
                   (uint32_t)vspace, (uint32_t)(vspace >> 32),
                   (uint32_t)virt, (uint32_t)num_pages);
}

static inline int sys_irq_notify(cap_handle_t irq_cap, cap_handle_t ep_cap) {
  return (int)_sc4(SYS_IRQ_NOTIFY,
                   (uint32_t)irq_cap, (uint32_t)(irq_cap >> 32),
                   (uint32_t)ep_cap,  (uint32_t)(ep_cap >> 32));
}

static inline uint32_t sys_defer_call(void) {
  return (uint32_t)_sc0(SYS_DEFER_CALL);
}

static inline int sys_reply_to(uint32_t token, sys_ipc_msg_t *msg) {
  return (int)_sc2(SYS_REPLY_TO, token, (uint32_t)(uintptr_t)msg);
}

static inline cap_handle_t sys_bootstrap_cap(uint32_t slot) {
  return _sc1(SYS_BOOTSTRAP_CAP, slot);
}

#endif /* ULIB_SYSCALL_H_ */
