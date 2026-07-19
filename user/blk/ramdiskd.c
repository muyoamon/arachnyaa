/*
 * ramdiskd — RAM-backed block device service
 *
 * Protocol: blk:
 * Endpoint: bootstrap cap slot 0
 *
 * Implements the blk: protocol (blk_proto.h) against a page-allocated backing
 * store.  This is the development/test backend for fatfs: — it has no IRQ or
 * port-IO complexity, so the filesystem can be brought up against it before the
 * real ATA driver (atad) exists.  atad is a drop-in replacement speaking the
 * identical protocol.
 *
 * Sector transfer uses the shared bounce buffer established by BLK_OP_ATTACH:
 * the client's page cap is mapped at BLK_SHARED_VBASE and READ/WRITE memcpy
 * between the backing store and that page.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../ulib/syscall.h"
#include "../ulib/string.h"
#include "blk_proto.h"

/* ---- backing store ---- */
#define RAMDISK_MB       4u
#define RAMDISK_BYTES    (RAMDISK_MB * 1024u * 1024u)
#define RAMDISK_SECTORS  (RAMDISK_BYTES / BLK_SECTOR_SIZE)
#define PAGE_SIZE_U      4096u

#define RAMDISK_VBASE    0x30000000u  /* backing store */
#define BLK_SHARED_VBASE 0x28000000u  /* client's shared bounce buffer */

static uint8_t     *g_store  = NULL;         /* mapped backing store */
static uint8_t     *g_shared = NULL;         /* mapped shared buffer (or NULL) */
static cap_handle_t g_shared_cap = 0;

/* Device object id: device 0 → oid 1. */
#define BLK_DEV_OID 1u

/* ---- map a page-allocated region at a fixed vaddr; returns base or NULL ---- */
static uint8_t *map_region(uint32_t vaddr, uint32_t npages) {
  cap_handle_t self = sys_vspace_self();
  cap_handle_t pg   = sys_page_alloc(npages, 0, 0);
  if (!pg || !self) return NULL;
  sys_vspace_map_args_t ma = {
    .vspace_cap = self,
    .virt_addr  = vaddr,
    .page_cap   = pg,
    .prot_flags = VMM_PROT_READ | VMM_PROT_WRITE,
  };
  if (sys_vspace_map(&ma) != 0) { sys_cap_close(pg); return NULL; }
  sys_cap_close(pg); /* mapping keeps the VMOBJ alive */
  return (uint8_t *)(uintptr_t)vaddr;
}

/* ---- reply helpers ---- */
static void reply_status(int32_t status) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));
  reply.num_bytes = sizeof(status);
  memcpy(reply.data, &status, sizeof(status));
  sys_reply(&reply);
}

/* ---- IPC_OP_OPEN: return the single device handle ---- */
static void handle_open(const sys_ipc_msg_t *req) {
  (void)req;
  sys_ipc_msg_t    reply;
  sys_open_reply_t oreply;
  memset(&reply,  0, sizeof(reply));
  memset(&oreply, 0, sizeof(oreply));

  /* Block ops travel via sys_call; no direct read/write on the handle. */
  oreply.allowed_ops = KOP_CALL | KOP_CLOSE;
  reply.object_id    = BLK_DEV_OID;
  reply.num_bytes    = sizeof(oreply);
  memcpy(reply.data, &oreply, sizeof(oreply));
  sys_reply(&reply);
}

/* ---- BLK_OP_INFO ---- */
static void handle_info(void) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));
  blk_info_t info = {
    .sector_size  = BLK_SECTOR_SIZE,
    .sector_count = RAMDISK_SECTORS,
  };
  reply.num_bytes = sizeof(info);
  memcpy(reply.data, &info, sizeof(info));
  sys_reply(&reply);
}

/* ---- BLK_OP_ATTACH: map the client's page cap as the shared buffer ---- */
static void handle_attach(const sys_ipc_msg_t *req) {
  if (req->num_handles < 1 || req->handles[0] == 0) { reply_status(-1); return; }

  /* Replace any previous attachment. */
  if (g_shared) {
    sys_vspace_unmap(sys_vspace_self(), BLK_SHARED_VBASE, 1);
    if (g_shared_cap) sys_cap_close(g_shared_cap);
    g_shared = NULL; g_shared_cap = 0;
  }

  cap_handle_t page_cap = (cap_handle_t)req->handles[0];
  sys_vspace_map_args_t ma = {
    .vspace_cap = sys_vspace_self(),
    .virt_addr  = BLK_SHARED_VBASE,
    .page_cap   = page_cap,
    .prot_flags = VMM_PROT_READ | VMM_PROT_WRITE,
  };
  if (sys_vspace_map(&ma) != 0) { sys_cap_close(page_cap); reply_status(-1); return; }

  g_shared     = (uint8_t *)(uintptr_t)BLK_SHARED_VBASE;
  g_shared_cap = page_cap; /* keep the cap so the mapping stays valid */
  reply_status(0);
}

/* ---- BLK_OP_READ / BLK_OP_WRITE ---- */
static void handle_io(const sys_ipc_msg_t *req, bool is_write) {
  if (!g_shared || !g_store)             { reply_status(-1); return; }
  if (req->num_bytes < sizeof(blk_io_req_t)) { reply_status(-1); return; }

  blk_io_req_t io;
  memcpy(&io, req->data, sizeof(io));
  if (io.count == 0 || io.count > BLK_MAX_XFER) { reply_status(-1); return; }
  if (io.lba + io.count > RAMDISK_SECTORS)      { reply_status(-1); return; }

  uint32_t bytes  = (uint32_t)io.count * BLK_SECTOR_SIZE;
  uint8_t *disk_p = g_store + (uint32_t)io.lba * BLK_SECTOR_SIZE;

  if (is_write) memcpy(disk_p, g_shared, bytes);
  else          memcpy(g_shared, disk_p, bytes);

  reply_status(0);
}

/* ---- entry point ---- */
void _start(void) {
  cap_handle_t my_ep = sys_bootstrap_cap(0);

  g_store = map_region(RAMDISK_VBASE, RAMDISK_BYTES / PAGE_SIZE_U);
  if (g_store) memset(g_store, 0, RAMDISK_BYTES);

  for (;;) {
    sys_ipc_msg_t req;
    memset(&req, 0, sizeof(req));
    if (sys_recv(my_ep, &req) != 0) continue;

    switch (req.opcode) {
    case IPC_OP_OPEN:  handle_open(&req);      break;
    case IPC_OP_CLOSE: reply_status(0);        break;
    case BLK_OP_INFO:  handle_info();          break;
    case BLK_OP_ATTACH: handle_attach(&req);   break;
    case BLK_OP_READ:  handle_io(&req, false); break;
    case BLK_OP_WRITE: handle_io(&req, true);  break;
    default:           reply_status(-1);       break;
    }
  }
}
