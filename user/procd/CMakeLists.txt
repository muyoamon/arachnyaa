add_executable(arachnyaa_procd main.c)

set_target_properties(arachnyaa_procd PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

target_compile_options(arachnyaa_procd PRIVATE
  -ffreestanding
  -nostdlib
  -Wall -Wextra -Werror
  -nostartfiles
  -fno-pic
  -m32
  -g
)

target_link_libraries(arachnyaa_procd PRIVATE ulib)
