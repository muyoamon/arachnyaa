set(BLK_FLAGS -ffreestanding -nostdlib -Wall -Wextra -Werror -nostartfiles -fno-pic -m32 -g)

add_executable(arachnyaa_ramdiskd ramdiskd.c)
set_target_properties(arachnyaa_ramdiskd PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
target_compile_options(arachnyaa_ramdiskd PRIVATE ${BLK_FLAGS})
target_include_directories(arachnyaa_ramdiskd PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(arachnyaa_ramdiskd PRIVATE ulib)

add_executable(arachnyaa_atad atad.c)
set_target_properties(arachnyaa_atad PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
target_compile_options(arachnyaa_atad PRIVATE ${BLK_FLAGS})
target_include_directories(arachnyaa_atad PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(arachnyaa_atad PRIVATE ulib)
