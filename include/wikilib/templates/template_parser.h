#pragma once

/**
 * @file template_parser.hpp
 * @brief Parser for MediaWiki template definitions
 */

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "wikilib/core/types.h"
#include "wikilib/markup/ast.h"

namespace wikilib::templates {

// ============================================================================
// Template definition
// ============================================================================

/**
 * @brief Parsed template definition
 */
struct TemplateDefinition {
    std::string name;
    std::string content; // Raw wikitext
    std::vector<std::string> documented_params; // Parameters mentioned in doc
    std::optional<std::string> description;
    bool is_parser_function = false;

    [[nodiscard]] bool empty() const {
        return content.empty();
    }
};

/**
 * @brief Template parameter info
 */
struct ParameterInfo {
    std::string name;
    std::optional<std::string> default_value;
    std::optional<std::string> description;
    bool required = false;
    std::vector<std::string> aliases;
};

// ============================================================================
// Template parser
// ============================================================================

/**
 * @brief Parser for template definitions and documentation
 */
class TemplateParser {
public:
    /**
     * @brief Parse template page content
     */
    [[nodiscard]] TemplateDefinition parse(std::string_view content);

    /**
     * @brief Extract parameter documentation from TemplateData
     */
    [[nodiscard]] std::vector<ParameterInfo> parse_template_data(std::string_view json);

    /**
     * @brief Find all parameter references in template
     */
    [[nodiscard]] std::vector<std::string> find_parameters(std::string_view content);

    /**
     * @brief Check if content is a template redirect
     */
    [[nodiscard]] std::optional<std::string> get_redirect_target(std::string_view content);
};

// ============================================================================
// Template invocation parsing
// ============================================================================

/**
 * @brief Parsed template invocation
 */
struct TemplateInvocation {
    std::string name;
    std::vector<std::pair<std::string, std::string>> parameters; // name -> value
    SourceRange location;

    /**
     * @brief Get parameter value by name
     */
    [[nodiscard]] std::optional<std::string_view> get(std::string_view name) const;

    /**
     * @brief Get positional parameter (1-indexed)
     */
    [[nodiscard]] std::optional<std::string_view> get(size_t position) const;

    /**
     * @brief Get parameter with default
     */
    [[nodiscard]] std::string_view get_or(std::string_view name, std::string_view default_value) const;
};

/**
 * @brief Parse a single template invocation
 */
[[nodiscard]] Result<TemplateInvocation> parse_invocation(std::string_view input);

/**
 * @brief Find all template invocations in text
 */
[[nodiscard]] std::vector<TemplateInvocation> find_invocations(std::string_view input);

// ============================================================================
// Parser function handling
// ============================================================================

/**
 * @brief Known parser functions
 */
enum class ParserFunction {
    If, // #if:
    Ifeq, // #ifeq:
    Ifexist, // #ifexist:
    Ifexpr, // #ifexpr:
    Switch, // #switch:
    Expr, // #expr:
    Time, // #time:
    Titleparts, // #titleparts:
    Invoke, // #invoke: (Lua modules)
    Tag, // #tag:
    Language, // #language:
    Unknown
};

/**
 * @brief Check if name is a parser function
 */
[[nodiscard]] bool is_parser_function(std::string_view name) noexcept;

/**
 * @brief Get parser function type from name
 */
[[nodiscard]] ParserFunction get_parser_function(std::string_view name) noexcept;

/**
 * @brief Get parser function name
 */
[[nodiscard]] std::string_view parser_function_name(ParserFunction func) noexcept;

} // namespace wikilib::templates

