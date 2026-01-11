cmake_minimum_required(VERSION 3.16)

project(wikilib
    VERSION 0.1.0
    DESCRIPTION "Common library for parsing Wikipedia and Wiktionary MediaWiki markup"
    LANGUAGES CXX
)

# ============================================================================
# Build options
# ============================================================================
option(WIKILIB_BUILD_TESTS "Build unit tests" ON)
option(WIKILIB_BUILD_EXAMPLES "Build example programs" ON)
option(WIKILIB_BUILD_DOCS "Build documentation" OFF)
option(WIKILIB_USE_SYSTEM_DEPS "Use system-installed dependencies" OFF)

# ============================================================================
# C++ Standard
# ============================================================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Output directories
# ============================================================================
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ============================================================================
# Dependencies
# ============================================================================
include(cmake/Dependencies.cmake)

# ============================================================================
# Library target
# ============================================================================
add_library(wikilib)
add_library(wikilib::wikilib ALIAS wikilib)

target_sources(wikilib PRIVATE
    # Core
    src/core/text_utils.cpp
    src/core/unicode_utils.cpp
    src/core/string_pool.cpp
    
    # MediaWiki markup parsing
    src/markup/tokenizer.cpp
    src/markup/parser.cpp
    src/markup/ast.cpp
    src/markup/wikitext_visitor.cpp
    
    # Template handling
    src/templates/template_parser.cpp
    src/templates/template_expander.cpp
    #src/templates/parameter_parser.cpp
    
    # Dump processing
    src/dump/xml_reader.cpp
    src/dump/bz2_stream.cpp
    src/dump/page_handler.cpp
    src/dump/index_parser.cpp
    
    # Output formats
    src/output/plain_text.cpp
    src/output/json_writer.cpp
)

target_include_directories(wikilib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(wikilib PUBLIC cxx_std_20)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(wikilib PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror=return-type
    )
endif()

# Link dependencies
target_link_libraries(wikilib
    PUBLIC
        ${BZIP2_LIBRARIES}
    PRIVATE
        pugixml::pugixml
)

# ============================================================================
# Tests
# ============================================================================
if(WIKILIB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Examples
# ============================================================================
if(WIKILIB_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ============================================================================
# Installation
# ============================================================================
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS wikilib
    EXPORT wikilibTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/wikilib
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT wikilibTargets
    FILE wikilibTargets.cmake
    NAMESPACE wikilib::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wikilib
)

configure_package_config_file(
    cmake/wikilibConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/wikilibConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wikilib
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/wikilibConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/wikilibConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/wikilibConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wikilib
)
