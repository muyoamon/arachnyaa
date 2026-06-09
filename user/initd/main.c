#include <stddef.h>
#include <stdint.h>
#include "syscall.h"
#include "string.h"

/*
 * Pre-installed caps at fixed indices (kernel installs before first reschedule):
 *   index 0: BOOTSTRAP_LOG_HANDLER — endpoint with bind+call+reply rights
 *   index 1: BOOT_MANIFEST_CAP     — VMOBJ covering physical [0, 1MB)
 *
 * Use sys_bootstrap_cap(slot) to retrieve handles — it reads the live generation
 * counter from the cap table rather than relying on hardcoded generation values.
 */
#define BOOTSTRAP_LOG_HANDLER \
  (((uint64_t)KOBJ_ENDPOINT << 56) | ((uint64_t)1 << 32) | 0u)

/* Virtual address where initd maps the 1MB boot info region. */
#define BOOTINFO_VADDR  0x10000000u
#define BOOTINFO_SIZE   0x100000u
#define PAGE_SIZE_U     4096u

/* Multiboot structures (minimal, matching kernel's multiboot_info_t). */
typedef struct {
  uint32_t flags;
  uint32_t mem_lower;
  uint32_t mem_upper;
  uint32_t boot_device;
  uint32_t cmdline;
  uint32_t mods_count;
  uint32_t mods_addr;
} __attribute__((packed)) mb_info_t;

typedef struct {
  uint32_t mod_start;
  uint32_t mod_end;
  uint32_t cmdline;
  uint32_t pad;
} __attribute__((packed)) mb_module_t;

#define MB_FLAG_MODS (1u << 3)

/* Boot module table built from the manifest. */
#define BM_MAX_MODULES  16
#define BM_OID_BASE     0x1000u   /* object_ids for bm: handles */
#define LOG_STDOUT_OID  1u

typedef struct {
  const char *name;       /* points into the mapped VMOBJ */
  uint32_t    phys_start;
  uint32_t    phys_end;
} bm_module_t;

static bm_module_t bm_modules[BM_MAX_MODULES];
static uint32_t    bm_module_count;

/* ---- helpers ---- */

static void dbgwrite(const char *str) {
  while (*str) sys_putc(*str++);
}

static void puts_raw(const char *buf, size_t len) {
  for (size_t i = 0; i < len; i++) sys_putc(buf[i]);
}

static int starts_with(const char *buf, size_t len, const char *prefix) {
  size_t plen = strlen(prefix);
  if (len < plen) return 0;
  return memcmp(buf, prefix, plen) == 0;
}

/* Returns 1 if buf[0..len-1] equals null-terminated literal exactly. */
static int streq_bytes(const char *buf, size_t len, const char *lit) {
  size_t i = 0;
  while (lit[i] != '\0') {
    if (i >= len || buf[i] != lit[i]) return 0;
    i++;
  }
  return i == len;
}

/* ---- boot manifest parsing ---- */

static void parse_boot_modules(void) {
  /* Kernel wrote mb_info_phys at physical 0x0 (= BOOTINFO_VADDR offset 0). */
  uint32_t mb_info_phys = *(volatile uint32_t *)(uintptr_t)BOOTINFO_VADDR;
  if (mb_info_phys == 0 || mb_info_phys >= BOOTINFO_SIZE) return;

  const mb_info_t *mb =
      (const mb_info_t *)(uintptr_t)(BOOTINFO_VADDR + mb_info_phys);
  if (!(mb->flags & MB_FLAG_MODS) || mb->mods_count == 0) return;
  if (mb->mods_addr == 0 || mb->mods_addr >= BOOTINFO_SIZE) return;

  const mb_module_t *mods =
      (const mb_module_t *)(uintptr_t)(BOOTINFO_VADDR + mb->mods_addr);

  uint32_t count = mb->mods_count;
  if (count > BM_MAX_MODULES) count = BM_MAX_MODULES;

  for (uint32_t i = 0; i < count; i++) {
    if (mods[i].cmdline == 0 || mods[i].cmdline >= BOOTINFO_SIZE) continue;
    bm_modules[bm_module_count].name =
        (const char *)(uintptr_t)(BOOTINFO_VADDR + mods[i].cmdline);
    bm_modules[bm_module_count].phys_start = mods[i].mod_start;
    bm_modules[bm_module_count].phys_end   = mods[i].mod_end;
    bm_module_count++;
  }
}

static int bm_find_module(const char *name, size_t name_len) {
  for (uint32_t i = 0; i < bm_module_count; i++) {
    const char *mod_name = bm_modules[i].name;
    size_t mod_name_len = strlen(mod_name);
    if (mod_name_len == name_len && memcmp(mod_name, name, name_len) == 0)
      return (int)i;
  }
  return -1;
}

/* ---- protocol setup ---- */

static void bind_log_protocol(void) {
  static const char protocol[] = "log";
  sys_ns_bind(protocol, BOOTSTRAP_LOG_HANDLER,
              KOP_OPEN | KOP_WRITE | KOP_CLOSE | KOP_READ);
}

static void bind_bm_protocol(void) {
  static const char protocol[] = "bm";
  sys_ns_bind(protocol, BOOTSTRAP_LOG_HANDLER,
              KOP_OPEN | KOP_CALL | KOP_CLOSE);
}

static void spawn_log_client(void) {
  sys_proc_arg_t arg;
  pid_t pid = 0;

  memset(&arg, 0, sizeof(arg));
  arg.flags = SYS_PROG_F_BOOTMODULE;
  arg.module_name = "log-client";
  arg.argv0 = "log-client";

  sys_spawn(&arg, &pid, 0);
  (void)pid;
}

/* ---- IPC request handlers ---- */

static void handle_log_open(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  sys_open_reply_t open_reply;

  memset(&reply, 0, sizeof(reply));
  memset(&open_reply, 0, sizeof(open_reply));

  /* data now contains "log:<path>" — check for "log:stdout" / "log:console" */
  if (streq_bytes((const char *)req->data, req->num_bytes, "log:stdout") ||
      streq_bytes((const char *)req->data, req->num_bytes, "log:console")) {
    reply.object_id = LOG_STDOUT_OID;
    open_reply.allowed_ops = KOP_WRITE | KOP_CLOSE | KOP_READ;
    reply.num_bytes = sizeof(open_reply);
    memcpy(reply.data, &open_reply, sizeof(open_reply));
  } else {
    reply.object_id = 0;
    reply.num_bytes = 0;
  }

  sys_reply(&reply);
}

static void handle_bm_open(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  sys_open_reply_t open_reply;

  memset(&reply, 0, sizeof(reply));
  memset(&open_reply, 0, sizeof(open_reply));

  /* data contains "bm:<name>" — skip the 3-byte prefix */
  if (req->num_bytes <= 3) {
    sys_reply(&reply);
    return;
  }
  const char *name = (const char *)req->data + 3;
  size_t name_len = req->num_bytes - 3;

  int idx = bm_find_module(name, name_len);
  if (idx < 0) {
    reply.object_id = 0;
    sys_reply(&reply);
    return;
  }

  reply.object_id = BM_OID_BASE + (uint64_t)(uint32_t)idx;
  open_reply.allowed_ops = KOP_CALL | KOP_CLOSE;
  reply.num_bytes = sizeof(open_reply);
  memcpy(reply.data, &open_reply, sizeof(open_reply));
  sys_reply(&reply);
}

static void handle_log_write(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  uint32_t written = 0;

  memset(&reply, 0, sizeof(reply));

  puts_raw((const char *)req->data, req->num_bytes);
  written = req->num_bytes;

  reply.num_bytes = sizeof(written);
  memcpy(reply.data, &written, sizeof(written));
  sys_reply(&reply);
}

static void handle_log_read(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  static const char str[] = "Hello from reading on initd.\n";
  size_t nbyte_to_read = *(const size_t *)req->data;

  memset(&reply, 0, sizeof(reply));
  reply.num_bytes = sizeof(str) > nbyte_to_read
                        ? (uint32_t)nbyte_to_read
                        : (uint32_t)sizeof(str);
  memcpy(reply.data, str, reply.num_bytes);
  sys_reply(&reply);
}

static void handle_log_close(const sys_ipc_msg_t *req) {
  (void)req;
  dbgwrite("Received close request!\n");
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));
  sys_reply(&reply);
}

/* Called when client does sys_call(bm_handle, {IPC_OP_READ}, &rep).
 * Creates a fixed-physical VMOBJ for the module and transfers the cap. */
static void handle_bm_read(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  uint32_t idx = (uint32_t)(req->object_id - BM_OID_BASE);
  if (idx >= bm_module_count) {
    sys_reply(&reply);
    return;
  }

  uint32_t phys_start = bm_modules[idx].phys_start;
  uint32_t phys_end   = bm_modules[idx].phys_end;
  if (phys_end <= phys_start) {
    sys_reply(&reply);
    return;
  }

  size_t num_pages = (phys_end - phys_start + PAGE_SIZE_U - 1) / PAGE_SIZE_U;
  cap_handle_t page_cap = sys_page_alloc(num_pages, SYS_PAGE_F_FIXED,
                                         (uintptr_t)phys_start);
  if (page_cap == 0) {
    sys_reply(&reply);
    return;
  }

  reply.num_handles = 1;
  reply.handles[0]  = page_cap;
  sys_reply(&reply);
}

static void handle_bm_close(const sys_ipc_msg_t *req) {
  (void)req;
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));
  sys_reply(&reply);
}

static void handle_unknown(void) {
  sys_ipc_msg_t reply;
  uint32_t rc = 0;

  memset(&reply, 0, sizeof(reply));
  reply.num_bytes = sizeof(rc);
  memcpy(reply.data, &rc, sizeof(rc));
  sys_reply(&reply);
}

/* ---- server loop ---- */

static void server_loop(void) {
  dbgwrite("Starting initd server!\n");
  for (;;) {
    sys_ipc_msg_t req;
    int err;

    memset(&req, 0, sizeof(req));
    err = sys_recv(BOOTSTRAP_LOG_HANDLER, &req);
    if (err != 0) continue;

    if (req.opcode == IPC_OP_OPEN) {
      /* Dispatch OPEN by protocol prefix embedded in data. */
      const char *data = (const char *)req.data;
      size_t len = req.num_bytes;
      if (starts_with(data, len, "log:"))
        handle_log_open(&req);
      else if (starts_with(data, len, "bm:"))
        handle_bm_open(&req);
      else
        handle_unknown();
    } else if (req.object_id >= BM_OID_BASE) {
      /* bm: object — dispatch by opcode */
      switch (req.opcode) {
      case IPC_OP_READ:  handle_bm_read(&req);  break;
      case IPC_OP_CLOSE: handle_bm_close(&req); break;
      default:           handle_unknown();       break;
      }
    } else {
      /* log: object */
      switch (req.opcode) {
      case IPC_OP_WRITE: handle_log_write(&req); break;
      case IPC_OP_READ:  handle_log_read(&req);  break;
      case IPC_OP_CLOSE: handle_log_close(&req); break;
      default:           handle_unknown();        break;
      }
    }
  }
}

/* ---- entry point ---- */

void _start(void) {
  /* Map boot manifest VMOBJ (cap index 1) into our address space. */
  cap_handle_t self_vspace = sys_vspace_self();
  cap_handle_t boot_manifest_cap = sys_bootstrap_cap(1);
  if (self_vspace != 0 && boot_manifest_cap != 0) {
    sys_vspace_map_args_t map_args = {
      .vspace_cap = self_vspace,
      .virt_addr  = BOOTINFO_VADDR,
      .page_cap   = boot_manifest_cap,
      .prot_flags = VMM_PROT_READ,
    };
    sys_vspace_map(&map_args);
    parse_boot_modules();
  }

  /* Bind tty: protocol and spawn ttyd before log setup. */
  cap_handle_t tty_ep = sys_ep_create();
  sys_ns_bind("tty", tty_ep, KOP_OPEN | KOP_CALL | KOP_READ | KOP_WRITE | KOP_CLOSE);

  {
    sys_proc_arg_t tty_arg;
    memset(&tty_arg, 0, sizeof(tty_arg));
    tty_arg.flags       = SYS_PROG_F_BOOTMODULE;
    tty_arg.module_name = "ttyd";
    tty_arg.argv0       = "ttyd";
    tty_arg.endpoint    = tty_ep;
    sys_spawn(&tty_arg, NULL, NULL);
  }

  /* Obtain a focus handle from ttyd (so initd can switch vterms later). */
  {
    sys_ipc_msg_t focus_req, focus_rep;
    memset(&focus_req, 0, sizeof(focus_req));
    memset(&focus_rep, 0, sizeof(focus_rep));
    focus_req.opcode    = IPC_OP_OPEN;
    static const char focus_path[] = "tty:focus";
    focus_req.num_bytes = (uint32_t)(sizeof(focus_path) - 1);
    memcpy(focus_req.data, focus_path, focus_req.num_bytes);
    sys_call(tty_ep, &focus_req, &focus_rep);
    cap_handle_t focus_handle = focus_rep.handles[0];
    (void)focus_handle; /* stored for future focus switching */
  }

  bind_log_protocol();
  bind_bm_protocol();
  spawn_log_client();
  server_loop();
  sys_exit(0);
}
