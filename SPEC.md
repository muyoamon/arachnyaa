# Arachnyaa v1 Resource Model Spec

## Summary

Arachnyaa v1 keeps the kernel small and generic. The kernel owns process scheduling, address spaces, capability tables, synchronous IPC, and a per-process protocol namespace. User space resolves resources through names of the form `<protocol>:<opaque-path>`, but the kernel only splits on the first `:`. Everything after that separator is passed unchanged to the selected handler.

The first implementation slice is intentionally small:

- Per-process protocol namespace with snapshot inheritance on spawn.
- Endpoint capabilities for server control and remote handles for opened resources.
- Generic IPC via `sys_call`, `sys_recv`, and `sys_reply`.
- Wrapper syscalls such as `open`, `write`, and `cap_close`.
- A bootstrap `log:` protocol to validate end-to-end dispatch.

## Frozen Rules

### Namespace model

- Each process owns a namespace map from protocol name to handler endpoint.
- A child process inherits a snapshot of the parent namespace at spawn time.
- Mutations only affect the current process and children spawned later from it.
- The kernel never parses opaque paths past the first `:`.

### Capability and handle model

- Namespace bindings store referenced endpoint objects, not process-local handles.
- `open` resolves a protocol once and returns a remote handle bound to the producing endpoint and server object id.
- All later operations dispatch through the bound endpoint, not by re-resolving the caller namespace.
- Capabilities carry per-handle rights bits enforced by the kernel.

### Rights model

- Endpoint capabilities use endpoint-control rights such as `R_EP_BIND`, `R_EP_CALL`, and `R_EP_REPLY`.
- Remote resource handles use protocol operation rights such as `KOP_READ`, `KOP_WRITE`, `KOP_MAP`, `KOP_CALL`, and `KOP_CLOSE`.
- Namespace bindings declare the maximum object operation rights that `open` through that binding may return.

### IPC model

- `sys_call` is the generic synchronous message primitive.
- `sys_recv` blocks on an endpoint until a message is available.
- `sys_reply` completes the active received call for the current server thread.
- Wrapper syscalls such as `write` are convenience APIs over the same endpoint/remote-handle transport.

## v1 `OPEN` Contract

### Request

- `sys_open("<protocol>:<opaque-path>", flags)` resolves the protocol in the caller namespace.
- The kernel sends an IPC request to the bound endpoint with:
  - `opcode = IPC_OP_OPEN`
  - `flags = open flags`
  - `object_id = 0`
  - `data[0..num_bytes)` = opaque path bytes
  - `num_handles = 0`

### Success reply

- The server replies with:
  - `object_id = server-defined non-zero object id`
  - `num_handles = 0`
  - `num_bytes = sizeof(sys_open_reply_t)`
  - `data` containing:

```c
typedef struct {
  uint32_t allowed_ops;
} sys_open_reply_t;
```

- The kernel clamps returned rights as:

```c
final_ops = reply.allowed_ops & binding->declared_ops;
```

- If `final_ops == 0`, `object_id == 0`, `num_handles != 0`, or `num_bytes` does not match `sizeof(sys_open_reply_t)`, `open` fails.
- On success, the kernel creates a `KOBJ_REMOTE` handle bound to:
  - the producing endpoint
  - the returned `object_id`
  - the clamped `final_ops`

### Later operations

- Operations on the returned handle always dispatch to the producing endpoint.
- The kernel sets the outgoing IPC `object_id` to the stored remote object id.
- The caller namespace is not consulted again after `open`.

## v1 ABI

- `SYS_NS_BIND(protocol, handler_cap, declared_ops)`
- `SYS_OPEN(name, flags) -> cap_handle`
- `SYS_WRITE(handle, buf, len) -> bytes or negative error`
- `SYS_CAP_CLOSE(handle)`
- `SYS_CALL(handle, msg, reply)`
- `SYS_RECV(endpoint_handle, out_msg)`
- `SYS_REPLY(reply_msg)`
- `SYS_SPAWN(args, out_pid, out_cap)`
- `SYS_EXIT(status)`

The current bootstrap path installs one endpoint capability into PID 1 so that `initd` can publish `log:` from user space. That bootstrap convention is temporary and should later move to a more explicit bootstrap message or descriptor table.
