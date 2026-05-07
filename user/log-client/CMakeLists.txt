add_executable(arachnyaa_log_client main.c)

set_target_properties(arachnyaa_log_client PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

target_compile_options(arachnyaa_log_client PRIVATE
  -ffreestanding
  -nostdlib
  -Wall -Wextra -Werror
  -nostartfiles
  -fno-pic
  -m32
  -g
)

if (TARGET arachnyaa_log_client) 
  set(ISO_DIR ${CMAKE_BINARY_DIR}/iso)
  set(BINARY ${CMAKE_BINARY_DIR}/user/arachnyaa_log_client)


endif()
