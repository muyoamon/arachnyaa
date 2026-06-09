/*
 * elfloader — user-space ELF loader service
 *
 * Protocol: elfloader:
 * Endpoint: BOOTSTRAP_SERVER_EP (cap slot 0)
 *
 * IPC_OP_OPEN "elfloader:elf32"
 *   → object_id=ELF32_OID, allowed_ops=KOP_EXEC|KOP_CALL
 *
 * IPC_OP_EXEC (object_id==ELF32_OID)
 *   req.handles[0] = stdin cap
 *   req.handles[1] = stdout cap
 *   req.handles[2] = stderr cap
 *   req.handles[3] = elf_page_cap (KOBJ_VMOBJ with ELF bytes, transferred by kernel)
 *   req.data[0+]   = argv0 (null-terminated, optional)
 *   reply.handles[0] = watch_cap (KOBJ_PROC with R_PROC_WAIT)
 */

#include <stdint.h>
#include <stddef.h>
#include "syscall.h"
#include "string.h"

/* ---- ELF32 definitions ---- */
#define EI_NIDENT   16
#define ELFMAG0     0x7F
#define ELFMAG1     'E'
#define ELFMAG2     'L'
#define ELFMAG3     'F'
#define ELFCLASS32  1
#define ELFDATA2LSB 1
#define EV_CURRENT  1
#define ET_EXEC     2
#define ET_DYN      3
#define EM_386      3
#define PT_LOAD     1
#define PF_W        0x2

typedef struct {
  uint8_t  e_ident[EI_NIDENT];
  uint16_t e_type;
  uint16_t e_machine;
  uint32_t e_version;
  uint32_t e_entry;
  uint32_t e_phoff;
  uint32_t e_shoff;
  uint32_t e_flags;
  uint16_t e_ehsize;
  uint16_t e_phentsize;
  uint16_t e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;
  uint16_t e_shstrndx;
} Elf32_Ehdr;

typedef struct {
  uint32_t p_type;
  uint32_t p_offset;
  uint32_t p_vaddr;
  uint32_t p_paddr;
  uint32_t p_filesz;
  uint32_t p_memsz;
  uint32_t p_flags;
  uint32_t p_align;
} Elf32_Phdr;

/* ---- Constants ---- */
#define PAGE_SIZE_U       4096u
#define ELF32_OID         1u

/* Temporary mapping slots in elfloader's own address space. These must not
   overlap elfloader's code (~0x08048000) or its stack (~0xBFDFF000). */
#define TMP_ELF_BASE      0x40000000u  /* source ELF read here; max 1MB = 256 pages */
#define TMP_ELF_MAXPAGES  256u
#define TMP_SEG_BASE      0x50000000u  /* one target segment at a time */

#define USER_STACK_TOP    0xBFDFF000u
#define USER_STACK_PAGES  4u
#define USER_STACK_SIZE   (USER_STACK_PAGES * PAGE_SIZE_U)
#define USER_STACK_BASE   (USER_STACK_TOP - USER_STACK_SIZE)

/* ET_DYN load bias (matches kernel's choose_dyn_base). */
#define USER_ENTRY_BASE   0x08048000u

static inline uint32_t pg_floor(uint32_t x) { return x & ~(PAGE_SIZE_U - 1u); }
static inline uint32_t pg_ceil(uint32_t x)  { return (x + PAGE_SIZE_U - 1u) & ~(PAGE_SIZE_U - 1u); }

/* ---- OPEN handler ---- */

static void handle_open(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  static const char elf32_path[] = "elfloader:elf32";
  if (req->num_bytes == sizeof(elf32_path) - 1 &&
      memcmp(req->data, elf32_path, req->num_bytes) == 0) {
    sys_open_reply_t oreply = { .allowed_ops = KOP_EXEC | KOP_CALL };
    reply.object_id = ELF32_OID;
    reply.num_bytes  = sizeof(oreply);
    memcpy(reply.data, &oreply, sizeof(oreply));
  }
  sys_reply(&reply);
}

/* ---- EXEC handler ---- */

static void handle_exec(const sys_ipc_msg_t *req) {
  cap_handle_t stdin_cap  = req->handles[0];
  cap_handle_t stdout_cap = req->handles[1];
  cap_handle_t stderr_cap = req->handles[2];
  cap_handle_t elf_cap    = req->handles[3];

  const char *argv0 = (req->num_bytes > 0) ? (const char *)req->data : "elf";

  /* Caps we need to clean up regardless of success/failure. */
  cap_handle_t self_vspace   = 0;
  cap_handle_t target_vspace = 0;
  cap_handle_t watch_cap     = 0;
  int mapped_elf = 0;

  if (elf_cap == 0) goto done;

  self_vspace = sys_vspace_self();
  if (self_vspace == 0) goto done;

  /* Map source ELF into our address space for reading. */
  {
    sys_vspace_map_args_t ma = {
      .vspace_cap = self_vspace,
      .virt_addr  = TMP_ELF_BASE,
      .page_cap   = elf_cap,
      .prot_flags = VMM_PROT_READ,
    };
    if (sys_vspace_map(&ma) != 0) goto done;
    mapped_elf = 1;
  }

  const Elf32_Ehdr *eh = (const Elf32_Ehdr *)TMP_ELF_BASE;

  /* Validate ELF magic + class + arch. */
  if (eh->e_ident[0] != ELFMAG0 || eh->e_ident[1] != ELFMAG1 ||
      eh->e_ident[2] != ELFMAG2 || eh->e_ident[3] != ELFMAG3 ||
      eh->e_ident[4] != ELFCLASS32 || eh->e_ident[5] != ELFDATA2LSB ||
      eh->e_version   != EV_CURRENT || eh->e_machine != EM_386 ||
      (eh->e_type != ET_EXEC && eh->e_type != ET_DYN) ||
      eh->e_phentsize != sizeof(Elf32_Phdr))
    goto done;

  uint32_t base        = (eh->e_type == ET_DYN) ? USER_ENTRY_BASE : 0u;
  uint32_t entry_point = base + eh->e_entry;

  target_vspace = sys_vspace_create();
  if (target_vspace == 0) goto done;

  /* Load each PT_LOAD segment into target_vspace. */
  const Elf32_Phdr *phdrs =
      (const Elf32_Phdr *)(TMP_ELF_BASE + eh->e_phoff);

  for (uint32_t i = 0; i < eh->e_phnum; i++) {
    const Elf32_Phdr *ph = &phdrs[i];
    if (ph->p_type != PT_LOAD || ph->p_memsz == 0) continue;

    uint32_t seg_va    = base + ph->p_vaddr;
    uint32_t map_begin = pg_floor(seg_va);
    uint32_t map_end   = pg_ceil(seg_va + ph->p_memsz);
    uint32_t npages    = (map_end - map_begin) / PAGE_SIZE_U;

    cap_handle_t seg_cap = sys_page_alloc(npages, 0, 0);
    if (seg_cap == 0) goto done;

    /* Map into self at TMP_SEG_BASE for writing. */
    sys_vspace_map_args_t self_ma = {
      .vspace_cap = self_vspace,
      .virt_addr  = TMP_SEG_BASE,
      .page_cap   = seg_cap,
      .prot_flags = VMM_PROT_READ | VMM_PROT_WRITE,
    };
    if (sys_vspace_map(&self_ma) != 0) {
      sys_cap_close(seg_cap);
      goto done;
    }

    /* Copy file data, zero BSS tail. */
    uint8_t *tmp = (uint8_t *)TMP_SEG_BASE + (seg_va - map_begin);
    memcpy(tmp, (const void *)(TMP_ELF_BASE + ph->p_offset), ph->p_filesz);
    if (ph->p_memsz > ph->p_filesz)
      memset(tmp + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);

    /* Unmap temporary write window. */
    sys_vspace_unmap(self_vspace, TMP_SEG_BASE, npages);

    /* Map into target with ELF permissions (always readable). */
    uint32_t prot = VMM_PROT_READ;
    if (ph->p_flags & PF_W) prot |= VMM_PROT_WRITE;

    sys_vspace_map_args_t tgt_ma = {
      .vspace_cap = target_vspace,
      .virt_addr  = map_begin,
      .page_cap   = seg_cap,
      .prot_flags = prot,
    };
    int rc = sys_vspace_map(&tgt_ma);
    sys_cap_close(seg_cap);
    if (rc != 0) goto done;
  }

  /* Allocate and map user stack in target. */
  {
    cap_handle_t stk = sys_page_alloc(USER_STACK_PAGES, 0, 0);
    if (stk == 0) goto done;
    sys_vspace_map_args_t stk_ma = {
      .vspace_cap = target_vspace,
      .virt_addr  = USER_STACK_BASE,
      .page_cap   = stk,
      .prot_flags = VMM_PROT_READ | VMM_PROT_WRITE,
    };
    int rc = sys_vspace_map(&stk_ma);
    sys_cap_close(stk);
    if (rc != 0) goto done;
  }

  uint32_t user_sp = USER_STACK_TOP - 16u;

  /* Call proc:spawn via procd. */
  {
    cap_handle_t proc_spawn_h = sys_open("proc:spawn", 0);
    if (proc_spawn_h == 0) goto done;

    sys_ipc_msg_t spawn_req, spawn_rep;
    memset(&spawn_req, 0, sizeof(spawn_req));
    memset(&spawn_rep, 0, sizeof(spawn_rep));

    spawn_req.opcode      = IPC_OP_EXEC;
    spawn_req.num_handles = 4;

    /* Pack entry_point + user_sp into data, then argv0. */
    size_t av_len = strlen(argv0) + 1u;
    spawn_req.num_bytes = (uint32_t)(8u + av_len);
    memcpy(spawn_req.data + 0, &entry_point, 4);
    memcpy(spawn_req.data + 4, &user_sp,     4);
    memcpy(spawn_req.data + 8, argv0, av_len);

    /* handles[0]=vspace transferred to procd; [1..3]=stdio transferred to child */
    spawn_req.handles[0] = target_vspace;
    spawn_req.handles[1] = stdin_cap;
    spawn_req.handles[2] = stdout_cap;
    spawn_req.handles[3] = stderr_cap;

    int rc = sys_call(proc_spawn_h, &spawn_req, &spawn_rep);
    sys_cap_close(proc_spawn_h);

    if (rc == 0 && spawn_rep.handles[0] != 0)
      watch_cap = spawn_rep.handles[0];
  }

done:
  if (mapped_elf)
    sys_vspace_unmap(self_vspace, TMP_ELF_BASE, TMP_ELF_MAXPAGES);
  if (self_vspace)   sys_cap_close(self_vspace);
  if (target_vspace) sys_cap_close(target_vspace);

  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));
  if (watch_cap) {
    reply.num_handles = 1;
    reply.handles[0]  = watch_cap;
  }
  sys_reply(&reply);

  if (watch_cap) sys_cap_close(watch_cap);  /* transferred by sys_reply */
}

/* ---- entry point ---- */

void _start(void) {
  cap_handle_t my_ep = sys_bootstrap_cap(0);

  for (;;) {
    sys_ipc_msg_t req;
    memset(&req, 0, sizeof(req));
    if (sys_recv(my_ep, &req) != 0) continue;

    switch (req.opcode) {
    case IPC_OP_OPEN:
      handle_open(&req);
      break;
    case IPC_OP_EXEC:
      if (req.object_id == ELF32_OID)
        handle_exec(&req);
      else
        goto unknown;
      break;
    default:
    unknown:;
      sys_ipc_msg_t r;
      memset(&r, 0, sizeof(r));
      sys_reply(&r);
      break;
    }
  }
}
