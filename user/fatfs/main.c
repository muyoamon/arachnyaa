/*
 * fatfs — FAT16 filesystem service
 *
 * Protocol: fatfs:
 * Endpoint: bootstrap cap slot 0
 *
 * A VFS backend (see ../vfs/fs_proto.h): structurally a sibling of ramfs, but
 * file data lives on a block device reached through a blk: handle rather than in
 * RAM.  Sectors move through a shared bounce buffer established with the device
 * via BLK_OP_ATTACH (see ../blk/blk_proto.h).
 *
 * Scope: read + write (open/read/write/close, seek/stat/readdir, create via
 * FS_O_CREAT, truncate, mkdir, unlink), 8.3 short names, read-through with no
 * caching.  Superfloppy layout (BPB at LBA 0, no MBR).  Both FAT copies are
 * kept in sync on every FAT update.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../ulib/syscall.h"
#include "../ulib/string.h"
#include "../vfs/fs_proto.h"
#include "../blk/blk_proto.h"

/* ---- limits ---- */
#define FATFS_MAX_FDS 64
#define FATFS_BUF_VBASE 0x28000000u  /* shared bounce buffer (1 page) */

/* ---- block device + shared buffer ---- */
static cap_handle_t g_blk = 0;
static uint8_t     *g_buf = NULL;   /* shared bounce buffer, 1 sector used */

/* ---- parsed BPB / geometry (sectors) ---- */
static uint32_t g_fat_start;
static uint32_t g_num_fats;
static uint32_t g_fat_sectors;   /* sectors per FAT copy */
static uint32_t g_root_start;
static uint32_t g_root_sectors;
static uint32_t g_data_start;
static uint32_t g_total_sectors;
static uint32_t g_spc;           /* sectors per cluster */
static uint32_t g_cluster_bytes;
static uint32_t g_max_cluster;   /* highest valid cluster number + 1 */
static bool     g_mounted = false;

#define FAT_EOC       0xFFF8u   /* >= this = end of chain */
#define FAT_EOC_MARK  0xFFFFu   /* value written to mark end of chain */
#define DIRENT_SIZE   32u
#define ATTR_DIR      0x10u
#define ATTR_VOLUME   0x08u
#define ATTR_LFN      0x0Fu

/* ---- little-endian readers over g_buf ---- */
static uint16_t rd16(uint32_t off) { return (uint16_t)(g_buf[off] | (g_buf[off + 1] << 8)); }
/* rd32 over an arbitrary byte pointer (e.g. a copied dirent buffer). */
static uint32_t rd32_bytes(const uint8_t *b) {
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
         ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

/* ---- block IO through the shared buffer ---- */
static int blk_io(uint32_t lba, uint16_t count, bool is_write) {
  sys_ipc_msg_t req, rep;
  blk_io_req_t  io = { .lba = lba, .count = count };
  memset(&req, 0, sizeof(req));
  memset(&rep, 0, sizeof(rep));
  req.opcode    = is_write ? BLK_OP_WRITE : BLK_OP_READ;
  req.num_bytes = sizeof(io);
  memcpy(req.data, &io, sizeof(io));
  if (sys_call(g_blk, &req, &rep) != 0) return -1;
  int32_t st = -1;
  if (rep.num_bytes >= sizeof(st)) memcpy(&st, rep.data, sizeof(st));
  return st;
}
#define blk_read(lba)  blk_io((lba), 1, false)
#define blk_write(lba) blk_io((lba), 1, true)

static uint32_t cluster_to_lba(uint16_t cl) {
  return g_data_start + (uint32_t)(cl - 2u) * g_spc;
}

/* Read the FAT entry for `cl`. Clobbers g_buf. */
static uint16_t fat_next(uint16_t cl) {
  uint32_t foff = (uint32_t)cl * 2u;
  uint32_t fsec = g_fat_start + foff / BLK_SECTOR_SIZE;
  uint32_t eoff = foff % BLK_SECTOR_SIZE;
  if (blk_read(fsec) != 0) return FAT_EOC;
  return rd16(eoff);
}

/* Write the FAT entry for `cl` to all FAT copies. Clobbers g_buf. */
static void fat_set(uint16_t cl, uint16_t val) {
  uint32_t foff    = (uint32_t)cl * 2u;
  uint32_t sec_off = foff / BLK_SECTOR_SIZE;
  uint32_t eoff    = foff % BLK_SECTOR_SIZE;
  for (uint32_t f = 0; f < g_num_fats; f++) {
    uint32_t sec = g_fat_start + f * g_fat_sectors + sec_off;
    if (blk_read(sec) != 0) return;
    g_buf[eoff]     = (uint8_t)(val & 0xFF);
    g_buf[eoff + 1] = (uint8_t)(val >> 8);
    blk_write(sec);
  }
}

/* Zero all data sectors of a cluster. Clobbers g_buf. */
static void zero_cluster(uint16_t cl) {
  memset(g_buf, 0, BLK_SECTOR_SIZE);
  uint32_t lba = cluster_to_lba(cl);
  for (uint32_t s = 0; s < g_spc; s++) blk_write(lba + s);
}

/* Allocate a free cluster: scan the FAT for a 0x0000 entry, mark it EOC, zero
 * its data.  Returns the cluster number, or 0 if the volume is full. */
static uint16_t alloc_cluster(void) {
  for (uint32_t sec = 0; sec < g_fat_sectors; sec++) {
    if (blk_read(g_fat_start + sec) != 0) return 0;
    for (uint32_t e = 0; e < BLK_SECTOR_SIZE / 2u; e++) {
      uint32_t cl = sec * (BLK_SECTOR_SIZE / 2u) + e;
      if (cl < 2u) continue;
      if (cl >= g_max_cluster) return 0;
      if (rd16(e * 2u) == 0x0000u) {
        fat_set((uint16_t)cl, FAT_EOC_MARK);
        zero_cluster((uint16_t)cl);
        return (uint16_t)cl;
      }
    }
  }
  return 0;
}

/* Free an entire cluster chain (mark every entry 0x0000). Clobbers g_buf. */
static void free_chain(uint16_t first) {
  uint16_t cl = first;
  while (cl >= 2u && cl < FAT_EOC) {
    uint16_t nxt = fat_next(cl);
    fat_set(cl, 0x0000);
    cl = nxt;
  }
}

/* ---- 8.3 name helpers ---- */
static char up(char c) { return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c; }

/* Does an 8.3 raw name (11 bytes, space-padded) match path component [comp,len)? */
static bool name_matches(const uint8_t raw[11], const char *comp, size_t len) {
  /* Build normalized "NAME.EXT" from raw. */
  char norm[13];
  size_t n = 0;
  for (int i = 0; i < 8 && raw[i] != ' '; i++) norm[n++] = (char)raw[i];
  if (raw[8] != ' ') {
    norm[n++] = '.';
    for (int i = 8; i < 11 && raw[i] != ' '; i++) norm[n++] = (char)raw[i];
  }
  norm[n] = '\0';
  if (n != len) return false;
  for (size_t i = 0; i < len; i++)
    if (up(norm[i]) != up(comp[i])) return false;
  return true;
}

/* Copy 8.3 raw name into a printable "name.ext" (lowercased). */
static void name_to_str(const uint8_t raw[11], char *out, size_t out_sz) {
  size_t n = 0;
  for (int i = 0; i < 8 && raw[i] != ' ' && n + 1 < out_sz; i++)
    out[n++] = (char)(raw[i] >= 'A' && raw[i] <= 'Z' ? raw[i] + 32 : raw[i]);
  if (raw[8] != ' ' && n + 1 < out_sz) {
    out[n++] = '.';
    for (int i = 8; i < 11 && raw[i] != ' ' && n + 1 < out_sz; i++)
      out[n++] = (char)(raw[i] >= 'A' && raw[i] <= 'Z' ? raw[i] + 32 : raw[i]);
  }
  out[n] = '\0';
}

/* ---- directory iteration ----
 * Read raw 32-byte entry `index` of a directory (root or subdir) into out[32].
 * Also returns the on-disk location (lba, byte offset) for write-back.
 * Returns 1 if the entry slot exists, 0 if `index` is beyond the directory. */
static int dir_get_entry(bool is_root, uint16_t first_cluster, uint32_t index,
                         uint8_t out[DIRENT_SIZE],
                         uint32_t *ent_lba, uint16_t *ent_off) {
  uint32_t lba, off;
  if (is_root) {
    uint32_t per_sec = BLK_SECTOR_SIZE / DIRENT_SIZE; /* 16 */
    uint32_t sec_ord = index / per_sec;
    if (sec_ord >= g_root_sectors) return 0;
    lba = g_root_start + sec_ord;
    off = (index % per_sec) * DIRENT_SIZE;
  } else {
    uint32_t per_clu = g_cluster_bytes / DIRENT_SIZE;
    uint32_t clu_ord = index / per_clu;
    uint16_t cl = first_cluster;
    for (uint32_t i = 0; i < clu_ord; i++) {
      cl = fat_next(cl);
      if (cl < 2u || cl >= FAT_EOC) return 0;
    }
    uint32_t e = index % per_clu;
    lba = cluster_to_lba(cl) + (e * DIRENT_SIZE) / BLK_SECTOR_SIZE;
    off = (e * DIRENT_SIZE) % BLK_SECTOR_SIZE;
  }
  if (blk_read(lba) != 0) return 0;
  memcpy(out, g_buf + off, DIRENT_SIZE);
  if (ent_lba) *ent_lba = lba;
  if (ent_off) *ent_off = (uint16_t)off;
  return 1;
}

/* Is a raw entry a real, listable file/dir? (not end/deleted/lfn/volume) */
static int entry_is_regular(const uint8_t e[DIRENT_SIZE]) {
  if (e[0] == 0x00) return -1;            /* end of directory */
  if (e[0] == 0xE5) return 0;             /* deleted */
  if (e[11] == ATTR_LFN) return 0;        /* long-name component */
  if (e[11] & ATTR_VOLUME) return 0;      /* volume label */
  return 1;
}

/* Find a child named [comp,len) in a directory. Fills entry + location. */
static bool dir_find(bool is_root, uint16_t first_cluster,
                     const char *comp, size_t len,
                     uint8_t out[DIRENT_SIZE], uint32_t *ent_lba, uint16_t *ent_off) {
  for (uint32_t idx = 0;; idx++) {
    uint8_t e[DIRENT_SIZE];
    uint32_t lba; uint16_t off;
    if (!dir_get_entry(is_root, first_cluster, idx, e, &lba, &off)) return false;
    int r = entry_is_regular(e);
    if (r < 0) return false;   /* end of directory */
    if (r == 0) continue;
    if (name_matches(e, comp, len)) {
      memcpy(out, e, DIRENT_SIZE);
      if (ent_lba) *ent_lba = lba;
      if (ent_off) *ent_off = off;
      return true;
    }
  }
}

/* ---- resolved entry ---- */
typedef struct {
  uint16_t first_cluster;
  uint32_t size;
  bool     is_dir;
  bool     is_root;
  uint32_t dirent_lba;
  uint16_t dirent_off;
} fat_entry_t;

/* Resolve an absolute path (after "fatfs:") to an entry. */
static bool resolve_path(const char *path, size_t len, fat_entry_t *out) {
  /* Root directory. */
  const char *p = path, *end = path + len;
  if (p < end && *p == '/') p++;
  if (p >= end) {
    out->first_cluster = 0; out->size = 0;
    out->is_dir = true; out->is_root = true;
    out->dirent_lba = 0; out->dirent_off = 0;
    return true;
  }

  bool     cur_root    = true;
  uint16_t cur_cluster = 0;
  while (p < end) {
    const char *sep = p;
    while (sep < end && *sep != '/') sep++;
    size_t clen = (size_t)(sep - p);
    if (clen == 0) { p = sep + 1; continue; }

    uint8_t e[DIRENT_SIZE];
    uint32_t lba; uint16_t off;
    if (!dir_find(cur_root, cur_cluster, p, clen, e, &lba, &off)) return false;

    uint16_t first = (uint16_t)(e[26] | (e[27] << 8));
    bool     isdir = (e[11] & ATTR_DIR) != 0;

    const char *next = (*sep == '/') ? sep + 1 : sep;
    bool last = (next >= end);
    if (!last) {
      if (!isdir) return false;   /* path component is not a directory */
      cur_root = false; cur_cluster = first;
      p = next;
      continue;
    }
    out->first_cluster = first;
    out->size          = rd32_bytes(e + 28);
    out->is_dir        = isdir;
    out->is_root       = false;
    out->dirent_lba    = lba;
    out->dirent_off    = off;
    return true;
  }
  return false;
}

/* ---- open FD table ---- */
typedef struct {
  uint16_t first_cluster;
  uint32_t size;
  uint32_t cursor;
  bool     is_dir;
  bool     is_root;
  uint32_t dirent_lba;
  uint16_t dirent_off;
  bool     dirty;   /* dir entry (size/first_cluster) needs write-back on close */
  bool     in_use;
} fatfs_fd_t;

static fatfs_fd_t g_fds[FATFS_MAX_FDS];

#define FD_TO_OID(fd)  ((uint64_t)(fd) + 1u)
#define OID_TO_FD(oid) ((uint32_t)((oid) - 1u))

static uint32_t fd_alloc(void) {
  for (uint32_t i = 0; i < FATFS_MAX_FDS; i++)
    if (!g_fds[i].in_use) { memset(&g_fds[i], 0, sizeof(g_fds[i])); g_fds[i].in_use = true; return i; }
  return (uint32_t)-1;
}

static int starts_with(const char *buf, size_t len, const char *prefix) {
  size_t plen = strlen(prefix);
  return len >= plen && memcmp(buf, prefix, plen) == 0;
}

/* ---- write-path helpers ---- */

/* Write an fd's size + first_cluster back to its directory entry. */
static void flush_dirent(fatfs_fd_t *f) {
  if (!f->dirty || f->is_root || f->dirent_lba == 0) { f->dirty = false; return; }
  if (blk_read(f->dirent_lba) != 0) return;
  uint8_t *e = g_buf + f->dirent_off;
  e[26] = (uint8_t)(f->first_cluster & 0xFF);
  e[27] = (uint8_t)(f->first_cluster >> 8);
  e[28] = (uint8_t)(f->size & 0xFF);
  e[29] = (uint8_t)(f->size >> 8);
  e[30] = (uint8_t)(f->size >> 16);
  e[31] = (uint8_t)(f->size >> 24);
  blk_write(f->dirent_lba);
  f->dirty = false;
}

/* Return the cluster covering byte `offset` of file `f`, extending (allocating
 * + linking) the chain when `allocate` is set. Returns 0 on failure/full. */
static uint16_t cluster_for_offset(fatfs_fd_t *f, uint32_t offset, bool allocate) {
  uint32_t clu_ord = offset / g_cluster_bytes;
  if (f->first_cluster == 0) {
    if (!allocate) return 0;
    uint16_t nc = alloc_cluster();
    if (nc == 0) return 0;
    f->first_cluster = nc;
    f->dirty = true;
  }
  uint16_t cl = f->first_cluster;
  for (uint32_t i = 0; i < clu_ord; i++) {
    uint16_t nxt = fat_next(cl);
    if (nxt < 2u || nxt >= FAT_EOC) {
      if (!allocate) return 0;
      uint16_t nc = alloc_cluster();
      if (nc == 0) return 0;
      fat_set(cl, nc);   /* link; alloc_cluster already marked nc as EOC */
      cl = nc;
    } else {
      cl = nxt;
    }
  }
  return cl;
}

/* Build an 11-byte space-padded 8.3 name from a path component. Returns false
 * if the name has no basename. */
static bool make_8_3(const char *name, size_t len, uint8_t out[11]) {
  memset(out, ' ', 11);
  size_t dot = len;
  for (size_t i = 0; i < len; i++) if (name[i] == '.') dot = i;
  size_t nlen = (dot < len) ? dot : len;
  size_t elen = (dot < len) ? (len - dot - 1u) : 0u;
  const char *ext = (dot < len) ? name + dot + 1u : NULL;
  if (nlen == 0) return false;
  if (nlen > 8) nlen = 8;
  if (elen > 3) elen = 3;
  for (size_t i = 0; i < nlen; i++) out[i]     = (uint8_t)up(name[i]);
  for (size_t i = 0; i < elen; i++) out[8 + i] = (uint8_t)up(ext[i]);
  return true;
}

/* Resolve a path's parent directory + leaf component. */
static bool resolve_parent(const char *path, size_t len, bool *p_root,
                           uint16_t *p_cluster, const char **leaf, size_t *leaf_len) {
  const char *end = path + len;
  const char *slash = NULL;
  for (const char *q = path; q < end; q++) if (*q == '/') slash = q;

  if (!slash) {                     /* bare name → parent is root */
    *p_root = true; *p_cluster = 0; *leaf = path; *leaf_len = len; return true;
  }
  *leaf = slash + 1; *leaf_len = (size_t)(end - (slash + 1));
  if (*leaf_len == 0) return false; /* trailing slash */
  if (slash == path) { *p_root = true; *p_cluster = 0; return true; }

  fat_entry_t pe;
  if (!resolve_path(path, (size_t)(slash - path), &pe) || !pe.is_dir) return false;
  *p_root = pe.is_root; *p_cluster = pe.first_cluster;
  return true;
}

/* Insert a directory entry into a parent directory. Returns its on-disk
 * location. Fails if the directory has no free slot (no extension for now). */
static bool dir_add_entry(bool p_root, uint16_t p_cluster, const uint8_t name83[11],
                          uint8_t attr, uint16_t first_cluster, uint32_t size,
                          uint32_t *out_lba, uint16_t *out_off) {
  for (uint32_t idx = 0;; idx++) {
    uint8_t e[DIRENT_SIZE];
    uint32_t lba; uint16_t off;
    if (!dir_get_entry(p_root, p_cluster, idx, e, &lba, &off)) return false;
    if (e[0] != 0x00 && e[0] != 0xE5) continue;

    uint8_t ne[DIRENT_SIZE];
    memset(ne, 0, sizeof(ne));
    memcpy(ne, name83, 11);
    ne[11] = attr;
    ne[26] = (uint8_t)(first_cluster & 0xFF);
    ne[27] = (uint8_t)(first_cluster >> 8);
    ne[28] = (uint8_t)(size & 0xFF);
    ne[29] = (uint8_t)(size >> 8);
    ne[30] = (uint8_t)(size >> 16);
    ne[31] = (uint8_t)(size >> 24);

    if (blk_read(lba) != 0) return false;
    memcpy(g_buf + off, ne, DIRENT_SIZE);
    if (blk_write(lba) != 0) return false;
    if (out_lba) *out_lba = lba;
    if (out_off) *out_off = off;
    return true;
  }
}

/* ---- IPC_OP_OPEN ---- */
static void handle_open(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t    reply;
  sys_open_reply_t oreply;
  memset(&reply,  0, sizeof(reply));
  memset(&oreply, 0, sizeof(oreply));

  const char *data = (const char *)req->data;
  size_t      dlen = req->num_bytes;
  if (!starts_with(data, dlen, "fatfs:") || !g_mounted) { sys_reply(&reply); return; }
  const char *path     = data + 6;
  size_t      path_len = dlen  - 6;

  fat_entry_t fe;
  bool dirty = false;
  if (!resolve_path(path, path_len, &fe)) {
    /* Create the file if requested. */
    if (!(req->flags & FS_O_CREAT)) { sys_reply(&reply); return; }
    bool p_root; uint16_t p_cluster; const char *leaf; size_t leaf_len;
    if (!resolve_parent(path, path_len, &p_root, &p_cluster, &leaf, &leaf_len)) {
      sys_reply(&reply); return;
    }
    uint8_t name83[11];
    if (!make_8_3(leaf, leaf_len, name83)) { sys_reply(&reply); return; }
    uint32_t d_lba; uint16_t d_off;
    if (!dir_add_entry(p_root, p_cluster, name83, 0x00, 0, 0, &d_lba, &d_off)) {
      sys_reply(&reply); return;
    }
    fe.first_cluster = 0; fe.size = 0; fe.is_dir = false; fe.is_root = false;
    fe.dirent_lba = d_lba; fe.dirent_off = d_off;
  }

  if ((req->flags & FS_O_DIRECTORY) && !fe.is_dir) { sys_reply(&reply); return; }

  /* Truncate to zero if requested (free the chain, zero the size). */
  if ((req->flags & FS_O_TRUNC) && !fe.is_dir && fe.first_cluster != 0) {
    free_chain(fe.first_cluster);
    fe.first_cluster = 0;
    fe.size          = 0;
    dirty            = true;
  }

  uint32_t fd = fd_alloc();
  if (fd == (uint32_t)-1) { sys_reply(&reply); return; }
  g_fds[fd].first_cluster = fe.first_cluster;
  g_fds[fd].size          = fe.size;
  g_fds[fd].cursor        = 0;
  g_fds[fd].is_dir        = fe.is_dir;
  g_fds[fd].is_root       = fe.is_root;
  g_fds[fd].dirent_lba    = fe.dirent_lba;
  g_fds[fd].dirent_off    = fe.dirent_off;
  g_fds[fd].dirty         = dirty;

  oreply.allowed_ops = fe.is_dir
      ? (KOP_CALL | KOP_CLOSE)
      : (KOP_READ | KOP_WRITE | KOP_CALL | KOP_CLOSE | KOP_EXEC);
  reply.object_id = FD_TO_OID(fd);
  reply.num_bytes = sizeof(oreply);
  memcpy(reply.data, &oreply, sizeof(oreply));
  sys_reply(&reply);
}

/* ---- IPC_OP_READ ---- */
static void handle_read(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  uint32_t fd = OID_TO_FD(req->object_id);
  if (fd >= FATFS_MAX_FDS || !g_fds[fd].in_use || g_fds[fd].is_dir) { sys_reply(&reply); return; }
  fatfs_fd_t *f = &g_fds[fd];

  uint32_t requested = 0;
  if (req->num_bytes >= 4) memcpy(&requested, req->data, 4);
  if (requested > 256u) requested = 256u;

  if (f->cursor >= f->size) { sys_reply(&reply); return; } /* EOF */

  /* Walk the cluster chain to the cluster covering the cursor. */
  uint32_t clu_ord = f->cursor / g_cluster_bytes;
  uint16_t cl = f->first_cluster;
  for (uint32_t i = 0; i < clu_ord; i++) {
    cl = fat_next(cl);
    if (cl < 2u || cl >= FAT_EOC) { sys_reply(&reply); return; }
  }
  uint32_t off_in_clu = f->cursor % g_cluster_bytes;
  uint32_t sec_in_clu = off_in_clu / BLK_SECTOR_SIZE;
  uint32_t off_in_sec = off_in_clu % BLK_SECTOR_SIZE;

  if (blk_read(cluster_to_lba(cl) + sec_in_clu) != 0) { sys_reply(&reply); return; }

  /* Return up to a sector boundary, the request, and remaining size. */
  uint32_t avail_sec = BLK_SECTOR_SIZE - off_in_sec;
  uint32_t avail_file = f->size - f->cursor;
  uint32_t n = requested;
  if (n > avail_sec)  n = avail_sec;
  if (n > avail_file) n = avail_file;

  memcpy(reply.data, g_buf + off_in_sec, n);
  f->cursor += n;
  reply.num_bytes = n;
  sys_reply(&reply);
}

/* ---- IPC_OP_WRITE ---- */
static void handle_write(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  uint32_t written = 0;
  memset(&reply, 0, sizeof(reply));

  uint32_t fd = OID_TO_FD(req->object_id);
  if (fd >= FATFS_MAX_FDS || !g_fds[fd].in_use || g_fds[fd].is_dir) goto done;
  fatfs_fd_t *f = &g_fds[fd];

  uint32_t remaining = req->num_bytes;
  if (remaining > 256u) remaining = 256u;
  const uint8_t *src = req->data;

  while (remaining > 0) {
    uint16_t cl = cluster_for_offset(f, f->cursor, true);
    if (cl == 0) break; /* volume full */

    uint32_t off_in_clu = f->cursor % g_cluster_bytes;
    uint32_t sec_in_clu = off_in_clu / BLK_SECTOR_SIZE;
    uint32_t off_in_sec = off_in_clu % BLK_SECTOR_SIZE;
    uint32_t chunk      = BLK_SECTOR_SIZE - off_in_sec;
    if (chunk > remaining) chunk = remaining;

    uint32_t lba = cluster_to_lba(cl) + sec_in_clu;
    if (blk_read(lba) != 0) break;               /* read-modify-write */
    memcpy(g_buf + off_in_sec, src, chunk);
    if (blk_write(lba) != 0) break;

    f->cursor += chunk; src += chunk; remaining -= chunk; written += chunk;
    if (f->cursor > f->size) { f->size = f->cursor; f->dirty = true; }
  }

done:
  reply.num_bytes = sizeof(written);
  memcpy(reply.data, &written, sizeof(written));
  sys_reply(&reply);
}

/* ---- FS_OP_SEEK ---- */
static void handle_seek(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  uint32_t fd = OID_TO_FD(req->object_id);
  if (fd >= FATFS_MAX_FDS || !g_fds[fd].in_use) { sys_reply(&reply); return; }
  if (req->num_bytes < sizeof(fs_seek_req_t)) { sys_reply(&reply); return; }
  fatfs_fd_t *f = &g_fds[fd];

  fs_seek_req_t s;
  memcpy(&s, req->data, sizeof(s));
  int32_t base = 0;
  switch (s.whence) {
  case FS_SEEK_SET: base = 0;                    break;
  case FS_SEEK_CUR: base = (int32_t)f->cursor;   break;
  case FS_SEEK_END: base = (int32_t)f->size;     break;
  default: sys_reply(&reply); return;
  }
  int32_t np = base + s.offset;
  if (np < 0) np = 0;
  f->cursor = (uint32_t)np;

  fs_seek_rep_t r = { .new_pos = f->cursor };
  reply.num_bytes = sizeof(r);
  memcpy(reply.data, &r, sizeof(r));
  sys_reply(&reply);
}

/* ---- FS_OP_STAT ---- */
static void handle_stat(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  uint32_t fd = OID_TO_FD(req->object_id);
  if (fd >= FATFS_MAX_FDS || !g_fds[fd].in_use) { sys_reply(&reply); return; }

  fs_stat_t st = {
    .size = g_fds[fd].size,
    .type = g_fds[fd].is_dir ? FS_TYPE_DIR : FS_TYPE_FILE,
  };
  reply.num_bytes = sizeof(st);
  memcpy(reply.data, &st, sizeof(st));
  sys_reply(&reply);
}

/* ---- FS_OP_READDIR ---- */
static void handle_readdir(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  uint32_t fd = OID_TO_FD(req->object_id);
  if (fd >= FATFS_MAX_FDS || !g_fds[fd].in_use || !g_fds[fd].is_dir) { sys_reply(&reply); return; }
  fatfs_fd_t *f = &g_fds[fd];

  uint32_t target = 0;
  if (req->num_bytes >= 4) memcpy(&target, req->data, 4);

  uint32_t found = 0;
  for (uint32_t idx = 0;; idx++) {
    uint8_t e[DIRENT_SIZE];
    if (!dir_get_entry(f->is_root, f->first_cluster, idx, e, NULL, NULL)) break;
    int r = entry_is_regular(e);
    if (r < 0) break;      /* end */
    if (r == 0) continue;
    if (found == target) {
      fs_dirent_t de;
      memset(&de, 0, sizeof(de));
      name_to_str(e, de.name, sizeof(de.name));
      de.type = (e[11] & ATTR_DIR) ? FS_TYPE_DIR : FS_TYPE_FILE;
      reply.num_bytes = sizeof(de);
      memcpy(reply.data, &de, sizeof(de));
      sys_reply(&reply);
      return;
    }
    found++;
  }
  sys_reply(&reply); /* past end → empty */
}

/* ---- FS_OP_TRUNCATE ---- */
static void handle_truncate(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  uint32_t fd = OID_TO_FD(req->object_id);
  if (fd >= FATFS_MAX_FDS || !g_fds[fd].in_use || g_fds[fd].is_dir) { sys_reply(&reply); return; }
  fatfs_fd_t *f = &g_fds[fd];

  uint32_t new_size = 0;
  if (req->num_bytes >= 4) memcpy(&new_size, req->data, 4);

  if (new_size == 0) {
    if (f->first_cluster != 0) free_chain(f->first_cluster);
    f->first_cluster = 0;
  } else {
    /* Ensure the chain reaches the last byte (allocates as needed). */
    if (cluster_for_offset(f, new_size - 1u, true) == 0) { sys_reply(&reply); return; }
    /* Free any clusters beyond the new end. */
    uint32_t last_ord = (new_size - 1u) / g_cluster_bytes;
    uint16_t cl = f->first_cluster;
    for (uint32_t i = 0; i < last_ord; i++) cl = fat_next(cl);
    uint16_t tail = fat_next(cl);
    fat_set(cl, FAT_EOC_MARK);
    if (tail >= 2u && tail < FAT_EOC) free_chain(tail);
  }
  f->size  = new_size;
  f->dirty = true;
  flush_dirent(f);
  sys_reply(&reply);
}

/* ---- FS_OP_MKDIR (name in data, on a dir handle) ---- */
static void handle_mkdir(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  uint32_t fd = OID_TO_FD(req->object_id);
  if (fd >= FATFS_MAX_FDS || !g_fds[fd].in_use || !g_fds[fd].is_dir) { sys_reply(&reply); return; }
  fatfs_fd_t *f = &g_fds[fd];

  size_t nlen = req->num_bytes;
  if (nlen == 0 || nlen >= 64) { sys_reply(&reply); return; }
  uint8_t name83[11];
  if (!make_8_3((const char *)req->data, nlen, name83)) { sys_reply(&reply); return; }

  uint16_t nc = alloc_cluster();
  if (nc == 0) { sys_reply(&reply); return; }

  /* Initialise the new directory cluster with "." and ".." entries. */
  uint8_t dot[DIRENT_SIZE], dotdot[DIRENT_SIZE];
  memset(dot, 0, sizeof(dot));    memset(dot, ' ', 11);    dot[0] = '.';
  dot[11] = ATTR_DIR;   dot[26] = (uint8_t)(nc & 0xFF);   dot[27] = (uint8_t)(nc >> 8);
  memset(dotdot, 0, sizeof(dotdot)); memset(dotdot, ' ', 11); dotdot[0] = '.'; dotdot[1] = '.';
  dotdot[11] = ATTR_DIR;
  uint16_t parent_cl = f->is_root ? 0 : f->first_cluster;
  dotdot[26] = (uint8_t)(parent_cl & 0xFF); dotdot[27] = (uint8_t)(parent_cl >> 8);

  uint32_t lba = cluster_to_lba(nc);
  if (blk_read(lba) == 0) {
    memcpy(g_buf, dot, DIRENT_SIZE);
    memcpy(g_buf + DIRENT_SIZE, dotdot, DIRENT_SIZE);
    blk_write(lba);
  }

  uint16_t parent = f->is_root ? 0 : f->first_cluster;
  if (!dir_add_entry(f->is_root, parent, name83, ATTR_DIR, nc, 0, NULL, NULL)) {
    free_chain(nc);
  }
  sys_reply(&reply);
}

/* ---- FS_OP_UNLINK (name in data, on a dir handle) ---- */
static void handle_unlink(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  uint32_t fd = OID_TO_FD(req->object_id);
  if (fd >= FATFS_MAX_FDS || !g_fds[fd].in_use || !g_fds[fd].is_dir) { sys_reply(&reply); return; }
  fatfs_fd_t *f = &g_fds[fd];

  size_t nlen = req->num_bytes;
  if (nlen == 0 || nlen >= 64) { sys_reply(&reply); return; }

  uint8_t e[DIRENT_SIZE];
  uint32_t lba; uint16_t off;
  if (!dir_find(f->is_root, f->first_cluster, (const char *)req->data, nlen, e, &lba, &off)) {
    sys_reply(&reply); return;
  }
  uint16_t first = (uint16_t)(e[26] | (e[27] << 8));
  if (first >= 2u) free_chain(first);

  if (blk_read(lba) == 0) {
    g_buf[off] = 0xE5;                 /* mark deleted */
    blk_write(lba);
  }
  sys_reply(&reply);
}

/* ---- IPC_OP_CLOSE ---- */
static void handle_close(const sys_ipc_msg_t *req) {
  uint32_t fd = OID_TO_FD(req->object_id);
  if (fd < FATFS_MAX_FDS && g_fds[fd].in_use) {
    if (g_fds[fd].dirty) flush_dirent(&g_fds[fd]);
    g_fds[fd].in_use = false;
  }
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));
  sys_reply(&reply);
}

/* ---- mount: open blk:0, attach buffer, parse BPB ---- */
static bool mount_blk(void) {
  g_blk = sys_open("blk:0", 0);
  if (g_blk == 0) return false;

  cap_handle_t pg = sys_page_alloc(1, 0, 0);
  if (pg == 0) return false;
  sys_vspace_map_args_t ma = {
    .vspace_cap = sys_vspace_self(),
    .virt_addr  = FATFS_BUF_VBASE,
    .page_cap   = pg,
    .prot_flags = VMM_PROT_READ | VMM_PROT_WRITE,
  };
  if (sys_vspace_map(&ma) != 0) { sys_cap_close(pg); return false; }
  g_buf = (uint8_t *)FATFS_BUF_VBASE;

  sys_ipc_msg_t areq, arep;
  memset(&areq, 0, sizeof(areq));
  memset(&arep, 0, sizeof(arep));
  areq.opcode = BLK_OP_ATTACH;
  areq.num_handles = 1;
  areq.handles[0]  = pg;
  if (sys_call(g_blk, &areq, &arep) != 0) { sys_cap_close(pg); return false; }
  sys_cap_close(pg); /* device holds its own reference */

  if (blk_read(0) != 0) return false;

  uint16_t bps          = rd16(11);
  uint32_t spc          = g_buf[13];
  uint32_t reserved     = rd16(14);
  uint32_t num_fats     = g_buf[16];
  uint32_t root_entries = rd16(17);
  uint32_t total16      = rd16(19);
  uint32_t fat_size16   = rd16(22);
  uint32_t total32      = rd32_bytes(g_buf + 32);
  if (bps != BLK_SECTOR_SIZE || spc == 0 || num_fats == 0) return false;

  g_fat_start     = reserved;
  g_num_fats      = num_fats;
  g_fat_sectors   = fat_size16;
  g_root_start    = reserved + num_fats * fat_size16;
  g_root_sectors  = (root_entries * DIRENT_SIZE + BLK_SECTOR_SIZE - 1) / BLK_SECTOR_SIZE;
  g_data_start    = g_root_start + g_root_sectors;
  g_total_sectors = total16 ? total16 : total32;
  g_spc           = spc;
  g_cluster_bytes = spc * BLK_SECTOR_SIZE;
  g_max_cluster   = (g_total_sectors - g_data_start) / spc + 2u;
  return true;
}

/* ---- entry point ---- */
void _start(void) {
  cap_handle_t my_ep = sys_bootstrap_cap(0);
  memset(g_fds, 0, sizeof(g_fds));

  g_mounted = mount_blk();

  for (;;) {
    sys_ipc_msg_t req;
    memset(&req, 0, sizeof(req));
    if (sys_recv(my_ep, &req) != 0) continue;

    switch (req.opcode) {
    case IPC_OP_OPEN:  handle_open(&req);  break;
    case IPC_OP_READ:  handle_read(&req);  break;
    case IPC_OP_WRITE: handle_write(&req); break;
    case IPC_OP_CLOSE: handle_close(&req); break;
    default:
      switch (req.opcode) {
      case FS_OP_SEEK:     handle_seek(&req);     break;
      case FS_OP_STAT:     handle_stat(&req);     break;
      case FS_OP_READDIR:  handle_readdir(&req);  break;
      case FS_OP_TRUNCATE: handle_truncate(&req); break;
      case FS_OP_MKDIR:    handle_mkdir(&req);    break;
      case FS_OP_UNLINK:   handle_unlink(&req);   break;
      default: { sys_ipc_msg_t r; memset(&r, 0, sizeof(r)); sys_reply(&r); } break;
      }
      break;
    }
  }
}
