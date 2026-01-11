/**
 * @file parse_example.cpp
 * @brief Example: Parsing wikitext and extracting information
 */

#include <iostream>
#include <wikilib.hpp>

using namespace wikilib;
using namespace wikilib::markup;

int main() {
    // Sample wikitext
    const char *wikitext = R"(
== Introduction ==
This is a '''sample''' article about [[MediaWiki]] markup.

=== Features ===
* '''Bold''' text
* ''Italic'' text  
* [[Internal links|Links]]
* {{Template|param=value}}

== External Links ==
* [https://www.mediawiki.org MediaWiki website]

[[Category:Examples]]
)";

    // Parse the wikitext
    auto result = parse(wikitext);

    if (!result.success()) {
        std::cerr << "Parse failed!" << std::endl;
        for (const auto &error: result.errors) {
            std::cerr << "  " << error.format() << std::endl;
        }
        return 1;
    }

    std::cout << "Parse successful!" << std::endl;
    std::cout << std::endl;

    // Extract links
    LinkExtractor links;
    links.visit(*result.document);

    std::cout << "Links found:" << std::endl;
    for (const auto &link: links.links()) {
        std::cout << "  - " << link.target;
        if (link.is_external) {
            std::cout << " (external)";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    // Extract templates
    TemplateExtractor templates;
    templates.visit(*result.document);

    std::cout << "Templates found:" << std::endl;
    for (const auto &tmpl: templates.templates()) {
        std::cout << "  - {{" << tmpl.name << "}}";
        if (!tmpl.parameters.empty()) {
            std::cout << " with " << tmpl.parameters.size() << " parameter(s)";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    // Extract sections
    SectionExtractor sections;
    sections.visit(*result.document);

    std::cout << "Sections:" << std::endl;
    for (const auto &section: sections.sections()) {
        std::string indent(section.level - 1, ' ');
        std::cout << indent << "- " << section.title << " (level " << section.level << ")" << std::endl;
    }
    std::cout << std::endl;

    // Extract categories
    CategoryExtractor categories;
    categories.visit(*result.document);

    std::cout << "Categories:" << std::endl;
    for (const auto &cat: categories.categories()) {
        std::cout << "  - " << cat.name << std::endl;
    }
    std::cout << std::endl;

    // Convert to plain text
    PlainTextExtractor text_extractor;
    text_extractor.visit(*result.document);

    std::cout << "Plain text:" << std::endl;
    std::cout << "---" << std::endl;
    std::cout << text_extractor.text() << std::endl;
    std::cout << "---" << std::endl;

    return 0;
}
