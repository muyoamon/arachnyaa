set(KERNEL_MM_SOURCES
  kheap.c 
  kstack.c 
  pmm.c 
  vmm.c
  tracker.c
)

target_sources(arachnyaa_kernel PRIVATE
  ${KERNEL_MM_SOURCES}
)
