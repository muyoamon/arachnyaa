set(KERNEL_SYS_SOURCES
  namespace.c
  write.c
  ipc.c
  pipe.c
  proc.c
  read.c
  ep.c
  irq.c
  device.c
  vspace.c
)

target_sources(arachnyaa_kernel PRIVATE
  ${KERNEL_SYS_SOURCES}
)
