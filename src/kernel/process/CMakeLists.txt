set(KERNEL_PROCESS_SOURCES
  task.c
)

target_sources(arachnyaa_kernel PRIVATE
  ${KERNEL_PROCESS_SOURCES}
)
