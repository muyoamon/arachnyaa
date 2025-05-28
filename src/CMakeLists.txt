set(KERNEL_SOURCES "")
set(KERNEL_LINKER_SCRIPTS "")


# --- Define the Kernel Executable HERE ---
add_executable(arachnyaa_kernel "")
target_sources(arachnyaa_kernel PRIVATE ${KERNEL_SOURCES})
target_include_directories(arachnyaa_kernel PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/include
)

set_target_properties(arachnyaa_kernel PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

target_compile_options(arachnyaa_kernel PRIVATE
  -ffreestanding
  -nostdlib
  -Wall -Wextra -Werror
  -g
)

target_link_options(arachnyaa_kernel PRIVATE
  -nostdlib
  -lgcc
)

add_subdirectory(drivers)
add_subdirectory(arch)
add_subdirectory(kernel)

add_dependencies(arachnyaa_kernel arch_asm_objects)

target_link_libraries(arachnyaa_kernel PRIVATE "")
