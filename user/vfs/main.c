/*
 * vfs — virtual filesystem multiplexer
 *
 * Protocol: vfs:
 * Endpoint: bootstrap cap slot 0
 *
 * Maintains a mount table ({path → backend_protocol_string}) and proxies
 * all FS operations to the appropriate backend service.
 *
 * Bare names (no leading '/') are resolved via the exec search path (/bin).
 * The exec search path is global for now; per-process search paths via
 * namespace inheritance are left for a future improvement.
 *
 * VFS control handle: sys_open("vfs:", 0) → VFS_CTRL_OID.
 * Accepts FS_OP_MOUNT and FS_OP_UMOUNT to manage the mount table.
 *
 * IPC_OP_EXEC: VFS delegates to elfloader:elf32-fh, passing the backend
 * handle with move semantics (sys_cap_close, not sys_close, after transfer).
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../ulib/syscall.h"
#include "../ulib/string.h"
#include "../ulib/printf.h"
#include "../../include/uapi/fs.h"

static void dbg_hex(const char *tag, uint32_t v) {
  const char *p = tag;
  while (*p) sys_putc(*p++);
  sys_putc(':');
  for (int i = 7; i >= 0; i--)
    sys_putc("0123456789abcdef"[(v >> (i*4)) & 0xf]);
  sys_putc('\n');
}

/* Read handle_open's own return address — use only as a macro inside handle_open. */
#define CHKRA(tag) do { \
  uint32_t __ra; \
  __asm__ volatile ("movl 4(%%ebp),%0" : "=r"(__ra)); \
  dbg_hex((tag), __ra); \
} while (0)

/* ---- limits ---- */
#define VFS_MAX_MOUNTS  8
#define VFS_MAX_OBJECTS 64
#define VFS_CTRL_OID    0xFFFFu

/* ---- mount table ---- */
typedef struct {
  char mount_path[120]; /* e.g., "/" */
  char proto[16];       /* e.g., "ramfs" */
  bool in_use;
} vfs_mount_t;

static vfs_mount_t g_mounts[VFS_MAX_MOUNTS];

/* Exec search path: list of absolute directories to try for bare names. */
static const char *g_exec_paths[] = { "/bin", NULL };

/* ---- open object table ---- */
typedef struct {
  cap_handle_t bh;     /* backend KOBJ_REMOTE handle; 0 if consumed by exec */
  bool         in_use;
} vfs_obj_t;

static vfs_obj_t g_objects[VFS_MAX_OBJECTS];

/* Object IDs: slot + 1 (0 = invalid; VFS_CTRL_OID = 0xFFFF is special). */
#define OBJ_TO_OID(slot)  ((uint64_t)(slot) + 1u)
#define OID_TO_OBJ(oid)   ((uint32_t)((oid) - 1u))

/* ---- helpers ---- */

static uint32_t obj_alloc(cap_handle_t bh) {
  for (uint32_t i = 0; i < VFS_MAX_OBJECTS; i++) {
    if (!g_objects[i].in_use) {
      g_objects[i].bh     = bh;
      g_objects[i].in_use = true;
      return i;
    }
  }
  return (uint32_t)-1;
}

static int starts_with(const char *buf, size_t len, const char *prefix) {
  size_t plen = strlen(prefix);
  return len >= plen && memcmp(buf, prefix, plen) == 0;
}

/* Find mount with longest matching prefix for abs_path. */
static vfs_mount_t *find_mount(const char *abs_path) {
  vfs_mount_t *best     = NULL;
  size_t       best_len = 0;
  for (uint32_t i = 0; i < VFS_MAX_MOUNTS; i++) {
    if (!g_mounts[i].in_use) continue;
    size_t mlen = strlen(g_mounts[i].mount_path);
    if (mlen > best_len && starts_with(abs_path, strlen(abs_path), g_mounts[i].mount_path)) {
      best     = &g_mounts[i];
      best_len = mlen;
    }
  }
  return best;
}

/* Open a file on the backend for abs_path using the mount table.
 * Returns a cap_handle_t or 0 on failure. */
static cap_handle_t vfs_open_backend(const char *abs_path, uint32_t flags) {
  vfs_mount_t *m = find_mount(abs_path);
  if (!m) return 0;

  size_t      mlen    = strlen(m->mount_path);
  const char *sub     = abs_path + mlen;
  /* Ensure sub starts with '/' unless mount is "/" */
  char backend_uri[256];
  if (*sub == '/' || mlen == 1) {
    /* mount_path is "/" (len 1) or sub already starts with '/' */
    snprintf(backend_uri, sizeof(backend_uri), "%s:%s", m->proto,
             (mlen == 1) ? abs_path : sub);
  } else {
    snprintf(backend_uri, sizeof(backend_uri), "%s:/%s", m->proto, sub);
  }

  return sys_open(backend_uri, flags);
}

/* Stat a backend handle to get file type. Returns FS_TYPE_FILE on error. */
static uint8_t vfs_stat_type(cap_handle_t bh) {
  uint32_t ebp_val;
  __asm__ volatile ("mov %%ebp,%0" : "=r"(ebp_val));
  dbg_hex("SV", ebp_val);

  /* print [EBP+0] (saved EBP) and [EBP+4] (return addr) */
  uint32_t saved_ebp = *(volatile uint32_t *)(uintptr_t)(ebp_val);
  uint32_t ret_addr  = *(volatile uint32_t *)(uintptr_t)(ebp_val + 4);
  dbg_hex("SE", saved_ebp);
  dbg_hex("SR", ret_addr);

  sys_ipc_msg_t stat_req, stat_rep;
  memset(&stat_req, 0, sizeof(stat_req));
  memset(&stat_rep, 0, sizeof(stat_rep));
  stat_req.opcode = FS_OP_STAT;

  /* Check ret addr again before sys_call */
  ret_addr = *(volatile uint32_t *)(uintptr_t)(ebp_val + 4);
  dbg_hex("SB", ret_addr);

  int rc = (int)sys_call(bh, &stat_req, &stat_rep);
  sys_putc('P'); sys_putc('\n'); /* SP: if this prints, sys_call ret was correct */

  /* Check ret addr after sys_call */
  ret_addr = *(volatile uint32_t *)(uintptr_t)(ebp_val + 4);
  dbg_hex("SA", ret_addr);

  if (rc != 0) return FS_TYPE_FILE;
  if (stat_rep.num_bytes < sizeof(fs_stat_t)) return FS_TYPE_FILE;
  fs_stat_t st;
  memcpy(&st, stat_rep.data, sizeof(st));
  return st.type;
}

/* ---- IPC_OP_OPEN ---- */

static void handle_open(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t  reply;
  sys_open_reply_t oreply;
  memset(&reply,  0, sizeof(reply));
  memset(&oreply, 0, sizeof(oreply));
  CHKRA("R0");

  /* data = "vfs:<path>" */
  const char *data = (const char *)req->data;
  size_t      dlen = req->num_bytes;

  /* Accept both "vfs:<path>" (explicit protocol) and ":<path>" (default namespace). */
  const char *path;
  size_t      path_len;
  if (starts_with(data, dlen, "vfs:")) {
    path     = data + 4;
    path_len = dlen - 4;
  } else if (dlen > 0 && data[0] == ':') {
    path     = data + 1;
    path_len = dlen - 1;
  } else {
    sys_reply(&reply); return;
  }
  CHKRA("R1");

  /* Empty path → control handle. */
  if (path_len == 0) {
    CHKRA("R2");
    oreply.allowed_ops  = KOP_CALL | KOP_CLOSE;
    reply.object_id     = VFS_CTRL_OID;
    reply.num_bytes     = sizeof(oreply);
    memcpy(reply.data, &oreply, sizeof(oreply));
    sys_reply(&reply);
    return;
  }

  /* Resolve to an absolute path. */
  char abs_path[256];
  if (path[0] == '/') {
    /* Already absolute. */
    size_t copy_len = path_len < (sizeof(abs_path) - 1u) ? path_len : (sizeof(abs_path) - 1u);
    memcpy(abs_path, path, copy_len);
    abs_path[copy_len] = '\0';
    CHKRA("R3");
  } else {
    /* Bare name → try exec search path directories. */
    cap_handle_t found_bh = 0;
    for (uint32_t i = 0; g_exec_paths[i] != NULL; i++) {
      snprintf(abs_path, sizeof(abs_path), "%s/", g_exec_paths[i]);
      CHKRA("RS");
      size_t base_len = strlen(abs_path);
      size_t name_len = path_len < (sizeof(abs_path) - base_len - 1u)
                        ? path_len : (sizeof(abs_path) - base_len - 1u);
      memcpy(abs_path + base_len, path, name_len);
      CHKRA("RM");
      abs_path[base_len + name_len] = '\0';

      found_bh = vfs_open_backend(abs_path, req->flags);
      CHKRA("RB");
      if (found_bh) goto got_bh;
    }
    sys_reply(&reply);
    return;

  got_bh:;
    uint8_t  ftype = vfs_stat_type(found_bh);
    CHKRA("RT");
    uint32_t slot  = obj_alloc(found_bh);
    CHKRA("RO");
    if (slot == (uint32_t)-1) { sys_close(found_bh); sys_reply(&reply); return; }

    oreply.allowed_ops = (ftype == FS_TYPE_DIR)
                         ? (KOP_CALL | KOP_CLOSE)
                         : (KOP_READ | KOP_WRITE | KOP_CALL | KOP_CLOSE | KOP_EXEC);
    reply.object_id = OBJ_TO_OID(slot);
    reply.num_bytes = sizeof(oreply);
    CHKRA("RF");
    memcpy(reply.data, &oreply, sizeof(oreply));
    sys_reply(&reply);
    return;
  }

  /* Absolute path: open via mount table. */
  cap_handle_t bh = vfs_open_backend(abs_path, req->flags);
  CHKRA("RA");
  if (!bh) { sys_reply(&reply); return; }

  uint8_t  ftype = vfs_stat_type(bh);
  CHKRA("RT");
  uint32_t slot  = obj_alloc(bh);
  CHKRA("RO");
  if (slot == (uint32_t)-1) { sys_close(bh); sys_reply(&reply); return; }

  oreply.allowed_ops = (ftype == FS_TYPE_DIR)
                       ? (KOP_CALL | KOP_CLOSE)
                       : (KOP_READ | KOP_WRITE | KOP_CALL | KOP_CLOSE | KOP_EXEC);
  reply.object_id = OBJ_TO_OID(slot);
  reply.num_bytes = sizeof(oreply);
  CHKRA("RF");
  memcpy(reply.data, &oreply, sizeof(oreply));
  sys_reply(&reply);
}

/* ---- IPC_OP_READ ---- */

static void handle_read(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  uint32_t slot = OID_TO_OBJ(req->object_id);
  if (slot >= VFS_MAX_OBJECTS || !g_objects[slot].in_use || !g_objects[slot].bh) {
    sys_reply(&reply); return;
  }

  /* sys_read returns the data directly into a buffer; we then forward it. */
  uint32_t requested = 0;
  if (req->num_bytes >= 4) memcpy(&requested, req->data, 4);
  if (requested > 256u) requested = 256u;

  uint8_t buf[256];
  int n = sys_read(g_objects[slot].bh, buf, requested);
  if (n > 0) {
    reply.num_bytes = (uint32_t)n;
    memcpy(reply.data, buf, (uint32_t)n);
  }
  sys_reply(&reply);
}

/* ---- IPC_OP_WRITE ---- */

static void handle_write(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  uint32_t      written = 0;
  memset(&reply, 0, sizeof(reply));

  uint32_t slot = OID_TO_OBJ(req->object_id);
  if (slot < VFS_MAX_OBJECTS && g_objects[slot].in_use && g_objects[slot].bh) {
    int n = sys_write(g_objects[slot].bh, req->data, req->num_bytes);
    if (n > 0) written = (uint32_t)n;
  }
  reply.num_bytes = sizeof(written);
  memcpy(reply.data, &written, sizeof(written));
  sys_reply(&reply);
}

/* ---- IPC_OP_CLOSE ---- */

static void handle_close(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  if (req->object_id == VFS_CTRL_OID) {
    sys_reply(&reply); return;
  }

  uint32_t slot = OID_TO_OBJ(req->object_id);
  if (slot < VFS_MAX_OBJECTS && g_objects[slot].in_use) {
    if (g_objects[slot].bh) {
      sys_close(g_objects[slot].bh); /* also releases cap internally */
    }
    g_objects[slot].in_use = false;
    g_objects[slot].bh     = 0;
  }
  sys_reply(&reply);
}

/* ---- IPC_OP_EXEC ---- */

static void handle_exec(const sys_ipc_msg_t *req) {
  dbg_hex("REQ", (uint32_t)(uintptr_t)req);
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  uint32_t slot = OID_TO_OBJ(req->object_id);
  if (slot >= VFS_MAX_OBJECTS || !g_objects[slot].in_use || !g_objects[slot].bh) {
    sys_reply(&reply); return;
  }

  cap_handle_t elf_h = sys_open("elfloader:elf32-fh", 0);
  if (!elf_h) { sys_reply(&reply); return; }

  sys_ipc_msg_t exec_fwd, exec_rep;
  memset(&exec_fwd, 0, sizeof(exec_fwd));
  memset(&exec_rep,  0, sizeof(exec_rep));

  exec_fwd.opcode      = IPC_OP_EXEC;
  exec_fwd.num_handles = 4;
  exec_fwd.handles[0]  = req->handles[0]; /* stdin  */
  exec_fwd.handles[1]  = req->handles[1]; /* stdout */
  exec_fwd.handles[2]  = req->handles[2]; /* stderr */
  exec_fwd.handles[3]  = g_objects[slot].bh;

  /* Forward argv0. */
  dbg_hex("NBP", (uint32_t)(uintptr_t)req);
  uint32_t nb = req->num_bytes < 255u ? req->num_bytes : 255u;
  exec_fwd.num_bytes = nb;
  if (nb > 0) memcpy(exec_fwd.data, req->data, nb);

  sys_call(elf_h, &exec_fwd, &exec_rep);
  sys_cap_close(elf_h);

  /* Move semantics: give up our caps without sending IPC_OP_CLOSE.
   * Elfloader holds derived caps and is responsible for closing them. */
  sys_cap_close((cap_handle_t)req->handles[0]);
  sys_cap_close((cap_handle_t)req->handles[1]);
  sys_cap_close((cap_handle_t)req->handles[2]);
  sys_cap_close(g_objects[slot].bh); /* derived cap in elfloader owns the close */
  g_objects[slot].bh = 0;           /* consumed; CLOSE handler will no-op the backend */

  /* Forward watch_cap to caller. */
  if (exec_rep.handles[0]) {
    reply.num_handles = 1;
    reply.handles[0]  = exec_rep.handles[0];
  }
  sys_reply(&reply);
  if (reply.num_handles > 0)
    sys_cap_close((cap_handle_t)reply.handles[0]); /* transferred by sys_reply */
}

/* ---- custom FS ops: forward to backend ---- */

static void handle_fs_op(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  uint32_t slot = OID_TO_OBJ(req->object_id);
  if (slot >= VFS_MAX_OBJECTS || !g_objects[slot].in_use || !g_objects[slot].bh) {
    sys_reply(&reply); return;
  }

  sys_ipc_msg_t fwd;
  memcpy(&fwd, req, sizeof(fwd));
  sys_ipc_msg_t rep;
  memset(&rep, 0, sizeof(rep));
  sys_call(g_objects[slot].bh, &fwd, &rep);
  sys_reply(&rep);
}

/* ---- FS_OP_MOUNT (on VFS_CTRL_OID) ---- */

static void handle_mount(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  if (req->num_bytes < sizeof(fs_mount_req_t)) { sys_reply(&reply); return; }

  fs_mount_req_t mr;
  memcpy(&mr, req->data, sizeof(mr));
  mr.mount_path[119] = '\0';
  mr.backend_proto[15] = '\0';

  /* Find free slot or replace existing same path. */
  for (uint32_t i = 0; i < VFS_MAX_MOUNTS; i++) {
    if (!g_mounts[i].in_use ||
        strcmp(g_mounts[i].mount_path, mr.mount_path) == 0) {
      memcpy(g_mounts[i].mount_path, mr.mount_path,  sizeof(g_mounts[i].mount_path));
      memcpy(g_mounts[i].proto,      mr.backend_proto, sizeof(g_mounts[i].proto));
      g_mounts[i].in_use = true;
      sys_reply(&reply);
      return;
    }
  }
  sys_reply(&reply);
}

/* ---- FS_OP_UMOUNT (on VFS_CTRL_OID) ---- */

static void handle_umount(const sys_ipc_msg_t *req) {
  char path[120];
  size_t n = req->num_bytes < 119u ? req->num_bytes : 119u;
  memcpy(path, req->data, n);
  path[n] = '\0';

  for (uint32_t i = 0; i < VFS_MAX_MOUNTS; i++) {
    if (g_mounts[i].in_use && strcmp(g_mounts[i].mount_path, path) == 0)
      g_mounts[i].in_use = false;
  }
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));
  sys_reply(&reply);
}

/* ---- entry point ---- */

void _start(void) {
  cap_handle_t my_ep = sys_bootstrap_cap(0);

  sys_putc('V'); sys_putc('F'); sys_putc('S'); sys_putc('\n');  /* startup marker */

  memset(g_mounts,  0, sizeof(g_mounts));
  memset(g_objects, 0, sizeof(g_objects));

  for (;;) {
    sys_putc('L'); sys_putc('\n');  /* loop top */
    { uint32_t _ebp; __asm__ volatile ("mov %%ebp,%0" : "=r"(_ebp)); dbg_hex("EL", _ebp); }
    sys_ipc_msg_t req;
    memset(&req, 0, sizeof(req));
    sys_putc('W'); sys_putc('\n');  /* before sys_recv */
    if (sys_recv(my_ep, &req) != 0) { sys_putc('E'); sys_putc('\n'); continue; }
    sys_putc('G'); sys_putc('\n');  /* got message */
    dbg_hex("OP", req.opcode);

    switch (req.opcode) {
    case IPC_OP_OPEN:  handle_open(&req);  sys_putc('o'); sys_putc('\n'); break;
    case IPC_OP_READ:  handle_read(&req);  sys_putc('r'); sys_putc('\n'); break;
    case IPC_OP_WRITE: handle_write(&req); sys_putc('w'); sys_putc('\n'); break;
    case IPC_OP_CLOSE: handle_close(&req); sys_putc('c'); sys_putc('\n'); break;
    case IPC_OP_EXEC: {
      uint32_t ebp_val;
      __asm__ volatile ("mov %%ebp, %0" : "=r"(ebp_val));
      dbg_hex("EBP", ebp_val);
      dbg_hex("ADDR", (uint32_t)(uintptr_t)&req);
      handle_exec(&req);
      sys_putc('x'); sys_putc('\n');
      break;
    }

    default:
      /* Control-object ops or forwarded FS ops. */
      if (req.object_id == VFS_CTRL_OID) {
        switch (req.opcode) {
        case FS_OP_MOUNT:  handle_mount(&req);  sys_putc('m'); sys_putc('\n'); break;
        case FS_OP_UMOUNT: handle_umount(&req); sys_putc('u'); sys_putc('\n'); break;
        default: { sys_ipc_msg_t r; memset(&r,0,sizeof(r)); sys_reply(&r); sys_putc('d'); sys_putc('\n'); } break;
        }
      } else {
        handle_fs_op(&req); sys_putc('f'); sys_putc('\n');
      }
      break;
    }
    sys_putc('Z'); sys_putc('\n');  /* loop bottom */
  }
}
