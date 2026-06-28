add_library(ulib STATIC string.c printf.c fd.c)

target_compile_options(ulib PRIVATE
  -ffreestanding
  -nostdlib
  -Wall -Wextra -Werror
  -fno-pic
  -m32
  -g
)

target_include_directories(ulib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
