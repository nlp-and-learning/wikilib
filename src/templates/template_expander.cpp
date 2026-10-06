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
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include "expansion_syntax.h"
#include "expression.h"
#include "wikilib/markup/parser.h"
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

namespace {
bool supported_magic_word(MagicWord word) {
    switch (word) {
        case MagicWord::PageName:
        case MagicWord::FullPageName:
        case MagicWord::BasePageName:
        case MagicWord::SubPageName:
        case MagicWord::RootPageName:
        case MagicWord::NameSpace:
        case MagicWord::CurrentYear:
        case MagicWord::CurrentMonth:
        case MagicWord::CurrentDay:
        case MagicWord::CurrentTime:
        case MagicWord::CurrentTimestamp:
            return true;
        default:
            return false;
    }
}
} // namespace

// ============================================================================
// TemplateExpander implementation
// ============================================================================

TemplateExpander::TemplateExpander(std::shared_ptr<TemplateProvider> provider, ExpanderConfig config) :
    provider_(std::move(provider)), config_(std::move(config)) {
}

Result<std::string> TemplateExpander::run_operation(const std::function<std::string()> &action) {
    const bool root = !operation_active_;
    if (root) {
        operation_active_ = true;
        operation_expansions_ = 0;
        cache_.clear();
    }

    struct Guard {
        TemplateExpander &self;
        bool root;

        ~Guard() {
            if (root) {
                self.operation_active_ = false;
                self.cache_.clear();
            }
        }
    } guard{*this, root};

    try {
        if (config_.max_depth < 0 || config_.max_expansions < 0)
            throw std::invalid_argument("Expansion limits must be nonnegative");
        auto result = action();
        if (result.size() > config_.max_output_bytes)
            throw std::runtime_error("Maximum expansion output size exceeded");
        return result;
    } catch (const std::exception &e) {
        ++stats_.errors;
        return std::unexpected(ParseError{std::string("Expansion error: ") + e.what(), {}, ErrorSeverity::Error, ""});
    }
}

void TemplateExpander::append(std::string &output, std::string_view text) const {
    if (output.size() > config_.max_output_bytes || text.size() > config_.max_output_bytes - output.size())
        throw std::runtime_error("Maximum expansion output size exceeded");
    output.append(text);
}

void TemplateExpander::consume_expansion(const ExpansionContext &context) {
    if (context.depth < 0 || context.max_depth < 0)
        throw std::invalid_argument("Invalid expansion context depth");
    if (context.depth >= std::min(context.max_depth, config_.max_depth))
        throw std::runtime_error("Maximum template recursion depth exceeded");
    if (operation_expansions_ >= config_.max_expansions)
        throw std::runtime_error("Maximum expansion count exceeded");
    ++operation_expansions_;
    stats_.max_depth_reached = std::max(stats_.max_depth_reached, context.depth + 1);
}

Result<std::string> TemplateExpander::expand(std::string_view input, const PageInfo &page) {
    ExpansionContext context;
    context.page = page;
    context.max_depth = config_.max_depth;
    return run_operation([&] { return expand_recursive(input, context); });
}

Result<void> TemplateExpander::expand_ast(markup::DocumentNode &doc, const PageInfo &page) {
    try {
        // Reparse after expansion so generated block markup becomes AST nodes.
        auto expanded = expand(doc.to_wikitext(), page);
        if (!expanded)
            return std::unexpected(expanded.error());
        markup::ParserConfig config;
        config.preserve_whitespace = true;
        config.tokenizer.preserve_comments = true;
        markup::Parser parser(config);
        auto parsed = parser.parse(*expanded, page);
        if (!parsed.document || parsed.has_errors()) {
            ++stats_.errors;
            return std::unexpected(parsed.errors.empty()
                                           ? ParseError{"Expanded AST parse failed", {}, ErrorSeverity::Error, ""}
                                           : parsed.errors.front());
        }
        auto &replacement = *parsed.document;
        replacement.categories.clear();
        replacement.redirect = nullptr;
        std::function<void(markup::Node &, markup::Node *)> repair = [&](markup::Node &node, markup::Node *parent) {
            node.parent = parent;
            if (node.type == markup::NodeType::Category)
                replacement.categories.push_back(static_cast<markup::CategoryNode *>(&node));
            if (node.type == markup::NodeType::Redirect)
                replacement.redirect = static_cast<markup::RedirectNode *>(&node);
            for (auto &child: node.children())
                repair(*child, &node);
            if (node.type == markup::NodeType::Template)
                for (auto &param: static_cast<markup::TemplateNode &>(node).parameters)
                    for (auto &child: param.value)
                        repair(*child, &node);
        };
        for (auto &node: replacement.content)
            repair(*node, &doc);
        doc.content = std::move(replacement.content);
        doc.categories = std::move(replacement.categories);
        doc.redirect = replacement.redirect;
        doc.location = replacement.location;
        return {};
    } catch (const std::exception &error) {
        ++stats_.errors;
        return std::unexpected(
                ParseError{std::string("AST expansion error: ") + error.what(), {}, ErrorSeverity::Error, ""});
    }
}

Result<std::string> TemplateExpander::expand_template(const TemplateInvocation &invocation,
                                                      const ExpansionContext &context) {
    return run_operation([&] { return expand_one(invocation, context); });
}

std::string TemplateExpander::expand_one(const TemplateInvocation &invocation, const ExpansionContext &context) {
    consume_expansion(context);
    const auto nested = [&](std::string_view text) {
        auto child = context;
        ++child.depth;
        return expand_recursive(text, child);
    };
    const auto preserve = [&] {
        std::string text = "{{" + invocation.name;
        for (const auto &[key, value]: invocation.parameters) {
            append(text, "|");
            if (!key.empty()) {
                append(text, key);
                append(text, "=");
            }
            append(text, value);
        }
        append(text, "}}");
        return config_.preserve_unknown ? text : std::string{};
    };
    const auto raw_name = detail::trim(invocation.name);
    if (raw_name.starts_with('#')) {
        const auto colon = raw_name.find(':');
        const auto func = get_parser_function(raw_name.substr(0, colon));
        if (!config_.expand_parser_functions || func == ParserFunction::Unknown || func == ParserFunction::Invoke ||
            func == ParserFunction::Titleparts || func == ParserFunction::Language)
            return preserve();
        std::vector<std::string> args;
        if (colon != raw_name.npos)
            args.emplace_back(raw_name.substr(colon + 1));
        for (const auto &[key, value]: invocation.parameters)
            args.push_back(key.empty() ? value : key + "=" + value);
        // Evaluate only the arguments needed by the selected function/branch.
        return evaluate_function(func, args, context);
    }
    auto name = std::string(detail::trim(nested(invocation.name)));
    if (name.starts_with("Template:"))
        name.erase(0, 9);
    const auto word = get_magic_word(name);
    if (word != MagicWord::Unknown)
        return supported_magic_word(word) ? evaluate_magic_word(word, context) : preserve();
    if (!provider_)
        throw std::runtime_error("Template provider is required");
    auto found = cache_.find(name);
    if (found == cache_.end()) {
        auto content = provider_->get_template(name);
        if (!content) {
            if (config_.fail_on_missing)
                throw std::runtime_error("Template not found: " + name);
            return preserve();
        }
        found = cache_.emplace(name, std::move(*content)).first;
    } else
        ++stats_.cache_hits;
    // Copy before nested lookups, since unordered_map rehash invalidates iterators.
    const std::string definition = found->second;
    ExpansionContext child = context;
    ++child.depth;
    child.parameters.clear();
    size_t positional = 1;
    for (const auto &[key, value]: invocation.parameters) {
        const auto parameter_name = key.empty() ? std::to_string(positional++) : std::string(detail::trim(nested(key)));
        child.parameters[parameter_name] = nested(value);
    }
    ++stats_.templates_expanded;
    return expand_recursive(definition, child);
}

Result<std::string> TemplateExpander::evaluate_parser_function(ParserFunction func,
                                                               const std::vector<std::string> &args,
                                                               const ExpansionContext &context) {
    return run_operation([&] {
        consume_expansion(context);
        if (!config_.expand_parser_functions || func == ParserFunction::Invoke || func == ParserFunction::Unknown ||
            func == ParserFunction::Titleparts || func == ParserFunction::Language) {
            if (!config_.preserve_unknown)
                return std::string{};
            std::string text = "{{" + std::string(parser_function_name(func));
            if (!args.empty()) {
                append(text, ":");
                append(text, args.front());
            }
            for (size_t i = 1; i < args.size(); ++i) {
                append(text, "|");
                append(text, args[i]);
            }
            append(text, "}}");
            return text;
        }
        return evaluate_function(func, args, context);
    });
}

std::string TemplateExpander::evaluate_function(ParserFunction func, const std::vector<std::string> &args,
                                                const ExpansionContext &context) {
    ++stats_.parser_functions_evaluated;
    const auto arg = [&](size_t index) {
        if (index >= args.size())
            return std::string{};
        auto child = context;
        ++child.depth;
        return expand_recursive(args[index], child);
    };
    switch (func) {
        case ParserFunction::If:
            return arg(detail::trim(arg(0)).empty() ? 2 : 1);
        case ParserFunction::Ifeq:
            return arg(detail::trim(arg(0)) == detail::trim(arg(1)) ? 2 : 3);
        case ParserFunction::Ifexpr:
            return arg(detail::Expression(arg(0)).evaluate() == 0 ? 2 : 1);
        case ParserFunction::Expr:
            return evaluate_expr({arg(0)});
        case ParserFunction::Ifexist:
            if (!provider_)
                throw std::runtime_error("Template provider is required");
            return arg(provider_->template_exists(detail::trim(arg(0))) ? 1 : 2);
        case ParserFunction::Switch: {
            const auto comparison = std::string(detail::trim(arg(0)));
            std::string fallback;
            bool matched = false;
            for (size_t i = 1; i < args.size(); ++i) {
                const auto pieces = detail::split(args[i], '=');
                if (pieces.size() == 1) {
                    if (detail::trim(arg(i)) == comparison)
                        matched = true;
                    if (i + 1 == args.size())
                        fallback = args[i];
                    continue;
                }
                auto child = context;
                ++child.depth;
                const auto key = std::string(detail::trim(expand_recursive(pieces.front(), child)));
                const auto value = args[i].substr(pieces.front().size() + 1);
                if (key == "#default")
                    fallback = value;
                else if (matched || key == comparison)
                    return expand_recursive(value, child);
            }
            auto child = context;
            ++child.depth;
            return expand_recursive(fallback, child);
        }
        case ParserFunction::Time: {
            if (args.size() > 1)
                throw std::runtime_error("#time date/language arguments are unsupported");
            return evaluate_time({arg(0)}, context);
        }
        case ParserFunction::Tag: {
            if (args.size() > 2)
                throw std::runtime_error("#tag attributes are unsupported");
            const auto tag = arg(0);
            if (tag.empty())
                throw std::runtime_error("#tag requires a tag name");
            std::string text = "<" + tag + ">";
            append(text, arg(1));
            append(text, "</" + tag + ">");
            return text;
        }
        default:
            throw std::runtime_error("Unsupported parser function");
    }
}

std::string TemplateExpander::expand_recursive(std::string_view input, ExpansionContext &context) {
    if (context.depth > std::min(context.max_depth, config_.max_depth))
        throw std::runtime_error("Maximum template recursion depth exceeded");
    std::string result;
    size_t pos = 0;
    while (pos < input.size()) {
        if (const auto end = detail::opaque_end(input, pos); end != pos) {
            append(result, input.substr(pos, end - pos));
            pos = end;
            continue;
        }
        if (!input.substr(pos).starts_with("{{")) {
            append(result, input.substr(pos++, 1));
            continue;
        }
        const auto end = detail::brace_end(input, pos);
        if (end == input.npos) {
            append(result, input.substr(pos));
            break;
        }
        const auto original = input.substr(pos, end - pos);
        if (original.starts_with("{{{")) {
            consume_expansion(context);
            const auto pieces = detail::split(original.substr(3, original.size() - 6), '|');
            auto child = context;
            ++child.depth;
            const auto name = std::string(detail::trim(expand_recursive(pieces.front(), child)));
            if (auto value = context.get_param(name))
                append(result, *value);
            else if (pieces.size() > 1)
                append(result, expand_recursive(original.substr(3 + pieces.front().size() + 1,
                                                                original.size() - 7 - pieces.front().size()),
                                                child));
            else
                append(result, original);
        } else {
            auto invocation = parse_invocation(original);
            if (!invocation)
                throw std::runtime_error(invocation.error().message);
            append(result, expand_one(*invocation, context));
        }
        pos = end;
    }
    return result;
}

std::string TemplateExpander::evaluate_expr(const std::vector<std::string> &args) {
    if (args.empty())
        throw std::runtime_error("Expression error: expected expression");
    const double value = detail::Expression(args.front()).evaluate();
    if (value == 0)
        return "0";
    std::ostringstream result;
    result.imbue(std::locale::classic());
    result << std::setprecision(std::numeric_limits<double>::digits10) << value;
    return result.str();
}

std::string TemplateExpander::evaluate_time(const std::vector<std::string> &args, const ExpansionContext &ctx) {
    (void) ctx;

    if (args.empty())
        return "";

    std::string format = args[0];
    std::time_t now = std::time(nullptr);
    std::tm tm_storage{};
    std::tm *tm = ::gmtime_r(&now, &tm_storage);

    if (!tm)
        return "";

    // Convert MediaWiki format to strftime
    // This is simplified - full implementation would handle all MW format codes
    std::string result;
    for (size_t i = 0; i < format.size(); ++i) {
        char c = format[i];
        switch (c) {
            case 'Y': { // 4-digit year
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%04d", 1900 + tm->tm_year);
                result += buf;
                break;
            }
            case 'y': { // 2-digit year
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%02d", tm->tm_year % 100);
                result += buf;
                break;
            }
            case 'm': { // Month (01-12)
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%02d", tm->tm_mon + 1);
                result += buf;
                break;
            }
            case 'n': { // Month (1-12)
                result += std::to_string(tm->tm_mon + 1);
                break;
            }
            case 'd': { // Day (01-31)
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%02d", tm->tm_mday);
                result += buf;
                break;
            }
            case 'j': { // Day (1-31)
                result += std::to_string(tm->tm_mday);
                break;
            }
            case 'H': { // Hour (00-23)
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%02d", tm->tm_hour);
                result += buf;
                break;
            }
            case 'i': { // Minute (00-59)
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%02d", tm->tm_min);
                result += buf;
                break;
            }
            case 's': { // Second (00-59)
                char buf[16];
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
    std::string title = context.page.title;
    std::string ns;
    if (context.page.namespace_id != 0) {
        const auto colon = title.find(':');
        if (colon != title.npos) {
            ns = title.substr(0, colon);
            title.erase(0, colon + 1);
        }
    }
    switch (word) {
        case MagicWord::PageName:
            return title;
        case MagicWord::FullPageName:
            return context.page.full_title();
        case MagicWord::BasePageName: {
            const auto slash = title.rfind('/');
            return slash == title.npos ? title : title.substr(0, slash);
        }
        case MagicWord::SubPageName: {
            const auto slash = title.rfind('/');
            return slash == title.npos ? title : title.substr(slash + 1);
        }
        case MagicWord::RootPageName:
            return title.substr(0, title.find('/'));
        case MagicWord::NameSpace:
            return ns;
        case MagicWord::CurrentYear:
        case MagicWord::CurrentMonth:
        case MagicWord::CurrentDay:
        case MagicWord::CurrentTime:
        case MagicWord::CurrentTimestamp: {
            const auto now = std::time(nullptr);
            std::tm tm{};
            if (!::gmtime_r(&now, &tm))
                return "";
            const char *format = "%Y";
            if (word == MagicWord::CurrentMonth)
                format = "%m";
            if (word == MagicWord::CurrentDay)
                format = "%d";
            if (word == MagicWord::CurrentTime)
                format = "%H:%M";
            if (word == MagicWord::CurrentTimestamp)
                format = "%Y%m%d%H%M%S";
            char result[32];
            return std::strftime(result, sizeof(result), format, &tm) ? std::string(result) : std::string{};
        }
        default:
            return "";
    }
}

} // namespace wikilib::templates
