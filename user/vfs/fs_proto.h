#ifndef VFS_FS_PROTO_H_
#define VFS_FS_PROTO_H_

#include <stdint.h>

/*
 * VFS filesystem protocol.
 *
 * Defines the contract that vfs: enforces on its backend services (e.g. ramfs:)
 * and that clients use when talking to vfs: handles.
 *
 * Standard IPC opcodes (IPC_OP_OPEN/READ/WRITE/CLOSE/EXEC) are handled by the
 * kernel routing layer.  The opcodes below are service-defined and sent via
 * sys_call(); the kernel passes them through opaquely.
 *
 * Any service that wishes to be mountable under vfs: must implement at minimum:
 *   FS_OP_SEEK, FS_OP_STAT, FS_OP_READDIR
 */

/* ---- VFS-protocol opcodes (sent via sys_call, kernel-transparent) ---- */
enum {
  FS_OP_SEEK     = 0x100, /* fs_seek_req_t in data → fs_seek_rep_t in reply */
  FS_OP_STAT     = 0x101, /* no request data → fs_stat_t in reply */
  FS_OP_READDIR  = 0x102, /* uint32_t index in data → fs_dirent_t in reply */
  FS_OP_TRUNCATE = 0x103, /* uint32_t size in data */
  FS_OP_MKDIR    = 0x104, /* char name[] in data (on a dir handle) */
  FS_OP_UNLINK   = 0x105, /* char name[] in data (on a dir handle) */
  FS_OP_RENAME   = 0x106, /* fs_rename_req_t in data (on a dir handle) */
  FS_OP_MOUNT    = 0x107, /* VFS only: fs_mount_req_t in data (on control handle) */
  FS_OP_UMOUNT   = 0x108, /* VFS only: char path[] in data (on control handle) */
};

/* ---- Open flags (passed as sys_open flags, interpreted by vfs:/backends) ---- */
enum {
  FS_O_RDONLY    = 0x00, /* read-only (default) */
  FS_O_WRONLY    = 0x01, /* write-only */
  FS_O_RDWR      = 0x02, /* read-write */
  FS_O_CREAT     = 0x04, /* create if not exists */
  FS_O_TRUNC     = 0x08, /* truncate to zero on open */
  FS_O_DIRECTORY = 0x10, /* must be a directory */
  FS_O_EXEC      = 0x20, /* search exec path for bare names */
};

/* ---- Node types ---- */
enum {
  FS_TYPE_FILE = 0,
  FS_TYPE_DIR  = 1,
};

/* ---- Seek whence ---- */
enum {
  FS_SEEK_SET = 0,
  FS_SEEK_CUR = 1,
  FS_SEEK_END = 2,
};

/* ---- FS_OP_SEEK ---- */
typedef struct {
  int32_t offset;
  uint8_t whence; /* FS_SEEK_* */
} fs_seek_req_t;

typedef struct {
  uint32_t new_pos;
} fs_seek_rep_t;

/* ---- FS_OP_STAT ---- */
typedef struct {
  uint32_t size;
  uint8_t  type;    /* FS_TYPE_* */
  uint8_t  _pad[3];
} fs_stat_t;

/* ---- FS_OP_READDIR ---- */
typedef struct {
  char    name[247]; /* null-terminated */
  uint8_t type;      /* FS_TYPE_* */
} fs_dirent_t; /* 248 bytes — fits in IPC inline data */

/* ---- FS_OP_RENAME ---- */
typedef struct {
  char old_name[128];
  char new_name[128];
} fs_rename_req_t; /* 256 bytes exactly */

/* ---- FS_OP_MOUNT (VFS control handle only) ---- */
typedef struct {
  char mount_path[120];   /* e.g., "/" or "/mnt/usb" */
  char backend_proto[16]; /* e.g., "ramfs" or "ext2" */
} fs_mount_req_t; /* 136 bytes */

#endif /* VFS_FS_PROTO_H_ */
