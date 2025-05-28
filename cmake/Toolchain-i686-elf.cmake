set(CMAKE_SYSTEM_NAME Generic) # Bare metal!
set(CMAKE_SYSTEM_PROCESSOR i686)

# --- Specify Cross Compilers ---
set(TOOLCHAIN_PREFIX i686-elf-)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc) # Using GCC for ASM (GAS syntax)

# --- Set Compiler Flags ---
set(CMAKE_C_FLAGS "-m32 -ffreestanding -nostdlib -O0 -g" CACHE STRING "C Flags")
set(CMAKE_ASM_FLAGS "-m32 -g" CACHE STRING "ASM Flags")

# --- Set Linker Flags ---
set(CMAKE_EXE_LINKER_FLAGS "-nostdlib -nostartfiles -lgcc" CACHE STRING "Linker Flags")

# --- Search Paths ---
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# --- NASM ---
set(CMAKE_ASM_NASM_COMPILER nasm CACHE STRING "NASM Compiler")
set(CMAKE_ASM_NASM_FLAGS "-f elf32 -g -F dwarf" CACHE STRING "NASM Flags")
