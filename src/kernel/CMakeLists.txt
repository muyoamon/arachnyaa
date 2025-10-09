

# --- Collect Sources ---
set(KERNEL_CORE_SOURCES
    kmain.c
    tty.c
    time.c
    syscall.c
)


target_sources(arachnyaa_kernel PRIVATE
  ${KERNEL_CORE_SOURCES}
)


add_subdirectory(process)
add_subdirectory(mm)
