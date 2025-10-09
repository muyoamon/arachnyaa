set(KERNEL_PROCESS_SOURCES
  task.c
  thread.c
)

message("kernel process sources")

target_sources(arachnyaa_kernel PRIVATE
  ${KERNEL_PROCESS_SOURCES}
)
