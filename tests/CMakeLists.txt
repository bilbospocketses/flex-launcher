# Unit tests for the pure layout module. layout.c must stay free of SDL, so this links it alone.
add_executable(test_layout test_layout.c "${PROJECT_SOURCE_DIR}/src/layout.c")
target_include_directories(test_layout PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME layout COMMAND test_layout)

# Unit tests for the pure UTF-8 helpers (title truncation), likewise linked on their own
add_executable(test_utf8 test_utf8.c "${PROJECT_SOURCE_DIR}/src/utf8.c")
target_include_directories(test_utf8 PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME utf8 COMMAND test_utf8)
