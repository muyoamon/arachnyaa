add_executable(arachnyaac main.c) 

file(GLOB sources
  "${CMAKE_SOURCE_DIR}/src/*[!.txt]/"
)
foreach(i IN ITEMS ${sources})
    IF(IS_DIRECTORY ${i})
        add_subdirectory(${i})
        target_include_directories(arachnyaac PRIVATE ${i})
        message(STATUS ${i})
    endif()
endforeach()

