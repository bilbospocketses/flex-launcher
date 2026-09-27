# Unit tests for the pure layout module. layout.c must stay free of SDL, so this links it alone.
add_executable(test_layout test_layout.c "${PROJECT_SOURCE_DIR}/src/layout.c")
target_include_directories(test_layout PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME layout COMMAND test_layout)
