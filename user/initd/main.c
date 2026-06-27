#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "syscall.h"
#include "string.h"
#include "bootmod.h"
#include "../vfs/fs_proto.h"

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
static const char *bm_parse_fail_reason;

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
  /* Kernel wrote a compact boot_mod_table_t at physical 0x0 (= BOOTINFO_VADDR). */
  const boot_mod_table_t *tbl =
      (const boot_mod_table_t *)(uintptr_t)BOOTINFO_VADDR;

  if (tbl->magic != BOOT_MOD_TABLE_MAGIC) {
    bm_parse_fail_reason = "bad magic";
    return;
  }

  uint32_t n = tbl->mod_count;
  if (n > BM_MAX_MODULES) n = BM_MAX_MODULES;

  for (uint32_t i = 0; i < n; i++) {
    bm_modules[i].name      = tbl->mods[i].name;
    bm_modules[i].phys_start = tbl->mods[i].phys_start;
    bm_modules[i].phys_end   = tbl->mods[i].phys_end;
  }
  bm_module_count = n;
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
              KOP_OPEN | KOP_CALL | KOP_READ | KOP_EXEC | KOP_CLOSE);
  /* "" binding is done in _start() after vfs_ep is created */
}

// static void spawn_log_client(void) {
//   sys_proc_arg_t arg;
//   pid_t pid = 0;
//
//   memset(&arg, 0, sizeof(arg));
//   arg.flags = SYS_PROG_F_BOOTMODULE;
//   arg.module_name = "log-client";
//   arg.argv0 = "log-client";
//
//   sys_spawn(&arg, &pid, 0);
//   (void)pid;
// }

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

  /* data contains "<proto>:<name>"; strip up to and including the colon */
  const char *data_str = (const char *)req->data;
  size_t colon_pos = req->num_bytes;
  for (size_t i = 0; i < req->num_bytes; i++) {
    if (data_str[i] == ':') { colon_pos = i; break; }
  }
  if (colon_pos == req->num_bytes) {
    sys_reply(&reply);
    return;
  }
  const char *name = data_str + colon_pos + 1;
  size_t name_len = req->num_bytes - colon_pos - 1;

  int idx = bm_find_module(name, name_len);
  if (idx < 0) {
    reply.object_id = 0;
    sys_reply(&reply);
    return;
  }

  reply.object_id = BM_OID_BASE + (uint64_t)(uint32_t)idx;
  open_reply.allowed_ops = KOP_CALL | KOP_READ | KOP_EXEC | KOP_CLOSE;
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

/* Called when client does sys_call(bm_handle, {IPC_OP_EXEC}, &rep) with data=argv0
 * and handles[0..2]=stdio.  Opens elfloader:elf32 and forwards the request. */
static void handle_bm_exec(const sys_ipc_msg_t *req) {
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

  cap_handle_t elf32_h = sys_open("elfloader:elf32", 0);
  if (elf32_h == 0) {
    sys_cap_close(page_cap);
    sys_reply(&reply);
    return;
  }

  sys_ipc_msg_t exec_req, exec_rep;
  memset(&exec_req, 0, sizeof(exec_req));
  memset(&exec_rep, 0, sizeof(exec_rep));
  exec_req.opcode      = IPC_OP_EXEC;
  exec_req.num_handles = 4;
  exec_req.handles[0]  = req->handles[0]; /* stdin (transferred from caller) */
  exec_req.handles[1]  = req->handles[1]; /* stdout */
  exec_req.handles[2]  = req->handles[2]; /* stderr */
  exec_req.handles[3]  = page_cap;

  if (req->num_bytes > 0) {
    uint32_t nb = req->num_bytes < 255u ? req->num_bytes : 255u;
    exec_req.num_bytes = nb;
    memcpy(exec_req.data, req->data, nb);
    exec_req.data[nb] = '\0';
  } else {
    const char *mod_name = bm_modules[idx].name;
    size_t name_len = strlen(mod_name);
    if (name_len > 254u) name_len = 254u;
    exec_req.num_bytes = (uint32_t)(name_len + 1u);
    memcpy(exec_req.data, mod_name, name_len + 1u);
  }

  sys_call(elf32_h, &exec_req, &exec_rep);
  sys_cap_close(elf32_h);
  sys_cap_close(page_cap);

  /* Release stdio caps that were transferred to us */
  for (int i = 0; i < 3; i++) {
    if (req->handles[i]) sys_cap_close((cap_handle_t)req->handles[i]);
  }

  if (exec_rep.handles[0] != 0) {
    reply.num_handles = 1;
    reply.handles[0]  = exec_rep.handles[0];
  }
  sys_reply(&reply);
  if (reply.num_handles > 0)
    sys_cap_close(reply.handles[0]); /* transferred by sys_reply */
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
      else if (starts_with(data, len, "bm:") || starts_with(data, len, ":"))
        handle_bm_open(&req);
      else
        handle_unknown();
    } else if (req.object_id >= BM_OID_BASE) {
      /* bm: object — dispatch by opcode */
      switch (req.opcode) {
      case IPC_OP_READ:  handle_bm_read(&req);  break;
      case IPC_OP_EXEC:  handle_bm_exec(&req);  break;
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

void tty_dbglog(cap_handle_t tty_handle, const char* str) {
  sys_write(tty_handle, str, strlen(str));
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

  /*
   * Bind ALL protocols before spawning any service.  Each spawned service
   * inherits a snapshot of this process's namespace at spawn time; binding
   * late means later-spawned services (procd, elfloader) are missing entries,
   * and so is every process they in turn spawn (e.g. the shell via procd).
   */
  cap_handle_t tty_ep   = sys_ep_create();
  cap_handle_t proc_ep  = sys_ep_create();
  cap_handle_t elf_ep   = sys_ep_create();
  cap_handle_t ramfs_ep = sys_ep_create();
  cap_handle_t vfs_ep   = sys_ep_create();

  sys_ns_bind("tty",       tty_ep,   KOP_OPEN | KOP_CALL | KOP_READ | KOP_WRITE | KOP_CLOSE);
  sys_ns_bind("proc",      proc_ep,  KOP_OPEN | KOP_CALL | KOP_CLOSE);
  sys_ns_bind("elfloader", elf_ep,   KOP_OPEN | KOP_CALL | KOP_EXEC | KOP_CLOSE);
  sys_ns_bind("ramfs",     ramfs_ep, KOP_OPEN | KOP_CALL | KOP_READ | KOP_WRITE | KOP_CLOSE);
  sys_ns_bind("vfs",       vfs_ep,   KOP_OPEN | KOP_CALL | KOP_READ | KOP_WRITE | KOP_CLOSE | KOP_EXEC);
  sys_ns_bind("",          vfs_ep,   KOP_OPEN | KOP_CALL | KOP_READ | KOP_WRITE | KOP_CLOSE | KOP_EXEC);
  bind_log_protocol();
  bind_bm_protocol();

  /* Now spawn services — each inherits the full namespace above. */
  {
    sys_proc_arg_t tty_arg;
    memset(&tty_arg, 0, sizeof(tty_arg));
    tty_arg.flags       = SYS_PROG_F_BOOTMODULE;
    tty_arg.module_name = "ttyd";
    tty_arg.argv0       = "ttyd";
    tty_arg.endpoint    = tty_ep;
    sys_spawn(&tty_arg, NULL, NULL);
  }

  {
    sys_proc_arg_t proc_arg;
    memset(&proc_arg, 0, sizeof(proc_arg));
    proc_arg.flags       = SYS_PROG_F_BOOTMODULE;
    proc_arg.module_name = "procd";
    proc_arg.argv0       = "procd";
    proc_arg.endpoint    = proc_ep;
    sys_spawn(&proc_arg, NULL, NULL);
  }

  {
    sys_proc_arg_t elf_arg;
    memset(&elf_arg, 0, sizeof(elf_arg));
    elf_arg.flags       = SYS_PROG_F_BOOTMODULE;
    elf_arg.module_name = "elfloader";
    elf_arg.argv0       = "elfloader";
    elf_arg.endpoint    = elf_ep;
    sys_spawn(&elf_arg, NULL, NULL);
  }

  {
    sys_proc_arg_t ramfs_arg;
    memset(&ramfs_arg, 0, sizeof(ramfs_arg));
    ramfs_arg.flags       = SYS_PROG_F_BOOTMODULE;
    ramfs_arg.module_name = "ramfs";
    ramfs_arg.argv0       = "ramfs";
    ramfs_arg.endpoint    = ramfs_ep;
    sys_spawn(&ramfs_arg, NULL, NULL);
  }

  {
    sys_proc_arg_t vfs_arg;
    memset(&vfs_arg, 0, sizeof(vfs_arg));
    vfs_arg.flags       = SYS_PROG_F_BOOTMODULE;
    vfs_arg.module_name = "vfs";
    vfs_arg.argv0       = "vfs";
    vfs_arg.endpoint    = vfs_ep;
    sys_spawn(&vfs_arg, NULL, NULL);
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
    (void)focus_handle;
  }

  cap_handle_t g_tty = sys_open("tty:0", 0);
  if (g_tty == 0)
    sys_exit(1);
  tty_dbglog(g_tty, "Hello from tty!\n");
  // spawn_log_client();

  tty_dbglog(g_tty, "lauching shell\n");

  /* Print module list for debugging (tty is up, output is visible). */
  {
    if (bm_parse_fail_reason) {
      tty_dbglog(g_tty, "bm parse fail: ");
      tty_dbglog(g_tty, bm_parse_fail_reason);
      tty_dbglog(g_tty, "\n");
    }
    char mcount_buf[4] = { '0' + (char)(bm_module_count / 10),
                           '0' + (char)(bm_module_count % 10), '\n', '\0' };
    tty_dbglog(g_tty, "bm_module_count=");
    tty_dbglog(g_tty, mcount_buf);
    for (uint32_t mi = 0; mi < bm_module_count; mi++) {
      tty_dbglog(g_tty, "  mod: ");
      tty_dbglog(g_tty, bm_modules[mi].name);
      tty_dbglog(g_tty, "\n");
    }
  }

  /* Seed /bin in ramfs from boot modules. */
  tty_dbglog(g_tty, "seeding ramfs /bin\n");
  {
    /* Create /bin directory */
    cap_handle_t root_h = sys_open("ramfs:/", FS_O_DIRECTORY);
    if (root_h != 0) {
      sys_ipc_msg_t mkdir_req, mkdir_rep;
      memset(&mkdir_req, 0, sizeof(mkdir_req));
      memset(&mkdir_rep, 0, sizeof(mkdir_rep));
      mkdir_req.opcode    = FS_OP_MKDIR;
      static const char bin_name[] = "bin";
      mkdir_req.num_bytes = (uint32_t)(sizeof(bin_name) - 1u);
      memcpy(mkdir_req.data, bin_name, sizeof(bin_name) - 1u);
      sys_call(root_h, &mkdir_req, &mkdir_rep);
      sys_close(root_h);
    }

    /* Write each boot module ELF into /bin/<name>.
     * Modules are loaded at physical addresses > 1MB (outside the BOOT_INFO
     * VMOBJ which only covers [0, 1MB)).  We must map each module's physical
     * pages at a scratch virtual address before reading them. */
#define MOD_SEED_VADDR 0x20000000u
    cap_handle_t seed_vspace = sys_vspace_self();

    for (uint32_t mi = 0; mi < bm_module_count; mi++) {
      char path[128];
      static const char bin_prefix[] = "ramfs:/bin/";
      size_t prefix_len = sizeof(bin_prefix) - 1u;
      size_t name_len   = strlen(bm_modules[mi].name);
      if (prefix_len + name_len + 1u > sizeof(path)) continue;
      memcpy(path, bin_prefix, prefix_len);
      memcpy(path + prefix_len, bm_modules[mi].name, name_len);
      path[prefix_len + name_len] = '\0';

      cap_handle_t fh = sys_open(path, FS_O_CREAT | FS_O_WRONLY);
      if (fh == 0) continue;

      uint32_t phys_start = bm_modules[mi].phys_start;
      uint32_t size       = bm_modules[mi].phys_end - bm_modules[mi].phys_start;
      uint32_t npages     = (size + PAGE_SIZE_U - 1u) / PAGE_SIZE_U;

      cap_handle_t mod_cap = sys_page_alloc(npages, SYS_PAGE_F_FIXED,
                                            (uintptr_t)phys_start);
      if (mod_cap == 0) { sys_close(fh); continue; }

      sys_vspace_map_args_t margs = {
        .vspace_cap = seed_vspace,
        .virt_addr  = MOD_SEED_VADDR,
        .page_cap   = mod_cap,
        .prot_flags = VMM_PROT_READ,
      };
      if (sys_vspace_map(&margs) != 0) {
        sys_cap_close(mod_cap);
        sys_close(fh);
        continue;
      }

      const uint8_t *src = (const uint8_t *)MOD_SEED_VADDR;
      uint32_t done = 0;
      while (done < size) {
        uint32_t chunk = (size - done) < 256u ? (size - done) : 256u;
        int n = sys_write(fh, src + done, chunk);
        if (n <= 0) break;
        done += (uint32_t)n;
      }

      sys_vspace_unmap(seed_vspace, MOD_SEED_VADDR, npages);
      sys_cap_close(mod_cap);
      sys_close(fh);
    }
  }

  /* Mount ramfs at "/" in VFS */
  tty_dbglog(g_tty, "mounting ramfs at /\n");
  {
    cap_handle_t vfs_ctrl = sys_open("vfs:", 0);
    if (vfs_ctrl != 0) {
      sys_ipc_msg_t mount_req, mount_rep;
      memset(&mount_req, 0, sizeof(mount_req));
      memset(&mount_rep, 0, sizeof(mount_rep));
      mount_req.opcode    = FS_OP_MOUNT;
      fs_mount_req_t mr;
      memset(&mr, 0, sizeof(mr));
      static const char mnt_path[]  = "/";
      static const char mnt_proto[] = "ramfs";
      memcpy(mr.mount_path,    mnt_path,  sizeof(mnt_path)  - 1u);
      memcpy(mr.backend_proto, mnt_proto, sizeof(mnt_proto) - 1u);
      mount_req.num_bytes = sizeof(mr);
      memcpy(mount_req.data, &mr, sizeof(mr));
      sys_call(vfs_ctrl, &mount_req, &mount_rep);
      sys_close(vfs_ctrl);
    }
  }

  /* Spawn shell via elfloader (after all namespace bindings so shell inherits them). */
  {
    int shell_idx = bm_find_module("shell", 5);
    if (shell_idx < 0) {
      tty_dbglog(g_tty, "shell NOT found in bm!\n");
    }
    if (shell_idx >= 0) {
      tty_dbglog(g_tty, "shell found in bootmodule\n");
      uint32_t phys_start = bm_modules[shell_idx].phys_start;
      uint32_t phys_end   = bm_modules[shell_idx].phys_end;
      if (phys_end > phys_start) {
        tty_dbglog(g_tty, "allocating pages for shell\n");
        size_t num_pages =
            (phys_end - phys_start + PAGE_SIZE_U - 1u) / PAGE_SIZE_U;
        cap_handle_t shell_page =
            sys_page_alloc(num_pages, SYS_PAGE_F_FIXED, (uintptr_t)phys_start);
        if (shell_page != 0) {
          cap_handle_t elf32_h = sys_open("elfloader:elf32", 0);
          if (elf32_h != 0) {
            sys_ipc_msg_t exec_req, exec_rep;
            memset(&exec_req, 0, sizeof(exec_req));
            memset(&exec_rep, 0, sizeof(exec_rep));
            exec_req.opcode      = IPC_OP_EXEC;
            exec_req.num_handles = 4;
            exec_req.handles[3]  = shell_page;
            static const char shell_argv0[] = "shell";
            exec_req.num_bytes   = sizeof(shell_argv0);
            memcpy(exec_req.data, shell_argv0, sizeof(shell_argv0));
            sys_call(elf32_h, &exec_req, &exec_rep);
            sys_cap_close(elf32_h);
            if (exec_rep.handles[0] != 0) {
              sys_cap_close(exec_rep.handles[0]);
            }
          }
          sys_cap_close(shell_page);
        }
      }
    }
  }

  server_loop();
  sys_exit(0);
}
