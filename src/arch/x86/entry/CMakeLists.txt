# --- append NASM_SOURCES ---
list(APPEND NASM_SOURCES
  entry/switch_to_user.s
  entry/user_start.s
)

set(NASM_SOURCES ${NASM_SOURCES} PARENT_SCOPE)

