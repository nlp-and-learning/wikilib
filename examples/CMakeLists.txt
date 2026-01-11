cmake_minimum_required(VERSION 3.16)

# Example: Basic parsing
add_executable(example_parse
    parse_example.cpp
)
target_link_libraries(example_parse PRIVATE wikilib::wikilib)

# Example: Dump processing
add_executable(example_dump
    dump_example.cpp
)
target_link_libraries(example_dump PRIVATE wikilib::wikilib)

# Example: Template extraction
add_executable(example_templates
    template_example.cpp
)
target_link_libraries(example_templates PRIVATE wikilib::wikilib)

# Example: Plain text conversion
add_executable(example_plain_text
    plain_text_example.cpp
)
target_link_libraries(example_plain_text PRIVATE wikilib::wikilib)

# Example: Extract pages from dump using index
add_executable(example_extract_pages
    extract_pages_example.cpp
)
target_link_libraries(example_extract_pages PRIVATE wikilib::wikilib)
