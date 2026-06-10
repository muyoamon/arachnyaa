# Arachnyaa Implementation Plan

## Milestone 1: Core substrate ✓

- Kernel object and capability lifecycle (ref-counted `kobj_t`, `cap_table_t`)
- Process capability tables and protocol namespaces initialized on spawn
- Namespace map with snapshot inheritance
- Generic `open`, `write`, `call`, `recv`, `reply` handler dispatch

## Milestone 2: First vertical slice ✓

- `ns_bind`, `open`, `write`, `cap_close` syscalls wired end-to-end
- Bootstrap endpoint installed into PID 1
- `initd` binds `log:`; `log-client` opens and writes through it
- All IPC routed through endpoint/remote-handle transport (no kernel-direct dispatch)

## Milestone 3: Interactive shell ✓

- Extended syscalls: `ep_create`, `proc_wait`, `irq_claim/wait/notify`, `io_in/out`, vspace primitives
- User-space `ulib` (syscall wrappers, string ops)
- `ttyd`: VGA framebuffer, PS/2 keyboard, virtual terminals, ANSI escape sequences, hardware cursor
- `procd`: format-agnostic process spawning from prepared address spaces
- `elfloader`: user-space ELF32 loader using vspace primitives
- `initd` extended: `bm:` boot-module protocol, service bootstrap sequence
- Interactive `shell`: `$PATH` resolution, builtins (`exit`, `set`, `unset`, `clear`), child wait

## Milestone 4: Pipes and filesystem

- `piped`: relay service for stdio pipes; shell `|` syntax
- Ramdisk or virtio-blk backed `fs:` service
- Programs stored as files rather than multiboot modules
- Shell `$VAR` expansion and quoted tokens
