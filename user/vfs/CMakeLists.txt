add_executable(arachnyaa_vfs main.c)

set_target_properties(arachnyaa_vfs PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

target_compile_options(arachnyaa_vfs PRIVATE
  -ffreestanding
  -nostdlib
  -Wall -Wextra -Werror
  -nostartfiles
  -fno-pic
  -m32
  -g
)

target_include_directories(arachnyaa_vfs PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(arachnyaa_vfs PRIVATE ulib)
