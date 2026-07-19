#ifndef BLK_PROTO_H_
#define BLK_PROTO_H_

#include <stdint.h>

/*
 * blk: block-device protocol.
 *
 * A block device exposes a linear array of fixed-size sectors and knows nothing
 * about files.  Filesystem services (e.g. fatfs:) sit on top of it.
 *
 * Standard IPC opcodes (IPC_OP_OPEN/CLOSE) are handled by the kernel routing
 * layer; sys_open("blk:0") yields a KOBJ_REMOTE handle for device 0.  The
 * opcodes below are service-defined and sent via sys_call() — the kernel passes
 * them through opaquely.
 *
 * Sector data does not travel inline in the 256-byte IPC message.  Instead the
 * client allocates a page (sys_page_alloc), maps it, and hands the page cap to
 * the device once via BLK_OP_ATTACH.  The device maps the same physical page,
 * so BLK_OP_READ/WRITE move sectors through that shared buffer with no IPC copy.
 * The shared buffer is one 4 KB page = BLK_MAX_XFER sectors.
 */

#define BLK_SECTOR_SIZE 512u
#define BLK_MAX_XFER    8u   /* sectors per transfer (4096 / 512) */

/* ---- opcodes (0x200 range: clear of fs_proto.h's 0x100) ---- */
enum {
  BLK_OP_INFO   = 0x200, /* no request data → blk_info_t in reply */
  BLK_OP_ATTACH = 0x201, /* handles[0] = page cap for the shared buffer */
  BLK_OP_READ   = 0x202, /* blk_io_req_t in data → count sectors into shared buffer */
  BLK_OP_WRITE  = 0x203, /* blk_io_req_t in data ← count sectors from shared buffer */
};

/* ---- BLK_OP_INFO ---- */
typedef struct {
  uint32_t sector_size;  /* always BLK_SECTOR_SIZE for now */
  uint32_t sector_count; /* total addressable sectors */
} blk_info_t;

/* ---- BLK_OP_READ / BLK_OP_WRITE ---- */
typedef struct {
  uint32_t lba;   /* starting sector */
  uint16_t count; /* number of sectors; must be 1..BLK_MAX_XFER */
} blk_io_req_t;

/* ---- reply status (reply.data[0..3] as int32_t; 0 = ok, negative = error) ---- */

#endif /* BLK_PROTO_H_ */
