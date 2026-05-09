# ArachnyaaOS Pre-Shell Roadmap

## Purpose

This roadmap defines the minimum system requirements for ArachnyaaOS to reach a useful first shell. The goal is not a feature-complete OS, but a coherent user-space environment where a shell is not forced to compensate for missing kernel, IPC, or runtime fundamentals.

This roadmap assumes the current direction is kept:

- kernel manages scheduling, address spaces, capabilities, namespaces, and synchronous IPC
- PID 1 (`initd`) bootstraps the protocol namespace from user space
- resources are resolved with `<protocol>:<opaque-path>`
- endpoint capabilities are distinct from remote resource handles
- `sys_call` is the generic message primitive, while helpers such as `open`, `read`, and `write` are typed wrappers

## Current Baseline

The current codebase already has the first real vertical slice:

- PID 1 receives a bootstrap endpoint capability
- `initd` binds `log:`
- `initd` can spawn a second user program
- a client can `open("log:stdout")`
- the kernel routes `OPEN` and `WRITE` through synchronous IPC
- `initd` serves the request and prints output

This is enough to validate the object model and namespace model, but not enough to support a useful shell.

## Shell Readiness Criteria

A first shell becomes worthwhile only when all of these are true:

- it can launch child programs reliably
- it can wait for child exit and collect status
- it can read user input and write output through stable resource handles
- it can close handles cleanly
- it can resolve programs and core resources without hardcoded kernel special cases
- basic failure cases return predictable errors instead of crashing or hanging
- namespace inheritance and isolation behavior are explicit and predictable
- bare paths can resolve ergonomically through parent-configured defaults where intended

## Roadmap

### Phase 1: Stabilize the Core Runtime

These items should be finished before adding more user-space complexity.

- Harden syscall pointer validation where user memory is copied in or out.
- Finish the obvious IPC validation paths:
  - malformed message sizes
  - invalid handle counts
  - cancelled calls
  - missing active reply context
- Make process teardown and capability teardown reliable.
- Finish address-space cleanup enough that repeated spawn/exit cycles do not leak indefinitely.
- Improve fault reporting so bad user-space behavior is diagnosable.

### Phase 2: Complete Basic Process Lifecycle

A shell without process lifecycle support is mostly a demo launcher.

- Keep `spawn` as the primary creation primitive.
- Add `wait` or equivalent child-exit collection.
- Define orphan handling and PID 1 reparenting behavior.
- Return stable exit statuses.
- Decide whether the parent eventually receives a child process capability, or whether PID/status is the only initial contract.

Minimum requirement before shell:

- `initd` or another program can spawn a child, wait for it, and observe exit status.

### Phase 3: Complete Resource-Level I/O

The shell needs stable, ordinary input and output resources.

- Add `read` as the counterpart to `write`.
- Decide and implement resource-level `close` semantics.
  - `cap_close` should remain capability-table close.
  - a future `close` wrapper should perform protocol-level close behavior where appropriate.
- Freeze reply conventions for `READ` and `WRITE` the same way `OPEN` is already frozen.
- Ensure remote object operations always route using stored producer binding and server object id.

Minimum requirement before shell:

- one process can `open`, `read`, `write`, and close a console-like resource through user-space services.

### Phase 4: Build Interactive Console Input

Output alone is not enough. The shell needs a real input source.

- Expose keyboard input to user space as a service rather than only kernel-side behavior.
- Create a user-space console/input service, likely under `log:`, `tty:`, or `console:`.
- Support blocking reads from an input handle.
- Define simple line-oriented behavior first; full terminal emulation can come later.

Recommended first target:

- `console:stdin`
- `console:stdout`
- `console:stderr`

Minimum requirement before shell:

- a program can block waiting for keyboard input and receive bytes or lines.

### Phase 5: Improve Bootstrap Ergonomics

The system should become less ad hoc before the shell becomes the main interface.

- Replace the fixed bootstrap handle convention with a clearer bootstrap descriptor or initial message.
- Expose process and namespace services in user space where helpful.
- Add a small `proc:`-style service or library wrappers so programs do not need raw syscall knowledge for common lifecycle operations.
- Add namespace inspection/debug support to make service wiring easier to understand.

Minimum requirement before shell:

- core bootstrap and service discovery are understandable without reading kernel constants.

### Phase 6: Freeze Namespace Inheritance and Shadowing Semantics

Before a shell exists, namespace behavior must be explicit enough that program launch, isolation, and path resolution are understandable from user space.

- Keep implicit namespace inheritance on spawn as the default behavior.
  - A child receives a snapshot of the parent namespace unless the spawn request says otherwise.
- Add an explicit spawn mode for isolated children.
  - In this mode, the child starts with an empty or minimal namespace and only receives protocols the parent passes deliberately.
- Define selective namespace passing during spawn.
  - A parent should be able to provide a chosen subset of protocol bindings without copying the whole parent namespace.
- Freeze downward-only shadowing semantics.
  - If a child rebinds a protocol, that shadow affects only the child and its descendants.
  - Existing parent and sibling bindings remain unchanged.
- Add user-space-visible rules for namespace mutation and inspection so a shell or launcher can reason about what a child will see.

Minimum requirement before shell:

- user space can choose between inherited and isolated spawn behavior
- protocol shadowing is stable and documented
- namespace contents can be inspected for debugging and service composition

### Phase 7: Add Default No-Protocol Resolution

For shell and launcher ergonomics, the namespace should support a default resolver for names without an explicit protocol prefix.

- Add a distinguished empty-protocol binding, conceptually:
  - `sys_ns_bind("", handler, declared_ops)`
- Define lookup behavior for names without `:`.
  - If a resource name contains `:`, resolve normally.
  - If it does not, first consult the empty-protocol binding.
  - The selected handler then receives the full original path unchanged.
- Use this to support parent-configured defaults such as:
  - bare `foo/bar` resolving through a `file:`-like service
  - command names resolving through a launcher or executable lookup service
- Keep this as a namespace policy, not kernel pathname parsing.
  - The kernel should still only distinguish “has explicit protocol” vs “use default binding”.
  - All further interpretation stays in user space.

Minimum requirement before shell:

- a shell can open or execute bare names without hardcoding one protocol into the shell itself
- parent namespace configuration controls what “bare path” means for descendants

### Phase 8: Program Discovery and Loading

A shell is much more useful if it can run programs by name without relying on boot-module-only wiring forever.

- Keep boot-module spawning for early development.
- Add a non-hardcoded program-loading story after that:
  - simple ram-backed filesystem, or
  - a user-space program registry service, or
  - a basic `file:`/`exec:` loader path
- Define how the shell resolves command names to executable resources.

Minimum requirement before shell:

- the shell can launch at least a small set of programs by stable names, not just by build-time bootstrap assumptions.

## Suggested Implementation Order

If the goal is to reach a first shell quickly without creating throwaway architecture, the recommended order is:

1. `wait` and exit status collection
2. `read`
3. resource-level `close`
4. console input service
5. freeze namespace inheritance / isolated spawn semantics
6. add default no-protocol resolution
7. bootstrap descriptor cleanup
8. simple program discovery/loading
9. first shell

## What the First Shell Does Not Need

These are useful later, but should not block the first shell:

- `fork`
- pipes
- full filesystem semantics
- path expansion and globbing
- terminal job control
- asynchronous IPC
- networking
- multi-user security model

## First Shell Scope

A reasonable first shell for ArachnyaaOS should only need to do this:

- print a prompt
- read a line from console input
- resolve bare resource/program names through namespace policy rather than hardcoded protocol assumptions
- spawn a named program
- wait for it to exit
- print its exit status or simple errors

That is enough to validate the OS design without prematurely building a Unix clone.

## Exit Condition

ArachnyaaOS is ready for the first shell when the following path works reliably:

1. `initd` starts and publishes the core namespace.
2. A console service provides input and output handles.
3. A user program reads a command line from the console.
4. It resolves a program by name.
5. It spawns the program.
6. It waits for the child to exit.
7. It returns to the prompt without leaking state or requiring reboot.
