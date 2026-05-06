# Arachnyaa Implementation Plan

## Milestone 1: Core substrate

- Finish kernel object and revocation lifecycles.
- Initialize process capability tables and namespaces during process creation.
- Add a kernel-managed namespace map with snapshot inheritance on spawn.
- Introduce generic handler operations for `open` and `write`.

## Milestone 2: First vertical slice

- Add `ns_bind`, `open`, `write`, and `cap_close` syscall handling.
- Install one bootstrap endpoint capability into PID 1.
- Let `initd` bind `log:` and open `log:stdout`.
- Route writes through the produced handle instead of a hardcoded write path.

## Milestone 3: Expansion path

- Replace the bootstrap-cap constant with an explicit bootstrap descriptor.
- Add synchronous endpoint call/reply IPC instead of kernel-direct handler invocation.
- Add explicit handle transfer across processes.
- Expand protocol-backed services from `log:` to files, devices, and networking.
