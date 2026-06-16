add_executable(arachnyaa_ramfs main.c)

set_target_properties(arachnyaa_ramfs PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

target_compile_options(arachnyaa_ramfs PRIVATE
  -ffreestanding
  -nostdlib
  -Wall -Wextra -Werror
  -nostartfiles
  -fno-pic
  -m32
  -g
)

target_include_directories(arachnyaa_ramfs PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(arachnyaa_ramfs PRIVATE ulib)
