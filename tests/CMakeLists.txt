# Unit tests for the pure layout module. layout.c must stay free of SDL, so this links it alone.
add_executable(test_layout test_layout.c "${PROJECT_SOURCE_DIR}/src/layout.c")
target_include_directories(test_layout PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME layout COMMAND test_layout)

# Unit tests for the pure UTF-8 helpers (title truncation), likewise linked on their own
add_executable(test_utf8 test_utf8.c "${PROJECT_SOURCE_DIR}/src/utf8.c")
target_include_directories(test_utf8 PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME utf8 COMMAND test_utf8)

# A function to link inih the way the launcher does
function(link_inih target)
  if (UNIX)
    target_link_libraries(${target} PkgConfig::INIH)
  else ()
    target_link_libraries(${target} ${INIH})
    target_include_directories(${target} PRIVATE ${INIH_INCLUDE_DIR})
  endif ()
endfunction()

# Unit tests for the icon library's manifest reader. library.c must stay free of SDL, so it links with inih alone.
add_executable(test_library test_library.c "${PROJECT_SOURCE_DIR}/src/library.c")
target_include_directories(test_library PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(test_library PRIVATE LIBRARY_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/library")
link_inih(test_library)
add_test(NAME library COMMAND test_library)
