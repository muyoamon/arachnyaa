set(KERNEL_SYS_SOURCES
  namespace.c
  write.c
)

target_sources(arachnyaa_kernel PRIVATE
  ${KERNEL_SYS_SOURCES}
)
