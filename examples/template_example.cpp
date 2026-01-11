/**
 * @file template_example.cpp
 * @brief Example: Working with MediaWiki templates
 */

#include <iostream>
#include <wikilib.hpp>

using namespace wikilib;
using namespace wikilib::templates;

void example_parse_template_definition() {
    std::cout << "=== Template Definition Parsing ===" << std::endl;

    // Example template definition
    const char* template_def = R"(
<noinclude>
This template creates a simple infobox.
</noinclude>
{| class="infobox"
! colspan="2" | {{{title|Unknown}}}
|-
| '''Name:''' || {{{name}}}
|-
| '''Age:''' || {{{age|N/A}}}
|-
| '''Location:''' || {{{location|Unknown}}}
|}
<noinclude>
{{Documentation}}
</noinclude>
)";

    TemplateParser parser;
    auto def = parser.parse(template_def);

    std::cout << "Template content length: " << def.content.size() << " bytes" << std::endl;
    std::cout << "Found parameters:" << std::endl;
    for (const auto& param : def.documented_params) {
        std::cout << "  - {{{" << param << "}}}" << std::endl;
    }
    std::cout << std::endl;
}

void example_parse_invocations() {
    std::cout << "=== Template Invocation Parsing ===" << std::endl;

    // Example wikitext with template invocations
    const char* wikitext = R"(
This article uses several templates:

{{Infobox person
| name = John Doe
| age = 30
| location = New York
}}

Some text with {{cite book|title=Example|author=Smith}}.

{{#if: {{{1|}}} | Value is {{{1}}} | No value provided}}
)";

    auto invocations = find_invocations(wikitext);

    std::cout << "Found " << invocations.size() << " template invocations:" << std::endl;
    for (const auto& inv : invocations) {
        std::cout << "  {{" << inv.name << "}}";
        if (!inv.parameters.empty()) {
            std::cout << " with " << inv.parameters.size() << " parameter(s)";
        }
        std::cout << std::endl;

        // Show parameters
        for (const auto& [name, value] : inv.parameters) {
            if (name.empty()) {
                std::cout << "    - (positional): " << value << std::endl;
            } else {
                std::cout << "    - " << name << " = " << value << std::endl;
            }
        }
    }
    std::cout << std::endl;
}

void example_template_invocation_api() {
    std::cout << "=== Template Invocation API ===" << std::endl;

    auto result = parse_invocation("{{Template|first|second|name=value|other=data}}");

    if (result.has_value()) {
        const auto& inv = result.value();

        std::cout << "Template name: " << inv.name << std::endl;
        std::cout << "Parameter count: " << inv.parameters.size() << std::endl;

        // Get by position
        auto first = inv.get(1);
        if (first) {
            std::cout << "First positional parameter: " << *first << std::endl;
        }

        // Get by name
        auto name_param = inv.get("name");
        if (name_param) {
            std::cout << "Named parameter 'name': " << *name_param << std::endl;
        }

        // Get with default
        std::cout << "Parameter 'missing' with default: "
                  << inv.get_or("missing", "DEFAULT") << std::endl;
    }
    std::cout << std::endl;
}

void example_parser_functions() {
    std::cout << "=== Parser Functions ===" << std::endl;

    const char* functions[] = {
        "#if", "#ifeq", "#switch", "#expr", "#time", "#invoke"
    };

    std::cout << "Recognized parser functions:" << std::endl;
    for (const auto& func : functions) {
        if (is_parser_function(func)) {
            auto pf = get_parser_function(func);
            std::cout << "  " << func << " -> " << parser_function_name(pf) << std::endl;
        }
    }
    std::cout << std::endl;
}

void example_template_data() {
    std::cout << "=== TemplateData Parsing ===" << std::endl;

    // Example TemplateData JSON (from Wikipedia)
    const char* template_data_json = R"({
    "description": "An example template with parameters",
    "params": {
        "title": {
            "label": "Title",
            "description": "The title of the item",
            "type": "string",
            "required": true
        },
        "author": {
            "label": "Author",
            "description": "The author's name",
            "type": "string",
            "default": "Unknown"
        },
        "year": {
            "label": "Year",
            "description": "Publication year",
            "type": "number"
        }
    }
})";

    TemplateParser parser;
    auto params = parser.parse_template_data(template_data_json);

    std::cout << "Parsed " << params.size() << " parameters from TemplateData:" << std::endl;
    for (const auto& p : params) {
        std::cout << "  - " << p.name;
        if (p.required) {
            std::cout << " (REQUIRED)";
        }
        std::cout << std::endl;

        if (p.description) {
            std::cout << "    Description: " << *p.description << std::endl;
        }
        if (p.default_value) {
            std::cout << "    Default: " << *p.default_value << std::endl;
        }
    }
    std::cout << std::endl;
}

void example_redirect_detection() {
    std::cout << "=== Redirect Detection ===" << std::endl;

    const char* redirect_template = "#REDIRECT [[Target Template]]";

    TemplateParser parser;
    auto target = parser.get_redirect_target(redirect_template);

    if (target) {
        std::cout << "This is a redirect to: " << *target << std::endl;
    } else {
        std::cout << "Not a redirect" << std::endl;
    }
    std::cout << std::endl;
}

int main() {
    std::cout << "MediaWiki Template Processing Examples" << std::endl;
    std::cout << "=======================================" << std::endl;
    std::cout << std::endl;

    example_parse_template_definition();
    example_parse_invocations();
    example_template_invocation_api();
    example_parser_functions();
    example_template_data();
    example_redirect_detection();

    std::cout << "All examples completed!" << std::endl;

    return 0;
}
