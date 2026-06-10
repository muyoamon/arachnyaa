# Arachnyaa Roadmap

## Completed

### Core kernel substrate
- Process scheduling, address spaces, capability tables, synchronous IPC
- Per-process protocol namespace with snapshot inheritance on spawn
- Endpoint capabilities, remote resource handles, rights enforcement
- Syscalls: `exit`, `ns_bind`, `open`, `write`, `read`, `close`, `cap_close`, `call`, `recv`, `reply`, `spawn`
- `sys_defer_call` / `sys_reply_to` for deferred server replies

### Extended kernel primitives
- `sys_ep_create` — user-space endpoint creation
- `sys_proc_wait` — block until a child process exits; return exit code
- `sys_irq_claim` / `sys_irq_wait` / `sys_irq_notify` — IRQ ownership and notification
- `sys_io_in` / `sys_io_out` — gated hardware I/O port access
- `sys_vspace_create` / `sys_vspace_self` / `sys_page_alloc` / `sys_vspace_map` / `sys_vspace_unmap` — user-space address space construction

### User-space services
- `initd` (PID 1): namespace bootstrap, `log:` debug output, `bm:` boot-module access
- `ttyd`: VGA text framebuffer, PS/2 keyboard, virtual terminals, ANSI escape sequences
- `procd`: process spawning from prepared address spaces, death watching
- `elfloader`: user-space ELF32 loader; segments mapped via vspace primitives; delegates spawn to `procd`

### Shell
- Interactive shell (`shell`) reading from `tty:0`
- Bare command names resolve via the kernel `""` default namespace binding (no PATH variable)
- Explicit protocol paths (`proto:name`) passed through directly
- Builtins: `exit`, `set VAR VALUE`, `unset VAR`, `clear`
- Waits for child processes to exit before re-prompting
- Clears terminal on startup

### Default namespace
- `sys_ns_bind("", ...)` registers a catch-all handler for bare names (no `:` prefix)
- `sys_open("name", 0)` is equivalent to `sys_open(":name", 0)` — both resolve via `""`
- `initd` binds `""` to the `bm:` endpoint so all inherited namespaces resolve bare names to boot modules
- Composable: any process can rebind `""` to a different service (e.g. a VFS) for its children

### Terminal
- Hardware cursor tracks the active vterm cursor position via VGA CRT registers
- ANSI escape parser in vterm: `\033[2J` (clear screen), `\033[H` / `\033[row;colH` (cursor position), `\033[{0,1,2}K` (erase line variants)

---

## Up Next

### Pipes
A `pipe:` service would let the shell connect programs with `|`. Requires `piped` to relay writes to reads between two endpoint handles, and shell tokenizer changes to split on `|` and wire up stdio caps.

### Filesystem / persistent storage
A simple ramdisk or virtio-blk backed `fs:` service. Would let programs be stored as files rather than multiboot modules.

### Shell quality-of-life
- `$VAR` expansion in command arguments
- Quoted string tokens
- Exit status display

### Namespace isolation on spawn
The shell currently inherits the full initd namespace. An explicit "isolated spawn" mode (empty or subset namespace) would allow sandboxing.

### Multiple virtual terminals
`ttyd` already supports up to 4 vterms and a `tty:focus` handle. Wiring focus-switching to a key combination (e.g. Alt+F1–F4) and spawning one shell per vterm would make multi-terminal use practical.
