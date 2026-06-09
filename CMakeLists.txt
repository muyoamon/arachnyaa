cmake_minimum_required(VERSION 3.28)
project(arachnyaa C ASM_NASM)

enable_language(ASM_NASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)



add_compile_options(
  -Wall -Wextra -Werror
  -g
)

add_subdirectory(src/)
add_subdirectory(lib/)
add_subdirectory(user/)

if(TARGET arachnyaa_kernel)
    set(KERNEL_BINARY ${CMAKE_BINARY_DIR}/src/arachnyaa_kernel)
    set(INIT_BINARY ${CMAKE_BINARY_DIR}/user/init)
    set(ISO_DIR ${CMAKE_BINARY_DIR}/iso)
    set(ISO_FILE ${CMAKE_BINARY_DIR}/arachnyaa.iso)

    # Prepare ISO structure
    add_custom_command(
        OUTPUT ${ISO_DIR}/boot/grub/grub.cfg
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ISO_DIR}/boot/grub
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/boot/grub.cfg
            ${ISO_DIR}/boot/grub/grub.cfg
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${KERNEL_BINARY}
            ${ISO_DIR}/boot/arachnyaa_kernel
        DEPENDS arachnyaa_kernel ${CMAKE_SOURCE_DIR}/boot/grub.cfg
        COMMENT "Preparing ISO directory..."
    )

    add_custom_command(
        OUTPUT ${ISO_DIR}/boot/initd
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:arachnyaa_init>
          ${ISO_DIR}/boot/initd
        DEPENDS arachnyaa_init
        COMMENT "Copy arachnyaa_init to ${ISO_DIR}/boot/initd"
    )

    add_custom_command(
        OUTPUT ${ISO_DIR}/boot/log-client
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:arachnyaa_log_client>
          ${ISO_DIR}/boot/log-client
        DEPENDS arachnyaa_log_client
        COMMENT "Copy arachnyaa_log_client to ${ISO_DIR}/boot/log-client"
    )

    add_custom_command(
        OUTPUT ${ISO_DIR}/boot/ttyd
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:arachnyaa_ttyd>
          ${ISO_DIR}/boot/ttyd
        DEPENDS arachnyaa_ttyd
        COMMENT "Copy arachnyaa_ttyd to ${ISO_DIR}/boot/ttyd"
    )

    add_custom_command(
        OUTPUT ${ISO_DIR}/boot/procd
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:arachnyaa_procd>
          ${ISO_DIR}/boot/procd
        DEPENDS arachnyaa_procd
        COMMENT "Copy arachnyaa_procd to ${ISO_DIR}/boot/procd"
    )

    # Build ISO using grub-mkrescue
    add_custom_target(iso
        COMMAND grub-mkrescue -o ${ISO_FILE} ${ISO_DIR}
        DEPENDS ${ISO_DIR}/boot/grub/grub.cfg ${ISO_DIR}/boot/initd ${ISO_DIR}/boot/log-client ${ISO_DIR}/boot/ttyd ${ISO_DIR}/boot/procd
        COMMENT "Building bootable ISO image..."
        VERBATIM
    )

    # Run QEMU
    add_custom_target(run-qemu
        COMMAND qemu-system-i386 -cdrom ${ISO_FILE}
        DEPENDS iso
        COMMENT "Running QEMU..."
        VERBATIM
    )

    add_custom_target(debug-qemu-iso
        COMMAND qemu-system-i386 -cdrom ${ISO_FILE} -d int -s -S -monitor stdio# -s: GDB stub, -S: Stop at startup
        DEPENDS iso
        COMMENT "Running QEMU in debug mode (localhost:1234)..."
        VERBATIM
    )
endif()




