add_executable(arachnyaa_shell main.c)

set_target_properties(arachnyaa_shell PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

target_compile_options(arachnyaa_shell PRIVATE
  -ffreestanding
  -nostdlib
  -Wall -Wextra -Werror
  -nostartfiles
  -fno-pic
  -m32
  -g
)

target_link_libraries(arachnyaa_shell PRIVATE ulib)
