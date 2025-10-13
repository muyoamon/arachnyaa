set(KERNEL_PROCESS_SOURCES
  process.c
  thread.c
)

message("kernel process sources")

target_sources(arachnyaa_kernel PRIVATE
  ${KERNEL_PROCESS_SOURCES}
)
