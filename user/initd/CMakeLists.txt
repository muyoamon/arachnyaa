add_executable(arachnyaa_init main.c)

set_target_properties(arachnyaa_init PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

target_compile_options(arachnyaa_init PRIVATE
  -ffreestanding
  -nostdlib
  -Wall -Wextra -Werror
  -nostartfiles
  -fno-pic
  -m32
  -g
)

target_include_directories(arachnyaa_init PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(arachnyaa_init PRIVATE ulib)

if (TARGET arachyaa_init)
  set(ISO_DIR ${CMAKE_BINARY_DIR}/iso)
  set(BINARY ${CMAKE_BINARY_DIR}/user/arachnyaa_init)


endif()
