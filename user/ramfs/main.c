/*
 * ramfs — in-memory filesystem service
 *
 * Protocol: ramfs:
 * Endpoint: bootstrap cap slot 0
 *
 * Supports IPC_OP_OPEN/READ/WRITE/CLOSE and FS_OP_SEEK/STAT/READDIR/
 * TRUNCATE/MKDIR/UNLINK/RENAME.
 *
 * Limits: RAMFS_MAX_INODES inodes, RAMFS_MAX_FDS open file descriptors.
 * File data is stored in page-allocated memory via a bump allocator at
 * RAMFS_DATA_VBASE. Allocation is page-granular (4 KB minimum per file).
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../ulib/syscall.h"
#include "../ulib/string.h"
#include "../../include/uapi/fs.h"

/* ---- limits ---- */
#define RAMFS_MAX_INODES  128
#define RAMFS_MAX_FDS     64
#define RAMFS_NAME_MAX    64
#define PAGE_SIZE_U       4096u

/* ---- bump allocator for file data ---- */
#define RAMFS_DATA_VBASE  0x30000000u
#define RAMFS_DATA_LIMIT  (RAMFS_DATA_VBASE + 32u * 1024u * 1024u)
static uint32_t g_bump = RAMFS_DATA_VBASE;

static void *bump_alloc(uint32_t size) {
  uint32_t npages = (size + PAGE_SIZE_U - 1u) / PAGE_SIZE_U;
  if (g_bump + npages * PAGE_SIZE_U > RAMFS_DATA_LIMIT) return NULL;
  cap_handle_t self = sys_vspace_self();
  cap_handle_t pg   = sys_page_alloc(npages, 0, 0);
  if (!pg || !self) return NULL;
  sys_vspace_map_args_t ma = {
    .vspace_cap = self,
    .virt_addr  = g_bump,
    .page_cap   = pg,
    .prot_flags = VMM_PROT_READ | VMM_PROT_WRITE,
  };
  if (sys_vspace_map(&ma) != 0) { sys_cap_close(pg); return NULL; }
  sys_cap_close(pg);
  void *ptr = (void *)(uintptr_t)g_bump;
  g_bump += npages * PAGE_SIZE_U;
  return ptr;
}

/* ---- inode table ---- */
typedef struct {
  char     name[RAMFS_NAME_MAX];
  uint8_t  type;         /* FS_TYPE_FILE or FS_TYPE_DIR */
  uint32_t parent_idx;   /* parent inode index; root's parent is 0 (itself) */
  uint32_t size;         /* byte count for files */
  uint8_t *data;         /* page-allocated; NULL for dirs */
  uint32_t data_cap;     /* allocated bytes (page multiple) */
  bool     in_use;
} ramfs_inode_t;

static ramfs_inode_t g_inodes[RAMFS_MAX_INODES];
static uint32_t      g_inode_count = 0;

/* Inode 0 = root directory, pre-created. */
#define ROOT_INODE 0u
#define INVALID_INODE (~0u)

static void inodes_init(void) {
  memset(g_inodes, 0, sizeof(g_inodes));
  g_inodes[0].type       = FS_TYPE_DIR;
  g_inodes[0].parent_idx = 0u;  /* root's parent is itself */
  g_inodes[0].in_use     = true;
  g_inode_count          = 1u;
}

static uint32_t inode_alloc(void) {
  for (uint32_t i = 0; i < RAMFS_MAX_INODES; i++) {
    if (!g_inodes[i].in_use) {
      memset(&g_inodes[i], 0, sizeof(g_inodes[i]));
      g_inodes[i].in_use = true;
      if (i >= g_inode_count) g_inode_count = i + 1u;
      return i;
    }
  }
  return INVALID_INODE;
}

/* Find a direct child of parent_idx with the given name component. */
static uint32_t inode_find_child(uint32_t parent_idx,
                                 const char *comp, size_t comp_len) {
  for (uint32_t i = 0; i < g_inode_count; i++) {
    if (!g_inodes[i].in_use) continue;
    if (g_inodes[i].parent_idx != parent_idx) continue;
    if (i == ROOT_INODE && parent_idx == ROOT_INODE) continue; /* skip root self-ref */
    size_t nlen = strlen(g_inodes[i].name);
    if (nlen == comp_len && memcmp(g_inodes[i].name, comp, comp_len) == 0)
      return i;
  }
  return INVALID_INODE;
}

/* Resolve an absolute path (after the "ramfs:" prefix) to an inode index.
 * Returns INVALID_INODE if any component is not found. */
static uint32_t path_to_inode(const char *path, size_t path_len) {
  uint32_t cur = ROOT_INODE;
  const char *p   = path;
  const char *end = path + path_len;

  /* Skip leading slash. */
  if (p < end && *p == '/') p++;

  while (p < end) {
    /* Find next '/' or end. */
    const char *sep = p;
    while (sep < end && *sep != '/') sep++;
    size_t comp_len = (size_t)(sep - p);
    if (comp_len == 0) { p = sep + 1; continue; } /* double slash */

    /* "." is self, ".." could be added later; skip for now */
    uint32_t child = inode_find_child(cur, p, comp_len);
    if (child == INVALID_INODE) return INVALID_INODE;
    cur = child;
    p = (*sep == '/') ? sep + 1 : sep;
  }
  return cur;
}

/* Extract the leaf name from a path (last component). */
static const char *path_leaf(const char *path, size_t path_len, size_t *leaf_len) {
  const char *end  = path + path_len;
  const char *leaf = path;
  for (const char *p = path; p < end; p++) {
    if (*p == '/') leaf = p + 1;
  }
  *leaf_len = (size_t)(end - leaf);
  return leaf;
}

/* Resolve path up to the parent directory. Returns parent inode or INVALID_INODE. */
static uint32_t path_to_parent(const char *path, size_t path_len) {
  size_t leaf_len;
  const char *leaf = path_leaf(path, path_len, &leaf_len);
  size_t parent_len = (size_t)(leaf - path);
  if (parent_len == 0) return ROOT_INODE; /* no parent component = root */
  return path_to_inode(path, parent_len);
}

/* Grow a file's data buffer to at least new_size bytes. Returns 0 on success. */
static int inode_grow(ramfs_inode_t *in, uint32_t new_size) {
  if (new_size <= in->data_cap) return 0;
  uint32_t new_cap = (new_size + PAGE_SIZE_U - 1u) & ~(PAGE_SIZE_U - 1u);
  uint8_t *new_data = bump_alloc(new_cap);
  if (!new_data) return -1;
  if (in->data && in->size > 0) memcpy(new_data, in->data, in->size);
  in->data     = new_data;
  in->data_cap = new_cap;
  return 0;
}

/* ---- open file descriptor table ---- */
typedef struct {
  uint32_t inode_idx;
  uint32_t cursor;
  bool     in_use;
} ramfs_fd_t;

static ramfs_fd_t g_fds[RAMFS_MAX_FDS];

static uint32_t fd_alloc(uint32_t inode_idx) {
  for (uint32_t i = 0; i < RAMFS_MAX_FDS; i++) {
    if (!g_fds[i].in_use) {
      g_fds[i].inode_idx = inode_idx;
      g_fds[i].cursor    = 0u;
      g_fds[i].in_use    = true;
      return i;
    }
  }
  return INVALID_INODE;
}

/* Object ID encoding: fd_index + 1 (0 is invalid) */
#define FD_TO_OID(fd)   ((uint64_t)(fd) + 1u)
#define OID_TO_FD(oid)  ((uint32_t)((oid) - 1u))

/* ---- helpers ---- */

static int starts_with(const char *buf, size_t len, const char *prefix) {
  size_t plen = strlen(prefix);
  return len >= plen && memcmp(buf, prefix, plen) == 0;
}

/* ---- IPC_OP_OPEN ---- */

static void handle_open(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  sys_open_reply_t oreply;
  memset(&reply,  0, sizeof(reply));
  memset(&oreply, 0, sizeof(oreply));

  /* data = "ramfs:<path>" */
  const char *data = (const char *)req->data;
  size_t dlen = req->num_bytes;

  /* Strip "ramfs:" prefix. */
  if (!starts_with(data, dlen, "ramfs:")) {
    sys_reply(&reply);
    return;
  }
  const char *path     = data + 6; /* after "ramfs:" */
  size_t      path_len = dlen  - 6;

  uint32_t flags = req->flags;
  bool creat     = (flags & FS_O_CREAT) != 0;
  bool is_dir    = (flags & FS_O_DIRECTORY) != 0;

  uint32_t inode_idx = path_to_inode(path, path_len);

  if (inode_idx == INVALID_INODE) {
    if (!creat) { sys_reply(&reply); return; }

    /* Create: resolve parent, allocate new inode. */
    uint32_t parent = path_to_parent(path, path_len);
    if (parent == INVALID_INODE) { sys_reply(&reply); return; }

    size_t leaf_len;
    const char *leaf = path_leaf(path, path_len, &leaf_len);
    if (leaf_len == 0 || leaf_len >= RAMFS_NAME_MAX) { sys_reply(&reply); return; }

    inode_idx = inode_alloc();
    if (inode_idx == INVALID_INODE) { sys_reply(&reply); return; }

    memcpy(g_inodes[inode_idx].name, leaf, leaf_len);
    g_inodes[inode_idx].name[leaf_len] = '\0';
    g_inodes[inode_idx].type           = is_dir ? FS_TYPE_DIR : FS_TYPE_FILE;
    g_inodes[inode_idx].parent_idx     = parent;
    g_inodes[inode_idx].size           = 0u;
    g_inodes[inode_idx].data           = NULL;
    g_inodes[inode_idx].data_cap       = 0u;
  }

  /* Check type vs flags. */
  if (is_dir && g_inodes[inode_idx].type != FS_TYPE_DIR) {
    sys_reply(&reply); return;
  }

  /* Truncate if requested. */
  if ((flags & FS_O_TRUNC) && g_inodes[inode_idx].type == FS_TYPE_FILE) {
    g_inodes[inode_idx].size = 0u;
  }

  uint32_t fd = fd_alloc(inode_idx);
  if (fd == INVALID_INODE) { sys_reply(&reply); return; }

  if (g_inodes[inode_idx].type == FS_TYPE_FILE) {
    oreply.allowed_ops = KOP_READ | KOP_WRITE | KOP_CALL | KOP_CLOSE | KOP_EXEC;
  } else {
    oreply.allowed_ops = KOP_CALL | KOP_CLOSE;
  }

  reply.object_id = FD_TO_OID(fd);
  reply.num_bytes = sizeof(oreply);
  memcpy(reply.data, &oreply, sizeof(oreply));
  sys_reply(&reply);
}

/* ---- IPC_OP_READ ---- */

static void handle_read(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  uint32_t fd_idx = OID_TO_FD(req->object_id);
  if (fd_idx >= RAMFS_MAX_FDS || !g_fds[fd_idx].in_use) {
    sys_reply(&reply); return;
  }
  ramfs_fd_t    *fd = &g_fds[fd_idx];
  ramfs_inode_t *in = &g_inodes[fd->inode_idx];

  if (in->type != FS_TYPE_FILE) { sys_reply(&reply); return; }

  uint32_t requested = 0;
  if (req->num_bytes >= 4) memcpy(&requested, req->data, 4);
  if (requested > 256u) requested = 256u;

  uint32_t avail = (fd->cursor < in->size) ? (in->size - fd->cursor) : 0u;
  uint32_t n     = (requested < avail) ? requested : avail;

  if (n > 0 && in->data) memcpy(reply.data, in->data + fd->cursor, n);
  fd->cursor  += n;
  reply.num_bytes = n;
  sys_reply(&reply);
}

/* ---- IPC_OP_WRITE ---- */

static void handle_write(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  uint32_t written = 0;
  memset(&reply, 0, sizeof(reply));

  uint32_t fd_idx = OID_TO_FD(req->object_id);
  if (fd_idx >= RAMFS_MAX_FDS || !g_fds[fd_idx].in_use) goto done;

  ramfs_fd_t    *fd = &g_fds[fd_idx];
  ramfs_inode_t *in = &g_inodes[fd->inode_idx];

  if (in->type != FS_TYPE_FILE) goto done;

  uint32_t n = req->num_bytes;
  if (n == 0) goto done;

  uint32_t new_end = fd->cursor + n;
  if (new_end > in->data_cap) {
    if (inode_grow(in, new_end) != 0) goto done;
  }
  memcpy(in->data + fd->cursor, req->data, n);
  fd->cursor += n;
  if (fd->cursor > in->size) in->size = fd->cursor;
  written = n;

done:
  reply.num_bytes = sizeof(written);
  memcpy(reply.data, &written, sizeof(written));
  sys_reply(&reply);
}

/* ---- IPC_OP_CLOSE ---- */

static void handle_close(const sys_ipc_msg_t *req) {
  uint32_t fd_idx = OID_TO_FD(req->object_id);
  if (fd_idx < RAMFS_MAX_FDS && g_fds[fd_idx].in_use)
    g_fds[fd_idx].in_use = false;
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));
  sys_reply(&reply);
}

/* ---- FS_OP_SEEK ---- */

static void handle_seek(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  uint32_t fd_idx = OID_TO_FD(req->object_id);
  if (fd_idx >= RAMFS_MAX_FDS || !g_fds[fd_idx].in_use) {
    sys_reply(&reply); return;
  }
  ramfs_fd_t    *fd = &g_fds[fd_idx];
  ramfs_inode_t *in = &g_inodes[fd->inode_idx];

  if (req->num_bytes < sizeof(fs_seek_req_t)) { sys_reply(&reply); return; }

  fs_seek_req_t sreq;
  memcpy(&sreq, req->data, sizeof(sreq));

  int32_t  base = 0;
  switch (sreq.whence) {
  case FS_SEEK_SET: base = 0;              break;
  case FS_SEEK_CUR: base = (int32_t)fd->cursor; break;
  case FS_SEEK_END: base = (int32_t)in->size;   break;
  default: sys_reply(&reply); return;
  }

  int32_t new_pos = base + sreq.offset;
  if (new_pos < 0) new_pos = 0;
  fd->cursor = (uint32_t)new_pos;

  fs_seek_rep_t srep = { .new_pos = fd->cursor };
  reply.num_bytes = sizeof(srep);
  memcpy(reply.data, &srep, sizeof(srep));
  sys_reply(&reply);
}

/* ---- FS_OP_STAT ---- */

static void handle_stat(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  uint32_t fd_idx = OID_TO_FD(req->object_id);
  if (fd_idx >= RAMFS_MAX_FDS || !g_fds[fd_idx].in_use) {
    sys_reply(&reply); return;
  }
  ramfs_inode_t *in = &g_inodes[g_fds[fd_idx].inode_idx];

  fs_stat_t st = { .size = in->size, .type = in->type };
  reply.num_bytes = sizeof(st);
  memcpy(reply.data, &st, sizeof(st));
  sys_reply(&reply);
}

/* ---- FS_OP_READDIR ---- */

static void handle_readdir(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  uint32_t fd_idx = OID_TO_FD(req->object_id);
  if (fd_idx >= RAMFS_MAX_FDS || !g_fds[fd_idx].in_use) {
    sys_reply(&reply); return;
  }
  ramfs_inode_t *dir_in = &g_inodes[g_fds[fd_idx].inode_idx];
  if (dir_in->type != FS_TYPE_DIR) { sys_reply(&reply); return; }

  uint32_t target_idx = 0;
  if (req->num_bytes >= 4) memcpy(&target_idx, req->data, 4);

  uint32_t dir_inode_idx = g_fds[fd_idx].inode_idx;
  uint32_t found = 0;
  for (uint32_t i = 0; i < g_inode_count; i++) {
    if (!g_inodes[i].in_use) continue;
    if (i == ROOT_INODE && dir_inode_idx == ROOT_INODE) continue;
    if (g_inodes[i].parent_idx != dir_inode_idx) continue;
    if (i == dir_inode_idx) continue; /* skip self */
    if (found == target_idx) {
      fs_dirent_t de;
      memset(&de, 0, sizeof(de));
      size_t nlen = strlen(g_inodes[i].name);
      if (nlen >= sizeof(de.name)) nlen = sizeof(de.name) - 1u;
      memcpy(de.name, g_inodes[i].name, nlen);
      de.name[nlen] = '\0';
      de.type = g_inodes[i].type;
      reply.num_bytes = sizeof(de);
      memcpy(reply.data, &de, sizeof(de));
      sys_reply(&reply);
      return;
    }
    found++;
  }
  /* Index out of range — reply with empty (num_bytes == 0 signals end). */
  sys_reply(&reply);
}

/* ---- FS_OP_MKDIR ---- */

static void handle_mkdir(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  uint32_t fd_idx = OID_TO_FD(req->object_id);
  if (fd_idx >= RAMFS_MAX_FDS || !g_fds[fd_idx].in_use) {
    sys_reply(&reply); return;
  }
  uint32_t dir_inode = g_fds[fd_idx].inode_idx;
  if (g_inodes[dir_inode].type != FS_TYPE_DIR) { sys_reply(&reply); return; }

  size_t name_len = req->num_bytes;
  if (name_len == 0 || name_len >= RAMFS_NAME_MAX) { sys_reply(&reply); return; }

  /* Check if name already exists. */
  if (inode_find_child(dir_inode, (const char *)req->data, name_len) != INVALID_INODE) {
    sys_reply(&reply); return;
  }

  uint32_t new_idx = inode_alloc();
  if (new_idx == INVALID_INODE) { sys_reply(&reply); return; }

  memcpy(g_inodes[new_idx].name, req->data, name_len);
  g_inodes[new_idx].name[name_len] = '\0';
  g_inodes[new_idx].type           = FS_TYPE_DIR;
  g_inodes[new_idx].parent_idx     = dir_inode;

  int32_t result = 0;
  reply.num_bytes = sizeof(result);
  memcpy(reply.data, &result, sizeof(result));
  sys_reply(&reply);
}

/* ---- FS_OP_UNLINK ---- */

static void handle_unlink(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  uint32_t fd_idx = OID_TO_FD(req->object_id);
  if (fd_idx >= RAMFS_MAX_FDS || !g_fds[fd_idx].in_use) {
    sys_reply(&reply); return;
  }
  uint32_t dir_inode = g_fds[fd_idx].inode_idx;
  if (g_inodes[dir_inode].type != FS_TYPE_DIR) { sys_reply(&reply); return; }

  size_t name_len = req->num_bytes;
  uint32_t target = inode_find_child(dir_inode, (const char *)req->data, name_len);
  if (target != INVALID_INODE) g_inodes[target].in_use = false;

  sys_reply(&reply);
}

/* ---- FS_OP_RENAME ---- */

static void handle_rename(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  uint32_t fd_idx = OID_TO_FD(req->object_id);
  if (fd_idx >= RAMFS_MAX_FDS || !g_fds[fd_idx].in_use) {
    sys_reply(&reply); return;
  }
  uint32_t dir_inode = g_fds[fd_idx].inode_idx;
  if (g_inodes[dir_inode].type != FS_TYPE_DIR) { sys_reply(&reply); return; }

  if (req->num_bytes < sizeof(fs_rename_req_t)) { sys_reply(&reply); return; }

  fs_rename_req_t rreq;
  memcpy(&rreq, req->data, sizeof(rreq));
  rreq.old_name[127] = '\0';
  rreq.new_name[127] = '\0';

  uint32_t target = inode_find_child(dir_inode, rreq.old_name, strlen(rreq.old_name));
  if (target == INVALID_INODE) { sys_reply(&reply); return; }

  size_t new_len = strlen(rreq.new_name);
  if (new_len == 0 || new_len >= RAMFS_NAME_MAX) { sys_reply(&reply); return; }

  memcpy(g_inodes[target].name, rreq.new_name, new_len + 1u);
  sys_reply(&reply);
}

/* ---- FS_OP_TRUNCATE ---- */

static void handle_truncate(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  uint32_t fd_idx = OID_TO_FD(req->object_id);
  if (fd_idx >= RAMFS_MAX_FDS || !g_fds[fd_idx].in_use) {
    sys_reply(&reply); return;
  }
  ramfs_inode_t *in = &g_inodes[g_fds[fd_idx].inode_idx];
  if (in->type != FS_TYPE_FILE) { sys_reply(&reply); return; }

  uint32_t new_size = 0;
  if (req->num_bytes >= 4) memcpy(&new_size, req->data, 4);

  if (new_size > in->data_cap) {
    if (inode_grow(in, new_size) != 0) { sys_reply(&reply); return; }
  }
  if (new_size > in->size) {
    /* Zero the extension. */
    memset(in->data + in->size, 0, new_size - in->size);
  }
  in->size = new_size;
  sys_reply(&reply);
}

/* ---- unknown ---- */

static void handle_unknown(void) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));
  sys_reply(&reply);
}

/* ---- entry point ---- */

void _start(void) {
  cap_handle_t my_ep = sys_bootstrap_cap(0);

  inodes_init();
  memset(g_fds, 0, sizeof(g_fds));

  for (;;) {
    sys_ipc_msg_t req;
    memset(&req, 0, sizeof(req));
    if (sys_recv(my_ep, &req) != 0) continue;

    sys_putc('r'); sys_putc('=');
    { uint32_t op = req.opcode; char buf[9]; int i=7; buf[8]=0;
      while(i>=0){buf[i--]="0123456789abcdef"[op&0xF];op>>=4;} sys_putc(buf[0]); }
    sys_putc('\n');

    switch (req.opcode) {
    case IPC_OP_OPEN:  handle_open(&req);  break;
    case IPC_OP_READ:  handle_read(&req);  break;
    case IPC_OP_WRITE: handle_write(&req); break;
    case IPC_OP_CLOSE: handle_close(&req); break;

    default:
      /* Custom FS ops dispatched by opcode, regardless of object_id */
      switch (req.opcode) {
      case FS_OP_SEEK:     handle_seek(&req);     break;
      case FS_OP_STAT:     sys_putc('!'); handle_stat(&req);     break;
      case FS_OP_READDIR:  handle_readdir(&req);  break;
      case FS_OP_MKDIR:    handle_mkdir(&req);    break;
      case FS_OP_UNLINK:   handle_unlink(&req);   break;
      case FS_OP_RENAME:   handle_rename(&req);   break;
      case FS_OP_TRUNCATE: handle_truncate(&req); break;
      default:             handle_unknown();       break;
      }
      break;
    }
  }
}
