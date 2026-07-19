/*
 * atad — ATA PIO block device driver
 *
 * Protocol: blk:  (drop-in replacement for ramdiskd — identical protocol)
 * Endpoint: bootstrap cap slot 0
 *
 * Drives the primary ATA bus (I/O ports 0x1F0-0x1F7 + control 0x3F6) in 28-bit
 * LBA PIO mode against the master drive.  Polled (no IRQ) for v1 — the timer
 * still preempts atad, so busy-waiting wastes CPU but does not hang the system.
 *
 * Sector data moves through the shared bounce buffer that the filesystem
 * attaches via BLK_OP_ATTACH; 512-byte sectors are transferred with the bulk
 * 16-bit port-IO primitives sys_io_insw/sys_io_outsw (one syscall per sector).
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../ulib/syscall.h"
#include "../ulib/string.h"
#include "blk_proto.h"

/* ---- primary ATA bus registers ---- */
#define ATA_DATA    0x1F0u
#define ATA_SECCNT  0x1F2u
#define ATA_LBA0    0x1F3u
#define ATA_LBA1    0x1F4u
#define ATA_LBA2    0x1F5u
#define ATA_DRIVE   0x1F6u
#define ATA_STATUS  0x1F7u   /* read  */
#define ATA_CMD     0x1F7u   /* write */
#define ATA_ALTSTAT 0x3F6u

#define ST_BSY 0x80u
#define ST_DRQ 0x08u
#define ST_ERR 0x01u

#define CMD_READ     0x20u
#define CMD_WRITE    0x30u
#define CMD_FLUSH    0xE7u
#define CMD_IDENTIFY 0xECu

#define WORDS_PER_SECTOR (BLK_SECTOR_SIZE / 2u)

/* ---- shared bounce buffer ---- */
#define BLK_SHARED_VBASE 0x28000000u
static uint8_t     *g_shared = NULL;
static cap_handle_t g_shared_cap = 0;
static uint32_t     g_sector_count = 0;

#define BLK_DEV_OID 1u

/* 400 ns settle: read alternate status four times (~100 ns each). */
static void ata_delay(void) {
  for (int i = 0; i < 4; i++) (void)sys_io_in(ATA_ALTSTAT);
}

/* Wait for BSY to clear. Returns -1 on timeout or ERR. */
static int ata_wait_ready(void) {
  for (int i = 0; i < 1000000; i++) {
    uint8_t st = (uint8_t)sys_io_in(ATA_STATUS);
    if (st & ST_BSY) continue;
    return (st & ST_ERR) ? -1 : 0;
  }
  return -1;
}

/* Wait for DRQ (data request) with BSY clear. Returns -1 on timeout or ERR. */
static int ata_wait_drq(void) {
  for (int i = 0; i < 1000000; i++) {
    uint8_t st = (uint8_t)sys_io_in(ATA_STATUS);
    if (st & ST_ERR) return -1;
    if (!(st & ST_BSY) && (st & ST_DRQ)) return 0;
  }
  return -1;
}

/* Program the LBA28 registers for the master drive. */
static void ata_setup(uint32_t lba, uint8_t count) {
  sys_io_out(ATA_DRIVE, 0xE0u | ((lba >> 24) & 0x0Fu)); /* LBA mode, master */
  ata_delay();
  sys_io_out(ATA_SECCNT, count);
  sys_io_out(ATA_LBA0, lba & 0xFFu);
  sys_io_out(ATA_LBA1, (lba >> 8) & 0xFFu);
  sys_io_out(ATA_LBA2, (lba >> 16) & 0xFFu);
}

/* IDENTIFY the master drive; returns LBA28 sector count (0 if absent). */
static uint32_t ata_identify(void) {
  if (ata_wait_ready() != 0) return 0;
  sys_io_out(ATA_DRIVE, 0xA0u);
  ata_delay();
  sys_io_out(ATA_SECCNT, 0);
  sys_io_out(ATA_LBA0, 0);
  sys_io_out(ATA_LBA1, 0);
  sys_io_out(ATA_LBA2, 0);
  sys_io_out(ATA_CMD, CMD_IDENTIFY);
  if (sys_io_in(ATA_STATUS) == 0) return 0; /* no drive */
  if (ata_wait_drq() != 0) return 0;

  uint16_t id[256];
  sys_io_insw(ATA_DATA, id, 256);
  return (uint32_t)id[60] | ((uint32_t)id[61] << 16); /* LBA28 total sectors */
}

/* Read/write `count` sectors starting at `lba` to/from the shared buffer. */
static int ata_rw(uint32_t lba, uint8_t count, bool is_write) {
  if (!g_shared) return -1;
  if (ata_wait_ready() != 0) return -1;
  ata_setup(lba, count);
  sys_io_out(ATA_CMD, is_write ? CMD_WRITE : CMD_READ);

  for (uint8_t s = 0; s < count; s++) {
    if (ata_wait_drq() != 0) return -1;
    uint8_t *p = g_shared + (uint32_t)s * BLK_SECTOR_SIZE;
    if (is_write) sys_io_outsw(ATA_DATA, p, WORDS_PER_SECTOR);
    else          sys_io_insw(ATA_DATA, p, WORDS_PER_SECTOR);
    ata_delay();
  }

  if (is_write) {
    if (ata_wait_ready() != 0) return -1;
    sys_io_out(ATA_CMD, CMD_FLUSH);
    ata_wait_ready();
  }
  return 0;
}

/* ---- reply helper ---- */
static void reply_status(int32_t status) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));
  reply.num_bytes = sizeof(status);
  memcpy(reply.data, &status, sizeof(status));
  sys_reply(&reply);
}

/* ---- IPC_OP_OPEN ---- */
static void handle_open(const sys_ipc_msg_t *req) {
  (void)req;
  sys_ipc_msg_t    reply;
  sys_open_reply_t oreply;
  memset(&reply,  0, sizeof(reply));
  memset(&oreply, 0, sizeof(oreply));
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
  blk_info_t info = { .sector_size = BLK_SECTOR_SIZE, .sector_count = g_sector_count };
  reply.num_bytes = sizeof(info);
  memcpy(reply.data, &info, sizeof(info));
  sys_reply(&reply);
}

/* ---- BLK_OP_ATTACH ---- */
static void handle_attach(const sys_ipc_msg_t *req) {
  if (req->num_handles < 1 || req->handles[0] == 0) { reply_status(-1); return; }
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
  g_shared_cap = page_cap;
  reply_status(0);
}

/* ---- BLK_OP_READ / BLK_OP_WRITE ---- */
static void handle_io(const sys_ipc_msg_t *req, bool is_write) {
  if (!g_shared || req->num_bytes < sizeof(blk_io_req_t)) { reply_status(-1); return; }
  blk_io_req_t io;
  memcpy(&io, req->data, sizeof(io));
  if (io.count == 0 || io.count > BLK_MAX_XFER)        { reply_status(-1); return; }
  if (io.lba + io.count > g_sector_count)              { reply_status(-1); return; }
  reply_status(ata_rw(io.lba, (uint8_t)io.count, is_write));
}

/* ---- entry point ---- */
void _start(void) {
  cap_handle_t my_ep = sys_bootstrap_cap(0);
  g_sector_count = ata_identify();

  for (;;) {
    sys_ipc_msg_t req;
    memset(&req, 0, sizeof(req));
    if (sys_recv(my_ep, &req) != 0) continue;

    switch (req.opcode) {
    case IPC_OP_OPEN:   handle_open(&req);      break;
    case IPC_OP_CLOSE:  reply_status(0);        break;
    case BLK_OP_INFO:   handle_info();          break;
    case BLK_OP_ATTACH: handle_attach(&req);    break;
    case BLK_OP_READ:   handle_io(&req, false); break;
    case BLK_OP_WRITE:  handle_io(&req, true);  break;
    default:            reply_status(-1);       break;
    }
  }
}
