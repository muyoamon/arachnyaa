# arachnyaa

Hobby x86 (i686) microkernel OS written in C and NASM assembly.

The kernel is intentionally minimal: scheduling, address spaces, capability tables, synchronous IPC, and per-process protocol namespaces. All resource access goes through user-space services discovered via `<protocol>:<opaque-path>` names.

## Current State

The system boots to an interactive shell. The following services run in user space:

| Service | Protocol | Role |
|---------|----------|------|
| `initd` | `log:`, `bm:`, `""` | PID 1; bootstraps namespace; exposes boot modules; `""` is the default namespace |
| `ttyd` | `tty:` | VGA text output, PS/2 keyboard input, virtual terminals |
| `procd` | `proc:` | Process spawning and death watching |
| `elfloader` | `elfloader:` | User-space ELF loader; maps segments, sets up address space |
| `shell` | — | Interactive command shell |

Shell features:
- Bare command names (no `:` prefix) resolve via the `""` default namespace binding (currently `bm:`)
- Explicit protocol paths (`proto:name`) passed through directly
- Builtins: `exit`, `set VAR VALUE`, `unset VAR`, `clear`
- Waits for child processes to exit before re-prompting
- ANSI escape support in the terminal (`\033[2J`, `\033[H`, cursor positioning)

## Architecture

```
user/          — user-space programs (initd, ttyd, procd, elfloader, shell)
src/sys/       — syscall handler implementations (kernel side)
src/kernel/    — core kernel: caps, kobj, IPC, namespace, scheduling
src/mm/        — physical/virtual memory, kernel heap
src/process/   — process/thread lifecycle, ELF loader, scheduler
src/arch/x86/  — GDT, IDT, TSS, paging, context switch, syscall entry
src/drivers/   — keyboard, TTY (kernel-side debug only)
src/boot/      — multiboot info parsing
lib/           — freestanding string library
include/       — headers mirroring src/ layout
user/ulib/     — freestanding user-space library (syscall wrappers, string)
```

Key concepts: **kernel objects** (`kobj_t`, ref-counted), **capabilities** (64-bit handles encoding type + generation + index), **synchronous IPC** (`sys_call` / `sys_recv` / `sys_reply`), **protocol namespace** (per-process map from protocol string to handler endpoint, inherited on spawn).

See `SPEC.md` for the full syscall ABI and protocol contracts.

## Build

Requires a cross-compiler toolchain (`i686-elf-gcc`, `nasm`) and `grub-mkrescue`.

```sh
# Build kernel + user programs
cmake --build build/

# Build bootable ISO
cmake --build build/ --target iso

# Run in QEMU
cmake --build build/ --target run-qemu

# Debug with GDB (QEMU pauses at startup, GDB stub on localhost:1234)
cmake --build build/ --target debug-qemu-iso
```

Reconfigure from scratch:

```sh
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-i686-elf.cmake
```

There are no automated tests. Verification is done by booting in QEMU and observing output.
