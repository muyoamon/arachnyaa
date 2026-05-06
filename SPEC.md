# Arachnyaa v1 Resource Model Spec

## Summary

Arachnyaa v1 keeps the kernel small and generic. The kernel owns process scheduling, address spaces, capability tables, and a per-process protocol namespace. User space resolves resources through names of the form `<protocol>:<opaque-path>`, but the kernel only splits on the first `:`. Everything after that separator is passed unchanged to the selected handler.

The first implementation slice is intentionally small:

- Per-process protocol namespace with snapshot inheritance on spawn.
- Capability-bound handler objects and produced resource handles.
- `ns_bind`, `open`, `write`, and `cap_close` syscalls.
- A bootstrap `log:` protocol to validate end-to-end dispatch.

## Frozen Rules

### Namespace model

- Each process owns a namespace map from protocol name to handler object.
- A child process inherits a snapshot of the parent namespace at spawn time.
- Mutations only affect the current process and children spawned later from it.
- The kernel never parses opaque paths past the first `:`.

### Capability and handle model

- Namespace bindings store referenced handler objects, not process-local handles.
- `open` resolves a protocol once and returns a handle bound to the producing object.
- All later operations dispatch through the bound object, not by re-resolving the caller namespace.
- Capabilities carry per-handle rights bits enforced by the kernel.

### Handler contract

- Kernel-known operation bits are `OPEN`, `CALL`, `READ`, `WRITE`, `MAP`, and `CLOSE`.
- A protocol binding declares which of those operations are exposed through that binding.
- A handler object must implement `open` to serve namespace resolution.
- Returned objects may implement operations like `write` directly.

## v1 ABI

- `SYS_NS_BIND(protocol, handler_cap, declared_ops)`
- `SYS_OPEN(name, flags) -> cap_handle`
- `SYS_WRITE(handle, buf, len) -> bytes or negative error`
- `SYS_CAP_CLOSE(handle)`
- `SYS_EXIT(status)`

The current bootstrap path installs one endpoint capability into PID 1 so that `initd` can publish `log:` from user space. That bootstrap convention is temporary and should later move to a more explicit bootstrap message or descriptor table.
