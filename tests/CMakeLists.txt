cmake_minimum_required(VERSION 3.16)

include(GoogleTest)

# Test executable
add_executable(wikilib_tests
    test_main.cpp
    core/test_text_utils.cpp
    core/test_unicode_utils.cpp
    core/test_string_pool.cpp
    markup/test_tokenizer.cpp
    markup/test_parser.cpp
    markup/test_ast.cpp
    templates/test_template_parser.cpp
)

target_link_libraries(wikilib_tests
    PRIVATE
        wikilib::wikilib
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(wikilib_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Register tests
gtest_discover_tests(wikilib_tests)

# Test data directory
#add_custom_target(copy_test_data
#    COMMAND ${CMAKE_COMMAND} -E copy_directory
#        ${CMAKE_CURRENT_SOURCE_DIR}/data
#        ${CMAKE_CURRENT_BINARY_DIR}/data
#)
#add_dependencies(wikilib_tests copy_test_data)
