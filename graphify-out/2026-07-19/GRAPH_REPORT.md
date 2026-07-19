# Graph Report - arachnyaa  (2026-07-18)

## Corpus Check
- 137 files · ~45,750 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 766 nodes · 1867 edges · 93 communities (84 shown, 9 thin omitted)
- Extraction: 64% EXTRACTED · 36% INFERRED · 0% AMBIGUOUS · INFERRED: 679 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `36b8399c`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- [[_COMMUNITY_Boot & TTY Drivers|Boot & TTY Drivers]]
- [[_COMMUNITY_Kernel Bootstrap & IPC|Kernel Bootstrap & IPC]]
- [[_COMMUNITY_Capability Resolution & Strings|Capability Resolution & Strings]]
- [[_COMMUNITY_Hardware IO & IRQ|Hardware I/O & IRQ]]
- [[_COMMUNITY_Capability Table Management|Capability Table Management]]
- [[_COMMUNITY_ELF Loader & initd Services|ELF Loader & initd Services]]
- [[_COMMUNITY_Process Namespace & Context|Process Namespace & Context]]
- [[_COMMUNITY_Address Space & ELF Parsing|Address Space & ELF Parsing]]
- [[_COMMUNITY_User-Space Syscall Library|User-Space Syscall Library]]
- [[_COMMUNITY_TTY Framebuffer & Keyboard|TTY Framebuffer & Keyboard]]
- [[_COMMUNITY_Memory Manager & Scheduler|Memory Manager & Scheduler]]
- [[_COMMUNITY_Project Docs & Roadmap|Project Docs & Roadmap]]
- [[_COMMUNITY_Virtual Memory Manager|Virtual Memory Manager]]
- [[_COMMUNITY_Memory Free Tracker (AVL)|Memory Free Tracker (AVL)]]
- [[_COMMUNITY_Interactive Shell|Interactive Shell]]
- [[_COMMUNITY_User Program CMake Targets|User Program CMake Targets]]
- [[_COMMUNITY_Syscall Dispatcher|Syscall Dispatcher]]
- [[_COMMUNITY_User Printf Library|User Printf Library]]
- [[_COMMUNITY_Kernel CMake Build|Kernel CMake Build]]
- [[_COMMUNITY_Kernel Source CMake Groups|Kernel Source CMake Groups]]
- [[_COMMUNITY_Misc include kernel kobj h kernel k|Misc: include kernel kobj h kernel k]]
- [[_COMMUNITY_Misc spec syscall abi|Misc: spec syscall abi]]
- [[_COMMUNITY_Community 78|Community 78]]
- [[_COMMUNITY_Community 79|Community 79]]
- [[_COMMUNITY_Community 80|Community 80]]
- [[_COMMUNITY_Community 81|Community 81]]
- [[_COMMUNITY_Community 82|Community 82]]
- [[_COMMUNITY_Community 83|Community 83]]
- [[_COMMUNITY_Community 88|Community 88]]
- [[_COMMUNITY_Community 89|Community 89]]
- [[_COMMUNITY_Community 90|Community 90]]
- [[_COMMUNITY_Community 91|Community 91]]
- [[_COMMUNITY_Community 92|Community 92]]

## God Nodes (most connected - your core abstractions)
1. `memset()` - 46 edges
2. `sys_reply()` - 42 edges
3. `memcpy()` - 36 edges
4. `kmain()` - 36 edges
5. `scheduler_get_current()` - 34 edges
6. `syscall_dispatcher()` - 30 edges
7. `handle_exec_fh()` - 23 edges
8. `kfree()` - 23 edges
9. `cap_handle_t` - 21 edges
10. `handle_exec()` - 20 edges

## Surprising Connections (you probably didn't know these)
- `_start()` --calls--> `sys_open()`  [INFERRED]
  user/shell/main.c → src/sys/namespace.c
- `vfs_stat_type()` --calls--> `sys_call()`  [INFERRED]
  user/vfs/main.c → src/sys/ipc.c
- `handle_read()` --calls--> `sys_read()`  [INFERRED]
  user/vfs/main.c → src/sys/read.c
- `handle_exec()` --calls--> `sys_call()`  [INFERRED]
  user/vfs/main.c → src/sys/ipc.c
- `handle_fs_op()` --calls--> `sys_call()`  [INFERRED]
  user/vfs/main.c → src/sys/ipc.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **User-Space Execution Pipeline** — spec_bm_protocol, spec_elfloader_elf32, spec_proc_spawn [EXTRACTED 0.95]
- **Boot Service Initialization Sequence** — spec_initd, spec_ttyd, spec_procd, spec_elfloader [EXTRACTED 0.95]
- **Kernel Core Concepts** — spec_kobj, spec_capability_system, spec_synchronous_ipc, spec_protocol_namespace [EXTRACTED 0.95]
- **User-Space Executable Programs** — initd_cmakelists_initd_target, ttyd_cmakelists_ttyd_target, procd_cmakelists_procd_target, elfloader_cmakelists_elfloader_target, shell_cmakelists_shell_target [EXTRACTED 0.95]
- **Kernel Source Groups** — kernel_cmakelists_kernel_sources, mm_cmakelists_mm_sources, process_cmakelists_process_sources, sys_cmakelists_sys_sources [EXTRACTED 0.95]

## Communities (93 total, 9 thin omitted)

### Community 0 - "Boot & TTY Drivers"
Cohesion: 0.24
Nodes (20): kerror_t, invlpg(), page_fault_handler(), phys_to_virt(), read_cr2(), serial_init_once(), serial_putc(), serial_puthex() (+12 more)

### Community 1 - "Kernel Bootstrap & IPC"
Cohesion: 0.06
Nodes (83): cap_rights_t, cap_sys_arg_t, cap_table_t, bm_vmobj_release(), bootstrap_endpoint_create(), endpoint_release(), process_install_boot_manifest_cap(), process_install_bootstrap_log_handler() (+75 more)

### Community 2 - "Capability Resolution & Strings"
Cohesion: 0.14
Nodes (28): spinlock_t, ipc_call_t, ipc_call(), ipc_call_create(), ipc_call_destroy(), ipc_call_enqueue(), ipc_endpoint_destroy(), ipc_recv_next() (+20 more)

### Community 3 - "Hardware I/O & IRQ"
Cohesion: 0.12
Nodes (44): align_up(), avl_delete_by_start(), avl_insert_exact(), balance_factor(), find_fit(), find_predecessor(), find_successor(), height() (+36 more)

### Community 4 - "Capability Table Management"
Cohesion: 0.29
Nodes (17): kmain(), tty_write_dec(), tty_write_hex(), tty_writestring(), pmm_alloc_frame(), pmm_bitmap_clear(), pmm_bitmap_set(), pmm_bitmap_test() (+9 more)

### Community 5 - "ELF Loader & initd Services"
Cohesion: 0.08
Nodes (76): cap_handle_t, handle_open(), bind_bm_protocol(), bind_log_protocol(), bm_find_module(), dbgwrite(), handle_bm_close(), handle_bm_exec() (+68 more)

### Community 6 - "Process Namespace & Context"
Cohesion: 0.08
Nodes (44): arch_context_t, kobj_init(), contains_colon(), process_namespace_bind(), process_namespace_destroy(), process_namespace_inherit(), process_namespace_init(), process_namespace_lookup() (+36 more)

### Community 7 - "Address Space & ELF Parsing"
Cohesion: 0.15
Nodes (22): Elf32_Ehdr, elf_load_result_t, as_free(), as_map(), as_memcpy(), as_zero(), round_page(), vma_insert() (+14 more)

### Community 8 - "User-Space Syscall Library"
Cohesion: 0.07
Nodes (71): main(), fh_read_exact(), fh_seek(), handle_exec(), handle_exec_fh(), pab_write(), pg_ceil(), pg_floor() (+63 more)

### Community 9 - "TTY Framebuffer & Keyboard"
Cohesion: 0.11
Nodes (30): dbgprint(), _start(), fb_attr(), fb_blit(), fb_clear(), fb_init(), fb_set_cursor(), kb_init() (+22 more)

### Community 10 - "Memory Manager & Scheduler"
Cohesion: 0.08
Nodes (39): arch_registers_t, as_t, irq_flags_t, irq_exit_tail(), irq_rm_flag(), irq_set_flag(), as_get(), as_load_ptable() (+31 more)

### Community 11 - "Project Docs & Roadmap"
Cohesion: 0.11
Nodes (17): Arachnyaa Resource Model Spec, `bm:` (served by `initd`), Boot Path, Capability and handle model, `elfloader:` (served by `elfloader`), Frozen Rules, IPC model, `IPC_OP_EXEC` (convention, not a kernel syscall) (+9 more)

### Community 12 - "Virtual Memory Manager"
Cohesion: 0.16
Nodes (17): pipe_buf_put(), pipe_close_read(), pipe_close_write(), pipe_create(), pipe_read(), _pipe_read_release(), pipe_write(), _pipe_write_release() (+9 more)

### Community 13 - "Memory Free Tracker (AVL)"
Cohesion: 0.27
Nodes (10): vga_color(), serial_init(), tty_initialize(), tty_put_entry_at(), tty_putc(), tty_scroll(), tty_set_color(), tty_update_cursor() (+2 more)

### Community 14 - "Interactive Shell"
Cohesion: 0.13
Nodes (25): cap_entry_t, cap_resolve(), mempcpy(), stpcpy(), strcat(), strcpy(), strlen(), cap_handle_t (+17 more)

### Community 16 - "Syscall Dispatcher"
Cohesion: 0.25
Nodes (6): kprint(), kprint_char(), kprint_hex(), irq_has_notify_ep(), pic_send_eoi(), isr_common_stub_handler()

### Community 17 - "User Printf Library"
Cohesion: 0.80
Nodes (4): emit_int(), emit_str(), emit_uint(), snprintf()

### Community 34 - "Misc: include kernel kobj h kernel k"
Cohesion: 0.32
Nodes (7): multiboot_find_module(), multiboot_get_info(), multiboot_map_bootinfo(), multiboot_set_info(), multiboot_unmap_bootinfo(), multiboot_module_t, multiboot_info_t

### Community 65 - "Misc: spec syscall abi"
Cohesion: 0.13
Nodes (14): Arachnyaa Roadmap, Completed, Core kernel substrate, Default namespace, Extended kernel primitives, Filesystem / persistent storage, Multiple virtual terminals, Namespace isolation on spawn (+6 more)

### Community 78 - "Community 78"
Cohesion: 0.33
Nodes (5): Arachnyaa Implementation Plan, Milestone 1: Core substrate ✓, Milestone 2: First vertical slice ✓, Milestone 3: Interactive shell ✓, Milestone 4: Pipes and filesystem

### Community 80 - "Community 80"
Cohesion: 0.40
Nodes (4): arachnyaa, Architecture, Build, Current State

### Community 82 - "Community 82"
Cohesion: 0.15
Nodes (19): inb(), outb(), keyboard_handle_scancode(), keyboard_init(), serial_putc(), sys_io_in(), sys_io_out(), hal_initialize() (+11 more)

### Community 83 - "Community 83"
Cohesion: 0.47
Nodes (5): cap_handle_t, kobj_t, process_t, _cap_resolve(), sys_read()

### Community 92 - "Community 92"
Cohesion: 0.50
Nodes (3): arch_sys_exit(), arch_sys_putc(), arch_sys_write()

## Knowledge Gaps
- **88 isolated node(s):** `vfs_mount_t`, `cap_handle_t`, `sys_pipe_result_t`, `kobj_t`, `process_t` (+83 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **9 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `scheduler_get_current()` connect `Kernel Bootstrap & IPC` to `Boot & TTY Drivers`, `Capability Resolution & Strings`, `ELF Loader & initd Services`, `Process Namespace & Context`, `Memory Manager & Scheduler`, `Virtual Memory Manager`, `Interactive Shell`, `Community 83`?**
  _High betweenness centrality (0.124) - this node is a cross-community bridge._
- **Why does `kmain()` connect `Capability Table Management` to `Boot & TTY Drivers`, `Kernel Bootstrap & IPC`, `Misc: include kernel kobj h kernel k`, `Process Namespace & Context`, `Memory Manager & Scheduler`, `Memory Free Tracker (AVL)`, `Syscall Dispatcher`, `Community 82`?**
  _High betweenness centrality (0.108) - this node is a cross-community bridge._
- **Why does `syscall_dispatcher()` connect `User-Space Syscall Library` to `Syscall Dispatcher`, `Community 92`, `ELF Loader & initd Services`, `Interactive Shell`?**
  _High betweenness centrality (0.076) - this node is a cross-community bridge._
- **Are the 45 inferred relationships involving `memset()` (e.g. with `fh_seek()` and `handle_exec()`) actually correct?**
  _`memset()` has 45 INFERRED edges - model-reasoned connections that need verification._
- **Are the 38 inferred relationships involving `sys_reply()` (e.g. with `handle_exec()` and `handle_exec_fh()`) actually correct?**
  _`sys_reply()` has 38 INFERRED edges - model-reasoned connections that need verification._
- **Are the 35 inferred relationships involving `memcpy()` (e.g. with `fh_seek()` and `handle_exec()`) actually correct?**
  _`memcpy()` has 35 INFERRED edges - model-reasoned connections that need verification._
- **Are the 35 inferred relationships involving `kmain()` (e.g. with `multiboot_find_module()` and `multiboot_map_bootinfo()`) actually correct?**
  _`kmain()` has 35 INFERRED edges - model-reasoned connections that need verification._