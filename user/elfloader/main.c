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
#include "../vfs/fs_proto.h"
#include "../procd/proc_args.h"

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
#define ELF32_FH_OID      2u  /* file-handle variant: handles[3] = readable KOBJ_REMOTE */

/* Temporary mapping slots in elfloader's own address space. These must not
   overlap elfloader's code (~0x08048000) or its stack (~0xBFDFF000). */
#define TMP_ELF_BASE      0x40000000u  /* source ELF read here; max 1MB = 256 pages */
#define TMP_ELF_MAXPAGES  256u
#define TMP_SEG_BASE      0x50000000u  /* one target segment at a time */
#define TMP_STACK_BASE    0x61000000u  /* temporary window to write PAB onto child stack */

#define USER_STACK_TOP    0xBFDFF000u
#define USER_STACK_PAGES  4u
#define USER_STACK_SIZE   (USER_STACK_PAGES * PAGE_SIZE_U)
#define USER_STACK_BASE   (USER_STACK_TOP - USER_STACK_SIZE)

/* ET_DYN load bias (matches kernel's choose_dyn_base). */
#define USER_ENTRY_BASE   0x08048000u

static inline uint32_t pg_floor(uint32_t x) { return x & ~(PAGE_SIZE_U - 1u); }
static inline uint32_t pg_ceil(uint32_t x)  { return (x + PAGE_SIZE_U - 1u) & ~(PAGE_SIZE_U - 1u); }

/* ---- File-handle helpers (for elf32-fh path) ---- */

static uint32_t fh_seek(cap_handle_t fh, int32_t off, uint8_t whence) {
  sys_ipc_msg_t req, rep;
  memset(&req, 0, sizeof(req));
  memset(&rep, 0, sizeof(rep));
  req.opcode    = FS_OP_SEEK;
  req.num_bytes = sizeof(fs_seek_req_t);
  fs_seek_req_t s = { off, whence };
  memcpy(req.data, &s, sizeof(s));
  if (sys_call(fh, &req, &rep) != 0) return 0;
  if (rep.num_bytes < (uint32_t)sizeof(fs_seek_rep_t)) return 0;
  fs_seek_rep_t r;
  memcpy(&r, rep.data, sizeof(r));
  return r.new_pos;
}

static int fh_read_exact(cap_handle_t fh, void *buf, uint32_t size) {
  uint8_t *dst = (uint8_t *)buf;
  uint32_t done = 0;
  while (done < size) {
    uint32_t want = (size - done < 256u) ? (size - done) : 256u;
    int n = sys_read(fh, dst + done, want);
    if (n <= 0) return -1;
    done += (uint32_t)n;
  }
  return 0;
}

/* ---- OPEN handler ---- */

static void handle_open(const sys_ipc_msg_t *req) {
  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));

  static const char elf32_path[]    = "elfloader:elf32";
  static const char elf32_fh_path[] = "elfloader:elf32-fh";

  if (req->num_bytes == sizeof(elf32_path) - 1 &&
      memcmp(req->data, elf32_path, req->num_bytes) == 0) {
    sys_open_reply_t oreply = { .allowed_ops = KOP_EXEC | KOP_CALL };
    reply.object_id = ELF32_OID;
    reply.num_bytes  = sizeof(oreply);
    memcpy(reply.data, &oreply, sizeof(oreply));
  } else if (req->num_bytes == sizeof(elf32_fh_path) - 1 &&
             memcmp(req->data, elf32_fh_path, req->num_bytes) == 0) {
    sys_open_reply_t oreply = { .allowed_ops = KOP_EXEC | KOP_CALL };
    reply.object_id = ELF32_FH_OID;
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
      eh->e_phentsize != sizeof(Elf32_Phdr)) {
    goto done;
  }

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
  if (elf_cap)       sys_cap_close(elf_cap);
  if (self_vspace)   sys_cap_close(self_vspace);
  if (target_vspace) sys_cap_close(target_vspace);
  if (stdin_cap)     sys_cap_close(stdin_cap);
  if (stdout_cap)    sys_cap_close(stdout_cap);
  if (stderr_cap)    sys_cap_close(stderr_cap);

  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));
  if (watch_cap) {
    reply.num_handles = 1;
    reply.handles[0]  = watch_cap;
  }
  sys_reply(&reply);

  if (watch_cap) sys_cap_close(watch_cap);  /* transferred by sys_reply */
}

/* ---- PAB writer ---- */

/*
 * Write a Process Args Block onto a child's stack buffer.
 *
 * buf        — pointer to stack pages in elfloader's vspace (TMP_STACK_BASE)
 * buf_size   — total stack size in bytes (USER_STACK_SIZE)
 * stack_base — first virtual address of the stack in the child's vspace
 * exec_data  — raw sys_ipc_msg_t.data from the exec message (proc_exec_args_t)
 * exec_nbytes — num_bytes from the exec message
 *
 * Returns the byte offset from buf where argc is written; the child's initial
 * esp = stack_base + returned offset.  Returns buf_size on failure.
 */
static uint32_t pab_write(uint8_t *buf, uint32_t buf_size, uint32_t stack_base,
                          const uint8_t *exec_data, uint32_t exec_nbytes) {
  const uint8_t *argv_blob = NULL, *envp_blob = NULL;
  uint32_t argv_bytes = 0, envp_bytes = 0;

  if (exec_nbytes >= PROC_EXEC_ARGS_HDR_SIZE) {
    const proc_exec_args_t *pea = (const proc_exec_args_t *)exec_data;
    uint32_t ab = pea->argv_bytes, eb = pea->envp_bytes;
    if (ab + eb <= PROC_EXEC_ARGS_BLOB_MAX &&
        PROC_EXEC_ARGS_HDR_SIZE + ab + eb <= exec_nbytes) {
      argv_bytes = ab; envp_bytes = eb;
      argv_blob  = pea->blobs;
      envp_blob  = pea->blobs + ab;
    }
  }

  /* Count args by counting null terminators. */
  uint32_t argc = 0, envc = 0;
  for (uint32_t i = 0; i < argv_bytes; i++) if (argv_blob[i] == '\0') argc++;
  for (uint32_t i = 0; i < envp_bytes; i++) if (envp_blob[i] == '\0') envc++;

  uint32_t str_size  = argv_bytes + envp_bytes;
  uint32_t auxv_size = 3u * (uint32_t)sizeof(proc_auxv_t);
  uint32_t envp_size = (envc + 1u) * 4u;
  uint32_t argv_size = (argc + 1u) * 4u;
  uint32_t argc_size = 4u;

  /* Strings go at the top (high end); structured data is placed below, 4-byte aligned. */
  if (str_size > buf_size) return buf_size;
  uint32_t str_off   = buf_size - str_size;
  uint32_t struct_top = str_off & ~3u;

  if (auxv_size + envp_size + argv_size + argc_size > struct_top) return buf_size;

  /* Copy strings. */
  if (argv_bytes > 0) memcpy(buf + str_off,              argv_blob, argv_bytes);
  if (envp_bytes > 0) memcpy(buf + str_off + argv_bytes, envp_blob, envp_bytes);

  /* Build structured area downward from struct_top. */
  uint32_t off = struct_top;

  off -= auxv_size;
  proc_auxv_t *auxv = (proc_auxv_t *)(buf + off);
  auxv[0].type = AT_CAPS_BASE;  auxv[0].value = 0u;
  auxv[1].type = AT_CAPS_COUNT; auxv[1].value = 3u;
  auxv[2].type = AT_NULL;       auxv[2].value = 0u;

  off -= envp_size;
  uint32_t *envp_ptrs = (uint32_t *)(buf + off);
  {
    uint32_t soff = argv_bytes;
    for (uint32_t i = 0; i < envc; i++) {
      envp_ptrs[i] = stack_base + str_off + soff;
      while (soff < str_size && buf[str_off + soff] != '\0') soff++;
      soff++;
    }
    envp_ptrs[envc] = 0u;
  }

  off -= argv_size;
  uint32_t *argv_ptrs = (uint32_t *)(buf + off);
  {
    uint32_t soff = 0u;
    for (uint32_t i = 0; i < argc; i++) {
      argv_ptrs[i] = stack_base + str_off + soff;
      while (soff < argv_bytes && buf[str_off + soff] != '\0') soff++;
      soff++;
    }
    argv_ptrs[argc] = 0u;
  }

  off -= argc_size;
  *(uint32_t *)(buf + off) = argc;

  return off;
}

/* ---- EXEC-FH handler (reads ELF via file handle, no page-cap) ---- */

static void handle_exec_fh(const sys_ipc_msg_t *req) {
  cap_handle_t stdin_cap  = (cap_handle_t)req->handles[0];
  cap_handle_t stdout_cap = (cap_handle_t)req->handles[1];
  cap_handle_t stderr_cap = (cap_handle_t)req->handles[2];
  cap_handle_t file_h     = (cap_handle_t)req->handles[3];

  const char *argv0;
  if (req->num_bytes >= PROC_EXEC_ARGS_HDR_SIZE) {
    const proc_exec_args_t *pea = (const proc_exec_args_t *)req->data;
    argv0 = (pea->argv_bytes > 0) ? (const char *)pea->blobs : "elf";
  } else {
    argv0 = (req->num_bytes > 0) ? (const char *)req->data : "elf";
  }

  cap_handle_t self_vspace   = 0;
  cap_handle_t target_vspace = 0;
  cap_handle_t watch_cap     = 0;

  /* Stack-local ELF header + phdrs (max 32 phdrs × 32 bytes = 1024 bytes). */
  uint8_t ehdr_buf[52];
  uint8_t phdr_buf[32 * sizeof(Elf32_Phdr)];

  if (file_h == 0) goto done;

  self_vspace = sys_vspace_self();
  if (self_vspace == 0) goto done;

  /* Read ELF header (52 bytes; file cursor starts at 0). */
  if (fh_read_exact(file_h, ehdr_buf, sizeof(ehdr_buf)) != 0) goto done;

  {
    const Elf32_Ehdr *eh = (const Elf32_Ehdr *)ehdr_buf;
    if (eh->e_ident[0] != ELFMAG0 || eh->e_ident[1] != ELFMAG1 ||
        eh->e_ident[2] != ELFMAG2 || eh->e_ident[3] != ELFMAG3 ||
        eh->e_ident[4] != ELFCLASS32 || eh->e_ident[5] != ELFDATA2LSB ||
        eh->e_version   != EV_CURRENT || eh->e_machine != EM_386 ||
        (eh->e_type != ET_EXEC && eh->e_type != ET_DYN) ||
        eh->e_phentsize != sizeof(Elf32_Phdr)) {
      goto done;
    }

    uint32_t phdr_bytes = (uint32_t)eh->e_phnum * sizeof(Elf32_Phdr);
    if (phdr_bytes > sizeof(phdr_buf)) goto done;

    fh_seek(file_h, (int32_t)eh->e_phoff, FS_SEEK_SET);
    if (fh_read_exact(file_h, phdr_buf, phdr_bytes) != 0) goto done;

    uint32_t base        = (eh->e_type == ET_DYN) ? USER_ENTRY_BASE : 0u;
    uint32_t entry_point = base + eh->e_entry;

    target_vspace = sys_vspace_create();
    if (target_vspace == 0) goto done;

    for (uint32_t i = 0; i < eh->e_phnum; i++) {
      const Elf32_Phdr *ph = (const Elf32_Phdr *)(phdr_buf + i * sizeof(Elf32_Phdr));
      if (ph->p_type != PT_LOAD || ph->p_memsz == 0) continue;

      uint32_t seg_va    = base + ph->p_vaddr;
      uint32_t map_begin = pg_floor(seg_va);
      uint32_t map_end   = pg_ceil(seg_va + ph->p_memsz);
      uint32_t npages    = (map_end - map_begin) / PAGE_SIZE_U;

      cap_handle_t seg_cap = sys_page_alloc(npages, 0, 0);
      if (seg_cap == 0) goto done;

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

      uint8_t *tmp = (uint8_t *)TMP_SEG_BASE + (seg_va - map_begin);

      fh_seek(file_h, (int32_t)ph->p_offset, FS_SEEK_SET);
      if (fh_read_exact(file_h, tmp, ph->p_filesz) != 0) {
        sys_vspace_unmap(self_vspace, TMP_SEG_BASE, npages);
        sys_cap_close(seg_cap);
        goto done;
      }
      if (ph->p_memsz > ph->p_filesz)
        memset(tmp + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);

      sys_vspace_unmap(self_vspace, TMP_SEG_BASE, npages);

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

    /* Stack — map into target vspace (permanent) and self (temporary to write PAB). */
    uint32_t user_sp = 0;
    {
      cap_handle_t stk = sys_page_alloc(USER_STACK_PAGES, 0, 0);
      if (stk == 0) goto done;

      sys_vspace_map_args_t stk_tgt = {
        .vspace_cap = target_vspace,
        .virt_addr  = USER_STACK_BASE,
        .page_cap   = stk,
        .prot_flags = VMM_PROT_READ | VMM_PROT_WRITE,
      };
      if (sys_vspace_map(&stk_tgt) != 0) { sys_cap_close(stk); goto done; }

      sys_vspace_map_args_t stk_tmp = {
        .vspace_cap = self_vspace,
        .virt_addr  = TMP_STACK_BASE,
        .page_cap   = stk,
        .prot_flags = VMM_PROT_READ | VMM_PROT_WRITE,
      };
      if (sys_vspace_map(&stk_tmp) != 0) { sys_cap_close(stk); goto done; }
      sys_cap_close(stk);

      uint32_t argc_off = pab_write(
          (uint8_t *)TMP_STACK_BASE, USER_STACK_SIZE, USER_STACK_BASE,
          req->data, req->num_bytes);
      sys_vspace_unmap(self_vspace, TMP_STACK_BASE, USER_STACK_PAGES);

      if (argc_off >= USER_STACK_SIZE) goto done;
      user_sp = USER_STACK_BASE + argc_off;
    }

    {
      cap_handle_t proc_spawn_h = sys_open("proc:spawn", 0);
      if (proc_spawn_h == 0) goto done;

      sys_ipc_msg_t spawn_req, spawn_rep;
      memset(&spawn_req, 0, sizeof(spawn_req));
      memset(&spawn_rep, 0, sizeof(spawn_rep));

      spawn_req.opcode      = IPC_OP_EXEC;
      spawn_req.num_handles = 4;
      size_t av_len = strlen(argv0) + 1u;
      spawn_req.num_bytes = (uint32_t)(8u + av_len);
      memcpy(spawn_req.data + 0, &entry_point, 4);
      memcpy(spawn_req.data + 4, &user_sp,     4);
      memcpy(spawn_req.data + 8, argv0, av_len);

      spawn_req.handles[0] = target_vspace;
      spawn_req.handles[1] = stdin_cap;
      spawn_req.handles[2] = stdout_cap;
      spawn_req.handles[3] = stderr_cap;

      int rc = sys_call(proc_spawn_h, &spawn_req, &spawn_rep);
      sys_cap_close(proc_spawn_h);

      if (rc == 0 && spawn_rep.handles[0] != 0)
        watch_cap = (cap_handle_t)spawn_rep.handles[0];
    }
  }  /* end block containing eh/entry_point/base */

done:
  /* Close file handle — sends IPC_OP_CLOSE to backend exactly once. */
  if (file_h) sys_close(file_h);
  if (self_vspace)   sys_cap_close(self_vspace);
  if (target_vspace) sys_cap_close(target_vspace);
  if (stdin_cap)     sys_cap_close(stdin_cap);
  if (stdout_cap)    sys_cap_close(stdout_cap);
  if (stderr_cap)    sys_cap_close(stderr_cap);

  sys_ipc_msg_t reply;
  memset(&reply, 0, sizeof(reply));
  if (watch_cap) {
    reply.num_handles = 1;
    reply.handles[0]  = watch_cap;
  }
  sys_reply(&reply);
  if (watch_cap) sys_cap_close(watch_cap);
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
      else if (req.object_id == ELF32_FH_OID)
        handle_exec_fh(&req);
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
