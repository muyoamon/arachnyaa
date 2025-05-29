# --- Collect Sources ---
set(KERNEL_CORE_SOURCES
    kmain.c
    tty.c
    pmm.c 
    time.c
)

target_sources(arachnyaa_kernel PRIVATE
  ${KERNEL_CORE_SOURCES}
)
