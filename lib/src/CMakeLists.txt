set(KERNEL_DRIVER_SOURCES

  string.c

)

target_sources(arachnyaa_kernel PRIVATE ${KERNEL_DRIVER_SOURCES})

list(APPEND KERNEL_SOURCES

  ${KERNEL_DRIVER_SOURCES}
)
