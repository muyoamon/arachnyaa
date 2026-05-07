# --- Collect Sources ---
set(KERNEL_CORE_SOURCES
    kmain.c
    tty.c
    time.c
    syscall.c
    irq.c
    cap.c 
    kobj.c 
    revnode.c
    namespace.c
    bootstrap.c
    ipc.c
)


target_sources(arachnyaa_kernel PRIVATE
  ${KERNEL_CORE_SOURCES}
)
