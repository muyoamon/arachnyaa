# Graph Report - .  (2026-06-10)

## Corpus Check
- Corpus is ~36,504 words - fits in a single context window. You may not need a graph.

## Summary
- 679 nodes · 1484 edges · 78 communities (77 shown, 1 thin omitted)
- Extraction: 70% EXTRACTED · 30% INFERRED · 0% AMBIGUOUS · INFERRED: 444 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

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

## God Nodes (most connected - your core abstractions)
1. `kmain()` - 36 edges
2. `scheduler_get_current()` - 34 edges
3. `kfree()` - 23 edges
4. `kcap_install_root()` - 20 edges
5. `kobj_put()` - 20 edges
6. `cap_resolve()` - 19 edges
7. `cap_handle_t` - 19 edges
8. `scheduler_reschedule()` - 17 edges
9. `outb()` - 15 edges
10. `cap_handle_t` - 15 edges

## Surprising Connections (you probably didn't know these)
- `Filesystem Service (fs:)` --semantically_similar_to--> `bm: Boot Module Protocol`  [INFERRED] [semantically similar]
  ROADMAP.md → SPEC.md
- `serial_init()` --calls--> `outb()`  [INFERRED]
  src/kernel/tty.c → include/drivers/io.h
- `tty_update_cursor()` --calls--> `outb()`  [INFERRED]
  src/kernel/tty.c → include/drivers/io.h
- `sys_io_out()` --calls--> `outb()`  [INFERRED]
  src/sys/device.c → include/drivers/io.h
- `serial_init_once()` --calls--> `outb()`  [INFERRED]
  src/arch/x86/paging.c → include/drivers/io.h

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **User-Space Execution Pipeline** — spec_bm_protocol, spec_elfloader_elf32, spec_proc_spawn [EXTRACTED 0.95]
- **Boot Service Initialization Sequence** — spec_initd, spec_ttyd, spec_procd, spec_elfloader [EXTRACTED 0.95]
- **Kernel Core Concepts** — spec_kobj, spec_capability_system, spec_synchronous_ipc, spec_protocol_namespace [EXTRACTED 0.95]
- **User-Space Executable Programs** — initd_cmakelists_initd_target, ttyd_cmakelists_ttyd_target, procd_cmakelists_procd_target, elfloader_cmakelists_elfloader_target, shell_cmakelists_shell_target [EXTRACTED 0.95]
- **Kernel Source Groups** — kernel_cmakelists_kernel_sources, mm_cmakelists_mm_sources, process_cmakelists_process_sources, sys_cmakelists_sys_sources [EXTRACTED 0.95]

## Communities (78 total, 1 thin omitted)

### Community 0 - "Boot & TTY Drivers"
Cohesion: 0.07
Nodes (64): multiboot_find_module(), multiboot_get_info(), multiboot_map_bootinfo(), multiboot_set_info(), multiboot_unmap_bootinfo(), vga_color(), kmain(), kprint() (+56 more)

### Community 1 - "Kernel Bootstrap & IPC"
Cohesion: 0.07
Nodes (56): spinlock_t, ipc_call_t, bm_vmobj_release(), bootstrap_endpoint_create(), endpoint_release(), process_install_boot_manifest_cap(), process_install_bootstrap_log_handler(), ipc_call() (+48 more)

### Community 2 - "Capability Resolution & Strings"
Cohesion: 0.09
Nodes (41): cap_entry_t, cap_resolve(), scheduler_get_current(), mempcpy(), stpcpy(), strcat(), strcpy(), strlen() (+33 more)

### Community 3 - "Hardware I/O & IRQ"
Cohesion: 0.08
Nodes (32): arch_registers_t, inb(), outb(), keyboard_handle_scancode(), keyboard_init(), irq_flags_t, irq_exit_tail(), irq_rm_flag() (+24 more)

### Community 4 - "Capability Table Management"
Cohesion: 0.12
Nodes (39): cap_rights_t, cap_sys_arg_t, cap_table_t, cap_alloc_slot(), cap_free_slot(), cap_make_handle(), cap_next_gen(), cap_table_destroy() (+31 more)

### Community 5 - "ELF Loader & initd Services"
Cohesion: 0.10
Nodes (38): handle_exec(), handle_open(), pg_ceil(), pg_floor(), _start(), bind_bm_protocol(), bind_log_protocol(), bm_find_module() (+30 more)

### Community 6 - "Process Namespace & Context"
Cohesion: 0.09
Nodes (35): arch_context_t, contains_colon(), process_namespace_bind(), process_namespace_destroy(), process_namespace_inherit(), process_namespace_init(), process_namespace_lookup(), user_enter() (+27 more)

### Community 7 - "Address Space & ELF Parsing"
Cohesion: 0.09
Nodes (37): Elf32_Ehdr, elf_load_result_t, as_create(), as_free(), as_map(), as_memcpy(), as_put(), as_zero() (+29 more)

### Community 8 - "User-Space Syscall Library"
Cohesion: 0.12
Nodes (36): _sc0(), _sc1(), _sc2(), _sc3(), _sc4(), sys_bootstrap_cap(), sys_call(), sys_cap_close() (+28 more)

### Community 9 - "TTY Framebuffer & Keyboard"
Cohesion: 0.13
Nodes (27): fb_attr(), fb_blit(), fb_clear(), fb_init(), fb_set_cursor(), kb_init(), kb_translate(), dispatch_vterm() (+19 more)

### Community 10 - "Memory Manager & Scheduler"
Cohesion: 0.14
Nodes (27): as_t, as_get(), as_load_ptable(), rq_init(), rq_peek(), rq_pop(), rq_push(), rq_remove() (+19 more)

### Community 11 - "Project Docs & Roadmap"
Cohesion: 0.10
Nodes (31): Milestone 1: Core Substrate, Milestone 2: First Vertical Slice, Milestone 3: Interactive Shell, Milestone 4: Pipes and Filesystem, Arachnyaa OS, Microkernel Design, Filesystem Service (fs:), Namespace Isolation on Spawn (+23 more)

### Community 12 - "Virtual Memory Manager"
Cohesion: 0.17
Nodes (24): crit_enter(), crit_exit(), vmm_alloc(), vmm_alloc_region(), vmm_any(), vmm_decode(), vmm_exactly_one(), vmm_free() (+16 more)

### Community 13 - "Memory Free Tracker (AVL)"
Cohesion: 0.26
Nodes (22): align_up(), avl_delete_by_start(), avl_insert_exact(), balance_factor(), find_fit(), find_predecessor(), find_successor(), height() (+14 more)

### Community 14 - "Interactive Shell"
Cohesion: 0.18
Nodes (9): _start(), tokenize(), tty_puts(), tty_readline(), tty_write(), var_set(), var_unset(), memmove() (+1 more)

### Community 15 - "User Program CMake Targets"
Cohesion: 0.50
Nodes (9): elfloader Executable Target, initd Executable Target, log-client Executable Target, procd Executable Target, shell Executable Target, ttyd Executable Target, ulib Static Library Target, Freestanding Build Pattern (no libc) (+1 more)

### Community 16 - "Syscall Dispatcher"
Cohesion: 0.31
Nodes (7): syscall_cap(), syscall_dispatcher(), native_word, cap_handle_t, arch_sys_exit(), arch_sys_putc(), arch_sys_write()

### Community 17 - "User Printf Library"
Cohesion: 0.90
Nodes (4): emit_int(), emit_str(), emit_uint(), snprintf()

### Community 18 - "Kernel CMake Build"
Cohesion: 0.50
Nodes (4): x86 Architecture CMake Target, arachnyaa_kernel CMake Target, Drivers CMake Target, Freestanding String Library Target

### Community 19 - "Kernel Source CMake Groups"
Cohesion: 0.50
Nodes (4): Kernel CMake Sources, Memory Manager CMake Sources, Process Layer CMake Sources, Syscall Handler CMake Sources

## Knowledge Gaps
- **56 isolated node(s):** `multiboot_module_t`, `multiboot_info_t`, `kobj_type_t`, `revnode_t`, `kobj_t` (+51 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **1 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `kmain()` connect `Boot & TTY Drivers` to `Kernel Bootstrap & IPC`, `Hardware I/O & IRQ`, `Process Namespace & Context`, `Memory Manager & Scheduler`, `Virtual Memory Manager`?**
  _High betweenness centrality (0.105) - this node is a cross-community bridge._
- **Why does `scheduler_get_current()` connect `Capability Resolution & Strings` to `Boot & TTY Drivers`, `Kernel Bootstrap & IPC`, `Capability Table Management`, `Process Namespace & Context`, `Address Space & ELF Parsing`, `Memory Manager & Scheduler`?**
  _High betweenness centrality (0.082) - this node is a cross-community bridge._
- **Why does `kfree()` connect `Kernel Bootstrap & IPC` to `Capability Table Management`, `Memory Free Tracker (AVL)`, `Process Namespace & Context`, `Address Space & ELF Parsing`?**
  _High betweenness centrality (0.048) - this node is a cross-community bridge._
- **Are the 35 inferred relationships involving `kmain()` (e.g. with `multiboot_find_module()` and `multiboot_map_bootinfo()`) actually correct?**
  _`kmain()` has 35 INFERRED edges - model-reasoned connections that need verification._
- **Are the 32 inferred relationships involving `scheduler_get_current()` (e.g. with `sys_bootstrap_cap()` and `sys_cap_close()`) actually correct?**
  _`scheduler_get_current()` has 32 INFERRED edges - model-reasoned connections that need verification._
- **Are the 21 inferred relationships involving `kfree()` (e.g. with `bm_vmobj_release()` and `bootstrap_endpoint_create()`) actually correct?**
  _`kfree()` has 21 INFERRED edges - model-reasoned connections that need verification._
- **Are the 11 inferred relationships involving `kcap_install_root()` (e.g. with `process_install_boot_manifest_cap()` and `process_install_bootstrap_log_handler()`) actually correct?**
  _`kcap_install_root()` has 11 INFERRED edges - model-reasoned connections that need verification._