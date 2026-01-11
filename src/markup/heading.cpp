/**
 * @file heading.cpp
 * @brief Implementation of heading parsing utilities
 */

#include "wikilib/markup/heading.h"
#include "wikilib/markup/parser.h"
#include "wikilib/markup/ast.h"

namespace wikilib::markup {

HeadingInfo parse_heading(std::string_view input) {
    Parser parser;
    auto result = parser.parse(input);

    HeadingInfo info;
    if (!result.success()) {
        return info;
    }

    for (const auto& node : result.document->content) {
        if (node->type == NodeType::Heading) {
            info.found = true;
            auto* heading = static_cast<HeadingNode*>(node.get());
            info.level = heading->level;

            // Extract title from content
            for (const auto& child : heading->content) {
                if (child->type == NodeType::Text) {
                    auto* text = static_cast<TextNode*>(child.get());
                    info.title += text->text;
                }
            }
            break;
        }
    }

    return info;
}

} // namespace wikilib::markup