# Graph Report - arachnyaa  (2026-06-27)

## Corpus Check
- 127 files · ~42,863 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 735 nodes · 1737 edges · 82 communities (79 shown, 3 thin omitted)
- Extraction: 66% EXTRACTED · 34% INFERRED · 0% AMBIGUOUS · INFERRED: 592 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `fcb85e58`
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
- [[_COMMUNITY_Misc spec syscall abi|Misc: spec syscall abi]]
- [[_COMMUNITY_Community 78|Community 78]]
- [[_COMMUNITY_Community 79|Community 79]]
- [[_COMMUNITY_Community 80|Community 80]]
- [[_COMMUNITY_Community 81|Community 81]]

## God Nodes (most connected - your core abstractions)
1. `memset()` - 46 edges
2. `sys_reply()` - 42 edges
3. `memcpy()` - 36 edges
4. `kmain()` - 36 edges
5. `scheduler_get_current()` - 34 edges
6. `kfree()` - 23 edges
7. `kcap_install_root()` - 20 edges
8. `kobj_put()` - 20 edges
9. `sys_call()` - 19 edges
10. `cap_resolve()` - 19 edges

## Surprising Connections (you probably didn't know these)
- `fh_seek()` --calls--> `sys_call()`  [INFERRED]
  user/elfloader/main.c → src/sys/ipc.c
- `fh_read_exact()` --calls--> `sys_read()`  [INFERRED]
  user/elfloader/main.c → src/sys/read.c
- `handle_exec()` --calls--> `sys_call()`  [INFERRED]
  user/elfloader/main.c → src/sys/ipc.c
- `handle_exec_fh()` --calls--> `sys_call()`  [INFERRED]
  user/elfloader/main.c → src/sys/ipc.c
- `_start()` --calls--> `sys_recv()`  [INFERRED]
  user/elfloader/main.c → src/sys/ipc.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **User-Space Execution Pipeline** — spec_bm_protocol, spec_elfloader_elf32, spec_proc_spawn [EXTRACTED 0.95]
- **Boot Service Initialization Sequence** — spec_initd, spec_ttyd, spec_procd, spec_elfloader [EXTRACTED 0.95]
- **Kernel Core Concepts** — spec_kobj, spec_capability_system, spec_synchronous_ipc, spec_protocol_namespace [EXTRACTED 0.95]
- **User-Space Executable Programs** — initd_cmakelists_initd_target, ttyd_cmakelists_ttyd_target, procd_cmakelists_procd_target, elfloader_cmakelists_elfloader_target, shell_cmakelists_shell_target [EXTRACTED 0.95]
- **Kernel Source Groups** — kernel_cmakelists_kernel_sources, mm_cmakelists_mm_sources, process_cmakelists_process_sources, sys_cmakelists_sys_sources [EXTRACTED 0.95]

## Communities (82 total, 3 thin omitted)

### Community 0 - "Boot & TTY Drivers"
Cohesion: 0.05
Nodes (79): multiboot_find_module(), multiboot_get_info(), multiboot_map_bootinfo(), multiboot_set_info(), multiboot_unmap_bootinfo(), inb(), outb(), keyboard_handle_scancode() (+71 more)

### Community 1 - "Kernel Bootstrap & IPC"
Cohesion: 0.06
Nodes (65): spinlock_t, ipc_call_t, bm_vmobj_release(), bootstrap_endpoint_create(), endpoint_release(), process_install_boot_manifest_cap(), process_install_bootstrap_log_handler(), cap_table_destroy() (+57 more)

### Community 2 - "Capability Resolution & Strings"
Cohesion: 0.43
Nodes (5): mempcpy(), stpcpy(), strcat(), strcpy(), strlen()

### Community 3 - "Hardware I/O & IRQ"
Cohesion: 0.20
Nodes (22): crit_enter(), crit_exit(), vmm_alloc(), vmm_alloc_region(), vmm_any(), vmm_decode(), vmm_exactly_one(), vmm_free() (+14 more)

### Community 4 - "Capability Table Management"
Cohesion: 0.08
Nodes (59): cap_entry_t, cap_rights_t, cap_sys_arg_t, cap_table_t, cap_alloc_slot(), cap_free_slot(), cap_make_handle(), cap_next_gen() (+51 more)

### Community 5 - "ELF Loader & initd Services"
Cohesion: 0.08
Nodes (84): fh_read_exact(), fh_seek(), handle_exec(), handle_exec_fh(), handle_open(), pg_ceil(), pg_floor(), _start() (+76 more)

### Community 6 - "Process Namespace & Context"
Cohesion: 0.09
Nodes (36): arch_context_t, contains_colon(), process_namespace_bind(), process_namespace_destroy(), process_namespace_inherit(), process_namespace_init(), process_namespace_lookup(), user_enter() (+28 more)

### Community 7 - "Address Space & ELF Parsing"
Cohesion: 0.09
Nodes (36): Elf32_Ehdr, elf_load_result_t, as_free(), as_map(), as_memcpy(), as_put(), as_zero(), round_page() (+28 more)

### Community 8 - "User-Space Syscall Library"
Cohesion: 0.12
Nodes (36): _sc0(), _sc1(), _sc2(), _sc3(), _sc4(), sys_bootstrap_cap(), sys_call(), sys_cap_close() (+28 more)

### Community 9 - "TTY Framebuffer & Keyboard"
Cohesion: 0.13
Nodes (27): fb_attr(), fb_blit(), fb_clear(), fb_init(), fb_set_cursor(), kb_init(), kb_translate(), dispatch_vterm() (+19 more)

### Community 10 - "Memory Manager & Scheduler"
Cohesion: 0.08
Nodes (39): arch_registers_t, as_t, irq_flags_t, irq_exit_tail(), irq_rm_flag(), irq_set_flag(), as_get(), as_load_ptable() (+31 more)

### Community 11 - "Project Docs & Roadmap"
Cohesion: 0.11
Nodes (17): Arachnyaa Resource Model Spec, `bm:` (served by `initd`), Boot Path, Capability and handle model, `elfloader:` (served by `elfloader`), Frozen Rules, IPC model, `IPC_OP_EXEC` (convention, not a kernel syscall) (+9 more)

### Community 12 - "Virtual Memory Manager"
Cohesion: 0.60
Nodes (5): handle_open(), handle_spawn_call(), reply_empty(), _start(), sys_ipc_msg_t

### Community 13 - "Memory Free Tracker (AVL)"
Cohesion: 0.26
Nodes (22): align_up(), avl_delete_by_start(), avl_insert_exact(), balance_factor(), find_fit(), find_predecessor(), find_successor(), height() (+14 more)

### Community 14 - "Interactive Shell"
Cohesion: 0.13
Nodes (21): dbgprint(), _start(), process_t, _start(), tokenize(), tty_puts(), tty_readline(), tty_write() (+13 more)

### Community 15 - "User Program CMake Targets"
Cohesion: 0.47
Nodes (6): log-client Executable Target, procd Executable Target, shell Executable Target, ttyd Executable Target, ulib Static Library Target, Freestanding Build Pattern (no libc)

### Community 16 - "Syscall Dispatcher"
Cohesion: 0.31
Nodes (7): syscall_cap(), syscall_dispatcher(), native_word, cap_handle_t, arch_sys_exit(), arch_sys_putc(), arch_sys_write()

### Community 17 - "User Printf Library"
Cohesion: 0.80
Nodes (4): emit_int(), emit_str(), emit_uint(), snprintf()

### Community 19 - "Kernel Source CMake Groups"
Cohesion: 0.50
Nodes (4): Kernel CMake Sources, Memory Manager CMake Sources, Process Layer CMake Sources, Syscall Handler CMake Sources

### Community 65 - "Misc: spec syscall abi"
Cohesion: 0.13
Nodes (14): Arachnyaa Roadmap, Completed, Core kernel substrate, Default namespace, Extended kernel primitives, Filesystem / persistent storage, Multiple virtual terminals, Namespace isolation on spawn (+6 more)

### Community 78 - "Community 78"
Cohesion: 0.33
Nodes (5): Arachnyaa Implementation Plan, Milestone 1: Core substrate ✓, Milestone 2: First vertical slice ✓, Milestone 3: Interactive shell ✓, Milestone 4: Pipes and filesystem

### Community 80 - "Community 80"
Cohesion: 0.40
Nodes (4): arachnyaa, Architecture, Build, Current State

## Knowledge Gaps
- **84 isolated node(s):** `cap_handle_t`, `ramfs_inode_t`, `vfs_mount_t`, `cap_handle_t`, `kobj_t` (+79 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **3 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `scheduler_get_current()` connect `Capability Table Management` to `Boot & TTY Drivers`, `Kernel Bootstrap & IPC`, `ELF Loader & initd Services`, `Process Namespace & Context`, `Address Space & ELF Parsing`, `Memory Manager & Scheduler`, `Interactive Shell`?**
  _High betweenness centrality (0.138) - this node is a cross-community bridge._
- **Why does `kmain()` connect `Boot & TTY Drivers` to `Kernel Bootstrap & IPC`, `Memory Manager & Scheduler`, `Process Namespace & Context`?**
  _High betweenness centrality (0.119) - this node is a cross-community bridge._
- **Why does `_start()` connect `ELF Loader & initd Services` to `User-Space Syscall Library`, `Capability Table Management`, `Interactive Shell`?**
  _High betweenness centrality (0.103) - this node is a cross-community bridge._
- **Are the 45 inferred relationships involving `memset()` (e.g. with `fh_seek()` and `handle_exec()`) actually correct?**
  _`memset()` has 45 INFERRED edges - model-reasoned connections that need verification._
- **Are the 38 inferred relationships involving `sys_reply()` (e.g. with `handle_exec()` and `handle_exec_fh()`) actually correct?**
  _`sys_reply()` has 38 INFERRED edges - model-reasoned connections that need verification._
- **Are the 35 inferred relationships involving `memcpy()` (e.g. with `fh_seek()` and `handle_exec()`) actually correct?**
  _`memcpy()` has 35 INFERRED edges - model-reasoned connections that need verification._
- **Are the 35 inferred relationships involving `kmain()` (e.g. with `multiboot_find_module()` and `multiboot_map_bootinfo()`) actually correct?**
  _`kmain()` has 35 INFERRED edges - model-reasoned connections that need verification._