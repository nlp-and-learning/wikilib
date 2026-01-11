/**
 * @file template_parser.cpp
 * @brief Implementation of MediaWiki template parser
 */

#include "wikilib/templates/template_parser.h"
#include <algorithm>
#include <cctype>
#include <regex>

namespace wikilib::templates {

// ============================================================================
// TemplateParser implementation
// ============================================================================

TemplateDefinition TemplateParser::parse(std::string_view content) {
    TemplateDefinition def;

    // Check for redirect first
    auto redirect = get_redirect_target(content);
    if (redirect) {
        def.name = *redirect;
        def.content = std::string(content);
        return def;
    }

    def.content = std::string(content);

    // Extract noinclude sections for documentation
    size_t noinclude_start = content.find("<noinclude>");
    if (noinclude_start != std::string_view::npos) {
        size_t noinclude_end = content.find("</noinclude>", noinclude_start);
        if (noinclude_end != std::string_view::npos) {
            std::string_view noinclude_content =
                    content.substr(noinclude_start + 11, noinclude_end - noinclude_start - 11);

            // Look for {{Documentation}} or description
            if (noinclude_content.find("{{Documentation") != std::string_view::npos ||
                noinclude_content.find("{{documentation") != std::string_view::npos) {
                // Has separate documentation page
            }

            // Extract any plain text as description
            std::string desc;
            bool in_tag = false;
            bool in_template = false;
            int template_depth = 0;

            for (size_t i = 0; i < noinclude_content.size(); ++i) {
                char c = noinclude_content[i];
                if (c == '<') {
                    in_tag = true;
                } else if (c == '>') {
                    in_tag = false;
                } else if (c == '{' && i + 1 < noinclude_content.size() && noinclude_content[i + 1] == '{') {
                    in_template = true;
                    template_depth++;
                    ++i;
                } else if (c == '}' && i + 1 < noinclude_content.size() && noinclude_content[i + 1] == '}') {
                    template_depth--;
                    if (template_depth <= 0) {
                        in_template = false;
                        template_depth = 0;
                    }
                    ++i;
                } else if (!in_tag && !in_template) {
                    desc += c;
                }
            }

            // Trim description
            size_t start = desc.find_first_not_of(" \t\n\r");
            size_t end = desc.find_last_not_of(" \t\n\r");
            if (start != std::string::npos && end != std::string::npos && start <= end) {
                def.description = desc.substr(start, end - start + 1);
            }
        }
    }

    // Find all parameters
    def.documented_params = find_parameters(content);

    return def;
}

std::vector<ParameterInfo> TemplateParser::parse_template_data(std::string_view json) {
    std::vector<ParameterInfo> params;

    // Simple JSON parsing for TemplateData format
    // Looking for "params": { "name": { ... }, ... }

    size_t params_pos = json.find("\"params\"");
    if (params_pos == std::string_view::npos) {
        return params;
    }

    // Find the opening brace of params object
    size_t brace_start = json.find('{', params_pos + 8);
    if (brace_start == std::string_view::npos) {
        return params;
    }

    // Parse parameter entries
    int depth = 1;
    size_t i = brace_start + 1;
    std::string current_name;
    bool in_string = false;
    bool escape_next = false;

    while (i < json.size() && depth > 0) {
        char c = json[i];

        if (escape_next) {
            escape_next = false;
            ++i;
            continue;
        }

        if (c == '\\') {
            escape_next = true;
            ++i;
            continue;
        }

        if (c == '"') {
            in_string = !in_string;
            if (in_string && depth == 1) {
                // Start of parameter name
                size_t name_start = i + 1;
                size_t name_end = json.find('"', name_start);
                if (name_end != std::string_view::npos) {
                    current_name = std::string(json.substr(name_start, name_end - name_start));
                    i = name_end;
                }
            }
        } else if (!in_string) {
            if (c == '{') {
                depth++;
                if (depth == 2 && !current_name.empty()) {
                    // Start of parameter object
                    ParameterInfo info;
                    info.name = current_name;

                    // Find the end of this parameter object
                    int param_depth = 1;
                    size_t param_start = i + 1;
                    size_t j = param_start;
                    bool param_in_string = false;

                    while (j < json.size() && param_depth > 0) {
                        char pc = json[j];
                        if (pc == '"' && (j == 0 || json[j - 1] != '\\')) {
                            param_in_string = !param_in_string;
                        } else if (!param_in_string) {
                            if (pc == '{')
                                param_depth++;
                            else if (pc == '}')
                                param_depth--;
                        }
                        ++j;
                    }

                    std::string_view param_json = json.substr(param_start, j - param_start - 1);

                    // Extract description
                    size_t desc_pos = param_json.find("\"description\"");
                    if (desc_pos != std::string_view::npos) {
                        size_t colon = param_json.find(':', desc_pos);
                        size_t quote_start = param_json.find('"', colon + 1);
                        if (quote_start != std::string_view::npos) {
                            size_t quote_end = quote_start + 1;
                            while (quote_end < param_json.size()) {
                                if (param_json[quote_end] == '"' && param_json[quote_end - 1] != '\\') {
                                    break;
                                }
                                ++quote_end;
                            }
                            info.description =
                                    std::string(param_json.substr(quote_start + 1, quote_end - quote_start - 1));
                        }
                    }

                    // Extract default
                    size_t default_pos = param_json.find("\"default\"");
                    if (default_pos != std::string_view::npos) {
                        size_t colon = param_json.find(':', default_pos);
                        size_t quote_start = param_json.find('"', colon + 1);
                        if (quote_start != std::string_view::npos) {
                            size_t quote_end = quote_start + 1;
                            while (quote_end < param_json.size()) {
                                if (param_json[quote_end] == '"' && param_json[quote_end - 1] != '\\') {
                                    break;
                                }
                                ++quote_end;
                            }
                            info.default_value =
                                    std::string(param_json.substr(quote_start + 1, quote_end - quote_start - 1));
                        }
                    }

                    // Check required
                    size_t required_pos = param_json.find("\"required\"");
                    if (required_pos != std::string_view::npos) {
                        size_t true_pos = param_json.find("true", required_pos);
                        if (true_pos != std::string_view::npos && true_pos < required_pos + 20) {
                            info.required = true;
                        }
                    }

                    params.push_back(std::move(info));
                    current_name.clear();
                    i = j - 1;
                    depth = 1;
                }
            } else if (c == '}') {
                depth--;
            }
        }

        ++i;
    }

    return params;
}

std::vector<std::string> TemplateParser::find_parameters(std::string_view content) {
    std::vector<std::string> params;
    std::unordered_map<std::string, bool> seen;

    // Find all {{{param}}} or {{{param|default}}}
    size_t pos = 0;
    while (pos < content.size()) {
        size_t start = content.find("{{{", pos);
        if (start == std::string_view::npos) {
            break;
        }

        // Find matching }}}
        int depth = 1;
        size_t i = start + 3;
        size_t param_name_end = std::string_view::npos;

        while (i < content.size() && depth > 0) {
            if (content[i] == '{' && i + 2 < content.size() && content[i + 1] == '{' && content[i + 2] == '{') {
                depth++;
                i += 3;
            } else if (content[i] == '}' && i + 2 < content.size() && content[i + 1] == '}' && content[i + 2] == '}') {
                depth--;
                if (depth == 0) {
                    break;
                }
                i += 3;
            } else {
                if (content[i] == '|' && depth == 1 && param_name_end == std::string_view::npos) {
                    param_name_end = i;
                }
                ++i;
            }
        }

        if (depth == 0) {
            size_t name_end = (param_name_end != std::string_view::npos) ? param_name_end : i;
            std::string_view param_name = content.substr(start + 3, name_end - start - 3);

            // Trim whitespace
            size_t trim_start = param_name.find_first_not_of(" \t\n");
            size_t trim_end = param_name.find_last_not_of(" \t\n");

            if (trim_start != std::string_view::npos && trim_end != std::string_view::npos) {
                std::string name(param_name.substr(trim_start, trim_end - trim_start + 1));
                if (!seen[name]) {
                    seen[name] = true;
                    params.push_back(name);
                }
            }
        }

        pos = start + 3;
    }

    return params;
}

std::optional<std::string> TemplateParser::get_redirect_target(std::string_view content) {
    // Skip leading whitespace
    size_t start = content.find_first_not_of(" \t\n\r");
    if (start == std::string_view::npos) {
        return std::nullopt;
    }

    content = content.substr(start);

    // Check for #REDIRECT (case-insensitive)
    if (content.size() < 9) {
        return std::nullopt;
    }

    std::string prefix(content.substr(0, 9));
    std::transform(prefix.begin(), prefix.end(), prefix.begin(), [](unsigned char c) { return std::toupper(c); });

    if (prefix != "#REDIRECT") {
        return std::nullopt;
    }

    // Find [[target]]
    size_t link_start = content.find("[[", 9);
    if (link_start == std::string_view::npos) {
        return std::nullopt;
    }

    size_t link_end = content.find("]]", link_start);
    if (link_end == std::string_view::npos) {
        return std::nullopt;
    }

    return std::string(content.substr(link_start + 2, link_end - link_start - 2));
}

// ============================================================================
// TemplateInvocation implementation
// ============================================================================

std::optional<std::string_view> TemplateInvocation::get(std::string_view name) const {
    for (const auto &[param_name, value]: parameters) {
        if (param_name == name) {
            return value;
        }
    }
    return std::nullopt;
}

std::optional<std::string_view> TemplateInvocation::get(size_t position) const {
    size_t current_pos = 1;
    for (const auto &[param_name, value]: parameters) {
        // Positional parameters have empty names or numeric names
        if (param_name.empty()) {
            if (current_pos == position) {
                return value;
            }
            ++current_pos;
        } else {
            // Check if param_name is a number equal to position
            bool is_number = !param_name.empty();
            for (char c: param_name) {
                if (!std::isdigit(static_cast<unsigned char>(c))) {
                    is_number = false;
                    break;
                }
            }
            if (is_number) {
                size_t num = std::stoul(param_name);
                if (num == position) {
                    return value;
                }
            }
        }
    }
    return std::nullopt;
}

std::string_view TemplateInvocation::get_or(std::string_view name, std::string_view default_value) const {
    auto result = get(name);
    return result.value_or(default_value);
}

// ============================================================================
// Free functions
// ============================================================================

Result<TemplateInvocation> parse_invocation(std::string_view input) {
    TemplateInvocation inv;

    // Trim whitespace
    size_t start = input.find_first_not_of(" \t\n");
    if (start == std::string_view::npos) {
        return std::unexpected(ParseError{"Empty template invocation", {}, ErrorSeverity::Error, ""});
    }

    input = input.substr(start);

    // Check for {{ prefix
    if (!input.starts_with("{{")) {
        return std::unexpected(ParseError{"Template invocation must start with {{", {}, ErrorSeverity::Error, ""});
    }

    input = input.substr(2);

    // Find matching }}
    int depth = 1;
    size_t end = 0;
    while (end < input.size() && depth > 0) {
        if (input[end] == '{' && end + 1 < input.size() && input[end + 1] == '{') {
            depth++;
            end += 2;
        } else if (input[end] == '}' && end + 1 < input.size() && input[end + 1] == '}') {
            depth--;
            if (depth == 0)
                break;
            end += 2;
        } else {
            ++end;
        }
    }

    std::string_view content = input.substr(0, end);

    // Parse template name (until first |)
    size_t pipe_pos = std::string_view::npos;
    depth = 0;
    for (size_t i = 0; i < content.size(); ++i) {
        char c = content[i];
        if (c == '{' && i + 1 < content.size() && content[i + 1] == '{') {
            depth++;
            ++i;
        } else if (c == '}' && i + 1 < content.size() && content[i + 1] == '}') {
            depth--;
            ++i;
        } else if (c == '|' && depth == 0) {
            pipe_pos = i;
            break;
        }
    }

    std::string_view name_part;
    std::string_view params_part;

    if (pipe_pos != std::string_view::npos) {
        name_part = content.substr(0, pipe_pos);
        params_part = content.substr(pipe_pos + 1);
    } else {
        name_part = content;
    }

    // Trim name
    size_t name_start = name_part.find_first_not_of(" \t\n");
    size_t name_end = name_part.find_last_not_of(" \t\n");
    if (name_start != std::string_view::npos && name_end != std::string_view::npos) {
        inv.name = std::string(name_part.substr(name_start, name_end - name_start + 1));
    }

    // Parse parameters
    if (!params_part.empty()) {
        size_t param_start = 0;
        depth = 0;

        for (size_t i = 0; i <= params_part.size(); ++i) {
            bool at_end = (i == params_part.size());
            char c = at_end ? '\0' : params_part[i];

            if (!at_end && c == '{' && i + 1 < params_part.size() && params_part[i + 1] == '{') {
                depth++;
                ++i;
            } else if (!at_end && c == '}' && i + 1 < params_part.size() && params_part[i + 1] == '}') {
                depth--;
                ++i;
            } else if ((c == '|' && depth == 0) || at_end) {
                std::string_view param = params_part.substr(param_start, i - param_start);

                // Find = for named parameter
                size_t eq_pos = std::string_view::npos;
                int eq_depth = 0;
                for (size_t j = 0; j < param.size(); ++j) {
                    if (param[j] == '{' && j + 1 < param.size() && param[j + 1] == '{') {
                        eq_depth++;
                        ++j;
                    } else if (param[j] == '}' && j + 1 < param.size() && param[j + 1] == '}') {
                        eq_depth--;
                        ++j;
                    } else if (param[j] == '=' && eq_depth == 0) {
                        eq_pos = j;
                        break;
                    }
                }

                std::string param_name;
                std::string param_value;

                if (eq_pos != std::string_view::npos) {
                    std::string_view name_sv = param.substr(0, eq_pos);
                    std::string_view value_sv = param.substr(eq_pos + 1);

                    // Trim
                    size_t ns = name_sv.find_first_not_of(" \t\n");
                    size_t ne = name_sv.find_last_not_of(" \t\n");
                    if (ns != std::string_view::npos && ne != std::string_view::npos) {
                        param_name = std::string(name_sv.substr(ns, ne - ns + 1));
                    }

                    size_t vs = value_sv.find_first_not_of(" \t\n");
                    size_t ve = value_sv.find_last_not_of(" \t\n");
                    if (vs != std::string_view::npos && ve != std::string_view::npos) {
                        param_value = std::string(value_sv.substr(vs, ve - vs + 1));
                    } else {
                        param_value = "";
                    }
                } else {
                    // Positional parameter - trim value only
                    size_t vs = param.find_first_not_of(" \t\n");
                    size_t ve = param.find_last_not_of(" \t\n");
                    if (vs != std::string_view::npos && ve != std::string_view::npos) {
                        param_value = std::string(param.substr(vs, ve - vs + 1));
                    }
                }

                inv.parameters.emplace_back(std::move(param_name), std::move(param_value));
                param_start = i + 1;
            }
        }
    }

    return inv;
}

std::vector<TemplateInvocation> find_invocations(std::string_view input) {
    std::vector<TemplateInvocation> invocations;

    size_t pos = 0;
    while (pos < input.size()) {
        size_t start = input.find("{{", pos);
        if (start == std::string_view::npos) {
            break;
        }

        // Skip {{{ (parameter)
        if (start + 2 < input.size() && input[start + 2] == '{') {
            pos = start + 3;
            continue;
        }

        // Find matching }}
        int depth = 1;
        size_t end = start + 2;
        while (end < input.size() && depth > 0) {
            if (input[end] == '{' && end + 1 < input.size() && input[end + 1] == '{') {
                if (end + 2 < input.size() && input[end + 2] == '{') {
                    // Skip {{{
                    end += 3;
                    continue;
                }
                depth++;
                end += 2;
            } else if (input[end] == '}' && end + 1 < input.size() && input[end + 1] == '}') {
                if (end + 2 < input.size() && input[end + 2] == '}' && depth == 1) {
                    // This might be }}} - check context
                }
                depth--;
                end += 2;
            } else {
                ++end;
            }
        }

        if (depth == 0) {
            std::string_view invocation_text = input.substr(start, end - start);
            auto result = parse_invocation(invocation_text);
            if (result) {
                result->location.begin.offset = start;
                result->location.end.offset = end;
                invocations.push_back(std::move(*result));
            }
        }

        pos = (depth == 0) ? end : start + 2;
    }

    return invocations;
}

// ============================================================================
// Parser function handling
// ============================================================================

bool is_parser_function(std::string_view name) noexcept {
    if (name.empty())
        return false;
    if (name[0] == '#') {
        name = name.substr(1);
    }

    // Convert to lowercase for comparison
    std::string lower;
    lower.reserve(name.size());
    for (char c: name) {
        lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    // Remove trailing colon if present
    if (!lower.empty() && lower.back() == ':') {
        lower.pop_back();
    }

    static const std::unordered_map<std::string, bool> functions = {
            {"if", true},        {"ifeq", true},    {"ifexist", true},    {"ifexpr", true},    {"switch", true},
            {"expr", true},      {"time", true},    {"titleparts", true}, {"invoke", true},    {"tag", true},
            {"language", true},  {"rel2abs", true}, {"filepath", true},   {"urlencode", true}, {"anchorencode", true},
            {"ns", true},        {"nse", true},     {"localurl", true},   {"fullurl", true},   {"canonicalurl", true},
            {"formatnum", true}, {"grammar", true}, {"gender", true},     {"plural", true},    {"bidi", true},
            {"padleft", true},   {"padright", true}};

    return functions.find(lower) != functions.end();
}

ParserFunction get_parser_function(std::string_view name) noexcept {
    if (name.empty())
        return ParserFunction::Unknown;
    if (name[0] == '#') {
        name = name.substr(1);
    }

    // Convert to lowercase
    std::string lower;
    lower.reserve(name.size());
    for (char c: name) {
        lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    // Remove trailing colon
    if (!lower.empty() && lower.back() == ':') {
        lower.pop_back();
    }

    if (lower == "if")
        return ParserFunction::If;
    if (lower == "ifeq")
        return ParserFunction::Ifeq;
    if (lower == "ifexist")
        return ParserFunction::Ifexist;
    if (lower == "ifexpr")
        return ParserFunction::Ifexpr;
    if (lower == "switch")
        return ParserFunction::Switch;
    if (lower == "expr")
        return ParserFunction::Expr;
    if (lower == "time")
        return ParserFunction::Time;
    if (lower == "titleparts")
        return ParserFunction::Titleparts;
    if (lower == "invoke")
        return ParserFunction::Invoke;
    if (lower == "tag")
        return ParserFunction::Tag;
    if (lower == "language")
        return ParserFunction::Language;

    return ParserFunction::Unknown;
}

std::string_view parser_function_name(ParserFunction func) noexcept {
    switch (func) {
        case ParserFunction::If:
            return "#if";
        case ParserFunction::Ifeq:
            return "#ifeq";
        case ParserFunction::Ifexist:
            return "#ifexist";
        case ParserFunction::Ifexpr:
            return "#ifexpr";
        case ParserFunction::Switch:
            return "#switch";
        case ParserFunction::Expr:
            return "#expr";
        case ParserFunction::Time:
            return "#time";
        case ParserFunction::Titleparts:
            return "#titleparts";
        case ParserFunction::Invoke:
            return "#invoke";
        case ParserFunction::Tag:
            return "#tag";
        case ParserFunction::Language:
            return "#language";
        case ParserFunction::Unknown:
            return "";
    }
    return "";
}

} // namespace wikilib::templates
