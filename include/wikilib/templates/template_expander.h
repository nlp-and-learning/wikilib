#pragma once

/**
 * @file template_expander.hpp
 * @brief Template expansion and transclusion
 */

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include "wikilib/core/types.h"
#include "wikilib/markup/ast.h"
#include "wikilib/templates/template_parser.h"

namespace wikilib::templates {

// ============================================================================
// Template provider interface
// ============================================================================

/**
 * @brief Interface for retrieving template content
 *
 * Implement this to provide template content from your data source
 * (dump files, database, API, etc.)
 */
class TemplateProvider {
public:
    virtual ~TemplateProvider() = default;

    /**
     * @brief Get template content by name
     * @param name Template name (without "Template:" prefix)
     * @return Template content or nullopt if not found
     */
    [[nodiscard]] virtual std::optional<std::string> get_template(std::string_view name) = 0;

    /**
     * @brief Check if template exists
     */
    [[nodiscard]] virtual bool template_exists(std::string_view name) = 0;

    /**
     * @brief Get module content (for #invoke)
     */
    [[nodiscard]] virtual std::optional<std::string> get_module(std::string_view name) {
        (void) name;
        return std::nullopt;
    }
};

/**
 * @brief Simple in-memory template provider
 */
class MemoryTemplateProvider : public TemplateProvider {
public:
    void add_template(std::string name, std::string content);
    void remove_template(const std::string &name);
    void clear();

    [[nodiscard]] std::optional<std::string> get_template(std::string_view name) override;
    [[nodiscard]] bool template_exists(std::string_view name) override;
private:
    std::unordered_map<std::string, std::string> templates_;
};

// ============================================================================
// Expansion context
// ============================================================================

/**
 * @brief Context for template expansion
 */
struct ExpansionContext {
    PageInfo page; // Current page being expanded
    std::unordered_map<std::string, std::string> parameters; // Current template params
    int depth = 0; // Current recursion depth
    int max_depth = 40; // Maximum recursion depth

    [[nodiscard]] std::optional<std::string_view> get_param(std::string_view name) const;
};

// ============================================================================
// Expander configuration
// ============================================================================

/**
 * @brief Configuration for template expansion
 */
struct ExpanderConfig {
    int max_depth = 40; // Maximum recursive body/argument/default expansion levels
    int max_expansions = 10000; // Per operation: templates/functions/parameters/magic words
    bool expand_parser_functions = true; // Evaluate #if, #switch, etc.
    bool evaluate_lua = false; // Reserved; Lua is not implemented
    bool fail_on_missing = false; // Error on missing templates
    bool preserve_unknown = true; // Keep unexpanded if can't expand
    size_t max_output_bytes = 16 * 1024 * 1024; // Limit each expanded string
};

// ============================================================================
// Template expander
// ============================================================================

/**
 * @brief Expands templates in wikitext
 */
class TemplateExpander {
public:
    explicit TemplateExpander(std::shared_ptr<TemplateProvider> provider, ExpanderConfig config = {});

    /**
     * @brief Expand the documented subset of templates in wikitext
     * Limits and errors return an unexpected ParseError. Stats are cumulative;
     * budgets and raw-definition cache reset for each public operation.
     * See docs/TEMPLATE_EXPANSION.md for supported behavior and limitations.
     */
    [[nodiscard]] Result<std::string> expand(std::string_view input, const PageInfo &page = {});

    /**
     * @brief Expand serialized AST wikitext and reparse it transactionally
     * Generated markup becomes AST nodes. Source locations refer to regenerated
     * wikitext; existing child pointers are invalidated on success. Parent,
     * category, and redirect pointers are rebuilt. Errors leave doc unchanged.
     */
    [[nodiscard]] Result<void> expand_ast(markup::DocumentNode &doc, const PageInfo &page = {});

    /**
     * @brief Expand single template
     */
    [[nodiscard]] Result<std::string> expand_template(const TemplateInvocation &invocation,
                                                      const ExpansionContext &context);

    /**
     * @brief Evaluate parser function
     */
    [[nodiscard]] Result<std::string> evaluate_parser_function(ParserFunction func,
                                                               const std::vector<std::string> &args,
                                                               const ExpansionContext &context);

    /**
     * @brief Get cumulative expansion statistics (cache hits mean definition reuse)
     */
    struct Stats {
        int templates_expanded = 0;
        int parser_functions_evaluated = 0;
        int max_depth_reached = 0;
        int cache_hits = 0;
        int errors = 0;
    };

    [[nodiscard]] const Stats &stats() const noexcept {
        return stats_;
    }

    /**
     * @brief Reset statistics
     */
    void reset_stats() {
        stats_ = {};
    }
private:
    std::shared_ptr<TemplateProvider> provider_;
    ExpanderConfig config_;
    Stats stats_;

    bool operation_active_ = false;
    int operation_expansions_ = 0;

    // Raw definitions cached only within a public operation; results always
    // evaluate the current page/parameters and consume the expansion budget.
    std::unordered_map<std::string, std::string> cache_;

    Result<std::string> run_operation(const std::function<std::string()> &action);
    void consume_expansion(const ExpansionContext &context);
    void append(std::string &output, std::string_view text) const;
    std::string expand_one(const TemplateInvocation &invocation, const ExpansionContext &context);
    std::string evaluate_function(ParserFunction func, const std::vector<std::string> &args,
                                  const ExpansionContext &context);
    std::string expand_recursive(std::string_view input, ExpansionContext &context);

    std::string evaluate_expr(const std::vector<std::string> &args);
    std::string evaluate_time(const std::vector<std::string> &args, const ExpansionContext &ctx);
};

// ============================================================================
// Magic words
// ============================================================================

/**
 * @brief Known magic words
 */
enum class MagicWord {
    // Page info
    PageName,
    PageNameE,
    FullPageName,
    FullPageNameE,
    BasePageName,
    SubPageName,
    RootPageName,
    TalkPageName,
    SubjectPageName,

    // Namespaces
    NameSpace,
    NameSpaceE,
    TalkSpace,
    SubjectSpace,

    // Dates
    CurrentYear,
    CurrentMonth,
    CurrentDay,
    CurrentTime,
    CurrentTimestamp,

    // Statistics
    NumberOfPages,
    NumberOfArticles,
    NumberOfFiles,

    // Site
    SiteName,
    Server,
    ServerName,

    // Misc
    ContentLanguage,

    Unknown
};

/**
 * @brief Get magic word type from name
 */
[[nodiscard]] MagicWord get_magic_word(std::string_view name) noexcept;

/**
 * @brief Evaluate a supported page/UTC-date magic word
 * Unsupported enum values return an empty string; TemplateExpander preserves
 * their invocation according to preserve_unknown instead.
 */
[[nodiscard]] std::string evaluate_magic_word(MagicWord word, const ExpansionContext &context);

} // namespace wikilib::templates
