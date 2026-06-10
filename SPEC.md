# Arachnyaa Resource Model Spec

## Summary

Arachnyaa keeps the kernel small and generic. The kernel owns process scheduling, address spaces, capability tables, synchronous IPC, and a per-process protocol namespace. User space resolves resources through names of the form `<protocol>:<opaque-path>`; the kernel only splits on the first `:` and routes everything after it unchanged to the registered handler.

---

## Frozen Rules

### Namespace model

- Each process owns a namespace map from protocol string to handler endpoint.
- A child process inherits a snapshot of the parent namespace at spawn time.
- Mutations only affect the current process and children spawned later from it.
- The kernel never parses opaque paths past the first `:`.
- `""` (empty string) is a valid protocol key. `sys_open("name", 0)` with no `:` is equivalent to `sys_open(":name", 0)` — both resolve via the `""` binding. This lets any service act as the default namespace for bare names.

### Capability and handle model

- Namespace bindings store referenced endpoint objects, not process-local handles.
- `sys_open` resolves a protocol once and returns a `KOBJ_REMOTE` handle bound to the producing endpoint and a server-assigned `object_id`.
- All later operations dispatch through the bound endpoint using the stored `object_id`.
- Capabilities carry per-handle rights bits enforced by the kernel.

### IPC model

- `sys_call` — synchronous send; caller blocks until server replies.
- `sys_recv` — server blocks on an endpoint until a message arrives.
- `sys_reply` — server completes the active call.
- `sys_defer_call` — server parks the active call, returns a token; reply later with `sys_reply_to`.
- Messages (`sys_ipc_msg_t`) carry up to 256 inline bytes + 4 capability handles. The kernel copies message data and transfers handles across process cap tables.

---

## Syscall ABI

Syscalls use `int $0x80`. Arguments in `ebx`, `ecx`, `edx`, `esi`. Return: 64-bit value in `eax` (low) / `edi` (high). Negative return = `kerror_t` error code.

| # | Name | Arguments | Return |
|---|------|-----------|--------|
| 0x00 | `SYS_EXIT` | status | — |
| 0x01 | `SYS_NS_BIND` | protocol\_str, handler\_cap, declared\_ops | error |
| 0x02 | `SYS_OPEN` | name\_str, flags | cap\_handle |
| 0x03 | `SYS_WRITE` | handle, buf, len | bytes\_written |
| 0x04 | `SYS_CAP_CLOSE` | handle | error |
| 0x05 | `SYS_PUTC` | char | — *(debug only)* |
| 0x06 | `SYS_CALL` | handle, \*msg, \*reply | error |
| 0x07 | `SYS_REPLY` | \*reply\_msg | error |
| 0x08 | `SYS_RECV` | endpoint\_handle, \*out\_msg | error |
| 0x09 | `SYS_SPAWN` | \*args, \*out\_pid, \*out\_cap | error |
| 0x0A | `SYS_READ` | handle, buf, len | bytes\_read |
| 0x0B | `SYS_CLOSE` | handle | error |
| 0x0C | `SYS_EP_CREATE` | — | cap\_handle |
| 0x0D | `SYS_PROC_WAIT` | proc\_cap | exit\_code |
| 0x0E | `SYS_IRQ_CLAIM` | irq\_num | cap\_handle |
| 0x0F | `SYS_IRQ_WAIT` | irq\_cap | error |
| 0x10 | `SYS_IRQ_NOTIFY` | irq\_cap, endpoint\_cap | error |
| 0x11 | `SYS_IO_IN` | port | value |
| 0x12 | `SYS_IO_OUT` | port, value | — |
| 0x13 | `SYS_VSPACE_CREATE` | — | cap\_handle |
| 0x14 | `SYS_VSPACE_SELF` | — | cap\_handle |
| 0x15 | `SYS_PAGE_ALLOC` | num\_pages, flags, phys\_addr | cap\_handle |
| 0x16 | `SYS_VSPACE_MAP` | \*args | error |
| 0x17 | `SYS_VSPACE_UNMAP` | vspace\_cap, virt\_addr, num\_pages | error |
| 0x18 | `SYS_DEFER_CALL` | — | token |
| 0x19 | `SYS_REPLY_TO` | token, \*msg | error |

`SYS_PAGE_ALLOC` flags:

| Flag | Value | Meaning |
|------|-------|---------|
| `SYS_PAGE_F_FIXED` | 0x1 | Wrap an existing physical range; do not allocate new frames |

`SYS_VSPACE_MAP` takes a `sys_vspace_map_args_t` struct: `{vspace_cap, virt_addr, page_cap, prot_flags}`. `prot_flags` is a bitmask of `VMM_PROT_READ`, `VMM_PROT_WRITE`, `VMM_PROT_EXEC`.

---

## IPC Protocol Conventions

### `IPC_OP_OPEN`

Sent by the kernel when a process calls `sys_open("<protocol>:<path>")` or `sys_open("<bare-name>")`.

**Request** (received by server via `sys_recv`):
- `opcode = IPC_OP_OPEN`
- `data[0..num_bytes)` = the full resource name as passed to `sys_open`, i.e. `"<protocol>:<path>"` (or `":<path>"` when the `""` default namespace matched)
- `object_id = 0`, `num_handles = 0`

**Reply** (server calls `sys_reply`):
- `object_id` = server-assigned non-zero object id
- `data` = `sys_open_reply_t { uint32_t allowed_ops; }`
- `num_bytes = sizeof(sys_open_reply_t)`

The kernel clamps: `final_ops = reply.allowed_ops & binding->declared_ops`. On success it creates a `KOBJ_REMOTE` bound to the endpoint + `object_id` + `final_ops`.

### `IPC_OP_EXEC` (convention, not a kernel syscall)

Used by any service that can execute a resource (e.g. `bm:`, `elfloader:`). The shell identifies executable handles by checking `KOP_EXEC` in `allowed_ops` returned by `IPC_OP_OPEN`.

**Request**:
- `opcode = IPC_OP_EXEC`
- `handles[0..2]` = stdin, stdout, stderr caps (transferred to loader)
- `handles[3]` = ELF page cap (for `elfloader:`) or omitted
- `data` = argv0 (null-terminated string)

**Reply**:
- `handles[0]` = watch cap (`KOBJ_PROC` with `R_PROC_WAIT`); 0 on failure

---

## User-Space Service Protocols

### `log:` (served by `initd`)

| Path | Ops | Behavior |
|------|-----|----------|
| `log:stdout` | WRITE | Writes bytes to the kernel TTY debug output |

### `bm:` (served by `initd`)

Exposes boot modules loaded by the multiboot bootloader. Also bound as the `""` (default) namespace, so bare names like `sys_open("shell", 0)` resolve here without an explicit `bm:` prefix.

| Path | Ops | Behavior |
|------|-----|----------|
| `bm:<name>` or `<name>` | READ | Returns a `KOBJ_VMOBJ` page cap over the module's physical memory |
| `bm:<name>` or `<name>` | EXEC | Forwards to `elfloader:elf32` with the module's page cap + stdio; returns watch cap |

### `tty:` (served by `ttyd`)

Virtual terminal service. Manages VGA text framebuffer and PS/2 keyboard.

| Path | Ops | Behavior |
|------|-----|----------|
| `tty:<N>` | READ, WRITE, CLOSE | Attach to virtual terminal N (0-indexed); READ blocks until a newline |
| `tty:` | READ, WRITE, CLOSE | Allocate a new virtual terminal |
| `tty:focus` | WRITE | Write a `uint32_t` vterm index to switch the focused (visible) terminal |

Writes to a `tty:N` handle support ANSI escape sequences:

| Sequence | Effect |
|----------|--------|
| `\033[2J` | Clear entire screen |
| `\033[H` | Move cursor to top-left (1,1) |
| `\033[row;colH` | Move cursor to row, col (1-indexed) |
| `\033[0K` | Erase from cursor to end of line |
| `\033[1K` | Erase from start of line to cursor |
| `\033[2K` | Erase entire current line |

IRQ notifications arrive on the server endpoint as `IPC_OP_NOTIFY` messages with the raw scancode in `data[0]`.

### `proc:` (served by `procd`)

| Path | Ops | Behavior |
|------|-----|----------|
| `proc:spawn` | CALL | Spawn a process from a prepared address space; see below |

**`proc:spawn` CALL request**:
- `data[0..3]` = entry\_point (uint32\_t)
- `data[4..7]` = user\_sp (uint32\_t)
- `data[8..]` = argv0 (null-terminated)
- `handles[0]` = vspace cap (transferred; becomes child's address space)
- `handles[1..3]` = stdin, stdout, stderr caps (transferred to child)

**Reply**:
- `handles[0]` = watch cap (`KOBJ_PROC`)

### `elfloader:` (served by `elfloader`)

| Path | Ops | Behavior |
|------|-----|----------|
| `elfloader:elf32` | EXEC | Load an ELF32 binary, create address space, spawn via `proc:spawn` |

Follows the `IPC_OP_EXEC` convention: `handles[3]` is a `KOBJ_VMOBJ` page cap whose physical memory contains the ELF image. The loader maps segments into a new address space and delegates spawn to `proc:spawn`.

---

## Boot Path

1. Multiboot bootloader loads kernel + modules (`initd`, `ttyd`, `procd`, `elfloader`, `shell`, …).
2. `kmain` initializes TTY → IDT → PIT → PMM → VMM → kheap → TSS → keyboard → scheduler.
3. `initd` ELF is loaded from the multiboot module list and spawned as PID 1.
4. A bootstrap endpoint capability is installed into PID 1's cap table.
5. `initd` binds `log:`, `bm:`, and `""` (default namespace → bm), then spawns `ttyd`, `procd`, and `elfloader` via `SYS_SPAWN` (kernel ELF loader, boot-module path).
6. `initd` spawns `shell` via `elfloader:elf32` using a fixed vmobj over the shell module's physical memory.
7. Scheduler starts; kernel never returns to `kmain`.

The kernel ELF loader (`SYS_SPAWN`) is used only for the initial service bootstrap (PID 1 through `elfloader`). All subsequent program loading goes through the `bm:` → `elfloader:elf32` → `proc:spawn` user-space pipeline.
