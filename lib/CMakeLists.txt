
file(GLOB sources
  "${CMAKE_SOURCE_DIR}/lib/*[!.txt]/"
)
foreach(i IN ITEMS ${sources})
    IF(IS_DIRECTORY ${i})
        add_subdirectory(${i})
        message(STATUS ${i})
    endif()
endforeach()

