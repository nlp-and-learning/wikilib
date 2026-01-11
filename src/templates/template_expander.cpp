/**
 * @file template_expander.cpp
 * @brief Implementation of template expansion and transclusion
 */

#include "wikilib/templates/template_expander.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stack>
#include "wikilib/templates/template_parser.h"

namespace wikilib::templates {

// ============================================================================
// MemoryTemplateProvider implementation
// ============================================================================

void MemoryTemplateProvider::add_template(std::string name, std::string content) {
    templates_[std::move(name)] = std::move(content);
}

void MemoryTemplateProvider::remove_template(const std::string &name) {
    templates_.erase(name);
}

void MemoryTemplateProvider::clear() {
    templates_.clear();
}

std::optional<std::string> MemoryTemplateProvider::get_template(std::string_view name) {
    auto it = templates_.find(std::string(name));
    if (it != templates_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool MemoryTemplateProvider::template_exists(std::string_view name) {
    return templates_.find(std::string(name)) != templates_.end();
}

// ============================================================================
// ExpansionContext implementation
// ============================================================================

std::optional<std::string_view> ExpansionContext::get_param(std::string_view name) const {
    auto it = parameters.find(std::string(name));
    if (it != parameters.end()) {
        return it->second;
    }
    return std::nullopt;
}

// ============================================================================
// TemplateExpander implementation
// ============================================================================

TemplateExpander::TemplateExpander(std::shared_ptr<TemplateProvider> provider, ExpanderConfig config) :
    provider_(std::move(provider)), config_(std::move(config)) {
}

Result<std::string> TemplateExpander::expand(std::string_view input, const PageInfo &page) {
    ExpansionContext context;
    context.page = page;
    context.depth = 0;
    context.max_depth = config_.max_depth;

    try {
        std::string result = expand_recursive(input, context);
        return result;
    } catch (const std::exception &e) {
        return std::unexpected(ParseError{std::string("Expansion error: ") + e.what(), {}, ErrorSeverity::Error, ""});
    }
}

Result<void> TemplateExpander::expand_ast(markup::DocumentNode &doc, const PageInfo &page) {
    // Walk the AST and expand template nodes
    ExpansionContext context;
    context.page = page;
    context.depth = 0;
    context.max_depth = config_.max_depth;

    // This is a simplified implementation
    // Full implementation would recursively process template nodes

    return {};
}

Result<std::string> TemplateExpander::expand_template(const TemplateInvocation &invocation,
                                                      const ExpansionContext &context) {
    if (context.depth >= context.max_depth) {
        stats_.errors++;
        if (config_.fail_on_missing) {
            return std::unexpected(ParseError{"Maximum template recursion depth exceeded", invocation.location,
                                              ErrorSeverity::Error, ""});
        }
        return "{{" + invocation.name + "}}";
    }

    // Check for parser function
    if (is_parser_function(invocation.name)) {
        ParserFunction func = get_parser_function(invocation.name);
        std::vector<std::string> args;
        for (const auto &[name, value]: invocation.parameters) {
            args.push_back(value);
        }
        return evaluate_parser_function(func, args, context);
    }

    // Check cache
    std::string cache_key = invocation.name;
    for (const auto &[name, value]: invocation.parameters) {
        cache_key += "|" + name + "=" + value;
    }

    auto cache_it = cache_.find(cache_key);
    if (cache_it != cache_.end()) {
        stats_.cache_hits++;
        return cache_it->second;
    }

    // Get template content
    auto template_content = provider_->get_template(invocation.name);
    if (!template_content) {
        stats_.errors++;
        if (config_.fail_on_missing) {
            return std::unexpected(ParseError{"Template not found: " + invocation.name, invocation.location,
                                              ErrorSeverity::Error, ""});
        }
        if (config_.preserve_unknown) {
            return "{{" + invocation.name + "}}";
        }
        return "";
    }

    // Create new context with parameters
    ExpansionContext new_context = context;
    new_context.depth = context.depth + 1;
    new_context.parameters.clear();

    size_t positional_index = 1;
    for (const auto &[name, value]: invocation.parameters) {
        if (name.empty()) {
            new_context.parameters[std::to_string(positional_index)] = value;
            ++positional_index;
        } else {
            new_context.parameters[name] = value;
        }
    }

    // Expand the template content
    std::string result = expand_recursive(*template_content, new_context);

    stats_.templates_expanded++;
    if (new_context.depth > stats_.max_depth_reached) {
        stats_.max_depth_reached = new_context.depth;
    }

    // Cache result
    cache_[cache_key] = result;

    return result;
}

Result<std::string> TemplateExpander::evaluate_parser_function(ParserFunction func,
                                                               const std::vector<std::string> &args,
                                                               const ExpansionContext &context) {
    if (!config_.expand_parser_functions) {
        // Reconstruct the function call
        std::string result = "{{" + std::string(parser_function_name(func));
        for (const auto &arg: args) {
            result += "|" + arg;
        }
        result += "}}";
        return result;
    }

    stats_.parser_functions_evaluated++;

    switch (func) {
        case ParserFunction::If:
            return evaluate_if(args);
        case ParserFunction::Ifeq:
            return evaluate_ifeq(args);
        case ParserFunction::Switch:
            return evaluate_switch(args);
        case ParserFunction::Expr:
            return evaluate_expr(args);
        case ParserFunction::Time:
            return evaluate_time(args, context);
        case ParserFunction::Ifexist:
            if (args.empty())
                return "";
            if (provider_->template_exists(args[0])) {
                return args.size() > 1 ? args[1] : "";
            }
            return args.size() > 2 ? args[2] : "";
        case ParserFunction::Ifexpr:
            if (args.empty())
                return "";
            // Simplified: treat as #if for non-zero result
            {
                std::string expr_result = evaluate_expr({args[0]});
                bool is_true = !expr_result.empty() && expr_result != "0";
                if (is_true) {
                    return args.size() > 1 ? args[1] : "";
                }
                return args.size() > 2 ? args[2] : "";
            }
        case ParserFunction::Invoke:
            // Lua modules not supported without Lua runtime
            return args.empty() ? "" : "{{#invoke:" + args[0] + "}}";
        case ParserFunction::Tag:
            if (args.empty())
                return "";
            {
                std::string tag = args[0];
                std::string content = args.size() > 1 ? args[1] : "";
                return "<" + tag + ">" + content + "</" + tag + ">";
            }
        case ParserFunction::Language:
            // Return language code as-is
            return args.empty() ? "" : args[0];
        case ParserFunction::Titleparts:
            if (args.empty())
                return "";
            {
                std::string title = args[0];
                // Simplified: just return the title
                return title;
            }
        default:
            return "";
    }
}

std::string TemplateExpander::expand_recursive(std::string_view input, ExpansionContext &context) {
    if (stats_.templates_expanded + stats_.parser_functions_evaluated > config_.max_expansions) {
        return std::string(input);
    }

    std::string result;
    result.reserve(input.size());

    size_t pos = 0;
    while (pos < input.size()) {
        // Look for {{{ (parameter) first
        size_t param_start = input.find("{{{", pos);
        size_t template_start = input.find("{{", pos);

        // Skip {{{{ which is escaped
        while (template_start != std::string_view::npos && template_start + 2 < input.size() &&
               input[template_start + 2] == '{' &&
               (template_start + 3 >= input.size() || input[template_start + 3] != '{')) {
            // This is {{{ (parameter), not {{
            if (param_start == template_start) {
                break;
            }
            template_start = input.find("{{", template_start + 2);
        }

        // Handle parameter references {{{name}}}
        if (param_start != std::string_view::npos &&
            (template_start == std::string_view::npos || param_start < template_start)) {
            result += input.substr(pos, param_start - pos);

            // Find closing }}}
            int depth = 1;
            size_t end = param_start + 3;
            while (end < input.size() && depth > 0) {
                if (end + 2 < input.size() && input[end] == '{' && input[end + 1] == '{' && input[end + 2] == '{') {
                    depth++;
                    end += 3;
                } else if (end + 2 < input.size() && input[end] == '}' && input[end + 1] == '}' &&
                           input[end + 2] == '}') {
                    depth--;
                    if (depth == 0)
                        break;
                    end += 3;
                } else {
                    ++end;
                }
            }

            if (depth == 0) {
                std::string_view param_content = input.substr(param_start + 3, end - param_start - 3);

                // Split by | for default value
                size_t pipe_pos = param_content.find('|');
                std::string_view param_name;
                std::string_view default_value;

                if (pipe_pos != std::string_view::npos) {
                    param_name = param_content.substr(0, pipe_pos);
                    default_value = param_content.substr(pipe_pos + 1);
                } else {
                    param_name = param_content;
                }

                // Trim whitespace
                while (!param_name.empty() && std::isspace(static_cast<unsigned char>(param_name.front()))) {
                    param_name.remove_prefix(1);
                }
                while (!param_name.empty() && std::isspace(static_cast<unsigned char>(param_name.back()))) {
                    param_name.remove_suffix(1);
                }

                auto value = context.get_param(param_name);
                if (value) {
                    result += *value;
                } else if (!default_value.empty()) {
                    result += expand_recursive(default_value, context);
                } else {
                    // Keep unexpanded
                    result += input.substr(param_start, end + 3 - param_start);
                }

                pos = end + 3;
            } else {
                result += input.substr(pos, 3);
                pos = param_start + 3;
            }
            continue;
        }

        // Handle templates {{name}}
        if (template_start != std::string_view::npos) {
            result += input.substr(pos, template_start - pos);

            // Skip {{{ which is parameter
            if (template_start + 2 < input.size() && input[template_start + 2] == '{') {
                result += "{{";
                pos = template_start + 2;
                continue;
            }

            // Find matching }}
            int depth = 1;
            size_t end = template_start + 2;
            while (end < input.size() && depth > 0) {
                if (end + 1 < input.size() && input[end] == '{' && input[end + 1] == '{') {
                    if (end + 2 < input.size() && input[end + 2] == '{') {
                        // Skip {{{
                        end += 3;
                        continue;
                    }
                    depth++;
                    end += 2;
                } else if (end + 1 < input.size() && input[end] == '}' && input[end + 1] == '}') {
                    depth--;
                    if (depth == 0)
                        break;
                    end += 2;
                } else {
                    ++end;
                }
            }

            if (depth == 0) {
                std::string_view template_text = input.substr(template_start, end + 2 - template_start);
                auto invocation_result = parse_invocation(template_text);

                if (invocation_result) {
                    auto expanded = expand_template(*invocation_result, context);
                    if (expanded) {
                        // Recursively expand the result
                        result += expand_recursive(*expanded, context);
                    } else {
                        result += template_text;
                    }
                } else {
                    result += template_text;
                }

                pos = end + 2;
            } else {
                result += "{{";
                pos = template_start + 2;
            }
        } else {
            // No more templates
            result += input.substr(pos);
            break;
        }
    }

    return result;
}

std::string TemplateExpander::evaluate_if(const std::vector<std::string> &args) {
    if (args.empty())
        return "";

    // Trim and check condition
    std::string condition = args[0];
    size_t start = condition.find_first_not_of(" \t\n\r");
    size_t end = condition.find_last_not_of(" \t\n\r");

    bool is_true = (start != std::string::npos && end != std::string::npos && start <= end);

    if (is_true) {
        return args.size() > 1 ? args[1] : "";
    }
    return args.size() > 2 ? args[2] : "";
}

std::string TemplateExpander::evaluate_ifeq(const std::vector<std::string> &args) {
    if (args.size() < 2)
        return "";

    // Trim both values
    auto trim = [](const std::string &s) -> std::string {
        size_t start = s.find_first_not_of(" \t\n\r");
        size_t end = s.find_last_not_of(" \t\n\r");
        if (start == std::string::npos)
            return "";
        return s.substr(start, end - start + 1);
    };

    std::string val1 = trim(args[0]);
    std::string val2 = trim(args[1]);

    if (val1 == val2) {
        return args.size() > 2 ? args[2] : "";
    }
    return args.size() > 3 ? args[3] : "";
}

std::string TemplateExpander::evaluate_switch(const std::vector<std::string> &args) {
    if (args.empty())
        return "";

    auto trim = [](const std::string &s) -> std::string {
        size_t start = s.find_first_not_of(" \t\n\r");
        size_t end = s.find_last_not_of(" \t\n\r");
        if (start == std::string::npos)
            return "";
        return s.substr(start, end - start + 1);
    };

    std::string comparison = trim(args[0]);
    std::string default_value;
    std::string fallthrough_value;
    bool found_match = false;

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string &arg = args[i];
        size_t eq_pos = arg.find('=');

        if (eq_pos != std::string::npos) {
            std::string case_val = trim(arg.substr(0, eq_pos));
            std::string result_val = arg.substr(eq_pos + 1);

            if (case_val == "#default") {
                default_value = result_val;
            } else if (case_val == comparison || found_match) {
                return result_val;
            }
        } else {
            // Fallthrough case
            std::string case_val = trim(arg);
            if (case_val == comparison) {
                found_match = true;
            }
        }
    }

    return default_value;
}

std::string TemplateExpander::evaluate_expr(const std::vector<std::string> &args) {
    if (args.empty())
        return "";

    std::string expr = args[0];

    // Remove whitespace
    expr.erase(std::remove_if(expr.begin(), expr.end(), [](unsigned char c) { return std::isspace(c); }), expr.end());

    if (expr.empty())
        return "";

    // Simple expression evaluator for basic arithmetic
    // This is a simplified implementation
    try {
        // Handle simple numbers
        bool all_digits = true;
        bool has_dot = false;
        bool has_sign = false;
        size_t start = 0;

        if (!expr.empty() && (expr[0] == '+' || expr[0] == '-')) {
            has_sign = true;
            start = 1;
        }

        for (size_t i = start; i < expr.size(); ++i) {
            if (expr[i] == '.') {
                if (has_dot) {
                    all_digits = false;
                    break;
                }
                has_dot = true;
            } else if (!std::isdigit(static_cast<unsigned char>(expr[i]))) {
                all_digits = false;
                break;
            }
        }

        if (all_digits && !expr.empty() && (start < expr.size())) {
            double val = std::stod(expr);
            if (val == std::floor(val) && std::abs(val) < 1e15) {
                return std::to_string(static_cast<long long>(val));
            }
            std::ostringstream oss;
            oss << std::setprecision(10) << val;
            return oss.str();
        }

        // For complex expressions, return as-is or 0
        // Full implementation would need expression parser
        return "0";
    } catch (...) {
        return "0";
    }
}

std::string TemplateExpander::evaluate_time(const std::vector<std::string> &args, const ExpansionContext &ctx) {
    (void) ctx;

    if (args.empty())
        return "";

    std::string format = args[0];
    std::time_t now = std::time(nullptr);
    std::tm *tm = std::gmtime(&now);

    if (!tm)
        return "";

    // Convert MediaWiki format to strftime
    // This is simplified - full implementation would handle all MW format codes
    std::string result;
    for (size_t i = 0; i < format.size(); ++i) {
        char c = format[i];
        switch (c) {
            case 'Y': { // 4-digit year
                char buf[5];
                std::snprintf(buf, sizeof(buf), "%04d", 1900 + tm->tm_year);
                result += buf;
                break;
            }
            case 'y': { // 2-digit year
                char buf[3];
                std::snprintf(buf, sizeof(buf), "%02d", tm->tm_year % 100);
                result += buf;
                break;
            }
            case 'm': { // Month (01-12)
                char buf[3];
                std::snprintf(buf, sizeof(buf), "%02d", tm->tm_mon + 1);
                result += buf;
                break;
            }
            case 'n': { // Month (1-12)
                result += std::to_string(tm->tm_mon + 1);
                break;
            }
            case 'd': { // Day (01-31)
                char buf[3];
                std::snprintf(buf, sizeof(buf), "%02d", tm->tm_mday);
                result += buf;
                break;
            }
            case 'j': { // Day (1-31)
                result += std::to_string(tm->tm_mday);
                break;
            }
            case 'H': { // Hour (00-23)
                char buf[3];
                std::snprintf(buf, sizeof(buf), "%02d", tm->tm_hour);
                result += buf;
                break;
            }
            case 'i': { // Minute (00-59)
                char buf[3];
                std::snprintf(buf, sizeof(buf), "%02d", tm->tm_min);
                result += buf;
                break;
            }
            case 's': { // Second (00-59)
                char buf[3];
                std::snprintf(buf, sizeof(buf), "%02d", tm->tm_sec);
                result += buf;
                break;
            }
            default:
                result += c;
                break;
        }
    }

    return result;
}

// ============================================================================
// Magic words implementation
// ============================================================================

MagicWord get_magic_word(std::string_view name) noexcept {
    // Convert to uppercase for comparison
    std::string upper;
    upper.reserve(name.size());
    for (char c: name) {
        upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    // Page names
    if (upper == "PAGENAME")
        return MagicWord::PageName;
    if (upper == "PAGENAMEE")
        return MagicWord::PageNameE;
    if (upper == "FULLPAGENAME")
        return MagicWord::FullPageName;
    if (upper == "FULLPAGENAMEE")
        return MagicWord::FullPageNameE;
    if (upper == "BASEPAGENAME")
        return MagicWord::BasePageName;
    if (upper == "SUBPAGENAME")
        return MagicWord::SubPageName;
    if (upper == "ROOTPAGENAME")
        return MagicWord::RootPageName;
    if (upper == "TALKPAGENAME")
        return MagicWord::TalkPageName;
    if (upper == "SUBJECTPAGENAME" || upper == "ARTICLEPAGENAME")
        return MagicWord::SubjectPageName;

    // Namespaces
    if (upper == "NAMESPACE")
        return MagicWord::NameSpace;
    if (upper == "NAMESPACEE")
        return MagicWord::NameSpaceE;
    if (upper == "TALKSPACE")
        return MagicWord::TalkSpace;
    if (upper == "SUBJECTSPACE" || upper == "ARTICLESPACE")
        return MagicWord::SubjectSpace;

    // Dates
    if (upper == "CURRENTYEAR")
        return MagicWord::CurrentYear;
    if (upper == "CURRENTMONTH" || upper == "CURRENTMONTH2")
        return MagicWord::CurrentMonth;
    if (upper == "CURRENTDAY" || upper == "CURRENTDAY2")
        return MagicWord::CurrentDay;
    if (upper == "CURRENTTIME")
        return MagicWord::CurrentTime;
    if (upper == "CURRENTTIMESTAMP")
        return MagicWord::CurrentTimestamp;

    // Statistics
    if (upper == "NUMBEROFPAGES")
        return MagicWord::NumberOfPages;
    if (upper == "NUMBEROFARTICLES")
        return MagicWord::NumberOfArticles;
    if (upper == "NUMBEROFFILES")
        return MagicWord::NumberOfFiles;

    // Site
    if (upper == "SITENAME")
        return MagicWord::SiteName;
    if (upper == "SERVER")
        return MagicWord::Server;
    if (upper == "SERVERNAME")
        return MagicWord::ServerName;

    // Misc
    if (upper == "CONTENTLANGUAGE" || upper == "CONTENTLANG")
        return MagicWord::ContentLanguage;

    return MagicWord::Unknown;
}

std::string evaluate_magic_word(MagicWord word, const ExpansionContext &context) {
    std::time_t now = std::time(nullptr);
    std::tm *tm = std::gmtime(&now);

    auto url_encode = [](const std::string &s) -> std::string {
        std::string result;
        for (char c: s) {
            if (c == ' ') {
                result += '_';
            } else {
                result += c;
            }
        }
        return result;
    };

    switch (word) {
        case MagicWord::PageName:
            return context.page.title;

        case MagicWord::PageNameE:
            return url_encode(context.page.title);

        case MagicWord::FullPageName:
            return context.page.full_title();

        case MagicWord::FullPageNameE:
            return url_encode(context.page.full_title());

        case MagicWord::BasePageName: {
            const std::string &title = context.page.title;
            size_t slash = title.rfind('/');
            if (slash != std::string::npos) {
                return title.substr(0, slash);
            }
            return title;
        }

        case MagicWord::SubPageName: {
            const std::string &title = context.page.title;
            size_t slash = title.rfind('/');
            if (slash != std::string::npos) {
                return title.substr(slash + 1);
            }
            return title;
        }

        case MagicWord::RootPageName: {
            const std::string &title = context.page.title;
            size_t slash = title.find('/');
            if (slash != std::string::npos) {
                return title.substr(0, slash);
            }
            return title;
        }

        case MagicWord::TalkPageName:
            return "Talk:" + context.page.title;

        case MagicWord::SubjectPageName:
            return context.page.title;

        case MagicWord::NameSpace:
            // Simplified - return empty for main namespace
            return "";

        case MagicWord::NameSpaceE:
            return "";

        case MagicWord::TalkSpace:
            return "Talk";

        case MagicWord::SubjectSpace:
            return "";

        case MagicWord::CurrentYear:
            if (tm) {
                return std::to_string(1900 + tm->tm_year);
            }
            return "";

        case MagicWord::CurrentMonth:
            if (tm) {
                char buf[3];
                std::snprintf(buf, sizeof(buf), "%02d", tm->tm_mon + 1);
                return buf;
            }
            return "";

        case MagicWord::CurrentDay:
            if (tm) {
                char buf[3];
                std::snprintf(buf, sizeof(buf), "%02d", tm->tm_mday);
                return buf;
            }
            return "";

        case MagicWord::CurrentTime:
            if (tm) {
                char buf[6];
                std::snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
                return buf;
            }
            return "";

        case MagicWord::CurrentTimestamp:
            if (tm) {
                char buf[15];
                std::snprintf(buf, sizeof(buf), "%04d%02d%02d%02d%02d%02d", 1900 + tm->tm_year, tm->tm_mon + 1,
                              tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
                return buf;
            }
            return "";

        case MagicWord::NumberOfPages:
            return "0";

        case MagicWord::NumberOfArticles:
            return "0";

        case MagicWord::NumberOfFiles:
            return "0";

        case MagicWord::SiteName:
            return "Wikipedia";

        case MagicWord::Server:
            return "//en.wikipedia.org";

        case MagicWord::ServerName:
            return "en.wikipedia.org";

        case MagicWord::ContentLanguage:
            return "en";

        case MagicWord::Unknown:
        default:
            return "";
    }
}

} // namespace wikilib::templates
