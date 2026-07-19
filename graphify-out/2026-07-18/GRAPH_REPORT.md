# Graph Report - arachnyaa  (2026-07-18)

## Corpus Check
- 136 files · ~45,328 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 764 nodes · 1864 edges · 92 communities (83 shown, 9 thin omitted)
- Extraction: 64% EXTRACTED · 36% INFERRED · 0% AMBIGUOUS · INFERRED: 679 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `ea95584e`
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
- `syscall_dispatcher()` --calls--> `sys_reply()`  [INFERRED]
  src/kernel/syscall.c → user/ulib/syscall.h
- `sys_pipe()` --calls--> `sys_cap_close()`  [INFERRED]
  src/sys/pipe.c → user/ulib/syscall.h
- `fh_read_exact()` --calls--> `sys_read()`  [INFERRED]
  user/elfloader/main.c → src/sys/read.c
- `sys_read()` --calls--> `memcpy()`  [INFERRED]
  src/sys/read.c → user/ulib/string.c
- `handle_read()` --calls--> `sys_read()`  [INFERRED]
  user/vfs/main.c → src/sys/read.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **User-Space Execution Pipeline** — spec_bm_protocol, spec_elfloader_elf32, spec_proc_spawn [EXTRACTED 0.95]
- **Boot Service Initialization Sequence** — spec_initd, spec_ttyd, spec_procd, spec_elfloader [EXTRACTED 0.95]
- **Kernel Core Concepts** — spec_kobj, spec_capability_system, spec_synchronous_ipc, spec_protocol_namespace [EXTRACTED 0.95]
- **User-Space Executable Programs** — initd_cmakelists_initd_target, ttyd_cmakelists_ttyd_target, procd_cmakelists_procd_target, elfloader_cmakelists_elfloader_target, shell_cmakelists_shell_target [EXTRACTED 0.95]
- **Kernel Source Groups** — kernel_cmakelists_kernel_sources, mm_cmakelists_mm_sources, process_cmakelists_process_sources, sys_cmakelists_sys_sources [EXTRACTED 0.95]

## Communities (92 total, 9 thin omitted)

### Community 0 - "Boot & TTY Drivers"
Cohesion: 0.07
Nodes (63): multiboot_find_module(), multiboot_get_info(), multiboot_map_bootinfo(), multiboot_set_info(), multiboot_unmap_bootinfo(), vga_color(), kmain(), kprint() (+55 more)

### Community 1 - "Kernel Bootstrap & IPC"
Cohesion: 0.15
Nodes (22): bm_vmobj_release(), bootstrap_endpoint_create(), endpoint_release(), process_install_boot_manifest_cap(), process_install_bootstrap_log_handler(), revnode_root_new(), ipc_endpoint_init(), _release_remote() (+14 more)

### Community 2 - "Capability Resolution & Strings"
Cohesion: 0.10
Nodes (38): spinlock_t, ipc_call_t, ipc_call(), ipc_call_create(), ipc_call_destroy(), ipc_call_enqueue(), ipc_endpoint_destroy(), ipc_recv_next() (+30 more)

### Community 3 - "Hardware I/O & IRQ"
Cohesion: 0.20
Nodes (22): crit_enter(), crit_exit(), vmm_alloc(), vmm_alloc_region(), vmm_any(), vmm_decode(), vmm_exactly_one(), vmm_free() (+14 more)

### Community 4 - "Capability Table Management"
Cohesion: 0.24
Nodes (21): cap_sys_arg_t, cap_alloc_slot(), cap_make_handle(), cap_next_gen(), cap_validate(), cap_validate_mut(), kcap_derive(), kcap_revoke() (+13 more)

### Community 5 - "ELF Loader & initd Services"
Cohesion: 0.09
Nodes (75): handle_open(), bind_bm_protocol(), bind_log_protocol(), bm_find_module(), dbgwrite(), handle_bm_close(), handle_bm_exec(), handle_bm_open() (+67 more)

### Community 6 - "Process Namespace & Context"
Cohesion: 0.08
Nodes (43): arch_context_t, kobj_init(), contains_colon(), process_namespace_bind(), process_namespace_destroy(), process_namespace_inherit(), process_namespace_init(), process_namespace_lookup() (+35 more)

### Community 7 - "Address Space & ELF Parsing"
Cohesion: 0.14
Nodes (24): Elf32_Ehdr, elf_load_result_t, as_create(), as_free(), as_map(), as_memcpy(), as_put(), as_zero() (+16 more)

### Community 8 - "User-Space Syscall Library"
Cohesion: 0.06
Nodes (75): main(), fh_read_exact(), fh_seek(), handle_exec(), handle_exec_fh(), pab_write(), pg_ceil(), pg_floor() (+67 more)

### Community 9 - "TTY Framebuffer & Keyboard"
Cohesion: 0.13
Nodes (27): fb_attr(), fb_blit(), fb_clear(), fb_init(), fb_set_cursor(), kb_init(), kb_translate(), dispatch_vterm() (+19 more)

### Community 10 - "Memory Manager & Scheduler"
Cohesion: 0.09
Nodes (35): arch_registers_t, as_t, irq_exit_tail(), as_get(), as_load_ptable(), timer_set_slice(), timer_sleep_ticks(), rq_init() (+27 more)

### Community 11 - "Project Docs & Roadmap"
Cohesion: 0.11
Nodes (17): Arachnyaa Resource Model Spec, `bm:` (served by `initd`), Boot Path, Capability and handle model, `elfloader:` (served by `elfloader`), Frozen Rules, IPC model, `IPC_OP_EXEC` (convention, not a kernel syscall) (+9 more)

### Community 12 - "Virtual Memory Manager"
Cohesion: 0.16
Nodes (17): pipe_buf_put(), pipe_close_read(), pipe_close_write(), pipe_create(), pipe_read(), _pipe_read_release(), pipe_write(), _pipe_write_release() (+9 more)

### Community 13 - "Memory Free Tracker (AVL)"
Cohesion: 0.26
Nodes (22): align_up(), avl_delete_by_start(), avl_insert_exact(), balance_factor(), find_fit(), find_predecessor(), find_successor(), height() (+14 more)

### Community 14 - "Interactive Shell"
Cohesion: 0.20
Nodes (15): cap_entry_t, cap_resolve(), cap_handle_t, cap_handle_t, pid_t, process_t, sys_proc_arg_t, sys_irq_claim() (+7 more)

### Community 16 - "Syscall Dispatcher"
Cohesion: 0.19
Nodes (15): cap_rights_t, cap_table_t, cap_free_slot(), cap_table_destroy(), cap_table_init(), kcap_install_root(), revnode_create(), _revnode_get_next_child() (+7 more)

### Community 17 - "User Printf Library"
Cohesion: 0.80
Nodes (4): emit_int(), emit_str(), emit_uint(), snprintf()

### Community 34 - "Misc: include kernel kobj h kernel k"
Cohesion: 0.23
Nodes (15): scheduler_get_current(), as_t, cap_handle_t, kobj_t, process_t, sys_vspace_map_args_t, aspace_release(), install_aspace_cap() (+7 more)

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
Cohesion: 0.09
Nodes (30): inb(), outb(), keyboard_handle_scancode(), keyboard_init(), irq_flags_t, irq_rm_flag(), irq_set_flag(), timer_isr_handler() (+22 more)

### Community 83 - "Community 83"
Cohesion: 0.39
Nodes (7): ipc_install_remote_handle(), kobj_arr_get_next_free(), kobj_create(), kobj_get(), cap_handle_t, kobj_t, kobj_t

## Knowledge Gaps
- **88 isolated node(s):** `cap_handle_t`, `sys_pipe_result_t`, `kobj_t`, `process_t`, `kobj_t` (+83 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **9 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `scheduler_get_current()` connect `Misc: include kernel kobj h kernel k` to `Boot & TTY Drivers`, `Kernel Bootstrap & IPC`, `Capability Resolution & Strings`, `Capability Table Management`, `ELF Loader & initd Services`, `Process Namespace & Context`, `Memory Manager & Scheduler`, `Virtual Memory Manager`, `Interactive Shell`?**
  _High betweenness centrality (0.124) - this node is a cross-community bridge._
- **Why does `kmain()` connect `Boot & TTY Drivers` to `Kernel Bootstrap & IPC`, `Community 82`, `Memory Manager & Scheduler`, `Process Namespace & Context`?**
  _High betweenness centrality (0.108) - this node is a cross-community bridge._
- **Why does `syscall_dispatcher()` connect `User-Space Syscall Library` to `Boot & TTY Drivers`, `Community 82`, `ELF Loader & initd Services`, `Interactive Shell`?**
  _High betweenness centrality (0.076) - this node is a cross-community bridge._
- **Are the 45 inferred relationships involving `memset()` (e.g. with `fh_seek()` and `handle_exec()`) actually correct?**
  _`memset()` has 45 INFERRED edges - model-reasoned connections that need verification._
- **Are the 38 inferred relationships involving `sys_reply()` (e.g. with `handle_exec()` and `handle_exec_fh()`) actually correct?**
  _`sys_reply()` has 38 INFERRED edges - model-reasoned connections that need verification._
- **Are the 35 inferred relationships involving `memcpy()` (e.g. with `fh_seek()` and `handle_exec()`) actually correct?**
  _`memcpy()` has 35 INFERRED edges - model-reasoned connections that need verification._
- **Are the 35 inferred relationships involving `kmain()` (e.g. with `multiboot_find_module()` and `multiboot_map_bootinfo()`) actually correct?**
  _`kmain()` has 35 INFERRED edges - model-reasoned connections that need verification._