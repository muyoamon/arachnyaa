# --- Collect Sources ---
set(KERNEL_BOOT_SOURCES
    multiboot.c
)


target_sources(arachnyaa_kernel PRIVATE
  ${KERNEL_BOOT_SOURCES}
)


