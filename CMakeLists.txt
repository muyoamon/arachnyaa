cmake_minimum_required(VERSION 3.28)
project(arachnyaa C ASM ASM_NASM)

enable_language(ASM)
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

    add_custom_command(
        OUTPUT ${ISO_DIR}/boot/elfloader
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:arachnyaa_elfloader>
          ${ISO_DIR}/boot/elfloader
        DEPENDS arachnyaa_elfloader
        COMMENT "Copy arachnyaa_elfloader to ${ISO_DIR}/boot/elfloader"
    )

    add_custom_command(
        OUTPUT ${ISO_DIR}/boot/ramfs
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:arachnyaa_ramfs>
          ${ISO_DIR}/boot/ramfs
        DEPENDS arachnyaa_ramfs
        COMMENT "Copy arachnyaa_ramfs to ${ISO_DIR}/boot/ramfs"
    )

    add_custom_command(
        OUTPUT ${ISO_DIR}/boot/vfs
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:arachnyaa_vfs>
          ${ISO_DIR}/boot/vfs
        DEPENDS arachnyaa_vfs
        COMMENT "Copy arachnyaa_vfs to ${ISO_DIR}/boot/vfs"
    )

    add_custom_command(
        OUTPUT ${ISO_DIR}/boot/shell
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:arachnyaa_shell>
          ${ISO_DIR}/boot/shell
        DEPENDS arachnyaa_shell
        COMMENT "Copy arachnyaa_shell to ${ISO_DIR}/boot/shell"
    )

    add_custom_command(
        OUTPUT ${ISO_DIR}/boot/echo
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:arachnyaa_echo>
          ${ISO_DIR}/boot/echo
        DEPENDS arachnyaa_echo
        COMMENT "Copy arachnyaa_echo to ${ISO_DIR}/boot/echo"
    )

    # Build ISO using grub-mkrescue
    add_custom_target(iso
        COMMAND grub-mkrescue -o ${ISO_FILE} ${ISO_DIR}
        DEPENDS ${ISO_DIR}/boot/grub/grub.cfg ${ISO_DIR}/boot/initd ${ISO_DIR}/boot/log-client ${ISO_DIR}/boot/ttyd ${ISO_DIR}/boot/procd ${ISO_DIR}/boot/elfloader ${ISO_DIR}/boot/ramfs ${ISO_DIR}/boot/vfs ${ISO_DIR}/boot/shell ${ISO_DIR}/boot/echo
        COMMENT "Building bootable ISO image..."
        VERBATIM
    )

    # Run QEMU
    add_custom_target(run-qemu
        COMMAND qemu-system-i386 -cdrom ${ISO_FILE} -serial stdio
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




