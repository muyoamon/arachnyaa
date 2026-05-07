set(KERNEL_SYS_SOURCES
  namespace.c
  write.c
  ipc.c
  proc.c
)

target_sources(arachnyaa_kernel PRIVATE
  ${KERNEL_SYS_SOURCES}
)
