add_executable(arachnyaa_ttyd main.c kb.c fb.c vterm.c)

set_target_properties(arachnyaa_ttyd PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

target_compile_options(arachnyaa_ttyd PRIVATE
    -ffreestanding -nostdlib -Wall -Wextra -Werror -nostartfiles -fno-pic -m32 -g
)

target_include_directories(arachnyaa_ttyd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../ulib)

target_link_libraries(arachnyaa_ttyd PRIVATE ulib)
