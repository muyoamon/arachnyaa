add_executable(arachnyaa_cat main.c)

set_target_properties(arachnyaa_cat PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

target_compile_options(arachnyaa_cat PRIVATE
  -ffreestanding
  -nostdlib
  -Wall -Wextra -Werror
  -nostartfiles
  -fno-pic
  -m32
  -g
)

target_sources(arachnyaa_cat PRIVATE
  ${CMAKE_SOURCE_DIR}/user/ulib/crt0.S
)

target_link_libraries(arachnyaa_cat PRIVATE ulib)
