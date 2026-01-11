/**
 * @file plain_text_example.cpp
 * @brief Example: Converting wikitext to plain text
 */

#include <iostream>
#include <wikilib.hpp>

using namespace wikilib;
using namespace wikilib::output;

void example_basic_conversion() {
    std::cout << "=== Basic Wikitext to Plain Text ===" << std::endl;

    const char* wikitext = R"(
== Introduction ==
This is a '''bold''' and ''italic'' text example.

MediaWiki is a [[wiki]] software that powers [[Wikipedia]].

=== Features ===
* Free and open source
* Multilingual support
* Rich markup language

{{Note|This is a template}}
)";

    std::string plain = to_plain_text(wikitext);

    std::cout << "Plain text output:" << std::endl;
    std::cout << "---" << std::endl;
    std::cout << plain << std::endl;
    std::cout << "---" << std::endl;
    std::cout << std::endl;
}

void example_custom_configuration() {
    std::cout << "=== Custom Configuration ===" << std::endl;

    const char* wikitext = R"(
== Section Title ==
Visit [[Main Page|the main page]] for more information.

* Item one
* Item two
* Item three
)";

    // Configuration 1: Show link targets
    PlainTextConfig config1;
    config1.show_link_targets = true;
    config1.heading_underlines = true;

    std::cout << "With link targets and heading underlines:" << std::endl;
    std::cout << "---" << std::endl;
    std::cout << to_plain_text(wikitext, config1) << std::endl;
    std::cout << "---" << std::endl;
    std::cout << std::endl;

    // Configuration 2: ASCII bullets, no headings
    PlainTextConfig config2;
    config2.include_headings = false;
    config2.list_item_prefix = "- ";

    std::cout << "No headings, ASCII bullets:" << std::endl;
    std::cout << "---" << std::endl;
    std::cout << to_plain_text(wikitext, config2) << std::endl;
    std::cout << "---" << std::endl;
    std::cout << std::endl;
}

void example_extract_summary() {
    std::cout << "=== Extract Summary ===" << std::endl;

    const char* article = R"(
'''MediaWiki''' is a free and open-source wiki software platform.
Originally developed for use on [[Wikipedia]], it is now used by several
other projects of the [[Wikimedia Foundation]] and by many other wikis.

MediaWiki is written in the [[PHP]] programming language and uses a
backend database. It is highly customizable and supports a wide range
of features including user access controls, content categorization, and
extensive API for integration with other systems.

== History ==
MediaWiki was originally developed by [[Magnus Manske]] and improved by
[[Lee Daniel Crocker]]. The software was first released in 2002.

== Features ==
=== Core Features ===
* Page editing with revision history
* User management and permissions
* Built-in search functionality
)";

    // Extract first paragraph
    std::string first_para = extract_first_paragraph(article);
    std::cout << "First paragraph:" << std::endl;
    std::cout << "---" << std::endl;
    std::cout << first_para << std::endl;
    std::cout << "---" << std::endl;
    std::cout << std::endl;

    // Extract summary (max 200 chars)
    std::string summary = extract_summary(article, 200);
    std::cout << "Summary (200 chars):" << std::endl;
    std::cout << "---" << std::endl;
    std::cout << summary << std::endl;
    std::cout << "---" << std::endl;
    std::cout << std::endl;
}

void example_complex_markup() {
    std::cout << "=== Complex Markup Conversion ===" << std::endl;

    const char* complex = R"(
{| class="wikitable"
|+ Table Caption
! Header 1 !! Header 2
|-
| Cell 1 || Cell 2
|-
| Cell 3 || Cell 4
|}

# First item
# Second item
## Nested item
## Another nested item
# Third item

External link: [https://www.mediawiki.org MediaWiki]

[[Category:Examples]]
)";

    PlainTextConfig config;
    config.include_tables = true;

    std::string plain = to_plain_text(complex, config);

    std::cout << "Converted output:" << std::endl;
    std::cout << "---" << std::endl;
    std::cout << plain << std::endl;
    std::cout << "---" << std::endl;
    std::cout << std::endl;
}

void example_custom_converter() {
    std::cout << "=== Custom Converter Configuration ===" << std::endl;

    const char* wikitext = R"(
= Main Title =
This is the first paragraph with '''bold''' text.

== Subsection ==
This is the second paragraph.

* List item 1
* List item 2
)";

    PlainTextConverter converter;

    // Customize configuration
    converter.config().heading_underlines = true;
    converter.config().list_bullets = true;
    converter.config().paragraph_separator = "\n\n---\n\n";

    // Parse first
    auto parse_result = markup::parse(wikitext);
    if (parse_result.success()) {
        std::string plain = converter.convert(*parse_result.document);

        std::cout << "Custom conversion:" << std::endl;
        std::cout << "---" << std::endl;
        std::cout << plain << std::endl;
        std::cout << "---" << std::endl;
    }
    std::cout << std::endl;
}

void example_line_wrapping() {
    std::cout << "=== Line Wrapping ===" << std::endl;

    const char* wikitext = R"(
This is a very long paragraph that should be wrapped at a specific column width to make it more readable when displayed in a terminal or text file that has limited width constraints.
)";

    PlainTextConfig config;
    config.max_line_width = 50;  // Wrap at 50 characters

    std::string wrapped = to_plain_text(wikitext, config);

    std::cout << "Wrapped at 50 characters:" << std::endl;
    std::cout << "---" << std::endl;
    std::cout << wrapped << std::endl;
    std::cout << "---" << std::endl;
    std::cout << std::endl;
}

int main() {
    std::cout << "Plain Text Conversion Examples" << std::endl;
    std::cout << "==============================" << std::endl;
    std::cout << std::endl;

    example_basic_conversion();
    example_custom_configuration();
    example_extract_summary();
    example_complex_markup();
    example_custom_converter();
    example_line_wrapping();

    std::cout << "All examples completed!" << std::endl;

    return 0;
}
