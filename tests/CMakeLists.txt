# Unit tests for the pure layout module. layout.c must stay free of SDL, so this links it alone.
add_executable(test_layout test_layout.c "${PROJECT_SOURCE_DIR}/src/layout.c")
target_include_directories(test_layout PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME layout COMMAND test_layout)

# Unit tests for the pure UTF-8 helpers (title truncation), likewise linked on their own
add_executable(test_utf8 test_utf8.c "${PROJECT_SOURCE_DIR}/src/utf8.c")
target_include_directories(test_utf8 PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME utf8 COMMAND test_utf8)

# Unit tests for UTF-8 file access (non-ASCII names, the replace and its retry on Windows)
add_executable(test_fileio test_fileio.c "${PROJECT_SOURCE_DIR}/src/fileio.c")
target_include_directories(test_fileio PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME fileio COMMAND test_fileio)

# Unit tests for editing config.ini line by line; the shipped sample config must survive a round trip
add_executable(test_inidoc test_inidoc.c "${PROJECT_SOURCE_DIR}/src/inidoc.c" "${PROJECT_SOURCE_DIR}/src/fileio.c")
target_include_directories(test_inidoc PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(test_inidoc PRIVATE SAMPLE_CONFIG="${PROJECT_BINARY_DIR}/config.ini")
add_test(NAME inidoc COMMAND test_inidoc)

# Unit tests for the settings screen's save: fresh read, edits, backup, replace and the Linux fallback
add_executable(test_config_save test_config_save.c "${PROJECT_SOURCE_DIR}/src/config_save.c"
  "${PROJECT_SOURCE_DIR}/src/inidoc.c" "${PROJECT_SOURCE_DIR}/src/fileio.c")
target_include_directories(test_config_save PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME config_save COMMAND test_config_save)

# Unit tests for the settings table and the settings screen's pages; settings.c reads counts and
# sizes through layout.c's parsers
add_executable(test_settings test_settings.c "${PROJECT_SOURCE_DIR}/src/settings.c" "${PROJECT_SOURCE_DIR}/src/layout.c")
target_include_directories(test_settings PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME settings COMMAND test_settings)

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
add_executable(test_library test_library.c "${PROJECT_SOURCE_DIR}/src/library.c" "${PROJECT_SOURCE_DIR}/src/fileio.c")
target_include_directories(test_library PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(test_library PRIVATE LIBRARY_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/library")
link_inih(test_library)
add_test(NAME library COMMAND test_library)

# Every generic icon in the shipped library must parse with nanosvg, the launcher's own SVG parser
add_executable(test_library_svg test_library_svg.c)
target_include_directories(test_library_svg PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/external")
target_compile_definitions(test_library_svg PRIVATE LIBRARY_ROOT="${PROJECT_SOURCE_DIR}/assets/icons/library")
link_inih(test_library_svg)
if (UNIX)
  target_link_libraries(test_library_svg m)
endif ()
add_test(NAME library_svg COMMAND test_library_svg)
