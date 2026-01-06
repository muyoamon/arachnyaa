set(KERNEL_PROCESS_SOURCES
  elf_loader.c
  process.c
  thread.c
  rq.c 
  scheduler.c
)

message("kernel process sources")

target_sources(arachnyaa_kernel PRIVATE
  ${KERNEL_PROCESS_SOURCES}
)
