set(KERNEL_SYS_SOURCES
  write.c
)

message("kernel sys sources")

target_sources(arachnyaa_kernel PRIVATE
  ${KERNEL_SYS_SOURCES}
)
