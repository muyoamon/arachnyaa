add_executable(arachnyaa_echo main.c)

set_target_properties(arachnyaa_echo PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

target_compile_options(arachnyaa_echo PRIVATE
  -ffreestanding
  -nostdlib
  -Wall -Wextra -Werror
  -nostartfiles
  -fno-pic
  -m32
  -g
)

target_sources(arachnyaa_echo PRIVATE
  ${CMAKE_SOURCE_DIR}/user/ulib/crt0.S
)

target_link_libraries(arachnyaa_echo PRIVATE ulib)
