add_executable(arachnyaa_ls main.c)

set_target_properties(arachnyaa_ls PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

target_compile_options(arachnyaa_ls PRIVATE
  -ffreestanding
  -nostdlib
  -Wall -Wextra -Werror
  -nostartfiles
  -fno-pic
  -m32
  -g
)

target_sources(arachnyaa_ls PRIVATE
  ${CMAKE_SOURCE_DIR}/user/ulib/crt0.S
)

target_link_libraries(arachnyaa_ls PRIVATE ulib)
