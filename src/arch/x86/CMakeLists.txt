# --- Find Linker Script ---
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)
if(NOT EXISTS ${LINKER_SCRIPT})
    message(FATAL_ERROR "Linker script not found: ${LINKER_SCRIPT}")
endif()
message("Linker script found!: ${LINKER_SCRIPT}")

target_link_options(arachnyaa_kernel PRIVATE -T ${LINKER_SCRIPT})

target_link_options(arachnyaa_kernel PRIVATE -Wl,-Map=${CMAKE_BINARY_DIR}/arachnyaa.map)

find_program(NASM_EXECUTABLE nasm)
if(NOT NASM_EXECUTABLE)
    message(FATAL_ERROR "NASM not found! please install NASM.")
endif()

# --- NASM_SOURCES ---
set(NASM_SOURCES
  boot.s 
  gdt.s 
  idt.s
  tss_asm.s
)

set(ARCH_OBJECTS "")

foreach(NASM_SOURCE ${NASM_SOURCES})
  get_filename_component(OBJECT_BASENAME ${NASM_SOURCE} NAME_WE)
  set(OBJECT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${OBJECT_BASENAME}.o)
  add_custom_command(
    OUTPUT ${OBJECT_FILE}
    COMMAND ${NASM_EXECUTABLE} -f elf32 -g -F dwarf ${CMAKE_CURRENT_SOURCE_DIR}/${NASM_SOURCE} -o ${OBJECT_FILE}
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${NASM_SOURCE}
    COMMENT "Assembling ${NASM_SOURCE} with NASM"
    VERBATIM
  )
  list(APPEND ARCH_OBJECTS ${OBJECT_FILE})
endforeach()

# --- Collect Sources ---
set(ARCH_C_SOURCES
    gdt.c
    idt.c
    interrupts.c
    hal.c
    paging.c
    tss.c
    syscall.c
    string.c
    init.c
)

target_sources(arachnyaa_kernel PRIVATE
  ${ARCH_C_SOURCES}
  ${ARCH_OBJECTS}
)

# --- Add sources to the global list ---
list(APPEND KERNEL_SOURCES ${ARCH_SOURCES})

# --- Add linker script to the global list ---
list(APPEND KERNEL_LINKER_SCRIPTS ${LINKER_SCRIPT})

set(KERNEL_SOURCES ${KERNEL_SOURCES} PARENT_SCOPE)

add_custom_target(
  arch_asm_objects ALL
  DEPENDS ${ARCH_OBJECTS}
)
